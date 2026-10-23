// D:\workspace\Fanan_web_hub\src\SoundMeasure.h

#pragma once

#include <string>
#include <vector>

//==============================================================================
/**
    Fanan AI's ears: what a plugin being made sounds like, measured. The Plugins Creator
    Studio renders the plugin offline (SoundCheck), and this turns the audio into a short
    report the AI reads: levels and peaks, the tone (where the energy sits, how bright, how
    noisy), the envelope, how much the sound moves while held, its pitch and purity, the
    stereo image, clicks, and for effects what they did to the test signal.

    Plain C++ (no JUCE): it runs on any thread.
*/
namespace SoundMeasure
{
    /** One render: the plugin's output, and for an effect the test signal it was given. */
    struct Take
    {
        std::vector<float> left, right;       // the plugin's output
        std::vector<float> inLeft, inRight;   // an effect's input (empty for an instrument)
        double sampleRate = 48000.0;
        double start = 0.0;                   // seconds: where the notes (or the signal) start
        double release = 1.0;                 // seconds: where the notes end (or the signal stops)
        std::vector<int> notes;               // an instrument's notes (MIDI numbers)
        bool pitched = true;                  // false: notes are triggers (drums), not pitches
        float velocity = 0.8f;                // 0..1
        std::string signal;                   // an effect's test signal (see signalNames)
    };

    /** The test signals an effect can be given. */
    const std::vector<std::string>& signalNames();

    /** Fills left and right with a test signal (seconds long, then nothing): "music" (drums,
        bass and a chord), "drums", "chord", "noise" (pink), "impulse" or "sine" (1 kHz). False
        if there's no such signal. */
    bool makeSignal (const std::string& name, double sampleRate, double seconds,
                     std::vector<float>& left, std::vector<float>& right);

    /** The report: plain lines with numbers, then the things worth fixing. soft is the same
        notes played softly (an instrument's velocity response), or null. */
    std::string describe (const Take& take, const Take* soft);
}
