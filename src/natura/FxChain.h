// C:\workspace\SONIFY PLUGIN natura\src\FxChain.h
#pragma once
//==============================================================================
//  FxChain.h  -  what happens after the voices are summed.
//
//  ARCHETYPE STAGE  the parts of an instrument that act on everything it
//                   plays at once: the EP's stereo tremolo, the vibraphone
//                   motor, the tonewheel scanner vibrato / chorus and the
//                   pipe organ's tremulant.
//  FX CHAIN         in series, no sends (as in Zax):
//                   rotary -> phaser -> chorus -> delay -> reverb -> EQ ->
//                   width -> volume -> safety limiter.
//==============================================================================
#include "NaturaVoice.h"

namespace natura
{
    /** Short modulated delay with linear read (chorus, vibrato, doppler). */
    struct ModDelay
    {
        std::vector<float> buf;
        int mask = 0, w = 0;
        void prepare (int maxSamples)
        {
            int n = 16;
            while (n < maxSamples + 4) n <<= 1;
            buf.assign ((size_t) n, 0.0f);
            mask = n - 1;
            w = 0;
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
        float read (float d) const
        {
            d = dsp::clampv (d, 1.0f, (float) (mask - 2));
            const int   i = (int) d;
            const float f = d - (float) i;
            const float a = buf[(size_t) ((w - i) & mask)], b = buf[(size_t) ((w - i - 1) & mask)];
            return a + f * (b - a);
        }
    };

    //==========================================================================
    class ArchetypeStage
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            scanL.prepare ((int) (0.01 * sr)); scanR.prepare ((int) (0.01 * sr));
            tremL.prepare ((int) (0.01 * sr)); tremR.prepare ((int) (0.01 * sr));
            ph = 0.0;
        }

        void reset() { scanL.clear(); scanR.clear(); tremL.clear(); tremR.clear(); }

        /** motorState: 0 = as set, 1 = forced on, 2 = forced off.
            vibState:   0 = as set, 1 = forced on, 2 = forced off. */
        void process (const Snapshot& s, int motorState, int vibState, float* L, float* R, int n)
        {
            switch (s.arch)
            {
                case Archetype::TineEP:
                {
                    const float depth = s[P::ep_tremdepth] / 100.0f;
                    if (depth <= 0.001f) return;
                    const double inc = std::max (0.1f, law::lfoRate (s[P::ep_tremrate])) / sr;
                    for (int i = 0; i < n; ++i)
                    {
                        const float sv = (float) std::sin (dsp::kTwoPi * ph);
                        ph += inc; if (ph >= 1.0) ph -= 1.0;
                        L[i] *= 1.0f - depth * (0.5f + 0.5f * sv);
                        R[i] *= 1.0f - depth * (0.5f - 0.5f * sv);
                    }
                    return;
                }
                case Archetype::StruckBar:
                {
                    float depth = s[P::sb_motordepth] / 100.0f;
                    if (motorState == 1) depth = std::max (depth, 0.45f);
                    if (motorState == 2) depth = 0.0f;
                    if (depth <= 0.001f) return;
                    const double inc = std::max (0.1f, law::lfoRate (s[P::sb_motorrate])) / sr;
                    for (int i = 0; i < n; ++i)
                    {
                        const float sv = (float) std::sin (dsp::kTwoPi * ph);
                        const float sv2 = (float) std::sin (dsp::kTwoPi * ph + 0.6);
                        ph += inc; if (ph >= 1.0) ph -= 1.0;
                        L[i] *= 1.0f - depth * 0.85f * (0.5f + 0.5f * sv);
                        R[i] *= 1.0f - depth * 0.85f * (0.5f + 0.5f * sv2);
                    }
                    return;
                }
                case Archetype::Tonewheel:
                {
                    int mode = (int) s[P::tw_vib];
                    if (vibState == 1 && mode == 0) mode = 6;
                    if (vibState == 2) mode = 0;
                    if (mode == 0) return;
                    static const float depthMs[4] = { 0.0f, 0.22f, 0.45f, 0.9f };
                    const float dms = depthMs[mode > 3 ? mode - 3 : mode];
                    const bool chorus = mode > 3;
                    const double inc = 6.9 / sr;
                    for (int i = 0; i < n; ++i)
                    {
                        const float sv = (float) std::sin (dsp::kTwoPi * ph);
                        ph += inc; if (ph >= 1.0) ph -= 1.0;
                        const float d = (float) ((1.0 + dms * (0.5 + 0.5 * sv)) * 0.001 * sr);
                        scanL.write (L[i]); scanR.write (R[i]);
                        const float wl = scanL.read (d), wr = scanR.read (d);
                        L[i] = chorus ? 0.5f * (L[i] + wl) * 1.3f : wl;
                        R[i] = chorus ? 0.5f * (R[i] + wr) * 1.3f : wr;
                    }
                    return;
                }
                case Archetype::PipeOrgan:
                {
                    const float depth = s[P::po_tremdepth] / 100.0f;
                    if (depth <= 0.001f) return;
                    const double inc = std::max (0.5f, law::lfoRate (s[P::po_tremrate])) / sr;
                    for (int i = 0; i < n; ++i)
                    {
                        const float sv = (float) std::sin (dsp::kTwoPi * ph);
                        ph += inc; if (ph >= 1.0) ph -= 1.0;
                        const float d = (float) ((1.0 + depth * 0.35 * (0.5 + 0.5 * sv)) * 0.001 * sr);
                        tremL.write (L[i]); tremR.write (R[i]);
                        const float a = 1.0f - depth * 0.45f * (0.5f + 0.5f * sv);
                        L[i] = tremL.read (d) * a;
                        R[i] = tremR.read (d) * a;
                    }
                    return;
                }
                default: return;
            }
        }

