// D:\workspace\Fanan_web_hub\src\TomahawkSampler.cpp

#include "TomahawkSampler.h"
#include "BuiltinPlugins.h"
#include "tomahawk/TomahawkVoice.h"

#include <cmath>

namespace
{
    Tomahawks::ScreenFactory& screenFactory()
    {
        static Tomahawks::ScreenFactory factory;
        return factory;
    }

    struct Spec
    {
        uint32_t id;
        const char* name;
        double minValue, maxValue, defaultValue;
        bool stepped = false;
    };

    using P = TomahawkSampler;

    const Spec specs[]
    {
        { P::volume,    "Volume",     0.0, 2.0,  1.0 },
        { P::bendRange, "Bend Range", 1.0, 12.0, 2.0, true },
        { P::lowVelocity, "Low Velocity Response", 0.0, 100.0, 0.0, true },
    };

    static_assert (std::size (specs) == static_cast<size_t> (P::numParams), "one spec per parameter");

    const Spec& specOf (uint32_t id) noexcept
    {
        return specs[juce::jlimit<size_t> (0, std::size (specs) - 1, static_cast<size_t> (id) - 1)];
    }
}

//==============================================================================
Plugin::Descriptor Tomahawks::describe()
{
    Plugin::Descriptor descriptor;
    descriptor.id = playerId;
    descriptor.name = "Tomahawk";
    descriptor.vendor = "Sonify";
    descriptor.version = "1.0";
    descriptor.features = { "instrument", "sampler", "stereo" };
    return descriptor;
}

void Tomahawks::setUserFolder (std::function<juce::File()> userData)
{
    Tomahawk::Library::setUserFolder (std::move (userData));
}

void Tomahawks::setScreenFactory (ScreenFactory factory)
{
    screenFactory() = std::move (factory);
}

//==============================================================================
TomahawkSampler::TomahawkSampler()
    : descriptor (Tomahawks::describe()),
      voice (std::make_unique<Tomahawk::Voice>())
{
    inFile.push_back (descriptor);

    for (const auto& spec : specs)
    {
        Parameter parameter;
        parameter.id = spec.id;
        parameter.name = spec.name;
        parameter.minValue = spec.minValue;
        parameter.maxValue = spec.maxValue;
        parameter.defaultValue = spec.defaultValue;
        parameter.stepped = spec.stepped;
        parameters.push_back (parameter);
        values[spec.id].store (spec.defaultValue);
    }

    voice->setBendRange (static_cast<float> (specOf (bendRange).defaultValue));
    voice->setLowVelocityResponse (static_cast<float> (specOf (lowVelocity).defaultValue));
}

TomahawkSampler::~TomahawkSampler()
{
    screen = nullptr;   // its windows first: they read the voice
    voice = nullptr;
}

const juce::File& TomahawkSampler::getSourceFile() const noexcept
{
    return Builtins::file();
}

//==============================================================================
void TomahawkSampler::applyToVoice (uint32_t id, double value) noexcept
{
    if (id == bendRange)
        voice->setBendRange (static_cast<float> (value));
    else if (id == lowVelocity)
        voice->setLowVelocityResponse (static_cast<float> (value));
}

double TomahawkSampler::get (Param id) const noexcept
{
    return id >= 1 && id <= static_cast<uint32_t> (numParams) ? values[id].load (std::memory_order_relaxed) : 0.0;
}

double TomahawkSampler::getDefault (Param id) const noexcept
{
    return specOf (id).defaultValue;
}

void TomahawkSampler::set (Param id, double value)
{
    queueParameter (static_cast<int> (id) - 1, value);
}

int TomahawkSampler::findParameter (uint32_t id) const noexcept
{
    return id >= 1 && id <= static_cast<uint32_t> (numParams) ? static_cast<int> (id) - 1 : -1;
}

double TomahawkSampler::getParameterValue (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, numParams) ? values[static_cast<size_t> (index) + 1].load (std::memory_order_relaxed) : 0.0;
}

double TomahawkSampler::queueParameter (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, numParams))
        return 0.0;

    const auto& spec = specs[static_cast<size_t> (index)];
    value = juce::jlimit (spec.minValue, spec.maxValue, std::isfinite (value) ? value : spec.defaultValue);

    if (spec.stepped)
        value = std::round (value);

    values[spec.id].store (value);

    applyToVoice (spec.id, value);

    stateChanged.store (true);
    return value;
}

