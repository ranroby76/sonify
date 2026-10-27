// D:\workspace\Fanan_web_hub\src\MainLive.cpp

#include "MainComponent.h"
#include "PageDropZone.h"
#include "VideoSound.h"

//==============================================================================
// The Live Show (LiveShow.h), its own tab: decks, the tracks marked as its channels (a singer's mic,
// a player's instrument, through their plugins), the pads, the master and its effects channel (the
// dynamic EQ that ducks the music under the voices, a compressor, a drive and the limiter). While
// its tab shows (or it still plays: a deck, a pad, the recording, the stream), it plays on Main in
// place of the song, and the song stops; playing with its tab out of sight, the Studio is heard
// with it (a track opened there, monitored or played). It's used three ways, its tabs: DJ show (the
// mixer, the library and the pads), Live & podcasts (the channels too, and a playlist; Mute mics and
// Mute playback, both on as Sonify starts) and Live streaming (the same, streamed: Enable streaming
// lets the picture and the sound out to it; the Controller sets its cameras). Its settings are kept
// with the song (mixing, as the Console's faders: not undo steps); the tracks on its decks, the
// pads' sounds, the tab and the playlist too.
//
// Each track is listened to once for its tempo, first downbeat and key (RecordingAnalysis), or its
// tempo typed: what's known of it, its hot cues and its loops stay in the Live Show's track library
// (liveshow-tracks.json, with Sonify's other settings). Each deck has its tempo (the
// player's BPM): a track following it (Track Sync) plays at it, else as recorded; SYNC makes it the
// song's tempo (the DAW's), and a deck started synced comes in on the beat of another synced one.
//
// The headphones play on a Sub output of the song's (Song Setup's Audio I/O: a DJ controller's
// outputs 3-4, say). The night records (the show, and with stems each deck and channel), and a
// recording is made ready to publish: -16 LUFS for a podcast, -14 for a stream (PodcastExport.h).
// MIDI Link moves its knobs, faders and buttons from a controller.
//
// And it streams live (LiveStream.h): up to four cameras and the screen as the Controller lays them
// out (LiveController.h, MainLiveControl.cpp), the show's title, what's playing and a logo, with
// the show's sound, to YouTube, Facebook, Twitch, TikTok or a server of the user's own, several at
// once, and an MP4 copy if wanted. TikTok (or a server said to be vertical) gets the vertical
// picture, made and encoded beside the landscape one. Its settings (stream keys among them) are the
// setup's, kept next to Sonify's settings, not in songs.
//
// The playlist's tabs have one player: decks 1 and 2 by turns (the playlist's zipper), the deck
// behind only there to crossfade the next track in. The page shows the deck in front (the one
// playing, the newest of two; else the one to play); deck 2 plays through deck 1's channel and the
// crossfader leaves them alone (LiveShow::Mixer::setOnePlayer). Tracks go on the playlist (from the
// library, or dropped in from a folder on the computer), never straight into the player. A track
// that plays to its end, or is stopped (another put in the player, the next one's crossfade paused
// under it), goes back to its start; the player's back-to-start button takes it there too. A video on
// the playlist plays its sound on the deck (read straight from it by FFmpeg, about as fast as an
// audio file: VideoSound.h; the kinds VLC lines up itself are read out by VLC once:
// VideoMedia::Preparation) and its picture on the show's screen (LiveScreen.h): the picture in
// picture on the page, and full screen on the computer's other screens (a projector, a TV on HDMI);
// a track that's only sound shows its name there, on black. The screens chosen are the setup's,
// kept next to Sonify's settings.

namespace
{
    juce::var liveSpecsToVar (const LiveShow::ParamSpec* specs, size_t count)
    {
        juce::Array<juce::var> list;

        for (size_t i = 0; i < count; ++i)
        {
            const auto& spec = specs[i];
            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("id", spec.id);
            item->setProperty ("name", spec.name);
            item->setProperty ("kind", spec.kind == LiveShow::ParamSpec::Kind::toggle ? "toggle"
                                     : spec.kind == LiveShow::ParamSpec::Kind::choice ? "choice"
                                     : spec.kind == LiveShow::ParamSpec::Kind::fader  ? "fader" : "knob");
            item->setProperty ("min", static_cast<double> (spec.minimum));
            item->setProperty ("max", static_cast<double> (spec.maximum));
            item->setProperty ("default", static_cast<double> (spec.defaultValue));
            item->setProperty ("unit", spec.unit);

            if (spec.kind == LiveShow::ParamSpec::Kind::choice)
            {
                juce::Array<juce::var> names;

                for (const auto& name : spec.choiceNames())
                    names.add (name);

                item->setProperty ("choices", names);
            }

            list.add (juce::var (item.get()));
        }

        return list;
    }

    /** A meter as the page draws it: [vu left, vu right (dBFS, RMS), peak left, peak right (since
        the last look, dBFS)]. */
    juce::var liveMeterToVar (LiveShow::Meter& meter)
    {
        const auto round = [] (float value) { return std::round (value * 10.0f) / 10.0f; };
        const auto peakDb = [] (float peak) { return peak > 1.0e-5f ? 20.0f * std::log10 (peak) : -100.0f; };

        return juce::Array<juce::var> { static_cast<double> (round (meter.vuLeft.load())), static_cast<double> (round (meter.vuRight.load())),
                                        static_cast<double> (round (peakDb (meter.peakLeft.exchange (0.0f)))),
                                        static_cast<double> (round (peakDb (meter.peakRight.exchange (0.0f)))) };
    }

    /** A deck, as messages name it: in the playlist's tabs decks 1 and 2 are the one player. */
    juce::String deckName (int deck, const juce::String& mode = "dj")
    {
        return mode != "dj" && deck < 2 ? juce::String ("The player") : "Deck " + juce::String (deck + 1);
    }

    juce::String padName (int pad)
    {
        return "Pad " + juce::String (pad + 1);
    }

    juce::String bpmText (double bpm)
    {
        return juce::String (bpm, std::abs (bpm - std::round (bpm)) < 0.005 ? 0 : 2) + " BPM";
    }

    // A pad is a short sound: a jingle, a sting, an effect.
    constexpr double longestPadMinutes = 10.0;

    // A playlist kept in a file of its own.
    constexpr const char* playlistExtension = ".sonifyplaylist";

    // A tempo typed for a track: within these.
    constexpr double slowestTypedBpm = 40.0, fastestTypedBpm = 300.0;

    /** A tempo as the player's tempo slider holds it (its Tempo, 60 to 200 BPM): halved or doubled
        into it when it's outside (a 210 BPM song: 105). A track follows either the same, at the
        nearer of half, double or as recorded (applyDeckTempo). */
    double playerBpm (double bpm)
    {
        const auto& spec = LiveShow::deckSpecs()[static_cast<size_t> (LiveShow::deckBpm)];
        const auto lowest = static_cast<double> (spec.minimum), highest = static_cast<double> (spec.maximum);

        while (bpm > highest)
            bpm *= 0.5;

        while (bpm > 0.0 && bpm < lowest)
            bpm *= 2.0;

        return juce::jlimit (lowest, highest, bpm);
    }

    /** The tempo range (its choice) for a share of the track's speed: the one it has when that's
        enough, else the smallest that is. */
    int rangeFor (double fraction, int current)
    {
        if (std::abs (fraction) <= static_cast<double> (LiveShow::pitchRangeOf (current)) + 1.0e-9)
            return current;

        for (int range = 0; range < LiveShow::numPitchRanges; ++range)
            if (std::abs (fraction) <= static_cast<double> (LiveShow::pitchRangeOf (range)) + 1.0e-9)
                return range;

        return LiveShow::numPitchRanges - 1;
    }

    /** What's known of a track, for the page. */
    juce::var trackInfoToVar (const LiveShow::TrackInfo& info)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("bpm", info.bpm);
        item->setProperty ("firstBeat", info.firstBeat);
        item->setProperty ("keyTonic", info.keyTonic);
        item->setProperty ("keyMinor", info.keyMinor);
        item->setProperty ("key", LiveShow::keyName (info.keyTonic, info.keyMinor));
        item->setProperty ("byUser", info.bpmByUser);

        juce::Array<juce::var> cues;

        for (const auto cue : info.hotCues)
            cues.add (cue);

        item->setProperty ("cues", cues);

        juce::Array<juce::var> loops;

        for (const auto& loop : info.loops)
            loops.add (juce::Array<juce::var> { loop.start, loop.end });

        item->setProperty ("loops", loops);
        return juce::var (item.get());
    }

    /** What a video on a deck needs read out by VLC (the kinds VideoSound leaves to it): its sound
        as a WAV (silence as long as it, when it has none: its picture still plays), no small pictures. */
    VideoMedia::Wanted deckVideoWanted()
    {
        VideoMedia::Wanted wanted;
        wanted.sound = true;
        wanted.silenceWhenNoSound = true;
        wanted.thumbnails = false;
        return wanted;
    }
}

//==============================================================================
void MainComponent::handleLive (const juce::var& event)
{
    auto& show = engine.getLiveShow();
    const auto action = event.getProperty ("action", {}).toString();
    const auto deckIndex = static_cast<int> (event.getProperty ("deck", -1));
    auto* deck = juce::isPositiveAndBelow (deckIndex, LiveShow::maxDecks) ? &show.decks[static_cast<size_t> (deckIndex)] : nullptr;
    const auto padIndex = static_cast<int> (event.getProperty ("pad", -1));
    const bool isPad = juce::isPositiveAndBelow (padIndex, LiveShow::numPads);

    if (action == "shown")
    {
        liveShown = static_cast<bool> (event.getProperty ("shown", false));
        updateLiveOwnership();

        // Tracks changed while it was hidden: its channels as they are now.
        if (liveShown && std::exchange (liveStale, false))
            pushLive();

        return;
    }

    if (action == "state")
    {
        pushLive();

        // The waveforms the page doesn't have yet (it names the files it has, deck by deck).
        const auto have = event.getProperty ("have", {});

        for (int d = 0; d < LiveShow::maxDecks; ++d)
        {
            const auto track = engine.getDeckTrack (d);
            const auto known = have.isArray() && d < have.size() ? have[d].toString() : juce::String();

            if (track == nullptr ? known.isNotEmpty() : known != track->file.getFullPathName())
                pushDeckWave (d);
        }

        return;
    }

    if (action == "mode")
    {
        setLiveMode (event.getProperty ("mode", "live").toString());
        return;
    }

    // Live streaming: Enable streaming lets the picture and the sound out to the stream; not
    // enabled, the stream shows the show's name (and that it's coming back), in silence.
    if (action == "on")
    {
        setLiveSwitch ("stream", static_cast<bool> (event.getProperty ("on", false)));
        return;
    }

    // Live & podcasts, Live streaming: the mutes (MIDI Link's live.mute: both).
    if (action == "muteMics" || action == "mutePlayback" || action == "mute")
    {
        setLiveSwitch (action == "muteMics" ? "mics" : action == "mutePlayback" ? "playback" : "both",
                       static_cast<bool> (event.getProperty ("on", false)));
        return;
    }

    // The Controller: the stream's cameras and screen, in a window of its own.
    if (action == "controller")
    {
        openLiveController (static_cast<bool> (event.getProperty ("open", false)));
        return;
    }

    // The Monitor: the picture as the viewers see it, in a window over every other (never in the stream).
    if (action == "monitor")
    {
        if (liveControl != nullptr && (liveMode == "stream" || ! static_cast<bool> (event.getProperty ("open", false))))
            liveControl->showMonitor (static_cast<bool> (event.getProperty ("open", false)));

        return;
    }

    if (action == "set")
    {
        const auto target = event.getProperty ("target", {}).toString();
        const auto id = event.getProperty ("id", {}).toString();
        const auto value = static_cast<float> (static_cast<double> (event.getProperty ("value", 0.0)));

        if (! std::isfinite (value))
            return;

        if (target == "master")
        {
            if (const auto index = LiveShow::masterIndexOf (id); index >= 0)
            {
                // Four decks are DJ show's alone.
                if (index == LiveShow::deckCount && liveMode != "dj")
                    return;

                show.settings.set (index, value);

                if (index == LiveShow::deckCount)
                {
                    liveDjDecks = show.settings.get (LiveShow::deckCount);
                    keepLiveExtras();

                    // Two decks again: the two that go out of sight stop.
                    if (show.getDeckCount() == 2)
                        for (int d = 2; d < LiveShow::maxDecks; ++d)
                            show.decks[static_cast<size_t> (d)].setPlaying (false);
                }
            }
        }
        else if (target == "deck" && deck != nullptr)
        {
            // The playlist's one player: its channel is deck 1's (deck 2 plays through it).
            if (const auto index = LiveShow::deckIndexOf (id); index >= 0)
                setLiveDeckValue (liveMode != "dj" && deckIndex < 2 && LiveShow::isChannelParam (index) ? 0 : deckIndex, index, value);
        }
        else if (target == "channel")
        {
            if (auto* track = engine.findTrack (static_cast<int> (event.getProperty ("track", 0))); track != nullptr && ! track->isMixChannel())
                if (const auto index = LiveShow::channelIndexOf (id); index >= 0)
                    track->live.settings.set (index, value);
        }

        // The page shows the control where it put it: nothing goes back.
        markChanged();
        return;
    }

    if (action == "deck" && deck != nullptr)
    {
        handleLiveDeck (deckIndex, event);
        return;
    }

    if (action == "load" && deck != nullptr)
    {
        const auto item = static_cast<int> (event.getProperty ("item", 0));
        livePlayed.erase (item);   // put on a deck by hand: it plays again
        loadDeck (deckIndex, juce::File (event.getProperty ("path", {}).toString()), item);
        return;
    }

    if (action == "eject" && deck != nullptr)
    {
        if (deck->isPlaying())
        {
            pushLive (deckName (deckIndex, liveMode) + " is playing: pause it first.");
            return;
        }

        emptyDeck (deckIndex);
        markChanged();
        pushLive();
        return;
    }

    if (action == "channel")
    {
        const auto trackId = static_cast<int> (event.getProperty ("track", 0));
        const auto on = static_cast<bool> (event.getProperty ("on", false));

        if (auto* track = engine.findTrack (trackId); track != nullptr && ! track->isMixChannel())
        {
            engine.setTrackLive (trackId, on);
            markChanged();

            const auto hint = on && track->kind == Track::Kind::audio && track->inputLeft.load() < 0
                                  ? track->name + " has no input to hear: choose one for it in the Studio (its input, in the Inspector)."
                                  : juce::String();
            pushLive (hint);
        }

        return;
    }

    if (action == "pad" && isPad)
    {
        if (engine.getPadSound (padIndex) == nullptr)
        {
            pushLive (padName (padIndex) + " has no sound: drag one from the library onto it.");
            return;
        }

        // An export: the show can't come on (kept, the press would play once it's done, by itself).
        if (engine.isMixdownRunning())
        {
            pushLive ("Not while an export is running: play the pads once it's done.");
            return;
        }

        // A press waiting counts as playing: the show comes on for it (its tab out of sight too).
        show.pads[static_cast<size_t> (padIndex)].toggle();
        updateLiveOwnership();
        return;   // whether it plays comes with the meters
    }

    if (action == "padLoad" && isPad)
    {
        loadPad (padIndex, juce::File (event.getProperty ("path", {}).toString()));
        return;
    }

    if (action == "padClear" && isPad)
    {
        ++padLoadToken[static_cast<size_t> (padIndex)];
        padLoading[static_cast<size_t> (padIndex)].clear();
        engine.setPadSound (padIndex, nullptr);
        markChanged();
        pushLive();
        return;
    }

    if (action == "phones")
    {
        engine.setLivePhonesOutput (static_cast<int> (event.getProperty ("output", 0)));
        markChanged();
        pushLive();
        return;
    }

    if (action == "record")
    {
        recordLive (static_cast<bool> (event.getProperty ("on", false)), static_cast<bool> (event.getProperty ("stems", liveRecordStems)));
        return;
    }

    if (action == "podcast")
    {
        const auto lufs = juce::jlimit (-24.0, -9.0, static_cast<double> (event.getProperty ("lufs", -16.0)));

        if (! static_cast<bool> (event.getProperty ("choose", false)))
        {
            startPodcast (lastLiveTake, lufs);
            return;
        }

        // A recording of the user's choosing (one from another night).
        auto folder = lastLiveTake.existsAsFile() ? lastLiveTake.getParentDirectory() : UserData::recordings();
        podcastChooser = std::make_unique<juce::FileChooser> ("A recording to make ready to publish", folder, "*.wav;*.flac;*.aif;*.aiff;*.mp3;*.ogg");
        podcastChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                     [safeThis = juce::Component::SafePointer<MainComponent> (this), lufs] (const juce::FileChooser& chooser)
        {
            if (safeThis != nullptr && chooser.getResult() != juce::File())
                safeThis->startPodcast (chooser.getResult(), lufs);
        });

        return;
    }

    if (action == "reveal")
    {
        if (const juce::File file (event.getProperty ("path", {}).toString()); file.exists())
            file.revealToUser();

        return;
    }

    if (action.startsWith ("stream"))
    {
        handleLiveStream (action, event);
        return;
    }

    if (action.startsWith ("playlist"))
    {
        handlePlaylist (action, event);
        return;
    }

    // The show's screen: the picture in picture on the page, the other screens.
    if (action.startsWith ("screen"))
    {
        handleLiveScreen (action, event);
        return;
    }

    // Files dragged in from a folder on the computer, over the playlist.
    if (action == "fileDrag")
    {
        showDropZone (event);
        return;
    }

    // The effects window shows the stereo width's picture (the goniometer): its points come with
    // the status while it does.
    if (action == "effectsWatch")
    {
        liveWidthWatched = static_cast<bool> (event.getProperty ("width", false));
        return;
    }

    // An effect's controls back where they start (its switch stays as it is).
    if (action == "resetEffect")
    {
        const auto name = event.getProperty ("effect", {}).toString();

        for (const auto& effect : LiveShow::effectsChannel)
            if (name == effect.name)
                for (int index = effect.first; index <= effect.last; ++index)
                    show.settings.set (index, LiveShow::masterSpecs()[static_cast<size_t> (index)].defaultValue);

        markChanged();
        pushLive();
        return;
    }

    if (action == "list")
    {
        // Nowhere named (the studio first looked at, each time Sonify starts): the folder Browse
        // chose, until another is chosen.
        auto path = event.getProperty ("path", {}).toString();

        if (path.isEmpty())
            path = keptLibraryFolder();

        pushLiveListing (path);
        return;
    }

    // Browse: a folder for the library, kept for every start from now on (until another is chosen).
    if (action == "browse")
    {
        const juce::File from (liveListingPath);
        libraryChooser = std::make_unique<juce::FileChooser> ("The folder of the tracks for the show", from.isDirectory() ? from : juce::File::getSpecialLocation (juce::File::userMusicDirectory));
        libraryChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                     [safeThis = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& chooser)
        {
            const auto folder = chooser.getResult();

            if (safeThis == nullptr || ! folder.isDirectory())
                return;

            safeThis->pushLiveListing (folder.getFullPathName());

            if (! keepLibraryFolder (folder))
                safeThis->pushLive ("Couldn't keep the library's folder in " + libraryFolderFile().getParentDirectory().getFullPathName()
                                    + ": the library opens there only until Sonify closes.");
        });

        return;
    }

    if (action == "analyze")
        analyseLiveFolder (juce::File (event.getProperty ("path", {}).toString()));
}

