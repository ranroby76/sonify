// D:\workspace\Fanan_web_hub\src\Vst3Plugin.cpp

#include "Vst3Plugin.h"

#if FANAN_PLUGIN_WINDOWS
 #include "PluginHeader.h"
#endif

#include <map>

namespace
{
    /** JUCE's plugin formats: made when first needed, on the message thread, and let go
        by shutdownFormats() (not left to static destruction, which comes too late for
        JUCE's leak checks). */
    std::unique_ptr<juce::AudioPluginFormatManager>& formatHolder()
    {
        static std::unique_ptr<juce::AudioPluginFormatManager> manager;
        return manager;
    }

    juce::AudioPluginFormatManager& formats()
    {
        auto& manager = formatHolder();

        if (manager == nullptr)
        {
            manager = std::make_unique<juce::AudioPluginFormatManager>();
           #if FANAN_PLUGIN_WINDOWS
            manager->addFormat (std::make_unique<juce::VST3PluginFormat>());   // with plugin windows (first: vst3())

            // The Sampling Studio's nodes load these straight from their files.
            #if JUCE_PLUGINHOST_VST
             manager->addFormat (std::make_unique<juce::VSTPluginFormat>());
            #endif
            #if JUCE_PLUGINHOST_AU && JUCE_MAC
             manager->addFormat (std::make_unique<juce::AudioUnitPluginFormat>());
            #endif
            #if JUCE_PLUGINHOST_LV2 && (JUCE_LINUX || JUCE_BSD)
             manager->addFormat (std::make_unique<juce::LV2PluginFormat>());
            #endif
           #else
            manager->addFormat (std::make_unique<juce::VST3PluginFormatHeadless>());
           #endif
        }

        return *manager;
    }

    juce::AudioPluginFormat& vst3()
    {
        return *formats().getFormat (0);
    }

    /** One of the formats by JUCE's name for it, or none. */
    juce::AudioPluginFormat* formatNamed (const juce::String& name)
    {
        auto& manager = formats();

        for (int i = 0; i < manager.getNumFormats(); ++i)
            if (auto* format = manager.getFormat (i); format != nullptr && format->getName() == name)
                return format;

        return nullptr;
    }

    /** VST3 categories as CLAP feature words, so both kinds of plugin sort alike. */
    juce::StringArray featuresFor (const juce::PluginDescription& description)
    {
        static const std::map<juce::String, const char*> words {
            { "synth", "synthesizer" }, { "sampler", "sampler" }, { "drum", "drum-machine" }, { "piano", "synthesizer" },
            { "generator", "synthesizer" }, { "external", "synthesizer" }, { "reverb", "reverb" }, { "delay", "delay" },
            { "eq", "equalizer" }, { "dynamics", "compressor" }, { "distortion", "distortion" }, { "filter", "filter" },
            { "modulation", "chorus" }, { "analyzer", "analyzer" }, { "tools", "utility" }, { "mastering", "mastering" },
            { "pitch shift", "pitch-shifter" }, { "restoration", "restoration" }, { "spatial", "utility" }, { "surround", "surround" }
        };

        juce::StringArray features;
        features.add (description.isInstrument ? "instrument" : "audio-effect");

        for (const auto& part : juce::StringArray::fromTokens (description.category, "|", ""))
            if (const auto found = words.find (part.trim().toLowerCase()); found != words.end())
                features.addIfNotAlreadyThere (found->second);

        return features;
    }

    Plugin::Descriptor descriptorFor (const juce::PluginDescription& description)
    {
        return { description.createIdentifierString(), description.name,
                 description.manufacturerName.isNotEmpty() ? description.manufacturerName : juce::String ("Unknown vendor"),
                 description.version, featuresFor (description) };
    }
}

//==============================================================================
juce::FileSearchPath Vst3Plugin::getDefaultFolders()
{
    // A format of its own for the question, so asking doesn't keep JUCE's VST3 host alive.
    return juce::VST3PluginFormatHeadless().getDefaultLocationsToSearch();
}

void Vst3Plugin::shutdownFormats()
{
    formatHolder().reset();
}

