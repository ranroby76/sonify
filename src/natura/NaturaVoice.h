// C:\workspace\SONIFY PLUGIN natura\src\NaturaVoice.h
#pragma once
//==============================================================================
//  NaturaVoice.h  -  one voice: up to three unison copies of the archetype
//  model, then the synthesis department (body, character, filter, envelopes,
//  LFOs, attack glide), the articulation shaping and the dynamics.
//
//  Control runs every 16 samples (pitch, pressure, filter coefficients);
//  audio runs per sample.  Nothing allocates here after prepare().
//==============================================================================
#include "Archetypes.h"
#include "SynthCommon.h"
#include "AttackLibrary.h"
#include "ToneLayer.h"

namespace natura
{
    constexpr int kCtl = 16;           // control block
    constexpr int kMaxUnison = 3;

    /** Everything a block's settings say, read once per audio block. */
    struct Snapshot
    {
        float v[kNumParams] = {};
        Archetype arch = Archetype::StruckString;
        float spec[kMaxSpec] = {};
        oscengine::Params osc;
        Dynamics dynMode = Dynamics::Struck;
        bool  sustained = false;
        int   regOffset = 0;

        float operator[] (P p) const { return v[(int) p]; }
    };

    /** What the player is doing right now (updated between MIDI events). */
    struct Live
    {
        float bend   = 0.0f;   // semitones
        float mw     = 0.0f;   // 0..1
        float at     = 0.0f;   // 0..1
        float breath = 1.0f, expr = 1.0f;
        float pedal  = 0.0f;   // 0..1 sustain pedal (continuous: half pedal)
        float soft   = 0.0f;   // soft pedal
    };

    struct NoteStart
    {
        int   note = 60;           // sounding note (octave already applied)
        float velocity = 0.8f;     // 0..1 after the curve
        int   rawVelocity = 100;
        Art   art = Art::Normal;
        bool  attackGlide = false;
        float glideFromSemis = 0.0f;   // portamento start offset (0 = none)
        float glideMs = 0.0f;
        bool  percTrigger = true;
        std::uint32_t seed = 1;
    };

