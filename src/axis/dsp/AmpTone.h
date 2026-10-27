#pragma once

#include "AmpTube.h"

// ============================================================================
//  AXIS AMP — the tone stack.
//
//  The bass / mid / treble network of the classic American and British amps,
//  exactly: its transfer function from its own component values, as derived
//  by Yeh and Smith ("Discretization of the '59 Fender Bassman Tone Stack",
//  DAFx 2006), discretised with the bilinear transform.
//
//  The circuit (driven by a cathode follower, read at the treble wiper):
//
//      in ── C1 ── treble pot (R1) ── wiper = out
//                       │ bottom
//      in ── R4 ──┬── C2 ──┤
//                 │       bass pot (R2, as a variable resistor)
//                 │        │
//                 └── C3 ── mid pot (R3): C3 to its wiper, the bass pot to
//                                         its top, its bottom to ground
//
//  Checked two ways before it went in: the formula below agrees symbolically
//  with a nodal analysis of that circuit, term for term, and the bilinear
//  coefficients agree with an independent bilinear transform to 1e-15.
// ============================================================================
namespace axis::dsp::amp
{
    struct ToneStackParts
    {
        double r1 = 250.0e3, r2 = 1.0e6, r3 = 25.0e3, r4 = 56.0e3;   // treble, bass, mid pots; slope resistor
        double c1 = 250.0e-12, c2 = 20.0e-9, c3 = 20.0e-9;            // treble, bass, mid caps
        bool fixedMid = false;      // no mid pot: R3 is a fixed resistor (the mid pot always full up)
    };

    class ToneStack
    {
    public:
        void setParts (const ToneStackParts& p) noexcept     { parts = p; dirty = true; }

        void prepare (double sampleRate) noexcept
        {
            fs = sampleRate;
            dirty = true;
            update();
            reset();
        }

        /** The knobs, 0..1 of their travel. Bass and treble have the audio taper of the pots in
            the amps (gentler than a strict "A", so the middle of the knob sounds like the
            middle); the mid pot is linear. */
        void setKnobs (double bass, double mid, double treble) noexcept
        {
            const double l = audioTaper (bass, 0.2), t = audioTaper (treble, 0.2);
            const double m = parts.fixedMid ? 1.0 : std::clamp (mid, 0.0, 1.0);

            if (l != knobL || m != knobM || t != knobT)
            {
                knobL = l; knobM = m; knobT = t;
                dirty = true;
            }
        }

        /** New coefficients only when something changed. */
        void update() noexcept
        {
            if (! dirty || fs <= 0.0)
                return;

            dirty = false;

            // The pots are never quite at an end: a wiper resting on the track's end is still a
            // little way into it, and it keeps every term finite.
            const double l = std::clamp (knobL, 1.0e-3, 1.0), m = std::clamp (knobM, 1.0e-3, 1.0), t = std::clamp (knobT, 1.0e-3, 1.0);
            const double R1 = parts.r1, R2 = parts.r2, R3 = parts.r3, R4 = parts.r4;
            const double C1 = parts.c1, C2 = parts.c2, C3 = parts.c3;

            const double b1 = t*C1*R1 + m*C3*R3 + l*(C1*R2 + C2*R2) + (C1*R3 + C2*R3);

            const double b2 = t*(C1*C2*R1*R4 + C1*C3*R1*R4) - m*m*(C1*C3*R3*R3 + C2*C3*R3*R3)
                            + m*(C1*C3*R1*R3 + C1*C3*R3*R3 + C2*C3*R3*R3)
                            + l*(C1*C2*R1*R2 + C1*C2*R2*R4 + C1*C3*R2*R4)
                            + l*m*(C1*C3*R2*R3 + C2*C3*R2*R3)
                            + (C1*C2*R1*R3 + C1*C2*R3*R4 + C1*C3*R3*R4);

            const double b3 = l*m*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4)
                            - m*m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
                            + m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
                            + t*C1*C2*C3*R1*R3*R4 - t*m*C1*C2*C3*R1*R3*R4
                            + t*l*C1*C2*C3*R1*R2*R4;

            const double a0 = 1.0;

            const double a1 = (C1*R1 + C1*R3 + C2*R3 + C2*R4 + C3*R4) + m*C3*R3 + l*(C1*R2 + C2*R2);

            const double a2 = m*(C1*C3*R1*R3 - C2*C3*R3*R4 + C1*C3*R3*R3 + C2*C3*R3*R3)
                            + l*m*(C1*C3*R2*R3 + C2*C3*R2*R3)
                            - m*m*(C1*C3*R3*R3 + C2*C3*R3*R3)
                            + l*(C1*C2*R2*R4 + C1*C2*R1*R2 + C1*C3*R2*R4 + C2*C3*R2*R4)
                            + (C1*C2*R1*R4 + C1*C3*R1*R4 + C1*C2*R3*R4 + C1*C2*R1*R3 + C1*C3*R3*R4 + C2*C3*R3*R4);

            const double a3 = l*m*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4)
                            - m*m*(C1*C2*C3*R1*R3*R3 + C1*C2*C3*R3*R3*R4)
                            + m*(C1*C2*C3*R3*R3*R4 + C1*C2*C3*R1*R3*R3 - C1*C2*C3*R1*R3*R4)
                            + l*C1*C2*C3*R1*R2*R4
                            + C1*C2*C3*R1*R3*R4;

            // The bilinear transform (Yeh and Smith's own closed form, c = 2 fs).
            const double c = 2.0 * fs, c2 = c * c, c3 = c2 * c;

            const double B0 = -b1*c - b2*c2 - b3*c3;
            const double B1 = -b1*c + b2*c2 + 3.0*b3*c3;
            const double B2 =  b1*c + b2*c2 - 3.0*b3*c3;
            const double B3 =  b1*c - b2*c2 + b3*c3;

            const double A0 = -a0 - a1*c - a2*c2 - a3*c3;
            const double A1 = -3.0*a0 - a1*c + a2*c2 + 3.0*a3*c3;
            const double A2 = -3.0*a0 + a1*c + a2*c2 - 3.0*a3*c3;
            const double A3 = -a0 + a1*c - a2*c2 + a3*c3;

            nb[0] = B0 / A0; nb[1] = B1 / A0; nb[2] = B2 / A0; nb[3] = B3 / A0;
            na[1] = A1 / A0; na[2] = A2 / A0; na[3] = A3 / A0;
        }

        void reset() noexcept               { s1 = s2 = s3 = 0.0; }

        /** Third order, Direct Form II transposed, in double. */
        double process (double x) noexcept
        {
            const double y = nb[0] * x + s1;
            s1 = nb[1] * x - na[1] * y + s2;
            s2 = nb[2] * x - na[2] * y + s3;
            s3 = nb[3] * x - na[3] * y;
            return y;
        }

    private:
        ToneStackParts parts;
        double fs = 0.0;
        double knobL = 0.5, knobM = 0.5, knobT = 0.5;
        double nb[4] { 1.0, 0.0, 0.0, 0.0 }, na[4] { 1.0, 0.0, 0.0, 0.0 };
        double s1 = 0.0, s2 = 0.0, s3 = 0.0;
        bool dirty = true;
    };
}