std::vector<Plugin::Descriptor> Vst3Plugin::describe (const juce::File& file, juce::String& error)
{
    juce::OwnedArray<juce::PluginDescription> found;
    vst3().findAllTypesForFile (found, file.getFullPathName());

    std::vector<Descriptor> list;

    for (const auto* description : found)
        list.push_back (descriptorFor (*description));

    if (list.empty())
        error = file.getFileName() + " has no VST3 plugin Sonify can load.";

    return list;
}

std::unique_ptr<Vst3Plugin> Vst3Plugin::load (const juce::File& file, const juce::String& pluginId, juce::String& error)
{
    juce::OwnedArray<juce::PluginDescription> found;
    vst3().findAllTypesForFile (found, file.getFullPathName());

    if (found.isEmpty())
    {
        error = file.getFileName() + " has no VST3 plugin Sonify can load.";
        return nullptr;
    }

    const juce::PluginDescription* chosen = found.getFirst();

    if (pluginId.isNotEmpty())
    {
        chosen = nullptr;

        for (const auto* description : found)
            if (description->createIdentifierString() == pluginId)
                chosen = description;

        if (chosen == nullptr)
        {
            error = file.getFileName() + " no longer has that plugin.";
            return nullptr;
        }
    }

    std::vector<Descriptor> inFile;

    for (const auto* description : found)
        inFile.push_back (descriptorFor (*description));

    juce::String message;
    auto instance = formats().createPluginInstance (*chosen, 48000.0, 512, message);

    if (instance == nullptr)
    {
        error = message.isNotEmpty() ? message : "Couldn't load " + chosen->name + ".";
        return nullptr;
    }

    return std::unique_ptr<Vst3Plugin> (new Vst3Plugin (std::move (instance), *chosen, std::move (inFile), file));
}

std::vector<Plugin::Descriptor> Vst3Plugin::describeAs (const juce::File& file, const juce::String& juceFormat, const juce::String& shownFormat,
                                                        juce::String& error)
{
    auto* format = formatNamed (juceFormat);

    if (format == nullptr)
    {
        error = shownFormat + " plugins can't be loaded in this Sonify.";
        return {};
    }

    juce::OwnedArray<juce::PluginDescription> found;
    format->findAllTypesForFile (found, file.getFullPathName());

    std::vector<Descriptor> list;

    for (const auto* description : found)
        list.push_back (descriptorFor (*description));

    if (list.empty())
        error = file.getFileName() + " has no " + shownFormat + " plugin Sonify can load.";

    return list;
}

std::unique_ptr<Vst3Plugin> Vst3Plugin::loadAs (const juce::File& file, const juce::String& juceFormat, const juce::String& shownFormat,
                                                const juce::String& pluginId, juce::String& error)
{
    auto* format = formatNamed (juceFormat);

    if (format == nullptr)
    {
        error = shownFormat + " plugins can't be loaded in this Sonify.";
        return nullptr;
    }

    juce::OwnedArray<juce::PluginDescription> found;
    format->findAllTypesForFile (found, file.getFullPathName());

    if (found.isEmpty())
    {
        error = file.getFileName() + " has no " + shownFormat + " plugin Sonify can load.";
        return nullptr;
    }

    const juce::PluginDescription* chosen = found.getFirst();

    if (pluginId.isNotEmpty())
    {
        chosen = nullptr;

        for (const auto* description : found)
            if (description->createIdentifierString() == pluginId)
                chosen = description;

        if (chosen == nullptr)
        {
            error = file.getFileName() + " no longer has that plugin.";
            return nullptr;
        }
    }

    std::vector<Descriptor> inFile;

    for (const auto* description : found)
        inFile.push_back (descriptorFor (*description));

    juce::String message;
    auto instance = formats().createPluginInstance (*chosen, 48000.0, 512, message);

    if (instance == nullptr)
    {
        error = message.isNotEmpty() ? message : "Couldn't load " + chosen->name + ".";
        return nullptr;
    }

    std::unique_ptr<Vst3Plugin> plugin (new Vst3Plugin (std::move (instance), *chosen, std::move (inFile), file));
    plugin->formatName = shownFormat;
    return plugin;
}

