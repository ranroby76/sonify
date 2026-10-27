// C:\workspace\SONIFY PLUGIN natura\src\SynthCommon.h
#pragma once
//==============================================================================
//  SynthCommon.h  -  the synthesis department every voice runs after its
//  model: Grex's AHDSR (one signed curve for decay and release), the TPT
//  state-variable filter, LFOs with pre-delay, the body resonator and the
//  character stage.
//==============================================================================
#include "ModelDsp.h"

namespace natura
{
    //==========================================================================
    //  AHDSR - from Grex.  Attack overshoots towards 1.5 for punch and reaches
    //  full level in exactly the labelled time; decay and release run an exact
    //  phase through one signed curve, so every shape takes the labelled time
    //  and only the contour changes:  k > 0 front-loaded (EXP, TIGHT),
    //  k = 0 linear, k < 0 holds then falls (LOG, SWELL).
    //==========================================================================
    struct AHDSREnvelope
    {
        enum Stage { Idle, Attack, Hold, Decay, Sustain, Release };
        Stage stage = Idle;
        float curveK = 6.0f, level = 0.0f;
        float attackCoeff = 1.0f, sustainLevel = 0.0f, holdSamples = 0.0f;
        int   holdCounter = 0;
        float segStart = 0.0f, segEnd = 0.0f;
        double segPhase = 0.0, segInc = 0.0;
        float decayInc = 0.0f;

        static constexpr float kAttackTarget = 1.5f;
        static constexpr float kAttackScale  = 1.0f / 1.0986f;   // ln(1.5/0.5)

        static float msToCoeff (float ms, double sr)
        {
            if (ms < 0.1f) return 1.0f;
            return 1.0f - std::exp (-1.0f / ((float) ms * 0.001f * (float) sr));
        }

        static float fallAmount (float p, float k)
        {
            p = dsp::clampv (p, 0.0f, 1.0f);
            if (std::abs (k) < 1.0e-3f) return p;
            return (1.0f - std::exp (-k * p)) / (1.0f - std::exp (-k));
        }

        void noteOn (float attackMs, float holdMs, float decayMs, float sustain, double sr, bool fromCurrent = true)
        {
            if (! fromCurrent) level = 0.0f;
            sustainLevel = sustain;
            attackCoeff  = msToCoeff (attackMs * kAttackScale, sr);
            const double decaySamples = decayMs * 0.001 * sr;
            decayInc = (decayMs < 0.1f || decaySamples < 1.0) ? 1.0e9f : (float) (1.0 / decaySamples);
            holdSamples = (float) (sr * holdMs / 1000.0);
            holdCounter = 0;
            stage = Attack;
        }

        void noteOff (float releaseMs, double sr)
        {
            const double relSamples = releaseMs * 0.001 * sr;
            segStart = level;
            segEnd   = 0.0f;
            segPhase = 0.0;
            segInc   = (releaseMs < 0.1f || relSamples < 1.0) ? 1.0e9 : (1.0 / relSamples);
            if (stage != Idle) stage = Release;
        }

        void kill() { stage = Idle; level = 0.0f; }

        float process()
        {
            switch (stage)
            {
                case Attack:
                    level += (kAttackTarget - level) * attackCoeff;
                    if (level >= 0.999f) { level = 1.0f; stage = Hold; holdCounter = 0; }
                    break;
                case Hold:
                    if ((float) ++holdCounter >= holdSamples)
                    {
                        segStart = level; segEnd = sustainLevel;
                        segPhase = 0.0;   segInc = decayInc;
                        stage = Decay;
                    }
                    break;
                case Decay:
                    segPhase += segInc;
                    if (segPhase >= 1.0) { level = sustainLevel; stage = Sustain; }
                    else level = segStart + (segEnd - segStart) * fallAmount ((float) segPhase, curveK);
                    break;
                case Sustain:
                    level = sustainLevel;
                    break;
                case Release:
                    segPhase += segInc;
                    if (segPhase >= 1.0) { level = 0.0f; stage = Idle; }
                    else level = segStart + (segEnd - segStart) * fallAmount ((float) segPhase, curveK);
                    break;
                case Idle:
                    break;
            }
            return level;
        }

        bool isActive() const { return stage != Idle; }
    };

    //==========================================================================
    //  TPT state-variable filter.  Coefficients are set once per control block
    //  (the tan() lives there, not in the sample loop).
    //  type: 0 LP, 1 HP, 2 BP, 3 Notch
    //==========================================================================
    struct TptSvf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 1.414f;
        int   type = 0;

