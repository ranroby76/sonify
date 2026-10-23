// D:\workspace\Fanan_web_hub\src\RecordingAnalysis.h

#pragma once

#include "AudioMedia.h"
#include "TempoMap.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class AudioEngine;

//==============================================================================
/**
    Backing for a recording: listens to an audio track for its tempo, its beat and its chords,
    so Sonify's session players can play along with it (Track > Add Session Players, From Track).

    The tempo comes from how regularly the sound's onsets come round (their autocorrelation,
    leaning toward everyday tempos), then the one steady beat that fits the onsets best over the
    whole recording; the bars start where the chords tend to change and the low end hits. The
    chords come from the notes heard on each beat (spectral peaks gathered into notes by their
    harmonics, tuned to the recording), matched against major, minor, seventh, sus, diminished
    and power chords, a bar at a time, or two to a bar when it clearly changes in the middle.

    A recording made to Sonify's click already follows the song's beat, and keeps its place. Any
    other has a tempo of its own: the song can take it, and the song's audio then moves alike
    (less than half a bar, unless that would take some before the song's start) so the
    recording's first downbeat lands on a bar line.

    Listens on a thread of its own; the audio it reads is shared and never changes.
*/
class RecordingAnalysis final : private juce::Thread
{
public:
    /** A clip of the recording, as the song plays it. */
    struct Clip
    {
        std::shared_ptr<const AudioData> data;
        double startSeconds = 0.0;      // where it plays in the song now
        double offsetSeconds = 0.0;     // where in the file its window starts
        double lengthSeconds = 0.0;
        float gain = 1.0f;
    };

    /** The song's beat as it stands where the recording is: its tempo and time signature there,
        counted from a bar line at or before the recording (the song's tempo map can change
        elsewhere; a recording is heard against the stretch it starts in). */
    struct Grid
    {
        double bpm = 120.0;             // quarter notes a minute
        int numerator = 4, denominator = 4;
        double originSeconds = 0.0;     // where that bar line is, in the song's time
        int originBar = 0;              // which bar it is (from 0)
    };

    /** The grid of the song's tempo map at a song position (quarter notes). */
    static Grid gridAt (const TempoMap& map, double ppq);

    struct Result
    {
        bool ok = false;
        juce::String error;             // "Cancelled" when cancelled

        double bpm = 0.0;               // the recording's tempo, quarter notes a minute
        bool followsSong = false;       // it plays to the song's beat already: nothing needs to move
        double downbeatSeconds = 0.0;   // its first downbeat, in the song's time as it stands
        int firstBar = 1;               // when it follows the song: the song's bar the chords start in
        juce::String chords;            // a bar at a time from the first downbeat, as SessionPlayers reads them
        int keyTonic = -1;              // its key, from all the notes heard: 0 C .. 11 B (-1: not found)
        bool keyMinor = false;
        int bars = 0;
        double steadiness = 1.0;        // 0..1: how well one steady tempo fits it all
    };

    /** tempo: one to go by (quarter notes a minute), or 0 to find it. */
    RecordingAnalysis (std::vector<Clip> clips, Grid song, double tempo = 0.0);
    ~RecordingAnalysis() override;

    void start()                                       { startThread (juce::Thread::Priority::normal); }
    void cancel()                                      { signalThreadShouldExit(); }
    bool isDone() const noexcept                       { return done.load(); }
    double getProgress() const noexcept                { return progress.load(); }

    /** Once isDone(). */
    const Result& getResult() const noexcept           { return result; }

    //==============================================================================
    static constexpr double rate = 22050.0;            // what it listens at
    static constexpr double longest = 12.0 * 60.0;     // seconds listened to at most

    /** Says how far it got (0..1); false stops it. */
    using Report = std::function<bool (double progress)>;

    /** The listening itself: a mono signal at `rate` whose first sample plays at songSeconds. */
    static Result analyse (std::vector<float> mono, double songSeconds, Grid song, double tempo, const Report& report);

    /** The clips as one mono signal at `rate`, from where the first starts (songSeconds), at
        most `longest` long. */
    static std::vector<float> mixDown (const std::vector<Clip>& clips, double& songSeconds);

    //==============================================================================
    // In the song. Message thread only.

    /** An audio track's clips, as the song plays them now. */
    static std::vector<Clip> clipsOf (AudioEngine& engine, int trackId);

    /** The song's audio and tempo as they stand: what was heard still fits while it's the same. */
    static juce::String layoutOf (AudioEngine& engine);

    /** For a recording with a tempo of its own: the song at its tempo, and all the song's audio
        moved alike (in time with itself) so the recording's first downbeat lands on a bar line,
        the nearest one it can (no clip before the song's start). MIDI stays on its bars. */
    struct Alignment
    {
        double bpm = 0.0;               // the song's new tempo
        double shiftSeconds = 0.0;      // how far the audio moves
        int startBar = 1;               // the bar the first downbeat lands on
    };

    static Alignment alignmentFor (const Result& heard, AudioEngine& engine);
    static void align (AudioEngine& engine, const Alignment& alignment);

    /** Listens to an audio track here and now (Sonify AI's way: it waits). */
    static Result listen (AudioEngine& engine, int trackId, double tempo = 0.0);

private:
    void run() override;

    std::vector<Clip> clips;
    Grid song;
    double tempo;
    Result result;
    std::atomic<bool> done { false };
    std::atomic<double> progress { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordingAnalysis)
};
