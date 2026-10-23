// D:\workspace\Fanan_web_hub\src\Choir.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <memory>
#include <vector>

struct AudioPlaylist;
class Transport;

//==============================================================================
/**
    A choir track's settings: the voices made from its vocal, as its Choir window sets them.

    Each voice sings the track's vocal moved to its part:
      scale       steps of the song's key from the note sung (2: a third above in the key)
      chord       chord notes from it (1: the next chord note above), the chords played by
                  another track (chordTrack); without chords there, thirds of the key
      semitones   a fixed interval
    then by whole octaves. A voice is a man's or a woman's: from a man's lead a woman's voice has
    its formants (the resonances that tell voices apart) raised, and the other way round; its
    character moves them a little more, darker or brighter. Humanize makes the voices people: each
    sings a little late or early (timing), a little off and drifting (pitch), with a vibrato of
    its own. Spread places them across the stereo; the hall puts them in a room.
*/
struct ChoirSettings
{
    static constexpr int maxVoices = 16;

    enum class Mode { scale, chord, semitones };

    struct Voice
    {
        bool female = false;
        float character = 0.0f;       // -1 darker .. +1 brighter
        Mode mode = Mode::scale;
        int interval = 0;             // scale: -7..7 steps; chord: -4..4 chord notes; semitones: -24..24
        int octave = 0;               // -2..2
        float levelDb = 0.0f;         // -36..6
        float pan = 0.0f;             // -1 left .. +1 right
        bool mute = false, solo = false;

        bool operator== (const Voice& other) const noexcept;
    };

    bool leadFemale = false;          // the vocal on the track: a man's or a woman's
    float leadDb = 0.0f;              // the vocal itself in the mix; at or below leadOff: not heard
    float choirDb = 0.0f;             // the voices together
    float spread = 0.8f;              // 0..1: how far the voices' pans reach
    float timing = 0.5f;              // humanize, 0..1
    float pitch = 0.4f;
    float vibrato = 0.2f;
    float hall = 0.25f;               // 0..1
    int chordTrack = 0;               // the instrument track playing the chords (0: none)
    int count = 0;                    // voices in use
    std::array<Voice, maxVoices> voices {};

    static constexpr float leadOff = -60.0f;

    /** The page's and the song file's form: { leadFemale, lead, choir, spread, timing, pitch,
        vibrato, hall, chordTrack, voices: [{ female, character, mode ("scale" | "chord" |
        "semitones"), interval, octave, level, pan, mute, solo }] }. */
    juce::var toVar() const;

    /** Read back, each value kept in its range; what's missing takes its default. */
    static ChoirSettings fromVar (const juce::var& value);

    /** A new choir track's: four voices around a man's lead (two women an octave up, two men). */
    static ChoirSettings makeDefault();

    bool operator== (const ChoirSettings& other) const noexcept;
    bool operator!= (const ChoirSettings& other) const noexcept  { return ! operator== (other); }
};

//==============================================================================
/**
    The voices of a choir track, made live from its audio clips as the song plays.

    Each voice is a pitch and formant shifter (Signalsmith Stretch, MIT) following the vocal. A
    shifter needs the sound ahead of the moment it plays (about 150 ms), and the clips are all in
    memory, so the choir reads them ahead of the song: what the voices sing lines up with the
    vocal exactly. Where the song jumps (play from elsewhere, a loop going round) the reading
    follows the loop ahead of time, and a jump it didn't see coming starts the voices again there,
    a few at a time so a block never takes too long (with many voices they come in over the first
    tens of milliseconds).

    The note sung is followed (YIN pitch detection, on the vocal read ahead) for the scale and
    chord parts; the key comes from the song channel and the chords from another track's notes,
    both laid out in song samples (Timeline) by the engine whenever they change.

    Threads: the engine creates and prepares it on the message thread; settings changes are
    made ready there (prepareChange: the shifters a new voice needs) and applied under the
    engine's graph lock (apply), as is the timeline. process() runs on the audio thread, under
    that lock.
*/
class Choir final
{
public:
    struct VoiceDsp;

    /** The song's keys and the chords, in song samples, in order. */
    struct Timeline
    {
        struct Key
        {
            juce::int64 sample = 0;
            int root = 0;                     // 0 C .. 11 B
            juce::uint16 notes = 0x0ab5;      // the scale's notes from the root (TempoMap::scaleNotes)

            bool operator== (const Key& other) const noexcept   { return sample == other.sample && root == other.root && notes == other.notes; }
        };

