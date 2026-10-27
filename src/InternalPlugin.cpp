// D:\workspace\Fanan_web_hub\src\InternalPlugin.cpp

#include "InternalPlugin.h"

#if FANAN_PLUGIN_WINDOWS
 #include "PluginHeader.h"
#endif

#include "BuiltinPlugins.h"

// Sonify's own plugins, each written as a plugin is (a juce::AudioProcessor with a window of its
// own) and compiled into Sonify. Their folders under src are theirs alone: Sonify reaches them
// only through the entries at the bottom of this file.
#include "axis/PluginProcessor.h"
#include "natura/PluginProcessor.h"
#include "zax/PluginProcessor.h"

namespace
{
    /** A parameter's stable id, from the name the plugin gave it ("DRV_GAIN"): songs keep values
        by it, so it must stay the same when the plugin gains or loses parameters, or they move
        about. A hash of the name does that, where the list position wouldn't. (FNV-1a.) */
    uint32_t idForName (const juce::String& name)
    {
        uint32_t hash = 2166136261u;

        for (const auto character : name)
        {
            hash ^= static_cast<uint32_t> (character) & 0xffu;
            hash *= 16777619u;
        }

        return hash;
    }
}

//==============================================================================
std::unique_ptr<InternalPlugin> InternalPlugin::make (std::unique_ptr<juce::AudioProcessor> processor,
                                                      Descriptor descriptor, Role role, const juce::File& file)
{
    if (processor == nullptr)
        return nullptr;

    return std::unique_ptr<InternalPlugin> (new InternalPlugin (std::move (processor), std::move (descriptor), role, file));
}

InternalPlugin::InternalPlugin (std::unique_ptr<juce::AudioProcessor> ownProcessor, Descriptor ownDescriptor,
                                Role ownRole, const juce::File& file)
    : processor (std::move (ownProcessor)),
      descriptor (std::move (ownDescriptor)),
      sourceFile (file),
      role (ownRole)
{
    pluginsInFile.push_back (descriptor);

    // A sidechain input (a ducker, a compressor) is switched on, so tracks can send to it.
    if (processor->getBusCount (true) > 1)
        if (auto* bus = processor->getBus (true, 1); bus != nullptr && ! bus->isEnabled())
            bus->enable (true);

    inputChannels = processor->getMainBusNumInputChannels();
    outputChannels = processor->getTotalNumOutputChannels();

    const auto& all = processor->getParameters();

    for (int i = 0; i < all.size(); ++i)
    {
        auto* parameter = all[i];
        Parameter info;
        info.id = static_cast<uint32_t> (i);

        // Its own name for it, not the list position.
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            info.id = idForName (withId->paramID);

        // Two names that hash alike (all but unheard of): the later one keeps its position, so
        // every parameter still has an id of its own.
        for (const auto& already : parameters)
            if (already.id == info.id)
                info.id = static_cast<uint32_t> (i);

        info.name = parameter->getName (64);
        info.minValue = 0.0;
        info.maxValue = 1.0;
        info.defaultValue = parameter->getDefaultValue();
        info.stepped = parameter->isDiscrete() || parameter->isBoolean();

        // Out of sight: what the plugin keeps from automation, and Bypass, since inserts have
        // their own.
        info.hidden = ! parameter->isAutomatable() || parameter == processor->getBypassParameter();
        parameters.push_back (info);
        hosted.push_back (parameter);
    }

    processor->addListener (this);
    processor->setPlayHead (&playHead);
}

InternalPlugin::~InternalPlugin()
{
   #if FANAN_PLUGIN_WINDOWS
    screenWindow.reset();   // the editor goes before the plugin it shows
   #endif

    processor->removeListener (this);
    processor->setPlayHead (nullptr);
    deactivate();
    processor.reset();
}