Vst3Plugin::Vst3Plugin (std::unique_ptr<juce::AudioPluginInstance> pluginInstance, const juce::PluginDescription& description,
                        std::vector<Descriptor> inFile, const juce::File& file)
    : instance (std::move (pluginInstance)),
      descriptor (descriptorFor (description)),
      pluginsInFile (std::move (inFile)),
      sourceFile (file),
      role (description.isInstrument ? Role::instrument : Role::effect)
{
    // A sidechain input (compressors, duckers) is switched on, so tracks can send to it.
    if (instance->getBusCount (true) > 1)
        if (auto* bus = instance->getBus (true, 1); bus != nullptr && ! bus->isEnabled())
            bus->enable (true);

    inputChannels = instance->getMainBusNumInputChannels();
    outputChannels = instance->getTotalNumOutputChannels();

    const auto& all = instance->getParameters();

    for (int i = 0; i < all.size(); ++i)
    {
        auto* parameter = all[i];
        Parameter info;
        info.id = static_cast<uint32_t> (i);

        // The VST3 id, not the list position: it stays the same when a plugin is updated.
        if (auto* withId = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
            info.id = static_cast<uint32_t> (withId->getParameterID().getLargeIntValue());

        info.name = parameter->getName (64);
        info.minValue = 0.0;
        info.maxValue = 1.0;
        info.defaultValue = parameter->getDefaultValue();
        info.stepped = parameter->isDiscrete() || parameter->isBoolean();

        // Out of sight: what a plugin keeps from automation (JUCE-built instruments carry
        // 2,080 hidden MIDI controller parameters), and Bypass, since inserts have their own.
        info.hidden = ! parameter->isAutomatable() || parameter == instance->getBypassParameter();
        parameters.push_back (info);
        hosted.push_back (parameter);
    }

    instance->addListener (this);
    instance->setPlayHead (&playHead);
}

//==============================================================================
#if FANAN_PLUGIN_WINDOWS
/** The plugin's own window: floats over the Hub, hides when closed, and goes with the plugin.
    Its header (PluginHeader.h) is over the editor: ON/OFF, the name, the format. */
class Vst3Plugin::ScreenWindow final : public juce::DocumentWindow
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

Plugin::ScreenKind Vst3Plugin::getScreenKind() const noexcept
{
   #if FANAN_PLUGIN_WINDOWS
    return instance->hasEditor() ? ScreenKind::native : ScreenKind::none;
   #else
    return ScreenKind::none;
   #endif
}

bool Vst3Plugin::showNativeScreen (const juce::String& title)
{
   #if FANAN_PLUGIN_WINDOWS
    if (screenWindow == nullptr)
    {
        auto* editor = instance->hasEditor() ? instance->createEditorAndMakeActive() : nullptr;

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

void Vst3Plugin::hideNativeScreen()
{
   #if FANAN_PLUGIN_WINDOWS
    if (screenWindow != nullptr)
        screenWindow->setVisible (false);
   #endif
}

Vst3Plugin::~Vst3Plugin()
{
   #if FANAN_PLUGIN_WINDOWS
    screenWindow.reset();   // the editor goes before the plugin it shows
   #endif

    instance->removeListener (this);
    instance->setPlayHead (nullptr);
    deactivate();
    instance.reset();
}

//==============================================================================
bool Vst3Plugin::saveState (juce::MemoryBlock& destination)
{
    destination.reset();
    instance->getStateInformation (destination);
    return true;
}

bool Vst3Plugin::loadState (const juce::MemoryBlock& source)
{
    instance->setStateInformation (source.getData(), static_cast<int> (source.getSize()));
    return true;
}

juce::MemoryBlock Vst3Plugin::getVst3Preset()
{
    if (formatName != "VST3" || instance == nullptr)
        return {};

    if (auto* client = instance->getVST3Client())
        return client->getPreset();

    return {};
}

bool Vst3Plugin::setVst3Preset (const juce::MemoryBlock& preset)
{
    if (formatName != "VST3" || instance == nullptr || preset.isEmpty())
        return false;

    if (auto* client = instance->getVST3Client(); client != nullptr && client->setPreset (preset))
    {
        stateChanged.store (true);
        return true;
    }

    return false;
}

int Vst3Plugin::findParameter (uint32_t id) const noexcept
{
    for (size_t i = 0; i < parameters.size(); ++i)
        if (parameters[i].id == id)
            return static_cast<int> (i);

    return -1;
}

double Vst3Plugin::getParameterValue (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, static_cast<int> (hosted.size())) ? hosted[static_cast<size_t> (index)]->getValue() : 0.0;
}

juce::String Vst3Plugin::getParameterText (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (hosted.size())))
        return {};

    auto* parameter = hosted[static_cast<size_t> (index)];
    const auto text = parameter->getText (static_cast<float> (value), 64);
    const auto label = parameter->getLabel();
    return label.isEmpty() || text.endsWith (label) ? text : text + " " + label;
}

