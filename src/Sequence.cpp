// D:\workspace\Fanan_web_hub\src\Sequence.cpp

#include "Sequence.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <tuple>
#include <utility>

namespace
{
    constexpr juce::uint8 noteOn = 0x90, noteOff = 0x80, pitchBendStatus = 0xe0, aftertouchStatus = 0xd0, controllerStatus = 0xb0;
    constexpr int sustainPedal = 64;

    // At the same moment: notes end, controllers go back to rest (a clip ending), controllers
    // move (a clip starting there sets its own), then notes start, already bent.
    enum Order : juce::uint8 { ending = 0, resting = 1, moving = 2, starting = 3 };

    Sequence::Event controlEvent (double ppq, juce::uint8 status, int value, juce::uint8 order)
    {
        if (status == pitchBendStatus)
        {
            const auto raw = juce::jlimit (0, 16383, value + 8192);
            return { ppq, status, static_cast<juce::uint8> (raw & 0x7f), static_cast<juce::uint8> ((raw >> 7) & 0x7f), order };
        }

        return { ppq, status, static_cast<juce::uint8> (juce::jlimit (0, 127, value)), 0, order };
    }

    int valueOf (const Sequence::Event& event) noexcept
    {
        if (event.status == pitchBendStatus)
            return ((event.data2 & 0x7f) << 7 | (event.data1 & 0x7f)) - 8192;

        return event.data1 & 0x7f;
    }

    /** A clip's line of one controller: the value in effect where its window opens (a trim may
        hide the point that set it), its points in the window, and rest at its right edge. */
    void addControls (std::vector<Sequence::Event>& events, const MidiClip& clip, const std::vector<MidiControlPoint>& points,
                      juce::uint8 status)
    {
        constexpr int rest = 0;

        if (points.empty())
            return;

        size_t first = 0;
        int before = rest;

        while (first < points.size() && points[first].time < clip.offset)
            before = points[first++].value;

        int last = rest;

        if (before != rest)
        {
            events.push_back (controlEvent (clip.start, status, before, moving));
            last = before;
        }

        for (auto i = first; i < points.size() && points[i].time < clip.offset + clip.length; ++i)
        {
            events.push_back (controlEvent (clip.songPosition (points[i].time), status, points[i].value, moving));
            last = points[i].value;
        }

        if (last != rest)
            events.push_back (controlEvent (clip.start + clip.length, status, rest, resting));
    }

    /** A clip's controller automation (its "cc:<n>" lines) as controller events: the value where
        its window opens, then each change of the value as the lines go (looked at every 1/64 of a
        quarter note), within its window. The sustain pedal is let go at its end. */
    void addControllerLines (std::vector<Sequence::Event>& events, const MidiClip& clip, std::bitset<128>& used)
    {
        constexpr double step = 1.0 / 64.0;

        for (const auto& line : clip.envelopes)
        {
            if (! line.key.startsWith ("cc:") || line.points.empty())
                continue;

            const auto controller = line.key.fromFirstOccurrenceOf ("cc:", false, false).getIntValue();

            if (controller < 0 || controller > 127)
                continue;

            used.set (static_cast<size_t> (controller));
            const auto event = [controller] (double ppq, int value, juce::uint8 order)
            {
                return Sequence::Event { ppq, controllerStatus, static_cast<juce::uint8> (controller),
                                         static_cast<juce::uint8> (juce::jlimit (0, 127, value)), order };
            };

            const auto from = clip.offset, to = clip.offset + clip.length;
            auto last = juce::roundToInt (ClipEnvelope::valueAt (line.points, from));
            events.push_back (event (clip.start, last, moving));

            // Each stretch between points, looked at closely; a flat one sends nothing.
            for (auto t = from + step; t < to - 1.0e-9; t += step)
            {
                const auto value = juce::roundToInt (ClipEnvelope::valueAt (line.points, t));

                if (value != last)
                {
                    events.push_back (event (clip.songPosition (t), value, moving));
                    last = value;
                }
            }

            if (controller == sustainPedal && last > 0)
                events.push_back (event (clip.start + clip.length, 0, resting));
        }
    }

