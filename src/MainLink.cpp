// D:\workspace\Fanan_web_hub\src\MainLink.cpp
//
// MIDI Link (MidiLink.h), the toolbar's strip: a device's knobs, sliders, wheels, pads and keys
// linked to Sonify's controls by dragging what the strip shows onto them. What a link moves, how the
// page hears of it, the Last Touched slot, and the map's files. As with the control surfaces,
// mixing this way is never an undo step (a send's turn is, as on the page).

#include "MainComponent.h"

#include <cmath>

//==============================================================================
void MainComponent::setupMidiLink()
{
    midiLink = std::make_unique<MidiLink> (static_cast<MidiLink::Host&> (*this), [this] (std::vector<MidiLink::Message>& into)
    {
        engine.takeLinkMessages (into);
    });

    // What a linked source sends plays Sonify, not a track (the Live Show Controller's pads too).
    midiLink->onLinkedSourcesChanged = [this] (const std::vector<MidiLink::Source>& sources)
    {
        midiLinkSources.clear();

        for (const auto& source : sources)
            midiLinkSources.push_back ({ source.port, source.kind == MidiLink::Kind::note ? 1 : source.kind == MidiLink::Kind::pitchBend ? 2 : 0,
                                         source.channel, source.number });

        applyLinkedSources();
    };

    // The map as it was left (the same in every song), kept up to date from now on.
    midiLink->load (MidiLink::getDefaultAutoFile());
    midiLink->setAutoFile (MidiLink::getDefaultAutoFile());
}

void MainComponent::applyLinkedSources()
{
    auto linked = midiLinkSources;

    if (liveControl != nullptr)
        for (const auto& pad : liveControl->getPadSources())
            linked.push_back ({ pad.port, pad.kind == MidiLink::Kind::note ? 1 : pad.kind == MidiLink::Kind::pitchBend ? 2 : 0,
                                pad.channel, pad.number });

    engine.setLinkedSources (std::move (linked));
}

void MainComponent::applyMidiLinkDevices()
{
    if (midiLink == nullptr)
        return;

    // Chips name the devices; a controller's notes are its pads (a keyboard's are for playing).
    std::map<juce::String, juce::String> names;
    juce::StringArray pads;

    for (const auto& device : prefs.devices)
    {
        if (device.kind == "surface" || device.input.isEmpty())
            continue;

        names.emplace (device.input, device.name);

        if (device.kind == "controller")
            pads.addIfNotAlreadyThere (device.input);
    }

    midiLink->setDevices (std::move (names), std::move (pads));
}

//==============================================================================
bool MainComponent::linkMovedLately (int channelId) const
{
    const auto found = linkMovedMs.find (channelId);
    return found != linkMovedMs.end() && juce::Time::getMillisecondCounterHiRes() - found->second < 1000.0;
}

int MainComponent::linkChannelId (const juce::StringArray& parts) const
{
    // "track.<n>": the n-th track as the page numbers them; "fx.<n>", "bus.<n>": the n-th FX channel, bus.
    if (parts.size() < 2 || (parts[0] != "track" && parts[0] != "fx" && parts[0] != "bus"))
        return 0;

    const auto kindOf = [] (const Track& track) { return track.kind == Track::Kind::fx ? "fx" : track.kind == Track::Kind::bus ? "bus" : "track"; };
    const auto wanted = parts[1].getIntValue();
    int count = 0;

    for (const auto& track : engine.getTracks())
        if (parts[0] == kindOf (*track) && ++count == wanted)
            return track->id;

    return 0;
}

int MainComponent::linkSendId (int channelId, int position)
{
    // The page's list: a send whose FX channel or plugin is gone isn't in it.
    const auto* track = engine.findTrack (channelId);

    if (track == nullptr)
        return 0;

    int count = 0;

    for (const auto& send : track->sends)
    {
        bool shown = false;

        if (send.sidechain)
        {
            auto* plugin = engine.getPlugin (send.target, send.slot);
            shown = plugin != nullptr && plugin->hasSidechain();
        }
        else if (const auto* fx = engine.findTrack (send.target))
        {
            shown = fx->kind == Track::Kind::fx;
        }

        if (shown && ++count == position)
            return send.id;
    }

    return 0;
}

