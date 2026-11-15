// D:\workspace\Fanan_web_hub\src\kit\kit_PluginProcessor.cpp

#include "PluginProcessor.h"
#include "PluginAssets.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstring>

namespace
{
    constexpr std::int32_t off = -100;   // a scope watching nothing (the runtime's own "off")

    /** A value as text: whole numbers on a wide range, its unit after it (as Sonify shows it). */
    juce::String valueText (const stella::ParamInfo& info, float value)
    {
        const auto range = info.max - info.min;
        const auto decimals = range >= 100.0f ? 0 : (range >= 10.0f ? 1 : 2);
        const auto text = juce::String (value, decimals);
        return info.unit.empty() ? text : text + " " + juce::String::fromUTF8 (info.unit.c_str());
    }
}

//==============================================================================
/** A module parameter as the host sees it: its range, default and skew as the module
    declares them (stella::Param), its id the runtime's ("<module>.<param>"). */
struct SonifyPluginProcessor::Parameter final : juce::AudioParameterFloat
{
    Parameter (const stella::ParamInfo& info)
        : AudioParameterFloat (juce::ParameterID { juce::String::fromUTF8 (info.id.c_str()), 1 },
                               juce::String::fromUTF8 (info.name.c_str()),
                               rangeOf (info),
                               juce::jlimit (info.min, info.max, info.def),
                               juce::AudioParameterFloatAttributes()
                                   .withLabel (juce::String::fromUTF8 (info.unit.c_str()))
                                   .withStringFromValueFunction ([info] (float value, int) { return valueText (info, value); })
                                   .withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue(); }))
    {
    }

    static juce::NormalisableRange<float> rangeOf (const stella::ParamInfo& info)
    {
        const auto max = info.max > info.min ? info.max : info.min + 1.0f;
        return { info.min, max, 0.0f, info.skew > 0.0f ? info.skew : 1.0f };
    }
};

//==============================================================================
SonifyPluginProcessor::SonifyPluginProcessor()
    : AudioProcessor (PluginInfo::kind == 0
                          ? BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          : BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                             .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // What it carries (pictures, looks, factory presets), where the window reads it.
    PluginAssets::unpack();

    built = runtime.build();

    if (built)
    {
        for (int i = 0; i < runtime.getNumParams(); ++i)
        {
            infos.push_back (runtime.getParam (i));
            auto* parameter = new Parameter (infos.back());
            parameters.push_back (parameter);
            addParameter (parameter);
        }
    }

    // As in Sonify: the scopes watch nothing until the window asks.
    for (auto& ring : scopeRing)
        ring.assign ((size_t) scopeSize, 0.0f);

    for (auto& scope : pendingSources.scopes)
        scope = { off, 0 };

    setUpLibrary();
    setUpLicence();

    // Its presets: the factory ones (unpacked), then the user's, shared with Sonify by its name.
    presets.setPlugin (juce::String::fromUTF8 (PluginInfo::name), PluginAssets::presetsFile(), false);

    // A new one starts on its first preset, as in Sonify.
    if (! presets.isEmpty())
        if (const auto& first = presets.getEntries().front(); first.isFactory())
        {
            presets.choose (0);
            applyPreset (first.preset);
        }
}

SonifyPluginProcessor::~SonifyPluginProcessor() = default;

//==============================================================================
void SonifyPluginProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxFrames = juce::jlimit (32, 8192, samplesPerBlock > 0 ? samplesPerBlock : 512);
    played.ensureSize (4096);

    if (built)
    {
        const stella::blob::Library::Scope scope (library);
        prepared = runtime.prepare (sampleRate, maxFrames);
    }

    sourcesChanged.store (true);   // the runtime's taps and scopes, set again
}

void SonifyPluginProcessor::releaseResources() {}

void SonifyPluginProcessor::reset()
{
    if (built)
    {
        const stella::blob::Library::Scope scope (library);
        runtime.reset();
    }
}

