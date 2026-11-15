// D:\workspace\Fanan_web_hub\src\timberwolf\blocks\BlocksChokehold.cpp
//==============================================================================
//  chokehold's effects (github.com/chkhld/jsfx, MIT, Copyright (c) 2019-2025
//  chokehold), ported from his JSFX to C++ line by line: Amp Sim, Cabinet Sim
//  (with its five impulse responses), Chug Thug, the clippers, Foldback,
//  Signal Crusher, Telephone, EQ 560, Filthy Delay, Ring Mod, Gate/Expander
//  and Track Comp.  The arithmetic is his; what's Timber Wolf's is the block
//  around it (its knobs, tempo sync for the delay, one pass for a mono guitar).
//
//  MIT License: Permission is hereby granted, free of charge, to any person
//  obtaining a copy of this software and associated documentation files (the
//  "Software"), to deal in the Software without restriction, including without
//  limitation the rights to use, copy, modify, merge, publish, distribute,
//  sublicense, and/or sell copies of the Software, and to permit persons to
//  whom the Software is furnished to do so, subject to the following
//  conditions: The above copyright notice and this permission notice shall be
//  included in all copies or substantial portions of the Software.  THE
//  SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
//==============================================================================
#include "BlockKit.h"
#include "ChokeholdCabs.h"
#include "../TwRegistry.h"

namespace tw
{
    namespace
    {
        const juce::String chokeCredit = "chokehold (github.com/chkhld/jsfx), MIT";
        constexpr double dPi = juce::MathConstants<double>::pi;

        inline double dbToGainD (double db)    { return std::pow (10.0, db * 0.05); }

        /** chokehold's state-variable EQ (Andrew Simper's), as his JSFX writes it. */
        struct Eq
        {
            double a1 = 0, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0, ic1 = 0, ic2 = 0;

            static double gOf (double sr, double hz)   { return std::tan (dPi * juce::jlimit (1.0, sr * 0.499, hz) / sr); }
            void coeffs (double g, double k)          { a1 = 1.0 / (1.0 + g * (g + k)); a2 = a1 * g; a3 = a2 * g; }

            void pk (double sr, double hz, double q, double db)
            {
                const double A = std::pow (10.0, db * 0.025), g = gOf (sr, hz), k = 1.0 / (q * A);
                coeffs (g, k); m0 = 1; m1 = k * (A * A - 1.0); m2 = 0;
            }
            void hp (double sr, double hz, double q)   { const double g = gOf (sr, hz), k = 1.0 / q; coeffs (g, k); m0 = 1; m1 = -k; m2 = -1; }
            void lp (double sr, double hz, double q)   { const double g = gOf (sr, hz), k = 1.0 / q; coeffs (g, k); m0 = 0; m1 = 0; m2 = 1; }
            void bp (double sr, double hz, double q)   { const double g = gOf (sr, hz), k = 1.0 / q; coeffs (g, k); m0 = 0; m1 = 1; m2 = 0; }
            void ls (double sr, double hz, double q, double db)
            {
                const double A = std::pow (10.0, db * 0.025), g = gOf (sr, hz), k = 1.0 / q;
                coeffs (g, k); m0 = 1; m1 = k * (A - 1.0); m2 = A * A - 1.0;
            }
            void hs (double sr, double hz, double q, double db)
            {
                const double A = std::pow (10.0, db * 0.025), g = gOf (sr, hz), k = 1.0 / q;
                coeffs (g, k); m0 = A * A; m1 = k * (1.0 - A) * A; m2 = 1.0 - m0;
            }

            double tick (double x) noexcept
            {
                const double v3 = x - ic2;
                const double v1 = a1 * ic1 + a2 * v3;
                const double v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0 * v1 - ic1;
                ic2 = 2.0 * v2 - ic2;
                return m0 * x + m1 * v1 + m2 * v2;
            }

            void reset() noexcept   { ic1 = ic2 = 0; }
        };

        /** His Butterworth lowpass: a stack of 2nd-order sections (his oversampling filters). */
        struct Bw
        {
            static constexpr int maxOrder = 32;
            int order = 0;
            double a[maxOrder] {}, d1[maxOrder] {}, d2[maxOrder] {}, w0[maxOrder] {}, w1[maxOrder] {}, w2[maxOrder] {};

            void set (double hz, double sr, int n)
            {
                order = juce::jlimit (1, maxOrder, n);
                const double a1 = std::tan (dPi * juce::jlimit (1.0, sr * 0.499, hz) / sr), a2 = a1 * a1, ro4 = 1.0 / (4.0 * order);

                for (int s = 0; s < order; ++s)
                {
                    const double r = std::sin (dPi * (2.0 * s + 1.0) * ro4), ar2 = 2.0 * a1 * r, s2 = a2 + ar2 + 1.0, rs2 = 1.0 / s2;
                    a[s] = a2 * rs2; d1[s] = 2.0 * (1.0 - a2) * rs2; d2[s] = -(a2 - ar2 + 1.0) * rs2;
                }

                reset();
            }

            double tick (double x) noexcept
            {
                for (int s = 0; s < order; ++s)
                {
                    w0[s] = d1[s] * w1[s] + d2[s] * w2[s] + x;
                    x = a[s] * (w0[s] + 2.0 * w1[s] + w2[s]);
                    w2[s] = w1[s]; w1[s] = w0[s];
                }

                return x;
            }

            void reset() noexcept
            {
                std::fill (std::begin (w0), std::end (w0), 0.0);
                std::fill (std::begin (w1), std::end (w1), 0.0);
                std::fill (std::begin (w2), std::end (w2), 0.0);
            }
        };

        /** His envelope follower ("pow(0.01, ...)": the time to fall to 1 %). */
        struct Env
        {
            double env = 0, att = 0, rel = 0;

            void set (double sr, double msA, double msR)
            {
                att = std::pow (0.01, 1.0 / (juce::jmax (0.001, msA) * sr * 0.001));
                rel = std::pow (0.01, 1.0 / (juce::jmax (0.001, msR) * sr * 0.001));
            }

            double follow (double x) noexcept
            {
                const double a = std::abs (x);
                env = ((a > env) ? att : rel) * (env - a) + a;
                return env;
            }

            void reset() noexcept   { env = 0; }
        };

        /** His attack/release in dB ("exp(-1000 / (ms * srate))"). */
        struct AttRel
        {
            double env = 0, ca = 0, cr = 0;
            void set (double sr, double msA, double msR)
            {
                ca = msA > 0 ? std::exp (-1000.0 / (msA * sr)) : 0.0;
                cr = msR > 0 ? std::exp (-1000.0 / (msR * sr)) : 0.0;
            }
            double tick (double x) noexcept   { env = x + ((x > env) ? ca : cr) * (env - x); return env; }
            void reset() noexcept             { env = 0; }
        };

        struct DcBlock
        {
            double in = 0, out = 0;
            double tick (double x) noexcept   { out *= 0.99988487; out += x - in; in = x; return out; }
            void reset() noexcept             { in = out = 0; }
        };

        inline double hard (double x)        { return juce::jlimit (-1.0, 1.0, x); }
        inline double soft (double x)        { return std::tanh (x); }
        inline double decelerate (double v)  { return 1.0 - (1.0 - v) * (1.0 - v); }

        /** His fast tanh. */
        inline double fastTanh (double n)
        {
            const double xa = std::abs (n), x2 = xa * xa, x3 = x2 * xa, x4 = x2 * x2, x7 = x4 * x3;
            const double res = 1.0 - 1.0 / (1.0 + xa + x2 + 0.58576695 * x3 + 0.55442112 * x4 + 0.057481508 * x7);
            return n < 0 ? -res : res;
        }

        inline double gainToDb (double g)    { return std::log (juce::jmax (0.0000000630957, std::abs (g))) * 8.68588963806503655; }

