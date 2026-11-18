// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakGraph.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "ZodiakNodes.h"
#include "ZodiakRack.h"

//==============================================================================
//  ZodiakGraph.h  —  a channel's GRAPH, Colosseum's way: nodes and the
//  cables between them.  Every channel has one; a new one is the line
//
//      VSTi -> Vowel -> Filter -> Saturator -> Trance Gate -> Dyn EQ
//           -> Sync Delay -> Reverb -> Output
//
//  each effect at rest, and the user rewires it freely: any output into any
//  input (an input sums all that comes in; an output feeds as many as it
//  likes), nodes added (the canvas's right-click) and removed.  The Output
//  is always there and goes into the channel's mixer strip.
//
//  PINS are stereo: one output (not on the Output), one main input (not on a
//  VSTi or a Sidechain In), and on the Dyn EQ a second one, SC: its key.
//
//  LOOPS.  A cable that would close a loop is refused, unless the loop goes
//  through a FEEDBACK node: what comes into a Feedback node goes out one
//  block later, at its level.  (The graph is played in order, every node
//  after the nodes it hears; a Feedback node's input is the one cable the
//  order doesn't follow.)
//
//  LATENCY.  A plugin that reports latency delays its branch; every other
//  branch that meets it is delayed to match before they're summed, so
//  parallel branches stay in time.  The graph's latency is its Output's;
//  the engine delays every channel to the latest one (setTailDelay) and
//  tells the DAW.  (A Feedback node's loop, and a Sidechain In's key, are
//  not compensated.)
//
//  Only the nodes the Output hears are played.
//
//  THREADS.  The message thread changes a graph (its nodes, its cables, a
//  plugin in or out); the new order is worked out off the lock and put in
//  under it, briefly.  The audio thread only TRIES for the lock: a graph
//  being changed plays silence for that block.  A node's settings are
//  atomics, moved from anywhere without the lock.  A node or plugin is
//  destroyed on the message thread, never under the lock.
//==============================================================================

namespace zodiak::graph
{
    static constexpr int kMaxNodes = 40;
    static constexpr int kPinMain = 0, kPinSc = 1;

    /** A cable: src's output into dst's input `pin`. */
    struct Wire
    {
        int src = 0, dst = 0, pin = kPinMain;
        bool operator== (const Wire& o) const noexcept   { return src == o.src && dst == o.dst && pin == o.pin; }
    };

    /** The channels' sounds, for the Sidechain In nodes: this block's when that
        channel has already played it, else the last block's. */
    struct Taps
    {
        std::array<const float*, kNumChannels> L {}, R {};
        std::array<int, kNumChannels> length {};
    };

    //==========================================================================
    class Node
    {
    public:
        Node (int uid, int type, bool byHand);
        ~Node();

        const int uid, type;
        std::atomic<bool> on { true };
        juce::Point<float> pos;                       // on the canvas (message thread)

        std::array<std::atomic<float>, kMaxNodeParams> values;   // a built-in node's settings
        std::unique_ptr<NodeDsp>      dsp;            // a built-in node's sound
        std::unique_ptr<rack::Hosted> plugin;         // VSTi / VST FX: the plugin (nullptr: none loaded)
        juce::File   wantedFile;                      // ...the plugin the set holds, while it loads or when it's missing
        juce::String wantedSonify;                    // ...or the one of Sonify's own it holds
        juce::String wantedName;

        //======================================================================
        //  Its settings: a built-in node's own, or a plugin's parameters (0..1).
        //  get / set: any thread.
        //======================================================================
        int          numParams() const noexcept;
        juce::String paramName (int p) const;
        juce::String paramId (int p) const;           // a built-in's id, or a plugin parameter's index
        int          paramFromId (const juce::String& id) const;
        float        get (int p) const noexcept;
        void         set (int p, float v) noexcept;
        float        getNorm (int p) const noexcept;
        void         setNorm (int p, float n) noexcept;
        juce::String text (int p, float v) const;
        void         range (int p, float& min, float& max, float& skew) const noexcept;
        int          places (int p) const noexcept;   // a stepped setting's places (0: smooth)
        bool         isMask (int p) const noexcept;
        float        restValue (int p) const noexcept;

        juce::String title() const;                   // the plugin's name, else the kind's
        int          numInputs() const noexcept       { return numInputsOf (type); }
        bool         hasOutput() const noexcept       { return hasOutputPin (type); }

        /** The vowel's marker / the gate's step, for the window. */
        float shown() const noexcept                  { return dsp != nullptr ? dsp->shown.load (std::memory_order_relaxed) : 0.0f; }

        /** Its loudest output since the window last looked. */
        float takePeak() noexcept                     { return peak.exchange (0.0f); }

        ZBetel::StyleDucker* ducker() noexcept;        // the Dyn EQ's