bool SonifyPluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return PluginInfo::kind == 0 || layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void SonifyPluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    const auto inputs = getTotalNumInputChannels();

    if (! prepared || numSamples <= 0)
    {
        // Not playing: an instrument is silent, an effect lets the sound through.
        if (PluginInfo::kind == 0)
            buffer.clear();

        return;
    }

    applyWindowSources();

    // The host's notes, and the window's keyboards (which light up with every note played).
    played.clear();
    played.addEvents (midi, 0, numSamples, 0);
    keyboard.processNextMidiBuffer (played, 0, numSamples, true);

    // The host's tempo and position at the start of the block.
    double tempo = 120.0, beat = 0.0, barStart = 0.0;
    bool playing = false;
    int numerator = 4, denominator = 4;

    if (auto* head = getPlayHead())
    {
        if (const auto position = head->getPosition())
        {
            tempo = position->getBpm().orFallback (120.0);
            beat = position->getPpqPosition().orFallback (0.0);
            barStart = position->getPpqPositionOfLastBarStart().orFallback (0.0);
            playing = position->getIsPlaying();

            if (const auto signature = position->getTimeSignature())
            {
                numerator = signature->numerator;
                denominator = signature->denominator;
            }
        }
    }

    if (! (tempo > 1.0 && tempo < 2000.0))
        tempo = 120.0;

    auto& shared = runtime.getShared();
    auto midiIterator = played.cbegin();

    // In stretches of at most maxFrames, each with its own notes.
    for (int offset = 0; offset < numSamples; offset += maxFrames)
    {
        const auto frames = juce::jmin (maxFrames, numSamples - offset);
        std::uint32_t count = 0;

        for (; midiIterator != played.cend(); ++midiIterator)
        {
            const auto metadata = *midiIterator;

            if (metadata.samplePosition >= offset + frames)
                break;

            const auto* raw = metadata.data;

            if (metadata.numBytes < 1 || raw[0] >= 0xf0 || count >= (std::uint32_t) stella::maxEvents)
                continue;

            shared.events[count++] = { (std::uint32_t) juce::jlimit (0, frames - 1, metadata.samplePosition - offset), raw[0],
                                       metadata.numBytes > 1 ? raw[1] : (std::uint8_t) 0,
                                       metadata.numBytes > 2 ? raw[2] : (std::uint8_t) 0, 0 };
        }

        shared.numEvents = count;

        for (size_t i = 0; i < parameters.size() && i < (size_t) stella::maxParams; ++i)
            shared.params[i] = parameters[i]->get();

        for (int c = 0; c < 2; ++c)
        {
            if (inputs > 0)
                std::memcpy (shared.in[c], buffer.getReadPointer (juce::jmin (c, inputs - 1), offset), sizeof (float) * (size_t) frames);
            else
                std::memset (shared.in[c], 0, sizeof (float) * (size_t) frames);
        }

        // Where this stretch starts in the song: the block's start, moved on while it plays.
        const auto at = beat + (playing ? (double) offset * tempo / (60.0 * sampleRate) : 0.0);
        const auto whole = std::floor (at);
        shared.transport.tempo = (float) tempo;
        shared.transport.beatWhole = (std::int32_t) whole;
        shared.transport.beatFraction = (float) (at - whole);
        shared.transport.barStart = (float) barStart;
        shared.transport.playing = playing ? 1 : 0;
        shared.transport.numerator = numerator > 0 ? numerator : 4;
        shared.transport.denominator = denominator > 0 ? denominator : 4;

        {
            const stella::blob::Library::Scope scope (library);   // what the modules' stella_blob_* calls reach
            runtime.process (frames);
        }

        captureLive (frames);

        for (int c = 0; c < buffer.getNumChannels(); ++c)
            std::memcpy (buffer.getWritePointer (c, offset), shared.out[c < 2 ? c : 1], sizeof (float) * (size_t) frames);
    }
}

//==============================================================================
void SonifyPluginProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    juce::DynamicObject::Ptr values (new juce::DynamicObject());

    for (size_t i = 0; i < infos.size(); ++i)
        values->setProperty (juce::String::fromUTF8 (infos[i].id.c_str()), (double) parameters[i]->get());

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    state->setProperty ("version", juce::String::fromUTF8 (PluginInfo::version));
    state->setProperty ("values", juce::var (values.get()));

    // The preset it's on (and whether its values changed since), as a DAW keeps it.
    if (const auto* chosen = presets.getCurrentEntry())
    {
        state->setProperty ("preset", chosen->preset.name);
        state->setProperty ("presetGroup", chosen->group);
        state->setProperty ("presetModified", presets.isModified());
    }

    const auto text = juce::JSON::toString (juce::var (state.get()), true);
    destination.replaceAll (text.toRawUTF8(), text.getNumBytesAsUTF8());
}

void SonifyPluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto state = juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes));
    const auto values = state.getProperty ("values", {});

    if (! values.isObject())
        return;

    // Matched by id: parameters a newer version added keep their defaults.
    for (size_t i = 0; i < infos.size(); ++i)
    {
        const auto id = juce::String::fromUTF8 (infos[i].id.c_str());
        const auto value = values.hasProperty (id) ? (float) (double) values.getProperty (id, infos[i].def) : infos[i].def;
        parameters[i]->setValueNotifyingHost (parameters[i]->convertTo0to1 (juce::jlimit (infos[i].min, infos[i].max, value)));
    }

    // The preset it was on, if it's still there.
    presets.restore (state.getProperty ("preset", {}).toString(), state.getProperty ("presetGroup", {}).toString(),
                     static_cast<bool> (state.getProperty ("presetModified", false)));
}

