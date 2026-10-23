// D:\workspace\Fanan_web_hub\src\SamplingStudio.h

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>

#include "AudioEngine.h"
#include "SfzFile.h"

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <vector>

//==============================================================================
/**
    The Sampling Studio's work (MainSampling.cpp has its page): Sampler Arena's Sampling Module,
    in Sonify. Its three modes:

      VSTi Sampling   the keys picked on the keyboard, each at its velocity layers (and BPM
                      layers), played one at a time and recorded until the sound dies away;
      Chord Sampling  the chord types picked, on every root of a key range whose notes all fit
                      in it, played together;
      Sample Builder  no recording: the audio files (or the song's clips) put in each key's slots,
                      each with its velocity range, made into an instrument.

    What's sampled: the studio's nodes (SamplingRack.h: the VSTis the Sampling node's MIDI is
    wired to, through their effects, into the Output node: Sampler Arena's node system), a synth
    outside the computer (notes out of a MIDI port, its sound on an audio track's input), or the
    user playing (an audio track's input: the note to play shown, each recorded from where it
    starts until it dies away).

    As Sampler Arena: a layer is recorded at its top less the velocity offset (a single full layer
    at the sampling velocity); a note is held for its duration (milliseconds, or bars at the BPM
    layer's tempo or the song's) and recorded until its level stays under the silence threshold
    for the silence duration; the tail fades out over the fade time; each sample is written as a
    WAV of the bit depth, rate and channels chosen, its key in its smpl chunk; with Create Legato
    Loop each gets a loop (LoopFinder.h: a .loop file beside it, and the loop in its smpl chunk);
    with Create SFZ (not for chords) the instrument's SFZ is written, its samples in a samples
    folder beside it (chords go there too). Without, the samples go straight into the folder.
    Files of the same name are written over.

    Sonify's own besides: round robins (2 to 4 takes of each, played in turn), the silence before
    the attack trimmed, everything turned up together (normalize), stop and resume, and the
    instrument made from what's recorded at any time. Sampler Arena's slips are left out: a 32-bit
    sample is 32-bit float, a rate other than the device's is converted to (not relabelled),
    names never collide (BPM layers, a custom name with several layers).

    Turbo (about 5 times as fast as the music) and Unleashed (as fast as it goes) render the
    nodes away from the audio device; a synth or the user plays in real time.

    Threads: the session is the message thread's; the engine calls its tap (the notes, the sound)
    on the audio thread, or Turbo's thread; a thread of its own keeps the takes, then makes the
    instrument; and, for a synth, a thread of its own sends the notes out.
*/
namespace Sampling
{
    enum class Mode { vsti, chord, builder };
    enum class Source { nodes, hardware, voice };
    enum class Speed { realTime, turbo, unleashed };
    enum class DurationMode { time, bars };

    struct VelocityLayer
    {
        int low = 1, high = 127;
    };

    /** A key's own settings (its E button's): each used only when set. */
    struct KeyOverride
    {
        bool customVelocity = false, customDuration = false, customKeyShift = false;
        std::vector<VelocityLayer> layers;
        float durationMs = 2000.0f, durationBars = 1.0f;
        DurationMode durationMode = DurationMode::time;
        int keyShift = 0;
    };

    /** A Sample Builder slot: a file (or a song's clip: a window of a file, with its gain and
        fades) for a key, with its velocity range. */
    struct BuilderSlot
    {
        juce::File file;
        juce::String name;                      // its output name (the file's, without its extension)
        int lovel = 0, hivel = 127;
        bool clip = false;
        double offset = 0.0, length = 0.0, gainDb = 0.0, fadeIn = 0.0, fadeOut = 0.0;
    };

    /** Chord Sampling's chord types, Sampler Arena's 18. */
    struct ChordType
    {
        const char* name;
        const char* suffix;
        std::vector<int> intervals;
    };

    const std::vector<ChordType>& chordTypes();

    /** Everything a session (or a build) needs: the page's choices, kept with Sonify's settings
        and in presets (Sampler Arena's .smppreset). */
    struct Settings
    {
        Mode mode = Mode::vsti;
        Source source = Source::nodes;
        int trackId = 0;                        // the audio track hearing the synth, the mic
        juce::String midiOutput;                // hardware: the MIDI output's identifier
        int midiChannel = 1;                    // hardware: 1..16

        juce::String family { "Sample" };      // the samples' names start with it
        std::set<int> keys;                     // the keys picked (VSTi, Builder)
        std::map<int, int> boundaries;          // the yellow locators: 0 the first zone's low key, -1 the last's high key, n zone n's low key
        bool noKeyRanges = false;               // each sample plays its own key only
        bool fillGap = false;                   // kept from presets (no control): the end zones to 0 and 127

        std::vector<VelocityLayer> layers { VelocityLayer {} };
        int samplingVelocity = 127;             // a single full layer is played at this
        int velocityOffset = 3;                 // a layer is played at its top less this
        int keyShift = 0;                       // where the samples sit in the SFZ, in semitones

