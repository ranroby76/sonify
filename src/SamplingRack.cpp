// D:\workspace\Fanan_web_hub\src\SamplingRack.cpp

#include "SamplingRack.h"

#include <algorithm>
#include <map>
#include <set>

namespace
{
    constexpr int capacityFloor = 4096;   // a step's sound holds this much (or two device blocks)
}

//==============================================================================
SamplingRack::SamplingRack (AudioEngine& audioEngine) : engine (audioEngine)
{
    auto sampler = std::make_unique<Node>();
    sampler->id = samplerId;
    sampler->kind = Kind::sampler;
    sampler->name = "Sampling";
    nodes.push_back (std::move (sampler));

    auto output = std::make_unique<Node>();
    output->id = outputId;
    output->kind = Kind::output;
    output->name = "Output";
    nodes.push_back (std::move (output));

    auto midiIn = std::make_unique<Node>();
    midiIn->id = midiInId;
    midiIn->kind = Kind::midiIn;
    midiIn->name = "MIDI In";
    nodes.push_back (std::move (midiIn));

    chunkNotes.ensureSize (8192);
    chunkLive.ensureSize (8192);
    chunkBoth.ensureSize (16384);
    engine.setMidiInInputs (AudioEngine::MidiIn::rack, midiInputs);
    rebuild();
}

SamplingRack::~SamplingRack()
{
    // The plan goes first (nothing plays them any more), then the plugins' windows, then them.
    {
        const juce::SpinLock::ScopedLockType lock (planLock);
        plan.reset();
    }

    closeWindows();

    for (auto& node : nodes)
        if (node->plugin != nullptr)
            node->plugin->deactivate();

    nodes.clear();
}

//==============================================================================
SamplingRack::Node* SamplingRack::find (int id) const noexcept
{
    for (const auto& node : nodes)
        if (node->id == id)
            return node.get();

    return nullptr;
}

juce::String SamplingRack::kindName (Kind kind)
{
    switch (kind)
    {
        case Kind::sampler:     return "sampler";
        case Kind::instrument:  return "instrument";
        case Kind::effect:      return "effect";
        case Kind::output:      return "output";
        case Kind::midiIn:      return "midiin";
    }

    return "effect";
}

bool SamplingRack::activatePlugin (Plugin& plugin)
{
    // Before the device starts, it's activated as it does (prepareRack).
    if (! engine.isAudioRunning())
        return true;

    if (plugin.isActive())
        plugin.deactivate();

    // The device's rate and block, as the rack plays them (when it started before the rack was made).
    if (rate <= 0.0 || blockSize <= 0)
    {
        const juce::SpinLock::ScopedLockType lock (planLock);
        rate = engine.getPlaybackRate();
        blockSize = engine.getBlockSize();
    }

    return plugin.activate (rate, blockSize);
}

int SamplingRack::addPlugin (std::unique_ptr<Plugin> plugin, const juce::String& format, float x, float y, juce::String& error)
{
    if (plugin == nullptr)
    {
        error = "No plugin.";
        return 0;
    }

    const auto role = plugin->getRole();

    if (role == Plugin::Role::noAudio || plugin->getMainOutputChannels() <= 0)
    {
        error = plugin->getDescriptor().name + " makes no sound of its own (a MIDI effect?): it can't go in the rack.";
        return 0;
    }

    if (! activatePlugin (*plugin))
    {
        error = plugin->getDescriptor().name + " couldn't start at " + juce::String (engine.getPlaybackRate() / 1000.0, 1) + " kHz.";
        return 0;
    }

    auto node = std::make_unique<Node>();
    node->id = nextId++;
    node->kind = role == Plugin::Role::instrument ? Kind::instrument : Kind::effect;
    node->format = format.isNotEmpty() ? format : plugin->getFormatName();
    node->file = plugin->getSourceFile();
    node->pluginId = plugin->getDescriptor().id;
    node->name = plugin->getDescriptor().name;
    node->vendor = plugin->getDescriptor().vendor;
    node->x = x;
    node->y = y;
    node->plugin = std::move (plugin);

    const auto id = node->id;
    nodes.push_back (std::move (node));
    rebuild();
    return id;
}

