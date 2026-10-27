// C:\workspace\SONIFY PLUGIN natura\src\ModelsOrgan.h
#pragma once
//==============================================================================
//  ModelsOrgan.h  -  the three organ-like archetypes.
//
//    TONEWHEEL ORGAN  nine drawbar sines with contact bounce, key click,
//                     leakage, single-trigger percussion and top foldback.
//    PIPE ORGAN       five ranks (principal, stopped flute, reed, mixture,
//                     16' bourdon) from band-limited tables, each rank with
//                     its own speech time and pitch, chiff and wind.
//    FREE REED        accordion / harmonica / reed organ: up to four reeds per
//                     key from the register switch, musette detune, a
//                     mellow-to-bright reed tone and a cassotto chamber.
//==============================================================================
#include "ModelBase.h"
#include <array>
#include <map>
#include <memory>
#include <mutex>

namespace natura
{
    //==========================================================================
    //  Band-limited single-cycle tables, one mip per octave band.
    //==========================================================================
    struct WaveTables
    {
        static constexpr int kSize  = 2048;
        static constexpr int kBands = 10;                       // 16 Hz * 2^b

        enum Rank { Principal = 0, Flute, Reed, Mixture, Bourdon, ReedMellow, ReedBright, kNumRanks };

        std::vector<float> data;                                 // [rank][band][kSize + 1]

        const float* table (int rank, int band) const
        {
            return data.data() + ((size_t) rank * kBands + (size_t) band) * (kSize + 1);
        }

        static int bandFor (double hz)
        {
            const double b = std::log2 (std::max (16.0, hz) / 16.0);
            return std::min (kBands - 1, std::max (0, (int) b));
        }

        static float read (const float* t, double ph)
        {
            const double p = ph * kSize;
            const int    i = (int) p;
            const float  f = (float) (p - i);
            const int    j = i & (kSize - 1);
            return t[j] + f * (t[j + 1] - t[j]);
        }

        static double amplitude (int rank, int h)
        {
            switch (rank)
            {
                case Principal:  return std::pow ((double) h, -1.25) * ((h % 2 == 0) ? 1.12 : 1.0);
                case Flute:      return (h % 2 == 1) ? std::pow ((double) h, -2.1) : (h == 2 ? 0.04 : 0.0);
                case Reed:       return std::pow ((double) h, -0.65) * (1.0 + 0.6 * std::exp (-std::pow ((h - 6.0) / 3.0, 2.0)));
                case Mixture:    return (h == 2 || h == 3 || h == 4 || h == 6 || h == 8) ? 0.55 / std::sqrt ((double) h) : 0.0;
                case Bourdon:    return (h % 2 == 1) ? std::pow ((double) h, -2.4) : 0.0;
                case ReedMellow: return std::pow ((double) h, -1.6) * ((h % 2 == 1) ? 1.0 : 0.45);
                case ReedBright: return std::pow ((double) h, -0.8) * (1.0 + 0.5 * std::exp (-std::pow ((h - 4.0) / 2.0, 2.0)));
                default:         return 0.0;
            }
        }

        explicit WaveTables (double sr)
        {
            data.assign ((size_t) kNumRanks * kBands * (kSize + 1), 0.0f);
            std::vector<double> acc ((size_t) kSize);
            for (int r = 0; r < kNumRanks; ++r)
                for (int b = 0; b < kBands; ++b)
                {
                    const double topHz = 16.0 * std::pow (2.0, b + 1);
                    // the mixture rank plays its harmonics from the fundamental,
                    // the bourdon an octave down: both are read at the key's pitch
                    const int maxH = std::max (1, std::min (64, (int) (0.45 * sr / topHz)));
                    std::fill (acc.begin(), acc.end(), 0.0);
                    for (int h = 1; h <= maxH; ++h)
                    {
                        const double a = amplitude (r, h);
                        if (a <= 0.0) continue;
                        for (int i = 0; i < kSize; ++i)
                            acc[(size_t) i] += a * std::sin (dsp::kTwoPi * h * i / kSize);
                    }
                    double peak = 1.0e-9;
                    for (double v : acc) peak = std::max (peak, std::abs (v));
                    float* t = data.data() + ((size_t) r * kBands + (size_t) b) * (kSize + 1);
                    for (int i = 0; i < kSize; ++i) t[i] = (float) (acc[(size_t) i] / peak);
                    t[kSize] = t[0];
                }
        }

