// D:\workspace\Fanan_web_hub\src\Container.cpp

#include "Container.h"
#include "BuiltinPlugins.h"
#include "NodeTools.h"

#include <algorithm>
#include <cmath>
#include <map>

using Graph = juce::AudioProcessorGraph;
using IoProcessor = juce::AudioProcessorGraph::AudioGraphIOProcessor;

//==============================================================================
namespace
{
    Containers::Host* currentHost = nullptr;

    Containers::ScreenFactory& screenFactory()
    {
        static Containers::ScreenFactory factory;
        return factory;
    }

    std::atomic<int> testSlotIds { 1000000 };

    int newSlotId()
    {
        if (auto* host = Containers::getHost())
            return host->allocateSlotId();

        return testSlotIds++;
    }

    std::unique_ptr<Plugin> loadAnyPlugin (const juce::File& file, const juce::String& pluginId, juce::String& error)
    {
        if (auto* host = Containers::getHost())
            return host->loadPlugin (file, pluginId, error);

        return Plugin::load (file, pluginId, error);
    }

    constexpr int midiBufferBytes = 32768;

    /** A system tool by the name it's saved under; null for one this Sonify doesn't have. */
    std::unique_ptr<juce::AudioProcessor> makeTool (const juce::String& toolName)
    {
        if (toolName == "Connector")          return std::make_unique<ContainerTools::Connector>();
        if (toolName == "StereoMeter")        return std::make_unique<ContainerTools::StereoMeter>();
        if (toolName == "MidiMonitor")        return std::make_unique<ContainerTools::MidiMonitor>();
        if (toolName == "Latcher")            return std::make_unique<NodeTools::Latcher>();
        if (toolName == "StepSeq")            return std::make_unique<NodeTools::StepSeq>();
        if (toolName == "TransientSplitter")  return std::make_unique<NodeTools::TransientSplitter>();
        return nullptr;
    }

    /** The name a tool is saved under; empty when the processor isn't a tool. */
    juce::String toolNameOf (juce::AudioProcessor* proc)
    {
        if (dynamic_cast<ContainerTools::Connector*> (proc) != nullptr)          return "Connector";
        if (dynamic_cast<ContainerTools::StereoMeter*> (proc) != nullptr)        return "StereoMeter";
        if (dynamic_cast<ContainerTools::MidiMonitor*> (proc) != nullptr)        return "MidiMonitor";
        if (dynamic_cast<NodeTools::Latcher*> (proc) != nullptr)                 return "Latcher";
        if (dynamic_cast<NodeTools::StepSeq*> (proc) != nullptr)                 return "StepSeq";
        if (dynamic_cast<NodeTools::TransientSplitter*> (proc) != nullptr)       return "TransientSplitter";
        return {};
    }

    /** Applies a level ramped from where it was (the Mixer's faders), and returns the block's peak. */
    float applyGain (juce::AudioBuffer<float>& buffer, int channels, int numSamples, float& last, float target) noexcept
    {
        channels = juce::jmin (channels, buffer.getNumChannels());

        for (int channel = 0; channel < channels; ++channel)
        {
            if (std::abs (target - last) > 1.0e-6f)
                buffer.applyGainRamp (channel, 0, numSamples, last, target);
            else if (std::abs (target - 1.0f) > 1.0e-6f)
                buffer.applyGain (channel, 0, numSamples, target);
        }

        last = target;
        float peak = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
            peak = juce::jmax (peak, buffer.getMagnitude (channel, 0, numSamples));

        return peak;
    }

    // Up by one for every change to any container's graph, and for every container that goes.
    std::atomic<juce::uint32> structureGeneration { 0 };
}

bool Containers::isContainerId (const juce::String& pluginId)
{
    return pluginId == instrumentId || pluginId == effectId;
}

Plugin::Descriptor Containers::describe (bool effect)
{
    Plugin::Descriptor descriptor;
    descriptor.id = effect ? effectId : instrumentId;
    descriptor.name = ContainerPlugin::defaultName (effect, false);
    descriptor.vendor = "Sonify";
    descriptor.version = "1.0";
    descriptor.features = effect ? juce::StringArray { "audio-effect", "utility", "container", "stereo" }
                                 : juce::StringArray { "instrument", "container", "stereo" };
    return descriptor;
}

juce::File Containers::defaultFolder()
{
    if (auto* host = getHost())
        return host->containersFolder();

    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sonify").getChildFile ("Containers");
}

void Containers::setHost (Host* host)                 { currentHost = host; }
Containers::Host* Containers::getHost()                { return currentHost; }
void Containers::setScreenFactory (ScreenFactory f)    { screenFactory() = std::move (f); }

//==============================================================================
// PluginNode
PluginNode::PluginNode (std::unique_ptr<Plugin> p, int id, ContainerPlugin& ownerContainer)
    : juce::AudioProcessor (busesFor (*p)),
      slotId (id),
      plugin (std::move (p)),
      owner (ownerContainer),
      name (plugin->getDescriptor().name),
      instrument (plugin->getRole() != Plugin::Role::effect),
      sidechain (plugin->getRole() == Plugin::Role::effect && plugin->hasSidechain()),
      makesMidi (dynamic_cast<ContainerPlugin*> (plugin.get()) != nullptr || plugin->producesMidi())
{
    filtered.ensureSize (midiBufferBytes);
    notesOff.ensureSize (4096);
}

PluginNode::~PluginNode()
{
    plugin->hideNativeScreen();

    if (plugin->isActive())
        plugin->deactivate();
}

juce::AudioProcessor::BusesProperties PluginNode::busesFor (const Plugin& p)
{
    BusesProperties buses;

    switch (p.getRole())
    {
        case Plugin::Role::instrument:
            buses = buses.withOutput ("Output", juce::AudioChannelSet::stereo(), true);
            break;

        case Plugin::Role::effect:
            buses = buses.withInput ("Input", juce::AudioChannelSet::stereo(), true);

            if (p.hasSidechain())
                buses = buses.withInput ("Sidechain", juce::AudioChannelSet::stereo(), true);

            buses = buses.withOutput ("Output", juce::AudioChannelSet::stereo(), true);
            break;

        case Plugin::Role::noAudio:
        default:
            break;
    }

    return buses;
}

const juce::String PluginNode::getName() const
{
    if (auto* inner = asContainer())
        return inner->getContainerName();

    return name;
}

bool PluginNode::isContainer() const noexcept           { return asContainer() != nullptr; }
ContainerPlugin* PluginNode::asContainer() const noexcept { return dynamic_cast<ContainerPlugin*> (plugin.get()); }

juce::String PluginNode::getFailure() const
{
    const juce::ScopedLock lock (failureLock);
    return failure;
}

void PluginNode::setFailure (const juce::String& why)
{
    const juce::ScopedLock lock (failureLock);
    failure = why;
    failed.store (why.isNotEmpty());
}

void PluginNode::whileSuspended (const std::function<void()>& fn)
{
    suspendProcessing (true);   // waits for a block the audio thread is in to end
    fn();
    suspendProcessing (false);
}

void PluginNode::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    if (plugin->isActive())
        plugin->deactivate();

    if (! plugin->activate (sampleRate, juce::jmax (1, samplesPerBlock)))
        setFailure (name + " couldn't start at " + juce::String (sampleRate / 1000.0, 1) + " kHz.");
    else if (getFailure().contains ("couldn't start"))
        setFailure ({});

    // Its latency, so the graph delays the paths beside it to match (it rebuilds when this changes).
    setLatencySamples (plugin->isActive() ? plugin->getLatencySamples() : 0);

    held.fill (0);
    lastMask = channelMask.load();
}

void PluginNode::releaseResources()
{
    if (plugin->isActive())
        plugin->deactivate();
}

