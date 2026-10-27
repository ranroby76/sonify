// D:\workspace\Fanan_web_hub\src\MainImport.cpp

#include "MainComponent.h"
#include "PageDropZone.h"
#include "VideoSound.h"

#include <cmath>

//==============================================================================
// Media brought into the song (hub-import.js): audio files dragged in straight from a folder on
// the computer and dropped in the arrangement, Load Media... (an audio track's right-click menu,
// and the arrangement's), and File > Import Media Files....
//
// "importMedia" events from the page:
//   { action: "fileDrag", left, top, width, height }   files from the computer over the
//        arrangement: Sonify's drop zone goes over it (the page can't learn where such files are)
//   { action: "files", paths, trackId, after, start, oneTrack }   the files to bring in, from
//        start (quarter notes): the first onto trackId when that's an audio track, each other one
//        (or every one, with no such track) onto a new audio track named after it, each new track
//        under the one before (the first under `after`, or at the end); oneTrack: all on one
//        track, one after another
//   { action: "choose", trackId, after, start }   Load Media...: the files chosen go the same way
// Sonify tells the page:
//   "arrangeFileDrag" { active, x, y, shift, count, name }   files moving over the drop zone
//                                                           (active false: it's gone)
//   "arrangeFileDrop" { paths, x, y, shift }                 dropped there; the page answers with
//                                                           "files", placed where that is
//   "mediaImport" { state: "working" | "done", done, total, name }   files being read into WAVs
//
// Audio files of the kinds Sonify plays as they are (WAV, AIFF, FLAC, Ogg Vorbis, MP3; WMA on
// Windows; M4A, AAC and CAF on a Mac) go in as they are. Other kinds FFmpeg reads (M4A, AAC, Opus,
// WMA, ALAC, AC-3, WavPack, APE...) are written into WAVs first, in the song's Media folder, on a
// thread of their own: every part of Sonify (the Audio Editor, freezing, exports) reads those.
// A folder brings its audio files, by name. A video becomes the song's video, as when it's
// imported (one at a time). All the clips and tracks are one undo step.

namespace
{
    /** Kinds FFmpeg reads sound from: the ones Sonify doesn't play as they are become WAVs. */
    const char* const convertedKinds = "m4a;m4b;m4r;aac;adts;opus;oga;ogg;wma;asf;alac;caf;ac3;eac3;dts;wv;ape;mka;amr;awb;au;snd;"
                                       "mp2;mpa;tta;tak;aifc;wav;wave;aif;aiff;flac;mp3;3ga";

    bool shiftHeld();

    juce::var pathList (const juce::Array<juce::File>& files)
    {
        juce::Array<juce::var> list;

        for (const auto& file : files)
            list.add (file.getFullPathName());

        return list;
    }
}

bool MainComponent::isImportable (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    return engine.getMedia().isAudioFile (file)
        || VideoMedia::isVideoFile (file)
        || (VideoSound::available() && file.hasFileExtension (convertedKinds));
}

juce::Array<juce::File> MainComponent::mediaIn (const juce::StringArray& paths)
{
    juce::Array<juce::File> found;

    for (const auto& path : paths)
    {
        const juce::File file (path);

        if (! file.isDirectory())
        {
            if (isImportable (file))
                found.addIfNotAlreadyThere (file);

            continue;
        }

        // A folder: its audio files, by name (not its videos, nor what's in its folders).
        std::vector<juce::File> inside;

        for (const auto& entry : juce::RangedDirectoryIterator (file, false, "*", juce::File::findFiles | juce::File::ignoreHiddenFiles))
            if (isImportable (entry.getFile()) && ! VideoMedia::isVideoFile (entry.getFile()))
                inside.push_back (entry.getFile());

        std::sort (inside.begin(), inside.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });

        for (const auto& each : inside)
            found.addIfNotAlreadyThere (each);
    }

    return found;
}

//==============================================================================
void MainComponent::handleImport (const juce::var& event)
{
    const auto action = event.getProperty ("action", {}).toString();

    if (action == "fileDrag")
    {
        showArrangeDropZone (event);
        return;
    }

    MediaPlace place;
    place.trackId = static_cast<int> (event.getProperty ("trackId", 0));
    place.after = static_cast<int> (event.getProperty ("after", 0));
    place.start = juce::jmax (0.0, static_cast<double> (event.getProperty ("start", 0.0)));
    place.oneTrack = static_cast<bool> (event.getProperty ("oneTrack", false));

    if (! std::isfinite (place.start))
        place.start = 0.0;

    if (action == "choose")
    {
        chooseMedia (place);
        return;
    }

    if (action == "files")
    {
        juce::StringArray paths;

        if (const auto* list = event.getProperty ("paths", {}).getArray())
            for (const auto& path : *list)
                paths.add (path.toString());

        importMedia (mediaIn (paths), place);
    }
}

