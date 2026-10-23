// D:\workspace\Fanan_web_hub\src\Container.h

#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "Plugin.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class ContainerPlugin;

//==============================================================================
/**
    The Node Rack (internally: containers): Sonify's system tool for a whole patch of plugins
    in one slot, as Colosseum's rack is. A rack is a plugin like any other (Node Rack goes on
    a track's instrument slot, Node Rack FX into its inserts); inside, its own graph wires
    plugins and system tools as nodes: audio, MIDI and sidechain cables between pins, from the
    rack's inputs to its outputs, synced to the song's tempo and time signature. Its window
    (ContainerCanvas.h) has Colosseum's Rack (the node canvas), Mixer and Manual. Saving a
    patch (a .container file) keeps everything in it: every node with its settings and its
    plugin's state, every cable, and the mixer's levels.

    Nodes:
      a plugin       any plugin Sonify has (VST3, WCLAP or Sonify's own, a Container too: a
                     rack inside the rack), wrapped in a PluginNode: its own MIDI channel
                     filter, pass-through, mute, transport override (Colosseum's CH, P, M and
                     T buttons) and mixer level;
      a system tool  Colosseum's Connector, Latcher, Step Seq and Transient Splitter
                     (NodeTools.h), and the Stereo Meter and MIDI Monitor;
      I/O            the container's Audio In (main L/R, then the sidechain L/R a track can
                     send to), Audio Out, MIDI In (the track's notes) and MIDI Out (what
                     other tracks can listen to).

    Threads: the graph is changed on the message thread (JUCE's graph rebuilds and prepares
    nodes there, and swaps its render sequence in for the audio thread); process() runs on
    the audio thread under the engine's lock, like any plugin's.
*/
namespace Containers
{
    inline constexpr const char* instrumentId = "sonify.builtin.container";     // on an instrument slot
    inline constexpr const char* effectId     = "sonify.builtin.fxcontainer";   // in the inserts

    bool isContainerId (const juce::String& pluginId);

    Plugin::Descriptor describe (bool effect);

    /** Where .container files are saved (Documents\Sonify\Containers unless the host says). */
    juce::File defaultFolder();

    /** A place on the canvas (its virtual coordinates). */
    struct Spot
    {
        float x = 0.0f, y = 0.0f;
    };

    /** A plugin Sonify offers, for the canvas's menu and its Tools panel. */
    struct Choice
    {
        juce::String file, id, name, vendor, kind, category, format;
        bool favorite = false;
    };

    /** What a container needs of Sonify. MainComponent is it; without one (the engine tests),
        plugins load straight from their files and node ids count from a million. */
    struct Host
    {
        virtual ~Host() = default;

        /** A plugin by its file and id, made but not yet activated (its node activates it). */
        virtual std::unique_ptr<Plugin> loadPlugin (const juce::File& file, const juce::String& pluginId, juce::String& error) = 0;

        /** The plugins the user has, for the menu (Sonify's own system tools are added by the canvas). */
        virtual std::vector<Choice> listPlugins() = 0;

        /** Shows a node's plugin screen as Sonify shows any plugin's: its web page, the page's
            generic controls, or its own window. */
        virtual void openScreen (Plugin& plugin) = 0;

        /** An id no other plugin in the engine has, so the page can name a node's plugin. */
        virtual int allocateSlotId() = 0;

        /** Something in a container changed (its name, a node, a cable): the song has unsaved
            changes, and the page shows the container's new name. */
        virtual void containerChanged (ContainerPlugin& container) = 0;

        /** The folder container presets go to. */
        virtual juce::File containersFolder()                       { return defaultFolder(); }

        /** The playing rate and block size plugins are activated at (0: not playing). */
        virtual double sampleRate() const                           { return 0.0; }

        /** The folder Browse opens plugin files from: the last one used (empty: the VST3 folder). */
        virtual juce::File pluginBrowseFolder()                     { return {}; }
        virtual void setPluginBrowseFolder (const juce::File&)      {}

        /** Sonify's own window (its native handle), which rack windows stay in front of; null: none. */
        virtual void* mainWindowHandle()                            { return nullptr; }
    };

    void setHost (Host* host);
    Host* getHost();

    /** The container's window: made by ContainerCanvas.cpp in the app (none in the tests). */
    struct Screen
    {
        virtual ~Screen() = default;
        virtual void show (const juce::String& title) = 0;
        virtual void hide() = 0;
        virtual bool isShowing() const = 0;
    };

