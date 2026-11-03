// D:\workspace\Fanan_web_hub\src\SonifySpace.h

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

/** Space Lifter 4's engine: a reverb and an echo blended into one space.

    Plain C++ (no JUCE), so the reverb alone can go into the instruments too (Natura, Tomahawk,
    Grex, AXIS): SonifySpace::Reverb is a whole reverb by itself (pre-delay, colour, early
    reflections, its voicings, its shapes), SonifySpace::Echo a whole echo, and SonifySpace::Space
    the two wired together in one of four routings.

    Everything is allocated in prepare(); set() and the audio calls never allocate, and run on
    the audio thread. Run them with denormals flushed (juce::ScopedNoDenormals). */
namespace SonifySpace
{
    constexpr float pi = 3.14159265358979f;
    constexpr float twoPi = 6.28318530717959f;

    inline float clampf (float x, float lo, float hi) noexcept   { return std::min (hi, std::max (lo, x)); }

    /** tanh, near enough for saturation: odd, unity slope at 0, flat past +-3. */
    inline float softClip (float x) noexcept
    {
        x = clampf (x, -3.0f, 3.0f);
        const auto x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    /** Saturation by drive (small signals pass at unity; the more drive, the sooner it bends). */
    inline float saturate (float x, float drive) noexcept    { return softClip (x * drive) / drive; }

    /** A sine from a phase 0..1 (a refined parabola, within 0.1%): for LFOs. */
    inline float lfoSine (float phase) noexcept
    {
        const auto t = 2.0f * phase - 1.0f;
        const auto y = -4.0f * t * (1.0f - std::abs (t));
        return 0.225f * (y * std::abs (y) - y) + y;
    }

    inline float wrap01 (float phase) noexcept   { return phase - std::floor (phase); }

    /** The one-pole coefficient that settles in about ms milliseconds. */
    inline float settle (float ms, float rate) noexcept
    {
        return 1.0f - std::exp (-1.0f / std::max (1.0f, ms * 0.001f * rate));
    }

    inline float dbToGain (float db) noexcept   { return std::pow (10.0f, db / 20.0f); }

    //==============================================================================
    struct Lfo
    {
        float phase = 0.0f, step = 0.0f;

        void setRate (float hz, float rate) noexcept   { step = hz / rate; }
        float next() noexcept
        {
            phase += step;
            phase -= std::floor (phase);
            return lfoSine (phase);
        }
    };

    struct Random
    {
        uint32_t state = 0x9E3779B9u;

        explicit Random (uint32_t seed = 0x9E3779B9u) : state (seed != 0 ? seed : 1u) {}

        /** -1..1 */
        float next() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return static_cast<float> (static_cast<int32_t> (state)) * (1.0f / 2147483648.0f);
        }
    };

    struct Smooth
    {
        float value = 0.0f, target = 0.0f, coefficient = 1.0f;

        void setTime (float ms, float rate) noexcept   { coefficient = settle (ms, rate); }
        void jump (float v) noexcept                   { value = target = v; }
        float next() noexcept                          { value += coefficient * (target - value); return value; }
    };

    //==============================================================================
    /** A delay line, read before each write: at(1) is the last sample written. */
    class DelayLine
    {
    public:
        void allocate (int maxDelaySamples)
        {
            int size = 8;

            while (size < maxDelaySamples + 8)
                size <<= 1;

            buffer.assign (static_cast<size_t> (size), 0.0f);
            mask = size - 1;
            pos = 0;
        }

        void clear() noexcept                   { std::fill (buffer.begin(), buffer.end(), 0.0f); }
        float longest() const noexcept          { return static_cast<float> (mask - 4); }

        void write (float x) noexcept
        {
            buffer[static_cast<size_t> (pos)] = x;
            pos = (pos + 1) & mask;
        }

        float at (int delay) const noexcept     { return buffer[static_cast<size_t> ((pos - delay) & mask)]; }

        float readLinear (float delay) const noexcept
        {
            delay = clampf (delay, 1.0f, longest());
            const auto i = static_cast<int> (delay);
            const auto f = delay - static_cast<float> (i);
            const auto a = at (i);
            return a + f * (at (i + 1) - a);
        }

        /** Hermite: for modulated reads (no dulling, no zipper). */
        float readCubic (float delay) const noexcept
        {
            delay = clampf (delay, 2.0f, longest());
            const auto i = static_cast<int> (delay);
            const auto f = delay - static_cast<float> (i);
            const auto p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
            const auto c1 = 0.5f * (p2 - p0);
            const auto c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            const auto c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
            return ((c3 * f + c2) * f + c1) * f + p1;
        }

    private:
        std::vector<float> buffer = std::vector<float> (8, 0.0f);
        int mask = 7, pos = 0;
    };

    /** A Schroeder allpass around its own line (kept public: a plate's taps read inside it). */
    struct Allpass
    {
        DelayLine line;

        float process (float x, float delay, float g) noexcept
        {
            const auto delayed = line.readLinear (delay);
            const auto w = x + g * delayed;
            line.write (w);
            return delayed - g * w;
        }

        float processCubic (float x, float delay, float g) noexcept
        {
            const auto delayed = line.readCubic (delay);
            const auto w = x + g * delayed;
            line.write (w);
            return delayed - g * w;
        }
    };

    struct OnePole
    {
        float a = 1.0f, z = 0.0f;

        void setCutoff (float hz, float rate) noexcept   { a = 1.0f - std::exp (-twoPi * clampf (hz, 1.0f, 0.49f * rate) / rate); }
        float lowpass (float x) noexcept                 { z += a * (x - z); return z; }
        float highpass (float x) noexcept                { return x - lowpass (x); }
        void reset() noexcept                            { z = 0.0f; }
    };

    /** RBJ low- and high-pass (transposed direct form II). */
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;

        void lowpass (float hz, float q, float rate) noexcept    { design (hz, q, rate, true); }
        void highpass (float hz, float q, float rate) noexcept   { design (hz, q, rate, false); }

        void design (float hz, float q, float rate, bool low) noexcept
        {
            const auto w = twoPi * clampf (hz, 10.0f, 0.45f * rate) / rate;
            const auto cs = std::cos (w);
            const auto alpha = std::sin (w) / (2.0f * q);
            const auto a0 = 1.0f + alpha;

            if (low)
            {
                b1 = (1.0f - cs) / a0;
                b0 = b2 = 0.5f * b1;
            }
            else
            {
                b1 = -(1.0f + cs) / a0;
                b0 = b2 = -0.5f * b1;
            }

            a1 = -2.0f * cs / a0;
            a2 = (1.0f - alpha) / a0;
        }

        void pass() noexcept   { b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f; }

