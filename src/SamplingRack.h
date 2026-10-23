// D:\workspace\Fanan_web_hub\src\SamplingRack.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "AudioEngine.h"
#include "Plugin.h"

#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
/**
    The Sampling Studio's nodes (Sampler Arena's node system, kept within its area): the
    plugins it samples, loaded straight from their files, and how they're wired.

      Sampling   the studio's notes: its MIDI out wires to the VSTis that play them.
      MIDI In    the live MIDI (the ports it picks, and the keyboard beside the nodes), to hear
                 the VSTis before sampling: its MIDI out wires to them too. It never samples.
      VSTi       an instrument plugin: MIDI in, its sound out as a pair (left, right).
      Effect     an effect plugin: sound in as a pair, sound out as a pair.
      Output     the sound that's sampled (and heard), stereo: what's wired into each side, added.

    A sound wire goes from one side of a node's pair to one side of another's (left to left,
    right to right, or crossed: two wires into one side add up). Wired node to node, both sides
    go at once, left to left and right to right.

    A VSTi is sampled (flagged) once the Sampling node's MIDI reaches it and its sound reaches the
    Output, straight or through effects; Sample needs at least one. Several flagged VSTis are
    sampled together, as one sound. A node can be muted (M: silent), an effect passed through
    (P: its sound goes on untouched). No loops: a wire that would make one isn't made.

    Threads: the nodes, the wires and the plugins are the message thread's; what the audio thread
    plays (the plan: the plugins in the order they play, each one's inputs) is swapped in under
    the rack's own lock, so the engine (holding its graph lock) never meets a half-made change.
    Plugins are activated as they're added (at the device's rate and block), and again as the
    device starts. The MIDI In node's ports are the engine's (AudioEngine::MidiIn::rack), which
    hands it the live notes only while the Sampling Studio is in front.
*/
class SamplingRack final : public AudioEngine::RackPlayer
{
public:
    enum class Kind { sampler, instrument, effect, output, midiIn };

    static constexpr int samplerId = 1;
    static constexpr int outputId = 2;
    static constexpr int midiInId = 3;

    /** A sound wire's side: left or right (a MIDI wire's is 0). bothSides: node to node, a wire
        for each side. */
    static constexpr int leftSide = 0, rightSide = 1, bothSides = -1;

    explicit SamplingRack (AudioEngine& engine);
    ~SamplingRack() override;

    //==============================================================================
    // Message thread

    /** A plugin as a node (an instrument: a VSTi; an effect; a plugin with no sound out isn't
        taken). x, y: its place in the area (the page's pixels). Returns its id, or 0 with why. */
    int addPlugin (std::unique_ptr<Plugin> plugin, const juce::String& format, float x, float y, juce::String& error);

    bool removeNode (int id);
    bool moveNode (int id, float x, float y);

    /** A wire from one node's out to another's in: a side of a pair to a side of a pair, or both
        sides (bothSides). Returns why it can't be, or nothing. */
    juce::String connect (int from, int to, int fromSide = bothSides, int toSide = bothSides);

    /** That wire (bothSides: every wire between the two nodes). */
    bool disconnect (int from, int to, int fromSide = bothSides, int toSide = bothSides);

    /** The MIDI In node's ports, by name (AudioEngine::everyMidiInput: all of them). */
    void setMidiInputs (const juce::StringArray& ports);
    juce::StringArray getMidiInputs() const                { return midiInputs; }

    void setMuted (int id, bool muted);
    void setPassThrough (int id, bool pass);

    /** The plugin's own window. False (with why) when it has none. */
    bool showWindow (int id, juce::String& why);
    void closeWindows();

    /** A VSTi the Sampling node's notes reach whose sound reaches the Output. */
    bool isFlagged (int id) const;

    /** At least one VSTi flagged: Sample can begin. */
    bool isReady() const;

    bool hasPlugins() const;