    using ScreenFactory = std::function<std::unique_ptr<Screen> (ContainerPlugin&)>;
    void setScreenFactory (ScreenFactory factory);
}

//==============================================================================
/**
    A Sonify plugin as a node of a container's graph (Colosseum's MeteringProcessor): its
    pins follow the plugin (an instrument: no audio in; an effect: main in, and a sidechain
    in when it has one), and it adds what Colosseum's nodes have: a MIDI channel filter for
    instruments (CH), pass-through for effects (P), and a transport of its own (T) for
    tempo-synced plugins. Muting is the graph's bypass (M), with every note let go.
*/
class PluginNode final : public juce::AudioProcessor
{
public:
    PluginNode (std::unique_ptr<Plugin> plugin, int slotId, ContainerPlugin& owner);
    ~PluginNode() override;

    Plugin& getPlugin() const noexcept                  { return *plugin; }
    const int slotId;

    bool isInstrument() const noexcept                  { return instrument; }
    bool hasSidechain() const noexcept                  { return sidechain; }
    bool isContainer() const noexcept;
    ContainerPlugin* asContainer() const noexcept;

    /** The MIDI channels an instrument hears, one bit each (bit 0: channel 1). */
    void setMidiChannelMask (int mask) noexcept         { channelMask.store (mask & 0xffff); }
    int getMidiChannelMask() const noexcept             { return channelMask.load(); }

    void setPassThrough (bool on) noexcept              { passThrough.store (on); }
    bool isPassThrough() const noexcept                 { return passThrough.load(); }

    /** Muted (the graph's bypass): every note the plugin holds is let go first. */
    void noteMuted() noexcept                           { notesOffOwed.store (true); }

    // Colosseum's T: a tempo and time signature of its own instead of the song's.
    bool isTransportSynced() const noexcept             { return synced.load(); }
    void setTransportSynced (bool s) noexcept           { synced.store (s); }
    double getCustomTempo() const noexcept              { return tempo.load(); }
    void setCustomTempo (double bpm) noexcept           { tempo.store (juce::jlimit (20.0, 999.0, bpm)); }
    int getCustomTimeSigNumerator() const noexcept      { return numerator.load(); }
    int getCustomTimeSigDenominator() const noexcept    { return denominator.load(); }
    void setCustomTimeSignature (int n, int d) noexcept { numerator.store (juce::jlimit (1, 32, n)); denominator.store (juce::jlimit (1, 32, d)); }

    bool isMidiInActive() const noexcept                { return midiInActive.load(); }
    bool isMidiOutActive() const noexcept               { return midiOutActive.load(); }

    /** Its level in the rack's Mixer (linear, 1: 0 dB), and what it played last (peak, for the meter). */
    void setGain (float linear) noexcept                { gain.store (juce::jlimit (0.0f, 20.0f, linear)); }
    float getGain() const noexcept                      { return gain.load(); }
    float getLevel() const noexcept                     { return level.load(); }

    /** Why the plugin stopped (a WCLAP that crashed); empty while it works. */
    juce::String getFailure() const;
    void setFailure (const juce::String& why);
    bool hasFailed() const noexcept                     { return failed.load(); }

    /** Runs fn while the audio thread can't be inside this node. */
    void whileSuspended (const std::function<void()>& fn);

    //==============================================================================
    const juce::String getName() const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override                     { return false; }
    bool acceptsMidi() const override                   { return true; }
    bool producesMidi() const override                  { return makesMidi; }
    double getTailLengthSeconds() const override        { return 0.0; }
    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

private:
    static BusesProperties busesFor (const Plugin& plugin);
    void filterChannels (juce::MidiBuffer& midi) noexcept;

    std::unique_ptr<Plugin> plugin;
    ContainerPlugin& owner;
    const juce::String name;
    const bool instrument, sidechain, makesMidi;

    std::atomic<int> channelMask { 0xffff };
    int lastMask = 0xffff;                           // audio thread
    std::array<juce::uint16, 128> held {};           // audio thread: the channels each note is held on
    std::atomic<bool> passThrough { false }, notesOffOwed { false };
    std::atomic<bool> synced { true };
    std::atomic<double> tempo { 120.0 };
    std::atomic<int> numerator { 4 }, denominator { 4 };
    std::atomic<bool> midiInActive { false }, midiOutActive { false };
    float midiInDecay = 0.0f, midiOutDecay = 0.0f;
    std::atomic<float> gain { 1.0f }, level { 0.0f };
    float lastGain = 1.0f;                           // audio thread: ramped from here

