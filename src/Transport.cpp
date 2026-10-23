// D:\workspace\Fanan_web_hub\src\Transport.cpp

#include "Transport.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double clickLengthSeconds = 0.06;
    constexpr double clickAttackSeconds = 0.0004;
    constexpr double clickDecaySeconds = 0.010;
    constexpr double accentFrequency = 1500.0;
    constexpr double beatFrequency = 1000.0;

    clap_beattime toBeatTime (double quarterNotes) noexcept
    {
        return static_cast<clap_beattime> (std::llround (quarterNotes * static_cast<double> (CLAP_BEATTIME_FACTOR)));
    }

    clap_sectime toSecTime (double seconds) noexcept
    {
        return static_cast<clap_sectime> (std::llround (seconds * static_cast<double> (CLAP_SECTIME_FACTOR)));
    }
}

//==============================================================================
Transport::Transport()
    : audioMap (std::make_unique<TempoMap>())
{
}

void Transport::setPlaying (bool shouldPlay) noexcept
{
    if (! shouldPlay)
        recordSetting.store (false);   // stopping ends recording

    requestedPlaying.store (shouldPlay ? 1 : 0);
}

void Transport::togglePlaying() noexcept
{
    // A request the audio thread hasn't picked up yet counts as the current state,
    // so two quick toggles cancel out instead of both reading "stopped".
    const auto pending = requestedPlaying.load();
    const bool current = pending >= 0 ? pending == 1 : playingNow.load();
    setPlaying (! current);
}

void Transport::setRecording (bool shouldRecord) noexcept
{
    recordSetting.store (shouldRecord);
}

void Transport::startRecording() noexcept
{
    // From stopped (a request to play not picked up yet counts as playing): the audio thread
    // starts it, with the count-in and the preroll.
    const auto pending = requestedPlaying.load();
    const bool playingAlready = pending >= 0 ? pending == 1 : playingNow.load();

    recordSetting.store (true);

    if (! playingAlready)
        requestedRecordStart.store (true);
}

void Transport::setRecordOptions (const RecordOptions& options) noexcept
{
    precountSetting.store (options.precount);
    prerollSetting.store (options.preroll);
    postrollSetting.store (options.postroll);
    autopunchSetting.store (options.autopunch);
    precountBarsSetting.store (juce::jlimit (1, maxRollBars, options.precountBars));
    prerollBarsSetting.store (juce::jlimit (1, maxRollBars, options.prerollBars));
    postrollBarsSetting.store (juce::jlimit (1, maxRollBars, options.postrollBars));
}

juce::var Transport::recordOptionsToVar (const RecordOptions& options)
{
    juce::DynamicObject::Ptr object (new juce::DynamicObject());
    object->setProperty ("precount", options.precount);
    object->setProperty ("precountBars", options.precountBars);
    object->setProperty ("preroll", options.preroll);
    object->setProperty ("prerollBars", options.prerollBars);
    object->setProperty ("postroll", options.postroll);
    object->setProperty ("postrollBars", options.postrollBars);
    object->setProperty ("autopunch", options.autopunch);
    return juce::var (object.get());
}

Transport::RecordOptions Transport::recordOptionsFromVar (const juce::var& value, RecordOptions options)
{
    const auto flag = [&value] (const char* name, bool& field)
    {
        if (const auto v = value.getProperty (name, {}); v.isBool() || v.isInt())
            field = static_cast<bool> (v);
    };

    const auto bars = [&value] (const char* name, int& field)
    {
        if (const auto v = value.getProperty (name, {}); v.isInt() || v.isInt64() || v.isDouble())
            field = juce::jlimit (1, maxRollBars, static_cast<int> (v));
    };

    flag ("precount", options.precount);
    flag ("preroll", options.preroll);
    flag ("postroll", options.postroll);
    flag ("autopunch", options.autopunch);
    bars ("precountBars", options.precountBars);
    bars ("prerollBars", options.prerollBars);
    bars ("postrollBars", options.postrollBars);
    return options;
}

