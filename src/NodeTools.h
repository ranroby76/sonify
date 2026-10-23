// D:\workspace\Fanan_web_hub\src\NodeTools.h

#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "SonifyDsp.h"

#include <array>
#include <atomic>

//==============================================================================
/**
    Colosseum's MIDI and audio system tools for the Node Rack (Container.h), as nodes of its
    graph: the Latcher, the Step Seq and the Transient Splitter. Their faces on the canvas and
    their editors are drawn by ContainerCanvas; what they keep is saved with the rack.

    They follow the song: the rack's graph hands them its transport (the song's tempo and time
    signature, or the rack's own when its T says so) through the graph's play head.
*/
namespace NodeTools
{
    //==============================================================================
    /** The Latcher: a 4x4 pad grid of latching notes. A pad pressed (by the mouse, or by its
        trigger note coming in) turns its note on and holds it; pressed again, the note is let
        go. Key releases of trigger notes are swallowed; everything else passes through. MIDI only. */
    class Latcher final : public juce::AudioProcessor
    {
    public:
        static constexpr int numPads = 16, gridRows = 4, gridCols = 4;

        struct Pad
        {
            int triggerNote = 60;     // the note that comes in and toggles it
            int outputNote = 60;      // the note it plays
            int velocity = 100;       // 1 .. 127
            int midiChannel = 1;      // 1 .. 16
        };

        Latcher();

        const juce::String getName() const override                 { return "Latcher"; }
        void prepareToPlay (double, int) override                   {}
        void releaseResources() override                            {}
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
        using juce::AudioProcessor::processBlock;
        juce::AudioProcessorEditor* createEditor() override          { return nullptr; }
        bool hasEditor() const override                              { return false; }
        bool acceptsMidi() const override                            { return true; }
        bool producesMidi() const override                           { return true; }
        double getTailLengthSeconds() const override                 { return 0.0; }
        int getNumPrograms() override                                { return 1; }
        int getCurrentProgram() override                             { return 0; }
        void setCurrentProgram (int) override                        {}
        const juce::String getProgramName (int) override             { return {}; }
        void changeProgramName (int, const juce::String&) override   {}
        void getStateInformation (juce::MemoryBlock& destination) override;
        void setStateInformation (const void* data, int size) override;
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

        /** The pads' settings (message thread; the audio thread reads them as they are). */
        Pad& getPad (int index)                                      { return pads[static_cast<size_t> (juce::jlimit (0, numPads - 1, index))]; }
        const Pad& getPad (int index) const                          { return pads[static_cast<size_t> (juce::jlimit (0, numPads - 1, index))]; }
        static Pad defaultPad (int index);

        bool isPadLatched (int index) const noexcept;
        int countLatched() const noexcept;

        /** A pad pressed with the mouse: it shows at once, its note goes out on the next block. */
        void togglePadFromUI (int index);

        /** Every latched pad let go (All Off, Panic). */
        void allNotesOff();

