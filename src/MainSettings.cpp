// D:\workspace\Fanan_web_hub\src\MainSettings.cpp
//
// MainComponent's side of File > Settings (the rest of MainComponent is in MainComponent.cpp):
// Sonify's own settings (Preferences), the audio device (Audio Setup), the MIDI ports, the
// external MIDI devices with the clock and timecode they're sent, and the control surfaces that
// play Sonify (MIDI Setup), network discovery (Network), the user data folder and autosave
// (Locations), and letting the audio device go while Sonify is in the background.

#include "MainComponent.h"
#include "SipFiles.h"
#include "Song.h"

#include <algorithm>

namespace
{
    bool nativeDialogShowing();   // (at the end of this file: Windows' own headers come last)

    /** As in MainComponent.cpp: a page event's handler runs just after WebView2's own callback
        has returned (device and COM calls must not run inside it). */
    template <typename Handler>
    std::function<void (const juce::var&)> afterSettingsEvent (MainComponent* owner, Handler handler)
    {
        return [safe = juce::Component::SafePointer<MainComponent> (owner), handler = std::move (handler)] (const juce::var& event)
        {
            juce::MessageManager::callAsync ([safe, handler, event]
            {
                if (safe != nullptr)
                    handler (event);
            });
        };
    }

    juce::var toVar (const juce::StringArray& strings)
    {
        juce::Array<juce::var> list;

        for (const auto& text : strings)
            list.add (text);

        return list;
    }

    template <typename Number>
    juce::var numbersToVar (const juce::Array<Number>& numbers)
    {
        juce::Array<juce::var> list;

        for (const auto value : numbers)
            list.add (value);

        return list;
    }

    juce::var bitsToVar (const juce::BigInteger& bits, int count)
    {
        juce::Array<juce::var> list;

        for (int i = 0; i < count; ++i)
            list.add (bits[i]);

        return list;
    }

    juce::uint8 blockedOf (const Preferences::Device& device)
    {
        return static_cast<juce::uint8> ((device.filterAftertouch ? Track::blockAftertouch : 0)
                                         | (device.filterProgramChange ? Track::blockProgramChange : 0)
                                         | (device.filterPitchBend ? Track::blockPitchBend : 0)
                                         | (device.filterControllers ? Track::blockControllers : 0));
    }

    /** How a MIDI port is connected, from the device Windows has behind it (the port's
        identifier): usb, bluetooth, virtual (software only: a loopMIDI cable, a loopback), or
        "" when it doesn't say. */
    juce::String linkOf (const juce::MidiDeviceInfo& info)
    {
        const auto id = info.identifier.toLowerCase();

        if (id.contains ("usb#") || id.contains ("usb\\"))
            return "usb";

        if (id.contains ("bthle") || id.contains ("bthenum") || id.contains ("bluetooth"))
            return "bluetooth";

        if (id.contains ("root#") || id.contains ("loopback") || id.contains ("midiu_diag") || id.contains ("midiu_loop")
            || id.contains ("midiu_virt") || id.contains ("midiu_app"))
            return "virtual";

        return {};
    }

    juce::var transportEvent (const juce::String& property, const juce::var& value)
    {
        juce::DynamicObject::Ptr event (new juce::DynamicObject());
        event->setProperty (property, value);
        return juce::var (event.get());
    }
}

//==============================================================================
juce::WebBrowserComponent::Options MainComponent::withSettingsEvents (juce::WebBrowserComponent::Options options)
{
    return options
        .withEventListener ("getSettings", afterSettingsEvent (this, [this] (const juce::var&) { pushSettings(); }))
        .withEventListener ("setPreference", afterSettingsEvent (this, [this] (const juce::var& event) { setPreference (event); }))
        .withEventListener ("chooseUserDataFolder", afterSettingsEvent (this, [this] (const juce::var&) { chooseUserDataFolder(); }))
        .withEventListener ("resetUserDataFolder", afterSettingsEvent (this, [this] (const juce::var&) { setUserDataFolder ({}); }))
        .withEventListener ("audioSetupGet", afterSettingsEvent (this, [this] (const juce::var& event)
        {
            // Devices plugged in since are found when the page asks afresh.
            if (static_cast<bool> (event.getProperty ("rescan", false)))
                if (auto* type = engine.getDeviceManager().getCurrentDeviceTypeObject())
                    type->scanForDevices();

            pushAudioSetup();
        }))
        .withEventListener ("audioSetupSet", afterSettingsEvent (this, [this] (const juce::var& event) { changeAudioSetup (event); }))
        .withEventListener ("audioControlPanel", afterSettingsEvent (this, [this] (const juce::var&)
        {
            // Not while a mixdown renders or a take records: the device carries them (and it's
            // opened again after the driver's window).
            if (engine.isMixdownRunning() || engine.isRecording() || engine.isTakeActive())
            {
                pushAudioSetup ("Not while recording or exporting a mixdown.");
                return;
            }

            auto& manager = engine.getDeviceManager();

            // The driver's own window (ASIO): what it changes takes effect when the device opens again.
            // (The device watch keeps out of it meanwhile: the driver may pause while it shows.)
            if (auto* device = manager.getCurrentAudioDevice(); device != nullptr && device->hasControlPanel())
            {
                const juce::ScopedValueSetter<bool> busy (deviceSettingUp, true);
                deviceReopens = 0;
                deviceHeard = false;

                if (device->showControlPanel())
                {
                    manager.closeAudioDevice();
                    manager.restartLastAudioDevice();
                }
            }

            pushAudioSetup();
            pushTracks();
        }))
        .withEventListener ("audioTestTone", afterSettingsEvent (this, [this] (const juce::var&)
        {
            // On Main's channels only (every output channel is open: JUCE's own test would beep on all).
            engine.playTestTone();
        }))
        .withEventListener ("midiPortEnabled", afterSettingsEvent (this, [this] (const juce::var& event)
        {
            setMidiPortEnabled (event.getProperty ("id", {}).toString(), static_cast<bool> (event.getProperty ("enabled", false)));
        }))
        .withEventListener ("midiDeviceSave", afterSettingsEvent (this, [this] (const juce::var& event) { saveMidiDevice (event); }))
        .withEventListener ("midiDeviceRemove", afterSettingsEvent (this, [this] (const juce::var& event)
        {
            removeMidiDevice (event.getProperty ("id", {}).toString());
        }))
        .withEventListener ("replaceLibraryFolder", afterSettingsEvent (this, [this] (const juce::var& event)
        {
            replaceLibraryFolder (event.getProperty ("folder", {}).toString());
        }))
        .withEventListener ("addDefaultLibraryFolders", afterSettingsEvent (this, [this] (const juce::var&)
        {
            if (library.addDefaultFolders() > 0)
                startLibraryScan (false);
            else
                pushLibrary ("The usual plugin folders are all listed already (or aren't on this computer).");
        }));
}