void MainComponent::handleLiveDeck (int deckIndex, const juce::var& event)
{
    auto& show = engine.getLiveShow();
    auto& deck = show.decks[static_cast<size_t> (deckIndex)];
    const auto place = static_cast<size_t> (deckIndex);
    auto& info = deckInfo[place];
    const auto what = event.getProperty ("do", {}).toString();
    const auto track = engine.getDeckTrack (deckIndex);
    const auto starting = what == "play" || (what == "toggle" && ! deck.isPlaying());

    // The playlist's tabs: a track on its way into the player (a video's sound being read out
    // first, say) plays once it's there, not the one it takes the place of.
    if (starting && ! deck.isPlaying() && deckLoading[place].isNotEmpty() && (liveMode != "dj" || track == nullptr))
    {
        pushLive (deckLoading[place] + " is still being read: it can play in a moment.");
        return;
    }

    if ((what == "play" || what == "toggle") && track == nullptr)
    {
        pushLive (liveMode == "dj" ? "Load a track on " + deckName (deckIndex) + " first: drag one onto it from the library, or double-click it there."
                                   : juce::String ("Nothing in the player yet: put tracks on the playlist, then press one's play button (or Auto plays them in turn)."));
        return;
    }

    if (starting)
    {
        // Synced: it comes in on the beat of another synced deck playing.
        if (! deck.isPlaying() && deck.settings.isOn (LiveShow::deckSync) && deck.settings.isOn (LiveShow::deckTrackSync))
            if (const auto lead = liveLeadDeck (deckIndex); lead >= 0)
                alignDeckBeat (deckIndex, lead);

        // Started by hand: at full (a playlist's fade let go), and it may lead on to the next again.
        deck.fade (1.0f, 0.0);
        deckEndedAt[place] = 0.0;
        deckHandedOver[place] = false;
        deckFinished[place] = false;
        deck.setPlaying (true);
        updateLiveOwnership();
        return;
    }

    if (what == "pause" || what == "toggle")
    {
        deck.setPlaying (false);

        // The playlist's one player: the last track, still fading out under it, stops with it.
        if (liveMode != "dj" && deckIndex < 2)
        {
            const auto other = static_cast<size_t> (1 - deckIndex);

            if (show.decks[other].isPlaying() && deckHandedOver[other])
            {
                show.decks[other].setPlaying (false);
                show.decks[other].seek (0.0);   // stopped: back at its start
            }
        }

        return;
    }

    // Back to its start (the player's button): playing, it plays on from there.
    if (what == "rewind")
    {
        deck.fade (1.0f, 0.03);   // moved by hand: a fade under way is let go
        deck.seek (0.0);
        return;
    }

    if (what == "seek")
    {
        deck.fade (1.0f, 0.03);   // moved by hand: a fade under way is let go
        deck.seek (static_cast<double> (event.getProperty ("seconds", 0.0)));
        return;
    }

    // The rest needs a track.
    if (track == nullptr)
    {
        pushLive (liveMode == "dj" ? deckName (deckIndex) + " has no track: load one first."
                                   : juce::String ("Nothing in the player: press a track's play button on the playlist first."));
        return;
    }

    if (what == "trackBpm")
    {
        const auto bpm = static_cast<double> (event.getProperty ("bpm", 0.0));

        if (! std::isfinite (bpm) || bpm < slowestTypedBpm || bpm > fastestTypedBpm)
        {
            pushLive ("A track's tempo is between 40 and 300 BPM.");
            return;
        }

        // Listening still under way finds its key and first beat, and leaves the tempo typed.
        info.bpm = std::round (bpm * 100.0) / 100.0;
        info.bpmByUser = true;
        deck.setBeat (info.bpm, info.firstBeat);
        keepDeckInfo (deckIndex);
        applyDeckTempo (deckIndex);
        pushLive();
        return;
    }

    if (what == "listen")
    {
        info.bpmByUser = false;   // what's heard is its tempo now
        listenToDeck (deckIndex);
        pushLive ((liveMode == "dj" ? deckName (deckIndex) + ": listening" : juce::String ("Listening")) + " to " + track->title + " again for its tempo and key.");
        return;
    }

    const auto index = static_cast<int> (event.getProperty ("index", -1));

    // A hot cue: there at once.
    if (what == "hotcue")
    {
        if (juce::isPositiveAndBelow (index, LiveShow::numHotCues) && info.hotCues[static_cast<size_t> (index)] >= 0.0)
            deck.seek (info.hotCues[static_cast<size_t> (index)]);

        return;
    }

    // A hot cue's marker moved.
    if (what == "cueAt")
    {
        const auto seconds = static_cast<double> (event.getProperty ("seconds", 0.0));

        if (juce::isPositiveAndBelow (index, LiveShow::numHotCues) && std::isfinite (seconds))
        {
            info.hotCues[static_cast<size_t> (index)] = juce::jlimit (0.0, track->lengthSeconds, seconds);
            keepDeckInfo (deckIndex);
            pushLive();
        }

        return;
    }

    if (! juce::isPositiveAndBelow (index, LiveShow::numLoops))
        return;

    auto& loop = info.loops[static_cast<size_t> (index)];

    // A loop's markers, where it plays from: four beats (two seconds while its tempo isn't known).
    const auto placeLoop = [&]
    {
        const auto start = juce::jlimit (0.0, juce::jmax (0.0, track->lengthSeconds - 0.5), deck.getPosition());
        const auto length = info.bpm > 0.0 ? 4.0 * 60.0 / info.bpm : 2.0;
        loop.start = start;
        loop.end = juce::jmin (track->lengthSeconds, start + length);
    };

    // Its first button: the loop on (its markers placed), or off.
    if (what == "loopOn")
    {
        if (static_cast<bool> (event.getProperty ("on", false)))
        {
            if (! loop.isOn())
                placeLoop();
        }
        else
        {
            loop = {};

            if (deckLoop[place] == index)
            {
                deckLoop[place] = -1;
                deck.setLoop (-1.0, -1.0);
            }
        }

        keepDeckInfo (deckIndex);
        pushLive();
        return;
    }

    // Its markers moved: a loop playing follows them.
    if (what == "loopAt")
    {
        const auto start = static_cast<double> (event.getProperty ("start", -1.0));
        const auto end = static_cast<double> (event.getProperty ("end", -1.0));

        if (std::isfinite (start) && std::isfinite (end) && end - start >= 0.05)
        {
            loop.start = juce::jlimit (0.0, track->lengthSeconds, start);
            loop.end = juce::jlimit (loop.start + 0.05, track->lengthSeconds, end);

            if (deckLoop[place] == index)
                deck.setLoop (loop.start, loop.end);

            keepDeckInfo (deckIndex);
            pushLive();
        }

        return;
    }

    // Its second button: at its start at once, and round it (playing) until it's switched off.
    if (what == "loopPlay")
    {
        if (static_cast<bool> (event.getProperty ("on", false)))
        {
            if (! loop.isOn())
            {
                placeLoop();
                keepDeckInfo (deckIndex);
            }

            deckLoop[place] = index;
            deck.setLoop (loop.start, loop.end);
            deck.seek (loop.start);
        }
        else if (deckLoop[place] == index)
        {
            deckLoop[place] = -1;
            deck.setLoop (-1.0, -1.0);
        }

        pushLive();
    }
}

//==============================================================================
double MainComponent::songTempo()
{
    return juce::jlimit (40.0, 300.0, engine.getTransport().getState().bpm);
}

void MainComponent::applyDeckTempo (int d)
{
    auto& deck = engine.getLiveShow().decks[static_cast<size_t> (d)];
    const auto own = deckInfo[static_cast<size_t> (d)].bpm;
    const bool hasTrack = engine.getDeckTrack (d) != nullptr;
    double fraction = 0.0;

    if (hasTrack && own > 0.0)
    {
        if (deck.settings.isOn (LiveShow::deckTrackSync))
        {
            // The player's tempo: at half or double when that's nearer the track's own (a 70 BPM
            // ballad with 128 BPM tracks plays at 64: two of their beats to one of its).
            const auto bpm = static_cast<double> (deck.settings.get (LiveShow::deckBpm));
            auto ratio = bpm / own;

            while (ratio > juce::MathConstants<double>::sqrt2 + 1.0e-9)
                ratio *= 0.5;

            while (ratio < 1.0 / juce::MathConstants<double>::sqrt2 - 1.0e-9)
                ratio *= 2.0;

            const auto widest = static_cast<double> (LiveShow::pitchRangeOf (LiveShow::numPitchRanges - 1));
            fraction = juce::jlimit (-widest, widest, ratio - 1.0);
        }
        else if (! deck.settings.isOn (LiveShow::deckSync))
        {
            // As recorded: the player's tempo shows the track's own (halved or doubled onto the slider).
            deck.settings.set (LiveShow::deckBpm, static_cast<float> (playerBpm (std::round (own * 100.0) / 100.0)));
        }
    }

    // The range and the slider's place in it, together (the audio thread never has one without the other).
    const auto range = rangeFor (fraction, juce::roundToInt (deck.settings.get (LiveShow::deckRange)));
    deck.setSpeed (range, static_cast<float> (fraction / static_cast<double> (LiveShow::pitchRangeOf (range))));
    liveValuesDue = true;
}

void MainComponent::followSong (int d)
{
    // SYNC: the player's tempo is the song's (halved or doubled onto the slider: the track plays the
    // same), and its track follows it.
    auto& deck = engine.getLiveShow().decks[static_cast<size_t> (d)];
    liveSongTempo = songTempo();
    deck.settings.set (LiveShow::deckSync, 1.0f);
    deck.settings.set (LiveShow::deckBpm, static_cast<float> (playerBpm (liveSongTempo)));
    deck.settings.set (LiveShow::deckTrackSync, 1.0f);
    applyDeckTempo (d);
}

void MainComponent::setDeckSynced (int d, bool synced)
{
    auto& show = engine.getLiveShow();
    auto& deck = show.decks[static_cast<size_t> (d)];

    if (! synced)
    {
        deck.settings.set (LiveShow::deckSync, 0.0f);
        deck.alignTo (-1);
        return;   // it stays at the tempo it has
    }

    followSong (d);

    if (auto* item = findPlaylistItem (deckItem[static_cast<size_t> (d)]); item != nullptr && item->getDynamicObject() != nullptr)
    {
        item->getDynamicObject()->setProperty ("sync", true);
        keepLiveExtras();
    }

    // Playing: its beats onto another synced deck's now.
    if (deck.isPlaying())
        if (const auto lead = liveLeadDeck (d); lead >= 0)
            alignDeckBeat (d, lead);

    if (deckInfo[static_cast<size_t> (d)].bpm <= 0.0 && engine.getDeckTrack (d) != nullptr)
        pushLive ((liveMode == "dj" ? deckName (d) + "'s track" : engine.getDeckTrack (d)->title) + " has no tempo yet: it follows the song's once it's known (or typed).");
    else
        pushLive();
}

void MainComponent::setLiveDeckValue (int d, int index, float value)
{
    const auto place = static_cast<size_t> (d);
    auto& deck = engine.getLiveShow().decks[place];
    const bool wasOn = deck.settings.isOn (index);
    deck.settings.set (index, value);

    // In the playlist's tabs a playlist's track has one Sync: the deck's SYNC and its item's Sync
    // to player are the same switch (on: the song's tempo; off: as recorded).
    auto* item = findPlaylistItem (deckItem[place]);
    auto* fields = item != nullptr ? item->getDynamicObject() : nullptr;
    const bool linked = fields != nullptr && liveMode != "dj";
    const auto itemBefore = fields != nullptr ? juce::JSON::toString (*item, true) : juce::String();
    bool pushed = false;

    if (index == LiveShow::deckSync)
    {
        if (deck.settings.isOn (index) && ! wasOn)
        {
            setDeckSynced (d, true);
            pushed = true;
        }
        else if (! deck.settings.isOn (index) && wasOn)
        {
            deck.alignTo (-1);   // a beat waiting to be lined up isn't

            if (linked)
            {
                deck.settings.set (LiveShow::deckTrackSync, 0.0f);
                applyDeckTempo (d);
            }
        }
    }
    else if (index == LiveShow::deckBpm)
    {
        // Its tempo moved by hand: its track follows it, and the song's no longer leads.
        if (deck.settings.isOn (LiveShow::deckSync) && std::abs (static_cast<double> (value) - playerBpm (songTempo())) > 0.01)
            deck.settings.set (LiveShow::deckSync, 0.0f);

        if (! deck.settings.isOn (LiveShow::deckTrackSync))
            deck.settings.set (LiveShow::deckTrackSync, 1.0f);

        applyDeckTempo (d);
    }
    else if (index == LiveShow::deckTrackSync)
    {
        // Played as recorded: the song's tempo doesn't lead it either.
        if (! deck.settings.isOn (index))
            deck.settings.set (LiveShow::deckSync, 0.0f);

        applyDeckTempo (d);
    }

    // The playlist's item on the deck takes its options as they change (its banner shows them).
    if (fields != nullptr)
    {
        if (index == LiveShow::deckKey)
            fields->setProperty ("key", juce::roundToInt (deck.settings.get (index)));
        else if (index == LiveShow::deckTrackVolume)
            fields->setProperty ("volume", static_cast<double> (deck.settings.get (index)));
        else if (index == LiveShow::deckSync || index == LiveShow::deckTrackSync || index == LiveShow::deckBpm)
            fields->setProperty ("sync", deck.settings.isOn (linked ? LiveShow::deckSync : LiveShow::deckTrackSync));

        if (juce::JSON::toString (*item, true) != itemBefore)
        {
            keepLiveExtras();
            liveStateDue = liveStateDue || ! pushed;   // shown with the next status (a knob turning sends no whole state each move)
        }
    }

    liveValuesDue = true;   // what followed (Track Sync, the tempo) shows on the page
}

void MainComponent::followSongTempo()
{
    const auto tempo = songTempo();

    if (std::abs (tempo - liveSongTempo) < 1.0e-6)
        return;

    liveSongTempo = tempo;
    auto& show = engine.getLiveShow();

    for (int d = 0; d < LiveShow::maxDecks; ++d)
    {
        auto& deck = show.decks[static_cast<size_t> (d)];

        if (! deck.settings.isOn (LiveShow::deckSync))
            continue;

        deck.settings.set (LiveShow::deckBpm, static_cast<float> (playerBpm (tempo)));
        applyDeckTempo (d);
    }
}

int MainComponent::liveLeadDeck (int except) const
{
    const auto& show = engine.getLiveShow();
    int best = -1;
    float loudest = -1000.0f;

    for (int d = 0; d < show.getDeckCount(); ++d)
    {
        const auto& player = show.decks[static_cast<size_t> (d)];

        if (d == except || ! player.isPlaying() || deckInfo[static_cast<size_t> (d)].bpm <= 0.0 || engine.getDeckTrack (d) == nullptr
            || ! player.settings.isOn (LiveShow::deckSync) || ! player.settings.isOn (LiveShow::deckTrackSync))
            continue;

        // The loudest of them in the room: its fader highest.
        if (const auto fader = player.settings.get (LiveShow::deckVolume); fader > loudest)
        {
            loudest = fader;
            best = d;
        }
    }

    return best;
}

void MainComponent::alignDeckBeat (int d, int lead)
{
    // The audio thread lines it up in the block it starts in (or the next, playing), from where
    // both decks are then: not from where they were when this was asked.
    if (deckInfo[static_cast<size_t> (d)].bpm > 0.0 && deckInfo[static_cast<size_t> (lead)].bpm > 0.0)
        engine.getLiveShow().decks[static_cast<size_t> (d)].alignTo (lead);
}

//==============================================================================
void MainComponent::setLiveMode (const juce::String& mode)
{
    // DJ show isn't offered (its tab is hidden): anything but Live streaming is Live & podcasts.
    const auto next = mode == "stream" ? mode : juce::String ("live");

    if (next == liveMode)
        return;

    // A stream going out stays where it's run from.
    if (liveMode == "stream" && liveVideo != nullptr && liveVideo->isStreaming())
    {
        pushLive ("The stream is live: end it first (Stream), then change how the show is used.");
        return;
    }

    liveMode = next;

    // The Controller and the Monitor are Live streaming's.
    if (liveMode != "stream" && liveControl != nullptr)
    {
        if (liveControl->isWindowShown())
            openLiveController (false);

        liveControl->showMonitor (false);
    }

    applyLiveDecks();
    applyLiveMute();
    updateLiveScreen();   // the playlist's tabs: the player's deck, the screen on (DJ show: off)
    keepLiveExtras();
    pushLive (liveMode == "stream" && ! liveOnAir ? "Live streaming: the stream gets the show's name until you press Enable streaming." : juce::String());
}

void MainComponent::setLiveSwitch (const juce::String& which, bool on)
{
    if (which == "stream")
    {
        liveOnAir = on;
        applyLiveMute();
        pushLive (on ? "Streaming enabled: the picture and the sound go out to the stream."
                     : "Streaming not enabled: the stream shows the show's name, and that it's coming back.");
    }
    else
    {
        if (which == "mics" || which == "both")
            liveMicsMuted = on;

        if (which == "playback" || which == "both")
            livePlaybackMuted = on;

        applyLiveMute();
        pushLive (liveMicsMuted && livePlaybackMuted ? "Mics and playback muted: nothing is heard."
                  : liveMicsMuted                    ? "Mics muted."
                  : livePlaybackMuted                ? "Playback muted."
                                                     : juce::String());
    }

    if (liveControl != nullptr)
        liveControl->refresh();
}

void MainComponent::applyLiveDecks()
{
    auto& show = engine.getLiveShow();

    // DJ show: its own two or four, as it was left, each on its own channel and the crossfader.
    if (liveMode == "dj")
    {
        show.setOnePlayer (false);
        show.settings.set (LiveShow::deckCount, liveDjDecks);
        return;
    }

    // The playlist plays the decks by turns: two of them, one player (deck 2 through deck 1's
    // channel, no crossfader). Decks 3 and 4 stop.
    show.setOnePlayer (true);
    show.settings.set (LiveShow::deckCount, 0.0f);

    for (int d = 2; d < LiveShow::maxDecks; ++d)
        show.decks[static_cast<size_t> (d)].setPlaying (false);
}

void MainComponent::applyLiveMute()
{
    // The mutes are the playlist's tabs' (DJ show has none); the stream hears the show only while
    // streaming is enabled (the room always does).
    auto& show = engine.getLiveShow();
    const bool mutes = liveMode == "live" || liveMode == "stream";
    const bool streaming = liveMode == "stream" && liveOnAir;
    show.setMicsMuted (mutes && liveMicsMuted);
    show.setPlaybackMuted (mutes && livePlaybackMuted);
    show.setStreamOpen (streaming);

    if (liveVideo != nullptr)
        liveVideo->setHolding (! streaming);
}