void PluginNode::filterChannels (juce::MidiBuffer& midi) noexcept
{
    const auto mask = channelMask.load();
    filtered.clear();

    // Channels just turned off: their notes end.
    if (mask != lastMask)
    {
        const auto gone = static_cast<juce::uint16> (lastMask & ~mask);

        if (gone != 0)
            for (int note = 0; note < 128; ++note)
                for (int channel = 0; channel < 16; ++channel)
                    if ((held[static_cast<size_t> (note)] & gone & (1 << channel)) != 0)
                    {
                        filtered.addEvent (juce::MidiMessage::noteOff (channel + 1, note), 0);
                        held[static_cast<size_t> (note)] = static_cast<juce::uint16> (held[static_cast<size_t> (note)] & ~(1 << channel));
                    }

        lastMask = mask;
    }

    for (const auto metadata : midi)
    {
        const auto* data = metadata.data;
        const auto status = metadata.numBytes > 0 ? data[0] : 0;

        if (status >= 0x80 && status < 0xf0)
        {
            const auto channel = status & 0x0f;

            if ((mask & (1 << channel)) == 0)
                continue;

            if (metadata.numBytes >= 3)
            {
                const auto note = static_cast<size_t> (data[1] & 0x7f);

                if ((status & 0xf0) == 0x90 && data[2] > 0)
                    held[note] = static_cast<juce::uint16> (held[note] | (1 << channel));
                else if ((status & 0xf0) == 0x80 || ((status & 0xf0) == 0x90 && data[2] == 0))
                    held[note] = static_cast<juce::uint16> (held[note] & ~(1 << channel));
            }
        }

        filtered.addEvent (data, metadata.numBytes, metadata.samplePosition);
    }

    // Copied back rather than swapped, so this node's own buffer (sized as it was made) stays its own.
    midi.clear();
    midi.addEvents (filtered, 0, -1, 0);
}

void PluginNode::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const auto numSamples = buffer.getNumSamples();
    const auto outputs = juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels());

    if (numSamples <= 0)
        return;

    // Suspended while the message thread works on the plugin (the graph checks this before it
    // takes the callback lock, so it's checked again here, under it).
    if (isSuspended())
    {
        for (int channel = 0; channel < outputs; ++channel)
            buffer.clear (channel, 0, numSamples);

        midi.clear();
        return;
    }

    if (! midi.isEmpty())
    {
        midiInActive.store (true);
        midiInDecay = 1.0f;
    }
    else if ((midiInDecay *= 0.9f) < 0.01f)
    {
        midiInActive.store (false);
    }

    // Unmuted again: whatever it held when it was muted is let go first.
    if (notesOffOwed.exchange (false))
    {
        notesOff.clear();

        for (int note = 0; note < 128; ++note)
            for (int channel = 0; channel < 16; ++channel)
                if ((held[static_cast<size_t> (note)] & (1 << channel)) != 0)
                    notesOff.addEvent (juce::MidiMessage::noteOff (channel + 1, note), 0);

        held.fill (0);

        for (int channel = 1; channel <= 16; ++channel)
            notesOff.addEvent (juce::MidiMessage::controllerEvent (channel, 123, 0), 0);

        notesOff.addEvents (midi, 0, numSamples, 0);
        midi.clear();
        midi.addEvents (notesOff, 0, -1, 0);
    }

    filterChannels (midi);

    // The transport the plugin sees: the container's, or this node's own tempo and metre.
    const auto* transport = owner.getBlockTransport();
    clap_event_transport_t own {};

    if (! synced.load())
    {
        if (transport != nullptr)
            own = *transport;

        own.flags |= CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
        own.tempo = tempo.load();
        own.tempo_inc = 0.0;
        own.tsig_num = static_cast<uint16_t> (numerator.load());
        own.tsig_denom = static_cast<uint16_t> (denominator.load());
        transport = &own;
    }

    float* left = buffer.getNumChannels() > 0 ? buffer.getWritePointer (0) : nullptr;
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;
    bool processed = false;

    if (plugin->isActive() && ! failed.load())
    {
        processed = true;

        switch (plugin->getRole())
        {
            case Plugin::Role::instrument:
                plugin->process (nullptr, nullptr, left, right, numSamples, midi, transport);
                break;

            case Plugin::Role::effect:
                if (passThrough.load())
                {
                    processed = false;
                    break;
                }

                if (sidechain && buffer.getNumChannels() >= 4)
                {
                    plugin->clearSidechain (numSamples);
                    const auto room = juce::jmin (numSamples, plugin->getSidechainCapacity());

                    for (int channel = 0; channel < 2; ++channel)
                        if (auto* destination = plugin->getSidechainChannel (channel))
                            juce::FloatVectorOperations::copy (destination, buffer.getReadPointer (2 + channel), room);
                }

                plugin->process (left, right, left, right, numSamples, midi, transport);
                break;

            case Plugin::Role::noAudio:
            default:
                plugin->process (nullptr, nullptr, nullptr, nullptr, numSamples, midi, transport);
                break;
        }
    }
    else if (instrument)
    {
        for (int channel = 0; channel < outputs; ++channel)
            buffer.clear (channel, 0, numSamples);
    }

    // Its level in the rack's Mixer, and what the meter shows.
    if (outputs > 0)
        level.store (juce::jmax (applyGain (buffer, juce::jmin (outputs, 2), numSamples, lastGain, gain.load()), level.load() * 0.85f));

    // (The channels past the outputs, a sidechain's, are its sources' own buffers: left alone.)
    midi.clear();

    // Only what it played this block: one not processed (passed through, stopped) plays nothing.
    if (makesMidi && processed)
    {
        const auto& out = plugin->getMidiOutput();
        midi.addEvents (out, 0, numSamples, 0);

        if (! out.isEmpty())
        {
            midiOutActive.store (true);
            midiOutDecay = 1.0f;
        }
    }

    if (midi.isEmpty() && (midiOutDecay *= 0.9f) < 0.01f)
        midiOutActive.store (false);
}

void PluginNode::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Muted: an instrument is silent; an effect lets the sound through untouched. (Past the
    // outputs are its sources' own buffers: never written.)
    if (instrument)
        for (int channel = 0; channel < juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels()); ++channel)
            buffer.clear (channel, 0, buffer.getNumSamples());

    midi.clear();
    midiInActive.store (false);
    midiOutActive.store (false);
    level.store (0.0f);
}

//==============================================================================
// System tools
namespace ContainerTools
{
    Connector::Connector()
        : juce::AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                                 .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    float Connector::getVolumeDb() const noexcept
    {
        const auto normalised = volume.load();

        if (normalised <= 0.0f)
            return -100.0f;

        if (normalised <= 0.5f)
            return juce::jmap (normalised / 0.5f, -60.0f, 0.0f);

        return juce::jmap ((normalised - 0.5f) / 0.5f, 0.0f, 25.0f);
    }

    void Connector::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        const auto target = muted.load() || volume.load() <= 0.0f ? 0.0f : juce::Decibels::decibelsToGain (getVolumeDb());

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.applyGainRamp (channel, 0, buffer.getNumSamples(), lastGain, target);

