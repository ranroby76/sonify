// D:\workspace\Fanan_web_hub\src\LoopFinder.cpp

#include "LoopFinder.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>

namespace
{
    /** The pitch period (samples) at a point: Sampler Arena's autocorrelation, or -1. */
    double detectPitch (const float* data, juce::int64 totalSamples, juce::int64 centre, double sampleRate)
    {
        constexpr int windowSize = 4096;
        auto winStart = centre - windowSize / 2;

        if (winStart < 0)
            winStart = 0;

        if (winStart + windowSize >= totalSamples)
            winStart = totalSamples - windowSize - 1;

        if (winStart < 0)
            return -1.0;

        const auto* win = data + winStart;
        auto minLag = static_cast<int> (sampleRate / 4000.0);
        auto maxLag = static_cast<int> (sampleRate / 20.0);

        if (maxLag > windowSize / 2)
            maxLag = windowSize / 2;

        if (minLag < 2)
            minLag = 2;

        float energy = 0.0f;

        for (int i = 0; i < windowSize; ++i)
            energy += win[i] * win[i];

        if (energy < 1.0e-10f)
            return -1.0;

        const auto correlationAt = [win] (int lag)
        {
            float sum = 0.0f, e1 = 0.0f, e2 = 0.0f;
            const auto count = windowSize - lag;

            for (int i = 0; i < count; ++i)
            {
                sum += win[i] * win[i + lag];
                e1 += win[i] * win[i];
                e2 += win[i + lag] * win[i + lag];
            }

            const auto denominator = std::sqrt (e1 * e2);
            return denominator > 1.0e-10f ? sum / denominator : 0.0f;
        };

        // The first strong peak after a dip: the fundamental, not a harmonic.
        float bestCorrelation = 0.0f;
        int bestLag = -1;
        bool foundDip = false;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            const auto correlation = correlationAt (lag);

            if (! foundDip && correlation < 0.5f)
                foundDip = true;

            if (foundDip && correlation > bestCorrelation && correlation > 0.7f)
            {
                bestCorrelation = correlation;
                bestLag = lag;
            }
            else if (bestLag > 0 && bestCorrelation > 0.9f && correlation < bestCorrelation)
            {
                // Past the top of the fundamental's peak. (Sampler Arena stopped at the first lag
                // over 0.9, on the way up: a clean tone came out several percent sharp.)
                break;
            }
        }

        if (bestLag <= 0 || bestCorrelation < 0.7f)
            return -1.0;

        if (bestLag > minLag && bestLag < maxLag)
        {
            const auto before = correlationAt (bestLag - 1);
            const auto after = correlationAt (bestLag + 1);
            const auto denominator = before - 2.0f * bestCorrelation + after;

            if (std::abs (denominator) > 1.0e-10f)
                return static_cast<double> (bestLag) + static_cast<double> (juce::jlimit (-0.5f, 0.5f, 0.5f * (before - after) / denominator));
        }

