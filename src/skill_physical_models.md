<!-- D:\workspace\Fanan_web_hub\src\skill_physical_models.md -->
---
name: physical-models
for: plugins
title: Physical models
description: Tested physical modelling for acoustic-sounding instruments: a plucked string (Karplus-Strong with tuning compensation, pluck position, stiffness, damping) and a modal resonator with real mode ratios (string, bar, marimba, bell, glass, membrane, plate), mallet and bow exciters, and a complete polyphonic instrument module. Read it for plucked, struck, bowed or bell-like instruments.
---
# Physical models

`modules/Physical.h` (written against the dsp-toolkit skill's `Dsp.h` and `DspFx.h`: write those first) and a complete instrument, `modules/Physical.cpp`. Ideas after Mutable Instruments' Rings and Elements and the classic Karplus-Strong papers; the code is written for `stella_api.h`.

## What each part does
- `PluckString`: a noise burst one period long, darkened by brightness and velocity, combed by the pluck position, runs round a delay loop tuned to the note. `tune()` compensates the loop filter's and the dispersion allpass's delay, so it's in tune (within a cent) from C1 to C7. `decay` is the -60 dB time; `damp()` on note-off shortens it (a palm mute, a released key). Stiffness bends the overtones sharp like a piano string.
- `Modal`: up to 16 two-pole resonators at the material's mode ratios. Upper modes die sooner (more so with low brightness); where it's struck (position) silences modes at their nodes, as on a real bar. Each mode's gain includes `sin (w)`, so every pitch is equally loud.
- `Exciter`: `strike()` is a mallet (soft felt to hard stick; a harder hit rings more highs); `setFlow()` is continuous noise for bowing, rubbing or blowing while the note is held.

## Materials and what they make
- `string` (harmonic ratios): a struck or bowed string, a soft harp.
- `bar`: xylophone, glockenspiel (high notes), wind chimes; `marimba`: marimba and vibraphone (add a slow tremolo for a vibe).
- `bell`: church and tubular bells, the hum an octave below; long decays (5 to 15 s).
- `glass`: wine glass, singing bowl (bow it: `bow` 0.5 to 0.9).
- `membrane`: toms, hand drums, tablas (short decays, with a pitch drop from a quick envelope on the note).
- `plate`: gongs, metal sheets, sound-design hits.

## Recipes
- Acoustic guitar: string, decay 3 to 6 s, brightness 0.5, position 0.15, a short damp (150 ms); a body: two or three `Svf` band-passes (100, 200, 400 Hz) mixed under the dry string.
- Harp: string, brightness 0.4, position 0.3, decay 4 s, no damping on release.
- Piano-ish: string, stiffness 0.3 to 0.5, two strings per voice detuned by 1 to 2 cents, brightness from velocity.
- Kalimba / music box: bar, hardness 0.8, decay 1.5 s, brightness 0.7.
- Marimba: marimba, hardness 0.4, decay 1.2 s; vibraphone: marimba, decay 4 s, brightness 0.8, tremolo 5 Hz.
- Bowed glass pad: glass, bow 0.7, hardness 0, decay 6 s, release 800 ms, with a chorus and reverb after it.

## Tips
- Physical models ring past the note-off: when you listen (`analyze_sound`), give a long tail (3 to 8 s), and read "still sounding at the end" as normal for them.
- A plucked sound's RMS is low and its peaks high (crest 20 dB or more): judge its level by the peaks (about -6 dBFS for one note).
- For a noise-excited (bowed or blown) note, the measured pitch wobbles by a few cents from test to test: that's the noise, not a tuning error.
- Mode decays are per material; expose them as one "Decay" and one "Brightness" knob, plus "Hardness" (the mallet) and "Position".

## Physical.h
```cpp
// modules/Physical.h: physical models: a plucked string and a modal resonator (bars, bells,
// glass, membranes...), with their exciters. Written against Dsp.h and DspFx.h.
#pragma once
#include "Dsp.h"
#include "DspFx.h"

namespace physical
{
    using stella::pi;
    using stella::twoPi;

    //==========================================================================
    /** A plucked string (Karplus-Strong, extended): a burst of noise runs round a tuned delay
        loop that loses its highs a little on every trip. brightness 0..1 (the pluck and the
        string's tone), decay in seconds (to -60 dB), position 0..1 (where it's plucked: near 0.5
        hollow, near 0 or 1 thin), stiffness 0..1 (piano-like inharmonic stretch). */
    struct PluckString
    {
        dsp::DelayLine line;
        dsp::Noise noise;
        float rate = 48000.0f, loss = 0.0f, feedback = 0.99f, damping = 0.5f, delay = 100.0f;
        float apState = 0.0f, apCoef = 0.0f, excite = 0.0f, exciteLp = 0.0f, burstLp = 0.0f;
        int burst = 0, burstLength = 0, positionDelay = 0;
        dsp::DelayLine pluckComb;

        void prepare (float sampleRate)
        {
            rate = sampleRate;
            line.prepare ((int) (sampleRate / 20.0f) + 8);       // down to 20 Hz
            pluckComb.prepare ((int) (sampleRate / 20.0f) + 8);
        }

        void pluck (float hz, float velocity, float brightness, float position, std::uint32_t seed) noexcept
        {
            noise.seed (seed);
            burstLength = (int) (rate / std::fmax (hz, 20.0f));   // one period of noise
            burst = burstLength;
            excite = 0.4f + 0.6f * velocity;
            exciteLp = 0.15f + 0.85f * brightness * (0.5f + 0.5f * velocity);   // a soft pluck is darker
            positionDelay = std::max (1, (int) ((float) burstLength * stella::clamp (position, 0.02f, 0.5f)));
        }

        /** Tunes the loop: call when the pitch, decay, brightness or stiffness change (every block is fine). */
        void tune (float hz, float decaySeconds, float brightness, float stiffness) noexcept
        {
            hz = stella::clamp (hz, 20.0f, rate * 0.3f);
            damping = 0.25f + 0.74f * brightness;              // the loop filter: y = d x + (1 - d) y1
            apCoef = -0.6f * stiffness;                         // dispersion: one first-order allpass
            const float filterDelay = (1.0f - damping) / damping;
            const float allpassDelay = (1.0f - apCoef) / (1.0f + apCoef);
            delay = stella::clamp (rate / hz - filterDelay - allpassDelay, 2.0f, (float) (line.size - 8));
            feedback = std::pow (10.0f, -3.0f / (std::fmax (0.05f, decaySeconds) * hz));   // -60 dB after decaySeconds
        }

        /** Damps the string (a note-off): it dies away in about release seconds. */
        void damp (float hz, float releaseSeconds) noexcept
        {
            feedback = std::fmin (feedback, std::pow (10.0f, -3.0f / (std::fmax (0.02f, releaseSeconds) * hz)));
        }

        float next() noexcept
        {
            float in = 0.0f;

            if (burst > 0)   // the pluck: noise, darkened by the brightness, combed by the position
            {
                --burst;
                const float n = noise.next() * excite;
                burstLp += (n - burstLp) * exciteLp;
                pluckComb.push (burstLp);
                in = burstLp - pluckComb.read ((float) positionDelay);
            }

            const float fromLoop = line.read (delay);
            loss += (fromLoop - loss) * damping;                // highs fade on every trip
            const float ap = apCoef * loss + apState;           // y = c x + s; s = x - c y
            apState = dsp::undenormal (loss - apCoef * ap);
            const float y = ap * feedback + in;
            line.push (dsp::fastTanh (y));                      // keeps a hard pluck from building up
            return y;
        }
    };

    //==========================================================================
    /** A modal resonator: a bank of tuned ringing modes, each a two-pole resonator with its
        own level and decay (upper modes die sooner). The material sets the modes' ratios. */
    struct Modal
    {
        enum Material { string, bar, marimba, bell, glass, membrane, plate };
        static constexpr int maxModes = 16;

        float rate = 48000.0f;
        float y1[maxModes] {}, y2[maxModes] {}, a1[maxModes] {}, a2[maxModes] {}, gain[maxModes] {};
        int modes = 0;

        void prepare (float sampleRate) noexcept    { rate = sampleRate; }
        void clear() noexcept                       { for (int k = 0; k < maxModes; ++k) y1[k] = y2[k] = 0.0f; }

        static const float* ratios (Material m, int& count) noexcept
        {
            static const float stringRatios[]   { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
            static const float barRatios[]      { 1.0f, 2.756f, 5.404f, 8.933f, 13.345f, 18.638f };              // a free bar (xylophone, wind chime)
            static const float marimbaRatios[]  { 1.0f, 3.984f, 10.668f, 17.591f, 21.012f };                    // a bar tuned by its arch
            static const float bellRatios[]     { 0.5f, 1.0f, 1.183f, 1.506f, 2.0f, 2.514f, 2.662f, 3.011f, 4.166f, 5.433f, 6.796f };   // hum, prime, minor third...
            static const float glassRatios[]    { 1.0f, 2.32f, 4.25f, 6.63f, 9.38f };                            // a bowl or wine glass
            static const float membraneRatios[] { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f, 3.600f, 3.652f };   // a drum head
            static const float plateRatios[]    { 1.0f, 1.62f, 2.46f, 2.85f, 3.90f, 4.32f, 5.27f, 6.10f, 6.88f, 7.65f, 8.94f, 9.75f };

            switch (m)
            {
                case string:   count = 16; return stringRatios;
                case bar:      count = 6;  return barRatios;
                case marimba:  count = 5;  return marimbaRatios;
                case bell:     count = 11; return bellRatios;
                case glass:    count = 5;  return glassRatios;
                case membrane: count = 10; return membraneRatios;
                case plate:    count = 12; return plateRatios;
            }

            count = 1;
            return stringRatios;
        }

        /** Sets the modes: decay in seconds (the lowest mode's -60 dB), brightness 0..1 (how much
            the upper modes ring), position 0..1 (where it's struck: some modes vanish at nodes). */
        void tune (Material material, float hz, float decaySeconds, float brightness, float position) noexcept
        {
            int count = 0;
            const float* r = ratios (material, count);
            modes = 0;

            for (int k = 0; k < count && modes < maxModes; ++k)
            {
                const float f = hz * r[k];

                if (f > rate * 0.45f)
                    break;

                const float t60 = decaySeconds / (1.0f + (r[k] - 1.0f) * (1.2f - brightness));   // higher modes die sooner
                const float radius = std::pow (10.0f, -3.0f / (std::fmax (0.01f, t60) * rate));
                a1[modes] = 2.0f * radius * std::cos (twoPi * f / rate);
                a2[modes] = -radius * radius;
                const float strike = std::fabs (std::sin (pi * (float) (k + 1) * (0.05f + 0.9f * position)));
                // sin (w) makes every mode equally loud for the same strike, whatever its pitch.
                gain[modes] = 0.3f * std::sin (twoPi * f / rate) * strike * std::pow (0.8f + 0.2f * brightness, (float) k) / std::sqrt (r[k]);
                ++modes;
            }
        }

        float process (float x) noexcept
        {
            float out = 0.0f;

            for (int k = 0; k < modes; ++k)
            {
                const float y = a1[k] * y1[k] + a2[k] * y2[k] + x * gain[k];
                y2[k] = y1[k];
                y1[k] = dsp::undenormal (y);
                out += y;
            }

            return out;
        }
    };

    //==========================================================================
    /** What sets a resonator ringing. */
    struct Exciter
    {
        dsp::Noise noise;
        dsp::OnePole tone;
        float rate = 48000.0f, phase = 1.0f, step = 0.0f, amount = 0.0f, flow = 0.0f;

        void prepare (float sampleRate) noexcept    { rate = sampleRate; }

        /** A mallet: a smooth bump; hardness 0..1 from a soft felt (8 ms) to a hard stick (0.3 ms). */
        void strike (float velocity, float hardness, std::uint32_t seed) noexcept
        {
            noise.seed (seed);
            hardness *= 0.6f + 0.4f * velocity;   // a harder hit rings more highs
            phase = 0.0f;
            step = 1.0f / (rate * (0.008f - 0.0077f * hardness));
            amount = 4.0f * velocity * step;   // the same push whatever the hardness: a soft mallet is duller, not quieter
            tone.set (800.0f + 12000.0f * hardness * velocity, rate);
        }

        /** Continuous excitation (bowed, blown, rubbed): breath 0..1 while the note is held. */
        void setFlow (float breath) noexcept    { flow = breath; tone.set (2500.0f, rate); }

        float next() noexcept
        {
            float out = 0.0f;

            if (phase < 1.0f)
            {
                out += amount * (0.5f - 0.5f * std::cos (twoPi * phase));
                phase += step;
            }

            if (flow > 0.0f)
                out += tone.lowpass (noise.next()) * flow * 0.02f;

            return out;
        }
    };
}
```

## Physical.cpp
```cpp
// modules/Physical.cpp: a polyphonic physical-model instrument: a plucked string or a struck
// (or bowed) modal resonator of a chosen material, with damping on release.
#include "Physical.h"
#include "DspEnv.h"

class Physical final : public stella::Module
{
public:
    static constexpr int maxVoices = 8;

    void prepare (double sampleRate, int) override
    {
        rate = (float) sampleRate;

        for (auto& v : voices)
        {
            v.string.prepare (rate);
            v.modal.prepare (rate);
            v.exciter.prepare (rate);
        }

        dcL.prepare (rate); dcR.prepare (rate);
        volume.prepare (sampleRate, 30.0f);
        volume.snap (stella::dbToGain (param (P_volume)));
    }

    void reset() override
    {
        for (auto& v : voices) { v.modal.clear(); v.string.line.clear(); v.active = false; v.level = 0.0f; }
    }

    void process (const stella::Context& c, const float* const*, float* const* out) override
    {
        const int model = (int) std::lround (param (P_model));   // 0 string, 1.. the Modal materials
        const float decay = param (P_decay), brightness = param (P_bright), position = param (P_position);
        const float hardness = param (P_hardness), bow = param (P_bow), release = param (P_release) * 0.001f;
        int n = 0;

        // Retune what's playing once a block (pitch bend, knobs).
        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            v.hz = stella::noteToHz (v.note + c.pitchBend * 2.0f);

            if (model == 0)
            {
                v.string.tune (v.hz, decay, brightness, param (P_stiffness));
                if (! v.held) v.string.damp (v.hz, release);
            }
            else
            {
                v.modal.tune ((physical::Modal::Material) model, v.hz, v.held ? decay : std::fmin (decay, release), brightness, position);
            }

            v.exciter.setFlow (v.held ? bow * (0.5f + 0.5f * v.velocity) : 0.0f);
        }

        for (int i = 0; i < c.numFrames; ++i)
        {
            for (; n < c.numNotes && c.notes[n].frame <= i; ++n)
            {
                const auto& e = c.notes[n];

                if (e.on)
                {
                    float levels[maxVoices];
                    for (int k = 0; k < maxVoices; ++k) levels[k] = voices[k].active ? voices[k].level : 0.0f;

                    auto& v = voices[allocator.noteOn (e.note, maxVoices, levels)];
                    v.note = (float) e.note;
                    v.hz = stella::noteToHz (v.note);
                    v.velocity = e.velocity;
                    v.active = v.held = true;
                    v.level = 1.0f;
                    v.quiet = 0;
                    const auto seed = 7u + (std::uint32_t) e.note * 2654435761u + ++strikes;

                    if (model == 0)
                    {
                        v.string.tune (v.hz, decay, brightness, param (P_stiffness));
                        v.string.pluck (v.hz, e.velocity, brightness, position, seed);
                    }
                    else
                    {
                        v.modal.tune ((physical::Modal::Material) model, v.hz, decay, brightness, position);
                        v.exciter.strike (e.velocity, hardness, seed);
                    }
                }
                else if (const int k = allocator.noteOff (e.note, maxVoices); k >= 0)
                {
                    voices[k].held = false;   // damped from the next block
                }
            }

            float left = 0.0f, right = 0.0f;

            for (int k = 0; k < maxVoices; ++k)
            {
                auto& v = voices[k];

                if (! v.active)
                    continue;

                const float y = model == 0 ? v.string.next() : v.modal.process (v.exciter.next());
                v.level = std::fmax (std::fabs (y), v.level * 0.9995f);

                // A voice is free once it has been silent for a while.
                if (std::fabs (y) < 1.0e-5f) { if (++v.quiet > 4800 && ! v.held) v.active = false; }
                else v.quiet = 0;

                static const float spread[maxVoices] { 0.0f, -0.12f, 0.12f, -0.24f, 0.24f, -0.06f, 0.06f, 0.18f };
                const float pan = 0.5f + spread[k];   // voices spread a little, the first one centred
                left += y * (1.0f - pan);
                right += y * pan;
            }

            const float g = volume.next (stella::dbToGain (param (P_volume)));
            out[0][i] = dsp::fastTanh (dcL.process (left * g));
            out[1][i] = dsp::fastTanh (dcR.process (right * g));
        }
    }

private:
    enum { P_model, P_decay, P_bright, P_position, P_hardness, P_stiffness, P_bow, P_release, P_volume };

    struct Voice
    {
        physical::PluckString string;
        physical::Modal modal;
        physical::Exciter exciter;
        float note = 60.0f, hz = 261.6f, velocity = 1.0f, level = 0.0f;
        bool active = false, held = false;
        int quiet = 0;
    };

    Voice voices[maxVoices];
    dsp::VoiceAllocator<maxVoices> allocator;
    dsp::DcBlock dcL, dcR;
    stella::Smoother volume;
    float rate = 48000.0f;
    std::uint32_t strikes = 0;
};

namespace
{
    const char* const outputs[] { "out L", "out R" };

    const stella::Param params[]
    {
        { "model",     "Model",      0.0f, 6.0f,    0.0f,   "",   1.0f },   // string, bar, marimba, bell, glass, membrane, plate
        { "decay",     "Decay",      0.1f, 20.0f,   2.5f,   "s",  0.4f },
        { "bright",    "Brightness", 0.0f, 1.0f,    0.6f,   "",   1.0f },
        { "position",  "Position",   0.0f, 1.0f,    0.25f,  "",   1.0f },
        { "hardness",  "Hardness",   0.0f, 1.0f,    0.6f,   "",   1.0f },
        { "stiffness", "Stiffness",  0.0f, 1.0f,    0.0f,   "",   1.0f },
        { "bow",       "Bow",        0.0f, 1.0f,    0.0f,   "",   1.0f },
        { "release",   "Release",    20.0f, 4000.0f, 250.0f, "ms", 0.4f },
        { "volume",    "Volume",     -48.0f, 6.0f,  -3.0f,  "dB", 1.0f },
    };

    stella::Registrar registrar ({ "Physical", "Physical model", nullptr, 0, outputs, STELLA_COUNT (outputs), params, STELLA_COUNT (params),
                                   [] () -> stella::Module* { return new Physical(); } });
}
```

The "Model" parameter is 0 for the string and 1 to 6 for the materials (bar, marimba, bell, glass, membrane, plate), shown on the panel as a dropdown with those names.
