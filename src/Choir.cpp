// D:\workspace\Fanan_web_hub\src\Choir.cpp

#include "Choir.h"
#include "AudioMedia.h"
#include "Transport.h"

#if __has_include (<signalsmith-stretch/signalsmith-stretch.h>)
 #include <signalsmith-stretch/signalsmith-stretch.h>
 #define SONIFY_CHOIR_STRETCH 1
#else
 #define SONIFY_CHOIR_STRETCH 0
#endif

#include <algorithm>
#include <cmath>
#include <limits>

//==============================================================================
// Settings
namespace
{
    const char* modeName (ChoirSettings::Mode mode)
    {
        switch (mode)
        {
            case ChoirSettings::Mode::chord:     return "chord";
            case ChoirSettings::Mode::semitones: return "semitones";
            case ChoirSettings::Mode::scale:     break;
        }

        return "scale";
    }

    ChoirSettings::Mode modeFrom (const juce::String& name)
    {
        return name == "chord" ? ChoirSettings::Mode::chord : name == "semitones" ? ChoirSettings::Mode::semitones : ChoirSettings::Mode::scale;
    }

    double numberOf (const juce::var& value, double fallback)
    {
        if (value.isInt() || value.isInt64() || value.isDouble() || value.isBool())
        {
            const auto number = static_cast<double> (value);
            return std::isfinite (number) ? number : fallback;
        }

        return fallback;
    }

    float rangedFloat (const juce::var& object, const char* key, float fallback, float low, float high)
    {
        return juce::jlimit (low, high, static_cast<float> (numberOf (object.getProperty (key, {}), fallback)));
    }

    int rangedInt (const juce::var& object, const char* key, int fallback, int low, int high)
    {
        return juce::jlimit (low, high, static_cast<int> (std::lround (numberOf (object.getProperty (key, {}), fallback))));
    }

    int intervalLimit (ChoirSettings::Mode mode)
    {
        return mode == ChoirSettings::Mode::semitones ? 24 : mode == ChoirSettings::Mode::chord ? 4 : 7;
    }
}

bool ChoirSettings::Voice::operator== (const Voice& other) const noexcept
{
    return female == other.female && juce::exactlyEqual (character, other.character) && mode == other.mode
        && interval == other.interval && octave == other.octave && juce::exactlyEqual (levelDb, other.levelDb)
        && juce::exactlyEqual (pan, other.pan) && mute == other.mute && solo == other.solo;
}

bool ChoirSettings::operator== (const ChoirSettings& other) const noexcept
{
    if (leadFemale != other.leadFemale || ! juce::exactlyEqual (leadDb, other.leadDb) || ! juce::exactlyEqual (choirDb, other.choirDb)
        || ! juce::exactlyEqual (spread, other.spread) || ! juce::exactlyEqual (timing, other.timing)
        || ! juce::exactlyEqual (pitch, other.pitch) || ! juce::exactlyEqual (vibrato, other.vibrato)
        || ! juce::exactlyEqual (hall, other.hall) || chordTrack != other.chordTrack || count != other.count)
        return false;

    for (int i = 0; i < count; ++i)
        if (! (voices[static_cast<size_t> (i)] == other.voices[static_cast<size_t> (i)]))
            return false;

    return true;
}

juce::var ChoirSettings::toVar() const
{
    juce::DynamicObject::Ptr object (new juce::DynamicObject());
    object->setProperty ("leadFemale", leadFemale);
    object->setProperty ("lead", static_cast<double> (leadDb));
    object->setProperty ("choir", static_cast<double> (choirDb));
    object->setProperty ("spread", static_cast<double> (spread));
    object->setProperty ("timing", static_cast<double> (timing));
    object->setProperty ("pitch", static_cast<double> (pitch));
    object->setProperty ("vibrato", static_cast<double> (vibrato));
    object->setProperty ("hall", static_cast<double> (hall));
    object->setProperty ("chordTrack", chordTrack);

    juce::Array<juce::var> list;

    for (int i = 0; i < count; ++i)
    {
        const auto& voice = voices[static_cast<size_t> (i)];
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("female", voice.female);
        item->setProperty ("character", static_cast<double> (voice.character));
        item->setProperty ("mode", modeName (voice.mode));
        item->setProperty ("interval", voice.interval);
        item->setProperty ("octave", voice.octave);
        item->setProperty ("level", static_cast<double> (voice.levelDb));
        item->setProperty ("pan", static_cast<double> (voice.pan));
        item->setProperty ("mute", voice.mute);
        item->setProperty ("solo", voice.solo);
        list.add (juce::var (item.get()));
    }

    object->setProperty ("voices", list);
    return juce::var (object.get());
}

