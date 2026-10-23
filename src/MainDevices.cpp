// D:\workspace\Fanan_web_hub\src\MainDevices.cpp
//
// Sonify devices (DeviceHost.h): made by Sonify AI with the Device Studio (DeviceStudio.h) in the
// Plugins Creator Studio's Build with AI (type MIDI FX), played in instrument tracks' MIDI FX, kept
// in My Devices and in the songs that use them. A device opened from the page (a track's MIDI FX
// window, the MIDI FX menu's New Device) shows there. What devices move in Sonify (their outputs,
// mapped by the user, and controls they name) goes through MIDI Link's host, so it moves exactly
// what a MIDI controller would.

#include "MainComponent.h"
#include "Licences.h"
#include "CreatorStudio.h"

namespace
{
    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    juce::var copyOf (const juce::var& value)
    {
        return juce::JSON::parse (juce::JSON::toString (value, true));
    }

    /** The track's name as MIDI Link and the page number it: "Track 2 (Piano)". */
    juce::String trackLabel (const AudioEngine& engine, int trackId)
    {
        int number = 0;

        for (const auto& track : engine.getTracks())
        {
            if (track->kind == Track::Kind::fx || track->kind == Track::Kind::bus)
                continue;

            ++number;

            if (track->id == trackId)
                return "Track " + juce::String (number) + " (" + track->name + ")";
        }

        return {};
    }
}

//==============================================================================
void MainComponent::setupDevices()
{
    MidiFx::setExternalMaker (Devices::makeFx);
    Devices::setMyDevicesFolder (UserData::getFolder().getChildFile ("Devices"));
    Devices::setSampleRate (engine.getPlaybackRate());

    deviceStudio = std::make_unique<DeviceStudio> (engine, DeviceStudio::Hooks {
        [this] (const std::shared_ptr<Devices::Build>& build, bool addIfNowhere) { return placeDeviceBuild (build, addIfNowhere); },
        [this] { deviceStudioDue = true; } });

    deviceAgent = std::make_unique<AiAgent> (*server, *deviceStudio, AiAgent::Hooks {
        [] (const juce::String&) {},
        [this] (const juce::String&, bool)
        {
            // The conversation stays with the device, so it carries on next time.
            if (deviceStudio->isOpen())
                deviceStudio->keepConversation (deviceAgent->saveConversation());

            pushDeviceAi();
        },
        [this] { pushDeviceAi(); } });

    deviceTimer.startTimerHz (60);
}

//==============================================================================
juce::String MainComponent::placeDeviceBuild (const std::shared_ptr<Devices::Build>& build, bool addIfNowhere)
{
    if (build == nullptr)
        return {};

    // Every place the device is: the new build, each keeping its settings and maps.
    const auto replaced = engine.replaceMidiFx ([&build] (const MidiFx::Device& old) -> std::unique_ptr<MidiFx::Device>
    {
        const auto* fx = dynamic_cast<const Devices::DeviceFx*> (&old);

        if (fx == nullptr || fx->getBuild() == nullptr || fx->getBuild()->deviceId != build->deviceId || fx->getBuild() == build)
            return nullptr;

        auto fresh = std::make_unique<Devices::DeviceFx> (build, Devices::currentSampleRate());

        for (size_t i = 0; i < fresh->spec.params.size(); ++i)
            for (size_t j = 0; j < old.spec.params.size(); ++j)
                if (old.spec.params[j].id == fresh->spec.params[i].id)
                    fresh->set (static_cast<int> (i), old.get (static_cast<int> (j)));

        fresh->maps = copyOf (fx->maps);
        return fresh;
    });

    // (The song now holds the new build: it has changed, as when a device is updated.)
    if (replaced > 0)
        markChanged();

    juce::StringArray places;

    for (const auto& track : engine.getTracks())
        for (int i = 0; i < track->midiFx.size(); ++i)
            if (const auto* fx = dynamic_cast<const Devices::DeviceFx*> (track->midiFx.at (i)->device.get());
                fx != nullptr && fx->getBuild() != nullptr && fx->getBuild()->deviceId == build->deviceId)
                places.addIfNotAlreadyThere (trackLabel (engine, track->id));

    if (! places.isEmpty())
    {
        pushTracks();
        return places.joinIntoString (", ");
    }

    if (! addIfNowhere)
        return {};

    // Nowhere yet: on the track it's tried on, the selected one, or the first instrument track.
    const auto isInstrument = [this] (int id)
    {
        const auto* track = engine.findTrack (id);
        return track != nullptr && track->kind == Track::Kind::instrument && ! track->midiFx.isFull();
    };

    auto trackId = isInstrument (deviceTryTrack) ? deviceTryTrack : isInstrument (engine.getSelectedTrackId()) ? engine.getSelectedTrackId() : 0;

    for (const auto& track : engine.getTracks())
        if (trackId == 0 && isInstrument (track->id))
            trackId = track->id;

    if (trackId == 0 || ! addDeviceToTrack (trackId, build, false))
        return {};

    deviceTryTrack = trackId;
    return trackLabel (engine, trackId);
}

