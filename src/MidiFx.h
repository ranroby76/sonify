// D:\workspace\Fanan_web_hub\src\MidiFx.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "TempoMap.h"
#include "Transport.h"

#include <array>
#include <atomic>
#include <bitset>
#include <memory>
#include <vector>

//==============================================================================
/**
    MIDI FX: the devices in front of an instrument (Studio One's Note FX, Ableton's MIDI
    effects). An instrument track's notes, the clips' and the live ones, go through its chain
    of devices before the instrument hears them.

    Every device works on a clock of quarter notes, so it keeps time with the song: while the
    song plays the clock runs with it (and the grid is the song's), while it's stopped it runs
    at the song's tempo where the song stands. Devices that work in a key follow the song
    channel's key and scale wherever the notes are.

    The same devices are MIDI tools for clips: run offline over a clip's notes (Apply), they
    give the notes they'd play, which become the clip's.

    Nothing here allocates on the audio thread: every list has its room reserved up front.
*/
namespace MidiFx
{
    constexpr int maxParams = 12;

    //==============================================================================
    /** A key: its root (0 C .. 11 B) and its scale's notes above the root, one bit a semitone. */
    struct Key
    {
        int root = 0;
        juce::uint16 notes = 0x0ab5;     // major

        bool contains (int pitch) const noexcept;

        /** The key's note nearest a pitch: direction 0 the nearer (the lower on a tie), 1 up, -1 down. */
        int snap (int pitch, int direction = 0) const noexcept;

        /** steps notes of the key up (down when negative) from a pitch. A pitch outside the key
            moves with the key's note below it, keeping its distance from that note. */
        int shift (int pitch, int steps) const noexcept;

        /** A key whose scale has every note (chromatic) gives chords nothing to go on: the
            major scale from the pitch itself instead. */
        Key forChords (int pitch) const noexcept;
    };

    //==============================================================================
    /** One MIDI message, at a time on the device clock (quarter notes). */
    struct Event
    {
        double time = 0.0;
        juce::uint8 data[3] {};
        juce::uint8 size = 0;

        bool isNoteOn() const noexcept       { return size == 3 && (data[0] & 0xf0) == 0x90 && data[2] > 0; }
        bool isNoteOff() const noexcept      { return size == 3 && ((data[0] & 0xf0) == 0x80 || ((data[0] & 0xf0) == 0x90 && data[2] == 0)); }
        bool isNote() const noexcept         { return isNoteOn() || isNoteOff(); }
        int channel() const noexcept         { return data[0] & 0x0f; }
        int pitch() const noexcept           { return data[1] & 0x7f; }
        int velocity() const noexcept        { return data[2] & 0x7f; }

        static Event noteOn (double time, int channel, int pitch, int velocity) noexcept;
        static Event noteOff (double time, int channel, int pitch) noexcept;
    };

    /** Events, with their room reserved up front: adding never allocates. Past its room an
        event is dropped (a block never comes near it). */
    class EventList
    {
    public:
        explicit EventList (int capacity = 2048);

        void clear() noexcept                          { count = 0; }
        void truncate (int newSize) noexcept           { count = newSize < 0 ? 0 : (newSize < count ? newSize : count); }
        bool add (const Event& event) noexcept;
        int size() const noexcept                      { return count; }
        bool isEmpty() const noexcept                  { return count == 0; }
        const Event& operator[] (int index) const noexcept   { return events[static_cast<size_t> (index)]; }
        Event& operator[] (int index) noexcept               { return events[static_cast<size_t> (index)]; }

        /** By time, keeping the order of events at the same time. */
        void sortByTime() noexcept;

    private:
        std::vector<Event> events;
        int count = 0;
    };

    /** Events waiting for their time (echoes, note ends, steps), kept in time order. */
    class Pending
    {
    public:
        explicit Pending (int capacity = 1024);

        bool add (const Event& event) noexcept;
        bool isEmpty() const noexcept                  { return count == 0; }
        void clear() noexcept                          { count = 0; }
        int size() const noexcept                      { return count; }
        int capacity() const noexcept                  { return static_cast<int> (events.size()); }

        /** The time of the first one (only when not empty). */
        double firstTime() const noexcept              { return events[0].time; }

        /** Takes out the first one. */
        Event takeFirst() noexcept;

