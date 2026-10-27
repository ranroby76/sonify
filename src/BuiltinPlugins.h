// D:\workspace\Fanan_web_hub\src\BuiltinPlugins.h

#pragma once

#include "Plugin.h"
#include "SonifyDsp.h"

#include <array>
#include <cmath>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
/**
    Sonify's own effects, instruments and system tools, built into the executable: Sonify EQ,
    Sonify Compressor, Sonify Limiter and Sonify Mastering (what the Mastering Assistant sets up
    on Main); Grex's effects (Chorus, Phaser, Auto-Wah, Reverb, Delay, Sweetener, Ducker:
    BuiltinEffects.cpp); the Test Synth (TestSynth.cpp); and the system tools: Container and FX
    Container (Container.h), the Playlist Player (PlaylistPlayer.h) and the Sonify Sampler
    (SonifySampler.h). And Sonify's own plugins, each written the way a plugin is and hosted as a
    VST3 is, only made in Sonify (InternalPlugin.h): Natura, its modelling synth, and Zax, its drum
    synth. Always there, whatever plugins the user has, so the Browser offers them and Sonify AI
    can use them.

    They're named by their plugin ids (all starting with idPrefix); the executable stands in
    as their file in the Browser and in songs, and a song finds them wherever Sonify is.
    The effects' parameters are in their own units (dB, Hz, ms). Each has a window of its own:
    the effects and the Test Synth a designed panel (BuiltinScreen.cpp draws it with the Plugins
    Creator's panel renderer, from panel_<name>.json), the system tools their own windows.
*/
namespace Builtins
{
    inline constexpr const char* idPrefix = "sonify.builtin.";
    inline constexpr const char* eqId = "sonify.builtin.eq";
    inline constexpr const char* compressorId = "sonify.builtin.compressor";
    inline constexpr const char* limiterId = "sonify.builtin.limiter";
    inline constexpr const char* masteringId = "sonify.builtin.mastering";
    inline constexpr const char* chorusId = "sonify.builtin.chorus";
    inline constexpr const char* phaserId = "sonify.builtin.phaser";
    inline constexpr const char* autoWahId = "sonify.builtin.autowah";
    inline constexpr const char* reverbId = "sonify.builtin.reverb";
    inline constexpr const char* delayId = "sonify.builtin.delay";
    inline constexpr const char* sweetenerId = "sonify.builtin.sweetener";
    inline constexpr const char* duckerId = "sonify.builtin.ducker";
    inline constexpr const char* testSynthId = "sonify.builtin.testsynth";

    // Sonify's own plugins (InternalPlugin.h): their ids are listed in Internals::entries(), and
    // these are the two Sonify asks for by name.
    inline constexpr const char* naturaId = "sonify.builtin.natura";
    inline constexpr const char* zaxId = "sonify.builtin.zax";

    bool isBuiltinId (const juce::String& pluginId);

    /** The file the built-ins are listed under: the executable itself. */
    const juce::File& file();
    bool isBuiltinFile (const juce::String& path);

    /** Every built-in, for the Browser's library. */
    const std::vector<Plugin::Descriptor>& describe();

    /** The Browser's Type for one: EQ, Dynamics, Mastering, Modulation, Reverb, Delay, Filter,
        Synth or System Tool. */
    juce::String categoryOf (const juce::String& pluginId);

    std::unique_ptr<Plugin> create (const juce::String& pluginId, juce::String& error);
}

//==============================================================================
/** What the built-ins share: a fixed list of parameters (each atomic, so the audio thread reads
    them while the message thread sets them), their state as JSON, and stereo processing in place.
    An instrument overrides getRole(), getMainInputChannels() and process().

    Its window: a panel (getPanelName(): panel_<name>.json among the built-in panels), drawn by
    the screen the app installs (setScreenFactory; BuiltinScreen.cpp). Its meters watch "in L",
    "in R", "out L" and "out R" (peaks this class keeps) and what the plugin shows of its own
    (readSource: its getDisplaySources() as 0..1); its scopes draw readCurve. */
class BuiltinPlugin : public Plugin
{
public:
    /** The window the app makes for a built-in's panel. */
    struct Screen
    {
        virtual ~Screen() = default;
        virtual void show (const juce::String& title) = 0;
        virtual void hide() = 0;
    };

    using ScreenFactory = std::function<std::unique_ptr<Screen> (BuiltinPlugin&)>;
    static void setScreenFactory (ScreenFactory factory);

