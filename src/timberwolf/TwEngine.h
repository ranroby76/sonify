// D:\workspace\Fanan_web_hub\src\timberwolf\TwEngine.h
#pragma once
//==============================================================================
//  TwEngine.h  -  the chain, playing.
//
//  The editor changes a Graph (TwGraph.h) on the message thread; rebuild()
//  turns it into a Plan the audio thread plays: a list of steps over a few
//  stereo buffers, one buffer per path in play.
//
//      block   an effect processes the buffer of its path, in place
//      fork    a split: path B's buffer gets the signal (all of it, or the
//              highs of a crossover, A keeping the lows)
//      blend   the two paths meet: B's buffer is mixed into A's, each at its
//              level and pan, the earlier one delayed to meet the later (an
//              oversampled amp on one path, nothing on the other: no combing)
//
//  The effects themselves (units) outlive plans: moving an effect, or adding
//  another, keeps every other effect's delay lines and envelopes as they are.
//  A new effect is made and prepared on the message thread before the plan that
//  plays it is handed over; one taken out is let go once the audio thread has
//  left the plan that played it.
//
//  An effect's knobs and its on switch are its slot's host parameters, read at
//  the start of every buffer.  Switched on or off, it fades over 10 ms; one
//  with trails (delays, reverbs) keeps ringing out after it's off.
//
//  Threads: everything but process() on the message thread; process() on the
//  audio thread, never allocating, never locking.
//==============================================================================
#include "TwGraph.h"
#include "TwRegistry.h"

#include <atomic>
#include <map>
#include <memory>

namespace tw
{
    /** Where an effect's knobs come from: its slot's host parameters, 0..1. */
    struct SlotSource
    {
        std::array<const std::atomic<float>*, maxKnobs> knobs {};
        const std::atomic<float>* on = nullptr;
    };

    class Engine
    {
    public:
        Engine();
        ~Engine();

        void setSlotSources (const std::array<SlotSource, maxBlocks>& sources);

        /** The rate and the largest buffer; every effect prepared again. Not while playing. */
        void prepare (double sampleRate, int maxBlock);
        void reset();

        /** The graph as it is now: effects made for new uids, the new plan handed over, the
            ones no longer in it let go. Returns the chain's latency in samples. */
        int rebuild (const Graph& graph);

        /** A parallel's split and blend settings, without a rebuild. */
        void updateParallel (int uid, const ParallelSettings& s);

        /** Audio thread: the chain over l and r, in place. */
        void process (float* l, float* r, int n, double bpm, bool playing) noexcept;

        int getLatency() const noexcept                 { return latency.load(); }

        /** Message thread, about 30 times a second: every effect's messageTick. */
        void tick();

        /** For the panel: an effect's meter (its Block::meter), -1 when it has none. */
        float meterOf (int uid) const;

        /** For the panel: whether an effect is sounding now (on, or ringing out). */
        bool isSounding (int uid) const;

        const Context& context() const noexcept         { return ctx; }

    private:
        struct BlockUnit;
        struct ParUnit;
        struct Op;
        struct Plan;

        int compile (const std::vector<Node>& chain, int level, int& nextLevel, Plan& plan, const Graph& graph);
        void runBlock (BlockUnit& u, float* l, float* r, int n) noexcept;
        void guard (BlockUnit& u, float* l, float* r, int n) noexcept;
        void handOver (std::unique_ptr<Plan> next);

        Context ctx;
        std::array<SlotSource, maxBlocks> slots {};

        std::map<int, std::unique_ptr<BlockUnit>> units;    // by uid (message thread owns)
        std::map<int, std::unique_ptr<ParUnit>> parallels;  // by uid

        std::unique_ptr<Plan> live;
        std::atomic<Plan*> current { nullptr };
        std::atomic<Plan*> inUse { nullptr };

        static constexpr int maxLevels = maxBlocks + 1;
        juce::AudioBuffer<float> buffers;   // 2 channels per level
        juce::AudioBuffer<float> scratch;   // dry copy, ramp
        bool prepared = false;

        std::atomic<int> latency { 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Engine)
    };
}