        lastGain = target;
    }

    void Connector::getStateInformation (juce::MemoryBlock& destination)
    {
        juce::MemoryOutputStream stream (destination, false);
        stream.writeFloat (volume.load());
        stream.writeBool (muted.load());
    }

    void Connector::setStateInformation (const void* data, int size)
    {
        juce::MemoryInputStream stream (data, static_cast<size_t> (juce::jmax (0, size)), false);

        if (size >= static_cast<int> (sizeof (float)))
            setVolume (stream.readFloat());

        if (size >= static_cast<int> (sizeof (float) + 1))
            muted.store (stream.readBool());
    }

    //==============================================================================
    StereoMeter::StereoMeter()
        : juce::AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true))
    {
    }

    void StereoMeter::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        const auto blocksPerSecond = static_cast<float> (sampleRate) / static_cast<float> (juce::jmax (1, samplesPerBlock));
        levelDecay = std::pow (0.01f, 1.0f / (0.3f * blocksPerSecond));    // about 300 ms to fall
        peakHoldBlocks = static_cast<int> (1.5f * blocksPerSecond);       // peaks hold 1.5 s
        leftHold = rightHold = 0;
    }

    void StereoMeter::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        const auto numSamples = buffer.getNumSamples();
        const auto blockLeft = buffer.getNumChannels() >= 1 ? buffer.getMagnitude (0, 0, numSamples) : 0.0f;
        const auto blockRight = buffer.getNumChannels() >= 2 ? buffer.getMagnitude (1, 0, numSamples) : blockLeft;
        constexpr float floor = 0.0001f;   // -80 dB: below it the bar is empty

        const auto fall = [this] (std::atomic<float>& level, float block)
        {
            const auto now = level.load();
            const auto next = block > now ? block : now * levelDecay;
            level.store (next < floor ? 0.0f : next);
        };

        fall (leftLevel, blockLeft);
        fall (rightLevel, blockRight);

        const auto hold = [this] (std::atomic<float>& peak, int& counter, float block)
        {
            const auto now = peak.load();

            if (block >= now)
            {
                peak.store (block);
                counter = peakHoldBlocks;
            }
            else if (counter > 0)
            {
                --counter;
            }
            else
            {
                const auto next = now * 0.995f;
                peak.store (next < floor ? 0.0f : next);
            }
        };

        hold (leftPeak, leftHold, blockLeft);
        hold (rightPeak, rightHold, blockRight);

        if (blockLeft >= 0.99f)  leftClipping.store (true);
        if (blockRight >= 0.99f) rightClipping.store (true);

        // A meter only takes the sound in: its channels are its sources' own, left as they are.
    }

    void StereoMeter::resetClipping() noexcept
    {
        leftClipping.store (false);
        rightClipping.store (false);
        leftPeak.store (0.0f);
        rightPeak.store (0.0f);
    }

    //==============================================================================
    juce::String MidiMonitor::Event::toString() const
    {
        if (isCC)
            return juce::String::formatted ("CC %d, Ch %d, Val %d", ccNumber, channel, ccValue);

        return juce::String::formatted ("%d, %d, %d, %d", isNoteOn ? 144 : 128, channel, noteNumber, velocity);
    }

    MidiMonitor::MidiMonitor()
        : juce::AudioProcessor (BusesProperties())
    {
    }

    void MidiMonitor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        buffer.clear();
        bool any = false;

        {
            // The canvas reads at 5 Hz; when it holds the lock, this block's events wait for the next.
            const juce::SpinLock::ScopedTryLockType tryLock (lock);

            if (tryLock.isLocked())
            {
                for (const auto metadata : midi)
                {
                    const auto message = metadata.getMessage();
                    const auto channel = message.getChannel();

                    if (channel < 1 || channel > 16)
                        continue;

                    auto& event = events[static_cast<size_t> (channel - 1)];

                    if (message.isNoteOn() || message.isNoteOff())
                    {
                        event.isActive = true;
                        event.isNoteOn = message.isNoteOn();
                        event.isCC = false;
                        event.channel = channel;
                        event.noteNumber = message.getNoteNumber();
                        event.velocity = message.getVelocity();
                        any = true;
                    }
                    else if (message.isController())
                    {
                        event.isActive = true;
                        event.isCC = true;
                        event.isNoteOn = false;
                        event.channel = channel;
                        event.ccNumber = message.getControllerNumber();
                        event.ccValue = message.getControllerValue();
                        any = true;
                    }
                }
            }
        }

        if (any)
            changed.store (true);

        midi.clear();
    }

    std::array<MidiMonitor::Event, 16> MidiMonitor::getEvents() const
    {
        const juce::SpinLock::ScopedLockType hold (lock);
        return events;
    }

    //==============================================================================
    Missing::Missing (std::unique_ptr<juce::XmlElement> savedNode, int inputs, int outputs, bool midiInput, bool midiOutput)
        : juce::AudioProcessor ([&]
          {
              BusesProperties buses;

              if (inputs > 0)
                  buses = buses.withInput ("Input", juce::AudioChannelSet::canonicalChannelSet (juce::jmin (2, inputs)), true);

              if (inputs > 2)
                  buses = buses.withInput ("Sidechain", juce::AudioChannelSet::canonicalChannelSet (juce::jmin (2, inputs - 2)), true);

              if (outputs > 0)
                  buses = buses.withOutput ("Output", juce::AudioChannelSet::canonicalChannelSet (juce::jmin (2, outputs)), true);

              return buses;
          }()),
          saved (std::move (savedNode)),
          midiIn (midiInput),
          midiOut (midiOutput)
    {
        title = "Missing: " + saved->getStringAttribute ("name", "plugin");
    }
}

//==============================================================================
// ContainerPlugin
const std::vector<Plugin::Parameter> ContainerPlugin::noParameters;

juce::String ContainerPlugin::defaultName (bool isFx, bool nested)
{
    if (nested)
        return isFx ? "FX Container" : "Container";

    return isFx ? "Node Rack FX" : "Node Rack";
}

juce::Optional<juce::AudioPlayHead::PositionInfo> ContainerPlugin::GraphPlayHead::getPosition() const
{
    // The tools hear the song as the plugins do: the block's transport (the rack's own tempo and
    // metre when its T says so).
    const auto* transport = owner.getBlockTransport();

    if (transport == nullptr)
        return {};

    PositionInfo info;

    if ((transport->flags & CLAP_TRANSPORT_HAS_TEMPO) != 0 && transport->tempo > 0.0)
        info.setBpm (transport->tempo);

    if ((transport->flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE) != 0 && transport->tsig_num > 0 && transport->tsig_denom > 0)
        info.setTimeSignature (TimeSignature { static_cast<int> (transport->tsig_num), static_cast<int> (transport->tsig_denom) });

    if ((transport->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) != 0)
    {
        info.setPpqPosition (static_cast<double> (transport->song_pos_beats) / static_cast<double> (CLAP_BEATTIME_FACTOR));
        info.setPpqPositionOfLastBarStart (static_cast<double> (transport->bar_start) / static_cast<double> (CLAP_BEATTIME_FACTOR));
        info.setBarCount (transport->bar_number);
    }

    if ((transport->flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE) != 0)
        info.setTimeInSeconds (static_cast<double> (transport->song_pos_seconds) / static_cast<double> (CLAP_SECTIME_FACTOR));

    info.setIsPlaying ((transport->flags & CLAP_TRANSPORT_IS_PLAYING) != 0);
    info.setIsRecording ((transport->flags & CLAP_TRANSPORT_IS_RECORDING) != 0);
    info.setIsLooping ((transport->flags & CLAP_TRANSPORT_IS_LOOP_ACTIVE) != 0);
    return info;
}

ContainerPlugin::ContainerPlugin (bool isFx)
    : effect (isFx),
      descriptor (Containers::describe (isFx)),
      graph (std::make_unique<Graph>()),
      containerName (defaultName (isFx, false))
{
    inFile.push_back (descriptor);
    graph->setPlayHead (&playHead);   // the tools follow the song through it
    graph->setPlayConfigDetails (effect ? 4 : 0, 2, 48000.0, 512);
    makeIoNodes();
    placeIoNodes();

    // A new FX Container lets the track's sound through until something goes in between.
    if (effect && audioIn != nullptr && audioOut != nullptr)
    {
        graph->addConnection ({ { audioIn->nodeID, 0 }, { audioOut->nodeID, 0 } });
        graph->addConnection ({ { audioIn->nodeID, 1 }, { audioOut->nodeID, 1 } });
    }

    updateMidiOutUsed();
    prepareSidechain (512);

    // The macros, as the rack's parameters.
    for (int i = 0; i < numMacros; ++i)
    {
        Parameter p;
        p.id = static_cast<uint32_t> (i + 1);
        p.name = "Macro " + juce::String (i + 1);
        p.minValue = 0.0;
        p.maxValue = 1.0;
        p.defaultValue = 0.0;
        macroParameters.push_back (p);
        macroValues[static_cast<size_t> (i)].store (0.0);
    }
}

ContainerPlugin::~ContainerPlugin()
{
    {
        const juce::SpinLock::ScopedLockType lock (liveLock);
        liveLinks.clear();
    }

    screen.reset();   // the window goes before what it shows
    deactivate();
    graph->clear (Graph::UpdateKind::sync);
    graph->setPlayHead (nullptr);
    ++structureGeneration;   // a canvas that had entered it goes back up
}

juce::uint32 ContainerPlugin::getStructureGeneration() noexcept
{
    return structureGeneration.load();
}

const juce::File& ContainerPlugin::getSourceFile() const noexcept
{
    return Builtins::file();
}

bool ContainerPlugin::isIoNode (const Graph::Node* node) const noexcept
{
    return node != nullptr && (node == audioIn.get() || node == audioOut.get() || node == midiIn.get() || node == midiOut.get());
}

