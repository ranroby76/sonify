// D:\workspace\Fanan_web_hub\src\BuiltinPlugins.cpp

#include "BuiltinPlugins.h"
#include "BuiltinEffects.h"
#include "Container.h"
#include "PlaylistPlayer.h"
#include "SonifySampler.h"

#include <complex>
#include <cstring>

namespace
{
    using Spec = BuiltinPlugin::Spec;

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

    Plugin::Descriptor eqDescriptor()          { return descriptorFor (Builtins::eqId, "Sonify EQ", { "audio-effect", "equalizer", "stereo" }); }
    Plugin::Descriptor compressorDescriptor()  { return descriptorFor (Builtins::compressorId, "Sonify Compressor", { "audio-effect", "compressor", "stereo" }); }
    Plugin::Descriptor limiterDescriptor()     { return descriptorFor (Builtins::limiterId, "Sonify Limiter", { "audio-effect", "limiter", "mastering", "stereo" }); }
    Plugin::Descriptor masteringDescriptor()   { return descriptorFor (Builtins::masteringId, "Sonify Mastering", { "audio-effect", "mastering", "stereo" }); }

    //==============================================================================
    // Sonify EQ: a channel EQ. Low and high cuts, two shelves, two bells.
    namespace EqParam
    {
        enum : uint32_t { lowCut = 1, lowFreq, lowGain, lowMidFreq, lowMidGain, lowMidQ, highMidFreq, highMidGain, highMidQ,
                          highFreq, highGain, highCut, output };
    }

    class EqPlugin final : public BuiltinPlugin
    {
    public:
        EqPlugin()
            : BuiltinPlugin (eqDescriptor(),
                             {
                                 { EqParam::lowCut,      "Low Cut",         10.0,   500.0,    10.0, "Hz", 0, false, false, true, "min" },
                                 { EqParam::lowFreq,     "Low Shelf Freq",  30.0,   500.0,   100.0, "Hz", 0 },
                                 { EqParam::lowGain,     "Low Shelf Gain", -15.0,    15.0,     0.0, "dB", 1 },
                                 { EqParam::lowMidFreq,  "Low Mid Freq",    40.0,  2000.0,   300.0, "Hz", 0 },
                                 { EqParam::lowMidGain,  "Low Mid Gain",   -15.0,    15.0,     0.0, "dB", 1 },
                                 { EqParam::lowMidQ,     "Low Mid Q",        0.3,     8.0,     1.0, "",   2 },
                                 { EqParam::highMidFreq, "High Mid Freq",  300.0, 12000.0,  2500.0, "Hz", 0 },
                                 { EqParam::highMidGain, "High Mid Gain",  -15.0,    15.0,     0.0, "dB", 1 },
                                 { EqParam::highMidQ,    "High Mid Q",       0.3,     8.0,     1.0, "",   2 },
                                 { EqParam::highFreq,    "High Shelf Freq", 1500.0, 18000.0, 8000.0, "Hz", 0 },
                                 { EqParam::highGain,    "High Shelf Gain", -15.0,   15.0,     0.0, "dB", 1 },
                                 { EqParam::highCut,     "High Cut",       2000.0, 22000.0, 22000.0, "Hz", 0, false, false, true, "max" },
                                 { EqParam::output,      "Output",         -24.0,    24.0,     0.0, "dB", 1 }
                             })
        {
        }

        const char* getPanelName() const noexcept override   { return "eq"; }
        juce::StringArray getCurveSources() const override   { return { "eq.curve" }; }

        void readCurve (const juce::String& source, float* destination, int numSamples) override
        {
            if (source != "eq.curve")
                return BuiltinPlugin::readCurve (source, destination, numSamples);

            // The curve the bands make together (at a standard rate: the shape, whatever the device's).
            using namespace SonifyDsp;
            constexpr double rate = 48000.0;
            const auto lowCut = valueOf (EqParam::lowCut), highCut = valueOf (EqParam::highCut);
            const std::array<Biquad, 6> bands {
                lowCut > 10.0 ? Biquad::highPass (rate, lowCut, 0.7071) : Biquad {},
                Biquad::lowShelf (rate, valueOf (EqParam::lowFreq), valueOf (EqParam::lowGain)),
                Biquad::peak (rate, valueOf (EqParam::lowMidFreq), valueOf (EqParam::lowMidQ), valueOf (EqParam::lowMidGain)),
                Biquad::peak (rate, valueOf (EqParam::highMidFreq), valueOf (EqParam::highMidQ), valueOf (EqParam::highMidGain)),
                Biquad::highShelf (rate, valueOf (EqParam::highFreq), valueOf (EqParam::highGain)),
                highCut < 22000.0 ? Biquad::lowPass (rate, highCut, 0.7071) : Biquad {}
            };
            const auto output = valueOf (EqParam::output);

            drawResponse (destination, numSamples, 18.0, [&] (double hz)
            {
                double db = output;

                for (const auto& band : bands)
                    db += magnitudeDb (band, hz, rate);

                return db;
            });
        }

    private:
        void prepareAudio (double rate, int) override
        {
            sampleRate = rate;

            for (auto& pair : states)
                for (auto& state : pair)
                    state.reset();
        }

