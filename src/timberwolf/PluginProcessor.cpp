// D:\workspace\Fanan_web_hub\src\timberwolf\PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TwFactory.h"
#include "TwPresetFiles.h"

using namespace tw;

namespace
{
    /** An array's item, or nothing when it's shorter (or isn't one). */
    juce::var itemOf (const juce::var& list, int i)
    {
        if (const auto* a = list.getArray())
            if (i >= 0 && i < a->size())
                return (*a)[i];

        return {};
    }

    inline float safety (float x) noexcept
    {
        constexpr float knee = 2.0f, ceiling = 4.0f;
        const float a = std::abs (x);

        if (! (a < 1.0e6f))   // NaN or a blow-up: silence
            return 0.0f;

        if (a <= knee)
            return x;

        const float over = knee + (ceiling - knee) * std::tanh ((a - knee) / (ceiling - knee));
        return x < 0.0f ? -over : over;
    }

    juce::String slotId (int slot, int knob)
    {
        return "s" + juce::String (slot + 1).paddedLeft ('0', 2) + (knob < 0 ? juce::String ("on") : "k" + juce::String (knob + 1).paddedLeft ('0', 2));
    }

    juce::File globalFolder()
    {
        return sonify::presets::settingsFolder().getChildFile ("Timber Wolf");
    }
}

//==============================================================================
/** A slot's knob or on switch, as the host sees it: 0..1, named and shown as the effect in the
    slot has it. */
class TimberWolfProcessor::SlotParam final : public juce::AudioProcessorParameterWithID
{
public:
    SlotParam (const TimberWolfProcessor& o, int slotIndex, int knobIndex)
        : AudioProcessorParameterWithID (juce::ParameterID { slotId (slotIndex, knobIndex), 1 },
                                         "S" + juce::String (slotIndex + 1) + (knobIndex < 0 ? juce::String (" On") : " K" + juce::String (knobIndex + 1))),
          owner (o), slot (slotIndex), knob (knobIndex)
    {
        value.store (knob < 0 ? 1.0f : 0.5f);
    }

    float getValue() const override                     { return value.load(); }
    void setValue (float v) override                    { value.store (juce::jlimit (0.0f, 1.0f, v)); }
    float getDefaultValue() const override              { return knob < 0 ? 1.0f : 0.5f; }
    bool isDiscrete() const override                    { return knob < 0 || (spec() != nullptr && spec()->isChoice()); }
    bool isBoolean() const override                     { return knob < 0; }

    int getNumSteps() const override
    {
        if (knob < 0) return 2;
        if (const auto* s = spec(); s != nullptr && s->isChoice()) return s->choices.size();
        return AudioProcessorParameterWithID::getNumSteps();
    }

    juce::String getName (int maximumLength) const override
    {
        juce::String n = "S" + juce::String (slot + 1);

        if (const auto* info = owner.slotInfo[(size_t) slot].load())
        {
            n << " " << info->name;

            if (knob < 0)                       n << " On";
            else if (knob < (int) info->knobs.size()) n << " " << info->knobs[(size_t) knob].name;
            else                                n << " K" << (knob + 1);
        }
        else
        {
            n << (knob < 0 ? juce::String (" On") : " K" + juce::String (knob + 1));
        }

        return n.substring (0, maximumLength);
    }

    juce::String getLabel() const override              { return {}; }

    juce::String getText (float v, int) const override
    {
        if (knob < 0)
            return v > 0.5f ? "On" : "Off";

        if (const auto* s = spec())
            return s->text (s->toPlain (v));

        return juce::String (juce::roundToInt (v * 100.0f)) + " %";
    }

    float getValueForText (const juce::String& text) const override
    {
        if (knob < 0)
            return text.trim().equalsIgnoreCase ("on") || text.getFloatValue() > 0.5f ? 1.0f : 0.0f;

        if (const auto* s = spec())
        {
            if (s->isChoice())
            {
                const int i = s->choices.indexOf (text.trim(), true);
                return i >= 0 ? s->toNorm ((float) i) : value.load();
            }

            return s->toNorm (text.getFloatValue());
        }

        return juce::jlimit (0.0f, 1.0f, text.getFloatValue() * 0.01f);
    }

    const std::atomic<float>* raw() const noexcept      { return &value; }

private:
    const KnobSpec* spec() const
    {
        const auto* info = owner.slotInfo[(size_t) slot].load();
        return (info != nullptr && knob >= 0 && knob < (int) info->knobs.size()) ? &info->knobs[(size_t) knob] : nullptr;
    }

    const TimberWolfProcessor& owner;
    const int slot, knob;
    std::atomic<float> value { 0.5f };
};

