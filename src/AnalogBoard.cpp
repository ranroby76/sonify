// D:\workspace\Fanan_web_hub\src\AnalogBoard.cpp

#include "AnalogBoard.h"

#include <cmath>

namespace AnalogBoard
{
namespace
{
    using Kind = ParamSpec::Kind;

    constexpr double twoPi = 2.0 * SonifyDsp::pi;

    void storeMax (std::atomic<float>& target, float value) noexcept
    {
        auto current = target.load (std::memory_order_relaxed);

        while (value > current && ! target.compare_exchange_weak (current, value, std::memory_order_relaxed))
        {
        }
    }

    float dbGain (float db, float closedAt) noexcept
    {
        return db <= closedAt ? 0.0f : juce::Decibels::decibelsToGain (db);
    }

    /** A circular delay line: tap (0) is the sample pushed last. */
    class DelayLine
    {
    public:
        void prepare (int longest)
        {
            const auto size = juce::nextPowerOfTwo (juce::jmax (8, longest + 4));
            data.assign (static_cast<size_t> (size), 0.0f);
            mask = size - 1;
            writeAt = 0;
        }

        void clear() noexcept                     { std::fill (data.begin(), data.end(), 0.0f); clearAt = -1; }
        int length() const noexcept               { return mask + 1; }

        /** Clearing it a little at a time (the audio thread, starting afresh): from the start... */
        void beginClear() noexcept                { clearAt = 0; }

        /** ...up to budget samples now (taken from the budget). True once it's all clear. */
        bool clearSome (int& budget) noexcept
        {
            if (clearAt < 0)
                return true;

            const auto now = juce::jmin (budget, length() - clearAt);
            std::fill (data.begin() + clearAt, data.begin() + clearAt + now, 0.0f);
            clearAt += now;
            budget -= now;

            if (clearAt < length())
                return false;

            clearAt = -1;
            return true;
        }

        void push (float sample) noexcept
        {
            data[static_cast<size_t> (writeAt)] = sample;
            writeAt = (writeAt + 1) & mask;
        }

        float tap (int delay) const noexcept
        {
            return data[static_cast<size_t> ((writeAt - 1 - delay) & mask)];
        }

        float tapFractional (float delay) const noexcept
        {
            const auto whole = static_cast<int> (delay);
            const auto fraction = delay - static_cast<float> (whole);
            const auto a = tap (whole);
            return a + (tap (whole + 1) - a) * fraction;
        }

    private:
        std::vector<float> data;
        int mask = 0, writeAt = 0, clearAt = -1;
    };

    /** A Schroeder allpass of a fixed length; its line can be tapped. */
    struct Allpass
    {
        DelayLine line;
        int length = 1;

        void prepare (int samples)                { length = juce::jmax (1, samples); line.prepare (length + 64); }

        float process (float input, float gain) noexcept
        {
            const auto delayed = line.tap (length - 1);
            const auto w = input + gain * delayed;
            line.push (w);
            return delayed - gain * w;
        }

        /** Its length moved by excursion samples (a modulated allpass). */
        float process (float input, float gain, float excursion) noexcept
        {
            const auto delayed = line.tapFractional (juce::jmax (0.0f, static_cast<float> (length - 1) + excursion));
            const auto w = input + gain * delayed;
            line.push (w);
            return delayed - gain * w;
        }
    };

