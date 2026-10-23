// D:\workspace\Fanan_web_hub\src\Mp3Encoder.h

#pragma once

#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

//==============================================================================
/**
    Writes an MP3 file with LAME (mp3lame.dll next to Sonify, which the build puts there; loaded
    the first time it's needed). Constant bit rate, joint stereo, LAME's high-quality setting;
    the LAME tag at the start of the file carries the exact length, so players don't add silence.

    LAME takes any input rate and writes the nearest one MP3 has (at most 48 kHz).
*/
class Mp3Encoder final
{
public:
    Mp3Encoder();
    ~Mp3Encoder();

    /** "" when LAME is there and loads; otherwise why not. */
    static juce::String problem();

    /** Starts the file (replacing one already there). kbps: 32 to 320. */
    juce::Result open (const juce::File& file, double sampleRate, int kbps);

    /** Samples in -1..1, both channels the same length. */
    bool write (const float* left, const float* right, int numSamples);

    /** Ends the file: what LAME still holds, then its tag. */
    juce::Result close();

private:
    struct Lame;
    static Lame& lame();

    void* flags = nullptr;
    std::unique_ptr<juce::FileOutputStream> stream;
    std::vector<unsigned char> buffer;
    juce::File target;
    bool failed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mp3Encoder)
};
