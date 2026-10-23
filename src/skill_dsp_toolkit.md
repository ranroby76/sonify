<!-- D:\workspace\Fanan_web_hub\src\skill_dsp_toolkit.md -->
---
name: dsp-toolkit
for: plugins
title: DSP toolkit
description: The shared building blocks every good plugin starts from (band-limited oscillators, zero-delay filters, a ladder, click-free envelopes, LFOs, voice allocation, delay lines, 2x oversampling, soft clipping), as three tested headers to write into modules/, plus the rules for clean, pro-sounding DSP and tempo sync (LFOs, delays and steps in time with the song). Read it before writing the first module of any instrument or effect.
---
# DSP toolkit

Three headers, tested: write the ones the plugin needs into `modules/` with `write_file` (each in a step of its own), then include them from the modules. They're written against `stella_api.h` only. Don't paste them into a module: one copy in `modules/`, included everywhere.

- `modules/Dsp.h`: `fastTanh` (soft clip), `Noise`, `polyBlep`, `Osc` (saw, square/pulse, triangle, sine; hard sync), `Svf` (zero-delay state-variable filter: low, band, high, notch), `Ladder` (fat 24 dB low-pass with saturated feedback), `OnePole`, `DcBlock`. Almost every plugin needs it.
- `modules/DspEnv.h`: `Adsr` (exponential, click-free), `Lfo` (six shapes including smooth random), `VoiceAllocator` (polyphony with stealing; mono with last-note priority). Every instrument needs it.
- `modules/DspFx.h`: `DelayLine` (Hermite-interpolated reads, for delays, chorus, flangers and strings), `Oversampler2x` (halfband up and down around any shaper). Effects and physical models need it.