    juce::MidiBuffer filtered, notesOff;
    juce::CriticalSection failureLock;
    juce::String failure;
    std::atomic<bool> failed { false };              // the audio thread's view of it, without the lock

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginNode)
};

//==============================================================================
/** Colosseum's system tools for containers (their node faces are drawn by the canvas). */
namespace ContainerTools
{
    /** Connector: stereo in, stereo out, a volume and a mute (a place to sum or split cables). */
    class Connector final : public juce::AudioProcessor
    {
    public:
        Connector();
        const juce::String getName() const override                 { return "Connector"; }
        void prepareToPlay (double, int) override                   {}
        void releaseResources() override                            {}
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
        using juce::AudioProcessor::processBlock;
        juce::AudioProcessorEditor* createEditor() override          { return nullptr; }
        bool hasEditor() const override                              { return false; }
        bool acceptsMidi() const override                            { return false; }
        bool producesMidi() const override                           { return false; }
        double getTailLengthSeconds() const override                 { return 0.0; }
        int getNumPrograms() override                                { return 1; }
        int getCurrentProgram() override                             { return 0; }
        void setCurrentProgram (int) override                        {}
        const juce::String getProgramName (int) override             { return {}; }
        void changeProgramName (int, const juce::String&) override   {}
        void getStateInformation (juce::MemoryBlock&) override;
        void setStateInformation (const void*, int) override;

        /** 0 silence, 0.5 unity (0 dB), 1 +25 dB. */
        void setVolume (float normalised) noexcept                   { volume.store (juce::jlimit (0.0f, 1.0f, normalised)); }
        float getVolume() const noexcept                             { return volume.load(); }
        float getVolumeDb() const noexcept;
        void setMuted (bool m) noexcept                              { muted.store (m); }
        bool isMuted() const noexcept                                { return muted.load(); }
        void toggleMute() noexcept                                   { muted.store (! muted.load()); }

    private:
        std::atomic<float> volume { 0.5f };
        std::atomic<bool> muted { false };
        float lastGain = 1.0f;
    };

    /** Stereo Meter: L/R levels with peak hold and clip lights; it takes audio in only. */
    class StereoMeter final : public juce::AudioProcessor
    {
    public:
        StereoMeter();
        const juce::String getName() const override                 { return "Stereo Meter"; }
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override                            {}
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
        using juce::AudioProcessor::processBlock;
        juce::AudioProcessorEditor* createEditor() override          { return nullptr; }
        bool hasEditor() const override                              { return false; }
        bool acceptsMidi() const override                            { return false; }
        bool producesMidi() const override                           { return false; }
        double getTailLengthSeconds() const override                 { return 0.0; }
        int getNumPrograms() override                                { return 1; }
        int getCurrentProgram() override                             { return 0; }
        void setCurrentProgram (int) override                        {}
        const juce::String getProgramName (int) override             { return {}; }
        void changeProgramName (int, const juce::String&) override   {}
        void getStateInformation (juce::MemoryBlock&) override       {}
        void setStateInformation (const void*, int) override         {}

        float getLeftLevel() const noexcept                          { return leftLevel.load(); }
        float getRightLevel() const noexcept                         { return rightLevel.load(); }
        float getLeftPeak() const noexcept                           { return leftPeak.load(); }
        float getRightPeak() const noexcept                          { return rightPeak.load(); }
        bool isLeftClipping() const noexcept                         { return leftClipping.load(); }
        bool isRightClipping() const noexcept                        { return rightClipping.load(); }
        void resetClipping() noexcept;

    private:
        std::atomic<float> leftLevel { 0.0f }, rightLevel { 0.0f }, leftPeak { 0.0f }, rightPeak { 0.0f };
        std::atomic<bool> leftClipping { false }, rightClipping { false };
        float levelDecay = 0.95f;
        int peakHoldBlocks = 0, leftHold = 0, rightHold = 0;
    };

    /** MIDI Monitor: the last note or controller on each of the 16 channels; MIDI in only. */
    class MidiMonitor final : public juce::AudioProcessor
    {
    public:
        struct Event
        {
            bool isActive = false, isNoteOn = false, isCC = false;
            int channel = 1, noteNumber = 60, velocity = 0, ccNumber = 0, ccValue = 0;
            juce::String toString() const;
        };