        void update() noexcept override
        {
            using namespace SonifyDsp;
            const auto lowCut = valueOf (EqParam::lowCut);
            const auto highCut = valueOf (EqParam::highCut);
            std::array<bool, 6> wasOff {};

            for (size_t band = 0; band < filters.size(); ++band)
                wasOff[band] = filters[band].isIdentity();

            filters[0] = lowCut > 10.0 ? Biquad::highPass (sampleRate, lowCut, 0.7071) : Biquad {};
            filters[1] = Biquad::lowShelf (sampleRate, valueOf (EqParam::lowFreq), valueOf (EqParam::lowGain));
            filters[2] = Biquad::peak (sampleRate, valueOf (EqParam::lowMidFreq), valueOf (EqParam::lowMidQ), valueOf (EqParam::lowMidGain));
            filters[3] = Biquad::peak (sampleRate, valueOf (EqParam::highMidFreq), valueOf (EqParam::highMidQ), valueOf (EqParam::highMidGain));
            filters[4] = Biquad::highShelf (sampleRate, valueOf (EqParam::highFreq), valueOf (EqParam::highGain));
            filters[5] = highCut < 22000.0 && highCut < sampleRate * 0.49 ? Biquad::lowPass (sampleRate, highCut, 0.7071) : Biquad {};
            output = static_cast<float> (dbToGain (valueOf (EqParam::output)));

            // A band switched off kept what it last held (it's skipped): back on, it starts clean.
            for (size_t band = 0; band < filters.size(); ++band)
                if (wasOff[band] && ! filters[band].isIdentity())
                    for (auto& state : states[band])
                        state.reset();
        }

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            for (size_t band = 0; band < filters.size(); ++band)
            {
                if (filters[band].isIdentity())
                    continue;

                auto& [stateLeft, stateRight] = states[band];

                for (int i = 0; i < numSamples; ++i)
                {
                    left[i] = stateLeft.process (filters[band], left[i]);
                    right[i] = stateRight.process (filters[band], right[i]);
                }
            }

            if (std::abs (output - 1.0f) > 1.0e-6f)
            {
                juce::FloatVectorOperations::multiply (left, output, numSamples);
                juce::FloatVectorOperations::multiply (right, output, numSamples);
            }
        }

        static double dbToGain (double db) noexcept   { return SonifyDsp::dbToGain (db); }