//==============================================================================
TimberWolfProcessor::TimberWolfProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    registry::all();   // every effect described, here on the message thread

    for (auto& s : slotInfo)
        s.store (nullptr);

    auto in = std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "input", 1 }, "Input",
                                                            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                                                            juce::AudioParameterFloatAttributes().withLabel ("dB"));
    auto out = std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "Output",
                                                             juce::NormalisableRange<float> (-48.0f, 12.0f, 0.1f), 0.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB"));
    input = in.get();
    output = out.get();
    addParameter (in.release());
    addParameter (out.release());

    for (int p = 0; p < numPedals; ++p)
    {
        auto exp = std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "exp" + juce::String (p + 1), 1 },
                                                                 "EXP " + juce::String (p + 1),
                                                                 juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f);
        pedalParams[(size_t) p] = exp.get();
        addParameter (exp.release());
        pedalCc[(size_t) p].store (-1);
    }

    std::array<SlotSource, maxBlocks> sources {};

    for (int s = 0; s < maxBlocks; ++s)
    {
        auto on = std::make_unique<SlotParam> (*this, s, -1);
        onParams[(size_t) s] = on.get();
        sources[(size_t) s].on = on->raw();
        addParameter (on.release());

        for (int k = 0; k < maxKnobs; ++k)
        {
            auto p = std::make_unique<SlotParam> (*this, s, k);
            knobParams[(size_t) s][(size_t) k] = p.get();
            sources[(size_t) s].knobs[(size_t) k] = p->raw();
            addParameter (p.release());
        }
    }

    engine.setSlotSources (sources);

    for (auto& v : learnedKind)    v.store (0);
    for (auto& v : learnedNumber)  v.store (0);
    for (auto& v : learnedChannel) v.store (0);

    loadGlobalLearned();

    // A new Timber Wolf opens on the first factory rig.
    if (numFactoryPresets() > 0)
        loadFactoryPreset (0);
    else
        rebuild();

    modified = false;
    startTimerHz (30);
}

TimberWolfProcessor::~TimberWolfProcessor()
{
    stopTimer();
}

//==============================================================================
bool TimberWolfProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TimberWolfProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    engine.prepare (rate, juce::jmax (32, samplesPerBlock));
    tunerAnalyzer.prepare (rate);
    setLatencySamples (engine.getLatency());
}

void TimberWolfProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int inChannels = getTotalNumInputChannels();
    const int outChannels = getTotalNumOutputChannels();

    for (int ch = inChannels; ch < outChannels; ++ch)
        buffer.clear (ch, 0, n);

    // A foot controller: learning, footswitches, pedals.
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int target = learnTarget.load();

        if (target >= 0 && (m.isController() || m.isNoteOn()))
        {
            learnedKind[(size_t) target].store (m.isController() ? (int) Learned::Kind::cc : (int) Learned::Kind::note);
            learnedNumber[(size_t) target].store (m.isController() ? m.getControllerNumber() : m.getNoteNumber());
            learnedChannel[(size_t) target].store (m.getChannel());
            learnTarget.store (-1);
            learnedFromAudio.fetch_add (1);
            continue;
        }

        for (int i = 0; i < numSwitches + numPedals; ++i)
        {
            Learned l;
            l.kind = (Learned::Kind) learnedKind[(size_t) i].load (std::memory_order_relaxed);
            l.number = learnedNumber[(size_t) i].load (std::memory_order_relaxed);
            l.channel = 0;   // a learned message answers on any channel: controllers change channels

            if (! l.matches (m))
                continue;

            if (i < numSwitches)
            {
                // A press: a note on, or a controller going up past the middle.
                const bool press = m.isNoteOn() || (m.isController() && m.getControllerValue() >= 64);

                if (press)
                {
                    int start1, size1, start2, size2;
                    pressFifo.prepareToWrite (1, start1, size1, start2, size2);
                    if (size1 > 0) pressQueue[(size_t) start1] = i;
                    pressFifo.finishedWrite (size1);
                }
            }
            else if (m.isController())
            {
                const int p = i - numSwitches;
                pedalParams[(size_t) p]->setValueNotifyingHost ((float) m.getControllerValue() / 127.0f);
            }
        }
    }

    midi.clear();

    // The pedals move their knobs.
    for (int p = 0; p < numPedals; ++p)
    {
        const int slot = pedalSlot[(size_t) p].load (std::memory_order_relaxed);
        const int k = pedalKnob[(size_t) p].load (std::memory_order_relaxed);

        if (slot < 0 || k < 0)
            continue;

        const float pos = pedalParams[(size_t) p]->get();

        if (pos != pedalLast[(size_t) p])
        {
            pedalLast[(size_t) p] = pos;
            const float from = pedalFrom[(size_t) p].load (std::memory_order_relaxed);
            const float to = pedalTo[(size_t) p].load (std::memory_order_relaxed);
            knobParams[(size_t) slot][(size_t) k]->setValue (from + (to - from) * pos);
            pedalMoved.store (true);
        }
    }

    float* l = buffer.getWritePointer (0);
    float* r = outChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    if (r == nullptr)
    {
        monoRight.setSize (1, n, false, false, true);
        r = monoRight.getWritePointer (0);
    }

    if (inChannels < 2)
        juce::FloatVectorOperations::copy (r, l, n);

    const float inGain = dbToGain (input->get());
    juce::FloatVectorOperations::multiply (l, inGain, n);
    juce::FloatVectorOperations::multiply (r, inGain, n);

    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
        peakIn = juce::jmax (peakIn, std::abs (l[i]), std::abs (r[i]));

    tunerAnalyzer.feed (l, n);

    // The tempo: the host's, unless it's tapped.
    double bpm = tappedBpm.load();
    bool playing = false;

    if (auto* head = getPlayHead())
    {
        if (const auto pos = head->getPosition())
        {
            playing = pos->getIsPlaying();

            if (! tapTempo.load())
                if (const auto hostBpm = pos->getBpm())
                    bpm = *hostBpm;
        }
    }

    bpmNow.store (bpm);
    engine.process (l, r, n, bpm, playing);

    const float outGain = (tuning.load() && muteWhileTuning) ? 0.0f : dbToGain (output->get());
    float peakOut = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        // A safety net, not a sound: past +6 dBFS it bends softly towards +12 dBFS at most, so
        // an extreme setting can't hurt ears or speakers. Below that it does nothing.
        l[i] = safety (l[i] * outGain);
        r[i] = safety (r[i] * outGain);
        peakOut = juce::jmax (peakOut, std::abs (l[i]), std::abs (r[i]));
    }

    if (outChannels < 2)
        juce::FloatVectorOperations::addWithMultiply (l, r, 1.0f, n), juce::FloatVectorOperations::multiply (l, 0.5f, n);

    meterIn.store (juce::jmax (peakIn, meterIn.load() * 0.9f));
    meterOut.store (juce::jmax (peakOut, meterOut.load() * 0.9f));
}

