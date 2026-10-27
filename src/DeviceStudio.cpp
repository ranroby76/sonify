// D:\workspace\Fanan_web_hub\src\DeviceStudio.cpp

#include "DeviceStudio.h"
#include "Skills.h"
#include "SonifyAiData.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    juce::var object()
    {
        return juce::var (new juce::DynamicObject());
    }

    juce::var failure (const juce::String& message)
    {
        auto result = object();
        result.getDynamicObject()->setProperty ("error", message);
        return result;
    }

    double rounded (double value, double step = 1.0e-4)
    {
        return std::round (value / step) * step;
    }

    juce::String withoutTitleLine (juce::String text)
    {
        // The first line names the file's place in the project, for people: the AI doesn't need it.
        if (text.startsWith ("<!--"))
            return text.fromFirstOccurrenceOf ("-->", false, false).trimStart();

        if (text.startsWith ("//"))
            return text.fromFirstOccurrenceOf ("\n", false, false).trimStart();

        return text;
    }

    juce::String unitName (MidiFx::Unit unit)
    {
        static const char* names[] = { "number", "percent", "semitones", "steps", "pitch", "rate", "choice", "toggle", "ms", "bars", "decimal" };
        const auto index = static_cast<size_t> (unit);
        return index < std::size (names) ? names[index] : "number";
    }

    /** A device's settings and outputs as the AI reads them. */
    juce::var paramsForAi (const Devices::Build& build)
    {
        juce::Array<juce::var> params;

        for (const auto& p : build.spec.params)
        {
            auto item = object();
            auto* f = item.getDynamicObject();
            f->setProperty ("id", p.id);
            f->setProperty ("name", p.name);
            f->setProperty ("unit", unitName (p.unit));
            f->setProperty ("min", p.min);
            f->setProperty ("max", p.max);
            f->setProperty ("default", p.defaultValue);

            if (p.unit == MidiFx::Unit::choice && ! p.choices.isEmpty())
                f->setProperty ("choices", p.choices.joinIntoString ("|"));

            params.add (item);
        }

        return params;
    }

    juce::var outputsForAi (const Devices::Build& build)
    {
        juce::Array<juce::var> outputs;

        for (const auto& o : build.outputs)
        {
            auto item = object();
            item.getDynamicObject()->setProperty ("id", o.id);
            item.getDynamicObject()->setProperty ("name", o.name);
            outputs.add (item);
        }

        return outputs;
    }

    /** "A minor", "F# major", "Bb dorian": the root and the scale; false when it isn't one. */
    bool parseKey (const juce::String& text, int& root, juce::String& scale)
    {
        static const juce::StringArray sharps { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const juce::StringArray flats { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        const auto trimmed = text.trim();
        const auto name = trimmed.upToFirstOccurrenceOf (" ", false, false);
        auto index = sharps.indexOf (name, true);

        if (index < 0)
            index = flats.indexOf (name, true);

        if (index < 0)
            return false;

        root = index;
        scale = trimmed.fromFirstOccurrenceOf (" ", false, false).trim().toLowerCase();

        if (scale.isEmpty())
            scale = "major";

        return TempoMap::getScales().contains (scale);
    }

    /** Where the device's places are: every DeviceFx of a device in the song's MIDI FX. */
    template <typename Visit>
    void forEachPlace (AudioEngine& engine, const juce::String& deviceId, Visit&& visit)
    {
        for (const auto& track : engine.getTracks())
        {
            for (int i = 0; i < track->midiFx.size(); ++i)
            {
                auto* slot = track->midiFx.at (i);
                auto* fx = dynamic_cast<Devices::DeviceFx*> (slot->device.get());

                if (fx != nullptr && fx->getBuild() != nullptr && fx->getBuild()->deviceId == deviceId)
                    visit (*track, *slot, *fx);
            }
        }
    }
}

//==============================================================================
DeviceStudio::DeviceStudio (AudioEngine& e, Hooks h)
    : engine (e), hooks (std::move (h))
{
}

DeviceStudio::~DeviceStudio()
{
    *alive = false;
    buildCancelled->store (true);
    testCancelled->store (true);
    jobs.removeAllJobs (true, 30000);   // a build stops its compiler; a test stops at its next block
}

void DeviceStudio::cancel()
{
    // Sonify AI was stopped: the build or test under way stops too (and answers, "Stopped.").
    buildCancelled->store (true);
    testCancelled->store (true);
}

void DeviceStudio::changed()
{
    if (hooks.stateChanged)
        hooks.stateChanged();
}

void DeviceStudio::resetBuildState()
{
    lastBuildOk = true;
    lastBuildText.clear();
    lastBuildLog.clear();
    playsOn.clear();
}

juce::Result DeviceStudio::save()
{
    if (! opened)
        return juce::Result::ok();

    if (file == juce::File())
        file = Devices::freeFileFor (device.name);

    const auto result = device.write (file);
    libraryRead = false;
    return result;
}

//==============================================================================
void DeviceStudio::newDevice()
{
    close();
    device = Devices::DeviceFile::make ("New Device");
    file = juce::File();
    opened = true;
    changed();
}

juce::Result DeviceStudio::open (const juce::File& source)
{
    juce::File inLibrary = source;

    if (source.getParentDirectory() != Devices::devicesFolder() || ! source.hasFileExtension (Devices::fileExtension))
        if (const auto result = import (source, inLibrary); result.failed())
            return result;

    Devices::DeviceFile loaded;

    if (const auto result = loaded.read (inLibrary); result.failed())
        return result;

    // Someone else's protected MIDI FX: it plays in songs, but its maker's work isn't opened for editing.
    if (loaded.isLocked())
        return juce::Result::fail ("\"" + loaded.name + "\" is protected by its maker: it plays in your songs, but it can't be opened for editing.");

    close();
    device = std::move (loaded);
    file = inLibrary;
    opened = true;

    // Its last build plays as it is: ready for the page and for tests.
    if (! device.wasm.isEmpty())
    {
        juce::String error;
        build = Devices::makeBuild (device.wasm, device.id, device.name, device.description, device.version, error);

        if (build != nullptr)
            Devices::remember (build);
        else
            lastBuildText = "Its last build can't play here: " + error;
    }

    changed();
    return juce::Result::ok();
}

juce::Result DeviceStudio::openBuild (const std::shared_ptr<const Devices::Build>& source)
{
    if (source == nullptr)
        return juce::Result::fail ("That device isn't here.");

    if (const auto* entry = findInLibrary (source->deviceId))
        return open (entry->file);

    // A device from someone's song: a file of its own, its build without its code.
    auto made = Devices::DeviceFile::make (source->name);
    made.id = source->deviceId.isNotEmpty() ? source->deviceId : made.id;
    made.description = source->description;
    made.version = source->version;
    made.wasm = source->wasm;
    made.licence = source->licence;   // a protected MIDI FX keeps its license.soni
    const auto target = Devices::freeFileFor (made.name);

    if (const auto result = made.write (target); result.failed())
        return result;

    libraryRead = false;
    return open (target);
}

void DeviceStudio::close()
{
    if (building)
        buildCancelled->store (true);

    if (testing)
        testCancelled->store (true);

    // A build or test still running for the device closing finds another one open, and is dropped.
    buildCancelled = std::make_shared<std::atomic<bool>> (false);
    testCancelled = std::make_shared<std::atomic<bool>> (false);
    opened = false;
    building = false;
    testing = false;
    device = {};
    file = juce::File();
    build.reset();
    resetBuildState();
    changed();
}

juce::Result DeviceStudio::setInfo (const juce::String& name, const juce::String& description)
{
    if (! opened)
        return juce::Result::fail ("No device is open.");

    const auto newName = name.trim().substring (0, 60);

    if (newName.isNotEmpty())
        device.name = newName;

    if (description.isNotEmpty())
        device.description = description.trim().substring (0, 600);

    // The file follows the name (when that name is free).
    if (file.existsAsFile() && file.getFileNameWithoutExtension() != juce::File::createLegalFileName (device.name))
    {
        const auto target = Devices::freeFileFor (device.name, file);

        if (target != file && file.moveFileTo (target))
            file = target;
    }

    const auto result = save();

    // The song's places show the new name: the same build, named again.
    if (build != nullptr && (build->name != device.name || build->description != device.description))
    {
        juce::String error;

        if (auto renamed = Devices::makeBuild (build->wasm, device.id, device.name, device.description, build->version, error))
        {
            Devices::remember (renamed);
            build = renamed;

            // Only where it is already: a new name doesn't put it on a track.
            if (hooks.placeBuild)
                playsOn = hooks.placeBuild (build, false);
        }
    }

    changed();
    return result;
}

void DeviceStudio::setVendor (const juce::String& vendor)
{
    if (! opened)
        return;

    device.vendor = vendor.trim().substring (0, 60);

    if (file.existsAsFile())
        save();

    changed();
}

void DeviceStudio::keepConversation (const juce::var& conversation)
{
    if (! opened)
        return;

    device.chat = conversation;

    // Only once there's something of the device to keep (a conversation alone makes the file too).
    save();
    changed();
}

//==============================================================================
void DeviceStudio::buildDevice (std::function<void (const juce::var&)> done)
{
    juce::String why;

    if (! opened)
        return done (failure ("No device is open."));

    if (building)
        return done (failure ("It's building already."));

    if (std::none_of (device.sources.begin(), device.sources.end(), [] (const auto& s) { return s.first.endsWith (".cpp"); }))
        return done (failure ("The device has no code yet: write device.cpp first."));

    if (! Devices::Compiler::isAvailable (&why) || ! Devices::isAvailable (&why))
        return done (failure (why));

    building = true;
    lastBuildText = juce::String::fromUTF8 ("Building\xe2\x80\xa6");
    changed();

    const auto sources = device.sources;
    const auto deviceId = device.id;
    const auto fingerprint = device.fingerprint();
    auto token = alive;
    buildCancelled = std::make_shared<std::atomic<bool>> (false);   // its own: a stop stops this one
    auto cancel = buildCancelled;

    jobs.addJob ([this, sources, deviceId, fingerprint, token, cancel, done]
    {
        auto result = Devices::Compiler::compile (sources, cancel.get());

        juce::MessageManager::callAsync ([this, result = std::move (result), deviceId, fingerprint, token, cancel, done]
        {
            if (! *token)
                return;

            // The device closed (or another opened) while it built: closing cleared building (and
            // another build may be under way now, which this one leaves alone).
            if (cancel != buildCancelled)
                return done (failure ("The device was closed while it built."));

            building = false;

            if (! opened || device.id != deviceId)
            {
                changed();
                return done (failure ("The device was closed while it built."));
            }

            if (cancel->load())
            {
                lastBuildText = "Stopped.";
                changed();
                return done (failure ("Stopped."));
            }

            if (! result.ok)
            {
                lastBuildOk = false;
                lastBuildLog = result.log;
                lastBuildText = "It didn't build.";
                changed();
                return done (failure ("It didn't build. The compiler says:\n" + result.log));
            }

            juce::String error;
            auto made = Devices::makeBuild (result.wasm, device.id, device.name, device.description, device.version + 1, error);

            if (made == nullptr)
            {
                lastBuildOk = false;
                lastBuildLog = error;
                lastBuildText = "It built, but it doesn't run.";
                changed();
                return done (failure ("It built, but it doesn't run: " + error));
            }

            Devices::remember (made);
            device.version = made->version;
            device.wasm = result.wasm;
            device.builtFrom = fingerprint;
            build = made;
            save();

            playsOn = hooks.placeBuild ? hooks.placeBuild (made, true) : juce::String();
            lastBuildOk = true;
            lastBuildLog = result.log;
            lastBuildText = "Version " + juce::String (made->version) + " built in " + juce::String (result.seconds, 1) + " s.";
            changed();

            auto reply = object();
            auto* f = reply.getDynamicObject();
            f->setProperty ("ok", true);
            f->setProperty ("version", made->version);
            f->setProperty ("seconds", rounded (result.seconds, 0.1));
            f->setProperty ("params", paramsForAi (*made));
            f->setProperty ("outputs", outputsForAi (*made));

            if (result.log.isNotEmpty())
                f->setProperty ("warnings", result.log.substring (0, 3000));

            f->setProperty ("playsOn", playsOn.isNotEmpty() ? playsOn
                                       : juce::String ("nowhere yet: the song has no instrument track to try it on. Tell the user to add one (or select one): it plays there once they do."));
            done (reply);
        });
    });
}

//==============================================================================
const std::vector<DeviceStudio::Entry>& DeviceStudio::getLibrary (bool refresh)
{
    if (libraryRead && ! refresh)
        return library;

    library.clear();

    // What each file says it is, without reading its code and build.
    for (const auto& f : Devices::listMyDevices())
    {
        Devices::DeviceFile::Info info;

        if (Devices::DeviceFile::readInfo (f, info).failed())
            continue;

        library.push_back ({ f, info.id, info.name, info.description, info.version, info.built });
    }

    libraryRead = true;
    return library;
}

const DeviceStudio::Entry* DeviceStudio::findInLibrary (const juce::String& deviceId)
{
    for (const auto& entry : getLibrary())
        if (entry.id == deviceId)
            return &entry;

    return nullptr;
}

std::shared_ptr<Devices::Build> DeviceStudio::buildOfFile (const juce::File& source, juce::String& error)
{
    Devices::DeviceFile d;

    if (const auto result = d.read (source); result.failed())
    {
        error = result.getErrorMessage();
        return {};
    }

    if (d.wasm.isEmpty())
    {
        error = "\"" + d.name + "\" isn't built yet: open it in the Plugins Creator Studio.";
        return {};
    }

    auto made = Devices::makeBuild (d.wasm, d.id, d.name, d.description, d.version, error, d.licence);

    if (made != nullptr)
        Devices::remember (made);

    return made;
}

juce::Result DeviceStudio::trash (juce::File target)
{
    // (A copy: closing the open device clears the file it may have been given.)
    if (opened && target == file)
        close();

    Devices::DeviceFile::Info info;
    const auto knownId = Devices::DeviceFile::readInfo (target, info).wasOk() ? info.id : juce::String();

    if (target.existsAsFile() && ! target.moveToTrash() && ! target.deleteFile())
        return juce::Result::fail ("Couldn't remove " + target.getFileName() + ".");

    libraryRead = false;

    // The code the user brought for it goes too, unless another copy of the device is still here.
    if (knownId.isNotEmpty() && findInLibrary (knownId) == nullptr)
        referenceFolderOf (knownId).deleteRecursively();

    changed();
    return juce::Result::ok();
}

//==============================================================================
juce::File DeviceStudio::referenceFolderOf (const juce::String& deviceId)
{
    return Devices::myDevicesFolder().getChildFile ("Reference code").getChildFile (juce::File::createLegalFileName (deviceId));
}

CodeReference DeviceStudio::getReference() const
{
    // A device without an id has none (it isn't open): a folder of nothing.
    return CodeReference (referenceFolderOf (device.id.isNotEmpty() ? device.id : juce::String ("none")));
}

CodeReference::Added DeviceStudio::addReference (const juce::Array<juce::File>& sources)
{
    if (! opened || device.id.isEmpty())
    {
        CodeReference::Added nothing;
        nothing.problems.add ("no MIDI FX is open for the code");
        return nothing;
    }

    return getReference().add (sources);
}

juce::Result DeviceStudio::import (const juce::File& source, juce::File& copied)
{
    Devices::DeviceFile d;

    if (const auto result = d.read (source); result.failed())
        return result;

    // An older copy of the same device is replaced; the same one isn't copied again; and one that
    // isn't newer than My Devices' (a copy saved earlier) comes in as a device of its own, so
    // nothing newer is lost. Anything else gets a file of its own.
    const auto* existing = findInLibrary (d.id);
    copied = Devices::freeFileFor (d.name);

    if (existing != nullptr)
    {
        Devices::DeviceFile::Info info;
        const auto known = Devices::DeviceFile::readInfo (existing->file, info).wasOk();

        if (existing->file == source || (known && info.version == d.version && info.builtFrom == d.builtFrom))
        {
            copied = existing->file;
            return juce::Result::ok();
        }

        if (known && d.version > info.version)
        {
            copied = existing->file;
        }
        else
        {
            d.id = juce::Uuid().toDashedString();
            d.name = (d.name + " (older copy)").substring (0, 60);
            copied = Devices::freeFileFor (d.name);
        }
    }

    if (copied == source)
        return juce::Result::ok();

    if (opened && copied == file)
        close();

    const auto result = d.write (copied);
    libraryRead = false;
    changed();
    return result;
}

//==============================================================================
juce::var DeviceStudio::buildSummary() const
{
    auto summary = object();
    auto* f = summary.getDynamicObject();
    f->setProperty ("ok", lastBuildOk);
    f->setProperty ("text", lastBuildText);
    return summary;
}

juce::var DeviceStudio::getState()
{
    auto state = object();
    auto* f = state.getDynamicObject();
    juce::String why;
    const auto canRun = Devices::isAvailable (&why);
    juce::String compilerWhy;
    const auto canBuild = Devices::Compiler::isAvailable (&compilerWhy);
    f->setProperty ("available", canRun && canBuild);
    f->setProperty ("why", ! canRun ? why : compilerWhy);
    f->setProperty ("open", opened);

    juce::Array<juce::var> list;

    for (const auto& entry : getLibrary())
    {
        auto item = object();
        auto* i = item.getDynamicObject();
        i->setProperty ("file", entry.file.getFullPathName());
        i->setProperty ("id", entry.id);
        i->setProperty ("name", entry.name);
        i->setProperty ("description", entry.description);
        i->setProperty ("version", entry.version);
        i->setProperty ("built", entry.built);
        list.add (item);
    }

    f->setProperty ("library", list);
    f->setProperty ("folder", Devices::devicesFolder().getFullPathName());

    if (! opened)
        return state;

    f->setProperty ("file", file.getFullPathName());
    f->setProperty ("id", device.id);
    f->setProperty ("name", device.name);
    f->setProperty ("description", device.description);
    f->setProperty ("vendor", device.vendor);
    f->setProperty ("version", device.version);
    f->setProperty ("built", build != nullptr);
    f->setProperty ("upToDate", build != nullptr && device.isBuiltFromSources());
    f->setProperty ("hasCode", ! device.sources.empty());
    f->setProperty ("building", building);
    f->setProperty ("lastBuild", buildSummary());
    f->setProperty ("playsOn", playsOn);

    if (build != nullptr)
        f->setProperty ("device", build->describe());

    juce::Array<juce::var> files;

    for (const auto& [fileName, text] : device.sources)
    {
        auto item = object();
        item.getDynamicObject()->setProperty ("name", fileName);
        item.getDynamicObject()->setProperty ("lines", text.isEmpty() ? 0 : juce::StringArray::fromLines (text).size());
        files.add (item);
    }

    f->setProperty ("files", files);
    return state;
}

juce::var DeviceStudio::takeLiveStatus()
{
    auto status = object();
    auto* f = status.getDynamicObject();

    if (! opened)
        return status;

    juce::StringArray places, faults;
    juce::String log;
    double micros = 0.0;
    int count = 0;

    forEachPlace (engine, device.id, [&] (const Track& track, MidiFxChain::Slot&, Devices::DeviceFx& fx)
    {
        places.add (track.name);

        if (const auto text = Devices::describeFault (fx.getFault()); text.isNotEmpty())
            faults.addIfNotAlreadyThere (text);

        log << fx.takeLog();
        micros = juce::jmax (micros, fx.getAverageMicroseconds());
        ++count;
    });

    f->setProperty ("places", places.joinIntoString (", "));
    f->setProperty ("count", count);
    f->setProperty ("fault", faults.joinIntoString (" "));
    f->setProperty ("log", log.substring (0, 8000));

    // Its share of the audio time: a block is about 10 ms.
    f->setProperty ("load", rounded (micros / 10000.0, 0.001));
    return status;
}

//==============================================================================
// The AI's tools.
juce::String DeviceStudio::deviceGuide()
{
    static const juce::String text = []
    {
        auto guideText = withoutTitleLine (juce::String::fromUTF8 (SonifyAiData::sonify_device_guide_md, SonifyAiData::sonify_device_guide_mdSize));
        auto header = withoutTitleLine (juce::String::fromUTF8 (SonifyAiData::sonify_device_h, SonifyAiData::sonify_device_hSize));

        // Sonify's own parts of the header (how it talks to the device) aren't the AI's business:
        // left out, which keeps every request smaller.
        juce::StringArray kept;
        bool inside = false;

        for (const auto& line : juce::StringArray::fromLines (header))
        {
            const auto trimmed = line.trim();

            if (trimmed == "// [internal]")
            {
                inside = true;
                kept.add ("    // (Sonify's internals, left out here)");
            }
            else if (trimmed == "// [/internal]")
            {
                inside = false;
            }
            else if (! inside)
            {
                kept.add (line);
            }
        }

        auto all = guideText + "\n\n# sonify_device.h (the API)\n\n```cpp\n" + kept.joinIntoString ("\n").trim() + "\n```\n";

        if (const auto skills = Skills::index (Skills::For::devices); skills.isNotEmpty())
            all << "\n" << skills;

        return all;
    }();

    return text;
}

juce::String DeviceStudio::guide()
{
    return deviceGuide();
}

juce::var DeviceStudio::definitions()
{
    static const juce::var tools = []
    {
        auto list = juce::JSON::parse (R"JSON([
  { "name": "get_device",
    "description": "Looks at the open device: its name and description, its source files with their code, whether it's built and up to date with its code, its settings and outputs (as its panel shows them), the last build's errors, where it plays in the song, and what it logged lately. Look before changing code you haven't seen in this conversation.",
    "input_schema": { "type": "object", "properties": {} } },
  { "name": "get_song",
    "description": "Looks at the song the device plays in: its tempo, time signature and key, and its tracks, FX channels and buses, numbered as control() names them (track.<n>, fx.<n>, bus.<n>).",
    "input_schema": { "type": "object", "properties": {} } },
  { "name": "set_device_info",
    "description": "Names the device (2 to 4 words, what it does: \"Triplet Arp\") and describes it in one plain sentence for its panel. Do it when you start a new device, and when what it does changes.",
    "input_schema": { "type": "object", "properties": {
      "name": { "type": "string" }, "description": { "type": "string" } } } },
  { "name": "write_file",
    "description": "Writes a whole source file of the device, made or replaced: device.cpp, or a helper .h or .cpp. Names: letters, digits, _ and -, ending .cpp or .h; 8 files at most. Keep a file under about 250 lines, so each step stays short: split a big device into files (helpers in .h files that device.cpp includes).",
    "input_schema": { "type": "object", "properties": {
      "file": { "type": "string" }, "text": { "type": "string" } }, "required": ["file", "text"] } },
  { "name": "edit_file",
    "description": "Changes one place in a source file: find is replaced by replace. find must be there exactly once (take a few lines around it to be sure). For fixes and small changes, instead of writing the whole file again.",
    "input_schema": { "type": "object", "properties": {
      "file": { "type": "string" }, "find": { "type": "string" }, "replace": { "type": "string" } }, "required": ["file", "find", "replace"] } },
  { "name": "delete_file",
    "description": "Removes a source file from the device.",
    "input_schema": { "type": "object", "properties": { "file": { "type": "string" } }, "required": ["file"] } },
  { "name": "read_reference",
    "description": "Reads one of the user's own code files (get_device lists them under reference): up to 400 lines at a time, so a long file is read a part at a time.",
    "input_schema": { "type": "object", "properties": {
      "path": { "type": "string", "description": "As listed: reference/..." },
      "from": { "type": "integer", "description": "The first line to read (default 1)." },
      "lines": { "type": "integer", "description": "How many lines (default and most: 400)." } }, "required": ["path"] } },
  { "name": "search_reference",
    "description": "Finds a text (any case) in all of the user's own code: the files it's in the name of, and each file and line it's on.",
    "input_schema": { "type": "object", "properties": { "text": { "type": "string" } }, "required": ["text"] } },
  { "name": "build_device",
    "description": "Compiles the device (a few seconds). Built, it plays at once in the song: on the track it's tried on and every other place it already is (each keeps its settings). Returns its settings and outputs as its panel shows them, and where it plays. When it doesn't build, returns the compiler's errors: fix them (edit_file) and build again.",
    "input_schema": { "type": "object", "properties": {} } },
  { "name": "test_device",
    "description": "Runs the last build over notes faster than real time (nothing is heard, the song isn't touched) and returns what it played: its notes (start and length in beats, quarter notes, from 0), its other messages, its moves of Sonify's controls and outputs (with the beat they came at), and its log. Test what you build before you say it's done.",
    "input_schema": { "type": "object", "properties": {
      "notes": { "type": "array", "description": "The notes played into it.", "items": { "type": "object", "properties": {
        "pitch": { "type": "integer" }, "start": { "type": "number", "description": "beats from 0" },
        "length": { "type": "number", "description": "beats" }, "velocity": { "type": "integer" },
        "channel": { "type": "integer", "description": "0..15 as in the code (0 is MIDI channel 1)" } }, "required": ["pitch", "start", "length"] } },
      "controllers": { "type": "array", "description": "Controller moves played into it.", "items": { "type": "object", "properties": {
        "at": { "type": "number" }, "controller": { "type": "integer" }, "value": { "type": "integer" }, "channel": { "type": "integer" } }, "required": ["at", "controller", "value"] } },
      "beats": { "type": "number", "description": "How long to run (default: 4 beats past the last note)." },
      "playing": { "type": "boolean", "description": "true (default): as while the song plays; false: as while it's stopped (live playing)." },
      "tempo": { "type": "number", "description": "BPM (default: the song's)." },
      "signature": { "type": "string", "description": "\"3/4\" (default: the song's)." },
      "key": { "type": "string", "description": "\"A minor\", \"F# dorian\"... (default: the song's)." },
      "params": { "type": "object", "description": "Settings by id (default: their defaults).", "additionalProperties": { "type": "number" } } } } }
])JSON");

        // The skills: know-how read when a device needs it (Skills.h).
        if (auto* array = list.getArray())
            array->add (Skills::toolDefinition (Skills::For::devices));

        return list;
    }();

    return tools;
}

