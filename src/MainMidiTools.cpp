// D:\workspace\Fanan_web_hub\src\MainMidiTools.cpp
//
// MainComponent: MIDI FX and MIDI tools.
//
//   MIDI FX     the devices in front of an instrument track's instrument (MidiFx.h): added,
//               removed and moved (each one undo step), switched on and off, set
//   Apply       a MIDI tool on notes (a clip's, or the selected ones): the built-in devices run
//               here (MidiFx::runOffline); Magenta's models run on the page (hub-miditools.js),
//               which sends the notes back as an ordinary note edit (one undo step)
//   Magenta     the live device's windows go to the page as they end; its answers come back
//   Catalog     Magenta tools installed into Documents\Sonify\Fanan Content\MIDI Tools from where
//               their definitions say (Cloud::installMidiTool), served to the page from there;
//               magenta.js itself is built into Sonify (MagentaJs.cmake)

#include "MainComponent.h"

#if __has_include ("SonifyMagentaData.h")
 #include "SonifyMagentaData.h"
 #define SONIFY_MAGENTA 1
#else
 #define SONIFY_MAGENTA 0
#endif

namespace
{
    juce::var object()      { return juce::var (new juce::DynamicObject()); }

    const char* unitName (MidiFx::Unit unit)
    {
        switch (unit)
        {
            case MidiFx::Unit::percent:    return "percent";
            case MidiFx::Unit::semitones:  return "semitones";
            case MidiFx::Unit::steps:      return "steps";
            case MidiFx::Unit::pitch:      return "pitch";
            case MidiFx::Unit::rate:       return "rate";
            case MidiFx::Unit::choice:     return "choice";
            case MidiFx::Unit::toggle:     return "toggle";
            case MidiFx::Unit::ms:         return "ms";
            case MidiFx::Unit::bars:       return "bars";
            case MidiFx::Unit::decimal:    return "decimal";
            case MidiFx::Unit::track:      return "track";
            case MidiFx::Unit::number:
            default:                       return "number";
        }
    }

    bool isToolId (const juce::String& id)
    {
        return id.isNotEmpty() && id.length() <= 64 && id.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789._-") && ! id.startsWith (".");
    }

    bool isPlainName (const juce::String& name)
    {
        return name.isNotEmpty() && name.length() <= 120
            && name.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") && ! name.startsWith (".");
    }

    double number (const juce::var& value, double fallback)
    {
        const auto result = value.isDouble() || value.isInt() || value.isInt64() || value.isBool() ? static_cast<double> (value) : fallback;
        return std::isfinite (result) ? result : fallback;
    }

    juce::var notesToVar (const std::vector<MidiFx::Note>& notes)
    {
        juce::Array<juce::var> list;
        list.ensureStorageAllocated (static_cast<int> (notes.size()));

        for (const auto& note : notes)
            list.add (juce::Array<juce::var> { note.pitch, note.start, note.length, note.velocity });

        return list;
    }
}

//==============================================================================
juce::File MainComponent::midiToolsFolder()
{
    return contentFolder().getChildFile ("MIDI Tools");
}

void MainComponent::emitMidiFxTypes()
{
    if (webView == nullptr)
        return;

    juce::Array<juce::var> list;

    for (const auto& spec : MidiFx::types())
    {
        juce::Array<juce::var> params;

        for (const auto& p : spec.params)
        {
            auto item = object();
            auto* fields = item.getDynamicObject();
            fields->setProperty ("id", p.id);
            fields->setProperty ("name", p.name);
            fields->setProperty ("min", p.min);
            fields->setProperty ("max", p.max);
            fields->setProperty ("value", p.defaultValue);
            fields->setProperty ("unit", unitName (p.unit));

            if (! p.choices.isEmpty())
            {
                juce::Array<juce::var> choices;

                for (const auto& choice : p.choices)
                    choices.add (choice);

                fields->setProperty ("choices", choices);
            }

            params.add (item);
        }

        auto item = object();
        auto* fields = item.getDynamicObject();
        fields->setProperty ("id", spec.id);
        fields->setProperty ("name", spec.name);
        fields->setProperty ("description", spec.description);
        fields->setProperty ("magenta", spec.magenta);
        fields->setProperty ("params", params);
        list.add (item);
    }

    auto data = object();
    data.getDynamicObject()->setProperty ("types", list);
    data.getDynamicObject()->setProperty ("maxDevices", MidiFxChain::maxDevices);
    webView->emitEventIfBrowserIsVisible ("midiFxTypes", data);
}