//==============================================================================
void TimberWolfProcessor::timerCallback()
{
    int start1, size1, start2, size2;
    pressFifo.prepareToRead (pressFifo.getNumReady(), start1, size1, start2, size2);

    std::vector<int> presses;
    for (int i = 0; i < size1; ++i) presses.push_back (pressQueue[(size_t) (start1 + i)]);
    for (int i = 0; i < size2; ++i) presses.push_back (pressQueue[(size_t) (start2 + i)]);
    pressFifo.finishedRead (size1 + size2);

    for (int i : presses)
        runFootswitch (i);

    engine.tick();

    if (const int learnedNow = learnedFromAudio.load(); learnedNow != learnedSeen)
    {
        learnedSeen = learnedNow;
        saveGlobalLearned();

        if (onValuesChanged)
            onValuesChanged();
    }

    if (pedalMoved.exchange (false))
    {
        markModified();

        if (onValuesChanged)
            onValuesChanged();
    }

    if (tuning.load())
        tunerAnalyzer.analyse();
}

//==============================================================================
//  The chain
//==============================================================================
void TimberWolfProcessor::rebuild()
{
    for (int s = 0; s < maxBlocks; ++s)
    {
        const int uid = chain.uidInSlot (s);
        slotInfo[(size_t) s].store (uid != 0 ? registry::find (chain.blocks.at (uid).type) : nullptr);
    }

    const int latency = engine.rebuild (chain);

    if (latency != getLatencySamples())
        setLatencySamples (latency);

    // The pedals follow their effects into their slots.
    for (int p = 0; p < numPedals; ++p)
        setPedal (p, pedals[(size_t) p]);

    updateHostDisplay (ChangeDetails().withParameterInfoChanged (true));

    if (onChainChanged)
        onChainChanged();
}

void TimberWolfProcessor::giveSlotDefaults (int uid)
{
    const int slot = slotOf (uid);
    const auto* info = infoOf (uid);

    if (slot < 0 || info == nullptr)
        return;

    for (int k = 0; k < maxKnobs; ++k)
    {
        const float norm = k < (int) info->knobs.size() ? info->knobs[(size_t) k].toNorm (info->knobs[(size_t) k].def) : 0.5f;
        knobParams[(size_t) slot][(size_t) k]->setValueNotifyingHost (norm);
    }

    onParams[(size_t) slot]->setValueNotifyingHost (1.0f);
}

int TimberWolfProcessor::addBlock (const juce::String& type, const Path& path, int index)
{
    const int uid = chain.addBlock (type, path, index);

    if (uid != 0)
    {
        giveSlotDefaults (uid);
        markModified();
        rebuild();
    }

    return uid;
}

int TimberWolfProcessor::addParallel (const Path& path, int index)
{
    const int uid = chain.addParallel (path, index);
    if (uid != 0) { markModified(); rebuild(); }
    return uid;
}

int TimberWolfProcessor::splitAround (int uid)
{
    const int par = chain.splitAround (uid);
    if (par != 0) { markModified(); rebuild(); }
    return par;
}

bool TimberWolfProcessor::removeNode (int uid)
{
    if (! chain.remove (uid))
        return false;

    for (auto& f : switches)
        f.uids.erase (std::remove (f.uids.begin(), f.uids.end(), uid), f.uids.end());

    for (auto& p : pedals)
        if (p.uid == uid)
            p = {};

    markModified();
    rebuild();
    return true;
}

bool TimberWolfProcessor::unsplit (int parallelUid)
{
    if (! chain.unsplit (parallelUid))
        return false;

    markModified();
    rebuild();
    return true;
}

bool TimberWolfProcessor::moveNode (int uid, const Path& path, int index)
{
    if (! chain.move (uid, path, index))
        return false;

    markModified();
    rebuild();
    return true;
}

bool TimberWolfProcessor::swapPaths (int parallelUid)
{
    if (! chain.swapPaths (parallelUid))
        return false;

    markModified();
    rebuild();
    return true;
}

bool TimberWolfProcessor::replaceBlock (int uid, const juce::String& type)
{
    if (! chain.replace (uid, type))
        return false;

    giveSlotDefaults (uid);
    markModified();
    rebuild();
    return true;
}

void TimberWolfProcessor::setParallel (int uid, const ParallelSettings& s)
{
    if (auto* n = chain.node (uid); n != nullptr && n->kind == Node::Kind::parallel)
    {
        n->settings = s;
        engine.updateParallel (uid, s);
        markModified();
    }
}

const BlockInfo* TimberWolfProcessor::infoOf (int uid) const
{
    auto it = chain.blocks.find (uid);
    return it != chain.blocks.end() ? registry::find (it->second.type) : nullptr;
}

int TimberWolfProcessor::slotOf (int uid) const
{
    auto it = chain.blocks.find (uid);
    return it != chain.blocks.end() ? it->second.slot : -1;
}

float TimberWolfProcessor::knobValue (int uid, int knob) const
{
    const int slot = slotOf (uid);
    return (slot >= 0 && knob >= 0 && knob < maxKnobs) ? knobParams[(size_t) slot][(size_t) knob]->getValue() : 0.0f;
}

