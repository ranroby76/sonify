// D:\workspace\Fanan_web_hub\src\LiveShow.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "AnalogBoard.h"
#include "SonifyDsp.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

//==============================================================================
/**
    The Live Show (a studio of its own): a DJ's decks and mixer, with the singers and players
    on it too, playing on Main in place of the song (VirtualDJ is its model).

      the decks       two or four, each playing a track (an audio file in memory) at its own
                      speed (the tempo slider), with Play, a position to jump to, four hot cues
                      and four loops (each between two markers), and the track's own volume
                      (Track Volume). A track keeps its key at any tempo (Signalsmith Stretch, as
                      Sonify's Sync Tempo), and Key moves it by semitones (to the singer). Each
                      has a channel on the mixer: Gain, its fader, and A, Thru or B on the
                      crossfader. A track's tempo and first downbeat (and key) are found by
                      listening (RecordingAnalysis), or typed: with SYNC on, it plays at the
                      song's tempo (the DAW's), its beats on those of another deck synced
      live channels   Studio tracks marked as channels (Track::live): a singer's mic, an oud, a
                      keyboard, as their tracks make them (the input through the track's
                      plugins), each with its own fader, pan and mute, and Duck (the dynamic EQ
                      listens to it)
      the effects     the show's effects channel (the master's FX button opens its window), each
                      effect with a switch of its own:
                        the dynamic EQ (Grex's ducker), on the decks' music: it makes room for
                        the voices. Four bell points, each an EQ bell with a level of its own
                        (Gain: silence up to 25 dB louder, as it is in the middle) going down
                        from there as far as the voices in that same band ask for, no further
                        than its Duck; and a level point that turns the whole music down
                        (talking over it: a podcast's bed), each point with a switch of its own
                        too. The channels with Duck on are what it listens to, after their faders
                        then, on the master after its fader (MasterEffects): a compressor, a
                        drive (soft saturation), the stereo width (wider or narrower, the lows
                        kept in the middle) and the limiter (Gain, and a peak limiter that never
                        looks ahead). Nothing is delayed; while the limiter is on, nothing leaves
                        above its Ceiling
      the pads        eight sounds (jingles, stings, effects) played at a touch, on their own
                      fader, never ducked
      the master      the music, the voices and the pads together, the master fader, then the
                      compressor, the drive, the stereo width and the limiter
      the headphones  a deck or a channel with its headphones button on is heard there before
                      its fader (the next track, cued while the room hears the show), against
                      the show by the Mix knob (or split: the cue left, the show right), at their
                      own level, through a limiter of their own. Nothing cued: the show
      one player      the playlist's tabs: the first two decks are one player (setOnePlayer).
                      The second plays through the first's channel (Gain, headphones), only to
                      crossfade the next track in; the crossfader leaves both alone, and neither
                      has a fader of its own (the master's is the one)
      recording       the night as it's heard (the master), and each deck (after its fader and
                      the crossfader) and each channel (as it comes in) on files of their own
      the stream      the master, as it's heard, to the live stream (a SoundTap: LiveStream.h)

    Settings are atomics (AnalogBoard::Settings) anyone may set at any time; the audio thread
    reads them each block. Tracks and pad sounds are loaded on another thread (DeckTrack::load)
    and put in place under the engine's graph lock.

    prepare() allocates; nothing else does.
*/
namespace LiveShow
{
    using ParamSpec = AnalogBoard::ParamSpec;
    template <size_t count> using Settings = AnalogBoard::Settings<count>;
    using Meter = AnalogBoard::Meter;

    /** Where a recording's blocks go (a file, written on a thread of its own). */
    using Recorder = juce::AudioFormatWriter::ThreadedWriter;

    constexpr int maxDecks = 4;
    constexpr int numHotCues = 4;
    constexpr int numLoops = 4;             // a deck's loops, each between two markers
    constexpr int numPads = 8;
    constexpr float silentDb = -72.0f;      // a fader at or below this is closed
    constexpr double longestTrackMinutes = 60.0;

    //==============================================================================
    // A deck's settings, in this order. Its speed (pitch, and range: how far pitch goes) is set
    // from its tempo (bpm: the player's BPM) when its track follows it (trackSync); sync: the
    // player's BPM is the song's (MainLive.cpp).
    enum DeckParam
    {
        deckGain, deckVolume, deckAssign, deckPitch, deckRange,
        deckKey, deckPhones, deckSync, deckTrackVolume, deckBpm, deckTrackSync,
        numDeckParams
    };

    /** A deck's channel on the mixer (Gain, its fader, its place on the crossfader, its
        headphones), as against its player's settings (its tempo, key, Sync, the track's volume). */
    constexpr bool isChannelParam (int index) noexcept
    {
        return index == deckGain || index == deckVolume || index == deckAssign || index == deckPhones;
    }

    // A live channel's.
    enum ChannelParam
    {
        channelVolume, channelPan, channelMute, channelDuck, channelPhones,
        numChannelParams
    };