Plugin* MainComponent::findLinkedPlugin (const juce::String& pluginId, int& channel, int& insertId)
{
    // The selected track's copy first (its instrument, then its inserts), then Main's, then the
    // other tracks' in order.
    const auto inTrack = [&] (const Track& track) -> Plugin*
    {
        if (track.instrument != nullptr && track.instrument->getDescriptor().id == pluginId)
        {
            channel = track.id;
            insertId = AudioEngine::instrumentSlot;
            return track.instrument.get();
        }

        for (int i = 0; i < track.inserts.size(); ++i)
        {
            if (const auto* insert = track.inserts.at (i); insert != nullptr && insert->plugin != nullptr
                                                           && insert->plugin->getDescriptor().id == pluginId)
            {
                channel = track.id;
                insertId = insert->id;
                return insert->plugin.get();
            }
        }

        return nullptr;
    };

    if (const auto* selected = engine.findTrack (engine.getSelectedTrackId()))
        if (auto* plugin = inTrack (*selected))
            return plugin;

    const auto& main = engine.getMainInserts();

    for (int i = 0; i < main.size(); ++i)
    {
        if (const auto* insert = main.at (i); insert != nullptr && insert->plugin != nullptr && insert->plugin->getDescriptor().id == pluginId)
        {
            channel = AudioEngine::mainChannel;
            insertId = insert->id;
            return insert->plugin.get();
        }
    }

    for (const auto& track : engine.getTracks())
        if (auto* plugin = inTrack (*track))
            return plugin;

    return nullptr;
}

//==============================================================================
//==============================================================================
// Focus: knobs, faders and buttons that move the selected track's controls, whichever track it
// is (its fader, pan, sends, Mute, Solo, Record, Monitor), and eight knobs for its instrument's
// first knobs (an effect track: its first insert's).
juce::String MainComponent::focusKey (const juce::String& key) const
{
    const auto* selected = engine.findTrack (engine.getSelectedTrackId());

    if (selected == nullptr || ! key.startsWith ("focus."))
        return {};

    const auto kindOf = [] (const Track& track) { return track.kind == Track::Kind::fx ? "fx" : track.kind == Track::Kind::bus ? "bus" : "track"; };
    const juce::String kind (kindOf (*selected));
    int number = 0;

    for (const auto& track : engine.getTracks())
    {
        if (kind != kindOf (*track))
            continue;

        ++number;

        if (track->id == selected->id)
            return kind + "." + juce::String (number) + key.fromFirstOccurrenceOf ("focus", false, false);
    }

    return {};
}

Plugin* MainComponent::focusKnob (int k, int& channel, int& insertId, int& index)
{
    auto* track = engine.findTrack (engine.getSelectedTrackId());

    if (track == nullptr || k < 1)
        return nullptr;

    // Its instrument's knobs; without one (or none to turn), its first insert's that has some.
    const auto pick = [&] (Plugin* plugin, int slot) -> Plugin*
    {
        if (plugin == nullptr)
            return nullptr;

        int count = 0;
        const auto& parameters = plugin->getParameters();

        for (size_t i = 0; i < parameters.size(); ++i)
        {
            if (parameters[i].hidden || parameters[i].readOnly)
                continue;

            if (++count == k)
            {
                channel = track->id;
                insertId = slot;
                index = static_cast<int> (i);
                return plugin;
            }
        }

        return nullptr;
    };

    const auto hasKnobs = [] (Plugin* plugin)
    {
        return plugin != nullptr && std::any_of (plugin->getParameters().begin(), plugin->getParameters().end(),
                                                 [] (const Plugin::Parameter& p) { return ! p.hidden && ! p.readOnly; });
    };

    if (hasKnobs (track->instrument.get()))
        return pick (track->instrument.get(), AudioEngine::instrumentSlot);

    for (int i = 0; i < track->inserts.size(); ++i)
        if (const auto* insert = track->inserts.at (i); insert != nullptr && hasKnobs (insert->plugin.get()))
            return pick (insert->plugin.get(), insert->id);

    return nullptr;
}

