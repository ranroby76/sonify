// D:\workspace\Fanan_web_hub\src\MidiFx.cpp

#include "MidiFx.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MidiFx
{
namespace
{
    constexpr double notANumber = std::numeric_limits<double>::quiet_NaN();
    constexpr double eps = 1.0e-9;

    int clampPitch (int pitch) noexcept           { return juce::jlimit (0, 127, pitch); }
    bool validPitch (int pitch) noexcept           { return pitch >= 0 && pitch <= 127; }
    int clampVelocity (int velocity) noexcept     { return juce::jlimit (1, 127, velocity); }
}

//==============================================================================
// Key
bool Key::contains (int pitch) const noexcept
{
    if (notes == 0)
        return true;

    const auto step = ((pitch - root) % 12 + 12) % 12;
    return ((notes >> step) & 1) != 0;
}

int Key::snap (int pitch, int direction) const noexcept
{
    if (contains (pitch))
        return pitch;

    for (int distance = 1; distance < 12; ++distance)
    {
        if (direction <= 0 && contains (pitch - distance))
            return pitch - distance;

        if (direction >= 0 && contains (pitch + distance))
            return pitch + distance;
    }

    return pitch;
}

int Key::shift (int pitch, int steps) const noexcept
{
    if (steps == 0)
        return pitch;

    auto base = snap (pitch, -1);
    const auto offset = pitch - base;

    for (int i = 0; i < std::abs (steps); ++i)
    {
        const int direction = steps > 0 ? 1 : -1;
        auto next = base + direction;

        for (int guard = 0; guard < 12 && ! contains (next); ++guard)
            next += direction;

        base = next;
    }

    return base + offset;
}

Key Key::forChords (int pitch) const noexcept
{
    constexpr juce::uint16 major = 0x0ab5, minor = 0x05ad;

    if (notes == 0 || notes == 0x0fff)
        return { ((pitch % 12) + 12) % 12, major };

    // Five- and six-note scales stack their thirds from the scale they come from.
    if (notes == 0x0295)
        return { root, major };

    if (notes == 0x04a9 || notes == 0x04e9)
        return { root, minor };

    return *this;
}

//==============================================================================
// Events
Event Event::noteOn (double time, int channel, int pitch, int velocity) noexcept
{
    Event e;
    e.time = time;
    e.data[0] = static_cast<juce::uint8> (0x90 | (channel & 15));
    e.data[1] = static_cast<juce::uint8> (clampPitch (pitch));
    e.data[2] = static_cast<juce::uint8> (clampVelocity (velocity));
    e.size = 3;
    return e;
}

Event Event::noteOff (double time, int channel, int pitch) noexcept
{
    Event e;
    e.time = time;
    e.data[0] = static_cast<juce::uint8> (0x80 | (channel & 15));
    e.data[1] = static_cast<juce::uint8> (clampPitch (pitch));
    e.data[2] = 0;
    e.size = 3;
    return e;
}

EventList::EventList (int capacity)
    : events (static_cast<size_t> (juce::jmax (1, capacity)))
{
}

bool EventList::add (const Event& event) noexcept
{
    if (count >= static_cast<int> (events.size()))
        return false;

    events[static_cast<size_t> (count++)] = event;
    return true;
}

void EventList::sortByTime() noexcept
{
    // Insertion sort: the lists are short and nearly in order, and it keeps ties in order.
    for (int i = 1; i < count; ++i)
    {
        const auto item = events[static_cast<size_t> (i)];
        auto j = i - 1;

        while (j >= 0 && events[static_cast<size_t> (j)].time > item.time)
        {
            events[static_cast<size_t> (j + 1)] = events[static_cast<size_t> (j)];
            --j;
        }

        events[static_cast<size_t> (j + 1)] = item;
    }
}

Pending::Pending (int capacity)
    : events (static_cast<size_t> (juce::jmax (1, capacity)))
{
}

bool Pending::add (const Event& event) noexcept
{
    if (count >= static_cast<int> (events.size()))
        return false;

    // After any at the same time: they keep the order they were scheduled in.
    auto at = count;

    while (at > 0 && events[static_cast<size_t> (at - 1)].time > event.time)
    {
        events[static_cast<size_t> (at)] = events[static_cast<size_t> (at - 1)];
        --at;
    }

    events[static_cast<size_t> (at)] = event;
    ++count;
    return true;
}

Event Pending::takeFirst() noexcept
{
    const auto first = events[0];

    for (int i = 1; i < count; ++i)
        events[static_cast<size_t> (i - 1)] = events[static_cast<size_t> (i)];

    --count;
    return first;
}

bool Pending::removeNoteOn (int channel, int pitch, double notAfter) noexcept
{
    for (int i = count; --i >= 0;)
    {
        const auto& e = events[static_cast<size_t> (i)];

        if (e.isNoteOn() && e.channel() == channel && e.pitch() == pitch && e.time <= notAfter)
        {
            for (int j = i + 1; j < count; ++j)
                events[static_cast<size_t> (j - 1)] = events[static_cast<size_t> (j)];

            --count;
            return true;
        }
    }

    return false;
}

void Voices::on (int channel, int pitch) noexcept
{
    auto& c = counts[index (channel, pitch)];

    if (c < 255)
    {
        ++c;
        ++total;
    }
}

bool Voices::off (int channel, int pitch) noexcept
{
    auto& c = counts[index (channel, pitch)];

    if (c == 0)
        return true;

    --c;
    --total;
    return c == 0;
}

void Voices::clear() noexcept
{
    counts.fill (0);
    total = 0;
}

void Voices::releaseAll (EventList& out, double time, bool everyTime) noexcept
{
    if (total == 0)
        return;

    for (int channel = 0; channel < 16; ++channel)
        for (int pitch = 0; pitch < 128; ++pitch)
            for (int n = everyTime ? counts[index (channel, pitch)] : juce::jmin (1, static_cast<int> (counts[index (channel, pitch)])); n > 0; --n)
                out.add (Event::noteOff (time, channel, pitch));

    clear();
}

//==============================================================================
// Time
namespace
{
    /** Marks for a stretch of song: the bar and key where it starts and at each bar line in it. */
    int marksFor (const TempoMap& map, double from, double to, Transport::Mark* out, int max) noexcept
    {
        constexpr double near = 1.0e-7;
        int count = 0;

        const auto add = [&] (double ppq, const TempoMap::Bar& bar, bool barLine)
        {
            if (count >= max)
                return;

            const auto key = map.keyAt (bar.start);
            auto& mark = out[count++];
            mark.ppq = ppq;
            mark.segment = 0;
            mark.barLine = barLine;
            mark.bar = bar.index;
            mark.barLength = bar.length();
            mark.keyRoot = key.root;
            mark.keyNotes = TempoMap::scaleNotes (key.scale);
            mark.bpm = map.bpmAt (ppq);
        };

        auto bar = map.barAt (from + near);
        add (from, bar, std::abs (from - bar.start) < near);

        while (count < max)
        {
            const auto next = bar.start + bar.length();

            if (next >= to - near)
                break;

            const auto following = map.barAt (next + near);

            if (following.start < next - 1.0e-6)
                break;

            add (next, following, true);
            bar = following;
        }

        return count;
    }
}

void Time::build (const Transport& transport, double clock, int numSamples) noexcept
{
    offline = false;
    numPieces = 0;
    start = clock;

    auto stoppedRate = transport.getPpqPerSample();

    if (! (stoppedRate > 0.0))
        stoppedRate = 1.0e-9;

    auto at = 0;
    auto c = clock;
    const auto segments = transport.getNumSegments();

    for (int s = 0; s < segments && numPieces < maxPieces - 1; ++s)
    {
        const auto& segment = transport.getSegment (s);

        if (segment.offset > at)
        {
            pieces[static_cast<size_t> (numPieces++)] = { c, stoppedRate, at, segment.offset - at, false, 0.0, -1 };
            c += stoppedRate * (segment.offset - at);
            at = segment.offset;
        }

        if (numPieces >= maxPieces - 1)
            break;

        const auto rate = segment.perSample > 0.0 ? segment.perSample : stoppedRate;
        pieces[static_cast<size_t> (numPieces++)] = { c, rate, segment.offset, segment.length, true, segment.from, s };
        c += rate * segment.length;
        at = segment.offset + segment.length;
    }

    if (at < numSamples || numPieces == 0)
    {
        const auto length = juce::jmax (0, numSamples - at);
        pieces[static_cast<size_t> (numPieces++)] = { c, stoppedRate, at, length, false, 0.0, -1 };
        c += stoppedRate * length;
    }

    end = c;

    numMarks = juce::jmin (transport.getNumMarks(), static_cast<int> (marks.size()));

    for (int i = 0; i < numMarks; ++i)
        marks[static_cast<size_t> (i)] = transport.getMark (i);

    numChordMarks = juce::jmin (transport.getNumChordMarks(), static_cast<int> (chordMarks.size()));

    for (int i = 0; i < numChordMarks; ++i)
        chordMarks[static_cast<size_t> (i)] = transport.getChordMark (i);

    bpm = numMarks > 0 ? marks[0].bpm : 120.0;
}

void Time::buildOffline (const TempoMap& map, double from, double to, bool playing) noexcept
{
    offline = true;
    start = from;
    end = juce::jmax (from + eps, to);
    numPieces = 1;
    pieces[0] = { from, end - from, 0, 1, playing, from, playing ? 0 : -1 };
    numMarks = marksFor (map, from, end, marks.data(), static_cast<int> (marks.size()));
    numChordMarks = Transport::chordMarksFor (map, from, end, 0, chordMarks.data(), static_cast<int> (chordMarks.size()));
    bpm = map.bpmAt (from);
}

bool Time::isPlaying() const noexcept
{
    for (int i = 0; i < numPieces; ++i)
        if (pieces[static_cast<size_t> (i)].playing)
            return true;

    return false;
}

const Time::Piece& Time::pieceAt (double time) const noexcept
{
    auto index = 0;

    while (index + 1 < numPieces && pieces[static_cast<size_t> (index + 1)].clock <= time)
        ++index;

    return pieces[static_cast<size_t> (index)];
}

double Time::songAt (double time) const noexcept
{
    const auto& piece = pieceAt (time);
    return piece.playing ? piece.song + (time - piece.clock) : notANumber;
}

int Time::sampleAt (double time) const noexcept
{
    const auto& piece = pieceAt (time);

    if (piece.length <= 0)
        return piece.offset;

    const auto within = piece.perSample > 0.0 ? std::floor ((time - piece.clock) / piece.perSample + 1.0e-6) : 0.0;
    return piece.offset + static_cast<int> (juce::jlimit (0.0, static_cast<double> (piece.length - 1), within));
}

double Time::timeAtSample (int sample) const noexcept
{
    for (int i = 0; i < numPieces; ++i)
    {
        const auto& piece = pieces[static_cast<size_t> (i)];

        if (sample < piece.offset + piece.length || i == numPieces - 1)
            return piece.clock + static_cast<double> (sample - piece.offset) * piece.perSample;
    }

    return start;
}

const Transport::Mark& Time::markAt (double time) const noexcept
{
    static const Transport::Mark none;

    if (numMarks == 0)
        return none;

    const auto& piece = pieceAt (time);

    if (! piece.playing)
    {
        for (int i = 0; i < numMarks; ++i)
            if (marks[static_cast<size_t> (i)].segment < 0)
                return marks[static_cast<size_t> (i)];

        return marks[static_cast<size_t> (numMarks - 1)];
    }

    const auto song = songAt (time);
    const Transport::Mark* found = nullptr;

    for (int i = 0; i < numMarks; ++i)
    {
        const auto& mark = marks[static_cast<size_t> (i)];

        if (mark.segment != piece.segment)
            continue;

        if (found == nullptr || mark.ppq <= song + 1.0e-7)
            found = &mark;
    }

    return found != nullptr ? *found : marks[0];
}

Time::Chord Time::chordAt (double time) const noexcept
{
    if (numChordMarks == 0)
        return {};

    const auto& piece = pieceAt (time);
    const Transport::ChordMark* found = nullptr;

    if (! piece.playing)
    {
        // Stopped: the chord where the song stands (or, offline past the clip's end, the last).
        for (int i = 0; i < numChordMarks && found == nullptr; ++i)
            if (chordMarks[static_cast<size_t> (i)].segment < 0)
                found = &chordMarks[static_cast<size_t> (i)];

        if (found == nullptr)
            found = &chordMarks[static_cast<size_t> (numChordMarks - 1)];
    }
    else
    {
        const auto song = songAt (time);

        for (int i = 0; i < numChordMarks; ++i)
        {
            const auto& mark = chordMarks[static_cast<size_t> (i)];

            if (mark.segment != piece.segment)
                continue;

            if (found == nullptr || mark.ppq <= song + 1.0e-7)
                found = &mark;
        }
    }

    if (found == nullptr || found->root < 0)
        return {};

    return { found->root, found->notes, found->bass };
}

Key Time::keyAt (double time) const noexcept
{
    const auto& mark = markAt (time);
    return { mark.keyRoot, mark.keyNotes };
}

double Time::barLengthAt (double time) const noexcept
{
    return markAt (time).barLength;
}

int Time::gridLines (double step, double origin, GridLine* out, int max) const noexcept
{
    if (! (step > 0.0) || max <= 0)
        return 0;

    int count = 0;

    for (int i = 0; i < numPieces && count < max; ++i)
    {
        const auto& piece = pieces[static_cast<size_t> (i)];
        const auto span = piece.perSample * piece.length;

        if (span <= 0.0)
            continue;

        // Each block takes the lines from just before its start to just before its end, so
        // a line on the boundary between two blocks is in one of them, never both.
        const auto base = piece.playing ? piece.song : origin;

        if (std::isnan (base))
            continue;

        const auto from = piece.playing ? piece.song : piece.clock - origin;
        const auto to = from + span;
        auto n = static_cast<juce::int64> (std::ceil ((from - eps) / step));

        for (; count < max; ++n)
        {
            const auto line = static_cast<double> (n) * step;

            if (line >= to - eps)
                break;

            out[count++] = { piece.clock + juce::jmax (0.0, line - from), n };
        }
    }

    return count;
}

int Time::barLines (BarLine* out, int max) const noexcept
{
    int count = 0;

    for (int i = 0; i < numMarks && count < max; ++i)
    {
        const auto& mark = marks[static_cast<size_t> (i)];

        if (! mark.barLine || mark.segment < 0)
            continue;

        for (int p = 0; p < numPieces; ++p)
        {
            const auto& piece = pieces[static_cast<size_t> (p)];

            if (piece.playing && piece.segment == mark.segment)
            {
                out[count++] = { piece.clock + juce::jmax (0.0, mark.ppq - piece.song), mark.bar, mark.barLength };
                break;
            }
        }
    }

    return count;
}

//==============================================================================
// Rates
const juce::StringArray& rateNames()
{
    static const juce::StringArray names { "1/1", "1/2", "1/2 dotted", "1/2 triplet", "1/4", "1/4 dotted", "1/4 triplet",
                                           "1/8", "1/8 dotted", "1/8 triplet", "1/16", "1/16 dotted", "1/16 triplet",
                                           "1/32", "1/32 triplet", "1/64" };
    return names;
}

double rateLength (float choice) noexcept
{
    static constexpr double lengths[] = { 4.0, 2.0, 3.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0,
                                          0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0,
                                          0.125, 1.0 / 12.0, 0.0625 };
    const auto index = juce::jlimit (0, static_cast<int> (std::size (lengths)) - 1, juce::roundToInt (choice));
    return lengths[index];
}

//==============================================================================
// Device
Device::Device (const TypeSpec& typeSpec, int pendingCapacity)
    : spec (typeSpec), pending (pendingCapacity)
{
    for (size_t i = 0; i < values.size(); ++i)
        values[i].store (i < spec.params.size() ? spec.params[i].defaultValue : 0.0f);
}

float Device::get (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, maxParams) ? values[static_cast<size_t> (index)].load (std::memory_order_relaxed) : 0.0f;
}