//==============================================================================
void MainComponent::loadPreferences()
{
    prefs.load();
    applySongDefaults();
    followMidiPorts();   // listens to the MIDI inputs, and applies the external devices
    applyDiscovery();
}

void MainComponent::applySongDefaults()
{
    // Before any Make Default: Main plays where Settings had the audio going (when that wasn't
    // the first pair), now that every output channel is open.
    if (! prefs.songIo.isObject())
    {
        const auto pair = engine.getStartupOutputPair();

        if (pair.first > 0)
        {
            auto outputs = AudioEngine::defaultOutputs();
            outputs.front().left = pair.first;
            outputs.front().right = pair.second;

            juce::DynamicObject::Ptr io (new juce::DynamicObject());
            io->setProperty ("inputs", Song::inputsToVar (AudioEngine::defaultInputs()));
            io->setProperty ("outputs", Song::outputsToVar (outputs));
            io->setProperty ("audition", 0);
            prefs.songIo = juce::var (io.get());
            savePreferences();
        }
    }

    if (! prefs.songIo.isObject())
        return;

    const auto inputs = prefs.songIo.getProperty ("inputs", {});
    const auto outputs = prefs.songIo.getProperty ("outputs", {});

    engine.setSongDefaults (inputs.isArray() ? Song::inputsFromVar (inputs) : AudioEngine::defaultInputs(),
                            outputs.isArray() ? Song::outputsFromVar (outputs) : AudioEngine::defaultOutputs(),
                            static_cast<int> (prefs.songIo.getProperty ("audition", 0)));

    // The song Sonify starts with is a new one.
    engine.setInputs (engine.getSongDefaultInputs());
    engine.setOutputs (engine.getSongDefaultOutputs());
    engine.setAuditionOutput (engine.getSongDefaultAudition());
}

void MainComponent::setSongDefaults (const juce::var& io)
{
    juce::DynamicObject::Ptr kept (new juce::DynamicObject());
    kept->setProperty ("inputs", Song::inputsToVar (Song::inputsFromVar (io.getProperty ("inputs", {}))));
    kept->setProperty ("outputs", Song::outputsToVar (Song::outputsFromVar (io.getProperty ("outputs", {}))));
    kept->setProperty ("audition", static_cast<int> (io.getProperty ("audition", 0)));
    prefs.songIo = juce::var (kept.get());
    savePreferences();

    engine.setSongDefaults (Song::inputsFromVar (prefs.songIo.getProperty ("inputs", {})),
                            Song::outputsFromVar (prefs.songIo.getProperty ("outputs", {})),
                            static_cast<int> (prefs.songIo.getProperty ("audition", 0)));
}

void MainComponent::savePreferences()
{
    prefs.save();
}

void MainComponent::pushSettings()
{
    if (webView == nullptr)
        return;

    auto state = prefs.toVar();
    auto* fields = state.getDynamicObject();

    // The MIDI ports devices can use: inputs (with whether Sonify listens) and outputs.
    juce::Array<juce::var> outputs;

    for (const auto& info : juce::MidiOutput::getAvailableDevices())
    {
        juce::DynamicObject::Ptr port (new juce::DynamicObject());
        port->setProperty ("id", info.identifier);
        port->setProperty ("name", info.name);
        outputs.add (juce::var (port.get()));
    }

    fields->setProperty ("midiInputs", midiInputsToVar());
    fields->setProperty ("midiOutputs", outputs);
    fields->setProperty ("syncProblems", toVar (engine.getSyncProblems()));
    fields->setProperty ("surfaceProblems", toVar (surfaceProblems()));

    juce::StringArray defaults;

    for (const auto& folder : Library::getDefaultFolders())
        defaults.add (folder.getFullPathName());

    fields->setProperty ("defaultPluginFolders", toVar (defaults));
    fields->setProperty ("computerName", juce::SystemStats::getComputerName());
    fields->setProperty ("discoveryPort", discoveryPort);

    webView->emitEventIfBrowserIsVisible ("settings", state);
}

