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
            for (auto& b : body) b.reset();
            for (auto& c : chr) c.reset();
            for (auto& f : flt) f.reset();
            active = false;
        }

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

        void kill() { active = false; hasPending = false; stealing = false; }

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

            // drive (breath / bow / bellows) envelope for sustained archetypes
            const float tr = dsp::clampv (s[P::exc_transient] / 100.0f + fx.transientAdd, 0.0f, 1.0f);
            float attS = s.arch == Archetype::BowedString ? dsp::lerp (0.005f, 0.3f, s[P::bw_attack] / 100.0f)
                                                         : dsp::lerp (0.12f, 0.008f, tr);
            attS *= fx.attackMul;
            driveRise = (float) (1.0 / std::max (1.0, attS * sr));
            if (legato && wasActive) driveRise *= 3.0f;
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

            float vibCents = s[P::vib_depth] * 0.6f;                                    // 0..60 cents
            const int mwT = (int) s[P::mw_target], atT = (int) s[P::at_target];
            if (mwT == 0) vibCents += live.mw * 60.0f;
            if (atT == 0) vibCents += live.at * 60.0f;
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

            for (int i = 0; i < n; ++i)
            {
                float l = uL[i], r = uR[i];
                if (useBody) { l = body[0].process (l); r = body[1].process (r); }
                l = chr[0].process (l); r = chr[1].process (r);
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
        float drive = 0.0f, driveRise = 0.001f, driveFall = 0.001f;
        float humanCents = 0.0f, varBright = 0.0f, varLevel = 1.0f;
        double elapsed = 0.0, keyUpTime = -1.0, letGoTime = 0.0, rollTimer = 0.0, amPh = 0.0, flPh = 0.0;
        std::uint32_t rng = 1;
    };
}