ChoirSettings ChoirSettings::fromVar (const juce::var& value)
{
    ChoirSettings out;

    if (! value.isObject())
        return makeDefault();

    out.leadFemale = static_cast<bool> (value.getProperty ("leadFemale", false));
    out.leadDb = rangedFloat (value, "lead", 0.0f, leadOff, 6.0f);
    out.choirDb = rangedFloat (value, "choir", 0.0f, -36.0f, 12.0f);
    out.spread = rangedFloat (value, "spread", 0.8f, 0.0f, 1.0f);
    out.timing = rangedFloat (value, "timing", 0.5f, 0.0f, 1.0f);
    out.pitch = rangedFloat (value, "pitch", 0.4f, 0.0f, 1.0f);
    out.vibrato = rangedFloat (value, "vibrato", 0.2f, 0.0f, 1.0f);
    out.hall = rangedFloat (value, "hall", 0.25f, 0.0f, 1.0f);
    out.chordTrack = juce::jmax (0, rangedInt (value, "chordTrack", 0, 0, std::numeric_limits<int>::max()));

    if (const auto* list = value.getProperty ("voices", {}).getArray())
    {
        for (const auto& item : *list)
        {
            if (! item.isObject() || out.count >= maxVoices)
                continue;

            auto& voice = out.voices[static_cast<size_t> (out.count++)];
            voice.female = static_cast<bool> (item.getProperty ("female", false));
            voice.character = rangedFloat (item, "character", 0.0f, -1.0f, 1.0f);
            voice.mode = modeFrom (item.getProperty ("mode", {}).toString());
            const auto limit = intervalLimit (voice.mode);
            voice.interval = rangedInt (item, "interval", 0, -limit, limit);
            voice.octave = rangedInt (item, "octave", 0, -2, 2);
            voice.levelDb = rangedFloat (item, "level", 0.0f, -36.0f, 6.0f);
            voice.pan = rangedFloat (item, "pan", 0.0f, -1.0f, 1.0f);
            voice.mute = static_cast<bool> (item.getProperty ("mute", false));
            voice.solo = static_cast<bool> (item.getProperty ("solo", false));
        }
    }

    return out;
}

ChoirSettings ChoirSettings::makeDefault()
{
    ChoirSettings out;
    out.count = 4;

    // Around a man's lead: a third above and the note itself an octave up (two women), a third
    // above and a fourth below (two men), across the stereo.
    const struct { bool female; int interval, octave; float pan; } parts[] = {
        { true, 2, 1, -0.6f }, { true, 0, 1, 0.6f }, { false, 2, 0, -0.25f }, { false, -3, 0, 0.25f }
    };

    for (int i = 0; i < out.count; ++i)
    {
        auto& voice = out.voices[static_cast<size_t> (i)];
        voice.female = parts[i].female;
        voice.interval = parts[i].interval;
        voice.octave = parts[i].octave;
        voice.pan = parts[i].pan;
    }

    return out;
}

//==============================================================================
/** The note sung: YIN on the vocal, low-passed and taken down to about 12 kHz, every ~5 ms. */
class Choir::PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        factor = juce::jmax (1, static_cast<int> (std::lround (sampleRate / 12000.0)));
        rate = sampleRate / factor;
        maxTau = juce::jmin (static_cast<int> (rate / 65.0), ringSize / 4);
        minTau = juce::jmax (2, static_cast<int> (rate / 1100.0));
        window.assign (static_cast<size_t> (frame + maxTau), 0.0f);
        difference.assign (static_cast<size_t> (maxTau + 1), 0.0f);

        // Two biquads: a fourth-order low-pass well under half the reduced rate.
        const auto cutoff = juce::jmin (2800.0, 0.23 * rate);
        for (auto& stage : stages)
            stage.setCoefficients (juce::IIRCoefficients::makeLowPass (sampleRate, cutoff, 0.7071));

        reset();
    }

    void reset() noexcept
    {
        for (auto& stage : stages)
            stage.reset();

        std::fill (ring.begin(), ring.end(), 0.0f);
        written = 0;
        phase = 0;
        untilNext = hop;
        historyCount = 0;
        historyStart = 0;
    }

    /** n samples of the vocal fed, the first of them at feed index firstIndex. */
    void push (const float* input, int n, juce::int64 firstIndex) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            auto sample = input[i];

            for (auto& stage : stages)
                sample = stage.processSingleSampleRaw (sample);

            if (++phase < factor)
                continue;

            phase = 0;
            ring[static_cast<size_t> (written & (ringSize - 1))] = sample;
            ++written;

            if (--untilNext > 0 || written < frame + maxTau)
                continue;

            untilNext = hop;
            const auto midi = analyse();
            // The window's middle, in feed samples.
            const auto centre = firstIndex + i - static_cast<juce::int64> ((frame + maxTau) / 2) * factor;
            remember (centre, midi);
        }
    }

    /** The pitch (MIDI note, with cents) at or just before a feed index; -1 for no note. */
    float pitchAt (juce::int64 index) const noexcept
    {
        float found = -1.0f;

        for (int k = 0; k < historyCount; ++k)
        {
            const auto& entry = history[static_cast<size_t> ((historyStart + k) % historySize)];

            if (entry.index > index)
                break;

            found = entry.midi;
        }

        return found;
    }