void MainComponent::setPreference (const juce::var& event)
{
    const auto number = [&event] (const char* key, int fallback) { return static_cast<int> (event.getProperty (key, fallback)); };

    if (event.hasProperty ("luminance"))   prefs.luminance = juce::jlimit (0, 100, number ("luminance", prefs.luminance));
    if (event.hasProperty ("contrast"))    prefs.contrast = juce::jlimit (0, 100, number ("contrast", prefs.contrast));

    if (event.hasProperty ("accent"))
        if (const auto accent = event.getProperty ("accent", {}).toString(); Preferences::getAccents().contains (accent))
            prefs.accent = accent;

    if (event.hasProperty ("keymap"))
        if (const auto keymap = event.getProperty ("keymap", {}).toString(); Preferences::getKeymaps().contains (keymap))
            prefs.keymap = keymap;

    if (event.hasProperty ("discoverable"))
    {
        prefs.discoverable = static_cast<bool> (event.getProperty ("discoverable", false));
        applyDiscovery();
    }

    if (event.hasProperty ("autosave"))
        prefs.autosave = static_cast<bool> (event.getProperty ("autosave", true));

    if (event.hasProperty ("autosaveMinutes"))
        prefs.autosaveMinutes = juce::jlimit (Preferences::minAutosaveMinutes, Preferences::maxAutosaveMinutes,
                                              number ("autosaveMinutes", prefs.autosaveMinutes));

    if (event.hasProperty ("releaseInBackground"))
    {
        prefs.releaseInBackground = static_cast<bool> (event.getProperty ("releaseInBackground", false));

        if (! prefs.releaseInBackground)
            engine.releaseDevice (false);
    }

    if (event.hasProperty ("timecodeRate"))
    {
        if (const auto rate = event.getProperty ("timecodeRate", {}).toString(); Preferences::getTimecodeRates().contains (rate))
            prefs.timecodeRate = rate;

        applyMidiDevices();
    }

    // The track list's Options menu: only the names it knows, each on or off.
    if (const auto options = event.getProperty ("trackOptions", {}); options.isObject())
        for (const auto& option : Preferences::getTrackOptions())
            if (const auto value = options.getProperty (option.first, {}); value.isBool())
                prefs.trackOptions[option.first] = static_cast<bool> (value);

    savePreferences();
    pushSettings();
}

//==============================================================================
void MainComponent::applyMidiDevices()
{
    std::vector<AudioEngine::MidiDevice> devices;
    std::vector<MidiSync::Target> targets;
    std::vector<ControlSurfaces::Config> surfaceList;
    std::vector<PadGrids::Config> padList;
    juce::StringArray surfacePorts;

    for (const auto& device : prefs.devices)
    {
        // A pad grid (Launchpad, APC mini) plays Song Blocks' Live: no track hears its port either.
        if (device.kind == "surface" && PadGrids::isPadProtocol (device.protocol))
        {
            padList.push_back ({ device.name, PadGrids::modelFromName (device.protocol), device.input, device.output });

            if (device.input.isNotEmpty())
                surfacePorts.addIfNotAlreadyThere (device.input);

            continue;
        }

        // A control surface plays Sonify itself, in the device list's order: no track hears its port.
        if (device.kind == "surface")
        {
            surfaceList.push_back ({ device.name, ControlSurfaces::protocolFromName (device.protocol), device.input, device.output });

            if (device.input.isNotEmpty())
                surfacePorts.addIfNotAlreadyThere (device.input);

            continue;
        }

        AudioEngine::MidiDevice engineDevice;
        engineDevice.name = device.name;
        engineDevice.input = device.input;
        engineDevice.channels = static_cast<juce::uint16> (device.channels & 0xffff);
        engineDevice.blocked = blockedOf (device);
        engineDevice.split = device.splitChannels;
        engineDevice.defaultInput = device.defaultInput;
        engineDevice.mpe = device.mpe;
        engineDevice.pitchRange = device.pitchRange;
        devices.push_back (engineDevice);

        if (device.output.isNotEmpty() && (device.sendClock || device.sendTimecode))
            targets.push_back ({ device.output, device.sendClock, device.clockStart, device.sendTimecode });
    }

    engine.setSurfacePorts (surfacePorts);
    engine.setMidiDevices (std::move (devices));
    engine.setSyncTargets (targets, MidiSync::rateFromName (prefs.timecodeRate));
    if (surfaces != nullptr)
        surfaces->configure (surfaceList);

    if (padGrids != nullptr)
        padGrids->configure (padList);

    applyMidiLinkDevices();
}

void MainComponent::saveMidiDevice (const juce::var& event)
{
    auto device = Preferences::deviceFromVar (event.getProperty ("device", {}));
    juce::String oldName;

    if (const auto* before = prefs.findDevice (device.id))
        oldName = before->name;

    const auto id = prefs.putDevice (std::move (device));
    const auto* saved = prefs.findDevice (id);

    if (saved != nullptr)
    {
        // Renamed: the tracks that chose it keep it (the song changes with them).
        if (oldName.isNotEmpty() && saved->name != oldName)
        {
            engine.renameMidiSource (oldName, saved->name);
            markChanged();
        }

        // Sonify listens to the port it receives from (even one turned off before).
        auto& manager = engine.getDeviceManager();
        prefs.midiInputsOff.removeString (saved->input);

        for (const auto& info : juce::MidiInput::getAvailableDevices())
            if (info.name == saved->input && ! manager.isMidiInputDeviceEnabled (info.identifier))
                manager.setMidiInputDeviceEnabled (info.identifier, true);
    }

    savePreferences();
    applyMidiDevices();
    pushSettings();
    pushTracks();
}

