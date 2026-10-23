// D:\workspace\Fanan_web_hub\src\MainVideo.cpp
//
// The song's video, as Studio One has it: one video, its picture in the Video lane over the
// tracks (a strip of small pictures) and in the Video window where the song is, its sound an
// ordinary audio clip on a track of its own. Song > Export Video writes a new MP4: the picture
// as it was, the song's mix as its sound. VLC reads and shows it (VideoMedia.h).

#include "MainComponent.h"

namespace
{
    constexpr const char* thumbnailsPath = "song-video-thumbnails.jpg";
}

//==============================================================================
void MainComponent::handleSongVideo (const juce::var& event)
{
    const auto action = event.getProperty ("action", {}).toString();
    const auto& video = engine.getSongVideo();

    if (action == "import")
    {
        const auto at = event.hasProperty ("at") ? juce::jmax (0.0, static_cast<double> (event.getProperty ("at", 0.0)))
                                                 : engine.getTransport().getState().ppq;
        const auto path = event.getProperty ("file", {}).toString();

        if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
            importVideo (juce::File (path), at);
        else
            chooseVideo (at);
    }
    else if (action == "move")
    {
        if (video.isEmpty())
            return;

        const auto before = video;     // a copy: the engine's own changes next
        auto after = before;
        after.start = juce::jmax (0.0, static_cast<double> (event.getProperty ("start", before.start)));

        if (after == before)
            return pushSongVideo (true);

        engine.setSongVideo (after);
        addToHistory ("Move Video", new Edits::VideoChanged (engine, before, after));
        pushSongVideo();
    }
    else if (action == "remove")
    {
        if (video.isEmpty())
            return;

        const auto before = video;
        engine.setSongVideo ({});
        addToHistory ("Remove Video", new Edits::VideoChanged (engine, before, {}));
        pushSongVideo();
    }
    else if (action == "window")
    {
        showVideoWindow (static_cast<bool> (event.getProperty ("show", true)));
    }
    else if (action == "export")
    {
        chooseVideoExport();
    }
    else if (action == "cancelExport")
    {
        if (videoExportStage == VideoExportStage::mixing)
            engine.cancelMixdown();
        else if (videoExport != nullptr)
            videoExport->cancel();
    }
}

//==============================================================================
void MainComponent::importVideo (const juce::File& file, double at)
{
    if (const auto problem = VideoMedia::problem(); problem.isNotEmpty())
        return pushTracks (problem);

    if (videoImport != nullptr)
        return pushTracks ("A video is being imported: one at a time.");

    if (! file.existsAsFile())
        return pushTracks (file.getFileName() + " isn't there.");

    // Read once, faster than it plays: its sound into a WAV, its small pictures (VideoMedia.h).
    videoImport = std::make_unique<VideoMedia::Preparation> (file, VideoMedia::Wanted {});
    videoImportAt = juce::jmax (0.0, at);
    videoImportGeneration = songGeneration;
    videoProgressPushed = -1.0;
    pushSongVideo (true);
}

void MainComponent::chooseVideo (double at)
{
    if (choosingFile)
        return;

    choosingFile = true;
    songChooser = std::make_unique<juce::FileChooser> ("Import a video", juce::File::getSpecialLocation (juce::File::userMoviesDirectory),
                                                       VideoMedia::wildcards());

    songChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis = juce::Component::SafePointer<MainComponent> (this), at] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        const auto chosen = chooser.getResult();

        if (chosen.existsAsFile())
            safeThis->importVideo (chosen, at);
    });
}