    /** tanh, closely enough for saturation and far quicker: a rational curve, level at the
        bottom (slope 1) and flat from 3 on. */
    inline float softClip (float x) noexcept
    {
        x = juce::jlimit (-3.0f, 3.0f, x);
        const auto x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    /** softClip's slope at x. */
    inline float softClipSlope (float x) noexcept
    {
        const auto x2 = x * x, d = 27.0f + 9.0f * x2;
        return ((27.0f + 3.0f * x2) * d - x * (27.0f + x2) * 18.0f * x) / (d * d);
    }

    /** A slow sine (wow, flutter, a reverb's movement): worked out every 16 samples, in a line between. */
    struct Lfo
    {
        double phase = 0.0, step = 0.0;
        float value = 0.0f, slope = 0.0f;
        int left = 0;

        void setRate (double sampleRate, double hz, double startPhase = 0.0) noexcept
        {
            step = 2.0 * SonifyDsp::pi * hz / sampleRate;
            phase = startPhase;
            value = static_cast<float> (std::sin (phase));
            left = 0;
        }

        float next() noexcept
        {
            if (left == 0)
            {
                phase += 16.0 * step;

                if (phase >= 2.0 * SonifyDsp::pi)
                    phase -= 2.0 * SonifyDsp::pi;

                slope = (static_cast<float> (std::sin (phase)) - value) / 16.0f;
                left = 16;
            }

            --left;
            value += slope;
            return value;
        }
    };

    /** A one-pole low-pass: coefficient from a cutoff. */
    struct OnePole
    {
        float z = 0.0f, a = 0.0f;

        void setCutoff (double sampleRate, double hz) noexcept   { a = static_cast<float> (std::exp (-twoPi * hz / sampleRate)); }
        float lowPass (float x) noexcept                          { z = x + a * (z - x); return z; }
        float highPass (float x) noexcept                         { return x - lowPass (x); }
        void reset() noexcept                                     { z = 0.0f; }
    };

    /** A number with at most this many decimals, none trailing ("2.4", "16", "-3.5"). */
    juce::String number (float value, int decimals)
    {
        auto text = juce::String (value, decimals);

        if (text.containsChar ('.'))
            text = text.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

        return text == "-0" ? juce::String ("0") : text;
    }
}

//==============================================================================
juce::StringArray ParamSpec::choiceNames() const
{
    return juce::StringArray::fromTokens (choices, "|", "");
}

float ParamSpec::clamp (float value) const noexcept
{
    if (! std::isfinite (value))
        value = defaultValue;

    value = juce::jlimit (minimum, maximum, value);

    if (kind == Kind::toggle || kind == Kind::choice)
        value = std::round (value);

    return value;
}

juce::String ParamSpec::text (float value) const
{
    value = clamp (value);
    const juce::String u (unit);
    const juce::String key (id);

    if (kind == Kind::toggle)
        return value >= 0.5f ? "On" : "Off";

    if (kind == Kind::choice)
    {
        const auto names = choiceNames();
        return names[juce::jlimit (0, names.size() - 1, static_cast<int> (value))];
    }

    if (key == "pan")
    {
        const auto amount = juce::roundToInt (std::abs (value) * 100.0f);
        return amount == 0 ? juce::String ("C") : (value < 0.0f ? "L" : "R") + juce::String (amount);
    }

    if (u == "dB")
    {
        if (kind == Kind::fader && value <= silentDb)
            return juce::String::fromUTF8 ("\xe2\x88\x92\xe2\x88\x9e dB");   // −∞ dB

        if ((key.startsWith ("send") || key.endsWith ("Return")) && value <= offDb)
            return "Off";

        const auto shown = std::abs (value) < 0.05f ? 0.0f : value;
        return (shown > 0.0f && (key == "lf" || key == "lmf" || key == "hmf" || key == "hf" || key == "trim") ? "+" : "")
             + juce::String (shown, 1) + " dB";
    }

    if (u == "Hz")
    {
        if ((key == "hpf" || key == "widthBass") && value <= minimum + 0.5f)
            return "Off";

        return value >= 1000.0f ? number (value / 1000.0f, value >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (value)) + " Hz";   // "2.4 kHz"
    }

    if (u == "ms")
        return (value < 10.0f ? number (value, 1) : juce::String (juce::roundToInt (value))) + " ms";

    if (u == "%")
        return juce::String (juce::roundToInt (value)) + "%";

    if (u == ":1")
        return number (value, 1) + ":1";

    return number (value, 2);
}

const std::array<ParamSpec, numStripParams>& stripSpecs()
{
    static const std::array<ParamSpec, numStripParams> specs {{
        { "trim",       "Trim",      "Preamp",     Kind::knob,   -20.0f,  20.0f,    0.0f,  "dB", "" },
        { "drive",      "Drive",     "Preamp",     Kind::knob,     0.0f, 100.0f,    0.0f,  "%",  "" },
        { "phase",      "Phase",     "Preamp",     Kind::toggle,   0.0f,   1.0f,    0.0f,  "",   "" },
        { "hpf",        "High Pass", "Filter",     Kind::knob,    20.0f, 400.0f,   20.0f,  "Hz", "" },
        { "lf",         "LF",        "EQ",         Kind::knob,   -15.0f,  15.0f,    0.0f,  "dB", "" },
        { "lfFreq",     "LF Freq",   "EQ",         Kind::knob,    30.0f, 450.0f,  100.0f,  "Hz", "" },
        { "lmf",        "LMF",       "EQ",         Kind::knob,   -15.0f,  15.0f,    0.0f,  "dB", "" },
        { "lmfFreq",    "LMF Freq",  "EQ",         Kind::knob,   200.0f, 2500.0f, 600.0f,  "Hz", "" },
        { "hmf",        "HMF",       "EQ",         Kind::knob,   -15.0f,  15.0f,    0.0f,  "dB", "" },
        { "hmfFreq",    "HMF Freq",  "EQ",         Kind::knob,   600.0f, 7000.0f, 3000.0f, "Hz", "" },
        { "hf",         "HF",        "EQ",         Kind::knob,   -15.0f,  15.0f,    0.0f,  "dB", "" },
        { "hfFreq",     "HF Freq",   "EQ",         Kind::knob,  1500.0f, 16000.0f, 8000.0f, "Hz", "" },
        { "eqOn",       "EQ In",     "EQ",         Kind::toggle,   0.0f,   1.0f,    1.0f,  "",   "" },
        { "compOn",     "Comp In",   "Dynamics",   Kind::toggle,   0.0f,   1.0f,    0.0f,  "",   "" },
        { "threshold",  "Threshold", "Dynamics",   Kind::knob,   -40.0f,   0.0f,  -12.0f,  "dB", "" },
        { "ratio",      "Ratio",     "Dynamics",   Kind::knob,     1.0f,  20.0f,    3.0f,  ":1", "" },
        { "release",    "Release",   "Dynamics",   Kind::knob,    50.0f, 1500.0f, 200.0f,  "ms", "" },
        { "fastAttack", "Fast",      "Dynamics",   Kind::toggle,   0.0f,   1.0f,    0.0f,  "",   "" },
        { "makeup",     "Make-up",   "Dynamics",   Kind::knob,     0.0f,  20.0f,    0.0f,  "dB", "" },
        { "sendPlate",  "Plate",     "Sends",      Kind::knob,   offDb,    6.0f,   offDb,  "dB", "" },
        { "sendEcho",   "Echo",      "Sends",      Kind::knob,   offDb,    6.0f,   offDb,  "dB", "" },
        { "pan",        "Pan",       "Fader",      Kind::knob,    -1.0f,   1.0f,    0.0f,  "",   "" },
        { "volume",     "Fader",     "Fader",      Kind::fader, silentDb,  10.0f,   0.0f,  "dB", "" },
        { "mute",       "Mute",      "Fader",      Kind::toggle,   0.0f,   1.0f,    0.0f,  "",   "" },
        { "solo",       "Solo",      "Fader",      Kind::toggle,   0.0f,   1.0f,    0.0f,  "",   "" },
    }};

    return specs;
}

const std::array<ParamSpec, numMasterParams>& masterSpecs()
{
    static const std::array<ParamSpec, numMasterParams> specs {{
        { "plateDecay",    "Decay",         "Plate",          Kind::knob,     0.0f, 100.0f,   55.0f, "%",  "" },
        { "plateTone",     "Tone",          "Plate",          Kind::knob,     0.0f, 100.0f,   50.0f, "%",  "" },
        { "platePredelay", "Pre-delay",     "Plate",          Kind::knob,     0.0f, 100.0f,   20.0f, "ms", "" },
        { "plateReturn",   "Return",        "Plate",          Kind::knob,    offDb,   6.0f,    0.0f, "dB", "" },
        { "echoSync",      "Sync",          "Tape Echo",      Kind::toggle,   0.0f,   1.0f,    1.0f, "",   "" },
        { "echoNote",      "Time",          "Tape Echo",      Kind::choice,   0.0f,  10.0f,    6.0f, "",   "1/32|1/16T|1/16|1/16.|1/8T|1/8|1/8.|1/4T|1/4|1/4.|1/2" },
        { "echoTime",      "Time (ms)",     "Tape Echo",      Kind::knob,    20.0f, 1500.0f, 350.0f, "ms", "" },
        { "echoFeedback",  "Feedback",      "Tape Echo",      Kind::knob,     0.0f,  95.0f,   35.0f, "%",  "" },
        { "echoTone",      "Tone",          "Tape Echo",      Kind::knob,     0.0f, 100.0f,   50.0f, "%",  "" },
        { "echoWow",       "Wow",           "Tape Echo",      Kind::knob,     0.0f, 100.0f,   20.0f, "%",  "" },
        { "echoReturn",    "Return",        "Tape Echo",      Kind::knob,    offDb,   6.0f,    0.0f, "dB", "" },
        { "busOn",         "Bus Comp",      "Bus Compressor", Kind::toggle,   0.0f,   1.0f,    0.0f, "",   "" },
        { "busThreshold",  "Threshold",     "Bus Compressor", Kind::knob,   -30.0f,   0.0f,  -10.0f, "dB", "" },
        { "busRatio",      "Ratio",         "Bus Compressor", Kind::choice,   0.0f,   2.0f,    0.0f, "",   "2:1|4:1|10:1" },
        { "busAttack",     "Attack",        "Bus Compressor", Kind::choice,   0.0f,   5.0f,    4.0f, "",   "0.1 ms|0.3 ms|1 ms|3 ms|10 ms|30 ms" },
        { "busRelease",    "Release",       "Bus Compressor", Kind::choice,   0.0f,   4.0f,    4.0f, "",   "0.1 s|0.3 s|0.6 s|1.2 s|Auto" },
        { "busMakeup",     "Make-up",       "Bus Compressor", Kind::knob,     0.0f,  15.0f,    0.0f, "dB", "" },
        { "busMix",        "Mix",           "Bus Compressor", Kind::knob,     0.0f, 100.0f,  100.0f, "%",  "" },
        { "tapeOn",        "Tape",          "Tape Machine",   Kind::toggle,   0.0f,   1.0f,    0.0f, "",   "" },
        { "tapeDrive",     "Drive",         "Tape Machine",   Kind::knob,     0.0f, 100.0f,   30.0f, "%",  "" },
        { "tapeSpeed",     "Speed",         "Tape Machine",   Kind::choice,   0.0f,   1.0f,    0.0f, "",   "15 ips|30 ips" },
        { "tapeWow",       "Wow & Flutter", "Tape Machine",   Kind::knob,     0.0f, 100.0f,   15.0f, "%",  "" },
        { "tapeHiss",      "Hiss",          "Tape Machine",   Kind::toggle,   0.0f,   1.0f,    0.0f, "",   "" },
        { "masterVolume",  "Master",        "Master",         Kind::fader, silentDb,  10.0f,    0.0f, "dB", "" },
    }};

    return specs;
}

int stripIndexOf (const juce::String& id)
{
    const auto& specs = stripSpecs();

    for (size_t i = 0; i < specs.size(); ++i)
        if (id == specs[i].id)
            return static_cast<int> (i);

    return -1;
}

int masterIndexOf (const juce::String& id)
{
    const auto& specs = masterSpecs();

    for (size_t i = 0; i < specs.size(); ++i)
        if (id == specs[i].id)
            return static_cast<int> (i);

    return -1;
}

//==============================================================================
void Meter::measure (const float* left, const float* right, int numSamples, double sampleRate, float reductionDb) noexcept
{
    if (numSamples <= 0)
        return;

    double sumLeft = 0.0, sumRight = 0.0;
    float peakL = 0.0f, peakR = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto l = left[i], r = right[i];
        sumLeft += static_cast<double> (l) * l;
        sumRight += static_cast<double> (r) * r;
        peakL = juce::jmax (peakL, std::abs (l));
        peakR = juce::jmax (peakR, std::abs (r));
    }

