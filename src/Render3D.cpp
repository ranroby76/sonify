// D:\workspace\Fanan_web_hub\src\Render3D.cpp

#include "Render3D.h"
#include "Body3D.h"
#include "MeshField.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    // The panel camera's lean for the frames drawn on this thread (Render3D::ScopedView), and
    // the slope of the body's face the control stands on (degrees: back-to-front, sideways).
    thread_local float viewLeanX = 0.0f, viewLeanY = 0.0f, viewSlopeX = 0.0f, viewSlopeZ = 0.0f;
}

namespace
{
    constexpr float pi = 3.14159265358979f;

    //==========================================================================
    struct V3
    {
        float x = 0, y = 0, z = 0;

        V3() = default;
        constexpr V3 (float a, float b, float c) : x (a), y (b), z (c) {}

        V3 operator+ (V3 o) const { return { x + o.x, y + o.y, z + o.z }; }
        V3 operator- (V3 o) const { return { x - o.x, y - o.y, z - o.z }; }
        V3 operator* (float s) const { return { x * s, y * s, z * s }; }
        V3 operator* (V3 o) const { return { x * o.x, y * o.y, z * o.z }; }
        V3 operator- () const { return { -x, -y, -z }; }
        V3& operator+= (V3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    };

    inline float dot (V3 a, V3 b)       { return a.x * b.x + a.y * b.y + a.z * b.z; }
    inline V3 cross (V3 a, V3 b)        { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    inline float length (V3 a)          { return std::sqrt (dot (a, a)); }
    inline V3 normalise (V3 a)          { const auto l = length (a); return l > 1.0e-9f ? a * (1.0f / l) : V3 { 0, 1, 0 }; }
    inline V3 reflect (V3 i, V3 n)      { return i - n * (2.0f * dot (i, n)); }
    inline V3 mixv (V3 a, V3 b, float t) { return a + (b - a) * t; }

    inline float clamp01 (float v)      { return v < 0 ? 0 : (v > 1 ? 1 : v); }
    inline float mixf (float a, float b, float t) { return a + (b - a) * t; }
    inline float smoothstep (float a, float b, float v) { const float t = clamp01 ((v - a) / (b - a)); return t * t * (3 - 2 * t); }
    inline float len2 (float a, float b) { return std::sqrt (a * a + b * b); }

    inline float smin (float a, float b, float k)
    {
        if (k <= 1.0e-6f) return std::min (a, b);
        const float h = clamp01 (0.5f + 0.5f * (b - a) / k);
        return mixf (b, a, h) - k * h * (1.0f - h);
    }

    inline float smax (float a, float b, float k)   { return -smin (-a, -b, k); }

    /** An isosceles trapezoid about the y axis: half width r1 at y = -he, r2 at y = he (iq's). */
    float sdTrapezoid (float px, float py, float r1, float r2, float he)
    {
        const float k1x = r2, k1y = he, k2x = r2 - r1, k2y = 2.0f * he;
        px = std::abs (px);
        const float cax = px - std::min (px, py < 0.0f ? r1 : r2), cay = std::abs (py) - he;
        const float t = clamp01 (((k1x - px) * k2x + (k1y - py) * k2y) / (k2x * k2x + k2y * k2y));
        const float cbx = px - k1x + k2x * t, cby = py - k1y + k2y * t;
        const float s = (cbx < 0.0f && cay < 0.0f) ? -1.0f : 1.0f;
        return s * std::sqrt (std::min (cax * cax + cay * cay, cbx * cbx + cby * cby));
    }

    /** A rounded rectangle, half sizes a, b, corners rounded by c. */
    float sdRoundRect (float x, float z, float a, float b, float c)
    {
        c = std::min (c, std::min (a, b));
        const float qx = std::abs (x) - a + c, qz = std::abs (z) - b + c;
        return len2 (std::max (qx, 0.0f), std::max (qz, 0.0f)) + std::min (std::max (qx, qz), 0.0f) - c;
    }

    /** A 2D shape (its distance d2) raised from y0 to y1, its edges rounded by e. */
    float extrude (float d2, float y, float y0, float y1, float e)
    {
        const float h = (y1 - y0) * 0.5f, cy = (y0 + y1) * 0.5f;
        e = std::min (e, h * 0.98f);
        const float wx = d2 + e, wy = std::abs (y - cy) - h + e;
        return std::min (std::max (wx, wy), 0.0f) + len2 (std::max (wx, 0.0f), std::max (wy, 0.0f)) - e;
    }

    /** A cone with round ends from a (radius r1) to b (radius r2) (iq's). */
    float sdRoundCone (V3 p, V3 a, V3 b, float r1, float r2)
    {
        const V3 ba = b - a;
        const float l2 = dot (ba, ba), rr = r1 - r2, a2 = l2 - rr * rr, il2 = 1.0f / l2;
        const V3 pa = p - a;
        const float y = dot (pa, ba), z = y - l2;
        const V3 xv = pa * l2 - ba * y;
        const float x2 = dot (xv, xv), y2 = y * y * l2, z2 = z * z * l2;
        const float k = (rr >= 0 ? 1.0f : -1.0f) * rr * rr * x2;

        if ((z >= 0 ? 1.0f : -1.0f) * a2 * z2 > k) return std::sqrt (x2 + z2) * il2 - r2;
        if ((y >= 0 ? 1.0f : -1.0f) * a2 * y2 < k) return std::sqrt (x2 + y2) * il2 - r1;
        return (std::sqrt (x2 * a2 * il2) + y * rr) * il2 - r1;
    }

    float hash1 (float n)   { const float s = std::sin (n) * 43758.5453f; return s - std::floor (s); }

    float noise1 (float x)
    {
        const float i = std::floor (x), f = x - i;
        const float u = f * f * (3 - 2 * f);
        return mixf (hash1 (i), hash1 (i + 1.0f), u);
    }

    V3 linear (juce::Colour c)
    {
        auto lin = [] (float v) { return std::pow (v, 2.2f); };
        return { lin (c.getFloatRed()), lin (c.getFloatGreen()), lin (c.getFloatBlue()) };
    }

    //==========================================================================
    // Materials

    struct MaterialDef { float metal, rough, coat; int aniso; };   // aniso: 0 none, 1 spun (round its axis), 2 brushed (across)

    MaterialDef materialDef (int index)
    {
        static const MaterialDef defs[] = {
            { 0.0f, 0.24f, 0.85f, 0 },   // Gloss plastic
            { 0.0f, 0.45f, 0.25f, 0 },   // Satin plastic
            { 0.0f, 0.78f, 0.0f,  0 },   // Matte plastic
            { 0.0f, 0.9f,  0.0f,  0 },   // Rubber
            { 0.0f, 0.3f,  0.7f,  0 },   // Bakelite
            { 1.0f, 0.34f, 0.0f,  2 },   // Brushed aluminium
            { 1.0f, 0.28f, 0.0f,  1 },   // Spun aluminium
            { 1.0f, 0.05f, 0.0f,  0 },   // Chrome
            { 1.0f, 0.22f, 0.0f,  0 },   // Steel
            { 1.0f, 0.16f, 0.0f,  0 },   // Gold
            { 1.0f, 0.22f, 0.0f,  0 },   // Brass
            { 1.0f, 0.2f,  0.0f,  0 },   // Copper
            { 1.0f, 0.36f, 0.25f, 2 },   // Anodized
            { 0.0f, 0.4f,  0.55f, 0 },   // Painted metal
        };

        return defs[juce::jlimit (0, (int) (sizeof (defs) / sizeof (defs[0])) - 1, index)];
    }

    struct Surface
    {
        V3 albedo { 0.1f, 0.1f, 0.1f };
        float metal = 0, rough = 0.5f, coat = 0;
        int aniso = 0;
        V3 tangent { 1, 0, 0 };
        V3 emission;
        float glow = 0;    // a lamp: light it gives the panel round it isn't needed here, only its own
    };

    Surface surfaceOf (int material, juce::Colour colour)
    {
        const auto def = materialDef (material);
        Surface s;
        s.albedo = linear (colour);
        s.metal = def.metal;
        s.rough = def.rough;
        s.coat = def.coat;
        s.aniso = def.aniso;
        return s;
    }

    /** What's painted on the panel round it (a slot, scale marks, a light ring and its glow). */
    struct Paint
    {
        V3 colour;
        float alpha = 0;     // the panel's own colour there (0: see-through)
        V3 emission;         // light it gives
        float emissionAlpha = 0;
    };

    //==========================================================================
    struct Scene
    {
        virtual ~Scene() = default;
        virtual float map (V3 p, int& part) const = 0;
        virtual Surface shade (V3 p, V3 n, int part) const = 0;
        virtual void ground (float, float, Paint&) const {}

        /** Without its see-through parts (what's seen through glass, and what casts shadows). */
        virtual float mapSolid (V3 p, int& part) const   { return map (p, part); }

        /** How see-through a part is (0..1: glass). */
        virtual float glassOf (int) const                 { return 0.0f; }

        /** How far apart its normal is measured on a part (0: as fine as the picture): a
            model from a file, over a cell of its field, so its faces shade smoothly. */
        virtual float normalStep (int) const              { return 0.0f; }

        float extentX = 10, extentZ = 10, top = 10;   // the object's box (centred, on the panel)
        float stepScale = 0.9f;                       // under 1 where the shape is bent (flutes, knurls)
        int maxSteps = 220;                           // a ray's steps (a big body takes more)
        float aoSize = 0;                             // how far round it its shadows in corners reach (0: by its size)
    };

    //==========================================================================
    // A knob: a turned body (its foot, its skirt, its top a dome or a dish), its side fluted,
    // knurled or ribbed, a cap set into its top, its pointer, a light ring on the panel.
    struct KnobScene final : Scene
    {
        float R, H, Rt, e, Rs = 0, Hs = 0, bev = 0;
        bool skirt, cap, capGroove, skirtLine, pointerSide = false, ring;
        int topKind, grip, pointer, ringFrom;
        float td, gn, gd, rc, pl, pw, py;
        float angle;    // radians, clockwise from 12 o'clock
        float turnFrom, turnTo, current;   // degrees
        float ringIn = 0, ringOut = 0;
        int ringSegments;
        Surface body, skirtSurface, capSurface, pointerSurface;
        V3 ringColour;
        float domeR = 0, domeY = 0, dishR = 0, dishY = 0;

        KnobScene (const Model3D& m, float W, float Hc, float t)
        {
            const float side = std::min (W, Hc);
            R = m.number ("diameter") * 0.01f * side * 0.5f;
            H = std::max (2.0f, m.number ("height") * 0.01f * 2.0f * R);
            Rt = R * (1.0f - m.number ("taper") * 0.01f);
            e = std::min (m.number ("edge") * 0.01f * H, std::min (Rt, H) * 0.48f);
            topKind = m.choice ("top");
            td = m.number ("topDepth") * 0.01f * Rt;

            skirt = m.flag ("skirt");

            if (skirt)
            {
                Rs = R * m.number ("skirtDiameter") * 0.01f;
                Hs = std::max (1.0f, H * m.number ("skirtHeight") * 0.01f);
                bev = (Rs - R * 0.92f) * m.number ("skirtBevel") * 0.01f;
            }

            grip = m.choice ("grip");
            gn = std::round (m.number ("gripCount"));
            gd = m.number ("gripDepth") * 0.01f * R;

            cap = m.flag ("cap");
            capGroove = m.flag ("capGroove");
            rc = (Rt - e * 0.6f) * m.number ("capDiameter") * 0.01f;

            pointer = m.choice ("pointer");
            pl = (Rt - e * 0.5f) * m.number ("pointerLength") * 0.01f;
            pw = std::max (0.8f, Rt * m.number ("pointerWidth") * 0.01f);
            py = Rt * m.number ("pointerY") * 0.01f;   // towards the edge (+) or the centre (-)
            skirtLine = skirt && m.flag ("skirtLine") && pointer != 0;
            pointerSide = m.flag ("pointerSide") && pointer != 0;

            turnFrom = m.number ("turnFrom");
            turnTo = m.number ("turnTo");
            current = turnFrom + (turnTo - turnFrom) * t;
            angle = current * pi / 180.0f;

            ring = m.flag ("ring");
            ringFrom = m.choice ("ringFrom");
            ringSegments = juce::roundToInt (m.number ("ringSegments"));
            ringColour = linear (m.colour ("ringColour"));
            ringIn = std::max (R, Rs) + R * m.number ("ringGap") * 0.01f;
            ringOut = ringIn + R * m.number ("ringWidth") * 0.01f;

            body = surfaceOf (m.choice ("material"), m.colour ("colour"));
            skirtSurface = surfaceOf (m.choice ("skirtMaterial"), m.colour ("skirtColour"));
            capSurface = surfaceOf (m.choice ("capMaterial"), m.colour ("capColour"));
            pointerSurface = surfaceOf (1, m.colour ("pointerColour"));
            pointerSurface.coat = 0.3f;

            // The dome's and the dish's spheres.
            const float rTop = std::max (1.0f, Rt - e);

            if (topKind == 1 && td > 0.05f)
            {
                domeR = (rTop * rTop + td * td) / (2.0f * td);
                domeY = H - domeR + td;
            }

            if (topKind == 2 && td > 0.05f)
            {
                const float rd = rTop * 0.86f;
                dishR = (rd * rd + td * td) / (2.0f * td);
                dishY = H + dishR - td;
            }

            extentX = extentZ = std::max (std::max (R, Rs), ring ? ringOut * 1.4f : 0.0f) + 2.0f;
            top = H + (topKind == 1 ? td : 0.0f) + (pointer == 4 ? 2.0f : 0.0f) + 1.0f;
            stepScale = grip == 0 ? 0.9f : 0.6f;
        }

        V3 toObject (V3 p) const
        {
            const float c = std::cos (angle), s = std::sin (angle);
            return { p.x * c + p.z * s, p.y, -p.x * s + p.z * c };
        }

        float topAt (float r) const
        {
            if (domeR > 0 && r < Rt) return std::max (H, domeY + std::sqrt (std::max (0.0f, domeR * domeR - r * r)));
            if (dishR > 0 && r < Rt) return std::min (H, dishY - std::sqrt (std::max (0.0f, dishR * dishR - r * r)));
            return H;
        }

        float map (V3 wp, int& part) const override
        {
            const V3 p = toObject (wp);
            const float r = len2 (p.x, p.z);

            // The body: a trapezoid turned round its axis, its top edge rounded.
            float d = sdTrapezoid (r, p.y - H * 0.5f, R - e, Rt - e, H * 0.5f - e) - e;

            // Its grip, on its side (between the skirt and the top's rounding).
            if (grip != 0 && gd > 0.01f)
            {
                const float y0 = (skirt ? Hs : 0.0f) + 0.5f, y1 = H - e * 0.8f;
                const float w = clamp01 ((p.y - y0) / 1.5f) * clamp01 ((y1 - p.y) / 1.5f) * clamp01 (1.0f - std::abs (d) / (gd * 3.0f + 2.0f));

                if (w > 0.0f)
                {
                    const float th = std::atan2 (p.z, p.x);

                    if (grip == 1)          // flutes: round grooves
                        d += gd * w * std::pow (0.5f + 0.5f * std::cos (gn * th), 4.0f);
                    else if (grip == 3)     // ribs: flat-topped ribs with square grooves
                        d += gd * w * smoothstep (0.35f, 0.65f, 0.5f + 0.5f * std::cos (gn * th));
                    else                    // knurled: a diamond pattern
                    {
                        const float pitch = 2.0f * pi * R / gn;
                        const float a = gn * th / (2.0f * pi) + p.y / pitch, b = gn * th / (2.0f * pi) - p.y / pitch;
                        auto tri = [] (float v) { const float f = v - std::floor (v); return std::abs (f - 0.5f) * 2.0f; };
                        d += gd * w * std::max (tri (a), tri (b));
                    }
                }
            }

            // Its top: a dome on it, or a dish in it.
            if (domeR > 0)
            {
                const float dome = std::max (len2 (r, p.y - domeY) - domeR, r - (Rt - e * 0.7f));
                d = smin (d, std::max (dome, -p.y + H * 0.5f), std::max (0.5f, e * 0.5f));
            }

            if (dishR > 0)
                d = smax (d, -(len2 (r, p.y - dishY) - dishR), 0.6f);

            // A groove round the cap.
            if (cap && capGroove)
                d = smax (d, -(len2 (r - rc, p.y - topAt (rc)) - std::max (0.6f, Rt * 0.025f)), 0.3f);

            // The pointer: cut in, or standing up, along -z (12 o'clock before it turns).
            if (pointer == 3 || pointer == 4)
            {
                const float zc = -(pl * 0.5f + pl * 0.2f) - py, hl = pl * 0.45f;
                const float yt = topAt (std::abs (zc));
                const float depth = std::max (0.8f, Rt * 0.05f);
                const float bx = std::abs (p.x) - pw * 0.5f, bz = std::abs (p.z - zc) - hl;

                if (pointer == 3)
                {
                    const float by = std::abs (p.y - yt) - depth;
                    const float box = len2 (std::max (bx, 0.0f), std::max (bz, 0.0f)) + std::min (std::max (bx, bz), 0.0f);
                    d = smax (d, -std::max (box, by), 0.4f);
                }
                else
                {
                    const float ridge = extrude (len2 (std::max (bx, 0.0f), std::max (bz, 0.0f)) + std::min (std::max (bx, bz), 0.0f) - 0.0f,
                                                 p.y, yt - 2.0f, yt + depth, std::min (pw * 0.4f, depth * 0.5f));

                    if (ridge < d)
                    {
                        part = 2;
                        return smin (d, ridge, 0.4f);
                    }
                }
            }

            part = 0;

            // The skirt: its foot, sloping to its rim.
            if (skirt)
            {
                const float er = std::min (1.2f, Hs * 0.3f);
                const float ds = sdTrapezoid (r, p.y - Hs * 0.5f, Rs - er, std::max (R * 0.5f, Rs - bev - er), Hs * 0.5f - er) - er;

                if (ds < d)
                    part = 1;

                d = smin (d, ds, std::min (1.5f, Hs * 0.25f));
            }

            return d;
        }

        Surface shade (V3 wp, V3 n, int part) const override
        {
            const V3 p = toObject (wp);
            const float r = len2 (p.x, p.z);
            const float upness = n.y;
            Surface s = part == 1 ? skirtSurface : (part == 2 ? pointerSurface : body);

            if (part == 0 && cap && r < rc && upness > 0.5f && p.y > H * 0.5f)
                s = capSurface;

            // The pointer painted on the top (a line or a dot), or filling its groove.
            if (part == 0 && upness > 0.45f && p.y > H * 0.5f)
            {
                const float pz = p.z + py;   // (moved along its line)
                const bool onLine = (pointer == 1 || pointer == 3) && std::abs (p.x) < pw * 0.5f && pz < -pl * 0.2f && pz > -(pl + pl * 0.2f) * 0.95f;
                const float dotZ = -pl * 0.8f - py;
                const bool onDot = pointer == 2 && len2 (p.x, p.z - dotZ) < pw * 0.9f;

                if (onLine || onDot)
                    s = pointerSurface;
            }

            if (part == 1 && skirtLine && std::abs (p.x) < pw * 0.5f && p.z < -R * 0.95f && upness > 0.2f)
                s = pointerSurface;

            // ...and down its side (a tall cylinder's, a cone's), as wide there as on its top.
            if (part == 0 && pointerSide && (pointer == 1 || pointer == 3) && p.z < 0.0f && upness < 0.6f
                  && p.y > (skirt ? Hs : 0.0f) + 0.5f && std::abs (p.x) < pw * 0.5f)
                s = pointerSurface;

            // Spun and brushed metal: round its axis, or across.
            if (s.aniso == 1)
                s.tangent = normalise (V3 { -p.z, 0, p.x });
            else if (s.aniso == 2)
                s.tangent = { 1, 0, 0 };

            if (s.aniso != 0)
            {
                // The tangent back in the world (it turns with the knob).
                const float c = std::cos (angle), si = std::sin (angle);
                s.tangent = { s.tangent.x * c - s.tangent.z * si, s.tangent.y, s.tangent.x * si + s.tangent.z * c };

                if (s.aniso == 1 && upness > 0.6f)
                    s.albedo = s.albedo * (0.9f + 0.2f * noise1 (r * 2.3f));
            }

            return s;
        }

        void ground (float x, float z, Paint& paint) const override
        {
            if (! ring)
                return;

            const float r = len2 (x, z);
            const float w = ringOut - ringIn;

            if (r < ringIn - w * 2.0f || r > ringOut + w * 2.5f)
                return;

            // Its angle on the panel, clockwise from 12 o'clock.
            const float a = std::atan2 (x, -z) * 180.0f / pi;

            if (a < turnFrom - 4.0f || a > turnTo + 4.0f)
                return;

            const float lo = ringFrom == 1 ? std::min (0.0f, current) : turnFrom;
            const float hi = ringFrom == 1 ? std::max (0.0f, current) : current;

            // The LEDs: a band, or one apiece with gaps between them.
            bool inLed = a >= turnFrom && a <= turnTo;
            float ledCentre = a;

            if (ringSegments > 0)
            {
                const float span = (turnTo - turnFrom) / (float) ringSegments;
                const float k = std::floor ((a - turnFrom) / span);
                const float f = (a - turnFrom) / span - k;
                inLed = inLed && f > 0.16f && f < 0.84f;
                ledCentre = turnFrom + (k + 0.5f) * span;
            }

            const bool lit = ledCentre >= lo - 0.01f && ledCentre <= hi + 0.01f && hi > lo + 0.01f;
            const float inBand = clamp01 (0.5f + std::min (r - ringIn, ringOut - r));

            if (inLed && inBand > 0.0f)
            {
                if (lit)
                {
                    paint.emission = ringColour * 2.6f + V3 { 0.35f, 0.35f, 0.35f };
                    paint.emissionAlpha = inBand;
                }
                else
                {
                    paint.colour = ringColour * 0.08f + V3 { 0.015f, 0.015f, 0.018f };
                    paint.alpha = inBand * 0.9f;
                }

                return;
            }

            // The glow round what's lit.
            if (lit && a >= lo - 3.0f && a <= hi + 3.0f)
            {
                const float dist = r < ringIn ? ringIn - r : (r > ringOut ? r - ringOut : 0.0f);
                const float g = 0.55f * std::exp (-dist / std::max (1.0f, w * 0.7f));
                paint.emission = ringColour * 1.6f;
                paint.emissionAlpha = std::max (paint.emissionAlpha, g);
            }
        }
    };

    //==========================================================================
    // A push button: a rounded cap (round, square, a pill...) in its bezel, going down when
    // it's on, its lamp lit then; a label on its top.
    struct ButtonScene final : Scene
    {
        float a, b, c, h, e, travel, yo, g = 0, bw = 0, bh = 0;
        int topShape, light, lightPlace;
        float td, lightOff;
        bool on, bezel;
        Surface cap, bezelSurface;
        V3 lightColour, labelColour;
        float domeR = 0, domeY = 0, dishR = 0, dishY = 0;
        std::vector<float> label;
        int labelW = 0, labelH = 0;

        ButtonScene (const Model3D& m, float W, float Hc, float t)
        {
            const int shape = m.choice ("shape");
            a = m.number ("width") * 0.01f * W * 0.5f;
            b = m.number ("depth") * 0.01f * Hc * 0.5f;

            if (shape <= 2)
                a = b = std::min (a, b);   // round and square ones are as deep as they're wide

            const float small = std::min (a, b);
            c = shape == 0 ? small : shape == 1 ? small * 0.32f : shape == 2 ? small * 0.08f : shape == 3 ? small
                                   : small * m.number ("corner") * 0.01f;

            h = std::max (1.5f, m.number ("height") * 0.01f * 2.0f * a);
            e = std::min (m.number ("edge") * 0.01f * h, small * 0.6f);
            on = t >= 0.5f;
            travel = m.number ("travel") * 0.01f * h;

            bezel = m.flag ("bezel");

            if (bezel)
            {
                g = std::max (0.6f, small * 0.04f);
                bw = std::max (1.0f, m.number ("bezelWidth") * 0.01f * 2.0f * a);
                bh = std::max (1.0f, m.number ("bezelHeight") * 0.01f * h);
            }

            yo = on ? -travel : 0.0f;

            topShape = m.choice ("top");
            td = m.number ("topDepth") * 0.01f * small;

            const float rTop = std::max (1.0f, std::sqrt (a * a + b * b) * (shape == 0 ? 0.72f : 0.95f));

            if (topShape == 1 && td > 0.05f)
            {
                domeR = (rTop * rTop + td * td) / (2.0f * td);
                domeY = h - domeR + td;
            }

            if (topShape == 2 && td > 0.05f)
            {
                const float rd = small * 0.92f;
                dishR = (rd * rd + td * td) / (2.0f * td);
                dishY = h + dishR - td;
            }

            light = m.choice ("light");
            lightPlace = m.choice ("lightPlace");
            lightColour = linear (m.colour ("lightColour"));
            lightOff = m.number ("lightOff") * 0.01f;

            cap = surfaceOf (m.choice ("material"), m.colour ("colour"));
            bezelSurface = surfaceOf (m.choice ("bezelMaterial"), m.colour ("bezelColour"));
            labelColour = linear (m.colour ("labelColour"));

            // The label, as a mask over its top.
            if (const auto words = m.text ("label").trim(); words.isNotEmpty())
            {
                labelW = 256;
                labelH = std::max (16, juce::roundToInt (256.0f * b / std::max (1.0f, a)));
                juce::Image mask (juce::Image::SingleChannel, labelW, labelH, true, juce::SoftwareImageType());
                {
                    juce::Graphics lg (mask);
                    lg.setColour (juce::Colours::white);
                    lg.setFont (juce::Font (juce::FontOptions ((float) labelH * m.number ("labelSize") * 0.01f, juce::Font::bold)));
                    lg.drawFittedText (words, 0, 0, labelW, labelH, juce::Justification::centred, 2, 0.5f);
                }

                label.resize ((size_t) labelW * (size_t) labelH);
                juce::Image::BitmapData data (mask, juce::Image::BitmapData::readOnly);

                for (int y = 0; y < labelH; ++y)
                    for (int x = 0; x < labelW; ++x)
                        label[(size_t) y * (size_t) labelW + (size_t) x] = (float) *data.getPixelPointer (x, y) / 255.0f;
            }

            extentX = a + g + bw + (light == 3 ? small * 0.5f : 0.0f) + 2.0f;
            extentZ = b + g + bw + (light == 3 ? small * 0.5f : 0.0f) + 2.0f;
            top = h + std::max (0.0f, td) + 1.0f;
            stepScale = 0.9f;
        }

        float capDistance (V3 p) const
        {
            const float y = p.y - yo;
            const float d2 = sdRoundRect (p.x, p.z, a - e * 0.25f, b - e * 0.25f, std::max (0.0f, c - e * 0.25f));
            float d = extrude (d2, y, -h, h + (domeR > 0 ? td : 0.0f), e);

            if (domeR > 0)
                d = smax (d, len2 (len2 (p.x, p.z), y - domeY) - domeR, e * 0.4f);
            else
                d = smax (d, y - h, e * 0.1f);

            if (dishR > 0)
                d = smax (d, -(len2 (len2 (p.x, p.z), y - dishY) - dishR), 0.5f);

            return d;
        }

        float map (V3 p, int& part) const override
        {
            float d = capDistance (p);
            part = 0;

            if (bezel)
            {
                const float outer = sdRoundRect (p.x, p.z, a + g + bw, b + g + bw, c + g + bw);
                const float inner = sdRoundRect (p.x, p.z, a + g, b + g, c + g);
                const float ring = extrude (std::max (outer, -inner), p.y, -1.0f, bh, std::min (bw * 0.45f, bh * 0.45f));

                if (ring < d)
                    part = 1;

                d = std::min (d, ring);
            }

            return d;
        }

        bool inLamp (float x, float z) const
        {
            const float small = std::min (a, b);

            if (light == 1)
            {
                const float lx = lightPlace == 2 ? -a * 0.58f : 0.0f;
                const float lz = lightPlace == 0 ? -b * 0.58f : 0.0f;
                return len2 (x - lx, z - lz) < small * 0.13f;
            }

            if (light == 2)
                return std::abs (x) < a * 0.5f && std::abs (z + b * 0.62f) < small * 0.07f;

            return false;
        }

        Surface shade (V3 p, V3 n, int part) const override
        {
            if (part == 1)
            {
                auto s = bezelSurface;

                if (s.aniso == 1) s.tangent = normalise (V3 { -p.z, 0, p.x });
                return s;
            }

            auto s = cap;
            const bool onTop = n.y > 0.55f && p.y - yo > h * 0.4f;

            if (s.aniso == 1) s.tangent = normalise (V3 { -p.z, 0, p.x });

            // The whole cap lit (backlit, see-through plastic). Off, its colour shows as much as
            // "Off, it shows" says (from a dark cap at 0 to the tinted plastic from 30 up).
            if (light == 4)
            {
                s.albedo = mixv (s.albedo, lightColour, on ? 0.6f : 0.6f * juce::jmin (1.0f, lightOff / 0.3f));
                s.emission = lightColour * (on ? 1.35f : lightOff * 0.6f);
            }

            if (onTop && inLamp (p.x, p.z))
            {
                s = surfaceOf (0, juce::Colours::black);
                s.albedo = lightColour * 0.25f;
                s.emission = lightColour * (on ? 2.8f : lightOff * 0.9f) + (on ? V3 { 0.4f, 0.4f, 0.4f } : V3 {});
            }

            // The label printed on its top.
            if (onTop && ! label.empty())
            {
                const float u = (p.x / a * 0.5f + 0.5f) * (float) labelW, v = (p.z / b * 0.5f + 0.5f) * (float) labelH;
                const int ix = juce::jlimit (0, labelW - 1, (int) u), iy = juce::jlimit (0, labelH - 1, (int) v);
                const float k = label[(size_t) iy * (size_t) labelW + (size_t) ix];

                if (k > 0.0f)
                {
                    s.albedo = mixv (s.albedo, labelColour, k);
                    s.metal = s.metal * (1.0f - k);
                    s.rough = mixf (s.rough, 0.5f, k);
                }
            }

            return s;
        }

        void ground (float x, float z, Paint& paint) const override
        {
            if (light != 3)
                return;

            // A ring of light on the panel round it.
            const float small = std::min (a, b);
            const float inner = (bezel ? g + bw : g) + small * 0.06f, width = small * 0.12f;
            const float d = sdRoundRect (x, z, a + inner + width * 0.5f, b + inner + width * 0.5f, c + inner + width * 0.5f);
            const float band = clamp01 (0.5f + width * 0.5f - std::abs (d));

            if (band > 0.0f)
            {
                if (on)
                {
                    paint.emission = lightColour * 2.4f + V3 { 0.3f, 0.3f, 0.3f };
                    paint.emissionAlpha = band;
                }
                else
                {
                    paint.colour = lightColour * (0.05f + lightOff * 0.4f);
                    paint.alpha = band * 0.85f;
                }
            }
            else if (on)
            {
                paint.emission = lightColour * 1.4f;
                paint.emissionAlpha = 0.5f * std::exp (-(std::abs (d) - width * 0.5f) / std::max (1.0f, width));
            }
        }
    };

    //==========================================================================
    // A slider: its track (a slot in the panel with scale marks beside it) and its cap
    // travelling along it: a classic fader cap (a dip across its top, grooves), a block or a
    // round cap. Up the canvas when it's taller than it's wide, else across.
    struct SliderScene final : Scene
    {
        bool vertical;
        float lt, wt, cw, cl, ch, pos, tickLen;
        int capShape, ticks, grooves;
        Surface capSurface, lineSurface;
        V3 trackColour, tickColour;
        bool line;
        float dishR = 0, dishY = 0;

        SliderScene (const Model3D& m, float W, float Hc, float t)
        {
            vertical = Hc >= W;
            const float along = vertical ? Hc : W, across = vertical ? W : Hc;

            lt = m.number ("trackLength") * 0.01f * along * 0.5f;
            wt = std::max (0.8f, m.number ("trackWidth") * 0.01f * across * 0.5f);
            capShape = m.choice ("cap");
            cw = m.number ("capWidth") * 0.01f * across * 0.5f;
            cl = capShape == 2 ? cw : std::max (1.0f, m.number ("capLength") * 0.01f * along * 0.5f);
            ch = std::max (1.5f, m.number ("capHeight") * 0.01f * across);
            grooves = juce::roundToInt (m.number ("grooves"));
            ticks = juce::roundToInt (m.number ("ticks"));
            tickLen = m.number ("tickLength") * 0.01f * across;

            // The cap's travel: bottom to top (left to right), within the track.
            const float travel = std::max (0.0f, lt - cl * 0.6f);
            pos = travel * (1.0f - 2.0f * t);        // along its axis: + bottom, - top
            if (! vertical) pos = -pos;              // across: left to right

            capSurface = surfaceOf (m.choice ("material"), m.colour ("colour"));
            const auto lineColour = m.colour ("lineColour");
            line = lineColour.getAlpha() > 8;
            lineSurface = surfaceOf (1, lineColour);
            trackColour = linear (m.colour ("trackColour"));
            tickColour = linear (m.colour ("tickColour"));

            if (capShape == 0)
            {
                const float dd = ch * 0.18f;
                dishR = (cl * cl + dd * dd) / (2.0f * dd);
                dishY = ch + dishR - dd;
            }

            extentX = (vertical ? std::max (cw, wt + tickLen + 4.0f) : lt + cl) + 2.0f;
            extentZ = (vertical ? lt + cl : std::max (cw, wt + tickLen + 4.0f)) + 2.0f;
            top = ch + 1.0f;
            stepScale = grooves > 0 ? 0.75f : 0.9f;
        }

        /** Along the track and across it, the cap's middle at 0. */
        void local (V3 p, float& u, float& v) const
        {
            if (vertical) { u = p.z - pos; v = p.x; }
            else          { u = p.x - pos; v = p.z; }
        }

        float map (V3 p, int& part) const override
        {
            float u, v;
            local (p, u, v);
            part = 0;

            if (capShape == 2)
            {
                // Round: a short turned cap with a dome.
                const float r = len2 (u, v);
                float d = sdTrapezoid (r, p.y - ch * 0.5f, cw, cw * 0.86f, ch * 0.5f);
                d = smax (d, len2 (r, p.y - (ch - cw * 2.2f)) - cw * 2.2f, 1.0f);
                return d - 0.0f;
            }

            const float e = std::min (ch * 0.22f, std::min (cw, cl) * 0.3f);
            const float d2 = sdRoundRect (v, u, cw, cl, std::min (cw, cl) * 0.2f);
            float d = extrude (d2, p.y, -1.0f, ch, e);

            if (capShape == 0)
            {
                // The classic fader: a dip along its top (where the finger goes), grooves across it.
                d = smax (d, -(len2 (u, p.y - dishY) - dishR), 0.6f);

                if (grooves > 0 && p.y > ch * 0.55f)
                {
                    const float spacing = (cl * 1.7f) / (float) (grooves + 1);
                    const float k = u / spacing;
                    const float groove = std::max (0.0f, std::cos ((k - std::floor (k)) * 2.0f * pi));
                    d += 0.5f * std::min (spacing * 0.25f, 1.6f) * std::pow (groove, 8.0f) * clamp01 ((cl * 0.85f - std::abs (u)) / 2.0f);
                }
            }

            return d;
        }

        Surface shade (V3 p, V3 n, int) const override
        {
            float u, v;
            local (p, u, v);
            auto s = capSurface;

            if (s.aniso == 2) s.tangent = vertical ? V3 { 1, 0, 0 } : V3 { 0, 0, 1 };
            if (s.aniso == 1) s.tangent = normalise (V3 { -v, 0, u });

            // Its line across its middle.
            if (line && n.y > 0.4f && p.y > ch * 0.4f && std::abs (u) < std::max (0.7f, cl * 0.06f))
                s = lineSurface;

            return s;
        }

        void ground (float x, float z, Paint& paint) const override
        {
            const float u = vertical ? z : x, v = vertical ? x : z;

            // The slot: dark, deepest along the edge the light can't reach.
            const float d = sdRoundRect (v, u, wt, lt, wt);

            if (d < 0.5f)
            {
                const float cover = clamp01 (0.5f - d);
                const float depth = smoothstep (0.0f, wt * 0.9f, -d);
                paint.colour = trackColour * (0.45f + 0.55f * depth);
                paint.alpha = cover;
                return;
            }

            // Its scale marks, either side.
            if (ticks > 1)
            {
                const float from = wt + std::max (2.0f, wt * 0.8f), to = from + tickLen;
                const float av = std::abs (v);

                if (av > from && av < to)
                {
                    const float travel = std::max (0.0f, lt - cl * 0.6f);
                    const float step = 2.0f * travel / (float) (ticks - 1);
                    const float k = (u + travel) / step;
                    const float near = std::abs (k - std::round (k)) * step;

                    if (k > -0.5f && k < (float) ticks - 0.5f)
                    {
                        const float cover = clamp01 (0.9f - near) * clamp01 (av - from + 0.5f) * clamp01 (to - av + 0.5f);

                        if (cover > 0.0f)
                        {
                            paint.colour = tickColour;
                            paint.alpha = cover * 0.9f;
                        }
                    }
                }
            }
        }
    };

    //==========================================================================
    // A toggle switch: a lever on a nut, down (off) or up (on).
    struct ToggleScene final : Scene
    {
        float rn, hn, rb, hb, ll, rl, tilt;
        int base, tip;
        bool up;
        Surface nut, lever;
        V3 p0, p1;

        ToggleScene (const Model3D& m, float W, float Hc, float t)
        {
            base = m.choice ("base");
            tip = m.choice ("tip");
            rn = m.number ("baseDiameter") * 0.01f * W * 0.5f;
            hn = std::max (1.5f, rn * 0.32f);
            rb = rn * 0.46f;
            hb = rn * 0.28f;
            ll = m.number ("leverLength") * 0.01f * Hc;
            rl = std::max (0.8f, m.number ("leverWidth") * 0.01f * W * 0.5f);
            tilt = m.number ("angle") * pi / 180.0f;
            up = t >= 0.5f;

            nut = surfaceOf (m.choice ("baseMaterial"), m.colour ("baseColour"));
            lever = surfaceOf (m.choice ("leverMaterial"), m.colour ("leverColour"));

            p0 = { 0, hn + hb, 0 };
            const float dz = (up ? -1.0f : 1.0f) * std::sin (tilt) * ll;
            p1 = { 0, hn + hb + std::cos (tilt) * ll, dz };

            extentX = std::max (rn * (base == 2 ? 1.6f : 1.15f), rl * 3.0f) + 2.0f;
            extentZ = std::max (rn * (base == 2 ? 2.2f : 1.15f), std::abs (dz) + rl * 3.0f) + 2.0f;
            top = p1.y + rl * 3.0f + 1.0f;
            stepScale = 0.85f;
        }

        float map (V3 p, int& part) const override
        {
            const float r = len2 (p.x, p.z);
            float dNut;

            if (base == 0)
            {
                // A hex nut, its edges chamfered.
                const float k1 = 0.8660254f, k2 = 0.5f;
                const float qx = std::abs (p.x), qz = std::abs (p.z);
                const float hex = std::max (qz * k1 + qx * k2, qx) - rn * k1;
                dNut = extrude (hex, p.y, 0.0f, hn, std::min (hn * 0.25f, rn * 0.1f));
                dNut = smax (dNut, len2 (r, p.y - hn * 0.5f) - rn * 1.08f, 0.5f);
            }
            else if (base == 1)
            {
                dNut = extrude (r - rn, p.y, 0.0f, hn, std::min (hn * 0.35f, rn * 0.15f));
            }
            else
            {
                // A plate, a small round nut on it.
                const float plate = extrude (sdRoundRect (p.x, p.z, rn * 1.5f, rn * 2.1f, rn * 0.3f), p.y, -1.0f, hn * 0.45f, hn * 0.2f);
                dNut = std::min (plate, extrude (r - rn * 0.75f, p.y, 0.0f, hn, hn * 0.3f));
            }

            // The threaded bush it comes up through.
            float bush = extrude (r - rb, p.y, hn - 0.5f, hn + hb, std::min (0.6f, rb * 0.1f));
            bush += 0.25f * std::max (0.0f, std::sin (p.y * 2.2f)) * clamp01 ((hn + hb - 0.6f - p.y) * 2.0f);

            float d = std::min (dNut, bush);
            part = 0;

            // The lever, from its pivot, thinning towards its tip.
            float lev = sdRoundCone (p, p0, p1, rl, rl * 0.72f);

            if (tip == 1)
                lev = smin (lev, length (p - p1) - rl * 1.45f, rl * 0.4f);
            else if (tip == 2)
            {
                // A paddle: a flat blade going on from its tip.
                const V3 axis = normalise (p1 - p0), side { 1, 0, 0 }, face = normalise (cross (axis, side));
                const V3 q = p - (p1 + axis * (rl * 1.2f));
                const float along = dot (q, axis), across = q.x, through = dot (q, face);
                const float blade = extrude (sdRoundRect (across, along, rl * 1.7f, rl * 2.0f, rl * 0.9f), through, -rl * 0.45f, rl * 0.45f, rl * 0.3f);
                lev = smin (lev, blade, rl * 0.5f);
            }

            if (lev < d)
                part = 1;

            return smin (d, lev, std::min (rl * 0.3f, 0.8f));
        }

        Surface shade (V3 p, V3, int part) const override
        {
            auto s = part == 1 ? lever : nut;

            if (s.aniso == 1) s.tangent = normalise (V3 { -p.z, 0, p.x });
            return s;
        }
    };

    //==========================================================================
    // The window's 3D body (Body3D): its parts, in their materials and surfaces, on the floor.
    struct BodyScene final : Scene
    {
        const Body3D& body;
        V3 floorColour;
        float floorAlpha = 0;

        // The Showroom: the panel's picture printed on the upward faces, a floor fading out.
        std::vector<juce::PixelARGB> decal;
        int decalW = 0, decalH = 0;
        float decalScale = 1.0f, floorFade = 0.0f;

        void setDecal (const juce::Image& image, float scaleOfIt)
        {
            if (! image.isValid())
                return;

            const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);
            decalW = image.getWidth();
            decalH = image.getHeight();
            decalScale = scaleOfIt;
            decal.resize ((size_t) decalW * (size_t) decalH);

            for (int y = 0; y < decalH; ++y)
                for (int x = 0; x < decalW; ++x)
                    decal[(size_t) y * (size_t) decalW + (size_t) x] = data.getPixelColour (x, y).getPixelARGB();
        }

        explicit BodyScene (const Body3D& b) : body (b)
        {
            extentX = std::max (std::abs (b.lo[0]), std::abs (b.hi[0])) + 2.0f;
            extentZ = std::max (std::abs (b.lo[2]), std::abs (b.hi[2])) + 2.0f;
            top = std::max (1.0f, b.hi[1]) + 2.0f;
            stepScale = 0.8f;
            maxSteps = 420;
            aoSize = 9.0f;
            floorColour = linear (b.floor);
            floorAlpha = b.floor.getFloatAlpha();
        }

        float map (V3 p, int& part) const override        { return body.distance (p.x, p.y, p.z, part); }
        float mapSolid (V3 p, int& part) const override   { return body.distance (p.x, p.y, p.z, part, true); }

        float glassOf (int part) const override
        {
            return juce::isPositiveAndBelow (part, (int) body.getParts().size()) ? body.getParts()[(size_t) part].glass : 0.0f;
        }

        float normalStep (int part) const override
        {
            if (! juce::isPositiveAndBelow (part, (int) body.getParts().size()))
                return 0.0f;

            const auto& bp = body.getParts()[(size_t) part];

            if (bp.kind != Body3D::Kind::mesh || bp.mesh == nullptr)
                return 0.0f;

            return bp.mesh->cell * std::min (bp.meshScale[0], std::min (bp.meshScale[1], bp.meshScale[2])) * 0.9f;
        }

        Surface shade (V3 p, V3 n, int part) const override
        {
            if (! juce::isPositiveAndBelow (part, (int) body.getParts().size()))
                return surfaceOf (0, juce::Colour (0xff303034));

            const auto& bp = body.getParts()[(size_t) part];
            auto s = surfaceOf (bp.material, bp.colour);

            float colourScale = 1.0f, roughAdd = 0.0f;
            body.surfaceAt (part, p.x, p.y, p.z, colourScale, roughAdd);
            s.albedo = s.albedo * colourScale;
            s.rough = clamp01 (s.rough + roughAdd);

            if (bp.surface == Body3D::Surface::brushed)
                s.aniso = 2;

            // Spun round its own middle; brushed across.
            if (s.aniso == 1)
                s.tangent = normalise (V3 { -(p.z - bp.cz), 0, p.x - bp.cx });
            else if (s.aniso == 2)
                s.tangent = { 1, 0, 0 };

            if (bp.glass > 0.0f)
            {
                s.albedo = s.albedo * (1.0f - bp.glass);
                s.metal = 0.0f;
                s.rough = 0.08f;
                s.coat = 0.35f;
            }

            if (bp.glow.getAlpha() > 0)
                s.emission = linear (bp.glow) * (bp.glowStrength * 3.0f * bp.glow.getFloatAlpha());

            // The panel's picture, printed where the face looks up.
            if (decalW > 0 && n.y > 0.35f)
            {
                const float u = (p.x + (float) body.width * 0.5f) * decalScale, v = (p.z + (float) body.height * 0.5f) * decalScale;
                const int ix = (int) u, iy = (int) v;

                if (ix >= 0 && iy >= 0 && ix < decalW && iy < decalH)
                {
                    const auto px = decal[(size_t) iy * (size_t) decalW + (size_t) ix];
                    const float a = (float) px.getAlpha() / 255.0f * clamp01 ((n.y - 0.35f) * 6.0f);

                    if (a > 0.004f)
                    {
                        const float un = px.getAlpha() > 0 ? 255.0f / (float) px.getAlpha() : 0.0f;
                        const auto c = juce::Colour::fromFloatRGBA (std::min (1.0f, (float) px.getRed() * un / 255.0f),
                                                                    std::min (1.0f, (float) px.getGreen() * un / 255.0f),
                                                                    std::min (1.0f, (float) px.getBlue() * un / 255.0f), 1.0f);
                        s.albedo = mixv (s.albedo, linear (c), a);
                        s.metal *= 1.0f - a;
                        s.rough = mixf (s.rough, 0.42f, a);
                        s.coat *= 1.0f - a * 0.6f;
                        s.aniso = a > 0.5f ? 0 : s.aniso;
                    }
                }
            }

            return s;
        }

        void ground (float x, float z, Paint& paint) const override
        {
            paint.colour = floorColour;
            paint.alpha = floorAlpha;

            // A showroom's floor: lit round the body, fading out into the dark.
            if (floorFade > 0.0f)
                paint.alpha *= clamp01 (1.0f - (std::sqrt (x * x + z * z) - floorFade) / (floorFade * 1.6f));
        }
    };

