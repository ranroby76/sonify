// D:\workspace\Fanan_web_hub\src\MainTracks.cpp

#include "MainComponent.h"
#include "Song.h"

//==============================================================================
// The track list's right-click menu (hub-tracks.js), as Studio One's track column has it:
//   addTracks      Add Tracks... (its dialog), and Add Audio Track (mono / stereo), Add Instrument
//                  Track, Add Choir Track: { type: "instrument" | "audio" | "choir" | "bus" | "fx",
//                  count, name, mono, input (-1: the usual), ascending (each next track the next
//                  input), file, pluginId (an instrument from the Browser) }. A choir track is an
//                  audio track singing its clips as a choir too (Choir.h), with four voices to
//                  start. One undo step; the new tracks are selected.
//   addForInputs   Add Tracks for all Inputs: an audio track for each of the song's inputs (Song
//                  Setup > Audio I/O Setup), named after it, mono or stereo as it is.
//   storePreset    Store Track Preset...: the tracks named ({ ids }) into a .trackpreset file.
//   loadPreset     Load Track Preset...: a preset's tracks added at the end, as one undo step.

namespace
{
    juce::File trackPresetFolder()
    {
        return UserData::getFolder().getChildFile ("Track Presets");
    }
}

void MainComponent::handleTrackMenu (const juce::var& event)
{
    const auto action = event.getProperty ("action", {}).toString();

    if (action == "addTracks")
    {
        const auto type = event.getProperty ("type", "instrument").toString();
        const auto choir = type == "choir";
        const auto kind = type == "audio" || choir ? Track::Kind::audio : type == "bus" ? Track::Kind::bus
                        : type == "fx" ? Track::Kind::fx : Track::Kind::instrument;
        const auto count = juce::jlimit (1, 64, static_cast<int> (event.getProperty ("count", 1)));
        const auto name = event.getProperty ("name", {}).toString().trim().substring (0, 60);
        const auto mono = static_cast<bool> (event.getProperty ("mono", false));
        const auto input = static_cast<int> (event.getProperty ("input", -1));
        const auto ascending = static_cast<bool> (event.getProperty ("ascending", false));
        const auto file = event.getProperty ("file", {}).toString();
        const auto pluginId = event.getProperty ("pluginId", {}).toString();
        const auto inputCount = static_cast<int> (engine.getInputs().size());

        const auto label = count > 1 ? juce::String ("Add Tracks")
                         : choir ? juce::String ("Add Choir Track")
                         : kind == Track::Kind::audio ? juce::String ("Add Audio Track")
                         : kind == Track::Kind::bus ? juce::String ("Add Bus Channel")
                         : kind == Track::Kind::fx ? juce::String ("Add FX Channel") : juce::String ("Add Track");

        std::vector<int> added;

        for (int i = 0; i < count; ++i)
        {
            const auto id = engine.addTrack (kind);
            addToHistory (label, new Edits::TrackAdded (engine, id, static_cast<int> (engine.getTracks().size()) - 1), i > 0);

            if (kind == Track::Kind::audio)
            {
                engine.setTrackMono (id, mono);

                if (input >= 0 && inputCount > 0)
                    engine.setTrackInput (id, ascending ? (input + i) % inputCount : juce::jmin (input, inputCount - 1));
            }

            // A choir track, named Choir 1, Choir 2... (the name given, if any, follows).
            if (choir)
            {
                const auto choirs = std::count_if (engine.getTracks().begin(), engine.getTracks().end(),
                                                   [] (const auto& t) { return t->choir != nullptr; });
                engine.setTrackChoir (id, ChoirSettings::makeDefault());

                if (auto* track = engine.findTrack (id))
                {
                    track->name = "Choir " + juce::String (static_cast<int> (choirs) + 1);
                    track->nameIsAutomatic = false;
                }
            }

            if (name.isNotEmpty())
            {
                if (auto* track = engine.findTrack (id))
                {
                    track->name = count > 1 ? name + " " + juce::String (i + 1) : name;
                    track->nameIsAutomatic = false;
                }
            }

            added.push_back (id);
        }

        // An instrument from the Browser on each: its loading joins the same undo step.
        if (kind == Track::Kind::instrument && file.isNotEmpty())
            for (const auto id : added)
                loadPluginInto (id, juce::File (file), pluginId, -1, 0, true);

        selectNewTracks (added);
        return;
    }

    if (action == "addForInputs")
    {
        const auto inputs = engine.getInputs();
        std::vector<int> added;

        for (size_t i = 0; i < inputs.size(); ++i)
        {
            const auto id = engine.addTrack (Track::Kind::audio);
            addToHistory ("Add Tracks for All Inputs", new Edits::TrackAdded (engine, id, static_cast<int> (engine.getTracks().size()) - 1), i > 0);
            engine.setTrackInput (id, static_cast<int> (i));
            engine.setTrackMono (id, inputs[i].right < 0);

            if (auto* track = engine.findTrack (id); track != nullptr && inputs[i].name.isNotEmpty())
            {
                track->name = inputs[i].name;
                track->nameIsAutomatic = false;
            }

            added.push_back (id);
        }

        selectNewTracks (added);
        return;
    }

    if (action == "storePreset" || action == "loadPreset")
    {
        std::vector<int> ids;

        if (const auto* list = event.getProperty ("ids", {}).getArray())
            for (const auto& value : *list)
                if (const auto id = static_cast<int> (value); engine.findTrack (id) != nullptr)
                    ids.push_back (id);

        chooseTrackPresetFile (action == "storePreset", ids);
    }
}

