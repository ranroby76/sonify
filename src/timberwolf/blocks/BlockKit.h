// D:\workspace\Fanan_web_hub\src\timberwolf\blocks\BlockKit.h
#pragma once
//==============================================================================
//  BlockKit.h  -  the small parts Timber Wolf's effects are built from.
//
//  Filters (one-pole, the RBJ cookbook biquads, a state-variable filter), a
//  fractional delay line, an LFO, an envelope follower, a DC blocker, the usual
//  clipping curves, and a 2x/4x/8x oversampler around a nonlinear stage.  All
//  of it allocates in prepare only.
//==============================================================================
#include "../TwBlock.h"

#include <juce_dsp/juce_dsp.h>

#include <vector>

namespace tw::kit
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    //==========================================================================
    struct OnePole
    {
        float a = 0.0f, z = 0.0f;

        void lowpass (double sr, double hz)   { a = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * juce::jlimit (1.0, sr * 0.49, hz) / sr)); }
        float lp (float x) noexcept           { z += a * (x - z); return z; }
        float hp (float x) noexcept           { z += a * (x - z); return x - z; }
        void reset() noexcept                 { z = 0.0f; }
    };

    /** RBJ cookbook biquad, transposed direct form II (double state: low filters stay clean). */
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        enum class Type { lowpass, highpass, bandpass, notch, peak, lowshelf, highshelf, allpass };

        void set (Type t, double sr, double hz, double q, double gainDb = 0.0)
        {
            hz = juce::jlimit (5.0, sr * 0.49, hz);
            q = juce::jmax (0.05, q);
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = juce::MathConstants<double>::twoPi * hz / sr;
            const double cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0 * q);
            double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;

            switch (t)
            {
                case Type::lowpass:   B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
                case Type::highpass:  B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
                case Type::bandpass:  B0 = alpha; B1 = 0; B2 = -alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
                case Type::notch:     B0 = 1; B1 = -2 * cw; B2 = 1; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
                case Type::allpass:   B0 = 1 - alpha; B1 = -2 * cw; B2 = 1 + alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
                case Type::peak:      B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; break;
                case Type::lowshelf:
                {
                    const double s = 2 * std::sqrt (A) * alpha;
                    B0 = A * ((A + 1) - (A - 1) * cw + s); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s);
                    A0 = (A + 1) + (A - 1) * cw + s; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s;
                    break;
                }
                case Type::highshelf:
                {
                    const double s = 2 * std::sqrt (A) * alpha;
                    B0 = A * ((A + 1) + (A - 1) * cw + s); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s);
                    A0 = (A + 1) - (A - 1) * cw + s; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s;
                    break;
                }
            }

            b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
        }

        float run (float xf) noexcept
        {
            const double x = xf;
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }

        void reset() noexcept   { z1 = z2 = 0.0; }
    };

    /** A Simper/Cytomic state-variable filter: lowpass, bandpass, highpass at once; safe to sweep. */
    struct Svf
    {
        float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        float low = 0, band = 0, high = 0;

        void set (double sr, double hz, double q)
        {
            g = (float) std::tan (juce::MathConstants<double>::pi * juce::jlimit (5.0, sr * 0.49, hz) / sr);
            k = (float) (1.0 / juce::jmax (0.05, q));
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void run (float x) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            low = v2; band = v1; high = x - k * v1 - v2;
        }

        void reset() noexcept   { ic1 = ic2 = 0.0f; }
    };

    struct DcBlock
    {
        float x1 = 0, y1 = 0, r = 0.995f;
        void prepare (double sr)               { r = (float) std::exp (-juce::MathConstants<double>::twoPi * 10.0 / sr); }
        float run (float x) noexcept           { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
        void reset() noexcept                  { x1 = y1 = 0.0f; }
    };

    //==========================================================================
    /** A delay line read anywhere between its samples (cubic). */
    struct Delay
    {
        std::vector<float> buf;
        int mask = 0, w = 0;

        void prepare (int maxSamples)
        {
            int size = 1;
            while (size < maxSamples + 4) size <<= 1;
            buf.assign ((size_t) size, 0.0f);
            mask = size - 1;
            w = 0;
        }

        void reset() noexcept                  { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        void push (float x) noexcept           { buf[(size_t) w] = x; w = (w + 1) & mask; }
        int capacity() const noexcept          { return mask - 3; }

        /** d samples ago (d >= 1 after a push of this sample). */
        float read (float d) const noexcept
        {
            d = juce::jlimit (1.0f, (float) (mask - 3), d);
            const float pos = (float) w - d;
            const int i = (int) std::floor (pos);
            const float f = pos - (float) i;
            const float x0 = buf[(size_t) ((i - 1) & mask)], x1 = buf[(size_t) (i & mask)];
            const float x2 = buf[(size_t) ((i + 1) & mask)], x3 = buf[(size_t) ((i + 2) & mask)];
            const float c1 = 0.5f * (x2 - x0), c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
            const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
            return ((c3 * f + c2) * f + c1) * f + x1;
        }

        float readInt (int d) const noexcept   { return buf[(size_t) ((w - juce::jlimit (1, mask, d)) & mask)]; }
    };

    //==========================================================================
    struct Lfo
    {
        double phase = 0.0;

        /** Advances by hz; shapes: 0 sine, 1 triangle, 2 square (softened), 3 saw down, 4 random (smooth). */
        float next (double hz, double sr, int shape = 0) noexcept
        {
            phase += hz / sr;
            if (phase >= 1.0) { phase -= std::floor (phase); held = target; target = rnd.nextFloat() * 2.0f - 1.0f; }
            const float p = (float) phase;

            switch (shape)
            {
                case 1:  return 1.0f - 4.0f * std::abs (p - 0.5f);
                case 2:  return std::tanh (6.0f * std::sin (twoPi * p));
                case 3:  return 1.0f - 2.0f * p;
                case 4:  return held + (target - held) * (0.5f - 0.5f * std::cos (pi * p));
                default: return std::sin (twoPi * p);
            }
        }

        void reset() noexcept   { phase = 0.0; }

        juce::Random rnd;
        float held = 0.0f, target = 0.0f;
    };

    struct Follower
    {
        float env = 0.0f, att = 0.0f, rel = 0.0f;

        void set (double sr, double attackMs, double releaseMs)
        {
            att = (float) std::exp (-1.0 / juce::jmax (1.0, attackMs * 0.001 * sr));
            rel = (float) std::exp (-1.0 / juce::jmax (1.0, releaseMs * 0.001 * sr));
        }

        float run (float x) noexcept
        {
            const float a = std::abs (x);
            env = a + (a > env ? att : rel) * (env - a);
            return env;
        }

        void reset() noexcept   { env = 0.0f; }
    };

    //==========================================================================
    inline float softClip (float x) noexcept        { return std::tanh (x); }
    inline float hardClip (float x) noexcept        { return juce::jlimit (-1.0f, 1.0f, x); }
    inline float cubicClip (float x) noexcept
    {
        x = juce::jlimit (-1.5f, 1.5f, x);
        return x - (4.0f / 27.0f) * x * x * x;
    }

    /** Beat divisions for tempo-synced times: names and lengths in beats. */
    inline const juce::StringArray& divisionNames()
    {
        static const juce::StringArray n { "Off", "1/1", "1/2", "1/2 dot", "1/4", "1/4 dot", "1/4 trip", "1/8", "1/8 dot", "1/8 trip", "1/16", "1/16 dot", "1/16 trip", "1/32" };
        return n;
    }

    inline double divisionBeats (int i)
    {
        static const double b[] { 0, 4, 2, 3, 1, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125 };
        return b[juce::jlimit (0, 13, i)];
    }

    /** A time in ms: free, or the division of a beat at bpm when sync is on. */
    inline double syncedMs (double freeMs, int division, double bpm)
    {
        if (division <= 0 || bpm <= 0.0)
            return freeMs;

        return divisionBeats (division) * 60000.0 / bpm;
    }

    //==========================================================================
    /** A nonlinear stage run oversampled: up, the stage per sample, down. Stereo. */
    class Oversampled
    {
    public:
        void prepare (int factorLog2, int maxBlock)
        {
            os = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) factorLog2,
                     juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
            os->initProcessing ((size_t) juce::jmax (1, maxBlock));
            factor = 1 << factorLog2;
        }

        void reset()                                { if (os) os->reset(); }
        int getFactor() const noexcept              { return factor; }
        int latency() const noexcept                { return os ? (int) std::round (os->getLatencyInSamples()) : 0; }

        template <typename Stage>
        void process (float* l, float* r, int n, Stage&& stage)
        {
            float* ch[] { l, r };
            juce::dsp::AudioBlock<float> block (ch, 2, (size_t) n);
            auto up = os->processSamplesUp (block);
            float* ul = up.getChannelPointer (0);
            float* ur = up.getChannelPointer (1);
            const int un = (int) up.getNumSamples();

            for (int i = 0; i < un; ++i)
                stage (ul[i], ur[i]);

            os->processSamplesDown (block);
        }

    private:
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        int factor = 1;
    };

    /** Whether the two sides carry the same signal (a mono guitar in stereo). */
    inline bool isMono (const float* l, const float* r, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            if (l[i] != r[i])
                return false;

        return true;
    }

    /** An effect's info, short to write. */
    inline BlockInfo info (const juce::String& id, const juce::String& name, Family family, const juce::String& icon,
                           const juce::String& credit, const juce::String& description, std::vector<KnobSpec> knobs,
                           std::function<std::unique_ptr<Block>()> make, bool trails = false)
    {
        BlockInfo b;
        b.id = id; b.name = name; b.family = family; b.icon = icon; b.credit = credit; b.description = description;
        b.knobs = std::move (knobs); b.make = std::move (make); b.trails = trails;
        return b;
    }

    template <typename T>
    std::function<std::unique_ptr<Block>()> maker()   { return [] { return std::unique_ptr<Block> (new T()); }; }
}
