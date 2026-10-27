// C:\workspace\SONIFY PLUGIN natura\src\ModelsKeys.h
#pragma once
//==============================================================================
//  ModelsKeys.h  -  the three "keyboard" archetypes.
//
//    STRUCK STRING   piano family: hammer -> 1..3 detuned waveguide strings
//                    with dispersion (stiffness), register-dependent decay,
//                    dampers, a sympathetic octave string and key-off thump.
//    TINE / REED EP  electric piano: a hammered modal tine/reed into a
//                    pickup non-linearity (Rhodes bark, Wurli bite).
//    STRUCK BAR      mallets: six modes whose ratios morph across wood,
//                    tuned metal, free bars and cantilever combs, a mallet
//                    pulse and a tube resonator.
//==============================================================================
#include "ModelBase.h"

namespace natura
{
    //==========================================================================
    //  A felt (or neoprene, or yarn) hammer's force on what it strikes.  Felt
    //  stiffens as it is squeezed, so the harder the blow the shorter the contact
    //  and the sharper its peak: a smooth sin^p bump for a gentle touch, turning
    //  into a peaked, nearly triangular pulse for a hard one (that corner is what
    //  carries the top of a hard blow; a smooth bump's spectrum dies away).  The
    //  area (what the blow gives the string) stays that of a plain bump.
    //==========================================================================
    struct HammerForce
    {
        int   len = 0, pos = 0;
        float ampSmooth = 0.0f, ampCusp = 0.0f, p = 1.0f, spike = 0.0f, spikeMul = 0.5f;
        static constexpr float cuspA = 1.0f;

        /** sharpness 1 (round) .. 5 (very peaked); cusp 0..1 how much of the peaked pulse;
            edge 0..1 the felt's hard outer skin, a fraction of a millisecond of sharp contact
            as the blow lands (broadband: the "ting" in a hard piano note). */
        void trigger (float lengthSamples, float amplitude, float sharpness, float cusp, float edge = 0.0f, double sr = 48000.0)
        {
            spike    = amplitude * edge;
            spikeMul = (float) std::exp (-1.0 / (0.0001 * sr));
            len = std::max (2, (int) lengthSamples);
            pos = 0;
            p   = std::max (1.0f, sharpness);
            cusp = dsp::clampv (cusp, 0.0f, 1.0f);
            // each shape's area brought to the raised cosine's 0.5
            const float areaS = (float) (std::tgamma ((p + 1.0) * 0.5) / (std::sqrt (dsp::kPi) * std::tgamma (p * 0.5 + 1.0)));
            const float areaC = 1.0f - 1.0f / (cuspA + 1.0f);
            ampSmooth = amplitude * (1.0f - cusp) * 0.5f / std::max (0.05f, areaS);
            ampCusp   = amplitude * cusp * 0.5f / areaC;
        }
        bool  running() const { return pos < len; }
        float next()
        {
            if (pos >= len) return 0.0f;
            const double x = (double) pos / (double) len;
            ++pos;
            const float s = (float) std::sin (dsp::kPi * x);
            float y = s <= 0.0f ? 0.0f : ampSmooth * std::exp (p * std::log (s));
            y += spike;
            spike *= spikeMul;
            if (ampCusp != 0.0f) y += ampCusp * (1.0f - (float) std::pow (std::abs (2.0 * x - 1.0), (double) cuspA));
            return y;
        }
    };

    //==========================================================================
    //  The knock of a strike: what the frame, the soundboard or the case says as
    //  the hammer lands (three short low resonances) and the click of the impact
    //  itself (a few milliseconds of noise high up).  The first thing the ear
    //  hears of a piano, a mallet or a tine.
    //==========================================================================
    struct StrikeKnock
    {
        dsp::Mode modes[3];
        float gains[3] = { 1.0f, 0.6f, 0.35f };
        dsp::Burst click;
        dsp::Biquad clickHp;
        bool  active = false;

        void reset() { for (auto& m : modes) m.reset(); click = {}; clickHp.reset(); active = false; }

        void set (const float hz[3], const float t60[3], double sr)
        {
            for (int k = 0; k < 3; ++k) modes[k].set (hz[k], t60[k], sr);
        }

