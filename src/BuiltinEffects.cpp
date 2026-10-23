// D:\workspace\Fanan_web_hub\src\BuiltinEffects.cpp

#include "BuiltinEffects.h"
#include "BuiltinPlugins.h"
#include "GrexFx.h"

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
    // Sonify Reverb: Grex's hall (a diffuser into a Householder network), with its early
    // reflections. Dry and wet are separate: the source stays where it is.
    namespace ReverbParam   { enum : uint32_t { size = 1, decay, damping, preDelay, lowCut, highCut, early, earlySize, dry, wet }; }

    class ReverbPlugin final : public BuiltinPlugin
    {
    public:
        ReverbPlugin()
            : BuiltinPlugin (descriptorFor (Builtins::reverbId, "Sonify Reverb", { "audio-effect", "reverb", "stereo" }),
                             {
                                 { ReverbParam::size,      "Size",        0.0,   100.0,    50.0, "%",  0 },
                                 { ReverbParam::decay,     "Decay",       0.0,   100.0,    50.0, "%",  0 },
                                 { ReverbParam::damping,   "Damping",     0.0,   100.0,    50.0, "%",  0 },
                                 { ReverbParam::preDelay,  "Pre-Delay",  25.0,   200.0,    25.0, "ms", 0 },
                                 { ReverbParam::lowCut,    "Low Cut",    20.0,  1000.0,   150.0, "Hz", 0 },
                                 { ReverbParam::highCut,   "High Cut", 1000.0, 20000.0, 20000.0, "Hz", 0, false, false, true, "max" },
                                 { ReverbParam::early,     "Early",       0.0,   100.0,     0.0, "%",  0 },
                                 { ReverbParam::earlySize, "Early Size",  0.0,   100.0,    50.0, "%",  0 },
                                 { ReverbParam::dry,       "Dry",         0.0,   100.0,   100.0, "%",  0 },
                                 { ReverbParam::wet,       "Wet",         0.0,   100.0,    30.0, "%",  0 }
                             })
        {
        }

        const char* getPanelName() const noexcept override   { return "reverb"; }

    private:
        void prepareAudio (double rate, int) override   { reverb.prepare (rate, true); }
        void update() noexcept override {}

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            const auto preDelay01 = static_cast<float> ((valueOf (ReverbParam::preDelay) - 25.0) / 175.0);
            reverb.process (left, right, numSamples,
                            percent (valueOf (ReverbParam::size)), percent (valueOf (ReverbParam::damping)), percent (valueOf (ReverbParam::wet)),
                            preDelay01, getSampleRate(), percent (valueOf (ReverbParam::dry)), percent (valueOf (ReverbParam::decay)),
                            hzToNorm (valueOf (ReverbParam::lowCut)), hzToNorm (valueOf (ReverbParam::highCut)),
                            percent (valueOf (ReverbParam::early)), percent (valueOf (ReverbParam::earlySize)));
        }

        GrexFx::ReverbFx reverb;
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

            drawResponse (destination, numSamples, 24.0, [&] (double hz)
            {
                // Bells subtract a band-pass: the response of 1 - a * bp, added in dB.
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
            });
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
            ReverbPlugin().getDescriptor(), DelayPlugin().getDescriptor(), SweetenerPlugin().getDescriptor(),
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
    if (id == Builtins::reverbId)      return std::make_unique<ReverbPlugin>();
    if (id == Builtins::delayId)       return std::make_unique<DelayPlugin>();
    if (id == Builtins::sweetenerId)   return std::make_unique<SweetenerPlugin>();
    if (id == Builtins::duckerId)      return std::make_unique<DuckerPlugin>();
    if (id == Builtins::testSynthId)   return TestSynths::create();
    return nullptr;
}