void TomahawkSampler::setParameterFromAudio (uint32_t paramId, int& indexHint, double value) noexcept
{
    indexHint = findParameter (paramId);

    if (indexHint < 0)
        return;

    const auto& spec = specs[static_cast<size_t> (indexHint)];
    value = juce::jlimit (spec.minValue, spec.maxValue, std::isfinite (value) ? value : spec.defaultValue);

    if (spec.stepped)
        value = std::round (value);

    values[spec.id].store (value, std::memory_order_relaxed);

    applyToVoice (spec.id, value);
}

juce::String TomahawkSampler::getParameterText (int index, double value)
{
    if (! juce::isPositiveAndBelow (index, numParams))
        return {};

    if (specs[static_cast<size_t> (index)].id == volume)
        return value <= 0.0001 ? juce::String ("-inf dB") : juce::String (juce::Decibels::gainToDecibels (value), 1) + " dB";

    if (specs[static_cast<size_t> (index)].id == lowVelocity)
        return juce::String (juce::roundToInt (value));

    return juce::String (juce::roundToInt (value)) + " st";
}

//==============================================================================
bool TomahawkSampler::saveState (juce::MemoryBlock& destination)
{
    auto* params = new juce::DynamicObject();

    for (const auto& spec : specs)
        params->setProperty (juce::String (spec.id), values[spec.id].load());

    auto* root = new juce::DynamicObject();
    root->setProperty ("plugin", descriptor.id);
    root->setProperty ("version", 1);
    root->setProperty ("params", juce::var (params));

    if (auto xml = voice->saveState().createXml())
        root->setProperty ("voice", xml->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader()));

    const auto text = juce::JSON::toString (juce::var (root), true);
    destination.reset();
    destination.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
    return true;
}

bool TomahawkSampler::loadState (const juce::MemoryBlock& source)
{
    const auto parsed = juce::JSON::parse (source.toString());

    if (! parsed.isObject() || parsed.getProperty ("plugin", {}).toString() != descriptor.id)
        return false;

    if (const auto* params = parsed.getProperty ("params", {}).getDynamicObject())
        for (const auto& [name, value] : params->getProperties())
            queueParameter (findParameter (static_cast<uint32_t> (name.toString().getIntValue())), static_cast<double> (value));

    juce::ValueTree state;

    if (auto xml = juce::parseXML (parsed.getProperty ("voice", {}).toString()))
        state = juce::ValueTree::fromXml (*xml);

    voice->loadState (state);
    stateChanged.store (false);
    return true;
}

//==============================================================================
bool TomahawkSampler::activate (double sampleRate, int maxBlockSize)
{
    voice->prepare (sampleRate > 0.0 ? sampleRate : 48000.0, maxBlockSize > 0 ? maxBlockSize : 512);
    active.store (true);
    return true;
}

bool TomahawkSampler::showNativeScreen (const juce::String& title)
{
    if (screen == nullptr)
        if (auto& factory = screenFactory())
            screen = factory (*this);

    if (screen == nullptr)
        return false;

    screen->show (title);
    return true;
}

void TomahawkSampler::hideNativeScreen()
{
    if (screen != nullptr)
        screen->hide();
}

//==============================================================================
void TomahawkSampler::process (const float*, const float*, float* outLeft, float* outRight,
                               int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept
{
    if (outLeft == nullptr || outRight == nullptr || numSamples <= 0)
        return;

    juce::FloatVectorOperations::clear (outLeft, numSamples);
    juce::FloatVectorOperations::clear (outRight, numSamples);

    // A mixdown waits for a sound being read (the device never does).
    if (Plugin::offlineRendering().load (std::memory_order_relaxed))
        for (int waited = 0; voice->isLoading() && waited < 60000; ++waited)
            juce::Thread::sleep (1);

    const auto bpm = transport != nullptr && (transport->flags & CLAP_TRANSPORT_HAS_TEMPO) != 0 && transport->tempo > 1.0
                         ? transport->tempo : 120.0;

    voice->render (outLeft, outRight, numSamples, midi, bpm);

    const auto gain = static_cast<float> (get (volume));

    if (std::abs (gain - 1.0f) > 1.0e-6f)
    {
        juce::FloatVectorOperations::multiply (outLeft, gain, numSamples);
        juce::FloatVectorOperations::multiply (outRight, gain, numSamples);
    }
}
