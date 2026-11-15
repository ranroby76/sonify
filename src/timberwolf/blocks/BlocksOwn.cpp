// D:\workspace\Fanan_web_hub\src\timberwolf\blocks\BlocksOwn.cpp
//==============================================================================
//  Timber Wolf's own effects, written from well-known designs: the fuzz, the
//  volume pedal and auto swell, tremolo, rotary speaker, vibrato and chorus,
//  the pitch family (shifter, whammy, harmonizer, doubler), tape echo,
//  ping-pong and reverse delays, the shimmer reverb, and Volume / Pan.
//==============================================================================
#include "BlockKit.h"
#include "../TwRegistry.h"

namespace tw
{
    namespace
    {
        const juce::String ownCredit = "Timber Wolf (Fanan)";

        //======================================================================
        /** A pitch shifter: two taps sliding along a delay line half a window apart, each faded
            in and out (sin^2, so they always add to one) - the classic delay-line shifter. */
        struct Shifter
        {
            kit::Delay line;
            double phase = 0.0;
            float window = 2048.0f;

            void prepare (double sr, double windowMs)
            {
                window = (float) juce::jmax (64.0, windowMs * 0.001 * sr);
                line.prepare ((int) window + 8);
                phase = 0.0;
            }

            void reset()   { line.reset(); phase = 0.0; }

            float process (float x, double ratio) noexcept
            {
                line.push (x);
                phase += (1.0 - ratio) / window;
                phase -= std::floor (phase);

                const double pb = phase + 0.5 - std::floor (phase + 0.5);
                const float ga = (float) juce::square (std::sin (juce::MathConstants<double>::pi * phase));
                const float gb = 1.0f - ga;
                return line.read (1.0f + (float) phase * window) * ga + line.read (1.0f + (float) pb * window) * gb;
            }
        };

        inline double semitonesToRatio (double st)   { return std::pow (2.0, st / 12.0); }

        //======================================================================
        //  Fuzz: a two-transistor fuzz's character - a biased, starved clip, oversampled.
        //======================================================================
        class Fuzz final : public Block
        {
        public:
            void prepare (double sr, int maxBlock) override
            {
                rate = sr;
                os.prepare (2, maxBlock);
                for (auto& d : dc) d.prepare (sr);
                for (auto& d : dcIn) d.prepare (sr * 4.0);
                for (auto& f : tone) f.reset();
                lastTone = -1.0f;
            }

            void reset() override
            {
                os.reset();
                for (auto& d : dc) d.reset();
                for (auto& d : dcIn) d.reset();
                for (auto& f : tone) f.reset();
            }

            int latency() const override    { return os.latency(); }

            void process (float* l, float* r, int n) override
            {
                const float drive = dbToGain (juce::jmap (knob[0], 0.0f, 100.0f, 6.0f, 46.0f));
                const float bias = knob[2] * 0.01f * 0.6f;
                const float octave = knob[3] * 0.01f;
                const float level = dbToGain (knob[4]);

                if (knob[1] != lastTone)
                {
                    lastTone = knob[1];
                    const double hz = 500.0 * std::pow (16.0, knob[1] * 0.01);
                    for (auto& f : tone) f.set (kit::Biquad::Type::lowpass, rate, hz, 0.6);
                }

                os.process (l, r, n, [&] (float& a, float& b)
                {
                    a = stage (dcIn[0].run (a), drive, bias, octave);
                    b = stage (dcIn[1].run (b), drive, bias, octave);
                });

                for (int i = 0; i < n; ++i)
                {
                    l[i] = tone[0].run (dc[0].run (l[i])) * level;
                    r[i] = tone[1].run (dc[1].run (r[i])) * level;
                }
            }

        private:
            static float stage (float x, float drive, float bias, float octave) noexcept
            {
                // An octave: the wave rectified (the Octavia's trick), blended in before the clip.
                const float y = x * (1.0f - octave) + std::abs (x) * octave * 1.6f;
                const float v = y * drive + bias;
                // Asymmetric: the bottom softer and later than the top, as a starved transistor.
                const float clipped = v > 0.0f ? std::tanh (v) : std::tanh (v * 0.6f) * 0.85f;
                return clipped - std::tanh (bias) * 0.9f;
            }

            kit::Oversampled os;
            kit::DcBlock dc[2], dcIn[2];
            kit::Biquad tone[2];
            double rate = 48000.0;
            float lastTone = -1.0f;
        };

        //======================================================================
        class VolumePedal final : public Block
        {
        public:
            void prepare (double sr, int) override  { for (auto& s : smooth) s.prepare (sr, 8.0f, gainFor()); }

            void process (float* l, float* r, int n) override
            {
                const float target = gainFor();
                for (int i = 0; i < n; ++i)
                {
                    const float g = smooth[0].next (target);
                    l[i] *= g;
                    r[i] *= g;
                }
            }

        private:
            float gainFor() const
            {
                const float p = knob[0] * 0.01f;
                const float minDb = knob[1];

                const float floor = minDb <= -60.0f ? 0.0f : dbToGain (minDb);

                // Linear: the level itself follows the pedal. Log (as a volume pot): the pedal
                // moves evenly through the decibels, from the minimum up.
                if (knob[2] > 0.5f)
                    return floor + (1.0f - floor) * p;

                if (p <= 0.0f)
                    return floor;

                const float bottom = minDb <= -60.0f ? -60.0f : minDb;
                return juce::jmax (floor, dbToGain (bottom * (1.0f - p)) * (p < 0.02f && floor == 0.0f ? p / 0.02f : 1.0f));
            }