        MidiMonitor();
        const juce::String getName() const override                 { return "MIDI Monitor"; }
        void prepareToPlay (double, int) override                   {}
        void releaseResources() override                            {}
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
        using juce::AudioProcessor::processBlock;
        juce::AudioProcessorEditor* createEditor() override          { return nullptr; }
        bool hasEditor() const override                              { return false; }
        bool acceptsMidi() const override                            { return true; }
        bool producesMidi() const override                           { return false; }
        double getTailLengthSeconds() const override                 { return 0.0; }
        int getNumPrograms() override                                { return 1; }
        int getCurrentProgram() override                             { return 0; }
        void setCurrentProgram (int) override                        {}
        const juce::String getProgramName (int) override             { return {}; }
        void changeProgramName (int, const juce::String&) override   {}
        void getStateInformation (juce::MemoryBlock&) override       {}
        void setStateInformation (const void*, int) override         {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

        std::array<Event, 16> getEvents() const;
        bool hasChanged() const noexcept                             { return changed.load(); }
        void clearChanged() noexcept                                 { changed.store (false); }

    private:
        std::array<Event, 16> events;
        mutable juce::SpinLock lock;
        std::atomic<bool> changed { false };
    };

    /** A plugin a container had that isn't here now: kept as it was, so saving again loses
        nothing, and shown in red until the plugin is back. */
    class Missing final : public juce::AudioProcessor
    {
    public:
        Missing (std::unique_ptr<juce::XmlElement> savedNode, int inputs, int outputs, bool midiIn, bool midiOut);
        const juce::String getName() const override                 { return title; }
        void prepareToPlay (double, int) override                   {}
        void releaseResources() override                            {}
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
        {
            // Silent out; its inputs past the outputs are its sources' own buffers, left alone.
            for (int channel = 0; channel < juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels()); ++channel)
                buffer.clear (channel, 0, buffer.getNumSamples());

            midi.clear();
        }
        using juce::AudioProcessor::processBlock;
        juce::AudioProcessorEditor* createEditor() override          { return nullptr; }
        bool hasEditor() const override                              { return false; }
        bool acceptsMidi() const override                            { return midiIn; }
        bool producesMidi() const override                           { return midiOut; }
        double getTailLengthSeconds() const override                 { return 0.0; }
        int getNumPrograms() override                                { return 1; }
        int getCurrentProgram() override                             { return 0; }
        void setCurrentProgram (int) override                        {}
        const juce::String getProgramName (int) override             { return {}; }
        void changeProgramName (int, const juce::String&) override   {}
        void getStateInformation (juce::MemoryBlock&) override       {}
        void setStateInformation (const void*, int) override         {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

        const juce::XmlElement& getSaved() const noexcept            { return *saved; }
        juce::String getWhy() const                                  { return why; }
        void setWhy (const juce::String& reason)                     { why = reason; }

    private:
        std::unique_ptr<juce::XmlElement> saved;
        juce::String title, why;
        const bool midiIn, midiOut;
    };
}

//==============================================================================
/**
    The container itself: a Sonify plugin holding a graph. Two kinds, by where it goes:
    Container (an instrument: MIDI in, audio out) and FX Container (an effect: the track's
    sound in, with a sidechain, and out). Both take MIDI and can play MIDI out.
*/
class ContainerPlugin final : public Plugin
{
public:
    explicit ContainerPlugin (bool effect);
    ~ContainerPlugin() override;

    bool isEffect() const noexcept                       { return effect; }

    /** The graph lines its plugins up itself (each node's latency); the container is as late
        as its longest path. */
    int getLatencySamples() const noexcept override      { return juce::jmax (0, graph->getLatencySamples()); }

    //==============================================================================
    // The graph, message thread.
    juce::AudioProcessorGraph& getGraph() noexcept       { return *graph; }
    juce::AudioProcessorGraph::Node::Ptr getAudioInputNode() const   { return audioIn; }
    juce::AudioProcessorGraph::Node::Ptr getAudioOutputNode() const  { return audioOut; }
    juce::AudioProcessorGraph::Node::Ptr getMidiInputNode() const    { return midiIn; }
    juce::AudioProcessorGraph::Node::Ptr getMidiOutputNode() const   { return midiOut; }
    bool isIoNode (const juce::AudioProcessorGraph::Node* node) const noexcept;