    private:
        double sr = 48000.0, ph = 0.0;
        ModDelay scanL, scanR, tremL, tremR;
    };

    //==========================================================================
    //  ROTARY SPEAKER: horn and drum with their own inertia, doppler and
    //  amplitude rotation, a tube preamp in front, two mics for stereo.
    //==========================================================================
    class Rotary
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            hornD.prepare ((int) (0.006 * sr));
            drumD.prepare ((int) (0.006 * sr));
            xLp.lowpass (800.0, 0.707, sr);
            xHp.highpass (800.0, 0.707, sr);
            hornSpeed = 0.83f; drumSpeed = 0.67f;
        }
        void reset() { hornD.clear(); drumD.clear(); xLp.reset(); xHp.reset(); }

        void process (float* L, float* R, int n, bool fast, float drive, float balance, float mix)
        {
            const float hT = fast ? 6.7f : 0.83f, dT = fast ? 5.9f : 0.67f;
            const float hk = (float) (1.0 - std::exp (-1.0 / ((hT > hornSpeed ? 0.7 : 1.1) * sr)));
            const float dk = (float) (1.0 - std::exp (-1.0 / ((dT > drumSpeed ? 3.0 : 4.5) * sr)));
            const float pre = 1.0f + drive * 6.0f, post = 1.0f / std::sqrt (pre);
            const float hornG = std::sqrt (balance) * 1.4f, drumG = std::sqrt (1.0f - balance) * 1.4f;

            for (int i = 0; i < n; ++i)
            {
                hornSpeed += (hT - hornSpeed) * hk;
                drumSpeed += (dT - drumSpeed) * dk;
                hPh += hornSpeed / sr; if (hPh >= 1.0) hPh -= 1.0;
                dPh += drumSpeed / sr; if (dPh >= 1.0) dPh -= 1.0;

                const float dry = 0.5f * (L[i] + R[i]);
                const float x = std::tanh (dry * pre) * post;
                const float lo = xLp.process (x), hi = xHp.process (x);
                hornD.write (hi); drumD.write (lo);

                const float hs = (float) std::sin (dsp::kTwoPi * hPh), hc = (float) std::cos (dsp::kTwoPi * hPh);
                const float ds = (float) std::sin (dsp::kTwoPi * dPh), dc = (float) std::cos (dsp::kTwoPi * dPh);
                const float hDel = (float) ((1.6 + 0.65 * hs) * 0.001 * sr);
                const float dDel = (float) ((1.2 + 0.25 * ds) * 0.001 * sr);
                const float h = hornD.read (hDel), d = drumD.read (dDel);

                const float hl = h * (0.75f + 0.25f * hc) * hornG, hr = h * (0.75f - 0.25f * hs) * hornG;
                const float dl = d * (0.85f + 0.15f * dc) * drumG, dr = d * (0.85f - 0.15f * ds) * drumG;
                L[i] = L[i] * (1.0f - mix) + (hl + dl) * mix;
                R[i] = R[i] * (1.0f - mix) + (hr + dr) * mix;
            }
        }

    private:
        double sr = 48000.0, hPh = 0.0, dPh = 0.25;
        float hornSpeed = 0.83f, drumSpeed = 0.67f;
        ModDelay hornD, drumD;
        dsp::Biquad xLp, xHp;
    };

    //==========================================================================
    class Phaser
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; reset(); }
        void reset() { for (auto& c : st) for (auto& a : c) a = {}; fbL = fbR = 0.0f; }

        void process (float* L, float* R, int n, float rateHz, float depth, float fb, float mix)
        {
            const double inc = std::max (0.02f, rateHz) / sr;
            for (int i = 0; i < n; ++i)
            {
                ph += inc; if (ph >= 1.0) ph -= 1.0;
                for (int c = 0; c < 2; ++c)
                {
                    const float lfo = 0.5f + 0.5f * (float) std::sin (dsp::kTwoPi * (ph + c * 0.25));
                    const float hz = 200.0f * std::pow (20.0f, lfo * depth);
                    const float t = (float) std::tan (dsp::kPi * std::min (hz, (float) sr * 0.45f) / sr);
                    const float a = (t - 1.0f) / (t + 1.0f);
                    float* io = c == 0 ? L : R;
                    float& fbS = c == 0 ? fbL : fbR;
                    float y = io[i] + fbS * fb * 0.85f;
                    for (auto& s : st[c]) y = s.process (y, a);
                    fbS = y;
                    io[i] = io[i] * (1.0f - mix * 0.5f) + y * mix * 0.5f;
                }
            }
        }

    private:
        struct AP { float x1 = 0.0f, y1 = 0.0f; float process (float x, float a) { const float y = a * x + x1 - a * y1; x1 = x; y1 = y; return y; } };
        double sr = 48000.0, ph = 0.0;
        AP st[2][6];
        float fbL = 0.0f, fbR = 0.0f;
    };

    //==========================================================================
    class Chorus
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; dl.prepare ((int) (0.03 * sr)); dr.prepare ((int) (0.03 * sr)); }
        void reset() { dl.clear(); dr.clear(); }

        void process (float* L, float* R, int n, float rateHz, float depth, float mix)
        {
            const double inc = std::max (0.05f, rateHz) / sr;
            for (int i = 0; i < n; ++i)
            {
                ph += inc; if (ph >= 1.0) ph -= 1.0;
                const float s1 = (float) std::sin (dsp::kTwoPi * ph), s2 = -s1;
                dl.write (L[i]); dr.write (R[i]);
                const float wl = dl.read ((float) ((7.0 + depth * 4.0 * (0.5 + 0.5 * s1)) * 0.001 * sr));
                const float wr = dr.read ((float) ((7.0 + depth * 4.0 * (0.5 + 0.5 * s2)) * 0.001 * sr));
                L[i] = L[i] * (1.0f - mix * 0.5f) + wl * mix * 0.7f;
                R[i] = R[i] * (1.0f - mix * 0.5f) + wr * mix * 0.7f;
            }
        }

    private:
        double sr = 48000.0, ph = 0.0;
        ModDelay dl, dr;
    };

    //==========================================================================
    class StereoDelay
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; dl.prepare ((int) (2.1 * sr)); dr.prepare ((int) (2.1 * sr)); }
        void reset() { dl.clear(); dr.clear(); lpL.reset(); lpR.reset(); }

        void process (float* L, float* R, int n, float ms, float fb, float tone, bool pingPong, float mix)
        {
            const float target = (float) (ms * 0.001 * sr);
            lpL.setCutoff (dsp::lerp (1500.0f, 14000.0f, tone), sr);
            lpR.a = lpL.a;
            for (int i = 0; i < n; ++i)
            {
                time += (target - time) * 0.0005f;           // glide, no zipper on changes
                const float yl = dl.read (time), yr = dr.read (time);
                const float fl = lpL.process (yl) * fb, fr = lpR.process (yr) * fb;
                if (pingPong)
                {
                    dl.write (0.5f * (L[i] + R[i]) + fr);
                    dr.write (fl);
                }
                else
                {
                    dl.write (L[i] + fl);
                    dr.write (R[i] + fr);
                }
                L[i] += yl * mix;
                R[i] += yr * mix;
            }
        }

    private:
        double sr = 48000.0;
        float time = 10000.0f;
        ModDelay dl, dr;
        dsp::OnePoleLP lpL, lpR;
    };

    //==========================================================================
    //  8-line feedback delay network reverb with input diffusion, damping and
    //  pre-delay.
    //==========================================================================
    class Reverb
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            const double scale = sr / 48000.0;
            static const int baseLen[8] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
            for (int i = 0; i < 8; ++i)
            {
                lines[i].prepare ((int) (baseLen[i] * scale * 2.2) + 8);
                len[i] = (float) (baseLen[i] * scale);
            }
            static const int apLen[4] = { 142, 107, 379, 277 };
            for (int i = 0; i < 4; ++i) { diff[i].prepare ((int) (apLen[i] * scale) + 4); diffLen[i] = (int) (apLen[i] * scale); }
            pre.prepare ((int) (0.25 * sr));
            reset();
        }

        void reset()
        {
            for (auto& l : lines) l.clear();
            for (auto& d : diff) d.clear();
            pre.clear();
            for (auto& d : damp) d.reset();
        }

        void process (float* L, float* R, int n, float size, float dampAmt, float preDelay, float mix)
        {
            const float rt60 = dsp::lerp (0.5f, 7.0f, size * size);
            const float lscale = dsp::lerp (0.55f, 1.6f, size);
            float g[8], d[8];
            for (int i = 0; i < 8; ++i)
            {
                d[i] = len[i] * lscale;
                g[i] = (float) std::pow (10.0, -3.0 * d[i] / (rt60 * sr));
                damp[i].setCutoff (dsp::lerp (14000.0f, 1800.0f, dampAmt), sr);
            }
            const float preS = (float) (preDelay * 0.2 * sr) + 1.0f;

            for (int i = 0; i < n; ++i)
            {
                modPh += 0.37 / sr; if (modPh >= 1.0) modPh -= 1.0;
                const float mod = (float) std::sin (dsp::kTwoPi * modPh) * 6.0f;

                pre.write (0.5f * (L[i] + R[i]));
                float x = pre.read (preS);
                for (int k = 0; k < 4; ++k)
                {
                    const float delayed = diff[k].read ((float) diffLen[k]);
                    const float v = x + 0.6f * delayed;
                    diff[k].write (v);
                    x = delayed - 0.6f * v;
                }

                float o[8];
                for (int k = 0; k < 8; ++k)
                    o[k] = damp[k].process (lines[k].read (d[k] + (k < 2 ? (k == 0 ? mod : -mod) : 0.0f))) * g[k];

                // Hadamard-style mixing (orthogonal, energy preserving)
                float a[8];
                for (int k = 0; k < 8; k += 2) { a[k] = o[k] + o[k + 1]; a[k + 1] = o[k] - o[k + 1]; }
                float b[8];
                for (int k = 0; k < 8; k += 4)
                    for (int j = 0; j < 2; ++j) { b[k + j] = a[k + j] + a[k + j + 2]; b[k + j + 2] = a[k + j] - a[k + j + 2]; }
                float c[8];
                for (int j = 0; j < 4; ++j) { c[j] = b[j] + b[j + 4]; c[j + 4] = b[j] - b[j + 4]; }
                const float norm = 0.35355339f;
                for (int k = 0; k < 8; ++k) lines[k].write (c[k] * norm + (k % 2 == 0 ? x : -x) * 0.5f);

                const float wl = (o[0] + o[2] + o[4] + o[6]) * 0.5f;
                const float wr = (o[1] + o[3] + o[5] + o[7]) * 0.5f;
                L[i] = L[i] * (1.0f - mix * 0.5f) + wl * mix;
                R[i] = R[i] * (1.0f - mix * 0.5f) + wr * mix;
            }
        }

    private:
        double sr = 48000.0, modPh = 0.0;
        ModDelay lines[8], diff[4], pre;
        float len[8] = {};
        int   diffLen[4] = {};
        dsp::OnePoleLP damp[8];
    };

    //==========================================================================
    class FxChain
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            stage.prepare (sr); rotary.prepare (sr); phaser.prepare (sr); chorus.prepare (sr);
            delay.prepare (sr); reverb.prepare (sr);
            reset();
        }

        void reset()
        {
            stage.reset(); rotary.reset(); phaser.reset(); chorus.reset(); delay.reset(); reverb.reset();
            for (auto& e : eqLo) e.reset();
            for (auto& e : eqHi) e.reset();
        }

        void process (const Snapshot& s, bool rotorFast, int motorState, int vibState, float* L, float* R, int n)
        {
            stage.process (s, motorState, vibState, L, R, n);

            if (s[P::rot_on] >= 0.5f)
                rotary.process (L, R, n, rotorFast, s[P::rot_drive] / 100.0f, s[P::rot_balance] / 100.0f, s[P::rot_mix] / 100.0f);
            if (s[P::pha_on] >= 0.5f)
                phaser.process (L, R, n, law::lfoRate (s[P::pha_rate]), s[P::pha_depth] / 100.0f, s[P::pha_fb] / 100.0f, s[P::pha_mix] / 100.0f);
            if (s[P::cho_on] >= 0.5f)
                chorus.process (L, R, n, law::lfoRate (s[P::cho_rate]), s[P::cho_depth] / 100.0f, s[P::cho_mix] / 100.0f);
            if (s[P::dly_on] >= 0.5f)
                delay.process (L, R, n, law::delayMs (s[P::dly_time]), s[P::dly_fb] / 100.0f * 0.92f, s[P::dly_tone] / 100.0f,
                               s[P::dly_pingpong] >= 0.5f, s[P::dly_mix] / 100.0f);
            if (s[P::rev_on] >= 0.5f)
                reverb.process (L, R, n, s[P::rev_size] / 100.0f, s[P::rev_damp] / 100.0f, s[P::rev_predelay] / 100.0f, s[P::rev_mix] / 100.0f);

            const float lo = s[P::eq_low], hi = s[P::eq_high];
            const bool eqOn = std::abs (lo) > 0.05f || std::abs (hi) > 0.05f;
            if (eqOn && (lo != lastLo || hi != lastHi))
            {
                lastLo = lo; lastHi = hi;
                for (auto& e : eqLo) e.lowShelf (120.0, lo, sr);
                for (auto& e : eqHi) e.highShelf (6000.0, hi, sr);
            }
            const float width = s[P::out_width] / 100.0f;
            const float vol = dsp::dbToGain (s[P::out_volume]);
            for (int i = 0; i < n; ++i)
            {
                float l = L[i], r = R[i];
                if (eqOn) { l = eqHi[0].process (eqLo[0].process (l)); r = eqHi[1].process (eqLo[1].process (r)); }
                const float m = 0.5f * (l + r), sd = 0.5f * (l - r) * width;
                l = (m + sd) * vol; r = (m - sd) * vol;
                // safety: a soft knee above -1 dBFS, never a hard clip
                L[i] = std::abs (l) < 0.89f ? l : std::copysign (0.89f + 0.11f * std::tanh ((std::abs (l) - 0.89f) / 0.11f), l);
                R[i] = std::abs (r) < 0.89f ? r : std::copysign (0.89f + 0.11f * std::tanh ((std::abs (r) - 0.89f) / 0.11f), r);
            }
        }

    private:
        double sr = 48000.0;
        ArchetypeStage stage;
        Rotary rotary;
        Phaser phaser;
        Chorus chorus;
        StereoDelay delay;
        Reverb reverb;
        dsp::Biquad eqLo[2], eqHi[2];
        float lastLo = 1000.0f, lastHi = 1000.0f;
    };
}
