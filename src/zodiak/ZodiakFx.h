// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakFx.h
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "ZodiakParams.h"
#include "I3QFilters.h"
#include "ZodiakFilters.h"

//==============================================================================
//  ZodiakFx.h  —  the effects every channel runs its sound through:
//
//      VowelFilter   eight columns, each one vowel (A E O U I); a marker runs
//                    across them at the song's tempo and the filter morphs
//                    from vowel to vowel: LOOP, PING-PONG, or ONCE per note
//      ChannelFilter one filter: I3Q's 17 types, then Zodiak's 20 models (Avenger's
//                    families, the crying ones); OFF passes the sound through
//      TranceGate    16 steps chopping the sound at the song's tempo
//      SyncDelay     a ping-pong echo locked to the song's tempo (left, right, left ...)
//
//  Each one is plain C++: prepare() allocates, the rest runs on the audio
//  thread without allocating.  The tempo-synced ones follow the song's
//  position while it plays (so they land on the beat wherever it starts) and
//  run on their own clock while it's stopped.
//==============================================================================

namespace zodiak::fx
{
    constexpr float kPi = 3.14159265358979f;

    inline float softClip (float x) noexcept
    {
        x = std::clamp (x, -3.0f, 3.0f);
        return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
    }

    inline float onePole (float ms, double sr) noexcept
    {
        return 1.0f - std::exp (-1.0f / std::max (1.0f, ms * 0.001f * (float) sr));
    }

    inline double divisionBeats (float index) noexcept    { return (double) stella::syncBeats (index); }

    //==========================================================================
    //  A zero-delay state-variable band-pass (unity at its centre)
    //==========================================================================
    struct BandPass
    {
        float a1 = 0, a2 = 0, a3 = 0, k = 1, ic1 = 0, ic2 = 0;

        void set (float hz, float q, float sr) noexcept
        {
            const float g = std::tan (kPi * std::clamp (hz, 20.0f, sr * 0.45f) / sr);
            k = 1.0f / std::max (0.3f, q);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        float tick (float x) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return k * v1;
        }

        void clear() noexcept   { ic1 = ic2 = 0.0f; }
    };

    //==========================================================================
    //  VowelFilter
    //==========================================================================
    class VowelFilter
    {
    public:
        static constexpr int kFormants = 3;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            mixSmooth = onePole (20.0f, sr);
            reset();
        }

        void reset() noexcept
        {
            for (auto& ch : bands) for (auto& b : ch) b.clear();
            for (auto& ch : bands2) for (auto& b : ch) b.clear();
            freeSteps = noteSteps = 0.0;
            mixNow = 0.0f;
            countdown = 0;
        }

        /** A note started on the channel (ONCE starts its run again). */
        void noteOn() noexcept   { noteSteps = 0.0; }

        /** The marker now: 0 (column 1) .. 6 (back at column 1). */
        float position() const noexcept   { return shownPos.load (std::memory_order_relaxed); }

        void process (float* L, float* R, int n, const Transport& t,
                      float mix, float res, float rateIndex, int mode, const std::array<int, kVowelColumns>& cols) noexcept
        {
            const double stepBeats = std::max (1.0e-3, divisionBeats (rateIndex));
            const double stepsPerSample = t.beatsPerSample() / stepBeats;
            lastStepBeats = stepBeats;

            if (mix <= 0.0f && mixNow < 1.0e-4f)
            {
                // Silent: keep the clocks moving, nothing else
                freeSteps += stepsPerSample * n;
                noteSteps += stepsPerSample * n;
                mixNow = 0.0f;
                shownPos.store ((float) positionFor (t, mode), std::memory_order_relaxed);
                return;
            }

            for (int i = 0; i < n; ++i)
            {
                if (--countdown <= 0)
                {
                    countdown = kUpdate;
                    const auto tt = t.advancedBy (i);
                    updateFormants (positionFor (tt, mode), mode, res, cols);
                }

                mixNow += (mix - mixNow) * mixSmooth;

                float wl = 0.0f, wr = 0.0f;
                for (int f = 0; f < kFormants; ++f)
                {
                    // Two band-passes in a row: steep sides, so each formant stands apart
                    wl += gains[f] * bands2[0][(size_t) f].tick (bands[0][(size_t) f].tick (L[i]));
                    wr += gains[f] * bands2[1][(size_t) f].tick (bands[1][(size_t) f].tick (R[i]));
                }
                wl = softClip (wl * kMakeup);
                wr = softClip (wr * kMakeup);

                L[i] += (wl - L[i]) * mixNow;
                R[i] += (wr - R[i]) * mixNow;

                freeSteps += stepsPerSample;
                noteSteps += stepsPerSample;
            }
            shownPos.store ((float) positionFor (t.advancedBy (n), mode), std::memory_order_relaxed);
        }