        struct Chord
        {
            juce::int64 sample = 0;
            juce::uint16 notes = 0;           // pitch classes sounding (bit 0: C); 0: no chord here

            bool operator== (const Chord& other) const noexcept { return sample == other.sample && notes == other.notes; }
        };

        std::vector<Key> keys;
        std::vector<Chord> chords;

        bool operator== (const Timeline& other) const noexcept  { return keys == other.keys && chords == other.chords; }
    };

    /** A settings change made ready on the message thread. */
    struct Change
    {
        Change();
        ~Change();
        Change (Change&&) noexcept;
        Change& operator= (Change&&) noexcept;

        ChoirSettings settings;
        std::array<std::unique_ptr<VoiceDsp>, ChoirSettings::maxVoices> voices;   // new shifters, where the choir has none yet
    };

    Choir();
    ~Choir();

    /** No audio callback active. The voices made so far are set up again at this rate. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Message thread: the shifters the settings' voices need that the choir doesn't have yet. */
    Change prepareChange (const ChoirSettings& settings) const;

    /** Under the engine's graph lock (cheap: pointers and a copy). */
    void apply (Change& change) noexcept;

    const ChoirSettings& getSettings() const noexcept           { return settings; }

    /** Under the engine's graph lock; the old one comes back, to be let go outside it. */
    std::unique_ptr<const Timeline> swapTimeline (std::unique_ptr<const Timeline> fresh) noexcept;
    const Timeline* getTimeline() const noexcept                { return timeline.get(); }

    /** Audio thread. left and right hold the vocal as the track plays it (its clips): the lead's
        level goes onto it, and the voices are added, made from the playlist read ahead. */
    void process (const AudioPlaylist* list, const Transport& transport, float* left, float* right, int numSamples) noexcept;

    /** How far ahead the voices read, in samples (the shifters' latency). */
    int getLookahead() const noexcept                           { return lookahead; }

private:
    struct Span
    {
        juce::int64 index = 0;      // the first fed sample it holds (counted from the last start)
        juce::int64 song = 0;       // where that is in the song, in samples
        juce::int64 length = 0;
    };

    class PitchTracker;

    void start (juce::int64 songSample) noexcept;
    void startVoice (VoiceDsp& voice, int index) noexcept;
    void renderChunk (float* left, float* right, int numSamples) noexcept;
    void feed (float* dest, int numSamples) noexcept;
    void readFed (float* dest, juce::int64 fromIndex, int numSamples) const noexcept;
    juce::int64 songAt (juce::int64 index) const noexcept;
    juce::int64 spanLeft (juce::int64 index) const noexcept;
    void addSpan (juce::int64 song, juce::int64 length) noexcept;
    void dropSpansBefore (juce::int64 index) noexcept;
    void setVoiceParameters (VoiceDsp& voice, int index, float semitones) noexcept;
    float partSemitones (const ChoirSettings::Voice& voice, juce::int64 songSample) noexcept;

    static void readMono (const AudioPlaylist* list, juce::int64 from, int numSamples, float* dest) noexcept;

    ChoirSettings settings;
    std::array<std::unique_ptr<VoiceDsp>, ChoirSettings::maxVoices> voices;
    std::unique_ptr<const Timeline> timeline;
    std::unique_ptr<PitchTracker> tracker;
    juce::Reverb hall;
    juce::Reverb::Parameters hallParameters;

    double sampleRate = 0.0;
    int lookahead = 0;              // samples: what a shifter needs ahead (its input and output latency)
    int analysisDelay = 0;          // how far behind the newest sample a shifter's analysis sits

    static constexpr int maxChunk = 256;
    static constexpr int maxSpans = 64;
    static constexpr int startsPerBlock = 3;

    // Audio thread: the reading ahead.
    bool rolling = false;
    const AudioPlaylist* reading = nullptr;          // the playlist this block reads
    juce::int64 fedIndex = 0, outIndex = 0;          // samples fed to the shifters, and played, since the last start
    juce::int64 feedSong = 0;                        // where the next sample fed comes from in the song
    bool loopOn = false;
    juce::int64 loopStart = 0, loopEnd = 0;
    std::array<Span, maxSpans> spans {};
    int spanCount = 0;
    int startsLeft = 0;

    // The note sung, held while it wavers (and through breaths), and its pitch as last heard.
    float heldNote = -1.0f, sungPitch = -1.0f;

    std::vector<float> feedBuffer, startBuffer, busLeft, busRight;
    float leadGain = 1.0f, choirGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Choir)
};