bool DeviceStudio::looksOnly (const juce::String& name)
{
    return name == "get_device" || name == "get_song" || name == "test_device" || name == "read_skill"
        || name == "read_reference" || name == "search_reference";
}

juce::String DeviceStudio::describe (const juce::String& name, const juce::var& input, const juce::var& result)
{
    if (name == "write_file" || name == "edit_file" || name == "delete_file")
        return "Worked on the device";

    if (name == "set_device_info")
        return juce::String::fromUTF8 ("Named it \xe2\x80\x9c") + device.name + juce::String::fromUTF8 ("\xe2\x80\x9d");

    if (name == "build_device")
    {
        const auto where = result.getProperty ("playsOn", {}).toString();
        return "Built version " + result.getProperty ("version", 0).toString()
             + (playsOn.isNotEmpty() ? juce::String::fromUTF8 (" \xc2\xb7 playing on ") + where : juce::String());
    }

    if (name == "test_device")
    {
        const auto notes = static_cast<int> (result.getProperty ("noteCount", 0));
        juce::ignoreUnused (input);
        return "Tried it: " + juce::String (notes) + (notes == 1 ? " note" : " notes") + " out";
    }

    return {};
}

void DeviceStudio::run (const juce::String& name, const juce::var& input, std::function<void (const juce::var&)> done)
{
    if (name == "get_song")
        return done (getSongForAi());

    if (name == "read_skill")
    {
        juce::String text;

        if (const auto r = Skills::read (Skills::For::devices, input.getProperty ("name", {}).toString(), text); r.failed())
            return done (failure (r.getErrorMessage()));

        return done (juce::var (text));
    }

    if (! opened)
        return done (failure ("No device is open in the Plugins Creator Studio."));

    if (name == "get_device")        return done (getDeviceForAi());
    if (name == "set_device_info")   return done (setInfoForAi (input));
    if (name == "write_file")        return done (writeFile (input));
    if (name == "edit_file")         return done (editFile (input));
    if (name == "delete_file")       return done (deleteFile (input));
    if (name == "build_device")      return buildDevice (std::move (done));

    if (name == "read_reference")
    {
        juce::String text;

        if (const auto r = getReference().read (input.getProperty ("path", {}).toString(), (int) input.getProperty ("from", 1),
                                                (int) input.getProperty ("lines", 0), text); r.failed())
            return done (failure (r.getErrorMessage()));

        return done (juce::var (text));
    }

    if (name == "search_reference")
    {
        const auto reference = getReference();

        if (reference.isEmpty())
            return done (failure ("The user hasn't brought any code of their own for this device."));

        return done (juce::var (reference.search (input.getProperty ("text", {}).toString(), CodeReference::maxSearchResults)));
    }

    if (name == "test_device")       return test (input, std::move (done));

    done (failure ("There's no tool called " + name + "."));
}