void MainComponent::removeMidiDevice (const juce::String& id)
{
    if (! prefs.removeDevice (id))
        return;

    savePreferences();
    applyMidiDevices();
    pushSettings();
    pushTracks();
}

//==============================================================================
void MainComponent::applyDiscovery()
{
    // Sonify announces itself on the local network every second and a half while this is on.
    if (prefs.discoverable && advertiser == nullptr)
        advertiser = std::make_unique<juce::NetworkServiceDiscovery::Advertiser> ("SonifyRemote", juce::SystemStats::getComputerName(),
                                                                                   discoveryPort, remotePort);
    else if (! prefs.discoverable)
        advertiser.reset();
}

//==============================================================================
void MainComponent::chooseUserDataFolder()
{
    if (choosingFile)
        return;

    choosingFile = true;
    fileChooser = std::make_unique<juce::FileChooser> ("Where your songs, recordings, mixdowns and autosaves go", UserData::getFolder());

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        const auto folder = chooser.getResult();

        if (folder.isDirectory())
            safeThis->setUserDataFolder (folder.getFullPathName());
    });
}

void MainComponent::setUserDataFolder (const juce::String& folder)
{
    // The default folder chosen by hand is the default.
    prefs.userData = folder == UserData::getDefaultFolder().getFullPathName() ? juce::String() : folder;
    UserData::setFolder (prefs.userData);
    savePreferences();
    pushSettings();
    pushSong();   // new songs go there now

    // My Devices is in the user data folder too, and so are the plugins made in the Plugins
    // Creator Studio.
    Devices::setMyDevicesFolder (UserData::getFolder().getChildFile ("Devices"));
    Sip::migrate();   // a user data folder from an older Sonify: its plugins into .sip files
    library.refreshMadePlugins();
    pushLibrary();

    if (deviceStudio != nullptr)
    {
        deviceStudio->getLibrary (true);
        pushDeviceStudio();
    }
}

//==============================================================================
void MainComponent::pushAudioSetup (const juce::String& error)
{
    if (webView == nullptr)
        return;

    auto& manager = engine.getDeviceManager();
    juce::DynamicObject::Ptr state (new juce::DynamicObject());

    juce::StringArray types;

    for (auto* type : manager.getAvailableDeviceTypes())
        types.add (type->getTypeName());

    state->setProperty ("types", toVar (types));
    state->setProperty ("type", manager.getCurrentAudioDeviceType());

    const auto setup = manager.getAudioDeviceSetup();

    if (auto* type = manager.getCurrentDeviceTypeObject())
    {
        // ASIO drivers are one device for both directions; Windows Audio has inputs and outputs apart.
        state->setProperty ("separate", type->hasSeparateInputsAndOutputs());
        state->setProperty ("outputs", toVar (type->getDeviceNames (false)));
        state->setProperty ("inputs", toVar (type->getDeviceNames (true)));
    }

    state->setProperty ("output", setup.outputDeviceName);
    state->setProperty ("input", setup.inputDeviceName);

    if (auto* device = manager.getCurrentAudioDevice())
    {
        const auto rate = device->getCurrentSampleRate();
        const auto inputNames = device->getInputChannelNames();
        const auto outputNames = device->getOutputChannelNames();

        state->setProperty ("open", true);
        state->setProperty ("deviceName", device->getName());
        state->setProperty ("rates", numbersToVar (device->getAvailableSampleRates()));
        state->setProperty ("rate", rate);
        state->setProperty ("buffers", numbersToVar (device->getAvailableBufferSizes()));
        state->setProperty ("buffer", device->getCurrentBufferSizeSamples());
        state->setProperty ("inputChannels", toVar (inputNames));
        state->setProperty ("activeInputs", bitsToVar (device->getActiveInputChannels(), inputNames.size()));
        state->setProperty ("outputChannels", toVar (outputNames));
        state->setProperty ("activeOutputs", bitsToVar (device->getActiveOutputChannels(), outputNames.size()));
        state->setProperty ("inputLatencyMs", rate > 0.0 ? 1000.0 * device->getInputLatencyInSamples() / rate : 0.0);
        state->setProperty ("outputLatencyMs", rate > 0.0 ? 1000.0 * device->getOutputLatencyInSamples() / rate : 0.0);
        state->setProperty ("hasControlPanel", device->hasControlPanel());
    }
    else
    {
        state->setProperty ("open", false);
    }

    state->setProperty ("released", engine.isDeviceReleased());
    state->setProperty ("releaseInBackground", prefs.releaseInBackground);
    state->setProperty ("error", error);
    webView->emitEventIfBrowserIsVisible ("audioSetup", juce::var (state.get()));
}

//==============================================================================
juce::var MainComponent::midiInputsToVar()
{
    auto& manager = engine.getDeviceManager();
    juce::Array<juce::var> list;

    for (const auto& info : juce::MidiInput::getAvailableDevices())
    {
        juce::DynamicObject::Ptr port (new juce::DynamicObject());
        port->setProperty ("id", info.identifier);
        port->setProperty ("name", info.name);
        port->setProperty ("enabled", manager.isMidiInputDeviceEnabled (info.identifier));
        port->setProperty ("busy", midiPortsBusy.contains (info.name));
        port->setProperty ("link", linkOf (info));   // the page says what kind of device it is
        list.add (juce::var (port.get()));
    }

    return list;
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The audio device manager: a device or a MIDI port came or went, or something was set.
    followMidiPorts();
}