    // A VU's swing: about 300 ms to settle.
    const auto follow = 1.0 - std::exp (-static_cast<double> (numSamples) / (0.3 * juce::jmax (1.0, sampleRate)));
    powerLeft += (sumLeft / numSamples - powerLeft) * follow;
    powerRight += (sumRight / numSamples - powerRight) * follow;

    vuLeft.store (static_cast<float> (10.0 * std::log10 (juce::jmax (1.0e-10, powerLeft))), std::memory_order_relaxed);
    vuRight.store (static_cast<float> (10.0 * std::log10 (juce::jmax (1.0e-10, powerRight))), std::memory_order_relaxed);
    storeMax (peakLeft, peakL);
    storeMax (peakRight, peakR);
    reduction.store (reductionDb, std::memory_order_relaxed);
}

void Meter::clear() noexcept
{
    powerLeft = powerRight = 0.0;
    vuLeft.store (-100.0f, std::memory_order_relaxed);
    vuRight.store (-100.0f, std::memory_order_relaxed);
    reduction.store (0.0f, std::memory_order_relaxed);
}

//==============================================================================
Strip::Strip()
{
}

void Strip::prepare (double rate, int maxBlockSize)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;

    for (auto* buffer : { &output, &scratch })
    {
        buffer->setSize (2, juce::jmax (4096, 2 * maxBlockSize), false, true, true);
        buffer->clear();
    }

    compressor.prepare (sampleRate);
    seenChanges = settings.getChanges() - 1;   // worked out again at the next block
    reset();
}

void Strip::reset() noexcept
{
    for (auto& side : states)
        side = {};

    compressor.reset();
    wasCompressing = false;
    fresh = true;   // the next block starts as the settings are, with no crossfade
    meter.clear();
}

