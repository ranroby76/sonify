// D:\workspace\Fanan_web_hub\src\MidiSync.cpp

#include "MidiSync.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double ticksPerQuarter = 24.0;   // MIDI clock
    constexpr double epsilon = 1.0e-9;
}

//==============================================================================
/** Messages for one block, written into the caller's array. */
struct MidiSync::Planner::Out
{
    Message* messages = nullptr;
    int capacity = 0;
    int count = 0;

    void add (double time, juce::uint32 targets, std::initializer_list<juce::uint8> bytes) noexcept
    {
        if (targets == 0 || count >= capacity)
            return;

        auto& message = messages[count++];
        message.time = time;
        message.targets = targets;
        message.size = 0;

        for (const auto byte : bytes)
            if (message.size < static_cast<int> (message.bytes.size()))
                message.bytes[static_cast<size_t> (message.size++)] = byte;
    }

    /** Song Position Pointer: where the song is, in sixteenth notes. */
    void songPosition (double time, juce::uint32 targets, double ppq) noexcept
    {
        const auto sixteenths = juce::jlimit (0, 16383, static_cast<int> (std::floor (ppq * 4.0 + 1.0e-6)));
        add (time, targets, { 0xf2, static_cast<juce::uint8> (sixteenths & 0x7f), static_cast<juce::uint8> ((sixteenths >> 7) & 0x7f) });
    }

    /** MTC full frame: the time as hours, minutes, seconds and frames, at once. */
    void fullFrame (double time, juce::uint32 targets, double seconds, Rate rate) noexcept
    {
        if (targets == 0)
            return;

        const auto frame = static_cast<juce::int64> (std::floor (juce::jmax (0.0, seconds) * framesPerSecond (rate) + 1.0e-6));
        const auto tc = timecodeOf (frame, rate);

        add (time, targets, { 0xf0, 0x7f, 0x7f, 0x01, 0x01,
                              static_cast<juce::uint8> ((static_cast<int> (rate) << 5) | (tc[0] & 0x1f)),
                              static_cast<juce::uint8> (tc[1]), static_cast<juce::uint8> (tc[2]), static_cast<juce::uint8> (tc[3]),
                              0xf7 });
    }

    /** In time order, keeping the order of messages due at the same moment (Start before the first tick). */
    void sort() noexcept
    {
        for (int i = 1; i < count; ++i)
        {
            const auto message = messages[i];
            int j = i - 1;

            while (j >= 0 && messages[j].time > message.time)
            {
                messages[j + 1] = messages[j];
                --j;
            }

            messages[j + 1] = message;
        }
    }
};

namespace
{
    /** A quarter-frame message's data byte. Each group of eight (two frames) spells out the time
        at its first frame, a nibble at a time, the last one with the frame rate. */
    juce::uint8 quarterFrameData (juce::int64 quarter, MidiSync::Rate rate) noexcept
    {
        const auto piece = static_cast<int> (quarter & 7);
        const auto tc = MidiSync::Planner::timecodeOf ((quarter >> 3) * 2, rate);
        int value = 0;

        switch (piece)
        {
            case 0:  value = tc[3] & 0x0f; break;
            case 1:  value = (tc[3] >> 4) & 0x01; break;
            case 2:  value = tc[2] & 0x0f; break;
            case 3:  value = (tc[2] >> 4) & 0x03; break;
            case 4:  value = tc[1] & 0x0f; break;
            case 5:  value = (tc[1] >> 4) & 0x03; break;
            case 6:  value = tc[0] & 0x0f; break;
            default: value = ((tc[0] >> 4) & 0x01) | (static_cast<int> (rate) << 1); break;
        }

        return static_cast<juce::uint8> ((piece << 4) | value);
    }
}

//==============================================================================
double MidiSync::Planner::framesPerSecond (Rate rate) noexcept
{
    switch (rate)
    {
        case Rate::fps24:        return 24.0;
        case Rate::fps25:        return 25.0;
        case Rate::fps2997drop:  return 30000.0 / 1001.0;
        case Rate::fps30:        return 30.0;
    }

    return 30.0;
}

