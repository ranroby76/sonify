// D:\workspace\Fanan_web_hub\src\Track.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "AnalogBoard.h"
#include "AudioMedia.h"
#include "Choir.h"
#include "LiveShow.h"
#include "Automation.h"
#include "EffectChain.h"
#include "LatencyDelay.h"
#include "MidiFx.h"
#include "Sequence.h"
#include "Transport.h"
#include "Plugin.h"

#include <array>
#include <atomic>
#include <memory>
#include <tuple>
#include <utility>

//==============================================================================
/**
    One track, of one of two kinds:
      instrument  MIDI clips and live notes, through its MIDI FX, into a sound source
                  (the built-in test synth or a WCLAP instrument);
      audio       audio clips and a live input from the audio device, which it can
                  monitor and record. A choir track is an audio track whose clips sing as a
                  choir as well (its Choir).
    Either way, then a chain of insert effects and the fader settings. Frozen, a render of what
    it feeds its mixer (before the fader) plays in its place. While the song is mixed
    on the Analog Board, its dry sound (before the inserts) goes to its strip there instead. While
    the Live Show plays, a track marked as one of its channels plays there (its input or live
    notes through its inserts), and the others rest.

    The engine owns the tracks. Fader settings are atomics the page may change at
    any time. Plugins are swapped only by the engine, while it holds its graph lock,
    so the audio thread never sees a half-installed plugin.
*/
class Track final
{
public:
    // fx: an FX channel (Studio One's): no clips of its own, it plays what tracks send to it.
    /** A bus is a Console channel the tracks chosen play into (instead of Main or a Sub), mixed
        through its own inserts, fader and pan into its output. Buses don't play into buses. */
    enum class Kind { instrument, audio, fx, bus };

    /** An FX channel or a bus: a channel of the Console, with no clips and no notes. */
    bool isMixChannel() const noexcept          { return kind == Kind::fx || kind == Kind::bus; }

    /** A send: a copy of the track's sound (after its fader) to an FX channel, or to a
        plugin's sidechain input (a compressor ducking under the kick, say). */
    struct Send
    {
        int id = 0;
        bool sidechain = false;
        int target = 0;            // an FX channel's track id; for a sidechain, the channel the plugin is on
        int slot = 0;              // sidechain: 0 the channel's instrument, otherwise an insert's id
        float levelDb = 0.0f;
        bool on = true;
        float lastGain = 0.0f;     // audio thread: where the level's ramp left off

        // A sidechain key into Main's or an FX channel's plugin: as late as that sound is
        // (plugin delay compensation). Made by the engine when needed; kept while the send is.
        std::shared_ptr<LatencyDelay> keyDelay;
    };

    Track (int id, juce::uint32 colourArgb, Kind kind = Kind::instrument);
    ~Track();

    //==============================================================================
    const int id;
    const Kind kind;
    juce::String name;             // message thread
    bool nameIsAutomatic = true;   // renamed after its instrument until the user names it
    juce::uint32 colour = 0;       // ARGB, message thread

    /** The Editor's Drums view for this track (its drum map): {names: {"36": "Kick", ...} (rows the
        user named), rows: [pitches added to the view]}. Void: General MIDI's names, the usual kit.
        Message thread; kept with the song (Track::sanitiseDrumMap tidies one that comes in). */
    juce::var drumMap;

    /** A drum map as above, from anything (the page's, a song's): names of 0..127 up to 40
        characters, rows 0..127 each once; void when there's nothing in it. */
    static juce::var sanitiseDrumMap (const juce::var& map);

    /** Devices a DAWproject brought that can't play here (not installed, a format Sonify's tracks
        don't play, another DAW's own): placeholders, kept with the song as they came, which Export
        DAWproject writes back (DawProject.h). Each {name, format, role, place (among the channel's
        devices), xml, stateFile (its settings, a file in the song's folder)}. Void: none.
        Message thread. */
    juce::var kept;

    std::atomic<float> volumeDb { 0.0f };
    std::atomic<float> pan { 0.0f };           // -1 left .. +1 right
    std::atomic<bool> mute { false }, solo { false }, monitor { false }, recordArm { false };
    std::atomic<bool> polarity { false };      // inverted: its sound turned upside down (Ø), before its fader
    bool monitorDecided = false;   // Monitor was set by the user, or once by Sonify (an instrument plugin added): Sonify leaves it alone now
    std::atomic<float> peakLeft { 0.0f }, peakRight { 0.0f };