    private:
        std::array<Pad, numPads> pads;
        std::array<std::atomic<bool>, numPads> latched;
        std::atomic<juce::uint32> pendingToggles { 0 };
        juce::MidiBuffer output;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Latcher)
    };

    //==============================================================================
    /** The Step Seq: Colosseum's 16-lane CC step sequencer. Each lane plays a row of controller
        values (2 .. 128 steps) on its own channel and CC number, at a note rate (straight,
        triplet or dotted), in an order (forward, reverse, ping-pong, random, drunk), with swing
        and an optional glide between steps. It follows the song's tempo and time signature
        (SYN), or a tempo of its own. MIDI only: what comes in passes through, the lanes add theirs. */
    class StepSeq final : public juce::AudioProcessor
    {
    public:
        enum RateDiv   { rate1_1 = 0, rate1_2, rate1_4, rate1_8, rate1_16, rate1_32, rate1_64, rate1_128, numRates };
        enum RateType  { typeNormal = 0, typeTriplet, typeDotted, numRateTypes };
        enum StepOrder { orderForward = 0, orderReverse, orderPingPong, orderRandom, orderDrunk, numOrders };
        enum SpeedMode { speedHalf = 0, speedNormal, speedDouble, numSpeedModes };

        static constexpr int maxSlots = 16, maxSteps = 128;
        static constexpr std::array<int, 7> allowedStepCounts { 2, 4, 8, 16, 32, 64, 128 };

        struct Slot
        {
            bool enabled = false;
            int stepCount = 16;
            int midiChannel = 1;
            int ccNumber = 1;
            RateDiv rate = rate1_8;
            RateType type = typeNormal;
            StepOrder order = orderForward;
            SpeedMode speed = speedNormal;
            bool smooth = false;
            float swing = 0.0f;                      // 0 .. 100 %
            std::array<int, maxSteps> steps {};

            // Playing (audio thread; not saved)
            double phase = 0.0;
            int currentStep = 0, previousStep = 0, lastSent = -1;
            bool pingPongForward = true;

            void resetPlayback() noexcept { phase = 0.0; currentStep = previousStep = 0; lastSent = -1; pingPongForward = true; }
        };

        StepSeq();

        const juce::String getName() const override                 { return "Step Seq"; }
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override                            {}
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
        using juce::AudioProcessor::processBlock;
        juce::AudioProcessorEditor* createEditor() override          { return nullptr; }
        bool hasEditor() const override                              { return false; }
        bool acceptsMidi() const override                            { return true; }
        bool producesMidi() const override                           { return true; }
        double getTailLengthSeconds() const override                 { return 0.0; }
        int getNumPrograms() override                                { return 1; }
        int getCurrentProgram() override                             { return 0; }
        void setCurrentProgram (int) override                        {}
        const juce::String getProgramName (int) override             { return {}; }
        void changeProgramName (int, const juce::String&) override   {}
        void getStateInformation (juce::MemoryBlock& destination) override;
        void setStateInformation (const void* data, int size) override;
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

        // Transport
        void play() noexcept                                         { playing.store (true); }
        void stop() noexcept                                         { playing.store (false); resetOwed.store (true); }
        void togglePlayStop() noexcept                               { if (playing.load()) stop(); else play(); }
        bool isPlaying() const noexcept                              { return playing.load(); }

        void setBpm (double bpm) noexcept                            { ownBpm.store (juce::jlimit (20.0, 400.0, bpm)); }
        double getBpm() const noexcept                               { return ownBpm.load(); }
        double getEffectiveBpm() const noexcept                      { return effectiveBpm.load(); }

        void setSyncToSongTempo (bool s) noexcept                    { syncTempo.store (s); }
        bool isSyncedToSongTempo() const noexcept                    { return syncTempo.load(); }
        void setSyncToSongTimeSig (bool s) noexcept                  { syncTimeSig.store (s); }
        bool isSyncedToSongTimeSig() const noexcept                  { return syncTimeSig.load(); }

        void setTimeSigNumerator (int n) noexcept                    { numerator.store (juce::jlimit (1, 16, n)); }
        int getTimeSigNumerator() const noexcept                     { return numerator.load(); }
        void setTimeSigDenominator (int d) noexcept                  { denominator.store (juce::jlimit (1, 16, d)); }
        int getTimeSigDenominator() const noexcept                   { return denominator.load(); }

        // Lanes (edited on the message thread; the audio thread reads them as they are)
        Slot& getSlot (int index)                                    { return slots[static_cast<size_t> (juce::jlimit (0, maxSlots - 1, index))]; }
        const Slot& getSlot (int index) const                        { return slots[static_cast<size_t> (juce::jlimit (0, maxSlots - 1, index))]; }
        int getSlotCurrentStep (int index) const noexcept;
        int countEnabled() const noexcept;
        bool slotHasData (int index) const;
        void clearSlot (int index);
        void copySlotToClipboard (int index);
        bool pasteSlotFromClipboard (int index);
        bool hasClipboardData() const noexcept                       { return clipboardValid; }

        static juce::String rateName (RateDiv rate);
        static juce::String typeName (RateType type);
        static juce::String orderName (StepOrder order);
        static juce::String speedName (SpeedMode speed);

    private:
        static double rateBeats (RateDiv rate);
        static double typeMultiplier (RateType type);
        static double speedMultiplier (SpeedMode speed);
        int nextStep (Slot& slot);
        static int valueOf (const Slot& slot);
        static void copySettings (const Slot& from, Slot& to);

        std::array<Slot, maxSlots> slots;
        std::array<std::atomic<int>, maxSlots> shownStep;
        Slot clipboard;
        bool clipboardValid = false;

        std::atomic<bool> playing { false }, resetOwed { false };
        std::atomic<double> ownBpm { 120.0 }, effectiveBpm { 120.0 };
        std::atomic<bool> syncTempo { true }, syncTimeSig { true };
        std::atomic<int> numerator { 4 }, denominator { 4 };
        double sampleRate = 44100.0;
        juce::Random random;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSeq)
    };

    //==============================================================================
    /** The Transient Splitter: splits a stereo sound into its attacks (outputs 1-2) and what
        sustains (outputs 3-4), by a fast envelope against a slow one, with no latency. The
        detection can be focused on a band; gains, balance, a hard gate and a swap are on the editor. */
    class TransientSplitter final : public juce::AudioProcessor
    {
    public:
        TransientSplitter();

        const juce::String getName() const override                 { return "Transient Splitter"; }
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override                            {}
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
        using juce::AudioProcessor::processBlock;
        juce::AudioProcessorEditor* createEditor() override          { return nullptr; }
        bool hasEditor() const override                              { return false; }
        bool acceptsMidi() const override                            { return false; }
        bool producesMidi() const override                           { return false; }
        double getTailLengthSeconds() const override                 { return 0.0; }
        int getNumPrograms() override                                { return 1; }
        int getCurrentProgram() override                             { return 0; }
        void setCurrentProgram (int) override                        {}
        const juce::String getProgramName (int) override             { return {}; }
        void changeProgramName (int, const juce::String&) override   {}
        void getStateInformation (juce::MemoryBlock& destination) override;
        void setStateInformation (const void* data, int size) override;
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

        // Detection
        std::atomic<float> sensitivity { 0.5f };        // 0 .. 1
        std::atomic<float> decay { 50.0f };             // ms
        std::atomic<float> holdTime { 10.0f };          // ms
        std::atomic<float> smoothing { 2.0f };          // ms
        std::atomic<float> focusHighPass { 20.0f };     // Hz (20: off)
        std::atomic<float> focusLowPass { 20000.0f };   // Hz (20k: off)

        // Output
        std::atomic<float> transientGainDb { 0.0f };    // -60 .. +12
        std::atomic<float> sustainGainDb { 0.0f };
        std::atomic<float> balance { 0.0f };            // -1 all transient .. +1 all sustain

        // Modes
        std::atomic<bool> stereoLinked { true }, gateMode { false }, invertMode { false };

        // Meters, for the face and the editor
        std::atomic<float> transientRmsL { 0.0f }, transientRmsR { 0.0f }, sustainRmsL { 0.0f }, sustainRmsR { 0.0f };
        std::atomic<float> transientActivity { 0.0f };

    private:
        void updateFilters();
        float coefficientFor (float ms) const noexcept
        {
            return ms <= 0.0f ? 0.0f : std::exp (-1.0f / static_cast<float> (sampleRate * ms * 0.001));
        }

        double sampleRate = 44100.0;
        juce::AudioBuffer<float> dry;
        float fastL = 0.0f, fastR = 0.0f, slowL = 0.0f, slowR = 0.0f;
        float gateL = 0.0f, gateR = 0.0f, smoothL = 0.0f, smoothR = 0.0f;
        int holdL = 0, holdR = 0;
        SonifyDsp::Biquad highPass, lowPass;
        SonifyDsp::BiquadState highPassL, highPassR, lowPassL, lowPassR;
        float lastHighPass = -1.0f, lastLowPass = -1.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientSplitter)
    };
}
