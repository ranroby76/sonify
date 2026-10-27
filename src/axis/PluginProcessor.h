#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/Gate.h"
#include "dsp/Comp.h"
#include "dsp/Drive.h"
#include "dsp/AmpModel.h"
#include "dsp/Space.h"

#include "../AxisPack.h"   // the AXIS pack beside Sonify: the cabinets' impulse responses

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

    axis::dsp::AmpModel& ampModule() noexcept { return amp; }

    // ---- the cabinet's impulse response ----
    /** Where the AXIS pack is: beside Sonify (on a Mac in the app's Resources, where the build
        puts it), else in Sonify's own folder (AppData\Roaming\Fanan\Sonify\AXIS). */
    static juce::File packFile();

    /** The AXIS pack, opened the first time it's asked for (nullptr: there isn't one, or it
        can't be read; getPackProblem says which). Message thread. */
    const AxisPack::Pack* getPack();
    juce::String getPackProblem() const                 { return packProblem; }

    /** The impulse responses in the pack, by name, in its order. Message thread. */
    juce::StringArray packCabinetNames();

    /** The impulse response in the cabinet, as a rig and the state remember it: "pack:<name>"
        (one in the AXIS pack), a file's full path, or "" (none). */
    const juce::String& getCabIr() const noexcept      { return cabIrSource; }
    juce::String getCabIrName() const                   { return amp.getCabinet().getIrName(); }
    const juce::String& getCabIrProblem() const noexcept { return cabIrProblem; }

    /** Puts an impulse response in the cabinet (and, select: turns CAB_TYPE to it). "" when it's
        in, else why not: the cabinet keeps what it had. Message thread. */
    juce::String setCabIr (const juce::String& source, bool select = true);

    /** No impulse response any more; CAB_TYPE on IR goes to the modelled cabinet it falls back to. */
    void clearCabIr();

    /** The last IR problem forgotten (a modelled cabinet was picked instead). */
    void forgetCabIrProblem()                           { cabIrProblem.clear(); }

    // ---- rigs (presets) ----
    /** The rig as it stands: every pedal's settings, the chain's order, the cabinet's IR. */
    axis::PresetManager::Rig currentRig() const;

    /** Puts a rig on: the values it knows, its chain order, and its IR. Values this AXIS
        doesn't have are ignored, and ones the rig doesn't mention are left as they are, so a rig
        from an older AXIS still loads. */
    void applyRig (const axis::PresetManager::Rig& rig);

    /** One of the rigs AXIS comes with (PresetManager::factoryRigs()): the pedals go back to
        their own settings first, so a factory rig is the same every time. */
    void loadFactory (int index);

    /** Writes the rig as it stands into a file of its own, and that rig becomes the one loaded. */
    juce::Result savePresetFile (const juce::File& file, const juce::String& name);

    /** Loads one. false when the file isn't an AXIS rig (nothing changes). */
    bool loadPresetFile (const juce::File& file);

    /** The rig that's loaded, and its file when it's one of the user's (none: it isn't). */
    const juce::String& getPresetName() const noexcept { return presetName; }
    const juce::File& getPresetFile() const noexcept   { return presetFile; }

private:
    void pullParams();
    axis::dsp::AmpModel::Params ampParams();

    /** A rig's or the state's IR put back: "" none; one that's gone, none either (and why). */
    void restoreCabIr (const juce::String& source);

    juce::AudioProcessorValueTreeState apvts;

    axis::dsp::Gate       gate;
    axis::dsp::Comp       comp;
    axis::dsp::Drive      drive;
    axis::dsp::AmpModel   amp;
    axis::dsp::Space      space;

    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f };
    float meterInSm = 0.0f, meterOutSm = 0.0f;

    juce::CriticalSection orderLock;
    std::array<int, axis::numModules> order { 0, 1, 2, 3, 4 };

    void resetToDefaults();

    juce::String presetName { "Default" };   // the rig that's loaded
    juce::File   presetFile;                 // none: not one of the user's files

    juce::String cabIrSource, cabIrProblem;  // see getCabIr
    std::unique_ptr<AxisPack::Pack> pack;
    juce::String packProblem;
    bool packTried = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AxisAudioProcessor)
};
