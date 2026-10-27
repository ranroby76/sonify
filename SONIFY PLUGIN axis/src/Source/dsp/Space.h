#pragma once

#include <juce_dsp/juce_dsp.h>

namespace axis::dsp
{

/** SPACE — plate / hall / spring reverb.

    Four damped comb filters into two allpasses per channel, with a pre-delay
    in front. SPRING adds a short dispersive allpass chain, which is what gives
    a real spring tank its boing; PLATE runs denser and brighter; HALL runs
    longer with more damping.

    DUCK pulls the tail down while you're playing and lets it back up in the
    gaps, so the reverb never sits on top of the notes.
*/
class Space
{
public:
    enum class Type { plate = 0, hall, spring };

    struct Params
    {
        Type  type     = Type::spring;
        float preDlyMs = 24.0f;
        float decay    = 46.0f;
        float size     = 58.0f;
        float damp     = 62.0f;
        float width    = 80.0f;
        bool  shimmer  = false;
        bool  freeze   = false;
        bool  duck     = true;
        float mix      = 28.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate  = spec.sampleRate;
        numChannels = juce::jmax (1, (int) spec.numChannels);

        dry.setSize (numChannels, (int) spec.maximumBlockSize);

        preDelay.resize ((size_t) numChannels);
        for (auto& d : preDelay)
        {
            d.prepare (spec);
            d.setMaximumDelayInSamples ((int) (0.3 * sampleRate) + 8);
        }

        combs.assign ((size_t) numChannels, {});
        allpasses.assign ((size_t) numChannels, {});
        springs.assign ((size_t) numChannels, {});

        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int c = 0; c < numCombs; ++c)
                combs[(size_t) ch][(size_t) c].prepare (sampleRate, baseComb[c] * (ch == 0 ? 1.0 : 1.021));

            for (int a = 0; a < numAllpass; ++a)
                allpasses[(size_t) ch][(size_t) a].prepare (sampleRate, baseAllpass[a] * (ch == 0 ? 1.0 : 1.017));

            for (int s = 0; s < numSprings; ++s)
                springs[(size_t) ch][(size_t) s].prepare (sampleRate, baseSpring[s] * (ch == 0 ? 1.0 : 1.011));
        }

        reset();
    }

    void reset()
    {
        for (auto& d : preDelay) d.reset();
        for (auto& chCombs : combs)      for (auto& c : chCombs) c.reset();
        for (auto& chAps   : allpasses)  for (auto& a : chAps)   a.reset();
        for (auto& chSpr   : springs)    for (auto& s : chSpr)   s.reset();
        duckEnv = 0.0f;
        dry.clear();
    }

    void setParams (const Params& p)
    {
        params = p;

        float sizeScale = juce::jmap (p.size, 0.0f, 100.0f, 0.55f, 1.85f);
        float fb        = juce::jmap (p.decay, 0.0f, 100.0f, 0.62f, 0.94f);
        float dampAmt   = juce::jmap (p.damp, 0.0f, 100.0f, 0.02f, 0.78f);

        switch (p.type)
        {
            case Type::plate:  sizeScale *= 0.78f; dampAmt *= 0.70f; break;
            case Type::hall:   sizeScale *= 1.32f; fb = juce::jmin (0.965f, fb * 1.04f); break;
            case Type::spring: sizeScale *= 0.62f; dampAmt *= 1.15f; break;
        }

        if (p.freeze)
        {
            fb      = 0.999f;
            dampAmt = 0.01f;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int c = 0; c < numCombs; ++c)
                combs[(size_t) ch][(size_t) c].set (sizeScale, fb, dampAmt);

            for (int a = 0; a < numAllpass; ++a)
                allpasses[(size_t) ch][(size_t) a].set (sizeScale, 0.5f);

            for (int s = 0; s < numSprings; ++s)
                springs[(size_t) ch][(size_t) s].set (1.0f, 0.62f);
        }

        preDelaySamples = p.preDlyMs * 0.001f * static_cast<float> (sampleRate);
        duckCoeff = 1.0f - std::exp (-1.0f / (0.030f * static_cast<float> (sampleRate)));
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = juce::jmin (numChannels, buffer.getNumChannels());
        const int numSm = buffer.getNumSamples();

        if (dry.getNumChannels() < numCh || dry.getNumSamples() < numSm)
            dry.setSize (numCh, numSm, false, false, true);

        for (int ch = 0; ch < numCh; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, numSm);

        const float mixAmt   = params.mix * 0.01f;
        const float widthAmt = params.width * 0.01f;
        const bool  useSpring = (params.type == Type::spring);

        for (int i = 0; i < numSm; ++i)
        {
            // duck from the dry input, not the wet tail
            float det = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                det = juce::jmax (det, std::abs (dry.getSample (ch, i)));

            duckEnv += (det - duckEnv) * duckCoeff;

            const float duckGain = params.duck
                ? juce::jlimit (0.28f, 1.0f, 1.0f - duckEnv * 2.2f)
                : 1.0f;

            float wet[2] = { 0.0f, 0.0f };

            for (int ch = 0; ch < numCh && ch < 2; ++ch)
            {
                auto& pd = preDelay[(size_t) ch];
                pd.pushSample (0, dry.getSample (ch, i));
                float x = pd.popSample (0, preDelaySamples);

                float sum = 0.0f;
                for (int c = 0; c < numCombs; ++c)
                    sum += combs[(size_t) ch][(size_t) c].process (x);

                sum *= 0.25f;

                for (int a = 0; a < numAllpass; ++a)
                    sum = allpasses[(size_t) ch][(size_t) a].process (sum);

                if (useSpring)
                    for (int s = 0; s < numSprings; ++s)
                        sum = springs[(size_t) ch][(size_t) s].process (sum);

                wet[ch] = sum * duckGain;
            }

            // mid/side width on the tail only
            if (numCh > 1)
            {
                const float mid  = (wet[0] + wet[1]) * 0.5f;
                const float side = (wet[0] - wet[1]) * 0.5f * widthAmt;
                wet[0] = mid + side;
                wet[1] = mid - side;
            }

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                d[i] = dry.getSample (ch, i) * (1.0f - mixAmt)
                     + wet[juce::jmin (ch, 1)] * mixAmt;
            }
        }
    }