//==============================================================================
void MainComponent::handleMidiFx (const juce::String& what, const juce::var& event)
{
    const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
    const auto fxId = static_cast<int> (event.getProperty ("fxId", 0));

    if (what == "on")
    {
        if (engine.setMidiFxOn (trackId, fxId, static_cast<bool> (event.getProperty ("on", true))))
        {
            markChanged();
            pushTracks();
        }

        return;
    }

    if (what == "param")
    {
        if (engine.findTrack (trackId) != nullptr)
        {
            engine.setMidiFxParam (trackId, fxId, static_cast<int> (event.getProperty ("index", -1)),
                                   static_cast<float> (number (event.getProperty ("value", {}), 0.0)));
            markChanged();
        }

        return;
    }

    // Adding, removing and moving are undo steps.
    auto before = engine.getMidiFx (trackId);
    juce::String name;
    int added = 0;

    if (what == "add")
    {
        const auto type = event.getProperty ("type", {}).toString();
        auto settings = event;
        name = "Add MIDI FX";

        // A Sonify device: its build, from its file in My Devices (or one already here).
        if (type == "device")
        {
            juce::String error;
            settings = deviceSettingsFor (event, error);

            if (settings.isVoid())
            {
                if (webView != nullptr)
                {
                    auto data = object();
                    data.getDynamicObject()->setProperty ("trackId", trackId);
                    data.getDynamicObject()->setProperty ("error", error);
                    webView->emitEventIfBrowserIsVisible ("midiFxAdded", data);
                }

                return;
            }

            name = "Add " + settings.getProperty ("device", {}).getProperty ("name", "Device").toString();
        }

        added = engine.addMidiFx (trackId, type, static_cast<int> (event.getProperty ("position", -1)), settings);

        if (added == 0)
        {
            const auto* track = engine.findTrack (trackId);
            const auto why = track == nullptr || track->kind != Track::Kind::instrument ? "MIDI FX go on instrument tracks."
                           : track->midiFx.isFull() ? "That track's MIDI FX are full."
                                                    : "That MIDI FX couldn't be added.";

            if (webView != nullptr)
            {
                auto data = object();
                data.getDynamicObject()->setProperty ("trackId", trackId);
                data.getDynamicObject()->setProperty ("error", why);
                webView->emitEventIfBrowserIsVisible ("midiFxAdded", data);
            }

            return;
        }
    }
    else if (what == "remove")
    {
        if (! engine.removeMidiFx (trackId, fxId))
            return;

        name = "Remove MIDI FX";
    }
    else if (what == "move")
    {
        const auto* track = engine.findTrack (trackId);
        const auto from = track != nullptr ? track->midiFx.indexOf (fxId) : -1;
        const auto to = static_cast<int> (event.getProperty ("position", from));

        if (from < 0 || from == to || ! engine.moveMidiFx (trackId, fxId, to))
            return;

        name = "Move MIDI FX";
    }
    else
    {
        return;
    }

    addToHistory (name, new Edits::MidiFxChanged (engine, trackId, before, engine.getMidiFx (trackId)));
    pushTracks();

    if (added != 0 && webView != nullptr)
    {
        auto data = object();
        data.getDynamicObject()->setProperty ("trackId", trackId);
        data.getDynamicObject()->setProperty ("fxId", added);
        data.getDynamicObject()->setProperty ("open", static_cast<bool> (event.getProperty ("open", false)));
        webView->emitEventIfBrowserIsVisible ("midiFxAdded", data);
    }
}