bool SamplingRack::removeNode (int id)
{
    if (id == samplerId || id == outputId || id == midiInId)
        return false;

    const auto found = std::find_if (nodes.begin(), nodes.end(), [id] (const auto& node) { return node->id == id; });

    if (found == nodes.end())
        return false;

    wires.erase (std::remove_if (wires.begin(), wires.end(), [id] (const Wire& w) { return w.from == id || w.to == id; }), wires.end());

    // Out of the plan first; then the node (its window, its plugin) goes outside the lock.
    std::unique_ptr<Node> gone = std::move (*found);
    nodes.erase (found);
    rebuild();

    if (gone->plugin != nullptr)
    {
        gone->plugin->hideNativeScreen();
        gone->plugin->deactivate();
    }

    return true;
}

bool SamplingRack::moveNode (int id, float x, float y)
{
    auto* node = find (id);

    if (node == nullptr || ! std::isfinite (x) || ! std::isfinite (y))
        return false;

    node->x = juce::jmax (0.0f, x);
    node->y = juce::jmax (0.0f, y);
    return true;
}

//==============================================================================
bool SamplingRack::reaches (int from, int to) const
{
    std::set<int> seen;
    std::vector<int> waiting { from };

    while (! waiting.empty())
    {
        const auto at = waiting.back();
        waiting.pop_back();

        if (at == to)
            return true;

        if (! seen.insert (at).second)
            continue;

        for (const auto& wire : wires)
            if (wire.from == at)
                waiting.push_back (wire.to);
    }

    return false;
}

juce::String SamplingRack::connect (int from, int to, int fromSide, int toSide)
{
    const auto* source = find (from);
    const auto* target = find (to);

    if (source == nullptr || target == nullptr || from == to)
        return "There's nothing to wire there.";

    // MIDI: the Sampling node's notes, or the MIDI In's, to a VSTi.
    if (isMidiSource (source->kind))
    {
        if (target->kind != Kind::instrument)
            return source->kind == Kind::midiIn ? "The MIDI In node's MIDI goes to a VSTi." : "The Sampling node's MIDI goes to a VSTi.";

        if (std::none_of (wires.begin(), wires.end(), [from, to] (const Wire& w) { return w.from == from && w.to == to; }))
            wires.push_back ({ from, to, 0, 0 });

        rebuild();
        return {};
    }

    // Sound: a VSTi's or an effect's, into an effect or the Output.
    if (source->kind == Kind::instrument || source->kind == Kind::effect)
    {
        if (target->kind != Kind::effect && target->kind != Kind::output)
            return target->kind == Kind::instrument ? "A VSTi takes MIDI, not sound: wire the sound to an effect or the Output."
                                                    : "Sound goes into an effect or the Output.";

        if (reaches (to, from))
            return "That wire would make a loop.";

        const auto add = [this, from, to] (int a, int b)
        {
            if (std::none_of (wires.begin(), wires.end(), [=] (const Wire& w) { return w.from == from && w.to == to && w.fromSide == a && w.toSide == b; }))
                wires.push_back ({ from, to, a, b });
        };

        // Node to node: both sides, each to its own. Otherwise the one side asked for (a side
        // not given is the other's).
        if (fromSide == bothSides && toSide == bothSides)
        {
            add (leftSide, leftSide);
            add (rightSide, rightSide);
        }
        else
        {
            const auto a = fromSide == bothSides ? toSide : fromSide;
            const auto b = toSide == bothSides ? fromSide : toSide;
            add (juce::jlimit (leftSide, rightSide, a), juce::jlimit (leftSide, rightSide, b));
        }

        rebuild();
        return {};
    }

    return "The Output's sound is what's sampled: it has nothing to wire on.";
}

bool SamplingRack::disconnect (int from, int to, int fromSide, int toSide)
{
    const auto before = wires.size();
    wires.erase (std::remove_if (wires.begin(), wires.end(), [=] (const Wire& w)
    {
        return w.from == from && w.to == to && (fromSide == bothSides || w.fromSide == fromSide) && (toSide == bothSides || w.toSide == toSide);
    }), wires.end());

    if (wires.size() == before)
        return false;

    rebuild();
    return true;
}

void SamplingRack::setMidiInputs (const juce::StringArray& ports)
{
    midiInputs = ports;
    midiInputs.trim();
    midiInputs.removeEmptyStrings();
    midiInputs.removeDuplicates (false);
    engine.setMidiInInputs (AudioEngine::MidiIn::rack, midiInputs);
}

void SamplingRack::setMuted (int id, bool muted)
{
    if (auto* node = find (id))
        node->muted.store (muted);
}

