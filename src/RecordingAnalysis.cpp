// D:\workspace\Fanan_web_hub\src\RecordingAnalysis.cpp

#include "RecordingAnalysis.h"
#include "AudioEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <string_view>


namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr double rate = RecordingAnalysis::rate;

    //==============================================================================
    /** Magnitude spectra of Hann-windowed real frames of 2^order samples, through a complex
        FFT of half the size. */
    class Spectrum
    {
    public:
        explicit Spectrum (int order)
            : size (1 << order), half (size / 2),
              window (static_cast<size_t> (size)), re (static_cast<size_t> (half)), im (static_cast<size_t> (half)),
              halfCos (static_cast<size_t> (half / 2)), halfSin (static_cast<size_t> (half / 2)),
              fullCos (static_cast<size_t> (half + 1)), fullSin (static_cast<size_t> (half + 1)),
              reversed (static_cast<size_t> (half))
        {
            for (int i = 0; i < size; ++i)
                window[static_cast<size_t> (i)] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * pi * i / size));

            for (int j = 0; j < half / 2; ++j)
            {
                halfCos[static_cast<size_t> (j)] = std::cos (2.0 * pi * j / half);
                halfSin[static_cast<size_t> (j)] = -std::sin (2.0 * pi * j / half);
            }

            for (int k = 0; k <= half; ++k)
            {
                fullCos[static_cast<size_t> (k)] = std::cos (2.0 * pi * k / size);
                fullSin[static_cast<size_t> (k)] = -std::sin (2.0 * pi * k / size);
            }

            int bits = 0;

            while ((1 << bits) < half)
                ++bits;

            for (int i = 0; i < half; ++i)
            {
                int r = 0;

                for (int b = 0; b < bits; ++b)
                    if (i & (1 << b))
                        r |= 1 << (bits - 1 - b);

                reversed[static_cast<size_t> (i)] = r;
            }
        }

        int getSize() const noexcept           { return size; }

        /** |X[k]| for k = 0 .. size / 2 of a frame of size samples. */
        void magnitudes (const float* frame, std::vector<float>& out)
        {
            for (int n = 0; n < half; ++n)
            {
                const auto i = static_cast<size_t> (reversed[static_cast<size_t> (n)]);
                re[i] = frame[2 * n] * window[static_cast<size_t> (2 * n)];
                im[i] = frame[2 * n + 1] * window[static_cast<size_t> (2 * n + 1)];
            }

            transform();
            out.resize (static_cast<size_t> (half + 1));

            for (int k = 0; k <= half; ++k)
            {
                const auto a = static_cast<size_t> (k % half), b = static_cast<size_t> ((half - k) % half);
                const auto zr = re[a], zi = im[a], cr = re[b], ci = -im[b];
                const auto er = 0.5 * (zr + cr), ei = 0.5 * (zi + ci);
                const auto odr = 0.5 * (zi - ci), odi = -0.5 * (zr - cr);
                const auto wr = fullCos[static_cast<size_t> (k)], wi = fullSin[static_cast<size_t> (k)];
                const auto xr = er + wr * odr - wi * odi, xi = ei + wr * odi + wi * odr;
                out[static_cast<size_t> (k)] = static_cast<float> (std::sqrt (xr * xr + xi * xi));
            }
        }

    private:
        void transform()
        {
            for (int length = 2; length <= half; length <<= 1)
            {
                const auto step = half / length;

                for (int start = 0; start < half; start += length)
                {
                    for (int j = 0; j < length / 2; ++j)
                    {
                        const auto wr = halfCos[static_cast<size_t> (j * step)], wi = halfSin[static_cast<size_t> (j * step)];
                        const auto a = static_cast<size_t> (start + j), b = a + static_cast<size_t> (length / 2);
                        const auto tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                        re[b] = re[a] - tr;
                        im[b] = im[a] - ti;
                        re[a] += tr;
                        im[a] += ti;
                    }
                }
            }
        }

        int size, half;
        std::vector<float> window;
        std::vector<double> re, im, halfCos, halfSin, fullCos, fullSin;
        std::vector<int> reversed;
    };

    /** Frame `index` of a signal (hop apart, size long), zeros past its ends. */
    void frameOf (const std::vector<float>& x, int index, int hop, std::vector<float>& frame)
    {
        const auto from = static_cast<long long> (index) * hop;
        const auto size = static_cast<long long> (frame.size());
        const auto n = static_cast<long long> (x.size());
        std::fill (frame.begin(), frame.end(), 0.0f);

        if (from < n)
            std::copy (x.begin() + from, x.begin() + std::min (n, from + size), frame.begin());
    }

    //==============================================================================
    // Onsets: how strongly the sound starts something new, frame by frame.
    struct Onsets
    {
        static constexpr int size = 1024, hop = 256;
        static constexpr double fps = rate / hop;

        std::vector<float> strength;   // spectral flux above its local average
        std::vector<float> low;        // the same under 200 Hz: kicks, bass notes
        std::vector<float> level;      // loudness (rms)

        /** Seconds from the signal's start of a frame's middle, and back. */
        static double secondsOf (double frame)    { return (frame * hop + size / 2) / rate; }
        static double frameAt (double seconds)    { return (seconds * rate - size / 2) / hop; }
    };

    bool findOnsets (const std::vector<float>& x, Onsets& onsets, const RecordingAnalysis::Report& report, double from, double to)
    {
        Spectrum spectrum (10);
        const auto n = static_cast<int> (x.size());
        const auto frames = 1 + std::max (0, (n - Onsets::size + Onsets::hop - 1) / Onsets::hop);
        const auto binHz = rate / Onsets::size;
        const auto top = static_cast<int> (8000.0 / binHz), lowTop = static_cast<int> (200.0 / binHz);

        std::vector<float> frame (static_cast<size_t> (Onsets::size)), mags, previous;
        std::vector<float> flux (static_cast<size_t> (frames)), low (static_cast<size_t> (frames)), level (static_cast<size_t> (frames));

        for (int t = 0; t < frames; ++t)
        {
            frameOf (x, t, Onsets::hop, frame);
            double power = 0.0;

            for (const auto s : frame)
                power += static_cast<double> (s) * s;

            level[static_cast<size_t> (t)] = static_cast<float> (std::sqrt (power / Onsets::size));
            spectrum.magnitudes (frame.data(), mags);

            // Each bin's rise, on a compressed scale (loud and soft sounds alike).
            for (int k = 0; k <= top; ++k)
                mags[static_cast<size_t> (k)] = std::log1p (100.0f * mags[static_cast<size_t> (k)] / (Onsets::size / 4));

            if (! previous.empty())
            {
                double all = 0.0, under = 0.0;

                for (int k = 1; k <= top; ++k)
                {
                    const auto rise = std::max (0.0f, mags[static_cast<size_t> (k)] - previous[static_cast<size_t> (k)]);
                    all += rise;

                    if (k <= lowTop)
                        under += rise;
                }

                flux[static_cast<size_t> (t)] = static_cast<float> (all);
                low[static_cast<size_t> (t)] = static_cast<float> (under);
            }

            std::swap (previous, mags);

            if ((t & 511) == 0 && ! report (from + (to - from) * t / frames))
                return false;
        }

        // Above the average around it (a quarter of a second each way): onsets, not loudness.
        const auto reach = static_cast<int> (0.25 * Onsets::fps);
        std::vector<double> sum (static_cast<size_t> (frames + 1), 0.0);

        for (int t = 0; t < frames; ++t)
            sum[static_cast<size_t> (t + 1)] = sum[static_cast<size_t> (t)] + flux[static_cast<size_t> (t)];

        onsets.strength.assign (static_cast<size_t> (frames), 0.0f);

        for (int t = 0; t < frames; ++t)
        {
            const auto a = std::max (0, t - reach), b = std::min (frames, t + reach + 1);
            const auto mean = (sum[static_cast<size_t> (b)] - sum[static_cast<size_t> (a)]) / (b - a);
            onsets.strength[static_cast<size_t> (t)] = static_cast<float> (std::max (0.0, flux[static_cast<size_t> (t)] - mean));
        }

        onsets.low = std::move (low);
        onsets.level = std::move (level);
        return true;
    }

    /** Gaussian smoothing, sigma in frames. */
    std::vector<float> smooth (const std::vector<float>& v, double sigma)
    {
        const auto reach = static_cast<int> (std::ceil (3.0 * sigma));
        std::vector<double> kernel (static_cast<size_t> (2 * reach + 1));
        double total = 0.0;

        for (int i = -reach; i <= reach; ++i)
            total += kernel[static_cast<size_t> (i + reach)] = std::exp (-0.5 * i * i / (sigma * sigma));

        std::vector<float> out (v.size(), 0.0f);
        const auto n = static_cast<int> (v.size());

        for (int t = 0; t < n; ++t)
        {
            double s = 0.0;

            for (int i = -reach; i <= reach; ++i)
                if (t + i >= 0 && t + i < n)
                    s += kernel[static_cast<size_t> (i + reach)] * v[static_cast<size_t> (t + i)];

            out[static_cast<size_t> (t)] = static_cast<float> (s / total);
        }

        return out;
    }

    float valueAt (const std::vector<float>& v, double position)
    {
        if (position < 0.0 || position > static_cast<double> (v.size()) - 1.0)
            return 0.0f;

        const auto i = static_cast<size_t> (position);
        const auto f = static_cast<float> (position - static_cast<double> (i));
        return i + 1 < v.size() ? v[i] + f * (v[i + 1] - v[i]) : v[i];
    }

    //==============================================================================
    // The beat: a steady pulse, period and phase in onset frames. A pulse is the song's beat:
    // a quarter note, or a dotted quarter in 6/8, 9/8 and 12/8.
    struct Pulse
    {
        double period = 0.0;     // frames
        double phase = 0.0;      // frames: pulses fall at pivot + phase + k * period
        double score = 0.0;      // the average onset strength the pulses meet
    };

    struct Metre
    {
        int pulsesPerBar = 4;
        double quartersPerPulse = 1.0;

        explicit Metre (const RecordingAnalysis::Grid& grid)
        {
            const auto numerator = std::max (1, grid.numerator), denominator = std::max (1, grid.denominator);

            if (denominator == 8 && numerator % 3 == 0)
            {
                pulsesPerBar = numerator / 3;
                quartersPerPulse = 1.5;
            }
            else
            {
                const auto quarters = numerator * 4.0 / denominator;
                pulsesPerBar = std::max (1, static_cast<int> (std::lround (quarters)));
                quartersPerPulse = quarters / pulsesPerBar;
            }
        }
    };

    /** The average onset strength met by pulses (period, phase about pivot) from frame `first` to `last`. */
    double meet (const std::vector<float>& s, double pivot, double period, double phase, double first, double last)
    {
        const auto k0 = std::ceil ((first - pivot - phase) / period);
        const auto k1 = std::floor ((last - pivot - phase) / period);
        double total = 0.0;
        int count = 0;

        for (auto k = k0; k <= k1; k += 1.0)
        {
            total += valueAt (s, pivot + phase + k * period);
            ++count;
        }

        return count > 0 ? total / count : 0.0;
    }

    /** How regularly the onsets come round at each period (frames, the index), leaning toward
        `around` pulses a minute: their autocorrelation, weighted. Empty when there's too little. */
    std::vector<double> regularity (const std::vector<float>& o, int first, int last, double around)
    {
        const auto n = last - first + 1;
        double mean = 0.0;

        for (int t = first; t <= last; ++t)
            mean += o[static_cast<size_t> (t)];

        mean /= std::max (1, n);
        std::vector<double> c (static_cast<size_t> (std::max (0, n)));

        for (int t = 0; t < n; ++t)
            c[static_cast<size_t> (t)] = o[static_cast<size_t> (first + t)] - mean;

        const auto minLag = static_cast<int> (std::floor (Onsets::fps * 60.0 / 240.0));
        const auto maxLag = std::min (n / 2, static_cast<int> (std::ceil (Onsets::fps * 60.0 / 40.0)));

        if (maxLag <= minLag + 2)
            return {};

        double zero = 0.0;

        for (const auto v : c)
            zero += v * v;

        if (zero <= 0.0)
            return {};

        zero /= n;
        std::vector<double> score (static_cast<size_t> (maxLag + 2), 0.0);

        for (int lag = minLag - 1; lag <= maxLag + 1; ++lag)
        {
            double s = 0.0;

            for (int t = 0; t + lag < n; ++t)
                s += c[static_cast<size_t> (t)] * c[static_cast<size_t> (t + lag)];

            const auto ac = s / (n - lag) / zero;
            const auto bpm = Onsets::fps * 60.0 / lag;
            const auto octaves = std::log2 (bpm / around) / 0.9;
            score[static_cast<size_t> (lag)] = std::max (0.0, ac) * std::exp (-0.5 * octaves * octaves);
        }

        return score;
    }

    /** The top of the parabola through a lag and its neighbours. */
    double refine (const std::vector<double>& score, int lag)
    {
        if (lag < 1 || lag + 1 >= static_cast<int> (score.size()))
            return lag;

        const auto a = score[static_cast<size_t> (lag - 1)], b = score[static_cast<size_t> (lag)], d = score[static_cast<size_t> (lag + 1)];
        const auto bend = a - 2.0 * b + d;
        return lag + (bend < 0.0 ? std::clamp (0.5 * (a - d) / bend, -0.5, 0.5) : 0.0);
    }

    /** How much a pulse's beats stand out from what falls midway between them (0..1): low when
        the midpoints are beats too, and the pulse skips every other one. */
    double contrastOf (const std::vector<float>& smoothed, double period, int first, int last)
    {
        double phase = 0.0, on = 0.0;

        for (double f = 0.0; f < period; f += 0.5)
        {
            if (const auto v = meet (smoothed, 0.0, period, f, first, last); v > on)
            {
                on = v;
                phase = f;
            }
        }

        const auto between = meet (smoothed, 0.0, period, phase + 0.5 * period, first, last);
        return on > 0.0 ? std::clamp ((on - between) / on, 0.0, 1.0) : 0.0;
    }

    /** The strongest lag within 4% of a target lag, between the frames. */
    double peakNear (const std::vector<double>& score, double target, int minLag)
    {
        const auto maxLag = static_cast<int> (score.size()) - 2;
        const auto from = std::max (minLag, static_cast<int> (std::floor (target * 0.96)));
        const auto to = std::min (maxLag, static_cast<int> (std::ceil (target * 1.04)));

        if (from > to)
            return 0.0;

        int lag = from;

        for (int l = from; l <= to; ++l)
            if (score[static_cast<size_t> (l)] > score[static_cast<size_t> (lag)])
                lag = l;

        return refine (score, lag);
    }

    /** The pulse's period (frames): the most regular; or half of it, when that's nearly as
        regular and the slower pulse skips beats as strong as its own (every other beat). */
    double pulsePeriod (const std::vector<float>& o, const std::vector<float>& smoothed, int first, int last,
                        double around, bool compound)
    {
        const auto score = regularity (o, first, last, around);

        if (score.empty())
            return 0.0;

        const auto minLag = static_cast<int> (std::floor (Onsets::fps * 60.0 / 240.0));
        const auto maxLag = static_cast<int> (score.size()) - 2;
        int best = minLag;

        for (int lag = minLag; lag <= maxLag; ++lag)
            if (score[static_cast<size_t> (lag)] > score[static_cast<size_t> (best)])
                best = lag;

        if (score[static_cast<size_t> (best)] <= 0.0)
            return 0.0;

        const auto period = refine (score, best);

        if (compound)
            return period;

        if (const auto faster = peakNear (score, 0.5 * period, minLag); faster > 0.0)
        {
            const auto regular = score[static_cast<size_t> (std::lround (faster))] / score[static_cast<size_t> (best)];
            const auto slowContrast = contrastOf (smoothed, period, first, last);
            const auto fastContrast = contrastOf (smoothed, faster, first, last);


            if (regular >= 0.85 && slowContrast < 0.35 && fastContrast >= 0.75)
                return faster;
        }

        return period;
    }

    /** The steady pulse that meets the onsets best, its period within `spread` of the given one. */
    Pulse fitPulse (const std::vector<float>& s, double period, double spread, double first, double last)
    {
        const auto pivot = 0.5 * (first + last);
        Pulse best;

        const auto search = [&] (double fromPeriod, double toPeriod, double periodStep, double phaseFrom, double phaseTo, double phaseStep)
        {
            for (auto p = fromPeriod; p <= toPeriod + 1.0e-12; p += periodStep)
            {
                for (auto f = phaseFrom; f < phaseTo; f += phaseStep)
                {
                    const auto score = meet (s, pivot, p, f, first, last);

                    if (score > best.score)
                        best = { p, f, score };
                }
            }
        };

        search (period * (1.0 - spread), period * (1.0 + spread), period * 0.0005, 0.0, period, 0.25);

        const auto coarse = best;
        search (coarse.period * (1.0 - 0.0008), coarse.period * (1.0 + 0.0008), coarse.period * 0.00005,
                coarse.phase - 0.5, coarse.phase + 0.5, 0.05);

        best.phase = std::fmod (std::fmod (best.phase, best.period) + best.period, best.period);
        best.phase += pivot;   // from here on, about frame 0
        best.phase = std::fmod (best.phase, best.period);
        return best;
    }

    //==============================================================================
    // Notes: what pitches sound, frame by frame, gathered into pitch classes.
    struct Notes
    {
        static constexpr int size = 8192, hop = 1024;

        std::vector<std::array<float, 12>> chroma;   // all the notes heard
        std::vector<std::array<float, 12>> bass;     // the lowest one
        std::vector<float> loudness;

        static double secondsOf (int frame)    { return (frame * static_cast<double> (hop) + size / 2) / rate; }
    };

    struct Peak
    {
        float frequency, amplitude;
    };

    /** The spectral peaks of a frame, 35 Hz .. 5 kHz, frequency and amplitude between the bins. */
    void findPeaks (const std::vector<float>& mags, std::vector<Peak>& peaks)
    {
        peaks.clear();
        const auto binHz = rate / Notes::size;
        const auto first = std::max (3, static_cast<int> (35.0 / binHz)), last = std::min (static_cast<int> (mags.size()) - 3, static_cast<int> (5000.0 / binHz));
        float top = 0.0f;

        for (int k = first; k <= last; ++k)
            top = std::max (top, mags[static_cast<size_t> (k)]);

        if (top <= 1.0e-9f)
            return;

        const auto floor = top * 0.0056f;   // -45 dB

        for (int k = first; k <= last; ++k)
        {
            const auto m = mags[static_cast<size_t> (k)];

            if (m < floor || m <= mags[static_cast<size_t> (k - 1)] || m < mags[static_cast<size_t> (k + 1)]
                || m <= mags[static_cast<size_t> (k - 2)] || m < mags[static_cast<size_t> (k + 2)])
                continue;

            const auto a = std::log (mags[static_cast<size_t> (k - 1)] + 1.0e-12f), b = std::log (m), c = std::log (mags[static_cast<size_t> (k + 1)] + 1.0e-12f);
            const auto bend = a - 2.0f * b + c;
            const auto shift = bend < 0.0f ? std::clamp (0.5f * (a - c) / bend, -0.5f, 0.5f) : 0.0f;
            peaks.push_back ({ static_cast<float> ((k + shift) * binHz), std::exp (b - 0.25f * (a - c) * shift) });
        }

        // The strongest sixty, by frequency.
        if (peaks.size() > 60)
        {
            std::nth_element (peaks.begin(), peaks.begin() + 60, peaks.end(), [] (const Peak& x, const Peak& y) { return x.amplitude > y.amplitude; });
            peaks.resize (60);
        }

        std::sort (peaks.begin(), peaks.end(), [] (const Peak& x, const Peak& y) { return x.frequency < y.frequency; });
    }

    /** The tuning (cents from A = 440 Hz, -50..50) the peaks lean to, by their strength. */
    double tuningOf (const std::vector<std::vector<Peak>>& frames)
    {
        double x = 0.0, y = 0.0;

        for (const auto& peaks : frames)
        {
            for (const auto& p : peaks)
            {
                if (p.frequency < 80.0f || p.frequency > 2000.0f)
                    continue;

                const auto cents = 1200.0 * std::log2 (p.frequency / 440.0);
                const auto angle = 2.0 * pi * cents / 100.0;
                x += p.amplitude * std::cos (angle);
                y += p.amplitude * std::sin (angle);
            }
        }

        return x == 0.0 && y == 0.0 ? 0.0 : std::atan2 (y, x) / (2.0 * pi) * 100.0;
    }

    /** The notes of one frame: the pitch whose harmonics sound strongest, then the next with
        what it explains taken away, and so on. MIDI pitch and strength. */
    void notesOf (std::vector<Peak> peaks, double reference, std::vector<std::pair<int, float>>& found)
    {
        found.clear();
        constexpr int lowest = 28, highest = 96;   // E1 .. C7
        std::array<float, highest + 1> strength {};
        std::array<std::array<int, 10>, highest + 1> matched {};
        float first = 0.0f;

        const auto nearest = [&peaks] (double target, double tolerance)
        {
            auto it = std::lower_bound (peaks.begin(), peaks.end(), target - tolerance, [] (const Peak& p, double f) { return p.frequency < f; });
            int best = -1;

            for (; it != peaks.end() && it->frequency <= target + tolerance; ++it)
                if (it->amplitude > 0.0f && (best < 0 || it->amplitude > peaks[static_cast<size_t> (best)].amplitude))
                    best = static_cast<int> (it - peaks.begin());

            return best;
        };

        // The out-of-tune harmonics of the notes found (the 7th sits a third of a semitone under
        // a note, the 11th and 13th between two): a peak there is theirs, not a note's own pitch.
        std::vector<double> outOfTune;

        const auto isOutOfTune = [&outOfTune] (double frequency, double pitch)
        {
            if (std::abs (1200.0 * std::log2 (frequency / pitch)) <= 20.0)
                return false;

            for (const auto partial : outOfTune)
                if (std::abs (1200.0 * std::log2 (frequency / partial)) <= 12.0)
                    return true;

            return false;
        };

        for (int round = 0; round < 6; ++round)
        {
            int bestPitch = -1;

            for (int p = lowest; p <= highest; ++p)
            {
                const auto f0 = reference * std::pow (2.0, (p - 69) / 12.0);
                double s = 0.0, weight = 1.0;
                auto& hits = matched[static_cast<size_t> (p)];
                hits.fill (-1);

                for (int h = 1; h <= 10 && h * f0 <= 5000.0; ++h, weight *= 0.8)
                {
                    // Harmonics within a quarter tone or so: strings stretch them a little.
                    const auto target = h * f0;
                    const auto at = nearest (target, std::max (target * 0.025, 1.0 * rate / Notes::size));

                    if (at < 0 || (h == 1 && isOutOfTune (peaks[static_cast<size_t> (at)].frequency, f0)))
                    {
                        if (h == 1)
                            break;      // a note sounds its own pitch

                        continue;
                    }

                    hits[static_cast<size_t> (h - 1)] = at;
                    s += weight * std::sqrt (peaks[static_cast<size_t> (at)].amplitude);
                }

                if (hits[0] < 0)
                    s = 0.0;

                strength[static_cast<size_t> (p)] = static_cast<float> (s);

                if (s > 0.0 && (bestPitch < 0 || s > strength[static_cast<size_t> (bestPitch)]))
                    bestPitch = p;
            }

            if (bestPitch < 0)
                break;

            const auto s = strength[static_cast<size_t> (bestPitch)];

            if (round == 0)
                first = s;
            else if (s < 0.12f * first)
                break;

            found.push_back ({ bestPitch, s });

            if (const auto own = matched[static_cast<size_t> (bestPitch)][0]; own >= 0)
                for (const auto h : { 7 })
                    outOfTune.push_back (h * static_cast<double> (peaks[static_cast<size_t> (own)].frequency));

            // What it explains: its own pitch, and its harmonics as far as they follow a smooth
            // envelope; a harmonic that stands out above its neighbours is another note's too.
            const auto& hits = matched[static_cast<size_t> (bestPitch)];
            std::array<float, 12> amplitude {};

            for (int h = 1; h <= 10; ++h)
                if (const auto at = hits[static_cast<size_t> (h - 1)]; at >= 0)
                    amplitude[static_cast<size_t> (h)] = peaks[static_cast<size_t> (at)].amplitude;

            for (int h = 1; h <= 10; ++h)
            {
                const auto at = hits[static_cast<size_t> (h - 1)];

                if (at < 0)
                    continue;

                auto& peak = peaks[static_cast<size_t> (at)].amplitude;

                if (h == 1)
                {
                    peak = 0.0f;
                    continue;
                }

                const auto here = amplitude[static_cast<size_t> (h)];
                const auto around = (amplitude[static_cast<size_t> (h - 1)] + here + amplitude[static_cast<size_t> (h + 1)]) / 3.0f;
                peak = std::max (0.0f, peak - std::min (here, std::max (around, 0.5f * here)));
            }
        }
    }

    bool findNotes (const std::vector<float>& x, Notes& notes, const RecordingAnalysis::Report& report, double from, double to)
    {
        Spectrum spectrum (13);
        const auto n = static_cast<int> (x.size());
        const auto frames = 1 + std::max (0, (n - Notes::size + Notes::hop - 1) / Notes::hop);
        std::vector<float> frame (static_cast<size_t> (Notes::size)), mags;
        std::vector<std::vector<Peak>> peaks (static_cast<size_t> (frames));
        notes.loudness.assign (static_cast<size_t> (frames), 0.0f);

        for (int t = 0; t < frames; ++t)
        {
            frameOf (x, t, Notes::hop, frame);
            double power = 0.0;

            for (const auto s : frame)
                power += static_cast<double> (s) * s;

            notes.loudness[static_cast<size_t> (t)] = static_cast<float> (std::sqrt (power / Notes::size));
            spectrum.magnitudes (frame.data(), mags);
            findPeaks (mags, peaks[static_cast<size_t> (t)]);

            if ((t & 63) == 0 && ! report (from + 0.6 * (to - from) * t / frames))
                return false;
        }

        const auto reference = 440.0 * std::pow (2.0, tuningOf (peaks) / 1200.0);
        notes.chroma.assign (static_cast<size_t> (frames), {});
        notes.bass.assign (static_cast<size_t> (frames), {});
        std::vector<std::pair<int, float>> found;

        for (int t = 0; t < frames; ++t)
        {
            notesOf (peaks[static_cast<size_t> (t)], reference, found);
            auto& chroma = notes.chroma[static_cast<size_t> (t)];
            float strongest = 0.0f;

            // The chord is heard above the bass: the lowest octaves count for less.
            for (const auto& [pitch, strength] : found)
            {
                chroma[static_cast<size_t> (pitch % 12)] += strength * (pitch < 40 ? 0.25f : pitch < 48 ? 0.6f : 1.0f);
                strongest = std::max (strongest, strength);
            }

            // The bass: the lowest note of any weight.
            int lowest = 128;
            float lowStrength = 0.0f;

            for (const auto& [pitch, strength] : found)
            {
                if (strength >= 0.25f * strongest && pitch < lowest)
                {
                    lowest = pitch;
                    lowStrength = strength;
                }
            }

            if (lowest < 128)
                notes.bass[static_cast<size_t> (t)][static_cast<size_t> (lowest % 12)] += lowStrength;

            if ((t & 63) == 0 && ! report (from + (to - from) * (0.6 + 0.4 * t / frames)))
                return false;
        }

        return true;
    }

    /** The notes heard between two times (seconds from the signal's start); the frames at the
        edges, which hear the neighbours too, are left out. */
    void gather (const Notes& notes, double fromIn, double toIn, std::array<float, 12>& chroma, std::array<float, 12>& bass, double& loudness)
    {
        chroma.fill (0.0f);
        bass.fill (0.0f);
        loudness = 0.0;
        const auto margin = std::min (0.12, 0.2 * (toIn - fromIn));
        const auto from = fromIn + margin, to = toIn - margin;
        const auto frames = static_cast<int> (notes.chroma.size());
        auto first = static_cast<int> (std::ceil ((from * rate - Notes::size / 2) / Notes::hop));
        auto last = static_cast<int> (std::ceil ((to * rate - Notes::size / 2) / Notes::hop)) - 1;

        // A span shorter than a frame takes the nearest frame.
        if (last < first)
            first = last = static_cast<int> (std::lround ((0.5 * (from + to) * rate - Notes::size / 2) / Notes::hop));

        int count = 0;

        for (int t = std::max (0, first); t <= std::min (frames - 1, last); ++t)
        {
            for (size_t pc = 0; pc < 12; ++pc)
            {
                chroma[pc] += notes.chroma[static_cast<size_t> (t)][pc];
                bass[pc] += notes.bass[static_cast<size_t> (t)][pc];
            }

            loudness += notes.loudness[static_cast<size_t> (t)];
            ++count;
        }

        if (count > 0)
            loudness /= count;
    }

    //==============================================================================
    // Chords
    const std::array<const char*, 12> noteNames { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    struct Shape
    {
        const char* name;
        std::vector<int> tones;
        double bias;            // how readily it's heard: the triads first
    };

    const std::vector<Shape>& shapes()
    {
        static const std::vector<Shape> list {
            { "",     { 0, 4, 7 },      0.0 },
            { "m",    { 0, 3, 7 },      0.0 },
            { "7",    { 0, 4, 7, 10 }, -0.015 },
            { "maj7", { 0, 4, 7, 11 }, -0.02 },
            { "m7",   { 0, 3, 7, 10 }, -0.015 },
            { "sus4", { 0, 5, 7 },     -0.08 },
            { "dim",  { 0, 3, 6 },     -0.09 },
            { "5",    { 0, 7 },        -0.07 }
        };

        return list;
    }

    struct Heard
    {
        int root = -1;
        int shape = 0;
        int bass = -1;          // a chord tone other than the root in the bass: an inversion
        double score = -1.0e9;

        juce::String name() const
        {
            if (root < 0)
                return "N.C.";

            juce::String text (noteNames[static_cast<size_t> (root)]);
            text << shapes()[static_cast<size_t> (shape)].name;

            if (bass >= 0 && bass != root)
                text << "/" << noteNames[static_cast<size_t> (bass)];

            return text;
        }
    };

    /** The key the whole recording leans to (Krumhansl and Kessler's profiles): its tonic, and
        the notes that belong (a minor key's leading note too, for its major dominant). */
    struct Key
    {
        int tonic = 0;
        bool minor = false;
        std::array<bool, 12> scale {};

        bool holds (int root, const std::vector<int>& tones) const
        {
            for (const auto tone : tones)
                if (! scale[static_cast<size_t> ((root + tone) % 12)])
                    return false;

            return true;
        }
    };

    Key keyOf (const Notes& notes)
    {
        static const std::array<double, 12> majorProfile { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const std::array<double, 12> minorProfile { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        std::array<double, 12> total {};

        for (const auto& frame : notes.chroma)
            for (size_t pc = 0; pc < 12; ++pc)
                total[pc] += frame[pc];

        const auto correlation = [&total] (const std::array<double, 12>& profile, int tonic)
        {
            double mx = 0.0, my = 0.0;

            for (size_t i = 0; i < 12; ++i)
            {
                mx += total[i];
                my += profile[i];
            }

            mx /= 12.0;
            my /= 12.0;
            double sxy = 0.0, sxx = 0.0, syy = 0.0;

            for (int i = 0; i < 12; ++i)
            {
                const auto x = total[static_cast<size_t> (i)] - mx;
                const auto y = profile[static_cast<size_t> ((i - tonic + 12) % 12)] - my;
                sxy += x * y;
                sxx += x * x;
                syy += y * y;
            }

            return sxx > 0.0 && syy > 0.0 ? sxy / std::sqrt (sxx * syy) : 0.0;
        };

        Key key;
        double best = -2.0;

        for (int tonic = 0; tonic < 12; ++tonic)
        {
            for (const auto minor : { false, true })
            {
                if (const auto r = correlation (minor ? minorProfile : majorProfile, tonic); r > best)
                {
                    best = r;
                    key.tonic = tonic;
                    key.minor = minor;
                }
            }
        }

        for (const auto step : key.minor ? std::vector<int> { 0, 2, 3, 5, 7, 8, 10, 11 } : std::vector<int> { 0, 2, 4, 5, 7, 9, 11 })
            key.scale[static_cast<size_t> ((key.tonic + step) % 12)] = true;

        return key;
    }

    /** Notes heard, made comparable: the chroma at unit length, the bass as shares. */
    struct Heard12
    {
        std::array<double, 12> chroma {}, bass {};
        bool empty = true;

        Heard12 (const std::array<float, 12>& chromaIn, const std::array<float, 12>& bassIn)
        {
            double norm = 0.0, bassTotal = 0.0;

            for (size_t pc = 0; pc < 12; ++pc)
            {
                norm += static_cast<double> (chromaIn[pc]) * chromaIn[pc];
                bassTotal += bassIn[pc];
            }

            if (norm <= 0.0)
                return;

            empty = false;
            norm = std::sqrt (norm);

            for (size_t pc = 0; pc < 12; ++pc)
            {
                chroma[pc] = chromaIn[pc] / norm;
                bass[pc] = bassTotal > 0.0 ? bassIn[pc] / bassTotal : 0.0;
            }
        }
    };

    /** How well a chord explains what's heard: the notes it holds (each shape scaled alike), how
        readily such a chord is heard, the bass on its root, and the key's own chords first. */
    double fit (const Heard12& heard, int root, int shapeIndex, const Key& key)
    {
        const auto& shape = shapes()[static_cast<size_t> (shapeIndex)];
        double inside = 0.0;

        for (const auto tone : shape.tones)
            inside += heard.chroma[static_cast<size_t> ((root + tone) % 12)];

        return inside / std::sqrt (static_cast<double> (shape.tones.size())) + shape.bias
             + 0.05 * heard.bass[static_cast<size_t> (root)] + (key.holds (root, shape.tones) ? 0.03 : 0.0);
    }

    Heard chordOf (const Heard12& heard, const Key& key)
    {
        Heard best;

        if (heard.empty)
            return best;

        const auto& c = heard.chroma;
        const auto& b = heard.bass;

        for (int root = 0; root < 12; ++root)
        {
            for (int s = 0; s < static_cast<int> (shapes().size()); ++s)
            {
                if (const auto score = fit (heard, root, s, key); score > best.score)
                {
                    best.root = root;
                    best.shape = s;
                    best.score = score;
                }
            }
        }

        // A power chord (root and fifth alone) is played as the key has it: major or minor, as
        // the third that's there leans, or as the key's scale says.
        if (std::string_view (shapes()[static_cast<size_t> (best.shape)].name) == "5")
        {
            const auto major = key.holds (best.root, { 0, 4, 7 }), minor = key.holds (best.root, { 0, 3, 7 });
            const auto majorThird = c[static_cast<size_t> ((best.root + 4) % 12)], minorThird = c[static_cast<size_t> ((best.root + 3) % 12)];

            if (major && (! minor || majorThird >= minorThird))
                best.shape = 0;
            else if (minor)
                best.shape = 1;
        }

        // An inversion: another chord tone clearly in the bass.
        int low = 0;

        for (int pc = 1; pc < 12; ++pc)
            if (b[static_cast<size_t> (pc)] > b[static_cast<size_t> (low)])
                low = pc;

        if (low != best.root && b[static_cast<size_t> (low)] >= 0.6 && b[static_cast<size_t> (low)] >= 2.0 * b[static_cast<size_t> (best.root)])
            for (const auto tone : shapes()[static_cast<size_t> (best.shape)].tones)
                if ((best.root + tone) % 12 == low)
                    best.bass = low;

        return best;
    }

    /** The bass note heard most, when it's most of what the bass played; or -1. */
    int lowestOf (const std::array<float, 12>& bass)
    {
        float total = 0.0f;
        int low = 0;

        for (int pc = 0; pc < 12; ++pc)
        {
            total += bass[static_cast<size_t> (pc)];

            if (bass[static_cast<size_t> (pc)] > bass[static_cast<size_t> (low)])
                low = pc;
        }

        return total > 0.0f && bass[static_cast<size_t> (low)] >= 0.5f * total ? low : -1;
    }

    double cosine (const std::array<float, 12>& a, const std::array<float, 12>& b)
    {
        double ab = 0.0, aa = 0.0, bb = 0.0;

        for (size_t i = 0; i < 12; ++i)
        {
            ab += static_cast<double> (a[i]) * b[i];
            aa += static_cast<double> (a[i]) * a[i];
            bb += static_cast<double> (b[i]) * b[i];
        }

        return aa > 0.0 && bb > 0.0 ? ab / std::sqrt (aa * bb) : 0.0;
    }
}

//==============================================================================
RecordingAnalysis::RecordingAnalysis (std::vector<Clip> c, Grid s, double t)
    : juce::Thread ("Sonify recording analysis"), clips (std::move (c)), song (s), tempo (t)
{
}

RecordingAnalysis::~RecordingAnalysis()
{
    stopThread (10000);
}

void RecordingAnalysis::run()
{
    double songSeconds = 0.0;
    auto mono = mixDown (clips, songSeconds);
    clips.clear();   // the audio it held is let go

    if (threadShouldExit())
    {
        result.error = "Cancelled";
    }
    else
    {
        result = analyse (std::move (mono), songSeconds, song, tempo, [this] (double p)
        {
            progress = 0.1 + 0.9 * p;
            return ! threadShouldExit();
        });
    }

    progress = 1.0;
    done = true;
}

std::vector<float> RecordingAnalysis::mixDown (const std::vector<Clip>& clips, double& songSeconds)
{
    songSeconds = 0.0;
    double from = 1.0e30, to = 0.0;

    for (const auto& clip : clips)
    {
        if (clip.data == nullptr || clip.lengthSeconds <= 0.0)
            continue;

        from = std::min (from, clip.startSeconds);
        to = std::max (to, clip.startSeconds + clip.lengthSeconds);
    }

    if (to <= from)
        return {};

    to = std::min (to, from + longest);
    songSeconds = from;
    std::vector<float> mono (static_cast<size_t> (std::ceil ((to - from) * rate)) + 1, 0.0f);

    for (const auto& clip : clips)
    {
        if (clip.data == nullptr || clip.lengthSeconds <= 0.0)
            continue;

        const auto& samples = clip.data->samples;
        const auto channels = samples.getNumChannels();
        const auto available = samples.getNumSamples();
        const auto inRate = clip.data->playbackRate;

        if (channels == 0 || available == 0 || inRate <= 0.0)
            continue;

        // Mono, with a gentle low-pass when it comes down a long way (most of what would fold
        // back from above the new Nyquist goes).
        const auto ratio = inRate / rate;
        const auto smoothing = ratio > 1.5;

        const auto input = [&] (long long i) -> float
        {
            const auto at = [&] (long long j) -> float
            {
                if (j < 0 || j >= available)
                    return 0.0f;

                float s = 0.0f;

                for (int ch = 0; ch < channels; ++ch)
                    s += samples.getSample (ch, static_cast<int> (j));

                return s / static_cast<float> (channels);
            };

            return smoothing ? 0.5f * (at (i) + at (i + 1)) : at (i);
        };

        const auto firstOut = static_cast<long long> (std::ceil ((clip.startSeconds - from) * rate));
        const auto lastOut = std::min (static_cast<long long> (mono.size()) - 1,
                                       static_cast<long long> (std::floor ((clip.startSeconds - from + clip.lengthSeconds) * rate)));
        const auto offset = clip.offsetSeconds * inRate;

        for (auto j = std::max (0LL, firstOut); j <= lastOut; ++j)
        {
            // Stopped (on its thread): nothing to give.
            if ((j & 0xffff) == 0 && juce::Thread::currentThreadShouldExit())
                return {};

            // Where in the file: the clip's window, from its offset.
            const auto position = offset + ((static_cast<double> (j) / rate) - (clip.startSeconds - from)) * inRate;
            const auto i = static_cast<long long> (std::floor (position));
            const auto f = static_cast<float> (position - static_cast<double> (i));

            // Cubic (Catmull-Rom) between the samples.
            const auto y0 = input (i - 1), y1 = input (i), y2 = input (i + 1), y3 = input (i + 2);
            const auto value = y1 + 0.5f * f * (y2 - y0 + f * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + f * (3.0f * (y1 - y2) + y3 - y0)));
            mono[static_cast<size_t> (j)] += clip.gain * value;
        }
    }

    return mono;
}

//==============================================================================
RecordingAnalysis::Result RecordingAnalysis::analyse (std::vector<float> x, double songSeconds, Grid song,
                                                      double tempoGiven, const Report& report)
{
    Result result;

    const auto fail = [&result] (const juce::String& why)
    {
        result.ok = false;
        result.error = why;
        return result;
    };

    // Louder and quieter recordings alike.
    float peak = 0.0f;

    for (const auto s : x)
        peak = std::max (peak, std::abs (s));

    if (x.size() < static_cast<size_t> (4.0 * rate))
        return fail ("The recording is too short to find its beat: it takes a few bars.");

    if (peak < 1.0e-4f)
        return fail ("The recording is silent.");

    for (auto& s : x)
        s /= peak;

    //==============================================================================
    // 1. Onsets, and where the sound is.
    Onsets onsets;

    if (! findOnsets (x, onsets, report, 0.0, 0.3))
        return fail ("Cancelled");

    const auto frames = static_cast<int> (onsets.strength.size());
    const auto loudest = *std::max_element (onsets.level.begin(), onsets.level.end());
    int firstSound = 0, lastSound = frames - 1;

    while (firstSound < frames - 1 && onsets.level[static_cast<size_t> (firstSound)] < 0.05f * loudest)
        ++firstSound;

    while (lastSound > firstSound && onsets.level[static_cast<size_t> (lastSound)] < 0.05f * loudest)
        --lastSound;

    if ((lastSound - firstSound) / Onsets::fps < 4.0)
        return fail ("The recording is too short to find its beat: it takes a few bars.");

    //==============================================================================
    // 2. The pulse: its period from the onsets' regularity (or the tempo given), then the
    //    steady pulse that meets them best over the whole recording.
    const Metre metre (song);
    const auto smoothed = smooth (onsets.strength, 1.5);
    const auto songPulseBpm = song.bpm / metre.quartersPerPulse;
    double period = 0.0;

    if (tempoGiven > 0.0)
        period = Onsets::fps * 60.0 / (tempoGiven / metre.quartersPerPulse);
    else
        period = pulsePeriod (onsets.strength, smoothed, firstSound, lastSound, 115.0 / metre.quartersPerPulse, metre.quartersPerPulse > 1.01);

    if (period <= 0.0)
        return fail ("No clear beat was found in it.");

    auto pulse = fitPulse (smoothed, period, tempoGiven > 0.0 ? 0.01 : 0.03, firstSound, lastSound);

    if (pulse.score <= 0.0)
        return fail ("No clear beat was found in it.");

    // The song's own beat: a recording made to the click follows it already, at whatever level
    // its pulse was heard (half time, double time...), the player a little early or late throughout.
    const auto songPeriod = Onsets::fps * 60.0 / songPulseBpm;
    const auto songSecondsPerPulse = 60.0 / songPulseBpm;
    const auto songPulseFrame = [&] (double k) { return Onsets::frameAt (song.originSeconds + k * songSecondsPerPulse - songSeconds); };
    double songScore = 0.0, songShift = 0.0;
    double level = 0.0;

    for (const auto candidate : { 1.0, 2.0, 0.5, 3.0, 1.0 / 3.0, 4.0, 0.25, 1.5, 2.0 / 3.0 })
    {
        if (std::abs (pulse.period / (songPeriod * candidate) - 1.0) < 0.03)
        {
            level = candidate;
            break;
        }
    }

    if (level > 0.0 && (tempoGiven <= 0.0 || std::abs (tempoGiven / song.bpm - 1.0) < 0.01))
    {
        const auto levelPeriod = songPeriod * level;

        // At a slower level, its pulses may fall on any of the song's: each is tried.
        for (int start = 0; start < std::max (1, static_cast<int> (std::lround (level))); ++start)
        {
            for (double shift = -0.06 * Onsets::fps; shift <= 0.06 * Onsets::fps; shift += 0.25)
            {
                auto phase = std::fmod (songPulseFrame (start) + shift, levelPeriod);

                if (phase < 0.0)
                    phase += levelPeriod;

                if (const auto score = meet (smoothed, 0.0, levelPeriod, phase, firstSound, lastSound); score > songScore)
                {
                    songScore = score;
                    songShift = shift;
                }
            }
        }
    }

    result.followsSong = level > 0.0 && songScore >= 0.9 * pulse.score;

    if (result.followsSong)
    {
        pulse.period = songPeriod;
        pulse.phase = std::fmod (songPulseFrame (0.0) + songShift, songPeriod);

        if (pulse.phase < 0.0)
            pulse.phase += songPeriod;

        pulse.score = songScore;
    }

    result.bpm = result.followsSong ? song.bpm : Onsets::fps * 60.0 / pulse.period * metre.quartersPerPulse;

    if (! report (0.32))
        return fail ("Cancelled");

    //==============================================================================
    // 3. The notes.
    Notes notes;

    if (! findNotes (x, notes, report, 0.32, 0.9))
        return fail ("Cancelled");

    //==============================================================================
    // 4. The pulses through the recording, and the notes on each.
    const auto secondsPerPulse = pulse.period / Onsets::fps;
    std::vector<double> pulses;   // seconds from the signal's start

    for (auto k = std::ceil ((firstSound - pulse.phase) / pulse.period - 1.0); ; k += 1.0)
    {
        const auto frame = pulse.phase + k * pulse.period;

        if (frame > lastSound + pulse.period)
            break;

        pulses.push_back (Onsets::secondsOf (frame));
    }

    const auto count = static_cast<int> (pulses.size());

    if (count < 2 * metre.pulsesPerBar)
        return fail ("The recording is too short to find its bars.");

    std::vector<std::array<float, 12>> chromaOf (static_cast<size_t> (count)), bassOf (static_cast<size_t> (count));
    std::vector<double> loudnessOf (static_cast<size_t> (count)), lowOf (static_cast<size_t> (count));

    for (int k = 0; k < count; ++k)
    {
        gather (notes, pulses[static_cast<size_t> (k)], pulses[static_cast<size_t> (k)] + secondsPerPulse,
                chromaOf[static_cast<size_t> (k)], bassOf[static_cast<size_t> (k)], loudnessOf[static_cast<size_t> (k)]);

        const auto frame = Onsets::frameAt (pulses[static_cast<size_t> (k)]);
        float hit = 0.0f;

        for (int d = -2; d <= 2; ++d)
            hit = std::max (hit, valueAt (onsets.low, frame + d));

        lowOf[static_cast<size_t> (k)] = hit;
    }

    //==============================================================================
    // 5. The bars: where one chord explains each bar best, the chords change most and the low
    //    end hits hardest (or the song's own bars, for a recording that follows them).
    const auto key = keyOf (notes);
    result.keyTonic = key.tonic;
    result.keyMinor = key.minor;
    int downbeat = 0;   // the pulse a bar starts on

    if (result.followsSong)
    {
        // The pulse nearest a bar line of the song.
        const auto barSeconds = songSecondsPerPulse * metre.pulsesPerBar;
        double nearest = 1.0e9;

        for (int k = 0; k < std::min (count, metre.pulsesPerBar); ++k)
        {
            const auto t = songSeconds + pulses[static_cast<size_t> (k)] - song.originSeconds;
            const auto off = std::abs (t / barSeconds - std::round (t / barSeconds));

            if (off < nearest)
            {
                nearest = off;
                downbeat = k;
            }
        }
    }
    else if (metre.pulsesPerBar > 1)
    {
        std::vector<double> change (static_cast<size_t> (count), 0.0);
        double lowMean = 0.0;

        for (int k = 0; k < count; ++k)
            lowMean += lowOf[static_cast<size_t> (k)];

        lowMean = std::max (1.0e-9, lowMean / count);

        for (int k = 2; k + 1 < count; ++k)
        {
            std::array<float, 12> before {}, after {};

            for (size_t pc = 0; pc < 12; ++pc)
            {
                before[pc] = chromaOf[static_cast<size_t> (k - 2)][pc] + chromaOf[static_cast<size_t> (k - 1)][pc];
                after[pc] = chromaOf[static_cast<size_t> (k)][pc] + chromaOf[static_cast<size_t> (k + 1)][pc];
            }

            change[static_cast<size_t> (k)] = 1.0 - cosine (before, after);
        }

        double bestScore = -1.0e9;

        for (int m = 0; m < metre.pulsesPerBar; ++m)
        {
            double changes = 0.0, hits = 0.0, fits = 0.0;
            int n = 0, bars = 0;

            for (int k = m; k < count; k += metre.pulsesPerBar)
            {
                changes += change[static_cast<size_t> (k)];
                hits += lowOf[static_cast<size_t> (k)] / lowMean;
                ++n;

                // How well one chord explains each bar, when the bars start here.
                if (k + metre.pulsesPerBar <= count)
                {
                    std::array<float, 12> chroma {}, bass {};

                    for (int j = k; j < k + metre.pulsesPerBar; ++j)
                    {
                        for (size_t pc = 0; pc < 12; ++pc)
                        {
                            chroma[pc] += chromaOf[static_cast<size_t> (j)][pc];
                            bass[pc] += bassOf[static_cast<size_t> (j)][pc];
                        }
                    }

                    if (const Heard12 heard (chroma, bass); ! heard.empty)
                    {
                        fits += chordOf (heard, key).score;
                        ++bars;
                    }
                }
            }

            changes /= std::max (1, n);
            hits /= std::max (1, n);
            fits /= std::max (1, bars);
            const auto score = fits + 0.5 * changes + 0.05 * hits;

            if (score > bestScore)
            {
                bestScore = score;
                downbeat = m;
            }
        }
    }

    // The first downbeat: the first bar line with sound in its bar (a short pickup before it is let be).
    const auto soundStarts = Onsets::secondsOf (firstSound);
    const auto soundEnds = Onsets::secondsOf (lastSound);

    while (downbeat + metre.pulsesPerBar < count
           && pulses[static_cast<size_t> (downbeat)] + 0.5 * secondsPerPulse * metre.pulsesPerBar < soundStarts)
        downbeat += metre.pulsesPerBar;

    result.downbeatSeconds = songSeconds + pulses[static_cast<size_t> (downbeat)];

    if (result.followsSong)
        result.firstBar = 1 + song.originBar + static_cast<int> (std::lround ((result.downbeatSeconds - song.originSeconds) / (songSecondsPerPulse * metre.pulsesPerBar)));

    //==============================================================================
    // 6. The chords, a bar at a time; two when the halves clearly differ.
    const auto pulsesPerBar = metre.pulsesPerBar;
    const auto barSeconds = secondsPerPulse * pulsesPerBar;
    std::vector<double> barLoudness;
    std::vector<juce::String> written;

    for (int bar = 0; ; ++bar)
    {
        const auto start = pulses[static_cast<size_t> (downbeat)] + bar * barSeconds;

        if (start > soundEnds - 0.25 * barSeconds)
            break;

        std::array<float, 12> chroma {}, bass {};
        double loudness = 0.0;
        gather (notes, start, start + barSeconds, chroma, bass, loudness);
        barLoudness.push_back (loudness);

        const Heard12 heard (chroma, bass);
        const auto whole = chordOf (heard, key);
        juce::String text = whole.name();

        if (pulsesPerBar >= 2 && whole.root >= 0)
        {
            const auto middle = start + barSeconds * (pulsesPerBar / 2) / pulsesPerBar;
            std::array<float, 12> chromaA {}, bassA {}, chromaB {}, bassB {};
            double unused = 0.0;
            gather (notes, start, middle, chromaA, bassA, unused);
            gather (notes, middle, start + barSeconds, chromaB, bassB, unused);
            const Heard12 heardA (chromaA, bassA), heardB (chromaB, bassB);
            const auto a = chordOf (heardA, key), b = chordOf (heardB, key);

            // Two chords when the halves are explained clearly better by their own chords than
            // by the bar's, or when the bass moves to each half's root.
            const auto bassMoves = lowestOf (bassA) == a.root && lowestOf (bassB) == b.root;
            const auto gainA = a.score - fit (heardA, whole.root, whole.shape, key);
            const auto gainB = b.score - fit (heardB, whole.root, whole.shape, key);

            if (a.root >= 0 && b.root >= 0 && a.root != b.root
                && (gainA + gainB > 0.06 || (bassMoves && gainA + gainB > 0.02)))
                text = a.name() + " " + b.name();
        }

        written.push_back (text);

        if (written.size() >= 2048)
            break;
    }

    // Bars too quiet to hear chords in carry on the chord before.
    if (! barLoudness.empty())
    {
        auto sorted = barLoudness;
        std::sort (sorted.begin(), sorted.end());
        const auto typical = sorted[sorted.size() / 2];

        for (size_t i = 0; i < written.size(); ++i)
            if (barLoudness[i] < 0.05 * typical)
                written[i] = i == 0 ? "N.C." : "%";
    }

    // A bar each, as the session players read them.
    juce::StringArray bars;

    for (const auto& text : written)
        bars.add (text);

    result.chords = bars.joinIntoString (" | ");
    result.bars = bars.size();

    //==============================================================================
    // 7. How steady it is: the pulse found again in stretches of 16 pulses, close to where the
    //    one steady pulse puts it.
    int stretches = 0, steady = 0;

    for (int k = 0; k + 16 <= count; k += 8)
    {
        const auto first = Onsets::frameAt (pulses[static_cast<size_t> (k)]) - 0.5 * pulse.period;
        const auto last = Onsets::frameAt (pulses[static_cast<size_t> (k + 15)]) + 0.5 * pulse.period;
        double bestScore = 0.0, bestShift = 0.0;

        for (double shift = -0.3 * pulse.period; shift <= 0.3 * pulse.period; shift += 0.25)
        {
            const auto score = meet (smoothed, 0.0, pulse.period, pulse.phase + shift, first, last);

            if (score > bestScore)
            {
                bestScore = score;
                bestShift = shift;
            }
        }

        if (bestScore <= 0.0)
            continue;

        ++stretches;

        if (std::abs (bestShift) / Onsets::fps <= 0.05)
            ++steady;
    }

    result.steadiness = stretches > 0 ? static_cast<double> (steady) / stretches : 1.0;
    result.ok = true;
    report (1.0);
    return result;
}

//==============================================================================
std::vector<RecordingAnalysis::Clip> RecordingAnalysis::clipsOf (AudioEngine& engine, int trackId)
{
    std::vector<Clip> clips;
    const auto* track = engine.findTrack (trackId);

    if (track == nullptr || track->kind != Track::Kind::audio)
        return clips;

    const auto& map = engine.getTempoMap();

    for (const auto& clip : engine.getClips (trackId))
    {
        if (! clip.isAudio())
            continue;

        juce::String error;
        auto data = engine.getMedia().get (juce::File (clip.audioFile), engine.getPlaybackRate(), error);

        if (data == nullptr)
            continue;   // a missing file: there's nothing to hear

        Clip c;
        c.data = std::move (data);
        c.startSeconds = map.secondsAt (clip.start);
        c.offsetSeconds = clip.audioOffset;
        c.lengthSeconds = clip.audioLength;
        c.gain = juce::Decibels::decibelsToGain (static_cast<float> (clip.audioGainDb), -100.0f);
        clips.push_back (std::move (c));
    }

    return clips;
}

juce::String RecordingAnalysis::layoutOf (AudioEngine& engine)
{
    juce::String layout;
    layout << juce::JSON::toString (engine.getTempoMap().toVar(), true);

    for (const auto& track : engine.getTracks())
    {
        if (track->kind != Track::Kind::audio)
            continue;

        layout << "|" << track->id;

        for (const auto& clip : track->clips)
            if (clip.isAudio())
                layout << ";" << clip.id << "," << juce::String (clip.start, 6) << "," << juce::String (clip.audioOffset, 6) << ","
                       << juce::String (clip.audioLength, 6) << "," << clip.audioFile;
    }

    return layout;
}

RecordingAnalysis::Grid RecordingAnalysis::gridAt (const TempoMap& map, double ppq)
{
    const auto bar = map.barAt (ppq);
    Grid grid;
    grid.bpm = map.bpmAt (ppq);
    grid.numerator = bar.numerator;
    grid.denominator = bar.denominator;
    grid.originSeconds = map.secondsAt (bar.start);
    grid.originBar = bar.index;
    return grid;
}

RecordingAnalysis::Alignment RecordingAnalysis::alignmentFor (const Result& heard, AudioEngine& engine)
{
    const auto state = engine.getTransport().getState();
    const auto& map = engine.getTempoMap();

    // The tempo as the song shows it, to a hundredth. The song takes it all through; its bars
    // (the time signatures) stay as they are.
    Alignment alignment;
    alignment.bpm = std::round (juce::jlimit (20.0, 300.0, heard.bpm > 0.0 ? heard.bpm : state.bpm) * 100.0) / 100.0;
    const auto quartersPerSecond = alignment.bpm / 60.0;

    // Where the song's audio starts now, at the earliest: none of it may move before the start.
    auto earliest = heard.downbeatSeconds;

    for (const auto& track : engine.getTracks())
        if (track->kind == Track::Kind::audio)
            for (const auto& clip : track->clips)
                if (clip.isAudio())
                    earliest = std::min (earliest, map.secondsAt (clip.start));

    // The bar line nearest the downbeat, at the new tempo.
    auto bar = map.barAt (map.nearestBar (heard.downbeatSeconds * quartersPerSecond)).index;
    alignment.shiftSeconds = map.ppqOfBar (bar) / quartersPerSecond - heard.downbeatSeconds;

    while (earliest + alignment.shiftSeconds < -1.0e-9 && bar < TempoMap::maxBar)
    {
        ++bar;
        alignment.shiftSeconds = map.ppqOfBar (bar) / quartersPerSecond - heard.downbeatSeconds;
    }

    alignment.startBar = 1 + bar;
    return alignment;
}

void RecordingAnalysis::align (AudioEngine& engine, const Alignment& alignment)
{
    const auto map = engine.getTempoMap();
    const auto quartersPerSecond = alignment.bpm / 60.0;
    std::vector<int> audioTracks;

    for (const auto& track : engine.getTracks())
        if (track->kind == Track::Kind::audio)
            audioTracks.push_back (track->id);

    // Every clip of audio where it plays now in seconds, moved alike, then in the new tempo's quarters.
    for (const auto id : audioTracks)
    {
        auto clips = engine.getClips (id);

        for (auto& clip : clips)
        {
            if (! clip.isAudio())
                continue;

            clip.start = std::max (0.0, (map.secondsAt (clip.start) + alignment.shiftSeconds) * quartersPerSecond);
            clip.length = clip.audioLength * quartersPerSecond;
        }

        engine.setClips (id, std::move (clips));
    }

    engine.setTempoMap (map.withOneTempo (alignment.bpm));
}

RecordingAnalysis::Result RecordingAnalysis::listen (AudioEngine& engine, int trackId, double tempo)
{
    double songSeconds = 0.0;
    auto mono = mixDown (clipsOf (engine, trackId), songSeconds);

    if (mono.empty())
    {
        Result result;
        result.error = "That track has no recording to listen to.";
        return result;
    }

    return analyse (std::move (mono), songSeconds, gridAt (engine.getTempoMap(), engine.getTempoMap().ppqAt (songSeconds)), tempo, [] (double) { return true; });
}
