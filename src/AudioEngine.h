// D:\workspace\Fanan_web_hub\src\AudioEngine.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include "MidiSync.h"
#include "SongBlocks.h"
#include "Track.h"
#include "Transport.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

//==============================================================================
/**
    Native audio side of the Hub.

    Owns the audio device (ASIO preferred on Windows), listens to hardware MIDI
    inputs, runs the transport and metronome, and plays the tracks. Notes from the
    web UI and from hardware MIDI meet in one MidiMessageCollector and go to every
    track that is monitoring; selecting a track makes it the one that monitors. Live
    MIDI goes to the studio in front only (setMidiFocus): the song's, the Plugins
    Creator Studio's MIDI In, or the Sampling Studio rack's MIDI In node.

    Signal flow: each track (its clips and live notes, its source, its inserts, its
    fader) mixes into the Main channel, which runs its own inserts and fader before
    the device, or into a Sub output (Song Setup's Audio I/O), which has a fader of its
    own and plays on device channels of its own. Every output channel of the device is
    open; the song's outputs say which channels Main and each Sub play on. The metronome
    and the Browser's preview play on the Audition output, after all of that.

    Or the song is mixed on the Analog Board (AnalogBoard.h): each instrument and audio
    track's dry sound (before its inserts) goes through its strip on the board, and the
    board's master section plays on Main, in place of the Console's inserts, sends, FX
    channels, buses and Main.

    Or the Live Show plays (LiveShow.h): its decks and the tracks marked as its channels (their
    input or live notes through their inserts) on its own mixer, on Main; the song stops and the
    other tracks rest.

    While the Sampling Studio samples, it holds one track (setSamplingTap): the track plays the
    studio's notes alone, and its sound goes to the studio as it plays; the song stays stopped.
    Or it holds its own rack of plugin nodes (setSamplingRack, rackTrackId): the rack plays the
    studio's notes and its Output node's sound goes to the studio, and to Main to be heard
    (the rack's MIDI In node, live notes to hear its VSTis by, rests meanwhile).
    Sampling faster than the music (Turbo, Unleashed), the rack is rendered away from the device
    (renderSampling, on the studio's thread), and the device stands aside meanwhile (silence), as
    it does for a mixdown.

    Recording: the audio thread stamps each live note with its song position and
    queues it; updateRecording() files the notes into a clip on every armed track.

    The audio thread holds the graph lock while it renders. The message thread only
    takes that lock for quick pointer swaps (adding or removing a track, installing a
    plugin); anything slow, like loading or destroying a plugin, happens outside it.
*/
class AudioEngine final : public juce::AudioIODeviceCallback,
                          public juce::MidiInputCallback,
                          private juce::ChangeListener
{
public:
    //==============================================================================
    struct TrackMeter
    {
        int id = 0;
        float left = 0.0f, right = 0.0f;
        float inputLeft = 0.0f, inputRight = 0.0f;   // audio tracks: the live input's level, each side as the channel hears it
        bool automated = false;          // automation is moving its volume or pan
        float volume = 0.0f, pan = 0.0f; // their values now, when it is
    };

    struct Status
    {
        bool deviceOpen = false;
        bool running = false;
        juce::String typeName;
        juce::String deviceName;
        double sampleRate = 0.0;
        int bufferSize = 0;
        double bufferMs = 0.0;
        double outputLatencyMs = 0.0;
        double cpu = 0.0;
        int xruns = -1;
        float peakLeft = 0.0f;         // Main channel, after its fader
        float peakRight = 0.0f;
        int hardwareNoteOns = 0;
        juce::uint64 notesLow = 0;     // notes 0-63, one bit each
        juce::uint64 notesHigh = 0;    // notes 64-127
        juce::StringArray midiInputs;  // what tracks can choose: getMidiSources()
        bool released = false;         // the device is let go while Sonify is in the background
        Transport::State transport;
        std::vector<TrackMeter> trackMeters;
        std::vector<TrackMeter> outputMeters;   // the Sub outputs (id: the output's), after their faders
    };

    enum class PluginChange { none, changed, failed };

    //==============================================================================
    AudioEngine();
    ~AudioEngine() override;

    juce::AudioDeviceManager& getDeviceManager() noexcept   { return deviceManager; }

    /** Play, stop, tempo and so on. Safe to call from the message thread at any time. */
    Transport& getTransport() noexcept                      { return transport; }

    // Live notes, message thread
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allNotesOff();

    /** Message thread. Also resets the peak meters, so call it at the UI refresh rate. */
    Status getStatus();

    //==============================================================================
    // Tracks, message thread only.
    const std::vector<std::unique_ptr<Track>>& getTracks() const noexcept   { return tracks; }
    Track* findTrack (int id) const noexcept;
    int getSelectedTrackId() const noexcept                                  { return selectedTrackId; }

    /** Adds an instrument track playing the test synth, selects it and returns its id. wantedId:
        the id it's to have, when no track has it (an undo putting a whole song back as it was);
        otherwise a fresh one. New ids stay above every id given. */
    int addTrack (Track::Kind kind = Track::Kind::instrument, int wantedId = 0);
    void removeTrack (int id);

    /** The selected track is the one that monitors: live notes go to it alone. */
    void selectTrack (int id);

    /** byUser: the user set it (the Monitor button, a control surface, Fanan AI), so Sonify no
        longer turns it on by itself (monitorOnce). */
    void setTrackMonitor (int id, bool shouldMonitor, bool byUser = false);

    /** An instrument plugin was just added to an instrument track: Monitor goes on, once in the
        track's life, unless the user has set it already. */
    void monitorOnce (int trackId);
    void setTrackName (int id, const juce::String& name);

    //==============================================================================
    // Plugins. A channel is a track id, or mainChannel for the Main channel. A plugin
    // on a channel is named by its insert id; instrumentSlot names a track's instrument.
    static constexpr int mainChannel = 0;
    static constexpr int instrumentSlot = 0;

    /** Instruments and note effects replace a track's instrument. Effects go into the
        channel's inserts at position (-1 for the end), or replace the insert replaceInsertId.
        insertIdUsed tells where it went. */
    /** What a plugin load took out: the track's previous instrument, or the insert it
        replaced (with where it was). Kept for undo instead of being destroyed. */
    struct Displaced
    {
        std::unique_ptr<Plugin> instrument;
        std::unique_ptr<EffectChain::Insert> insert;
        int insertPosition = -1;
    };

    /** wantedInsertId: the id a new insert is to have, when no insert has it (an undo putting a
        whole song back as it was); otherwise a fresh one. New ids stay above every id given. */
    juce::Result loadPlugin (int channel, const juce::File& file, const juce::String& pluginId,
                             int position, int replaceInsertId, int& insertIdUsed, Displaced* displaced = nullptr,
                             int wantedInsertId = 0);
    void clearInstrument (int trackId);
    void removeInsert (int channel, int insertId);

    //==============================================================================
    // Clip effects: an audio clip's own effect (MidiClip::effect, Track::ClipEffectSlot), playing
    // that clip alone before the track's inserts. Its plugin is found by getPlugin (the track's id,
    // its slot id) like an insert's, so its window opens the same way. Message thread.

    /** Gives an audio clip an effect of its own (a plugin file and id), in place of any it had. */
    juce::Result setClipEffect (int trackId, int clipId, const juce::File& file, const juce::String& pluginId);
    bool removeClipEffect (int trackId, int clipId);

    /** The effect with this uid as it is now (its plugin's state taken), for a pasted clip; null if
        it's gone. */
    std::shared_ptr<const ClipEffect> clipEffectNow (int uid);

    /** Each clip effect's state, from its plugin into its clip (before the song is saved). */
    void captureClipEffects();

    /** A clip's effect as it plays: its slot id (0: none yet) and why it isn't playing (empty: it is). */
    int clipEffectSlot (int trackId, int clipId, juce::String& error) const;

    //==============================================================================
    // Silence Areas (MidiClip::Silence): an audio clip silent wherever its sound stays quiet.

    using SilenceRegions = std::vector<std::pair<juce::int64, juce::int64>>;

    /** The stretches of a file (its samples in memory) that are silent with these settings. */
    std::shared_ptr<const SilenceRegions> silenceRegions (const std::shared_ptr<const AudioData>& data, const MidiClip::Silence& silence);

    /** The same in seconds of the file, those between from and to (a clip's window), for the page;
        silencedSeconds: how much of that window they take. */
    std::vector<std::pair<double, double>> silenceSeconds (const juce::String& file, const MidiClip::Silence& silence,
                                                           double from, double to, double& silencedSeconds);

    bool setClipSilence (int trackId, int clipId, const MidiClip::Silence& silence);

    /** One of a clip's own automation lines (ClipEnvelope's key), its points in content time; no
        points takes the line away. False when nothing changed. */
    bool setClipEnvelope (int trackId, int clipId, const juce::String& key, std::vector<ClipEnvelopePoint> points);
    void moveInsert (int channel, int insertId, int position);
    void setInsertBypass (int channel, int insertId, bool bypassed);

    const EffectChain& getMainInserts() const noexcept       { return mainInserts; }

    //==============================================================================
    // MIDI FX (MidiFx.h), message thread: the devices in front of an instrument track's
    // instrument, each named by its id (unique in the engine).

    /** Adds a device of a kind (MidiFx::types()) at position (-1: the end) and returns its id;
        0 when there's no such instrument track or kind, or no room. settings as getMidiFx gives
        each device (its id is kept when it's free). */
    int addMidiFx (int trackId, const juce::String& type, int position, const juce::var& settings = {});

    /** Apply's stand-in for the music: for each device that follows a track's notes (Harmonize),
        that track's notes from its clips, at song positions, on the MIDI FX board
        (MidiFx::Board::setOffline). Message thread, before MidiFx::runOffline. */
    void prepareOfflineFollowing (const std::vector<std::unique_ptr<MidiFx::Device>>& devices) const;
    bool removeMidiFx (int trackId, int fxId);
    bool moveMidiFx (int trackId, int fxId, int position);
    bool setMidiFxOn (int trackId, int fxId, bool on);

    /** Returns the value actually used. */
    float setMidiFxParam (int trackId, int fxId, int index, float value);

    /** A track's devices as plain data, for songs, undo and the page: [{ id, type, on,
        params: { <param id>: value }, tool, toolName, generator }]. */
    juce::var getMidiFx (int trackId) const;

    /** Replaces a track's devices with these. */
    void setMidiFx (int trackId, const juce::var& devices);

    /** Every device remake() gives a replacement for is replaced by it, in its slot (a Sonify
        device rebuilt in the Device Studio: its new build, settings kept). Returns how many. */
    int replaceMidiFx (const std::function<std::unique_ptr<MidiFx::Device> (const MidiFx::Device&)>& remake);

    /** What a Magenta device heard: a window for the page's model to answer. */
    struct MagentaRequest
    {
        int trackId = 0, fxId = 0;
        juce::String tool;
        int mode = 0;
        float temperature = 1.0f;
        bool inKey = true;
        MidiFx::MagentaDevice::Window window;
    };

    /** The windows Magenta devices heard since the last call (about 20 times a second). */
    void takeMagentaRequests (std::vector<MagentaRequest>& into);

    /** The page's answer for a Magenta device; false when the device is gone or has no room. */
    bool answerMagenta (int trackId, int fxId, MidiFx::MagentaDevice::Window answer);

    //==============================================================================
    // Songs, message thread.

    /** Back to an empty song: no tracks, an empty Main channel at 0 dB, the transport stopped
        at the start with default settings, and the Live Show as a new show starts (unless
        keepLiveShow: an undo's snapshot of the song, which leaves the show as it is). */
    void clearSong (bool keepLiveShow = false);

    /** A track's instrument (insertId 0) or one of its inserts; channel 0 is Main. A plugin inside
        a container on the channel is found by its node's slot id too. Message thread. */
    Plugin* getPlugin (int channel, int insertId) noexcept;
    /** Where a plugin is: its channel and slot (instrumentSlot, an insert's id, or a container
        node's slot id). False when it's in no channel. Message thread. */
    bool locatePlugin (const Plugin& plugin, int& channel, int& slot) const;
    /** An id no plugin in the song has had: for a plugin inside a container. Message thread. */
    int allocateInsertId() noexcept                          { return nextInsertId++; }

    //==============================================================================
    // Sends and FX channels. A send copies a track's sound, after its fader, to an FX channel
    // (a track of Kind::fx, shown in the Console) or to a plugin's sidechain input (to duck
    // a bass under the kick, say). FX channels don't send, so sends can't go round in circles.
    std::vector<Track::Send> getSends (int trackId) const;
    void setSends (int trackId, std::vector<Track::Send> sends);   // the whole list (undo uses it)
    int addSend (int trackId, Track::Send send);                   // its id, or 0 when there's nowhere to send
    void setSendLevel (int trackId, int sendId, float levelDb);
    void setSendOn (int trackId, int sendId, bool on);
    void removeSend (int trackId, int sendId);

    struct SidechainTarget
    {
        int channel = 0, slot = 0;              // slot: instrumentSlot, or an insert's id
        juce::String pluginName, channelName;
    };

    /** Every plugin in the song with a sidechain input, in the order of the channels. */
    std::vector<SidechainTarget> getSidechainTargets();

    //==============================================================================
    // MIDI in, per track: an instrument track hears the MIDI devices it chose (while it
    // monitors or records) and the notes other tracks' plugins play (always, like a cable).
    // New instrument tracks take the default device (Settings > MIDI Setup), or else the
    // first MIDI device: the user's keyboard.
    void setTrackMidiInputs (int trackId, const juce::StringArray& sources);

    /** An external MIDI device as the engine sees it: a name tracks choose, the port it
        receives from, and what of that gets through. */
    struct MidiDevice
    {
        juce::String name, input;
        juce::uint16 channels = 0xffff;  // the MIDI channels it uses, one bit each
        juce::uint8 blocked = 0;         // Track::Block flags
        bool split = false;              // each channel is a track input of its own too ("Keys|3")
        bool defaultInput = false;
        bool mpe = false;
        int pitchRange = 48;             // MPE: semitones
    };

    /** The external devices (Settings > MIDI Setup). Tracks that chose them hear them at once. */
    void setMidiDevices (std::vector<MidiDevice> devices);

    /** What a track can choose: each device whose port is here (and each of its channels when
        split), then the enabled MIDI ports no device uses, by their own names (not a control
        surface's). */
    const juce::StringArray& getMidiSources() const noexcept    { return midiSources; }

    /** The source a new instrument track hears when no device is the default (Settings > MIDI
        Setup): the first of the track MIDI in list, keyboards first (the page ranks them). */
    void setPreferredMidiSource (const juce::String& source)    { preferredMidiSource = source; }

    /** A device renamed: the tracks that chose it keep it under its new name. */
    void renameMidiSource (const juce::String& from, const juce::String& to);

    /** The ports control surfaces send from (Mackie Control, HUI): no track hears them. What comes
        in on them is kept for the surfaces, which take it on the message thread. */
    void setSurfacePorts (const juce::StringArray& portNames);
    void takeSurfaceMessages (std::vector<std::pair<juce::String, juce::MidiMessage>>& into);

    /** MIDI Link (the toolbar's strip, MidiLink.h): a device's knob, slider, pad or wheel linked
        to one of Sonify's controls. What it sends plays that control and reaches no track. */
    struct LinkSource
    {
        juce::String port;          // the MIDI port, by name
        int kind = 0;               // 0 a controller (CC), 1 a note, 2 pitch bend
        int channel = 1;            // 1 .. 16
        int number = 0;             // the controller's or the note's number; 0 for pitch bend
    };

    void setLinkedSources (std::vector<LinkSource> sources);

    /** Every controller, note and pitch bend from the MIDI ports (not a control surface's) since
        the last call, for MIDI Link: the port it came in on, and it. */
    void takeLinkMessages (std::vector<std::pair<juce::String, juce::MidiMessage>>& into);

    //==============================================================================
    // Sync: MIDI clock and timecode to external devices while the song plays.
    void setSyncTargets (const std::vector<MidiSync::Target>& targets, MidiSync::Rate rate)   { midiSync.setTargets (targets, rate); }
    juce::StringArray getSyncProblems() const                                                { return midiSync.getProblems(); }

    /** Lets the audio device go for other apps (Sonify in the background) or takes it back.
        Message thread. */
    void releaseDevice (bool shouldRelease);
    bool isDeviceReleased() const noexcept                   { return deviceReleased; }

    /** How many blocks the device has asked for: it moves on while the device runs (MainComponent's
        watch sees one that stopped by itself). Any thread. */
    juce::uint32 getDeviceCallbacks() const noexcept          { return deviceCallbacks.load (std::memory_order_relaxed); }

    /** In a block the device asked for, not yet done with it (a plugin taking its time over it):
        the device is fine meanwhile (closing it would only wait for that block). Any thread. */
    bool isInDeviceCallback() const noexcept                  { return deviceInCallback.load(); }

    /** The device opened again with the setup it last ran with (it stopped by itself: a driver's
        reset that never came back, a dropout, the computer waking). True when it runs again.
        Message thread. */
    bool reopenDevice (const juce::AudioDeviceManager::AudioDeviceSetup& setup);

    /** True when any plugin reported a change to its state since the last call. */
    bool consumePluginStateChanges() noexcept;

    //==============================================================================
    // Undo support, message thread. Taking something out keeps it alive, plugins and
    // all, for the undo history; putting it back first makes it ready for the device.

    std::vector<MidiClip> getClips (int trackId) const;
    void setClips (int trackId, std::vector<MidiClip> clips);

    /** Takes a track out of the song; index says where it was. */
    std::unique_ptr<Track> detachTrack (int id, int& index);
    void attachTrack (std::unique_ptr<Track> track, int index);

    /** The tracks in this order (ids); any left out keep their place after them. */
    void setTrackOrder (const std::vector<int>& ids);

    /** Takes an insert out of its chain; position says where it was. */
    std::unique_ptr<EffectChain::Insert> detachInsert (int channel, int insertId, int& position);
    bool attachInsert (int channel, std::unique_ptr<EffectChain::Insert> insert, int position);
    int getInsertPosition (int channel, int insertId) noexcept;

    /** Puts an instrument (or none: the test synth) on a track and returns the one it had. */
    std::unique_ptr<Plugin> swapInstrument (int trackId, std::unique_ptr<Plugin> instrument);

    /** True while a recording take is being filed into clips. */
    bool isTakeActive() const noexcept                        { return take.active || audioTakeActive; }

    //==============================================================================
    // Mixdown (Song > Export Mixdown), message thread. The song renders faster than real
    // time on a thread of its own while the device plays silence; afterwards the transport
    // is put back as it was.
    struct MixdownSettings
    {
        enum class Format { wav, flac, ogg, mp3 };

        juce::File file;                // Main's mix; none for stems alone
        Format format = Format::wav;
        int bits = 24;                  // WAV: 16, 24 or 32 (floating point); FLAC: 16 or 24
        int kbps = 320;                 // MP3 and OGG
        double outputRate = 0.0;        // 0: the device's rate; otherwise converted to it
        double startPpq = 0.0, endPpq = 0.0;
        double tailSeconds = 2.0;       // after the range, for effects to ring out

        /** Stems: each channel's own sound (after its inserts, fader and pan; solos set aside,
            mutes kept) to a file of its own, from the same render as Main's mix. */
        struct Stem
        {
            int trackId = 0;            // a track or an FX channel
            juce::File file;
        };

        std::vector<Stem> stems;

        /** A track's freeze instead (its id; 0: none): its own sound before its fader, into
            freezeFile (a float WAV at the device's rate). Only it plays, and the tracks it needs
            (freezeSources: those it takes notes from, those keying its plugins' sidechains). */
        int freezeTrackId = 0;
        juce::File freezeFile;
        std::vector<int> freezeSources;

        std::vector<int> onlyClipIds;   // with a freeze: only these clips of the track play (Export Clip to Audio,
                                        // Merge Clips, Bounce to New Track)
        bool dry = false;               // with a freeze: the track's inserts left out (Merge Clips)
    };

    /** ".wav", ".flac", ".ogg" or ".mp3". */
    static juce::String extensionOf (MixdownSettings::Format format);

    juce::Result startMixdown (MixdownSettings settings);
    bool isMixdownRunning() const noexcept                   { return mixdown != nullptr; }
    double getMixdownProgress() const noexcept;
    void cancelMixdown();

    /** Once the render has ended: puts the song back as it was, and says how it went. */
    std::optional<juce::Result> collectMixdown (juce::File& written);

    /** Where the song ends, in quarter notes: the end of its last clip or envelope point. */
    double getSongEnd() const;

    //==============================================================================
    // Freezing, message thread. A frozen track (instrument or audio) plays a render of its own
    // sound as it feeds its mixer, before its fader (its clips through its instrument or as they
    // are, its MIDI FX, and on the Console its inserts) in place of all that, which rests: its
    // fader, pan, mute, solo and sends still work. Anything that changes what it plays (its
    // clips, plugins, their parameters and envelopes, a track it takes notes or its sidechain
    // key from, that key's fader, pan or mute, the tempo, the key, the mixer) thaws it again.
    // Renders stay on the disk while the song is open (an undo can freeze a track again);
    // discardRenders tidies up.

    /** Renders the track over the song into file (a mixdown of its own: the device is quiet
        meanwhile); collectMixdown says when it's done, and applyFreeze plays it. */
    juce::Result startFreeze (int trackId, const juce::File& file);

    /** Clips alone, as their track plays them (its sound before its fader), from the first one's
        start to the last one's end (and tailSeconds after, for what rings on) into file (a float
        WAV at the device's rate); collectMixdown says when it's done, and the track plays all its
        clips again. Export Clip to Audio and Bounce to New Track take the track's inserts; Merge
        Clips leaves them out (dry: each clip with its own effect, gain, fades and Silence Areas). */
    juce::Result startClipRender (int trackId, const std::vector<int>& clipIds, const juce::File& file,
                                  double tailSeconds = 0.0, bool dry = false);

    /** The render startFreeze made, played by its track, if the song is still as it was
        rendered (the track and what it needs unchanged, the same tempos and mixer). */
    juce::Result applyFreeze (const juce::File& written);

    /** The track plays a render from now on: dry, one made for the Analog Board. justMade: this
        session rendered it (so it goes with the song unless a saved song names it). False when
        the file can't be played. */
    bool freezeTrack (int trackId, const juce::File& file, bool dry, bool justMade = false);

    /** It plays itself again. */
    void unfreezeTrack (int trackId);

    /** The song was saved: its frozen tracks' renders are kept from now on. */
    void markFrozenSaved();

    /** The song went (another opened, a new one, the song closed) with its undo steps: the
        renders made for it that no saved song names are deleted, and the renders no track plays
        leave memory. songGoing: the song open now goes too (Sonify quits). */
    void discardRenders (bool songGoing = false);

    /** A frozen track's render, by its path. */
    bool isFrozenRender (const juce::String& path) const;

    /** The track freezing now, or one it needs (their plugins' windows stay closed meanwhile). */
    bool isFreezeRendering (int trackId) const noexcept;

    /** Called often: a frozen track whose sidechain key's fader, pan or mute moved (whatever
        moved it) thaws. */
    void checkFrozenKeys();

    /** The tracks a change thawed since this was last asked (their ids). */
    std::vector<int> takeThawed()                            { return std::exchange (thawed, {}); }

    /** What a track plays changed (its clips, plugins, their parameters or envelopes): its
        content revision moves on, and a frozen track thaws. */
    void contentChanged (int trackId);

    //==============================================================================
    // Automation, message thread.
    const std::vector<AutomationLane>* getAutomation (int trackId) const;

    /** Replaces the envelope for its target; one with no points goes. */
    bool setAutomationLane (int trackId, AutomationLane lane);

    /** All of a track's envelopes at once (undo, songs). */
    bool setAutomationLanes (int trackId, std::vector<AutomationLane> lanes);

    void setAutomationMode (int trackId, AutomationMode mode);

    //==============================================================================
    // Audio inputs and recording, message thread.

    /** A song input, as in Studio One's Audio I/O Setup: a name and where it is among the
        device's active input channels (the order the audio callback gets them in). */
    struct SongInput
    {
        juce::String name;
        int left = 0, right = -1;        // right -1: mono
        std::vector<int> also;           // a mono input's other channels (a mono channel hears them all as one; a stereo one, on its sides)
    };

    static std::vector<SongInput> defaultInputs();
    const std::vector<SongInput>& getInputs() const noexcept   { return inputs; }
    void setInputs (std::vector<SongInput> newInputs);
    void setTrackInput (int trackId, int inputIndex);

    /** An audio track's channel: mono (its input as one sound, on both sides, recorded as one)
        or stereo (each of its input's channels on a side of its own). Not while it records. */
    void setTrackMono (int trackId, bool mono);

    /** A choir track (Choir.h): an audio track whose clips sing as a choir too. Makes an audio
        track one, or changes its choir's settings (a frozen one plays itself again). False when
        it isn't an audio track. */
    bool setTrackChoir (int trackId, const ChoirSettings& settings);

    /** A choir track's settings; null when the track isn't one. */
    const ChoirSettings* getTrackChoir (int trackId) const;

    /** The device's active input channels, by name, in the callback's order. */
    juce::StringArray getInputChannelNames() const;

    //==============================================================================
    // Audio outputs, message thread. Every output channel of the device is open, and the song's
    // outputs (Studio One's Audio I/O Setup > Outputs) say where things play.

    /** A song output: Main (id 0, always the first) or a Sub output that tracks and FX channels
        can play into instead of Main (to a headphone mix, an outboard effect, or another app
        down a virtual cable). Its channels are the device's (right -1: mono). A Sub has a fader
        and a mute of its own, and can play the Main mix too. */
    struct SongOutput
    {
        int id = 0;
        juce::String name { "Main" };
        int left = 0, right = 1;
        bool mainMix = false;            // a Sub: the Main mix (after its fader) plays here too
        float volumeDb = 0.0f;           // a Sub's fader; Main's is the Main channel's
        bool mute = false;
    };

    static constexpr int maxSubOutputs = 32;

    /** Main on the device's first two output channels, and no Subs. */
    static std::vector<SongOutput> defaultOutputs();
    const std::vector<SongOutput>& getOutputs() const noexcept   { return outputs; }

    /** The song's outputs: Main first (made if missing), then up to maxSubOutputs Subs. A new
        output (id below 0) gets an id the song has never used, so tracks that played into a
        removed Sub never land on a new one; audition, when given, may name a new output by the
        id it came with. Tracks playing into a Sub that's gone play into Main. */
    void setOutputs (std::vector<SongOutput> newOutputs, std::optional<int> audition = std::nullopt);

    /** The next id a new output gets: songs keep it, so ids are never used twice. */
    int getNextOutputId() const noexcept                       { return nextOutputId; }
    void setNextOutputId (int next) noexcept                   { nextOutputId = juce::jmax (1, next); }
    void setTrackOutput (int trackId, int outputId);

    /** A track plays into a bus (its id) instead of its output; 0: back to its output. Only
        tracks play into buses (not FX channels, nor buses). */
    void setTrackBus (int trackId, int busId);
    int getTrackBus (int trackId) const;    // the bus it plays into, when that bus is in the song
    void setOutputVolume (int outputId, float volumeDb);   // a Sub's fader
    void setOutputMute (int outputId, bool mute);

    /** Where the Browser's preview and the metronome play: an output's id (0: Main). */
    void setAuditionOutput (int outputId);
    int getAuditionOutput() const noexcept                     { return auditionOutput; }

    /** What new songs, and songs saved before they had outputs, start with (Song Setup's Make Default). */
    void setSongDefaults (std::vector<SongInput> defaultInputs, std::vector<SongOutput> defaultOutputs, int audition);
    const std::vector<SongInput>& getSongDefaultInputs() const noexcept     { return songDefaultInputs; }
    const std::vector<SongOutput>& getSongDefaultOutputs() const noexcept   { return songDefaultOutputs; }
    int getSongDefaultAudition() const noexcept                             { return songDefaultAudition; }

    /** Every output channel of the device, by name (the last device's while none is open). */
    juce::StringArray getOutputChannelNames();

    /** The output pair the device played on when Sonify started, if it wasn't the first pair
        (Settings used to choose one pair): {-1, -1} otherwise. */
    std::pair<int, int> getStartupOutputPair() const noexcept  { return startupOutputPair; }

    /** Opens every output channel of the device if some aren't (not while recording or exporting).
        Called when the device changes, and now and then in case a change came meanwhile. */
    void openAllOutputChannels();

    /** A short tone on Main's channels (Settings' Test). Any thread. */
    void playTestTone() noexcept                               { testToneRequested.store (true); }

    /** Where recordings go: the song's Media folder (empty: Documents\Fanan Web Hub\Recordings). */
    void setRecordingFolder (const juce::File& folder)          { recordingFolder = folder; }
    const juce::File& getRecordingFolder() const noexcept       { return recordingFolder; }

    /** WAV sample format for recordings: 16, 24 or 32 (floating point). */
    void setRecordBits (int bits);
    int getRecordBits() const noexcept                         { return recordBits; }

    AudioMedia& getMedia() noexcept                             { return media; }
    double getPlaybackRate() const noexcept                    { return currentSampleRate > 0.0 ? currentSampleRate : 48000.0; }
    int getBlockSize() const noexcept                          { return currentBlockSize > 0 ? currentBlockSize : 512; }

    //==============================================================================
    // The Plugins Creator Studio's live preview (CreatorAudio.h): the plugin being made plays on
    // the Audition output after everything else, to the device only (never into a mixdown). While
    // the studio listens, the live notes (the MIDI devices', the page's) go to it alone, not to
    // the tracks, so a keyboard plays the plugin being made and nothing else.
    struct CreatorPreview
    {
        virtual ~CreatorPreview() = default;

        /** Audio thread, under the graph lock. live: the notes played this block (empty while the
            studio doesn't listen); inLeft / inRight: the device's first two inputs (either may be
            null), for an effect to work on; song: the song's tempo and position, for tempo sync.
            Adds into left and right. */
        virtual void renderPreview (const juce::MidiBuffer& live, const float* inLeft, const float* inRight,
                                    float* left, float* right, int numSamples, const clap_event_transport_t& song) noexcept = 0;
    };

    /** Message thread. nullptr: none. */
    void setCreatorPreview (CreatorPreview* preview);

    /** Whether the live notes go to the preview (the Plugins Creator Studio in front); the tracks
        then hear only what lets go of their notes. Any thread. */
    void setCreatorListening (bool shouldListen) noexcept      { creatorListening.store (shouldListen); }
    bool isCreatorListening() const noexcept                   { return creatorListening.load(); }

    //==============================================================================
    // MIDI In: live MIDI (the devices', the page's keyboards) goes to the studio in front only
    // (the page's selector says which: MainComponent.cpp). The song's studios share it, the
    // tracks hearing it as they choose (the Studio, the Analog Board, Song Builder Blocks, the
    // Live Show); the Plugins Creator Studio (unless a MIDI FX is open: that plays on its track)
    // and the Sampling Studio (its rack's MIDI In node) take it for themselves, from the inputs
    // each one picks; Sonify AI, the Web Browser and the Guide hear none. What isn't heard still
    // lets go of what it held (note-offs, pedals up), so nothing hangs. Control surfaces and MIDI
    // Link are Sonify's own, in every studio.
    enum class MidiFocus { song, creator, sampling, none };

    /** Message thread. */
    void setMidiFocus (MidiFocus focus) noexcept               { midiFocus.store (static_cast<int> (focus)); }
    MidiFocus getMidiFocus() const noexcept                    { return static_cast<MidiFocus> (midiFocus.load()); }

    /** The studios with a MIDI In of their own. */
    enum class MidiIn { creator = 0, rack = 1 };
    static constexpr int numMidiIns = 2;
    static constexpr const char* everyMidiInput = "*";

    /** The ports a MIDI In hears, by name (everyMidiInput: all of them, as they come and go); the
        page's keyboards it always hears. Message thread. */
    void setMidiInInputs (MidiIn in, const juce::StringArray& ports);
    juce::StringArray getMidiInInputs (MidiIn in) const;

    /** What a MIDI In can pick: the enabled MIDI inputs, but a control surface's; device: the
        MIDI device (Settings > MIDI Setup) on that port, if there's one. Message thread. */
    struct MidiInPort
    {
        juce::String name, device;
    };

    std::vector<MidiInPort> getMidiInPorts() const;

    /** The rack's MIDI In listens only while the nodes show. Any thread. */
    void setRackMidiInOpen (bool open) noexcept                { rackMidiInOpen.store (open); }

    /** Goes up as a MIDI In lets messages through (its light blinks with it). Any thread. */
    juce::uint32 getMidiInActivity (MidiIn in) const noexcept
    {
        return midiIns[static_cast<size_t> (in)].activity.load (std::memory_order_relaxed);
    }

    /** The MIDI monitor (the Sampling Studio's): what comes in while it's on, oldest first.
        Clock and active sensing aren't kept. */
    struct MonitoredMessage
    {
        double time = 0.0;          // seconds, on Time::getMillisecondCounterHiRes()'s clock
        juce::String source;        // the port's name, or "Keyboard" (the page's keyboards)
        juce::uint8 bytes[3] {};
        int size = 0;               // 1..3; a SysEx's first three bytes, with its length in sysexLength
        int sysexLength = 0;
        bool heard = false;         // the studio in front takes it
    };

    void setMidiMonitor (bool on) noexcept                     { monitorOn.store (on); }
    void takeMonitored (std::vector<MonitoredMessage>& into);   // message thread

    //==============================================================================
    // The Sampling Studio's hold on one track while it samples (SamplingStudio.h): the track
    // hears the studio's notes and nothing else (no keyboard, no MIDI device, no other track's
    // notes), plays live (an audio track takes its input, monitored or not, and is heard only
    // while it monitors; a frozen track plays its instrument, not its render), and its sound as
    // it comes out of its inserts (before its fader) goes to the studio, block by block. The song
    // stays stopped meanwhile. On the device only: a mixdown or a freeze never hears it.
    struct SamplingTap
    {
        virtual ~SamplingTap() = default;

        /** Audio thread, under the graph lock, before the track plays: this block's notes for it. */
        virtual void tapNotes (juce::MidiBuffer& into, int numSamples) noexcept = 0;

        /** Audio thread, under the graph lock, after it played: its sound this block. */
        virtual void tapSound (const float* left, const float* right, int numSamples) noexcept = 0;

        /** The tempo the track hears now (a BPM layer's), 0 for the song's. Audio thread. */
        virtual double tapTempo() const noexcept   { return 0.0; }
    };

    /** Message thread: the tap, and the track it holds (null: none); rackTrackId: the Sampling
        Studio's rack (setSamplingRack) instead of a track. renderOffline: rendered by
        renderSampling (Turbo, Unleashed), never by the device. */
    void setSamplingTap (SamplingTap* tap, int trackId, bool renderOffline = false);

    /** The Sampling Studio's rack of plugin nodes (SamplingRack.h), as setSamplingTap's and
        auditionNote's track. */
    static constexpr int rackTrackId = -1;

    /** The Sampling Studio's rack: its plugins, wired from the studio's notes to its Output node,
        played by the engine while it's awake (the studio shows, or samples), on Main. */
    struct RackPlayer
    {
        virtual ~RackPlayer() = default;

        /** The device starts (no audio callback meanwhile), under the graph lock: the plugins
            activated at its rate and block size. */
        virtual void prepareRack (double sampleRate, int blockSize) = 0;

        /** The device stopped, under the graph lock. */
        virtual void releaseRack() = 0;

        /** Whether the device plays it now. Any thread. */
        virtual bool isRackAwake() const noexcept = 0;

        /** Audio thread (or Turbo's), under the graph lock: one block, the studio's notes in (and
            live: its MIDI In node's, empty while it samples); the Output node's sound added into
            left and right. */
        virtual void renderRack (const juce::MidiBuffer& notes, const juce::MidiBuffer& live, float* left, float* right,
                                 int numSamples, const clap_event_transport_t& transport) noexcept = 0;
    };

    /** Message thread. nullptr: none. */
    void setSamplingRack (RackPlayer* rack);
    bool isSampling() const noexcept                           { return samplingOn.load(); }

    /** Turbo and Unleashed, the studio's thread: the held track's next block (its notes from the
        tap, its sound to it), at most getBlockSize() samples. False when there's nothing to
        render (no offline tap, the track gone), or not now (a mixdown renders, the device is
        stopped or starting again). */
    bool renderSampling (int numSamples);

    /** The Sampling Studio's audition (a right-click on a key): a note to one instrument track,
        whatever is selected, or to the rack (rackTrackId). Any thread. */
    void auditionNote (int trackId, int note, int velocity, bool on);

    //==============================================================================
    // Recording and clips, message thread.

    /** Records on the armed tracks (the selected one, armed for you, when none is) and
        starts playback if it's stopped. */
    void startRecording();

    /** Punches out; playback goes on. */
    void stopRecording();
    bool isRecording() const noexcept                         { return transport.getState().recording; }

    void setTrackRecordArm (int id, bool shouldArm);

    /** Quantizing notes onto a grid (the page's Quantize panel; for recording, Record Quantize: the
        notes a take recorded go onto it as the take ends, in the same undo step). grid in quarter
        notes, laid from each bar's start (a bar or more: the nearest bar line); strength 0..1;
        swing 0..1 moves every second grid position later by up to a third of the grid (1: a
        triplet shuffle); starts and ends say what moves. */
    struct NoteQuantize
    {
        bool on = false;
        double grid = 0.25, strength = 1.0, swing = 0.0;
        bool starts = true, ends = false;
    };

    void setRecordQuantize (const NoteQuantize& settings) noexcept   { recordQuantize = settings; }
    const NoteQuantize& getRecordQuantize() const noexcept            { return recordQuantize; }

    /** A song position moved toward its grid position. */
    static double quantized (const TempoMap& map, double ppq, const NoteQuantize& settings) noexcept;

    /** Puts a clip on a track (its notes and controller lines sorted, given a fresh id, or keeping
        its own when keepId and no clip has it: an undo putting a whole song back as it was);
        returns the id, or 0. */
    int addClip (int trackId, MidiClip clip, bool keepId = false);
    bool deleteClip (int trackId, int clipId);

    /** Where a clip sits and which part of its content it shows. */
    struct ClipWindow
    {
        double start = 0.0, length = 4.0, offset = 0.0;
    };

    /** Replaces a clip's notes (content positions; impossible ones are dropped), its pitch bend
        and aftertouch lines when given, its window too when one is given, and its seed (what its
        notes' operators roll from) when given. The Editor sends the window when an edit grows the
        clip. */
    bool setClipNotes (int trackId, int clipId, std::vector<MidiNote> notes, const ClipWindow* window = nullptr,
                       const std::vector<MidiControlPoint>* pitchBend = nullptr, const std::vector<MidiControlPoint>* aftertouch = nullptr,
                       const juce::uint32* seed = nullptr);

    /** Moves a clip to a song position, on the same track or another. */
    bool moveClip (int trackId, int clipId, int toTrackId, double newStart);

    /** Copies a clip to a track and song position; returns the copy's id, or 0. */
    int copyClip (int trackId, int clipId, int toTrackId, double newStart);

    /** New start and length, the notes staying where they are in the song: trimming hides
        notes rather than deleting them, and dragging the edge back shows them again. */
    bool setClipRange (int trackId, int clipId, double newStart, double newLength);

    /** Splits a clip at a song position inside it: notes and controller points from there on
        go to the new right-hand clip (which starts with the bend and aftertouch in effect
        there). Returns its id, or 0. */
    int splitClip (int trackId, int clipId, double at);

    /** An audio clip's own volume and fades (seconds), as on Studio One's events. The fades
        together never exceed the clip. */
    bool setClipAudio (int trackId, int clipId, double gainDb, double fadeIn, double fadeOut);

    /** The arrangement's Mute tool: a clip (MIDI or audio) muted stays in the song, silent. */
    bool setClipMuted (int trackId, int clipId, bool muted);

    /** Puts a whole audio file on an audio track as a clip starting at a song position.
        Returns the clip's id, or 0 (not an audio track, or not a file the Hub can read). */
    int importAudio (int trackId, const juce::File& file, double start);

    //==============================================================================
    // Voice control: one song input heard by the voice listener (VoiceControl), and short
    // cues for the musician (heard in the headphones, never recorded or mixed down).

    /** Which song input the listener hears (-1: none), message thread. A stereo input is heard as mono. */
    void setVoiceInput (int inputIndex);
    int getVoiceInput() const noexcept                          { return voiceInput; }

    /** While on, the audio thread queues the voice input's samples for the listener. */
    void setVoiceListening (bool shouldListen) noexcept         { voiceListening.store (shouldListen); }

    /** The listener's own thread: the queued samples (mono, at getPlaybackRate()). Returns how many. */
    int readVoice (float* destination, int maxSamples) noexcept;

    enum class Cue { listening, heard, done, question, problem };

    /** Plays a cue after everything else, from any thread. */
    void playCue (Cue cue) noexcept                             { pendingCue.store (static_cast<int> (cue) + 1); }
    bool isCuePlaying() const noexcept                          { return cueSounding.load(); }

    /** True while the audio device runs (the callback is being called). */
    bool isAudioRunning() const noexcept                        { return audioRunning.load(); }

    //==============================================================================
    // The Browser's preview, message thread: one file at a time, after the Main fader.
    bool previewAudio (const juce::File& file);
    bool startPreview (const juce::File& file, std::shared_ptr<const AudioData> data);
    int previewRequest = 0;   // message thread: which preview asked last (previewAudioWhenRead)
    void stopPreview();

    /** The Loop Editor's preview: a sample from its start, then round its loop (file samples)
        for ever, its last crossfade samples faded (equal power) into those just before its start,
        as an SFZ player does.
        setPreviewLoop moves the loop while it plays. getPreviewFilePosition: where it plays, in
        file samples (-1: nothing playing). */
    bool previewLoop (const juce::File& file, juce::int64 loopStart, juce::int64 loopEnd, int crossfade);
    void setPreviewLoop (juce::int64 loopStart, juce::int64 loopEnd, int crossfade);
    juce::int64 getPreviewFilePosition() const noexcept;
    const juce::String& getPreviewFile() const noexcept         { return previewPath; }

    static constexpr double minimumClipLength = 1.0 / 16.0;   // quarter notes

    /** True for the clip a take is recording into right now. */
    bool isRecordingInto (int trackId, int clipId) const noexcept;

    /** The Browser's preview: the file read on its own thread first (a long one doesn't hold the
        app up); done (message thread) says whether it plays. A newer preview, or stopping, while
        it's read: this one never starts. */
    void previewAudioWhenRead (const juce::File& file, std::function<void (bool)> done);

    /** Call about 20 times a second: files recorded notes into clips. True when clips changed. */
    bool updateRecording();

    /** Returns the value actually used, after clamping and rounding. */
    double setPluginParameter (int channel, int insertId, int index, double value);
    juce::String getPluginParameterText (int channel, int insertId, int index);

    /** Call about 20 times a second. A failed plugin is removed and message says why. */
    PluginChange updatePlugins (juce::String& message);

    //==============================================================================
    // Plugin delay compensation (message thread). Every track is made as late as the slowest
    // track's instrument and inserts, every FX channel and bus as late as the slowest of them,
    // and what goes straight to Main or a Sub waits for the FX channels and buses: so whatever
    // latencies the plugins have, everything meets in time. The metronome, recordings and
    // renders (mixdowns, stems, freezes) are put back in step with the song.
    void updateCompensation();

    /** How late a render's sound is (samples), to leave out at its start: trackId 0 Main's mix,
        else that track's stem; freeze: that track's own sound for its freeze (dry: no inserts). */
    int renderLatency (int trackId, bool freeze, bool dry) const;

    /** How late the song is heard now, in samples (all of the compensation and Main's inserts). */
    int getOutputLatency() const noexcept                       { return outputLatencySamples.load (std::memory_order_relaxed); }

    /** A plugin's parameters read again, the device waiting only for the swap of its lists. */
    void rescanWithoutWaiting (Plugin& plugin);

    //==============================================================================
    // Main channel: every track passes through it. The metronome is added after it,
    // so the click keeps its own level whatever the Main fader does.
    void setMasterVolume (float db) noexcept;
    void setMasterMute (bool shouldMute) noexcept             { masterMute.store (shouldMute); }
    float getMasterVolume() const noexcept                    { return masterVolumeDb.load(); }
    bool isMasterMuted() const noexcept                       { return masterMute.load(); }

    /** Main's volume envelope (the song channel's Main Volume lane): dB by song position, moving
        the Main fader while the song plays. None: the fader stays where it's put. */
    void setMainAutomation (std::vector<AutomationPoint> points);
    const std::vector<AutomationPoint>& getMainAutomation() const noexcept   { return mainAutomation; }

    /** The song's Song Blocks and their order (SongBlocks.h): song data, nothing plays from it.
        Each change counts up a generation, so the page hears of it once. Message thread. */
    const SongBlocks& getSongBlocks() const noexcept          { return songBlocks; }
    void setSongBlocks (SongBlocks blocks)                    { songBlocks = std::move (blocks); ++songBlocksGeneration; }
    int getSongBlocksGeneration() const noexcept              { return songBlocksGeneration; }

    /** The song's video (one, as in Studio One): its file, and the song position its picture
        starts at (quarter notes, as a clip's: tempo changes before it move it as they move its
        sound's clip). Song data: its sound plays as an ordinary audio clip, and the Video window
        shows its picture where the song is (MainVideo.cpp). Message thread. */
    struct SongVideo
    {
        juce::File file;
        double start = 0.0;

        bool isEmpty() const noexcept                             { return file == juce::File(); }
        bool operator== (const SongVideo& other) const noexcept   { return file == other.file && start == other.start; }
        bool operator!= (const SongVideo& other) const noexcept   { return ! operator== (other); }
    };

    const SongVideo& getSongVideo() const noexcept            { return songVideo; }
    void setSongVideo (SongVideo video)                       { songVideo = std::move (video); ++songVideoGeneration; }
    int getSongVideoGeneration() const noexcept               { return songVideoGeneration; }

    //==============================================================================
    // The Analog Board (AnalogBoard.h), message thread. Its settings are atomics: set them at
    // any time. Switching between the Console and the board fades across two blocks.

    /** The song mixed on the board (true) or on the Console. Frozen tracks rendered for the
        other one thaw. */
    void setBoardMixing (bool shouldMix);
    bool isBoardMixing() const noexcept                       { return boardWanted.load(); }

    /** The board's master section; each track's strip is its own (Track::board). */
    AnalogBoard::Master& getBoard() noexcept                  { return board; }
    const AnalogBoard::Master& getBoard() const noexcept      { return board; }

    /** Every strip and the master back to where a new board starts (a new song). */
    void resetBoard();

    /** True while no strip nor the master has moved from where it starts. */
    bool isBoardFresh() const;

    /** Each strip takes its track's Console fader, pan, mute and solo; the master takes Main's fader. */
    void copyConsoleToBoard();

    //==============================================================================
    // The Live Show (LiveShow.h), message thread: two or four decks, the tracks marked as its
    // channels (their input or live notes through their inserts), the ducker and the master,
    // playing on Main in place of the song (while its tab shows, or it still plays: MainLive.cpp).
    // Switching fades across two blocks. While the show plays, the song is stopped and stays
    // stopped (whoever asks it to play). While its tab shows, the other tracks rest; while it
    // plays with its tab out of sight, the Studio is heard with it (setStudioWithShow). A mixdown
    // is never the show: none starts while it plays.

    void setLiveShowOn (bool shouldPlay) noexcept             { liveWanted.store (shouldPlay); }
    bool isLiveShowOn() const noexcept                        { return liveWanted.load(); }

    /** While the show has Main and its tab is out of sight (the Studio in front, say), the Studio
        is heard with it, as when it rests: its tracks (an input monitored, an instrument played),
        FX channels and buses through Main's inserts and fader (or the Analog Board's master while
        it mixes), the show on top at its own master (its recording and stream are the show's
        alone). Its own channels' tracks, and their solo, stay the show's. The song stays stopped,
        and a switch to or from the board waits for the show to end. The Studio coming in or going
        fades over a block; the show starting or ending meanwhile, the Studio plays on through it. */
    void setStudioWithShow (bool heard) noexcept              { studioWithShow.store (heard); }

    LiveShow::Mixer& getLiveShow() noexcept                   { return liveShow; }
    const LiveShow::Mixer& getLiveShow() const noexcept       { return liveShow; }

    /** Marks an instrument or audio track as one of the show's channels, or not. */
    void setTrackLive (int trackId, bool isChannel);

    /** Puts a track on a deck (null: none), stopped at its start, its cue point there; the track
        it had is let go here, on the message thread. */
    void setDeckTrack (int deck, std::shared_ptr<const LiveShow::DeckTrack> track);
    std::shared_ptr<const LiveShow::DeckTrack> getDeckTrack (int deck) const;

    /** The files a song's decks had when it was saved: Song::restore leaves them here, and the
        Live Show loads them (MainLive.cpp). Empty files: nothing for that deck. */
    void setDeckFilesToLoad (const std::array<juce::File, LiveShow::maxDecks>& files)   { deckFilesToLoad = files; deckFilesWaiting = true; }
    bool takeDeckFilesToLoad (std::array<juce::File, LiveShow::maxDecks>& files)        { files = deckFilesToLoad; return std::exchange (deckFilesWaiting, false); }

    /** A pad's sound (null: none), stopped; the sound it had is let go here, on the message thread. */
    void setPadSound (int pad, std::shared_ptr<const LiveShow::DeckTrack> sound);
    std::shared_ptr<const LiveShow::DeckTrack> getPadSound (int pad) const;

    /** The pads' files, as the decks' (Song::restore, then MainLive.cpp). */
    void setPadFilesToLoad (const std::array<juce::File, LiveShow::numPads>& files)     { padFilesToLoad = files; padFilesWaiting = true; }
    bool takePadFilesToLoad (std::array<juce::File, LiveShow::numPads>& files)          { files = padFilesToLoad; return std::exchange (padFilesWaiting, false); }

    /** The Live Show's own, kept with the song: the way it's used (its tab) and its playlist.
        MainLive.cpp keeps them here as they change (Song::capture saves them); Song::restore
        leaves a song's, for MainLive.cpp to take. */
    /** Main's placeholders from a DAWproject (Track::kept, for Main): kept with the song, written
        back by Export DAWproject. Message thread. */
    void setMainKept (const juce::var& kept)                    { mainKept = kept; }
    const juce::var& getMainKept() const noexcept              { return mainKept; }

    void keepLiveShowExtras (const juce::var& extras)          { liveExtras = extras; }
    const juce::var& getLiveShowExtras() const noexcept       { return liveExtras; }
    void setLiveShowExtras (const juce::var& extras)           { liveExtras = extras; liveExtrasWaiting = true; }
    bool takeLiveShowExtras (juce::var& extras)                { extras = liveExtras; return std::exchange (liveExtrasWaiting, false); }

    /** The Live Show's headphones: a Sub output's id (0: none). While the show plays, that Sub
        plays the headphones (its fader and mute still count; its Main mix doesn't). */
    void setLivePhonesOutput (int outputId);
    int getLivePhonesOutput() const noexcept                  { return livePhonesOutput; }

    /** Recording the night: the show as it's heard (Show.wav) and, with stems, each deck shown
        and each of the show's channels on a file of its own, in a folder of its own under the
        recordings folder. It records while the show has Main. */
    struct LiveTakeFiles
    {
        juce::File folder, show;
        std::vector<juce::File> stems;
    };

    /** The show's sound for a live stream (LiveStream.h), as it's heard: made and put in place
        here (4 seconds of room), let go of by closeLiveTap once nothing reads it any more. */
    LiveShow::SoundTap* openLiveTap();
    void closeLiveTap();

    juce::Result startLiveRecording (bool stems);
    LiveTakeFiles stopLiveRecording();                        // the files, closed
    bool isLiveRecording() const noexcept                     { return liveTake.show != nullptr; }
    double getLiveRecordedSeconds() const noexcept;

    //==============================================================================
    // The song channel's tempo map (tempo changes and ramps, time signatures, keys), message
    // thread. Audio clips follow it at once: they keep their speed, so they cover more or
    // fewer quarter notes.
    void setTempoMap (const TempoMap& map);
    const TempoMap& getTempoMap() const noexcept               { return transport.getTempoMap(); }

    /** How many quarter notes audio of this length covers from a position; the time between two positions. */
    double audioLengthInPpq (double startPpq, double seconds) const noexcept;
    double secondsBetween (double fromPpq, double toPpq) const noexcept;

    //==============================================================================
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

private:
    //==============================================================================
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void openDevices();
    void saveDeviceState();
    void refreshMidiInputNames();
    void queue (juce::MidiMessage message);
    void trackNotes (const juce::MidiBuffer& buffer) noexcept;
    void captureRecording (const juce::MidiBuffer& live, bool mpe) noexcept;
    MidiClip* takeClip (Track& track, double from, double to);
    void recordNote (int pitch, double start, double end, float velocity);
    void recordControl (juce::uint8 status, double ppq, int value);
    void finishTake();
    void rebuildSequence (Track& track, bool releaseSounding);

    /** A choir track's keys and chords (Choir::Timeline), made again in song samples from the
        song channel and its chord track's notes; true when they changed. */
    bool rebuildChoirTimeline (Track& track);

    /** The choir tracks taking their chords from this track: their chords made again (a frozen
        one whose chords changed plays itself again). */
    void refreshChoirsFollowing (int trackId);

    /** One block of the whole song: tracks, Main, metronome. The device's callback runs it,
        and so does the mixdown thread while the device plays silence. */
    void renderBlock (const float* const* inputs, int numInputs, float* const* outputs, int numOutputs, int numSamples) noexcept;

    class MixdownThread;
    std::unique_ptr<MixdownThread> mixdown;

    /** The device stopping or starting again (a driver's reset) while a mixdown renders: the render
        is cancelled and waited for first, so nothing it uses changes under it. */
    void stopMixdownThread();
    std::atomic<bool> offline { false }, deviceInCallback { false };
    std::atomic<juce::uint32> deviceCallbacks { 0 };   // the device's blocks, counted (any thread reads it)

    /** Export Stems: each stem's channel added to its own buffer as it leaves its fader. Set by
        the mixdown thread while it renders (the device stands aside then), read by renderBlock. */
    struct StemTaps
    {
        std::vector<int> trackIds;
        juce::AudioBuffer<float> buffers;   // two channels per stem
        std::vector<float> lastGain;
        void take (const Track& track, int numSamples) noexcept;

        /** The Analog Board mixing: the track's strip, as it leaves its fader. */
        void takeBoard (const Track& track, int numSamples) noexcept;
    };

    StemTaps* stemTaps = nullptr;

    /** Freezing: the track's own sound as render() leaves it (before its fader), and the tracks
        that play meanwhile (it and the ones it needs). Set by the mixdown thread, like stemTaps. */
    struct FreezeTap
    {
        int trackId = 0;
        std::vector<int> plays;
        bool dry = false;                    // its track without its inserts
        juce::AudioBuffer<float> buffer;
        bool takes (int id) const noexcept     { return std::find (plays.begin(), plays.end(), id) != plays.end(); }
        void take (const Track& track, int numSamples) noexcept;
    };

    FreezeTap* freezeTap = nullptr;
    std::vector<int> thawed;                 // tracks a change thawed, for the page

    /** The freeze rendering now: what it was rendered from, so its render can be checked. */
    struct FreezeStart
    {
        int trackId = 0;
        TempoMap map;
        bool dry = false;                                 // the Analog Board mixing
        std::vector<std::pair<int, int>> revisions;       // the track and those it needs: id, content revision
        std::vector<Track::KeyLevel> keys;                // the tracks keying it, as they sounded
    };

    FreezeStart freezeStart;
    std::set<juce::String> sessionRenders;   // renders made since the song opened (paths)
    std::set<juce::String> savedRenders;     // ...that a saved song names
    std::set<juce::String> loadedRenders;    // renders in memory

    /** The track and the tracks its render needs: those it takes notes from, those keying its
        plugins' sidechains, the tracks playing into such a bus or sending to such an FX channel,
        and theirs in turn. */
    std::vector<int> freezeNeeds (int trackId) const;

    /** Its render at the playback rate, as a playlist (null when it can't be read). */
    std::unique_ptr<const AudioPlaylist> frozenPlaylist (const juce::File& file);
    void thaw (Track& track, bool tellPage);
    void contentChanged (Track& track);
    void thawDependents (int trackId);   // frozen tracks whose renders this track plays a part in

    /** The tracks sending into a sidechain the track's render hears, with their faders, pans and
        mutes (none with the Analog Board mixing: its sends rest). */
    std::vector<Track::KeyLevel> keyLevels (int trackId, bool dry) const;

    /** A track's volume or pan envelope changed: a frozen track it keys thaws. */
    void faderEnvelopeChanged (Track& key);

    /** A mixdown's first block starts each fader (and send, and Main's fader) where it is, not
        ramping there from the last block the device played: the song may have been elsewhere,
        or soloed, until then. */
    std::atomic<bool> settleGains { false };

    struct SavedTransport
    {
        bool playing = false, loop = false, metronome = false;
        double ppq = 0.0, loopStart = 0.0, loopEnd = 16.0;
        float metronomeVolume = 0.6f;
    };

    SavedTransport beforeMixdown;
    void rebuildAutomation (Track& track);
    /** Audio thread. ppq: where the song is (plugins' parameters); heardPpq: where the track's
        sound is heard, after plugin delay compensation (its fader and pan). */
    static void applyAutomation (Track& track, double ppq, double heardPpq) noexcept;
    void resolveInput (Track& track) noexcept;
    void beginAudioRecording();
    void finishAudioRecording();
    void abandonAudioRecording();
    bool followTempo();
    void mixPreview (float* left, float* right, int numSamples) noexcept;   // audio thread, under the lock

    /** One block of the Live Show into left and right (Main), under the graph lock: its channels'
        tracks, then its decks, ducker and master. */
    void renderLive (const float* const* deviceInputs, int numInputs, const clap_event_transport_t& position,
                     int numSamples, int mixSamples, int deviceSlots, float* left, float* right,
                     float* phonesLeft, float* phonesRight) noexcept;
    void resolveVoiceInput() noexcept;
    void captureVoice (const float* const* inputs, int numInputs, int numSamples) noexcept;   // audio thread
    void mixCue (float* left, float* right, int numSamples) noexcept;                         // audio thread
    void pushPass (juce::int64 fileSample, double ppq) noexcept;   // audio thread
    static MidiClip* findClip (Track& track, int clipId) noexcept;
    // Bars and time through the song channel's tempo map (message thread).
    double barStartAt (double ppq) const noexcept;
    void fitAudioLength (MidiClip& clip) const noexcept;
    double barEndAt (double ppq) const noexcept;
    void setNoteBit (int note, bool isOn) noexcept;
    EffectChain* chainFor (int channel) noexcept;
    Plugin* findPlugin (int channel, int insertId) noexcept;
    bool activatePlugin (Plugin& plugin);

    static juce::File getStateFile();

    //==============================================================================
    juce::AudioDeviceManager deviceManager;
    Transport transport;
    juce::MidiMessageCollector midiCollector;
    juce::MidiBuffer midiBuffer, noMidi;
    juce::StringArray enabledMidiInputNames;
    juce::StringArray midiSources;                 // what tracks can choose (message thread)
    std::vector<MidiDevice> midiDevices;           // message thread
    juce::String preferredMidiSource;              // message thread: the page's first choice
    const MidiDevice* findMidiDevice (const juce::String& name) const noexcept;
    void refreshMidiSources();
    juce::String defaultMidiSource() const;

    // Sync: the device's callback plans it (never the mixdown thread).
    MidiSync midiSync;
    double blockStartTime = 0.0;                   // audio thread: when this device block began
    bool syncThisBlock = false;                    // audio thread: the device is playing the song now
    std::atomic<double> syncDelayMs { 0.0 };       // until a block is heard: the output latency
    bool deviceReleased = false;                   // message thread

    std::atomic<float> peakLeft { 0.0f };
    std::atomic<float> peakRight { 0.0f };
    std::atomic<juce::uint64> activeNotesLow { 0 };
    std::atomic<juce::uint64> activeNotesHigh { 0 };
    std::atomic<int> hardwareNoteOnCount { 0 };

    std::atomic<float> masterVolumeDb { 0.0f };
    std::atomic<bool> masterMute { false };
    float lastMasterGain = 1.0f;       // audio thread

    // The Analog Board: the master section, what the message thread asks for, and what the audio
    // thread plays (a switch fades the old mix out over one block, the new one in over the next).
    AnalogBoard::Master board;
    std::atomic<bool> boardWanted { false };
    bool boardActive = false, boardFadingOut = false;   // audio thread

    // The Live Show: its mixer, what the message thread asks for, and what the audio thread plays
    // (switching fades the old out over a block and the new in over the next, as the board's
    // does); the decks' tracks and the pads' sounds, kept alive here (message thread); the files a
    // song gave them.
    LiveShow::Mixer liveShow;
    std::atomic<bool> liveWanted { false };
    bool liveActive = false, liveFadingOut = false;     // audio thread

    // The Studio heard with the show (its tab out of sight): asked for, whether it was heard the
    // last block and its level then (audio thread: in from silence, out to it), and the show's
    // own sound then, apart (Main's two channels, then its headphones' two), added on top.
    std::atomic<bool> studioWithShow { false };
    bool studioHeardWithShow = false;
    float studioWithShowGain = 0.0f;
    bool liveSwitchKeepsStudio = false;                // the show's switch under way keeps the Studio heard
    juce::AudioBuffer<float> showMix;
    std::array<std::shared_ptr<const LiveShow::DeckTrack>, LiveShow::maxDecks> deckTracks;
    std::array<juce::File, LiveShow::maxDecks> deckFilesToLoad;
    bool deckFilesWaiting = false;
    std::array<std::shared_ptr<const LiveShow::DeckTrack>, LiveShow::numPads> padSounds;
    std::array<juce::File, LiveShow::numPads> padFilesToLoad;
    bool padFilesWaiting = false;
    juce::var liveExtras;                              // message thread
    juce::var mainKept;                                // message thread
    bool liveExtrasWaiting = false;
    int livePhonesOutput = 0;                          // message thread

    // The night's recording, message thread (its writers are the show's, under the graph lock).
    struct LiveTake
    {
        juce::File folder, showFile;
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> show;

        struct Stem
        {
            juce::File file;
            std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
            int deck = -1, trackId = 0;                // a deck, or a channel's track
        };

        std::vector<Stem> stems;
    };

    LiveTake liveTake;
    std::unique_ptr<LiveShow::SoundTap> liveTap;       // message thread (the show's under the graph lock)

    // While the board mixes, the Console's effects play silence for this long first (their tails
    // die away, heard nowhere), then rest: audio thread.
    static constexpr double consoleQuietSeconds = 10.0;
    juce::int64 consoleQuietSamples = 0;
    juce::AudioBuffer<float> consoleQuiet;             // Main's inserts' silence

    // Main's volume envelope: the message thread's, and the audio thread's copy (swapped under
    // the graph lock), with the value it last set.
    std::vector<AutomationPoint> mainAutomation;
    SongBlocks songBlocks;
    int songBlocksGeneration = 0;
    SongVideo songVideo;
    int songVideoGeneration = 0;
    std::unique_ptr<const std::vector<AutomationPoint>> mainAutomationPlayback;
    double lastMainAutomationValue = std::numeric_limits<double>::quiet_NaN();
    EffectChain mainInserts;
    juce::AudioBuffer<float> mix;      // the Main channel's stereo mix
    int nextInsertId = 1;

    // Clip effects: the latest of each (by uid) a paste may ask for, and the state of a plugin
    // that went (its clip deleted, moved, its effect replaced), for its next one.
    std::map<int, std::weak_ptr<const ClipEffect>> clipEffectsKnown;
    std::map<int, juce::MemoryBlock> clipEffectStatesLeft;
    std::map<int, std::unique_ptr<Plugin>> clipEffectsLoaded;   // setClipEffect's plugin, for its slot (loaded once)

    // Silence Areas: each file's loudness, a peak every 10 ms (worked out once per file in memory).
    struct SilenceLevels
    {
        std::weak_ptr<const AudioData> data;
        std::vector<float> peaks;
        int window = 1;                   // samples per peak
    };

    std::map<const AudioData*, SilenceLevels> silenceLevels;

    /** The track's clip effects for its clips now: those still there kept (their plugins playing
        on), new ones loaded. Message thread, outside the graph lock. */
    std::vector<std::shared_ptr<Track::ClipEffectSlot>> buildClipEffects (Track& track);
    bool captureClipEffect (Track& track, Track::ClipEffectSlot& slot);
    void updateClipEffects (juce::String& message, PluginChange& change);
    int nextMidiFxId = 1;

    MidiFxChain::Slot* findMidiFx (int trackId, int fxId) const noexcept;
    bool midiFxIdInUse (int fxId) const noexcept;

    // Recording: the audio thread writes, updateRecording() reads.
    struct RecordedEvent
    {
        double ppq = 0.0;
        juce::uint8 status = 0, data1 = 0, data2 = 0;
    };

    static constexpr juce::uint8 punchOutMarker = 0;   // a queued "recording stopped here", not MIDI
    static constexpr int recordQueueSize = 4096;
    juce::AbstractFifo recordQueue { recordQueueSize };
    std::array<RecordedEvent, recordQueueSize> recordedEvents {};
    bool capturing = false;            // audio thread: was recording in the last block
    int lastBendQueued = -1, lastPressureQueued = -1;   // audio thread: the controller values last recorded

    // The take being recorded, message thread.
    struct Take
    {
        bool active = false;
        std::vector<int> trackIds;             // the tracks armed when it began
        std::map<int, int> clipIds;            // track id -> the clip it records into
        std::bitset<128> held;
        std::array<double, 128> heldStart {};
        std::array<float, 128> heldVelocity {};
        double lastPosition = 0.0;
        bool stopped = false;                  // the audio thread has marked where it stopped
        bool controlsFiled = false;            // bend or aftertouch filed since the page last heard
        juce::uint32 controlsToldAt = 0;
    };

    Take take;
    int nextClipId = 1;
    NoteQuantize recordQuantize;                // Record Quantize, the page's
    void quantizeTake (MidiClip& clip) const;

    // Audio: files in memory, the song's inputs, and recording.
    AudioMedia media;
    std::vector<SongInput> inputs = defaultInputs();

    // Outputs: the message thread's list and defaults, and the audio thread's plan of them.
    std::vector<SongOutput> outputs = defaultOutputs();
    int auditionOutput = 0;
    std::vector<SongInput> songDefaultInputs = defaultInputs();
    std::vector<SongOutput> songDefaultOutputs = defaultOutputs();
    int songDefaultAudition = 0;
    juce::StringArray outputChannelNames;      // the device's, kept while it's let go
    juce::String outputsAskedOf;               // the device every output channel was last asked of
    std::pair<int, int> startupOutputPair { -1, -1 };

    struct SubPlan
    {
        int left = -1, right = -1;
        bool mainMix = false;
    };

    struct OutputPlan
    {
        int mainLeft = 0, mainRight = 1;
        int subCount = 0;
        int audition = -1;                     // -1: Main, else a Sub's place
        int phones = -1;                       // the Live Show's headphones: a Sub's place, or -1
        std::array<SubPlan, maxSubOutputs> subs {};
    };

    OutputPlan outputPlan;                     // changed under the graph lock
    std::array<std::atomic<float>, maxSubOutputs> subVolumeDb {};
    std::array<std::atomic<bool>, maxSubOutputs> subMute {};
    std::array<std::atomic<float>, maxSubOutputs> subPeakLeft {}, subPeakRight {};
    std::array<float, maxSubOutputs> subLastGain {};   // audio thread
    juce::AudioBuffer<float> subMix;                   // two channels a Sub output

    // Plugin delay compensation. Tracks meet at T (the slowest track), buses leave at T + B (the
    // slowest bus), FX channels at T + B + F (the slowest FX channel; what tracks send them waits
    // B for what buses send). So what goes straight to Main or a Sub waits: a track's B + F, a
    // bus's F; and the Subs wait for Main's inserts. Gathered here and delayed together.
    static constexpr int directPlaces = maxSubOutputs + 1;                       // Main, then each Sub
    juce::AudioBuffer<float> directMix;                // set 0: tracks, set 1: buses; two channels a place
    std::array<std::array<std::unique_ptr<LatencyDelay>, directPlaces>, 2> directDelays;
    std::array<std::unique_ptr<LatencyDelay>, maxSubOutputs> subDelays;          // the Subs, late by Main's inserts
    std::atomic<int> tracksWaitSamples { 0 }, busesWaitSamples { 0 }, sendsWaitSamples { 0 },
                     subsWaitSamples { 0 }, outputLatencySamples { 0 };
    std::atomic<int> lateTracks { 0 }, lateBuses { 0 }, lateFx { 0 };          // how late each kind's fader is heard
    int latencyTracks = 0, latencyBuses = 0, latencyFx = 0, latencyMain = 0;    // message thread: the last worked out
    int recordCompensation = 0;                        // message thread: the take's, as it started
    juce::uint64 blockCount = 0;                       // audio thread: the delays know a block they missed
    bool showAloneBlock = false;                       // audio thread: the Live Show alone this block (nothing delayed)
    int sendsWaitBlock = 0;                            // audio thread: this block's wait for what tracks send FX channels
    int tracksWaitBlock = 0, busesWaitBlock = 0;       // audio thread: this block's waits for what goes to Main
    juce::AudioBuffer<float> keyScratch;               // audio thread: a sidechain key on its way through its delay

    /** The delay at a place, made (and prepared) when first needed. Message thread. */
    void makeDelay (std::unique_ptr<LatencyDelay>& delay);
    int nextOutputId = 1;                              // message thread: the song's next new output id

    // Audio thread: where things go in the callback's outputs, which are the device's active
    // output channels in order (outputIndexOfChannel: device channel -> that place, -1 if closed).
    std::vector<int> outputIndexOfChannel;             // set while no callback runs
    int cueLeftChannel = 0, cueRightChannel = 1;       // the Audition output's places
    int mainLeftIndex = 0, mainRightIndex = 1;         // Main's places

    // Settings' Test: a short tone on Main, audio thread.
    std::atomic<bool> testToneRequested { false };
    bool testTonePlaying = false;
    juce::int64 testToneSample = 0;
    double testTonePhase = 0.0;
    void mixTestTone (float* left, float* right, int numSamples) noexcept;

    static std::vector<SongInput> tidyInputs (std::vector<SongInput> list);
    static std::vector<SongOutput> tidyOutputs (std::vector<SongOutput> list, int& nextId, std::map<int, int>* newIds = nullptr);
    void planOutputs();
    void resolveOutput (Track& track);
    int subSlotOf (int outputId) const noexcept;
    bool hasOutput (int outputId) const noexcept;
    juce::File recordingFolder;
    int recordBits = 24;
    juce::uint32 followedGeneration = 0;   // the tempo map audio clips were last worked out with

    // The preview: the message thread keeps the data alive, the audio thread plays it.
    std::shared_ptr<const AudioData> previewData;
    const AudioData* previewPlaying = nullptr;   // changed under the graph lock
    juce::int64 previewPosition = 0;             // audio thread, or under the lock
    std::atomic<bool> previewFinished { false };
    juce::int64 previewLoopStart = 0, previewLoopEnd = 0;   // in the data's samples, under the lock (none: end <= start)
    int previewCrossfade = 0;
    bool previewLooped = false;                  // it has come round its loop once
    double previewRatio = 1.0;                   // the data's samples per file sample
    std::atomic<juce::int64> previewShown { -1 };
    juce::String previewPath;
    double mediaRate = 0.0;            // the sample rate the media in memory is at

    juce::TimeSliceThread writerThread { "Fanan audio recorder" };

    struct AudioRecording
    {
        int trackId = 0;
        juce::File file;
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    };

    std::vector<AudioRecording> audioRecordings;   // message thread
    bool audioTakeActive = false;

    // Where each pass of a take starts in its files (a new pass each time the loop goes round).
    struct Pass
    {
        juce::int64 fileSample = 0;
        double ppq = 0.0;
    };

    static constexpr int passQueueSize = 256;
    juce::AbstractFifo passQueue { passQueueSize };
    std::array<Pass, passQueueSize> queuedPasses {};
    std::vector<Pass> takePasses;      // message thread
    juce::int64 recordedSamples = 0;   // audio thread: written so far this take
    bool recordingAudio = false;       // audio thread: whether the last block recorded
    double recordedUpTo = 0.0;         // audio thread: where the last recorded stretch of song ended

    juce::SpinLock graphLock;
    std::vector<std::unique_ptr<Track>> tracks;

    // MIDI devices, each with a queue of its own so tracks can choose them.
    static constexpr int maxMidiDevices = 32;
    std::array<std::unique_ptr<juce::MidiMessageCollector>, maxMidiDevices> deviceCollectors;
    std::array<juce::MidiBuffer, maxMidiDevices> deviceBuffers;   // audio thread: this block's
    juce::StringArray deviceSlotIds, deviceSlotNames;              // under deviceLock
    juce::SpinLock deviceLock;
    std::atomic<int> deviceSlotCount { 0 };
    juce::MidiBuffer recordMidi;                                   // audio thread: what the armed track hears

    // Control surfaces' ports: their messages are queued for the surfaces (MIDI thread in,
    // message thread out), never for tracks.
    static constexpr size_t maxSurfaceMessages = 4096;
    juce::StringArray surfacePorts;                                // message thread
    std::array<bool, maxMidiDevices> surfaceSlots {};              // under deviceLock
    juce::CriticalSection surfaceQueueLock;
    std::vector<std::pair<int, juce::MidiMessage>> surfaceQueue;   // under surfaceQueueLock: slot, message
    void markSurfaceSlots();                                       // under deviceLock

    // MIDI Link: the knobs, sliders, pads and wheels it sees (MIDI thread in, message thread
    // out), and the linked ones, kept from the tracks.
    static constexpr size_t maxLinkMessages = 4096;
    juce::CriticalSection linkQueueLock;
    std::vector<std::pair<int, juce::MidiMessage>> linkQueue;      // under linkQueueLock: slot, message
    std::vector<LinkSource> linkedSources;                         // message thread
    std::vector<juce::uint32> linkedKeys;                          // under deviceLock: sorted (linkKeyOf)
    void markLinkedKeys();                                         // under deviceLock

    /** A message's place for MIDI Link: its port's slot, kind, channel and number; 0 when it's
        none of a controller, a note or pitch bend. */
    static juce::uint32 linkKeyOf (int slot, int kind, int channel, int number) noexcept;
    static juce::uint32 linkKeyOf (int slot, const juce::MidiMessage& message) noexcept;

    // The page's own notes (the piano roll's), kept apart: they go to the selected track.
    juce::MidiMessageCollector pageCollector;
    juce::MidiBuffer pageBuffer;

    // The Plugins Creator Studio's preview (changed under the graph lock), and the live notes it
    // takes while it listens (audio thread).
    CreatorPreview* creatorPreview = nullptr;
    std::atomic<bool> creatorListening { false };
    juce::MidiBuffer creatorMidi, creatorScratch;

    // MIDI In (setMidiFocus): the studio in front, and each MIDI In's ports and held notes.
    std::atomic<int> midiFocus { 0 };
    std::atomic<bool> rackMidiInOpen { false };

    struct MidiInState
    {
        juce::StringArray inputs { everyMidiInput };     // message thread
        std::atomic<juce::uint32> mask { 0xffffffffu };  // the device slots heard
        std::atomic<bool> every { true };                // and a port with no slot
        std::atomic<juce::uint32> activity { 0 };

        // Audio thread: what was heard last block, and the notes and pedals it holds down.
        juce::uint32 maskHeard = 0xffffffffu;
        bool everyHeard = true, holding = false;
        std::array<std::array<juce::uint64, 2>, 16> held {}, heldBefore {};
        juce::uint16 pedals = 0, pedalsBefore = 0;
        bool holdingBefore = false, letGoPending = false;   // held through blocks it didn't play: let go when it plays
    };

    std::array<MidiInState, numMidiIns> midiIns;
    juce::MidiBuffer rackLive, noLive;                   // audio thread: the rack's MIDI In this block
    void markMidiInSlots();                              // under deviceLock

    /** Audio thread: a MIDI In's notes this block (takes: it listens), and, when it stops
        listening or loses a port, the release of what it held. */
    void gatherMidiIn (MidiInState& in, juce::MidiBuffer& into, bool takes, int deviceSlots, int numSamples) noexcept;

    /** Whether the studio in front takes a message from this slot (-1: a port with no slot, -2:
        the page's keyboards). Any thread. */
    bool frontHears (int slot) const noexcept;

    // The monitor: a ring of the latest messages (the MIDI thread's and the page's in, the
    // message thread's out), slots as frontHears takes them.
    struct RawMonitored
    {
        double time = 0.0;
        int slot = -1;
        juce::uint8 bytes[3] {};
        int size = 0, sysexLength = 0;
        bool heard = false;
    };

    static constexpr int monitorCapacity = 256;
    std::atomic<bool> monitorOn { false };
    juce::SpinLock monitorLock;
    std::array<RawMonitored, monitorCapacity> monitorRing {};
    int monitorStart = 0, monitorCount = 0;              // under monitorLock
    void monitor (int slot, const juce::MidiMessage& message) noexcept;

    // The Sampling Studio's tap and the track it holds (changed under the graph lock), and
    // whether one is set (any thread: the song stays stopped meanwhile).
    SamplingTap* samplingTap = nullptr;
    int samplingTrackId = 0;
    bool samplingOffline = false;
    std::atomic<bool> samplingOn { false };

    /** Turbo, Unleashed: the studio's thread renders (renderSampling), and the device stands aside
        meanwhile as it does for a mixdown (silence, nothing of the song touched), so the transport,
        the tracks and the MIDI FX board have one audio thread at a time. */
    std::atomic<bool> samplingRenders { false };
    juce::MidiBuffer samplingNotes;                 // renderSampling's (under the graph lock)
    RackPlayer* samplingRack = nullptr;             // the studio's rack (changed under the graph lock)
    juce::AudioBuffer<float> rackSound;             // its block (sized as the device starts)
    juce::MidiBuffer rackNotes;                     // the audio thread's notes for it
    void renderRackBlock (const juce::MidiBuffer& notes, const juce::MidiBuffer& live, int numSamples,
                          const clap_event_transport_t& position) noexcept;
    juce::MidiMessageCollector auditionCollector;
    juce::MidiBuffer auditionBuffer;
    std::atomic<int> auditionTrackId { 0 };
    void assignMidiDeviceSlots();
    void resolveMidiInputs (Track& track);

    // Audio thread: this block's order of the tracks (reserved once, so it never allocates).
    std::vector<Track*> renderOrder;
    std::vector<char> placed;
    bool orderTracks() noexcept;
    void clearSidechains (int numSamples) noexcept;
    void sendFrom (Track& track, int numSamples, bool settle) noexcept;
    Track* busOf (const Track& track) const noexcept;               // under the graph lock
    bool soloedThroughBus (const Track& track) const noexcept;      // under the graph lock
    int nextTrackId = 1;
    std::atomic<int> selectedTrackId { 0 };   // the audio thread sends the page's own notes to it

    // Voice control: the audio thread writes the voice input's samples, the listener reads them.
    int voiceInput = -1;                                  // message thread: a song input
    std::atomic<int> voiceLeft { -1 }, voiceRight { -1 }; // its channels in the callback
    std::atomic<bool> voiceListening { false };
    static constexpr int voiceQueueSize = 1 << 17;        // about 2.7 seconds at 48 kHz
    juce::AbstractFifo voiceQueue { voiceQueueSize };
    std::vector<float> voiceSamples = std::vector<float> (static_cast<size_t> (voiceQueueSize));

    // Cues: the one sounding now, audio thread.
    std::atomic<int> pendingCue { 0 };
    std::atomic<bool> cueSounding { false };
    int cuePlaying = 0;                                   // 0: none, else Cue + 1
    juce::int64 cueSample = 0;
    double cuePhase = 0.0;

    std::atomic<bool> audioRunning { false };
    double currentSampleRate = 0.0;
    int currentBlockSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