juce::var DeviceStudio::getDeviceForAi()
{
    auto result = object();
    auto* f = result.getDynamicObject();
    f->setProperty ("name", device.name);
    f->setProperty ("description", device.description);
    f->setProperty ("version", device.version);
    f->setProperty ("built", build != nullptr);
    f->setProperty ("upToDate", build != nullptr && device.isBuiltFromSources());

    auto files = object();

    for (const auto& [fileName, text] : device.sources)
        files.getDynamicObject()->setProperty (fileName, text);

    f->setProperty ("files", files);

    if (device.sources.empty() && build != nullptr)
        f->setProperty ("note", "This device came without its code (from someone's song or file): it can't be changed, only written again from scratch.");

    // The user's own code, by name: read_reference reads it, a part at a time.
    if (const auto code = getReference().listing (60); code.isNotEmpty())
        f->setProperty ("reference", "The user's own code (read only; port it as the guide's \"The user's own code\" says):\n" + code);

    if (build != nullptr)
    {
        f->setProperty ("params", paramsForAi (*build));
        f->setProperty ("outputs", outputsForAi (*build));
    }

    if (! lastBuildOk)
        f->setProperty ("lastBuildErrors", lastBuildLog.substring (0, 6000));

    const auto live = takeLiveStatus();
    f->setProperty ("playsOn", live.getProperty ("places", {}));

    if (live.getProperty ("fault", {}).toString().isNotEmpty())
        f->setProperty ("fault", live.getProperty ("fault", {}));

    if (live.getProperty ("log", {}).toString().isNotEmpty())
        f->setProperty ("log", live.getProperty ("log", {}).toString().getLastCharacters (3000));

    return result;
}