    /** The FX Container's Audio In, on or off (Colosseum's ON/OFF): off, the track's sound and
        the sidechain don't come in. (Not the graph's bypass: that would leave its buffers stale.) */
    bool isAudioInputOn() const noexcept                 { return audioInputOn.load(); }
    void setAudioInputOn (bool on);

    /** Whether a node is off: a muted plugin or tool, Audio Out turned off, or Audio In. */
    bool isOff (const juce::AudioProcessorGraph::Node* node) const noexcept;

    /** A plugin as a node at a place on the canvas; null (and error) when it can't load. */
    juce::AudioProcessorGraph::Node::Ptr addPlugin (const juce::File& file, const juce::String& pluginId,
                                                    Containers::Spot at, juce::String& error);
    juce::AudioProcessorGraph::Node::Ptr addPluginNode (std::unique_ptr<Plugin> plugin, Containers::Spot at);

    /** A system tool by its name: "Connector", "Latcher", "StepSeq", "TransientSplitter",
        "StereoMeter" or "MidiMonitor". */
    juce::AudioProcessorGraph::Node::Ptr addTool (const juce::String& toolName, Containers::Spot at);

    /** Takes a node out (its cables too); what it held is destroyed once the audio thread lets go. */
    void removeNode (juce::AudioProcessorGraph::NodeID id);

    bool connect (juce::AudioProcessorGraph::NodeID from, int fromChannel, juce::AudioProcessorGraph::NodeID to, int toChannel);
    bool connectMidi (juce::AudioProcessorGraph::NodeID from, juce::AudioProcessorGraph::NodeID to);
    void disconnect (const juce::AudioProcessorGraph::Connection& connection);
    void disconnectNode (juce::AudioProcessorGraph::NodeID id);

    /** Mutes a node (Colosseum's M: the graph's bypass), letting its notes go. */
    void setNodeMuted (juce::AudioProcessorGraph::NodeID id, bool muted);

    /** Every PluginNode in this container and in the containers inside it. */
    void forEachPluginNode (const std::function<void (PluginNode&)>& fn) const;

    /** A plugin in here (or deeper) by its slot id; null when there's none. */
    Plugin* findPlugin (int slotId) const;
    PluginNode* findNode (const Plugin& plugin) const;

    /** Says the container changed (for the song and the page) and repaints the canvas. */
    void changed();

    /** A tool's settings were edited (its editor): the song has unsaved changes, nothing else. */
    void toolEdited();

    /** PANIC: every note in the rack (and the racks inside it) let go, every latch released. */
    void panic();

    /** Reset: back to an empty rack (its name, its I/O and nothing else). */
    void reset();

    /** The rack's output level in its Mixer (linear, 1: 0 dB), and its last peak (for the meter). */
    void setOutputGain (float linear) noexcept           { outputGain.store (juce::jlimit (0.0f, 20.0f, linear)); }
    float getOutputGain() const noexcept                 { return outputGain.load(); }
    float getOutputLevel() const noexcept                { return outputLevel.load(); }

    /** What a new one is called: "Node Rack" and "Node Rack FX" in a track, "Container" and
        "FX Container" inside a rack. */
    static juce::String defaultName (bool effect, bool nested);
    std::function<void()> onStructureChanged;      // for anyone else who wants to hear of it

    /** Goes up whenever any container changes (a node, a cable, a preset loaded over it) or one
        goes. A canvas checks it before it uses what it keeps of the graphs it shows (the same
        container can be shown in two windows, and one inside it entered in a third). Message thread. */
    static juce::uint32 getStructureGeneration() noexcept;

    //==============================================================================
    // What Colosseum's container has: a name, a mute, a transport override and the zoom it was left at.
    juce::String getContainerName() const                { return containerName; }
    void setContainerName (const juce::String& newName);
    bool isMuted() const noexcept                        { return muted.load(); }
    void setMuted (bool m) noexcept                      { muted.store (m); }
    bool isTransportSynced() const noexcept              { return synced.load(); }
    void setTransportSynced (bool s) noexcept            { synced.store (s); }
    double getCustomTempo() const noexcept               { return tempo.load(); }
    void setCustomTempo (double bpm) noexcept            { tempo.store (juce::jlimit (20.0, 999.0, bpm)); }
    int getCustomTimeSigNumerator() const noexcept       { return numerator.load(); }
    int getCustomTimeSigDenominator() const noexcept     { return denominator.load(); }
    void setCustomTimeSignature (int n, int d) noexcept  { numerator.store (juce::jlimit (1, 32, n)); denominator.store (juce::jlimit (1, 32, d)); }
    float containerZoom = 0.7f;
    Containers::Spot containerPan;

