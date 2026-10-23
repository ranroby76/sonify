// D:\workspace\Fanan_web_hub\src\SongBlocks.h

#pragma once

#include "Sequence.h"

#include <algorithm>
#include <vector>

class AudioEngine;

//==============================================================================
/**
    Song Blocks: Sonify's take on Studio One's Scratch Pads. Clips selected in the arrangement
    are made into a block: a snapshot of them as they were (a copy, not linked to them), from
    the start of the bar the first one starts in to the end of the bar the last one ends in. A
    block is a pad in the Song Blocks window; pads put in order are the song, and Build Song
    lays their clips out on the timeline one block after another, each clip on the track it
    came from.

    The blocks and their order are part of the song: saved with it, and every change to them is
    a step in Edit > Undo. Nothing plays from them: they're copies until they're built.
    Message thread only.
*/
struct SongBlock
{
    /** One clip of it, and the track it came from (a track's id is the song's while it's open;
        song files name it by its place). */
    struct Part
    {
        int trackId = 0;
        MidiClip clip;            // start: quarter notes from the block's start
    };

    int id = 0;
    juce::String name;
    juce::String colour;          // "#rrggbb"
    double length = 0.0;          // quarter notes: the whole bars it was made from
    int bars = 0;                 // how many bars that was, where it was made
    std::vector<Part> parts;
};

struct SongBlocks
{
    std::vector<SongBlock> blocks;
    std::vector<int> sequence;    // the song: block ids in the order Build Song lays them out
    int nextId = 1;

    const SongBlock* find (int id) const noexcept
    {
        const auto found = std::find_if (blocks.begin(), blocks.end(), [id] (const SongBlock& b) { return b.id == id; });
        return found != blocks.end() ? &*found : nullptr;
    }

    SongBlock* find (int id) noexcept
    {
        const auto found = std::find_if (blocks.begin(), blocks.end(), [id] (const SongBlock& b) { return b.id == id; });
        return found != blocks.end() ? &*found : nullptr;
    }

    /** The song's length in quarter notes: every block in it, one after another. */
    double sequenceLength() const noexcept
    {
        double total = 0.0;

        for (const auto id : sequence)
            if (const auto* block = find (id))
                total += block->length;

        return total;
    }

    /** The pads' colours, in the order new blocks take them. */
    static juce::String colourFor (int index)
    {
        static const char* const palette[] = { "#2fb36b", "#3f8fd8", "#e5a93d", "#d65db1", "#e5484d", "#8b6cf0", "#25b5b0", "#c2c95a" };
        return palette[static_cast<size_t> (juce::jmax (0, index)) % (sizeof (palette) / sizeof (palette[0]))];
    }
};

//==============================================================================
/** What the Song Blocks window does to them (MainBlocks.cpp takes each as one undo step). */
namespace SongBlockEdits
{
    /** A new block from these clips (by id), as they are in the song now, added after the
        others; its id, or 0 when none of the clips is there. */
    int make (const AudioEngine& engine, SongBlocks& blocks, const std::vector<int>& clipIds, const juce::String& name);

    /** A copy of a block, right after it among the pads (its id, 0: no such block). */
    int duplicate (SongBlocks& blocks, int id);

    /** A block taken out, and out of the song's order. */
    bool remove (SongBlocks& blocks, int id);

    /** A name no other block has: the one asked for, or "Block 1", "Block 2"... */
    juce::String freshName (const SongBlocks& blocks, const juce::String& wanted);

    /** The tracks the song's blocks lay clips on (those still in the song). */
    std::vector<int> tracksOf (const AudioEngine& engine, const SongBlocks& blocks);

    struct Built
    {
        double from = 0.0, to = 0.0;     // song positions the song was laid out between
        int placed = 0, skipped = 0;     // clips laid out, clips whose track has gone
    };

    /** Lays the song out from a song position: each block's clips on the tracks they came from,
        one block after another, in the song's order. */
    Built build (AudioEngine& engine, double at);
}