juce::String TimberWolfProcessor::knobText (int uid, int knob) const
{
    const auto* info = infoOf (uid);

    if (info == nullptr || knob < 0 || knob >= (int) info->knobs.size())
        return {};

    const auto& spec = info->knobs[(size_t) knob];
    return spec.text (spec.toPlain (knobValue (uid, knob)));
}

void TimberWolfProcessor::setKnobValue (int uid, int knob, float norm)
{
    const int slot = slotOf (uid);

    if (slot < 0 || knob < 0 || knob >= maxKnobs)
        return;

    knobParams[(size_t) slot][(size_t) knob]->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    markModified();
}

void TimberWolfProcessor::beginKnobGesture (int uid, int knob)
{
    if (const int slot = slotOf (uid); slot >= 0 && knob >= 0 && knob < maxKnobs)
        knobParams[(size_t) slot][(size_t) knob]->beginChangeGesture();
}

void TimberWolfProcessor::endKnobGesture (int uid, int knob)
{
    if (const int slot = slotOf (uid); slot >= 0 && knob >= 0 && knob < maxKnobs)
        knobParams[(size_t) slot][(size_t) knob]->endChangeGesture();
}

void TimberWolfProcessor::resetKnob (int uid, int knob)
{
    if (const auto* info = infoOf (uid); info != nullptr && knob >= 0 && knob < (int) info->knobs.size())
    {
        beginKnobGesture (uid, knob);
        setKnobValue (uid, knob, info->knobs[(size_t) knob].toNorm (info->knobs[(size_t) knob].def));
        endKnobGesture (uid, knob);
    }
}

bool TimberWolfProcessor::isOn (int uid) const
{
    const int slot = slotOf (uid);
    return slot >= 0 && onParams[(size_t) slot]->getValue() > 0.5f;
}

void TimberWolfProcessor::setOn (int uid, bool on)
{
    if (const int slot = slotOf (uid); slot >= 0)
    {
        onParams[(size_t) slot]->beginChangeGesture();
        onParams[(size_t) slot]->setValueNotifyingHost (on ? 1.0f : 0.0f);
        onParams[(size_t) slot]->endChangeGesture();
        markModified();
    }
}

//==============================================================================
//  Tempo
//==============================================================================
void TimberWolfProcessor::tap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();

    if (! taps.empty() && now - taps.back() > 2000.0)
        taps.clear();

    taps.push_back (now);

    if (taps.size() > 5)
        taps.erase (taps.begin());

    if (taps.size() >= 2)
    {
        const double ms = (taps.back() - taps.front()) / (double) (taps.size() - 1);
        tappedBpm.store (juce::jlimit (30.0, 300.0, 60000.0 / ms));
        tapTempo.store (true);
    }
}

//==============================================================================
//  Footswitches, pedals, snapshots
//==============================================================================
void TimberWolfProcessor::setFootswitch (int i, const Footswitch& f)
{
    switches[(size_t) juce::jlimit (0, numSwitches - 1, i)] = f;
    markModified();
}

void TimberWolfProcessor::runFootswitch (int i)
{
    if (i < 0 || i >= numSwitches)
        return;

    const auto& f = switches[(size_t) i];

    switch (f.job)
    {
        case Footswitch::Job::toggle:
        {
            // All its effects go the way the first one isn't.
            bool any = false, to = true;

            for (int uid : f.uids)
                if (slotOf (uid) >= 0) { to = ! isOn (uid); any = true; break; }

            if (any)
                for (int uid : f.uids)
                    setOn (uid, to);
            break;
        }

        case Footswitch::Job::presetUp:     stepPreset (1);  break;
        case Footswitch::Job::presetDown:   stepPreset (-1); break;
        case Footswitch::Job::snapshot:     recallSnapshot (f.snapshot); break;
        case Footswitch::Job::tap:          tap(); break;
        case Footswitch::Job::tuner:        setTuning (! isTuning()); break;
        case Footswitch::Job::none:
        default:                            break;
    }

    if (onValuesChanged)
        onValuesChanged();
}

void TimberWolfProcessor::pressFootswitch (int i)
{
    runFootswitch (i);
}

bool TimberWolfProcessor::isFootswitchLit (int i) const
{
    const auto& f = footswitch (i);

    switch (f.job)
    {
        case Footswitch::Job::toggle:
            for (int uid : f.uids)
                if (slotOf (uid) >= 0)
                    return isOn (uid);
            return false;

        case Footswitch::Job::snapshot:     return snapshotNow == f.snapshot;
        case Footswitch::Job::tuner:        return isTuning();
        case Footswitch::Job::tap:          return tapTempo.load();
        case Footswitch::Job::presetUp:
        case Footswitch::Job::presetDown:
        case Footswitch::Job::none:
        default:                            return false;
    }
}

juce::String TimberWolfProcessor::footswitchLabel (int i) const
{
    const auto& f = footswitch (i);

    if (f.label.isNotEmpty())
        return f.label;

    switch (f.job)
    {
        case Footswitch::Job::toggle:
        {
            juce::StringArray names;

            for (int uid : f.uids)
                if (const auto* info = infoOf (uid))
                    names.add (info->name);

            return names.isEmpty() ? juce::String ("-") : names.joinIntoString (" + ");
        }

        case Footswitch::Job::presetUp:     return "Preset +";
        case Footswitch::Job::presetDown:   return "Preset -";
        case Footswitch::Job::snapshot:
        {
            const auto& s = snapshot (f.snapshot);
            return s.name.isNotEmpty() ? s.name : "Snap " + juce::String (f.snapshot + 1);
        }
        case Footswitch::Job::tap:          return "Tap";
        case Footswitch::Job::tuner:        return "Tuner";
        case Footswitch::Job::none:
        default:                            return "-";
    }
}

