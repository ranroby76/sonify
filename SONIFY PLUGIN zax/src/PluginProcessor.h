// C:\workspace\SONIFY PLUGIN zax\src\PluginProcessor.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <memory>
#include "ZaxEngine.h"
#include "ZaxKitFiles.h"

//==============================================================================
//  Zax — Sonify's GM drum synth.  Grex's EDM kit engine, one voice per hit,
//  every key on its fixed GM role (ZaxGm.h), kits as .zaxkit files.
//
//  The kit lives here twice: the WORKING copy the editor changes (message
//  thread), and the copy the engine plays, replaced whole on every edit
//  (publishKit).  The plugin's state keeps the whole kit, so a song plays back
//  exactly as it was saved even when the kit file changed since.
//==============================================================================
class ZaxProcessor final : public juce::AudioProcessor
{
public:
    ZaxProcessor();
    ~ZaxProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                          { return true; }

    const juce::String getName() const override              { return "Zax"; }
    bool acceptsMidi() const override                        { return true; }
    bool producesMidi() const override                       { return false; }
    bool isMidiEffect() const override                       { return false; }
    double getTailLengthSeconds() const override             { return 4.0; }

    int getNumPrograms() override                            { return 1; }
    int getCurrentProgram() override                         { return 0; }
    void setCurrentProgram (int) override                    {}
    const juce::String getProgramName (int) override         { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    //  The editor's side (message thread)
    //==========================================================================
    zax::Kit&         kit() noexcept                         { return *working; }
    const juce::File& kitFile() const noexcept               { return currentFile; }
    juce::String      kitName() const                        { return juce::String (working->name); }
    bool              isModified() const noexcept            { return modified; }

    /** The working kit changed: play it from the next hit. */
    void kitEdited();

    juce::Result loadKit (const zax::files::Entry& entry);
    juce::Result saveKitAs (const juce::File& file, const juce::String& name);
    juce::Result saveKit();                                 // over the file it came from

    /** Bumped on every kit load (the editor rebuilds its menus). */
    int kitGeneration() const noexcept                       { return generation; }

    /** A key played from the editor (no MIDI behind it). */
    void auditionNoteOn (int key, int velocity) noexcept;
    void auditionNoteOff (int key) noexcept;

    /** The keys sounding now: held keys, plus any hit since the last call. */
    void readActivity (std::array<uint32_t, 4>& mask) noexcept;

    /** The window's size, 0.4 .. 1.0 of the design size.  Kept with the song,
        and as the size a new Zax window opens at. */
    float getUiScale() const noexcept                        { return uiScale; }
    void  setUiScale (float scale);

    /** Message-thread housekeeping: frees kits the audio thread let go of. */
    void housekeeping()                                      { engine.freeRetired(); }

private:
    void publishKit();
    void keyDown (int key) noexcept;
    void keyUp (int key) noexcept;

    zax::Engine                 engine;
    std::unique_ptr<zax::Kit>   working = std::make_unique<zax::Kit>();
    juce::File                  currentFile;
    bool                        modified   = false;
    float                       uiScale    = 0.8f;
    int                         generation = 0;

    // Editor auditions -> audio thread: a single-producer ring of (key, velocity)
    static constexpr int kQueue = 256;
    std::array<std::atomic<int>, kQueue> queue {};
    std::atomic<int> qWrite { 0 }, qRead { 0 };

    std::array<std::atomic<uint32_t>, 4> held {}, hits {};
    std::array<uint8_t, 128>             heldCount {};   // audio thread

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZaxProcessor)
};
