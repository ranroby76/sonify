// D:\workspace\Fanan_web_hub\src\CreatorPlugin.h

#pragma once

#include "Licences.h"
#include "Plugin.h"
#include "PluginLibrary.h"
#include "PresetLibrary.h"
#include "WasmPlugin.h"
#include "stella_licence.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

//==============================================================================
/**
    Plugins made in the Plugins Creator Studio, as Sonify's own: Add to Sonify puts the one that
    plays into a folder of its own here (no VST3 or CLAP), the Browser lists it under
    "Made in Sonify", and tracks play it like Sonify's system tools, with its own window: the
    panel designed in Edit UI.

    A made plugin is one file, "<name>.sip", in <user data>\Plugins (SipFiles.h: with every other
    Sonify plugin, MIDI FX too), holding sip.json (its kind: instrument or fx) and these files,
    which it plays from once unpacked (Sip::unpacked). Before, it was a folder of them,
    "<name>.sonifyplugin" (the Store's packages still carry one):
        plugin.json      its name, company, kind, id ("sonify.made." and the project's id) and version
        plugin.wasm      the plugin, as built (it runs in its sandbox, as in the studio)
        description.json what the plugin says of itself: its parameters, modules and displays
        elements.wasm    its panel's programmed elements, when it has some
        presets.json     its factory presets
        library.json     a sampler's sample library: its blobs by name (PluginLibrary), when it has one
        gui              its panel: layout.json, its pictures and its own looks
        license.soni     a protected one's product and serial formula (stella_licence.h)
        snapshot.png     a picture of its panel, as it opens, for the Browser (made by Sonify
                         whenever it's missing or older than the panel; one newer than the rest of
                         the folder, a maker's own, is kept)
    A protected plugin is built with its product's id and demo inside (WasmCompiler's
    Protection): until it's registered on this computer (its Registration window, from the
    registration badge on its panel or Register under it), it plays its demo.
    Adding the same project again updates it in place (songs keep finding it); tracks that
    already play it get the new version when their song opens again.

    The user's own presets aren't in it: they're on the hard drive, by the plugin's name
    (PresetLibrary), shared with the studio and every track that plays it.
*/
namespace MadePlugins
{
    inline constexpr const char* extension = ".sip";                 // SipFiles.h
    inline constexpr const char* folderExtension = ".sonifyplugin";   // the folder it was before, and the Store's package
    inline constexpr const char* idPrefix = "sonify.made.";
    inline constexpr const char* category = "Made in Sonify";
    inline constexpr const char* formatName = "Sonify";
    inline constexpr const char* folderLabel = "Sonify Plugins";

    /** <user data>\Plugins: where they're kept, as .sip files (Sip::folder). */
    juce::File folder();

    /** The song's tempo and position as the plugins hear them (tempo sync); none: 120, stopped. */
    WasmPlugin::Transport transportOf (const clap_event_transport_t* song) noexcept;

    /** Whether a plugin file is a made plugin's: a .sip (or an old .sonifyplugin folder). */
    bool isMadeFile (const juce::String& path);

    /** Every made plugin's .sip (instruments and effects; MIDI FX are DeviceFiles'). */
    juce::Array<juce::File> installed();

    /** The plugin in a made plugin's .sip or folder (none if it can't be read). */
    std::vector<Plugin::Descriptor> describe (const juce::File& pluginFile);

    /** The file a song or a container named, as it is now: an old .sonifyplugin folder that was
        turned into a .sip (or any made plugin's file that's gone) is found again by its id. */
    juce::File resolve (const juce::File& file, const juce::String& pluginId);

    /** What Add to Sonify takes from the studio. */
    struct Package
    {
        juce::String projectId, name, vendor;
        juce::String kind { "instrument" };      // "instrument", "effect" or "widget" (Project::kindToString)
        std::string description;                  // the playing plugin's own (WasmPlugin::getDescription)
        std::vector<std::uint8_t> wasm, elementsWasm;
        juce::File guiFolder;                     // copied, without the studio's view of it
        juce::File presetsFile;                   // copied when there is one
        juce::Array<PluginLibrary::Instrument> library;   // a sampler's blobs (they aren't copied)
        juce::MemoryBlock licence;                // a protected plugin's license.soni (empty: not protected)
        bool forOthers = false;                   // for other computers (the Store's download): its blobs by name only, never where they are here
    };

