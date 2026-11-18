// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakEngine.h
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <memory>
#include "ZodiakParams.h"
#include "ZodiakGraph.h"
#include "ZodiakMacros.h"
#include "ZodiakRack.h"
#include "ZodiakPattern.h"
#include "ZodiakLink.h"

//==============================================================================
//  ZodiakEngine.h  —  Zodiak's sound: eight channels alike, each one its
//  GRAPH (ZodiakGraph.h), then the mixer; the master's VST FX and the master.
//
//  A channel, in order:
//
//      notes (transpose + octave)  ->  its graph: the VSTi node(s) play them,
//      through whatever the graph is wired to, into its Output  ->  the
//      delay that lines it up with the latest channel  ->  volume, pan,
//      mute, solo  ->  the mix
//
//  The master: the mix, then the master's VST FX, then its volume and balance.
//
//  PATTERNS (ZodiakPattern.h): the sequencer plays each channel's chosen
//  pattern into it, ahead of the MIDI that comes in.
//
//  MIDI:  MIDI channels 1..8 play Zodiak's channels 1..8.  A source in the
//         MIDI LINK map (ZodiakLink.h) moves its control instead, on the MIDI
//         channel it was linked from, and plays nothing.
//
//  The channels play in an order: one whose Sidechain In reads another plays
//  after it (a ring of them: the one that closes it hears the last block).
//
//  Every setting lives in an atomic the windows write and the audio thread
//  reads.  A macro is its destination's control (ZodiakMacros.h).
//==============================================================================

namespace zodiak
{
    struct MacroSlot
    {
        std::atomic<float> value { 0.5f };      // its own, while it has no destination
        std::atomic<int>   dest  { 0 };
    };

    struct ChannelState
    {
        ChannelState()
        {
            for (int p = 0; p < kNumChanParams; ++p) chan[(size_t) p].store (chanParamDefault (p));
        }

        std::array<std::atomic<float>, kNumChanParams> chan;
        std::array<MacroSlot, kNumMacros>               macros;

        std::atomic<float>    peak  { 0.0f };     // the loudest sample since the window last looked
        std::atomic<float>    peakL { 0.0f }, peakR { 0.0f };
        std::atomic<uint32_t> notes { 0 };        // counts the notes played (the activity lamp)
    };

    //==========================================================================
    class Engine
    {
    public:
        Engine();
        ~Engine();

        void prepare (double sampleRate, int maxBlock);
        void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, const Transport& transport) noexcept;

        //======================================================================
        ChannelState&       channel (int ch) noexcept          { return channels[(size_t) juce::jlimit (0, kNumChannels - 1, ch)]; }
        std::atomic<float>& master (int p) noexcept            { return masterParams[(size_t) juce::jlimit (0, kNumMasterParams - 1, p)]; }
        graph::Graph&       graph (int ch) noexcept            { return graphs[(size_t) juce::jlimit (0, kNumChannels - 1, ch)]; }
        rack::Rack&         masterRack() noexcept              { return masterFx; }
        seq::Sequencer&     patterns() noexcept                { return sequencer; }
        juce::AudioPlayHead* pluginPlayHead() noexcept         { return &playHead; }

        double sampleRate() const noexcept                     { return sr; }
        int    maxBlock() const noexcept                       { return capacity; }

        //======================================================================
        //  MACROS (ZodiakMacros.h).  Where a macro's knob stands (0..1), and
        //  turning it.  Message thread, or the audio thread (MIDI LINK: the
        //  graph's lock is tried for, and a graph being changed misses that move).
        //======================================================================
        float macroNorm (int ch, int slot) noexcept;
        void  setMacroNorm (int ch, int slot, float norm) noexcept;
        juce::String macroText (int ch, int slot);
        /** The nine defaults onto a channel, from its graph as it is. */
        void  resetMacros (int ch);

        /** A channel's setting (0..1, with its skew), and back. */
        float chanNorm (int ch, int p) const noexcept;
        void  setChanNorm (int ch, int p, float n) noexcept;

        //======================================================================
        //  Plugins (message thread): into a graph's node, or the master's VST FX
        //======================================================================
        void setNodePlugin (int ch, int uid, std::unique_ptr<rack::Hosted> plugin);
        int  addMasterPlugin (std::unique_ptr<rack::Hosted> plugin);
        bool replaceMasterPlugin (int uid, std::unique_ptr<rack::Hosted> plugin);

        /** Lines the channels up with the latest one; the latency the DAW is told. */
        int  alignLatency();

        /** Message thread: plays a note on a channel from a window (velocity 0 = off). */
        void audition (int ch, int note, int velocity) noexcept;

