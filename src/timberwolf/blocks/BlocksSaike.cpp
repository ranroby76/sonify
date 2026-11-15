// D:\workspace\Fanan_web_hub\src\timberwolf\blocks\BlocksSaike.cpp
//==============================================================================
//  Saike's effects (Joep Vanlier, github.com/JoepVanlier/JSFX, MIT, Copyright
//  (c) 2022 Joep Vanlier), ported from his JSFX: ToneStacks - fourteen amps'
//  passive tone networks, their knobs smoothed and their level kept even.
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
#include "SaikeToneStacks.h"
#include "../TwRegistry.h"

#include <complex>

namespace tw
{
    namespace
    {
        const juce::String saikeCredit = "Saike / Joep Vanlier (github.com/JoepVanlier/JSFX), MIT";

        class ToneStackBlock final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                model = -1;
                reset();
            }

            void reset() override
            {
                for (auto& s : state)
                    std::fill (std::begin (s), std::end (s), 0.0);

                sBass = knob[1] * 0.01;
                sMid = knob[2] * 0.01;
                sTreble = knob[3] * 0.01;
                dirty = true;
            }

            void process (float* l, float* r, int n) override
            {
                const int wanted = juce::jlimit (0, 13, (int) knob[0]);
                const double tb = knob[1] * 0.01, tm = knob[2] * 0.01, tt = knob[3] * 0.01;
                const int normalise = (int) knob[4];
                const double level = dbToGain (knob[5]);

                if (wanted != model)
                {
                    model = wanted;
                    for (auto& s : state) std::fill (std::begin (s), std::end (s), 0.0);
                    dirty = true;
                }

                for (int i = 0; i < n; ++i)
                {
                    // His smoothing: 1 % of the way each sample, the stack worked out again every 16.
                    const double db = tb - sBass, dm = tm - sMid, dt = tt - sTreble;
                    sBass += 0.01 * db; sMid += 0.01 * dm; sTreble += 0.01 * dt;

                    const bool moving = std::abs (db) + std::abs (dm) + std::abs (dt) > 1.0e-7;

                    if ((moving && (++counter & 15) == 0) || dirty)
                    {
                        design();
                        dirty = false;
                        settled = false;
                    }

                    if (! moving && ! settled)
                    {
                        gainComp = estimateGain();
                        settled = true;
                    }

                    gainNow = normalise > 0 ? 0.95 * gainNow + 0.05 * (normalise == 2 ? gainComp * rmsTrim : gainComp) : 1.0;

                    l[i] = (float) (run (state[0], l[i]) * gainNow * level);
                    r[i] = (float) (run (state[1], r[i]) * gainNow * level);
                }
            }

        private:
            void design()
            {
                using namespace saikestacks;
                const double K = 2.0 * rate;   // his "correct frequency scaling"
                const double vol = 1.0;

                switch (model)
                {
                    case 0:  initMarshall (K, sBass, sMid, sTreble, vol, s); break;
                    case 1:  initFender (K, sBass, sMid, sTreble, vol, s); break;
                    case 2:  initVox (K, sBass, sMid, sTreble, vol, s); break;
                    case 3:  initJames (K, sBass, sMid, sTreble, vol, s); break;
                    case 4:  initESeries (K, sBass, sMid, sTreble, vol, s); break;
                    case 5:  initBench (K, sBass, sMid, sTreble, vol, s); break;
                    case 6:  initBigMuff (K, sBass, sMid, sTreble, vol, s); break;
                    case 7:  initHiwatt (K, sBass, sMid, sTreble, vol, s); break;
                    case 8:  initCrate (K, sBass, sMid, sTreble, vol, s); break;
                    case 9:  initDumbleRock (K, sBass, sMid, sTreble, vol, s); break;
                    case 10: initDumbleJazz (K, sBass, sMid, sTreble, vol, s); break;
                    case 11: initAria (K, sBass, sMid, sTreble, vol, s); break;
                    case 12: initBoneRay (K, sBass, sMid, sTreble, vol, s); break;
                    default: initWah (K, sBass, sMid, sTreble, vol, s); break;
                }

                a[1] = s.a1; a[2] = s.a2; a[3] = s.a3; a[4] = s.a4; a[5] = s.a5; a[6] = s.a6;
                b[0] = s.b0; b[1] = s.b1; b[2] = s.b2; b[3] = s.b3; b[4] = s.b4; b[5] = s.b5; b[6] = s.b6;
                order = juce::jlimit (1, 6, s.coeffs - 1);
            }

            double run (double* z, double x) const noexcept
            {
                const double y = b[0] * x + z[0];

                for (int k = 1; k < order; ++k)
                    z[k - 1] = b[k] * x - a[k] * y + z[k];

                z[order - 1] = b[order] * x - a[order] * y;
                return std::isfinite (y) ? y : 0.0;
            }

            /** His "Normalize gain": the stack's peak gain over the band, brought to 2 (his
                level), worked out from its transfer function rather than an FFT of its impulse. */
            double estimateGain()
            {
                double peak = 1.0e-6, power = 0.0;
                constexpr int points = 192;

                for (int p = 0; p < points; ++p)
                {
                    const double hz = 20.0 * std::pow (1000.0, (double) p / (points - 1));
                    const double w = juce::MathConstants<double>::twoPi * hz / rate;
                    const std::complex<double> zi = std::polar (1.0, -w);
                    std::complex<double> num = b[0], den = 1.0, zk = 1.0;

                    for (int k = 1; k <= order; ++k)
                    {
                        zk *= zi;
                        num += b[k] * zk;
                        den += a[k] * zk;
                    }

                    const double m = std::abs (num / den);
                    peak = juce::jmax (peak, m);
                    power += m * m;
                }

                rmsTrim = peak / juce::jmax (1.0e-6, std::sqrt (power / points));
                return juce::jlimit (0.05, 40.0, 2.0 / peak * 0.5);
            }

            saikestacks::Stack s;
            double a[7] {}, b[7] { 1 };
            int order = 3;
            double state[2][6] {};
            double rate = 48000.0, sBass = 0.5, sMid = 0.5, sTreble = 0.5;
            double gainComp = 1.0, gainNow = 1.0, rmsTrim = 1.0;
            int model = -1, counter = 0;
            bool dirty = true, settled = false;
        };
    }

    void registry::addSaike (std::vector<BlockInfo>& list)
    {
        using namespace kit;

        list.push_back (info ("sk_tonestack", "Tone Stack", Family::filter, "gfx-tone", saikeCredit,
            "Fourteen amps' own bass, mid and treble networks, from their component values.",
            { choice ("Stack", { "Marshall", "Fender", "Vox", "James", "E-Series", "Bench", "Big Muff", "Hiwatt", "Crate",
                                 "Dumble Rock", "Dumble Jazz", "Aria", "Bone Ray", "Wah" }),
              percent ("Bass", 50.0f), percent ("Mid", 50.0f), percent ("Treble", 50.0f),
              choice ("Normalize", { "Off", "Peak", "RMS" }, 1), knob ("Level", -18.0f, 12.0f, 0.0f, " dB", 0.0f, 1) },
            maker<ToneStackBlock>()));
    }
}
