// D:\workspace\Fanan_web_hub\src\MasteringAssistant.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "SonifyDsp.h"

#include <array>
#include <atomic>
#include <functional>

//==============================================================================
/**
    The Mastering Assistant's listening and deciding, on a thread of its own (Song > Mastering
    Assistant). It reads the song as Main played it (rendered to a file, with any earlier
    mastering bypassed), then sets up Sonify Mastering:

      listening   loudness (ITU-R BS.1770: integrated, gated), true peak, and the tonal balance in
                  octave bands, only where the song plays
      EQ          gentle moves (a few dB at most) towards a balance typical of finished music,
                  leaning warm or bright when asked
      glue        light compression when the song's peaks stand far above its loudness, more for
                  the loud target
      drive       into the true-peak limiter (ceiling -1 dBTP), fitted by running the whole
                  mastering over the song until it lands on the loudness asked for

    Its answer: the settings, and what it found, before and after.
*/
class MasteringAssistant final : private juce::Thread
{
public:
    enum class Loudness { dynamic, streaming, loud };
    enum class Character { clean, warm, bright };

    struct Options
    {
        Loudness loudness = Loudness::streaming;
        Character character = Character::clean;
        double inputGain = 1.0;          // undoes Main's fader: the render is after it
    };

    struct Result
    {
        bool ok = false;
        juce::String error;              // "Cancelled" when cancelled
        SonifyDsp::MasteringSettings settings;
        double targetLufs = -14.0;
        double beforeLufs = 0.0, beforePeakDb = 0.0;
        double afterLufs = 0.0, afterPeakDb = 0.0;
        double compressionDb = 0.0;      // the glue's reduction on the loudest passages
        double limitingDb = 0.0;         // the most the limiter takes
        std::array<double, 5> eqDb {};   // low, low mid, mid, presence, air
    };

    MasteringAssistant (const juce::File& rendered, Options);
    ~MasteringAssistant() override;

    void start()                                       { startThread (juce::Thread::Priority::normal); }
    void cancel()                                      { signalThreadShouldExit(); }
    bool isDone() const noexcept                       { return done.load(); }
    double getProgress() const noexcept                { return progress.load(); }

    /** Once it's done. */
    const Result& getResult() const noexcept           { return result; }

    static double targetFor (Loudness) noexcept;
    static juce::String nameOf (Loudness);
    static juce::String nameOf (Character);
    static Loudness loudnessFrom (const juce::String& name);
    static Character characterFrom (const juce::String& name);

    /** The EQ moves (low, low mid, mid, presence, air, in dB) for a song whose octave bands
        (31.5 Hz .. 16 kHz, in dB) read as given. */
    static std::array<double, 5> eqFor (const std::array<double, 10>& bandLevelsDb, Character);

    static constexpr double ceilingDb = -1.0;

private:
    void run() override;
    Result analyse();

    /** Reads the whole render in blocks; progress runs from..to. False when cancelled. */
    bool readAll (juce::AudioFormatReader& reader, const std::function<void (float*, float*, int)>& block, double from, double to);

    juce::File file;
    Options options;
    Result result;
    std::atomic<bool> done { false };
    std::atomic<double> progress { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasteringAssistant)
};