bool MainComponent::liveShowPlaying()
{
    auto& show = engine.getLiveShow();
    bool playing = engine.isLiveRecording() || (liveVideo != nullptr && liveVideo->isStreaming());

    for (const auto& player : show.decks)
        playing = playing || player.isPlaying();

    // A pad pressed counts at once (only the audio thread starts it: the show is to come on for it).
    for (const auto& pad : show.pads)
        playing = playing || pad.isPlaying() || pad.isStarting();

    // The playlist going on by itself, between two tracks: still the show (its channels stay live,
    // and the next one comes, whichever studio is in front).
    return playing || playlistBetweenTracks();
}

void MainComponent::updateLiveOwnership()
{
    // An export renders the song while the device plays silence: never in the middle of a show.
    const bool want = (liveShown || liveShowPlaying()) && ! engine.isMixdownRunning();

    if (want != engine.isLiveShowOn())
        engine.setLiveShowOn (want);

    // The show can't play now (an export): a pad's press waiting is let go, never played later by itself.
    if (! want)
        for (auto& pad : engine.getLiveShow().pads)
            pad.cancel();

    // Playing with its tab out of sight (the Studio in front, say), the Studio is heard with it:
    // a track opened there sounds (its input monitored, an instrument played); the song waits.
    engine.setStudioWithShow (! liveShown);
}

void MainComponent::keepLiveExtras()
{
    juce::DynamicObject::Ptr extras (new juce::DynamicObject());
    extras->setProperty ("mode", liveMode);
    extras->setProperty ("djDecks", static_cast<double> (liveDjDecks));   // DJ show's decks
    extras->setProperty ("playlist", juce::var (livePlaylist));
    engine.keepLiveShowExtras (juce::var (extras.get()));
    markChanged();
}

void MainComponent::placeDefaultCues (int d)
{
    const auto track = engine.getDeckTrack (d);

    if (track == nullptr)
        return;

    // At the start, by their order, a little apart: then dragged where they belong.
    auto& info = deckInfo[static_cast<size_t> (d)];
    const auto gap = juce::jlimit (0.5, 6.0, track->lengthSeconds * 0.015);

    for (int i = 0; i < LiveShow::numHotCues; ++i)
        if (info.hotCues[static_cast<size_t> (i)] < 0.0)
            info.hotCues[static_cast<size_t> (i)] = juce::jmin (track->lengthSeconds, i * gap);
}

//==============================================================================
// The playlist: tracks in order, each with its tempo (the track's own, or the player's: Sync, which
// in the playlist's tabs is its deck's SYNC too: the song's tempo), its key, its volume and its
// timing; played onto a deck (dragged there), it takes them along. Kept in a file of its own with
// its tracks' hot cues and loops.

juce::var* MainComponent::findPlaylistItem (int id)
{
    if (id <= 0)
        return nullptr;

    for (auto& item : livePlaylist)
        if (static_cast<int> (item.getProperty ("id", 0)) == id)
            return &item;

    return nullptr;
}

juce::var MainComponent::playlistToVar()
{
    juce::Array<juce::var> items;

    for (const auto& item : livePlaylist)
    {
        auto copy = juce::JSON::parse (juce::JSON::toString (item));
        auto* fields = copy.getDynamicObject();

        if (fields == nullptr)
            continue;

        // What's known of its track: its tempo and key.
        const juce::File file (item.getProperty ("path", {}).toString());

        if (const auto known = liveTracks().find (file))
        {
            fields->setProperty ("bpm", known->bpm);
            fields->setProperty ("keyName", LiveShow::keyName (known->keyTonic, known->keyMinor));
            fields->setProperty ("keyTonic", known->keyTonic);
            fields->setProperty ("keyMinor", known->keyMinor);
        }

        fields->setProperty ("missing", ! file.existsAsFile());
        fields->setProperty ("video", VideoMedia::isVideoFile (file));
        fields->setProperty ("played", livePlayed.count (static_cast<int> (item.getProperty ("id", 0))) > 0);
        items.add (copy);
    }

    return items;
}

void MainComponent::applyItemToDeck (int d)
{
    auto& deck = engine.getLiveShow().decks[static_cast<size_t> (d)];
    const auto* item = findPlaylistItem (deckItem[static_cast<size_t> (d)]);

    if (item == nullptr)
        return;

    const bool follows = static_cast<bool> (item->getProperty ("sync", false));
    deck.settings.set (LiveShow::deckKey, static_cast<float> (juce::jlimit (-12, 12, static_cast<int> (item->getProperty ("key", 0)))));
    deck.settings.set (LiveShow::deckTrackVolume, static_cast<float> (juce::jlimit (0.0, 1.0, static_cast<double> (item->getProperty ("volume", 0.5)))));

    // The playlist's tabs: Sync is the deck's SYNC too, the song's tempo (a deck playing comes in
    // on the beat of another synced one); off, the track plays as recorded.
    if (liveMode != "dj")
    {
        if (follows)
        {
            const bool was = deck.settings.isOn (LiveShow::deckSync) && deck.settings.isOn (LiveShow::deckTrackSync);
            followSong (d);

            if (! was && deck.isPlaying())
                if (const auto lead = liveLeadDeck (d); lead >= 0)
                    alignDeckBeat (d, lead);
        }
        else
        {
            deck.settings.set (LiveShow::deckSync, 0.0f);
            deck.settings.set (LiveShow::deckTrackSync, 0.0f);
            deck.alignTo (-1);
            applyDeckTempo (d);
        }

        liveValuesDue = true;
        return;
    }

    // DJ show: Sync is the deck's Slider sync (its tempo slider); played as recorded, the song's
    // tempo leads it no longer.
    deck.settings.set (LiveShow::deckTrackSync, follows ? 1.0f : 0.0f);

    if (! follows)
        deck.settings.set (LiveShow::deckSync, 0.0f);

    applyDeckTempo (d);
}

void MainComponent::handlePlaylist (const juce::String& action, const juce::var& event)
{
    const auto id = static_cast<int> (event.getProperty ("id", 0));

    if (action == "playlistAdd")
    {
        // Tracks dropped on it (or added from the library), at a place or at its end.
        auto at = static_cast<int> (event.getProperty ("at", -1));
        std::vector<juce::File> unknown;
        int added = 0;

        if (const auto* paths = event.getProperty ("paths", {}).getArray())
        {
            for (const auto& path : *paths)
            {
                const juce::File file (path.toString());

                // Its sound, or a video (its sound on the player, its picture on the screen).
                if (! file.existsAsFile() || ! (engine.getMedia().isAudioFile (file) || VideoMedia::isVideoFile (file)))
                    continue;

                juce::DynamicObject::Ptr item (new juce::DynamicObject());
                item->setProperty ("id", livePlaylistNext++);
                item->setProperty ("path", file.getFullPathName());
                item->setProperty ("title", file.getFileNameWithoutExtension());
                item->setProperty ("sync", false);
                item->setProperty ("key", 0);
                item->setProperty ("volume", 0.5);
                item->setProperty ("timing", 0.0);

                if (juce::isPositiveAndBelow (at, livePlaylist.size() + 1))
                    livePlaylist.insert (at++, juce::var (item.get()));
                else
                    livePlaylist.add (juce::var (item.get()));

                if (const auto known = liveTracks().find (file); ! known.has_value() || ! known->heard)
                    unknown.push_back (file);

                ++added;
            }
        }

        if (added == 0)
        {
            pushLive ("Only audio and video files go on the playlist.");
            return;
        }

        keepLiveExtras();
        pushLive();

        if (! unknown.empty())
            analyseLiveFiles (unknown, "the playlist");

        return;
    }

    const auto place = [this] (int itemId)
    {
        for (int i = 0; i < livePlaylist.size(); ++i)
            if (static_cast<int> (livePlaylist.getReference (i).getProperty ("id", 0)) == itemId)
                return i;

        return -1;
    };

    if (action == "playlistMove")
    {
        const auto from = place (id);
        const auto to = static_cast<int> (event.getProperty ("to", -1));

        if (from >= 0 && juce::isPositiveAndBelow (to, livePlaylist.size()))
        {
            livePlaylist.move (from, to);
            keepLiveExtras();
            pushLive();
        }

        return;
    }

    if (action == "playlistAuto")
    {
        livePlaylistAuto = static_cast<bool> (event.getProperty ("on", true));
        pushLive (livePlaylistAuto ? "The playlist plays itself: each track after the one before." : juce::String());
        return;
    }

    // Kept in a file of its own, every item's options with it (and its track's markers); and opened again.
    if (action == "playlistSave" || action == "playlistOpen")
    {
        const bool saving = action == "playlistSave";

        if (saving && livePlaylist.isEmpty())
        {
            pushLive ("The playlist is empty: nothing to keep.");
            return;
        }

        choosePlaylistFile (saving);
        return;
    }

    if (action == "playlistRemove" || action == "playlistClear")
    {
        int before = 0;   // the item before the one removed (0: it was the first)

        if (action == "playlistClear")
        {
            livePlaylist.clear();
        }
        else if (const auto at = place (id); at >= 0)
        {
            before = at > 0 ? static_cast<int> (livePlaylist.getReference (at - 1).getProperty ("id", 0)) : 0;
            livePlaylist.remove (at);
        }

        // The playlist's tabs: the list emptied empties the player (one playing fades out). A track
        // that's gone off it is never played: a deck holding it, or bringing it, is emptied (the next
        // comes in its place), unless it's playing (it plays on, the list going on after it).
        if (liveMode != "dj")
        {
            if (livePlaylist.isEmpty())
                clearPlayer();
            else
                dropGoneFromPlayer (id, before);
        }

        // DJ show's decks keep what they have, as tracks of their own; the first two, waiting, as
        // an old list's (the playlist's tabs put the next in their place, Auto never plays them).
        for (int d = 0; d < LiveShow::maxDecks; ++d)
        {
            auto& item = deckItem[static_cast<size_t> (d)];

            if (item > 0 && findPlaylistItem (item) == nullptr)
                item = d < LiveScreen::numDecks && ! engine.getLiveShow().decks[static_cast<size_t> (d)].isPlaying() ? -1 : 0;
        }

        keepLiveExtras();
        pushLive();
        return;
    }

    auto* item = findPlaylistItem (id);

    if (item == nullptr || item->getDynamicObject() == nullptr)
        return;

    // Into the player (its play button, or a double-click): there, ready (Play starts it). What
    // plays stops, back at its start and not played (to be chosen again, or for the playlist to come
    // to): the user has jumped, nothing plays on under it. The deck behind then holds what comes
    // after the one chosen: the playlist goes on from it. An item is never on both decks.
    if (action == "playlistCue")
    {
        if (liveMode == "dj")
            return;

        auto& show = engine.getLiveShow();
        const auto title = item->getProperty ("title", {}).toString();

        // The one playing (or coming in), chosen: it plays on.
        for (int d = 0; d < 2; ++d)
        {
            if (show.decks[static_cast<size_t> (d)].isPlaying() && deckItem[static_cast<size_t> (d)] == id)
            {
                pushLive (title + " is playing now.");
                return;
            }
        }

        // Another plays (two: the next crossfading in): it stops, back at its start.
        juce::String stopped;

        for (int d = 0; d < 2; ++d)
        {
            const auto at = static_cast<size_t> (d);

            if (! show.decks[at].isPlaying())
                continue;

            if (const auto track = engine.getDeckTrack (d); track != nullptr && (stopped.isEmpty() || d == liveFront))
                stopped = track->title;

            show.decks[at].setPlaying (false);
            show.decks[at].seek (0.0);
            deckHandedOver[at] = false;
            deckStarted[at] = false;
            deckFinished[at] = false;
            deckEndedAt[at] = 0.0;

            if (deckItem[at] > 0)
                livePlayed.erase (deckItem[at]);
        }

        const auto deck = liveFront;
        const auto other = 1 - deck;
        const auto slot = static_cast<size_t> (deck), otherSlot = static_cast<size_t> (other);
        livePlayed.erase (id);   // chosen by hand: it plays again

        // In the player: the deck behind is to hold what comes after it. The one it takes the place
        // of, left part-way (paused), goes back to its start.
        const auto chosen = [this, &show, &stopped, &title] (int d)
        {
            const auto behind = static_cast<size_t> (1 - d);

            if (! show.decks[behind].isPlaying() && deckStarted[behind] && ! liveDeckDone (1 - d))
            {
                show.decks[behind].seek (0.0);
                deckStarted[behind] = false;
                deckEndedAt[behind] = 0.0;
            }

            liveCued = d;
            liveFront = d;
            deckStale[static_cast<size_t> (d)] = false;
            deckStale[static_cast<size_t> (1 - d)] = true;
            pushLive (stopped.isNotEmpty() ? stopped + " stopped (back at its start): " + title + " is in the player. Play starts it." : juce::String());
        };

        // On a deck already, loaded: it stays there, the user's (from its start, when it had played,
        // to its end or part-way).
        const auto waiting = [this, id] (int d)
        {
            const auto at = static_cast<size_t> (d);
            return deckItem[at] == id && engine.getDeckTrack (d) != nullptr && deckLoading[at].isEmpty() && deckPreparing[at] == nullptr;
        };

        const auto takeIt = [this, &show] (int d)
        {
            const auto at = static_cast<size_t> (d);

            // Played (to its end, or part-way and paused): from its start again.
            if (deckStarted[at] || liveDeckDone (d))
            {
                show.decks[at].fade (1.0f, 0.0);
                show.decks[at].seek (0.0);
                deckHandedOver[at] = false;
                deckStarted[at] = false;
                deckFinished[at] = false;
                deckEndedAt[at] = 0.0;
            }

            deckByUser[at] = true;
        };

        // Waiting on the deck behind (or on its way there): that one comes in front, the player's.
        const bool comingThere = (deckLoading[otherSlot].isNotEmpty() || deckPreparing[otherSlot] != nullptr) && deckLoadItem[otherSlot] == id;

        if (waiting (other) || comingThere)
        {
            if (comingThere)
                deckLoadByUser[otherSlot] = true;
            else
                takeIt (other);

            deckByUser[slot] = false;
            chosen (other);
            return;
        }

        if (waiting (deck))
        {
            takeIt (deck);
            chosen (deck);
            return;
        }

        // On its way onto that deck already (its play button pressed twice): it comes there, the user's.
        if ((deckLoading[slot].isNotEmpty() || deckPreparing[slot] != nullptr) && deckLoadItem[slot] == id)
        {
            deckLoadByUser[slot] = true;
            chosen (deck);
            return;
        }

        loadDeck (deck, juce::File (item->getProperty ("path", {}).toString()), id, true);
        chosen (deck);
        return;
    }

    // Its options: Sync (the player's tempo, or its own), Key and Volume; on a deck, at once.
    if (action == "playlistSet")
    {
        const auto field = event.getProperty ("field", {}).toString();
        const auto value = static_cast<double> (event.getProperty ("value", 0.0));

        if (! std::isfinite (value))
            return;

        auto* fields = item->getDynamicObject();

        if (field == "sync")
            fields->setProperty ("sync", value >= 0.5);
        else if (field == "key")
            fields->setProperty ("key", juce::jlimit (-12, 12, juce::roundToInt (value)));
        else if (field == "volume")
            fields->setProperty ("volume", juce::jlimit (0.0, 1.0, value));
        else if (field == "timing")
            fields->setProperty ("timing", juce::jlimit (-60.0, 60.0, std::round (value * 10.0) / 10.0));
        else
            return;

        for (int d = 0; d < LiveShow::maxDecks; ++d)
            if (deckItem[static_cast<size_t> (d)] == id)
                applyItemToDeck (d);

        keepLiveExtras();
        pushLive();
        return;
    }

    // Its track's own tempo, as the user knows it (for every deck and list that has the track).
    if (action == "playlistBpm")
    {
        const auto bpm = static_cast<double> (event.getProperty ("bpm", 0.0));

        if (! std::isfinite (bpm) || bpm < slowestTypedBpm || bpm > fastestTypedBpm)
        {
            pushLive ("A track's tempo is between 40 and 300 BPM.");
            return;
        }

        const juce::File file (item->getProperty ("path", {}).toString());
        auto info = liveTracks().find (file).value_or (LiveShow::TrackInfo {});
        info.bpm = std::round (bpm * 100.0) / 100.0;
        info.bpmByUser = true;
        liveTracks().set (file, info);
        saveLiveTracksSoon();

        for (int d = 0; d < LiveShow::maxDecks; ++d)
        {
            if (const auto track = engine.getDeckTrack (d); track != nullptr && track->file == file)
            {
                deckInfo[static_cast<size_t> (d)].bpm = info.bpm;
                deckInfo[static_cast<size_t> (d)].bpmByUser = true;
                engine.getLiveShow().decks[static_cast<size_t> (d)].setBeat (info.bpm, deckInfo[static_cast<size_t> (d)].firstBeat);
                applyDeckTempo (d);
            }
        }

        pushLive();
    }
}

int MainComponent::nextUnplayedAfter (int itemId)
{
    bool past = itemId == 0;

    for (const auto& item : livePlaylist)
    {
        const auto id = static_cast<int> (item.getProperty ("id", 0));

        if (! past)
        {
            past = id == itemId;
            continue;
        }

        if (livePlayed.count (id) == 0 && juce::File (item.getProperty ("path", {}).toString()).existsAsFile())
            return id;
    }

    return 0;
}

int MainComponent::playlistIndexOf (int itemId) const
{
    for (int i = 0; i < livePlaylist.size(); ++i)
        if (static_cast<int> (livePlaylist.getReference (i).getProperty ("id", 0)) == itemId)
            return i;

    return -1;
}

int MainComponent::listPlaceOf (int d)
{
    const auto place = static_cast<size_t> (d);

    if (findPlaylistItem (deckItem[place]) != nullptr)
        return deckItem[place];

    // Its item went off the list while it played: the list goes on after the one it came after.
    if (deckItem[place] == 0 && engine.getDeckTrack (d) != nullptr && findPlaylistItem (deckAfter[place]) != nullptr)
        return deckAfter[place];

    return 0;
}

void MainComponent::emptyDeck (int d, bool keepLoad)
{
    if (! juce::isPositiveAndBelow (d, LiveShow::maxDecks))
        return;

    const auto place = static_cast<size_t> (d);

    // A load under way is forgotten (and a video being read out for it), unless it's kept: its
    // track comes in place of none.
    if (! keepLoad)
    {
        ++deckLoadToken[place];
        deckPreparing[place] = nullptr;
        deckLoading[place].clear();

        if (liveCued == d)
            liveCued = -1;
    }

    ++deckListenToken[place];           // the listening is forgotten
    deckVideo[place] = {};
    deckStale[place] = false;
    deckError[place].clear();
    deckListening[place] = false;
    deckInfo[place] = {};
    deckItem[place] = 0;
    deckAfter[place] = 0;
    deckLoop[place] = -1;
    deckStarted[place] = false;
    deckHandedOver[place] = false;
    deckFinished[place] = false;
    deckEndedAt[place] = 0.0;
    engine.getLiveShow().decks[place].setLoop (-1.0, -1.0);
    engine.setDeckTrack (d, nullptr);
    syncScreenDeck (d);
    pushDeckWave (d);
}