float Device::set (int index, float value) noexcept
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (spec.params.size())) || ! std::isfinite (value))
        return get (index);

    const auto& param = spec.params[static_cast<size_t> (index)];
    auto v = juce::jlimit (param.min, param.max, value);

    if (param.unit != Unit::percent && param.unit != Unit::decimal && param.unit != Unit::ms)
        v = static_cast<float> (juce::roundToInt (v));

    values[static_cast<size_t> (index)].store (v, std::memory_order_relaxed);
    return v;
}

void Device::flush (EventList& out, double time) noexcept
{
    pending.clear();
    overflow.clear();
    endsNow.clear();
    sounding.releaseAll (out, time);
    reset();
}

void Device::schedule (const Event& event) noexcept
{
    if (pending.add (event) || ! event.isNoteOff())
        return;

    if (overflow.add (event))
        return;

    // No room anywhere: a note whose start still waits doesn't play at all (its end can't go
    // before it); one already started ends now.
    if (! pending.removeNoteOn (event.channel(), event.pitch(), event.time))
        endsNow.add (event);
}

void Device::emit (EventList& out, const Event& event) noexcept
{
    if (event.isNoteOn())
        sounding.on (event.channel(), event.pitch());
    else if (event.isNoteOff())
        sounding.off (event.channel(), event.pitch());

    out.add (event);
}

void Device::emitDue (EventList& out, double before) noexcept
{
    for (int i = 0; i < endsNow.size(); ++i)
    {
        auto e = endsNow[i];
        e.time = juce::jmin (e.time, before);
        emit (out, e);
    }

    endsNow.clear();

    // The two queues in time order.
    for (;;)
    {
        const bool fromPending = ! pending.isEmpty() && pending.firstTime() < before;
        const bool fromOverflow = ! overflow.isEmpty() && overflow.firstTime() < before;

        if (! fromPending && ! fromOverflow)
            break;

        if (fromPending && (! fromOverflow || pending.firstTime() <= overflow.firstTime()))
            emit (out, pending.takeFirst());
        else
            emit (out, overflow.takeFirst());
    }
}

//==============================================================================
// The devices
namespace
{
    /** Remembers what each held note became, so its end ends those. */
    class NoteMap
    {
    public:
        static constexpr int maxOut = 8;
        static constexpr juce::int8 unknown = -1;

        struct Entry
        {
            juce::int8 count = unknown;
            std::array<juce::uint8, maxOut> pitches {};
        };

        Entry& at (int channel, int pitch) noexcept     { return entries[static_cast<size_t> ((channel & 15) * 128 + (pitch & 127))]; }
        void clear() noexcept                            { for (auto& e : entries) e.count = unknown; }

    private:
        std::array<Entry, 16 * 128> entries {};
    };

    /** A device that turns each note into others (none, one or a chord) as it starts. */
    class RemapDevice : public Device
    {
    public:
        explicit RemapDevice (const TypeSpec& s) : Device (s)   { map.clear(); }

        void process (const EventList& in, EventList& out, const Time& time) noexcept override
        {
            for (int i = 0; i < in.size(); ++i)
            {
                const auto& e = in[i];

                if (e.isNoteOn())
                {
                    auto& entry = map.at (e.channel(), e.pitch());
                    end (entry, out, e.time, e.channel());   // started again before it ended

                    std::array<int, NoteMap::maxOut> pitches {}, velocities {};
                    const auto made = juce::jmin (NoteMap::maxOut, transform (e, time, pitches.data(), velocities.data()));
                    entry.count = 0;

                    for (int k = 0; k < made; ++k)
                    {
                        const auto pitch = pitches[static_cast<size_t> (k)];

                        if (! validPitch (pitch))
                            continue;

                        bool twice = false;

                        for (int j = 0; j < entry.count; ++j)
                            twice = twice || entry.pitches[static_cast<size_t> (j)] == pitch;

                        if (twice)
                            continue;

                        entry.pitches[static_cast<size_t> (entry.count++)] = static_cast<juce::uint8> (pitch);
                        emit (out, Event::noteOn (e.time, e.channel(), pitch, velocities[static_cast<size_t> (k)]));
                    }
                }
                else if (e.isNoteOff())
                {
                    auto& entry = map.at (e.channel(), e.pitch());

                    if (entry.count == NoteMap::unknown)
                        emit (out, e);                   // a note it never saw start
                    else
                        end (entry, out, e.time, e.channel());
                }
                else
                {
                    emit (out, e);
                }
            }
        }

    protected:
        /** The notes a starting note becomes: pitches and velocities; returns how many. */
        virtual int transform (const Event& on, const Time& time, int* pitches, int* velocities) noexcept = 0;

        void reset() noexcept override                   { map.clear(); }

    private:
        void end (NoteMap::Entry& entry, EventList& out, double time, int channel) noexcept
        {
            for (int k = 0; k < entry.count; ++k)
                emit (out, Event::noteOff (time, channel, entry.pitches[static_cast<size_t> (k)]));

            entry.count = NoteMap::unknown;
        }

        NoteMap map;
    };

    //==========================================================================
    class PitchDevice final : public RemapDevice
    {
    public:
        enum { semitones, steps };
        using RemapDevice::RemapDevice;

        int transform (const Event& on, const Time& time, int* pitches, int* velocities) noexcept override
        {
            pitches[0] = time.keyAt (on.time).shift (on.pitch(), getInt (steps)) + getInt (semitones);
            velocities[0] = on.velocity();
            return 1;
        }
    };

    class ScaleDevice final : public RemapDevice
    {
    public:
        enum { mode };
        enum { nearest, up, down, remove };
        using RemapDevice::RemapDevice;

        int transform (const Event& on, const Time& time, int* pitches, int* velocities) noexcept override
        {
            const auto key = time.keyAt (on.time);
            const auto how = getInt (mode);

            if (how == remove && ! key.contains (on.pitch()))
                return 0;

            pitches[0] = key.snap (on.pitch(), how == up ? 1 : (how == down ? -1 : 0));
            velocities[0] = on.velocity();
            return 1;
        }
    };

    class ChordDevice final : public RemapDevice
    {
    public:
        enum { shape, inversion, voicing, added };
        enum { close, open, wide };
        using RemapDevice::RemapDevice;

        int transform (const Event& on, const Time& time, int* pitches, int* velocities) noexcept override
        {
            // Scale steps above the played note, for each shape.
            static constexpr int shapes[][5] = {
                { 0, 2, 4, -1, -1 },     // triad
                { 0, 2, 4, 6, -1 },      // seventh
                { 0, 2, 4, 6, 8 },       // ninth
                { 0, 1, 4, -1, -1 },     // sus2
                { 0, 3, 4, -1, -1 },     // sus4
                { 0, 2, 4, 5, -1 },      // sixth
                { 0, 2, 4, 8, -1 },      // add9
                { 0, 4, 7, -1, -1 },     // power (root, fifth, octave)
                { 0, 7, 14, -1, -1 }     // octaves
            };

            const auto which = juce::jlimit (0, static_cast<int> (std::size (shapes)) - 1, getInt (shape));
            const auto key = time.keyAt (on.time).forChords (on.pitch());
            int count = 0;

            for (const auto degree : shapes[which])
                if (degree >= 0)
                    pitches[count++] = key.shift (on.pitch(), degree);

            std::sort (pitches, pitches + count);

            // Inversions take the lowest notes up an octave.
            for (int i = 0; i < juce::jmin (getInt (inversion), count - 1); ++i)
            {
                pitches[0] += 12;
                std::sort (pitches, pitches + count);
            }

            const auto spread = getInt (voicing);

            if (spread == open && count >= 3)
                pitches[1] += 12;
            else if (spread == wide && count >= 3)
                for (int i = 1; i < count; i += 2)
                    pitches[i] += 12;

            std::sort (pitches, pitches + count);

            const auto addedVelocity = clampVelocity (juce::roundToInt (on.velocity() * get (added) / 100.0f));

            for (int i = 0; i < count; ++i)
                velocities[i] = pitches[i] == on.pitch() ? on.velocity() : addedVelocity;

            return count;
        }
    };

