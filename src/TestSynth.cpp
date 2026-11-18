// D:\workspace\Fanan_web_hub\src\TestSynth.cpp
//
// Cayman (the test synth) as a plugin: the tone a track plays before it has an instrument of its own
// (Track.cpp's: a sine with two harmonics that fade, quick to judge latency by), grown into a
// small synth with a panel (panel_testsynth.json).
//
//   Two oscillators, the second tuned and detuned against the first; a sub an octave down and
//   noise. Their waves: Classic, Sine, Triangle, Saw, Square and Pulse (drawn as they always
//   were), then twelve wavetables borrowed from the Plugins Creator project "My Plugin"
//   (CaymanWavetables.h). Unison stacks up to seven detuned copies, spread across the stereo.
//   A filter (off by default) with its own envelope, key tracking and the LFO's sweep: low, high
//   and band pass as they always were, then fourteen more types from the same project
//   (CaymanFilters.h): 24 dB ones, notch, peak, shelves, a dirty low pass, ladders, a formant,
//   combs and a phaser.
//   The amp envelope, velocity's say in the level, and Grex's LOW VELOCITY RESPONSE (soft notes
//   lifted: the velocity to the power 2.5^-amount, so a full one stays full).
//   Two LFOs, each free or in time with the song, sixteen waves (four drawn, twelve read from the
//   wavetables). The first sweeps the filter (LFO AMT). Vibrato (which the mod wheel adds to) and
//   tremolo have a sine of their own, at the VIBRATION page's RATE.
//   The matrix sends either LFO to thirteen of the knobs, which move as it plays them.
//   The cutoff's key shot (the yellow knob on its disc): the cutoff jumps to where it points while a
//   key is down and falls back when it's let go; a note that takes over a sounding voice keeps it up.
//   The envelopes follow their knobs while a note plays, its release too. The amp envelope's
//   SHAPE (Grex's) bends its decay and release: LOG holds, then falls; LIN is a straight line (as
//   it always was); EXP drops fast, then trails off (about 7 is the first My Plugin's envelope).
//   Poly, Mono, Legato or Acid, with glide; the pitch bend's range.
//   Drive, as the first "My Plugin" has it: in each voice, before its filter (so the notes of a
//   chord don't distort one another), Warm, Hard, Fold or Fuzz. RAW leaves its aliasing in, the
//   grit that project had; with RAW off it runs at twice the rate (its later oversampler). Acid, as that project
//   plays it: a note that overlaps the one before slides to it (at the Slide time) without a new
//   envelope, and a note at velocity 115 or more is accented (louder, the filter opened further
//   and closing faster), in any mode.
//   An arpeggiator: Domina's seeded one (SeededArpCore.h, as it is in that plugin). A seed makes
//   a whole pattern; the keys held are its chord.
//   Its effects, in a row after the voices: Grex's BBD chorus, phaser and auto-wah, the vowel filter
//   and its gate, the first My Plugin's ping-pong delay (free, or a note value in time with the
//   song), Grex's reverb. Each one is off while its Mix is at 0.
//   The vowel filter is Zodiak's: eight columns, a vowel each, a marker stepping across them in time
//   with the song, MORPHing from vowel to vowel or JUMPing as it enters a column. Under the columns,
//   a 16-step gate, two steps a column.
//
// Its defaults are the old tone (Classic, the envelope 1 ms / 350 ms / 45 % / 180 ms, now Grex's shape,
// level 0.14 at full velocity), close to the tone a track played before it had an instrument. After
// the effects it's brought up to the first My Plugin's level (its oscillators at 0.5, Volume -6 dB).

