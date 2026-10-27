// C:\workspace\SONIFY PLUGIN natura\src\PluginProcessor.h
#pragma once
//==============================================================================
//  PluginProcessor.h  -  Natura, the Sonify modeling synth.
//==============================================================================
#include <JuceHeader.h>
#include "NaturaEngine.h"
#include "PresetManager.h"

class NaturaProcessor final : public juce::AudioProcessor
{
public:
    NaturaProcessor();
    ~NaturaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Natura"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return presetName; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // presets and roots (message thread)
    void applyValues (const std::vector<float>& values);
    std::vector<float> currentValues() const;
    void loadArchetypeRoot (int archetype);
    void loadFactory (int index);
    bool loadPresetFile (const juce::File& file);
    juce::Result savePresetFile (const juce::File& file, const juce::String& name);

    /** The panel picked an articulation: honoured even when the parameter
        already holds that index (a keyswitch may have moved off it). */
    void requestArticulation (int index);

    float value (natura::P p) const { return raw[(int) p]->load(); }
    juce::RangedAudioParameter* param (natura::P p) const { return params[(size_t) p]; }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    natura::NaturaEngine engine;

    juce::String presetName { "Grand Piano" };
    juce::File   presetFile;                 // empty: not one of the user's files

private:
    std::array<std::atomic<float>*, natura::kNumParams> raw {};
    std::array<juce::RangedAudioParameter*, natura::kNumParams> params {};
    std::array<float, natura::kNumParams> values {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NaturaProcessor)
};
