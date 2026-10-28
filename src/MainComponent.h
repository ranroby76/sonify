// D:\workspace\Fanan_web_hub\src\MainComponent.h

#pragma once

#include <set>

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "AudioEngine.h"
#include "Container.h"
#include "Cloud.h"
#include "ControlSurface.h"
#include "PadGrid.h"
#include "MidiLink.h"
#include "FananServer.h"
#include "AiTools.h"
#include "AiAgent.h"
#include "GuideAi.h"
#include "DeviceStudio.h"
#include "VoiceControl.h"
#include "PhoneRemote.h"
#include "PluginScanner.h"
#include "Edits.h"
#include "Library.h"
#include "MasteringAssistant.h"
#include "Preferences.h"
#include "RecordingAnalysis.h"
#include "PodcastExport.h"
#include "LiveController.h"
#include "LiveScreen.h"
#include "LiveStream.h"
#include "BlobMaker.h"
#include "SamplingStudio.h"
#include "SamplingRack.h"
#include "SfzLoops.h"
#include "SessionPlayers.h"
#include "StemSplitter.h"
#include "AudioRender.h"
#include "AudioToMidi.h"
#include "VideoMedia.h"
#include "VideoMp4.h"
#include "VideoScreen.h"
#include "DawProject.h"
#include "AiMix.h"
#include "MixAnalysis.h"

#include <map>
#include <optional>

class MasteringPlugin;
class CreatorStudio;

//==============================================================================
/**
    Hosts the web UI (WebView2 on Windows) and bridges it to the native AudioEngine.

    Web -> native events
        uiReady, uiShown   the page has drawn itself (the window's splash goes),
        noteOn {note, velocity}, noteOff {note}, allNotesOff, studioShown {id} (the studio in front: it alone hears MIDI),
        transport {action?, ppq?, bpm?, numerator?, denominator?, loop?, loopStart?, loopEnd?,
                   metronome?, metronomeVolume?}   (positions in quarter notes),
        addTrack {file?, pluginId?}, removeTrack {id}, selectTrack {id},
        trackSettings {id, name?, volume?, pan?, mute?, solo?, monitor?, recordArm?, input?, output?}, masterSettings {volume?, mute?},
        songSetup {inputs, outputs?, audition?, recordBits}   Song Setup's OK, songSetupDefault {inputs, outputs, audition}
        (Make Default), outputSettings {id, volume?, mute?}   a Sub output's fader and mute,
        loadPlugin {trackId, file, pluginId, position?, replaceInsertId?}, clearInstrument {trackId},
        removeInsert {trackId, insertId}, moveInsert {trackId, insertId, position},
        bypassInsert {trackId, insertId, bypass}, setParam {trackId, insertId, index, value},
        newSong {name, folder, bpm, numerator, denominator}, openSong {path?}, saveSong, saveSongAs,
        songView {view}, chooseSongFolder {folder}, dawProject {action: "import" | "export"},
        addLibraryFolder, removeLibraryFolder {folder}, rescanLibrary {full}, setFavorite {file, id, favorite},
        revealLibraryFile {file},
        deleteClip {trackId, clipId},
        setClipNotes {trackId, clipId, notes: [[pitch, start, length, velocity, muted?, operators?], ...], start?, length?,
                      offset?, pitchBend?: [[time, -8192..8191], ...], aftertouch?: [[time, 0..127], ...], seed?}
                      (a note's operators: MidiNote::toVar; the seed they roll from),
        clipEdits {edits: [{op: move|copy|range|split|delete|add, trackId, clipId, toTrackId?, start?, length?,
                            at?, offset?, notes?, pitchBend?, aftertouch?}, ...]}   applied in order, then one tracks update,
        transport action "record" toggles recording,
        addLibraryFiles, removeLibraryFile {file},
        choosePlugins {thirdParty}   the answer to the question at startup,
        aiSettings {deep?}   Fanan AI's Deep switch: every request to the strongest model,
        voiceSettings {enabled?, input?}   voice control on or off, and the song input its mic is on (-1: none),
        remoteSettings {enabled?, newKey?}   Sonify Remote (the phone pages) on or off; newKey: a new pairing key (phones scan again),
        aiCommandList {names}   the page's commands (Hub.commands), for Fanan AI's run_app_command,
        undoToggle   FL Studio's Ctrl+Z: undo, or redo right after an undo,
        masterSong {loudness: dynamic|streaming|loud, character: clean|warm|bright}   Song > Mastering
        Assistant, cancelMastering, masteringState (the page asks how it stands),
        masteringListen {original, sameLoudness}   comparing the master with the original,
        sessionSetup (Track > Add Session Players asks for each player's best instrument),
        sessionDetect {trackId, tempo?}   the chords a track plays; an audio track is listened to for its
        tempo, beat and chords (at the tempo given, when one is), sessionCancelDetect   listening stops,
        addSessionPlayers {style, intensity 0..1, players: [drums|bass|keys], instruments: {drums: {file, id} |
                           "none" (the Test Synth); absent: the best fit}, chords, startBar, times,
                           alignTrack?   the song takes that recording's tempo, its audio onto the bars},
        stemsState (Event > Split into Stems asks how it stands), splitStems {trackId, clipId, stems:
        [vocals|drums|bass|guitar|piano|other], muteOriginal}   guitar or piano splits with the six-stem
        model, cancelStems,
        addBusChannel {ids?}   a bus, and the tracks named playing into it (Add Bus for Selected Channels),
        tracksSettings {ids, and what trackSettings takes (mute, solo, recordArm, monitor, volume, pan,
        colour "#rrggbb"...)}   the same change to every selected channel, removeTracks {ids},
        duplicateTracks {ids}   each copy right under its original, moveTracks {ids, before (a track's id,
        0: the end)}, renameTracks {names: [{id, name}]},
        midiLinkState (the MIDI Link strip asks how it stands), midiLinkSet {source, target: {key,
        pluginId?, pluginName?, paramId?, paramName?}, mode: push|toggle|knob}   a chip dropped on a
        control, midiLinkMode {source, mode}, midiLinkClear {target (its id)}   the control's links go,
        midiLinkCancel   the links as they were before the last midiLinkSet, midiLinkMenu {action:
        save|load|clearAll}   (MainLink.cpp)

    Song Blocks (MainBlocks.cpp), one event: songBlocks {action: make {clipIds, name?} | edit {id, name?,
        colour?} | duplicate {id} | remove {id} | sequence {ids, label?} (the song's order) | build {at}
        (the song laid out on the timeline from that song position)}   each one undo step

    The song's video (MainVideo.cpp), one event: songVideo {action: import {file?, at? (a song position;
        absent: the playhead)} (no file: Sonify asks for one) | move {start} | remove | window {show} |
        export (Sonify asks where the new MP4 goes) | cancelExport}   import, move and remove are each one
        undo step; a video dropped as audio (importAudio) is imported the same way

    The Device Studio (MainDevices.cpp): deviceStudio {action: shown {shown} | state | new | open {file} |
        openSlot {trackId, fxId} (a device's window: Open in Plugins Creator Studio) | close | build | rename {name,
        description} | tryOn {trackId} | placeOn {trackId} (one undo step) | trash {file} | reveal | import |
        export | update {trackId, fxId} (that place to the newer version in My Devices)},
        deviceAiSend {text} (a request to Sonify AI about the open device; with none open, a new one),
        deviceAiStop, deviceAiClear, deviceMap {trackId, fxId, output (its id), target (MIDI Link's, or
        none: unmapped)}; midiFxAdd {type: "device", file (a .sonidev)} adds a device from My Devices
        (devices are made in the Plugins Creator Studio's Build with AI: new, open and openSlot show
        the device there)

    The Plugins Creator Studio (MainCreator.cpp), Sonify's own window over its page: creatorShow {x, y,
        width, height} (its page's area) | creatorHide | creatorTab {index} | creatorState

    The Analog Board (MainBoard.cpp), its own tab: board {action: shown {shown} | state | mixing {on}
        (the song mixed on the board, or on the Console; the first time, the strips start at the
        Console's faders) | set {strip (a track's id, or "master"), id (a setting's), value} |
        copyConsole (the strips take the Console's fader, pan, mute and solo) | reset {strip} |
        resetAll}   mixing, as the Console's faders: not undo steps

    The Live Show (MainLive.cpp), its own tab (Main is the show's while it shows, or the show
        plays): live {action: shown {shown} | state {have (the files whose waveforms the page has,
        deck by deck)} | mode {mode: dj | live | stream} (DJ show, Live & podcasts, Live
        streaming) | on {on} (Live streaming: Enable streaming; off, the stream gets the show's
        name and silence, the room still hears the show) | muteMics {on} | mutePlayback {on} (Live
        & podcasts and Live streaming: the live channels, or the player, decks and pads, silent;
        both start muted) | mute {on} (both) | controller {open} (the Controller, a window of its
        own: the cameras and the screen, presets, MIDI) | monitor {open} (the Monitor: the picture as
        the viewers see it, in a window over every other, never in the stream) | set {target: master | deck {deck} | channel {track},
        id (a setting's), value} (a deck's bpm: its tempo, its track following it; sync: the
        song's tempo) | deck {deck, do: play | pause | toggle | rewind (back to its start; playing,
        it plays on from there) | seek {seconds} | trackBpm {bpm}
        (the track's own, as the user knows it) | listen (its tempo and key heard again) | hotcue
        {index} (there) | cueAt {index, seconds} (its marker moved) | loopOn {index, on} (its
        markers placed, or the loop off) | loopAt {index, start, end} | loopPlay {index, on} (at
        its start, round it)} | load {deck, path, item} (a track on a stopped deck, from the
        playlist's item when there's one: DJ show) | eject {deck} | channel {track, on} (a track
        as one of the show's channels, or not) | playlistAdd {paths, at} (audio files and
        videos) | playlistMove {id, to} | playlistRemove {id} | playlistClear | playlistSet {id,
        field: sync | key | volume | timing, value} | playlistBpm {id, bpm} | playlistAuto {on}
        (it plays itself: the decks by turns) | playlistCue {id} (into the player, or next while
        one plays) | playlistSave | playlistOpen (a file of its own, chosen) | screen {open, kept}
        (the picture in picture, a window of its own over every other; kept: the user's own
        choice, for next time, not an opening for a video) | screenOutput {id, on} (the
        picture full screen on another of the computer's screens) | screenDisplays | fileDrag
        {left, top, width, height} (files from a folder on the computer dragged over the
        playlist: Sonify takes the drop there) | list {path} (a folder's audio files and
        videos, with the tempo and key of those known; none: the folder Browse chose) | browse
        (a folder for the library, kept) | analyze {path} (the tempo and key of a folder's
        tracks, heard) | pad
        {pad} (play it, or stop it) | padLoad {pad, path} | padClear {pad} | phones {output} (the
        headphones' Sub output, 0: none) | record {on, stems} | podcast {lufs, choose} (the last
        recording, or one chosen, made ready to publish) | reveal {path} | resetEffect {effect}
        (dynamicEq, compressor, drive, width or limiter: its controls back where they start) |
        effectsWatch {width} (the width's goniometer shows: the status brings its points) |
        streamPanel {open} (the Stream settings panel: its picture is made while it shows) |
        streamSettings {settings} (look, destinations, quality, sync: kept next to Sonify's
        settings, not the song; the sources are the Controller's) | streamDevices (the cameras
        and screens again) |
        streamLogo {choose} (or clear) | stream {on}}   kept with the song, not undo steps.
        Native -> web: live {kind: state (its front: the deck the playlist's player shows; its
        screen: the other screens, how many have the picture, and whether the picture in picture
        is open) | values (the settings, as MIDI Link moved them) | wave (a deck's waveform, once
        per track) | listing | preview {data, tall?} (the stream's picture, a JPEG, 4 a second
        while the panel shows; the vertical one too, tall, while a vertical destination is on) |
        fileDrop {paths, y}
        (dropped from a folder over the playlist, that far down the page)}, and the meters, the
        decks' positions and tempos, the pads playing, the recording's length, the podcast's
        progress and the stream's state in status while the tab shows

    The Sampling Studio (MainSampling.cpp), its own tab, Sampler Arena's Sampling Module: sampling
        {action: shown {shown} | state | config {settings} (Sampling::Settings::toVar, kept) | start
        {settings, replace} (VSTi or Chord Sampling; the Sample Builder builds) | stop | resume |
        finish | discard | skip | redo | reveal {path?} | setFolder | adHoc | adHocClear |
        presetSave {settings} | presetLoad (.smppreset) | audition {note, velocity, on, settings} |
        tryIt {sfz?} | builderLoad {note, row} | builderDrag {left, top, width, height, note, row} |
        builderInfo {paths} | loopLoad | loopOpen {path} | loopSelect {index} | loopFind |
        loopCandidate {index} | loopSet {start, end} | loopRemove | loopBatch | loopSave {as} |
        loopPlay | loopStop | loopXfade {ms, on} | loopAi {all} | blob {op: state | add | addFolder |
        addLast | remove {id} | clear | beside {on} | setFolder | samples {value} | start | stop |
        reveal {id?}} (SFZ to Blob, MainBlobs.cpp)}. Native -> web: sampling {kind:
        state (the folders, the rate and tempo, the MIDI outputs, the choices kept, the session:
        its state, step, shots [[note, velocity, layer, take, bpm, notes, chord]], marks, meters;
        the Builder's; what was made last) | progress | exists {name, count, folder} |
        builderFiles {paths, note, row} | builderInfo {files} | preset {settings, name} | notice
        {text} | loop (the Loop Editor: its regions, the sample, its loop and candidates, peaks) |
        loopPosition {position, playing} | tried {trackId} | problem {text} | blob {items, beside,
        folder, samples, running} (SFZ to Blob's list)}

    MIDI FX and MIDI tools (MainMidiTools.cpp)
        midiFxAdd {trackId, type, position?, params?, tool?, toolName?, generator?, open?}, midiFxRemove {trackId, fxId},
        midiFxMove {trackId, fxId, position}   (each one undo step), midiFxOn {trackId, fxId, on},
        midiFxParam {trackId, fxId, index, value},
        midiToolRun {requestId, devices: [{type, params}], notes: [[pitch, start, length, velocity, channel?]], from, to}
        the built-in devices applied to notes (song positions); the page commits what comes back,
        midiToolTrack {afterTrackId, name, label, drums?, clips: [{start, length, notes}]}   a tool's notes on a
        new track under one (one undo step), magentaAnswer {trackId, fxId, window, length, notes},
        midiToolsList, midiToolInstall {tool}, midiToolRemove {id}

    File > Settings (MainSettings.cpp)
        getSettings, setPreference {luminance?, contrast?, accent?, keymap?, discoverable?, autosave?, autosaveMinutes?,
                                    releaseInBackground?, timecodeRate?},
        chooseUserDataFolder, resetUserDataFolder,
        audioSetupGet {rescan?}, audioSetupSet {type? | device?, output?, input?, rate?, buffer?, inputChannels?: [bool]},
        audioControlPanel, audioTestTone, midiPortEnabled {id, enabled}   a port turned off stays off,
        midiDeviceSave {device}   (kind keyboard, controller, drums, instrument or surface; a surface's
        protocol mackie, extender or hui), midiDeviceRemove {id},
        replaceLibraryFolder {folder}, addDefaultLibraryFolders

    trackId 0 is the Main channel. insertId 0 is a track's instrument.

    Native -> web events
        status   20 times a second once the page reports uiReady (with the Sub outputs' meters; volume
                 and pan for tracks automation or a control surface moves, masterVolume for Main's)
        runCommand {command}   a page command a control surface asked for (zoomIn, zoomOut)
        aiRun {command} | {event, payload}   Fanan AI runs a page command, or has the page send an event as it would (AiTools)
        tracks   whenever the track list, a plugin or the Main mute changes (tracks, fxChannels and busChannels,
                 each with the bus it plays into; includes the Main channel, and
                 the song's inputs and outputs with the device's channels and what Reset to Default goes back to)
        library  whenever the Browser's plugin list changes: plugins (with type, features,
                 favorite, missing, last used, Folder-view group), the Plugin Folders, scan progress
        song     the song's name, file and unsaved changes, recent songs; with "opened" and its
                 view settings after New or Open; songFolder  a folder picked for new songs
        param    reply to setParam
        voiceState  {offered, enabled, why, stage: off|listening|command, heard, message, input, inputs}
        remoteState {offered, enabled, why, connected, problem, url, email, localUrl, localProblem, localSeen}
        settings    Sonify's settings (Preferences::toVar) with the MIDI ports (inputs {id, name, enabled,
                    busy (another app holds it), link (usb, bluetooth, virtual or "")}, outputs {id,
                    name}), the usual plugin folders, the computer's name, any MIDI output that couldn't
                    be opened, and why a control surface can't work (surfaceProblems); again whenever a
                    port comes, goes, or is turned on or off
        audioSetup  the audio device only: driver types, devices, rates, block sizes, channels, latency
        mastering   {state: idle|rendering|analysing|done|failed|cancelled, progress, error, insertId (Sonify
                    Mastering on Main, 0: none), result (what the assistant found), original, sameLoudness}
        sessionSetup    {best: {drums, bass, keys: {file, id, name} or null (the Test Synth)}}
        sessionChords   {trackId, chords (a progression, "" when it plays no notes), startBar, bars}; for an
                        audio track also {audio, listening, progress, tempo (the recording's), songTempo,
                        follows (it plays to the song's beat already), steadiness 0..1, error}
        sessionPlayers  {ok, error, trackIds, notes}   the answer to addSessionPlayers
        stems       {state: idle|downloading|splitting|done|failed|cancelled, progress, error, modelReady,
                    problem (why stems can't be split on this computer, or ""), trackId, clipId, stems, trackIds (the new ones)}
        selectTracks {ids}   the page selects these (the copies duplicateTracks made)
        midiFxTypes {types: [{id, name, description, magenta, params: [{id, name, min, max, value, unit,
                    choices?}]}], maxDevices}   at uiReady; tracks carry midiFx: [{id, type, name, on, params, tool?}]
        midiFxAdded {trackId, fxId?, open?, error?}, midiToolResult {requestId, notes?, error?},
        magentaRequest {trackId, fxId, tool, mode, temperature, inKey, window, length, bpm, key: {root, notes},
                    notes}   a window a Magenta device heard,
        midiTools {installed: [tool definitions], magenta (magenta.js built in), folder, error?},
        midiToolProgress {id, fraction}, midiToolInstalled {id, ok, error}
        songBlocks  {blocks: [{id, name, colour, length, bars, parts: [{trackId, start, length, audio,
                    notes: [[pitch, start, length]] (block positions)}]}], sequence: [ids]}   whenever they
                    change (and at uiReady); songBlockMade {id, action} (a pad just made or copied),
                    songBlocksBuilt {from, to, placed, skipped, notice}
        songVideo   {file, name, start, end (song positions), seconds, width, height, missing, thumbnails: {url,
                    count, columns, cellWidth, cellHeight, every} or null, preparing: {name, progress} or null,
                    problem (why video can't play on this computer, or ""), window (the Video window shows)}
                    whenever any of it changes (and at uiReady)
        videoExport {state: running|done|failed|cancelled, progress, file, error}   Export Video
        deviceStudio {available, why, open, library: [{file, id, name, description, version, built}], folder,
                    and the open device's file, id, name, description, version, built, upToDate, hasCode,
                    building, lastBuild {ok, text}, playsOn, device (its build: params, outputs), files,
                    aiBusy, tryTrack}   whenever it changes; deviceStudioMessage {message | error},
        deviceAiState   as aiState, for the Device Studio's conversation,
        deviceLive  {places, count, fault, load, log}   the open device playing, while the Studio shows;
        creator     {tab, devices}   the Plugins Creator Studio's tab, whenever it changes; runCommand
                    {command} for Sonify's keys pressed in its window and its audio device pill;
        tracks' midiFx items of type "device" also carry device (its build: deviceId, name, version,
        hash, params, outputs), maps {output id: target}, missing, fault
        board       {mixing, specs: {strip: [...], master: [...]} (each setting: id, name, section, kind
                    knob|toggle|choice|fader, min, max, default, unit, choices?), strips: [{id (the track's),
                    values: {<setting>: value}}], master: {<setting>: value}}   when any of it changes (with
                    the tracks); status carries board {strips: {<track id>: meter}, master: meter} while
                    the board's tab shows, a meter [vu left, vu right (dBFS RMS), peak left, peak right
                    (dBFS), reduction (dB)]; tracks carries boardMixing
        midiLink    {recent: [{key, label, device, channel, kind: cc|note|pb, link: {target, mode} or null}] (the chips,
                    newest first), links: [{source, label, device, channel, target, mode}], touched: {pluginId,
                    pluginName, paramId, paramName, where} or null (Last Touched), message (a map that couldn't
                    be loaded or saved)}   when any of it changes
*/
class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener,
                            private ControlSurfaces::Host,
                            private MidiLink::Host,
                            private Containers::Host,
                            private LiveControl::Host
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void paintOverChildren (juce::Graphics& g) override;
    void resized() override;

    /** Runs next once the song's changes are safe: nothing unsaved, saved now, or the user
        chose not to save. Cancel (or a cancelled Save As) runs nothing. For New, Open and quit. */
    void whenSongIsSafe (std::function<void()> next);