bool MainComponent::addDeviceToTrack (int trackId, const std::shared_ptr<Devices::Build>& build, bool open)
{
    if (build == nullptr)
        return false;

    auto settings = object();
    settings.getDynamicObject()->setProperty ("device", build->describe());

    const auto before = engine.getMidiFx (trackId);
    const auto added = engine.addMidiFx (trackId, "device", -1, settings);

    if (added == 0)
        return false;

    addToHistory ("Add " + build->name, new Edits::MidiFxChanged (engine, trackId, before, engine.getMidiFx (trackId)));
    pushTracks();

    if (webView != nullptr)
    {
        auto data = object();
        data.getDynamicObject()->setProperty ("trackId", trackId);
        data.getDynamicObject()->setProperty ("fxId", added);
        data.getDynamicObject()->setProperty ("open", open);
        webView->emitEventIfBrowserIsVisible ("midiFxAdded", data);
    }

    return true;
}

juce::var MainComponent::deviceSettingsFor (const juce::var& event, juce::String& error)
{
    // A device from My Devices (its file), or one already here (its build, by hash).
    std::shared_ptr<Devices::Build> build;
    const auto path = event.getProperty ("file", {}).toString();

    if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
        build = DeviceStudio::buildOfFile (juce::File (path), error);
    else if (const auto hash = event.getProperty ("device", {}).getProperty ("hash", {}).toString(); hash.isNotEmpty())
        build = Devices::findBuild (hash, event.getProperty ("device", {}).getProperty ("deviceId", {}).toString());   // its own device's (not another with the same code)

    if (build == nullptr)
    {
        if (error.isEmpty())
            error = "That device isn't here.";

        return {};
    }

    auto settings = copyOf (event);
    settings.getDynamicObject()->setProperty ("device", build->describe());
    return settings;
}