juce::var DeviceStudio::getSongForAi()
{
    auto result = object();
    auto* f = result.getDynamicObject();
    const auto& map = engine.getTempoMap();
    f->setProperty ("tempo", map.getTempos().front().bpm);
    f->setProperty ("signature", juce::String (map.getSignatures().front().numerator) + "/" + juce::String (map.getSignatures().front().denominator));
    f->setProperty ("key", TempoMap::keyName (map.getKeys().front()));

    if (map.getTempos().size() > 1 || map.getSignatures().size() > 1 || map.getKeys().size() > 1)
        f->setProperty ("changes", "The song changes tempo, signature or key along the way: the block's keyAt() and bpm() follow them.");

    juce::Array<juce::var> tracks, fx, buses;
    int trackNumber = 0, fxNumber = 0, busNumber = 0;

    for (const auto& track : engine.getTracks())
    {
        auto item = object();
        auto* i = item.getDynamicObject();
        i->setProperty ("name", track->name);

        if (track->kind == Track::Kind::fx)
        {
            i->setProperty ("control", "fx." + juce::String (++fxNumber));
            fx.add (item);
            continue;
        }

        if (track->kind == Track::Kind::bus)
        {
            i->setProperty ("control", "bus." + juce::String (++busNumber));
            buses.add (item);
            continue;
        }

        i->setProperty ("control", "track." + juce::String (++trackNumber));
        i->setProperty ("kind", track->kind == Track::Kind::instrument ? "instrument" : "audio");

        if (track->instrument != nullptr)
            i->setProperty ("instrument", track->instrument->getDescriptor().name);

        if (! device.id.isEmpty())
        {
            bool here = false;
            forEachPlace (engine, device.id, [&] (const Track& t, MidiFxChain::Slot&, Devices::DeviceFx&) { here = here || &t == track.get(); });

            if (here)
                i->setProperty ("hasThisDevice", true);
        }

        tracks.add (item);
    }

    f->setProperty ("tracks", tracks);
    f->setProperty ("fxChannels", fx);
    f->setProperty ("buses", buses);
    return result;
}