        float process (float x) noexcept
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void reset() noexcept   { z1 = z2 = 0.0f; }
    };

    /** Darker or brighter around 1 kHz: -1 is 6 dB down above and up below, +1 the other way. */
    struct Tilt
    {
        OnePole split;
        float low = 1.0f, high = 1.0f;

        void set (float colour, float rate) noexcept
        {
            split.setCutoff (1000.0f, rate);
            high = dbToGain (6.0f * colour);
            low = 1.0f / high;
        }

        float process (float x) noexcept
        {
            const auto lo = split.lowpass (x);
            return low * lo + high * (x - lo);
        }
    };

    /** An early digital box's converters: band-limited, held at a lower rate, 12 bits. */
    struct Degrade
    {
        Biquad before, after;
        int hold = 1, count = 0;
        float held = 0.0f, steps = 2048.0f;

        void prepare (float rate, float targetRate, float bandwidthHz, int bits) noexcept
        {
            hold = std::max (1, static_cast<int> (std::lround (rate / targetRate)));
            before.lowpass (std::min (bandwidthHz, 0.45f * rate / static_cast<float> (hold)), 0.707f, rate);
            after.lowpass (std::min (bandwidthHz * 1.3f, 0.45f * rate), 0.707f, rate);
            steps = static_cast<float> (1 << (bits - 1));
        }

        float process (float x) noexcept
        {
            x = before.process (x);

            if (++count >= hold)
            {
                count = 0;
                held = std::round (clampf (x, -1.5f, 1.5f) * steps) / steps;
            }

            return after.process (held);
        }

        void reset() noexcept   { before.reset(); after.reset(); held = 0.0f; count = 0; }
    };

    //==============================================================================
    // THE REVERB
    //==============================================================================
    enum class Voicing : int { plate, hall, room, chamber, cathedral, spring, vintage, shimmer };
    constexpr int numVoicings = 8;

    /** How the tail is shaped after the sound that makes it: as it decays, cut off (gate),
        rising into a stop (reverse), or swelling up after the hit (bloom). */
    enum class Shape : int { decay, gate, reverse, bloom };

    struct ReverbSettings
    {
        Voicing voicing = Voicing::plate;
        float size = 0.5f, decay = 0.5f, damping = 0.5f;     // 0..1
        float preDelayMs = 10.0f;                            // 0..250
        float early = 0.0f, earlySize = 0.5f;                // 0..1
        float colour = 0.0f;                                 // -1 dark .. +1 bright
        float texture = 0.6f;                                // 0 grainy .. 1 smooth
        float mod = 0.25f;                                   // 0..1
        Shape shape = Shape::decay;
        float shapeMs = 350.0f;                              // the gate's, reverse's or bloom's length
        bool freeze = false;
    };

    /** What makes each voicing itself. */
    struct VoiceSpec
    {
        float rtShort, rtLong;          // its RT60 (s) at Decay 0 and 1
        float sizeSmall, sizeLarge;     // its lengths' scale at Size 0 and 1
        std::array<float, 8> linesMs;   // the network's lines at scale 1 (plate and spring: unused)
        float diffuserScale;            // its input diffusers' lengths
        float diffusion;                // how strongly they diffuse (at Texture 100%)
        float modHz, modMs;             // its lines' movement (at Mod 25%)
        float brightness;               // how long its highs ring against its RT
        float earlyOwn;                 // early reflections it has of itself
        float trim;                     // its output level
    };

    inline const VoiceSpec& voiceSpec (Voicing v) noexcept
    {
        static const std::array<VoiceSpec, numVoicings> specs {{
            // plate
            { 0.4f,  8.0f, 0.5f, 1.5f, {}, 1.0f, 0.75f, 1.0f, 0.5f, 1.0f, 0.0f, 1.0f },
            // hall
            { 0.6f, 12.0f, 0.5f, 1.6f, { 29.3f, 33.7f, 38.9f, 43.1f, 49.7f, 55.3f, 61.9f, 68.3f }, 1.0f, 0.72f, 0.55f, 0.45f, 0.75f, 0.12f, 0.48f },
            // room
            { 0.15f, 2.5f, 0.5f, 1.8f, { 7.3f, 9.1f, 11.3f, 13.7f, 15.1f, 17.9f, 19.3f, 23.1f }, 0.55f, 0.65f, 0.8f, 0.15f, 0.8f, 0.4f, 0.53f },
            // chamber
            { 0.3f,  4.5f, 0.5f, 1.6f, { 13.1f, 16.7f, 19.9f, 23.3f, 26.9f, 29.7f, 33.1f, 37.3f }, 0.75f, 0.75f, 0.6f, 0.25f, 1.0f, 0.25f, 0.5f },
            // cathedral
            { 2.0f, 25.0f, 0.6f, 1.6f, { 47.3f, 55.9f, 63.1f, 71.7f, 81.3f, 89.9f, 97.1f, 107.3f }, 1.4f, 0.75f, 0.3f, 0.8f, 0.65f, 0.1f, 0.52f },
            // spring
            { 0.6f,  6.0f, 0.7f, 1.3f, {}, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.05f },
            // vintage digital
            { 0.4f, 10.0f, 0.4f, 1.4f, { 31.1f, 35.9f, 41.3f, 46.7f, 51.1f, 58.3f, 63.7f, 71.9f }, 0.9f, 0.6f, 1.1f, 1.2f, 0.6f, 0.1f, 0.62f },
            // shimmer
            { 1.2f, 14.0f, 0.7f, 1.6f, { 29.3f, 33.7f, 38.9f, 43.1f, 49.7f, 55.3f, 61.9f, 68.3f }, 1.0f, 0.75f, 0.4f, 0.6f, 0.8f, 0.0f, 0.39f }
        }};

        return specs[static_cast<size_t> (std::clamp (static_cast<int> (v), 0, numVoicings - 1))];
    }

    /** The Hadamard 8x8 (orthonormal: the network loses nothing it isn't told to). */
    inline void hadamard8 (float* x) noexcept
    {
        for (int h = 1; h < 8; h <<= 1)
            for (int i = 0; i < 8; i += h * 2)
                for (int j = i; j < i + h; ++j)
                {
                    const auto a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }

        for (int i = 0; i < 8; ++i)
            x[i] *= 0.353553391f;
    }

    /** Eight modulated lines through a Hadamard matrix, each losing its highs faster than its
        lows (a one-pole whose DC and Nyquist gains are the two decays exactly). */
    class Network
    {
    public:
        void allocate (float rate)
        {
            for (auto& line : lines)
                line.allocate (static_cast<int> (0.26f * rate));

            for (auto& d : diffusersL) d.line.allocate (static_cast<int> (0.03f * rate));
            for (auto& d : diffusersR) d.line.allocate (static_cast<int> (0.03f * rate));

            scale.setTime (120.0f, rate);
        }

        void clear() noexcept
        {
            for (auto& line : lines) line.clear();
            for (auto& d : diffusersL) d.line.clear();
            for (auto& d : diffusersR) d.line.clear();
            damp.fill (0.0f);
        }

        void set (const VoiceSpec& v, float sizeScale, float rt, float highRt, float texture, float modScale, bool freeze,
                  float rate, bool jumpScale) noexcept
        {
            if (jumpScale)
                scale.jump (sizeScale);
            else
                scale.target = sizeScale;

            for (size_t i = 0; i < 8; ++i)
            {
                base[i] = v.linesMs[i] * 0.001f * rate;
                const auto length = base[i] * sizeScale;
                const auto g = freeze ? 1.0f : std::pow (10.0f, -3.0f * length / (rt * rate));
                const auto gh = freeze ? 1.0f : std::pow (10.0f, -3.0f * length / (highRt * rate));
                dampA[i] = (g - gh) / (g + gh);
                dampB[i] = g * (1.0f - dampA[i]);

                lfos[i].setRate (v.modHz * (0.73f + 0.071f * static_cast<float> (i)), rate);
                depth[i] = v.modMs * 0.001f * rate * modScale * (0.8f + 0.05f * static_cast<float> (i));
            }

            static constexpr std::array<float, 4> diffL { 4.77f, 3.59f, 12.73f, 9.31f };
            static constexpr std::array<float, 4> diffR { 4.97f, 3.37f, 12.31f, 9.83f };

            for (size_t i = 0; i < 4; ++i)
            {
                diffLenL[i] = diffL[i] * v.diffuserScale * 0.001f * rate;
                diffLenR[i] = diffR[i] * v.diffuserScale * 0.001f * rate;
            }

            const auto amount = v.diffusion * (0.25f + 0.75f * clampf (texture, 0.0f, 1.0f));
            diffG = { amount, amount, amount * 0.83f, amount * 0.83f };
        }

        void tick (float inL, float inR, float& outL, float& outR) noexcept
        {
            for (size_t i = 0; i < 4; ++i)
            {
                inL = diffusersL[i].process (inL, diffLenL[i], diffG[i]);
                inR = diffusersR[i].process (inR, diffLenR[i], diffG[i]);
            }

            const auto s = scale.next();
            float y[8];

            for (size_t i = 0; i < 8; ++i)
            {
                const auto delayed = lines[i].readCubic (base[i] * s + depth[i] * lfos[i].next());
                damp[i] = dampB[i] * delayed + dampA[i] * damp[i];
                y[i] = damp[i];
            }

            outL = 0.5f * (y[0] - y[1] + y[2] - y[3] + y[4] - y[5] + y[6] - y[7]);
            outR = 0.5f * (y[0] + y[1] - y[2] - y[3] + y[4] + y[5] - y[6] - y[7]);

            hadamard8 (y);

            lines[0].write (y[0] + inL);
            lines[1].write (y[1] + inR);
            lines[2].write (y[2] - inL);
            lines[3].write (y[3] - inR);
            lines[4].write (y[4] + inL);
            lines[5].write (y[5] - inR);
            lines[6].write (y[6] - inL);
            lines[7].write (y[7] + inR);
        }

    private:
        std::array<DelayLine, 8> lines;
        std::array<Lfo, 8> lfos;
        std::array<float, 8> base {}, depth {}, dampA {}, dampB {}, damp {};
        std::array<Allpass, 4> diffusersL, diffusersR;
        std::array<float, 4> diffLenL {}, diffLenR {}, diffG {};
        Smooth scale;
    };

    /** Dattorro's plate (the figure-of-eight tank, in its 29761 Hz lengths scaled). */
    class Plate
    {
    public:
        void allocate (float rate)
        {
            k = rate / 29761.0f;
            const auto most = [&] (float samples) { return static_cast<int> (samples * k * 1.6f) + 200; };

            in1.line.allocate (most (142.0f));
            in2.line.allocate (most (107.0f));
            in3.line.allocate (most (379.0f));
            in4.line.allocate (most (277.0f));
            apL1.line.allocate (most (672.0f + 90.0f));
            apR1.line.allocate (most (908.0f + 90.0f));
            apL2.line.allocate (most (1800.0f));
            apR2.line.allocate (most (2656.0f));
            dL1.allocate (most (4453.0f));
            dR1.allocate (most (4217.0f));
            dL2.allocate (most (3720.0f));
            dR2.allocate (most (3163.0f));

            scale.setTime (120.0f, rate);
            lfoL.setRate (0.91f, rate);
            lfoR.setRate (1.07f, rate);
            lfoR.phase = 0.25f;
        }

        void clear() noexcept
        {
            for (auto* a : { &in1, &in2, &in3, &in4, &apL1, &apR1, &apL2, &apR2 })
                a->line.clear();

            for (auto* d : { &dL1, &dR1, &dL2, &dR2 })
                d->clear();

            dampL = dampR = 0.0f;
        }

        void set (float sizeScale, float rt, float damping, float texture, float modScale, bool freeze, bool jumpScale) noexcept
        {
            if (jumpScale)
                scale.jump (sizeScale);
            else
                scale.target = sizeScale;

            // Four decay multiplies around the whole figure of eight.
            const auto loopSeconds = 21589.0f * sizeScale / 29761.0f;
            decay = freeze ? 1.0f : std::pow (10.0f, -3.0f * loopSeconds / (4.0f * rt));
            damp = freeze ? 0.0f : 0.02f + 0.7f * std::pow (clampf (damping, 0.0f, 1.0f), 1.3f);

            const auto t = 0.35f + 0.65f * clampf (texture, 0.0f, 1.0f);
            inG1 = 0.75f * t;
            inG2 = 0.625f * t;
            decayDiffusion1 = 0.7f * (0.6f + 0.4f * t);
            decayDiffusion2 = 0.5f * (0.6f + 0.4f * t);
            excursion = 16.0f * k * modScale;
        }

        void tick (float inL, float inR, float& outL, float& outR) noexcept
        {
            const auto s = scale.next();
            const auto ks = k * s;

            auto x = 0.5f * (inL + inR);
            x = in1.process (x, 142.0f * k, inG1);
            x = in2.process (x, 107.0f * k, inG1);
            x = in3.process (x, 379.0f * k, inG2);
            x = in4.process (x, 277.0f * k, inG2);

            const auto leftTank = dL2.readLinear (3720.0f * ks);
            const auto rightTank = dR2.readLinear (3163.0f * ks);

            // The left half, fed by the right's end.
            {
                auto a = x + decay * rightTank;
                a = apL1.processCubic (a, 672.0f * ks + excursion * lfoL.next(), -decayDiffusion1);
                auto v = dL1.readLinear (4453.0f * ks);
                dL1.write (a);
                dampL += (1.0f - damp) * (v - dampL);
                v = apL2.process (dampL * decay, 1800.0f * ks, decayDiffusion2);
                dL2.write (v);
            }

            // The right half, fed by the left's.
            {
                auto a = x + decay * leftTank;
                a = apR1.processCubic (a, 908.0f * ks + excursion * lfoR.next(), -decayDiffusion1);
                auto v = dR1.readLinear (4217.0f * ks);
                dR1.write (a);
                dampR += (1.0f - damp) * (v - dampR);
                v = apR2.process (dampR * decay, 2656.0f * ks, decayDiffusion2);
                dR2.write (v);
            }

            outL = 0.6f * (dR1.readLinear (266.0f * ks) + dR1.readLinear (2974.0f * ks) - apR2.line.readLinear (1913.0f * ks)
                           + dR2.readLinear (1996.0f * ks) - dL1.readLinear (1990.0f * ks) - apL2.line.readLinear (187.0f * ks)
                           - dL2.readLinear (1066.0f * ks));
            outR = 0.6f * (dL1.readLinear (353.0f * ks) + dL1.readLinear (3627.0f * ks) - apL2.line.readLinear (1228.0f * ks)
                           + dL2.readLinear (2673.0f * ks) - dR1.readLinear (2111.0f * ks) - apR2.line.readLinear (335.0f * ks)
                           - dR2.readLinear (121.0f * ks));
        }

    private:
        float k = 1.0f;
        Allpass in1, in2, in3, in4, apL1, apR1, apL2, apR2;
        DelayLine dL1, dR1, dL2, dR2;
        Lfo lfoL, lfoR;
        Smooth scale;
        float decay = 0.5f, damp = 0.3f, dampL = 0.0f, dampR = 0.0f;
        float inG1 = 0.75f, inG2 = 0.625f, decayDiffusion1 = 0.7f, decayDiffusion2 = 0.5f, excursion = 16.0f;
    };

    /** Two springs: each a loop whose every pass runs through a long chain of first-order
        allpasses (the lows lag the highs: the chirp, the drip), band-limited as a tank is. */
    class Spring
    {
    public:
        static constexpr int stages = 64;

        void allocate (float sampleRate)
        {
            rate = sampleRate;

            for (auto& unit : units)
                unit.loop.allocate (static_cast<int> (0.08f * rate));

            // About 4.7 samples of lag per stage at 48 kHz, as long in time at any rate.
            const auto lag = 4.7f * rate / 48000.0f;
            coefficient = (1.0f - lag) / (1.0f + lag);
            scale.setTime (120.0f, rate);
        }

        void clear() noexcept
        {
            for (auto& unit : units)
            {
                unit.loop.clear();
                unit.chain.fill (0.0f);
                unit.low.reset();
                unit.high.reset();
                unit.toneLow.reset();
                unit.toneHigh.reset();
            }
        }

        void set (float sizeScale, float rt, float damping, bool freeze, bool jumpScale) noexcept
        {
            if (jumpScale)
                scale.jump (sizeScale);
            else
                scale.target = sizeScale;

            for (size_t i = 0; i < units.size(); ++i)
            {
                auto& unit = units[i];
                unit.base = (i == 0 ? 0.0383f : 0.0447f) * rate;
                unit.feedback = freeze ? 1.0f : std::pow (10.0f, -3.0f * unit.base * sizeScale / (rt * rate));
                unit.low.setCutoff (freeze ? 0.49f * rate : 11000.0f * (1.0f - 0.5f * clampf (damping, 0.0f, 1.0f)), rate);
                unit.high.setCutoff (freeze ? 1.0f : 35.0f, rate);
                unit.toneLow.lowpass (4800.0f * (1.0f - 0.45f * clampf (damping, 0.0f, 1.0f)), 0.6f, rate);
                unit.toneHigh.highpass (110.0f, 0.6f, rate);
            }
        }

        void tick (float inL, float inR, float& outL, float& outR) noexcept
        {
            const auto s = scale.next();
            const float ins[2] { 0.75f * inL + 0.25f * inR, 0.25f * inL + 0.75f * inR };
            float outs[2];

            for (size_t i = 0; i < units.size(); ++i)
            {
                auto& unit = units[i];
                const auto back = unit.loop.readLinear (unit.base * s);
                auto v = ins[i] + unit.feedback * unit.high.highpass (unit.low.lowpass (back));

                for (auto& z : unit.chain)
                {
                    const auto y = coefficient * v + z;
                    z = v - coefficient * y;
                    v = y;
                }

                unit.loop.write (softClip (v));
                outs[i] = unit.toneHigh.process (unit.toneLow.process (back));
            }

            outL = outs[0] + 0.2f * outs[1];
            outR = outs[1] + 0.2f * outs[0];
        }

    private:
        struct Unit
        {
            DelayLine loop;
            std::array<float, stages> chain {};
            OnePole low, high;           // in the loop: gentle, so the mids ring as long as the RT
            Biquad toneLow, toneHigh;    // out of it: the tank's band
            float base = 1000.0f, feedback = 0.5f;
        };

        std::array<Unit, 2> units;
        float coefficient = -0.65f, rate = 48000.0f;
        Smooth scale;
    };

    /** An octave up: two heads sweeping a short line, crossfaded (a shimmer's pitch). */
    class OctaveUp
    {
    public:
        void allocate (float rate)
        {
            line.allocate (static_cast<int> (0.1f * rate));
            window = 0.06f * rate;
        }

        void clear() noexcept   { line.clear(); phase = 0.0f; }

        float process (float x) noexcept
        {
            line.write (x);
            phase = wrap01 (phase + 1.0f / window);
            const auto other = wrap01 (phase + 0.5f);
            const auto g1 = 0.5f - 0.5f * lfoSine (wrap01 (phase + 0.25f));
            const auto g2 = 1.0f - g1;
            return g1 * line.readLinear (2.0f + window * (1.0f - phase)) + g2 * line.readLinear (2.0f + window * (1.0f - other));
        }

    private:
        DelayLine line;
        float window = 2880.0f, phase = 0.0f;
    };

    /** The reflections off the near walls: ten taps a side over the early size. */
    class EarlyReflections
    {
    public:
        void allocate (float rate)
        {
            left.allocate (static_cast<int> (0.1f * rate));
            right.allocate (static_cast<int> (0.1f * rate));
            spread.setTime (80.0f, rate);
        }

        void clear() noexcept   { left.clear(); right.clear(); }
        void setSpread (float samples, bool jump) noexcept   { if (jump) spread.jump (samples); else spread.target = samples; }

        void tick (float inL, float inR, float& outL, float& outR) noexcept
        {
            static constexpr std::array<float, 10> timesL { 0.043f, 0.137f, 0.212f, 0.297f, 0.373f, 0.452f, 0.531f, 0.668f, 0.794f, 0.945f };
            static constexpr std::array<float, 10> timesR { 0.067f, 0.115f, 0.239f, 0.281f, 0.401f, 0.486f, 0.577f, 0.623f, 0.828f, 0.913f };

            left.write (inL);
            right.write (inR);

            const auto s = spread.next();
            float l = 0.0f, r = 0.0f;

            for (size_t i = 0; i < 10; ++i)
            {
                const auto sign = (i & 1) == 0 ? 1.0f : -1.0f;
                l += sign * (1.0f - 0.7f * timesL[i]) * (0.7f * left.readLinear (timesL[i] * s) + 0.3f * right.readLinear (timesR[i] * s));
                r += sign * (1.0f - 0.7f * timesR[i]) * (0.7f * right.readLinear (timesR[i] * s) + 0.3f * left.readLinear (timesL[i] * s));
            }

            outL = 0.38f * l;
            outR = 0.38f * r;
        }

    private:
        DelayLine left, right;
        Smooth spread;
    };

    /** The tail's shape: a gain that follows the sound feeding it (the onsets, whether it plays). */
    class ShapeEnvelope
    {
    public:
        void prepare (float sampleRate) noexcept
        {
            rate = sampleRate;
            fastUp = settle (0.5f, rate);
            fastDown = settle (25.0f, rate);
            slowUp = settle (40.0f, rate);
            slowDown = settle (400.0f, rate);
            opening = settle (1.5f, rate);
            closing = settle (30.0f, rate);
            dipping = settle (3.0f, rate);
            reset();
        }

        void reset() noexcept
        {
            fast = slow = 0.0f;
            sinceOnset = 1.0e9f;
            hold = 0.0f;
            gain = shape == Shape::reverse || shape == Shape::gate ? 0.0f : 1.0f;
        }

        void set (Shape newShape, float ms) noexcept
        {
            if (newShape != shape)
            {
                shape = newShape;
                reset();
            }

            length = std::max (1.0f, ms * 0.001f * rate);
        }

        float process (float level) noexcept
        {
            if (shape == Shape::decay)
                return 1.0f;

            fast += (level > fast ? fastUp : fastDown) * (level - fast);
            slow += (level > slow ? slowUp : slowDown) * (level - slow);
            sinceOnset += 1.0f;

            if (fast > 2.0f * slow + 1.0e-4f && fast > 0.002f && sinceOnset > 0.06f * rate)
                sinceOnset = 0.0f;

            float target = 1.0f;

            switch (shape)
            {
                case Shape::gate:
                    hold = fast > 0.003f ? length : hold - 1.0f;
                    target = hold > 0.0f ? 1.0f : 0.0f;
                    gain += (target > gain ? opening : closing) * (target - gain);
                    break;

                case Shape::reverse:
                {
                    const auto t = sinceOnset / length;
                    target = t < 1.0f ? t * t : 0.0f;
                    gain += (target > gain ? 0.5f : closing) * (target - gain);
                    break;
                }

                case Shape::bloom:
                {
                    // A swell over the shape's length (smoothstep), after a dip at the hit.
                    const auto t = std::min (1.0f, sinceOnset / length);
                    target = 0.03f + 0.97f * t * t * (3.0f - 2.0f * t);
                    gain += (target > gain ? 0.5f : dipping) * (target - gain);
                    break;
                }

                case Shape::decay:
                    break;
            }

            return gain;
        }

    private:
        Shape shape = Shape::decay;
        float rate = 48000.0f, length = 16000.0f;
        float fast = 0.0f, slow = 0.0f, sinceOnset = 1.0e9f, hold = 0.0f, gain = 1.0f;
        float fastUp = 0.1f, fastDown = 0.001f, slowUp = 0.001f, slowDown = 0.0001f;
        float opening = 0.01f, closing = 0.001f, dipping = 0.01f;
    };

    /** The reverb: pre-delay, colour, early reflections, a voicing's body, its shape. */
    class Reverb
    {
    public:
        void prepare (double sampleRate)
        {
            rate = static_cast<float> (sampleRate > 0.0 ? sampleRate : 48000.0);

            preL.allocate (static_cast<int> (0.26f * rate));
            preR.allocate (static_cast<int> (0.26f * rate));
            network.allocate (rate);
            plate.allocate (rate);
            spring.allocate (rate);
            octave.allocate (rate);
            early.allocate (rate);
            shaper.prepare (rate);
            degradeL.prepare (rate, 32000.0f, 9000.0f, 12);
            degradeR.prepare (rate, 32000.0f, 9000.0f, 12);
            shimmerLow.setCutoff (6500.0f, rate);
            shimmerHigh.setCutoff (250.0f, rate);

            preDelay.setTime (60.0f, rate);
            inputGain.setTime (40.0f, rate);
            fade.setTime (6.0f, rate);
            earlyLevel.setTime (30.0f, rate);
            fade.jump (1.0f);
            inputGain.jump (1.0f);

            prepared = true;
            applied = false;
            set (settings);
            reset();
        }

        void reset() noexcept
        {
            preL.clear();
            preR.clear();
            clearBody();
            early.clear();
            shaper.reset();
            lastL = lastR = 0.0f;
        }

        /** jump: at once, no glides or fades (its first settings, before it plays). */
        void set (const ReverbSettings& s, bool jump = false) noexcept
        {
            if (jump && prepared)
            {
                if (s.voicing != settings.voicing)
                    clearBody();

                applied = false;
                pendingVoicing = false;
                fade.jump (1.0f);
                inputGain.jump (s.freeze ? 0.0f : 1.0f);
            }

            const auto voicingChanged = applied && s.voicing != settings.voicing;
            settings = s;

            if (! prepared)
                return;

            if (voicingChanged)
            {
                pendingVoicing = true;      // it fades out, clears, takes the new voicing, fades in
                fade.target = 0.0f;
            }

            if (! pendingVoicing)
                applyBody (! applied);

            preDelay.target = clampf (s.preDelayMs, 0.0f, 250.0f) * 0.001f * rate;
            inputGain.target = s.freeze ? 0.0f : 1.0f;

            const auto& v = voiceSpec (s.voicing);
            earlyLevel.target = clampf (v.earlyOwn + s.early, 0.0f, 1.0f);
            early.setSpread ((6.0f + 84.0f * clampf (s.earlySize, 0.0f, 1.0f)) * 0.001f * rate, ! applied);

            tiltL.set (clampf (s.colour, -1.0f, 1.0f), rate);
            tiltR.set (clampf (s.colour, -1.0f, 1.0f), rate);
            shaper.set (s.shape, clampf (s.shapeMs, 10.0f, 4000.0f));

            if (! applied)
            {
                preDelay.jump (preDelay.target);
                earlyLevel.jump (earlyLevel.target);
            }

            applied = true;
        }

        /** Its RT60 now (seconds): how long its tail rings. */
        float getRt60() const noexcept
        {
            const auto& v = voiceSpec (settings.voicing);
            return v.rtShort * std::pow (v.rtLong / v.rtShort, clampf (settings.decay, 0.0f, 1.0f));
        }

        void tick (float inL, float inR, float& outL, float& outR) noexcept
        {
            const auto g = inputGain.next();
            preL.write (inL * g);
            preR.write (inR * g);

            const auto d = preDelay.next();
            auto xL = d >= 1.0f ? preL.readLinear (d) : inL * g;
            auto xR = d >= 1.0f ? preR.readLinear (d) : inR * g;

            xL = tiltL.process (xL);
            xR = tiltR.process (xR);

            const auto shapeGain = shaper.process (std::max (std::abs (xL), std::abs (xR)));

            float eL = 0.0f, eR = 0.0f;
            early.tick (xL, xR, eL, eR);
            const auto e = earlyLevel.next();

            float bL = 0.0f, bR = 0.0f;
            const auto voicing = settings.voicing;

            switch (voicing)
            {
                case Voicing::plate:
                    plate.tick (xL, xR, bL, bR);
                    break;

                case Voicing::spring:
                    spring.tick (xL, xR, bL, bR);
                    break;

                case Voicing::vintage:
                    network.tick (xL + 0.4f * e * eL, xR + 0.4f * e * eR, bL, bR);
                    bL = degradeL.process (bL);
                    bR = degradeR.process (bR);
                    break;

                case Voicing::shimmer:
                {
                    // An octave up of what rings, fed back in: each pass climbs.
                    const auto up = octave.process (0.5f * (lastL + lastR));
                    const auto feed = settings.freeze ? 0.0f : 0.3f * softClip (shimmerHigh.highpass (shimmerLow.lowpass (up)));
                    network.tick (xL + feed, xR + feed, bL, bR);
                    lastL = bL;
                    lastR = bR;
                    break;
                }

                case Voicing::hall:
                case Voicing::room:
                case Voicing::chamber:
                case Voicing::cathedral:
                    network.tick (xL + 0.4f * e * eL, xR + 0.4f * e * eR, bL, bR);
                    break;
            }

            const auto f = fade.next();

            if (pendingVoicing && f < 0.001f)
            {
                pendingVoicing = false;
                clearBody();
                applyBody (true);
                fade.target = 1.0f;
            }

            const auto trim = voiceSpec (voicing).trim * f * shapeGain;
            outL = trim * (bL + e * eL);
            outR = trim * (bR + e * eR);
        }

        const ReverbSettings& getSettings() const noexcept   { return settings; }

    private:
        void clearBody() noexcept
        {
            network.clear();
            plate.clear();
            spring.clear();
            octave.clear();
            degradeL.reset();
            degradeR.reset();
            shimmerLow.reset();
            shimmerHigh.reset();
            lastL = lastR = 0.0f;
        }

        void applyBody (bool jump) noexcept
        {
            const auto& s = settings;
            const auto& v = voiceSpec (s.voicing);
            const auto sizeScale = v.sizeSmall + (v.sizeLarge - v.sizeSmall) * clampf (s.size, 0.0f, 1.0f);
            const auto rt = getRt60();
            const auto highRt = rt * std::max (0.05f, v.brightness * (1.0f - 0.92f * clampf (s.damping, 0.0f, 1.0f)) + 0.04f);
            const auto modScale = 0.2f + 3.2f * clampf (s.mod, 0.0f, 1.0f);

            switch (s.voicing)
            {
                case Voicing::plate:
                    plate.set (sizeScale, rt, s.damping, s.texture, modScale, s.freeze, jump);
                    break;

                case Voicing::spring:
                    spring.set (sizeScale, rt, s.damping, s.freeze, jump);
                    break;

                case Voicing::hall:
                case Voicing::room:
                case Voicing::chamber:
                case Voicing::cathedral:
                case Voicing::vintage:
                case Voicing::shimmer:
                    network.set (v, sizeScale, rt, highRt, s.texture, modScale, s.freeze, rate, jump);
                    break;
            }
        }

        ReverbSettings settings;
        float rate = 48000.0f;
        bool prepared = false, applied = false, pendingVoicing = false;

        DelayLine preL, preR;
        Tilt tiltL, tiltR;
        Network network;
        Plate plate;
        Spring spring;
        OctaveUp octave;
        OnePole shimmerLow, shimmerHigh;
        Degrade degradeL, degradeR;
        EarlyReflections early;
        ShapeEnvelope shaper;
        Smooth preDelay, inputGain, fade, earlyLevel;
        float lastL = 0.0f, lastR = 0.0f;
    };

    //==============================================================================
    // THE ECHO
    //==============================================================================
    enum class EchoMode : int { single, dual, pingPong, rhythm };
    enum class EchoStyle : int { tape, bbd, oilCan, clean, vintage };

    struct EchoSettings
    {
        EchoMode mode = EchoMode::single;
        EchoStyle style = EchoStyle::tape;
        float timeMs = 375.0f;              // an echo (Rhythm: a step)
        float time2 = 0.75f;                // Dual: the second echo's time against the first
        float feedback = 0.35f;             // 0..1
        float groove = 0.0f;                // -0.5..0.5: every second echo late (+) or early (-)
        float feelMs = 0.0f;                // every echo late (+) or early (-)
        float accent = 0.0f;                // -1..1: the first (+) or second (-) of each pair louder
        float drive = 0.3f;                 // 0..1
        float mod = 0.25f;                  // 0..1
        bool freeze = false;
        std::array<float, 16> taps {};      // Rhythm: each step's level, 0..1
    };

    /** Two lines (A and B) that make every mode: Single and Ping-Pong pass the sound from one to
        the other (each its own time: groove), Dual keeps each to itself, Rhythm reads sixteen
        steps off A. Whatever goes in is coloured by the style, every pass. */
    class Echo
    {
    public:
        static constexpr float longestSeconds = 10.5f;

        void prepare (double sampleRate)
        {
            rate = static_cast<float> (sampleRate > 0.0 ? sampleRate : 48000.0);

            for (auto& line : lines)
                line.allocate (static_cast<int> (longestSeconds * rate));

            for (auto& c : colour)
            {
                c.smear1.line.allocate (static_cast<int> (0.02f * rate));
                c.smear2.line.allocate (static_cast<int> (0.02f * rate));
            }

            timeA.setTime (90.0f, rate);
            timeB.setTime (90.0f, rate);
            feel.setTime (90.0f, rate);
            feedback.setTime (30.0f, rate);
            inputGain.setTime (30.0f, rate);
            fade.setTime (6.0f, rate);
            fade.jump (1.0f);
            inputGain.jump (1.0f);

            prepared = true;
            applied = false;
            set (settings);
            reset();
        }

        void reset() noexcept
        {
            for (auto& line : lines)
                line.clear();

            for (auto& c : colour)
            {
                c.high.reset();
                c.low1.reset();
                c.low2.reset();
                c.smear1.line.clear();
                c.smear2.line.clear();
                c.degrade.reset();
            }

            backA = backB = 0.0f;
        }

        /** jump: at once, no glides or fades (its first settings, before it plays). */
        void set (const EchoSettings& s, bool jump = false) noexcept
        {
            if (jump && prepared)
            {
                if (s.mode != settings.mode)
                    reset();

                applied = false;
                pending = false;
                fade.jump (1.0f);
                inputGain.jump (s.freeze ? 0.0f : 1.0f);
            }

            const auto changed = applied && (s.mode != settings.mode || s.style != settings.style);
            settings = s;

            if (! prepared)
                return;

            if (changed)
            {
                pending = true;
                fade.target = 0.0f;
            }

            applyTimes (! applied);
            feedback.target = s.freeze ? 1.0f : clampf (s.feedback, 0.0f, 1.0f);
            inputGain.target = s.freeze ? 0.0f : 1.0f;

            if (! pending)
                applyStyle();

            if (! applied)
                feedback.jump (feedback.target);

            applied = true;
        }

        /** The time only (the song's tempo moves it every block). */
        void setTime (float ms) noexcept
        {
            if (std::abs (ms - settings.timeMs) > 1.0e-3f)
            {
                settings.timeMs = ms;
                applyTimes (false);

                if (settings.style == EchoStyle::bbd && ! pending)
                    applyStyle();
            }
        }

        /** The echoes now (before what comes in is written). */
        void read (float& outL, float& outR) noexcept
        {
            const auto tA = timeA.next(), tB = timeB.next(), dt = feel.next();
            const auto mA = modulation (0), mB = modulation (1);
            const auto f = fade.next();
            const auto& s = settings;

            if (s.mode == EchoMode::rhythm)
            {
                const auto step = tA;
                const auto groove = clampf (s.groove, -0.5f, 0.5f);
                float l = 0.0f, r = 0.0f;

                for (int k = 1; k <= 16; ++k)
                {
                    const auto level = s.taps[static_cast<size_t> (k - 1)];

                    if (level < 1.0e-4f)
                        continue;

                    const auto at = (static_cast<float> (k) + ((k & 1) != 0 ? groove : 0.0f)) * step + dt + mA;
                    const auto onBeat = (k % 4) == 0;
                    const auto accent = onBeat ? std::min (1.0f, 1.0f + s.accent) : std::min (1.0f, 1.0f - s.accent);
                    const auto v = level * accent * lines[0].readCubic (std::max (2.0f, at));
                    const auto pan = (k & 1) != 0 ? -0.35f : 0.35f;
                    l += v * (1.0f - pan);
                    r += v * (1.0f + pan);
                }

                backA = lines[0].readCubic (16.0f * step + mA);
                backB = 0.0f;
                outL = 0.6f * f * l;
                outR = 0.6f * f * r;
                return;
            }

            backA = lines[0].readCubic (tA + mA);
            backB = lines[1].readCubic (tB + mB);

            const auto outA = lines[0].readCubic (std::max (2.0f, tA + dt + mA));
            const auto outB = lines[1].readCubic (std::max (2.0f, tB + dt + mB));
            const auto gA = std::min (1.0f, 1.0f + s.accent), gB = std::min (1.0f, 1.0f - s.accent);

            switch (s.mode)
            {
                case EchoMode::single:
                    outL = outR = f * (gA * outA + gB * outB);
                    break;

                case EchoMode::dual:
                case EchoMode::pingPong:
                    outL = f * gA * outA;
                    outR = f * gB * outB;
                    break;

                case EchoMode::rhythm:
                    break;
            }
        }

        /** What comes in (mono), written after read(). returnL/R and diffuse: what goes back
            round the loop in place of the echoes (a reverb in the loop), and how much of it. */
        void write (float in, float returnL = 0.0f, float returnR = 0.0f, float diffuse = 0.0f) noexcept
        {
            const auto x = in * inputGain.next();
            const auto fb = feedback.next();
            const auto loopA = backA + diffuse * (returnL - backA);
            const auto loopB = backB + diffuse * (returnR - backB);

            switch (settings.mode)
            {
                case EchoMode::single:
                case EchoMode::pingPong:
                    lines[0].write (colourOf (0, x + fb * loopB));
                    lines[1].write (colourOf (1, fb * loopA));
                    break;

                case EchoMode::dual:
                    lines[0].write (colourOf (0, x + fb * loopA));
                    lines[1].write (colourOf (1, x + fb * loopB));
                    break;

                case EchoMode::rhythm:
                    lines[0].write (colourOf (0, x + fb * (backA + diffuse * (0.5f * (returnL + returnR) - backA))));
                    lines[1].write (0.0f);
                    break;
            }

            if (pending && fade.value < 0.001f)
            {
                pending = false;
                reset();
                applyStyle();
                fade.target = 1.0f;
            }
        }

        const EchoSettings& getSettings() const noexcept   { return settings; }

    private:
        struct Colour
        {
            OnePole high;
            Biquad low1, low2;
            Allpass smear1, smear2;
            Degrade degrade;
            Lfo wow, flutter;
            Random random;
            float drift = 0.0f;
        };

        void applyTimes (bool jump) noexcept
        {
            const auto& s = settings;
            const auto t = clampf (s.timeMs, 1.0f, 4000.0f) * 0.001f * rate;
            const auto groove = clampf (s.groove, -0.5f, 0.5f);
            const auto most = longestSeconds * rate - 0.2f * rate;
            float a = t, b = t;

            switch (s.mode)
            {
                case EchoMode::single:
                case EchoMode::pingPong:
                    a = t * (1.0f + groove);
                    b = t * (1.0f - groove);
                    break;

                case EchoMode::dual:
                    a = t;
                    b = t * clampf (s.time2, 0.1f, 2.0f) + groove * t;
                    break;

                case EchoMode::rhythm:
                    a = b = std::min (t, most / 17.0f);
                    break;
            }

            timeA.target = clampf (a, 4.0f, most);
            timeB.target = clampf (b, 4.0f, most);

            const auto shortest = std::min (timeA.target, timeB.target);
            feel.target = clampf (s.feelMs * 0.001f * rate, -0.5f * shortest, 0.25f * rate);

            if (jump)
            {
                timeA.jump (timeA.target);
                timeB.jump (timeB.target);
                feel.jump (feel.target);
            }
        }

        void applyStyle() noexcept
        {
            const auto& s = settings;
            const auto drive = clampf (s.drive, 0.0f, 1.0f);
            const auto seconds = timeA.target / rate;

            for (size_t i = 0; i < colour.size(); ++i)
            {
                auto& c = colour[i];
                const auto side = i == 0 ? 1.0f : 1.13f;

                switch (s.style)
                {
                    case EchoStyle::tape:
                        c.high.setCutoff (45.0f, rate);
                        c.low1.lowpass (7200.0f - 2000.0f * drive, 0.6f, rate);
                        c.low2.pass();
                        c.wow.setRate (0.62f * side, rate);
                        c.flutter.setRate (6.3f * side, rate);
                        wowMs = 1.0f;
                        flutterMs = 0.07f;
                        driftMs = 0.35f;
                        driveAmount = 1.0f + 3.5f * drive;
                        break;

                    case EchoStyle::bbd:
                    {
                        // A bucket brigade loses its top the longer it holds the sound.
                        const auto cut = clampf (4500.0f * std::sqrt (0.25f / std::max (0.02f, seconds)), 1800.0f, 7500.0f);
                        c.high.setCutoff (80.0f, rate);
                        c.low1.lowpass (cut, 0.7f, rate);
                        c.low2.lowpass (cut * 1.15f, 0.55f, rate);
                        c.wow.setRate (0.38f * side, rate);
                        c.flutter.setRate (0.0f, rate);
                        wowMs = 0.55f;
                        flutterMs = 0.0f;
                        driftMs = 0.0f;
                        driveAmount = 1.0f + 2.0f * drive;
                        break;
                    }

                    case EchoStyle::oilCan:
                        c.high.setCutoff (120.0f, rate);
                        c.low1.lowpass (3300.0f, 0.65f, rate);
                        c.low2.pass();
                        c.wow.setRate (4.6f * side, rate);
                        c.flutter.setRate (0.31f * side, rate);
                        wowMs = 1.5f;
                        flutterMs = 0.6f;
                        driftMs = 0.1f;
                        driveAmount = 1.0f + 2.5f * drive;
                        break;

                    case EchoStyle::clean:
                        c.high.setCutoff (10.0f, rate);
                        c.low1.lowpass (0.45f * rate, 0.707f, rate);
                        c.low2.pass();
                        c.wow.setRate (0.25f * side, rate);
                        c.flutter.setRate (0.0f, rate);
                        wowMs = 0.0f;
                        flutterMs = 0.0f;
                        driftMs = 0.0f;
                        driveAmount = 0.3f + 1.2f * drive;
                        break;

                    case EchoStyle::vintage:
                        c.high.setCutoff (50.0f, rate);
                        c.low1.pass();
                        c.low2.pass();
                        c.degrade.prepare (rate, 22050.0f, 7000.0f, 12);
                        c.wow.setRate (0.21f * side, rate);
                        c.flutter.setRate (0.0f, rate);
                        wowMs = 0.15f;
                        flutterMs = 0.0f;
                        driftMs = 0.0f;
                        driveAmount = 0.6f + 2.0f * drive;
                        break;
                }
            }

            depthScale = clampf (s.mod, 0.0f, 1.0f) * 4.0f;
        }

        float modulation (size_t line) noexcept
        {
            auto& c = colour[line];
            const auto w = c.wow.next(), fl = c.flutter.next();

            if (driftMs > 0.0f)
                c.drift += 0.0005f * (c.random.next() - c.drift);

            const auto ms = depthScale * (wowMs * w + flutterMs * fl + driftMs * 8.0f * c.drift);
            return ms * 0.001f * rate;
        }

        float colourOf (size_t line, float x) noexcept
        {
            auto& c = colour[line];

            if (settings.freeze)
                return saturate (x, 0.3f);

            x = c.high.highpass (x);
            x = c.low2.process (c.low1.process (x));

            switch (settings.style)
            {
                case EchoStyle::oilCan:
                    x = c.smear1.process (x, 0.0037f * rate, 0.6f);
                    x = c.smear2.process (x, 0.0091f * rate, 0.5f);
                    break;

                case EchoStyle::vintage:
                    x = c.degrade.process (x);
                    break;

                case EchoStyle::tape:
                case EchoStyle::bbd:
                case EchoStyle::clean:
                    break;
            }

            return saturate (x, driveAmount);
        }

        EchoSettings settings;
        float rate = 48000.0f;
        bool prepared = false, applied = false, pending = false;

        std::array<DelayLine, 2> lines;
        std::array<Colour, 2> colour { Colour { {}, {}, {}, {}, {}, {}, {}, {}, Random (0x1234567u) },
                                       Colour { {}, {}, {}, {}, {}, {}, {}, {}, Random (0x7654321u) } };
        Smooth timeA, timeB, feel, feedback, inputGain, fade;
        float backA = 0.0f, backB = 0.0f;
        float wowMs = 1.0f, flutterMs = 0.07f, driftMs = 0.35f, depthScale = 1.0f, driveAmount = 2.0f;
    };

    //==============================================================================
    // THE SPACE: the two together
    //==============================================================================
    enum class Routing : int { echoToReverb, reverbInEcho, reverbToEcho, parallel };

    struct Settings
    {
        Routing routing = Routing::echoToReverb;
        float blend = 0.5f;            // 0: all echo .. 1: all reverb (in the loop: how much goes round)
        bool echoOn = true, reverbOn = true;
        EchoSettings echo;
        ReverbSettings reverb;
    };

    /** Writes the wet's two parts apart (each as loud as Blend makes it), so a ducker can take
        either: the echo's and the reverb's. */
    class Space
    {
    public:
        void prepare (double sampleRate)
        {
            rate = static_cast<float> (sampleRate > 0.0 ? sampleRate : 48000.0);
            echo.prepare (sampleRate);
            reverb.prepare (sampleRate);
            echoGain.setTime (30.0f, rate);
            reverbGain.setTime (30.0f, rate);
            loopUp = settle (5.0f, rate);
            loopDown = settle (400.0f, rate);
            loopLevel = 0.0f;
            prepared = true;
            set (settings, true);
        }

        void reset() noexcept
        {
            echo.reset();
            reverb.reset();
        }

        void resetEcho() noexcept     { echo.reset(); }
        void resetReverb() noexcept   { reverb.reset(); }

        void set (const Settings& s, bool jump = false) noexcept
        {
            settings = s;

            if (! prepared)
                return;

            echo.set (s.echo, jump);
            reverb.set (s.reverb, jump);

            const auto b = clampf (s.blend, 0.0f, 1.0f);
            const auto both = s.echoOn && s.reverbOn;
            echoGain.target = both ? std::cos (0.5f * pi * b) : (s.echoOn ? 1.0f : 0.0f);
            reverbGain.target = both ? std::sin (0.5f * pi * b) : (s.reverbOn ? 1.0f : 0.0f);

            // A reverb in the loop gives back as much as it holds: kept from building up.
            loopTrim = 1.0f / std::sqrt (1.0f + 6.0f * reverb.getRt60());

            if (jump)
            {
                echoGain.jump (echoGain.target);
                reverbGain.jump (reverbGain.target);
            }
        }

        void setEchoTime (float ms) noexcept   { settings.echo.timeMs = ms; echo.setTime (ms); }

        void process (const float* inL, const float* inR, float* echoL, float* echoR, float* reverbL, float* reverbR, int numSamples) noexcept
        {
            const auto echoOn = settings.echoOn, reverbOn = settings.reverbOn;
            const auto routing = echoOn && reverbOn ? settings.routing : Routing::parallel;
            const auto diffuse = clampf (settings.blend, 0.0f, 1.0f);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto xL = inL[i], xR = inR[i];
                float eL = 0.0f, eR = 0.0f, rL = 0.0f, rR = 0.0f;

                switch (routing)
                {
                    case Routing::echoToReverb:
                        echo.read (eL, eR);
                        echo.write (0.5f * (xL + xR));
                        reverb.tick (eL, eR, rL, rR);
                        break;

                    case Routing::reverbToEcho:
                        reverb.tick (xL, xR, rL, rR);
                        echo.read (eL, eR);
                        echo.write (0.5f * (rL + rR));
                        break;

                    case Routing::reverbInEcho:
                    {
                        echo.read (eL, eR);
                        reverb.tick (eL, eR, rL, rR);

                        // A reverb round a loop rings up at its strongest modes: what goes back
                        // round is held under a level (a slow leveller, then a soft clip).
                        auto backL = rL * loopTrim, backR = rR * loopTrim;
                        const auto level = std::max (std::abs (backL), std::abs (backR));
                        loopLevel += (level > loopLevel ? loopUp : loopDown) * (level - loopLevel);
                        const auto hold = loopLevel > loopCeiling ? loopCeiling / loopLevel : 1.0f;
                        backL = softClip (backL * hold);
                        backR = softClip (backR * hold);
                        echo.write (0.5f * (xL + xR), backL, backR, diffuse);
                        rL *= hold;
                        rR *= hold;
                        break;
                    }

                    case Routing::parallel:
                        if (echoOn)
                        {
                            echo.read (eL, eR);
                            echo.write (0.5f * (xL + xR));
                        }

                        if (reverbOn)
                            reverb.tick (xL, xR, rL, rR);
                        break;
                }

                const auto ge = echoGain.next(), gr = reverbGain.next();
                echoL[i] = ge * eL;
                echoR[i] = ge * eR;
                reverbL[i] = gr * rL;
                reverbR[i] = gr * rR;
            }
        }

        const Settings& getSettings() const noexcept   { return settings; }

    private:
        Settings settings;
        float rate = 48000.0f, loopTrim = 0.5f;
        float loopLevel = 0.0f, loopUp = 0.01f, loopDown = 0.0001f;
        static constexpr float loopCeiling = 0.35f;
        bool prepared = false;
        Echo echo;
        Reverb reverb;
        Smooth echoGain, reverbGain;
    };

    /** The wet's last touches: its low and high cut (12 dB an octave) and its width. */
    class WetTone
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            rate = static_cast<float> (sampleRate > 0.0 ? sampleRate : 48000.0);
            set (lowHz, highHz, width);
            for (auto* f : { &lowL, &lowR, &highL, &highR }) f->reset();
        }

        /** highHz at or over 20 kHz: no high cut. lowHz at or under 20 Hz: no low cut. */
        void set (float newLowHz, float newHighHz, float newWidth) noexcept
        {
            lowHz = newLowHz;
            highHz = newHighHz;
            width = newWidth;

            if (lowHz > 20.5f) { lowL.highpass (lowHz, 0.707f, rate); lowR.highpass (lowHz, 0.707f, rate); }
            else               { lowL.pass(); lowR.pass(); }

            if (highHz < 19999.0f) { highL.lowpass (highHz, 0.707f, rate); highR.lowpass (highHz, 0.707f, rate); }
            else                   { highL.pass(); highR.pass(); }
        }

        void process (float* left, float* right, int numSamples) noexcept
        {
            const auto side = clampf (width, 0.0f, 2.0f);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto l = highL.process (lowL.process (left[i]));
                const auto r = highR.process (lowR.process (right[i]));
                const auto mid = 0.5f * (l + r), sides = 0.5f * (l - r) * side;
                left[i] = mid + sides;
                right[i] = mid - sides;
            }
        }

    private:
        float rate = 48000.0f, lowHz = 20.0f, highHz = 20000.0f, width = 1.0f;
        Biquad lowL, lowR, highL, highR;
    };
}