        /** His compressor's gain computer: threshold, ratio, knee, range, and its makeup. */
        struct GainCalc
        {
            double threshold = 0, ratio = 1, makeup = 1, knee = 0, kneeLower = 0, kneeUpper = 0, range = -144;

            void set (double thr, double fullRatio, double kneeDb, double rangeDb)
            {
                threshold = thr;
                ratio = 1.0 / juce::jmax (0.01, fullRatio);
                makeup = dbToGainD (std::abs ((threshold + std::abs (threshold) * ratio) * 0.4));
                knee = kneeDb;
                kneeUpper = threshold + knee * 0.5;
                kneeLower = threshold - knee * 0.5;
                range = rangeDb;
            }

            double tick (double db) const noexcept
            {
                double slope = 1.0 - ratio, reduction;

                if (knee > 0.0 && db > kneeLower && db < kneeUpper)
                {
                    slope *= ((db - kneeLower) / knee) * 0.5;
                    reduction = slope * (kneeLower - db);
                }
                else
                {
                    reduction = juce::jmin (0.0, slope * (threshold - db));
                }

                return dbToGainD (juce::jmax (reduction, range));
            }
        };

        struct Comp
        {
            AttRel attRel;
            GainCalc calc;
            double gr = 1, feedback = 0;
            bool autoMakeup = true;

            double tick (double key) noexcept
            {
                const double fb = 1.0 - (1.0 - gr) * feedback;
                attRel.tick (gainToDb (key * fb));
                gr = calc.tick (attRel.env);
                return gr * (autoMakeup ? calc.makeup : 1.0);
            }

            void reset() noexcept   { attRel.reset(); gr = 1; }
        };

        //======================================================================
        //  Amp Sim
        //======================================================================
        class AmpSim final : public Block
        {
        public:
            void prepare (double sr, int maxBlock) override
            {
                rate = sr;
                juce::ignoreUnused (maxBlock);
                ratio = -1;
                for (auto& c : ch) c.setupEnvs (sr);
                reset();
            }

            void reset() override
            {
                for (auto& c : ch) c.reset();
                ratio = -1;
            }

            void process (float* l, float* r, int n) override
            {
                update();
                const bool mono = kit::isMono (l, r, n);

                for (int i = 0; i < n; ++i)
                {
                    l[i] = (float) ch[0].run (l[i], *this);
                    if (mono) r[i] = l[i];
                    else      r[i] = (float) ch[1].run (r[i], *this);
                }
            }

        private:
            struct Channel
            {
                Eq bHp, bLp, inLs, inHs, inHp, inLp, e1, e2, e3, e4, e5, outLs, outHs, b1, b2, b3, tenK, depthEq, presEq;
                Bw up, dn;
                Env gateEnv, gateRev, ampEnv, maxEnv, maxRev;
                DcBlock dc;
                double buffer[16] {};

                void setupEnvs (double sr)
                {
                    ampEnv.set (sr, 50, 100);
                    gateEnv.set (sr, 5, 125);
                    gateRev.set (sr, 15, 250);
                    maxEnv.set (sr, 0.1, 0.1);
                    maxRev.set (sr, 10, 150);
                }

                void reset()
                {
                    for (auto* e : { &bHp, &bLp, &inLs, &inHs, &inHp, &inLp, &e1, &e2, &e3, &e4, &e5, &outLs, &outHs, &b1, &b2, &b3, &tenK, &depthEq, &presEq })
                        e->reset();
                    up.reset(); dn.reset();
                    gateEnv.reset(); gateRev.reset(); ampEnv.reset(); maxEnv.reset(); maxRev.reset();
                    dc.reset();
                }

                double amp (double s, const AmpSim& o, double env)
                {
                    if (o.booster)
                    {
                        s = bHp.tick (s);
                        s = bLp.tick (s);
                        s = soft (s * o.boosterDrive);
                    }

                    s = inLs.tick (s); s = inHs.tick (s); s = inHp.tick (s); s = inLp.tick (s);

                    const bool pos = s >= 0;
                    s *= o.dist[0]; s = pos ? hard (s) : soft (s); s *= 0.5;
                    s = e1.tick (s); s = e2.tick (s);
                    s *= o.dist[1]; s = pos ? std::atan (s) : hard (s); s *= 0.4;
                    s = e3.tick (s);
                    s *= o.dist[2]; s = hard (s); s *= 0.5;
                    s = e4.tick (s);
                    s *= o.dist[3]; s = pos ? std::atan (s) : soft (s); s *= 0.25;
                    s = e5.tick (s);
                    s *= o.dist[4]; s = pos ? soft (s) : hard (s); s *= 0.25;
                    s = outLs.tick (s); s = outHs.tick (s); s *= 0.5;
                    s = b1.tick (s); s = b2.tick (s); s = b3.tick (s);
                    s = tenK.tick (s);

                    const double d = o.depth * env * depthEq.tick (s) * 0.5;
                    const double p = o.presence * env * presEq.tick (s) * 0.5;
                    s += d + p;
                    return s * 0.6;
                }

                double run (double x, AmpSim& o)
                {
                    if (o.gateActive && (o.gatePos == 0 || o.gatePos == 2))
                    {
                        gateRev.follow (gateEnv.follow (x) < o.gateThreshold ? 0.0 : 1.0);
                        if (o.gatePos == 0) x *= gateRev.env;
                    }

                    const double env = decelerate (ampEnv.follow (x));
                    const int ratio = o.ratio;

                    if (ratio > 1)
                    {
                        x = up.tick (x * ratio);
                        for (int c = 0; c < ratio - 1; ++c) buffer[c] = up.tick (0.0);
                    }

                    x = amp (x, o, env);

                    if (ratio > 1)
                    {
                        for (int c = 0; c < ratio - 1; ++c) buffer[c] = amp (buffer[c], o, env);
                        x = dn.tick (x);
                        for (int c = 0; c < ratio - 1; ++c) buffer[c] = dn.tick (buffer[c]);
                    }

                    if (o.maximize < 0.0)
                    {
                        x *= o.maxGain;
                        maxRev.follow (maxEnv.follow (x) > o.maxThreshold ? o.maxThreshold : 1.0);
                        x *= maxRev.env;
                    }

                    x = dc.tick (x);
                    x = hard (x);

                    if (o.gateActive && o.gatePos == 1)
                    {
                        gateRev.follow (gateEnv.follow (x) < o.gateThreshold ? 0.0 : 1.0);
                        x *= gateRev.env;
                    }

                    if (o.gateActive && o.gatePos == 2)
                        x *= gateRev.env;

                    return x * o.outGain;
                }
            };