        /** Takes out the last note-on of that note at or before a time; false when there's none. */
        bool removeNoteOn (int channel, int pitch, double notAfter) noexcept;

    private:
        std::vector<Event> events;
        int count = 0;
    };

    /** How many times each note (channel and pitch) is sounding. */
    class Voices
    {
    public:
        void on (int channel, int pitch) noexcept;

        /** True when that was its last sounding (or it wasn't sounding at all: an end for a note
            started before this one listened, let through). */
        bool off (int channel, int pitch) noexcept;

        bool isSounding (int channel, int pitch) const noexcept  { return counts[index (channel, pitch)] > 0; }
        bool any() const noexcept                       { return total > 0; }
        void clear() noexcept;

        /** Note-offs at time for every note sounding, into out; then none sound. everyTime: one
            for each time a note was started (what a count of them downstream needs to reach 0),
            else one for each note. */
        void releaseAll (EventList& out, double time, bool everyTime = true) noexcept;

    private:
        static size_t index (int channel, int pitch) noexcept   { return static_cast<size_t> ((channel & 15) * 128 + (pitch & 127)); }

        std::array<juce::uint8, 16 * 128> counts {};
        int total = 0;
    };

    //==============================================================================
    /**
        The time a block covers on the device clock, and the music there. Built for each
        block from the transport (or, offline, from the tempo map); the devices only read it.
    */
    class Time
    {
    public:
        struct Piece
        {
            double clock = 0.0;          // where it starts on the clock
            double perSample = 0.0;      // clock (quarter notes) a sample
            int offset = 0, length = 0;  // samples in the block
            bool playing = false;
            double song = 0.0;           // the song position where it starts (playing)
            int segment = -1;            // the transport's segment
        };

        struct GridLine
        {
            double time = 0.0;           // on the clock
            juce::int64 number = 0;      // counted from the song's start (or from origin while stopped)
        };

        struct BarLine
        {
            double time = 0.0;
            int bar = 0;
            double length = 4.0;         // the bar's length in quarter notes
        };

        double start = 0.0, end = 0.0;   // the block on the clock: [start, end)
        bool offline = false;            // Apply on a clip: devices may move notes earlier than now
        double bpm = 120.0;              // where the block starts (milliseconds count with it)

        /** For a block of the audio device: clock is where the block starts on the clock. */
        void build (const Transport& transport, double clock, int numSamples) noexcept;

        /** For Apply: from..to on the song (the clock is the song position), with the map's music.
            Past the clip's end the song counts as stopped: devices finish what they started. */
        void buildOffline (const TempoMap& map, double from, double to, bool playing) noexcept;

        bool isPlaying() const noexcept;
        bool isPlayingAt (double time) const noexcept   { return pieceAt (time).playing; }

        /** The song position at a time on the clock (playing; NaN while stopped). */
        double songAt (double time) const noexcept;

        int sampleAt (double time) const noexcept;
        double timeAtSample (int sample) const noexcept;

        Key keyAt (double time) const noexcept;
        double barLengthAt (double time) const noexcept;

        /** The chord track's chord at a time (root -1: none there). */
        struct Chord
        {
            int root = -1;
            juce::uint16 notes = 0;      // above the root, one bit a semitone
            int bass = -1;               // a bass note of its own, or -1
        };

        Chord chordAt (double time) const noexcept;

        /** Milliseconds as quarter notes at the block's tempo. */
        double fromMilliseconds (double ms) const noexcept       { return ms / 60000.0 * bpm; }

        /** The grid lines of a step (quarter notes) in the block, in order, into out (at most
            max); returns how many. While the song plays they're the song's own grid; while it's
            stopped, steps from origin (none when origin is NaN). */
        int gridLines (double step, double origin, GridLine* out, int max) const noexcept;

        /** The bar lines in the block while the song plays (none while stopped). */
        int barLines (BarLine* out, int max) const noexcept;

        const Piece& pieceAt (double time) const noexcept;

        // The block as it was built: its pieces and the music's marks (devices that run
        // elsewhere, Sonify devices in their sandbox, get them as they are).
        int getNumPieces() const noexcept                         { return numPieces; }
        const Piece& getPiece (int index) const noexcept          { return pieces[static_cast<size_t> (index)]; }
        int getNumMarks() const noexcept                          { return numMarks; }
        const Transport::Mark& getMark (int index) const noexcept { return marks[static_cast<size_t> (index)]; }

