// D:\workspace\Fanan_web_hub\src\SoundMeasure.cpp

#include "SoundMeasure.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <numeric>

namespace SoundMeasure
{
namespace
{
    constexpr double pi = 3.14159265358979323846;

    double db (double amplitude) noexcept      { return 20.0 * std::log10 (std::max (amplitude, 1.0e-9)); }
    double dbPower (double power) noexcept     { return 10.0 * std::log10 (std::max (power, 1.0e-18)); }

    std::string text (const char* format, ...)
    {
        char buffer[1024];
        va_list args;
        va_start (args, format);
        std::vsnprintf (buffer, sizeof (buffer), format, args);
        va_end (args);
        return buffer;
    }

    /** Hz as people read it: 840 Hz, 3.2 kHz. */
    std::string hz (double f)
    {
        return f >= 1000.0 ? text ("%.1f kHz", f / 1000.0) : text ("%.0f Hz", f);
    }

    std::string ms (double seconds)
    {
        return seconds >= 1.0 ? text ("%.2f s", seconds) : text ("%.0f ms", seconds * 1000.0);
    }

    std::string noteName (int note)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return std::string (names[((note % 12) + 12) % 12]) + std::to_string (note / 12 - 1);
    }

    double noteHz (int note) noexcept    { return 440.0 * std::pow (2.0, (note - 69) / 12.0); }

    //==========================================================================
    void fft (std::vector<std::complex<double>>& a)
    {
        const auto n = a.size();

        for (std::size_t i = 1, j = 0; i < n; ++i)
        {
            auto bit = n >> 1;

            for (; (j & bit) != 0; bit >>= 1)
                j ^= bit;

            j ^= bit;

            if (i < j)
                std::swap (a[i], a[j]);
        }

        for (std::size_t length = 2; length <= n; length <<= 1)
        {
            const auto angle = -2.0 * pi / (double) length;

            for (std::size_t i = 0; i < n; i += length)
            {
                for (std::size_t k = 0; k < length / 2; ++k)
                {
                    const std::complex<double> w (std::cos (angle * (double) k), std::sin (angle * (double) k));
                    const auto u = a[i + k];
                    const auto v = a[i + k + length / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + length / 2] = u - v;
                }
            }
        }
    }

    /** An averaged power spectrum (Welch: Hann windows, half overlapping). */
    struct Spectrum
    {
        std::vector<double> power;   // bins 0 .. size / 2
        double binHz = 1.0;

        double bandPower (double fromHz, double toHz) const
        {
            double sum = 0.0;

            for (std::size_t i = 1; i < power.size(); ++i)
            {
                const auto f = (double) i * binHz;

                if (f >= fromHz && f < toHz)
                    sum += power[i];
            }

            return sum;
        }

        double centroid (double fromHz, double toHz) const
        {
            double weighted = 0.0, sum = 0.0;

            for (std::size_t i = 1; i < power.size(); ++i)
            {
                const auto f = (double) i * binHz;

                if (f >= fromHz && f < toHz)
                {
                    weighted += f * power[i];
                    sum += power[i];
                }
            }

            return sum > 0.0 ? weighted / sum : 0.0;
        }

        /** Where share of the energy above fromHz lies below. */
        double rolloff (double share, double fromHz) const
        {
            const auto total = bandPower (fromHz, 1.0e9);
            double sum = 0.0;

            for (std::size_t i = 1; i < power.size(); ++i)
            {
                const auto f = (double) i * binHz;

                if (f < fromHz)
                    continue;

                sum += power[i];

                if (sum >= share * total)
                    return f;
            }

            return (double) (power.size() - 1) * binHz;
        }

        /** 0 for pure tones .. 1 for white noise (spectral flatness), between two frequencies. */
        double flatness (double fromHz, double toHz) const
        {
            double logSum = 0.0, sum = 0.0;
            int count = 0;

            for (std::size_t i = 1; i < power.size(); ++i)
            {
                const auto f = (double) i * binHz;

                if (f >= fromHz && f < toHz)
                {
                    logSum += std::log (power[i] + 1.0e-20);
                    sum += power[i];
                    ++count;
                }
            }

            if (count == 0 || sum <= 0.0)
                return 0.0;

            return std::exp (logSum / count) / (sum / count);
        }

        /** The strongest bin's frequency near a frequency (within width either side). */
        double peakNear (double f, double width) const
        {
            double best = 0.0, at = f;

            for (std::size_t i = 1; i < power.size(); ++i)
            {
                const auto bin = (double) i * binHz;

                if (std::abs (bin - f) <= width && power[i] > best)
                {
                    best = power[i];
                    at = bin;
                }
            }

            return at;
        }
    };

    std::vector<double> hann (int size)
    {
        std::vector<double> w ((std::size_t) size);

        for (int i = 0; i < size; ++i)
            w[(std::size_t) i] = 0.5 - 0.5 * std::cos (2.0 * pi * i / (size - 1));

        return w;
    }

    /** Blackman-Harris: a wider peak but almost no leakage, to see what lies between harmonics. */
    std::vector<double> blackmanHarris (int size)
    {
        std::vector<double> w ((std::size_t) size);

        for (int i = 0; i < size; ++i)
        {
            const auto t = 2.0 * pi * i / (size - 1);
            w[(std::size_t) i] = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2.0 * t) - 0.01168 * std::cos (3.0 * t);
        }