private:
    struct Entry
    {
        juce::int64 index = 0;
        float midi = -1.0f;
    };

    float analyse() noexcept
    {
        const auto length = frame + maxTau;

        for (int j = 0; j < length; ++j)
            window[static_cast<size_t> (j)] = ring[static_cast<size_t> ((written - length + j) & (ringSize - 1))];

        // Too quiet to be a note.
        float energy = 0.0f;
        for (int j = 0; j < frame; ++j)
            energy += window[static_cast<size_t> (j)] * window[static_cast<size_t> (j)];

        if (energy / static_cast<float> (frame) < 1.0e-5f)
            return -1.0f;

        const auto* x = window.data();

        for (int tau = 1; tau <= maxTau; ++tau)
        {
            float sum = 0.0f;

            for (int j = 0; j < frame; ++j)
            {
                const auto d = x[j] - x[j + tau];
                sum += d * d;
            }

            difference[static_cast<size_t> (tau)] = sum;
        }

        // The cumulative mean normalised difference, and its first dip under the threshold.
        float running = 0.0f;
        int best = -1;
        float bestValue = 1.0f;
        std::array<float, 2> around {};

        for (int tau = 1; tau <= maxTau; ++tau)
        {
            running += difference[static_cast<size_t> (tau)];
            difference[static_cast<size_t> (tau)] = running > 0.0f ? difference[static_cast<size_t> (tau)] * static_cast<float> (tau) / running : 1.0f;
        }

        for (int tau = minTau; tau < maxTau; ++tau)
        {
            const auto value = difference[static_cast<size_t> (tau)];

            if (value < 0.15f)
            {
                while (tau + 1 < maxTau && difference[static_cast<size_t> (tau + 1)] < difference[static_cast<size_t> (tau)])
                    ++tau;

                best = tau;
                bestValue = difference[static_cast<size_t> (tau)];
                break;
            }

            if (value < bestValue)
            {
                bestValue = value;
                best = tau;
            }
        }

        if (best < minTau || bestValue > 0.3f)
            return -1.0f;

        // Between the samples: a parabola through the dip.
        around[0] = difference[static_cast<size_t> (best - 1)];
        around[1] = best + 1 <= maxTau ? difference[static_cast<size_t> (best + 1)] : difference[static_cast<size_t> (best)];
        const auto centre = difference[static_cast<size_t> (best)];
        const auto denominator = around[0] - 2.0f * centre + around[1];
        const auto shift = std::abs (denominator) > 1.0e-9f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (around[0] - around[1]) / denominator) : 0.0f;
        const auto frequency = rate / (static_cast<double> (best) + shift);
        return static_cast<float> (69.0 + 12.0 * std::log2 (frequency / 440.0));
    }

    void remember (juce::int64 index, float midi) noexcept
    {
        if (historyCount == historySize)
        {
            historyStart = (historyStart + 1) % historySize;
            --historyCount;
        }

        history[static_cast<size_t> ((historyStart + historyCount) % historySize)] = { index, midi };
        ++historyCount;
    }

    static constexpr int ringSize = 2048;
    static constexpr int frame = 256;
    static constexpr int hop = 64;
    static constexpr int historySize = 128;

    int factor = 4;
    double rate = 12000.0;
    int maxTau = 180, minTau = 11;
    std::array<juce::IIRFilter, 2> stages;
    std::array<float, ringSize> ring {};
    std::vector<float> window, difference;
    juce::int64 written = 0;
    int phase = 0, untilNext = hop;
    std::array<Entry, historySize> history {};
    int historyCount = 0, historyStart = 0;
};

//==============================================================================
/** One singer: a shifter, and who they are (when they come in, how they drift, their vibrato). */
struct Choir::VoiceDsp
{
    explicit VoiceDsp (int index)
        : random (static_cast<juce::int64> (index) * 7919 + 17)
    #if SONIFY_CHOIR_STRETCH
        , stretch (static_cast<long> (1000 + index))
    #endif
    {
        formantJitter = (random.nextFloat() * 2.0f - 1.0f) * 0.4f;
        baseDelayMs = 4.0f + random.nextFloat() * 22.0f;
        vibratoRate = 4.6f + random.nextFloat() * 1.4f;
        vibratoPhase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        pitchTarget = random.nextFloat() * 2.0f - 1.0f;
        timeTarget = random.nextFloat() * 2.0f - 1.0f;
    }