        static constexpr int maxPieces = Transport::maxSegments + 2;

    private:
        const Transport::Mark& markAt (double time) const noexcept;

        std::array<Piece, maxPieces> pieces {};
        int numPieces = 0;
        std::array<Transport::Mark, Transport::maxMarks> marks {};
        int numMarks = 0;
        std::array<Transport::ChordMark, Transport::maxChordMarks> chordMarks {};
        int numChordMarks = 0;
    };

    //==============================================================================
    enum class Unit { number, percent, semitones, steps, pitch, rate, choice, toggle, ms, bars, decimal,
                      track };   // a track's id (0: none): the page lists the instrument tracks

    struct ParamSpec
    {
        juce::String id, name;
        float min = 0.0f, max = 1.0f, defaultValue = 0.0f;
        Unit unit = Unit::number;
        juce::StringArray choices;       // choice and rate
    };

    class Device;

    struct TypeSpec
    {
        juce::String id, name, description;
        std::vector<ParamSpec> params;
        bool magenta = false;            // its notes come from a model on the page (Magenta)
        std::unique_ptr<Device> (*make) (const TypeSpec&) = nullptr;
    };

    /** Every kind of device, in the order the page lists them. */
    const std::vector<TypeSpec>& types();
    const TypeSpec* findType (const juce::String& id);

    /** Note values (rate params): their names and lengths in quarter notes. */
    const juce::StringArray& rateNames();
    double rateLength (float choice) noexcept;

    //==============================================================================
    /** One device. Its parameters are atomics the page may change any time. */
    class Device
    {
    public:
        explicit Device (const TypeSpec& spec, int pendingCapacity = 1024);
        virtual ~Device() = default;

        const TypeSpec& spec;

        float get (int index) const noexcept;
        int getInt (int index) const noexcept          { return juce::roundToInt (get (index)); }

        /** Clamped to the parameter's range (whole numbers for whole-number units). Any thread. */
        float set (int index, float value) noexcept;

        /** Audio thread (or Apply): the block's events in, what it plays out (in any order:
            the chain sorts them). */
        virtual void process (const EventList& in, EventList& out, const Time& time) noexcept = 0;

        /** Stops all it plays (note-offs at time into out) and forgets what it was doing. */
        void flush (EventList& out, double time) noexcept;

        /** True while it still has something to play (Apply runs on until it hasn't). */
        virtual bool isBusy() const noexcept           { return ! pending.isEmpty() || ! overflow.isEmpty() || sounding.any(); }

        /** What it adds to its description (AudioEngine::getMidiFx) besides its kind and params:
            a device made elsewhere (a Sonify device) says what it is. Message thread. */
        virtual void describeInto (juce::DynamicObject&) const {}

        /** The track whose notes it follows (Harmonize), which the engine plays first; 0: none. Any thread. */
        virtual int followsTrack() const noexcept     { return 0; }

    protected:
        /** What it does besides letting its notes go when flushed. */
        virtual void reset() noexcept {}

        /** Plays an event (counting its notes). */
        void emit (EventList& out, const Event& event) noexcept;

        /** Plays an event later. A note's end is never lost: with no room left for it, it waits
            in a reserve of its own (and should that fill too, it ends the note now). */
        void schedule (const Event& event) noexcept;

        /** The scheduled events due before a time, played. */
        void emitDue (EventList& out, double before) noexcept;

        juce::Random random;
        Voices sounding;
        Pending pending;

    private:
        Pending overflow { 512 };                        // note ends there was no room for
        EventList endsNow { 64 };                        // ends with no room anywhere: played at once
        std::array<std::atomic<float>, maxParams> values;
    };

    //==============================================================================
    /**
        What each instrument track plays into its instrument, block by block (after its MIDI FX),
        for the devices that follow another track's notes (Harmonize). The engine plays a track a
        device follows before the device's own track, so the notes are this block's, sample by
        sample; a track not played yet this block gives what it was holding at the last one.
        Audio thread (one at a time: the device's, or a mixdown's).

        Apply, offline, has no music playing: the followed tracks' clips stand in (setOffline).
    */
    class Board
    {
    public:
        using Notes = std::bitset<128>;

        /** A new block of the audio device (or of a mixdown). */
        void beginBlock() noexcept                    { ++block; }

        /** A track's notes this block (sample positions in the block). */
        void publish (int trackId, const juce::MidiBuffer& midi) noexcept;