    /** The last of these events before ppq: its value, or rest when there's none. */
    int valueBefore (const std::vector<Sequence::Event>& list, double ppq) noexcept
    {
        const auto after = std::lower_bound (list.begin(), list.end(), ppq, [] (const Sequence::Event& e, double t) { return e.ppq < t; });
        return after == list.begin() ? 0 : valueOf (*std::prev (after));
    }

    const char* const occurrenceNames[] { "", "first", "notFirst", "prev", "noPrev", "prevKey", "noPrevKey" };

    /** A number from a var, or a fallback when it isn't one. */
    double numberOf (const juce::var& value, double fallback) noexcept
    {
        if (value.isInt() || value.isInt64() || value.isDouble() || value.isBool())
        {
            const auto v = static_cast<double> (value);
            return std::isfinite (v) ? v : fallback;
        }

        return fallback;
    }

    /** SplitMix64: a well spread number from any other. */
    juce::uint64 mix (juce::uint64 x) noexcept
    {
        x += 0x9e3779b97f4a7c15ull;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
        return x ^ (x >> 31);
    }

    /** A roll in [0, 1) for a note on a pass: the same every time that pass plays. */
    double roll (juce::uint64 key, int pass, juce::uint64 salt) noexcept
    {
        const auto x = mix (key ^ mix (static_cast<juce::uint64> (static_cast<juce::uint32> (pass)) * 0x100000001b3ull + salt));
        return static_cast<double> (x >> 11) * (1.0 / 9007199254740992.0);
    }

    /** Two clips with the same content, as it plays (copies of one pattern). */
    bool sameContent (const MidiClip& a, const MidiClip& b) noexcept
    {
        if (! juce::exactlyEqual (a.offset, b.offset) || ! juce::exactlyEqual (a.length, b.length) || a.seed != b.seed
            || a.notes.size() != b.notes.size())
            return false;

        for (size_t i = 0; i < a.notes.size(); ++i)
            if (! a.notes[i].sameAs (b.notes[i]))
                return false;

        return true;
    }
}

//==============================================================================
bool NoteOperators::isDefault() const noexcept
{
    return operator== (NoteOperators());
}

bool NoteOperators::operator== (const NoteOperators& other) const noexcept
{
    // Settings that change nothing compare equal: a mask with no cycle, a curve with no repeats.
    const auto cycle = recurrenceLength >= 2 ? recurrenceLength : 0;
    const auto otherCycle = other.recurrenceLength >= 2 ? other.recurrenceLength : 0;
    const auto bits = cycle > 0 ? (recurrenceMask & ((1 << cycle) - 1)) : 0;
    const auto otherBits = otherCycle > 0 ? (other.recurrenceMask & ((1 << otherCycle) - 1)) : 0;

    return chance == other.chance && velocitySpread == other.velocitySpread
        && repeatCount == other.repeatCount && juce::exactlyEqual (repeatRate, other.repeatRate)
        && (repeats() ? repeatCurve == other.repeatCurve && repeatVelocity == other.repeatVelocity
                      : ! other.repeats())
        && occurrence == other.occurrence && cycle == otherCycle && bits == otherBits;
}

juce::var NoteOperators::toVar() const
{
    auto* object = new juce::DynamicObject();

    if (chance != 100)          object->setProperty ("c", chance);
    if (velocitySpread != 0)    object->setProperty ("vs", velocitySpread);

    if (repeats())
    {
        if (repeatCount >= 2)   object->setProperty ("rn", repeatCount);
        else                    object->setProperty ("rr", repeatRate);

        if (repeatCurve != 0)   object->setProperty ("rc", repeatCurve);
        if (repeatVelocity != 0) object->setProperty ("rv", repeatVelocity);
    }

    if (occurrence > always && occurrence <= withoutPreviousKey)
        object->setProperty ("oc", occurrenceNames[occurrence]);

    if (recurrenceLength >= 2)
    {
        object->setProperty ("rl", recurrenceLength);
        object->setProperty ("rm", recurrenceMask & ((1 << recurrenceLength) - 1));
    }

    return juce::var (object);
}

