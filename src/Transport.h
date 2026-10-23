// D:\workspace\Fanan_web_hub\src\Transport.h

#pragma once

#include <juce_core/juce_core.h>

#include <clap/clap.h>

#include "TempoMap.h"

#include <array>
#include <atomic>
#include <limits>
#include <memory>

//==============================================================================
/**
    Song position, tempo map, loop, record and metronome.

    The message thread sends requests (play, stop, locate, settings). The audio thread
    applies them at the start of each block and works out which stretches of the song
    the block covers (more than one when it loops back). Tracks play their clips from
    that plan, recording stamps live notes with it, and the metronome clicks on it, so
    all three agree to the sample. Everything the two threads share is atomic, except the
    tempo map, which the audio thread borrows only while it plans a block.

    Positions, the loop included, are in quarter notes (PPQ) from the start of the
    song, so a loop can start and end anywhere on the grid. The tempo map (the song
    channel's tempo changes, ramps and time signatures) turns them into time: while the
    song plays, the transport counts its time in samples, so audio clips (which keep
    their speed) land exactly whatever the tempo does. Bars are counted from 0 inside the
    engine; the page shows them from 1.
*/
class Transport final
{
public:
    //==============================================================================
    /** How the song stands. The musical fields are worked out from the map on the message
        thread: call getState() there. */
    struct State
    {
        bool playing = false;
        double ppq = 0.0;
        double bpm = 120.0;           // the tempo where the song is
        int numerator = 4;            // the time signature of the bar it's in
        int denominator = 4;
        bool loopEnabled = false;
        double loopStart = 0.0;       // where the loop starts, in quarter notes
        double loopEnd = 16.0;        // where it ends (and jumps back to loopStart)
        bool metronomeEnabled = false;
        float metronomeVolume = 0.6f;
        bool recording = false;       // Record is on (asked for)
        bool capturing = false;       // and what's played is being recorded now (the preroll, the
                                      // count-in and outside Autopunch's range it isn't)
        bool counting = false;        // the count-in (Precount) is clicking; the song waits
        int countBeatsLeft = 0;       // its beats still to come (with the one sounding)

        double seconds = 0.0;         // where the song is, in time
        int bar = 0;                  // the bar it's in (from 0), where that starts, and its length
        double barStart = 0.0, barLength = 4.0;
        double previousBarStart = 0.0;
    };

    /** A stretch of song that one block plays without jumping. */
    struct Segment
    {
        double from = 0.0, to = 0.0;     // song positions, quarter notes
        int offset = 0, length = 0;      // where in the block, in samples
        double perSample = 0.0;          // quarter notes a sample, on average across it
        juce::int64 sample = 0;          // song time at its start, in samples
        double seconds = 0.0;            // and in seconds
    };

    static constexpr int maxSegments = 32;

    /** The music at a point of the block (MIDI FX follow it): where a segment starts, and each
        bar line inside it; while stopped, one, where the song stands. */
    struct Mark
    {
        double ppq = 0.0;
        int segment = -1;                // the segment it's in; -1 while stopped
        bool barLine = false;            // a bar starts here
        int bar = 0;                     // the bar it's in (from 0) and that bar's length
        double barLength = 4.0;
        int keyRoot = 0;                 // the key there: 0 C .. 11 B, and its scale's notes
        juce::uint16 keyNotes = 0x0ab5;  // (TempoMap::scaleNotes)
        double bpm = 120.0;
    };

    static constexpr int maxMarks = 96;

    /** The chord track's chord: where it starts (or, first in each segment, where the segment
        starts: the one holding there), its root (-1: no chord), its notes above the root
        (TempoMap::chordNotes) and a bass of its own (-1: none). */
    struct ChordMark
    {
        double ppq = 0.0;
        int segment = -1;
        int root = -1;
        juce::uint16 notes = 0;
        int bass = -1;
    };

    static constexpr int maxChordMarks = 48;

    /** How recording starts and ends, as Studio One's transport has it:
          precount    from stopped, the metronome counts this many bars in first (the song waits)
          preroll     from stopped, the song plays this many bars before where it stands, and
                      recording starts where it stood
          autopunch   records only inside the loop's range (its start and end are the punch in
                      and out); the song plays on outside it
          postroll    with Autopunch (the loop not playing round): the song plays this many bars
                      past the punch out, then stops */
    struct RecordOptions
    {
        bool precount = false, preroll = false, postroll = false, autopunch = false;
        int precountBars = 1, prerollBars = 1, postrollBars = 1;
    };

    static constexpr int maxRollBars = 8;

    static constexpr double minTempo = TempoMap::minTempo;
    static constexpr double maxTempo = TempoMap::maxTempo;
    static constexpr double minLoopLength = 1.0 / 16.0;   // a 64th note
    static constexpr double maxPosition = 1.0e6;          // quarter notes; far past any song