void TimberWolfProcessor::setPedal (int i, const Pedal& p)
{
    i = juce::jlimit (0, numPedals - 1, i);
    pedals[(size_t) i] = p;

    const int slot = p.uid != 0 ? slotOf (p.uid) : -1;
    const auto* info = p.uid != 0 ? infoOf (p.uid) : nullptr;
    const bool ok = slot >= 0 && info != nullptr && p.knob >= 0 && p.knob < (int) info->knobs.size();

    pedalFrom[(size_t) i].store (p.from);
    pedalTo[(size_t) i].store (p.to);
    pedalKnob[(size_t) i].store (ok ? p.knob : -1);
    pedalSlot[(size_t) i].store (ok ? slot : -1);
    pedalLast[(size_t) i] = -1.0f;
}

float TimberWolfProcessor::pedalPosition (int i) const
{
    return pedalParams[(size_t) juce::jlimit (0, numPedals - 1, i)]->get();
}

void TimberWolfProcessor::setPedalPosition (int i, float pos)
{
    pedalParams[(size_t) juce::jlimit (0, numPedals - 1, i)]->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, pos));
}

void TimberWolfProcessor::learn (int target)
{
    learnTarget.store (target >= 0 && target < numSwitches + numPedals ? target : -1);
}

void TimberWolfProcessor::forget (int target)
{
    if (target < 0 || target >= numSwitches + numPedals)
        return;

    learnedKind[(size_t) target].store (0);
    learnedNumber[(size_t) target].store (0);
    learnedChannel[(size_t) target].store (0);
    saveGlobalLearned();
}

Learned TimberWolfProcessor::learned (int target) const
{
    Learned l;

    if (target >= 0 && target < numSwitches + numPedals)
    {
        l.kind = (Learned::Kind) learnedKind[(size_t) target].load();
        l.number = learnedNumber[(size_t) target].load();
        l.channel = learnedChannel[(size_t) target].load();
    }

    return l;
}

void TimberWolfProcessor::storeSnapshot (int i)
{
    i = juce::jlimit (0, numSnapshots - 1, i);
    auto& s = snapshots[(size_t) i];
    s.stored = true;

    for (int slot = 0; slot < maxBlocks; ++slot)
    {
        s.on[(size_t) slot] = onParams[(size_t) slot]->getValue();

        for (int k = 0; k < maxKnobs; ++k)
            s.knobs[(size_t) slot][(size_t) k] = knobParams[(size_t) slot][(size_t) k]->getValue();
    }

    s.parallels.clear();

    std::function<void (const std::vector<Node>&)> walk = [&] (const std::vector<Node>& c)
    {
        for (const auto& n : c)
            if (n.kind == Node::Kind::parallel)
            {
                s.parallels[n.uid] = n.settings;
                walk (n.a);
                walk (n.b);
            }
    };

    walk (chain.main);
    snapshotNow = i;
    markModified();
}

void TimberWolfProcessor::recallSnapshot (int i)
{
    i = juce::jlimit (0, numSnapshots - 1, i);
    const auto& s = snapshots[(size_t) i];

    if (! s.stored)
        return;

    for (int slot = 0; slot < maxBlocks; ++slot)
    {
        if (chain.uidInSlot (slot) == 0)
            continue;

        onParams[(size_t) slot]->setValueNotifyingHost (s.on[(size_t) slot]);

        for (int k = 0; k < maxKnobs; ++k)
            knobParams[(size_t) slot][(size_t) k]->setValueNotifyingHost (s.knobs[(size_t) slot][(size_t) k]);
    }

    for (const auto& [uid, settings] : s.parallels)
        if (auto* n = chain.node (uid); n != nullptr && n->kind == Node::Kind::parallel)
        {
            n->settings = settings;
            engine.updateParallel (uid, settings);
        }

    snapshotNow = i;

    if (onValuesChanged)
        onValuesChanged();
}

void TimberWolfProcessor::renameSnapshot (int i, const juce::String& name)
{
    snapshots[(size_t) juce::jlimit (0, numSnapshots - 1, i)].name = name;
    markModified();
}

void TimberWolfProcessor::clearSnapshot (int i)
{
    i = juce::jlimit (0, numSnapshots - 1, i);
    snapshots[(size_t) i] = {};

    if (snapshotNow == i)
        snapshotNow = -1;

    markModified();
}

