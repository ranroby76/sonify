// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakInserts.h
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
//  ZodiakInserts.h  —  the mixer's own VST FX: WAH and PHASER.
//
//  Tomahawk's wah and phaser, as I3Q's Effects module plays them (I3QEffects.cpp),
//  taken out on their own so any channel (and the master) can put them in its
//  VST FX list, next to plugins loaded from file.
//
//      WAH      a resonant band-pass swept by the signal's level and an LFO
//      PHASER   four all-pass stages with feedback, their corner swept by an LFO
//
//  Each one's settings are atomics the windows write; the audio thread reads
//  them once a block.  Plain C++ (no JUCE).
//==============================================================================

namespace zodiak::inserts
{
    enum Kind : int { kWah = 0, kPhaser = 1, kNumBuiltIns };

    inline const char* kindName (int k) noexcept   { return k == kWah ? "Wah" : k == kPhaser ? "Phaser" : ""; }
    inline const char* kindId (int k) noexcept     { return k == kWah ? "wah" : k == kPhaser ? "phaser" : ""; }

    static constexpr int kMaxParams = 6;

    struct ParamSpec { const char* id; const char* name; float min, max, def; const char* unit; float skew; };

    inline int numParams (int k) noexcept   { return k == kWah ? 6 : k == kPhaser ? 4 : 0; }

    inline const ParamSpec& param (int k, int i) noexcept
    {
        static const ParamSpec wah[6] = {
            { "sens",  "Sensitivity", 0.0f,  100.0f,   50.0f, "%",  1.0f },
            { "rate",  "Rate",        0.05f, 8.0f,     1.0f,  "Hz", 0.4f },
            { "lfo",   "LFO Depth",   0.0f,  100.0f,   0.0f,  "%",  1.0f },
            { "freq",  "Frequency",   80.0f, 1500.0f,  400.0f, "Hz", 0.5f },
            { "q",     "Q",           0.0f,  100.0f,   60.0f, "%",  1.0f },
            { "mix",   "Mix",         0.0f,  100.0f,  100.0f, "%",  1.0f } };
        static const ParamSpec phaser[4] = {
            { "rate",  "Rate",        0.05f, 4.0f,     1.6f,  "Hz", 0.5f },
            { "depth", "Depth",       0.0f,  100.0f,   80.0f, "%",  1.0f },
            { "fb",    "Feedback",    0.0f,  100.0f,   50.0f, "%",  1.0f },
            { "mix",   "Mix",         0.0f,  100.0f,   50.0f, "%",  1.0f } };
        if (k == kWah)    return wah[std::clamp (i, 0, 5)];
        return phaser[std::clamp (i, 0, 3)];
    }

    //==========================================================================
    //  One built-in insert: its settings and its sound
    //==========================================================================
    class BuiltIn
    {
    public:
        explicit BuiltIn (int k) : kind (k)
        {
            for (int i = 0; i < kMaxParams; ++i)
                values[(size_t) i].store (i < numParams (kind) ? param (kind, i).def : 0.0f);
        }

        int getKind() const noexcept            { return kind; }
        std::atomic<float>& value (int i)       { return values[(size_t) std::clamp (i, 0, kMaxParams - 1)]; }

        void prepare (double sampleRate)
        {
            sr = (float) (sampleRate > 0.0 ? sampleRate : 48000.0);
            reset();
        }

        void reset() noexcept
        {
            for (int c = 0; c < 2; ++c)
            {
                wahLow[c] = wahBand[c] = phaserFb[c] = 0.0f;
                for (auto& st : phaser[c]) st = {};
            }
            phaserLfo = wahLfo = wahEnv = 0.0f;
        }

        void process (float* L, float* R, int n) noexcept
        {
            if (kind == kWah) processWah (L, R, n);
            else              processPhaser (L, R, n);
        }

    private:
        static constexpr float twoPi = 6.28318530718f, pi = 3.14159265359f, halfPi = 1.57079632679f;

        struct PhaserStage
        {
            float x1 = 0.0f, y1 = 0.0f;

            float tick (float x, float a) noexcept
            {
                const float y = a * x + x1 - a * y1;
                x1 = x;
                y1 = std::fabs (y) < 1.0e-20f ? 0.0f : y;
                return y1;
            }
        };

