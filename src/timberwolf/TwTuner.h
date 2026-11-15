// D:\workspace\Fanan_web_hub\src\timberwolf\TwTuner.h
#pragma once
//==============================================================================
//  TwTuner.h  -  the tuner: the guitar's pitch, read from what comes in.
//
//  The audio thread hands it the input (feed), brought down to about 11 kHz
//  into a ring; the panel asks for the pitch (analyse) about 30 times a second,
//  on the message thread, which runs YIN (de Cheveigne & Kawahara, 2002) over
//  the last 2048 samples there: about 0.19 s, enough for a low B (31 Hz).
//
//  Threads: feed on the audio thread; analyse and the readings on the message
//  thread.  The ring is written by one and read by the other without locks: a
//  reading may straddle a write, which costs a slightly wrong reading at worst.
//==============================================================================
#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace tw
{
    class Tuner
    {
    public:
        static constexpr int window = 2048;

        void prepare (double sampleRate)
        {
            factor = juce::jmax (1, (int) std::round (sampleRate / 11025.0));
            rate = sampleRate / factor;
            ring.assign ((size_t) window * 2, 0.0f);
            write.store (0);
            acc = 0.0f;
            count = 0;
            lp1 = lp2 = 0.0f;
            const double fc = 2500.0 / sampleRate;
            lpCoeff = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * fc));
        }

        /** Audio thread: the input (mono). */
        void feed (const float* x, int n) noexcept
        {
            if (ring.empty())
                return;

            int w = write.load (std::memory_order_relaxed);
            const int size = (int) ring.size();

            for (int i = 0; i < n; ++i)
            {
                lp1 += lpCoeff * (x[i] - lp1);
                lp2 += lpCoeff * (lp1 - lp2);
                acc += lp2;

                if (++count >= factor)
                {
                    ring[(size_t) w] = acc / (float) factor;
                    w = (w + 1) % size;
                    acc = 0.0f;
                    count = 0;
                }
            }

            write.store (w, std::memory_order_release);
        }

        /** Message thread: the pitch now, in Hz (0: none heard). */
        float analyse()
        {
            if (ring.empty())
                return 0.0f;

            const int size = (int) ring.size();
            const int w = write.load (std::memory_order_acquire);

            frame.resize ((size_t) window);
            float peak = 0.0f;

            for (int i = 0; i < window; ++i)
            {
                frame[(size_t) i] = ring[(size_t) ((w - window + i + size) % size)];
                peak = juce::jmax (peak, std::abs (frame[(size_t) i]));
            }

            level = peak;

            if (peak < 0.004f)
            {
                hz = 0.0f;
                return hz;
            }

            const int half = window / 2;
            diff.assign ((size_t) half, 0.0f);

            for (int tau = 1; tau < half; ++tau)
            {
                float sum = 0.0f;

                for (int j = 0; j < half; ++j)
                {
                    const float d = frame[(size_t) j] - frame[(size_t) (j + tau)];
                    sum += d * d;
                }

                diff[(size_t) tau] = sum;
            }

            // Cumulative mean normalised difference, then the first dip under the threshold.
            diff[0] = 1.0f;
            float running = 0.0f;

            for (int tau = 1; tau < half; ++tau)
            {
                running += diff[(size_t) tau];
                diff[(size_t) tau] = running > 0.0f ? diff[(size_t) tau] * (float) tau / running : 1.0f;
            }

            const int minTau = juce::jmax (2, (int) (rate / 1400.0));
            int found = -1;

            for (int tau = minTau; tau < half - 1; ++tau)
            {
                if (diff[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 < half - 1 && diff[(size_t) tau + 1] < diff[(size_t) tau])
                        ++tau;

                    found = tau;
                    break;
                }
            }

            if (found < 0)
            {
                hz = 0.0f;
                return hz;
            }

            const float a = diff[(size_t) found - 1], b = diff[(size_t) found], c = diff[(size_t) found + 1];
            const float denom = a - 2.0f * b + c;
            const float exact = (float) found + (std::abs (denom) > 1.0e-9f ? 0.5f * (a - c) / denom : 0.0f);

            const float now = (float) (rate / exact);

            // A little smoothing while the note holds; a jump (a new string) is taken at once.
            hz = (hz > 0.0f && std::abs (12.0f * std::log2 (now / hz)) < 0.5f) ? hz + 0.35f * (now - hz) : now;
            return hz;
        }

        float lastHz() const noexcept   { return hz; }
        float lastLevel() const noexcept { return level; }

        /** A pitch as its note's name and how far off it is, in cents (A = a4 Hz). */
        static void note (float hz, float a4, juce::String& name, float& cents)
        {
            static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

            if (hz <= 0.0f)
            {
                name = "-";
                cents = 0.0f;
                return;
            }

            const float midi = 69.0f + 12.0f * std::log2 (hz / a4);
            const int nearest = (int) std::round (midi);
            cents = (midi - (float) nearest) * 100.0f;
            name = juce::String (names[(nearest % 12 + 12) % 12]) + juce::String (nearest / 12 - 1);
        }

    private:
        std::vector<float> ring, frame, diff;
        std::atomic<int> write { 0 };
        int factor = 4, count = 0;
        double rate = 12000.0;
        float acc = 0.0f, lp1 = 0.0f, lp2 = 0.0f, lpCoeff = 0.2f;
        float hz = 0.0f, level = 0.0f;
    };
}