        return static_cast<double> (bestLag);
    }

    struct Score
    {
        float total = 0.0f, correlation = 0.0f, levelMatch = 0.0f, valueMatch = 0.0f, slopeMatch = 0.0f;
    };

    /** How well a loop's end matches its start: Sampler Arena's scoreLoopPoint. */
    Score scoreLoopPoint (const float* data, juce::int64 loopStart, juce::int64 loopEnd, double period, juce::int64 totalSamples)
    {
        Score score;
        const auto window = juce::jmax (256, static_cast<int> (period * 4));

        if (loopStart + window >= totalSamples || loopEnd + window >= totalSamples || loopEnd <= loopStart)
            return score;

        float sumAB = 0.0f, sumAA = 0.0f, sumBB = 0.0f;

        for (int i = 0; i < window; ++i)
        {
            const auto a = data[loopStart + i];
            const auto b = data[loopEnd + i];
            sumAB += a * b;
            sumAA += a * a;
            sumBB += b * b;
        }

        const auto denominator = std::sqrt (sumAA * sumBB);
        score.correlation = denominator > 1.0e-10f ? sumAB / denominator : 0.0f;

        if (score.correlation < 0.5f)
            return score;

        // The two ends at much the same level (within about 2:1): an end in the decay is no loop.
        const auto rmsStart = std::sqrt (sumAA / static_cast<float> (window));
        const auto rmsEnd = std::sqrt (sumBB / static_cast<float> (window));
        score.levelMatch = rmsStart > 1.0e-9f && rmsEnd > 1.0e-9f ? juce::jmin (rmsStart, rmsEnd) / juce::jmax (rmsStart, rmsEnd) : 0.0f;

        if (score.levelMatch < 0.45f)
            return score;

        float peak = 0.001f;

        for (int i = -10; i <= 10; ++i)
            peak = juce::jmax (peak, std::abs (data[juce::jlimit<juce::int64> (0, totalSamples - 1, loopStart + i)]));

        score.valueMatch = 1.0f - juce::jmin (1.0f, std::abs (data[loopEnd] - data[loopStart]) / peak);

        const auto slopeStart = loopStart + 1 < totalSamples ? data[loopStart + 1] - data[loopStart] : 0.0f;
        const auto slopeEnd = loopEnd + 1 < totalSamples ? data[loopEnd + 1] - data[loopEnd] : 0.0f;
        score.slopeMatch = 1.0f - juce::jmin (1.0f, std::abs (slopeEnd - slopeStart) / juce::jmax (0.001f, peak));

        score.total = score.correlation * 0.5f + score.valueMatch * 0.2f + score.slopeMatch * 0.1f + score.levelMatch * 0.2f;
        return score;
    }

    std::vector<float> monoOf (const juce::AudioBuffer<float>& audio)
    {
        const auto total = audio.getNumSamples();
        const auto channels = juce::jmax (1, audio.getNumChannels());
        std::vector<float> mono (static_cast<size_t> (total), 0.0f);

        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        {
            const auto* data = audio.getReadPointer (channel);

            for (int i = 0; i < total; ++i)
                mono[static_cast<size_t> (i)] += data[i];
        }

        for (auto& value : mono)
            value /= static_cast<float> (channels);

        return mono;
    }

    /** The share of a window's energy in its first differences: how bright it is. */
    float brightness (const float* data, juce::int64 at, int length, juce::int64 total)
    {
        double energy = 0.0, edges = 0.0;

        for (int i = 1; i < length && at + i < total; ++i)
        {
            const auto value = static_cast<double> (data[at + i]);
            const auto change = value - static_cast<double> (data[at + i - 1]);
            energy += value * value;
            edges += change * change;
        }

        return energy > 1.0e-12 ? static_cast<float> (edges / energy) : 0.0f;
    }

    /** The extra numbers Fanan AI weighs. */
    void addDetails (LoopFinder::Candidate& c, const float* mono, juce::int64 total, double rate, double period)
    {
        c.seconds = static_cast<double> (c.loopEnd - c.loopStart) / rate;
        c.periods = period > 0.0 ? static_cast<double> (c.loopEnd - c.loopStart) / period : 0.0;
        c.startSeconds = static_cast<double> (c.loopStart) / rate;
        c.endSeconds = static_cast<double> (c.loopEnd) / rate;

        const auto startPeriod = detectPitch (mono, total, c.loopStart + 2048, rate);
        const auto endPeriod = detectPitch (mono, total, c.loopEnd - 2048, rate);
        c.startPitchHz = startPeriod > 0.0 ? rate / startPeriod : 0.0;
        c.endPitchHz = endPeriod > 0.0 ? rate / endPeriod : 0.0;

        // The level inside the loop: 30 ms windows, from the quietest to the loudest.
        const auto window = juce::jmax<juce::int64> (64, static_cast<juce::int64> (rate * 0.030));
        float quietest = 1.0e9f, loudest = 0.0f;

        for (auto at = c.loopStart; at + window <= c.loopEnd; at += window / 2)
        {
            double sum = 0.0;

            for (juce::int64 i = 0; i < window; ++i)
                sum += static_cast<double> (mono[at + i]) * static_cast<double> (mono[at + i]);

            const auto rms = static_cast<float> (std::sqrt (sum / static_cast<double> (window)));
            quietest = juce::jmin (quietest, rms);
            loudest = juce::jmax (loudest, rms);
        }

        c.levelWobbleDb = loudest > 0.0f && quietest < 1.0e8f
                        ? juce::Decibels::gainToDecibels (loudest, -100.0f) - juce::Decibels::gainToDecibels (juce::jmax (quietest, 1.0e-6f), -100.0f)
                        : 0.0f;

        const auto length = static_cast<int> (juce::jmax (512.0, period * 8.0));
        const auto a = brightness (mono, c.loopStart, length, total);
        const auto b = brightness (mono, juce::jmax<juce::int64> (0, c.loopEnd - length), length, total);
        c.brightnessMatch = a > 0.0f && b > 0.0f ? juce::jmin (a, b) / juce::jmax (a, b) : 0.0f;
    }
}

//==============================================================================
LoopFinder::Result LoopFinder::analyze (const juce::File& file, int maxCandidates, int crossfadeMs)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        Result result;
        result.error = "Could not read file: " + file.getFileName();
        return result;
    }

    if (reader->lengthInSamples < 8192 || reader->lengthInSamples > static_cast<juce::int64> (reader->sampleRate * 600.0))
    {
        Result result;
        result.error = reader->lengthInSamples < 8192 ? "File too short for loop analysis" : "File too long for loop analysis";
        return result;
    }

    juce::AudioBuffer<float> audio (static_cast<int> (juce::jlimit (1u, 2u, reader->numChannels)), static_cast<int> (reader->lengthInSamples));
    reader->read (&audio, 0, static_cast<int> (reader->lengthInSamples), 0, true, audio.getNumChannels() > 1);
    return analyze (audio, reader->sampleRate, maxCandidates, crossfadeMs);
}

