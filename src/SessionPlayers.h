// D:\workspace\Fanan_web_hub\src\SessionPlayers.h

#pragma once

#include "AudioEngine.h"
#include "Library.h"

#include <map>
#include <vector>

//==============================================================================
/**
    Sonify's session players (Track > Add Session Players, and Sonify AI): a drummer, a bass
    player and a keys player who play a chord progression in a style, each on a track of its
    own with a fitting installed instrument.

    The parts are made from patterns, not recordings: each style has its grooves at three
    intensities, played over the chords (roots, fifths and approach notes for the bass, voiced
    chords that move as little as they can for the keys), with fills at phrase ends, crashes at
    phrase starts, a little swing where the style has it, and human velocities and timing.
    Drums use General MIDI notes (36 kick, 38 snare, 42 hi-hat...), so any GM drum plugin plays them.

    Chords come written as text (bars between |, chords in a bar split it evenly) or read off the
    notes a track plays. Message thread only.
*/
namespace SessionPlayers
{
    enum class Player { drums, bass, keys };
    enum class Style { pop, rock, ballad, funk, house, hiphop };

    struct Chord
    {
        double start = 0.0, length = 0.0;     // song positions, quarter notes
        int root = -1;                        // pitch class 0..11; -1: no chord (N.C.)
        int bass = -1;                        // a slash chord's bass note (pitch class), or -1
        std::vector<int> intervals;           // from the root, in semitones
        juce::String name;
    };

    /** A chord symbol: C, Am, F#m7, Bbmaj7, G/B, Dsus4, E7, Cadd9, Bdim, N.C. False when it isn't one. */
    bool parseChord (const juce::String& symbol, Chord& chord);

    /** A progression as written, placed from startPpq: bars between | (chords in a bar split it
        evenly, on the beat), or, without any |, a bar for each chord. % repeats the chord before,
        N.C. (or x) is silence. Played `times` times. */
    juce::Result parseProgression (const juce::String& text, double startPpq, double quartersPerBar, int beatsPerBar,
                                   int times, std::vector<Chord>& chords);

    /** The chords a track plays, a bar at a time (halves when they change mid-bar), written as a
        progression to edit; firstBar (from 1) and bars say where. Empty when it plays no notes. */
    juce::String detectChords (const std::vector<MidiClip>& clips, const TempoMap& map, int& firstBar, int& bars);

    /** One player's part over the chords, from fromPpq to toPpq (song positions). */
    std::vector<MidiNote> generate (Player player, Style style, const std::vector<Chord>& chords,
                                    double fromPpq, double toPpq, int numerator, int denominator,
                                    double intensity, juce::uint32 seed);

    struct Request
    {
        Style style = Style::pop;
        double intensity = 0.5;                                   // 0 calm .. 1 busy
        std::vector<Player> players { Player::drums, Player::bass, Player::keys };

        // Each player's instrument: a library entry's file and id. None given: the best fit
        // installed; file "none": the Test Synth.
        std::map<Player, std::pair<juce::String, juce::String>> instruments;

        juce::String chords;
        int startBar = 1;
        int times = 1;
        juce::uint32 seed = 0;
    };

    /** A track for each player, with its instrument and its part as one clip. trackIds: the new
        tracks; notes: anything worth telling (an instrument that wouldn't load). */
    juce::Result create (AudioEngine& engine, Library& library, const Request& request,
                         std::vector<int>& trackIds, juce::StringArray& notes);

    /** The installed instrument that fits a player best (by type, name and features), or null. */
    const Library::Entry* bestInstrument (const Library& library, Player player);

    Style styleFrom (const juce::String& name);
    juce::String nameOf (Style style);
    bool playerFrom (const juce::String& name, Player& player);
    juce::String nameOf (Player player);              // "drums", "bass", "keys"
    juce::String trackNameOf (Player player);         // "Drums", "Bass", "Keys"
}