        return w;
    }

    Spectrum welch (const std::vector<float>& x, std::size_t from, std::size_t to, int size, double rate, bool sharp = false)
    {
        Spectrum s;
        s.binHz = rate / size;
        s.power.assign ((std::size_t) size / 2 + 1, 0.0);
        to = std::min (to, x.size());

        if (to <= from)
            return s;

        // Whole windows only, so no frame ends in an edge; a range shorter than one window is
        // windowed over its own length, then padded.
        const auto available = to - from;
        const auto windowLength = (int) std::min<std::size_t> ((std::size_t) size, available);
        const auto window = sharp ? blackmanHarris (windowLength) : hann (windowLength);
        std::vector<std::complex<double>> buffer ((std::size_t) size);
        int frames = 0;

        for (auto position = from; position + (std::size_t) windowLength <= to; position += (std::size_t) windowLength / 2)
        {
            for (std::size_t i = 0; i < (std::size_t) size; ++i)
                buffer[i] = i < (std::size_t) windowLength ? (double) x[position + i] * window[i] : 0.0;

            fft (buffer);

            for (std::size_t i = 0; i < s.power.size(); ++i)
                s.power[i] += std::norm (buffer[i]);

            ++frames;

            if (windowLength < 2)
                break;
        }

        if (frames == 0)
            return s;

        for (auto& p : s.power)
            p /= frames;

        return s;
    }

    //==========================================================================
    struct Band
    {
        const char* name;
        double from, to;
    };

    const Band bands[]
    {
        { "sub 20-60 Hz", 20.0, 60.0 },
        { "bass 60-250", 60.0, 250.0 },
        { "low-mid 250-500", 250.0, 500.0 },
        { "mid 500-2k", 500.0, 2000.0 },
        { "high-mid 2-4k", 2000.0, 4000.0 },
        { "presence 4-8k", 4000.0, 8000.0 },
        { "air 8k+", 8000.0, 1.0e9 },
    };

    double rms (const std::vector<float>& x, std::size_t from, std::size_t to)
    {
        to = std::min (to, x.size());

        if (to <= from)
            return 0.0;

        double sum = 0.0;

        for (auto i = from; i < to; ++i)
            sum += (double) x[i] * x[i];

        return std::sqrt (sum / (double) (to - from));
    }

    double stereoRms (const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to)
    {
        const auto a = rms (l, from, to), b = rms (r, from, to);
        return std::sqrt (0.5 * (a * a + b * b));
    }

    std::vector<float> mono (const std::vector<float>& l, const std::vector<float>& r)
    {
        std::vector<float> m (l.size());

        for (std::size_t i = 0; i < l.size(); ++i)
            m[i] = 0.5f * (l[i] + (i < r.size() ? r[i] : l[i]));

        return m;
    }

    /** The loudest sample in each millisecond (both sides). */
    std::vector<float> envelope (const std::vector<float>& l, const std::vector<float>& r, double rate, int& perStep)
    {
        perStep = std::max (1, (int) std::lround (rate / 1000.0));
        std::vector<float> env ((l.size() + (std::size_t) perStep - 1) / (std::size_t) perStep, 0.0f);

        for (std::size_t i = 0; i < l.size(); ++i)
        {
            auto& e = env[i / (std::size_t) perStep];
            e = std::max (e, std::max (std::abs (l[i]), i < r.size() ? std::abs (r[i]) : 0.0f));
        }

        return env;
    }

    /** Per frame of 1024 samples, half overlapping: level (RMS) and brightness (centroid). */
    struct Frames
    {
        std::vector<double> level, centroid, flux;
        std::size_t hop = 512;
    };

    Frames frames (const std::vector<float>& x, std::size_t from, std::size_t to, double rate)
    {
        Frames f;
        constexpr int size = 1024;
        const auto window = hann (size);
        std::vector<std::complex<double>> buffer (size);
        std::vector<double> previous (size / 2 + 1, 0.0), magnitude (size / 2 + 1);
        to = std::min (to, x.size());

        for (auto position = from; position + size <= to; position += f.hop)
        {
            for (std::size_t i = 0; i < (std::size_t) size; ++i)
                buffer[i] = (double) x[position + i] * window[i];

            fft (buffer);

            double weighted = 0.0, sum = 0.0, rise = 0.0, total = 0.0;

            for (std::size_t i = 1; i < magnitude.size(); ++i)
            {
                magnitude[i] = std::abs (buffer[i]);
                const auto p = magnitude[i] * magnitude[i];
                weighted += (double) i * rate / size * p;
                sum += p;
                rise += std::max (0.0, magnitude[i] - previous[i]);
                total += magnitude[i];
            }

            f.level.push_back (rms (x, position, position + size));
            f.centroid.push_back (sum > 0.0 ? weighted / sum : 0.0);
            f.flux.push_back (total > 0.0 && ! f.level.empty() && f.level.size() > 1 ? rise / total : 0.0);
            previous = magnitude;
        }

        return f;
    }

    void meanAndDeviation (const std::vector<double>& v, std::size_t from, std::size_t to, double& mean, double& deviation)
    {
        mean = deviation = 0.0;
        to = std::min (to, v.size());

        if (to <= from)
            return;

        for (auto i = from; i < to; ++i)
            mean += v[i];

        mean /= (double) (to - from);

        for (auto i = from; i < to; ++i)
            deviation += (v[i] - mean) * (v[i] - mean);

        deviation = std::sqrt (deviation / (double) (to - from));
    }

    /** The fundamental (YIN), knowing which note was played: when the sound repeats at that
        note's period it's taken (a strong overtone or a sub doesn't fool it); otherwise the
        clearest period it finds. 0 if there isn't a clear one. clarity: 0..1. subOctave: the
        sound also repeats only every two periods (a sub-oscillator an octave down). */
    double pitchOf (const std::vector<float>& x, std::size_t from, std::size_t to, double rate, double expectedHz,
                    double& clarity, bool& subOctave)
    {
        clarity = 0.0;
        subOctave = false;
        to = std::min (to, x.size());
        const auto minLag = (std::size_t) (rate / 2500.0), maxLag = (std::size_t) (rate / 25.0);
        const auto window = std::min<std::size_t> ((std::size_t) (rate * 0.05), to > from ? to - from : 0);

        if (to <= from || window + maxLag + 2 > to - from)
            return 0.0;

        std::vector<double> d (maxLag + 2, 0.0);

        for (auto lag = (std::size_t) 1; lag <= maxLag + 1; ++lag)
        {
            double sum = 0.0;

            for (std::size_t i = 0; i < window; ++i)
            {
                const auto diff = (double) x[from + i] - (double) x[from + i + lag];
                sum += diff * diff;
            }

            d[lag] = sum;
        }

        // Cumulative mean normalised difference.
        std::vector<double> cmnd (d.size(), 1.0);
        double running = 0.0;

        for (std::size_t lag = 1; lag < d.size(); ++lag)
        {
            running += d[lag];
            cmnd[lag] = running > 0.0 ? d[lag] * (double) lag / running : 1.0;
        }

        auto deepestNear = [&cmnd, minLag, maxLag] (double centre, double width)
        {
            const auto lo = std::max<std::size_t> (std::max<std::size_t> (minLag, 2), (std::size_t) std::floor (centre * (1.0 - width)));
            const auto hi = std::min<std::size_t> (maxLag, (std::size_t) std::ceil (centre * (1.0 + width)));
            std::size_t best = 0;

            for (auto lag = lo; lag <= hi; ++lag)
                if (best == 0 || cmnd[lag] < cmnd[best])
                    best = lag;

            return best;
        };

        auto refine = [&cmnd, rate] (std::size_t best)
        {
            auto lag = (double) best;

            if (best > 1 && best + 1 < cmnd.size())
            {
                const auto a = cmnd[best - 1], b = cmnd[best], c = cmnd[best + 1];
                const auto denominator = a - 2.0 * b + c;

                if (std::abs (denominator) > 1.0e-12)
                    lag += 0.5 * (a - c) / denominator;
            }

            return rate / lag;
        };

        // The note's own period first (within about a semitone).
        const auto expectedLag = expectedHz > 0.0 ? rate / expectedHz : 0.0;

        if (expectedLag > (double) minLag && expectedLag < (double) maxLag)
        {
            const auto atNote = deepestNear (expectedLag, 0.06);

            if (atNote != 0 && cmnd[atNote] < 0.25)
            {
                clarity = std::clamp (1.0 - cmnd[atNote], 0.0, 1.0);

                if (expectedLag * 2.0 < (double) maxLag)
                    if (const auto twice = deepestNear (expectedLag * 2.0, 0.06); twice != 0 && cmnd[twice] < cmnd[atNote] * 0.4)
                        subOctave = true;

                return refine (atNote);
            }
        }

        // Otherwise the clearest period there is.
        std::size_t best = 0;

        for (auto lag = std::max<std::size_t> (minLag, 2); lag <= maxLag; ++lag)
        {
            if (cmnd[lag] < 0.2)
            {
                while (lag + 1 <= maxLag && cmnd[lag + 1] < cmnd[lag])
                    ++lag;

                best = lag;
                break;
            }
        }

        if (best == 0)
            return 0.0;

        clarity = std::clamp (1.0 - cmnd[best], 0.0, 1.0);
        return refine (best);
    }

    /** The energy between the harmonics against the energy on them (dB): noise, detune spread,
        inharmonic partials or aliasing. A clean oscillator at a high note sits below about -35 dB. */
    double betweenHarmonicsDb (const Spectrum& s, double f0)
    {
        if (f0 <= 0.0)
            return 0.0;

        double onHarmonics = 0.0, between = 0.0;
        const auto tolerance = std::max (4.5 * s.binHz, 0.01 * f0);

        for (std::size_t i = 1; i < s.power.size(); ++i)
        {
            const auto f = (double) i * s.binHz;

            if (f < f0 * 0.5)
                continue;

            const auto k = std::max (1.0, std::round (f / f0));

            if (std::abs (f - k * f0) <= tolerance)
                onHarmonics += s.power[i];
            else
                between += s.power[i];
        }

        return onHarmonics > 0.0 ? dbPower (between / onHarmonics) : 0.0;
    }

    /** The first millisecond from start where the envelope reaches a level. */
    double reachTime (const std::vector<float>& env, std::size_t from, std::size_t to, float level)
    {
        to = std::min (to, env.size());

        for (auto i = from; i < to; ++i)
            if (env[i] >= level)
                return (double) (i - from) / 1000.0;

        return -1.0;
    }

    /** How long after from the envelope stays below a level for good (20 ms on end); -1: it doesn't. */
    double fadeTime (const std::vector<float>& env, std::size_t from, float level)
    {
        if (from >= env.size())
            return -1.0;

        std::size_t quietSince = from;
        bool quiet = false;

        for (auto i = from; i < env.size(); ++i)
        {
            if (env[i] < level)
            {
                if (! quiet)
                {
                    quiet = true;
                    quietSince = i;
                }
            }
            else
            {
                quiet = false;
            }
        }

        // Quiet at the end, and for at least 20 ms.
        if (quiet && env.size() - quietSince >= 20)
            return (double) (quietSince - from) / 1000.0;

        return -1.0;
    }

    /** The edge of a sound in 2 ms pieces: how its high-frequency burst at index compares to the
        typical piece in [from, to). A ratio well above 1 is a click. */
    double burstRatio (const std::vector<float>& x, std::size_t at, std::size_t from, std::size_t to, double rate)
    {
        const auto piece = std::max<std::size_t> (8, (std::size_t) (rate * 0.002));
        to = std::min (to, x.size());

        if (to <= from + piece * 4 || at + piece >= x.size())
            return 0.0;

        auto energy = [&x] (std::size_t a, std::size_t b)
        {
            double sum = 0.0;

            for (auto i = std::max<std::size_t> (a, 2); i < b && i < x.size(); ++i)
            {
                const auto d = (double) x[i] - 2.0 * x[i - 1] + x[i - 2];   // the highs, strongly
                sum += d * d;
            }

            return sum;
        };

        std::vector<double> pieces;

        for (auto p = from; p + piece <= to; p += piece)
            pieces.push_back (energy (p, p + piece));

        if (pieces.empty())
            return 0.0;

        // Against the loud end of what the held sound does anyway (a saw's own edges aren't clicks).
        const auto index = std::min (pieces.size() - 1, (std::size_t) ((double) pieces.size() * 0.98));
        std::nth_element (pieces.begin(), pieces.begin() + (long) index, pieces.end());
        const auto typical = pieces[index];

        double edge = 0.0;

        for (auto p = at > piece * 2 ? at - piece * 2 : 0; p < at + piece * 3; p += piece / 2)
            edge = std::max (edge, energy (p, p + piece));

        return typical > 1.0e-14 ? edge / typical : (edge > 1.0e-10 ? 1000.0 : 0.0);
    }

    struct Stereo
    {
        double correlation = 1.0, sideDb = -120.0, monoLossDb = 0.0, balanceDb = 0.0;
    };

    Stereo stereoOf (const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to)
    {
        Stereo s;
        to = std::min ({ to, l.size(), r.size() });

        if (to <= from)
            return s;

        double ll = 0.0, rr = 0.0, lr = 0.0, mm = 0.0, ss = 0.0;

        for (auto i = from; i < to; ++i)
        {
            const double a = l[i], b = r[i];
            ll += a * a;
            rr += b * b;
            lr += a * b;
            mm += 0.25 * (a + b) * (a + b);
            ss += 0.25 * (a - b) * (a - b);
        }

        s.correlation = ll > 0.0 && rr > 0.0 ? lr / std::sqrt (ll * rr) : 1.0;
        s.sideDb = mm > 0.0 ? dbPower (ss / mm) : -120.0;
        s.monoLossDb = (ll + rr) > 0.0 ? dbPower (mm / (0.5 * (ll + rr))) : 0.0;
        s.balanceDb = rr > 0.0 && ll > 0.0 ? dbPower (ll / rr) : 0.0;
        return s;
    }

    std::string bandLine (const Spectrum& s)
    {
        const auto total = s.bandPower (20.0, 1.0e9);
        std::string line;

        for (const auto& b : bands)
        {
            const auto share = total > 0.0 ? 100.0 * s.bandPower (b.from, b.to) / total : 0.0;
            line += (line.empty() ? "" : ", ") + text ("%s %.0f%%", b.name, share);
        }

        return line;
    }

    /** Cleans a channel: what isn't a number becomes silence (counted). */
    std::vector<float> cleaned (const std::vector<float>& x, int& badSamples)
    {
        std::vector<float> c (x);

        for (auto& v : c)
        {
            if (! std::isfinite (v))
            {
                v = 0.0f;
                ++badSamples;
            }
        }

        return c;
    }

    //==========================================================================
    // Test signals.
    struct Noise
    {
        std::uint32_t state = 0x12345678u;

        float white() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) ((double) state / 4294967295.0 * 2.0 - 1.0);
        }
    };

    struct Pink
    {
        Noise noise;
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;

        float next() noexcept
        {
            const auto w = noise.white();
            b0 = 0.99886f * b0 + w * 0.0555179f;
            b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f;
            b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f;
            b5 = -0.7616f * b5 - w * 0.0168980f;
            const auto out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
            b6 = w * 0.115926f;
            return out * 0.11f;
        }
    };

    /** A band-limited saw (PolyBLEP). */
    struct Saw
    {
        double phase = 0.0, step = 0.0;

        float next() noexcept
        {
            phase += step;

            if (phase >= 1.0)
                phase -= 1.0;

            auto v = 2.0 * phase - 1.0;

            if (phase < step)
            {
                const auto t = phase / step;
                v -= t + t - t * t - 1.0;
            }
            else if (phase > 1.0 - step)
            {
                const auto t = (phase - 1.0) / step;
                v -= t * t + t + t + 1.0;
            }

            return (float) v;
        }
    };

    void addDrums (std::vector<float>& l, std::vector<float>& r, double rate, std::size_t length, float gain)
    {
        Noise noise;
        float last = 0.0f;
        const auto beat = rate * 0.5;   // 120 BPM

        for (std::size_t i = 0; i < length; ++i)
        {
            const auto t = std::fmod ((double) i, beat * 4.0);   // a bar
            const auto inBeat = std::fmod (t, beat);
            const auto beatIndex = (int) (t / beat);
            const auto inEighth = std::fmod (t, beat * 0.5);
            double v = 0.0;

            // Kick on 1 and 3: a falling sine.
            if (beatIndex == 0 || beatIndex == 2)
            {
                const auto s = inBeat / rate;
                v += 0.9 * std::sin (2.0 * pi * (50.0 * s + 110.0 * (1.0 - std::exp (-s * 30.0)) / 30.0)) * std::exp (-s * 7.0);
            }

            // Snare on 2 and 4: a tone and noise.
            if (beatIndex == 1 || beatIndex == 3)
            {
                const auto s = inBeat / rate;
                v += 0.35 * std::sin (2.0 * pi * 190.0 * s) * std::exp (-s * 25.0) + 0.45 * noise.white() * std::exp (-s * 18.0);
            }

            // Hats on every eighth: short bright noise.
            {
                const auto s = inEighth / rate;
                const auto w = noise.white();
                const auto bright = w - last;   // a crude high-pass
                last = w;
                v += 0.18 * bright * std::exp (-s * 90.0);
            }

            l[i] += (float) v * gain;
            r[i] += (float) v * gain;
        }
    }

    void addChord (std::vector<float>& l, std::vector<float>& r, double rate, std::size_t length, float gain)
    {
        const int notes[] { 48, 52, 55, 59 };   // C major 7
        Saw left[4], right[4];

        for (int n = 0; n < 4; ++n)
        {
            left[n].step = right[n].step = noteHz (notes[n]) / rate;   // mono: what an effect adds to the width shows
            left[n].phase = right[n].phase = 0.37 * n;
        }

        double lpL = 0.0, lpR = 0.0;
        const auto coeff = 1.0 - std::exp (-2.0 * pi * 2500.0 / rate);

        for (std::size_t i = 0; i < length; ++i)
        {
            double a = 0.0, b = 0.0;

            for (int n = 0; n < 4; ++n)
            {
                a += left[n].next();
                b += right[n].next();
            }

            lpL += (a - lpL) * coeff;
            lpR += (b - lpR) * coeff;
            const auto fade = std::min (1.0, (double) i / (rate * 0.01)) * std::min (1.0, (double) (length - i) / (rate * 0.01));
            l[i] += (float) (lpL * 0.25 * fade) * gain;
            r[i] += (float) (lpR * 0.25 * fade) * gain;
        }
    }

    void addBass (std::vector<float>& l, std::vector<float>& r, double rate, std::size_t length, float gain)
    {
        Saw saw;
        saw.step = noteHz (36) / rate;
        double lp = 0.0;
        const auto coeff = 1.0 - std::exp (-2.0 * pi * 500.0 / rate);
        const auto eighth = rate * 0.25;

        for (std::size_t i = 0; i < length; ++i)
        {
            const auto s = std::fmod ((double) i, eighth) / rate;
            lp += (saw.next() - lp) * coeff;
            const auto v = lp * std::min (1.0, s * 400.0) * std::exp (-s * 6.0) * 0.7;
            l[i] += (float) v * gain;
            r[i] += (float) v * gain;
        }
    }

    void normalise (std::vector<float>& l, std::vector<float>& r, std::size_t length, float peak)
    {
        float highest = 0.0f;

        for (std::size_t i = 0; i < length; ++i)
            highest = std::max ({ highest, std::abs (l[i]), std::abs (r[i]) });

        if (highest <= 0.0f)
            return;

        const auto gain = peak / highest;

        for (std::size_t i = 0; i < length; ++i)
        {
            l[i] *= gain;
            r[i] *= gain;
        }
    }
}

