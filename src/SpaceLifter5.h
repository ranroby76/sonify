// D:\workspace\Fanan_web_hub\src\SpaceLifter5.h
//
// SpaceLifter 5's reverb engine (from the NebulaVerb project). Header-only, no JUCE dependency, so the same code
// runs here and in NebulaVerb's offline IR renderer (tools/render_ir.cpp), which tunes it.
//
// Signal flow:
//   in -> pre-delay -+-> early reflections (tapped delay) ----------------+
//                    +-> input diffusion -> 16-line FDN (3-band decay,    +-> low-cut -> high-cut -> gate -> mix
//                         ^                  modulated, in-loop allpass) -+
//                         +---- shimmer (split HP -> pitch shift -> soft clip) <-+
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace SpaceLifter5
{
constexpr int kNumLines = 16;
constexpr int kNumTypes = 8;
constexpr float kPi = 3.14159265358979323846f;

struct TypeDef
{
    const char* name;
    float minMs, maxMs;          // FDN delay-line range
    float lineDiff;              // in-loop allpass coefficient (echo density inside the tank)
    float inDiff, inDiffScale;   // input allpass coefficient and length scale
    float lowMult, lowFc;        // low-band decay multiplier (asymptote) and corner
    float hiMult, hiFc;          // high-band decay multiplier (asymptote) at damping = 50 %, and corner
    float airCapSec;             // absolute ceiling on high-band decay time (air absorption)
    float erStartMs, erSpanMs, erGain;
    int   erTaps;
    float modDepth;              // peak delay modulation in samples @48 kHz when Modulation = 100 %
    float modRate;               // slowest LFO rate in Hz
    float lateGain;
    uint32_t seed;
};

// Band multipliers were fitted to per-octave decay measurements of the reference halls
// (see tools/targets.json). Delay ranges / diffusion set how fast the tail becomes smooth.
inline const std::array<TypeDef, kNumTypes>& typeTable()
{
    static const std::array<TypeDef, kNumTypes> t { {
        //  name          min   max   lDif  iDif  iScl  lowM  lowFc  hiM   hiFc    air   erS  erSpan erG  taps  modD  modR  late  seed
        { "Room",          9.f,  41.f, 0.55f, 0.65f, 0.60f, 1.05f, 200.f, 0.85f, 6000.f, 10.f,  3.f,  32.f, 0.70f, 10,  10.f, 0.31f, 1.00f, 11u },
        { "Plate",         5.f,  31.f, 0.66f, 0.72f, 0.45f, 1.00f, 200.f, 0.88f, 7000.f, 12.f,  1.f,  14.f, 0.40f,  8,  12.f, 0.43f, 1.00f, 23u },
        { "Hall",         23.f,  89.f, 0.50f, 0.68f, 1.00f, 1.08f, 300.f, 0.59f, 3000.f, 10.f,  8.f,  82.f, 0.55f, 14,  18.f, 0.23f, 1.00f, 37u },
        { "Vocal Hall",   19.f,  73.f, 0.52f, 0.70f, 0.85f, 1.40f, 250.f, 0.95f, 8000.f, 12.f,  6.f,  60.f, 0.50f, 12,  16.f, 0.27f, 1.00f, 41u },
        { "Arena",        31.f, 113.f, 0.36f, 0.60f, 1.30f, 1.00f, 300.f, 0.77f, 4000.f, 12.f, 12.f, 130.f, 0.60f, 16,  20.f, 0.19f, 1.00f, 53u },
        { "Cathedral",    37.f, 131.f, 0.48f, 0.68f, 1.40f, 1.15f, 300.f, 0.64f, 2500.f, 10.f, 14.f, 150.f, 0.50f, 16,  22.f, 0.17f, 1.00f, 67u },
        { "Space",        41.f, 149.f, 0.58f, 0.72f, 1.50f, 0.80f, 150.f, 0.80f, 6000.f, 16.f, 10.f, 110.f, 0.40f, 12,  34.f, 0.13f, 1.00f, 79u },
        { "Nebula",       47.f, 173.f, 0.62f, 0.75f, 1.80f, 1.10f, 250.f, 0.60f, 4000.f, 20.f, 16.f, 160.f, 0.30f, 12,  44.f, 0.11f, 1.00f, 97u },
    } };
    return t;
}

struct Params
{
    int   type        = 2;
    float mix         = 0.30f;    // 0..1
    float predelayMs  = 20.0f;    // 0..500
    float decaySec    = 3.0f;     // 0.2..30 (mid-band RT60)
    float modulation  = 0.35f;    // 0..1
    float earlyLate   = 0.55f;    // 0 = early only, 1 = late only
    float lowCutHz    = 20.0f;
    float highCutHz   = 20000.0f;
    float damping     = 0.50f;    // 0..1, 0.5 = the type's natural high-frequency decay
    bool  shimmer     = false;
    float feedback    = 0.40f;    // 0..1
    float shiftSemis  = 12.0f;    // -24..+24
    float splitHz     = 400.0f;   // only content above this is shimmered
    bool  gate        = false;
    float thresholdDb = -30.0f;
    float attackMs    = 5.0f;
    float holdMs      = 250.0f;
    float releaseMs   = 200.0f;
    bool  bypass      = false;
};

//==============================================================================
namespace detail
{
inline int nextPow2 (int v) { int p = 1; while (p < v) p <<= 1; return p; }

inline bool isPrime (int n)
{
    if (n < 2) return false;
    for (int i = 2; (long long) i * i <= n; ++i) if (n % i == 0) return false;
    return true;
}
inline int nearestPrime (int n) { n = std::max (n, 3); for (int d = 0;; ++d) { if (isPrime (n + d)) return n + d; if (n - d > 2 && isPrime (n - d)) return n - d; } }

struct Lcg
{
    uint32_t s;
    explicit Lcg (uint32_t seed) : s (seed * 2654435761u + 12345u) {}
    float next() { s = s * 1664525u + 1013904223u; return (float) ((s >> 8) & 0xFFFFFF) / 16777216.0f; }   // 0..1
};

struct Delay
{
    std::vector<float> buf; int mask = 0, w = 0;
    void resize (int minLen) { int n = nextPow2 (minLen + 8); buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (int d) const { return buf[(size_t) ((w - d) & mask)]; }                  // d >= 1 : sample written d writes ago
    inline float readLin (float d) const
    {
        int i = (int) d; float f = d - (float) i;
        float a = buf[(size_t) ((w - i) & mask)], b = buf[(size_t) ((w - i - 1) & mask)];
        return a + f * (b - a);
    }
    // 5th-order Lagrange, central interval. Odd-order central Lagrange never exceeds unity gain, so it is
    // safe inside a feedback loop, and 5th order keeps the loss at 8 kHz negligible (3rd order audibly
    // shortened the high-frequency decay once the lines were modulated).
    inline float readLagrange (float d) const
    {
        const int i = (int) d; const float f = d - (float) i;
        const float a = f + 2.0f, b = f + 1.0f, c = f, e = f - 1.0f, g = f - 2.0f, h = f - 3.0f;
        const float ab = a * b, gh = g * h, ce = c * e;
        const float c0 = b * ce * gh * (-1.0f / 120.0f);
        const float c1 = a * ce * gh * (1.0f / 24.0f);
        const float c2 = ab * e * gh * (-1.0f / 12.0f);
        const float c3 = ab * c * gh * (1.0f / 12.0f);
        const float c4 = ab * ce * h * (-1.0f / 24.0f);
        const float c5 = ab * ce * g * (1.0f / 120.0f);
        return c0 * buf[(size_t) ((w - i + 2) & mask)] + c1 * buf[(size_t) ((w - i + 1) & mask)]
             + c2 * buf[(size_t) ((w - i) & mask)]     + c3 * buf[(size_t) ((w - i - 1) & mask)]
             + c4 * buf[(size_t) ((w - i - 2) & mask)] + c5 * buf[(size_t) ((w - i - 3) & mask)];
    }
};

struct Allpass
{
    Delay d; int len = 1; float g = 0.5f;
    void resize (int maxLen) { d.resize (maxLen + 2); }
    void clear() { d.clear(); }
    inline float process (float x)
    {
        float z = d.read (len);
        float y = z - g * x;
        d.write (x + g * y);
        return y;
    }
};

// First-order shelves. Gain G applies at DC (low shelf) or Nyquist (high shelf), unity at the other end.
struct Shelf1
{
    float b0 = 1, b1 = 0, a1 = 0, x1 = 0, y1 = 0;
    void clear() { x1 = y1 = 0; }
    void setLow (float G, float t)   // t = tan(pi*fc/fs)
    {
        float s = std::sqrt (G), a = t / s, b = t * s, n = 1.0f / (1.0f + a);
        b0 = (1.0f + b) * n; b1 = (b - 1.0f) * n; a1 = (a - 1.0f) * n;
    }
    void setHigh (float G, float t)
    {
        float s = std::sqrt (G), n = 1.0f / (1.0f / s + t);
        b0 = (s + t) * n; b1 = (t - s) * n; a1 = (t - 1.0f / s) * n;
    }
    inline float process (float x) { float y = b0 * x + b1 * x1 - a1 * y1; x1 = x; y1 = y; return y; }
};

// Topology-preserving state-variable filter: stays stable while its cutoff is being moved.
struct Svf
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1.41421356f;
    void clear() { ic1 = ic2 = 0; }
    void set (float fc, float fs)
    {
        float g = std::tan (kPi * std::min (fc, 0.45f * fs) / fs);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void tick (float x, float& lp, float& hp)
    {
        float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; hp = x - k * v1 - v2;
    }
    inline float lowpass (float x)  { float lp, hp; tick (x, lp, hp); return lp; }
    inline float highpass (float x) { float lp, hp; tick (x, lp, hp); return hp; }
};

// Two crossfading delay taps (Hann windows): simple, artefacts are smoothed by the reverb tank.
struct PitchShifter
{
    Delay d; float phase = 0, win = 2880, inc = 0;
    void prepare (float fs) { win = 0.060f * fs; d.resize ((int) win + 16); phase = 0; }
    void clear() { d.clear(); phase = 0; }
    void setSemitones (float st) { inc = (1.0f - std::pow (2.0f, st / 12.0f)) / win; }
    inline float process (float x)
    {
        d.write (x);
        float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
        float g1 = 0.5f - 0.5f * std::cos (2.0f * kPi * phase);
        float y = g1 * d.readLin (1.0f + phase * win) + (1.0f - g1) * d.readLin (1.0f + p2 * win);
        phase += inc; if (phase >= 1.0f) phase -= 1.0f; else if (phase < 0.0f) phase += 1.0f;
        return y;
    }
};

inline void hadamard16 (float* v)
{
    for (int h = 1; h < 16; h <<= 1)
        for (int i = 0; i < 16; i += h << 1)
            for (int j = i; j < i + h; ++j) { float a = v[j], b = v[j + h]; v[j] = a + b; v[j + h] = a - b; }
    for (int i = 0; i < 16; ++i) v[i] *= 0.25f;
}

inline float hadamardSign (int row, int col)   // +-1 entry of the 16x16 Hadamard matrix
{
    int x = row & col, c = 0; while (x) { c ^= (x & 1); x >>= 1; }
    return c ? -1.0f : 1.0f;
}
} // namespace detail

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const float maxLineMs = 180.0f;
        const int modMargin = (int) (64.0f * fs / 48000.0f) + 16;
        for (int i = 0; i < kNumLines; ++i)
        {
            line[i].resize ((int) (maxLineMs * 0.001f * fs) + modMargin);
            lineAp[i].resize ((int) (maxLineMs * 0.4f * 0.001f * fs) + 8);
        }
        for (int c = 0; c < 2; ++c)
        {
            pre[c].resize ((int) (0.52f * fs));
            er[c].resize ((int) (0.20f * fs));
            for (auto& a : inAp[c]) a.resize ((int) (0.040f * fs));
            shifter[c].prepare (fs);
        }
        smoothCoef   = 1.0f - std::exp (-1.0f / (0.020f * fs));           // per-sample gain smoothing (20 ms)
        ctlCoef      = 1.0f - std::exp (-(float) kCtl / (0.060f * fs));   // control-rate smoothing (60 ms)
        fadeStep     = 1.0f / (0.030f * fs);
        prepared = true;
        applyType (target.type);
        snapControls();
        reset();
    }

    void reset()
    {
        for (int i = 0; i < kNumLines; ++i) { line[i].clear(); lineAp[i].clear(); lowSh[i].clear(); hiSh[i].clear(); }
        for (int c = 0; c < 2; ++c)
        {
            pre[c].clear(); er[c].clear(); for (auto& a : inAp[c]) a.clear();
            shifter[c].clear(); lowCut[c].clear(); highCut[c].clear(); splitHp[c].clear();
            shimLp[c] = 0; lateOut[c] = 0;
        }
        gateGain = cur.gate ? 0.0f : 1.0f; gateHold = 0;
        ctlCount = 0;
    }

    void setParams (const Params& p) { target = p; target.type = std::clamp (p.type, 0, kNumTypes - 1); }

    bool isPrepared() const { return prepared; }
    int  currentType() const { return activeType; }

    // In-place stereo processing.
    void process (float* L, float* R, int n)
    {
        if (! prepared) return;

        // type switch: fade the wet signal out, rebuild the tank, fade back in
        if (target.type != activeType && fadeDir == 0) fadeDir = -1;

        const float pdTarget = std::clamp (target.predelayMs, 0.0f, 500.0f) * 0.001f * fs;
        const float mixT = target.bypass ? 0.0f : std::clamp (target.mix, 0.0f, 1.0f);
        const float dryT = std::cos (mixT * 0.5f * kPi), wetT = std::sin (mixT * 0.5f * kPi);
        const float el = std::clamp (target.earlyLate, 0.0f, 1.0f);
        const float earlyT = std::cos (el * 0.5f * kPi) * 1.41421356f, lateT = std::sin (el * 0.5f * kPi) * 1.41421356f;
        // With no pitch shift the shimmer path is plain feedback around the tank, which rings at single
        // frequencies when the decay is long, so the return fades out as Shift approaches 0.
        const float shimT = target.shimmer ? std::clamp (target.feedback, 0.0f, 1.0f) * std::min (1.0f, std::abs (target.shiftSemis)) : 0.0f;
        const float inT = target.bypass ? 0.0f : 1.0f;

        const float thr = std::pow (10.0f, target.thresholdDb / 20.0f);
        const float atkStep = 1.0f / std::max (1.0f, target.attackMs * 0.001f * fs);
        const float relStep = 1.0f / std::max (1.0f, target.releaseMs * 0.001f * fs);
        const int holdSamples = (int) (target.holdMs * 0.001f * fs);

        for (int s = 0; s < n; ++s)
        {
            if (ctlCount <= 0) { updateControls(); ctlCount = kCtl; }
            --ctlCount;

            const float dryL = L[s], dryR = R[s];

            // smoothed gains
            dryG += smoothCoef * (dryT - dryG);     wetG += smoothCoef * (wetT - wetG);
            earlyG += smoothCoef * (earlyT - earlyG); lateG += smoothCoef * (lateT - lateG);
            shimG += smoothCoef * (shimT - shimG);  inG += smoothCoef * (inT - inG);
            pdCur += smoothCoef * 0.25f * (pdTarget - pdCur);

            // pre-delay
            pre[0].write (dryL * inG); pre[1].write (dryR * inG);
            const float xl = pre[0].readLin (1.0f + pdCur), xr = pre[1].readLin (1.0f + pdCur);

            // early reflections
            er[0].write (xl); er[1].write (xr);
            float eL = 0, eR = 0;
            for (int k = 0; k < erCount; ++k)
            {
                eL += erGainL[k] * (0.8f * er[0].read (erTapL[k]) + 0.35f * er[1].read (erTapL[k] + 7));
                eR += erGainR[k] * (0.8f * er[1].read (erTapR[k]) + 0.35f * er[0].read (erTapR[k] + 5));
            }

            // shimmer return (uses last sample's late output)
            float sL = 0, sR = 0;
            if (shimG > 1.0e-4f)
            {
                float hl = splitHp[0].highpass (lateOut[0]), hr = splitHp[1].highpass (lateOut[1]);
                shimLp[0] += shimLpCoef * (hl - shimLp[0]); shimLp[1] += shimLpCoef * (hr - shimLp[1]);
                // cross-fed so the shimmer widens instead of stacking in one channel
                sL = std::tanh (shifter[0].process (shimLp[1]) * shimDrive) * shimG * shimLoop;
                sR = std::tanh (shifter[1].process (shimLp[0]) * shimDrive) * shimG * shimLoop;
            }

            // input diffusion
            float dl = xl + sL, dr = xr + sR;
            for (auto& a : inAp[0]) dl = a.process (dl);
            for (auto& a : inAp[1]) dr = a.process (dr);

            // feedback delay network
            float y[kNumLines];
            float oL = 0, oR = 0;
            for (int i = 0; i < kNumLines; ++i)
            {
                // rotate LFO (coupled form)
                const float c = lfoC[i], sn = lfoS[i];
                const float nx = lfoX[i] * c - lfoY[i] * sn, ny = lfoX[i] * sn + lfoY[i] * c;
                lfoX[i] = nx; lfoY[i] = ny;

                float v = line[i].readLagrange (baseLen[i] + modDepthCur * lfoSign[i] * nx);
                v = hiSh[i].process (lowSh[i].process (v)) * gMid[i];
                v = lineAp[i].process (v);
                y[i] = v;
                oL += outL[i] * v; oR += outR[i] * v;
            }
            float fb[kNumLines];
            for (int i = 0; i < kNumLines; ++i) fb[i] = y[i];
            detail::hadamard16 (fb);
            for (int i = 0; i < kNumLines; ++i) line[i].write (fb[i] + injL[i] * dl + injR[i] * dr);

            oL *= lateNorm; oR *= lateNorm;
            lateOut[0] = oL; lateOut[1] = oR;

            // wet sum, filters
            float wl = earlyG * erLevel * eL + lateG * oL;
            float wr = earlyG * erLevel * eR + lateG * oR;
            wl = highCut[0].lowpass (lowCut[0].highpass (wl));
            wr = highCut[1].lowpass (lowCut[1].highpass (wr));

            // gate, keyed from the dry input
            if (target.gate)
            {
                const float key = std::max (std::abs (dryL), std::abs (dryR));
                if (key > thr) gateHold = holdSamples;
                if (gateHold > 0) { --gateHold; gateGain = std::min (1.0f, gateGain + atkStep); }
                else              gateGain = std::max (0.0f, gateGain - relStep);
            }
            else gateGain = std::min (1.0f, gateGain + atkStep);
            const float gg = gateGain * gateGain * (3.0f - 2.0f * gateGain);   // smoothstep: no corners in the gate curve

            // type-switch fade
            if (fadeDir != 0)
            {
                fade += (float) fadeDir * fadeStep;
                if (fade <= 0.0f) { fade = 0.0f; applyType (target.type); clearTank(); fadeDir = 1; }
                else if (fade >= 1.0f) { fade = 1.0f; fadeDir = 0; }
            }

            const float wg = wetG * gg * fade;
            L[s] = dryL * dryG + wl * wg;
            R[s] = dryR * dryG + wr * wg;
        }

        // keep LFO amplitude from drifting
        for (int i = 0; i < kNumLines; ++i)
        {
            const float m = lfoX[i] * lfoX[i] + lfoY[i] * lfoY[i];
            if (m > 0.0f) { const float k = 1.0f / std::sqrt (m); lfoX[i] *= k; lfoY[i] *= k; }
        }
    }

private:
    static constexpr int kCtl = 32;   // control-rate interval in samples

    void clearTank()
    {
        for (int i = 0; i < kNumLines; ++i) { line[i].clear(); lineAp[i].clear(); lowSh[i].clear(); hiSh[i].clear(); }
        for (int c = 0; c < 2; ++c) { for (auto& a : inAp[c]) a.clear(); er[c].clear(); shifter[c].clear(); shimLp[c] = 0; lateOut[c] = 0; splitHp[c].clear(); }
    }

    void applyType (int t)
    {
        activeType = std::clamp (t, 0, kNumTypes - 1);
        const TypeDef& td = typeTable()[(size_t) activeType];
        detail::Lcg rng (td.seed);

        // delay lengths: geometric spread with a little jitter, rounded to distinct primes
        int used[kNumLines] = {};
        for (int i = 0; i < kNumLines; ++i)
        {
            const float u = ((float) i + 0.15f + 0.7f * rng.next()) / (float) kNumLines;
            const float ms = td.minMs * std::pow (td.maxMs / td.minMs, u);
            int len = detail::nearestPrime ((int) (ms * 0.001f * fs));
            for (bool clash = true; clash;) { clash = false; for (int j = 0; j < i; ++j) if (used[j] == len) { len = detail::nearestPrime (len + 2); clash = true; } }
            used[i] = len;
            const int ap = detail::nearestPrime ((int) ((float) len * (0.23f + 0.14f * rng.next())));
            lineAp[i].len = ap; lineAp[i].g = td.lineDiff;
            baseLen[i] = (float) (len - ap);            // total loop length stays `len`
            loopLen[i] = (float) len;
            // spread LFO rates and start phases
            const float rate = td.modRate * (1.0f + 0.31f * (float) i + 0.2f * rng.next());
            const float w = 2.0f * kPi * rate / fs;
            lfoC[i] = std::cos (w); lfoS[i] = std::sin (w);
            const float ph = 2.0f * kPi * rng.next();
            lfoX[i] = std::cos (ph); lfoY[i] = std::sin (ph);
            lfoSign[i] = (i & 1) ? -1.0f : 1.0f;
        }
        // The shortest line must stay longer than the modulation excursion plus interpolator reach.
        for (int i = 0; i < kNumLines; ++i) baseLen[i] = std::max (baseLen[i], 10.0f + td.modDepth * fs / 48000.0f);

        // orthogonal +-1 patterns: inject / pick up through different Hadamard rows -> decorrelated L and R
        for (int i = 0; i < kNumLines; ++i)
        {
            injL[i] = 0.25f * detail::hadamardSign (3, i);  injR[i] = 0.25f * detail::hadamardSign (12, i);
            outL[i] = 0.25f * detail::hadamardSign (5, i);  outR[i] = 0.25f * detail::hadamardSign (10, i);
        }

        // input diffusers
        static const float apMs[4] = { 4.771f, 3.595f, 12.73f, 9.307f };
        static const float apG[4]  = { 1.0f, 1.0f, 0.86f, 0.86f };
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k)
            {
                const float ms = apMs[k] * td.inDiffScale * (c == 0 ? 1.0f : 1.073f);
                inAp[c][(size_t) k].len = std::min (detail::nearestPrime ((int) (ms * 0.001f * fs)), (int) (0.039f * fs));
                inAp[c][(size_t) k].g = td.inDiff * apG[k];
            }

        // early reflections
        erCount = std::min (td.erTaps, kMaxEr);
        float eL = 0, eR = 0;
        for (int k = 0; k < erCount; ++k)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float u = ((float) k + 0.1f + 0.8f * rng.next()) / (float) erCount;
                const float ms = td.erStartMs + (td.erSpanMs - td.erStartMs) * std::pow (u, 1.35f);
                float g = std::pow (td.erStartMs / ms, 0.55f) * (rng.next() < 0.5f ? -1.0f : 1.0f);
                if (k == 0) g = std::abs (g);
                const int tap = std::max (1, (int) (ms * 0.001f * fs));
                if (c == 0) { erTapL[k] = tap; erGainL[k] = g; eL += g * g; }
                else        { erTapR[k] = tap; erGainR[k] = g; eR += g * g; }
            }
        }
        const float nL = eL > 0 ? 1.0f / std::sqrt (eL) : 0.0f, nR = eR > 0 ? 1.0f / std::sqrt (eR) : 0.0f;
        for (int k = 0; k < erCount; ++k) { erGainL[k] *= nL; erGainR[k] *= nR; }
        erLevel = td.erGain;

        updateDecay();
    }

    void snapControls()
    {
        cur = target;
        const float mixT = cur.bypass ? 0.0f : cur.mix;
        dryG = std::cos (mixT * 0.5f * kPi); wetG = std::sin (mixT * 0.5f * kPi);
        earlyG = std::cos (cur.earlyLate * 0.5f * kPi) * 1.41421356f; lateG = std::sin (cur.earlyLate * 0.5f * kPi) * 1.41421356f;
        shimG = cur.shimmer ? cur.feedback : 0.0f; inG = cur.bypass ? 0.0f : 1.0f;
        pdCur = cur.predelayMs * 0.001f * fs;
        fade = 1.0f; fadeDir = 0;
        updateDecay(); updateFilters();
        modDepthCur = cur.modulation * typeTable()[(size_t) activeType].modDepth * fs / 48000.0f;
    }

    // control-rate: smooth the slow parameters and refresh coefficients only while they move
    void updateControls()
    {
        auto slew = [this] (float& c, float t, float eps) { if (std::abs (t - c) < eps) { if (c == t) return false; c = t; return true; } c += ctlCoef * (t - c); return true; };
        bool d = false, f = false;
        d |= slewLog (cur.decaySec, target.decaySec);
        d |= slew (cur.damping, target.damping, 1.0e-4f);
        f |= slewLog (cur.lowCutHz, target.lowCutHz);
        f |= slewLog (cur.highCutHz, target.highCutHz);
        f |= slewLog (cur.splitHz, target.splitHz);
        if (cur.shiftSemis != target.shiftSemis) { cur.shiftSemis = target.shiftSemis; f = true; }
        cur.modulation += ctlCoef * (target.modulation - cur.modulation);
        modDepthCur = cur.modulation * typeTable()[(size_t) activeType].modDepth * fs / 48000.0f;
        cur.gate = target.gate; cur.shimmer = target.shimmer;
        if (d) updateDecay();
        if (f) updateFilters();
    }

    bool slewLog (float& c, float t)
    {
        if (c == t) return false;
        const float r = t / c;
        if (r > 0.999f && r < 1.001f) { c = t; return true; }
        c *= std::pow (r, ctlCoef);
        return true;
    }

    void updateDecay()
    {
        const TypeDef& td = typeTable()[(size_t) activeType];
        const float rtMid = std::clamp (cur.decaySec, 0.1f, 60.0f);
        const float d = std::clamp (cur.damping, 0.0f, 1.0f);
        // damping 0.5 reproduces the type's measured high-frequency ratio; 0 = no extra loss, 1 = heavily damped
        const float hiMult = std::pow (td.hiMult, 4.0f * d * d);
        const float hiFc = td.hiFc * std::pow (2.0f, (0.5f - d) * 2.0f);
        float rtLow = rtMid * td.lowMult;
        float rtHi = std::min (rtMid * std::min (hiMult, 1.0f), td.airCapSec);
        rtHi = std::min (rtHi, rtMid);
        const float tl = std::tan (kPi * td.lowFc / fs), th = std::tan (kPi * std::min (hiFc, 0.45f * fs) / fs);
        for (int i = 0; i < kNumLines; ++i)
        {
            const float e = -3.0f * loopLen[i] / fs;
            const float gm = std::pow (10.0f, e / rtMid), gl = std::pow (10.0f, e / rtLow), gh = std::pow (10.0f, e / rtHi);
            gMid[i] = gm;
            lowSh[i].setLow (gl / gm, tl);
            hiSh[i].setHigh (gh / gm, th);
        }
        // Normalise the tail to unit energy: an FDN with unit-norm in/out vectors returns
        // fs*RT / (13.8 * 16 * meanLoopLength) of the input energy, so divide that out.
        // The wet level then stays put when Decay or the type changes.
        float meanLen = 0; for (int i = 0; i < kNumLines; ++i) meanLen += loopLen[i]; meanLen /= (float) kNumLines;
        lateNorm = td.lateGain * std::sqrt (13.8155f * (float) kNumLines * meanLen / (fs * std::max (rtMid, 0.3f)));
    }

    void updateFilters()
    {
        for (int c = 0; c < 2; ++c)
        {
            lowCut[c].set (std::clamp (cur.lowCutHz, 10.0f, 2000.0f), fs);
            highCut[c].set (std::clamp (cur.highCutHz, 500.0f, 22000.0f), fs);
            splitHp[c].set (std::clamp (cur.splitHz, 40.0f, 8000.0f), fs);
            shifter[c].setSemitones (cur.shiftSemis);
        }
        shimLpCoef = 1.0f - std::exp (-2.0f * kPi * 9000.0f / fs);
    }

    //==========================================================================
    static constexpr int kMaxEr = 16;

    float fs = 48000.0f;
    bool prepared = false;
    Params target, cur;
    int activeType = 2;

    detail::Delay line[kNumLines];
    detail::Allpass lineAp[kNumLines];
    detail::Shelf1 lowSh[kNumLines], hiSh[kNumLines];
    float baseLen[kNumLines] = {}, loopLen[kNumLines] = {}, gMid[kNumLines] = {};
    float injL[kNumLines] = {}, injR[kNumLines] = {}, outL[kNumLines] = {}, outR[kNumLines] = {};
    float lfoX[kNumLines] = {}, lfoY[kNumLines] = {}, lfoC[kNumLines] = {}, lfoS[kNumLines] = {}, lfoSign[kNumLines] = {};

    detail::Delay pre[2], er[2];
    std::array<detail::Allpass, 4> inAp[2];
    int erTapL[kMaxEr] = {}, erTapR[kMaxEr] = {}; float erGainL[kMaxEr] = {}, erGainR[kMaxEr] = {}; int erCount = 0;

    detail::PitchShifter shifter[2];
    detail::Svf lowCut[2], highCut[2], splitHp[2];
    float shimLp[2] = {}, lateOut[2] = {};
    float shimLpCoef = 0.5f, shimDrive = 1.0f, shimLoop = 0.85f, erLevel = 1.0f;

    float dryG = 1, wetG = 0, earlyG = 1, lateG = 1, shimG = 0, inG = 1, pdCur = 0;
    float modDepthCur = 0, lateNorm = 0.3f;
    float smoothCoef = 0.001f, ctlCoef = 0.01f, fadeStep = 0.001f;
    float fade = 1.0f; int fadeDir = 0;
    float gateGain = 1.0f; int gateHold = 0;
    int ctlCount = 0;
};
} // namespace SpaceLifter5
