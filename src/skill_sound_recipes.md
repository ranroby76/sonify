<!-- D:\workspace\Fanan_web_hub\src\skill_sound_recipes.md -->
---
name: sound-recipes
for: plugins
title: Sound recipes
description: How classic and modern sounds are made (basses, acid, Reese, leads, supersaws, plucks, pads, strings, brass, keys, electric piano, bells, organ, choir, wobble, chiptune) with settings for the synth-engines instrument, what makes a patch sound professional (drift, velocity, key tracking, macros, width, headroom), and how to make presets that sell. Read it when the user names a sound, a genre or a famous instrument, or asks for presets.
---
# Sound recipes

Settings are for the synth-engines skill's `Synth.cpp` (engine, timbre, morph, cutoff, resonance, envelope amount, ADSR, glide, mono) with effects from the effects skills. They're starting points: build, then listen with `analyze_sound` and adjust by ear (the sound-critic skill).

## What every pro patch has
- **Movement.** Nothing in nature is static. Give each voice a slow random drift (about 2 to 4 cents, 0.2 to 0.5 Hz: a `dsp::Lfo` with `smoothRandom`, a different seed per voice), and put an LFO or an envelope on the filter, the pulse width or the engine's morph.
- **Velocity.** Always level (`0.35 + 0.65 * velocity`), and for expressive sounds the filter envelope amount or the brightness too.
- **Key tracking.** The filter follows the keyboard (about half: `cutoff * 2^((note - 60) / 24)`), so high notes don't go dull and low notes don't go thin.
- **Envelopes that fit.** Plucks: decay 120 to 300 ms, sustain 0. Pads: attack 0.4 to 1.5 s, release 1.5 to 4 s. Basses: attack under 5 ms, release under 150 ms. The filter envelope usually a little faster than the amp envelope.
- **Headroom.** One note about -18 to -12 dBFS RMS, a big chord peaking under -1 dBFS.
- **Stereo where it helps.** Pads, leads and keys wide (unison spread, chorus); basses and kicks mono below 150 Hz.
- **Macros.** One knob that moves several things the musician thinks of as one: "Brightness" (cutoff and envelope amount), "Space" (reverb mix and size), "Dirt" (drive and a little resonance), "Movement" (LFO depth on several targets). The panel's hero knobs.

