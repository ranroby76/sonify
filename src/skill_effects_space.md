<!-- D:\workspace\Fanan_web_hub\src\skill_effects_space.md -->
---
name: effects-space
for: plugins
title: Space effects
description: Tested time and space effects: chorus and ensemble, flanger, phaser, tape delay with wow, flutter and ping-pong, a granular pitch shifter, a lush eight-line FDN reverb with modulation and shimmer, and a bass-safe stereo widener, plus a complete chorus, delay and reverb module. Read it for any modulation, delay, reverb or stereo effect, or to add space to an instrument.
---
# Space effects

`modules/Space.h` (written against the dsp-toolkit skill's `Dsp.h` and `DspFx.h`: write those first) and a complete effect, `modules/Space.cpp`. The delay lines are sized in `prepare()`; nothing allocates in `process()`.

## The processors
- `Chorus` (rate Hz, depth, mix): three copies at 9, 13 and 17 ms swaying on LFOs a third apart, panned left, centre, right. Mono in, stereo out: for a stereo source, chorus the middle and add the input's side back (as `Space.cpp` does). Measured on a mono chord: it opens the sides to about -9 dB under the middle with a correlation near 0.8: wide but safe in mono.
- `Flanger` (rate, depth, feedback -0.95..0.95, mix): 0.4 to 6.4 ms swept; negative feedback is hollow, positive is metallic. One per channel, the right one's LFO a quarter ahead (`prepare (rate, 0.25f)`).
- `Phaser` (rate, depth, feedback 0..0.9, mix): six allpasses swept 200 Hz to 4 kHz. Slow (0.1 to 0.5 Hz) for pads, faster for guitars.
- `TapeDelay` (ms, feedback 0..1.1, tone, wobble, ping-pong, mix): each repeat goes darker (low-pass) and softer (saturation), with a 90 Hz high-pass so the lows don't pile up; changing the time glides like tape. Feedback over 1 builds into self-oscillating dub washes, held by the saturation (the analyzer reports it as "still sounding": that's the point there, not a fault).
- `PitchShifter` (semitones): two crossfaded read heads; good for shimmer, octaves and detune, grainy on solo voices.
- `Reverb` (size, decay s, damping, pre-delay ms, shimmer, width): pre-delay, four Schroeder diffusers, then eight delay lines with prime-ish lengths, a Hadamard mix (every line feeds every other without gain), per-line damping and slow modulation (no metallic ring). Its two outputs come from orthogonal sign patterns: measured correlation about 0 (full width) and balanced. The decay knob sets the low frequencies' -60 dB time; damping makes the highs die sooner, so a measured broadband RT60 reads a little shorter. Shimmer feeds the tail back an octave up (cleaned of lows and fizz): ethereal pads, stable even at full.
- `Width` (0 mono, 1 as is, 2 double): widens only above 150 Hz so the bass stays solid.

## Recipes
- **Juno chorus**: chorus rate 0.5, depth 0.6, mix 50 (mode II: rate 0.8, depth 0.8).
- **Slapback**: delay 90 to 130 ms, feedback 10, tone 0.6, mix 30.
- **Dub delay**: 375 ms (or the beat), feedback 75 to 105, tone 0.3, wobble 0.4, ping-pong on.
- **Room**: size 0.25, decay 0.6, damping 0.5, pre-delay 5 ms. **Hall**: size 0.7, decay 3, damping 0.35, pre-delay 25. **Plate**: size 0.45, decay 2, damping 0.15 (bright), pre-delay 0. **Ambient**: size 1, decay 12, damping 0.4, shimmer 0.4, pre-delay 60.
- **Synth pad space**: chorus 30%, delay 20% at 1/4 with feedback 40, reverb 35% hall.
- Delays and reverbs in an instrument plugin: put `Space` (or just the parts needed) after the voice module in the graph; a reverb send is just its mix knob.