void SamplingRack::setPassThrough (int id, bool pass)
{
    if (auto* node = find (id); node != nullptr && node->kind == Kind::effect)
        node->pass.store (pass);
}

bool SamplingRack::showWindow (int id, juce::String& why)
{
    auto* node = find (id);

    if (node == nullptr || node->plugin == nullptr)
    {
        why = node != nullptr && node->missing ? node->name + " isn't loaded: " + node->file.getFullPathName() + " couldn't be opened."
                                               : "That node has no plugin window.";
        return false;
    }

    auto& plugin = *node->plugin;

    if (plugin.getScreenKind() == Plugin::ScreenKind::native && plugin.showNativeScreen (node->name))
        return true;

    why = node->name + " has no window of its own.";
    return false;
}

void SamplingRack::closeWindows()
{
    for (auto& node : nodes)
        if (node->plugin != nullptr)
            node->plugin->hideNativeScreen();
}

//==============================================================================
bool SamplingRack::hearsNotes (int id) const
{
    return std::any_of (wires.begin(), wires.end(), [id] (const Wire& w) { return w.from == samplerId && w.to == id; });
}

bool SamplingRack::hearsLive (int id) const
{
    return std::any_of (wires.begin(), wires.end(), [id] (const Wire& w) { return w.from == midiInId && w.to == id; });
}

bool SamplingRack::reachesOutput (int id) const
{
    // Through nodes that play: a missing plugin breaks its chain.
    std::set<int> seen;
    std::vector<int> waiting { id };

    while (! waiting.empty())
    {
        const auto at = waiting.back();
        waiting.pop_back();

        if (at == outputId)
            return true;

        const auto* node = find (at);

        if (node == nullptr || node->plugin == nullptr || ! seen.insert (at).second)
            continue;

        for (const auto& wire : wires)
            if (wire.from == at)
                waiting.push_back (wire.to);
    }

    return false;
}

bool SamplingRack::isFlagged (int id) const
{
    const auto* node = find (id);
    return node != nullptr && node->kind == Kind::instrument && node->plugin != nullptr && hearsNotes (id) && reachesOutput (id);
}

bool SamplingRack::isReady() const
{
    return std::any_of (nodes.begin(), nodes.end(), [this] (const auto& node) { return isFlagged (node->id); });
}

bool SamplingRack::hasPlugins() const
{
    return std::any_of (nodes.begin(), nodes.end(), [] (const auto& node) { return node->plugin != nullptr; });
}

//==============================================================================
std::unique_ptr<SamplingRack::Plan> SamplingRack::makePlan() const
{
    auto made = std::make_unique<Plan>();

    // The plugin nodes whose sound reaches the Output, upstream first (each after its inputs).
    std::vector<int> order;
    std::set<int> placed;
    std::vector<int> candidates;

    for (const auto& node : nodes)
        if ((node->kind == Kind::instrument || node->kind == Kind::effect) && node->plugin != nullptr && reachesOutput (node->id))
            candidates.push_back (node->id);

    bool progress = true;

    while (progress && placed.size() < candidates.size())
    {
        progress = false;

        for (const auto id : candidates)
        {
            if (placed.count (id) != 0)
                continue;

            // Ready once every input that plays is placed.
            const auto ready = std::all_of (wires.begin(), wires.end(), [&] (const Wire& w)
            {
                return w.to != id || w.from == samplerId || w.from == midiInId || placed.count (w.from) != 0
                    || std::find (candidates.begin(), candidates.end(), w.from) == candidates.end();
            });

            if (ready)
            {
                order.push_back (id);
                placed.insert (id);
                progress = true;
            }
        }
    }

    std::map<int, int> stepOf;
    const auto capacity = juce::jmax (capacityFloor, 2 * juce::jmax (1, blockSize > 0 ? blockSize : engine.getBlockSize()));

    for (const auto id : order)
    {
        auto* node = find (id);
        Step step;
        step.node = node;
        step.plugin = node->plugin.get();
        step.instrument = node->kind == Kind::instrument;
        step.hearsNotes = step.instrument && hearsNotes (id);
        step.hearsLive = step.instrument && hearsLive (id);

        for (const auto& wire : wires)
            if (wire.to == id)
                if (const auto found = stepOf.find (wire.from); found != stepOf.end())
                    step.inputs.push_back ({ found->second, wire.fromSide, wire.toSide });

        step.sound.setSize (2, capacity, false, true, false);
        stepOf[id] = static_cast<int> (made->steps.size());
        made->steps.push_back (std::move (step));
    }

    for (const auto& wire : wires)
        if (wire.to == outputId)
            if (const auto found = stepOf.find (wire.from); found != stepOf.end())
                made->outputInputs.push_back ({ found->second, wire.fromSide, wire.toSide });

    return made;
}