            Smooth smooth[1];
        };

        //======================================================================
        //  Auto Swell: every new note fades in, as a volume pedal rolled up after the pick.
        //======================================================================
        class AutoSwell final : public Block
        {
        public:
            void prepare (double sr, int) override  { rate = sr; follower.set (sr, 1.0, 60.0); reset(); }
            void reset() override                   { follower.reset(); gain = 0.0f; armed = true; }

            void process (float* l, float* r, int n) override
            {
                const float threshold = dbToGain (knob[0]);
                const float rise = 1.0f / (float) juce::jmax (1.0, knob[1] * 0.001 * rate);
                const float level = dbToGain (knob[2]);

                for (int i = 0; i < n; ++i)
                {
                    const float e = follower.run (0.5f * (l[i] + r[i]));

                    // A new note: the level jumps up past the threshold after having fallen under
                    // half of it.
                    if (e < threshold * 0.5f)
                        armed = true;

                    if (armed && e > threshold)
                    {
                        armed = false;
                        gain = 0.0f;
                    }

                    gain = juce::jmin (1.0f, gain + rise);
                    const float g = gain * gain * level;
                    l[i] *= g;
                    r[i] *= g;
                }
            }

        private:
            kit::Follower follower;
            double rate = 48000.0;
            float gain = 0.0f;
            bool armed = true;
        };

        //======================================================================
        class Tremolo final : public Block
        {
        public:
            void prepare (double sr, int) override  { rate = sr; for (auto& f : lowpass) f.lowpass (sr, 750.0); reset(); }
            void reset() override                   { phase = 0.0; for (auto& f : lowpass) f.reset(); }

            void process (float* l, float* r, int n) override
            {
                const double hz = knob[1] > 0.5f ? 1000.0 / kit::syncedMs (1000.0 / knob[0], (int) knob[1], bpm()) : knob[0];
                const float depth = knob[2] * 0.01f;
                const int shape = (int) knob[3];
                const double offset = knob[4] / 360.0;
                const bool harmonic = knob[5] > 0.5f;
                const float level = dbToGain (knob[6]);

                for (int i = 0; i < n; ++i)
                {
                    phase += hz / rate;
                    phase -= std::floor (phase);

                    const float ml = mod (phase, shape), mr = mod (phase + offset, shape);

                    if (harmonic)
                    {
                        // Harmonic (the brown Fenders): the lows and highs throb in turn.
                        const float lowL = lowpass[0].lp (l[i]), lowR = lowpass[1].lp (r[i]);
                        l[i] = (lowL * (1.0f - depth * ml) + (l[i] - lowL) * (1.0f - depth * (1.0f - ml))) * level;
                        r[i] = (lowR * (1.0f - depth * mr) + (r[i] - lowR) * (1.0f - depth * (1.0f - mr))) * level;
                    }
                    else
                    {
                        l[i] *= (1.0f - depth * ml) * level;
                        r[i] *= (1.0f - depth * mr) * level;
                    }
                }
            }

        private:
            /** 0 .. 1: how far down the volume goes now. */
            static float mod (double p, int shape) noexcept
            {
                p -= std::floor (p);

                switch (shape)
                {
                    case 1:  return (float) (1.0 - std::abs (2.0 * p - 1.0));
                    case 2:  return 0.5f + 0.5f * std::tanh (8.0f * (float) std::sin (juce::MathConstants<double>::twoPi * p));
                    default: return (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * p));
                }
            }

            double rate = 48000.0, phase = 0.0;
            kit::OnePole lowpass[2];
        };

