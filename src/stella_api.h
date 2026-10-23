// D:\workspace\Fanan_web_hub\src\stella_api.h
//
// The Stella DSP API. Every block of a Stella plugin is a module written against this
// file. Modules are compiled to WebAssembly for the live preview, and natively for the
// exported CLAP and VST3 plugins, from the same source.
//
// A module:
//   - is a class derived from stella::Module, in its own .cpp file in the project's
//     modules folder;
//   - has mono audio inputs and outputs, each one float buffer at the sample rate
//     (stereo is two ports; control signals are ports too);
//   - declares its parameters (stable ids: presets, automation and the GUI rely on them);
//   - registers itself with a stella::Registrar at the end of its file.
//
// Rules inside process(): no allocation, no locks, no waiting, no I/O. Allocate in
// prepare(). Feedback between modules isn't allowed; keep feedback inside one module.
//
// Samplers read the plugin's sample library through stella::blob (at the end): the host reads
// the samples from the disk as they play, and a module only asks for each voice's sound.

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#define STELLA_COUNT(array) ((int) (sizeof (array) / sizeof ((array)[0])))

namespace stella
{
    //==========================================================================
    /** A parameter a module exposes. Once the plugin exists, its id never changes. */
    struct Param
    {
        const char* id;     // e.g. "cutoff": letters, digits and _ only
        const char* name;   // e.g. "Cutoff"
        float min, max, def;
        const char* unit;   // e.g. "Hz", "dB", "%", "ms", or ""
        float skew;         // 1 = linear; below 1 gives the low end more room (frequencies, times)
    };

    /** One note event inside the current block. */
    struct Note
    {
        int frame;          // where in the block it happens
        int note;           // MIDI note number 0..127
        float velocity;     // 0..1 (0 for note-offs)
        bool on;            // true: note on, false: note off
    };

    /** Everything a module gets with each block. */
    struct Context
    {
        double sampleRate;
        int numFrames;
        const Note* notes;  // this block's notes, in time order
        int numNotes;
        float pitchBend;    // -1..1
        float modWheel;     // 0..1

        // The host's tempo and transport, for tempo sync (see "Tempo sync" below).
        double tempo;       // beats (quarter notes) a minute: the song's, or the studio's 120
        double beat;        // where this block starts, in beats since the song's start (stands still while stopped)
        double barStart;    // where the bar this block starts in began, in beats
        bool playing;       // the song is playing (its position moves)
        int numerator;      // the time signature: 4 and 4 for 4/4
        int denominator;
    };

    //==========================================================================
    /** A DSP block. */
    class Module
    {
    public:
        virtual ~Module() = default;

        /** Before the first block, and whenever the sample rate or block size change.
            Allocate here, never in process(). */
        virtual void prepare (double sampleRate, int maxFrames) { (void) sampleRate; (void) maxFrames; }

        /** inputs[i] and outputs[i] hold context.numFrames samples. Unconnected inputs read
            silence. Write every sample of every output. */
        virtual void process (const Context& context, const float* const* inputs, float* const* outputs) = 0;

        /** Clear delay lines, voices and so on. */
        virtual void reset() {}

        /** The current value of this module's parameter number index (in declaration order). */
        float param (int index) const noexcept    { return params[index]; }

        /** Shows a value on the GUI (a meter or a lamp bound to "<moduleId>.<displayName>"):
            an LFO's position, an envelope's level, gain reduction... Once per block is enough. */
        void display (int index, float value) noexcept    { if (displays != nullptr) displays[index] = value; }

        float* params = nullptr;     // set by the runtime; modules only read it
        float* displays = nullptr;   // set by the runtime; write it with display()
    };

    //==========================================================================
    /** What a module type is: its name, ports and parameters. One per module class. */
    struct ModuleType
    {
        const char* type;                   // the class name, e.g. "LowPass"
        const char* name;                   // shown to the user, e.g. "Low-pass filter"
        const char* const* inputs;          // port names, e.g. { "in" }
        int numInputs;
        const char* const* outputs;         // e.g. { "out" }
        int numOutputs;
        const Param* params;
        int numParams;
        Module* (*create)();
        const char* const* displayNames = nullptr;   // optional: values the module shows, e.g. { "lfo", "gr" }
        int numDisplays = 0;
    };

    void registerModuleType (const ModuleType& type);