juce::var DeviceStudio::setInfoForAi (const juce::var& input)
{
    const auto name = input.getProperty ("name", {}).toString();
    const auto description = input.getProperty ("description", {}).toString();

    if (name.trim().isEmpty() && description.trim().isEmpty())
        return failure ("Give a name, a description, or both.");

    if (const auto result = setInfo (name, description); result.failed())
        return failure (result.getErrorMessage());

    auto result = object();
    result.getDynamicObject()->setProperty ("ok", true);
    result.getDynamicObject()->setProperty ("name", device.name);
    return result;
}

juce::var DeviceStudio::writeFile (const juce::var& input)
{
    const auto fileName = input.getProperty ("file", {}).toString().trim();
    const auto text = input.getProperty ("text", {}).toString();

    if (! Devices::DeviceFile::isValidSourceName (fileName))
        return failure ("\"" + fileName + "\" isn't a name Sonify takes: letters, digits, _ and -, ending .cpp or .h (and not sonify_device.h).");

    if (text.length() > Devices::DeviceFile::maxSourceSize)
        return failure ("That file is too long: split it into smaller files.");

    if (device.sources.count (fileName) == 0 && static_cast<int> (device.sources.size()) >= Devices::DeviceFile::maxSources)
        return failure ("The device has 8 files already: put this in one of them, or delete one.");

    device.sources[fileName] = text;

    if (const auto result = save(); result.failed())
        return failure (result.getErrorMessage());

    changed();
    auto result = object();
    result.getDynamicObject()->setProperty ("ok", true);
    result.getDynamicObject()->setProperty ("lines", juce::StringArray::fromLines (text).size());
    return result;
}