NoteOperators NoteOperators::fromVar (const juce::var& value)
{
    NoteOperators ops;

    if (! value.isObject())
        return ops;

    const auto whole = [&value] (const char* name, int fallback, int low, int high)
    {
        return juce::jlimit (low, high, juce::roundToInt (numberOf (value.getProperty (name, {}), fallback)));
    };

    ops.chance = whole ("c", 100, 0, 100);
    ops.velocitySpread = whole ("vs", 0, 0, 100);
    ops.repeatCount = whole ("rn", 0, 0, maxRepeats);

    if (ops.repeatCount < 2)
    {
        ops.repeatCount = 0;
        const auto rate = numberOf (value.getProperty ("rr", {}), 0.0);
        ops.repeatRate = rate >= 1.0 / 128.0 && rate <= 16.0 ? rate : 0.0;
    }

    ops.repeatCurve = whole ("rc", 0, -100, 100);
    ops.repeatVelocity = whole ("rv", 0, -100, 100);

    const auto occurrence = value.getProperty ("oc", {}).toString();

    for (int i = withoutPreviousKey; i > always; --i)
        if (occurrence == occurrenceNames[i])
            ops.occurrence = i;

    ops.recurrenceLength = whole ("rl", 0, 0, 8);

    if (ops.recurrenceLength < 2)
        ops.recurrenceLength = 0;
    else
        ops.recurrenceMask = whole ("rm", 0xff, 0, 0xff) & ((1 << ops.recurrenceLength) - 1);

    return ops;
}

//==============================================================================
juce::var MidiNote::toVar() const
{
    juce::Array<juce::var> values { pitch, start, length, juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f)) };
    const auto plain = ops.isDefault();

    if (muted || ! plain)
        values.add (muted ? 1 : 0);

    if (! plain)
        values.add (ops.toVar());

    return values;
}

bool MidiNote::fromVar (const juce::var& value, MidiNote& note)
{
    const auto* fields = value.getArray();

    if (fields == nullptr || fields->size() < 4)
        return false;

    note = {};
    note.pitch = juce::roundToInt (numberOf ((*fields)[0], -1.0));
    note.start = numberOf ((*fields)[1], 0.0);
    note.length = numberOf ((*fields)[2], 0.25);
    note.velocity = static_cast<float> (numberOf ((*fields)[3], 100.0) / 127.0);
    note.muted = fields->size() >= 5 && numberOf ((*fields)[4], 0.0) > 0.5;

    if (fields->size() >= 6)
        note.ops = NoteOperators::fromVar ((*fields)[5]);

    return true;
}

bool MidiNote::sameAs (const MidiNote& other) const noexcept
{
    return pitch == other.pitch && juce::exactlyEqual (start, other.start) && juce::exactlyEqual (length, other.length)
        && juce::exactlyEqual (velocity, other.velocity) && muted == other.muted && ops == other.ops;
}

//==============================================================================
void MidiClip::tidy (std::vector<MidiControlPoint>& points, int minimum, int maximum)
{
    points.erase (std::remove_if (points.begin(), points.end(), [] (const MidiControlPoint& point) { return ! std::isfinite (point.time); }),
                  points.end());

    for (auto& point : points)
        point.value = juce::jlimit (minimum, maximum, point.value);

    std::stable_sort (points.begin(), points.end(), [] (const MidiControlPoint& a, const MidiControlPoint& b) { return a.time < b.time; });

    // One value per moment: the last one given.
    std::vector<MidiControlPoint> kept;
    kept.reserve (points.size());

    for (const auto& point : points)
    {
        if (! kept.empty() && juce::exactlyEqual (kept.back().time, point.time))
            kept.back() = point;
        else
            kept.push_back (point);
    }

    points = std::move (kept);
}