void MainComponent::finishVideoImport()
{
    const auto prepared = videoImport->getResult();
    const auto file = videoImport->getVideo();
    videoImport = nullptr;

    // Another song opened meanwhile: the video was for the one before.
    if (videoImportGeneration != songGeneration)
    {
        pushSongVideo (true);
        return pushTracks (file.getFileName() + " was imported for the song before: import it again here.");
    }

    if (! prepared.ok())
    {
        pushSongVideo (true);

        if (prepared.error != "Cancelled")
            pushTracks ("Couldn't import " + file.getFileName() + ": " + prepared.error);

        return;
    }

    const auto before = engine.getSongVideo();
    const AudioEngine::SongVideo after { file, videoImportAt };
    bool joined = false;

    // Its sound: a WAV of its own with the song's recordings (its Media folder), on a new audio
    // track named after the video, starting with its picture.
    if (prepared.sound.existsAsFile())
    {
        auto folder = engine.getRecordingFolder();

        if (folder == juce::File())
            folder = UserData::recordings();

        folder.createDirectory();
        const auto wav = folder.getNonexistentChildFile (file.getFileNameWithoutExtension() + " (video sound)", ".wav", false);

        if (prepared.sound.copyFileTo (wav))
        {
            const auto trackId = engine.addTrack (Track::Kind::audio);
            engine.setTrackName (trackId, file.getFileNameWithoutExtension());
            addToHistory ("Import Video", new Edits::TrackAdded (engine, trackId, static_cast<int> (engine.getTracks().size()) - 1));
            joined = true;

            auto clipsBefore = Edits::snapshot (engine, { trackId });

            if (engine.importAudio (trackId, wav, videoImportAt) > 0)
                addToHistory ("Import Video", new Edits::ClipsChanged (engine, std::move (clipsBefore), Edits::snapshot (engine, { trackId })), true);
        }
        else
        {
            pushTracks ("Couldn't write the video's sound into " + folder.getFullPathName());
        }
    }

    engine.setSongVideo (after);
    addToHistory ("Import Video", new Edits::VideoChanged (engine, before, after), joined);

    videoPrepared = prepared;
    videoPreparedFor = file;
    videoPrepareError.clear();      // (a video before that couldn't be read)
    ++videoThumbnailsVersion;
    pushTracks();
    pushSongVideo (true);
    showVideoWindow (true);
}

//==============================================================================
void MainComponent::updateVideo()
{
    if (videoImport != nullptr)
    {
        if (videoImport->isDone())
        {
            finishVideoImport();
        }
        else if (std::abs (videoImport->getProgress() - videoProgressPushed) >= 0.01)
        {
            videoProgressPushed = videoImport->getProgress();
            pushSongVideo (true);
        }
    }

    if (videoPreparing != nullptr)
    {
        if (videoPreparing->isDone())
        {
            const auto prepared = videoPreparing->getResult();

            if (videoPreparing->getVideo() == engine.getSongVideo().file)
            {
                if (prepared.ok())
                {
                    videoPrepared = prepared;
                    videoPreparedFor = videoPreparing->getVideo();
                    ++videoThumbnailsVersion;
                }
                else
                {
                    // Nothing made for it: tried again once the song's video changes (pushSongVideo),
                    // or, when it went away meanwhile, once it's back (below).
                    videoPreparedFor = juce::File();

                    if (videoPreparing->getVideo().existsAsFile())
                        videoPrepareError = prepared.error;
                }
            }
            else if (videoPreparedFor == videoPreparing->getVideo())
            {
                // Thrown away (the song's video changed meanwhile): made again if it comes back.
                videoPreparedFor = juce::File();
            }

            videoPreparing = nullptr;
            pushSongVideo (true);
        }
        else if (std::abs (videoPreparing->getProgress() - videoProgressPushed) >= 0.01)
        {
            videoProgressPushed = videoPreparing->getProgress();
            pushSongVideo (true);
        }
    }

    // The song's video wasn't there (a drive not plugged in, a file put back later): looked for
    // every two seconds, and got ready once it's back.
    if (const auto& video = engine.getSongVideo(); ! video.isEmpty() && videoPreparedFor != video.file && videoPrepareError.isEmpty()
                                                   && juce::Time::getMillisecondCounter() - videoLookedFor >= 2000)
    {
        videoLookedFor = juce::Time::getMillisecondCounter();

        if (video.file.existsAsFile() && VideoMedia::problem().isEmpty())
            pushSongVideo (true);
    }

    // An undo, a redo or another song: the lane and the window catch up.
    pushSongVideo();
}

double MainComponent::videoEnd() const
{
    const auto& video = engine.getSongVideo();

    if (video.isEmpty() || ! videoPrepared || videoPreparedFor != video.file)
        return video.start;

    const auto& map = engine.getTempoMap();
    return map.ppqAt (map.secondsAt (video.start) + videoPrepared->info.seconds);
}