    private:
        static constexpr int   kUpdate = 16;
        static constexpr float kMakeup = 3.4f;          // about the dry level on a full sound
        static constexpr float kHold = 0.55f;           // of a step the vowel is held; it moves to the next in the rest

        // Formant frequency (Hz), level (dB) and bandwidth (Hz) of each vowel, in the columns' order A E O U I
        struct Formant { float hz, db, bw; };
        static const Formant& formant (int vowel, int f) noexcept
        {
            static const Formant t[kNumVowels][kFormants] =
            {
                // A talking synth's vowels: the spoken formants, the upper two strong (they tell
                // the vowels apart; at a singer's levels a synth only "wah"s)
                { { 730.0f,  0.0f, 90.0f }, { 1090.0f,  -2.0f, 100.0f }, { 2440.0f,  -8.0f, 130.0f } },    // A
                { { 530.0f,  0.0f, 70.0f }, { 1840.0f,  -3.0f, 110.0f }, { 2480.0f,  -7.0f, 130.0f } },    // E
                { { 570.0f,  0.0f, 70.0f }, {  840.0f,  -3.0f,  90.0f }, { 2410.0f, -11.0f, 120.0f } },    // O
                { { 300.0f,  0.0f, 60.0f }, {  870.0f,  -5.0f,  80.0f }, { 2240.0f, -13.0f, 140.0f } },    // U
                { { 270.0f,  0.0f, 60.0f }, { 2290.0f,  -2.0f, 110.0f }, { 3010.0f,  -5.0f, 130.0f } },    // I
            };
            return t[std::clamp (vowel, 0, kNumVowels - 1)][f];
        }

        double positionFor (const Transport& t, int mode) const noexcept
        {
            const double steps = mode == 2 ? noteSteps : (t.playing ? t.ppq / lastStepBeats : freeSteps);
            if (mode == 2) return std::min (steps, (double) (kVowelColumns - 1));
            if (mode == 1)
            {
                const double cyc = 2.0 * (kVowelColumns - 1);
                const double p = std::fmod (std::max (0.0, steps), cyc);
                return p <= kVowelColumns - 1 ? p : cyc - p;
            }
            return std::fmod (std::max (0.0, steps), (double) kVowelColumns);
        }

        void updateFormants (double pos, int mode, float res, const std::array<int, kVowelColumns>& cols) noexcept
        {
            const int   i0 = std::clamp ((int) std::floor (pos), 0, kVowelColumns - 1);
            // A vowel is held, then moves quickly to the next (speech: not one long sweep)
            float fr = ((float) (pos - std::floor (pos)) - kHold) / (1.0f - kHold);
            fr = std::clamp (fr, 0.0f, 1.0f);
            fr = fr * fr * (3.0f - 2.0f * fr);
            const int   i1 = mode == 0 ? (i0 + 1) % kVowelColumns : std::min (i0 + 1, kVowelColumns - 1);
            const int   va = cols[(size_t) i0], vb = cols[(size_t) i1];
            const float bwScale = 2.2f - 1.8f * std::clamp (res, 0.0f, 1.0f);       // wide .. narrow (screaming)
            float firstHz = 500.0f;

            for (int f = 0; f < kFormants; ++f)
            {
                const auto& a = formant (va, f);
                const auto& b = formant (vb, f);
                const float hz = a.hz * std::pow (b.hz / a.hz, fr);
                const float bw = (a.bw + (b.bw - a.bw) * fr) * bwScale;
                const float db = a.db + (b.db - a.db) * fr;
                // A synth's harmonics fall away upward: the upper formants get half of that back
                // (3 dB an octave above the first), or they'd be lost under it
                if (f == 0) firstHz = hz;
                gains[f] = std::pow (10.0f, db / 20.0f) * std::sqrt (hz / firstHz);
                for (int c = 0; c < 2; ++c)
                {
                    // (two in a row are narrower than one: each a little wider keeps the formant's width)
                    bands[(size_t) c][(size_t) f].set (hz, 0.7f * hz / bw, (float) sr);
                    bands2[(size_t) c][(size_t) f].set (hz, 0.7f * hz / bw, (float) sr);
                }
            }
        }

