// D:\workspace\Fanan_web_hub\src\MasterSection.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "AnalogBoard.h"
#include "SonifyDsp.h"

#include <array>
#include <atomic>
#include <vector>

//==============================================================================
/**
    The Mastering section: the last thing on Main, after its fader, whichever mixer mixes the song
    (the Console, or the Analog Board after its Master fader). One for the song, saved with it.

      EQ          a low cut, a low shelf, a mid bell and a high shelf
      Multiband   three bands split at two crossovers (Linkwitz-Riley, 24 dB an octave, their sum
                  flat), each with its own compressor (Threshold, Ratio; Attack and Release for
                  all three; Comp In), its own Gain and its own stereo Width (0% mono, 100% as
                  it is, up to 200%: the low band's narrowed keeps the bass in the middle)
      Tape        soft tape saturation (Drive), a little low bump and softer top with it
      Limiter     Gain into a true-peak brickwall limiter (it looks ahead 1.5 ms) that never lets
                  the sound over its Ceiling; Release
    Switched in (Mastering) or out, the sound fades out over 10 ms and back in over 10 ms (its
    limiter's look-ahead delays it a little while it's in). Mode (Auto, Mix + Master, Master only)
    is what Fanan AI's mastering works for; Auto takes Master only when the song is a single
    finished track.

    Its meters measure what leaves Main, in or out: loudness as EBU R 128 has it (momentary
    400 ms, short-term 3 s, integrated while the song plays, from the start of play or the last
    reset), the highest true peak, the limiter's and each band's reduction.

    Every setting is an atomic anyone may set at any time (the page, Fanan AI, MIDI Link); the
    audio thread reads them each block. prepare() allocates; nothing else does.
*/
namespace MasterSection
{
    using ParamSpec = AnalogBoard::ParamSpec;

    enum Param
    {
        on, mode,
        lowCut, eqLow, eqLowFreq, eqMid, eqMidFreq, eqHigh, eqHighFreq,
        xoverLow, xoverHigh, mbOn, mbAttack, mbRelease,
        lowThreshold, lowRatio, lowGain, lowWidth,
        midThreshold, midRatio, midGain, midWidth,
        highThreshold, highRatio, highGain, highWidth,
        tapeOn, tapeDrive,
        limitGain, ceiling, limitRelease,
        numParams
    };

    constexpr int numBands = 3;

    /** A band's settings, by band (0 low, 1 mid, 2 high): its threshold, then the rest in order. */
    constexpr int bandParam (int band, int first) noexcept   { return first + band * (midThreshold - lowThreshold); }

    const std::array<ParamSpec, numParams>& specs();
    int indexOf (const juce::String& id);

    using Settings = AnalogBoard::Settings<numParams>;

    /** What Mode means for a song (Auto resolved): true when it's mastered as one finished track. */
    enum class Mode { mixAndMaster, masterOnly };

    //==============================================================================
    /** The meters, read on the message thread (the audio thread writes them). */
    struct Meters
    {
        std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f };   // LUFS
        std::atomic<float> truePeak { -100.0f };       // dBTP, the highest since the last reset
        std::atomic<float> limiter { 0.0f };           // its reduction, the block's most (dB)
        std::array<std::atomic<float>, numBands> bands {};   // each band's compressor reduction (dB)
        std::atomic<bool> resetRequested { false };    // the integrated loudness and the true peak start again
    };

    //==============================================================================
    class Processor
    {
    public:
        Processor();
        ~Processor();

        Settings settings { specs() };
        Meters meters;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        /** Main's mix, in place, as the settings have it: in, out, or (switched) fading out over
            10 ms and back in over 10 ms. bypass: left out whatever its switch says (a render without
            it). playing: the song plays (the integrated loudness counts only then). settle: as it's
            set, no fade (a mixdown's first block). Then its meters. */
        void process (float* left, float* right, int numSamples, bool bypass, bool playing, bool settle) noexcept;

        /** The samples it delays the sound by while it's in (the limiter's look-ahead); 0 out. */
        int getLatency() const noexcept;

        /** The same, whether it's in or not (what a render with it in is late by). */
        int getChainLatency() const noexcept;

        /** It's in now (its switch on, as the audio thread last took it). */
        bool isActive() const noexcept   { return active; }

    private:
        class Network;

        void refresh() noexcept;
        void runChain (float* left, float* right, int numSamples) noexcept;
        void measure (const float* left, const float* right, int numSamples, bool playing) noexcept;

        double sampleRate = 48000.0;
        int maxBlock = 512;
        juce::uint32 seenChanges = 0xffffffffu;
        bool active = false, fadingOut = false, fadingIn = false;
        int fadeAt = 0;                                 // samples into a fade

        std::unique_ptr<Network> network;

        // The loudness: K-weighted 100 ms steps, the last 30 kept (momentary: 4, short-term: 30),
        // and every 400 ms block since the reset in a histogram (0.1 LU a bin) for the gating.
        static constexpr int histogramBins = 1000;      // -70 .. +30 LUFS
        SonifyDsp::Biquad shelf, highPass;
        std::array<SonifyDsp::BiquadState, 4> kStates {};
        double stepSum = 0.0;
        int stepLength = 4800, stepCount = 0;
        std::array<double, 30> steps {};
        int stepsKept = 0, stepAt = 0;
        std::vector<double> binPower;                   // the blocks' mean squares summed, by bin
        std::vector<juce::int64> binCount;
        std::array<SonifyDsp::TruePeakInterpolator, 2> peakDetectors;
        float peakHeld = 0.0f;
    };

    /** Auto's answer for a song: Master only when one audio track has clips and nothing else does. */
    Mode resolveMode (const Settings& settings, int tracksWithClips, bool onlyAnAudioTrack) noexcept;
}
