// C:\workspace\SONIFY PLUGIN natura\src\ModelsWind.h
#pragma once
//==============================================================================
//  ModelsWind.h  -  the four blown archetypes, all self-oscillating waveguides.
//
//    LIP REED     brass: a resonant lip valve against a bore with a bell
//                 filter; lip tension, growl, buzz, bell and mutes.
//    SINGLE REED  clarinet <-> saxophone: a beating reed on a cylindrical
//                 bore, a conical bore, and a morph between the two.
//    DOUBLE REED  oboe / English horn / bassoon: a stiffer reed on a conical
//                 bore with the nasal formants of the double reed family.
//    AIR JET      flute / recorder / pan flute / ocarina: a jet that drives
//                 the bore through the cubic jet curve; breath, jet length,
//                 flutter and overblowing.
//
//  These follow the published waveguide topologies of the Synthesis ToolKit
//  family of instruments, written from scratch for Natura.
//==============================================================================
#include "ModelBase.h"

namespace natura
{
    inline float reedTable (float x, float offset, float slope)
    {
        return dsp::clampv (offset + slope * x, -1.0f, 1.0f);
    }

    /** OneZero averager (zero at -1): phase delay is exactly half a sample. */
    struct Averager
    {
        float x1 = 0.0f;
        void  reset() { x1 = 0.0f; }
        float process (float x) { const float y = 0.5f * (x + x1); x1 = x; return y; }
    };