void MainComponent::linkMove (const MidiLink::Target& target, float position)
{
    position = juce::jlimit (0.0f, 1.0f, position);
    const auto now = juce::Time::getMillisecondCounterHiRes();

    // Focus: the selected track's own control (its knob: as a plugin's knob moves).
    if (target.key.startsWith ("focus."))
    {
        if (target.key.startsWith ("focus.knob."))
        {
            int channel = 0, insertId = 0, index = -1;

            if (auto* plugin = focusKnob (target.key.fromLastOccurrenceOf (".", false, false).getIntValue(), channel, insertId, index))
            {
                const auto& parameter = plugin->getParameters()[static_cast<size_t> (index)];
                auto value = parameter.minValue + static_cast<double> (position) * (parameter.maxValue - parameter.minValue);

                if (parameter.stepped && plugin->getFormatName() != "VST3")
                    value = std::round (value);

                engine.setPluginParameter (channel, insertId, index, value);
                automationParamTouched (channel, insertId, index);
                markChanged();
            }

            return;
        }

        MidiLink::Target own;
        own.key = focusKey (target.key);

        if (own.key.isNotEmpty())
            linkMove (own, position);

        return;
    }

    if (target.key == "plugin")
    {
        int channel = 0, insertId = 0;
        auto* plugin = findLinkedPlugin (target.pluginId, channel, insertId);
        const auto index = plugin != nullptr ? plugin->findParameter (target.paramId) : -1;

        if (index < 0)
            return;

        const auto& parameter = plugin->getParameters()[static_cast<size_t> (index)];
        auto value = parameter.minValue + static_cast<double> (position) * (parameter.maxValue - parameter.minValue);

        // A stepped parameter in its own values (CLAP's) lands on one; VST3's are 0..1 and the
        // plugin steps them itself.
        if (parameter.stepped && plugin->getFormatName() != "VST3")
            value = std::round (value);

        engine.setPluginParameter (channel, insertId, index, value);
        automationParamTouched (channel, insertId, index);
        markChanged();
        return;
    }

    if (target.key == "main.volume")
    {
        setChannelVolume (ControlSurfaces::mainChannel, ControlSurfaces::faderToDb (position));
        linkMovedMs[ControlSurfaces::mainChannel] = now;
        return;
    }

    const auto parts = juce::StringArray::fromTokens (target.key, ".", "");

    if (parts[0] == "board")
    {
        linkMoveBoard (parts, position);
        return;
    }

    if (parts[0] == "live")
    {
        linkMoveLive (parts, position);
        return;
    }

    const auto id = parts.size() >= 3 ? linkChannelId (parts) : 0;

    if (id == 0)
        return;

    if (parts[2] == "volume")
    {
        setChannelVolume (id, ControlSurfaces::faderToDb (position));
        linkMovedMs[id] = now;
    }
    else if (parts[2] == "pan")
    {
        // The knob's middle is the centre (127 steps have no exact middle).
        const auto pan = position * 2.0f - 1.0f;
        setChannelPan (id, std::abs (pan) < 0.01f ? 0.0f : pan);
        linkMovedMs[id] = now;
    }
    else if (parts[2] == "send" && parts.size() >= 4)
    {
        if (const auto sendId = linkSendId (id, parts[3].getIntValue()); sendId != 0)
            setChannelSendLevel (id, sendId, ControlSurfaces::faderToDb (position));
    }
}

void MainComponent::linkSet (const MidiLink::Target& target, bool on)
{
    const auto& key = target.key;

    if (key.startsWith ("focus."))
    {
        MidiLink::Target own;
        own.key = focusKey (key);

        if (own.key.isNotEmpty())
            linkSet (own, on);

        return;
    }

    if (key == "main.mute")
    {
        engine.setMasterMute (on);
        markChanged();
        surfaceTracksDue = true;
        return;
    }

    if (key.startsWith ("transport."))
    {
        const auto action = key.fromFirstOccurrenceOf (".", false, false);
        const auto state = engine.getTransport().getState();

        if (action == "play")
        {
            if (on != state.playing)
                surfaceTransport (on ? "play" : "stop");
        }
        else if (action == "stop" || action == "start")
        {
            if (on)
                surfaceTransport (action);
        }
        else if (action == "record")
        {
            if (on != engine.isRecording())
                surfaceTransport ("record");
        }
        else if (action == "loop")
        {
            surfaceLoop (on);
        }
        else if (action == "metronome")
        {
            surfaceMetronome (on);
        }

        return;
    }

    const auto parts = juce::StringArray::fromTokens (key, ".", "");

    if (parts[0] == "board")
    {
        linkSetBoard (parts, on);
        return;
    }

    if (parts[0] == "live")
    {
        linkSetLive (parts, on);
        return;
    }

    if (parts[0] == "launch")
    {
        linkSetLauncher (parts, on);
        return;
    }

    const auto id = parts.size() >= 3 ? linkChannelId (parts) : 0;

    if (id == 0)
        return;

    if (parts[2] == "mute")
    {
        setChannelMute (id, on);
    }
    else if (parts[2] == "solo")
    {
        setChannelSolo (id, on);
    }
    else if (parts[2] == "arm")
    {
        setChannelRecordArm (id, on);
    }
    else if (parts[2] == "monitor")
    {
        engine.setTrackMonitor (id, on, true);
        markChanged();
        surfaceTracksDue = true;
    }
}