//==============================================================================
void MainComponent::runMidiTool (const juce::var& event)
{
    const auto requestId = event.getProperty ("requestId", 0);
    auto reply = object();
    reply.getDynamicObject()->setProperty ("requestId", requestId);

    const auto finish = [this, &reply] (const juce::String& error)
    {
        if (error.isNotEmpty())
            reply.getDynamicObject()->setProperty ("error", error);

        if (webView != nullptr)
            webView->emitEventIfBrowserIsVisible ("midiToolResult", reply);
    };

    std::vector<std::unique_ptr<MidiFx::Device>> devices;

    if (const auto* list = event.getProperty ("devices", {}).getArray())
    {
        for (const auto& item : *list)
        {
            auto device = MidiFx::create (item.getProperty ("type", {}).toString(), item);

            if (device == nullptr || device->spec.magenta)
                return finish ("That tool can't be applied here.");

            devices.push_back (std::move (device));
        }
    }

    if (devices.empty())
        return finish ("No tool to apply.");

    std::vector<MidiFx::Note> notes;

    if (const auto* items = event.getProperty ("notes", {}).getArray())
    {
        for (const auto& item : *items)
        {
            const auto* values = item.getArray();

            if (values == nullptr || values->size() < 4)
                continue;

            MidiFx::Note note;
            note.pitch = static_cast<int> (number ((*values)[0], -1.0));
            note.start = number ((*values)[1], -1.0);
            note.length = number ((*values)[2], 0.0);
            note.velocity = juce::jlimit (1, 127, static_cast<int> (number ((*values)[3], 100.0)));
            note.channel = values->size() > 4 ? juce::jlimit (0, 15, static_cast<int> (number ((*values)[4], 0.0))) : 0;

            if (note.pitch >= 0 && note.pitch <= 127 && note.start >= 0.0 && note.length > 0.0)
                notes.push_back (note);
        }
    }

    const auto from = juce::jmax (0.0, number (event.getProperty ("from", {}), 0.0));
    const auto to = juce::jmax (from + 1.0 / 64.0, number (event.getProperty ("to", {}), from + 4.0));

    if (to - from > 4096.0 || notes.size() > 20000)
        return finish ("That's too much to apply a tool to at once.");

    engine.prepareOfflineFollowing (devices);   // Harmonize: the track it follows, from its clips
    auto result = MidiFx::runOffline (devices, std::move (notes), engine.getTempoMap(), from, to);

    // What lands in the span stays in it: notes starting past its end are left out, and a
    // note moved a hair before its start starts there.
    std::vector<MidiFx::Note> kept;

    for (auto note : result)
    {
        if (note.start >= to - 1.0e-9 || note.start + note.length <= from)
            continue;

        if (note.start < from)
        {
            note.length -= from - note.start;
            note.start = from;
        }

        kept.push_back (note);
    }

    reply.getDynamicObject()->setProperty ("notes", notesToVar (kept));
    finish ({});
}

void MainComponent::addMidiToolTrack (const juce::var& event)
{
    const auto afterId = static_cast<int> (event.getProperty ("afterTrackId", 0));
    const bool drums = static_cast<bool> (event.getProperty ("drums", false));
    int index = static_cast<int> (engine.getTracks().size());

    for (size_t i = 0; i < engine.getTracks().size(); ++i)
        if (engine.getTracks()[i]->id == afterId)
            index = static_cast<int> (i) + 1;

    const auto id = engine.addTrack (Track::Kind::instrument);
    int at = 0;

    if (auto moved = engine.detachTrack (id, at))
        engine.attachTrack (std::move (moved), index);

    juce::String note;

    if (const auto* best = SessionPlayers::bestInstrument (library, drums ? SessionPlayers::Player::drums : SessionPlayers::Player::keys))
    {
        int slot = 0;

        if (engine.loadPlugin (id, juce::File (best->file), best->id, -1, 0, slot).wasOk())
            library.markUsed (best->file, best->id);
        else
            note = best->name + " didn't load, so the new track plays Cayman.";
    }

    const auto name = event.getProperty ("name", {}).toString().substring (0, 60).trim();
    engine.setTrackName (id, name.isNotEmpty() ? name : juce::String ("MIDI Tool"));

    if (const auto* clips = event.getProperty ("clips", {}).getArray())
    {
        for (const auto& item : *clips)
        {
            MidiClip clip;
            clip.start = juce::jmax (0.0, number (item.getProperty ("start", {}), 0.0));
            clip.length = juce::jlimit (1.0 / 16.0, 4096.0, number (item.getProperty ("length", {}), 4.0));
            clip.offset = 0.0;
            clip.notes = notesFromVar (item.getProperty ("notes", {}));
            engine.addClip (id, std::move (clip));
        }
    }

    const auto label = event.getProperty ("label", "MIDI Tool").toString().substring (0, 60);
    addToHistory (label, new Edits::TrackAdded (engine, id, index));
    pushTracks (note);
}

