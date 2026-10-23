// D:\workspace\Fanan_web_hub\src\SfzLoops.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "LoopFinder.h"

#include <vector>

//==============================================================================
/**
    The Sampling Studio's Loop Editor (Sampler Arena's WaveformLoopEditor), its work away from
    the page: an SFZ read with each region's sample, keys, velocities and loop; saved again with
    the loops as set; the loops put in the samples' WAVs; and Fanan AI's help picking loops.

    Unlike Sampler Arena's, Save changes nothing in the SFZ but the loops of the regions whose
    loop was changed: every other opcode, header, comment and line stays as it was (Sampler Arena
    wrote the file anew with only six opcodes a region). Regions may use <group>, <global> and
    <master> (their keys, velocities, samples and loops are inherited), <control>'s default_path,
    key=, and note names (c4). Save As into another folder keeps the samples found (their paths,
    or default_path, made relative to the new folder).

    Ask Fanan AI: for each sample, the loop finder's candidates (and its loop now, if it has one)
    with what describes each (LoopFinder::Candidate) go to Fanan AI, which picks the loop that
    should sound smoothest held (or none, for a sound that doesn't sustain) and the crossfade.
    Only when asked; it uses credits.
*/
namespace SfzLoops
{
    struct Region
    {
        juce::String sample;                    // as the SFZ names it
        juce::File file;                        // where it is
        int lokey = 0, hikey = 127, keycenter = 60, lovel = 0, hivel = 127;

        bool looped = false;
        juce::String loopMode;                  // as read (loop_sustain when Sonify writes it)
        juce::int64 loopStart = 0, loopEnd = 0;
        int crossfade = 0;                      // samples
        bool changed = false;                   // Save writes its loop
        bool inheritedLoop = false;             // a <group> (or <global>) loops it

        // Where it is in the text (Patch::text).
        int headerStart = 0, bodyStart = 0, bodyEnd = 0;

        struct Opcode
        {
            juce::String name;
            int start = 0, valueStart = 0, end = 0;
        };

        std::vector<Opcode> own;                // its own opcodes
    };

    struct Patch
    {
        juce::File file;
        juce::String text;                      // as read, its line ends made \n
        bool crlf = false;                      // written back with CR LF
        std::vector<Region> regions;

        struct PathOpcode
        {
            int valueStart = 0, end = 0;
            juce::File points;                  // what it names (default_path: a folder)
            bool isDefaultPath = false;
            bool backslash = false;
        };

        std::vector<PathOpcode> paths;          // sample= and default_path=, wherever they are
    };

    /** Reads an SFZ (false, with why, when it can't be read or has no region). */
    bool load (const juce::File& sfz, Patch& patch, juce::String& why);

    /** The SFZ's text with every changed region's loop as set, its paths right for target's folder. */
    juce::String textFor (const Patch& patch, const juce::File& target);

    /** Writes the SFZ to target (as it was read: line ends, encoding), then each looped sample's
        loop into its WAV's smpl chunk (and a loop removed, out of it); the patch is read again
        from target. */
    juce::Result save (Patch& patch, const juce::File& target);

    /** A sample's waveform for the page: { channels, frames, rate, peaks: [[min, max...] each
        channel] }, each a whole number -127..127, buckets of them. */
    juce::var peaks (const juce::AudioBuffer<float>& audio, double sampleRate, int buckets = 1600);

    /** A key's frequency (equal temperament, A4 = 440 Hz). */
    double keyHz (int note);

    //==============================================================================
    /** What Fanan AI is told about one sample. */
    struct AiSample
    {
        int index = 0;                          // its region
        juce::String name;
        int keycenter = 60, lovel = 0, hivel = 127, channels = 1;
        double seconds = 0.0;
        LoopFinder::Result found;               // its candidates (1..)
        bool hasCurrent = false;
        LoopFinder::Candidate current;          // its loop now (0)
    };

    /** Fanan AI's pick for a sample: candidate 1.. one found, 0 its loop now, -1 no loop. */
    struct AiPick
    {
        int index = -1, candidate = -1, crossfadeMs = 100;
        juce::String why;
    };

    juce::String aiGuide();
    juce::String aiRequest (const std::vector<AiSample>& samples);
    std::vector<AiPick> aiPicks (const juce::var& answer, juce::String& why);

    /** How many samples go in one request (more: several requests, each charged). */
    constexpr int aiSamplesPerRequest = 24;
}