    //==========================================================================
    //  LIP REED (brass)
    //  The lips are a pressure-controlled valve: each cycle they open for a
    //  fraction of the period, and the harder the player blows the shorter and
    //  sharper that opening becomes - which is why brass brightens so much
    //  with level.  The puffs fill the bore (a resonant comb with a bell
    //  filter), radiate through the bell and, at fortissimo, steepen into the
    //  "cuivre" edge.  Pitch is held by the lips, so every note speaks in tune
    //  and slurs glide like a real lip slur.
    //  spec: 0 lip tension, 1 growl, 2 bell, 3 mute, 4 wah, 5 bore, 6 buzz
    //==========================================================================
    class LipReedModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            bore.prepare ((int) (sr / 20.0) + 64);
            reset();
        }

        void reset() override
        {
            bore.clear();
            ph = 0.0;
            bellLp.reset();
            radHp.reset();
            dc.prepare (sr, 20.0);
            dc.reset();
            for (auto& f : muteF) f.reset();
            growlLp.reset();
            speak.setNow (0.0f);
            buzz = {};
            buzzBp.reset();
            lastMute = -1;
        }

        bool isSustained() const override { return true; }
        Dynamics dynamics() const override { return Dynamics::Physical; }

        void start (const ModelIn& in, bool legato) override
        {
            noise.seed (0xB4A55u + in.seed * 2654435761u + (std::uint32_t) in.note * 313u);
            growlLp.setCutoff (85.0, sr);
            if (! legato)
            {
                bore.clear();
                ph = 0.0;
                // the lips find the note from slightly below; tight lips find it at once
                speak.setNow (-(1.0f - in.spec[0]) * 70.0f * (1.0f - in.transient * 0.6f));
                speak.to (0.0f, (int) (dsp::lerp (0.06f, 0.015f, in.spec[0]) * sr));
                const float b = (0.02f + 0.06f * in.transient) * (0.3f + in.noise * 1.4f);
                buzz.trigger (b, 0.04f, sr);
                buzzBp.bandpass (std::min (6000.0, (double) in.freq * 6.0), 1.2, sr);
            }
            configure (in);
        }

        void release (const ModelIn&) override {}

        void render (const ModelIn& in, float* out, int n) override
        {
            if (std::abs (in.freq - lastFreq) > 0.01f || in.spec[2] != lastBell || in.spec[5] != lastBore || in.damping != lastDamp)
                configure (in);

            const int muteNow = in.mute != Mute::Open ? (int) in.mute
                                                      : (int) std::lround (in.spec[3] * 4.0f);
            if (muteNow != lastMute || (muteNow == (int) Mute::Wah && std::abs (in.wah - lastWah) > 0.002f))
                setMute (muteNow, in.wah);

            const float growl = dsp::clampv (in.spec[1] * 0.6f + in.growl, 0.0f, 1.2f);
            const float tension = in.spec[0];
            const float edge = 0.4f + 2.6f * in.spec[6] + 0.8f * in.hardness;   // the fortissimo "cuivre"
            const float dynP = 0.25f + 0.75f * in.dyn;
            const float gB = 0.62f + 0.2f * in.decay;                          // how long the bore rings

            for (int i = 0; i < n; ++i)
            {
                // breath and the lip's opening fraction for this cycle
                float p = in.pressure * dynP * (0.8f + 0.4f * in.strength);
                float jitter = 0.0f;
                if (growl > 0.0f)
                {
                    const float g = growlLp.process (noise.next());
                    p *= 1.0f + growl * 4.0f * g;
                    jitter = growl * 2.5f * g;
                }
                p = std::max (0.0f, p);
                const float oq = dsp::clampv (dsp::lerp (0.85f, 0.28f, std::pow (std::min (1.2f, p), 1.1f))
                                              - (tension - 0.5f) * 0.12f, 0.18f, 0.92f);

                const double hz = (double) in.freq * std::pow (2.0, (speak.next() + jitter * 30.0f) / 1200.0);
                ph += hz / sr;
                if (ph >= 1.0) ph -= 1.0;
                float open = 0.0f;
                if (ph < (double) oq)
                    open = 0.5f * (1.0f - (float) std::cos (dsp::kTwoPi * ph / oq));
                float u = open * std::sqrt (p) * (1.0f / std::sqrt (oq));

                // the bore: a resonant comb whose losses are the bell
                const float fb = bellLp.process (bore.tap (dBore));
                const float y  = u + gB * fb;
                bore.write (y);

                // radiation (the bell passes the highs) and the brassy edge
                float r = radHp.process (dc.process (y)) * 0.22f * regGain;
                const float k = 0.3f + edge * p * p;        // small signals pass, loud ones steepen
                r = std::tanh (r * k) / k;
                if (buzz.running()) r += buzzBp.process (buzz.next() * noise.next());
                r += r * in.noise * 0.02f * noise.next();

                float sOut = r;
                if (muteNow == (int) Mute::Straight)    sOut = muteF[2].process (muteF[1].process (muteF[0].process (sOut)));
                else if (muteNow == (int) Mute::Cup)    sOut = muteF[1].process (muteF[0].process (sOut)) * 1.6f;
                else if (muteNow == (int) Mute::Harmon) sOut = muteF[2].process (muteF[1].process (muteF[0].process (sOut))) * 0.9f;
                else if (muteNow == (int) Mute::Wah)    sOut = muteF[0].process (sOut) * 2.0f;
                out[i] = sOut * 0.75f;
            }
        }

    private:
        void configure (const ModelIn& in)
        {
            lastFreq = in.freq;  lastBell = in.spec[2];  lastBore = in.spec[5];  lastDamp = in.damping;
            const float f0 = dsp::clampv (in.freq, 25.0f, (float) sr * 0.2f);
            // bell: a bright bell loses little; a big bore (horn, tuba) is darker
            const float dark = dsp::clampv (0.6f - 0.5f * in.spec[2] + 0.3f * in.spec[5] + 0.3f * (in.damping - 0.5f), 0.02f, 0.9f);
            bellLp.a = dark;
            const double w = dsp::kTwoPi * f0 / sr;
            dBore = std::max (2.0f, (float) (sr / f0) - dsp::OnePoleLP::phaseDelay (dark, w));
            radHp.setCutoff (dsp::lerp (420.0f, 90.0f, in.spec[5]), sr);
            regGain = 1.0f + dsp::clampv ((float) (64 - in.note) / 28.0f, 0.0f, 1.2f);
        }

        void setMute (int m, float wah)
        {
            lastMute = m;  lastWah = wah;
            for (auto& f : muteF) f.reset();
            switch (m)
            {
                case (int) Mute::Straight: muteF[0].highpass (600.0, 0.7, sr); muteF[1].peak (1800.0, 1.2, 6.0, sr); muteF[2].lowpass (7000.0, 0.7, sr); break;
                case (int) Mute::Cup:      muteF[0].bandpass (900.0, 0.9, sr); muteF[1].lowpass (3500.0, 0.7, sr); break;
                case (int) Mute::Harmon:   muteF[0].highpass (900.0, 0.7, sr); muteF[1].peak (1600.0, 5.0, 12.0, sr); muteF[2].lowpass (5000.0, 0.7, sr); break;
                case (int) Mute::Wah:      muteF[0].bandpass (dsp::lerp (450.0f, 2400.0f, wah), 2.5, sr); break;
                default: break;
            }
        }

        dsp::FracDelay bore;
        dsp::OnePoleLP bellLp, growlLp;
        dsp::OnePoleHP radHp;
        dsp::DCBlocker dc;
        dsp::Biquad    muteF[3], buzzBp;
        dsp::Burst     buzz;
        dsp::Ramp      speak;
        dsp::Noise     noise;
        double ph = 0.0;
        float dBore = 100.0f, regGain = 1.0f;
        float lastFreq = -1.0f, lastBell = -1.0f, lastBore = -1.0f, lastDamp = -1.0f, lastWah = -1.0f;
        int   lastMute = -1;
    };

    //==========================================================================
    //  SINGLE REED (clarinet <-> saxophone)
    //  spec: 0 reed stiffness, 1 bite, 2 bore (cylindrical..conical),
    //        3 subtone, 4 key click
    //==========================================================================
    class SingleReedModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            const int maxLen = (int) (sr / 20.0) + 64;
            cyl.prepare (maxLen);
            con0.prepare (maxLen);
            con1.prepare (maxLen);
            reset();
        }

        void reset() override
        {
            cyl.clear(); con0.clear(); con1.clear();
            avC.reset(); avS.reset();
            tone.reset();
            click = {};
            clickBp.reset();
        }

        bool isSustained() const override { return true; }
        Dynamics dynamics() const override { return Dynamics::Physical; }

        void start (const ModelIn& in, bool legato) override
        {
            noise.seed (0x5A11u + in.seed * 2654435761u + (std::uint32_t) in.note * 211u);
            if (! legato) { cyl.clear(); con0.clear(); con1.clear(); avC.reset(); avS.reset(); }
            configure (in);
            const float k = in.spec[4] * (legato ? 0.07f : 0.03f);
            if (k > 0.0005f) { click.trigger (k, 0.012f, sr); clickBp.bandpass (1900.0, 1.5, sr); }
        }

        void release (const ModelIn&) override {}

        void render (const ModelIn& in, float* out, int n) override
        {
            if (std::abs (in.freq - lastFreq) > 0.01f || in.spec[2] != lastBore)
                configure (in);

            const float bore  = in.spec[2];
            const bool  runC  = bore < 0.98f;
            const bool  runS  = bore > 0.02f;
            const float stiff = in.spec[0], bite = in.spec[1], sub = in.spec[3];

            const float offC  = 0.7f + (bite - 0.5f) * 0.08f;
            const float slC   = -0.38f + stiff * 0.16f;
            const float offS  = 0.7f + (bite - 0.5f) * 0.08f;
            const float slS   = 0.24f + stiff * 0.16f;
            const float nGain = 0.12f + in.noise * 0.25f + sub * 0.35f;
            const float reg   = dsp::clampv (1.0f - (float) (in.note - 60) * 0.0035f, 0.85f, 1.1f);
            const float maxP  = (0.78f + 0.3f * in.strength) * (1.0f - 0.12f * sub) * reg;
            tone.lowpass (dsp::lerp (9000.0f, 1700.0f, sub) * (1.0f + 0.3f * in.bright), 0.7, sr);

            // each bore speaks inside its own pressure window: the clarinet's is
            // wide, the cone's narrow (it overblows to the octave above it)
            const float winC = 0.72f + 0.28f * in.dyn;
            const float winS = 0.82f + 0.1f * in.dyn;

            for (int i = 0; i < n; ++i)
            {
                const float base = in.pressure * maxP;
                const float nz   = nGain * noise.next();

                float sC = 0.0f, sS = 0.0f;
                if (runC)
                {
                    float breath = base * winC;
                    breath += breath * nz;
                    float pd = -0.95f * avC.process (cyl.tap (dCyl));
                    pd -= breath;
                    const float o = breath + pd * reedTable (pd, offC, slC);
                    cyl.write (o);
                    sC = o;
                }
                if (runS)
                {
                    float breath = base * winS;
                    breath += breath * nz;
                    const float temp = -0.95f * avS.process (con0.tap (d0));
                    const float o = temp - con1.tap (d1);
                    const float pd = breath - o;
                    con1.write (temp);
                    con0.write (breath - pd * reedTable (pd, offS, slS) - temp);
                    sS = o * 0.3f * 2.4f;
                }

                float s = dsp::lerp (sC, sS, bore);
                if (click.running()) s += clickBp.process (click.next() * noise.next());
                out[i] = tone.process (s) * 0.32f;
            }
        }

    private:
        void configure (const ModelIn& in)
        {
            lastFreq = in.freq;  lastBore = in.spec[2];
            const float f0 = dsp::clampv (in.freq, 25.0f, (float) sr * 0.2f);
            dCyl = std::max (2.0f, (float) (sr / f0) * 0.5f - 0.5f);
            const float dTot = std::max (4.0f, (float) (sr / f0) - 0.5f);
            const float pos  = 0.2f;
            d0 = std::max (2.0f, (1.0f - pos) * dTot);
            d1 = std::max (2.0f, pos * dTot);
        }

        dsp::FracDelay cyl, con0, con1;
        Averager avC, avS;
        dsp::Biquad tone, clickBp;
        dsp::Burst click;
        dsp::Noise noise;
        float dCyl = 50.0f, d0 = 80.0f, d1 = 20.0f;
        float lastFreq = -1.0f, lastBore = -1.0f;
    };

    //==========================================================================
    //  DOUBLE REED (oboe / English horn / bassoon)
    //  spec: 0 reed stiffness, 1 bite, 2 nasal, 3 bore (oboe..bassoon),
    //        4 key click
    //==========================================================================
    class DoubleReedModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            const int maxLen = (int) (sr / 20.0) + 64;
            con0.prepare (maxLen);
            con1.prepare (maxLen);
            reset();
        }

        void reset() override
        {
            con0.clear(); con1.clear();
            av.reset();
            f1.reset(); f2.reset(); hp.reset();
            click = {};
            clickBp.reset();
        }

        bool isSustained() const override { return true; }
        Dynamics dynamics() const override { return Dynamics::Physical; }

        void start (const ModelIn& in, bool legato) override
        {
            noise.seed (0xD0B1u + in.seed * 2654435761u + (std::uint32_t) in.note * 157u);
            if (! legato) { con0.clear(); con1.clear(); av.reset(); }
            configure (in);
            const float k = in.spec[4] * (legato ? 0.06f : 0.025f);
            if (k > 0.0005f) { click.trigger (k, 0.01f, sr); clickBp.bandpass (2100.0, 1.5, sr); }
        }

        void release (const ModelIn&) override {}

        void render (const ModelIn& in, float* out, int n) override
        {
            if (std::abs (in.freq - lastFreq) > 0.01f || in.spec[2] != lastNasal || in.spec[3] != lastBore)
                configure (in);

            const float off   = 0.7f + (in.spec[1] - 0.5f) * 0.08f;
            const float sl    = 0.24f + in.spec[0] * 0.16f;
            const float nGain = 0.08f + in.noise * 0.2f;
            const float reg   = dsp::clampv (1.0f - (float) (in.note - 60) * 0.0035f, 0.84f, 1.1f);  // high notes overblow easily
            const float maxP  = (0.78f + 0.3f * in.strength) * reg;

            const float win = 0.82f + 0.1f * in.dyn;

            for (int i = 0; i < n; ++i)
            {
                float breath = in.pressure * maxP * win;
                breath += breath * nGain * noise.next();

                const float temp = -0.95f * av.process (con0.tap (d0));
                const float o = temp - con1.tap (d1);
                const float pd = breath - o;
                con1.write (temp);
                con0.write (breath - pd * reedTable (pd, off, sl) - temp);

                float s = hp.process (f2.process (f1.process (o * 0.24f)));
                if (click.running()) s += clickBp.process (click.next() * noise.next());
                out[i] = s;
            }
        }

    private:
        void configure (const ModelIn& in)
        {
            lastFreq = in.freq;  lastNasal = in.spec[2];  lastBore = in.spec[3];
            const float f0 = dsp::clampv (in.freq, 25.0f, (float) sr * 0.2f);
            const float dTot = std::max (4.0f, (float) (sr / f0) - 0.5f);
            const float pos  = 0.2f;
            d0 = std::max (2.0f, (1.0f - pos) * dTot);
            d1 = std::max (2.0f, pos * dTot);

            // the double-reed formants: oboe high and narrow, bassoon low and round
            const float b = in.spec[3], nas = in.spec[2];
            f1.peak (dsp::lerp (1400.0f, 480.0f, b), 2.2, 9.0f * nas, sr);
            f2.peak (dsp::lerp (2900.0f, 1150.0f, b), 2.0, 5.0f * nas, sr);
            hp.highpass (dsp::lerp (220.0f, 45.0f, b), 0.7, sr);
        }

        dsp::FracDelay con0, con1;
        Averager av;
        dsp::Biquad f1, f2, hp, clickBp;
        dsp::Burst click;
        dsp::Noise noise;
        float d0 = 80.0f, d1 = 20.0f;
        float lastFreq = -1.0f, lastNasal = -1.0f, lastBore = -1.0f;
    };

    //==========================================================================
    //  AIR JET (flute family)
    //  spec: 0 breath noise, 1 jet length, 2 flutter, 3 overblow, 4 pipe (open..stopped)
    //==========================================================================
    class AirJetModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            const int maxLen = (int) (sr / 15.0) + 64;
            boreD.prepare (maxLen);
            jetD.prepare (maxLen);
            reset();
        }

        void reset() override
        {
            boreD.clear(); jetD.clear();
            fy = 0.0f;
            dc.prepare (sr, 20.0);
            dc.reset();
            tone.reset();
            flutLp.reset();
            chiff = {};
            chiffBp.reset();
        }

        bool isSustained() const override { return true; }
        Dynamics dynamics() const override { return Dynamics::Physical; }

        void start (const ModelIn& in, bool legato) override
        {
            noise.seed (0xF1u + in.seed * 2654435761u + (std::uint32_t) in.note * 53u);
            if (! legato) { boreD.clear(); jetD.clear(); fy = 0.0f; }
            configure (in);
            flutLp.setCutoff (35.0, sr);
            if (! legato && in.transient > 0.01f)
            {
                chiffBp.bandpass (std::min (8000.0, (double) in.freq * 4.0), 2.0, sr);
                chiff.trigger (in.transient * 0.06f, 0.05f, sr);
            }
        }

        void release (const ModelIn&) override {}

        void render (const ModelIn& in, float* out, int n) override
        {
            if (std::abs (in.freq - lastFreq) > 0.01f || in.spec[1] != lastJet || in.spec[3] != lastOver
                || in.spec[4] != lastPipe || in.damping != lastDamp)
                configure (in);

            const float maxP   = (0.85f + 0.3f * in.dyn) * (0.9f + 0.2f * in.strength);
            const float nGain  = 0.04f + 0.3f * in.spec[0] + 0.08f * in.noise;
            const float flut   = dsp::clampv (in.spec[2] * 0.6f + in.growl, 0.0f, 1.2f);
            const float endR   = dsp::lerp (0.7f, 0.8f, in.spec[4]);
            const float jetR   = 0.5f;
            tone.lowpass (dsp::lerp (12000.0f, 2500.0f, in.spec[4]) * (1.0f + 0.3f * in.bright), 0.7, sr);

            for (int i = 0; i < n; ++i)
            {
                float breath = in.pressure * maxP;
                float mod = nGain * noise.next();
                if (flut > 0.0f)
                {
                    flutPh += 24.0 / sr;
                    if (flutPh >= 1.0) flutPh -= 1.0;
                    mod += flut * (flutPh < 0.5 ? 0.35f : -0.35f) + flut * 2.0f * flutLp.process (noise.next());
                }
                breath += breath * mod;

                // reflection from the far end through the bore losses
                fy = fB * boreD.tap (dBore) + fP * fy;
                const float temp = dc.process (fy);           // = -(-1 * filtered)
                float pd = breath - jetR * temp;
                jetD.write (pd);
                pd = jetD.tap (dJet);
                float jt = pd * (pd * pd - 1.0f);
                jt = dsp::clampv (jt, -1.0f, 1.0f);
                pd = jt + endR * temp;
                boreD.write (pd);

                float s = 0.3f * pd;
                if (chiff.running()) s += chiffBp.process (chiff.next() * noise.next());
                out[i] = tone.process (s) * 0.33f;
            }
        }

    private:
        void configure (const ModelIn& in)
        {
            lastFreq = in.freq;  lastJet = in.spec[1];  lastOver = in.spec[3];  lastPipe = in.spec[4];  lastDamp = in.damping;
            const float f0 = dsp::clampv (in.freq, 40.0f, (float) sr * 0.15f);
            const float fl = f0;

            fP = dsp::clampv (0.7f - 0.1f * 22050.0f / (float) sr + (in.damping - 0.5f) * 0.25f + in.spec[4] * 0.1f, 0.05f, 0.95f);
            fB = 1.0f - fP;
            const double w = dsp::kTwoPi * fl / sr;
            const float lpD = dsp::OnePoleLP::phaseDelay (fP, w);
            dBore = std::max (4.0f, (float) (sr / fl) - lpD + 1.0f);

            // the jet's travel time decides which bore mode it feeds: around half the
            // bore is the fundamental, overblowing shortens it towards the octave
            float ratio = dsp::lerp (0.48f, 0.53f, in.spec[1]);
            ratio *= 1.0f - 0.48f * in.spec[3];
            dJet = std::max (2.0f, dBore * ratio);
        }

        dsp::FracDelay boreD, jetD;
        dsp::DCBlocker dc;
        dsp::Biquad tone, chiffBp;
        dsp::OnePoleLP flutLp;
        dsp::Burst chiff;
        dsp::Noise noise;
        double flutPh = 0.0;
        float fy = 0.0f, fP = 0.6f, fB = 0.4f, dBore = 100.0f, dJet = 30.0f;
        float lastFreq = -1.0f, lastJet = -1.0f, lastOver = -1.0f, lastPipe = -1.0f, lastDamp = -1.0f;
    };
}