    class RandomDevice final : public RemapDevice
    {
    public:
        enum { chance, range, direction, skip };
        enum { both, up, down };
        using RemapDevice::RemapDevice;

        int transform (const Event& on, const Time& time, int* pitches, int* velocities) noexcept override
        {
            if (random.nextFloat() * 100.0f < get (skip))
                return 0;

            auto pitch = on.pitch();

            if (random.nextFloat() * 100.0f < get (chance))
            {
                const auto steps = 1 + random.nextInt (juce::jmax (1, getInt (range)));
                const auto way = getInt (direction);
                const bool goUp = way == up || (way == both && random.nextBool());
                const auto key = time.keyAt (on.time);
                pitch = key.shift (key.snap (pitch), goUp ? steps : -steps);
            }

            pitches[0] = pitch;
            velocities[0] = on.velocity();
            return 1;
        }
    };

    class RangeDevice final : public RemapDevice
    {
    public:
        enum { low, high, mode };
        enum { remove, fold };
        using RemapDevice::RemapDevice;

        int transform (const Event& on, const Time&, int* pitches, int* velocities) noexcept override
        {
            const auto lowest = juce::jmin (getInt (low), getInt (high));
            const auto highest = juce::jmax (getInt (low), getInt (high));
            auto pitch = on.pitch();

            if (pitch < lowest || pitch > highest)
            {
                if (getInt (mode) == remove)
                    return 0;

                while (pitch < lowest)
                    pitch += 12;

                while (pitch > highest)
                    pitch -= 12;

                if (pitch < lowest)
                    return 0;   // narrower than an octave, and this note has no place in it
            }

            pitches[0] = pitch;
            velocities[0] = on.velocity();
            return 1;
        }
    };

    //==========================================================================
    class VelocityDevice final : public Device
    {
    public:
        enum { mode, low, high, curve, randomness };
        enum { scale, fixed };
        using Device::Device;

        void process (const EventList& in, EventList& out, const Time&) noexcept override
        {
            for (int i = 0; i < in.size(); ++i)
            {
                auto e = in[i];

                if (e.isNoteOn())
                {
                    double v;

                    if (getInt (mode) == fixed)
                    {
                        v = get (high);
                    }
                    else
                    {
                        const auto exponent = std::pow (2.0, -get (curve) / 50.0);
                        v = get (low) + (get (high) - get (low)) * std::pow (e.velocity() / 127.0, exponent);
                    }

                    const auto spread = get (randomness);

                    if (spread > 0.0f)
                        v += (random.nextDouble() * 2.0 - 1.0) * spread;

                    e.data[2] = static_cast<juce::uint8> (clampVelocity (juce::roundToInt (v)));
                }

                emit (out, e);
            }
        }
    };

    //==========================================================================
    /** Grex's velocity curve: one bipolar Curve, soft (-100) to hard (+100), out = in^gamma with
        gamma = 2.5^-curve (on 0..1), 0 and 127 staying put; a note struck never becomes silent.
        Then the lowest and highest the notes may reach. */
    class VelocityCurveDevice final : public Device
    {
    public:
        enum { curve, low, high };
        using Device::Device;

        void process (const EventList& in, EventList& out, const Time&) noexcept override
        {
            const auto c = juce::jlimit (-1.0, 1.0, get (curve) / 100.0);
            const auto gamma = std::pow (2.5, -c);
            const auto lowest = juce::jlimit (1, 127, getInt (low));
            const auto highest = juce::jlimit (lowest, 127, getInt (high));

            for (int i = 0; i < in.size(); ++i)
            {
                auto e = in[i];

                if (e.isNoteOn() && e.velocity() > 0)
                {
                    auto v = e.velocity() / 127.0;

                    if (std::abs (c) >= 1.0e-3)
                        v = std::pow (v, gamma);

                    const auto shaped = lowest + (highest - lowest) * v;
                    e.data[2] = static_cast<juce::uint8> (clampVelocity (juce::jmax (1, juce::roundToInt (shaped))));
                }

                emit (out, e);
            }
        }
    };

    //==========================================================================
    class LengthDevice final : public Device
    {
    public:
        enum { mode, length, gate, stretch };
        enum { fixed, stretched, legato };

        explicit LengthDevice (const TypeSpec& s) : Device (s)   { reset(); }

        void process (const EventList& in, EventList& out, const Time& time) noexcept override
        {
            const auto how = getInt (mode);
            const auto tolerance = time.offline ? eps : 1.0e-4;

            for (int i = 0; i < in.size(); ++i)
            {
                const auto& e = in[i];
                emitDue (out, e.time);
                const auto at = slot (e);

                if (e.isNoteOn())
                {
                    started[at] = e.time;
                    startedAs[at] = static_cast<juce::int8> (how);   // its end follows the mode it started in

                    if (how == fixed)
                    {
                        emit (out, e);
                        schedule (Event::noteOff (e.time + juce::jmax (minimumLength, rateLength (get (length)) * get (gate) / 100.0),
                                                  e.channel(), e.pitch()));
                    }
                    else if (how == legato)
                    {
                        // A new moment ends the notes held over from the last one.
                        if (e.time > groupTime + tolerance)
                        {
                            for (int k = 0; k < groupCount; ++k)
                            {
                                const auto& note = group[static_cast<size_t> (k)];

                                if (note.released)
                                {
                                    removeScheduledEnd (note.channel, note.pitch);
                                    emit (out, Event::noteOff (e.time, note.channel, note.pitch));
                                }
                            }

                            groupCount = 0;
                            groupTime = e.time;
                        }

                        if (groupCount < static_cast<int> (group.size()))
                            group[static_cast<size_t> (groupCount++)] = { e.channel(), e.pitch(), false };

                        emit (out, e);
                    }
                    else
                    {
                        emit (out, e);
                    }
                }
                else if (e.isNoteOff())
                {
                    const auto since = started[at];

                    if (since < 0.0)
                    {
                        emit (out, e);   // never saw it start
                        continue;
                    }

                    started[at] = -1.0;
                    const auto as = startedAs[at];

                    if (as == stretched)
                    {
                        schedule (Event::noteOff (since + (e.time - since) * get (stretch) / 100.0, e.channel(), e.pitch()));
                    }
                    else if (as == legato)
                    {
                        bool inGroup = false;

                        for (int k = 0; k < groupCount; ++k)
                        {
                            auto& note = group[static_cast<size_t> (k)];

                            if (note.channel == e.channel() && note.pitch == e.pitch() && ! note.released)
                            {
                                note.released = true;
                                inGroup = true;
                                // Held at most this long past its end, when nothing comes after it.
                                schedule (Event::noteOff (e.time + rateLength (get (length)), e.channel(), e.pitch()));
                                break;
                            }
                        }

                        if (! inGroup)
                            emit (out, e);
                    }
                    // fixed: the note's own end was scheduled when it started
                }
                else
                {
                    emit (out, e);
                }
            }

            emitDue (out, time.end);
        }

    private:
        static constexpr double minimumLength = 1.0 / 128.0;

        struct Held
        {
            int channel = 0, pitch = 0;
            bool released = false;
        };

        static size_t slot (const Event& e) noexcept      { return static_cast<size_t> (e.channel() * 128 + e.pitch()); }

        void reset() noexcept override
        {
            started.fill (-1.0);
            startedAs.fill (fixed);
            groupCount = 0;
            groupTime = -1.0e9;
        }

        void removeScheduledEnd (int channel, int pitch) noexcept
        {
            // Pending has no random access: take everything out and put back what stays.
            std::array<Event, 64> keep {};
            int kept = 0;
            bool removed = false;

            while (! pending.isEmpty() && kept < static_cast<int> (keep.size()))
            {
                const auto e = pending.takeFirst();

                if (! removed && e.isNoteOff() && e.channel() == channel && e.pitch() == pitch)
                {
                    removed = true;
                    continue;
                }

                keep[static_cast<size_t> (kept++)] = e;
            }

            for (int k = 0; k < kept; ++k)
                pending.add (keep[static_cast<size_t> (k)]);
        }

        std::array<double, 16 * 128> started {};
        std::array<juce::int8, 16 * 128> startedAs {};
        std::array<Held, 32> group {};
        int groupCount = 0;
        double groupTime = -1.0e9;
    };

    //==========================================================================
    class EchoDevice final : public Device
    {
    public:
        enum { time_, repeats, decay, steps, dry };
        static constexpr int maxRepeats = 16;

        explicit EchoDevice (const TypeSpec& s) : Device (s, 4096)   { reset(); }

        void process (const EventList& in, EventList& out, const Time& time) noexcept override
        {
            const auto delay = rateLength (get (time_));
            const auto keepDry = get (dry) > 0.5f;

            for (int i = 0; i < in.size(); ++i)
            {
                const auto& e = in[i];
                emitDue (out, e.time);
                auto& note = notes[static_cast<size_t> (e.channel() * 128 + e.pitch())];

                if (e.isNoteOn())
                {
                    // Started again before it ended: its echoes end where the new ones start.
                    if (note.count > 0)
                        endEchoes (note, e.time, e.channel(), true);

                    if (keepDry)
                        emit (out, e);

                    note.started = e.time;
                    note.delay = delay;
                    note.dry = keepDry;
                    note.count = 0;

                    const auto key = time.keyAt (e.time);
                    auto velocity = static_cast<double> (e.velocity());

                    for (int k = 1; k <= juce::jlimit (1, maxRepeats, getInt (repeats)); ++k)
                    {
                        velocity *= get (decay) / 100.0;

                        // An echo starts only with room left for its end (ends come when the note does).
                        if (velocity < 1.0 || pending.size() + endsOwed + 2 > pending.capacity())
                            break;

                        const auto pitch = key.shift (e.pitch(), k * getInt (steps));

                        if (! validPitch (pitch))
                            continue;

                        note.pitches[static_cast<size_t> (note.count)] = static_cast<juce::uint8> (pitch);
                        note.times[static_cast<size_t> (note.count)] = e.time + k * delay;
                        ++note.count;
                        ++endsOwed;
                        schedule (Event::noteOn (e.time + k * delay, e.channel(), pitch, juce::roundToInt (velocity)));
                    }

                    note.open = true;
                }
                else if (e.isNoteOff())
                {
                    if (! note.open)
                    {
                        emit (out, e);
                        continue;
                    }

                    if (note.dry)
                        emit (out, e);

                    endEchoes (note, e.time, e.channel(), false);
                }
                else
                {
                    emit (out, e);
                }
            }

            emitDue (out, time.end);
        }

    private:
        struct Note
        {
            bool open = false, dry = true;
            double started = 0.0, delay = 0.5;
            int count = 0;
            std::array<juce::uint8, maxRepeats> pitches {};
            std::array<double, maxRepeats> times {};
        };

        /** Each echo lasts as long as the note did. Cut (the note played again): echoes already
            sounding end where the new ones begin. */
        void endEchoes (Note& note, double at, int channel, bool cut) noexcept
        {
            const auto held = juce::jmax (1.0 / 128.0, at - note.started);

            for (int k = 0; k < note.count; ++k)
            {
                const auto start = note.times[static_cast<size_t> (k)];
                const auto end = cut && start <= at ? at : start + held;
                schedule (Event::noteOff (juce::jmax (end, start + 1.0 / 128.0), channel, note.pitches[static_cast<size_t> (k)]));
            }

            endsOwed = juce::jmax (0, endsOwed - note.count);
            note.count = 0;
            note.open = false;
        }

        void reset() noexcept override
        {
            for (auto& n : notes)
                n = Note {};

            endsOwed = 0;
        }

        std::array<Note, 16 * 128> notes {};
        int endsOwed = 0;                    // echoes started whose ends aren't scheduled yet
    };

    //==========================================================================
    /** Held notes, in the order they were played. */
    struct HeldNotes
    {
        struct Note
        {
            int channel = 0, pitch = 0, velocity = 100;
        };