void SamplingRack::rebuild()
{
    auto next = makePlan();

    {
        const juce::SpinLock::ScopedLockType lock (planLock);
        std::swap (plan, next);
    }

    // The old plan goes here, outside the lock.
}

//==============================================================================
void SamplingRack::prepareRack (double sampleRate, int newBlockSize)
{
    const juce::SpinLock::ScopedLockType lock (planLock);
    rate = sampleRate;
    blockSize = newBlockSize;

    for (auto& node : nodes)
    {
        if (node->plugin == nullptr)
            continue;

        if (node->plugin->isActive())
            node->plugin->deactivate();

        node->plugin->activate (sampleRate, newBlockSize);
    }

    // The steps' sound as long as two of the device's blocks.
    if (plan != nullptr)
        for (auto& step : plan->steps)
            if (step.sound.getNumSamples() < 2 * newBlockSize)
                step.sound.setSize (2, 2 * newBlockSize, false, true, false);
}

void SamplingRack::releaseRack()
{
    const juce::SpinLock::ScopedLockType lock (planLock);

    for (auto& node : nodes)
        if (node->plugin != nullptr)
            node->plugin->stopProcessing();
}

void SamplingRack::renderRack (const juce::MidiBuffer& notes, const juce::MidiBuffer& live, float* leftOut, float* rightOut,
                               int numSamples, const clap_event_transport_t& transport) noexcept
{
    const juce::SpinLock::ScopedLockType lock (planLock);

    if (plan == nullptr || plan->steps.empty() || numSamples <= 0)
        return;

    // A block at a time as long as the plugins were readied for (and the steps hold).
    auto chunk = blockSize > 0 ? blockSize : engine.getBlockSize();

    for (const auto& step : plan->steps)
        chunk = juce::jmin (chunk, step.sound.getNumSamples());

    chunk = juce::jmax (1, chunk);

    const auto slice = [] (const juce::MidiBuffer& from, juce::MidiBuffer& into, int start, int count) noexcept
    {
        into.clear();

        for (const auto metadata : from)
            if (metadata.samplePosition >= start && metadata.samplePosition < start + count)
                into.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition - start);
    };

    // One side of a step's sound added into a buffer.
    const auto addSide = [this] (float* into, const Input& input, int count) noexcept
    {
        const auto& sound = plan->steps[static_cast<size_t> (input.step)].sound;
        juce::FloatVectorOperations::add (into, sound.getReadPointer (input.fromSide == rightSide ? 1 : 0), count);
    };

    for (int start = 0; start < numSamples; start += chunk)
    {
        const auto count = juce::jmin (chunk, numSamples - start);

        slice (notes, chunkNotes, start, count);
        slice (live, chunkLive, start, count);

        // A VSTi wired from both: the studio's notes and the live ones, together.
        chunkBoth.clear();
        chunkBoth.addEvents (chunkNotes, 0, -1, 0);
        chunkBoth.addEvents (chunkLive, 0, -1, 0);

        for (auto& step : plan->steps)
        {
            auto* soundLeft = step.sound.getWritePointer (0);
            auto* soundRight = step.sound.getWritePointer (1);
            juce::FloatVectorOperations::clear (soundLeft, count);
            juce::FloatVectorOperations::clear (soundRight, count);

            // Its plugin restarting (update): out of the plan, silent.
            if (step.node->restarting)
                continue;

            if (step.instrument)
            {
                const auto& heard = step.hearsNotes && step.hearsLive ? chunkBoth
                                  : step.hearsNotes ? chunkNotes
                                  : step.hearsLive ? chunkLive : noNotes;
                step.plugin->process (nullptr, nullptr, soundLeft, soundRight, count, heard, &transport);
            }
            else
            {
                // What's wired into each side, added; through the effect unless it's passed through.
                for (const auto& input : step.inputs)
                    addSide (input.toSide == rightSide ? soundRight : soundLeft, input, count);

                if (! step.node->pass.load (std::memory_order_relaxed))
                    step.plugin->process (soundLeft, soundRight, soundLeft, soundRight, count, noNotes, &transport);
            }

            if (step.node->muted.load (std::memory_order_relaxed))
            {
                juce::FloatVectorOperations::clear (soundLeft, count);
                juce::FloatVectorOperations::clear (soundRight, count);
            }
        }

        for (const auto& input : plan->outputInputs)
            addSide ((input.toSide == rightSide ? rightOut : leftOut) + start, input, count);
    }
}