Strip::Chain Strip::workOutChain() const noexcept
{
    const auto get = [this] (int index) { return static_cast<double> (settings.get (index)); };
    Chain next;

    next.trimGain = static_cast<float> (SonifyDsp::dbToGain (get (trim)) * (settings.isOn (phase) ? -1.0 : 1.0));

    // The preamp: a soft curve, a little lopsided (as a transformer and tubes are), level at the
    // bottom so only what's loud bends.
    const auto amount = get (drive) / 100.0;
    next.driving = amount > 0.0001;
    next.driveGain = static_cast<float> (1.0 + 12.0 * amount);
    next.driveBias = static_cast<float> (0.2 * amount);
    next.driveBiasOut = softClip (next.driveBias);
    next.driveNorm = 1.0f / (next.driveGain * softClipSlope (next.driveBias));

    next.hpfOn = get (hpf) > stripSpecs()[hpf].minimum + 0.5;
    next.eqOn = settings.isOn (eqOn);
    next.filters[0] = next.hpfOn ? SonifyDsp::Biquad::highPass (sampleRate, get (hpf), 0.707) : SonifyDsp::Biquad {};
    next.filters[1] = SonifyDsp::Biquad::lowShelf (sampleRate, get (lfFreq), get (lf));
    next.filters[2] = SonifyDsp::Biquad::peak (sampleRate, get (lmfFreq), 1.1, get (lmf));
    next.filters[3] = SonifyDsp::Biquad::peak (sampleRate, get (hmfFreq), 1.1, get (hmf));
    next.filters[4] = SonifyDsp::Biquad::highShelf (sampleRate, get (hfFreq), get (hf));
    return next;
}

float Strip::run (const Chain& chain, ChainState& state, float input) noexcept
{
    auto x = input * chain.trimGain;

    if (chain.driving)
    {
        x = (softClip (chain.driveGain * x + chain.driveBias) - chain.driveBiasOut) * chain.driveNorm;

        // The lopsided curve leaves a little DC: taken out at once.
        const auto y = static_cast<double> (x) - state.dcIn + 0.9995 * state.dcOut;
        state.dcIn = x;
        state.dcOut = y;
        x = static_cast<float> (y);
    }

    if (chain.hpfOn)
        x = state.filters[0].process (chain.filters[0], x);

    // Every band, a flat one too, so each keeps its memory up to date for when it moves.
    if (chain.eqOn)
        for (size_t f = 1; f < chain.filters.size(); ++f)
            x = state.filters[f].process (chain.filters[f], x);

    return x;
}

void Strip::process (const float* left, const float* right, int numSamples, Master& master, bool audible, bool settle) noexcept
{
    numSamples = juce::jmin (numSamples, output.getNumSamples(), master.getBlockSize());
    outputSamples = juce::jmax (0, numSamples);

    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    auto* outLeft = output.getWritePointer (0);
    auto* outRight = output.getWritePointer (1);
    bool crossfaded = false;

    // A change to the preamp or the filters plays both ways over this block, the old fading into
    // the new, so knobs and switches never click. A stage switched on starts from silence.
    if (settings.getChanges() != seenChanges)
    {
        seenChanges = settings.getChanges();
        const auto next = workOutChain();
        compressor.set (settings.get (threshold), settings.get (ratio), settings.isOn (fastAttack) ? 1.0f : 10.0f,
                        settings.get (release), 4.0f, settings.get (makeup));

        if (! fresh && ! settle)
        {
            const auto step = 1.0f / static_cast<float> (numSamples);

            for (int channel = 0; channel < 2; ++channel)
            {
                const auto* in = channel == 0 ? left : right;
                auto* out = channel == 0 ? outLeft : outRight;
                auto& oldState = states[static_cast<size_t> (channel)];
                auto newState = oldState;

                if (next.hpfOn && ! chain.hpfOn)
                    newState.filters[0].reset();

                if (next.eqOn && ! chain.eqOn)
                    for (size_t f = 1; f < newState.filters.size(); ++f)
                        newState.filters[f].reset();

                if (next.driving && ! chain.driving)
                    newState.dcIn = newState.dcOut = 0.0;

                for (int i = 0; i < numSamples; ++i)
                {
                    const auto was = run (chain, oldState, in[i]);
                    const auto now = run (next, newState, in[i]);
                    out[i] = was + (now - was) * step * static_cast<float> (i + 1);
                }

                oldState = newState;
            }

            crossfaded = true;
        }

        chain = next;
    }

    fresh = false;

    if (! crossfaded)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto* in = channel == 0 ? left : right;
            auto* out = channel == 0 ? outLeft : outRight;
            auto& state = states[static_cast<size_t> (channel)];

            for (int i = 0; i < numSamples; ++i)
                out[i] = run (chain, state, in[i]);
        }
    }

    // The compressor; switched in or out, it crossfades over the block.
    const bool compressing = settings.isOn (compOn);

    if (compressing != wasCompressing)
    {
        auto* copyLeft = scratch.getWritePointer (0);
        auto* copyRight = scratch.getWritePointer (1);
        juce::FloatVectorOperations::copy (copyLeft, outLeft, numSamples);
        juce::FloatVectorOperations::copy (copyRight, outRight, numSamples);

        if (compressing)
        {
            compressor.reset();
            compressor.prime (copyLeft, copyRight, numSamples);
        }

        compressor.process (copyLeft, copyRight, numSamples);
        const auto step = 1.0f / static_cast<float> (numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto g = compressing ? step * static_cast<float> (i + 1) : 1.0f - step * static_cast<float> (i + 1);
            outLeft[i] += (copyLeft[i] - outLeft[i]) * g;
            outRight[i] += (copyRight[i] - outRight[i]) * g;
        }
    }
    else if (compressing)
    {
        compressor.process (outLeft, outRight, numSamples);
    }

    wasCompressing = compressing;

    // Pan (a stereo source: the far side turns down, on a sine curve) and the fader.
    const auto faderGain = audible ? dbGain (settings.get (volume), silentDb) : 0.0f;
    const auto position = settings.get (pan);
    const auto curve = [] (float amount) { return static_cast<float> (std::cos (amount * SonifyDsp::pi * 0.5)); };
    const auto targetLeft = faderGain * (position > 0.0f ? curve (position) : 1.0f);
    const auto targetRight = faderGain * (position < 0.0f ? curve (-position) : 1.0f);
    const auto targetPlate = dbGain (settings.get (sendPlate), offDb);
    const auto targetEcho = dbGain (settings.get (sendEcho), offDb);

    if (settle)
    {
        lastLeftGain = targetLeft;
        lastRightGain = targetRight;
        lastPlate = targetPlate;
        lastEcho = targetEcho;
    }

    const auto count = static_cast<float> (numSamples);
    const auto stepLeft = (targetLeft - lastLeftGain) / count, stepRight = (targetRight - lastRightGain) / count;
    const auto stepPlate = (targetPlate - lastPlate) / count, stepEcho = (targetEcho - lastEcho) / count;
    const bool sendsPlate = targetPlate > 0.0f || lastPlate > 0.0f;
    const bool sendsEcho = targetEcho > 0.0f || lastEcho > 0.0f;

    auto* busLeft = master.bus (0);
    auto* busRight = master.bus (1);
    auto* plateLeft = master.plateSend (0);
    auto* plateRight = master.plateSend (1);
    auto* echoLeft = master.echoSend (0);
    auto* echoRight = master.echoSend (1);

    auto gainLeft = lastLeftGain, gainRight = lastRightGain, plate = lastPlate, echoGain = lastEcho;

    for (int i = 0; i < numSamples; ++i)
    {
        gainLeft += stepLeft;
        gainRight += stepRight;
        plate += stepPlate;
        echoGain += stepEcho;

        const auto l = outLeft[i] * gainLeft;
        const auto r = outRight[i] * gainRight;
        outLeft[i] = l;
        outRight[i] = r;
        busLeft[i] += l;
        busRight[i] += r;

        if (sendsPlate)
        {
            plateLeft[i] += l * plate;
            plateRight[i] += r * plate;
        }

        if (sendsEcho)
        {
            echoLeft[i] += l * echoGain;
            echoRight[i] += r * echoGain;
        }
    }

    lastLeftGain = targetLeft;
    lastRightGain = targetRight;
    lastPlate = targetPlate;
    lastEcho = targetEcho;

    meter.measure (outLeft, outRight, numSamples, sampleRate, compressing ? compressor.getReductionDb() : 0.0f);
}