void ContainerPlugin::makeIoNodes()
{
    audioIn = effect ? graph->addNode (std::make_unique<IoProcessor> (IoProcessor::audioInputNode)) : nullptr;
    audioOut = graph->addNode (std::make_unique<IoProcessor> (IoProcessor::audioOutputNode));
    midiIn = graph->addNode (std::make_unique<IoProcessor> (IoProcessor::midiInputNode));
    midiOut = graph->addNode (std::make_unique<IoProcessor> (IoProcessor::midiOutputNode));

    if (audioIn != nullptr) audioIn->properties.set ("ioNodeName", "Container Audio In");
    audioOut->properties.set ("ioNodeName", "Container Audio Out");
    midiIn->properties.set ("ioNodeName", "Container MIDI In");
    midiOut->properties.set ("ioNodeName", "Container MIDI Out");
}

void ContainerPlugin::placeIoNodes()
{
    // On Colosseum's grid (40 px squares): inputs on row 3, outputs on row 11.
    if (audioIn != nullptr) { audioIn->properties.set ("x", 160.0); audioIn->properties.set ("y", 120.0); }
    audioOut->properties.set ("x", 160.0); audioOut->properties.set ("y", 440.0);
    midiIn->properties.set ("x", 680.0);   midiIn->properties.set ("y", 120.0);
    midiOut->properties.set ("x", 680.0);  midiOut->properties.set ("y", 440.0);
}

void ContainerPlugin::updateMidiOutUsed()
{
    bool used = false;

    if (midiOut != nullptr)
        for (const auto& connection : graph->getConnections())
            if (connection.destination.nodeID == midiOut->nodeID)
                used = true;

    midiOutUsed.store (used);
}

void ContainerPlugin::setContainerName (const juce::String& newName)
{
    const auto trimmed = newName.trim();

    if (trimmed.isEmpty() || trimmed == containerName)
        return;

    containerName = trimmed;
    descriptor.name = trimmed;
    changed();
}

void ContainerPlugin::changed()
{
    ++structureGeneration;   // every canvas showing it (or one around it) looks again
    stateChanged.store (true);
    updateMidiOutUsed();
    rebuildLiveLinks();      // the macros' controls, wherever their nodes are now

    if (onStructureChanged)
        onStructureChanged();

    if (auto* host = Containers::getHost())
        host->containerChanged (*this);
}

Graph::Node::Ptr ContainerPlugin::addPlugin (const juce::File& file, const juce::String& pluginId,
                                             Containers::Spot at, juce::String& error)
{
    auto plugin = loadAnyPlugin (file, pluginId, error);

    if (plugin == nullptr)
    {
        if (error.isEmpty())
            error = "That plugin couldn't be loaded.";

        return nullptr;
    }

    return addPluginNode (std::move (plugin), at);
}

Graph::Node::Ptr ContainerPlugin::addPluginNode (std::unique_ptr<Plugin> plugin, Containers::Spot at)
{
    if (plugin == nullptr)
        return nullptr;

    auto node = graph->addNode (std::make_unique<PluginNode> (std::move (plugin), newSlotId(), *this));

    if (node != nullptr)
    {
        node->properties.set ("x", static_cast<double> (at.x));
        node->properties.set ("y", static_cast<double> (at.y));
        changed();
    }

    return node;
}

void ContainerPlugin::toolEdited()
{
    stateChanged.store (true);

    if (auto* host = Containers::getHost())
        host->containerChanged (*this);
}

void ContainerPlugin::panic()
{
    // Notes off and sound off on every channel into the graph, on the next block; every plugin
    // lets go of what it holds; the latches open; the racks inside do the same.
    panicOwed.store (true);

    for (auto* node : graph->getNodes())
    {
        auto* proc = node->getProcessor();

        if (auto* pluginNode = dynamic_cast<PluginNode*> (proc))
        {
            pluginNode->noteMuted();

            if (auto* inner = pluginNode->asContainer())
                inner->panic();
        }
        else if (auto* latcher = dynamic_cast<NodeTools::Latcher*> (proc))
        {
            latcher->allNotesOff();
        }
    }
}

void ContainerPlugin::reset()
{
    // An empty rack of the same kind (an FX rack passes the sound straight through), keeping its name.
    const ContainerPlugin fresh (effect);
    auto xml = juce::parseXML (fresh.toXml());

    if (xml == nullptr)
        return;

    xml->setAttribute ("name", containerName);
    xml->setAttribute ("containerZoom", static_cast<double> (containerZoom));
    fromXml (xml->toString());
}

Graph::Node::Ptr ContainerPlugin::addTool (const juce::String& toolName, Containers::Spot at)
{
    auto tool = makeTool (toolName);

    if (tool == nullptr)
        return nullptr;

    auto node = graph->addNode (std::move (tool));

    if (node != nullptr)
    {
        node->properties.set ("x", static_cast<double> (at.x));
        node->properties.set ("y", static_cast<double> (at.y));
        changed();
    }

    return node;
}

void ContainerPlugin::removeNode (Graph::NodeID id)
{
    auto* node = graph->getNodeForId (id);

    if (node == nullptr || isIoNode (node))
        return;

    // Its window goes now (the graph keeps what it removed until the audio thread lets go).
    if (auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor()))
        pluginNode->getPlugin().hideNativeScreen();

    // The macros let go of it first: the audio thread never reaches a node that's gone.
    macroLinks.erase (std::remove_if (macroLinks.begin(), macroLinks.end(),
                                      [id] (const MacroLink& link) { return link.kind != MacroLink::Kind::output && link.nodeUid == id.uid; }),
                      macroLinks.end());
    rebuildLiveLinks (id.uid);

    graph->disconnectNode (id);
    graph->removeNode (id);
    changed();
}

bool ContainerPlugin::connect (Graph::NodeID from, int fromChannel, Graph::NodeID to, int toChannel)
{
    const auto added = graph->addConnection ({ { from, fromChannel }, { to, toChannel } });

    if (added)
        changed();

    return added;
}

bool ContainerPlugin::connectMidi (Graph::NodeID from, Graph::NodeID to)
{
    return connect (from, Graph::midiChannelIndex, to, Graph::midiChannelIndex);
}

void ContainerPlugin::disconnect (const Graph::Connection& connection)
{
    if (graph->removeConnection (connection))
        changed();
}

void ContainerPlugin::disconnectNode (Graph::NodeID id)
{
    if (graph->disconnectNode (id))
        changed();
}

void ContainerPlugin::setAudioInputOn (bool on)
{
    if (audioInputOn.exchange (on) != on)
        changed();
}

bool ContainerPlugin::isOff (const Graph::Node* node) const noexcept
{
    if (node == nullptr)
        return false;

    return node == audioIn.get() ? ! audioInputOn.load() : node->isBypassed();
}

void ContainerPlugin::setNodeMuted (Graph::NodeID id, bool shouldMute)
{
    if (auto* node = graph->getNodeForId (id))
    {
        if (node->isBypassed() == shouldMute)
            return;

        if (auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor()))
            pluginNode->noteMuted();

        node->setBypassed (shouldMute);
        changed();
    }
}

int ContainerPlugin::getPluginMuted (const Plugin& plugin) const
{
    for (auto* node : graph->getNodes())
    {
        if (auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor()))
        {
            if (&pluginNode->getPlugin() == &plugin)
                return node->isBypassed() ? 1 : 0;

            if (auto* inner = pluginNode->asContainer())
                if (const auto muted = inner->getPluginMuted (plugin); muted >= 0)
                    return muted;
        }
    }

    return -1;
}

bool ContainerPlugin::setPluginMuted (const Plugin& plugin, bool muted)
{
    for (auto* node : graph->getNodes())
    {
        if (auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor()))
        {
            if (&pluginNode->getPlugin() == &plugin)
            {
                setNodeMuted (node->nodeID, muted);
                return true;
            }

            if (auto* inner = pluginNode->asContainer())
                if (inner->setPluginMuted (plugin, muted))
                    return true;
        }
    }

    return false;
}

void ContainerPlugin::forEachPluginNode (const std::function<void (PluginNode&)>& fn) const
{
    for (auto* node : graph->getNodes())
    {
        if (auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor()))
        {
            fn (*pluginNode);

            if (auto* inner = pluginNode->asContainer())
                inner->forEachPluginNode (fn);
        }
    }
}