std::array<int, 4> MidiSync::Planner::timecodeOf (juce::int64 frame, Rate rate) noexcept
{
    const int nominal = rate == Rate::fps24 ? 24 : rate == Rate::fps25 ? 25 : 30;
    frame = juce::jmax<juce::int64> (0, frame);

    // Drop frame: the numbers 00 and 01 are skipped at the start of every minute, except every tenth.
    if (rate == Rate::fps2997drop)
    {
        constexpr juce::int64 perTenMinutes = 17982, perMinute = 1798;
        const auto tens = frame / perTenMinutes;
        const auto rest = frame % perTenMinutes;
        frame += 18 * tens + (rest > 1 ? 2 * ((rest - 2) / perMinute) : 0);
    }

    const auto seconds = frame / nominal;
    return { static_cast<int> ((seconds / 3600) % 24), static_cast<int> ((seconds / 60) % 60),
             static_cast<int> (seconds % 60), static_cast<int> (frame % nominal) };
}

void MidiSync::Planner::resync (double ppq, double seconds, Rate rate) noexcept
{
    nextTick = static_cast<juce::int64> (std::ceil (ppq * ticksPerQuarter - 1.0e-6));
    nextQuarter = static_cast<juce::int64> (std::ceil (seconds * 4.0 * framesPerSecond (rate) - 1.0e-6));
    lastRate = rate;
}

void MidiSync::Planner::reset() noexcept
{
    running = false;
    runningClock = 0;
    toldStopped = -1.0;
    toldStoppedSeconds = -1.0;
}

int MidiSync::Planner::plan (const Transport& transport, double sampleRate, double blockTime, double delayMs,
                             juce::uint32 clock, juce::uint32 startOnly, juce::uint32 timecode, Rate rate,
                             Message* messages, int maxOut) noexcept
{
    Out out { messages, maxOut };

    if (sampleRate <= 0.0)
        return 0;

    const auto msPerSample = 1000.0 / sampleRate;
    const auto timeAt = [&] (double sampleOffset) { return blockTime + delayMs + sampleOffset * msPerSample; };
    const auto segments = transport.getNumSegments();

    if (segments == 0)
    {
        if (running)
        {
            out.add (timeAt (0.0), runningClock, { 0xfc });
            reset();
        }

        // Where the song is, each time it moves while stopped: the devices follow it there.
        const auto position = transport.getBlockStart();

        const auto seconds = transport.getBlockStartSeconds();

        if (std::abs (position - toldStopped) > epsilon || std::abs (seconds - toldStoppedSeconds) > 0.5 / sampleRate)
        {
            toldStopped = position;
            toldStoppedSeconds = seconds;
            out.songPosition (timeAt (0.0), clock & ~startOnly, position);
            out.fullFrame (timeAt (0.0), timecode, transport.getBlockStartSeconds(), rate);
        }

        return out.count;
    }

    const auto perSecond = 4.0 * framesPerSecond (rate);

    for (int i = 0; i < segments; ++i)
    {
        const auto& segment = transport.getSegment (i);
        const auto at = timeAt (segment.offset);
        const auto ppqPerSample = segment.perSample;   // the tempo may ramp: each stretch has its own
        const auto previousEnd = i > 0 ? transport.getSegment (i - 1).to : lastEnd;
        const auto jumped = std::abs (segment.from - previousEnd) > 1.0e-6;

        // The song's time jumps without the position moving when the tempo before it changes.
        const auto previousEndSeconds = i > 0 ? transport.getSegment (i - 1).seconds + transport.getSegment (i - 1).length / sampleRate : lastEndSeconds;
        const auto timeJumped = std::abs (segment.seconds - previousEndSeconds) > 0.5 / sampleRate;

        if (! running)
        {
            // From the top: Start. From anywhere else: where, then Continue (unless a device takes Start only).
            const auto fromTop = segment.from < 1.0e-6;
            const auto starters = clock & (fromTop ? clock : startOnly);
            const auto continuers = clock & ~starters;

            // A device counts from the first clock after Start or Continue, and Song Position names
            // a sixteenth: the clock begins on the next sixteenth, the one Song Position names.
            const auto sixteenth = static_cast<juce::int64> (std::ceil (segment.from * 4.0 - 1.0e-6));

            out.songPosition (at, continuers, static_cast<double> (sixteenth) / 4.0);
            out.add (at, continuers, { 0xfb });
            out.add (at, starters, { 0xfa });
            out.fullFrame (at, timecode, segment.seconds, rate);

            running = true;
            runningClock = clock;
            resync (segment.from, segment.seconds, rate);
            nextTick = sixteenth * 6;
        }
        else if (jumped || timeJumped || rate != lastRate)
        {
            // The loop went round (or the frame rate changed, or a tempo before the song position
            // did): where the song is now. The clock follows the quarter notes, the timecode the time.
            if (jumped)
                out.songPosition (at, runningClock, segment.from);

            out.fullFrame (at, timecode, segment.seconds, rate);
            resync (segment.from, segment.seconds, rate);
        }

        // Clock: 24 ticks a quarter note, tick k at k/24.
        for (; static_cast<double> (nextTick) < segment.to * ticksPerQuarter - epsilon && out.count < maxOut; ++nextTick)
            out.add (timeAt (segment.offset + (static_cast<double> (nextTick) / ticksPerQuarter - segment.from) / ppqPerSample),
                     runningClock, { 0xf8 });

        // Timecode: four quarter frames a frame, on the song's own time.
        const auto secondsFrom = segment.seconds;
        const auto secondsTo = segment.seconds + segment.length / sampleRate;

        for (; static_cast<double> (nextQuarter) < secondsTo * perSecond - epsilon && out.count < maxOut; ++nextQuarter)
        {
            const auto seconds = static_cast<double> (nextQuarter) / perSecond;
            out.add (timeAt (segment.offset + (seconds - secondsFrom) * sampleRate), timecode, { 0xf1, quarterFrameData (nextQuarter, rate) });
        }
    }

    lastEnd = transport.getSegment (segments - 1).to;
    lastEndSeconds = transport.getSegment (segments - 1).seconds + transport.getSegment (segments - 1).length / sampleRate;
    out.sort();
    return out.count;
}