        double sampleRate = 48000.0;
        std::array<SonifyDsp::Biquad, 6> filters {};
        std::array<std::array<SonifyDsp::BiquadState, 2>, 6> states {};
        float output = 1.0f;
    };

    //==============================================================================
    // Sonify Compressor: soft knee, peak or RMS, parallel mix, and a sidechain input to duck.
    namespace CompParam
    {
        enum : uint32_t { threshold = 1, ratio, attack, release, knee, makeup, mix, detector, sidechain };
    }

    class CompressorPlugin final : public BuiltinPlugin
    {
    public:
        CompressorPlugin()
            : BuiltinPlugin (compressorDescriptor(),
                             {
                                 { CompParam::threshold, "Threshold", -60.0,    0.0, -18.0, "dB", 1 },
                                 { CompParam::ratio,     "Ratio",       1.0,   20.0,   3.0, ":1", 1 },
                                 { CompParam::attack,    "Attack",      0.1,  200.0,  10.0, "ms", 1 },
                                 { CompParam::release,   "Release",     5.0, 2000.0, 120.0, "ms", 0 },
                                 { CompParam::knee,      "Knee",        0.0,   24.0,   6.0, "dB", 1 },
                                 { CompParam::makeup,    "Makeup",    -12.0,   24.0,   0.0, "dB", 1 },
                                 { CompParam::mix,       "Mix",         0.0,  100.0, 100.0, "%",  0 },
                                 { CompParam::detector,  "Detector",    0.0,    1.0,   1.0, "",   0, true, false, true, nullptr, { "Peak", "RMS" } },
                                 { CompParam::sidechain, "Sidechain",   0.0,    1.0,   0.0, "",   0, true, false, true, nullptr, { "Off", "On" } }
                             })
        {
        }

        bool hasSidechain() const noexcept override   { return true; }

        const char* getPanelName() const noexcept override   { return "compressor"; }
        juce::StringArray getDisplaySources() const override  { return { "comp.gr" }; }

        float readSource (const juce::String& source) const noexcept override
        {
            // Gain reduction, as a VU reads it (0 at rest, down the scale as it works).
            if (source == "comp.gr")
                return vuReduction (reduction.load (std::memory_order_relaxed));

            return BuiltinPlugin::readSource (source);
        }

    private:
        void prepareAudio (double rate, int maxBlockSize) override
        {
            compressor.prepare (rate);
            prepareSidechain (maxBlockSize);
        }

        void update() noexcept override
        {
            SonifyDsp::Compressor::Settings s;
            s.thresholdDb = static_cast<float> (valueOf (CompParam::threshold));
            s.ratio = static_cast<float> (valueOf (CompParam::ratio));
            s.attackMs = static_cast<float> (valueOf (CompParam::attack));
            s.releaseMs = static_cast<float> (valueOf (CompParam::release));
            s.kneeDb = static_cast<float> (valueOf (CompParam::knee));
            s.makeupDb = static_cast<float> (valueOf (CompParam::makeup));
            s.mix = static_cast<float> (valueOf (CompParam::mix) / 100.0);
            s.rms = valueOf (CompParam::detector) > 0.5;
            compressor.setSettings (s);
        }

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            // Keyed by what the sidechain brings, when asked (it's silent while nothing sends to it).
            const bool keyed = valueOf (CompParam::sidechain) > 0.5 && getSidechainCapacity() >= numSamples;
            compressor.process (left, right, numSamples,
                                keyed ? getSidechainChannel (0) : nullptr,
                                keyed ? getSidechainChannel (1) : nullptr);
            reduction.store (static_cast<float> (compressor.getReductionDb()), std::memory_order_relaxed);
        }

        SonifyDsp::Compressor compressor;
        std::atomic<float> reduction { 0.0f };
    };

    //==============================================================================
    // Sonify Limiter: gain into a true-peak brickwall.
    namespace LimitParam
    {
        enum : uint32_t { gain = 1, ceiling, release };
    }

    class LimiterPlugin final : public BuiltinPlugin
    {
    public:
        LimiterPlugin()
            : BuiltinPlugin (limiterDescriptor(),
                             {
                                 { LimitParam::gain,    "Gain",      0.0,   24.0,  0.0, "dB",   1 },
                                 { LimitParam::ceiling, "Ceiling", -12.0,    0.0, -1.0, "dBTP", 1 },
                                 { LimitParam::release, "Release",   1.0, 1000.0, 80.0, "ms",   0 }
                             })
        {
        }

        const char* getPanelName() const noexcept override   { return "limiter"; }
        int getLatencySamples() const noexcept override      { return limiter.getLatency(); }   // its look-ahead
        juce::StringArray getDisplaySources() const override  { return { "limit.gr" }; }

        float readSource (const juce::String& source) const noexcept override
        {
            if (source == "limit.gr")
                return juce::jlimit (0.0f, 1.0f, reduction.load (std::memory_order_relaxed) / 12.0f);

            return BuiltinPlugin::readSource (source);
        }

    private:
        void prepareAudio (double rate, int) override
        {
            limiter.prepare (rate);
        }

        void update() noexcept override
        {
            gain = static_cast<float> (SonifyDsp::dbToGain (valueOf (LimitParam::gain)));
            limiter.setSettings (static_cast<float> (valueOf (LimitParam::ceiling)), static_cast<float> (valueOf (LimitParam::release)));
        }

        void processAudio (float* left, float* right, int numSamples) noexcept override
        {
            if (std::abs (gain - 1.0f) > 1.0e-6f)
            {
                juce::FloatVectorOperations::multiply (left, gain, numSamples);
                juce::FloatVectorOperations::multiply (right, gain, numSamples);
            }

            limiter.process (left, right, numSamples);
            reduction.store (static_cast<float> (limiter.getReductionDb()), std::memory_order_relaxed);
        }

        SonifyDsp::Limiter limiter;
        float gain = 1.0f;
        std::atomic<float> reduction { 0.0f };
    };

    //==============================================================================
    namespace MasterParam
    {
        enum : uint32_t { input = 1, lowCut, low, lowFreq, lowMid, lowMidFreq, mid, midFreq, presence, presenceFreq, air, airFreq,
                          eqAmount, threshold, ratio, attack, release, drive, width, ceiling, limiterRelease, output,
                          listen = 30, sameLoudness, loudnessChange };
    }
}

//==============================================================================
bool Builtins::isBuiltinId (const juce::String& pluginId)
{
    return pluginId.startsWith (idPrefix);
}

const juce::File& Builtins::file()
{
    static const juce::File executable = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    return executable;
}

bool Builtins::isBuiltinFile (const juce::String& path)
{
    return path.isNotEmpty() && path.equalsIgnoreCase (file().getFullPathName());
}

const std::vector<Plugin::Descriptor>& Builtins::describe()
{
    static const std::vector<Plugin::Descriptor> all = []
    {
        std::vector<Plugin::Descriptor> list { eqDescriptor(), compressorDescriptor(), limiterDescriptor(), masteringDescriptor() };

        for (const auto& more : BuiltinEffects::describe())
            list.push_back (more);

        for (const auto& tool : { Containers::describe (false), Containers::describe (true), Playlists::describe(), Samplers::describe() })
            list.push_back (tool);

        return list;
    }();

    return all;
}

juce::String Builtins::categoryOf (const juce::String& pluginId)
{
    if (pluginId == eqId)          return "EQ";
    if (pluginId == masteringId)   return "Mastering";
    if (const auto category = BuiltinEffects::categoryOf (pluginId); category.isNotEmpty())
        return category;
    if (Containers::isContainerId (pluginId) || pluginId == Playlists::playerId || pluginId == Samplers::playerId)
        return "System Tool";
    return "Dynamics";
}