        /** Built once per sample rate and shared by every voice. */
        static std::shared_ptr<const WaveTables> get (double sr)
        {
            static std::mutex lock;
            static std::map<int, std::shared_ptr<const WaveTables>> cache;
            const std::lock_guard<std::mutex> g (lock);
            auto& slot = cache[(int) std::lround (sr)];
            if (! slot) slot = std::make_shared<WaveTables> (sr);
            return slot;
        }
    };

    inline float hashCents (int note, int salt, float range)
    {
        std::uint32_t h = (std::uint32_t) note * 2654435761u ^ (std::uint32_t) salt * 40503u;
        h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
        return ((float) (h & 0xFFFF) / 65535.0f * 2.0f - 1.0f) * range;
    }

    //==========================================================================
    //  TONEWHEEL ORGAN
    //  spec: 0..8 drawbars (16' 5 1/3' 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1'),
    //        9 click, 10 leakage, 11 percussion (0 off, .5 2nd, 1 3rd),
    //        12 perc decay, 13 perc volume, 14 vibrato/chorus (engine)
    //==========================================================================
    class TonewheelModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override { sr = sampleRate; reset(); }

        void reset() override
        {
            for (auto& p : ph) p = 0.0;
            percPh = 0.0;
            perc = {};
            click = {};
            clickBp.reset();
        }

        bool isSustained() const override { return true; }
        Dynamics dynamics() const override { return Dynamics::Swell; }

        void start (const ModelIn& in, bool legato) override
        {
            noise.seed (0x7E11u + in.seed * 2654435761u + (std::uint32_t) in.note * 31u);
            if (! legato)
            {
                // free-running wheels: any phase; the contacts close a little apart
                for (int k = 0; k < 9; ++k)
                {
                    ph[k] = noise.unit();
                    contactDelay[k] = (int) (noise.unit() * 0.0016 * sr * (0.3 + in.spec[9]));
                }
                percPh = 0.0;
                click.trigger ((0.04f + in.spec[9] * 0.22f) * (0.6f + 0.8f * in.transient), 0.006f, sr);
                clickBp.bandpass (3200.0, 0.9, sr);
            }

            const int pm = (int) std::lround (in.spec[11] * 2.0f);
            if (pm > 0 && in.percTrigger)
            {
                percHarm = pm == 1 ? 2.0 : 3.0;
                const float t60 = dsp::lerp (0.35f, 1.6f, in.spec[12]);
                perc.trigger (0.75f * in.spec[13], t60, sr);
            }
            else perc = {};
        }

        void release (const ModelIn&) override {}

        void render (const ModelIn& in, float* out, int n) override
        {
            static const double ratio[9] = { 0.5, 1.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0 };
            const auto& sine = dsp::SineTable::get();

            float  lv[9];
            double inc[9];
            const float leak = in.spec[10] * 0.025f;
            float total = 0.0f;
            for (int k = 0; k < 9; ++k)
            {
                const int steps = (int) std::lround (in.spec[k] * 8.0f);
                lv[k] = steps == 0 ? leak : std::pow (10.0f, (float) (steps - 8) * 3.0f / 20.0f);
                total += lv[k];
                double f = in.freq * ratio[k];
                while (f > 5900.0) f *= 0.5;                       // top foldback, like the real wheels
                inc[k] = f / sr;
            }
            const float norm = 0.55f / std::max (1.0f, std::sqrt (total * 1.6f));
            double pf = in.freq * percHarm;
            while (pf > 5900.0) pf *= 0.5;
            const double pinc = pf / sr;

            for (int i = 0; i < n; ++i)
            {
                float s = 0.0f;
                for (int k = 0; k < 9; ++k)
                {
                    if (contactDelay[k] > 0) { --contactDelay[k]; ph[k] += inc[k]; continue; }
                    // slight wheel shape: a touch of 3rd harmonic like a real tonewheel
                    const float v = sine.at (ph[k]);
                    s += lv[k] * (v - 0.035f * v * v * v);
                    ph[k] += inc[k];
                    if (ph[k] >= 1.0) ph[k] -= 1.0;
                }
                s *= norm;

                if (perc.running())
                {
                    s += perc.next() * sine.at (percPh) * 0.5f;
                    percPh += pinc;
                    if (percPh >= 1.0) percPh -= 1.0;
                }
                if (click.running())
                    s += clickBp.process (click.next() * noise.next());

                out[i] = s * in.pressure * 0.5f;
            }
        }