    static constexpr float silentDb = -72.0f;  // at or below this the fader is closed
    static constexpr float maxDb = 10.0f;      // top of the fader, as on Studio One

    //==============================================================================
    // Plugins. Null instrument: the test synth plays. The inserts follow in order. The MIDI FX
    // (instrument tracks) come before the instrument: what it hears goes through them first.
    std::unique_ptr<Plugin> instrument;
    EffectChain inserts;
    MidiFxChain midiFx;

    /** Its instrument is out for a moment (the engine starts it again outside its graph lock): the
        track plays nothing meanwhile, not the test synth. Set and cleared under that lock. */
    bool instrumentAway = false;

    /** Its instrument switched off (the header on its window, the instrument's ON/OFF): it still
        plays, so nothing it holds is lost or left hanging, but it's heard silent. Going off or on
        it fades over a few milliseconds (instrumentLevel, on the audio thread). A new instrument
        comes in on. */
    std::atomic<bool> instrumentOff { false };
    float instrumentLevel = 1.0f, instrumentFadeStep = 1.0f / 480.0f;

    //==============================================================================
    // Clips. The message thread owns the clips; after a change the engine builds a new
    // Sequence and swaps it in under its graph lock, and the audio thread plays that.
    std::vector<MidiClip> clips;
    std::unique_ptr<const Sequence> sequence;
    Sequencer sequencer;

    //==============================================================================
    // Audio tracks. input is which of the engine's inputs the track hears; inputLeft and
    // inputRight are where that is among the device's active input channels (right -1:
    // mono). A mono input can take more channels (inputAlso, each one up: 0 is none).
    // The channel is stereo or mono (monoMode). Stereo: each of the input's channels on a side
    // of its own (its left on the left, its right on the right; one channel alone on both).
    // Mono: all of them together as one sound, connected to both sides (two same monos), and
    // recorded as one. The playlist is swapped, and the recorder set, under the engine's graph lock.
    static constexpr int maxInputChannels = 8;       // a mono input's channels, at the most
    int input = 0;                                   // message thread
    std::atomic<int> inputLeft { -1 }, inputRight { -1 };
    std::array<std::atomic<int>, maxInputChannels - 1> inputAlso {};
    std::atomic<bool> monoMode { false };

    /** A mono input on more than one channel. */
    bool hearsSeveralInputs() const noexcept
    {
        for (const auto& also : inputAlso)
            if (also.load (std::memory_order_relaxed) > 0)
                return true;

        return false;
    }

    /** Its recordings' channels: two on a stereo channel hearing two or more, else one. */
    int recordingChannels() const noexcept
    {
        return ! monoMode.load() && (inputRight.load() >= 0 || hearsSeveralInputs()) ? 2 : 1;
    }

    std::atomic<float> inputPeakLeft { 0.0f }, inputPeakRight { 0.0f };   // its input as it hears it, each side
    std::unique_ptr<const AudioPlaylist> playlist;
    juce::AudioFormatWriter::ThreadedWriter* recorder = nullptr;

    /** A take's waveform as it's recorded, for the page to draw it growing: each bucket of
        peakBucket file samples its loudest and quietest sample (both channels together, -127..127,
        as AudioData::peaks are). The audio thread adds buckets while it records; the message
        thread reads the first `buckets` of them. Its room is made before the take starts. */
    struct LivePeaks
    {
        static constexpr int peakBucket = 512;
        std::vector<juce::int8> data;          // two a bucket, made to its full size before the take
        std::atomic<int> buckets { 0 };        // written so far
        float high = 0.0f, low = 0.0f;
        int inBucket = 0;

        void take (const float* left, const float* right, int numSamples) noexcept
        {
            for (int i = 0; i < numSamples; ++i)
            {
                high = juce::jmax (high, left[i], right[i]);
                low = juce::jmin (low, left[i], right[i]);

                if (++inBucket < peakBucket)
                    continue;

                const auto at = buckets.load (std::memory_order_relaxed);

                if (2 * static_cast<size_t> (at) + 1 < data.size())
                {
                    data[2 * static_cast<size_t> (at)] = static_cast<juce::int8> (juce::jlimit (-127, 127, juce::roundToInt (high * 127.0f)));
                    data[2 * static_cast<size_t> (at) + 1] = static_cast<juce::int8> (juce::jlimit (-127, 127, juce::roundToInt (low * 127.0f)));
                    buckets.store (at + 1, std::memory_order_release);
                }

                high = low = 0.0f;
                inBucket = 0;
            }
        }
    };