//==============================================================================
/** The Plate: Jon Dattorro's plate reverb ("Effect Design, Part 1", 1997): a pre-delay, four
    diffusers, and a figure-eight tank of modulated allpasses, delays and damping, tapped seven
    times a side. Its lengths are the paper's, at 29761 Hz, scaled to the rate. */
class Master::Plate
{
public:
    void prepare (double rate)
    {
        sampleRate = rate;
        const auto scale = rate / 29761.0;
        const auto scaled = [scale] (int samples) { return juce::jmax (1, static_cast<int> (std::round (samples * scale))); };

        preDelay.prepare (static_cast<int> (0.11 * rate));
        diffusers[0].prepare (scaled (142));
        diffusers[1].prepare (scaled (107));
        diffusers[2].prepare (scaled (379));
        diffusers[3].prepare (scaled (277));

        excursion = static_cast<float> (16.0 * scale);
        leftModulated.prepare (scaled (672));
        rightModulated.prepare (scaled (908));
        leftModulated.line.prepare (scaled (672) + static_cast<int> (excursion) + 8);
        rightModulated.line.prepare (scaled (908) + static_cast<int> (excursion) + 8);
        leftDelay1.prepare (scaled (4453));
        leftDelay2.prepare (scaled (3720));
        rightDelay1.prepare (scaled (4217));
        rightDelay2.prepare (scaled (3163));
        leftDiffuser.prepare (scaled (1800));
        rightDiffuser.prepare (scaled (2656));
        lengths = { scaled (4453), scaled (3720), scaled (4217), scaled (3163) };

        const int taps[14] = { 266, 2974, 1913, 1996, 1990, 187, 1066, 353, 3627, 1228, 2673, 2111, 335, 121 };

        for (size_t i = 0; i < 14; ++i)
            tapAt[i] = scaled (taps[i]);

        inputCut.setCutoff (rate, 120.0);
        reset();
    }

    void reset() noexcept
    {
        for (auto* line : lines())
            line->clear();

        resetStates();
    }

    /** Starting afresh on the audio thread: the lines are cleared bit by bit (clearSome). */
    void beginClear() noexcept
    {
        for (auto* line : lines())
            line->beginClear();

        resetStates();
    }

    bool clearSome (int& budget) noexcept
    {
        bool done = true;

        for (auto* line : lines())
            done = line->clearSome (budget) && done;

        return done;
    }

    void resetStates() noexcept
    {
        bandwidthState = dampLeft = dampRight = 0.0f;
        leftOut = rightOut = 0.0f;
        inputCut.reset();
        lfoLeft.setRate (sampleRate, 1.0, 0.0);
        lfoRight.setRate (sampleRate, 1.0, 0.5 * SonifyDsp::pi);
    }

    void set (float decayPercent, float tonePercent, float preDelayMs) noexcept
    {
        decay = 0.2f + 0.75f * juce::jlimit (0.0f, 1.0f, decayPercent / 100.0f);
        const auto tone = juce::jlimit (0.0f, 1.0f, tonePercent / 100.0f);
        damping = 0.0005f + 0.7f * std::pow (1.0f - tone, 1.5f);
        bandwidth = 0.55f + 0.4495f * tone;
        preDelaySamples = juce::jlimit (0, preDelay.length() - 8, static_cast<int> (preDelayMs * 0.001f * static_cast<float> (sampleRate)));
    }

