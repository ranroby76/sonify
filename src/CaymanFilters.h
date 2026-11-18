// D:\workspace\Fanan_web_hub\src\CaymanFilters.h
//
// Cayman's extra filter types, borrowed from the Plugins Creator project "My Plugin" (its modules/Filters.h).
//
// A multi-mode filter with 17 types: zero-delay-feedback state-variable filters,
// a saturating ladder, a diode "acid" ladder, a vowel formant filter, combs and a phaser.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace caymanflt
{
    constexpr int numTypes = 17;

    inline const char* const names[numTypes]
    {
        "LP 12", "LP 24", "HP 12", "HP 24", "BP 12", "BP 24", "Notch", "Peak",
        "Low Shelf", "High Shelf", "Dirty LP", "Ladder", "Formant", "Comb +", "Comb -", "Phaser", "Acid"
    };

    inline float softClip (float x) noexcept
    {
        x = std::clamp (x, -3.0f, 3.0f);
        return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
    }

    struct Svf
    {
        float g = 0.1f, k = 1.4f, a1 = 0, a2 = 0, a3 = 0, m0 = 0, m1 = 0, m2 = 1;
        float ic1 = 0, ic2 = 0;

        void set (float gIn, float kIn) noexcept
        {
            g = gIn; k = kIn;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void mix (float a, float b, float c) noexcept   { m0 = a; m1 = b; m2 = c; }

        float tick (float v0) noexcept
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return m0 * v0 + m1 * v1 + m2 * v2;
        }

        void clear() noexcept   { ic1 = ic2 = 0.0f; }
    };

    class Filter
    {
    public:
        void prepare (double sampleRate)
        {
            sr = (float) sampleRate;
            int size = 1;
            while (size < (int) (sr / 18.0f) + 8)
                size <<= 1;
            delay.assign ((size_t) size, 0.0f);
            mask = size - 1;
            reset();
        }

        void reset() noexcept
        {
            for (auto& s : svf)
                s.clear();
            for (auto& s : ladder)
                s = 0.0f;
            std::fill (delay.begin(), delay.end(), 0.0f);
            writePos = 0;
            damp = last = fbLow = 0.0f;
        }

        void setType (int t) noexcept
        {
            t = std::clamp (t, 0, numTypes - 1);
            if (t != type)
            {
                type = t;
                reset();
            }
        }

        // cutoff in Hz, res 0..1. Call every few samples, not every sample.
        void setParams (float cutoff, float res) noexcept
        {
            const float nyq = sr * 0.49f;
            cutoff = std::clamp (cutoff, 20.0f, nyq * 0.95f);
            res = std::clamp (res, 0.0f, 1.0f);
            const float w = stellaPi * cutoff / sr;
            const float g = std::tan (w);
            const float k = 2.0f - 1.96f * res;            // damping: 2 (no peak) .. 0.04 (screams)

            switch (type)
            {
                case 0: case 1: case 10:
                    for (auto& s : svf) { s.set (g, type == 1 ? 2.0f - 1.6f * res : k); s.mix (0, 0, 1); }
                    if (type == 1) svf[1].set (g, 1.4f);
                    break;
                case 2: case 3:
                    for (auto& s : svf) { s.set (g, type == 3 ? 2.0f - 1.6f * res : k); s.mix (1, -s.k, -1); }
                    if (type == 3) { svf[1].set (g, 1.4f); svf[1].mix (1, -1.4f, -1); }
                    break;
                case 4: case 5:
                {
                    const float kb = 2.0f - 1.9f * res;
                    for (auto& s : svf) { s.set (g, kb); s.mix (0, kb, 0); }   // unity gain at the centre
                    break;
                }
                case 6:
                    svf[0].set (g, 2.0f - 1.8f * res); svf[0].mix (1, -svf[0].k, 0);
                    break;
                case 7:
                {
                    const float A = std::pow (10.0f, (6.0f + 12.0f * res) / 40.0f);
                    svf[0].set (g, 1.0f / (1.2f * A)); svf[0].mix (1, svf[0].k * (A * A - 1.0f), 0);
                    break;
                }
                case 8:
                {
                    const float A = std::pow (10.0f, (6.0f + 12.0f * res) / 40.0f);
                    svf[0].set (g / std::sqrt (A), 1.4f); svf[0].mix (1, 1.4f * (A - 1.0f), A * A - 1.0f);
                    break;
                }
                case 9:
                {
                    const float A = std::pow (10.0f, (6.0f + 12.0f * res) / 40.0f);
                    svf[0].set (g * std::sqrt (A), 1.4f); svf[0].mix (A * A, 1.4f * (1.0f - A) * A, 1.0f - A * A);
                    break;
                }
                case 11:
                    G = g / (1.0f + g);
                    ladderK = 3.9f * res;
                    break;
                case 12:
                {
                    // The cutoff sweeps the vowels A, E, I, O, U.
                    static const float vowels[5][3] { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 280, 2250, 2900 },
                                                      { 450, 800, 2830 }, { 325, 700, 2530 } };
                    float m = std::log2 (cutoff / 20.0f) / 9.966f * 4.0f;
                    m = std::clamp (m, 0.0f, 3.999f);
                    const int i = (int) m;
                    const float f = m - (float) i;
                    const float kf = 0.35f - 0.3f * res;
                    const float gains[3] { 1.0f, 0.6f, 0.35f };

                    for (int b = 0; b < 3; ++b)
                    {
                        const float hz = std::min (vowels[i][b] + f * (vowels[i + 1][b] - vowels[i][b]), nyq * 0.9f);
                        svf[b].set (std::tan (stellaPi * hz / sr), kf);
                        svf[b].mix (0, kf * gains[b] * 2.0f, 0);
                    }
                    break;
                }
                case 13: case 14:
                    combDelay = std::clamp (sr / cutoff, 2.0f, (float) mask - 2.0f);
                    combFb = (type == 13 ? 1.0f : -1.0f) * (0.3f + 0.67f * res);
                    combDamp = std::exp (-stellaTwoPi * std::min (12000.0f, nyq * 0.8f) / sr);
                    break;
                case 15:
                {
                    const float spread[4] { 0.5f, 0.85f, 1.4f, 2.3f };
                    for (int b = 0; b < 4; ++b)
                    {
                        const float hz = std::min (cutoff * spread[b], nyq * 0.9f);
                        svf[b].set (std::tan (stellaPi * hz / sr), 1.2f);
                        svf[b].mix (1, -2.4f, 0);     // all-pass
                    }
                    phaserFb = 0.85f * res;
                    break;
                }
                case 16:
                {
                    // The diode ladder, solved stage by stage (see process).
                    const float gd = std::min (g, 2.5f);
                    diodeA[3] = gd / (1.0f + gd);
                    diodeD[3] = 1.0f / (1.0f + gd);
                    for (int i = 2; i >= 0; --i)
                    {
                        diodeD[i] = 1.0f / (1.0f + 2.0f * gd - gd * diodeA[i + 1]);
                        diodeA[i] = gd * diodeD[i];
                    }
                    diodeG = gd;
                    diodeK = 20.0f * std::pow (res, 1.6f);
                    diodeHp = 1.0f - std::exp (-stellaTwoPi * 70.0f / sr);
                    break;
                }
                default: break;
            }
        }

        float process (float x) noexcept
        {
            switch (type)
            {
                case 0: case 2: case 6: case 7: case 8: case 9:
                    return svf[0].tick (x);
                case 1: case 3: case 5:
                    return svf[1].tick (svf[0].tick (x));
                case 4:
                    return svf[0].tick (x);
                case 10:
                {
                    // Resonance pushed through saturation: gritty, squelchy.
                    const float y = svf[0].tick (softClip (x * 1.5f));
                    svf[0].ic1 = softClip (svf[0].ic1 * 0.7f) / 0.7f;
                    return y * 0.8f;
                }
                case 11:
                {
                    const float g2 = G * G;
                    // Each stage: v = (in - s) G ; y = v + s ; s = y + v. Solved for the feedback.
                    const float S = G * G * G * (ladder[0] * (1.0f - G)) + G * G * (ladder[1] * (1.0f - G))
                                  + G * (ladder[2] * (1.0f - G)) + ladder[3] * (1.0f - G);
                    const float G4 = g2 * g2;
                    float u = (x * (1.0f + 0.5f * ladderK) - ladderK * S) / (1.0f + ladderK * G4);
                    u = softClip (u);
                    float in = u;
                    for (int i = 0; i < 4; ++i)
                    {
                        const float v = (in - ladder[i]) * G;
                        const float y = v + ladder[i];
                        ladder[i] = y + v;
                        in = y;
                    }
                    return in;
                }
                case 12:
                    return svf[0].tick (x) + svf[1].tick (x) + svf[2].tick (x);
                case 13: case 14:
                {
                    float rp = (float) writePos - combDelay;
                    if (rp < 0.0f) rp += (float) (mask + 1);
                    const int i0 = (int) rp;
                    const float f = rp - (float) i0;
                    const float d0 = delay[(size_t) (i0 & mask)], d1 = delay[(size_t) ((i0 + 1) & mask)];
                    const float delayed = d0 + f * (d1 - d0);
                    damp = delayed + combDamp * (damp - delayed);
                    const float y = x + combFb * damp;
                    delay[(size_t) writePos] = softClip (y);
                    writePos = (writePos + 1) & mask;
                    return y * 0.5f;
                }
                case 15:
                {
                    float y = x + phaserFb * last;
                    for (int b = 0; b < 4; ++b)
                        y = svf[b].tick (y);
                    last = softClip (y);
                    return 0.5f * (x + y);
                }
                case 16:
                {
                    // Four coupled stages:  y1 = g (x - 2 y1 + y2) + s1 ... y4 = g (y3 - y4) + s4.
                    // Each stage is written as y[i] = a[i] y[i-1] + b[i], from the last one back,
                    // which gives the output as A x + B and the feedback without a delay.
                    float b[4];
                    b[3] = ladder[3] * diodeD[3];
                    for (int i = 2; i >= 0; --i)
                        b[i] = (diodeG * b[i + 1] + ladder[i]) * diodeD[i];

                    const float A = diodeA[0] * diodeA[1] * diodeA[2] * diodeA[3];
                    const float B = diodeA[3] * (diodeA[2] * (diodeA[1] * b[0] + b[1]) + b[2]) + b[3];

                    float u = (x * 1.6f - diodeK * (B - fbLow)) / (1.0f + diodeK * A);
                    u = softClip (u);

                    float y = u;
                    for (int i = 0; i < 4; ++i)
                    {
                        y = diodeA[i] * y + b[i];
                        ladder[i] = 2.0f * y - ladder[i];
                    }

                    // The feedback path loses its deep lows: the thin, biting resonance.
                    fbLow += (y - fbLow) * diodeHp;
                    if (std::fabs (fbLow) < 1.0e-20f) fbLow = 0.0f;
                    return y * (1.0f + 0.42f * diodeK);
                }
                default:
                    return x;
            }
        }

    private:
        static constexpr float stellaPi = 3.14159265358979323846f;
        static constexpr float stellaTwoPi = 2.0f * stellaPi;

        float sr = 48000.0f;
        int type = 0;
        Svf svf[4];
        float ladder[4] {}, G = 0.1f, ladderK = 0.0f;
        std::vector<float> delay;
        int mask = 0, writePos = 0;
        float combDelay = 100.0f, combFb = 0.0f, combDamp = 0.0f, damp = 0.0f;
        float phaserFb = 0.0f, last = 0.0f;
        float diodeA[4] {}, diodeD[4] {}, diodeG = 0.1f, diodeK = 0.0f, diodeHp = 0.01f, fbLow = 0.0f;
    };

    // A 2x oversampler for the drive: two chains of all-pass sections (a half-band pair).
    struct Oversampler
    {
        static constexpr int numCoefs = 6;

        struct Chain
        {
            float x1 = 0.0f, y1 = 0.0f;

            float tick (float x, float c) noexcept
            {
                const float y = c * (x - y1) + x1;
                x1 = x;
                y1 = std::fabs (y) < 1.0e-20f ? 0.0f : y;
                return y1;
            }
        };

        // x in, two samples at twice the rate out.
        void up (float x, float& first, float& second) noexcept
        {
            float a = x, b = x;
            for (int i = 0; i < numCoefs; i += 2) a = upChain[i].tick (a, coef (i));
            for (int i = 1; i < numCoefs; i += 2) b = upChain[i].tick (b, coef (i));
            first = a;
            second = b;
        }

        // Two samples at twice the rate in, one out.
        float down (float first, float second) noexcept
        {
            float a = second, b = first;
            for (int i = 0; i < numCoefs; i += 2) a = downChain[i].tick (a, coef (i));
            for (int i = 1; i < numCoefs; i += 2) b = downChain[i].tick (b, coef (i));
            return 0.5f * (a + b);
        }

        static float coef (int i) noexcept
        {
            static const float c[numCoefs] { 0.0682040760f, 0.2402703580f, 0.4486762359f,
                                             0.6411223671f, 0.7999975637f, 0.9344822355f };
            return c[i];
        }

        Chain upChain[numCoefs], downChain[numCoefs];
    };
}