    class NaturaVoice
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            for (auto& u : units) u.prepare (sr);
            tone.prepare (sr);
            shCoefFast    = 1.0f - (float) std::exp (-1.0 / (0.0002 * sr));
            shCoefFastRel = 1.0f - (float) std::exp (-1.0 / (0.012 * sr));
            shCoefSlow    = 1.0f - (float) std::exp (-1.0 / (0.020 * sr));
            shCoefSlowRel = 1.0f - (float) std::exp (-1.0 / (0.080 * sr));
            for (auto& b : body) b.reset();
            for (auto& c : chr) c.reset();
            for (auto& f : flt) f.reset();
            active = false;
        }

        /** The engine's memory of how loud each recorded attack meets the model. */
        void setAttackMemory (AttackMemory* m) { atkMem = m; }

        bool  isActive()   const { return active; }
        bool  isKeyDown()  const { return active && keyDown; }
        bool  isLetGo()    const { return letGoDone; }
        int   getNote()    const { return note; }
        long  getAge()     const { return age; }
        float getPitchSemis() const { return (float) note + glideOffset; }
        bool  sostenutoHeld = false;

        //======================================================================
        void start (const NoteStart& ns, const Snapshot& s, long ageStamp)
        {
            if (active && ! stealing)
            {
                // fade the old note out quickly, then start this one
                pending = ns;
                pendingSnapAge = ageStamp;
                hasPending = true;
                stealing = true;
                stealLeft = stealLen;
                return;
            }
            begin (ns, s, ageStamp, false);
        }

        /** Mono: move to a new note.  legato = no re-articulation. */
        void moveTo (const NoteStart& ns, const Snapshot& s, bool legato, long ageStamp)
        {
            if (! active) { begin (ns, s, ageStamp, false); return; }
            const float from = (float) note + glideOffset;
            NoteStart n2 = ns;
            const float portMs = law::portaMs (s[P::port]);
            float t = portMs;
            const float lv = s[P::leg_vel] / 100.0f;
            if (lv > 0.0f) t *= dsp::lerp (1.0f, 0.25f, ns.velocity * lv);
            n2.glideFromSemis = from - (float) ns.note;
            n2.glideMs = t;
            begin (n2, s, ageStamp, legato);
        }

        /** Key up. */
        void keyUp (const Snapshot& s, const Live& live)
        {
            if (! active || ! keyDown) return;
            keyDown = false;
            keyUpTime = elapsed;
            updateLetGo (s, live);
        }

        /** Pedal or sostenuto moved: maybe let go now. */
        void updateLetGo (const Snapshot& s, const Live& live)
        {
            if (! active || letGoDone || keyDown) return;
            const float pedal = effectivePedal (live);
            if (pedal >= 0.5f || sostenutoHeld) { modelReleaseOnce (s, live); return; }
            letGo (s, live);
        }

        void kill() { active = false; hasPending = false; stealing = false; atk.on = false; tone.stop(); }

        /** Fast fade (archetype change, all-notes-off). */
        void fadeOut() { if (active) { hasPending = false; stealing = true; stealLeft = stealLen; } }

        //======================================================================
        void render (const Snapshot& s, const Live& live, float* outL, float* outR, int n)
        {
            int done = 0;
            while (done < n && active)
            {
                const int todo = std::min (kCtl, n - done);
                renderBlock (s, live, outL + done, outR + done, todo);
                done += todo;
            }
        }

    private:
        //======================================================================
        void begin (const NoteStart& ns, const Snapshot& s, long ageStamp, bool legato)
        {
            const bool wasActive = active;
            active = true;
            stealing = false;
            hasPending = false;
            arch = s.arch;
            note = ns.note;
            vel  = ns.velocity;
            age  = ageStamp;
            art  = ns.art;
            fx   = artFx (ns.art, s.arch);
            keyDown = true;
            letGoDone = false;
            modelReleased = false;
            sostenutoHeld = false;
            elapsed = 0.0;
            keyUpTime = -1.0;
            rollTimer = 0.0;

            // humanize and variation: every note a little different
            rng = ns.seed * 2654435761u + 12345u;
            const float hum = s[P::humanize] / 100.0f;
            const float var = s[P::amp_var] / 100.0f;
            humanCents = (rnd() * 2.0f - 1.0f) * hum * 10.0f;
            vel = dsp::clampv (vel * fx.velMul * (1.0f + (rnd() * 2.0f - 1.0f) * hum * 0.12f), 0.02f, 1.0f);
            varBright = (rnd() * 2.0f - 1.0f) * var * 0.3f;
            varLevel  = 1.0f + (rnd() * 2.0f - 1.0f) * var * 0.12f;

            // portamento / slide
            glideOffset = ns.glideFromSemis;
            if (fx.pitch == PitchFx::Slide && std::abs (glideOffset) < 0.01f && ! legato) glideOffset = -2.0f;
            float gms = ns.glideMs;
            if (fx.pitch == PitchFx::Slide) gms = std::max (gms, 70.0f);
            glideRate = (std::abs (glideOffset) > 0.001f && gms > 0.5f) ? std::abs (glideOffset) / (gms * 0.001f) : 1.0e9f;
            if (gms <= 0.5f) glideOffset = 0.0f;

            // attack glide (scoop into the note)
            agActive = ns.attackGlide;
            agDepth  = law::glideCents (s[P::ag_depth]) / 100.0f;
            agTime   = law::glideMs (s[P::ag_time]) * 0.001f;
            agK      = law::shapeK (s[P::ag_shape]);

            // envelopes
            const bool fresh = ! legato || ! wasActive;
            if (fresh || fx.forceRetrig || ! legato)
            {
                ampEnv.curveK = law::shapeK (s[P::amp_shape]);
                ampEnv.noteOn (law::envA (s[P::amp_a]) * 1000.0f, 0.0f,
                               s[P::amp_d] >= 99.5f ? 1.0e9f : law::envD (s[P::amp_d]) * 1000.0f,
                               s[P::amp_s] / 100.0f, sr, wasActive);
                fltEnv.curveK = 4.0f;
                fltEnv.noteOn (law::envA (s[P::fenv_a]) * 1000.0f, 0.0f,
                               s[P::fenv_d] >= 99.5f ? 1.0e9f : law::envD (s[P::fenv_d]) * 1000.0f,
                               s[P::fenv_s] / 100.0f, sr, wasActive);
            }
            if (fresh)
            {
                const double ph = rnd();
                vibLfo.reset (0.0);
                ampLfo.reset (ph);
                fltLfo.reset (ph);
                for (auto& b : body) b.reset();
                for (auto& c : chr) c.reset();
                for (auto& f : flt) f.reset();
                drive = legato ? drive : 0.0f;
                dcL.prepare (sr, 15.0); dcR.prepare (sr, 15.0);
                dcL.reset(); dcR.reset();
            }

            // drive (breath / bow / bellows) envelope for sustained archetypes: how fast
            // each mechanism really speaks.  A tongued or bowed note is set going in tens
            // of milliseconds, harder playing faster; organ keys close at once.
            const float tr = dsp::clampv (s[P::exc_transient] / 100.0f + fx.transientAdd, 0.0f, 1.0f);
            const float velFast = dsp::lerp (1.35f, 0.7f, vel);
            float attS = 0.05f;
            switch (s.arch)
            {
                case Archetype::BowedString: attS = dsp::lerp (0.004f, 0.25f, std::pow (s[P::bw_attack] / 100.0f, 1.5f)) * velFast; break;
                case Archetype::Tonewheel:   attS = dsp::lerp (0.010f, 0.0015f, tr); break;
                case Archetype::PipeOrgan:   attS = dsp::lerp (0.030f, 0.004f, tr); break;   // the ranks' own speech does the rest
                case Archetype::FreeReed:    attS = dsp::lerp (0.060f, 0.008f, tr) * velFast; break;
                case Archetype::SingleReed:
                case Archetype::DoubleReed:  attS = dsp::lerp (0.015f, 0.001f, tr) * velFast; break;   // the breath is already there behind the tongue: the reed's own attack (TonguedAttack) does the shaping
                default:                     attS = dsp::lerp (0.040f, 0.003f, tr) * velFast; break;   // breath: tongued
            }
            attS *= fx.attackMul;
            driveRise = (float) (1.0 / std::max (1.0, attS * sr));
            if (legato && wasActive) driveRise *= 3.0f;

            // the tongue (and the bow's bite): a pressure spike as the note is released,
            // dying in a few tens of milliseconds - what makes a blown note speak rather
            // than swell in.  Not on a slur.
            tongue = 0.0f;
            if (! (legato && wasActive))
            {
                switch (s.arch)
                {
                    case Archetype::LipReed:     tongue = (0.08f + 0.3f * tr) * (0.4f + 0.6f * vel); break;
                    case Archetype::SingleReed:
                    case Archetype::DoubleReed:  tongue = (0.06f + 0.22f * tr) * (0.4f + 0.6f * vel); break;
                    case Archetype::AirJet:      tongue = (0.08f + 0.3f * tr) * (0.4f + 0.6f * vel); break;
                    case Archetype::BowedString: tongue = (0.05f + 0.25f * tr) * vel; break;
                    case Archetype::FreeReed:    tongue = 0.08f * tr * vel; break;
                    default: break;
                }
            }

            // the transient shaper (struck and plucked archetypes): ATTACK above the middle
            // lifts the first milliseconds of each note, below it softens them
            shapeAmt = (s.arch == Archetype::StruckString || s.arch == Archetype::TineEP || s.arch == Archetype::StruckBar
                        || s.arch == Archetype::PluckedString) ? (tr - 0.5f) * (tr > 0.5f ? 1.6f : 0.9f) : 0.0f;
            if (fresh) { shFast = shSlow = 0.0f; }
            driveFall = (float) (1.0 / ((s.arch == Archetype::BowedString ? 0.08 : 0.045) * sr));
            if (fx.forceRetrig && wasActive && s.sustained) drive *= 0.35f;   // re-tongue

            // unison
            nUnits = juce::jlimit (1, kMaxUnison, (int) std::lround (s[P::res_unison]));
            const float d = s[P::res_detune] / 100.0f * 25.0f;
            static const float spread2[2] = { -0.5f, 0.5f }, spread3[3] = { -1.0f, 0.0f, 1.0f };
            for (int u = 0; u < nUnits; ++u)
            {
                const float k = nUnits == 1 ? 0.0f : (nUnits == 2 ? spread2[u] : spread3[u]);
                unitCents[u] = k * d + (nUnits > 1 ? (rnd() - 0.5f) * d * 0.15f : 0.0f);
                unitPan[u]   = k * 0.75f;
            }

            // the models
            ModelIn in;
            fillModelIn (in, s, Live{}, (float) note, 0);
            in.percTrigger = ns.percTrigger;
            for (int u = 0; u < nUnits; ++u)
            {
                in.seed = ns.seed * 31u + (std::uint32_t) u * 7919u;
                in.freq = baseFreq * std::pow (2.0f, unitCents[u] / 1200.0f);
                if (fx.softPluck) in.velocity *= 0.45f;
                units[u].get (arch).start (in, legato && wasActive && (fx.forceLegato || ! fx.forceRetrig));
            }
            lastIn = in;

            // the recorded attack: a new note's first moments, not a slur
            if (! (legato && wasActive)) { atk.on = false; startAttack (s); }
            else atk.moved = true;      // slurred on while the attack plays: it finishes, but teaches nothing

            startTone (s, ! (legato && wasActive));
        }

        //======================================================================
        //  TONE LAYER.  AUTO picks the instrument the panel describes (and its
        //  family's neighbours outside its range); the source choice names one,
        //  a designed character among them, or turns it off.
        //======================================================================
        int autoToneSet (const Snapshot& s, float hz) const
        {
            using A = Archetype;
            using T = ToneSet;
            T fam[3]; int nf = 0;
            auto add = [&] (T t) { if (nf < 3) fam[nf++] = t; };
            switch (s.arch)
            {
                case A::StruckString: add (T::Piano); break;
                case A::StruckBar:
                {
                    const float m = s[P::sb_material];
                    if (m < 15.0f) add (T::Marimba);
                    else if (m >= 40.0f && m < 65.0f) add (T::Vibraphone);
                    else return -1;
                    break;
                }
                case A::PluckedString:
                    if (s[P::pl_ring] >= 80.0f && s[P::pl_plectrum] <= 30.0f && s[P::pl_amp] < 0.5f) add (T::Harp);
                    else return -1;
                    break;
                case A::BowedString:
                {
                    const float b = s[P::body_size];
                    if (b < 38.0f)      { add (T::Violin); add (T::Viola); add (T::Cello); }
                    else if (b < 58.0f) { add (T::Viola); add (T::Violin); add (T::Cello); }
                    else if (b < 83.0f) { add (T::Cello); add (T::Viola); add (T::Contrabass); }
                    else                { add (T::Contrabass); add (T::Cello); }
                    break;
                }
                case A::LipReed:
                {
                    const float bore = s[P::lr_bore];
                    const int mute = (int) std::lround (s[P::lr_mute]);
                    if (bore < 45.0f && (mute == 1 || mute == 2)) { add (T::TrumpetStraight); add (T::Trumpet); }
                    else if (bore < 45.0f && mute >= 3)           { add (T::TrumpetHarmon); add (T::Trumpet); }
                    else if (bore < 45.0f) { add (T::Trumpet); add (T::Trombone); }
                    else if (bore < 75.0f) { add (T::Trombone); add (T::Tuba); add (T::Trumpet); }
                    else if (bore < 92.0f) { add (T::Horn); add (T::Trombone); }
                    else                   { add (T::Tuba); add (T::Trombone); }
                    break;
                }
                case A::SingleReed:
                {
                    const float bore = s[P::sr_bore];
                    if (bore < 50.0f) add (T::Clarinet);
                    else if (bore >= 95.0f) { add (T::TenorSax); add (T::SopranoSax); }
                    else { add (T::SopranoSax); add (T::TenorSax); }
                    break;
                }
                case A::DoubleReed:
                    if (s[P::dr_bore] < 60.0f) { add (T::Oboe); add (T::Bassoon); }
                    else                       { add (T::Bassoon); add (T::Oboe); }
                    break;
                case A::AirJet:
                {
                    const float pipe = s[P::aj_pipe];
                    if (pipe < 10.0f)       { add (T::Flute); add (T::Piccolo); }
                    else if (pipe < 40.0f)  { add (T::Recorder); add (T::Flute); }
                    else if (pipe >= 80.0f) { add (T::Ocarina); add (T::Recorder); }
                    else                    { add (T::Ney); }                       // pan pipes: the ney's breath
                    break;
                }
                case A::FreeReed:
                {
                    const int reg = (int) std::lround (s[P::fr_register]);
                    if (reg == 1 && s[P::fr_chamber] < 10.0f) add (T::Harmonica);
                    else if (reg == 7) add (T::Organ);
                    else return -1;
                    break;
                }
                case A::PipeOrgan: add (T::Organ); break;
                default: return -1;
            }
            const auto& lib = ToneLibrary::get();
            int best = -1; float bestOut = 1.0e9f;
            for (int i = 0; i < nf; ++i)
            {
                const float out = lib.semisOutside ((int) fam[i], hz);
                if (out <= 3.0f) return (int) fam[i];
                if (out < bestOut) { bestOut = out; best = (int) fam[i]; }
            }
            return best;     // the layer is a colour: the nearest table stretches well past its range
        }

        void startTone (const Snapshot& s, bool fresh)
        {
            const int src = (int) std::lround (s[P::tl_source]);
            const bool wanted = (s[P::tl_mix] > 0.05f || s[P::tl_sub] > 0.05f) && src != 1
                                && arch != Archetype::Synth && arch != Archetype::Tonewheel;
            if (! wanted) { tone.stop(); return; }
            const int set = src >= 2 ? src - 2 : autoToneSet (s, baseFreq);
            if (set < 0)
            {
                // nothing to colour with: the sub alone still gives depth (a plain sine table)
                if (s[P::tl_sub] > 0.05f) tone.start ((int) ToneSet::Warm, baseFreq, 0.5f, 1.0f, fresh);
                else tone.stop();
                toneNoTable = true;
                return;
            }
            toneNoTable = false;
            const float morph = dsp::clampv (0.1f + 0.8f * vel + s[P::tl_color] / 100.0f, 0.0f, 1.0f);
            tone.start (set, baseFreq, morph, std::pow (2.0f, s[P::tl_formant] / 100.0f), fresh);
        }

        //======================================================================
        //  SAMPLED ATTACK.  AUTO reads the instrument the panel describes and
        //  chooses the set (and its neighbours in the family for notes outside
        //  its range); the source choice can name one instead, or turn it off.
        //======================================================================
        int autoAttackSet (const Snapshot& s, float hz) const
        {
            using A = Archetype;
            using S = AttackSet;
            S fam[3]; int nf = 0;
            auto add = [&] (S a) { if (nf < 3) fam[nf++] = a; };
            switch (s.arch)
            {
                case A::StruckString: add (S::Piano); break;
                case A::PluckedString:
                {
                    if (s[P::pl_amp] >= 0.5f) return -1;                                      // electric guitars, basses, clavinet
                    if (s[P::pl_plectrum] >= 90.0f) add (S::Harpsichord);
                    else if (s[P::pl_ring] >= 80.0f && s[P::pl_plectrum] <= 30.0f) add (S::Harp);
                    else return -1;                                                            // guitars: the model's own pluck
                    break;
                }
                case A::StruckBar:
                {
                    const float m = s[P::sb_material];
                    add (m < 15.0f ? S::Marimba : m < 40.0f ? S::Xylophone : m < 65.0f ? S::Vibraphone
                         : (m >= 76.5f && m < 90.0f) ? S::TubularBells : S::Glockenspiel);
                    break;
                }
                case A::BowedString:
                {
                    const float b = s[P::body_size];
                    if (b < 38.0f)      { add (S::Violin); add (S::Viola); add (S::Cello); }
                    else if (b < 58.0f) { add (S::Viola); add (S::Violin); add (S::Cello); }
                    else if (b < 83.0f) { add (S::Cello); add (S::Viola); add (S::Contrabass); }
                    else                { add (S::Contrabass); add (S::Cello); }
                    break;
                }
                case A::LipReed:
                {
                    const float bore = s[P::lr_bore];
                    const int mute = (int) std::lround (s[P::lr_mute]);
                    if (bore < 45.0f && (mute == 1 || mute == 2)) { add (S::TrumpetStraight); add (S::Trumpet); }
                    else if (bore < 45.0f && mute >= 3)           { add (S::TrumpetHarmon); add (S::Trumpet); }
                    else if (bore < 45.0f) { add (S::Trumpet); add (S::Trombone); }
                    else if (bore < 75.0f) { add (S::Trombone); add (S::Tuba); add (S::Trumpet); }
                    else if (bore < 92.0f) { add (S::Horn); add (S::Trombone); }
                    else                   { add (S::Tuba); add (S::Trombone); }
                    break;
                }
                case A::SingleReed:
                {
                    const float bore = s[P::sr_bore];
                    if (bore < 50.0f) add (S::Clarinet);
                    else if (bore >= 95.0f) { add (S::TenorSax); add (S::SopranoSax); }
                    else { add (S::SopranoSax); add (S::TenorSax); }
                    break;
                }
                case A::DoubleReed:
                    if (s[P::dr_bore] < 60.0f) { add (S::Oboe); add (S::Bassoon); }
                    else                       { add (S::Bassoon); add (S::Oboe); }
                    break;
                case A::AirJet:
                {
                    const float pipe = s[P::aj_pipe];
                    if (pipe < 10.0f)      { add (S::Flute); add (S::Piccolo); }
                    else if (pipe < 40.0f) { add (S::Recorder); add (S::Flute); }
                    else if (pipe >= 80.0f){ add (S::Ocarina); add (S::Recorder); }
                    else return -1;                                                            // pan pipes: the model's own chiff
                    break;
                }
                case A::FreeReed:
                    if ((int) std::lround (s[P::fr_register]) == 1 && s[P::fr_chamber] < 10.0f) add (S::Harmonica);
                    else return -1;
                    break;
                default: return -1;
            }
            // the first of the family that covers the note (within a few semitones), else the closest
            const auto& lib = AttackLibrary::get();
            int best = -1; float bestOut = 1.0e9f;
            for (int i = 0; i < nf; ++i)
            {
                const float out = lib.semisOutside ((int) fam[i], hz);
                if (out <= 3.0f) return (int) fam[i];
                if (out < bestOut) { bestOut = out; best = (int) fam[i]; }
            }
            return bestOut <= 9.0f ? best : -1;
        }

        void startAttack (const Snapshot& s)
        {
            const auto& lib = AttackLibrary::get();
            const float amount = s[P::exc_sample] / 100.0f;
            const int src = (int) std::lround (s[P::exc_source]);
            if (! lib.ok() || amount <= 0.001f || src == 1 || atkMem == nullptr) return;
            if (fx.excite != Excite::Normal || fx.harmonic || fx.softPluck) return;        // pizzicato, col legno, harmonics...
            const float hz = baseFreq;
            int set = -1;
            if (src >= 2) { set = src - 2; if (lib.semisOutside (set, hz) > 12.0f) return; }
            else set = autoAttackSet (s, hz);
            if (set < 0 || set >= kNumAttackSets) return;
            const int idx = lib.pick (set, hz, vel);
            if (idx < 0) return;

            const auto& sm = lib.sample (idx);
            const auto& st = lib.set (set);
            const float r = (float) lib.rate() * 0.001f;
            atk.pcm     = lib.pcm (idx);
            atk.len     = sm.length;
            atk.rootHz  = sm.rootHz;
            atk.srcRate = (float) lib.rate();
            atk.headS   = std::max (st.headMs, sm.headMs) * r;
            atk.fadeS   = std::max (20.0f * r, std::min (st.fadeMs * r, (float) sm.length - 20.0f * r - atk.headS));
            atk.winS    = 10.0f * r;
            atk.fM = atk.fS = 0.0f;
            atkFollow = 1.0f - (float) std::exp (-1.0 / (std::max (0.006, 2.5 / std::max (20.0, (double) hz)) * sr));   // smooth over a few periods
            atk.pos     = 0.0;
            atk.mix     = std::min (1.0f, amount);
            atk.boost   = amount > 1.0f ? dsp::dbToGain (12.0f * (amount - 1.0f)) : 1.0f;   // the transient gainer: up to +12 dB at the onset
            atk.set     = set;
            atk.gHead   = atkMem->lookup (set, note, vel);
            atk.gMatch  = atk.gHead;
            atk.gEnd    = atk.gHead * atk.boost;
            atk.boostK  = -1.0f / (0.030f * (float) lib.rate());
            atk.em = atk.es = 0.0;
            atk.matched = false;
            atk.moved = false;
            atk.on = true;
        }

        /** One sample of the attack against the model's mid (after body and character):
            returns what to add, and how much of the model to keep. */
        float attackTick (float modelMid, double step, float& modelW)
        {
            modelW = 1.0f;
            const double p = atk.pos;
            if (p >= (double) (atk.len - 3) || p >= (double) (atk.headS + atk.fadeS)) { atk.on = false; return 0.0f; }
            const int i = (int) p;
            const float f = (float) (p - (double) i);
            const std::int16_t* d = atk.pcm;
            const float xm1 = i > 0 ? (float) d[i - 1] : 0.0f, x0 = (float) d[i], x1 = (float) d[i + 1], x2 = (float) d[i + 2];
            // 4-point Hermite
            const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            const float x = (((c3 * f + c2) * f + c1) * f + x0) * (1.0f / 32768.0f);
            atk.pos += step;
            atk.fM += (modelMid * modelMid - atk.fM) * atkFollow;
            atk.fS += (x * x - atk.fS) * atkFollow;

            float g, wS;
            if (p < (double) atk.headS)
            {
                if (p >= (double) (atk.headS - atk.winS)) { atk.em += (double) modelMid * modelMid; atk.es += (double) x * x; }
                // the transient itself at the level remembered; through the head's second half the
                // recording settles onto the model's own level, so the handover starts level
                float gh = atk.gHead;
                const double half = 0.5 * (double) atk.headS;
                if (p > half && atk.fS > 1.0e-12f)
                {
                    const float u = (float) ((p - half) / half), w = u * u * (3.0f - 2.0f * u);
                    const float lock = dsp::clampv (std::sqrt (atk.fM / atk.fS), atk.gHead * 0.25f, atk.gHead * 4.0f);
                    gh += (lock - gh) * w;
                }
                // the transient gainer lifts the onset, fading over its first 30 ms
                const float bst = atk.boost > 1.0f ? 1.0f + (atk.boost - 1.0f) * std::exp ((float) p * atk.boostK) : 1.0f;
                atk.gEnd = gh * bst;
                g = atk.gEnd;
                wS = atk.mix;
                modelW = 1.0f - atk.mix;
            }
            else
            {
                if (! atk.matched)
                {
                    atk.matched = true;
                    if (atk.es > 1.0e-10)
                    {
                        atk.gMatch = dsp::clampv ((float) std::sqrt (atk.em / atk.es), 1.0e-4f, 100.0f);
                        if (! atk.moved)   // an odd note moves the memory a step, not all the way
                            atkMem->store (atk.set, note, vel, dsp::clampv (atk.gMatch, atk.gHead * 0.125f, atk.gHead * 8.0f));
                    }
                }
                const float q = dsp::clampv ((float) ((p - (double) atk.headS) / (double) atk.fadeS), 0.0f, 1.0f);
                const float sq = q * q * (3.0f - 2.0f * q);
                // through the handover the recording follows the model's level moment by moment,
                // so a recording still swelling (or the model still settling) cannot bulge the seam
                const float lock = atk.fS > 1.0e-12f ? dsp::clampv (std::sqrt (atk.fM / atk.fS), atk.gMatch * 0.25f, atk.gMatch * 4.0f) : atk.gMatch;
                g = atk.gEnd + (lock - atk.gEnd) * sq;
                const float a = q * 1.5707963f;
                wS = atk.mix * std::cos (a);
                modelW = 1.0f - atk.mix + atk.mix * std::sin (a);
            }
            return x * g * wS;
        }

        void letGo (const Snapshot& s, const Live& live)
        {
            if (letGoDone) return;
            letGoDone = true;
            modelReleaseOnce (s, live);
            float r = law::envR (s[P::amp_r]);
            if (fx.releaseMinS > 0.0f) r = std::max (r, fx.releaseMinS);
            if (fx.releaseMaxS > 0.0f) r = std::min (r, fx.releaseMaxS);
            ampEnv.noteOff (r * 1000.0f, sr);
            fltEnv.noteOff (law::envR (s[P::fenv_r]) * 1000.0f, sr);
            letGoTime = elapsed;
        }

        void modelReleaseOnce (const Snapshot& s, const Live& live)
        {
            if (modelReleased) return;
            modelReleased = true;
            ModelIn in;
            fillModelIn (in, s, live, (float) note, 0);
            in.released = true;
            for (int u = 0; u < nUnits; ++u) units[u].get (arch).release (in);
        }

        float effectivePedal (const Live& live) const
        {
            float p = live.pedal;
            if (fx.pedal >= 0.0f) p = std::max (p, fx.pedal);
            if (sostenutoHeld) p = 1.0f;
            return p;
        }

        float rnd()
        {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            return (float) (rng & 0xFFFFFF) / 16777215.0f;
        }

        //======================================================================
        void fillModelIn (ModelIn& in, const Snapshot& s, const Live& live, float semis, int)
        {
            in.note      = note;
            in.velocity  = vel;
            in.strength  = s[P::exc_strength] / 100.0f;
            in.hardness  = s[P::exc_hardness] / 100.0f;
            in.noise     = dsp::clampv (s[P::exc_noise] / 100.0f + fx.noiseAdd, 0.0f, 1.0f);
            in.transient = dsp::clampv (s[P::exc_transient] / 100.0f + fx.transientAdd, 0.0f, 1.0f);
            in.decay     = s[P::res_decay] / 100.0f;
            in.damping   = s[P::res_damping] / 100.0f;
            in.inharm    = s[P::res_inharm] / 100.0f;
            in.oddEven   = s[P::res_oddeven] / 100.0f;
            for (int i = 0; i < kMaxSpec; ++i) in.spec[i] = s.spec[i];
            if (fx.overblowSpec >= 0.0f && arch == Archetype::AirJet) in.spec[3] = fx.overblowSpec;
            if (fx.subtoneSpec >= 0.0f && arch == Archetype::SingleReed) in.spec[3] = std::max (in.spec[3], fx.subtoneSpec);
            in.dampAdd   = fx.dampAdd;
            in.decayMul  = fx.decayMul;
            in.excite    = fx.excite;
            in.mute      = fx.mute;
            in.harmonic  = fx.harmonic;
            in.soft      = std::max (fx.soft, live.soft);
            in.pedal     = effectivePedal (live);
            in.released  = ! keyDown;
            in.stateOffset = s.regOffset;
            in.osc       = &s.osc;
            const float baseHz = 440.0f * std::pow (2.0f, (semis - 69.0f) / 12.0f);
            baseFreq = baseHz * fx.freqMul;
            in.freq = baseFreq;
        }

        //======================================================================
        void renderBlock (const Snapshot& s, const Live& live, float* outL, float* outR, int n)
        {
            const double dt = (double) n / sr;
            elapsed += dt;

            // ---- staccato lets go by itself; rolls re-strike ---------------------
            if (fx.autoRelMs > 0.0f && keyDown && elapsed * 1000.0 >= fx.autoRelMs)
            {
                keyDown = false;
                keyUpTime = elapsed;
                letGo (s, live);
            }
            if (fx.roll && keyDown && ! s.sustained)
            {
                rollTimer += dt;
                const double period = 1.0 / std::max (2.0f, law::artRate (s[P::art_rate]));
                if (rollTimer >= period)
                {
                    rollTimer -= period;
                    ModelIn in = lastIn;
                    in.velocity = vel * (0.6f + 0.35f * rnd());
                    in.seed = rng;
                    for (int u = 0; u < nUnits; ++u) units[u].get (arch).start (in, false);
                }
            }

            // ---- pitch ------------------------------------------------------------
            if (glideOffset != 0.0f)
            {
                const float step = glideRate * (float) dt;
                glideOffset = std::abs (glideOffset) <= step ? 0.0f : glideOffset - std::copysign (step, glideOffset);
            }
            float semis = (float) note + glideOffset + live.bend + humanCents / 100.0f;

            // vibrato: the panel's depth, plus what the wheel (or aftertouch) brings in up to
            // WHEEL VIBRATO; a wheel resting a hair off zero brings in nothing
            float vibCents = law::vibCents (s[P::vib_depth]);
            const int mwT = (int) s[P::mw_target], atT = (int) s[P::at_target];
            const float wheelCents = law::vibCents (s[P::vib_wheel]);
            auto dead = [] (float x) { return x <= 0.04f ? 0.0f : (x - 0.04f) / 0.96f; };
            if (mwT == 0) vibCents += dead (live.mw) * wheelCents;
            if (atT == 0) vibCents += dead (live.at) * wheelCents;
            if (vibCents > 0.01f)
                semis += vibLfo.tick (law::vibRate (s[P::vib_rate]), law::lfoDelay (s[P::vib_delay]), dt) * vibCents / 100.0f;

            if (agActive)
            {
                const double p = elapsed / std::max (0.001, (double) agTime);
                if (p >= 1.0) agActive = false;
                else semis -= agDepth * (1.0f - AHDSREnvelope::fallAmount ((float) p, agK));
            }
            semis += pitchFx (s);

            // ---- dynamics -------------------------------------------------------------
            const int src = (int) s[P::dyn_src];
            float dyn = vel;
            switch (src)
            {
                case 1: dyn = live.mw; break;
                case 2: dyn = live.breath; break;
                case 3: dyn = live.expr; break;
                case 4: dyn = live.at; break;
                default: break;
            }
            float levelMod = 1.0f;
            if (mwT == 2) levelMod *= live.mw;
            if (atT == 2) levelMod *= 0.3f + 0.7f * live.at;

            float bright = varBright + fx.brightAdd;
            if (mwT == 1) bright += live.mw;
            if (atT == 1) bright += live.at * 0.8f;
            const float wah = mwT == 3 ? live.mw : (atT == 3 ? live.at : 0.5f);

            // ---- drive envelope (sustained archetypes) ------------------------------
            const bool driveUp = ! letGoDone;
            float target = 1.0f;
            if (fx.accent > 0.0f) target += fx.accent * (float) std::exp (-elapsed / 0.12);
            if (tongue > 0.0f)    target += tongue * (float) std::exp (-elapsed / 0.035);
            if (fx.sfz)
                target = elapsed < 0.06 ? 1.35f : (float) (0.42 + 0.93 * std::exp (-(elapsed - 0.06) / 0.12));
            if (driveUp)
            {
                if (drive < target) drive = std::min (target, drive + driveRise * (float) n);
                else                drive = std::max (target, drive - driveRise * 0.5f * (float) n);
            }
            else drive = std::max (0.0f, drive - driveFall * (float) n);

            float pressure = drive;
            float growl = fx.growl;
            if (fx.amRate > 0.0f)
            {
                amPh += law::artRate (s[P::art_rate]) * fx.amRate * dt;
                amPh -= std::floor (amPh);
                const float m = std::sqrt (std::abs ((float) std::sin (dsp::kPi * amPh * 2.0)));
                pressure *= (1.0f - fx.amDepth) + fx.amDepth * m;
            }
            if (fx.flutter > 0.0f)
            {
                flPh += 24.0 * dt;
                flPh -= std::floor (flPh);
                if (arch != Archetype::AirJet && arch != Archetype::LipReed)
                    pressure *= 1.0f + fx.flutter * (flPh < 0.5 ? 0.22f : -0.22f);
                growl += fx.flutter * 0.5f;
            }
            if (fx.growl > 0.0f && arch != Archetype::LipReed && arch != Archetype::AirJet)
                pressure *= 1.0f + fx.growl * 0.25f * (rnd() * 2.0f - 1.0f);

            // ---- the model ----------------------------------------------------------
            ModelIn in;
            fillModelIn (in, s, live, semis, 0);
            in.pressure = s.sustained ? pressure : 0.0f;
            in.dyn      = dsp::clampv (dyn, 0.0f, 1.0f);
            in.bright   = dsp::clampv (bright, -1.0f, 1.0f);
            in.growl    = growl;
            in.wah      = wah;
            if (s.dynMode == Dynamics::Direct) in.pressure = pressure * in.dyn;
            lastIn = in;

            float uL[kCtl] = {}, uR[kCtl] = {};
            for (int u = 0; u < nUnits; ++u)
            {
                in.freq = baseFreq * std::pow (2.0f, unitCents[u] / 1200.0f);
                float buf[kCtl];
                units[u].get (arch).render (in, buf, n);
                const float pan = unitPan[u];
                const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
                for (int i = 0; i < n; ++i) { uL[i] += buf[i] * gl; uR[i] += buf[i] * gr; }
            }
            const float uniNorm = nUnits == 1 ? 1.4142f : (nUnits == 2 ? 1.1f : 0.95f);

            // ---- level -------------------------------------------------------------
            float level = (s[P::amp_gain] / 100.0f) * varLevel * levelMod * uniNorm;
            switch (s.dynMode)
            {
                case Dynamics::Physical: level *= 0.25f + 0.75f * in.dyn; break;
                case Dynamics::Swell:    level *= in.dyn; break;
                case Dynamics::Struck:   if (src != 0) level *= in.dyn; break;
                default: break;
            }

            // ---- body, character, filter ---------------------------------------------
            const bool useBody = arch != Archetype::Synth && s[P::body_mix] > 0.5f;
            if (useBody)
                for (auto& b : body) b.configure (s[P::body_size] / 100.0f, s[P::body_material] / 100.0f,
                                                  s[P::body_reso] / 100.0f, s[P::body_mix] / 100.0f, sr);
            for (auto& c : chr) c.configure (s[P::chr_drive] / 100.0f, s[P::chr_sat] / 100.0f, s[P::chr_tone] / 100.0f, sr);

            const int ftype = (int) s[P::flt_type];
            const float fAmt = s[P::fenv_amt], lfoD = s[P::flfo_depth], key = s[P::flt_key];
            const bool filterOn = ! (ftype == 0 && s[P::flt_cut] >= 99.5f && std::abs (fAmt) < 0.5f && lfoD < 0.5f && key < 0.5f);
            const float fe = fltEnv.level;
            for (int i = 0; i < n; ++i) fltEnv.process();
            if (filterOn)
            {
                float ui = s[P::flt_cut] + fAmt * fe + (key / 100.0f) * ((float) note - 60.0f) * 0.836f;
                if (lfoD > 0.5f) ui += fltLfo.tick (law::lfoRate (s[P::flfo_rate]), law::lfoDelay (s[P::flfo_delay]), dt) * lfoD * 0.3f;
                if (mwT == 1) ui += live.mw * 15.0f;
                const float hz = law::cutoffHz (dsp::clampv (ui, 0.0f, 100.0f));
                for (auto& f : flt) f.set (hz, s[P::flt_res] / 100.0f * 0.97f, ftype, sr);
            }

            float trem = 1.0f;
            const float aD = s[P::alfo_depth] / 100.0f;
            if (aD > 0.0f)
                trem = 1.0f - aD * 0.5f * (1.0f - ampLfo.tick (law::lfoRate (s[P::alfo_rate]), law::lfoDelay (s[P::alfo_delay]), dt));

            // transient shaper: a fast and a slow follower of the note; where the fast one
            // runs ahead (the attack) the note is lifted (or softened) by their ratio
            if (shapeAmt != 0.0f)
            {
                const float aF = shCoefFast, rF = shCoefFastRel, aS = shCoefSlow, rS = shCoefSlowRel;
                for (int i = 0; i < n; ++i)
                {
                    const float x = 0.5f * (std::abs (uL[i]) + std::abs (uR[i]));
                    shFast += (x - shFast) * (x > shFast ? aF : rF);
                    shSlow += (x - shSlow) * (x > shSlow ? aS : rS);
                    const float ratio = shFast / std::max (1.0e-6f, shSlow);
                    if (ratio > 1.001f)
                    {
                        const float g = dsp::clampv (std::exp (shapeAmt * std::log (ratio)), 0.5f, 4.0f);
                        uL[i] *= g;  uR[i] *= g;
                    }
                }
            }

            const double atkStep = atk.on ? (double) (baseFreq / atk.rootHz) * (double) atk.srcRate / sr : 0.0;
            // the tone layer: up to 60 it is added under the model, past 60 it takes over from it
            // (the model kept at a quarter by 100); the two together kept at the model's loudness
            const bool toneOn = tone.isActive();
            const float toneHz  = baseFreq * std::pow (2.0f, s[P::tl_detune] * 0.2f / 1200.0f);    // up to 20 cents off the model
            const float tlMix   = toneNoTable ? 0.0f : s[P::tl_mix] / 100.0f;
            const float layerG  = tlMix * 1.4f;
            const float modelG  = tlMix > 0.6f ? 1.0f - (tlMix - 0.6f) / 0.4f * 0.75f : 1.0f;
            const float toneNorm = 1.0f / std::sqrt (modelG * modelG + layerG * layerG + modelG * layerG);   // locked, they add partly in step
            const float toneMix = layerG * toneNorm, toneModel = modelG * toneNorm;
            const float toneSub = s[P::tl_sub] / 100.0f;
            const bool  toneLock = s[P::tl_detune] < 0.5f;
            for (int i = 0; i < n; ++i)
            {
                float l = uL[i], r = uR[i];
                if (useBody) { l = body[0].process (l); r = body[1].process (r); }
                l = chr[0].process (l); r = chr[1].process (r);
                if (toneOn)
                {
                    const float t = tone.tick (0.5f * (l + r), toneHz, toneMix, toneSub, toneLock);
                    l = l * toneModel + t;  r = r * toneModel + t;
                }
                if (atk.on)
                {
                    float wM;
                    const float a = attackTick (0.5f * (l + r), atkStep, wM);
                    l = l * wM + a;  r = r * wM + a;
                }
                if (filterOn) { l = flt[0].process (l); r = flt[1].process (r); }
                float g = ampEnv.process() * level * trem;
                if (stealing)
                {
                    g *= (float) stealLeft / (float) stealLen;
                    if (stealLeft > 0) --stealLeft;
                }
                outL[i] += dcL.process (l) * g;
                outR[i] += dcR.process (r) * g;
            }

            // ---- end of the voice ------------------------------------------------------
            if (stealing && stealLeft <= 0)
            {
                if (hasPending)
                {
                    hasPending = false;
                    active = false;
                    begin (pending, s, pendingSnapAge, false);
                }
                else active = false;
                return;
            }
            if (! ampEnv.isActive()) { active = false; return; }
            if (letGoDone && elapsed - letGoTime > 30.0) active = false;   // safety net
        }

        //======================================================================
        float pitchFx (const Snapshot& s)
        {
            const float rate = law::artRate (s[P::art_rate]);
            switch (fx.pitch)
            {
                case PitchFx::Scoop:
                    return elapsed < 0.14 ? -3.0f * (1.0f - AHDSREnvelope::fallAmount ((float) (elapsed / 0.14), 4.0f)) : 0.0f;
                case PitchFx::Fall:
                    if (keyUpTime < 0.0) return 0.0f;
                    return -7.0f * AHDSREnvelope::fallAmount ((float) std::min (1.0, (elapsed - keyUpTime) / 0.4), -3.0f);
                case PitchFx::Doit:
                    if (keyUpTime < 0.0) return 0.0f;
                    return 5.0f * AHDSREnvelope::fallAmount ((float) std::min (1.0, (elapsed - keyUpTime) / 0.35), -3.0f);
                case PitchFx::Bend:
                {
                    if (elapsed < 0.12) return 0.0f;
                    const float t = (float) std::min (1.0, (elapsed - 0.12) / 0.25);
                    return 2.0f * t * t * (3.0f - 2.0f * t);
                }
                case PitchFx::Shake:
                    return 1.5f + 1.5f * (float) std::sin (dsp::kTwoPi * rate * elapsed - dsp::kPi * 0.5);
                case PitchFx::Trill:
                {
                    const float sv = (float) std::sin (dsp::kTwoPi * rate * 0.5 * elapsed);
                    return s[P::art_trill] * (0.5f + 0.5f * std::tanh (sv * 6.0f));
                }
                default:
                    return 0.0f;
            }
        }

        //======================================================================
        double sr = 48000.0;
        ModelBank units[kMaxUnison];
        BodyFilter body[2];
        Character  chr[2];
        TptSvf     flt[2];
        dsp::DCBlocker dcL, dcR;
        AHDSREnvelope ampEnv, fltEnv;
        DelayedLfo vibLfo, ampLfo, fltLfo;
        ModelIn lastIn;

        struct AttackPlay
        {
            bool   on = false, matched = false, moved = false;
            const std::int16_t* pcm = nullptr;
            int    len = 0, set = 0;
            double pos = 0.0, em = 0.0, es = 0.0;
            float  rootHz = 440.0f, srcRate = 48000.0f;
            float  headS = 0.0f, fadeS = 1.0f, winS = 1.0f;
            float  mix = 1.0f, boost = 1.0f, gHead = 1.0f, gMatch = 1.0f;
            float  fM = 0.0f, fS = 0.0f, gEnd = 1.0f, boostK = 0.0f;
        } atk;
        float atkFollow = 0.005f;
        AttackMemory* atkMem = nullptr;
        ToneLayer tone;
        bool toneNoTable = false;

        Archetype arch = Archetype::StruckString;
        ArtFx fx;
        Art   art = Art::Normal;
        bool  active = false, keyDown = false, letGoDone = false, modelReleased = false;
        bool  stealing = false, hasPending = false;
        int   stealLeft = 0;
        static constexpr int stealLen = 96;
        NoteStart pending;
        long  pendingSnapAge = 0;
        int   note = 60, nUnits = 1;
        long  age = 0;
        float vel = 0.8f, baseFreq = 440.0f;
        float unitCents[kMaxUnison] = {}, unitPan[kMaxUnison] = {};
        float glideOffset = 0.0f, glideRate = 1.0e9f;
        bool  agActive = false;
        float agDepth = 0.0f, agTime = 0.06f, agK = 6.0f;
        float drive = 0.0f, driveRise = 0.001f, driveFall = 0.001f, tongue = 0.0f;
        float shapeAmt = 0.0f, shFast = 0.0f, shSlow = 0.0f;
        float shCoefFast = 0.1f, shCoefFastRel = 0.01f, shCoefSlow = 0.01f, shCoefSlowRel = 0.001f;
        float humanCents = 0.0f, varBright = 0.0f, varLevel = 1.0f;
        double elapsed = 0.0, keyUpTime = -1.0, letGoTime = 0.0, rollTimer = 0.0, amPh = 0.0, flPh = 0.0;
        std::uint32_t rng = 1;
    };
}