void MainComponent::clearPlayer()
{
    auto& show = engine.getLiveShow();
    std::array<const LiveShow::DeckTrack*, LiveScreen::numDecks> had {};
    std::array<int, LiveScreen::numDecks> tokens {};
    bool fading = false;

    for (int d = 0; d < LiveScreen::numDecks; ++d)
    {
        const auto place = static_cast<size_t> (d);

        // Nothing on its way comes now.
        tokens[place] = ++deckLoadToken[place];
        deckPreparing[place] = nullptr;
        deckLoading[place].clear();
        had[place] = engine.getDeckTrack (d).get();

        if (show.decks[place].isPlaying())
        {
            show.decks[place].setPlaying (false);
            fading = true;
        }
    }

    liveCued = -1;

    // One playing fades out first (a few milliseconds, on the audio thread), then goes: each deck
    // that still has what it had (not started again meanwhile, nor another track put on its way there).
    const auto empty = [this, had, tokens]
    {
        for (int d = 0; d < LiveScreen::numDecks; ++d)
        {
            const auto place = static_cast<size_t> (d);

            if (! engine.getLiveShow().decks[place].isPlaying() && engine.getDeckTrack (d).get() == had[place]
                && deckLoadToken[place] == tokens[place])
                emptyDeck (d);
        }
    };

    if (! fading)
    {
        empty();
        return;
    }

    juce::Timer::callAfterDelay (150, [safeThis = juce::Component::SafePointer<MainComponent> (this), empty]
    {
        if (safeThis == nullptr)
            return;

        empty();
        safeThis->markChanged();
        safeThis->pushLive();
    });
}

void MainComponent::dropGoneFromPlayer (int gone, int before)
{
    auto& show = engine.getLiveShow();

    for (int d = 0; d < LiveScreen::numDecks; ++d)
    {
        const auto place = static_cast<size_t> (d);
        const bool holding = deckItem[place] > 0 && findPlaylistItem (deckItem[place]) == nullptr;
        const bool bringing = (deckLoading[place].isNotEmpty() || deckPreparing[place] != nullptr)
                              && deckLoadItem[place] > 0 && findPlaylistItem (deckLoadItem[place]) == nullptr;

        // A track of its own whose list place was the one gone: the list goes on after the one before it.
        if (deckItem[place] == 0 && deckAfter[place] == gone)
            deckAfter[place] = before;

        if (show.decks[place].isPlaying())
        {
            // It plays on, as a track of its own; the list goes on after where it was.
            if (holding)
            {
                deckItem[place] = 0;
                deckAfter[place] = before;
            }

            if (bringing)
            {
                ++deckLoadToken[place];
                deckPreparing[place] = nullptr;
                deckLoading[place].clear();
            }

            continue;
        }

        // Waiting on it, or on its way there: never played (the next comes in its place). One
        // holding it while another item (still on the list) is on its way there: its track goes,
        // the one on its way still comes (the user's choice, or the zipper's).
        const bool bringingAnother = (deckLoading[place].isNotEmpty() || deckPreparing[place] != nullptr) && ! bringing;

        if (holding || bringing)
            emptyDeck (d, holding && bringingAnother);
    }
}

void MainComponent::updatePlaylist()
{
    if ((liveMode != "live" && liveMode != "stream") || ! engine.isLiveShowOn())
        return;

    auto& show = engine.getLiveShow();
    const auto now = juce::Time::getMillisecondCounterHiRes();
    bool changed = false;

    const auto atEnd = [this, &show] (int d)
    {
        const auto track = engine.getDeckTrack (d);
        return track != nullptr && show.decks[static_cast<size_t> (d)].getPosition() >= track->lengthSeconds - 0.05;
    };

    // Done: it led on to the next, or it played to its end (back at its start since).
    const auto done = [this, &show, &atEnd] (int d)
    {
        const auto place = static_cast<size_t> (d);
        return deckHandedOver[place] || deckFinished[place] || (! show.decks[place].isPlaying() && atEnd (d));
    };

    // Each deck: started (its track has played since it came), and an item played to its end
    // counts as played (its banner dims; the list goes on past it). Played to its end, it goes
    // back to its start (done all the same).
    for (int d = 0; d < 2; ++d)
    {
        const auto place = static_cast<size_t> (d);

        if (show.decks[place].isPlaying() && engine.getDeckTrack (d) != nullptr)
            deckStarted[place] = true;

        if (deckStarted[place] && ! deckFinished[place] && ! show.decks[place].isPlaying() && atEnd (d))
        {
            deckFinished[place] = true;
            show.decks[place].seek (0.0);
            changed = true;
        }

        if (deckStarted[place] && deckItem[place] > 0 && done (d) && livePlayed.insert (deckItem[place]).second)
            changed = true;
    }

    if (! livePlaylistAuto || livePlaylist.isEmpty())
    {
        if (changed)
            pushLive();

        return;
    }

    // Ready to come in: a track loaded on a deck that isn't playing, not done (an item still to
    // play, or a track the user put there).
    const auto ready = [this, &show, &done] (int d)
    {
        const auto place = static_cast<size_t> (d);
        return deckLoading[place].isEmpty() && engine.getDeckTrack (d) != nullptr && ! show.decks[place].isPlaying() && ! done (d)
               && ! deckStale[place]
               && (deckItem[place] == 0 || (deckItem[place] > 0 && livePlayed.count (deckItem[place]) == 0));
    };

    // The next one as each ends: by the ending one's timing.
    for (int a = 0; a < 2; ++a)
    {
        const auto b = 1 - a;
        const auto pa = static_cast<size_t> (a), pb = static_cast<size_t> (b);
        auto& ending = show.decks[pa];
        auto& coming = show.decks[pb];
        const auto track = engine.getDeckTrack (a);

        if (track == nullptr || deckItem[pa] < 0 || ! deckStarted[pa] || deckHandedOver[pa] || ! ready (b))
            continue;

        const auto* item = findPlaylistItem (deckItem[pa]);
        const auto timing = item != nullptr ? static_cast<double> (item->getProperty ("timing", 0.0)) : 0.0;
        const auto position = ending.getPosition();
        const bool ended = deckFinished[pa] || position >= track->lengthSeconds - 0.05;

        const auto start = [&] (double fadeSeconds)
        {
            // In on the beat of the one ending, both synced.
            if (coming.settings.isOn (LiveShow::deckSync) && coming.settings.isOn (LiveShow::deckTrackSync))
                if (const auto lead = liveLeadDeck (b); lead >= 0)
                    alignDeckBeat (b, lead);

            if (fadeSeconds > 0.0)
                coming.fade (1.0f, fadeSeconds, 0.0f);
            else
                coming.fade (1.0f, 0.0);

            deckEndedAt[pb] = 0.0;
            coming.setPlaying (true);
            deckStarted[pb] = true;
            deckHandedOver[pa] = true;

            if (deckItem[pa] > 0)
                livePlayed.insert (deckItem[pa]);

            changed = true;
        };

        if (ending.isPlaying() && ! ended)
        {
            deckEndedAt[pa] = 0.0;

            // Below 0: the crossfade, this one out and the next in, over what's left of it.
            if (timing < 0.0)
            {
                const auto remaining = (track->lengthSeconds - position) / juce::jmax (0.05, ending.getSpeed());

                if (remaining <= -timing)
                {
                    ending.fade (0.0f, remaining);
                    start (remaining);
                }
            }

            continue;
        }

        // Paused by hand before its end: it waits.
        if (! ended)
            continue;

        // At its end: the next straight away, or after the pause its timing asks for.
        if (deckEndedAt[pa] <= 0.0)
            deckEndedAt[pa] = now;

        if (now - deckEndedAt[pa] >= juce::jmax (0.0, timing) * 1000.0)
            start (0.0);
    }

    // The decks filled by turns (the zipper): a deck that isn't playing holds what comes next, the
    // first item not played after the other deck's.
    for (int d = 0; d < 2; ++d)
    {
        const auto place = static_cast<size_t> (d), other = static_cast<size_t> (1 - d);

        if (show.decks[place].isPlaying() || deckLoading[place].isNotEmpty())
            continue;

        // Played to its end, the next not started yet (it waits out the pause, or is on its way): it
        // stays as it is until the next one starts (it hands over to it then).
        if (engine.getDeckTrack (d) != nullptr && deckStarted[place] && ! deckHandedOver[place] && done (d)
            && (ready (1 - d) || deckLoading[other].isNotEmpty() || deckPreparing[other] != nullptr))
            continue;

        if (engine.getDeckTrack (d) != nullptr && deckItem[place] >= 0 && ! done (d))
        {
            // It stays (an item to come, or the user's own track) unless the list has gone past
            // it: an item before the one playing now that's been started, or that the zipper put
            // there (one the user put there still plays next); or the user has put another in the
            // player since (stale: what comes after that one is due here).
            const auto playing = show.decks[other].isPlaying() ? deckItem[other] : 0;
            const bool behind = playing > 0 && deckItem[place] > 0 && (deckStarted[place] || ! deckByUser[place])
                                && playlistIndexOf (deckItem[place]) < playlistIndexOf (playing);

            if (! behind && ! deckStale[place])
                continue;
        }

        // (A track of its own whose item went off the list as it played: the list goes on after where it was.)
        const auto otherItem = deckLoading[other].isNotEmpty() ? deckLoadItem[other] : deckItem[other];
        auto next = nextUnplayedAfter (findPlaylistItem (otherItem) != nullptr ? otherItem
                                                                                : deckLoading[other].isEmpty() ? listPlaceOf (1 - d) : 0);

        if (next != 0 && next == otherItem)
            next = nextUnplayedAfter (next);

        if (next == deckItem[place] && engine.getDeckTrack (d) != nullptr && ! done (d))
        {
            deckStale[place] = false;   // it's what comes next after all
            continue;
        }

        if (const auto* item = findPlaylistItem (next))
        {
            deckStale[place] = false;
            loadDeck (d, juce::File (item->getProperty ("path", {}).toString()), next, false);
        }
    }

    if (changed)
        pushLive();
}

//==============================================================================
// The playlist's one player (decks 1 and 2 by turns) and the show's screen.

bool MainComponent::liveDeckDone (int d)
{
    // It led on to the next, or it played to its end (back at its start since), or it stands there.
    const auto place = static_cast<size_t> (d);
    const auto& deck = engine.getLiveShow().decks[place];
    const auto track = engine.getDeckTrack (d);
    const bool atEnd = track != nullptr && deck.getPosition() >= track->lengthSeconds - 0.05;
    return deckHandedOver[place] || deckFinished[place] || (! deck.isPlaying() && atEnd);
}

bool MainComponent::playlistBetweenTracks()
{
    if ((liveMode != "live" && liveMode != "stream") || ! livePlaylistAuto || livePlaylist.isEmpty())
        return false;

    for (int d = 0; d < 2; ++d)
    {
        const auto place = static_cast<size_t> (d), other = static_cast<size_t> (1 - d);

        // One that has played to its end, not yet led on to the next...
        if (! deckStarted[place] || deckHandedOver[place] || ! liveDeckDone (d))
            continue;

        // ...and the next: waiting on the other deck, on its way there, or still to come.
        const bool waiting = engine.getDeckTrack (1 - d) != nullptr && ! deckStarted[other] && ! liveDeckDone (1 - d) && ! deckStale[other];
        const bool coming = deckLoading[other].isNotEmpty() || deckPreparing[other] != nullptr;

        if (waiting || coming || nextUnplayedAfter (listPlaceOf (d)) != 0)
            return true;
    }

    return false;
}

int MainComponent::liveFrontDeck()
{
    auto& show = engine.getLiveShow();
    const auto newer = [this] { return deckStartOrder[1] > deckStartOrder[0] ? 1 : 0; };
    const bool firstPlays = show.decks[0].isPlaying(), secondPlays = show.decks[1].isPlaying();

    // Playing: that one; both (the next crossfading in): the newer.
    if (firstPlays && secondPlays)
        return newer();

    if (firstPlays != secondPlays)
        return firstPlays ? 0 : 1;

    const auto hasTrack = [this] (int d) { return engine.getDeckTrack (d) != nullptr || deckLoading[static_cast<size_t> (d)].isNotEmpty(); };

    // Neither plays. The one the user put in the player (double-clicked on the playlist) stays there.
    if (juce::isPositiveAndBelow (liveCued, LiveScreen::numDecks) && hasTrack (liveCued))
        return liveCued;

    // Paused part-way: it stays in front (the newer of two). One the user has put another in the
    // player in place of (stale: due to hold what comes after that one) never comes back by itself.
    const auto paused = [&] (int d) { return engine.getDeckTrack (d) != nullptr && deckStarted[static_cast<size_t> (d)] && ! liveDeckDone (d)
                                             && ! deckStale[static_cast<size_t> (d)]; };
    const bool firstPaused = paused (0), secondPaused = paused (1);

    if (firstPaused != secondPaused)
        return firstPaused ? 0 : 1;

    if (firstPaused)
        return newer();

    // Else the one to play next (put there by the user first, then the earlier on the playlist).
    const auto ready = [&] (int d) { return hasTrack (d) && ! deckStarted[static_cast<size_t> (d)] && ! liveDeckDone (d)
                                            && ! deckStale[static_cast<size_t> (d)]; };
    const bool firstReady = ready (0), secondReady = ready (1);

    if (firstReady != secondReady)
        return firstReady ? 0 : 1;

    if (firstReady)
    {
        if (deckByUser[0] != deckByUser[1])
            return deckByUser[0] ? 0 : 1;

        const auto first = playlistIndexOf (deckItem[0]), second = playlistIndexOf (deckItem[1]);

        if (first >= 0 && second >= 0 && first != second)
            return first < second ? 0 : 1;

        return liveFront;
    }

    // Nothing to come: the one that played last (at its end), else as it was.
    if (hasTrack (0) != hasTrack (1))
        return hasTrack (0) ? 0 : 1;

    if (deckStartOrder[0] != deckStartOrder[1])
        return newer();

    return liveFront;
}

LiveScreen::Screen& MainComponent::liveScreenNow()
{
    if (liveScreen == nullptr)
    {
        liveScreen = std::make_unique<LiveScreen::Screen> (*this, [this] (int d)
        {
            const auto place = static_cast<size_t> (d);
            const auto& deck = engine.getLiveShow().decks[place];
            const bool loaded = engine.getDeckTrack (d) != nullptr;

            // The next to play: behind the player, not started, not done, not on its way out.
            const bool coming = loaded && d != liveFront && ! deck.isPlaying() && ! deckStarted[place] && ! liveDeckDone (d) && ! deckStale[place];
            return LiveScreen::DeckState { loaded, deck.getPosition(), deck.isPlaying(), deck.getSpeed(), deck.getFadeLevel(), coming };
        });

        // The picture in picture: closed by its own button (the user's choice, kept), moved or
        // sized (kept), and its HDMI button (the screens' menu, there).
        liveScreen->onPipClosed = [this]
        {
            livePipOpen = livePipKept = false;
            saveScreenSettings();
            updateLiveScreen();
            pushLive();
        };

        liveScreen->onPipMoved = [this] { saveScreenSettings(); };
        liveScreen->onPipOutputs = [this] (juce::Rectangle<int> area) { showPipOutputs (area); };

        // The screens chosen last time (this computer's), open once the show has been looked at;
        // and the picture in picture as the user left it (where, how big, and open or not).
        const auto kept = juce::JSON::parse (screenSettingsFile());
        liveScreenOutputs.clear();

        if (const auto* outputs = kept.getProperty ("outputs", {}).getArray())
            for (const auto& id : *outputs)
                liveScreenOutputs.addIfNotAlreadyThere (id.toString());

        if (const auto pip = kept.getProperty ("pip", {}); pip.isObject())
        {
            LiveScreen::Screen::PipPlace place;
            const auto number = [&pip] (const char* name) { return static_cast<int> (pip.getProperty (name, 0)); };

            if (number ("width") > 0 && number ("height") > 0)
                place.area = { number ("x"), number ("y"), number ("width"), number ("height") };

            // (Maximised is for the moment it was asked for: it comes back at its own size.)
            place.mode = pip.getProperty ("mode", "normal").toString() == "min" ? juce::String ("min") : juce::String ("normal");
            liveScreen->setPipPlace (place);
            livePipOpen = livePipKept = static_cast<bool> (pip.getProperty ("open", false));
        }

        for (int d = 0; d < LiveScreen::numDecks; ++d)
            syncScreenDeck (d);
    }

    return *liveScreen;
}

void MainComponent::saveScreenSettings()
{
    juce::Array<juce::var> ids;

    for (const auto& kept : liveScreenOutputs)
        ids.add (kept);

    juce::DynamicObject::Ptr settings (new juce::DynamicObject());
    settings->setProperty ("outputs", ids);

    if (liveScreen != nullptr)
    {
        const auto& place = liveScreen->getPipPlace();
        juce::DynamicObject::Ptr pip (new juce::DynamicObject());
        pip->setProperty ("open", livePipKept);
        pip->setProperty ("x", place.area.getX());
        pip->setProperty ("y", place.area.getY());
        pip->setProperty ("width", place.area.getWidth());
        pip->setProperty ("height", place.area.getHeight());
        pip->setProperty ("mode", place.mode);
        settings->setProperty ("pip", juce::var (pip.get()));
    }

    screenSettingsFile().replaceWithText (juce::JSON::toString (juce::var (settings.get())));
}

void MainComponent::setLiveScreenOutput (const juce::String& id, bool on)
{
    if (id.isEmpty() || liveMode == "dj")
        return;

    if (on)
        liveScreenOutputs.addIfNotAlreadyThere (id);
    else
        liveScreenOutputs.removeString (id);

    liveScreenArmed = true;
    auto& screen = liveScreenNow();
    screen.setActive (true);
    screen.setOutput (id, on);
    saveScreenSettings();

    juce::String name;

    for (const auto& display : screen.getDisplays())
        if (display.id == id)
            name = display.name;

    pushLive (on ? "The show's screen is full screen on " + (name.isNotEmpty() ? name : juce::String ("the other screen")) + "." : juce::String());
}

void MainComponent::showPipOutputs (juce::Rectangle<int> area)
{
    if (liveMode == "dj")
        return;

    // As the page's HDMI menu: the computer's other screens, each ticked that has the picture.
    const auto displays = liveScreenNow().getDisplays();
    juce::PopupMenu menu;
    menu.addSectionHeader ("The show's screen, full screen on");

    if (displays.empty())
        menu.addItem (-1, "No other screen found: connect a projector or a TV", false);

    for (size_t i = 0; i < displays.size(); ++i)
        menu.addItem (static_cast<int> (i) + 1, displays[i].name, true, liveScreenOutputs.contains (displays[i].id));

    menu.addSeparator();
    menu.addItem (1000, "Look for screens again");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (area),
                        [safeThis = juce::Component::SafePointer<MainComponent> (this), displays] (int chosen)
    {
        if (safeThis == nullptr || chosen == 0)
            return;

        if (chosen == 1000)
        {
            safeThis->pushLive();
            return;
        }

        if (juce::isPositiveAndBelow (chosen - 1, static_cast<int> (displays.size())))
        {
            const auto& display = displays[static_cast<size_t> (chosen - 1)];
            safeThis->setLiveScreenOutput (display.id, ! safeThis->liveScreenOutputs.contains (display.id));
        }
    });
}

juce::File MainComponent::screenSettingsFile()
{
    // The setup's, as the stream's are: next to Sonify's settings (the screens are this computer's).
    return SettingsFolder::file ("liveshow-screen.json", "FananWebHub-liveshow-screen.json");
}

