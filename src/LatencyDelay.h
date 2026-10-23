// D:\workspace\Fanan_web_hub\src\LatencyDelay.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cstring>

//==============================================================================
/**
    Plugin delay compensation's delay line: a stereo stretch of memory that makes a track, a bus
    or a group of outputs late by a set number of samples, so sounds that went through plugins
    with different latencies meet in time.

    Its memory is made on the message thread (prepare); process() runs on the audio thread and
    never allocates. A delay that starts (from none, or after blocks it wasn't used in) starts
    from silence, so nothing old is heard.
*/
class LatencyDelay final
{
public:
    /** The longest delay it holds (about 0.68 s at 48 kHz): compensation stops there. */
    static constexpr int capacity = 32768;

    LatencyDelay() = default;

    /** Message thread, before the audio thread can use it. */
    void prepare()
    {
        ring.setSize (2, capacity, false, true, false);
        ring.clear();
        writeAt = 0;
        lastDelay = 0;
        filled = 0;
    }

    /** Audio thread: not used this block (no delay): the next delay starts from silence. */
    void rest() noexcept                     { lastDelay = 0; }

    /** Forgets what it holds (a render starting; not on the device's thread: it clears it all). */
    void clear() noexcept
    {
        ring.clear();
        writeAt = 0;
        lastDelay = 0;
        filled = 0;
    }

    /** Audio thread: the block, late by delay samples, in place. right may be null (mono).
        block: the engine's count of blocks: a block skipped since the last one it played means
        what it holds is old, and it starts again from silence. */
    void process (float* left, float* right, int numSamples, int delay, juce::uint64 block) noexcept
    {
        if (left == nullptr || ring.getNumSamples() != capacity)
            return;

        delay = juce::jlimit (0, capacity - 1, delay);

        if (block != lastBlock + 1)
            lastDelay = 0;   // it wasn't played meanwhile: what it holds is from before

        lastBlock = block;

        if (delay == 0 && lastDelay == 0)
            return;

        if (lastDelay == 0)
            filled = 0;   // from nothing: what it holds is from before

        // Silence where it reads that wasn't written since (a start, or a longer delay than
        // before): never what it held before.
        if (delay > filled)
        {
            clearSpan ((writeAt - delay) & (capacity - 1), delay - filled);
            filled = delay;
        }

        lastDelay = delay;

        if (delay == 0)
            return;

        filled = juce::jmin (capacity, filled + numSamples);

        auto* ringLeft = ring.getWritePointer (0);
        auto* ringRight = ring.getWritePointer (1);
        constexpr int mask = capacity - 1;

        for (int i = 0; i < numSamples; ++i)
        {
            ringLeft[writeAt] = left[i];
            ringRight[writeAt] = right != nullptr ? right[i] : left[i];

            const auto readAt = (writeAt - delay) & mask;
            left[i] = ringLeft[readAt];

            if (right != nullptr)
                right[i] = ringRight[readAt];

            writeAt = (writeAt + 1) & mask;
        }
    }

private:
    static_assert ((capacity & (capacity - 1)) == 0, "the capacity wraps with a mask");

    /** Silence over count samples from from, round the ring (no more: it's the audio thread). */
    void clearSpan (int from, int count) noexcept
    {
        const auto first = juce::jmin (count, capacity - from);

        for (int channel = 0; channel < 2; ++channel)
        {
            auto* data = ring.getWritePointer (channel);
            std::memset (data + from, 0, sizeof (float) * static_cast<size_t> (first));

            if (count > first)
                std::memset (data, 0, sizeof (float) * static_cast<size_t> (count - first));
        }
    }

    juce::AudioBuffer<float> ring;
    int writeAt = 0, lastDelay = 0;
    int filled = 0;                       // samples written since it (re)started, up to its capacity
    juce::uint64 lastBlock = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LatencyDelay)
};