//==============================================================================
std::unique_ptr<const Sequence> Sequence::build (const std::vector<MidiClip>& clips, int quietControlsClip, const Sequence* previous)
{
    auto sequence = std::make_unique<Sequence>();
    auto& events = sequence->events;

    // The clips whose notes have operators, and among them the copies placed back to back (one
    // pattern repeated): a chain, whose passes count copy by copy.
    const auto playable = [] (const MidiClip& clip, const MidiNote& note)
    {
        return ! note.muted && clip.shows (note) && note.pitch >= 0 && note.pitch <= 127;
    };

    std::vector<int> chainOf (clips.size(), -1), copyOf (clips.size(), 0);
    std::vector<size_t> operated;

    for (size_t i = 0; i < clips.size(); ++i)
    {
        const auto& clip = clips[i];

        if (clip.isAudio() || clip.muted)
            continue;

        if (std::any_of (clip.notes.begin(), clip.notes.end(), [&] (const MidiNote& n) { return playable (clip, n) && ! n.ops.isDefault(); }))
            operated.push_back (i);
    }

    std::stable_sort (operated.begin(), operated.end(), [&clips] (size_t a, size_t b) { return clips[a].start < clips[b].start; });

    int chains = 0;
    std::vector<size_t> chainEnd;   // each chain's last copy so far

    for (const auto i : operated)
    {
        const auto& clip = clips[i];

        for (int c = 0; c < chains && chainOf[i] < 0; ++c)
        {
            const auto& last = clips[chainEnd[static_cast<size_t> (c)]];

            if (std::abs (last.start + last.length - clip.start) < 1.0e-6 && sameContent (last, clip))
            {
                chainOf[i] = c;
                copyOf[i] = copyOf[chainEnd[static_cast<size_t> (c)]] + 1;
                chainEnd[static_cast<size_t> (c)] = i;
            }
        }

        if (chainOf[i] < 0)
        {
            chainOf[i] = chains++;
            chainEnd.push_back (i);
        }
    }

    for (size_t i = 0; i < clips.size(); ++i)
    {
        const auto& clip = clips[i];

        if (clip.isAudio() || clip.muted)
            continue;

        const auto withOperators = chainOf[i] >= 0;

        for (const auto& note : clip.notes)
        {
            // Only notes the clip shows play (a muted one doesn't); a note running over the clip's
            // end is cut there.
            if (! playable (clip, note))
                continue;

            const auto on = clip.songPosition (note.start);
            const auto off = juce::jmin (on + juce::jmax (note.length, minimumNoteLength), clip.start + clip.length);
            const auto pitch = static_cast<juce::uint8> (note.pitch);
            const auto velocity = static_cast<juce::uint8> (juce::jlimit (1, 127, juce::roundToInt (note.velocity * 127.0f)));

            if (! withOperators)
            {
                events.push_back ({ on, noteOn, pitch, velocity, starting });
                events.push_back ({ off, noteOff, pitch, 0, ending });
                continue;
            }

            // In a clip with operators every note is followed (the ones with none count as played
            // for the notes after them).
            OperatorNote entry;
            entry.ops = note.ops;
            entry.chain = chainOf[i];
            entry.clip = static_cast<int> (i);
            entry.copy = copyOf[i];
            entry.pitch = pitch;
            entry.key = mix (static_cast<juce::uint64> (clip.seed) * 0x1000193ull
                             ^ mix (static_cast<juce::uint64> (note.pitch) << 40
                                    ^ static_cast<juce::uint64> (static_cast<juce::int64> (std::llround (note.start * 3840.0)))));
            const auto index = static_cast<int> (sequence->operatorNotes.size());
            sequence->operatorNotes.push_back (entry);

            const auto& ops = note.ops;
            const auto span = off - on;

            if (! ops.repeats() || span <= minimumNoteLength * 2.0)
            {
                events.push_back ({ on, noteOn, pitch, velocity, starting, index, true });
                events.push_back ({ off, noteOff, pitch, 0, ending, index, true });
                continue;
            }

            // Repeats: evenly through its length (a count), or every note value (a rate), the curve
            // crowding them to its start or its end; the velocity going towards the last one's.
            auto count = ops.repeatCount >= 2 ? ops.repeatCount
                                              : static_cast<int> (std::ceil (span / ops.repeatRate - 1.0e-3));
            count = juce::jlimit (1, NoteOperators::maxRepeats, count);

            const auto exponent = std::pow (4.0, -ops.repeatCurve / 100.0);
            const auto placeOf = [&] (int k)
            {
                if (k >= count)
                    return 1.0;

                const auto share = ops.repeatCount >= 2 ? static_cast<double> (k) / count : juce::jmin (1.0, k * ops.repeatRate / span);
                return std::pow (share, exponent);
            };

            const auto from = static_cast<double> (velocity);
            const auto change = ops.repeatVelocity / 100.0;
            const auto to = change >= 0.0 ? from + change * (127.0 - from) : from + change * from;

            // The repeats a curve crowds closer than the shortest note go (and one that would be
            // shorter than that at the end): the one before lasts until the next that stays. The
            // first always plays, where the note starts.
            std::array<int, NoteOperators::maxRepeats> kept {};
            int keptCount = 0;

            for (int k = 0; k < count; ++k)
                if (k == 0 || ((placeOf (k) - placeOf (kept[static_cast<size_t> (keptCount - 1)])) * span >= minimumNoteLength
                               && (1.0 - placeOf (k)) * span >= minimumNoteLength))
                    kept[static_cast<size_t> (keptCount++)] = k;

            for (int r = 0; r < keptCount; ++r)
            {
                const auto k = kept[static_cast<size_t> (r)];
                const auto a = on + placeOf (k) * span;
                const auto b = r + 1 < keptCount ? on + placeOf (kept[static_cast<size_t> (r + 1)]) * span : off;
                const auto v = count > 1 ? from + (to - from) * k / (count - 1) : from;
                const auto level = static_cast<juce::uint8> (juce::jlimit (1, 127, juce::roundToInt (v)));
                events.push_back ({ a, noteOn, pitch, level, starting, index, r == 0 });
                events.push_back ({ b, noteOff, pitch, 0, ending, index, r == 0 });
            }
        }

        if (clip.id != quietControlsClip || quietControlsClip == 0)
        {
            addControls (events, clip, clip.pitchBend, pitchBendStatus);
            addControls (events, clip, clip.aftertouch, aftertouchStatus);
        }

        if (! clip.muted)
            addControllerLines (events, clip, sequence->controllersUsed);
    }

    // By time; at the same time a note-off comes first (so a repeated note plays again), then
    // controllers going to rest, then controllers moving, then note-ons.
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b)
    {
        if (a.ppq < b.ppq) return true;
        if (b.ppq < a.ppq) return false;
        return a.order < b.order;
    });

    for (const auto& event : events)
    {
        if (event.status == pitchBendStatus)
            sequence->bends.push_back (event);
        else if (event.status == aftertouchStatus)
            sequence->pressures.push_back (event);
        else if (event.status == controllerStatus)
            sequence->controllerEvents.push_back (event);
    }

    sequence->chains.resize (static_cast<size_t> (chains));
    sequence->noteStates.resize (sequence->operatorNotes.size());

    for (const auto& clip : clips)
        sequence->clipIds.push_back (clip.id);

    // The one it replaces: its notes found among these by their clip and what they roll from (the
    // seed, the key, the start), its chains by those notes, its clips by their ids. Worked out
    // here, so carryFrom only copies.
    if (previous != nullptr)
    {
        const auto& before = previous->operatorNotes;
        std::vector<std::tuple<int, juce::uint64, int>> known;   // clip id, key, its place
        known.reserve (before.size());

        for (size_t j = 0; j < before.size(); ++j)
        {
            const auto clip = static_cast<size_t> (before[j].clip);
            known.emplace_back (clip < previous->clipIds.size() ? previous->clipIds[clip] : 0, before[j].key, static_cast<int> (j));
        }

        std::sort (known.begin(), known.end());

        sequence->noteFrom.assign (sequence->operatorNotes.size(), -1);
        sequence->chainFrom.assign (static_cast<size_t> (chains), -1);

        for (size_t i = 0; i < sequence->operatorNotes.size(); ++i)
        {
            const auto& note = sequence->operatorNotes[i];
            const auto clipId = clips[static_cast<size_t> (note.clip)].id;
            const auto found = std::lower_bound (known.begin(), known.end(), std::make_tuple (clipId, note.key, -1));

            if (found == known.end() || std::get<0> (*found) != clipId || std::get<1> (*found) != note.key)
                continue;

            const auto j = std::get<2> (*found);
            sequence->noteFrom[i] = j;

            if (auto& chain = sequence->chainFrom[static_cast<size_t> (note.chain)]; chain < 0)
                chain = before[static_cast<size_t> (j)].chain;
        }

        std::map<int, int> placeOf;   // clip id -> its place here

        for (size_t i = clips.size(); i-- > 0;)
            placeOf[clips[i].id] = static_cast<int> (i);

        sequence->clipFrom.assign (previous->clipIds.size(), -1);

        for (size_t k = 0; k < previous->clipIds.size(); ++k)
            if (const auto place = placeOf.find (previous->clipIds[k]); place != placeOf.end())
                sequence->clipFrom[k] = place->second;
    }

    return sequence;
}