private:
    // ---- building blocks ----
    struct Comb
    {
        void prepare (double sr, double ms)
        {
            maxLen = (int) (sr * ms * 0.001 * 2.0) + 4;
            buf.assign ((size_t) maxLen, 0.0f);
            baseLen = (int) (sr * ms * 0.001);
            len = baseLen;
            idx = 0;
        }

        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); store = 0.0f; idx = 0; }

        void set (float sizeScale, float feedback, float damping)
        {
            len  = juce::jlimit (4, maxLen - 1, (int) (baseLen * sizeScale));
            fb   = feedback;
            damp = damping;
        }

        float process (float x) noexcept
        {
            const float y = buf[(size_t) idx];
            store = y * (1.0f - damp) + store * damp;
            buf[(size_t) idx] = x + store * fb;

            if (++idx >= len) idx = 0;
            return y;
        }

        std::vector<float> buf;
        int   maxLen = 0, baseLen = 0, len = 0, idx = 0;
        float fb = 0.8f, damp = 0.3f, store = 0.0f;
    };

    struct Allpass
    {
        void prepare (double sr, double ms)
        {
            maxLen = (int) (sr * ms * 0.001 * 2.0) + 4;
            buf.assign ((size_t) maxLen, 0.0f);
            baseLen = (int) (sr * ms * 0.001);
            len = baseLen;
            idx = 0;
        }

        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); idx = 0; }

        void set (float sizeScale, float gain)
        {
            len = juce::jlimit (4, maxLen - 1, (int) (baseLen * sizeScale));
            g   = gain;
        }

        float process (float x) noexcept
        {
            const float bufOut = buf[(size_t) idx];
            const float y      = -x + bufOut;
            buf[(size_t) idx]  = x + bufOut * g;

            if (++idx >= len) idx = 0;
            return y;
        }

        std::vector<float> buf;
        int   maxLen = 0, baseLen = 0, len = 0, idx = 0;
        float g = 0.5f;
    };

    static constexpr int numCombs   = 4;
    static constexpr int numAllpass = 2;
    static constexpr int numSprings = 3;

    static constexpr double baseComb[numCombs]       = { 29.7, 37.1, 41.1, 43.7 };
    static constexpr double baseAllpass[numAllpass]  = { 5.0, 1.7 };
    static constexpr double baseSpring[numSprings]   = { 4.2, 3.1, 2.3 };

    double sampleRate  = 44100.0;
    int    numChannels = 2;
    Params params;

    std::vector<juce::dsp::DelayLine<float>> preDelay;
    std::vector<std::array<Comb, numCombs>>      combs;
    std::vector<std::array<Allpass, numAllpass>> allpasses;
    std::vector<std::array<Allpass, numSprings>> springs;

    juce::AudioBuffer<float> dry;
    float preDelaySamples = 1000.0f, duckEnv = 0.0f, duckCoeff = 0.01f;
};

} // namespace axis::dsp