//==============================================================================
//  Rigs: the chain, every effect's values, snapshots, footswitch jobs, pedal targets
//==============================================================================
juce::var TimberWolfProcessor::rigToVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "Timber Wolf rig");
    root->setProperty ("version", 1);
    root->setProperty ("chain", chain.toVar());

    // Values by uid (not by slot): a rig reads back the same however its slots fall.
    auto* values = new juce::DynamicObject();

    for (const auto& [uid, entry] : chain.blocks)
    {
        auto* v = new juce::DynamicObject();
        v->setProperty ("on", onParams[(size_t) entry.slot]->getValue() > 0.5f);

        juce::Array<juce::var> k;
        for (int i = 0; i < maxKnobs; ++i)
            k.add (knobParams[(size_t) entry.slot][(size_t) i]->getValue());

        v->setProperty ("knobs", k);
        values->setProperty (juce::String (uid), juce::var (v));
    }

    root->setProperty ("values", juce::var (values));

    juce::Array<juce::var> snaps;

    for (const auto& s : snapshots)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("stored", s.stored);
        o->setProperty ("name", s.name);

        if (s.stored)
        {
            auto* byUid = new juce::DynamicObject();

            for (const auto& [uid, entry] : chain.blocks)
            {
                auto* v = new juce::DynamicObject();
                v->setProperty ("on", s.on[(size_t) entry.slot] > 0.5f);

                juce::Array<juce::var> k;
                for (int i = 0; i < maxKnobs; ++i)
                    k.add (s.knobs[(size_t) entry.slot][(size_t) i]);

                v->setProperty ("knobs", k);
                byUid->setProperty (juce::String (uid), juce::var (v));
            }

            o->setProperty ("values", juce::var (byUid));

            auto* pars = new juce::DynamicObject();

            for (const auto& [uid, ps] : s.parallels)
            {
                auto* p = new juce::DynamicObject();
                p->setProperty ("split", ps.splitType);
                p->setProperty ("xover", ps.crossoverHz);
                p->setProperty ("levelA", ps.levelA);
                p->setProperty ("levelB", ps.levelB);
                p->setProperty ("panA", ps.panA);
                p->setProperty ("panB", ps.panB);
                p->setProperty ("invertB", ps.invertB);
                pars->setProperty (juce::String (uid), juce::var (p));
            }

            o->setProperty ("parallels", juce::var (pars));
        }

        snaps.add (juce::var (o));
    }

    root->setProperty ("snapshots", snaps);
    root->setProperty ("snapshotNow", snapshotNow);

    juce::Array<juce::var> sw;

    for (const auto& f : switches)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("job", (int) f.job);
        juce::Array<juce::var> uids;
        for (int u : f.uids) uids.add (u);
        o->setProperty ("uids", uids);
        o->setProperty ("snapshot", f.snapshot);
        o->setProperty ("label", f.label);
        sw.add (juce::var (o));
    }

    root->setProperty ("switches", sw);

    juce::Array<juce::var> pd;

    for (const auto& p : pedals)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("uid", p.uid);
        o->setProperty ("knob", p.knob);
        o->setProperty ("from", p.from);
        o->setProperty ("to", p.to);
        pd.add (juce::var (o));
    }

    root->setProperty ("pedals", pd);
    return juce::var (root);
}

void TimberWolfProcessor::rigFromVar (const juce::var& v)
{
    Graph g;

    if (! g.fromVar (v.getProperty ("chain", {})))
        return;

    chain = std::move (g);

    for (const auto& [uid, entry] : chain.blocks)
    {
        giveSlotDefaults (uid);

        const auto values = v.getProperty ("values", {}).getProperty (juce::String (uid), {});

        if (! values.isObject())
            continue;

        onParams[(size_t) entry.slot]->setValueNotifyingHost ((bool) values.getProperty ("on", true) ? 1.0f : 0.0f);

        if (const auto* k = values.getProperty ("knobs", {}).getArray())
            for (int i = 0; i < juce::jmin (maxKnobs, k->size()); ++i)
                knobParams[(size_t) entry.slot][(size_t) i]->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (float) (double) (*k)[i]));
    }

    // Snapshots
    for (int i = 0; i < numSnapshots; ++i)
    {
        auto& s = snapshots[(size_t) i];
        s = {};
        const auto o = itemOf (v.getProperty ("snapshots", {}), i);

        if (! o.isObject())
            continue;

        s.name = o.getProperty ("name", {}).toString();
        s.stored = (bool) o.getProperty ("stored", false);

        if (! s.stored)
            continue;

        for (int slot = 0; slot < maxBlocks; ++slot)
        {
            s.on[(size_t) slot] = onParams[(size_t) slot]->getValue();
            for (int k = 0; k < maxKnobs; ++k)
                s.knobs[(size_t) slot][(size_t) k] = knobParams[(size_t) slot][(size_t) k]->getValue();
        }

        for (const auto& [uid, entry] : chain.blocks)
        {
            const auto sv = o.getProperty ("values", {}).getProperty (juce::String (uid), {});

            if (! sv.isObject())
                continue;

            s.on[(size_t) entry.slot] = (bool) sv.getProperty ("on", true) ? 1.0f : 0.0f;

            if (const auto* k = sv.getProperty ("knobs", {}).getArray())
                for (int j = 0; j < juce::jmin (maxKnobs, k->size()); ++j)
                    s.knobs[(size_t) entry.slot][(size_t) j] = juce::jlimit (0.0f, 1.0f, (float) (double) (*k)[j]);
        }

        if (const auto* pars = o.getProperty ("parallels", {}).getDynamicObject())
        {
            for (const auto& p : pars->getProperties())
            {
                ParallelSettings ps;
                ps.splitType = (int) p.value.getProperty ("split", 0);
                ps.crossoverHz = (float) (double) p.value.getProperty ("xover", 700.0);
                ps.levelA = (float) (double) p.value.getProperty ("levelA", 0.0);
                ps.levelB = (float) (double) p.value.getProperty ("levelB", 0.0);
                ps.panA = (float) (double) p.value.getProperty ("panA", 0.0);
                ps.panB = (float) (double) p.value.getProperty ("panB", 0.0);
                ps.invertB = (bool) p.value.getProperty ("invertB", false);
                s.parallels[p.name.toString().getIntValue()] = ps;
            }
        }
    }

    snapshotNow = (int) v.getProperty ("snapshotNow", -1);

    // Footswitches
    for (int i = 0; i < numSwitches; ++i)
    {
        auto& f = switches[(size_t) i];
        f = {};
        const auto o = itemOf (v.getProperty ("switches", {}), i);

        if (! o.isObject())
            continue;

        f.job = (Footswitch::Job) juce::jlimit (0, (int) Footswitch::Job::tuner, (int) o.getProperty ("job", 0));
        f.snapshot = juce::jlimit (0, numSnapshots - 1, (int) o.getProperty ("snapshot", 0));
        f.label = o.getProperty ("label", {}).toString();

        if (const auto* uids = o.getProperty ("uids", {}).getArray())
            for (const auto& u : *uids)
                if (chain.blocks.count ((int) u) != 0)
                    f.uids.push_back ((int) u);
    }

    // Pedals
    for (int i = 0; i < numPedals; ++i)
    {
        Pedal p;
        const auto o = itemOf (v.getProperty ("pedals", {}), i);

        if (o.isObject())
        {
            p.uid = (int) o.getProperty ("uid", 0);
            p.knob = (int) o.getProperty ("knob", 0);
            p.from = (float) (double) o.getProperty ("from", 0.0);
            p.to = (float) (double) o.getProperty ("to", 1.0);

            if (chain.blocks.count (p.uid) == 0)
                p = {};
        }

        pedals[(size_t) i] = p;
    }

    engine.reset();
    rebuild();
}