//==============================================================================
float SonifyPluginProcessor::getValue (int index) const
{
    return juce::isPositiveAndBelow (index, (int) parameters.size()) ? parameters[(size_t) index]->get() : 0.0f;
}

void SonifyPluginProcessor::setValueFromWindow (int index, float value)
{
    if (! juce::isPositiveAndBelow (index, (int) parameters.size()))
        return;

    auto* parameter = parameters[(size_t) index];
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (juce::jlimit (infos[(size_t) index].min, infos[(size_t) index].max, value)));
    parameter->endChangeGesture();
}

std::map<juce::String, float> SonifyPluginProcessor::currentValues() const
{
    std::map<juce::String, float> current;

    for (size_t i = 0; i < infos.size(); ++i)
        current[juce::String::fromUTF8 (infos[i].id.c_str())] = parameters[i]->get();

    return current;
}

void SonifyPluginProcessor::applyPreset (const Preset& preset)
{
    // Its values by id; parameters it doesn't name go back to their defaults.
    for (size_t i = 0; i < infos.size(); ++i)
    {
        const auto found = preset.values.find (juce::String::fromUTF8 (infos[i].id.c_str()));
        const auto value = found != preset.values.end() ? found->second : infos[i].def;
        parameters[i]->setValueNotifyingHost (parameters[i]->convertTo0to1 (juce::jlimit (infos[i].min, infos[i].max, value)));
    }
}

juce::AudioProcessorEditor* SonifyPluginProcessor::createEditor()
{
    return new SonifyPluginEditor (*this);
}

//==============================================================================
// What the window watches.
bool SonifyPluginProcessor::resolve (const juce::String& source, std::int32_t& node, std::int32_t& port) const
{
    return built && runtime.findSignal (source.toStdString(), node, port);
}

void SonifyPluginProcessor::setWindowSources (const juce::StringArray& levels, const juce::StringArray& scopes)
{
    Sources sources;
    levelNames.clear();
    scopeNames.clear();

    for (const auto& source : levels)
    {
        std::int32_t node = 0, port = 0;

        if ((int) sources.taps.size() < stella::maxTaps && resolve (source, node, port))
        {
            sources.taps.push_back ({ node, port });
            levelNames.add (source);
        }
    }

    for (auto& scope : sources.scopes)
        scope = { off, 0 };

    for (const auto& source : scopes)
    {
        std::int32_t node = 0, port = 0;

        if (scopeNames.size() < stella::maxScopes && resolve (source, node, port))
        {
            sources.scopes[(size_t) scopeNames.size()] = { node, port };
            scopeNames.add (source);
        }
    }

    {
        const juce::SpinLock::ScopedLockType lock (sourcesLock);
        pendingSources = std::move (sources);
    }

    sourcesChanged.store (true);
}

void SonifyPluginProcessor::applyWindowSources() noexcept
{
    if (! sourcesChanged.load())
        return;

    const juce::SpinLock::ScopedTryLockType lock (sourcesLock);

    if (! lock.isLocked())
        return;   // the next block

    sourcesChanged.store (false);
    auto& shared = runtime.getShared();
    numTaps = (int) pendingSources.taps.size();

    for (int t = 0; t < numTaps; ++t)
    {
        shared.taps[t] = { pendingSources.taps[(size_t) t].first, pendingSources.taps[(size_t) t].second, 0.0f, 0.0f };
        tapPeak[(size_t) t].store (0.0f);
        tapRms[(size_t) t].store (0.0f);
    }

    shared.numTaps = (std::uint32_t) numTaps;

    for (int s = 0; s < stella::maxScopes; ++s)
    {
        shared.scopeNode[s] = pendingSources.scopes[(size_t) s].first;
        shared.scopePort[s] = pendingSources.scopes[(size_t) s].second;
    }
}

void SonifyPluginProcessor::captureLive (int frames) noexcept
{
    auto& shared = runtime.getShared();

    for (int t = 0; t < numTaps; ++t)
    {
        auto held = tapPeak[(size_t) t].load (std::memory_order_relaxed);
        const auto peak = shared.taps[t].peak;

        while (peak > held && ! tapPeak[(size_t) t].compare_exchange_weak (held, peak, std::memory_order_relaxed)) {}

        tapRms[(size_t) t].store (shared.taps[t].rms, std::memory_order_relaxed);
    }

    for (int s = 0; s < stella::maxScopes; ++s)
    {
        if (shared.scopeNode[s] == off || shared.scope[s] == nullptr)
            continue;

        const auto write = scopeWrite[(size_t) s].load (std::memory_order_relaxed);
        const auto count = juce::jmin ((std::uint32_t) frames, shared.scopeFrames);

        for (std::uint32_t i = 0; i < count; ++i)
            scopeRing[(size_t) s][(write + i) % (std::uint32_t) scopeSize] = shared.scope[s][i];

        scopeWrite[(size_t) s].store (write + count, std::memory_order_release);
    }

    for (int d = 0; d < stella::maxDisplays; ++d)
        displayValues[(size_t) d].store (shared.displays[d], std::memory_order_relaxed);
}