    Transport();

    //==============================================================================
    // Any thread (normally the message thread)
    void setPlaying (bool shouldPlay) noexcept;
    void togglePlaying() noexcept;
    void locate (double ppq) noexcept;
    void setLoop (bool enabled, double startPpq, double endPpq) noexcept;
    void setMetronome (bool enabled, float volume) noexcept;

    /** Recording happens while playing; stopping ends it. */
    void setRecording (bool shouldRecord) noexcept;

    /** Record from where the song stands: from stopped, with the count-in and the preroll the
        options ask for, then playing; while playing, it punches in (Autopunch still decides
        where it records). */
    void startRecording() noexcept;

    void setRecordOptions (const RecordOptions& options) noexcept;
    RecordOptions getRecordOptions() const noexcept;

    /** As a song and the page keep them: { precount, precountBars, preroll, prerollBars, postroll,
        postrollBars, autopunch }; what's missing (or nonsense) stays as it is in fallback. */
    static juce::var recordOptionsToVar (const RecordOptions& options);
    static RecordOptions recordOptionsFromVar (const juce::var& value, RecordOptions fallback);

    //==============================================================================
    // Message thread: the song channel's map of tempo and signatures.
    void setTempoMap (const TempoMap& map);
    const TempoMap& getTempoMap() const noexcept               { return songMap; }

    /** The tempo that holds where the song is now (one tempo: the song's), and the time
        signature of the bar it's in, from that signature's change on. */
    void setTempo (double bpm);
    void setTimeSignature (int numerator, int denominator);

    /** Counts up each time the map changes (clips that keep their speed follow it). */
    juce::uint32 getMapGeneration() const noexcept            { return mapGeneration.load(); }

    State getState() const noexcept;

    /** True for the denominators the transport accepts: 1, 2, 4, 8, 16 and 32. */
    static bool isValidDenominator (int denominator) noexcept  { return TempoMap::isValidDenominator (denominator); }

    //==============================================================================
    // Audio thread
    void prepare (double sampleRate) noexcept;

    /** Applies pending requests, plans the block, and describes its start for plugins. */
    void beginBlock (clap_event_transport_t& transportForPlugins, int numSamples) noexcept;

    /** The stretches of song this block plays, in order; none while stopped.
        Valid from beginBlock() until endBlock(). */
    int getNumSegments() const noexcept                       { return numSegments; }
    const Segment& getSegment (int index) const noexcept      { return segments[static_cast<size_t> (index)]; }

    /** Quarter notes a sample where the block starts (each segment has its own). */
    double getPpqPerSample() const noexcept                   { return ppqPerSample; }

    /** The bar and key where each segment starts and at each bar line in it, in order (one,
        where the song stands, while stopped). Valid from beginBlock() until endBlock(). */
    int getNumMarks() const noexcept                          { return numMarks; }
    const Mark& getMark (int index) const noexcept            { return marks[static_cast<size_t> (index)]; }

    /** The chord track's chords: the one holding where each segment starts, then each change in
        it, in order (one, where the song stands, while stopped). Valid from beginBlock() until endBlock(). */
    int getNumChordMarks() const noexcept                     { return numChordMarks; }
    const ChordMark& getChordMark (int index) const noexcept  { return chordMarks[static_cast<size_t> (index)]; }

    /** The chord marks for a stretch of song (segment: theirs), into out (at most max); returns
        how many. Allocates nothing (the audio thread asks it). */
    static int chordMarksFor (const TempoMap& map, double from, double to, int segment, ChordMark* out, int max) noexcept;
    bool isRecordingNow() const noexcept                      { return capturing; }

    /** The loop in song samples while it plays round (start, and end: where it jumps back to
        start), for what reads the song ahead of time (a choir track's voices). False while it
        isn't on or the song is stopped. Valid from beginBlock() until endBlock(). */
    bool getLoopSamples (juce::int64& start, juce::int64& end) const noexcept
    {
        start = loopFromSample;
        end = loopToSample;
        return loopSamplesOn;
    }

    /** Where this block starts in the song (quarter notes, and seconds), playing or not. */
    double getBlockStart() const noexcept                     { return position; }
    double getBlockStartSeconds() const noexcept              { return sampleRate > 0.0 ? static_cast<double> (songSample) / sampleRate : 0.0; }

    /** Moves to where the plan ends and adds the metronome to the output (either may be null). */
    void endBlock (float* left, float* right, int numSamples) noexcept;

    /** The clicks late by this many samples, as late as the song is heard (plugin delay
        compensation). Any thread. */
    void setClickDelay (int samples) noexcept                  { clickDelay.store (juce::jmax (0, samples), std::memory_order_relaxed); }