    // The master's: the crossfader, the decks shown, the master fader, the pads' fader, the
    // headphones, and the effects channel, each effect's switch first: the dynamic EQ (the
    // ducker: four bells and the level, each point's own switch first; a bell's Gain and Duck are
    // levels as Track Volume has them: silence, 0.5 as it is, 1 for 25 dB up), the compressor,
    // the drive, the stereo width and the limiter.
    enum MasterParam
    {
        crossfader, crossCurve, deckCount, masterVolume,
        padVolume,
        phonesMix, phonesLevel, phonesSplit,
        duckOn,
        bell1On, bell1Freq, bell1Q, bell1Gain, bell1Duck, bell1Threshold, bell1Ratio, bell1Attack, bell1Release,
        bell2On, bell2Freq, bell2Q, bell2Gain, bell2Duck, bell2Threshold, bell2Ratio, bell2Attack, bell2Release,
        bell3On, bell3Freq, bell3Q, bell3Gain, bell3Duck, bell3Threshold, bell3Ratio, bell3Attack, bell3Release,
        bell4On, bell4Freq, bell4Q, bell4Gain, bell4Duck, bell4Threshold, bell4Ratio, bell4Attack, bell4Release,
        levelOn, levelDepth, levelThreshold, levelRatio, levelAttack, levelRelease,
        compressorOn, compressorThreshold, compressorRatio, compressorAttack, compressorRelease,
        compressorKnee, compressorMakeup,
        driveOn, driveAmount, driveTone, driveMix, driveOutput,
        widthOn, widthAmount, widthBassMono, widthOutput,
        limiterOn, limiterGain, limiterCeiling, limiterRelease,
        numMasterParams
    };

    /** The effects channel's effects, as the page names them, and each one's settings (its switch,
        then its controls: those a reset puts back; the dynamic EQ's points' own switches among them). */
    struct Effect
    {
        const char* name;                   // "dynamicEq", "compressor", "drive", "width", "limiter"
        int on, first, last;
    };

    inline constexpr std::array<Effect, 5> effectsChannel {{
        { "dynamicEq",  duckOn,       bell1On,             levelRelease },
        { "compressor", compressorOn, compressorThreshold, compressorMakeup },
        { "drive",      driveOn,      driveAmount,         driveOutput },
        { "width",      widthOn,      widthAmount,         widthOutput },
        { "limiter",    limiterOn,    limiterGain,         limiterRelease },
    }};

    const std::array<ParamSpec, numDeckParams>& deckSpecs();
    const std::array<ParamSpec, numChannelParams>& channelSpecs();
    const std::array<ParamSpec, numMasterParams>& masterSpecs();

    /** A setting's place by its id; -1 when there's none. */
    int deckIndexOf (const juce::String& id);
    int channelIndexOf (const juce::String& id);
    int masterIndexOf (const juce::String& id);

    /** The tempo slider's ranges ("8%"), as fractions of the track's own speed: the one it has is
        the smallest that holds the tempo it was last set to (SYNC, a playlist's track). */
    float pitchRangeOf (int choice) noexcept;
    constexpr int numPitchRanges = 4;

    /** A track's volume (Track Volume, 0..1) as a gain: silence to as recorded in its lower half,
        as recorded to 25 dB louder in its upper half. */
    float trackVolumeGain (float position) noexcept;

    /** Where a deck sits on the crossfader when the decks start fresh: 1 and 3 on A, 2 and 4 on B. */
    inline int defaultAssign (int deck) noexcept    { return deck % 2 == 0 ? 0 : 2; }

    /** Whether this build keeps a track's key at another tempo (Signalsmith Stretch is in it).
        Without it, a deck plays as a record does: faster is higher. */
    bool canKeepKey() noexcept;

    /** A key's name: "Am", "F#", "Bb". Empty for none (tonic -1). */
    juce::String keyName (int tonic, bool minor);

    //==============================================================================
    /** A track on a deck, or a pad's sound: the file's sound at the device's rate (16-bit,
        interleaved: a long track in memory costs half what floats would), and its waveform for
        the page (wavePerSecond points a second, four bytes each: the loudest moment, then how
        much low, middle and high it holds, 0..255). Never changed once made. */
    struct DeckTrack
    {
        juce::File file;
        juce::String title;                     // the file's name, without its extension
        std::vector<juce::int16> samples;       // frames of `channels` samples
        int channels = 2;
        juce::int64 frames = 0;
        double playbackRate = 0.0;
        double lengthSeconds = 0.0;

        double wavePerSecond = 100.0;           // exactly: playbackRate / the frames each point covers
        std::vector<juce::uint8> wave;

        float sampleAt (int channel, juce::int64 frame) const noexcept
        {
            if (frame < 0 || frame >= frames)
                return 0.0f;

            return static_cast<float> (samples[static_cast<size_t> (frame * channels + juce::jmin (channel, channels - 1))]) * (1.0f / 32768.0f);
        }

        /** Reads a file into memory at playbackRate (slow: any thread but the audio thread).
            formats: a manager only this thread uses. Null and why when it can't. standsFor: the
            file it's the sound of (a video, its sound read out into a WAV): its name and path.
            keepGoing false (Sonify closing) stops it: null. */
        static std::shared_ptr<const DeckTrack> load (const juce::File& file, double playbackRate,
                                                      juce::AudioFormatManager& formats, juce::String& error,
                                                      const juce::File& standsFor = {},
                                                      const std::function<bool()>& keepGoing = {});