        static constexpr int capacity = 32;
        std::array<Note, capacity> notes {};
        int count = 0;

        void add (int channel, int pitch, int velocity) noexcept
        {
            remove (channel, pitch);

            if (count < capacity)
                notes[static_cast<size_t> (count++)] = { channel, pitch, velocity };
        }

        void remove (int channel, int pitch) noexcept
        {
            for (int i = 0; i < count; ++i)
            {
                if (notes[static_cast<size_t> (i)].channel == channel && notes[static_cast<size_t> (i)].pitch == pitch)
                {
                    for (int j = i + 1; j < count; ++j)
                        notes[static_cast<size_t> (j - 1)] = notes[static_cast<size_t> (j)];

                    --count;
                    return;
                }
            }
        }

        bool has (int channel, int pitch) const noexcept
        {
            for (int i = 0; i < count; ++i)
                if (notes[static_cast<size_t> (i)].channel == channel && notes[static_cast<size_t> (i)].pitch == pitch)
                    return true;

            return false;
        }
    };

    /** A device that plays on a grid of steps: arpeggiator, note repeat, euclidean rhythms.
        While the song plays the grid is the song's; while it's stopped, the grid starts with
        the first note played. */
    class StepDevice : public Device
    {
    public:
        using Device::Device;

        void process (const EventList& in, EventList& out, const Time& time) noexcept override
        {
            begin (time);
            const auto step = stepLength();
            const auto tolerance = time.offline ? eps : 2.0e-4;
            std::array<Time::GridLine, 256> lines {};
            auto count = time.gridLines (step, origin, lines.data(), static_cast<int> (lines.size()));
            int line = 0, i = 0;

            while (i < in.size() || line < count)
            {
                const bool event = i < in.size() && (line >= count || in[i].time <= lines[static_cast<size_t> (line)].time + tolerance);

                if (! event)
                {
                    emitDue (out, lines[static_cast<size_t> (line)].time);
                    onStep (out, lines[static_cast<size_t> (line)], time, step);
                    ++line;
                    continue;
                }

                const auto& e = in[i++];
                emitDue (out, e.time);

                // Stopped, nothing going on: the grid starts with this note.
                if (e.isNoteOn() && ! time.isPlayingAt (e.time) && std::isnan (origin) && wantsGrid (e))
                {
                    origin = e.time;
                    count = time.gridLines (step, origin, lines.data(), static_cast<int> (lines.size()));
                    line = 0;

                    while (line < count && lines[static_cast<size_t> (line)].time < e.time - tolerance)
                        ++line;
                }

                onEvent (out, e, time);

                if (! time.isPlayingAt (e.time) && idle())
                    origin = notANumber;
            }

            emitDue (out, time.end);
        }

    protected:
        virtual double stepLength() const noexcept = 0;
        virtual void begin (const Time&) noexcept {}
        virtual bool wantsGrid (const Event&) const noexcept   { return true; }
        virtual void onEvent (EventList& out, const Event& e, const Time& time) noexcept = 0;
        virtual void onStep (EventList& out, const Time::GridLine& line, const Time& time, double step) noexcept = 0;
        virtual bool idle() const noexcept = 0;

        void reset() noexcept override                  { origin = notANumber; resetSteps(); }
        virtual void resetSteps() noexcept {}

        double origin = notANumber;
    };

    //==========================================================================
    class ArpDevice final : public StepDevice
    {
    public:
        enum { style, rate, gate, octaves, hold, swing, velocity };
        enum { up, down, upDown, downUp, upAndDown, converge, diverge, asPlayed, randomOrder, chord };

        using StepDevice::StepDevice;

    private:
        double stepLength() const noexcept override      { return rateLength (get (rate)); }
        bool idle() const noexcept override              { return active().count == 0; }

        const HeldNotes& active() const noexcept         { return get (hold) > 0.5f ? latched : held; }

        void begin (const Time&) noexcept override
        {
            const bool holding = get (hold) > 0.5f;

            if (! holding)
                latched.count = 0;
            else if (latched.count == 0 && held.count > 0)
                latched = held;

            if (idle())
                steps = 0;
        }

        void onEvent (EventList& out, const Event& e, const Time&) noexcept override
        {
            const bool holding = get (hold) > 0.5f;

            if (e.isNoteOn())
            {
                if (idle())
                    steps = 0;

                if (holding && held.count == 0)
                    latched.count = 0;          // a new chord replaces the one held

                held.add (e.channel(), e.pitch(), e.velocity());

                if (holding)
                    latched.add (e.channel(), e.pitch(), e.velocity());
            }
            else if (e.isNoteOff())
            {
                if (! held.has (e.channel(), e.pitch()) && ! latched.has (e.channel(), e.pitch()))
                    emit (out, e);              // not one of its notes

                held.remove (e.channel(), e.pitch());
            }
            else
            {
                emit (out, e);
            }
        }

        void onStep (EventList& out, const Time::GridLine& line, const Time&, double step) noexcept override
        {
            const auto& notes = active();

            if (notes.count == 0)
                return;

            // The notes in order, over the octaves.
            std::array<HeldNotes::Note, HeldNotes::capacity * 4> sequence {};
            int length = 0;
            const auto order = getInt (style);
            const auto span = juce::jlimit (1, 4, getInt (octaves));

            std::array<HeldNotes::Note, HeldNotes::capacity> base {};
            std::copy (notes.notes.begin(), notes.notes.begin() + notes.count, base.begin());

            if (order != asPlayed)
                std::sort (base.begin(), base.begin() + notes.count, [] (const auto& a, const auto& b) { return a.pitch < b.pitch; });

            for (int octave = 0; octave < span; ++octave)
            {
                for (int i = 0; i < notes.count; ++i)
                {
                    auto note = base[static_cast<size_t> (i)];
                    note.pitch += 12 * octave;

                    if (validPitch (note.pitch))
                        sequence[static_cast<size_t> (length++)] = note;
                }
            }

            if (length == 0)
                return;

            const auto swingDelay = (line.number % 2 != 0) ? step * get (swing) / 100.0 : 0.0;
            const auto at = line.time + swingDelay;
            const auto end = at + juce::jmax (1.0 / 128.0, step * get (gate) / 100.0);
            const auto fixedVelocity = getInt (velocity);

            const auto play = [&] (const HeldNotes::Note& note)
            {
                const auto v = fixedVelocity > 0 ? fixedVelocity : note.velocity;

                if (swingDelay > 0.0)
                    schedule (Event::noteOn (at, note.channel, note.pitch, v));
                else
                    emit (out, Event::noteOn (at, note.channel, note.pitch, v));

                schedule (Event::noteOff (end, note.channel, note.pitch));
            };

            if (order == chord)
            {
                // The whole chord each step, an octave up each time round.
                const auto octave = static_cast<int> (steps % span);

                for (int i = 0; i < notes.count; ++i)
                {
                    auto note = base[static_cast<size_t> (i)];
                    note.pitch += 12 * octave;

                    if (validPitch (note.pitch))
                        play (note);
                }
            }
            else
            {
                play (sequence[static_cast<size_t> (indexFor (order, length))]);
            }

            ++steps;
        }

        int indexFor (int order, int n) noexcept
        {
            const auto s = static_cast<int> (steps % 1000000);

            switch (order)
            {
                case down:        return n - 1 - s % n;
                case upDown:      { if (n < 2) return 0; const auto k = s % (2 * n - 2); return k < n ? k : 2 * n - 2 - k; }
                case downUp:      { if (n < 2) return 0; const auto k = s % (2 * n - 2); return n - 1 - (k < n ? k : 2 * n - 2 - k); }
                case upAndDown:   { const auto k = s % (2 * n); return k < n ? k : 2 * n - 1 - k; }
                case converge:    { const auto k = s % n; return k % 2 == 0 ? k / 2 : n - 1 - k / 2; }
                case diverge:     { const auto k = n - 1 - s % n; return k % 2 == 0 ? k / 2 : n - 1 - k / 2; }
                case randomOrder:
                {
                    auto pick = random.nextInt (n);

                    if (n > 1 && pick == lastPick)
                        pick = (pick + 1 + random.nextInt (n - 1)) % n;

                    lastPick = pick;
                    return pick;
                }
                case up:
                case asPlayed:
                default:          return s % n;
            }
        }

        void resetSteps() noexcept override
        {
            held.count = 0;
            latched.count = 0;
            steps = 0;
            lastPick = -1;
        }

        HeldNotes held, latched;
        juce::int64 steps = 0;
        int lastPick = -1;
    };

    //==========================================================================
    class RepeatDevice final : public StepDevice
    {
    public:
        enum { rate, gate, fade };
        using StepDevice::StepDevice;

    private:
        double stepLength() const noexcept override      { return rateLength (get (rate)); }
        bool idle() const noexcept override              { return held.count == 0; }

        void onEvent (EventList& out, const Event& e, const Time&) noexcept override
        {
            if (e.isNoteOn())
            {
                held.add (e.channel(), e.pitch(), e.velocity());
                hits[static_cast<size_t> (e.channel() * 128 + e.pitch())] = 0;
                lastStart[static_cast<size_t> (e.channel() * 128 + e.pitch())] = e.time;
                emit (out, e);
                schedule (Event::noteOff (e.time + noteLength(), e.channel(), e.pitch()));
            }
            else if (e.isNoteOff())
            {
                if (! held.has (e.channel(), e.pitch()))
                    emit (out, e);

                held.remove (e.channel(), e.pitch());
            }
            else
            {
                emit (out, e);
            }
        }

        void onStep (EventList& out, const Time::GridLine& line, const Time& time, double) noexcept override
        {
            const auto tolerance = time.offline ? eps : 2.0e-4;

            for (int i = 0; i < held.count; ++i)
            {
                const auto& note = held.notes[static_cast<size_t> (i)];
                const auto at = static_cast<size_t> (note.channel * 128 + note.pitch);

                if (line.time <= lastStart[at] + tolerance)
                    continue;   // it was played on this very step

                lastStart[at] = line.time;
                const auto count = ++hits[at];
                const auto v = note.velocity * std::pow (1.0 - get (fade) / 100.0, static_cast<double> (count));
                emit (out, Event::noteOn (line.time, note.channel, note.pitch, juce::jmax (1, juce::roundToInt (v))));
                schedule (Event::noteOff (line.time + noteLength(), note.channel, note.pitch));
            }
        }

        double noteLength() const noexcept      { return juce::jmax (1.0 / 128.0, stepLength() * get (gate) / 100.0); }

        void resetSteps() noexcept override     { held.count = 0; }

        HeldNotes held;
        std::array<int, 16 * 128> hits {};
        std::array<double, 16 * 128> lastStart {};
    };

    //==========================================================================
    class EuclidDevice final : public StepDevice
    {
    public:
        enum { stepsParam, pulses, rotate, rate, gate, source, note, velocity, accent };
        enum { heldNotes, heldChord, fixedNote };
        using StepDevice::StepDevice;

    private:
        double stepLength() const noexcept override      { return rateLength (get (rate)); }
        bool idle() const noexcept override              { return getInt (source) == fixedNote || held.count == 0; }
        bool wantsGrid (const Event&) const noexcept override   { return getInt (source) != fixedNote; }

        void onEvent (EventList& out, const Event& e, const Time&) noexcept override
        {
            // A note let go is let go whatever it plays now (it may switch back to the held notes).
            if (e.isNoteOff() && getInt (source) == fixedNote)
                held.remove (e.channel(), e.pitch());

            if (getInt (source) == fixedNote || ! e.isNote())
            {
                emit (out, e);                  // played along with
                return;
            }

            if (e.isNoteOn())
            {
                if (held.count == 0)
                    turn = 0;

                held.add (e.channel(), e.pitch(), e.velocity());
            }
            else
            {
                if (! held.has (e.channel(), e.pitch()))
                    emit (out, e);

                held.remove (e.channel(), e.pitch());
            }
        }

