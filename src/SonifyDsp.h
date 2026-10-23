// D:\workspace\Fanan_web_hub\src\SonifyDsp.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

//==============================================================================
/**
    Sonify's own sound processing: the EQ, compressor, limiter and loudness meter behind its
    built-in effects (Sonify EQ, Compressor, Limiter and Mastering), and behind the Mastering
    Assistant, which runs the very same mastering offline to find its settings.

    prepare() allocates; nothing else does, so processing is safe on the audio thread.
*/
namespace SonifyDsp
{
    constexpr double pi = 3.14159265358979323846;

    inline double dbToGain (double db) noexcept    { return std::pow (10.0, db / 20.0); }
    inline double gainToDb (double gain) noexcept  { return gain > 1.0e-12 ? 20.0 * std::log10 (gain) : -240.0; }

    //==============================================================================
    /** A second-order filter, from Robert Bristow-Johnson's cookbook, normalised (a0 = 1). */
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

        static double clampFrequency (double sampleRate, double frequency) noexcept
        {
            return juce::jlimit (1.0, sampleRate * 0.49, frequency);
        }

        static Biquad normalised (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
        {
            return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
        }

        static Biquad peak (double sampleRate, double frequency, double q, double gainDb) noexcept
        {
            if (std::abs (gainDb) < 1.0e-4)
                return {};

            const auto a = std::pow (10.0, gainDb / 40.0);
            const auto w = 2.0 * pi * clampFrequency (sampleRate, frequency) / sampleRate;
            const auto alpha = std::sin (w) / (2.0 * juce::jmax (0.05, q));
            const auto c = std::cos (w);
            return normalised (1.0 + alpha * a, -2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, -2.0 * c, 1.0 - alpha / a);
        }

        /** Shelves with the cookbook's slope S = 1. */
        static Biquad lowShelf (double sampleRate, double frequency, double gainDb) noexcept
        {
            if (std::abs (gainDb) < 1.0e-4)
                return {};

            const auto a = std::pow (10.0, gainDb / 40.0);
            const auto w = 2.0 * pi * clampFrequency (sampleRate, frequency) / sampleRate;
            const auto c = std::cos (w);
            const auto beta = 2.0 * std::sqrt (a) * std::sin (w) / std::sqrt (2.0);
            return normalised (a * ((a + 1.0) - (a - 1.0) * c + beta),
                               2.0 * a * ((a - 1.0) - (a + 1.0) * c),
                               a * ((a + 1.0) - (a - 1.0) * c - beta),
                               (a + 1.0) + (a - 1.0) * c + beta,
                               -2.0 * ((a - 1.0) + (a + 1.0) * c),
                               (a + 1.0) + (a - 1.0) * c - beta);
        }

        static Biquad highShelf (double sampleRate, double frequency, double gainDb) noexcept
        {
            if (std::abs (gainDb) < 1.0e-4)
                return {};

            const auto a = std::pow (10.0, gainDb / 40.0);
            const auto w = 2.0 * pi * clampFrequency (sampleRate, frequency) / sampleRate;
            const auto c = std::cos (w);
            const auto beta = 2.0 * std::sqrt (a) * std::sin (w) / std::sqrt (2.0);
            return normalised (a * ((a + 1.0) + (a - 1.0) * c + beta),
                               -2.0 * a * ((a - 1.0) + (a + 1.0) * c),
                               a * ((a + 1.0) + (a - 1.0) * c - beta),
                               (a + 1.0) - (a - 1.0) * c + beta,
                               2.0 * ((a - 1.0) - (a + 1.0) * c),
                               (a + 1.0) - (a - 1.0) * c - beta);
        }

        static Biquad highPass (double sampleRate, double frequency, double q) noexcept
        {
            const auto w = 2.0 * pi * clampFrequency (sampleRate, frequency) / sampleRate;
            const auto c = std::cos (w);
            const auto alpha = std::sin (w) / (2.0 * juce::jmax (0.05, q));
            return normalised ((1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
        }

        static Biquad lowPass (double sampleRate, double frequency, double q) noexcept
        {
            const auto w = 2.0 * pi * clampFrequency (sampleRate, frequency) / sampleRate;
            const auto c = std::cos (w);
            const auto alpha = std::sin (w) / (2.0 * juce::jmax (0.05, q));
            return normalised ((1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
        }

        /** A band of about an octave around frequency (0 dB at its centre): for measuring. */
        static Biquad bandPass (double sampleRate, double frequency, double q) noexcept
        {
            const auto w = 2.0 * pi * clampFrequency (sampleRate, frequency) / sampleRate;
            const auto c = std::cos (w);
            const auto alpha = std::sin (w) / (2.0 * juce::jmax (0.05, q));
            return normalised (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
        }

        bool isIdentity() const noexcept
        {
            return b0 == 1.0 && b1 == 0.0 && b2 == 0.0 && a1 == 0.0 && a2 == 0.0;
        }
    };

    /** One channel's memory of a Biquad (transposed direct form II, in double). */
    struct BiquadState
    {
        double z1 = 0.0, z2 = 0.0;

        float process (const Biquad& c, float input) noexcept
        {
            const double x = input;
            const double y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return static_cast<float> (y);
        }

        double processDouble (const Biquad& c, double x) noexcept
        {
            const double y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return y;
        }

        void reset() noexcept   { z1 = z2 = 0.0; }
    };

    //==============================================================================
    /** Four times oversampled reconstruction of a signal, to find its true peaks between the
        samples (as ITU-R BS.1770 measures them): a windowed-sinc filter in four phases. */
    class TruePeakInterpolator
    {
    public:
        static constexpr int taps = 24;        // per phase
        static constexpr int delay = taps / 2; // the points found lie about this many samples back

        TruePeakInterpolator()
        {
            // A Kaiser-windowed sinc cut at the original Nyquist, each phase summing to 1. Stored
            // oldest tap first, to run along the history as it's kept.
            constexpr int length = taps * 4;
            constexpr double centre = (length - 1) / 2.0;
            constexpr double beta = 5.0;

            const auto bessel = [] (double x)
            {
                double sum = 1.0, term = 1.0;

                for (int k = 1; k < 32; ++k)
                {
                    term *= (x / (2.0 * k)) * (x / (2.0 * k));
                    sum += term;
                }

                return sum;
            };

            std::array<double, length> h {};

            for (int i = 0; i < length; ++i)
            {
                const auto t = (i - centre) / 4.0;
                const auto sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (pi * t) / (pi * t);
                const auto ratio = (i - centre) / centre;
                const auto window = bessel (beta * std::sqrt (juce::jmax (0.0, 1.0 - ratio * ratio))) / bessel (beta);
                h[static_cast<size_t> (i)] = sinc * window;
            }

            for (int phase = 0; phase < 4; ++phase)
            {
                double sum = 0.0;

                for (int j = 0; j < taps; ++j)
                    sum += h[static_cast<size_t> (4 * j + phase)];

                // Tap j weighs the sample j back; kept oldest first.
                for (int j = 0; j < taps; ++j)
                    coefficients[static_cast<size_t> (phase)][static_cast<size_t> (taps - 1 - j)]
                        = static_cast<float> (h[static_cast<size_t> (4 * j + phase)] / sum);
            }
        }

        void reset() noexcept
        {
            history.fill (0.0f);
            position = 0;
        }

        /** Takes the next sample; answers the largest magnitude among the sample `delay` back and
            the four points it can now reconstruct just after it. */
        float push (float sample) noexcept
        {
            // The history is kept twice over, so the last `taps` samples always lie in a row.
            position = (position + 1) % taps;
            history[static_cast<size_t> (position)] = sample;
            history[static_cast<size_t> (position + taps)] = sample;

            const auto* recent = history.data() + position + 1;   // oldest .. newest
            float peak = std::abs (recent[taps - 1 - delay]);

            for (int phase = 0; phase < 4; ++phase)
            {
                const auto* c = coefficients[static_cast<size_t> (phase)].data();
                float sum = 0.0f;

                for (int j = 0; j < taps; ++j)
                    sum += c[j] * recent[j];

                peak = juce::jmax (peak, std::abs (sum));
            }

            return peak;
        }

    private:
        std::array<std::array<float, taps>, 4> coefficients {};
        std::array<float, 2 * taps> history {};
        int position = 0;
    };

    //==============================================================================
    /** A feed-forward, stereo-linked compressor with a soft knee (after Giannoulis, Massberg and
        Reiss's design): its gain reduction follows attack and release smoothly. It can listen to
        a sidechain (key) instead of the signal it compresses. */
    class Compressor
    {
    public:
        struct Settings
        {
            float thresholdDb = 0.0f;
            float ratio = 1.0f;                  // 1: no compression
            float attackMs = 10.0f, releaseMs = 120.0f;
            float kneeDb = 6.0f;
            float makeupDb = 0.0f;
            float mix = 1.0f;                    // parallel compression: 0 dry .. 1 compressed
            bool rms = true;                     // detector: RMS (smooth) or peak (fast)
        };

        void prepare (double rate) noexcept
        {
            sampleRate = rate > 0.0 ? rate : 48000.0;
            setSettings (settings);
            reset();
        }

        void setSettings (const Settings& s) noexcept
        {
            settings = s;
            const auto coefficient = [this] (double ms) { return std::exp (-1.0 / (juce::jmax (0.01, ms) * 0.001 * sampleRate)); };
            attack = coefficient (s.attackMs);
            release = coefficient (s.releaseMs);
            detector = coefficient (10.0);       // the RMS window
            slope = s.ratio > 1.0f ? 1.0 - 1.0 / static_cast<double> (s.ratio) : 0.0;
            makeup = dbToGain (s.makeupDb);
        }

        const Settings& getSettings() const noexcept   { return settings; }

        void reset() noexcept
        {
            power = 0.0;
            held = smoothed = 0.0;
            reductionTotal = 0.0;
            reductionSamples = 0;
        }

        /** In place. The key, when given, is what's listened to. */
        void process (float* left, float* right, int numSamples, const float* keyLeft = nullptr, const float* keyRight = nullptr) noexcept
        {
            const auto threshold = static_cast<double> (settings.thresholdDb);
            const auto knee = juce::jmax (0.0, static_cast<double> (settings.kneeDb));
            const auto wet = juce::jlimit (0.0, 1.0, static_cast<double> (settings.mix));
            const auto active = slope > 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                const double l = keyLeft != nullptr ? keyLeft[i] : left[i];
                const double r = keyRight != nullptr ? keyRight[i] : right[i];
                double level;

                if (settings.rms)
                {
                    power = detector * power + (1.0 - detector) * 0.5 * (l * l + r * r);
                    level = power > 1.0e-14 ? 10.0 * std::log10 (power) + 3.0103 : -140.0;   // a sine reads its peak
                }
                else
                {
                    const auto peakValue = juce::jmax (std::abs (l), std::abs (r));
                    level = peakValue > 1.0e-7 ? 20.0 * std::log10 (peakValue) : -140.0;
                }

                // How far over: none below the knee, a curve through it, the ratio above.
                double over = 0.0;

                if (active)
                {
                    const auto above = level - threshold;

                    if (2.0 * above <= -knee)
                        over = 0.0;
                    else if (knee > 0.0 && 2.0 * std::abs (above) <= knee)
                        over = slope * (above + knee * 0.5) * (above + knee * 0.5) / (2.0 * knee);
                    else
                        over = slope * above;
                }

                // Smooth: release from the peak of the reduction, attack towards it.
                held = juce::jmax (over, release * held + (1.0 - release) * over);
                smoothed = attack * smoothed + (1.0 - attack) * held;

                reductionTotal += smoothed;
                ++reductionSamples;

                const auto gain = static_cast<float> (dbToGainFast (-smoothed) * makeup);

                if (wet >= 1.0)
                {
                    left[i] *= gain;
                    right[i] *= gain;
                }
                else
                {
                    const auto mixed = static_cast<float> ((1.0 - wet) + wet * gain);
                    left[i] *= mixed;
                    right[i] *= mixed;
                }
            }
        }

        double getReductionDb() const noexcept      { return smoothed; }

        /** The reduction summed since the last call, and over how many samples (the assistant's measure). */
        double takeReduction (juce::int64& samples) noexcept
        {
            const auto total = reductionTotal;
            samples = reductionSamples;
            reductionTotal = 0.0;
            reductionSamples = 0;
            return total;
        }

    private:
        static double dbToGainFast (double db) noexcept   { return std::exp (db * 0.11512925464970229); }   // ln(10) / 20

        Settings settings;
        double sampleRate = 48000.0;
        double attack = 0.0, release = 0.0, detector = 0.0, slope = 0.0, makeup = 1.0;
        double power = 0.0, held = 0.0, smoothed = 0.0;
        double reductionTotal = 0.0;
        juce::int64 reductionSamples = 0;
    };

    //==============================================================================
    /** A brickwall limiter that looks ahead and listens for true peaks, so nothing leaves it
        above the ceiling, even between the samples. Its delay: getLatency() samples. */
    class Limiter
    {
    public:
        void prepare (double rate) noexcept
        {
            sampleRate = rate > 0.0 ? rate : 48000.0;
            window = juce::jmax (2, static_cast<int> (std::lround (0.0015 * sampleRate)));   // 1.5 ms ahead
            delayLength = window - 1 + TruePeakInterpolator::delay;

            for (auto& line : delayLines)
                line.assign (static_cast<size_t> (delayLength + 1), 0.0f);

            minimumValues.assign (static_cast<size_t> (window + 1), 1.0);
            minimumTimes.assign (static_cast<size_t> (window + 1), 0);
            averageRing.assign (static_cast<size_t> (window), 1.0);
            setSettings (ceilingDb, releaseMs);
            reset();
        }

        void setSettings (float newCeilingDb, float newReleaseMs) noexcept
        {
            ceilingDb = newCeilingDb;
            releaseMs = newReleaseMs;
            ceiling = dbToGain (juce::jmin (0.0f, newCeilingDb) - 0.1);   // a hair under, for the interpolation's error
            release = std::exp (-1.0 / (juce::jmax (1.0f, newReleaseMs) * 0.001 * sampleRate));
        }

        int getLatency() const noexcept   { return delayLength; }

        /** How far it's turning the sound down now, in dB (0: not at all), for a meter. */
        double getReductionDb() const noexcept   { return gainNow < 1.0 ? -20.0 * std::log10 (juce::jmax (1.0e-6, gainNow)) : 0.0; }

        void reset() noexcept
        {
            for (auto& line : delayLines)
                std::fill (line.begin(), line.end(), 0.0f);

            for (auto& detector : detectors)
                detector.reset();

            writeAt = 0;
            time = 0;
            head = tail = 0;
            std::fill (averageRing.begin(), averageRing.end(), 1.0);
            averageAt = 0;
            averageSum = static_cast<double> (window);
            released = 1.0;
            maxReductionDb = 0.0;
            reductionTotal = 0.0;
            reductionSamples = 0;
        }

        void process (float* left, float* right, int numSamples) noexcept
        {
            const auto length = delayLength + 1;
            const auto ringSize = static_cast<int> (minimumValues.size());

            for (int i = 0; i < numSamples; ++i)
            {
                // The true peak just found, and the gain that keeps it under the ceiling.
                const auto peak = juce::jmax (detectors[0].push (left[i]), detectors[1].push (right[i]));
                const double needed = peak > ceiling ? ceiling / peak : 1.0;

                // The lowest gain needed over the window ahead (a running minimum)...
                while (head != tail)
                {
                    const auto last = (tail + ringSize - 1) % ringSize;

                    if (minimumValues[static_cast<size_t> (last)] < needed)
                        break;

                    tail = last;
                }

                minimumValues[static_cast<size_t> (tail)] = needed;
                minimumTimes[static_cast<size_t> (tail)] = time;
                tail = (tail + 1) % ringSize;

                while (minimumTimes[static_cast<size_t> (head)] <= time - window)
                    head = (head + 1) % ringSize;

                const auto lowest = minimumValues[static_cast<size_t> (head)];

                // ...released slowly once it's past (never above what's needed)...
                released = lowest <= released ? lowest : released + (lowest - released) * (1.0 - release);

                // ...and averaged across the window, so it's reached gently before the peak arrives.
                averageSum += released - averageRing[static_cast<size_t> (averageAt)];
                averageRing[static_cast<size_t> (averageAt)] = released;
                averageAt = (averageAt + 1) % window;
                const auto gain = juce::jmin (1.0, averageSum / window);
                gainNow = gain;

                // The audio, late by the look-ahead, leaves at that gain.
                const auto readAt = (writeAt + 1) % length;
                delayLines[0][static_cast<size_t> (writeAt)] = left[i];
                delayLines[1][static_cast<size_t> (writeAt)] = right[i];
                const auto outLeft = static_cast<float> (delayLines[0][static_cast<size_t> (readAt)] * gain);
                const auto outRight = static_cast<float> (delayLines[1][static_cast<size_t> (readAt)] * gain);
                writeAt = readAt;
                ++time;

                // A last guard against rounding: no sample over the ceiling.
                const auto limit = static_cast<float> (ceiling);
                left[i] = juce::jlimit (-limit, limit, outLeft);
                right[i] = juce::jlimit (-limit, limit, outRight);

                const auto reduction = -gainToDb (gain);
                maxReductionDb = juce::jmax (maxReductionDb, reduction);
                reductionTotal += reduction;
                ++reductionSamples;
            }
        }

        double getMaxReductionDb() const noexcept    { return maxReductionDb; }

        double takeReduction (juce::int64& samples) noexcept
        {
            const auto total = reductionTotal;
            samples = reductionSamples;
            reductionTotal = 0.0;
            reductionSamples = 0;
            return total;
        }

    private:
        double sampleRate = 48000.0;
        float ceilingDb = -1.0f, releaseMs = 80.0f;
        double ceiling = 1.0, release = 0.0;
        int window = 72, delayLength = 77;

        std::array<TruePeakInterpolator, 2> detectors;
        std::array<std::vector<float>, 2> delayLines;
        int writeAt = 0;
        juce::int64 time = 0;

        std::vector<double> minimumValues;           // the running minimum's candidates, oldest first
        std::vector<juce::int64> minimumTimes;
        int head = 0, tail = 0;

        std::vector<double> averageRing;
        int averageAt = 0;
        double averageSum = 0.0, released = 1.0;
        double gainNow = 1.0;                        // the gain the last sample left at (a meter's)

        double maxReductionDb = 0.0, reductionTotal = 0.0;
        juce::int64 reductionSamples = 0;
    };

    //==============================================================================
    /** Loudness as ITU-R BS.1770-4 and EBU R 128 measure it: K-weighted, in 400 ms blocks
        every 100 ms, gated (absolute at -70 LUFS, then 10 LU under the mean) for the
        integrated loudness of a whole song. Stereo, both channels weighted 1. */
    class LoudnessMeter
    {
    public:
        void prepare (double rate)
        {
            sampleRate = rate > 0.0 ? rate : 48000.0;

            // The K-weighting's two stages for this rate, from their analog design (as libebur128 does).
            {
                const auto f0 = 1681.974450955533, g = 3.999843853973347, q = 0.7071752369554196;
                const auto k = std::tan (pi * f0 / sampleRate);
                const auto vh = std::pow (10.0, g / 20.0);
                const auto vb = std::pow (vh, 0.4996667741545416);
                const auto a0 = 1.0 + k / q + k * k;
                shelf = { (vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
                          2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
            }

            {
                const auto f0 = 38.13547087602444, q = 0.5003270373238773;
                const auto k = std::tan (pi * f0 / sampleRate);
                const auto a0 = 1.0 + k / q + k * k;
                highPass = { 1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
            }

            stepLength = juce::jmax (1, static_cast<int> (std::lround (sampleRate * 0.1)));
            reset();
        }

        void reset()
        {
            for (auto& state : states)
                state.reset();

            steps.clear();
            stepSum = 0.0;
            stepCount = 0;
        }

        void process (const float* left, const float* right, int numSamples)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto l = states[2].processDouble (highPass, states[0].processDouble (shelf, left[i]));
                const auto r = states[3].processDouble (highPass, states[1].processDouble (shelf, right[i]));
                stepSum += l * l + r * r;

                if (++stepCount == stepLength)
                {
                    steps.push_back (stepSum / stepLength);
                    stepSum = 0.0;
                    stepCount = 0;
                }
            }
        }

        /** Each 400 ms block's mean square (both channels added), a block every 100 ms. */
        std::vector<double> getBlocks() const
        {
            std::vector<double> blocks;

            for (size_t i = 3; i < steps.size(); ++i)
                blocks.push_back ((steps[i] + steps[i - 1] + steps[i - 2] + steps[i - 3]) / 4.0);

            return blocks;
        }

        static double loudnessOf (double meanSquare) noexcept
        {
            return meanSquare > 1.0e-20 ? -0.691 + 10.0 * std::log10 (meanSquare) : -200.0;
        }

        /** The gate a block must pass to count in the integrated loudness (its mean square). */
        static double relativeGate (const std::vector<double>& blocks)
        {
            const auto absolute = std::pow (10.0, (-70.0 + 0.691) / 10.0);
            double sum = 0.0;
            int count = 0;

            for (const auto z : blocks)
                if (z > absolute) { sum += z; ++count; }

            if (count == 0)
                return absolute;

            return juce::jmax (absolute, sum / count * std::pow (10.0, -10.0 / 10.0));
        }

        /** Integrated loudness in LUFS; -200 for silence. */
        static double integrated (const std::vector<double>& blocks)
        {
            const auto gate = relativeGate (blocks);
            double sum = 0.0;
            int count = 0;

            for (const auto z : blocks)
                if (z > gate) { sum += z; ++count; }

            return count > 0 ? loudnessOf (sum / count) : -200.0;
        }

        double getIntegrated() const     { return integrated (getBlocks()); }

        /** The 100 ms steps so far (mean squares): the assistant lines other measures up with them. */
        const std::vector<double>& getSteps() const noexcept   { return steps; }
        int getStepLength() const noexcept                     { return stepLength; }

    private:
        double sampleRate = 48000.0;
        Biquad shelf, highPass;
        std::array<BiquadState, 4> states;
        std::vector<double> steps;
        double stepSum = 0.0;
        int stepLength = 4800, stepCount = 0;
    };

    /** The highest true peak of a stereo signal so far. */
    class TruePeakMeter
    {
    public:
        void reset()
        {
            for (auto& detector : detectors)
                detector.reset();

            peak = 0.0f;
        }

        void process (const float* left, const float* right, int numSamples) noexcept
        {
            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax (peak, detectors[0].push (left[i]), detectors[1].push (right[i]));
        }

        double getPeakDb() const noexcept   { return gainToDb (peak); }

    private:
        std::array<TruePeakInterpolator, 2> detectors;
        float peak = 0.0f;
    };

    //==============================================================================
    /** Everything Sonify Mastering does, in order: the input level, a low cut, five EQ bands (a
        low shelf, three bells, an air shelf), glue compression, the drive into the limiter (what
        sets the loudness), stereo width, then the true-peak limiter and the output level. */
    struct MasteringSettings
    {
        float inputGainDb = 0.0f;
        float lowCutHz = 20.0f;                       // 10 or below: off
        float lowGainDb = 0.0f, lowHz = 90.0f;        // low shelf
        float lowMidGainDb = 0.0f, lowMidHz = 250.0f;
        float midGainDb = 0.0f, midHz = 1000.0f;
        float presenceGainDb = 0.0f, presenceHz = 3500.0f;
        float airGainDb = 0.0f, airHz = 10000.0f;     // high shelf
        float eqAmount = 1.0f;                        // 0..1, scales the five bands
        Compressor::Settings compressor { 0.0f, 1.0f, 25.0f, 200.0f, 6.0f, 0.0f, 1.0f, true };
        float driveDb = 0.0f;                         // into the limiter
        float width = 1.0f;                           // 0 mono .. 1 as it is .. 2 wide
        float ceilingDb = -1.0f, limiterReleaseMs = 80.0f;
        float outputDb = 0.0f;
    };

    class MasteringChain
    {
    public:
        static constexpr int bands = 6;               // the low cut and the five EQ bands

        void prepare (double rate)
        {
            sampleRate = rate > 0.0 ? rate : 48000.0;
            compressor.prepare (sampleRate);
            limiter.prepare (sampleRate);
            setSettings (settings);
            reset();
        }

        void setSettings (const MasteringSettings& s) noexcept
        {
            settings = s;
            const auto amount = juce::jlimit (0.0f, 1.0f, s.eqAmount);
            std::array<bool, bands> wasOff {};

            for (size_t band = 0; band < bands; ++band)
                wasOff[band] = filters[band].isIdentity();

            filters[0] = s.lowCutHz > 10.0f ? Biquad::highPass (sampleRate, s.lowCutHz, 0.7071) : Biquad {};
            filters[1] = Biquad::lowShelf (sampleRate, s.lowHz, s.lowGainDb * amount);
            filters[2] = Biquad::peak (sampleRate, s.lowMidHz, 0.9, s.lowMidGainDb * amount);
            filters[3] = Biquad::peak (sampleRate, s.midHz, 0.8, s.midGainDb * amount);
            filters[4] = Biquad::peak (sampleRate, s.presenceHz, 0.8, s.presenceGainDb * amount);
            filters[5] = Biquad::highShelf (sampleRate, s.airHz, s.airGainDb * amount);

            // A band switched off kept what it last held (it's skipped): back on, it starts clean.
            for (size_t band = 0; band < bands; ++band)
                if (wasOff[band] && ! filters[band].isIdentity())
                    for (auto& state : states[band])
                        state.reset();

            inputGain = static_cast<float> (dbToGain (s.inputGainDb));
            drive = static_cast<float> (dbToGain (s.driveDb));
            outputGain = static_cast<float> (dbToGain (juce::jmin (0.0f, s.outputDb)));
            compressor.setSettings (s.compressor);
            limiter.setSettings (s.ceilingDb, s.limiterReleaseMs);
        }

        const MasteringSettings& getSettings() const noexcept   { return settings; }

        void reset() noexcept
        {
            for (auto& pair : states)
                for (auto& state : pair)
                    state.reset();

            compressor.reset();
            limiter.reset();
        }

        /** The input gain and the EQ, in place. */
        void processEq (float* left, float* right, int numSamples) noexcept
        {
            for (int i = 0; i < numSamples; ++i)
            {
                auto l = left[i] * inputGain;
                auto r = right[i] * inputGain;

                for (size_t band = 0; band < bands; ++band)
                {
                    if (filters[band].isIdentity())
                        continue;

                    l = states[band][0].process (filters[band], l);
                    r = states[band][1].process (filters[band], r);
                }

                left[i] = l;
                right[i] = r;
            }
        }

        /** The glue compression, in place. */
        void processGlue (float* left, float* right, int numSamples) noexcept
        {
            compressor.process (left, right, numSamples);
        }

        /** The drive, the width, the limiter and the output level, in place. */
        void processEnd (float* left, float* right, int numSamples) noexcept
        {
            if (std::abs (drive - 1.0f) > 1.0e-6f)
            {
                juce::FloatVectorOperations::multiply (left, drive, numSamples);
                juce::FloatVectorOperations::multiply (right, drive, numSamples);
            }

            const auto width = juce::jlimit (0.0f, 2.0f, settings.width);

            if (std::abs (width - 1.0f) > 1.0e-4f)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const auto mid = 0.5f * (left[i] + right[i]);
                    const auto side = 0.5f * (left[i] - right[i]) * width;
                    left[i] = mid + side;
                    right[i] = mid - side;
                }
            }

            limiter.process (left, right, numSamples);

            if (outputGain < 0.9999f)
            {
                juce::FloatVectorOperations::multiply (left, outputGain, numSamples);
                juce::FloatVectorOperations::multiply (right, outputGain, numSamples);
            }
        }

        void process (float* left, float* right, int numSamples) noexcept
        {
            processEq (left, right, numSamples);
            processGlue (left, right, numSamples);
            processEnd (left, right, numSamples);
        }

        int getLatency() const noexcept   { return limiter.getLatency(); }

        /** The glue compressor's and the limiter's reduction now, in dB, for meters. */
        double getGlueReductionDb() const noexcept      { return compressor.getReductionDb(); }
        double getLimiterReductionDb() const noexcept   { return limiter.getReductionDb(); }

        Compressor compressor;
        Limiter limiter;

    private:
        double sampleRate = 48000.0;
        MasteringSettings settings;
        std::array<Biquad, bands> filters {};
        std::array<std::array<BiquadState, 2>, bands> states {};
        float inputGain = 1.0f, drive = 1.0f, outputGain = 1.0f;
    };
}