Transport::RecordOptions Transport::getRecordOptions() const noexcept
{
    RecordOptions options;
    options.precount = precountSetting.load();
    options.preroll = prerollSetting.load();
    options.postroll = postrollSetting.load();
    options.autopunch = autopunchSetting.load();
    options.precountBars = precountBarsSetting.load();
    options.prerollBars = prerollBarsSetting.load();
    options.postrollBars = postrollBarsSetting.load();
    return options;
}

void Transport::locate (double ppq) noexcept
{
    requestedLocate.store (juce::jmax (0.0, ppq));
}

void Transport::setLoop (bool enabled, double startPpq, double endPpq) noexcept
{
    if (! std::isfinite (startPpq) || ! std::isfinite (endPpq))
        return;

    startPpq = juce::jlimit (0.0, maxPosition - minLoopLength, startPpq);
    endPpq = juce::jlimit (startPpq + minLoopLength, maxPosition, endPpq);

    loopStartSetting.store (startPpq);
    loopEndSetting.store (endPpq);
    loopSetting.store (enabled);
}

void Transport::setMetronome (bool enabled, float volume) noexcept
{
    if (std::isfinite (volume))
        metronomeVolumeSetting.store (juce::jlimit (0.0f, 1.0f, volume));

    metronomeSetting.store (enabled);
}

//==============================================================================
void Transport::setTempoMap (const TempoMap& map)
{
    if (map == songMap)
        return;

    songMap = map;
    auto fresh = std::make_unique<TempoMap> (map);   // made here: nothing allocates under the lock

    {
        const juce::SpinLock::ScopedLockType lock (mapLock);
        std::swap (audioMap, fresh);
        mapGeneration.fetch_add (1);
    }
}   // the old copy goes here, outside the lock

void Transport::setTempo (double bpm)
{
    if (std::isfinite (bpm))
        setTempoMap (songMap.withTempoAt (positionNow.load(), bpm));
}

void Transport::setTimeSignature (int numerator, int denominator)
{
    if (isValidDenominator (denominator))
        setTempoMap (songMap.withSignatureAt (positionNow.load(), juce::jlimit (1, 32, numerator), denominator));
}

Transport::State Transport::getState() const noexcept
{
    State state;
    state.playing = playingNow.load();
    state.ppq = positionNow.load();
    state.loopEnabled = loopSetting.load();
    state.loopStart = loopStartSetting.load();
    state.loopEnd = loopEndSetting.load();
    state.metronomeEnabled = metronomeSetting.load();
    state.metronomeVolume = metronomeVolumeSetting.load();
    state.recording = recordSetting.load();
    state.capturing = capturingNow.load();
    state.counting = countingNow.load();
    state.countBeatsLeft = countBeatsNow.load();

    // Where the song is, in the map's terms.
    const auto bar = songMap.barAt (state.ppq);
    state.bpm = songMap.bpmAt (state.ppq);
    state.numerator = bar.numerator;
    state.denominator = bar.denominator;
    state.seconds = songMap.secondsAt (state.ppq);
    state.bar = bar.index;
    state.barStart = bar.start;
    state.barLength = bar.length();
    state.previousBarStart = bar.index > 0 ? songMap.ppqOfBar (bar.index - 1) : 0.0;
    return state;
}

//==============================================================================
void Transport::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    clickSamplesLeft = 0;
    numClicks = 0;
    numPending = 0;
    plannedGeneration = 0xffffffff;   // song time is counted afresh at the new rate
}

