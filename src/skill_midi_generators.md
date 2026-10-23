<!-- D:\workspace\Fanan_web_hub\src\skill_midi_generators.md -->
---
name: midi-generators
for: devices
title: MIDI generators
description: Generative and creative MIDI tools for devices, tested: a seeded random source, Euclidean rhythms, probability and ratchets, a Marbles-style "deja vu" loop with spread and bias, notes kept in the song's key, Markov melodies, chord voice leading, swing and humanizing, plus two complete devices (a generative melody sequencer and Euclidean drums) and recipes for arpeggiators, chord generators and evolving patterns. Read it for any device that generates, varies or humanizes notes.
---
# MIDI generators

`Generators.h` (a helper file the device includes: write it with `write_file` as "Generators.h") and two complete devices. Ideas after Mutable Instruments' Marbles (Emilie Gillet's MIT-licensed designs), Euclidean rhythms (Toussaint) and generative composition; the code is written for `sonify_device.h`.

## The tools
- `gen::Random`: seeded xorshift. `uniform()` 0..1, `chance (p)`, `below (n)`, `bell()` (about -1..1, most near 0). Seed it from a fixed number (or a "Seed" param) so a pattern can be repeated.
- `gen::euclid (step, steps, hits, rotate)`: hits spread evenly over steps. Classics: 3/8 tresillo, 5/8 cinquillo, 4/16 four on the floor, 2/16 rotated 4 a backbeat, 7/16 and 9/16 samba and bossa feels, 5/12 and 7/12 African bell patterns, 11/16 busy hats.
- `gen::DejaVu`: a loop of up to 16 remembered values. Deja vu 0: always new; 1: an exact loop; 0.5 to 0.8: a pattern that mostly repeats and slowly evolves (the sweet spot).
- `gen::shaped (u, spread, bias)`: a 0..1 value from a uniform one: spread 0 always the bias, 1 a full range; bias low to high. Edges bounce back in.
- `gen::noteInKey (key, root, value, range)`: a note in the song's key, `range` key steps above the root (7 an octave, 14 two).
- `gen::markovStep (random, position, calm)`: a melody that moves by steps and small leaps and drifts home (calm 1 stepwise and singable, 0 jumpy). Position is in key steps: `key.shift (root, position)`.
- `gen::voiceLead (chord, count, previous, previousCount)`: each chord note moved by octaves to sit closest to the previous chord: smooth progressions instead of jumping blocks.
- `gen::swung (time, stepNumber, step, amount)` and `gen::humanize (random, time, velocity, step, amount)`.

