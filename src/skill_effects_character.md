<!-- D:\workspace\Fanan_web_hub\src\skill_effects_character.md -->
---
name: effects-character
for: plugins
title: Character effects
description: Tested tone and dynamics effects: oversampled saturation (tape, tube, console, fuzz) with emphasis, bit crushing, RBJ EQ filters, a soft-knee compressor with make-up, a transient shaper and an auto-wah, plus a complete channel-strip module. Read it for distortion, saturation, lo-fi, EQ, compressor, transient and channel-strip effects.
---
# Character effects

`modules/Character.h` (written against the dsp-toolkit skill's `Dsp.h` and `DspFx.h`: write those first) and a complete channel strip, `modules/Character.cpp`. Every processor handles one channel: keep one per channel (an array of two), and link the detectors of a stereo compressor (the louder side drives both).

## The processors
- `Saturator` (`type` 0 tape, 1 tube, 2 console, 3 fuzz; `drive` dB; `tone` -1..1): lifts the highs before the curve and lowers them after (the emphasis trick: the drive bites mids and highs without fizz), 2x oversampled, DC-blocked. Measured on a sine at 18 dB of drive: tape and console add odd harmonics (-15 to -16 dB), tube adds even and odd alike (-19 / -17 dB: warm), fuzz is the hardest (-12 dB) and, even oversampled, the only one near the aliasing line (-53 dB): keep fuzz's drive to taste, or oversample it 4x (two `Oversampler2x` in a row) for bright sources.
- `Crusher` (bits 1..16, downsample 1..64): lo-fi on purpose. Its aliasing is the sound; follow it with a gentle low-pass (`OnePole` at 6 to 10 kHz) for a vintage sampler rather than a broken one.
- `Biquad` (RBJ cookbook: low and high shelves, peaks, low and high cuts): EQs and tone stacks. Set coefficients once a block, smoothing big jumps.
- `Compressor` (threshold, ratio, attack, release, knee; `makeupDb()`): feed it the louder of the two channels, multiply both by its gain. Fast attack (0.1 to 1 ms) tames peaks; slow attack (5 to 30 ms) lets the hit through and squashes the body (punchy, but the peaks rise: test with `analyze_sound` on "drums" and watch the peak). Ratio 20 with a fast attack is a limiter. Show `reductionDb` on a meter (a display).
- `Transients` (`attack`, `sustain` -1..1): more or less snap and tail at any level; +1 attack adds up to 6 dB on hits, so lower the output to match.
- `AutoWah` (sensitivity, low and high Hz, resonance): a band-pass that follows the playing: funk guitar, quacky bass, talking synths.

## Recipes
- **Warm bus glue**: console at 3 to 6 dB of drive, compressor ratio 2, attack 10 ms, release 100 ms, threshold for 2 to 3 dB of reduction.
- **Tape**: tape at 6 to 12 dB, tone -0.3, plus the space skill's tape delay's wobble at a low setting for flutter, and a high cut at 12 kHz.
- **Tube preamp**: tube at 8 to 15 dB, low shelf +2 dB at 110 Hz, high shelf -2 dB: thick and soft.
- **Fuzz pedal**: fuzz at 20 to 30 dB, a mid peak +4 dB at 800 Hz before it, a high cut at 5 kHz after it.
- **Lo-fi sampler**: crusher at 12 bits and downsample 2 to 3, a low-pass at 8 kHz, tape saturation, a little wobble.
- **Drum smasher**: compressor ratio 8, attack 0.5 ms, release 60 ms, threshold -30 dB, mixed 40% with the dry (parallel compression: the strip's Mix knob).
- **De-mud**: a peak -3 dB at 300 Hz (wide, Q 0.7); **air**: a high shelf +3 dB at 10 kHz.

## Tips
- Gain staging: drive changes loudness. Each saturator compensates (1/sqrt of the drive), but give the plugin an Output knob, and keep in and out levels close so the user hears the character, not just "louder".
- Mix knobs (dry/wet) make every effect friendlier: parallel saturation and compression keep the original transients.
- Always test distortion with `analyze_sound` and the "sine" signal: it reports the harmonics (even = warm, odd = edgy) and the aliasing; and "music" for the overall level change.

## Character.h
```cpp
// modules/Character.h: effects that shape tone and dynamics: saturation (tape, tube, console,
// fuzz), bit crushing, EQ, compression, transient shaping, auto-wah. Written against Dsp.h
// and DspFx.h. Each processes one channel: make one per channel (stereo: two).
#pragma once
#include "Dsp.h"
#include "DspFx.h"

namespace character
{
    using stella::pi;
    using stella::twoPi;

    //==========================================================================
    /** The saturation curves, x in, y out (about unity gain for small signals). */
    namespace curves
    {
        /** Tape: soft and symmetric, odd harmonics, rounds peaks gently. */
        inline float tape (float x) noexcept      { return dsp::fastTanh (x); }

        /** Tube: asymmetric (a bias), so even harmonics too: warm, a little fat. */
        inline float tube (float x) noexcept
        {
            constexpr float bias = 0.35f;
            return (dsp::fastTanh (x + bias) - dsp::fastTanh (bias)) * 1.12f;
        }

        /** Console: a sine curve, the gentlest (the "analog summing" glue). */
        inline float console (float x) noexcept
        {
            x = stella::clamp (x, -1.5707963f, 1.5707963f);
            return std::sin (x);
        }

        /** Fuzz: hard and square-ish, then softened at the corners. */
        inline float fuzz (float x) noexcept
        {
            const float hard = stella::clamp (x * 3.0f, -1.0f, 1.0f);
            return 0.7f * hard + 0.3f * dsp::fastTanh (x * 2.0f);
        }
    }

    /** Saturation with the classic emphasis trick: highs are lifted before the curve and lowered
        after, so drive bites the mids and highs without fizz; oversampled so it doesn't alias.
        type 0 tape, 1 tube, 2 console, 3 fuzz; drive in dB (0 .. 36); tone -1 dark .. 1 bright. */
    struct Saturator
    {
        dsp::Oversampler2x os;
        dsp::OnePole pre, post;
        dsp::DcBlock dc;
        float rate = 48000.0f;

        void prepare (float sampleRate) noexcept
        {
            rate = sampleRate;
            pre.set (2500.0f, rate);
            post.set (2500.0f, rate);
            dc.prepare (rate);
        }

        float process (float x, int type, float driveDb, float tone) noexcept
        {
            const float drive = stella::dbToGain (driveDb);
            const float emphasis = 0.5f + 0.5f * stella::clamp (tone, -1.0f, 1.0f);   // how much the highs are lifted first

            // Lift the highs, saturate, then put them back where they were.
            const float lifted = x + (x - pre.lowpass (x)) * emphasis;
            const float makeup = 1.0f / std::sqrt (drive);

            const float y = os.process (lifted, [type, drive, makeup] (float v)
            {
                const float d = v * drive;
                switch (type)
                {
                    case 1:  return curves::tube (d) * makeup;
                    case 2:  return curves::console (d) * makeup;
                    case 3:  return curves::fuzz (d) * makeup;
                    default: return curves::tape (d) * makeup;
                }
            });

            const float lowered = y - (y - post.lowpass (y)) * (emphasis / (1.0f + emphasis));
            return dc.process (lowered);
        }
    };

    //==========================================================================
    /** Bit crusher: fewer bits (grit) and a lower sample rate (aliasing on purpose: lo-fi, chip,
        broken). bits 1..16, downsample 1..64 (hold each sample that many times). */
    struct Crusher
    {
        float held = 0.0f, counter = 0.0f;

        float process (float x, float bits, float downsample) noexcept
        {
            counter += 1.0f;

            if (counter >= downsample)
            {
                counter -= downsample;
                const float steps = std::pow (2.0f, stella::clamp (bits, 1.0f, 16.0f) - 1.0f);
                held = std::round (x * steps) / steps;
            }

            return held;
        }
    };

    //==========================================================================
    /** A biquad with the RBJ "Audio EQ Cookbook" designs: low and high shelves, peaks, cuts. */
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;

        enum Type { lowShelf, highShelf, peak, lowCut, highCut };

        void set (Type type, float hz, float gainDb, float q, float rate) noexcept
        {
            const float w = twoPi * stella::clamp (hz, 10.0f, rate * 0.45f) / rate;
            const float cw = std::cos (w), sw = std::sin (w);
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float alpha = sw / (2.0f * std::fmax (0.1f, q));
            float n0 = 1, n1 = 0, n2 = 0, d0 = 1, d1 = 0, d2 = 0;

            switch (type)
            {
                case peak:
                    n0 = 1 + alpha * A; n1 = -2 * cw; n2 = 1 - alpha * A;
                    d0 = 1 + alpha / A; d1 = -2 * cw; d2 = 1 - alpha / A;
                    break;
                case lowShelf:
                {
                    const float s = 2 * std::sqrt (A) * alpha;
                    n0 = A * ((A + 1) - (A - 1) * cw + s); n1 = 2 * A * ((A - 1) - (A + 1) * cw); n2 = A * ((A + 1) - (A - 1) * cw - s);
                    d0 = (A + 1) + (A - 1) * cw + s;       d1 = -2 * ((A - 1) + (A + 1) * cw);    d2 = (A + 1) + (A - 1) * cw - s;
                    break;
                }
                case highShelf:
                {
                    const float s = 2 * std::sqrt (A) * alpha;
                    n0 = A * ((A + 1) + (A - 1) * cw + s); n1 = -2 * A * ((A - 1) + (A + 1) * cw); n2 = A * ((A + 1) + (A - 1) * cw - s);
                    d0 = (A + 1) - (A - 1) * cw + s;       d1 = 2 * ((A - 1) - (A + 1) * cw);      d2 = (A + 1) - (A - 1) * cw - s;
                    break;
                }
                case lowCut:
                    n0 = (1 + cw) / 2; n1 = -(1 + cw); n2 = (1 + cw) / 2;
                    d0 = 1 + alpha; d1 = -2 * cw; d2 = 1 - alpha;
                    break;
                case highCut:
                    n0 = (1 - cw) / 2; n1 = 1 - cw; n2 = (1 - cw) / 2;
                    d0 = 1 + alpha; d1 = -2 * cw; d2 = 1 - alpha;
                    break;
            }

            b0 = n0 / d0; b1 = n1 / d0; b2 = n2 / d0; a1 = d1 / d0; a2 = d2 / d0;
        }

        float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = dsp::undenormal (b1 * x - a1 * y + z2);
            z2 = dsp::undenormal (b2 * x - a2 * y);
            return y;
        }

        void reset() noexcept    { z1 = z2 = 0.0f; }
    };

    //==========================================================================
    /** Follows a signal's level: fast up (attack), slow down (release). Seconds; out is linear. */
    struct Follower
    {
        float level = 0.0f, up = 0.0f, down = 0.0f;

        void set (float attack, float release, float rate) noexcept
        {
            up = 1.0f - std::exp (-1.0f / (std::fmax (0.0001f, attack) * rate));
            down = 1.0f - std::exp (-1.0f / (std::fmax (0.001f, release) * rate));
        }

        float process (float x) noexcept
        {
            const float a = std::fabs (x);
            level += (a - level) * (a > level ? up : down);
            level = dsp::undenormal (level);
            return level;
        }
    };

    /** A compressor (feed-forward, soft knee). Call gainFor (detector) with the loudest of the
        channels (stereo-linked), then multiply each channel by the gain. thresholdDb, ratio
        (1..20; 20 is a limiter), kneeDb, attack and release in seconds. */
    struct Compressor
    {
        Follower follower;
        float thresholdDb = -18.0f, ratio = 4.0f, kneeDb = 6.0f, reductionDb = 0.0f;

        void set (float threshold, float r, float attack, float release, float rate, float knee = 6.0f) noexcept
        {
            thresholdDb = threshold; ratio = std::fmax (1.0f, r); kneeDb = knee;
            follower.set (attack, release, rate);
        }

        float gainFor (float detector) noexcept
        {
            const float levelDb = 20.0f * std::log10 (std::fmax (follower.process (detector), 1.0e-6f));
            const float over = levelDb - thresholdDb;
            float reduce = 0.0f;

            if (over > kneeDb * 0.5f)
                reduce = over * (1.0f - 1.0f / ratio);
            else if (over > -kneeDb * 0.5f)   // inside the knee: ease in
                reduce = (1.0f - 1.0f / ratio) * (over + kneeDb * 0.5f) * (over + kneeDb * 0.5f) / (2.0f * kneeDb);

            reductionDb = reduce;
            return stella::dbToGain (-reduce);
        }

        /** A fair make-up gain for these settings (dB): about half what's taken at the threshold. */
        float makeupDb() const noexcept    { return -thresholdDb * (1.0f - 1.0f / ratio) * 0.5f; }
    };

    /** Transient shaper: more (or less) attack and sustain, whatever the level. Call gainFor
        with the detector; attack and sustain -1..1 (1 doubles it, -1 halves it). */
    struct Transients
    {
        Follower fast, slow;

        void prepare (float rate) noexcept    { fast.set (0.0005f, 0.02f, rate); slow.set (0.02f, 0.2f, rate); }

        float gainFor (float detector, float attack, float sustain) noexcept
        {
            const float f = fast.process (detector), s = slow.process (detector);
            const float onset = stella::clamp ((f - s) / std::fmax (s, 1.0e-4f), 0.0f, 2.0f);   // high at a hit's start
            const float tail = stella::clamp ((s - f) / std::fmax (s, 1.0e-4f), 0.0f, 1.0f);    // high as it decays
            return std::fmax (0.0f, 1.0f + attack * onset * 0.5f + sustain * tail);
        }
    };

    //==========================================================================
    /** Auto-wah: a resonant band-pass whose frequency follows the playing. sensitivity 0..1,
        lowest and highest Hz, resonance 0..1. */
    struct AutoWah
    {
        Follower follower;
        dsp::Svf filter;
        float rate = 48000.0f;
        int counter = 0;

        void prepare (float sampleRate) noexcept    { rate = sampleRate; follower.set (0.005f, 0.12f, rate); }

        float process (float x, float sensitivity, float lowHz, float highHz, float resonance) noexcept
        {
            const float level = stella::clamp (follower.process (x) * (1.0f + sensitivity * 12.0f), 0.0f, 1.0f);

            if ((counter++ & 7) == 0)
                filter.set (lowHz * std::pow (highHz / lowHz, level), resonance, rate);

            return filter.bandpass (x) * 1.5f;
        }
    };
}
```

## Character.cpp
```cpp
// modules/Character.cpp: a complete stereo channel strip: input, three-band EQ, compressor,
// saturation (tape, tube, console, fuzz), transient shaper, dry/wet mix, output.
#include "Character.h"

class Character final : public stella::Module
{
public:
    void prepare (double sampleRate, int) override
    {
        rate = (float) sampleRate;
        for (auto& s : saturators) s.prepare (rate);
        transients.prepare (rate);
        input.prepare (sampleRate, 20.0f);
        output.prepare (sampleRate, 20.0f);
        mix.prepare (sampleRate, 20.0f);
        input.snap (stella::dbToGain (param (P_input)));
        output.snap (stella::dbToGain (param (P_output)));
        mix.snap (param (P_mix) * 0.01f);
    }

    void reset() override
    {
        for (auto& b : low) b.reset();
        for (auto& b : mid) b.reset();
        for (auto& b : high) b.reset();
    }

    void process (const stella::Context& c, const float* const* in, float* const* out) override
    {
        // Coefficients once a block.
        for (int ch = 0; ch < 2; ++ch)
        {
            low[ch].set (character::Biquad::lowShelf, 110.0f, param (P_low), 0.7f, rate);
            mid[ch].set (character::Biquad::peak, param (P_midFreq), param (P_mid), 0.9f, rate);
            high[ch].set (character::Biquad::highShelf, 8000.0f, param (P_high), 0.7f, rate);
        }

        compressor.set (param (P_threshold), param (P_ratio), param (P_attack) * 0.001f, param (P_release) * 0.001f, rate);
        const int type = (int) std::lround (param (P_type));
        const float drive = param (P_drive), tone = param (P_tone);
        const float punch = param (P_punch), body = param (P_body);
        const bool compressing = param (P_ratio) > 1.01f;
        const float makeup = stella::dbToGain (compressor.makeupDb());

        for (int i = 0; i < c.numFrames; ++i)
        {
            const float gIn = input.next (stella::dbToGain (param (P_input)));
            float l = in[0][i] * gIn, r = in[1][i] * gIn;
            const float dryL = l, dryR = r;

            l = high[0].process (mid[0].process (low[0].process (l)));
            r = high[1].process (mid[1].process (low[1].process (r)));

            if (compressing)
            {
                const float g = compressor.gainFor (std::fmax (std::fabs (l), std::fabs (r))) * makeup;   // stereo-linked
                l *= g; r *= g;
            }

            if (drive > 0.05f)
            {
                l = saturators[0].process (l, type, drive, tone);
                r = saturators[1].process (r, type, drive, tone);
            }

            if (std::fabs (punch) > 0.01f || std::fabs (body) > 0.01f)
            {
                const float g = transients.gainFor (std::fmax (std::fabs (l), std::fabs (r)), punch, body);
                l *= g; r *= g;
            }

            const float wet = mix.next (param (P_mix) * 0.01f);
            const float gOut = output.next (stella::dbToGain (param (P_output)));
            out[0][i] = (dryL + (l - dryL) * wet) * gOut;
            out[1][i] = (dryR + (r - dryR) * wet) * gOut;
        }

        display (0, stella::clamp (compressor.reductionDb / 24.0f, 0.0f, 1.0f));   // a gain reduction meter
    }

private:
    enum { P_input, P_low, P_mid, P_midFreq, P_high, P_threshold, P_ratio, P_attack, P_release,
           P_type, P_drive, P_tone, P_punch, P_body, P_mix, P_output };

    character::Biquad low[2], mid[2], high[2];
    character::Compressor compressor;
    character::Saturator saturators[2];
    character::Transients transients;
    stella::Smoother input, output, mix;
    float rate = 48000.0f;
};

namespace
{
    const char* const inputs[] { "in L", "in R" };
    const char* const outputs[] { "out L", "out R" };
    const char* const displayNames[] { "gr" };

    const stella::Param params[]
    {
        { "input",     "Input",      -24.0f, 24.0f,  0.0f,    "dB", 1.0f },
        { "low",       "Low",        -15.0f, 15.0f,  0.0f,    "dB", 1.0f },
        { "mid",       "Mid",        -15.0f, 15.0f,  0.0f,    "dB", 1.0f },
        { "mid_freq",  "Mid freq",   200.0f, 6000.0f, 1200.0f, "Hz", 0.3f },
        { "high",      "High",       -15.0f, 15.0f,  0.0f,    "dB", 1.0f },
        { "threshold", "Threshold",  -48.0f, 0.0f,   -18.0f,  "dB", 1.0f },
        { "ratio",     "Ratio",      1.0f,   20.0f,  1.0f,    ":1", 0.5f },
        { "attack",    "Attack",     0.1f,   100.0f, 10.0f,   "ms", 0.4f },
        { "release",   "Release",    10.0f,  1000.0f, 120.0f, "ms", 0.4f },
        { "type",      "Character",  0.0f,   3.0f,   0.0f,    "",   1.0f },   // tape, tube, console, fuzz
        { "drive",     "Drive",      0.0f,   36.0f,  0.0f,    "dB", 1.0f },
        { "tone",      "Tone",       -1.0f,  1.0f,   0.0f,    "",   1.0f },
        { "punch",     "Punch",      -1.0f,  1.0f,   0.0f,    "",   1.0f },
        { "body",      "Body",       -1.0f,  1.0f,   0.0f,    "",   1.0f },
        { "mix",       "Mix",        0.0f,   100.0f, 100.0f,  "%",  1.0f },
        { "output",    "Output",     -24.0f, 12.0f,  0.0f,    "dB", 1.0f },
    };

    stella::Registrar registrar ({ "Character", "Channel strip", inputs, STELLA_COUNT (inputs), outputs, STELLA_COUNT (outputs),
                                   params, STELLA_COUNT (params), [] () -> stella::Module* { return new Character(); },
                                   displayNames, STELLA_COUNT (displayNames) });
}
```

Its "Character" parameter shows on the panel as a dropdown: Tape, Tube, Console, Fuzz. The "gr" display drives a gain-reduction meter (0..1 is 0 to 24 dB).