void Sequence::carryFrom (const Sequence& previous) const noexcept
{
    for (size_t i = 0; i < noteStates.size() && i < noteFrom.size(); ++i)
        if (const auto j = noteFrom[i]; juce::isPositiveAndBelow (j, static_cast<int> (previous.noteStates.size())))
            noteStates[i] = previous.noteStates[static_cast<size_t> (j)];

    for (size_t c = 0; c < chains.size() && c < chainFrom.size(); ++c)
    {
        const auto k = chainFrom[c];

        if (! juce::isPositiveAndBelow (k, static_cast<int> (previous.chains.size())))
            continue;

        // Its pass goes on, in the copy it was playing (found by its clip's id: places change).
        chains[c] = previous.chains[static_cast<size_t> (k)];
        const auto playing = chains[c].clip;
        chains[c].clip = juce::isPositiveAndBelow (playing, static_cast<int> (clipFrom.size())) ? clipFrom[static_cast<size_t> (playing)] : -1;
    }
}

//==============================================================================
void Sequence::startRun() const noexcept
{
    for (auto& chain : chains)
        chain = {};

    for (auto& state : noteStates)
        state = {};
}

void Sequence::jumped() const noexcept
{
    for (auto& chain : chains)
        chain.clip = -1;
}

bool Sequence::decide (const Event& event, juce::uint8& velocity) const noexcept
{
    if (event.note < 0 || static_cast<size_t> (event.note) >= operatorNotes.size())
        return true;

    const auto& note = operatorNotes[static_cast<size_t> (event.note)];
    auto& state = noteStates[static_cast<size_t> (event.note)];

    // A note's end: it ends what it started.
    if (event.status != noteOn)
        return state.plays;

    auto& chain = chains[static_cast<size_t> (note.chain)];

    // Into a copy of the clip (or into the clip again, the loop gone round): the next pass. Where
    // playback starts, a copy counts from its place among the copies.
    if (chain.clip != note.clip)
    {
        chain.pass = juce::jmax (note.copy, chain.last + 1);
        chain.last = chain.pass;
        chain.clip = note.clip;
    }

    // Decided once a pass, by its first repeat (or by the first one heard, started in the middle).
    if (event.first || state.pass != chain.pass)
    {
        const auto& ops = note.ops;
        const auto pass = chain.pass;
        bool plays = true;

        switch (ops.occurrence)
        {
            case NoteOperators::first:              plays = pass == 0; break;
            case NoteOperators::notFirst:           plays = pass > 0; break;
            case NoteOperators::withPrevious:       plays = chain.previousPlayed; break;
            case NoteOperators::withoutPrevious:    plays = ! chain.previousPlayed; break;
            case NoteOperators::withPreviousKey:    plays = ! chain.keyMissed.test (note.pitch); break;
            case NoteOperators::withoutPreviousKey: plays = chain.keyMissed.test (note.pitch); break;
            default: break;
        }

        if (ops.recurrenceLength >= 2)
            plays = plays && ((ops.recurrenceMask >> (pass % ops.recurrenceLength)) & 1) != 0;

        if (ops.chance < 100)
            plays = plays && roll (note.key, pass, 1) * 100.0 < ops.chance;

        state.spread = ops.velocitySpread > 0 ? juce::roundToInt ((roll (note.key, pass, 2) * 2.0 - 1.0) * ops.velocitySpread * 1.27) : 0;
        state.plays = plays;
        state.pass = pass;

        // What the notes after it go by.
        chain.previousPlayed = plays;
        chain.keyMissed.set (note.pitch, ! plays);
    }

    velocity = static_cast<juce::uint8> (juce::jlimit (1, 127, velocity + state.spread));
    return state.plays;
}

