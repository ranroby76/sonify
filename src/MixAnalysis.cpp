// D:\workspace\Fanan_web_hub\src\MixAnalysis.cpp

#include "MixAnalysis.h"
#include "SonifyDsp.h"

#include <algorithm>
#include <cmath>
#include <thread>

namespace MixAnalysis
{
namespace
{
    constexpr double frameSeconds = 0.1;     // the masking's and the activity's step
    constexpr float silentDb = -60.0f;       // a frame quieter than this (broadband RMS) is a rest
    constexpr float bandFloorDb = -50.0f;    // a band quieter than this in a frame isn't fighting anything
    constexpr float closeDb = 6.0f;          // two bands within this of each other fight
    constexpr int maxPairs = 10;

    double round1 (double value)   { return std::round (value * 10.0) / 10.0; }
    float toDb (double meanSquare) { return meanSquare > 1.0e-14 ? static_cast<float> (10.0 * std::log10 (meanSquare)) : -140.0f; }

    struct Measured
    {
        Input input;
        juce::String error;
        double lufs = -200.0, shortTermMax = -200.0, peakDb = -140.0, truePeakDb = -140.0, activeShare = 0.0;
        double correlation = 1.0, sideDb = -140.0;
        std::array<float, numBands> bands {};
        std::vector<std::array<float, numBands>> frames;   // each band's level, frame by frame (dBFS)
        std::vector<bool> active;                          // the frames it plays in
        bool ok = false;
    };

    void measure (Measured& m, const std::atomic<bool>& cancelled, std::atomic<juce::int64>& doneSamples)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (m.input.file));

        if (reader == nullptr)
        {
            m.error = "Couldn't read " + m.input.file.getFileName() + " (Sonify reads WAV, AIFF, FLAC, Ogg and MP3).";
            return;
        }

        const auto rate = reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;
        const bool mix = m.input.role != "stem";

        SonifyDsp::LoudnessMeter loudness;
        loudness.prepare (rate);
        SonifyDsp::TruePeakMeter truePeak;
        truePeak.reset();

        std::array<SonifyDsp::Biquad, numBands> filters {};
        std::array<SonifyDsp::BiquadState, numBands> states {};

        for (int band = 0; band < numBands; ++band)
            filters[static_cast<size_t> (band)] = SonifyDsp::Biquad::bandPass (rate, bandCentres()[static_cast<size_t> (band)], 1.41);

        const auto frameLength = juce::jmax (1, static_cast<int> (std::lround (rate * frameSeconds)));
        std::array<double, numBands> frameBand {}, totalBand {};
        double frameAll = 0.0;
        int frameCount = 0;
        juce::int64 activeSamples = 0, totalFrames = 0;
        double sumLL = 0.0, sumRR = 0.0, sumLR = 0.0, sumMid = 0.0, sumSide = 0.0;
        float peak = 0.0f;

        constexpr int blockSize = 8192;
        juce::AudioBuffer<float> buffer (2, blockSize);
        const auto length = reader->lengthInSamples;

        for (juce::int64 at = 0; at < length; at += blockSize)
        {
            if (cancelled.load())
                return;

            const auto n = static_cast<int> (juce::jmin<juce::int64> (blockSize, length - at));
            buffer.clear();
            reader->read (&buffer, 0, n, at, true, true);
            const auto* left = buffer.getReadPointer (0);
            const auto* right = reader->numChannels > 1 ? buffer.getReadPointer (1) : left;

            loudness.process (left, right, n);

            if (mix || m.input.role == "reference")
                truePeak.process (left, right, n);

            for (int i = 0; i < n; ++i)
            {
                const auto l = static_cast<double> (left[i]), r = static_cast<double> (right[i]);
                peak = juce::jmax (peak, std::abs (left[i]), std::abs (right[i]));
                const auto mono = 0.5 * (l + r);
                frameAll += mono * mono;

                for (size_t band = 0; band < numBands; ++band)
                {
                    const auto y = states[band].processDouble (filters[band], mono);
                    frameBand[band] += y * y;
                }

                // The stereo, where it plays.
                sumLL += l * l;
                sumRR += r * r;
                sumLR += l * r;
                const auto side = 0.5 * (l - r);
                sumMid += mono * mono;
                sumSide += side * side;

                if (++frameCount < frameLength)
                    continue;

                // A frame: its bands, and whether it plays.
                const auto playing = toDb (frameAll / frameLength) > silentDb;
                std::array<float, numBands> levels {};

                for (size_t band = 0; band < numBands; ++band)
                {
                    levels[band] = toDb (frameBand[band] / frameLength);

                    if (playing)
                        totalBand[band] += frameBand[band];

                    frameBand[band] = 0.0;
                }

                m.frames.push_back (levels);
                m.active.push_back (playing);
                ++totalFrames;

                if (playing)
                    activeSamples += frameLength;

                frameAll = 0.0;
                frameCount = 0;
            }

            doneSamples.fetch_add (n);
        }

        m.lufs = loudness.getIntegrated();