#include "BuiltinEffects.h"
#include "BuiltinPlugins.h"
#include "CaymanFilters.h"
#include "CaymanWavetables.h"
#include "GrexFx.h"
#include "SeededArpCore.h"

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
                          glide, mode, bendRange, drive, chorus, output,
                          // the effects (added after the first release: new ids go at the end)
                          chorusRate, chorusDepth,
                          phaserRate, phaserDepth, phaserFeedback, phaserMix,
                          wahSens, wahFreq, wahQ, wahRate, wahDepth, wahMix,
                          vowel, vowelRate, vowelDepth, vowelRes, vowelMix,
                          delayTime, delayFeedback, delayTone, delayMix,
                          reverbSize, reverbDamp, reverbPre, reverbMix,
                          // the voice stack, Acid mode and the arpeggiator
                          unison, uniDetune, width, slide, accent,
                          arpOn, arpSeed, arpIdiom, arpOriginality, arpBars, arpOctaves, arpComplexity, arpRests,
                          arpW16, arpW8, arpW4, arpSort, arpGateMin, arpGateMax, arpRun, arpHold,
                          // the key shot, the second LFO, sync, the matrix
                          keyShot, lfo1Sync, lfo1Note, lfo1Depth, lfo2Wave, lfo2Rate, lfo2Sync, lfo2Note, lfo2Depth,
                          routes, routesEnd = routes + 2 * 13,
                          // the vowel's columns, its note value and mode, its gate
                          vowelColumns = routesEnd, vowelNote = vowelColumns + 8, vowelMode, vowelGate,
                          vowelSteps, vowelStepsEnd = vowelSteps + 16,
                          // the drive's kind, and whether it's left raw
                          driveType = vowelStepsEnd, driveRaw,
                          ampShape,
                          // the delay's sync and note value, the vibration's own rate
                          delaySync, delayNote, vibRate,
                          // Grex's low-velocity lift
                          lowVel };

        /** The matrix's destinations, in the panel's order. */
        constexpr uint32_t destinations[] { osc2Wave, osc2Detune, pulseWidth, osc2Level, subLevel, cutoff, resonance, filterEnv, keyTrack, aAttack, uniDetune, width, velocity };
        constexpr int numDestinations = 13;

        /** The route from LFO `lfo` (0 or 1) to destination `d`: on (1) or off (0). */
        constexpr uint32_t route (int lfo, int d) noexcept   { return routes + static_cast<uint32_t> (lfo * numDestinations + d); }
    }

    enum Wave { classic, sine, triangle, saw, square, pulse };
    enum FilterType { filterOff, lowPass, highPass, bandPass };
    enum LfoWave { lfoSine, lfoTriangle, lfoSquare, lfoRandom };   // then the wavetables, from Pulse 25 (the bank's fifth) on
    constexpr int firstTableLfo = 4;

    /** The delay's note values, in beats. */
    constexpr double delayBeats[] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 4.0 / 3.0, 1.5, 2.0, 3.0, 4.0 };

    /** The LFOs' note values when they follow the song, in beats. */
    constexpr double noteBeats[] { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 3.0, 1.5, 0.75, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };

    /** Grex's envelope (its SynthCommon.h AHDSREnvelope, without the hold): the attack rises towards
        1.5 and stops at full level, which it reaches in exactly the time set, quick off the mark and
        rounding into the top rather than a straight ramp; the decay and release are below. Its settings
        can change while a note sounds: juce::ADSR restarts a release from the sustain level when its settings change, so a
        note let go a moment ago would jump, or with sustain at 0 stop dead. Here a release changed
        mid-way runs its new length from where it is.

        Its decay and release are Grex's: a phase that runs 0..1 over the set time, mapped through
        one curve, so every shape takes exactly the time set and only the contour differs:

            fall (p, k) = (1 - e^-kp) / (1 - e^-k),   p when k is 0

        k = 0 is the straight line (the shape Cayman had before Grex's); positive k drops fast and trails off
        (Grex's EXP is 6, the first My Plugin's about 7); negative k holds, then falls (LOG is -4). */
    class Envelope
    {
    public:
        using Parameters = juce::ADSR::Parameters;

        void setSampleRate (double rate) noexcept   { sampleRate = rate; recalculate(); }

        /** The decay and release's curve (see fall). Changed mid-way, the fall carries on from where
            it is, along the new curve, in the time it had left: it doesn't jump to the new curve's level. */
        void setCurve (float k) noexcept
        {
            if (std::abs (k - curve) < 1.0e-6f)
                return;

            curve = k;

            if ((state == State::decay || state == State::release) && phase < 1.0)
            {
                step /= juce::jmax (1.0e-6, 1.0 - phase);
                from = level;
                phase = 0.0;
            }
        }

        void setParameters (const Parameters& p) noexcept
        {
            const bool releaseMoved = std::abs (p.release - parameters.release) > 1.0e-6f;
            parameters = p;
            recalculate();

            if (state == State::release && releaseMoved)
            {
                if (parameters.release > 0.0f) startFall (level, 0.0f, parameters.release);
                else                           reset();
            }
        }

        bool isActive() const noexcept   { return state != State::idle; }

        void noteOn() noexcept
        {
            if (attackCoef > 0.0f)  { state = State::attack; }
            else if (decays())      { level = 1.0f; startDecay(); }
            else                    { level = parameters.sustain; state = State::sustain; }
        }

        void noteOff() noexcept
        {
            if (state == State::idle)
                return;

            if (parameters.release > 0.0f)
            {
                startFall (level, 0.0f, parameters.release);
                state = State::release;
            }
            else
            {
                reset();
            }
        }

        void reset() noexcept   { level = 0.0f; state = State::idle; }

        float getNextSample() noexcept
        {
            switch (state)
            {
                case State::idle:     return 0.0f;
                case State::attack:   level += (attackTarget - level) * attackCoef; if (level >= 0.999f) { level = 1.0f; next(); } break;
                case State::decay:
                    phase += step;
                    if (phase >= 1.0) { level = parameters.sustain; state = State::sustain; }
                    else              level = from + (parameters.sustain - from) * fall (static_cast<float> (phase));
                    break;
                case State::sustain:  level = parameters.sustain; break;
                case State::release:
                    phase += step;
                    if (phase >= 1.0) reset();
                    else              level = from * (1.0f - fall (static_cast<float> (phase)));
                    break;
            }

            return level;
        }

    private:
        enum class State { idle, attack, decay, sustain, release };

        bool decays() const noexcept   { return parameters.decay > 0.0f && parameters.sustain < 1.0f; }

        float fall (float p) const noexcept
        {
            if (std::abs (curve) < 1.0e-3f)
                return p;

            return (1.0f - std::exp (-curve * p)) / (1.0f - std::exp (-curve));
        }

        void startFall (float start, float, float seconds) noexcept
        {
            from = start;
            phase = 0.0;
            step = 1.0 / juce::jmax (1.0, seconds * sampleRate);
        }

        void startDecay() noexcept
        {
            startFall (level, parameters.sustain, parameters.decay);
            state = State::decay;
        }

        void recalculate() noexcept
        {
            // Grex's attack: a rise towards 1.5 (a Moog's punch) that passes full level at exactly the
            // time set (its time constant is the attack over ln (1.5 / 0.5))
            attackCoef = parameters.attack > 0.0f
                           ? 1.0f - std::exp (-1.0f / (parameters.attack / 1.0986f * static_cast<float> (sampleRate)))
                           : -1.0f;

            if (state == State::attack && attackCoef <= 0.0f)
                next();

            // a decay under way keeps its place and takes the new time; it ends if there's no
            // longer anywhere to fall to
            if (state == State::decay)
            {
                if (! decays() || level <= parameters.sustain) state = State::sustain;
                else step = 1.0 / juce::jmax (1.0, parameters.decay * sampleRate);
            }
        }

        void next() noexcept
        {
            if (state == State::attack)
            {
                if (decays()) startDecay();
                else          state = State::sustain;
            }
        }

        Parameters parameters;
        State state = State::idle;
        double sampleRate = 48000.0, phase = 0.0, step = 0.0;
        float level = 0.0f, attackCoef = 0.0f, from = 0.0f, curve = 6.0f;     // Grex's EXP until told otherwise
        static constexpr float attackTarget = 1.5f;
    };
    enum Mode { poly, mono, legato, acid };

    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    constexpr int maxVoices = 16, maxUnison = 7;
    constexpr int firstTableWave = 6;                    // the waves from here on are read from the wavetables
    constexpr float accentVelocity = 115.0f / 127.0f;    // a note this hard or harder is accented

    /** The wavetables, built the first time a Cayman is prepared (never on the audio thread). */
    const caymanwt::Bank& wavetables()
    {
        static const caymanwt::Bank bank = []
        {
            caymanwt::Bank built;
            built.build();
            return built;
        }();

        return bank;
    }

    /** The idioms' names, as the arpeggiator has them. */
    std::vector<const char*> idiomNames()
    {
        std::vector<const char*> names;

        for (int i = 0; i < fanan::idiomCount(); ++i)
            names.push_back (fanan::idiomName (i));

        return names;
    }
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
        float lowVelGamma = 1.0f;      // Grex's low-velocity lift: velocity^this (1: as played)
        double lfoHz = 5.0, vibratoCents = 0.0, lfoOctaves = 0.0;
        float tremolo = 0.0f;
        double glideSeconds = 0.0, bendRange = 2.0;

        // the borrowed filter's type when `filter` is one of its, else -1 (the filter Cayman always had)
        int extraFilter = -1;

        // the unison stack: each copy's pitch ratio and where it sits in the stereo
        int unison = 1;
        bool wide = false;
        float uniNorm = 1.0f, maxRatio = 1.0f;
        std::array<float, maxUnison> ratio { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        std::array<float, maxUnison> panLeft { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f }, panRight { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

        double slideSeconds = 0.045;
        float accent = 0.0f;

        double keyShotOctaves = 0.0;   // the cutoff's jump while a key is down
        float ampCurve = 6.0f;         // the amp envelope's SHAPE (Grex's EXP; 0 is a straight line)

        // the drive, as the first My Plugin works it out: how hard, made up, how much of it, trimmed
        int driveKind = 0;
        bool driveRaw = true;
        float driveGain = 1.0f, driveComp = 1.0f, driveWet = 0.0f, drivePost = 1.0f;
    };

    //==============================================================================
    /** The vowel filter, borrowed from Zodiak's (zodiak/ZodiakFx.h): three formants, each two
        band-passes in a row so it stands apart, morphing along A E O U I. Zodiak steps through a
        pattern at the song's tempo; here a knob picks the vowel and the filter's own LFO sweeps
        round it. A vowel is held, then moves quickly to the next, as speech does. */
    class VowelFilter
    {
    public:
        void prepare (double rate) noexcept   { sampleRate = rate; reset(); }

        void reset() noexcept
        {
            for (auto& channel : bands)
                for (auto& band : channel)
                    band = {};

            mixNow = 0.0f;
            countdown = 0;
        }

        /** The marker runs from `from` (in columns, 0..8 a lap) by `perSample` a sample over the
            eight `columns` (each a vowel, 0..4: A E O U I). MORPH glides from a column's vowel to the
            next one's across the whole column; JUMP changes as the marker enters a column (in 12 ms,
            so it doesn't click). */
        void process (float* left, float* right, int numSamples, double from, double perSample, const std::array<int, 8>& columns,
                      bool jump, float res, float mix) noexcept
        {
            const auto smooth = 1.0f - std::exp (-1.0f / (0.02f * static_cast<float> (sampleRate)));

            for (int i = 0; i < numSamples; ++i)
            {
                if (--countdown <= 0)
                {
                    countdown = updateEvery;
                    const auto at = from + i * perSample;
                    const auto lap = at - 8.0 * std::floor (at / 8.0);
                    const auto column = juce::jlimit (0, 7, static_cast<int> (std::floor (lap)));
                    const auto into = static_cast<float> (lap - column);

                    if (jump)
                    {
                        const auto seconds = into / juce::jmax (1.0e-9, perSample * sampleRate);
                        setFormants (columns[static_cast<size_t> ((column + 7) % 8)], columns[static_cast<size_t> (column)],
                                     juce::jmin (1.0f, static_cast<float> (seconds / 0.012)), res);
                    }
                    else
                    {
                        setFormants (columns[static_cast<size_t> (column)], columns[static_cast<size_t> ((column + 1) % 8)], into, res);
                    }
                }

                mixNow += (mix - mixNow) * smooth;
                float wetLeft = 0.0f, wetRight = 0.0f;

                for (int f = 0; f < formants; ++f)
                {
                    wetLeft += gains[f] * bands[0][static_cast<size_t> (formants + f)].tick (bands[0][static_cast<size_t> (f)].tick (left[i]));
                    wetRight += gains[f] * bands[1][static_cast<size_t> (formants + f)].tick (bands[1][static_cast<size_t> (f)].tick (right[i]));
                }

                left[i] += (softClip (wetLeft * makeUp) - left[i]) * mixNow;
                right[i] += (softClip (wetRight * makeUp) - right[i]) * mixNow;
            }
        }

    private:
        static constexpr int formants = 3, updateEvery = 16;
        static constexpr float makeUp = 3.4f;      // about the dry level on a full sound

        /** A zero-delay state-variable band-pass, unity at its centre. */
        struct BandPass
        {
            float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f, ic1 = 0.0f, ic2 = 0.0f;

            void set (float hz, float q, float rate) noexcept
            {
                const auto g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, rate * 0.45f, hz) / rate);
                k = 1.0f / juce::jmax (0.3f, q);
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }

            float tick (float x) noexcept
            {
                const auto v3 = x - ic2;
                const auto v1 = a1 * ic1 + a2 * v3;
                const auto v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                return k * v1;
            }
        };

        static float softClip (float x) noexcept
        {
            x = juce::jlimit (-3.0f, 3.0f, x);
            return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
        }

        /** The formants `t` of the way from vowel `from` to vowel `to`. */
        void setFormants (int from, int to, float t, float res) noexcept
        {
            // Each vowel's formants: frequency (Hz), level (dB), bandwidth (Hz). A talking synth's:
            // the spoken formants with the upper two strong, which is what tells the vowels apart.
            struct Formant { float hz, db, bw; };
            static constexpr Formant table[5][formants] =
            {
                { { 730.0f, 0.0f, 90.0f }, { 1090.0f, -2.0f, 100.0f }, { 2440.0f,  -8.0f, 130.0f } },   // A
                { { 530.0f, 0.0f, 70.0f }, { 1840.0f, -3.0f, 110.0f }, { 2480.0f,  -7.0f, 130.0f } },   // E
                { { 570.0f, 0.0f, 70.0f }, {  840.0f, -3.0f,  90.0f }, { 2410.0f, -11.0f, 120.0f } },   // O
                { { 300.0f, 0.0f, 60.0f }, {  870.0f, -5.0f,  80.0f }, { 2240.0f, -13.0f, 140.0f } },   // U
                { { 270.0f, 0.0f, 60.0f }, { 2290.0f, -2.0f, 110.0f }, { 3010.0f,  -5.0f, 130.0f } }    // I
            };

            from = juce::jlimit (0, 4, from);
            to = juce::jlimit (0, 4, to);
            t = juce::jlimit (0.0f, 1.0f, t);
            const auto widen = 2.2f - 1.8f * juce::jlimit (0.0f, 1.0f, res);       // wide .. narrow (screaming)
            float firstHz = 500.0f;

            for (int f = 0; f < formants; ++f)
            {
                const auto& a = table[from][f];
                const auto& b = table[to][f];
                const auto hz = a.hz * std::pow (b.hz / a.hz, t);
                const auto bw = (a.bw + (b.bw - a.bw) * t) * widen;
                const auto db = a.db + (b.db - a.db) * t;

                if (f == 0)
                    firstHz = hz;

                // A synth's harmonics fall away upward: the upper formants get half of that back.
                gains[f] = std::pow (10.0f, db / 20.0f) * std::sqrt (hz / firstHz);

                for (auto& channel : bands)
                {
                    channel[static_cast<size_t> (f)].set (hz, 0.7f * hz / bw, static_cast<float> (sampleRate));
                    channel[static_cast<size_t> (formants + f)].set (hz, 0.7f * hz / bw, static_cast<float> (sampleRate));
                }
            }
        }

        double sampleRate = 48000.0;
        std::array<std::array<BandPass, 2 * formants>, 2> bands;   // per channel: each formant's first pass, then its second
        float gains[formants] { 1.0f, 1.0f, 1.0f };
        float mixNow = 0.0f;
        int countdown = 0;
    };

    //==============================================================================
    /** The first My Plugin's ping-pong delay: what comes in goes into the left line, each side's
        echo (darkened by a one-pole, rounded by a tanh) feeds the other's, and the echoes are added
        to the sound (it isn't turned down). A new time glides there (150 ms), as a tape's would. */
    class PingPongDelay
    {
    public:
        void prepare (double rate)
        {
            sampleRate = static_cast<float> (rate);
            for (auto& line : lines)
                line.assign (static_cast<size_t> (4.1 * rate), 0.0f);
            glide = 1.0f - std::exp (-1.0f / (0.150f * sampleRate));
            first = true;
            reset();
        }

        void reset() noexcept
        {
            for (auto& line : lines)
                std::fill (line.begin(), line.end(), 0.0f);
            pos = 0;
            tone[0] = tone[1] = 0.0f;
        }

        /** samples: the time; feedback 0..0.95; mix 0..1 (the echoes' level); toneHz: the darkening. */
        void process (float* left, float* right, int count, float samples, float feedback, float mix, float toneHz) noexcept
        {
            const auto size = static_cast<int> (lines[0].size());
            if (size < 4)
                return;

            const auto target = juce::jlimit (1.0f, static_cast<float> (size - 2), samples);
            if (first) { time = target; first = false; }
            const auto toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * toneHz / sampleRate);

            for (int i = 0; i < count; ++i)
            {
                time += (target - time) * glide;
                const auto l = tap (0, time), r = tap (1, time);
                tone[0] += (r - tone[0]) * toneCoef;
                tone[1] += (l - tone[1]) * toneCoef;
                if (std::abs (tone[0]) < 1.0e-20f) tone[0] = 0.0f;
                if (std::abs (tone[1]) < 1.0e-20f) tone[1] = 0.0f;

                const auto mono = 0.5f * (left[i] + right[i]);
                lines[0][static_cast<size_t> (pos)] = mono + std::tanh (tone[0] * feedback);
                lines[1][static_cast<size_t> (pos)] = std::tanh (tone[1] * feedback);
                if (++pos >= size) pos = 0;

                left[i] += l * mix;
                right[i] += r * mix;
            }
        }

    private:
        float tap (int side, float delay) const noexcept
        {
            const auto& line = lines[static_cast<size_t> (side)];
            const auto size = static_cast<int> (line.size());
            auto r = static_cast<float> (pos) - delay;
            if (r < 0.0f) r += static_cast<float> (size);
            const auto i0 = juce::jlimit (0, size - 1, static_cast<int> (r));
            const auto i1 = i0 + 1 >= size ? 0 : i0 + 1;
            const auto frac = r - static_cast<float> (i0);
            return line[static_cast<size_t> (i0)] + (line[static_cast<size_t> (i1)] - line[static_cast<size_t> (i0)]) * frac;
        }

        std::array<std::vector<float>, 2> lines;
        int pos = 0;
        float sampleRate = 48000.0f, glide = 0.0f, time = 1.0f, tone[2] {};
        bool first = true;
    };

    class Voice
    {
    public:
        void prepare (double rate)
        {
            sampleRate = rate;
            dcPole = 1.0 - juce::MathConstants<double>::twoPi * 8.0 / rate;
            amp.setSampleRate (rate);
            fenv.setSampleRate (rate);

            for (auto& filter : extra)
                filter.prepare (rate);
        }

        bool isActive() const noexcept     { return amp.isActive(); }
        int getNote() const noexcept       { return note; }
        bool isHeld() const noexcept       { return held; }
        juce::uint64 getAge() const noexcept { return age; }
        float getShot() const noexcept     { return shot; }

        void start (int newNote, float velocity, double fromNote, bool retrigger, const Settings& s, juce::uint64 order,
                    juce::Random& random) noexcept
        {
            note = newNote;
            held = true;
            age = order;
            targetNote = newNote;
            sliding = false;
            currentNote = fromNote >= 0.0 && s.glideSeconds > 0.0 ? fromNote : static_cast<double> (newNote);
            accented = s.accent > 0.0f && velocity >= accentVelocity;
            gain = 0.14f * (1.0f - s.velocity + s.velocity * velocity) * (accented ? 1.0f + 0.8f * s.accent : 1.0f);

            if (retrigger || ! amp.isActive())
            {
                const bool wasSilent = ! amp.isActive();
                brightness = 1.0;
                brightnessFall = std::exp (-1.0 / (0.25 * sampleRate));

                // One voice starts in phase (the tone it always had); a stack starts scattered,
                // or its copies would all move together.
                for (int u = 0; u < maxUnison; ++u)
                {
                    phase1[u] = u == 0 ? 0.0 : random.nextDouble();
                    phase2[u] = u == 0 ? 0.0 : random.nextDouble();
                }

                phaseSub = 0.0;
                ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0;

                if (wasSilent)
                    for (auto& filter : extra)
                        filter.reset();

                if (wasSilent)
                    shot = 0.0f;       // a voice taken over keeps its key shot up

                if (accented)
                    accentCharge += (1.0f - accentCharge) * 0.55f;

                setEnvelopes (s);
                amp.noteOn();
                fenv.noteOn();
            }
        }

        /** The envelopes' settings, while it sounds too (an accented note's filter closes faster,
            towards 200 ms). */
        void setEnvelopes (const Settings& s) noexcept
        {
            auto filterEnv = s.filterEnv;

            if (accented)
                filterEnv.decay += (juce::jmin (filterEnv.decay, 0.2f) - filterEnv.decay) * s.accent;

            amp.setCurve (s.ampCurve);
            amp.setParameters (s.ampEnv);
            fenv.setParameters (filterEnv);
        }

        /** Mono, Legato and Acid: another note under the same finger, without starting over.
            `slide`: Acid's, at the Slide time rather than the Glide. */
        void glideTo (int newNote, juce::uint64 order, bool slide) noexcept
        {
            note = newNote;
            targetNote = newNote;
            held = true;
            age = order;
            sliding = slide;
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
            accentCharge = 0.0f;
            shot = 0.0f;
        }

        double getCurrentNote() const noexcept   { return currentNote; }

        /** Adds what it plays to both sides (the same buffer twice when only one goes out and
            there's no room to work the other). */
        void render (float* outLeft, float* outRight, int numSamples, const Settings& s, double bendSemitones, double lfoValue,
                     double vibratoCents, juce::Random& random) noexcept
        {
            const auto& bank = wavetables();
            const bool twoSides = outRight != outLeft;
            const bool wide = s.wide && twoSides;
            const int channels = wide ? 2 : 1;

            for (int start = 0; start < numSamples; start += controlStep)
            {
                const auto count = juce::jmin (controlStep, numSamples - start);

                // Glide: the pitch slides in semitones, at the speed asked (Acid's slide at its own).
                const auto glideTime = sliding ? s.slideSeconds : s.glideSeconds;

                if (glideTime > 0.0 && std::abs (currentNote - targetNote) > 1.0e-4)
                {
                    const auto step = std::exp (-count / (glideTime * sampleRate));
                    currentNote = targetNote + (currentNote - targetNote) * step;
                }
                else
                {
                    currentNote = targetNote;
                }

                accentCharge *= 1.0f - juce::jmin (1.0f, static_cast<float> (count / sampleRate / 0.35));

                // The key shot: up in 3 ms while the key is down, back in 150 ms once it's let go.
                shot += ((held ? 1.0f : 0.0f) - shot) * (1.0f - std::exp (-static_cast<float> (count / (sampleRate * (held ? 0.003 : 0.15)))));

                const auto basePitch = currentNote + bendSemitones + vibratoCents / 100.0;
                const auto hz1 = 440.0 * std::pow (2.0, (basePitch + s.octave1 - 69.0) / 12.0);
                const auto hz2 = 440.0 * std::pow (2.0, (basePitch + s.pitch2 - 69.0) / 12.0);
                const auto dt1 = juce::jlimit (0.0, 0.45, hz1 / sampleRate);
                const auto dt2 = juce::jlimit (0.0, 0.45, hz2 / sampleRate);
                const auto dtSub = dt1 * 0.5;

                // A wavetable is read at the level that stays clean at this pitch.
                const auto level1 = s.wave1 >= firstTableWave ? caymanwt::Bank::levelFor (static_cast<float> (hz1) * s.maxRatio, static_cast<float> (sampleRate)) : 0;
                const auto level2 = s.wave2 >= firstTableWave ? caymanwt::Bank::levelFor (static_cast<float> (hz2) * s.maxRatio, static_cast<float> (sampleRate)) : 0;

                // The filter's cutoff where it is now: its envelope, the key, the LFO, an accent.
                if (s.filter != filterOff)
                {
                    const auto envNow = static_cast<double> (fenvLevel) * (accented ? 1.0 + 0.7 * s.accent : 1.0);
                    const auto octaves = s.envOctaves * envNow + s.keyTrack * (currentNote - 60.0) / 12.0 + s.lfoOctaves * lfoValue
                                       + (accented ? 2.0 * accentCharge * s.accent : 0.0) + s.keyShotOctaves * shot;
                    const auto fc = juce::jlimit (20.0, sampleRate * 0.45, s.cutoff * std::pow (2.0, octaves));

                    if (s.extraFilter < 0)
                    {
                        g = std::tan (juce::MathConstants<double>::pi * fc / sampleRate);
                        k = 2.0 - 1.95 * s.resonance;
                        a1 = 1.0 / (1.0 + g * (g + k));
                        a2 = g * a1;
                        a3 = g * a2;
                    }
                    else
                    {
                        for (int c = 0; c < channels; ++c)
                        {
                            extra[c].setType (s.extraFilter);
                            extra[c].setParams (static_cast<float> (fc), static_cast<float> (s.resonance));
                        }
                    }
                }

                for (int i = start; i < start + count; ++i)
                {
                    // The oscillators, each copy of the stack at its own pitch and place.
                    double mix[2] { 0.0, 0.0 };

                    for (int u = 0; u < s.unison; ++u)
                    {
                        const auto ratio = static_cast<double> (s.ratio[static_cast<size_t> (u)]);
                        double value = 0.0;

                        if (s.level1 > 0.0f)   value += s.level1 * oscillator (s.wave1, phase1[u], dt1 * ratio, s.width, bank, level1);
                        if (s.level2 > 0.0f)   value += s.level2 * oscillator (s.wave2 == classic ? sine : s.wave2, phase2[u], dt2 * ratio, s.width, bank, level2);

                        advance (phase1[u], dt1 * ratio);
                        advance (phase2[u], dt2 * ratio);

                        if (wide)
                        {
                            mix[0] += value * s.panLeft[static_cast<size_t> (u)];
                            mix[1] += value * s.panRight[static_cast<size_t> (u)];
                        }
                        else
                        {
                            mix[0] += value;
                        }
                    }

                    double middle = 0.0;      // the sub and the noise: in the middle, one of each

                    if (s.sub > 0.0f)      middle += s.sub * std::sin (twoPi * phaseSub);
                    if (s.noise > 0.0f)    middle += s.noise * (random.nextDouble() * 2.0 - 1.0);

                    advance (phaseSub, dtSub);
                    brightness *= brightnessFall;

                    double out[2] { 0.0, 0.0 };

                    for (int c = 0; c < channels; ++c)
                    {
                        auto sample = mix[c] * s.uniNorm + middle;

                        if (s.driveWet > 0.0f)
                            sample = drive (sample, c, s);

                        if (s.filter != filterOff)
                        {
                            if (s.extraFilter < 0)
                            {
                                // The state-variable filter (Zavalishin's).
                                const auto v3 = sample - ic2[c];
                                const auto v1 = a1 * ic1[c] + a2 * v3;
                                const auto v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
                                ic1[c] = 2.0 * v1 - ic1[c];
                                ic2[c] = 2.0 * v2 - ic2[c];
                                sample = s.filter == lowPass ? v2 : s.filter == bandPass ? v1 * k : sample - k * v1 - v2;
                            }
                            else
                            {
                                sample = extra[c].process (static_cast<float> (sample));
                            }
                        }

                        // what a lopsided drive leaves at 0 Hz, taken off again
                        if (s.driveWet > 0.0f)
                        {
                            const auto y = sample - dcIn[c] + dcPole * dcOut[c];
                            dcIn[c] = sample;
                            dcOut[c] = std::abs (y) < 1.0e-20 ? 0.0 : y;
                            sample = y;
                        }

                        out[c] = sample;
                    }

                    fenvLevel = fenv.getNextSample();
                    const auto level = gain * amp.getNextSample();

                    if (wide)
                    {
                        outLeft[i] += static_cast<float> (out[0]) * level;
                        outRight[i] += static_cast<float> (out[1]) * level;
                    }
                    else
                    {
                        const auto both = static_cast<float> (out[0]) * level;
                        outLeft[i] += both;

                        if (twoSides)
                            outRight[i] += both;
                    }
                }

                if (! amp.isActive())
                    return;
            }
        }

    private:
        /** The first My Plugin's drive curves: Warm (a soft clip), Hard (a clip), Fold (a sine, which
            turns over) and Fuzz (lopsided). */
        static float shape (float v, int kind) noexcept
        {
            switch (kind)
            {
                case 1:   return juce::jlimit (-1.0f, 1.0f, v);
                case 2:   return std::sin (v);
                case 3:   return caymanflt::softClip (v + 0.5f) - 0.46581f;
                default:  return caymanflt::softClip (v);
            }
        }

        /** Its drive, at the level that project's voices run at (half Cayman's), then back. */
        double drive (double x, int c, const Settings& s) noexcept
        {
            const auto curve = [&s] (float v)
            {
                const auto y = shape (v * s.driveGain, s.driveKind) * s.driveComp * s.drivePost;
                return v + (y - v) * s.driveWet;
            };
            const auto in = static_cast<float> (x * 0.5);

            if (s.driveRaw)
                return 2.0 * curve (in);

            float first, second;
            driveOver[c].up (in, first, second);
            return 2.0 * driveOver[c].down (curve (first), curve (second));
        }

        double oscillator (int wave, double phase, double dt, float width, const caymanwt::Bank& bank, int level) noexcept
        {
            if (wave >= firstTableWave)      // the tables number theirs from Sine: Pulse 25 is their fourth
                return bank.read (wave - 2, level, static_cast<float> (phase));

            switch (wave)
            {
                case classic:
                {
                    const auto p = twoPi * phase;
                    return std::sin (p) + brightness * (0.5 * std::sin (2.0 * p) + 0.25 * std::sin (3.0 * p));
                }
                // Every wave starts at zero going up, or (the saw) at its top coming down, as the
                // wavetables and the sine do: so two oscillators started together push their
                // fundamentals the same way. (The saw rose and the triangle started at its bottom
                // once: mixed with another wave they cancelled most of the note's fundamental.)
                case triangle:   return phase < 0.25 ? 4.0 * phase : (phase < 0.75 ? 2.0 - 4.0 * phase : 4.0 * phase - 4.0);
                case saw:        return 1.0 - 2.0 * phase + polyBlep (phase, dt);
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
        Envelope amp, fenv;
        float fenvLevel = 0.0f;
        int note = -1, targetNote = 60;
        bool held = false, sliding = false, accented = false;
        juce::uint64 age = 0;
        double currentNote = 60.0;
        double phase1[maxUnison] {}, phase2[maxUnison] {}, phaseSub = 0.0;
        double brightness = 1.0, brightnessFall = 1.0;
        float gain = 0.0f, accentCharge = 0.0f, shot = 0.0f;
        double g = 0.0, k = 2.0, a1 = 0.0, a2 = 0.0, a3 = 0.0, ic1[2] {}, ic2[2] {};
        caymanflt::Filter extra[2];      // the borrowed types, one for each side
        caymanflt::Oversampler driveOver[2];
        double dcIn[2] {}, dcOut[2] {}, dcPole = 0.999;
    };

    //==============================================================================
    class TestSynth final : public BuiltinPlugin
    {
    public:
        TestSynth()
            : BuiltinPlugin (TestSynths::describe(),
                             {
                                 { SynthParam::osc1Wave,   "Osc 1 Wave",    0.0,    17.0,     0.0, "",   0, true, false, true, nullptr, { "Classic", "Sine", "Triangle", "Saw", "Square", "Pulse", "Pulse 25", "Pulse 10", "Double Saw", "Organ", "Vox", "Glass", "Sync", "FM", "Fold", "Crush", "Metal", "Grit" } },
                                 { SynthParam::osc1Octave, "Osc 1 Octave", -2.0,     2.0,     0.0, "",   0, true },
                                 { SynthParam::osc1Level,  "Osc 1 Level",   0.0,   100.0,   100.0, "%",  0 },
                                 { SynthParam::osc2Wave,   "Osc 2 Wave",    0.0,    17.0,     3.0, "",   0, true, false, true, nullptr, { "Classic", "Sine", "Triangle", "Saw", "Square", "Pulse", "Pulse 25", "Pulse 10", "Double Saw", "Organ", "Vox", "Glass", "Sync", "FM", "Fold", "Crush", "Metal", "Grit" } },
                                 { SynthParam::osc2Octave, "Osc 2 Octave", -2.0,     2.0,     0.0, "",   0, true },
                                 { SynthParam::osc2Semi,   "Osc 2 Semi",  -12.0,    12.0,     0.0, "st", 0, true },
                                 { SynthParam::osc2Detune, "Osc 2 Detune",  0.0,    50.0,     7.0, "ct", 0 },
                                 { SynthParam::osc2Level,  "Osc 2 Level",   0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::subLevel,   "Sub",           0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::noiseLevel, "Noise",         0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::pulseWidth, "Pulse Width",   5.0,    95.0,    50.0, "%",  0 },
                                 { SynthParam::filterType, "Filter",        0.0,    17.0,     0.0, "",   0, true, false, true, nullptr,
                                   { "Off", "Low Pass", "High Pass", "Band Pass", "LP 24", "HP 24", "BP 24", "Notch", "Peak", "Low Shelf", "High Shelf",
                                     "Dirty LP", "Ladder", "Formant", "Comb +", "Comb -", "Phaser", "Acid" } },
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
                                 { SynthParam::lfoWave,    "LFO Wave",      0.0,    15.0,     0.0, "",   0, true, false, true, nullptr, { "Sine", "Triangle", "Square", "Random", "Pulse 25", "Pulse 10", "Double Saw", "Organ", "Vox", "Glass", "Sync", "FM", "Fold", "Crush", "Metal", "Grit" } },
                                 { SynthParam::lfoRate,    "LFO Rate",      0.05,   20.0,     5.0, "Hz", 2 },
                                 { SynthParam::vibrato,    "Vibrato",       0.0,   100.0,     0.0, "ct", 0 },
                                 { SynthParam::lfoFilter,  "LFO Filter",    0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::tremolo,    "Tremolo",       0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::glide,      "Glide",         0.0,  2000.0,     0.0, "ms", 0 },
                                 { SynthParam::mode,       "Mode",          0.0,     3.0,     0.0, "",   0, true, false, true, nullptr, { "Poly", "Mono", "Legato", "Acid" } },
                                 { SynthParam::bendRange,  "Bend Range",    0.0,    24.0,     2.0, "st", 0, true },
                                 { SynthParam::drive,      "Drive",         0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::chorus,     "Chorus",        0.0,   100.0,     0.0, "%",  0 },
                                 { SynthParam::output,     "Output",      -24.0,     6.0,    -6.0, "dB", 1 },
                                 // The effects. Each one's Mix starts at 0 (off), so the old tone is untouched.
                                 { SynthParam::chorusRate,     "Chorus Rate",      0.0,   100.0,   25.0, "%",  0 },
                                 { SynthParam::chorusDepth,    "Chorus Depth",     0.0,   100.0,   60.0, "%",  0 },
                                 { SynthParam::phaserRate,     "Phaser Rate",      0.05,    4.0,    0.5, "Hz", 2 },
                                 { SynthParam::phaserDepth,    "Phaser Depth",     0.0,   100.0,   80.0, "%",  0 },
                                 { SynthParam::phaserFeedback, "Phaser Feedback",  0.0,    90.0,   50.0, "%",  0 },
                                 { SynthParam::phaserMix,      "Phaser Mix",       0.0,   100.0,    0.0, "%",  0 },
                                 { SynthParam::wahSens,        "Wah Sensitivity",  0.0,   100.0,   50.0, "%",  0 },
                                 { SynthParam::wahFreq,        "Wah Frequency",   80.0,  1500.0,  400.0, "Hz", 0 },
                                 { SynthParam::wahQ,           "Wah Resonance",    0.0,   100.0,   60.0, "%",  0 },
                                 { SynthParam::wahRate,        "Wah Rate",         0.05,    8.0,    1.0, "Hz", 2 },
                                 { SynthParam::wahDepth,       "Wah Depth",        0.0,   100.0,    0.0, "%",  0 },
                                 { SynthParam::wahMix,         "Wah Mix",          0.0,   100.0,    0.0, "%",  0 },
                                 { SynthParam::vowel,          "Vowel",            0.0,     4.0,    0.0, "",   0, true, true, true, nullptr, { "A", "E", "O", "U", "I" } },   // the first sweep's (unused now)
                                 { SynthParam::vowelRate,      "Vowel Rate",       0.05,    8.0,    0.5, "Hz", 2, false, true },
                                 { SynthParam::vowelDepth,     "Vowel Depth",      0.0,   100.0,   50.0, "%",  0, false, true },
                                 { SynthParam::vowelRes,       "Vowel Resonance",  0.0,   100.0,   50.0, "%",  0 },
                                 { SynthParam::vowelMix,       "Vowel Mix",        0.0,   100.0,    0.0, "%",  0 },
                                 { SynthParam::delayTime,      "Delay Time",      10.0,  2400.0,  375.0, "ms", 0 },
                                 { SynthParam::delayFeedback,  "Delay Feedback",   0.0,    95.0,   45.0, "%",  0 },
                                 { SynthParam::delayTone,      "Delay Tone",     500.0, 20000.0, 5000.0, "Hz", 0 },
                                 { SynthParam::delayMix,       "Delay Mix",        0.0,   100.0,    0.0, "%",  0 },
                                 { SynthParam::reverbSize,     "Reverb Size",      0.0,   100.0,   50.0, "%",  0 },
                                 { SynthParam::reverbDamp,     "Reverb Damping",   0.0,   100.0,   50.0, "%",  0 },
                                 { SynthParam::reverbPre,      "Reverb Pre-Delay", 0.0,   200.0,   20.0, "ms", 0 },
                                 { SynthParam::reverbMix,      "Reverb Mix",       0.0,   100.0,    0.0, "%",  0 },
                                 // The voice stack and Acid mode. One voice and no accent: the old tone untouched.
                                 { SynthParam::unison,         "Unison",           1.0,     7.0,    1.0, "",   0, true },
                                 { SynthParam::uniDetune,      "Unison Detune",    0.0,   100.0,   20.0, "ct", 0 },
                                 { SynthParam::width,          "Width",            0.0,   100.0,   60.0, "%",  0 },
                                 { SynthParam::slide,          "Slide",            1.0,   500.0,   45.0, "ms", 0 },
                                 { SynthParam::accent,         "Accent",           0.0,   100.0,    0.0, "%",  0 },
                                 // The arpeggiator (Domina's): off at first.
                                 { SynthParam::arpOn,          "Arp",              0.0,     1.0,    0.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::arpSeed,        "Arp Seed",         0.0, 999999.0,  973.0, "",  0, true },
                                 { SynthParam::arpIdiom,       "Arp Idiom",        0.0, static_cast<double> (fanan::idiomCount() - 1), 0.0, "", 0, true, false, true, nullptr, idiomNames() },
                                 { SynthParam::arpOriginality, "Arp Originality",  0.0,   100.0,   75.0, "%",  0 },
                                 { SynthParam::arpBars,        "Arp Bars",         1.0,     8.0,    4.0, "",   0, true },
                                 { SynthParam::arpOctaves,     "Arp Octaves",      1.0,     4.0,    1.0, "",   0, true },
                                 { SynthParam::arpComplexity,  "Arp Complexity",   0.0,   100.0,   80.0, "%",  0 },
                                 { SynthParam::arpRests,       "Arp Rests",        0.0,   100.0,   12.0, "%",  0 },
                                 { SynthParam::arpW16,         "Arp 1/16",         0.0,   100.0,  100.0, "%",  0 },
                                 { SynthParam::arpW8,          "Arp 1/8",          0.0,   100.0,  100.0, "%",  0 },
                                 { SynthParam::arpW4,          "Arp 1/4",          0.0,   100.0,   35.0, "%",  0 },
                                 { SynthParam::arpSort,        "Arp Sort",         0.0,     1.0,    1.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::arpGateMin,     "Arp Gate Min",     0.0,   100.0,   45.0, "%",  0 },
                                 { SynthParam::arpGateMax,     "Arp Gate Max",     0.0,   100.0,   90.0, "%",  0 },
                                 { SynthParam::arpRun,         "Arp Run",          0.0,     2.0,    1.0, "",   0, true, false, true, nullptr, { "Chord", "Free", "Song" } },
                                 { SynthParam::arpHold,        "Arp Hold",         0.0,     1.0,    0.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 // The cutoff's key shot (0: none), the second LFO, sync, the matrix (nothing routed).
                                 { SynthParam::keyShot,        "Key Shot",       -100.0,  100.0,    0.0, "%",  0 },
                                 { SynthParam::lfo1Sync,       "LFO 1 Sync",       0.0,     1.0,    0.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::lfo1Note,       "LFO 1 Note",       0.0,    13.0,    4.0, "",   0, true, false, true, nullptr, { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/2 D", "1/4 D", "1/8 D", "1/4 T", "1/8 T", "1/16 T" } },
                                 { SynthParam::lfo1Depth,      "LFO 1 Depth",      0.0,   100.0,   50.0, "%",  0 },
                                 { SynthParam::lfo2Wave,       "LFO 2 Wave",       0.0,    15.0,    1.0, "",   0, true, false, true, nullptr, { "Sine", "Triangle", "Square", "Random", "Pulse 25", "Pulse 10", "Double Saw", "Organ", "Vox", "Glass", "Sync", "FM", "Fold", "Crush", "Metal", "Grit" } },
                                 { SynthParam::lfo2Rate,       "LFO 2 Rate",       0.05,   20.0,    0.5, "Hz", 2 },
                                 { SynthParam::lfo2Sync,       "LFO 2 Sync",       0.0,     1.0,    0.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::lfo2Note,       "LFO 2 Note",       0.0,    13.0,    2.0, "",   0, true, false, true, nullptr, { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/2 D", "1/4 D", "1/8 D", "1/4 T", "1/8 T", "1/16 T" } },
                                 { SynthParam::lfo2Depth,      "LFO 2 Depth",      0.0,   100.0,   50.0, "%",  0 },
                                 { SynthParam::route (0, 0), "LFO 1 > Osc 2 Wave", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 1), "LFO 1 > Osc 2 Detune", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 2), "LFO 1 > Pulse Width", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 3), "LFO 1 > Osc 2 Level", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 4), "LFO 1 > Sub", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 5), "LFO 1 > Cutoff", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 6), "LFO 1 > Resonance", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 7), "LFO 1 > Env Amount", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 8), "LFO 1 > Key Track", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 9), "LFO 1 > Attack", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 10), "LFO 1 > Unison Detune", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 11), "LFO 1 > Width", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (0, 12), "LFO 1 > Velocity", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 0), "LFO 2 > Osc 2 Wave", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 1), "LFO 2 > Osc 2 Detune", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 2), "LFO 2 > Pulse Width", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 3), "LFO 2 > Osc 2 Level", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 4), "LFO 2 > Sub", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 5), "LFO 2 > Cutoff", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 6), "LFO 2 > Resonance", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 7), "LFO 2 > Env Amount", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 8), "LFO 2 > Key Track", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 9), "LFO 2 > Attack", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 10), "LFO 2 > Unison Detune", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 11), "LFO 2 > Width", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::route (1, 12), "LFO 2 > Velocity", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 // The vowel's eight columns (Zodiak's A E O U I U O E), the note each lasts, the mode,
                                 // and its gate: 16 steps, two a column, all open (no gating until one's turned off).
                                 { SynthParam::vowelColumns + 0, "Vowel Column 1", 0.0, 4.0, 0.0, "", 0, true, false, true, nullptr, { "A", "E", "O", "U", "I" } },
                                 { SynthParam::vowelColumns + 1, "Vowel Column 2", 0.0, 4.0, 1.0, "", 0, true, false, true, nullptr, { "A", "E", "O", "U", "I" } },
                                 { SynthParam::vowelColumns + 2, "Vowel Column 3", 0.0, 4.0, 2.0, "", 0, true, false, true, nullptr, { "A", "E", "O", "U", "I" } },
                                 { SynthParam::vowelColumns + 3, "Vowel Column 4", 0.0, 4.0, 3.0, "", 0, true, false, true, nullptr, { "A", "E", "O", "U", "I" } },
                                 { SynthParam::vowelColumns + 4, "Vowel Column 5", 0.0, 4.0, 4.0, "", 0, true, false, true, nullptr, { "A", "E", "O", "U", "I" } },
                                 { SynthParam::vowelColumns + 5, "Vowel Column 6", 0.0, 4.0, 3.0, "", 0, true, false, true, nullptr, { "A", "E", "O", "U", "I" } },
                                 { SynthParam::vowelColumns + 6, "Vowel Column 7", 0.0, 4.0, 2.0, "", 0, true, false, true, nullptr, { "A", "E", "O", "U", "I" } },
                                 { SynthParam::vowelColumns + 7, "Vowel Column 8", 0.0, 4.0, 1.0, "", 0, true, false, true, nullptr, { "A", "E", "O", "U", "I" } },
                                 { SynthParam::vowelNote, "Vowel Note", 0.0, 13.0, 4.0, "", 0, true, false, true, nullptr, { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/2 D", "1/4 D", "1/8 D", "1/4 T", "1/8 T", "1/16 T" } },
                                 { SynthParam::vowelMode, "Vowel Mode", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Morph", "Jump" } },
                                 { SynthParam::vowelGate, "Vowel Gate", 0.0, 100.0, 100.0, "%", 0 },
                                 { SynthParam::vowelSteps + 0, "Vowel Gate 1", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 1, "Vowel Gate 2", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 2, "Vowel Gate 3", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 3, "Vowel Gate 4", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 4, "Vowel Gate 5", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 5, "Vowel Gate 6", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 6, "Vowel Gate 7", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 7, "Vowel Gate 8", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 8, "Vowel Gate 9", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 9, "Vowel Gate 10", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 10, "Vowel Gate 11", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 11, "Vowel Gate 12", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 12, "Vowel Gate 13", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 13, "Vowel Gate 14", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 14, "Vowel Gate 15", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::vowelSteps + 15, "Vowel Gate 16", 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 // The drive's kind (Warm: as it always was) and RAW (on: as it always was).
                                 { SynthParam::driveType, "Drive Type", 0.0, 3.0, 0.0, "", 0, true, false, true, nullptr, { "Warm", "Hard", "Fold", "Fuzz" } },
                                 { SynthParam::driveRaw,  "Drive Raw",  0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 // The amp envelope's SHAPE: 0 is the straight line it always had.
                                 { SynthParam::ampShape,  "Amp Shape", -20.0, 20.0, 6.0, "", 1 },     // Grex's EXP
                                 // The delay free (as it was) at first; VIBRATION's rate where LFO 1's was.
                                 { SynthParam::delaySync, "Delay Sync", 0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } },
                                 { SynthParam::delayNote, "Delay Note", 0.0, 13.0, 7.0, "", 0, true, false, true, nullptr,
                                   { "1/32", "1/16 T", "1/16", "1/8 T", "1/16 D", "1/8", "1/4 T", "1/8 D", "1/4", "1/2 T", "1/4 D", "1/2", "1/2 D", "1/1" } },
                                 { SynthParam::vibRate,   "Vibration Rate", 0.5, 12.0, 5.0, "Hz", 2 },
                                 { SynthParam::lowVel,    "Low Velocity", 0.0, 100.0, 0.0, "%", 0 }
                             })
        {
            // Each destination's parameter, for moving it in its knob's travel.
            for (int d = 0; d < SynthParam::numDestinations; ++d)
                for (const auto& spec : getSpecs())
                    if (spec.id == SynthParam::destinations[d])
                        destinationSpecs[static_cast<size_t> (d)] = &spec;
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

        /** The panel's wheels, sent in with the next block as a pitch wheel and a mod wheel would be. */
        void panelWheel (int wheel, float value) override
        {
            if (wheel == 0) panelBendIn.store (juce::jlimit (-1.0f, 1.0f, value));
            else            panelModIn.store (juce::jlimit (0.0f, 1.0f, value));
        }

        /** "lfo 1" and "lfo 2": where each LFO is now (-1..1), so the knobs it plays can move with it. */
        float readSource (const juce::String& source) const noexcept override
        {
            if (source == "lfo 1") return lfoNow[0].load (std::memory_order_relaxed);
            if (source == "lfo 2") return lfoNow[1].load (std::memory_order_relaxed);
            if (source == "vowel step") return vowelShown.load (std::memory_order_relaxed);     // the vowel's marker, 0..8
            if (source == "bend") return bendNow.load (std::memory_order_relaxed);             // the pitch wheel, -1..1
            if (source == "mod wheel") return modNow.load (std::memory_order_relaxed);         // the mod wheel, 0..1
            if (source == "key shot") return shotNow.load (std::memory_order_relaxed);         // how far the newest note's key shot is thrown, 0..1
            return BuiltinPlugin::readSource (source);
        }

        void readCurve (const juce::String& source, float* destination, int numSamples) override
        {
            if (source != "synth.wave")
                return BuiltinPlugin::readCurve (source, destination, numSamples);

            // What it plays: the last sound out, as the scope shows it.
            const auto count = juce::jmin (numSamples, static_cast<int> (scope.size()));
            const auto end = scopeAt.load (std::memory_order_acquire);

            for (int i = 0; i < count; ++i)
                destination[i] = scope[static_cast<size_t> ((end + i) % static_cast<int> (scope.size()))] * 2.0f;
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

            // Both sides are worked: the other one in a spare buffer when only one goes out (in
            // the same buffer if a block is ever longer than that spare).
            const bool stereoOut = outRight != nullptr && outRight != outLeft;
            auto* right = stereoOut ? outRight : (numSamples <= static_cast<int> (spare.size()) ? spare.data() : outLeft);

            if (! stereoOut && right != outLeft)
                juce::FloatVectorOperations::clear (right, numSamples);

            // The panel's keyboard joins the notes that came.
            incoming.clear();
            incoming.addEvents (midi, 0, numSamples, 0);
            keyboard.processNextMidiBuffer (incoming, 0, numSamples, true);

            // and its wheels, where they moved
            if (const auto b = panelBendIn.exchange (wheelUnmoved); b != wheelUnmoved)
                incoming.addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, 8192 + juce::roundToInt (b * (b < 0.0f ? 8192.0f : 8191.0f)))), 0);
            if (const auto m = panelModIn.exchange (wheelUnmoved); m != wheelUnmoved)
                incoming.addEvent (juce::MidiMessage::controllerEvent (1, 1, juce::roundToInt (m * 127.0f)), 0);

            if (arpIsOn != arpWasOn)
                switchArp();

            if (arpIsOn)
            {
                playArp (outLeft, right, numSamples, transport);
            }
            else
            {
                // Played between the events, each where it falls.
                int at = 0;

                for (const auto metadata : incoming)
                {
                    const auto position = juce::jlimit (0, numSamples, metadata.samplePosition);
                    renderSpan (outLeft, right, at, position - at);
                    at = position;
                    handle (metadata.getMessage());
                }

                renderSpan (outLeft, right, at, numSamples - at);
            }

            // The vowel's marker, in columns: from the song's beat while it plays, its own clock
            // (at the song's tempo) while it's stopped, so it carries on from where the song left it.
            vowelPerSample = getBlockTempo() / 60.0 / getSampleRate() / fx.vowelBeats;

            if (transport != nullptr && isBlockPlaying() && (transport->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) != 0)
                vowelAt = static_cast<double> (transport->song_pos_beats) / static_cast<double> (CLAP_BEATTIME_FACTOR) / fx.vowelBeats;
            else
                vowelAt = vowelClock;

            const auto vowelEnd = vowelAt + numSamples * vowelPerSample;
            vowelClock = vowelEnd - 8.0 * std::floor (vowelEnd / 8.0);
            vowelShown.store (static_cast<float> (vowelClock), std::memory_order_relaxed);

            finish (outLeft, right, numSamples, stereoOut);
            measureOutput (outLeft, stereoOut ? outRight : outLeft, numSamples);
        }

    private:
        void processAudio (float*, float*, int) noexcept override {}   // process() plays it

        void prepareAudio (double rate, int maxBlockSize) override
        {
            wavetables();      // built here the first time, never while playing

            for (auto& voice : voices)
            {
                voice.prepare (rate);
                voice.kill();
            }

            arp.reset();
            arpEvents.clear();
            arpEvents.reserve (1024);
            arpOffs.clear();
            arpOffs.reserve (256);
            timed.clear();
            timed.reserve (2048);
            arpBeat = 0.0;
            arpWasOn = arpIsOn;

            chorusFx.prepare (rate);
            phaserFx.prepare (rate);
            wahFx.prepare (rate);
            vowelFx.prepare (rate);
            delayFx.prepare (rate);
            reverbFx.prepare (rate);
            phaserOn = wahOn = vowelOn = delayOn = reverbOn = false;
            spare.assign (static_cast<size_t> (juce::jmax (64, maxBlockSize)), 0.0f);
            incoming.ensureSize (4096);
            scope.assign (4096, 0.0f);
            lfoPhase = lfo2Phase = vibPhase = 0.0;
            live = s;
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
            s.ampCurve = static_cast<float> (valueOf (ampShape));
            s.velocity = static_cast<float> (valueOf (velocity) / 100.0);
            s.lowVelGamma = std::pow (2.5f, -static_cast<float> (valueOf (lowVel) / 100.0));
            s.lfoShape = static_cast<int> (valueOf (lfoWave));
            s.lfoHz = valueOf (lfoRate);
            lfo2Shape = static_cast<int> (valueOf (lfo2Wave));
            lfo2Hz = valueOf (lfo2Rate);
            lfoSynced[0] = valueOf (lfo1Sync) > 0.5;
            lfoSynced[1] = valueOf (lfo2Sync) > 0.5;
            lfoBeats[0] = noteBeats[juce::jlimit (0, 13, static_cast<int> (std::lround (valueOf (lfo1Note))))];
            lfoBeats[1] = noteBeats[juce::jlimit (0, 13, static_cast<int> (std::lround (valueOf (lfo2Note))))];
            s.keyShotOctaves = valueOf (keyShot) / 100.0 * std::log2 (20000.0 / 20.0);   // the whole of the cutoff knob's travel at 100 %

            // The matrix: what each LFO plays, and how far (at 100 %, half the knob's travel either way).
            routed = false;

            for (int lfo = 0; lfo < 2; ++lfo)
            {
                depth[lfo] = valueOf (lfo == 0 ? lfo1Depth : lfo2Depth) / 100.0;

                for (int d = 0; d < numDestinations; ++d)
                {
                    routeOn[lfo][d] = valueOf (route (lfo, d)) > 0.5 && depth[lfo] > 0.0;
                    routed = routed || routeOn[lfo][d];
                }
            }
            s.vibratoCents = valueOf (vibrato);
            vibHz = valueOf (vibRate);
            s.lfoOctaves = 2.0 * valueOf (lfoFilter) / 100.0;
            s.tremolo = static_cast<float> (valueOf (tremolo) / 100.0);
            s.glideSeconds = valueOf (glide) / 1000.0;
            s.slideSeconds = valueOf (slide) / 1000.0;
            s.accent = static_cast<float> (valueOf (accent) / 100.0);
            s.mode = static_cast<int> (valueOf (mode));

            // Cayman's filter types from the fifth on, as the borrowed filter numbers them.
            static constexpr int borrowedTypes[] { 1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
            s.extraFilter = s.filter > bandPass ? borrowedTypes[juce::jlimit (0, 13, s.filter - bandPass - 1)] : -1;

            // The stack: the inner copies sit closer to the middle, so the beating never falls
            // into a pattern; each has its place across the stereo.
            s.unison = juce::jlimit (1, maxUnison, static_cast<int> (std::lround (valueOf (unison))));
            const auto spread = static_cast<float> (valueOf (width) / 100.0);
            stack (s, static_cast<float> (valueOf (uniDetune)), spread);
            s.wide = s.unison > 1 && spread > 0.0f;
            live = s;

            // The notes sounding follow the envelopes' knobs too.
            for (auto& voice : voices)
                if (voice.isActive())
                    voice.setEnvelopes (s);

            // The arpeggiator's settings (it bakes its pattern again when one of these changes).
            arpIsOn = valueOf (arpOn) > 0.5;
            arpHolds = valueOf (arpHold) > 0.5;
            arpBars = juce::jlimit (1, 8, static_cast<int> (std::lround (valueOf (SynthParam::arpBars))));
            const auto gateA = static_cast<float> (valueOf (arpGateMin) / 100.0), gateB = static_cast<float> (valueOf (arpGateMax) / 100.0);
            auto& a = arp.params;
            a.seed = static_cast<uint32_t> (juce::jlimit (0, 999999, static_cast<int> (std::lround (valueOf (arpSeed)))));
            a.patternBars = arpBars;
            a.octaveRange = juce::jlimit (1, 4, static_cast<int> (std::lround (valueOf (arpOctaves))));
            a.complexity = static_cast<float> (valueOf (arpComplexity) / 100.0);
            a.idiom = juce::jlimit (0, fanan::idiomCount() - 1, static_cast<int> (std::lround (valueOf (arpIdiom))));
            a.originality = static_cast<float> (valueOf (arpOriginality) / 100.0);
            a.weight16 = static_cast<float> (valueOf (arpW16) / 100.0);
            a.weight8 = static_cast<float> (valueOf (arpW8) / 100.0);
            a.weight4 = static_cast<float> (valueOf (arpW4) / 100.0);
            a.restProb = static_cast<float> (valueOf (arpRests) / 100.0);
            a.gateMin = juce::jmin (gateA, gateB);
            a.gateMax = juce::jmax (gateA, gateB);
            a.sortNotes = valueOf (arpSort) > 0.5;
            a.restart = static_cast<fanan::RestartMode> (juce::jlimit (0, 2, static_cast<int> (std::lround (valueOf (arpRun)))));
            s.bendRange = valueOf (bendRange);
            {
                // the drive: the first My Plugin's curves (all the way wet from 20 %). Its gain (1 + 40d²)
                // squared a saw by 40 % and changed nothing past it; here the same 41 times at the top is
                // reached as 41^(d^1.5), so the squash keeps growing to the end, and it isn't trimmed after.
                // Fold keeps the first sums: past its first turn-over it keeps changing anyway.
                const auto d = static_cast<float> (valueOf (drive) / 100.0);
                s.driveKind = juce::jlimit (0, 3, static_cast<int> (std::lround (valueOf (driveType))));
                s.driveRaw = valueOf (driveRaw) > 0.5;
                s.driveGain = s.driveKind == 2 ? 1.0f + 40.0f * d * d : std::pow (41.0f, d * std::sqrt (d));
                const auto reference = s.driveKind == 2 ? juce::jmin (s.driveGain, juce::MathConstants<float>::halfPi) : s.driveGain;
                // (Fuzz reaches about 2.7 times further down than up: the first My Plugin levelled it by
                // its short side, which sends a chord over full scale. Here it's levelled by its long one.)
                s.driveComp = 1.0f / juce::jmax (0.1f, std::abs (driveShape (reference, s.driveKind)), std::abs (driveShape (-reference, s.driveKind)));
                s.driveWet = juce::jmin (1.0f, d * 5.0f);
                s.drivePost = s.driveKind == 2 ? 1.0f - 0.25f * d : 1.0f;
            }
            // The voices play at 0.14 (the old tone's level, which the effects were tuned to); the first
            // My Plugin's oscillators come out at 0.5, so after the effects it's made up to that
            // (+11 dB), and OUTPUT is its Volume (-6 dB at first).
            outputGain = static_cast<float> (juce::Decibels::decibelsToGain (valueOf (output))) * myPluginLevel;

            const auto part = [this] (uint32_t id) { return static_cast<float> (valueOf (id) / 100.0); };
            const auto real = [this] (uint32_t id) { return static_cast<float> (valueOf (id)); };
            fx.chorusRate = part (chorusRate);     fx.chorusDepth = part (chorusDepth);     fx.chorusMix = part (chorus);
            fx.phaserRate = (real (phaserRate) - 0.05f) / (4.0f - 0.05f);
            fx.phaserDepth = part (phaserDepth);   fx.phaserFeedback = part (phaserFeedback);   fx.phaserMix = part (phaserMix);
            fx.wahSens = part (wahSens);           fx.wahFreq = real (wahFreq);             fx.wahQ = part (wahQ);
            fx.wahRate = real (wahRate);           fx.wahDepth = part (wahDepth);           fx.wahMix = part (wahMix);
            fx.vowelRes = part (vowelRes);         fx.vowelMix = part (vowelMix);
            fx.vowelJump = valueOf (vowelMode) > 0.5;
            fx.vowelBeats = noteBeats[juce::jlimit (0, 13, static_cast<int> (std::lround (valueOf (vowelNote))))];
            fx.gateDepth = part (vowelGate);
            fx.gateSteps = 0;

            for (int i = 0; i < 8; ++i)
                fx.vowelColumns[static_cast<size_t> (i)] = juce::jlimit (0, 4, static_cast<int> (std::lround (valueOf (vowelColumns + static_cast<uint32_t> (i)))));

            for (int i = 0; i < 16; ++i)
                if (valueOf (vowelSteps + static_cast<uint32_t> (i)) > 0.5)
                    fx.gateSteps |= 1u << i;
            fx.delaySeconds = real (delayTime) / 1000.0f;   fx.delayFeedback = part (delayFeedback);
            fx.delaySynced = valueOf (delaySync) > 0.5;
            fx.delayBeats = static_cast<float> (delayBeats[juce::jlimit (0, 13, static_cast<int> (std::lround (valueOf (delayNote))))]);
            fx.delayTone = real (delayTone);       fx.delayMix = part (delayMix);
            fx.reverbSize = part (reverbSize);     fx.reverbDamp = part (reverbDamp);
            fx.reverbPre = real (reverbPre) / 200.0f;       fx.reverbMix = part (reverbMix);
        }

        /** The stack: the inner copies sit closer to the middle, so the beating never falls into a
            pattern; each has its place across the stereo. */
        static void stack (Settings& settings, float detune, float spread) noexcept
        {
            settings.maxRatio = 1.0f;

            for (int u = 0; u < maxUnison; ++u)
            {
                const auto t = settings.unison > 1 && u < settings.unison ? static_cast<float> (u) / static_cast<float> (settings.unison - 1) - 0.5f : 0.0f;
                const auto bent = std::copysign (0.5f * std::pow (std::abs (t * 2.0f), 1.5f), t);
                const auto pan = (t * 2.0f * spread + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                settings.ratio[static_cast<size_t> (u)] = std::exp2 (bent * detune / 1200.0f);
                settings.panLeft[static_cast<size_t> (u)] = std::cos (pan) * 1.41421356f;
                settings.panRight[static_cast<size_t> (u)] = std::sin (pan) * 1.41421356f;
                settings.maxRatio = juce::jmax (settings.maxRatio, settings.ratio[static_cast<size_t> (u)]);
            }

            settings.uniNorm = 1.0f / std::sqrt (static_cast<float> (settings.unison));
        }

        /** The settings the voices play now: the knobs' own, then the matrix's LFOs on top. A
            destination moves in its knob's travel (a frequency or time as its knob turns, a
            choice by whole steps), so it sounds as the knob would turned there. */
        void modulate (double lfo1, double lfo2) noexcept
        {
            using namespace SynthParam;
            live = s;

            if (! routed)
                return;

            float detune = static_cast<float> (valueOf (uniDetune)), spread = static_cast<float> (valueOf (width) / 100.0);
            bool restack = false, envelopes = false;

            for (int d = 0; d < numDestinations; ++d)
            {
                const auto offset = (routeOn[0][d] ? depth[0] * 0.5 * lfo1 : 0.0) + (routeOn[1][d] ? depth[1] * 0.5 * lfo2 : 0.0);
                const auto* spec = destinationSpecs[static_cast<size_t> (d)];

                if (std::abs (offset) < 1.0e-6 || spec == nullptr)
                    continue;

                // the knob's travel, moved, and back to a value
                const auto log = BuiltinPlugin::isLogRange (*spec);
                const auto base = valueOf (destinations[d]);
                const auto travel = juce::jlimit (0.0, 1.0, (log ? std::log (juce::jmax (spec->minValue, base) / spec->minValue) / std::log (spec->maxValue / spec->minValue)
                                                                 : (base - spec->minValue) / (spec->maxValue - spec->minValue)) + offset);
                auto v = log ? spec->minValue * std::pow (spec->maxValue / spec->minValue, travel) : spec->minValue + travel * (spec->maxValue - spec->minValue);

                if (spec->stepped)
                    v = std::round (v);

                switch (destinations[d])
                {
                    case osc2Wave:     live.wave2 = static_cast<int> (v); break;
                    case osc2Detune:   live.pitch2 = 12.0 * valueOf (osc2Octave) + valueOf (osc2Semi) + v / 100.0; break;
                    case pulseWidth:   live.width = static_cast<float> (v / 100.0); break;
                    case osc2Level:    live.level2 = static_cast<float> (v / 100.0); break;
                    case subLevel:     live.sub = static_cast<float> (v / 100.0); break;
                    case cutoff:       live.cutoff = v; break;
                    case resonance:    live.resonance = v / 100.0; break;
                    case filterEnv:    live.envOctaves = 5.0 * v / 100.0; break;
                    case keyTrack:     live.keyTrack = v / 100.0; break;
                    case aAttack:      live.ampEnv.attack = static_cast<float> (v / 1000.0); envelopes = true; break;
                    case uniDetune:    detune = static_cast<float> (v); restack = true; break;
                    case width:        spread = static_cast<float> (v / 100.0); restack = true; break;
                    case velocity:     live.velocity = static_cast<float> (v / 100.0); break;
                    default:           break;
                }
            }

            if (restack)
            {
                stack (live, detune, spread);
                live.wide = live.unison > 1 && spread > 0.0f;
            }

            if (envelopes)
                for (auto& voice : voices)
                    if (voice.isActive())
                        voice.setEnvelopes (live);
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
                bendNow.store (static_cast<float> (bend), std::memory_order_relaxed);
            }
            else if (message.isController())
            {
                const auto number = message.getControllerNumber();

                if (number == 1)
                {
                    modWheel = message.getControllerValue() / 127.0;
                    modNow.store (static_cast<float> (modWheel), std::memory_order_relaxed);
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
                    arp.reset();
                    arpOffs.clear();
                }
            }
            else if (message.isAllNotesOff() || message.isAllSoundOff())
            {
                for (auto& voice : voices)
                    voice.kill();

                keyDown.fill (false);
                arp.reset();
                arpOffs.clear();
            }
        }

        //==============================================================================
        /** The arpeggiator going on or off: what was sounding is let go, and it starts clean. */
        void switchArp() noexcept
        {
            for (auto& voice : voices)
                if (voice.isActive() && voice.isHeld())
                    voice.release();

            keyDown.fill (false);
            arp.reset();
            arpOffs.clear();
            arpWasOn = arpIsOn;
        }

        /** A block with the arpeggiator on: the keys go to it (they are its chord), and the notes
            it makes are played where they fall. It keeps the song's beat while the song plays,
            and its own clock at the song's tempo while it doesn't. */
        void playArp (float* left, float* right, int numSamples, const clap_event_transport_t* transport) noexcept
        {
            const auto beatsPerSample = juce::jlimit (20.0, 400.0, getBlockTempo()) / 60.0 / getSampleRate();
            double beatsPerBar = 4.0;

            if (transport != nullptr && (transport->flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE) != 0 && transport->tsig_denom > 0)
                beatsPerBar = juce::jmax (1.0, transport->tsig_num * 4.0 / transport->tsig_denom);

            double blockStart = arpBeat;

            if (transport != nullptr && isBlockPlaying() && (transport->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) != 0)
            {
                const auto songBeat = static_cast<double> (transport->song_pos_beats) / static_cast<double> (CLAP_BEATTIME_FACTOR);

                // The song moved (it started, looped or was sent somewhere): the pattern goes with it.
                if (std::abs (songBeat - arpBeat) > 0.02)
                    arp.reanchor (songBeat);

                blockStart = songBeat;
            }

            arp.params.beatsPerBar = static_cast<float> (beatsPerBar);
            arp.params.loopStartBeats = 0.0f;
            arp.params.loopEndBeats = static_cast<float> (arpBars * beatsPerBar);
            arp.rebakeIfNeeded();
            arp.setHoldBar (arpHolds, beatsPerBar, blockStart);

            int at = 0;

            // Plays up to a sample: the arp's notes for that stretch, and those ending in it.
            const auto playTo = [&] (int position)
            {
                if (position <= at)
                    return;

                const auto fromBeat = blockStart + at * beatsPerSample, toBeat = blockStart + position * beatsPerSample;
                const auto sampleAt = [&] (double beat) { return juce::jlimit (at, position, at + static_cast<int> ((beat - fromBeat) / beatsPerSample)); };
                timed.clear();

                for (size_t i = 0; i < arpOffs.size();)
                {
                    if (arpOffs[i].beat < toBeat)
                    {
                        timed.push_back ({ sampleAt (arpOffs[i].beat), false, arpOffs[i].note, 0.0f });
                        arpOffs[i] = arpOffs.back();
                        arpOffs.pop_back();
                    }
                    else
                    {
                        ++i;
                    }
                }

                arpEvents.clear();
                arp.process (fromBeat, toBeat, arpEvents);

                for (const auto& event : arpEvents)
                {
                    if (timed.size() + 2 > timed.capacity())
                        break;

                    timed.push_back ({ sampleAt (event.startBeat), true, event.note, event.velocity });
                    const auto offBeat = event.startBeat + event.lengthBeats;

                    if (offBeat < toBeat)
                        timed.push_back ({ sampleAt (offBeat), false, event.note, 0.0f });
                    else if (arpOffs.size() < arpOffs.capacity())
                        arpOffs.push_back ({ event.note, offBeat });
                    else
                        timed.push_back ({ position, false, event.note, 0.0f });      // no room to remember it: it ends here
                }

                // On the same sample a note ends before one starts, or a repeated pitch would
                // silence itself the moment it began again.
                std::sort (timed.begin(), timed.end(), [] (const TimedNote& a, const TimedNote& b)
                {
                    return a.sample != b.sample ? a.sample < b.sample : (! a.on && b.on);
                });

                for (const auto& t : timed)
                {
                    renderSpan (left, right, at, t.sample - at);
                    at = t.sample;

                    if (t.on) noteOn (t.note, t.velocity);
                    else      noteOff (t.note);
                }

                renderSpan (left, right, at, position - at);
                at = position;
            };

            for (const auto metadata : incoming)
            {
                const auto position = juce::jlimit (0, numSamples, metadata.samplePosition);
                playTo (position);
                const auto message = metadata.getMessage();

                if (message.isNoteOn())        arp.noteOn (message.getNoteNumber(), message.getFloatVelocity(), blockStart + position * beatsPerSample);
                else if (message.isNoteOff())  arp.noteOff (message.getNoteNumber());
                else                           handle (message);
            }

            playTo (numSamples);
            arpBeat = blockStart + numSamples * beatsPerSample;
        }

        void noteOn (int note, float velocity) noexcept
        {
            keyDown[static_cast<size_t> (note)] = true;

            if (s.lowVelGamma < 0.9999f)
                velocity = std::pow (juce::jlimit (0.0f, 1.0f, velocity), s.lowVelGamma);
            ++order;

            if (s.mode != poly)
            {
                // One voice: a new note glides from where it is; Legato doesn't start the envelopes over.
                auto& voice = voices[0];
                const bool sounding = voice.isActive() && voice.isHeld();

                // Legato and Acid: a note that overlaps the one before carries on from it (Acid's
                // slides at its own time). Acid's other notes start on their pitch, with no glide.
                if (sounding && (s.mode == legato || s.mode == acid))
                    voice.glideTo (note, order, s.mode == acid);
                else
                    voice.start (note, velocity, s.mode == acid ? -1.0 : (voice.isActive() ? voice.getCurrentNote() : lastNote), true, live, order, random);

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

            chosen->start (note, velocity, s.glideSeconds > 0.0 ? lastNote : -1.0, true, live, order, random);
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
                        voice.glideTo (other, ++order, s.mode == acid);
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
        void renderSpan (float* left, float* right, int start, int count) noexcept
        {
            if (count <= 0)
                return;

            // In short pieces, so the LFO moves smoothly whatever the block.
            constexpr int piece = 32;

            for (int at = start; at < start + count; at += piece)
            {
                const auto length = juce::jmin (piece, start + count - at);
                const auto lfoValue = lfoAt (s.lfoShape, lfoPhase, randomValue);
                const auto lfo2Value = lfoAt (lfo2Shape, lfo2Phase, randomValue2);
                lfoNow[0].store (static_cast<float> (lfoValue), std::memory_order_relaxed);
                lfoNow[1].store (static_cast<float> (lfo2Value), std::memory_order_relaxed);

                // Free, or one cycle a note value at the song's tempo.
                const auto tempo = getBlockTempo();
                advanceLfo (lfoPhase, randomValue, lfoSynced[0] ? tempo / 60.0 / lfoBeats[0] : s.lfoHz, length);
                advanceLfo (lfo2Phase, randomValue2, lfoSynced[1] ? tempo / 60.0 / lfoBeats[1] : lfo2Hz, length);

                modulate (lfoValue, lfo2Value);

                // vibrato and tremolo: their own sine
                const auto vibValue = std::sin (twoPi * vibPhase);
                vibPhase += vibHz * length / getSampleRate();
                vibPhase -= std::floor (vibPhase);
                const auto vibrato = (s.vibratoCents + 50.0 * modWheel) * vibValue;

                const Voice* newest = nullptr;

                for (auto& voice : voices)
                {
                    if (voice.isActive())
                    {
                        voice.render (left + at, right + at, length, live, bend * s.bendRange, lfoValue, vibrato, random);
                        if (newest == nullptr || voice.getAge() > newest->getAge()) newest = &voice;
                    }
                }

                shotNow.store (newest != nullptr ? newest->getShot() : 0.0f, std::memory_order_relaxed);

                if (s.tremolo > 0.0f)
                {
                    const auto level = 1.0f - s.tremolo * 0.5f * static_cast<float> (1.0 - vibValue);
                    juce::FloatVectorOperations::multiply (left + at, level, length);

                    if (right != left)
                        juce::FloatVectorOperations::multiply (right + at, level, length);
                }
            }
        }

        void advanceLfo (double& phase, double& randomNow, double hz, int length) noexcept
        {
            phase += hz * length / getSampleRate();

            if (phase >= 1.0)
            {
                phase -= std::floor (phase);
                randomNow = random.nextDouble() * 2.0 - 1.0;
            }
        }

        /** An LFO's wave at its phase: four drawn, the rest read from the wavetables (a gentle
            level of them, 128 harmonics, so an edge doesn't click), -1..1. */
        static double lfoAt (int shape, double phase, double randomNow) noexcept
        {
            if (shape >= firstTableLfo)
                return juce::jlimit (-1.0, 1.0, static_cast<double> (wavetables().read (juce::jmin (shape, caymanwt::numTables - 1), 3, static_cast<float> (phase))) / 0.9);

            switch (shape)
            {
                case lfoTriangle:   return 1.0 - 4.0 * std::abs (phase - 0.5);
                case lfoSquare:     return phase < 0.5 ? 1.0 : -1.0;
                case lfoRandom:     return randomNow;
                case lfoSine:
                default:            return std::sin (twoPi * phase);
            }
        }

        /** The drive's curves, for working out its make-up (the voice has its own copy). */
        static float driveShape (float v, int kind) noexcept
        {
            switch (kind)
            {
                case 1:   return juce::jlimit (-1.0f, 1.0f, v);
                case 2:   return std::sin (v);
                case 3:   return caymanflt::softClip (v + 0.5f) - 0.46581f;
                default:  return caymanflt::softClip (v);
            }
        }

        void finish (float* left, float* right, int numSamples, bool stereoOut) noexcept
        {
            const bool anyEffect = fx.chorusMix > 0.0f || fx.phaserMix > 0.0f || fx.wahMix > 0.0f || fx.vowelMix > 0.0f || fx.gating() || gateLevel < 0.9999f
                                || fx.delayMix > 0.0f || fx.reverbMix > 0.0f
                                || phaserOn || wahOn || vowelOn || delayOn || reverbOn;
            const bool gained = std::abs (outputGain - 1.0f) > 1.0e-6f;

            if (right != left)
            {
                // Both sides were played: the effects, then one side folded in if only one goes
                // out, then the output level.
                if (anyEffect)
                    effects (left, right, numSamples);

                if (! stereoOut)
                    for (int i = 0; i < numSamples; ++i)
                        left[i] = 0.5f * (left[i] + right[i]);

                if (gained)
                {
                    juce::FloatVectorOperations::multiply (left, outputGain, numSamples);

                    if (stereoOut)
                        juce::FloatVectorOperations::multiply (right, outputGain, numSamples);
                }
            }
            else
            {
                // One side only, in a block longer than the spare: the effects in pieces.
                for (int at = 0; at < numSamples; at += static_cast<int> (spare.size()))
                {
                    const auto count = juce::jmin (numSamples - at, static_cast<int> (spare.size()));
                    auto* l = left + at;

                    if (anyEffect)
                    {
                        juce::FloatVectorOperations::copy (spare.data(), l, count);
                        effects (l, spare.data(), count);

                        for (int i = 0; i < count; ++i)
                            l[i] = 0.5f * (l[i] + spare[static_cast<size_t> (i)]);
                    }

                    if (gained)
                        juce::FloatVectorOperations::multiply (l, outputGain, count);
                }
            }

            // The scope's copy.
            auto writeAt = scopeAt.load (std::memory_order_relaxed);

            for (int i = 0; i < numSamples; ++i)
            {
                scope[static_cast<size_t> (writeAt)] = left[i];
                writeAt = (writeAt + 1) % static_cast<int> (scope.size());
            }

            scopeAt.store (writeAt, std::memory_order_release);
        }

        /** The effects, in their order. One that's off (its Mix at 0) is passed by, and cleared
            as it goes off so it doesn't come back later with an old tail. */
        void effects (float* left, float* right, int count) noexcept
        {
            const auto rate = getSampleRate();

            if (fx.chorusMix > 0.0f)
                chorusFx.process (left, right, count, fx.chorusRate, fx.chorusDepth, fx.chorusMix, rate);

            if (fx.phaserMix > 0.0f)
                phaserFx.process (left, right, count, fx.phaserRate, fx.phaserDepth, fx.phaserFeedback, fx.phaserMix, rate);
            else if (phaserOn)
                phaserFx.reset();

            if (fx.wahMix > 0.0f)
                wahFx.process (left, right, count, fx.wahSens, fx.wahRate, fx.wahDepth, fx.wahFreq, fx.wahQ, fx.wahMix, rate);
            else if (wahOn)
                wahFx.reset();

            if (fx.vowelMix > 0.0f)
                vowelFx.process (left, right, count, vowelAt, vowelPerSample, fx.vowelColumns, fx.vowelJump, fx.vowelRes, fx.vowelMix);
            else if (vowelOn)
                vowelFx.reset();

            // The gate: 16 steps, two a column, under the same marker. A closed step turns the sound
            // down by the gate's depth, in 2 ms (back up in 2 ms too, so it chops without clicking).
            if (fx.gating() || gateLevel < 0.9999f)
            {
                const auto speed = 1.0f - std::exp (-1.0f / (0.002f * static_cast<float> (rate)));

                for (int i = 0; i < count; ++i)
                {
                    const auto half = static_cast<long long> (std::floor ((vowelAt + i * vowelPerSample) * 2.0));
                    const auto step = static_cast<int> (((half % 16) + 16) % 16);
                    const auto open = (fx.gateSteps >> step & 1u) != 0 || ! fx.gating();
                    gateLevel += ((open ? 1.0f : 1.0f - fx.gateDepth) - gateLevel) * speed;
                    left[i] *= gateLevel;
                    right[i] *= gateLevel;
                }
            }

            vowelAt += count * vowelPerSample;

            if (fx.delayMix > 0.0f)
            {
                const auto seconds = fx.delaySynced ? fx.delayBeats * 60.0 / getBlockTempo() : static_cast<double> (fx.delaySeconds);
                delayFx.process (left, right, count, static_cast<float> (juce::jlimit (0.001, 4.0, seconds) * rate), fx.delayFeedback, fx.delayMix, fx.delayTone);
            }
            else if (delayOn)
                delayFx.reset();

            if (fx.reverbMix > 0.0f)
                reverbFx.process (left, right, count, fx.reverbSize, fx.reverbDamp, fx.reverbMix, fx.reverbPre, rate);
            else if (reverbOn)
                reverbFx.reset();

            phaserOn = fx.phaserMix > 0.0f;
            wahOn = fx.wahMix > 0.0f;
            vowelOn = fx.vowelMix > 0.0f;
            delayOn = fx.delayMix > 0.0f;
            reverbOn = fx.reverbMix > 0.0f;
        }

        /** The effects' settings (read from the parameters when one changes). */
        struct Effects
        {
            float chorusRate = 0.25f, chorusDepth = 0.6f, chorusMix = 0.0f;
            float phaserRate = 0.1f, phaserDepth = 0.8f, phaserFeedback = 0.5f, phaserMix = 0.0f;
            float wahSens = 0.5f, wahFreq = 400.0f, wahQ = 0.6f, wahRate = 1.0f, wahDepth = 0.0f, wahMix = 0.0f;
            float vowelRes = 0.5f, vowelMix = 0.0f, gateDepth = 1.0f;
            std::array<int, 8> vowelColumns { 0, 1, 2, 3, 4, 3, 2, 1 };
            bool vowelJump = true;
            double vowelBeats = 1.0;
            uint32_t gateSteps = 0xffffu;

            bool gating() const noexcept { return gateDepth > 0.0f && gateSteps != 0xffffu; }
            float delaySeconds = 0.375f, delayFeedback = 0.45f, delayTone = 5000.0f, delayMix = 0.0f, delayBeats = 0.75f;
            bool delaySynced = false;
            float reverbSize = 0.5f, reverbDamp = 0.5f, reverbPre = 0.1f, reverbMix = 0.0f;
        };

        std::array<Voice, maxVoices> voices;
        std::array<bool, 128> keyDown {};
        Settings s, live;              // the knobs' settings, and those playing now (the matrix's on top)
        juce::MidiKeyboardState keyboard;
        juce::MidiBuffer incoming;
        juce::Random random;
        // the arpeggiator
        struct TimedNote { int sample; bool on; int note; float velocity; };
        struct PendingOff { int note; double beat; };
        fanan::SeededArpCore arp;
        std::vector<fanan::ArpEvent> arpEvents;
        std::vector<PendingOff> arpOffs;       // the notes it started that end in a later block
        std::vector<TimedNote> timed;
        double arpBeat = 0.0;                  // where its clock stands, in beats
        int arpBars = 4;
        bool arpIsOn = false, arpWasOn = false, arpHolds = false;

        GrexFx::ChorusFx chorusFx;
        GrexFx::PhaserFx phaserFx;
        GrexFx::AutoWahFx wahFx;
        VowelFilter vowelFx;
        PingPongDelay delayFx;
        GrexFx::ReverbFx reverbFx;
        Effects fx;
        bool phaserOn = false, wahOn = false, vowelOn = false, delayOn = false, reverbOn = false;
        double vowelAt = 0.0, vowelPerSample = 0.0, vowelClock = 0.0;   // the vowel's marker, in columns
        float gateLevel = 1.0f;
        std::atomic<float> vowelShown { 0.0f };
        std::vector<float> spare;      // the right side when only one channel goes out
        std::vector<float> scope;
        std::atomic<int> scopeAt { 0 };
        juce::uint64 order = 0;
        double lastNote = -1.0, bend = 0.0, modWheel = 0.0, lfoPhase = 0.0, randomValue = 0.0;

        // the second LFO, sync, the matrix
        int lfo2Shape = lfoTriangle;
        double lfo2Hz = 0.5, lfo2Phase = 0.0, randomValue2 = 0.0;
        bool lfoSynced[2] { false, false };
        double lfoBeats[2] { 1.0, 4.0 };
        double depth[2] { 0.5, 0.5 };
        bool routeOn[2][SynthParam::numDestinations] {};
        bool routed = false;
        std::array<const Spec*, SynthParam::numDestinations> destinationSpecs {};
        std::atomic<float> lfoNow[2] { 0.0f, 0.0f };
        double vibHz = 5.0, vibPhase = 0.0;      // the vibration's own sine
        std::atomic<float> shotNow { 0.0f };
        // the panel's wheels: where they were moved to (not yet sent in), and where the synth's are
        static constexpr float wheelUnmoved = -100.0f;
        std::atomic<float> panelBendIn { wheelUnmoved }, panelModIn { wheelUnmoved }, bendNow { 0.0f }, modNow { 0.0f };
        bool sustainDown = false;
        static constexpr float myPluginLevel = 0.5f / 0.14f;
        float outputGain = 0.5f * myPluginLevel;
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