std::unique_ptr<Plugin> Builtins::create (const juce::String& pluginId, juce::String& error)
{
    if (pluginId == eqId)          return std::make_unique<EqPlugin>();
    if (pluginId == compressorId)  return std::make_unique<CompressorPlugin>();
    if (pluginId == limiterId)     return std::make_unique<LimiterPlugin>();
    if (pluginId == masteringId)   return std::make_unique<MasteringPlugin>();
    if (auto made = BuiltinEffects::create (pluginId))  return made;
    if (pluginId == Containers::instrumentId)  return std::make_unique<ContainerPlugin> (false);
    if (pluginId == Containers::effectId)      return std::make_unique<ContainerPlugin> (true);
    if (pluginId == Playlists::playerId)       return std::make_unique<PlaylistPlayer>();
    if (pluginId == Samplers::playerId)        return std::make_unique<SonifySampler>();

    error = "This version of Sonify has no system tool called " + pluginId.fromFirstOccurrenceOf (idPrefix, false, false) + ".";
    return nullptr;
}

//==============================================================================
BuiltinPlugin::BuiltinPlugin (Descriptor d, std::vector<Spec> s)
    : descriptor (std::move (d)), specs (std::move (s))
{
    inFile.push_back (descriptor);
    values = std::make_unique<std::atomic<double>[]> (specs.size());
    uint32_t largest = 0;

    for (size_t i = 0; i < specs.size(); ++i)
    {
        const auto& spec = specs[i];

        Parameter parameter;
        parameter.id = spec.id;
        parameter.name = spec.name;
        parameter.minValue = spec.minValue;
        parameter.maxValue = spec.maxValue;
        parameter.defaultValue = spec.defaultValue;
        parameter.stepped = spec.stepped;
        parameter.hidden = spec.hidden;
        parameters.push_back (parameter);

        values[i].store (spec.defaultValue);
        largest = juce::jmax (largest, spec.id);
    }

    indexById.assign (static_cast<size_t> (largest) + 1, -1);

    for (size_t i = 0; i < specs.size(); ++i)
        indexById[static_cast<size_t> (specs[i].id)] = static_cast<int> (i);
}

BuiltinPlugin::~BuiltinPlugin()
{
    screen.reset();   // its window goes first: it reads the plugin
}

namespace
{
    BuiltinPlugin::ScreenFactory& screenFactory()
    {
        static BuiltinPlugin::ScreenFactory factory;
        return factory;
    }
}

void BuiltinPlugin::setScreenFactory (ScreenFactory factory)
{
    screenFactory() = std::move (factory);
}

bool BuiltinPlugin::showNativeScreen (const juce::String& title)
{
    if (getPanelName() == nullptr)
        return false;

    if (screen == nullptr && screenFactory() != nullptr)
        screen = screenFactory() (*this);

    if (screen == nullptr)
        return false;

    screen->show (title);
    return true;
}

void BuiltinPlugin::hideNativeScreen()
{
    if (screen != nullptr)
        screen->hide();
}

float BuiltinPlugin::readSource (const juce::String& source) const noexcept
{
    const auto at = [this] (int index) { return peaks[static_cast<size_t> (index)].load (std::memory_order_relaxed); };

    if (source == "in L")                                return at (0);
    if (source == "in R")                                return at (1);
    if (source == "out L" || source == "plugin.out L")   return at (2);
    if (source == "out R" || source == "plugin.out R")   return at (3);
    if (source == "in")                                  return juce::jmax (at (0), at (1));
    if (source == "out")                                 return juce::jmax (at (2), at (3));
    return 0.0f;
}

void BuiltinPlugin::readCurve (const juce::String&, float* destination, int numSamples)
{
    juce::FloatVectorOperations::clear (destination, numSamples);
}

void BuiltinPlugin::drawResponse (float* destination, int numSamples, double rangeDb, const std::function<double (double)>& dbAt)
{
    // A scope draws 1024 values from the first rising zero crossing: one is put at the start (a
    // value below zero, then the curve's first), so the curve is drawn from the left edge.
    juce::FloatVectorOperations::clear (destination, numSamples);
    constexpr int points = 1024;

    if (numSamples < points + 2)
        return;

    const auto at = [&] (int i)
    {
        const auto hz = 20.0 * std::pow (1000.0, static_cast<double> (i) / (points - 1));
        return static_cast<float> (juce::jlimit (-1.0, 1.0, dbAt (hz) / rangeDb));
    };

    destination[0] = -1.0f;
    const auto first = at (0);
    destination[1] = juce::jmax (0.0f, first);

    for (int i = 1; i < points; ++i)
        destination[i + 1] = at (i);
}

double BuiltinPlugin::magnitudeDb (const SonifyDsp::Biquad& f, double hz, double sampleRate)
{
    if (f.isIdentity())
        return 0.0;

    const auto w = 2.0 * juce::MathConstants<double>::pi * hz / sampleRate;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    const auto h = (f.b0 + f.b1 * z1 + f.b2 * z2) / (1.0 + f.a1 * z1 + f.a2 * z2);
    return 20.0 * std::log10 (juce::jmax (1.0e-9, std::abs (h)));
}