    struct Spec
    {
        uint32_t id;
        const char* name;
        double minValue, maxValue, defaultValue;
        const char* unit = "";
        int decimals = 1;
        bool stepped = false, hidden = false, saved = true;
        const char* offAt = nullptr;             // "min" or "max": the value that reads "Off"
        std::vector<const char*> choices {};     // stepped parameters' names for their values
    };

    BuiltinPlugin (Descriptor, std::vector<Spec>);
    ~BuiltinPlugin() override;

    const std::vector<Spec>& getSpecs() const noexcept                   { return specs; }

    /** How many decimals a parameter's values keep, and show: none on a percentage (0..100 in
        whole steps), at most two on the rest (enough for about a hundred steps on its range). */
    static int decimalsOf (const Spec& spec) noexcept;

    /** A frequency or time over a wide range: its knob turns as it's heard (log), so its value
        is kept exactly (rounding it to whole units would hold the knob at the bottom quarter)
        and shown with the decimals its size calls for. */
    static bool isLogRange (const Spec& spec) noexcept;
    static int decimalsFor (const Spec& spec, double value) noexcept;

    /** A value as the parameter keeps it: in its range, on its steps. */
    static double quantised (const Spec& spec, double value) noexcept;

    //==============================================================================
    // Its panel.
    ScreenKind getScreenKind() const noexcept override                   { return getPanelName() != nullptr ? ScreenKind::native : ScreenKind::none; }
    bool showNativeScreen (const juce::String& title) override;
    void hideNativeScreen() override;

    /** Its panel's name (panel_<name>.json), or none: the Hub's generic controls. */
    virtual const char* getPanelName() const noexcept                    { return nullptr; }

    /** A meter's, lamp's or display's level (message thread): "in L/R", "out L/R" as linear
        peaks, the plugin's own displays as 0..1. */
    virtual float readSource (const juce::String& source) const noexcept;
    virtual juce::StringArray getDisplaySources() const                  { return {}; }

    /** A scope's picture (message thread): numSamples values, -1..1. */
    virtual void readCurve (const juce::String& source, float* destination, int numSamples);
    virtual juce::StringArray getCurveSources() const                    { return {}; }

    /** Its panel's keyboard (instruments): a note played, and whether one sounds. */
    virtual void panelNote (int note, float velocity)                    { juce::ignoreUnused (note, velocity); }
    virtual bool isPanelNoteDown (int note) const                        { juce::ignoreUnused (note); return false; }

    /** The last parameter its panel moved (MIDI Link's Last Touched), once. */
    void panelTouched (int index) noexcept                               { touched.store (index); }

    /** The preset its panel's preset manager is on (message thread), kept with the song: its
        name and group (Factory, My presets, a bank's), and whether its values changed since.
        known: a song or the panel has said which (until then its panel finds it). */
    struct PresetChoice
    {
        juce::String name, group;
        bool modified = false, known = false;
    };

    PresetChoice presetChoice;

    /** Something to keep with the song changed (its panel's preset). */
    void markStateChanged() noexcept                                     { stateChanged.store (true); }

    juce::String getFormatName() const override                          { return "Built-in"; }
    const Descriptor& getDescriptor() const noexcept override            { return descriptor; }
    const std::vector<Descriptor>& getPluginsInFile() const noexcept override { return inFile; }
    const juce::File& getSourceFile() const noexcept override            { return Builtins::file(); }
    Role getRole() const noexcept override                               { return Role::effect; }
    int getMainInputChannels() const noexcept override                   { return 2; }
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
    int takeTouchedParameter() noexcept override                         { return touched.exchange (-1); }
    double queueParameter (int index, double value) override;
    void setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept override;

    bool activate (double sampleRate, int maxBlockSize) override;
    void deactivate() override                                           { active.store (false); }
    bool isActive() const noexcept override                              { return active.load(); }

    void process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                  int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept override;

protected:
    /** A parameter's value by its id (any thread). */
    double valueOf (uint32_t id) const noexcept;

    /** The tempo where this block starts (process() keeps the host's transport); 120 without one. */
    double getBlockTempo() const noexcept;
    bool isBlockPlaying() const noexcept;

    /** Keeps the output's peaks for the panel's meters (an instrument's own process() calls it). */
    void measureOutput (const float* left, const float* right, int numSamples) noexcept;

    double getSampleRate() const noexcept                                { return currentRate; }

    /** Fills a scope's values with a response curve: dbAt(hz) from 20 Hz to 20 kHz, +-rangeDb
        across its height. */
    static void drawResponse (float* destination, int numSamples, double rangeDb, const std::function<double (double)>& dbAt);
    static double magnitudeDb (const SonifyDsp::Biquad& filter, double hz, double sampleRate);