## Tips
- Context has no tempo: delay times are in milliseconds (give "Time" a skew around 0.4). The user can match a song's tempo by ear, or you can offer note-value choices computed from a BPM knob.
- Test reverbs and delays with `analyze_sound`: "impulse" for the decay time (RT60) and the first echo, "drums" for density and tails, "chord" for width and modulation; a tail of 4 to 12 s for long reverbs.
- Width is not loudness: keep an eye on "in mono it loses" (under about 3 dB is healthy for a reverb, under 1 dB for a chorused instrument).

## Space.h
```cpp
// modules/Space.h: time and space effects: chorus/ensemble, flanger, phaser, tape and ping-pong
// delay, a pitch shifter, and a lush FDN reverb with shimmer. Written against Dsp.h and DspFx.h.
// Allocate in prepare() (they size their delay lines there), never in process().
#pragma once
#include "Dsp.h"
#include "DspFx.h"

namespace space
{
    using stella::pi;
    using stella::twoPi;

    inline float msToSamples (float ms, float rate) noexcept    { return ms * 0.001f * rate; }

    //==========================================================================
    /** Chorus / ensemble: three delayed copies swaying on LFOs a third apart, spread in stereo
        (the Juno and string-machine shimmer). rate Hz (0.1 .. 3), depth 0..1, mix 0..1. */
    struct Chorus
    {
        dsp::DelayLine line;
        float rate = 48000.0f, phase = 0.0f;

        void prepare (float sampleRate)    { rate = sampleRate; line.prepare ((int) msToSamples (40.0f, rate)); }

        void process (float in, float hz, float depth, float mix, float& left, float& right) noexcept
        {
            line.push (in);
            phase += hz / rate;
            if (phase >= 1.0f) phase -= 1.0f;

            float l = 0.0f, r = 0.0f;
            static const float pans[3] { 0.15f, 0.5f, 0.85f };

            for (int v = 0; v < 3; ++v)
            {
                const float lfo = std::sin (twoPi * (phase + (float) v / 3.0f));
                const float d = msToSamples (9.0f + 4.0f * (float) v + depth * 4.5f * lfo, rate);
                const float y = line.read (d);
                l += y * (1.0f - pans[v]);
                r += y * pans[v];
            }

            left = in * (1.0f - mix) + l * mix * 0.9f;
            right = in * (1.0f - mix) + r * mix * 0.9f;
        }
    };

    //==========================================================================
    /** Flanger: a very short delay swept by an LFO, with feedback: the jet-plane sweep.
        hz, depth 0..1, feedback -0.95..0.95, mix 0..1. One per channel (offset their phases). */
    struct Flanger
    {
        dsp::DelayLine line;
        float rate = 48000.0f, phase = 0.0f, fb = 0.0f;

        void prepare (float sampleRate, float startPhase = 0.0f)    { rate = sampleRate; phase = startPhase; line.prepare ((int) msToSamples (12.0f, rate)); }

        float process (float in, float hz, float depth, float feedback, float mix) noexcept
        {
            phase += hz / rate;
            if (phase >= 1.0f) phase -= 1.0f;
            const float lfo = 0.5f - 0.5f * std::cos (twoPi * phase);
            const float delayed = line.read (msToSamples (0.4f + depth * 6.0f * lfo, rate));
            line.push (in + dsp::fastTanh (delayed * stella::clamp (feedback, -0.95f, 0.95f)));
            return in * (1.0f - 0.5f * mix) + delayed * 0.5f * mix;
        }
    };

    //==========================================================================
    /** Phaser: six first-order allpass stages swept together (notches that glide), with
        feedback. hz, depth 0..1, feedback 0..0.9, mix 0..1. One per channel. */
    struct Phaser
    {
        float rate = 48000.0f, phase = 0.0f, state[6] {}, last = 0.0f;

        void prepare (float sampleRate, float startPhase = 0.0f) noexcept    { rate = sampleRate; phase = startPhase; }

        float process (float in, float hz, float depth, float feedback, float mix) noexcept
        {
            phase += hz / rate;
            if (phase >= 1.0f) phase -= 1.0f;
            const float lfo = 0.5f - 0.5f * std::cos (twoPi * phase);
            const float f = 200.0f * std::pow (20.0f, depth * lfo);   // 200 Hz .. 4 kHz
            const float t = std::tan (pi * std::fmin (f, rate * 0.45f) / rate);
            const float a = (t - 1.0f) / (t + 1.0f);

            float x = in + last * feedback;

            for (auto& s : state)   // y = a x + s; s = x - a y
            {
                const float y = a * x + s;
                s = dsp::undenormal (x - a * y);
                x = y;
            }

            last = x;
            return in * (1.0f - 0.5f * mix) + x * 0.5f * mix;
        }
    };

    //==========================================================================
    /** Tape delay, stereo, optionally ping-pong: echoes that darken and saturate as they repeat,
        with wow and flutter. time in ms (changes glide, like a tape's speed), feedback 0..1.1
        (above 1 it builds, held by the saturation), tone 0 dark .. 1 bright, wobble 0..1. */
    struct TapeDelay
    {
        dsp::DelayLine left, right;
        dsp::OnePole lowL, lowR, highL, highR;
        stella::Smoother time;
        float rate = 48000.0f, wow = 0.0f, flutter = 0.0f;

        void prepare (float sampleRate, float maxMs = 2000.0f)
        {
            rate = sampleRate;
            left.prepare ((int) msToSamples (maxMs + 20.0f, rate));
            right.prepare ((int) msToSamples (maxMs + 20.0f, rate));
            time.prepare (sampleRate, 250.0f);   // a slow glide when the time changes
            time.snap (msToSamples (350.0f, rate));
        }

        void process (float inL, float inR, float ms, float feedback, float tone, float wobble, bool pingPong, float mix,
                      float& outL, float& outR) noexcept
        {
            wow += 0.6f / rate;     if (wow >= 1.0f) wow -= 1.0f;
            flutter += 6.3f / rate; if (flutter >= 1.0f) flutter -= 1.0f;
            const float mod = wobble * (std::sin (twoPi * wow) * 0.004f + std::sin (twoPi * flutter) * 0.0008f);
            const float d = time.next (msToSamples (ms, rate)) * (1.0f + mod);

            const float cutoff = 1500.0f + 9000.0f * tone;
            lowL.set (cutoff, rate); lowR.set (cutoff, rate);
            highL.set (90.0f, rate); highR.set (90.0f, rate);

            float l = left.read (d), r = right.read (pingPong ? d : d * 1.003f);   // a hair apart: width
            l = dsp::fastTanh (highL.highpass (lowL.lowpass (l)) * 1.1f) / 1.1f;     // each repeat darker and softer
            r = dsp::fastTanh (highR.highpass (lowR.lowpass (r)) * 1.1f) / 1.1f;

            if (pingPong)
            {
                left.push (0.5f * (inL + inR) + r * feedback);   // the input starts on the left, then bounces
                right.push (l * feedback);
            }
            else
            {
                left.push (inL + l * feedback);
                right.push (inR + r * feedback);
            }

            outL = inL + l * mix;   // the dry sound stays whole; the echoes come on top
            outR = inR + r * mix;
        }
    };

    //==========================================================================
    /** Pitch shifter: two read heads sliding through a short delay, crossfaded (granular, the
        "Doppler" way). semitones -24..24. Fine for shimmer, octaves and detune; a little grainy
        on solo instruments. */
    struct PitchShifter
    {
        dsp::DelayLine line;
        float rate = 48000.0f, phase = 0.0f, window = 2400.0f;

        void prepare (float sampleRate)    { rate = sampleRate; window = 0.05f * rate; line.prepare ((int) window * 2 + 16); }

        float process (float in, float semitones) noexcept
        {
            line.push (in);
            const float ratio = std::pow (2.0f, semitones / 12.0f);
            phase += (1.0f - ratio) / window;   // the heads drift through the window
            phase -= std::floor (phase);

            float out = 0.0f;

            for (int h = 0; h < 2; ++h)
            {
                float p = phase + 0.5f * (float) h;
                p -= std::floor (p);
                const float gain = std::sin (pi * p);
                out += line.read (2.0f + p * window) * gain * gain;   // sin² windows half a cycle apart add up to 1
            }

            return out;
        }
    };

    //==========================================================================
    /** A lush stereo reverb: pre-delay, four diffusers, then an eight-line feedback delay
        network (Hadamard mixing) with damping and slow modulation (no metallic ring), and an
        optional shimmer (the tail fed back an octave up). size 0..1, decay seconds (0.3 .. 30),
        damping 0..1 (dark), predelay ms, shimmer 0..1, width 0..1. */
    struct Reverb
    {
        static constexpr int lines = 8;
        dsp::DelayLine pre, diffusers[4], delays[lines];
        dsp::OnePole damping[lines];
        PitchShifter shimmerShift;
        dsp::OnePole shimmerHigh, shimmerLow;
        float rate = 48000.0f, gains[lines] {}, lengths[lines] {}, feedbackSample = 0.0f;
        float modPhase[lines] {};
        float lastSize = -1.0f, lastDecay = -1.0f, lastDamp = -1.0f;

        void prepare (float sampleRate)
        {
            rate = sampleRate;
            pre.prepare ((int) msToSamples (250.0f, rate));
            static const float diffuseMs[4] { 4.77f, 3.59f, 12.73f, 9.30f };
            for (int i = 0; i < 4; ++i) diffusers[i].prepare ((int) msToSamples (diffuseMs[i] + 1.0f, rate));
            for (int i = 0; i < lines; ++i) { delays[i].prepare ((int) msToSamples (160.0f, rate)); modPhase[i] = (float) i / (float) lines; }
            shimmerShift.prepare (rate);
            shimmerHigh.set (300.0f, rate);
            shimmerLow.set (7000.0f, rate);
            lastSize = -1.0f;
        }

        void update (float size, float decay, float damp) noexcept
        {
            static const float baseMs[lines] { 29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.9f, 67.3f, 73.1f };   // no common factors

            for (int i = 0; i < lines; ++i)
            {
                lengths[i] = msToSamples (baseMs[i] * (0.45f + 1.55f * size), rate);
                gains[i] = std::pow (0.001f, lengths[i] / (std::fmax (0.1f, decay) * rate));   // -60 dB after decay seconds
                damping[i].set (16000.0f * std::pow (0.05f, damp), rate);                     // 16 kHz .. 800 Hz
            }

            lastSize = size; lastDecay = decay; lastDamp = damp;
        }

        void process (float inL, float inR, float size, float decay, float damp, float predelayMs, float shimmer, float width,
                      float& outL, float& outR) noexcept
        {
            if (size != lastSize || decay != lastDecay || damp != lastDamp)
                update (size, decay, damp);

            // Pre-delay, then diffusion: Schroeder allpasses smear the input into a dense wash.
            pre.push (0.5f * (inL + inR) + feedbackSample * shimmer * 0.3f);
            float x = pre.read (std::fmax (1.0f, msToSamples (predelayMs, rate)));
            static const float diffuseMs[4] { 4.77f, 3.59f, 12.73f, 9.30f };

            for (int i = 0; i < 4; ++i)
            {
                const float delayed = diffusers[i].read (msToSamples (diffuseMs[i], rate));
                const float v = x + 0.6f * delayed;
                diffusers[i].push (v);
                x = delayed - 0.6f * v;
            }

            // The network: read every line (slowly swaying), mix them (Hadamard), feed them back.
            float y[lines], out[lines];

            for (int i = 0; i < lines; ++i)
            {
                modPhase[i] += (0.3f + 0.07f * (float) i) / rate;
                if (modPhase[i] >= 1.0f) modPhase[i] -= 1.0f;
                const float sway = (i & 1) == 0 ? 6.0f * std::sin (twoPi * modPhase[i]) : 0.0f;   // half the lines sway a few samples
                y[i] = damping[i].lowpass (delays[i].read (lengths[i] + sway)) * gains[i];
                out[i] = y[i];
            }

            // Fast Hadamard transform: every line feeds every other, energy kept (scaled 1/sqrt 8).
            for (int span = 1; span < lines; span <<= 1)
                for (int i = 0; i < lines; i += span << 1)
                    for (int j = i; j < i + span; ++j)
                    {
                        const float a = y[j], b = y[j + span];
                        y[j] = a + b; y[j + span] = a - b;
                    }

            for (int i = 0; i < lines; ++i)
            {
                const float fed = y[i] * 0.35355339f + x * ((i & 1) == 0 ? 0.5f : -0.5f);
                delays[i].push (dsp::undenormal (fed));
            }

            // Two outputs from two orthogonal sign patterns over the lines: uncorrelated, wide.
            float l = 0.0f, r = 0.0f;
            // (Both rows are also orthogonal to the input's own pattern, so neither side gets more of it.)
            static const float rowL[lines] { 1, 1, -1, -1, 1, 1, -1, -1 }, rowR[lines] { 1, 1, 1, 1, -1, -1, -1, -1 };

            for (int i = 0; i < lines; ++i)
            {
                l += out[i] * rowL[i];
                r += out[i] * rowR[i];
            }

            l *= 0.5f; r *= 0.5f;

            // Shimmer: the tail an octave up, cleaned (no lows, no fizz) and fed back gently.
            if (shimmer > 0.001f)
                feedbackSample = shimmerLow.lowpass (shimmerHigh.highpass (shimmerShift.process (0.5f * (l + r), 12.0f)));
            else
                feedbackSample = 0.0f;

            const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * width;
            outL = mid + side;
            outR = mid - side;
        }
    };

    //==========================================================================
    /** Stereo width by mid and side: 0 mono, 1 as it is, 2 twice as wide (widening only above
        about 150 Hz, so the bass stays solid). */
    struct Width
    {
        dsp::OnePole lowSide;

        void prepare (float rate) noexcept    { lowSide.set (150.0f, rate); }

        void process (float& left, float& right, float width) noexcept
        {
            const float mid = 0.5f * (left + right);
            const float side = 0.5f * (left - right);
            const float low = lowSide.lowpass (side);
            const float scaled = low * std::fmin (width, 1.0f) + (side - low) * width;   // widening only above 150 Hz: the bass stays solid
            left = mid + scaled;
            right = mid - scaled;
        }
    };
}
```