        /** The same from a reader already open (a video's sound, read straight from the video:
            VideoSound). named: the file it's the sound of (its name and path). */
        static std::shared_ptr<const DeckTrack> load (std::unique_ptr<juce::AudioFormatReader> reader, const juce::File& named,
                                                      double playbackRate, juce::String& error,
                                                      const std::function<bool()>& keepGoing = {});
    };

    //==============================================================================
    /** What Sonify knows of a track: its tempo, first downbeat and key, found by listening (or
        the tempo typed by the user), its hot cues and its loops (the markers where they were left). */
    struct TrackInfo
    {
        double bpm = 0.0;                       // quarter notes a minute; 0: no steady beat found
        double firstBeat = 0.0;                 // its first downbeat, seconds into the file
        int keyTonic = -1;                      // 0 C .. 11 B; -1: not known
        bool keyMinor = false;
        bool bpmByUser = false;                 // typed: listening again doesn't change it
        bool heard = false;                     // listened to (else only its markers are kept yet)
        std::array<double, numHotCues> hotCues { -1.0, -1.0, -1.0, -1.0 };   // seconds; -1: not placed yet

        struct Loop
        {
            double start = -1.0, end = -1.0;    // seconds; start -1: not on (no markers)
            bool isOn() const noexcept          { return start >= 0.0 && end > start; }
        };

        std::array<Loop, numLoops> loops {};

        juce::var toVar() const;
        static TrackInfo fromVar (const juce::var& data);
    };

    /** Listens to a track for its tempo, first downbeat and key (its first four minutes: slow, any
        thread but the audio thread). keepGoing false stops it (the result is then empty). */
    TrackInfo analyse (const DeckTrack& track, const std::function<bool()>& keepGoing);

    /** The same, from a file not loaded (the library's Analyze). */
    TrackInfo analyse (const juce::File& file, juce::AudioFormatManager& formats, const std::function<bool()>& keepGoing,
                       juce::String& error);

    /** The same, from a reader already open (a video's sound: VideoSound), read from its start. */
    TrackInfo analyse (juce::AudioFormatReader& reader, const std::function<bool()>& keepGoing);

    /** What's known of the tracks the Live Show has met (Live Show tracks.json), by file: an entry
        holds while its file is unchanged (its size and date). Message thread. */
    class TrackLibrary
    {
    public:
        explicit TrackLibrary (juce::File storeFile);

        std::optional<TrackInfo> find (const juce::File& track);
        void set (const juce::File& track, const TrackInfo& info);

        bool needsSaving() const noexcept           { return dirty; }
        void save();

    private:
        struct Entry
        {
            juce::int64 size = 0, modified = 0;
            TrackInfo info;
        };

        void load();

        juce::File store;
        std::map<juce::String, Entry> entries;
        bool loaded = false, dirty = false;
    };

    //==============================================================================
    /** One deck: its player and its channel on the mixer. */
    class Deck
    {
    public:
        Deck();
        ~Deck();

        Settings<numDeckParams> settings { deckSpecs() };
        Meter meter;                            // its channel, before its fader

        //==========================================================================
        // Any thread: what the page asks for.

        void setPlaying (bool shouldPlay) noexcept      { wantPlaying.store (shouldPlay); }

        /** Fades it in or out, over its other gains: to a level (0..1) over seconds (0: at once),
            from where it is (or from `from`), along an equal-power curve (two decks crossfading
            keep the loudness). A new track starts at full. Any thread. */
        void fade (float to, double seconds, float from = -1.0f) noexcept
        {
            fadeFromRequest.store (from);
            fadeToRequest.store (to);
            fadeSecondsRequest.store (seconds);
            fadeRequests.fetch_add (1, std::memory_order_release);
        }

        /** A CDJ's Cue: while it plays, back to the cue point, stopped; while it's stopped, the
            cue point is set where it is. */
        void cue() noexcept;

        /** Jumps there (seconds into the track); playing goes on from there. Stopped, it's there at
            once for the page (the audio thread makes it so in its next block). */
        void seek (double seconds) noexcept
        {
            seekRequest.store (juce::jmax (0.0, seconds));

            if (! wantPlaying.load())
                positionSeconds.store (juce::jmax (0.0, seconds));
        }

        /** Moves it on (back, below 0) by this much of the track (seconds), from wherever it is when
            the audio thread gets to it: a beat lined up stays lined up, however long the asking took. */
        void shift (double seconds) noexcept
        {
            auto now = shiftRequest.load();

            while (! shiftRequest.compare_exchange_weak (now, now + seconds)) {}

            if (! wantPlaying.load())
                positionSeconds.store (juce::jmax (0.0, positionSeconds.load() + seconds));
        }

        /** A new track: stopped, at its start, its cue point there too, no loop, no beat known yet. */
        void startOver() noexcept;

