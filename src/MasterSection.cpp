// D:\workspace\Fanan_web_hub\src\MasterSection.cpp

#include "MasterSection.h"

#include <cmath>

namespace MasterSection
{
namespace
{
    using Kind = ParamSpec::Kind;
    using SonifyDsp::Biquad;
    using SonifyDsp::BiquadState;

    /** tanh, closely enough for tape's saturation and far quicker: level at the bottom (slope 1),
        flat from 3 on. */
    inline float softClip (float x) noexcept
    {
        x = juce::jlimit (-3.0f, 3.0f, x);
        const auto x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    constexpr double butterworthQ = 0.70710678118654752;
}

//==============================================================================
const std::array<ParamSpec, numParams>& specs()
{
    static const std::array<ParamSpec, numParams> list {{
        { "on",            "Mastering",       "Mastering", Kind::toggle,   0.0f,     1.0f,     0.0f, "",   "" },
        { "mode",          "Mode",            "Mastering", Kind::choice,   0.0f,     2.0f,     0.0f, "",   "Auto|Mix + Master|Master only" },
        { "lowCut",        "Low Cut",         "EQ",        Kind::knob,    10.0f,   200.0f,    10.0f, "Hz", "" },
        { "eqLow",         "Low",             "EQ",        Kind::knob,   -12.0f,    12.0f,     0.0f, "dB", "" },
        { "eqLowFreq",     "Low Freq",        "EQ",        Kind::knob,    30.0f,   300.0f,    90.0f, "Hz", "" },
        { "eqMid",         "Mid",             "EQ",        Kind::knob,   -12.0f,    12.0f,     0.0f, "dB", "" },
        { "eqMidFreq",     "Mid Freq",        "EQ",        Kind::knob,   200.0f,  6000.0f,  1000.0f, "Hz", "" },
        { "eqHigh",        "High",            "EQ",        Kind::knob,   -12.0f,    12.0f,     0.0f, "dB", "" },
        { "eqHighFreq",    "High Freq",       "EQ",        Kind::knob,  2000.0f, 16000.0f, 10000.0f, "Hz", "" },
        { "xoverLow",      "Low / Mid",       "Multiband", Kind::knob,    60.0f,   600.0f,   200.0f, "Hz", "" },
        { "xoverHigh",     "Mid / High",      "Multiband", Kind::knob,  1000.0f, 10000.0f,  3000.0f, "Hz", "" },
        { "mbOn",          "Comp In",         "Multiband", Kind::toggle,   0.0f,     1.0f,     0.0f, "",   "" },
        { "mbAttack",      "Attack",          "Multiband", Kind::knob,     1.0f,   100.0f,    20.0f, "ms", "" },
        { "mbRelease",     "Release",         "Multiband", Kind::knob,    20.0f,  1000.0f,   200.0f, "ms", "" },
        { "lowThreshold",  "Low Threshold",   "Low Band",  Kind::knob,   -40.0f,     0.0f,   -18.0f, "dB", "" },
        { "lowRatio",      "Low Ratio",       "Low Band",  Kind::knob,     1.0f,    10.0f,     2.0f, ":1", "" },
        { "lowGain",       "Low Gain",        "Low Band",  Kind::knob,   -12.0f,    12.0f,     0.0f, "dB", "" },
        { "lowWidth",      "Low Width",       "Low Band",  Kind::knob,     0.0f,   200.0f,   100.0f, "%",  "" },
        { "midThreshold",  "Mid Threshold",   "Mid Band",  Kind::knob,   -40.0f,     0.0f,   -18.0f, "dB", "" },
        { "midRatio",      "Mid Ratio",       "Mid Band",  Kind::knob,     1.0f,    10.0f,     2.0f, ":1", "" },
        { "midGain",       "Mid Gain",        "Mid Band",  Kind::knob,   -12.0f,    12.0f,     0.0f, "dB", "" },
        { "midWidth",      "Mid Width",       "Mid Band",  Kind::knob,     0.0f,   200.0f,   100.0f, "%",  "" },
        { "highThreshold", "High Threshold",  "High Band", Kind::knob,   -40.0f,     0.0f,   -18.0f, "dB", "" },
        { "highRatio",     "High Ratio",      "High Band", Kind::knob,     1.0f,    10.0f,     2.0f, ":1", "" },
        { "highGain",      "High Gain",       "High Band", Kind::knob,   -12.0f,    12.0f,     0.0f, "dB", "" },
        { "highWidth",     "High Width",      "High Band", Kind::knob,     0.0f,   200.0f,   100.0f, "%",  "" },
        { "tapeOn",        "Tape In",         "Tape",      Kind::toggle,   0.0f,     1.0f,     0.0f, "",   "" },
        { "tapeDrive",     "Drive",           "Tape",      Kind::knob,     0.0f,   100.0f,    30.0f, "%",  "" },
        { "limitGain",     "Gain",            "Limiter",   Kind::knob,     0.0f,    24.0f,     0.0f, "dB", "" },
        { "ceiling",       "Ceiling",         "Limiter",   Kind::knob,    -6.0f,     0.0f,    -1.0f, "dB", "" },
        { "limitRelease",  "Release",         "Limiter",   Kind::knob,    10.0f,   500.0f,    80.0f, "ms", "" },
    }};

    return list;
}

int indexOf (const juce::String& id)
{
    const auto& list = specs();

    for (size_t i = 0; i < list.size(); ++i)
        if (id == list[i].id)
            return static_cast<int> (i);

    return -1;
}

Mode resolveMode (const Settings& settings, int tracksWithClips, bool onlyAnAudioTrack) noexcept
{
    const auto chosen = juce::roundToInt (settings.get (mode));

    if (chosen == 1) return Mode::mixAndMaster;
    if (chosen == 2) return Mode::masterOnly;

    return tracksWithClips == 1 && onlyAnAudioTrack ? Mode::masterOnly : Mode::mixAndMaster;
}

//==============================================================================
/** The chain itself: the EQ, the multiband (split, compressed, its gains and widths), the tape and
    the limiter. Allocates in prepare() only. */
class Processor::Network
{
public:
    void prepare (double rate, int maxBlock)
    {
        sampleRate = rate;
        blockSize = juce::jmax (16, maxBlock);
        limiter.prepare (sampleRate);

        for (auto& compressor : compressors)
            compressor.prepare (sampleRate);

        for (auto* buffer : { &bands, &squeezed })
            buffer->setSize (2 * numBands, blockSize, false, true, true);

        dry.setSize (2, blockSize, false, true, true);
        reset();
    }