void MainComponent::syncScreenDeck (int d)
{
    if (! juce::isPositiveAndBelow (d, LiveScreen::numDecks) || liveScreen == nullptr)
        return;

    const auto track = engine.getDeckTrack (d);
    const auto& video = deckVideo[static_cast<size_t> (d)];

    if (track == nullptr)
        liveScreen->clearDeck (d);
    else
        liveScreen->setTrack (d, video.video, video.info, track->title);
}

void MainComponent::updateLiveScreen()
{
    auto& show = engine.getLiveShow();
    const bool playlistTabs = liveMode != "dj";

    // VLC (the screen's pictures) starts on a thread of its own once the playlist's tabs are looked
    // at: its first start looks over all its plugins, and neither the page nor the first video waits.
    if (playlistTabs && liveShown && ! videoStarted)
    {
        videoStarted = true;
        videoStarting = true;

        liveListener.addJob ([this]
        {
            VideoMedia::problem();
            videoStarting = false;
        });
    }

    // Which deck started last: of two playing, the player shows the newer (the next, crossfading in).
    for (int d = 0; d < LiveScreen::numDecks; ++d)
    {
        const auto place = static_cast<size_t> (d);
        const auto playing = show.decks[place].isPlaying();

        if (playing && ! deckWasPlaying[place])
        {
            deckStartOrder[place] = ++liveStartCounter;
            liveCued = -1;   // the user's choice is playing: the player follows what plays again
        }

        deckWasPlaying[place] = playing;
    }

    // Videos being read out for the decks: how far, and onto the deck once done.
    for (int d = 0; d < LiveShow::maxDecks; ++d)
    {
        const auto place = static_cast<size_t> (d);

        if (deckPreparing[place] == nullptr)
            continue;

        if (deckPreparing[place]->isDone())
        {
            finishDeckVideo (d);
        }
        else if (std::abs (deckPreparing[place]->getProgress() - deckProgressShown[place]) >= 0.05)
        {
            deckProgressShown[place] = deckPreparing[place]->getProgress();
            liveStateDue = true;
        }
    }

    if (playlistTabs)
    {
        if (const auto front = liveFrontDeck(); front != liveFront)
        {
            liveFront = front;
            liveStateDue = true;   // the page shows the other deck now
        }
    }

    // The screen: made once the playlist's tabs are used.
    if (liveScreen == nullptr && ! playlistTabs)
        return;

    auto& screen = liveScreenNow();
    screen.setActive (playlistTabs);
    screen.setFront (liveFront);

    // The other screens chosen last time: once the show has been looked at (not as Sonify starts).
    if (liveShown && ! liveScreenArmed)
    {
        liveScreenArmed = true;

        for (const auto& id : liveScreenOutputs)
            screen.setOutput (id, true);
    }

    // A video coming into the player opens the picture in picture (as Playlisted's window does).
    // Closed by hand, it stays closed for that video, and opens again for the next one; an opening
    // for a video isn't the user's choice (not kept), and never comes maximised by itself.
    if (playlistTabs)
    {
        const auto inFront = static_cast<size_t> (liveFront);
        const auto& video = deckVideo[inFront].video;

        if (engine.getDeckTrack (liveFront) != nullptr && video != juce::File() && deckLoading[inFront].isEmpty() && video != livePipVideo)
        {
            livePipVideo = video;

            if (! livePipOpen)
            {
                livePipOpen = true;
                liveStateDue = true;

                if (screen.getPipPlace().mode == "max")
                    screen.setPipPlace ({ screen.getPipPlace().area, "normal" });
            }
        }
    }

    // It shows while the show is on (its tab in front, or it plays: Sonify minimised, another app
    // in front, another studio in front).
    screen.setPipShown (livePipOpen && playlistTabs && engine.isLiveShowOn());
}

juce::var MainComponent::screenToVar()
{
    juce::DynamicObject::Ptr screen (new juce::DynamicObject());
    juce::Array<juce::var> displays;

    // The computer's other screens (not the one Sonify is on), and which have the picture.
    if (liveMode != "dj")
    {
        for (const auto& display : liveScreenNow().getDisplays())
        {
            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("id", display.id);
            item->setProperty ("name", display.name);
            item->setProperty ("on", liveScreenOutputs.contains (display.id));
            displays.add (juce::var (item.get()));
        }
    }

    screen->setProperty ("displays", displays);
    screen->setProperty ("outputs", liveScreen != nullptr ? liveScreen->getOutputCount() : 0);
    screen->setProperty ("pip", livePipOpen);
    return juce::var (screen.get());
}

void MainComponent::handleLiveScreen (const juce::String& action, const juce::var& event)
{
    // The picture in picture open or closed (the page's Screen button: the user's choice, kept;
    // or opened for videos added to the playlist: not kept, and never maximised by itself).
    if (action == "screen")
    {
        if (liveMode == "dj")
            return;

        auto& screen = liveScreenNow();     // (what was kept is in first)
        const auto open = static_cast<bool> (event.getProperty ("open", false));
        const auto kept = static_cast<bool> (event.getProperty ("kept", true));

        if (open && ! kept && ! livePipOpen && screen.getPipPlace().mode == "max")
            screen.setPipPlace ({ screen.getPipPlace().area, "normal" });

        livePipOpen = open;

        if (kept)
        {
            livePipKept = open;
            saveScreenSettings();
        }

        updateLiveScreen();
        pushLive();
        return;
    }

    // Full screen on one of the computer's other screens, or not.
    if (action == "screenOutput")
    {
        setLiveScreenOutput (event.getProperty ("id", {}).toString(), static_cast<bool> (event.getProperty ("on", false)));
        return;
    }

    // The computer's screens looked at again (one plugged in).
    if (action == "screenDisplays")
        pushLive();
}

void MainComponent::showDropZone (const juce::var& event)
{
    if (liveMode == "dj" || webView == nullptr)
        return;

    const juce::Rectangle<int> area (juce::roundToInt (static_cast<double> (event.getProperty ("left", 0.0))),
                                     juce::roundToInt (static_cast<double> (event.getProperty ("top", 0.0))),
                                     juce::roundToInt (static_cast<double> (event.getProperty ("width", 0.0))),
                                     juce::roundToInt (static_cast<double> (event.getProperty ("height", 0.0))));

    if (area.getWidth() < 40 || area.getHeight() < 40)
        return;

    if (liveDropZone == nullptr)
    {
        auto zone = std::make_unique<PageDropZone> ("Drop to add to the playlist");

        // Audio files and videos, and folders of them.
        zone->accepts = [this] (const juce::File& file)
        {
            return file.isDirectory() || engine.getMedia().isAudioFile (file) || VideoMedia::isVideoFile (file);
        };

        zone->onDrop = [this] (const juce::StringArray& dropped, juce::Point<int> onPage)
        {
            juce::StringArray paths;

            for (const auto& path : dropped)
            {
                const juce::File file (path);

                if (! file.isDirectory())
                {
                    if (engine.getMedia().isAudioFile (file) || VideoMedia::isVideoFile (file))
                        paths.add (file.getFullPathName());

                    continue;
                }

                // A folder: its tracks and videos, by name.
                std::vector<juce::File> inside;

                for (const auto& entry : juce::RangedDirectoryIterator (file, false, "*", juce::File::findFiles | juce::File::ignoreHiddenFiles))
                    if (engine.getMedia().isAudioFile (entry.getFile()) || VideoMedia::isVideoFile (entry.getFile()))
                        inside.push_back (entry.getFile());

                std::sort (inside.begin(), inside.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });

                for (const auto& found : inside)
                    paths.add (found.getFullPathName());
            }

            if (paths.isEmpty())
            {
                pushLive ("Only audio and video files go on the playlist.");
                return;
            }

            // The page knows where in the list that is.
            if (webView != nullptr)
            {
                juce::DynamicObject::Ptr item (new juce::DynamicObject());
                juce::Array<juce::var> list;

                for (const auto& path : paths)
                    list.add (path);

                item->setProperty ("kind", "fileDrop");
                item->setProperty ("paths", list);
                item->setProperty ("y", onPage.y);
                webView->emitEventIfBrowserIsVisible ("live", juce::var (item.get()));
            }
        };

        liveDropZone = std::move (zone);
    }

    static_cast<PageDropZone*> (liveDropZone.get())->showOver (webView->localAreaToGlobal (area), area.getPosition());
}

int MainComponent::liveLinkDeck (int deck, const juce::String& what) const
{
    // The playlist's tabs have one player: its channel is deck 1's, the rest the deck in front.
    if (liveMode == "dj" || ! juce::isPositiveAndBelow (deck, LiveScreen::numDecks))
        return deck;

    const auto index = LiveShow::deckIndexOf (what);
    return index >= 0 && LiveShow::isChannelParam (index) ? 0 : liveFront;
}

juce::File MainComponent::libraryFolderFile()
{
    // With Sonify's other settings (AppData\Roaming\Fanan\Sonify): a new build of Sonify (its
    // build folder made again) doesn't lose it.
    return SettingsFolder::file ("liveshow-library.json", "FananWebHub-liveshow-library.json");
}

juce::String MainComponent::keptLibraryFolder()
{
    const auto folder = juce::JSON::parse (libraryFolderFile()).getProperty ("folder", {}).toString();
    return juce::File::isAbsolutePath (folder) ? folder : juce::String();
}

bool MainComponent::keepLibraryFolder (const juce::File& folder)
{
    const auto kept = libraryFolderFile();
    juce::DynamicObject::Ptr data (new juce::DynamicObject());
    data->setProperty ("folder", folder.getFullPathName());

    return kept.getParentDirectory().createDirectory().wasOk()
        && kept.replaceWithText (juce::JSON::toString (juce::var (data.get())));
}

void MainComponent::choosePlaylistFile (bool saving)
{
    auto folder = UserData::getFolder().getChildFile ("Playlists");
    folder.createDirectory();
    const auto start = lastPlaylistFile.existsAsFile() ? lastPlaylistFile
                                                        : (saving ? folder.getChildFile ("Playlist" + juce::String (playlistExtension)) : folder);
    playlistChooser = std::make_unique<juce::FileChooser> (saving ? "Keep the playlist" : "Open a playlist", start, juce::String ("*") + playlistExtension);
    const auto chooserFlags = saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting
                                     : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    playlistChooser->launchAsync (chooserFlags, [safeThis = juce::Component::SafePointer<MainComponent> (this), saving] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (safeThis == nullptr || file == juce::File())
            return;

        if (saving)
            safeThis->savePlaylist (file.withFileExtension (playlistExtension));
        else
            safeThis->openPlaylist (file);
    });
}

void MainComponent::savePlaylist (const juce::File& file)
{
    juce::Array<juce::var> items;

    for (const auto& item : livePlaylist)
    {
        juce::DynamicObject::Ptr kept (new juce::DynamicObject());

        for (const auto* field : { "path", "title", "sync", "key", "volume", "timing" })
            kept->setProperty (field, item.getProperty (field, {}));

        // Its track as it is now: its tempo, first beat and key, its hot cues' and loops' markers.
        if (const auto known = liveTracks().find (juce::File (item.getProperty ("path", {}).toString())))
            kept->setProperty ("track", known->toVar());

        items.add (juce::var (kept.get()));
    }

    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("format", "Sonify playlist");
    root->setProperty ("version", 2);
    root->setProperty ("items", items);

    if (! file.replaceWithText (juce::JSON::toString (juce::var (root.get()), true)))
    {
        pushLive ("Couldn't write " + file.getFullPathName() + ".");
        return;
    }

    lastPlaylistFile = file;
    pushLive ("The playlist is kept: " + file.getFileName() + ".");
}

void MainComponent::openPlaylist (const juce::File& file)
{
    const auto data = juce::JSON::parse (file);
    const auto* items = data.getProperty ("items", {}).getArray();

    if (data.getProperty ("format", {}).toString() != "Sonify playlist" || items == nullptr)
    {
        pushLive (file.getFileName() + " isn't a Sonify playlist.");
        return;
    }

    livePlaylist.clear();
    livePlayed.clear();

    // The decks: one playing plays on (as a track of its own: the new list follows it from its top);
    // the others take the new list's. The playlist's tabs: the player's emptied at once (what it
    // held, or was bringing, isn't on this list), the new list's first coming in its place.
    for (int d = 0; d < LiveShow::maxDecks; ++d)
    {
        const auto place = static_cast<size_t> (d);
        const bool playing = engine.getLiveShow().decks[place].isPlaying();

        if (liveMode != "dj" && d < LiveScreen::numDecks && ! playing)
        {
            emptyDeck (d);
            continue;
        }

        if (playing && (deckLoading[place].isNotEmpty() || deckPreparing[place] != nullptr))
        {
            ++deckLoadToken[place];
            deckPreparing[place] = nullptr;
            deckLoading[place].clear();
        }

        deckItem[place] = playing ? 0 : -1;
        deckAfter[place] = 0;
    }

    std::vector<juce::File> unknown;
    int missing = 0;

    for (const auto& kept : *items)
    {
        const juce::File track (kept.getProperty ("path", {}).toString());

        if (track.getFullPathName().isEmpty())
            continue;

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", livePlaylistNext++);
        item->setProperty ("path", track.getFullPathName());
        item->setProperty ("title", kept.getProperty ("title", track.getFileNameWithoutExtension()));
        item->setProperty ("sync", static_cast<bool> (kept.getProperty ("sync", false)));
        item->setProperty ("key", juce::jlimit (-12, 12, static_cast<int> (kept.getProperty ("key", 0))));
        item->setProperty ("volume", juce::jlimit (0.0, 1.0, static_cast<double> (kept.getProperty ("volume", 0.5))));
        item->setProperty ("timing", juce::jlimit (-60.0, 60.0, static_cast<double> (kept.getProperty ("timing", 0.0))));
        livePlaylist.add (juce::var (item.get()));

        if (! track.existsAsFile())
        {
            ++missing;
            continue;
        }

        // Its track's markers (and tempo) as the playlist kept them.
        if (const auto saved = kept.getProperty ("track", {}); saved.isObject())
            restoreTrackInfo (track, LiveShow::TrackInfo::fromVar (saved));

        if (const auto known = liveTracks().find (track); ! known.has_value() || ! known->heard)
            unknown.push_back (track);
    }

    lastPlaylistFile = file;
    keepLiveExtras();
    pushLive (file.getFileNameWithoutExtension() + ": " + juce::String (livePlaylist.size()) + (livePlaylist.size() == 1 ? " track" : " tracks")
              + (missing > 0 ? ", " + juce::String (missing) + " of them not found (struck through)." : juce::String (".")));

    if (! unknown.empty())
        analyseLiveFiles (unknown, file.getFileNameWithoutExtension());
}

void MainComponent::restoreTrackInfo (const juce::File& file, const LiveShow::TrackInfo& saved)
{
    // What Sonify heard of the track stays (its tempo, unless the playlist's was typed); its hot
    // cues and loops are the playlist's.
    auto info = liveTracks().find (file).value_or (LiveShow::TrackInfo {});

    if (! info.heard)
    {
        info = saved;
    }
    else if (saved.bpmByUser && saved.bpm > 0.0)
    {
        info.bpm = saved.bpm;
        info.bpmByUser = true;
    }

    info.hotCues = saved.hotCues;
    info.loops = saved.loops;
    liveTracks().set (file, info);
    saveLiveTracksSoon();

    // A deck holding it shows them now (a loop playing follows its markers).
    for (int d = 0; d < LiveShow::maxDecks; ++d)
    {
        const auto track = engine.getDeckTrack (d);

        if (track == nullptr || track->file != file)
            continue;

        const auto place = static_cast<size_t> (d);
        auto& deck = engine.getLiveShow().decks[place];
        auto& known = deckInfo[place];
        known.hotCues = info.hotCues;
        known.loops = info.loops;
        placeDefaultCues (d);

        if (info.heard && (! known.heard || info.bpmByUser))
        {
            known.bpm = info.bpm;
            known.firstBeat = info.firstBeat;
            known.keyTonic = info.keyTonic;
            known.keyMinor = info.keyMinor;
            known.bpmByUser = info.bpmByUser;
            known.heard = true;
            deck.setBeat (known.bpm, known.firstBeat);
            applyDeckTempo (d);
        }

        if (const auto playing = deckLoop[place]; playing >= 0)
        {
            if (const auto& loop = known.loops[static_cast<size_t> (playing)]; loop.isOn())
            {
                deck.setLoop (loop.start, loop.end);
            }
            else
            {
                deckLoop[place] = -1;
                deck.setLoop (-1.0, -1.0);
            }
        }

        keepDeckInfo (d);
    }
}

//==============================================================================
LiveShow::TrackLibrary& MainComponent::liveTracks()
{
    if (trackLibrary == nullptr)
        trackLibrary = std::make_unique<LiveShow::TrackLibrary> (SettingsFolder::file ("liveshow-tracks.json", "FananWebHub-liveshow-tracks.json"));

    return *trackLibrary;
}

void MainComponent::saveLiveTracksSoon()
{
    if (liveTracksSaveDue)
        return;

    liveTracksSaveDue = true;

    juce::Timer::callAfterDelay (2000, [safeThis = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safeThis == nullptr)
            return;

        safeThis->liveTracksSaveDue = false;

        if (safeThis->trackLibrary != nullptr && safeThis->trackLibrary->needsSaving())
            safeThis->trackLibrary->save();
    });
}

void MainComponent::keepDeckInfo (int d)
{
    if (const auto track = engine.getDeckTrack (d); track != nullptr)
    {
        liveTracks().set (track->file, deckInfo[static_cast<size_t> (d)]);
        saveLiveTracksSoon();
    }
}

void MainComponent::deckLoaded (int d)
{
    const auto place = static_cast<size_t> (d);
    auto& show = engine.getLiveShow();
    auto& deck = show.decks[place];
    const auto track = engine.getDeckTrack (d);
    deckListening[place] = false;
    deckLoop[place] = -1;
    deckHandedOver[place] = false;
    deckStarted[place] = false;
    deckFinished[place] = false;
    deckByUser[place] = deckLoadByUser[place];
    deckEndedAt[place] = 0.0;
    deck.setLoop (-1.0, -1.0);
    ++deckListenToken[place];

    if (track == nullptr)
    {
        deckInfo[place] = {};
        deckItem[place] = 0;
        deckAfter[place] = 0;
        return;
    }

    if (const auto known = liveTracks().find (track->file); known.has_value() && known->heard)
    {
        deckInfo[place] = *known;
        deck.setBeat (known->bpm, known->firstBeat);
    }
    else
    {
        // Not heard yet: its markers (moved before it was) stay, its tempo and key are listened for.
        deckInfo[place] = known.value_or (LiveShow::TrackInfo {});
        deck.setBeat (deckInfo[place].bpm, deckInfo[place].firstBeat);
        listenToDeck (d);
    }

    // Its hot cues where they were left (or at its start, a little apart, the first time).
    placeDefaultCues (d);
    keepDeckInfo (d);

    // From the playlist: its options come along. From the library: its key and volume as recorded
    // (the deck's tempo and SYNC stay as they are).
    deckItem[place] = findPlaylistItem (deckLoadItem[place]) != nullptr ? deckLoadItem[place] : 0;
    deckAfter[place] = 0;

    if (deckItem[place] != 0)
    {
        applyItemToDeck (d);
    }
    else
    {
        deck.settings.set (LiveShow::deckKey, 0.0f);
        deck.settings.set (LiveShow::deckTrackVolume, 0.5f);
        applyDeckTempo (d);
    }
}

