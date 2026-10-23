// D:\workspace\Fanan_web_hub\src\TempoMap.h

#pragma once

#include <juce_core/juce_core.h>

#include <vector>

//==============================================================================
/**
    The song channel's map of the song in time, as Studio One's global tracks keep it:
        Tempo        changes at song positions (quarter notes); each holds until the next, or
                     (ramp) glides there in a straight line
        Signature    time signature changes, each from the start of a bar
        Key          key and scale changes, each from the start of a bar (MIDI tools follow them)
        Chords       the chord track: chords at song positions, each holding until the next (none
                     before the first); a chord is a root, a quality ("m7", see getChordQualities)
                     and maybe a bass note of its own (C/E)
    There is always one of each at the start of the song (but the chords: there may be none).

    It turns song positions (quarter notes) into seconds and back, says the tempo anywhere, and
    finds the bars: where each starts, how long it is and how it's counted. Bars are counted from
    0 here; the page shows them from 1.

    A value: copied freely, compared, and kept in songs as plain data (toVar, fromVar).
*/
class TempoMap final
{
public:
    struct Tempo
    {
        double ppq = 0.0;
        double bpm = 120.0;
        bool ramp = false;                  // glides to the next change's tempo

        bool operator== (const Tempo& other) const noexcept;
    };

    struct Signature
    {
        int bar = 0;
        int numerator = 4, denominator = 4;

        bool operator== (const Signature& other) const noexcept;
    };

    struct Key
    {
        int bar = 0;
        int root = 0;                       // 0 C .. 11 B
        juce::String scale { "major" };     // see getScales()

        bool operator== (const Key& other) const noexcept;
    };

    struct Chord
    {
        double ppq = 0.0;
        int root = 0;                       // 0 C .. 11 B
        juce::String quality;               // "" major, "m", "7", "maj7"... (getChordQualities)
        int bass = -1;                      // a bass note of its own (0..11), or -1: the root

        bool operator== (const Chord& other) const noexcept;
    };

    /** A bar: where it starts (quarter notes), and its time signature. */
    struct Bar
    {
        int index = 0;
        double start = 0.0;
        int numerator = 4, denominator = 4;

        double beat() const noexcept        { return 4.0 / denominator; }
        double length() const noexcept      { return numerator * beat(); }
    };

    static constexpr double minTempo = 20.0;
    static constexpr double maxTempo = 250.0;
    static constexpr int maxBar = 100000;

    /** 120 BPM, 4/4 and C major all through. */
    TempoMap();

    //==============================================================================
    /** Each list is tidied: in order, one per position, values in range, and the first at the
        start of the song (the earliest one moves there when none is). */
    void setTempos (std::vector<Tempo> list);
    void setSignatures (std::vector<Signature> list);
    void setKeys (std::vector<Key> list);
    void setChords (std::vector<Chord> list);

    const std::vector<Tempo>& getTempos() const noexcept            { return tempos; }
    const std::vector<Signature>& getSignatures() const noexcept    { return signatures; }
    const std::vector<Key>& getKeys() const noexcept                { return keys; }
    const std::vector<Chord>& getChords() const noexcept            { return chords; }

    /** One tempo all through, no ramps. */
    bool hasOneTempo() const noexcept                               { return tempos.size() == 1; }

    //==============================================================================
    double secondsAt (double ppq) const noexcept;
    double ppqAt (double seconds) const noexcept;
    double bpmAt (double ppq) const noexcept;

    Bar barAt (double ppq) const noexcept;
    double ppqOfBar (int index) const noexcept;
    Signature signatureAt (double ppq) const noexcept;
    Key keyAt (double ppq) const noexcept;

    /** The chord that holds at a position (null: no chord before it). Allocates nothing. */
    const Chord* chordAt (double ppq) const noexcept;

    /** The nearest bar line to a position. */
    double nearestBar (double ppq) const noexcept;

    /** A position moved toward the nearest point of a quantize grid (quarter notes), laid from
        the start of the bar it's in: a grid of a bar or more is the nearest bar line. swing 0..1
        moves every second grid point later by up to a third of the grid (1: a triplet shuffle);
        strength 0..1 is how far the position moves toward it. */
    double quantized (double ppq, double grid, double swing = 0.0, double strength = 1.0) const noexcept;

    //==============================================================================
    /** The tempo change that holds at this position set to bpm (with one tempo: the song's). */
    TempoMap withTempoAt (double ppq, double bpm) const;

    /** The signature of the bar this position is in, from that signature's change on. */
    TempoMap withSignatureAt (double ppq, int numerator, int denominator) const;

    /** One tempo all through (signatures and keys kept). */
    TempoMap withOneTempo (double bpm) const;

    //==============================================================================
    /** {tempos: [[ppq, bpm, ramp]], signatures: [[bar, numerator, denominator]], keys: [[bar, root, scale]],
        chords: [[ppq, root, quality, bass]]} (chords only when there are any) */
    juce::var toVar() const;

    /** A map from songs (or the page). Lists missing or bad: the defaults for them. */
    static TempoMap fromVar (const juce::var& value);

    static bool isValidDenominator (int denominator) noexcept;
    static const juce::StringArray& getScales();

    /** A scale's notes above its root, one bit per semitone (bit 0: the root); major for a name
        it doesn't know. Allocates nothing: the audio thread asks it too. */
    static juce::uint16 scaleNotes (const juce::String& scale) noexcept;
    static juce::String keyName (const Key& key);       // "A minor"

    /** The chord qualities the chord track knows, "" (major) first. */
    static const juce::StringArray& getChordQualities();

    /** A chord quality's notes above its root, one bit per semitone (bit 0: the root); major for
        one it doesn't know. Allocates nothing. */
    static juce::uint16 chordNotes (const juce::String& quality) noexcept;
    static juce::String chordName (const Chord& chord);  // "Am7", "C/E"

    bool operator== (const TempoMap& other) const noexcept;
    bool operator!= (const TempoMap& other) const noexcept     { return ! operator== (other); }

private:
    void rebuild();

    /** The tempo in effect at the start of tempos[i] and where its stretch ends. */
    double secondsIn (size_t index, double ppq) const noexcept;

    std::vector<Tempo> tempos;
    std::vector<double> tempoSeconds;       // seconds at each tempo change
    std::vector<Signature> signatures;
    std::vector<double> signaturePpq;       // where each signature's first bar starts
    std::vector<Key> keys;
    std::vector<Chord> chords;
};