LoopFinder::Result LoopFinder::analyze (const juce::AudioBuffer<float>& audio, double sampleRate, int maxCandidates, int crossfadeMs)
{
    Result result;
    result.sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    result.totalSamples = audio.getNumSamples();

    if (result.totalSamples < 8192)
    {
        result.error = "File too short for loop analysis";
        return result;
    }

    const auto monoData = monoOf (audio);
    const auto* mono = monoData.data();
    const auto total = result.totalSamples;
    const auto rate = result.sampleRate;

    // The sustain: the level (30 ms windows) over 5% of its loudest, after the first 100 ms.
    const auto windowSize = static_cast<int> (rate * 0.030);
    const auto hop = juce::jmax (1, windowSize / 2);
    std::vector<float> envelope;

    for (juce::int64 at = 0; at + windowSize < total; at += hop)
    {
        float sum = 0.0f;

        for (int j = 0; j < windowSize; ++j)
            sum += mono[at + j] * mono[at + j];

        envelope.push_back (std::sqrt (sum / static_cast<float> (windowSize)));
    }

    if (envelope.size() < 20)
    {
        result.error = "Audio too short";
        return result;
    }

    const auto peak = *std::max_element (envelope.begin(), envelope.end());
    result.peakLevelDb = juce::Decibels::gainToDecibels (peak, -100.0f);

    if (peak < 0.001f)
    {
        result.error = "Audio too quiet";
        return result;
    }

    const auto quiet = peak * 0.05f;
    const auto minAttack = static_cast<juce::int64> (rate * 0.100);
    size_t sustainStartIndex = 0;

    for (size_t i = 0; i < envelope.size(); ++i)
    {
        if (static_cast<juce::int64> (i) * hop >= minAttack && envelope[i] > quiet)
        {
            sustainStartIndex = i;
            break;
        }
    }

    auto sustainEndIndex = envelope.size() - 1;

    for (auto i = envelope.size() - 1; i > sustainStartIndex; --i)
    {
        if (envelope[i] > quiet)
        {
            sustainEndIndex = i;
            break;
        }
    }

    result.sustainStart = static_cast<juce::int64> (sustainStartIndex) * hop;
    result.sustainEnd = static_cast<juce::int64> (sustainEndIndex) * hop;
    auto sustainLength = result.sustainEnd - result.sustainStart;

    if (sustainLength < 4096)
    {
        result.sustainStart = total / 10;
        result.sustainEnd = total * 9 / 10;
        sustainLength = result.sustainEnd - result.sustainStart;
    }

    // The body's end: the last point still at half the loudest (loops stay in the full sound).
    auto bodyEnd = result.sustainEnd;

    for (auto i = envelope.size(); i-- > 0;)
    {
        if (envelope[i] > peak * 0.5f)
        {
            bodyEnd = static_cast<juce::int64> (i) * hop;
            break;
        }
    }

    bodyEnd = juce::jlimit (result.sustainStart, result.sustainEnd, bodyEnd);

    if (bodyEnd <= result.sustainStart + static_cast<juce::int64> (rate * 0.3))
        bodyEnd = result.sustainEnd;

    result.bodyEnd = bodyEnd;

    // The pitch: in the middle of the sustain, else at a third, else at two thirds.
    auto period = detectPitch (mono, total, result.sustainStart + sustainLength / 2, rate);

    if (period <= 0.0)
        period = detectPitch (mono, total, result.sustainStart + sustainLength / 3, rate);

    if (period <= 0.0)
        period = detectPitch (mono, total, result.sustainStart + sustainLength * 2 / 3, rate);

    if (period <= 0.0)
    {
        result.error = juce::String::fromUTF8 ("Could not detect pitch \xe2\x80\x94 sample may be noise or unpitched");
        return result;
    }

    result.periodSamples = period;
    result.pitchHz = rate / period;
    const auto crossfadeSamples = static_cast<int> (rate * crossfadeMs / 1000.0);

    // Loop starts: rising zero crossings in the first half of the sustain (past its first 4
    // periods), about 50 of them.
    auto minLength = static_cast<juce::int64> (period * 8);

    if (minLength < static_cast<juce::int64> (rate * 0.5))
        minLength = static_cast<juce::int64> (rate * 0.5);

    std::vector<juce::int64> crossings;
    const auto searchStart = result.sustainStart + static_cast<juce::int64> (period * 4);
    const auto searchEnd = result.sustainStart + sustainLength / 2;

    for (auto i = searchStart; i < searchEnd - 1; ++i)
        if (mono[i] <= 0.0f && mono[i + 1] > 0.0f)
            crossings.push_back (i);

    if (crossings.empty())
        for (auto i = searchStart; i < searchEnd; i += static_cast<juce::int64> (period))
            crossings.push_back (i);

    if (crossings.size() > 50)
    {
        const auto step = crossings.size() / 50;
        std::vector<juce::int64> thinned;

        for (size_t i = 0; i < crossings.size(); i += step)
            thinned.push_back (crossings[i]);

        crossings = std::move (thinned);
    }

    struct Scored
    {
        juce::int64 start, end;
        float score, lateness;
        Score parts;
    };

    std::vector<Scored> scored;

    for (const auto loopStart : crossings)
    {
        const auto idealEnd = bodyEnd - static_cast<juce::int64> (period * 2);

        if (idealEnd <= loopStart + minLength)
            continue;

        const auto n = static_cast<int> (static_cast<double> (idealEnd - loopStart) / period);

        if (n < 8)
            continue;

        for (int offset = 0; offset >= -2; --offset)
        {
            const auto tryN = n + offset;

            if (tryN < 8)
                continue;

            const auto rawEnd = loopStart + static_cast<juce::int64> (static_cast<double> (tryN) * period + 0.5);
            auto bestEnd = rawEnd;
            Score best;
            best.total = -1.0f;
            const auto range = static_cast<int> (period * 0.25);

            for (int nudge = -range; nudge <= range; ++nudge)
            {
                const auto testEnd = rawEnd + nudge;

                if (testEnd < loopStart + minLength || testEnd >= total - 1)
                    continue;

                const auto score = scoreLoopPoint (mono, loopStart, testEnd, period, total);

                if (score.total > best.total)
                {
                    best = score;
                    bestEnd = testEnd;
                }
            }

            if (best.total < 0.5f)
                continue;

            // Later is better: nearer the end of the strong body.
            const auto span = static_cast<float> (bodyEnd - result.sustainStart);
            const auto lateness = span > 0.0f ? 0.5f + 0.5f * static_cast<float> (bestEnd - result.sustainStart) / span : 1.0f;
            scored.push_back ({ loopStart, bestEnd, best.total * lateness, lateness, best });
        }
    }

    if (scored.empty())
    {
        result.error = "No period-aligned loop points found";
        return result;
    }

    std::sort (scored.begin(), scored.end(), [] (const Scored& a, const Scored& b) { return a.score > b.score; });

    // Within 2 periods of a better one: the same loop.
    const auto minSeparation = static_cast<juce::int64> (period * 2);
    std::vector<Scored> kept;

    for (const auto& candidate : scored)
    {
        const auto tooClose = std::any_of (kept.begin(), kept.end(), [&] (const Scored& other)
        {
            return std::abs (candidate.start - other.start) < minSeparation && std::abs (candidate.end - other.end) < minSeparation;
        });

        if (! tooClose)
            kept.push_back (candidate);

        if (static_cast<int> (kept.size()) >= juce::jmax (1, maxCandidates))
            break;
    }

    for (const auto& k : kept)
    {
        Candidate c;
        c.loopStart = k.start;
        c.loopEnd = k.end;
        c.score = k.score;
        c.crossfadeSamples = crossfadeSamples;
        c.correlation = k.parts.correlation;
        c.levelMatch = k.parts.levelMatch;
        c.valueMatch = k.parts.valueMatch;
        c.slopeMatch = k.parts.slopeMatch;
        c.lateness = k.lateness;
        addDetails (c, mono, total, rate, period);
        result.candidates.push_back (c);
    }

    result.success = true;
    return result;
}

LoopFinder::Candidate LoopFinder::describe (const juce::AudioBuffer<float>& audio, double sampleRate, juce::int64 loopStart, juce::int64 loopEnd,
                                            double periodSamples)
{
    Candidate c;
    c.loopStart = loopStart;
    c.loopEnd = loopEnd;
    const auto monoData = monoOf (audio);
    const auto total = static_cast<juce::int64> (monoData.size());

    if (loopStart < 0 || loopEnd <= loopStart || loopEnd >= total)
        return c;

    const auto rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    auto period = periodSamples;

    if (period <= 0.0)
        period = detectPitch (monoData.data(), total, (loopStart + loopEnd) / 2, rate);

    const auto parts = scoreLoopPoint (monoData.data(), loopStart, loopEnd, juce::jmax (1.0, period), total);
    c.correlation = parts.correlation;
    c.levelMatch = parts.levelMatch;
    c.valueMatch = parts.valueMatch;
    c.slopeMatch = parts.slopeMatch;
    c.score = parts.total;
    addDetails (c, monoData.data(), total, rate, period);
    return c;
}