        /** thump: the resonances' strike, clickAmp: the impact click, clickHz its band. */
        void strike (float thump, float clickAmp, double clickHz, double sr)
        {
            for (int k = 0; k < 3; ++k) { modes[k].reset(); modes[k].process (thump * gains[k]); }
            if (clickAmp > 0.0f) { click.trigger (clickAmp, 0.006f, sr); clickHp.highpass (clickHz, 0.7, sr); }
            active = true;
        }

        float next (dsp::Noise& n)
        {
            if (! active) return 0.0f;
            float y = 0.0f, e = 0.0f;
            for (auto& m : modes) { y += m.process (0.0f); e += m.energy(); }
            if (click.running()) y += clickHp.process (click.next() * n.next());
            else if (e < 1.0e-12f) active = false;
            return y;
        }
    };

    //==========================================================================
    //  STRUCK STRING
    //  spec: 0 hammer position, 1 strings, 2 detune, 3 sympathetic,
    //        4 key-off, 5 pedal noise (engine), 6 lid
    //==========================================================================
    class StruckStringModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            const int maxLen = (int) (sr / 20.0) + 64;
            for (auto& l : line) l.prepare (maxLen);
            symp.prepare (maxLen);
            excBuf.prepare ((int) (sr / 20.0) + 64);
            reset();
        }

        void reset() override
        {
            for (auto& l : line) l.clear();
            symp.clear();
            excBuf.clear();
            for (int s = 0; s < 3; ++s)
            {
                lpY[s] = 0.0f;
                for (auto& a : ap[s]) a.reset();
                loopDc[s].prepare (sr, 1.0);
                loopDc[s].reset();
            }
            sympY = 0.0f;
            hammer = {};
            thump = {};
            thumpLp.reset();
            knock.reset();
            prompt = 0.0f;
            toneLp.reset();
            outDc.prepare (sr, 20.0);
            outDc.reset();
        }

        bool isSustained() const override { return false; }
        Dynamics dynamics() const override { return Dynamics::Struck; }

        void start (const ModelIn& in, bool legato) override
        {
            note = in.note;
            released = false;
            noise.seed (0xA5A5u + in.seed * 2654435761u + (std::uint32_t) in.note * 7919u);
            configure (in, ! legato);

            // hammer: harder + louder = shorter contact and a sharper peak (the felt
            // stiffens as it's squeezed), so a hard blow is brighter, not only louder
            const float hard = dsp::clampv (in.hardness * 0.55f + in.velocity * 0.45f
                                            - in.soft * 0.25f + in.bright * 0.2f, 0.0f, 1.0f);
            const float msFree = dsp::lerp (4.0f, 0.4f, std::pow (hard, 0.7f));
            const float ms     = std::min (msFree, 0.85f * 1000.0f / std::max (20.0f, in.freq));   // treble hammers are light and quick
            const float lenS   = (float) (ms * 0.001 * sr);
            const float treble = 1.0f + dsp::clampv ((float) (in.note - 72) / 36.0f, 0.0f, 1.0f);
            const float amp    = std::pow (in.velocity, 1.4f) * (0.45f + in.strength) * treble * (legato ? 0.5f : 1.0f);
            hammer.trigger (lenS, amp * 24.0f * (float) (sr / 48000.0) / std::max (8.0f, (float) (msFree * 0.001 * sr)),
                            dsp::lerp (1.4f, 3.0f, hard), std::pow (hard, 1.5f) * (0.4f + 0.6f * in.transient),
                            std::pow (hard, 3.0f) * (0.3f + 0.7f * in.transient) * 1.4f, sr);
            hammerNoise = in.noise * 0.35f;

            // the knock: the frame and soundboard as the hammer lands, and the impact
            if (! legato)
            {
                const float tr = in.transient;
                const float k  = std::pow (in.velocity, 1.6f) * (0.25f + 0.75f * tr) * (0.6f + 0.6f * hard);
                const float hz[3]  = { 92.0f, 176.0f, 330.0f };
                const float t60[3] = { 0.09f, 0.06f, 0.04f };
                knock.set (hz, t60, sr);
                knock.strike (k * 0.05f, k * (0.05f + 0.08f * hard), dsp::lerp (1500.0f, 3000.0f, hard), sr);

                // prompt sound and aftersound: the strings of a unison pass their energy
                // to the bridge fast at first, then slowly - a piano note drops quickly
                // from its strike to the long ring under it
                const float reg = dsp::clampv ((float) (in.note - 21) / 87.0f, 0.0f, 1.0f);
                prompt = 1.0f;
                promptMul = dsp::sampleGainForT60 (dsp::lerp (2.6f, 0.55f, reg) * std::pow (2.0f, (in.decay - 0.5f) * 1.5f), sr);
                after = dsp::lerp (0.62f, 0.72f, reg);
            }
        }

        void release (const ModelIn& in) override
        {
            released = true;
            configure (in, false);
            if (in.spec[4] > 0.001f && note < 89)
                thump.trigger (in.spec[4] * 0.08f * (0.4f + in.velocity), 0.035f, sr);
        }

        void render (const ModelIn& in, float* out, int n) override
        {
            if (std::abs (in.freq - lastFreq) > 0.01f || in.pedal != lastPedal || in.released != released
                || in.damping != lastDamp || in.decay != lastDecay)
            {
                released = in.released;
                configure (in, false);
            }

            const float posD   = std::max (2.0f, period * dsp::lerp (0.04f, 0.22f, in.spec[0]));
            const float lid    = in.spec[6];
            const float sympLv = in.spec[3];
            const float inv    = 1.0f / std::sqrt ((float) nStrings);

            for (int i = 0; i < n; ++i)
            {
                // excitation: hammer pulse with a little felt noise, comb-filtered by position
                float e = hammer.next();
                if (e != 0.0f) e *= 1.0f + hammerNoise * noise.next();
                excBuf.write (e);
                const float ex = e - 0.9f * excBuf.tap (posD);

                float sum = 0.0f;
                for (int s = 0; s < nStrings; ++s)
                {
                    const float v = line[s].tap (len[s]);
                    lpY[s] = (1.0f - lpA) * v + lpA * lpY[s];
                    float y = lpY[s] * g[s];
                    for (int k = 0; k < nAp; ++k) y = ap[s][k].process (y);
                    y = loopDc[s].process (y);      // a string has no DC mode
                    line[s].write (y + ex);
                    sum += v;
                }
                sum *= inv;

                // sympathetic octave string, fed lightly from the main strings
                if (sympLv > 0.001f)
                {
                    const float v = symp.tap (sympLen);
                    sympY = 0.5f * v + 0.5f * sympY;
                    symp.write (sympY * sympG + sum * 0.015f * sympLv);
                    sum += v * sympLv * 0.6f;
                }

                // prompt sound over aftersound
                sum *= after + (1.0f - after) * prompt;
                prompt *= promptMul;

                // the knock of the strike, and the key-off thump
                sum += knock.next (noise);
                if (thump.running())
                    sum += thumpLp.process (thump.next() * noise.next());

                // lid: open = more top end
                sum = outDc.process (sum);
                const float low = toneLp.process (sum);
                out[i] = (low + (0.45f + lid * 0.9f) * (sum - low)) * 1.6f;
            }
        }

    private:
        void configure (const ModelIn& in, bool fresh)
        {
            lastFreq = in.freq;  lastPedal = in.pedal;  lastDamp = in.damping;  lastDecay = in.decay;
            const float f0 = dsp::clampv (in.freq, 20.0f, (float) sr * 0.3f);
            period = (float) (sr / f0);

            // how many strings: the register decides the ceiling, the knob the count
            int want = 1 + (int) std::lround (in.spec[1] * 2.0f);
            const int regMax = in.note < 34 ? 1 : (in.note < 49 ? 2 : 3);
            want = std::min (want, regMax);
            if (in.soft > 0.5f) want = std::max (1, want - 1);
            if (fresh || want != nStrings)
            {
                if (fresh) for (auto& l : line) l.clear();
                nStrings = want;
            }

            // decay: long in the bass, short in the treble
            float t60 = 16.0f * std::pow (0.965f, (float) (in.note - 21));
            t60 = dsp::clampv (t60, 0.45f, 16.0f) * std::pow (2.0f, (in.decay - 0.5f) * 3.0f) * in.decayMul;
            t60 *= 1.0f - 0.92f * in.dampAdd;

            const bool hasDamper = in.note < 89;
            if (released && hasDamper)
            {
                const float damped = in.note < 48 ? 0.22f : 0.11f;
                t60 = std::exp (dsp::lerp (std::log (damped), std::log (t60), dsp::clampv (in.pedal, 0.0f, 1.0f)));
            }

            // loop low-pass: the string's own losses
            float a = 0.02f + 0.32f * in.damping + (in.note < 40 ? 0.15f * (40 - in.note) / 40.0f : 0.0f);
            a *= dsp::clampv (std::pow (300.0f / f0, 0.6f), 0.15f, 1.0f);    // per-period loss shrinks up the keyboard
            a += 0.3f * in.dampAdd;
            lpA = dsp::clampv (a - in.bright * 0.1f, 0.0f, 0.9f);

            // stiffness: allpass dispersion, strongest at the extremes of the keyboard
            const float regF = 0.45f + 0.55f * std::min (1.0f, std::abs ((float) in.note - 62.0f) / 46.0f);
            float c = -(0.08f + 0.62f * in.inharm) * regF;
            const double w = dsp::kTwoPi * f0 / sr;
            const float lpD = dsp::OnePoleLP::phaseDelay (lpA, w);

            nAp = 4;
            float apD = dsp::Allpass1::phaseDelay (c, w);
            while (nAp > 0 && period - lpD - apD * (float) nAp < 3.0f) --nAp;
            if (nAp == 0) c = 0.0f;
            for (int s = 0; s < 3; ++s) for (auto& f : ap[s]) f.c = c;

            // in the treble the loop runs thousands of times a second: never let
            // the loss filter alone take more per pass than the decay allows
            {
                const float need = dsp::loopGainForT60 (t60, f0) / 0.9999f;
                for (int it = 0; it < 12 && lpA > 0.0005f
                     && dsp::OnePoleLP::magnitude (lpA, w) < need; ++it)
                    lpA *= 0.6f;
            }

            const float spread = in.spec[2] * 8.0f;           // cents
            const float cents[3] = { 0.0f, spread, -spread * 0.6f };
            for (int s = 0; s < 3; ++s)
            {
                const float fs = f0 * std::pow (2.0f, cents[s] / 1200.0f);
                const double ws = dsp::kTwoPi * fs / sr;
                const float  ps = (float) (sr / fs);
                const float  dLp = dsp::OnePoleLP::phaseDelay (lpA, ws);
                const float  dAp = nAp > 0 ? dsp::Allpass1::phaseDelay (c, ws) * (float) nAp : 0.0f;
                // the in-loop DC blocker (1 Hz) only leads the lowest fundamentals by a
                // few cents; compensating it would detune every upper partial instead
                len[s] = std::max (2.0f, ps - dLp - dAp);
                const float mag = dsp::OnePoleLP::magnitude (lpA, ws) * dsp::FracDelay::lagrangeMag (len[s], ws);
                g[s] = std::min (0.99995f, dsp::loopGainForT60 (t60, fs) / std::max (0.05f, mag));
            }

            // sympathetic octave string
            const float f2 = std::min (f0 * 2.0f, (float) sr * 0.3f);
            const double w2 = dsp::kTwoPi * f2 / sr;
            sympLen = std::max (2.0f, (float) (sr / f2) - dsp::OnePoleLP::phaseDelay (0.5f, w2));
            sympG   = std::min (0.9999f, dsp::loopGainForT60 (t60 * 1.6f, f2)
                                         / std::max (0.05f, dsp::OnePoleLP::magnitude (0.5f, w2) * dsp::FracDelay::lagrangeMag (sympLen, w2)));

            toneLp.setCutoff (1500.0, sr);
            thumpLp.setCutoff (260.0, sr);
        }

        dsp::FracDelay line[3], symp, excBuf;
        dsp::Allpass1  ap[3][4];
        float lpY[3] = {}, len[3] = { 100, 100, 100 }, g[3] = { 0.99f, 0.99f, 0.99f };
        float lpA = 0.2f, period = 100.0f, sympLen = 50.0f, sympG = 0.99f, sympY = 0.0f;
        int   nStrings = 1, nAp = 0, note = 60;
        float hammerNoise = 0.0f;
        float lastFreq = -1.0f, lastPedal = -1.0f, lastDamp = -1.0f, lastDecay = -1.0f;
        bool  released = false;
        HammerForce hammer;
        StrikeKnock knock;
        float prompt = 0.0f, promptMul = 0.9999f, after = 0.5f;
        dsp::Burst thump;
        dsp::DCBlocker loopDc[3], outDc;
        dsp::OnePoleLP thumpLp, toneLp;
        dsp::Noise noise;
    };

    //==========================================================================
    //  TINE / REED ELECTRIC PIANO
    //  spec: 0 type (Rhodes..Wurli), 1 tine chime, 2 pickup, 3 bark,
    //        4 tremolo rate (engine), 5 tremolo depth (engine), 6 key-off
    //==========================================================================
    class TineModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override { sr = sampleRate; reset(); }

        void reset() override
        {
            for (auto& m : modes) m.reset();
            hammer = {};
            keyoff = {};
            keyLp.reset();
            knock.reset();
        }

        bool isSustained() const override { return false; }
        Dynamics dynamics() const override { return Dynamics::Struck; }

        void start (const ModelIn& in, bool legato) override
        {
            note = in.note;
            released = false;
            noise.seed (0x51u + in.seed * 2654435761u + (std::uint32_t) in.note * 131u);
            configure (in);
            const float hard = dsp::clampv (in.hardness * 0.5f + in.velocity * 0.5f + in.bright * 0.2f, 0.0f, 1.0f);
            const float lenS = (float) (dsp::lerp (2.8f, 0.45f, hard) * 0.001 * sr);
            hammerLen = lenS;
            const float strike = std::pow (in.velocity, 1.3f) * (0.4f + in.strength * 1.2f) * (legato ? 0.4f : 1.0f);
            hammer.trigger (lenS, strike, dsp::lerp (1.3f, 2.6f, hard), hard * hard * (0.4f + 0.6f * in.transient));
            velNow = in.velocity;

            // a hard blow rings the tine's (or reed's) upper modes far more than a soft one -
            // the "ping" of a struck tine - and the hammer tip clicks as it lands
            chimeVel = 0.45f + 1.6f * in.velocity * in.velocity * (0.5f + in.transient);
            configure (in);
            if (! legato)
            {
                const float k = std::pow (in.velocity, 1.8f) * (0.2f + 0.8f * in.transient);
                const float hz[3]  = { 220.0f, 540.0f, 1300.0f };
                const float t60[3] = { 0.03f, 0.02f, 0.012f };
                knock.set (hz, t60, sr);
                knock.strike (k * 0.05f, k * 0.12f * (0.4f + hard), dsp::lerp (2200.0f, 4000.0f, hard), sr);
            }
        }

        void release (const ModelIn& in) override
        {
            released = true;
            configure (in);
            if (in.spec[6] > 0.001f)
                keyoff.trigger (in.spec[6] * 0.05f, 0.03f, sr);
        }

        void render (const ModelIn& in, float* out, int n) override
        {
            if (std::abs (in.freq - lastFreq) > 0.01f || in.pedal != lastPedal || in.released != released
                || in.decay != lastDecay)
            {
                released = in.released;
                configure (in);
            }

            const float type  = in.spec[0];
            const float drive = 0.7f + in.spec[3] * 2.6f * (0.35f + velNow);
            const float p     = (in.spec[2] - 0.5f) * 1.5f;
            const float tp    = std::tanh (p);
            const float norm  = 1.0f / (0.55f + drive * 0.45f);
            const float hn    = 2.0f / std::max (2.0f, hammerLen);

            for (int i = 0; i < n; ++i)
            {
                const float h = hammer.next() * hn;
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                    if (modes[k].active) sum += modes[k].process (h * amp[k]);

                // pickup: asymmetric for the tine (even harmonics, the "bark"),
                // symmetric and harder for the reed (odd harmonics, the "bite")
                const float x  = sum * drive;
                const float rh = std::tanh (x + p) - tp;
                const float xc = dsp::clampv (x, -1.6f, 1.6f);
                const float wu = xc - 0.26f * xc * xc * xc;
                float y = dsp::lerp (rh, wu, type) * norm;

                if (keyoff.running()) y += keyLp.process (keyoff.next() * noise.next());
                y += knock.next (noise);
                out[i] = y * 0.5f;
            }
        }

    private:
        void configure (const ModelIn& in)
        {
            lastFreq = in.freq;  lastPedal = in.pedal;  lastDecay = in.decay;
            const float f0   = in.freq;
            const float type = in.spec[0];
            const float chime = in.spec[1];

            // Rhodes: tine + tonebar + cantilever overtones.  Wurli: reed overtones, shorter.
            const float ratio[4] = { 1.0f, dsp::lerp (2.0f, 3.0f, type), 6.27f, 17.55f };
            amp[0] = 1.0f;
            amp[1] = dsp::lerp (0.12f, 0.04f, type);
            amp[2] = chime * dsp::lerp (0.55f, 0.40f, type) * chimeVel;
            amp[3] = chime * dsp::lerp (0.20f, 0.12f, type) * chimeVel;

            float t60 = dsp::clampv (6.5f * std::pow (0.968f, (float) (in.note - 36)), 0.6f, 9.0f);
            t60 *= dsp::lerp (1.0f, 0.55f, type) * std::pow (2.0f, (in.decay - 0.5f) * 3.0f) * in.decayMul;
            t60 *= 1.0f - 0.9f * in.dampAdd;
            float tChime = dsp::lerp (0.30f, 0.18f, type) * (0.5f + in.decay);

            if (released)
            {
                const float k = dsp::clampv (in.pedal, 0.0f, 1.0f);
                t60    = std::exp (dsp::lerp (std::log (0.12f), std::log (t60), k));
                tChime = std::min (tChime, t60);
            }

            const float damp = 1.0f - 0.6f * in.damping;
            modes[0].set (f0 * ratio[0], t60, sr);
            modes[1].set (f0 * ratio[1], t60 * 0.6f, sr);
            modes[2].set (f0 * ratio[2], tChime * damp, sr);
            modes[3].set (f0 * ratio[3], tChime * 0.3f * damp, sr);
            keyLp.setCutoff (900.0, sr);
        }

        dsp::Mode  modes[4];
        float      amp[4] = { 1, 0.1f, 0.3f, 0.1f };
        HammerForce hammer;
        StrikeKnock knock;
        float chimeVel = 1.0f;
        dsp::Burst keyoff;
        dsp::OnePoleLP keyLp;
        dsp::Noise noise;
        float hammerLen = 10.0f, velNow = 0.8f;
        float lastFreq = -1.0f, lastPedal = -1.0f, lastDecay = -1.0f;
        int   note = 60;
        bool  released = false;
    };

    //==========================================================================
    //  STRUCK BAR
    //  spec: 0 mallet, 1 material, 2 strike position, 3 resonator tube,
    //        4 motor rate (engine), 5 motor depth (engine)
    //==========================================================================
    class StruckBarModel final : public ModelBase
    {
    public:
        static constexpr int kModes = 6;

        void prepare (double sampleRate) override { sr = sampleRate; reset(); }

        void reset() override
        {
            for (auto& m : modes) m.reset();
            tube.reset();
            mallet = {};
            knock.reset();
        }

        bool isSustained() const override { return false; }
        Dynamics dynamics() const override { return Dynamics::Struck; }

        void start (const ModelIn& in, bool /*legato*/) override
        {
            note = in.note;
            released = false;
            noise.seed (0xBA12u + in.seed * 2654435761u + (std::uint32_t) in.note * 977u);
            configure (in);

            const float hard = dsp::clampv (in.spec[0] * 0.6f + in.hardness * 0.4f + in.bright * 0.25f, 0.0f, 1.0f);
            float ms = dsp::lerp (6.0f, 0.2f, std::pow (hard, 0.4f)) * dsp::lerp (1.15f, 0.8f, in.velocity);
            ms = std::min (ms, 0.7f * 1000.0f / std::max (20.0f, in.freq));     // small bars, short contact
            malletLen = (float) (ms * 0.001 * sr);
            mallet.trigger (malletLen, std::pow (in.velocity, 1.2f) * (0.4f + in.strength * 1.2f),
                            dsp::lerp (1.2f, 3.0f, hard), hard * hard * (0.3f + 0.7f * in.transient));
            clickNoise = in.noise * 0.3f * hard;

            // the mallet's clack on the bar (a hard mallet's far brighter), and the knock of
            // the bar on its cord
            {
                const float k = std::pow (in.velocity, 1.5f) * (0.2f + 0.8f * in.transient);
                const float hz[3]  = { 160.0f, 420.0f, 900.0f };
                const float t60[3] = { 0.04f, 0.025f, 0.015f };
                knock.set (hz, t60, sr);
                knock.strike (k * 0.12f, k * (0.02f + hard * hard * hard * 0.6f), dsp::lerp (1500.0f, 5000.0f, hard), sr);
            }

            // spectral weighting: a soft mallet barely reaches the upper modes
            const float tilt = dsp::lerp (2.2f, 0.6f, hard);
            for (int k = 0; k < kModes; ++k)
            {
                float a = std::pow (1.0f / ratio[k], tilt);
                if (k > 0) a *= dsp::lerp (0.35f, 1.25f, in.spec[2]);
                weight[k] = a;
            }
        }

        void release (const ModelIn& in) override
        {
            released = true;
            configure (in);
        }

        void render (const ModelIn& in, float* out, int n) override
        {
            if (std::abs (in.freq - lastFreq) > 0.01f || in.pedal != lastPedal || in.released != released
                || in.spec[1] != lastMat || in.decay != lastDecay || in.dampAdd != lastDampAdd)
            {
                released = in.released;
                configure (in);
            }

            const float hn  = 2.0f / std::max (2.0f, malletLen);
            const float tubeLv = in.spec[3];

            for (int i = 0; i < n; ++i)
            {
                float e = mallet.next() * hn;
                if (e != 0.0f) e *= 1.0f + clickNoise * noise.next();
                float sum = 0.0f;
                for (int k = 0; k < kModes; ++k)
                    if (modes[k].active) sum += modes[k].process (e * weight[k]);
                if (tubeLv > 0.001f)
                    sum += tube.process (sum) * tubeLv * 1.4f;     // resonator tube: a narrow lift at the fundamental
                sum += knock.next (noise);
                out[i] = sum * 0.42f;
            }
        }

    private:
        void configure (const ModelIn& in)
        {
            lastFreq = in.freq;  lastPedal = in.pedal;  lastMat = in.spec[1];  lastDecay = in.decay;  lastDampAdd = in.dampAdd;

            static const float sets[5][kModes] = {
                { 1.0f, 3.99f, 9.97f,  18.0f,  27.0f,  38.0f  },   // tuned wood (marimba)
                { 1.0f, 3.0f,  6.0f,   9.8f,   14.0f,  19.0f  },   // xylophone
                { 1.0f, 4.0f,  10.0f,  18.5f,  28.0f,  40.0f  },   // tuned metal (vibes)
                { 1.0f, 2.756f,5.404f, 8.933f, 13.34f, 18.64f },   // free bar (glock, celesta)
                { 1.0f, 6.27f, 17.55f, 34.39f, 56.84f, 84.91f }    // cantilever comb (music box)
            };
            static const float baseT60[5] = { 1.3f, 0.75f, 6.0f, 3.2f, 2.2f };
            static const float decExp[5]  = { 1.25f, 1.0f, 0.85f, 0.8f, 1.0f };

            const float m = dsp::clampv (in.spec[1], 0.0f, 1.0f) * 4.0f;
            const int   i0 = std::min (3, (int) m);
            const float fr = m - (float) i0;
            for (int k = 0; k < kModes; ++k)
                ratio[k] = dsp::lerp (sets[i0][k], sets[i0 + 1][k], fr);

            float t0 = std::exp (dsp::lerp (std::log (baseT60[i0]), std::log (baseT60[i0 + 1]), fr));
            const float de = dsp::lerp (decExp[i0], decExp[i0 + 1], fr);
            t0 *= std::pow (0.972f, (float) (in.note - 60)) * std::pow (2.0f, (in.decay - 0.5f) * 3.0f) * in.decayMul;
            t0  = dsp::clampv (t0, 0.08f, 30.0f) * (1.0f - 0.95f * in.dampAdd);

            const bool hasDamper = in.spec[1] >= 0.3f && in.spec[1] < 0.9f;
            if (released && hasDamper)
                t0 = std::exp (dsp::lerp (std::log (0.18f), std::log (t0), dsp::clampv (in.pedal, 0.0f, 1.0f)));

            const float dampHi = 1.0f + in.damping * 2.0f;
            for (int k = 0; k < kModes; ++k)
            {
                const float tk = t0 / std::pow (ratio[k], de * dampHi * 0.5f + 0.25f);
                modes[k].set (in.freq * ratio[k], std::max (0.01f, tk), sr);
            }
            tube.bandpass (in.freq, 24.0, sr);
        }

        dsp::Mode  modes[kModes];
        dsp::Biquad tube;
        float      ratio[kModes] = { 1, 4, 10, 18, 27, 38 };
        float      weight[kModes] = { 1, 0.3f, 0.1f, 0.05f, 0.02f, 0.01f };
        HammerForce mallet;
        StrikeKnock knock;
        dsp::Noise noise;
        float malletLen = 20.0f, clickNoise = 0.0f;
        float lastFreq = -1.0f, lastPedal = -1.0f, lastMat = -1.0f, lastDecay = -1.0f, lastDampAdd = -1.0f;
        int   note = 60;
        bool  released = false;
    };
}