    /** The sends (stereo, heard in mono as a plate is fed) into the plate's stereo sound. */
    void process (const float* inLeft, const float* inRight, float* outL, float* outR, int numSamples) noexcept
    {
        const auto diffusion2 = juce::jlimit (0.25f, 0.5f, decay + 0.15f);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto mono = inputCut.highPass (0.5f * (inLeft[i] + inRight[i]));

            const auto delayed = preDelay.tap (preDelaySamples);
            preDelay.push (mono);

            bandwidthState += bandwidth * (delayed - bandwidthState);
            auto x = diffusers[0].process (bandwidthState, 0.75f);
            x = diffusers[1].process (x, 0.75f);
            x = diffusers[2].process (x, 0.625f);
            x = diffusers[3].process (x, 0.625f);

            const auto modLeft = excursion * lfoLeft.next();
            const auto modRight = excursion * lfoRight.next();

            // The left half of the tank, fed by the right's end, and the right by the left's.
            const auto intoLeft = x + decay * rightOut;
            const auto intoRight = x + decay * leftOut;

            const auto a = leftModulated.process (intoLeft, -0.7f, modLeft);
            const auto b = leftDelay1.tap (lengths[0] - 1);
            leftDelay1.push (a);
            dampLeft = b + damping * (dampLeft - b);
            const auto c = leftDiffuser.process (dampLeft * decay, diffusion2);
            const auto newLeftOut = leftDelay2.tap (lengths[1] - 1);
            leftDelay2.push (c);

            const auto d = rightModulated.process (intoRight, -0.7f, modRight);
            const auto e = rightDelay1.tap (lengths[2] - 1);
            rightDelay1.push (d);
            dampRight = e + damping * (dampRight - e);
            const auto f = rightDiffuser.process (dampRight * decay, diffusion2);
            const auto newRightOut = rightDelay2.tap (lengths[3] - 1);
            rightDelay2.push (f);

            leftOut = newLeftOut;
            rightOut = newRightOut;

            const auto t = [this] (int n) { return tapAt[static_cast<size_t> (n)]; };

            outL[i] = 0.6f * (rightDelay1.tap (t (0)) + rightDelay1.tap (t (1)) - rightDiffuser.line.tap (t (2)) + rightDelay2.tap (t (3))
                              - leftDelay1.tap (t (4)) - leftDiffuser.line.tap (t (5)) - leftDelay2.tap (t (6)));
            outR[i] = 0.6f * (leftDelay1.tap (t (7)) + leftDelay1.tap (t (8)) - leftDiffuser.line.tap (t (9)) + leftDelay2.tap (t (10))
                              - rightDelay1.tap (t (11)) - rightDiffuser.line.tap (t (12)) - rightDelay2.tap (t (13)));
        }
    }

private:
    std::array<DelayLine*, 13> lines() noexcept
    {
        return { &preDelay, &diffusers[0].line, &diffusers[1].line, &diffusers[2].line, &diffusers[3].line,
                 &leftModulated.line, &rightModulated.line, &leftDiffuser.line, &rightDiffuser.line,
                 &leftDelay1, &leftDelay2, &rightDelay1, &rightDelay2 };
    }

    double sampleRate = 48000.0;
    DelayLine preDelay;
    std::array<Allpass, 4> diffusers;
    Allpass leftModulated, rightModulated, leftDiffuser, rightDiffuser;
    DelayLine leftDelay1, leftDelay2, rightDelay1, rightDelay2;
    std::array<int, 4> lengths {};
    std::array<int, 14> tapAt {};
    OnePole inputCut;
    float excursion = 16.0f;
    float decay = 0.6f, damping = 0.2f, bandwidth = 0.9f;
    int preDelaySamples = 0;
    float bandwidthState = 0.0f, dampLeft = 0.0f, dampRight = 0.0f, leftOut = 0.0f, rightOut = 0.0f;
    Lfo lfoLeft, lfoRight;
};

//==============================================================================
/** The Tape Echo: one tape loop, its repeats darker and rounder each time round, its speed
    wandering a little (wow and flutter), a little wider on the right. Time changes glide, as a
    tape's speed does. */
class Master::Echo
{
public:
    void prepare (double rate)
    {
        sampleRate = rate;
        line.prepare (static_cast<int> (3.1 * rate));
        spread = static_cast<float> (0.006 * rate);
        lowCut.setCutoff (rate, 120.0);
        smoothing = static_cast<float> (1.0 - std::exp (-1.0 / (0.12 * rate)));
        reset();
    }

    void reset() noexcept
    {
        line.clear();
        resetStates();
    }

    void beginClear() noexcept
    {
        line.beginClear();
        resetStates();
    }

    bool clearSome (int& budget) noexcept     { return line.clearSome (budget); }

    void resetStates() noexcept
    {
        lowCut.reset();
        highCut.reset();
        snap = true;
        wow.setRate (sampleRate, 0.6);
        flutter.setRate (sampleRate, 7.0);
    }

    void set (double seconds, float feedbackPercent, float tonePercent, float wowPercent) noexcept
    {
        target = static_cast<float> (juce::jlimit (0.005, 3.0, seconds) * sampleRate);

        // A fresh loop starts at its time; afterwards the time glides.
        if (snap)
        {
            current = target;
            snap = false;
        }

        feedback = juce::jlimit (0.0f, 0.95f, feedbackPercent / 100.0f);
        highCut.setCutoff (sampleRate, 1500.0 * std::pow (8.0, juce::jlimit (0.0f, 1.0f, tonePercent / 100.0f)));
        const auto wander = juce::jlimit (0.0f, 1.0f, wowPercent / 100.0f);
        wowDepth = static_cast<float> (0.003 * sampleRate) * wander;
        flutterDepth = static_cast<float> (0.0003 * sampleRate) * wander;
    }

    void process (const float* inLeft, const float* inRight, float* outL, float* outR, int numSamples) noexcept
    {
        const auto longest = static_cast<float> (line.length() - 4) - spread - wowDepth - flutterDepth;

        for (int i = 0; i < numSamples; ++i)
        {
            current += (target - current) * smoothing;

            const auto wander = wowDepth * wow.next() + flutterDepth * flutter.next();
            const auto delay = juce::jlimit (2.0f, longest, current + wander);

            const auto repeat = line.tapFractional (delay - 1.0f);
            const auto wide = line.tapFractional (delay - 1.0f + spread);

            // Into the loop: the sends and the repeat, darker, without its lows, rounded off.
            auto w = 0.5f * (inLeft[i] + inRight[i]) + feedback * repeat;
            w = lowCut.highPass (highCut.lowPass (w));
            w = softClip (1.2f * w) / 1.2f;
            line.push (w);

            outL[i] = repeat;
            outR[i] = wide;
        }
    }

private:
    double sampleRate = 48000.0;
    DelayLine line;
    OnePole lowCut, highCut;
    float target = 0.0f, current = 0.0f, smoothing = 0.001f, spread = 0.0f;
    float feedback = 0.35f, wowDepth = 0.0f, flutterDepth = 0.0f;
    bool snap = true;
    Lfo wow, flutter;
};

