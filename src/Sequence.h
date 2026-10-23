// D:\workspace\Fanan_web_hub\src\Sequence.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Transport.h"

#include <atomic>
#include <bitset>
#include <memory>
#include <utility>
#include <vector>

//==============================================================================
/**
    A note's operators (Bitwig's): whether it plays each time the music comes round, and how.

        Chance       the chance it plays, rolled again on every pass
        Vel Spread   its velocity moves up to this much up or down, rolled again on every pass
        Repeats      ratchets: played several times through its length, evenly (a count) or every
                     note value (a rate), crowded to its start or end (the curve), its velocity
                     going towards silence or the top by the last one
        Occurrence   only on the first pass (or never on it), or only when the note before it in
                     the clip (or on its key) played (or didn't)
        Recurrence   a cycle of 2 to 8 passes, and the passes of it the note plays on

    A pass is a time the music plays the clip: again each time the loop goes round over it, and
    copy by copy along copies of the clip placed back to back (one pattern repeated). The rolls
    come from the clip's seed (the Editor's Seed), so a pass sounds the same every time it plays,
    in a mixdown too. Chance and Vel Spread are the only random ones.
*/
struct NoteOperators
{
    enum Occurrence { always, first, notFirst, withPrevious, withoutPrevious, withPreviousKey, withoutPreviousKey };

    int chance = 100;                  // % it plays
    int velocitySpread = 0;            // % of the velocity range it may move, up or down
    int repeatCount = 0;               // 2..64: played this many times through its length (0: once)
    double repeatRate = 0.0;           // or again every this many quarter notes (0: not)
    int repeatCurve = 0;               // -100..100: the repeats crowd towards its start (below 0) or its end
    int repeatVelocity = 0;            // -100..100: the last repeat's velocity, from the note's towards silence or the top
    int occurrence = always;
    int recurrenceLength = 0;          // 2..8 passes a cycle (0: every pass)
    int recurrenceMask = 0xff;         // the passes of the cycle it plays on, bit 0 the first

    static constexpr int maxRepeats = 64;

    bool repeats() const noexcept      { return repeatCount >= 2 || repeatRate > 0.0; }
    bool isDefault() const noexcept;
    bool operator== (const NoteOperators& other) const noexcept;
    bool operator!= (const NoteOperators& other) const noexcept   { return ! operator== (other); }

    /** For the page and the song file: only what isn't the default ({} when nothing is):
        c chance, vs velocity spread, rn repeat count, rr repeat rate, rc repeat curve, rv repeat
        velocity, oc occurrence ("first", "notFirst", "prev", "noPrev", "prevKey", "noPrevKey"),
        rl recurrence length, rm recurrence mask. */
    juce::var toVar() const;

    /** Read back, every value kept in range. */
    static NoteOperators fromVar (const juce::var& value);
};

//==============================================================================
/** One note in a clip. Times are quarter notes on the clip's own content timeline. */
struct MidiNote
{
    int pitch = 60;
    double start = 0.0;
    double length = 0.25;
    float velocity = 0.8f;             // 0..1
    bool muted = false;                // the Editor's Mute tool: kept in the clip, but silent
    NoteOperators ops {};

    /** As the page and the song file have it: [pitch, start, length, velocity 1-127], then 1 for a
        muted note (0 when operators follow), then its operators when they aren't the default. */
    juce::var toVar() const;

    /** Read back; false when the value isn't a note. */
    static bool fromVar (const juce::var& value, MidiNote& note);

    /** Everything about it the same (a copy of it). */
    bool sameAs (const MidiNote& other) const noexcept;
};

/** A point on one of a clip's controller lines (pitch bend, aftertouch): from its time on the
    controller has its value, until the next point, or the clip's end, where it goes back to rest. */
struct MidiControlPoint
{
    double time = 0.0;                 // quarter notes on the clip's content timeline
    int value = 0;                     // pitch bend -8192..8191 (0 at rest), aftertouch 0..127
};