int Sequence::pitchBendBefore (double ppq) const noexcept   { return valueBefore (bends, ppq); }
int Sequence::aftertouchBefore (double ppq) const noexcept  { return valueBefore (pressures, ppq); }

int Sequence::controllerBefore (int controller, double ppq) const noexcept
{
    auto at = std::lower_bound (controllerEvents.begin(), controllerEvents.end(), ppq,
                                [] (const Event& e, double t) { return e.ppq < t; });

    while (at != controllerEvents.begin())
    {
        --at;

        if (at->data1 == controller)
            return at->data2;
    }

    return -1;
}

//==============================================================================
double ClipEnvelope::valueAt (const std::vector<ClipEnvelopePoint>& points, double time) noexcept
{
    if (points.empty())
        return 0.0;

    if (time <= points.front().time)
        return points.front().value;

    if (time >= points.back().time)
        return points.back().value;

    const auto after = std::upper_bound (points.begin(), points.end(), time,
                                         [] (double t, const ClipEnvelopePoint& p) { return t < p.time; });
    const auto& b = *after;
    const auto& a = *(after - 1);
    const auto span = b.time - a.time;
    return span > 0.0 ? a.value + (b.value - a.value) * (time - a.time) / span : b.value;
}

bool ClipEnvelope::isKey (const juce::String& key, bool audio)
{
    if (audio)
        return key == "gain" || key == "pan"
            || (key.startsWith ("fx:") && key.substring (3).containsOnly ("0123456789") && key.length() > 3);

    if (! key.startsWith ("cc:") || key.length() <= 3 || ! key.substring (3).containsOnly ("0123456789"))
        return false;

    const auto controller = key.substring (3).getIntValue();
    return controller >= 0 && controller <= 127;
}