## Basses
- **Sub bass**: `Morph` morph 0 (sine) or 0.15 (a touch of triangle for audibility on small speakers), mono, glide 0 to 30 ms, cutoff open, A 2 ms, R 80 ms. Pure low end: check the analyzer's "sub" and "bass" shares and the peaks.
- **Moog-style bass**: `Analog` timbre 0.15 (saw), sub 0.4, cutoff 300 Hz, resonance 0.35, envelope amount 0.6, A 2 ms, D 250 ms, S 0.3, R 120 ms, mono, glide 40 ms, a little drive (the ladder's drive 1.5 to 2.5).
- **Acid (303)**: `Analog` timbre 0 (saw) or 1 (square), mono, cutoff 350 to 600 Hz, resonance 0.8 to 0.9, envelope amount 0.7, D 180 ms, S 0. Accent = velocity: more envelope amount and level on hard notes. Slide = legato glide (60 ms) only when notes overlap. Follow with tube or fuzz saturation and a delay.
- **Reese**: `Supersaw` timbre 0.2 to 0.3, morph 0.6 (or two saws 10 to 15 cents apart), mono, ladder low-pass at 500 to 900 Hz with a slow LFO (0.1 to 0.3 Hz) on the cutoff, drive, then a chorus at low mix. Dark, wide, moving.
- **FM bass**: `Fm` ratio 1 (morph 0.15), depth 0.5, feedback 0.3, mono, cutoff open, A 1 ms, D 300 ms, S 0.5: a slap-like attack from the decaying depth.
- **Wobble**: `Analog` or `Supersaw`, mono, ladder with high resonance (0.6), an LFO on the cutoff (rate 1 to 8 Hz as a knob, square or sine), drive, a "Wobble rate" and "Wobble depth" on the panel.
- **808**: see the drum-synthesis skill's pitched 808.

## Leads
- **Supersaw lead (trance)**: `Supersaw` timbre 0.5, morph 0.7, 16 voices, cutoff 6 to 8 kHz, resonance 0.1, A 5 ms, R 300 ms, delay 25% (dotted eighth), reverb 25%.
- **Mono synth lead**: `Analog` timbre 0.3, mono, glide 60 to 120 ms, vibrato on the mod wheel, cutoff 2 kHz, envelope amount 0.3.
- **Sync / screaming lead**: hard sync (a slave `Osc` reset by the master, its pitch on a knob or an envelope), drive, cutoff open.
- **Chiptune**: `Morph` morph 0.95 to 1 (narrow pulse) or 0.75 (square), no filter, A 0, R 50 ms; then a crusher at 8 bits; arpeggios come from a MIDI device.

## Plucks and keys
- **Pluck**: `Analog` timbre 0.1, cutoff 300 Hz, envelope amount 0.8, A 1 ms, D 180 ms, S 0, R 200 ms, a delay. Or `Fold` with the filter envelope also driving the fold depth (west-coast pluck).
- **Electric piano**: two `Fm` engines per voice mixed: a body (ratio 1, depth 0.3) and a tine (ratio 14, depth 0.4, its own decay short); velocity raises the depth (the bark); then a tremolo (an LFO on the level, 4 to 6 Hz) or a chorus.
- **DX bell / glass**: `Fm` ratio 3.5, depth 0.6, long decay (2 to 4 s), sustain 0, release 1.5 s, reverb. Or the physical-models skill's bell.
- **Organ**: `Additive` timbre 0.55, morph 0.05; or drawbars as fixed harmonic levels (1, 2, 3, 4, 6, 8 at 0.8, 0.6, 0.5, 0.3, 0.2, 0.15), A and R 5 ms (organs click on: keep a 3 ms attack), then a rotary effect: a chorus at 6 Hz for fast, 0.8 Hz for slow, with a little tremolo.
- **Clavinet-ish**: `Analog` timbre 0.8 (square), pulse width 0.25 (morph 0.55), cutoff 1.5 kHz, resonance 0.3, envelope amount 0.5, D 300 ms, S 0.2, an auto-wah.

## Pads, strings, brass, voices
- **Warm pad**: `Analog` detuned (detune 10 to 14 cents), timbre 0.2, cutoff 1.2 kHz, resonance 0.15, an LFO (0.15 Hz) on the cutoff, A 0.8 s, R 2.5 s, chorus 40%, hall reverb 40%.
- **Huge pad**: `Supersaw` timbre 0.45, morph 0.8, 16 voices, A 1 s, R 3 s, ambient reverb with shimmer 0.3.
- **String machine**: `Analog` saw, detune 8 cents, A 300 ms, R 900 ms, a high-pass at 200 Hz (a `Biquad` low cut), the ensemble chorus (rate 0.6, depth 0.8, mix 60%), reverb.
- **Brass**: `Analog` saw, cutoff 600 Hz, envelope amount 0.6 with the filter attack at 60 to 120 ms (the swell), A 30 ms, S 0.8, R 200 ms, a delayed vibrato (it fades in after 300 ms).
- **Choir**: `Formant` morph 0.1 to 0.3 (a to e), timbre 0.4, vibrato 5 Hz at about 15 cents, a slow LFO on the vowel, A 400 ms, R 1.5 s, chorus and hall reverb. Several voices per note detuned a few cents make a section.

## Presets that sell
- Make them genuinely different: cover the plugin's range (a bass, a lead, a pad, a pluck, an FX), not five variations of one sound.
- Name them for what they evoke ("Velvet Sub", "Neon Arp", "Frozen Lake"), never "Preset 1".
- Set every preset's level so they match within 2 or 3 dB: a user compares presets by ear and louder always seems better.
- Save at most three per answer (the guide's limit), each with only the values that differ from the defaults.