int MidiSync::Planner::halt (double now, Message* messages, int maxOut) noexcept
{
    Out out { messages, maxOut };

    if (running)
    {
        out.add (now, runningClock, { 0xfc });
        reset();
    }

    return out.count;
}

//==============================================================================
MidiSync::MidiSync()
    : juce::Thread ("Sonify MIDI sync")
{
    startThread (juce::Thread::Priority::highest);
}

MidiSync::~MidiSync()
{
    stopThread (2000);

    // Whatever is still queued (a Stop on the way out) goes now.
    for (;;)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        queue.prepareToRead (1, start1, size1, start2, size2);

        if (size1 <= 0)
            break;

        const auto message = queued[static_cast<size_t> (start1)];
        queue.finishedRead (1);
        send (message);
    }
}

MidiSync::Rate MidiSync::rateFromName (const juce::String& name) noexcept
{
    if (name == "24")     return Rate::fps24;
    if (name == "25")     return Rate::fps25;
    if (name == "29.97")  return Rate::fps2997drop;
    return Rate::fps30;
}

void MidiSync::setTargets (const std::vector<Target>& targets, Rate rate)
{
    // One output per port, whatever the devices on it ask for.
    std::vector<Target> merged;

    for (const auto& target : targets)
    {
        if (target.output.isEmpty() || ! (target.clock || target.timecode))
            continue;

        auto existing = std::find_if (merged.begin(), merged.end(), [&] (const Target& t) { return t.output == target.output; });

        if (existing == merged.end())
        {
            if (merged.size() < static_cast<size_t> (maxTargets))
                merged.push_back (target);

            continue;
        }

        existing->clock = existing->clock || target.clock;
        existing->clockStart = existing->clockStart || (target.clock && target.clockStart);
        existing->timecode = existing->timecode || target.timecode;
    }

    // Nothing more is queued for the outputs while they change.
    const auto wasClocked = clockOutputs.exchange (0);
    startOnlyOutputs.store (0);
    timecodeOutputs.store (0);

    juce::uint32 clock = 0, startOnly = 0, timecode = 0;
    const auto available = juce::MidiOutput::getAvailableDevices();

    {
        const juce::ScopedLock sl (outputLock);
        problems.clear();

        std::array<std::unique_ptr<juce::MidiOutput>, maxTargets> kept;
        std::array<juce::String, maxTargets> keptNames;
        std::array<bool, maxTargets> wasOpen {};

        for (size_t j = 0; j < outputs.size(); ++j)
            wasOpen[j] = outputs[j] != nullptr;

        const auto place = [&] (size_t j, std::unique_ptr<juce::MidiOutput> output, const Target& target)
        {
            kept[j] = std::move (output);
            keptNames[j] = target.output;

            if (target.clock)                       clock |= 1u << j;
            if (target.clock && target.clockStart)  startOnly |= 1u << j;
            if (target.timecode)                    timecode |= 1u << j;
        };

        // A port already open stays open, in its place, so what's already queued for it still
        // reaches it (and if its clock is turned off, it hears Stop).
        for (const auto& target : merged)
        {
            for (size_t j = 0; j < outputs.size(); ++j)
            {
                if (outputs[j] == nullptr || outputNames[j] != target.output)
                    continue;

                if ((wasClocked & (1u << j)) != 0 && ! target.clock)
                    outputs[j]->sendMessageNow (juce::MidiMessage::midiStop());

                place (j, std::move (outputs[j]), target);
                break;
            }
        }

        // New ports open, in places nothing was queued for.
        for (const auto& target : merged)
        {
            if (std::find (keptNames.begin(), keptNames.end(), target.output) != keptNames.end())
                continue;

            std::unique_ptr<juce::MidiOutput> output;

            for (const auto& info : available)
                if (output == nullptr && info.name == target.output)
                    output = juce::MidiOutput::openDevice (info.identifier);

            if (output == nullptr)
            {
                problems.add (target.output);
                continue;
            }

            auto slot = kept.size();

            for (size_t j = 0; j < kept.size() && slot == kept.size(); ++j)
                if (kept[j] == nullptr && ! wasOpen[j])
                    slot = j;

            for (size_t j = 0; j < kept.size() && slot == kept.size(); ++j)
                if (kept[j] == nullptr)
                    slot = j;

            if (slot < kept.size())
                place (slot, std::move (output), target);
        }

        // The ports nobody sends to any more close; one still running a clock hears Stop first.
        for (size_t j = 0; j < outputs.size(); ++j)
        {
            if (outputs[j] == nullptr)
                continue;

            if ((wasClocked & (1u << j)) != 0)
                outputs[j]->sendMessageNow (juce::MidiMessage::midiStop());

            outputs[j].reset();
        }

        for (size_t j = 0; j < outputs.size(); ++j)
        {
            outputs[j] = std::move (kept[j]);
            outputNames[j] = keptNames[j];
        }
    }

    // The audio thread starts afresh: devices playing hear where the song is, and Continue.
    restart.store (true);
    rateSetting.store (static_cast<int> (rate));
    startOnlyOutputs.store (startOnly);
    timecodeOutputs.store (timecode);
    clockOutputs.store (clock);
}