//==============================================================================
void MainComponent::updateMagenta()
{
    std::vector<AudioEngine::MagentaRequest> requests;
    engine.takeMagentaRequests (requests);

    if (webView == nullptr)
        return;

    static const char* modes[] = { "answer", "follow", "loop" };

    for (const auto& request : requests)
    {
        const auto& window = request.window;
        std::vector<MidiFx::Note> notes;

        for (int i = 0; i < window.count; ++i)
        {
            const auto& played = window.notes[static_cast<size_t> (i)];
            MidiFx::Note note;
            note.pitch = played.pitch;
            note.start = played.start;
            note.length = played.length;
            note.velocity = played.velocity;
            notes.push_back (note);
        }

        std::sort (notes.begin(), notes.end(), [] (const auto& a, const auto& b) { return a.start < b.start; });

        auto key = object();
        key.getDynamicObject()->setProperty ("root", window.keyRoot);
        key.getDynamicObject()->setProperty ("notes", static_cast<int> (window.keyNotes));

        auto data = object();
        auto* fields = data.getDynamicObject();
        fields->setProperty ("trackId", request.trackId);
        fields->setProperty ("fxId", request.fxId);
        fields->setProperty ("tool", request.tool);
        fields->setProperty ("mode", modes[juce::jlimit (0, 2, request.mode)]);
        fields->setProperty ("temperature", request.temperature);
        fields->setProperty ("inKey", request.inKey);
        fields->setProperty ("window", window.number);
        fields->setProperty ("length", window.length);
        fields->setProperty ("bpm", window.bpm);
        fields->setProperty ("key", key);
        fields->setProperty ("notes", notesToVar (notes));
        webView->emitEventIfBrowserIsVisible ("magentaRequest", data);
    }
}

void MainComponent::answerMagentaWindow (const juce::var& event)
{
    MidiFx::MagentaDevice::Window answer;
    answer.number = static_cast<int> (event.getProperty ("window", 0));
    answer.length = juce::jmax (0.25, number (event.getProperty ("length", {}), 4.0));
    answer.count = 0;

    if (const auto* items = event.getProperty ("notes", {}).getArray())
    {
        for (const auto& item : *items)
        {
            const auto* values = item.getArray();

            if (values == nullptr || values->size() < 4 || answer.count >= MidiFx::MagentaDevice::maxWindowNotes)
                continue;

            const auto pitch = static_cast<int> (number ((*values)[0], -1.0));
            const auto start = number ((*values)[1], -1.0);
            const auto length = number ((*values)[2], 0.0);

            if (pitch < 0 || pitch > 127 || start < 0.0 || start >= answer.length || length <= 0.0)
                continue;

            auto& note = answer.notes[static_cast<size_t> (answer.count++)];
            note.pitch = static_cast<juce::uint8> (pitch);
            note.start = static_cast<float> (start);
            note.length = static_cast<float> (juce::jmin (length, answer.length - start));
            note.velocity = static_cast<juce::uint8> (juce::jlimit (1, 127, static_cast<int> (number ((*values)[3], 100.0))));
        }
    }

    engine.answerMagenta (static_cast<int> (event.getProperty ("trackId", 0)), static_cast<int> (event.getProperty ("fxId", 0)), answer);
}