    LivePeaks* livePeaks = nullptr;            // set with recorder, under the engine's graph lock

    /** An audio clip's own effect (MidiClip::effect), live: its plugin plays that clip's sound
        alone, all the time (so its tail rings on past the clip's end), and adds it to the track
        before the inserts. The engine builds the list from the clips (keeping the plugins of
        effects still there) and swaps it in with the playlist, under its graph lock. */
    struct ClipEffectSlot
    {
        int clipId = 0, uid = 0;
        int slotId = 0;                              // the engine's id for its plugin (its window, its values): unique, as an insert's
        std::unique_ptr<Plugin> plugin;              // null: it couldn't be loaded (error says why), and the clip plays as it is
        juce::String error;
        std::atomic<bool> away { false };            // taken out for a moment (a restart): the clip plays as it is
        juce::AudioBuffer<float> sound;              // audio thread: the clip's sound, through the plugin
        std::unique_ptr<LatencyDelay> delay;         // as late as the slowest clip effect (made when needed)
    };

    std::vector<std::shared_ptr<ClipEffectSlot>> clipEffects;
    ClipEffectSlot* findClipEffect (int slotId) const noexcept;
    int getBlockCapacity() const noexcept                { return buffer.getNumSamples(); }   // the longest block it renders

    /** A choir track: an audio track whose clips (its vocal) sing as a choir too (Choir.h). Set
        once, by the engine under its graph lock; its input, while monitored, is heard alone. */
    std::unique_ptr<Choir> choir;
    bool isChoir() const noexcept                    { return choir != nullptr; }

    //==============================================================================
    // Frozen (instrument and audio tracks): its own sound as it feeds its mixer, rendered over
    // the song into frozenFile, plays in place of its clips, instrument, MIDI FX and inserts,
    // which rest (the Live Show still hears its input and notes live). On the Console that's
    // its sound through its inserts; with the Analog Board mixing, its dry sound (frozenDry).
    // frozen is swapped in under the engine's graph lock; the rest is the message thread's.
    std::unique_ptr<const AudioPlaylist> frozen;
    juce::File frozenFile;                           // none: it isn't frozen
    bool frozenDry = false;                          // rendered for the Analog Board (before its inserts)
    TempoMap frozenMap;                              // the song's tempos, signatures and keys it was rendered at
    int contentRevision = 0;                         // counts the changes to what it plays

    /** A track keying a sidechain its render heard, as it sounded then: a send comes after the
        fader, pan and mute (one its automation drives as the song plays: that lane, not the knob). */
    struct KeyLevel
    {
        int id = 0;
        bool mute = false, volumeAutomated = false, panAutomated = false;
        float volumeDb = 0.0f, pan = 0.0f;

        bool operator== (const KeyLevel& other) const noexcept
        {
            return id == other.id && mute == other.mute
                && volumeAutomated == other.volumeAutomated && panAutomated == other.panAutomated
                && (volumeAutomated || juce::exactlyEqual (volumeDb, other.volumeDb))
                && (panAutomated || juce::exactlyEqual (pan, other.pan));
        }
    };

    std::vector<KeyLevel> frozenKeys;                // the tracks keying its render, as they were

    bool isFrozen() const noexcept                   { return frozenFile != juce::File(); }

    //==============================================================================
    // Where it plays (any kind of track): Main, or a Sub output (Song Setup > Audio I/O Setup >
    // Outputs), by the song output's id; 0 is Main. outputSlot is the audio thread's: the Sub's
    // place among the engine's Sub outputs, -1 for Main.
    int output = 0;                                  // message thread
    std::atomic<int> outputSlot { -1 };

    // A bus it plays into instead (its track id; 0: none), for tracks only. While that bus isn't
    // in the song (removed, until an undo brings it back), the track plays into its output.
    std::atomic<int> outputBus { 0 };

    //==============================================================================
    // Automation. The envelopes are the message thread's; the playback copy is swapped in
    // under the engine's graph lock and applied by the audio thread at each block.
    std::vector<AutomationLane> automation;

    // Sends: the message thread's list, and the audio thread's copy (swapped under the graph lock).
    std::vector<Send> sends;
    std::vector<Send> audioSends;
    int nextSendId = 1;

    /** What an FX channel or a bus plays: tracks' sends, or the tracks playing into the bus,
        add into it each block. */
    juce::AudioBuffer<float> busInput;