        void onStep (EventList& out, const Time::GridLine& line, const Time& time, double step) noexcept override
        {
            const auto count = juce::jlimit (1, 32, getInt (stepsParam));
            const auto hitsInPattern = juce::jlimit (0, count, getInt (pulses));
            const auto position = static_cast<int> (((line.number + getInt (rotate)) % count + count) % count);

            // Spread the hits as evenly as they go (Bjorklund's rhythms, rotated).
            const auto hit = hitsInPattern > 0 && (position * hitsInPattern) % count < hitsInPattern;

            if (! hit)
                return;

            const bool first = position == 0;
            const auto end = line.time + juce::jmax (1.0 / 128.0, step * get (gate) / 100.0);
            const auto how = getInt (source);

            const auto play = [&] (int channel, int pitch, int v)
            {
                v = clampVelocity (v + (first ? getInt (accent) : 0));
                emit (out, Event::noteOn (line.time, channel, pitch, v));
                schedule (Event::noteOff (end, channel, pitch));
            };

            if (how == fixedNote)
            {
                if (time.isPlayingAt (line.time))
                    play (0, getInt (note), getInt (velocity));

                return;
            }

            if (held.count == 0)
                return;

            if (how == heldChord)
            {
                for (int i = 0; i < held.count; ++i)
                    play (held.notes[static_cast<size_t> (i)].channel, held.notes[static_cast<size_t> (i)].pitch,
                          held.notes[static_cast<size_t> (i)].velocity);
            }
            else
            {
                const auto& n = held.notes[static_cast<size_t> (turn++ % held.count)];
                play (n.channel, n.pitch, n.velocity);
            }
        }

        void resetSteps() noexcept override     { held.count = 0; turn = 0; }

        HeldNotes held;
        int turn = 0;
    };

    //==========================================================================
    class StrumDevice final : public Device
    {
    public:
        enum { time_, direction, tilt };
        enum { up, down, alternate, randomWay };

        explicit StrumDevice (const TypeSpec& s) : Device (s)   { reset(); }

        void process (const EventList& in, EventList& out, const Time& time) noexcept override
        {
            const auto gap = time.fromMilliseconds (get (time_));
            const auto window = time.fromMilliseconds (60.0);
            int i = 0;

            while (i < in.size())
            {
                const auto& e = in[i];
                emitDue (out, e.time);

                if (! e.isNoteOn())
                {
                    if (e.isNoteOff())
                    {
                        const auto delay = delays[slot (e)];
                        delays[slot (e)] = -1.0;

                        if (delay > 0.0)
                            schedule (Event::noteOff (e.time + delay, e.channel(), e.pitch()));
                        else
                            emit (out, e);
                    }
                    else
                    {
                        emit (out, e);
                    }

                    ++i;
                    continue;
                }

                // The notes starting at this moment, as one group, in the strum's order.
                std::array<Event, 32> group {};
                int count = 0;

                while (i < in.size() && in[i].isNoteOn() && in[i].time <= e.time + 1.0e-6 && count < static_cast<int> (group.size()))
                    group[static_cast<size_t> (count++)] = in[i++];

                // Notes a moment after the last group's first are more of the same chord.
                if (e.time > chordStart + window)
                {
                    chordStart = e.time;
                    chordIndex = 0;
                    const auto way = getInt (direction);
                    goingUp = way == up || (way == alternate ? ! goingUp : (way == randomWay ? random.nextBool() : false));

                    if (way == alternate && firstChord)
                        goingUp = true;

                    firstChord = false;
                }

                std::sort (group.begin(), group.begin() + count, [this] (const Event& a, const Event& b)
                {
                    return goingUp ? a.pitch() < b.pitch() : a.pitch() > b.pitch();
                });

                for (int k = 0; k < count; ++k)
                {
                    auto note = group[static_cast<size_t> (k)];
                    const auto index = chordIndex++;
                    const auto when = juce::jmax (note.time, chordStart + index * gap);
                    const auto tiltBy = get (tilt) / 100.0 * index * 8.0;
                    note.data[2] = static_cast<juce::uint8> (clampVelocity (juce::roundToInt (note.velocity() + tiltBy)));
                    delays[slot (note)] = when - note.time;
                    note.time = when;

                    if (when > group[static_cast<size_t> (k)].time + eps)
                        schedule (note);
                    else
                        emit (out, note);
                }
            }

            emitDue (out, time.end);
        }

    private:
        static size_t slot (const Event& e) noexcept     { return static_cast<size_t> (e.channel() * 128 + e.pitch()); }

        void reset() noexcept override
        {
            delays.fill (-1.0);
            chordStart = -1.0e9;
            chordIndex = 0;
            goingUp = true;
            firstChord = true;
        }

        std::array<double, 16 * 128> delays {};
        double chordStart = -1.0e9;
        int chordIndex = 0;
        bool goingUp = true, firstChord = true;
    };

    //==========================================================================
    class HumanizeDevice final : public Device
    {
    public:
        enum { timing, velocity, length };

        explicit HumanizeDevice (const TypeSpec& s) : Device (s)   { reset(); }

        void process (const EventList& in, EventList& out, const Time& time) noexcept override
        {
            const auto spread = time.fromMilliseconds (get (timing));

            for (int i = 0; i < in.size(); ++i)
            {
                auto e = in[i];
                emitDue (out, e.time);
                const auto at = static_cast<size_t> (e.channel() * 128 + e.pitch());

                if (e.isNoteOn())
                {
                    // Live, a note can only come later; Apply moves notes both ways.
                    const auto offset = time.offline ? (random.nextDouble() - 0.5) * spread : random.nextDouble() * spread;
                    const auto v = e.velocity() + (random.nextDouble() * 2.0 - 1.0) * get (velocity);
                    e.data[2] = static_cast<juce::uint8> (clampVelocity (juce::roundToInt (v)));
                    played[at] = e.time;
                    moved[at] = e.time + offset;
                    stretch[at] = 1.0 + (random.nextDouble() * 2.0 - 1.0) * get (length) / 100.0;
                    e.time = moved[at];

                    if (offset > eps)
                        schedule (e);
                    else
                        emit (out, e);
                }
                else if (e.isNoteOff())
                {
                    if (played[at] < -1.0e8)
                    {
                        emit (out, e);   // never saw it start
                        continue;
                    }

                    // As long as it was played, give or take, from where it moved to.
                    auto end = moved[at] + juce::jmax (1.0 / 128.0, (e.time - played[at]) * stretch[at]);

                    if (! time.offline)
                        end = juce::jmax (end, e.time);

                    played[at] = -1.0e9;
                    e.time = end;

                    if (end > in[i].time + eps)
                        schedule (e);
                    else
                        emit (out, e);
                }
                else
                {
                    emit (out, e);
                }
            }

            emitDue (out, time.end);
        }

    private:
        void reset() noexcept override
        {
            played.fill (-1.0e9);
            moved.fill (0.0);
            stretch.fill (1.0);
        }

        std::array<double, 16 * 128> played {}, moved {}, stretch {};
    };

    //==========================================================================
    /** Transpose Map: each of the twelve notes to another (every D an F#, say), or left out. A
        note goes to the nearest of its new name, up or down. */
    class TransposeMapDevice final : public RemapDevice
    {
    public:
        static constexpr int leaveOut = 12;
        using RemapDevice::RemapDevice;

        int transform (const Event& on, const Time&, int* pitches, int* velocities) noexcept override
        {
            const auto from = on.pitch() % 12;
            const auto to = juce::jlimit (0, leaveOut, getInt (from));

            if (to == leaveOut)
                return 0;

            pitches[0] = on.pitch() + ((to - from + 18) % 12) - 6;
            velocities[0] = on.velocity();
            return 1;
        }
    };

    /** Harmonize: each note moved to the nearest note of what another track is playing (its
        chord), or up or down to one. With nothing playing there: the last chord it heard, the
        note as it is, or nothing. */
    /** Follow Chords: each note moved to the chord track's chord where it plays. */
    class FollowChordsDevice final : public RemapDevice
    {
    public:
        enum { fit, part };
        enum { nearest, up, down };
        enum { anyPart, bassPart };

        using RemapDevice::RemapDevice;

        int transform (const Event& on, const Time& time, int* pitches, int* velocities) noexcept override
        {
            const auto chord = time.chordAt (on.time);
            velocities[0] = on.velocity();

            // No chord there (before the first): as it was played.
            if (chord.root < 0)
            {
                pitches[0] = on.pitch();
                return 1;
            }

            // A bass line: the chord's bass note (its root when it has none of its own), the nearest one.
            if (getInt (part) == bassPart)
            {
                const auto target = chord.bass >= 0 ? chord.bass : chord.root;
                const auto pitch = on.pitch();
                const auto below = pitch - ((pitch % 12 - target + 12) % 12);
                const auto above = below + 12;
                const auto how = getInt (fit);
                auto chosen = how == up ? (below == pitch ? pitch : above)
                            : how == down ? below
                            : (pitch - below <= above - pitch ? below : above);

                if (chosen > 127) chosen -= 12;
                if (chosen < 0) chosen += 12;

                pitches[0] = chosen;
                return 1;
            }

            // Any other part: the nearest note of the chord (its bass of its own counts too).
            juce::uint16 notes = 0;

            for (int i = 0; i < 12; ++i)
                if ((chord.notes >> i) & 1)
                    notes = static_cast<juce::uint16> (notes | (1u << ((chord.root + i) % 12)));

            if (chord.bass >= 0)
                notes = static_cast<juce::uint16> (notes | (1u << chord.bass));

            pitches[0] = fitTo (on.pitch(), notes);
            return 1;
        }

    private:
        int fitTo (int pitch, juce::uint16 notes) const noexcept
        {
            const auto how = getInt (fit);
            const auto has = [notes] (int p) { return p >= 0 && p <= 127 && ((notes >> (p % 12)) & 1) != 0; };

            for (int d = 0; d < 12; ++d)
            {
                if (how != up && has (pitch - d))
                    return pitch - d;

                if (how != down && has (pitch + d))
                    return pitch + d;
            }

            return pitch;
        }
    };

    class HarmonizeDevice final : public RemapDevice
    {
    public:
        enum { track, fit, silent };
        enum { nearest, up, down };
        enum { lastChord, asItIs, leaveOut };

        using RemapDevice::RemapDevice;

        int followsTrack() const noexcept override    { return getInt (track); }

        int transform (const Event& on, const Time& time, int* pitches, int* velocities) noexcept override
        {
            const auto source = getInt (track);
            Board::Notes held;

            if (source > 0)
                held = time.offline ? board().offlineHeldAt (source, on.time) : board().heldAt (source, time.sampleAt (on.time));

            // The chord as its twelve notes.
            juce::uint16 chord = 0;

            for (int pitch = 0; pitch < 128; ++pitch)
                if (held.test (static_cast<size_t> (pitch)))
                    chord = static_cast<juce::uint16> (chord | (1u << (pitch % 12)));

            if (chord != 0)
            {
                remembered = chord;
            }
            else
            {
                const auto how = getInt (silent);

                if (how == leaveOut)
                    return 0;

                chord = how == lastChord ? remembered : juce::uint16 (0);
            }

            pitches[0] = chord != 0 ? fitTo (on.pitch(), chord) : on.pitch();
            velocities[0] = on.velocity();
            return 1;
        }

    private:
        void reset() noexcept override
        {
            RemapDevice::reset();
            remembered = 0;
        }

        int fitTo (int pitch, juce::uint16 chord) const noexcept
        {
            const auto how = getInt (fit);
            const auto has = [chord] (int p) { return p >= 0 && p <= 127 && ((chord >> (p % 12)) & 1) != 0; };

            for (int d = 0; d < 12; ++d)
            {
                if (how != up && has (pitch - d))
                    return pitch - d;

                if (how != down && has (pitch + d))
                    return pitch + d;
            }

            return pitch;
        }

        juce::uint16 remembered = 0;     // the last chord heard (audio thread)
    };

    //==========================================================================
    template <typename DeviceType>
    std::unique_ptr<Device> make (const TypeSpec& spec)     { return std::make_unique<DeviceType> (spec); }