Plugin* ContainerPlugin::findPlugin (int slotId) const
{
    Plugin* found = nullptr;

    forEachPluginNode ([&] (PluginNode& node)
    {
        if (found == nullptr && node.slotId == slotId)
            found = &node.getPlugin();
    });

    return found;
}

PluginNode* ContainerPlugin::findNode (const Plugin& plugin) const
{
    PluginNode* found = nullptr;

    forEachPluginNode ([&] (PluginNode& node)
    {
        if (found == nullptr && &node.getPlugin() == &plugin)
            found = &node;
    });

    return found;
}

//==============================================================================
// State
namespace
{
    juce::String base64Of (const juce::MemoryBlock& block)   { return block.toBase64Encoding(); }

    juce::MemoryBlock blockOf (const juce::XmlElement* stateXml)
    {
        juce::MemoryBlock block;

        if (stateXml != nullptr)
            block.fromBase64Encoding (stateXml->getAllSubText().trim());

        return block;
    }
}

juce::String ContainerPlugin::toXml() const
{
    juce::XmlElement root ("ContainerPreset");
    root.setAttribute ("name", containerName);
    root.setAttribute ("version", 1);
    root.setAttribute ("kind", effect ? "effect" : "instrument");
    root.setAttribute ("muted", muted.load());
    root.setAttribute ("transportSynced", synced.load());
    root.setAttribute ("customTempo", tempo.load());
    root.setAttribute ("customTimeSigNum", numerator.load());
    root.setAttribute ("customTimeSigDen", denominator.load());
    root.setAttribute ("containerZoom", static_cast<double> (containerZoom));
    root.setAttribute ("panX", static_cast<double> (containerPan.x));
    root.setAttribute ("panY", static_cast<double> (containerPan.y));
    root.setAttribute ("outputGain", static_cast<double> (outputGain.load()));

    auto* nodesXml = root.createNewChildElement ("Nodes");

    for (auto* node : graph->getNodes())
    {
        auto* proc = node->getProcessor();

        // A missing plugin is written back as it was read, where it now sits.
        if (auto* missing = dynamic_cast<ContainerTools::Missing*> (proc))
        {
            auto* copy = new juce::XmlElement (missing->getSaved());
            copy->setAttribute ("id", static_cast<int> (node->nodeID.uid));
            copy->setAttribute ("x", static_cast<double> (node->properties["x"]));
            copy->setAttribute ("y", static_cast<double> (node->properties["y"]));
            copy->setAttribute ("bypassed", node->isBypassed());
            nodesXml->addChildElement (copy);
            continue;
        }

        auto* nodeXml = nodesXml->createNewChildElement ("Node");
        nodeXml->setAttribute ("id", static_cast<int> (node->nodeID.uid));
        nodeXml->setAttribute ("x", static_cast<double> (node->properties["x"]));
        nodeXml->setAttribute ("y", static_cast<double> (node->properties["y"]));
        nodeXml->setAttribute ("bypassed", isOff (node));

        if (node == audioIn.get())        { nodeXml->setAttribute ("type", "AudioInput");  continue; }
        if (node == audioOut.get())       { nodeXml->setAttribute ("type", "AudioOutput"); continue; }
        if (node == midiIn.get())         { nodeXml->setAttribute ("type", "MidiInput");   continue; }
        if (node == midiOut.get())        { nodeXml->setAttribute ("type", "MidiOutput");  continue; }

        const auto toolName = toolNameOf (proc);

        if (toolName.isNotEmpty())
        {
            nodeXml->setAttribute ("type", "SystemTool");
            nodeXml->setAttribute ("toolName", toolName);
            juce::MemoryBlock state;
            proc->getStateInformation (state);

            if (state.getSize() > 0)
                nodeXml->createNewChildElement ("State")->addTextElement (base64Of (state));

            continue;
        }

        if (auto* pluginNode = dynamic_cast<PluginNode*> (proc))
        {
            auto& plugin = pluginNode->getPlugin();
            const auto& d = plugin.getDescriptor();
            nodeXml->setAttribute ("type", "Plugin");
            nodeXml->setAttribute ("name", d.name);
            nodeXml->setAttribute ("pluginId", d.id);
            nodeXml->setAttribute ("file", Builtins::isBuiltinId (d.id) ? juce::String() : plugin.getSourceFile().getFullPathName());
            nodeXml->setAttribute ("format", plugin.getFormatName());
            nodeXml->setAttribute ("inputs", proc->getTotalNumInputChannels());
            nodeXml->setAttribute ("outputs", proc->getTotalNumOutputChannels());
            nodeXml->setAttribute ("midiOut", proc->producesMidi());
            nodeXml->setAttribute ("midiChannelMask", pluginNode->getMidiChannelMask());
            nodeXml->setAttribute ("passThrough", pluginNode->isPassThrough());
            nodeXml->setAttribute ("transportSynced", pluginNode->isTransportSynced());
            nodeXml->setAttribute ("customTempo", pluginNode->getCustomTempo());
            nodeXml->setAttribute ("customTimeSigNum", pluginNode->getCustomTimeSigNumerator());
            nodeXml->setAttribute ("customTimeSigDen", pluginNode->getCustomTimeSigDenominator());
            nodeXml->setAttribute ("gain", static_cast<double> (pluginNode->getGain()));

            juce::MemoryBlock state;

            if (plugin.canSaveState() && plugin.saveState (state) && state.getSize() > 0)
                nodeXml->createNewChildElement ("State")->addTextElement (base64Of (state));
        }
    }

    // The macros: each one's name and value, and the controls they drive.
    auto* macrosXml = root.createNewChildElement ("Macros");

    for (int i = 0; i < numMacros; ++i)
    {
        auto* m = macrosXml->createNewChildElement ("Macro");
        m->setAttribute ("index", i);
        m->setAttribute ("name", macroNames[static_cast<size_t> (i)]);
        m->setAttribute ("value", macroValues[static_cast<size_t> (i)].load());
    }

    for (const auto& link : macroLinks)
    {
        auto* l = macrosXml->createNewChildElement ("Link");
        l->setAttribute ("macro", link.macro);
        l->setAttribute ("kind", link.kind == MacroLink::Kind::parameter ? "parameter" : link.kind == MacroLink::Kind::level ? "level" : "output");
        l->setAttribute ("node", static_cast<int> (link.nodeUid));
        l->setAttribute ("param", juce::String (static_cast<juce::int64> (link.paramId)));
        l->setAttribute ("from", static_cast<double> (link.from));
        l->setAttribute ("to", static_cast<double> (link.to));
        l->setAttribute ("inverted", link.inverted);
    }

    auto* connectionsXml = root.createNewChildElement ("Connections");

    for (const auto& connection : graph->getConnections())
    {
        auto* c = connectionsXml->createNewChildElement ("Connection");
        c->setAttribute ("srcNode", static_cast<int> (connection.source.nodeID.uid));
        c->setAttribute ("srcCh", connection.source.channelIndex);
        c->setAttribute ("srcMidi", connection.source.isMIDI());
        c->setAttribute ("dstNode", static_cast<int> (connection.destination.nodeID.uid));
        c->setAttribute ("dstCh", connection.destination.channelIndex);
        c->setAttribute ("dstMidi", connection.destination.isMIDI());
    }

    return root.toString();
}

