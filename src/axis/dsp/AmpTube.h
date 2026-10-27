#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================================
//  AXIS AMP — the tubes, and the small filters round them.
//
//  Plain C++17 (no JUCE), so the amp's sound can be built and measured on its
//  own. Everything here comes from published, well-proven knowledge:
//
//    the triode       Norman Koren's 12AX7 model (Koren, "Improved VT models
//                     for SPICE simulations", 1996): mu 100, ex 1.4, kg1 1060,
//                     kp 600, kvb 300, rgi 2000
//    a gain stage     solved along its AC load line, plate resistor, cathode
//                     resistor and grid current included, into a table; the
//                     cathode's bypass capacitor as the shelf it makes (the
//                     standard "static curve plus filters" approach, Pakarinen
//                     and Yeh, "A Review of Digital Techniques for Modeling
//                     Vacuum-Tube Guitar Amplifiers", Computer Music Journal
//                     33:2, 2009)
//    aliasing         first-order antiderivative anti-aliasing (Parker,
//                     Zavalishin and Le Bivic, DAFx 2016) on every curve, with
//                     the oversampling round the amp (AmpModel.h)
//    the filters      the bilinear transform of the circuits' own poles and
//                     zeros; the biquads are the Audio EQ Cookbook's (RBJ)
// ============================================================================
namespace axis::dsp::amp
{
    constexpr double pi = 3.14159265358979323846;

    //==========================================================================
    // Small filters, in double: the poles of a cathode or a coupling cap sit
    // a long way below the oversampled rate, where float runs out of room.

    /** One pole and one zero, from the analog H(s) = (s + wz) / (s + wp) scaled to 1 at
        high frequencies, or a plain lowpass or highpass. Bilinear transform. */
    struct FirstOrder
    {
        double b0 = 1.0, b1 = 0.0, a1 = 0.0, z = 0.0;

        void setZeroPole (double wz, double wp, double fs) noexcept
        {
            const double c = 2.0 * fs;
            b0 = (c + wz) / (c + wp);
            b1 = (wz - c) / (c + wp);
            a1 = (wp - c) / (c + wp);
        }

        void setLowpass (double fc, double fs) noexcept
        {
            const double k = std::tan (pi * std::clamp (fc, 1.0, fs * 0.49) / fs);
            b0 = b1 = k / (1.0 + k);
            a1 = (k - 1.0) / (k + 1.0);
        }

        void setHighpass (double fc, double fs) noexcept
        {
            const double k = std::tan (pi * std::clamp (fc, 0.1, fs * 0.49) / fs);
            b0 = 1.0 / (1.0 + k);
            b1 = -b0;
            a1 = (k - 1.0) / (k + 1.0);
        }

        void setThrough() noexcept       { b0 = 1.0; b1 = 0.0; a1 = 0.0; }

        double process (double x) noexcept
        {
            const double y = b0 * x + z;
            z = b1 * x - a1 * y;
            return y;
        }

        void reset() noexcept            { z = 0.0; }
    };

    /** A biquad (Direct Form II transposed) with the Audio EQ Cookbook's designs. */
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, z1 = 0.0, z2 = 0.0;

        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void reset() noexcept            { z1 = z2 = 0.0; }
        void setThrough() noexcept       { b0 = 1.0; b1 = b2 = a1 = a2 = 0.0; }

        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
        {
            b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
        }

        void setPeak (double f, double q, double db, double fs) noexcept
        {
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * clampF (f, fs) / fs;
            const double al = std::sin (w) / (2.0 * q), cw = std::cos (w);
            set (1.0 + al * A, -2.0 * cw, 1.0 - al * A, 1.0 + al / A, -2.0 * cw, 1.0 - al / A);
        }