    std::unique_ptr<Scene> makeScene (const Model3D& m, float W, float H, float t)
    {
        switch (m.kind)
        {
            case Model3D::Kind::button: return std::make_unique<ButtonScene> (m, W, H, t);
            case Model3D::Kind::slider: return std::make_unique<SliderScene> (m, W, H, t);
            case Model3D::Kind::toggle: return std::make_unique<ToggleScene> (m, W, H, t);
            case Model3D::Kind::knob:
            default:                    return std::make_unique<KnobScene> (m, W, H, t);
        }
    }

    //==========================================================================
    // The light: the panel's key light, a soft studio round it.
    struct Lighting
    {
        V3 key;            // towards the key light (the panel's global light)
        V3 shadowKey;      // ...what casts the shadows: no lower than 52 degrees, so they stay short
        V3 fill;           // a softer light from the other side
        V3 box;            // a big softbox overhead, leaning towards the key light
        float keyPower = 2.6f;
        float shadowSharpness = 16.0f;
        float exposure = 1.0f;

        /** The studio round it, seen along d (rough: how blurred). */
        V3 env (V3 d, float rough) const
        {
            const float r = std::pow (clamp01 (rough), 0.6f);
            const float sharp = mixf (900.0f, 3.0f, r);
            const float keyLobe = std::pow (std::max (0.0f, dot (d, key)), sharp) * (0.6f + 3.6f * (1.0f - rough));
            const float boxLobe = std::pow (std::max (0.0f, dot (d, box)), mixf (14.0f, 1.5f, r)) * (0.55f + 0.35f * (1.0f - rough));
            const float fillLobe = std::pow (std::max (0.0f, dot (d, fill)), sharp * 0.4f) * (0.2f + 0.7f * (1.0f - rough));
            // The room: a dim ceiling getting brighter overhead, a brighter band round the horizon
            // (what a chrome or a gold part shows as its bright streak), the dark panel below.
            const float base = d.y > 0.0f ? 0.035f + 0.26f * d.y * d.y : 0.02f + 0.015f * (1.0f + d.y);
            const float rim = 0.42f * std::pow (clamp01 (1.0f - std::abs (d.y - 0.12f) * 2.6f), 2.0f);
            const float v = base + rim + keyLobe + boxLobe + fillLobe;
            return V3 { v * 1.02f, v, v * 0.97f };
        }
    };