void MainComponent::followMidiPorts()
{
    auto& manager = engine.getDeviceManager();
    const auto inputs = juce::MidiInput::getAvailableDevices();
    const auto outputs = juce::MidiOutput::getAvailableDevices();

    juce::String names;

    for (const auto& port : inputs)
        names << port.name << "\n";

    names << "\n";

    for (const auto& port : outputs)
        names << port.name << "\n";

    const auto portsChanged = names != midiPortNames;
    midiPortNames = names;

    // Ports come or go: the ones that couldn't be opened are tried again.
    if (portsChanged)
        midiPortsBusy.clear();

    // Sonify listens to every MIDI input but the ones turned off by hand, so tracks can pick it:
    // a keyboard plugged in, or a loopMIDI port made, is in their MIDI lists at once.
    for (const auto& port : inputs)
    {
        const auto listening = manager.isMidiInputDeviceEnabled (port.identifier);

        // Off by hand stays off (JUCE opens the ones it opened at startup again when ports change).
        if (prefs.midiInputsOff.contains (port.name))
        {
            if (listening)
                manager.setMidiInputDeviceEnabled (port.identifier, false);

            continue;
        }

        if (listening || midiPortsBusy.contains (port.name))
            continue;

        manager.setMidiInputDeviceEnabled (port.identifier, true);

        // Not opened: another app may hold it. Tried again when the ports change.
        if (! manager.isMidiInputDeviceEnabled (port.identifier))
            midiPortsBusy.addIfNotAlreadyThere (port.name);
    }

    // A port that came may be one a device sends MIDI clock or timecode to, or a control surface's.
    if (portsChanged)
        applyMidiDevices();

    // The page hears of ports coming and going, and turned on or off: MIDI Setup lists them, and
    // so do the tracks' MIDI lists (with the ones Sonify isn't listening to).
    auto told = names;

    for (const auto& port : inputs)
        told << (manager.isMidiInputDeviceEnabled (port.identifier) ? "+" : "-") << (midiPortsBusy.contains (port.name) ? "!" : "");

    if (told != midiPortsTold)
    {
        midiPortsTold = told;
        pushSettings();
    }
}

void MainComponent::setMidiPortEnabled (const juce::String& identifier, bool enabled)
{
    juce::String name;

    for (const auto& port : juce::MidiInput::getAvailableDevices())
        if (port.identifier == identifier)
            name = port.name;

    if (name.isEmpty())
        return;

    // Turned off by hand, it stays off, after Sonify starts again too. Turned on, it's opened now.
    if (enabled)
        prefs.midiInputsOff.removeString (name);
    else
        prefs.midiInputsOff.addIfNotAlreadyThere (name);

    savePreferences();

    auto& manager = engine.getDeviceManager();
    midiPortsBusy.removeString (name);
    manager.setMidiInputDeviceEnabled (identifier, enabled);

    if (enabled && ! manager.isMidiInputDeviceEnabled (identifier))
        midiPortsBusy.add (name);

    midiPortsTold.clear();   // the page hears how it went
    followMidiPorts();
}

void MainComponent::changeAudioSetup (const juce::var& event)
{
    // Not while a mixdown renders or a take records: the device carries them.
    if (engine.isMixdownRunning() || engine.isRecording() || engine.isTakeActive())
    {
        pushAudioSetup ("Not while recording or exporting a mixdown.");
        return;
    }

    auto& manager = engine.getDeviceManager();
    juce::String error;
    const juce::ScopedValueSetter<bool> busy (deviceSettingUp, true);   // the device watch keeps out of it
    deviceReopens = 0;                  // the user's choice now: nothing of the watch's to bring back
    deviceHeard = false;

    // Taken back first if it was let go: changing it means using it.
    engine.releaseDevice (false);

    if (event.hasProperty ("type"))
    {
        const auto type = event.getProperty ("type", {}).toString();

        if (type != manager.getCurrentAudioDeviceType())
            manager.setCurrentAudioDeviceType (type, true);   // its default device opens

        if (manager.getCurrentAudioDevice() == nullptr)
            error = "No " + type + " device could be opened.";
    }
    else
    {
        auto setup = manager.getAudioDeviceSetup();

        if (event.hasProperty ("device"))
        {
            // One device both ways (ASIO).
            setup.outputDeviceName = setup.inputDeviceName = event.getProperty ("device", {}).toString();
            setup.useDefaultInputChannels = setup.useDefaultOutputChannels = true;
        }

        if (event.hasProperty ("output"))
        {
            setup.outputDeviceName = event.getProperty ("output", {}).toString();
            setup.useDefaultOutputChannels = true;
        }

        if (event.hasProperty ("input"))
        {
            setup.inputDeviceName = event.getProperty ("input", {}).toString();
            setup.useDefaultInputChannels = true;
        }

        if (event.hasProperty ("rate"))
            setup.sampleRate = static_cast<double> (event.getProperty ("rate", 0.0));

        if (event.hasProperty ("buffer"))
            setup.bufferSize = static_cast<int> (event.getProperty ("buffer", 0));

        if (const auto* list = event.getProperty ("inputChannels", {}).getArray())
        {
            setup.inputChannels.clear();

            for (int i = 0; i < list->size(); ++i)
                if (static_cast<bool> ((*list)[i]))
                    setup.inputChannels.setBit (i);

            setup.useDefaultInputChannels = false;
        }

        // Outputs: every channel stays open (the engine opens them all); Song Setup's Outputs
        // say what plays where.
        error = manager.setAudioDeviceSetup (setup, true);
    }

    // The song's inputs follow the device's active input channels.
    pushTracks();
    pushAudioSetup (error);
    pushStatus();
}

