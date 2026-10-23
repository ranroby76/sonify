// D:\workspace\Fanan_web_hub\src\Preferences.cpp

#include "Preferences.h"

#include <algorithm>
#include <vector>

namespace
{
    constexpr const char* formatName = "Sonify Settings";
    constexpr int formatVersion = 1;

    juce::File chosenUserData;   // empty: the default

    int number (const juce::var& value, int fallback, int min, int max)
    {
        if (! (value.isInt() || value.isInt64() || value.isDouble()))
            return fallback;

        return juce::jlimit (min, max, static_cast<int> (value));
    }

    bool flag (const juce::var& value, bool fallback)
    {
        return value.isBool() || value.isInt() ? static_cast<bool> (value) : fallback;
    }

    juce::String text (const juce::var& value, const juce::String& fallback = {})
    {
        return value.isString() ? value.toString() : fallback;
    }
}

//==============================================================================
juce::StringArray Preferences::getKeymaps()
{
    return { "studioOne", "cubase", "logic", "proTools", "ableton", "cakewalk", "flStudio" };
}

juce::StringArray Preferences::getTimecodeRates()
{
    return { "24", "25", "29.97", "30" };
}

const std::vector<std::pair<juce::String, bool>>& Preferences::getTrackOptions()
{
    static const std::vector<std::pair<juce::String, bool>> options {
        { "soloFollowsSelection", false }, { "instrumentInputFollowsSelection", false }, { "audioInputFollowsSelection", false },
        { "locateOnEmptyClick", false }, { "cursorFollowsEdit", false }, { "automationFollowsEvents", false }, { "noOverlap", false },
        { "colorizeTracks", false }, { "showTrackNumbers", true }, { "showClipNames", true },
        { "monitorFollowsRecord", false }, { "scrollPausesAutoscroll", true }
    };
    return options;
}

bool Preferences::trackOption (const juce::String& name) const
{
    if (const auto chosen = trackOptions.find (name); chosen != trackOptions.end())
        return chosen->second;

    for (const auto& [known, fallback] : getTrackOptions())
        if (known == name)
            return fallback;

    return false;
}

juce::File Preferences::getFile()
{
    return SettingsFolder::file ("settings.json", "FananWebHub-settings.json");
}

//==============================================================================
juce::File SettingsFolder::get()
{
    static const juce::File folder = []
    {
        const auto made = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                              .getChildFile ("Fanan")
                              .getChildFile ("Sonify");
        made.createDirectory();
        return made;
    }();

    return folder;
}

juce::File SettingsFolder::file (const juce::String& name, const juce::String& oldName)
{
    const auto target = get().getChildFile (name);

    // Kept beside Sonify.exe before (a new build lost it): taken over, once.
    if (oldName.isNotEmpty() && ! target.exists())
    {
        const auto old = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile (oldName);

        if (old.existsAsFile())
        {
            get().createDirectory();
            old.copyFileTo (target);
        }
    }

    return target;
}