    ParamSpec param (const char* id, const char* name, float min, float max, float defaultValue, Unit unit, juce::StringArray choices = {})
    {
        ParamSpec p;
        p.id = id;
        p.name = name;
        p.min = min;
        p.max = max;
        p.defaultValue = defaultValue;
        p.unit = unit;
        p.choices = std::move (choices);
        return p;
    }

    ParamSpec choice (const char* id, const char* name, juce::StringArray choices, int defaultIndex)
    {
        const auto last = static_cast<float> (choices.size() - 1);
        return param (id, name, 0.0f, last, static_cast<float> (defaultIndex), Unit::choice, std::move (choices));
    }

    ParamSpec rateParam (const char* id, const char* name, const char* defaultRate)
    {
        return param (id, name, 0.0f, static_cast<float> (rateNames().size() - 1),
                      static_cast<float> (rateNames().indexOf (defaultRate)), Unit::rate, rateNames());
    }

    ParamSpec toggle (const char* id, const char* name, bool defaultOn)
    {
        return param (id, name, 0.0f, 1.0f, defaultOn ? 1.0f : 0.0f, Unit::toggle);
    }

    const juce::StringArray& noteChoices()
    {
        static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "Leave out" };
        return names;
    }

    /** One of Transpose Map's twelve: a note, and what it becomes (itself at first). */
    ParamSpec mapParam (int note)
    {
        const auto& names = noteChoices();
        auto p = choice (names[note].toRawUTF8(), names[note].toRawUTF8(), names, note);
        p.id = "n" + juce::String (note);
        p.name = names[note] + juce::String::fromUTF8 (" \xe2\x86\x92");
        return p;
    }
}

//==============================================================================
const std::vector<TypeSpec>& types()
{
    static const std::vector<TypeSpec> list = []
    {
        std::vector<TypeSpec> t;

        const auto add = [&t] (const char* id, const char* name, const char* description, std::vector<ParamSpec> params,
                               std::unique_ptr<Device> (*maker) (const TypeSpec&), bool magenta = false)
        {
            TypeSpec spec;
            spec.id = id;
            spec.name = name;
            spec.description = description;
            spec.params = std::move (params);
            spec.make = maker;
            spec.magenta = magenta;
            t.push_back (std::move (spec));
        };

        add ("arpeggiator", "Arpeggiator", "Plays the held notes one at a time on the song's grid.",
             { choice ("style", "Style", { "Up", "Down", "Up-Down", "Down-Up", "Up & Down", "Converge", "Diverge", "As Played", "Random", "Chord" }, 0),
               rateParam ("rate", "Rate", "1/16"),
               param ("gate", "Gate", 5.0f, 200.0f, 80.0f, Unit::percent),
               param ("octaves", "Octaves", 1.0f, 4.0f, 1.0f, Unit::number),
               toggle ("hold", "Hold", false),
               param ("swing", "Swing", 0.0f, 75.0f, 0.0f, Unit::percent),
               param ("velocity", "Velocity", 0.0f, 127.0f, 0.0f, Unit::number) },
             make<ArpDevice>);

        add ("chord", "Chord", "Turns each note into a chord of the song's key.",
             { choice ("shape", "Shape", { "Triad", "Seventh", "Ninth", "Sus2", "Sus4", "Sixth", "Add9", "Power", "Octaves" }, 0),
               choice ("inversion", "Inversion", { "Root", "First", "Second", "Third" }, 0),
               choice ("voicing", "Voicing", { "Close", "Open", "Wide" }, 0),
               param ("added", "Added notes", 10.0f, 100.0f, 85.0f, Unit::percent) },
             make<ChordDevice>);

        add ("scale", "Scale", "Keeps every note in the song's key.",
             { choice ("mode", "Out-of-key notes", { "Nearest", "Up", "Down", "Remove" }, 0) },
             make<ScaleDevice>);

        add ("pitch", "Pitch", "Transposes by semitones and by steps of the song's key.",
             { param ("semitones", "Semitones", -48.0f, 48.0f, 0.0f, Unit::semitones),
               param ("steps", "Steps in key", -14.0f, 14.0f, 0.0f, Unit::steps) },
             make<PitchDevice>);

        add ("velocity", "Velocity", "Shapes how hard the notes play.",
             { choice ("mode", "Mode", { "Scale", "Fixed" }, 0),
               param ("low", "Lowest", 1.0f, 127.0f, 1.0f, Unit::number),
               param ("high", "Highest", 1.0f, 127.0f, 127.0f, Unit::number),
               param ("curve", "Curve", -100.0f, 100.0f, 0.0f, Unit::number),
               param ("random", "Random", 0.0f, 64.0f, 0.0f, Unit::number) },
             make<VelocityDevice>);

        add ("velocityCurve", "Velocity Curve", "Grex's curve: plays softer (drag down) or harder (drag up), the loudest and softest notes kept where they are.",
             { param ("curve", "Curve", -100.0f, 100.0f, 0.0f, Unit::number),
               param ("low", "Lowest", 1.0f, 127.0f, 1.0f, Unit::number),
               param ("high", "Highest", 1.0f, 127.0f, 127.0f, Unit::number) },
             make<VelocityCurveDevice>);

        add ("length", "Note Length", "Fixed lengths, longer notes, or legato.",
             { choice ("mode", "Mode", { "Fixed", "Stretch", "Legato" }, 0),
               rateParam ("length", "Length", "1/8"),
               param ("gate", "Gate", 10.0f, 200.0f, 100.0f, Unit::percent),
               param ("stretch", "Stretch", 100.0f, 800.0f, 200.0f, Unit::percent) },
             make<LengthDevice>);

        add ("echo", "Echo", "Repeats each note, softer each time, stepping through the key.",
             { rateParam ("time", "Time", "1/8 dotted"),
               param ("repeats", "Repeats", 1.0f, 16.0f, 3.0f, Unit::number),
               param ("decay", "Each repeat", 10.0f, 100.0f, 70.0f, Unit::percent),
               param ("steps", "Steps in key", -12.0f, 12.0f, 0.0f, Unit::steps),
               toggle ("dry", "Original", true) },
             make<EchoDevice>);

        add ("repeat", "Note Repeat", "Repeats held notes on the grid, like a drum machine's note repeat.",
             { rateParam ("rate", "Rate", "1/16"),
               param ("gate", "Gate", 10.0f, 100.0f, 50.0f, Unit::percent),
               param ("fade", "Fade", 0.0f, 50.0f, 0.0f, Unit::percent) },
             make<RepeatDevice>);

        add ("strum", "Strum", "Spreads the notes of a chord like a strum.",
             { param ("time", "Time", 0.0f, 200.0f, 30.0f, Unit::ms),
               choice ("direction", "Direction", { "Up", "Down", "Alternate", "Random" }, 0),
               param ("tilt", "Velocity tilt", -100.0f, 100.0f, 0.0f, Unit::percent) },
             make<StrumDevice>);

        add ("humanize", "Humanize", "Small random changes in timing, velocity and length.",
             { param ("timing", "Timing", 0.0f, 60.0f, 12.0f, Unit::ms),
               param ("velocity", "Velocity", 0.0f, 40.0f, 8.0f, Unit::number),
               param ("length", "Length", 0.0f, 50.0f, 10.0f, Unit::percent) },
             make<HumanizeDevice>);

        add ("random", "Random", "Moves some notes to other notes of the key, or leaves them out.",
             { param ("chance", "Chance", 0.0f, 100.0f, 50.0f, Unit::percent),
               param ("range", "Range", 1.0f, 12.0f, 2.0f, Unit::steps),
               choice ("direction", "Direction", { "Both", "Up", "Down" }, 0),
               param ("skip", "Leave out", 0.0f, 100.0f, 0.0f, Unit::percent) },
             make<RandomDevice>);

        add ("range", "Range", "Keeps notes between two pitches: leaves the rest out or folds them in.",
             { param ("low", "Lowest", 0.0f, 127.0f, 36.0f, Unit::pitch),
               param ("high", "Highest", 0.0f, 127.0f, 96.0f, Unit::pitch),
               choice ("mode", "Outside", { "Remove", "Fold in" }, 1) },
             make<RangeDevice>);

        add ("euclidean", "Euclidean", "Euclidean rhythms: plays the held notes, or one note, in evenly spread hits.",
             { param ("steps", "Steps", 2.0f, 32.0f, 16.0f, Unit::number),
               param ("pulses", "Hits", 0.0f, 32.0f, 5.0f, Unit::number),
               param ("rotate", "Rotate", 0.0f, 31.0f, 0.0f, Unit::number),
               rateParam ("rate", "Rate", "1/16"),
               param ("gate", "Gate", 10.0f, 100.0f, 50.0f, Unit::percent),
               choice ("source", "Plays", { "Held notes", "Held chord", "One note" }, 2),
               param ("note", "Note", 0.0f, 127.0f, 36.0f, Unit::pitch),
               param ("velocity", "Velocity", 1.0f, 127.0f, 100.0f, Unit::number),
               param ("accent", "Accent", 0.0f, 40.0f, 15.0f, Unit::number) },
             make<EuclidDevice>);

        add ("followChords", "Follow Chords", "Moves each note to the chord track's chord where it plays (Master Song Channel > Chords).",
             { choice ("fit", "Fit", { "Nearest", "Up", "Down" }, 0),
               choice ("part", "Part", { "Any part: the nearest chord note", "Bass: the chord's root (or its bass)" }, 0) },
             make<FollowChordsDevice>);

        add ("harmonize", "Harmonize", "Moves each note to the chord another track is playing.",
             { param ("track", "Follows", 0.0f, 10000000.0f, 0.0f, Unit::track),
               choice ("fit", "Fit", { "Nearest", "Up", "Down" }, 0),
               choice ("silent", "No chord there", { "Last chord", "As played", "Leave out" }, 0) },
             make<HarmonizeDevice>);

        add ("transposeMap", "Transpose Map", "Turns each note into another: every D an F#, say, or leaves it out.",
             { mapParam (0), mapParam (1), mapParam (2), mapParam (3), mapParam (4), mapParam (5),
               mapParam (6), mapParam (7), mapParam (8), mapParam (9), mapParam (10), mapParam (11) },
             make<TransposeMapDevice>);

        add ("magenta", "Magenta", "A Magenta model plays along: it hears what you play and answers.",
             { choice ("bars", "Window", { "1 bar", "2 bars", "4 bars", "8 bars" }, 1),
               choice ("mode", "Mode", { "Answer", "Follow", "Loop" }, 0),
               param ("temperature", "Surprise", 0.1f, 2.0f, 1.0f, Unit::decimal),
               toggle ("inKey", "In key", true),
               toggle ("thru", "Hear what I play", true) },
             make<MagentaDevice>, true);

        return t;
    }();

    return list;
}

const TypeSpec* findType (const juce::String& id)
{
    for (const auto& spec : types())
        if (spec.id == id)
            return &spec;

    return nullptr;
}

namespace
{
    std::atomic<ExternalMaker> externalMaker { nullptr };
}

void setExternalMaker (ExternalMaker maker) noexcept
{
    externalMaker.store (maker);
}

std::unique_ptr<Device> create (const juce::String& typeId, const juce::var& settings)
{
    const auto* spec = findType (typeId);

    if (spec == nullptr)
    {
        const auto maker = externalMaker.load();
        return maker != nullptr ? maker (typeId, settings) : nullptr;
    }

    auto device = spec->make (*spec);

    if (const auto* params = settings.getProperty ("params", {}).getDynamicObject())
    {
        for (size_t i = 0; i < spec->params.size(); ++i)
        {
            const auto& value = params->getProperty (spec->params[i].id);

            if (value.isDouble() || value.isInt() || value.isInt64() || value.isBool())
                device->set (static_cast<int> (i), static_cast<float> (static_cast<double> (value)));
        }
    }

    if (auto* magenta = dynamic_cast<MagentaDevice*> (device.get()))
    {
        magenta->tool = settings.getProperty ("tool", {}).toString().substring (0, 200);
        magenta->toolName = settings.getProperty ("toolName", {}).toString().substring (0, 200);
        magenta->generator.store (static_cast<bool> (settings.getProperty ("generator", false)));
    }

    return device;
}