//==============================================================================
//  Presets
//==============================================================================
int TimberWolfProcessor::numFactoryPresets() const              { return (int) factory::presets().size(); }

juce::String TimberWolfProcessor::factoryPresetName (int index) const
{
    const auto& list = factory::presets();
    return index >= 0 && index < (int) list.size() ? list[(size_t) index].name : juce::String();
}

void TimberWolfProcessor::loadFactoryPreset (int index)
{
    const auto& list = factory::presets();

    if (index < 0 || index >= (int) list.size())
        return;

    rigFromVar (factory::rigOf (list[(size_t) index]));
    presetName = list[(size_t) index].name;
    presetFile = juce::File();
    factoryIndex = index;
    modified = false;

    if (onChainChanged)
        onChainChanged();
}

juce::Result TimberWolfProcessor::savePresetFile (const juce::File& file, const juce::String& name)
{
    std::map<juce::String, float> values;

    for (const auto& [uid, entry] : chain.blocks)
    {
        values[slotId (entry.slot, -1)] = onParams[(size_t) entry.slot]->getValue();

        for (int k = 0; k < maxKnobs; ++k)
            values[slotId (entry.slot, k)] = knobParams[(size_t) entry.slot][(size_t) k]->getValue();
    }

    auto* extras = new juce::DynamicObject();
    extras->setProperty ("rig", rigToVar());

    const auto result = sonify::presets::save (file, presetfiles::pluginName, name, values, juce::var (extras));

    if (result.wasOk())
    {
        presetName = sonify::presets::cleanName (name);
        presetFile = file;
        factoryIndex = -1;
        modified = false;
    }

    return result;
}

bool TimberWolfProcessor::loadPresetFile (const juce::File& file)
{
    const auto loaded = sonify::presets::load (file);

    if (! loaded)
        return false;

    const auto rig = loaded->whole.getProperty ("rig", {});

    if (! rig.isObject())
        return false;

    rigFromVar (rig);
    presetName = loaded->name;
    presetFile = file;
    factoryIndex = -1;
    modified = false;

    if (onChainChanged)
        onChainChanged();

    return true;
}

juce::Result TimberWolfProcessor::saveBankFile (const juce::File& file)
{
    juce::Array<juce::var> list;

    for (const auto& f : presetfiles::userPresets())
    {
        if (const auto loaded = sonify::presets::load (f))
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", loaded->name);
            o->setProperty ("rig", loaded->whole.getProperty ("rig", {}));
            list.add (juce::var (o));
        }
    }

    // The rig as it stands, too, when it isn't one of them.
    if (! presetFile.existsAsFile())
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", presetName);
        o->setProperty ("rig", rigToVar());
        list.add (juce::var (o));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("format", presetfiles::bankFormat);
    root->setProperty ("version", 1);
    root->setProperty ("presets", list);

    if (! file.replaceWithText (juce::JSON::toString (juce::var (root))))
        return juce::Result::fail ("Couldn't write " + file.getFullPathName());

    return juce::Result::ok();
}

int TimberWolfProcessor::loadBankFile (const juce::File& file)
{
    const auto json = juce::JSON::parse (file.loadFileAsString());

    if (json.getProperty ("format", {}).toString() != presetfiles::bankFormat)
        return 0;

    int count = 0;

    if (const auto* list = json.getProperty ("presets", {}).getArray())
    {
        // Each written as one of the user's presets, through this plugin (so its values are real).
        const auto keep = rigToVar();
        const auto keepName = presetName;
        const auto keepFile = presetFile;
        const int keepFactory = factoryIndex;

        for (const auto& item : *list)
        {
            const auto name = item.getProperty ("name", "Preset").toString();
            const auto rig = item.getProperty ("rig", {});

            if (! rig.isObject())
                continue;

            rigFromVar (rig);

            auto target = presetfiles::fileFor (name);
            for (int i = 2; target.existsAsFile() && i < 100; ++i)
                target = presetfiles::fileFor (name + " " + juce::String (i));

            if (savePresetFile (target, target.getFileNameWithoutExtension()).wasOk())
                ++count;
        }

        rigFromVar (keep);
        presetName = keepName;
        presetFile = keepFile;
        factoryIndex = keepFactory;
        modified = false;

        if (onChainChanged)
            onChainChanged();
    }

    return count;
}