        /** Its beats onto those of deck `lead` (both tempos and first beats known): the audio thread
            moves it on (or back, less than a beat) in the block it starts playing in (or the next,
            while it plays), from where both are in that very block. -1: none waiting. Any thread. */
        void alignTo (int lead) noexcept                { alignRequest.store (lead); }

        bool isPlaying() const noexcept                 { return wantPlaying.load(); }

        /** How far it's faded in now (fade(): 0 silent .. 1 full, along the fade's straight line;
            a picture's dissolve follows it). Any thread. */
        float getFadeLevel() const noexcept             { return fadeLevel.load (std::memory_order_relaxed); }

        /** Where it's heard now (seconds into the track): through the stretcher, its delay allowed for. */
        double getPosition() const noexcept             { return positionSeconds.load(); }
        double getCuePoint() const noexcept             { return cuePoint.load(); }

        /** Its speed now, as a share of the track's own (1: as recorded). */
        double getSpeed() const noexcept;

        /** Its tempo range and its slider's place in it, together: the audio thread never plays the
            new range with the old place (or the other way round). Message thread. */
        void setSpeed (int range, float pitch) noexcept;

        /** The track's beat: its tempo (0: not known) and first downbeat (seconds). */
        void setBeat (double bpm, double firstBeatSeconds) noexcept   { trackBpm.store (bpm); firstBeat.store (firstBeatSeconds); }
        double getTrackBpm() const noexcept             { return trackBpm.load(); }
        double getFirstBeat() const noexcept            { return firstBeat.load(); }

        /** Its tempo now (the track's, at its speed); 0 while the track's isn't known. */
        double getBpm() const noexcept                  { return getTrackBpm() * getSpeed(); }

        /** A loop between two places (seconds); end at or before start: none. */
        void setLoop (double startSeconds, double endSeconds) noexcept   { loopStart.store (startSeconds); loopEnd.store (endSeconds); }
        double getLoopStart() const noexcept            { return loopStart.load(); }
        double getLoopEnd() const noexcept              { return loopEnd.load(); }
        bool isLooping() const noexcept                 { return loopEnd.load() > loopStart.load() && loopStart.load() >= 0.0; }

        //==========================================================================
        // Audio thread (or while no callback runs).

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        /** The track it plays (null: none). Under the engine's graph lock: the caller keeps it alive. */
        void setTrack (const DeckTrack* newTrack) noexcept;
        const DeckTrack* getTrack() const noexcept      { return track; }

        /** This block: the track at the deck's speed (and key), through its channel (Gain), its
            fader last. getOutput() has it. settle: the gains start where they are. */
        void process (int numSamples, bool settle) noexcept;

        /** The channel it plays through (Gain, its fader, its headphones): its own (null), or
            another deck's (the playlist's one player). faderless: its fader isn't used (the
            playlist's one player: the master's fader is the one). Audio thread, before process(). */
        void playThrough (const Settings<numDeckParams>* channel, bool faderless = false) noexcept
        {
            channelFrom = channel;
            noFader = faderless;
        }

        /** Audio thread, before the decks play a block: a beat lined up when it's asked for (alignTo)
            and this deck plays (or is about to), against `decks` (the lead among them). */
        template <size_t count>
        void lineUp (const std::array<Deck, count>& decks) noexcept
        {
            auto lead = alignRequest.load (std::memory_order_relaxed);

            if (lead < 0 || ! wantPlaying.load (std::memory_order_relaxed) || ! alignRequest.compare_exchange_strong (lead, -1))
                return;

            if (juce::isPositiveAndBelow (lead, static_cast<int> (count)) && &decks[static_cast<size_t> (lead)] != this)
                lineUpWith (decks[static_cast<size_t> (lead)]);
        }

        const float* getOutput (int channel) const noexcept   { return output.getReadPointer (juce::jlimit (0, 1, channel)); }

        /** With its headphones button on: this block before its fader (else null). */
        const float* getCue (int channel) const noexcept      { return cueing ? preFader.getReadPointer (juce::jlimit (0, 1, channel)) : nullptr; }

        /** Its recording (after its fader and the crossfader), under the engine's graph lock. */
        Recorder* recorder = nullptr;

    private:
        struct Stretcher;

        void playPlain (float* left, float* right, int numSamples) noexcept;
        void playStretched (float* left, float* right, int numSamples) noexcept;

        /** count frames of the track from `from` (frames) into the stretcher's input, round the
            loop when there is one; returns where it got to. */
        double fillInput (double from, int count, float* left, float* right) const noexcept;

        /** The loop in the track's frames now (false: none). */
        bool loopFrames (double& start, double& end) const noexcept;

        /** A jump waiting for the sound to fade out; takeJump makes it (false: there was none). */
        bool jumpWaiting() const noexcept               { return pendingJump >= 0.0 || pendingShift != 0.0; }
        bool takeJump() noexcept;

        /** Its next beat onto the lead's: a jump of less than a beat (audio thread). */
        void lineUpWith (const Deck& lead) noexcept;