        DurationMode durationMode = DurationMode::time;
        float durationMs = 2000.0f, durationBars = 1.0f;
        int timeSigNumerator = 4, timeSigDenominator = 4;
        std::vector<int> bpmLayers;             // each note at each tempo (empty: once, at the song's)

        int minChordKey = 48, maxChordKey = 72;
        std::set<int> chords;                   // the chord types picked (chordTypes() indices)

        std::map<int, KeyOverride> overrides;
        std::map<int, juce::String> customNames;
        std::map<int, std::vector<BuilderSlot>> slots;

        float silenceDb = -60.0f;
        float silenceMs = 500.0f;
        bool fadeOut = true;
        int fadeMs = 200;
        bool createSfz = false;
        bool autoLoop = false;                  // Create Legato Loop

        int bitDepth = 24;                      // 16, 24 or 32 (float)
        int sampleRateChoice = 0;               // 0: the audio interface's
        int channelMode = 0;                    // 0 stereo, 1 mono (left), 2 mono (left and right mixed)
        Speed speed = Speed::realTime;

        // Sonify's own.
        int roundRobins = 0;                    // takes of each, played in turn: 0 (one), 2..4
        bool trim = false;                      // the silence before each attack trimmed
        bool normalize = false;                 // the loudest sample at -1 dB, all turned up together
        float triggerDb = -35.0f;               // the user playing: a note starts above this

        // Sonify's, at the start.
        double sampleRate = 48000.0;            // the device's: what's recorded
        double songTempo = 120.0;               // bars, without BPM layers

        static constexpr double maxTailSeconds = 30.0;

        static Settings fromVar (const juce::var& value);
        juce::var toVar() const;

        /** A preset: Sampler Arena's module state ("AutoSamplerState"), Sonify's own besides
            (Sampler Arena ignores them). fromPreset keeps base's track, synth and rate. */
        juce::ValueTree toPreset() const;
        static Settings fromPreset (const juce::ValueTree& tree, const Settings& base);

        int keyShiftFor (int note) const;
        const std::vector<VelocityLayer>& layersFor (int note) const;
        double holdSecondsFor (int note, double bpm) const;
        double outputRate() const;              // the WAVs' rate
    };

    /** The zones a key's sample plays: Sampler Arena's (from 3 below each key, the yellow
        locators applied), or each key alone without key ranges. */
    struct KeyZone
    {
        int root = 60, low = 60, high = 60;
    };

    std::vector<KeyZone> zonesFor (const Settings& settings);

    /** One sample of the plan. */
    struct Shot
    {
        int note = 60;                          // its key (a chord's root)
        std::vector<int> notes;                 // what's played (a chord's notes)
        int velocity = 127, layer = 0;
        int take = 0;                           // round robins: 1.. (0: the one take)
        int bpm = 0;                            // its BPM layer (0: none)
        double holdSeconds = 2.0;
        juce::String chordSuffix;               // "Maj"... (empty: one note)
        int prefix = -1;                        // a chord's number in its type
        juce::String file;                      // its WAV's name
    };

    /** Every sample to record, in order: key by key (chord type by chord type, root by root),
        each layer soft to loud, each BPM layer, each take. Names never collide. */
    std::vector<Shot> plan (const Settings& settings);

    /** Where a mode's samples and its SFZ go, under the folder: <folder>\<family>\samples with an
        SFZ (or chords), else the folder itself. */
    juce::File samplesFolderFor (const Settings& settings, const juce::File& folder);
    juce::File sfzFileFor (const Settings& settings, const juce::File& folder);

    //==============================================================================
    class Session final : public AudioEngine::SamplingTap
    {
    public:
        /** folder: where the samples go (the Ad Hoc folder, or the module's). */
        Session (AudioEngine& engine, const Settings& settings, const juce::File& folder);
        ~Session() override;

        const Settings& getSettings() const noexcept           { return settings; }
        const std::vector<Shot>& getShots() const noexcept     { return shots; }
        const juce::File& getFolder() const noexcept           { return folder; }
        juce::File getSfzFile() const;                         // none without one
        bool isOffline() const noexcept                        { return offline; }

        /** Begins: the takes' folder, the MIDI output (hardware), the threads; then sampling. */
        juce::Result start();

        void stop();          // the note playing let go, its take thrown away; what's recorded is kept
        void resume();        // on from the first shot not recorded
        void finish();        // the samples made from what's recorded (stopping first)
        void skip();          // voice: the note waiting is left out
        void redo();          // voice: the last note recorded is played again

        /** The message thread, several times a second. */
        void update();

        enum class State { ready, running, stopping, stopped, finishing, done, failed };
        enum class Step { idle, settling, playing, releasing, waiting, recording };

