#pragma once

#include "AmpTube.h"

#include <array>

// ============================================================================
//  AXIS CAB — the four modelled cabinets, and the microphone in front of them.
//
//  Plain C++17, like the amp's core. Each cabinet is a short chain of filters:
//  the cone's low resonance and the box below it, the body, the breakup peaks
//  of the paper, and the steep fall above them that every guitar speaker has.
//
//  The two closed 4x12s are fitted to real impulse responses, a 4x12 with the
//  modern British speaker and one with the vintage one: within about 1 dB
//  (third-octave, 80 Hz to 8 kHz) of what was measured. The two open backs
//  are drawn from what open-back combos are known for (less low end, the box
//  open behind them; the American one scooped, the British one forward and
//  bright).
//
//  Every cabinet is levelled to the same energy as an impulse response is in
//  an AXIS pack (AxisPack::readIr), so the modelled ones and the IRs sit at
//  one level and swapping cabinets doesn't change how loud the amp is.
// ============================================================================
namespace axis::dsp::cab
{
    using amp::Biquad;

    enum class Band : int { none = 0, highpass, lowpass, peak };

    struct Filter { Band kind = Band::none; double hz = 1000.0, q = 0.7, db = 0.0; };

    struct SpeakerSpec
    {
        const char* name;               // on the cabinet's menu
        std::array<Filter, 10> filters;
    };

    inline constexpr int numSpeakers = 4;

    inline const std::array<SpeakerSpec, numSpeakers>& speakerSpecs()
    {
        using B = Band;

        static const std::array<SpeakerSpec, numSpeakers> specs
        {{
            { "1x12 OPEN",
              {{ { B::highpass, 70.0, 0.75 }, { B::peak, 120.0, 0.9, 2.0 }, { B::peak, 500.0, 0.8, -3.0 },
                 { B::peak, 2500.0, 1.2, 4.0 }, { B::peak, 4500.0, 2.0, 3.0 },
                 { B::lowpass, 6500.0, 0.85 }, { B::lowpass, 7200.0, 0.55 } }} },

            { "2x12 ALNICO",
              {{ { B::highpass, 85.0, 0.9 }, { B::peak, 150.0, 0.9, 2.0 }, { B::peak, 800.0, 1.0, 2.0 },
                 { B::peak, 3200.0, 1.5, 5.0 }, { B::peak, 5000.0, 3.0, 4.0 },
                 { B::lowpass, 6000.0, 0.9 }, { B::lowpass, 6800.0, 0.55 } }} },

            { "4x12 VINTAGE",
              {{ { B::highpass, 101.0, 1.8 }, { B::highpass, 101.0, 0.83 }, { B::peak, 171.0, 1.2, 4.65 },
                 { B::peak, 1078.0, 2.5, -3.25 }, { B::peak, 2924.0, 1.77, 10.0 }, { B::peak, 4757.0, 4.7, 10.0 },
                 { B::lowpass, 6438.0, 0.5 }, { B::lowpass, 7082.0, 0.5 }, { B::lowpass, 8047.0, 0.5 } }} },

            { "4x12 MODERN",
              {{ { B::highpass, 130.0, 1.37 }, { B::highpass, 130.0, 0.9 }, { B::peak, 110.0, 1.04, 12.0 },
                 { B::peak, 680.0, 2.5, 3.0 }, { B::peak, 1984.0, 1.04, 5.5 }, { B::peak, 5171.0, 2.0, 10.0 },
                 { B::lowpass, 5458.0, 2.0 }, { B::lowpass, 6004.0, 0.53 }, { B::lowpass, 6822.0, 0.5 } }} },
        }};

        return specs;
    }

    /** One modelled cabinet, one channel. */
    class Speaker
    {
    public:
        /** Its filters at a sample rate, and the level that puts it at the energy of a levelled
            IR (levelFor, worked out beforehand: it renders half a second, so not on the audio
            thread). Nothing here allocates. */
        void design (int index, double fs, double levelled) noexcept
        {
            const auto& spec = speakerSpecs()[(size_t) std::clamp (index, 0, numSpeakers - 1)];
            count = 0;

            for (const auto& f : spec.filters)
            {
                if (f.kind == Band::none)
                    continue;

                auto& b = bands[(size_t) count++];

                if (f.kind == Band::highpass)     b.setHighpass (f.hz, f.q, fs);
                else if (f.kind == Band::lowpass) b.setLowpass (f.hz, f.q, fs);
                else                               b.setPeak (f.hz, f.q, f.db, fs);
            }

            level = levelled;
            reset();
        }

        /** The level that gives a cabinet the energy of one: 1 / sqrt (the energy of its impulse
            response). */
        static double levelFor (int index, double fs) noexcept
        {
            Speaker s;
            s.design (index, fs, 1.0);

            double energy = 0.0;
            const int length = (int) (0.5 * fs);

            for (int i = 0; i < length; ++i)
            {
                const double v = s.process (i == 0 ? 1.0 : 0.0);
                energy += v * v;
            }

            return energy > 1.0e-12 ? 1.0 / std::sqrt (energy) : 1.0;
        }

        void reset() noexcept
        {
            for (auto& b : bands)
                b.reset();
        }

        double process (double x) noexcept
        {
            for (int k = 0; k < count; ++k)
                x = bands[(size_t) k].process (x);

            return x * level;
        }

    private:
        std::array<Biquad, 10> bands;
        int count = 0;
        double level = 1.0;
    };

    /** The microphone: across the cone from its centre (0, the brightest) to its edge (100, the
        darkest), and off its axis either way of the middle (Y 50 is on axis; further up or down
        costs the top end). On the modelled cabinets and the IRs alike. */
    class Mic
    {
    public:
        void set (double x, double y, double fs) noexcept
        {
            const double edge = std::clamp (x, 0.0, 100.0) / 100.0;
            const double offAxis = std::abs (std::clamp (y, 0.0, 100.0) - 50.0) / 50.0;
            cone.setHighShelf (2500.0, 0.7, 2.0 - 9.0 * edge, fs);
            axis.setHighShelf (5000.0, 0.7, -5.0 * offAxis, fs);
        }

        void reset() noexcept               { cone.reset(); axis.reset(); }
        double process (double x) noexcept  { return axis.process (cone.process (x)); }

        /** What the cabinet's picture says about where the mic is. */
        static const char* describe (double x, double y) noexcept
        {
            const bool off = std::abs (y - 50.0) > 18.0;

            if (x < 33.0) return off ? "CAP · OFF AXIS"  : "CAP · ON AXIS";
            if (x < 66.0) return off ? "CONE · OFF AXIS" : "CONE · ON AXIS";
            return off ? "EDGE · OFF AXIS" : "EDGE · ON AXIS";
        }

    private:
        Biquad cone, axis;
    };
}
