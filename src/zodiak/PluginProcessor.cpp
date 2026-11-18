// D:\workspace\Fanan_web_hub\src\zodiak\PluginProcessor.cpp
#include "PluginProcessor.h"
#include "ZodiakLinkApply.h"
#include "ZodiakEditor.h"

//==============================================================================
ZodiakProcessor::ZodiakProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // The size the last window was given (55% until one is chosen)
    {
        const auto f = zodiak::sets::zodiakFolder().getChildFile ("ui_scale.txt");
        if (f.existsAsFile())
            uiScale = juce::jlimit (0.3f, 1.0f, f.loadFileAsString().trim().getFloatValue());
        if (uiScale < 0.3f) uiScale = 0.55f;

        const auto t = zodiak::sets::zodiakFolder().getChildFile ("roll_theme.txt");
        rollDark = t.existsAsFile() && t.loadFileAsString().trim() == "dark";
    }

    // The MIDI LINK map in force (before the set: an older set's MIDI learn joins it)
    eng->links().load (zodiak::link::mapFile());
    savedLinkGen = eng->links().generation();

    startUp();
    linkPump.startTimerHz (60);
}

ZodiakProcessor::~ZodiakProcessor()
{
    linkPump.stopTimer();
    *alive = false;
}

//==============================================================================
//  MIDI LINK
//==============================================================================
void ZodiakProcessor::pumpLinks()
{
    auto& map = eng->links();
    zodiak::link::applyDeferred (map, *eng);
    watchGraphs();

    // MIDI learn: the source moved, onto the target waiting
    if (learnTarget.valid())
    {
        const int hit = eng->takeLearnHit();
        if (hit >= 0)
        {
            const auto t = learnTarget;
            learnTarget = {};
            linkSource (zodiak::link::Source::fromKey (hit), t);
        }
        else if (! eng->isLearnArmed())
            learnTarget = {};
    }

    // The map changed: its file, at once
    if (map.generation() != savedLinkGen)
    {
        savedLinkGen = map.generation();
        map.save (zodiak::link::mapFile());
    }
}

void ZodiakProcessor::watchGraphs()
{
    bool changed = false;
    for (int ch = 0; ch < zodiak::kNumChannels; ++ch)
    {
        auto& g = eng->graph (ch);
        if (g.latencyMoved()) g.rebuild();          // a plugin reports another latency
        if (g.generation() != seenGraphGen[(size_t) ch]) { seenGraphGen[(size_t) ch] = g.generation(); changed = true; }
    }
    if (! changed) return;

    // A link onto a node's setting was read by its id: its index, on the graph as it is
    auto& map = eng->links();
    for (int i = 0; i < zodiak::link::Map::kMaxLinks; ++i)
    {
        auto& sl = map.slot (i);
        if (sl.source.load() < 0 || sl.kind.load() != zodiak::link::kNode) continue;
        const auto t = zodiak::link::resolve (*eng, map.targetOf (i));
        if (t.b >= 0 && t.b != sl.b.load()) sl.b.store (t.b);
    }

    // The channels lined up with the latest one; the DAW told
    const int lat = eng->alignLatency();
    if (lat != getLatencySamples()) setLatencySamples (lat);
    ++rackGen;
}

int ZodiakProcessor::linkSource (const zodiak::link::Source& s, const zodiak::link::Target& t)
{
    const int slot = eng->links().assign (s, t, zodiak::link::defaultMode (*eng, t, s));
    pumpLinks();
    return slot;
}

void ZodiakProcessor::startLinkLearn (const zodiak::link::Target& t)
{
    learnTarget = t;
    eng->armLearn();
}

void ZodiakProcessor::cancelLinkLearn()
{
    learnTarget = {};
    eng->cancelLearn();
}

bool ZodiakProcessor::isLinkLearning (const zodiak::link::Target& t) const
{
    return learnTarget.valid() && learnTarget.sameAs (t) && eng->isLearnArmed();
}

juce::Result ZodiakProcessor::saveLinkMapAs (const juce::File& file)
{
    if (! eng->links().save (file))
        return juce::Result::fail ("Couldn't write " + file.getFullPathName());
    return juce::Result::ok();
}

juce::Result ZodiakProcessor::loadLinkMap (const juce::File& file)
{
    if (! eng->links().load (file))
        return juce::Result::fail (file.getFileName() + " is not a Zodiak MIDI map.");
    pumpLinks();
    return juce::Result::ok();
}