bool MainComponent::linkIsOn (const MidiLink::Target& target)
{
    const auto& key = target.key;

    if (key.startsWith ("focus."))
    {
        MidiLink::Target own;
        own.key = focusKey (key);
        return own.key.isNotEmpty() && linkIsOn (own);
    }

    if (key == "main.mute")
        return engine.isMasterMuted();

    if (key.startsWith ("transport."))
    {
        const auto action = key.fromFirstOccurrenceOf (".", false, false);
        const auto state = engine.getTransport().getState();
        return action == "play"      ? state.playing
             : action == "record"    ? engine.isRecording()
             : action == "loop"      ? state.loopEnabled
             : action == "metronome" ? state.metronomeEnabled
                                     : false;
    }

    const auto parts = juce::StringArray::fromTokens (key, ".", "");

    if (parts[0] == "board")
        return linkIsOnBoard (parts);

    if (parts[0] == "live")
        return linkIsOnLive (parts);

    const auto* track = parts.size() >= 3 ? engine.findTrack (linkChannelId (parts)) : nullptr;

    if (track == nullptr)
        return false;

    return parts[2] == "mute"    ? track->mute.load()
         : parts[2] == "solo"    ? track->solo.load()
         : parts[2] == "arm"     ? track->recordArm.load()
         : parts[2] == "monitor" ? track->monitor.load()
                                 : false;
}

//==============================================================================
void MainComponent::updateMidiLink()
{
    if (midiLink == nullptr)
        return;

    auto due = midiLink->takeChanged();

    // A knob taken hold of in a plugin's window: the strip's Last Touched slot.
    const auto look = [&] (int channel, int insertId, Plugin* plugin, const juce::String& where)
    {
        if (plugin == nullptr)
            return;

        const auto index = plugin->takeTouchedParameter();
        const auto& parameters = plugin->getParameters();

        if (! juce::isPositiveAndBelow (index, static_cast<int> (parameters.size())))
            return;

        // Recorded too, when its track records automation (Touch, Latch, Write) and the song plays.
        automationParamTouched (channel, insertId, index);

        const auto& parameter = parameters[static_cast<size_t> (index)];

        if (parameter.hidden || parameter.readOnly)
            return;

        juce::DynamicObject::Ptr touched (new juce::DynamicObject());
        touched->setProperty ("pluginId", plugin->getDescriptor().id);
        touched->setProperty ("pluginName", plugin->getDescriptor().name);
        touched->setProperty ("paramId", static_cast<juce::int64> (parameter.id));
        touched->setProperty ("paramName", parameter.name);
        touched->setProperty ("where", where);
        touched->setProperty ("channel", channel);
        touched->setProperty ("insertId", insertId);

        const juce::var now (touched.get());

        if (juce::JSON::toString (now, true) != juce::JSON::toString (linkTouched, true))
        {
            linkTouched = now;
            due = true;
        }
    };

    for (const auto& track : engine.getTracks())
    {
        look (track->id, AudioEngine::instrumentSlot, track->instrument.get(), track->name);

        for (int i = 0; i < track->inserts.size(); ++i)
            if (const auto* insert = track->inserts.at (i))
                look (track->id, insert->id, insert->plugin.get(), track->name);
    }

    const auto& main = engine.getMainInserts();

    for (int i = 0; i < main.size(); ++i)
        if (const auto* insert = main.at (i))
            look (AudioEngine::mainChannel, insert->id, insert->plugin.get(), "Main");

    // MIDI Learn linked a control: the page shows its link.
    if (const auto learned = midiLink->takeLearned())
    {
        linkLearnedKey = learned->key();
        due = true;
    }

    if (due)
        pushMidiLink();
}

void MainComponent::pushMidiLink()
{
    if (webView == nullptr || midiLink == nullptr)
        return;

    const auto kindOf = [] (MidiLink::Kind kind) { return kind == MidiLink::Kind::note ? "note" : kind == MidiLink::Kind::pitchBend ? "pb" : "cc"; };

    juce::Array<juce::var> recent, links;

    for (const auto& chip : midiLink->getRecent())
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("key", chip.source.key());
        item->setProperty ("label", chip.source.label());
        item->setProperty ("device", midiLink->deviceOf (chip.source.port));
        item->setProperty ("channel", chip.source.channel);
        item->setProperty ("kind", kindOf (chip.source.kind));

        if (const auto* link = midiLink->findLink (chip.source))
        {
            juce::DynamicObject::Ptr linked (new juce::DynamicObject());
            linked->setProperty ("target", link->target.toVar());
            linked->setProperty ("mode", MidiLink::modeName (link->mode));
            item->setProperty ("link", juce::var (linked.get()));
        }

        recent.add (juce::var (item.get()));
    }

    for (const auto& link : midiLink->getLinks())
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("source", link.source.key());
        item->setProperty ("label", link.source.label());
        item->setProperty ("device", midiLink->deviceOf (link.source.port));
        item->setProperty ("channel", link.source.channel);
        item->setProperty ("kind", kindOf (link.source.kind));
        item->setProperty ("target", link.target.toVar());
        item->setProperty ("mode", MidiLink::modeName (link.mode));
        links.add (juce::var (item.get()));
    }

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    state->setProperty ("recent", recent);
    state->setProperty ("links", links);
    state->setProperty ("touched", linkTouched);
    state->setProperty ("message", std::exchange (linkMessage, {}));
    state->setProperty ("learning", midiLink->getLearning().has_value() ? midiLink->getLearning()->toVar() : juce::var());
    state->setProperty ("learned", std::exchange (linkLearnedKey, {}));
    webView->emitEventIfBrowserIsVisible ("midiLink", juce::var (state.get()));
}