        void reset() { ic1 = ic2 = 0.0f; }

        void set (float cutoffHz, float resonance01, int t, double sr)
        {
            type = t;
            const float g = (float) std::tan (dsp::kPi * dsp::clampv ((double) cutoffHz, 20.0, sr * 0.45) / sr);
            // resonance 0 is Butterworth (Q 0.707), the honest "no peak" setting
            const float q = 0.7071f + resonance01 * resonance01 * 14.0f;
            k  = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        float process (float x)
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            switch (type)
            {
                case 1:  return x - k * v1 - v2;
                case 2:  return v1 * k;
                case 3:  return x - k * v1;
                default: return v2;
            }
        }
    };

    //==========================================================================
    //  Sine LFO with pre-delay and a fade-in over the same time (control rate)
    //==========================================================================
    struct DelayedLfo
    {
        double phase = 0.0;
        double elapsed = 0.0;

        void reset (double startPhase = 0.0) { phase = startPhase; elapsed = 0.0; }

        /** dt seconds have passed; returns -1..1 scaled by the fade-in. */
        float tick (float rateHz, float delayS, double dt)
        {
            elapsed += dt;
            phase += rateHz * dt;
            phase -= std::floor (phase);
            float fade = 1.0f;
            if (delayS > 0.0f)
            {
                if (elapsed < delayS) fade = 0.0f;
                else fade = (float) std::min (1.0, (elapsed - delayS) / std::max (0.05, (double) delayS * 0.5));
            }
            return fade * (float) std::sin (dsp::kTwoPi * phase);
        }
    };

    //==========================================================================
    //  BODY: four resonances of a wooden box / soundboard.  Size moves them,
    //  material sets how sharp they ring, resonance how loud, mix how much of
    //  the box the "microphone" hears.
    //==========================================================================
    struct BodyFilter
    {
        dsp::Biquad peaks[4];
        dsp::OnePoleLP air;
        float mix = 0.0f;
        float lastSize = -1.0f, lastMat = -1.0f, lastReso = -1.0f;

        void reset() { for (auto& p : peaks) p.reset(); air.reset(); }

        void configure (float size, float material, float reso, float mixAmt, double sr)
        {
            mix = mixAmt;
            if (size == lastSize && material == lastMat && reso == lastReso) return;
            lastSize = size; lastMat = material; lastReso = reso;
            static const float base[4] = { 280.0f, 470.0f, 1050.0f, 2800.0f };
            static const float gains[4] = { 1.0f, 0.85f, 0.6f, 0.45f };
            const float scale = std::pow (2.0f, (0.5f - size) * 2.4f);
            const float q = dsp::lerp (1.6f, 9.0f, material);
            const float db = reso * 13.0f;
            for (int i = 0; i < 4; ++i)
                peaks[i].peak (base[i] * scale, q, db * gains[i], sr);
            air.setCutoff (dsp::lerp (16000.0f, 5500.0f, mixAmt), sr);
        }

        float process (float x)
        {
            if (mix <= 0.001f) return x;
            float y = x;
            for (auto& p : peaks) y = p.process (y);
            y = air.process (y);
            return x + (y - x) * mix;
        }
    };

    //==========================================================================
    //  CHARACTER: drive (symmetric saturation), warmth (asymmetric: even
    //  harmonics) and a tilt EQ around 800 Hz.
    //==========================================================================
    struct Character
    {
        dsp::OnePoleLP tiltLp;
        float drive = 0.0f, warmth = 0.0f, tilt = 0.0f;
        float pre = 1.0f, bias = 0.0f, tb = 0.0f, post = 1.0f, lowG = 1.0f, highG = 1.0f;

        void reset() { tiltLp.reset(); }

        void configure (float d, float w, float t, double sr)
        {
            drive = d; warmth = w; tilt = t;
            pre  = 1.0f + d * d * 14.0f;
            bias = w * 0.35f;
            tb   = std::tanh (bias);
            post = 1.0f / std::sqrt (pre);
            tiltLp.setCutoff (800.0, sr);
            lowG  = dsp::dbToGain (-t * 6.0f);
            highG = dsp::dbToGain ( t * 6.0f);
        }

        float process (float x)
        {
            if (drive > 0.001f || warmth > 0.001f)
                x = (std::tanh (x * pre + bias) - tb) * (drive > 0.001f ? post * 1.3f : 1.0f);
            if (std::abs (tilt) > 0.005f)
            {
                const float lo = tiltLp.process (x);
                x = lo * lowG + (x - lo) * highG;
            }
            return x;
        }
    };
}