void TimberWolfProcessor::stepPreset (int delta)
{
    const int factoryCount = numFactoryPresets();
    const auto users = presetfiles::userPresets();
    const int total = factoryCount + users.size();

    if (total == 0)
        return;

    int now = factoryIndex;

    if (now < 0)
        for (int i = 0; i < users.size(); ++i)
            if (users[i] == presetFile) { now = factoryCount + i; break; }

    const int next = now < 0 ? 0 : ((now + delta) % total + total) % total;

    if (next < factoryCount)
        loadFactoryPreset (next);
    else
        loadPresetFile (users[next - factoryCount]);
}

//==============================================================================
//  MIDI learned: Timber Wolf-wide, kept beside Sonify's settings
//==============================================================================
void TimberWolfProcessor::loadGlobalLearned()
{
    const auto json = juce::JSON::parse (globalFolder().getChildFile ("midi.json"));

    if (const auto* list = json.getProperty ("learned", {}).getArray())
    {
        for (int i = 0; i < juce::jmin (numSwitches + numPedals, list->size()); ++i)
        {
            const auto& o = (*list)[i];
            learnedKind[(size_t) i].store (juce::jlimit (0, 2, (int) o.getProperty ("kind", 0)));
            learnedNumber[(size_t) i].store (juce::jlimit (0, 127, (int) o.getProperty ("number", 0)));
            learnedChannel[(size_t) i].store (juce::jlimit (0, 16, (int) o.getProperty ("channel", 0)));
        }
    }
}

void TimberWolfProcessor::saveGlobalLearned() const
{
    juce::Array<juce::var> list;

    for (int i = 0; i < numSwitches + numPedals; ++i)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("kind", learnedKind[(size_t) i].load());
        o->setProperty ("number", learnedNumber[(size_t) i].load());
        o->setProperty ("channel", learnedChannel[(size_t) i].load());
        list.add (juce::var (o));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("learned", list);

    const auto folder = globalFolder();
    folder.createDirectory();
    folder.getChildFile ("midi.json").replaceWithText (juce::JSON::toString (juce::var (root)));
}

//==============================================================================
//  State (songs)
//==============================================================================
void TimberWolfProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("rig", rigToVar());
    root->setProperty ("presetName", presetName);
    root->setProperty ("presetFile", presetFile.getFullPathName());
    root->setProperty ("factoryIndex", factoryIndex);
    root->setProperty ("modified", modified);
    root->setProperty ("input", input->get());
    root->setProperty ("output", output->get());
    root->setProperty ("tapTempo", tapTempo.load());
    root->setProperty ("tappedBpm", tappedBpm.load());
    root->setProperty ("muteWhileTuning", muteWhileTuning);
    root->setProperty ("uiScale", uiScale);

    juce::Array<juce::var> exp;
    for (const auto* p : pedalParams) exp.add (p->get());
    root->setProperty ("exp", exp);

    juce::Array<juce::var> learnedList;
    for (int i = 0; i < numSwitches + numPedals; ++i)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("kind", learnedKind[(size_t) i].load());
        o->setProperty ("number", learnedNumber[(size_t) i].load());
        o->setProperty ("channel", learnedChannel[(size_t) i].load());
        learnedList.add (juce::var (o));
    }
    root->setProperty ("learned", learnedList);

    const auto text = juce::JSON::toString (juce::var (root), true);
    destData.replaceAll (text.toRawUTF8(), text.getNumBytesAsUTF8());
}

void TimberWolfProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes));

    if (! json.isObject())
        return;

    rigFromVar (json.getProperty ("rig", {}));
    presetName = json.getProperty ("presetName", "Init").toString();
    presetFile = juce::File (json.getProperty ("presetFile", {}).toString());
    factoryIndex = (int) json.getProperty ("factoryIndex", -1);
    modified = (bool) json.getProperty ("modified", false);

    *input = (float) (double) json.getProperty ("input", 0.0);
    *output = (float) (double) json.getProperty ("output", 0.0);
    tapTempo.store ((bool) json.getProperty ("tapTempo", false));
    tappedBpm.store (juce::jlimit (30.0, 300.0, (double) json.getProperty ("tappedBpm", 120.0)));
    muteWhileTuning = (bool) json.getProperty ("muteWhileTuning", true);
    uiScale = juce::jlimit (0.5f, 1.25f, (float) (double) json.getProperty ("uiScale", 0.85));

    if (const auto* exp = json.getProperty ("exp", {}).getArray())
        for (int i = 0; i < juce::jmin (numPedals, exp->size()); ++i)
            *pedalParams[(size_t) i] = juce::jlimit (0.0f, 1.0f, (float) (double) (*exp)[i]);

    // A song's learned MIDI only fills in what this computer hasn't learned (the foot
    // controller here is what counts).
    if (const auto* list = json.getProperty ("learned", {}).getArray())
        for (int i = 0; i < juce::jmin (numSwitches + numPedals, list->size()); ++i)
            if (learnedKind[(size_t) i].load() == 0)
            {
                const auto& o = (*list)[i];
                learnedKind[(size_t) i].store (juce::jlimit (0, 2, (int) o.getProperty ("kind", 0)));
                learnedNumber[(size_t) i].store (juce::jlimit (0, 127, (int) o.getProperty ("number", 0)));
                learnedChannel[(size_t) i].store (juce::jlimit (0, 16, (int) o.getProperty ("channel", 0)));
            }

    if (onChainChanged)
        onChainChanged();
}

void TimberWolfProcessor::setUiScale (float s)
{
    uiScale = juce::jlimit (0.5f, 1.25f, std::round (s * 20.0f) / 20.0f);
}

//==============================================================================
juce::AudioProcessorEditor* TimberWolfProcessor::createEditor()
{
    return new TimberWolfEditor (*this);
}
