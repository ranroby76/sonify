// D:\workspace\Fanan_web_hub\src\SongBlocks.cpp

#include "SongBlocks.h"
#include "AudioEngine.h"

#include <cmath>
#include <limits>
#include <set>

juce::String SongBlockEdits::freshName (const SongBlocks& blocks, const juce::String& wanted)
{
    const auto taken = [&blocks] (const juce::String& name)
    {
        return std::any_of (blocks.blocks.begin(), blocks.blocks.end(), [&name] (const SongBlock& b) { return b.name.equalsIgnoreCase (name); });
    };

    constexpr int longest = 60;
    const auto base = wanted.trim().substring (0, longest);

    if (base.isNotEmpty() && ! taken (base))
        return base;

    for (int n = base.isNotEmpty() ? 2 : static_cast<int> (blocks.blocks.size()) + 1;; ++n)
    {
        // "Verse 2", "Verse 3"... (the name cut so the number fits).
        const auto number = " " + juce::String (n);
        const auto name = base.isNotEmpty() ? base.substring (0, longest - number.length()).trimEnd() + number : "Block" + number;

        if (! taken (name))
            return name;
    }
}

int SongBlockEdits::make (const AudioEngine& engine, SongBlocks& blocks, const std::vector<int>& clipIds, const juce::String& name)
{
    // The clips as they are now: the block starts on the bar the first one starts in and ends on
    // the bar line after the last one ends.
    const std::set<int> wanted (clipIds.begin(), clipIds.end());
    SongBlock block;
    auto first = std::numeric_limits<double>::max(), last = 0.0;

    for (const auto& track : engine.getTracks())
    {
        for (const auto& clip : track->clips)
        {
            if (wanted.count (clip.id) == 0)
                continue;

            block.parts.push_back ({ track->id, clip });
            first = juce::jmin (first, clip.start);
            last = juce::jmax (last, clip.start + clip.length);
        }
    }

    if (block.parts.empty())
        return 0;

    const auto& map = engine.getTempoMap();
    const auto start = map.barAt (first).start;
    const auto lastBar = map.barAt (last);
    const auto end = juce::jmax (last - lastBar.start < 1.0e-9 ? lastBar.start : lastBar.start + lastBar.length(), start + 1.0e-6);

    for (auto& part : block.parts)
    {
        part.clip.start -= start;
        part.clip.id = 0;
    }

    block.id = blocks.nextId++;
    block.name = freshName (blocks, name);
    block.colour = SongBlocks::colourFor (static_cast<int> (blocks.blocks.size()));
    block.length = end - start;
    block.bars = juce::jmax (1, map.barAt (end - 1.0e-6).index - map.barAt (start + 1.0e-6).index + 1);
    blocks.blocks.push_back (std::move (block));
    return blocks.blocks.back().id;
}

int SongBlockEdits::duplicate (SongBlocks& blocks, int id)
{
    const auto* block = blocks.find (id);

    if (block == nullptr)
        return 0;

    auto copy = *block;
    copy.id = blocks.nextId++;
    copy.name = freshName (blocks, block->name);
    const auto made = copy.id;

    const auto at = std::find_if (blocks.blocks.begin(), blocks.blocks.end(), [id] (const SongBlock& b) { return b.id == id; });
    blocks.blocks.insert (at + 1, std::move (copy));
    return made;
}

bool SongBlockEdits::remove (SongBlocks& blocks, int id)
{
    if (blocks.find (id) == nullptr)
        return false;

    blocks.blocks.erase (std::remove_if (blocks.blocks.begin(), blocks.blocks.end(), [id] (const SongBlock& b) { return b.id == id; }),
                         blocks.blocks.end());
    blocks.sequence.erase (std::remove (blocks.sequence.begin(), blocks.sequence.end(), id), blocks.sequence.end());
    return true;
}

std::vector<int> SongBlockEdits::tracksOf (const AudioEngine& engine, const SongBlocks& blocks)
{
    std::vector<int> tracks;

    for (const auto id : blocks.sequence)
        if (const auto* block = blocks.find (id))
            for (const auto& part : block->parts)
                if (std::find (tracks.begin(), tracks.end(), part.trackId) == tracks.end() && engine.findTrack (part.trackId) != nullptr)
                    tracks.push_back (part.trackId);

    return tracks;
}

SongBlockEdits::Built SongBlockEdits::build (AudioEngine& engine, double at)
{
    // A copy: adding clips doesn't touch the blocks, but nothing here leans on that.
    const auto blocks = engine.getSongBlocks();
    Built built;
    built.from = juce::jmax (0.0, std::isfinite (at) ? at : 0.0);
    auto position = built.from;

    for (const auto id : blocks.sequence)
    {
        const auto* block = blocks.find (id);

        if (block == nullptr)
            continue;

        for (const auto& part : block->parts)
        {
            auto clip = part.clip;
            clip.start = position + part.clip.start;

            if (engine.addClip (part.trackId, std::move (clip)) != 0)
                ++built.placed;
            else
                ++built.skipped;   // its track has gone
        }

        position += block->length;
    }

    built.to = position;
    return built;
}