        void setLowShelf (double f, double q, double db, double fs) noexcept
        {
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * clampF (f, fs) / fs;
            const double al = std::sin (w) / (2.0 * q), cw = std::cos (w), sa = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1.0) - (A - 1.0) * cw + sa), 2.0 * A * ((A - 1.0) - (A + 1.0) * cw),
                 A * ((A + 1.0) - (A - 1.0) * cw - sa), (A + 1.0) + (A - 1.0) * cw + sa,
                 -2.0 * ((A - 1.0) + (A + 1.0) * cw), (A + 1.0) + (A - 1.0) * cw - sa);
        }

        void setHighShelf (double f, double q, double db, double fs) noexcept
        {
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * clampF (f, fs) / fs;
            const double al = std::sin (w) / (2.0 * q), cw = std::cos (w), sa = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1.0) + (A - 1.0) * cw + sa), -2.0 * A * ((A - 1.0) + (A + 1.0) * cw),
                 A * ((A + 1.0) + (A - 1.0) * cw - sa), (A + 1.0) - (A - 1.0) * cw + sa,
                 2.0 * ((A - 1.0) - (A + 1.0) * cw), (A + 1.0) - (A - 1.0) * cw - sa);
        }

        void setHighpass (double f, double q, double fs) noexcept
        {
            const double w = 2.0 * pi * clampF (f, fs) / fs, al = std::sin (w) / (2.0 * q), cw = std::cos (w);
            set ((1.0 + cw) * 0.5, -(1.0 + cw), (1.0 + cw) * 0.5, 1.0 + al, -2.0 * cw, 1.0 - al);
        }

        void setLowpass (double f, double q, double fs) noexcept
        {
            const double w = 2.0 * pi * clampF (f, fs) / fs, al = std::sin (w) / (2.0 * q), cw = std::cos (w);
            set ((1.0 - cw) * 0.5, 1.0 - cw, (1.0 - cw) * 0.5, 1.0 + al, -2.0 * cw, 1.0 - al);
        }

    private:
        static double clampF (double f, double fs) noexcept   { return std::clamp (f, 1.0, fs * 0.49); }
    };

    //==========================================================================
    /** Koren's 12AX7: plate current (A) from plate-cathode and grid-cathode volts. */
    struct Koren12AX7
    {
        static constexpr double mu = 100.0, ex = 1.4, kg1 = 1060.0, kp = 600.0, kvb = 300.0, rgi = 2000.0;

        static double plateCurrent (double vpk, double vgk) noexcept
        {
            vpk = std::max (vpk, 1.0e-3);
            const double arg = std::min (kp * (1.0 / mu + vgk / std::sqrt (kvb + vpk * vpk)), 60.0);
            const double e1 = (vpk / kp) * std::log1p (std::exp (arg));
            return e1 > 0.0 ? 2.0 * std::pow (e1, ex) / kg1 : 0.0;
        }

        static double gridCurrent (double vgk) noexcept
        {
            return vgk > 0.0 ? std::pow (vgk, 1.5) / rgi : 0.0;
        }
    };

    /** The root of an increasing function between lo (f < 0) and hi (f > 0): Illinois' false
        position, which keeps a bracket and still converges fast on smooth curves. */
    template <typename F>
    double solveRising (F&& f, double lo, double hi, double tolerance = 1.0e-12, int most = 80)
    {
        double flo = f (lo), fhi = f (hi);

        if (flo >= 0.0) return lo;
        if (fhi <= 0.0) return hi;

        int side = 0;
        double x = lo;

        for (int i = 0; i < most; ++i)
        {
            x = (lo * fhi - hi * flo) / (fhi - flo);
            const double fx = f (x);

            if (std::abs (fx) < tolerance || (hi - lo) < tolerance * (1.0 + std::abs (x)))
                break;

            if (fx > 0.0)
            {
                hi = x; fhi = fx;
                if (side == +1) flo *= 0.5;
                side = +1;
            }
            else
            {
                lo = x; flo = fx;
                if (side == -1) fhi *= 0.5;
                side = -1;
            }
        }

        return x;
    }

    //==========================================================================
    /** One 12AX7 gain stage as it is in the amp: its plate resistor, its cathode resistor
        (bypassed by a capacitor or not), the supply, the next stage's grid leak as part of its
        AC load, and the resistance driving its grid (for grid current). */
    struct TriodeSpec
    {
        double plateR = 100.0e3;
        double cathodeR = 1.5e3;
        double cathodeC = 22.0e-6;      // the bypass cap (0: unbypassed)
        double supply = 300.0;          // B+ at the top of the plate resistor
        double loadR = 1.0e6;           // what the plate drives, AC-coupled (the next grid leak)
        double sourceR = 68.0e3;        // the grid stopper and what drives it
        double couplingHz = 15.0;       // the coupling cap into its grid, with its grid leak
        double millerHz = 16000.0;      // the plate's and the Miller capacitance's rolloff
    };

    /** A stage's static transfer, grid volts to plate swing (volts, inverted as the tube inverts),
        solved once into a table with its antiderivative, for ADAA. */
    class TriodeCurve
    {
    public:
        void build (const TriodeSpec& spec, double lowest = -60.0, double highest = 60.0, int points = 4096)
        {
            using T = Koren12AX7;

            const bool bypassed = spec.cathodeC > 0.0;
            const double rp = spec.plateR, rk = spec.cathodeR, b = spec.supply;
            rac = rp * spec.loadR / (rp + spec.loadR);

            // The working point, with no signal: Vg = 0, Vk = Ip Rk, Vp = B - Ip Rp.
            ip0 = solveRising ([&] (double ip) { return ip - T::plateCurrent (b - ip * (rp + rk), -ip * rk); },
                               0.0, b / (rp + rk));
            vk0 = ip0 * rk;
            vp0 = b - ip0 * rp;

            // Small-signal numbers there, for the cathode's shelf.
            const double d = 1.0e-3;
            gm = (T::plateCurrent (vp0 - vk0, -vk0 + d) - T::plateCurrent (vp0 - vk0, -vk0 - d)) / (2.0 * d);
            rplate = 2.0 * d / (T::plateCurrent (vp0 - vk0 + d, -vk0) - T::plateCurrent (vp0 - vk0 - d, -vk0));

            // The curve, along the AC load line through the working point.
            lo = lowest;
            hi = highest;
            n = std::max (64, points);
            step = (hi - lo) / (n - 1);
            inverse = 1.0 / step;
            y.assign ((size_t) n, 0.0);

            const double ipMost = ip0 + vp0 / rac;    // plate pulled all the way down

            const auto plateFor = [&] (double vg)
            {
                if (bypassed)
                    return solveRising ([&] (double ip) { return ip - T::plateCurrent (vp0 - (ip - ip0) * rac - vk0, vg - vk0); },
                                        0.0, ipMost);

                return solveRising ([&] (double ip) { return ip - T::plateCurrent (vp0 - (ip - ip0) * rac - ip * rk, vg - ip * rk); },
                                    0.0, ipMost);
            };

            for (int i = 0; i < n; ++i)
            {
                const double vin = lo + step * i;
                double ip = plateFor (vin);
                const double vk = bypassed ? vk0 : ip * rk;

                // Grid current: the grid can't rise much above the cathode, through what drives it.
                if (vin - vk > 0.0)
                {
                    // Bypassed, the cathode stands still and the grid's own equation is all there
                    // is; unbypassed, the cathode moves with the plate current, so it's solved too.
                    // Both rise with vg, from below zero (no grid current) to above it at vin.
                    double vg = vin;

                    if (bypassed)
                        vg = solveRising ([&] (double g) { return g - vin + spec.sourceR * T::gridCurrent (g - vk0); },
                                          vk0, vin, 1.0e-10, 60);
                    else
                        vg = solveRising ([&] (double g) { return g - vin + spec.sourceR * T::gridCurrent (g - plateFor (g) * rk); },
                                          0.0, vin, 1.0e-10, 60);

                    ip = plateFor (vg);
                }

                y[(size_t) i] = -(ip - ip0) * rac;
            }

            // Centred on the working point exactly, so silence stays silence.
            const double atZero = valueAt (0.0);

            for (auto& v : y)
                v -= atZero;

            // The antiderivative of the piecewise-linear curve, exactly.
            Y.assign ((size_t) n, 0.0);

            for (int i = 1; i < n; ++i)
                Y[(size_t) i] = Y[(size_t) i - 1] + 0.5 * step * (y[(size_t) i - 1] + y[(size_t) i]);

            slopeLo = (y[1] - y[0]) * inverse;
            slopeHi = (y[(size_t) n - 1] - y[(size_t) n - 2]) * inverse;

            // Small-signal gain, bypassed (the curve) and unbypassed (Rk in the loop).
            gainBypassed = (valueAt (1.0e-3) - valueAt (-1.0e-3)) / 2.0e-3;
            const double mu = gm * rplate;
            gainUnbypassed = -mu * rac / (rac + rplate + (mu + 1.0) * rk);
            built = true;
        }

        /** Plate swing for a grid voltage (linear between the points, straight on past the ends). */
        double valueAt (double x) const noexcept
        {
            x = sane (x);

            if (x <= lo) return y[0] + slopeLo * (x - lo);
            if (x >= hi) return y[(size_t) n - 1] + slopeHi * (x - hi);

            const double p = (x - lo) * inverse;
            const int i = std::min ((int) p, n - 2);
            const double t = p - i;
            return y[(size_t) i] + t * (y[(size_t) i + 1] - y[(size_t) i]);
        }

        /** Its antiderivative, consistent with valueAt everywhere. */
        double integralAt (double x) const noexcept
        {
            x = sane (x);

            if (x <= lo)
            {
                const double d = x - lo;
                return Y[0] + y[0] * d + 0.5 * slopeLo * d * d;
            }

            if (x >= hi)
            {
                const double d = x - hi;
                return Y[(size_t) n - 1] + y[(size_t) n - 1] * d + 0.5 * slopeHi * d * d;
            }

            const double p = (x - lo) * inverse;
            const int i = std::min ((int) p, n - 2);
            const double d = (p - i) * step;
            const double slope = (y[(size_t) i + 1] - y[(size_t) i]) * inverse;
            return Y[(size_t) i] + y[(size_t) i] * d + 0.5 * slope * d * d;
        }

        bool isBuilt() const noexcept               { return built; }
        double smallSignalGain() const noexcept     { return gainBypassed; }     // negative: it inverts
        double unbypassedGain() const noexcept      { return gainUnbypassed; }
        double plateResistance() const noexcept     { return rplate; }
        double transconductance() const noexcept    { return gm; }
        double acLoad() const noexcept              { return rac; }
        double quiescentPlate() const noexcept      { return vp0; }
        double quiescentCathode() const noexcept    { return vk0; }

    private:
        /** Never a NaN into the table (it would index anywhere), nor anything so far past its ends
            that the straight line on from them runs off to infinity: a bad sample upstream costs
            one wrong sample here, not a crash. */
        double sane (double x) const noexcept
        {
            if (! (x == x))
                return 0.0;

            return std::clamp (x, lo - 1.0e4, hi + 1.0e4);
        }

        std::vector<double> y, Y;
        double lo = -1.0, hi = 1.0, step = 1.0, inverse = 1.0, slopeLo = 0.0, slopeHi = 0.0;
        int n = 0;
        double ip0 = 0.0, vk0 = 0.0, vp0 = 0.0, rac = 1.0, gm = 0.0, rplate = 1.0;
        double gainBypassed = 0.0, gainUnbypassed = 0.0;
        bool built = false;
    };

    //==========================================================================
    /** First-order antiderivative anti-aliasing round any curve with valueAt and integralAt.
        A half-sample delay comes with it, as it always does. */
    struct Adaa
    {
        double x1 = 0.0, f1 = 0.0;

        template <typename Curve>
        double process (const Curve& curve, double x) noexcept
        {
            if (! (x == x))
                x = 0.0;

            const double d = x - x1;
            const double fx = curve.integralAt (x);
            const double out = std::abs (d) > 1.0e-6 ? (fx - f1) / d : curve.valueAt (0.5 * (x + x1));
            x1 = x;
            f1 = fx;
            return out;
        }

        template <typename Curve>
        void reset (const Curve& curve) noexcept    { x1 = 0.0; f1 = curve.integralAt (0.0); }
    };

    //==========================================================================
    /** A gain stage as it plays: its coupling cap, its cathode's shelf, the tube, its rolloff. */
    class TriodeStage
    {
    public:
        void setCurve (const TriodeCurve* c, const TriodeSpec& s) noexcept
        {
            curve = c;
            spec = s;
        }

        void prepare (double fs) noexcept
        {
            coupling.setHighpass (spec.couplingHz, fs);
            rolloff.setLowpass (spec.millerHz, fs);

            // The bypass cap: full gain above its corner, the unbypassed gain below it. As a
            // pole and a zero: H(s) = (s + wz) / (s + wp), wz = 1 / (Rk Ck), wp = wz Ab / Au.
            if (curve != nullptr && spec.cathodeC > 0.0 && curve->unbypassedGain() != 0.0)
            {
                const double wz = 1.0 / (spec.cathodeR * spec.cathodeC);
                const double ratio = curve->smallSignalGain() / curve->unbypassedGain();
                cathode.setZeroPole (wz, wz * ratio, fs);
            }
            else
            {
                cathode.setThrough();
            }

            reset();
        }

        void reset() noexcept
        {
            coupling.reset();
            cathode.reset();
            rolloff.reset();

            if (curve != nullptr && curve->isBuilt())
                adaa.reset (*curve);
        }

        double process (double x) noexcept
        {
            x = cathode.process (coupling.process (x));
            return rolloff.process (adaa.process (*curve, x));
        }

    private:
        const TriodeCurve* curve = nullptr;
        TriodeSpec spec;
        FirstOrder coupling, cathode, rolloff;
        Adaa adaa;
    };

    //==========================================================================
    /** A volume pot with a bright cap across its top half, as in the amp: at a setting a (0..1
        of its travel, after its taper) the divider passes a at low frequencies and everything
        above 1 / ((1 - a) R C). Exactly H(s) = (s + a wb) / (s + wb), wb = 1 / (a (1 - a) R C).

        The bright filter always runs, so switching BRIGHT fades between it and the plain pot
        (ten milliseconds) rather than starting a filter from nothing mid-note. */
    struct VolumePot
    {
        FirstOrder bright;
        double level = 1.0, mix = 0.0, mixTarget = 0.0, mixStep = 0.001;

        void set (double a, double potR, double brightC, bool brightOn, double fs) noexcept
        {
            // Kept off the very top of the track: there the cap has nothing left to bypass.
            a = std::clamp (a, 1.0e-4, 0.999);
            level = a;
            const double wz = 1.0 / ((1.0 - a) * potR * std::max (brightC, 1.0e-12));
            bright.setZeroPole (wz, wz / a, fs);
            mixTarget = brightOn && brightC > 0.0 ? 1.0 : 0.0;
            mixStep = 1.0 / (0.010 * fs);
        }

        double process (double x) noexcept
        {
            const double withCap = bright.process (x);

            if (mix != mixTarget)
                mix = mix < mixTarget ? std::min (mixTarget, mix + mixStep) : std::max (mixTarget, mix - mixStep);

            return x * level + mix * (withCap - x * level);
        }

        void reset() noexcept               { bright.reset(); mix = mixTarget; }
    };

    /** The standard audio taper of a pot ("A"): about 10% of the track at half its travel. */
    inline double audioTaper (double x, double atHalf = 0.1) noexcept
    {
        x = std::clamp (x, 0.0, 1.0);
        const double k = 2.0 * std::log ((1.0 - atHalf) / atHalf);
        return (std::exp (k * x) - 1.0) / (std::exp (k) - 1.0);
    }

    //==========================================================================
    /** log(cosh(u)), without overflow (and 0 for a NaN, so one can't spread). */
    inline double logCosh (double u) noexcept
    {
        if (! (u == u))
            return 0.0;

        const double a = std::min (std::abs (u), 1.0e6);
        return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994530942;
    }

    /** A cathode follower: it follows, until its grid catches its cathode on the way up (a
        firm, early limit) or it cuts off on the way down (a far softer, later one). */
    struct FollowerCurve
    {
        double up = 40.0, down = 120.0;     // volts

        double valueAt (double x) const noexcept
        {
            return x > 0.0 ? up * std::tanh (x / up) : down * std::tanh (x / down);
        }

        double integralAt (double x) const noexcept
        {
            return x > 0.0 ? up * up * logCosh (x / up) : down * down * logCosh (x / down);
        }
    };

    /** A soft limit for the phase inverter: tanh. */
    struct TanhCurve
    {
        double valueAt (double x) const noexcept       { return std::tanh (x); }
        double integralAt (double x) const noexcept    { return logCosh (x); }
    };

    /** The power tubes: a cubic that runs straight into its limit, a harder knee than tanh. */
    struct PowerCurve
    {
        double valueAt (double x) const noexcept
        {
            if (x >= 1.0)  return 1.0;
            if (x <= -1.0) return -1.0;
            return 1.5 * x - 0.5 * x * x * x;
        }

        double integralAt (double x) const noexcept
        {
            const double a = std::abs (x);
            if (a >= 1.0) return a - 0.375;
            const double x2 = x * x;
            return 0.75 * x2 - 0.125 * x2 * x2;
        }
    };
}