        /** The notes a track is holding at a sample of this block (what started at that sample too). */
        Notes heldAt (int trackId, int sample) const noexcept;

        // Apply (message thread): the followed tracks' notes, song positions.
        struct OfflineNote
        {
            int pitch = 60;
            double start = 0.0, end = 0.0;
        };

        void setOffline (int trackId, std::vector<OfflineNote> notes);
        void clearOffline();
        Notes offlineHeldAt (int trackId, double songPosition) const;

        static constexpr int maxTracks = 256, maxChanges = 256;

    private:
        struct Change
        {
            int sample = 0;
            juce::uint8 pitch = 0;
            bool on = false;
            bool all = false;            // every note let go
        };

        struct Entry
        {
            int trackId = 0;
            juce::uint64 block = 0;
            Notes before, after;          // at the block's start and end
            int count = 0;
            std::array<Change, maxChanges> changes {};
        };

        juce::uint64 block = 1;
        std::array<Entry, maxTracks> entries {};
        std::vector<std::pair<int, std::vector<OfflineNote>>> offline;
    };

    /** The one board (the engine's tracks write to it, Harmonize reads it). */
    Board& board() noexcept;

    /** A device of a kind, set as described (as AudioEngine::getMidiFx describes each device:
        { params: { <param id>: value }, tool, toolName, generator }); unknown kind: null. */
    std::unique_ptr<Device> create (const juce::String& typeId, const juce::var& settings = {});

    /** Makes the devices of kinds that aren't built in (Sonify devices: "device"), for create().
        Set once at startup; none: only the built-in kinds. Message thread. */
    using ExternalMaker = std::unique_ptr<Device> (*) (const juce::String& typeId, const juce::var& settings);
    void setExternalMaker (ExternalMaker maker) noexcept;

    //==============================================================================
    /**
        The Magenta device: what you play (or nothing, for a tool that makes its own) is
        heard in windows of whole bars; each window that ends goes to the page, whose model
        answers with notes for a window to come, which it then plays. The page's side is
        hub-miditools.js.

        An answer plays from the start of the window after it comes in (a model takes a moment,
        so that's the window after next for the window heard):
        Answer: listens one window, waits for the answer, plays it, listens again.
        Follow: listens to every window and plays each answer as it comes, a window behind.
        Loop:   plays its last answer every window until a new one comes.
    */
    class MagentaDevice final : public Device
    {
    public:
        enum Param { bars, mode, temperature, inKey, thru };
        enum Mode { answer, follow, loop };

        static constexpr int maxWindowNotes = 256;

        struct WindowNote
        {
            float start = 0.0f, length = 0.0f;   // quarter notes from the window's start
            juce::uint8 pitch = 60, velocity = 100;
        };

        struct Window
        {
            int number = 0;              // a request: the window heard; an answer: the window to play in
            double length = 8.0;         // quarter notes
            double bpm = 120.0;
            int keyRoot = 0;
            juce::uint16 keyNotes = 0x0ab5;
            int count = 0;
            std::array<WindowNote, maxWindowNotes> notes {};
        };

        explicit MagentaDevice (const TypeSpec& spec);

        // Message thread.
        juce::String tool;               // the MIDI tool's id (hub-miditools.js knows it)
        juce::String toolName;
        std::atomic<bool> generator { false };   // makes notes with nothing played

        /** The windows heard since the last call. */
        bool takeRequest (Window& into) noexcept;

        /** An answer from the page: false when there's no room for it now. */
        bool giveAnswer (const Window& reply) noexcept;

        void process (const EventList& in, EventList& out, const Time& time) noexcept override;

    private:
        void reset() noexcept override;
        void startWindow (double time, double length) noexcept;
        void endWindow (double time, const Time& blockTime) noexcept;
        void playAnswer (EventList& out, double from, double to) noexcept;
        void takeAnswers() noexcept;

        // Audio thread. Answer mode goes round: hear a window, wait for the answer, play it.
        enum class Phase { hearing, waiting, answering };
        Phase phase = Phase::hearing;
        int waitedWindows = 0;
        bool running = false, playingMode = false, listening = true, expectingAnswer = false;
        double windowStart = 0.0, windowLength = 8.0;
        int windowNumber = 0, emptyWindows = 0, firstWindowOfRun = 0;
        Window heard;                                     // the window being heard
        std::array<float, 16 * 128> heldSince {};         // notes being held: when they started (-1 none)
        std::array<int, 16 * 128> lastVelocity {};
        Window playing;                                   // the answer playing now
        bool havePlaying = false;
        int nextNote = 0;
        Window waiting;                                   // an answer come in: it plays from the next window
        bool haveWaiting = false;