    float ggx (float nh, float a)
    {
        const float a2 = a * a, d = nh * nh * (a2 - 1.0f) + 1.0f;
        return a2 / (pi * d * d + 1.0e-6f);
    }

    float smith (float nv, float nl, float rough)
    {
        const float k = (rough + 1.0f) * (rough + 1.0f) / 8.0f;
        return (nv / (nv * (1 - k) + k)) * (nl / (nl * (1 - k) + k));
    }

    //==========================================================================
    struct Renderer
    {
        const Scene& scene;
        Lighting light;
        float W, H, scale, pad;
        V3 camC, camR, camU;    // towards the camera, its right, its up (the picture's plane: camR, camU)
        V3 view;                // towards the camera from this control: camC, or leaning to where the panel camera is
        float camL = 0;         // its distance (0: seen from infinitely far, no perspective)
        V3 target;              // where it looks (the Showroom: the body's middle; otherwise the origin)
        float shadowAlpha;
        float pixel;            // one picture pixel, in canvas pixels

        Renderer (const Scene& s, const Model3D& m, float w, float h, float sc, float pd, float lightAngle, float lightAltitude)
            : scene (s), W (w), H (h), scale (sc), pad (pd)
        {
            const float az = lightAngle * pi / 180.0f;
            const float alt = juce::jlimit (12.0f, 85.0f, lightAltitude) * pi / 180.0f;
            light.key = normalise ({ std::cos (alt) * std::cos (az), std::sin (alt), -std::cos (alt) * std::sin (az) });
            const float shadowAlt = std::max (alt, 52.0f * pi / 180.0f);
            light.shadowKey = normalise ({ std::cos (shadowAlt) * std::cos (az), std::sin (shadowAlt), -std::cos (shadowAlt) * std::sin (az) });
            light.box = normalise ({ std::cos (az) * 0.45f, 1.0f, -std::sin (az) * 0.45f });
            const float az2 = az + pi * 0.85f, alt2 = 40.0f * pi / 180.0f;
            light.fill = normalise ({ std::cos (alt2) * std::cos (az2), std::sin (alt2), -std::cos (alt2) * std::sin (az2) });
            light.shadowSharpness = mixf (28.0f, 2.0f, m.number ("softness") * 0.01f);
            light.exposure = std::pow (2.0f, m.number ("exposure") * 0.01f * 1.5f);
            shadowAlpha = m.number ("shadow") * 0.01f;

            // Seen from its tilt; standing on a sloped face of the body, seen as that face
            // turns it: a face rising towards the back shows the control's top foreshortened
            // and its back, one rising to the left its left side.
            const float tilt = (m.number ("tilt") - viewSlopeX) * pi / 180.0f;
            camC = { 0, std::cos (tilt), std::sin (tilt) };
            camR = { 1, 0, 0 };
            camU = { 0, std::sin (tilt), -std::cos (tilt) };

            if (std::abs (viewSlopeZ) > 0.01f)
            {
                const float roll = viewSlopeZ * pi / 180.0f, rc = std::cos (roll), rs = std::sin (roll);
                auto turn = [rc, rs] (V3 v) { return V3 { v.x * rc - v.y * rs, v.x * rs + v.y * rc, v.z }; };
                camC = turn (camC);
                camR = turn (camR);
                camU = turn (camU);
            }

            // The panel camera: the picture stays in the panel camera's plane, the rays come from
            // where it is (a control on the left is seen from its right).
            view = normalise (camC - camR * viewLeanX + camU * viewLeanY);

            const float p = m.number ("perspective") * 0.01f;
            camL = p < 0.005f ? 0.0f : std::max (W, H) * (1.4f + 9.0f * (1.0f - p) * (1.0f - p));
            pixel = 1.0f / scale;
        }

