#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/Gate.h"
#include "dsp/Comp.h"
#include "dsp/Drive.h"
#include "dsp/AmpCapture.h"
#include "dsp/Space.h"

class AxisAudioProcessor : public juce::AudioProcessor
{
public:
    AxisAudioProcessor();
    ~AxisAudioProcessor() override = default;

    // ---- AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AXIS"; }

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presetName; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- AXIS ----
    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }

    /** Chain order as module indices, e.g. {0,1,2,3,4}. Owned by the editor's
        drag-to-reorder and persisted with the plugin state. */
    std::array<int, axis::numModules> chainOrder() const;
    void setChainOrder (const std::array<int, axis::numModules>& order);

    /** Metering, read by the UI. */
    float inputLevel()  const noexcept { return meterIn.load();  }
    float outputLevel() const noexcept { return meterOut.load(); }

    axis::dsp::AmpCapture& ampModule() noexcept { return amp; }

    // ---- rigs (presets) ----
    /** The rig as it stands: every pedal's settings, the chain's order, the AMP's capture. */
    axis::PresetManager::Rig currentRig() const;

    /** Puts a rig on: the values it knows, its chain order, and its capture. Values this AXIS
        doesn't have are ignored, and ones the rig doesn't mention are left as they are, so a rig
        from an older AXIS still loads. */
    void applyRig (const axis::PresetManager::Rig& rig);

    /** Writes the rig as it stands into a file of its own, and that rig becomes the one loaded. */
    juce::Result savePresetFile (const juce::File& file, const juce::String& name);

    /** Loads one. false when the file isn't an AXIS rig (nothing changes). */
    bool loadPresetFile (const juce::File& file);

    /** The rig that's loaded, and its file when it's one of the user's (none: it isn't). */
    const juce::String& getPresetName() const noexcept { return presetName; }
    const juce::File& getPresetFile() const noexcept   { return presetFile; }

private:
    void pullParams();

    juce::AudioProcessorValueTreeState apvts;

    axis::dsp::Gate       gate;
    axis::dsp::Comp       comp;
    axis::dsp::Drive      drive;
    axis::dsp::AmpCapture amp;
    axis::dsp::Space      space;

    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f };
    float meterInSm = 0.0f, meterOutSm = 0.0f;

    juce::CriticalSection orderLock;
    std::array<int, axis::numModules> order { 0, 1, 2, 3, 4 };

    juce::String presetName { "Default" };   // the rig that's loaded
    juce::File   presetFile;                 // none: not one of the user's files

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AxisAudioProcessor)
};