        juce::AbstractFifo requestFifo { 4 }, answerFifo { 4 };
        std::array<Window, 4> requests, answers;

        JUCE_DECLARE_NON_COPYABLE (MagentaDevice)
    };

    //==============================================================================
    /** A note for Apply: song positions in quarter notes. */
    struct Note
    {
        int pitch = 60;
        double start = 0.0, length = 0.25;
        int velocity = 100;              // 1..127
        int channel = 0;
    };

    /** Runs notes through devices the way the song would play them from `from` to `to` (a
        clip's span, on the song), then on until the devices are done (a few bars at most):
        the notes they play, sorted. Message thread; the devices are used up. */
    std::vector<Note> runOffline (std::vector<std::unique_ptr<Device>>& devices, std::vector<Note> notes,
                                  const TempoMap& map, double from, double to);
}

//==============================================================================
/**
    An instrument track's MIDI FX: up to maxDevices devices in order, each of which can be
    switched off.

    The engine changes the chain only while it holds its graph lock, which the audio thread
    holds while it plays, so a change is never seen half done; room for every slot is
    reserved up front. Whenever the chain's devices change, everything they play stops.
*/
class MidiFxChain final
{
public:
    static constexpr int maxDevices = 8;

    struct Slot
    {
        Slot (int slotId, std::unique_ptr<MidiFx::Device> d) : id (slotId), device (std::move (d)) {}

        const int id;                                // unique in the engine, so the page can name it
        std::unique_ptr<MidiFx::Device> device;
        std::atomic<bool> on { true };
        bool wasOn = true;                           // audio thread

        JUCE_DECLARE_NON_COPYABLE (Slot)
    };

    MidiFxChain();

    //==============================================================================
    // Message thread; the engine holds its graph lock around the changes.
    int size() const noexcept                       { return static_cast<int> (slots.size()); }
    bool isFull() const noexcept                    { return size() >= maxDevices; }
    Slot* at (int index) const noexcept;
    Slot* find (int slotId) const noexcept;
    int indexOf (int slotId) const noexcept;

    bool add (std::unique_ptr<Slot> slot, int position);
    std::unique_ptr<Slot> remove (int slotId);
    bool move (int slotId, int position);

    /** Puts another device in a slot (a new build of the same device, say), keeping the slot,
        its id and whether it's on; returns the one it held (null when there's no such slot).
        What the old one played stops. */
    std::unique_ptr<MidiFx::Device> replace (int slotId, std::unique_ptr<MidiFx::Device> device);

    /** The slots in this order: those named by id stay (themselves, settings and all), fresh
        ones come in (moved out of fresh), and the rest go into gone (to destroy after letting go
        of the lock). Allocates nothing when fresh and gone have room for maxDevices. */
    void rebuild (const std::vector<int>& order, std::vector<std::unique_ptr<Slot>>& fresh,
                  std::vector<std::unique_ptr<Slot>>& gone);

    //==============================================================================
    /** Everything the devices play stops at the next block (any thread). */
    void requestRelease() noexcept                  { releaseRequested.store (true); }

    /** Whether a device that's on follows that track's notes (Harmonize), or any track. Audio
        thread, under the engine's graph lock (as process). */
    bool follows (int trackId) const noexcept;
    bool followsAny() const noexcept;

    /** The block's notes, in place: through the devices in order. With no devices and
        nothing sounding it does nothing. */
    void process (juce::MidiBuffer& midi, const Transport& transport, int numSamples) noexcept;

private:
    std::vector<std::unique_ptr<Slot>> slots;
    std::atomic<bool> releaseRequested { false };

    // Audio thread.
    MidiFx::EventList first, second, flushed;
    MidiFx::Time time;
    MidiFx::Voices sounding;                         // what the instrument has been sent
    std::array<bool, 16 * 128> heldIn {};            // notes coming in that have started and not ended
    juce::MidiBuffer others;                         // longer messages (SysEx) go round the devices
    double clock = 0.0;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiFxChain)
};