        // What the page asks for.
        std::atomic<bool> wantPlaying { false };
        std::atomic<double> seekRequest { -1.0 }, positionSeconds { 0.0 }, cuePoint { 0.0 };
        std::atomic<double> shiftRequest { 0.0 };
        std::atomic<double> trackBpm { 0.0 }, firstBeat { 0.0 }, loopStart { -1.0 }, loopEnd { -1.0 };
        std::atomic<int> alignRequest { -1 };
        std::atomic<juce::uint32> speedWrites { 0 };     // odd while setSpeed writes the range and the slider
        mutable std::atomic<double> lastSpeed { 1.0 };  // the speed as it was last read whole

        // The player, audio thread.
        const DeckTrack* track = nullptr;
        double sampleRate = 48000.0;
        double position = 0.0;                  // where it's heard, in the track's frames
        double pendingJump = -1.0;              // where to go once the sound has faded out
        double pendingShift = 0.0;              // or how far to move on from there (frames)
        float level = 0.0f;                     // the click-free fade in and out
        std::atomic<float> fadeFromRequest { -1.0f }, fadeToRequest { 1.0f };
        std::atomic<double> fadeSecondsRequest { 0.0 };
        std::atomic<int> fadeRequests { 0 };
        int fadeSeen = 0;
        float fadePosition = 1.0f, fadeTarget = 1.0f, fadeStep = 0.0f;   // fade(): 0 silent .. 1 full
        std::atomic<float> fadeLevel { 1.0f };  // fadePosition, for anyone
        const Settings<numDeckParams>* channelFrom = nullptr;   // another deck's channel (null: its own)
        bool noFader = false;                                   // its fader out of use (the playlist's one player)
        bool stretching = false;                // another tempo or key: through the stretcher
        bool handingOver = false;               // this block: from playing plain to the stretcher
        bool primed = false;                    // the stretcher is at `position`
        double inputPosition = 0.0;             // where the stretcher has read to (frames)
        double inputCarry = 0.0;                // the part of a frame it hasn't read yet
        std::unique_ptr<Stretcher> stretcher;

        // The channel, audio thread.
        float lastGain = 1.0f, lastFader = 1.0f;
        juce::AudioBuffer<float> output, preFader;
        juce::AudioBuffer<float> plainBlock;    // the block played plain, crossfaded into the stretcher's
        bool cueing = false;

        JUCE_DECLARE_NON_COPYABLE (Deck)
    };

    //==============================================================================
    /** A live channel: a Studio track's sound on the Live Show's mixer. Each instrument and audio
        track has one (Track::live); only the tracks marked as channels play. */
    class Channel
    {
    public:
        Settings<numChannelParams> settings { channelSpecs() };
        Meter meter;                            // its sound as it comes in, before its fader

        void prepare (double rate) noexcept     { sampleRate = rate; reset(); }
        void reset() noexcept                   { lastLeft = lastRight = lastSide = 0.0f; meter.clear(); }

        /** Its recording (as it comes in, before its fader), under the engine's graph lock. */
        Recorder* recorder = nullptr;

    private:
        friend class Mixer;
        double sampleRate = 48000.0;
        float lastLeft = 0.0f, lastRight = 0.0f, lastSide = 0.0f;
    };

    //==============================================================================
    /** The show's sound as it's heard, for a live stream: the audio thread puts each block in,
        the stream's thread takes it out (lock-free; what doesn't fit is let go, and counted). */
    class SoundTap
    {
    public:
        SoundTap (double sampleRate, int capacitySamples)
            : rate (sampleRate), fifo (juce::jmax (1024, capacitySamples)),
              left (static_cast<size_t> (fifo.getTotalSize()), 0.0f), right (left)
        {
        }

        double getSampleRate() const noexcept       { return rate; }
        int getNumReady() const noexcept            { return fifo.getNumReady(); }
        juce::int64 getDropped() const noexcept     { return dropped.load (std::memory_order_relaxed); }

        /** Audio thread. */
        void push (const float* l, const float* r, int numSamples) noexcept
        {
            const auto scope = fifo.write (numSamples);
            copyIn (l, r, scope.startIndex1, scope.blockSize1, 0);
            copyIn (l, r, scope.startIndex2, scope.blockSize2, scope.blockSize1);

            if (const auto lost = numSamples - scope.blockSize1 - scope.blockSize2; lost > 0)
                dropped.fetch_add (lost, std::memory_order_relaxed);
        }

        /** The reader: up to most samples into l and r; how many. */
        int pull (float* l, float* r, int most) noexcept
        {
            const auto scope = fifo.read (most);
            copyOut (l, r, scope.startIndex1, scope.blockSize1, 0);
            copyOut (l, r, scope.startIndex2, scope.blockSize2, scope.blockSize1);
            return scope.blockSize1 + scope.blockSize2;
        }

    private:
        void copyIn (const float* l, const float* r, int at, int count, int from) noexcept
        {
            if (count <= 0)
                return;

            std::copy (l + from, l + from + count, left.begin() + at);
            std::copy (r + from, r + from + count, right.begin() + at);
        }

        void copyOut (float* l, float* r, int at, int count, int into) const noexcept
        {
            if (count <= 0)
                return;

            std::copy (left.begin() + at, left.begin() + at + count, l + into);
            std::copy (right.begin() + at, right.begin() + at + count, r + into);
        }