    /** Message thread (or no audio running): set up for a rate, ready to play without allocating. */
    void setUp (double sampleRate, int lookahead)
    {
        out.assign (maxChunkSamples, 0.0f);
        delayLine.assign (static_cast<size_t> (juce::nextPowerOfTwo (static_cast<int> (0.1 * sampleRate) + maxChunkSamples + 8)), 0.0f);
        delayWrite = 0;
        delaySamples = -1.0f;

    #if SONIFY_CHOIR_STRETCH
        stretch.presetDefault (1, static_cast<float> (sampleRate), true);

        // Once through, so its buffers are as big as they get before the audio thread uses it.
        std::vector<float> silence (static_cast<size_t> (juce::jmax (lookahead, maxChunkSamples)), 0.0f);
        const float* in = silence.data();
        float* output = out.data();
        stretch.outputSeek (&in, lookahead);
        stretch.process (&in, maxChunkSamples, &output, maxChunkSamples);
        stretch.reset();
    #else
        juce::ignoreUnused (lookahead);
    #endif

        stale = true;
        ready = true;
    }

    static constexpr int maxChunkSamples = 256;

    juce::Random random;
#if SONIFY_CHOIR_STRETCH
    signalsmith::stretch::SignalsmithStretch<float> stretch;
#endif
    std::vector<float> out, delayLine;
    int delayWrite = 0;
    float delaySamples = -1.0f;
    bool stale = true, ready = false;
    float lastLeft = 0.0f, lastRight = 0.0f;

    float formantJitter = 0.0f, baseDelayMs = 10.0f, vibratoRate = 5.0f, vibratoPhase = 0.0f;
    float pitchDrift = 0.0f, pitchTarget = 0.0f, timeDrift = 0.0f, timeTarget = 0.0f;
    int driftCountdown = 0;
};

//==============================================================================
Choir::Change::Change() = default;
Choir::Change::~Change() = default;
Choir::Change::Change (Change&&) noexcept = default;
Choir::Change& Choir::Change::operator= (Change&&) noexcept = default;

Choir::Choir()
    : tracker (std::make_unique<PitchTracker>())
{
    static_assert (maxChunk == VoiceDsp::maxChunkSamples);
}

Choir::~Choir() = default;

void Choir::prepare (double rate, int)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;

#if SONIFY_CHOIR_STRETCH
    {
        signalsmith::stretch::SignalsmithStretch<float> probe;
        probe.presetDefault (1, static_cast<float> (sampleRate), true);
        lookahead = probe.inputLatency() + probe.outputLatency();
        analysisDelay = probe.inputLatency();
    }
#else
    lookahead = 0;
    analysisDelay = 0;
#endif

    for (auto& voice : voices)
        if (voice != nullptr)
            voice->setUp (sampleRate, lookahead);

    tracker->prepare (sampleRate);
    feedBuffer.assign (static_cast<size_t> (maxChunk), 0.0f);
    startBuffer.assign (static_cast<size_t> (juce::jmax (1, lookahead)), 0.0f);
    busLeft.assign (static_cast<size_t> (maxChunk), 0.0f);
    busRight.assign (static_cast<size_t> (maxChunk), 0.0f);

    hall.setSampleRate (sampleRate);
    hallParameters = {};
    hallParameters.roomSize = 0.78f;
    hallParameters.damping = 0.5f;
    hallParameters.width = 1.0f;
    hallParameters.wetLevel = 0.0f;
    hallParameters.dryLevel = 1.0f;
    hall.setParameters (hallParameters);
    hall.reset();

    rolling = false;
    spanCount = 0;
}

Choir::Change Choir::prepareChange (const ChoirSettings& wanted) const
{
    Change change;
    change.settings = wanted;

    for (int i = 0; i < wanted.count; ++i)
    {
        if (voices[static_cast<size_t> (i)] != nullptr)
            continue;

        auto voice = std::make_unique<VoiceDsp> (i);

        if (sampleRate > 0.0)
            voice->setUp (sampleRate, lookahead);

        change.voices[static_cast<size_t> (i)] = std::move (voice);
    }

    return change;
}

void Choir::apply (Change& change) noexcept
{
    for (size_t i = 0; i < voices.size(); ++i)
        if (change.voices[i] != nullptr && voices[i] == nullptr)
            voices[i] = std::move (change.voices[i]);

    settings = change.settings;
}

std::unique_ptr<const Choir::Timeline> Choir::swapTimeline (std::unique_ptr<const Timeline> fresh) noexcept
{
    std::swap (timeline, fresh);
    return fresh;
}