float SonifyPluginProcessor::takeLevel (const juce::String& source, bool rms)
{
    if (! built)
        return 0.0f;

    // A module display ("lfo.position"), else a signal's level.
    if (const auto display = runtime.findDisplay (source.toStdString()); display >= 0 && display < stella::maxDisplays)
        return displayValues[(size_t) display].load (std::memory_order_relaxed);

    const auto tap = levelNames.indexOf (source);

    if (tap < 0)
        return 0.0f;

    return rms ? tapRms[(size_t) tap].load (std::memory_order_relaxed)
               : tapPeak[(size_t) tap].exchange (0.0f, std::memory_order_relaxed);
}

void SonifyPluginProcessor::readScope (const juce::String& source, float* destination, int numSamples)
{
    const auto scope = scopeNames.indexOf (source);

    if (scope < 0)
    {
        juce::FloatVectorOperations::clear (destination, numSamples);
        return;
    }

    const auto write = scopeWrite[(size_t) scope].load (std::memory_order_acquire);
    const auto count = juce::jmin (numSamples, scopeSize);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto age = (std::uint32_t) (count - i);
        destination[i] = i < count && age <= write ? scopeRing[(size_t) scope][(write - age) % (std::uint32_t) scopeSize] : 0.0f;
    }
}

//==============================================================================
// A sampler's library.
void SonifyPluginProcessor::setUpLibrary()
{
    const auto* vendor = PluginInfo::vendor[0] != 0 ? PluginInfo::vendor : "Made in Sonify";

    for (int i = 0; i < PluginInfo::numLibrary; ++i)
    {
        stella::blob::Entry entry;
        entry.name = PluginInfo::library[i].name;
        entry.path = stella::blob::findPluginBlob (PluginInfo::library[i].file, vendor, PluginInfo::name);
        libraryEntries.push_back (entry);
    }

    if (! libraryEntries.empty())
        library.setEntries (libraryEntries);
}

juce::StringArray SonifyPluginProcessor::getMissingInstruments() const
{
    juce::StringArray missing;

    for (const auto& entry : libraryEntries)
        if (entry.path.empty())
            missing.add (juce::String::fromUTF8 (entry.name.c_str()));

    return missing;
}

juce::File SonifyPluginProcessor::libraryFolder() const
{
    const auto vendor = juce::String::fromUTF8 (PluginInfo::vendor);
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile (juce::File::createLegalFileName (vendor.isNotEmpty() ? vendor : juce::String ("Made in Sonify")))
               .getChildFile (juce::File::createLegalFileName (juce::String::fromUTF8 (PluginInfo::name)));
}

//==============================================================================
// Protection.
void SonifyPluginProcessor::setUpLicence()
{
   #if STELLA_PROTECTED
    soniFound = stella::licence::native::findSoni (stella::licence::product(), soni);

    if (soniFound)
        runtime.setDemoTimes (soni.playSeconds, soni.silentSeconds);

    checkRegistration();
   #endif
}

void SonifyPluginProcessor::checkRegistration()
{
   #if STELLA_PROTECTED
    const auto now = soniFound && stella::licence::native::isRegistered (soni);
    registered.store (now);
    runtime.setLicensed (now);
   #endif
}

juce::String SonifyPluginProcessor::getComputerId() const
{
    return juce::String::fromUTF8 (stella::licence::grouped (stella::licence::native::computerId()).c_str());
}

juce::String SonifyPluginProcessor::getRegistrationName() const
{
    return soniFound && ! soni.name.empty() ? juce::String::fromUTF8 (soni.name.c_str()) : juce::String::fromUTF8 (PluginInfo::name);
}

juce::String SonifyPluginProcessor::registerSerial (const juce::String& serial)
{
    std::string why;

   #if STELLA_PROTECTED
    if (! registered.load())
    {
        why = stella::licence::native::registerSerial (soni, serial.toStdString());   // soni not found: says license.soni is missing

        if (why.empty())
        {
            checkRegistration();
            return {};
        }
    }
   #endif

    // Any protected sample library it plays that isn't registered yet.
    bool libraryWaits = false;

    for (const auto& licence : stella::blob::licencesInUse())
        libraryWaits = libraryWaits || ! licence.registered;

    if (libraryWaits)
    {
        const auto libraryWhy = stella::blob::registerSerial (serial.toStdString());

        if (libraryWhy.empty())
            return {};

        if (why.empty())
            why = libraryWhy;
    }

    return why.empty() ? juce::String ("Nothing here needs registering.") : juce::String::fromUTF8 (why.c_str());
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SonifyPluginProcessor();
}