    /** The transport this block's nodes see (this container's own when it isn't synced). Audio thread. */
    const clap_event_transport_t* getBlockTransport() const noexcept  { return blockTransport; }

    /** Plugins that couldn't be loaded the last time a state or preset was read. */
    const juce::StringArray& getProblems() const noexcept { return problems; }

    //==============================================================================
    // Macros: eight knobs on the rack's Rack page, each driving any number of controls in the
    // rack (a plugin's parameter, a node's Mixer level, the rack's Output), each over a range
    // of its own (from..to of the control's range, maybe inverted). They are the rack's own
    // parameters (ids 1..8, 0..1), so the song automates them and MIDI Learn and the AI reach
    // them. A macro reaches the nodes in this rack's own graph (a Container inside is a
    // node: its own macros are its parameters). Message thread unless said.
    static constexpr int numMacros = 8;

    struct MacroLink
    {
        enum class Kind { parameter, level, output };

        int macro = 0;
        Kind kind = Kind::parameter;
        juce::uint32 nodeUid = 0;       // the node (parameter, level)
        uint32_t paramId = 0;           // parameter: the plugin's own stable id
        float from = 0.0f, to = 1.0f;   // the part of the control's range the macro sweeps, 0..1
        bool inverted = false;          // the macro turned up turns the control down

        bool sameControl (const MacroLink& other) const noexcept
        {
            return macro == other.macro && kind == other.kind && nodeUid == other.nodeUid && paramId == other.paramId;
        }
    };

    juce::String getMacroName (int macro) const;
    void setMacroName (int macro, const juce::String& name);       // empty: "Macro n" again
    double getMacroValue (int macro) const noexcept;
    void setMacroValue (int macro, double value);                  // the knob turned: its controls follow

    const std::vector<MacroLink>& getMacroLinks() const noexcept  { return macroLinks; }
    void addMacroLink (const MacroLink& link);                     // the same control again: replaced
    void setMacroLink (int index, const MacroLink& link);
    void removeMacroLink (int index);

    /** What a link drives, for the page: "Diva: Cutoff", "Diva: Level", "Rack Output". */
    juce::String describeMacroLink (const MacroLink& link) const;

    /** Learn: a parameter just touched in the window of a plugin in this rack (once); false
        when none since the last call. forgetTouched() lets go of what was touched before. */
    bool takeTouchedControl (juce::uint32& nodeUid, uint32_t& paramId);
    void forgetTouched();

    /** The macros were edited (names, links, values): the song has unsaved changes. */
    std::function<void()> onMacrosChanged;

    //==============================================================================
    // Presets and state: the whole container as XML (Colosseum's ContainerPreset).
    juce::String toXml() const;
    bool fromXml (const juce::String& xml);
    bool savePreset (const juce::File& file) const;
    bool loadPreset (const juce::File& file);

    //==============================================================================
    // Plugin
    juce::String getFormatName() const override                          { return "Built-in"; }
    const Descriptor& getDescriptor() const noexcept override            { return descriptor; }
    const std::vector<Descriptor>& getPluginsInFile() const noexcept override { return inFile; }
    const juce::File& getSourceFile() const noexcept override;
    Role getRole() const noexcept override                               { return effect ? Role::effect : Role::instrument; }
    int getMainInputChannels() const noexcept override                   { return effect ? 2 : 0; }
    int getMainOutputChannels() const noexcept override                  { return 2; }
    bool hasSidechain() const noexcept override                          { return effect; }
    bool producesMidi() const noexcept override                          { return midiOutUsed.load(); }

    bool canSaveState() const noexcept override                          { return true; }
    bool saveState (juce::MemoryBlock& destination) override;
    bool loadState (const juce::MemoryBlock& source) override;
    bool consumeStateChanged() noexcept override;

