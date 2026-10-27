#pragma once

#include <juce_dsp/juce_dsp.h>

namespace axis::dsp
{

/** Feed-forward compressor with three detector characters.

    OPTO  — program-dependent release, soft knee, slow
    FET   — fast attack, aggressive, hard-ish knee
    VCA   — clean and linear, exactly what the controls say
*/
class Comp
{
public:
    enum class Circuit { opto = 0, fet, vca };

    struct Params
    {
        Circuit circuit  = Circuit::opto;
        float threshDb   = -18.0f;
        float ratio      = 4.0f;
        float makeupDb   = 6.0f;
        float attackMs   = 14.0f;
        float releaseMs  = 280.0f;
        bool  autoGain   = true;
        bool  scHpf      = false;
        bool  parallel   = false;
        float mix        = 100.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        dry.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);

        hpf.prepare (spec);
        hpf.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        hpf.setCutoffFrequency (120.0f);

        reset();
    }

    void reset()
    {
        hpf.reset();
        env = 0.0f;
        gr  = 1.0f;
        dry.clear();
    }

    void setParams (const Params& p)
    {
        params = p;

        float atk = p.attackMs, rel = p.releaseMs;

        switch (p.circuit)
        {
            case Circuit::opto: atk *= 1.6f;  rel *= 1.9f;  knee = 9.0f; break;
            case Circuit::fet:  atk *= 0.35f; rel *= 0.75f; knee = 3.0f; break;
            case Circuit::vca:  knee = 6.0f;  break;
        }

        attackCoeff  = coeff (juce::jmax (0.05f, atk));
        releaseCoeff = coeff (juce::jmax (5.0f,  rel));
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels();
        const int numSm = buffer.getNumSamples();

        if (dry.getNumChannels() < numCh || dry.getNumSamples() < numSm)
            dry.setSize (numCh, numSm, false, false, true);

        for (int ch = 0; ch < numCh; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, numSm);

        const float mixAmt = (params.parallel ? juce::jmin (0.5f, params.mix * 0.01f)
                                              : params.mix * 0.01f);
        float makeup = juce::Decibels::decibelsToGain (params.makeupDb);

        if (params.autoGain)
        {
            // approximate the loss a given threshold/ratio costs and give it back
            const float est = (0.0f - params.threshDb) * (1.0f - 1.0f / params.ratio) * 0.45f;
            makeup *= juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 18.0f, est));
        }

        for (int i = 0; i < numSm; ++i)
        {
            float sc = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                sc += buffer.getReadPointer (ch)[i];

            sc /= static_cast<float> (juce::jmax (1, numCh));

            if (params.scHpf)
                sc = hpf.processSample (0, sc);

            const float rect = std::abs (sc);
            const float c    = (rect > env) ? attackCoeff : releaseCoeff;
            env += (rect - env) * c;

            const float lvlDb = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, env));
            const float over  = lvlDb - params.threshDb;

            float reduceDb = 0.0f;

            if (over > knee * 0.5f)
                reduceDb = over * (1.0f - 1.0f / params.ratio);
            else if (over > -knee * 0.5f)
            {
                const float t = over + knee * 0.5f;
                reduceDb = (1.0f - 1.0f / params.ratio) * (t * t) / (2.0f * knee);
            }

            gr = juce::Decibels::decibelsToGain (-reduceDb);
            lastGrDb = -reduceDb;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                const float wet = d[i] * gr * makeup;
                d[i] = dry.getSample (ch, i) * (1.0f - mixAmt) + wet * mixAmt;
            }
        }
    }

    float gainReductionDb() const noexcept { return lastGrDb; }

private:
    float coeff (float ms) const
    {
        return 1.0f - std::exp (-1.0f / (0.001f * ms * static_cast<float> (sampleRate)));
    }

    double sampleRate = 44100.0;
    Params params;

    juce::dsp::StateVariableTPTFilter<float> hpf;
    juce::AudioBuffer<float> dry;

    float env = 0.0f, gr = 1.0f, lastGrDb = 0.0f;
    float attackCoeff = 0.1f, releaseCoeff = 0.01f, knee = 6.0f;
};
} // namespace axis::dsp