void Transport::beginBlock (clap_event_transport_t& out, int numSamples) noexcept
{
    const auto locateTo = requestedLocate.exchange (std::numeric_limits<double>::quiet_NaN());
    bool moved = false;

    if (! std::isnan (locateTo))
    {
        position = locateTo;
        moved = true;
        numPending = 0;   // the clicks still waiting (plugin delay compensation) were for where it was
    }

    const auto request = requestedPlaying.exchange (-1);

    if (request >= 0)
    {
        playing = request == 1;
        counting = false;   // stop (or play) ends a count-in

        if (! playing)
            recordFrom = -1.0;
    }

    loopEnabled = loopSetting.load();
    loopStart = loopStartSetting.load();
    loopEnd = loopEndSetting.load();
    metronomeEnabled = metronomeSetting.load();
    metronomeVolume = metronomeVolumeSetting.load();
    recording = recordSetting.load();

    // Recording off (punched out): it starts anywhere again next time.
    if (! recording)
        recordFrom = -1.0;

    // The map is borrowed while the block is planned and described: a new one waits until then.
    const juce::SpinLock::ScopedLockType lock (mapLock);
    const auto& map = *audioMap;
    const auto generation = mapGeneration.load();

    // Record from stopped: the preroll moves the song back, the count-in holds it.
    if (requestedRecordStart.exchange (false) && ! playing && recording)
    {
        startRecordingFromStopped (map);
        moved = true;
    }

    // Postroll: past the punch out by its bars, the song stops (Autopunch, the loop not playing round).
    if (playing && recording && autopunchSetting.load() && postrollSetting.load() && ! loopActive() && loopEnd > loopStart)
    {
        // As many bars past the punch out as it says (bars as long as they are there).
        const auto outBar = map.barAt (loopEnd + 1.0e-9);
        const auto stopAt = loopEnd + (map.ppqOfBar (outBar.index + postrollBarsSetting.load()) - outBar.start);

        if (position >= stopAt - 1.0e-9)
        {
            playing = false;
            recording = false;
            recordSetting.store (false);
            recordFrom = -1.0;
        }
    }

    playingNow.store (playing);
    positionNow.store (position);

    // Song time follows the position when it jumps or the map changes (the music stays where it is).
    if (moved || ! playing || generation != plannedGeneration)
    {
        songSample = sampleRate > 0.0 ? static_cast<juce::int64> (std::llround (map.secondsAt (position) * sampleRate)) : 0;
        plannedGeneration = generation;
    }

    planBlock (map, numSamples);
    collectMarks (map);

    if (counting)
        countIn (map, numSamples);

    decideCapturing (map);

    // Describe the block start for plugins (tempo-synced effects, arpeggiators and so on).
    const auto bpm = map.bpmAt (position);
    const auto bar = map.barAt (position);

    out = {};
    out.header.size = static_cast<uint32_t> (sizeof (clap_event_transport_t));
    out.header.time = 0;
    out.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    out.header.type = CLAP_EVENT_TRANSPORT;
    out.header.flags = 0;

    out.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE
              | CLAP_TRANSPORT_HAS_SECONDS_TIMELINE | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;

    if (playing)
        out.flags |= CLAP_TRANSPORT_IS_PLAYING;

    if (loopActive())
        out.flags |= CLAP_TRANSPORT_IS_LOOP_ACTIVE;

    out.song_pos_beats = toBeatTime (position);
    out.song_pos_seconds = toSecTime (map.secondsAt (position));
    out.tempo = bpm;

    // A ramp: how much the tempo moves each sample through this block.
    out.tempo_inc = numSegments == 1 && numSamples > 0 ? (map.bpmAt (segments[0].to) - bpm) / numSamples : 0.0;

    out.loop_start_beats = toBeatTime (loopStart);
    out.loop_end_beats = toBeatTime (loopEnd);
    out.loop_start_seconds = toSecTime (map.secondsAt (loopStart));
    out.loop_end_seconds = toSecTime (map.secondsAt (loopEnd));
    out.bar_start = toBeatTime (bar.start);
    out.bar_number = static_cast<int32_t> (bar.index);
    out.tsig_num = static_cast<uint16_t> (bar.numerator);
    out.tsig_denom = static_cast<uint16_t> (bar.denominator);
}

void Transport::endBlock (float* left, float* right, int numSamples) noexcept
{
    if (numSegments > 0)
    {
        position = plannedEnd;
        songSample = plannedEndSample;
    }

    renderClicks (left, right, numSamples);
    positionNow.store (position);
}

//==============================================================================
void Transport::startRecordingFromStopped (const TempoMap& map) noexcept
{
    const auto from = position;
    recordFrom = from;

    // Preroll: that many bars before (at the same place in its bar), or the song's start.
    if (prerollSetting.load())
    {
        const auto bar = map.barAt (from + 1.0e-9);
        const auto bars = prerollBarsSetting.load();
        position = bar.index >= bars ? map.ppqOfBar (bar.index - bars) + (from - bar.start) : 0.0;
    }

    if (! precountSetting.load() || sampleRate <= 0.0)
    {
        playing = true;
        return;
    }

    // Precount: its bars at the tempo and signature where the song starts. Its first click waits a
    // moment, so the last beat ends on a block's edge and the song starts on the beat.
    const auto bar = map.barAt (position + 1.0e-9);
    const auto beatQuarters = bar.beat();
    countNumerator = juce::jmax (1, bar.numerator);
    countBeats = countNumerator * precountBarsSetting.load();
    countBeatSamples = beatQuarters * 60.0 / map.bpmAt (position) * sampleRate;
    countLength = static_cast<juce::int64> (std::llround (countBeats * countBeatSamples));
    countSample = 0;
    countStarted = false;
    counting = countLength > 0;
    playing = ! counting;
}