//==============================================================================
/** The Tape Machine: the mix recorded to tape and played back. Driven harder, it rounds the
    peaks off (the highs first, as tape's emphasis makes them), with tape's low bump and its
    gentle top, its speed wandering a little, and its hiss when asked. */
class Master::Tape
{
public:
    void prepare (double rate)
    {
        sampleRate = rate;

        for (auto& line : lines)
            line.prepare (static_cast<int> (0.01 * rate));

        baseDelay = static_cast<float> (0.0015 * rate);
        glide = static_cast<float> (1.0 - std::exp (-1.0 / (0.01 * rate)));
        reset();
    }

    void reset() noexcept
    {
        for (auto& line : lines)
            line.clear();

        for (auto& side : states)
            for (auto& state : side)
                state.reset();

        snapGains = true;
        wow.setRate (sampleRate, 0.5);
        flutter.setRate (sampleRate, 6.3);
    }

    void set (float drivePercent, int speed, float wowPercent, bool withHiss) noexcept
    {
        const auto amount = juce::jlimit (0.0f, 1.0f, drivePercent / 100.0f);
        const auto driveDb = 18.0 * amount;
        targetIn = static_cast<float> (SonifyDsp::dbToGain (driveDb));
        targetOut = static_cast<float> (SonifyDsp::dbToGain (-0.9 * driveDb));

        // A fresh tape starts at its drive; afterwards the drive glides (no zipper as its knob turns).
        if (snapGains)
        {
            inGain = targetIn;
            outGain = targetOut;
            snapGains = false;
        }

        const bool fast = speed > 0;
        filters[0] = SonifyDsp::Biquad::highShelf (sampleRate, 3000.0, 4.0);                     // emphasis
        filters[1] = SonifyDsp::Biquad::highShelf (sampleRate, 3000.0, -4.0);                    // and back
        filters[2] = SonifyDsp::Biquad::peak (sampleRate, fast ? 110.0 : 55.0, fast ? 1.0 : 1.1, fast ? 2.0 : 2.5);   // head bump
        filters[3] = SonifyDsp::Biquad::highPass (sampleRate, fast ? 35.0 : 22.0, 0.707);
        filters[4] = SonifyDsp::Biquad::lowPass (sampleRate, fast ? 21000.0 : 18000.0, 0.707);

        const auto wander = juce::jlimit (0.0f, 1.0f, wowPercent / 100.0f);
        wowDepth = static_cast<float> (0.0008 * sampleRate) * wander;
        flutterDepth = static_cast<float> (0.00008 * sampleRate) * wander;
        hiss = withHiss;
    }

    void process (float* left, float* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto delay = baseDelay + wowDepth * wow.next() + flutterDepth * flutter.next();
            inGain += (targetIn - inGain) * glide;
            outGain += (targetOut - outGain) * glide;

            for (int channel = 0; channel < 2; ++channel)
            {
                auto* data = channel == 0 ? left : right;
                auto& state = states[static_cast<size_t> (channel)];
                auto x = data[i] * inGain;

                x = state[0].process (filters[0], x);
                x = softClip (x);
                x = state[1].process (filters[1], x);
                x = state[2].process (filters[2], x);
                x = state[3].process (filters[3], x);
                x = state[4].process (filters[4], x);
                x *= outGain;

                auto& line = lines[static_cast<size_t> (channel)];
                line.push (x);
                auto played = line.tapFractional (delay);

                if (hiss)
                {
                    noise ^= noise << 13;
                    noise ^= noise >> 17;
                    noise ^= noise << 5;
                    played += 0.0006f * (static_cast<float> (noise) / 2147483648.0f - 1.0f);
                }

                data[i] = played;
            }
        }
    }

private:
    double sampleRate = 48000.0;
    std::array<DelayLine, 2> lines;
    std::array<SonifyDsp::Biquad, 5> filters {};
    std::array<std::array<SonifyDsp::BiquadState, 5>, 2> states {};
    float inGain = 1.0f, outGain = 1.0f, targetIn = 1.0f, targetOut = 1.0f, glide = 0.002f;
    float baseDelay = 64.0f, wowDepth = 0.0f, flutterDepth = 0.0f;
    bool snapGains = true;
    bool hiss = false;
    juce::uint32 noise = 0x9e3779b9u;
    Lfo wow, flutter;
};

//==============================================================================
Master::Master()
    : plate (std::make_unique<Plate>()), echo (std::make_unique<Echo>()), tape (std::make_unique<Tape>())
{
}

Master::~Master() = default;

void Master::prepare (double rate, int maxBlockSize)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    const auto size = juce::jmax (4096, 2 * maxBlockSize);

    for (auto* buffer : { &busBuffer, &plateBuffer, &echoBuffer, &returnBuffer })
    {
        buffer->setSize (2, size, false, true, true);
        buffer->clear();
    }

    plate->prepare (sampleRate);
    echo->prepare (sampleRate);
    tape->prepare (sampleRate);
    busCompressor.prepare (sampleRate);
    seenChanges = settings.getChanges() - 1;
    reset();
}

void Master::reset() noexcept
{
    plate->reset();
    echo->reset();
    tape->reset();
    busCompressor.reset();
    clearing = false;
    busWasOn = settings.isOn (busOn);
    tapeWasOn = settings.isOn (tapeOn);
    meter.clear();
}

void Master::startFresh() noexcept
{
    plate->beginClear();
    echo->beginClear();
    tape->reset();              // its lines are a few milliseconds long
    busCompressor.reset();
    clearing = true;
    busWasOn = settings.isOn (busOn);
    tapeWasOn = settings.isOn (tapeOn);
    meter.clear();
}

