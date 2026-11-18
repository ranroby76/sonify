// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakFilters.h
#pragma once

#include <algorithm>
#include <cmath>

//==============================================================================
//  ZodiakFilters.h  —  the filter node's character models, after the families
//  VPS Avenger offers (our own maths: Avenger doesn't publish its), and the
//  CRYING filters trance and rave leads scream through.
//
//      ANA LP 12 / LP 24 / HP 24   analog-smooth state-variable, its states
//                                  softly saturated; cheap and round
//      NEX LP 12 / LP 24 / HP 24   fast and bright (Nexus-like): the resonant
//                                  band added back, the top end lifted
//      POW LP 24                   a ladder with no level compensation: the
//                                  resonance bites, the body thins
//      POW HP                      closer to 6 dB, with a strong resonant peak
//      COS LP 24 / HP 12           bilinear biquads, the resonance held back:
//                                  damped and narrower, gentler slopes
//      PXL LP 24 / BP              clean and clear: resonance volume-compensated
//      SSM BP / BS                 two filters a BAND apart (the node's DRIVE
//                                  sets the distance: 0.1 .. 3 octaves)
//      IRS LP 12 / LP 24           impulse-invariant poles (not bilinear): a
//                                  softer top, damped at high resonance
//      Scream LP / Scream HP       the MS-20's Korg 35, its feedback through
//                                  tanh: screams, then self-oscillates
//      Cry BP                      two saturating resonant band-passes a
//                                  sixth apart: the vowel-like cry of a trance
//                                  lead sweeping
//      Rave 24                     a driven ladder whose feedback loses its
//                                  bass as the resonance rises: thin and
//                                  piercing, the acid/rave scream
//
//  One channel each; setParams every few samples (cutoff Hz, res 0..1, drive
//  0..1), process every sample.  Plain C++, no allocation.
//==============================================================================

namespace zfl
{
    enum Kind : int
    {
        anaLp12 = 0, anaLp24, anaHp24,
        nexLp12, nexLp24, nexHp24,
        powLp24, powHp,
        cosLp24, cosHp12,
        pxlLp24, pxlBp,
        ssmBp, ssmBs,
        irsLp12, irsLp24,
        screamLp, screamHp, cryBp, rave24,
        numKinds
    };

    inline const char* const names[numKinds]
    {
        "ANA LP 12", "ANA LP 24", "ANA HP 24",
        "NEX LP 12", "NEX LP 24", "NEX HP 24",
        "POW LP 24", "POW HP",
        "COS LP 24", "COS HP 12",
        "PXL LP 24", "PXL BP",
        "SSM BP", "SSM BS",
        "IRS LP 12", "IRS LP 24",
        "Scream LP", "Scream HP", "Cry BP", "Rave 24"
    };

    constexpr float pi = 3.14159265358979f;

    inline float sat (float x) noexcept           // soft, rational: ~tanh, cheap
    {
        x = std::clamp (x, -3.0f, 3.0f);
        return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
    }

    //==========================================================================
    struct Svf               // TPT state-variable (Zavalishin)
    {
        float g = 0.1f, k = 1.4f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        float lp = 0, bp = 0, hp = 0;

        void set (float gIn, float kIn) noexcept
        {
            g = gIn; k = kIn;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void tick (float v0, bool soft = false) noexcept
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            if (soft) { ic1 = 3.0f * sat (ic1 / 3.0f); ic2 = 3.0f * sat (ic2 / 3.0f); }
            lp = v2; bp = v1; hp = v0 - k * v1 - v2;
        }

        void clear() noexcept   { ic1 = ic2 = lp = bp = hp = 0.0f; }
    };

    struct OnePole           // TPT one-pole; fb = beta * z (the K35's feedback taps)
    {
        float a = 0.5f, z = 0.0f, beta = 0.0f;
        float lpOut (float x) noexcept   { const float v = (x - z) * a; const float y = v + z; z = y + v; return y; }
        float hpOut (float x) noexcept   { return x - lpOut (x); }
        float fb() const noexcept        { return beta * z; }
    };

