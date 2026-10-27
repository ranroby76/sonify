// D:\workspace\Fanan_web_hub\src\Vst3Plugin.h

#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

// With JUCE's GUI in the build (the app), VST3 plugins can show their own windows; the
// headless host (the engine tests) has none.
#if JUCE_MODULE_AVAILABLE_juce_audio_processors && JUCE_MODULE_AVAILABLE_juce_gui_basics
 #include <juce_audio_processors/juce_audio_processors.h>
 #define FANAN_PLUGIN_WINDOWS 1
#else
 #define FANAN_PLUGIN_WINDOWS 0
#endif

#include "Plugin.h"

#include <atomic>

//==============================================================================
/**
    A native VST3 plugin, hosted through JUCE's VST3 host (with its own window in the app).
    The Sampling Studio's nodes load the other formats JUCE hosts the same way (loadAs): VST2
    (in builds with the VST2 SDK), Audio Units on macOS, LV2 on Linux.

    Audio goes through one buffer sized at activation, so process() never allocates.
    VST3 parameters run from 0 to 1 and keep their VST3 ids, so songs find them again.
    VST3 categories ("Fx|Reverb", "Instrument|Synth") become the same feature words
    WCLAP plugins use, so the Browser sorts both kinds alike.
*/
class Vst3Plugin final : public Plugin,
                         private juce::AudioProcessorListener
{
public:
    static std::unique_ptr<Vst3Plugin> load (const juce::File& file, const juce::String& pluginId, juce::String& error);
    static std::vector<Descriptor> describe (const juce::File& file, juce::String& error);

    /** A plugin of another format JUCE hosts: juceFormat is JUCE's name for it ("VST", "AudioUnit",
        "LV2"), shownFormat Sonify's ("VST2", "AU", "LV2"). */
    static std::unique_ptr<Vst3Plugin> loadAs (const juce::File& file, const juce::String& juceFormat, const juce::String& shownFormat,
                                               const juce::String& pluginId, juce::String& error);
    static std::vector<Descriptor> describeAs (const juce::File& file, const juce::String& juceFormat, const juce::String& shownFormat,
                                               juce::String& error);

    /** Where VST3 plugins are installed (on Windows, Program Files\Common Files\VST3). */
    static juce::FileSearchPath getDefaultFolders();

    /** Lets go of JUCE's VST3 host. Called last at shutdown, when no plugin is left. */
    static void shutdownFormats();

    ~Vst3Plugin() override;

    juce::String getFormatName() const override                          { return formatName; }
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

    /** Its settings as a .vstpreset (its class id inside), as other hosts read them (Export
        DAWproject). Empty when it isn't a VST3 or can't give them. Message thread. */
    juce::MemoryBlock getVst3Preset();

    /** Takes settings from a .vstpreset (one made for this plugin, by any host). False when it
        isn't a VST3, or the preset isn't its own. Message thread. */
    bool setVst3Preset (const juce::MemoryBlock& preset);
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
    std::unique_ptr<ScreenWindow> screenWindow;   // before instance: it goes first
   #endif

    Vst3Plugin (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::PluginDescription& description,
                std::vector<Descriptor> pluginsInFile, const juce::File& file);

    /** The song's position, tempo and loop, as the plugin asks for them. A copy of the block's
        transport: some formats (Audio Units) ask outside processBlock too, from other threads. */
    struct PlayHead final : juce::AudioPlayHead
    {
        clap_event_transport_t transport {};
        bool hasTransport = false;
        double sampleRate = 48000.0;   // VST3 wants the position in samples too
        juce::Optional<PositionInfo> getPosition() const override;
    };

    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override   { stateChanged.store (true); }
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override    { stateChanged.store (true); }

    // The user took hold of a knob in the plugin's window (Sonify's own changes come without this).
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override { touchedParameter.store (index); }

    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::String formatName { "VST3" };
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vst3Plugin)
};