/**
    A MIDI clip on a track (Studio One calls it a part): a window onto its notes.

    The clip shows its content from offset to offset + length, placed at start in the
    song. Trimming a clip moves that window without touching the notes, so notes
    outside it are hidden, not lost, and come back when the clip is dragged longer.
    A note plays when its start is inside the window; it's cut at the clip's end.
    Its pitch bend and aftertouch (channel pressure) are lines of points in the same
    window: the value in effect where the window opens plays from its left edge, and
    both go back to rest at its right edge.
*/
/** An audio clip's own effect (Studio One's Event FX): a plugin that plays only that clip's sound,
    and its tail, before the track's inserts, leaving the other clips on the track as they are.
    The plugin itself is the engine's (Track::ClipEffectSlot); this is what the song, undo and a
    copy keep of it. Shared, and never changed once made: a newer state is a new one with the
    same uid (the effect it is, through its states, copies and undo). */
struct ClipEffect
{
    juce::String file, pluginId, name, format;   // the plugin, as Plugin::load takes it
    juce::MemoryBlock state;                      // its own state, as last taken (empty: as it starts)
    int uid = 0;

    static int newUid() noexcept
    {
        static std::atomic<int> next { 1 };
        return next.fetch_add (1);
    }
};

/** A point on one of a clip's own automation lines. */
struct ClipEnvelopePoint
{
    double time = 0.0;                 // content time: an audio clip's seconds into its file, a MIDI clip's quarter notes
    double value = 0.0;

    bool operator== (const ClipEnvelopePoint& other) const noexcept
    {
        return juce::exactlyEqual (time, other.time) && juce::exactlyEqual (value, other.value);
    }
};

/**
    One of a clip's own automation lines (the clip's Automation, from its right-click menu): points
    joined by straight lines, holding the first value before the first point and the last after
    the last. It belongs to the clip: it moves, copies, splits and trims with it, since its times
    are the clip's content times.

    key:  "gain"      an audio clip's volume, dB (-60 silence .. +12), on top of its own volume
          "pan"       an audio clip's balance, -1 (left) .. +1 (right)
          "fx:<id>"   a parameter of an audio clip's own effect (its stable id), in the parameter's range
          "cc:<n>"    a MIDI clip's controller n (0..127), as the instrument hears it
*/
struct ClipEnvelope
{
    juce::String key;
    std::vector<ClipEnvelopePoint> points;   // by time

    bool operator== (const ClipEnvelope& other) const noexcept     { return key == other.key && points == other.points; }
    bool operator!= (const ClipEnvelope& other) const noexcept     { return ! operator== (other); }

    /** The line's value at a time (0 with no points). */
    static double valueAt (const std::vector<ClipEnvelopePoint>& points, double time) noexcept;

    /** A key a clip of this kind can have (audio: gain, pan, fx:<id>; MIDI: cc:<n>). */
    static bool isKey (const juce::String& key, bool audio);

    /** The range a key's values are kept in (fx: none here, the plugin's own). */
    static std::pair<double, double> rangeOf (const juce::String& key);

    /** Sorted by time, impossible points dropped, values in range; lines without points, with
        keys of the wrong kind, and second lines of a key go. */
    static void tidy (std::vector<ClipEnvelope>& envelopes, bool audio);
};

struct MidiClip
{
    int id = 0;
    double start = 0.0;                // song position of the clip's left edge, quarter notes
    double length = 4.0;
    double offset = 0.0;               // content position shown at the left edge
    bool muted = false;                // the arrangement's Mute tool: kept in the song, but silent
    juce::uint32 seed = 0;             // what its notes' operators roll from (Chance, Vel Spread): the Editor's Seed
    std::vector<MidiNote> notes;       // content positions, sorted by start
    std::vector<MidiControlPoint> pitchBend;    // content positions, sorted by time
    std::vector<MidiControlPoint> aftertouch;   // channel pressure, content positions, sorted by time

    // An audio clip plays a window of a file instead of notes. Audio keeps its speed
    // whatever the tempo, so its window is in seconds; length above is the same window in
    // quarter notes at the current tempo, kept up to date by the engine.
    juce::String audioFile;            // set: an audio clip
    double audioOffset = 0.0;          // seconds into the file where the window starts
    double audioLength = 0.0;          // seconds of the file the window shows
    double audioGainDb = 0.0;          // the clip's own volume, as on Studio One's events
    double fadeIn = 0.0, fadeOut = 0.0;   // seconds

    // An audio clip made from an original (Audio > Sync Tempo, Pitch, Reverse): audioFile is the
    // render, made from this window of the original, which stays as it was. A further change
    // starts from the original again; Undo goes back to it.
    juce::String sourceFile;           // empty: the clip plays its file as it is
    double sourceOffset = 0.0;         // seconds into the original
    double sourceLength = 0.0;
    double sourceTempo = 0.0;          // the original's tempo, stretched to the song's (0: not stretched)
    double pitch = 0.0;                // semitones it's moved by
    bool reversed = false;
    bool followTempo = false;          // rendered again whenever the song's tempo changes under it