//==============================================================================
// Magenta
MagentaDevice::MagentaDevice (const TypeSpec& s)
    : Device (s)
{
    heldSince.fill (-1.0f);
    lastVelocity.fill (100);
}

bool MagentaDevice::takeRequest (Window& into) noexcept
{
    if (requestFifo.getNumReady() == 0)
        return false;

    const auto scope = requestFifo.read (1);
    into = requests[static_cast<size_t> (scope.startIndex1)];
    return true;
}

bool MagentaDevice::giveAnswer (const Window& reply) noexcept
{
    if (answerFifo.getFreeSpace() == 0)
        return false;

    const auto scope = answerFifo.write (1);
    answers[static_cast<size_t> (scope.startIndex1)] = reply;
    return true;
}

void MagentaDevice::reset() noexcept
{
    running = false;
    listening = true;
    expectingAnswer = false;
    havePlaying = false;
    haveWaiting = false;
    phase = Phase::hearing;
    waitedWindows = 0;
    firstWindowOfRun = windowNumber;   // answers for windows heard before this are stale
    nextNote = 0;
    emptyWindows = 0;
    heard.count = 0;
    heldSince.fill (-1.0f);
}

void MagentaDevice::startWindow (double time, double length) noexcept
{
    ++windowNumber;
    windowStart = time;
    windowLength = juce::jmax (0.25, length);
    heard.count = 0;
    heard.number = windowNumber;
    heard.length = windowLength;

    // Notes still held carry on into the new window, from its start.
    for (auto& since : heldSince)
        if (since >= 0.0f)
            since = 0.0f;

    const auto how = getInt (mode);

    // An answer that has come in plays from this window's start; Loop plays its last one again.
    const bool adopted = haveWaiting;

    if (haveWaiting)
    {
        playing = waiting;
        havePlaying = true;
        haveWaiting = false;
    }
    else if (havePlaying && how != loop)
    {
        havePlaying = false;
    }

    nextNote = 0;

    // Answer goes round: a window heard, the answer waited for, played, then hearing again.
    if (how == answer)
    {
        if (phase == Phase::answering)
            phase = Phase::hearing;
        else if (phase == Phase::waiting && adopted)
            phase = Phase::answering;
        else if (phase == Phase::waiting && ++waitedWindows > 2)
            phase = Phase::hearing;        // no answer came
    }
    else
    {
        phase = Phase::hearing;
    }

    listening = phase == Phase::hearing;
}

void MagentaDevice::endWindow (double time, const Time& blockTime) noexcept
{
    // Notes still held end with the window (as far as it's heard).
    for (size_t i = 0; i < heldSince.size(); ++i)
    {
        const auto since = heldSince[i];

        if (since >= 0.0f && listening && heard.count < maxWindowNotes)
        {
            auto& note = heard.notes[static_cast<size_t> (heard.count++)];
            note.start = since;
            note.length = static_cast<float> (juce::jmax (1.0 / 64.0, time - windowStart - since));
            note.pitch = static_cast<juce::uint8> (i % 128);
            note.velocity = static_cast<juce::uint8> (juce::jlimit (1, 127, lastVelocity[i]));
        }
    }

    const bool makesItsOwn = generator.load();
    const auto how = getInt (mode);
    bool asked = false;

    // A tool that makes its own asks every window it hears, except in Loop while its last
    // answer still plays (or one is on its way): then only something played asks again.
    const bool busyLooping = how == loop && (havePlaying || haveWaiting || expectingAnswer);
    const bool wanted = listening && (heard.count > 0 || (makesItsOwn && ! busyLooping));

    if (wanted && requestFifo.getFreeSpace() > 0)
    {
        const auto key = blockTime.keyAt (windowStart);
        heard.number = windowNumber;
        heard.length = juce::jmax (0.25, juce::jmin (windowLength, time - windowStart));
        heard.bpm = blockTime.bpm;
        heard.keyRoot = key.root;
        heard.keyNotes = key.notes;

        const auto scope = requestFifo.write (1);
        requests[static_cast<size_t> (scope.startIndex1)] = heard;
        asked = true;
    }

    if (asked && how == answer)
    {
        phase = Phase::waiting;
        waitedWindows = 0;
    }

    emptyWindows = heard.count > 0 || havePlaying || haveWaiting || asked || phase != Phase::hearing ? 0 : emptyWindows + 1;
    expectingAnswer = asked;
}

void MagentaDevice::takeAnswers() noexcept
{
    while (answerFifo.getNumReady() > 0)
    {
        const auto scope = answerFifo.read (1);
        const auto& reply = answers[static_cast<size_t> (scope.startIndex1)];

        // It plays from the next window's start (one heard before the song stopped or started: never).
        if (reply.number > firstWindowOfRun)
        {
            waiting = reply;
            haveWaiting = true;
        }
    }
}

void MagentaDevice::playAnswer (EventList& out, double from, double to) noexcept
{
    if (! havePlaying)
        return;

    while (nextNote < playing.count)
    {
        const auto& note = playing.notes[static_cast<size_t> (nextNote)];
        const auto at = windowStart + note.start;

        if (at >= to)
            break;

        ++nextNote;

        if (at < from - 1.0e-6)
            continue;   // already past (the answer came late)

        const auto end = juce::jmin (windowStart + windowLength, at + juce::jmax (1.0 / 64.0, static_cast<double> (note.length)));
        emit (out, Event::noteOn (at, 0, note.pitch, note.velocity));
        schedule (Event::noteOff (juce::jmax (at + 1.0 / 128.0, end), 0, note.pitch));
    }
}

void MagentaDevice::process (const EventList& in, EventList& out, const Time& time) noexcept
{
    takeAnswers();

    const auto thruOn = get (thru) > 0.5f;
    const auto windowBars = 1 << juce::jlimit (0, 3, getInt (bars));
    const auto playingNow = time.isPlaying();

    // Song started or stopped: windows start afresh.
    if (running && playingNow != playingMode)
    {
        flush (out, time.start);
        running = false;
    }

    // Where windows begin this block: bar lines (playing) or the free clock (stopped).
    std::array<Time::BarLine, 64> bars_ {};
    const auto numBars = playingNow ? time.barLines (bars_.data(), static_cast<int> (bars_.size())) : 0;
    int nextBar = 0;
    double cursor = time.start;

    const auto boundary = [&] (double at, double length)
    {
        playAnswer (out, cursor, at);
        emitDue (out, at);

        if (running)
            endWindow (at, time);

        running = true;
        playingMode = playingNow;
        startWindow (at, length);
        cursor = at;
    };

    int i = 0;

    while (true)
    {
        // The next boundary in this block.
        double next = time.end;
        double nextLength = 0.0;

        if (playingNow)
        {
            while (nextBar < numBars && (bars_[static_cast<size_t> (nextBar)].bar % windowBars != 0
                                         || bars_[static_cast<size_t> (nextBar)].time < cursor - 1.0e-9))
                ++nextBar;

            if (nextBar < numBars)
            {
                next = bars_[static_cast<size_t> (nextBar)].time;
                nextLength = windowBars * bars_[static_cast<size_t> (nextBar)].length;
            }
        }
        else if (running && windowStart + windowLength < time.end)
        {
            next = juce::jmax (cursor, windowStart + windowLength);
            nextLength = windowBars * time.barLengthAt (next);
        }

        // Events before it.
        while (i < in.size() && in[i].time < next)
        {
            const auto& e = in[i++];

            // Stopped and quiet: the first note starts the windows.
            if (! running && ! playingNow && e.isNoteOn())
            {
                boundary (e.time, windowBars * time.barLengthAt (e.time));
                emptyWindows = 0;
            }

            playAnswer (out, cursor, e.time);
            cursor = juce::jmax (cursor, e.time);

            if (e.isNote() && running && listening)
            {
                const auto at = static_cast<size_t> (e.channel() * 128 + e.pitch());
                const auto offset = static_cast<float> (e.time - windowStart);

                if (e.isNoteOn())
                {
                    heldSince[at] = juce::jmax (0.0f, offset);
                }
                else if (heldSince[at] >= 0.0f)
                {
                    if (heard.count < maxWindowNotes)
                    {
                        auto& note = heard.notes[static_cast<size_t> (heard.count++)];
                        note.start = heldSince[at];
                        note.length = juce::jmax (1.0f / 64.0f, offset - heldSince[at]);
                        note.pitch = static_cast<juce::uint8> (e.pitch());
                        note.velocity = static_cast<juce::uint8> (juce::jmax (1, lastVelocity[at]));
                    }

                    heldSince[at] = -1.0f;
                }

                if (e.isNoteOn())
                    lastVelocity[at] = e.velocity();
            }
            else if (e.isNote())
            {
                heldSince[static_cast<size_t> (e.channel() * 128 + e.pitch())] = -1.0f;
            }

            // What's played goes round its counts (its own ends follow it); an end always gets
            // through, so switching Hear what I play off never leaves a note hanging.
            if (thruOn || ! e.isNoteOn())
                out.add (e);
        }

        if (next >= time.end)
            break;

        boundary (next, nextLength);

        if (playingNow)
            ++nextBar;
    }

    playAnswer (out, cursor, time.end);
    emitDue (out, time.end);

    // Stopped, with nothing heard, asked or answered for a while: the windows rest.
    if (! playingNow && running && emptyWindows >= 2 && pending.isEmpty())
        running = false;
}

//==============================================================================
// The board: what each track plays, for the devices that follow it
Board& board() noexcept
{
    static Board instance;
    return instance;
}

void Board::publish (int trackId, const juce::MidiBuffer& midi) noexcept
{
    if (trackId <= 0)
        return;

    // Its entry, or a free one, or the one longest unheard from.
    Entry* entry = nullptr;
    Entry* oldest = &entries[0];

    for (auto& e : entries)
    {
        if (e.trackId == trackId)
        {
            entry = &e;
            break;
        }

        if (e.block < oldest->block)
            oldest = &e;
    }

    if (entry == nullptr)
    {
        entry = oldest;
        entry->trackId = trackId;
        entry->after.reset();
    }

    entry->before = entry->after;
    entry->count = 0;
    entry->block = block;

    for (const auto metadata : midi)
    {
        if (metadata.numBytes < 3)
            continue;

        const auto type = metadata.data[0] & 0xf0;
        const auto number = metadata.data[1] & 0x7f;
        Change change;
        change.sample = metadata.samplePosition;
        change.pitch = static_cast<juce::uint8> (number);

        if (type == 0x90 && metadata.data[2] > 0)
            change.on = true;
        else if (type == 0x80 || type == 0x90)
            change.on = false;
        else if (type == 0xb0 && (number == 120 || number == 123))
            change.all = true;
        else
            continue;

        if (change.all)
            entry->after.reset();
        else
            entry->after.set (number, change.on);

        if (entry->count < maxChanges)
            entry->changes[static_cast<size_t> (entry->count++)] = change;
    }
}

Board::Notes Board::heldAt (int trackId, int sample) const noexcept
{
    for (const auto& e : entries)
    {
        if (e.trackId != trackId)
            continue;

        // Not played yet this block: what it held at the end of the last one.
        if (e.block != block)
            return e.after;

        auto notes = e.before;

        for (int i = 0; i < e.count; ++i)
        {
            const auto& change = e.changes[static_cast<size_t> (i)];

            if (change.sample > sample)
                break;

            if (change.all)
                notes.reset();
            else
                notes.set (change.pitch, change.on);
        }

        return notes;
    }

    return {};
}

void Board::setOffline (int trackId, std::vector<OfflineNote> notes)
{
    for (auto& [id, list] : offline)
    {
        if (id == trackId)
        {
            list = std::move (notes);
            return;
        }
    }

    offline.emplace_back (trackId, std::move (notes));
}

