// D:\workspace\Fanan_web_hub\src\AudioToMidi.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <functional>
#include <vector>

//==============================================================================
/**
    Audio > Audio to MIDI: the notes an audio clip plays, for a MIDI clip on a new track.

        Notes   any instrument or voice, chords too: Spotify's Basic Pitch model (Apache 2.0,
                built into Sonify) hears which of the 88 piano keys sound in each frame and where
                notes start; its own note-making step (ported from basic-pitch's note_creation.py)
                turns that into notes. Runs through ONNX Runtime, as Split into Stems does.
        Drums   each hit, as a kick (36), snare (38) or closed hi-hat (42) by where its energy
                lies, its strength the velocity. Needs no model.

    Works on a window of a file (what the clip plays), on a thread of its own; transcribe() does
    the work where it's called (tests). Times are seconds from the window's start.
*/
class AudioToMidi final : private juce::Thread
{
public:
    enum class Mode { notes, drums };

    struct Job
    {
        juce::File file;
        double offset = 0.0, length = 0.0;     // seconds of it
        Mode mode = Mode::notes;
    };

    struct Note
    {
        double start = 0.0, length = 0.0;      // seconds
        int pitch = 60;
        int velocity = 100;
    };

    struct Result
    {
        bool ok = false;
        juce::String error;                    // "Cancelled" when cancelled
        std::vector<Note> notes;               // by start
    };

    explicit AudioToMidi (Job job);
    ~AudioToMidi() override;

    void start()                                        { startThread (juce::Thread::Priority::low); }
    void cancel();
    bool isDone() const noexcept                        { return done.load(); }
    double getProgress() const noexcept                 { return progress.load(); }
    const Job& getJob() const noexcept                  { return job; }

    /** Once isDone(). */
    const Result& getResult() const noexcept            { return result; }

    //==============================================================================
    using Report = std::function<bool (double progress)>;

    static Result transcribe (const Job& job, const Report& report);

    /** Why Notes can't run in this build or on this computer ("" when it can). */
    static juce::String notesProblem();

    static constexpr double longest = 15.0 * 60.0;      // seconds heard at most

    //==============================================================================
    // Basic Pitch's own numbers (basic_pitch/constants.py, inference.py, note_creation.py).
    static constexpr double modelRate = 22050.0;
    static constexpr int fftHop = 256;
    static constexpr int framesPerWindow = 172;         // ANNOT_N_FRAMES: 2 s at 86 frames a second
    static constexpr int windowSamples = 22050 * 2 - fftHop;   // AUDIO_N_SAMPLES
    static constexpr int overlapFrames = 30;
    static constexpr int keys = 88;
    static constexpr int lowestKey = 21;                // MIDI_OFFSET: A0

    /** Basic Pitch's note-making from its frame and onset activations (frames x 88, by frame):
        notes as (first frame, end frame, key index, amplitude). */
    struct FrameNote
    {
        int start = 0, end = 0, key = 0;
        float amplitude = 0.0f;
    };

    static std::vector<FrameNote> notesFromActivations (std::vector<float> frames, std::vector<float> onsets, int numFrames,
                                                        float onsetThreshold = 0.5f, float frameThreshold = 0.3f,
                                                        int minNoteFrames = 11, bool inferOnsets = true, bool melodiaTrick = true,
                                                        int energyTolerance = 11);

    /** Where a frame of the unwrapped activations is, in seconds (model_frames_to_time). */
    static double frameTime (int frame);

    /** Hits in a mono signal: (seconds, pitch, velocity). */
    static std::vector<Note> drumHits (const std::vector<float>& mono, double rate);

private:
    void run() override;

    Job job;
    Result result;
    std::atomic<bool> done { false };
    std::atomic<double> progress { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioToMidi)
};