//==============================================================================
// Reading the clips
void Choir::readMono (const AudioPlaylist* list, juce::int64 from, int numSamples, float* dest) noexcept
{
    std::fill (dest, dest + numSamples, 0.0f);

    if (list == nullptr || numSamples <= 0)
        return;

    constexpr juce::int64 fade = 64;   // as Track::playClips: clip edges ramp in and out
    const auto to = from + numSamples;

    for (const auto& item : list->items)
    {
        if (item.data == nullptr || item.lengthSamples <= 0)
            continue;

        const auto itemEnd = item.startSample + item.lengthSamples;

        if (itemEnd <= from || item.startSample >= to)
            continue;

        const auto& samples = item.data->samples;
        const auto* sourceLeft = samples.getReadPointer (0);
        const auto* sourceRight = samples.getReadPointer (samples.getNumChannels() > 1 ? 1 : 0);
        const auto available = static_cast<juce::int64> (samples.getNumSamples());
        const auto first = juce::jmax (from, item.startSample);
        const auto last = juce::jmin (to, itemEnd);

        for (auto song = first; song < last; ++song)
        {
            const auto intoClip = song - item.startSample;
            const auto source = item.offsetSamples + intoClip;

            if (source < 0 || source >= available)
                continue;

            auto gain = item.gain;
            const auto untilEnd = item.lengthSamples - intoClip;

            if (intoClip < fade)
                gain *= static_cast<float> (intoClip) / static_cast<float> (fade);
            else if (untilEnd < fade)
                gain *= static_cast<float> (untilEnd) / static_cast<float> (fade);

            if (intoClip < item.fadeInSamples)
                gain *= static_cast<float> (std::sin (juce::MathConstants<double>::halfPi * static_cast<double> (intoClip) / static_cast<double> (item.fadeInSamples)));

            if (untilEnd < item.fadeOutSamples)
                gain *= static_cast<float> (std::sin (juce::MathConstants<double>::halfPi * static_cast<double> (untilEnd) / static_cast<double> (item.fadeOutSamples)));

            dest[song - from] += 0.5f * (sourceLeft[source] + sourceRight[source]) * gain;
        }
    }
}

void Choir::addSpan (juce::int64 song, juce::int64 length) noexcept
{
    const auto index = fedIndex;

    if (spanCount > 0)
    {
        auto& last = spans[static_cast<size_t> (spanCount - 1)];

        if (last.index + last.length == index && last.song + last.length == song)
        {
            last.length += length;
            return;
        }
    }

    if (spanCount == maxSpans)
    {
        rolling = false;   // more jumps ahead than it can follow: it starts again where the song is
        return;
    }

    spans[static_cast<size_t> (spanCount++)] = { index, song, length };
}

void Choir::dropSpansBefore (juce::int64 index) noexcept
{
    int keep = 0;

    while (keep < spanCount && spans[static_cast<size_t> (keep)].index + spans[static_cast<size_t> (keep)].length <= index)
        ++keep;

    if (keep == 0)
        return;

    for (int i = keep; i < spanCount; ++i)
        spans[static_cast<size_t> (i - keep)] = spans[static_cast<size_t> (i)];

    spanCount -= keep;
}

juce::int64 Choir::songAt (juce::int64 index) const noexcept
{
    for (int i = 0; i < spanCount; ++i)
    {
        const auto& span = spans[static_cast<size_t> (i)];

        if (index >= span.index && index < span.index + span.length)
            return span.song + (index - span.index);
    }

    return std::numeric_limits<juce::int64>::min();
}

juce::int64 Choir::spanLeft (juce::int64 index) const noexcept
{
    for (int i = 0; i < spanCount; ++i)
    {
        const auto& span = spans[static_cast<size_t> (i)];

        if (index >= span.index && index < span.index + span.length)
            return span.index + span.length - index;
    }

    return 0;
}

void Choir::feed (float* dest, int numSamples) noexcept
{
    int done = 0;

    while (done < numSamples)
    {
        auto chunk = static_cast<juce::int64> (numSamples - done);

        // The loop goes round ahead of the song, as the song will.
        if (loopOn && feedSong < loopEnd && feedSong + chunk > loopEnd)
            chunk = loopEnd - feedSong;

        readMono (reading, feedSong, static_cast<int> (chunk), dest + done);
        addSpan (feedSong, chunk);
        fedIndex += chunk;
        feedSong += chunk;
        done += static_cast<int> (chunk);

        if (loopOn && feedSong == loopEnd)
            feedSong = loopStart;
    }

    tracker->push (dest, numSamples, fedIndex - numSamples);
}

void Choir::readFed (float* dest, juce::int64 fromIndex, int numSamples) const noexcept
{
    std::fill (dest, dest + numSamples, 0.0f);
    const auto toIndex = fromIndex + numSamples;

    for (int i = 0; i < spanCount; ++i)
    {
        const auto& span = spans[static_cast<size_t> (i)];
        const auto first = juce::jmax (fromIndex, span.index);
        const auto last = juce::jmin (toIndex, span.index + span.length);

        if (last > first)
            readMono (reading, span.song + (first - span.index), static_cast<int> (last - first), dest + (first - fromIndex));
    }
}

//==============================================================================
// The parts
namespace
{
    bool hasPitchClass (juce::uint16 notes, int note) noexcept
    {
        return ((notes >> (((note % 12) + 12) % 12)) & 1) != 0;
    }

    int bitCount (juce::uint16 notes) noexcept
    {
        int count = 0;

        for (int i = 0; i < 12; ++i)
            count += (notes >> i) & 1;

        return count;
    }

