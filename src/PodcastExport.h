// D:\workspace\Fanan_web_hub\src\PodcastExport.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>

//==============================================================================
/**
    A recording made ready to publish: a podcast (-16 LUFS, as Apple Podcasts and Spotify ask)
    or a video or stream (-14 LUFS, as YouTube plays them).

    Its loudness is measured as Sonify Mastering measures it (ITU-R BS.1770-4 and EBU R 128:
    K-weighted, gated, the whole recording), then the whole of it is turned up or down to the
    loudness asked for, a true-peak limiter keeping it under its ceiling (-1 dBTP). What the
    limiter takes off the loudness is measured too, and given back once (the limiter holding
    the peaks), so it lands within a few tenths of the target. Written as MP3 (LAME, constant
    bit rate), or as 24-bit WAV when LAME isn't there.

    Three passes over the file (measure, try, write), on a thread of its own.
*/
class PodcastExport final : private juce::Thread,
                            private juce::AsyncUpdater
{
public:
    struct Settings
    {
        juce::File source;                  // the recording (any file Sonify reads)
        juce::File target;                  // .mp3 (or .wav: written as WAV)
        double lufs = -16.0;
        float ceilingDb = -1.0f;            // true peak
        int kbps = 192;
    };

    struct Result
    {
        bool ok = false;
        juce::String error;                 // "Cancelled" when cancelled
        juce::File file;                    // what was written (a WAV when LAME wasn't there)
        bool wroteWav = false;
        double measuredLufs = -200.0;       // the recording as it was
        double gainDb = 0.0;                // what it was turned by
        double finalLufs = -200.0;          // as written
        double truePeakDb = -200.0;
    };

    explicit PodcastExport (Settings settings);
    ~PodcastExport() override;

    void start()                                        { startThread (juce::Thread::Priority::low); }
    void cancel()                                       { signalThreadShouldExit(); }
    bool isDone() const noexcept                        { return done.load(); }
    double getProgress() const noexcept                 { return progress.load(); }
    const Settings& getSettings() const noexcept        { return settings; }

    /** Once isDone(). */
    const Result& getResult() const noexcept            { return result; }

    /** Called on the message thread once it's done (it may be deleted from there). */
    std::function<void()> onDone;

    /** The work itself, here and now: report says how far it got (0..1) and false stops it. */
    static Result make (const Settings& settings, const std::function<bool (double)>& report);

private:
    void run() override;
    void handleAsyncUpdate() override;

    Settings settings;
    Result result;
    std::atomic<bool> done { false };
    std::atomic<double> progress { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PodcastExport)
};