    /** Writes it: over the .sip this project was added to before (replaced: true), or a new one. */
    juce::Result install (const Package& package, juce::File& installedFolder, bool& replaced);

    /** Writes a made plugin's folder as install does, into target (made or emptied first): the
        Store's download is this folder, zipped. license.soni is written when the package has one
        (and an old one taken away when it hasn't). */
    juce::Result writeFolder (const Package& package, const juce::File& target, int version);

    /** Makes a package a protected product's: built again from the project with its product's id
        and demo inside (WasmCompiler), checked to start and to carry the product, with its
        license.soni. Takes a few seconds: any thread (not the audio thread). */
    juce::Result protect (Package& package, const juce::File& projectFolder, const Licences::ProjectProtection& protection,
                          const std::atomic<bool>* cancelled = nullptr);

    /** The made plugin's .sip with this id ("sonify.made." ...), or none. */
    juce::File findInstalled (const juce::String& id);

    /** Its version in plugin.json (0: not installed). */
    int installedVersion (const juce::String& id);

    /** A made plugin's folder from the store (unpacked from its package, plugin.json already
        checked) put into the library as a .sip: over the one with the same id (replaced: true),
        or a new one named after it. */
    juce::Result installUnpacked (const juce::File& unpacked, const juce::String& id, juce::File& installedFolder, bool& replaced);

    /** Its panel's picture (snapshot.png among its files, unpacked; it may not be there yet). */
    inline constexpr const char* snapshotFileName = "snapshot.png";
    inline constexpr int snapshotWidth = 480;     // at most, in pixels
    juce::File snapshotOf (const juce::File& pluginFile);

    /** Missing, or older than anything else of it (its panel, presets or plugin changed). */
    bool snapshotDue (const juce::File& pluginFile);

    /** Draws its panel as it opens (its first preset), at most snapshotWidth wide, into its
        snapshot.png (with its unpacked files). Message thread. */
    juce::Result makeSnapshot (const juce::File& pluginFile);

    /** The id a store product's plugin has in Sonify. */
    inline juce::String storeId (const juce::String& product)    { return juce::String (idPrefix) + "store-" + product; }
}

//==============================================================================
/** A made plugin on a track: Sonify's Plugin around the sandboxed plugin (WasmPlugin), with its
    panel as its own window. Its presets are a modern preset manager's: the plugin's factory
    presets, the user's own, banks. A new one starts on its first preset; a song brings back the
    preset it was left on, with its values. */
class CreatorPlugin final : public Plugin
{
public:
    /** From a .sip (unpacked first) or an old .sonifyplugin folder. */
    static std::unique_ptr<Plugin> load (const juce::File& pluginFile, juce::String& error);
    ~CreatorPlugin() override;

    juce::String getFormatName() const override                          { return MadePlugins::formatName; }
    const Descriptor& getDescriptor() const noexcept override            { return descriptor; }
    const std::vector<Descriptor>& getPluginsInFile() const noexcept override { return inFile; }
    const juce::File& getSourceFile() const noexcept override            { return sourceFile; }
    Role getRole() const noexcept override                               { return instrument ? Role::instrument : Role::effect; }
    int getMainInputChannels() const noexcept override                   { return instrument ? 0 : 2; }
    int getMainOutputChannels() const noexcept override                  { return 2; }

    bool canSaveState() const noexcept override                          { return true; }
    bool saveState (juce::MemoryBlock& destination) override;
    bool loadState (const juce::MemoryBlock& source) override;
    bool consumeStateChanged() noexcept override                         { return stateChanged.exchange (false); }

    const std::vector<Parameter>& getParameters() const noexcept override { return parameters; }
    int findParameter (uint32_t id) const noexcept override;
    double getParameterValue (int index) const noexcept override;
    double readParameterValue (int index) override                       { return getParameterValue (index); }
    juce::String getParameterText (int index, double value) override;
    double queueParameter (int index, double value) override;
    int takeTouchedParameter() noexcept override                         { return touched.exchange (-1); }
    void setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept override;

    bool activate (double sampleRate, int maxBlockSize) override;
    void deactivate() override;
    bool isActive() const noexcept override                              { return active; }

    void process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                  int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept override;

    ScreenKind getScreenKind() const noexcept override                   { return ScreenKind::native; }
    bool showNativeScreen (const juce::String& title) override;
    void hideNativeScreen() override;

    Requests takeRequests() noexcept override;
    bool getError (juce::String& error) const override;