## The rules that make it sound pro
- **Gain staging.** A single note around -18 to -12 dBFS RMS, peaks near -6 dBFS; a full chord must not clip. Scale a voice sum by about `1 / sqrt(voices playing)` or a fixed 0.3, and finish with `fastTanh` only as a safety, not as the sound.
- **No aliasing.** Oscillators: `Osc` (PolyBLEP), never a naive `2 * phase - 1` saw or `phase < 0.5` square. Distortion, folding and hard clipping: inside `Oversampler2x`, and tame the amount as the pitch rises (the higher the note, the less fold or drive it can take before it aliases).
- **No clicks.** Envelopes from `Adsr` (it restarts from its current level). Smooth every parameter that reaches the audio with `stella::Smoother` or by updating coefficients every 16 samples. A voice is freed only once its envelope is idle. Never reset an oscillator's phase or a filter's state while it sounds.
- **No denormals, no blow-ups.** The filters here flush tiny values. Keep feedback below 1 (a delay's feedback at most 0.98), and keep a `fastTanh` inside any loop that can build up.
- **DC.** After anything asymmetric (drive, folding, FM feedback, rectifying) put a `DcBlock` at the output.
- **Cheap where it doesn't matter.** Compute filter coefficients (`set`) every 16 samples, not every sample; `std::pow (2.0f, x)` per sample per voice is fine for a few voices but not for 16 unison saws: compute the frequency once per block.
- **Velocity and expression.** Velocity scales the level (about `0.35 + 0.65 * velocity`) and usually the filter's envelope amount too. Pitch bend: `context.pitchBend * 2` semitones (or a range parameter). Mod wheel: a vibrato or filter depth.
- **Stereo.** Mono below about 150 Hz. Width comes from detuned voices panned apart, a chorus or decorrelated delays, never from inverting one side.
- **Voices.** 8 voices for most synths, 16 for pads and keys, 1 (mono) for basses and leads with glide. One module can hold all the voices: an array of `Voice` structs, a `VoiceAllocator`, and a loop.

## Dsp.h
```cpp
// modules/Dsp.h: the plugin's shared toolkit: oscillators, noise, filters, soft clipping.
#pragma once
#include "stella_api.h"
#include <cmath>
#include <cstdint>

namespace dsp
{
    using stella::pi;
    using stella::twoPi;

    inline float fastTanh (float x) noexcept           // soft clip: close to tanh, cheap, never past +-1
    {
        x = stella::clamp (x, -3.0f, 3.0f);
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float undenormal (float x) noexcept         { return std::fabs (x) < 1.0e-15f ? 0.0f : x; }

    //==========================================================================
    /** White noise: a tiny xorshift generator, -1..1. Seed each voice differently. */
    struct Noise
    {
        std::uint32_t state = 22222u;
        void seed (std::uint32_t s) noexcept    { state = s != 0 ? s : 1u; }
        float next() noexcept
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return (float) state * (2.0f / 4294967295.0f) - 1.0f;
        }
    };

    //==========================================================================
    /** PolyBLEP: smooths the jumps of a naive wave, so it doesn't alias. t: phase 0..1, dt: step. */
    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    /** A band-limited oscillator: saw, square/pulse (pw 0.05..0.95), triangle, sine. */
    struct Osc
    {
        enum Shape { saw, square, triangle, sine };
        float phase = 0.0f, dt = 0.0f;

        void setFrequency (float hz, float sampleRate) noexcept   { dt = stella::clamp (hz / sampleRate, 0.0f, 0.45f); }
        void reset (float startPhase = 0.0f) noexcept               { phase = startPhase; }

        float next (Shape shape, float pw = 0.5f) noexcept
        {
            float v = 0.0f;

            switch (shape)
            {
                case saw:      v = 2.0f * phase - 1.0f - polyBlep (phase, dt); break;
                case sine:     v = std::sin (twoPi * phase); break;
                case triangle: v = 1.0f - 4.0f * std::fabs (phase - 0.5f); break;   // its harmonics fall fast: no smoothing needed
                case square:
                {
                    float t2 = phase + 1.0f - pw;
                    t2 -= std::floor (t2);
                    v = (phase < pw ? 1.0f : -1.0f) + polyBlep (phase, dt) - polyBlep (t2, dt);
                    break;
                }
            }

            phase += dt;
            if (phase >= 1.0f) phase -= 1.0f;
            return v;
        }

        /** Hard sync: a slave restarts when its master wraps (call before next()). */
        bool wrapsNext() const noexcept    { return phase + dt >= 1.0f; }
    };

    //==========================================================================
    /** A zero-delay-feedback state-variable filter (Cytomic / Simper, TPT): every mode at once,
        stable when modulated fast, self-oscillates as resonance nears 1. */
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f, g = 0.0f, k = 2.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        float low = 0.0f, band = 0.0f, high = 0.0f;

        /** cutoff in Hz, resonance 0..1 (1: rings). */
        void set (float cutoff, float resonance, float sampleRate) noexcept
        {
            cutoff = stella::clamp (cutoff, 10.0f, sampleRate * 0.48f);
            g = std::tan (pi * cutoff / sampleRate);
            k = 2.0f - 1.98f * stella::clamp (resonance, 0.0f, 1.0f);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void reset() noexcept    { ic1 = ic2 = 0.0f; }

        void process (float x) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = undenormal (2.0f * v1 - ic1);
            ic2 = undenormal (2.0f * v2 - ic2);
            low = v2; band = v1; high = x - k * v1 - v2;
        }

        float lowpass (float x) noexcept    { process (x); return low; }
        float bandpass (float x) noexcept   { process (x); return band; }
        float highpass (float x) noexcept   { process (x); return high; }
        float notch (float x) noexcept      { process (x); return low + high; }
    };

    /** A 4-pole (24 dB) ladder-style low-pass: two SVF stages with a saturated feedback path.
        Fat and resonant, the classic subtractive sound. drive 1 = clean, 4 = gritty. */
    struct Ladder
    {
        Svf a, b;
        float fb = 0.0f, res = 0.0f, drive = 1.0f;

        void set (float cutoff, float resonance, float sampleRate, float driveAmount = 1.0f) noexcept
        {
            a.set (cutoff, 0.0f, sampleRate);
            b.set (cutoff, 0.0f, sampleRate);
            res = stella::clamp (resonance, 0.0f, 1.0f) * 3.6f;
            drive = driveAmount;
        }

        void reset() noexcept    { a.reset(); b.reset(); fb = 0.0f; }

        float process (float x) noexcept
        {
            const float in = fastTanh ((x - res * fb) * drive);
            fb = b.lowpass (a.lowpass (in));
            return fb * (1.0f + 0.4f * res) / std::fmax (1.0f, drive * 0.8f);   // keep level steady as resonance rises
        }
    };

    /** One-pole low-pass (TPT): smoothing, tone controls, damping inside feedback loops. */
    struct OnePole
    {
        float s = 0.0f, g = 0.0f;
        void set (float cutoff, float sampleRate) noexcept
        {
            const float w = std::tan (pi * stella::clamp (cutoff, 1.0f, sampleRate * 0.48f) / sampleRate);
            g = w / (1.0f + w);
        }
        float lowpass (float x) noexcept    { const float v = (x - s) * g; const float y = v + s; s = undenormal (y + v); return y; }
        float highpass (float x) noexcept   { return x - lowpass (x); }
        void reset() noexcept               { s = 0.0f; }
    };

    /** Removes DC (a 10 Hz high-pass): at the end of anything asymmetric (drive, folding, FM feedback). */
    struct DcBlock
    {
        float x1 = 0.0f, y1 = 0.0f, r = 0.9987f;
        void prepare (float sampleRate) noexcept    { r = 1.0f - twoPi * 10.0f / sampleRate; }
        float process (float x) noexcept            { const float y = x - x1 + r * y1; x1 = x; y1 = undenormal (y); return y; }
    };
}
```

## DspEnv.h
```cpp
// modules/DspEnv.h: envelopes, LFOs and voice allocation.
#pragma once
#include "Dsp.h"

namespace dsp
{
    //==========================================================================
    /** ADSR with exponential curves, click-free: attack aims past 1 so it ends crisply, release
        always starts from where it is. Times in seconds, sustain 0..1. */
    struct Adsr
    {
        enum Stage { idle, attack, decay, sustain, release };
        Stage stage = idle;
        float level = 0.0f, rate = 48000.0f;
        float attackCoef = 0.0f, decayCoef = 0.0f, releaseCoef = 0.0f, sustainLevel = 0.7f;

        void prepare (float sampleRate) noexcept    { rate = sampleRate; }

        /** A one-pole step for a time constant of seconds / divisor. */
        static float coef (float seconds, float sampleRate, float divisor) noexcept
        {
            return 1.0f - std::exp (-divisor / (std::fmax (0.0005f, seconds) * sampleRate));
        }

        /** a: seconds to full level; d and r: seconds to fade about 40 dB; s: 0..1. */
        void set (float a, float d, float s, float r) noexcept
        {
            attackCoef = coef (a, rate, 1.609f);   // aiming at 1.25, it reaches 1 after a
            decayCoef = coef (d, rate, 4.6f);
            releaseCoef = coef (r, rate, 4.6f);
            sustainLevel = stella::clamp (s, 0.0f, 1.0f);
        }

        void noteOn() noexcept     { stage = attack; }             // from where it is: retriggers don't click
        void noteOff() noexcept    { if (stage != idle) stage = release; }
        bool isActive() const noexcept    { return stage != idle; }

        float next() noexcept
        {
            switch (stage)
            {
                case attack:
                    level += (1.25f - level) * attackCoef;
                    if (level >= 1.0f) { level = 1.0f; stage = decay; }
                    break;
                case decay:
                    level += (sustainLevel - level) * decayCoef;
                    if (std::fabs (level - sustainLevel) < 1.0e-4f) stage = sustain;
                    break;
                case sustain:
                    level = sustainLevel;
                    break;
                case release:
                    level += (0.0f - level) * releaseCoef;
                    if (level < 1.0e-5f) { level = 0.0f; stage = idle; }
                    break;
                case idle:
                    break;
            }
            return level;
        }
    };

    /** An LFO: sine, triangle, saw, square, sample & hold, smooth random. rate in Hz, out -1..1. */
    struct Lfo
    {
        enum Shape { sine, triangle, saw, square, sampleHold, smoothRandom };
        float phase = 0.0f, held = 0.0f, from = 0.0f;
        Noise noise;

        float next (Shape shape, float hz, float sampleRate) noexcept
        {
            phase += hz / sampleRate;
            if (phase >= 1.0f) { phase -= 1.0f; from = held; held = noise.next(); }
            switch (shape)
            {
                case sine:         return std::sin (twoPi * phase);
                case triangle:     return 1.0f - 4.0f * std::fabs (phase - 0.5f);
                case saw:          return 2.0f * phase - 1.0f;
                case square:       return phase < 0.5f ? 1.0f : -1.0f;
                case sampleHold:   return held;
                case smoothRandom: return from + (held - from) * (0.5f - 0.5f * std::cos (pi * phase));
            }
            return 0.0f;
        }
    };

    //==========================================================================
    /** Which voice plays which note: polyphony with voice stealing (the quietest, then the
        oldest), or one voice (mono/legato) that remembers held notes for last-note priority. */
    template <int maxVoices>
    struct VoiceAllocator
    {
        int note[maxVoices];
        unsigned age[maxVoices] {};
        unsigned clock = 0;
        int held[16]; int numHeld = 0;    // mono: the notes held, last pressed last

        VoiceAllocator()    { for (auto& n : note) n = -1; }

        /** A voice for a new note: one already playing that note, else a silent one, else the
            quietest released one, else the oldest held one. levels: each voice's envelope now. */
        int noteOn (int midiNote, int voices, const float* levels) noexcept
        {
            int chosen = -1;
            for (int v = 0; v < voices && chosen < 0; ++v) if (note[v] == midiNote) chosen = v;
            for (int v = 0; v < voices && chosen < 0; ++v) if (note[v] < 0 && levels[v] < 1.0e-4f) chosen = v;

            if (chosen < 0)
                for (int v = 0; v < voices; ++v)
                    if (note[v] < 0 && (chosen < 0 || levels[v] < levels[chosen])) chosen = v;

            if (chosen < 0)
            {
                chosen = 0;
                for (int v = 1; v < voices; ++v) if (age[v] < age[chosen]) chosen = v;
            }

            note[chosen] = midiNote;
            age[chosen] = ++clock;
            return chosen;
        }

        /** The voice playing a released note (-1: none). */
        int noteOff (int midiNote, int voices) noexcept
        {
            for (int v = 0; v < voices; ++v)
                if (note[v] == midiNote) { note[v] = -1; return v; }
            return -1;
        }

        // Mono: press and release; returns the note that should sound now (-1: none).
        int monoOn (int midiNote) noexcept
        {
            monoOff (midiNote);
            if (numHeld < 16) held[numHeld++] = midiNote;
            return midiNote;
        }

        int monoOff (int midiNote) noexcept
        {
            for (int i = 0; i < numHeld; ++i)
                if (held[i] == midiNote) { for (int j = i; j < numHeld - 1; ++j) held[j] = held[j + 1]; --numHeld; break; }
            return numHeld > 0 ? held[numHeld - 1] : -1;
        }
    };
}
```

## DspFx.h
```cpp
// modules/DspFx.h: delay lines and oversampling, for effects and physical models.
#pragma once
#include "Dsp.h"

namespace dsp
{
    //==========================================================================
    /** A delay line with fractional reads (Hermite): chorus, flanger, delay, Karplus-Strong.
        size: the longest delay in samples, given in prepare() (never allocate in process()). */
    struct DelayLine
    {
        float* buffer = nullptr;
        int size = 0, mask = 0, write = 0;

        ~DelayLine()    { delete[] buffer; }

        void prepare (int maxSamples)
        {
            int n = 1;
            while (n < maxSamples + 4) n <<= 1;
            if (n != size) { delete[] buffer; buffer = new float[(size_t) n]; size = n; mask = n - 1; }
            clear();
        }

        void clear() noexcept    { for (int i = 0; i < size; ++i) buffer[i] = 0.0f; write = 0; }

        void push (float x) noexcept    { buffer[write] = x; write = (write + 1) & mask; }

        /** delay in samples (1 .. size - 4): 1 is the sample just pushed. */
        float read (float delay) const noexcept
        {
            const float pos = (float) write - stella::clamp (delay, 1.0f, (float) (size - 4));
            const int i = (int) std::floor (pos);
            const float f = pos - (float) i;
            const float xm1 = buffer[(i - 1) & mask], x0 = buffer[i & mask], x1 = buffer[(i + 1) & mask], x2 = buffer[(i + 2) & mask];
            const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }
    };

    /** 2x oversampling around a nonlinearity (drive, folding, hard clipping): halfband filters
        up and down. process(x, shaper) runs shaper twice per sample at double rate. */
    struct Oversampler2x
    {
        // A 31-tap halfband low-pass (windowed sinc): flat to about 0.4 of the base rate.
        static constexpr int taps = 31;
        float up[taps] {}, down[taps] {};

        static float tap (int i) noexcept
        {
            static const struct Table
            {
                float h[taps];
                Table()
                {
                    float sum = 0.0f;
                    for (int n = 0; n < taps; ++n)
                    {
                        const int d = n - taps / 2;
                        const float sinc = d == 0 ? 0.5f : std::sin (0.5f * pi * (float) d) / (pi * (float) d);
                        const float w = 0.42f - 0.5f * std::cos (twoPi * (float) n / (taps - 1)) + 0.08f * std::cos (2.0f * twoPi * (float) n / (taps - 1));
                        h[n] = sinc * w;
                        sum += h[n];
                    }
                    for (auto& v : h) v /= sum;   // unity at DC
                }
            } table;
            return table.h[i];
        }

        template <typename Shaper>
        float process (float x, Shaper&& shaper) noexcept
        {
            float out = 0.0f;

            for (int phase = 0; phase < 2; ++phase)
            {
                for (int i = taps - 1; i > 0; --i) up[i] = up[i - 1];
                up[0] = phase == 0 ? 2.0f * x : 0.0f;       // zero-stuffing, gain 2

                float u = 0.0f;
                for (int i = 0; i < taps; ++i) u += up[i] * tap (i);

                for (int i = taps - 1; i > 0; --i) down[i] = down[i - 1];
                down[0] = shaper (u);

                if (phase == 1)
                    for (int i = 0; i < taps; ++i) out += down[i] * tap (i);
            }

            return out;
        }

        void reset() noexcept    { for (auto& v : up) v = 0.0f; for (auto& v : down) v = 0.0f; }
    };
}
```

## Using them
- Oscillator per voice: `osc.setFrequency (stella::noteToHz (note + bend), rate); float s = osc.next (dsp::Osc::saw);`. For hard sync, before the master's `next()`: `if (master.wrapsNext()) slave.reset();`.
- A voice's filter: `filter.set (cutoffHz * std::pow (2.0f, envAmount * env * 5.0f), resonance, rate);` every 16 samples, then `filter.process (x)` (Ladder) or `filter.lowpass (x)` (Svf).
- Envelopes: `amp.prepare (rate)` in `prepare()`; `amp.set (attackSeconds, decaySeconds, sustain, releaseSeconds)` once per block; `noteOn()` / `noteOff()` from the notes; `amp.next()` per sample. A voice is free when `! amp.isActive()`.
- Voices: `float levels[N]` from each voice's `amp.level`, then `allocator.noteOn (note, N, levels)` gives the voice; `allocator.noteOff (note, N)` gives the one to release (-1: none). Mono: `monoOn` / `monoOff` return the note that should sound now (glide towards it; retrigger the envelope only when no note was held before, for legato).
- Delay: `line.prepare ((int) (maxSeconds * rate) + 8)` in `prepare()`; per sample `float y = line.read (delaySamples); line.push (x + y * feedback);`.
- Oversampling: `out = os.process (in, [drive] (float v) { return dsp::fastTanh (v * drive); });` one `Oversampler2x` per channel. It adds about 0.3 ms of latency: harmless.
- Effects keep a `Smoother` per knob that changes gain or mix: `mix.next (param (k))` per sample.

## Tempo sync
The song's tempo and position are in the context: `context.tempo`, `context.beat`, `context.playing` (see the builder guide). A synced rate is a selector over `stella::syncDivisions` (its GUI a `dropdown` listing the names in order: "4 bars", "2 bars", "1 bar", "1/2 dotted", "1/2", "1/2 triplet", "1/4", "1/4 dotted", "1/4 triplet", "1/8", "1/8 dotted", "1/8 triplet", "1/16", "1/16 dotted", "1/16 triplet", "1/32"), with a switch for free or synced. An LFO that lands on the beat while the song plays and keeps its rate while it's stopped:
```cpp
// modules/SyncLfo.cpp: a sine LFO, free in Hz or synced to the song (a note length).
#include "stella_api.h"
#include <cmath>

namespace
{
    const char* const outputs[] { "out" };
    const stella::Param params[] {
        { "rate", "Rate", 0.05f, 20.0f, 2.0f, "Hz", 0.4f },
        { "sync", "Sync", 0.0f, 1.0f, 1.0f, "", 1.0f },
        { "division", "Division", 0.0f, (float) (stella::numSyncDivisions - 1), 6.0f, "", 1.0f },   // 6: "1/4"
    };
    const char* const displays[] { "position" };

    class SyncLfo final : public stella::Module
    {
    public:
        void process (const stella::Context& c, const float* const*, float* const* outputs) override
        {
            const bool synced = param (1) >= 0.5f;
            const double cycleBeats = stella::syncBeats (param (2));
            const double step = (synced ? stella::syncHz (c, cycleBeats) : (double) param (0)) / c.sampleRate;

            for (int i = 0; i < c.numFrames; ++i)
            {
                // Synced and playing: where the song is. Otherwise: on from where it was.
                phase = synced && c.playing ? stella::syncPhase (c, i, cycleBeats) : phase + step - std::floor (phase + step);
                outputs[0][i] = std::sin ((float) phase * stella::twoPi);
            }

            display (0, 0.5f + 0.5f * std::sin ((float) phase * stella::twoPi));
        }

    private:
        double phase = 0.0;
    };

    stella::Module* create() { return new SyncLfo(); }
    stella::Registrar registrar ({ "SyncLfo", "Synced LFO", nullptr, 0, outputs, 1, params, STELLA_COUNT (params), create, displays, 1 });
}
```
- A synced delay: `const float samples = (float) stella::beatsToSamples (c, stella::syncBeats (param (k)));` per block, smoothed (a `Smoother` on the delay time) so a tempo change glides instead of clicking; size the line for the longest division at a slow tempo (`stella::beatsToSamples` of 4 beats at 40 bpm is 6 s).
- Gates, arpeggiators and step sequencers step on the beat: the step is `(int) std::floor (stella::beatAt (c, i) / stepBeats)` while playing; stopped, count steps from a phase of their own.