//==============================================================================
void MainComponent::replaceLibraryFolder (const juce::String& folder)
{
    if (choosingFile || ! library.getFolders().contains (folder))
        return;

    choosingFile = true;
    fileChooser = std::make_unique<juce::FileChooser> ("A Plugin Folder instead of " + folder,
                                                       juce::File (folder).isDirectory() ? juce::File (folder) : juce::File::getSpecialLocation (juce::File::userHomeDirectory));

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safeThis = juce::Component::SafePointer<MainComponent> (this), folder] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        const auto chosen = chooser.getResult();

        if (! chosen.isDirectory() || chosen.getFullPathName() == folder)
            return;

        safeThis->lastPluginFolder = chosen;
        safeThis->library.replaceFolder (folder, chosen);
        safeThis->startLibraryScan (false);
        safeThis->pushLibrary();
    });
}

//==============================================================================
void MainComponent::followForeground()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (now - lastForegroundCheck < 400.0)
        return;

    lastForegroundCheck = now;

    // Every output channel open (a device change during a take or a mixdown waited till now).
    engine.openAllOutputChannels();

    // Another app in front: the audio device is let go for it, unless Sonify is busy with it
    // (playing, recording, exporting, a live show going on: its decks, the night's recording, the
    // stream). Back in front, Sonify takes it again.
    const auto foreground = juce::Process::isForegroundProcess();
    const auto transport = engine.getTransport().getState();
    const auto busy = transport.playing || transport.recording || engine.isMixdownRunning() || engine.isTakeActive()
                      || engine.isSampling()                    // the Sampling Studio, real time or faster
                      || (engine.isLiveShowOn() && liveShowPlaying());

    if (prefs.releaseInBackground && ! foreground && ! busy && ! engine.isDeviceReleased())
    {
        engine.releaseDevice (true);
        pushAudioSetup();
    }
    else if (engine.isDeviceReleased() && (foreground || ! prefs.releaseInBackground))
    {
        engine.releaseDevice (false);
        pushAudioSetup();
    }
}

void MainComponent::watchAudioDevice()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (now - deviceWatchMs < 250.0 || deviceReopening)
        return;

    deviceWatchMs = now;
    auto& manager = engine.getDeviceManager();
    auto* device = manager.getCurrentAudioDevice();
    const auto blocks = engine.getDeviceCallbacks();
    const auto say = [this] (const juce::String& text)
    {
        if (liveShown)
            pushLive (text);
        else
            pushTracks (text);
    };

    // Being brought back (it stopped by itself, or vanished: a cable pulled for a moment): tried
    // again even while no device opens.
    const bool bringingBack = deviceReopens < 4 && (deviceReopens > 0 || (device == nullptr && deviceHeard));

    // None chosen, let go of on purpose (Sonify in the background), being set up (Settings, the
    // driver's own window, a driver's message box: the code that opens it is still under way), or
    // carrying an export or a take: nothing to watch.
    if ((device == nullptr && ! bringingBack) || engine.isDeviceReleased() || deviceSettingUp || nativeDialogShowing()
        || engine.isMixdownRunning() || engine.isTakeActive() || engine.getTransport().getState().recording)
    {
        deviceStalledMs = 0.0;
        lastDeviceCallbacks = blocks;
        return;
    }

    if (device != nullptr)
    {
        // Another device, or the same set up anew: watched afresh.
        const auto setup = manager.getAudioDeviceSetup();
        const auto watched = device->getTypeName() + "|" + setup.outputDeviceName + "|" + setup.inputDeviceName + "|"
                             + juce::String (setup.sampleRate) + "|" + juce::String (setup.bufferSize);

        if (watched != deviceWatched)
        {
            deviceWatched = watched;
            deviceHeard = deviceReopens > 0;   // opened again by the watch: still the one it watched
        }

        // Asking for blocks: it runs (its setup kept, to open it again with: JUCE forgets the
        // device's name once an open fails).
        if (blocks != lastDeviceCallbacks)
        {
            if (deviceReopens > 0)
                say ("The audio device stopped by itself: Sonify started it again.");

            lastDeviceCallbacks = blocks;
            deviceStalledMs = 0.0;
            deviceReopens = 0;
            deviceHeard = true;
            deviceRunningSetup = setup;
            return;
        }

        // One that never ran isn't pushed (Settings say why it didn't start).
        if (! deviceHeard)
            return;

        // Still in the last block it asked for (a plugin taking its time over it): the device is
        // fine (closing it would only wait for that block, and freeze Sonify meanwhile).
        if (engine.isInDeviceCallback())
        {
            deviceStalledMs = 0.0;
            return;
        }
    }

    if (deviceStalledMs <= 0.0)
    {
        deviceStalledMs = now;
        return;
    }

    // Silent for longer than any driver's own reset takes: started again, at most four times, further
    // apart each time (3.5, 5, 10, 20 seconds), so a device that's gone isn't pushed for ever.
    const auto wait = deviceReopens == 0 ? 3500.0 : 2500.0 * static_cast<double> (1 << juce::jmin (deviceReopens, 3));

    if (deviceReopens >= 4 || now - deviceStalledMs < wait)
        return;

    ++deviceReopens;
    bool running = false;

    {
        // (A driver's message box meanwhile runs the message loop: the watch keeps out.)
        const juce::ScopedValueSetter<bool> busy (deviceReopening, true);
        running = engine.reopenDevice (deviceRunningSetup);
    }

    deviceStalledMs = juce::Time::getMillisecondCounterHiRes();
    lastDeviceCallbacks = engine.getDeviceCallbacks();
    pushAudioSetup();

    if (! running)
        say (deviceReopens < 4 ? juce::String ("The audio device stopped by itself: Sonify is starting it again...")
                               : juce::String ("The audio device stopped and won't start again: check it's plugged in and on, then choose it in Settings > Audio."));
}