private:
    juce::WebBrowserComponent::Options createWebOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    std::optional<juce::WebBrowserComponent::Resource> getScreenResource (const juce::String& path);
    void openPluginScreen (int channel, int slot);
    void forwardScreenMessages();
    juce::String channelName (int channel) const;
    static juce::var automationToVar (const std::vector<AutomationLane>& lanes);
    juce::File defaultMixdownFolder() const;
    void exportMixdown (const juce::var& event);
    void updateMixdown();

    //==============================================================================
    // Song > Mastering Assistant: the song rendered through Main (an earlier mastering bypassed),
    // listened to on a thread of its own, then Sonify Mastering set up as Main's last insert with
    // Main's fader at 0 dB. One undo step. Nothing else renders meanwhile.
    enum class MasteringStage { idle, rendering, analysing };
    MasteringStage masteringStage = MasteringStage::idle;
    std::unique_ptr<MasteringAssistant> masteringAssistant;
    MasteringAssistant::Options masteringOptions;
    juce::File masteringRender;                 // the song as Main played it, in the temp folder
    int masteringBypassed = 0;                  // an earlier mastering, bypassed while it renders
    int songGeneration = 0, masteringGeneration = 0;   // a song opened meanwhile isn't mastered

    void startMastering (const juce::var& event);
    void cancelMastering();
    void updateMastering();
    void applyMastering (const MasteringAssistant::Result& result);
    void restoreMasteringBypass();
    void emitMastering (const juce::String& state, double progress = 0.0, const juce::String& error = {});
    juce::String masteringStateName() const;
    double masteringProgress() const;
    int findMasteringInsert() const;
    MasteringPlugin* findMasteringPlugin();
    void setMasteringListen (bool original, bool sameLoudness);
    void chooseMixdownFolder (const juce::String& current);
    void emitMixdown (const juce::String& state, const juce::File& file, double progress, const juce::String& error);
    bool revealMixdown = false;

    //==============================================================================
    // Freezing a track (its snowflake): its sound rendered as a mixdown of its own, into the
    // song's Media folder (Frozen), then played in its place. Not an undo step: it thaws by
    // itself when what it plays changes (an undo too). Renders no saved song names go with the
    // song (AudioEngine::discardRenders).
    int freezingTrackId = 0;                    // the track rendering now (0: none)
    int freezeGeneration = 0;                   // in which song
    void startFreezing (int trackId);
    void stopFreezing (int trackId);
    void updateFreeze();

    // Export Clip to Audio (an audio or MIDI clip's right-click): the clip alone, as its track plays
    // it (its instrument, its clip effect, its inserts), rendered as a freeze is into a float WAV,
    // then written as a 24-bit WAV with its tempo (an ACID chunk) into the folder chosen.
    struct ClipExport
    {
        int trackId = 0, clipId = 0;            // 0: none running
        juce::File render, file;
        double bpm = 0.0, beats = 0.0;
        int numerator = 4, denominator = 4;
    };

    ClipExport clipExport;
    std::unique_ptr<juce::FileChooser> clipExportChooser;
    void exportClip (int trackId, int clipId, bool chooseFolder);
    void startClipExport (int trackId, int clipId, const juce::File& folder);
    void updateClipExport();

    // Merge Clips and Bounce to New Track (as Studio One's Bounce Selection and Bounce to New
    // Track), rendered as Export Clip to Audio is: the clips alone as their track plays them.
    // Merge: audio clips on a track, the first selected to the last (the clips between them
    // too), each with its own effect, gain, fades and Silence Areas but without the track's
    // inserts, into one clip in their place. Bounce: the selected clips (audio or MIDI) through
    // the track (its instrument and its inserts, and what rings on after) onto a new audio track
    // under it, the clips muted. Each is one undo step.
    struct ClipBounce
    {
        enum class Kind { merge, newTrack };
        Kind kind = Kind::merge;
        int trackId = 0;                        // 0: none running
        std::vector<int> clipIds;               // rendered: replaced (merge) or muted (bounce)
        double startPpq = 0.0, endPpq = 0.0;
        juce::File file;
        int generation = 0;                     // the song it was started in
    };

    ClipBounce clipBounce;

    //==============================================================================
    // Recording automation (MainAutomation.cpp): a track in Touch, Latch or Write records the
    // controls taken hold of while the song plays (its fader, its pan, its plugins' parameters,
    // from the page, a control surface, MIDI Link or a plugin's own window), looked at 60 times a
    // second, into its envelopes: each one kept when it's let go (Touch) or when the song stops
    // (Latch, Write), as one undo step.
    struct AutomationWrite
    {
        int trackId = 0;
        AutomationTarget target;
        std::vector<AutomationPoint> points;       // what was recorded, from where it started
        double from = 0.0;                         // where it started (song position)
        double lastValue = 0.0;
        double lastChange = 0.0;                   // when it last moved (ms)
        int generation = 0;                        // the song it was recorded in
    };

    std::vector<AutomationWrite> automationWrites;
    bool automationWasPlaying = false;
    double automationLastPpq = 0.0;

    struct AutomationTimer final : public juce::Timer
    {
        explicit AutomationTimer (MainComponent& m) : owner (m) {}
        void timerCallback() override   { owner.updateAutomationRecording(); }
        MainComponent& owner;
    };

    AutomationTimer automationTimer { *this };

    /** A control taken hold of (it's recorded when its track records automation and the song plays). */
    void automationTouched (int trackId, const AutomationTarget& target);
    void automationParamTouched (int trackId, int insertId, int parameterIndex);
    void updateAutomationRecording();
    bool currentControlValue (int trackId, const AutomationTarget& target, double& value);
    void keepAutomationWrite (AutomationWrite& write, double to, bool joinStep);
    void finishAutomationWrites (int trackId = 0);   // 0: every track
    void mergeAudioClips (const juce::var& event);
    void bounceClipsToNewTrack (const juce::var& event);
    void startClipBounce (ClipBounce::Kind kind, int trackId, std::vector<int> clipIds);
    void updateClipBounce();
    bool stripSilence (int trackId, int clipId, const MidiClip::Silence& silence, juce::String& problem);   // one undo step with the gesture's
    void sendTransients (const juce::var& event);

    //==============================================================================
    // Track > Add Session Players: a drummer, a bass player and a keys player on new tracks,
    // playing a chord progression in a style (SessionPlayers). One undo step, with the song's new
    // tempo and its audio moved when they back a recording with a tempo of its own.
    void sendSessionSetup();
    void detectSessionChords (int trackId, double tempo);
    void addSessionPlayers (const juce::var& event);

    // Backing for a recording: an audio track listened to on a thread of its own.
    std::unique_ptr<RecordingAnalysis> recordingAnalysis;
    int heardTrack = 0;                         // the audio track listened to
    juce::String heardLayout;                   // the song's audio and tempo it was heard against
    RecordingAnalysis::Result heard;            // what was heard
    RecordingAnalysis::Alignment heardAlignment;
    double heardProgressSent = -1.0;

    void updateRecordingAnalysis();
    void cancelRecordingAnalysis();
    void emitHeard (bool listening, double progress);

    //==============================================================================
    // Event > Split into Stems: an audio clip separated on this computer (StemSplitter), its
    // model downloaded the first time (the six-stem one for guitar or piano); the stems on new
    // audio tracks under it. One undo step.
    enum class StemStage { idle, downloading, splitting };
    StemStage stemStage = StemStage::idle;
    std::unique_ptr<StemSplitter::Download> modelDownload;
    std::unique_ptr<StemSplitter> stemSplitter;
    StemSplitter::Job stemJob;
    int stemTrack = 0, stemClip = 0, stemGeneration = 0;
    double stemStart = 0.0, stemGain = 0.0, stemFadeIn = 0.0, stemFadeOut = 0.0;
    bool stemMute = true;
    double stemProgressSent = -1.0;

    juce::String splitStems (int trackId, int clipId, const juce::StringArray& stems, bool muteOriginal);   // "" when it starts
    void startStemSplitter();
    void cancelStems();
    void updateStems();
    void finishStems (const StemSplitter::Result& result);
    juce::String stemStateName() const;
    double stemProgress() const;
    void emitStems (const juce::String& state, double progress = 0.0, const juce::String& error = {}, const juce::Array<juce::var>& trackIds = {});

    //==============================================================================
    // The Audio menu (the audio editor's, MainAudio.cpp): Sync Tempo, Follow Tempo, Pitch,
    // Reverse and Back to the Original render a new file from the clip's original (AudioRender);
    // Normalize and Gain set the clip's volume; Audio to MIDI puts what it plays on a new track
    // (AudioToMidi); Detect Tempo listens for the clip's own tempo, and Analyse for its tempo, key
    // and chords (RecordingAnalysis; they answer with audioTempo and audioAnalysis, and change
    // nothing). One job at a time, each change one undo step. Clips that follow the song's tempo
    // are rendered again by themselves whenever it changes under them (no undo step: undoing the
    // tempo brings their earlier render back).
    struct AudioJob
    {
        juce::String op;                        // "syncTempo", "followTempo", "pitch", "reverse", "original", "toMidi", "detectTempo", "analyse"
        int trackId = 0, clipId = 0;
        int generation = 0;                     // the song it was for
        bool automatic = false;                 // a follow-tempo clip brought up to date
        double tempo = 0.0, semitones = 0.0;
        bool reverse = false, follow = false;
        juce::File source;                      // the original's window it renders from
        double sourceOffset = 0.0, sourceLength = 0.0;
        double leadBeats = 0.0, beats = 0.0;    // a follower: the window's beats before the clip, and the clip's
        AudioRender::Job render;                // the render it runs
        AudioToMidi::Mode mode = AudioToMidi::Mode::notes;
        MidiClip clip;                          // the clip as it was when the job started
        juce::uint32 mapGeneration = 0;         // and the tempo map

        // The Stretcher (op "stretch"): the stretch of what the clip shows, from .. to (seconds)
        // made newLength long; keepEnd: its end stays where it is in the song and what's before
        // it moves (the clip's start by shiftSeconds), else what's after it moves.
        double from = 0.0, to = 0.0, newLength = 0.0;
        bool keepEnd = false;
        // Warp markers (ops "warpMove", "warpRemove"): the marker at `at` (seconds of what the
        // clip shows) moved to newAt, between the markers either side of it (from, to); or taken away.
        double at = 0.0, newAt = 0.0;
        double shiftSeconds = 0.0;
        std::vector<std::pair<double, double>> warp;   // the render's time map the clip keeps (MidiClip::warp)
    };

    AudioJob audioJob;
    std::unique_ptr<AudioRender> audioRender;
    std::unique_ptr<AudioToMidi> audioToMidi;
    std::unique_ptr<RecordingAnalysis> audioTempo;
    double audioProgressSent = -1.0;
    juce::String followStamp;                   // the song's renders and tempo as last looked at
    juce::uint32 followSeenAt = 0;              // when that changed
    std::optional<AudioJob> followNext;         // a render to bring up to date, once things settle
    std::set<juce::String> followFailed;        // renders that couldn't be made: not tried again and again
    juce::String followFailedFor;               // the tempo map and song they failed with

    void handleAudioOp (const juce::var& event);
    juce::String startAudioOp (const AudioJob& job);           // "" when it started (or was done at once)
    void cancelAudioJob (bool automaticOnly);
    void updateAudioJobs();
    void finishAudioRender (const AudioRender::Result& result);
    void finishAudioToMidi (const AudioToMidi::Result& result);
    void sliceToSampler (const AudioJob& job, const juce::var& cuts);   // Slice to Sampler (MainAudio.cpp)
    void emitAudioJob (const AudioJob& job, const juce::String& state, double progress = 0.0, const juce::String& error = {}, int newTrackId = 0);
    void bringFollowersUpToDate();

    bool changeAudioClip (const AudioJob& job, const std::function<void (MidiClip&)>& change, const juce::String& name);
    const MidiClip* findAudioClip (int trackId, int clipId) const;
    double zeroCrossingNear (int trackId, int clipId, double ppq);   // Snap To Zero Crossings: the nearest within 10 ms
    AudioRender::Window sourceWindowOf (const MidiClip& clip);

    /** What the clip shows as a time map: (its seconds, seconds of its original's window as it plays),
        and that window. False for one that follows the tempo. */
    bool shownTimeMap (const MidiClip& clip, AudioRender::Window& window, std::vector<std::pair<double, double>>& map);
    AudioRender::Job renderJobFor (const MidiClip& clip, const AudioRender::Window& window, bool reverse, double semitones, double tempo,
                                   const juce::String& name, double leadBeats = 0.0);
    bool clipStillAsItWas (const AudioJob& job) const;
    void forgetUnusedAudio (const juce::String& file);
    void lookAtFollowersAgain();

    //==============================================================================
    // Fanan's content (Cloud: no account needed), and the Fanan account with its credits and
    // Sonify AI (FananServer: the same server, accounts and credits as Stella AI Studio).
    std::unique_ptr<Cloud> cloud;
    std::unique_ptr<FananServer> server;
    bool signingIn = false;            // asking the server for a sign-in link
    bool waitingForSignIn = false;     // sent to the browser to sign in, not back yet
    int signInAttempt = 0;             // a wait ends by itself after a while

    void emitCloud (const juce::String& event, const juce::var& data);
    void pushAccount (const juce::String& message = {}, const juce::String& error = {});
    void signIn (bool thenBuy, const juce::String& emailHint);
    void signOut();
    void buyCredits();

    /** A track's sends for the page, each named after where it goes (gone ones left out). */
    juce::var sendsToVar (const Track& track);

    //==============================================================================
    // Fanan's content: what Fanan publishes, installed into Documents\Fanan Web Hub\Fanan Content
    std::map<juce::String, juce::var> contentItems;   // the last listing, by id
    juce::var installedContent;                        // id -> { version, path, kind, title }

    static juce::File contentFolder();
    void loadInstalledContent();
    void saveInstalledContent() const;
    void listContent();
    void emitContent (const juce::String& error = {});
    void installContent (const juce::String& id);
    void previewContent (const juce::String& id);
    void openContent (const juce::String& id);

    //==============================================================================
    // MIDI FX and MIDI tools (MainMidiTools.cpp): the devices in front of instrument tracks'
    // instruments, the tools applied to notes, the Magenta device's windows to and from the
    // page, and the Magenta tools installed into Fanan Content\MIDI Tools.
    void handleMidiFx (const juce::String& what, const juce::var& event);
    void runMidiTool (const juce::var& event);
    void addMidiToolTrack (const juce::var& event);
    void updateMagenta();
    void answerMagentaWindow (const juce::var& event);
    void emitMidiFxTypes();
    void listMidiTools (const juce::String& error = {});
    void installMidiTool (const juce::var& tool);
    void removeMidiTool (const juce::String& id);
    static juce::File midiToolsFolder();
    std::optional<juce::WebBrowserComponent::Resource> getMidiToolResource (const juce::String& path);

    //==============================================================================
    // Web browsing tabs: each a WebView2 of its own over the page's content area, with no
    // bridge to the engine. Studio (the DAW) is the page itself.
    class WebTabView;
    class WebTabLayer;

    // Sonify's own page (webView): it never leaves Sonify's own address, so no page from
    // outside ever gets its bridge to the engine (a link dropped on the window, say).
    class HubPageView;

    struct WebTab
    {
        int id = 0;
        juce::String url, title;
        std::unique_ptr<WebTabView> view;   // made when the tab is first shown
    };

    // The layer is declared first so it outlives the tabs: their pages close inside it.
    std::unique_ptr<WebTabLayer> webTabLayer;   // the tabs' own window, over the Hub's page
    std::vector<WebTab> webTabs;
    int nextWebTabId = 1;

    WebTab* findWebTab (int id);
    int openWebTab (const juce::String& url, bool activate);
    void showWebTab (int id, juce::Rectangle<int> bounds);
    void hideWebTabs();
    void navigateWebTab (int id, const juce::String& url);
    void closeWebTab (int id);
    void webTabLoading (int id, const juce::String& url);
    void webTabLoaded (int id, const juce::String& url);
    void webTabError (int id, const juce::String& error);
    void emitWebTabState (const WebTab& tab, bool loading, const juce::String& error = {});
    void saveWebTabs() const;
    void restoreWebTabs();
    std::set<std::pair<int, int>> openScreens;   // web screens the page shows: channel, slot

    void handleUiReady();
    void timerCallback() override;
    void parentHierarchyChanged() override;

    // While the page loads (WebView2 starting, the page's files), the window shows Fanan's and
    // Sonify's logos with a spinner: a window of its own over the page's, until the page has
    // drawn itself.
    class Splash;
    std::unique_ptr<Splash> splash;
    void placeSplash();
    void dismissSplash (int milliseconds);

    // The ON/OFF on every plugin window's header (PluginHeader.h): the engine says and switches
    // it for each plugin in the song (AudioEngine::getPluginOn); the page's windows send pluginOn.
    int pluginSwitcher = 0;
    void switchPlugin (const Plugin& plugin, bool on);

    //==============================================================================
    // The song as a document: its file, unsaved changes, recent songs.
    juce::String songTitle() const;
    void updateTitle();
    void markChanged();
    void loadRecentSongs();
    void rememberRecent (const juce::File& file);
    void pushSong (bool opened = false, const juce::String& busy = {});
    void showSongMessage (const juce::String& title, const juce::String& message);
    bool writeSong (const juce::File& file);
    void saveSong (std::function<void()> afterSaving);
    void chooseSongLocation (std::function<void()> afterSaving);
    void chooseSongToOpen();
    void openSong (const juce::File& file);
    void newSong (const juce::var& event);

    /** File > Close Project: the song goes (after asking to save changes) and the Start page returns. */
    void closeSong();

    /** File > Import DAWproject... and Export DAWproject... (MainDawProject.cpp, DawProject.h).
        "dawProject" events from the page: { action: "import" | "export" }. An import is a new
        song (its own folder), opened once it's written; what didn't come across is said then. */
    void handleDawProject (const juce::var& event);
    void chooseDawProjectToImport();
    void importDawProject (const juce::File& source);
    void chooseDawProjectExport();
    void exportDawProject (const juce::File& target);
    static juce::var keptToPage (const juce::var& kept);   // a channel's placeholders, as the Console shows them

    // No project open: the Start page is where the user is (at launch, and after Close Project).
    // Also tells a page that loads again (it shouldn't) where to be.
    bool noProject = true;

    // At startup the page first asks whether to load third-party plugins (checked by the scan,
    // with its page, before the welcome page) or use only Sonify's own. Asked once a run: the
    // scan waits for the answer, and a page loading again later keeps it.
    bool pluginChoiceMade = false;
    void choosePlugins (bool thirdParty);

    //==============================================================================
    // Block internet: when on, nothing in the Hub reaches the network — the Fanan account and
    // Sonify AI, Fanan's content, web browsing tabs, plugins' web screens and previews. Kept Hub-wide.
    bool internetBlocked = false;

    //==============================================================================
    // Sonify AI driving the DAW (through Fanan's server). One request is one undo step.
    std::unique_ptr<AiTools> aiTools;
    std::unique_ptr<AiAgent> ai;
    juce::String aiSongBefore;   // the song as it was when the request began

    juce::String songSnapshot();
    void restoreSnapshot (const juce::String& song);
    void pushAi();
    void addAiAccount (juce::var& state) const;

    //==============================================================================
    // The Guide (MainGuide.cpp): Sonify's user guide, and Fanan AI answering questions about it.
    std::unique_ptr<GuideAi> guideAi;

    void setupGuide();
    void handleGuide (const juce::var& event);
    void pushGuide();

    //==============================================================================
    // The Analog Board (MainBoard.cpp): the song mixed on a second, analog-style mixer.
    void handleBoard (const juce::var& event);
    void pushBoard();
    juce::var boardMeters();
    bool linkBoardSetting (const juce::StringArray& parts,
                           std::function<void (AnalogBoard::Settings<AnalogBoard::numStripParams>&, int)> strip,
                           std::function<void (AnalogBoard::Settings<AnalogBoard::numMasterParams>&, int)> master,
                           std::function<void (MasterSection::Settings&, int)> mastering);
    void linkMoveBoard (const juce::StringArray& parts, float position);
    void linkSetBoard (const juce::StringArray& parts, bool on);
    bool linkIsOnBoard (const juce::StringArray& parts);

    bool boardShown = false, boardDue = false;

    // The Mastering section (MasterSection.h, on Main after its fader): its window over the page,
    // opened from the board, Main's strip or Song > Mastering Section; the same "board" events
    // with strip "mastering". File > Master a File...: a new song of one audio file, mastered.
    bool masteringShown = false;
    juce::var masteringMeters();
    juce::String masteringModeNow();                 // Auto resolved: "mixAndMaster" or "masterOnly"
    void masterFile();
    std::unique_ptr<juce::FileChooser> masterFileChooser;
    double boardPushedMs = 0.0;

    //==============================================================================
    // The Live Show (MainLive.cpp): two or four decks, the tracks marked as its channels, the pads,
    // the master and its effects, on Main in place of the song while its tab shows (or it plays). Used
    // three ways (its tabs): DJ show, Live & podcasts, Live streaming. Tracks are read into memory on
    // a thread of their own (the newest load for a deck wins), and listened to on another for their
    // tempo, first beat and key (kept, with their hot cues and loops, in its track library).
    void handleLive (const juce::var& event);
    void handleLiveDeck (int deck, const juce::var& event);
    void pushLive (const juce::String& note = {});
    void pushDeckWave (int deck);
    juce::var liveMeters();
    void loadDeck (int deck, const juce::File& file, int item = 0, bool byUser = true);   // byUser false: the playlist's zipper
    void loadPad (int pad, const juce::File& file);
    void loadSongDecks();                    // the tracks a song's decks had (and its pads), once it's open

    LiveShow::TrackLibrary& liveTracks();
    void saveLiveTracksSoon();
    void deckLoaded (int deck);              // what's known of its track, or listening to it
    void listenToDeck (int deck);
    void heardDeck (int deck, const juce::File& file, LiveShow::TrackInfo found);
    void keepDeckInfo (int deck);            // its tempo, first beat, hot cues and loops: into the library
    void analyseLiveFolder (const juce::File& folder);
    void analyseLiveFiles (std::vector<juce::File> files, const juce::String& what);   // the ones not known yet
    void pushLiveListing (const juce::String& path);

    /** A deck's speed from its tempo (the player's BPM): its track follows it (Track Sync) when its
        own tempo is known, at half or double when that's nearer; else it plays as recorded, and the
        player's BPM shows the track's. */
    void applyDeckTempo (int deck);
    void setDeckSynced (int deck, bool synced);   // SYNC: the player's BPM is the song's (the DAW's)
    void followSong (int deck);              // SYNC on, the track following the song's tempo (no beat lined up)
    void setLiveDeckValue (int deck, int index, float value);   // a deck's setting (the page, MIDI Link), and what follows it
    double songTempo();                      // the DAW's tempo, where the song stands
    void followSongTempo();                  // the status timer: synced decks follow a tempo change
    int liveLeadDeck (int except) const;     // the deck the others follow: synced, playing, its tempo known
    void alignDeckBeat (int deck, int lead); // its beats onto the lead deck's

    // Its tab (DJ show, Live & podcasts, Live streaming), Enable streaming and the mutes, and who has Main.
    void setLiveMode (const juce::String& mode);
    void setLiveSwitch (const juce::String& which, bool on);   // "stream" (Enable streaming), "mics", "playback", "both" (the mutes)
    void applyLiveMute();                    // the mutes (live & podcasts, streaming), and what goes to the stream
    void applyLiveDecks();                   // DJ show: its two or four decks; the playlist tabs: two, one player
    void updateLiveOwnership();              // Main is the show's while its tab shows, or it plays (its playlist between two tracks too)
    bool liveShowPlaying();                  // a deck or pad playing, the night recording, the stream, the playlist between two tracks
    void keepLiveExtras();                   // the mode and playlist, kept with the song

    // The playlist (Live & podcasts, Live streaming): tracks in order, each with its options.
    void handlePlaylist (const juce::String& action, const juce::var& event);
    juce::var playlistToVar();
    juce::var* findPlaylistItem (int id);
    void applyItemToDeck (int deck);         // the item a deck plays: its key, volume and Track Sync
    void placeDefaultCues (int deck);        // hot cues not placed yet: at the start, a little apart

    /** The status timer, playlist modes with Auto on: the decks filled by turns (the zipper: deck 1,
        deck 2, deck 1...), and the next one started as each ends, by its timing (below 0: a
        crossfade that long, before the end; above: a pause after it). */
    void updatePlaylist();
    int nextUnplayedAfter (int itemId);      // in the list's order (0: none left)
    int playlistIndexOf (int itemId) const;  // its place in the list (-1: not there)
    int listPlaceOf (int deck);              // where the list goes on after a deck's track: its item, or where it was (0: the top)
    void emptyDeck (int deck, bool keepLoad = false);   // its track off it, a load on its way forgotten (or kept) (Eject: it isn't playing)
    void clearPlayer();                      // the list emptied: the player's two decks emptied too (one playing fades out first)
    void dropGoneFromPlayer (int gone, int before);   // an item off the list: a deck holding or bringing it emptied, unless it plays
    void choosePlaylistFile (bool saving);   // Save... / Open...: where
    void savePlaylist (const juce::File& file);
    void openPlaylist (const juce::File& file);
    void restoreTrackInfo (const juce::File& file, const LiveShow::TrackInfo& saved);   // a playlist's markers for a track
    static juce::File libraryFolderFile();   // where the folder Browse chose is kept (AppData\Roaming\Fanan\Sonify)
    static juce::String keptLibraryFolder(); // that folder, the library's at every start ("": none chosen)
    static bool keepLibraryFolder (const juce::File& folder);

    // The playlist's tabs: one player (decks 1 and 2 by turns, the deck behind only crossfading the
    // next track in), videos on the playlist (their sound read straight from them for the deck:
    // VideoSound.h; the kinds VLC lines up itself, VLC reads out), the show's screen (LiveScreen.h:
    // the picture in picture, a window of its own over every other; full screen on other screens),
    // and files dropped in from a folder on the computer.
    int liveFrontDeck();                     // the deck the player shows: the one playing (the newer), or the one to play
    bool liveDeckDone (int deck);            // it led on to the next, or played to its end
    bool playlistBetweenTracks();            // Auto: one has played to its end, the next is still to come (the show goes on)
    void readDeckTrack (int deck, const juce::File& source, const juce::File& standsFor,
                        std::optional<VideoMedia::Info> video, int token);   // into memory, then onto the deck
    void readDeckVideo (int deck, const juce::File& video, int token);       // a video's sound straight from it (or VLC's)
    void deckTrackRead (int deck, std::shared_ptr<const LiveShow::DeckTrack> track, const juce::String& error,
                        const juce::File& standsFor, std::optional<VideoMedia::Info> video);   // onto the deck (or why not)
    void finishDeckVideo (int deck);         // a video's sound read out by VLC: onto the deck (or why not)
    void updateLiveScreen();                 // the status timer: the player's deck, videos being read, the screen
    LiveScreen::Screen& liveScreenNow();
    void syncScreenDeck (int deck);          // the track on a deck, for the screen
    void handleLiveScreen (const juce::String& action, const juce::var& event);
    juce::var screenToVar();
    static juce::File screenSettingsFile();
    void saveScreenSettings();               // the other screens chosen, the picture in picture's place (and whether the user left it open)
    void setLiveScreenOutput (const juce::String& id, bool on);   // full screen on another screen, or not (kept)
    void showPipOutputs (juce::Rectangle<int> area);             // the picture in picture's HDMI: the screens' menu there
    void showDropZone (const juce::var& event);   // files dragged in from a folder: taken over the playlist
    int liveLinkDeck (int deck, const juce::String& what) const;   // MIDI Link's deck, in the playlist's tabs

    // The Controller (LiveController.h, MainLiveControl.cpp): the stream's cameras and screen, each
    // Main, Custom or Hidden, its presets and its MIDI pads, in a window of its own.
    std::unique_ptr<LiveControl::Controller> liveControl;
    void setupLiveControl();
    void openLiveController (bool open);
    void applyLiveControl();                 // the stream's sources: the Controller's rows
    void applyLivePreviewing();              // the picture is made while the Stream settings panel, the Controller or the Monitor shows
    void controllerChanged() override;
    juce::Image controllerPicture() override;
    juce::Image controllerOutputPicture() override;
    juce::Image controllerTallPicture() override;
    juce::Image controllerTallOutputPicture() override;
    void controllerViewChanged() override;
    juce::Image controllerSourcePicture (int row) override;
    juce::String controllerSourceProblem (int row) override;
    std::vector<LiveStream::Device> controllerCameras (bool lookAgain) override;
    std::vector<LiveStream::Device> controllerScreens() override;
    juce::StringArray controllerMidiPorts() override;
    void controllerMidiPortChosen (const juce::String& port) override;
    bool controllerSwitch (const juce::String& which) override;
    void controllerSetSwitch (const juce::String& which, bool on) override;
    juce::String controllerStatus (bool& live) override;
    void controllerClosed() override;
    void controllerMonitorChanged() override;

    // The stream (LiveStream.h): the picture's look, and its destinations.
    void handleLiveStream (const juce::String& action, const juce::var& event);
    LiveStream::Studio& liveStudio();
    void applyStreamSettings();
    void startLiveStream();
    void stopLiveStream (const juce::String& why);
    bool streamHasVertical() const;          // a destination ticked takes the vertical picture (TikTok)
    juce::var streamToVar();
    juce::var streamStatsToVar();
    void updateLiveStream();                 // the status timer: what's playing, the preview, an ended stream
    static juce::File streamSettingsFile();

    void recordLive (bool on, bool stems);
    void finishLiveRecording (const juce::String& why = {});
    void startPodcast (const juce::File& source, double lufs);
    void finishPodcast();
    void pushLiveValues();                   // the settings alone (MIDI Link moved some)

    // MIDI Link: "live.deck.<n>.<setting or action>", "live.channel.<n>.<setting>",
    // "live.master.<setting>", "live.pad.<n>", "live.onair", "live.mute" (both), "live.mutemics",
    // "live.muteplayback", "live.record".
    void linkMoveLive (const juce::StringArray& parts, float position);
    void linkSetLive (const juce::StringArray& parts, bool on);
    bool linkIsOnLive (const juce::StringArray& parts);

    bool liveShown = false;
    bool liveStale = false;    // tracks changed while the show was hidden: pushed as it's shown
    juce::String liveMode { "live" };                   // "live" (Live & podcasts) or "stream" ("dj", DJ show, isn't offered)
    float liveDjDecks = 0.0f;                           // DJ show's decks (0: two, 1: four), kept while the other tabs play two
    bool liveOnAir = false;                             // Live streaming: Enable streaming (what goes to the stream)
    // Live & podcasts, Live streaming: the mics start muted (an open mic by surprise feeds back);
    // the player doesn't, so pressing Play is heard.
    bool liveMicsMuted = true, livePlaybackMuted = false;
    juce::Array<juce::var> livePlaylist;                // {id, path, title, sync, key, volume, timing}
    int livePlaylistNext = 1;
    bool livePlaylistAuto = true;                       // the playlist plays itself
    std::set<int> livePlayed;                           // the items played (this session)
    std::array<double, LiveShow::maxDecks> deckEndedAt {};       // when its item reached its end (ms)
    std::array<bool, LiveShow::maxDecks> deckHandedOver {};      // its item has led on to the next
    std::array<bool, LiveShow::maxDecks> deckFinished {};        // the playlist's tabs: played to its end, back at its start since
    std::array<bool, LiveShow::maxDecks> deckStarted {};         // its track has played since it came
    std::array<bool, LiveShow::maxDecks> deckByUser {}, deckLoadByUser {};   // put there by the user (not the zipper)
    std::array<bool, LiveShow::maxDecks> deckStale {};  // the user put another in the player: this one is to hold what comes after it
    juce::File lastPlaylistFile;
    std::unique_ptr<juce::FileChooser> playlistChooser, libraryChooser;
    std::array<int, LiveShow::maxDecks> deckItem {};    // the playlist item a deck plays (0: none)
    std::array<int, LiveShow::maxDecks> deckAfter {};   // its item went off the list as it played: the item it came after (0: the top)
    std::array<int, LiveShow::maxDecks> deckLoadItem {};
    std::array<int, LiveShow::maxDecks> deckLoop { -1, -1, -1, -1 };   // the loop it plays (-1: none)
    double liveSongTempo = 0.0;                         // the DAW's tempo, as the synced decks last took it
    juce::String liveListingPath;
    bool liveValuesDue = false;
    bool liveStateDue = false;                          // the whole state, with the next status (a banner to show)
    double liveValuesPushedMs = 0.0;
    bool liveRecordStems = true;
    juce::File lastLiveTake, lastPodcastFile;
    std::unique_ptr<PodcastExport> podcastExport;
    std::unique_ptr<juce::FileChooser> podcastChooser;

    std::unique_ptr<LiveStream::Studio> liveVideo;      // made when the Stream panel first opens
    juce::var streamSettings;                           // as the page has them
    std::vector<LiveStream::Device> streamCameras, streamScreens;
    juce::String streamLogoPath;
    juce::Image streamLogo;
    juce::String streamPlaying;
    LiveStream::Stats streamLast;                       // the stream's last word (kept once it's gone)
    bool streamPanelOpen = false, streamEnding = false;
    bool liveWidthWatched = false;          // the effects window shows the stereo width's goniometer
    double streamPreviewMs = 0.0;
    std::unique_ptr<juce::FileChooser> logoChooser;
    std::array<int, LiveShow::maxDecks> deckLoadToken {};
    std::array<juce::String, LiveShow::maxDecks> deckLoading, deckError;
    std::array<LiveShow::TrackInfo, LiveShow::maxDecks> deckInfo {};
    std::array<bool, LiveShow::maxDecks> deckListening {};
    std::array<std::atomic<int>, LiveShow::maxDecks> deckListenToken {};
    std::array<int, LiveShow::numPads> padLoadToken {};
    std::array<juce::String, LiveShow::numPads> padLoading;
    std::unique_ptr<LiveShow::TrackLibrary> trackLibrary;
    bool liveTracksSaveDue = false;
    std::atomic<int> folderListenToken { 0 };
    std::atomic<bool> liveClosing { false };

    // The playlist's one player and its screen.
    int liveFront = 0;                                   // the deck the player shows (0 or 1)
    int liveCued = -1;                                   // the deck the user put in the player, nothing playing (-1: none)
    juce::uint32 liveStartCounter = 0;
    std::array<juce::uint32, LiveShow::maxDecks> deckStartOrder {};   // when each last started (the newer is in front)
    std::array<bool, LiveShow::maxDecks> deckWasPlaying {};
    struct DeckVideo { juce::File video; VideoMedia::Info info; };   // a video on a deck (its sound on it)
    std::array<DeckVideo, LiveShow::maxDecks> deckVideo {};
    std::array<std::unique_ptr<VideoMedia::Preparation>, LiveShow::maxDecks> deckPreparing;   // a video VLC is reading out for it
    std::array<double, LiveShow::maxDecks> deckProgressShown {};
    bool videoStarted = false;                           // VLC started (on a thread of its own) for the playlist's tabs
    std::atomic<bool> videoStarting { false };           // and still starting (it can't be stopped)
    std::unique_ptr<LiveScreen::Screen> liveScreen;      // made once the playlist's tabs are used
    bool livePipOpen = false;                            // the picture in picture is open (it shows while the show is on)
    bool livePipKept = false;                            // open as the user left it (an opening for a video isn't kept)
    juce::File livePipVideo;                             // the last video that opened it, coming into the player
    bool liveScreenArmed = false;                        // the show was looked at: the screens chosen may open
    juce::StringArray liveScreenOutputs;                 // the other screens chosen (kept next to Sonify's settings)
    std::unique_ptr<juce::Component> liveDropZone;       // over the playlist while files are dragged in

    juce::AudioFormatManager deckFormats;   // the loader's thread alone uses it
    juce::ThreadPool deckLoader { juce::ThreadPoolOptions{}.withThreadName ("Sonify deck loader").withNumberOfThreads (1) };
    juce::ThreadPool liveListener { juce::ThreadPoolOptions{}.withThreadName ("Sonify Live Show listening").withNumberOfThreads (2) };

    //==============================================================================
    // The Sampling Studio (MainSampling.cpp, SamplingStudio.h): Sampler Arena's Sampling Module.
    // VSTi Sampling and Chord Sampling (an instrument track, a synth outside the computer or the
    // user playing, sampled into WAVs, with an SFZ), the Sample Builder (an instrument from audio
    // files and clips put on keys), presets (.smppreset), its folders, and the Loop Editor (an
    // SFZ's loops found, set by hand or picked by Fanan AI); and the Sonify Sampler's window and
    // folders (SonifySampler.h), where what it makes plays.
    void setupSampling();
    juce::WebBrowserComponent::Options withSamplingEvents (juce::WebBrowserComponent::Options options);
    void handleSampling (const juce::var& event);
    void startSampling (const juce::var& event);
    void startSampleBuild (const juce::var& event);
    void auditionSampling (const juce::var& event);
    void updateSampling();                   // its timer: the session's word, the Builder's
    void pushSampling (bool progressOnly = false);
    void emitSampling (const juce::var& payload);
    void samplingProblem (const juce::String& text);
    void trySampledInstrument (const juce::File& sfz, const juce::String& name);
    void showSamplingDropZone (const juce::var& event);   // files dragged in from a folder: taken over a Sample Slot
    void keepSampling();                                  // the choices and folders, for next time
    juce::File samplingFolder() const;                    // where samples go: the Ad Hoc folder, else the module's
    juce::File samplingModuleFolderOrDefault() const;
    static juce::File samplingSettingsFile();

    std::unique_ptr<Sampling::Session> sampling;         // the session (the destructor ends it before the engine goes)
    std::unique_ptr<Sampling::Build> sampleBuild;
    juce::String sampleBuildName;
    bool sampleBuildPushed = false;
    juce::File sampledInstrument;                        // the instrument made last (a session's or the Builder's), for Try It
    juce::File sampledFolder;                            // where its samples went
    juce::String sampledName;
    bool samplingShown = false;
    int samplingStatePushed = -1;                        // the session's state the page last had whole
    juce::var samplingSettings;                          // the page's choices (Sampling::Settings::toVar), kept
    juce::File samplingModuleFolder;                     // Set Folder (kept); none: the Samples folder
    juce::File samplingAdHoc;                            // the Ad Hoc folder (this run only); none: the module's
    std::unique_ptr<juce::MidiOutput> auditionOutput;    // a synth's port, while keys are auditioned
    juce::String auditionOutputId;
    int samplingDropNote = -1, samplingDropRow = -1;     // the Sample Slot files are dragged over
    std::unique_ptr<juce::Component> samplingDropZone;
    std::unique_ptr<juce::FileChooser> samplingChooser;
    juce::TimedCallback samplingTimer { [this] { updateSampling(); } };

    // The Sampling Studio's nodes (SamplingRack.h): made when the studio first shows (the plugins
    // kept from last time loaded then), played by the engine while the studio shows or samples.
    void makeSamplingRack();
    void handleSamplingRack (const juce::var& event);
    void pushSamplingRack();
    void updateSamplingRackAwake();
    void browseSamplingPlugins (const juce::String& format, const juce::String& folder);
    void loadSamplingPlugin (const juce::String& format, const juce::File& file, const juce::String& pluginId, float x, float y);
    juce::File samplingPluginFolderFor (const juce::String& format) const;   // where its browse window opens
    void keepSamplingPluginFolders();
    static juce::File samplingPluginFoldersFile();

    // SFZ to Blob (MainBlobs.cpp, BlobMaker.h): SFZs and their samples packed into blobs, the
    // files a sampler made in the Plugins Creator Studio plays from the disk.
    void handleBlobMaker (const juce::var& event);
    void pushBlobMaker();
    std::unique_ptr<BlobMaker> blobMaker;                // made when its tab first asks
    std::unique_ptr<juce::FileChooser> blobChooser;      // SFZ to Blob's dialogs

    // MIDI In (AudioEngine::setMidiFocus): the studio in front takes the live MIDI; the page says
    // which (studioShown {id}). The Sampling Studio's MIDI monitor, while it's in front.
    void studioShown (const juce::String& id);
    void pushMidiMonitor();
    juce::TimedCallback midiMonitorTimer { [this] { pushMidiMonitor(); } };
    int midiMonitorCount = 0;                            // the messages numbered so far

    std::unique_ptr<SamplingRack> samplingRack;
    juce::var samplingRackKept;                          // its nodes as kept, until it's made
    std::map<juce::String, juce::File> samplingPluginFolders;   // each format's last folder, kept
    juce::TimedCallback samplingRackTimer { [this] { if (samplingRack != nullptr && samplingRack->update()) pushSamplingRack(); } };

    // The Loop Editor (SfzLoops.h): an SFZ, its samples' loops.
    struct LoopEdit
    {
        SfzLoops::Patch patch;
        int selected = -1;
        juce::AudioBuffer<float> audio;                  // the selected sample
        double rate = 44100.0;
        LoopFinder::Result found;                        // its analysis (sustain, candidates)
        std::vector<LoopFinder::Candidate> candidates;   // shown: the finder's, or the loop it has
        int candidate = -1;
        juce::String score { "Score: --" };
        juce::String status;
        int crossfadeMs = 100;
        bool crossfadeOn = true;
        bool playing = false;
        bool busy = false;                               // Batch Find All, or Fanan AI, under way
        int generation = 0;                              // a new patch: work for the old one is dropped
        std::map<int, juce::String> why;                 // Fanan AI's reasons, by region
        juce::File folder;                               // where Load SFZ Patch starts (kept)
    };

    LoopEdit loopEdit;
    void handleLoopEditor (const juce::String& action, const juce::var& event);
    bool openLoopPatch (const juce::File& sfz);
    void selectLoopSample (int index);
    void selectLoopCandidate (int index);
    void applyLoopToRegion (SfzLoops::Region& region, juce::int64 loopStart, juce::int64 loopEnd, int crossfade, float score);
    void saveLoopPatch (const juce::File& target);
    void batchFindLoops();
    void askLoopAi (bool all);
    void sendLoopAi (int generation, std::shared_ptr<std::vector<SfzLoops::AiSample>> samples, size_t from, int picked, int unlooped);
    void pushLoopEditor (bool withPeaks);
    int loopCrossfadeSamples() const;
    juce::ThreadPool loopWorker { juce::ThreadPoolOptions{}.withThreadName ("Sonify loop editor").withNumberOfThreads (1) };
    juce::TimedCallback loopTimer { [this] { pushLoopPosition(); } };
    void pushLoopPosition();

    //==============================================================================
    // The Device Studio (MainDevices.cpp): Sonify devices made by Sonify AI on its own
    // conversation, played in the MIDI FX; what they move goes through MIDI Link's host.
    void setupDevices();
    void handleDeviceStudio (const juce::var& event);
    void handleDeviceMap (const juce::var& event);
    void handleDeviceRegister (const juce::var& event);   // a protected MIDI FX's Register: its Registration window
    void updateDevices();               // 60 times a second: what devices moved, the Studio's view
    void pushDeviceStudio();
    void pushDeviceAi();
    juce::String placeDeviceBuild (const std::shared_ptr<Devices::Build>& build, bool addIfNowhere);
    bool addDeviceToTrack (int trackId, const std::shared_ptr<Devices::Build>& build, bool open);
    juce::var deviceSettingsFor (const juce::var& event, juce::String& error);

    std::unique_ptr<DeviceStudio> deviceStudio;
    std::unique_ptr<AiAgent> deviceAgent;

    //==============================================================================
    // AI Mix and AI Master (MainAiMix.cpp, AiMix.h): Fanan AI mixing the song on the Analog Board
    // and mastering it in the Mastering section, for a genre, a reference track or neither. Sonify
    // measures the song (rendered: MixAnalysis), Fanan AI sets the knobs and measures again; then
    // Sonify measures the result, and the user compares it with what they had (A/B, the original
    // at the suggestion's loudness) and applies it or discards it. Its window: "aiMix" events.
    enum class AiMixPhase { idle, measuring, working, checking, suggestion };

    struct AiMixRender
    {
        bool running = false, analysing = false;
        juce::File folder;
        std::vector<MixAnalysis::Input> inputs;
        std::function<void (const juce::var&)> done;
    };

    void setupAiMix();
    void handleAiMix (const juce::var& event);
    void pushAiMix();
    void startAiMix (const juce::var& event);
    void aiMixMeasure (std::function<void (const juce::var&)> done);
    void updateAiMixRender();                          // from updateMixdown, while its render runs
    void aiMixRequestEnded (bool changed);
    void aiMixListen (bool suggestion);
    void aiMixEnd (bool apply);                        // Apply, or Discard
    void aiMixStop();
    void chooseAiMixReference();
    juce::var boardSnapshot();
    void restoreBoardSnapshot (const juce::var& snapshot);

    std::unique_ptr<AiMix> aiMixTools;
    std::unique_ptr<AiAgent> aiMixAgent;
    AiMixPhase aiMixPhase = AiMixPhase::idle;
    bool aiMixMasterOnly = false;
    bool aiMixShown = false;
    bool aiMixListeningBefore = false;
    int aiMixGeneration = 0;                           // the song it works on
    juce::String aiMixProblem, aiMixLabel;
    juce::File aiMixReference;
    juce::var aiMixReferenceReport;                    // the reference's numbers, measured once it's chosen
    bool aiMixReferenceBusy = false;
    juce::var aiMixBefore, aiMixAfter;                 // the board's and the Mastering section's settings
    juce::var aiMixBeforeReport, aiMixAfterReport;
    std::atomic<double> aiMixProgress { 0.0 };
    double aiMixPushedMs = 0.0;
    AiMixRender aiMixRender;
    std::shared_ptr<std::atomic<bool>> aiMixCancelled = std::make_shared<std::atomic<bool>> (false);
    std::unique_ptr<juce::FileChooser> aiMixChooser;
    juce::ThreadPool aiMixWorker { juce::ThreadPoolOptions{}.withThreadName ("Sonify AI Mix listening").withNumberOfThreads (1) };

    //==============================================================================
    // The Plugins Creator Studio (MainCreator.cpp): Stella AI Studio inside Sonify, in a window
    // of its own over the page's studio area while it's in front. It makes MIDI FX with the
    // Device Studio above. (The studio itself is declared after the web view: it goes first.)
    void setupCreator();
    juce::WebBrowserComponent::Options withCreatorEvents (juce::WebBrowserComponent::Options options);
    void placeCreator();
    void hideCreator();
    void emitCreatorState();
    void creatorPluginAdded (const juce::File& folder, const juce::String& name, bool replaced);
    juce::Rectangle<int> creatorArea;   // the page's studio area (its pixels) while the studio shows
    bool creatorShown = false;
    juce::TimedCallback deviceTimer { [this] { updateDevices(); } };
    std::map<std::pair<juce::int64, juce::String>, float> deviceMoveValues;   // (device, control) -> its last move
    int deviceTryTrack = 0;             // where a device that plays nowhere yet goes
    int deviceTicks = 0;
    bool deviceStudioShown = false, deviceStudioDue = false;
    juce::String deviceLivePushed;

    //==============================================================================
    // Voice control: the musician talks to Sonify from the recording room ("Sonify ... over")
    // and hears beeps back. Always off at launch; the mic's input is remembered. (The
    // VoiceControl itself is declared after the engine it listens to.)
    bool voiceRequest = false;          // Sonify AI is on a spoken request: a beep says how it ended
    juce::var voiceInputChoice;         // the mic's input as last chosen: { name, left, right }
    juce::String voiceInputsShown;      // the song's inputs as the page last heard of them
    double againFrom = -1.0;            // where playing or recording last began ("again" goes back there)
    bool againRecords = false;
    bool startedByVoice = false;        // that start was a voice command's, which knows it exactly
    bool wasPlaying = false, wasRecording = false;
    double stoppedAt = 0.0;

    void pushVoice();
    void loadVoiceInput();
    void chooseVoiceInput (int index);
    void followVoiceInput();
    void followTransport();
    void forgetWhereItBegan();          // a new song: "again" has nowhere to go back to
    void runVoiceCommand (const juce::String& command);
    void voiceAgain (VoiceControl::Quick quick, int attempt);
    void voiceRecord (int attempt);
    void voiceProblem (const juce::String& message);

    //==============================================================================
    // Sonify Remote: Sonify AI from a phone (the page /sonify on Fanan's site, signed in to the
    // same account). Always off at launch.
    std::unique_ptr<PhoneRemote> phoneRemote;
    void pushRemote();
    void askFromPhone (const juce::String& text, bool spoken);
    juce::StringArray pageCommands;   // the page's commands, for Fanan AI (aiCommandList)

    //==============================================================================
    // File > Settings (MainSettings.cpp): how Sonify looks and which shortcuts it uses (the page
    // applies both), the audio setup, the MIDI setup (MIDI inputs, external MIDI devices with the
    // clock and timecode they're sent, control surfaces), network discovery, the user data folder,
    // autosave, and letting the audio device go while Sonify is in the background.
    Preferences prefs;
    std::unique_ptr<juce::NetworkServiceDiscovery::Advertiser> advertiser;
    double lastAutosave = 0.0;          // when the song was last autosaved, or its changes began
    double lastForegroundCheck = 0.0;
    bool lastHistoryWasUndo = false;    // for FL Studio's Ctrl+Z, which redoes right after an undo
    juce::uint64 historySteps = 0;      // steps begun, undone or redone: a plugin loading later joins its step only while it's unchanged

    // The audio device watched: one that stops by itself (a driver's reset that never comes back, a
    // dropout, the computer waking) is opened again, a few times, further apart each time.
    double deviceWatchMs = 0.0, deviceStalledMs = 0.0;
    juce::uint32 lastDeviceCallbacks = 0;
    juce::String deviceWatched;         // the device and its setup (another chosen: watched afresh)
    bool deviceHeard = false;           // it ran since it was chosen (one that never started isn't pushed)
    bool deviceSettingUp = false;       // Settings change it, or its driver's window shows: not watched
    bool deviceReopening = false;       // the watch is starting it again (a driver's message box may show)
    juce::AudioDeviceManager::AudioDeviceSetup deviceRunningSetup;   // the setup it last ran with
    int deviceReopens = 0;

    // MIDI inputs: every port is listened to but the ones turned off by hand, so a keyboard
    // plugged in or a loopMIDI port made is in the tracks' MIDI lists at once.
    juce::String midiPortNames;         // the MIDI ports there were last time (to see them change)
    juce::String midiPortsTold;         // the ports as the page last heard of them
    juce::StringArray midiPortsBusy;    // inputs that couldn't be opened (another app holds them)

    // Remote control apps on the network hear Sonify announce itself on this port (JUCE's
    // service discovery), with the port a remote control link will take (nothing listens yet).
    static constexpr int discoveryPort = 50841;
    static constexpr int remotePort = 50842;

    juce::WebBrowserComponent::Options withSettingsEvents (juce::WebBrowserComponent::Options options);
    void loadPreferences();
    void savePreferences();
    void pushSettings();
    void setPreference (const juce::var& event);
    void applyMidiDevices();
    void applyDiscovery();
    void chooseUserDataFolder();
    void setUserDataFolder (const juce::String& folder);
    void pushAudioSetup (const juce::String& error = {});
    void changeAudioSetup (const juce::var& event);
    void followMidiPorts();
    void setMidiPortEnabled (const juce::String& identifier, bool enabled);
    void applySongDefaults();
    void setSongDefaults (const juce::var& io);
    juce::var midiInputsToVar();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;   // the audio device manager
    void saveMidiDevice (const juce::var& event);
    void removeMidiDevice (const juce::String& id);
    void replaceLibraryFolder (const juce::String& folder);
    void followForeground();
    void watchAudioDevice();            // the status timer: a device that stopped by itself, opened again
    void autosaveIfDue();
    void undoToggle();

    //==============================================================================
    // Control surfaces (Settings > MIDI Setup): Mackie Control, its Extender and HUI play Sonify.
    // What they change the page hears through the status (faders, pan) or a tracks update a few
    // times a second at most (the rest). A send's knob turned is one undo step, as a send's
    // fader dragged on the page: from where it started, once it rests. MainSettings.cpp.
    bool surfaceTracksDue = false;
    double surfaceTracksPushedMs = 0.0;
    bool surfaceSendEditing = false;
    int surfaceSendTrack = 0;
    std::vector<Track::Send> surfaceSendsBefore;
    double surfaceSendChangedMs = 0.0;
    juce::StringArray surfaceProblems() const;
    void surfaceSendEdit (int trackId);
    void commitSurfaceSendEdit();

    std::vector<ControlSurfaces::Channel> getSurfaceChannels (bool withSends) override;
    float getMainVolume() override;
    Transport::State getSurfaceTransport() override;
    const TempoMap& getSurfaceTempoMap() override;
    double getSurfaceSongEnd() override;
    double getSurfaceFrameRate() override;
    void setChannelVolume (int channelId, float db) override;
    void setChannelPan (int channelId, float pan) override;
    void setChannelMute (int channelId, bool mute) override;
    void setChannelSolo (int channelId, bool solo) override;
    void setChannelRecordArm (int channelId, bool arm) override;
    void selectChannel (int channelId) override;
    void setChannelSendLevel (int channelId, int sendId, float db) override;
    void setChannelSendOn (int channelId, int sendId, bool on) override;
    void setChannelAutomationRead (int channelId, bool read) override;
    void surfaceTransport (const juce::String& action) override;
    void surfaceLocate (double ppq) override;
    void surfaceLoop (bool on) override;
    void surfaceMetronome (bool on) override;
    void surfaceUndo (bool undo) override;
    void surfaceSave() override;
    void surfaceCommand (const juce::String& pageCommand) override;

    bool isInternetBlocked() const noexcept    { return internetBlocked; }
    void setInternetBlocked (bool blocked);
    void loadInternetBlocked();
    void pushInternetBlocked();
    void updateRecordingFolder();
    void listFolder (const juce::String& path);

    /** The Files tab's Open: a folder chosen becomes the song's files folder (kept with it) and the
        one new songs show. showSongFilesFolder: the song's own, else the one for new songs. */
    void chooseFilesFolder();
    void showSongFilesFolder();
    juce::var withFilesFolder (juce::var song) const;

    /** A folder's subfolders and audio (and video, unless audioOnly) files, and the places to go:
        the Browser's Files tab and the Live Show's library. */
    juce::var folderListing (const juce::String& path, bool audioOnly);
    void sendAudioFileInfo (const juce::String& path);
    void pushFinePeaks (const juce::String& path);
    void importAudioFile (const juce::File& file, int trackId, double start, bool joinPreviousStep);
    bool importMidiFile (const juce::File& file, int trackId, double start, bool joinPreviousStep);   // a MIDI file's notes as a clip
    void chooseAudioToImport();

    /** Media import (MainImport.cpp): audio files dragged in from a folder on the computer and
        dropped in the arrangement, Load Media..., File > Import Media Files.... Kinds Sonify
        doesn't play as they are (M4A, AAC, Opus...) are read into WAVs first by FFmpeg. */
    struct MediaPlace
    {
        int trackId = 0;            // the first file onto this audio track (none: a new track)
        int after = 0;              // new tracks under this one (0: at the end)
        double start = 0.0;         // quarter notes
        bool oneTrack = false;      // all on one track, one after another
    };

    void handleImport (const juce::var& event);
    void showArrangeDropZone (const juce::var& event);
    bool isImportable (const juce::File& file);
    juce::Array<juce::File> mediaIn (const juce::StringArray& paths);    // folders opened, what can't come in left out
    void chooseMedia (MediaPlace place);
    void importMedia (const juce::Array<juce::File>& files, MediaPlace place);
    void placeMedia (const juce::Array<juce::File>& files, MediaPlace place, juce::StringArray problems);
    int addAudioTrackAfter (int afterId, const juce::String& name, const juce::String& undoName, bool joinPreviousStep);

    std::unique_ptr<juce::Component> arrangeDropZone;    // over the arrangement while files are dragged in
    std::unique_ptr<juce::FileChooser> mediaChooser;
    juce::File lastMediaFolder;
    juce::StringArray arrangeDragPaths;                  // what's being dragged over the arrangement
    int arrangeDragCount = 0;
    juce::String arrangeDragName;
    juce::Point<int> arrangeDragAt { -1, -1 };
    bool arrangeDragShift = false;
    double arrangeDragSent = 0.0;

    /** Files being read into WAVs stop (their thread looks at it) once Sonify closes. */
    struct ImportAlive
    {
        std::shared_ptr<std::atomic<bool>> flag = std::make_shared<std::atomic<bool>> (true);
        ImportAlive() = default;
        ~ImportAlive()   { flag->store (false); }
        JUCE_DECLARE_NON_COPYABLE (ImportAlive)
    };

    ImportAlive importAlive;
    void pushAudioPeaks (const juce::String& path);

    juce::File songFile;               // empty until the song is first saved
    bool songChanged = false;
    bool askingToSave = false;
    juce::var songView;                // the page's view settings, kept with the song
    juce::String songFilesFolder;      // the Files tab's folder for this song (empty: the one for new songs)
    std::unique_ptr<juce::FileChooser> filesFolderChooser;
    juce::StringArray recentSongs;     // newest first
    std::unique_ptr<juce::FileChooser> songChooser;
    std::unique_ptr<juce::FileChooser> dawProjectChooser;
    std::unique_ptr<DawProject::Writer> dawProjectWriter;   // an export being written
    juce::String importTitle;          // a song just imported: what its message is called, and what it says first
    juce::StringArray importNotes;

    void pushStatus();
    void pushTracks (const juce::String& notice = {});

    /** The song channel (tempo map and Main Volume lane), when it changed since the page had it. */
    void pushSongChannel (bool force = false);
    void setSongChannel (const juce::var& event);
    void setMainAutomationFrom (const juce::var& event);
    juce::uint32 pushedMapGeneration = 0;
    bool songChannelPushed = false;
    std::vector<AutomationPoint> pushedMainAutomation;
    void pushLibrary (const juce::String& notice = {});

    // The Browser's pictures of made plugins' panels (MadePlugins::makeSnapshot): those missing or
    // out of date are drawn one per timer tick, and the Browser hears once they're all done.
    juce::StringArray snapshotQueue;
    double snapshotDrawnMs = 0.0;                 // the last one drawn (they're spaced out)
    std::set<juce::String> snapshotTried;     // folder + when it last changed: one that failed isn't tried again until it changes
    void updateSnapshots();

    void handleTransport (const juce::var& event);
    bool handleTrackSettings (const juce::var& event, bool push = true);   // true: something the track list shows changed

    //==============================================================================
    // The track list's Options menu (Preferences::getTrackOptions; MainTrackOptions.cpp): what the
    // engine does for it. The page does the rest (hub-trackoptions.js).
    struct CarriedClip
    {
        int fromTrack = 0, toTrack = 0;
        double start = 0.0, end = 0.0;    // where it was, song positions
        double shift = 0.0;               // how far it went
        bool copy = false;                // a copy: the envelopes stay where they were too
    };

    void armTrack (int trackId, bool arm);              // Record arm (Monitoring follows record)
    bool inputFollowsSelection (int trackId);           // true: a track's monitoring changed
    void clearOverlaps (const std::vector<std::pair<int, int>>& placed);   // No overlap
    void carryAutomation (const std::vector<CarriedClip>& carried);         // Automation follows events

    /** Several tracks and channels at once (the page's selection): settings (mixing, no undo
        step), remove, duplicate, move and rename (each one undo step). */
    void handleTrackGroup (const juce::String& what, const juce::var& event);
    juce::Array<juce::var> trackClipboard;   // tracks the track menu's Cut or Copy kept (Song::copyTrack), for Paste

    /** The track list's right-click menu (MainTracks.cpp): Add Tracks, Add Audio Track (mono /
        stereo), Add Tracks for all Inputs, Store and Load Track Preset. */
    void handleTrackMenu (const juce::var& event);
    void selectNewTracks (const std::vector<int>& ids);   // they become the page's selection
    void chooseTrackPresetFile (bool storing, const std::vector<int>& ids);
    std::unique_ptr<juce::FileChooser> trackPresetChooser;

    /** The Score's exports (MainScore.cpp): PDF and MusicXML, made by the page, saved here. */
    void handleScore (const juce::var& event);
    std::unique_ptr<juce::FileChooser> scoreChooser;
    void loadPluginInto (int channel, const juce::File& file, const juce::String& pluginId,
                         int position, int replaceInsertId, bool joinUndoStep = false);
    void recordPluginLoad (int channel, int insertId, AudioEngine::Displaced& displaced,
                           const Edits::TrackName& nameBefore, bool joinUndoStep);

    //==============================================================================
    // Undo and redo (Edit > Undo, Ctrl+Z; Edit > Redo, Ctrl+Y or Ctrl+Shift+Z).
    void addToHistory (const juce::String& name, juce::UndoableAction* action, bool joinPreviousStep = false);
    void undoOrRedo (bool undo);
    void setPluginParameter (const juce::var& event);
    void chooseLibraryFiles();

    static juce::var pluginToVar (Plugin& plugin);
    static juce::var insertsToVar (const EffectChain& chain);
    juce::var clipsToVar (const Track& track) const;
    static std::vector<MidiNote> notesFromVar (const juce::var& list);
    static std::vector<MidiControlPoint> controlsFromVar (const juce::var& list, int minimum, int maximum);
    static juce::var controlsToVar (const std::vector<MidiControlPoint>& points);
    void applyClipEdits (const juce::var& edits, const juce::String& label = {});

    // Declaration order is destruction order in reverse: the web view is destroyed before the
    // engine it talks to.
    AudioEngine engine;

    // Declared after the engine so it's destroyed before it: plugins kept for undo must go
    // while the WebAssembly engine still runs (the destructor clears it first as well).
    juce::UndoManager undoManager { 5000, 100 };

    // The control surfaces read the engine and take what its MIDI thread kept for them, so they
    // go before it (the destructor ends them first, and they go dark).
    std::unique_ptr<ControlSurfaces> surfaces;

    // Pad grids (Launchpad, APC mini) playing Song Blocks' Live (MainBlocks.cpp). Their ports are
    // surfaces' too: what comes from them is sorted out of the surfaces' queue here.
    std::unique_ptr<PadGrids> padGrids;
    std::vector<ControlSurfaces::Message> surfacePending;   // taken from the queue for the control surfaces
    void drainSurfaceMessages();
    void updatePadGrids();
    PadGrids::View padView();

    // MIDI Link (the toolbar's strip) takes what the engine's MIDI thread kept for it: declared
    // after the engine, so it goes first. MainLink.cpp.
    std::unique_ptr<MidiLink> midiLink;
    std::map<int, double> linkMovedMs;              // channel (mainChannel: Main) -> when a link last moved its fader or pan
    std::vector<MidiLink::Link> linksBeforeEdit;    // as they were before the last drop (Cancel)
    juce::var linkTouched;                          // Last Touched: the plugin parameter last taken hold of
    juce::String linkMessage;
    juce::String linkLearnedKey;            // MIDI Learn linked this source: the page opens its link's window
    std::unique_ptr<juce::FileChooser> linkChooser;
    std::vector<AudioEngine::LinkSource> midiLinkSources;   // MIDI Link's links' sources (the Controller's pads join them)
    void applyLinkedSources();                      // the engine keeps them all from the tracks
    void setupMidiLink();
    void applyMidiLinkDevices();
    void updateMidiLink();
    void pushMidiLink();
    void handleMidiLink (const juce::String& what, const juce::var& event);
    void chooseMidiLinkFile (bool saving);
    bool linkMovedLately (int channelId) const;
    int linkChannelId (const juce::StringArray& keyParts) const;
    int linkSendId (int channelId, int position);
    Plugin* findLinkedPlugin (const juce::String& pluginId, int& channel, int& insertId);
    void linkMove (const MidiLink::Target& target, float position) override;
    void linkSet (const MidiLink::Target& target, bool on) override;
    bool linkIsOn (const MidiLink::Target& target) override;
    // Containers (MainContainers.cpp): what a container (Container.h) asks of Sonify.
    std::unique_ptr<Plugin> loadPlugin (const juce::File& file, const juce::String& pluginId, juce::String& error) override;
    std::vector<Containers::Choice> listPlugins() override;
    void openScreen (Plugin& plugin) override;
    int allocateSlotId() override;
    void containerChanged (ContainerPlugin& container) override;
    juce::File containersFolder() override;
    double sampleRate() const override;
    juce::File pluginBrowseFolder() override;
    void setPluginBrowseFolder (const juce::File& folder) override;
    void* mainWindowHandle() override;
    bool containersChanged = false;    // the page hears of it at the next timer tick
    void setupContainers();
    static void addContainerInfo (Plugin& plugin, juce::DynamicObject& state);

    // Song Blocks (MainBlocks.cpp): made, edited, put in order and built, as the page asks.
    void handleSongBlocks (const juce::var& event);
    void buildSongFromBlocks (double at);
    void pushSongBlocks (bool force = false);
    int pushedBlocksGeneration = -1;

    // Song Blocks' Live (MainBlocks.cpp): the blocks launched as on a clip launcher. Launching a
    // block on a track lays its clips there from the next bar (or beat, or two bars), copy after
    // copy while the song plays, until another is launched there or the track stops: the jam is
    // written on the timeline as it goes (after the end of the song), one Undo step for it all.
    struct LaunchPlaced
    {
        int clipId = 0;
        double start = 0.0, end = 0.0;
    };

    struct LaunchTrack
    {
        int blockId = 0;                   // what plays (or will, from startsAt); 0: nothing
        double startsAt = 0.0;
        int beforeBlockId = 0;             // what plays until startsAt
        double nextCopy = 0.0;             // where its next copy goes
        std::vector<LaunchPlaced> placed;    // the clips it laid that may still be cut
    };

    bool launcherOn = false;
    int launchQuantize = 1;                  // 0 a beat, 1 a bar, 2 two bars
    std::map<int, LaunchTrack> launchTracks;
    juce::uint64 launchHistoryAt = 0;        // historySteps after the jam's last step: the next joins it
    bool launchHistoryOpen = false;
    bool launchWasPlaying = false;

    void handleLauncher (const juce::String& action, const juce::var& event);
    double launchPoint (bool& startPlaying) const;
    void launchOnTracks (const std::vector<std::pair<int, int>>& launches);   // (track, block; 0: stop)
    void launchPlace (int trackId, LaunchTrack& live, const SongBlock& block, double at);
    void launchCut (int trackId, LaunchTrack& live, double at);
    void launchEdit (const std::vector<int>& trackIds, const std::function<void()>& change);
    void updateLauncher();
    void pushLauncher();
    void linkSetLauncher (const juce::StringArray& parts, bool on);   // MIDI Link's launch.* (MainBlocks.cpp)

    // Focus (MIDI Link's focus.*): the selected track's controls (MainLink.cpp).
    juce::String focusKey (const juce::String& key) const;            // "focus.pan" -> "track.3.pan" (empty: none)
    Plugin* focusKnob (int k, int& channel, int& insertId, int& index);   // its k-th knob (1 .. 8), or null

    // The song's video (MainVideo.cpp): imported (its sound onto an audio track of its own),
    // moved, taken away, shown in the Video window where the song is, exported with the mix.
    void handleSongVideo (const juce::var& event);
    void importVideo (const juce::File& file, double at);
    void chooseVideo (double at);
    void finishVideoImport();
    void updateVideo();                 // the timer's: preparations, the export
    void pushSongVideo (bool force = false);
    void syncVideoPicture();
    void showVideoWindow (bool show);
    VideoScreen::Where videoWhere();
    double videoEnd() const;            // where the picture ends (a song position); its start when unknown
    std::optional<juce::WebBrowserComponent::Resource> getVideoResource();
    void chooseVideoExport();
    void startVideoExport (const juce::File& target);
    void updateVideoExport();
    void emitVideoExport (const juce::String& state, double progress, const juce::String& error = {});

    std::unique_ptr<VideoMedia::Preparation> videoImport;       // a video being imported
    double videoImportAt = 0.0;
    int videoImportGeneration = 0;                               // the song it's for
    juce::String videoPrepareError;                              // why the song's video couldn't be read
    std::unique_ptr<VideoMedia::Preparation> videoPreparing;    // the song's video's pictures, made again
    std::optional<VideoMedia::Prepared> videoPrepared;          // what the lane and the Video window show
    juce::File videoPreparedFor;
    std::unique_ptr<VideoScreen::Picture> videoPicture;
    std::unique_ptr<VideoScreen::Window> videoWindow;
    int pushedVideoGeneration = -1, videoThumbnailsVersion = 0;
    double videoProgressPushed = -1.0;
    juce::uint32 videoLookedFor = 0;                             // when a song's video that wasn't there was last looked for

    enum class VideoExportStage { idle, mixing, writing };
    VideoExportStage videoExportStage = VideoExportStage::idle;
    juce::File videoExportTarget, videoExportMix, videoExportSource;
    std::unique_ptr<Mp4::Export> videoExport;

    Edits::Clips takeBefore;           // the armed tracks' clips as a recording take began
    bool takeRunning = false;

    // While a take records, the clips growing on its tracks go to the page a few times a second
    // (takeClips) instead of the whole song; the whole song goes once as it ends.
    bool takeClipsDue = false, takeEnded = false;
    double takeClipsPushedMs = 0.0;
    void pushTakeClips();

    // An audio take's waveform as it's written ("takeWave"), the new peaks ten times a second:
    // track id -> (the bucket its pass shows from, the buckets sent so far).
    std::map<int, std::pair<juce::int64, juce::int64>> takeWaveSent;
    double takeWavesPushedMs = 0.0;
    void pushTakeWaves (double nowMs);

    // Its listener reads the engine: declared after it, so it goes first (the destructor also
    // ends it before anything else).
    std::unique_ptr<VoiceControl> voice;
    Library library;
    std::unique_ptr<juce::WebBrowserComponent> webView;
    std::set<juce::String> screenOrigins;   // web plugin screens with their own address (their frames may load it)

    // The Plugins Creator Studio plays in the engine and talks to the server: declared after
    // them (and the web view), so it goes first; its window goes after it.
    std::unique_ptr<juce::Component> creatorLayer;
    std::unique_ptr<CreatorStudio> creator;

    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File lastPluginFolder;

    // Library scans: one plugin file per timer tick.
    void startLibraryScan (bool full, bool atStartup = false);
    void updateScan();
    void emitScan (const juce::String& state);
    void chooseLibraryFolder();
    // Plugins are read by copies of the Hub (PluginScanner), never by the Hub itself, and always
    // with the scanning page showing: at startup before the welcome page, and on every rescan.
    std::unique_ptr<PluginScanner> scanner;
    bool scanAtStartup = false;
    int scanFound = 0;
    juce::Array<juce::var> scanProblems;        // { name, error } for each file that couldn't be read
    bool choosingFile = false;
    std::map<int, juce::String> loadingFiles;      // channel -> file being loaded into it (0: Main)
    std::map<int, juce::String> trackMessages;     // channel -> why the last load failed
    juce::String webViewError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