            void update()
            {
                const int newRatio = 1 << juce::jlimit (0, 3, (int) knob[11]);
                const float gain = knob[3], low = knob[4] * 0.01f, mid = knob[5] * 0.01f, high = knob[6] * 0.01f;

                gateActive = knob[0] > -96.0f;
                gatePos = (int) knob[1];
                gateThreshold = dbToGainD (knob[0]);
                booster = knob[2] > 0.5f;
                depth = knob[7] * 0.01;
                presence = knob[8] * 0.01;
                maximize = knob[9];
                maxThreshold = dbToGainD (maximize);
                maxGain = dbToGainD (-maximize);
                outGain = dbToGainD (knob[10]);

                const double inputGain = dbToGainD (decelerate (gain * 0.01) * 16.0);
                const double k[] { 1.5, 0.85, 1.25, 1.25, 0.75 };
                for (int i = 0; i < 5; ++i) dist[i] = inputGain * k[i];

                if (newRatio != ratio)
                {
                    ratio = newRatio;
                    osRate = rate * ratio;

                    for (auto& c : ch)
                    {
                        c.up.set (22050.0, osRate, 4);   // his "Normal" filtering
                        c.dn.set (22000.0, osRate, 8);
                    }

                    lastGain = -1.0f;   // the filters again, at the new rate
                }

                if (gain == lastGain && low == lastLow && mid == lastMid && high == lastHigh)
                    return;

                lastGain = gain; lastLow = low; lastMid = mid; lastHigh = high;
                const double sr = osRate, f = gain * 0.01;

                for (auto& c : ch)
                {
                    c.bHp.hp (sr, 250, 0.25);
                    c.bLp.lp (sr, 3500, 0.15);
                    const double tiltGain = 1.0 + 3.0 * f;
                    c.inLs.ls (sr, 1000, 0.5, -tiltGain);
                    c.inHs.hs (sr, 1000, 0.5, +tiltGain);
                    c.inHp.hp (sr, 50.0 + 200.0 * f, 0.5);
                    c.inLp.lp (sr, 14000.0 - 10000.0 * f, 0.5);
                    c.e1.pk (sr, 150, 2.5, -3.0 * f);
                    c.e2.pk (sr, 600, 1.5, -6.0 * f);
                    c.e3.pk (sr, 750, 1.5, +3.0 * f);
                    c.e4.pk (sr, 2000, 2.0, -3.0 * f);
                    c.e5.pk (sr, 120, 2.0, -2.0 * f);
                    const double outTilt = 3.0 + 3.0 * f;
                    c.outLs.ls (sr, 10000, 0.25, +outTilt);
                    c.outHs.hs (sr, 10000, 0.25, -outTilt);
                    c.b1.ls (sr, 350, 0.25 + std::abs (low), low * 4.5);
                    c.b2.pk (sr, 750, 0.5 + std::abs (mid), mid * 4.5);
                    c.b3.hs (sr, 2500, 0.2 + std::abs (high) * 0.5, high * 6.0);
                    c.tenK.pk (sr, 9000, 2, 22.0);
                    c.depthEq.pk (sr, 100, 1.75, 12.0);
                    c.presEq.pk (sr, 6000, 0.25, 12.0);
                }
            }

            Channel ch[2];
            double rate = 48000.0, osRate = 96000.0;
            int ratio = -1;
            float lastGain = -1, lastLow = -9, lastMid = -9, lastHigh = -9;

            bool gateActive = false, booster = false;
            int gatePos = 0;
            double gateThreshold = 0, depth = 0.5, presence = 0.5, maximize = -3, maxThreshold = 1, maxGain = 1, outGain = 1;
            double dist[5] {};
            const double boosterDrive = dbToGainD (24.0);

            friend struct Channel;
        };

        //======================================================================
        //  Cabinet Sim: his five impulse responses, convolved.
        //======================================================================
        class CabSim final : public Block
        {
        public:
            CabSim() : conv (juce::dsp::Convolution::NonUniform { 256 }) {}

            void prepare (double sr, int maxBlock) override
            {
                conv.prepare ({ sr, (juce::uint32) juce::jmax (1, maxBlock), 2 });
                loaded = -1;
                load ((int) knob[0]);
            }

            void reset() override   { conv.reset(); }

            void process (float* l, float* r, int n) override
            {
                float* chans[] { l, r };
                juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
                conv.process (juce::dsp::ProcessContextReplacing<float> (block));

                const float g = dbToGain (knob[1]) * makeup;
                juce::FloatVectorOperations::multiply (l, g, n);
                juce::FloatVectorOperations::multiply (r, g, n);
            }

            void messageTick() override   { load ((int) uiKnob[0]); }

        private:
            void load (int index)
            {
                index = juce::jlimit (0, 4, index);

                if (index == loaded)
                    return;

                loaded = index;
                const auto& ir = chokeholdcabs::irs[index];

                // His conditioning: peak brought to -18 dB.
                float peak = 0.0f;
                for (int i = 0; i < ir.length; ++i) peak = juce::jmax (peak, std::abs (ir.data[i]));

                juce::AudioBuffer<float> buffer (1, ir.length);
                const float g = peak > 0.0f ? 0.1258925412f / peak : 1.0f;
                for (int i = 0; i < ir.length; ++i) buffer.setSample (0, i, ir.data[i] * g);

                conv.loadImpulseResponse (std::move (buffer), ir.sampleRate, juce::dsp::Convolution::Stereo::no,
                                          juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
            }

            juce::dsp::Convolution conv;
            int loaded = -1;
            const float makeup = 2.0f;   // his output stage (x0.5) and the level of an amp's cabinet in the chain
        };

        //======================================================================
        //  Chug Thug: the lows driven into a compressor, the highs left alone.
        //======================================================================
        class ChugThug final : public Block
        {
        public:
            void prepare (double sr, int) override  { rate = sr; lastHz = -1; for (auto& c : comp) { c.attRel.set (sr, 5.0, 250.0); c.calc.set (-6.0, 4.0, 4.5, -144.0); c.autoMakeup = false; } reset(); }
            void reset() override                   { for (auto& f : lo) f.reset(); for (auto& f : hi) f.reset(); for (auto& c : comp) c.reset(); }

            void process (float* l, float* r, int n) override
            {
                if (knob[1] != lastHz)
                {
                    lastHz = knob[1];
                    for (int c = 0; c < 2; ++c)
                        for (int s = 0; s < 2; ++s)
                        {
                            lo[c * 2 + s].set (kit::Biquad::Type::lowpass, rate, lastHz, 0.70710678);
                            hi[c * 2 + s].set (kit::Biquad::Type::highpass, rate, lastHz, 0.70710678);
                        }
                }

                const double gain = dbToGainD (knob[0]), trim = dbToGainD (knob[4]);
                const double dbDrive = knob[2] * 0.24, drive = dbToGainD (dbDrive), pad = dbToGainD (-dbDrive);
                const double balance = knob[3] * 0.01;
                const double balHi = balance >= 0.5 ? 1.0 : balance * 2.0;
                const double balLo = balance <= 0.5 ? 1.0 : 1.0 - (balance * 2.0 - 1.0);

                for (int i = 0; i < n; ++i)
                {
                    const double xl = l[i] * gain, xr = r[i] * gain;
                    const double hiL = hi[1].run (hi[0].run ((float) xl)), hiR = hi[3].run (hi[2].run ((float) xr));
                    double loL = lo[1].run (lo[0].run ((float) xl)) * drive, loR = lo[3].run (lo[2].run ((float) xr)) * drive;

                    // Stereo linked: both sides by the one that's squeezed more.
                    const double grL = comp[0].tick (std::abs (loL)), grR = comp[1].tick (std::abs (loR));
                    const double gr = juce::jmin (grL, grR);
                    loL *= gr * pad;
                    loR *= gr * pad;

                    l[i] = (float) ((loL * balLo + hiL * balHi) * trim);
                    r[i] = (float) ((loR * balLo + hiR * balHi) * trim);
                }
            }

        private:
            kit::Biquad lo[4], hi[4];   // Linkwitz-Riley: two Butterworths each
            Comp comp[2];
            double rate = 48000.0;
            float lastHz = -1;
        };

        //======================================================================
        //  The clippers: soft (his fast tanh), hard, sine; oversampled as his are.
        //======================================================================
        class Clipper final : public Block
        {
        public:
            void prepare (double sr, int) override  { rate = sr; ratio = -1; reset(); }
            void reset() override                   { for (auto& b : up) b.reset(); for (auto& b : dn) b.reset(); for (auto& d : dc) d.reset(); }

