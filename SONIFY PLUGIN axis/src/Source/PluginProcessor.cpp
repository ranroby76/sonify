#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace axis;

namespace
{
    float getFloat (juce::AudioProcessorValueTreeState& s, juce::StringRef id)
    {
        if (auto* p = s.getRawParameterValue (id))
            return p->load();

        jassertfalse;
        return 0.0f;
    }

    bool getBool (juce::AudioProcessorValueTreeState& s, juce::StringRef id)
    {
        return getFloat (s, id) > 0.5f;
    }

    int getChoice (juce::AudioProcessorValueTreeState& s, juce::StringRef id)
    {
        return static_cast<int> (getFloat (s, id));
    }
}

AxisAudioProcessor::AxisAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AXIS", createLayout())
{
    apvts.state.setProperty ("chainOrder", "0,1,2,3,4", nullptr);
}

// ---------------------------------------------------------------------------

void AxisAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32> (samplesPerBlock),
        static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()))
    };

    gate .prepare (spec);
    comp .prepare (spec);
    drive.prepare (spec);
    amp  .prepare (spec);
    space.prepare (spec);

    meterInSm = meterOutSm = 0.0f;
    setLatencySamples (amp.latencySamples());
}

void AxisAudioProcessor::releaseResources()
{
    gate.reset();  comp.reset();  drive.reset();
    amp.reset();   space.reset();
}

bool AxisAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    // mono-in / stereo-out is normal for a guitar rig
    return in == out || (in == juce::AudioChannelSet::mono()
                         && out == juce::AudioChannelSet::stereo());
}

// ---------------------------------------------------------------------------

void AxisAudioProcessor::pullParams()
{
    using namespace axis::pid;

    // ---- GATE ----
    axis::dsp::Gate::Params g;
    g.mode       = static_cast<axis::dsp::Gate::Mode> (getChoice (apvts, gateMode));
    g.threshDb   = getFloat  (apvts, gateThresh);
    g.depth      = getFloat  (apvts, gateDepth);
    g.releaseMs  = getFloat  (apvts, gateRelease);
    g.holdMs     = getFloat  (apvts, gateHold);
    g.scHpfHz    = getFloat  (apvts, gateScHpf);
    g.hissFilter = getBool   (apvts, gateHiss);
    g.autoLearn  = getBool   (apvts, gateLearn);
    g.range      = getFloat  (apvts, gateRange);
    gate.setParams (g);

    // ---- COMP ----
    axis::dsp::Comp::Params c;
    c.circuit   = static_cast<axis::dsp::Comp::Circuit> (getChoice (apvts, compCircuit));
    c.threshDb  = getFloat (apvts, compThresh);
    c.ratio     = getFloat (apvts, compRatio);
    c.makeupDb  = getFloat (apvts, compMakeup);
    c.attackMs  = getFloat (apvts, compAttack);
    c.releaseMs = getFloat (apvts, compRelease);
    c.autoGain  = getBool  (apvts, compAuto);
    c.scHpf     = getBool  (apvts, compScHpf);
    c.parallel  = getBool  (apvts, compParal);
    c.mix       = getFloat (apvts, compMix);
    comp.setParams (c);

    // ---- DRIVE ----
    axis::dsp::Drive::Params d;
    d.voice   = static_cast<axis::dsp::Drive::Voice> (getChoice (apvts, drvVoice));
    d.tone    = getFloat (apvts, drvTone);
    d.gain    = getFloat (apvts, drvGain);
    d.level   = getFloat (apvts, drvLevel);
    d.body    = getFloat (apvts, drvBody);
    d.bite    = getFloat (apvts, drvBite);
    d.dynamic = getBool  (apvts, drvDynamic);
    d.stereo  = getBool  (apvts, drvStereo);
    d.mix     = getFloat (apvts, drvMix);
    drive.setParams (d);

    // ---- AMP ----
    axis::dsp::AmpCapture::Params a;
    a.slim      = static_cast<axis::dsp::AmpCapture::Slim> (getChoice (apvts, ampSlim));
    a.trimDb    = getFloat (apvts, ampTrim);
    a.bass      = getFloat (apvts, ampBass);
    a.mid       = getFloat (apvts, ampMid);
    a.treble    = getFloat (apvts, ampTreble);
    a.presence  = getFloat (apvts, ampPresence);
    a.cabSim    = getBool  (apvts, ampCab);
    a.stackPost = getBool  (apvts, ampStack);
    a.doubler   = getBool  (apvts, ampDbl);
    a.output    = getFloat (apvts, ampOutput);
    a.micX      = getFloat (apvts, cabMicX);
    a.micY      = getFloat (apvts, cabMicY);
    amp.setParams (a);

    // ---- SPACE ----
    axis::dsp::Space::Params s;
    s.type     = static_cast<axis::dsp::Space::Type> (getChoice (apvts, spcType));
    s.preDlyMs = getFloat (apvts, spcPredly);
    s.decay    = getFloat (apvts, spcDecay);
    s.size     = getFloat (apvts, spcSize);
    s.damp     = getFloat (apvts, spcDamp);
    s.width    = getFloat (apvts, spcWidth);
    s.shimmer  = getBool  (apvts, spcShimmer);
    s.freeze   = getBool  (apvts, spcFreeze);
    s.duck     = getBool  (apvts, spcDuck);
    s.mix      = getFloat (apvts, spcMix);
    space.setParams (s);
}

void AxisAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSm  = buffer.getNumSamples();

    // mono guitar into a stereo rig: fan the input out first
    if (numIn == 1 && numOut > 1)
        for (int ch = 1; ch < numOut; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSm);

    for (int ch = numIn; ch < numOut && numIn > 1; ++ch)
        buffer.clear (ch, 0, numSm);

    pullParams();

    // ---- input meter ----
    {
        const float mag = buffer.getMagnitude (0, numSm);
        meterInSm += (mag - meterInSm) * 0.2f;
        meterIn.store (meterInSm);
    }

    // ---- run the chain in the user's order ----
    std::array<int, axis::numModules> localOrder;
    {
        const juce::ScopedLock sl (orderLock);
        localOrder = order;
    }

    using namespace axis::pid;

    for (int slot = 0; slot < axis::numModules; ++slot)
    {
        switch (localOrder[(size_t) slot])
        {
            case 0: if (! getBool (apvts, gateBypass))  gate .process (buffer); break;
            case 1: if (! getBool (apvts, compBypass))  comp .process (buffer); break;
            case 2: if (! getBool (apvts, drvBypass))   drive.process (buffer); break;
            case 3: if (! getBool (apvts, ampBypass))   amp  .process (buffer); break;
            case 4: if (! getBool (apvts, spcBypass))   space.process (buffer); break;
            default: break;
        }
    }

    // ---- output meter + safety ----
    {
        const float mag = buffer.getMagnitude (0, numSm);
        meterOutSm += (mag - meterOutSm) * 0.2f;
        meterOut.store (meterOutSm);
    }

    for (int ch = 0; ch < numOut; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);

        for (int i = 0; i < numSm; ++i)
            if (! std::isfinite (d[i]))
                d[i] = 0.0f;
    }
}

// ---------------------------------------------------------------------------

std::array<int, axis::numModules> AxisAudioProcessor::chainOrder() const
{
    const juce::ScopedLock sl (orderLock);
    return order;
}

void AxisAudioProcessor::setChainOrder (const std::array<int, axis::numModules>& newOrder)
{
    juce::StringArray parts;

    {
        const juce::ScopedLock sl (orderLock);
        order = newOrder;

        for (auto v : order)
            parts.add (juce::String (v));
    }

    apvts.state.setProperty ("chainOrder", parts.joinIntoString (","), nullptr);
}

// ---------------------------------------------------------------------------
//  Rigs (presets)
// ---------------------------------------------------------------------------

axis::PresetManager::Rig AxisAudioProcessor::currentRig() const
{
    PresetManager::Rig rig;
    rig.name = presetName;
    rig.order = chainOrder();
    rig.capture = amp.currentCapturePath();

    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                rig.values[withId->paramID] = ranged->convertFrom0to1 (ranged->getValue());

    return rig;
}

void AxisAudioProcessor::applyRig (const axis::PresetManager::Rig& rig)
{
    for (const auto& [id, value] : rig.values)
    {
        if (auto* param = apvts.getParameter (id))
        {
            const auto norm = param->getNormalisableRange().convertTo0to1 (value);
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        }
    }

    setChainOrder (rig.order);

    // The capture as the rig had it: gone when the rig had none, and the file may have moved
    // since (then the AMP simply has no capture, as it would after a failed load).
    const juce::File capture (rig.capture);

    if (capture != juce::File (amp.currentCapturePath()))
    {
        amp.loadCapture (capture);
        setLatencySamples (amp.latencySamples());
    }
}

juce::Result AxisAudioProcessor::savePresetFile (const juce::File& file, const juce::String& name)
{
    auto rig = currentRig();
    rig.name = PresetManager::cleanName (name);

    const auto result = PresetManager::save (file, rig);

    if (result.wasOk())
    {
        presetName = rig.name;
        presetFile = file;
    }

    return result;
}

bool AxisAudioProcessor::loadPresetFile (const juce::File& file)
{
    const auto rig = PresetManager::load (file);

    if (! rig.has_value())
        return false;

    applyRig (*rig);
    presetName = rig->name;
    presetFile = file;
    return true;
}

// ---------------------------------------------------------------------------

void AxisAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // The rig that's loaded goes with the state, so the song shows its name again.
    apvts.state.setProperty ("presetName", presetName, nullptr);
    apvts.state.setProperty ("presetFile", presetFile.getFullPathName(), nullptr);

    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AxisAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));

        const auto str = apvts.state.getProperty ("chainOrder", "0,1,2,3,4").toString();
        juce::StringArray parts;
        parts.addTokens (str, ",", "");

        if (parts.size() == axis::numModules)
        {
            std::array<int, axis::numModules> loaded {};
            for (int i = 0; i < axis::numModules; ++i)
                loaded[(size_t) i] = juce::jlimit (0, axis::numModules - 1,
                                                   parts[i].getIntValue());

            const juce::ScopedLock sl (orderLock);
            order = loaded;
        }

        presetName = apvts.state.getProperty ("presetName", presetName).toString();

        const auto path = apvts.state.getProperty ("presetFile", "").toString();
        presetFile = path.isNotEmpty() && juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
    }
}

juce::AudioProcessorEditor* AxisAudioProcessor::createEditor()
{
    return new AxisAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AxisAudioProcessor();
}