    /** Protected: it plays its demo until it's registered on this computer. */
    bool isProtected() const noexcept                                    { return licensed != nullptr; }

    /** Protected, and not registered on this computer: it plays its demo. */
    bool isInDemo() const noexcept;

    /** Its Registration window (Licences::showRegistration), for its license.soni. */
    void showRegistration();

    /** A sampler's instruments whose blobs aren't on this computer (PluginLibrary says where
        they're looked for); Locate... adds the folder they're in to the places it looks. */
    juce::StringArray getMissingInstruments() const;
    void locateLibrary (const juce::File& folder);

    /** Its panel as its window opens (its values now), at most maxWidth wide; none without a
        panel. Message thread. */
    juce::Image drawPanel (int maxWidth);

private:
    CreatorPlugin() = default;

    struct Info
    {
        int index = 0;                 // the plugin's own parameter number
        juce::String id, name, module, unit;
        float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
    };

    struct ModuleInfo
    {
        int index = 0;
        juce::String id;
        juce::StringArray outputs;
    };

    struct DisplayInfo
    {
        int index = 0;
        juce::String id;
    };

    class Screen;
    friend class Screen;

    void findLibrary();   // each instrument's blob, as it's found now
    void setValue (int index, float value, bool fromScreen);
    void applyPreset (const Preset& preset, bool byUser);   // byUser: the song has changed
    std::map<juce::String, float> currentValues() const;
    int indexOfPluginParameter (int pluginIndex) const noexcept;
    bool resolve (const juce::String& source, int& moduleIndex, int& port) const;
    void setScreenSources (const juce::StringArray& levels, const juce::StringArray& scopes);
    void applyScreenSources();
    float takeLevel (const juce::String& source, bool rms);
    void readScope (const juce::String& source, float* destination, int numSamples) const;

    Descriptor descriptor;
    std::vector<Descriptor> inFile;
    juce::File sourceFile;      // its .sip (or old folder): what songs keep
    juce::File sourceFolder;    // its files, unpacked: what it plays from
    bool instrument = true;

    std::vector<std::uint8_t> wasmBytes;
    juce::Array<PluginLibrary::Instrument> library;      // a sampler's blobs (library.json)
    std::vector<stella::blob::Entry> libraryEntries;     // ...and where each was found
    mutable juce::CriticalSection libraryLock;           // libraryEntries: activate() may run on the device's thread
    std::vector<Info> infos;                      // as the plugin described them, in the host's order
    std::vector<Parameter> parameters;
    std::unique_ptr<std::atomic<float>[]> values; // the host's copy, so values hold while inactive
    std::vector<ModuleInfo> modules;
    std::vector<DisplayInfo> displays;

    // The playing instance: replaced only on the message thread, under the lock; the audio
    // thread plays it while it can take the lock (a block during a swap is silent).
    juce::SpinLock wasmLock;
    std::unique_ptr<WasmPlugin> wasm;
    bool active = false;
    std::atomic<double> activeRate { 0.0 };      // the audio thread reads it too (the demo's timing)
    int activeBlockSize = 0;
    juce::String failure;                         // why it couldn't start
    bool failureReported = false;

    // The panel's keyboard plays it too (and lights up with every note it plays).
    juce::MidiKeyboardState keyboardState;
    juce::MidiBuffer playedMidi;                  // sized as it activates
    static constexpr int maxEvents = 1024;
    std::array<WasmPlugin::MidiEvent, maxEvents> events {};

    // Its presets (message thread): the chosen one is remembered with the song.
    PresetLibrary presets;

    // What the panel's meters and scopes watch (message thread).
    juce::StringArray levelSources, scopeSources, tappedSources;

    // Its protection: its license.soni (from its folder), the product (the id built into its
    // WebAssembly first: its files can be changed, its build can't), and its registration's flag
    // on this computer (null: not protected).
    stella::licence::Soni soni;
    juce::String productId;
    bool protectedBuild = false;                       // its WebAssembly plays the demo itself (stella_licence.h)
    std::shared_ptr<std::atomic<bool>> licensed;
    std::uint32_t licenceToken = 0;                    // what its WebAssembly expects while registered
    stella::licence::DemoGate demoGate;                // a protected plugin whose WebAssembly doesn't carry the demo

    std::unique_ptr<Screen> screen;

    std::atomic<bool> stateChanged { false };
    std::atomic<int> touched { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CreatorPlugin)
};