        // The loudest short-term loudness (3 s: thirty 100 ms steps).
        const auto& steps = loudness.getSteps();
        double sum = 0.0;

        for (size_t i = 0; i < steps.size(); ++i)
        {
            sum += steps[i];

            if (i >= 30)
                sum -= steps[i - 30];

            if (i >= 29)
                m.shortTermMax = juce::jmax (m.shortTermMax, SonifyDsp::LoudnessMeter::loudnessOf (sum / 30.0));
        }

        if (steps.size() < 30 && ! steps.empty())
            m.shortTermMax = SonifyDsp::LoudnessMeter::loudnessOf (sum / static_cast<double> (steps.size()));

        m.peakDb = peak > 1.0e-7f ? 20.0 * std::log10 (peak) : -140.0;
        m.truePeakDb = (mix || m.input.role == "reference") ? truePeak.getPeakDb() : m.peakDb;
        m.activeShare = totalFrames > 0 ? static_cast<double> (std::count (m.active.begin(), m.active.end(), true)) / static_cast<double> (totalFrames) : 0.0;

        for (size_t band = 0; band < numBands; ++band)
            m.bands[band] = activeSamples > 0 ? toDb (totalBand[band] / static_cast<double> (activeSamples)) : -140.0f;

        m.correlation = sumLL > 1.0e-12 && sumRR > 1.0e-12 ? sumLR / std::sqrt (sumLL * sumRR) : 1.0;
        m.sideDb = sumMid > 1.0e-12 ? (sumSide > 1.0e-14 ? 10.0 * std::log10 (sumSide / sumMid) : -140.0) : -140.0;
        m.ok = true;
    }

    juce::var toVar (const Measured& m)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("name", m.input.name);

        if (m.input.trackId != 0)
            object->setProperty ("track_id", m.input.trackId);

        if (! m.ok)
        {
            object->setProperty ("error", m.error);
            return juce::var (object);
        }

        const bool silent = m.lufs <= -70.0;
        object->setProperty ("lufs", silent ? juce::var ("silent") : juce::var (round1 (m.lufs)));
        object->setProperty ("short_term_max_lufs", round1 (juce::jmax (-100.0, m.shortTermMax)));
        object->setProperty ("peak_db", round1 (juce::jmax (-100.0, m.peakDb)));

        if (m.input.role != "stem")
            object->setProperty ("true_peak_dbtp", round1 (juce::jmax (-100.0, m.truePeakDb)));

        object->setProperty ("crest_db", silent ? juce::var() : juce::var (round1 (m.peakDb - m.lufs)));
        object->setProperty ("plays_percent", juce::roundToInt (m.activeShare * 100.0));

        juce::Array<juce::var> bands;

        for (const auto level : m.bands)
            bands.add (round1 (juce::jmax (-100.0, static_cast<double> (level))));

        object->setProperty ("bands_dbfs", bands);
        object->setProperty ("correlation", std::round (m.correlation * 100.0) / 100.0);
        object->setProperty ("side_vs_mid_db", round1 (juce::jmax (-100.0, m.sideDb)));
        return juce::var (object);
    }

    juce::String bandName (int band)
    {
        const auto hz = bandCentres()[static_cast<size_t> (band)];
        return hz >= 1000.0 ? juce::String (hz / 1000.0, hz >= 10000.0 ? 0 : 1).trimCharactersAtEnd (".0") + " kHz"
                            : juce::String (juce::roundToInt (hz)) + " Hz";
    }

    /** Where stems fight: for each pair and octave, the share of the frames both play in where
        both are loud in it and within 6 dB of each other; only octaves that matter to both (within
        15 dB of each one's loudest). The worst first. */
    juce::var masking (const std::vector<Measured>& stems)
    {
        struct Pair { size_t a, b; int band; double share; float levelA, levelB; };
        std::vector<Pair> pairs;

        for (size_t a = 0; a < stems.size(); ++a)
        {
            for (size_t b = a + 1; b < stems.size(); ++b)
            {
                const auto& x = stems[a];
                const auto& y = stems[b];

                if (! x.ok || ! y.ok || x.lufs <= -70.0 || y.lufs <= -70.0)
                    continue;

                const auto frames = juce::jmin (x.frames.size(), y.frames.size());
                const auto loudestX = *std::max_element (x.bands.begin(), x.bands.end());
                const auto loudestY = *std::max_element (y.bands.begin(), y.bands.end());

                for (int band = 0; band < numBands; ++band)
                {
                    const auto k = static_cast<size_t> (band);

                    if (x.bands[k] < loudestX - 15.0f || y.bands[k] < loudestY - 15.0f)
                        continue;

                    int both = 0, fighting = 0;
                    double sumX = 0.0, sumY = 0.0;

                    for (size_t f = 0; f < frames; ++f)
                    {
                        if (! x.active[f] || ! y.active[f])
                            continue;

                        ++both;
                        const auto lx = x.frames[f][k], ly = y.frames[f][k];

                        if (lx > bandFloorDb && ly > bandFloorDb && std::abs (lx - ly) < closeDb)
                        {
                            ++fighting;
                            sumX += lx;
                            sumY += ly;
                        }
                    }

                    if (both < 20 || fighting == 0)
                        continue;

                    const auto share = static_cast<double> (fighting) / static_cast<double> (both);

                    if (share >= 0.25)
                        pairs.push_back ({ a, b, band, share, static_cast<float> (sumX / fighting), static_cast<float> (sumY / fighting) });
                }
            }
        }

        // The worst first; two octaves a pair at most (the two it fights most in, the louder first
        // when even), so one pair can't fill the list.
        std::sort (pairs.begin(), pairs.end(), [] (const Pair& p, const Pair& q)
        {
            if (std::abs (p.share - q.share) > 0.02)
                return p.share > q.share;

            return juce::jmin (p.levelA, p.levelB) > juce::jmin (q.levelA, q.levelB);
        });

        std::vector<Pair> kept;

        for (const auto& p : pairs)
        {
            const auto same = std::count_if (kept.begin(), kept.end(), [&p] (const Pair& k) { return k.a == p.a && k.b == p.b; });

            if (same < 2)
                kept.push_back (p);
        }

        juce::Array<juce::var> list;

        for (size_t i = 0; i < kept.size() && i < static_cast<size_t> (maxPairs); ++i)
        {
            const auto& p = kept[i];
            auto* object = new juce::DynamicObject();
            object->setProperty ("a", stems[p.a].input.name);
            object->setProperty ("a_track_id", stems[p.a].input.trackId);
            object->setProperty ("b", stems[p.b].input.name);
            object->setProperty ("b_track_id", stems[p.b].input.trackId);
            object->setProperty ("octave", bandName (p.band));
            object->setProperty ("fighting_percent", juce::roundToInt (p.share * 100.0));
            object->setProperty ("a_level_dbfs", round1 (p.levelA));
            object->setProperty ("b_level_dbfs", round1 (p.levelB));
            list.add (juce::var (object));
        }

        return list;
    }
}