    // MIDI in: what the track's instrument hears. External devices by name ("Keys"), one
    // channel of a split device ("Keys|3"), MIDI ports no device uses by their own name, and
    // other tracks' plugins that play notes themselves ("track:<id>"). The message thread's
    // list, and the audio thread's resolved copies (swapped under the graph lock).
    juce::StringArray midiInputs;

    /** A MIDI port the track hears, with what of it gets through. */
    struct MidiRoute
    {
        int slot = 0;                    // the port's queue in the engine
        juce::uint16 channels = 0xffff;  // the MIDI channels let through, one bit each (bit 0: channel 1)
        juce::uint8 blocked = 0;         // the kinds of message kept out: Block flags
    };

    enum Block : juce::uint8 { blockAftertouch = 1, blockProgramChange = 2, blockPitchBend = 4, blockControllers = 8 };

    std::vector<MidiRoute> midiRoutes;
    std::vector<int> midiSourceTracks;
    juce::MidiBuffer liveMidi;       // audio thread: this block's notes in

    // MPE: while the track hears an MPE device, its instrument is told so (the MPE configuration
    // and the pitch bend range) in the next block, and again whenever the instrument changes.
    std::atomic<int> mpeRange { 0 };                 // message thread sets it: 0, no MPE device
    std::atomic<bool> mpeResend { false };           // the range changed: tell the instrument again
    const void* mpeToldInstrument = nullptr;         // audio thread: the instrument last told

    /** Its strip on the Analog Board (AnalogBoard.h): instrument and audio tracks have one. */
    AnalogBoard::Strip board;
    bool boardInputUnheard = false;  // audio thread: this block, its input isn't heard on its strip (the Sampling Studio holds it, not monitored)

    /** Its channel on the Live Show's mixer (LiveShow.h), and whether it's one of the show's
        channels (a singer's mic, a player's instrument): instrument and audio tracks. */
    LiveShow::Channel live;
    std::atomic<bool> liveChannel { false };

    int height = 0;                  // its height in the arrangement; 0 follows the view's size
    std::atomic<int> automationMode { static_cast<int> (AutomationMode::read) };

    /** Controls taken hold of while automation is recorded (Touch, Latch, Write): their envelopes
        don't move them meanwhile. Set on the message thread, read by the audio thread (Automation::keyOf; 0: free). */
    static constexpr int maxHeldTargets = 32;
    std::array<std::atomic<juce::uint64>, maxHeldTargets> heldTargets {};

    bool isHeld (const AutomationTarget& target) const noexcept
    {
        const auto key = Automation::keyOf (target);

        for (const auto& held : heldTargets)
            if (held.load (std::memory_order_relaxed) == key)
                return true;

        return false;
    }

    /** Holds a target (false when there's no room), or lets it go. Message thread. */
    bool setHeld (const AutomationTarget& target, bool held) noexcept
    {
        const auto key = Automation::keyOf (target);

        for (auto& slot : heldTargets)
            if (slot.load() == key)
            {
                if (! held)
                    slot.store (0);

                return true;
            }

        if (! held)
            return true;

        for (auto& slot : heldTargets)
            if (slot.load() == 0)
            {
                slot.store (key);
                return true;
            }

        return false;
    }
    std::unique_ptr<AutomationPlayback> automationPlayback;

    /** Held notes (and a held sustain pedal) are released in the next block it plays (used when
        the track stops listening, and around a render). */
    void requestNotesOff() noexcept { notesOffRequested.store (true); }

    //==============================================================================
    // Audio side. prepare() runs while no audio callback is active.
    void prepare (double sampleRate, int maxBlockSize);

    /** Plays the clips and the live notes (instrument) or the clips and the live input
        (audio) through the source and the inserts, into the track's own buffer. live is
        empty unless the track is monitoring or armed. inputs are the device's active
        input channels; recording says whether an audio track's recorder takes this block.
        dry: the inserts are left out (the Analog Board mixes the song). hearInput: an audio
        track hears its input even when it isn't monitoring (the Live Show plays it). A frozen
        track plays its render instead, unless hearInput (the show hears it live). */
    void render (const juce::MidiBuffer& liveNotes, const float* const* inputs, int numInputs, bool recording,
                 const Transport& transport, const clap_event_transport_t& position, int numSamples, bool dry = false,
                 bool hearInput = false) noexcept;

