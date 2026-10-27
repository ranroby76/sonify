#pragma once

#include <juce_dsp/juce_dsp.h>

namespace axis::dsp
{

/** Transparent overdrive.

    The chain is: input gain -> body shelf -> asymmetric soft clip ->
    bite shelf -> tone tilt -> level, with a dry/wet mix across the whole lot.

    DYNAMIC TOUCH drives the clipper harder when you dig in and backs off when
    you play softly, which is what makes a real pedal feel alive rather than
    like a fixed transfer curve.
*/
class Drive
{
public:
    enum class Voice { warm = 0, open, tight };

    struct Params
    {
        Voice voice   = Voice::warm;
        float tone    = 42.0f;
        float gain    = 64.0f;
        float level   = 55.0f;
        float body    = 38.0f;
        float bite    = 71.0f;
        bool  dynamic = true;
        bool  stereo  = false;
        float mix     = 100.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = (int) spec.numChannels;

        dry.setSize (numChannels, (int) spec.maximumBlockSize);

        bodyFilter.resize ((size_t) numChannels);
        biteFilter.resize ((size_t) numChannels);
        tiltLow.resize    ((size_t) numChannels);
        tiltHigh.resize   ((size_t) numChannels);

        reset();
    }

    void reset()
    {
        for (auto& f : bodyFilter) f.reset();
        for (auto& f : biteFilter) f.reset();
        for (auto& f : tiltLow)    f.reset();
        for (auto& f : tiltHigh)   f.reset();

        env = 0.0f;
        dry.clear();
    }

    void setParams (const Params& p)
    {
        params = p;

        // voice sets the clipper's asymmetry and the pre-emphasis corner
        switch (p.voice)
        {
            case Voice::warm:  asym = 0.22f; preHz = 480.0f;  drivePad = 1.0f;  break;
            case Voice::open:  asym = 0.08f; preHz = 320.0f;  drivePad = 0.86f; break;
            case Voice::tight: asym = 0.30f; preHz = 760.0f;  drivePad = 1.18f; break;
        }

        const float bodyDb = juce::jmap (p.body, 0.0f, 100.0f, -6.0f, 9.0f);
        const float biteDb = juce::jmap (p.bite, 0.0f, 100.0f, -5.0f, 8.0f);
        const float tiltDb = juce::jmap (p.tone, 0.0f, 100.0f, -7.0f, 7.0f);

        auto bodyCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 180.0, 0.7, juce::Decibels::decibelsToGain (bodyDb));
        auto biteCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 2600.0, 0.7, juce::Decibels::decibelsToGain (biteDb));
        auto tiltLoCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 700.0, 0.6, juce::Decibels::decibelsToGain (-tiltDb));
        auto tiltHiCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 1800.0, 0.6, juce::Decibels::decibelsToGain (tiltDb));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            bodyFilter[(size_t) ch].coefficients = bodyCoeffs;
            biteFilter[(size_t) ch].coefficients = biteCoeffs;
            tiltLow   [(size_t) ch].coefficients = tiltLoCoeffs;
            tiltHigh  [(size_t) ch].coefficients = tiltHiCoeffs;
        }

        preCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                    * preHz / static_cast<float> (sampleRate));
        envCoeff = 1.0f - std::exp (-1.0f / (0.012f * static_cast<float> (sampleRate)));
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = juce::jmin (numChannels, buffer.getNumChannels());
        const int numSm = buffer.getNumSamples();

        if (dry.getNumChannels() < numCh || dry.getNumSamples() < numSm)
            dry.setSize (numCh, numSm, false, false, true);

        for (int ch = 0; ch < numCh; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, numSm);

        const float driveAmt = juce::jmap (params.gain, 0.0f, 100.0f, 1.0f, 48.0f) * drivePad;
        const float outGain  = juce::jmap (params.level, 0.0f, 100.0f, 0.0f, 1.6f);
        const float mixAmt   = params.mix * 0.01f;

        for (int i = 0; i < numSm; ++i)
        {
            // one shared envelope so both channels breathe together unless
            // TRUE STEREO is on
            float det = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                det = juce::jmax (det, std::abs (buffer.getReadPointer (ch)[i]));

            env += (det - env) * envCoeff;

            const float touch = params.dynamic
                ? juce::jlimit (0.55f, 1.65f, 0.75f + env * 2.6f)
                : 1.0f;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                float x = d[i];

                x = bodyFilter[(size_t) ch].processSample (x);
                x = clip (x * driveAmt * touch);
                x = biteFilter[(size_t) ch].processSample (x);
                x = tiltLow  [(size_t) ch].processSample (x);
                x = tiltHigh [(size_t) ch].processSample (x);

                const float wet = x * outGain;
                d[i] = dry.getSample (ch, i) * (1.0f - mixAmt) + wet * mixAmt;
            }
        }
    }

private:
    /** Asymmetric soft clip. The offset pushes the positive and negative
        halves through different parts of the curve, which is where the even
        harmonics — the "warmth" — come from. */
    float clip (float x) const noexcept
    {
        const float biased = x + asym;
        const float y      = std::tanh (biased) - std::tanh (asym);
        return y * 0.72f;
    }

    double sampleRate = 44100.0;
    int    numChannels = 2;
    Params params;

    using Filter = juce::dsp::IIR::Filter<float>;

    std::vector<Filter> bodyFilter, biteFilter, tiltLow, tiltHigh;
    juce::AudioBuffer<float> dry;

    float asym = 0.22f, preHz = 480.0f, drivePad = 1.0f;
    float preCoeff = 0.1f, envCoeff = 0.01f, env = 0.0f;
};

} // namespace axis::dsp
