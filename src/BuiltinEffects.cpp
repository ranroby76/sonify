// D:\workspace\Fanan_web_hub\src\BuiltinEffects.cpp

#include "BuiltinEffects.h"
#include "BuiltinPlugins.h"
#include "GrexFx.h"
#include "SonifySpace.h"

#include <cmath>
#include <complex>

namespace
{
    Plugin::Descriptor descriptorFor (const char* id, const char* name, const juce::StringArray& features)
    {
        Plugin::Descriptor descriptor;
        descriptor.id = id;
        descriptor.name = name;
        descriptor.vendor = "Sonify";
        descriptor.version = "1.0";
        descriptor.features = features;
        return descriptor;
    }

    float percent (double value) noexcept   { return static_cast<float> (value / 100.0); }

    /** A knob in Hz as the Grex effects take it: 0..1 on 20 * 1000^x (20 Hz to 20 kHz). */
    float hzToNorm (double hz) noexcept     { return static_cast<float> (std::log (juce::jmax (20.0, hz) / 20.0) / std::log (1000.0)); }

    //==============================================================================
    // Sonify Chorus: Grex's BBD chorus, voiced like a Juno's.
    namespace ChorusParam   { enum : uint32_t { rate = 1, depth, mix, output }; }

    class ChorusPlugin final : public BuiltinPlugin
    {
    public:
        ChorusPlugin()
            : BuiltinPlugin (descriptorFor (Builtins::chorusId, "Sonify Chorus", { "audio-effect", "chorus", "stereo" }),
                             {
                                 { ChorusParam::rate,   "Rate",   0.15,  3.0,  0.6, "Hz", 2 },
                                 { ChorusParam::depth,  "Depth",  0.0, 100.0, 50.0, "%",  0 },
                                 { ChorusParam::mix,    "Mix",    0.0, 100.0, 60.0, "%",  0 },
                                 { ChorusParam::output, "Output", -12.0, 12.0,  0.0, "dB", 1 }
                             })
        {
        }

        const char* getPanelName() const noexcept override   { return "chorus"; }

    private:
        void prepareAudio (double rate, int) override   { chorus.prepare (rate); }
        void update() noexcept override
        {
            rate01 = static_cast<float> ((valueOf (ChorusParam::rate) - 0.15) / (3.0 - 0.15));
            depth01 = percent (valueOf (ChorusParam::depth));
            mix01 = percent (valueOf (ChorusParam::mix));
            output = static_cast<float> (juce::Decibels::decibelsToGain (valueOf (ChorusParam::output)));
        }

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            chorus.process (left, right, numSamples, rate01, depth01, mix01, getSampleRate());
            applyGain (left, right, numSamples, output);
        }

        static void applyGain (float* left, float* right, int n, float gain) noexcept
        {
            if (std::abs (gain - 1.0f) < 1.0e-6f)
                return;

            juce::FloatVectorOperations::multiply (left, gain, n);
            juce::FloatVectorOperations::multiply (right, gain, n);
        }

