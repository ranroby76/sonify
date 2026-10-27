// C:\workspace\SONIFY PLUGIN natura\src\ModelsStrings.h
#pragma once
//==============================================================================
//  ModelsStrings.h  -  the two string archetypes driven by hand.
//
//    PLUCKED STRING  extended Karplus-Strong: shaped pluck (finger to pick),
//                    pluck-position comb, loop losses and stiffness, palm
//                    mute, natural harmonics, pickup comb and an amp stage.
//    BOWED STRING    the classic bowed waveguide: neck and bridge halves
//                    joined at the bow by a friction curve; bow pressure,
//                    speed and position, rosin noise; pizzicato and col
//                    legno excite the same string by plucking / striking it.
//==============================================================================
#include "ModelBase.h"

namespace natura
{
    //==========================================================================
    //  PLUCKED STRING
    //  spec: 0 pluck position, 1 plectrum (finger..pick), 2 fret noise,
    //        3 palm mute, 4 pickup, 5 let ring, 6 amp on, 7 amp drive,
    //        8 amp tone, 9 key-off
    //==========================================================================
    class PluckedStringModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            const int maxLen = (int) (sr / 20.0) + 64;
            line.prepare (maxLen);
            pickBuf.prepare (maxLen);
            exc.assign ((size_t) maxLen, 0.0f);
            reset();
        }

        void reset() override
        {
            line.clear();
            pickBuf.clear();
            lpY = 0.0f;
            for (auto& a : ap) a.reset();
            excPos = excLen = 0;
            fret = {};  keyoff = {};  snap = {};  snapF.reset();
            fretBp.reset(); keyLp.reset();
            cabLp.reset(); cabHp.reset(); cabPk.reset();
        }

        bool isSustained() const override { return false; }
        Dynamics dynamics() const override { return Dynamics::Struck; }

        void start (const ModelIn& in, bool legato) override
        {
            released = false;
            noise.seed (0x6A17u + in.seed * 2654435761u + (std::uint32_t) in.note * 4099u + (std::uint32_t) (in.velocity * 1000.0f));
            configure (in);

            const bool hammerOn = legato && in.excite != Excite::Strike;
            const float vel = in.velocity;

            // the pluck: a displacement shape mixed with noise, then the plectrum
            // decides how much top it keeps
            const int L = std::max (4, std::min ((int) exc.size() - 1, (int) std::lround (period)));
            const float beta = in.harmonic ? 0.5f : dsp::lerp (0.06f, 0.45f, in.spec[0]);
            const bool  dead = in.excite == Excite::Strike;
            const float noisiness = dead ? 1.0f : dsp::clampv (0.15f + in.noise * 0.5f + in.spec[1] * 0.25f, 0.0f, 1.0f);
            float a = dsp::lerp (0.78f, 0.04f, in.spec[1]) - vel * 0.18f - in.bright * 0.15f + in.dampAdd * 0.4f;
            if (in.harmonic) a = std::max (a, 0.5f);
            a = dsp::clampv (a, 0.0f, 0.93f);

            float amp = std::pow (vel, 1.2f) * (0.35f + in.strength * 1.1f);
            if (hammerOn) amp *= 0.18f;

            float y = 0.0f, mean = 0.0f;
            for (int i = 0; i < L; ++i)
            {
                const float x  = (float) i / (float) L;
                const float tri = x < beta ? x / beta : (1.0f - x) / (1.0f - beta);
                const float v  = dsp::lerp (tri, noise.next(), noisiness);
                y = (1.0f - a) * v + a * y;
                exc[(size_t) i] = y;
                mean += y;
            }
            mean /= (float) L;
            // pluck-position comb: the node at beta is never excited
            const int pd = std::max (1, (int) std::lround (beta * L));
            for (int i = L - 1; i >= 0; --i)
            {
                const float prev = i - pd >= 0 ? exc[(size_t) (i - pd)] - mean : 0.0f;
                exc[(size_t) i] = ((exc[(size_t) i] - mean) - 0.95f * prev) * amp;
            }
            excLen = L;
            excPos = 0;

            // the plectrum's snap off the string (a pick's bright tick), or the soft thump of a
            // finger's flesh - what a plucked note starts with before the string speaks
            if (! hammerOn)
            {
                const float pick = in.spec[1];
                const float k = vel * (0.25f + 0.75f * in.transient) * (dead ? 1.5f : 1.0f);
                if (pick > 0.45f)
                {
                    snap.trigger (k * (pick - 0.3f) * 0.05f, dsp::lerp (0.003f, 0.0015f, pick), sr);
                    snapF.highpass (dsp::lerp (2000.0f, 4500.0f, pick), 0.7, sr);
                }
                else
                {
                    snap.trigger (k * (0.6f - pick) * 0.06f, 0.012f, sr);
                    snapF.lowpass (dsp::lerp (250.0f, 600.0f, pick), 0.7, sr);
                }
            }

            const float fn = in.spec[2] * (hammerOn ? 0.12f : 0.05f) * (0.4f + vel);
            if (fn > 0.0005f)
            {
                fret.trigger (fn, hammerOn ? 0.05f : 0.025f, sr);
                fretBp.bandpass (2600.0, 1.2, sr);
            }
        }

        void release (const ModelIn& in) override
        {
            released = true;
            configure (in);
            if (in.spec[9] > 0.001f)
            {
                keyoff.trigger (in.spec[9] * 0.06f, 0.03f, sr);
                keyLp.setCutoff (1400.0, sr);
            }
        }

        void render (const ModelIn& in, float* out, int n) override
        {
            if (std::abs (in.freq - lastFreq) > 0.01f || in.pedal != lastPedal || in.released != released
                || in.damping != lastDamp || in.decay != lastDecay || in.dampAdd != lastDampAdd || in.spec[3] != lastMute)
            {
                released = in.released;
                configure (in);
            }

            const float pickup  = in.spec[4];
            const float pickMix = std::min (1.0f, pickup * 4.0f) * 0.85f;
            const float pickD   = std::max (2.0f, period * dsp::lerp (0.30f, 0.06f, pickup));
            const bool  ampOn   = in.spec[6] > 0.5f;
            const float drive   = in.spec[7];
            const float pre     = 1.0f + drive * 45.0f;
            const float bias    = 0.18f * drive;
            const float tb      = std::tanh (bias);
            const float post    = 1.0f / std::sqrt (1.0f + drive * 6.0f);
            if (ampOn)
            {
                cabLp.lowpass (dsp::lerp (2400.0f, 7200.0f, in.spec[8]), 0.9, sr);
                cabHp.highpass (85.0, 0.7, sr);
                cabPk.peak (1300.0, 1.0, 3.0, sr);
            }

            for (int i = 0; i < n; ++i)
            {
                const float e = excPos < excLen ? exc[(size_t) excPos++] : 0.0f;
                const float v = line.tap (len);
                lpY = (1.0f - lpA) * v + lpA * lpY;
                float y = lpY * g;
                for (int k = 0; k < nAp; ++k) y = ap[k].process (y);
                line.write (y + e);

                pickBuf.write (v);
                float s = v - pickMix * pickBuf.tap (pickD);

                if (fret.running())   s += fretBp.process (fret.next() * noise.next());
                if (snap.running())   s += snapF.process (snap.next() * noise.next());
                if (keyoff.running()) s += keyLp.process (keyoff.next() * noise.next());

                if (ampOn)
                {
                    s = (std::tanh (s * pre + bias) - tb) * post;
                    s = cabPk.process (cabHp.process (cabLp.process (s))) * 0.5f;
                }
                out[i] = s * 0.7f;
            }
        }

    private:
        void configure (const ModelIn& in)
        {
            lastFreq = in.freq;  lastPedal = in.pedal;  lastDamp = in.damping;  lastDecay = in.decay;
            lastDampAdd = in.dampAdd;  lastMute = in.spec[3];
            const float f0 = dsp::clampv (in.freq, 20.0f, (float) sr * 0.3f);
            period = (float) (sr / f0);

            const float mute = dsp::clampv (in.spec[3] + in.dampAdd, 0.0f, 1.0f);
            float t60 = dsp::clampv (9.0f * std::pow (0.975f, (float) (in.note - 28)), 0.6f, 14.0f);
            t60 *= std::pow (2.0f, (in.decay - 0.5f) * 3.0f) * in.decayMul;
            t60  = std::exp (dsp::lerp (std::log (t60), std::log (0.12f), mute));
            if (in.excite == Excite::Strike) t60 = std::min (t60, 0.05f);

            if (released)
            {
                const float hold = std::max (dsp::clampv (in.pedal, 0.0f, 1.0f), in.spec[5]);
                t60 = std::exp (dsp::lerp (std::log (0.09f), std::log (t60), hold));
            }

            lpA = dsp::clampv (0.02f + 0.6f * in.damping + 0.55f * mute - in.bright * 0.1f, 0.0f, 0.93f);

            const double w = dsp::kTwoPi * f0 / sr;
            {
                const float need = dsp::loopGainForT60 (t60, f0) / 0.9999f;
                for (int it = 0; it < 12 && lpA > 0.0005f && dsp::OnePoleLP::magnitude (lpA, w) < need; ++it)
                    lpA *= 0.6f;
            }
            float c = -0.55f * in.inharm;
            const float lpD = dsp::OnePoleLP::phaseDelay (lpA, w);
            nAp = 2;
            const float apD = dsp::Allpass1::phaseDelay (c, w);
            while (nAp > 0 && period - lpD - apD * (float) nAp < 3.0f) --nAp;
            if (nAp == 0) c = 0.0f;
            for (auto& a : ap) a.c = c;

            len = std::max (2.0f, period - lpD - (nAp > 0 ? apD * (float) nAp : 0.0f));
            g = std::min (0.99995f, dsp::loopGainForT60 (t60, f0)
                                    / std::max (0.05f, dsp::OnePoleLP::magnitude (lpA, w) * dsp::FracDelay::lagrangeMag (len, w)));
        }

        dsp::FracDelay line, pickBuf;
        dsp::Allpass1  ap[2];
        std::vector<float> exc;
        int   excPos = 0, excLen = 0, nAp = 0;
        float lpY = 0.0f, lpA = 0.2f, g = 0.99f, len = 100.0f, period = 100.0f;
        float lastFreq = -1.0f, lastPedal = -1.0f, lastDamp = -1.0f, lastDecay = -1.0f, lastDampAdd = -1.0f, lastMute = -1.0f;
        bool  released = false;
        dsp::Burst fret, keyoff, snap;
        dsp::Biquad fretBp, cabLp, cabHp, cabPk, snapF;
        dsp::OnePoleLP keyLp;
        dsp::Noise noise;
    };

    //==========================================================================
    //  BOWED STRING
    //  spec: 0 bow pressure, 1 bow speed, 2 bow position, 3 rosin, 4 bow attack
    //==========================================================================
    class BowedStringModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            const int maxLen = (int) (sr / 20.0) + 64;
            neck.prepare (maxLen);
            bridge.prepare (maxLen);
            hill.peak (2600.0, 1.0, 4.0, sr);
            reset();
        }

        void reset() override
        {
            neck.clear(); bridge.clear();
            sfY = 0.0f;
            hill.reset();
            dc.prepare (sr, 18.0);
            dc.reset();
            hit = {};
            scratch = {};
            scratchBp.reset();
        }

        bool isSustained() const override { return true; }
        Dynamics dynamics() const override { return Dynamics::Physical; }

        void start (const ModelIn& in, bool legato) override
        {
            noise.seed (0xB0B0u + in.seed * 2654435761u + (std::uint32_t) in.note * 769u);
            mode = in.excite;
            configure (in);
            if (mode == Excite::Normal && ! legato)
            {
                // the bow grips the string at once: it starts from a Helmholtz motion of its
                // own (primed, then scaled back), with the scratch of the hair biting in
                neck.clear(); bridge.clear(); sfY = 0.0f;
                hit = {};  scratch = {};
                primeOscillation (*this, in, sr, 0.12);
                const float seed = dsp::clampv ((0.05f + 0.25f * in.transient) * (0.5f + 0.5f * in.velocity), 0.02f, 0.35f);
                neck.scale (seed); bridge.scale (seed); sfY *= seed;
                dc.reset(); hill.reset();
                const float bite = (0.25f + 0.75f * in.spec[0]) * (0.3f + in.transient) * in.velocity;
                scratch.trigger (bite * 0.035f, dsp::lerp (0.05f, 0.025f, in.transient), sr);
                scratchBp.bandpass (dsp::lerp (2200.0f, 3800.0f, in.hardness), 0.8, sr);
            }
            if (mode != Excite::Normal && ! legato)
            {
                // pizzicato / col legno: set the string going without the bow
                const float ms = mode == Excite::Strike ? 0.25f : 1.2f;
                hit.trigger ((float) (ms * 0.001 * sr), std::pow (in.velocity, 1.2f) * (mode == Excite::Strike ? 0.45f : 0.3f));
                hitNoise = mode == Excite::Strike ? 0.8f : 0.15f;
            }
        }

        void release (const ModelIn& in) override { configure (in); }

        void render (const ModelIn& in, float* out, int n) override
        {
            if (std::abs (in.freq - lastFreq) > 0.01f || in.damping != lastDamp || in.decay != lastDecay
                || in.spec[2] != lastPos || in.released != lastRel)
                configure (in);

            const bool  bowing = mode == Excite::Normal;
            // high strings and a bow near the bridge both ask for more force and
            // less speed (the Schelleng window) - keep the knobs inside it
            const float reg    = dsp::clampv (1.0f - (float) (in.note - 60) / 60.0f, 0.5f, 1.25f);
            const float maxVel = 0.03f + 0.17f * in.spec[1] * (0.5f + in.strength) * reg;
            const float force  = 0.35f + 0.65f * (in.spec[0] * 0.8f + in.hardness * 0.2f);
            const float slope  = dsp::clampv ((5.0f - 4.0f * force) * std::sqrt (betaNow / 0.2f), 0.6f, 5.0f);
            const float rosin  = in.spec[3] * 0.012f + in.noise * 0.006f;

            for (int i = 0; i < n; ++i)
            {
                const float bridgeOut = bridge.tap (dBridge);
                const float neckOut   = neck.tap (dNeck);

                sfY = sfB * bridgeOut + sfP * sfY;
                const float bridgeRefl = -sfY * sfG;
                const float nutRefl    = -neckOut;
                const float stringVel  = bridgeRefl + nutRefl;

                float newVel = 0.0f;
                if (bowing)
                {
                    float bowVel = maxVel * in.pressure * (0.35f + 0.65f * in.dyn);
                    if (rosin > 0.0f) bowVel += noise.next() * rosin * in.pressure;
                    const float dv = bowVel - stringVel;
                    float t = std::abs ((dv) * slope) + 0.75f;
                    t = 1.0f / (t * t * t * t);
                    t = dsp::clampv (t, 0.01f, 0.98f);
                    newVel = dv * t;
                }
                else if (hit.running())
                {
                    float h = hit.next();
                    h *= 1.0f + hitNoise * noise.next();
                    newVel = h;
                }

                neck.write (bridgeRefl + newVel);
                bridge.write (nutRefl + newVel);

                float o = hill.process (dc.process (bridgeOut)) * 0.85f;
                if (scratch.running()) o += scratchBp.process (scratch.next() * noise.next());
                out[i] = o;
            }
        }

    private:
        void configure (const ModelIn& in)
        {
            lastFreq = in.freq;  lastDamp = in.damping;  lastDecay = in.decay;  lastPos = in.spec[2];  lastRel = in.released;
            const float f0 = dsp::clampv (in.freq, 20.0f, (float) sr * 0.25f);
            float beta = dsp::lerp (0.07f, 0.30f, in.spec[2]);

            if (mode == Excite::Normal)
            {
                // the string's losses while bowed
                sfP = dsp::clampv (0.75f - 0.2f * 22050.0f / (float) sr + (in.damping - 0.5f) * 0.35f, 0.05f, 0.95f);
                sfB = 1.0f - sfP;
                sfG = dsp::clampv (0.93f + 0.06f * in.decay, 0.85f, 0.995f);
            }
            else
            {
                // plucked / struck: a free string that rings out
                sfP = dsp::clampv (0.25f + 0.5f * in.damping, 0.02f, 0.9f);
                sfB = 1.0f - sfP;
                float t60 = dsp::clampv (1.4f * std::pow (0.975f, (float) (in.note - 43)), 0.25f, 3.0f)
                          * std::pow (2.0f, (in.decay - 0.5f) * 2.0f) * in.decayMul;
                if (mode == Excite::Strike) t60 *= 0.25f;
                if (in.released && in.pedal < 0.5f) t60 = std::min (t60, 0.15f);
                const double w = dsp::kTwoPi * f0 / sr;
                sfG = std::min (0.9995f, dsp::loopGainForT60 (t60, f0) / std::max (0.05f, dsp::OnePoleLP::magnitude (sfP, w)));
            }

            const double w = dsp::kTwoPi * f0 / sr;
            const float lpD = dsp::OnePoleLP::phaseDelay (sfP, w);
            const float base = std::max (4.0f, (float) (sr / f0) - lpD + 0.25f);
            beta = std::max (beta, 3.5f / base);              // the bow can't sit on the bridge
            betaNow = beta;
            dBridge = base * beta;
            dNeck   = std::max (2.0f, base - dBridge);
        }

        dsp::FracDelay neck, bridge;
        dsp::Biquad hill;
        dsp::DCBlocker dc;
        dsp::Pulse hit;
        dsp::Burst scratch;
        dsp::Biquad scratchBp;
        dsp::Noise noise;
        Excite mode = Excite::Normal;
        float sfY = 0.0f, sfP = 0.6f, sfB = 0.4f, sfG = 0.95f;
        float dBridge = 20.0f, dNeck = 100.0f, hitNoise = 0.0f, betaNow = 0.13f;
        float lastFreq = -1.0f, lastDamp = -1.0f, lastDecay = -1.0f, lastPos = -1.0f;
        bool  lastRel = false;
    };
}