Graph::Node::Ptr ContainerPlugin::nodeFromXml (const juce::XmlElement& xml)
{
    const auto type = xml.getStringAttribute ("type");

    if (type == "SystemTool")
    {
        const auto toolName = xml.getStringAttribute ("toolName");
        auto tool = makeTool (toolName);

        if (tool == nullptr)
        {
            problems.add ("The system tool " + toolName + " isn't in this Sonify");
            return nullptr;
        }

        const auto state = blockOf (xml.getChildByName ("State"));

        if (state.getSize() > 0)
            tool->setStateInformation (state.getData(), static_cast<int> (state.getSize()));

        return graph->addNode (std::move (tool));
    }

    if (type != "Plugin")
        return nullptr;

    const auto name = xml.getStringAttribute ("name", "plugin");
    const auto pluginId = xml.getStringAttribute ("pluginId");
    const juce::String path = xml.getStringAttribute ("file");
    const auto file = juce::File::isAbsolutePath (path) ? juce::File (path) : Builtins::file();
    const auto fileName = path.fromLastOccurrenceOf ("\\", false, false).fromLastOccurrenceOf ("/", false, false);

    juce::String error;
    auto plugin = loadAnyPlugin (file, pluginId, error);

    if (plugin != nullptr)
    {
        const auto state = blockOf (xml.getChildByName ("State"));

        if (state.getSize() > 0 && plugin->canSaveState() && ! plugin->loadState (state))
            problems.add (name + "'s settings couldn't be restored");

        // A container inside says what's missing in it too.
        if (auto* inner = dynamic_cast<ContainerPlugin*> (plugin.get()))
            for (const auto& problem : inner->getProblems())
                problems.add (inner->getContainerName() + ": " + problem);

        auto node = graph->addNode (std::make_unique<PluginNode> (std::move (plugin), newSlotId(), *this));

        if (auto* pluginNode = node != nullptr ? dynamic_cast<PluginNode*> (node->getProcessor()) : nullptr)
        {
            pluginNode->setMidiChannelMask (xml.getIntAttribute ("midiChannelMask", 0xffff));
            pluginNode->setPassThrough (xml.getBoolAttribute ("passThrough", false));
            pluginNode->setTransportSynced (xml.getBoolAttribute ("transportSynced", true));
            pluginNode->setCustomTempo (xml.getDoubleAttribute ("customTempo", 120.0));
            pluginNode->setCustomTimeSignature (xml.getIntAttribute ("customTimeSigNum", 4), xml.getIntAttribute ("customTimeSigDen", 4));
            pluginNode->setGain (static_cast<float> (xml.getDoubleAttribute ("gain", 1.0)));
        }

        return node;
    }

    // Not here: kept as it was, so saving again keeps it.
    problems.add (name + (fileName.isNotEmpty() ? " (" + fileName + ")" : juce::String()) + " isn't on this computer");
    auto missing = std::make_unique<ContainerTools::Missing> (std::make_unique<juce::XmlElement> (xml),
                                                              xml.getIntAttribute ("inputs", 2), xml.getIntAttribute ("outputs", 2),
                                                              true, xml.getBoolAttribute ("midiOut", false));
    missing->setWhy (error);
    return graph->addNode (std::move (missing));
}

bool ContainerPlugin::fromXml (const juce::String& text)
{
    const auto xml = juce::parseXML (text);

    if (xml == nullptr || ! xml->hasTagName ("ContainerPreset"))
        return false;

    problems.clear();
    containerName = xml->getStringAttribute ("name", containerName).trim();

    if (containerName.isEmpty())
        containerName = defaultName (effect, false);

    descriptor.name = containerName;
    muted.store (xml->getBoolAttribute ("muted", false));
    synced.store (xml->getBoolAttribute ("transportSynced", true));
    setCustomTempo (xml->getDoubleAttribute ("customTempo", 120.0));
    setCustomTimeSignature (xml->getIntAttribute ("customTimeSigNum", 4), xml->getIntAttribute ("customTimeSigDen", 4));
    containerZoom = static_cast<float> (xml->getDoubleAttribute ("containerZoom", 0.7));
    containerPan = { static_cast<float> (xml->getDoubleAttribute ("panX", 0.0)), static_cast<float> (xml->getDoubleAttribute ("panY", 0.0)) };
    setOutputGain (static_cast<float> (xml->getDoubleAttribute ("outputGain", 1.0)));

    // Everything goes (its windows too); the I/O comes back where it was, the nodes after it.
    // The macros let go of the nodes first.
    {
        const juce::SpinLock::ScopedLockType lock (liveLock);
        liveLinks.clear();
    }

    macroLinks.clear();
    hideNodeScreens();
    graph->clear();
    ++structureGeneration;   // now, not only at the end: a plugin loading below may show a dialog (and canvases paint)
    makeIoNodes();
    placeIoNodes();
    audioInputOn.store (true);

    std::map<int, Graph::NodeID> ids;

    if (auto* nodesXml = xml->getChildByName ("Nodes"))
    {
        for (auto* nodeXml : nodesXml->getChildIterator())
        {
            if (! nodeXml->hasTagName ("Node"))
                continue;

            const auto type = nodeXml->getStringAttribute ("type");
            Graph::Node::Ptr node;

            if (type == "AudioInput")        node = audioIn;
            else if (type == "AudioOutput")  node = audioOut;
            else if (type == "MidiInput")    node = midiIn;
            else if (type == "MidiOutput")   node = midiOut;
            else                             node = nodeFromXml (*nodeXml);

            if (node == nullptr)
                continue;

            ids[nodeXml->getIntAttribute ("id")] = node->nodeID;
            node->properties.set ("x", nodeXml->getDoubleAttribute ("x", 100.0));
            node->properties.set ("y", nodeXml->getDoubleAttribute ("y", 100.0));

            const auto off = nodeXml->getBoolAttribute ("bypassed", false);

            if (node == audioIn)
                audioInputOn.store (! off);
            else if (node == audioOut || ! isIoNode (node.get()))
                node->setBypassed (off);
        }
    }

    if (auto* connectionsXml = xml->getChildByName ("Connections"))
    {
        for (auto* c : connectionsXml->getChildIterator())
        {
            const auto from = ids.find (c->getIntAttribute ("srcNode"));
            const auto to = ids.find (c->getIntAttribute ("dstNode"));

            if (from == ids.end() || to == ids.end())
                continue;

            if (c->getBoolAttribute ("srcMidi"))
                graph->addConnection ({ { from->second, Graph::midiChannelIndex }, { to->second, Graph::midiChannelIndex } });
            else
                graph->addConnection ({ { from->second, c->getIntAttribute ("srcCh") }, { to->second, c->getIntAttribute ("dstCh") } });
        }
    }

    // The macros (none in a patch from before them: each back to its own name, at 0).
    for (int i = 0; i < numMacros; ++i)
    {
        macroNames[static_cast<size_t> (i)].clear();
        macroValues[static_cast<size_t> (i)].store (0.0);
    }

    if (auto* macrosXml = xml->getChildByName ("Macros"))
    {
        for (auto* m : macrosXml->getChildWithTagNameIterator ("Macro"))
        {
            const auto i = m->getIntAttribute ("index", -1);

            if (i < 0 || i >= numMacros)
                continue;

            macroNames[static_cast<size_t> (i)] = m->getStringAttribute ("name").trim().substring (0, 40);
            macroValues[static_cast<size_t> (i)].store (juce::jlimit (0.0, 1.0, m->getDoubleAttribute ("value", 0.0)));
        }

        for (auto* l : macrosXml->getChildWithTagNameIterator ("Link"))
        {
            MacroLink link;
            link.macro = l->getIntAttribute ("macro", -1);
            const auto kind = l->getStringAttribute ("kind");
            link.kind = kind == "level" ? MacroLink::Kind::level : kind == "output" ? MacroLink::Kind::output : MacroLink::Kind::parameter;
            link.paramId = static_cast<uint32_t> (l->getStringAttribute ("param").getLargeIntValue());
            link.from = static_cast<float> (juce::jlimit (0.0, 1.0, l->getDoubleAttribute ("from", 0.0)));
            link.to = static_cast<float> (juce::jlimit (0.0, 1.0, l->getDoubleAttribute ("to", 1.0)));
            link.inverted = l->getBoolAttribute ("inverted", false);

            if (link.macro < 0 || link.macro >= numMacros)
                continue;

            // Its node, as this rack numbers it now.
            if (link.kind != MacroLink::Kind::output)
            {
                const auto node = ids.find (l->getIntAttribute ("node"));

                if (node == ids.end())
                    continue;

                link.nodeUid = node->second.uid;
            }

            macroLinks.push_back (link);
        }
    }

    for (int i = 0; i < numMacros; ++i)
        macroParameters[static_cast<size_t> (i)].name = getMacroName (i);

    macroNamesChanged.store (true);
    changed();
    return true;
}

bool ContainerPlugin::savePreset (const juce::File& file) const
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (toXml());
}

bool ContainerPlugin::loadPreset (const juce::File& file)
{
    return file.existsAsFile() && fromXml (file.loadFileAsString());
}

bool ContainerPlugin::saveState (juce::MemoryBlock& destination)
{
    destination.reset();
    const auto text = toXml();
    destination.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
    return true;
}

