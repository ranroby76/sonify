// C:\workspace\SONIFY PLUGIN natura\src\ModelDsp.h
#pragma once
//==============================================================================
//  ModelDsp.h  -  the small building blocks every Natura model is made of.
//
//  Pure C++, no JUCE: the audio thread runs it, and the offline test harness
//  renders it from the same code.  Nothing here allocates after prepare().
//==============================================================================
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace natura::dsp
{
    constexpr double kPi    = 3.14159265358979323846;
    constexpr double kTwoPi = 6.28318530717958647692;

    template <typename T> inline T clampv (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
    inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

    /** Loop gain per period that gives a T60 of t60 seconds at f0. */
    inline float loopGainForT60 (float t60, float f0)
    {
        t60 = std::max (0.005f, t60);
        f0  = std::max (1.0f, f0);
        return std::pow (10.0f, -3.0f / (t60 * f0));
    }

    /** Per-sample multiplier that gives a T60 of t60 seconds. */
    inline float sampleGainForT60 (float t60, double sr)
    {
        t60 = std::max (0.001f, t60);
        return (float) std::pow (10.0, -3.0 / ((double) t60 * sr));
    }

    //==========================================================================
    //  Fast white noise, -1..1
    //==========================================================================
    struct Noise
    {
        std::uint32_t s = 0x12345678u;
        void seed (std::uint32_t v) { s = v ? v : 0x9E3779B9u; }
        float next()
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) (std::int32_t) s * (1.0f / 2147483648.0f);
        }
        float unit() { return 0.5f * (next() + 1.0f); }   // 0..1
    };

    //==========================================================================
    //  Fractional delay line, 4-point Lagrange read.
    //  tap(d): the sample written d samples ago (d = 1 is the last write).
    //==========================================================================
    struct FracDelay
    {
        std::vector<float> buf;
        int mask = 0, w = 0;

        void prepare (int maxSamples)
        {
            int n = 16;
            while (n < maxSamples + 8) n <<= 1;
            buf.assign ((size_t) n, 0.0f);
            mask = n - 1;
            w = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        int  capacity() const { return mask - 6; }

        void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }

        float at (int d) const { return buf[(size_t) ((w - d) & mask)]; }

        float tap (float d) const
        {
            d = clampv (d, 2.0f, (float) capacity());
            const int   i = (int) d;
            const float t = d - (float) i + 1.0f;          // 1 .. 2
            const float x0 = at (i - 1), x1 = at (i), x2 = at (i + 1), x3 = at (i + 2);
            const float tm1 = t - 1.0f, tm2 = t - 2.0f, tm3 = t - 3.0f;
            const float h0 = -tm1 * tm2 * tm3 * (1.0f / 6.0f);
            const float h1 =  t   * tm2 * tm3 * 0.5f;
            const float h2 = -t   * tm1 * tm3 * 0.5f;
            const float h3 =  t   * tm1 * tm2 * (1.0f / 6.0f);
            return h0 * x0 + h1 * x1 + h2 * x2 + h3 * x3;
        }

        /** |H| of the Lagrange read at w for delay d.  In a loop that runs
            thousands of periods a second, even 0.5% loss per pass is fatal to
            the decay, so loop gains are divided by this. */
        static float lagrangeMag (float d, double w)
        {
            const int   i = (int) std::max (2.0f, d);
            const float t = std::max (2.0f, d) - (float) i + 1.0f;
            const float tm1 = t - 1.0f, tm2 = t - 2.0f, tm3 = t - 3.0f;
            const float h[4] = { -tm1 * tm2 * tm3 * (1.0f / 6.0f), t * tm2 * tm3 * 0.5f,
                                 -t * tm1 * tm3 * 0.5f, t * tm1 * tm2 * (1.0f / 6.0f) };
            std::complex<double> acc (0.0, 0.0);
            for (int k = 0; k < 4; ++k) acc += (double) h[k] * std::polar (1.0, -w * (double) k);
            return (float) std::abs (acc);
        }

        /** Linear read, used where Lagrange's slight HF lift is unwanted. */
        float tapLin (float d) const
        {
            d = clampv (d, 1.0f, (float) capacity());
            const int   i = (int) d;
            const float f = d - (float) i;
            return at (i) + f * (at (i + 1) - at (i));
        }
    };

    //==========================================================================
    //  One-pole low-pass  y = (1-a) x + a y1   (a 0..1, higher = darker)
    //==========================================================================
    struct OnePoleLP
    {
        float a = 0.0f, y = 0.0f;
        void  reset() { y = 0.0f; }
        void  setCutoff (double hz, double sr) { a = (float) std::exp (-kTwoPi * std::max (1.0, hz) / sr); }
        float process (float x) { y = (1.0f - a) * x + a * y; return y; }

        /** Phase delay in samples of the filter at w (radians/sample). */
        static float phaseDelay (float a, double w)
        {
            if (w <= 1.0e-9) return a / (1.0f - a);
            return (float) (std::atan2 ((double) a * std::sin (w), 1.0 - (double) a * std::cos (w)) / w);
        }
        static float magnitude (float a, double w)
        {
            const std::complex<double> z = std::polar (1.0, -w);
            return (float) std::abs ((1.0 - a) / (1.0 - (double) a * z));
        }
    };

    struct OnePoleHP
    {
        OnePoleLP lp;
        void  reset() { lp.reset(); }
        void  setCutoff (double hz, double sr) { lp.setCutoff (hz, sr); }
        float process (float x) { return x - lp.process (x); }
    };

    struct DCBlocker
    {
        float x1 = 0.0f, y1 = 0.0f, r = 0.995f;
        void  prepare (double sr, double hz = 20.0) { r = (float) (1.0 - kTwoPi * hz / sr); }
        void  reset() { x1 = y1 = 0.0f; }
        float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }

        /** Phase delay in samples at w - NEGATIVE (a DC blocker leads), so a
            loop that carries one must be made that much LONGER. */
        static float phaseDelay (float r, double w)
        {
            const std::complex<double> z = std::polar (1.0, -w);
            const std::complex<double> h = (1.0 - z) / (1.0 - (double) r * z);
            return (float) (-std::arg (h) / std::max (1.0e-9, w));
        }
    };

    //==========================================================================
    //  First-order allpass  H = (c + z^-1) / (1 + c z^-1)
    //==========================================================================
    struct Allpass1
    {
        float c = 0.0f, x1 = 0.0f, y1 = 0.0f;
        void  reset() { x1 = y1 = 0.0f; }
        float process (float x) { const float y = c * x + x1 - c * y1; x1 = x; y1 = y; return y; }

        static float phaseDelay (float c, double w)
        {
            if (w <= 1.0e-9) return (1.0f - c) / (1.0f + c);
            const std::complex<double> z = std::polar (1.0, -w);
            const std::complex<double> h = ((double) c + z) / (1.0 + (double) c * z);
            double ph = -std::arg (h);
            if (ph < 0.0) ph += kTwoPi;
            return (float) (ph / w);
        }
    };

    //==========================================================================
    //  RBJ biquad, transposed direct form II
    //==========================================================================
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        void  reset() { z1 = z2 = 0.0f; }
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
        {
            b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
            a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
        }

        static double w0 (double hz, double sr) { return kTwoPi * clampv (hz, 10.0, sr * 0.49) / sr; }

        void lowpass (double hz, double q, double sr)
        {
            const double w = w0 (hz, sr), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
        }
        void highpass (double hz, double q, double sr)
        {
            const double w = w0 (hz, sr), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
        }
        void bandpass (double hz, double q, double sr)        // 0 dB peak
        {
            const double w = w0 (hz, sr), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
            set (al, 0, -al, 1 + al, -2 * cw, 1 - al);
        }
        void peak (double hz, double q, double db, double sr)
        {
            const double A = std::pow (10.0, db / 40.0);
            const double w = w0 (hz, sr), cw = std::cos (w), al = std::sin (w) / (2.0 * q);
            set (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
        }
        void lowShelf (double hz, double db, double sr)
        {
            const double A = std::pow (10.0, db / 40.0);
            const double w = w0 (hz, sr), cw = std::cos (w), sw = std::sin (w);
            const double al = sw / 2.0 * std::sqrt (2.0), sa = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * cw + sa), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sa),
                 (A + 1) + (A - 1) * cw + sa, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sa);
        }
        void highShelf (double hz, double db, double sr)
        {
            const double A = std::pow (10.0, db / 40.0);
            const double w = w0 (hz, sr), cw = std::cos (w), sw = std::sin (w);
            const double al = sw / 2.0 * std::sqrt (2.0), sa = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * cw + sa), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - sa),
                 (A + 1) - (A - 1) * cw + sa, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - sa);
        }
    };

    //==========================================================================
    //  Modal resonator: a decaying rotation.  An impulse of height A rings as a
    //  sine of peak A, so mode amplitudes are set directly by the input.
    //==========================================================================
    struct Mode
    {
        float re = 0.0f, im = 0.0f;
        float c = 1.0f, s = 0.0f, r = 0.0f;
        bool  active = false;

        void reset() { re = im = 0.0f; }

        void set (double hz, float t60, double sr)
        {
            active = hz > 0.0 && hz < sr * 0.46;
            const double w = kTwoPi * hz / sr;
            c = (float) std::cos (w);
            s = (float) std::sin (w);
            r = sampleGainForT60 (t60, sr);
        }
        void setDecayOnly (float t60, double sr) { r = sampleGainForT60 (t60, sr); }

        float process (float x)
        {
            const float nr = r * (c * re - s * im) + x;
            const float ni = r * (s * re + c * im);
            re = nr; im = ni;
            return im;
        }
        float energy() const { return re * re + im * im; }
    };

    //==========================================================================
    //  Raised-cosine hammer / mallet pulse.  Length in samples, area ~ amp.
    //==========================================================================
    struct Pulse
    {
        int   len = 0, pos = 0;
        float amp = 0.0f;

        void trigger (float lengthSamples, float amplitude)
        {
            len = std::max (2, (int) lengthSamples);
            pos = 0;
            amp = amplitude;
        }
        bool  running() const { return pos < len; }
        float next()
        {
            if (pos >= len) return 0.0f;
            const float ph = (float) pos / (float) len;
            ++pos;
            return amp * 0.5f * (1.0f - (float) std::cos (kTwoPi * ph));
        }
    };

    //==========================================================================
    //  Exponential one-shot burst (noise clicks, chiff, thumps)
    //==========================================================================
    struct Burst
    {
        float level = 0.0f, mul = 0.0f;
        void  trigger (float amp, float t60, double sr) { level = amp; mul = sampleGainForT60 (t60, sr); }
        float next() { const float v = level; level *= mul; if (level < 1.0e-6f) level = 0.0f; return v; }
        bool  running() const { return level > 0.0f; }
    };

    //==========================================================================
    //  Linear ramp smoother
    //==========================================================================
    struct Ramp
    {
        float v = 0.0f, target = 0.0f, step = 0.0f;
        int   left = 0;
        void  setNow (float x) { v = target = x; left = 0; step = 0.0f; }
        void  to (float x, int samples)
        {
            target = x;
            if (samples <= 0) { v = x; left = 0; return; }
            left = samples;
            step = (target - v) / (float) samples;
        }
        float next() { if (left > 0) { v += step; if (--left == 0) v = target; } return v; }
    };

    inline float softClip (float x) { return x / (1.0f + std::abs (x)); }

    /** Cheap tanh for the hot loops: exact at 0, smooth, |y| < 1. */
    inline float fastTanh (float x)
    {
        x = clampv (x, -4.5f, 4.5f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    //==========================================================================
    //  Sine table for additive models
    //==========================================================================
    struct SineTable
    {
        static constexpr int kSize = 4096;
        float t[kSize + 1];
        SineTable()
        {
            for (int i = 0; i <= kSize; ++i) t[i] = (float) std::sin (kTwoPi * (double) i / kSize);
        }
        float at (double ph) const        // ph 0..1
        {
            const double p = ph * kSize;
            const int    i = (int) p;
            const float  f = (float) (p - i);
            return t[i & (kSize - 1)] + f * (t[(i & (kSize - 1)) + 1] - t[i & (kSize - 1)]);
        }
        static const SineTable& get() { static const SineTable s; return s; }
    };
}