    void reset() noexcept
    {
        eqStates = {};
        splitStates = {};
        tapeStates = {};

        for (auto& compressor : compressors)
            compressor.reset();

        limiter.reset();
        compressing = false;
        taping = false;
        fresh = true;
    }

    int getLatency() const noexcept   { return limiter.getLatency(); }
    int getBlockSize() const noexcept { return blockSize; }

    void set (const Settings& settings) noexcept
    {
        const auto get = [&settings] (int index) { return static_cast<double> (settings.get (index)); };

        Eq next;
        next.lowCut = get (lowCut) > specs()[lowCut].minimum + 0.5;
        next.filters[0] = next.lowCut ? Biquad::highPass (sampleRate, get (lowCut), butterworthQ) : Biquad {};
        next.filters[1] = Biquad::lowShelf (sampleRate, get (eqLowFreq), get (eqLow));
        next.filters[2] = Biquad::peak (sampleRate, get (eqMidFreq), 0.8, get (eqMid));
        next.filters[3] = Biquad::highShelf (sampleRate, get (eqHighFreq), get (eqHigh));
        nextEq = next;
        eqChanged = true;

        // The crossovers: their Q's Butterworth, two in a row (Linkwitz-Riley); the mid's above
        // the low's always.
        const auto low = get (xoverLow);
        const auto high = juce::jmax (get (xoverHigh), low * 1.5);
        lowLp = Biquad::lowPass (sampleRate, low, butterworthQ);
        lowHp = Biquad::highPass (sampleRate, low, butterworthQ);
        highLp = Biquad::lowPass (sampleRate, high, butterworthQ);
        highHp = Biquad::highPass (sampleRate, high, butterworthQ);

        for (int band = 0; band < numBands; ++band)
        {
            SonifyDsp::Compressor::Settings c;
            c.thresholdDb = settings.get (bandParam (band, lowThreshold));
            c.ratio = settings.get (bandParam (band, lowRatio));
            c.attackMs = settings.get (mbAttack);
            c.releaseMs = settings.get (mbRelease);
            c.kneeDb = 6.0f;
            c.makeupDb = 0.0f;
            c.mix = 1.0f;
            c.rms = true;
            compressors[static_cast<size_t> (band)].setSettings (c);

            targetGain[static_cast<size_t> (band)] = juce::Decibels::decibelsToGain (settings.get (bandParam (band, lowGain)));
            targetWidth[static_cast<size_t> (band)] = settings.get (bandParam (band, lowWidth)) / 100.0f;
        }

        wantCompress = settings.isOn (mbOn);

        // The tape: how hard it's driven, and the low bump and softer top that come with it.
        const auto amount = get (tapeDrive) / 100.0;
        tapeDriveGain = static_cast<float> (1.0 + 5.0 * amount);
        tapeFilters[0] = Biquad::lowShelf (sampleRate, 60.0, 1.5 * amount);
        tapeFilters[1] = Biquad::highShelf (sampleRate, 12000.0, -2.0 * amount);
        wantTape = settings.isOn (tapeOn);

        targetLimitGain = juce::Decibels::decibelsToGain (settings.get (limitGain));
        limiter.setSettings (settings.get (ceiling), settings.get (limitRelease));

        if (fresh)
        {
            eq = nextEq;
            eqChanged = false;
            lastGain = targetGain;
            lastWidth = targetWidth;
            lastLimitGain = targetLimitGain;
            compressing = wantCompress;
            taping = wantTape;
            fresh = false;
        }
    }

