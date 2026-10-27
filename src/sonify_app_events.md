<!-- D:\workspace\Fanan_web_hub\src\sonify_app_events.md -->
# Sonify's own events (for Fanan AI's send_app_event)

Every action Sonify's screen does reaches Sonify as one of these events, with the fields in braces (? means optional). send_app_event sends one exactly as the screen would: the same result, the same undo step. Ids are those get_song shows (a track's id, an insert's id, a clip's id); positions in quarter notes from the song's start unless an event says otherwise.

Keep it to what the user asked. Events that only report or ask how something stands (uiReady, ...State, ...Watch) change nothing: don't send them.

```
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
```
