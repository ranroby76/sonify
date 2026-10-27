<!-- D:\workspace\Fanan_web_hub\src\skill_mix_genres.md -->
---
name: mix-genres
for: mixing
title: Genre mixing profiles
description: Each genre's mix and master targets (loudness, true peak, crest, tonal balance in octaves, width, low end, vocal placement, compression, space): read it first whenever the request names a genre or a style ("mix it as a club house hit").
---
# Genre mixing profiles

Targets for an **AI Mix Suggestion** or **AI Master Suggestion** in a genre. They are typical, not rules: a song's own character comes first, and the user decides.

**Tonal balance** is each octave of the master **relative to its 1 kHz octave** (bands_dbfs minus the 1 kHz value), as finished songs in the genre measure: 63 Hz, 125, 250, 500, 1k (0), 2k, 4k, 8k, 16k. Compare the song's shape with it and move the Mastering EQ gently (3 dB at most) where it's clearly off (over 2 dB). Below 63 Hz: the 31.5 Hz octave is usually 4-10 dB under the 63 Hz one; more and it rumbles.

**Loudness** is the master's integrated LUFS; **crest** its peak over loudness (dB): the higher, the more dynamic.

### Club house hit
- Loudness -8 to -7 LUFS, ceiling -1 dBTP, crest 7-9 dB.
- Balance: 63 +10..+13, 125 +7, 250 +3, 500 +1, 1k 0, 2k -2, 4k -4, 8k -6, 16k -11.
- Kick and bass rule the low end: kick leads at 63 Hz, bass at 125 Hz (bass dynamic EQ at 63 Hz keyed to the kick, 4-6 dB). Lows mono (lowWidth 0-30), highs wide (highWidth 120-130).
- Vocal up front and dry-ish, Plate short; Echo on the vocal throws only. Bus Compressor working (2-3 dB). Tape in, light.

### EDM / festival
- Loudness -7 to -6 LUFS, ceiling -1 dBTP, crest 6-8 dB.
- Balance: 63 +11..+14, 125 +7, 250 +2, 500 0, 1k 0, 2k -1, 4k -3, 8k -5, 16k -9 (bright top).
- Kick over everything at 63 Hz (bass and pads keyed to it). Very wide highs and pads (highWidth 125-135), lows mono. Multiband: tame the low band (2-3 dB). Big reverbs on leads, clean low end under them.

### Pop
- Loudness -10 to -8 LUFS, ceiling -1 dBTP, crest 8-10 dB.
- Balance: 63 +7..+9, 125 +6, 250 +3, 500 +1, 1k 0, 2k -2, 4k -4, 8k -6, 16k -11.
- The lead vocal is the song: loudest stem by 2-4 dB, compressed (3-6 dB), present (2-4 kHz) and airy; everything else keyed out of its way (guitars and keys dynamic EQ at 2-4 kHz and 250-500 Hz on the vocal). Plate on the vocal, Echo throws. Bus Compressor 1-2 dB.

### Hip hop / trap
- Loudness -8 to -7 LUFS, ceiling -1 dBTP, crest 7-9 dB.
- Balance: 63 +12..+15 (808 heavy), 125 +7, 250 +2, 500 0, 1k 0, 2k -1, 4k -3, 8k -5, 16k -10.
- The 808 or bass and the vocal lead; the kick keyed against the 808 (or the 808 at 63 Hz dipping under the kick) so they don't sum to mud. Vocal dry and loud, a short Plate; hats bright, panned a little. Lows strictly mono (lowWidth 0).

### R&B / soul
- Loudness -10 to -9 LUFS, ceiling -1 dBTP, crest 9-11 dB.
- Balance: 63 +8..+10, 125 +6, 250 +3, 500 +1, 1k 0, 2k -2, 4k -4, 8k -6, 16k -11 (smooth, not harsh).
- Warm, round low end, the vocal intimate and smooth (compressed, Plate long-ish, Echo). Tape in for warmth (drive 30-40). Avoid hard highs: no presence boosts over 2 dB.

### Rock
- Loudness -10 to -8 LUFS, ceiling -1 dBTP, crest 8-10 dB.
- Balance: 63 +5..+7, 125 +5, 250 +3, 500 +2, 1k 0, 2k -1, 4k -3, 8k -6, 16k -12 (mids forward).
- Guitars wide (double-tracked hard left and right: pan ±0.7-0.9), vocal on top of them (guitars keyed at 2-3 kHz to the vocal), kick and snare punchy, bass under the kick at 63 Hz. Bus Compressor 2 dB, Tape in.

