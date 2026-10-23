// C:\workspace\Fanan_web_hub\src\EffectChain.cpp

#include "EffectChain.h"

#include <algorithm>

namespace
{
    constexpr int minimumBufferSize = 4096;
}

//==============================================================================
EffectChain::EffectChain()
{
    // Reserved now, so changes made under the engine's lock never allocate.
    inserts.reserve (static_cast<size_t> (maxInserts));
}

EffectChain::~EffectChain()
{
    clear();
}

void EffectChain::clear()
{
    // Last insert first, the reverse of how a signal meets them.
    while (! inserts.empty())
        inserts.pop_back();
}

//==============================================================================
EffectChain::Insert* EffectChain::at (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, size()) ? inserts[static_cast<size_t> (index)].get() : nullptr;
}

EffectChain::Insert* EffectChain::find (int insertId) const noexcept
{
    return at (indexOf (insertId));
}

int EffectChain::indexOf (int insertId) const noexcept
{
    for (size_t i = 0; i < inserts.size(); ++i)
        if (inserts[i]->id == insertId)
            return static_cast<int> (i);

    return -1;
}

bool EffectChain::add (std::unique_ptr<Insert> insert, int position)
{
    if (insert == nullptr || isFull())
        return false;

    const auto where = position < 0 ? size() : juce::jmin (position, size());
    inserts.insert (inserts.begin() + where, std::move (insert));
    return true;
}

std::unique_ptr<EffectChain::Insert> EffectChain::remove (int insertId)
{
    const auto index = indexOf (insertId);

    if (index < 0)
        return {};

    auto removed = std::move (inserts[static_cast<size_t> (index)]);
    inserts.erase (inserts.begin() + index);
    return removed;
}

std::unique_ptr<EffectChain::Insert> EffectChain::replace (int insertId, std::unique_ptr<Insert> newInsert)
{
    const auto index = indexOf (insertId);

    if (index < 0 || newInsert == nullptr)
        return {};

    auto old = std::move (inserts[static_cast<size_t> (index)]);
    inserts[static_cast<size_t> (index)] = std::move (newInsert);
    return old;
}

bool EffectChain::move (int insertId, int newPosition)
{
    const auto from = indexOf (insertId);

    if (from < 0)
        return false;

    const auto to = juce::jlimit (0, size() - 1, newPosition);

    if (to > from)
        std::rotate (inserts.begin() + from, inserts.begin() + from + 1, inserts.begin() + to + 1);
    else if (to < from)
        std::rotate (inserts.begin() + to, inserts.begin() + from, inserts.begin() + from + 1);

    return true;
}

//==============================================================================
void EffectChain::prepare (int maxBlockSize)
{
    dry.setSize (2, juce::jmax (maxBlockSize * 2, minimumBufferSize), false, true, true);
}

int EffectChain::getLatencySamples() const noexcept
{
    int total = 0;

    for (const auto& insert : inserts)
        if (insert != nullptr && insert->plugin != nullptr && insert->plugin->isActive() && ! insert->bypassed.load (std::memory_order_relaxed))
            total += juce::jmax (0, insert->plugin->getLatencySamples());

    return total;
}

void EffectChain::process (float* left, float* right, int numSamples,
                           const juce::MidiBuffer& midi, const clap_event_transport_t& transport) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (const auto& slot : inserts)
    {
        auto& insert = *slot;
        const bool bypassed = insert.bypassed.load (std::memory_order_relaxed);
        auto* plugin = insert.plugin.get();

        // A plugin that could not start passes the signal through untouched.
        if (plugin == nullptr || ! plugin->isActive())
        {
            insert.mix = bypassed ? 0.0f : 1.0f;
            continue;
        }

        if (bypassed && insert.mix <= 0.0f)
            continue;

        if (! bypassed && insert.mix >= 1.0f)
        {
            plugin->process (left, right, left, right, numSamples, midi, &transport);
            continue;
        }

        // Switching: fade from where the mix is now to where it's going, across this block.
        const auto target = bypassed ? 0.0f : 1.0f;

        if (numSamples > dry.getNumSamples())
        {
            insert.mix = target;   // no room for a fade (never with a normal block size)

            if (! bypassed)
                plugin->process (left, right, left, right, numSamples, midi, &transport);

            continue;
        }

        auto* dryLeft = dry.getWritePointer (0);
        auto* dryRight = dry.getWritePointer (1);
        std::copy (left, left + numSamples, dryLeft);
        std::copy (right, right + numSamples, dryRight);

        plugin->process (left, right, left, right, numSamples, midi, &transport);

        const auto step = (target - insert.mix) / static_cast<float> (numSamples);
        auto mix = insert.mix;

        for (int i = 0; i < numSamples; ++i)
        {
            mix += step;
            left[i] = dryLeft[i] + (left[i] - dryLeft[i]) * mix;
            right[i] = dryRight[i] + (right[i] - dryRight[i]) * mix;
        }

        insert.mix = target;
    }
}
