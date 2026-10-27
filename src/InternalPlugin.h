// D:\workspace\Fanan_web_hub\src\InternalPlugin.h

#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

// With JUCE's GUI in the build (the app), an internal plugin shows its own window; the headless
// host (the engine tests) has none. The same switch Vst3Plugin.h makes, and it defines
// FANAN_PLUGIN_WINDOWS when it's included first.
#if JUCE_MODULE_AVAILABLE_juce_audio_processors && JUCE_MODULE_AVAILABLE_juce_gui_basics
 #include <juce_audio_processors/juce_audio_processors.h>
 #ifndef FANAN_PLUGIN_WINDOWS
  #define FANAN_PLUGIN_WINDOWS 1
 #endif
#else
 #ifndef FANAN_PLUGIN_WINDOWS
  #define FANAN_PLUGIN_WINDOWS 0
 #endif
#endif

#include "Plugin.h"

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    Sonify's own plugins, built into Sonify.exe: Zax (its drum synth), Natura (its modelling
    synth) and AXIS (its guitar board). Each is a juce::AudioProcessor with a window of its own,
    written as a plugin would be, and Sonify hosts it exactly as it hosts a VST3 - the same
    parameters, the same state in songs, the same window over the Hub - only made in Sonify
    instead of loaded from a file. Nothing is installed, and no plugin file exists: the Browser
    lists them under Sonify.exe with the other built-ins, by ids starting with Builtins::idPrefix.

    Internals::entries() is the list of them, each with how to make it. Builtins::describe(),
    Builtins::create() and Builtins::categoryOf() pass these ids on to here, so everything else in
    Sonify (tracks, inserts, songs, undo, Fanan AI, the page) works with them as with any plugin.

    Audio goes through one buffer sized at activation, so process() never allocates. Parameters
    run from 0 to 1, as a VST3's do, and keep their own ids, so songs find them again.

    Threads: making, activating, state and parameters on the message thread; process() on the
    audio thread, while the engine holds its graph lock.
*/
class InternalPlugin final : public Plugin,
                            private juce::AudioProcessorListener
{
public:
    ~InternalPlugin() override;

    /** Makes one around a processor of Sonify's own. */
    static std::unique_ptr<InternalPlugin> make (std::unique_ptr<juce::AudioProcessor> processor,
                                                 Descriptor descriptor, Role role, const juce::File& file);

    juce::String getFormatName() const override                          { return "Sonify"; }
    const Descriptor& getDescriptor() const noexcept override            { return descriptor; }
    const std::vector<Descriptor>& getPluginsInFile() const noexcept override { return pluginsInFile; }
    const juce::File& getSourceFile() const noexcept override            { return sourceFile; }
    Role getRole() const noexcept override                               { return role; }
    int getMainInputChannels() const noexcept override                   { return inputChannels; }
    bool hasSidechain() const noexcept override                          { return sidechainChannels > 0; }
    bool producesMidi() const noexcept override                          { return makesMidi; }
    int getMainOutputChannels() const noexcept override                  { return outputChannels; }

    bool canSaveState() const noexcept override                          { return true; }
    bool saveState (juce::MemoryBlock& destination) override;
    bool loadState (const juce::MemoryBlock& source) override;
    bool consumeStateChanged() noexcept override                         { return stateChanged.exchange (false); }

    int takeTouchedParameter() noexcept override                         { return touchedParameter.exchange (-1); }

    const std::vector<Parameter>& getParameters() const noexcept override { return parameters; }
    int findParameter (uint32_t id) const noexcept override;
    double getParameterValue (int index) const noexcept override;
    double readParameterValue (int index) override                       { return getParameterValue (index); }
    juce::String getParameterText (int index, double value) override;
    double queueParameter (int index, double value) override;
    void setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept override;

    bool activate (double sampleRate, int maxBlockSize) override;
    int getLatencySamples() const noexcept override;
    void deactivate() override;
    bool isActive() const noexcept override                              { return active.load(); }

    void process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                  int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept override;

    ScreenKind getScreenKind() const noexcept override;
    bool showNativeScreen (const juce::String& title) override;
    void hideNativeScreen() override;

private:
   #if FANAN_PLUGIN_WINDOWS
    class ScreenWindow;
    std::unique_ptr<ScreenWindow> screenWindow;   // before processor: it goes first
   #endif

    InternalPlugin (std::unique_ptr<juce::AudioProcessor> processor, Descriptor descriptor,
                    Role role, const juce::File& file);

    /** The song's position, tempo and loop, as the plugin asks for them. A copy of the block's
        transport: a plugin may ask for it outside processBlock too. */
    struct PlayHead final : juce::AudioPlayHead
    {
        clap_event_transport_t transport {};
        bool hasTransport = false;
        double sampleRate = 48000.0;
        juce::Optional<PositionInfo> getPosition() const override;
    };

    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override    { stateChanged.store (true); }
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override   { stateChanged.store (true); }

    // The user took hold of a knob in the plugin's window (Sonify's own changes come without this).
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override { touchedParameter.store (index); }

    std::unique_ptr<juce::AudioProcessor> processor;
    Descriptor descriptor;
    std::vector<Descriptor> pluginsInFile;
    juce::File sourceFile;
    Role role = Role::effect;

    std::vector<Parameter> parameters;
    std::vector<juce::AudioProcessorParameter*> hosted;

    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midiBuffer;
    PlayHead playHead;
    std::atomic<bool> active { false }, stateChanged { false };
    std::atomic<int> touchedParameter { -1 };
    double activeRate = 0.0;
    int activeBlockSize = 0;
    int inputChannels = 0, outputChannels = 0;   // inputChannels: the main bus
    int totalInputChannels = 0;                  // with the sidechain's
    int sidechainOffset = -1, sidechainChannels = 0;
    bool makesMidi = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InternalPlugin)
};

//==============================================================================
/**
    Which plugins of Sonify's own there are. One entry each: what the Browser shows, and how to
    make it. A new one is added here and nowhere else - Builtins asks this list.
*/
namespace Internals
{
    struct Entry
    {
        juce::String id;                 // "sonify.builtin.zax": Builtins::idPrefix and its name
        juce::String name, vendor, version;
        juce::String category;           // the Browser's Type ("Synth", "Drums", "Guitar")
        juce::StringArray features;       // CLAP feature words, as the other plugins' are
        Plugin::Role role = Plugin::Role::effect;
        std::function<std::unique_ptr<juce::AudioProcessor>()> make;
    };

    /** Every plugin of Sonify's own, in the order the Browser lists them. */
    const std::vector<Entry>& entries();

    /** One by its id, or none: this id isn't one of Sonify's own plugins. */
    const Entry* find (const juce::String& pluginId);

    /** All of them for the Browser's library. */
    std::vector<Plugin::Descriptor> describe();

    /** Makes the one with this id. file: what the Browser and songs list them under
        (Builtins::file(), Sonify.exe itself). */
    std::unique_ptr<Plugin> create (const juce::String& pluginId, const juce::File& file, juce::String& error);
}