        double rate;
        juce::AbstractFifo fifo;
        std::vector<float> left, right;
        std::atomic<juce::int64> dropped { 0 };

        JUCE_DECLARE_NON_COPYABLE (SoundTap)
    };

    //==============================================================================
    /** A pad: a sound played once at a touch (a jingle, a sting). */
    class Pad
    {
    public:
        /** Plays it from its start (playing already: stops it). Any thread. */
        void toggle() noexcept                  { command.store (playing.load() ? stopCommand : playCommand); }
        void stop() noexcept                    { command.store (stopCommand); }
        bool isPlaying() const noexcept         { return playing.load(); }

        /** Pressed to play, not started yet: the audio thread starts it in the show's next block (the
            show is to come on for it). Any thread. */
        bool isStarting() const noexcept        { return command.load() == playCommand; }

        /** A press not taken up yet let go (the show can't play now: it never plays later, by itself).
            Any thread. */
        void cancel() noexcept                  { command.store (0); }

        /** Its sound (null: none), under the engine's graph lock: the caller keeps it alive. A press
            waiting for the sound it had is let go. */
        void setSound (const DeckTrack* newSound) noexcept   { sound = newSound; at = 0.0; level = 0.0f; playing.store (false); command.store (0); }
        const DeckTrack* getSound() const noexcept           { return sound; }

    private:
        friend class Mixer;
        static constexpr int playCommand = 1, stopCommand = 2;
        std::atomic<int> command { 0 };
        std::atomic<bool> playing { false };
        const DeckTrack* sound = nullptr;
        double at = 0.0;
        float level = 0.0f;
        bool stopping = false;
    };

    //==============================================================================
    /** The effects channel's dynamic EQ (Grex's ducker): an EQ on the music (the decks) whose four
        bells each have a level of their own (Gain: silence up to 25 dB louder) and go down from it
        where the voices are, as far as they ask for and no further than the bell's Duck (silence:
        as far as they ask); and the level point, the whole music turned down by up to its Depth.
        Switched off (the effect, or a point on its own): as it came, never a jump. */
    class Ducker
    {
    public:
        static constexpr int numBells = 4;
        static constexpr int numPoints = numBells + 1;  // four bells, and the level

        void prepare (double sampleRate);
        void reset() noexcept;

        /** music through the bells and ducked by side (the voices it listens to, mono), as
            settings say. on: it works; off, the music goes through as it came, and it still
            listens (the page's meters show what it hears). */
        void process (float* left, float* right, const float* side, int numSamples,
                      const Settings<numMasterParams>& settings, bool on) noexcept;

        /** For the page: each point's gain now (dB: a bell's from silence, -100, up to +25; the
            level's 0 or below), and how loud the voices are in its band (dBFS). */
        float getGainDb (int point) const noexcept      { return gainDb[static_cast<size_t> (juce::jlimit (0, numPoints - 1, point))].load (std::memory_order_relaxed); }
        float getVoiceLevel (int point) const noexcept  { return voiceLevel[static_cast<size_t> (juce::jlimit (0, numPoints - 1, point))].load (std::memory_order_relaxed); }

        /** A bell's settings, by bell (0..3): its switch, then the rest in the same order. */
        static constexpr int bellParam (int bell, int first) noexcept   { return first + bell * (bell2On - bell1On); }

    private:
        struct Point
        {
            SonifyDsp::Biquad band;                         // a bell's band (0 dB at its centre)
            std::array<SonifyDsp::BiquadState, 2> music {};
            SonifyDsp::BiquadState voice;
            float envelope = 0.0f, attack = 0.0f, release = 0.0f;
            float gain = 1.0f, step = 0.0f;                 // its gain now, gliding to the next
            float level = 1.0f;                             // a bell's own level (Gain); the level point's: 1
            float floor = 1.0f;                             // as low as the voices take it (Duck; 0: silence), never above level
            float threshold = -40.0f, slope = 0.5f;
            bool on = true;                                 // its own switch (off: as it came, and it still listens)
            bool working = false;                           // switched in (the effect's switch and its own)
            int glide = 0;                                  // samples left of a glide (switched, or its level moved: never a jump)
            float loudest = 0.0f;                           // this block's envelope, at its highest
        };

        void refresh (const Settings<numMasterParams>& settings) noexcept;

        double sampleRate = 48000.0;
        juce::uint32 seenChanges = 0xffffffffu;
        bool fresh = true;                              // nothing set yet: a bell's level is where it is, no glide
        std::array<Point, numPoints> points {};
        std::array<std::atomic<float>, numPoints> gainDb {}, voiceLevel {};
        int countdown = 0;
    };