        double sr = 48000.0;
        std::array<std::array<BandPass, kFormants>, 2> bands, bands2;
        float  gains[kFormants] { 1.0f, 1.0f, 1.0f };
        double freeSteps = 0.0, noteSteps = 0.0, lastStepBeats = 1.0;
        float  mixNow = 0.0f, mixSmooth = 0.01f;
        int    countdown = 0;
        std::atomic<float> shownPos { 0.0f };
    };

    //==========================================================================
    //  The filter's types: 0 OFF, 1..17 I3Q's, then Zodiak's models
    //  (ZodiakFilters.h: Avenger's families and the crying ones)
    //==========================================================================
    static constexpr int kFirstModelType = 1 + flt::numTypes;                 // 18
    static constexpr int kNumFilterTypes = kFirstModelType + zfl::numKinds;   // with OFF

    /** One channel of the filter, any type but OFF (1..). */
    class FilterVoice
    {
    public:
        void prepare (double sampleRate)   { i3q.prepare (sampleRate); model.prepare (sampleRate); }
        void reset() noexcept              { i3q.reset(); model.reset(); }

        void setType (int type) noexcept
        {
            isModel = type >= kFirstModelType;
            if (isModel) model.setKind (type - kFirstModelType);
            else         i3q.setType (type - 1);
        }

        void setParams (float cutoff, float res, float drive) noexcept
        {
            if (isModel) model.setParams (cutoff, res, drive);
            else         i3q.setParams (cutoff, res);
        }

        float process (float x) noexcept   { return isModel ? model.process (x) : i3q.process (x); }

    private:
        flt::Filter i3q;
        zfl::Model  model;
        bool isModel = false;
    };

    /** Where DRIVE goes: into the filter (the screaming ones), as the band (SSM), or
        a tanh before it (the rest). */
    inline bool driveIsInside (int type) noexcept
    {
        const int k = type - kFirstModelType;
        return k == zfl::ssmBp || k == zfl::ssmBs || k == zfl::screamLp || k == zfl::screamHp || k == zfl::cryBp || k == zfl::rave24;
    }

    //==========================================================================
    //  ChannelFilter: the filter on the channel (type 0 = OFF)
    //==========================================================================
    class ChannelFilter
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            for (auto& f : filt) f.prepare (sampleRate);
            cutSmooth = onePole (25.0f, sr);
            reset();
        }

        void reset() noexcept
        {
            for (auto& f : filt) f.reset();
            countdown = 0;
            cutNow = -1.0f;
        }

        /** modOct: per sample, octaves added to the (smoothed) cutoff - the node's
            envelope; nullptr: none.  The filter follows it every 16 samples.
            drive 0..1 (see driveIsInside). */
        void process (float* L, float* R, int n, int type, float cutoff, float res, const float* modOct = nullptr, float drive = 0.0f) noexcept
        {
            if (type <= 0)
            {
                lastType = 0;
                cutNow = -1.0f;
                return;
            }
            type = std::min (type, kNumFilterTypes - 1);
            if (type != lastType)
            {
                for (auto& f : filt) { f.setType (type); f.reset(); }
                lastType = type;
            }
            if (cutNow < 0.0f) cutNow = cutoff;

            // DRIVE before the filter: tanh, its peaks kept where they were
            const bool inside = driveIsInside (type);
            const float pre = inside ? 0.0f : std::clamp (drive, 0.0f, 1.0f);
            const float preGain = 1.0f + 7.0f * pre, preNorm = pre > 0.0f ? 1.0f / std::tanh (preGain) : 1.0f;

            const float logTarget = std::log (std::max (20.0f, cutoff));
            for (int i = 0; i < n; ++i)
            {
                if (--countdown <= 0)
                {
                    countdown = kUpdate;
                    const float logNow = std::log (std::max (20.0f, cutNow));
                    cutNow = std::exp (logNow + (logTarget - logNow) * std::min (1.0f, cutSmooth * kUpdate));
                    float cut = cutNow;
                    if (modOct != nullptr)
                        cut = std::clamp (cutNow * std::exp2 (modOct[i]), 20.0f, (float) (sr * 0.45));
                    for (auto& f : filt) f.setParams (cut, res, inside ? drive : 0.0f);
                }
                float l = L[i], r = R[i];
                if (pre > 0.0f) { l = std::tanh (l * preGain) * preNorm; r = std::tanh (r * preGain) * preNorm; }
                L[i] = filt[0].process (l);
                R[i] = filt[1].process (r);
            }
        }

    private:
        static constexpr int kUpdate = 16;
        double sr = 48000.0;
        FilterVoice filt[2];
        int   lastType = 0, countdown = 0;
        float cutNow = -1.0f, cutSmooth = 0.01f;
    };

    //==========================================================================
    //  TranceGate
    //==========================================================================
    class TranceGate
    {
    public:
        void prepare (double sampleRate)   { sr = sampleRate; reset(); }

        void reset() noexcept   { env = 1.0f; freeSteps = 0.0; depthNow = 0.0f; }

        /** The step playing now (0..15), for the window. */
        int currentStep() const noexcept   { return shownStep.load (std::memory_order_relaxed); }

        void process (float* L, float* R, int n, const Transport& t, float depth, float rateIndex,
                      float attackMs, float releaseMs, int steps) noexcept
        {
            const double stepBeats = std::max (1.0e-3, divisionBeats (rateIndex));
            const double stepsPerSample = t.beatsPerSample() / stepBeats;
            const float att = onePole (attackMs, sr), rel = onePole (releaseMs, sr);
            const float dSmooth = onePole (20.0f, sr);

            double s = t.playing ? t.ppq / stepBeats : freeSteps;
            for (int i = 0; i < n; ++i)
            {
                depthNow += (depth - depthNow) * dSmooth;
                const int step = ((int) std::floor (s)) & (kGateSteps - 1);
                const float target = ((steps >> step) & 1) ? 1.0f : 0.0f;
                env += (target - env) * (target > env ? att : rel);
                const float g = 1.0f - depthNow * (1.0f - env);
                L[i] *= g;
                R[i] *= g;
                s += stepsPerSample;
            }
            freeSteps = t.playing ? s : freeSteps + stepsPerSample * n;
            shownStep.store (((int) std::floor (s)) & (kGateSteps - 1), std::memory_order_relaxed);
        }

    private:
        double sr = 48000.0, freeSteps = 0.0;
        float  env = 1.0f, depthNow = 0.0f;
        std::atomic<int> shownStep { 0 };
    };

    //==========================================================================
    //  SyncDelay: ping-pong.  Its time is one of two things, never a mix:
    //
    //      SYNC on    a note value at the song's tempo, to the sample.  A new
    //                 note value (or tempo) is crossfaded to in 30 ms: no slide,
    //                 no pitch swoop, the echoes just land on the new grid.
    //      SYNC off   milliseconds, whatever the tempo.  Moving it slides the
    //                 time like a tape head (the pitch bends on the way).
    //
    //  The sound goes into the left line, the left line's echo into the right
    //  line, and the right line's echo - times FEEDBACK - back into the left.
    //  So the echoes come left, right, left, right, one TIME apart, in pairs:
    //  the first two at full level, each pair after that FEEDBACK times the one
    //  before (the right side is as loud as the left, at any FEEDBACK).
    //
    //  Every echo, the first too, passes TONE (a low-pass) and a fixed low cut
    //  (the echoes of a bass or a kick don't pile up under the sound): each
    //  one is a little darker than the one before.  A fifth of each side spills
    //  into the other, so an echo is never in one ear alone.  MIX is the echoes'
    //  level on top of the sound, which stays as it is.
    //==========================================================================
    class SyncDelay
    {
    public:
        static constexpr double kMaxSeconds = 6.5;      // one bar at 37 BPM
        static constexpr float  kLowCutHz = 110.0f, kSpill = 0.2f;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            size = (int) (kMaxSeconds * sr) + 4;
            for (auto& b : buf) b.assign ((size_t) size, 0.0f);
            timeSmooth = onePole (60.0f, sr);
            fadeStep   = 1.0f / std::max (1.0f, 0.03f * (float) sr);
            mixSmooth  = onePole (20.0f, sr);
            reset();
        }

        void reset() noexcept
        {
            for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
            pos = 0;
            lp[0] = lp[1] = hp[0] = hp[1] = 0.0f;
            timeNow = -1.0f;
            mixNow = 0.0f;
            idle = 0;
        }

        /** synced: the time is `beats` quarter notes at the song's tempo; else `freeMs` milliseconds. */
        void process (float* L, float* R, int n, const Transport& t, bool synced, float beats, float freeMs,
                      float feedback, float toneHz, float mix) noexcept
        {
            if (size <= 4) return;

            // Long silent: nothing to do (and nothing left ringing)
            if (mix <= 0.0f && mixNow < 1.0e-4f)
            {
                if (idle < size) { idle += n; if (idle >= size) for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }
                mixNow = 0.0f;
                timeNow = -1.0f;
                return;
            }
            idle = 0;

            const double seconds = synced ? (double) beats * 60.0 / (t.bpm > 1.0 ? t.bpm : 120.0) : (double) freeMs * 0.001;
            const float target = (float) std::clamp (seconds * sr, 2.0, (double) size - 3.0);
            if (timeNow < 0.0f) { timeNow = timeFrom = target; fade = 1.0f; }
            const float toneA = 1.0f - std::exp (-2.0f * kPi * std::clamp (toneHz, 200.0f, (float) sr * 0.45f) / (float) sr);
            const float lowCutA = 1.0f - std::exp (-2.0f * kPi * kLowCutHz / (float) sr);
            const float fb = std::clamp (feedback, 0.0f, 0.95f);

            for (int i = 0; i < n; ++i)
            {
                mixNow += (mix - mixNow) * mixSmooth;

                if (synced)
                {
                    // On the note, exactly: a new note value or tempo is faded to, never slid to
                    if (fade < 1.0f) fade = std::min (1.0f, fade + fadeStep);
                    else if (std::abs (target - timeNow) > 0.25f) { timeFrom = timeNow; timeNow = target; fade = 0.0f; }
                }
                else
                {
                    if (fade < 1.0f) fade = std::min (1.0f, fade + fadeStep);
                    timeNow += (target - timeNow) * timeSmooth;       // the tape head slides
                }

                float a = tap (buf[0], timeNow);
                float b = tap (buf[1], timeNow);
                if (fade < 1.0f)
                {
                    const float a0 = tap (buf[0], timeFrom), b0 = tap (buf[1], timeFrom);
                    a = a0 + (a - a0) * fade;
                    b = b0 + (b - b0) * fade;
                }

                // Every echo passes the tone filter and the low cut: the first one too
                lp[0] += (a - lp[0]) * toneA;
                lp[1] += (b - lp[1]) * toneA;
                hp[0] += (lp[0] - hp[0]) * lowCutA;
                hp[1] += (lp[1] - hp[1]) * lowCutA;
                const float wa = lp[0] - hp[0], wb = lp[1] - hp[1];

                // In -> the left line -> the right line -> back (x feedback) into the left
                const float in = 0.5f * (L[i] + R[i]);
                buf[0][(size_t) pos] = softClip (in + fb * wb);
                buf[1][(size_t) pos] = wa;
                if (++pos >= size) pos = 0;

                L[i] += (wa + kSpill * wb) * mixNow;
                R[i] += (wb + kSpill * wa) * mixNow;
            }
        }

    private:
        float tap (const std::vector<float>& b, float delay) const noexcept
        {
            float r = (float) pos - delay;
            while (r < 0.0f) r += (float) size;
            const int i0 = (int) r;
            const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
            const float f = r - (float) i0;
            return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * f;
        }

        double sr = 48000.0;
        std::array<std::vector<float>, 2> buf;
        int   size = 0, pos = 0, idle = 0;
        float lp[2] {}, hp[2] {}, timeFrom = 0.0f, fade = 1.0f, fadeStep = 0.001f, timeNow = -1.0f, timeSmooth = 0.001f, mixNow = 0.0f, mixSmooth = 0.01f;
    };
}