void MainComponent::listenToDeck (int d)
{
    const auto place = static_cast<size_t> (d);
    const auto track = engine.getDeckTrack (d);

    if (track == nullptr)
        return;

    const auto token = ++deckListenToken[place];
    deckListening[place] = true;

    liveListener.addJob ([this, safeThis = juce::Component::SafePointer<MainComponent> (this), d, place, token, track]
    {
        auto found = LiveShow::analyse (*track, [this, place, token] { return ! liveClosing.load() && deckListenToken[place].load() == token; });

        juce::MessageManager::callAsync ([safeThis, d, place, token, found, file = track->file]
        {
            if (safeThis == nullptr || safeThis->deckListenToken[place].load() != token)
                return;   // gone, or the deck has moved on

            safeThis->heardDeck (d, file, found);
        });
    });
}

void MainComponent::heardDeck (int d, const juce::File& file, LiveShow::TrackInfo found)
{
    const auto place = static_cast<size_t> (d);
    auto& deck = engine.getLiveShow().decks[place];
    deckListening[place] = false;

    const auto track = engine.getDeckTrack (d);

    if (track == nullptr || track->file != file)
        return;

    // A tempo typed while it listened, and its hot cues, stay.
    if (deckInfo[place].bpmByUser)
    {
        found.bpm = deckInfo[place].bpm;
        found.bpmByUser = true;
    }

    found.hotCues = deckInfo[place].hotCues;
    found.loops = deckInfo[place].loops;
    found.heard = true;
    deckInfo[place] = found;
    deck.setBeat (found.bpm, found.firstBeat);
    keepDeckInfo (d);
    applyDeckTempo (d);

    pushLive (found.bpm <= 0.0 ? (liveMode == "dj" ? deckName (d) + ": no" : juce::String ("No")) + " steady beat heard in " + track->title + ". Type its BPM."
                               : juce::String());
}

void MainComponent::analyseLiveFolder (const juce::File& folder)
{
    if (! folder.isDirectory())
        return;

    // The tracks not known yet (and in the playlist's tabs, the videos: their sound).
    std::vector<juce::File> files;
    const bool videos = liveMode != "dj";

    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFiles | juce::File::ignoreHiddenFiles))
        if (engine.getMedia().isAudioFile (entry.getFile()) || (videos && VideoMedia::isVideoFile (entry.getFile())))
            if (const auto known = liveTracks().find (entry.getFile()); ! known.has_value() || ! known->heard)
                files.push_back (entry.getFile());

    if (files.empty())
    {
        pushLive ("Every track in " + folder.getFileName() + " has been listened to already.");
        return;
    }

    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    analyseLiveFiles (std::move (files), folder.getFileName());
}

void MainComponent::analyseLiveFiles (std::vector<juce::File> files, const juce::String& what)
{
    if (files.empty())
        return;

    const auto token = ++folderListenToken;
    const auto total = static_cast<int> (files.size());
    pushLive ("Listening to " + juce::String (total) + (total == 1 ? " track" : " tracks") + " in " + what
              + " for their tempo and key. They fill in as they're done.");

    liveListener.addJob ([this, safeThis = juce::Component::SafePointer<MainComponent> (this), files, token, total]
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        const auto keepGoing = [this, token] { return ! liveClosing.load() && folderListenToken.load() == token; };
        int done = 0;

        for (const auto& file : files)
        {
            if (! keepGoing())
                return;

            juce::String error;
            LiveShow::TrackInfo found;

            // A video: its sound, read straight from it (the kinds VLC lines up itself: read out by
            // VLC, once: kept, so it's on a deck at once later).
            if (VideoMedia::isVideoFile (file))
            {
                auto opened = VideoSound::open (file);

                if (opened.reader != nullptr)
                {
                    found = LiveShow::analyse (*opened.reader, keepGoing);
                }
                else if (opened.error.isNotEmpty())
                {
                    error = opened.error;
                }
                else
                {
                    const auto prepared = VideoMedia::problem().isEmpty() ? VideoMedia::prepareNow (file, deckVideoWanted(), nullptr, &liveClosing)
                                                                          : VideoMedia::Prepared {};

                    if (prepared.ok() && prepared.sound.existsAsFile())
                        found = LiveShow::analyse (prepared.sound, formats, keepGoing, error);
                    else
                        error = prepared.error.isNotEmpty() ? prepared.error : juce::String ("Video can't be read here.");
                }
            }
            else
            {
                found = LiveShow::analyse (file, formats, keepGoing, error);
            }

            if (! keepGoing())
                return;

            ++done;

            juce::MessageManager::callAsync ([safeThis, file, found, error, done, total]
            {
                if (safeThis == nullptr)
                    return;

                auto& self = *safeThis;

                // A file it can't read isn't kept; a tempo typed meanwhile, hot cues and loops stay.
                if (error.isEmpty())
                {
                    auto kept = found;
                    kept.heard = true;

                    if (const auto known = self.liveTracks().find (file))
                    {
                        if (known->bpmByUser)
                        {
                            kept.bpm = known->bpm;
                            kept.bpmByUser = true;
                        }

                        kept.hotCues = known->hotCues;
                        kept.loops = known->loops;
                    }

                    self.liveTracks().set (file, kept);
                    self.saveLiveTracksSoon();
                }

                if (done == total || done % 8 == 0)
                {
                    if (self.liveListingPath.isNotEmpty())
                        self.pushLiveListing (self.liveListingPath);

                    self.pushLive (done == total ? "Listened to " + juce::String (total) + (total == 1 ? " track" : " tracks")
                                                       + ": their tempo and key show." : juce::String());
                }
            });
        }
    });
}

void MainComponent::pushLiveListing (const juce::String& path)
{
    if (webView == nullptr)
        return;

    // Its audio files and videos (the playlist's tabs play videos; DJ show's library leaves them out).
    auto listing = folderListing (path, false);
    liveListingPath = listing.getProperty ("path", {}).toString();

    // Each track's tempo and key, when they're known.
    if (const auto* files = listing.getProperty ("files", {}).getArray())
    {
        for (const auto& item : *files)
        {
            if (auto* object = item.getDynamicObject())
            {
                if (const auto known = liveTracks().find (juce::File (object->getProperty ("path").toString())))
                {
                    if (known->bpm > 0.0)
                        object->setProperty ("bpm", known->bpm);

                    object->setProperty ("key", LiveShow::keyName (known->keyTonic, known->keyMinor));
                    object->setProperty ("known", known->heard || known->bpmByUser);
                }
            }
        }
    }

    listing.getDynamicObject()->setProperty ("kind", "listing");
    webView->emitEventIfBrowserIsVisible ("live", listing);
}

//==============================================================================
void MainComponent::pushLive (const juce::String& note)
{
    if (webView == nullptr)
        return;

    auto& show = engine.getLiveShow();

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    state->setProperty ("kind", "state");
    state->setProperty ("on", engine.isLiveShowOn());
    state->setProperty ("mode", liveMode);
    state->setProperty ("onAir", liveOnAir);
    state->setProperty ("micsMuted", liveMicsMuted);
    state->setProperty ("playbackMuted", livePlaybackMuted);
    state->setProperty ("controllerOpen", liveControl != nullptr && liveControl->isWindowShown());
    state->setProperty ("monitorOpen", liveControl != nullptr && liveControl->isMonitorShown());
    state->setProperty ("songTempo", std::round (songTempo() * 100.0) / 100.0);
    state->setProperty ("playlist", playlistToVar());
    state->setProperty ("playlistAuto", livePlaylistAuto);
    state->setProperty ("playlistFile", lastPlaylistFile.getFileNameWithoutExtension());
    state->setProperty ("note", note);
    state->setProperty ("canKeepKey", LiveShow::canKeepKey());

    // The headphones: the song's Sub outputs, and the one they play on.
    juce::Array<juce::var> subs;

    for (const auto& output : engine.getOutputs())
    {
        if (output.id == 0)
            continue;

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", output.id);
        item->setProperty ("name", output.name);
        subs.add (juce::var (item.get()));
    }

    state->setProperty ("outputs", subs);
    state->setProperty ("phonesOutput", engine.getLivePhonesOutput());

    // The night's recording, and the podcast.
    state->setProperty ("recording", engine.isLiveRecording());
    state->setProperty ("stems", liveRecordStems);
    state->setProperty ("lastTake", lastLiveTake.existsAsFile() ? lastLiveTake.getFullPathName() : juce::String());
    state->setProperty ("podcastBusy", podcastExport != nullptr);
    state->setProperty ("podcastFile", lastPodcastFile.existsAsFile() ? lastPodcastFile.getFullPathName() : juce::String());
    state->setProperty ("stream", streamToVar());

    // What the controls are (small enough to send each time).
    juce::DynamicObject::Ptr specs (new juce::DynamicObject());
    specs->setProperty ("deck", liveSpecsToVar (LiveShow::deckSpecs().data(), LiveShow::deckSpecs().size()));
    specs->setProperty ("channel", liveSpecsToVar (LiveShow::channelSpecs().data(), LiveShow::channelSpecs().size()));
    specs->setProperty ("master", liveSpecsToVar (LiveShow::masterSpecs().data(), LiveShow::masterSpecs().size()));
    state->setProperty ("specs", juce::var (specs.get()));
    state->setProperty ("master", show.settings.toVar (true));

    // The decks: their settings, the track each has (or is loading), and what's known of it.
    juce::Array<juce::var> decks;

    for (int d = 0; d < LiveShow::maxDecks; ++d)
    {
        const auto place = static_cast<size_t> (d);
        const auto& player = show.decks[place];
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("values", player.settings.toVar (true));
        item->setProperty ("loading", deckLoading[place]);
        item->setProperty ("error", deckError[place]);

        // A video being read out for it: how far.
        if (deckPreparing[place] != nullptr)
            item->setProperty ("progress", std::round (deckPreparing[place]->getProgress() * 100.0) / 100.0);

        if (const auto track = engine.getDeckTrack (d); track != nullptr)
        {
            item->setProperty ("title", track->title);
            item->setProperty ("file", track->file.getFullPathName());
            item->setProperty ("length", track->lengthSeconds);
            item->setProperty ("info", trackInfoToVar (deckInfo[place]));
            item->setProperty ("listening", deckListening[place]);
            item->setProperty ("item", deckItem[place]);
            item->setProperty ("activeLoop", deckLoop[place]);
            item->setProperty ("video", deckVideo[place].video != juce::File());
        }

        decks.add (juce::var (item.get()));
    }

    state->setProperty ("decks", decks);

    // The playlist's tabs: the deck the player shows, and the show's screen.
    state->setProperty ("front", liveFront);
    state->setProperty ("screen", screenToVar());

    // The pads: the sound each has (or is loading).
    juce::Array<juce::var> pads;

    for (int p = 0; p < LiveShow::numPads; ++p)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("loading", padLoading[static_cast<size_t> (p)]);

        if (const auto sound = engine.getPadSound (p); sound != nullptr)
        {
            item->setProperty ("title", sound->title);
            item->setProperty ("file", sound->file.getFullPathName());
            item->setProperty ("length", sound->lengthSeconds);
        }

        pads.add (juce::var (item.get()));
    }

    state->setProperty ("pads", pads);

    // The show's channels, in the song's order (the page names them from its tracks).
    juce::Array<juce::var> channels;

    for (const auto& track : engine.getTracks())
    {
        if (track->isMixChannel() || ! track->liveChannel.load())
            continue;

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", track->id);
        item->setProperty ("values", track->live.settings.toVar (true));
        item->setProperty ("hasInput", track->kind != Track::Kind::audio || track->inputLeft.load() >= 0);
        channels.add (juce::var (item.get()));
    }

    state->setProperty ("channels", channels);
    webView->emitEventIfBrowserIsVisible ("live", juce::var (state.get()));
    liveValuesDue = false;
    liveStateDue = false;
}

void MainComponent::pushLiveValues()
{
    liveValuesDue = false;

    if (webView == nullptr)
        return;

    auto& show = engine.getLiveShow();
    juce::DynamicObject::Ptr values (new juce::DynamicObject());
    values->setProperty ("kind", "values");
    values->setProperty ("master", show.settings.toVar (true));

    juce::Array<juce::var> decks;

    for (const auto& player : show.decks)
        decks.add (player.settings.toVar (true));

    values->setProperty ("decks", decks);

    juce::Array<juce::var> channels;

    for (const auto& track : engine.getTracks())
    {
        if (track->isMixChannel() || ! track->liveChannel.load())
            continue;

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", track->id);
        item->setProperty ("values", track->live.settings.toVar (true));
        channels.add (juce::var (item.get()));
    }

    values->setProperty ("channels", channels);
    webView->emitEventIfBrowserIsVisible ("live", juce::var (values.get()));
}

void MainComponent::pushDeckWave (int deck)
{
    if (webView == nullptr || ! juce::isPositiveAndBelow (deck, LiveShow::maxDecks))
        return;

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("kind", "wave");
    item->setProperty ("deck", deck);

    // Four bytes a point (the peak, then low, middle and high), as Base64.
    if (const auto track = engine.getDeckTrack (deck); track != nullptr)
    {
        item->setProperty ("file", track->file.getFullPathName());
        item->setProperty ("length", track->lengthSeconds);
        item->setProperty ("perSecond", track->wavePerSecond);
        item->setProperty ("data", juce::Base64::toBase64 (track->wave.data(), track->wave.size()));
    }

    webView->emitEventIfBrowserIsVisible ("live", juce::var (item.get()));
}

juce::var MainComponent::liveMeters()
{
    auto& show = engine.getLiveShow();
    juce::DynamicObject::Ptr meters (new juce::DynamicObject());
    meters->setProperty ("on", engine.isLiveShowOn());
    meters->setProperty ("songTempo", std::round (songTempo() * 100.0) / 100.0);

    // The decks: where each is (seconds), whether it plays, its speed and tempo now, its cue point,
    // its loop and its meter.
    juce::Array<juce::var> decks;

    for (auto& player : show.decks)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("position", player.getPosition());
        item->setProperty ("playing", player.isPlaying());
        item->setProperty ("speed", player.getSpeed());
        item->setProperty ("bpm", std::round (player.getBpm() * 100.0) / 100.0);

        if (player.isLooping())
            item->setProperty ("loop", juce::Array<juce::var> { player.getLoopStart(), player.getLoopEnd() });

        item->setProperty ("meter", liveMeterToVar (player.meter));
        decks.add (juce::var (item.get()));
    }

    meters->setProperty ("decks", decks);

    juce::Array<juce::var> pads;

    for (const auto& pad : show.pads)
        pads.add (pad.isPlaying());

    meters->setProperty ("pads", pads);

    juce::DynamicObject::Ptr channels (new juce::DynamicObject());

    for (const auto& track : engine.getTracks())
        if (! track->isMixChannel() && track->liveChannel.load())
            channels->setProperty (juce::String (track->id), liveMeterToVar (track->live.meter));

    meters->setProperty ("channels", juce::var (channels.get()));
    meters->setProperty ("master", liveMeterToVar (show.meter));
    // Its effects: how much the limiter, the compressor and the drive pull down (dB), and how loud
    // it goes into the limiter (dBFS); the width's below.
    meters->setProperty ("limiter", std::round (show.takeLimiterReduction() * 10.0f) / 10.0);
    meters->setProperty ("compressor", std::round (show.takeCompressorReduction() * 10.0f) / 10.0);
    meters->setProperty ("drive", std::round (show.takeDriveSaturation() * 10.0f) / 10.0);
    meters->setProperty ("limiterIn", std::round (show.takeLimiterInput() * 10.0f) / 10.0);

    // The stereo after the width: how alike the sides are (+1 mono, -1 out of phase), and, while
    // the width's picture shows, its latest moments as mid and side, in turn.
    meters->setProperty ("correlation", std::round (show.getStereoCorrelation() * 100.0f) / 100.0);

    if (liveWidthWatched)
    {
        constexpr int points = 192;
        std::array<float, points> mids {}, sides {};
        show.readStereo (mids.data(), sides.data(), points);
        juce::Array<juce::var> stereo;
        stereo.ensureStorageAllocated (points * 2);

        for (int i = 0; i < points; ++i)
        {
            stereo.add (std::round (mids[static_cast<size_t> (i)] * 1000.0f) / 1000.0);
            stereo.add (std::round (sides[static_cast<size_t> (i)] * 1000.0f) / 1000.0);
        }

        meters->setProperty ("stereo", stereo);
    }
    meters->setProperty ("phones", liveMeterToVar (show.phonesMeter));

    // How long the night has recorded, and how far the podcast has got.
    if (engine.isLiveRecording())
        meters->setProperty ("recorded", std::round (engine.getLiveRecordedSeconds() * 10.0) / 10.0);

    if (podcastExport != nullptr)
        meters->setProperty ("podcast", std::round (podcastExport->getProgress() * 1000.0) / 1000.0);

    // The stream, while it's live (or the panel shows).
    if (liveVideo != nullptr && (liveVideo->isStreaming() || streamPanelOpen))
        meters->setProperty ("stream", streamStatsToVar());

    // The dynamic EQ's points (the four bells, then the level): each one's gain now (dB: -100 for
    // silence), and how loud the voices are in its band.
    juce::Array<juce::var> ducker;

    for (int point = 0; point < LiveShow::Ducker::numPoints; ++point)
        ducker.add (juce::Array<juce::var> { std::round (show.ducker.getGainDb (point) * 10.0f) / 10.0,
                                             std::round (show.ducker.getVoiceLevel (point) * 10.0f) / 10.0 });

    meters->setProperty ("ducker", ducker);
    return juce::var (meters.get());
}

//==============================================================================
// The live stream.

juce::File MainComponent::streamSettingsFile()
{
    // The setup's, as MIDI Link's map is: next to Sonify's settings (stream keys stay on this computer).
    return SettingsFolder::file ("liveshow-stream.json", "FananWebHub-liveshow-stream.json");
}

LiveStream::Studio& MainComponent::liveStudio()
{
    if (liveVideo == nullptr)
    {
        liveVideo = std::make_unique<LiveStream::Studio>();

        if (! streamSettings.isObject())
            streamSettings = juce::JSON::parse (streamSettingsFile());

        // A first stream: YouTube, waiting for its key (the Controller has the test pattern on
        // Main until a camera is chosen).
        if (! streamSettings.isObject())
        {
            juce::DynamicObject::Ptr made (new juce::DynamicObject());
            made->setProperty ("title", "");
            made->setProperty ("nowPlaying", true);
            made->setProperty ("logo", "");
            made->setProperty ("size", "720p");
            made->setProperty ("videoKbps", 4500);
            made->setProperty ("audioKbps", 160);
            made->setProperty ("syncMs", 0);
            made->setProperty ("copy", false);

            juce::DynamicObject::Ptr youtube (new juce::DynamicObject());
            youtube->setProperty ("service", "YouTube");
            youtube->setProperty ("key", "");
            youtube->setProperty ("on", true);
            made->setProperty ("destinations", juce::Array<juce::var> { juce::var (youtube.get()) });
            streamSettings = juce::var (made.get());
        }

        applyStreamSettings();
        applyLiveControl();   // its sources: the Controller's rows
        applyLiveMute();      // streaming not enabled: its picture held
    }

    return *liveVideo;
}