            void process (float* l, float* r, int n) override
            {
                const int newRatio = 1 << juce::jlimit (0, 3, (int) knob[3]);

                if (newRatio != ratio)
                {
                    ratio = newRatio;
                    for (auto& b : up) b.set (22050.0, rate * ratio, 4);
                    for (auto& b : dn) b.set (22000.0, rate * ratio, 8);
                }

                type = (int) knob[0];
                gain = dbToGainD (knob[1]);
                ceiling = dbToGainD (knob[2]);
                const bool blocker = knob[4] > 0.5f;

                float* chans[] { l, r };

                for (int c = 0; c < 2; ++c)
                {
                    float* x = chans[c];

                    for (int i = 0; i < n; ++i)
                    {
                        double s = x[i];

                        if (ratio > 1)
                        {
                            s = clip (up[c].tick (s * ratio));
                            for (int k = 0; k < ratio - 1; ++k) buffer[k] = clip (up[c].tick (0.0));
                            s = dn[c].tick (s);
                            for (int k = 0; k < ratio - 1; ++k) dn[c].tick (buffer[k]);
                        }
                        else
                        {
                            s = clip (s);
                        }

                        x[i] = (float) (blocker ? dc[c].tick (s) : s);
                    }
                }
            }

        private:
            double clip (double s) const noexcept
            {
                switch (type)
                {
                    case 1:  return juce::jlimit (-ceiling, ceiling, s * gain);
                    case 2:
                    {
                        const double v = s * gain / ceiling;
                        return (std::abs (v) < dPi * 0.5 ? std::sin (v) : (v < 0 ? -1.0 : 1.0)) * ceiling;
                    }
                    default:
                    {
                        const double c = juce::jmin (ceiling, 1.0);
                        return fastTanh (s * gain / c) * c;
                    }
                }
            }

            Bw up[2], dn[2];
            DcBlock dc[2];
            double buffer[8] {};
            double rate = 48000.0, gain = 1.0, ceiling = 1.0;
            int ratio = -1, type = 0;
        };

        //======================================================================
        class Foldback final : public Block
        {
        public:
            void prepare (double, int) override   { reset(); }
            void reset() override                 { for (auto& d : dc) d.reset(); }

            void process (float* l, float* r, int n) override
            {
                const double c = dbToGainD (knob[0]), g = dbToGainD (knob[1]);
                const bool blocker = knob[2] > 0.5f;
                float* chans[] { l, r };

                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                    {
                        double v = chans[ch][i] * g;
                        const double sign = v < 0 ? -1.0 : 1.0;
                        v = std::fmod (std::abs (v), 2.0 * c);   // his folding loop, in one step
                        if (v > c) v = 2.0 * c - v;
                        v *= sign;
                        chans[ch][i] = (float) (blocker ? dc[ch].tick (v) : v);
                    }
            }

        private:
            DcBlock dc[2];
        };

        //======================================================================
        //  Signal Crusher
        //======================================================================
        class SignalCrusher final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                for (auto& e : blank) e.set (sr, 10, 300);
                lastRatio = -1; lastHz = -1; lastTilt = -99;
                reset();
            }

            void reset() override
            {
                for (auto& s : st) s = {};
                for (auto& b : lpDown) b.reset();
                for (auto& b : lpUp) b.reset();
                for (auto& d : dc) d.reset();
                for (auto& e : blank) e.reset();
                for (auto& t : tiltLs) t = {};
                for (auto& t : tiltHs) t = {};
            }

            void process (float* l, float* r, int n) override
            {
                const int down = (int) knob[0], dnFilt = (int) knob[1], up = (int) knob[2], upFilt = (int) knob[3];
                const int ratio = juce::jlimit (1, 32, (int) std::lround (knob[4]));
                const double bits = knob[5], dither = knob[6] * 0.01;
                const double inGain = dbToGainD (knob[9]), outTrim = dbToGainD (knob[10]);
                const double tilt = knob[8] * 0.01;

                if (ratio != lastRatio)
                {
                    lastRatio = ratio;
                    const double cut = juce::jmin (rate / ratio / 2.0, 20000.0);
                    for (auto& b : lpDown) b.set (cut, rate, 8);
                    for (auto& b : lpUp) b.set (cut, rate, 8);
                    for (auto& s : st) s = {};
                }

                if (knob[7] != lastHz || tilt != lastTilt)
                {
                    lastHz = knob[7]; lastTilt = tilt;
                    for (int c = 0; c < 2; ++c)
                    {
                        tiltLs[c].shelf (rate, lastHz, 0.1, -tilt * 12.0, true);
                        tiltHs[c].shelf (rate, lastHz, 0.1, tilt * 12.0, false);
                    }
                }

                const double bitsLevel = 1.0 / std::pow (2.0, bits);
                float* chans[] { l, r };

                for (int c = 0; c < 2; ++c)
                {
                    auto& s = st[c];

                    for (int i = 0; i < n; ++i)
                    {
                        double x = chans[c][i] * inGain;
                        blank[c].follow (x != 0.0 ? 1.0 : 0.0);

                        if (down > 0)
                        {
                            if (dnFilt == 1) x = lpDown[c].tick (x);

                            s.counterDS += 1;

                            if (s.counterDS > ratio || s.counterDS == 1)
                            {
                                s.lastDS = s.thisDS;
                                s.thisDS = x;
                                s.counterDS = 1;
                            }
                            else
                            {
                                const double mu = (double) s.counterDS / ratio;
                                if (down == 1) x = s.thisDS;
                                if (down == 2) x = 0.0;
                                if (down == 3) x = s.lastDS * (1.0 - mu) + s.thisDS * mu;
                                if (down == 4) { const double m2 = (1.0 - std::cos (mu * dPi)) * 0.5; x = s.lastDS * (1.0 - m2) + s.thisDS * m2; }
                            }

                            if (down == 2) x *= 1.0 + 1.0 / ratio;
                            if (dnFilt == 2) x = lpDown[c].tick (x);
                        }

                        if (up > 0)
                        {
                            if (upFilt == 1) x = lpUp[c].tick (x);

                            s.counterUS += 1;

                            if (s.counterUS > ratio || s.counterUS == 1)
                            {
                                s.lastUS = s.thisUS;
                                s.thisUS = x;
                                s.counterUS = 1;
                            }
                            else
                            {
                                const double mu = (double) s.counterUS / ratio;
                                if (up == 1) x = s.lastUS * (1.0 - mu) + s.thisUS * mu;
                                if (up == 2) { const double m2 = (1.0 - std::cos (mu * dPi)) * 0.5; x = s.lastUS * (1.0 - m2) + s.thisUS * m2; }
                            }

                            if (upFilt == 2) x = lpUp[c].tick (x);
                        }

                        if (bits < 24.0)
                        {
                            if (std::abs (x) < bitsLevel) x = 0.0;
                            x += blank[c].env * dither * bitsLevel * (rnd.nextDouble() * 2.0 - 1.0);
                        }

                        x = dc[c].tick (x);
                        x = juce::jlimit (-1.0, 1.0, x);

                        if (tilt != 0.0)
                        {
                            x = tiltLs[c].tick (x);
                            x = tiltHs[c].tick (x);
                        }

                        chans[c][i] = (float) (x * outTrim);
                    }
                }
            }

        private:
            /** His Orfanidis shelves. */
            struct Shelf
            {
                double a1 = 0, a2 = 0, b0 = 1, b1 = 0, b2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;

                void shelf (double sr, double hz, double q, double db, bool low)
                {
                    const double G0 = std::pow (10.0, db / 20.0), wc = std::tan (dPi * hz / sr), sg = std::sqrt (G0);
                    double a0;

                    if (low)
                    {
                        b0 = G0 * wc * wc + sg * wc / q + 1; b1 = 2 * (G0 * wc * wc - 1); b2 = G0 * wc * wc - sg * wc / q + 1;
                        a0 = wc * wc + sg * wc / q + 1; a1 = 2 * (wc * wc - 1); a2 = wc * wc - sg * wc / q + 1;
                    }
                    else
                    {
                        b0 = G0 + sg * wc / q + wc * wc; b1 = 2 * (wc * wc - G0); b2 = G0 - sg * wc / q + wc * wc;
                        a0 = 1 + sg * wc / q + wc * wc; a1 = 2 * (wc * wc - 1); a2 = 1 - sg * wc / q + wc * wc;
                    }

                    a1 /= a0; a2 /= a0; b0 /= a0; b1 /= a0; b2 /= a0;
                }

