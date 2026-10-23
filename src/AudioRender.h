// D:\workspace\Fanan_web_hub\src\AudioRender.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "Sequence.h"
#include "TempoMap.h"

#include <atomic>
#include <functional>
#include <utility>
#include <vector>

//==============================================================================
/**
    Audio > Sync Tempo, Pitch and Reverse: a new file made from a window of an audio clip's
    original, which stays as it is (Undo goes back to it, and later changes start from it again).

    The window can be reversed, moved in pitch (semitones and hundredths) and stretched in time.
    The stretch follows a time map: where each second of the new file takes the original from, as
    points joined by straight lines, so a clip following the song's tempo bends with its ramps.
    Time and pitch go through Signalsmith Stretch (MIT), at the original's sample rate; the file
    is a 32-bit WAV.

    The new file's name comes from what it's made of, so the same render is made once and found
    again (an undo and redo, a tempo set back).

    Renders on a thread of its own; render() does the work where it's called (tests).
*/
class AudioRender final : private juce::Thread
{
public:
    struct Job
    {
        juce::File source;                  // the original
        double offset = 0.0;                // seconds into it where the window starts
        double length = 0.0;                // seconds of it
        bool reverse = false;
        double semitones = 0.0;             // pitch, -24 .. +24

        /** (new file's seconds, window's seconds) from (0, 0) to (outputLength, length); empty:
            the window keeps its time. */
        std::vector<std::pair<double, double>> timeMap;
        double outputLength = 0.0;          // seconds of the new file (0: the window's length)

        juce::File folder;                  // where the new file goes
        juce::String name;                  // what it's called before its key ("Vocal")
    };

    struct Result
    {
        bool ok = false;
        juce::String error;                 // "Cancelled" when cancelled
        juce::File file;
        double lengthSeconds = 0.0;
        bool reused = false;                // the same render was there already
    };

    explicit AudioRender (Job job);
    ~AudioRender() override;

    void start()                                        { startThread (juce::Thread::Priority::low); }
    void cancel()                                       { signalThreadShouldExit(); }
    bool isDone() const noexcept                        { return done.load(); }
    double getProgress() const noexcept                 { return progress.load(); }
    const Job& getJob() const noexcept                  { return job; }

    /** Once isDone(). */
    const Result& getResult() const noexcept            { return result; }

    //==============================================================================
    /** Says how far it got (0..1); false stops it. */
    using Report = std::function<bool (double progress)>;

    static constexpr double longest = 30.0 * 60.0;      // seconds of a window it renders at most

    /** The work itself. */
    static Result render (const Job& job, const Report& report);

    /** The file a job makes (its name says what it's made of). */
    static juce::File fileFor (const Job& job);

    //==============================================================================
    /** A window of an audio file. */
    struct Window
    {
        juce::File file;
        double offset = 0.0, length = 0.0;     // seconds
    };

    /** The window of the clip's original it plays: for a render, the part of the original the
        part of the render it shows came from (renderLength: the render's whole length, 0 when
        it's missing: all of the window it was made from); for one following the tempo, its beats
        in the original. For any other clip, its own window. */
    static Window originalWindowOf (const MidiClip& clip, double renderLength);

    /** All of the original's window a render was made from (a clip that follows the tempo is
        rendered whole, so trimming it shows and hides audio as with any clip). */
    static Window wholeWindowOf (const MidiClip& clip)          { return { juce::File (clip.sourceFile), clip.sourceOffset, clip.sourceLength }; }

    /** The job that makes what the clip should play from that window: reversed, moved in pitch,
        and (tempo > 0: the window's own tempo) stretched so its beats land on the song's along
        the tempo map, the window's start leadBeats before the clip's start. */
    static Job jobFor (const MidiClip& clip, const Window& window, bool reverse, double semitones, double tempo,
                       const TempoMap& map, const juce::File& folder, const juce::String& name, double leadBeats = 0.0);

    /** The window's seconds for the new file's seconds, and back, along the job's time map. */
    static double windowSecondsAt (const Job& job, double outputSeconds);
    static double outputSecondsAt (const Job& job, double windowSeconds);

    /** Stretches (and moves in pitch) channels of samples: output[c] gets outputSamples from
        input[c], the input position for each output sample given by inputAt (in samples). */
    static void stretch (std::vector<std::vector<float>> input, std::vector<std::vector<float>>& output,
                         int outputSamples, double sampleRate, double semitones,
                         const std::function<double (double outputSample)>& inputAt, const Report& report);

private:
    void run() override;

    Job job;
    Result result;
    std::atomic<bool> done { false };
    std::atomic<double> progress { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRender)
};