//==============================================================================
void MainComponent::listMidiTools (const juce::String& error)
{
    if (webView == nullptr)
        return;

    juce::Array<juce::var> installed;

    for (const auto& folder : midiToolsFolder().findChildFiles (juce::File::findDirectories, false))
    {
        if (! isToolId (folder.getFileName()))
            continue;   // a download still going ("<id>~download") or something else

        auto tool = juce::JSON::parse (folder.getChildFile ("tool.json"));

        if (! tool.isObject() || ! folder.getChildFile ("model").getChildFile ("weights_manifest.json").existsAsFile())
            continue;

        tool.getDynamicObject()->setProperty ("id", folder.getFileName());
        installed.add (tool);
    }

    auto data = object();
    data.getDynamicObject()->setProperty ("installed", installed);
    data.getDynamicObject()->setProperty ("magenta", SONIFY_MAGENTA != 0);
    data.getDynamicObject()->setProperty ("folder", midiToolsFolder().getFullPathName());

    if (error.isNotEmpty())
        data.getDynamicObject()->setProperty ("error", error);

    webView->emitEventIfBrowserIsVisible ("midiTools", data);
}

void MainComponent::installMidiTool (const juce::var& tool)
{
    const auto id = tool.getProperty ("id", {}).toString();

    if (! isToolId (id))
        return;

    const auto progress = [this, id] (double fraction, const juce::String&)
    {
        if (webView == nullptr)
            return;

        auto data = object();
        data.getDynamicObject()->setProperty ("id", id);
        data.getDynamicObject()->setProperty ("fraction", fraction);
        webView->emitEventIfBrowserIsVisible ("midiToolProgress", data);
    };

    cloud->installMidiTool (tool, midiToolsFolder(), [this, id] (const juce::Result& result, const juce::var&)
    {
        if (webView != nullptr)
        {
            auto data = object();
            data.getDynamicObject()->setProperty ("id", id);
            data.getDynamicObject()->setProperty ("ok", result.wasOk());
            data.getDynamicObject()->setProperty ("error", result.getErrorMessage());
            webView->emitEventIfBrowserIsVisible ("midiToolInstalled", data);
        }

        listMidiTools();
    }, progress);
}

void MainComponent::removeMidiTool (const juce::String& id)
{
    if (! isToolId (id))
        return;

    const auto folder = midiToolsFolder().getChildFile (id);

    if (folder.isDirectory() && ! folder.deleteRecursively())
        return listMidiTools ("Couldn't remove it: is a file in its folder open somewhere?");

    listMidiTools();
}

std::optional<juce::WebBrowserComponent::Resource> MainComponent::getMidiToolResource (const juce::String& path)
{
    const auto toBytes = [] (const void* data, size_t size)
    {
        std::vector<std::byte> bytes (size);
        std::memcpy (bytes.data(), data, size);
        return bytes;
    };

    // magenta.js, built in.
    if (path == "magenta/magentamusic.js")
    {
       #if SONIFY_MAGENTA
        return juce::WebBrowserComponent::Resource { toBytes (SonifyMagentaData::magentamusic_js, static_cast<size_t> (SonifyMagentaData::magentamusic_jsSize)),
                                                     "text/javascript" };
       #else
        return std::nullopt;
       #endif
    }

    // A tool's model: midi-tools/<id>/model/<file>, and nothing else of the disk.
    const auto parts = juce::StringArray::fromTokens (path, "/", {});

    if (parts.size() != 4 || parts[0] != "midi-tools" || ! isToolId (parts[1]) || parts[2] != "model" || ! isPlainName (parts[3]))
        return std::nullopt;

    const auto file = midiToolsFolder().getChildFile (parts[1]).getChildFile ("model").getChildFile (parts[3]);
    juce::MemoryBlock data;

    if (! file.existsAsFile() || ! file.loadFileAsData (data))
        return std::nullopt;

    return juce::WebBrowserComponent::Resource { toBytes (data.getData(), data.getSize()),
                                                 parts[3].endsWithIgnoreCase (".json") ? "application/json" : "application/octet-stream" };
}