// Default.zset when it reads, else the set built in: never a Zodiak that can't play
void ZodiakProcessor::startUp()
{
    const auto file = zodiak::sets::defaultFile();
    if (file.existsAsFile())
        if (const auto xml = zodiak::sets::readFile (file))
            if (applySet (*xml))
            {
                currentSetFile = file;
                setApplied();
                return;
            }

    toRestore.clear();
    restoreIndex = 0;
    ++restoreRun;
    zodiak::sets::applyBuiltIn (*eng, info);
    currentSetFile = juce::File();
    setApplied();
}

void ZodiakProcessor::setApplied()
{
    ++generation;
    ++rackGen;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

//==============================================================================
//  A set, and then its plugins, one after another
//==============================================================================
bool ZodiakProcessor::applySet (const juce::XmlElement& xml)
{
    std::vector<zodiak::sets::PluginToLoad> plugins;
    if (! zodiak::sets::apply (xml, *eng, info, plugins))
        return false;

    toRestore = std::move (plugins);
    restoreIndex = 0;
    ++restoreRun;
    missing.clear();

    // Plugins are made on the message thread
    std::weak_ptr<bool> token = alive;
    const int run = restoreRun;
    juce::MessageManager::callAsync ([this, token, run]
    {
        if (token.expired() || run != restoreRun) return;
        restoreNext();
    });
    return true;
}

void ZodiakProcessor::restoreNext()
{
    while (restoreIndex < (int) toRestore.size())
    {
        const auto p = toRestore[(size_t) restoreIndex++];
        const auto where = p.channel < 0 ? juce::String (" (master)") : " (channel " + juce::String (p.channel + 1) + ")";

        if (p.builtInKind >= 0)
        {
            zodiak::sets::addBuiltInInsert (eng->masterRack(), p);
            continue;
        }

        if (p.isSonify() ? ! zodiak::rack::isSonifyPlugin (p.sonifyId) : ! p.file.exists())
        {
            missing.add (p.name + where);
            continue;
        }

        bool instrument = false;
        if (p.channel >= 0)
        {
            auto* n = eng->graph (p.channel).find (p.node);
            if (n == nullptr) continue;
            instrument = n->type == zodiak::graph::ntInstrument;
        }

        std::weak_ptr<bool> token = alive;
        const int run = restoreRun;
        zodiak::rack::loadPlugin (p.isSonify() ? zodiak::rack::Source::sonify (p.sonifyId) : zodiak::rack::Source (p.file),
                                  eng->sampleRate(), eng->maxBlock(), instrument,
            [this, token, run, p, where] (std::unique_ptr<zodiak::rack::Hosted> h, const juce::String&)
            {
                if (token.expired() || run != restoreRun) return;

                if (h == nullptr)
                    missing.add (p.name + where);
                else
                {
                    zodiak::rack::restoreState (*h, p.state);
                    if (p.channel >= 0)
                        eng->setNodePlugin (p.channel, p.node, std::move (h));
                    else
                    {
                        const int uid = eng->addMasterPlugin (std::move (h));
                        if (auto* ins = eng->masterRack().findInsert (uid)) ins->on.store (p.on);
                    }
                    ++rackGen;
                }
                restoreNext();
            });
        return;     // the next one when this one is in
    }
    ++rackGen;
}

void ZodiakProcessor::loadNodePlugin (int ch, int uid, const zodiak::rack::Source& source, std::function<void (const juce::String&)> done)
{
    auto* n = eng->graph (ch).find (uid);
    if (n == nullptr) return;
    const bool instrument = n->type == zodiak::graph::ntInstrument;
    std::weak_ptr<bool> token = alive;
    zodiak::rack::loadPlugin (source, eng->sampleRate(), eng->maxBlock(), instrument,
        [this, token, ch, uid, done] (std::unique_ptr<zodiak::rack::Hosted> h, const juce::String& error)
        {
            if (token.expired()) return;
            if (h != nullptr && eng->graph (ch).find (uid) != nullptr)
            {
                eng->setNodePlugin (ch, uid, std::move (h));
                ++rackGen;
                watchGraphs();
                if (done) done ({});
            }
            else if (done) done (error.isNotEmpty() ? error : juce::String ("The plugin didn't load."));
        });
}

juce::String ZodiakProcessor::instrumentName (int ch) const
{
    if (auto* n = eng->graph (ch).firstInstrument())
        if (n->plugin != nullptr) return n->plugin->name;
    return {};
}

//==============================================================================
void ZodiakProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    eng->prepare (sampleRate, samplesPerBlock);
}