//==============================================================================
#if FANAN_PLUGIN_WINDOWS
/** The plugin's own window: floats over the Hub, hides when closed, and goes with the plugin.
    Its header (PluginHeader.h) is over the editor: ON/OFF, the name, the format. */
class InternalPlugin::ScreenWindow final : public juce::DocumentWindow
{
public:
    ScreenWindow (Plugin& plugin, const juce::String& title, juce::AudioProcessorEditor* editor)
        : DocumentWindow (title, juce::Colour (0xff2b2d31), DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        PluginHeader::attach (*this, plugin);
        setContentOwned (editor, true);
        setResizable (editor->isResizable(), false);
        centreWithSize (getWidth(), getHeight());
    }

    void closeButtonPressed() override   { setVisible (false); }
};
#endif

Plugin::ScreenKind InternalPlugin::getScreenKind() const noexcept
{
   #if FANAN_PLUGIN_WINDOWS
    return processor->hasEditor() ? ScreenKind::native : ScreenKind::none;
   #else
    return ScreenKind::none;
   #endif
}

bool InternalPlugin::showNativeScreen (const juce::String& title)
{
   #if FANAN_PLUGIN_WINDOWS
    if (screenWindow == nullptr)
    {
        auto* editor = processor->hasEditor() ? processor->createEditorIfNeeded() : nullptr;

        if (editor == nullptr)
            return false;

        screenWindow = std::make_unique<ScreenWindow> (*this, title, editor);
    }

    screenWindow->setName (title);
    screenWindow->setVisible (true);
    screenWindow->toFront (true);
    return true;
   #else
    juce::ignoreUnused (title);
    return false;
   #endif
}

void InternalPlugin::hideNativeScreen()
{
   #if FANAN_PLUGIN_WINDOWS
    if (screenWindow != nullptr)
        screenWindow->setVisible (false);
   #endif
}

//==============================================================================
bool InternalPlugin::saveState (juce::MemoryBlock& destination)
{
    destination.reset();
    processor->getStateInformation (destination);
    return true;
}

bool InternalPlugin::loadState (const juce::MemoryBlock& source)
{
    processor->setStateInformation (source.getData(), static_cast<int> (source.getSize()));
    return true;
}

int InternalPlugin::findParameter (uint32_t id) const noexcept
{
    for (size_t i = 0; i < parameters.size(); ++i)
        if (parameters[i].id == id)
            return static_cast<int> (i);

    return -1;
}

double InternalPlugin::getParameterValue (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, static_cast<int> (hosted.size())) ? hosted[static_cast<size_t> (index)]->getValue() : 0.0;
}

juce::String InternalPlugin::getParameterText (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (hosted.size())))
        return {};

    auto* parameter = hosted[static_cast<size_t> (index)];
    const auto text = parameter->getText (static_cast<float> (value), 64);
    const auto label = parameter->getLabel();
    return label.isEmpty() || text.endsWith (label) ? text : text + " " + label;
}

double InternalPlugin::queueParameter (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (hosted.size())))
        return 0.0;

    value = juce::jlimit (0.0, 1.0, value);
    hosted[static_cast<size_t> (index)]->setValueNotifyingHost (static_cast<float> (value));
    return value;
}

void InternalPlugin::setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept
{
    const auto count = static_cast<int> (parameters.size());

    if (! juce::isPositiveAndBelow (indexHint, count) || parameters[static_cast<size_t> (indexHint)].id != paramId)
    {
        indexHint = -1;

        for (int i = 0; i < count; ++i)
            if (parameters[static_cast<size_t> (i)].id == paramId)
                indexHint = i;

        if (indexHint < 0)
            return;
    }

    hosted[static_cast<size_t> (indexHint)]->setValue (static_cast<float> (juce::jlimit (0.0, 1.0, value)));
}

//==============================================================================
int InternalPlugin::getLatencySamples() const noexcept
{
    return processor != nullptr ? juce::jmax (0, processor->getLatencySamples()) : 0;
}