                double tick (double x) noexcept
                {
                    const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                    x2 = x1; x1 = x; y2 = y1; y1 = y;
                    return y;
                }
            };

            struct State { int counterDS = 0, counterUS = 0; double lastDS = 0, thisDS = 0, lastUS = 0, thisUS = 0; };

            State st[2];
            Bw lpDown[2], lpUp[2];
            DcBlock dc[2];
            Env blank[2];
            Shelf tiltLs[2], tiltHs[2];
            juce::Random rnd;
            double rate = 48000.0, lastTilt = -99;
            float lastHz = -1;
            int lastRatio = -1;
        };

        //======================================================================
        //  Telephone
        //======================================================================
        class Telephone final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                for (auto& h : keyHp) h.hp (sr, 750.0 * 0.5, 1.0);   // his eqHP takes half the frequency
                last.fill (-999.0f);
                reset();
            }

            void reset() override
            {
                for (auto& h : keyHp) h.reset();
                for (auto& c : comp) c.reset();
                for (auto& f : bpf) f.reset();
                for (auto& f : hpOut) f.reset();
                for (auto& f : lpOut) f.reset();
                for (auto& d : dc) d.reset();
            }

            void process (float* l, float* r, int n) override
            {
                bool changed = false;
                for (int k = 0; k < 9; ++k)
                    if (knob[(size_t) k] != last[(size_t) k]) { changed = true; last[(size_t) k] = knob[(size_t) k]; }

                if (changed)
                {
                    const double threshold = knob[0];
                    const double tf = std::abs (threshold) / 60.0;
                    const double atk = 10.0 * (1.0 - tf) + 1.0 * tf, rel = 10.0 * (1.0 - tf) + 100.0 * tf;

                    for (auto& c : comp)
                    {
                        c.attRel.set (rate, atk, rel);
                        c.calc.set (threshold, 30.0, 0.0, threshold);
                        c.autoMakeup = true;
                    }

                    compensation = dbToGainD (juce::jmax (0.0, 24.0 - (60.0 + threshold)));
                    const double hz = knob[4], q = knob[5];

                    for (int c = 0; c < 2; ++c)
                    {
                        bpf[c].bp (rate, hz, q);
                        hpOut[c].hp (rate, hz * 0.75 * 0.5, q * 0.5);
                        lpOut[c].lp (rate, hz * 1.50 * 0.5, q * 0.5);
                    }

                    noiseGain = knob[1] > -144.0f ? dbToGainD (knob[1]) : 0.0;
                    crackleProb = 0.000001 * knob[3];
                    crackleGain = knob[2] > -144.0f ? dbToGainD (knob[2]) : 0.0;
                    drive = dbToGainD (knob[6]);
                    attn = dbToGainD (-knob[6]);
                    trim = dbToGainD (knob[7]);
                }

                const bool clip = knob[8] > 0.5f;
                float* chans[] { l, r };

                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        double x = chans[c][i];
                        const double key = std::abs (keyHp[c].tick (x));
                        x *= comp[c].tick (key) * compensation;

                        if (x != 0.0)
                        {
                            if (noiseGain > 0.0) x += (rnd.nextDouble() * 2.0 - 1.0) * noiseGain;
                            if (crackleGain > 0.0 && rnd.nextDouble() < crackleProb) x += (x < 0 ? -1.0 : 1.0) * crackleGain;
                        }

                        x = bpf[c].tick (x);
                        x = hpOut[c].tick (x);
                        x = lpOut[c].tick (x);
                        x = fastTanh (x * drive / 0.975) * 0.975;
                        x = dc[c].tick (x);
                        x *= attn * trim;
                        if (clip) x = juce::jlimit (-0.998, 0.998, x);
                        chans[c][i] = (float) x;
                    }
            }

        private:
            Eq keyHp[2], bpf[2], hpOut[2], lpOut[2];
            Comp comp[2];
            DcBlock dc[2];
            juce::Random rnd;
            std::array<float, 9> last {};
            double rate = 48000.0, compensation = 1, noiseGain = 0, crackleProb = 0, crackleGain = 0, drive = 1, attn = 1, trim = 1;
        };

        //======================================================================
        //  EQ 560: ten bands, 2x oversampled, each narrower the more it moves.
        //======================================================================
        class Eq560 final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                const double os = sr * 2.0, nyquist = sr * 0.49887;

                for (int c = 0; c < 2; ++c)
                {
                    up[c].set (nyquist, os, 2);
                    dn[c].set (nyquist, os, 2);
                    lsBump[c].ls (sr, 120, 0.25, 0.5);
                    hpBump[c].hp (sr, 20, 1.25);
                    pkBump[c].pk (sr, 10000, 0.75, 0.5);
                    env[c].set (sr, 0.0, 50.0);
                }

                last.fill (-99.0f);
                reset();
            }

            void reset() override
            {
                for (int c = 0; c < 2; ++c)
                {
                    up[c].reset(); dn[c].reset(); lsBump[c].reset(); hpBump[c].reset(); pkBump[c].reset(); env[c].reset();
                    for (auto& b : bands[c]) b.reset();
                }
            }

            void process (float* l, float* r, int n) override
            {
                static const double freqs[10] { 31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
                const double os = rate * 2.0;

                for (int b = 0; b < 10; ++b)
                {
                    if (knob[(size_t) b] == last[(size_t) b])
                        continue;

                    last[(size_t) b] = knob[(size_t) b];
                    const double db = knob[(size_t) b];
                    const double mu = (std::abs (db) / 12.0) * (std::abs (db) / 12.0);
                    const double width = 0.126984 * (1.0 - mu) + 5.763566 * mu;

                    for (int c = 0; c < 2; ++c)
                        bands[c][b].pk (os, freqs[b], width, db);
                }

                const double trim = dbToGainD (knob[10]);
                const double noise1 = dbToGainD (-95.0);
                float* chans[] { l, r };

                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        double x = chans[c][i];

                        if (env[c].tickDb (x) <= 0.0)
                            continue;   // silence stays silent (his: no noise into nothing)

                        x += rnd.nextDouble() * 2.0 * noise1 - noise1;
                        x *= trim;
                        x = lsBump[c].tick (x);
                        x = hpBump[c].tick (x);
                        x = pkBump[c].tick (x);

                        x = up[c].tick (x * 2.0);
                        double odd = up[c].tick (0.0);

                        for (int b = 0; b < 10; ++b)
                            if (last[(size_t) b] != 0.0f)
                            {
                                x = bands[c][b].tick (x);
                                odd = bands[c][b].tick (odd);
                            }

                        x = dn[c].tick (x);
                        dn[c].tick (odd);
                        chans[c][i] = (float) x;
                    }
            }

        private:
            struct Hold
            {
                double env = 0, ca = 0, cr = 0;
                void set (double sr, double a, double rl)   { ca = a > 0 ? std::exp (-1000.0 / (a * sr)) : 0.0; cr = std::exp (-1000.0 / (rl * sr)); }
                double tickDb (double x) noexcept
                {
                    x = std::abs (x);
                    env = x + ((x > env) ? ca : cr) * (env - x);
                    if (env < 0.0000000630957) env = 0.0;
                    return env;
                }
                void reset() noexcept   { env = 0; }
            };

            Bw up[2], dn[2];
            Eq lsBump[2], hpBump[2], pkBump[2];
            Eq bands[2][10];
            Hold env[2];
            juce::Random rnd;
            std::array<float, 10> last {};
            double rate = 48000.0;
        };

        //======================================================================
        //  Filthy Delay
        //======================================================================
        class FilthyDelay final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                for (auto& h : history) h.prepare ((int) (3.1 * sr) + 8);
                timeSmooth.prepare (sr, 60.0f, -1.0f);
                lastHp = lastLp = -1; lastDirty = -1;
                reset();
            }

            void reset() override
            {
                for (auto& h : history) h.reset();
                for (auto& f : hp) f.reset();
                for (auto& f : lp) f.reset();
                for (auto& d : ds) d = {};
                timeSmooth.snap (-1.0f);
            }

            void process (float* l, float* r, int n) override
            {
                const int routing = (int) knob[0];
                const double ms = juce::jlimit (1.0, 3000.0, kit::syncedMs (knob[1], (int) knob[2], bpm()));
                const double feedback = knob[3] * 0.01;
                const bool dirty = knob[6] > 0.5f;
                const double drive = dbToGainD (knob[7]);
                const double dryMix = knob[8] * 0.01, wetMix = knob[9] * 0.01;

                if (knob[4] != lastHp || knob[5] != lastLp || (int) dirty != lastDirty)
                {
                    lastHp = knob[4]; lastLp = knob[5]; lastDirty = dirty ? 1 : 0;
                    const double q = 0.75 + (dirty ? 0.5 : 0.0);
                    for (int c = 0; c < 2; ++c)
                    {
                        hp[c].hp (rate, lastHp * 0.5, q);   // his eqHP/eqLP take half the frequency
                        lp[c].lp (rate, lastLp * 0.5, q);
                    }
                }

                const float target = (float) (ms * 0.001 * rate);
                if (timeSmooth.value < 0.0f) timeSmooth.snap (target);

                for (int i = 0; i < n; ++i)
                {
                    const float d = timeSmooth.next (target);
                    const double readL = feedback * history[0].read (d);
                    const double readR = feedback * history[1].read (d);
                    double fbL = readL, fbR = readR;

                    if (dirty)
                    {
                        fbL = ds[0].tick (fbL, rate);
                        fbR = ds[1].tick (fbR, rate);
                    }

                    if (lastHp > 20.0f)    { fbL = hp[0].tick (fbL); fbR = hp[1].tick (fbR); }
                    if (lastLp < 20000.0f) { fbL = lp[0].tick (fbL); fbR = lp[1].tick (fbR); }

                    if (dirty)
                    {
                        fbL = fastTanh (fbL * drive) / drive;
                        fbR = fastTanh (fbR * drive) / drive;
                    }

                    const double dryL = l[i], dryR = r[i];
                    double outL = readL, outR = readR;

                    switch (routing)
                    {
                        case 1:  history[0].push ((float) (dryR + fbL)); history[1].push ((float) (dryL + fbR)); break;
                        case 2:  history[0].push ((float) (dryL + fbR)); history[1].push ((float) (dryR + fbL)); outL = readR; outR = readL; break;
                        case 3:
                        {
                            const double m = (dryL + fbL) * 0.5 + (dryR + fbR) * 0.5;
                            history[0].push ((float) m); history[1].push ((float) m);
                            break;
                        }
                        default: history[0].push ((float) (dryL + fbL)); history[1].push ((float) (dryR + fbR)); break;
                    }

                    l[i] = (float) (outL * wetMix + dryMix * dryL);
                    r[i] = (float) (outR * wetMix + dryMix * dryR);
                }
            }

        private:
            /** His feedback downsampler (8x, "filthy"). */
            struct Down
            {
                Bw pre, post;
                int counter = 0;
                bool ready = false;

                double tick (double x, double sr)
                {
                    if (! ready)
                    {
                        const double hz = (sr / 8.0) * 0.45;
                        pre.set (hz, sr, 2);
                        post.set (hz, sr, 4);
                        ready = true;
                    }

                    x = pre.tick (x * 8.0);
                    counter += 1;
                    if (counter > 8) counter = 1;
                    else x = 0.0;
                    return post.tick (x);
                }
            };

            kit::Delay history[2];
            Eq hp[2], lp[2];
            Down ds[2];
            Smooth timeSmooth;
            double rate = 48000.0;
            float lastHp = -1, lastLp = -1;
            int lastDirty = -1;
        };

        //======================================================================
        //  Ring Mod
        //======================================================================
        class RingMod final : public Block
        {
        public:
            void prepare (double sr, int) override   { rate = sr; reset(); }
            void reset() override                    { phase = 0; z1 = 0; fdb[0] = fdb[1] = 0; dc[0].reset(); dc[1].reset(); }

            void process (float* l, float* r, int n) override
            {
                const int osc = (int) knob[0];
                const double ctrFreq = knob[1] * 1000.0 + knob[2];
                const double srcFreq = ctrFreq / rate;
                const double bias = knob[3] * 0.01, variation = knob[4] * 0.005, feedback = knob[5] * 0.01;
                const double drive = knob[6] * 0.01, mixWet = knob[7] * 0.01, mixDry = 1.0 - mixWet, trim = dbToGainD (knob[8]);
                const double b1 = std::exp (-2.0 * dPi * ((10.0 + 90.0 * variation) / rate)), a0 = 1.0 - b1;
                const double noise = dbToGainD (-144.0 + knob[4] * 0.01 * 35.5);
                const double maxDrive = dbToGainD (6.0), mixClean = std::sin (dPi * 0.5 + drive * dPi * 0.5);
                const bool unstable = knob[4] > 0.0f;

                if (mixWet <= 0.0)
                {
                    juce::FloatVectorOperations::multiply (l, (float) trim, n);
                    juce::FloatVectorOperations::multiply (r, (float) trim, n);
                    return;
                }

                float* chans[] { l, r };

                for (int i = 0; i < n; ++i)
                {
                    double instability = 0.0;

                    if (unstable)
                    {
                        const double randomise = ((rnd.nextDouble() * 2.0 - 1.0) * ctrFreq / rate) * variation;
                        z1 = randomise * a0 + z1 * b1;
                        instability = z1;
                    }

                    const double f = srcFreq + instability;
                    double carrier = 0.0;

                    if (osc == 0)
                    {
                        phase += 2.0 * dPi * f;
                        if (phase >= 2.0 * dPi) phase -= 2.0 * dPi;
                        if (phase < 0.0) phase += 2.0 * dPi;
                        carrier = std::sin (phase);
                    }
                    else if (osc < 7)
                    {
                        phase += (osc == 3 ? -f : f);
                        if (phase > 1.0) phase -= 1.0;
                        if (phase < 0.0) phase += 1.0;

                        if (osc == 1)      carrier = phase < 0.5 ? 4.0 * phase - 1.0 : 1.0 - 4.0 * (phase - 0.5);
                        else if (osc <= 3) carrier = phase * 2.0 - 1.0;
                        else
                        {
                            const double w = osc == 4 ? 0.25 : (osc == 5 ? 0.5 : 0.75);
                            carrier = phase < w ? 1.0 : -1.0;
                        }
                    }

                    if (osc < 7)
                        carrier += bias;

                    for (int c = 0; c < 2; ++c)
                    {
                        const double dry = chans[c][i];
                        double car = osc == 7 ? dry + bias + dry + instability * 10.0 : carrier;
                        double wet = dry + fdb[c] * feedback;

                        if (drive > 0.0)
                            wet = wet * mixClean + fastTanh (wet * drive * maxDrive) * 0.70710678 * drive;

                        wet *= car;
                        fdb[c] = (wet + (unstable ? (rnd.nextDouble() * 2.0 - 1.0) * noise : 0.0)) * 0.70710678;
                        wet = dry * mixDry + wet * mixWet;
                        chans[c][i] = (float) (dc[c].tick (wet) * trim);
                    }
                }
            }

        private:
            double rate = 48000.0, phase = 0, z1 = 0, fdb[2] {};
            DcBlock dc[2];
            juce::Random rnd;
        };

        //======================================================================
        //  Gate / Expander
        //======================================================================
        class GateExpander final : public Block
        {
        public:
            void prepare (double sr, int) override  { rate = sr; last.fill (-999.0f); reset(); }
            void reset() override                   { for (auto& g : gate) g.env = 0; for (auto& f : sc) f.reset(); }

            void process (float* l, float* r, int n) override
            {
                bool changed = false;
                for (int k = 0; k < 8; ++k)
                    if (knob[(size_t) k] != last[(size_t) k]) { changed = true; last[(size_t) k] = knob[(size_t) k]; }

                if (changed)
                {
                    const bool expander = knob[0] > 0.5f;

                    for (auto& g : gate)
                    {
                        g.att = std::exp (-1000.0 / (knob[4] * rate));
                        g.rel = std::exp (-1000.0 / (knob[5] * rate));
                        g.hysteresis = dbToGainD (knob[3]);
                        g.threshold = dbToGainD (knob[1]);
                        g.range = expander ? dbToGainD (knob[2]) : 0.0;
                    }

                    for (auto& f : sc)
                        f.hp (rate, knob[7] * 0.5, 1.5);   // his eqHP takes half the frequency
                }

                const double lnk = knob[6] * 0.01, spl = 1.0 - lnk;

                for (int i = 0; i < n; ++i)
                {
                    double kl = sc[0].tick (l[i]), kr = sc[1].tick (r[i]);

                    if (lnk > 0.0)
                    {
                        const double linked = juce::jmax (kl, kr) * lnk;
                        kl = kl * spl + linked;
                        kr = kr * spl + linked;
                    }

                    l[i] = (float) (l[i] * gate[0].tick (kl));
                    r[i] = (float) (r[i] * gate[1].tick (kr));
                }
            }

            float meter() const override    { return (float) (1.0 - juce::jmin (gate[0].env, 1.0)); }

        private:
            struct Gate
            {
                double env = 0, att = 0, rel = 0, hysteresis = 1, threshold = 0, range = 0;

                double tick (double sample) noexcept
                {
                    const double target = std::abs (sample) < threshold * hysteresis ? range : 1.0;
                    env = ((target > env) ? att : rel) * (env - target) + target;
                    return juce::jmax (env, range);
                }
            };

            Gate gate[2];
            Eq sc[2];
            std::array<float, 8> last {};
            double rate = 48000.0;
        };

        //======================================================================
        //  Track Comp
        //======================================================================
        class TrackComp final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                for (auto& s : sat) s.coeff = std::exp (-1000.0 / (50.0 * sr));
                last.fill (-999.0f);
                reset();
            }

            void reset() override
            {
                for (auto& c : comp) c.reset();
                for (auto& f : sc) f.reset();
                for (auto& s : sat) s.env = 0;
                for (auto& d : dc) d.reset();
            }

            void process (float* l, float* r, int n) override
            {
                bool changed = false;
                for (int k = 0; k < 12; ++k)
                    if (knob[(size_t) k] != last[(size_t) k]) { changed = true; last[(size_t) k] = knob[(size_t) k]; }

                if (changed)
                {
                    const double range = knob[6] <= -40.0f ? -400.0 : knob[6];

                    for (auto& c : comp)
                    {
                        c.attRel.set (rate, knob[2], knob[3]);
                        c.calc.set (knob[0], knob[1], knob[4], range);
                        c.feedback = knob[5] * 0.01;
                        c.autoMakeup = true;
                    }

                    for (auto& f : sc)
                        f.hp (rate, knob[8] * 0.5, 1.5);   // his eqHP takes half the frequency
                }

                const double lnk = knob[7] * 0.01, spl = 1.0 - lnk;
                const double satMix = knob[9] * 0.01, satDry = 1.0 - satMix;
                const double out = dbToGainD (knob[10]);
                const double wet = knob[11] * 0.01, dryW = 1.0 - wet;

                for (int i = 0; i < n; ++i)
                {
                    const double dryL = l[i], dryR = r[i];
                    double kl = knob[8] > 20.0f ? sc[0].tick (dryL) : dryL;
                    double kr = knob[8] > 20.0f ? sc[1].tick (dryR) : dryR;
                    kl = std::abs (kl); kr = std::abs (kr);

                    if (lnk > 0.0)
                    {
                        const double linked = (kl + kr) * 0.5 * lnk;
                        kl = kl * spl + linked;
                        kr = kr * spl + linked;
                    }

                    double xl = dryL * comp[0].tick (kl);
                    double xr = dryR * comp[1].tick (kr);

                    if (satMix > 0.0)
                    {
                        const double sl = sat[0].tick (comp[0].gr), sr = sat[1].tick (comp[1].gr);
                        xl = satDry * xl + satMix * sineClip (xl * (1.0 + (1.0 - sl)));
                        xr = satDry * xr + satMix * sineClip (xr * (1.0 + (1.0 - sr)));
                        xl = dc[0].tick (xl);
                        xr = dc[1].tick (xr);
                    }

                    xl *= out; xr *= out;
                    l[i] = (float) (dryW * dryL + wet * xl);
                    r[i] = (float) (dryW * dryR + wet * xr);
                }

                grNow.store ((float) juce::jmin (comp[0].gr, comp[1].gr));
            }

            float meter() const override    { return juce::jlimit (0.0f, 1.0f, (float) -gainToDb (grNow.load()) / 24.0f); }

        private:
            static double sineClip (double s)
            {
                return std::abs (s) > dPi * 0.5 ? (s < 0 ? -1.0 : 1.0) : std::sin (s);
            }

            struct Sat
            {
                double env = 0, coeff = 0;
                double tick (double x) noexcept   { env = x + coeff * (env - x); return env; }
            };

            Comp comp[2];
            Eq sc[2];
            Sat sat[2];
            DcBlock dc[2];
            std::array<float, 12> last {};
            std::atomic<float> grNow { 1.0f };
            double rate = 48000.0;
        };
    }

    //==========================================================================
    void registry::addChokehold (std::vector<BlockInfo>& list)
    {
        using namespace kit;

        list.push_back (info ("ch_clipper", "Clipper", Family::drive, "gfx-distortion", chokeCredit,
            "Soft, hard or sine clipping, oversampled: from warmth to a wall.",
            { choice ("Type", { "Soft", "Hard", "Sine" }), knob ("Boost", 0.0f, 48.0f, 12.0f, " dB", 12.0f, 1),
              knob ("Ceiling", -48.0f, 0.0f, -6.0f, " dB", -12.0f, 1), choice ("Oversampling", { "Off", "2x", "4x", "8x" }, 2), toggle ("DC block", true) },
            maker<Clipper>()));

        list.push_back (info ("ch_foldback", "Foldback", Family::drive, "gfx-fuzz", chokeCredit,
            "Past the ceiling the wave folds back down: a gnarly, ringing fuzz.",
            { knob ("Ceiling", -48.0f, 0.0f, -12.0f, " dB", -12.0f, 1), knob ("Boost", 0.0f, 48.0f, 12.0f, " dB", 12.0f, 1), toggle ("DC block", true) },
            maker<Foldback>()));

        list.push_back (info ("ch_crusher", "Signal Crusher", Family::drive, "gfx-bitcrusher", chokeCredit,
            "Down-sampling and bit grit, with a tilt EQ to tame it.",
            { choice ("Downsampling", { "Off", "Repeat", "Drop", "Linear", "Cosine" }, 1), choice ("Down filter", { "Off", "Pre", "Post" }, 1),
              choice ("Rebuild", { "Off", "Linear", "Cosine" }), choice ("Rebuild filter", { "Off", "Pre", "Post" }),
              knob ("Factor", 1.0f, 32.0f, 8.0f, "x", 8.0f, 0), knob ("Bits", 0.0f, 24.0f, 10.0f, {}, 0.0f, 1), percent ("Dither", 100.0f),
              knob ("Tilt freq", 100.0f, 10000.0f, 1000.0f, " Hz", 1000.0f), knob ("Tilt", -100.0f, 100.0f, 0.0f),
              knob ("Input", -24.0f, 24.0f, 0.0f, " dB", 0.0f, 1), knob ("Output", -24.0f, 24.0f, 0.0f, " dB", 0.0f, 1) },
            maker<SignalCrusher>()));

        list.push_back (info ("ch_chug", "Chug Thug", Family::dynamics, "gfx-compressor", chokeCredit,
            "Tightens palm mutes: the lows driven into a compressor, the highs left alone.",
            { knob ("Input", -24.0f, 24.0f, 0.0f, " dB", 0.0f, 1), knob ("Crossover", 80.0f, 800.0f, 250.0f, " Hz", 250.0f), percent ("Amount", 50.0f),
              knob ("Balance", 0.0f, 100.0f, 50.0f, " %"), knob ("Output", -24.0f, 24.0f, 0.0f, " dB", 0.0f, 1) },
            maker<ChugThug>()));

        list.push_back (info ("ch_gate", "Gate / Expander", Family::dynamics, "gfx-expander", chokeCredit,
            "A gate that fades to silence, or an expander that fades to its range.",
            { choice ("Mode", { "Gate", "Expander" }), knob ("Threshold", -60.0f, 0.0f, -60.0f, " dB", 0.0f, 1), knob ("Range", -40.0f, 0.0f, -40.0f, " dB", 0.0f, 1),
              knob ("Hysteresis", -12.0f, 0.0f, -3.0f, " dB", 0.0f, 1), knob ("Attack", 1.0f, 50.0f, 5.0f, " ms", 8.0f),
              knob ("Release", 50.0f, 2500.0f, 300.0f, " ms", 400.0f), percent ("Link", 0.0f), knob ("SC filter", 20.0f, 350.0f, 70.0f, " Hz", 80.0f) },
            maker<GateExpander>()));

        list.push_back (info ("ch_trackcomp", "Track Comp", Family::dynamics, "gfx-compressor", chokeCredit,
            "A full channel compressor: knee, feedback, range and saturation.",
            { knob ("Threshold", -60.0f, 0.0f, -18.0f, " dB", 0.0f, 1), knob ("Ratio", 1.0f, 20.0f, 4.0f, ":1", 4.0f, 1),
              knob ("Attack", 0.01f, 100.0f, 10.0f, " ms", 10.0f), knob ("Release", 20.0f, 1500.0f, 200.0f, " ms", 250.0f),
              knob ("Knee", 0.0f, 12.0f, 4.5f, " dB", 0.0f, 1), percent ("Feedback", 25.0f), knob ("Range", -40.0f, 0.0f, -40.0f, " dB", 0.0f, 1),
              percent ("Link", 50.0f), knob ("SC filter", 20.0f, 350.0f, 70.0f, " Hz", 80.0f), percent ("Saturation", 25.0f),
              knob ("Output", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1), percent ("Mix", 100.0f) },
            maker<TrackComp>()));

        list.push_back (info ("ch_telephone", "Telephone", Family::filter, "gfx-formant", chokeCredit,
            "An old phone line: squeezed, band-limited, hissing and crackling.",
            { knob ("Squeeze", -60.0f, 0.0f, -40.0f, " dB", 0.0f, 1), knob ("Noise", -144.0f, 0.0f, -60.0f, " dB", -60.0f, 0),
              knob ("Crackle", -144.0f, 0.0f, -24.0f, " dB", -40.0f, 0), percent ("Crackle %", 25.0f), knob ("Filter", 1000.0f, 3000.0f, 1750.0f, " Hz"),
              knob ("Filter Q", 1.0f, 6.0f, 3.5f, {}, 0.0f, 2), knob ("Drive", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1),
              knob ("Trim", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1), choice ("Clip", { "Overshoot", "Clip" }, 1) },
            maker<Telephone>()));

        list.push_back (info ("ch_eq560", "EQ 560", Family::filter, "gfx-graphic-eq", chokeCredit,
            "A ten-band graphic EQ in the API 560's manner: each band narrower the more it moves.",
            { knob ("31", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1), knob ("63", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1), knob ("125", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1),
              knob ("250", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1), knob ("500", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1), knob ("1K", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1),
              knob ("2K", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1), knob ("4K", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1), knob ("8K", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1),
              knob ("16K", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1), knob ("Trim", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1) },
            maker<Eq560>()));

        list.push_back (info ("ch_ringmod", "Ring Mod", Family::modulation, "gfx-ring-mod", chokeCredit,
            "A ring modulator: eight carriers, bias, instability and feedback.",
            { choice ("Carrier", { "Sine", "Triangle", "Saw up", "Saw down", "Pulse 25%", "Square", "Pulse 75%", "Self" }),
              knob ("kHz", 0.0f, 10.0f, 0.0f, " kHz", 0.0f, 2), knob ("Hz", 1.0f, 1000.0f, 500.0f, " Hz", 300.0f), knob ("Bias", -100.0f, 100.0f, 0.0f, " %"),
              percent ("Instability", 0.0f), percent ("Feedback", 0.0f), percent ("Drive", 0.0f), percent ("Mix", 100.0f),
              knob ("Trim", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1) },
            maker<RingMod>()));

        list.push_back (info ("ch_filthy", "Filthy Delay", Family::time, "gfx-analog-delay", chokeCredit,
            "A delay whose repeats get dirtier: down-sampled and driven in the feedback.",
            { choice ("Routing", { "Stereo", "Inverted", "Ping-pong", "Mono" }), knob ("Time", 1.0f, 3000.0f, 500.0f, " ms", 500.0f),
              choice ("Sync", divisionNames()), percent ("Feedback", 50.0f), knob ("Low cut", 20.0f, 1000.0f, 250.0f, " Hz", 250.0f),
              knob ("High cut", 2000.0f, 20000.0f, 10000.0f, " Hz", 8000.0f), choice ("Quality", { "Clean", "Filthy" }, 1),
              knob ("Drive", 0.0f, 24.0f, 6.0f, " dB", 0.0f, 1), percent ("Dry", 100.0f), percent ("Wet", 50.0f) },
            maker<FilthyDelay>(), true));

        list.push_back (info ("ch_ampsim", "Amp Sim", Family::ampcab, "gfx-preamp", chokeCredit,
            "chokehold's amp: five clipping stages, pedal boost, depth and presence, a maximizer.",
            { knob ("Gate", -96.0f, 0.0f, -96.0f, " dB", -48.0f, 0), choice ("Gate position", { "Input", "Output", "Triggered" }),
              toggle ("Boost", false), percent ("Gain", 50.0f), knob ("Low", -100.0f, 100.0f, 0.0f), knob ("Mid", -100.0f, 100.0f, 0.0f),
              knob ("High", -100.0f, 100.0f, 0.0f), percent ("Depth", 50.0f), percent ("Presence", 50.0f),
              knob ("Maximizer", -6.0f, 0.0f, -3.0f, " dB", 0.0f, 1), knob ("Output", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1),
              choice ("Oversampling", { "Off", "2x", "4x", "8x" }, 1) },
            maker<AmpSim>()));

        {
            juce::StringArray cabs;
            for (const auto& ir : chokeholdcabs::irs)
                cabs.add (ir.name);

            list.push_back (info ("ch_cabsim", "Cab Sim", Family::ampcab, "gfx-cab", chokeCredit,
                "chokehold's cabinets: four guitar 4x12s and a bass 8x10, convolved.",
                { choice ("Cabinet", cabs), knob ("Level", -24.0f, 12.0f, 0.0f, " dB", 0.0f, 1) },
                maker<CabSim>()));
        }
    }
}