void Transport::countIn (const TempoMap&, int numSamples) noexcept
{
    // The first block: the count starts late enough to end on a block's edge (for a steady block size).
    if (! countStarted && numSamples > 0)
    {
        countStarted = true;
        countSample = -((static_cast<juce::int64> (numSamples) - countLength % numSamples) % numSamples);
    }

    numClicks = 0;

    for (int beat = 0; beat < countBeats && numClicks < maxClicksPerBlock; ++beat)
    {
        const auto at = static_cast<juce::int64> (std::llround (beat * countBeatSamples));

        if (at >= countSample && at < countSample + numSamples)
            clicks[static_cast<size_t> (numClicks++)] = { static_cast<int> (at - countSample), beat % countNumerator == 0 };
    }

    const auto playedTo = countSample + numSamples;
    const auto heard = juce::jmax<juce::int64> (0, playedTo);
    countBeatsNow.store (juce::jlimit (0, countBeats, countBeats - static_cast<int> (static_cast<double> (heard) / countBeatSamples)));
    countSample = playedTo;

    // Counted in: the song starts with the next block.
    if (countSample >= countLength)
    {
        counting = false;
        playing = true;
        countBeatsNow.store (0);
    }
}

void Transport::decideCapturing (const TempoMap&) noexcept
{
    capturing = false;

    if (playing && recording && numSegments > 0)
    {
        const auto punch = autopunchSetting.load() && loopEnd > loopStart;

        for (int i = 0; i < numSegments && ! capturing; ++i)
        {
            const auto& segment = segments[static_cast<size_t> (i)];
            const bool pastStart = recordFrom < 0.0 || segment.to > recordFrom;
            const bool inside = ! punch || (segment.to > loopStart && segment.from < loopEnd);
            capturing = pastStart && inside;
        }

        // Recording has begun: the loop going round records all of it.
        if (capturing)
            recordFrom = -1.0;
    }

    capturingNow.store (capturing);
    countingNow.store (counting);

    if (! counting)
        countBeatsNow.store (0);
}

//==============================================================================
void Transport::planBlock (const TempoMap& map, int numSamples) noexcept
{
    numSegments = 0;
    numClicks = 0;
    loopSamplesOn = false;
    ppqPerSample = sampleRate > 0.0 ? map.bpmAt (position) / (60.0 * sampleRate) : 0.0;

    if (! playing || ppqPerSample <= 0.0 || numSamples <= 0)
        return;

    const auto looping = loopActive();

    // The loop in samples, as the jump below lands it: back to its start's sample when the song
    // reaches the first sample at or after its end.
    if (looping)
    {
        loopSamplesOn = true;
        loopFromSample = static_cast<juce::int64> (std::llround (map.secondsAt (loopStart) * sampleRate));
        loopToSample = static_cast<juce::int64> (std::ceil (map.secondsAt (loopEnd) * sampleRate - 1.0e-6));
    }

    const auto ppqAtSample = [&map, this] (juce::int64 sample) { return map.ppqAt (static_cast<double> (sample) / sampleRate); };
    auto from = position;
    auto fromSample = songSample;
    int offset = 0;
    int remaining = numSamples;

    // Each segment consumes at least one sample. Even the shortest loop allowed at the
    // fastest tempo fits a normal block in far fewer segments than the cap.
    while (remaining > 0 && numSegments < maxSegments)
    {
        const auto to = ppqAtSample (fromSample + remaining);
        auto& segment = segments[static_cast<size_t> (numSegments++)];

        if (looping && from < loopEnd && to >= loopEnd)
        {
            // The tolerance keeps a distance that is a whole number of samples (in exact
            // arithmetic) from rounding up one sample late.
            const auto loopEndSample = map.secondsAt (loopEnd) * sampleRate;
            const auto toEnd = juce::jlimit (1, remaining, static_cast<int> (std::ceil (loopEndSample - static_cast<double> (fromSample) - 1.0e-6)));
            segment = { from, loopEnd, offset, toEnd, (loopEnd - from) / toEnd, fromSample, static_cast<double> (fromSample) / sampleRate };
            offset += toEnd;
            remaining -= toEnd;
            from = loopStart;
            fromSample = static_cast<juce::int64> (std::llround (map.secondsAt (loopStart) * sampleRate));
        }
        else
        {
            segment = { from, to, offset, remaining, (to - from) / remaining, fromSample, static_cast<double> (fromSample) / sampleRate };
            from = to;
            fromSample += remaining;
            remaining = 0;
        }
    }

    // Out of segments (a pathological loop): the rest of the block continues without jumping.
    if (remaining > 0)
    {
        auto& last = segments[static_cast<size_t> (numSegments - 1)];
        last.length += remaining;
        fromSample = last.sample + last.length;
        from = last.to = ppqAtSample (fromSample);
        last.perSample = (last.to - last.from) / last.length;
    }

    plannedEnd = from;
    plannedEndSample = fromSample;
    ppqPerSample = segments[0].perSample;

    for (int i = 0; i < numSegments; ++i)
        collectClicks (map, segments[static_cast<size_t> (i)]);
}