    /** Semitones from the note to the scale note steps away (from the nearest scale note when
        the note sung isn't one). */
    int scaleSteps (int note, int root, juce::uint16 notes, int steps) noexcept
    {
        if (notes == 0)
            notes = 0x0fff;

        const auto inScale = [root, notes] (int n) { return hasPitchClass (notes, n - root); };
        auto base = note;

        if (! inScale (base))
        {
            for (int d = 1; d <= 6; ++d)
            {
                if (inScale (note - d)) { base = note - d; break; }
                if (inScale (note + d)) { base = note + d; break; }
            }
        }

        auto target = base;
        const auto direction = steps > 0 ? 1 : -1;
        auto left = std::abs (steps);

        for (int guard = 0; left > 0 && guard < 60; ++guard)
        {
            target += direction;

            if (inScale (target))
                --left;
        }

        return target - note;
    }

    /** Semitones from the note to the chord note steps away (passing the note's own). */
    int chordSteps (int note, juce::uint16 chord, int steps) noexcept
    {
        auto target = note;
        const auto direction = steps > 0 ? 1 : -1;
        auto left = std::abs (steps);

        for (int guard = 0; left > 0 && guard < 60; ++guard)
        {
            target += direction;

            if (hasPitchClass (chord, target))
                --left;
        }

        return target - note;
    }
}

float Choir::partSemitones (const ChoirSettings::Voice& voice, juce::int64 songSample) noexcept
{
    const auto octave = 12.0f * static_cast<float> (voice.octave);

    if (voice.mode == ChoirSettings::Mode::semitones || voice.interval == 0)
        return octave + static_cast<float> (voice.mode == ChoirSettings::Mode::semitones ? voice.interval : 0);

    // The key and the chord where the shifter listens now.
    int root = 0;
    juce::uint16 scale = 0x0ab5, chord = 0;

    if (timeline != nullptr)
    {
        const auto& keys = timeline->keys;
        auto key = std::upper_bound (keys.begin(), keys.end(), songSample, [] (juce::int64 s, const Timeline::Key& k) { return s < k.sample; });

        if (key != keys.begin())
        {
            --key;
            root = key->root;
            scale = key->notes;
        }
        else if (! keys.empty())
        {
            root = keys.front().root;
            scale = keys.front().notes;
        }

        const auto& chords = timeline->chords;
        auto found = std::upper_bound (chords.begin(), chords.end(), songSample, [] (juce::int64 s, const Timeline::Chord& c) { return s < c.sample; });

        if (found != chords.begin())
            chord = std::prev (found)->notes;
    }

    // Before any note is heard: as if the key's root were sung.
    const auto note = heldNote >= 0.0f ? static_cast<int> (heldNote) : 60 + root;

    if (voice.mode == ChoirSettings::Mode::chord)
    {
        if (bitCount (chord) >= 2)
            return octave + static_cast<float> (chordSteps (note, chord, voice.interval));

        // No chords there: thirds of the key.
        return octave + static_cast<float> (scaleSteps (note, root, scale, juce::jlimit (-7, 7, voice.interval * 2)));
    }

    return octave + static_cast<float> (scaleSteps (note, root, scale, voice.interval));
}

void Choir::setVoiceParameters (VoiceDsp& voice, int index, float semitones) noexcept
{
    const auto& part = settings.voices[static_cast<size_t> (index)];

    // A woman's voice from a man's has its resonances higher (about three semitones), and the
    // other way round; its character and the singer's own make a little more of a difference.
    auto formant = part.female == settings.leadFemale ? 0.0f : (part.female ? 3.0f : -3.0f);
    formant += part.character * 2.5f + voice.formantJitter;

    const auto vibrato = settings.vibrato * 0.3f * std::sin (voice.vibratoPhase);
    const auto drift = settings.pitch * 0.15f * voice.pitchDrift;

#if SONIFY_CHOIR_STRETCH
    // Above 8 kHz (the breath and the s's) the sound isn't moved, which keeps it natural.
    voice.stretch.setTransposeSemitones (semitones + vibrato + drift, static_cast<float> (8000.0 / sampleRate));
    voice.stretch.setFormantSemitones (formant, true);

    // The note sung tells it where the voice's harmonics are, so it finds the resonances cleanly
    // (its own guess can mistake them and leave stray tones).
    voice.stretch.setFormantBase (sungPitch > 0.0f ? static_cast<float> (440.0 * std::pow (2.0, (sungPitch - 69.0) / 12.0) / sampleRate) : 0.0f);
#else
    juce::ignoreUnused (semitones, vibrato, drift, formant);
#endif
}

//==============================================================================
// Playing
void Choir::start (juce::int64 songSample) noexcept
{
    spanCount = 0;
    fedIndex = outIndex = 0;
    feedSong = songSample;
    heldNote = -1.0f;
    sungPitch = -1.0f;
    tracker->reset();
    rolling = true;

    if (lookahead > 0)
        feed (startBuffer.data(), lookahead);

    for (auto& voice : voices)
        if (voice != nullptr)
            voice->stale = true;

    hall.reset();
}