    private:
        double ph[9] = {}, percPh = 0.0, percHarm = 2.0;
        int    contactDelay[9] = {};
        dsp::Burst perc, click;
        dsp::Biquad clickBp;
        dsp::Noise noise;
    };

    //==========================================================================
    //  PIPE ORGAN
    //  spec: 0 principal 8', 1 flute 8', 2 reed 8', 3 mixture, 4 bourdon 16',
    //        5 chiff, 6 wind, 7 tremulant rate (engine), 8 tremulant depth (engine)
    //==========================================================================
    class PipeOrganModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            tables = WaveTables::get (sr);
            windBp.bandpass (1300.0, 0.5, sr);
            reset();
        }

        void reset() override
        {
            for (auto& p : ph) p = 0.0;
            for (auto& r : speech) r.setNow (0.0f);
            chiff = {};
            chiffBp.reset();
            windBp.reset();
        }

        bool isSustained() const override { return true; }
        Dynamics dynamics() const override { return Dynamics::Swell; }

        void start (const ModelIn& in, bool legato) override
        {
            noise.seed (0x0123u + in.seed * 2654435761u + (std::uint32_t) in.note * 17u);
            static const float speechMs[5] = { 45.0f, 70.0f, 18.0f, 30.0f, 95.0f };
            const float scale = dsp::lerp (1.6f, 0.4f, in.transient);
            for (int r = 0; r < 5; ++r)
            {
                detune[r] = std::pow (2.0f, hashCents (in.note, r + 1, 1.3f) / 1200.0f);
                if (! legato)
                {
                    ph[r] = noise.unit();
                    speech[r].setNow (0.0f);
                }
                speech[r].to (1.0f, (int) (speechMs[r] * scale * 0.001 * sr));
            }
            if (! legato)
            {
                const double cf = std::min (7500.0, (double) in.freq * 3.0);
                chiffBp.bandpass (cf, 3.0, sr);
                chiff.trigger (in.spec[5] * 0.5f * (0.4f + in.transient), 0.07f, sr);
            }
        }

        void release (const ModelIn&) override {}

        void render (const ModelIn& in, float* out, int n) override
        {
            static const double mult[5] = { 1.0, 1.0, 1.0, 1.0, 0.5 };
            const float lv[5] = { in.spec[0], in.spec[1] * 1.1f, in.spec[2] * 0.75f, in.spec[3] * 0.6f, in.spec[4] * 1.2f };
            float total = 0.0f;
            for (float v : lv) total += v;
            const float norm = 0.5f / std::max (0.8f, std::sqrt (total * 1.4f));

            const float* tab[5];
            double inc[5];
            for (int r = 0; r < 5; ++r)
            {
                const double f = (double) in.freq * mult[r] * detune[r];
                inc[r] = f / sr;
                tab[r] = tables->table (r, WaveTables::bandFor (f));
            }
            const float wind = in.spec[6] * 0.03f;

            for (int i = 0; i < n; ++i)
            {
                float s = 0.0f;
                for (int r = 0; r < 5; ++r)
                {
                    const float sp = speech[r].next();
                    if (lv[r] <= 0.0005f) { ph[r] += inc[r]; if (ph[r] >= 1.0) ph[r] -= 1.0; continue; }
                    s += lv[r] * sp * WaveTables::read (tab[r], ph[r]);
                    ph[r] += inc[r];
                    if (ph[r] >= 1.0) ph[r] -= 1.0;
                }
                s *= norm;
                if (chiff.running()) s += chiffBp.process (chiff.next() * noise.next());
                if (wind > 0.0f)     s += windBp.process (noise.next()) * wind;
                out[i] = s * in.pressure * 0.8f;
            }
        }

    private:
        std::shared_ptr<const WaveTables> tables;
        double ph[5] = {};
        float  detune[5] = { 1, 1, 1, 1, 1 };
        dsp::Ramp   speech[5];
        dsp::Burst  chiff;
        dsp::Biquad chiffBp, windBp;
        dsp::Noise  noise;
    };

    //==========================================================================
    //  FREE REED
    //  spec: 0 bellows, 1 register, 2 musette, 3 reed tone, 4 chamber
    //==========================================================================
    class FreeReedModel final : public ModelBase
    {
    public:
        static constexpr int kRegisters = 10;

        static const char* registerName (int r)
        {
            static const char* names[kRegisters] = { "BASSOON", "CLARINET", "PICCOLO", "BANDONEON", "VIOLIN",
                                                     "MUSETTE", "OBOE", "ORGAN", "ACCORDION", "MASTER" };
            return names[std::min (kRegisters - 1, std::max (0, r))];
        }

        void prepare (double sampleRate) override
        {
            sr = sampleRate;
            tables = WaveTables::get (sr);
            airBp.bandpass (3000.0, 0.7, sr);
            reset();
        }

        void reset() override
        {
            for (auto& p : ph) p = 0.0;
            chamber.reset();
            airBp.reset();
            speak.setNow (0.0f);
        }

        bool isSustained() const override { return true; }
        Dynamics dynamics() const override { return Dynamics::Direct; }

        void start (const ModelIn& in, bool legato) override
        {
            noise.seed (0xACC0u + in.seed * 2654435761u + (std::uint32_t) in.note * 101u);
            if (! legato)
                for (int k = 0; k < 4; ++k) ph[k] = noise.unit();
            // the reed speaks slightly flat and settles
            speak.setNow (-(1.0f - in.transient) * 9.0f);
            speak.to (0.0f, (int) (0.045 * sr));
        }

        void release (const ModelIn&) override {}

        void render (const ModelIn& in, float* out, int n) override
        {
            // register -> reeds: { octave, musette sign }
            struct Reed { int oct; int sign; };
            static const Reed regs[kRegisters][4] = {
                { { -1, 0 }, { 9, 0 }, { 9, 0 }, { 9, 0 } },          // L
                { {  0, 0 }, { 9, 0 }, { 9, 0 }, { 9, 0 } },          // M
                { {  1, 0 }, { 9, 0 }, { 9, 0 }, { 9, 0 } },          // H
                { { -1, 0 }, { 0, 0 }, { 9, 0 }, { 9, 0 } },          // L M
                { {  0, 0 }, { 0, 1 }, { 9, 0 }, { 9, 0 } },          // M M+
                { {  0,-1 }, { 0, 0 }, { 0, 1 }, { 9, 0 } },          // M- M M+
                { {  0, 0 }, { 1, 0 }, { 9, 0 }, { 9, 0 } },          // M H
                { { -1, 0 }, { 1, 0 }, { 9, 0 }, { 9, 0 } },          // L H
                { { -1, 0 }, { 0, 0 }, { 1, 0 }, { 9, 0 } },          // L M H
                { { -1, 0 }, { 0,-1 }, { 0, 1 }, { 1, 0 } } };        // L M- M+ H

            const int reg = std::min (kRegisters - 1, std::max (0, (int) std::lround (in.spec[1] * (kRegisters - 1)) + in.stateOffset));
            const float musette = in.spec[2] * 40.0f;
            const float morph = dsp::clampv (in.spec[3] + 0.35f * (in.pressure - 0.6f) + in.bright * 0.3f, 0.0f, 1.0f);
            const float amp = std::pow (std::max (0.0f, in.pressure), dsp::lerp (0.5f, 1.6f, in.spec[0]));
            chamber.setCutoff (dsp::lerp (12000.0f, 1600.0f, in.spec[4]), sr);
            const float air = in.noise * 0.05f;

            int count = 0;
            double inc[4];
            const float* mel[4];
            const float* bri[4];
            for (int k = 0; k < 4; ++k)
            {
                const Reed r = regs[reg][k];
                if (r.oct == 9) break;
                const float cents = (float) r.sign * musette + hashCents (in.note, k + 7, 1.5f);
                const double f = (double) in.freq * std::pow (2.0, (double) r.oct + cents / 1200.0);
                inc[k] = f / sr;
                const int band = WaveTables::bandFor (f);
                mel[k] = tables->table (WaveTables::ReedMellow, band);
                bri[k] = tables->table (WaveTables::ReedBright, band);
                ++count;
            }
            const float norm = 0.42f / std::sqrt ((float) std::max (1, count));

            for (int i = 0; i < n; ++i)
            {
                const double bend = std::pow (2.0, speak.next() / 1200.0);
                float s = 0.0f;
                for (int k = 0; k < count; ++k)
                {
                    s += dsp::lerp (WaveTables::read (mel[k], ph[k]), WaveTables::read (bri[k], ph[k]), morph);
                    ph[k] += inc[k] * bend;
                    if (ph[k] >= 1.0) ph[k] -= 1.0;
                }
                s = chamber.process (s * norm);
                if (air > 0.0f) s += airBp.process (noise.next()) * air;
                out[i] = s * amp;
            }
        }

    private:
        std::shared_ptr<const WaveTables> tables;
        double ph[4] = {};
        dsp::OnePoleLP chamber;
        dsp::Biquad airBp;
        dsp::Ramp   speak;
        dsp::Noise  noise;
    };
}