    //==============================================================================
    /** The effects channel on the master, after its fader (its dynamic EQ works on the music
        before it: Ducker): the compressor, the drive, the stereo width and the limiter, in that
        order, each switched on or off on its own (gliding over a block, both ways: no jump).
        Nothing is delayed.
          the compressor  Threshold, Ratio, Attack, Release, Knee, Makeup
          the drive       soft saturation: Drive (how hard: the peaks stay where they were and the
                          rest comes up, rounder), Tone (darker or brighter), Mix (with the sound as
                          it came) and Output
          the width       the stereo image wider or narrower, as mid and side (Dozius' widener):
                          Width (0% mono, 100% as it is, up to 200%), Bass Mono (the side below it
                          taken out, 12 dB an octave: the bass and the kick stay in the middle, firm
                          on any PA; at its lowest, off) and Output
          the limiter     Gain going in, then a peak limiter that never looks ahead: on the sample
                          a peak comes in it pulls down to the Ceiling at once, holds there a
                          moment (the bass's own waves don't move it) and lets go over its
                          Release. While it's on, nothing leaves above the Ceiling
        Audio thread. */
    class MasterEffects
    {
    public:
        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        /** In place. settle: its controls start where they are (no glide). */
        void process (float* left, float* right, int numSamples, const Settings<numMasterParams>& settings, bool settle) noexcept;

        /** Since the last look (any thread): the most the compressor, the drive (how far its curve
            rounds the peaks) and the limiter pulled down (dB), and the loudest going into the
            limiter, after its Gain (dBFS). */
        float takeCompressorReduction() noexcept    { return compressorMost.exchange (0.0f, std::memory_order_relaxed); }
        float takeDriveSaturation() noexcept        { return driveMost.exchange (0.0f, std::memory_order_relaxed); }
        float takeLimiterReduction() noexcept       { return limiterMost.exchange (0.0f, std::memory_order_relaxed); }
        float takeInputPeak() noexcept              { return inputMost.exchange (-100.0f, std::memory_order_relaxed); }

        /** The stereo after the width (whether it's on or not), any thread: how alike the two
            sides are (+1 mono, 0 unrelated or one side alone, -1 out of phase; averaged over about
            300 ms; +1 in silence), and its latest moments as (mid, side) pairs, oldest first, for the width's
            picture (a goniometer). */
        float getCorrelation() const noexcept       { return correlation.load (std::memory_order_relaxed); }
        static constexpr int stereoPoints = 512;
        void readStereo (float* mids, float* sides, int count) const noexcept;

        /** The most the limiter pulled down in the last block (dB): the master meter's. */
        float getBlockReduction() const noexcept    { return blockReduction; }

    private:
        /** An effect switched: the block as it came, and the effect's, one gliding into the other. */
        template <typename Work>
        void switchOver (float* left, float* right, int n, bool on, Work&& work) noexcept;

        void compress (float* left, float* right, int n) noexcept;
        void drive (float* left, float* right, int n, const Settings<numMasterParams>& settings, bool settle) noexcept;
        void limit (float* left, float* right, int n, const Settings<numMasterParams>& settings, bool settle) noexcept;
        void widen (float* left, float* right, int n, const Settings<numMasterParams>& settings, bool settle) noexcept;
        void watchStereo (const float* left, const float* right, int n) noexcept;

        double sampleRate = 48000.0;
        AnalogBoard::ChannelCompressor compressor;
        bool compressing = false, driving = false, widening = false, limiting = true;
        std::vector<float> dryLeft, dryRight;   // a block as it came, while an effect switches

        // The drive: how hard (the curve's reach), its tone, mix and output, as they were; its
        // tone's low band.
        float lastReach = 0.3f, lastLowGain = 1.0f, lastHighGain = 1.0f, lastMix = 1.0f, lastOutput = 1.0f;
        float toneCoefficient = 0.1f;
        std::array<float, 2> toneLow {};

        // The width: Width and Output as they were; the side's highpass (Bass Mono), its corner and
        // its state; the stereo watched (correlation sums; the goniometer's points, every 4th sample).
        float lastWidth = 1.0f, lastWidthOutput = 1.0f, bassCorner = 0.0f;
        float hb0 = 1.0f, hb1 = 0.0f, hb2 = 0.0f, ha1 = 0.0f, ha2 = 0.0f, hz1 = 0.0f, hz2 = 0.0f;
        double sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;
        std::atomic<float> correlation { 1.0f };
        std::array<std::atomic<float>, stereoPoints> pointMid {}, pointSide {};   // read on another thread (readStereo)
        std::atomic<int> pointAt { 0 };
        int pointSkip = 0;

        // The limiter.
        float lastGain = 1.0f, lastCeiling = 1.0f;
        float gain = 1.0f;                      // its own pull, now
        int holding = 0;                        // samples it still holds there
        float blockReduction = 0.0f;
        std::atomic<float> compressorMost { 0.0f }, driveMost { 0.0f }, limiterMost { 0.0f }, inputMost { -100.0f };
    };

    //==============================================================================
    /** The Live Show's mixer: its decks, the live channels, the pads, the master and its effects. */
    class Mixer
    {
    public:
        Mixer();

        Settings<numMasterParams> settings { masterSpecs() };
        std::array<Deck, maxDecks> decks;
        std::array<Pad, numPads> pads;
        Ducker ducker;                          // the effects channel's dynamic EQ, on the music
        Meter meter;                            // the master, after its fader and its effects

        /** The decks showing (2 or 4). */
        int getDeckCount() const noexcept      { return settings.get (deckCount) >= 0.5f ? 4 : 2; }