void MainComponent::autosaveIfDue()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    // The time counts from the first change since the song was saved (or last autosaved).
    if (! prefs.autosave || noProject || ! songChanged)
    {
        lastAutosave = now;
        return;
    }

    if (now - lastAutosave < prefs.autosaveMinutes * 60000.0)
        return;

    // Not in the middle of a take or a mixdown: the next tick, once it's done.
    if (engine.isMixdownRunning() || engine.isRecording() || engine.isTakeActive())
        return;

    lastAutosave = now;

    // A copy in the user data folder's Autosave, never over the song itself: open it like any song.
    const auto folder = UserData::autosave();

    if (! folder.createDirectory())
        return;

    const auto file = folder.getChildFile (juce::File::createLegalFileName (songTitle()) + " (Autosave)" + Song::fileExtension);
    file.replaceWithText (juce::JSON::toString (withFilesFolder (Song::capture (engine, songTitle(), songView, file))));
}

void MainComponent::undoToggle()
{
    // FL Studio's Ctrl+Z: undo, and right after an undo, redo it.
    undoOrRedo (! (lastHistoryWasUndo && undoManager.canRedo()));
}

//==============================================================================
// Control surfaces (Mackie Control, its Extender, HUI): what they see of Sonify and change in it.
// Mixing (faders, pan, mutes, solos) is never an undo step, as on the page.
juce::StringArray MainComponent::surfaceProblems() const
{
    auto problems = surfaces != nullptr ? surfaces->getProblems() : juce::StringArray();

    // The port a surface sends from must be listened to.
    for (const auto& device : prefs.devices)
    {
        if (device.kind != "surface" || device.input.isEmpty())
            continue;

        if (prefs.midiInputsOff.contains (device.input))
            problems.add (device.name + ": its port " + device.input + " is turned off under MIDI Inputs.");
        else if (midiPortsBusy.contains (device.input))
            problems.add (device.name + ": couldn't open " + device.input + " (another app may be using it).");
    }

    // A pad grid's lights need its Send To port (its pads still launch without).
    for (const auto& device : prefs.devices)
        if (device.kind == "surface" && PadGrids::isPadProtocol (device.protocol) && (device.input.isEmpty() || device.output.isEmpty()))
            problems.add (device.name + ": choose both its ports (Receive From for its pads, Send To for its lights).");

    return problems;
}

std::vector<ControlSurfaces::Channel> MainComponent::getSurfaceChannels (bool withSends)
{
    // The Console's order: tracks, then FX channels.
    std::vector<ControlSurfaces::Channel> list, fxChannels;
    const auto selected = engine.getSelectedTrackId();

    for (const auto& track : engine.getTracks())
    {
        ControlSurfaces::Channel channel;
        channel.id = track->id;
        channel.kind = track->isMixChannel()               ? ControlSurfaces::Channel::Kind::fx
                     : track->kind == Track::Kind::audio   ? ControlSurfaces::Channel::Kind::audio
                                                           : ControlSurfaces::Channel::Kind::instrument;
        channel.name = track->name;
        channel.volumeDb = track->volumeDb.load();
        channel.pan = track->pan.load();
        channel.mute = track->mute.load();
        channel.solo = track->solo.load();
        channel.recordArm = track->recordArm.load();
        channel.selected = track->id == selected && ! track->isMixChannel();
        channel.automationRead = Automation::reads (track->automationMode.load());

        // Its sends to FX channels, as the page lists them (not one whose FX channel is gone).
        if (withSends)
            for (const auto& send : track->sends)
                if (const auto* fx = send.sidechain ? nullptr : engine.findTrack (send.target); fx != nullptr && fx->kind == Track::Kind::fx)
                    channel.sends.push_back ({ send.id, send.levelDb, send.on });

        (track->isMixChannel() ? fxChannels : list).push_back (std::move (channel));
    }

    list.insert (list.end(), std::make_move_iterator (fxChannels.begin()), std::make_move_iterator (fxChannels.end()));
    return list;
}

float MainComponent::getMainVolume()                   { return engine.getMasterVolume(); }
Transport::State MainComponent::getSurfaceTransport()  { return engine.getTransport().getState(); }
double MainComponent::getSurfaceSongEnd()              { return engine.getSongEnd(); }
const TempoMap& MainComponent::getSurfaceTempoMap()    { return engine.getTempoMap(); }

double MainComponent::getSurfaceFrameRate()
{
    return prefs.timecodeRate == "29.97" ? 29.97 : juce::jmax (1.0, prefs.timecodeRate.getDoubleValue());
}