bool ContainerPlugin::loadState (const juce::MemoryBlock& source)
{
    const auto loaded = fromXml (source.toString());
    consumeStateChanged();   // as the song has it: not a change (its plugins' too)
    return loaded;
}

bool ContainerPlugin::consumeStateChanged() noexcept
{
    auto any = stateChanged.exchange (false);

    for (auto* node : graph->getNodes())
        if (auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor()))
            any = pluginNode->getPlugin().consumeStateChanged() || any;

    return any;
}

//==============================================================================
// Audio
bool ContainerPlugin::activate (double sampleRate, int maxBlockSize)
{
    deactivate();

    rate = sampleRate;
    blockSize = juce::jmax (1, maxBlockSize);
    work.setSize (effect ? 4 : 2, blockSize, false, true, false);
    workMidi.ensureSize (midiBufferBytes);
    midiOutput.ensureSize (midiBufferBytes);
    prepareSidechain (blockSize);

    graph->setPlayConfigDetails (effect ? 4 : 0, 2, rate, blockSize);
    graph->prepareToPlay (rate, blockSize);   // prepares (activates) every node, here on the message thread
    active.store (true);
    return true;
}

void ContainerPlugin::deactivate()
{
    if (! active.exchange (false))
        return;

    graph->releaseResources();
}

void ContainerPlugin::stopProcessing()
{
    // The engine only reaches the plugins on its tracks: the ones in here hear it from the container.
    for (auto* node : graph->getNodes())
        if (auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor()))
            pluginNode->getPlugin().stopProcessing();
}

void ContainerPlugin::process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                               int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept
{
    midiOutput.clear();
    const auto n = juce::jmin (numSamples, work.getNumSamples());

    if (! active.load() || n <= 0)
    {
        if (outLeft != nullptr)  juce::FloatVectorOperations::clear (outLeft, numSamples);
        if (outRight != nullptr) juce::FloatVectorOperations::clear (outRight, numSamples);
        return;
    }

    if (effect)
    {
        if (inLeft != nullptr)  work.copyFrom (0, 0, inLeft, n);  else work.clear (0, 0, n);
        if (inRight != nullptr) work.copyFrom (1, 0, inRight, n); else work.copyFrom (1, 0, work, 0, 0, n);

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto* sc = channel < sidechain.getNumChannels() && sidechain.getNumSamples() >= n ? sidechain.getReadPointer (channel) : nullptr;

            if (sc != nullptr)
                work.copyFrom (2 + channel, 0, sc, n);
            else
                work.clear (2 + channel, 0, n);
        }

        // Audio In turned off: nothing comes in, the sidechain neither.
        if (! audioInputOn.load())
            work.clear (0, n);
    }
    else
    {
        work.clear (0, n);
    }

    workMidi.clear();

    // PANIC: every note off and every sound off, on every channel, before anything else.
    if (panicOwed.exchange (false))
        for (int channel = 1; channel <= 16; ++channel)
        {
            workMidi.addEvent (juce::MidiMessage::controllerEvent (channel, 64, 0), 0);
            workMidi.addEvent (juce::MidiMessage::allNotesOff (channel), 0);
            workMidi.addEvent (juce::MidiMessage::allSoundOff (channel), 0);
        }

    workMidi.addEvents (midi, 0, n, 0);

    // Macros the song's automation moved: their controls follow before the graph plays.
    if (const auto moved = macrosFromAudio.exchange (0); moved != 0)
    {
        const juce::SpinLock::ScopedTryLockType lock (liveLock);

        if (lock.isLocked())
        {
            for (auto& link : liveLinks)
            {
                if (((moved >> link.macro) & 1u) == 0)
                    continue;

                const auto amount = linkAmount (link.from, link.to, link.inverted, macroValues[static_cast<size_t> (link.macro)].load());

                if (link.kind == MacroLink::Kind::parameter && link.plugin != nullptr)
                {
                    auto value = link.minValue + amount * (link.maxValue - link.minValue);

                    if (link.stepped)
                        value = std::round (value);

                    link.plugin->setParameterFromAudio (link.paramId, link.hint, value);
                }
                else if (link.kind == MacroLink::Kind::level && link.node != nullptr)
                {
                    link.node->setGain (levelGain (amount));
                }
                else if (link.kind == MacroLink::Kind::output)
                {
                    setOutputGain (levelGain (amount));
                }
            }
        }
        else
        {
            macrosFromAudio.fetch_or (moved);   // the links are being made again: next block
        }
    }

    // The nodes' transport: the song's, or this container's own tempo and metre.
    if (synced.load())
    {
        blockTransport = transport;
    }
    else
    {
        ownTransport = transport != nullptr ? *transport : clap_event_transport_t {};
        ownTransport.flags |= CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
        ownTransport.tempo = tempo.load();
        ownTransport.tempo_inc = 0.0;
        ownTransport.tsig_num = static_cast<uint16_t> (numerator.load());
        ownTransport.tsig_denom = static_cast<uint16_t> (denominator.load());
        blockTransport = &ownTransport;
    }

    juce::AudioBuffer<float> view (work.getArrayOfWritePointers(), work.getNumChannels(), n);
    graph->processBlock (view, workMidi);
    blockTransport = nullptr;

    const bool silent = muted.load();

    // The Mixer's Output fader, and its meter.
    const auto peak = applyGain (view, 2, n, lastOutputGain, outputGain.load());
    outputLevel.store (silent ? 0.0f : juce::jmax (peak, outputLevel.load() * 0.85f));

    for (int channel = 0; channel < 2; ++channel)
        if (auto* out = channel == 0 ? outLeft : outRight)
        {
            if (silent)
                juce::FloatVectorOperations::clear (out, numSamples);
            else
                juce::FloatVectorOperations::copy (out, view.getReadPointer (channel), n);
        }

    if (! silent && midiOutUsed.load())
        midiOutput.addEvents (workMidi, 0, n, 0);
}

bool ContainerPlugin::showNativeScreen (const juce::String& title)
{
    if (screen == nullptr)
        if (auto& factory = screenFactory())
            screen = factory (*this);

    if (screen == nullptr)
        return false;

    screen->show (title);
    return true;
}

void ContainerPlugin::hideNativeScreen()
{
    if (screen != nullptr)
        screen->hide();

    // Its plugins' windows go with it (a container inside closes its own).
    hideNodeScreens();
}

void ContainerPlugin::hideNodeScreens()
{
    for (auto* node : graph->getNodes())
        if (auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor()))
            pluginNode->getPlugin().hideNativeScreen();
}

Plugin::Requests ContainerPlugin::takeRequests() noexcept
{
    Requests forHost;
    bool rescanned = false;

    // What the plugins inside ask of the host, the container does for them.
    for (auto* node : graph->getNodes())
    {
        auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor());

        if (pluginNode == nullptr)
            continue;

        auto& plugin = pluginNode->getPlugin();
        const auto requests = plugin.takeRequests();
        juce::String error;

        if (requests.processError && plugin.getError (error))
        {
            pluginNode->setFailure (plugin.getDescriptor().name + " stopped: " + error);
            setNodeMuted (node->nodeID, true);
            ++structureGeneration;   // the canvas shows it stopped, even one muted already
            continue;
        }

        if (requests.callback)
            plugin.runOnMainThread();

        if (requests.restart && plugin.isActive() && rate > 0.0)
            pluginNode->whileSuspended ([&] { plugin.deactivate(); plugin.activate (rate, blockSize); });

        // Its parameters change under the audio thread's feet: only while it's out of the way.
        // The page then shows them again.
        if (requests.rescan)
        {
            pluginNode->whileSuspended ([&] { plugin.rescanParameters(); });
            forHost.rescan = true;
            rescanned = true;
        }

        // As the engine does: queued values are flushed only while nothing plays them (playing,
        // the plugin takes them as it processes).
        if (requests.flush && ! active.load())
            plugin.flushQueuedParameters();
    }

    // A plugin's parameters read again: the macros find theirs again.
    if (rescanned)
        rebuildLiveLinks();

    // A macro renamed: the host shows its parameters' new names.
    if (macroNamesChanged.exchange (false))
        forHost.rescan = true;

    return forHost;
}