//==============================================================================
void MainComponent::showArrangeDropZone (const juce::var& event)
{
    if (webView == nullptr)
        return;

    const juce::Rectangle<int> area (juce::roundToInt (static_cast<double> (event.getProperty ("left", 0.0))),
                                     juce::roundToInt (static_cast<double> (event.getProperty ("top", 0.0))),
                                     juce::roundToInt (static_cast<double> (event.getProperty ("width", 0.0))),
                                     juce::roundToInt (static_cast<double> (event.getProperty ("height", 0.0))));

    if (area.getWidth() < 40 || area.getHeight() < 20)
        return;

    if (arrangeDropZone == nullptr)
    {
        // Quiet: the page shows where the files would go as they move.
        auto zone = std::make_unique<PageDropZone> (juce::String());

        zone->accepts = [this] (const juce::File& file) { return file.isDirectory() || isImportable (file); };

        zone->onMove = [this] (const juce::StringArray& files, juce::Point<int> onPage)
        {
            if (webView == nullptr)
                return;

            // What's dragged is looked at once (a folder opened), then only where it is.
            if (files != arrangeDragPaths)
            {
                arrangeDragPaths = files;
                const auto media = mediaIn (files);
                arrangeDragCount = media.size();
                arrangeDragName = media.isEmpty() ? juce::String() : media.getFirst().getFileName();
            }

            const auto shift = shiftHeld();
            const auto now = juce::Time::getMillisecondCounterHiRes();

            if (onPage == arrangeDragAt && shift == arrangeDragShift && now - arrangeDragSent < 250.0)
                return;

            arrangeDragAt = onPage;
            arrangeDragShift = shift;
            arrangeDragSent = now;

            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("active", true);
            item->setProperty ("x", onPage.x);
            item->setProperty ("y", onPage.y);
            item->setProperty ("shift", shift);
            item->setProperty ("count", arrangeDragCount);
            item->setProperty ("name", arrangeDragName);
            webView->emitEventIfBrowserIsVisible ("arrangeFileDrag", juce::var (item.get()));
        };

        zone->onHidden = [this]
        {
            arrangeDragPaths.clear();
            arrangeDragAt = { -1, -1 };

            if (webView == nullptr)
                return;

            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("active", false);
            webView->emitEventIfBrowserIsVisible ("arrangeFileDrag", juce::var (item.get()));
        };

        zone->onDrop = [this] (const juce::StringArray& dropped, juce::Point<int> onPage)
        {
            const auto media = mediaIn (dropped);

            if (media.isEmpty())
            {
                pushTracks ("Only audio files (and a video) go in the arrangement.");
                return;
            }

            // The page knows which track and bar that is.
            if (webView != nullptr)
            {
                juce::DynamicObject::Ptr item (new juce::DynamicObject());
                item->setProperty ("paths", pathList (media));
                item->setProperty ("x", onPage.x);
                item->setProperty ("y", onPage.y);
                item->setProperty ("shift", shiftHeld());
                webView->emitEventIfBrowserIsVisible ("arrangeFileDrop", juce::var (item.get()));
            }
        };

        arrangeDropZone = std::move (zone);
    }

    static_cast<PageDropZone*> (arrangeDropZone.get())->showOver (webView->localAreaToGlobal (area), area.getPosition());
}

//==============================================================================
//==============================================================================
// File > Master a File...: an audio file chosen, then a new song (as File > New Song makes one,
// named after it) with the file on an audio track of its own, the Mastering section in and its
// mode Master only, and its window open on the page.

