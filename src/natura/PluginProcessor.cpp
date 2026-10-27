// C:\workspace\SONIFY PLUGIN natura\src\PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace natura;

NaturaProcessor::NaturaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NATURA", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        raw[(size_t) i]    = apvts.getRawParameterValue (info (i).id);
        params[(size_t) i] = apvts.getParameter (info (i).id);
        jassert (raw[(size_t) i] != nullptr && params[(size_t) i] != nullptr);
    }
    // a new instance opens on the first factory sound
    applyValues (factoryValues (factoryPresets().front()));
    presetName = factoryPresets().front().name;
}

bool NaturaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void NaturaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

void NaturaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    for (int i = 0; i < kNumParams; ++i) values[(size_t) i] = raw[(size_t) i]->load (std::memory_order_relaxed);
    engine.process (buffer, midi, values.data());
}

juce::AudioProcessorEditor* NaturaProcessor::createEditor()
{
    return new NaturaEditor (*this);
}

//==============================================================================
void NaturaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetFile", presetFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NaturaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            presetName = state.getProperty ("presetName", presetName).toString();
            const auto path = state.getProperty ("presetFile", "").toString();
            presetFile = path.isNotEmpty() && juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
            apvts.replaceState (state);
        }
}

//==============================================================================
std::vector<float> NaturaProcessor::currentValues() const
{
    std::vector<float> v ((size_t) kNumParams);
    for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = raw[(size_t) i]->load();
    return v;
}

void NaturaProcessor::applyValues (const std::vector<float>& v)
{
    // archetype first: the engine resets its instrument before the rest arrives
    auto set = [this, &v] (int i)
    {
        auto* p = params[(size_t) i];
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v[(size_t) i]));
        p->endChangeGesture();
    };
    set ((int) P::archetype);
    for (int i = 0; i < kNumParams; ++i)
        if (i != (int) P::archetype) set (i);
}

void NaturaProcessor::loadArchetypeRoot (int archetype)
{
    auto v = rootValues ((Archetype) juce::jlimit (0, kNumArchetypes - 1, archetype));
    v[(size_t) P::out_volume] = value (P::out_volume);
    applyValues (v);
    presetName = juce::String (archetypeName (archetype)) + " (root)";
    presetFile = juce::File();
}

void NaturaProcessor::loadFactory (int index)
{
    const auto& list = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    auto v = factoryValues (list[(size_t) index]);
    v[(size_t) P::out_volume] = value (P::out_volume);
    applyValues (v);
    presetName = list[(size_t) index].name;
    presetFile = juce::File();
}

bool NaturaProcessor::loadPresetFile (const juce::File& file)
{
    auto loaded = PresetManager::load (file);
    if (! loaded) return false;
    applyValues (loaded->values);
    presetName = loaded->name;
    presetFile = file;
    return true;
}

juce::Result NaturaProcessor::savePresetFile (const juce::File& file, const juce::String& name)
{
    const auto r = PresetManager::save (file, name, currentValues());
    if (r.wasOk()) { presetName = name; presetFile = file; }
    return r;
}

void NaturaProcessor::requestArticulation (int index)
{
    auto* p = params[(size_t) P::art_select];
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
    p->endChangeGesture();
    engine.forcedArt.store (index);
}

