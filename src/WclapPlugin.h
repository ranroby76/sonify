// D:\workspace\Fanan_web_hub\src\WclapPlugin.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <clap/clap.h>
#include <clap/ext/draft/webview.h>   // web screens

#include "Plugin.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
/**
    Hosts one WCLAP plugin (a CLAP plugin compiled to WebAssembly), or a native CLAP plugin
    (loadNative: a .clap library, a bundle on macOS, as the Sampling Studio's nodes load them).

    wclap-bridge loads the WebAssembly module through Wasmtime and hands back an
    ordinary CLAP factory, so everything here is plain CLAP hosting. A native plugin's factory
    comes from its library's clap_entry; its own window (CLAP's gui extension) opens in a window
    of Sonify's.

    Threads:
      - load, activate, deactivate, parameters and the destructor: message thread
      - process(): audio thread
    The owner (AudioEngine) guarantees the audio thread is never inside process()
    while the message thread activates, deactivates, rescans or destroys.
*/
class WclapPlugin final : public Plugin
{
public:




    //==============================================================================
    /** Loads a .wasm file, a .wclap.tar.gz bundle, or a .wclap folder (pick its module.wasm).
        An empty pluginId picks the first plugin in the file. Returns nullptr and fills
        error on failure. Message thread.
    */
    static std::unique_ptr<WclapPlugin> load (const juce::File& file, const juce::String& pluginId, juce::String& error);

    /** Loads a native CLAP plugin: a .clap file (Windows, Linux) or bundle (macOS). An empty
        pluginId picks the first plugin in it. Message thread. */
    static std::unique_ptr<WclapPlugin> loadNative (const juce::File& file, const juce::String& pluginId, juce::String& error);

    /** A native plugin's library, shared by every plugin loaded from it (clap_entry's init once,
        deinit when the last one goes). */
    struct NativeModule;

    /** Stops the WebAssembly engine. Call once at shutdown, after every plugin is gone. */
    static void shutdownEngine();

    juce::String getFormatName() const override                   { return nativeModule != nullptr ? "CLAP" : "WCLAP"; }

    // Web screens (CLAP's webview extension, carried through the bridge); a native plugin's own
    // window (CLAP's gui extension).
    ScreenKind getScreenKind() const noexcept override
    {
        return webviewExt != nullptr ? ScreenKind::web : nativeGui ? ScreenKind::native : ScreenKind::none;
    }

    bool showNativeScreen (const juce::String& title) override;
    void hideNativeScreen() override;
    juce::String getScreenUri() override;
    bool getScreenResource (const juce::String& path, juce::String& mime, juce::MemoryBlock& data) override;
    void setScreenOpen (bool open) override;
    bool sendToPlugin (const void* data, size_t size) override;
    std::vector<juce::MemoryBlock> takeScreenMessages() override;
    bool getScreenSize (int& width, int& height) override;

    ~WclapPlugin();

    //==============================================================================
    const Descriptor& getDescriptor() const noexcept override               { return descriptor; }
    const std::vector<Descriptor>& getPluginsInFile() const noexcept override { return pluginsInFile; }
    const juce::File& getSourceFile() const noexcept override               { return sourceFile; }

    //==============================================================================
    // Saving and restoring (songs), message thread.

    /** True when the plugin can save its whole state (the CLAP state extension). */
    bool canSaveState() const noexcept override                             { return stateExt != nullptr; }

    /** The plugin's own state, in its own format. False when it can't save or saving failed. */
    bool saveState (juce::MemoryBlock& destination) override;

    /** Hands a state from saveState() back to the plugin. */
    bool loadState (const juce::MemoryBlock& source) override;

    /** The index of the parameter with this CLAP id, or -1. */
    int findParameter (clap_id id) const noexcept override;

    /** A parameter's value as the plugin itself has it now (which may be rounded to its own
        precision), rather than the last value sent to it. For saving; message thread. */
    double readParameterValue (int index) override;

    /** True once after the plugin reports a change to its state. Any thread. */
    bool consumeStateChanged() noexcept override                            { return stateChanged.exchange (false); }

    /** The parameter the plugin's screen last changed (it says so with a gesture or a value it
        sends out: Sonify's own changes go in, never out). Any thread. */
    int takeTouchedParameter() noexcept override                            { return touchedParameter.exchange (-1); }
    Role getRole() const noexcept override                                  { return role; }
    int getMainInputChannels() const noexcept override                      { return mainInputChannels; }
    int getMainOutputChannels() const noexcept override                     { return mainOutputChannels; }