//==============================================================================
const std::array<double, numBands>& bandCentres()
{
    static const std::array<double, numBands> centres { 31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 };
    return centres;
}

juce::var analyse (const std::vector<Input>& inputs, const std::atomic<bool>& cancelled, const std::function<void (double)>& progress)
{
    std::vector<Measured> measured (inputs.size());
    juce::int64 totalSamples = 0;

    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        for (size_t i = 0; i < inputs.size(); ++i)
        {
            measured[i].input = inputs[i];

            if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (inputs[i].file)); reader != nullptr)
                totalSamples += reader->lengthInSamples;
        }
    }

    // On several threads, a file at a time each.
    std::atomic<size_t> next { 0 }, finished { 0 };
    std::atomic<juce::int64> doneSamples { 0 };
    const auto threads = static_cast<size_t> (juce::jlimit (1, 8, static_cast<int> (std::thread::hardware_concurrency()) - 1));
    std::vector<std::thread> workers;

    for (size_t t = 0; t < juce::jmin (threads, measured.size()); ++t)
    {
        workers.emplace_back ([&]
        {
            for (auto i = next.fetch_add (1); i < measured.size(); i = next.fetch_add (1))
            {
                measure (measured[i], cancelled, doneSamples);
                finished.fetch_add (1);
            }
        });
    }

    // Progress from here, while they work.
    while (progress && finished.load() < measured.size() && ! cancelled.load())
    {
        progress (totalSamples > 0 ? juce::jmin (1.0, static_cast<double> (doneSamples.load()) / static_cast<double> (totalSamples)) : 0.0);
        juce::Thread::sleep (100);
    }

    for (auto& worker : workers)
        worker.join();

    auto* result = new juce::DynamicObject();
    juce::var answer (result);

    if (cancelled.load())
    {
        result->setProperty ("error", "Cancelled");
        return answer;
    }

    std::vector<Measured> stems;
    juce::Array<juce::var> stemList;

    for (auto& m : measured)
    {
        if (m.input.role == "stem")
        {
            stemList.add (toVar (m));
            stems.push_back (std::move (m));
        }
        else
        {
            result->setProperty (m.input.role, toVar (m));
        }
    }

    if (! stems.empty())
    {
        result->setProperty ("stems", stemList);
        result->setProperty ("masking", masking (stems));
    }

    juce::Array<juce::var> bandList;

    for (int band = 0; band < numBands; ++band)
        bandList.add (bandName (band));

    result->setProperty ("octaves", bandList);

    if (progress)
        progress (1.0);

    return answer;
}

juce::String toText (const juce::var& report)
{
    return juce::JSON::toString (report, juce::JSON::FormatOptions{}.withSpacing (juce::JSON::Spacing::singleLine).withMaxDecimalPlaces (2));
}

juce::var headline (const juce::var& measured)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("lufs", measured.getProperty ("lufs", {}));
    object->setProperty ("truePeak", measured.hasProperty ("true_peak_dbtp") ? measured.getProperty ("true_peak_dbtp", {}) : measured.getProperty ("peak_db", {}));
    return juce::var (object);
}
}