//==============================================================================
bool SamplingRack::update()
{
    bool changed = false;

    for (auto& node : nodes)
    {
        if (node->plugin == nullptr)
            continue;

        auto& plugin = *node->plugin;
        const auto requests = plugin.takeRequests();

        if (requests.callback)
            plugin.runOnMainThread();

        if (requests.rescan || requests.restart)
        {
            // The node out of the plan under the lock, its plugin restarted outside it (the
            // engine, holding its graph lock, would otherwise wait on planLock all that time),
            // then back in.
            double restartRate = 0.0;
            int restartBlock = 0;

            {
                const juce::SpinLock::ScopedLockType lock (planLock);
                node->restarting = true;
                restartRate = rate;
                restartBlock = blockSize;
            }

            if (requests.restart && plugin.isActive() && restartRate > 0.0)
            {
                plugin.deactivate();
                plugin.activate (restartRate, restartBlock);
            }

            if (requests.rescan)
                plugin.rescanParameters();

            {
                const juce::SpinLock::ScopedLockType lock (planLock);
                node->restarting = false;
            }

            changed = true;
        }

        if (requests.flush && ! awake.load())
        {
            const juce::SpinLock::ScopedLockType lock (planLock);
            plugin.flushQueuedParameters();
        }
    }

    return changed;
}

//==============================================================================
juce::var SamplingRack::toVar (bool withStates) const
{
    juce::Array<juce::var> list;

    for (const auto& node : nodes)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", node->id);
        item->setProperty ("kind", kindName (node->kind));
        item->setProperty ("name", node->name);
        item->setProperty ("x", node->x);
        item->setProperty ("y", node->y);

        if (node->kind == Kind::midiIn)
            item->setProperty ("inputs", midiInputs);

        if (node->kind == Kind::instrument || node->kind == Kind::effect)
        {
            item->setProperty ("format", node->format);
            item->setProperty ("file", node->file.getFullPathName());
            item->setProperty ("pluginId", node->pluginId);
            item->setProperty ("vendor", node->vendor);
            item->setProperty ("muted", node->muted.load());
            item->setProperty ("pass", node->pass.load());

            if (node->missing)
                item->setProperty ("missing", true);

            if (withStates)
            {
                juce::String state = node->keptState;

                if (node->plugin != nullptr && node->plugin->canSaveState())
                {
                    juce::MemoryBlock block;

                    if (node->plugin->saveState (block) && block.getSize() > 0)
                        state = block.toBase64Encoding();
                }

                if (state.isNotEmpty())
                    item->setProperty ("state", state);
            }
            else
            {
                item->setProperty ("flagged", isFlagged (node->id));
                item->setProperty ("hasWindow", node->plugin != nullptr && node->plugin->getScreenKind() == Plugin::ScreenKind::native);
            }
        }

        list.add (juce::var (item.get()));
    }

    juce::Array<juce::var> links;

    for (const auto& wire : wires)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("from", wire.from);
        item->setProperty ("to", wire.to);
        item->setProperty ("fromSide", wire.fromSide);
        item->setProperty ("toSide", wire.toSide);
        item->setProperty ("midi", wire.from == samplerId || wire.from == midiInId);
        links.add (juce::var (item.get()));
    }

    juce::DynamicObject::Ptr result (new juce::DynamicObject());
    result->setProperty ("nodes", list);
    result->setProperty ("wires", links);

    if (! withStates)
        result->setProperty ("ready", isReady());

    return juce::var (result.get());
}

void SamplingRack::clear()
{
    std::vector<std::unique_ptr<Node>> gone;

    for (auto it = nodes.begin(); it != nodes.end();)
    {
        if ((*it)->kind == Kind::instrument || (*it)->kind == Kind::effect)
        {
            gone.push_back (std::move (*it));
            it = nodes.erase (it);
        }
        else
        {
            ++it;
        }
    }

    wires.clear();
    rebuild();

    for (auto& node : gone)
    {
        if (node->plugin != nullptr)
        {
            node->plugin->hideNativeScreen();
            node->plugin->deactivate();
        }
    }
}