## Space.cpp
```cpp
// modules/Space.cpp: a complete stereo space effect: chorus, tape delay (or ping-pong), a lush
// reverb with shimmer, and width. Each section has its own mix; at 0 it's skipped.
#include "Space.h"

class Space final : public stella::Module
{
public:
    void prepare (double sampleRate, int) override
    {
        rate = (float) sampleRate;
        chorus.prepare (rate);
        delay.prepare (rate, 2000.0f);
        reverb.prepare (rate);
        width.prepare (rate);
        output.prepare (sampleRate, 20.0f);
        output.snap (stella::dbToGain (param (P_output)));
    }

    void process (const stella::Context& c, const float* const* in, float* const* out) override
    {
        const float chorusMix = param (P_chorusMix) * 0.01f, delayMix = param (P_delayMix) * 0.01f, reverbMix = param (P_reverbMix) * 0.01f;
        const bool pingPong = param (P_pingPong) > 0.5f;

        for (int i = 0; i < c.numFrames; ++i)
        {
            float l = in[0][i], r = in[1][i];

            if (chorusMix > 0.0f)
            {
                // The chorus spreads the middle; the input's own stereo (its side) rides on top.
                const float side = 0.5f * (l - r);
                float a, b;
                chorus.process (0.5f * (l + r), param (P_chorusRate), param (P_chorusDepth), chorusMix, a, b);
                l = a + side;
                r = b - side;
            }

            if (delayMix > 0.0f)
                delay.process (l, r, param (P_delayTime), param (P_feedback) * 0.01f, param (P_delayTone), param (P_wobble),
                               pingPong, delayMix, l, r);

            if (reverbMix > 0.0f)
            {
                float wl, wr;
                reverb.process (l, r, param (P_size), param (P_decay), param (P_damping), param (P_predelay), param (P_shimmer), 1.0f, wl, wr);
                l += wl * reverbMix;
                r += wr * reverbMix;
            }

            width.process (l, r, param (P_width));

            const float g = output.next (stella::dbToGain (param (P_output)));
            out[0][i] = dsp::fastTanh (l * g);
            out[1][i] = dsp::fastTanh (r * g);
        }
    }

private:
    enum { P_chorusRate, P_chorusDepth, P_chorusMix, P_delayTime, P_feedback, P_delayTone, P_wobble, P_pingPong, P_delayMix,
           P_size, P_decay, P_damping, P_predelay, P_shimmer, P_reverbMix, P_width, P_output };

    space::Chorus chorus;
    space::TapeDelay delay;
    space::Reverb reverb;
    space::Width width;
    stella::Smoother output;
    float rate = 48000.0f;
};

namespace
{
    const char* const inputs[] { "in L", "in R" };
    const char* const outputs[] { "out L", "out R" };

    const stella::Param params[]
    {
        { "chorus_rate",  "Chorus rate",  0.05f, 5.0f,    0.6f,   "Hz", 0.5f },
        { "chorus_depth", "Chorus depth", 0.0f,  1.0f,    0.5f,   "",   1.0f },
        { "chorus_mix",   "Chorus",       0.0f,  100.0f,  0.0f,   "%",  1.0f },
        { "delay_time",   "Time",         10.0f, 2000.0f, 375.0f, "ms", 0.4f },
        { "feedback",     "Feedback",     0.0f,  110.0f,  35.0f,  "%",  1.0f },
        { "delay_tone",   "Tone",         0.0f,  1.0f,    0.5f,   "",   1.0f },
        { "wobble",       "Wobble",       0.0f,  1.0f,    0.2f,   "",   1.0f },
        { "pingpong",     "Ping-pong",    0.0f,  1.0f,    0.0f,   "",   1.0f },
        { "delay_mix",    "Delay",        0.0f,  100.0f,  0.0f,   "%",  1.0f },
        { "size",         "Size",         0.0f,  1.0f,    0.6f,   "",   1.0f },
        { "decay",        "Decay",        0.3f,  30.0f,   2.8f,   "s",  0.35f },
        { "damping",      "Damping",      0.0f,  1.0f,    0.4f,   "",   1.0f },
        { "predelay",     "Pre-delay",    0.0f,  200.0f,  20.0f,  "ms", 0.5f },
        { "shimmer",      "Shimmer",      0.0f,  1.0f,    0.0f,   "",   1.0f },
        { "reverb_mix",   "Reverb",       0.0f,  100.0f,  30.0f,  "%",  1.0f },
        { "width",        "Width",        0.0f,  2.0f,    1.0f,   "",   1.0f },
        { "output",       "Output",       -24.0f, 12.0f,  0.0f,   "dB", 1.0f },
    };

    stella::Registrar registrar ({ "Space", "Space", inputs, STELLA_COUNT (inputs), outputs, STELLA_COUNT (outputs),
                                   params, STELLA_COUNT (params), [] () -> stella::Module* { return new Space(); } });
}
```