void Choir::startVoice (VoiceDsp& voice, int index) noexcept
{
#if SONIFY_CHOIR_STRETCH
    // What the shifter needs from where the song is: the sound already read ahead.
    readFed (startBuffer.data(), outIndex, lookahead);
    setVoiceParameters (voice, index, partSemitones (settings.voices[static_cast<size_t> (index)], songAt (outIndex)));
    const float* in = startBuffer.data();
    voice.stretch.outputSeek (&in, lookahead);
#else
    juce::ignoreUnused (index);
#endif

    std::fill (voice.delayLine.begin(), voice.delayLine.end(), 0.0f);
    voice.delayWrite = 0;
    voice.delaySamples = -1.0f;
    voice.lastLeft = voice.lastRight = 0.0f;
    voice.stale = false;
}

void Choir::process (const AudioPlaylist* list, const Transport& transport, float* left, float* right, int numSamples) noexcept
{
    if (numSamples <= 0 || left == nullptr || right == nullptr || sampleRate <= 0.0)
        return;

    reading = list;

    // The vocal itself, at the lead's level.
    {
        const auto target = settings.leadDb <= ChoirSettings::leadOff ? 0.0f : juce::Decibels::decibelsToGain (settings.leadDb);
        const auto step = (target - leadGain) / static_cast<float> (numSamples);
        auto gain = leadGain;

        if (! juce::exactlyEqual (target, 1.0f) || ! juce::exactlyEqual (leadGain, 1.0f))
        {
            for (int i = 0; i < numSamples; ++i)
            {
                gain += step;
                left[i] *= gain;
                right[i] *= gain;
            }
        }

        leadGain = target;
    }

    juce::int64 loopFrom = 0, loopTo = 0;
    loopOn = transport.getLoopSamples (loopFrom, loopTo);
    loopStart = loopFrom;
    loopEnd = loopTo;

    const auto segments = transport.getNumSegments();

    if (segments == 0 || settings.count <= 0)
    {
        rolling = false;
        return;
    }

    startsLeft = startsPerBlock;

    for (int s = 0; s < segments; ++s)
    {
        const auto& segment = transport.getSegment (s);
        auto song = segment.sample;
        auto offset = segment.offset;
        auto length = juce::jmin (segment.length, numSamples - offset);

        while (length > 0)
        {
            if (lookahead > 0)
            {
                if (! rolling || songAt (outIndex) != song)
                    start (song);
            }
            else
            {
                // Without a shifter there's nothing ahead to keep: the song is read where it plays.
                if (! rolling || feedSong != song)
                {
                    rolling = true;
                    feedSong = song;
                    spanCount = 0;
                    fedIndex = outIndex;
                }
            }

            auto chunk = juce::jmin (length, maxChunk);

            if (lookahead > 0)
                chunk = static_cast<int> (juce::jmin (static_cast<juce::int64> (chunk), spanLeft (outIndex)));

            if (chunk <= 0)
            {
                start (song);
                continue;
            }

            renderChunk (left + offset, right + offset, chunk);
            outIndex += chunk;
            song += chunk;
            offset += chunk;
            length -= chunk;
            dropSpansBefore (outIndex);
        }
    }
}