    struct Biquad            // TDF II
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0;
        float tick (float x) noexcept
        {
            const float y = b0 * x + s1;
            s1 = b1 * x - a1 * y + s2;
            s2 = b2 * x - a2 * y;
            return y;
        }
        void clear() noexcept   { s1 = s2 = 0.0f; }

        void lowpass (float w, float q) noexcept           // RBJ
        {
            const float c = std::cos (w), al = std::sin (w) / (2.0f * q), n = 1.0f / (1.0f + al);
            b0 = (1.0f - c) * 0.5f * n; b1 = (1.0f - c) * n; b2 = b0;
            a1 = -2.0f * c * n; a2 = (1.0f - al) * n;
        }
        void highpass (float w, float q) noexcept
        {
            const float c = std::cos (w), al = std::sin (w) / (2.0f * q), n = 1.0f / (1.0f + al);
            b0 = (1.0f + c) * 0.5f * n; b1 = -(1.0f + c) * n; b2 = b0;
            a1 = -2.0f * c * n; a2 = (1.0f - al) * n;
        }
        void invariantLowpass (float w, float q) noexcept   // impulse-invariant 2-pole, unity at DC
        {
            q = std::max (q, 0.51f);
            const float sigma = w / (2.0f * q);
            const float wd = w * std::sqrt (std::max (1.0e-6f, 1.0f - 1.0f / (4.0f * q * q)));
            const float r = std::exp (-sigma);
            a1 = -2.0f * r * std::cos (wd);
            a2 = r * r;
            b0 = 1.0f + a1 + a2; b1 = 0.0f; b2 = 0.0f;
        }
    };

    //==========================================================================
    class Model
    {
    public:
        void prepare (double sampleRate) noexcept   { sr = (float) sampleRate; reset(); }

        void setKind (int k) noexcept
        {
            k = std::clamp (k, 0, numKinds - 1);
            if (k != kind) { kind = k; reset(); }
        }

        void reset() noexcept
        {
            for (auto& s : svf) s.clear();
            for (auto& b : bq) b.clear();
            for (auto& p : pole) p.z = 0.0f;
            fbHp.z = 0.0f;
            lastY = 0.0f;
        }

        void setParams (float cutoff, float res, float drive) noexcept
        {
            cutoff = std::clamp (cutoff, 20.0f, sr * 0.45f);
            res = std::clamp (res, 0.0f, 1.0f);
            drive = std::clamp (drive, 0.0f, 1.0f);
            const float w = 2.0f * pi * cutoff / sr;
            const float g = std::tan (0.5f * w);
            r = res;
            satAmt = 1.0f + 3.0f * drive;

            switch (kind)
            {
                case anaLp12: case anaHp24: case anaLp24:
                    svf[0].set (g, kind == anaLp24 || kind == anaHp24 ? 1.414f : 2.0f - 1.9f * res);
                    svf[1].set (g, 2.0f - 1.9f * res);
                    break;
                case nexLp12: case nexLp24: case nexHp24:
                    svf[0].set (g, kind == nexLp12 ? 2.0f - 1.94f * res : 1.2f);
                    svf[1].set (g, 2.0f - 1.94f * res);
                    svf[2].set (std::tan (pi * std::min (sr * 0.45f, 9000.0f) / sr), 1.0f);    // the top end's lift
                    break;
                case powLp24:
                {
                    const float G = g / (1.0f + g);
                    for (auto& p : pole) p.a = G;
                    k4 = 4.2f * std::pow (res, 1.2f);
                    break;
                }
                case powHp:
                    pole[0].a = g / (1.0f + g);
                    svf[0].set (g, 2.0f - 1.95f * res);
                    break;
                case cosLp24:
                {
                    const float q = 0.5f + 2.5f * std::pow (res, 1.5f);
                    bq[0].lowpass (w, 0.54f);
                    bq[1].lowpass (w, q);
                    break;
                }
                case cosHp12:
                    bq[0].highpass (w, 0.5f + 2.5f * std::pow (res, 1.5f));
                    break;
                case pxlLp24: case pxlBp:
                    svf[0].set (g, kind == pxlBp ? 2.0f - 1.97f * res : 1.414f);
                    svf[1].set (g, 2.0f - 1.97f * res);
                    comp = 1.0f / (1.0f + 0.6f * res * res * 3.0f);
                    break;
                case ssmBp: case ssmBs:
                {
                    const float band = 0.1f + 2.9f * drive;                    // octaves apart
                    const float lo = std::clamp (cutoff * std::exp2 (-0.5f * band), 20.0f, sr * 0.45f);
                    const float hi = std::clamp (cutoff * std::exp2 (0.5f * band), 20.0f, sr * 0.45f);
                    const float k = 2.0f - 1.85f * res;
                    svf[0].set (std::tan (pi * lo / sr), k);
                    svf[1].set (std::tan (pi * hi / sr), k);
                    satAmt = 1.0f;
                    break;
                }
                case irsLp12: case irsLp24:
                {
                    const float q = 0.6f + 4.0f * res * (1.0f - 0.45f * res);     // damped up high
                    if (kind == irsLp24) { bq[0].invariantLowpass (w, 0.6f); bq[1].invariantLowpass (w, q); }
                    else                   bq[0].invariantLowpass (w, q);
                    break;
                }
                case screamLp: case screamHp:
                {
                    // Korg 35 (Pirkle's ZDF form)
                    const float a = g / (1.0f + g);
                    for (auto& p : pole) p.a = a;
                    k35 = 0.01f + 1.97f * res;
                    a0 = 1.0f / (1.0f - k35 * a + k35 * a * a);
                    if (kind == screamLp) { pole[1].beta = (k35 - k35 * a) / (1.0f + g); pole[2].beta = -1.0f / (1.0f + g); }
                    else                  { pole[1].beta = -a / (1.0f + g);               pole[2].beta = 1.0f / (1.0f + g); }
                    break;
                }
                case cryBp:
                {
                    const float k = 2.0f - 1.97f * res;
                    svf[0].set (g, k);
                    svf[1].set (std::tan (pi * std::min (cutoff * 1.68f, sr * 0.45f) / sr), k * 1.2f);
                    comp = 1.0f + 2.0f * res;
                    break;
                }
                case rave24:
                {
                    const float G = g / (1.0f + g);
                    for (auto& p : pole) p.a = G;
                    k4 = 5.6f * res;
                    const float fh = std::clamp (cutoff * (0.05f + 0.45f * res), 20.0f, sr * 0.45f);  // the bass the feedback loses
                    const float gh = std::tan (pi * fh / sr);
                    fbHp.a = gh / (1.0f + gh);
                    break;
                }
                default: break;
            }
        }

        float process (float x) noexcept
        {
            switch (kind)
            {
                case anaLp12:  svf[0].tick (sat (x * 0.9f) / 0.9f, true); return svf[0].lp;
                case anaLp24:  svf[0].tick (sat (x * 0.9f) / 0.9f, true); svf[1].tick (svf[0].lp, true); return svf[1].lp;
                case anaHp24:  svf[0].tick (x, true); svf[1].tick (svf[0].hp, true); return svf[1].hp;

                case nexLp12:
                case nexLp24:
                {
                    float y;
                    if (kind == nexLp12) { svf[0].tick (x); y = svf[0].lp + 0.35f * (0.4f + r) * svf[0].bp; }
                    else { svf[0].tick (x); svf[1].tick (svf[0].lp); y = svf[1].lp + 0.35f * (0.4f + r) * svf[1].bp; }
                    svf[2].tick (y);
                    return y + 0.25f * svf[2].hp;                  // the treble a little up
                }
                case nexHp24:
                {
                    svf[0].tick (x); svf[1].tick (svf[0].hp);
                    const float y = svf[1].hp + 0.35f * (0.4f + r) * svf[1].bp;
                    svf[2].tick (y);
                    return y + 0.25f * svf[2].hp;
                }

                case powLp24:
                {
                    // Linear ladder, solved for its feedback; no passband compensation
                    const float G = pole[0].a;
                    const float S = G * G * G * (pole[0].z * (1.0f - G)) + G * G * (pole[1].z * (1.0f - G))
                                  + G * (pole[2].z * (1.0f - G)) + pole[3].z * (1.0f - G);
                    const float G4 = G * G * G * G;
                    float in = sat ((x * (1.0f + 0.4f * k4) - k4 * S) / (1.0f + k4 * G4));    // a little of the body given back
                    for (auto& p : pole) in = p.lpOut (in);
                    return in;
                }
                case powHp:
                {
                    const float hp6 = pole[0].hpOut (x);
                    svf[0].tick (x);
                    return 0.6f * hp6 + 0.4f * svf[0].hp + 0.6f * r * svf[0].bp;
                }

                case cosLp24:  return bq[1].tick (bq[0].tick (x));
                case cosHp12:  return bq[0].tick (x);

                case pxlLp24:  svf[0].tick (x); svf[1].tick (svf[0].lp); return svf[1].lp * comp;
                case pxlBp:    svf[0].tick (x); svf[1].tick (svf[0].bp * svf[0].k); return svf[1].bp * svf[1].k;

                case ssmBp:
                {
                    svf[0].tick (x); svf[1].tick (x);
                    return 0.5f * (svf[0].bp * svf[0].k + svf[1].bp * svf[1].k) * 1.4f;
                }
                case ssmBs:
                {
                    svf[0].tick (x);
                    const float n1 = x - svf[0].k * svf[0].bp;
                    svf[1].tick (n1);
                    return n1 - svf[1].k * svf[1].bp;
                }

                case irsLp12:  return bq[0].tick (x);
                case irsLp24:  return bq[1].tick (bq[0].tick (x));

                case screamLp:
                {
                    const float y1 = pole[0].lpOut (x);
                    const float s35 = pole[2].fb() + pole[1].fb();
                    const float u = std::tanh (satAmt * a0 * (y1 + s35));
                    float y = k35 * pole[1].lpOut (u);
                    pole[2].hpOut (y);
                    return y / k35;
                }
                case screamHp:
                {
                    const float y1 = pole[0].hpOut (x);
                    const float s35 = pole[1].fb() + pole[2].fb();
                    const float u = std::tanh (satAmt * a0 * (y1 + s35));
                    const float y = pole[1].hpOut (u);
                    pole[2].lpOut (k35 * y);
                    return y;
                }

                case cryBp:
                {
                    // Scaled down into the saturating states, back up after: DRIVE pushes it into the grit
                    const float in = 0.3f * satAmt;
                    svf[0].tick (x * in, true);
                    svf[1].tick (x * in, true);
                    const float y = (svf[0].bp * svf[0].k + 0.55f * svf[1].bp * svf[1].k) * comp / in;
                    return 2.0f * sat (0.5f * y);
                }

                case rave24:
                {
                    // Ladder; the feedback high-passed (thinner as it screams), through tanh
                    const float fbIn = fbHp.hpOut (lastY);
                    float in = std::tanh (satAmt * (x * (1.0f + 0.5f * k4) - k4 * fbIn)) / satAmt;
                    for (auto& p : pole) in = p.lpOut (in);
                    lastY = in;
                    return in;
                }
                default: return x;
            }
        }

    private:
        float sr = 48000.0f, r = 0.0f, satAmt = 1.0f, comp = 1.0f;
        float k4 = 0.0f, k35 = 0.01f, a0 = 1.0f, lastY = 0.0f;
        int kind = 0;
        Svf svf[3];
        Biquad bq[2];
        OnePole pole[4], fbHp;
    };
}