    /** At the end of a module's file:
            static stella::Registrar registrar ({ "LowPass", "Low-pass filter", inputs, STELLA_COUNT (inputs), ... });  */
    struct Registrar
    {
        explicit Registrar (const ModuleType& type)    { registerModuleType (type); }
    };

    //==========================================================================
    // Helpers modules may use.
    constexpr float pi    = 3.14159265358979323846f;
    constexpr float twoPi = 2.0f * pi;

    inline float noteToHz (float note) noexcept           { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
    inline float dbToGain (float db) noexcept             { return std::pow (10.0f, db * 0.05f); }
    inline float clamp (float v, float lo, float hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

    //==========================================================================
    // Tempo sync. A synced time is a note length: "1/4" is a beat, "1/8 dotted" three
    // sixteenths, "1/4 triplet" two thirds of a beat, "1 bar" four beats. A synced parameter is
    // a selector over syncDivisions: { "rate_sync", "Rate (sync)", 0, numSyncDivisions - 1,
    // 6, "", 1.0f } (6: "1/4"), its GUI a dropdown with syncDivisionNames' names in order.
    struct SyncDivision { const char* name; float beats; };

    constexpr SyncDivision syncDivisions[] {
        { "4 bars", 16.0f }, { "2 bars", 8.0f }, { "1 bar", 4.0f },
        { "1/2 dotted", 3.0f }, { "1/2", 2.0f }, { "1/2 triplet", 4.0f / 3.0f },
        { "1/4", 1.0f }, { "1/4 dotted", 1.5f }, { "1/4 triplet", 2.0f / 3.0f },
        { "1/8", 0.5f }, { "1/8 dotted", 0.75f }, { "1/8 triplet", 1.0f / 3.0f },
        { "1/16", 0.25f }, { "1/16 dotted", 0.375f }, { "1/16 triplet", 1.0f / 6.0f },
        { "1/32", 0.125f } };
    constexpr int numSyncDivisions = STELLA_COUNT (syncDivisions);

    /** A division's length in beats, from a selector parameter's value (rounded, kept in range). */
    inline float syncBeats (float division) noexcept
    {
        const int i = (int) (division + 0.5f);
        return syncDivisions[i < 0 ? 0 : (i >= numSyncDivisions ? numSyncDivisions - 1 : i)].beats;
    }

    /** The context's tempo, never 0. */
    inline double tempoOf (const Context& c) noexcept    { return c.tempo > 1.0 ? c.tempo : 120.0; }

    /** A length in beats, in seconds and in samples at the block's tempo (a synced delay's time). */
    inline double beatsToSeconds (const Context& c, double beats) noexcept   { return beats * 60.0 / tempoOf (c); }
    inline double beatsToSamples (const Context& c, double beats) noexcept   { return beatsToSeconds (c, beats) * c.sampleRate; }

    /** A synced cycle's frequency in Hz (an LFO's rate: one cycle every cycleBeats). */
    inline double syncHz (const Context& c, double cycleBeats) noexcept      { return tempoOf (c) / (60.0 * (cycleBeats > 1.0e-6 ? cycleBeats : 1.0e-6)); }

    /** Where in the song a frame of the block is, in beats. */
    inline double beatAt (const Context& c, int frame) noexcept              { return c.beat + (double) frame * tempoOf (c) / (60.0 * c.sampleRate); }

    /** While the song plays: a synced cycle's phase (0..1) at a frame, locked to the song's beat,
        so an LFO, a gate or an arpeggiator lands on the beat wherever playing starts. While it's
        stopped, keep running from your own phase at syncHz instead. */
    inline double syncPhase (const Context& c, int frame, double cycleBeats) noexcept
    {
        const double cycles = beatAt (c, frame) / (cycleBeats > 1.0e-6 ? cycleBeats : 1.0e-6);
        return cycles - std::floor (cycles);
    }

    /** Smooths a value per sample (a one-pole low-pass), for clickless parameter changes. */
    struct Smoother
    {
        void prepare (double sampleRate, float milliseconds = 20.0f) noexcept
        {
            coeff = 1.0f - std::exp (-1.0f / (0.001f * milliseconds * (float) sampleRate));
        }

        void snap (float v) noexcept         { value = v; }
        float next (float target) noexcept   { value += (target - value) * coeff; return value; }

        float value = 0.0f, coeff = 1.0f;
    };

    //==========================================================================
    // The plugin's sample library: blobs (sampled instruments, each one file made by the Sampling
    // Studio's SFZ to Blob), numbered 0, 1, 2... as the library lists them. The host reads their
    // samples from the disk as they play; a module finds the regions a note plays, starts a voice
    // for each, renders the voices' sound every block (at a speed: their pitch), and does the rest
    // itself (envelopes, filters, mixing).
    namespace blob
    {
        /** One region of an instrument: a sample and where it plays. Every field 4 bytes. */
        struct Region
        {
            std::int32_t sample;                    // its sample's number in the instrument
            std::int32_t lokey, hikey, keycenter;   // the keys it plays (MIDI notes), the key it was recorded at
            std::int32_t lovel, hivel;              // the velocities it plays (0..127)
            std::int32_t trigger;                   // attackTrigger, releaseTrigger (after the key's let go), firstTrigger, legatoTrigger
            std::int32_t seqLength, seqPosition;    // round robins: find() already picks the take whose turn it is
            std::int32_t group, offBy;              // a voice of a region whose offBy is a starting region's group stops (hi-hats)
            std::int32_t loopMode;                  // noLoop, oneShot (plays to its end whatever the key does), loopContinuous, loopSustain
            std::int32_t loopStart, loopEnd;        // its loop, in frames (loopEnd isn't played); -1: none
            std::int32_t offset, end;               // the frames it plays: from offset up to end
            std::int32_t channels;                  // 1 or 2 (a mono one renders the same sound into both)
            float sampleRate;                       // its sample's rate
            float volume;                           // dB
            float pan;                              // -100 (left) .. 100 (right)
            float tune;                             // cents (tune and transpose)
            float keytrack;                         // cents a key (100: a semitone a key)
            float veltrack;                         // amp_veltrack, % (100: soft notes much quieter)
            float ampAttack, ampHold, ampDecay;     // the SFZ's amp envelope, seconds
            float ampSustain;                       // %
            float ampRelease;                       // seconds
            float crossfade;                        // its loop's crossfade, seconds (the host does it)
        };

        static_assert (sizeof (Region) == 29 * 4, "every field 4 bytes: the same in WebAssembly and natively");

        enum : int { notLoaded = 0, loading = 1, ready = 2, unavailable = 3 };              // state()
        enum : int { attackTrigger = 0, releaseTrigger = 1, firstTrigger = 2, legatoTrigger = 3 };
        enum : int { noLoop = 0, oneShot = 1, loopContinuous = 2, loopSustain = 3 };
    }
}

#if defined (__wasm__)
 #define STELLA_HOST(name) __attribute__ ((import_module ("stella"), import_name (name)))
#else
 #define STELLA_HOST(name)
#endif

// What the host gives: Sonify for the live plugin, the plugin itself once it's exported.
extern "C"
{
    STELLA_HOST ("blob_count")   int   stella_blob_count();
    STELLA_HOST ("blob_name")    int   stella_blob_name (int instrument, char* text, int size);
    STELLA_HOST ("blob_state")   int   stella_blob_state (int instrument);
    STELLA_HOST ("blob_use")     void  stella_blob_use (int instrument);
    STELLA_HOST ("blob_regions") int   stella_blob_regions (int instrument);
    STELLA_HOST ("blob_region")  int   stella_blob_region (int instrument, int region, stella::blob::Region* info);
    STELLA_HOST ("blob_opcode")  float stella_blob_opcode (int instrument, int region, const char* name, float fallback);
    STELLA_HOST ("blob_find")    int   stella_blob_find (int instrument, int note, int velocity, int trigger, int* found, int maxFound);
    STELLA_HOST ("blob_start")   int   stella_blob_start (int instrument, int region, double fromFrame);
    STELLA_HOST ("blob_render")  int   stella_blob_render (int voice, float* left, float* right, int frames, float ratio);
    STELLA_HOST ("blob_release") void  stella_blob_release (int voice);
    STELLA_HOST ("blob_stop")    void  stella_blob_stop (int voice);
    STELLA_HOST ("blob_playing") int   stella_blob_playing (int voice);
    STELLA_HOST ("blob_demo")    int   stella_blob_demo (float* playSeconds, float* silentSeconds);
}

namespace stella::blob
{
    /** How many instruments the plugin's library has. */
    inline int count() noexcept                                    { return stella_blob_count(); }

    /** An instrument's name, into text (size bytes with its 0). Returns its length. */
    inline int name (int instrument, char* text, int size) noexcept { return stella_blob_name (instrument, text, size); }

    /** notLoaded, loading, ready, or unavailable (its file isn't on this computer, or it's locked
        until its product is registered). */
    inline int state (int instrument) noexcept                     { return stella_blob_state (instrument); }

    /** Keeps an instrument loaded (it loads when it's first used): call it every block for the
        instrument the plugin plays. One unused for a while is let go. */
    inline void use (int instrument) noexcept                      { stella_blob_use (instrument); }

    /** Its regions (0 until it's ready). */
    inline int regions (int instrument) noexcept                   { return stella_blob_regions (instrument); }
    inline bool region (int instrument, int index, Region& info) noexcept   { return stella_blob_region (instrument, index, &info) != 0; }

    /** Any opcode the SFZ gave a region, as a number (fallback when it gave none). */
    inline float opcode (int instrument, int index, const char* name, float fallback) noexcept
    {
        return stella_blob_opcode (instrument, index, name, fallback);
    }

    /** The regions a note plays (at a velocity 0..127): attackTrigger when it's played,
        releaseTrigger when it's let go. Round robins take turns, random regions are picked, and
        keyswitches change what plays (a keyswitch key itself finds none). Returns how many. */
    inline int find (int instrument, int note, int velocity, int trigger, int* found, int maxFound) noexcept
    {
        return stella_blob_find (instrument, note, velocity, trigger, found, maxFound);
    }

    /** Starts a voice on a region, from its offset (or from fromFrame). Returns the voice, or 0
        when it can't (not ready, or every voice is playing). */
    inline int start (int instrument, int index, double fromFrame = -1.0) noexcept   { return stella_blob_start (instrument, index, fromFrame); }

    /** The voice's next frames into left and right (written, not added), at a speed: ratio frames
        of its sample per frame out (pitchRatio). Its loop goes round by itself. Returns the frames
        it had: fewer once its sample has ended (the rest silent; stop it then). */
    inline int render (int voice, float* left, float* right, int frames, float ratio) noexcept
    {
        return stella_blob_render (voice, left, right, frames, ratio);
    }

    /** The key's let go: a loopSustain voice leaves its loop and plays on to its end. */
    inline void release (int voice) noexcept                       { stella_blob_release (voice); }

    /** The voice is done (its envelope has ended, or it's stolen): its place is free again. */
    inline void stop (int voice) noexcept                          { stella_blob_stop (voice); }

    inline bool playing (int voice) noexcept                       { return stella_blob_playing (voice) != 0; }

    /** Whether the library plays a protected blob library (one made with a product's protection)
        this computer isn't registered for: then the plugin plays its demo, playSeconds of sound and
        silentSeconds of silence (the runtime does it, not a module). Never blocks. */
    inline bool demo (float& playSeconds, float& silentSeconds) noexcept   { return stella_blob_demo (&playSeconds, &silentSeconds) != 0; }

    /** The speed a region plays a note at: its keytrack from its key, its tune, and its sample's
        rate against the plugin's (semitones: more, a pitch bend or a glide). */
    inline float pitchRatio (const Region& r, float note, double sampleRate, float semitones = 0.0f) noexcept
    {
        const auto shift = (note - (float) r.keycenter) * r.keytrack / 100.0f + r.tune / 100.0f + semitones;
        return std::pow (2.0f, shift / 12.0f) * (float) ((double) r.sampleRate / (sampleRate > 0.0 ? sampleRate : 48000.0));
    }

    /** A region's loudness at a velocity (0..1): its volume, and its amp_veltrack's curve. */
    inline float gainOf (const Region& r, float velocity) noexcept
    {
        const auto track = clamp (r.veltrack / 100.0f, 0.0f, 1.0f);
        return dbToGain (r.volume) * ((1.0f - track) + track * velocity * velocity);
    }

    /** A region's pan as left and right gains (equal power). */
    inline void panOf (const Region& r, float& left, float& right) noexcept
    {
        const auto angle = (clamp (r.pan, -100.0f, 100.0f) + 100.0f) / 200.0f * (pi * 0.5f);
        left = std::cos (angle) * 1.41421356f;
        right = std::sin (angle) * 1.41421356f;
        left = left > 1.0f ? 1.0f : left;
        right = right > 1.0f ? 1.0f : right;
    }
}