        struct Status
        {
            State state = State::ready;
            Step step = Step::idle;
            int shot = -1;                      // the one under way (or next), -1 none
            int recorded = 0, total = 0;
            int silent = 0;                     // takes that made no sound (left out)
            int written = 0;                    // WAVs written (finishing, done)
            float left = 0.0f, right = 0.0f;    // peaks since the last status
            double progress = 0.0;              // finishing: 0..1
            juce::String message;
        };

        Status getStatus();                     // takes the peaks since the last call
        State getState() const noexcept                         { return state.load(); }
        bool isRecorded (int index) const noexcept;
        bool isLeftOut (int index) const noexcept;

    private:
        // The audio thread (or Turbo's)
        void tapNotes (juce::MidiBuffer& into, int numSamples) noexcept override;
        void tapSound (const float* left, const float* right, int numSamples) noexcept override;
        double tapTempo() const noexcept override;
        void notesOn (juce::MidiBuffer& into, const Shot& shot, int offset) noexcept;
        void notesOff (juce::MidiBuffer& into, int offset) noexcept;
        void beginShot (juce::MidiBuffer& into) noexcept;
        void endTake() noexcept;
        void dropTake() noexcept;
        void nextShot (int from) noexcept;
        void pushFrames (const float* left, const float* right, int numSamples) noexcept;
        bool pushMarker (int type, int shot, int value) noexcept;

        // Message thread
        void attach();
        void detach();
        int firstOpenShot (int from) const noexcept;
        void resetAudioState (int from);

        class Keeper;           // keeps the takes, makes the samples
        class MidiSender;       // a synth's notes out
        class Renderer;         // Turbo and Unleashed

        AudioEngine& engine;
        const Settings settings;
        const std::vector<Shot> shots;
        const juce::File folder, takesFolder;
        const bool offline;

        std::unique_ptr<Keeper> keeper;
        std::unique_ptr<juce::MidiOutput> midiOut;
        std::unique_ptr<MidiSender> sender;
        std::unique_ptr<Renderer> renderer;

        std::atomic<State> state { State::ready };
        bool attached = false, finishWanted = false;
        double stopAskedMs = 0.0;
        juce::String problem;

        enum Mark : std::uint8_t { taken = 1, written = 2, skipped = 4, silent = 8 };
        std::unique_ptr<std::atomic<std::uint8_t>[]> marks;

        enum Command { noCommand = 0, stopCommand, skipCommand, redoCommand };
        std::atomic<int> command { noCommand };

        // The audio thread's (the message thread's only while the tap is off)
        Step step = Step::idle;
        int current = -1;
        int lastEnded = -1;
        juce::int64 stepSamples = 0, quietSamples = 0, tailSamples = 0, takeFrames = 0;
        juce::int64 holdSamples = 0;
        std::array<int, 8> heldNotes {};
        int heldCount = 0;
        bool takeOpen = false, takeBroken = false, armed = false, heardSound = false;
        juce::int64 silenceSamples = 0, settleSamples = 0, maxSettleSamples = 0, maxTailSamples = 0, armSamples = 0, minRecordSamples = 0;
        float silenceGain = 0.001f, triggerGain = 0.02f;
        std::atomic<double> tempoNow { 0.0 };

        static constexpr int preRollCapacity = 16384;
        std::vector<float> preRollLeft, preRollRight;
        int preRollAt = 0, preRollFilled = 0, preRollFrames = 0;

        std::atomic<int> shownShot { -1 };
        std::atomic<int> shownStep { 0 };
        std::atomic<bool> audioStopped { false }, audioDone { false };
        std::atomic<float> peakLeft { 0.0f }, peakRight { 0.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
    };

    //==============================================================================
    /** The Sample Builder: the slots' files (or clips) written as WAVs (each file's own name, as
        Sampler Arena; its rate, or the one chosen; the bit depth chosen), each with its loop when
        Create Legato Loop is on, and with Create SFZ the instrument (Sampler Arena's builder's:
        each sample on its key, the key shift applied, its velocity range). */
    class Build final : private juce::Thread
    {
    public:
        Build (const Settings& settings, const juce::File& folder);
        ~Build() override;

        void start()                                            { startThread(); }
        bool isDone() const noexcept                            { return done.load(); }
        double getProgress() const noexcept                     { return progress.load(); }
        int getWritten() const noexcept                         { return written.load(); }
        juce::String getProblem() const;                        // when it's done: empty, or what went wrong
        juce::File getSfzFile() const;                          // none without one

    private:
        void run() override;

        const Settings settings;
        const juce::File folder;
        std::atomic<bool> done { false };
        std::atomic<double> progress { 0.0 };
        std::atomic<int> written { 0 };
        juce::String problem;
    };

    /** A slot's sound: its file, or the clip's window of it with its gain and fades. */
    bool loadSlot (juce::AudioFormatManager& formats, const BuilderSlot& slot, juce::AudioBuffer<float>& audio, double& rate);

    /** A take at the WAVs' rate (the same buffer when it already is). */
    void convertRate (juce::AudioBuffer<float>& audio, int& numSamples, double fromRate, double toRate);
}
