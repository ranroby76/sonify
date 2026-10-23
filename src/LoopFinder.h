// D:\workspace\Fanan_web_hub\src\LoopFinder.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <vector>

//==============================================================================
/**
    Sampler Arena's loop finder (its LoopFinder.h), the same method so the same samples get the
    same loops: a loop is a whole number of the note's pitch periods long, from a rising zero
    crossing in the steady part of the sound to near the end of its strong body, the end nudged
    by up to a quarter period to the best match; scored by how alike the two ends are (over four
    periods), how close their levels, values and slopes are, and how late the end sits.

    The pitch: autocorrelation over 4096 samples in the middle of the sustain (else at a third,
    else at two thirds), 20 Hz to 4 kHz, the top of the first strong peak (Sampler Arena took the
    first lag over 0.9, short of the top: clean tones came out sharp), refined between samples. The sustain: where the level
    (30 ms windows) is over 5% of its loudest, after the first 100 ms. The body's end: the last
    point still at half the loudest.

    Each candidate also carries what Fanan AI weighs when it picks one (SfzLoops: Ask Fanan
    AI): how long the loop is, in seconds and in periods; where it sits; the pitch at each end;
    how much the level wobbles inside it; and how alike the two ends' brightness is.
*/
namespace LoopFinder
{
    struct Candidate
    {
        juce::int64 loopStart = 0, loopEnd = 0;
        float score = 0.0f;                     // 0 .. 1, as Sampler Arena's
        int crossfadeSamples = 0;

        // How it scored, and more about it (for Fanan AI).
        float correlation = 0.0f;               // the two ends over four periods, -1 .. 1
        float levelMatch = 0.0f;                // the quieter end's level over the louder's, 0 .. 1
        float valueMatch = 0.0f, slopeMatch = 0.0f;
        float lateness = 0.0f;                  // 0.5 (the sustain's start) .. 1 (the body's end)
        double seconds = 0.0;                   // the loop's length
        double periods = 0.0;                   // in pitch periods
        double startSeconds = 0.0, endSeconds = 0.0;
        double startPitchHz = 0.0, endPitchHz = 0.0;
        float levelWobbleDb = 0.0f;             // the level's spread inside the loop (30 ms windows)
        float brightnessMatch = 0.0f;           // the two ends' share of high frequencies, 0 .. 1 alike
    };

    struct Result
    {
        bool success = false;
        juce::String error;
        juce::int64 sustainStart = 0, sustainEnd = 0, bodyEnd = 0, totalSamples = 0;
        double sampleRate = 44100.0;
        double pitchHz = 0.0, periodSamples = 0.0;
        float peakLevelDb = -100.0f;
        std::vector<Candidate> candidates;      // best first
    };

    /** A sample's loops: up to maxCandidates, best first. */
    Result analyze (const juce::AudioBuffer<float>& audio, double sampleRate, int maxCandidates = 5, int crossfadeMs = 100);

    /** The same, from a file. */
    Result analyze (const juce::File& file, int maxCandidates = 5, int crossfadeMs = 100);

    /** A candidate's numbers, filled in for a loop set by hand (or read from an SFZ). */
    Candidate describe (const juce::AudioBuffer<float>& audio, double sampleRate, juce::int64 loopStart, juce::int64 loopEnd,
                        double periodSamples);
}