    /** The block render() made, and how long it is. */
    const float* getBlock (int channel) const noexcept   { return buffer.getReadPointer (juce::jlimit (0, 1, channel)); }
    float* getWritableBlock (int channel) noexcept       { return buffer.getWritePointer (juce::jlimit (0, 1, channel)); }

    //==============================================================================
    // Plugin delay compensation. The engine works out (message thread) how late each track's
    // sound must be so everything meets in time, and the audio thread delays its block by that.
    /** How late its own instrument and inserts make its sound (frozen: none, its render is in
        time). dry: without its inserts (the Analog Board mixing). Message thread. */
    int ownLatency (bool dry) const noexcept;

    std::atomic<int> compensation { 0 };            // samples its block is delayed
    std::unique_ptr<LatencyDelay> latencyDelay;     // made when it first needs one

    /** Audio thread: its block as it was before the delay, for sends that key another track's
        sidechain (that track's plugin hears its own sound before any delay). */
    void keepKeyBlock (int numSamples) noexcept;
    void dropKeyBlock() noexcept                    { keyHeld = false; }

    // An FX channel: what the tracks send it waits for what the buses send (they come later).
    juce::AudioBuffer<float> trackSends;            // sized as it's prepared
    std::unique_ptr<LatencyDelay> sendsDelay;       // made when it first needs one

    // An audio track's clip effects: its clips as late as the slowest of them (the plain clips
    // delayed too), so they stay in time with each other. Part of its own latency.
    /** The slowest clip effect's latency. Message thread. */
    int clipEffectsLatency() const noexcept;
    std::atomic<int> clipLatency { 0 };             // the engine's, as it last worked it out
    std::unique_ptr<LatencyDelay> clipsDelay;       // the plain clips (made when needed)

    /** What the clip delays hold, forgotten (the audio thread not rendering it: a render ended). */
    void clearClipDelays() noexcept;
    int getBlockSamples() const noexcept                 { return renderedSamples; }

    /** Its meters show this block (the Analog Board's strip mixed it). */
    void showLevels (float left, float right) noexcept;

    /** Its inserts play a block of silence (the Analog Board mixing: their tails die away, heard
        nowhere). After render(), whose block it takes. */
    void quietInserts (const clap_event_transport_t& position) noexcept;

    /** Adds the rendered block to the output with the fader settings applied. right may be null. */
    void mixInto (float* left, float* right, int numSamples, bool audible) noexcept;

    /** Adds the block as mixInto() just did (after the fader and pan) to a send's destination,
        at the send's level; lastGain carries the level's ramp between blocks. */
    void sendInto (float* left, float* right, int numSamples, float gain, float& lastGain,
                   bool beforeCompensation = false) const noexcept;

    /** The next block starts at the fader's own setting instead of ramping there from the last
        block's (a mixdown's first block: the song may have been soloed or elsewhere until then). */
    void settleGain (bool audible) noexcept;

private:
    /** Adds a playlist's audio (its clips, or its frozen render) at the song's place. Each item
        ramps in and out over a few samples, so it never clicks; a render made from the song's
        start, as the track played there, comes in as it is (fadeStart false). */
    /** A clip's automation of its own effect's parameters, where the block starts (audio thread). */
    static void automateClipEffect (const AudioPlaylist* list, const Transport& songTransport, int effectSlot, Plugin& plugin) noexcept;

    static void playClips (const AudioPlaylist* list, const Transport& transport, float* left, float* right, bool fadeStart = true,
                           int effectSlot = 0) noexcept;   // only the clips going to this clip effect (0: those with none)

    /** The fader's gain for each side, after the pan (the pan only turns a side down). */
    std::pair<float, float> faderGains (bool audible) const noexcept;

    juce::Synthesiser synth;
    juce::AudioBuffer<float> buffer;
    juce::uint64 clipClock = 0;          // audio thread: its renders, counted (the clip delays' blocks)
    juce::AudioBuffer<float> keyBlock;   // the block before plugin delay compensation (keepKeyBlock)
    bool keyHeld = false;
    juce::AudioBuffer<float> inputSum;   // its input's channels added up: one sound (mono), or each side's (stereo from more)
    juce::MidiBuffer notesOffMidi, releaseMidi, blockMidi;
    juce::MidiBuffer noNotes;            // what a clip effect hears: nothing
    std::atomic<bool> notesOffRequested { false };
    float lastGainLeft = 0.0f, lastGainRight = 0.0f;
    float rampFromLeft = 0.0f, rampFromRight = 0.0f;   // this block's fader ramp, for the sends
    int renderedSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Track)
};