        /** Every setting where a new show starts (the decks' places on the crossfader too). */
        void resetSettings();

        void prepare (double sampleRate, int maxBlockSize);

        /** Starting afresh (switched on): every fade from silence, the effects empty. */
        void reset() noexcept;

        //==========================================================================
        // Audio thread, one block: begin, a channel at a time, then finish.

        void begin (int numSamples) noexcept;

        /** A live channel's sound (its track's block, after its plugins), through its fader and
            pan into the voices, and to the ducker's ear when it ducks. */
        void addChannel (Channel& channel, const float* left, const float* right, int numSamples) noexcept;

        /** The decks through the crossfader, ducked under the voices (the dynamic EQ), the voices,
            the pads, the master fader and the master's effects: into left and right (which it
            writes, not adds). And the headphones into theirs, when there are some (written too). */
        void finish (float* left, float* right, int numSamples,
                     float* phonesLeft = nullptr, float* phonesRight = nullptr) noexcept;

        /** The effects' meters since the last look: the compressor's, the drive's and the limiter's
            pull (dB), and the loudest going into the limiter (dBFS). Any thread. */
        float takeCompressorReduction() noexcept      { return effects.takeCompressorReduction(); }
        float takeDriveSaturation() noexcept          { return effects.takeDriveSaturation(); }
        float takeLimiterReduction() noexcept         { return effects.takeLimiterReduction(); }
        float takeLimiterInput() noexcept             { return effects.takeInputPeak(); }
        float getStereoCorrelation() const noexcept   { return effects.getCorrelation(); }
        void readStereo (float* mids, float* sides, int count) const noexcept   { effects.readStereo (mids, sides, count); }

        /** The headphones' meter (after their level and limiter). */
        Meter phonesMeter;

        /** The night's recording (the master, as it's heard), under the engine's graph lock; and
            how long it has been recording (samples, any thread). */
        void setRecorder (Recorder* newRecorder) noexcept     { recorder = newRecorder; recordedSamples.store (0); }
        juce::int64 getRecordedSamples() const noexcept       { return recordedSamples.load (std::memory_order_relaxed); }

        /** The live stream's sound (the master, as it's heard), under the engine's graph lock. */
        void setTap (SoundTap* newTap) noexcept                { tap = newTap; }

        /** Mute mics: the live channels go silent (the room, the recording, the stream and their
            stems; they still show on their meters, and in the headphones when cued, and no longer
            duck the music). Mute playback: the decks and the pads go silent the same way (a deck
            cued still plays in the headphones). Any thread; each fades. */
        void setMicsMuted (bool shouldMute) noexcept           { micsMuted.store (shouldMute); }
        bool areMicsMuted() const noexcept                     { return micsMuted.load (std::memory_order_relaxed); }
        void setPlaybackMuted (bool shouldMute) noexcept       { playbackMuted.store (shouldMute); }
        bool isPlaybackMuted() const noexcept                  { return playbackMuted.load (std::memory_order_relaxed); }

        /** Live streaming: the show's sound goes to the stream only while it's enabled (else
            silence; the room and the recording hear it either way). Any thread; fades. */
        void setStreamOpen (bool open) noexcept                { streamOpen.store (open); }
        bool isStreamOpen() const noexcept                     { return streamOpen.load (std::memory_order_relaxed); }

        /** The playlist's tabs: decks 1 and 2 are one player. Deck 2 plays through deck 1's channel
            (its Gain, fader and headphones), and the crossfader leaves both alone (their own places
            on it stay, for DJ show). Any thread. */
        void setOnePlayer (bool one) noexcept                  { onePlayer.store (one); }
        bool isOnePlayer() const noexcept                      { return onePlayer.load (std::memory_order_relaxed); }

    private:
        /** A deck's share of the crossfader: A, Thru or B, where the crossfader is, along its curve. */
        float crossfaderGain (int assign) const noexcept;
        void playPads (float* left, float* right, int numSamples) noexcept;

        /** The headphones: what's cued against the show, at their level, limited. */
        void playPhones (const float* left, const float* right, float* phonesLeft, float* phonesRight, int numSamples) noexcept;

        double sampleRate = 48000.0;
        juce::AudioBuffer<float> music, voices, padBus, cueBus, scratch;
        std::vector<float> side;
        std::array<float, maxDecks> lastCross {};
        float lastMaster = 0.0f, lastPads = 0.0f, lastPhones = 0.0f, lastCueShare = 0.0f, lastShowShare = 1.0f;
        float lastPlayback = 1.0f;              // the decks and pads: 0 muted .. 1 heard
        float lastStream = 0.0f;                // the stream's sound: 0 shut .. 1 open
        std::atomic<bool> micsMuted { false }, playbackMuted { false }, streamOpen { false };
        std::atomic<bool> onePlayer { false };
        bool settle = true, anyCued = false;
        int blockSamples = 0;
        MasterEffects effects;
        SonifyDsp::Limiter phonesLimiter;
        Recorder* recorder = nullptr;
        std::atomic<juce::int64> recordedSamples { 0 };
        SoundTap* tap = nullptr;
    };
}