//==============================================================================
const std::vector<std::string>& signalNames()
{
    static const std::vector<std::string> names { "music", "drums", "chord", "noise", "impulse", "sine" };
    return names;
}

bool makeSignal (const std::string& name, double rate, double seconds, std::vector<float>& l, std::vector<float>& r)
{
    const auto length = (std::size_t) std::max (0.0, seconds * rate);

    if (std::find (signalNames().begin(), signalNames().end(), name) == signalNames().end())
        return false;

    l.assign (std::max (l.size(), length), 0.0f);
    r.assign (std::max (r.size(), length), 0.0f);

    if (name == "music")
    {
        addDrums (l, r, rate, length, 1.0f);
        addBass (l, r, rate, length, 0.8f);
        addChord (l, r, rate, length, 0.6f);
        normalise (l, r, length, 0.5f);          // -6 dBFS peaks
    }
    else if (name == "drums")
    {
        addDrums (l, r, rate, length, 1.0f);
        normalise (l, r, length, 0.5f);
    }
    else if (name == "chord")
    {
        addChord (l, r, rate, length, 1.0f);
        normalise (l, r, length, 0.4f);
    }
    else if (name == "noise")
    {
        Pink left, right;
        right.noise.state = 0x9e3779b9u;

        for (std::size_t i = 0; i < length; ++i)
        {
            l[i] = left.next() * 0.6f;
            r[i] = right.next() * 0.6f;
        }
    }
    else if (name == "impulse")
    {
        if (length > 0)
            l[0] = r[0] = 0.5f;
    }
    else if (name == "sine")
    {
        for (std::size_t i = 0; i < length; ++i)
        {
            const auto fade = std::min (1.0, (double) i / (rate * 0.005)) * std::min (1.0, (double) (length - i) / (rate * 0.005));
            l[i] = r[i] = (float) (0.25 * fade * std::sin (2.0 * pi * 997.0 * (double) i / rate));
        }
    }

    return true;
}