std::pair<double, double> ClipEnvelope::rangeOf (const juce::String& key)
{
    if (key == "gain")         return { -60.0, 12.0 };
    if (key == "pan")          return { -1.0, 1.0 };
    if (key.startsWith ("cc:")) return { 0.0, 127.0 };
    return { -1.0e12, 1.0e12 };   // a plugin's parameter: its own range, kept by the plugin
}

void ClipEnvelope::tidy (std::vector<ClipEnvelope>& envelopes, bool audio)
{
    std::vector<ClipEnvelope> kept;

    for (auto& line : envelopes)
    {
        if (! isKey (line.key, audio) || std::any_of (kept.begin(), kept.end(), [&line] (const ClipEnvelope& k) { return k.key == line.key; }))
            continue;

        const auto [lowest, highest] = rangeOf (line.key);
        auto& points = line.points;
        points.erase (std::remove_if (points.begin(), points.end(), [] (const ClipEnvelopePoint& p)
                                      { return ! std::isfinite (p.time) || ! std::isfinite (p.value) || p.time < -1.0e6 || p.time > 1.0e7; }),
                      points.end());

        for (auto& point : points)
            point.value = juce::jlimit (lowest, highest, point.value);

        std::stable_sort (points.begin(), points.end(), [] (const ClipEnvelopePoint& a, const ClipEnvelopePoint& b) { return a.time < b.time; });

        if (points.size() > 20000)
            points.resize (20000);

        if (! points.empty())
            kept.push_back (std::move (line));
    }

    envelopes = std::move (kept);
}

//==============================================================================
void Sequencer::sendPitchBend (int value, juce::MidiBuffer& out, int sampleOffset) noexcept
{
    const auto raw = juce::jlimit (0, 16383, value + 8192);
    const juce::uint8 message[] = { pitchBendStatus, static_cast<juce::uint8> (raw & 0x7f), static_cast<juce::uint8> ((raw >> 7) & 0x7f) };
    out.addEvent (message, 3, sampleOffset);
    pitchBendNow = value;
}

void Sequencer::sendAftertouch (int value, juce::MidiBuffer& out, int sampleOffset) noexcept
{
    const juce::uint8 message[] = { aftertouchStatus, static_cast<juce::uint8> (juce::jlimit (0, 127, value)) };
    out.addEvent (message, 2, sampleOffset);
    aftertouchNow = value;
}

void Sequencer::releaseAll (juce::MidiBuffer& out, int sampleOffset) noexcept
{
    for (int pitch = 0; pitch < 128; ++pitch)
    {
        if (! sounding.test (static_cast<size_t> (pitch)))
            continue;

        const juce::uint8 message[] = { noteOff, static_cast<juce::uint8> (pitch), 0 };
        out.addEvent (message, 3, sampleOffset);
    }

    sounding.reset();

    // Nothing left bent or pressed.
    if (pitchBendNow != 0)
        sendPitchBend (0, out, sampleOffset);

    if (aftertouchNow != 0)
        sendAftertouch (0, out, sampleOffset);

    // Nor the sustain pedal a clip's automation put down.
    if (std::exchange (sustainDown, false))
    {
        const juce::uint8 message[] = { controllerStatus, static_cast<juce::uint8> (sustainPedal), 0 };
        out.addEvent (message, 3, sampleOffset);
    }
}

