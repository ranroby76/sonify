// D:\workspace\Fanan_web_hub\src\TestSynth.cpp
//
// Cayman (the test synth) as a plugin: the tone a track plays before it has an instrument of its own
// (Track.cpp's: a sine with two harmonics that fade, quick to judge latency by), grown into a
// small synth with a panel (panel_testsynth.json).
//
//   Two oscillators (Classic, Sine, Triangle, Saw, Square, Pulse), the second tuned and detuned
//   against the first; a sub an octave down and noise.
//   A filter (low, high or band pass; off by default) with its own envelope, key tracking and the
//   LFO's sweep.
//   The amp envelope, velocity's say in the level.
//   An LFO (vibrato, which the mod wheel adds to; the filter; tremolo).
//   Poly, Mono or Legato, with glide; the pitch bend's range; drive; Grex's BBD chorus.
//
// Its defaults are the old tone exactly (Classic, the envelope 1 ms / 350 ms / 45 % / 180 ms,
// level 0.14 at full velocity), so loading it on a track changes nothing until it's turned.

#include "BuiltinEffects.h"
#include "BuiltinPlugins.h"
#include "GrexFx.h"

#include <array>
#include <cmath>

namespace
{
    namespace SynthParam
    {
        enum : uint32_t { osc1Wave = 1, osc1Octave, osc1Level, osc2Wave, osc2Octave, osc2Semi, osc2Detune, osc2Level,
                          subLevel, noiseLevel, pulseWidth,
                          filterType, cutoff, resonance, filterEnv, keyTrack, fAttack, fDecay, fSustain, fRelease,
                          aAttack, aDecay, aSustain, aRelease, velocity,
                          lfoWave, lfoRate, vibrato, lfoFilter, tremolo,
                          glide, mode, bendRange, drive, chorus, output };
    }

    enum Wave { classic, sine, triangle, saw, square, pulse };
    enum FilterType { filterOff, lowPass, highPass, bandPass };
    enum LfoWave { lfoSine, lfoTriangle, lfoSquare, lfoRandom };
    enum Mode { poly, mono, legato };

    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    constexpr int maxVoices = 16;
    constexpr int controlStep = 16;          // samples between filter and pitch updates