    /** In place, at most getBlockSize() samples. */
    void process (float* left, float* right, int n) noexcept
    {
        runEq (left, right, n);
        runMultiband (left, right, n);
        runTape (left, right, n);

        // Into the limiter.
        const auto step = (targetLimitGain - lastLimitGain) / static_cast<float> (n);
        auto gain = lastLimitGain;

        for (int i = 0; i < n; ++i)
        {
            gain += step;
            left[i] *= gain;
            right[i] *= gain;
        }

        lastLimitGain = targetLimitGain;
        limiter.process (left, right, n);
    }

    float takeLimiterReduction() noexcept
    {
        juce::int64 samples = 0;
        const auto total = limiter.takeReduction (samples);
        return samples > 0 ? static_cast<float> (total / static_cast<double> (samples)) : 0.0f;
    }

    float getBandReduction (int band) const noexcept
    {
        return compressing ? static_cast<float> (compressors[static_cast<size_t> (band)].getReductionDb()) : 0.0f;
    }

private:
    struct Eq
    {
        bool lowCut = false;
        std::array<Biquad, 4> filters {};
    };

    using EqStates = std::array<std::array<BiquadState, 4>, 2>;

    static float runEqSample (const Eq& eq, std::array<BiquadState, 4>& states, float x) noexcept
    {
        if (eq.lowCut)
            x = states[0].process (eq.filters[0], x);

        // The bands always run (a flat one too), so each is ready when it moves.
        for (size_t f = 1; f < eq.filters.size(); ++f)
            x = states[f].process (eq.filters[f], x);

        return x;
    }

