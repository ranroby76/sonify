// D:\workspace\Fanan_web_hub\src\zodiak\I3QFilters.h
//
// A multi-mode filter with 17 types: zero-delay-feedback state-variable filters,
// a saturating ladder, a diode "acid" ladder, a vowel formant filter, combs and a phaser.
// Also the 2x oversampler the voice drive runs through.
//
// ACID (type 16) is the TB-303 as ReBirth RB-338 plays it, so a ReBirth song's
// knob moves sound as they did there:
//      the cutoff     the filter sits at cutoff x (0.6 + 0.4 x reso): resonance
//                     pulls it up
//      the resonance  a 2-pole resonator whose Q grows with the resonance AND
//                     with the cutoff - about 1.7 at most while the filter is
//                     low, up to 29 once it opens: the squelch that turns into
//                     a whistle as it sweeps up
//      the bite       a high-pass follows the cutoff (at 15% of it, at all of
//                     it with the resonance full), so a resonant note is thin
//                     and biting, and the level rises as the filter opens
// The coefficients glide to each new setting over a few samples (no zipper).
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace flt
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
            rbS = rbY1 = rbY2 = rbO = 0.0f;
            rbRamp = 0;
            rbSet = false;
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
                    // ReBirth's 303 (see the top).  `position`: where the cutoff is on its
                    // 0..1 scale (1 = 17.64 kHz, a factor 1602 below it = 0).
                    const float t = res;
                    const float position = std::clamp (1.0f + std::log (cutoff / 17640.0f) / std::log (1602.0f), 0.0f, 1.0f);
                    const float wn = std::min (cutoff * (0.6f + 0.4f * t) / sr, 0.45f);        // cycles a sample

                    const float i8 = position * 1023.0f / 8.0f;
                    const float open = std::clamp (i8 * i8 * 0.000125f - 0.3f, 0.0f, 1.0f);    // 0 low .. 1 open
                    const float rows = t * 7.0f;
                    const float q = open * (std::pow (1.78f, rows) * 0.52f) + (1.0f - open) * (rows / 6.0f + 0.5f);

                    float target[4];
                    target[0] = 2.0f * std::exp (-stellaPi * wn / q) * std::cos (stellaTwoPi * wn);
                    target[1] = std::exp (-stellaTwoPi * wn / q);
                    target[3] = std::exp (-stellaTwoPi * wn * (0.85f * t + 0.15f));
                    target[2] = (1.0f - target[3]) * (target[1] + 1.0f - target[0]) / ((std::min (q, 3.0f) + 1.0f) * 0.8f);

                    if (! rbSet)
                    {
                        for (int i = 0; i < 4; ++i) { rbC[i] = target[i]; rbStep[i] = 0.0f; }
                        rbRamp = 0;
                        rbSet = true;
                    }
                    else
                    {
                        for (int i = 0; i < 4; ++i) rbStep[i] = (target[i] - rbC[i]) / (float) rbRampLength;
                        rbRamp = rbRampLength;
                    }
                    rbDc = std::exp (-stellaTwoPi * 100.0f / sr);
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
                    if (rbRamp > 0)
                    {
                        for (int i = 0; i < 4; ++i) rbC[i] += rbStep[i];
                        --rbRamp;
                    }
                    // The leaky integrator, the resonator, then the difference: a high-pass at the
                    // integrator's corner in front of a 2-pole low-pass
                    rbS = rbS * rbC[3] + x;
                    float y = rbS * rbC[2] + rbY1 * rbC[0] - rbY2 * rbC[1];
                    y = std::clamp (y, -64.0f, 64.0f);
                    rbO = (y - rbY1) + rbO * rbDc;
                    rbY2 = rbY1;
                    rbY1 = std::fabs (y) < 1.0e-20f ? 0.0f : y;
                    if (std::fabs (rbS) < 1.0e-20f) rbS = 0.0f;
                    if (std::fabs (rbO) < 1.0e-20f) rbO = 0.0f;
                    return rbO * rbMakeup;
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
        float fbLow = 0.0f;

        // ACID: the coefficients now (2-pole feedback x 2, input gain, the integrator's
        // feedback), their step while they glide, and the state
        static constexpr int rbRampLength = 16;
        static constexpr float rbMakeup = 1.0f;
        float rbC[4] { 0.0f, 0.0f, 0.0f, 0.0f }, rbStep[4] {};
        float rbS = 0.0f, rbY1 = 0.0f, rbY2 = 0.0f, rbO = 0.0f, rbDc = 0.98f;
        int rbRamp = 0;
        bool rbSet = false;
    };

    // A 2x oversampler for the voice drive: two chains of all-pass sections (a half-band pair).
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