//==============================================================================
void MainComponent::handleDeviceStudio (const juce::var& event)
{
    const auto action = event.getProperty ("action", {}).toString();
    const auto problem = [this] (const juce::String& text)
    {
        if (webView != nullptr)
        {
            auto data = object();
            data.getDynamicObject()->setProperty ("message", text);
            webView->emitEventIfBrowserIsVisible ("deviceStudioMessage", data);
        }
    };

    // Switching devices while Sonify AI works on one would pull it from under it.
    const auto switching = action == "new" || action == "open" || action == "openSlot" || action == "close" || action == "trash";

    if (switching && deviceAgent->isBusy())
        return problem ("Fanan AI is working on this device: stop it first (or wait for it).");

    // ...or the plugin the Plugins Creator Studio has open, which a device would take the place of.
    if (switching && creator != nullptr && creator->isBusy())
        return problem ("Fanan AI is still working on the plugin in the Plugins Creator Studio: once it's done (or stopped), open the device again.");

    // The conversation that comes with the device opened.
    const auto takeConversation = [this]
    {
        deviceAgent->restoreConversation (deviceStudio->isOpen() ? deviceStudio->getDevice().chat : juce::var());
    };

    if (action == "shown")
    {
        deviceStudioShown = static_cast<bool> (event.getProperty ("shown", false));
        deviceStudio->getLibrary (true);
        pushDeviceStudio();
    }
    else if (action == "state")
    {
        pushDeviceStudio();
        pushDeviceAi();
    }
    else if (action == "new")
    {
        deviceStudio->newDevice();
        takeConversation();
    }
    else if (action == "open")
    {
        if (const auto result = deviceStudio->open (juce::File (event.getProperty ("file", {}).toString())); result.failed())
            return problem (result.getErrorMessage());

        takeConversation();
    }
    else if (action == "openSlot")
    {
        // A device's window: "Open in Device Studio". It's tried on that track from now on.
        const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
        const auto fxId = static_cast<int> (event.getProperty ("fxId", 0));
        const auto* track = engine.findTrack (trackId);
        const auto* slot = track != nullptr ? track->midiFx.find (fxId) : nullptr;
        const auto* fx = slot != nullptr ? dynamic_cast<const Devices::DeviceFx*> (slot->device.get()) : nullptr;

        if (fx == nullptr || fx->getBuild() == nullptr)
            return problem ("That device can't be opened here (it isn't on this computer).");

        if (! deviceStudio->isOpen() || deviceStudio->getDevice().id != fx->getBuild()->deviceId)
        {
            if (const auto result = deviceStudio->openBuild (fx->getBuild()); result.failed())
                return problem (result.getErrorMessage());

            takeConversation();
        }

        deviceTryTrack = trackId;
    }
    else if (action == "close")
    {
        deviceStudio->close();
        takeConversation();
    }
    else if (action == "build")
    {
        deviceStudio->buildDevice ([this] (const juce::var& result)
        {
            if (result.hasProperty ("error"))
                if (webView != nullptr)
                    webView->emitEventIfBrowserIsVisible ("deviceStudioMessage", result);

            pushDeviceStudio();
        });
    }
    else if (action == "rename")
    {
        if (const auto result = deviceStudio->setInfo (event.getProperty ("name", {}).toString(), event.getProperty ("description", {}).toString()); result.failed())
            return problem (result.getErrorMessage());
    }
    else if (action == "tryOn")
    {
        deviceTryTrack = static_cast<int> (event.getProperty ("trackId", 0));
    }
    else if (action == "placeOn")
    {
        // Another place for the open device's build: a track of the user's choosing.
        const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
        const auto build = deviceStudio->getBuild();

        if (build == nullptr)
            return problem ("Build the device first.");

        const auto* track = engine.findTrack (trackId);

        if (track == nullptr || track->kind != Track::Kind::instrument)
            return problem ("Devices go on instrument tracks.");

        if (track->midiFx.isFull())
            return problem ("That track's MIDI FX are full.");

        if (! addDeviceToTrack (trackId, build, false))
            return problem ("It couldn't be added to that track.");

        deviceTryTrack = trackId;
    }
    else if (action == "trash")
    {
        if (const auto result = deviceStudio->trash (juce::File (event.getProperty ("file", {}).toString())); result.failed())
            return problem (result.getErrorMessage());

        takeConversation();
    }
    else if (action == "reveal")
    {
        const auto folder = Devices::myDevicesFolder();
        folder.createDirectory();
        folder.revealToUser();
    }
    else if (action == "import")
    {
        if (deviceAgent->isBusy())
            return problem ("Fanan AI is working on this device: stop it first (or wait for it).");

        fileChooser = std::make_unique<juce::FileChooser> ("Add a Sonify device", juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                           juce::String ("*") + Devices::fileExtension);
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [safe = juce::Component::SafePointer<MainComponent> (this), problem] (const juce::FileChooser& chooser)
        {
            if (safe == nullptr || ! chooser.getResult().existsAsFile())
                return;

            // Sonify AI may have started on the open device while the chooser was up.
            if (safe->deviceAgent->isBusy())
                return problem ("Fanan AI is working on this device: stop it first (or wait for it).");

            if (const auto result = safe->deviceStudio->open (chooser.getResult()); result.failed())
                return problem (result.getErrorMessage());

            safe->deviceAgent->restoreConversation (safe->deviceStudio->getDevice().chat);
            safe->pushDeviceStudio();
            safe->pushDeviceAi();

            if (safe->creator != nullptr)
                safe->creator->showMidiDevice();
        });
        return;
    }
    else if (action == "export")
    {
        if (! deviceStudio->isOpen())
            return;

        const auto suggested = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                   .getChildFile (juce::File::createLegalFileName (deviceStudio->getDevice().name) + Devices::fileExtension);
        fileChooser = std::make_unique<juce::FileChooser> ("Save a copy of the device", suggested, juce::String ("*") + Devices::fileExtension);
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safe = juce::Component::SafePointer<MainComponent> (this), problem] (const juce::FileChooser& chooser)
        {
            if (safe == nullptr || chooser.getResult() == juce::File() || ! safe->deviceStudio->isOpen())
                return;

            const auto target = chooser.getResult().withFileExtension (Devices::fileExtension);

            if (const auto result = safe->deviceStudio->getDevice().write (target); result.failed())
                problem (result.getErrorMessage());
        });
        return;
    }
    else if (action == "update")
    {
        // A place in the song to the newer version in My Devices.
        const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
        const auto fxId = static_cast<int> (event.getProperty ("fxId", 0));
        const auto* track = engine.findTrack (trackId);
        const auto* slot = track != nullptr ? track->midiFx.find (fxId) : nullptr;
        const auto* fx = slot != nullptr ? dynamic_cast<const Devices::DeviceFx*> (slot->device.get()) : nullptr;
        const auto* entry = fx != nullptr && fx->getBuild() != nullptr ? deviceStudio->findInLibrary (fx->getBuild()->deviceId) : nullptr;

        if (entry == nullptr)
            return problem ("There's no newer version of it in My Devices.");

        juce::String error;
        const auto newer = DeviceStudio::buildOfFile (entry->file, error);

        if (newer == nullptr)
            return problem (error);

        engine.replaceMidiFx ([fx, &newer] (const MidiFx::Device& old) -> std::unique_ptr<MidiFx::Device>
        {
            if (&old != fx)
                return nullptr;

            auto fresh = std::make_unique<Devices::DeviceFx> (newer, Devices::currentSampleRate());

            for (size_t i = 0; i < fresh->spec.params.size(); ++i)
                for (size_t j = 0; j < old.spec.params.size(); ++j)
                    if (old.spec.params[j].id == fresh->spec.params[i].id)
                        fresh->set (static_cast<int> (i), old.get (static_cast<int> (j)));

            fresh->maps = copyOf (fx->maps);
            return fresh;
        });

        markChanged();
        pushTracks();
    }

    pushDeviceStudio();
    pushDeviceAi();

    // A device opened: the Plugins Creator Studio's Build with AI shows it (the page brings the
    // studio to the front).
    if ((action == "new" || action == "open" || action == "openSlot") && creator != nullptr)
        creator->showMidiDevice();
}