    //==============================================================================
    // Message thread
    bool activate (double sampleRate, int maxBlockSize) override;
    bool hasSidechain() const noexcept override                             { return sidechainPort >= 0; }
    bool producesMidi() const noexcept override                             { return hasNoteOutput; }
    void deactivate() override;
    bool isActive() const noexcept override                                 { return active.load(); }
    void stopProcessing() override;

    const std::vector<Parameter>& getParameters() const noexcept override   { return parameters; }
    double getParameterValue (int index) const noexcept override;
    juce::String getParameterText (int index, double value) override;

    /** Queues a change for the next process() call. Returns the value actually used. */
    double queueParameter (int index, double value) override;
    void setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept override;

    /** Delivers queued changes with params.flush(). Only while nothing is processing. */
    void flushQueuedParameters() override;

    Requests takeRequests() noexcept override;
    void runOnMainThread() override;
    void rescanParameters() override;
    int getLatencySamples() const noexcept override                     { return latency.load (std::memory_order_relaxed); }
    bool readParameters() override;
    std::shared_ptr<void> takeReadParameters() override;
    bool getError (juce::String& message) const override;

    //==============================================================================
    // Audio thread. Inputs may be null (silence); outputs may be null (discarded).
    // The transport describes the song at the start of the block (may be null).
    void process (const float* inLeft, const float* inRight,
                  float* outLeft, float* outRight,
                  int numSamples, const juce::MidiBuffer& midi,
                  const clap_event_transport_t* transport) noexcept override;

private:
    //==============================================================================
    union EventSlot
    {
        clap_event_header_t header;
        clap_event_note_t note;
        clap_event_midi_t midi;
        clap_event_param_value_t param;
    };

    struct Port
    {
        int channels = 0;
        std::vector<std::vector<float>> storage;
        std::vector<float*> pointers;
    };

    struct ParamChange
    {
        int index = 0;
        double value = 0.0;
    };

    WclapPlugin() = default;

    bool initialise (const juce::File& location, const juce::String& pluginId, juce::String& error);
    bool createFromFactory (const juce::String& pluginId, const juce::String& displayName, juce::String& error);
    void scanPorts();
    void allocateBuffers();
    void buildInputEvents (const juce::MidiBuffer& midi, int numFrames) noexcept;
    void addParameterEvents() noexcept;
    bool pushEvent (const EventSlot& slot) noexcept;
    void addNoteEvent (uint16_t type, int time, int channel, int key, double velocity) noexcept;
    void releaseHeldNotes (int time, int channel) noexcept;
    int findParameterIndex (clap_id id) const noexcept;

    // clap_host_t
    static const void* CLAP_ABI hostGetExtension (const clap_host_t*, const char* extensionId);
    static void CLAP_ABI hostRequestRestart (const clap_host_t*);
    static void CLAP_ABI hostRequestProcess (const clap_host_t*);
    static void CLAP_ABI hostRequestCallback (const clap_host_t*);
    static void CLAP_ABI hostLog (const clap_host_t*, clap_log_severity, const char* message);
    static void CLAP_ABI hostParamsRescan (const clap_host_t*, clap_param_rescan_flags);
    static void CLAP_ABI hostParamsClear (const clap_host_t*, clap_id, clap_param_clear_flags);
    static void CLAP_ABI hostParamsRequestFlush (const clap_host_t*);
    static uint32_t CLAP_ABI hostNotePortsDialects (const clap_host_t*);
    static void CLAP_ABI hostNotePortsRescan (const clap_host_t*, uint32_t);
    static void CLAP_ABI hostStateMarkDirty (const clap_host_t*);
    static void CLAP_ABI hostLatencyChanged (const clap_host_t*);

    // A native plugin's: its window (clap_host_gui_t), and which thread is which.
    static void CLAP_ABI hostGuiResizeHintsChanged (const clap_host_t*);
    static bool CLAP_ABI hostGuiRequestResize (const clap_host_t*, uint32_t width, uint32_t height);
    static bool CLAP_ABI hostGuiRequestShow (const clap_host_t*);
    static bool CLAP_ABI hostGuiRequestHide (const clap_host_t*);
    static void CLAP_ABI hostGuiClosed (const clap_host_t*, bool wasDestroyed);
    static bool CLAP_ABI hostIsMainThread (const clap_host_t*);
    static bool CLAP_ABI hostIsAudioThread (const clap_host_t*);