    std::shared_ptr<const ClipEffect> effect;   // an audio clip's own effect (none: it plays as it is)

    // The Audio Editor's Stretcher: the render's own time map, as (render's seconds, seconds of
    // the original's window as it plays: backwards when reversed), from (0, 0) to (the render's
    // length, the window's). Empty: none (a synced render's time comes from the song's tempo).
    std::vector<std::pair<double, double>> warp;

    /** Silence Areas: wherever the clip's sound stays below thresholdDb for at least shortest
        seconds, it's silent (ramping over fade seconds): a gate that takes background noise out.
        Worked out from the file it plays (AudioEngine::silenceRegions); the file stays as it is. */
    struct Silence
    {
        bool on = false;
        double thresholdDb = -50.0, shortest = 0.15, fade = 0.01;

        bool operator== (const Silence& other) const noexcept
        {
            return on == other.on && (! on || (juce::exactlyEqual (thresholdDb, other.thresholdDb)
                                               && juce::exactlyEqual (shortest, other.shortest) && juce::exactlyEqual (fade, other.fade)));
        }

        bool operator!= (const Silence& other) const noexcept     { return ! operator== (other); }
    };

    Silence silence;

    /** The clip's own automation lines (ClipEnvelope): Gain, Pan and its effect's parameters on an
        audio clip; controllers on a MIDI clip. */
    std::vector<ClipEnvelope> envelopes;
    const ClipEnvelope* envelope (const juce::String& key) const noexcept
    {
        for (const auto& line : envelopes)
            if (line.key == key)
                return &line;

        return nullptr;
    }

    int effectUid() const noexcept                                 { return effect != nullptr ? effect->uid : 0; }

    bool isAudio() const noexcept                                  { return audioFile.isNotEmpty(); }
    bool isRender() const noexcept                                 { return sourceFile.isNotEmpty(); }

    /** Follows the song's tempo: its window is counted in the original's beats as well (offset:
        beats of the original before its left edge; length: the beats it shows), and its render
        covers the whole original window, so trimming shows or hides audio as with any clip. */
    bool followsTempo() const noexcept                             { return followTempo && sourceTempo > 0.0 && isRender(); }

    double songPosition (double contentPosition) const noexcept   { return start + contentPosition - offset; }
    double contentPosition (double songPosition) const noexcept   { return songPosition - start + offset; }
    bool shows (const MidiNote& note) const noexcept              { return note.start >= offset && note.start < offset + length; }

    static constexpr int pitchBendMin = -8192, pitchBendMax = 8191, aftertouchMax = 127;

    /** Points sorted by time, one per time (the last given wins), values in range, impossible times dropped. */
    static void tidy (std::vector<MidiControlPoint>& points, int minimum, int maximum);
};

//==============================================================================
/**
    Everything a track's clips play, flattened into one list of events sorted by song
    position: notes, pitch bend and aftertouch. Built on the message thread whenever the
    clips change and handed to the audio thread whole; the audio thread only ever reads it.
*/
class Sequence final
{
public:
    struct Event
    {
        double ppq = 0.0;
        juce::uint8 status = 0, data1 = 0, data2 = 0;
        juce::uint8 order = 0;         // at the same moment: notes end, controllers rest, controllers move, notes start
        int note = -1;                 // a note in a clip with operators: its place among them (-1: none)
        bool first = true;             // its first repeat (or it has none)
    };

    /** quietControlsClip: a clip whose pitch bend and aftertouch aren't played (one being recorded into).
        A note's repeats are its own notes here; whether each plays is decided as it comes (decide).
        previous: the one it's to take the place of, whose notes and chains it finds among its own
        (by their clips' ids and the notes), for carryFrom. */
    static std::unique_ptr<const Sequence> build (const std::vector<MidiClip>& clips, int quietControlsClip = 0,
                                                  const Sequence* previous = nullptr);

    /** As it takes the place of previous (the one build was given), under the engine's graph lock
        (no block plays meanwhile): each note's and chain's pass as previous left them, so an edit
        while the music plays doesn't start the operators' counting (First, a cycle) again. */
    void carryFrom (const Sequence& previous) const noexcept;