        //======================================================================
        //  MIDI LINK (ZodiakLink.h)
        //======================================================================
        link::Map&       links() noexcept                       { return linkMap; }
        int  lastMidi() const noexcept                          { return lastMidiWord.load (std::memory_order_relaxed); }
        void armLearn() noexcept                                { learnHit.store (-1); learnArmed.store (true); }
        void cancelLearn() noexcept                             { learnArmed.store (false); }
        bool isLearnArmed() const noexcept                      { return learnArmed.load(); }
        int  takeLearnHit() noexcept                            { return learnHit.exchange (-1); }

        /** The song's 16th step (0..15) while it plays, else -1: the step LEDs. */
        int   songStep() const noexcept               { return shownStep.load (std::memory_order_relaxed); }

        float takeMasterPeak (int side) noexcept      { return (side == 0 ? masterPeakL : masterPeakR).exchange (0.0f); }

        /** Every sound stops (a set loads, the transport jumps). */
        void requestSilence() noexcept                { silenceRequested.store (true); }

        //======================================================================
        //  TRANSPORT.  Zodiak can't start or stop the DAW, so it has a clock of
        //  its own, and plays from its own PLAY, at its own tempo.
        //      FOLLOW DAW on   it also plays when the DAW plays, from where the
        //                      DAW is, at the DAW's tempo
        //  REWIND takes its own clock back to bar 1.
        //======================================================================
        void play() noexcept                          { ownPlaying.store (true); }
        void stop() noexcept                          { ownPlaying.store (false); }
        void rewind() noexcept                        { rewindRequested.store (true); }

        bool   isPlaying() const noexcept             { return shownPlaying.load (std::memory_order_relaxed); }
        bool   isOwnClockPlaying() const noexcept     { return ownPlaying.load (std::memory_order_relaxed); }
        double position() const noexcept             { return shownPpq.load (std::memory_order_relaxed); }
        double tempo() const noexcept                 { return shownBpm.load (std::memory_order_relaxed); }
        int    beatsPerBar() const noexcept           { return shownBeatsPerBar.load (std::memory_order_relaxed); }
        bool   followsDaw() const noexcept            { return masterParams[mpFollow].load (std::memory_order_relaxed) > 0.5f; }

    private:
        struct Event { int frame; int note; int velocity; };

        class PlayHead final : public juce::AudioPlayHead
        {
        public:
            Transport now;
            juce::Optional<PositionInfo> getPosition() const override;
        };

        void processSegment (float* outL, float* outR, int n, const juce::MidiBuffer& midi, int midiOffset, const Transport& t) noexcept;
        int  sendNote (int ch, int note, int velocity) noexcept;
        int  liftVelocity (int velocity) const noexcept;
        void hostedMidi (int ch, const juce::MidiMessage& m, int frame) noexcept;
        void setMacroLocked (int ch, int dest, float norm) noexcept;
        float nodeNormLocked (int ch, int dest) noexcept;
        void silenceAll() noexcept;

        double sr = 48000.0;
        int    capacity = 512;
        std::array<ChannelState, kNumChannels> channels;
        std::array<std::atomic<float>, kNumMasterParams> masterParams;
        std::array<graph::Graph, kNumChannels> graphs;
        rack::Rack masterFx;
        seq::Sequencer sequencer;
        juce::MidiBuffer playMidi;
        PlayHead playHead;

        // Audio-thread working state
        std::array<std::array<int8_t, 128>, kNumChannels> sentNote {};
        std::array<float, kNumChannels> gainL {}, gainR {};
        std::array<bool, kNumChannels>  held {};
        std::array<int, kNumChannels>   lastLength {};
        std::array<juce::AudioBuffer<float>, kNumChannels> chOut;
        std::array<juce::MidiBuffer, kNumChannels>         hostMidi;
        juce::AudioBuffer<float> mixBuf;
        float masterL = 1.0f, masterR = 1.0f;

        static constexpr int kQueue = 512;
        std::array<std::atomic<int>, kQueue> queue {};
        std::atomic<int> qWrite { 0 }, qRead { 0 };

        // MIDI LINK
        link::Map         linkMap;
        std::atomic<int>  lastMidiWord { 0 };
        int               lastMidiCount = 0;
        std::atomic<bool> learnArmed { false };
        std::atomic<int>  learnHit { -1 };
        bool linkMidi (const juce::MidiMessage& m) noexcept;
        void applyLinkNow (int kind, int ch, int a, int b, int ev) noexcept;

        std::atomic<bool> silenceRequested { false };
        std::atomic<bool> ownPlaying { false }, rewindRequested { false }, shownPlaying { false };
        std::atomic<double> shownPpq { 0.0 }, shownBpm { 120.0 };
        std::atomic<int>  shownBeatsPerBar { 4 };
        double ownPpq = 0.0;
        double dawBpm = 120.0;
        std::atomic<int>  shownStep { -1 };
        std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f };
    };
}