double BuiltinPlugin::getBlockTempo() const noexcept
{
    const auto* t = blockTransport;
    return t != nullptr && (t->flags & CLAP_TRANSPORT_HAS_TEMPO) != 0 && t->tempo > 1.0 ? t->tempo : 120.0;
}

bool BuiltinPlugin::isBlockPlaying() const noexcept
{
    return blockTransport != nullptr && (blockTransport->flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
}

void BuiltinPlugin::measure (int first, const float* left, const float* right, int numSamples) noexcept
{
    // Peaks fall back over about a quarter second, so a meter read now and then misses none.
    const auto fall = static_cast<float> (std::exp (-numSamples / (0.25 * currentRate)));

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* data = channel == 0 ? left : right;
        auto& peak = peaks[static_cast<size_t> (first + channel)];
        const auto range = data != nullptr ? juce::FloatVectorOperations::findMinAndMax (data, numSamples) : juce::Range<float>();
        const auto level = juce::jmax (-range.getStart(), range.getEnd());
        peak.store (juce::jmax (level, peak.load (std::memory_order_relaxed) * fall), std::memory_order_relaxed);
    }
}

void BuiltinPlugin::measureOutput (const float* left, const float* right, int numSamples) noexcept
{
    measure (2, left, right, numSamples);
}

int BuiltinPlugin::indexOf (uint32_t id) const noexcept
{
    return id < indexById.size() ? indexById[static_cast<size_t> (id)] : -1;
}

int BuiltinPlugin::findParameter (uint32_t id) const noexcept
{
    return indexOf (id);
}

double BuiltinPlugin::getParameterValue (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, static_cast<int> (specs.size())) ? values[static_cast<size_t> (index)].load (std::memory_order_relaxed) : 0.0;
}

double BuiltinPlugin::valueOf (uint32_t id) const noexcept
{
    return getParameterValue (indexOf (id));
}

void BuiltinPlugin::setValue (uint32_t id, double value)
{
    queueParameter (indexOf (id), value);
}

juce::String BuiltinPlugin::getParameterText (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (specs.size())))
        return {};

    const auto& spec = specs[static_cast<size_t> (index)];

    if (spec.offAt != nullptr)
    {
        const bool atMin = std::strcmp (spec.offAt, "min") == 0 && value <= spec.minValue + 1.0e-6;
        const bool atMax = std::strcmp (spec.offAt, "max") == 0 && value >= spec.maxValue - 1.0e-6;

        if (atMin || atMax)
            return "Off";
    }

    if (! spec.choices.empty())
    {
        const auto choice = juce::jlimit (0, static_cast<int> (spec.choices.size()) - 1, juce::roundToInt (value - spec.minValue));
        return spec.choices[static_cast<size_t> (choice)];
    }

    const juce::String unit (spec.unit);

    if (unit == "Hz" && value >= 1000.0)
        return juce::String (value / 1000.0, value >= 10000.0 ? 1 : 2) + " kHz";

    auto text = juce::String (quantised (spec, value), decimalsOf (spec));

    // Gains read with their sign, as on a console.
    if (unit.startsWith ("dB") && value > 0.0 && spec.minValue < 0.0)
        text = "+" + text;

    if (unit.isEmpty())
        return text;

    return unit.startsWithChar (':') ? text + unit : text + " " + unit;
}

int BuiltinPlugin::decimalsOf (const Spec& spec) noexcept
{
    if (spec.stepped || std::strcmp (spec.unit, "%") == 0)
        return 0;

    auto decimals = juce::jlimit (0, 2, spec.decimals);
    const auto range = spec.maxValue - spec.minValue;

    while (decimals < 2 && range * std::pow (10.0, decimals) < 100.0)
        ++decimals;

    return decimals;
}

double BuiltinPlugin::quantised (const Spec& spec, double value) noexcept
{
    if (! std::isfinite (value))
        value = spec.defaultValue;

    value = juce::jlimit (spec.minValue, spec.maxValue, value);
    const auto scale = std::pow (10.0, decimalsOf (spec));
    return juce::jlimit (spec.minValue, spec.maxValue, std::round (value * scale) / scale);
}

double BuiltinPlugin::queueParameter (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (specs.size())))
        return 0.0;

    const auto& spec = specs[static_cast<size_t> (index)];

    value = quantised (spec, value);
    values[static_cast<size_t> (index)].store (value);
    dirty.store (true);

    if (spec.saved)
        stateChanged.store (true);

    return value;
}

void BuiltinPlugin::setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept
{
    if (! juce::isPositiveAndBelow (indexHint, static_cast<int> (specs.size())) || specs[static_cast<size_t> (indexHint)].id != paramId)
        indexHint = indexOf (paramId);

    if (indexHint < 0)
        return;

    const auto& spec = specs[static_cast<size_t> (indexHint)];
    values[static_cast<size_t> (indexHint)].store (quantised (spec, value), std::memory_order_relaxed);
    dirty.store (true, std::memory_order_relaxed);
}