void MainComponent::handleMidiLink (const juce::String& what, const juce::var& event)
{
    if (midiLink == nullptr)
        return;

    if (what == "set")
    {
        const auto source = MidiLink::Source::fromKey (event.getProperty ("source", {}).toString());
        const auto target = MidiLink::Target::fromVar (event.getProperty ("target", {}));

        if (source.has_value())
        {
            linksBeforeEdit = midiLink->getLinks();

            if (! midiLink->link (*source, target, MidiLink::modeFromName (event.getProperty ("mode", {}).toString(), MidiLink::defaultModeFor (*source, target))))
                linkMessage = "That control can't be linked.";
        }
    }
    else if (what == "mode")
    {
        if (const auto source = MidiLink::Source::fromKey (event.getProperty ("source", {}).toString()))
            midiLink->setMode (*source, MidiLink::modeFromName (event.getProperty ("mode", {}).toString(), MidiLink::Mode::toggle));
    }
    else if (what == "clear")
    {
        // {source}: that source's link (the MIDI Link window's bin); {target}: every link to that control.
        if (const auto source = MidiLink::Source::fromKey (event.getProperty ("source", {}).toString()))
            midiLink->unlinkSource (*source);
        else
            midiLink->unlinkTarget (event.getProperty ("target", {}).toString());
    }
    else if (what == "cancel")
    {
        midiLink->setLinks (linksBeforeEdit);
    }
    else if (what == "learn")
    {
        // MIDI Learn: {target} waits for the next control a device moves; no target: Learn stops.
        if (const auto target = event.getProperty ("target", {}); target.isObject())
        {
            const auto wanted = MidiLink::Target::fromVar (target);

            if (wanted.isValid())
            {
                linksBeforeEdit = midiLink->getLinks();
                midiLink->learn (wanted);
            }
            else
            {
                linkMessage = "That control can't be linked.";
            }
        }
        else
        {
            midiLink->stopLearning();
        }
    }
    else if (what == "menu")
    {
        const auto action = event.getProperty ("action", {}).toString();

        if (action == "clearAll")
        {
            linksBeforeEdit = midiLink->getLinks();
            midiLink->clear();
        }
        else if (action == "save" || action == "load")
        {
            chooseMidiLinkFile (action == "save");
        }
    }

    pushMidiLink();
}

void MainComponent::chooseMidiLinkFile (bool saving)
{
    if (choosingFile || midiLink == nullptr)
        return;

    choosingFile = true;
    const auto folder = MidiLink::getMapsFolder();
    folder.createDirectory();

    const auto pattern = juce::String ("*") + MidiLink::fileExtension;
    linkChooser = std::make_unique<juce::FileChooser> (saving ? "Save the MIDI Link map" : "Load a MIDI Link map",
                                                       saving ? folder.getChildFile (juce::String ("My Controllers") + MidiLink::fileExtension) : folder,
                                                       pattern);

    const auto chooserFlags = saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                                     : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    linkChooser->launchAsync (chooserFlags, [safeThis = juce::Component::SafePointer<MainComponent> (this), saving] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        auto file = chooser.getResult();

        if (file == juce::File() || safeThis->midiLink == nullptr)
            return;

        if (saving)
        {
            file = file.withFileExtension (MidiLink::fileExtension);

            if (! safeThis->midiLink->save (file))
                safeThis->linkMessage = "Couldn't save " + file.getFullPathName();
        }
        else
        {
            safeThis->linksBeforeEdit = safeThis->midiLink->getLinks();

            if (const auto loaded = safeThis->midiLink->load (file); loaded.failed())
                safeThis->linkMessage = loaded.getErrorMessage();
        }

        safeThis->pushMidiLink();
    });
}