double Vst3Plugin::queueParameter (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (hosted.size())))
        return 0.0;

    value = juce::jlimit (0.0, 1.0, value);
    hosted[static_cast<size_t> (index)]->setValueNotifyingHost (static_cast<float> (value));   // JUCE carries it to the audio thread
    return value;
}

void Vst3Plugin::setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept
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

    // JUCE's VST3 host takes it from the audio thread into the next processBlock.
    hosted[static_cast<size_t> (indexHint)]->setValue (static_cast<float> (juce::jlimit (0.0, 1.0, value)));
}

//==============================================================================
int Vst3Plugin::getLatencySamples() const noexcept
{
    // What the plugin reports (JUCE keeps it as the plugin tells it, after it's prepared).
    return instance != nullptr ? juce::jmax (0, instance->getLatencySamples()) : 0;
}

bool Vst3Plugin::activate (double sampleRate, int maxBlockSize)
{
    if (active.load() && juce::exactlyEqual (activeRate, sampleRate) && activeBlockSize >= maxBlockSize)
        return true;

    deactivate();
    instance->setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
    instance->prepareToPlay (sampleRate, maxBlockSize);

    inputChannels = instance->getMainBusNumInputChannels();
    totalInputChannels = instance->getTotalNumInputChannels();
    outputChannels = instance->getTotalNumOutputChannels();

    // Where the sidechain's channels sit in the block the plugin is given.
    sidechainOffset = -1;
    sidechainChannels = 0;

    if (instance->getBusCount (true) > 1)
    {
        if (auto* bus = instance->getBus (true, 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0)
        {
            sidechainOffset = instance->getChannelIndexInProcessBlockBuffer (true, 1, 0);
            sidechainChannels = juce::jmin (2, bus->getNumberOfChannels());
            prepareSidechain (maxBlockSize);
        }
    }

    buffer.setSize (juce::jmax (2, totalInputChannels, outputChannels), juce::jmax (1, maxBlockSize));
    makesMidi = instance->producesMidi();
    midiOutput.ensureSize (8192);
    midiBuffer.ensureSize (8192);

    activeRate = sampleRate;
    activeBlockSize = maxBlockSize;
    playHead.sampleRate = sampleRate;
    active.store (true);
    return true;
}

void Vst3Plugin::deactivate()
{
    if (active.exchange (false))
        instance->releaseResources();
}

void Vst3Plugin::process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
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
    instance->processBlock (block, midiBuffer);

    // A plugin that plays notes itself hands them back in the same buffer.
    midiOutput.clear();

    if (makesMidi)
        midiOutput.addEvents (midiBuffer, 0, numSamples, 0);

    if (outLeft != nullptr && outputChannels > 0)
        juce::FloatVectorOperations::copy (outLeft, buffer.getReadPointer (0), numSamples);

    if (outRight != nullptr && outputChannels > 0)
        juce::FloatVectorOperations::copy (outRight, buffer.getReadPointer (outputChannels > 1 ? 1 : 0), numSamples);
}

juce::Optional<juce::AudioPlayHead::PositionInfo> Vst3Plugin::PlayHead::getPosition() const
{
    PositionInfo info;
    info.setTimeInSamples (0);   // VST3 requires it, so it's always given

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
