// D:\workspace\Fanan_web_hub\src\Plugin.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <clap/clap.h>

#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
/**
    A hosted plugin, whatever its format: a WCLAP (a CLAP plugin compiled to WebAssembly),
    a native VST3, a native CLAP, or one made in the Plugins Creator Studio (CreatorPlugin.h). Tracks, insert chains, songs, undo and the page all work with this;
    only loading needs to know the format, and Plugin::load picks it from the file.

    Threads: loading, activating, state and parameters on the message thread; process()
    on the audio thread, while the engine holds its graph lock.
*/
class Plugin
{
public:
    struct Descriptor
    {
        juce::String id, name, vendor, version;
        juce::StringArray features;   // CLAP feature words; VST3 categories are translated to them
    };

    struct Parameter
    {
        uint32_t id = 0;              // stable across sessions: songs keep values by it
        void* cookie = nullptr;       // WCLAP: the plugin's own handle for it
        juce::String name;
        double minValue = 0.0, maxValue = 1.0, defaultValue = 0.0;
        bool stepped = false, hidden = false, readOnly = false;
    };

    enum class Role
    {
        instrument,   // makes sound from notes, replaces the test synth
        effect,       // processes the sound before it
        noAudio       // no audio output: gets notes, the test synth stays audible
    };

    /** What a plugin asked of the host since last time (WCLAP plugins ask; others don't). */
    struct Requests
    {
        bool callback = false, rescan = false, restart = false, flush = false, processError = false;
    };

    virtual ~Plugin() = default;

    //==============================================================================
    /** Loads the plugin with this id from a file: a .vst3, a native .clap, or a WCLAP (.wasm, a
        .wclap bundle folder or a .wclap.tar.gz). An empty id means the file's first plugin. */
    static std::unique_ptr<Plugin> load (const juce::File& file, const juce::String& pluginId, juce::String& error);

    /** Every plugin in a file, for the Browser's library. */
    static std::vector<Descriptor> describe (const juce::File& file, juce::String& error);

    static bool isVst3 (const juce::File& file);

    /** A native CLAP plugin's file: a .clap library, or a .clap bundle folder on a Mac. A WCLAP
        (.wclap) is not one: it's WebAssembly, and WclapPlugin loads it its own way. */
    static bool isClap (const juce::File& file);

    //==============================================================================
    // Plugins loaded straight from their files, by format (the Sampling Studio's nodes):
    // "VST3", "VST2", "CLAP" (native), "AU" (macOS) and "LV2" (Linux).

    /** Loads the plugin with this id (empty: the file's first) from a file of that format. */
    static std::unique_ptr<Plugin> loadFormat (const juce::File& file, const juce::String& format,
                                               const juce::String& pluginId, juce::String& error);

    /** The plugins in a file of that format: an id and a name each. */
    static std::vector<Descriptor> describeFormat (const juce::File& file, const juce::String& format, juce::String& error);

    /** Whether this Sonify hosts that format (VST2 only in builds with the VST2 SDK). */
    static bool isFormatAvailable (const juce::String& format);

    /** The formats this computer has, in the order their buttons show. */
    static juce::StringArray formatsForThisComputer();

    /** Where plugins of that format are installed, as is usual on this computer. */
    static juce::File defaultFolderFor (const juce::String& format);

    /** Whether a file (or a bundle folder) is a plugin of that format, by its name. */
    static bool looksLikePlugin (const juce::File& file, const juce::String& format);

    //==============================================================================
    // Trust: a native plugin (VST3, native CLAP, VST2, AU, LV2) is real code with the user's own
    // rights, so Sonify loads one only from a place the user chose: a file of the Library (scanned
    // or added by hand), a file inside one of its Plugin Folders or the system's plugin folders,
    // or a file the user picked this run. A song, a container, a sampling preset or an AI request
    // naming any other file (a network share, a disk image) loads nothing. WCLAPs and plugins made
    // in Sonify run in their sandbox and need no trust. Until setTrustedPlaces is first called (a
    // scanning copy of Sonify, tests) every file is trusted. Any thread.

    /** The Library's files and folders, every time they change. */
    static void setTrustedPlaces (const juce::StringArray& files, const juce::StringArray& folders);

    /** A file the user picked themselves (a file chooser, a click in a folder list): trusted this run. */
    static void approve (const juce::File& file);

    /** Whether a native plugin's file may load (any file that isn't native code: yes). */
    static bool isTrusted (const juce::File& file);

    /** Why an untrusted file didn't load, for the song's problems. */
    static juce::String untrustedMessage (const juce::File& file);

    /** Whether the song is being rendered faster than real time (a mixdown) rather than played
        to the device: plugins that read files as they play wait for their reads then, instead
        of playing silence. The engine sets it; any thread reads it. */
    static std::atomic<bool>& offlineRendering() noexcept
    {
        static std::atomic<bool> offline { false };
        return offline;
    }

    //==============================================================================
    virtual juce::String getFormatName() const = 0;                 // "WCLAP" or "VST3"
    virtual const Descriptor& getDescriptor() const noexcept = 0;
    virtual const std::vector<Descriptor>& getPluginsInFile() const noexcept = 0;
    virtual const juce::File& getSourceFile() const noexcept = 0;
    virtual Role getRole() const noexcept = 0;
    virtual int getMainInputChannels() const noexcept = 0;
    virtual int getMainOutputChannels() const noexcept = 0;

    // State, for songs.
    virtual bool canSaveState() const noexcept = 0;
    virtual bool saveState (juce::MemoryBlock& destination) = 0;
    virtual bool loadState (const juce::MemoryBlock& source) = 0;
    virtual bool consumeStateChanged() noexcept = 0;               // any thread