void MainComponent::setChannelVolume (int channelId, float db)
{
    if (channelId == ControlSurfaces::mainChannel)
        engine.setMasterVolume (db);
    else if (auto* track = engine.findTrack (channelId))
    {
        track->volumeDb.store (juce::jlimit (Track::silentDb, Track::maxDb, db));
        automationTouched (channelId, { AutomationTarget::Kind::volume, 0, 0 });
    }
    else
        return;

    markChanged();
}

void MainComponent::setChannelPan (int channelId, float pan)
{
    if (auto* track = engine.findTrack (channelId))
    {
        track->pan.store (juce::jlimit (-1.0f, 1.0f, pan));
        automationTouched (channelId, { AutomationTarget::Kind::pan, 0, 0 });
        markChanged();
    }
}

void MainComponent::setChannelMute (int channelId, bool mute)
{
    if (auto* track = engine.findTrack (channelId))
    {
        track->mute.store (mute);
        markChanged();
        surfaceTracksDue = true;
    }
}

void MainComponent::setChannelSolo (int channelId, bool solo)
{
    if (auto* track = engine.findTrack (channelId))
    {
        track->solo.store (solo);
        markChanged();
        surfaceTracksDue = true;
    }
}

void MainComponent::setChannelRecordArm (int channelId, bool arm)
{
    armTrack (channelId, arm);
    markChanged();
    surfaceTracksDue = true;
}

void MainComponent::selectChannel (int channelId)
{
    engine.selectTrack (channelId);

    if (inputFollowsSelection (channelId))
        pushTracks();

    surfaceTracksDue = true;
}

void MainComponent::setChannelSendLevel (int channelId, int sendId, float db)
{
    surfaceSendEdit (channelId);
    engine.setSendLevel (channelId, sendId, db);
    markChanged();
    surfaceTracksDue = true;
}

void MainComponent::setChannelSendOn (int channelId, int sendId, bool on)
{
    surfaceSendEdit (channelId);
    engine.setSendOn (channelId, sendId, on);
    markChanged();
    surfaceTracksDue = true;
}

void MainComponent::surfaceSendEdit (int trackId)
{
    // Another track's send: the turn before is a step of its own.
    if (surfaceSendEditing && surfaceSendTrack != trackId)
        commitSurfaceSendEdit();

    if (! surfaceSendEditing)
    {
        surfaceSendEditing = true;
        surfaceSendTrack = trackId;
        surfaceSendsBefore = engine.getSends (trackId);
    }

    surfaceSendChangedMs = juce::Time::getMillisecondCounterHiRes();
}

void MainComponent::commitSurfaceSendEdit()
{
    if (! std::exchange (surfaceSendEditing, false))
        return;

    auto before = std::move (surfaceSendsBefore);
    auto after = engine.getSends (surfaceSendTrack);
    surfaceSendsBefore.clear();

    const auto same = before.size() == after.size()
                   && std::equal (before.begin(), before.end(), after.begin(), [] (const Track::Send& a, const Track::Send& b)
                                  { return a.id == b.id && a.levelDb == b.levelDb && a.on == b.on; });

    if (! same)
        addToHistory ("Change Send", new Edits::SendsChanged (engine, surfaceSendTrack, std::move (before), std::move (after)));
}

void MainComponent::setChannelAutomationRead (int channelId, bool read)
{
    engine.setAutomationMode (channelId, read ? AutomationMode::read : AutomationMode::off);
    markChanged();
    surfaceTracksDue = true;
}

void MainComponent::surfaceTransport (const juce::String& action)
{
    handleTransport (transportEvent ("action", action));
}

void MainComponent::surfaceLocate (double ppq)
{
    // A mixdown owns the song while it renders.
    if (! engine.isMixdownRunning())
        engine.getTransport().locate (juce::jlimit (0.0, Transport::maxPosition, ppq));
}

void MainComponent::surfaceLoop (bool on)
{
    handleTransport (transportEvent ("loop", on));
}

void MainComponent::surfaceMetronome (bool on)
{
    handleTransport (transportEvent ("metronome", on));
}

void MainComponent::surfaceUndo (bool undo)
{
    undoOrRedo (undo);
}

void MainComponent::surfaceSave()
{
    // Not on the Start page, nor while a dialog is open (a Save As would take its place).
    if (noProject || juce::ModalComponentManager::getInstance()->getNumModalComponents() > 0)
        return;

    saveSong ({});
}

void MainComponent::surfaceCommand (const juce::String& pageCommand)
{
    if (webView != nullptr)
        webView->emitEventIfBrowserIsVisible ("runCommand", transportEvent ("command", pageCommand));
}

//==============================================================================
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

namespace
{
   #if JUCE_WINDOWS
    /** One of the message thread's windows: a dialog of Windows' own ("#32770": a message box),
        showing? Then it's found, and the looking stops. */
    BOOL CALLBACK findDialog (HWND window, LPARAM found)
    {
        wchar_t name[16] {};

        if (IsWindowVisible (window) && GetClassNameW (window, name, 16) > 0 && lstrcmpW (name, L"#32770") == 0)
        {
            *reinterpret_cast<bool*> (found) = true;
            return FALSE;
        }

        return TRUE;
    }
   #endif

    /** A dialog of Windows' own (a driver's message box) showing on the message thread: its loop
        runs Sonify's timers while the code that showed it (a device being opened) is still under
        way, so nothing may close the device meanwhile. */
    bool nativeDialogShowing()
    {
       #if JUCE_WINDOWS
        bool found = false;
        EnumThreadWindows (GetCurrentThreadId(), findDialog, reinterpret_cast<LPARAM> (&found));
        return found;
       #else
        return false;
       #endif
    }
}