void MainComponent::masterFile()
{
    if (choosingFile)
        return;

    whenSongIsSafe ([this]
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        auto wildcards = formats.getWildcardForAllFormats();

        if (VideoSound::available())
            for (const auto& kind : juce::StringArray::fromTokens (convertedKinds, ";", {}))
                if (! wildcards.containsIgnoreCase ("*." + kind + ";") && ! wildcards.endsWithIgnoreCase ("*." + kind))
                    wildcards << ";*." << kind;

        auto folder = lastMediaFolder;

        if (! folder.isDirectory())
            folder = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

        choosingFile = true;
        masterFileChooser = std::make_unique<juce::FileChooser> ("Master a File", folder, wildcards);

        masterFileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                        [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            auto& self = *safeThis;
            self.choosingFile = false;
            const auto file = chooser.getResult();

            if (! file.existsAsFile())
                return;

            self.lastMediaFolder = file.getParentDirectory();

            // A new song, named after the file (New Song's way: a folder of its own).
            const auto before = self.songFile;
            juce::DynamicObject::Ptr song (new juce::DynamicObject());
            song->setProperty ("name", file.getFileNameWithoutExtension() + " Master");
            self.newSong (juce::var (song.get()));

            if (self.songFile == before || self.songFile == juce::File())
                return;   // it couldn't be made (New Song said why)

            // The file alone: the instrument track a new song starts with goes.
            std::vector<int> ids;

            for (const auto& track : self.engine.getTracks())
                ids.push_back (track->id);

            for (const auto id : ids)
                self.engine.removeTrack (id);

            auto& mastering = self.engine.getMastering().settings;
            mastering.reset();
            mastering.set (MasterSection::on, 1.0f);
            mastering.set (MasterSection::mode, 2.0f);   // Master only

            self.importMedia ({ file }, MediaPlace {});
            self.undoManager.clearUndoHistory();
            self.markChanged();
            self.pushTracks();

            if (self.webView != nullptr)
                self.webView->emitEventIfBrowserIsVisible ("masteringOpen", juce::var());
        });
    });
}

void MainComponent::chooseMedia (MediaPlace place)
{
    if (choosingFile)
        return;

    // Every kind that comes in: the ones Sonify plays as they are, and the ones it reads into WAVs.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto wildcards = formats.getWildcardForAllFormats();

    if (VideoSound::available())
        for (const auto& kind : juce::StringArray::fromTokens (convertedKinds, ";", {}))
            if (! wildcards.containsIgnoreCase ("*." + kind + ";") && ! wildcards.endsWithIgnoreCase ("*." + kind))
                wildcards << ";*." << kind;

    auto folder = lastMediaFolder;

    if (! folder.isDirectory())
        folder = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    choosingFile = true;
    mediaChooser = std::make_unique<juce::FileChooser> ("Load Media", folder, wildcards);

    mediaChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                   | juce::FileBrowserComponent::canSelectMultipleItems,
                               [safeThis = juce::Component::SafePointer<MainComponent> (this), place] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        const auto chosen = chooser.getResults();

        if (chosen.isEmpty())
            return;

        safeThis->lastMediaFolder = chosen.getFirst().getParentDirectory();
        juce::StringArray paths;

        for (const auto& file : chosen)
            paths.add (file.getFullPathName());

        safeThis->importMedia (safeThis->mediaIn (paths), place);
    });
}

//==============================================================================
void MainComponent::importMedia (const juce::Array<juce::File>& files, MediaPlace place)
{
    juce::Array<juce::File> audio;
    juce::File video;
    juce::StringArray problems;

    for (const auto& file : files)
    {
        if (! VideoMedia::isVideoFile (file))
            audio.add (file);
        else if (video == juce::File())
            video = file;
        else
            problems.add (file.getFileName() + " wasn't brought in: a song has one video.");
    }

    // A video: the song's video from there, its sound on a track of its own (MainVideo.cpp).
    if (video != juce::File())
        importVideo (video, place.start);

    if (audio.isEmpty())
    {
        if (! problems.isEmpty())
            pushTracks (problems.joinIntoString ("\n"));

        return;
    }

    // The ones Sonify doesn't play as they are, and FFmpeg reads, become WAVs first. (The ones it
    // plays are read together first, on several threads.)
    std::vector<int> converting;

    {
        juce::StringArray readable;

        for (const auto& file : audio)
            if (engine.getMedia().isAudioFile (file))
                readable.add (file.getFullPathName());

        engine.getMedia().prefetch (readable, engine.getPlaybackRate());
    }

    for (int i = 0; i < audio.size(); ++i)
    {
        juce::String error;

        if (engine.getMedia().isAudioFile (audio[i]) && engine.getMedia().get (audio[i], engine.getPlaybackRate(), error) != nullptr)
            continue;

        if (VideoSound::available() && audio[i].hasFileExtension (convertedKinds))
            converting.push_back (i);
    }

    if (converting.empty())
    {
        placeMedia (audio, place, problems);
        return;
    }

    auto folder = engine.getRecordingFolder();

    if (folder == juce::File())
        folder = UserData::recordings();

    folder.createDirectory();

    const auto total = static_cast<int> (converting.size());
    const auto generation = songGeneration;
    const auto alive = importAlive.flag;

    const auto progress = [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::String& state, int done, int count, const juce::String& name)
    {
        juce::MessageManager::callAsync ([safeThis, state, done, count, name]
        {
            if (safeThis == nullptr || safeThis->webView == nullptr)
                return;

            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("state", state);
            item->setProperty ("done", done);
            item->setProperty ("total", count);
            item->setProperty ("name", name);
            safeThis->webView->emitEventIfBrowserIsVisible ("mediaImport", juce::var (item.get()));
        });
    };

    juce::Thread::launch ([safeThis = juce::Component::SafePointer<MainComponent> (this), audio, converting, folder, place, problems,
                           total, generation, alive, progress] () mutable
    {
        for (int n = 0; n < total; ++n)
        {
            if (! alive->load())
                return;

            auto& source = audio.getReference (converting[static_cast<size_t> (n)]);
            progress ("working", n, total, source.getFileName());

            const auto wav = folder.getNonexistentChildFile (source.getFileNameWithoutExtension(), ".wav", false);
            juce::String error;

            if (VideoSound::soundToWav (source, wav, error, [alive] { return alive->load(); }))
            {
                source = wav;
            }
            else
            {
                if (error == "Cancelled")
                    return;

                problems.add ("Couldn't bring in " + source.getFileName() + ": " + error);
                source = juce::File();
            }
        }

        progress ("done", total, total, {});

        juce::MessageManager::callAsync ([safeThis, audio, place, problems, generation]
        {
            // Another song opened meanwhile: not into that one.
            if (safeThis == nullptr || safeThis->songGeneration != generation)
                return;

            juce::Array<juce::File> readable;

            for (const auto& file : audio)
                if (file != juce::File())
                    readable.add (file);

            safeThis->placeMedia (readable, place, problems);
        });
    });
}