### Metal
- Loudness -9 to -7 LUFS, ceiling -1 dBTP, crest 7-9 dB.
- Balance: 63 +5..+7, 125 +5, 250 +2, 500 +1, 1k 0, 2k 0, 4k -2, 8k -5, 16k -11.
- Tight lows: high-pass guitars at 90-120 Hz, cut their 250-400 Hz mud; kick clicky (4 kHz) and on top at 63 Hz. Guitars wide, vocal keyed space at 1-3 kHz. Multiband on the low band to keep it tight.

### Indie / alternative
- Loudness -11 to -9 LUFS, ceiling -1 dBTP, crest 9-11 dB.
- Balance close to Pop's with less air (16k -13) and a little more 250-500 Hz warmth.
- Character over polish: Tape in, more room (Plate on drums and vocal), moderate compression.

### Acoustic / folk / singer-songwriter
- Loudness -14 to -12 LUFS, ceiling -1 dBTP, crest 11-14 dB.
- Balance: 63 +3..+5, 125 +4, 250 +3, 500 +1, 1k 0, 2k -2, 4k -4, 8k -7, 16k -12.
- Natural and open: light compression (2-3 dB), voice and guitar each with their own room (guitar keyed at 2-3 kHz to the voice, gently), a natural Plate. Little or no multiband; the limiter only catching peaks (2-3 dB at most).

### Ballad
- Loudness -13 to -11 LUFS, ceiling -1 dBTP, crest 10-13 dB.
- Balance like Pop's, softer highs (8k -7, 16k -12).
- The voice carries it: compressed smoothly, a long Plate and Echo; piano or strings keyed under it at 1-3 kHz. Keep the dynamics: limiter 2-4 dB at most.

### Jazz
- Loudness -16 to -14 LUFS, ceiling -1 dBTP, crest 12-16 dB.
- Balance: 63 +3..+5, 125 +5, 250 +3, 500 +1, 1k 0, 2k -2, 4k -5, 8k -8, 16k -14.
- As played in the room: little compression, natural panning (as the band stands), a room-like Plate on everything, no loudness push.

### Classical / orchestral
- Loudness -20 to -16 LUFS, ceiling -1 dBTP, crest 15-20 dB.
- Balance: 63 +2..+4, 125 +4, 250 +3, 500 +1, 1k 0, 2k -2, 4k -5, 8k -9, 16k -15.
- Hardly any processing: balance and space only, no multiband, the limiter only as a safety.

### Cinematic / trailer
- Loudness -12 to -9 LUFS, ceiling -1 dBTP, crest 10-14 dB.
- Balance: 63 +9..+12, 125 +6, 250 +2, 500 0, 1k 0, 2k -2, 4k -4, 8k -6, 16k -11.
- Huge lows and space: big Plate, wide highs (highWidth 130), hits on top (everything keyed under the drums at 63-125 Hz).

### Lo-fi
- Loudness -14 to -11 LUFS, ceiling -1 dBTP, crest 9-12 dB.
- Balance: 63 +6..+8, 125 +6, 250 +4, 500 +2, 1k 0, 2k -3, 4k -6, 8k -10, 16k -18 (dark).
- Tape in hard (drive 50-70) on the board and the master, rolled-off highs (eqHigh -3..-5), mono-ish (midWidth 80-90), soft compression.

### Reggaeton / Latin urban
- Loudness -8 to -7 LUFS, ceiling -1 dBTP, crest 7-9 dB.
- Balance: 63 +11..+13, 125 +7, 250 +2, 500 0, 1k 0, 2k -1, 4k -3, 8k -5, 16k -10.
- The dembow kick and snare lead; bass keyed under the kick at 63 Hz; vocals loud and present with short Echo throws.

### Middle Eastern pop / hafla
- Loudness -10 to -8 LUFS, ceiling -1 dBTP, crest 8-10 dB.
- Balance: 63 +7..+9, 125 +5, 250 +3, 500 +2, 1k 0, 2k -1, 4k -3, 8k -6, 16k -11 (warm, mids forward for the voice and lead instruments).
- The singer and the lead instrument (oud, qanun, violin, ney, keyboard) lead in turn: key the lead instrument at 1-3 kHz to the voice so it fills in only between phrases. Darbuka and percussion bright and wide, the bass and kick warm. A generous Plate and Echo on the voice, as the style loves.

### Country
- Loudness -10 to -9 LUFS, ceiling -1 dBTP, crest 9-11 dB.
- Balance like Pop's, a little brighter at 4-8 kHz (+1 dB) for the acoustic guitars and fiddle.
- Clear vocal on top, acoustic guitars wide, bass and kick tight together.

### Podcast / spoken word
- Loudness -16 LUFS (stereo; -19 for mono-style shows), ceiling -1 dBTP, crest 10-14 dB.
- Voices clear: high-pass 80-100 Hz, a little 3 kHz presence, compression 4-6 dB, music beds keyed under the voices at 1-4 kHz (6-10 dB of range). Little reverb, narrow (midWidth 80-100).
