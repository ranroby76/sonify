// D:\workspace\Fanan_web_hub\src\CaymanWavetables.h
//
// Cayman's wavetables, borrowed from the Plugins Creator project "My Plugin" (its modules/Wavetables.h).
//
// 16 single-cycle wavetables, band-limited into one table per octave (mipmaps),
// so every note plays without aliasing. Built once, outside the audio thread.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace caymanwt
{
    constexpr int tableSize = 2048;
    constexpr int numTables = 16;
    constexpr int numLevels = 11;     // level l keeps harmonics up to 1024 >> l

    inline const char* const names[numTables]
    {
        "Sine", "Triangle", "Saw", "Square", "Pulse 25", "Pulse 10", "Double Saw", "Organ",
        "Vox", "Glass", "Sync", "FM", "Fold", "Crush", "Metal", "Grit"
    };

    inline void fft (std::vector<std::complex<double>>& a, bool inverse)
    {
        const int n = (int) a.size();

        for (int i = 1, j = 0; i < n; ++i)
        {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap (a[(size_t) i], a[(size_t) j]);
        }

        for (int len = 2; len <= n; len <<= 1)
        {
            const double ang = 2.0 * 3.14159265358979323846 / len * (inverse ? 1.0 : -1.0);
            const std::complex<double> wl (std::cos (ang), std::sin (ang));

            for (int i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0, 0.0);

                for (int k = 0; k < len / 2; ++k)
                {
                    const auto u = a[(size_t) (i + k)];
                    const auto v = a[(size_t) (i + k + len / 2)] * w;
                    a[(size_t) (i + k)] = u + v;
                    a[(size_t) (i + k + len / 2)] = u - v;
                    w *= wl;
                }
            }
        }

        if (inverse)
            for (auto& x : a)
                x /= (double) n;
    }

    inline double gauss (double k, double centre, double width)
    {
        const double d = (k - centre) / width;
        return std::exp (-0.5 * d * d);
    }

    // One cycle of each shape, p in 0..1.
    inline double rawShape (int t, double p)
    {
        const double tp = 2.0 * 3.14159265358979323846;
        const double x = tp * p;
        const double s = std::sin (x);

        switch (t)
        {
            case 0:  return s;
            case 1:  return p < 0.25 ? 4.0 * p : (p < 0.75 ? 2.0 - 4.0 * p : 4.0 * p - 4.0);
            case 2:  return 1.0 - 2.0 * p;
            case 3:  return p < 0.5 ? 1.0 : -1.0;
            case 4:  return p < 0.25 ? 1.0 : -1.0;
            case 5:  return p < 0.1 ? 1.0 : -1.0;
            case 6:
            {
                const double p2 = std::fmod (p * 2.0 + 0.25, 1.0);
                return (1.0 - 2.0 * p) + 0.7 * (1.0 - 2.0 * p2);
            }
            case 7:
                return s + 0.8 * std::sin (2.0 * x) + 0.6 * std::sin (3.0 * x) + 0.5 * std::sin (4.0 * x)
                         + 0.3 * std::sin (6.0 * x) + 0.25 * std::sin (8.0 * x);
            case 8:
            {
                double v = 0.0;
                for (int k = 1; k <= 64; ++k)
                {
                    const double a = (gauss (k, 6.4, 2.0) + 0.6 * gauss (k, 11.0, 2.5) + 0.3 * gauss (k, 23.6, 4.0)) / std::sqrt ((double) k)
                                     + 0.3 / k;
                    v += a * std::sin (k * x);
                }
                return v;
            }
            case 9:
                return s + 0.6 * std::sin (2.0 * x) + 0.5 * std::sin (4.0 * x) + 0.45 * std::sin (8.0 * x)
                         + 0.4 * std::sin (16.0 * x) + 0.3 * std::sin (32.0 * x);
            case 10:
            {
                const double q = std::fmod (p * 3.3, 1.0);
                return (1.0 - 2.0 * q) * (1.0 - 0.3 * p);
            }
            case 11: return std::sin (x + 3.0 * std::sin (2.0 * x));
            case 12: return std::sin (5.0 * s);
            case 13: return std::floor (s * 3.5 + 0.5) / 3.5;
            case 14:
            {
                double v = 0.0;
                unsigned seed = 12345u;
                for (int k = 1; k <= 48; ++k)
                {
                    seed = seed * 1664525u + 1013904223u;
                    const double r = (double) (seed >> 8) / 16777216.0;
                    seed = seed * 1664525u + 1013904223u;
                    const double ph = tp * (double) (seed >> 8) / 16777216.0;
                    v += (r * r) / std::pow ((double) k, 0.6) * std::sin (k * x + ph);
                }
                return v;
            }
            default: return std::tanh (4.0 * ((1.0 - 2.0 * p) + 0.6 * std::sin (7.0 * x)));
        }
    }

    class Bank
    {
    public:
        void build()
        {
            if (! data.empty())
                return;

            data.assign ((size_t) numTables * numLevels * (tableSize + 1), 0.0f);
            std::vector<std::complex<double>> spectrum ((size_t) tableSize), work ((size_t) tableSize);

            for (int t = 0; t < numTables; ++t)
            {
                for (int i = 0; i < tableSize; ++i)
                    spectrum[(size_t) i] = { rawShape (t, (double) i / tableSize), 0.0 };

                fft (spectrum, false);
                spectrum[0] = 0.0;   // no DC
                double gain = 1.0;

                for (int l = 0; l < numLevels; ++l)
                {
                    const int limit = std::min (tableSize / 2 - 1, 1024 >> l);
                    std::fill (work.begin(), work.end(), std::complex<double>());

                    for (int k = 1; k <= limit; ++k)
                    {
                        work[(size_t) k] = spectrum[(size_t) k];
                        work[(size_t) (tableSize - k)] = spectrum[(size_t) (tableSize - k)];
                    }

                    fft (work, true);

                    if (l == 0)
                    {
                        double peak = 1e-9;
                        for (auto& v : work)
                            peak = std::max (peak, std::abs (v.real()));
                        gain = 0.9 / peak;
                    }

                    float* dst = table (t, l);
                    for (int i = 0; i < tableSize; ++i)
                        dst[i] = (float) (work[(size_t) i].real() * gain);
                    dst[tableSize] = dst[0];
                }
            }
        }

        // The richest level that stays clean at this pitch (folding only above ~18 kHz).
        static int levelFor (float hz, float sampleRate) noexcept
        {
            const float room = std::max (0.5f * sampleRate, sampleRate - 18000.0f);
            const float maxHarmonic = room / std::max (hz, 1.0f);
            int l = 0;
            while (l < numLevels - 1 && (float) (1024 >> l) > maxHarmonic)
                ++l;
            return l;
        }

        // phase in 0..1
        float read (int t, int level, float phase) const noexcept
        {
            const float pos = phase * (float) tableSize;
            int i = (int) pos;
            const float f = pos - (float) i;
            i &= tableSize - 1;
            const float* d = data.data() + ((size_t) t * numLevels + (size_t) level) * (tableSize + 1);
            return d[i] + f * (d[i + 1] - d[i]);
        }

    private:
        std::vector<float> data;

        float* table (int t, int l) noexcept
        {
            return data.data() + ((size_t) t * numLevels + (size_t) l) * (tableSize + 1);
        }
    };
}