void MainComponent::selectNewTracks (const std::vector<int>& ids)
{
    // The keyboard plays the first that's a track (FX channels and buses aren't played).
    for (const auto id : ids)
    {
        if (const auto* track = engine.findTrack (id); track != nullptr && ! track->isMixChannel())
        {
            engine.selectTrack (id);
            break;
        }
    }

    pushTracks();

    if (webView != nullptr && ! ids.empty())
    {
        juce::Array<juce::var> list;

        for (const auto id : ids)
            list.add (id);

        juce::DynamicObject::Ptr selection (new juce::DynamicObject());
        selection->setProperty ("ids", list);
        webView->emitEventIfBrowserIsVisible ("selectTracks", juce::var (selection.get()));
    }
}

void MainComponent::chooseTrackPresetFile (bool storing, const std::vector<int>& ids)
{
    if (choosingFile || (storing && ids.empty()))
        return;

    choosingFile = true;
    const auto folder = trackPresetFolder();
    folder.createDirectory();

    // Named after the first track stored.
    auto suggested = folder;

    if (storing)
        if (const auto* track = engine.findTrack (ids.front()))
            suggested = folder.getChildFile (juce::File::createLegalFileName (track->name + (ids.size() > 1 ? " and more" : ""))
                                             + Song::trackPresetExtension);

    trackPresetChooser = std::make_unique<juce::FileChooser> (storing ? "Store Track Preset" : "Load Track Preset", suggested,
                                                              juce::String ("*") + Song::trackPresetExtension);

    const auto chooserFlags = storing ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                               : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    trackPresetChooser->launchAsync (chooserFlags, [safeThis = juce::Component::SafePointer<MainComponent> (this), storing, ids] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        auto file = chooser.getResult();

        if (file == juce::File())
            return;

        if (storing)
        {
            file = file.withFileExtension (Song::trackPresetExtension);
            const auto preset = Song::captureTrackPreset (safeThis->engine, ids);

            if (! file.replaceWithText (juce::JSON::toString (preset)))
                safeThis->pushTracks ("Couldn't write " + file.getFullPathName());
            else
                safeThis->pushTracks ("Track preset stored: " + file.getFileNameWithoutExtension());

            return;
        }

        juce::StringArray problems;
        const auto loaded = Song::restoreTrackPreset (safeThis->engine, juce::JSON::parse (file), problems);

        if (loaded.empty())
        {
            safeThis->pushTracks (file.getFileName() + " isn't a track preset.");
            return;
        }

        const auto order = Edits::allTrackIds (safeThis->engine);
        bool first = true;

        for (const auto id : loaded)
        {
            const auto index = static_cast<int> (std::find (order.begin(), order.end(), id) - order.begin());
            safeThis->addToHistory ("Load Track Preset", new Edits::TrackAdded (safeThis->engine, id, index), ! first);
            first = false;
        }

        safeThis->markChanged();
        safeThis->selectNewTracks (loaded);

        if (! problems.isEmpty())
            safeThis->pushTracks (problems.joinIntoString ("\n"));
    });
}
