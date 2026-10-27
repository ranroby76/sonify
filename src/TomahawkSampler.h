// D:\workspace\Fanan_web_hub\src\TomahawkSampler.h

#pragma once

#include "Plugin.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class TomahawkSampler;

namespace Tomahawk { class Voice; }

//==============================================================================
/**
    The Tomahawk sampler: Grex's sound library played as Grex plays it, a Sonify instrument (a
    System Tool, like the Sonify Sampler) for a track's instrument slot or a Node Rack.

    The library (the Tomahawk_Sampler folder, unzipped from Tomahawk_Library_v1.zip anywhere on
    the computer, found with Locate Library) has two packs, CLASSIC and ORIENTAL, each sorted
    into categories. A sound plays through Grex's own engine (tomahawk\: Grex's Channel, its
    effects, and Grex's chorus, reverb and delay as the sound's own), voiced as Grex voices a
    right-hand pick: from the sound's own preset (.ins). Its one window is Grex's instrument
    editor (TomahawkScreen.h): SOUND ENGINE (PACK > CATEGORY > INSTRUMENT, the pack's picture,
    Grex's LOW VELOCITY RESPONSE and PITCH BEND RANGE), SYNTHESIS, MODULATION, EFFECTS (every
    effect in one row), its keyboard; SAVE PRESET keeps a sound's settings as its own, written into
    its preset in the presets' folders beside Sonify.exe (where shared presets are dropped too);
    a double-click on GAIN sets the sound's base gain, on OCTAVE its base octave, both written
    into its preset. Drum kits aren't the Tomahawk's.

    Its settings are kept with the song (the sound and everything the editor sets). The sound
    is read on a thread of its own; a mixdown waits for it.

    Threads: the instrument and the windows on the message thread; process() on the audio thread.
*/
namespace Tomahawks
{
    inline constexpr const char* playerId = "sonify.builtin.tomahawk";

    Plugin::Descriptor describe();

    /** Where Sonify keeps the user's things: the Tomahawk's settings and presets go in its
        Tomahawk folder. */
    void setUserFolder (std::function<juce::File()> userData);

    /** The Tomahawk's window: made by TomahawkScreen.cpp in the app (none in the engine tests). */
    struct Screen
    {
        virtual ~Screen() = default;
        virtual void show (const juce::String& title) = 0;
        virtual void hide() = 0;
        virtual bool isShowing() const = 0;
    };

    using ScreenFactory = std::function<std::unique_ptr<Screen> (TomahawkSampler&)>;
    void setScreenFactory (ScreenFactory factory);
}

//==============================================================================
class TomahawkSampler final : public Plugin
{
public:
    TomahawkSampler();
    ~TomahawkSampler() override;

    // Its parameters: ids kept with songs.
    enum Param : uint32_t
    {
        volume = 1,      // gain 0..2
        bendRange,       // semitones 1..12 (Grex's PITCH BEND RANGE, 2 at first)
        lowVelocity      // 0..100 (Grex's LOW VELOCITY RESPONSE: soft notes lifted, 0 at first)
    };

    static constexpr int numParams = static_cast<int> (lowVelocity);

    double get (Param id) const noexcept;
    void set (Param id, double value);
    double getDefault (Param id) const noexcept;

    /** The instrument and its settings (TomahawkVoice.h). Message thread. */
    Tomahawk::Voice& getVoice() noexcept                                 { return *voice; }

    /** The window changed something the song keeps. */
    void markChanged() noexcept                                          { stateChanged.store (true); }

    //==============================================================================
    // Plugin
    juce::String getFormatName() const override                          { return "Built-in"; }
    const Descriptor& getDescriptor() const noexcept override            { return descriptor; }
    const std::vector<Descriptor>& getPluginsInFile() const noexcept override { return inFile; }
    const juce::File& getSourceFile() const noexcept override;
    Role getRole() const noexcept override                               { return Role::instrument; }
    int getMainInputChannels() const noexcept override                   { return 0; }
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
    void setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept override;

    bool activate (double sampleRate, int maxBlockSize) override;
    void deactivate() override                                           { active.store (false); }
    bool isActive() const noexcept override                              { return active.load(); }

    void process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                  int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept override;

    ScreenKind getScreenKind() const noexcept override                   { return ScreenKind::native; }
    bool showNativeScreen (const juce::String& title) override;
    void hideNativeScreen() override;

private:
    void applyToVoice (uint32_t id, double value) noexcept;

    Descriptor descriptor;
    std::vector<Descriptor> inFile;
    std::vector<Parameter> parameters;
    std::array<std::atomic<double>, numParams + 1> values {};
    std::atomic<bool> stateChanged { false }, active { false };

    std::unique_ptr<Tomahawk::Voice> voice;
    std::unique_ptr<Tomahawks::Screen> screen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TomahawkSampler)
};