        /** A tall one seen at an angle that wouldn't fit its canvas: made smaller and centred (fit
            and the middle of what it covers, offX, offY), not cut off. */
        float fit = 1.0f, offX = 0.0f, offY = 0.0f;

        void fitInto (float canvasW, float canvasH)
        {
            float x0 = 1.0e9f, x1 = -1.0e9f, y0 = 1.0e9f, y1 = -1.0e9f;
            const float vc = std::max (0.2f, dot (view, camC));

            for (int c = 0; c < 8; ++c)
            {
                const V3 p { (c & 1) ? scene.extentX : -scene.extentX, (c & 2) ? scene.top : 0.0f, (c & 4) ? scene.extentZ : -scene.extentZ };
                const V3 onPlane = p - view * (dot (p, camC) / vc);
                const float x = dot (onPlane, camR), y = -dot (onPlane, camU);
                x0 = std::min (x0, x); x1 = std::max (x1, x);
                y0 = std::min (y0, y); y1 = std::max (y1, y);
            }

            if (x0 >= -canvasW * 0.5f && x1 <= canvasW * 0.5f && y0 >= -canvasH * 0.5f && y1 <= canvasH * 0.5f)
                return;   // it fits as it is

            fit = std::min (1.0f, std::min (0.97f * canvasW / std::max (1.0f, x1 - x0), 0.97f * canvasH / std::max (1.0f, y1 - y0)));
            offX = (x0 + x1) * 0.5f;
            offY = (y0 + y1) * 0.5f;
            pixel /= fit;
        }