    void runEq (float* left, float* right, int n) noexcept
    {
        // A change plays both ways over the block, the old fading into the new: no clicks.
        if (eqChanged)
        {
            eqChanged = false;
            auto newStates = eqStates;

            if (nextEq.lowCut && ! eq.lowCut)
                for (auto& side : newStates)
                    side[0].reset();

            const auto step = 1.0f / static_cast<float> (n);

            for (int channel = 0; channel < 2; ++channel)
            {
                auto* data = channel == 0 ? left : right;
                auto& oldSide = eqStates[static_cast<size_t> (channel)];
                auto& newSide = newStates[static_cast<size_t> (channel)];

                for (int i = 0; i < n; ++i)
                {
                    const auto was = runEqSample (eq, oldSide, data[i]);
                    const auto now = runEqSample (nextEq, newSide, data[i]);
                    data[i] = was + (now - was) * step * static_cast<float> (i + 1);
                }
            }

            eqStates = newStates;
            eq = nextEq;
            return;
        }

        for (int channel = 0; channel < 2; ++channel)
        {
            auto* data = channel == 0 ? left : right;
            auto& states = eqStates[static_cast<size_t> (channel)];

            for (int i = 0; i < n; ++i)
                data[i] = runEqSample (eq, states, data[i]);
        }
    }

    /** Split into three (the low band through the upper crossover's allpass, so the three add up
        flat), each band compressed (or not), then its gain and width, added back together. */
    void runMultiband (float* left, float* right, int n) noexcept
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto* in = channel == 0 ? left : right;
            auto& s = splitStates[static_cast<size_t> (channel)];
            auto* low = bands.getWritePointer (channel);
            auto* mid = bands.getWritePointer (2 + channel);
            auto* high = bands.getWritePointer (4 + channel);

            for (int i = 0; i < n; ++i)
            {
                const auto x = in[i];
                const auto lowPart = s[1].process (lowLp, s[0].process (lowLp, x));
                const auto rest = s[3].process (lowHp, s[2].process (lowHp, x));
                mid[i] = s[5].process (highLp, s[4].process (highLp, rest));
                high[i] = s[7].process (highHp, s[6].process (highHp, rest));

                // The low band through the upper crossover too (its lows and highs added back: an allpass).
                low[i] = s[9].process (highLp, s[8].process (highLp, lowPart)) + s[11].process (highHp, s[10].process (highHp, lowPart));
            }
        }

        // The compressors; switched in or out, they crossfade over the block.
        const bool switching = wantCompress != compressing;

        if (wantCompress || switching)
        {
            for (int band = 0; band < numBands; ++band)
            {
                auto* l = bands.getWritePointer (2 * band);
                auto* r = bands.getWritePointer (2 * band + 1);
                auto* cl = squeezed.getWritePointer (2 * band);
                auto* cr = squeezed.getWritePointer (2 * band + 1);
                juce::FloatVectorOperations::copy (cl, l, n);
                juce::FloatVectorOperations::copy (cr, r, n);

                if (switching && wantCompress)
                    compressors[static_cast<size_t> (band)].reset();

                compressors[static_cast<size_t> (band)].process (cl, cr, n);

                if (switching)
                {
                    const auto step = 1.0f / static_cast<float> (n);

                    for (int i = 0; i < n; ++i)
                    {
                        const auto w = wantCompress ? step * static_cast<float> (i + 1) : 1.0f - step * static_cast<float> (i + 1);
                        l[i] += (cl[i] - l[i]) * w;
                        r[i] += (cr[i] - r[i]) * w;
                    }
                }
                else
                {
                    juce::FloatVectorOperations::copy (l, cl, n);
                    juce::FloatVectorOperations::copy (r, cr, n);
                }
            }

            compressing = wantCompress;
        }

        // Each band's gain and width (ramped over the block), added back together.
        juce::FloatVectorOperations::clear (left, n);
        juce::FloatVectorOperations::clear (right, n);