juce::var DeviceStudio::editFile (const juce::var& input)
{
    const auto fileName = input.getProperty ("file", {}).toString().trim();
    const auto find = input.getProperty ("find", {}).toString();
    const auto replace = input.getProperty ("replace", {}).toString();
    const auto found = device.sources.find (fileName);

    if (found == device.sources.end())
        return failure ("The device has no file called " + fileName + ".");

    if (find.isEmpty())
        return failure ("find is empty.");

    auto& text = found->second;
    const auto at = text.indexOf (find);

    if (at < 0)
        return failure ("That text isn't in " + fileName + " (it must match exactly, spaces and all). Look again with get_device.");

    if (text.indexOf (at + 1, find) >= 0)
        return failure ("That text is in " + fileName + " more than once: take more lines around it so it's there once.");

    const auto changedText = text.substring (0, at) + replace + text.substring (at + find.length());

    if (changedText.length() > Devices::DeviceFile::maxSourceSize)
        return failure ("That makes the file too long: split it into smaller files.");

    text = changedText;

    if (const auto result = save(); result.failed())
        return failure (result.getErrorMessage());

    changed();
    auto result = object();
    result.getDynamicObject()->setProperty ("ok", true);
    return result;
}

juce::var DeviceStudio::deleteFile (const juce::var& input)
{
    const auto fileName = input.getProperty ("file", {}).toString().trim();

    if (device.sources.erase (fileName) == 0)
        return failure ("The device has no file called " + fileName + ".");

    if (const auto result = save(); result.failed())
        return failure (result.getErrorMessage());

    changed();
    auto result = object();
    result.getDynamicObject()->setProperty ("ok", true);
    return result;
}