bool BuiltinPlugin::saveState (juce::MemoryBlock& destination)
{
    auto* params = new juce::DynamicObject();

    for (size_t i = 0; i < specs.size(); ++i)
        if (specs[i].saved)
            params->setProperty (juce::String (specs[i].id), values[i].load());

    auto* root = new juce::DynamicObject();
    root->setProperty ("plugin", descriptor.id);
    root->setProperty ("version", 1);
    root->setProperty ("params", juce::var (params));

    if (const auto extra = saveExtra(); ! extra.isVoid())
        root->setProperty ("extra", extra);

    // The preset it's on (and whether its values changed since), as a DAW keeps it.
    if (presetChoice.known)
    {
        root->setProperty ("preset", presetChoice.name);
        root->setProperty ("presetGroup", presetChoice.group);
        root->setProperty ("presetModified", presetChoice.modified);
    }

    const auto text = juce::JSON::toString (juce::var (root), true);
    destination.reset();
    destination.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
    return true;
}

bool BuiltinPlugin::loadState (const juce::MemoryBlock& source)
{
    const auto parsed = juce::JSON::parse (source.toString());

    if (! parsed.isObject() || parsed.getProperty ("plugin", {}).toString() != descriptor.id)
        return false;

    if (const auto* params = parsed.getProperty ("params", {}).getDynamicObject())
    {
        for (const auto& [name, value] : params->getProperties())
        {
            const auto id = static_cast<uint32_t> (name.toString().getIntValue());
            const auto index = indexOf (id);

            if (index >= 0 && specs[static_cast<size_t> (index)].saved)
                queueParameter (index, static_cast<double> (value));
        }
    }

    loadExtra (parsed.getProperty ("extra", {}));

    presetChoice = {};

    if (parsed.hasProperty ("preset"))
        presetChoice = { parsed.getProperty ("preset", {}).toString(), parsed.getProperty ("presetGroup", {}).toString(),
                         static_cast<bool> (parsed.getProperty ("presetModified", false)), true };

    stateChanged.store (false);
    return true;
}

bool BuiltinPlugin::activate (double sampleRate, int maxBlockSize)
{
    currentRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    prepareAudio (sampleRate > 0.0 ? sampleRate : 48000.0, juce::jmax (1, maxBlockSize));
    dirty.store (true);
    active.store (true);
    return true;
}

void BuiltinPlugin::process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                             int numSamples, const juce::MidiBuffer&, const clap_event_transport_t* transport) noexcept
{
    if (outLeft == nullptr || outRight == nullptr || numSamples <= 0)
        return;

    // Effects work in place; a separate input is copied over first.
    if (inLeft != nullptr && inLeft != outLeft)
        juce::FloatVectorOperations::copy (outLeft, inLeft, numSamples);

    if (inRight != nullptr && inRight != outRight)
        juce::FloatVectorOperations::copy (outRight, inRight, numSamples);

    if (! active.load (std::memory_order_relaxed))
        return;

    blockTransport = transport;

    if (dirty.exchange (false, std::memory_order_acq_rel))
        update();

    measure (0, outLeft, outRight, numSamples);
    processAudio (outLeft, outRight, numSamples);
    measure (2, outLeft, outRight, numSamples);
}

bool BuiltinPlugin::takeDirty() noexcept
{
    return dirty.exchange (false, std::memory_order_acq_rel);
}

void BuiltinPlugin::setTransport (const clap_event_transport_t* transport) noexcept
{
    blockTransport = transport;
}

//==============================================================================
MasteringPlugin::MasteringPlugin()
    : BuiltinPlugin (masteringDescriptor(),
                     {
                         { MasterParam::input,          "Input",           -12.0,    12.0,     0.0, "dB",   1 },
                         { MasterParam::lowCut,         "Low Cut",          10.0,   120.0,    20.0, "Hz",   0, false, false, true, "min" },
                         { MasterParam::low,            "Low",              -6.0,     6.0,     0.0, "dB",   1 },
                         { MasterParam::lowFreq,        "Low Freq",         40.0,   200.0,    90.0, "Hz",   0 },
                         { MasterParam::lowMid,         "Low Mid",          -6.0,     6.0,     0.0, "dB",   1 },
                         { MasterParam::lowMidFreq,     "Low Mid Freq",    120.0,   600.0,   250.0, "Hz",   0 },
                         { MasterParam::mid,            "Mid",              -6.0,     6.0,     0.0, "dB",   1 },
                         { MasterParam::midFreq,        "Mid Freq",        500.0,  3000.0,  1000.0, "Hz",   0 },
                         { MasterParam::presence,       "Presence",         -6.0,     6.0,     0.0, "dB",   1 },
                         { MasterParam::presenceFreq,   "Presence Freq",  2000.0,  8000.0,  3500.0, "Hz",   0 },
                         { MasterParam::air,            "Air",              -6.0,     6.0,     0.0, "dB",   1 },
                         { MasterParam::airFreq,        "Air Freq",       6000.0, 16000.0, 10000.0, "Hz",   0 },
                         { MasterParam::eqAmount,       "EQ Amount",         0.0,   100.0,   100.0, "%",    0 },
                         { MasterParam::threshold,      "Glue Threshold",  -40.0,     0.0,     0.0, "dB",   1 },
                         { MasterParam::ratio,          "Glue Ratio",        1.0,     8.0,     2.0, ":1",   1 },
                         { MasterParam::attack,         "Glue Attack",       1.0,   100.0,    25.0, "ms",   0 },
                         { MasterParam::release,        "Glue Release",     20.0,  1000.0,   200.0, "ms",   0 },
                         { MasterParam::drive,          "Drive",           -24.0,    24.0,     0.0, "dB",   1 },
                         { MasterParam::width,          "Width",             0.0,   200.0,   100.0, "%",    0 },
                         { MasterParam::ceiling,        "Ceiling",          -6.0,     0.0,    -1.0, "dBTP", 1 },
                         { MasterParam::limiterRelease, "Limiter Release",  10.0,   500.0,    80.0, "ms",   0 },
                         { MasterParam::output,         "Output",          -12.0,     0.0,     0.0, "dB",   1 },
                         // Comparing (not kept with the song), and what the assistant added (kept).
                         { MasterParam::listen,         "Listen",            0.0,     1.0,     0.0, "",     0, true, true, false, nullptr, { "Master", "Original" } },
                         { MasterParam::sameLoudness,   "Same Loudness",     0.0,     1.0,     0.0, "",     0, true, true, false, nullptr, { "Off", "On" } },
                         { MasterParam::loudnessChange, "Loudness Change", -60.0,    60.0,     0.0, "dB",   1, false, true, true }
                     })
{
}