void MainComponent::pushSongVideo (bool force)
{
    const auto& video = engine.getSongVideo();
    const auto generation = engine.getSongVideoGeneration();

    const auto changed = generation != pushedVideoGeneration;

    if (! force && ! changed)
        return;

    pushedVideoGeneration = generation;

    // What the video's pictures are: from the cache, or made again (a song from elsewhere).
    // videoPreparedFor names the video only once something is made, or being made, for it: one
    // that isn't there (or VLC that isn't) is tried again at the next push (updateVideo() pushes
    // once it's back); one that couldn't be read, once the song's video changes (not at each
    // push, over and over).
    if (! video.isEmpty() && videoPreparedFor != video.file && (videoPrepareError.isEmpty() || changed))
    {
        videoPrepared.reset();
        videoPreparedFor = juce::File();
        videoPrepareError.clear();
        ++videoThumbnailsVersion;

        VideoMedia::Wanted wanted;
        wanted.sound = false;

        if (auto cached = VideoMedia::cached (video.file, wanted))
        {
            videoPrepared = std::move (cached);
            videoPreparedFor = video.file;
        }
        else if (video.file.existsAsFile() && VideoMedia::problem().isEmpty())
        {
            if (videoPreparing == nullptr || videoPreparing->getVideo() != video.file)
                videoPreparing = std::make_unique<VideoMedia::Preparation> (video.file, wanted);

            videoPreparedFor = video.file;
        }
    }

    syncVideoPicture();

    if (webView == nullptr)
        return;

    // (VLC is loaded only once there's a video to show.)
    juce::DynamicObject::Ptr data (new juce::DynamicObject());
    data->setProperty ("problem", video.isEmpty() && videoImport == nullptr ? juce::String() : VideoMedia::problem());
    data->setProperty ("window", videoWindow != nullptr && videoWindow->isVisible());

    if (videoImport != nullptr || (videoPreparing != nullptr && videoPreparing->getVideo() == video.file))
    {
        const auto& job = videoImport != nullptr ? *videoImport : *videoPreparing;
        juce::DynamicObject::Ptr preparing (new juce::DynamicObject());
        preparing->setProperty ("name", job.getVideo().getFileName());
        preparing->setProperty ("progress", job.getProgress());
        preparing->setProperty ("importing", videoImport != nullptr);
        data->setProperty ("preparing", juce::var (preparing.get()));
    }

    if (! video.isEmpty())
    {
        data->setProperty ("file", video.file.getFullPathName());
        data->setProperty ("name", video.file.getFileName());
        data->setProperty ("start", video.start);
        data->setProperty ("end", videoEnd());
        data->setProperty ("missing", ! video.file.existsAsFile());
        data->setProperty ("error", videoPrepareError);

        if (videoPrepared && videoPreparedFor == video.file)
        {
            const auto& info = videoPrepared->info;
            data->setProperty ("seconds", info.seconds);
            data->setProperty ("width", info.width);
            data->setProperty ("height", info.height);

            if (const auto& t = videoPrepared->thumbnails; t.isValid())
            {
                juce::DynamicObject::Ptr thumbs (new juce::DynamicObject());
                thumbs->setProperty ("url", juce::String (thumbnailsPath) + "?v=" + juce::String (videoThumbnailsVersion));
                thumbs->setProperty ("count", t.count);
                thumbs->setProperty ("columns", t.columns);
                thumbs->setProperty ("cellWidth", t.cellWidth);
                thumbs->setProperty ("cellHeight", t.cellHeight);
                thumbs->setProperty ("every", t.every);
                data->setProperty ("thumbnails", juce::var (thumbs.get()));
            }
        }
    }

    webView->emitEventIfBrowserIsVisible ("songVideo", juce::var (data.get()));
}

