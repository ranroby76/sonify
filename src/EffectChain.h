// C:\workspace\Fanan_web_hub\src\EffectChain.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Plugin.h"

#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
/**
    An ordered chain of insert effects, as on a Studio One channel. Every track has
    one, and so does the Main channel.

    The engine changes a chain only while it holds its graph lock, and the audio
    thread processes it under the same lock, so a change is never seen half done.
    Room for every insert is reserved up front: adding, moving or removing one never
    allocates while that lock is held.

    Each insert can be bypassed. Switching fades between the processed and the dry
    signal across one block, so it never clicks.
*/
class EffectChain final
{
public:
    static constexpr int maxInserts = 16;

    struct Insert
    {
        Insert (int insertId, std::unique_ptr<Plugin> effect)
            : id (insertId), plugin (std::move (effect)) {}

        const int id;                              // unique in the engine, so the page can name it
        std::unique_ptr<Plugin> plugin;
        std::atomic<bool> bypassed { false };
        float mix = 1.0f;                          // audio thread: 1 processed, 0 dry

        JUCE_DECLARE_NON_COPYABLE (Insert)
    };

    EffectChain();
    ~EffectChain();

    //==============================================================================
    // Message thread. The engine holds its graph lock around the changes.
    int size() const noexcept                 { return static_cast<int> (inserts.size()); }
    bool isFull() const noexcept              { return size() >= maxInserts; }
    Insert* at (int index) const noexcept;
    Insert* find (int insertId) const noexcept;
    int indexOf (int insertId) const noexcept;

    /** Adds at position (clamped; -1 means the end). Does nothing when the chain is full. */
    bool add (std::unique_ptr<Insert> insert, int position);

    /** Takes one out; destroy what it returns after letting go of the lock. */
    std::unique_ptr<Insert> remove (int insertId);

    /** Swaps in a new insert at the same position; returns the old one to destroy. */
    std::unique_ptr<Insert> replace (int insertId, std::unique_ptr<Insert> newInsert);

    /** Moves an insert to a new position (clamped). */
    bool move (int insertId, int newPosition);

    /** Destroys every insert. Only while no audio callback can run. */
    void clear();

    //==============================================================================
    // Audio side. prepare() runs while no audio callback is active.
    void prepare (int maxBlockSize);

    /** Message thread: how late the chain makes its sound, the inserts that play (not bypassed,
        started) added up. */
    int getLatencySamples() const noexcept;

    /** Runs the inserts in order, in place, on a stereo block. */
    void process (float* left, float* right, int numSamples,
                  const juce::MidiBuffer& midi, const clap_event_transport_t& transport) noexcept;

private:
    std::vector<std::unique_ptr<Insert>> inserts;
    juce::AudioBuffer<float> dry;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectChain)
};