juce::File SettingsFolder::folder (const juce::String& name, const juce::String& oldName)
{
    const auto target = get().getChildFile (name);

    // Kept under AppData\Fanan Web Hub before: moved over, once (where it can't be, it starts afresh).
    if (oldName.isNotEmpty() && ! target.exists())
    {
        std::vector<juce::File> oldHomes { juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Fanan Web Hub") };

       #if JUCE_WINDOWS
        oldHomes.push_back (juce::File::getSpecialLocation (juce::File::windowsLocalAppData).getChildFile ("Fanan Web Hub"));
       #endif

        for (const auto& home : oldHomes)
        {
            const auto old = home.getChildFile (oldName);

            if (old.isDirectory() && ! target.exists())
                old.moveFileTo (target);

            // The old folder goes once nothing is left in it.
            if (home.isDirectory() && home.getNumberOfChildFiles (juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles) == 0)
                home.deleteRecursively();
        }
    }

    target.createDirectory();
    return target;
}

//==============================================================================
void Preferences::load()
{
    const auto parsed = juce::JSON::parse (getFile());

    if (parsed.getProperty ("format", {}).toString() == formatName)
    {
        const auto appearance = parsed.getProperty ("appearance", {});
        luminance = number (appearance.getProperty ("luminance", {}), luminance, 0, 100);
        contrast = number (appearance.getProperty ("contrast", {}), contrast, 0, 100);

        keymap = text (parsed.getProperty ("keymap", {}), keymap);
        discoverable = flag (parsed.getProperty ("discoverable", {}), discoverable);

        const auto locations = parsed.getProperty ("locations", {});
        userData = text (locations.getProperty ("userData", {}));
        browserFolder = text (locations.getProperty ("browserFolder", {}));
        clipExportFolder = text (locations.getProperty ("clipExportFolder", {}));
        nodeRackFolder = text (locations.getProperty ("nodeRackFolder", {}));
        autosave = flag (locations.getProperty ("autosave", {}), autosave);
        autosaveMinutes = number (locations.getProperty ("autosaveMinutes", {}), autosaveMinutes, minAutosaveMinutes, maxAutosaveMinutes);

        releaseInBackground = flag (parsed.getProperty ("releaseInBackground", {}), releaseInBackground);
        timecodeRate = text (parsed.getProperty ("timecodeRate", {}), timecodeRate);

        devices.clear();

        if (const auto* list = parsed.getProperty ("devices", {}).getArray())
            for (const auto& item : *list)
                devices.push_back (deviceFromVar (item));

        midiInputsOff.clear();

        if (const auto* list = parsed.getProperty ("midiInputsOff", {}).getArray())
            for (const auto& item : *list)
                if (item.isString() && item.toString().isNotEmpty())
                    midiInputsOff.addIfNotAlreadyThere (item.toString());

        const auto io = parsed.getProperty ("songIo", {});
        songIo = io.isObject() ? io : juce::var();

        const auto options = parsed.getProperty ("trackOptions", {});
        trackOptions.clear();

        for (const auto& [name, fallback] : getTrackOptions())
            trackOptions[name] = flag (options.getProperty (name, {}), fallback);
    }

    tidy();
    UserData::setFolder (userData);
}

bool Preferences::save() const
{
    juce::DynamicObject::Ptr appearance (new juce::DynamicObject());
    appearance->setProperty ("luminance", luminance);
    appearance->setProperty ("contrast", contrast);

    juce::DynamicObject::Ptr locations (new juce::DynamicObject());
    locations->setProperty ("userData", userData);
    locations->setProperty ("browserFolder", browserFolder);
    locations->setProperty ("clipExportFolder", clipExportFolder);
    locations->setProperty ("nodeRackFolder", nodeRackFolder);
    locations->setProperty ("autosave", autosave);
    locations->setProperty ("autosaveMinutes", autosaveMinutes);

    juce::Array<juce::var> list, off;

    for (const auto& device : devices)
        list.add (deviceToVar (device));

    for (const auto& port : midiInputsOff)
        off.add (port);

    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("format", formatName);
    root->setProperty ("version", formatVersion);
    root->setProperty ("appearance", juce::var (appearance.get()));
    root->setProperty ("keymap", keymap);
    root->setProperty ("discoverable", discoverable);
    root->setProperty ("locations", juce::var (locations.get()));
    root->setProperty ("releaseInBackground", releaseInBackground);
    root->setProperty ("timecodeRate", timecodeRate);
    root->setProperty ("devices", list);
    root->setProperty ("midiInputsOff", off);

    if (songIo.isObject())
        root->setProperty ("songIo", songIo);

    root->setProperty ("trackOptions", trackOptionsToVar());

    return getFile().replaceWithText (juce::JSON::toString (juce::var (root.get())));
}

void Preferences::tidy()
{
    luminance = juce::jlimit (0, 100, luminance);
    contrast = juce::jlimit (0, 100, contrast);
    autosaveMinutes = juce::jlimit (minAutosaveMinutes, maxAutosaveMinutes, autosaveMinutes);

    if (! getKeymaps().contains (keymap))
        keymap = "studioOne";

    if (! getTimecodeRates().contains (timecodeRate))
        timecodeRate = "30";

    if (userData.isNotEmpty() && ! juce::File::isAbsolutePath (userData))
        userData.clear();

    // Every device with an id and a name of its own, and one default input at most.
    std::vector<Device> kept;
    bool haveDefault = false;

    for (auto device : devices)
    {
        if (device.id.isEmpty())
            device.id = juce::Uuid().toString();

        bool duplicateId = false;

        for (const auto& other : kept)
            duplicateId = duplicateId || other.id == device.id;

        if (duplicateId)
            continue;

        device.defaultInput = device.defaultInput && ! haveDefault;
        haveDefault = haveDefault || device.defaultInput;

        // A name of its own among the ones kept so far.
        auto base = device.name.removeCharacters ("|").trim();

        if (base.isEmpty())
            base = device.model.isNotEmpty() ? device.model : juce::String ("MIDI Device");

        auto name = base;

        for (int n = 2; std::any_of (kept.begin(), kept.end(), [&name] (const Device& d) { return d.name.equalsIgnoreCase (name); }); ++n)
            name = base + " (" + juce::String (n) + ")";

        device.name = name;
        kept.push_back (device);
    }

    devices = std::move (kept);
}

//==============================================================================
juce::var Preferences::toVar() const
{
    juce::DynamicObject::Ptr appearance (new juce::DynamicObject());
    appearance->setProperty ("luminance", luminance);
    appearance->setProperty ("contrast", contrast);

    juce::Array<juce::var> list;

    for (const auto& device : devices)
        list.add (deviceToVar (device));

    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("appearance", juce::var (appearance.get()));
    root->setProperty ("keymap", keymap);
    root->setProperty ("discoverable", discoverable);
    root->setProperty ("userData", UserData::getFolder().getFullPathName());
    root->setProperty ("userDataIsDefault", userData.isEmpty());
    root->setProperty ("userDataDefault", UserData::getDefaultFolder().getFullPathName());
    root->setProperty ("autosave", autosave);
    root->setProperty ("autosaveMinutes", autosaveMinutes);
    root->setProperty ("autosaveFolder", UserData::autosave().getFullPathName());
    root->setProperty ("releaseInBackground", releaseInBackground);
    root->setProperty ("timecodeRate", timecodeRate);
    root->setProperty ("devices", list);
    root->setProperty ("trackOptions", trackOptionsToVar());
    return juce::var (root.get());
}

juce::var Preferences::trackOptionsToVar() const
{
    juce::DynamicObject::Ptr options (new juce::DynamicObject());

    for (const auto& option : getTrackOptions())
        options->setProperty (option.first, trackOption (option.first));

    return juce::var (options.get());
}

const Preferences::Device* Preferences::findDevice (const juce::String& id) const
{
    for (const auto& device : devices)
        if (device.id == id)
            return &device;

    return nullptr;
}

juce::String Preferences::uniqueDeviceName (const juce::String& wanted, const juce::String& exceptId) const
{
    auto base = wanted.removeCharacters ("|").trim();

    if (base.isEmpty())
        base = "MIDI Device";

    const auto taken = [this, &exceptId] (const juce::String& name)
    {
        return std::any_of (devices.begin(), devices.end(), [&] (const Device& d) { return d.id != exceptId && d.name.equalsIgnoreCase (name); });
    };

    auto name = base;

    for (int n = 2; taken (name); ++n)
        name = base + " (" + juce::String (n) + ")";

    return name;
}

juce::String Preferences::putDevice (Device device)
{
    if (device.id.isEmpty())
        device.id = juce::Uuid().toString();

    device.name = uniqueDeviceName (device.name.isNotEmpty() ? device.name : device.model, device.id);
    device.pitchRange = juce::jlimit (1, 96, device.pitchRange);
    device.channels &= 0xffff;

    if (device.channels == 0)
        device.channels = 0xffff;

    if (device.defaultInput)
        for (auto& other : devices)
            other.defaultInput = false;

    const auto id = device.id;

    for (auto& existing : devices)
    {
        if (existing.id == id)
        {
            existing = std::move (device);
            return id;
        }
    }

    devices.push_back (std::move (device));
    return id;
}

bool Preferences::removeDevice (const juce::String& id)
{
    const auto before = devices.size();
    devices.erase (std::remove_if (devices.begin(), devices.end(), [&id] (const Device& d) { return d.id == id; }), devices.end());
    return devices.size() != before;
}

//==============================================================================
juce::var Preferences::deviceToVar (const Device& device)
{
    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("id", device.id);
    item->setProperty ("model", device.model);
    item->setProperty ("kind", device.kind);
    item->setProperty ("protocol", device.protocol);
    item->setProperty ("manufacturer", device.manufacturer);
    item->setProperty ("name", device.name);
    item->setProperty ("input", device.input);
    item->setProperty ("output", device.output);
    item->setProperty ("channels", device.channels);
    item->setProperty ("filterAftertouch", device.filterAftertouch);
    item->setProperty ("filterProgramChange", device.filterProgramChange);
    item->setProperty ("filterPitchBend", device.filterPitchBend);
    item->setProperty ("filterControllers", device.filterControllers);
    item->setProperty ("splitChannels", device.splitChannels);
    item->setProperty ("defaultInput", device.defaultInput);
    item->setProperty ("mpe", device.mpe);
    item->setProperty ("pitchRange", device.pitchRange);
    item->setProperty ("sendClock", device.sendClock);
    item->setProperty ("clockStart", device.clockStart);
    item->setProperty ("sendTimecode", device.sendTimecode);
    return juce::var (item.get());
}

Preferences::Device Preferences::deviceFromVar (const juce::var& value)
{
    Device device;
    device.id = text (value.getProperty ("id", {}));
    device.model = text (value.getProperty ("model", {}), "New Keyboard");

    const auto kind = text (value.getProperty ("kind", {}), "keyboard");
    device.kind = juce::StringArray { "keyboard", "controller", "drums", "instrument", "surface" }.contains (kind) ? kind : juce::String ("keyboard");

    const auto protocol = text (value.getProperty ("protocol", {}), "mackie");
    device.protocol = juce::StringArray { "mackie", "extender", "hui", "launchpad", "apcmini", "apcmini2" }.contains (protocol) ? protocol : juce::String ("mackie");
    device.manufacturer = text (value.getProperty ("manufacturer", {}));
    device.name = text (value.getProperty ("name", {}));
    device.input = text (value.getProperty ("input", {}));
    device.output = text (value.getProperty ("output", {}));
    device.channels = number (value.getProperty ("channels", {}), 0xffff, 0, 0xffff);

    if (device.channels == 0)
        device.channels = 0xffff;

    device.filterAftertouch = flag (value.getProperty ("filterAftertouch", {}), false);
    device.filterProgramChange = flag (value.getProperty ("filterProgramChange", {}), false);
    device.filterPitchBend = flag (value.getProperty ("filterPitchBend", {}), false);
    device.filterControllers = flag (value.getProperty ("filterControllers", {}), false);
    device.splitChannels = flag (value.getProperty ("splitChannels", {}), false);
    device.defaultInput = flag (value.getProperty ("defaultInput", {}), false);
    device.mpe = flag (value.getProperty ("mpe", {}), false);
    device.pitchRange = number (value.getProperty ("pitchRange", {}), 48, 1, 96);
    device.sendClock = flag (value.getProperty ("sendClock", {}), false);
    device.clockStart = flag (value.getProperty ("clockStart", {}), false);
    device.sendTimecode = flag (value.getProperty ("sendTimecode", {}), false);

    // A control surface plays Sonify: no track takes it, and it isn't sent the clock.
    if (device.kind == "surface")
    {
        device.channels = 0xffff;
        device.filterAftertouch = device.filterProgramChange = device.filterPitchBend = device.filterControllers = false;
        device.splitChannels = device.defaultInput = device.mpe = false;
        device.sendClock = device.clockStart = device.sendTimecode = false;
    }

    return device;
}

//==============================================================================
juce::File UserData::getDefaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sonify");
}

juce::File UserData::getFolder()
{
    return chosenUserData != juce::File() ? chosenUserData : getDefaultFolder();
}

void UserData::setFolder (const juce::String& folder)
{
    chosenUserData = folder.isNotEmpty() && juce::File::isAbsolutePath (folder) ? juce::File (folder) : juce::File();
}

juce::File UserData::songs()        { return getFolder().getChildFile ("Songs"); }
juce::File UserData::recordings()   { return getFolder().getChildFile ("Recordings"); }
juce::File UserData::mixdown()      { return getFolder().getChildFile ("Mixdown"); }
juce::File UserData::autosave()     { return getFolder().getChildFile ("Autosave"); }
