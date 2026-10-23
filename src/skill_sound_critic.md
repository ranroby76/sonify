<!-- D:\workspace\Fanan_web_hub\src\skill_sound_critic.md -->
---
name: sound-critic
for: plugins
title: Sound critic
description: How to listen to the plugin with analyze_sound and improve it one move at a time: which tests to run for each kind of plugin, what good numbers look like, how to turn the user's words (warmer, punchier, wider, darker, less muddy, more alive...) into concrete changes, trying them with "values" before touching code, and when to stop. Read it before judging or tuning any sound, and whenever the user describes a sound with feelings rather than settings.
---
# Sound critic

`analyze_sound` renders a copy of the plugin offline and measures it. It never changes anything and needs no approved plan, so use it freely to look before you change; but each call is one more step, so plan two or three at most per request.

## When to listen
- After the first build of a new sound, before you design the panel: catch silence, clipping, clicks, aliasing, wrong octaves and dead velocity early.
- When the user describes the sound in words ("too harsh", "make it fatter", "more vintage"): measure first, then change.
- Before you say a sound change is done.

## Which test
Instruments (`notes`, `velocity`, `hold`, `tail`; a soft pass is added by itself):
- One note at C3: the tone, envelope, movement, pitch, stereo and velocity response.
- One high note (C6, or C7 for leads): aliasing shows as "between its harmonics" (an aliasing oscillator about -15 dB; PolyBLEP about -30 dB; clean designs -40 dB or lower).
- A chord (C3, E3, G3, B3, D4): headroom (the peak must stay under about -1 dBFS) and how the voices add up.
- Basses: C1 and C2; leads: C4 and C5; pads: `hold` 3 and `tail` 4; plucks and physical models: `tail` 3 to 8 (they ring on).
- Drums: one drum note at a time with `"pitched": false` (36 kick, 38 snare, 42 closed hat...).

Effects and widgets (`signal`): "sine" for distortion (harmonics, even against odd, aliasing), "drums" for compressors and transient shapers (peak and crest change), "noise" for filters and EQs (the tone change per band), "chord" for chorus, phaser and width (movement, sides, correlation), "impulse" for reverbs and delays (first echo, RT60), "music" for the overall result.

## What good looks like
- **Level**: one note about -18 to -12 dBFS RMS, peaks about -6 dBFS; a full chord under -1 dBFS; effects within 1 to 2 dB of their input at the default settings (unless they're meant to be louder or quieter).
- **Problems that are always worth fixing**: samples that aren't numbers, going over 0 dBFS, DC offset, a click when the note ends, a wrong octave, out of tune (more than 15 cents), notes that never stop, aliasing on a high note or a sine (above about -30 dB between harmonics for an oscillator, above about -50 dB for a saturator), out of phase.
- **Tone, by sound** (the energy shares): a sub bass lives almost entirely in sub and bass; a full bass has 10 to 30% in low-mid and mid for presence on small speakers; pads and keys centre in the mids; leads have real energy in mid and high-mid. Brightness (the centre of the energy) for a full synth note: under about 500 Hz is dark, 500 to 1200 Hz warm, 1.5 to 3 kHz bright, above 4 kHz harsh or thin.
- **Movement**: a held pad or lead whose brightness varies less than about 3% is static: usually it wants an LFO, drift or an evolving filter. A bass or an organ can be static.
- **Velocity**: a soft note about 6 to 12 dB quieter, and for expressive sounds 10 to 30% darker. "The same at every velocity" is worth fixing in any played instrument.
- **Stereo**: an instrument's correlation 0.3 to 0.9; a reverb tail near 0 is fine; mono loss under about 3 dB.

## Words into moves
- **Warmer**: less 2 to 5 kHz (lower the cutoff a little, or a gentle high shelf down), a touch of tube saturation, a slower filter envelope. Brightness should drop 10 to 30%.
- **Brighter / more presence**: more cutoff or envelope amount, a high shelf up, less damping; on effects, tone up.
- **Darker / softer**: lower cutoff, less resonance, a softer exciter (mallets, plucks), more damping.
- **Punchier**: a faster amp attack (1 to 3 ms), a shorter, stronger filter envelope, a transient shaper's attack up, a compressor with a slower attack (5 to 20 ms).
- **Fatter / bigger**: detune or unison, a sub an octave down, gentle saturation, more 100 to 300 Hz, width on the upper part.
- **Thinner / cleaner**: fewer voices or less detune, a low cut at 100 to 200 Hz, less drive.
- **Wider**: detune spread across left and right, a chorus, the width processor above 150 Hz. Never invert one side. Check that the correlation stays above about 0.3.
- **Tighter**: shorter release, less reverb, mono low end, a faster decay.
- **Smoother / less harsh**: cut 2.5 to 5 kHz, lower resonance, oversample the drive, slower attack.
- **Less muddy**: cut 200 to 400 Hz (wide), high-pass anything that doesn't need lows.
- **More aggressive**: drive, resonance, folding, more high-mid; a little odd-harmonic saturation.
- **Vintage / analog**: drift (2 to 4 cents per voice), gentle saturation, a soft low-pass around 10 to 12 kHz, chorus, slightly slower envelopes.
- **Modern / clean**: no drift, precise envelopes, wide but mono-safe, sharp transients.
- **Airy**: a high shelf up at 10 kHz, a quiet noise layer, a bright plate reverb.
- **Deeper**: more sub, lower cutoff, a longer decay.
- **More alive**: an LFO on the filter, pulse width or morph; drift; velocity on brightness; mod wheel on vibrato or filter.

## The loop: one move at a time
1. Listen to it as it is (the baseline).
2. Choose one move that answers the user's words.
3. Try it without changing anything: the same test with `"values": { "<id>": <value> }`. Compare the numbers that matter (brightness, the band shares, the peaks, the movement).
4. If it moves the right way, make it real: knob values become the defaults (or a preset), structural changes become code (inside the approved plan).
5. Stop when the problem is gone or after about three tries. Your ears are numbers here; the user's ears win: never argue with what they hear.

## Telling the user
Plain words about the sound: "I warmed it up and softened the highs, and it answers your touch more now." No numbers, units or tool names unless they ask how it measures.