        float v (int i) const noexcept   { return values[(size_t) i].load (std::memory_order_relaxed); }

        void processWah (float* L, float* R, int n) noexcept
        {
            const float wahSense = v (0) * 0.01f;
            const float wahInc = v (1) / sr;
            const float wahDepth = v (2) * 0.01f;
            const float wahBase = v (3);
            const float wahRes = std::clamp (1.4f - v (4) * 0.01f * 1.3f, 0.1f, 1.4f);
            const float wahMix = v (5) * 0.01f;
            if (wahMix <= 0.0f) return;
            const float wahDry = std::cos (wahMix * halfPi), wahWet = std::sin (wahMix * halfPi) * wahRes * 2.0f;
            const float wahAttack = std::exp (-1.0f / (0.005f * sr)), wahRelease = std::exp (-1.0f / (0.120f * sr));

            for (int i = 0; i < n; ++i)
            {
                float x[2] { L[i], R[i] };
                const float rect = std::fabs (0.5f * (x[0] + x[1]));
                wahEnv = rect + (rect > wahEnv ? wahAttack : wahRelease) * (wahEnv - rect);
                const float sweep = 0.5f + 0.5f * std::sin (twoPi * wahLfo);
                wahLfo += wahInc;
                if (wahLfo >= 1.0f) wahLfo -= 1.0f;

                const float fc = std::clamp (wahBase + wahSense * wahEnv * 2600.0f * 4.0f + wahDepth * sweep * 2600.0f, 80.0f, 0.45f * sr);
                const float g = std::tan (pi * fc / sr);
                const float a1 = 1.0f / (1.0f + g * (g + wahRes));

                for (int c = 0; c < 2; ++c)
                {
                    const float hp = (x[c] - (g + wahRes) * wahBand[c] - wahLow[c]) * a1;
                    const float bp = g * hp + wahBand[c];
                    wahBand[c] = g * hp + bp;
                    const float lp = g * bp + wahLow[c];
                    wahLow[c] = g * bp + lp;
                    if (std::fabs (wahBand[c]) < 1.0e-20f) wahBand[c] = 0.0f;
                    if (std::fabs (wahLow[c]) < 1.0e-20f) wahLow[c] = 0.0f;
                    x[c] = x[c] * wahDry + bp * wahWet;
                }
                L[i] = x[0];
                R[i] = x[1];
            }
        }

        void processPhaser (float* L, float* R, int n) noexcept
        {
            const float phaserInc = v (0) / sr;
            const float phaserDepth = v (1) * 0.01f;
            const float phaserFeedback = v (2) * 0.01f * 0.9f;
            const float phaserMix = v (3) * 0.01f;
            if (phaserMix <= 0.0f) return;
            const float phaserDry = std::cos (phaserMix * halfPi), phaserWet = std::sin (phaserMix * halfPi);

            for (int i = 0; i < n; ++i)
            {
                float x[2] { L[i], R[i] };
                const float level = 0.5f + 0.5f * std::sin (twoPi * phaserLfo);
                phaserLfo += phaserInc;
                if (phaserLfo >= 1.0f) phaserLfo -= 1.0f;

                const float fc = std::min (200.0f * std::pow (10.0f, phaserDepth * level), 0.45f * sr);
                const float t = std::tan (pi * fc / sr);
                const float a = (t - 1.0f) / (t + 1.0f);

                for (int c = 0; c < 2; ++c)
                {
                    float y = x[c] + phaserFb[c] * phaserFeedback;
                    for (auto& st : phaser[c])
                        y = st.tick (y, a);
                    phaserFb[c] = std::fabs (y) < 1.0e-20f ? 0.0f : y;
                    x[c] = x[c] * phaserDry + y * phaserWet;
                }
                L[i] = x[0];
                R[i] = x[1];
            }
        }

        const int kind;
        std::array<std::atomic<float>, kMaxParams> values;
        float sr = 48000.0f;
        float phaserLfo = 0.0f, phaserFb[2] {};
        PhaserStage phaser[2][4];
        float wahLfo = 0.0f, wahEnv = 0.0f, wahLow[2] {}, wahBand[2] {};
    };
}