    /** Audio thread, before endBlock(): this block's clicks not delayed (nothing else is). */
    void setClicksOnTime (bool onTime) noexcept                { clicksOnTime = onTime; }

    /** The loop as it plays this block (quarter notes): false while it's off. Audio thread. */
    bool getLoopNow (double& start, double& end) const noexcept
    {
        start = loopStart;
        end = loopEnd;
        return loopActive();
    }

private:
    //==============================================================================
    struct Click
    {
        int sample = 0;
        bool accent = false;
    };

    void planBlock (const TempoMap& map, int numSamples) noexcept;
    void collectMarks (const TempoMap& map) noexcept;
    void addMark (const TempoMap& map, double ppq, int segment, const TempoMap::Bar& bar, bool barLine) noexcept;
    void collectClicks (const TempoMap& map, const Segment& segment) noexcept;
    void renderClicks (float* left, float* right, int numSamples) noexcept;
    void startClick (bool accent) noexcept;
    void startRecordingFromStopped (const TempoMap& map) noexcept;
    void countIn (const TempoMap& map, int numSamples) noexcept;
    void decideCapturing (const TempoMap& map) noexcept;

    bool loopActive() const noexcept        { return loopEnabled && loopEnd > loopStart; }

    //==============================================================================
    // Requests and settings, written by the message thread
    std::atomic<int> requestedPlaying { -1 };      // -1 none, 0 stop, 1 play
    std::atomic<double> requestedLocate { std::numeric_limits<double>::quiet_NaN() };
    std::atomic<bool> loopSetting { false };
    std::atomic<double> loopStartSetting { 0.0 }, loopEndSetting { 16.0 };
    std::atomic<bool> metronomeSetting { false };
    std::atomic<float> metronomeVolumeSetting { 0.6f };
    std::atomic<bool> recordSetting { false };
    std::atomic<bool> requestedRecordStart { false };
    std::atomic<bool> precountSetting { false }, prerollSetting { false }, postrollSetting { false }, autopunchSetting { false };
    std::atomic<int> precountBarsSetting { 1 }, prerollBarsSetting { 1 }, postrollBarsSetting { 1 };

    // The map: the message thread's own, and the audio thread's copy, swapped under the lock
    // (which the audio thread holds only while it plans a block).
    TempoMap songMap;
    std::unique_ptr<TempoMap> audioMap;
    juce::SpinLock mapLock;
    std::atomic<juce::uint32> mapGeneration { 0 };

    // Published by the audio thread
    std::atomic<bool> playingNow { false };
    std::atomic<double> positionNow { 0.0 };
    std::atomic<bool> capturingNow { false }, countingNow { false };
    std::atomic<int> countBeatsNow { 0 };

    // Audio thread only
    double sampleRate = 0.0;
    bool playing = false;
    double position = 0.0;
    juce::int64 songSample = 0;                    // song time where the block starts, in samples
    juce::uint32 plannedGeneration = 0xffffffff;   // the map songSample was worked out with
    bool loopEnabled = false;
    double loopStart = 0.0, loopEnd = 16.0;
    bool metronomeEnabled = false;
    float metronomeVolume = 0.6f;
    bool recording = false;
    bool capturing = false;                        // recording what plays, this block

    // Recording's start: where it starts recording (the preroll plays up to it; -1: anywhere),
    // and the count-in (samples into it, from below 0 while it waits for its first click, so it
    // ends on a block's edge; its beats; how long each is).
    double recordFrom = -1.0;
    bool counting = false, countStarted = false;
    juce::int64 countSample = 0, countLength = 0;
    double countBeatSamples = 0.0;
    int countBeats = 0, countNumerator = 4;

    std::array<Segment, maxSegments> segments {};
    int numSegments = 0;
    std::array<Mark, maxMarks> marks {};
    int numMarks = 0;
    std::array<ChordMark, maxChordMarks> chordMarks {};
    int numChordMarks = 0;
    double ppqPerSample = 0.0;
    double plannedEnd = 0.0;
    juce::int64 plannedEndSample = 0;
    bool loopSamplesOn = false;
    juce::int64 loopFromSample = 0, loopToSample = 0;

    static constexpr int maxClicksPerBlock = 64;
    std::array<Click, maxClicksPerBlock> clicks {};
    int numClicks = 0;

    // Clicks waiting to sound (late by clickDelay), in order: samples from this block's start.
    std::array<Click, maxClicksPerBlock> pendingClicks {};
    int numPending = 0;
    std::atomic<int> clickDelay { 0 };
    bool clicksOnTime = false;

    int clickSamplesLeft = 0;
    double clickTime = 0.0, clickPhase = 0.0, clickPhaseStep = 0.0;
    float clickGain = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Transport)
};