    // Parameters.
    virtual const std::vector<Parameter>& getParameters() const noexcept = 0;
    virtual int findParameter (uint32_t id) const noexcept = 0;
    virtual double getParameterValue (int index) const noexcept = 0;
    virtual double readParameterValue (int index) = 0;
    virtual juce::String getParameterText (int index, double value) = 0;
    virtual double queueParameter (int index, double value) = 0;

    /** The parameter (its index) the user last took hold of in the plugin's own window, once:
        -1 when none since the last call. Any thread. MIDI Link's Last Touched slot shows it. */
    virtual int takeTouchedParameter() noexcept                     { return -1; }

    /** Automation, on the audio thread, just before process(): sets the parameter with this
        stable id. indexHint is where it was last time; it's corrected if the plugin changed. */
    virtual void setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept
    {
        juce::ignoreUnused (paramId, indexHint, value);
    }

    // Audio.
    virtual bool activate (double sampleRate, int maxBlockSize) = 0;
    virtual void deactivate() = 0;
    virtual bool isActive() const noexcept = 0;
    virtual void stopProcessing() {}

    /** One block. Inputs may be null (instruments) and outputs may be null (note effects);
        an input may be the same buffer as its output (effects work in place). */
    virtual void process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                          int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept = 0;

    //==============================================================================
    // The plugin's own screen, on the message thread.
    //   web     a page the Hub shows in a window of its own page, fed with the plugin's
    //           files and messages (CLAP's webview extension; WCLAP plugins)
    //   native  a window of its own (VST3 editors), owned by the plugin
    //   none    no screen: the Hub shows its generic controls instead
    enum class ScreenKind { none, web, native };

    virtual ScreenKind getScreenKind() const noexcept                    { return ScreenKind::none; }

    /** Web: where the screen starts (a path the plugin serves itself, or a full URL). */
    virtual juce::String getScreenUri()                                   { return {}; }

    /** Web: one of the screen's files, by absolute path ("/index.html"). */
    virtual bool getScreenResource (const juce::String&, juce::String&, juce::MemoryBlock&) { return false; }

    /** Web: whether the page has the screen open (messages flow only while it does). */
    virtual void setScreenOpen (bool)                                     {}

    /** Web: a message from the screen to the plugin. */
    virtual bool sendToPlugin (const void*, size_t)                       { return false; }

    /** Web: the plugin's messages for its screen since the last call. */
    virtual std::vector<juce::MemoryBlock> takeScreenMessages()           { return {}; }

    /** The screen's preferred size, when the plugin has one. */
    virtual bool getScreenSize (int&, int&)                               { return false; }

    /** Native: shows the plugin's own window (made the first time), or hides it. */
    virtual bool showNativeScreen (const juce::String&)                   { return false; }
    virtual void hideNativeScreen()                                       {}

    /** How late its output is behind its input, in samples (look-ahead, linear phase): the
        engine delays everything else to match (plugin delay compensation). Read on the message
        thread, after activate(). */
    virtual int getLatencySamples() const noexcept                       { return 0; }

    // Upkeep on the message thread: only WCLAP plugins need it.
    virtual Requests takeRequests() noexcept                           { return {}; }
    virtual void runOnMainThread()                                     {}
    virtual void rescanParameters()                                    {}

    /** A rescan in two steps, so the device never waits for it: readParameters() asks the
        plugin for its parameters (message thread, while it may be playing) and
        takeReadParameters() puts what it read in place, a quick swap under the engine's lock,
        giving back the old lists to let go of after the lock. readParameters() false: this
        plugin rescans in one step (rescanParameters()). */
    virtual bool readParameters()                                      { return false; }
    virtual std::shared_ptr<void> takeReadParameters()                 { return {}; }
    virtual void flushQueuedParameters()                               {}
    virtual bool getError (juce::String&) const                        { return false; }

    //==============================================================================
    // Sidechain: a second audio input some plugins have (compressors, duckers), which other
    // tracks can send to. The engine clears and fills it on the audio thread before
    // process(); it stays silent while nothing sends to it.
    virtual bool hasSidechain() const noexcept                     { return false; }

    float* getSidechainChannel (int channel) noexcept
    {
        return channel >= 0 && channel < sidechain.getNumChannels() ? sidechain.getWritePointer (channel) : nullptr;
    }

    int getSidechainCapacity() const noexcept                       { return sidechain.getNumSamples(); }

    void clearSidechain (int numSamples) noexcept
    {
        const auto samples = juce::jmin (numSamples, sidechain.getNumSamples());

        for (int channel = 0; channel < sidechain.getNumChannels(); ++channel)
            juce::FloatVectorOperations::clear (sidechain.getWritePointer (channel), samples);
    }

    //==============================================================================
    // MIDI out: a plugin that plays notes itself (an arpeggiator, a MIDI-out instrument) can
    // drive other tracks' instruments. Its notes from the last block, on the audio thread.
    virtual bool producesMidi() const noexcept                      { return false; }
    const juce::MidiBuffer& getMidiOutput() const noexcept          { return midiOutput; }

protected:
    juce::MidiBuffer midiOutput;   // sized as the plugin activates

    /** Sized as the plugin activates (not on the audio thread), when it has a sidechain. */
    void prepareSidechain (int maxBlockSize)
    {
        sidechain.setSize (2, juce::jmax (1, maxBlockSize), false, true, false);
        sidechain.clear();
    }

    juce::AudioBuffer<float> sidechain;
};