        void ray (float cx, float cy, V3& o, V3& d) const
        {
            cx = offX + cx / fit;
            cy = offY + cy / fit;
            const V3 onPlane = camR * cx - camU * cy;

            if (camL <= 0.0f)
            {
                o = target + view * (std::max (W, H) * 4.0f) + onPlane;
                d = -view;
            }
            else
            {
                o = target + view * camL;
                d = normalise (target + onPlane - o);
            }
        }

        float map (V3 p, int& part) const        { return scene.map (p, part); }
        float mapSolid (V3 p, int& part) const   { return scene.mapSolid (p, part); }

        V3 normalAt (V3 p, int onPart = -1) const
        {
            const float h = std::max (std::max (0.02f, pixel * 0.35f), onPart >= 0 ? scene.normalStep (onPart) : 0.0f);
            int part = 0;
            const V3 k1 { 1, -1, -1 }, k2 { -1, -1, 1 }, k3 { -1, 1, -1 }, k4 { 1, 1, 1 };
            return normalise (k1 * map (p + k1 * h, part) + k2 * map (p + k2 * h, part) + k3 * map (p + k3 * h, part) + k4 * map (p + k4 * h, part));
        }

        float softShadow (V3 o, V3 d, float far) const
        {
            float res = 1.0f, t = 0.05f;
            int part = 0;

            // Its steps grow with how far it has gone, so a tall one's long shadow reaches its end.
            for (int i = 0; i < 64 && t < far; ++i)
            {
                const float h = mapSolid (o + d * t, part);

                if (h < 0.002f)
                    return 0.0f;

                res = std::min (res, light.shadowSharpness * h / t);
                t += juce::jlimit (0.05f, std::max (4.0f, t * 0.15f), h * scene.stepScale);
            }

            return clamp01 (res);
        }

        float occlusion (V3 p, V3 n) const
        {
            float occ = 0.0f, weight = 1.0f;
            int part = 0;
            const float size = scene.aoSize > 0.0f ? scene.aoSize : std::max (2.0f, std::max (scene.extentX, scene.extentZ) * 0.12f);

            for (int i = 1; i <= 5; ++i)
            {
                const float h = size * (float) i / 5.0f;
                occ += (h - map (p + n * h, part)) * weight;
                weight *= 0.6f;
            }

            return clamp01 (1.0f - occ / size * 1.1f);
        }

        /** Where a ray meets the object's box (false: never). */
        bool box (V3 o, V3 d, float& t0, float& t1) const
        {
            const V3 lo { -scene.extentX, -2.0f, -scene.extentZ }, hi { scene.extentX, scene.top, scene.extentZ };
            t0 = 0.0f; t1 = 1.0e9f;

            for (int k = 0; k < 3; ++k)
            {
                const float oo = k == 0 ? o.x : (k == 1 ? o.y : o.z);
                const float dd = k == 0 ? d.x : (k == 1 ? d.y : d.z);
                const float l = k == 0 ? lo.x : (k == 1 ? lo.y : lo.z), h = k == 0 ? hi.x : (k == 1 ? hi.y : hi.z);

                if (std::abs (dd) < 1.0e-9f)
                {
                    if (oo < l || oo > h) return false;
                    continue;
                }

                float a = (l - oo) / dd, b = (h - oo) / dd;
                if (a > b) std::swap (a, b);
                t0 = std::max (t0, a);
                t1 = std::min (t1, b);

                if (t0 > t1) return false;
            }

            return true;
        }

