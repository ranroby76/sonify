// C:\workspace\SONIFY PLUGIN zax\src\PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ZaxProcessor::ZaxProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    zax::files::installFactoryKits();

    // The size the last Zax window was given (80% until one is chosen)
    {
        const auto f = zax::files::zaxFolder().getChildFile ("ui_scale.txt");
        if (f.existsAsFile())
            uiScale = juce::jlimit (0.4f, 1.0f, f.loadFileAsString().trim().getFloatValue());
        if (uiScale < 0.4f) uiScale = 0.8f;
    }

    // Start on the first factory kit (its file, or the built-in kit)
    const auto entries = zax::files::listKits();
    if (! entries.empty() && loadKit (entries.front()).wasOk())
        return;
    *working = zax::factoryKits().front();
    publishKit();
}

//==============================================================================
void ZaxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    heldCount.fill (0);
    for (auto& h : held) h.store (0);
}

bool ZaxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ZaxProcessor::keyDown (int key) noexcept
{
    if (key < 0 || key > 127) return;
    if (heldCount[(size_t) key] < 255) ++heldCount[(size_t) key];
    held[(size_t) (key >> 5)].fetch_or (1u << (key & 31));
    hits[(size_t) (key >> 5)].fetch_or (1u << (key & 31));
}

void ZaxProcessor::keyUp (int key) noexcept
{
    if (key < 0 || key > 127) return;
    if (heldCount[(size_t) key] > 0) --heldCount[(size_t) key];
    if (heldCount[(size_t) key] == 0)
        held[(size_t) (key >> 5)].fetch_and (~(1u << (key & 31)));
}

void ZaxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples <= 0) { buffer.clear(); return; }

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
            if (const auto b = pos->getBpm()) bpm = *b;

    // The editor's auditions play at the top of the block
    for (int r = qRead.load (std::memory_order_relaxed); r != qWrite.load (std::memory_order_acquire); r = (r + 1) % kQueue)
    {
        const int ev  = queue[(size_t) r].load (std::memory_order_relaxed);
        const int key = ev & 0xFF, vel = (ev >> 8) & 0xFF;
        if (vel > 0) { engine.noteOn (key, vel); keyDown (key); }
        else         keyUp (key);
        qRead.store ((r + 1) % kQueue, std::memory_order_release);
    }

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int pos = 0;

    // Sample-accurate: render up to each event, then play it
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        if (at > pos) { engine.render (L + pos, R + pos, at - pos, bpm); pos = at; }

        if (msg.isNoteOn())
        {
            engine.noteOn (msg.getNoteNumber(), msg.getVelocity());
            keyDown (msg.getNoteNumber());
        }
        else if (msg.isNoteOff())
        {
            keyUp (msg.getNoteNumber());
        }
        else if (msg.isAllSoundOff() || msg.isAllNotesOff())
        {
            if (msg.isAllSoundOff()) engine.allSoundOff();
            heldCount.fill (0);
            for (auto& h : held) h.store (0);
        }
    }
    if (pos < numSamples)
        engine.render (L + pos, R + pos, numSamples - pos, bpm);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

//==============================================================================
void ZaxProcessor::auditionNoteOn (int key, int velocity) noexcept
{
    const int w = qWrite.load (std::memory_order_relaxed), next = (w + 1) % kQueue;
    if (next == qRead.load (std::memory_order_acquire)) return;          // full: drop it
    queue[(size_t) w].store ((key & 0xFF) | (juce::jlimit (1, 127, velocity) << 8), std::memory_order_relaxed);
    qWrite.store (next, std::memory_order_release);
}

void ZaxProcessor::auditionNoteOff (int key) noexcept
{
    const int w = qWrite.load (std::memory_order_relaxed), next = (w + 1) % kQueue;
    if (next == qRead.load (std::memory_order_acquire)) return;
    queue[(size_t) w].store (key & 0xFF, std::memory_order_relaxed);
    qWrite.store (next, std::memory_order_release);
}

void ZaxProcessor::readActivity (std::array<uint32_t, 4>& mask) noexcept
{
    for (size_t i = 0; i < 4; ++i)
        mask[i] = held[i].load() | hits[i].exchange (0u);
}

//==============================================================================
void ZaxProcessor::setUiScale (float scale)
{
    uiScale = juce::jlimit (0.4f, 1.0f, std::round (scale * 20.0f) / 20.0f);      // 5% steps
    const auto f = zax::files::zaxFolder().getChildFile ("ui_scale.txt");
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::String (uiScale, 2));
}

//==============================================================================
void ZaxProcessor::publishKit()
{
    engine.setKit (std::make_shared<const zax::Kit> (*working));
}

void ZaxProcessor::kitEdited()
{
    modified = true;
    publishKit();
}

juce::Result ZaxProcessor::loadKit (const zax::files::Entry& entry)
{
    auto k = std::make_unique<zax::Kit>();
    const auto r = zax::files::loadEntry (entry, *k);
    if (r.failed()) return r;
    working     = std::move (k);
    currentFile = entry.file;
    modified    = false;
    ++generation;
    publishKit();
    return r;
}

juce::Result ZaxProcessor::saveKitAs (const juce::File& file, const juce::String& name)
{
    working->name = name.toStdString();
    const auto r = zax::files::writeKit (*working, file);
    if (r.wasOk())
    {
        currentFile = file;
        modified    = false;
        ++generation;
    }
    return r;
}

juce::Result ZaxProcessor::saveKit()
{
    if (currentFile == juce::File())
        return juce::Result::fail ("This kit has no file yet: use Save As.");
    return saveKitAs (currentFile, kitName());
}

//==============================================================================
//  State: the kit's file and the whole kit, so a song sounds as it was saved.
void ZaxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml ("ZAX");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("file", currentFile.getFullPathName());
    xml.setAttribute ("modified", modified);
    xml.setAttribute ("uiScale", (double) uiScale);
    xml.addTextElement (juce::String::fromUTF8 (zax::serializeKit (*working).c_str()));
    copyXmlToBinary (xml, destData);
}

void ZaxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("ZAX")) return;

    auto k = std::make_unique<zax::Kit>();
    if (! zax::deserializeKit (xml->getAllSubText().toStdString(), *k)) return;

    working     = std::move (k);
    currentFile = juce::File::isAbsolutePath (xml->getStringAttribute ("file")) ? juce::File (xml->getStringAttribute ("file"))
                                                                                : juce::File();
    modified    = xml->getBoolAttribute ("modified", false);
    if (xml->hasAttribute ("uiScale"))
        uiScale = juce::jlimit (0.4f, 1.0f, (float) xml->getDoubleAttribute ("uiScale", 0.8));
    ++generation;
    publishKit();
}

//==============================================================================
juce::AudioProcessorEditor* ZaxProcessor::createEditor()
{
    return new ZaxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZaxProcessor();
}