    //==========================================================================
    // The operators, as the music plays: audio thread only (the Sequencer's).

    /** Playback starts: every clip is counted from where it stands in its copies again. */
    void startRun() const noexcept;

    /** A jump (the loop going round, a locate): each clip is entered again as it plays next, one
        pass on. */
    void jumped() const noexcept;

    /** Whether a note event (one with operators) plays this time; a note-on's velocity, spread. */
    bool decide (const Event& event, juce::uint8& velocity) const noexcept;

    const std::vector<Event>& getEvents() const noexcept     { return events; }

    /** The pitch bend (-8192..8191) and aftertouch (0..127) in effect just before a song position:
        what playing from there must send first. */
    int pitchBendBefore (double ppq) const noexcept;
    int aftertouchBefore (double ppq) const noexcept;

    /** A controller's value in effect just before a song position (a clip's automation of it);
        -1 when nothing set it. */
    int controllerBefore (int controller, double ppq) const noexcept;

    /** The controllers the clips' automation moves. */
    const std::bitset<128>& getControllers() const noexcept   { return controllersUsed; }

    static constexpr double minimumNoteLength = 1.0 / 128.0;   // quarter notes

private:
    struct OperatorNote
    {
        NoteOperators ops;
        int chain = 0;                 // the clip's chain: its copies back to back
        int clip = 0;                  // the clip (its place in the track's clips)
        int copy = 0;                  // which copy of the chain it is, from 0
        juce::uint64 key = 0;          // the clip's seed and the note: what it rolls from
        juce::uint8 pitch = 0;
    };

    struct ChainState
    {
        int clip = -1;                 // the copy being played (-1: none since the last jump)
        int pass = -1, last = -1;      // the pass playing, and the last one counted
        bool previousPlayed = true;
        std::bitset<128> keyMissed;    // the last note on each key didn't play
    };

    struct NoteState
    {
        int pass = -1;                 // the pass it was decided for
        bool plays = true;
        int spread = 0;                // added to its velocities
    };

    std::vector<Event> events;
    std::vector<Event> bends, pressures;   // the controller events alone, for finding where they stand
    std::vector<Event> controllerEvents;   // the clips' controller automation (CC), by time
    std::bitset<128> controllersUsed;
    std::vector<OperatorNote> operatorNotes;
    mutable std::vector<ChainState> chains;     // the audio thread's
    mutable std::vector<NoteState> noteStates;

    // What carryFrom hands on: each clip's id (by its place), and for each of its notes, chains and
    // (the previous one's) clips, the previous one's that is the same (-1: none).
    std::vector<int> clipIds;
    std::vector<int> noteFrom, chainFrom, clipFrom;
};

//==============================================================================
/**
    Plays a Sequence into a block's MIDI, following the transport's plan for the
    block. It remembers the notes it has started, so stopping, locating or looping
    back never leaves one hanging, and where pitch bend and aftertouch stand: they go
    back to rest when it stops, and playing from anywhere starts with the values in
    effect there. Audio thread only, apart from requestRelease().
*/
class Sequencer final
{
public:
    void render (const Sequence* sequence, const Transport& transport, juce::MidiBuffer& out) noexcept;

    /** Releases every note it started, at the start of the next block (any thread). */
    void requestRelease() noexcept        { releaseRequested.store (true); }

    /** Sends the pitch bend and aftertouch in effect at the playhead again, at the start of the
        next block, when they differ from what it last sent: the lines changed under it (any thread). */
    void requestChase() noexcept          { chaseRequested.store (true); }

private:
    void releaseAll (juce::MidiBuffer& out, int sampleOffset) noexcept;
    void chase (const Sequence& sequence, double ppq, juce::MidiBuffer& out, int sampleOffset) noexcept;
    void sendPitchBend (int value, juce::MidiBuffer& out, int sampleOffset) noexcept;
    void sendAftertouch (int value, juce::MidiBuffer& out, int sampleOffset) noexcept;

    std::bitset<128> sounding;
    std::atomic<bool> releaseRequested { false }, chaseRequested { false };
    bool wasPlaying = false;
    double expected = 0.0;             // where the next block should start if nothing jumps
    int pitchBendNow = 0;              // what it last sent (0: at rest)
    int aftertouchNow = 0;
    bool sustainDown = false;          // a clip's automation put the sustain pedal down
};