        V3 shadeSurface (V3 p, V3 d, int part) const
        {
            const V3 n = normalAt (p, part);
            const auto s = scene.shade (p, n, part);
            const V3 v = -d;
            const V3 l = light.key;
            const float nl = std::max (0.0f, dot (n, l));
            const float nv = std::max (0.02f, dot (n, v));
            const float sh = nl > 0.0f ? softShadow (p + n * std::max (0.05f, pixel), light.shadowKey, scene.top * 6.0f) : 0.0f;
            const float ao = occlusion (p, n);

            const V3 f0 = mixv (V3 { 0.04f, 0.04f, 0.04f }, s.albedo, s.metal);
            const V3 h = normalise (l + v);
            const float nh = std::max (0.0f, dot (n, h)), vh = std::max (0.0f, dot (v, h));
            const float fw = std::pow (1.0f - vh, 5.0f);
            const V3 fresnel = f0 + (V3 { 1, 1, 1 } - f0) * fw;

            // The key light's highlight: round, or drawn out across the grain (spun, brushed).
            const float rough = std::max (0.04f, s.rough);
            float specular;

            if (s.aniso != 0)
            {
                const V3 t = normalise (s.tangent - n * dot (s.tangent, n));
                const V3 bvec = cross (n, t);
                const float ax = std::max (0.03f, rough * 0.25f), ay = std::max (0.08f, rough * 1.8f);
                const float ht = dot (h, t), hb = dot (h, bvec);
                specular = nl > 0.0f ? std::exp (-((ht * ht) / (ay * ay) + (hb * hb) / (ax * ax)) / std::max (0.05f, nh * nh))
                                       / (4.0f * pi * ax * ay * std::sqrt (std::max (0.01f, nl * nv))) * nl : 0.0f;
                specular = std::min (specular, 40.0f);
            }
            else
            {
                const float a = rough * rough;
                specular = nl > 0.0f ? ggx (nh, a) * smith (nv, nl, rough) / (4.0f * nv * std::max (0.01f, nl)) * nl : 0.0f;
                specular = std::min (specular, 60.0f);
            }

            V3 colour = fresnel * (specular * light.keyPower * sh);

            // The studio round it, reflected (spun and brushed metal: smeared along the grain).
            const V3 r = reflect (d, n);
            const float fv = std::pow (1.0f - nv, 5.0f);
            const V3 fenv = f0 + (V3 { 1, 1, 1 } * std::max (1.0f - rough, f0.y) - f0) * fv;
            V3 envLight;

            if (s.aniso != 0)
            {
                const V3 t = normalise (s.tangent - n * dot (s.tangent, n));

                for (int k = -2; k <= 2; ++k)
                    envLight += light.env (normalise (r + t * (0.22f * (float) k * (0.4f + rough))), rough) * 0.2f;
            }
            else
            {
                envLight = light.env (r, rough);
            }

            colour += envLight * fenv * (0.35f + 0.65f * ao);

            // Its colour, lit (not a metal's: a metal is all reflection).
            const V3 kd = (V3 { 1, 1, 1 } - fresnel) * (1.0f - s.metal);
            const V3 ambient = light.env (n, 1.0f) * 0.55f;
            colour += s.albedo * kd * ((nl * sh * light.keyPower * 0.62f) + (ambient.y * ao));

            // A clear coat over it.
            if (s.coat > 0.0f)
            {
                const float ca = 0.05f * 0.05f;
                const float cs = nl > 0.0f ? ggx (nh, ca) * smith (nv, nl, 0.05f) / (4.0f * nv * std::max (0.01f, nl)) * nl : 0.0f;
                const float cf = 0.04f + 0.96f * std::pow (1.0f - nv, 5.0f);
                colour = colour * (1.0f - cf * s.coat * 0.5f)
                       + (V3 { 1, 1, 1 } * (std::min (cs, 80.0f) * light.keyPower * sh) + light.env (r, 0.05f)) * (cf * s.coat);
            }

            return colour + s.emission;
        }

        /** One sample: its colour (light) and how much of it is there. */
        void sample (float cx, float cy, V3& colour, float& alpha) const
        {
            V3 o, d;
            ray (cx, cy, o, d);
            trace (o, d, false, colour, alpha);
        }

        /** Along a ray: what it meets (solidOnly: through see-through parts), or the panel under it. */
        void trace (V3 o, V3 d, bool solidOnly, V3& colour, float& alpha) const
        {
            float tHit = -1.0f;
            int part = 0;
            float t0, t1;

            if (box (o, d, t0, t1))
            {
                float t = t0;

                for (int i = 0; i < scene.maxSteps && t < t1; ++i)
                {
                    const float h = solidOnly ? mapSolid (o + d * t, part) : map (o + d * t, part);

                    if (h < std::max (0.004f, pixel * 0.08f))
                    {
                        tHit = t;
                        break;
                    }

                    t += h * scene.stepScale;
                }
            }

            const float tGround = d.y < -1.0e-6f ? -o.y / d.y : -1.0f;

            if (tHit > 0.0f && (tGround < 0.0f || tHit < tGround + 0.5f))
            {
                const auto hit = o + d * tHit;
                colour = shadeSurface (hit, d, part);
                alpha = 1.0f;

                // Glass: its reflections over what's behind it, seen through it.
                if (const float glass = solidOnly ? 0.0f : scene.glassOf (part); glass > 0.0f)
                {
                    V3 behind;
                    float behindAlpha = 0.0f;
                    trace (hit + d * std::max (0.5f, pixel), d, true, behind, behindAlpha);
                    colour = colour * (1.0f - glass * 0.55f) + behind * (glass * 0.92f);
                    alpha = clamp01 ((1.0f - glass) + glass * behindAlpha);
                }

                return;
            }

            colour = {};
            alpha = 0.0f;

            if (tGround < 0.0f)
                return;

            // The panel: see-through, darkened by its shadow, painted where it's painted.
            const V3 g = o + d * tGround;
            Paint paint;
            scene.ground (g.x, g.z, paint);

            const float sh = softShadow (g + V3 { 0, 0.05f, 0 }, light.shadowKey, scene.top * 8.0f);
            const float ao = occlusion (g, { 0, 1, 0 });
            const float dark = clamp01 ((1.0f - sh) * 0.75f + (1.0f - ao) * 0.6f) * shadowAlpha;

            // Its own paint, lit and shadowed; then the shadow over what's under it.
            const float lit = (0.25f + 0.75f * sh) * (0.55f + 0.45f * ao);
            colour = paint.colour * (lit * light.keyPower * 0.45f);
            alpha = paint.alpha;
            colour = colour * (1.0f - dark * (1.0f - paint.alpha));
            alpha = alpha + dark * (1.0f - alpha);

            // Light it gives, over all of it.
            if (paint.emissionAlpha > 0.0f)
            {
                colour = colour * (1.0f - paint.emissionAlpha) + paint.emission * paint.emissionAlpha;
                alpha = alpha + paint.emissionAlpha * (1.0f - alpha);
            }
        }

        float toneMap (float v) const
        {
            v = 1.0f - std::exp (-v * 1.25f * light.exposure);
            return std::pow (clamp01 (v), 1.0f / 2.2f);
        }
    };
}

//==============================================================================
namespace
{
    std::atomic<int>& drafts()
    {
        static std::atomic<int> count { 0 };
        return count;
    }
}

namespace
{
    thread_local int forcedDraft = -1;   // a picture made later: as quick as it was asked for (-1: as drafts() says)
}

Render3D::ScopedDraft::ScopedDraft()    { ++drafts(); }
bool Render3D::drafting()                { return forcedDraft >= 0 ? forcedDraft == 1 : drafts() > 0; }

namespace
{
    std::atomic<int>& stretches()
    {
        static std::atomic<int> count { 0 };
        return count;
    }
}

Render3D::ScopedStretch::ScopedStretch()    { ++stretches(); }
Render3D::ScopedStretch::~ScopedStretch()   { --stretches(); }
bool Render3D::stretching()                 { return stretches() > 0; }
Render3D::ScopedDraft::~ScopedDraft()   { --drafts(); }

//==============================================================================
namespace
{
    thread_local juce::Component* painting = nullptr;   // ScopedLater's