    // clap_input_events_t / clap_output_events_t
    static uint32_t CLAP_ABI inputEventsSize (const clap_input_events_t*);
    static const clap_event_header_t* CLAP_ABI inputEventsGet (const clap_input_events_t*, uint32_t index);
    static bool CLAP_ABI outputEventsTryPush (const clap_output_events_t*, const clap_event_header_t*);

    static WclapPlugin& fromHost (const clap_host_t* clapHost) noexcept { return *static_cast<WclapPlugin*> (clapHost->host_data); }

    //==============================================================================
    static constexpr int maxEvents = 1024;
    static constexpr int paramQueueSize = 512;

    juce::File sourceFile;
    void* module = nullptr;

    std::shared_ptr<NativeModule> nativeModule;   // a native plugin's library
    const clap_plugin_factory_t* factory = nullptr;
    const clap_plugin_t* plugin = nullptr;
    bool initialised = false;
    clap_host_t host {};

    const clap_plugin_params_t* paramsExt = nullptr;
    const clap_plugin_latency_t* latencyExt = nullptr;
    std::atomic<int> latency { 0 };   // samples, read as it starts (plugin delay compensation)
    const clap_plugin_state_t* stateExt = nullptr;
    std::atomic<bool> stateChanged { false };
    std::atomic<int> touchedParameter { -1 };
    const clap_plugin_audio_ports_t* audioPortsExt = nullptr;
    const clap_plugin_note_ports_t* notePortsExt = nullptr;

    Descriptor descriptor;
    std::vector<Descriptor> pluginsInFile;
    Role role = Role::instrument;

    std::vector<Port> inputPorts, outputPorts;
    std::vector<clap_audio_buffer_t> inputBuffers, outputBuffers;
    int mainInputPort = -1, mainOutputPort = -1;
    int sidechainPort = -1;   // the first input port besides the main one
    int mainInputChannels = 0, mainOutputChannels = 0;

    bool hasNoteInput = false, sendClapNotes = true, sendMidi = false;
    bool hasNoteOutput = false;   // it plays notes itself (an arpeggiator)
    bool inProcess = false;       // audio thread: notes it puts out now belong to this block
    int notePortIndex = 0;
    std::array<std::array<juce::uint64, 2>, 16> heldNotes {};

    std::vector<Parameter> parameters;
    std::vector<std::pair<clap_id, int>> parameterLookup;     // sorted by id
    std::unique_ptr<std::atomic<double>[]> parameterValues;

    /** Parameters read, not yet in place (readParameters / takeReadParameters). */
    struct ParameterLists
    {
        std::vector<Parameter> parameters;
        std::vector<std::pair<clap_id, int>> lookup;
        std::unique_ptr<std::atomic<double>[]> values;
    };

    std::shared_ptr<ParameterLists> readLists;
    std::shared_ptr<ParameterLists> scanParameters() const;

    juce::AbstractFifo paramFifo { paramQueueSize };
    std::array<ParamChange, (size_t) paramQueueSize> paramQueue {};

    std::vector<EventSlot> events;
    int numEvents = 0;
    clap_input_events_t inputEvents {};
    clap_output_events_t outputEvents {};

    std::atomic<bool> active { false }, processing { false };
    double currentSampleRate = 0.0;
    int maxFrames = 0;
    int64_t steadyTime = 0;

    std::atomic<bool> callbackRequested { false }, rescanRequested { false }, restartRequested { false },
                      flushRequested { false }, processErrorSeen { false };

    // Automation's changes for the next process(): set and used on the audio thread only.
    static constexpr int automationQueueSize = 256;
    std::array<ParamChange, (size_t) automationQueueSize> automationChanges {};
    int numAutomationChanges = 0;
    void addParameterEvent (int index, double value) noexcept;

    // The web screen: the plugin's side of it, and what it has said to the page.
    const clap_plugin_webview_t* webviewExt = nullptr;
    const clap_plugin_gui_t* guiExt = nullptr;

    // A native plugin's own window (message thread).
    class NativeScreen;
    std::unique_ptr<NativeScreen> nativeScreen;
    bool nativeGui = false;        // it has a window on this computer's kind of window
    bool guiCreated = false;
    double guiScale = 1.0;         // Windows: its pixels to Sonify's
    void destroyNativeScreen();
    void nativeScreenResized (int width, int height);
    std::atomic<bool> screenOpen { false };
    juce::CriticalSection screenLock;
    std::vector<juce::MemoryBlock> screenMessages;
    static bool CLAP_ABI hostWebviewSend (const clap_host_t* host, const void* buffer, uint32_t size);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WclapPlugin)
};