    // The macros, as parameters.
    const std::vector<Parameter>& getParameters() const noexcept override { return macroParameters; }
    int findParameter (uint32_t id) const noexcept override               { return id >= 1 && id <= numMacros ? static_cast<int> (id) - 1 : -1; }
    double getParameterValue (int index) const noexcept override          { return getMacroValue (index); }
    double readParameterValue (int index) override                        { return getMacroValue (index); }
    juce::String getParameterText (int index, double value) override;
    double queueParameter (int index, double value) override;

    /** The song's automation of a macro (audio thread): its controls follow in the next block. */
    void setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept override;

    bool activate (double sampleRate, int maxBlockSize) override;
    void deactivate() override;
    bool isActive() const noexcept override                              { return active.load(); }

    /** The device stopped: every plugin inside stops processing too (a container inside passes it on). */
    void stopProcessing() override;
    double getActiveSampleRate() const noexcept                          { return rate; }
    int getActiveBlockSize() const noexcept                              { return blockSize; }

    void process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                  int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept override;

    ScreenKind getScreenKind() const noexcept override                   { return ScreenKind::native; }
    bool showNativeScreen (const juce::String& title) override;
    void hideNativeScreen() override;

    /** Looks after the plugins inside (what WCLAP plugins ask of the host, restarts, failures). */
    Requests takeRequests() noexcept override;

private:
    /** What the tools in the graph see of the song: the block's transport. */
    class GraphPlayHead final : public juce::AudioPlayHead
    {
    public:
        explicit GraphPlayHead (const ContainerPlugin& c) : owner (c) {}
        juce::Optional<PositionInfo> getPosition() const override;

    private:
        const ContainerPlugin& owner;
    };

    void makeIoNodes();
    void placeIoNodes();
    void hideNodeScreens();
    void updateMidiOutUsed();
    juce::AudioProcessorGraph::Node::Ptr nodeFromXml (const juce::XmlElement& xml);

    // Macros
    struct LiveLink
    {
        int macro = 0;
        MacroLink::Kind kind = MacroLink::Kind::parameter;
        Plugin* plugin = nullptr;       // parameter
        PluginNode* node = nullptr;     // level
        uint32_t paramId = 0;
        int hint = 0;
        double minValue = 0.0, maxValue = 1.0;
        bool stepped = false;
        float from = 0.0f, to = 1.0f;
        bool inverted = false;
    };

    /** What the audio thread uses to follow the macros, made again from macroLinks (leaving:
        a node about to go, left out). */
    void rebuildLiveLinks (juce::uint32 leaving = 0);
    void applyMacro (int macro);
    void macrosEdited();
    static double linkAmount (float from, float to, bool inverted, double macro) noexcept;
    static float levelGain (double amount) noexcept;

    const bool effect;
    Descriptor descriptor;
    std::vector<Descriptor> inFile;
    static const std::vector<Parameter> noParameters;

    std::unique_ptr<juce::AudioProcessorGraph> graph;
    juce::AudioProcessorGraph::Node::Ptr audioIn, audioOut, midiIn, midiOut;

    juce::String containerName;
    std::atomic<bool> muted { false }, synced { true }, audioInputOn { true };
    std::atomic<double> tempo { 120.0 };
    std::atomic<int> numerator { 4 }, denominator { 4 };
    std::atomic<bool> active { false }, stateChanged { false }, midiOutUsed { false }, panicOwed { false };
    std::atomic<float> outputGain { 1.0f }, outputLevel { 0.0f };
    float lastOutputGain = 1.0f;                       // audio thread: ramped from here
    juce::StringArray problems;
    GraphPlayHead playHead { *this };

    // Audio
    double rate = 0.0;
    int blockSize = 0;
    juce::AudioBuffer<float> work;
    juce::MidiBuffer workMidi;
    const clap_event_transport_t* blockTransport = nullptr;
    clap_event_transport_t ownTransport {};

    std::unique_ptr<Containers::Screen> screen;

    std::vector<Parameter> macroParameters;
    std::array<juce::String, numMacros> macroNames;
    std::array<std::atomic<double>, numMacros> macroValues {};
    std::vector<MacroLink> macroLinks;
    std::vector<LiveLink> liveLinks;                   // the audio thread's, swapped under liveLock
    juce::SpinLock liveLock;
    std::atomic<juce::uint32> macrosFromAudio { 0 };   // macros the automation moved (a bit each)
    std::atomic<bool> macroNamesChanged { false };     // the host shows the new names

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContainerPlugin)
};