    /** A reduction (dB) as a VU's needle reads it on a display meter (0..1): 0 VU at rest, down
        the scale as it turns the sound down, as a compressor's meter shows gain reduction. */
    static float vuReduction (float reductionDb) noexcept
    {
        const auto low = 0.1f, top = 1.4125375f;
        return juce::jlimit (0.0f, 1.0f, (std::pow (10.0f, -juce::jmax (0.0f, reductionDb) / 20.0f) - low) / (top - low));
    }

    /** For an instrument's own process(): whether a parameter changed (update() is due), and
        the block's transport. */
    bool takeDirty() noexcept;
    void setTransport (const clap_event_transport_t* transport) noexcept;
    bool isActiveNow() const noexcept                                    { return active.load (std::memory_order_relaxed); }

    /** Sets a parameter by its id (message thread), as the page or a song would. */
    void setValue (uint32_t id, double value);

    virtual void prepareAudio (double sampleRate, int maxBlockSize) = 0;

    /** Audio thread, at the start of a block after a parameter changed: the values to the DSP. */
    virtual void update() noexcept = 0;

    virtual void processAudio (float* left, float* right, int numSamples) noexcept = 0;

    /** More than parameters to keep with the song (message thread). */
    virtual juce::var saveExtra() const                                  { return {}; }
    virtual void loadExtra (const juce::var&)                            {}

    std::atomic<bool> stateChanged { false };

private:
    int indexOf (uint32_t id) const noexcept;

    Descriptor descriptor;
    std::vector<Descriptor> inFile;
    std::vector<Spec> specs;
    std::vector<Parameter> parameters;
    std::unique_ptr<std::atomic<double>[]> values;
    std::vector<int> indexById;                  // ids are small: a direct lookup
    std::atomic<bool> dirty { true }, active { false };

    // The panel's meters: input and output peaks, falling back between blocks.
    std::array<std::atomic<float>, 4> peaks {};       // in L, in R, out L, out R
    void measure (int first, const float* left, const float* right, int numSamples) noexcept;
    double currentRate = 48000.0;
    const clap_event_transport_t* blockTransport = nullptr;

    std::unique_ptr<Screen> screen;
    std::atomic<int> touched { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BuiltinPlugin)
};

//==============================================================================
/**
    Sonify Mastering: the whole mastering chain in one insert (SonifyDsp::MasteringChain), as the
    Mastering Assistant sets it up on Main. It keeps what the assistant found (loudness before and
    after, what it changed) with the song, and can play the original for comparison: as it is, or
    with the master turned down to the original's loudness so only the sound differs.
*/
class MasteringPlugin final : public BuiltinPlugin
{
public:
    MasteringPlugin();

    /** The assistant's settings and what it found, with the loudness it added (message thread). */
    void applyAssistant (const SonifyDsp::MasteringSettings& settings, const juce::var& info, double loudnessChangeDb);

    juce::var getInfo() const                                           { return info; }

    /** Comparing (not kept with the song): the original instead of the master, and the master at
        the original's loudness. */
    void setListen (bool original, bool sameLoudness);
    bool isListeningToOriginal() const noexcept;
    bool isSameLoudness() const noexcept;

    const char* getPanelName() const noexcept override   { return "mastering"; }
    int getLatencySamples() const noexcept override      { return chain.getLatency(); }   // its limiter's look-ahead
    juce::StringArray getDisplaySources() const override  { return { "master.glue", "master.limit" }; }
    juce::StringArray getCurveSources() const override    { return { "master.curve" }; }
    float readSource (const juce::String& source) const noexcept override;
    void readCurve (const juce::String& source, float* destination, int numSamples) override;

private:
    void prepareAudio (double sampleRate, int maxBlockSize) override;
    void update() noexcept override;
    void processAudio (float* left, float* right, int numSamples) noexcept override;
    juce::var saveExtra() const override                                { return info; }
    void loadExtra (const juce::var& extra) override                    { info = extra; }

    SonifyDsp::MasteringChain chain;
    juce::AudioBuffer<float> scratch;
    std::array<std::vector<float>, 2> dryDelay;  // the original, as late as the master
    int dryAt = 0;
    float dryMix = 0.0f;                         // 0 the master, 1 the original (ramped)
    float masterGain = 1.0f;                     // the master's level when matched (ramped)
    std::atomic<float> glueReduction { 0.0f }, limiterReduction { 0.0f };   // dB, for the panel's meters
    juce::var info;
};