//==============================================================================
std::string describe (const Take& take, const Take* soft)
{
    std::vector<std::string> lines, problems;
    const auto rate = take.sampleRate > 0.0 ? take.sampleRate : 48000.0;

    int bad = 0;
    const auto l = cleaned (take.left, bad);
    const auto r = cleaned (take.right.empty() ? take.left : take.right, bad);
    const auto length = std::min (l.size(), r.size());

    if (length < (std::size_t) (rate * 0.05))
        return "Nothing to measure: the render was too short.";

    const bool effect = ! take.inLeft.empty();
    const auto startAt = std::min (length, (std::size_t) (take.start * rate));
    const auto releaseAt = std::clamp ((std::size_t) (take.release * rate), startAt, length);
    const auto m = mono (l, r);

    if (bad > 0)
        problems.push_back (text ("It output %d samples that aren't numbers (NaN or infinity): a filter or feedback blew up. Fix it first: "
                                  "it's silenced in a song.", bad));

    //==========================================================================
    // Level.
    float peak = 0.0f;
    int over = 0;

    for (std::size_t i = 0; i < length; ++i)
    {
        const auto a = std::max (std::abs (l[i]), std::abs (r[i]));
        peak = std::max (peak, a);

        if (a > 1.0f)
            ++over;
    }

    const auto playing = stereoRms (l, r, startAt, std::max (releaseAt, startAt + (std::size_t) (rate * 0.05)));

    if (peak < 1.0e-4f)
    {
        lines.push_back ("Level: silent (no sound above -80 dBFS).");
        problems.push_back (effect ? "No sound comes out: check that the input reaches the output (plugin.in to plugin.out)."
                                   : "No sound comes out: check that notes reach the voice and the voice reaches plugin.out L and R.");

        std::string report;

        for (const auto& line : lines)
            report += line + "\n";

        report += "\nWorth fixing:\n";

        for (const auto& p : problems)
            report += "- " + p + "\n";

        return report;
    }

    lines.push_back (text ("Level: peak %.1f dBFS, loudness while playing (RMS) %.1f dBFS, crest %.1f dB.",
                           db (peak), db (playing), db (peak) - db (playing)));

    if (over > 0)
        problems.push_back (text ("It goes over 0 dBFS (%d samples, peak %+.1f dBFS): it will clip in a song. Lower the output, or add a soft clip at the end.",
                                  over, db (peak)));
    else if (peak > 0.944f)
        problems.push_back (text ("Peaks at %.1f dBFS: no headroom. Aim for about -6 dBFS.", db (peak)));

    if (playing < 0.05 && peak < 0.25f && ! effect)   // a pluck's RMS is low by nature: its peaks say whether it's quiet
        problems.push_back (text ("Quiet: %.1f dBFS while playing. Bring it up to about -18 to -12 dBFS.", db (playing)));

    {
        double sumL = 0.0, sumR = 0.0;

        for (std::size_t i = 0; i < length; ++i)
        {
            sumL += l[i];
            sumR += r[i];
        }

        const auto dc = std::max (std::abs (sumL), std::abs (sumR)) / (double) length;

        if (dc > 0.005)
            problems.push_back (text ("DC offset of %.3f: the output sits off zero. Add a DC blocker (a 10-20 Hz high-pass) at the end.", dc));
    }

    //==========================================================================
    // Tone.
    const auto measureTo = std::max (releaseAt, startAt + (std::size_t) (rate * 0.2));
    const auto spectrum = welch (m, startAt, measureTo, 4096, rate);
    const auto nyquist = rate * 0.5;

    lines.push_back ("Tone (share of the energy): " + bandLine (spectrum) + ".");
    lines.push_back (text ("Brightness: centre of the energy %s, 85%% of it below %s; noisiness %.2f (0 pure tones .. 1 white noise).",
                           hz (spectrum.centroid (20.0, nyquist)).c_str(), hz (spectrum.rolloff (0.85, 20.0)).c_str(),
                           spectrum.flatness (100.0, std::min (10000.0, nyquist))));

    int edgePerStep = 1;
    const auto env = envelope (l, r, rate, edgePerStep);
    const auto envStart = startAt / (std::size_t) edgePerStep;
    const auto envRelease = releaseAt / (std::size_t) edgePerStep;

    //==========================================================================
    // Instrument: the envelope, movement, pitch and clicks.
    double attackSeconds = 0.0;

    if (! effect)
    {
        float highest = 0.0f;

        for (auto i = envStart; i < std::min (envRelease + 50, env.size()); ++i)
            highest = std::max (highest, env[i]);

        attackSeconds = std::max (0.0, reachTime (env, envStart, env.size(), highest * 0.7f));   // within 3 dB: beating doesn't stretch it

        const auto heldFrom = releaseAt > (std::size_t) (rate * 0.1) ? std::max (startAt, releaseAt - (std::size_t) (rate * 0.1)) : startAt;
        const auto held = rms (m, heldFrom, releaseAt);

        double loudestFrame = 0.0;
        const auto f = frames (m, startAt, measureTo, rate);

        for (const auto v : f.level)
            loudestFrame = std::max (loudestFrame, v);

        float atRelease = 0.0f;

        for (auto i = envRelease > 5 ? envRelease - 5 : 0; i < std::min (envRelease, env.size()); ++i)
            atRelease = std::max (atRelease, env[i]);

        const auto floor = std::max (atRelease * 0.01f, 3.2e-4f);   // 40 dB down, or -70 dBFS
        const auto fade = fadeTime (env, envRelease, floor);

        std::string envelopeLine = text ("Envelope: reaches full level (within 3 dB) %s after the note starts; held level %.1f dB under its loudest moment",
                                         ms (attackSeconds).c_str(), db (held) - db (loudestFrame));

        if (fade >= 0.0)
            envelopeLine += text ("; fades out %s after the note ends.", ms (fade).c_str());
        else
            envelopeLine += text ("; still sounding %s after the note ends (end of the test).", ms ((double) (length - releaseAt) / rate).c_str());

        lines.push_back (envelopeLine);

        // The level over time, in eight pieces: five while held, three after.
        {
            std::string curve;

            for (int i = 0; i < 5; ++i)
            {
                const auto a = startAt + (releaseAt - startAt) * (std::size_t) i / 5;
                const auto b = startAt + (releaseAt - startAt) * (std::size_t) (i + 1) / 5;
                curve += text ("%s%.0f", curve.empty() ? "" : " ", std::max (-90.0, db (stereoRms (l, r, a, b))));
            }

            curve += " | note off |";

            for (int i = 0; i < 3; ++i)
            {
                const auto a = releaseAt + (length - releaseAt) * (std::size_t) i / 3;
                const auto b = releaseAt + (length - releaseAt) * (std::size_t) (i + 1) / 3;
                curve += text (" %.0f", std::max (-90.0, db (stereoRms (l, r, a, b))));
            }

            lines.push_back ("Level over time (dBFS, -90 is silence): " + curve + ".");
        }

        const auto end = rms (m, length > (std::size_t) (rate * 0.1) ? length - (std::size_t) (rate * 0.1) : 0, length);

        if (fade < 0.0 && db (end) > -60.0)
            problems.push_back (text ("It's still sounding at the end of the test (%.0f dBFS): right for something that rings on (a bell, "
                                      "a long pluck, a pad with a long release: test again with a longer tail), wrong if a voice never stops "
                                      "or noise is left running.", db (end)));

        // Movement while held.
        const auto settle = std::max (0.05, attackSeconds);
        const auto movingFrom = startAt + (std::size_t) (settle * rate);

        if (releaseAt > movingFrom + (std::size_t) (rate * 0.3))
        {
            const auto first = (movingFrom - startAt) / f.hop, last = (releaseAt - startAt) / f.hop;
            double centroidMean, centroidDeviation, levelMean, levelDeviation, fluxMean, fluxDeviation;
            std::vector<double> levelsDb;

            for (const auto v : f.level)
                levelsDb.push_back (db (v));

            meanAndDeviation (f.centroid, first, last, centroidMean, centroidDeviation);
            meanAndDeviation (levelsDb, first, last, levelMean, levelDeviation);
            meanAndDeviation (f.flux, first, last, fluxMean, fluxDeviation);

            double lowest = 1.0e9, highestCentroid = 0.0;

            for (auto i = first; i < std::min (last, f.centroid.size()); ++i)
            {
                lowest = std::min (lowest, f.centroid[i]);
                highestCentroid = std::max (highestCentroid, f.centroid[i]);
            }

            const auto variation = centroidMean > 0.0 ? 100.0 * centroidDeviation / centroidMean : 0.0;
            lines.push_back (text ("Movement while held: brightness varies %.0f%% (%s to %s), level varies %.1f dB, change %.2f "
                                   "(0 frozen .. 1 constantly new).",
                                   variation, hz (lowest).c_str(), hz (highestCentroid).c_str(), levelDeviation, fluxMean));

            if (variation < 2.5 && levelDeviation < 0.4 && (releaseAt - startAt) > (std::size_t) (rate * 0.8))
                lines.push_back ("(It's static while held: nothing moves. Fine for some sounds; pads, leads and keys usually "
                                 "come alive with slow modulation, drift or an evolving filter.)");
        }

        // Pitch: one note.
        if (take.notes.size() == 1 && take.pitched)
        {
            const auto from = startAt + (std::size_t) (std::max (0.08, attackSeconds) * rate);
            const auto to = std::min (releaseAt, from + (std::size_t) (rate * 0.5));
            double clarity = 0.0;
            bool subOctave = false;
            const auto expected = noteHz (take.notes.front());
            const auto f0 = pitchOf (m, from, to, rate, expected, clarity, subOctave);

            if (f0 > 0.0 && clarity > 0.8)
            {
                auto cents = 1200.0 * std::log2 (f0 / expected);
                std::string octave;

                const auto pitched = welch (m, from, to, 8192, rate, true);
                auto soundsAt = f0;

                // The partial nearest the note, measured finely: inharmonic sounds (bells, bars,
                // drums) are tuned by it, since their overtones mislead a period search.
                double partial = 0.0;
                {
                    const auto fine = welch (m, from, std::min (releaseAt, from + (std::size_t) (rate * 2.0)), 131072, rate);   // long and narrow
                    const auto total = fine.bandPower (20.0, nyquist);
                    std::size_t best = 0;

                    for (std::size_t i = 2; i + 1 < fine.power.size(); ++i)
                    {
                        const auto bin = (double) i * fine.binHz;

                        if (bin > expected * 0.94 && bin < expected * 1.06 && (best == 0 || fine.power[i] > fine.power[best]))
                            best = i;
                    }

                    if (best > 1 && fine.bandPower (((double) best - 3.0) * fine.binHz, ((double) best + 3.0) * fine.binHz) > 0.02 * total)
                    {
                        const auto a = std::log (fine.power[best - 1] + 1.0e-30), b = std::log (fine.power[best] + 1.0e-30),
                                   c = std::log (fine.power[best + 1] + 1.0e-30);
                        const auto denominator = a - 2.0 * b + c;
                        partial = ((double) best + (std::abs (denominator) > 1.0e-12 ? 0.5 * (a - c) / denominator : 0.0)) * fine.binHz;
                    }
                }

                std::string partialNote;

                // How far the period search and the partial disagree, octaves aside (a sub or an
                // overtone isn't a disagreement).
                auto apart = partial > 0.0 ? 1200.0 * std::log2 (f0 / partial) : 0.0;
                apart -= 1200.0 * std::round (apart / 1200.0);

                if (partial > 0.0 && (betweenHarmonicsDb (pitched, f0) > -30.0 || std::abs (apart) > 15.0))
                {
                    soundsAt = partial;
                    cents = 1200.0 * std::log2 (soundsAt / expected);
                    partialNote = " (its partial nearest the note: the sound is inharmonic)";
                }

                if (! partialNote.empty())                  { /* tuned by its partial */ }
                else if (std::abs (cents - 1200.0) < 60.0)  { octave = " (an octave up)"; cents -= 1200.0; }
                else if (std::abs (cents - 1902.0) < 60.0)  { octave = " (an octave and a fifth up)"; cents -= 1902.0; }
                else if (std::abs (cents + 1200.0) < 60.0)
                {
                    // Repeating every two periods: a sub-oscillator under the note, or the whole sound an
                    // octave low. Only a note that's (nearly) missing is a wrong octave for sure.
                    const auto width = 3.0 * pitched.binHz;
                    const auto atNote = pitched.bandPower (2.0 * f0 - width, 2.0 * f0 + width);
                    const auto atSub = pitched.bandPower (f0 - width, f0 + width);
                    subOctave = true;
                    soundsAt = 2.0 * f0;

                    if (atNote < 0.01 * atSub)
                        octave = " (an octave down)";

                    cents += 1200.0;
                }

                lines.push_back (text ("Pitch: %s should be %s; it sounds at %s%s, %+.0f cents%s%s, clarity %.2f. Between its harmonics: "
                                       "%.0f dB under them (noise, detune spread, inharmonic partials or aliasing: on a high note an aliasing "
                                       "oscillator measures about -15 dB, PolyBLEP about -30, cleaner designs -40 or lower).",
                                       noteName (take.notes.front()).c_str(), hz (expected).c_str(), hz (soundsAt).c_str(), partialNote.c_str(), cents, octave.c_str(),
                                       subOctave && octave.empty() ? ", and it also repeats an octave below (a sub-oscillator, or the whole sound an octave low)" : "", clarity, betweenHarmonicsDb (pitched, subOctave ? soundsAt * 0.5 : soundsAt)));

                if (std::abs (cents) > 15.0 && octave.empty())
                    problems.push_back (text ("Out of tune: %+.0f cents from %s.", cents, noteName (take.notes.front()).c_str()));

                if (! octave.empty())
                    problems.push_back ("It plays in the wrong octave" + octave + ": it doesn't repeat at the note's own pitch.");
            }
            else
            {
                lines.push_back ("Pitch: no clear pitch (noisy, percussive or inharmonic).");
            }
        }

        // Clicks: a burst of highs where the note ends (a voice cut off).
        {
            const auto heldStart = startAt + (std::size_t) (rate * std::max (0.02, attackSeconds));

            if (releaseAt > heldStart + (std::size_t) (rate * 0.05))
            {
                if (burstRatio (m, releaseAt, heldStart, releaseAt, rate) > 4.0)
                    problems.push_back ("A click when the note ends: its level drops too suddenly. Give the release at least a few "
                                        "milliseconds and stop voices only once they're silent.");

            }
        }

        if (soft != nullptr && ! soft->left.empty())
        {
            int ignored = 0;
            const auto sl = cleaned (soft->left, ignored);
            const auto sr = cleaned (soft->right.empty() ? soft->left : soft->right, ignored);
            const auto softStart = std::min (sl.size(), (std::size_t) (soft->start * rate));
            const auto softRelease = std::clamp ((std::size_t) (soft->release * rate), softStart, sl.size());
            const auto softLevel = stereoRms (sl, sr, softStart, softRelease);
            const auto softSpectrum = welch (mono (sl, sr), softStart, std::max (softRelease, softStart + (std::size_t) (rate * 0.2)), 4096, rate);
            const auto loudDb = db (playing), quietDb = db (softLevel);
            const auto bright = spectrum.centroid (20.0, nyquist), softBright = softSpectrum.centroid (20.0, nyquist);
            const auto darker = bright > 0.0 ? 100.0 * (bright - softBright) / bright : 0.0;

            lines.push_back (text ("Played softly (velocity %.0f%%): %.1f dB quieter and %.0f%% %s.", soft->velocity * 100.0f,
                                   loudDb - quietDb, std::abs (darker), darker >= 0.0 ? "darker" : "brighter"));

            if (loudDb - quietDb < 1.5 && std::abs (darker) < 3.0)
                problems.push_back ("It sounds the same however hard a key is played: let velocity shape its level (and its "
                                    "brightness, for expressive sounds).");
        }
    }

    //==========================================================================
    // Effect: what it did to the test signal.
    if (effect)
    {
        const auto il = take.inLeft;
        const auto ir = take.inRight.empty() ? take.inLeft : take.inRight;
        const auto inLevel = stereoRms (il, ir, startAt, releaseAt);
        const auto outLevel = stereoRms (l, r, startAt, releaseAt);

        if (take.signal != "impulse")
            lines.push_back (text ("Level change: %+.1f dB (out against in, while the signal plays).", db (outLevel) - db (inLevel)));

        if (take.signal != "impulse")
        {
            const auto inMono = mono (il, ir);
            const bool sine = take.signal == "sine";
            const auto inSpectrum = welch (inMono, startAt, measureTo, 4096, rate);
            std::string change;

            const auto inTotal = inSpectrum.bandPower (20.0, 1.0e9);

            for (const auto& b : bands)
            {
                const auto before = inSpectrum.bandPower (b.from, b.to), after = spectrum.bandPower (b.from, b.to);

                if (before > inTotal * 1.0e-5)   // only where the test signal has something
                    change += (change.empty() ? "" : ", ") + text ("%s %+.1f", b.name, dbPower (after) - dbPower (before));
            }

            if (! sine && ! change.empty())
                lines.push_back ("What it did to the tone (dB, out against in): " + change + ".");
            lines.push_back (text ("Brightness: %s in, %s out.", hz (inSpectrum.centroid (20.0, nyquist)).c_str(),
                                   hz (spectrum.centroid (20.0, nyquist)).c_str()));

            const auto inStereo = stereoOf (il, ir, startAt, releaseAt), outStereo = stereoOf (l, r, startAt, releaseAt);
            lines.push_back (text ("Width: the sides %.1f dB under the middle in, %.1f dB out.", std::max (-90.0, inStereo.sideDb),
                                   std::max (-90.0, outStereo.sideDb)));

            // Movement it adds (chorus, phaser, tremolo...).
            const auto inFrames = frames (inMono, startAt, releaseAt, rate), outFrames = frames (m, startAt, releaseAt, rate);
            double inMean, inDev, outMean, outDev;
            meanAndDeviation (inFrames.centroid, 0, inFrames.centroid.size(), inMean, inDev);
            meanAndDeviation (outFrames.centroid, 0, outFrames.centroid.size(), outMean, outDev);

            if (take.signal == "chord" || take.signal == "noise")
                lines.push_back (text ("Movement: brightness varies %.0f%% in, %.0f%% out.", inMean > 0 ? 100.0 * inDev / inMean : 0.0,
                                       outMean > 0 ? 100.0 * outDev / outMean : 0.0));

            // Its delay: where the output lines up best with the input.
            if (take.signal == "drums" || take.signal == "music")
            {
                const auto window = std::min<std::size_t> ((std::size_t) (rate * 0.25), releaseAt > startAt ? releaseAt - startAt : 0);
                const auto maxLag = (std::size_t) (rate * 0.05);
                double best = 0.0;
                std::size_t bestLag = 0;

                for (std::size_t lag = 0; lag <= maxLag && startAt + window + lag < length; lag += 2)
                {
                    double sum = 0.0;

                    for (std::size_t i = 0; i < window; i += 2)
                        sum += (double) inMono[startAt + i] * m[startAt + i + lag];

                    if (sum > best)
                    {
                        best = sum;
                        bestLag = lag;
                    }
                }

                if (bestLag > (std::size_t) (rate * 0.0015))
                    lines.push_back (text ("It delays the dry sound by about %s.", ms ((double) bestLag / rate).c_str()));
            }
        }

        // What's left once the input stops: the tail.
        {
            const auto tail = fadeTime (env, envRelease, 1.0e-3f);

            if (take.signal == "impulse")
            {
                // Times from when the impulse went in.
                float highest = 0.0f;
                std::size_t at = envStart;

                for (auto i = envStart; i < env.size(); ++i)
                    if (env[i] > highest) { highest = env[i]; at = i; }

                const auto first = reachTime (env, envStart, env.size(), highest * 0.1f);

                // The decay time (RT60) from the tail's slope: its level in 10 ms pieces, from 5 dB
                // under its loudest to 25 dB under, times three.
                std::vector<double> pieces;

                for (std::size_t p = 0; p + 10 <= env.size(); p += 10)
                {
                    double sum = 0.0;

                    for (std::size_t q = p * (std::size_t) edgePerStep; q < (p + 10) * (std::size_t) edgePerStep && q < length; ++q)
                        sum += 0.5 * ((double) l[q] * l[q] + (double) r[q] * r[q]);

                    pieces.push_back (dbPower (sum / (10.0 * edgePerStep)));
                }

                // The tail: past the direct sound (30 ms after the loudest moment), from its own peak.
                std::size_t loudestAt = 0;

                for (std::size_t p = 0; p < pieces.size(); ++p)
                    if (pieces[p] > pieces[loudestAt]) loudestAt = p;

                std::size_t tailAt = std::min (pieces.size(), loudestAt + 3);

                for (auto p = tailAt; p < pieces.size(); ++p)
                    if (pieces[p] > pieces[tailAt]) tailAt = p;

                double from5 = -1.0, to25 = -1.0;

                if (tailAt < pieces.size() && pieces[tailAt] > -150.0)
                {
                    const auto loudest = pieces[tailAt];

                    for (auto p = tailAt; p < pieces.size(); ++p)
                    {
                        if (from5 < 0.0 && pieces[p] <= loudest - 5.0)   from5 = (double) p * 0.01;
                        if (to25 < 0.0 && pieces[p] <= loudest - 25.0)  { to25 = (double) p * 0.01; break; }
                    }
                }

                const auto rt60 = from5 >= 0.0 && to25 > from5 ? 3.0 * (to25 - from5) : -1.0;
                lines.push_back (text ("Impulse: first sound after %s, loudest after %s; %s.", ms (std::max (0.0, first)).c_str(),
                                       ms ((double) (at - envStart) / 1000.0).c_str(),
                                       rt60 > 0.0 ? ("its tail decays 60 dB in about " + ms (rt60) + " (RT60)").c_str()
                                                  : "its tail rings past the end of the test"));
            }
            else if (tail >= 0.0)
            {
                lines.push_back (tail < 0.02 ? std::string ("Tail: it stops as soon as the input stops.")
                                             : "Tail: it rings " + ms (tail) + " after the input stops.");
            }
            else
            {
                lines.push_back ("Tail: still sounding at the end of the test (" + ms ((double) (length - releaseAt) / rate) + " after the input stopped).");
                const auto end = rms (m, length > (std::size_t) (rate * 0.1) ? length - (std::size_t) (rate * 0.1) : 0, length);

                if (db (end) > -45.0 && take.signal != "impulse")
                    problems.push_back (text ("It keeps sounding after the input stops (%.0f dBFS at the end): feedback that never dies "
                                              "away, or noise left running.", db (end)));
            }
        }

        // A sine in: what distortion it adds.
        if (take.signal == "sine")
        {
            const auto s = welch (m, startAt + (std::size_t) (rate * 0.05), releaseAt > (std::size_t) (rate * 0.03) ? releaseAt - (std::size_t) (rate * 0.03) : releaseAt, 8192, rate, true);
            const auto fundamental = s.peakNear (997.0, 30.0);
            const auto around = [&s] (double f) { return s.bandPower (f - 4.5 * s.binHz, f + 4.5 * s.binHz); };
            const auto base = around (fundamental);
            std::string harmonics;
            double sum = 0.0, even = 0.0, odd = 0.0;

            for (int k = 2; k * fundamental < nyquist - 3.0 * s.binHz; ++k)
            {
                const auto p = around (k * fundamental);
                sum += p;
                (k % 2 == 0 ? even : odd) += p;

                if (k <= 5)
                    harmonics += (harmonics.empty() ? "" : ", ") + text ("%d: %.0f dB", k, dbPower (p) - dbPower (base));
            }

            const auto harmonic = base + sum, total = s.bandPower (20.0, nyquist);
            const auto other = std::max (0.0, total - harmonic);
            const auto otherDb = dbPower (other) - dbPower (base);
            const auto thd = 100.0 * std::sqrt (sum / std::max (base, 1.0e-18));

            if (thd < 0.05)
                lines.push_back ("Distortion of a sine (997 Hz): none to speak of (clean).");
            else
                lines.push_back (text ("Distortion of a sine (997 Hz): harmonics %s (against the sine); total %.2f%%; %s.", harmonics.c_str(), thd,
                                       even > odd * 1.5 ? "mostly even harmonics (warm, tube-like)"
                                                        : (odd > even * 1.5 ? "mostly odd harmonics (edgier, tape and transistor-like)"
                                                                            : "even and odd alike")));

            lines.push_back (text ("Between the harmonics: %.0f dB under the sine (aliasing or noise: hard clipping without oversampling "
                                   "measures about -45 dB, gentle or oversampled saturation -70 dB or lower).", otherDb));

            if (otherDb > -50.0)
                problems.push_back (text ("Aliasing: what comes out of a clean sine has %.0f dB of energy that's neither the sine nor its "
                                          "harmonics. Oversample the distortion (2x to 4x), or soften it.", otherDb));
        }
    }

    //==========================================================================
    // Stereo.
    {
        const auto s = stereoOf (l, r, startAt, measureTo);
        if (s.sideDb < -90.0)
            lines.push_back ("Stereo: mono (left and right are the same).");
        else
            lines.push_back (text ("Stereo: correlation %.2f (1 mono, 0 very wide, below 0 out of phase); the sides %.1f dB under the middle; "
                                   "in mono it %s %.1f dB; balance %+.1f dB (left against right).",
                                   s.correlation, s.sideDb, s.monoLossDb < 0.0 ? "loses" : "gains", std::abs (s.monoLossDb), s.balanceDb));

        if (s.correlation < -0.2)
            problems.push_back ("Out of phase: the left and right cancel each other, so it gets thin or vanishes in mono. Keep the low end "
                                "in the middle and don't invert one side.");
        else if (s.monoLossDb < -4.0)
            problems.push_back (text ("Weak in mono (loses %.1f dB): the width comes from phase tricks. Keep the bass mono and widen only "
                                      "the highs.", -s.monoLossDb));

        if (std::abs (s.balanceDb) > 3.0)
            problems.push_back (text ("Leans to the %s by %.1f dB.", s.balanceDb > 0.0 ? "left" : "right", std::abs (s.balanceDb)));
    }

    std::string report;

    for (const auto& line : lines)
        report += line + "\n";

    report += problems.empty() ? "\nNothing technical stands out: judge the character against what the user asked for.\n"
                               : "\nWorth fixing:\n";

    for (const auto& p : problems)
        report += "- " + p + "\n";

    return report;
}
}