    /** The pictures made in the background (makeLater): a thread of its own, the newest first. */
    class Later final : public juce::Thread,
                        private juce::DeletedAtShutdown
    {
    public:
        static Later& get()
        {
            static Later* const instance = new Later();
            return *instance;
        }

        void add (const juce::String& key, const juce::String& place, std::function<void()> job, juce::Component& who)
        {
            Waiters dropped;

            {
                const std::lock_guard<std::mutex> hold (lock);

                if (key == runningKey)
                {
                    runningWho.push_back (&who);
                    return;
                }

                for (auto& waiting : jobs)
                {
                    if (waiting.key == key)
                    {
                        waiting.who.push_back (&who);
                        return;
                    }
                }

                // An older look for the same place, wanted by who alone: only the newest is.
                for (auto it = jobs.begin(); it != jobs.end();)
                {
                    const bool onlyWho = it->place == place && it->who.size() == 1 && it->who.front() == &who;
                    it = onlyWho ? jobs.erase (it) : std::next (it);
                }

                Job fresh;
                fresh.key = key;
                fresh.place = place;
                fresh.work = std::move (job);
                fresh.leanX = viewLeanX;
                fresh.leanY = viewLeanY;
                fresh.slopeX = viewSlopeX;
                fresh.slopeZ = viewSlopeZ;
                fresh.draft = Render3D::drafting();
                fresh.who.push_back (&who);
                jobs.push_back (std::move (fresh));

                while (jobs.size() > 24)
                {
                    for (auto& w : jobs.front().who)
                        dropped.push_back (w);

                    jobs.pop_front();
                }
            }

            repaint (std::move (dropped));
            notify();
        }

    private:
        using Waiters = std::vector<juce::Component::SafePointer<juce::Component>>;

        struct Job
        {
            juce::String key, place;
            std::function<void()> work;
            float leanX = 0, leanY = 0, slopeX = 0, slopeZ = 0;
            bool draft = false;
            Waiters who;
        };

        std::mutex lock;
        std::deque<Job> jobs;
        juce::String runningKey;
        Waiters runningWho;

        Later() : juce::Thread ("3D pictures")
        {
            startThread (juce::Thread::Priority::normal);
        }

        ~Later() override
        {
            stopThread (20000);   // a picture being made is finished first
        }

        static void repaint (Waiters who)
        {
            if (who.empty())
                return;

            juce::MessageManager::callAsync ([who = std::move (who)]
                                             {
                                                 for (const auto& c : who)
                                                     if (c != nullptr)
                                                         c->repaint();
                                             });
        }

        void run() override
        {
            while (! threadShouldExit())
            {
                Job job;

                {
                    const std::lock_guard<std::mutex> hold (lock);

                    if (! jobs.empty())
                    {
                        job = std::move (jobs.back());
                        jobs.pop_back();
                        runningKey = job.key;
                        runningWho = std::move (job.who);
                    }
                }

                if (job.work == nullptr)
                {
                    wait (200);
                    continue;
                }

                {
                    const Render3D::ScopedView view (job.leanX, job.leanY, job.slopeX, job.slopeZ);
                    const juce::ScopedValueSetter<int> quick (forcedDraft, job.draft ? 1 : 0);
                    job.work();
                }

                Waiters done;

                {
                    const std::lock_guard<std::mutex> hold (lock);
                    runningKey = {};
                    done = std::move (runningWho);
                    runningWho.clear();
                }

                repaint (std::move (done));
            }
        }
    };
}

Render3D::ScopedLater::ScopedLater (juce::Component& who) : old (painting)    { painting = &who; }
Render3D::ScopedLater::~ScopedLater()                                         { painting = old; }
juce::Component* Render3D::later()                                            { return painting; }

void Render3D::makeLater (const juce::String& key, const juce::String& place, std::function<void()> job, juce::Component& who)
{
    Later::get().add (key, place, std::move (job), who);
}

Render3D::ScopedView::ScopedView (float leanX, float leanY, float slopeX, float slopeZ)
    : oldX (viewLeanX), oldY (viewLeanY), oldSlopeX (viewSlopeX), oldSlopeZ (viewSlopeZ)
{
    viewLeanX = juce::jlimit (-1.0f, 1.0f, leanX);
    viewLeanY = juce::jlimit (-1.0f, 1.0f, leanY);
    viewSlopeX = juce::jlimit (-70.0f, 70.0f, slopeX);
    viewSlopeZ = juce::jlimit (-70.0f, 70.0f, slopeZ);
}

Render3D::ScopedView::~ScopedView()
{
    viewLeanX = oldX;
    viewLeanY = oldY;
    viewSlopeX = oldSlopeX;
    viewSlopeZ = oldSlopeZ;
}

namespace
{
    /** Every pixel of a picture w x h (pad pixels round the canvas W x H at scale), samples
        each way, on every core. */
    juce::Image renderPicture (const Renderer& renderer, int w, int h, float W, float H, float scale, float pad, int samples,
                               const std::atomic<int>* stop = nullptr, int stopUnless = 0)
    {
        std::vector<juce::PixelARGB> pixels ((size_t) w * (size_t) h);
        std::atomic<int> nextRow { 0 };
        std::atomic<bool> stopped { false };

        auto work = [&]
        {
            for (int y = nextRow++; y < h; y = nextRow++)
            {
                // A newer picture's wanted: this one's left.
                if (stop != nullptr && stop->load() != stopUnless)
                {
                    stopped = true;
                    return;
                }

                for (int x = 0; x < w; ++x)
                {
                    float r = 0, g = 0, b = 0, a = 0;

                    for (int sy = 0; sy < samples; ++sy)
                    {
                        for (int sx = 0; sx < samples; ++sx)
                        {
                            const float px = (float) x + ((float) sx + 0.5f) / (float) samples;
                            const float py = (float) y + ((float) sy + 0.5f) / (float) samples;
                            const float cx = (px - pad) / scale - W * 0.5f, cy = (py - pad) / scale - H * 0.5f;

                            V3 colour;
                            float alpha = 0;
                            renderer.sample (cx, cy, colour, alpha);

                            // Premultiplied, in the picture's colours.
                            r += renderer.toneMap (colour.x) * alpha;
                            g += renderer.toneMap (colour.y) * alpha;
                            b += renderer.toneMap (colour.z) * alpha;
                            a += alpha;
                        }
                    }

                    const float n = 1.0f / (float) (samples * samples);
                    a = clamp01 (a * n);
                    r = std::min (a, r * n);
                    g = std::min (a, g * n);
                    b = std::min (a, b * n);

                    pixels[(size_t) y * (size_t) w + (size_t) x] = juce::PixelARGB ((juce::uint8) juce::roundToInt (a * 255.0f), (juce::uint8) juce::roundToInt (r * 255.0f),
                                                                                    (juce::uint8) juce::roundToInt (g * 255.0f), (juce::uint8) juce::roundToInt (b * 255.0f));
                }
            }
        };

        const int threads = juce::jlimit (1, 32, (int) std::thread::hardware_concurrency());
        std::vector<std::thread> pool;

        for (int i = 1; i < threads; ++i)
            pool.emplace_back (work);

        work();

        for (auto& thread : pool)
            thread.join();

        if (stopped.load())
            return {};

        juce::Image image (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
        {
            juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    *reinterpret_cast<juce::PixelARGB*> (data.getPixelPointer (x, y)) = pixels[(size_t) y * (size_t) w + (size_t) x];
        }

        return image;
    }
}

juce::Image Render3D::renderFrame (const Model3D& model, int canvasWidth, int canvasHeight, float t, float scale, float pad,
                                   float lightAngle, float lightAltitude, int samples)
{
    scale = juce::jlimit (0.05f, 16.0f, scale);
    pad = juce::jlimit (0.0f, 2048.0f, std::ceil (pad));

    const float W = (float) juce::jmax (8, canvasWidth), H = (float) juce::jmax (8, canvasHeight);
    const int w = juce::jmax (1, (int) std::lround (W * scale) + 2 * (int) pad);
    const int h = juce::jmax (1, (int) std::lround (H * scale) + 2 * (int) pad);

    const auto scene = makeScene (model, W, H, juce::jlimit (0.0f, 1.0f, t));
    Renderer renderer (*scene, model, W, H, scale, pad, lightAngle, lightAltitude);
    renderer.fitInto (W, H);   // a tall one seen at an angle: made to fit

    if (samples <= 0)
        samples = Render3D::drafting() ? 1 : ((long long) w * h > 250000 ? 1 : ((long long) w * h > 40000 ? 2 : 3));

    return renderPicture (renderer, w, h, W, H, scale, pad, samples);
}

juce::Image Render3D::renderBody (const Body3D& body, float scale, float lightAngle, float lightAltitude, float cameraDistance, int samples)
{
    scale = juce::jlimit (0.05f, 8.0f, scale);

    const float W = (float) body.width, H = (float) body.height;
    const int w = juce::jmax (1, (int) std::lround (W * scale)), h = juce::jmax (1, (int) std::lround (H * scale));

    // Its view and light settings, as a model's (the renderer reads them from one).
    Model3D view;
    view.set ("tilt", body.tilt);
    view.set ("perspective", 0.0f);
    view.set ("shadow", body.shadow);
    view.set ("softness", body.softness);
    view.set ("exposure", body.exposure);

    const BodyScene scene (body);
    Renderer renderer (scene, view, W, H, scale, 0.0f, lightAngle, lightAltitude);
    renderer.view = renderer.camC;                     // seen from the panel camera, over the window's middle
    renderer.camL = std::max (0.0f, cameraDistance);   // ...from as far as it is (0: from infinitely far)

    if (samples <= 0)
        samples = Render3D::drafting() ? 1 : ((long long) w * h > 600000 ? 1 : 2);

    return renderPicture (renderer, w, h, W, H, scale, 0.0f, samples);
}

namespace
{
    /** The Showroom's camera round its frame (centre, radius), for a picture w x h. */
    void cameraOf (const Render3D::Showroom& showroom, int w, int h, V3& camC, V3& camR, V3& camU, V3& target, float& camL, float& fit)
    {
        const float radius = std::max (1.0f, showroom.radius);

        // Round it (yaw), above or below it (pitch), turned round its line of sight (roll).
        const float yaw = showroom.yaw * pi / 180.0f, pitch = juce::jlimit (-90.0f, 90.0f, showroom.pitch) * pi / 180.0f;
        camC = normalise ({ std::sin (yaw) * std::cos (pitch), std::sin (pitch), std::cos (yaw) * std::cos (pitch) });
        camR = normalise ({ std::cos (yaw), 0.0f, -std::sin (yaw) });
        camU = cross (camC, camR);

        if (std::abs (showroom.roll) > 1.0e-3f)
        {
            const float a = showroom.roll * pi / 180.0f, ca = std::cos (a), sa = std::sin (a);
            const V3 r = camR * ca + camU * sa, u = camU * ca - camR * sa;
            camR = r;
            camU = u;
        }

        const float p = juce::jlimit (0.0f, 100.0f, showroom.perspective) * 0.01f;
        camL = p < 0.005f ? 0.0f : radius * (1.7f + 7.0f * (1.0f - p) * (1.0f - p));

        // Its whole reach filling the picture (zoom: closer), the nearest part a little bigger.
        const float persp = camL > 0.0f ? 1.0f + radius / camL : 1.0f;
        fit = juce::jlimit (0.005f, 200.0f, 0.86f * (float) std::min (w, h) / (2.0f * radius) / persp * juce::jlimit (0.1f, 12.0f, showroom.zoom));

        target = V3 { showroom.centre[0], showroom.centre[1], showroom.centre[2] } + camR * showroom.panX + camU * showroom.panY;
    }