void Board::clearOffline()
{
    offline.clear();
}

Board::Notes Board::offlineHeldAt (int trackId, double songPosition) const
{
    Notes notes;

    for (const auto& [id, list] : offline)
        if (id == trackId)
            for (const auto& note : list)
                if (note.start <= songPosition + eps && songPosition < note.end - eps && validPitch (note.pitch))
                    notes.set (static_cast<size_t> (note.pitch));

    return notes;
}

//==============================================================================
// Apply
std::vector<Note> runOffline (std::vector<std::unique_ptr<Device>>& devices, std::vector<Note> notes,
                              const TempoMap& map, double from, double to)
{
    // The notes as the song plays them: ends before starts at the same moment. Nothing runs on
    // far past the span: a note that would (a huge length) ends a little after it.
    constexpr double tail = 32.0;
    std::vector<Event> input;
    input.reserve (notes.size() * 2);

    for (const auto& note : notes)
    {
        if (! validPitch (note.pitch) || ! std::isfinite (note.start) || ! std::isfinite (note.length)
            || note.start < from - tail || note.start >= to + tail)
            continue;

        const auto end = juce::jmin (note.start + juce::jmax (1.0 / 128.0, note.length), to + tail);
        input.push_back (Event::noteOn (note.start, note.channel, note.pitch, note.velocity));
        input.push_back (Event::noteOff (juce::jmax (end, note.start + 1.0 / 128.0), note.channel, note.pitch));
    }

    std::stable_sort (input.begin(), input.end(), [] (const Event& a, const Event& b)
    {
        if (a.time != b.time)
            return a.time < b.time;

        return a.isNoteOff() && ! b.isNoteOff();
    });

    if (! input.empty())
    {
        from = juce::jmin (from, input.front().time);
        to = juce::jmax (to, input.back().time);
    }

    // On past the end while the devices still play (echoes, held arpeggios), a few bars at most.
    // However long the clip, Apply takes 10 seconds at most (a Sonify device can be slow).
    constexpr double block = 0.25;
    constexpr double timeLimitMs = 10000.0;
    const auto startedMs = juce::Time::getMillisecondCounterHiRes();
    const auto limit = to + tail;
    Time time;
    EventList a (8192), b (8192);
    std::vector<Event> played;
    size_t next = 0;

    for (auto t = from; t < limit; t += block)
    {
        const auto end = t + block;
        const bool past = t >= to;

        if (past && next >= input.size())
        {
            bool busy = false;

            for (const auto& device : devices)
                busy = busy || device->isBusy();

            if (! busy)
                break;
        }

        time.buildOffline (map, t, end, ! past);
        a.clear();

        while (next < input.size() && input[next].time < end)
            a.add (input[next++]);

        auto* in = &a;
        auto* out = &b;

        for (auto& device : devices)
        {
            out->clear();
            device->process (*in, *out, time);
            out->sortByTime();
            std::swap (in, out);
        }

        for (int i = 0; i < in->size(); ++i)
            played.push_back ((*in)[i]);

        if (juce::Time::getMillisecondCounterHiRes() - startedMs > timeLimitMs)
            break;
    }

    // Whatever still sounds ends where Apply stopped.
    {
        a.clear();

        for (auto& device : devices)
        {
            b.clear();
            device->flush (b, limit);

            for (int i = 0; i < b.size(); ++i)
                played.push_back (b[i]);
        }
    }

    std::stable_sort (played.begin(), played.end(), [] (const Event& x, const Event& y) { return x.time < y.time; });

    // Starts and ends into notes, in the order they started.
    std::vector<Note> result;
    std::array<std::vector<size_t>, 16 * 128> open;

    for (const auto& e : played)
    {
        const auto at = static_cast<size_t> (e.channel() * 128 + e.pitch());

        if (e.isNoteOn())
        {
            Note note;
            note.pitch = e.pitch();
            note.start = e.time;
            note.length = -1.0;
            note.velocity = e.velocity();
            note.channel = e.channel();
            open[at].push_back (result.size());
            result.push_back (note);
        }
        else if (e.isNoteOff() && ! open[at].empty())
        {
            auto& note = result[open[at].front()];
            note.length = juce::jmax (1.0 / 128.0, e.time - note.start);
            open[at].erase (open[at].begin());
        }
    }

    for (auto& note : result)
        if (note.length < 0.0)
            note.length = juce::jmax (1.0 / 128.0, limit - note.start);

    std::stable_sort (result.begin(), result.end(), [] (const Note& x, const Note& y)
    {
        return x.start < y.start || (! (y.start < x.start) && x.pitch < y.pitch);
    });

    return result;
}
}

//==============================================================================
// The chain
bool MidiFxChain::follows (int trackId) const noexcept
{
    for (const auto& slot : slots)
        if (slot->on.load (std::memory_order_relaxed) && slot->device != nullptr && slot->device->followsTrack() == trackId)
            return true;

    return false;
}

bool MidiFxChain::followsAny() const noexcept
{
    for (const auto& slot : slots)
        if (slot->on.load (std::memory_order_relaxed) && slot->device != nullptr && slot->device->followsTrack() > 0)
            return true;

    return false;
}

MidiFxChain::MidiFxChain()
    : first (4096), second (4096), flushed (1024)
{
    slots.reserve (maxDevices);
    others.ensureSize (4096);
}

MidiFxChain::Slot* MidiFxChain::at (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, size()) ? slots[static_cast<size_t> (index)].get() : nullptr;
}

MidiFxChain::Slot* MidiFxChain::find (int slotId) const noexcept
{
    for (const auto& slot : slots)
        if (slot->id == slotId)
            return slot.get();

    return nullptr;
}

int MidiFxChain::indexOf (int slotId) const noexcept
{
    for (size_t i = 0; i < slots.size(); ++i)
        if (slots[i]->id == slotId)
            return static_cast<int> (i);

    return -1;
}

bool MidiFxChain::add (std::unique_ptr<Slot> slot, int position)
{
    if (slot == nullptr || slot->device == nullptr || isFull())
        return false;

    const auto index = position < 0 ? size() : juce::jlimit (0, size(), position);
    slots.insert (slots.begin() + index, std::move (slot));
    requestRelease();
    return true;
}

std::unique_ptr<MidiFxChain::Slot> MidiFxChain::remove (int slotId)
{
    const auto index = indexOf (slotId);

    if (index < 0)
        return nullptr;

    auto slot = std::move (slots[static_cast<size_t> (index)]);
    slots.erase (slots.begin() + index);
    requestRelease();
    return slot;
}

std::unique_ptr<MidiFx::Device> MidiFxChain::replace (int slotId, std::unique_ptr<MidiFx::Device> device)
{
    auto* slot = find (slotId);

    if (slot == nullptr || device == nullptr)
        return {};

    std::swap (slot->device, device);
    requestRelease();
    return device;
}

bool MidiFxChain::move (int slotId, int position)
{
    const auto from = indexOf (slotId);

    if (from < 0)
        return false;

    const auto to = juce::jlimit (0, size() - 1, position);

    if (to == from)
        return true;

    auto slot = std::move (slots[static_cast<size_t> (from)]);
    slots.erase (slots.begin() + from);
    slots.insert (slots.begin() + to, std::move (slot));
    requestRelease();
    return true;
}

void MidiFxChain::rebuild (const std::vector<int>& order, std::vector<std::unique_ptr<Slot>>& fresh,
                           std::vector<std::unique_ptr<Slot>>& gone)
{
    // Everything out (into room reserved beforehand), then back in the order asked.
    for (auto& slot : slots)
        gone.push_back (std::move (slot));

    slots.clear();

    const auto takeFrom = [] (std::vector<std::unique_ptr<Slot>>& list, int id) -> std::unique_ptr<Slot>
    {
        for (auto& slot : list)
            if (slot != nullptr && slot->id == id)
                return std::move (slot);

        return nullptr;
    };

    for (const auto id : order)
    {
        if (isFull())
            break;

        auto slot = takeFrom (gone, id);

        if (slot == nullptr)
            slot = takeFrom (fresh, id);

        if (slot != nullptr)
            slots.push_back (std::move (slot));
    }

    requestRelease();
}

void MidiFxChain::process (juce::MidiBuffer& midi, const Transport& transport, int numSamples) noexcept
{
    const bool release = releaseRequested.exchange (false);

    if (slots.empty() && ! sounding.any())
    {
        // Nothing to do but keep count of the notes held, for when devices come.
        for (const auto metadata : midi)
        {
            if (metadata.numBytes != 3)
                continue;

            const auto status = metadata.data[0] & 0xf0;

            if (status == 0x90 || status == 0x80)
                heldIn[static_cast<size_t> ((metadata.data[0] & 0x0f) * 128 + (metadata.data[1] & 0x7f))] = status == 0x90 && metadata.data[2] > 0;
        }

        wasPlaying = transport.getNumSegments() > 0;
        return;
    }

    time.build (transport, clock, numSamples);
    const bool playingNow = time.isPlaying();
    const bool stopping = wasPlaying && ! playingNow;
    wasPlaying = playingNow;

    // The block's events on the clock; longer messages go round.
    first.clear();
    others.clear();

    for (const auto metadata : midi)
    {
        if (metadata.numBytes >= 1 && metadata.numBytes <= 3)
        {
            MidiFx::Event e;
            e.time = time.timeAtSample (metadata.samplePosition);
            e.size = static_cast<juce::uint8> (metadata.numBytes);
            std::copy (metadata.data, metadata.data + metadata.numBytes, e.data);

            // Every note that comes in gets its own end before it starts again (a clip's two
            // overlapping notes of a pitch come as start, start, end), so the devices, and the
            // count of what the instrument sounds, always see a start and an end in turn.
            if (e.isNote())
            {
                auto& held = heldIn[static_cast<size_t> (e.channel() * 128 + e.pitch())];

                if (e.isNoteOn() && held)
                    first.add (MidiFx::Event::noteOff (e.time, e.channel(), e.pitch()));

                held = e.isNoteOn();
            }

            first.add (e);
        }
        else
        {
            others.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);
        }
    }

    midi.clear();
    second.clear();

    // Everything stops: the devices forget, and the instrument hears an end for each note it has.
    if (release || stopping)
    {
        for (auto& slot : slots)
        {
            flushed.clear();
            slot->device->flush (flushed, time.start);
            slot->wasOn = slot->on.load();
        }

        flushed.clear();
        sounding.releaseAll (flushed, time.start, false);

        for (int i = 0; i < flushed.size(); ++i)
            midi.addEvent (flushed[i].data, flushed[i].size, 0);
    }

    auto* in = &first;
    auto* out = &second;

    for (auto& slot : slots)
    {
        const bool on = slot->on.load();

        // Switched off: what it plays ends, heard by the devices after it.
        if (slot->wasOn && ! on)
        {
            flushed.clear();
            slot->device->flush (flushed, time.start);

            for (int i = 0; i < flushed.size(); ++i)
                in->add (flushed[i]);

            in->sortByTime();
        }

        slot->wasOn = on;

        if (! on)
            continue;

        out->clear();
        slot->device->process (*in, *out, time);
        out->sortByTime();
        std::swap (in, out);
    }

    // To the instrument: a note started again while it sounds is ended first (so every start the
    // instrument hears has its own end, whatever the instrument does with a pitch started twice),
    // and it ends with its last end.
    for (int i = 0; i < in->size(); ++i)
    {
        const auto& e = (*in)[i];

        if (e.isNoteOn())
        {
            if (sounding.isSounding (e.channel(), e.pitch()))
            {
                const auto end = MidiFx::Event::noteOff (e.time, e.channel(), e.pitch());
                midi.addEvent (end.data, end.size, time.sampleAt (e.time));
            }

            sounding.on (e.channel(), e.pitch());
        }
        else if (e.isNoteOff() && ! sounding.off (e.channel(), e.pitch()))
        {
            continue;
        }

        midi.addEvent (e.data, e.size, time.sampleAt (e.time));
    }

    for (const auto metadata : others)
        midi.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);

    clock = time.end;
}