void MainComponent::handleDeviceMap (const juce::var& event)
{
    // An output's Map button: what it moves (MIDI Link's Target), or nothing.
    const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
    const auto fxId = static_cast<int> (event.getProperty ("fxId", 0));
    const auto output = event.getProperty ("output", {}).toString().substring (0, 64);
    auto* track = engine.findTrack (trackId);
    auto* slot = track != nullptr ? track->midiFx.find (fxId) : nullptr;
    auto* fx = slot != nullptr ? dynamic_cast<Devices::DeviceFx*> (slot->device.get()) : nullptr;

    if (fx == nullptr || output.isEmpty())
        return;

    if (! fx->maps.isObject())
        fx->maps = object();

    const auto target = MidiLink::Target::fromVar (event.getProperty ("target", {}));

    if (target.isValid())
        fx->maps.getDynamicObject()->setProperty (output, target.toVar());
    else
        fx->maps.getDynamicObject()->removeProperty (output);

    markChanged();
    pushTracks();
}

void MainComponent::handleDeviceRegister (const juce::var& event)
{
    // A protected MIDI FX in its demo: its Registration window. Registered, the page's window
    // and lists lose their demo marks.
    const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
    const auto fxId = static_cast<int> (event.getProperty ("fxId", 0));
    auto* track = engine.findTrack (trackId);
    auto* slot = track != nullptr ? track->midiFx.find (fxId) : nullptr;
    auto* fx = slot != nullptr ? dynamic_cast<Devices::DeviceFx*> (slot->device.get()) : nullptr;

    if (fx == nullptr || fx->getBuild() == nullptr || ! fx->getBuild()->isProtected())
        return;

    Licences::showRegistration (fx->getBuild()->soni, [safe = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safe != nullptr)
            safe->pushTracks();
    }, true);
}

