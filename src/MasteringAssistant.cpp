// D:\workspace\Fanan_web_hub\src\MasteringAssistant.cpp

#include "MasteringAssistant.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    using namespace SonifyDsp;

    constexpr std::array<double, 10> bandCentres { 31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0 };

    /** Energy in octave bands of the mid signal, every 100 ms step (as the loudness meter's). */
    class BandMeter
    {
    public:
        BandMeter (double sampleRate, int stepSamples)
            : stepLength (stepSamples)
        {
            // Two of the same band-pass in a row: about two thirds of an octave, the same for every band.
            for (size_t b = 0; b < bandCentres.size(); ++b)
                filters[b] = Biquad::bandPass (sampleRate, juce::jmin (bandCentres[b], sampleRate * 0.45), 1.414);
        }

        void process (const float* left, const float* right, int numSamples)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto mid = 0.5 * (static_cast<double> (left[i]) + static_cast<double> (right[i]));

                for (size_t b = 0; b < bandCentres.size(); ++b)
                {
                    const auto y = second[b].processDouble (filters[b], first[b].processDouble (filters[b], mid));
                    energy[b] += y * y;
                }

                if (++count == stepLength)
                {
                    steps.push_back (energy);
                    energy.fill (0.0);
                    count = 0;
                }
            }
        }

        std::vector<std::array<double, 10>> steps;

    private:
        std::array<Biquad, 10> filters {};
        std::array<BiquadState, 10> first {}, second {};
        std::array<double, 10> energy {};
        int stepLength = 4800, count = 0;
    };

    /** The glue's reduction (dB) averaged over the loud milliseconds, simulated on the detector's
        level taken every millisecond: the same gain computer and smoothing as the compressor. */
    double simulateGlue (const std::vector<float>& levels, const std::vector<char>& loud, const Compressor::Settings& s)
    {
        const auto slope = s.ratio > 1.0f ? 1.0 - 1.0 / static_cast<double> (s.ratio) : 0.0;
        const auto knee = static_cast<double> (s.kneeDb);
        const auto attack = std::exp (-1.0 / juce::jmax (0.01, static_cast<double> (s.attackMs)));    // a step per millisecond
        const auto release = std::exp (-1.0 / juce::jmax (0.01, static_cast<double> (s.releaseMs)));
        double held = 0.0, smoothed = 0.0, sum = 0.0;
        juce::int64 count = 0;

        for (size_t i = 0; i < levels.size(); ++i)
        {
            const auto above = static_cast<double> (levels[i]) - s.thresholdDb;
            double over = 0.0;

            if (slope > 0.0)
            {
                if (2.0 * above <= -knee)
                    over = 0.0;
                else if (knee > 0.0 && 2.0 * std::abs (above) <= knee)
                    over = slope * (above + knee * 0.5) * (above + knee * 0.5) / (2.0 * knee);
                else
                    over = slope * above;
            }

            held = juce::jmax (over, release * held + (1.0 - release) * over);
            smoothed = attack * smoothed + (1.0 - attack) * held;

            if (i < loud.size() && loud[i] != 0)
            {
                sum += smoothed;
                ++count;
            }
        }

        return count > 0 ? sum / static_cast<double> (count) : 0.0;
    }
}

//==============================================================================
MasteringAssistant::MasteringAssistant (const juce::File& rendered, Options o)
    : Thread ("Sonify mastering"), file (rendered), options (o)
{
}

MasteringAssistant::~MasteringAssistant()
{
    stopThread (10000);
}

double MasteringAssistant::targetFor (Loudness loudness) noexcept
{
    switch (loudness)
    {
        case Loudness::dynamic:   return -16.0;
        case Loudness::streaming: return -14.0;
        case Loudness::loud:      return -10.0;
    }

    return -14.0;
}

juce::String MasteringAssistant::nameOf (Loudness loudness)
{
    return loudness == Loudness::dynamic ? "dynamic" : loudness == Loudness::loud ? "loud" : "streaming";
}

juce::String MasteringAssistant::nameOf (Character character)
{
    return character == Character::warm ? "warm" : character == Character::bright ? "bright" : "clean";
}