        //======================================================================
        //  Rotary: a Leslie - the horn and the drum, each with its own speed and inertia, the
        //  horn's doppler and throb heard from two microphones.
        //======================================================================
        class Rotary final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                for (auto& d : hornDelay) d.prepare ((int) (0.01 * sr) + 8);
                for (auto& f : split) f.set (kit::Biquad::Type::lowpass, sr, 800.0, 0.707);
                for (auto& f : splitHi) f.set (kit::Biquad::Type::highpass, sr, 800.0, 0.707);
                hornHz = knob[1];
                drumHz = knob[1] * 0.85;
                reset();
            }

            void reset() override
            {
                for (auto& d : hornDelay) d.reset();
                for (auto& f : split) f.reset();
                for (auto& f : splitHi) f.reset();
                hornPhase = drumPhase = 0.0;
            }

            void process (float* l, float* r, int n) override
            {
                const bool fast = knob[0] > 0.5f;
                const double hornTarget = fast ? knob[2] : knob[1];
                const double drumTarget = hornTarget * 0.85;
                const double rampSec = juce::jmax (0.05, (double) knob[3]);
                const double hornCoeff = std::exp (-1.0 / (rampSec * rate));
                const double drumCoeff = std::exp (-1.0 / (rampSec * 2.5 * rate));   // the heavier drum takes longer
                const float balance = knob[4] * 0.01f;
                const float doppler = knob[5] * 0.01f;
                const float drive = dbToGain (knob[6]);
                const float spread = knob[7] * 0.01f;
                const float depthSamples = (float) (0.00035 * rate) * doppler;

                for (int i = 0; i < n; ++i)
                {
                    hornHz = hornTarget + (hornHz - hornTarget) * hornCoeff;
                    drumHz = drumTarget + (drumHz - drumTarget) * drumCoeff;
                    hornPhase += hornHz / rate; hornPhase -= std::floor (hornPhase);
                    drumPhase += drumHz / rate; drumPhase -= std::floor (drumPhase);

                    const float in = std::tanh ((l[i] + r[i]) * 0.5f * drive) / juce::jmax (1.0f, std::tanh (drive));
                    const float lo = split[1].run (split[0].run (in));
                    const float hi = splitHi[1].run (splitHi[0].run (in));

                    const float hs = (float) std::sin (kit::twoPi * hornPhase), hc = (float) std::cos (kit::twoPi * hornPhase);
                    const float ds = (float) std::sin (kit::twoPi * drumPhase);

                    // The horn passes the two mics a quarter turn apart.
                    hornDelay[0].push (hi);
                    hornDelay[1].push (hi);
                    const float hornL = hornDelay[0].read (2.0f + depthSamples * (1.0f + hs)) * (1.0f - 0.35f * doppler * (0.5f + 0.5f * hs));
                    const float hornR = hornDelay[1].read (2.0f + depthSamples * (1.0f + hc)) * (1.0f - 0.35f * doppler * (0.5f + 0.5f * hc));
                    const float drum = lo * (1.0f - 0.25f * doppler * (0.5f + 0.5f * ds));

                    const float hornMid = 0.5f * (hornL + hornR);
                    const float outL = drum * (1.0f - balance) * 1.4f + (hornMid + (hornL - hornMid) * spread) * balance * 1.4f;
                    const float outR = drum * (1.0f - balance) * 1.4f + (hornMid + (hornR - hornMid) * spread) * balance * 1.4f;
                    l[i] = outL;
                    r[i] = outR;
                }
            }

        private:
            kit::Delay hornDelay[2];
            kit::Biquad split[2], splitHi[2];
            double rate = 48000.0, hornHz = 0.8, drumHz = 0.7, hornPhase = 0.0, drumPhase = 0.0;
        };

        //======================================================================
        class Vibrato final : public Block
        {
        public:
            void prepare (double sr, int) override  { rate = sr; for (auto& d : line) d.prepare ((int) (0.03 * sr) + 8); reset(); }
            void reset() override                   { for (auto& d : line) d.reset(); lfo.reset(); }

            void process (float* l, float* r, int n) override
            {
                const double hz = knob[1] > 0.5f ? 1000.0 / kit::syncedMs (1000.0 / knob[0], (int) knob[1], bpm()) : knob[0];
                const float depth = (float) (knob[2] * 0.001 * rate);
                const int shape = (int) knob[3];
                const float mix = knob[4] * 0.01f;

                for (int i = 0; i < n; ++i)
                {
                    const float m = lfo.next (hz, rate, shape);
                    const float d = 2.0f + depth * (1.0f + m);
                    line[0].push (l[i]);
                    line[1].push (r[i]);
                    l[i] = line[0].read (d) * mix + l[i] * (1.0f - mix);
                    r[i] = line[1].read (d) * mix + r[i] * (1.0f - mix);
                }
            }

        private:
            kit::Delay line[2];
            kit::Lfo lfo;
            double rate = 48000.0;
        };

        //======================================================================
        //  Chorus: up to three voices on their own LFO phases, spread across the sides.
        //======================================================================
        class Chorus final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                for (auto& d : line) d.prepare ((int) (0.06 * sr) + 8);
                lastTone = -1.0f;
                reset();
            }

            void reset() override   { for (auto& d : line) d.reset(); phase = 0.0; fb[0] = fb[1] = 0.0f; for (auto& t : tone) t.reset(); }

            void process (float* l, float* r, int n) override
            {
                const double hz = knob[0];
                const float depth = (float) (knob[1] * 0.01 * 0.004 * rate);
                const float base = (float) (knob[2] * 0.001 * rate);
                const int voices = juce::jlimit (1, 3, (int) knob[3]);
                const float spread = knob[4] * 0.01f;
                const float feedback = knob[5] * 0.01f;
                const float mix = knob[6] * 0.01f;

                if (knob[7] != lastTone)
                {
                    lastTone = knob[7];
                    for (auto& t : tone) t.lowpass (rate, knob[7]);
                }

                for (int i = 0; i < n; ++i)
                {
                    phase += hz / rate;
                    phase -= std::floor (phase);

                    line[0].push (l[i] + fb[0] * feedback);
                    line[1].push (r[i] + fb[1] * feedback);

                    float wl = 0.0f, wr = 0.0f;

                    for (int v = 0; v < voices; ++v)
                    {
                        const double p = phase + (double) v / voices;
                        const float m = (float) std::sin (kit::twoPi * p);
                        const float pan = voices == 1 ? 0.0f : ((float) v / (voices - 1) * 2.0f - 1.0f) * spread;
                        const float ml = (float) std::sin (kit::twoPi * (p + 0.25 * spread));
                        const float yl = line[0].read (base + depth * (1.0f + m));
                        const float yr = line[1].read (base + depth * (1.0f + ml));
                        wl += yl * (1.0f - juce::jmax (0.0f, pan));
                        wr += yr * (1.0f + juce::jmin (0.0f, pan));
                    }

                    wl /= (float) voices; wr /= (float) voices;
                    wl = tone[0].lp (wl); wr = tone[1].lp (wr);
                    fb[0] = wl; fb[1] = wr;

                    l[i] = l[i] * (1.0f - 0.5f * mix) + wl * mix;
                    r[i] = r[i] * (1.0f - 0.5f * mix) + wr * mix;
                }
            }

        private:
            kit::Delay line[2];
            kit::OnePole tone[2];
            double rate = 48000.0, phase = 0.0;
            float fb[2] {}, lastTone = -1.0f;
        };

        //======================================================================
        class PitchShift final : public Block
        {
        public:
            void prepare (double sr, int) override  { for (auto& s : shift) s.prepare (sr, 60.0); }
            void reset() override                   { for (auto& s : shift) s.reset(); }

            void process (float* l, float* r, int n) override
            {
                const double ratio = semitonesToRatio (knob[0] + knob[1] * 0.01);
                const float mix = knob[2] * 0.01f;
                const float level = dbToGain (knob[3]);
                const bool mono = kit::isMono (l, r, n);

                for (int i = 0; i < n; ++i)
                {
                    const float yl = shift[0].process (l[i], ratio);
                    const float yr = mono ? yl : shift[1].process (r[i], ratio);
                    l[i] = (l[i] * (1.0f - mix) + yl * mix) * level;
                    r[i] = (r[i] * (1.0f - mix) + yr * mix) * level;
                }
            }

        private:
            Shifter shift[2];
        };

        //======================================================================
        //  Whammy: the pedal bends from no shift to the interval.
        //======================================================================
        class Whammy final : public Block
        {
        public:
            void prepare (double sr, int) override  { rate = sr; for (auto& s : shift) s.prepare (sr, 45.0); pedal.prepare (sr, 15.0f, knob[0] * 0.01f); }
            void reset() override                   { for (auto& s : shift) s.reset(); }

            void process (float* l, float* r, int n) override
            {
                static const float intervals[] { 12.0f, 24.0f, -12.0f, -24.0f, 7.0f, 5.0f, -5.0f, -7.0f, -36.0f, 0.3f };
                const float interval = intervals[juce::jlimit (0, 9, (int) knob[1])];
                const bool harmony = knob[2] > 0.5f;
                const bool mono = kit::isMono (l, r, n);

                for (int i = 0; i < n; ++i)
                {
                    const float p = pedal.next (knob[0] * 0.01f);
                    const double ratio = semitonesToRatio (interval * p);
                    const float yl = shift[0].process (l[i], ratio);
                    const float yr = mono ? yl : shift[1].process (r[i], ratio);
                    l[i] = harmony ? 0.7f * (l[i] + yl) : yl;
                    r[i] = harmony ? 0.7f * (r[i] + yr) : yr;
                }
            }

        private:
            Shifter shift[2];
            Smooth pedal;
            double rate = 48000.0;
        };

        //======================================================================
        //  Harmonizer: two voices at fixed intervals beside the dry, panned apart.
        //======================================================================
        class Harmonizer final : public Block
        {
        public:
            void prepare (double sr, int) override  { for (auto& s : shift) s.prepare (sr, 50.0); }
            void reset() override                   { for (auto& s : shift) s.reset(); }

            void process (float* l, float* r, int n) override
            {
                const double r1 = semitonesToRatio (knob[0]), r2 = semitonesToRatio (knob[2]);
                const float g1 = knob[1] <= -60.0f ? 0.0f : dbToGain (knob[1]);
                const float g2 = knob[3] <= -60.0f ? 0.0f : dbToGain (knob[3]);
                const float spread = knob[4] * 0.01f;
                const float dry = knob[5] <= -60.0f ? 0.0f : dbToGain (knob[5]);

                for (int i = 0; i < n; ++i)
                {
                    const float x = 0.5f * (l[i] + r[i]);
                    const float v1 = shift[0].process (x, r1) * g1;
                    const float v2 = shift[1].process (x, r2) * g2;
                    l[i] = l[i] * dry + v1 * (1.0f + spread) * 0.5f + v2 * (1.0f - spread) * 0.5f;
                    r[i] = r[i] * dry + v1 * (1.0f - spread) * 0.5f + v2 * (1.0f + spread) * 0.5f;
                }
            }

        private:
            Shifter shift[2];
        };

        //======================================================================
        //  Doubler: a second (and third) player - slightly detuned, slightly late, wide.
        //======================================================================
        class Doubler final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                for (auto& s : shift) s.prepare (sr, 35.0);
                for (auto& d : late) d.prepare ((int) (0.08 * sr) + 8);
            }

            void reset() override   { for (auto& s : shift) s.reset(); for (auto& d : late) d.reset(); }

            void process (float* l, float* r, int n) override
            {
                const double cents = knob[0];
                const float delay = (float) (knob[1] * 0.001 * rate);
                const float width = knob[2] * 0.01f;
                const float mix = knob[3] * 0.01f;
                const double up = semitonesToRatio (cents * 0.01), down = semitonesToRatio (-cents * 0.01);

                for (int i = 0; i < n; ++i)
                {
                    const float x = 0.5f * (l[i] + r[i]);
                    late[0].push (shift[0].process (x, up));
                    late[1].push (shift[1].process (x, down));
                    const float a = late[0].read (1.0f + delay), b = late[1].read (1.0f + delay * 1.37f);
                    const float wl = a * (0.5f + 0.5f * width) + b * (0.5f - 0.5f * width);
                    const float wr = b * (0.5f + 0.5f * width) + a * (0.5f - 0.5f * width);
                    l[i] = l[i] * (1.0f - 0.5f * mix) + wl * mix;
                    r[i] = r[i] * (1.0f - 0.5f * mix) + wr * mix;
                }
            }

        private:
            Shifter shift[2];
            kit::Delay late[2];
            double rate = 48000.0;
        };

        //======================================================================
        //  Tape Echo: wow and flutter on the tape speed, the head driving the tape, the repeats
        //  losing their top and bottom.
        //======================================================================
        class TapeEcho final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                for (auto& d : tape) d.prepare ((int) (2.2 * sr) + 64);
                time.prepare (sr, 250.0f, -1.0f);
                lastTone = -1.0f;
                reset();
            }

            void reset() override
            {
                for (auto& d : tape) d.reset();
                for (auto& f : lp) f.reset();
                for (auto& f : hp) f.reset();
                fb[0] = fb[1] = 0.0f;
                wow.reset(); flutter.reset();
                time.snap (-1.0f);
            }

            void process (float* l, float* r, int n) override
            {
                const double ms = juce::jlimit (20.0, 2000.0, kit::syncedMs (knob[0], (int) knob[1], bpm()));
                const float feedback = knob[2] * 0.01f * 1.05f;
                const float wowDepth = knob[3] * 0.01f, flutterDepth = knob[4] * 0.01f;
                const float sat = dbToGain (knob[5] * 0.24f);
                const float mix = knob[7] * 0.01f;

                if (knob[6] != lastTone)
                {
                    lastTone = knob[6];
                    for (auto& f : lp) f.set (kit::Biquad::Type::lowpass, rate, knob[6], 0.6);
                    for (auto& f : hp) f.set (kit::Biquad::Type::highpass, rate, 120.0, 0.6);
                }

                const float target = (float) (ms * 0.001 * rate);
                if (time.value < 0.0f) time.snap (target);

                for (int i = 0; i < n; ++i)
                {
                    const float base = time.next (target);
                    const float mod = wow.next (0.6, rate, 4) * wowDepth * 0.004f * base
                                    + flutter.next (7.5, rate, 0) * flutterDepth * 0.0008f * base;
                    const float d = juce::jmax (4.0f, base + mod);

                    const float yl = tape[0].read (d), yr = tape[1].read (d);

                    // The head: soft saturation, and the repeats darker each time round.
                    const float in0 = l[i] + fb[0] * feedback, in1 = r[i] + fb[1] * feedback;
                    tape[0].push (std::tanh (in0 * sat) / sat);
                    tape[1].push (std::tanh (in1 * sat) / sat);
                    fb[0] = hp[0].run (lp[0].run (yl));
                    fb[1] = hp[1].run (lp[1].run (yr));

                    l[i] = l[i] * (1.0f - 0.5f * mix) + fb[0] * mix;
                    r[i] = r[i] * (1.0f - 0.5f * mix) + fb[1] * mix;
                }
            }

        private:
            kit::Delay tape[2];
            kit::Biquad lp[2], hp[2];
            kit::Lfo wow, flutter;
            Smooth time;
            double rate = 48000.0;
            float fb[2] {}, lastTone = -1.0f;
        };

        //======================================================================
        class PingPong final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                for (auto& d : line) d.prepare ((int) (2.6 * sr) + 64);
                time.prepare (sr, 120.0f, -1.0f);
                lastTone = -1.0f;
                reset();
            }

            void reset() override   { for (auto& d : line) d.reset(); for (auto& f : tone) f.reset(); time.snap (-1.0f); }

            void process (float* l, float* r, int n) override
            {
                const double ms = juce::jlimit (10.0, 2500.0, kit::syncedMs (knob[0], (int) knob[1], bpm()));
                const float feedback = knob[2] * 0.01f;
                const float width = knob[3] * 0.01f;
                const float mix = knob[5] * 0.01f;

                if (knob[4] != lastTone)
                {
                    lastTone = knob[4];
                    for (auto& f : tone) f.set (kit::Biquad::Type::lowpass, rate, knob[4], 0.7);
                }

                const float target = (float) (ms * 0.001 * rate);
                if (time.value < 0.0f) time.snap (target);

                for (int i = 0; i < n; ++i)
                {
                    const float d = time.next (target);
                    const float yl = line[0].read (d), yr = line[1].read (d);
                    const float in = 0.5f * (l[i] + r[i]);

                    // The input enters the left; each repeat crosses to the other side.
                    line[0].push (in + tone[1].run (yr) * feedback);
                    line[1].push (tone[0].run (yl) * feedback);

                    const float mid = 0.5f * (yl + yr), side = 0.5f * (yl - yr) * width;
                    l[i] = l[i] * (1.0f - 0.5f * mix) + (mid + side) * mix;
                    r[i] = r[i] * (1.0f - 0.5f * mix) + (mid - side) * mix;
                }
            }

        private:
            kit::Delay line[2];
            kit::Biquad tone[2];
            Smooth time;
            double rate = 48000.0;
            float lastTone = -1.0f;
        };

        //======================================================================
        //  Reverse Delay: each slice of the playing is played back backwards, faded at its ends.
        //======================================================================
        class ReverseDelay final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                const int size = (int) (2.1 * sr) + 8;
                for (auto& b : rec) b.assign ((size_t) size, 0.0f);
                for (auto& b : play) b.assign ((size_t) size, 0.0f);
                reset();
            }

            void reset() override
            {
                for (auto& b : rec) std::fill (b.begin(), b.end(), 0.0f);
                for (auto& b : play) std::fill (b.begin(), b.end(), 0.0f);
                pos = 0;
                length = 0;
                fb[0] = fb[1] = 0.0f;
            }

            void process (float* l, float* r, int n) override
            {
                const double ms = juce::jlimit (50.0, 2000.0, kit::syncedMs (knob[0], (int) knob[1], bpm()));
                const float feedback = knob[2] * 0.01f;
                const float mix = knob[3] * 0.01f;
                const int wanted = juce::jlimit (16, (int) rec[0].size() - 1, (int) (ms * 0.001 * rate));

                if (length == 0)
                    length = wanted;

                float* x[] { l, r };

                for (int i = 0; i < n; ++i)
                {
                    // At the end of a slice: what was recorded becomes what plays, backwards.
                    if (pos >= length)
                    {
                        for (int c = 0; c < 2; ++c)
                            std::copy (rec[c].begin(), rec[c].begin() + length, play[c].begin());

                        playLength = length;
                        length = wanted;
                        pos = 0;
                    }

                    const int fade = juce::jmax (1, playLength / 8);

                    for (int c = 0; c < 2; ++c)
                    {
                        float y = 0.0f;

                        if (playLength > 0 && pos < playLength)
                        {
                            const float g = juce::jmin (1.0f, (float) juce::jmin (pos, playLength - 1 - pos) / (float) fade);
                            y = play[c][(size_t) (playLength - 1 - pos)] * g;
                        }

                        rec[c][(size_t) pos] = x[c][i] + y * feedback;
                        x[c][i] = x[c][i] * (1.0f - 0.5f * mix) + y * mix;
                    }

                    ++pos;
                }
            }

        private:
            std::vector<float> rec[2], play[2];
            int pos = 0, length = 0, playLength = 0;
            double rate = 48000.0;
            float fb[2] {};
        };

        //======================================================================
        //  Shimmer: an eight-line feedback delay network with an octave (or a fifth) shifted up
        //  inside its loop, so each pass rises - the shimmering wash.
        //======================================================================
        class Shimmer final : public Block
        {
        public:
            void prepare (double sr, int) override
            {
                rate = sr;
                static const double lengthsMs[8] { 29.7, 37.1, 41.1, 43.7, 53.3, 59.9, 67.7, 73.1 };

                for (int k = 0; k < 8; ++k)
                {
                    base[k] = lengthsMs[k] * 0.001 * sr;
                    lines[k].prepare ((int) (base[k] * 2.6) + 16);
                }

                shifter.prepare (sr, 70.0);
                pre.prepare ((int) (0.25 * sr) + 8);
                lastDamp = -1.0f;
                reset();
            }

            void reset() override
            {
                for (auto& d : lines) d.reset();
                for (auto& f : damp) f.reset();
                shifter.reset();
                pre.reset();
                for (auto& f : lfoPhase) f = 0.0;
            }

            void process (float* l, float* r, int n) override
            {
                const double decay = knob[0];
                const double size = knob[1] * 0.01 * 1.5 + 0.5;
                const float amount = knob[2] * 0.01f;
                static const float intervals[] { 12.0f, 7.0f, 19.0f, 24.0f, -12.0f };
                const double ratio = std::pow (2.0, intervals[juce::jlimit (0, 4, (int) knob[3])] / 12.0);
                const float mix = knob[5] * 0.01f;
                const float preDelay = (float) juce::jmax (1.0, knob[6] * 0.001 * rate);

                if (knob[4] != lastDamp)
                {
                    lastDamp = knob[4];
                    const double hz = 18000.0 * std::pow (0.08, knob[4] * 0.01);
                    for (auto& f : damp) f.lowpass (rate, hz);
                }

                double gains[8];
                for (int k = 0; k < 8; ++k)
                    gains[k] = std::pow (10.0, -3.0 * base[k] * size / (decay * rate));   // -60 dB in "decay" seconds

                for (int i = 0; i < n; ++i)
                {
                    pre.push (0.5f * (l[i] + r[i]));
                    const float in = pre.read (preDelay);

                    float y[8];
                    for (int k = 0; k < 8; ++k)
                    {
                        lfoPhase[k] += (0.3 + 0.07 * k) / rate;
                        lfoPhase[k] -= std::floor (lfoPhase[k]);
                        const float wobble = (float) (std::sin (kit::twoPi * lfoPhase[k]) * 0.0006 * rate);
                        y[k] = lines[k].read ((float) (base[k] * size) + wobble + 2.0f);
                    }

                    // The shimmer: part of the network's sum goes back in pitched up.
                    float sum = 0.0f;
                    for (float v : y) sum += v;
                    const float shimmer = shifter.process (sum * 0.125f, ratio) * amount;

                    // Householder mix: each line feeds every other.
                    const float h = sum * 0.25f;

                    for (int k = 0; k < 8; ++k)
                    {
                        const float fbk = (y[k] - h) * (float) gains[k];
                        lines[k].push (damp[k].lp (fbk) + in * 0.35f + shimmer * 0.6f);
                    }

                    const float wl = (y[0] + y[2] + y[4] + y[6]) * 0.35f, wr = (y[1] + y[3] + y[5] + y[7]) * 0.35f;
                    l[i] = l[i] * (1.0f - mix) + wl * mix;
                    r[i] = r[i] * (1.0f - mix) + wr * mix;
                }
            }

        private:
            kit::Delay lines[8], pre;
            kit::OnePole damp[8];
            Shifter shifter;
            double base[8] {}, lfoPhase[8] {};
            double rate = 48000.0;
            float lastDamp = -1.0f;
        };

        //======================================================================
        class VolumePan final : public Block
        {
        public:
            void prepare (double sr, int) override  { for (auto& s : sm) s.prepare (sr, 10.0f, 1.0f); }

            void process (float* l, float* r, int n) override
            {
                const float g = knob[0] <= -60.0f ? 0.0f : dbToGain (knob[0]);
                const float pan = knob[1] * 0.01f;
                const float width = knob[2] * 0.01f;
                const float pol = knob[3] > 0.5f ? -1.0f : 1.0f;
                const float gl = (pan > 0.0f ? std::cos (pan * kit::pi * 0.5f) : 1.0f) * g * pol;
                const float gr = (pan < 0.0f ? std::cos (-pan * kit::pi * 0.5f) : 1.0f) * g * pol;

                for (int i = 0; i < n; ++i)
                {
                    const float mid = 0.5f * (l[i] + r[i]), side = 0.5f * (l[i] - r[i]) * width;
                    l[i] = (mid + side) * sm[0].next (gl);
                    r[i] = (mid - side) * sm[1].next (gr);
                }
            }

        private:
            Smooth sm[2];
        };
    }

    //==========================================================================
    void registry::addOwn (std::vector<BlockInfo>& list)
    {
        using namespace kit;

        list.push_back (info ("tw_fuzz", "Fuzz", Family::drive, "gfx-fuzz", ownCredit,
            "A starved two-transistor fuzz, with an octave for the Octavia sound.",
            { percent ("Fuzz", 70.0f), percent ("Tone", 55.0f), percent ("Bias", 30.0f), percent ("Octave", 0.0f),
              knob ("Level", -24.0f, 12.0f, -6.0f, " dB", 0.0f, 1) },
            maker<Fuzz>()));

        list.push_back (info ("tw_volume", "Volume Pedal", Family::dynamics, "gfx-volume-pedal", ownCredit,
            "A volume pedal: put Pedal on an expression pedal.",
            { percent ("Pedal", 100.0f), knob ("Minimum", -60.0f, 0.0f, -60.0f, " dB", -24.0f, 0), choice ("Taper", { "Log", "Linear" }) },
            maker<VolumePedal>()));

        list.push_back (info ("tw_swell", "Auto Swell", Family::dynamics, "gfx-volume-swell", ownCredit,
            "Every new note fades in: violin-like swells without a pedal.",
            { knob ("Threshold", -60.0f, 0.0f, -36.0f, " dB", -24.0f, 1), knob ("Rise", 20.0f, 2000.0f, 350.0f, " ms", 300.0f),
              knob ("Level", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1) },
            maker<AutoSwell>()));

        list.push_back (info ("tw_tremolo", "Tremolo", Family::modulation, "gfx-tremolo", ownCredit,
            "Volume throb: sine, triangle or square, in stereo, or harmonic.",
            { knob ("Rate", 0.1f, 20.0f, 5.0f, " Hz", 4.0f), choice ("Sync", divisionNames()), percent ("Depth", 60.0f),
              choice ("Shape", { "Sine", "Triangle", "Square" }), knob ("Stereo", 0.0f, 180.0f, 0.0f, " deg"),
              choice ("Harmonic", { "Off", "On" }), knob ("Level", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1) },
            maker<Tremolo>()));

        list.push_back (info ("tw_rotary", "Rotary", Family::modulation, "gfx-rotary", ownCredit,
            "A rotating speaker: switch Slow and Fast from a footswitch.",
            { choice ("Speed", { "Slow", "Fast" }), knob ("Slow", 0.2f, 2.0f, 0.8f, " Hz", 0.0f, 2), knob ("Fast", 3.0f, 9.0f, 6.7f, " Hz", 0.0f, 2),
              knob ("Ramp", 0.1f, 5.0f, 1.2f, " s", 1.0f, 2), percent ("Horn", 60.0f), percent ("Doppler", 70.0f),
              knob ("Drive", 0.0f, 24.0f, 3.0f, " dB", 0.0f, 1), percent ("Spread", 80.0f) },
            maker<Rotary>()));

        list.push_back (info ("tw_vibrato", "Vibrato", Family::modulation, "gfx-vibrato", ownCredit,
            "True pitch vibrato: a delay swept by its LFO.",
            { knob ("Rate", 0.1f, 14.0f, 5.0f, " Hz", 4.0f), choice ("Sync", divisionNames()), knob ("Depth", 0.0f, 8.0f, 1.5f, " ms", 2.0f, 2),
              choice ("Shape", { "Sine", "Triangle", "Square", "Saw", "Random" }), percent ("Mix", 100.0f) },
            maker<Vibrato>()));

        list.push_back (info ("tw_chorus", "Chorus", Family::modulation, "gfx-chorus", ownCredit,
            "One to three chorus voices, spread across the stereo field.",
            { knob ("Rate", 0.05f, 8.0f, 0.8f, " Hz", 1.0f), percent ("Depth", 50.0f), knob ("Delay", 2.0f, 30.0f, 8.0f, " ms", 8.0f),
              knob ("Voices", 1.0f, 3.0f, 2.0f, {}, 0.0f, 0), percent ("Spread", 80.0f), percent ("Feedback", 0.0f), percent ("Mix", 50.0f),
              knob ("Tone", 1000.0f, 18000.0f, 9000.0f, " Hz", 6000.0f) },
            maker<Chorus>()));

        list.push_back (info ("tw_pitch", "Pitch Shifter", Family::pitch, "gfx-pitch-shift", ownCredit,
            "Shifts the pitch up or down by semitones and cents.",
            { knob ("Semitones", -24.0f, 24.0f, 12.0f, " st", 0.0f, 0), knob ("Cents", -100.0f, 100.0f, 0.0f, " ct"), percent ("Mix", 50.0f),
              knob ("Level", -12.0f, 12.0f, 0.0f, " dB", 0.0f, 1) },
            maker<PitchShift>()));

        list.push_back (info ("tw_whammy", "Whammy", Family::pitch, "gfx-whammy", ownCredit,
            "The pedal bends the pitch to the interval: put Pedal on an expression pedal.",
            { percent ("Pedal", 0.0f), choice ("Interval", { "+1 oct", "+2 oct", "-1 oct", "-2 oct", "+5th", "+4th", "-4th", "-5th", "Dive bomb", "Detune" }),
              choice ("Mode", { "Whammy", "Harmony" }) },
            maker<Whammy>()));

        list.push_back (info ("tw_harmonizer", "Harmonizer", Family::pitch, "gfx-harmonizer", ownCredit,
            "Two more voices at fixed intervals, panned apart.",
            { knob ("Voice 1", -24.0f, 24.0f, 4.0f, " st", 0.0f, 0), knob ("Level 1", -60.0f, 6.0f, -6.0f, " dB", -12.0f, 1),
              knob ("Voice 2", -24.0f, 24.0f, 7.0f, " st", 0.0f, 0), knob ("Level 2", -60.0f, 6.0f, -6.0f, " dB", -12.0f, 1),
              percent ("Spread", 60.0f), knob ("Dry", -60.0f, 6.0f, 0.0f, " dB", -12.0f, 1) },
            maker<Harmonizer>()));

        list.push_back (info ("tw_doubler", "Doubler", Family::pitch, "gfx-doubler", ownCredit,
            "A second player: slightly detuned and late, spread wide.",
            { knob ("Detune", 0.0f, 50.0f, 9.0f, " ct", 10.0f), knob ("Delay", 5.0f, 60.0f, 18.0f, " ms", 20.0f), percent ("Width", 100.0f),
              percent ("Mix", 60.0f) },
            maker<Doubler>()));

        list.push_back (info ("tw_tape", "Tape Echo", Family::time, "gfx-tape-echo", ownCredit,
            "A tape echo: wow and flutter, saturating heads, darkening repeats.",
            { knob ("Time", 20.0f, 2000.0f, 420.0f, " ms", 400.0f), choice ("Sync", divisionNames()), percent ("Feedback", 40.0f),
              percent ("Wow", 30.0f), percent ("Flutter", 25.0f), percent ("Saturation", 40.0f), knob ("Tone", 1000.0f, 12000.0f, 4200.0f, " Hz", 4000.0f),
              percent ("Mix", 35.0f) },
            maker<TapeEcho>(), true));

        list.push_back (info ("tw_pingpong", "Ping-Pong", Family::time, "gfx-ping-pong", ownCredit,
            "Repeats bouncing between left and right.",
            { knob ("Time", 10.0f, 2500.0f, 375.0f, " ms", 400.0f), choice ("Sync", divisionNames()), percent ("Feedback", 45.0f),
              percent ("Width", 100.0f), knob ("Tone", 1000.0f, 18000.0f, 8000.0f, " Hz", 6000.0f), percent ("Mix", 35.0f) },
            maker<PingPong>(), true));

        list.push_back (info ("tw_reverse", "Reverse Delay", Family::time, "gfx-reverse", ownCredit,
            "Each slice of your playing comes back backwards.",
            { knob ("Time", 50.0f, 2000.0f, 600.0f, " ms", 600.0f), choice ("Sync", divisionNames()), percent ("Feedback", 30.0f), percent ("Mix", 45.0f) },
            maker<ReverseDelay>(), true));

        list.push_back (info ("tw_shimmer", "Shimmer", Family::ambience, "gfx-shimmer", ownCredit,
            "A big reverb whose tail keeps rising an octave (or a fifth) as it rings.",
            { knob ("Decay", 0.5f, 20.0f, 6.0f, " s", 5.0f), percent ("Size", 60.0f), percent ("Shimmer", 50.0f),
              choice ("Interval", { "+1 oct", "+5th", "+1 oct 5th", "+2 oct", "-1 oct" }), percent ("Damp", 35.0f), percent ("Mix", 35.0f),
              knob ("Pre-delay", 0.0f, 200.0f, 20.0f, " ms", 40.0f) },
            maker<Shimmer>(), true));

        list.push_back (info ("tw_volpan", "Volume / Pan", Family::utility, "gfx-volume-pedal", ownCredit,
            "Level, pan, stereo width and polarity.",
            { knob ("Volume", -60.0f, 12.0f, 0.0f, " dB", -6.0f, 1), knob ("Pan", -100.0f, 100.0f, 0.0f), knob ("Width", 0.0f, 200.0f, 100.0f, " %"),
              choice ("Polarity", { "Normal", "Inverted" }) },
            maker<VolumePan>()));
    }
}