std::optional<juce::WebBrowserComponent::Resource> MainComponent::getVideoResource()
{
    if (! videoPrepared || videoPreparedFor != engine.getSongVideo().file)
        return std::nullopt;

    juce::MemoryBlock data;

    if (! videoPrepared->thumbnails.file.loadFileAsData (data))
        return std::nullopt;

    const auto* bytes = static_cast<const std::byte*> (data.getData());
    return juce::WebBrowserComponent::Resource { std::vector<std::byte> (bytes, bytes + data.getSize()), "image/jpeg" };
}

//==============================================================================
VideoScreen::Where MainComponent::videoWhere()
{
    const auto& video = engine.getSongVideo();

    // (A mixdown races through the song: the picture rests meanwhile.)
    if (video.isEmpty() || ! videoPrepared || videoPreparedFor != video.file || engine.isMixdownRunning())
        return {};

    // The song's time where the picture is: the transport's, less where the picture starts.
    const auto state = engine.getTransport().getState();
    const auto start = engine.getTempoMap().secondsAt (video.start);
    return { true, state.seconds - start, state.playing, 1.0 };
}

void MainComponent::syncVideoPicture()
{
    const auto& video = engine.getSongVideo();

    if (videoPicture == nullptr)
        videoPicture = std::make_unique<VideoScreen::Picture> ([this] { return videoWhere(); });

    if (video.isEmpty())
    {
        if (videoPicture->isOpen())
            videoPicture->close();

        videoPicture->setMessage ("The song has no video. Song > Import Video adds one.");
        return;
    }

    if (const auto problem = VideoMedia::problem(); problem.isNotEmpty())
        return videoPicture->setMessage (problem);

    if (! video.file.existsAsFile())
    {
        videoPicture->close();
        return videoPicture->setMessage (video.file.getFileName() + " isn't there any more.");
    }

    if (! videoPrepared || videoPreparedFor != video.file)
    {
        videoPicture->close();

        if (videoPrepareError.isNotEmpty())
            return videoPicture->setMessage ("Couldn't read " + video.file.getFileName() + ": " + videoPrepareError);

        return videoPicture->setMessage ("Getting " + video.file.getFileName() + " ready...");
    }

    if (videoPicture->getFile() != video.file && ! videoPicture->open (video.file, videoPrepared->info))
        videoPicture->setMessage ("VLC couldn't play " + video.file.getFileName() + ".");

    if (videoWindow != nullptr)
        videoWindow->setName ("Video - " + video.file.getFileName());
}

void MainComponent::showVideoWindow (bool show)
{
    if (! show)
    {
        if (videoWindow != nullptr)
            videoWindow->setVisible (false);

        return pushSongVideo (true);
    }

    syncVideoPicture();

    if (videoWindow == nullptr)
    {
        videoWindow = std::make_unique<VideoScreen::Window> (*videoPicture, "Video");
        videoWindow->onClose = [this] { pushSongVideo (true); };
    }

    const auto& video = engine.getSongVideo();
    videoWindow->setName (video.isEmpty() ? juce::String ("Video") : "Video - " + video.file.getFileName());
    videoWindow->setVisible (true);
    videoWindow->toFront (true);
    pushSongVideo (true);
}

//==============================================================================
void MainComponent::chooseVideoExport()
{
    const auto& video = engine.getSongVideo();

    if (video.isEmpty())
        return emitVideoExport ("refused", 0.0, "The song has no video: Song > Import Video adds one.");

    if (! videoPrepared || videoPreparedFor != video.file)
        return emitVideoExport ("refused", 0.0, videoPrepareError.isNotEmpty() ? "Couldn't read the video: " + videoPrepareError
                                                : ! video.file.existsAsFile()  ? video.file.getFileName() + " isn't there any more."
                                                                               : juce::String ("The video is still being got ready: try again in a moment."));

    if (choosingFile)
        return;

    auto name = songTitle().isNotEmpty() ? songTitle() : video.file.getFileNameWithoutExtension();
    const auto folder = defaultMixdownFolder();
    folder.createDirectory();
    choosingFile = true;
    songChooser = std::make_unique<juce::FileChooser> ("Export Video", folder.getChildFile (juce::File::createLegalFileName (name) + ".mp4"), "*.mp4");

    songChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        safeThis->choosingFile = false;
        auto target = chooser.getResult();

        if (target == juce::File())
            return;

        if (! target.hasFileExtension (".mp4"))
            target = target.withFileExtension (".mp4");

        safeThis->startVideoExport (target);
    });
}

