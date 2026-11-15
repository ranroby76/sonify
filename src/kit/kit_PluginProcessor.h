// D:\workspace\Fanan_web_hub\src\kit\kit_PluginProcessor.h

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginInfo.h"
#include "PresetLibrary.h"
#include "stella_blob.h"
#include "stella_licence.h"
#include "stella_runtime.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>

//==============================================================================
/**
    The plugin: the Stella runtime (stella/stella_runtime.cpp) playing the plugin's modules
    (modules folder) wired as its graph says (stella/stella_graph.cpp), as JUCE's plugin. The
    same runtime plays it in Sonify's Plugins Creator Studio (as WebAssembly) and in the plugins
    its Export makes (natively), so it sounds the same.

        parameters   one per module parameter, in the runtime's order; its id is the runtime's
                     ("<module>.<param>"), so presets, songs and the window know it by that
        notes        the host's MIDI, and the window's keyboards (keyboard)
        tempo        the host's tempo and position (tempo sync)
        presets      the plugin's own, the user's (PresetLibrary: Documents\Sonify\Presets\
                     <plugin>, shared with Sonify), banks
        live data    what the window's meters, lamps and scopes watch: signals by name
                     ("voices.out", "plugin.out L") and module displays ("lfo.position")
        samples      a sampler's library (stella_blob.h): its blobs by their files' names
                     (PluginInfo.h), found on this computer as it starts
        protection   a protected plugin (PluginInfo::isProtected) plays its demo until this
                     computer is registered: its license.soni beside it, its Registration window
                     from the registration badge on its panel

    Its state: every parameter's value by id, and the preset it's on.
*/
class SonifyPluginProcessor final : public juce::AudioProcessor
{
public:
    SonifyPluginProcessor();
    ~SonifyPluginProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return juce::String::fromUTF8 (PluginInfo::name); }
    bool acceptsMidi() const override                        { return true; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 0.0; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // For the window (message thread).

    /** Whether its modules were made and wired; why not, when they weren't. */
    bool isBuilt() const noexcept                            { return built; }
    juce::String getBuildError() const                       { return juce::String::fromUTF8 (runtime.getError().c_str()); }

    int getNumPluginParameters() const noexcept              { return (int) infos.size(); }
    const stella::ParamInfo& getInfo (int index) const       { return infos[(size_t) index]; }
    float getValue (int index) const;

    /** The window moved a control: the host hears it (automation). */
    void setValueFromWindow (int index, float value);

    PresetLibrary presets;
    std::map<juce::String, float> currentValues() const;
    void applyPreset (const Preset& preset);

    /** The window's keyboards play it, and light up with every note played. */
    juce::MidiKeyboardState keyboard;

    /** What the window watches: levels (meters, lamps) and scopes, by name. */
    void setWindowSources (const juce::StringArray& levels, const juce::StringArray& scopes);
    float takeLevel (const juce::String& source, bool rms);
    void readScope (const juce::String& source, float* destination, int numSamples);

    /** A sampler's instruments whose blobs aren't on this computer, and where they're looked for. */
    juce::StringArray getMissingInstruments() const;
    juce::File libraryFolder() const;

    //==========================================================================
    // Protection (PluginInfo::isProtected): the plugin plays its demo until it's registered.
    bool isProtected() const noexcept                        { return PluginInfo::isProtected; }
    bool isRegistered() const noexcept                       { return registered.load(); }
    juce::String getComputerId() const;
    juce::String getRegistrationName() const;

    /** Registers this computer with a serial: "" when it's done, else why not. */
    juce::String registerSerial (const juce::String& serial);

private:
    struct Parameter;

    void setUpLibrary();
    void setUpLicence();
    void checkRegistration();
    void applyWindowSources() noexcept;   // the audio thread, at a block's start
    void captureLive (int frames) noexcept;
    bool resolve (const juce::String& source, std::int32_t& node, std::int32_t& port) const;

    stella::Runtime runtime;
    stella::blob::Library library;
    bool built = false, prepared = false;
    double sampleRate = 48000.0;
    int maxFrames = 512;

    std::vector<stella::ParamInfo> infos;
    std::vector<Parameter*> parameters;   // owned by the AudioProcessor

    juce::MidiBuffer played;              // the host's notes and the keyboard's, each block

    // What the window watches: set on the message thread, taken by the audio thread.
    struct Sources
    {
        std::vector<std::pair<std::int32_t, std::int32_t>> taps;   // node, port
        std::array<std::pair<std::int32_t, std::int32_t>, stella::maxScopes> scopes;
    };

    juce::SpinLock sourcesLock;
    Sources pendingSources;
    std::atomic<bool> sourcesChanged { false };
    juce::StringArray levelNames, scopeNames;          // message thread: in the order of the taps and scopes

    static constexpr int scopeSize = 4096;
    int numTaps = 0;
    std::array<std::atomic<float>, stella::maxTaps> tapPeak {}, tapRms {};
    std::array<std::vector<float>, stella::maxScopes> scopeRing;
    std::array<std::atomic<std::uint32_t>, stella::maxScopes> scopeWrite {};
    std::array<std::atomic<float>, stella::maxDisplays> displayValues {};

    std::vector<stella::blob::Entry> libraryEntries;

    stella::licence::Soni soni;
    bool soniFound = false;
    std::atomic<bool> registered { ! PluginInfo::isProtected };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonifyPluginProcessor)
};
