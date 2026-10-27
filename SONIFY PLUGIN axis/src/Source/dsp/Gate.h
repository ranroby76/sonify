#pragma once

#include <juce_dsp/juce_dsp.h>

namespace axis::dsp
{

/** Adaptive noise gate.

    Sidechain is high-passed so palm-muted low end doesn't hold the gate open,
    and ADAPT mode tracks the noise floor during quiet passages so the
    threshold follows the rig instead of a fixed number.
*/
class Gate
{
public:
    enum class Mode { fast = 0, smooth, adapt };

    struct Params
    {
        Mode  mode      = Mode::adapt;
        float threshDb  = -46.0f;
        float depth     = 88.0f;   // 0..100
        float releaseMs = 160.0f;
        float holdMs    = 40.0f;
        float scHpfHz   = 120.0f;
        bool  hissFilter = true;
        bool  autoLearn  = false;
        float range     = 100.0f;  // 0..100
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        scHpf.prepare (spec);
        scHpf.setType (juce::dsp::StateVariableTPTFilterType::highpass);

        hiss.prepare (spec);
        hiss.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        hiss.setCutoffFrequency (9500.0f);

        reset();
    }

    void reset()
    {
        scHpf.reset();
        hiss.reset();
        env      = 0.0f;
        gain     = 1.0f;
        holdLeft = 0;
        floorEst = 1.0e-4f;
    }

    void setParams (const Params& p)
    {
        params = p;
        scHpf.setCutoffFrequency (juce::jlimit (20.0f, 2000.0f, p.scHpfHz));

        const float atkMs = (p.mode == Mode::fast) ? 0.4f : 2.0f;
        attackCoeff  = coeff (atkMs);
        releaseCoeff = coeff (juce::jmax (1.0f, p.releaseMs));
        holdSamples  = static_cast<int> (p.holdMs * 0.001 * sampleRate);
        detCoeff     = coeff ((p.mode == Mode::fast) ? 1.0f : 6.0f);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels();
        const int numSm = buffer.getNumSamples();

        const float threshLin = juce::Decibels::decibelsToGain (effectiveThreshold());
        const float floorGain = 1.0f - (params.depth * 0.01f) * (params.range * 0.01f);

        for (int i = 0; i < numSm; ++i)
        {
            // ---- detector: mono sum, high-passed ----
            float sc = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                sc += buffer.getReadPointer (ch)[i];

            sc /= static_cast<float> (juce::jmax (1, numCh));
            sc  = scHpf.processSample (0, sc);

            const float rect = std::abs (sc);
            env += (rect - env) * detCoeff;

            // ---- adaptive floor tracking ----
            if (params.mode == Mode::adapt || params.autoLearn)
            {
                if (env < floorEst) floorEst += (env - floorEst) * 0.0008f;
                else                floorEst += (env - floorEst) * 0.000012f;
            }

            // ---- gate decision ----
            const bool open = env > threshLin;

            if (open)
                holdLeft = holdSamples;
            else if (holdLeft > 0)
                --holdLeft;

            const float target = (open || holdLeft > 0) ? 1.0f : floorGain;
            const float c      = (target > gain) ? attackCoeff : releaseCoeff;
            gain += (target - gain) * c;

            // ---- apply ----
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                float x = d[i] * gain;

                if (params.hissFilter && gain < 0.999f)
                    x = hiss.processSample (ch, x);

                d[i] = x;
            }
        }
    }

    float currentGain() const noexcept { return gain; }

private:
    float coeff (float ms) const
    {
        return 1.0f - std::exp (-1.0f / (0.001f * ms * static_cast<float> (sampleRate)));
    }

    float effectiveThreshold() const
    {
        if (params.mode != Mode::adapt && ! params.autoLearn)
            return params.threshDb;

        // sit a fixed margin above the tracked noise floor, but never below
        // the user's own setting by more than 12 dB
        const float floorDb = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, floorEst));
        return juce::jlimit (params.threshDb - 12.0f, 0.0f, floorDb + 9.0f);
    }

    double sampleRate = 44100.0;
    Params params;

    juce::dsp::StateVariableTPTFilter<float> scHpf, hiss;

    float env = 0.0f, gain = 1.0f, floorEst = 1.0e-4f;
    float attackCoeff = 0.5f, releaseCoeff = 0.01f, detCoeff = 0.1f;
    int   holdSamples = 0, holdLeft = 0;
};

} // namespace axis::dsp