MasteringAssistant::Loudness MasteringAssistant::loudnessFrom (const juce::String& name)
{
    return name == "dynamic" ? Loudness::dynamic : name == "loud" ? Loudness::loud : Loudness::streaming;
}

MasteringAssistant::Character MasteringAssistant::characterFrom (const juce::String& name)
{
    return name == "warm" ? Character::warm : name == "bright" ? Character::bright : Character::clean;
}

std::array<double, 5> MasteringAssistant::eqFor (const std::array<double, 10>& bandLevelsDb, Character character)
{
    // A balance typical of finished music, octave by octave from 31.5 Hz (pink noise reads flat).
    std::array<double, 10> target { 0.0, 4.0, 4.0, 2.0, 1.0, 0.0, -1.5, -3.0, -5.0, -9.0 };

    if (character == Character::warm)
    {
        target[1] += 1.5; target[2] += 1.5; target[3] += 0.5;
        target[7] -= 1.0; target[8] -= 1.5; target[9] -= 2.0;
    }
    else if (character == Character::bright)
    {
        target[1] -= 0.5; target[2] -= 0.5;
        target[7] += 1.0; target[8] += 1.5; target[9] += 2.0;
    }

    // Both relative to their mean from 63 Hz to 8 kHz; a band with next to nothing in it counts
    // as 40 dB down, so an empty top octave can't run away with the EQ.
    double rawMean = 0.0;

    for (size_t b = 1; b <= 8; ++b)
        rawMean += bandLevelsDb[b] / 8.0;

    std::array<double, 10> levels {};

    for (size_t b = 0; b < levels.size(); ++b)
        levels[b] = juce::jmax (bandLevelsDb[b], rawMean - 40.0);

    double levelMean = 0.0, targetMean = 0.0;

    for (size_t b = 1; b <= 8; ++b)
    {
        levelMean += levels[b] / 8.0;
        targetMean += target[b] / 8.0;
    }

    std::array<double, 10> off {};

    for (size_t b = 0; b < off.size(); ++b)
        off[b] = (levels[b] - levelMean) - (target[b] - targetMean);

    // Each band answers to the octaves under it; only differences over a dB count, and they're
    // met a bit more than half way, never more than a few dB.
    const auto correct = [] (double difference, double limit)
    {
        if (std::abs (difference) <= 1.0)
            return 0.0;

        const auto beyond = difference - (difference > 0.0 ? 1.0 : -1.0);
        return juce::jlimit (-limit, limit, -0.6 * beyond);
    };

    return { correct (0.5 * off[1] + 0.5 * off[2], 3.0),
             correct (0.7 * off[3] + 0.3 * off[4], 2.5),
             correct (off[5], 2.5),
             correct (0.5 * off[6] + 0.5 * off[7], 2.5),
             correct (0.6 * off[8] + 0.4 * off[9], 3.0) };
}

//==============================================================================
void MasteringAssistant::run()
{
    result = analyse();

    if (! result.ok && threadShouldExit())
        result.error = "Cancelled";

    progress.store (1.0);
    done.store (true);
}

bool MasteringAssistant::readAll (juce::AudioFormatReader& reader, const std::function<void (float*, float*, int)>& block, double from, double to)
{
    constexpr int blockSize = 4096;
    juce::AudioBuffer<float> buffer (2, blockSize);
    const auto length = reader.lengthInSamples;
    const auto gain = static_cast<float> (options.inputGain);

    for (juce::int64 at = 0; at < length; at += blockSize)
    {
        if (threadShouldExit())
            return false;

        const auto count = static_cast<int> (juce::jmin<juce::int64> (blockSize, length - at));

        if (! reader.read (&buffer, 0, count, at, true, true))
            buffer.clear (0, count);

        if (std::abs (gain - 1.0f) > 1.0e-7f)
            buffer.applyGain (0, count, gain);

        block (buffer.getWritePointer (0), buffer.getWritePointer (1), count);
        progress.store (from + (to - from) * static_cast<double> (at + count) / static_cast<double> (length));
    }

    return true;
}