//==============================================================================
// Macros
double ContainerPlugin::linkAmount (float from, float to, bool inverted, double macro) noexcept
{
    const auto m = juce::jlimit (0.0, 1.0, inverted ? 1.0 - macro : macro);
    return juce::jlimit (0.0, 1.0, static_cast<double> (from) + (static_cast<double> (to) - from) * m);
}

float ContainerPlugin::levelGain (double amount) noexcept
{
    // A level's range: silence at 0, then -60 dB .. +6 dB.
    if (amount <= 0.0005)
        return 0.0f;

    return juce::Decibels::decibelsToGain (static_cast<float> (-60.0 + 66.0 * amount));
}

juce::String ContainerPlugin::getMacroName (int macro) const
{
    if (macro < 0 || macro >= numMacros)
        return {};

    const auto& name = macroNames[static_cast<size_t> (macro)];
    return name.isNotEmpty() ? name : "Macro " + juce::String (macro + 1);
}

void ContainerPlugin::setMacroName (int macro, const juce::String& name)
{
    if (macro < 0 || macro >= numMacros)
        return;

    auto trimmed = name.trim().substring (0, 40);

    if (trimmed == "Macro " + juce::String (macro + 1))
        trimmed.clear();

    if (trimmed == macroNames[static_cast<size_t> (macro)])
        return;

    macroNames[static_cast<size_t> (macro)] = trimmed;
    macroParameters[static_cast<size_t> (macro)].name = getMacroName (macro);
    macroNamesChanged.store (true);
    macrosEdited();
}

double ContainerPlugin::getMacroValue (int macro) const noexcept
{
    return macro >= 0 && macro < numMacros ? macroValues[static_cast<size_t> (macro)].load() : 0.0;
}

void ContainerPlugin::setMacroValue (int macro, double value)
{
    if (macro < 0 || macro >= numMacros || ! std::isfinite (value))
        return;

    macroValues[static_cast<size_t> (macro)].store (juce::jlimit (0.0, 1.0, value));
    applyMacro (macro);
    macrosEdited();
}

juce::String ContainerPlugin::getParameterText (int index, double value)
{
    juce::ignoreUnused (index);
    return juce::String (juce::roundToInt (juce::jlimit (0.0, 1.0, value) * 100.0)) + "%";
}

double ContainerPlugin::queueParameter (int index, double value)
{
    setMacroValue (index, value);
    return getMacroValue (index);
}

void ContainerPlugin::setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept
{
    juce::ignoreUnused (indexHint);

    if (paramId < 1 || paramId > numMacros || ! std::isfinite (value))
        return;

    const auto macro = static_cast<int> (paramId) - 1;
    const auto clamped = juce::jlimit (0.0, 1.0, value);

    if (juce::exactlyEqual (macroValues[static_cast<size_t> (macro)].exchange (clamped), clamped))
        return;

    macrosFromAudio.fetch_or (1u << macro);
}

void ContainerPlugin::applyMacro (int macro)
{
    const auto value = getMacroValue (macro);

    for (const auto& link : macroLinks)
    {
        if (link.macro != macro)
            continue;

        const auto amount = linkAmount (link.from, link.to, link.inverted, value);

        if (link.kind == MacroLink::Kind::output)
        {
            setOutputGain (levelGain (amount));
            continue;
        }

        auto* node = graph->getNodeForId (Graph::NodeID { link.nodeUid });
        auto* pluginNode = node != nullptr ? dynamic_cast<PluginNode*> (node->getProcessor()) : nullptr;

        if (pluginNode == nullptr)
            continue;

        if (link.kind == MacroLink::Kind::level)
        {
            pluginNode->setGain (levelGain (amount));
            continue;
        }

        auto& plugin = pluginNode->getPlugin();
        const auto index = plugin.findParameter (link.paramId);

        if (index < 0)
            continue;

        const auto& p = plugin.getParameters()[static_cast<size_t> (index)];
        auto target = p.minValue + amount * (p.maxValue - p.minValue);

        if (p.stepped)
            target = std::round (target);

        plugin.queueParameter (index, target);
    }
}

void ContainerPlugin::macrosEdited()
{
    toolEdited();   // the song has unsaved changes

    if (onMacrosChanged)
        onMacrosChanged();
}

void ContainerPlugin::addMacroLink (const MacroLink& link)
{
    if (link.macro < 0 || link.macro >= numMacros)
        return;

    for (auto& existing : macroLinks)
    {
        if (existing.sameControl (link))
        {
            existing = link;
            rebuildLiveLinks();
            macrosEdited();
            return;
        }
    }

    macroLinks.push_back (link);
    rebuildLiveLinks();
    macrosEdited();
}

void ContainerPlugin::setMacroLink (int index, const MacroLink& link)
{
    if (index < 0 || index >= static_cast<int> (macroLinks.size()))
        return;

    macroLinks[static_cast<size_t> (index)] = link;
    rebuildLiveLinks();
    applyMacro (link.macro);
    macrosEdited();
}

void ContainerPlugin::removeMacroLink (int index)
{
    if (index < 0 || index >= static_cast<int> (macroLinks.size()))
        return;

    macroLinks.erase (macroLinks.begin() + index);
    rebuildLiveLinks();
    macrosEdited();
}

juce::String ContainerPlugin::describeMacroLink (const MacroLink& link) const
{
    if (link.kind == MacroLink::Kind::output)
        return "Rack Output";

    auto* node = graph->getNodeForId (Graph::NodeID { link.nodeUid });
    auto* pluginNode = node != nullptr ? dynamic_cast<PluginNode*> (node->getProcessor()) : nullptr;

    if (pluginNode == nullptr)
        return "(gone)";

    auto& plugin = pluginNode->getPlugin();
    const auto name = plugin.getDescriptor().name;

    if (link.kind == MacroLink::Kind::level)
        return name + ": Level";

    const auto index = plugin.findParameter (link.paramId);
    return name + ": " + (index >= 0 ? plugin.getParameters()[static_cast<size_t> (index)].name : juce::String ("(a parameter it no longer has)"));
}

bool ContainerPlugin::takeTouchedControl (juce::uint32& nodeUid, uint32_t& paramId)
{
    bool found = false;

    // Each plugin's touch is taken (so an old one doesn't count later); the first one wins.
    for (auto* node : graph->getNodes())
    {
        auto* pluginNode = dynamic_cast<PluginNode*> (node->getProcessor());

        if (pluginNode == nullptr)
            continue;

        auto& plugin = pluginNode->getPlugin();
        const auto index = plugin.takeTouchedParameter();

        if (! found && index >= 0 && index < static_cast<int> (plugin.getParameters().size()))
        {
            nodeUid = node->nodeID.uid;
            paramId = plugin.getParameters()[static_cast<size_t> (index)].id;
            found = true;
        }
    }

    return found;
}

void ContainerPlugin::forgetTouched()
{
    juce::uint32 node = 0;
    uint32_t param = 0;
    takeTouchedControl (node, param);
}

void ContainerPlugin::rebuildLiveLinks (juce::uint32 leaving)
{
    std::vector<LiveLink> fresh;
    fresh.reserve (macroLinks.size());

    for (const auto& link : macroLinks)
    {
        LiveLink live;
        live.macro = link.macro;
        live.kind = link.kind;
        live.from = link.from;
        live.to = link.to;
        live.inverted = link.inverted;
        live.paramId = link.paramId;

        if (link.kind != MacroLink::Kind::output)
        {
            if (link.nodeUid == leaving)
                continue;

            auto* node = graph->getNodeForId (Graph::NodeID { link.nodeUid });
            auto* pluginNode = node != nullptr ? dynamic_cast<PluginNode*> (node->getProcessor()) : nullptr;

            if (pluginNode == nullptr)
                continue;

            live.node = pluginNode;

            if (link.kind == MacroLink::Kind::parameter)
            {
                auto& plugin = pluginNode->getPlugin();
                const auto index = plugin.findParameter (link.paramId);

                if (index < 0)
                    continue;

                const auto& p = plugin.getParameters()[static_cast<size_t> (index)];
                live.plugin = &plugin;
                live.hint = index;
                live.minValue = p.minValue;
                live.maxValue = p.maxValue;
                live.stepped = p.stepped;
            }
        }

        fresh.push_back (live);
    }

    {
        const juce::SpinLock::ScopedLockType lock (liveLock);
        std::swap (liveLinks, fresh);
    }
}   // the old list goes here, outside the lock