void MainComponent::applyStreamSettings()
{
    if (liveVideo == nullptr)
        return;

    const auto& s = streamSettings;
    LiveStream::Look look;
    const auto size = s.getProperty ("size", "720p").toString();
    look.width = size == "1080p" ? 1920 : size == "480p" ? 854 : 1280;
    look.height = size == "1080p" ? 1080 : size == "480p" ? 480 : 720;
    look.fps = 30;
    look.title = s.getProperty ("title", {}).toString();
    look.showNowPlaying = static_cast<bool> (s.getProperty ("nowPlaying", true));

    // The logo, read again only when it's another file.
    const auto logoPath = s.getProperty ("logo", {}).toString();

    if (logoPath != streamLogoPath)
    {
        streamLogoPath = logoPath;
        streamLogo = logoPath.isNotEmpty() ? juce::ImageFileFormat::loadFrom (juce::File (logoPath)) : juce::Image();
    }

    look.logo = streamLogo;
    liveVideo->setLook (look);
}

juce::var MainComponent::streamToVar()
{
    juce::DynamicObject::Ptr stream (new juce::DynamicObject());
    const auto why = LiveStream::problem();
    stream->setProperty ("available", why.isEmpty());
    stream->setProperty ("problem", why);
    stream->setProperty ("live", liveVideo != nullptr && liveVideo->isStreaming());
    stream->setProperty ("ending", streamEnding);
    stream->setProperty ("settings", streamSettings);
    stream->setProperty ("logoName", streamLogoPath.isNotEmpty() ? juce::File (streamLogoPath).getFileName() : juce::String());

    const auto devices = [] (const std::vector<LiveStream::Device>& list)
    {
        juce::Array<juce::var> items;

        for (const auto& device : list)
        {
            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("id", device.id);
            item->setProperty ("name", device.name);
            items.add (juce::var (item.get()));
        }

        return items;
    };

    stream->setProperty ("cameras", devices (streamCameras));
    stream->setProperty ("screens", devices (streamScreens));
    return juce::var (stream.get());
}

juce::var MainComponent::streamStatsToVar()
{
    const auto streaming = liveVideo != nullptr && liveVideo->isStreaming();
    const auto stats = streaming ? liveVideo->getStats() : streamLast;

    juce::DynamicObject::Ptr item (new juce::DynamicObject());
    item->setProperty ("live", streaming);
    item->setProperty ("ending", streamEnding);
    item->setProperty ("seconds", std::round (stats.seconds * 10.0) / 10.0);
    item->setProperty ("fps", std::round ((liveVideo != nullptr ? liveVideo->getStats().fps : 0.0) * 10.0) / 10.0);
    item->setProperty ("encoder", stats.encoder);
    item->setProperty ("error", stats.error);
    item->setProperty ("source", liveVideo != nullptr ? liveVideo->getSourceProblem() : juce::String());

    juce::Array<juce::var> outputs;

    for (const auto& output : stats.outputs)
    {
        juce::DynamicObject::Ptr o (new juce::DynamicObject());
        o->setProperty ("name", output.name);
        o->setProperty ("state", output.state);
        o->setProperty ("error", output.error);
        o->setProperty ("kbps", juce::roundToInt (output.kbps));
        o->setProperty ("skips", output.skips);
        outputs.add (juce::var (o.get()));
    }

    item->setProperty ("outputs", outputs);
    return juce::var (item.get());
}

void MainComponent::handleLiveStream (const juce::String& action, const juce::var& event)
{
    liveStudio();   // made the first time it's needed

    if (action == "streamPanel")
    {
        streamPanelOpen = static_cast<bool> (event.getProperty ("open", false));
        applyLivePreviewing();
        pushLive();
        return;
    }

    if (action == "streamDevices")
    {
        streamCameras = LiveStream::cameras();
        streamScreens = LiveStream::screens();
        pushLive();
        return;
    }

    if (action == "streamSettings")
    {
        if (const auto settings = event.getProperty ("settings", {}); settings.isObject())
        {
            streamSettings = juce::JSON::parse (juce::JSON::toString (settings));   // a copy of its own
            streamSettingsFile().replaceWithText (juce::JSON::toString (streamSettings));
            applyStreamSettings();
            applyLivePreviewing();   // a vertical destination ticked: the vertical picture is made for the panel
        }

        return;   // the page shows them as it set them
    }

    if (action == "streamLogo")
    {
        const auto setLogo = [this] (const juce::String& path)
        {
            if (auto* settings = streamSettings.getDynamicObject())
            {
                settings->setProperty ("logo", path);
                streamSettingsFile().replaceWithText (juce::JSON::toString (streamSettings));
            }

            applyStreamSettings();

            if (path.isNotEmpty() && ! streamLogo.isValid())
                pushLive (juce::File (path).getFileName() + " isn't a picture Sonify can read: PNG or JPEG.");
            else
                pushLive();
        };

        if (! static_cast<bool> (event.getProperty ("choose", false)))
        {
            setLogo ({});
            return;
        }

        logoChooser = std::make_unique<juce::FileChooser> ("A logo for the stream's picture (a PNG with a clear background looks best)",
                                                           juce::File::getSpecialLocation (juce::File::userPicturesDirectory), "*.png;*.jpg;*.jpeg");
        logoChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [safeThis = juce::Component::SafePointer<MainComponent> (this), setLogo] (const juce::FileChooser& chooser)
        {
            if (safeThis != nullptr && chooser.getResult() != juce::File())
                setLogo (chooser.getResult().getFullPathName());
        });

        return;
    }

    if (action == "stream")
    {
        if (static_cast<bool> (event.getProperty ("on", false)))
            startLiveStream();
        else
            stopLiveStream ({});
    }
}

void MainComponent::startLiveStream()
{
    auto& studio = liveStudio();

    if (studio.isStreaming())
    {
        pushLive (streamEnding ? "The last stream is still ending: a moment." : juce::String());
        return;
    }

    // The stream is the show as it plays: its tab has Main.
    updateLiveOwnership();

    if (! engine.isLiveShowOn())
    {
        pushLive ("Not while an export is running: stream once it's done.");
        return;
    }

    if (const auto why = LiveStream::problem(); why.isNotEmpty())
    {
        pushLive (why);
        return;
    }

    const auto& s = streamSettings;
    LiveStream::Settings settings;
    int withoutKey = 0;
    bool tikTokWithoutServer = false;

    if (const auto* destinations = s.getProperty ("destinations", {}).getArray())
    {
        for (const auto& destination : *destinations)
        {
            if (! static_cast<bool> (destination.getProperty ("on", true)))
                continue;

            const auto service = destination.getProperty ("service", "Custom").toString();
            const auto key = destination.getProperty ("key", {}).toString().trim();
            const auto ingest = destination.getProperty ("server", {}).toString().trim();

            if (key.isEmpty())
            {
                ++withoutKey;
                continue;
            }

            // TikTok: its server URL and its key, both from its LIVE Center (a key pasted with the
            // server in it is whole).
            if (service == "TikTok" && ! key.startsWithIgnoreCase ("rtmp") && ! ingest.startsWithIgnoreCase ("rtmp"))
            {
                tikTokWithoutServer = true;
                continue;
            }

            const auto url = LiveStream::ingestUrl (service, key, ingest);
            const auto vertical = LiveStream::isVerticalService (service) || (service == "Custom" && static_cast<bool> (destination.getProperty ("vertical", false)));
            const auto name = service != "Custom" ? service : url.fromFirstOccurrenceOf ("://", false, false).upToFirstOccurrenceOf ("/", false, false);
            settings.destinations.push_back ({ name, url, vertical });
        }
    }

    // A copy on this computer, with the recordings (the vertical picture's too, beside it, while a
    // vertical destination streams).
    if (static_cast<bool> (s.getProperty ("copy", false)))
    {
        auto folder = UserData::recordings();
        folder.createDirectory();
        const auto name = "Live Show " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M") + " stream";
        const auto copy = folder.getNonexistentChildFile (name, ".mp4", false);
        const auto anyVertical = std::any_of (settings.destinations.begin(), settings.destinations.end(),
                                              [] (const LiveStream::Destination& d) { return d.vertical; });
        settings.destinations.push_back ({ "Copy on this computer", copy.getFullPathName(), false });

        if (anyVertical)
            settings.destinations.push_back ({ "Vertical copy on this computer", folder.getNonexistentChildFile (name + " (vertical)", ".mp4", false).getFullPathName(), true });
    }

    if (settings.destinations.empty())
    {
        pushLive (tikTokWithoutServer ? "Put in TikTok's server URL as well as its stream key (TikTok's LIVE Center gives both)."
                  : withoutKey > 0 ? "Put in the stream key of each destination (the service gives it to you), or keep a copy on this computer."
                                   : "Add a destination first: YouTube, Facebook, Twitch, TikTok or a server of your own (or keep a copy on this computer).");
        return;
    }

    settings.videoKbps = juce::jlimit (500, 20000, static_cast<int> (s.getProperty ("videoKbps", 4500)));
    settings.audioKbps = juce::jlimit (64, 320, static_cast<int> (s.getProperty ("audioKbps", 160)));
    settings.syncMs = juce::jlimit (-1000, 1000, static_cast<int> (s.getProperty ("syncMs", 0)));

    auto* tap = engine.openLiveTap();
    const auto started = studio.start (settings, *tap);

    if (started.failed())
    {
        engine.closeLiveTap();
        pushLive (started.getErrorMessage());
        return;
    }

    applyLivePreviewing();
    applyLiveMute();   // streaming not enabled: it goes out held, until Enable streaming
    streamLast = {};
    streamEnding = false;

    juce::StringArray names;

    for (const auto& destination : settings.destinations)
        names.add (destination.name);

    pushLive ("Going live: " + names.joinIntoString (", ")
              + (withoutKey > 0 ? " (" + juce::String (withoutKey) + " without a stream key left out)" : juce::String())
              + (tikTokWithoutServer ? juce::String (" (TikTok left out: its server URL is missing)") : juce::String())
              + (liveOnAir ? juce::String (".") : juce::String (". Streaming isn't enabled yet: the stream shows the show's name until you press Enable streaming.")));
}

bool MainComponent::streamHasVertical() const
{
    if (const auto* destinations = streamSettings.getProperty ("destinations", {}).getArray())
    {
        for (const auto& destination : *destinations)
        {
            if (! static_cast<bool> (destination.getProperty ("on", true)))
                continue;

            const auto service = destination.getProperty ("service", "Custom").toString();

            if (LiveStream::isVerticalService (service) || (service == "Custom" && static_cast<bool> (destination.getProperty ("vertical", false))))
                return true;
        }
    }

    return false;
}

void MainComponent::stopLiveStream (const juce::String& why)
{
    if (liveVideo == nullptr || ! liveVideo->isStreaming() || streamEnding)
        return;

    liveVideo->stop();
    streamEnding = true;
    pushLive (why + "the stream is ending: what's made goes out first.");
}

void MainComponent::updateLiveStream()
{
    if (liveVideo == nullptr)
        return;

    // What's playing: the deck playing loudest in the room.
    {
        auto& show = engine.getLiveShow();
        juce::String title;
        float loudest = -1000.0f;

        for (int d = 0; d < show.getDeckCount(); ++d)
        {
            const auto& player = show.decks[static_cast<size_t> (d)];
            const auto track = engine.getDeckTrack (d);

            if (track != nullptr && player.isPlaying() && player.settings.get (LiveShow::deckVolume) > loudest)
            {
                loudest = player.settings.get (LiveShow::deckVolume);
                title = track->title;
            }
        }

        if (title != streamPlaying)
        {
            streamPlaying = title;
            liveVideo->setNowPlaying (title);
        }
    }

    // A stream that has ended (stopped, or by itself): its last word kept, the tap let go.
    if (liveVideo->isStreaming())
    {
        const auto stats = liveVideo->getStats();

        if (liveVideo->collect())
        {
            streamLast = stats;
            streamLast.streaming = false;
            engine.closeLiveTap();
            streamEnding = false;
            applyLivePreviewing();
            pushLive (stats.error.isNotEmpty() ? "The stream stopped: " + stats.error : juce::String ("The stream has ended."));
        }
    }

    // The preview, 4 times a second while the panel shows.
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (streamPanelOpen && liveShown && webView != nullptr && now - streamPreviewMs > 250.0)
    {
        streamPreviewMs = now;

        // The landscape picture 480 across; the vertical one (while a vertical destination is
        // ticked) 270 across, as tall as the landscape one is wide.
        const auto send = [this] (const juce::Image& picture, int width, bool tall)
        {
            const auto shrunk = picture.rescaled (width, juce::jmax (1, width * picture.getHeight() / juce::jmax (1, picture.getWidth())),
                                                  juce::Graphics::lowResamplingQuality);
            juce::MemoryOutputStream jpeg;
            juce::JPEGImageFormat format;
            format.setQuality (0.7f);

            if (format.writeImageToStream (shrunk, jpeg))
            {
                juce::DynamicObject::Ptr item (new juce::DynamicObject());
                item->setProperty ("kind", "preview");
                item->setProperty ("tall", tall);
                item->setProperty ("data", juce::Base64::toBase64 (jpeg.getData(), jpeg.getDataSize()));
                webView->emitEventIfBrowserIsVisible ("live", juce::var (item.get()));
            }
        };

        if (const auto picture = liveVideo->getPicture(); picture.isValid())
            send (picture, 480, false);

        if (streamHasVertical())
            if (const auto tall = liveVideo->getTallPicture(); tall.isValid())
                send (tall, 270, true);
    }
}

//==============================================================================
void MainComponent::recordLive (bool on, bool stems)
{
    liveRecordStems = stems;

    if (on == engine.isLiveRecording())
    {
        pushLive();
        return;
    }

    if (! on)
    {
        finishLiveRecording();
        return;
    }

    // It records the show as it plays: its tab has Main.
    updateLiveOwnership();

    if (! engine.isLiveShowOn())
    {
        pushLive ("Not while an export is running: record once it's done.");
        return;
    }

    const auto started = engine.startLiveRecording (stems);
    pushLive (started.failed() ? started.getErrorMessage()
                               : juce::String ("Recording the night") + (stems ? ", each deck and channel on a file of its own too." : "."));
}

void MainComponent::finishLiveRecording (const juce::String& why)
{
    const auto files = engine.stopLiveRecording();

    if (files.show == juce::File())
        return;

    lastLiveTake = files.show;
    const auto stems = static_cast<int> (files.stems.size());
    pushLive (why + "the night is recorded: " + files.show.getFileName()
              + (stems > 0 ? " and " + juce::String (stems) + (stems == 1 ? " stem" : " stems") : juce::String())
              + ", in " + files.folder.getFileName() + ". Podcast makes it ready to publish.");
}

void MainComponent::startPodcast (const juce::File& source, double lufs)
{
    if (podcastExport != nullptr)
    {
        pushLive ("A recording is being made ready already: a moment.");
        return;
    }

    if (! source.existsAsFile())
    {
        pushLive ("Record the night first (REC), or choose a recording (Podcast, then Choose a recording).");
        return;
    }

    const bool stream = lufs > -15.0;
    PodcastExport::Settings settings;
    settings.source = source;
    settings.lufs = lufs;
    settings.target = source.getSiblingFile (source.getFileNameWithoutExtension() + (stream ? " (stream)" : " (podcast)") + ".mp3")
                            .getNonexistentSibling (false);

    podcastExport = std::make_unique<PodcastExport> (settings);
    podcastExport->onDone = [safeThis = juce::Component::SafePointer<MainComponent> (this)]
    {
        // Let go of once its own call has returned.
        juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->finishPodcast(); });
    };

    podcastExport->start();
    pushLive ("Making " + source.getFileName() + " ready " + (stream ? "for streams and video (-14 LUFS)" : "for a podcast (-16 LUFS)")
              + ": its loudness measured, then set, its peaks held under -1 dBTP.");
}

void MainComponent::finishPodcast()
{
    if (podcastExport == nullptr || ! podcastExport->isDone())
        return;

    const auto result = podcastExport->getResult();
    podcastExport = nullptr;

    if (! result.ok)
    {
        pushLive (result.error == "Cancelled" ? juce::String() : result.error);
        return;
    }

    lastPodcastFile = result.file;
    pushLive ("Ready: " + result.file.getFileName() + ", " + juce::String (result.finalLufs, 1) + " LUFS, its peaks at "
              + juce::String (result.truePeakDb, 1) + " dBTP"
              + (result.wroteWav ? juce::String (" (a WAV: an MP3 needs LAME next to Sonify).") : juce::String (".")));
}

//==============================================================================
// MIDI Link: a controller's knobs, faders and buttons on the Live Show.

namespace
{
    /** A knob's or fader's position (0..1) to a setting's value, along the curve the page's has. */
    float liveValueAt (const LiveShow::ParamSpec& spec, float position)
    {
        using Kind = LiveShow::ParamSpec::Kind;
        const juce::String id (spec.id), unit (spec.unit);
        const auto low = spec.minimum, high = spec.maximum;
        const auto p = juce::jlimit (0.0f, 1.0f, position);

        if (spec.kind == Kind::toggle)
            return p >= 0.5f ? 1.0f : 0.0f;

        if (spec.kind == Kind::choice)
            return std::round (p * high);

        if (spec.kind == Kind::fader)
            return p <= 0.003f ? low : juce::jmax (low, std::round ((high + 40.0f * std::log10 (p)) * 10.0f) / 10.0f);

        if (unit == "Hz" || unit == "ms" || id.endsWith ("Q") || id == "compRatio")
            return low * std::pow (high / low, p);

        if (low < 0.0f && high > 0.0f)
        {
            // The middle holds (127 steps have no exact middle).
            if (std::abs (p - 0.5f) < 0.012f)
                return 0.0f;

            const auto value = p < 0.5f ? low * (1.0f - p / 0.5f) : high * (p - 0.5f) / 0.5f;
            return id == "key" ? std::round (value) : value;
        }

        const auto value = low + (high - low) * p;
        return id == "bpm" ? std::round (value * 10.0f) / 10.0f : value;
    }

    /** The track of the show's n-th channel (from 1), as the page shows them; 0: none. */
    int liveChannelTrack (const AudioEngine& engine, int number)
    {
        int count = 0;

        for (const auto& track : engine.getTracks())
            if (! track->isMixChannel() && track->liveChannel.load() && ++count == number)
                return track->id;

        return 0;
    }

    juce::var liveEvent (std::initializer_list<std::pair<const char*, juce::var>> fields)
    {
        juce::DynamicObject::Ptr object (new juce::DynamicObject());

        for (const auto& [name, value] : fields)
            object->setProperty (name, value);

        return juce::var (object.get());
    }
}