juce::StringArray MidiSync::getProblems() const
{
    const juce::ScopedLock sl (outputLock);
    return problems;
}

void MidiSync::process (const Transport& transport, double sampleRate, double blockTime, double delayMs) noexcept
{
    if (restart.exchange (false))
        planner.reset();

    const auto clock = clockOutputs.load (std::memory_order_relaxed);
    const auto timecode = timecodeOutputs.load (std::memory_order_relaxed);

    if ((clock | timecode) == 0)
        return;

    const auto count = planner.plan (transport, sampleRate, blockTime, delayMs, clock,
                                     startOnlyOutputs.load (std::memory_order_relaxed), timecode,
                                     static_cast<Rate> (rateSetting.load (std::memory_order_relaxed)),
                                     scratch.data(), static_cast<int> (scratch.size()));
    queueMessages (scratch.data(), count);
}

void MidiSync::halt (double now) noexcept
{
    const auto count = planner.halt (now, scratch.data(), static_cast<int> (scratch.size()));
    queueMessages (scratch.data(), count);
}

void MidiSync::queueMessages (Message* messages, int count) noexcept
{
    for (int i = 0; i < count; ++i)
    {
        // A full queue drops the message rather than wait: the audio thread never blocks.
        const auto write = queue.write (1);

        if (write.blockSize1 > 0)
            queued[static_cast<size_t> (write.startIndex1)] = messages[i];
    }
}

void MidiSync::send (const Message& message)
{
    if (message.size <= 0)
        return;

    const juce::MidiMessage midi (message.bytes.data(), message.size);
    const juce::ScopedLock sl (outputLock);

    for (size_t i = 0; i < outputs.size(); ++i)
        if ((message.targets & (1u << i)) != 0 && outputs[i] != nullptr)
            outputs[i]->sendMessageNow (midi);
}

void MidiSync::run()
{
    while (! threadShouldExit())
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        queue.prepareToRead (1, start1, size1, start2, size2);

        if (size1 <= 0)
        {
            wait (1);
            continue;
        }

        const auto message = queued[static_cast<size_t> (start1)];
        const auto ahead = message.time - juce::Time::getMillisecondCounterHiRes();

        // Asleep most of the way, awake for the last moment, then out exactly on time.
        if (ahead > 1.5)
        {
            wait (juce::jlimit (1, 10, static_cast<int> (ahead - 1.0)));
            continue;
        }

        if (ahead > 0.05)
        {
            juce::Thread::yield();
            continue;
        }

        queue.finishedRead (1);
        send (message);
    }
}