template <typename Process>
void Master::crossfade (float* left, float* right, int numSamples, bool on, bool switching, Process&& process) noexcept
{
    auto* copyLeft = returnBuffer.getWritePointer (0);
    auto* copyRight = returnBuffer.getWritePointer (1);
    juce::FloatVectorOperations::copy (copyLeft, left, numSamples);
    juce::FloatVectorOperations::copy (copyRight, right, numSamples);
    process (copyLeft, copyRight);

    if (! switching)
        return;   // out, and staying out: only its memory moved on

    const auto step = 1.0f / static_cast<float> (numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = on ? step * static_cast<float> (i + 1) : 1.0f - step * static_cast<float> (i + 1);
        left[i] += (copyLeft[i] - left[i]) * g;
        right[i] += (copyRight[i] - right[i]) * g;
    }
}

void Master::begin (int numSamples) noexcept
{
    numSamples = juce::jmin (numSamples, busBuffer.getNumSamples());

    if (numSamples <= 0)
        return;

    for (auto* buffer : { &busBuffer, &plateBuffer, &echoBuffer })
        buffer->clear (0, numSamples);
}

void Master::refresh() noexcept
{
    seenChanges = settings.getChanges();
    const auto get = [this] (int index) { return settings.get (index); };

    plate->set (get (plateDecay), get (plateTone), get (platePredelay));

    SonifyDsp::Compressor::Settings glue;
    glue.thresholdDb = get (busThreshold);
    glue.ratio = std::array<float, 3> { 2.0f, 4.0f, 10.0f } [static_cast<size_t> (juce::jlimit (0, 2, static_cast<int> (get (busRatio))))];
    glue.attackMs = std::array<float, 6> { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f } [static_cast<size_t> (juce::jlimit (0, 5, static_cast<int> (get (busAttack))))];

    // Auto: a slower, smoother release that follows the music (the RMS detector's own swing).
    const auto releaseChoice = juce::jlimit (0, 4, static_cast<int> (get (busRelease)));
    glue.releaseMs = std::array<float, 5> { 100.0f, 300.0f, 600.0f, 1200.0f, 400.0f } [static_cast<size_t> (releaseChoice)];
    glue.rms = releaseChoice == 4;
    glue.kneeDb = 3.0f;
    glue.makeupDb = get (busMakeup);
    glue.mix = get (busMix) / 100.0f;
    busCompressor.setSettings (glue);

    tape->set (get (tapeDrive), static_cast<int> (get (tapeSpeed)), get (tapeWow), settings.isOn (tapeHiss));
}

void Master::finish (float* left, float* right, int numSamples, double bpm, bool settle) noexcept
{
    numSamples = juce::jmin (numSamples, busBuffer.getNumSamples());

    if (numSamples <= 0 || left == nullptr || right == nullptr)
        return;

    if (settings.getChanges() != seenChanges)
        refresh();

    // The echo's time: a note at the song's tempo, or milliseconds.
    {
        static constexpr std::array<double, 11> quarters { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };
        const auto note = quarters[static_cast<size_t> (juce::jlimit (0, 10, static_cast<int> (settings.get (echoNote))))];
        const auto tempo = juce::jlimit (20.0, 999.0, bpm > 0.0 ? bpm : 120.0);
        const auto seconds = settings.isOn (echoSync) ? note * 60.0 / tempo : settings.get (echoTime) / 1000.0;
        echo->set (seconds, settings.get (echoFeedback), settings.get (echoTone), settings.get (echoWow));
    }

    auto* busLeft = busBuffer.getWritePointer (0);
    auto* busRight = busBuffer.getWritePointer (1);
    auto* returnLeft = returnBuffer.getWritePointer (0);
    auto* returnRight = returnBuffer.getWritePointer (1);

    // The returns into the mix, each at its level.
    const auto addReturn = [&] (float target, float& last)
    {
        if (settle)
            last = target;

        const auto step = (target - last) / static_cast<float> (numSamples);
        auto gain = last;

        for (int i = 0; i < numSamples; ++i)
        {
            gain += step;
            busLeft[i] += returnLeft[i] * gain;
            busRight[i] += returnRight[i] * gain;
        }

        last = target;
    };

    // Starting afresh, the Plate and the Echo are silent until their memories are cleared (a
    // little each block, a few milliseconds in all).
    if (clearing)
    {
        int budget = 1 << 16;
        clearing = ! (plate->clearSome (budget) && echo->clearSome (budget));
    }
    else
    {
        plate->process (plateBuffer.getReadPointer (0), plateBuffer.getReadPointer (1), returnLeft, returnRight, numSamples);
        addReturn (dbGain (settings.get (plateReturn), offDb), lastPlateReturn);

        echo->process (echoBuffer.getReadPointer (0), echoBuffer.getReadPointer (1), returnLeft, returnRight, numSamples);
        addReturn (dbGain (settings.get (echoReturn), offDb), lastEchoReturn);
    }

    // The bus compressor, then the tape. Switched out, each still runs on a copy of the bus (and
    // isn't heard), so switching it in or out is a crossfade over the block between two sounds
    // already going: no click, no tail from long ago.
    const auto stage = [&] (bool on, bool& wasOn, auto&& process)
    {
        if (on && wasOn)
            process (busLeft, busRight);
        else
            crossfade (busLeft, busRight, numSamples, on, wasOn != on, process);

        wasOn = on;
    };

    const bool busCompressing = settings.isOn (busOn);
    stage (busCompressing, busWasOn, [this, numSamples] (float* l, float* r) { busCompressor.process (l, r, numSamples); });
    stage (settings.isOn (tapeOn), tapeWasOn, [this, numSamples] (float* l, float* r) { tape->process (l, r, numSamples); });

    // The master fader.
    const auto target = dbGain (settings.get (masterVolume), silentDb);

    if (settle)
        lastMasterGain = target;

    const auto step = (target - lastMasterGain) / static_cast<float> (numSamples);
    auto gain = lastMasterGain;

    for (int i = 0; i < numSamples; ++i)
    {
        gain += step;
        left[i] = busLeft[i] * gain;
        right[i] = busRight[i] * gain;
    }

    lastMasterGain = target;
    meter.measure (left, right, numSamples, sampleRate, busCompressing ? static_cast<float> (busCompressor.getReductionDb()) : 0.0f);
}
}
