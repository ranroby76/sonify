// D:\workspace\Fanan_web_hub\src\SessionPlayers.cpp

#include "SessionPlayers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace
{
    using namespace SessionPlayers;

    //==============================================================================
    // Chords
    const std::array<const char*, 12> noteNames { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    struct Quality
    {
        std::vector<const char*> spellings;   // the first is how Sonify writes it
        std::vector<int> intervals;
    };

    const std::vector<Quality>& qualities()
    {
        static const std::vector<Quality> list {
            { { "", "maj", "M", "major" },                          { 0, 4, 7 } },
            { { "m", "min", "-", "minor", "mi" },                   { 0, 3, 7 } },
            { { "7", "dom7", "dom" },                               { 0, 4, 7, 10 } },
            { { "maj7", "M7", "ma7", "^7", "^" },                   { 0, 4, 7, 11 } },
            { { "m7", "min7", "-7", "mi7" },                        { 0, 3, 7, 10 } },
            { { "mMaj7", "mM7", "mmaj7", "m(maj7)", "minmaj7" },    { 0, 3, 7, 11 } },
            { { "dim", "o" },                                       { 0, 3, 6 } },
            { { "dim7", "o7" },                                     { 0, 3, 6, 9 } },
            { { "m7b5", "min7b5", "-7b5", "halfdim" },              { 0, 3, 6, 10 } },
            { { "aug", "+", "#5" },                                 { 0, 4, 8 } },
            { { "sus2" },                                           { 0, 2, 7 } },
            { { "sus4", "sus" },                                    { 0, 5, 7 } },
            { { "7sus4", "7sus" },                                  { 0, 5, 7, 10 } },
            { { "6", "maj6" },                                      { 0, 4, 7, 9 } },
            { { "m6", "min6" },                                     { 0, 3, 7, 9 } },
            { { "9", "dom9" },                                      { 0, 4, 7, 10, 14 } },
            { { "maj9", "M9" },                                     { 0, 4, 7, 11, 14 } },
            { { "m9", "min9" },                                     { 0, 3, 7, 10, 14 } },
            { { "add9", "add2" },                                   { 0, 4, 7, 14 } },
            { { "madd9", "m(add9)", "minadd9" },                    { 0, 3, 7, 14 } },
            { { "11" },                                             { 0, 4, 7, 10, 14, 17 } },
            { { "m11", "min11" },                                   { 0, 3, 7, 10, 14, 17 } },
            { { "13" },                                             { 0, 4, 7, 10, 14, 21 } },
            { { "5" },                                              { 0, 7 } }
        };

        return list;
    }

    /** A note name (C, F#, Bb, eb) at the start of text: its pitch class, and how many characters it took. */
    int readNote (const juce::String& text, int& used)
    {
        used = 0;

        if (text.isEmpty())
            return -1;

        static const std::array<int, 7> letters { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        const auto letter = juce::CharacterFunctions::toUpperCase (text[0]);

        if (letter < 'A' || letter > 'G')
            return -1;

        auto pc = letters[static_cast<size_t> (letter - 'A')];
        used = 1;

        while (used < text.length() && (text[used] == '#' || text[used] == 'b'))
        {
            pc += text[used] == '#' ? 1 : -1;
            ++used;
        }

        return (pc % 12 + 12) % 12;
    }

    bool sameChord (const Chord& a, const Chord& b)
    {
        return a.root == b.root && a.bass == b.bass && a.intervals == b.intervals;
    }

    juce::String chordName (int root, const Quality& quality, int bass)
    {
        juce::String name (noteNames[static_cast<size_t> (root)]);
        name << quality.spellings.front();

        if (bass >= 0 && bass != root)
            name << "/" << noteNames[static_cast<size_t> (bass)];

        return name;
    }

    //==============================================================================
    // Patterns. A bar is a row of steps (sixteenths in 4/4); each player reads its own symbols.
    //   drums   X accent, x hit, g ghost, . nothing (a row per drum, General MIDI notes)
    //   bass    R root, r short root, 5 fifth, O octave, 3 third, b seventh, A a step into the
    //           next chord, - holds, . rests
    //   keys    C chord, c soft chord, B chord with the left hand's root, b left hand alone,
    //           a the next note of a broken chord, - holds, . rests
    enum class Tier { low, mid, high };

    constexpr int kick = 36, rim = 37, snare = 38, clap = 39, floorTom = 43, closedHat = 42, lowTom = 45,
                  openHat = 46, midTom = 47, crash = 49, highTom = 50, ride = 51;

    struct Pattern
    {
        std::vector<std::pair<int, std::string>> drums;
        std::string bass, keys;
    };

    struct Feel
    {
        double swing = 0.5;              // where the second sixteenth of each eighth falls (0.5: straight)
        bool staccato = false;           // short bass and keys (funk, house)
        bool electronic = false;         // drums on the grid
        int phraseBars = 4;              // fills before, crashes at, each phrase
    };

    Feel feelOf (Style style)
    {
        switch (style)
        {
            case Style::funk:   return { 0.54, true, false, 4 };
            case Style::house:  return { 0.5, true, true, 8 };
            case Style::hiphop: return { 0.58, false, true, 4 };
            case Style::ballad: return { 0.5, false, false, 8 };
            case Style::rock:
            case Style::pop:    break;
        }

        return {};
    }

    Pattern pattern44 (Style style, Tier tier)
    {
        const auto t = [tier] (const char* low, const char* mid, const char* high)
        {
            return std::string (tier == Tier::low ? low : tier == Tier::mid ? mid : high);
        };

        switch (style)
        {
            case Style::rock:
                return { { { kick,      t ("X.......X.......", "X.......X.X.....", "X.x.....X.X.....") },
                           { snare,     t ("....X.......X...", "....X.......X...", "....X.......X...") },
                           { closedHat, t ("x.x.x.x.x.x.x.x.", "X.x.X.x.X.x.X.x.", "................") },
                           { ride,      t ("................", "................", "X.x.X.x.X.x.X.x.") } },
                         t ("R---R---R---R---", "R-R-R-R-R-R-R-R-", "R-RRR-RRR-RRR-5A"),
                         t ("B-------C-------", "B---C---C---C---", "B.C.C.C.C.C.C.C.") };

            case Style::ballad:
                return { { { kick,      t ("X...............", "X.........x.....", "X.........x.....") },
                           { rim,       t ("....x.......x...", "....x.......x...", "................") },
                           { snare,     t ("................", "................", "....X.......X...") },
                           { closedHat, t ("x...x...x...x...", "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x.") } },
                         t ("R---------------", "R-------5-------", "R-----5-R-----3-"),
                         t ("B---------------", "b-a-a-a-b-a-a-a-", "b-a-a-a-a-a-a-a-") };

            case Style::funk:
                return { { { kick,      t ("X..x......X.....", "X..x..x...X..x..", "X..x..x.x.X..x..") },
                           { snare,     t ("....X.......X...", "....X..g.g..X..g", "..g.X..g.g.gX.g.") },
                           { closedHat, t ("x.x.x.x.x.x.x.x.", "XxXxXxXxXxXxXxXx", "XxXxXxXxXxXxXxXx") },
                           { openHat,   t ("................", "................", "......x.........") } },
                         t ("R..R......R..5..", "R.rR..R.O.R..5.A", "R.rRr.R.O.rR.5rA"),
                         t ("..C...C...C...C.", "..c.C..c..C.c...", "c.cC.cC.c.cC.cC.") };

            case Style::house:
                return { { { kick,      t ("X...X...X...X...", "X...X...X...X...", "X...X...X...X...") },
                           { clap,      t ("................", "....X.......X...", "....X.......X...") },
                           { closedHat, t ("..x...x...x...x.", "................", ".x.x.x.x.x.x.x.x") },
                           { openHat,   t ("................", "..x...x...x...x.", "..x...x...x...x.") } },
                         t ("..R...R...R...R.", "..R-..R-..R-..R-", "R.R-..R-.RR-..RA"),
                         t ("..C...C...C...C.", "..C...C...C...C.", "..C.C.C...C.C.C.") };

            case Style::hiphop:
                return { { { kick,      t ("X.........X.....", "X......x..X.....", "X......x.xX....x") },
                           { snare,     t ("....X.......X...", "....X.......X...", "....X.......X...") },
                           { closedHat, t ("x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x.", "x.xxx.x.x.xxx.x.") } },
                         t ("R-------..R-----", "R-----.R..R---5-", "R---.R.R..R-.R5A"),
                         t ("C---------------", "C-------..C-----", "C---..C-..C-..C-") };

            case Style::pop:
                break;
        }

        return { { { kick,      t ("X.......x.......", "X.....x.X.......", "X.....x.X.x.....") },
                   { snare,     t ("....x.......x...", "....X.......X...", "....X.......X..g") },
                   { closedHat, t ("x...x...x...x...", "x.x.x.x.x.x.x.x.", "XxxxXxxxXxxxXxxx") },
                   { openHat,   t ("................", "................", "..............x.") } },
                 t ("R-------R-------", "R---R-.RR---5-.A", "R-.RR-.RR-.R5-.A"),
                 t ("B-------C-------", "B-.C..C.C-.C..C.", "B.c.C.c.C.c.C.c.") };
    }

    /** 3/4: a waltz, whatever the style. */
    Pattern pattern34 (Tier tier)
    {
        const auto t = [tier] (const char* low, const char* mid, const char* high)
        {
            return std::string (tier == Tier::low ? low : tier == Tier::mid ? mid : high);
        };

        return { { { kick,      t ("X...........", "X...........", "X.......x...") },
                   { rim,       t ("....x...x...", "....x...x...", "............") },
                   { snare,     t ("............", "............", "....X...X...") },
                   { closedHat, t ("x...x...x...", "x.x.x.x.x.x.", "x.x.x.x.x.x.") } },
                 t ("R-----------", "R---5---5---", "R---5---3---"),
                 t ("B-----------", "b---C---C---", "b-a-a-a-a-a-") };
    }

    /** 6/8 (and 12/8, twice over): two lilting groups of three. */
    Pattern pattern68 (Tier tier)
    {
        const auto t = [tier] (const char* low, const char* mid, const char* high)
        {
            return std::string (tier == Tier::low ? low : tier == Tier::mid ? mid : high);
        };

        return { { { kick,      t ("X...........", "X...........", "X...x.X.....") },
                   { rim,       t ("......x.....", "............", "............") },
                   { snare,     t ("............", "......X.....", "......X.....") },
                   { closedHat, t ("x.x.x.x.x.x.", "x.x.x.x.x.x.", "x.x.x.x.x.x.") } },
                 t ("R-----------", "R-----5-----", "R---5-R---5-"),
                 t ("B-----------", "b.a.a.b.a.a.", "b.a.a.a.a.a.") };
    }

    /** Any other metre: the downbeat, a backbeat on the even beats, eighths on the hi-hat. */
    Pattern patternAny (int steps, int numerator, int denominator, Tier tier)
    {
        const auto beatSteps = denominator >= 8 ? 2 : 4;
        const auto beats = juce::jmax (1, steps / beatSteps);
        std::string kicks (static_cast<size_t> (steps), '.'), snares = kicks, hats = kicks, bass = kicks, keys = kicks;

        kicks[0] = 'X';
        hats[0] = 'x';
        bass[0] = 'R';
        keys[0] = tier == Tier::low ? 'B' : 'b';

        for (int beat = 1; beat < beats; ++beat)
        {
            const auto at = static_cast<size_t> (beat * beatSteps);

            if (beat % 2 == 1)
                snares[at] = tier == Tier::low ? 'x' : 'X';
            else if (tier == Tier::high)
                kicks[at] = 'x';

            if (tier != Tier::low)
                keys[at] = 'C';
        }

        for (int step = 0; step < steps; step += 2)
            hats[static_cast<size_t> (step)] = 'x';

        for (int step = 1; step < steps; ++step)
        {
            if (bass[static_cast<size_t> (step)] == '.')
                bass[static_cast<size_t> (step)] = '-';

            if (keys[static_cast<size_t> (step)] == '.')
                keys[static_cast<size_t> (step)] = '-';
        }

        if (tier != Tier::low && beats >= 3)
            bass[static_cast<size_t> ((beats / 2) * beatSteps)] = '5';

        juce::ignoreUnused (numerator);
        return { { { kick, kicks }, { snare, snares }, { closedHat, hats } }, bass, keys };
    }

    //==============================================================================
    // Playing
    class Performer
    {
    public:
        Performer (Player p, Style s, const std::vector<Chord>& c, double from, double to, int numerator, int denominator,
                   double intensityIn, juce::uint32 seed)
            : player (p), style (s), chords (c), fromPpq (from), toPpq (to), intensity (juce::jlimit (0.0, 1.0, intensityIn)),
              tier (intensity < 0.34 ? Tier::low : intensity < 0.67 ? Tier::mid : Tier::high),
              feel (feelOf (s)), random (seed)
        {
            quartersPerBar = numerator * 4.0 / juce::jmax (1, denominator);
            steps = juce::jmax (1, static_cast<int> (std::lround (quartersPerBar / 0.25)));
            stepLength = quartersPerBar / steps;

            if (numerator == 4 && denominator == 4)
                pattern = pattern44 (style, tier);
            else if (numerator == 3 && denominator == 4)
                pattern = pattern34 (tier);
            else if ((numerator == 6 || numerator == 12) && denominator == 8)
                pattern = pattern68 (tier);
            else
                pattern = patternAny (steps, numerator, denominator, tier);

            // Louder as it gets busier.
            levelShift = tier == Tier::low ? -10 : tier == Tier::high ? 6 : 0;
        }

        std::vector<MidiNote> play()
        {
            const auto bars = juce::jmax (1, static_cast<int> (std::ceil ((toPpq - fromPpq) / quartersPerBar - 1.0e-9)));

            for (int bar = 0; bar < bars; ++bar)
            {
                const auto barStart = fromPpq + bar * quartersPerBar;

                if (player == Player::drums)
                    playDrums (bar, bars, barStart);
                else if (player == Player::bass)
                    playBass (barStart);
                else
                    playKeys (barStart);
            }

            finishNotes (toPpq);

            // Nothing past the end, nothing before the start.
            notes.erase (std::remove_if (notes.begin(), notes.end(), [this] (const MidiNote& n) { return n.start >= toPpq - 1.0e-6; }), notes.end());

            for (auto& n : notes)
            {
                n.start = juce::jmax (fromPpq, n.start);
                n.length = juce::jmax (1.0 / 64.0, juce::jmin (n.length, toPpq - n.start));
            }

            std::sort (notes.begin(), notes.end(), [] (const MidiNote& a, const MidiNote& b) { return a.start < b.start || (a.start == b.start && a.pitch < b.pitch); });
            return notes;
        }

    private:
        //==============================================================================
        char symbolAt (const std::string& row, int step) const
        {
            return row.empty() ? '.' : row[static_cast<size_t> (step % static_cast<int> (row.size()))];
        }

        double timeOf (double barStart, int step)
        {
            auto time = barStart + step * stepLength;

            // Swing: the second sixteenth of each eighth comes late.
            if (step % 2 == 1 && feel.swing > 0.5)
                time += (feel.swing - 0.5) * 2.0 * stepLength;

            // Human timing, except where the style lives on the grid.
            const auto spread = player == Player::drums ? (feel.electronic ? 0.0 : 0.004) : 0.006;
            return time + spread * unit (random);
        }

        int velocityOf (int base, int spread)
        {
            return juce::jlimit (1, 127, juce::roundToInt (base + levelShift + spread * unit (random)));
        }

        void addNote (int pitch, double start, double length, int velocity)
        {
            if (pitch < 0 || pitch > 127)
                return;

            MidiNote note;
            note.pitch = pitch;
            note.start = start;
            note.length = juce::jmax (1.0 / 64.0, length);
            note.velocity = static_cast<float> (velocity) / 127.0f;
            notes.push_back (note);
        }

        const Chord* chordAt (double ppq) const
        {
            for (const auto& chord : chords)
                if (ppq >= chord.start - 1.0e-6 && ppq < chord.start + chord.length - 1.0e-6)
                    return &chord;

            return nullptr;
        }

        /** The first chord that starts after ppq (for a step into it). */
        const Chord* chordAfter (double ppq) const
        {
            for (const auto& chord : chords)
                if (chord.start > ppq + 1.0e-6)
                    return &chord;

            return nullptr;
        }

        //==============================================================================
        void playDrums (int bar, int bars, double barStart)
        {
            const auto phrase = juce::jmax (1, feel.phraseBars);
            const bool fillBar = tier != Tier::low && bar % phrase == phrase - 1 && bar < bars - 1;
            const auto fillSteps = ! fillBar ? 0 : juce::jmin (steps / 2, tier == Tier::high ? 8 : 4);
            const auto fillFrom = steps - fillSteps;

            // A crash where each phrase starts (just the first, when it's calm).
            if (bar == 0 || (tier != Tier::low && bar % phrase == 0))
                addNote (crash, timeOf (barStart, 0), 0.5, velocityOf (108, 6));

            for (const auto& [note, row] : pattern.drums)
            {
                for (int step = 0; step < steps; ++step)
                {
                    const auto symbol = symbolAt (row, step);

                    if (symbol == '.' || symbol == '-')
                        continue;

                    // The fill takes over the end of the bar (the kick on its first step stays).
                    if (fillSteps > 0 && step >= fillFrom && ! (note == kick && step == fillFrom))
                        continue;

                    const auto base = symbol == 'X' ? 112 : symbol == 'g' ? 44 : 90;
                    addNote (note, timeOf (barStart, step), 0.1, velocityOf (base, symbol == 'g' ? 4 : 7));
                }
            }

            if (fillSteps <= 0)
                return;

            // A roll on the snare (a clap in house), or snare into the toms.
            const bool roll = style == Style::house || style == Style::hiphop;
            static const std::array<int, 8> toms { snare, snare, snare, snare, highTom, midTom, lowTom, floorTom };

            for (int i = 0; i < fillSteps; ++i)
            {
                const auto step = fillFrom + i;
                const auto progress = static_cast<double> (i + 1) / fillSteps;
                const auto drum = roll ? (style == Style::house ? clap : snare)
                                       : toms[static_cast<size_t> (8 - fillSteps + i)];
                addNote (drum, timeOf (barStart, step), 0.1, velocityOf (juce::roundToInt (70 + 40 * progress), 5));
            }
        }

        //==============================================================================
        static int bassPitchOf (int pitchClass)
        {
            return 28 + ((pitchClass - 4) % 12 + 12) % 12;   // E1 .. D#2
        }

        static bool has (const Chord& chord, int interval)
        {
            return std::find (chord.intervals.begin(), chord.intervals.end(), interval) != chord.intervals.end();
        }

        int bassPitch (const Chord& chord, char symbol, double at) const
        {
            const auto root = bassPitchOf (chord.root);
            const auto bottom = bassPitchOf (chord.bass >= 0 ? chord.bass : chord.root);

            switch (symbol)
            {
                case 'O': return bottom + 12;
                case '5': return has (chord, 7) ? (root + 7 > 43 ? root - 5 : root + 7) : (has (chord, 6) ? root + 6 : (has (chord, 8) ? root + 8 : root + 7));
                case '3': return root + (has (chord, 3) ? 3 : has (chord, 4) ? 4 : has (chord, 5) ? 5 : has (chord, 2) ? 2 : 7);
                case 'b': return root + (has (chord, 10) ? 10 : has (chord, 11) ? 11 : has (chord, 9) ? 9 : 12);
                case 'A':
                {
                    // A half step into the next chord's bass, from below (from above at the bottom).
                    const auto* next = chordAfter (at);

                    if (next == nullptr || next->root < 0 || sameChord (*next, chord))
                        return bassPitch (chord, '5', at);

                    const auto target = bassPitchOf (next->bass >= 0 ? next->bass : next->root);
                    return target - 1 >= 28 ? target - 1 : target + 1;
                }
                default:  return bottom;
            }
        }

        void playBass (double barStart)
        {
            const auto& row = pattern.bass;
            char lastSymbol = 'R';

            for (int step = 0; step < steps; ++step)
            {
                const auto symbol = symbolAt (row, step);
                const auto at = barStart + step * stepLength;
                const auto* chord = chordAt (at);

                if (symbol == '.')
                {
                    finishNotes (at);
                    continue;
                }

                // Held through a chord change: the next chord's note, softer.
                const bool changed = chord != nullptr && current != nullptr && ! sameChord (*chord, *current);
                const auto play = symbol != '-' ? symbol : (changed ? (lastSymbol == 'A' ? 'R' : lastSymbol) : '-');

                if (play == '-')
                    continue;

                finishNotes (at);
                current = chord;

                if (chord == nullptr || chord->root < 0)
                    continue;   // no chord: the bass rests

                const auto base = play == 'R' ? 104 : play == 'r' ? 70 : play == 'O' ? 96 : 88;
                const auto velocity = velocityOf (symbol == '-' ? base - 14 : base, 5);
                startNote (bassPitch (*chord, play, at), timeOf (barStart, step), velocity, play == 'r' || feel.staccato);
                lastSymbol = play;
            }
        }

        //==============================================================================
        /** The chord's tones in the keys' middle register, as close as can be to the last ones. */
        std::vector<int> voicing (const Chord& chord)
        {
            std::vector<int> tones;

            for (const auto interval : chord.intervals)
            {
                const auto pc = (chord.root + interval) % 12;

                if (std::find (tones.begin(), tones.end(), pc) == tones.end())
                    tones.push_back (pc);
            }

            // At most four tones: the fifth goes first, then the extensions beyond.
            if (tones.size() > 4)
            {
                const auto fifth = (chord.root + 7) % 12;
                tones.erase (std::remove (tones.begin(), tones.end(), fifth), tones.end());
            }

            while (tones.size() > 4)
                tones.pop_back();

            std::vector<int> best;
            auto bestCost = 1.0e9;

            for (size_t rotation = 0; rotation < tones.size(); ++rotation)
            {
                std::vector<int> notesUp;
                auto pitch = 55 + ((tones[rotation] - 55) % 12 + 12) % 12;   // the lowest at G3 or above

                for (size_t i = 0; i < tones.size(); ++i)
                {
                    const auto pc = tones[(rotation + i) % tones.size()];
                    while (pitch % 12 != pc)
                        ++pitch;

                    notesUp.push_back (pitch);
                    ++pitch;
                }

                double cost = 0.0;

                if (lastVoicing.empty())
                {
                    double centre = 0.0;
                    for (const auto n : notesUp) centre += n;
                    cost = std::abs (centre / static_cast<double> (notesUp.size()) - 64.0);
                }
                else
                {
                    for (const auto n : notesUp)
                    {
                        auto nearest = 1.0e9;
                        for (const auto m : lastVoicing)
                            nearest = juce::jmin (nearest, static_cast<double> (std::abs (n - m)));
                        cost += nearest;
                    }

                    // Not too far from the middle of the keyboard, either.
                    cost += 0.3 * std::abs (notesUp.front() - 60);
                }

                if (cost < bestCost)
                {
                    bestCost = cost;
                    best = notesUp;
                }
            }

            lastVoicing = best;
            return best;
        }

        void playKeys (double barStart)
        {
            const auto& row = pattern.keys;
            char lastSymbol = 'C';

            for (int step = 0; step < steps; ++step)
            {
                const auto symbol = symbolAt (row, step);
                const auto at = barStart + step * stepLength;
                const auto* chord = chordAt (at);

                if (symbol == '.')
                {
                    finishNotes (at);
                    continue;
                }

                const bool changed = chord != nullptr && current != nullptr && ! sameChord (*chord, *current);
                auto play = symbol;

                if (symbol == '-')
                {
                    if (! changed)
                        continue;

                    play = lastSymbol == 'a' ? 'a' : 'c';   // held into a new chord: it's played again
                }

                // A broken chord rings on, as with the sustain pedal, until the chord or the figure changes.
                if (play != 'a' || changed)
                    finishNotes (at);

                // A broken chord starts again with each new chord, and with each note of the left hand.
                if (chord != current || play == 'b' || play == 'B')
                    arpeggio = 0;

                current = chord;

                if (chord == nullptr || chord->root < 0)
                    continue;

                if (changed || currentVoicing.empty())
                    currentVoicing = voicing (*chord);

                const auto when = timeOf (barStart, step);
                const auto shortNotes = feel.staccato;

                if (play == 'b' || play == 'B')
                    startNote (48 + (chord->bass >= 0 ? chord->bass : chord->root), when, velocityOf (80, 5), shortNotes);   // C3 .. B3

                if (play == 'C' || play == 'c' || play == 'B')
                {
                    const auto base = play == 'c' ? 66 : 90;

                    for (const auto pitch : currentVoicing)
                        startNote (pitch, when + 0.004 * std::abs (unit (random)), velocityOf (base, 5), shortNotes);
                }

                if (play == 'a')
                {
                    // Up through the chord and on into the octave above, then round again.
                    const auto size = static_cast<int> (currentVoicing.size());
                    const auto turn = arpeggio % (2 * size);
                    const auto pitch = currentVoicing[static_cast<size_t> (turn % size)] + (turn >= size ? 12 : 0);
                    ++arpeggio;
                    startNote (pitch, when, velocityOf (72, 6), false);
                }

                lastSymbol = play;
            }
        }

        //==============================================================================
        // Notes sound until the next thing their player does (or a rest); short ones stop sooner.
        struct Sounding
        {
            int pitch;
            double start;
            int velocity;
            bool shortNote;
        };

        void startNote (int pitch, double start, int velocity, bool shortNote)
        {
            // The same key again: the one still sounding ends first.
            for (auto s = sounding.begin(); s != sounding.end(); ++s)
            {
                if (s->pitch == pitch)
                {
                    addNote (s->pitch, s->start, juce::jmin (start - s->start - 0.02, s->shortNote ? stepLength * 1.4 : 1.0e9), s->velocity);
                    sounding.erase (s);
                    break;
                }
            }

            sounding.push_back ({ pitch, start, velocity, shortNote });
        }

        void finishNotes (double at)
        {
            for (const auto& s : sounding)
            {
                auto length = at - s.start - 0.02;

                if (s.shortNote)
                    length = juce::jmin (length, stepLength * 1.4);

                addNote (s.pitch, s.start, length, s.velocity);
            }

            sounding.clear();
        }

        //==============================================================================
        const Player player;
        const Style style;
        const std::vector<Chord>& chords;
        const double fromPpq, toPpq, intensity;
        const Tier tier;
        const Feel feel;
        std::mt19937 random;
        std::uniform_real_distribution<double> unit { -1.0, 1.0 };

        double quartersPerBar = 4.0, stepLength = 0.25;
        int steps = 16, levelShift = 0;
        Pattern pattern;

        std::vector<MidiNote> notes;
        std::vector<Sounding> sounding;
        const Chord* current = nullptr;
        std::vector<int> lastVoicing, currentVoicing;
        int arpeggio = 0;
    };
}

//==============================================================================
bool SessionPlayers::parseChord (const juce::String& symbolIn, Chord& chord)
{
    // Typographic spellings, a character at a time: sharp and flat signs, the triangle (maj7),
    // the circles (dim, and m7b5 crossed), a long minus.
    juce::String symbol;
    const auto trimmed = symbolIn.trim();

    for (auto p = trimmed.getCharPointer(); ! p.isEmpty();)
    {
        const auto c = p.getAndAdvance();
        const auto sevenNext = *p == '7';

        if (c == 0x266f)                        symbol << "#";
        else if (c == 0x266d)                   symbol << "b";
        else if (c == 0x2212 || c == 0x2013)    symbol << "-";
        else if (c == 0x00b0)                   symbol << "dim";
        else if (c == 0x0394 || c == 0x00f8)
        {
            symbol << (c == 0x0394 ? "maj7" : "m7b5");

            if (sevenNext)
                ++p;                            // its 7 is already said
        }
        else
        {
            symbol << juce::String::charToString (c);
        }
    }

    chord = {};

    if (symbol.equalsIgnoreCase ("N.C.") || symbol.equalsIgnoreCase ("NC") || symbol.equalsIgnoreCase ("x"))
    {
        chord.name = "N.C.";
        return true;
    }

    auto main = symbol;
    juce::String slash;

    if (symbol.containsChar ('/'))
    {
        main = symbol.upToFirstOccurrenceOf ("/", false, false);
        slash = symbol.fromFirstOccurrenceOf ("/", false, false);
    }

    int used = 0;
    const auto root = readNote (main, used);

    if (root < 0)
        return false;

    auto quality = main.substring (used);

    // Words in any case: Maj7, MIN, Sus4, Add9.
    for (const auto* word : { "maj", "min", "dim", "aug", "sus", "add", "dom" })
        if (quality.startsWithIgnoreCase (word))
            quality = juce::String (word) + quality.substring (3);

    const Quality* found = nullptr;

    for (const auto& candidate : qualities())
        for (const auto* spelling : candidate.spellings)
            if (found == nullptr && quality == spelling)
                found = &candidate;

    if (found == nullptr)
        return false;

    auto bass = -1;

    if (slash.isNotEmpty())
    {
        int bassUsed = 0;
        bass = readNote (slash, bassUsed);

        if (bass < 0 || bassUsed != slash.length())
            return false;
    }

    chord.root = root;
    chord.bass = bass == root ? -1 : bass;
    chord.intervals = found->intervals;
    chord.name = chordName (root, *found, chord.bass);
    return true;
}

juce::Result SessionPlayers::parseProgression (const juce::String& textIn, double startPpq, double quartersPerBar, int beatsPerBar,
                                               int times, std::vector<Chord>& chords)
{
    chords.clear();
    const auto text = textIn.replaceCharacters ("\r\n\t", "   ");
    std::vector<juce::StringArray> bars;

    if (text.containsChar ('|'))
    {
        auto parts = juce::StringArray::fromTokens (text, "|", "");

        for (int i = 0; i < parts.size(); ++i)
        {
            auto tokens = juce::StringArray::fromTokens (parts[i], " ", "");
            tokens.removeEmptyStrings();

            if (tokens.isEmpty())
            {
                if (i == 0 || i == parts.size() - 1)
                    continue;            // the | at either end

                tokens.add ("%");        // an empty bar: the chord goes on
            }

            bars.push_back (tokens);
        }
    }
    else
    {
        auto tokens = juce::StringArray::fromTokens (text, " ", "");
        tokens.removeEmptyStrings();

        for (const auto& token : tokens)
            bars.push_back (juce::StringArray (token));
    }

    if (bars.empty())
        return juce::Result::fail ("Type the chords to play: C | Am | F | G, say.");

    times = juce::jlimit (1, 64, times);

    if (bars.size() * static_cast<size_t> (times) > 2048)
        return juce::Result::fail ("That's more than 2048 bars: play fewer times, or fewer bars.");

    beatsPerBar = juce::jmax (1, beatsPerBar);
    const auto beat = quartersPerBar / beatsPerBar;
    Chord previous;
    bool havePrevious = false;

    for (int round = 0; round < times; ++round)
    {
        for (size_t b = 0; b < bars.size(); ++b)
        {
            const auto barStart = startPpq + static_cast<double> (static_cast<size_t> (round) * bars.size() + b) * quartersPerBar;
            const auto& tokens = bars[b];
            const auto count = tokens.size();

            for (int k = 0; k < count; ++k)
            {
                // Evenly through the bar, on a beat; more chords than beats and the extras drop out.
                const auto from = barStart + std::round (static_cast<double> (k) * beatsPerBar / count) * beat;
                const auto to = barStart + std::round (static_cast<double> (k + 1) * beatsPerBar / count) * beat;

                if (to <= from + 1.0e-9)
                    continue;

                Chord chord;
                const auto& token = tokens[k];

                if (token == "%" || token == "-" || token == "/" || token == ".")
                {
                    if (! havePrevious)
                        return juce::Result::fail ("The progression starts with \"" + token + "\": there's no chord to go on with yet.");

                    chord = previous;
                }
                else if (! parseChord (token, chord))
                {
                    return juce::Result::fail ("\"" + token + "\" isn't a chord Sonify knows. Try C, Am, F#m7, Bbmaj7, G/B or Dsus4.");
                }

                chord.start = from;
                chord.length = to - from;

                // The same chord again simply goes on.
                if (! chords.empty() && sameChord (chords.back(), chord)
                    && std::abs (chords.back().start + chords.back().length - from) < 1.0e-6)
                    chords.back().length += chord.length;
                else
                    chords.push_back (chord);

                previous = chord;
                havePrevious = true;
            }
        }
    }

    return juce::Result::ok();
}

juce::String SessionPlayers::detectChords (const std::vector<MidiClip>& clips, const TempoMap& map, int& firstBar, int& bars)
{
    struct Heard
    {
        double from, to;
        int pitch;
        float velocity;
    };

    std::vector<Heard> heard;

    for (const auto& clip : clips)
    {
        if (clip.isAudio())
            continue;

        for (const auto& note : clip.notes)
        {
            if (! clip.shows (note))
                continue;

            const auto from = clip.songPosition (note.start);
            const auto to = juce::jmin (from + note.length, clip.start + clip.length);

            if (to > from)
                heard.push_back ({ from, to, note.pitch, note.velocity });
        }
    }

    firstBar = 1;
    bars = 0;

    if (heard.empty())
        return {};

    auto begin = heard.front().from, end = heard.front().to;

    for (const auto& h : heard)
    {
        begin = juce::jmin (begin, h.from);
        end = juce::jmax (end, h.to);
    }

    // On the song's bars, as its time signatures count them.
    const auto first = map.barAt (begin + 1.0e-6).index;
    const auto endBar = map.barAt (end - 1.0e-6);
    const auto last = juce::jmax (first + 1, endBar.index + (end - endBar.start > 1.0e-6 ? 1 : 0));
    firstBar = first + 1;
    bars = last - first;

    struct Candidate
    {
        int root = -1;
        const Quality* quality = nullptr;
        int bass = -1;
        double score = 0.0;
    };

    // What sounds between from and to, and the chord that explains it best.
    const auto listen = [&heard] (double from, double to) -> Candidate
    {
        std::array<double, 12> weights {};
        int lowest = 128;
        double total = 0.0;

        for (const auto& h : heard)
        {
            const auto overlap = juce::jmin (h.to, to) - juce::jmax (h.from, from);

            if (overlap <= 0.0)
                continue;

            const auto w = overlap * (0.4 + h.velocity);
            weights[static_cast<size_t> (h.pitch % 12)] += w;
            total += w;
            lowest = juce::jmin (lowest, h.pitch);
        }

        Candidate best;

        if (total <= 0.0)
            return best;

        // The usual shapes only: major, minor, the three common sevenths, diminished, sus4 and the
        // bare fifth (indexes into qualities()).
        static const std::array<int, 8> shapes { 0, 1, 2, 3, 4, 6, 11, 23 };
        auto bestScore = -1.0e9;

        for (int root = 0; root < 12; ++root)
        {
            for (const auto shape : shapes)
            {
                const auto& quality = qualities()[static_cast<size_t> (shape)];
                double inside = 0.0;
                auto missing = 0.0;

                for (const auto interval : quality.intervals)
                {
                    const auto w = weights[static_cast<size_t> ((root + interval) % 12)];
                    inside += w;

                    if (w < 0.05 * total)
                        missing += interval == 7 ? 0.1 : 0.3;
                }

                auto score = inside - 0.6 * (total - inside) - missing * total;

                if (quality.intervals.size() > 3)
                    score -= 0.05 * total;          // a seventh must really be there

                if (quality.intervals.size() < 3)
                    score -= 0.15 * total;          // a bare fifth only when nothing else fits

                if (lowest % 12 == root)
                    score += 0.15 * total;

                if (score > bestScore)
                {
                    bestScore = score;
                    best.root = root;
                    best.quality = &quality;
                }
            }
        }

        // The lowest note, when it's a chord tone other than the root: an inversion (C/E).
        const auto lowPc = lowest % 12;

        for (const auto interval : best.quality->intervals)
            if (interval != 0 && (best.root + interval) % 12 == lowPc)
                best.bass = lowPc;

        best.score = bestScore / total;
        return best;
    };

    juce::StringArray written;
    juce::String previous;

    const auto nameOfCandidate = [] (const Candidate& c) { return chordName (c.root, *c.quality, c.bass); };

    for (int bar = first; bar < last; ++bar)
    {
        const auto from = map.ppqOfBar (bar), length = map.barAt (from).length();
        const auto middle = from + length * 0.5, to = from + length;
        const auto whole = listen (from, to);

        if (whole.quality == nullptr)
        {
            written.add (previous.isNotEmpty() ? "%" : "N.C.");
            continue;
        }

        // Two chords in the bar, when its halves clearly say so: a new root (a broken chord or a
        // melody leaves a half without some tone, which only seems to change its colour).
        const auto a = listen (from, middle), b = listen (middle, to);

        if (a.quality != nullptr && b.quality != nullptr && a.root != b.root
            && 0.5 * (a.score + b.score) > whole.score + 0.1)
        {
            written.add (nameOfCandidate (a) + " " + nameOfCandidate (b));
            previous = nameOfCandidate (b);
            continue;
        }

        written.add (nameOfCandidate (whole));
        previous = nameOfCandidate (whole);
    }

    return written.joinIntoString (" | ");
}

std::vector<MidiNote> SessionPlayers::generate (Player player, Style style, const std::vector<Chord>& chords,
                                                double fromPpq, double toPpq, int numerator, int denominator,
                                                double intensity, juce::uint32 seed)
{
    if (toPpq <= fromPpq)
        return {};

    Performer performer (player, style, chords, fromPpq, toPpq, juce::jmax (1, numerator), juce::jmax (1, denominator), intensity, seed);
    return performer.play();
}

//==============================================================================
const Library::Entry* SessionPlayers::bestInstrument (const Library& library, Player player)
{
    static const juce::StringArray drumWords { "drum", "kit", "beat", "808", "909", "percussion", "battery", "groove agent", "addictive" };
    static const juce::StringArray bassWords { "bass" };
    static const juce::StringArray keyWords { "piano", "keys", "rhodes", "e-piano", "epiano", "wurli", "organ", "clav", "grand", "upright" };

    const Library::Entry* best = nullptr;
    double bestScore = 0.0;

    for (const auto& entry : library.getEntries())
    {
        if (entry.kind != "instrument" || entry.missing || ! library.isOffered (entry.file))
            continue;

        const auto name = entry.name.toLowerCase();
        double score = 0.0;

        const auto named = [&name] (const juce::StringArray& words)
        {
            for (const auto& word : words)
                if (name.contains (word))
                    return true;

            return false;
        };

        if (player == Player::drums)
        {
            if (entry.category == "Drums")                                                score += 5.0;
            if (entry.features.contains ("drum") || entry.features.contains ("drum-machine")) score += 4.0;
            if (named (drumWords))                                                         score += 3.0;
        }
        else if (player == Player::bass)
        {
            if (named (bassWords) && ! name.contains ("drum"))                             score += 5.0;
        }
        else if (named (keyWords))
        {
            score += 5.0;
        }

        if (score <= 0.0)
            continue;

        // Among equals: favorites, then what's used most.
        score += (entry.favorite ? 1.0 : 0.0) + 0.05 * juce::jmin (20, entry.uses);

        if (score > bestScore)
        {
            bestScore = score;
            best = &entry;
        }
    }

    return best;
}

juce::Result SessionPlayers::create (AudioEngine& engine, Library& library, const Request& request,
                                     std::vector<int>& trackIds, juce::StringArray& notes)
{
    trackIds.clear();

    if (request.players.empty())
        return juce::Result::fail ("Choose at least one player.");

    // The players play in the time signature of the bar they start on.
    const auto& map = engine.getTempoMap();
    const auto startPpq = map.ppqOfBar (juce::jmax (0, request.startBar - 1));
    const auto startBar = map.barAt (startPpq);
    const auto numerator = juce::jmax (1, startBar.numerator);
    const auto denominator = juce::jmax (1, startBar.denominator);
    const auto quartersPerBar = startBar.length();

    std::vector<Chord> chords;
    const auto parsed = parseProgression (request.chords, startPpq, quartersPerBar, numerator, request.times, chords);

    if (parsed.failed())
        return parsed;

    if (chords.empty())
        return juce::Result::fail ("Type the chords to play: C | Am | F | G, say.");

    const auto endPpq = chords.back().start + chords.back().length;
    const auto seed = request.seed != 0 ? request.seed : static_cast<juce::uint32> (juce::Random::getSystemRandom().nextInt());

    for (const auto player : request.players)
    {
        auto part = generate (player, request.style, chords, startPpq, endPpq, numerator, denominator, request.intensity,
                              seed + static_cast<juce::uint32> (player) * 7919u);
        const auto trackId = engine.addTrack (Track::Kind::instrument);

        // Its instrument: the one chosen, or the best fit installed (Cayman when there's none).
        juce::String file, pluginId, pluginName;

        if (const auto chosen = request.instruments.find (player); chosen != request.instruments.end())
        {
            if (chosen->second.first != "none")
            {
                for (const auto& entry : library.getEntries())
                {
                    if (entry.file == chosen->second.first && entry.id == chosen->second.second && entry.kind == "instrument"
                        && ! entry.missing && library.isOffered (entry.file))
                    {
                        file = entry.file;
                        pluginId = entry.id;
                        pluginName = entry.name;
                    }
                }

                if (file.isEmpty())
                    notes.add ("The instrument chosen for the " + nameOf (player) + " isn't available now, so it plays Cayman.");
            }
        }
        else if (const auto* best = bestInstrument (library, player))
        {
            file = best->file;
            pluginId = best->id;
            pluginName = best->name;
        }

        if (file.isNotEmpty())
        {
            int slot = 0;
            const auto loaded = engine.loadPlugin (trackId, juce::File (file), pluginId, -1, 0, slot);

            if (loaded.failed())
                notes.add (pluginName + " didn't load for the " + nameOf (player) + ", so it plays Cayman: " + loaded.getErrorMessage());
            else
                library.markUsed (file, pluginId);
        }

        engine.setTrackName (trackId, trackNameOf (player));

        MidiClip clip;
        clip.start = startPpq;
        clip.length = endPpq - startPpq;
        clip.offset = 0.0;

        for (auto note : part)
        {
            note.start = juce::jmax (0.0, note.start - startPpq);
            clip.notes.push_back (note);
        }

        std::sort (clip.notes.begin(), clip.notes.end(), [] (const MidiNote& a, const MidiNote& b) { return a.start < b.start; });
        engine.addClip (trackId, std::move (clip));
        trackIds.push_back (trackId);
    }

    return juce::Result::ok();
}

//==============================================================================
SessionPlayers::Style SessionPlayers::styleFrom (const juce::String& name)
{
    const auto n = name.toLowerCase().removeCharacters (" -");

    if (n == "rock")                    return Style::rock;
    if (n == "ballad")                  return Style::ballad;
    if (n == "funk")                    return Style::funk;
    if (n == "house" || n == "edm")     return Style::house;
    if (n == "hiphop" || n == "rap")    return Style::hiphop;
    return Style::pop;
}

juce::String SessionPlayers::nameOf (Style style)
{
    switch (style)
    {
        case Style::rock:   return "rock";
        case Style::ballad: return "ballad";
        case Style::funk:   return "funk";
        case Style::house:  return "house";
        case Style::hiphop: return "hiphop";
        case Style::pop:    break;
    }

    return "pop";
}

bool SessionPlayers::playerFrom (const juce::String& name, Player& player)
{
    const auto n = name.toLowerCase().trim();

    if (n == "drums" || n == "drum" || n == "drummer")   { player = Player::drums; return true; }
    if (n == "bass" || n == "bassist")                   { player = Player::bass;  return true; }
    if (n == "keys" || n == "piano" || n == "keyboard")  { player = Player::keys;  return true; }
    return false;
}

juce::String SessionPlayers::nameOf (Player player)
{
    return player == Player::drums ? "drums" : player == Player::bass ? "bass" : "keys";
}

juce::String SessionPlayers::trackNameOf (Player player)
{
    return player == Player::drums ? "Drums" : player == Player::bass ? "Bass" : "Keys";
}