bool InternalPlugin::activate (double sampleRate, int maxBlockSize)
{
    if (active.load() && juce::exactlyEqual (activeRate, sampleRate) && activeBlockSize >= maxBlockSize)
        return true;

    deactivate();
    processor->setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
    processor->prepareToPlay (sampleRate, maxBlockSize);

    inputChannels = processor->getMainBusNumInputChannels();
    totalInputChannels = processor->getTotalNumInputChannels();
    outputChannels = processor->getTotalNumOutputChannels();

    // Where the sidechain's channels sit in the block the plugin is given.
    sidechainOffset = -1;
    sidechainChannels = 0;

    if (processor->getBusCount (true) > 1)
    {
        if (auto* bus = processor->getBus (true, 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0)
        {
            sidechainOffset = processor->getChannelIndexInProcessBlockBuffer (true, 1, 0);
            sidechainChannels = juce::jmin (2, bus->getNumberOfChannels());
            prepareSidechain (maxBlockSize);
        }
    }

    buffer.setSize (juce::jmax (2, totalInputChannels, outputChannels), juce::jmax (1, maxBlockSize));
    makesMidi = processor->producesMidi();
    midiOutput.ensureSize (8192);
    midiBuffer.ensureSize (8192);

    activeRate = sampleRate;
    activeBlockSize = maxBlockSize;
    playHead.sampleRate = sampleRate;
    active.store (true);
    return true;
}

void InternalPlugin::deactivate()
{
    if (active.exchange (false))
        processor->releaseResources();
}

void InternalPlugin::process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                              int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept
{
    // Not ready for this block: effects leave the sound as it is, instruments stay silent.
    if (! active.load (std::memory_order_relaxed) || numSamples <= 0 || numSamples > buffer.getNumSamples())
        return;

    const auto channels = buffer.getNumChannels();

    for (int channel = 0; channel < channels; ++channel)
        buffer.clear (channel, 0, numSamples);

    if (inLeft != nullptr && inputChannels > 0)
        buffer.copyFrom (0, 0, inLeft, numSamples);

    if (inputChannels > 1 && (inRight != nullptr || inLeft != nullptr))
        buffer.copyFrom (1, 0, inRight != nullptr ? inRight : inLeft, numSamples);

    // What tracks send to its sidechain (silence when nothing does).
    if (sidechainChannels > 0 && numSamples <= sidechain.getNumSamples()
        && sidechainOffset + sidechainChannels <= buffer.getNumChannels())
    {
        for (int channel = 0; channel < sidechainChannels; ++channel)
            buffer.copyFrom (sidechainOffset + channel, 0, sidechain, channel, 0, numSamples);
    }

    // A copy: a plugin may change the MIDI it's given.
    midiBuffer.clear();

    if (! midi.isEmpty())
        midiBuffer.addEvents (midi, 0, numSamples, 0);

    // Copied: the transport lives only for this call, and the plugin may ask for it later.
    if (transport != nullptr)
        playHead.transport = *transport;

    playHead.hasTransport = transport != nullptr;
    juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), channels, numSamples);
    processor->processBlock (block, midiBuffer);

    // A plugin that plays notes itself hands them back in the same buffer.
    midiOutput.clear();

    if (makesMidi)
        midiOutput.addEvents (midiBuffer, 0, numSamples, 0);

    if (outLeft != nullptr && outputChannels > 0)
        juce::FloatVectorOperations::copy (outLeft, buffer.getReadPointer (0), numSamples);

    if (outRight != nullptr && outputChannels > 0)
        juce::FloatVectorOperations::copy (outRight, buffer.getReadPointer (outputChannels > 1 ? 1 : 0), numSamples);
}