    /** The nodes and wires: for the page (withStates false), or to keep (the plugins' states too). */
    juce::var toVar (bool withStates) const;

    /** Nodes and wires kept before (toVar (true)): the plugins loaded again. One that can't be is
        kept as a missing node (silent, its settings kept), and said in problems. */
    void restore (const juce::var& saved, juce::StringArray& problems);

    /** Every plugin node gone; the Sampling, MIDI In and Output nodes stay (where they were). */
    void clear();

    /** Played by the device only while it's awake (the studio shows, or samples). */
    void setAwake (bool shouldBeAwake) noexcept           { awake.store (shouldBeAwake); }

    /** Plugins' upkeep: their calls for the message thread, restarts, parameter rescans. Several
        times a second. True when the page's view of them changed. */
    bool update();

    //==============================================================================
    // AudioEngine::RackPlayer: the engine's threads, under its graph lock
    void prepareRack (double sampleRate, int blockSize) override;
    void releaseRack() override;
    bool isRackAwake() const noexcept override              { return awake.load (std::memory_order_relaxed); }
    void renderRack (const juce::MidiBuffer& notes, const juce::MidiBuffer& live, float* leftOut, float* rightOut,
                     int numSamples, const clap_event_transport_t& transport) noexcept override;

private:
    struct Node
    {
        int id = 0;
        Kind kind = Kind::instrument;
        std::unique_ptr<Plugin> plugin;
        juce::String format;                 // "VST3", "VST2", "CLAP", "AU", "LV2"
        juce::File file;
        juce::String pluginId, name, vendor;
        float x = -1.0f, y = -1.0f;          // -1: the page places it
        std::atomic<bool> muted { false }, pass { false };
        bool missing = false;                // its plugin couldn't be loaded again: kept, silent
        bool restarting = false;             // out of the plan while its plugin restarts (planLock's): silent
        juce::String keptState;              // a missing one's state, as it was kept (base64)
    };

    struct Wire
    {
        int from = 0, to = 0;
        int fromSide = 0, toSide = 0;        // left or right (a MIDI wire's: 0)
    };

    /** A sound coming in: a step's side, into a side. */
    struct Input
    {
        int step = 0, fromSide = 0, toSide = 0;
    };

    /** What the audio thread plays: the plugin nodes reaching the Output, upstream first. */
    struct Step
    {
        Node* node = nullptr;
        Plugin* plugin = nullptr;
        bool instrument = false;
        bool hearsNotes = false;             // wired from the Sampling node
        bool hearsLive = false;              // wired from the MIDI In node
        std::vector<Input> inputs;           // the sound wired in
        juce::AudioBuffer<float> sound;
    };

    struct Plan
    {
        std::vector<Step> steps;
        std::vector<Input> outputInputs;     // the sound wired into the Output
    };

    Node* find (int id) const noexcept;
    bool reachesOutput (int id) const;
    bool hearsNotes (int id) const;
    bool hearsLive (int id) const;
    static bool isMidiSource (Kind kind) noexcept    { return kind == Kind::sampler || kind == Kind::midiIn; }
    bool reaches (int from, int to) const;   // through wires
    bool activatePlugin (Plugin& plugin);
    void rebuild();
    std::unique_ptr<Plan> makePlan() const;
    static juce::String kindName (Kind kind);

    AudioEngine& engine;
    std::vector<std::unique_ptr<Node>> nodes;
    std::vector<Wire> wires;
    int nextId = midiInId + 1;
    juce::StringArray midiInputs { AudioEngine::everyMidiInput };

    juce::SpinLock planLock;
    std::unique_ptr<Plan> plan;              // swapped under planLock
    std::atomic<bool> awake { false };
    double rate = 0.0;                       // as activated (the audio thread's while it renders)
    int blockSize = 0;
    juce::MidiBuffer chunkNotes, chunkLive, chunkBoth, noNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplingRack)
};