void SamplingRack::restore (const juce::var& saved, juce::StringArray& problems)
{
    clear();

    const auto* list = saved.getProperty ("nodes", {}).getArray();

    if (list == nullptr)
        return;

    std::map<int, int> idOf;   // the kept ids to the new ones

    for (const auto& item : *list)
    {
        const auto keptId = static_cast<int> (item.getProperty ("id", 0));
        const auto kind = item.getProperty ("kind", {}).toString();
        const auto x = static_cast<float> (static_cast<double> (item.getProperty ("x", -1.0)));
        const auto y = static_cast<float> (static_cast<double> (item.getProperty ("y", -1.0)));

        // The Sampling, MIDI In and Output nodes are always there: only where they were (and the
        // MIDI In's ports).
        if (kind == "sampler" || kind == "output" || kind == "midiin")
        {
            const auto id = kind == "sampler" ? samplerId : kind == "output" ? outputId : midiInId;
            find (id)->x = x;
            find (id)->y = y;
            idOf[keptId] = id;

            if (kind == "midiin")
            {
                juce::StringArray ports;

                if (const auto* picked = item.getProperty ("inputs", {}).getArray())
                    for (const auto& port : *picked)
                        ports.add (port.toString());

                setMidiInputs (ports);
            }

            continue;
        }

        if (kind != "instrument" && kind != "effect")
            continue;

        const auto format = item.getProperty ("format", "VST3").toString();
        const juce::File file (item.getProperty ("file", {}).toString());
        const auto pluginId = item.getProperty ("pluginId", {}).toString();
        const auto state = item.getProperty ("state", {}).toString();
        juce::String error;
        int id = 0;

        if (juce::File::isAbsolutePath (file.getFullPathName()))
        {
            if (auto plugin = Plugin::loadFormat (file, format, pluginId, error))
            {
                if (state.isNotEmpty() && plugin->canSaveState())
                {
                    juce::MemoryBlock block;

                    if (block.fromBase64Encoding (state))
                        plugin->loadState (block);
                }

                id = addPlugin (std::move (plugin), format, x, y, error);
            }
        }

        if (id == 0)
        {
            // Kept as it was, silent, so its place, wires and settings aren't lost.
            auto node = std::make_unique<Node>();
            node->id = nextId++;
            node->kind = kind == "instrument" ? Kind::instrument : Kind::effect;
            node->format = format;
            node->file = file;
            node->pluginId = pluginId;
            node->name = item.getProperty ("name", file.getFileNameWithoutExtension()).toString();
            node->vendor = item.getProperty ("vendor", {}).toString();
            node->x = x;
            node->y = y;
            node->missing = true;
            node->keptState = state;
            id = node->id;
            nodes.push_back (std::move (node));
            problems.add (find (id)->name + ": " + (error.isNotEmpty() ? error : file.getFullPathName() + " couldn't be opened."));
        }

        if (auto* node = find (id))
        {
            node->muted.store (static_cast<bool> (item.getProperty ("muted", false)));
            node->pass.store (static_cast<bool> (item.getProperty ("pass", false)));
        }

        idOf[keptId] = id;
    }

    if (const auto* links = saved.getProperty ("wires", {}).getArray())
    {
        for (const auto& link : *links)
        {
            const auto from = idOf.find (static_cast<int> (link.getProperty ("from", 0)));
            const auto to = idOf.find (static_cast<int> (link.getProperty ("to", 0)));

            // As kept (a missing node's wires too: they play again once it's back).
            if (from == idOf.end() || to == idOf.end() || from->second == to->second)
                continue;

            const auto* source = find (from->second);
            const auto midi = source != nullptr && isMidiSource (source->kind);

            // Kept before sound came in pairs: one wire was both sides.
            if (! midi && ! link.hasProperty ("fromSide"))
            {
                wires.push_back ({ from->second, to->second, leftSide, leftSide });
                wires.push_back ({ from->second, to->second, rightSide, rightSide });
                continue;
            }

            const auto side = [] (const juce::var& value) { return juce::jlimit (0, 1, static_cast<int> (value)); };
            wires.push_back ({ from->second, to->second, midi ? 0 : side (link.getProperty ("fromSide", 0)), midi ? 0 : side (link.getProperty ("toSide", 0)) });
        }
    }

    rebuild();
}