juce::Optional<juce::AudioPlayHead::PositionInfo> InternalPlugin::PlayHead::getPosition() const
{
    PositionInfo info;
    info.setTimeInSamples (0);

    if (! hasTransport)
        return info;

    const auto& t = transport;
    info.setIsPlaying ((t.flags & CLAP_TRANSPORT_IS_PLAYING) != 0);
    info.setIsRecording ((t.flags & CLAP_TRANSPORT_IS_RECORDING) != 0);
    info.setIsLooping ((t.flags & CLAP_TRANSPORT_IS_LOOP_ACTIVE) != 0);

    if ((t.flags & CLAP_TRANSPORT_HAS_TEMPO) != 0)
        info.setBpm (t.tempo);

    if ((t.flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE) != 0)
        info.setTimeSignature (TimeSignature { t.tsig_num, t.tsig_denom });

    if ((t.flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) != 0)
    {
        info.setPpqPosition (static_cast<double> (t.song_pos_beats) / CLAP_BEATTIME_FACTOR);
        info.setPpqPositionOfLastBarStart (static_cast<double> (t.bar_start) / CLAP_BEATTIME_FACTOR);
        info.setBarCount (t.bar_number);
        info.setLoopPoints (LoopPoints { static_cast<double> (t.loop_start_beats) / CLAP_BEATTIME_FACTOR,
                                         static_cast<double> (t.loop_end_beats) / CLAP_BEATTIME_FACTOR });
    }

    if ((t.flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE) != 0)
    {
        const auto seconds = static_cast<double> (t.song_pos_seconds) / CLAP_SECTIME_FACTOR;
        info.setTimeInSeconds (seconds);
        info.setTimeInSamples (static_cast<juce::int64> (std::llround (seconds * sampleRate)));
    }

    return info;
}

//==============================================================================
//  Sonify's own plugins
//==============================================================================
const std::vector<Internals::Entry>& Internals::entries()
{
    static const std::vector<Entry> list
    {
        { juce::String (Builtins::idPrefix) + "natura", "Natura", "Sonify", "1.0.0", "Synth",
          { "instrument", "synthesizer" }, Plugin::Role::instrument,
          [] { return std::unique_ptr<juce::AudioProcessor> (new NaturaProcessor()); } },

        { juce::String (Builtins::idPrefix) + "zax", "Zax", "Sonify", "1.0.0", "Synth",
          { "instrument", "drum-machine" }, Plugin::Role::instrument,
          [] { return std::unique_ptr<juce::AudioProcessor> (new ZaxProcessor()); } },

        { juce::String (Builtins::idPrefix) + "axis", "AXIS", "Sonify", "1.0.0", "Guitar",
          { "audio-effect", "distortion", "stereo" }, Plugin::Role::effect,
          [] { return std::unique_ptr<juce::AudioProcessor> (new AxisAudioProcessor()); } },
    };

    return list;
}

const Internals::Entry* Internals::find (const juce::String& pluginId)
{
    for (const auto& entry : entries())
        if (entry.id == pluginId)
            return &entry;

    return nullptr;
}

std::vector<Plugin::Descriptor> Internals::describe()
{
    std::vector<Plugin::Descriptor> list;

    for (const auto& entry : entries())
        list.push_back ({ entry.id, entry.name, entry.vendor, entry.version, entry.features });

    return list;
}

std::unique_ptr<Plugin> Internals::create (const juce::String& pluginId, const juce::File& file, juce::String& error)
{
    const auto* entry = find (pluginId);

    if (entry == nullptr || entry->make == nullptr)
    {
        error = "Sonify has no built-in plugin with that id.";
        return nullptr;
    }

    auto processor = entry->make();

    if (processor == nullptr)
    {
        error = "Couldn't start " + entry->name + ".";
        return nullptr;
    }

    auto plugin = InternalPlugin::make (std::move (processor),
                                        { entry->id, entry->name, entry->vendor, entry->version, entry->features },
                                        entry->role, file);

    if (plugin == nullptr)
        error = "Couldn't start " + entry->name + ".";

    return plugin;
}