void Choir::renderChunk (float* left, float* right, int n) noexcept
{
    auto* in = feedBuffer.data();
    feed (in, n);

    // The note sung where the shifters listen now, held while it wavers less than a semitone.
    const auto listening = fedIndex - analysisDelay - n / 2;

    if (const auto pitch = tracker->pitchAt (listening); pitch > 0.0f)
    {
        sungPitch = pitch;

        if (heldNote < 0.0f || std::abs (pitch - heldNote) > 0.75f)
            heldNote = static_cast<float> (std::lround (pitch));
    }

    const auto songNow = songAt (listening);
    std::fill (busLeft.begin(), busLeft.begin() + n, 0.0f);
    std::fill (busRight.begin(), busRight.begin() + n, 0.0f);

    const auto count = juce::jlimit (0, ChoirSettings::maxVoices, settings.count);
    bool anySolo = false;
    int audible = 0;

    for (int i = 0; i < count; ++i)
        anySolo = anySolo || settings.voices[static_cast<size_t> (i)].solo;

    for (int i = 0; i < count; ++i)
    {
        const auto& part = settings.voices[static_cast<size_t> (i)];
        audible += (! part.mute && (! anySolo || part.solo)) ? 1 : 0;
    }

    // More voices sound fuller, not louder.
    const auto ensemble = 1.0f / std::sqrt (static_cast<float> (juce::jmax (1, audible)));
    const auto chunkSeconds = static_cast<float> (n / sampleRate);

    for (int i = 0; i < count; ++i)
    {
        auto* voice = voices[static_cast<size_t> (i)].get();

        if (voice == nullptr || ! voice->ready)
            continue;

        if (voice->stale)
        {
            if (startsLeft <= 0)
                continue;   // it comes in a block later

            startVoice (*voice, i);
            --startsLeft;
        }

        const auto& part = settings.voices[static_cast<size_t> (i)];

        // Humanize: slow drifts toward new targets now and then, and the vibrato's swing.
        if ((voice->driftCountdown -= n) <= 0)
        {
            voice->driftCountdown = static_cast<int> ((0.4 + voice->random.nextFloat() * 0.9) * sampleRate);
            voice->pitchTarget = voice->random.nextFloat() * 2.0f - 1.0f;
            voice->timeTarget = voice->random.nextFloat() * 2.0f - 1.0f;
        }

        const auto follow = juce::jmin (1.0f, chunkSeconds / 0.35f);
        voice->pitchDrift += (voice->pitchTarget - voice->pitchDrift) * follow;
        voice->timeDrift += (voice->timeTarget - voice->timeDrift) * follow;
        voice->vibratoPhase += juce::MathConstants<float>::twoPi * voice->vibratoRate * chunkSeconds;

        if (voice->vibratoPhase > juce::MathConstants<float>::twoPi)
            voice->vibratoPhase -= juce::MathConstants<float>::twoPi;

        setVoiceParameters (*voice, i, partSemitones (part, songNow));

        auto* sung = voice->out.data();

    #if SONIFY_CHOIR_STRETCH
        const float* input = in;
        voice->stretch.process (&input, n, &sung, n);
    #else
        std::copy (in, in + n, sung);
    #endif

        // Each singer a little late or early (around the beat, never far), drifting slowly.
        const auto delayTarget = static_cast<float> (sampleRate * 0.001) * settings.timing * juce::jmax (0.0f, voice->baseDelayMs + 6.0f * voice->timeDrift);
        auto& line = voice->delayLine;
        const auto mask = static_cast<int> (line.size()) - 1;
        const auto maxDelay = static_cast<float> (line.size() - static_cast<size_t> (maxChunk) - 4);

        if (voice->delaySamples < 0.0f)
            voice->delaySamples = delayTarget;

        // Where it sits in the stereo, and how loud.
        const auto heard = ! part.mute && (! anySolo || part.solo);
        const auto gain = heard ? juce::Decibels::decibelsToGain (part.levelDb) * ensemble : 0.0f;
        const auto pan = juce::jlimit (-1.0f, 1.0f, part.pan * settings.spread);
        const auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const auto targetLeft = gain * juce::MathConstants<float>::sqrt2 * std::cos (angle);
        const auto targetRight = gain * juce::MathConstants<float>::sqrt2 * std::sin (angle);
        const auto stepLeft = (targetLeft - voice->lastLeft) / static_cast<float> (n);
        const auto stepRight = (targetRight - voice->lastRight) / static_cast<float> (n);
        auto gainLeft = voice->lastLeft, gainRight = voice->lastRight;

        for (int k = 0; k < n; ++k)
        {
            line[static_cast<size_t> (voice->delayWrite & mask)] = sung[k];
            voice->delaySamples += (delayTarget - voice->delaySamples) * 0.0005f;
            const auto delay = juce::jlimit (0.0f, maxDelay, voice->delaySamples);

            // Read behind the write head, counted from it: the fraction comes from the delay alone,
            // so it stays exact however long the song plays (the head wraps round the line).
            const auto back = static_cast<int> (std::ceil (delay));
            const auto fraction = static_cast<float> (back) - delay;
            const auto whole = (voice->delayWrite - back) & mask;
            const auto a = line[static_cast<size_t> (whole)];
            const auto b = line[static_cast<size_t> ((whole + 1) & mask)];
            const auto sample = a + (b - a) * fraction;
            voice->delayWrite = (voice->delayWrite + 1) & mask;

            gainLeft += stepLeft;
            gainRight += stepRight;
            busLeft[static_cast<size_t> (k)] += sample * gainLeft;
            busRight[static_cast<size_t> (k)] += sample * gainRight;
        }

        voice->lastLeft = targetLeft;
        voice->lastRight = targetRight;
    }

    // Voices no longer in use start afresh when they come back.
    for (int i = count; i < ChoirSettings::maxVoices; ++i)
        if (auto* voice = voices[static_cast<size_t> (i)].get())
            voice->stale = true;

    // The hall, then the choir into the track.
    const auto wet = settings.hall * 0.55f;

    if (! juce::exactlyEqual (wet, hallParameters.wetLevel))
    {
        hallParameters.wetLevel = wet;
        hallParameters.dryLevel = 1.0f - settings.hall * 0.3f;
        hall.setParameters (hallParameters);
    }

    if (settings.hall > 0.001f)
        hall.processStereo (busLeft.data(), busRight.data(), n);

    const auto target = juce::Decibels::decibelsToGain (settings.choirDb);
    const auto step = (target - choirGain) / static_cast<float> (n);
    auto gain = choirGain;

    for (int k = 0; k < n; ++k)
    {
        gain += step;
        left[k] += busLeft[static_cast<size_t> (k)] * gain;
        right[k] += busRight[static_cast<size_t> (k)] * gain;
    }

    choirGain = target;
}