void DeviceStudio::test (const juce::var& input, std::function<void (const juce::var&)> done)
{
    if (build == nullptr)
        return done (failure ("It isn't built yet: build_device first."));

    if (testing)
        return done (failure ("A test is running already."));

    testing = true;
    const auto tested = build;
    const auto upToDate = device.isBuiltFromSources();
    const auto map = engine.getTempoMap();
    auto token = alive;
    testCancelled = std::make_shared<std::atomic<bool>> (false);   // its own: the build's stays the build's
    auto cancel = testCancelled;

    jobs.addJob ([this, tested, upToDate, map, input, token, cancel, done]
    {
        auto result = runTest (tested, input, map, cancel.get());

        if (cancel->load())
            result = failure ("Stopped.");

        if (! upToDate && result.isObject())
            result.getDynamicObject()->setProperty ("note", "This was the last build: the code changed since. Build again to test the changes.");

        juce::MessageManager::callAsync ([this, result, token, cancel, done]
        {
            if (! *token)
                return;

            // (A test of a device closed since: a newer test may be running, and stays running.)
            if (cancel == testCancelled)
                testing = false;

            done (result);
        });
    });
}

//==============================================================================
juce::var DeviceStudio::runTest (const std::shared_ptr<const Devices::Build>& tested, const juce::var& input, const TempoMap& songMap,
                                 const std::atomic<bool>* stop)
{
    if (tested == nullptr)
        return failure ("It isn't built yet.");

    // The music to test in: the song's, unless the test says.
    TempoMap map;
    auto bpm = songMap.getTempos().front().bpm;
    auto signature = songMap.getSignatures().front();
    auto key = songMap.getKeys().front();

    if (const auto tempo = static_cast<double> (input.getProperty ("tempo", 0.0)); tempo > 0.0)
        bpm = juce::jlimit (TempoMap::minTempo, TempoMap::maxTempo, tempo);

    if (const auto text = input.getProperty ("signature", {}).toString(); text.containsChar ('/'))
    {
        const auto numerator = text.upToFirstOccurrenceOf ("/", false, false).getIntValue();
        const auto denominator = text.fromFirstOccurrenceOf ("/", false, false).getIntValue();

        if (numerator >= 1 && numerator <= 32 && TempoMap::isValidDenominator (denominator))
            signature = { 0, numerator, denominator };
    }

    if (const auto text = input.getProperty ("key", {}).toString(); text.isNotEmpty())
    {
        int root = 0;
        juce::String scale;

        if (! parseKey (text, root, scale))
            return failure ("\"" + text + "\" isn't a key Sonify knows (\"A minor\", \"F# dorian\"...).");

        key = { 0, root, scale };
    }

    map.setTempos ({ { 0.0, bpm, false } });
    map.setSignatures ({ { 0, signature.numerator, signature.denominator } });
    map.setKeys ({ { 0, key.root, key.scale } });

    Devices::DeviceFx fx (tested, 48000.0);
    fx.controlsLive = false;

    if (fx.getFault() != Devices::Fault::none)
        return failure ("It doesn't start: " + (fx.getStartError().isNotEmpty() ? fx.getStartError() : Devices::describeFault (fx.getFault())));

    if (const auto* params = input.getProperty ("params", {}).getDynamicObject())
    {
        for (const auto& [paramName, value] : params->getProperties())
        {
            const auto& list = fx.spec.params;
            const auto found = std::find_if (list.begin(), list.end(), [&] (const MidiFx::ParamSpec& p) { return p.id == paramName.toString(); });

            if (found == list.end())
                return failure ("It has no setting called " + paramName.toString() + ".");

            fx.set (static_cast<int> (found - list.begin()), static_cast<float> (static_cast<double> (value)));
        }
    }

    // What goes in: the notes (ends before starts at the same moment) and controller moves.
    std::vector<MidiFx::Event> events;
    double lastTime = 0.0;

    if (const auto* notes = input.getProperty ("notes", {}).getArray())
    {
        for (const auto& note : *notes)
        {
            const auto pitch = static_cast<int> (note.getProperty ("pitch", -1));
            const auto start = static_cast<double> (note.getProperty ("start", -1.0));
            const auto length = static_cast<double> (note.getProperty ("length", 0.25));
            const auto velocity = static_cast<int> (note.getProperty ("velocity", 100));
            const auto channel = juce::jlimit (0, 15, static_cast<int> (note.getProperty ("channel", 0)));

            if (pitch < 0 || pitch > 127 || ! std::isfinite (start) || start < 0.0 || start > 4096.0 || ! std::isfinite (length))
                continue;

            const auto end = start + juce::jlimit (1.0 / 128.0, 4096.0, length);
            events.push_back (MidiFx::Event::noteOn (start, channel, pitch, juce::jlimit (1, 127, velocity)));
            events.push_back (MidiFx::Event::noteOff (end, channel, pitch));
            lastTime = juce::jmax (lastTime, end);
        }
    }

    if (const auto* moves = input.getProperty ("controllers", {}).getArray())
    {
        for (const auto& move : *moves)
        {
            const auto at = static_cast<double> (move.getProperty ("at", -1.0));

            if (! std::isfinite (at) || at < 0.0 || at > 4096.0)
                continue;

            MidiFx::Event e;
            e.time = at;
            e.data[0] = static_cast<juce::uint8> (0xb0 | juce::jlimit (0, 15, static_cast<int> (move.getProperty ("channel", 0))));
            e.data[1] = static_cast<juce::uint8> (juce::jlimit (0, 127, static_cast<int> (move.getProperty ("controller", 1))));
            e.data[2] = static_cast<juce::uint8> (juce::jlimit (0, 127, static_cast<int> (move.getProperty ("value", 0))));
            e.size = 3;
            events.push_back (e);
            lastTime = juce::jmax (lastTime, at);
        }
    }

    std::stable_sort (events.begin(), events.end(), [] (const MidiFx::Event& a, const MidiFx::Event& b)
    {
        if (a.time != b.time)
            return a.time < b.time;

        return a.isNoteOff() && ! b.isNoteOff();
    });

    auto beats = static_cast<double> (input.getProperty ("beats", 0.0));

    if (! (beats > 0.0))
        beats = lastTime + 4.0;

    beats = juce::jlimit (1.0, 1024.0, beats);
    const bool playing = static_cast<bool> (input.getProperty ("playing", true));

    // Run, a quarter of a beat a block, as Apply does.
    constexpr double block = 0.25;
    constexpr double timeLimitMs = 8000.0;
    MidiFx::Time time;
    MidiFx::EventList in (4096), out (8192);
    std::vector<MidiFx::Event> played;
    juce::Array<juce::var> moves;
    juce::String log;
    size_t next = 0;
    bool slow = false;
    const auto started = juce::Time::getMillisecondCounterHiRes();

    for (double t = 0.0; t < beats; t += block)
    {
        time.buildOffline (map, t, t + block, playing);
        in.clear();
        out.clear();

        while (next < events.size() && events[next].time < t + block)
            in.add (events[next++]);

        fx.process (in, out, time);

        for (int i = 0; i < out.size(); ++i)
            if (played.size() < 20000)
                played.push_back (out[i]);

        for (const auto& move : fx.offlineMoves)
        {
            if (moves.size() >= 200)
                break;

            auto item = object();
            auto* m = item.getDynamicObject();
            m->setProperty ("at", rounded (t));

            if (move.kind == sonify::abi::controlOutput)
                m->setProperty ("output", juce::isPositiveAndBelow (move.output, static_cast<int> (tested->outputs.size()))
                                              ? tested->outputs[static_cast<size_t> (move.output)].id : juce::String (move.output));
            else
                m->setProperty ("control", juce::String (move.target));

            m->setProperty ("value", rounded (move.value, 0.001));
            moves.add (item);
        }

        fx.offlineMoves.clear();

        if (log.length() < 6000)
            log << fx.takeLog();

        if (stop != nullptr && stop->load())
            break;

        // (Stopped, it lets the notes through: the run goes on, so they end where they would.)
        if (juce::Time::getMillisecondCounterHiRes() - started > timeLimitMs)
        {
            slow = true;
            break;
        }
    }

    {
        out.clear();
        fx.flush (out, beats);

        for (int i = 0; i < out.size(); ++i)
            played.push_back (out[i]);
    }

    std::stable_sort (played.begin(), played.end(), [] (const MidiFx::Event& x, const MidiFx::Event& y) { return x.time < y.time; });

    // Starts and ends into notes; everything else as it is.
    juce::Array<juce::var> notes, other;
    std::vector<std::pair<int, int>> starts;   // (index in notes, channel * 128 + pitch) still sounding
    int noteCount = 0;

    for (const auto& e : played)
    {
        const auto at = e.channel() * 128 + e.pitch();

        if (e.isNoteOn())
        {
            ++noteCount;

            if (notes.size() >= 400)
                continue;

            auto item = object();
            auto* n = item.getDynamicObject();
            n->setProperty ("pitch", e.pitch());
            n->setProperty ("start", rounded (e.time));
            n->setProperty ("length", -1.0);
            n->setProperty ("velocity", e.velocity());

            if (e.channel() != 0)
                n->setProperty ("channel", e.channel());

            starts.push_back ({ notes.size(), at });
            notes.add (item);
        }
        else if (e.isNoteOff())
        {
            const auto found = std::find_if (starts.begin(), starts.end(), [at] (const auto& s) { return s.second == at; });

            if (found != starts.end())
            {
                auto& note = notes.getReference (found->first);
                const auto start = static_cast<double> (note.getProperty ("start", 0.0));
                note.getDynamicObject()->setProperty ("length", rounded (juce::jmax (1.0 / 128.0, e.time - start)));
                starts.erase (found);
            }
        }
        else if (other.size() < 200)
        {
            auto item = object();
            auto* o = item.getDynamicObject();
            o->setProperty ("at", rounded (e.time));
            const auto status = e.data[0] & 0xf0;
            o->setProperty ("type", status == 0xb0 ? "controller" : status == 0xe0 ? "pitchBend" : status == 0xc0 ? "programChange"
                                    : status == 0xd0 || status == 0xa0 ? "aftertouch" : "other");

            if (status == 0xb0)
            {
                o->setProperty ("controller", static_cast<int> (e.data[1]));
                o->setProperty ("value", static_cast<int> (e.data[2]));
            }
            else if (status == 0xe0)
            {
                o->setProperty ("bend", (static_cast<int> (e.data[2]) << 7 | e.data[1]) - 8192);
            }
            else
            {
                o->setProperty ("data", juce::String::toHexString (e.data, e.size));
            }

            if (e.channel() != 0)
                o->setProperty ("channel", e.channel());

            other.add (item);
        }
    }

    auto result = object();
    auto* f = result.getDynamicObject();
    f->setProperty ("noteCount", noteCount);
    f->setProperty ("notes", notes);

    if (noteCount > notes.size())
        f->setProperty ("notesNote", "Only the first 400 notes are listed.");

    if (! other.isEmpty())
        f->setProperty ("other", other);

    if (! moves.isEmpty())
        f->setProperty ("moves", moves);

    if (log.isNotEmpty())
        f->setProperty ("log", log.substring (0, 6000));

    f->setProperty ("beats", beats);
    f->setProperty ("tempo", bpm);
    f->setProperty ("key", TempoMap::keyName (key));

    if (const auto fault = Devices::describeFault (fx.getFault()); fault.isNotEmpty())
        f->setProperty ("fault", fault);

    if (slow)
        f->setProperty ("fault", "The test was stopped after 8 seconds: the device is far too slow.");

    return result;
}