void MasteringPlugin::applyAssistant (const SonifyDsp::MasteringSettings& s, const juce::var& newInfo, double loudnessChangeDb)
{
    setValue (MasterParam::input, s.inputGainDb);
    setValue (MasterParam::lowCut, s.lowCutHz);
    setValue (MasterParam::low, s.lowGainDb);
    setValue (MasterParam::lowFreq, s.lowHz);
    setValue (MasterParam::lowMid, s.lowMidGainDb);
    setValue (MasterParam::lowMidFreq, s.lowMidHz);
    setValue (MasterParam::mid, s.midGainDb);
    setValue (MasterParam::midFreq, s.midHz);
    setValue (MasterParam::presence, s.presenceGainDb);
    setValue (MasterParam::presenceFreq, s.presenceHz);
    setValue (MasterParam::air, s.airGainDb);
    setValue (MasterParam::airFreq, s.airHz);
    setValue (MasterParam::eqAmount, s.eqAmount * 100.0);
    setValue (MasterParam::threshold, s.compressor.thresholdDb);
    setValue (MasterParam::ratio, s.compressor.ratio);
    setValue (MasterParam::attack, s.compressor.attackMs);
    setValue (MasterParam::release, s.compressor.releaseMs);
    setValue (MasterParam::drive, s.driveDb);
    setValue (MasterParam::width, s.width * 100.0);
    setValue (MasterParam::ceiling, s.ceilingDb);
    setValue (MasterParam::limiterRelease, s.limiterReleaseMs);
    setValue (MasterParam::output, s.outputDb);
    setValue (MasterParam::loudnessChange, loudnessChangeDb);
    info = newInfo;
    stateChanged.store (true);
}

void MasteringPlugin::setListen (bool original, bool sameLoudness)
{
    setValue (MasterParam::listen, original ? 1.0 : 0.0);
    setValue (MasterParam::sameLoudness, sameLoudness ? 1.0 : 0.0);
}

bool MasteringPlugin::isListeningToOriginal() const noexcept   { return valueOf (MasterParam::listen) > 0.5; }
bool MasteringPlugin::isSameLoudness() const noexcept          { return valueOf (MasterParam::sameLoudness) > 0.5; }

void MasteringPlugin::prepareAudio (double rate, int)
{
    chain.prepare (rate);
    scratch.setSize (2, 2048, false, true, false);

    for (auto& line : dryDelay)
        line.assign (static_cast<size_t> (chain.getLatency() + 1), 0.0f);

    dryAt = 0;
    dryMix = isListeningToOriginal() ? 1.0f : 0.0f;
    masterGain = 1.0f;
}

void MasteringPlugin::update() noexcept
{
    SonifyDsp::MasteringSettings s;
    s.inputGainDb = static_cast<float> (valueOf (MasterParam::input));
    s.lowCutHz = static_cast<float> (valueOf (MasterParam::lowCut));
    s.lowGainDb = static_cast<float> (valueOf (MasterParam::low));
    s.lowHz = static_cast<float> (valueOf (MasterParam::lowFreq));
    s.lowMidGainDb = static_cast<float> (valueOf (MasterParam::lowMid));
    s.lowMidHz = static_cast<float> (valueOf (MasterParam::lowMidFreq));
    s.midGainDb = static_cast<float> (valueOf (MasterParam::mid));
    s.midHz = static_cast<float> (valueOf (MasterParam::midFreq));
    s.presenceGainDb = static_cast<float> (valueOf (MasterParam::presence));
    s.presenceHz = static_cast<float> (valueOf (MasterParam::presenceFreq));
    s.airGainDb = static_cast<float> (valueOf (MasterParam::air));
    s.airHz = static_cast<float> (valueOf (MasterParam::airFreq));
    s.eqAmount = static_cast<float> (valueOf (MasterParam::eqAmount) / 100.0);
    s.compressor.thresholdDb = static_cast<float> (valueOf (MasterParam::threshold));
    s.compressor.ratio = static_cast<float> (valueOf (MasterParam::ratio));
    s.compressor.attackMs = static_cast<float> (valueOf (MasterParam::attack));
    s.compressor.releaseMs = static_cast<float> (valueOf (MasterParam::release));
    s.driveDb = static_cast<float> (valueOf (MasterParam::drive));
    s.width = static_cast<float> (valueOf (MasterParam::width) / 100.0);
    s.ceilingDb = static_cast<float> (valueOf (MasterParam::ceiling));
    s.limiterReleaseMs = static_cast<float> (valueOf (MasterParam::limiterRelease));
    s.outputDb = static_cast<float> (valueOf (MasterParam::output));
    chain.setSettings (s);
}