    /** PolyBLEP: the step a saw or square takes, smoothed over a sample so it doesn't alias. */
    inline double polyBlep (double t, double dt) noexcept
    {
        if (t < dt)         { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt)   { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    /** The settings a block plays with (read from the parameters once per block). */
    struct Settings
    {
        int wave1 = classic, wave2 = saw, filter = filterOff, lfoShape = lfoSine, mode = poly;
        double octave1 = 0.0, pitch2 = 0.0;   // semitones
        float level1 = 1.0f, level2 = 0.0f, sub = 0.0f, noise = 0.0f, width = 0.5f;
        double cutoff = 8000.0, resonance = 0.2, envOctaves = 0.0, keyTrack = 0.0;
        juce::ADSR::Parameters filterEnv { 0.005f, 0.4f, 0.0f, 0.3f }, ampEnv { 0.001f, 0.35f, 0.45f, 0.18f };
        float velocity = 1.0f;
        double lfoHz = 5.0, vibratoCents = 0.0, lfoOctaves = 0.0;
        float tremolo = 0.0f;
        double glideSeconds = 0.0, bendRange = 2.0;
    };

    //==============================================================================
    class Voice
    {
    public:
        void prepare (double rate) noexcept
        {
            sampleRate = rate;
            amp.setSampleRate (rate);
            fenv.setSampleRate (rate);
        }

        bool isActive() const noexcept     { return amp.isActive(); }
        int getNote() const noexcept       { return note; }
        bool isHeld() const noexcept       { return held; }
        juce::uint64 getAge() const noexcept { return age; }

        void start (int newNote, float velocity, double fromNote, bool retrigger, const Settings& s, juce::uint64 order) noexcept
        {
            note = newNote;
            held = true;
            age = order;
            targetNote = newNote;
            currentNote = fromNote >= 0.0 && s.glideSeconds > 0.0 ? fromNote : static_cast<double> (newNote);
            gain = 0.14f * (1.0f - s.velocity + s.velocity * velocity);

            if (retrigger || ! amp.isActive())
            {
                brightness = 1.0;
                brightnessFall = std::exp (-1.0 / (0.25 * sampleRate));
                phase1 = phase2 = phaseSub = 0.0;
                ic1 = ic2 = 0.0;
                amp.setParameters (s.ampEnv);
                fenv.setParameters (s.filterEnv);
                amp.noteOn();
                fenv.noteOn();
            }
        }

        /** Mono and Legato: another note under the same finger, without starting over. */
        void glideTo (int newNote, juce::uint64 order) noexcept
        {
            note = newNote;
            targetNote = newNote;
            held = true;
            age = order;
        }

        void release() noexcept
        {
            held = false;
            amp.noteOff();
            fenv.noteOff();
        }

        void kill() noexcept
        {
            held = false;
            amp.reset();
            fenv.reset();
        }

        double getCurrentNote() const noexcept   { return currentNote; }

        void render (float* out, int numSamples, const Settings& s, double bendSemitones, double lfoValue, double vibratoCents,
                     juce::Random& random) noexcept
        {
            for (int start = 0; start < numSamples; start += controlStep)
            {
                const auto count = juce::jmin (controlStep, numSamples - start);

                // Glide: the pitch slides in semitones, at the speed asked.
                if (s.glideSeconds > 0.0 && std::abs (currentNote - targetNote) > 1.0e-4)
                {
                    const auto step = std::exp (-count / (s.glideSeconds * sampleRate));
                    currentNote = targetNote + (currentNote - targetNote) * step;
                }
                else
                {
                    currentNote = targetNote;
                }

                const auto basePitch = currentNote + bendSemitones + vibratoCents / 100.0;
                const auto hz1 = 440.0 * std::pow (2.0, (basePitch + s.octave1 - 69.0) / 12.0);
                const auto hz2 = 440.0 * std::pow (2.0, (basePitch + s.pitch2 - 69.0) / 12.0);
                const auto dt1 = juce::jlimit (0.0, 0.45, hz1 / sampleRate);
                const auto dt2 = juce::jlimit (0.0, 0.45, hz2 / sampleRate);
                const auto dtSub = dt1 * 0.5;

                // The filter's cutoff where it is now: its envelope, the key, the LFO.
                if (s.filter != filterOff)
                {
                    const auto envNow = static_cast<double> (fenvLevel);
                    const auto octaves = s.envOctaves * envNow + s.keyTrack * (currentNote - 60.0) / 12.0 + s.lfoOctaves * lfoValue;
                    const auto fc = juce::jlimit (20.0, sampleRate * 0.45, s.cutoff * std::pow (2.0, octaves));
                    g = std::tan (juce::MathConstants<double>::pi * fc / sampleRate);
                    k = 2.0 - 1.95 * s.resonance;
                    a1 = 1.0 / (1.0 + g * (g + k));
                    a2 = g * a1;
                    a3 = g * a2;
                }

                for (int i = start; i < start + count; ++i)
                {
                    double sample = 0.0;

                    if (s.level1 > 0.0f)   sample += s.level1 * oscillator (s.wave1, phase1, dt1, s.width);
                    if (s.level2 > 0.0f)   sample += s.level2 * oscillator (s.wave2 == classic ? sine : s.wave2, phase2, dt2, s.width);
                    if (s.sub > 0.0f)      sample += s.sub * std::sin (twoPi * phaseSub);
                    if (s.noise > 0.0f)    sample += s.noise * (random.nextDouble() * 2.0 - 1.0);

                    advance (phase1, dt1);
                    advance (phase2, dt2);
                    advance (phaseSub, dtSub);
                    brightness *= brightnessFall;

                    // The state-variable filter (Zavalishin's), when it's on.
                    if (s.filter != filterOff)
                    {
                        const auto v3 = sample - ic2;
                        const auto v1 = a1 * ic1 + a2 * v3;
                        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
                        ic1 = 2.0 * v1 - ic1;
                        ic2 = 2.0 * v2 - ic2;
                        sample = s.filter == lowPass ? v2 : s.filter == bandPass ? v1 * k : sample - k * v1 - v2;
                    }

                    fenvLevel = fenv.getNextSample();
                    out[i] += static_cast<float> (sample) * gain * amp.getNextSample();
                }

                if (! amp.isActive())
                    return;
            }
        }

    private:
        double oscillator (int wave, double phase, double dt, float width) noexcept
        {
            switch (wave)
            {
                case classic:
                {
                    const auto p = twoPi * phase;
                    return std::sin (p) + brightness * (0.5 * std::sin (2.0 * p) + 0.25 * std::sin (3.0 * p));
                }
                case triangle:   return 1.0 - 4.0 * std::abs (phase - 0.5);
                case saw:        return 2.0 * phase - 1.0 - polyBlep (phase, dt);
                case square:
                case pulse:
                {
                    const auto w = wave == square ? 0.5 : static_cast<double> (width);
                    auto value = phase < w ? 1.0 : -1.0;
                    value += polyBlep (phase, dt);
                    value -= polyBlep (std::fmod (phase + 1.0 - w, 1.0), dt);
                    return value * 0.8;
                }
                case sine:
                default:         return std::sin (twoPi * phase);
            }
        }

        static void advance (double& phase, double dt) noexcept
        {
            phase += dt;

            if (phase >= 1.0)
                phase -= 1.0;
        }

        double sampleRate = 48000.0;
        juce::ADSR amp, fenv;
        float fenvLevel = 0.0f;
        int note = -1, targetNote = 60;
        bool held = false;
        juce::uint64 age = 0;
        double currentNote = 60.0;
        double phase1 = 0.0, phase2 = 0.0, phaseSub = 0.0;
        double brightness = 1.0, brightnessFall = 1.0;
        float gain = 0.0f;
        double g = 0.0, k = 2.0, a1 = 0.0, a2 = 0.0, a3 = 0.0, ic1 = 0.0, ic2 = 0.0;
    };

    //==============================================================================
    class TestSynth final : public BuiltinPlugin
    {
    public:
        TestSynth()
            : BuiltinPlugin (TestSynths::describe(),
                             {
                                 { SynthParam::osc1Wave,   "Osc 1 Wave",    0.0,     5.0,     0.0, "",   0, true, false, true, nullptr, { "Classic", "Sine", "Triangle", "Saw", "Square", "Pulse" } },
                                 { SynthParam::osc1Octave, "Osc 1 Octave", -2.0,     2.0,     0.0, "",   0, true },
                                 { SynthParam::osc1Level,  "Osc 1 Level",   0.0,   100.0,   100.0, "%",  0 },
                                 { SynthParam::osc2Wave,   "Osc 2 Wave",    0.0,     5.0,     3.0, "",   0, true, false, true, nullptr, { "Classic", "Sine", "Triangle", "Saw", "Square", "Pulse" } },
                                 { SynthParam::osc2Octave, "Osc 2 Octave", -2.0,     2.0,     0.0, "",   0, true },
                                 { SynthParam::osc2Semi,   "Osc 2 Semi",  -12.0,    12.0,     0.0, "st", 0, true },
                                 { SynthParam::osc2Detune, "Osc 2 Detune",  0.0,    50.0,     7.0, "ct", 0 },
                                 { SynthParam::osc2Level,  "Osc 2 Level",   0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::subLevel,   "Sub",           0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::noiseLevel, "Noise",         0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::pulseWidth, "Pulse Width",   5.0,    95.0,    50.0, "%",  0 },
                                 { SynthParam::filterType, "Filter",        0.0,     3.0,     0.0, "",   0, true, false, true, nullptr, { "Off", "Low Pass", "High Pass", "Band Pass" } },
                                 { SynthParam::cutoff,     "Cutoff",       20.0, 20000.0,  8000.0, "Hz", 0 },
                                 { SynthParam::resonance,  "Resonance",     0.0,   100.0,    20.0, "%",  0 },
                                 { SynthParam::filterEnv,  "Env Amount", -100.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::keyTrack,   "Key Track",     0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::fAttack,    "F Attack",      1.0,  5000.0,     5.0, "ms", 0 },
                                 { SynthParam::fDecay,     "F Decay",       1.0,  5000.0,   400.0, "ms", 0 },
                                 { SynthParam::fSustain,   "F Sustain",     0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::fRelease,   "F Release",     1.0,  5000.0,   300.0, "ms", 0 },
                                 { SynthParam::aAttack,    "Attack",        1.0,  5000.0,     1.0, "ms", 0 },
                                 { SynthParam::aDecay,     "Decay",         1.0,  5000.0,   350.0, "ms", 0 },
                                 { SynthParam::aSustain,   "Sustain",       0.0,   100.0,    45.0, "%",  0 },
                                 { SynthParam::aRelease,   "Release",       1.0,  5000.0,   180.0, "ms", 0 },
                                 { SynthParam::velocity,   "Velocity",      0.0,   100.0,   100.0, "%",  0 },
                                 { SynthParam::lfoWave,    "LFO Wave",      0.0,     3.0,     0.0, "",   0, true, false, true, nullptr, { "Sine", "Triangle", "Square", "Random" } },
                                 { SynthParam::lfoRate,    "LFO Rate",      0.05,   20.0,     5.0, "Hz", 2 },
                                 { SynthParam::vibrato,    "Vibrato",       0.0,   100.0,     0.0, "ct", 0 },
                                 { SynthParam::lfoFilter,  "LFO Filter",    0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::tremolo,    "Tremolo",       0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::glide,      "Glide",         0.0,  1000.0,     0.0, "ms", 0 },
                                 { SynthParam::mode,       "Mode",          0.0,     2.0,     0.0, "",   0, true, false, true, nullptr, { "Poly", "Mono", "Legato" } },
                                 { SynthParam::bendRange,  "Bend Range",    1.0,    12.0,     2.0, "st", 0, true },
                                 { SynthParam::drive,      "Drive",         0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::chorus,     "Chorus",        0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::output,     "Output",      -24.0,     6.0,     0.0, "dB", 1 }
                             })
        {
        }

        Role getRole() const noexcept override                { return Role::instrument; }
        int getMainInputChannels() const noexcept override    { return 0; }
        const char* getPanelName() const noexcept override    { return "testsynth"; }
        juce::StringArray getCurveSources() const override    { return { "synth.wave" }; }

        void panelNote (int note, float velocity) override
        {
            if (velocity > 0.0f) keyboard.noteOn (1, note, velocity);
            else                 keyboard.noteOff (1, note, 0.0f);
        }

        bool isPanelNoteDown (int note) const override        { return keyboard.isNoteOnForChannels (0xffff, note); }

        void readCurve (const juce::String& source, float* destination, int numSamples) override
        {
            if (source != "synth.wave")
                return BuiltinPlugin::readCurve (source, destination, numSamples);

            // What it plays: the last sound out, as the scope shows it.
            const auto count = juce::jmin (numSamples, static_cast<int> (scope.size()));
            const auto end = scopeAt.load (std::memory_order_acquire);

            for (int i = 0; i < count; ++i)
                destination[i] = scope[static_cast<size_t> ((end + i) % static_cast<int> (scope.size()))] * 4.0f;
        }

        void process (const float*, const float*, float* outLeft, float* outRight, int numSamples,
                      const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept override
        {
            if (outLeft == nullptr || numSamples <= 0)
                return;

            juce::FloatVectorOperations::clear (outLeft, numSamples);

            if (outRight != nullptr && outRight != outLeft)
                juce::FloatVectorOperations::clear (outRight, numSamples);

            if (! isActiveNow())
                return;

            setTransport (transport);

            if (takeDirty())
                update();

            // The panel's keyboard joins the notes that came.
            incoming.clear();
            incoming.addEvents (midi, 0, numSamples, 0);
            keyboard.processNextMidiBuffer (incoming, 0, numSamples, true);

            // Played between the events, each where it falls.
            int at = 0;

            for (const auto metadata : incoming)
            {
                const auto position = juce::jlimit (0, numSamples, metadata.samplePosition);
                renderSpan (outLeft, at, position - at);
                at = position;
                handle (metadata.getMessage());
            }

            renderSpan (outLeft, at, numSamples - at);
            finish (outLeft, outRight, numSamples);
            measureOutput (outLeft, outRight != nullptr ? outRight : outLeft, numSamples);
        }

    private:
        void processAudio (float*, float*, int) noexcept override {}   // process() plays it

        void prepareAudio (double rate, int maxBlockSize) override
        {
            for (auto& voice : voices)
            {
                voice.prepare (rate);
                voice.kill();
            }

            chorusFx.prepare (rate);
            incoming.ensureSize (4096);
            juce::ignoreUnused (maxBlockSize);
            scope.assign (4096, 0.0f);
            lfoPhase = 0.0;
            sustainDown = false;
        }

        void update() noexcept override
        {
            using namespace SynthParam;
            const auto ms = [this] (uint32_t id) { return static_cast<float> (valueOf (id) / 1000.0); };

            s.wave1 = static_cast<int> (valueOf (osc1Wave));
            s.wave2 = static_cast<int> (valueOf (osc2Wave));
            s.octave1 = 12.0 * valueOf (osc1Octave);
            s.pitch2 = 12.0 * valueOf (osc2Octave) + valueOf (osc2Semi) + valueOf (osc2Detune) / 100.0;
            s.level1 = static_cast<float> (valueOf (osc1Level) / 100.0);
            s.level2 = static_cast<float> (valueOf (osc2Level) / 100.0);
            s.sub = static_cast<float> (valueOf (subLevel) / 100.0);
            s.noise = static_cast<float> (valueOf (noiseLevel) / 100.0) * 0.5f;
            s.width = static_cast<float> (valueOf (pulseWidth) / 100.0);
            s.filter = static_cast<int> (valueOf (filterType));
            s.cutoff = valueOf (cutoff);
            s.resonance = valueOf (resonance) / 100.0;
            s.envOctaves = 5.0 * valueOf (filterEnv) / 100.0;
            s.keyTrack = valueOf (keyTrack) / 100.0;
            s.filterEnv = { ms (fAttack), ms (fDecay), static_cast<float> (valueOf (fSustain) / 100.0), ms (fRelease) };
            s.ampEnv = { ms (aAttack), ms (aDecay), static_cast<float> (valueOf (aSustain) / 100.0), ms (aRelease) };
            s.velocity = static_cast<float> (valueOf (velocity) / 100.0);
            s.lfoShape = static_cast<int> (valueOf (lfoWave));
            s.lfoHz = valueOf (lfoRate);
            s.vibratoCents = valueOf (vibrato);
            s.lfoOctaves = 2.0 * valueOf (lfoFilter) / 100.0;
            s.tremolo = static_cast<float> (valueOf (tremolo) / 100.0);
            s.glideSeconds = valueOf (glide) / 1000.0;
            s.mode = static_cast<int> (valueOf (mode));
            s.bendRange = valueOf (bendRange);
            driveAmount = static_cast<float> (valueOf (drive) / 100.0);
            chorusMix = static_cast<float> (valueOf (chorus) / 100.0);
            outputGain = static_cast<float> (juce::Decibels::decibelsToGain (valueOf (output)));
        }

        //==============================================================================
        void handle (const juce::MidiMessage& message) noexcept
        {
            if (message.isNoteOn())
            {
                noteOn (message.getNoteNumber(), message.getFloatVelocity());
            }
            else if (message.isNoteOff())
            {
                noteOff (message.getNoteNumber());
            }
            else if (message.isPitchWheel())
            {
                bend = (message.getPitchWheelValue() - 8192) / 8192.0;
            }
            else if (message.isController())
            {
                const auto number = message.getControllerNumber();

                if (number == 1)
                {
                    modWheel = message.getControllerValue() / 127.0;
                }
                else if (number == 64)
                {
                    sustainDown = message.getControllerValue() >= 64;

                    // The pedal up: the notes it held (their keys let go) end.
                    if (! sustainDown)
                        for (auto& voice : voices)
                            if (voice.isActive() && voice.isHeld() && ! keyDown[static_cast<size_t> (juce::jlimit (0, 127, voice.getNote()))])
                                voice.release();
                }
                else if (number == 120 || number == 123)
                {
                    for (auto& voice : voices)
                    {
                        if (number == 120) voice.kill();
                        else               voice.release();
                    }

                    keyDown.fill (false);
                }
            }
            else if (message.isAllNotesOff() || message.isAllSoundOff())
            {
                for (auto& voice : voices)
                    voice.kill();

                keyDown.fill (false);
            }
        }

        void noteOn (int note, float velocity) noexcept
        {
            keyDown[static_cast<size_t> (note)] = true;
            ++order;

            if (s.mode != poly)
            {
                // One voice: a new note glides from where it is; Legato doesn't start the envelopes over.
                auto& voice = voices[0];
                const bool sounding = voice.isActive() && voice.isHeld();

                if (sounding && s.mode == legato)
                    voice.glideTo (note, order);
                else
                    voice.start (note, velocity, voice.isActive() ? voice.getCurrentNote() : lastNote, true, s, order);

                lastNote = note;
                return;
            }

            // Poly: a free voice, else the oldest one.
            Voice* chosen = nullptr;

            for (auto& voice : voices)
                if (! voice.isActive() && chosen == nullptr)
                    chosen = &voice;

            if (chosen == nullptr)
            {
                chosen = &voices[0];

                for (auto& voice : voices)
                    if (voice.getAge() < chosen->getAge())
                        chosen = &voice;
            }

            chosen->start (note, velocity, s.glideSeconds > 0.0 ? lastNote : -1.0, true, s, order);
            lastNote = note;
        }

        void noteOff (int note) noexcept
        {
            keyDown[static_cast<size_t> (note)] = false;

            if (s.mode != poly)
            {
                auto& voice = voices[0];

                if (voice.getNote() != note)
                    return;

                // Back to a key still held, as a mono synth does.
                for (int other = 127; other >= 0; --other)
                {
                    if (keyDown[static_cast<size_t> (other)])
                    {
                        voice.glideTo (other, ++order);
                        return;
                    }
                }
            }

            if (sustainDown)
                return;

            for (auto& voice : voices)
                if (voice.isActive() && voice.isHeld() && voice.getNote() == note)
                    voice.release();
        }

        //==============================================================================
        void renderSpan (float* out, int start, int count) noexcept
        {
            if (count <= 0)
                return;

            // In short pieces, so the LFO moves smoothly whatever the block.
            constexpr int piece = 32;

            for (int at = start; at < start + count; at += piece)
            {
                const auto length = juce::jmin (piece, start + count - at);
                const auto lfoValue = lfoAt (lfoPhase);
                lfoPhase += s.lfoHz * length / getSampleRate();

                if (lfoPhase >= 1.0)
                {
                    lfoPhase -= std::floor (lfoPhase);
                    randomValue = random.nextDouble() * 2.0 - 1.0;
                }

                const auto vibrato = (s.vibratoCents + 50.0 * modWheel) * lfoValue;

                for (auto& voice : voices)
                    if (voice.isActive())
                        voice.render (out + at, length, s, bend * s.bendRange, lfoValue, vibrato, random);

                if (s.tremolo > 0.0f)
                    juce::FloatVectorOperations::multiply (out + at, 1.0f - s.tremolo * 0.5f * static_cast<float> (1.0 - lfoValue), length);
            }
        }

        double lfoAt (double phase) const noexcept
        {
            switch (s.lfoShape)
            {
                case lfoTriangle:   return 1.0 - 4.0 * std::abs (phase - 0.5);
                case lfoSquare:     return phase < 0.5 ? 1.0 : -1.0;
                case lfoRandom:     return randomValue;
                case lfoSine:
                default:            return std::sin (twoPi * phase);
            }
        }

        void finish (float* left, float* outRight, int numSamples) noexcept
        {
            // Drive: warmth, the level kept about where it was.
            if (driveAmount > 0.0f)
            {
                const auto pre = 1.0f + 9.0f * driveAmount;
                const auto makeUp = 1.0f / std::tanh (pre * 0.3f) * 0.3f;

                for (int i = 0; i < numSamples; ++i)
                    left[i] = std::tanh (left[i] * pre) * makeUp;
            }

            if (std::abs (outputGain - 1.0f) > 1.0e-6f)
                juce::FloatVectorOperations::multiply (left, outputGain, numSamples);

            // The scope's copy.
            auto writeAt = scopeAt.load (std::memory_order_relaxed);

            for (int i = 0; i < numSamples; ++i)
            {
                scope[static_cast<size_t> (writeAt)] = left[i];
                writeAt = (writeAt + 1) % static_cast<int> (scope.size());
            }

            scopeAt.store (writeAt, std::memory_order_release);

            // Stereo: the same on both sides, then the chorus spreads it.
            if (outRight == nullptr || outRight == left)
                return;

            juce::FloatVectorOperations::copy (outRight, left, numSamples);

            if (chorusMix > 0.0f)
                chorusFx.process (left, outRight, numSamples, 0.25f, 0.6f, chorusMix, getSampleRate());
        }

        std::array<Voice, maxVoices> voices;
        std::array<bool, 128> keyDown {};
        Settings s;
        juce::MidiKeyboardState keyboard;
        juce::MidiBuffer incoming;
        juce::Random random;
        GrexFx::ChorusFx chorusFx;
        std::vector<float> scope;
        std::atomic<int> scopeAt { 0 };
        juce::uint64 order = 0;
        double lastNote = -1.0, bend = 0.0, modWheel = 0.0, lfoPhase = 0.0, randomValue = 0.0;
        bool sustainDown = false;
        float driveAmount = 0.0f, chorusMix = 0.0f, outputGain = 1.0f;
    };
}

//==============================================================================
Plugin::Descriptor TestSynths::describe()
{
    Plugin::Descriptor descriptor;
    descriptor.id = Builtins::testSynthId;
    descriptor.name = "Cayman";
    descriptor.vendor = "Sonify";
    descriptor.version = "2.0";
    descriptor.features = { "instrument", "synthesizer", "stereo" };
    return descriptor;
}

std::unique_ptr<Plugin> TestSynths::create()
{
    return std::make_unique<TestSynth>();
}