MasteringAssistant::Result MasteringAssistant::analyse()
{
    Result answer;
    const auto failure = [&answer] (const juce::String& why) { answer.error = why; return answer; };

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return failure ("Sonify couldn't read the song back to listen to it.");

    const auto rate = reader->sampleRate;

    if (rate <= 0.0 || reader->lengthInSamples < static_cast<juce::int64> (rate))
        return failure ("The song is too short to master: it needs at least a second.");

    // 1. Listening: loudness, true peak and the tonal balance.
    LoudnessMeter meter;
    meter.prepare (rate);
    TruePeakMeter peaks;
    peaks.reset();
    BandMeter bands (rate, meter.getStepLength());

    if (! readAll (*reader, [&] (float* l, float* r, int n)
                   {
                       meter.process (l, r, n);
                       peaks.process (l, r, n);
                       bands.process (l, r, n);
                   }, 0.0, 0.3))
        return failure ("Cancelled");

    const auto blocks = meter.getBlocks();
    answer.beforeLufs = LoudnessMeter::integrated (blocks);
    answer.beforePeakDb = peaks.getPeakDb();
    answer.targetLufs = targetFor (options.loudness);

    if (answer.beforeLufs < -60.0)
        return failure ("Main is silent: there's nothing to master. Is everything muted?");

    // Where the song plays: the 100 ms steps whose 400 ms block passes the loudness gate.
    const auto gate = LoudnessMeter::relativeGate (blocks);
    const auto& steps = meter.getSteps();
    std::vector<size_t> playing;

    for (size_t i = 3; i < steps.size() && i - 3 < blocks.size(); ++i)
        if (blocks[i - 3] > gate)
            playing.push_back (i);

    std::array<double, 10> bandLevels {};

    for (size_t b = 0; b < bandLevels.size(); ++b)
    {
        double energy = 0.0;

        for (const auto i : playing)
            if (i < bands.steps.size())
                energy += bands.steps[i][b];

        bandLevels[b] = 10.0 * std::log10 (energy + 1.0e-20);
    }

    // 2. EQ.
    auto& s = answer.settings;
    answer.eqDb = eqFor (bandLevels, options.character);
    s.lowGainDb = static_cast<float> (answer.eqDb[0]);
    s.lowMidGainDb = static_cast<float> (answer.eqDb[1]);
    s.midGainDb = static_cast<float> (answer.eqDb[2]);
    s.presenceGainDb = static_cast<float> (answer.eqDb[3]);
    s.airGainDb = static_cast<float> (answer.eqDb[4]);
    s.lowCutHz = 20.0f;
    s.ceilingDb = static_cast<float> (ceilingDb);

    // 3. Glue: how far the peaks stand above the loudness, against what the target leaves room for.
    const auto loud = options.loudness == Loudness::loud;
    const auto dynamic = options.loudness == Loudness::dynamic;
    const auto needed = juce::jmax (0.0, (answer.beforePeakDb - answer.beforeLufs) - (ceilingDb - answer.targetLufs));
    auto glue = juce::jlimit (0.0, loud ? 3.0 : dynamic ? 1.5 : 2.0, needed * 0.35);

    if (glue < 0.5)
        glue = 0.0;

    s.compressor = { 0.0f, glue > 0.0 ? (loud ? 2.5f : dynamic ? 1.5f : 2.0f) : 1.0f,
                     loud ? 15.0f : dynamic ? 30.0f : 25.0f,
                     loud ? 150.0f : dynamic ? 250.0f : 200.0f,
                     6.0f, 0.0f, 1.0f, true };
    s.limiterReleaseMs = loud ? 60.0f : dynamic ? 150.0f : 100.0f;

    if (glue > 0.0)
    {
        // The detector's level after the EQ, every millisecond...
        MasteringChain eq;
        eq.prepare (rate);
        eq.setSettings (s);
        eq.reset();

        const auto everyMs = juce::jmax (1, static_cast<int> (std::lround (rate / 1000.0)));
        const auto smoothing = std::exp (-1.0 / (0.010 * rate));   // the compressor's 10 ms RMS window
        std::vector<float> levels;
        levels.reserve (static_cast<size_t> (reader->lengthInSamples / everyMs + 1));
        double power = 0.0;
        int sinceLast = 0;

        if (! readAll (*reader, [&] (float* l, float* r, int n)
                       {
                           eq.processEq (l, r, n);

                           for (int i = 0; i < n; ++i)
                           {
                               power = smoothing * power + (1.0 - smoothing) * 0.5 * (static_cast<double> (l[i]) * l[i] + static_cast<double> (r[i]) * r[i]);

                               if (++sinceLast == everyMs)
                               {
                                   levels.push_back (static_cast<float> (power > 1.0e-14 ? 10.0 * std::log10 (power) + 3.0103 : -140.0));
                                   sinceLast = 0;
                               }
                           }
                       }, 0.3, 0.45))
            return failure ("Cancelled");

        // ...the loudest quarter of the song's playing steps...
        std::vector<double> playingLoudness;

        for (const auto i : playing)
            playingLoudness.push_back (steps[i]);

        std::sort (playingLoudness.begin(), playingLoudness.end());
        const auto loudFrom = playingLoudness.empty() ? 0.0 : playingLoudness[playingLoudness.size() * 3 / 4];
        const auto msPerStep = static_cast<double> (meter.getStepLength()) / static_cast<double> (everyMs);
        std::vector<char> loudMs (levels.size(), 0), isPlaying (steps.size(), 0);

        for (const auto i : playing)
            isPlaying[i] = 1;

        for (size_t i = 0; i < steps.size(); ++i)
        {
            if (steps[i] < loudFrom || isPlaying[i] == 0)
                continue;

            const auto first = static_cast<size_t> (static_cast<double> (i) * msPerStep);
            const auto last = juce::jmin (loudMs.size(), static_cast<size_t> (static_cast<double> (i + 1) * msPerStep));

            for (auto m = first; m < last; ++m)
                loudMs[m] = 1;
        }

        // ...and the threshold that takes the glue off them.
        double low = -60.0, high = 0.0;

        for (int round = 0; round < 24; ++round)
        {
            const auto middle = 0.5 * (low + high);
            s.compressor.thresholdDb = static_cast<float> (middle);

            if (simulateGlue (levels, loudMs, s.compressor) > glue)
                low = middle;
            else
                high = middle;
        }

        s.compressor.thresholdDb = static_cast<float> (0.5 * (low + high));
        answer.compressionDb = simulateGlue (levels, loudMs, s.compressor);
    }

    // 4. Drive: run the whole mastering over the song until it lands on the loudness asked for.
    s.driveDb = static_cast<float> (juce::jlimit (-24.0, 24.0, answer.targetLufs - answer.beforeLufs + glue * 0.7 + 0.3));
    MasteringChain chain;
    chain.prepare (rate);

    double lastDrive = 0.0, lastLoudness = 0.0;
    constexpr int passes = 4;

    for (int pass = 0; pass < passes; ++pass)
    {
        chain.setSettings (s);
        chain.reset();
        LoudnessMeter after;
        after.prepare (rate);
        TruePeakMeter afterPeaks;
        afterPeaks.reset();

        const auto from = 0.45 + 0.55 * pass / passes;

        if (! readAll (*reader, [&] (float* l, float* r, int n)
                       {
                           chain.process (l, r, n);
                           after.process (l, r, n);
                           afterPeaks.process (l, r, n);
                       }, from, from + 0.55 / passes))
            return failure ("Cancelled");

        answer.afterLufs = after.getIntegrated();
        answer.afterPeakDb = afterPeaks.getPeakDb();
        answer.limitingDb = chain.limiter.getMaxReductionDb();

        const auto miss = answer.targetLufs - answer.afterLufs;

        if (std::abs (miss) < 0.1 || pass == passes - 1)
            break;

        // The loudness follows the drive less than one for one once the limiter works.
        auto slope = 1.0;

        if (pass > 0 && std::abs (s.driveDb - lastDrive) > 1.0e-3)
            slope = juce::jlimit (0.2, 1.2, (answer.afterLufs - lastLoudness) / (s.driveDb - lastDrive));

        lastDrive = s.driveDb;
        lastLoudness = answer.afterLufs;
        s.driveDb = static_cast<float> (juce::jlimit (-24.0, 24.0, s.driveDb + miss / slope));
    }

    answer.ok = true;
    return answer;
}