int MainComponent::addAudioTrackAfter (int afterId, const juce::String& name, const juce::String& undoName, bool joinPreviousStep)
{
    const auto id = engine.addTrack (Track::Kind::audio);
    engine.setTrackName (id, name);

    // Under the track named (as Studio One puts a dropped file's track there), or at the end.
    if (afterId != 0 && engine.findTrack (afterId) != nullptr)
    {
        auto order = Edits::allTrackIds (engine);
        order.erase (std::remove (order.begin(), order.end(), id), order.end());
        const auto after = std::find (order.begin(), order.end(), afterId);
        order.insert (after == order.end() ? order.end() : after + 1, id);
        engine.setTrackOrder (order);
    }

    const auto placed = Edits::allTrackIds (engine);
    const auto index = static_cast<int> (std::find (placed.begin(), placed.end(), id) - placed.begin());
    addToHistory (undoName, new Edits::TrackAdded (engine, id, index), joinPreviousStep);
    return id;
}

void MainComponent::placeMedia (const juce::Array<juce::File>& files, MediaPlace place, juce::StringArray problems)
{
    const auto undoName = juce::String (files.size() > 1 ? "Import Audio Files" : "Import Audio");
    const auto* given = engine.findTrack (place.trackId);
    const auto ontoGiven = given != nullptr && given->kind == Track::Kind::audio;

    std::vector<int> added;
    bool first = true;               // the first change opens the undo step, the rest join it
    int lastTrack = 0;               // the track the last file went onto
    double at = place.start;         // where the next goes, one after another on one track

    for (const auto& file : files)
    {
        juce::String error;
        const auto data = engine.getMedia().get (file, engine.getPlaybackRate(), error);

        if (data == nullptr || data->lengthSeconds <= 0.0)
        {
            problems.add ("Couldn't read " + file.getFileName() + " as audio.");
            continue;
        }

        int trackId = 0;

        if (place.oneTrack && lastTrack != 0)
            trackId = lastTrack;
        else if (lastTrack == 0 && ontoGiven)
            trackId = place.trackId;
        else
        {
            trackId = addAudioTrackAfter (lastTrack != 0 ? lastTrack : place.after, file.getFileNameWithoutExtension(), undoName, ! first);
            first = false;
            added.push_back (trackId);
        }

        auto before = Edits::snapshot (engine, { trackId });

        if (engine.importAudio (trackId, file, place.oneTrack ? at : place.start) <= 0)
        {
            problems.add ("Couldn't put " + file.getFileName() + " on its track.");
            continue;
        }

        addToHistory (undoName, new Edits::ClipsChanged (engine, std::move (before), Edits::snapshot (engine, { trackId })), ! first);
        first = false;
        lastTrack = trackId;

        if (place.oneTrack)
            at += engine.audioLengthInPpq (at, data->lengthSeconds);
    }

    if (! added.empty())
        selectNewTracks (added);

    if (added.empty() || ! problems.isEmpty())
        pushTracks (problems.joinIntoString ("\n"));
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
    /** Shift held right now. While files are dragged in from Explorer the keys go to Explorer,
        so Windows is asked about the key itself. */
    bool shiftHeld()
    {
       #if JUCE_WINDOWS
        return (GetAsyncKeyState (VK_SHIFT) & 0x8000) != 0;
       #else
        return juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
       #endif
    }
}