## Patterns to build from them
- **Generative melody** (the `MarbleMelody` device below): grid steps, an Euclidean gate, chance, then a note from deja vu, spread and bias in the key. Add an "Octave" by raising the range, a "Calm" knob by switching to `markovStep`.
- **Euclidean drums** (`EuclidDrums` below): one lane per drum note (36, 38, 42; channel 9 is MIDI channel 10, the drum channel), rotation puts the snare on the backbeat, accents on downbeats, hat ratchets.
- **Smarter arpeggiator**: modes beyond up and down: converge (outside in), diverge, pinky (alternate with the top note), random walk (`markovStep` over the held notes' indexes), chord-inversion climb (each pass the next inversion), octave ratchets (repeat each note an octave up within the step). Hold the notes in an array sorted by pitch; step through it on the grid.
- **Chord generator**: one key in, a chord out: `key.chord (pitch, 3 or 4, notes)`, then `voiceLead` against the last chord played; a "Spread" knob opens the voicing (move the middle note up an octave); remember what each held key started, to end those notes.
- **Polyrhythm / polymeter**: two lanes with different step counts (`n % 5` against `n % 4`) or different rates (`grid` twice with different steps).
- **Probability and fills**: chance per step; raise the chance on the last bar of every 4 (`block.bars` gives the bar number while playing) for automatic fills.
- **Evolving over time**: a slow "Mutation" knob that is deja vu inverted; an "Energy" macro that raises hits, chance and velocity together.
- **Humanizer** (an effect device): pass every note through `humanize` (and keep the note-off with its own note-on's time shift, so lengths stay right).

## Rules that keep generators musical
- Stay in the song's key (`block.keyAt (time)`), and recompute the note per step: the key can change mid-song.
- Every note started needs its end: `playNote (start, length, ...)` does both.
- Work on the grid (`block.grid`) both while the song plays and while it's stopped (from the first note's time as the origin), so the device plays in time either way.
- Gate (note length) as a percent of the step; 50 to 80% for melodies, short (0.1 beats) for drums.
- Test with `test_device`: hold a note for 2 bars (playing and stopped), check the count of notes, their pitches in key and their times on the grid.

## Generators.h
```cpp
// Generators.h: generative tools for Sonify devices: a seeded random source, Euclidean rhythms,
// probability, a "deja vu" memory loop, random notes shaped by spread, bias and the key, Markov
// melodies, chord voice leading, swing and humanizing. Written against sonify_device.h.
#pragma once
#include "sonify_device.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace gen
{
    using namespace sonify;

    //==========================================================================
    /** A small random source: the same seed plays the same "random" again (xorshift). */
    struct Random
    {
        std::uint32_t state = 0x9e3779b9u;

        void seed (std::uint32_t s) noexcept    { state = s != 0 ? s * 2654435761u : 0x9e3779b9u; next(); }

        std::uint32_t next() noexcept
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return state;
        }

        float uniform() noexcept                { return (float) (next() & 0xffffffu) / 16777216.0f; }   // 0 .. 1
        bool chance (float probability) noexcept { return uniform() < probability; }
        int below (int n) noexcept              { return n > 0 ? (int) (next() % (std::uint32_t) n) : 0; }

        /** Bell-shaped around 0 (about -1 .. 1): sums of uniforms. */
        float bell() noexcept                   { return (uniform() + uniform() + uniform() - 1.5f) / 1.5f; }
    };

    //==========================================================================
    /** Euclidean rhythm: hits spread as evenly as possible over steps, turned by rotate
        (3 in 8 is the tresillo; 5 in 8 the cinquillo; 7 in 16 a samba feel). */
    inline bool euclid (int step, int steps, int hits, int rotate) noexcept
    {
        if (steps <= 0 || hits <= 0)
            return false;

        hits = hits > steps ? steps : hits;
        const int s = (((step + rotate) % steps) + steps) % steps;
        return (s * hits) % steps < hits;
    }

    //==========================================================================
    /** Deja vu (after Mutable Instruments' Marbles): a loop of remembered values. At 0 every
        value is new; at 1 the loop repeats exactly; in between it mostly repeats, sometimes
        changes. length: how many steps the loop holds (1 .. 16). */
    struct DejaVu
    {
        std::array<float, 16> memory {};
        int position = 0;

        float next (Random& random, float dejaVu, int length) noexcept
        {
            length = length < 1 ? 1 : (length > 16 ? 16 : length);
            position = (position + 1) % length;

            if (! random.chance (dejaVu))
                memory[(size_t) position] = random.uniform();   // something new in this slot

            return memory[(size_t) position];
        }

        void clear (Random& random) noexcept    { for (auto& m : memory) m = random.uniform(); position = 0; }
    };

    /** A value 0..1 shaped like Marbles' X: spread 0 (always the bias) .. 1 (a full range around
        it), bias 0 (low) .. 1 (high). u is a uniform 0..1 (from Random or DejaVu). Values past
        an edge bounce back in, so they don't pile up on the lowest or highest note. */
    inline float shaped (float u, float spread, float bias) noexcept
    {
        float v = bias + (u - 0.5f) * spread;
        if (v < 0.0f) v = -v;
        if (v > 1.0f) v = 2.0f - v;
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }

    /** A note in the key, from a 0..1 value: range in key steps above root (e.g. 14: two octaves). */
    inline int noteInKey (const Key& key, int root, float value, int range) noexcept
    {
        const int steps = (int) std::lround (value * (float) range);
        return key.shift (key.snap (root), steps);
    }

    //==========================================================================
    /** A Markov melody over scale steps: the next interval chosen from weights that favour steps
        and small leaps, with a pull back towards home. calm 0 (jumpy) .. 1 (stepwise). Returns
        the new position in key steps from the root (kept within -7 .. 14). */
    inline int markovStep (Random& random, int position, float calm) noexcept
    {
        // Moves: down a fifth .. up a fifth in key steps, and their weights when calm = 1 and 0.
        static const int moves[] { -4, -3, -2, -1, 0, 1, 2, 3, 4 };
        static const float stepwise[] { 0.02f, 0.05f, 0.12f, 0.28f, 0.06f, 0.28f, 0.12f, 0.05f, 0.02f };
        static const float jumpy[]    { 0.10f, 0.12f, 0.12f, 0.12f, 0.08f, 0.12f, 0.12f, 0.12f, 0.10f };
        float weights[9], total = 0.0f;

        for (int i = 0; i < 9; ++i)
        {
            weights[i] = stepwise[i] * calm + jumpy[i] * (1.0f - calm);

            // Far from home, moves towards it grow more likely.
            const int after = position + moves[i];
            if (after > 10 || after < -5) weights[i] *= 0.2f;
            if ((position > 4 && moves[i] < 0) || (position < -2 && moves[i] > 0)) weights[i] *= 1.6f;
            total += weights[i];
        }

        float pick = random.uniform() * total;

        for (int i = 0; i < 9; ++i)
        {
            pick -= weights[i];

            if (pick <= 0.0f)
                return position + moves[i] < -7 ? -7 : (position + moves[i] > 14 ? 14 : position + moves[i]);
        }

        return position;
    }

    //==========================================================================
    /** Voice leading: moves each note of a chord by octaves so the whole chord sits as close as
        possible to the previous one (smooth, like a keyboard player's hands). */
    inline void voiceLead (int* chord, int count, const int* previous, int previousCount) noexcept
    {
        if (previousCount <= 0)
            return;

        for (int i = 0; i < count; ++i)
        {
            int best = chord[i], bestDistance = 1000;

            for (int octave = -3; octave <= 3; ++octave)
            {
                const int candidate = chord[i] + 12 * octave;

                if (candidate < 24 || candidate > 108)
                    continue;

                int distance = 1000;

                for (int j = 0; j < previousCount; ++j)
                    distance = std::min (distance, std::abs (candidate - previous[j]));

                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = candidate;
                }
            }

            chord[i] = best;
        }
    }

    //==========================================================================
    /** Swing: pushes every second step later. amount 0 (straight) .. 1 (a full triplet feel,
        two thirds of the pair). */
    inline double swung (double time, long long stepNumber, double step, float amount) noexcept
    {
        return (stepNumber & 1) != 0 ? time + step * (double) amount / 3.0 : time;
    }

    /** Humanize: a little earlier or later and softer or harder, by amount 0..1. */
    inline void humanize (Random& random, double& time, int& velocity, double step, float amount) noexcept
    {
        time += step * 0.08 * (double) amount * (double) random.bell();
        velocity = (int) std::lround ((float) velocity * (1.0f + 0.25f * amount * random.bell()));
        velocity = velocity < 1 ? 1 : (velocity > 127 ? 127 : velocity);
    }
}
```

## device.cpp
```cpp
#include "sonify_device.h"
#include "Generators.h"
using namespace sonify;

// Marble Melody: a generative sequencer. Hold a note (or play the song): on each step of the
// grid an Euclidean rhythm and a chance decide whether a note plays, and the note comes from
// the song's key above the held note, shaped by spread and bias and remembered by deja vu.
class MarbleMelody : public Device
{
public:
    Param rate     { "rate", "Rate", 0, 15, 10, Unit::rate };                 // 1/16
    Param steps    { "steps", "Steps", 2, 32, 16, Unit::steps };
    Param hits     { "hits", "Hits", 1, 32, 7, Unit::number };
    Param rotate   { "rotate", "Rotate", 0, 31, 0, Unit::number };
    Param chance   { "chance", "Chance", 0, 100, 90, Unit::percent };
    Param spread   { "spread", "Spread", 0, 100, 60, Unit::percent };
    Param bias     { "bias", "Bias", 0, 100, 50, Unit::percent };
    Param dejaVu   { "dejavu", "Deja vu", 0, 100, 50, Unit::percent };
    Param loop     { "loop", "Loop", 1, 16, 8, Unit::steps };
    Param gate     { "gate", "Gate", 5, 100, 60, Unit::percent };
    Param swing    { "swing", "Swing", 0, 100, 0, Unit::percent };
    Param humanize { "humanize", "Humanize", 0, 100, 15, Unit::percent };

    MarbleMelody()    { random.seed (20261013u); memory.clear (random); held.fill (0); }

    void process (Block& block) override
    {
        // The notes held choose the root (the lowest); everything else passes through.
        for (const auto& e : block.events())
        {
            if (e.isNoteOn())
            {
                if (heldCount == 0 && ! block.isPlayingAt (e.time))
                    origin = e.time;   // stopped: the steps count from the first note
                held[(size_t) e.pitch()]++;
                ++heldCount;
            }
            else if (e.isNoteOff())
            {
                if (held[(size_t) e.pitch()] > 0) { held[(size_t) e.pitch()]--; --heldCount; }
                if (heldCount == 0) origin = std::numeric_limits<double>::quiet_NaN();
            }
            else
            {
                block.play (e);
            }
        }

        const bool playing = block.isPlaying();

        if (heldCount == 0 && ! playing)
            return;

        int root = 60;
        for (int p = 0; p < 128; ++p) if (held[(size_t) p] > 0) { root = p; break; }

        const double step = rate.length();
        GridLine lines[64];
        const int count = block.grid (step, origin, lines, 64);

        for (int i = 0; i < count; ++i)
        {
            const auto n = lines[i].number;

            if (! gen::euclid ((int) (n % steps.integer()), steps.integer(), hits.integer(), rotate.integer()))
                continue;

            if (! random.chance (chance.value() / 100.0f))
                continue;

            const float u = memory.next (random, dejaVu.value() / 100.0f, loop.integer());
            const float v = gen::shaped (u, spread.value() / 100.0f, bias.value() / 100.0f);
            const auto key = block.keyAt (lines[i].time);
            const int pitch = gen::noteInKey (key, root, v, 14);   // two octaves of the key

            double time = gen::swung (lines[i].time, n, step, swing.value() / 100.0f);
            int velocity = 96;
            gen::humanize (random, time, velocity, step, humanize.value() / 100.0f);
            block.playNote (time, step * gate.value() / 100.0, 0, pitch, velocity);
        }
    }

    void reset() override    { held.fill (0); heldCount = 0; origin = std::numeric_limits<double>::quiet_NaN(); }

private:
    gen::Random random;
    gen::DejaVu memory;
    std::array<int, 128> held {};
    int heldCount = 0;
    double origin = std::numeric_limits<double>::quiet_NaN();
};

SONIFY_DEVICE (MarbleMelody)
```

## Euclidean drums (another device.cpp)
```cpp
#include "sonify_device.h"
#include "Generators.h"
using namespace sonify;

// Euclid Drums: three Euclidean drum lanes (kick, snare, hats) with chance, ratchets and
// accents, on the song's grid (or from the first key pressed while stopped).
class EuclidDrums : public Device
{
public:
    Param kicks   { "kicks", "Kicks", 0, 16, 4, Unit::number };
    Param snares  { "snares", "Snares", 0, 16, 2, Unit::number };
    Param hats    { "hats", "Hats", 0, 16, 11, Unit::number };
    Param chance  { "chance", "Chance", 0, 100, 100, Unit::percent };
    Param ratchet { "ratchet", "Ratchets", 0, 100, 10, Unit::percent };
    Param accent  { "accent", "Accent", 0, 100, 40, Unit::percent };

    EuclidDrums()    { random.seed (7u); }

    void process (Block& block) override
    {
        for (const auto& e : block.events())
        {
            if (e.isNoteOn() && ! block.isPlayingAt (e.time) && std::isnan (origin)) origin = e.time;
            else if (! e.isNote()) block.play (e);
        }

        GridLine lines[64];
        const int count = block.grid (0.25, origin, lines, 64);   // sixteenths

        for (int i = 0; i < count; ++i)
        {
            const int s = (int) (lines[i].number % 16);
            const bool downbeat = s % 4 == 0;
            lane (block, lines[i].time, 36, gen::euclid (s, 16, kicks.integer(), 0), downbeat);
            lane (block, lines[i].time, 38, gen::euclid (s, 16, snares.integer(), 4), downbeat);   // rotated onto the backbeat
            lane (block, lines[i].time, 42, gen::euclid (s, 16, hats.integer(), 0), downbeat);
        }
    }

    void reset() override    { origin = std::numeric_limits<double>::quiet_NaN(); }

private:
    void lane (Block& block, double time, int note, bool hit, bool downbeat)
    {
        if (! hit || ! random.chance (chance.value() / 100.0f))
            return;

        const int velocity = downbeat && random.chance (accent.value() / 100.0f) ? 120 : 90;

        if (note == 42 && random.chance (ratchet.value() / 100.0f))   // a hat roll: two quick hits in the step
        {
            block.playNote (time, 0.06, 9, note, velocity);
            block.playNote (time + 0.125, 0.06, 9, note, velocity - 20);
            return;
        }

        block.playNote (time, 0.1, 9, note, velocity);
    }

    gen::Random random;
    double origin = std::numeric_limits<double>::quiet_NaN();
};

SONIFY_DEVICE (EuclidDrums)
```
