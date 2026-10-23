<!-- D:\workspace\Fanan_web_hub\src\skill_drum_synthesis.md -->
---
name: drum-synthesis
for: plugins
title: Drum synthesis
description: Tested analog-style drum voices (kick, snare, hi-hats with choke, clap, toms, cymbals, cowbell, rim) and a complete drum-machine module on General MIDI notes, with the recipes for 808, 909, trap, techno and lo-fi kits and a pitched 808 bass. Read it for drum machines, percussion instruments and kick or 808 synths.
---
# Drum synthesis

`modules/Drums.h` holds the drum voices (written against the dsp-toolkit skill's `Dsp.h`: write it first); `modules/DrumKit.cpp` is a complete drum machine. Every voice is monophonic, like the hardware: `trigger (velocity)` on a hit, `next()` per sample, and a 1 ms declick so a retrigger never clicks.

## The voices
- `Kick`: a sine whose pitch falls from a punch (about 12 ms), a noise click, and drive. `tune` 45 to 60 Hz for most kicks; `decay` 0.15 s (909, techno) to 1.5 s (808, trap); `punch` 0.3 (round) to 0.9 (snappy); `drive` adds harmonics so a deep kick reads on small speakers.
- `Snare`: two head tones (the second at 1.72x, pitch dropping as it settles) plus band-passed noise for the wires, brighter on hard hits. `snappy` balances wires against head.
- `Metal`: six square waves at the TR-808's ratios through a band-pass and a high-pass: closed hats (decay 0.04 to 0.1 s), open hats (0.3 to 0.6 s), crash (1 to 2.5 s, darker tone), ride (0.8 to 1.5 s, brighter, quieter). `choke()` (or a retrigger with a short decay) cuts an open hat when the closed one plays.
- `Clap`: noise bursts at 0, 10, 21 and 32 ms, then a tail: the "hands not quite together" sound.
- `Tom`: a kick-like body higher up with a drum head's second mode (1.594x).
- `Cowbell`: two squares at 540 and 800 Hz, band-passed, with a quick drop and a ring.

## The kit
`DrumKit.cpp` plays General MIDI drum notes: 36 kick, 37 rim, 38 snare, 39 clap, 42 closed hat, 44 pedal hat, 46 open hat, 41/43, 45/47 and 48/50 toms, 49 crash, 51 ride, 56 cowbell. The panel's keyboard (or pads, as a programmed element) plays them. When you listen with `analyze_sound`, play one drum at a time (`"notes": ["C1"]` is 36, the kick) with `"pitched": false`.

## Recipes
- **808**: kick tune 48, decay 1.2, punch 0.35, drive 0.15; snare tune 180, snappy 0.5, decay 0.2; closed hat 0.05, open 0.5, tone 0.6; clap decay 0.25; cowbell on.
- **909**: kick tune 55, decay 0.3, punch 0.7, drive 0.35 (punchy and short); snare tune 230, snappy 0.75; hats brighter (tone 0.8), closed 0.04; crash 1.8 s.
- **Trap**: kick or a separate 808 bass (below) with decay 1.5 to 2.5 s and drive 0.4; snare and clap layered (hit both, the clap slightly quieter); hats closed 0.03 with rolls from the MIDI.
- **Techno**: kick tune 50, decay 0.4, punch 0.6, drive 0.6 (a saturated thump); hats tone 0.4 (darker), open 0.25; a rumble is a reverb on the kick, low-passed, ducked.
- **Lo-fi**: everything darker (hats tone 0.2, snare snappy 0.3), then a character effect: bit reduction, a low-pass at 6 to 8 kHz, tape saturation (the effects-character skill).

## A pitched 808 bass
A playable 808 is one `Kick` per voice whose `tune` follows the note: `kick.tune = stella::noteToHz (note)`; play it mono with glide (slide the pitch while the old note still sounds: retrigger only when no note was held), decay 1 to 3 s, drive 0.3 to 0.6, punch 0.2 so it doesn't "boing". A sub-heavy sound: judge it by its peaks and on several notes (C1 to C3).

## Making a kit sound modern
- Level-match the voices on the panel: the kick and snare peaks about equal, hats 6 to 10 dB under.
- Velocity always does two things: level, and brightness or punch (already in the voices).
- One saturation stage across the whole kit (`fastTanh` with a drive knob) glues it; a transient knob (the effects-character skill) makes it hit harder.
- Give each drum its own level and decay on the panel, grouped by drum; tune knobs for kick, snare and toms.

## Drums.h
```cpp
// modules/Drums.h: analog-style drum voices (808 / 909 family), each monophonic: trigger()
// on a hit, next() per sample. Written against Dsp.h.
#pragma once
#include "Dsp.h"

namespace drums
{
    using stella::pi;
    using stella::twoPi;

    /** An exponential decay: -60 dB after seconds. */
    inline float decayCoef (float seconds, float rate) noexcept    { return std::pow (0.001f, 1.0f / (std::fmax (0.002f, seconds) * rate)); }

    /** Smooths a retrigger: what was sounding fades out over a millisecond instead of jumping. */
    struct Declick
    {
        float tail = 0.0f, coef = 0.98f;
        void prepare (float rate) noexcept           { coef = std::exp (-1.0f / (0.001f * rate)); }
        void trigger (float lastOut) noexcept        { tail = lastOut; }
        float apply (float x) noexcept               { tail *= coef; return x + tail; }
    };

    //==========================================================================
    /** Kick: a sine whose pitch falls fast from a punch, a click, and drive.
        tune: Hz of the body (40..90); decay: seconds (0.1 short 909 .. 1.5 long 808);
        punch: how far the pitch falls from (0..1); drive: 0 clean .. 1 saturated. */
    struct Kick
    {
        float rate = 48000.0f, phase = 0.0f, env = 0.0f, pitchEnv = 0.0f, click = 0.0f, last = 0.0f, velocity = 1.0f;
        float tune = 50.0f, decay = 0.5f, punch = 0.5f, drive = 0.2f, ampCoef = 0.999f, pitchCoef = 0.99f, clickCoef = 0.9f;
        dsp::Noise noise;
        dsp::OnePole clickTone;
        Declick declick;

        void prepare (float sampleRate) noexcept
        {
            rate = sampleRate;
            declick.prepare (rate);
            clickTone.set (6000.0f, rate);
            pitchCoef = std::exp (-1.0f / (0.012f * rate));   // the punch falls in ~12 ms
            clickCoef = std::exp (-1.0f / (0.0015f * rate));
        }

        void trigger (float v) noexcept
        {
            declick.trigger (last);
            velocity = v;
            phase = 0.0f;
            env = 1.0f;
            pitchEnv = 1.0f;
            click = 1.0f;
            ampCoef = decayCoef (decay, rate);
        }

        float next() noexcept
        {
            const float hz = tune * (1.0f + punch * 5.0f * velocity * pitchEnv);
            phase += hz / rate;
            if (phase >= 1.0f) phase -= 1.0f;

            float body = std::sin (twoPi * phase) * env;
            const float tick = clickTone.highpass (noise.next()) * click * 0.5f * velocity;
            body = dsp::fastTanh (body * (1.0f + drive * 4.0f)) / dsp::fastTanh (1.0f + drive * 4.0f);

            env *= ampCoef; pitchEnv *= pitchCoef; click *= clickCoef;
            last = declick.apply ((body + tick) * (0.4f + 0.6f * velocity));
            return last;
        }
    };

    //==========================================================================
    /** Snare: two drum-head tones and a burst of bright noise (the wires).
        tune: Hz of the head (150..300); snappy: wires against head (0..1); decay: seconds. */
    struct Snare
    {
        float rate = 48000.0f, p1 = 0.0f, p2 = 0.0f, toneEnv = 0.0f, noiseEnv = 0.0f, last = 0.0f, velocity = 1.0f;
        float tune = 190.0f, snappy = 0.6f, decay = 0.25f, toneCoef = 0.999f, noiseCoef = 0.999f;
        dsp::Noise noise;
        dsp::Svf wires;
        dsp::OnePole air;
        Declick declick;

        void prepare (float sampleRate) noexcept    { rate = sampleRate; declick.prepare (rate); air.set (500.0f, rate); }

        void trigger (float v) noexcept
        {
            declick.trigger (last);
            velocity = v;
            p1 = p2 = 0.0f;
            toneEnv = noiseEnv = 1.0f;
            toneCoef = decayCoef (decay * 0.45f, rate);
            noiseCoef = decayCoef (decay, rate);
            wires.set (3500.0f + 3000.0f * v, 0.3f, rate);   // a harder hit is brighter
        }

        float next() noexcept
        {
            const float bend = 1.0f + 0.5f * toneEnv * toneEnv;   // the head's pitch drops as it settles
            p1 += tune * bend / rate;          if (p1 >= 1.0f) p1 -= 1.0f;
            p2 += tune * 1.72f * bend / rate;  if (p2 >= 1.0f) p2 -= 1.0f;

            const float head = (std::sin (twoPi * p1) + 0.5f * std::sin (twoPi * p2)) * toneEnv;
            const float n = air.highpass (wires.bandpass (noise.next())) * noiseEnv;

            toneEnv *= toneCoef; noiseEnv *= noiseCoef;
            last = declick.apply ((head * (1.0f - 0.6f * snappy) + n * 2.2f * snappy) * (0.35f + 0.65f * velocity) * 0.6f);
            return last;
        }
    };

    //==========================================================================
    /** Hi-hat and cymbal: six square waves at the 808's clashing ratios, through a band-pass
        and a high-pass: metallic noise. tone: brightness (0..1); decay: seconds (closed 0.05,
        open 0.4, crash 1.5). choke() cuts it (a closed hat cuts the open one). */
    struct Metal
    {
        float rate = 48000.0f, env = 0.0f, coef = 0.999f, last = 0.0f, velocity = 1.0f, tone = 0.5f, decay = 0.08f, chokeCoef = 0.99f;
        bool choking = false;
        dsp::Osc squares[6];
        dsp::Svf band;
        dsp::OnePole high;
        Declick declick;

        void prepare (float sampleRate) noexcept
        {
            rate = sampleRate;
            declick.prepare (rate);
            chokeCoef = std::exp (-1.0f / (0.004f * rate));
            static const float ratios[6] { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };   // Hz in the 808

            for (int i = 0; i < 6; ++i)
            {
                squares[i].setFrequency (ratios[i] * 1.7f, rate);
                squares[i].reset ((float) i * 0.13f);
            }
        }

        void trigger (float v) noexcept
        {
            declick.trigger (last);
            velocity = v;
            env = 1.0f;
            choking = false;
            coef = decayCoef (decay, rate);
            band.set (6000.0f + 4000.0f * tone + 2000.0f * v, 0.55f, rate);   // a harder hit is brighter
            high.set (5000.0f + 3000.0f * tone, rate);
        }

        void choke() noexcept    { choking = true; }

        float next() noexcept
        {
            float sum = 0.0f;

            for (auto& s : squares)
                sum += s.next (dsp::Osc::square);

            const float y = high.highpass (band.bandpass (sum * 0.3f)) * env;
            env *= choking ? chokeCoef : coef;
            last = declick.apply (y * (0.3f + 0.7f * velocity) * 5.0f);
            return last;
        }
    };

    //==========================================================================
    /** Clap: a few quick bursts of band-passed noise (hands not quite together), then a tail.
        decay: the tail's seconds (0.1 .. 0.5). */
    struct Clap
    {
        float rate = 48000.0f, env = 0.0f, tailEnv = 0.0f, burstCoef = 0.99f, tailCoef = 0.999f, last = 0.0f, velocity = 1.0f, decay = 0.2f;
        int sinceStart = 1 << 30;
        dsp::Noise noise;
        dsp::Svf band;
        Declick declick;

        void prepare (float sampleRate) noexcept
        {
            rate = sampleRate;
            declick.prepare (rate);
            burstCoef = std::exp (-1.0f / (0.004f * rate));
            band.set (1300.0f, 0.45f, rate);
        }

        void trigger (float v) noexcept
        {
            declick.trigger (last);
            velocity = v;
            sinceStart = 0;
            env = 1.0f;
            tailEnv = 0.0f;
            tailCoef = decayCoef (decay, rate);
        }

        float next() noexcept
        {
            // Bursts at 0, 10, 21 and 32 ms; the tail starts with the last.
            if (sinceStart > 0 && (sinceStart == (int) (0.010f * rate) || sinceStart == (int) (0.021f * rate)))
                env = 0.9f;

            if (sinceStart == (int) (0.032f * rate))
                tailEnv = 0.8f;

            ++sinceStart;

            const float y = band.bandpass (noise.next()) * (env + tailEnv);
            env *= burstCoef;
            tailEnv *= tailCoef;
            last = declick.apply (y * (0.4f + 0.6f * velocity) * 2.4f);
            return last;
        }
    };

    //==========================================================================
    /** Tom: a kick-like body higher up, with a drum head's second mode and a touch of noise.
        tune: Hz (80..300); decay: seconds. */
    struct Tom
    {
        float rate = 48000.0f, p1 = 0.0f, p2 = 0.0f, env = 0.0f, pitchEnv = 0.0f, coef = 0.999f, pitchCoef = 0.99f, last = 0.0f, velocity = 1.0f;
        float tune = 120.0f, decay = 0.4f;
        dsp::Noise noise;
        dsp::OnePole soft;
        Declick declick;

        void prepare (float sampleRate) noexcept
        {
            rate = sampleRate;
            declick.prepare (rate);
            pitchCoef = std::exp (-1.0f / (0.05f * rate));
            soft.set (2000.0f, rate);
        }

        void trigger (float v) noexcept
        {
            declick.trigger (last);
            velocity = v;
            p1 = p2 = 0.0f;
            env = pitchEnv = 1.0f;
            coef = decayCoef (decay, rate);
        }

        float next() noexcept
        {
            const float hz = tune * (1.0f + 0.6f * pitchEnv * velocity);
            p1 += hz / rate;           if (p1 >= 1.0f) p1 -= 1.0f;
            p2 += hz * 1.594f / rate;  if (p2 >= 1.0f) p2 -= 1.0f;
            const float y = (std::sin (twoPi * p1) + 0.35f * std::sin (twoPi * p2) * pitchEnv + 0.08f * soft.lowpass (noise.next())) * env;
            env *= coef; pitchEnv *= pitchCoef;
            last = declick.apply (y * (0.4f + 0.6f * velocity) * 0.8f);
            return last;
        }
    };

    //==========================================================================
    /** Cowbell: two square waves a fifth-ish apart (540 and 800 Hz), band-passed, a short ring. */
    struct Cowbell
    {
        float rate = 48000.0f, env = 0.0f, coef = 0.999f, last = 0.0f, velocity = 1.0f, decay = 0.3f;
        dsp::Osc a, b;
        dsp::Svf band;
        Declick declick;

        void prepare (float sampleRate) noexcept
        {
            rate = sampleRate;
            declick.prepare (rate);
            a.setFrequency (540.0f, rate);
            b.setFrequency (800.0f, rate);
            band.set (2600.0f, 0.5f, rate);
        }

        void trigger (float v) noexcept    { declick.trigger (last); velocity = v; env = 1.0f; coef = decayCoef (decay, rate); }

        float next() noexcept
        {
            const float y = band.bandpass (a.next (dsp::Osc::square) + b.next (dsp::Osc::square)) * env * env;   // env squared: a quick drop, then a ring
            env *= coef;
            last = declick.apply (y * (0.4f + 0.6f * velocity) * 0.9f);
            return last;
        }
    };
}
```

## DrumKit.cpp
```cpp
// modules/DrumKit.cpp: a complete analog-style drum machine on General MIDI notes (36 kick,
// 38 snare, 39 clap, 42/44/46 hats, 41-50 toms, 49 crash, 51 ride, 56 cowbell, 37 rim).
#include "Drums.h"

class DrumKit final : public stella::Module
{
public:
    void prepare (double sampleRate, int) override
    {
        rate = (float) sampleRate;
        kick.prepare (rate); snare.prepare (rate); hat.prepare (rate); crash.prepare (rate); ride.prepare (rate);
        clap.prepare (rate); cowbell.prepare (rate); rim.prepare (rate);
        for (auto& t : toms) t.prepare (rate);
        dcL.prepare (rate); dcR.prepare (rate);
        volume.prepare (sampleRate, 30.0f);
        volume.snap (stella::dbToGain (param (P_volume)));
    }

    void process (const stella::Context& c, const float* const*, float* const* out) override
    {
        // The knobs, once a block (each drum reads them when it's hit).
        kick.tune = param (P_kickTune); kick.decay = param (P_kickDecay); kick.punch = param (P_kickPunch); kick.drive = param (P_kickDrive);
        snare.tune = param (P_snareTune); snare.snappy = param (P_snareSnappy); snare.decay = param (P_snareDecay);
        clap.decay = param (P_clapDecay);
        hat.tone = param (P_hatTone);
        crash.tone = param (P_hatTone) * 0.6f; crash.decay = param (P_cymbalDecay);
        ride.tone = 0.3f + param (P_hatTone) * 0.7f; ride.decay = param (P_cymbalDecay) * 0.7f;
        rim.tune = 420.0f; rim.decay = 0.05f;

        for (int t = 0; t < 3; ++t)
        {
            static const float ratios[3] { 1.0f, 1.33f, 1.78f };
            toms[t].tune = param (P_tomTune) * ratios[t];
            toms[t].decay = param (P_tomDecay);
        }

        int n = 0;

        for (int i = 0; i < c.numFrames; ++i)
        {
            for (; n < c.numNotes && c.notes[n].frame <= i; ++n)
                if (c.notes[n].on)
                    hit (c.notes[n].note, c.notes[n].velocity);

            const float k = kick.next(), s = snare.next(), h = hat.next(), cr = crash.next(), rd = ride.next();
            const float cl = clap.next(), cb = cowbell.next(), rm = rim.next();
            const float t0 = toms[0].next(), t1 = toms[1].next(), t2 = toms[2].next();

            // A drummer's view: kick, snare, clap in the middle; hats right; toms across; cymbals wide.
            const float centre = k + s + cl + rm;
            const float left = centre + h * 0.45f + t0 * 0.65f + t1 * 0.5f + t2 * 0.35f + cr * 0.55f + rd * 0.45f + cb * 0.45f;
            const float right = centre + h * 0.55f + t0 * 0.35f + t1 * 0.5f + t2 * 0.65f + cr * 0.45f + rd * 0.55f + cb * 0.55f;

            const float g = volume.next (stella::dbToGain (param (P_volume)));
            out[0][i] = dsp::fastTanh (dcL.process (left * g));
            out[1][i] = dsp::fastTanh (dcR.process (right * g));
        }
    }

private:
    enum { P_kickTune, P_kickDecay, P_kickPunch, P_kickDrive, P_snareTune, P_snareSnappy, P_snareDecay, P_clapDecay,
           P_hatTone, P_hatClosed, P_hatOpen, P_cymbalDecay, P_tomTune, P_tomDecay, P_volume };

    void hit (int note, float velocity)
    {
        switch (note)
        {
            case 35: case 36:  kick.trigger (velocity); break;
            case 37:           rim.trigger (velocity); break;
            case 38: case 40:  snare.trigger (velocity); break;
            case 39:           clap.trigger (velocity); break;
            case 42:           hat.decay = param (P_hatClosed); hat.trigger (velocity); break;           // closed: cuts the open hat
            case 44:           hat.decay = param (P_hatClosed) * 0.7f; hat.trigger (velocity * 0.7f); break;   // pedal
            case 46:           hat.decay = param (P_hatOpen); hat.trigger (velocity); break;
            case 41: case 43:  toms[0].trigger (velocity); break;
            case 45: case 47:  toms[1].trigger (velocity); break;
            case 48: case 50:  toms[2].trigger (velocity); break;
            case 49: case 57:  crash.trigger (velocity); break;
            case 51: case 59:  ride.trigger (velocity * 0.6f); break;
            case 56:           cowbell.trigger (velocity); break;
            default:           break;
        }
    }

    drums::Kick kick;
    drums::Snare snare;
    drums::Metal hat, crash, ride;
    drums::Clap clap;
    drums::Tom toms[3], rim;
    drums::Cowbell cowbell;
    dsp::DcBlock dcL, dcR;
    stella::Smoother volume;
    float rate = 48000.0f;
};

namespace
{
    const char* const outputs[] { "out L", "out R" };

    const stella::Param params[]
    {
        { "kick_tune",    "Kick tune",    35.0f, 100.0f, 52.0f,  "Hz", 1.0f },
        { "kick_decay",   "Kick decay",   0.08f, 2.0f,   0.55f,  "s",  0.5f },
        { "kick_punch",   "Kick punch",   0.0f,  1.0f,   0.5f,   "",   1.0f },
        { "kick_drive",   "Kick drive",   0.0f,  1.0f,   0.2f,   "",   1.0f },
        { "snare_tune",   "Snare tune",   140.0f, 320.0f, 190.0f, "Hz", 1.0f },
        { "snare_snappy", "Snappy",       0.0f,  1.0f,   0.6f,   "",   1.0f },
        { "snare_decay",  "Snare decay",  0.06f, 0.6f,   0.22f,  "s",  0.5f },
        { "clap_decay",   "Clap decay",   0.06f, 0.6f,   0.22f,  "s",  0.5f },
        { "hat_tone",     "Hat tone",     0.0f,  1.0f,   0.5f,   "",   1.0f },
        { "hat_closed",   "Closed hat",   0.02f, 0.25f,  0.06f,  "s",  0.5f },
        { "hat_open",     "Open hat",     0.1f,  1.5f,   0.45f,  "s",  0.5f },
        { "cymbal_decay", "Cymbals",      0.4f,  4.0f,   1.6f,   "s",  0.5f },
        { "tom_tune",     "Tom tune",     60.0f, 200.0f, 100.0f, "Hz", 1.0f },
        { "tom_decay",    "Tom decay",    0.1f,  1.2f,   0.4f,   "s",  0.5f },
        { "volume",       "Volume",       -48.0f, 6.0f,  -3.0f,  "dB", 1.0f },
    };

    stella::Registrar registrar ({ "DrumKit", "Drum kit", nullptr, 0, outputs, STELLA_COUNT (outputs), params, STELLA_COUNT (params),
                                   [] () -> stella::Module* { return new DrumKit(); } });
}
```