void MainComponent::linkMoveLive (const juce::StringArray& parts, float position)
{
    if (parts.size() == 3 && parts[1] == "master")
    {
        if (const auto index = LiveShow::masterIndexOf (parts[2]); index >= 0)
            handleLive (liveEvent ({ { "action", "set" }, { "target", "master" }, { "id", parts[2] },
                                     { "value", liveValueAt (LiveShow::masterSpecs()[static_cast<size_t> (index)], position) } }));
    }
    else if (parts.size() == 4 && parts[1] == "deck")
    {
        if (const auto index = LiveShow::deckIndexOf (parts[3]); index >= 0)
            handleLive (liveEvent ({ { "action", "set" }, { "target", "deck" }, { "deck", liveLinkDeck (parts[2].getIntValue() - 1, parts[3]) }, { "id", parts[3] },
                                     { "value", liveValueAt (LiveShow::deckSpecs()[static_cast<size_t> (index)], position) } }));
    }
    else if (parts.size() == 4 && parts[1] == "channel")
    {
        const auto track = liveChannelTrack (engine, parts[2].getIntValue());
        const auto index = LiveShow::channelIndexOf (parts[3]);

        if (track != 0 && index >= 0)
            handleLive (liveEvent ({ { "action", "set" }, { "target", "channel" }, { "track", track }, { "id", parts[3] },
                                     { "value", liveValueAt (LiveShow::channelSpecs()[static_cast<size_t> (index)], position) } }));
    }

    liveValuesDue = true;
}

void MainComponent::linkSetLive (const juce::StringArray& parts, bool on)
{
    if (parts.size() == 2)
    {
        if (parts[1] == "onair")
            setLiveSwitch ("stream", on);
        else if (parts[1] == "mute")
            setLiveSwitch ("both", on);
        else if (parts[1] == "mutemics")
            setLiveSwitch ("mics", on);
        else if (parts[1] == "muteplayback")
            setLiveSwitch ("playback", on);
        else if (parts[1] == "record")
            recordLive (on, liveRecordStems);

        return;
    }

    // A pad plays (or stops) at each press.
    if (parts.size() == 3 && parts[1] == "pad")
    {
        if (on)
            handleLive (liveEvent ({ { "action", "pad" }, { "pad", parts[2].getIntValue() - 1 } }));

        return;
    }

    // A setting's button: on or off.
    const auto isToggle = [] (const LiveShow::ParamSpec& spec) { return spec.kind == LiveShow::ParamSpec::Kind::toggle; };

    if (parts.size() == 3 && parts[1] == "master")
    {
        if (const auto index = LiveShow::masterIndexOf (parts[2]); index >= 0 && isToggle (LiveShow::masterSpecs()[static_cast<size_t> (index)]))
            handleLive (liveEvent ({ { "action", "set" }, { "target", "master" }, { "id", parts[2] }, { "value", on ? 1.0 : 0.0 } }));

        liveValuesDue = true;
        return;
    }

    if (parts.size() != 4)
        return;

    if (parts[1] == "channel")
    {
        const auto track = liveChannelTrack (engine, parts[2].getIntValue());
        const auto index = LiveShow::channelIndexOf (parts[3]);

        if (track != 0 && index >= 0 && isToggle (LiveShow::channelSpecs()[static_cast<size_t> (index)]))
            handleLive (liveEvent ({ { "action", "set" }, { "target", "channel" }, { "track", track }, { "id", parts[3] }, { "value", on ? 1.0 : 0.0 } }));

        liveValuesDue = true;
        return;
    }

    if (parts[1] != "deck")
        return;

    const auto deck = liveLinkDeck (parts[2].getIntValue() - 1, parts[3]);

    if (! juce::isPositiveAndBelow (deck, LiveShow::maxDecks))
        return;

    if (const auto index = LiveShow::deckIndexOf (parts[3]); index >= 0)
    {
        if (isToggle (LiveShow::deckSpecs()[static_cast<size_t> (index)]))
            handleLive (liveEvent ({ { "action", "set" }, { "target", "deck" }, { "deck", deck }, { "id", parts[3] }, { "value", on ? 1.0 : 0.0 } }));

        liveValuesDue = true;
        return;
    }

    const auto& action = parts[3];

    if (action == "play")
        handleLiveDeck (deck, liveEvent ({ { "do", on ? "play" : "pause" } }));
    else if (action.startsWith ("loop"))
        handleLiveDeck (deck, liveEvent ({ { "do", "loopPlay" }, { "index", action.getTrailingIntValue() - 1 }, { "on", on } }));
    else if (! on)
        return;   // a hot cue (or back to the start) acts as the button goes down
    else if (action == "rewind")
        handleLiveDeck (deck, liveEvent ({ { "do", "rewind" } }));
    else if (action.startsWith ("hotcue"))
        handleLiveDeck (deck, liveEvent ({ { "do", "hotcue" }, { "index", action.getTrailingIntValue() - 1 } }));
}

bool MainComponent::linkIsOnLive (const juce::StringArray& parts)
{
    auto& show = engine.getLiveShow();

    if (parts.size() == 2)
        return parts[1] == "onair"          ? liveOnAir
             : parts[1] == "mute"           ? liveMicsMuted && livePlaybackMuted
             : parts[1] == "mutemics"       ? liveMicsMuted
             : parts[1] == "muteplayback"   ? livePlaybackMuted
                                            : parts[1] == "record" && engine.isLiveRecording();

    if (parts.size() == 3 && parts[1] == "master")
    {
        const auto index = LiveShow::masterIndexOf (parts[2]);
        return index >= 0 && show.settings.isOn (index);
    }

    if (parts.size() != 4)
        return false;   // a pad: each press plays it, or stops it

    if (parts[1] == "channel")
    {
        const auto* track = engine.findTrack (liveChannelTrack (engine, parts[2].getIntValue()));
        const auto index = LiveShow::channelIndexOf (parts[3]);
        return track != nullptr && index >= 0 && track->live.settings.isOn (index);
    }

    const auto deck = liveLinkDeck (parts[2].getIntValue() - 1, parts[3]);

    if (parts[1] != "deck" || ! juce::isPositiveAndBelow (deck, LiveShow::maxDecks))
        return false;

    const auto& player = show.decks[static_cast<size_t> (deck)];

    if (const auto index = LiveShow::deckIndexOf (parts[3]); index >= 0)
        return player.settings.isOn (index);

    // Play is on while it plays, a loop while it loops; the hot cues act at each press.
    if (parts[3].startsWith ("loop"))
        return deckLoop[static_cast<size_t> (deck)] == parts[3].getTrailingIntValue() - 1;

    return parts[3] == "play" && player.isPlaying();
}

//==============================================================================
void MainComponent::loadDeck (int deck, const juce::File& file, int item, bool byUser)
{
    if (! juce::isPositiveAndBelow (deck, LiveShow::maxDecks))
        return;

    const auto place = static_cast<size_t> (deck);

    // A deck playing to the room keeps its track (as VirtualDJ does).
    if (engine.getLiveShow().decks[place].isPlaying())
    {
        pushLive (deckName (deck, liveMode) + " is playing: pause it first" + (liveMode == "dj" ? juce::String (", or load onto another deck.") : juce::String (".")));
        return;
    }

    if (! file.existsAsFile())
    {
        pushLive (file.getFullPathName() + " isn't there.");
        return;
    }

    const auto token = ++deckLoadToken[place];
    deckLoading[place] = file.getFileNameWithoutExtension();
    deckLoadItem[place] = item;   // a playlist's item brings its options along
    deckLoadByUser[place] = byUser;
    deckError[place].clear();
    deckPreparing[place] = nullptr;   // a video being read out for it is let go

    // A video: its picture is VLC's (the show's screen), its sound the deck's.
    if (VideoMedia::isVideoFile (file))
    {
        pushLive();
        readDeckVideo (deck, file, token);
        return;
    }

    pushLive();
    readDeckTrack (deck, file, {}, std::nullopt, token);
}

void MainComponent::readDeckVideo (int deck, const juce::File& video, int token)
{
    const auto place = static_cast<size_t> (deck);
    const auto rate = engine.getPlaybackRate();

    deckLoader.addJob ([safeThis = juce::Component::SafePointer<MainComponent> (this), closing = &liveClosing, deck, place, video, rate, token]
    {
        // Its picture needs VLC (asked here, not on the page's thread: VLC's first start looks over
        // all its plugins).
        if (const auto why = VideoMedia::problem(); why.isNotEmpty())
        {
            juce::MessageManager::callAsync ([safeThis, deck, place, token, why]
            {
                if (safeThis != nullptr && safeThis->deckLoadToken[place] == token)
                    safeThis->deckTrackRead (deck, nullptr, why, {}, std::nullopt);   // the playlist goes on past it
            });

            return;
        }

        // Its sound straight from it, lined up with its picture: about as fast as an audio file.
        auto opened = VideoSound::open (video);
        bool vlcReadsIt = opened.reader == nullptr && opened.error.isEmpty();
        auto error = opened.error;
        std::shared_ptr<const LiveShow::DeckTrack> track;

        if (opened.reader != nullptr)
            track = LiveShow::DeckTrack::load (std::move (opened.reader), video, rate, error, [closing] { return ! closing->load(); });

        // One VLC reads out first: longer than a deck takes is said at once, not after VLC has spent
        // minutes reading it all out.
        if (vlcReadsIt)
        {
            juce::String why;

            if (const auto probed = VideoMedia::probe (video, why); probed.has_value() && probed->seconds > LiveShow::longestTrackMinutes * 60.0)
            {
                vlcReadsIt = false;
                error = video.getFileName() + " is longer than an hour: a deck takes tracks up to an hour long.";
            }
        }

        juce::MessageManager::callAsync ([safeThis, deck, place, token, video, vlcReadsIt, track = std::move (track), error, info = opened.info]
        {
            if (safeThis == nullptr || safeThis->deckLoadToken[place] != token)
                return;   // gone, or a newer load for this deck won

            // A kind VLC lines up itself (or no FFmpeg here): VLC reads its sound out first (once:
            // it's kept, so the next time it's at once).
            if (vlcReadsIt)
            {
                safeThis->deckPreparing[place] = std::make_unique<VideoMedia::Preparation> (video, deckVideoWanted());
                safeThis->deckProgressShown[place] = 0.0;
                safeThis->pushLive();
                return;
            }

            safeThis->deckTrackRead (deck, track, error, video, info);
        });
    });
}

void MainComponent::readDeckTrack (int deck, const juce::File& source, const juce::File& standsFor,
                                   std::optional<VideoMedia::Info> video, int token)
{
    const auto place = static_cast<size_t> (deck);
    const auto rate = engine.getPlaybackRate();

    deckLoader.addJob ([this, safeThis = juce::Component::SafePointer<MainComponent> (this), deck, place, source, standsFor, video, rate, token]
    {
        // The loader's own formats, on its own thread.
        if (deckFormats.getNumKnownFormats() == 0)
            deckFormats.registerBasicFormats();

        juce::String error;
        auto track = LiveShow::DeckTrack::load (source, rate, deckFormats, error, standsFor, [this] { return ! liveClosing.load(); });

        juce::MessageManager::callAsync ([safeThis, deck, place, token, track = std::move (track), error, standsFor, video]
        {
            if (safeThis == nullptr || safeThis->deckLoadToken[place] != token)
                return;   // gone, or a newer load for this deck won

            safeThis->deckTrackRead (deck, track, error, standsFor, video);
        });
    });
}

void MainComponent::deckTrackRead (int deck, std::shared_ptr<const LiveShow::DeckTrack> track, const juce::String& error,
                                   const juce::File& standsFor, std::optional<VideoMedia::Info> video)
{
    const auto place = static_cast<size_t> (deck);
    deckLoading[place].clear();

    if (track == nullptr)
    {
        deckError[place] = error;

        // A playlist's item that can't be read is passed over (the playlist goes on).
        if (deckLoadItem[place] > 0)
            livePlayed.insert (deckLoadItem[place]);

        pushLive (error);
        return;
    }

    // Started meanwhile (its old track): it keeps playing that.
    if (engine.getLiveShow().decks[place].isPlaying())
    {
        pushLive (deckName (deck, liveMode) + " started playing meanwhile: " + track->title + " wasn't loaded.");
        return;
    }

    engine.setDeckTrack (deck, track);
    deckVideo[place] = video.has_value() ? DeckVideo { standsFor, *video } : DeckVideo {};
    deckLoaded (deck);
    syncScreenDeck (deck);
    markChanged();
    pushLive();
    pushDeckWave (deck);
}

void MainComponent::finishDeckVideo (int deck)
{
    const auto place = static_cast<size_t> (deck);
    const auto preparation = std::move (deckPreparing[place]);
    const auto prepared = preparation->getResult();
    const auto video = preparation->getVideo();

    // Its sound read out: onto the deck, its picture with it.
    if (prepared.ok() && prepared.sound.existsAsFile())
    {
        readDeckTrack (deck, prepared.sound, video, prepared.info, deckLoadToken[place]);
        return;
    }

    deckLoading[place].clear();
    const auto why = prepared.error == "Cancelled" ? juce::String()
                                                   : "Couldn't read " + video.getFileName()
                                                         + (prepared.error.isNotEmpty() ? ": " + prepared.error : juce::String ("."));
    deckError[place] = why;

    // A playlist's item that can't be read is passed over (the playlist goes on).
    if (deckLoadItem[place] > 0)
        livePlayed.insert (deckLoadItem[place]);

    pushLive (why);
}

void MainComponent::loadPad (int pad, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (pad, LiveShow::numPads))
        return;

    const auto place = static_cast<size_t> (pad);

    if (! file.existsAsFile())
    {
        pushLive (file.getFullPathName() + " isn't there.");
        return;
    }

    const auto token = ++padLoadToken[place];
    padLoading[place] = file.getFileNameWithoutExtension();
    pushLive();

    const auto rate = engine.getPlaybackRate();

    deckLoader.addJob ([this, safeThis = juce::Component::SafePointer<MainComponent> (this), pad, place, file, rate, token]
    {
        if (deckFormats.getNumKnownFormats() == 0)
            deckFormats.registerBasicFormats();

        juce::String error;
        const auto tooLong = file.getFileName() + " is longer than ten minutes: a pad takes a short sound (a jingle, a sting). Put a long one on a deck.";
        std::shared_ptr<const LiveShow::DeckTrack> sound;
        std::unique_ptr<juce::AudioFormatReader> reader (deckFormats.createReaderFor (file));

        // Its length first, as the file gives it: a long one is turned away before it's read into
        // memory (an hour of it held the loader, and Sonify closing, for minutes). Sonify closing stops it.
        if (reader != nullptr && reader->sampleRate > 0.0
            && static_cast<double> (reader->lengthInSamples) / reader->sampleRate > longestPadMinutes * 60.0)
            error = tooLong;
        else
            sound = LiveShow::DeckTrack::load (std::move (reader), file, rate, error, [this] { return ! liveClosing.load(); });

        if (sound != nullptr && sound->lengthSeconds > longestPadMinutes * 60.0)
        {
            error = tooLong;
            sound = nullptr;
        }

        juce::MessageManager::callAsync ([safeThis, pad, place, token, sound = std::move (sound), error]
        {
            if (safeThis == nullptr || safeThis->padLoadToken[place] != token)
                return;

            auto& self = *safeThis;
            self.padLoading[place].clear();

            if (sound == nullptr)
            {
                self.pushLive (error);
                return;
            }

            self.engine.setPadSound (pad, sound);
            self.markChanged();
            self.pushLive();
        });
    });
}

void MainComponent::loadSongDecks()
{
    // What a song left for the show, taken once: Song::restore leaves the song's own (a song opened,
    // an undo), clearSong leaves them empty (a new song, the project closed). Nothing of the song
    // before stays: the tab, the playlist and its place, the player, and every deck and pad the song
    // has nothing for (an empty entry, or none) emptied. What's kept next to Sonify's settings stays
    // as it is: the track library, the screens, the stream's setup.
    bool taken = false;

    // The song's tab and playlist.
    if (juce::var extras; engine.takeLiveShowExtras (extras))
    {
        taken = true;
        const auto mode = extras.getProperty ("mode", "live").toString();

        // (A stream going out stays where it's run from.)
        if (! (liveMode == "stream" && liveVideo != nullptr && liveVideo->isStreaming()))
            liveMode = mode == "stream" ? mode : juce::String ("live");   // a song saved in DJ show opens in Live & podcasts

        livePlaylist.clear();
        livePlaylistNext = 1;

        if (const auto* items = extras.getProperty ("playlist", {}).getArray())
        {
            for (const auto& item : *items)
            {
                if (! item.isObject() || item.getProperty ("path", {}).toString().isEmpty())
                    continue;

                livePlaylist.add (juce::JSON::parse (juce::JSON::toString (item)));
                livePlaylistNext = juce::jmax (livePlaylistNext, static_cast<int> (item.getProperty ("id", 0)) + 1);
            }
        }

        // The playlist's place, the player and what the decks did: the song's, from the start.
        deckItem.fill (0);
        deckAfter.fill (0);
        deckStale.fill (false);
        deckLoadItem.fill (0);
        deckByUser.fill (false);
        deckLoadByUser.fill (false);
        deckStarted.fill (false);
        deckHandedOver.fill (false);
        deckFinished.fill (false);
        deckEndedAt.fill (0.0);
        deckStartOrder.fill (0);
        livePlayed.clear();
        liveFront = 0;
        liveCued = -1;
        livePipVideo = juce::File();
        lastPlaylistFile = juce::File();

        // DJ show's decks: kept with the tab (the master has two while a playlist tab was left).
        const auto saved = static_cast<double> (engine.getLiveShow().settings.get (LiveShow::deckCount));
        liveDjDecks = static_cast<float> (static_cast<double> (extras.getProperty ("djDecks", saved)));
        applyLiveDecks();

        engine.keepLiveShowExtras (extras);
        applyLiveMute();
    }

    std::array<juce::File, LiveShow::maxDecks> files;

    if (engine.takeDeckFilesToLoad (files))
    {
        taken = true;

        for (int d = 0; d < LiveShow::maxDecks; ++d)
        {
            const auto place = static_cast<size_t> (d);
            const auto& file = files[place];

            // The same track already there (an undo of Fanan AI's request, say) stays as it is; a
            // load on its way there (the song before's) is forgotten.
            if (const auto current = engine.getDeckTrack (d); file != juce::File() && current != nullptr && current->file == file)
            {
                ++deckLoadToken[place];
                deckPreparing[place] = nullptr;
                deckLoading[place].clear();
                continue;
            }

            // Anything else there (or on its way) goes; then the song's own, when it has one.
            emptyDeck (d);

            if (file != juce::File())
                loadDeck (d, file);
        }
    }

    std::array<juce::File, LiveShow::numPads> padFiles;

    if (engine.takePadFilesToLoad (padFiles))
    {
        taken = true;

        for (int p = 0; p < LiveShow::numPads; ++p)
        {
            const auto place = static_cast<size_t> (p);
            const auto& file = padFiles[place];

            // A sound on its way (the song before's) is forgotten; the same sound already there stays.
            ++padLoadToken[place];
            padLoading[place].clear();
            const auto current = engine.getPadSound (p);

            if (file != juce::File() && current != nullptr && current->file == file)
                continue;

            // Anything else there goes; then the song's own, when it has one.
            if (current != nullptr)
                engine.setPadSound (p, nullptr);

            if (file != juce::File())
                loadPad (p, file);
        }
    }

    if (taken)
        pushLive();
}
