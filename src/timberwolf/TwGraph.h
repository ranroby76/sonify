// D:\workspace\Fanan_web_hub\src\timberwolf\TwGraph.h
#pragma once
//==============================================================================
//  TwGraph.h  -  the chain as the editor and the presets see it.
//
//  A chain is a row of nodes from the input to the output.  A node is either
//  an effect (a block, by its uid) or a parallel: a split that sends the signal
//  down two paths, A and B, each a chain of its own (so a path can split again),
//  and the blend where they meet.  Any order, up to maxBlocks effects, as many
//  splits as the effects leave room for.
//
//      input > Gate > Drive > [split  A: Clean amp > Cab  |  B: Lead amp > Cab  blend] > Delay > output
//
//  Every effect has a uid (unique in this chain, kept in presets) and a slot
//  (0..15): the host parameters it owns - its knobs and its on switch - are the
//  slot's.  A new effect takes the lowest free slot.  Every parallel has a uid
//  too, and its split and blend settings.
//
//  Where a chain is - the main one, or a path inside a parallel inside... - is
//  a Path: the steps from the main chain down.  Where a node is, is its chain's
//  Path and its index in that chain.
//
//  Message thread only: the audio thread plays what TwEngine compiles from it.
//==============================================================================
#include "TwBlock.h"

#include <map>
#include <optional>
#include <vector>

namespace tw
{
    /** A split's and its blend's settings. */
    struct ParallelSettings
    {
        int splitType = 0;          // 0: Y (both paths get it all), 1: crossover (A low, B high)
        float crossoverHz = 700.0f;
        float levelA = 0.0f, levelB = 0.0f;   // dB at the blend
        float panA = 0.0f, panB = 0.0f;       // -100 left .. 100 right
        bool invertB = false;                 // B's polarity flipped at the blend
    };

    struct Node
    {
        enum class Kind { block, parallel };

        Kind kind = Kind::block;
        int uid = 0;
        std::vector<Node> a, b;     // a parallel's paths (empty: the signal passes)
        ParallelSettings settings;  // a parallel's
    };

    /** A step down into a parallel: the parallel's index in its chain, and which path. */
    struct Step
    {
        int index = 0;
        bool pathB = false;

        bool operator== (const Step& o) const noexcept   { return index == o.index && pathB == o.pathB; }
    };

    using Path = std::vector<Step>;

    struct Location
    {
        Path chain;
        int index = 0;
    };

    /** An effect in the chain: what it is, and its slot. */
    struct BlockEntry
    {
        juce::String type;          // its BlockInfo's id
        int slot = 0;
    };

    //==========================================================================
    class Graph
    {
    public:
        std::vector<Node> main;
        std::map<int, BlockEntry> blocks;   // by uid

        //======================================================================
        int numBlocks() const noexcept          { return (int) blocks.size(); }
        bool isFull() const noexcept            { return numBlocks() >= maxBlocks; }
        int numParallels() const;

        /** The chain a path leads to (nullptr: there isn't one). */
        std::vector<Node>* chainAt (const Path&);
        const std::vector<Node>* chainAt (const Path&) const;

        /** Where a node (an effect or a parallel) is, by its uid. */
        std::optional<Location> find (int uid) const;
        const Node* node (int uid) const;
        Node* node (int uid);

        /** The effect in a slot, by its uid (0: none). */
        int uidInSlot (int slot) const;
        int freeSlot() const;

        //======================================================================
        //  Edits. Each returns false (and changes nothing) when it can't be done.
        //======================================================================
        /** A new effect of this type at index of the chain at path; its uid (0: full, or no such
            chain). */
        int addBlock (const juce::String& type, const Path& chain, int index);

        /** A new parallel (two empty paths) at index of the chain at path; its uid. */
        int addParallel (const Path& chain, int index);

        /** A parallel round an effect or parallel already there: it becomes path A. */
        int splitAround (int uid);

        /** An effect or a whole parallel (with everything in it) taken out. */
        bool remove (int uid);

        /** A parallel taken out, its paths kept: A's nodes then B's, in its place. */
        bool unsplit (int parallelUid);

        /** A node moved: to index of the chain at path (index as the chain is before the move). */
        bool move (int uid, const Path& chain, int index);

        /** A parallel's paths swapped. */
        bool swapPaths (int parallelUid);

        /** Another effect type in the same place and slot (its knobs go to the new type's
            defaults: the processor's part). */
        bool replace (int uid, const juce::String& type);

        void clear();

        //======================================================================
        //  As JSON (presets and the plugin's state).
        //======================================================================
        juce::var toVar() const;

        /** Reads one; unknown effect types are left out, slots are kept when free. False: it
            isn't a chain. */
        bool fromVar (const juce::var&);

        /** Every uid in the order the signal meets them (A before B). */
        std::vector<int> blockOrder() const;

    private:
        int nextUid = 1;
        int takeUid()   { return nextUid++; }

        static bool contains (const std::vector<Node>& chain, int uid);
        static void collectUids (const std::vector<Node>& chain, std::vector<int>& into, bool blocksOnly);
        void dropBlocksIn (const Node& n);
        bool removeNode (int uid, Node* taken);
    };
}