void Sequencer::chase (const Sequence& sequence, double ppq, juce::MidiBuffer& out, int sampleOffset) noexcept
{
    // Playing from the middle of a bend: the instrument hears where it stands first.
    if (const auto bend = sequence.pitchBendBefore (ppq); bend != pitchBendNow)
        sendPitchBend (bend, out, sampleOffset);

    if (const auto pressure = sequence.aftertouchBefore (ppq); pressure != aftertouchNow)
        sendAftertouch (pressure, out, sampleOffset);

    // The clips' controller automation: each controller where it stands.
    const auto& used = sequence.getControllers();

    if (used.any())
    {
        for (int controller = 0; controller < 128; ++controller)
        {
            if (! used.test (static_cast<size_t> (controller)))
                continue;

            if (const auto value = sequence.controllerBefore (controller, ppq); value >= 0)
            {
                const juce::uint8 message[] = { controllerStatus, static_cast<juce::uint8> (controller), static_cast<juce::uint8> (value) };
                out.addEvent (message, 3, sampleOffset);

                if (controller == sustainPedal)
                    sustainDown = value >= 64;
            }
        }
    }
}

void Sequencer::render (const Sequence* sequence, const Transport& transport, juce::MidiBuffer& out) noexcept
{
    // Asked to let go (the clips changed under it): what's playing stops, and the controllers
    // are found again where they stand.
    bool chaseDue = chaseRequested.exchange (false);

    if (releaseRequested.exchange (false))
    {
        releaseAll (out, 0);
        chaseDue = true;
    }

    const auto segments = transport.getNumSegments();

    if (segments == 0)
    {
        if (wasPlaying)
            releaseAll (out, 0);          // stopped

        wasPlaying = false;
        return;
    }

    for (int i = 0; i < segments; ++i)
    {
        const auto& segment = transport.getSegment (i);
        const auto perSample = segment.perSample;   // each stretch has its own (the tempo may ramp)

        // A jump (the loop going round, a locate while playing) releases what was playing; the
        // clips under it are entered again (their next pass).
        if (wasPlaying && std::abs (segment.from - expected) > 1.0e-6)
        {
            releaseAll (out, segment.offset);
            chaseDue = true;

            if (sequence != nullptr)
                sequence->jumped();
        }

        // Starting: the clips' passes count from where they stand.
        if (! wasPlaying)
        {
            chaseDue = true;

            if (sequence != nullptr)
                sequence->startRun();
        }

        wasPlaying = true;
        expected = segment.to;

        if (sequence == nullptr || perSample <= 0.0)
            continue;

        if (std::exchange (chaseDue, false))
            chase (*sequence, segment.from, out, segment.offset);

        const auto& events = sequence->getEvents();
        auto event = std::lower_bound (events.begin(), events.end(), segment.from,
                                       [] (const Sequence::Event& e, double ppq) { return e.ppq < ppq; });

        for (; event != events.end() && event->ppq < segment.to; ++event)
        {
            const auto within = static_cast<int> ((event->ppq - segment.from) / perSample);
            const auto at = segment.offset + juce::jlimit (0, segment.length - 1, within);

            if (event->status == pitchBendStatus)
            {
                sendPitchBend (valueOf (*event), out, at);
                continue;
            }

            if (event->status == aftertouchStatus)
            {
                sendAftertouch (valueOf (*event), out, at);
                continue;
            }

            if (event->status == controllerStatus)
            {
                const juce::uint8 message[] = { event->status, event->data1, event->data2 };
                out.addEvent (message, 3, at);

                if (event->data1 == sustainPedal)
                    sustainDown = event->data2 >= 64;

                continue;
            }

            const auto pitch = static_cast<size_t> (event->data1 & 0x7f);
            auto velocity = event->data2;

            // A note with operators: whether it plays this pass, and how hard.
            if (event->note >= 0 && ! sequence->decide (*event, velocity))
                continue;

            if (event->status == noteOff)
            {
                if (! sounding.test (pitch))
                    continue;             // started before playback began here: nothing to end

                sounding.reset (pitch);
            }
            else
            {
                sounding.set (pitch);
            }

            const juce::uint8 message[] = { event->status, event->data1, velocity };
            out.addEvent (message, 3, at);
        }
    }
}
