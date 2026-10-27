// D:\workspace\Fanan_web_hub\src\MixAnalysis.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <functional>
#include <vector>

//==============================================================================
/**
    What Fanan AI's mixing hears (AI Mix and AI Master): the song rendered to files (each strip
    of the Analog Board as it leaves its fader, Main's mix before the Mastering section, and after
    it), and a reference track, measured as numbers it can reason about.

      every file   loudness (integrated LUFS, gated as EBU R 128; the loudest short-term), the
                   sample peak, its crest (peak over loudness), how much of the song it plays in,
                   its tonal balance in ten octave bands (31.5 Hz .. 16 kHz, dBFS where it plays),
                   and its stereo (the sides' correlation, the side's level against the middle's)
      the mixes    their true peak too (dBTP)
      the stems    where two of them fight in the same octave (masking): the share of the time
                   both are loud in it and within 6 dB of each other, the worst pairs first

    Files are measured on several threads at once; it can be stopped, and it says how far it got.
*/
namespace MixAnalysis
{
    constexpr int numBands = 10;
    const std::array<double, numBands>& bandCentres();   // 31.5, 63, 125 ... 16000 Hz

    struct Input
    {
        juce::String name;          // what the AI calls it ("Vocal", "mix", "reference")
        juce::String role;          // "stem", "mix" (before mastering), "master" (after), "reference"
        int trackId = 0;            // a stem's track
        juce::File file;
    };

    /** Measures every input: { stems: [...], mix, master, reference, masking: [...] } (the roles
        there are). A file that can't be read is { name, error }. cancelled: stops soon, the
        answer { error: "Cancelled" }. progress: 0..1, from the threads (any thread). */
    juce::var analyse (const std::vector<Input>& inputs, const std::atomic<bool>& cancelled,
                       const std::function<void (double)>& progress = {});

    /** A report as the AI reads it: compact JSON, numbers to two decimals at most. */
    juce::String toText (const juce::var& report);

    /** A measured file's headline: { lufs, truePeak (or peak) } as the page shows it. */
    juce::var headline (const juce::var& measured);
}