//==============================================================================
void MainComponent::updateDevices()
{
    if (deviceStudio == nullptr)
        return;

    Devices::setSampleRate (engine.getPlaybackRate());

    // What devices moved: each control only when its value changed since the device last moved it.
    // While a mixdown renders (faster than time goes by), what devices move is left alone: it
    // would land at the wrong moments, and the transport isn't theirs to move then.
    std::vector<Devices::Move> moves;
    Devices::takeMoves (moves);

    if (engine.isMixdownRunning())
        moves.clear();

    for (const auto& move : moves)
    {
        MidiLink::Target target;

        if (move.kind == sonify::abi::controlTarget)
        {
            target.key = juce::String::fromUTF8 (move.target).substring (0, 64);
        }
        else
        {
            const auto* fx = Devices::findFx (move.instance);

            if (fx == nullptr || fx->getBuild() == nullptr || ! juce::isPositiveAndBelow (move.output, static_cast<int> (fx->getBuild()->outputs.size())))
                continue;

            const auto mapped = fx->maps.getProperty (fx->getBuild()->outputs[static_cast<size_t> (move.output)].id, {});

            if (! mapped.isObject())
                continue;

            target = MidiLink::Target::fromVar (mapped);
        }

        // A device names only what MIDI Link has (no plugin by name: that's what outputs are for).
        if (! target.isValid() || (move.kind == sonify::abi::controlTarget && target.key == "plugin"))
            continue;

        const auto key = std::make_pair (move.instance, target.id());
        const auto found = deviceMoveValues.find (key);

        if (target.isContinuous())
        {
            if (found != deviceMoveValues.end() && std::abs (found->second - move.value) < 1.0e-4f)
                continue;

            deviceMoveValues[key] = move.value;
            linkMove (target, move.value);
        }
        else
        {
            const auto on = move.value >= 0.5f;

            if (found != deviceMoveValues.end() && (found->second >= 0.5f) == on)
                continue;

            deviceMoveValues[key] = move.value;
            linkSet (target, on);
        }
    }

    // Now and then, what devices that are gone last moved is forgotten.
    if (++deviceTicks % 300 == 0)
        for (auto it = deviceMoveValues.begin(); it != deviceMoveValues.end();)
            it = Devices::findFx (it->first.first) == nullptr ? deviceMoveValues.erase (it) : std::next (it);

    if (deviceStudioDue)
    {
        deviceStudioDue = false;
        pushDeviceStudio();
    }

    // The Device Studio's view of the open device playing: about ten times a second while it shows.
    if (deviceStudioShown && deviceStudio->isOpen() && deviceTicks % 6 == 0 && webView != nullptr)
    {
        const auto live = deviceStudio->takeLiveStatus();
        const auto text = juce::JSON::toString (live, true);

        if (text != deviceLivePushed || live.getProperty ("log", {}).toString().isNotEmpty())
        {
            deviceLivePushed = text;
            webView->emitEventIfBrowserIsVisible ("deviceLive", live);
        }
    }
}

void MainComponent::pushDeviceStudio()
{
    // The Plugins Creator Studio shows the open device (a MIDI FX) in Build with AI.
    if (creator != nullptr && deviceStudio != nullptr)
        creator->midiDeviceChanged();

    if (webView == nullptr || deviceStudio == nullptr)
        return;

    auto state = deviceStudio->getState();
    auto* fields = state.getDynamicObject();
    fields->setProperty ("aiBusy", deviceAgent != nullptr && deviceAgent->isBusy());
    fields->setProperty ("tryTrack", deviceTryTrack);
    webView->emitEventIfBrowserIsVisible ("deviceStudio", state);
}

void MainComponent::pushDeviceAi()
{
    // ...with its Sonify AI's conversation.
    if (creator != nullptr && deviceAgent != nullptr)
        creator->midiDeviceChanged();

    if (webView == nullptr || deviceAgent == nullptr)
        return;

    auto state = deviceAgent->getState();
    addAiAccount (state);
    webView->emitEventIfBrowserIsVisible ("deviceAiState", state);
}
