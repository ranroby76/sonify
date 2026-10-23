<!-- D:\workspace\Fanan_web_hub\src\skill_synth_engines.md -->
---
name: synth-engines
for: plugins
title: Synth engines
description: Nine tested sound engines for instruments (analog, supersaw, morphing, west-coast folding, FM, phase distortion, additive, vocal formants, particles), each one voice's oscillator with two colour knobs, plus a complete polyphonic instrument module built around them (voices, mono and glide, envelopes, filter, velocity, bend, vibrato). Read it before building any synthesizer.
---
# Synth engines

`modules/Engines.h` holds the engines; `modules/Synth.cpp` is a complete instrument around one of them. Both build on the dsp-toolkit skill's `Dsp.h`, `DspEnv.h` and `DspFx.h` (read that skill too, and write those headers first). The engines' ideas follow Mutable Instruments' Plaits (Emilie Gillet's MIT-licensed open designs); the code here is written for `stella_api.h`.

## Which engine for which sound
Every engine has `prepare (rate)`, `start (note, velocity, seed)` and `render (hz, timbre, morph, left, right)`; timbre and morph are 0..1.
- `Analog` (timbre: saw to square, morph: pulse width; a sub an octave down): basses, leads, brass, classic polysynth. With the toolkit's `Ladder`: the Moog / Juno / Prophet family.
- `Supersaw` (timbre: detune, morph: side saws' level): trance leads, huge pads, hoovers, stabs. Use 16 voices for pads; detune 0.3 to 0.6 is the sweet spot.
- `Morph` (morph: sine, triangle, saw, square, narrow pulse; timbre: doubling detune): one-knob shape sweeps, soft keys, chiptune-ish leads (morph near 1 is a thin pulse).
- `Fold` (timbre: fold depth, morph: symmetry): Buchla / west-coast tones, bouncy plucks when the envelope drives timbre, aggressive basses. Tamed by pitch: it never aliases.
- `Fm` (timbre: depth, morph: ratio from 0.5, 1, 2, 3, 3.5, 4, 5, 7, 14; `feedback` member 0..1): electric pianos (ratio 1 or 14, depth 0.3 to 0.5), bells (3.5), basses (ratio 1 or 2, depth 0.5 plus feedback), metallic plucks. Its depth decays like a struck tine by itself.
- `PhaseDistortion` (timbre: bend, morph: CZ resonance): Casio CZ basses and leads, resonant sweeps without a filter.
- `Additive` (timbre: brightness, morph: where a resonant bump sits): organs, glass, airy pads, vowel-like peaks; very clean.
- `Formant` (morph: vowel a, e, i, o, u; timbre: voice size): choirs, talking leads, "ooh" and "aah" pads (with slow vibrato and a chorus).
- `Particles` (timbre: density, morph: scatter): rain, sparkle, glitter, bubbling textures; layer it quietly under a pad.

Layers: a voice can hold two engines and mix them (an `Fm` tine over a soft `Morph` sine for a Rhodes-like key; `Supersaw` plus `Analog`'s sub for a bass-heavy lead). Expose the mix as a knob.

## Making a plugin from them
- Keep only the engines the plugin uses in `Engines.h` (or write one engine into its own header): smaller files build faster and steps stay short.
- Name the knobs for what they do in this plugin ("Brightness", "Vowel", "Detune"), not "Timbre" and "Morph".
- Modulate: an envelope or LFO on timbre or morph is where these engines come alive (fold depth from the filter envelope: a pluck; vowel from a slow LFO: a choir that breathes).
- A selector between engines is fine (an "Engine" choice parameter and a `switch` in the voice), but each extra engine is code to write and build: a plugin with one great engine beats one with nine thin ones.
- After the first build, listen with `analyze_sound` (the sound-critic skill): one note at C3, one high (C6) for aliasing, a chord for headroom.
- Show it: the template's displays `"synth.env"` (the loudest voice) and `"synth.cutoff"` (the live cutoff, Hz) make the panel move as it plays: a `filter` curve with `"source": "synth.cutoff"`, and the 3D screens of the 3d-displays skill, whose `"engine"` setting names the engine used here.

## Engines.h
```cpp
// modules/Engines.h: sound engines, each one voice's oscillator part, written against Dsp.h.
// Every engine has prepare (sampleRate), start (note, velocity, seed) when its voice starts
// a note, and render (hz, timbre, morph, left, right) per sample: timbre and morph (0..1) are
// its two colour knobs. Keep only the engines the plugin uses.
#pragma once
#include "Dsp.h"
#include "DspFx.h"

namespace engines
{
    using stella::pi;
    using stella::twoPi;

    inline float sawBlep (float phase, float dt) noexcept    { return 2.0f * phase - 1.0f - dsp::polyBlep (phase, dt); }

    inline float pulseBlep (float phase, float dt, float pw) noexcept
    {
        float t2 = phase + 1.0f - pw;
        t2 -= std::floor (t2);
        return (phase < pw ? 1.0f : -1.0f) + dsp::polyBlep (phase, dt) - dsp::polyBlep (t2, dt);
    }

    inline void advance (float& phase, float dt) noexcept    { phase += dt; if (phase >= 1.0f) phase -= 1.0f; }

    //==========================================================================
    /** Analog: two detuned oscillators and a sub. timbre: saw to square; morph: pulse width. */
    struct Analog
    {
        float rate = 48000.0f, pa = 0.0f, pb = 0.0f, ps = 0.0f, detuneCents = 7.0f, subLevel = 0.3f;

        void prepare (float sampleRate) noexcept               { rate = sampleRate; }
        void start (int, float, std::uint32_t seed) noexcept   { pb = (float) (seed % 1000u) * 0.001f; }

        void render (float hz, float timbre, float morph, float& left, float& right) noexcept
        {
            const float spread = std::pow (2.0f, detuneCents / 1200.0f);
            const float da = stella::clamp (hz * spread / rate, 0.0f, 0.45f), db = stella::clamp (hz / spread / rate, 0.0f, 0.45f);
            const float ds = stella::clamp (hz * 0.5f / rate, 0.0f, 0.45f);
            const float pw = 0.5f - 0.45f * morph;

            const float a = sawBlep (pa, da) + (pulseBlep (pa, da, pw) - sawBlep (pa, da)) * timbre;
            const float b = sawBlep (pb, db) + (pulseBlep (pb, db, pw) - sawBlep (pb, db)) * timbre;
            const float s = pulseBlep (ps, ds, 0.5f);
            advance (pa, da); advance (pb, db); advance (ps, ds);

            left = 0.55f * a + 0.42f * b + subLevel * 0.7f * s;   // a little stereo from the detune
            right = 0.42f * a + 0.55f * b + subLevel * 0.7f * s;
        }
    };

    //==========================================================================
    /** Supersaw: seven saws spread in pitch and in stereo (the trance lead, the big pad).
        timbre: detune (gentle to huge); morph: the side saws' level against the centre one. */
    struct Supersaw
    {
        static constexpr int count = 7;
        float rate = 48000.0f, phase[count] {};

        void prepare (float sampleRate) noexcept    { rate = sampleRate; }

        void start (int, float, std::uint32_t seed) noexcept
        {
            dsp::Noise n; n.seed (seed);
            for (auto& p : phase) p = 0.5f + 0.5f * n.next();   // free-running phases: no flam, no thin attack
        }

        void render (float hz, float timbre, float morph, float& left, float& right) noexcept
        {
            static const float offsets[count] { -0.110f, -0.063f, -0.019f, 0.0f, 0.020f, 0.064f, 0.108f };
            static const float pans[count]    { -1.0f, 0.66f, -0.33f, 0.0f, 0.33f, -0.66f, 1.0f };
            const float detune = timbre * timbre * 0.9f + 0.01f;   // most of the knob in the useful, subtle range
            const float side = morph * 0.8f, centre = 1.0f - morph * 0.45f;
            left = right = 0.0f;

            for (int i = 0; i < count; ++i)
            {
                const float dt = stella::clamp (hz * (1.0f + offsets[i] * detune) / rate, 0.0f, 0.45f);
                const float v = sawBlep (phase[i], dt) * (i == 3 ? centre : side);
                advance (phase[i], dt);
                left += v * (0.5f - 0.5f * pans[i]);
                right += v * (0.5f + 0.5f * pans[i]);
            }

            left *= 0.8f; right *= 0.8f;
        }
    };

    //==========================================================================
    /** Morphing: one wave sliding sine, triangle, saw, square, narrow pulse, doubled for
        thickness. morph: the shape; timbre: detune of the second copy. */
    struct Morph
    {
        float rate = 48000.0f, p1 = 0.0f, p2 = 0.37f;

        void prepare (float sampleRate) noexcept               { rate = sampleRate; }
        void start (int, float, std::uint32_t) noexcept        {}

        static float shape (float phase, float dt, float morph) noexcept
        {
            const float position = stella::clamp (morph, 0.0f, 1.0f) * 3.999f;
            const int i = (int) position;
            const float f = position - (float) i;

            auto at = [phase, dt] (int k) -> float
            {
                switch (k)
                {
                    case 0:  return std::sin (twoPi * phase);
                    case 1:  return 1.0f - 4.0f * std::fabs (phase - 0.5f);
                    case 2:  return sawBlep (phase, dt);
                    case 3:  return pulseBlep (phase, dt, 0.5f);
                    default: return pulseBlep (phase, dt, 0.12f);
                }
            };

            return at (i) + (at (i + 1) - at (i)) * f;
        }

        void render (float hz, float timbre, float morph, float& left, float& right) noexcept
        {
            const float d1 = stella::clamp (hz / rate, 0.0f, 0.45f);
            const float d2 = stella::clamp (hz * std::pow (2.0f, timbre * 25.0f / 1200.0f) / rate, 0.0f, 0.45f);
            const float a = shape (p1, d1, morph), b = shape (p2, d2, morph);
            advance (p1, d1); advance (p2, d2);
            left = 0.55f * a + 0.35f * b;
            right = 0.35f * a + 0.55f * b;
        }
    };

    //==========================================================================
    /** West-coast folding: a sine folded over itself, oversampled, its depth tamed as the pitch
        rises so it never aliases. timbre: fold depth; morph: symmetry (adds even harmonics). */
    struct Fold
    {
        float rate = 48000.0f, phase = 0.0f;
        dsp::Oversampler2x os;
        dsp::DcBlock dc;

        void prepare (float sampleRate) noexcept               { rate = sampleRate; dc.prepare (sampleRate); }
        void start (int, float, std::uint32_t) noexcept        {}

        void render (float hz, float timbre, float morph, float& left, float& right) noexcept
        {
            const float dt = stella::clamp (hz / rate, 0.0f, 0.45f);
            const float x = std::sin (twoPi * phase);
            advance (phase, dt);

            // sin (g x) spreads to about (g + 1) harmonics: keep them under the doubled rate's Nyquist.
            const float limit = std::fmax (0.0f, 0.8f * rate / std::fmax (hz, 1.0f) - 1.0f);
            const float gain = std::fmin (1.0f + timbre * 9.0f, std::fmax (1.0f, limit));
            const float offset = morph * 0.8f;
            const float y = dc.process (os.process (x, [gain, offset] (float v) { return std::sin (0.5f * pi * gain * (v + offset)); }));
            left = right = y * 0.8f;
        }
    };

    //==========================================================================
    /** FM: a sine carrier modulated by a sine at a ratio, with a modulator that rings out like a
        struck tine (its depth decays to a third). timbre: depth; morph: the ratio (from a list
        of musical ones). DX7-style feedback on the modulator via feedback (0..1). */
    struct Fm
    {
        float rate = 48000.0f, pc = 0.0f, pm = 0.0f, depthEnv = 1.0f, decay = 0.9999f, last1 = 0.0f, last2 = 0.0f, velocity = 1.0f;
        float feedback = 0.0f;

        void prepare (float sampleRate) noexcept    { rate = sampleRate; decay = std::exp (-1.0f / (0.6f * sampleRate)); }
        void start (int, float v, std::uint32_t) noexcept    { depthEnv = 1.0f; velocity = v; pc = pm = 0.0f; last1 = last2 = 0.0f; }

        void render (float hz, float timbre, float morph, float& left, float& right) noexcept
        {
            static const float ratios[] { 0.5f, 1.0f, 2.0f, 3.0f, 3.5f, 4.0f, 5.0f, 7.0f, 14.0f };
            const int index = (int) (stella::clamp (morph, 0.0f, 1.0f) * 8.999f);
            const float ratio = ratios[index];

            // Carson's rule: the spectrum reaches about (depth + 1) x the modulator: keep it under Nyquist.
            const float fm = hz * ratio;
            const float maxDepth = std::fmax (0.0f, (0.45f * rate - hz) / std::fmax (fm, 1.0f) - 1.0f);
            depthEnv = 0.33f + (depthEnv - 0.33f) * decay;
            const float depth = std::fmin (timbre * 9.0f * depthEnv * (0.4f + 0.6f * velocity), maxDepth);

            const float mod = std::sin (twoPi * pm + feedback * 1.5f * 0.5f * (last1 + last2));   // averaged feedback: stable
            last2 = last1; last1 = mod;
            const float out = std::sin (twoPi * pc + depth * mod);

            advance (pc, stella::clamp (hz / rate, 0.0f, 0.45f));
            advance (pm, stella::clamp (fm / rate, 0.0f, 0.49f));
            left = right = out * 0.7f;
        }
    };

    //==========================================================================
    /** Phase distortion (Casio CZ): a cosine read through a bent phase, from soft to saw-bright,
        crossfaded with the CZ "resonance" wave (a windowed, sync-like sweep). timbre: bend;
        morph: resonance. */
    struct PhaseDistortion
    {
        float rate = 48000.0f, phase = 0.0f;
        void prepare (float sampleRate) noexcept               { rate = sampleRate; }
        void start (int, float, std::uint32_t) noexcept        { phase = 0.0f; }

        void render (float hz, float timbre, float morph, float& left, float& right) noexcept
        {
            const float dt = stella::clamp (hz / rate, 0.0f, 0.45f);
            const float tame = stella::clamp (1.0f - (hz - 1000.0f) / 4000.0f, 0.15f, 1.0f);   // less bend up high
            const float d = 0.5f - 0.48f * timbre * tame;
            const float bent = phase < d ? 0.5f * phase / d : 0.5f + 0.5f * (phase - d) / (1.0f - d);
            const float warp = -std::cos (twoPi * bent);
            const float k = 1.0f + (1.0f + 11.0f * timbre) * tame;
            const float res = (1.0f - phase) * std::cos (twoPi * phase * k) * 1.4f - 0.2f;
            advance (phase, dt);
            left = right = (warp + (res - warp) * morph) * 0.7f;
        }
    };

    //==========================================================================
    /** Additive: 32 harmonics with a brightness slope and a movable resonant bump (an organ, a
        glassy pad, a vowel-like peak). timbre: brightness; morph: where the bump sits. Each
        harmonic is a rotating pair: no sin() per sample. */
    struct Additive
    {
        static constexpr int harmonics = 32;
        float rate = 48000.0f, c[harmonics] {}, s[harmonics] {}, cr[harmonics] {}, sr[harmonics] {}, amp[harmonics] {};
        float lastHz = -1.0f, lastTimbre = -1.0f, lastMorph = -1.0f;
        int counter = 0;

        void prepare (float sampleRate) noexcept    { rate = sampleRate; }

        void start (int, float, std::uint32_t) noexcept
        {
            for (int k = 0; k < harmonics; ++k) { c[k] = 1.0f; s[k] = 0.0f; }
            lastHz = -1.0f;
        }

        void update (float hz, float timbre, float morph) noexcept
        {
            const float slope = 2.4f - 1.7f * timbre, centre = 1.0f + morph * 24.0f;
            float sum = 0.0f;

            for (int k = 0; k < harmonics; ++k)
            {
                const float n = (float) (k + 1), f = hz * n;
                const float w = twoPi * f / rate;
                cr[k] = std::cos (w); sr[k] = std::sin (w);
                const float bump = 1.0f + 5.0f * std::exp (-(n - centre) * (n - centre) / 6.0f);
                amp[k] = f < 0.45f * rate ? std::pow (n, -slope) * bump : 0.0f;   // nothing above Nyquist
                sum += amp[k];
            }

            for (auto& a : amp) a /= std::fmax (sum, 1.0e-6f) * 0.9f;
            lastHz = hz; lastTimbre = timbre; lastMorph = morph;
        }

        void render (float hz, float timbre, float morph, float& left, float& right) noexcept
        {
            if (std::fabs (hz - lastHz) > 0.01f || std::fabs (timbre - lastTimbre) > 0.002f || std::fabs (morph - lastMorph) > 0.002f)
                update (hz, timbre, morph);

            float out = 0.0f;
            const bool renormalise = (++counter & 63) == 0;   // rotation drifts: pull it back to the unit circle

            for (int k = 0; k < harmonics; ++k)
            {
                const float nc = c[k] * cr[k] - s[k] * sr[k];
                s[k] = c[k] * sr[k] + s[k] * cr[k];
                c[k] = nc;
                if (renormalise) { const float g = 1.5f - 0.5f * (c[k] * c[k] + s[k] * s[k]); c[k] *= g; s[k] *= g; }
                out += s[k] * amp[k];
            }

            left = right = out;
        }
    };

    //==========================================================================
    /** Vocal formants: two overlapping grains per pitch period, each a burst of three formant
        sines under a smooth window (the way voices and choirs are made). morph: the vowel
        (a, e, i, o, u); timbre: the voice's size (deep and large to small and bright). */
    struct Formant
    {
        struct Grain { float g = 1.0f, f[3] {}; bool on = false; };
        float rate = 48000.0f, phase = 0.0f;
        Grain grains[2];

        void prepare (float sampleRate) noexcept               { rate = sampleRate; }
        void start (int, float, std::uint32_t) noexcept        { phase = 0.0f; grains[0] = grains[1] = {}; }

        void render (float hz, float timbre, float morph, float& left, float& right) noexcept
        {
            // Formants (Hz) and their levels for a, e, i, o, u.
            static const float table[5][6] { { 730, 1090, 2440, 1.0f, 0.50f, 0.25f }, { 530, 1840, 2480, 1.0f, 0.40f, 0.30f },
                                             { 270, 2290, 3010, 1.0f, 0.25f, 0.25f }, { 570, 840, 2410, 1.0f, 0.45f, 0.15f },
                                             { 300, 870, 2240, 1.0f, 0.25f, 0.10f } };
            const float position = stella::clamp (morph, 0.0f, 1.0f) * 3.999f;
            const int v = (int) position;
            const float t = position - (float) v, scale = 0.8f + 0.5f * timbre;
            float freq[3], level[3];

            for (int k = 0; k < 3; ++k)
            {
                freq[k] = (table[v][k] + (table[v + 1][k] - table[v][k]) * t) * scale;
                level[k] = table[v][k + 3] + (table[v + 1][k + 3] - table[v][k + 3]) * t;
            }

            const float dt = stella::clamp (hz / rate, 0.0f, 0.45f);
            const float before = phase;
            advance (phase, dt);

            if (phase < before)                   { grains[0] = {}; grains[0].g = 0.0f; grains[0].on = true; }   // a new period
            if (before < 0.5f && phase >= 0.5f)   { grains[1] = {}; grains[1].g = 0.0f; grains[1].on = true; }   // half way

            float out = 0.0f;

            for (auto& grain : grains)
            {
                if (! grain.on) continue;
                const float window = 0.5f - 0.5f * std::cos (twoPi * grain.g);   // two Hann windows half a period apart add up to 1

                for (int k = 0; k < 3; ++k)
                {
                    out += std::sin (twoPi * grain.f[k]) * level[k] * window;
                    grain.f[k] += freq[k] / rate;
                    if (grain.f[k] >= 1.0f) grain.f[k] -= 1.0f;
                }

                grain.g += dt;
                if (grain.g >= 1.0f) grain.on = false;
            }

            left = right = out * 0.45f;
        }
    };

    //==========================================================================
    /** Particles: random impulses ringing three resonators tuned around the note (rain, sparkle,
        glass, bubbling textures). timbre: density; morph: how far the resonators scatter. */
    struct Particles
    {
        float rate = 48000.0f, ratio[3] { 1.0f, 1.0f, 1.0f };
        dsp::Svf filter[3];
        dsp::Noise noise;

        void prepare (float sampleRate) noexcept    { rate = sampleRate; }
        void start (int, float, std::uint32_t seed) noexcept    { noise.seed (seed); for (auto& f : filter) f.reset(); }

        void render (float hz, float timbre, float morph, float& left, float& right) noexcept
        {
            const float density = 2.0f * std::pow (1000.0f, timbre);   // 2 .. 2000 a second
            float impulse = 0.0f;

            if (noise.next() * 0.5f + 0.5f < density / rate)
            {
                impulse = noise.next();

                for (int k = 0; k < 3; ++k)   // each drop retunes the resonators a little
                {
                    ratio[k] = std::pow (2.0f, noise.next() * morph * 2.0f + (float) k * 0.58f * morph);
                    filter[k].set (stella::clamp (hz * ratio[k], 30.0f, 0.45f * rate), 0.985f, rate);
                }
            }

            const float a = filter[0].bandpass (impulse), b = filter[1].bandpass (impulse), c = filter[2].bandpass (impulse);
            left = (a * 0.6f + b * 0.3f + c * 0.5f) * 30.0f;
            right = (a * 0.3f + b * 0.6f + c * 0.5f) * 30.0f;
        }
    };
}
```

## Synth.cpp
```cpp
// modules/Synth.cpp: a complete polyphonic instrument around one engine (Engines.h): voices,
// mono with glide, filter and amp envelopes, velocity, pitch bend and mod-wheel vibrato. It shows
// the loudest voice's envelope ("synth.env", 0..1) and its filter's live cutoff ("synth.cutoff",
// in Hz): a filter curve bound to it opens and closes as it plays.
#include "Engines.h"
#include "DspEnv.h"

class Synth final : public stella::Module
{
public:
    static constexpr int maxVoices = 8;

    void prepare (double sampleRate, int) override
    {
        rate = (float) sampleRate;

        for (int v = 0; v < maxVoices; ++v)
        {
            auto& voice = voices[v];
            voice.engine.prepare (rate);
            voice.amp.prepare (rate);
            voice.fenv.prepare (rate);
        }

        dcL.prepare (rate); dcR.prepare (rate);
        volume.prepare (sampleRate, 30.0f);
        volume.snap (stella::dbToGain (param (P_volume)));
    }

    void reset() override
    {
        for (auto& v : voices) { v.amp.stage = dsp::Adsr::idle; v.amp.level = 0.0f; v.fenv.level = 0.0f; v.filterL.reset(); v.filterR.reset(); }
    }

    void process (const stella::Context& c, const float* const*, float* const* out) override
    {
        const float timbre = param (P_timbre), morph = param (P_morph);
        const float cutoff = param (P_cutoff), reso = param (P_reso), envAmount = param (P_envamt);
        const float glide = param (P_glide) * 0.001f;
        const bool mono = param (P_mono) > 0.5f;
        const int voiceCount = mono ? 1 : maxVoices;
        const float glideCoef = glide > 0.0005f ? 1.0f - std::exp (-1.0f / (glide * rate * 0.25f)) : 1.0f;
        const float vibrato = c.modWheel * 0.35f;   // semitones at full wheel
        int n = 0;

        for (auto& v : voices)
        {
            v.amp.set (param (P_attack) * 0.001f, param (P_decay) * 0.001f, param (P_sustain), param (P_release) * 0.001f);
            v.fenv.set (0.003f, param (P_decay) * 0.001f, 0.0f, param (P_release) * 0.001f);
        }

        for (int i = 0; i < c.numFrames; ++i)
        {
            for (; n < c.numNotes && c.notes[n].frame <= i; ++n)
                handle (c.notes[n], voiceCount, mono);

            const float lfo = vibratoLfo.next (dsp::Lfo::sine, 5.5f, rate) * vibrato;
            float left = 0.0f, right = 0.0f;

            for (int k = 0; k < voiceCount; ++k)
            {
                auto& v = voices[k];

                if (! v.amp.isActive())
                    continue;

                v.pitch += (v.target - v.pitch) * glideCoef;
                const float hz = stella::noteToHz (v.pitch + c.pitchBend * 2.0f + lfo);
                float l = 0.0f, r = 0.0f;
                v.engine.render (hz, timbre, morph, l, r);

                const float env = v.amp.next(), fe = v.fenv.next();

                if ((v.counter++ & 15) == 0)
                {
                    const float keyTrack = std::pow (2.0f, (v.pitch - 60.0f) / 24.0f);   // half key tracking
                    const float f = cutoff * keyTrack * std::pow (2.0f, envAmount * fe * (0.4f + 0.6f * v.velocity) * 5.0f);
                    v.filterL.set (f, reso, rate, 1.2f);
                    v.filterR.set (f, reso, rate, 1.2f);
                    v.cutoffNow = f;
                }

                const float gain = env * (0.35f + 0.65f * v.velocity);
                left += v.filterL.process (l) * gain;
                right += v.filterR.process (r) * gain;
            }

            const float g = volume.next (stella::dbToGain (param (P_volume)));
            out[0][i] = dsp::fastTanh (dcL.process (left * g));
            out[1][i] = dsp::fastTanh (dcR.process (right * g));
        }

        // The loudest voice: its envelope, and where its filter is now (the knob's cutoff when
        // nothing plays).
        const Voice* loudest = nullptr;

        for (int k = 0; k < voiceCount; ++k)
            if (voices[k].amp.isActive() && (loudest == nullptr || voices[k].amp.level > loudest->amp.level))
                loudest = &voices[k];

        display (0, loudest != nullptr ? loudest->amp.level : 0.0f);
        display (1, loudest != nullptr ? loudest->cutoffNow : cutoff);
    }

private:
    enum { P_timbre, P_morph, P_cutoff, P_reso, P_envamt, P_attack, P_decay, P_sustain, P_release, P_glide, P_mono, P_volume };

    struct Voice
    {
        engines::Analog engine;   // the plugin's engine: any struct from Engines.h (or two, mixed, for layers)
        dsp::Ladder filterL, filterR;
        dsp::Adsr amp, fenv;
        float pitch = 60.0f, target = 60.0f, velocity = 1.0f;
        float cutoffNow = 1000.0f;   // its filter's cutoff now, in Hz
        int counter = 0;
    };

    void startVoice (Voice& v, int note, float velocity, bool slide)
    {
        v.target = (float) note;
        if (! slide) v.pitch = v.target;
        v.velocity = velocity;
        const auto seed = 1u + (std::uint32_t) note * 2654435761u + ++starts;
        v.engine.start (note, velocity, seed);
        v.amp.noteOn(); v.fenv.noteOn();
    }

    void handle (const stella::Note& e, int voiceCount, bool mono)
    {
        if (mono)
        {
            auto& v = voices[0];
            const bool wasHeld = allocator.numHeld > 0;
            const int now = e.on ? allocator.monoOn (e.note) : allocator.monoOff (e.note);

            if (now < 0) { v.amp.noteOff(); v.fenv.noteOff(); return; }
            if (e.on && ! wasHeld) startVoice (v, now, e.velocity, v.amp.isActive());   // a new phrase: retrigger
            else v.target = (float) now;                                               // legato: slide only
            return;
        }

        if (e.on)
        {
            float levels[maxVoices];
            for (int k = 0; k < voiceCount; ++k) levels[k] = voices[k].amp.level;
            startVoice (voices[allocator.noteOn (e.note, voiceCount, levels)], e.note, e.velocity, false);
        }
        else if (const int k = allocator.noteOff (e.note, voiceCount); k >= 0)
        {
            voices[k].amp.noteOff(); voices[k].fenv.noteOff();
        }
    }

    Voice voices[maxVoices];
    dsp::VoiceAllocator<maxVoices> allocator;
    dsp::Lfo vibratoLfo;
    dsp::DcBlock dcL, dcR;
    stella::Smoother volume;
    float rate = 48000.0f;
    std::uint32_t starts = 0;
};

namespace
{
    const char* const outputs[] { "out L", "out R" };
    const char* const displayNames[] { "env", "cutoff" };

    const stella::Param params[]
    {
        { "timbre",   "Timbre",     0.0f, 1.0f,     0.4f,    "",   1.0f },
        { "morph",    "Morph",      0.0f, 1.0f,     0.3f,    "",   1.0f },
        { "cutoff",   "Cutoff",     40.0f, 16000.0f, 2500.0f, "Hz", 0.3f },
        { "reso",     "Resonance",  0.0f, 1.0f,     0.2f,    "",   1.0f },
        { "envamt",   "Env amount", 0.0f, 1.0f,     0.4f,    "",   1.0f },
        { "attack",   "Attack",     1.0f, 5000.0f,  5.0f,    "ms", 0.3f },
        { "decay",    "Decay",      5.0f, 5000.0f,  500.0f,  "ms", 0.3f },
        { "sustain",  "Sustain",    0.0f, 1.0f,     0.75f,   "",   1.0f },
        { "release",  "Release",    5.0f, 8000.0f,  350.0f,  "ms", 0.3f },
        { "glide",    "Glide",      0.0f, 1000.0f,  0.0f,    "ms", 0.4f },
        { "mono",     "Mono",       0.0f, 1.0f,     0.0f,    "",   1.0f },
        { "volume",   "Volume",     -48.0f, 6.0f,   -4.5f,   "dB", 1.0f },
    };

    stella::Registrar registrar ({ "Synth", "Synth", nullptr, 0, outputs, STELLA_COUNT (outputs), params, STELLA_COUNT (params),
                                   [] () -> stella::Module* { return new Synth(); }, displayNames, STELLA_COUNT (displayNames) });
}
```

Graph for it: `{ "format": 1, "modules": [ { "id": "synth", "type": "Synth" } ], "wires": [ { "from": "synth.out L", "to": "plugin.out L" }, { "from": "synth.out R", "to": "plugin.out R" } ] }`. Effects (chorus, delay, reverb) go after it as modules of their own (the effects skills).