    private:
        friend class Graph;
        void prepare (double sampleRate, int maxBlock);

        juce::AudioBuffer<float> in, sc, out, hold;   // hold: a Feedback node's last block
        int holdLength = 0;
        juce::MidiBuffer midi;
        int ownLatency = 0;
        std::atomic<float> peak { 0.0f };
    };

    //==========================================================================
    class Graph
    {
    public:
        Graph();
        ~Graph();

        juce::SpinLock lock;

        //======================================================================
        //  Message thread
        //======================================================================
        void prepare (double sampleRate, int maxBlock, juce::AudioPlayHead* playHead);

        /** The line a new channel has (every node at rest, the VSTi empty). */
        void makeDefault();

        /** Nothing but the Output. */
        void clear();

        /** A node at a place (prepared, in the graph, wired to nothing); uid 0: a new
            one.  byHand: its settings at work ("add"), else at rest ("def").
            nullptr when the graph is full (or that uid is taken). */
        Node* addNode (int type, juce::Point<float> pos, bool byHand, int uid = 0);

        /** A node and its cables out (the Output stays). */
        bool removeNode (int uid);

        enum class Connect { ok, already, loop, invalid };
        Connect connect (int src, int dst, int pin);
        bool    disconnect (const Wire& w);
        void    disconnectAll (int uid);

        /** Would src -> dst close a loop that no Feedback node breaks? */
        bool wouldLoop (int src, int dst) const;

        /** A plugin into a VSTi / VST FX node (prepared here, the old one destroyed
            here, after it has left).  nullptr: the node empties. */
        void setPlugin (int uid, std::unique_ptr<rack::Hosted> plugin);

        /** A plugin out of its node, handed over (its node stays). */
        std::unique_ptr<rack::Hosted> takePlugin (int uid);

        Node*       find (int uid) const noexcept;
        Node*       output() const noexcept;
        Node*       firstInstrument() const noexcept;
        const std::vector<std::unique_ptr<Node>>& nodeList() const noexcept   { return nodes; }
        const std::vector<Wire>&                  wireList() const noexcept   { return wires; }

        /** The Output's latency (samples), as the order last worked it out. */
        int  latency() const noexcept                { return totalLatency.load (std::memory_order_relaxed); }

        /** A plugin reports a latency other than the order was worked out with. */
        bool latencyMoved() const;

        /** Works the order out again (a plugin's latency moved). */
        void rebuild();

        /** The delay after the Output that lines this channel up with the latest one. */
        void setTailDelay (int samples);
        int  tailDelay() const noexcept              { return tailSamples; }

        /** Bumped by every change: the windows (and the engine's latency) look again. */
        int  generation() const noexcept             { return gen.load (std::memory_order_relaxed); }

        //======================================================================
        //  Audio thread, the lock held
        //======================================================================
        void noteOn() noexcept                        { notePending.store (true, std::memory_order_relaxed); }

        /** Plays the graph for n samples into L / R (the Output's sound, after the tail delay). */
        void render (int n, const juce::MidiBuffer& midi, const Transport& t, const Taps& taps, float* L, float* R) noexcept;

        /** The channels the Sidechain In nodes read (bit n: channel n). */
        int keySources() const noexcept;

        void silence() noexcept;

    private:
        struct Delay
        {
            juce::AudioBuffer<float> ring;
            int delay = 0, pos = 0;
            void setDelay (int samples);
            void add (const float* srcL, const float* srcR, float* dstL, float* dstR, int n) noexcept;
            void apply (float* L, float* R, int n) noexcept;
        };

        struct Feed { Node* src; int pin; int delay; };     // delay: into delays (-1: none)
        struct Step { Node* node; std::vector<Feed> feeds; };

        struct Plan
        {
            std::vector<Step>  steps;        // in order
            std::vector<Step>  feedback;     // the Feedback nodes' inputs, taken in at the block's end
            std::vector<Delay> delays;
            int latency = 0;
        };

        Plan  makePlan (const std::vector<Node*>& list, const std::vector<Wire>& cables, int skipUid) const;
        void  install (Plan&& p);           // swaps it in under the lock; the old one goes here
        void  replan()                      { install (makePlan (nodePointers(), wires, 0)); }
        std::vector<Node*> nodePointers() const;
        void  gather (const std::vector<Feed>& feeds, Node& into, int n) noexcept;

        std::vector<std::unique_ptr<Node>> nodes;
        std::vector<Wire> wires;
        Plan plan;
        Delay tail;
        int tailSamples = 0;
        int nextUid = 1;
        double sr = 48000.0;
        int    block = 512;
        juce::AudioPlayHead* head = nullptr;
        std::atomic<int>  totalLatency { 0 }, gen { 0 };
        std::atomic<bool> notePending { false };
    };
}