        for (int band = 0; band < numBands; ++band)
        {
            const auto b = static_cast<size_t> (band);
            const auto* l = bands.getReadPointer (2 * band);
            const auto* r = bands.getReadPointer (2 * band + 1);
            const auto gainStep = (targetGain[b] - lastGain[b]) / static_cast<float> (n);
            const auto widthStep = (targetWidth[b] - lastWidth[b]) / static_cast<float> (n);
            auto gain = lastGain[b], width = lastWidth[b];

            for (int i = 0; i < n; ++i)
            {
                gain += gainStep;
                width += widthStep;
                const auto midPart = 0.5f * (l[i] + r[i]) * gain;
                const auto side = 0.5f * (l[i] - r[i]) * gain * width;
                left[i] += midPart + side;
                right[i] += midPart - side;
            }

            lastGain[b] = targetGain[b];
            lastWidth[b] = targetWidth[b];
        }
    }

    void runTape (float* left, float* right, int n) noexcept
    {
        const bool switching = wantTape != taping;

        if (! wantTape && ! switching)
            return;

        if (switching && wantTape)
            tapeStates = {};

        auto* dryLeft = dry.getWritePointer (0);
        auto* dryRight = dry.getWritePointer (1);
        juce::FloatVectorOperations::copy (dryLeft, left, n);
        juce::FloatVectorOperations::copy (dryRight, right, n);

        for (int channel = 0; channel < 2; ++channel)
        {
            auto* data = channel == 0 ? left : right;
            auto& states = tapeStates[static_cast<size_t> (channel)];

            for (int i = 0; i < n; ++i)
            {
                auto x = softClip (tapeDriveGain * data[i]) / tapeDriveGain;
                x = states[0].process (tapeFilters[0], x);
                data[i] = states[1].process (tapeFilters[1], x);
            }
        }

        if (switching)
        {
            const auto step = 1.0f / static_cast<float> (n);

            for (int i = 0; i < n; ++i)
            {
                const auto w = wantTape ? step * static_cast<float> (i + 1) : 1.0f - step * static_cast<float> (i + 1);
                left[i] = dryLeft[i] + (left[i] - dryLeft[i]) * w;
                right[i] = dryRight[i] + (right[i] - dryRight[i]) * w;
            }

            taping = wantTape;
        }
    }

    double sampleRate = 48000.0;
    int blockSize = 512;
    bool fresh = true;

    Eq eq, nextEq;
    bool eqChanged = false;
    EqStates eqStates {};

    Biquad lowLp, lowHp, highLp, highHp;
    std::array<std::array<BiquadState, 12>, 2> splitStates {};
    std::array<SonifyDsp::Compressor, numBands> compressors;
    bool compressing = false, wantCompress = false;
    std::array<float, numBands> targetGain { 1.0f, 1.0f, 1.0f }, lastGain { 1.0f, 1.0f, 1.0f };
    std::array<float, numBands> targetWidth { 1.0f, 1.0f, 1.0f }, lastWidth { 1.0f, 1.0f, 1.0f };
    juce::AudioBuffer<float> bands, squeezed, dry;

    float tapeDriveGain = 1.0f;
    std::array<Biquad, 2> tapeFilters {};
    std::array<std::array<BiquadState, 2>, 2> tapeStates {};
    bool taping = false, wantTape = false;

    SonifyDsp::Limiter limiter;
    float targetLimitGain = 1.0f, lastLimitGain = 1.0f;
};

//==============================================================================
Processor::Processor() : network (std::make_unique<Network>())
{
    binPower.assign (histogramBins, 0.0);
    binCount.assign (histogramBins, 0);
}

Processor::~Processor() = default;