    void withFrame (const Body3D& body, Render3D::Showroom& view)
    {
        if (view.radius > 0.0f)
            return;

        for (int k = 0; k < 3; ++k)
            view.centre[k] = (body.lo[k] + body.hi[k]) * 0.5f;

        view.radius = 0.5f * std::sqrt ((body.hi[0] - body.lo[0]) * (body.hi[0] - body.lo[0]) + (body.hi[1] - body.lo[1]) * (body.hi[1] - body.lo[1])
                                        + (body.hi[2] - body.lo[2]) * (body.hi[2] - body.lo[2]));
    }
}

void Render3D::frameOf (const juce::var& json, int W, int H, Showroom& view)
{
    float lo[3] { 1.0e9f, 1.0e9f, 1.0e9f }, hi[3] { -1.0e9f, -1.0e9f, -1.0e9f };
    const float mirrorAt = Body3D::mirrorAtOf (json, W) - (float) W * 0.5f;

    auto number = [] (const juce::var& v, const char* key, float fallback)
    {
        const auto x = v.getProperty (key, {});
        return x.isVoid() ? fallback : (float) (double) x;
    };

    if (const auto* list = json.getProperty ("parts", {}).getArray())
    {
        for (const auto& p : *list)
        {
            if ((bool) p.getProperty ("hidden", false) || p.getProperty ("op", "add").toString().equalsIgnoreCase ("cut"))
                continue;

            const float w = number (p, "w", 100.0f), h = number (p, "h", 20.0f), d = number (p, "d", 100.0f);
            const float cx = number (p, "x", (float) W * 0.5f) - (float) W * 0.5f, cz = number (p, "z", (float) H * 0.5f) - (float) H * 0.5f;
            const float cy = number (p, "y", 0.0f) + h * 0.5f;
            float hx = w * 0.5f, hy = h * 0.5f, hz = d * 0.5f;

            if (const auto* r = p.getProperty ("rotate", {}).getArray(); r != nullptr && r->size() >= 3
                && std::abs ((double) (*r)[0]) + std::abs ((double) (*r)[1]) + std::abs ((double) (*r)[2]) > 0.01)
                hx = hy = hz = std::sqrt (hx * hx + hy * hy + hz * hz);

            float x0 = cx - hx, x1 = cx + hx, z0 = cz - hz, z1 = cz + hz;

            if (const auto repeat = p.getProperty ("repeat", {}); repeat.isObject())
            {
                const auto kind = repeat.getProperty ("kind", "row").toString();
                const float count = number (repeat, "count", 2.0f), countZ = kind == "grid" ? number (repeat, "countZ", 1.0f) : 1.0f;

                if (kind == "circle")
                {
                    const float reach = number (repeat, "radius", 0.0f) + std::max (hx, hz);
                    x0 = cx - reach; x1 = cx + reach; z0 = cz - reach; z1 = cz + reach;
                }
                else
                {
                    const float ex = number (repeat, "dx", w * 1.5f) * (count - 1.0f), ez = number (repeat, "dz", d * 1.5f) * (countZ - 1.0f);
                    x0 += std::min (0.0f, ex); x1 += std::max (0.0f, ex);
                    z0 += std::min (0.0f, ez); z1 += std::max (0.0f, ez);
                }
            }

            if (p.getProperty ("mirror", {}).toString().containsIgnoreCase ("x"))
            {
                const float m0 = 2.0f * mirrorAt - x1, m1 = 2.0f * mirrorAt - x0;
                x0 = std::min (x0, m0);
                x1 = std::max (x1, m1);
            }

            lo[0] = std::min (lo[0], x0); hi[0] = std::max (hi[0], x1);
            lo[1] = std::min (lo[1], cy - hy); hi[1] = std::max (hi[1], cy + hy);
            lo[2] = std::min (lo[2], z0); hi[2] = std::max (hi[2], z1);
        }
    }

    if (lo[0] > hi[0])
    {
        // Nothing yet: the window.
        lo[0] = -(float) W * 0.5f; hi[0] = (float) W * 0.5f;
        lo[1] = 0.0f; hi[1] = (float) std::min (W, H) * 0.1f;
        lo[2] = -(float) H * 0.5f; hi[2] = (float) H * 0.5f;
    }

    lo[1] = std::min (lo[1], 0.0f);

    for (int k = 0; k < 3; ++k)
        view.centre[k] = (lo[k] + hi[k]) * 0.5f;

    view.radius = std::max (8.0f, 0.5f * std::sqrt ((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2])));
}

juce::Image Render3D::renderShowroom (const Body3D& body, int width, int height, const Showroom& showroomIn,
                                      float lightAngle, float lightAltitude, int samples)
{
    const int w = juce::jmax (8, width), h = juce::jmax (8, height);
    auto showroom = showroomIn;
    withFrame (body, showroom);

    Model3D view;
    view.set ("tilt", 0.0f);
    view.set ("perspective", 0.0f);
    view.set ("shadow", showroom.shadow ? juce::jmax (55.0f, body.shadow) : 0.0f);
    view.set ("softness", body.softness);
    view.set ("exposure", body.exposure);

    BodyScene scene (body);
    scene.setDecal (showroom.decal, showroom.decalScale);
    scene.floorColour = linear (showroom.floor);
    scene.floorAlpha = showroom.floor.getFloatAlpha();
    scene.extentX += 4.0f;
    scene.extentZ += 4.0f;
    const float radius = std::sqrt (scene.extentX * scene.extentX + scene.top * scene.top + scene.extentZ * scene.extentZ);
    scene.floorFade = radius * 0.9f;

    const ScopedView level (0.0f, 0.0f);
    Renderer renderer (scene, view, (float) std::max (w, h) * 50.0f, (float) std::max (w, h) * 50.0f, 1.0f, 0.0f, lightAngle, lightAltitude);
    cameraOf (showroom, w, h, renderer.camC, renderer.camR, renderer.camU, renderer.target, renderer.camL, renderer.fit);
    renderer.view = renderer.camC;
    renderer.offX = renderer.offY = 0.0f;
    renderer.pixel = 1.0f / renderer.fit;

    if (samples <= 0)
        samples = Render3D::drafting() ? 1 : 2;

    return renderPicture (renderer, w, h, (float) w, (float) h, 1.0f, 0.0f, samples, showroom.stop, showroom.stopUnless);
}

Render3D::Camera Render3D::showroomCamera (int width, int height, const Showroom& showroom)
{
    Camera c;
    c.w = juce::jmax (8, width);
    c.h = juce::jmax (8, height);
    V3 camC, camR, camU, target;
    cameraOf (showroom, c.w, c.h, camC, camR, camU, target, c.camL, c.fit);

    auto put = [] (float* to, V3 v) { to[0] = v.x; to[1] = v.y; to[2] = v.z; };
    put (c.c, camC);
    put (c.r, camR);
    put (c.u, camU);
    put (c.target, target);
    return c;
}

bool Render3D::Camera::project (const float p[3], juce::Point<float>& at) const
{
    const V3 C { c[0], c[1], c[2] }, R { r[0], r[1], r[2] }, U { u[0], u[1], u[2] }, T { target[0], target[1], target[2] };
    const V3 rel = V3 { p[0], p[1], p[2] } - T;
    V3 onPlane = rel - C * dot (rel, C);

    if (camL > 0.0f)
    {
        // Where the line from the camera to it crosses the plane through where it looks.
        const float depth = camL - dot (rel, C);

        if (depth <= 1.0e-3f)
            return false;

        onPlane = onPlane * (camL / depth);
    }

    at = { (float) w * 0.5f + dot (onPlane, R) * fit, (float) h * 0.5f - dot (onPlane, U) * fit };
    return true;
}

void Render3D::Camera::ray (juce::Point<float> at, float o[3], float d[3]) const
{
    const V3 C { c[0], c[1], c[2] }, R { r[0], r[1], r[2] }, U { u[0], u[1], u[2] }, T { target[0], target[1], target[2] };
    const float cx = (at.x - (float) w * 0.5f) / fit, cy = (at.y - (float) h * 0.5f) / fit;
    const V3 onPlane = R * cx - U * cy;
    V3 origin, dir;

    if (camL <= 0.0f)
    {
        origin = T + C * 1.0e5f + onPlane;
        dir = -C;
    }
    else
    {
        origin = T + C * camL;
        dir = normalise (T + onPlane - origin);
    }

    o[0] = origin.x; o[1] = origin.y; o[2] = origin.z;
    d[0] = dir.x; d[1] = dir.y; d[2] = dir.z;
}

int Render3D::partAt (const Body3D& body, const Camera& camera, juce::Point<float> at)
{
    float o[3], d[3];
    camera.ray (at, o, d);
    const V3 O { o[0], o[1], o[2] }, D { d[0], d[1], d[2] };

    // Into its box, then along the ray to its surface.
    float t0 = 0.0f, t1 = 1.0e9f;

    for (int k = 0; k < 3; ++k)
    {
        const float oo = o[k], dd = d[k], l = body.lo[k] - 2.0f, h = body.hi[k] + 2.0f;

        if (std::abs (dd) < 1.0e-9f)
        {
            if (oo < l || oo > h)
                return -1;

            continue;
        }

        float a = (l - oo) / dd, b = (h - oo) / dd;

        if (a > b)
            std::swap (a, b);

        t0 = std::max (t0, a);
        t1 = std::min (t1, b);

        if (t0 > t1)
            return -1;
    }

    int part = -1;
    float t = t0;

    for (int i = 0; i < 600 && t < t1; ++i)
    {
        const auto p = O + D * t;
        const float dist = body.distance (p.x, p.y, p.z, part);

        if (dist < 0.05f)
            return part;

        t += std::max (0.02f, dist * 0.8f);
    }

    return -1;
}

bool Render3D::bodyFace (const Body3D& body, float cameraDistance, float x, float y, Face& face)
{
    const float W = (float) body.width, H = (float) body.height;

    Model3D view;
    view.set ("tilt", body.tilt);
    view.set ("perspective", 0.0f);

    const BodyScene scene (body);
    const ScopedView level (0.0f, 0.0f);
    Renderer renderer (scene, view, W, H, 1.0f, 0.0f, 120.0f, 45.0f);
    renderer.view = renderer.camC;
    renderer.camL = std::max (0.0f, cameraDistance);

    // The ray through that point of the window, as the body's picture is drawn...
    V3 o, d;
    renderer.ray (x - W * 0.5f, y - H * 0.5f, o, d);

    float t0, t1;

    if (! renderer.box (o, d, t0, t1))
        return false;

    int part = -1;
    float t = t0, tHit = -1.0f;

    for (int i = 0; i < scene.maxSteps && t < t1; ++i)
    {
        const float h = renderer.map (o + d * t, part);

        if (h < 0.01f)
        {
            tHit = t;
            break;
        }

        t += h * scene.stepScale;
    }

    const float tGround = d.y < -1.0e-6f ? -o.y / d.y : -1.0f;

    if (tHit <= 0.0f || (tGround >= 0.0f && tHit > tGround + 0.5f) || part < 0)
        return false;

    // ...where it meets the body, and which way the face there looks: measured over a few
    // pixels, so a texture's relief (tolex, knurling) doesn't tip what stands on it.
    const V3 p = o + d * tHit;
    const float e = 3.0f;
    int any = 0;
    const V3 n = normalise (V3 { renderer.map (p + V3 { e, 0, 0 }, any) - renderer.map (p - V3 { e, 0, 0 }, any),
                                 renderer.map (p + V3 { 0, e, 0 }, any) - renderer.map (p - V3 { 0, e, 0 }, any),
                                 renderer.map (p + V3 { 0, 0, e }, any) - renderer.map (p - V3 { 0, 0, e }, any) });

    face.part = part;
    face.height = p.y;
    face.slopeX = n.y > 0.05f ? std::atan2 (n.z, n.y) * 180.0f / pi : 0.0f;
    face.slopeZ = n.y > 0.05f ? std::atan2 (n.x, n.y) * 180.0f / pi : 0.0f;
    return true;
}

float Render3D::overhang (const Model3D& model, int canvasWidth, int canvasHeight, float lightAltitude)
{
    const bool glows = model.kind == Model3D::Kind::knob && model.flag ("ring");

    if (model.number ("shadow") <= 0.5f && ! glows)
        return 0.0f;

    // Where it and its shadow land on the picture, the way renderFrame draws it: made to fit
    // (a tall one seen at an angle is drawn smaller), its shadow cast from its top along the
    // shadow light (no lower than 52 degrees) whichever way the light comes from, plus the
    // shadow's soft edge, which grows with how far the shadow falls.
    const float W = (float) juce::jmax (8, canvasWidth), H = (float) juce::jmax (8, canvasHeight);
    const auto scene = makeScene (model, W, H, 0.0f);
    const ScopedView level (0.0f, 0.0f, viewSlopeX, viewSlopeZ);
    Renderer renderer (*scene, model, W, H, 1.0f, 0.0f, 0.0f, lightAltitude);
    renderer.fitInto (W, H);

    // Its longest: the panel's light (what lights it on the panel, not its own) may be any
    // height, and shadows are cast from no lower than 52 degrees.
    const float alt = 52.0f * pi / 180.0f;
    const float along = scene->top / std::tan (alt);                        // how far its top's shadow falls
    const float blur = scene->top / std::sin (alt) / renderer.light.shadowSharpness;   // its soft edge there
    const float vc = std::max (0.2f, dot (renderer.view, renderer.camC));
    float past = 0.0f;

    auto onPicture = [&] (V3 p)
    {
        const V3 onPlane = p - renderer.view * (dot (p, renderer.camC) / vc);
        const float x = (dot (onPlane, renderer.camR) - renderer.offX) * renderer.fit;
        const float y = (-dot (onPlane, renderer.camU) - renderer.offY) * renderer.fit;
        past = std::max (past, std::max (std::abs (x) - W * 0.5f, std::abs (y) - H * 0.5f));
    };

    for (int c = 0; c < 8; ++c)   // it, seen at its angle
        onPicture ({ (c & 1) ? scene->extentX : -scene->extentX, (c & 2) ? scene->top : 0.0f, (c & 4) ? scene->extentZ : -scene->extentZ });

    for (int a = 0; a < 16; ++a)    // its shadow, the light coming from any side: past its foot's edge on that side
    {
        const float az = (float) a * pi / 8.0f, cx = std::cos (az), sz = std::sin (az);
        const float edge = std::abs (cx) * scene->extentX + std::abs (sz) * scene->extentZ;
        const float reach = edge + along + blur * 0.8f;
        onPicture ({ cx * reach, 0.0f, sz * reach });
    }

    if (glows)
        past = std::max (past, std::max (scene->extentX, scene->extentZ) * renderer.fit * 0.25f);

    // A little more for the panel camera's lean and the perspective, and the blur's last fade.
    return std::max (0.0f, past * 1.12f + 6.0f);
}