        GrexFx::ChorusFx chorus;
        float rate01 = 0.0f, depth01 = 0.5f, mix01 = 0.6f, output = 1.0f;
    };

    //==============================================================================
    // Sonify Phaser: four all-pass stages swept by an LFO, with feedback.
    namespace PhaserParam   { enum : uint32_t { rate = 1, depth, feedback, mix }; }

    class PhaserPlugin final : public BuiltinPlugin
    {
    public:
        PhaserPlugin()
            : BuiltinPlugin (descriptorFor (Builtins::phaserId, "Sonify Phaser", { "audio-effect", "phaser", "stereo" }),
                             {
                                 { PhaserParam::rate,     "Rate",     0.05, 4.0,  0.5, "Hz", 2 },
                                 { PhaserParam::depth,    "Depth",    0.0, 100.0, 80.0, "%", 0 },
                                 { PhaserParam::feedback, "Feedback", 0.0,  90.0, 50.0, "%", 0 },
                                 { PhaserParam::mix,      "Mix",      0.0, 100.0, 50.0, "%", 0 }
                             })
        {
        }

        const char* getPanelName() const noexcept override   { return "phaser"; }

    private:
        void prepareAudio (double rate, int) override   { phaser.prepare (rate); }
        void update() noexcept override
        {
            rate01 = static_cast<float> ((valueOf (PhaserParam::rate) - 0.05) / (4.0 - 0.05));
            depth01 = percent (valueOf (PhaserParam::depth));
            feedback01 = percent (valueOf (PhaserParam::feedback));
            mix01 = percent (valueOf (PhaserParam::mix));
        }

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            phaser.process (left, right, numSamples, rate01, depth01, feedback01, mix01, getSampleRate());
        }

        GrexFx::PhaserFx phaser;
        float rate01 = 0.1f, depth01 = 0.8f, feedback01 = 0.5f, mix01 = 0.5f;
    };

    //==============================================================================
    // Sonify Auto-Wah: a resonant band-pass that the playing (and an LFO) sweeps.
    namespace WahParam   { enum : uint32_t { sensitivity = 1, frequency, resonance, rate, lfoDepth, mix }; }

    class AutoWahPlugin final : public BuiltinPlugin
    {
    public:
        AutoWahPlugin()
            : BuiltinPlugin (descriptorFor (Builtins::autoWahId, "Sonify Auto-Wah", { "audio-effect", "filter", "stereo" }),
                             {
                                 { WahParam::sensitivity, "Sensitivity", 0.0,  100.0,  50.0, "%",  0 },
                                 { WahParam::frequency,   "Frequency",  80.0, 1500.0, 400.0, "Hz", 0 },
                                 { WahParam::resonance,   "Resonance",   0.0,  100.0,  60.0, "%",  0 },
                                 { WahParam::rate,        "LFO Rate",    0.05,   8.0,   1.0, "Hz", 2 },
                                 { WahParam::lfoDepth,    "LFO Depth",   0.0,  100.0,   0.0, "%",  0 },
                                 { WahParam::mix,         "Mix",         0.0,  100.0, 100.0, "%",  0 }
                             })
        {
        }

        const char* getPanelName() const noexcept override   { return "autowah"; }

    private:
        void prepareAudio (double rate, int) override   { wah.prepare (rate); }
        void update() noexcept override {}

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            wah.process (left, right, numSamples, percent (valueOf (WahParam::sensitivity)), static_cast<float> (valueOf (WahParam::rate)),
                         percent (valueOf (WahParam::lfoDepth)), static_cast<float> (valueOf (WahParam::frequency)),
                         percent (valueOf (WahParam::resonance)), percent (valueOf (WahParam::mix)), getSampleRate());
        }

        GrexFx::AutoWahFx wah;
    };

    //==============================================================================
    // Sonify Delay: ping-pong, in time with the song (a note value) or in milliseconds.
    namespace DelayParam   { enum : uint32_t { sync = 1, division, time, feedback, damping, lowCut, dry, wet }; }

    // The note values, in quarter notes: 1/1, 1/2, 1/4, 1/8, 1/16, dotted 1/4 and 1/8, triplet 1/4 and 1/8.
    constexpr double divisions[] { 4.0, 2.0, 1.0, 0.5, 0.25, 1.5, 0.75, 2.0 / 3.0, 1.0 / 3.0 };

    class DelayPlugin final : public BuiltinPlugin
    {
    public:
        DelayPlugin()
            : BuiltinPlugin (descriptorFor (Builtins::delayId, "Sonify Delay", { "audio-effect", "delay", "stereo" }),
                             {
                                 { DelayParam::sync,     "Sync",       0.0,     1.0,    1.0, "",   0, true, false, true, nullptr, { "Free", "Song" } },
                                 { DelayParam::division, "Note",       0.0,     8.0,    6.0, "",   0, true, false, true, nullptr,
                                   { "1/1", "1/2", "1/4", "1/8", "1/16", "1/4 dotted", "1/8 dotted", "1/4 triplet", "1/8 triplet" } },
                                 { DelayParam::time,     "Time",      10.0,  2400.0,  375.0, "ms", 0 },
                                 { DelayParam::feedback, "Feedback",   0.0,    95.0,   45.0, "%",  0 },
                                 { DelayParam::damping,  "Damping",  500.0, 20000.0, 5000.0, "Hz", 0 },
                                 { DelayParam::lowCut,   "Low Cut",   20.0,  1000.0,   20.0, "Hz", 0, false, false, true, "min" },
                                 { DelayParam::dry,      "Dry",        0.0,   100.0,  100.0, "%",  0 },
                                 { DelayParam::wet,      "Wet",        0.0,   100.0,   35.0, "%",  0 }
                             })
        {
        }

        const char* getPanelName() const noexcept override   { return "delay"; }

    private:
        void prepareAudio (double rate, int) override   { delay.prepare (rate); }
        void update() noexcept override {}

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            const auto rate = getSampleRate();
            double seconds = valueOf (DelayParam::time) / 1000.0;

            if (valueOf (DelayParam::sync) > 0.5)
            {
                const auto index = juce::jlimit (0, 8, static_cast<int> (std::lround (valueOf (DelayParam::division))));
                seconds = divisions[index] * 60.0 / getBlockTempo();
            }

            delay.process (left, right, numSamples, static_cast<float> (juce::jlimit (0.001, 2.4, seconds) * rate),
                           percent (valueOf (DelayParam::feedback)), percent (valueOf (DelayParam::wet)), rate,
                           percent (valueOf (DelayParam::dry)), static_cast<float> (valueOf (DelayParam::damping)),
                           static_cast<float> (valueOf (DelayParam::lowCut)));
        }

        GrexFx::StereoDelayFx delay;
    };

    //==============================================================================
    /** Two ducking bells' response in dB at hz (a bell subtracts a band-pass: 1 - a * bp), each
        as deep as it dips now: the Ducker's curve, and Space Lifter's. */
    double duckBellsDb (const std::array<SonifyDsp::Biquad, 2>& bells, const std::array<double, 2>& amounts, double hz, double rate)
    {
        double db = 0.0;
        const auto w = 2.0 * juce::MathConstants<double>::pi * hz / rate;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);

        for (size_t band = 0; band < 2; ++band)
        {
            if (amounts[band] <= 0.0)
                continue;

            const auto& f = bells[band];
            const auto bp = (f.b0 + f.b1 * z1 + f.b2 * z2) / (1.0 + f.a1 * z1 + f.a2 * z2);
            db += 20.0 * std::log10 (juce::jmax (1.0e-6, std::abs (1.0 - amounts[band] * bp)));
        }

        return db;
    }

    //==============================================================================
    // Space Lifter 4 (reverb-echo; Space Lifter 3's successor): an echo and a reverb blended into one space (SonifySpace.h),
    // in one of four routings, with a two-band ducker on the wet alone (keyed by the dry or by
    // the sidechain). It took Sonify Reverb's place: its id, and its ten parameters first, so
    // the songs made with Sonify Reverb load into it (as its hall, without the echo).
    namespace SpaceParam
    {
        enum : uint32_t
        {
            size = 1, decay, damping, preDelay, lowCut, highCut, early, earlySize, dry, wet,
            routing, blend, voicing, shape, shapeTime, colour, texture, reverbOn,
            echoOn, mode, style, sync, note, time, time2, feedback, groove, feel, accent, drive,
            mod, freeze, width,
            tap1, tap16 = tap1 + 15,
            duckOn, duckKey, duckTarget,
            duckLow,                       // its eight: on, freq, Q, depth, threshold, ratio, attack, release
            duckHigh = duckLow + 8
        };
    }

    std::vector<BuiltinPlugin::Spec> spaceSpecs()
    {
        using P = BuiltinPlugin::Spec;
        static const char* tapNames[16] { "Tap 1", "Tap 2", "Tap 3", "Tap 4", "Tap 5", "Tap 6", "Tap 7", "Tap 8",
                                          "Tap 9", "Tap 10", "Tap 11", "Tap 12", "Tap 13", "Tap 14", "Tap 15", "Tap 16" };
        static constexpr double tapDefaults[16] { 0, 0, 100, 0, 0, 75, 0, 55, 0, 0, 40, 0, 0, 30, 0, 20 };

        std::vector<P> specs {
            { SpaceParam::size,       "Size",        0.0,   100.0,    50.0, "%",  0 },
            { SpaceParam::decay,      "Decay",       0.0,   100.0,    50.0, "%",  0 },
            { SpaceParam::damping,    "Damping",     0.0,   100.0,    40.0, "%",  0 },
            { SpaceParam::preDelay,   "Pre-Delay",   0.0,   250.0,    10.0, "ms", 0 },
            { SpaceParam::lowCut,     "Low Cut",    20.0,  1000.0,   120.0, "Hz", 0, false, false, true, "min" },
            { SpaceParam::highCut,    "High Cut", 1000.0, 20000.0, 20000.0, "Hz", 0, false, false, true, "max" },
            { SpaceParam::early,      "Early",       0.0,   100.0,     0.0, "%",  0 },
            { SpaceParam::earlySize,  "Early Size",  0.0,   100.0,    50.0, "%",  0 },
            { SpaceParam::dry,        "Dry",         0.0,   100.0,   100.0, "%",  0 },
            { SpaceParam::wet,        "Wet",         0.0,   100.0,    30.0, "%",  0 },

            { SpaceParam::routing,    "Routing",     0.0,     3.0,     0.0, "",   0, true, false, true, nullptr,
              { "Echo > Reverb", "Reverb in Echo", "Reverb > Echo", "Parallel" } },
            { SpaceParam::blend,      "Blend",       0.0,   100.0,    50.0, "%",  0 },
            { SpaceParam::voicing,    "Voicing",     0.0,     7.0,     0.0, "",   0, true, false, true, nullptr,
              { "Plate", "Hall", "Room", "Chamber", "Cathedral", "Spring", "Vintage Digital", "Shimmer" } },
            { SpaceParam::shape,      "Shape",       0.0,     3.0,     0.0, "",   0, true, false, true, nullptr,
              { "Decay", "Gate", "Reverse", "Bloom" } },
            { SpaceParam::shapeTime,  "Shape Time", 20.0,  2000.0,   350.0, "ms", 0 },
            { SpaceParam::colour,     "Color",    -100.0,   100.0,     0.0, "%",  0 },
            { SpaceParam::texture,    "Texture",     0.0,   100.0,    70.0, "%",  0 },
            { SpaceParam::reverbOn,   "Reverb On",   0.0,     1.0,     1.0, "",   0, true, false, true, nullptr, { "Off", "On" } },

            { SpaceParam::echoOn,     "Echo On",     0.0,     1.0,     1.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
            { SpaceParam::mode,       "Mode",        0.0,     3.0,     0.0, "",   0, true, false, true, nullptr,
              { "Single", "Dual", "Ping-Pong", "Rhythm" } },
            { SpaceParam::style,      "Style",       0.0,     4.0,     0.0, "",   0, true, false, true, nullptr,
              { "Tape", "BBD", "Oil Can", "Clean Digital", "Vintage Digital" } },
            { SpaceParam::sync,       "Sync",        0.0,     1.0,     1.0, "",   0, true, false, true, nullptr, { "Free", "Song" } },
            { SpaceParam::note,       "Note",        0.0,     8.0,     6.0, "",   0, true, false, true, nullptr,
              { "1/1", "1/2", "1/4", "1/8", "1/16", "1/4 dotted", "1/8 dotted", "1/4 triplet", "1/8 triplet" } },
            { SpaceParam::time,       "Time",       10.0,  2400.0,   375.0, "ms", 0 },
            { SpaceParam::time2,      "Echo 2",     25.0,   200.0,    75.0, "%",  0 },
            { SpaceParam::feedback,   "Feedback",    0.0,   100.0,    30.0, "%",  0 },
            { SpaceParam::groove,     "Groove",    -50.0,    50.0,     0.0, "%",  0 },
            { SpaceParam::feel,       "Feel",      -50.0,    50.0,     0.0, "ms", 0 },
            { SpaceParam::accent,     "Accent",   -100.0,   100.0,     0.0, "%",  0 },
            { SpaceParam::drive,      "Drive",       0.0,   100.0,    30.0, "%",  0 },

            { SpaceParam::mod,        "Mod",         0.0,   100.0,    25.0, "%",  0 },
            { SpaceParam::freeze,     "Freeze",      0.0,     1.0,     0.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
            { SpaceParam::width,      "Width",       0.0,   200.0,   100.0, "%",  0 }
        };

        for (uint32_t i = 0; i < 16; ++i)
            specs.push_back ({ SpaceParam::tap1 + i, tapNames[i], 0.0, 100.0, tapDefaults[i], "%", 0 });

        specs.push_back ({ SpaceParam::duckOn,     "Duck On",     0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Off", "On" } });
        specs.push_back ({ SpaceParam::duckKey,    "Duck Key",    0.0, 1.0, 0.0, "", 0, true, false, true, nullptr, { "Self", "Sidechain" } });
        specs.push_back ({ SpaceParam::duckTarget, "Duck Target", 0.0, 2.0, 0.0, "", 0, true, false, true, nullptr, { "Both", "Echo", "Reverb" } });

        // The two bands, as Sonify Ducker's (wide by default: the whole wet dips under the playing).
        struct Band { uint32_t base; const char* names[8]; double freq, q; };
        static const Band bands[2] {
            { SpaceParam::duckLow,  { "Duck Low On", "Duck Low Freq", "Duck Low Q", "Duck Low Depth", "Duck Low Threshold",
                                      "Duck Low Ratio", "Duck Low Attack", "Duck Low Release" }, 300.0, 0.5 },
            { SpaceParam::duckHigh, { "Duck High On", "Duck High Freq", "Duck High Q", "Duck High Depth", "Duck High Threshold",
                                      "Duck High Ratio", "Duck High Attack", "Duck High Release" }, 2800.0, 0.5 }
        };

        for (const auto& band : bands)
        {
            specs.push_back ({ band.base,     band.names[0], 0.0, 1.0, 1.0, "", 0, true, false, true, nullptr, { "Off", "On" } });
            specs.push_back ({ band.base + 1, band.names[1], 40.0, 16000.0, band.freq, "Hz", 0 });
            specs.push_back ({ band.base + 2, band.names[2], 0.3, 8.0, band.q, "", 2 });
            specs.push_back ({ band.base + 3, band.names[3], 0.0, 24.0, 12.0, "dB", 1 });
            specs.push_back ({ band.base + 4, band.names[4], -60.0, 0.0, -36.0, "dB", 1 });
            specs.push_back ({ band.base + 5, band.names[5], 1.0, 20.0, 4.0, ":1", 1 });
            specs.push_back ({ band.base + 6, band.names[6], 0.5, 200.0, 5.0, "ms", 1 });
            specs.push_back ({ band.base + 7, band.names[7], 20.0, 1000.0, 260.0, "ms", 0 });
        }

        return specs;
    }

    class SpacePlugin final : public BuiltinPlugin
    {
    public:
        SpacePlugin()
            : BuiltinPlugin (descriptorFor (Builtins::reverbId, "Space Lifter 4", { "audio-effect", "reverb", "delay", "stereo" }),
                             spaceSpecs())
        {
            ducker.setEnabled (true);
        }

        bool hasSidechain() const noexcept override           { return true; }
        const char* getPanelName() const noexcept override   { return "reverb"; }
        juce::StringArray getDisplaySources() const override  { return { "space.gr1", "space.gr2", "space.key1", "space.key2" }; }
        juce::StringArray getCurveSources() const override    { return { "space.curve" }; }

        float readSource (const juce::String& source) const noexcept override
        {
            if (source == "space.gr1" || source == "space.gr2")
                return ducking ? juce::jlimit (0.0f, 1.0f, ducker.getGainReductionDb (source.getLastCharacter() == '1' ? 0 : 1) / 24.0f) : 0.0f;

            if (source == "space.key1" || source == "space.key2")
                return ducking ? juce::jlimit (0.0f, 1.0f, (ducker.getSidechainLevelDb (source.getLastCharacter() == '1' ? 0 : 1) + 60.0f) / 60.0f) : 0.0f;

            return BuiltinPlugin::readSource (source);
        }

        void readCurve (const juce::String& source, float* destination, int numSamples) override
        {
            if (source != "space.curve")
                return BuiltinPlugin::readCurve (source, destination, numSamples);

            constexpr double rate = 48000.0;
            std::array<SonifyDsp::Biquad, 2> bells {};
            std::array<double, 2> amounts {};

            for (int band = 0; band < 2; ++band)
            {
                const auto base = band == 0 ? SpaceParam::duckLow : SpaceParam::duckHigh;

                if (! ducking || valueOf (base) < 0.5)
                    continue;

                bells[static_cast<size_t> (band)] = SonifyDsp::Biquad::bandPass (rate, valueOf (base + 1), valueOf (base + 2));
                amounts[static_cast<size_t> (band)] = 1.0 - juce::Decibels::decibelsToGain (-static_cast<double> (ducker.getGainReductionDb (band)));
            }

            drawResponse (destination, numSamples, 24.0, [&] (double hz) { return duckBellsDb (bells, amounts, hz, rate); });
        }

        /** Kept with the song: that it's Space Lifter 4's (a song without it is Sonify Reverb's). */
        juce::var saveExtra() const override
        {
            auto* extra = new juce::DynamicObject();
            extra->setProperty ("space", 1);
            return juce::var (extra);
        }

        void loadExtra (const juce::var& extra) override
        {
            if (extra.isObject() && extra.hasProperty ("space"))
                return;

            // A Sonify Reverb song: its hall alone, as it was.
            setValue (SpaceParam::voicing, 1.0);
            setValue (SpaceParam::echoOn, 0.0);
            setValue (SpaceParam::routing, 3.0);
            setValue (SpaceParam::blend, 100.0);
            setValue (SpaceParam::texture, 70.0);
            setValue (SpaceParam::mod, 20.0);
        }

    private:
        void prepareAudio (double rate, int maxBlockSize) override
        {
            chunk = juce::jmax (32, maxBlockSize);
            scratch.assign (static_cast<size_t> (chunk) * 7, 0.0f);
            space.prepare (rate);
            tone.prepare (rate);
            ducker.prepare (rate);
            prepareSidechain (maxBlockSize);
            dryGain = static_cast<float> (valueOf (SpaceParam::dry) / 100.0);
            wetGain = static_cast<float> (valueOf (SpaceParam::wet) / 100.0);
            fresh = true;
        }

        static int choice (double value) noexcept   { return static_cast<int> (std::lround (value)); }

        double echoMs() const noexcept
        {
            if (valueOf (SpaceParam::sync) > 0.5)
                return divisions[juce::jlimit (0, 8, choice (valueOf (SpaceParam::note)))] * 60000.0 / juce::jmax (20.0, getBlockTempo());

            return valueOf (SpaceParam::time);
        }

        void update() noexcept override
        {
            using namespace SonifySpace;
            Settings s;
            s.routing = static_cast<Routing> (juce::jlimit (0, 3, choice (valueOf (SpaceParam::routing))));
            s.blend = percent (valueOf (SpaceParam::blend));
            s.echoOn = valueOf (SpaceParam::echoOn) > 0.5;
            s.reverbOn = valueOf (SpaceParam::reverbOn) > 0.5;

            const auto frozen = valueOf (SpaceParam::freeze) > 0.5;
            const auto mod = percent (valueOf (SpaceParam::mod));

            auto& r = s.reverb;
            r.voicing = static_cast<Voicing> (juce::jlimit (0, numVoicings - 1, choice (valueOf (SpaceParam::voicing))));
            r.size = percent (valueOf (SpaceParam::size));
            r.decay = percent (valueOf (SpaceParam::decay));
            r.damping = percent (valueOf (SpaceParam::damping));
            r.preDelayMs = static_cast<float> (valueOf (SpaceParam::preDelay));
            r.early = percent (valueOf (SpaceParam::early));
            r.earlySize = percent (valueOf (SpaceParam::earlySize));
            r.colour = percent (valueOf (SpaceParam::colour));
            r.texture = percent (valueOf (SpaceParam::texture));
            r.mod = mod;
            r.shape = static_cast<Shape> (juce::jlimit (0, 3, choice (valueOf (SpaceParam::shape))));
            r.shapeMs = static_cast<float> (valueOf (SpaceParam::shapeTime));
            r.freeze = frozen;

            auto& e = s.echo;
            e.mode = static_cast<EchoMode> (juce::jlimit (0, 3, choice (valueOf (SpaceParam::mode))));
            e.style = static_cast<EchoStyle> (juce::jlimit (0, 4, choice (valueOf (SpaceParam::style))));
            e.timeMs = static_cast<float> (echoMs());
            e.time2 = percent (valueOf (SpaceParam::time2));
            e.feedback = percent (valueOf (SpaceParam::feedback));
            e.groove = percent (valueOf (SpaceParam::groove));
            e.feelMs = static_cast<float> (valueOf (SpaceParam::feel));
            e.accent = percent (valueOf (SpaceParam::accent));
            e.drive = percent (valueOf (SpaceParam::drive));
            e.mod = mod;
            e.freeze = frozen;

            for (uint32_t i = 0; i < 16; ++i)
                e.taps[i] = percent (valueOf (SpaceParam::tap1 + i));

            // Back on: what it held when it went off isn't heard again.
            if (s.echoOn && ! space.getSettings().echoOn)
                space.resetEcho();

            if (s.reverbOn && ! space.getSettings().reverbOn)
                space.resetReverb();

            space.set (s, fresh);     // its first settings: at once (no glide from the defaults)
            fresh = false;
            tone.set (static_cast<float> (valueOf (SpaceParam::lowCut)), static_cast<float> (valueOf (SpaceParam::highCut)),
                      percent (valueOf (SpaceParam::width)));

            ducking = valueOf (SpaceParam::duckOn) > 0.5;
            fromSidechain = valueOf (SpaceParam::duckKey) > 0.5;
            target = juce::jlimit (0, 2, choice (valueOf (SpaceParam::duckTarget)));

            for (int band = 0; band < 2; ++band)
            {
                const auto base = band == 0 ? SpaceParam::duckLow : SpaceParam::duckHigh;
                ducker.setBandEnabled (band, valueOf (base) > 0.5);
                ducker.setFreq (band, static_cast<float> (valueOf (base + 1)));
                ducker.setQ (band, static_cast<float> (valueOf (base + 2)));
                ducker.setDepthDb (band, static_cast<float> (valueOf (base + 3)));
                ducker.setThresholdDb (band, static_cast<float> (valueOf (base + 4)));
                ducker.setRatio (band, static_cast<float> (valueOf (base + 5)));
                ducker.setAttackMs (band, static_cast<float> (valueOf (base + 6)));
                ducker.setReleaseMs (band, static_cast<float> (valueOf (base + 7)));
            }
        }

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            const juce::ScopedNoDenormals noDenormals;

            // In time with the song: the tempo can move every block.
            space.setEchoTime (static_cast<float> (echoMs()));

            const auto sidechainReady = getSidechainCapacity() >= numSamples;

            for (int start = 0; start < numSamples; start += chunk)
                processChunk (left + start, right + start, start, juce::jmin (chunk, numSamples - start), sidechainReady);
        }

        void processChunk (float* left, float* right, int start, int n, bool sidechainReady) noexcept
        {
            const auto size = static_cast<size_t> (chunk);
            float* dryL = scratch.data();
            float* dryR = dryL + size;
            float* echoL = dryR + size;
            float* echoR = echoL + size;
            float* reverbL = echoR + size;
            float* reverbR = reverbL + size;
            float* silence = reverbR + size;

            std::copy (left, left + n, dryL);
            std::copy (right, right + n, dryR);

            space.process (dryL, dryR, echoL, echoR, reverbL, reverbR, n);

            const auto add = [n] (float* intoL, float* intoR, const float* l, const float* r)
            {
                for (int i = 0; i < n; ++i)
                {
                    intoL[i] += l[i];
                    intoR[i] += r[i];
                }
            };

            if (ducking)
            {
                // The key: the dry itself, or what the sidechain brings (silent while nothing sends to it).
                float* keyL = dryL;
                float* keyR = dryR;

                if (fromSidechain)
                {
                    keyL = keyR = silence;
                    std::fill (silence, silence + n, 0.0f);

                    if (sidechainReady && getSidechainChannel (0) != nullptr)
                    {
                        keyL = getSidechainChannel (0) + start;
                        keyR = getSidechainChannel (1) != nullptr ? getSidechainChannel (1) + start : keyL;
                    }
                }

                const auto duck = [&] (float* l, float* r)
                {
                    float* styleChannels[] { l, r };
                    juce::AudioBuffer<float> style (styleChannels, 2, n);
                    float* keyChannels[] { keyL, keyR };
                    const juce::AudioBuffer<float> key (keyChannels, 2, n);
                    ducker.process (style, key);
                };

                if (target == 0)
                {
                    add (echoL, echoR, reverbL, reverbR);
                    duck (echoL, echoR);
                }
                else
                {
                    duck (target == 1 ? echoL : reverbL, target == 1 ? echoR : reverbR);
                    add (echoL, echoR, reverbL, reverbR);
                }
            }
            else
            {
                add (echoL, echoR, reverbL, reverbR);
            }

            tone.process (echoL, echoR, n);

            // Dry and wet apart: the source stays where it is.
            const auto dryTo = static_cast<float> (valueOf (SpaceParam::dry) / 100.0);
            const auto wetTo = static_cast<float> (valueOf (SpaceParam::wet) / 100.0);
            const auto dryStep = (dryTo - dryGain) / static_cast<float> (n);
            const auto wetStep = (wetTo - wetGain) / static_cast<float> (n);

            for (int i = 0; i < n; ++i)
            {
                dryGain += dryStep;
                wetGain += wetStep;
                left[i] = dryGain * dryL[i] + wetGain * echoL[i];
                right[i] = dryGain * dryR[i] + wetGain * echoR[i];
            }

            dryGain = dryTo;
            wetGain = wetTo;
        }

        SonifySpace::Space space;
        SonifySpace::WetTone tone;
        GrexFx::StyleDucker ducker;
        std::vector<float> scratch;
        int chunk = 512, target = 0;
        bool ducking = false, fromSidechain = false, fresh = true;
        float dryGain = 1.0f, wetGain = 0.3f;
    };

    //==============================================================================
    // Sonify Sweetener: Grex's four-stage dynamics. Soften (the attack, softer or sharper), Peak
    // (the hits that poke out of their body), Tame (brightness ducked when it bites), Round
    // (warm saturation).
    namespace SweetParam
    {
        enum : uint32_t { mix = 1, softenOn, softenDepth, softenTime, peakOn, peakCeiling, peakRatio,
                          tameOn, tameDepth, tameFreq, roundOn, roundDrive, roundMix };
    }

    class SweetenerPlugin final : public BuiltinPlugin
    {
    public:
        SweetenerPlugin()
            : BuiltinPlugin (descriptorFor (Builtins::sweetenerId, "Sonify Sweetener", { "audio-effect", "compressor", "distortion", "stereo" }),
                             {
                                 { SweetParam::mix,         "Mix",           0.0,  100.0,  100.0, "%",  0 },
                                 { SweetParam::softenOn,    "Soften",        0.0,    1.0,    1.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 { SweetParam::softenDepth, "Soften Amount", -100.0, 100.0,  35.0, "%",  0 },
                                 { SweetParam::softenTime,  "Soften Time",   1.0,  150.0,    8.0, "ms", 0 },
                                 { SweetParam::peakOn,      "Peak",          0.0,    1.0,    1.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 { SweetParam::peakCeiling, "Peak Ceiling",  3.0,   24.0,   12.0, "dB", 1 },
                                 { SweetParam::peakRatio,   "Peak Ratio",    1.0,   20.0,    4.0, ":1", 1 },
                                 { SweetParam::tameOn,      "Tame",          0.0,    1.0,    1.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 { SweetParam::tameDepth,   "Tame Depth",    0.0,   12.0,    4.0, "dB", 1 },
                                 { SweetParam::tameFreq,    "Tame Freq",  1500.0, 8000.0, 4000.0, "Hz", 0 },
                                 { SweetParam::roundOn,     "Round",         0.0,    1.0,    1.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 { SweetParam::roundDrive,  "Round Drive",   0.0,  100.0,   25.0, "%",  0 },
                                 { SweetParam::roundMix,    "Round Mix",     0.0,  100.0,  100.0, "%",  0 }
                             })
        {
        }

        const char* getPanelName() const noexcept override   { return "sweetener"; }
        juce::StringArray getDisplaySources() const override  { return { "sweet.soften", "sweet.peak", "sweet.tame", "sweet.round" }; }

        float readSource (const juce::String& source) const noexcept override
        {
            // Each stage's work, 0 to 12 dB.
            const auto shown = [] (float db) { return juce::jlimit (0.0f, 1.0f, std::abs (db) / 12.0f); };

            if (source == "sweet.soften")   return shown (sweetener.getSoftenGrDb());
            if (source == "sweet.peak")     return shown (sweetener.getPeakGrDb());
            if (source == "sweet.tame")     return shown (sweetener.getTameGrDb());
            if (source == "sweet.round")    return shown (sweetener.getRoundGrDb());
            return BuiltinPlugin::readSource (source);
        }

    private:
        void prepareAudio (double rate, int) override   { sweetener.prepare (rate); }

        void update() noexcept override
        {
            params.enabled = true;
            params.mix = percent (valueOf (SweetParam::mix));
            params.softenOn = valueOf (SweetParam::softenOn) > 0.5;
            params.softenDepth = percent (valueOf (SweetParam::softenDepth));
            params.softenMs = static_cast<float> (valueOf (SweetParam::softenTime));
            params.peakOn = valueOf (SweetParam::peakOn) > 0.5;
            params.peakCeilDb = static_cast<float> (valueOf (SweetParam::peakCeiling));
            params.peakRatio = static_cast<float> (valueOf (SweetParam::peakRatio));
            params.tameOn = valueOf (SweetParam::tameOn) > 0.5;
            params.tameDepthDb = static_cast<float> (valueOf (SweetParam::tameDepth));
            params.tameFreqHz = static_cast<float> (valueOf (SweetParam::tameFreq));
            params.roundOn = valueOf (SweetParam::roundOn) > 0.5;
            params.roundDrive = percent (valueOf (SweetParam::roundDrive));
            params.roundMix = percent (valueOf (SweetParam::roundMix));
        }

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            sweetener.process (left, right, numSamples, params);
        }

        GrexFx::SweetenerFx sweetener;
        GrexFx::SweetenerFx::Params params;
    };

    //==============================================================================
    // Sonify Ducker: Grex's two-band ducker. Each band dips where the sidechain plays loud in its
    // range (a vocal sent to it carves its room out of the guitars), as deep as its Depth allows.
    namespace DuckParam
    {
        enum : uint32_t { on1 = 1, freq1, q1, depth1, threshold1, ratio1, attack1, release1,
                          on2, freq2, q2, depth2, threshold2, ratio2, attack2, release2 };
    }

    class DuckerPlugin final : public BuiltinPlugin
    {
    public:
        DuckerPlugin()
            : BuiltinPlugin (descriptorFor (Builtins::duckerId, "Sonify Ducker", { "audio-effect", "equalizer", "compressor", "stereo" }),
                             {
                                 { DuckParam::on1,        "Low On",        0.0,     1.0,    1.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 { DuckParam::freq1,      "Low Freq",     40.0, 16000.0,  450.0, "Hz", 0 },
                                 { DuckParam::q1,         "Low Q",         0.3,     8.0,    1.2, "",   2 },
                                 { DuckParam::depth1,     "Low Depth",     0.0,    24.0,    6.0, "dB", 1 },
                                 { DuckParam::threshold1, "Low Threshold", -60.0,    0.0,  -30.0, "dB", 1 },
                                 { DuckParam::ratio1,     "Low Ratio",     1.0,    20.0,    4.0, ":1", 1 },
                                 { DuckParam::attack1,    "Low Attack",    0.5,   200.0,    8.0, "ms", 1 },
                                 { DuckParam::release1,   "Low Release",  20.0,  1000.0,  180.0, "ms", 0 },
                                 { DuckParam::on2,        "High On",       0.0,     1.0,    1.0, "",   0, true, false, true, nullptr, { "Off", "On" } },
                                 { DuckParam::freq2,      "High Freq",    40.0, 16000.0, 2600.0, "Hz", 0 },
                                 { DuckParam::q2,         "High Q",        0.3,     8.0,    1.4, "",   2 },
                                 { DuckParam::depth2,     "High Depth",    0.0,    24.0,    6.0, "dB", 1 },
                                 { DuckParam::threshold2, "High Threshold", -60.0,   0.0,  -30.0, "dB", 1 },
                                 { DuckParam::ratio2,     "High Ratio",    1.0,    20.0,    4.0, ":1", 1 },
                                 { DuckParam::attack2,    "High Attack",   0.5,   200.0,    4.0, "ms", 1 },
                                 { DuckParam::release2,   "High Release", 20.0,  1000.0,  140.0, "ms", 0 }
                             })
        {
            ducker.setEnabled (true);
        }

        bool hasSidechain() const noexcept override           { return true; }
        const char* getPanelName() const noexcept override   { return "ducker"; }
        juce::StringArray getDisplaySources() const override  { return { "duck.gr1", "duck.gr2", "duck.key1", "duck.key2" }; }
        juce::StringArray getCurveSources() const override    { return { "duck.curve" }; }

        float readSource (const juce::String& source) const noexcept override
        {
            if (source == "duck.gr1" || source == "duck.gr2")
                return juce::jlimit (0.0f, 1.0f, ducker.getGainReductionDb (source.getLastCharacter() == '1' ? 0 : 1) / 24.0f);

            // The key's level against the threshold: -60 to 0 dB across the meter.
            if (source == "duck.key1" || source == "duck.key2")
                return juce::jlimit (0.0f, 1.0f, (ducker.getSidechainLevelDb (source.getLastCharacter() == '1' ? 0 : 1) + 60.0f) / 60.0f);

            return BuiltinPlugin::readSource (source);
        }

        void readCurve (const juce::String& source, float* destination, int numSamples) override
        {
            if (source != "duck.curve")
                return BuiltinPlugin::readCurve (source, destination, numSamples);

            // Each band as deep as it dips now (a bell: x - (1 - g) * bandpass(x)).
            constexpr double rate = 48000.0;
            std::array<SonifyDsp::Biquad, 2> bells {};
            std::array<double, 2> amounts {};

            for (int band = 0; band < 2; ++band)
            {
                const auto base = band == 0 ? DuckParam::on1 : DuckParam::on2;

                if (valueOf (base) < 0.5)
                    continue;

                bells[static_cast<size_t> (band)] = SonifyDsp::Biquad::bandPass (rate, valueOf (base + 1), valueOf (base + 2));
                amounts[static_cast<size_t> (band)] = 1.0 - juce::Decibels::decibelsToGain (-static_cast<double> (ducker.getGainReductionDb (band)));
            }

            drawResponse (destination, numSamples, 24.0, [&] (double hz) { return duckBellsDb (bells, amounts, hz, rate); });
        }

    private:
        void prepareAudio (double rate, int maxBlockSize) override
        {
            ducker.prepare (rate);
            prepareSidechain (maxBlockSize);
        }

        void update() noexcept override
        {
            for (int band = 0; band < 2; ++band)
            {
                const auto base = band == 0 ? DuckParam::on1 : DuckParam::on2;
                ducker.setBandEnabled (band, valueOf (base) > 0.5);
                ducker.setFreq (band, static_cast<float> (valueOf (base + 1)));
                ducker.setQ (band, static_cast<float> (valueOf (base + 2)));
                ducker.setDepthDb (band, static_cast<float> (valueOf (base + 3)));
                ducker.setThresholdDb (band, static_cast<float> (valueOf (base + 4)));
                ducker.setRatio (band, static_cast<float> (valueOf (base + 5)));
                ducker.setAttackMs (band, static_cast<float> (valueOf (base + 6)));
                ducker.setReleaseMs (band, static_cast<float> (valueOf (base + 7)));
            }
        }

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            // The key: what the sidechain brings (silent while nothing sends to it).
            float* styleChannels[] { left, right };
            juce::AudioBuffer<float> style (styleChannels, 2, numSamples);

            if (getSidechainCapacity() >= numSamples)
            {
                float* keyChannels[] { getSidechainChannel (0), getSidechainChannel (1) };
                const juce::AudioBuffer<float> key (keyChannels, 2, numSamples);
                ducker.process (style, key);
            }
        }

        GrexFx::StyleDucker ducker;
    };

    const std::vector<Plugin::Descriptor>& effectDescriptors()
    {
        static const std::vector<Plugin::Descriptor> all {
            ChorusPlugin().getDescriptor(), PhaserPlugin().getDescriptor(), AutoWahPlugin().getDescriptor(),
            SpacePlugin().getDescriptor(), DelayPlugin().getDescriptor(), SweetenerPlugin().getDescriptor(),
            DuckerPlugin().getDescriptor(), TestSynths::describe()
        };

        return all;
    }
}

//==============================================================================
const std::vector<Plugin::Descriptor>& BuiltinEffects::describe()
{
    return effectDescriptors();
}

juce::String BuiltinEffects::categoryOf (const juce::String& id)
{
    if (id == Builtins::chorusId || id == Builtins::phaserId)   return "Modulation";
    if (id == Builtins::autoWahId)                              return "Filter";
    if (id == Builtins::reverbId)                               return "Reverb";
    if (id == Builtins::delayId)                                return "Delay";
    if (id == Builtins::sweetenerId || id == Builtins::duckerId) return "Dynamics";
    if (id == Builtins::testSynthId)                            return "Synth";
    return {};
}

std::unique_ptr<Plugin> BuiltinEffects::create (const juce::String& id)
{
    if (id == Builtins::chorusId)      return std::make_unique<ChorusPlugin>();
    if (id == Builtins::phaserId)      return std::make_unique<PhaserPlugin>();
    if (id == Builtins::autoWahId)     return std::make_unique<AutoWahPlugin>();
    if (id == Builtins::reverbId)      return std::make_unique<SpacePlugin>();
    if (id == Builtins::delayId)       return std::make_unique<DelayPlugin>();
    if (id == Builtins::sweetenerId)   return std::make_unique<SweetenerPlugin>();
    if (id == Builtins::duckerId)      return std::make_unique<DuckerPlugin>();
    if (id == Builtins::testSynthId)   return TestSynths::create();
    return nullptr;
}