void MainComponent::startVideoExport (const juce::File& target)
{
    const auto& video = engine.getSongVideo();

    if (video.isEmpty() || ! videoPrepared || videoPreparedFor != video.file)
        return emitVideoExport ("refused", 0.0, "The song has no video ready.");

    if (! video.file.existsAsFile())
        return emitVideoExport ("refused", 0.0, video.file.getFileName() + " isn't there any more.");

    if (videoExportStage != VideoExportStage::idle)
        return emitVideoExport ("refused", 0.0, "A video is being exported already.");

    if (engine.isMixdownRunning() || masteringStage != MasteringStage::idle)
        return emitVideoExport ("refused", 0.0, "Not while a mixdown is being made.");

    if (engine.isRecording())
        return emitVideoExport ("refused", 0.0, "Not while recording.");

    // First the mix, over the picture's length exactly, at 48 kHz (the new file's sound).
    videoExportMix = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("Sonify video mix", ".wav", false);
    videoExportTarget = target;
    videoExportSource = video.file;

    AudioEngine::MixdownSettings settings;
    settings.file = videoExportMix;
    settings.format = AudioEngine::MixdownSettings::Format::wav;
    settings.bits = 32;
    settings.outputRate = 48000.0;
    settings.startPpq = video.start;
    settings.endPpq = videoEnd();
    settings.tailSeconds = 0.0;

    setMasteringListen (false, false);
    const auto result = engine.startMixdown (settings);

    if (result.failed())
        return emitVideoExport ("failed", 0.0, result.getErrorMessage());

    videoExportStage = VideoExportStage::mixing;
    emitVideoExport ("running", 0.0);
}

void MainComponent::updateVideoExport()
{
    if (videoExportStage == VideoExportStage::mixing)
    {
        juce::File written;

        if (const auto result = engine.collectMixdown (written))
        {
            pushStatus();

            if (result->failed())
            {
                videoExportStage = VideoExportStage::idle;
                videoExportMix.deleteFile();
                const auto cancelled = result->getErrorMessage() == "Cancelled";
                return emitVideoExport (cancelled ? "cancelled" : "failed", 0.0, cancelled ? juce::String() : result->getErrorMessage());
            }

            // Then the new file: the picture as it was, the mix as its sound (on a thread of its own).
            videoExportStage = VideoExportStage::writing;
            videoExport = std::make_unique<Mp4::Export> (videoExportSource, videoExportMix, videoExportTarget);
            return emitVideoExport ("running", 0.5);
        }

        return emitVideoExport ("running", 0.5 * engine.getMixdownProgress());
    }

    if (videoExportStage == VideoExportStage::writing && videoExport != nullptr)
    {
        if (! videoExport->isDone())
            return emitVideoExport ("running", 0.5 + 0.5 * videoExport->getProgress());

        const auto result = videoExport->getResult();
        videoExport = nullptr;
        videoExportStage = VideoExportStage::idle;
        videoExportMix.deleteFile();

        if (result.failed())
        {
            // (The new file is written beside it and only then put in its place: a file
            // already there stays as it was.)
            const auto cancelled = result.getErrorMessage() == "Cancelled";
            return emitVideoExport (cancelled ? "cancelled" : "failed", 0.0, cancelled ? juce::String() : result.getErrorMessage());
        }

        emitVideoExport ("done", 1.0);
    }
}

void MainComponent::emitVideoExport (const juce::String& state, double progress, const juce::String& error)
{
    if (webView == nullptr)
        return;

    // Refused before an export began ("refused": no video, not ready, recording...): no file named
    // (videoExportTarget is still the last export's, or the one under way's).
    const auto refused = state == "refused";

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("state", state);
    item->setProperty ("progress", progress);
    item->setProperty ("file", refused ? juce::String() : videoExportTarget.getFullPathName());
    item->setProperty ("name", refused ? juce::String() : videoExportTarget.getFileName());
    item->setProperty ("error", error);
    webView->emitEventIfBrowserIsVisible ("videoExport", juce::var (item.get()));
}