void Transport::addMark (const TempoMap& map, double ppq, int segment, const TempoMap::Bar& bar, bool barLine) noexcept
{
    if (numMarks >= maxMarks)
        return;

    const auto key = map.keyAt (bar.start);
    auto& mark = marks[static_cast<size_t> (numMarks++)];
    mark.ppq = ppq;
    mark.segment = segment;
    mark.barLine = barLine;
    mark.bar = bar.index;
    mark.barLength = bar.length();
    mark.keyRoot = key.root;
    mark.keyNotes = TempoMap::scaleNotes (key.scale);
    mark.bpm = map.bpmAt (ppq);
}

int Transport::chordMarksFor (const TempoMap& map, double from, double to, int segment, ChordMark* out, int max) noexcept
{
    if (max <= 0)
        return 0;

    const auto& chords = map.getChords();
    int count = 0;

    const auto add = [&] (double ppq, const TempoMap::Chord* chord)
    {
        if (count >= max)
            return;

        auto& mark = out[count++];
        mark.ppq = ppq;
        mark.segment = segment;
        mark.root = chord != nullptr ? chord->root : -1;
        mark.notes = chord != nullptr ? TempoMap::chordNotes (chord->quality) : juce::uint16 (0);
        mark.bass = chord != nullptr ? chord->bass : -1;
    };

    // The one holding where it starts, then each change before its end.
    add (from, map.chordAt (from));

    auto next = std::upper_bound (chords.begin(), chords.end(), from + 1.0e-9, [] (double p, const TempoMap::Chord& c) { return p < c.ppq; });

    for (; next != chords.end() && next->ppq < to && count < max; ++next)
        add (next->ppq, &*next);

    return count;
}

void Transport::collectMarks (const TempoMap& map) noexcept
{
    numMarks = 0;
    numChordMarks = 0;

    // A position this close before a bar line is on it (the samples' rounding).
    constexpr double near = 1.0e-7;

    if (numSegments == 0)
    {
        const auto bar = map.barAt (position + near);
        addMark (map, position, -1, bar, std::abs (position - bar.start) < near);
        numChordMarks = chordMarksFor (map, position, position, -1, chordMarks.data(), maxChordMarks);
        return;
    }

    for (int s = 0; s < numSegments; ++s)
    {
        const auto& segment = segments[static_cast<size_t> (s)];
        numChordMarks += chordMarksFor (map, segment.from, segment.to, s, chordMarks.data() + numChordMarks, maxChordMarks - numChordMarks);
        auto bar = map.barAt (segment.from + near);
        addMark (map, segment.from, s, bar, std::abs (segment.from - bar.start) < near);

        // The bar lines after its start (marks run out long before a block could reach them).
        while (numMarks < maxMarks)
        {
            const auto next = bar.start + bar.length();

            if (next >= segment.to - near)
                break;

            const auto following = map.barAt (next + near);

            if (following.start < next - 1.0e-6)   // past the last bar the map counts
                break;

            addMark (map, next, s, following, true);
            bar = following;
        }
    }
}