void Processor::prepare (double rate, int maxBlockSize)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    maxBlock = juce::jmax (16, maxBlockSize);
    network->prepare (sampleRate, maxBlock);

    // The K-weighting's two stages for this rate, from their analog design (as libebur128 does).
    {
        const auto f0 = 1681.974450955533, g = 3.999843853973347, q = 0.7071752369554196;
        const auto k = std::tan (SonifyDsp::pi * f0 / sampleRate);
        const auto vh = std::pow (10.0, g / 20.0);
        const auto vb = std::pow (vh, 0.4996667741545416);
        const auto a0 = 1.0 + k / q + k * k;
        shelf = { (vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
                  2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
    }

    {
        const auto f0 = 38.13547087602444, q = 0.5003270373238773;
        const auto k = std::tan (SonifyDsp::pi * f0 / sampleRate);
        const auto a0 = 1.0 + k / q + k * k;
        highPass = { 1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
    }

    stepLength = juce::jmax (1, static_cast<int> (std::lround (sampleRate * 0.1)));
    seenChanges = settings.getChanges() - 1;
    reset();
}

void Processor::reset() noexcept
{
    network->reset();
    seenChanges = settings.getChanges() - 1;   // set up again at the next block
    active = fadingOut = fadingIn = false;

    for (auto& state : kStates)
        state.reset();

    stepSum = 0.0;
    stepCount = 0;
    steps.fill (0.0);
    stepsKept = stepAt = 0;
    meters.resetRequested.store (true);
}

int Processor::getLatency() const noexcept
{
    return active ? network->getLatency() : 0;
}

int Processor::getChainLatency() const noexcept
{
    return network->getLatency();
}

void Processor::refresh() noexcept
{
    network->set (settings);
}

void Processor::runChain (float* left, float* right, int numSamples) noexcept
{
    // In pieces as long as the network's buffers.
    for (int done = 0; done < numSamples;)
    {
        const auto n = juce::jmin (network->getBlockSize(), numSamples - done);
        network->process (left + done, right + done, n);
        done += n;
    }
}

void Processor::process (float* left, float* right, int numSamples, bool bypass, bool playing, bool settle) noexcept
{
    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    if (const auto changes = settings.getChanges(); changes != seenChanges)
    {
        seenChanges = changes;
        refresh();
    }

    const bool wanted = settings.isOn (on) && ! bypass;
    const auto fadeLength = juce::jmax (16, juce::roundToInt (0.01 * sampleRate));   // 10 ms each way

    if (settle)
    {
        // At once, as it's set, and from silence (a render starts with nothing left from playing).
        active = wanted;
        network->reset();
        refresh();
        fadingOut = fadingIn = false;
    }
    else if (! fadingOut && ! fadingIn && wanted != active)
    {
        fadingOut = true;
        fadeAt = 0;
    }

    if (active)
        runChain (left, right, numSamples);

    if (fadingOut)
    {
        // Out to silence over 10 ms (the rest of the block silent), then the other way.
        for (int i = 0; i < numSamples; ++i)
        {
            const auto g = 1.0f - juce::jmin (1.0f, static_cast<float> (fadeAt++) / static_cast<float> (fadeLength));
            left[i] *= g;
            right[i] *= g;
        }

        if (fadeAt >= fadeLength)
        {
            fadingOut = false;
            active = ! active;

            if (active)
            {
                network->reset();
                refresh();
            }

            fadingIn = true;
            fadeAt = 0;
        }
    }
    else if (fadingIn)
    {
        // In from silence over 10 ms, once what comes out is sound (its look-ahead's empty start
        // left silent).
        const auto wait = active ? network->getLatency() : 0;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto g = juce::jlimit (0.0f, 1.0f, static_cast<float> (fadeAt++ - wait) / static_cast<float> (fadeLength));
            left[i] *= g;
            right[i] *= g;
        }

        if (fadeAt >= wait + fadeLength)
            fadingIn = false;
    }

    // The meters: the limiter's and the bands' reduction, then what leaves.
    meters.limiter.store (active ? network->takeLimiterReduction() : 0.0f, std::memory_order_relaxed);

    for (int band = 0; band < numBands; ++band)
        meters.bands[static_cast<size_t> (band)].store (active ? network->getBandReduction (band) : 0.0f, std::memory_order_relaxed);

    measure (left, right, numSamples, playing);
}

void Processor::measure (const float* left, const float* right, int numSamples, bool playing) noexcept
{
    if (meters.resetRequested.exchange (false))
    {
        std::fill (binPower.begin(), binPower.end(), 0.0);
        std::fill (binCount.begin(), binCount.end(), 0);

        for (auto& detector : peakDetectors)
            detector.reset();

        peakHeld = 0.0f;
        meters.integrated.store (-100.0f, std::memory_order_relaxed);
        meters.truePeak.store (-100.0f, std::memory_order_relaxed);
    }

    const auto loudnessOf = [] (double meanSquare) { return meanSquare > 1.0e-20 ? -0.691 + 10.0 * std::log10 (meanSquare) : -200.0; };

    for (int i = 0; i < numSamples; ++i)
    {
        peakHeld = juce::jmax (peakHeld, peakDetectors[0].push (left[i]), peakDetectors[1].push (right[i]));

        const auto l = kStates[2].processDouble (highPass, kStates[0].processDouble (shelf, left[i]));
        const auto r = kStates[3].processDouble (highPass, kStates[1].processDouble (shelf, right[i]));
        stepSum += l * l + r * r;

        if (++stepCount < stepLength)
            continue;

        // A 100 ms step: the momentary (its last four) and the short-term (its last thirty).
        steps[static_cast<size_t> (stepAt)] = stepSum / stepLength;
        stepAt = (stepAt + 1) % static_cast<int> (steps.size());
        stepsKept = juce::jmin (stepsKept + 1, static_cast<int> (steps.size()));
        stepSum = 0.0;
        stepCount = 0;

        const auto meanOfLast = [this] (int count)
        {
            double sum = 0.0;

            for (int k = 1; k <= count; ++k)
                sum += steps[static_cast<size_t> ((stepAt - k + static_cast<int> (steps.size())) % static_cast<int> (steps.size()))];

            return sum / juce::jmax (1, count);
        };

        const auto block = meanOfLast (juce::jmin (4, stepsKept));
        meters.momentary.store (static_cast<float> (juce::jmax (-100.0, loudnessOf (block))), std::memory_order_relaxed);
        meters.shortTerm.store (static_cast<float> (juce::jmax (-100.0, loudnessOf (meanOfLast (stepsKept)))), std::memory_order_relaxed);

        // The integrated loudness: each 400 ms block while the song plays, gated at -70 LUFS and
        // then 10 LU under their mean.
        if (! playing || stepsKept < 4)
            continue;

        const auto loudness = loudnessOf (block);

        if (loudness <= -70.0)
            continue;

        const auto bin = juce::jlimit (0, histogramBins - 1, static_cast<int> ((loudness + 70.0) * 10.0));
        binPower[static_cast<size_t> (bin)] += block;
        ++binCount[static_cast<size_t> (bin)];

        double sum = 0.0;
        juce::int64 count = 0;

        for (int b = 0; b < histogramBins; ++b)
        {
            sum += binPower[static_cast<size_t> (b)];
            count += binCount[static_cast<size_t> (b)];
        }

        if (count == 0)
            continue;

        const auto gate = loudnessOf (sum / static_cast<double> (count)) - 10.0;
        const auto firstBin = juce::jlimit (0, histogramBins, static_cast<int> (std::ceil ((gate + 70.0) * 10.0)));
        double gatedSum = 0.0;
        juce::int64 gatedCount = 0;

        for (int b = firstBin; b < histogramBins; ++b)
        {
            gatedSum += binPower[static_cast<size_t> (b)];
            gatedCount += binCount[static_cast<size_t> (b)];
        }

        if (gatedCount > 0)
            meters.integrated.store (static_cast<float> (loudnessOf (gatedSum / static_cast<double> (gatedCount))), std::memory_order_relaxed);
    }

    meters.truePeak.store (peakHeld > 1.0e-5f ? 20.0f * std::log10 (peakHeld) : -100.0f, std::memory_order_relaxed);
}
}