void MasteringPlugin::processAudio (float* left, float* right, int numSamples) noexcept
{
    // Comparing: the original, or the master turned down to the original's loudness.
    const auto targetMix = isListeningToOriginal() ? 1.0f : 0.0f;
    const auto targetGain = isSameLoudness() && targetMix < 0.5f
                              ? static_cast<float> (SonifyDsp::dbToGain (-valueOf (MasterParam::loudnessChange)))
                              : 1.0f;
    const auto length = static_cast<int> (dryDelay[0].size());

    for (int start = 0; start < numSamples; start += scratch.getNumSamples())
    {
        const auto count = juce::jmin (scratch.getNumSamples(), numSamples - start);
        auto* l = left + start;
        auto* r = right + start;
        auto* dryLeft = scratch.getWritePointer (0);
        auto* dryRight = scratch.getWritePointer (1);

        // The original, as late as the master comes out of the limiter.
        for (int i = 0; i < count; ++i)
        {
            dryDelay[0][static_cast<size_t> (dryAt)] = l[i];
            dryDelay[1][static_cast<size_t> (dryAt)] = r[i];
            const auto readAt = (dryAt + 1) % length;
            dryLeft[i] = dryDelay[0][static_cast<size_t> (readAt)];
            dryRight[i] = dryDelay[1][static_cast<size_t> (readAt)];
            dryAt = readAt;
        }

        chain.process (l, r, count);
        glueReduction.store (static_cast<float> (chain.getGlueReductionDb()), std::memory_order_relaxed);
        limiterReduction.store (static_cast<float> (chain.getLimiterReductionDb()), std::memory_order_relaxed);

        if (dryMix == 0.0f && targetMix == 0.0f && masterGain == 1.0f && targetGain == 1.0f)
            continue;

        // Switching glides across the block, so it never clicks.
        const auto mixStep = (targetMix - dryMix) / static_cast<float> (count);
        const auto gainStep = (targetGain - masterGain) / static_cast<float> (count);

        for (int i = 0; i < count; ++i)
        {
            dryMix += mixStep;
            masterGain += gainStep;
            l[i] = l[i] * masterGain * (1.0f - dryMix) + dryLeft[i] * dryMix;
            r[i] = r[i] * masterGain * (1.0f - dryMix) + dryRight[i] * dryMix;
        }

        dryMix = targetMix;
        masterGain = targetGain;
    }
}

float MasteringPlugin::readSource (const juce::String& source) const noexcept
{
    if (source == "master.glue")    return juce::jlimit (0.0f, 1.0f, glueReduction.load (std::memory_order_relaxed) / 12.0f);
    if (source == "master.limit")   return juce::jlimit (0.0f, 1.0f, limiterReduction.load (std::memory_order_relaxed) / 12.0f);
    return BuiltinPlugin::readSource (source);
}

void MasteringPlugin::readCurve (const juce::String& source, float* destination, int numSamples)
{
    if (source != "master.curve")
        return BuiltinPlugin::readCurve (source, destination, numSamples);

    // The tone the EQ gives (at its amount), as the chain sets its bands.
    using SonifyDsp::Biquad;
    constexpr double rate = 48000.0;
    const auto amount = valueOf (MasterParam::eqAmount) / 100.0;
    const auto lowCut = valueOf (MasterParam::lowCut);
    const std::array<Biquad, 6> bands {
        lowCut > 10.0 ? Biquad::highPass (rate, lowCut, 0.7071) : Biquad {},
        Biquad::lowShelf (rate, valueOf (MasterParam::lowFreq), valueOf (MasterParam::low) * amount),
        Biquad::peak (rate, valueOf (MasterParam::lowMidFreq), 0.9, valueOf (MasterParam::lowMid) * amount),
        Biquad::peak (rate, valueOf (MasterParam::midFreq), 0.8, valueOf (MasterParam::mid) * amount),
        Biquad::peak (rate, valueOf (MasterParam::presenceFreq), 0.8, valueOf (MasterParam::presence) * amount),
        Biquad::highShelf (rate, valueOf (MasterParam::airFreq), valueOf (MasterParam::air) * amount)
    };

    drawResponse (destination, numSamples, 9.0, [&] (double hz)
    {
        double db = 0.0;

        for (const auto& band : bands)
            db += magnitudeDb (band, hz, rate);

        return db;
    });
}