void Transport::collectClicks (const TempoMap& map, const Segment& segment) noexcept
{
    if (! metronomeEnabled || segment.length <= 0 || segment.perSample <= 0.0)
        return;

    // Each beat of each bar the segment touches; the bar's first is the accent.
    auto bar = map.barAt (segment.from);

    while (bar.start < segment.to && numClicks < maxClicksPerBlock)
    {
        const auto beat = bar.beat();

        for (int index = 0; index < bar.numerator && numClicks < maxClicksPerBlock; ++index)
        {
            const auto at = bar.start + index * beat;

            if (at < segment.from - 1.0e-9 || at >= segment.to)
                continue;

            const auto sample = static_cast<int> (std::llround ((at - segment.from) / segment.perSample));
            clicks[static_cast<size_t> (numClicks++)] = { segment.offset + juce::jlimit (0, segment.length - 1, sample), index == 0 };
        }

        const auto next = bar.start + bar.length();
        bar = map.barAt (next);

        if (bar.start < next - 1.0e-6)   // past the last bar the map counts
            break;
    }
}

void Transport::startClick (bool accent) noexcept
{
    clickSamplesLeft = static_cast<int> (clickLengthSeconds * sampleRate);
    clickTime = 0.0;
    clickPhase = 0.0;
    clickPhaseStep = juce::MathConstants<double>::twoPi * (accent ? accentFrequency : beatFrequency) / sampleRate;
    clickGain = metronomeVolume * (accent ? 0.5f : 0.32f);
}

void Transport::renderClicks (float* left, float* right, int numSamples) noexcept
{
    // This block's clicks wait as long as the song is late (plugin delay compensation): in
    // order, after those already waiting.
    const auto delay = clicksOnTime ? 0 : clickDelay.load (std::memory_order_relaxed);

    for (int c = 0; c < numClicks && numPending < maxClicksPerBlock; ++c)
    {
        auto click = clicks[static_cast<size_t> (c)];
        click.sample += delay;
        int place = numPending;

        while (place > 0 && pendingClicks[static_cast<size_t> (place - 1)].sample > click.sample)
        {
            pendingClicks[static_cast<size_t> (place)] = pendingClicks[static_cast<size_t> (place - 1)];
            --place;
        }

        pendingClicks[static_cast<size_t> (place)] = click;
        ++numPending;
    }

    numClicks = 0;

    // Those due in this block start; the rest wait on, a block nearer.
    const auto finishBlock = [this, numSamples]
    {
        int kept = 0;

        for (int c = 0; c < numPending; ++c)
        {
            auto click = pendingClicks[static_cast<size_t> (c)];

            if (click.sample >= numSamples)
            {
                click.sample -= numSamples;
                pendingClicks[static_cast<size_t> (kept++)] = click;
            }
        }

        numPending = kept;
    };

    if (numPending == 0 && clickSamplesLeft <= 0)
        return;

    const auto timeStep = 1.0 / sampleRate;
    int nextClick = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        while (nextClick < numPending && pendingClicks[static_cast<size_t> (nextClick)].sample <= i)
            startClick (pendingClicks[static_cast<size_t> (nextClick++)].accent);

        if (clickSamplesLeft <= 0)
        {
            if (nextClick >= numPending || pendingClicks[static_cast<size_t> (nextClick)].sample >= numSamples)
            {
                finishBlock();
                return;
            }

            continue;
        }

        const auto envelope = (1.0 - std::exp (-clickTime / clickAttackSeconds)) * std::exp (-clickTime / clickDecaySeconds);
        const auto sample = static_cast<float> (clickGain * envelope * std::sin (clickPhase));

        if (left != nullptr)
            left[i] += sample;

        if (right != nullptr && right != left)
            right[i] += sample;

        clickPhase += clickPhaseStep;
        clickTime += timeStep;
        --clickSamplesLeft;
    }

    finishBlock();
}