bool ZodiakProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ZodiakProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    zodiak::Transport t;
    t.sampleRate = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            if (const auto b = pos->getBpm())             t.bpm = *b;
            if (const auto q = pos->getPpqPosition())     t.ppq = *q;
            if (const auto bs = pos->getPpqPositionOfLastBarStart()) t.barStart = *bs;
            if (const auto ts = pos->getTimeSignature()) { t.numerator = ts->numerator; t.denominator = ts->denominator; }
            t.playing = pos->getIsPlaying();
        }

    eng->process (buffer, midi, t);
}

// The set's note of each channel's pattern, from where the sequencer has it (a learnt
// controller may have chosen since)
void ZodiakProcessor::syncPatternInfo()
{
    for (int ch = 0; ch < zodiak::kNumChannels; ++ch)
    {
        info.bank[(size_t) ch] = bank (ch);
        info.variation[(size_t) ch] = variation (ch);
    }
}

void ZodiakProcessor::choosePattern (int ch, int bank, int variation)
{
    if (ch < 0 || ch >= zodiak::kNumChannels) return;
    info.bank[(size_t) ch] = juce::jlimit (0, 3, bank);
    info.variation[(size_t) ch] = juce::jlimit (0, 7, variation);
    eng->patterns().select (ch, zodiak::seq::patternIndex (bank, variation));
}

juce::Result ZodiakProcessor::loadSet (const juce::File& file)
{
    const auto xml = zodiak::sets::readFile (file);
    if (xml == nullptr || ! applySet (*xml))
        return juce::Result::fail (file.getFileName() + " is not a Zodiak set.");
    info.name = file.getFileNameWithoutExtension();
    currentSetFile = file;
    setApplied();
    return juce::Result::ok();
}

juce::Result ZodiakProcessor::saveSetAs (const juce::File& file)
{
    info.name = file.getFileNameWithoutExtension();
    syncPatternInfo();
    const auto xml = zodiak::sets::capture (*eng, info);
    const auto r = zodiak::sets::writeFile (*xml, file);
    if (r.wasOk())
    {
        currentSetFile = file;
        updateHostDisplay (ChangeDetails().withProgramChanged (true));
    }
    return r;
}

juce::Result ZodiakProcessor::saveSet()
{
    if (currentSetFile == juce::File())
        return juce::Result::fail ("This set has no file yet: use SAVE AS.");
    return saveSetAs (currentSetFile);
}

void ZodiakProcessor::newSet()
{
    toRestore.clear();
    restoreIndex = 0;
    ++restoreRun;
    zodiak::sets::applyBuiltIn (*eng, info);
    currentSetFile = juce::File();
    setApplied();
}

void ZodiakProcessor::setUiScale (float scale)
{
    uiScale = juce::jlimit (0.3f, 1.0f, std::round (scale * 20.0f) / 20.0f);      // 5% steps
    const auto f = zodiak::sets::zodiakFolder().getChildFile ("ui_scale.txt");
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::String (uiScale, 2));
}

void ZodiakProcessor::setRollDark (bool dark)
{
    rollDark = dark;
    const auto f = zodiak::sets::zodiakFolder().getChildFile ("roll_theme.txt");
    f.getParentDirectory().createDirectory();
    f.replaceWithText (dark ? "dark" : "bright");
}

//==============================================================================
//  The DAW's project: the whole set, plus where it came from
//==============================================================================
void ZodiakProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement state ("ZODIAK");
    state.setAttribute ("version", 3);
    state.setAttribute ("setFile", currentSetFile.getFullPathName());
    state.setAttribute ("uiScale", (double) uiScale);
    syncPatternInfo();
    state.addChildElement (zodiak::sets::capture (*eng, info).release());
    copyXmlToBinary (state, destData);
}

void ZodiakProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto state = getXmlFromBinary (data, sizeInBytes);
    const auto* set  = state != nullptr && state->hasTagName ("ZODIAK") ? state->getChildByName (zodiak::sets::kTag) : nullptr;

    // Whatever the project held, Zodiak ends up with a set it can play
    if (set == nullptr || ! applySet (*set))
    {
        startUp();
        return;
    }

    const auto path = state->getStringAttribute ("setFile");
    currentSetFile = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
    if (state->hasAttribute ("uiScale"))
        uiScale = juce::jlimit (0.3f, 1.0f, (float) state->getDoubleAttribute ("uiScale", 0.55));
    setApplied();
}

//==============================================================================
juce::AudioProcessorEditor* ZodiakProcessor::createEditor()
{
    return new ZodiakEditor (*this);
}

// Zodiak is built into Sonify (src/InternalPlugin.cpp makes it): no createPluginFilter().
