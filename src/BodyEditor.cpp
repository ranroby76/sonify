// D:\workspace\Fanan_web_hub\src\BodyEditor.cpp

#include "BodyEditor.h"
#include "Showroom.h"
#include "GalleryHub.h"

#include "Body3D.h"
#include "FontLibrary.h"
#include "MeshField.h"
#include "Model3D.h"
#include "PluginCanvas.h"
#include "Preferences.h"
#include "Render3D.h"
#include "StellaLookAndFeel.h"

#include <array>
#include <cmath>

//==============================================================================
namespace
{
    constexpr float pi = 3.14159265358979f;

    //==========================================================================
    // A part's numbers, as the layout keeps them (window pixels: x, z its middle on the window,
    // y its foot above the floor).
    float numberOf (const juce::var& v, const juce::Identifier& key, float fallback)
    {
        const auto value = v.getProperty (key, {});
        return value.isVoid() || (value.isString() && value.toString().trim().isEmpty()) ? fallback : (float) (double) value;
    }

    void setProperty (const juce::var& v, const juce::Identifier& key, const juce::var& value)
    {
        if (auto* object = v.getDynamicObject())
            object->setProperty (key, value);
    }

    void removeProperty (const juce::var& v, const juce::Identifier& key)
    {
        if (auto* object = v.getDynamicObject())
            object->removeProperty (key);
    }

    juce::var makeObject()    { return juce::var (new juce::DynamicObject()); }

    juce::String colourText (juce::Colour c)    { return "#" + c.toDisplayString (true); }

    juce::Colour colourOf (const juce::var& v, juce::Colour fallback)
    {
        auto text = v.toString().trim().removeCharacters ("#");

        if (text.isEmpty())
            return fallback;

        if (text.length() == 6)
            text = "FF" + text;

        return juce::Colour ((juce::uint32) text.getHexValue64());
    }

    std::array<float, 3> rotationOf (const juce::var& part)
    {
        std::array<float, 3> r { 0, 0, 0 };

        if (const auto* list = part.getProperty ("rotate", {}).getArray(); list != nullptr)
            for (int i = 0; i < juce::jmin (3, list->size()); ++i)
                r[(size_t) i] = (float) (double) (*list)[i];

        return r;
    }

    //==========================================================================
    // Where a part lies: every copy of it (its repeats, its mirror) as the 8 corners of its box,
    // turned as Body3D turns it (world = Rz Ry Rx local), in window pixels (X across, Y up,
    // Z from the window's top).
    struct V3 { float x = 0, y = 0, z = 0; };
    using Corners = std::array<V3, 8>;

    void rotationMatrix (const std::array<float, 3>& degrees, float m[9])
    {
        const float ax = degrees[0] * pi / 180.0f, ay = degrees[1] * pi / 180.0f, az = degrees[2] * pi / 180.0f;
        const float cx = std::cos (ax), sx = std::sin (ax), cy = std::cos (ay), sy = std::sin (ay), cz = std::cos (az), sz = std::sin (az);
        const float Rx[9] { 1, 0, 0, 0, cx, -sx, 0, sx, cx };
        const float Ry[9] { cy, 0, sy, 0, 1, 0, -sy, 0, cy };
        const float Rz[9] { cz, -sz, 0, sz, cz, 0, 0, 0, 1 };
        float a[9];

        auto mul = [] (const float* l, const float* r, float* o)
        {
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    o[i * 3 + j] = l[i * 3] * r[j] + l[i * 3 + 1] * r[3 + j] + l[i * 3 + 2] * r[6 + j];
        };

        mul (Rz, Ry, a);
        mul (a, Rx, m);
    }

    std::vector<Corners> copiesOf (const juce::var& part, float mirrorAt)
    {
        const float w = numberOf (part, "w", 100), h = numberOf (part, "h", 20), d = numberOf (part, "d", 100);
        const V3 centre { numberOf (part, "x", 0), numberOf (part, "y", 0) + h * 0.5f, numberOf (part, "z", 0) };
        float m[9];
        rotationMatrix (rotationOf (part), m);

        // Its copies, in its own frame: an offset, and (round a circle) a turn.
        struct Copy { float ox, oz, turn; };
        std::vector<Copy> copies;
        const auto repeat = part.getProperty ("repeat", {});
        const auto kind = repeat.isObject() ? repeat.getProperty ("kind", "row").toString().toLowerCase() : juce::String();
        const int count = repeat.isObject() ? juce::jlimit (1, 256, (int) numberOf (repeat, "count", 2)) : 1;
        const int countZ = kind == "grid" ? juce::jlimit (1, 256, (int) numberOf (repeat, "countZ", 1)) : 1;
        const float dx = numberOf (repeat, "dx", w * 1.5f), dz = numberOf (repeat, "dz", d * 1.5f), radius = numberOf (repeat, "radius", 0);

        if (kind == "circle")
        {
            for (int k = 0; k < count; ++k)
                copies.push_back ({ 0, 0, 2.0f * pi * (float) k / (float) count });
        }
        else if (kind.isNotEmpty())
        {
            for (int j = 0; j < countZ; ++j)
                for (int i = 0; i < count; ++i)
                    copies.push_back ({ dx * (float) i, kind == "grid" ? dz * (float) j : 0.0f, 0.0f });
        }
        else
        {
            copies.push_back ({ 0, 0, 0 });
        }

        const bool mirrored = part.getProperty ("mirror", {}).toString().containsIgnoreCase ("x");
        std::vector<Corners> result;

        for (const auto& copy : copies)
        {
            Corners corners;

            for (int c = 0; c < 8; ++c)
            {
                float lx = (c & 1) ? w * 0.5f : -w * 0.5f, ly = (c & 2) ? h * 0.5f : -h * 0.5f, lz = (c & 4) ? d * 0.5f : -d * 0.5f;

                if (kind == "circle")
                {
                    lx += radius;
                    const float rx = lx * std::cos (copy.turn) - lz * std::sin (copy.turn);
                    const float rz = lx * std::sin (copy.turn) + lz * std::cos (copy.turn);
                    lx = rx; lz = rz;
                }
                else
                {
                    lx += copy.ox;
                    lz += copy.oz;
                }

                corners[(size_t) c] = { centre.x + m[0] * lx + m[1] * ly + m[2] * lz,
                                        centre.y + m[3] * lx + m[4] * ly + m[5] * lz,
                                        centre.z + m[6] * lx + m[7] * ly + m[8] * lz };
            }

            result.push_back (corners);

            if (mirrored)
            {
                auto other = corners;

                for (auto& p : other)
                    p.x = 2.0f * mirrorAt - p.x;   // across the body's middle

                result.push_back (other);
            }
        }

        return result;
    }

    /** A view's flat picture of a point: from the top (X, Z), the front (X, -Y), the side (Z, -Y). */
    juce::Point<float> flat (V3 p, int view)
    {
        return view == 0 ? juce::Point<float> (p.x, p.z) : (view == 1 ? juce::Point<float> (p.x, -p.y) : juce::Point<float> (p.z, -p.y));
    }

    juce::Path hullOf (std::vector<juce::Point<float>> points)
    {
        juce::Path path;

        if (points.size() < 3)
            return path;

        std::sort (points.begin(), points.end(), [] (auto a, auto b) { return a.x < b.x || (! (b.x < a.x) && a.y < b.y); });
        std::vector<juce::Point<float>> hull (points.size() * 2);
        size_t k = 0;

        auto cross = [] (juce::Point<float> o, juce::Point<float> a, juce::Point<float> b)
        {
            return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
        };

        for (size_t i = 0; i < points.size(); ++i)
        {
            while (k >= 2 && cross (hull[k - 2], hull[k - 1], points[i]) <= 0)
                --k;
            hull[k++] = points[i];
        }

        for (size_t i = points.size() - 1, t = k + 1; i > 0; --i)
        {
            while (k >= t && cross (hull[k - 2], hull[k - 1], points[i - 1]) <= 0)
                --k;
            hull[k++] = points[i - 1];
        }

        hull.resize (k > 0 ? k - 1 : 0);

        if (hull.size() < 2)
            return path;

        path.startNewSubPath (hull[0]);

        for (size_t i = 1; i < hull.size(); ++i)
            path.lineTo (hull[i]);

        path.closeSubPath();
        return path;
    }

    //==========================================================================
    // A profile as points: corners in 0..1 of its plane, each sharp or rounded (round: how much
    // of its two sides the rounding takes, 0..1), written as SVG path data.
    struct ProfilePoint
    {
        float x = 0, y = 0, round = 0;
    };

    juce::String number (float v)
    {
        return juce::String (std::round (v * 1000.0f) / 1000.0f);
    }

    juce::String pathFromPoints (const std::vector<ProfilePoint>& points)
    {
        const auto n = points.size();

        if (n < 3)
            return "M0 0 L1 0 L1 1 L0 1 Z";

        auto at = [&] (size_t i) { return juce::Point<float> (points[i % n].x, points[i % n].y); };

        // Where a rounded corner starts (coming in) and ends (going out).
        auto entry = [&] (size_t i)
        {
            const auto p = at (i), before = at (i + n - 1);
            return p + (before - p) * (juce::jlimit (0.0f, 1.0f, points[i % n].round) * 0.5f);
        };

        auto exit = [&] (size_t i)
        {
            const auto p = at (i), after = at (i + 1);
            return p + (after - p) * (juce::jlimit (0.0f, 1.0f, points[i % n].round) * 0.5f);
        };

        auto text = [] (juce::Point<float> p) { return number (p.x) + " " + number (p.y); };

        juce::String path = "M" + text (points[0].round > 0.0f ? exit (0) : at (0));

        for (size_t i = 1; i <= n; ++i)
        {
            if (points[i % n].round > 0.0f)
                path << " L" << text (entry (i)) << " Q" << text (at (i)) << " " << text (exit (i));
            else if (i < n)
                path << " L" << text (at (i));
        }

        return path + " Z";
    }

    /** A path's first outline as points: its lines' corners, a curve's control point as a
        rounded corner. */
    std::vector<ProfilePoint> pointsFromPath (const juce::String& svg)
    {
        std::vector<ProfilePoint> points;
        const auto path = juce::Drawable::parseSVGPath (svg);
        juce::Path::Iterator it (path);
        bool started = false;

        while (it.next())
        {
            using T = juce::Path::Iterator::PathElementType;

            if (it.elementType == T::startNewSubPath)
            {
                if (started)
                    break;   // the first outline only

                started = true;
                points.push_back ({ it.x1, it.y1, 0 });
            }
            else if (it.elementType == T::lineTo)
            {
                points.push_back ({ it.x1, it.y1, 0 });
            }
            else if (it.elementType == T::quadraticTo || it.elementType == T::cubicTo)
            {
                // The line came to where the rounding starts: the corner is its control point.
                const juce::Point<float> control = it.elementType == T::quadraticTo ? juce::Point<float> (it.x1, it.y1)
                                                                                    : juce::Point<float> ((it.x1 + it.x2) * 0.5f, (it.y1 + it.y2) * 0.5f);
                const juce::Point<float> end = it.elementType == T::quadraticTo ? juce::Point<float> (it.x2, it.y2) : juce::Point<float> (it.x3, it.y3);
                juce::Point<float> start = control;

                if (! points.empty())
                {
                    start = { points.back().x, points.back().y };
                    points.pop_back();
                }

                const auto before = points.empty() ? start : juce::Point<float> (points.back().x, points.back().y);
                const auto side = juce::jmax (1.0e-4f, before.getDistanceFrom (control));
                const auto round = juce::jlimit (0.1f, 1.0f, 2.0f * start.getDistanceFrom (control) / side);
                points.push_back ({ control.x, control.y, round });
                points.push_back ({ end.x, end.y, -1 });   // where it comes out: dropped below
            }
            else if (it.elementType == T::closePath)
            {
                break;
            }
        }

        std::vector<ProfilePoint> cleaned;

        for (size_t i = 0; i < points.size(); ++i)
        {
            // A curve's way out is no corner of its own (unless nothing follows it).
            if (points[i].round < 0.0f)
            {
                if (i + 1 < points.size())
                    continue;

                points[i].round = 0.0f;
            }

            cleaned.push_back (points[i]);
        }

        // The way back to the start, when it's the start again.
        if (cleaned.size() > 3)
        {
            const auto& first = cleaned.front();
            const auto& last = cleaned.back();

            if (std::abs (first.x - last.x) < 1.0e-3f && std::abs (first.y - last.y) < 1.0e-3f)
                cleaned.pop_back();
        }

        // A rounded first corner written as "M (its way out) ... Q (it) (its way out)": the
        // first point is its way out, the last the corner itself.
        if (cleaned.size() > 3 && cleaned.back().round > 0.0f)
        {
            const auto& first = cleaned.front();
            const auto& corner = cleaned.back();

            if (first.round <= 0.0f && std::abs (first.x - corner.x) + std::abs (first.y - corner.y) < 0.5f)
            {
                cleaned.erase (cleaned.begin());
                std::rotate (cleaned.rbegin(), cleaned.rbegin() + 1, cleaned.rend());
            }
        }

        return cleaned;
    }

    std::vector<ProfilePoint> pointsOf (const juce::var& part)
    {
        std::vector<ProfilePoint> points;

        if (const auto* list = part.getProperty ("points", {}).getArray(); list != nullptr)
            for (const auto& p : *list)
                if (const auto* xy = p.getArray(); xy != nullptr && xy->size() >= 2)
                    points.push_back ({ (float) (double) (*xy)[0], (float) (double) (*xy)[1], xy->size() > 2 ? (float) (double) (*xy)[2] : 0.0f });

        if (points.size() >= 3)
            return points;

        const auto kind = part.getProperty ("kind", "box").toString();
        return pointsFromPath (part.getProperty ("profile", kind == "revolve" ? "M0 0 L1 0 L1 1 L0 1 Z" : "M0 0 L1 0 L1 1 L0 1 Z").toString());
    }

    void writePoints (const juce::var& part, const std::vector<ProfilePoint>& points)
    {
        juce::Array<juce::var> list;

        for (const auto& p : points)
        {
            juce::Array<juce::var> xy { std::round (p.x * 1000.0f) / 1000.0f, std::round (p.y * 1000.0f) / 1000.0f };

            if (p.round > 0.0f)
                xy.add (std::round (p.round * 100.0f) / 100.0f);

            list.add (xy);
        }

        setProperty (part, "points", list);
        setProperty (part, "profile", pathFromPoints (points));
    }

    //==========================================================================
    // The profile presets: (x, y, round) in 0..1.
    struct Preset
    {
        const char* name;
        std::vector<ProfilePoint> points;
    };

    const std::vector<Preset>& extrudePresets()
    {
        static const std::vector<Preset> presets {
            { "Brick",                   { { 0, 0, 0.3f }, { 1, 0, 0.3f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Wedge (low at the front)", { { 0, 0, 0.2f }, { 0.12f, 0, 0.3f }, { 1, 0.55f, 0.3f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Keyboard step",           { { 0, 0.05f, 0.4f }, { 0.58f, 0, 0.3f }, { 0.6f, 0.46f, 0.3f }, { 1, 0.5f, 0.4f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Sloped panel at the back", { { 0, 0, 0.3f }, { 0.35f, 0, 0 }, { 0.5f, 0.45f, 0 }, { 1, 0.5f, 0.3f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Rounded front",           { { 0, 0, 0.2f }, { 0.75f, 0, 0.9f }, { 1, 0.5f, 0.9f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Desk (rises to the back)", { { 0, 0, 0.2f }, { 0.1f, 0, 0.2f }, { 1, 0.6f, 0.2f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Vintage cheek",           { { 0, 0.35f, 0.5f }, { 0.55f, 0, 0.3f }, { 0.64f, 0, 0.4f }, { 0.68f, 0.6f, 0.2f }, { 1, 0.62f, 0.3f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Trapezoid",               { { 0.15f, 0, 0.2f }, { 0.85f, 0, 0.2f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Meter bridge",            { { 0, 0, 0.3f }, { 0.22f, 0, 0.2f }, { 0.3f, 0.55f, 0.2f }, { 1, 0.6f, 0.3f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Pedal (sloped front)",    { { 0, 0.05f, 0.4f }, { 0.55f, 0, 0.3f }, { 1, 0.45f, 0.4f }, { 1, 1, 0 }, { 0, 1, 0 } } },
        };

        return presets;
    }

    const std::vector<Preset>& revolvePresets()
    {
        static const std::vector<Preset> presets {
            { "Cylinder",         { { 0, 0, 0 }, { 1, 0, 0.3f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Cone",             { { 0, 0, 0 }, { 0.1f, 0, 0.5f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Dome",             { { 0, 0, 0 }, { 0.6f, 0.05f, 1 }, { 1, 0.7f, 0.6f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Knob with a skirt", { { 0, 0, 0 }, { 0.6f, 0, 0.4f }, { 0.65f, 0.7f, 0.3f }, { 1, 0.78f, 0.3f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Rubber boot",      { { 0, 0, 0 }, { 0.35f, 0, 0.5f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Bell",             { { 0, 0, 0 }, { 0.25f, 0, 0.5f }, { 0.35f, 0.5f, 0.8f }, { 1, 0.9f, 0.5f }, { 1, 1, 0 }, { 0, 1, 0 } } },
            { "Washer",           { { 0.5f, 0, 0.3f }, { 1, 0, 0.3f }, { 1, 1, 0 }, { 0.5f, 1, 0 } } },
        };

        return presets;
    }

    //==========================================================================
    // The templates, made for a 900 x 460 window and fitted to the one they're used on.
    const char* const templateJson[] = {
        // Desktop module
        R"({ "tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": { "units": "mm", "mmWidth": 420 },
             "parts": [
              { "name": "Case", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 800, "h": 56, "d": 420, "round": 8, "material": "Painted metal", "color": "#FF1E1F23" },
              { "name": "Panel", "kind": "box", "x": 450, "z": 230, "y": 54, "w": 776, "h": 4, "d": 398, "round": 3, "material": "Brushed aluminium", "color": "#FFB9BDC4" },
              { "name": "Wood cheek", "kind": "box", "x": 30, "z": 230, "y": 0, "w": 44, "h": 74, "d": 440, "round": 6, "material": "Satin plastic", "color": "#FF6B3A1E", "surface": "wood", "mirror": "x" },
              { "name": "Screws", "kind": "cylinder", "x": 76, "z": 46, "y": 57, "w": 10, "h": 3, "d": 10, "round": 1, "material": "Steel", "color": "#FFC8CCD2", "mirror": "x",
                "repeat": { "kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 368 } } ] })",

        // Keyboard synth
        R"({ "tilt": 0, "shadow": 70, "floor": "#00000000", "editor": { "units": "mm", "mmWidth": 620 },
             "parts": [
              { "name": "Chassis", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 860, "h": 70, "d": 420, "round": 6,
                "points": [ [0, 0.05, 0.4], [0.58, 0, 0.3], [0.6, 0.46, 0.3], [1, 0.5, 0.4], [1, 1], [0, 1] ], "material": "Painted metal", "color": "#FF16171A" },
              { "name": "Keybed", "kind": "box", "op": "cut", "x": 450, "z": 350, "y": 30, "w": 820, "h": 20, "d": 120, "round": 3, "blend": 2 },
              { "name": "Screen well", "kind": "box", "op": "cut", "x": 250, "z": 110, "y": 64, "w": 130, "h": 12, "d": 46, "round": 2 },
              { "name": "Screen", "kind": "box", "x": 250, "z": 110, "y": 64, "w": 126, "h": 3, "d": 42, "glass": 50, "color": "#FF0A1426", "glow": "#FF102850", "glowStrength": 30 },
              { "name": "Accent line", "kind": "box", "x": 480, "z": 180, "y": 69.6, "w": 760, "h": 0.8, "d": 3, "material": "Gloss plastic", "color": "#FF2F7BFF" },
              { "name": "Screws", "kind": "cylinder", "x": 28, "z": 58, "y": 69, "w": 11, "d": 11, "h": 3, "round": 1.2, "material": "Steel", "color": "#FFC8CCD2", "mirror": "x" },
              { "name": "Vents", "kind": "box", "op": "cut", "x": 470, "z": 60, "y": 66, "w": 4, "h": 10, "d": 30, "round": 1.5, "repeat": { "kind": "row", "count": 12, "dx": 9 } } ] })",

        // Vintage synth (sloped panel)
        R"({ "tilt": 0, "shadow": 70, "floor": "#00000000", "editor": { "units": "mm", "mmWidth": 760 },
             "parts": [
              { "name": "Case", "kind": "extrude", "plane": "side", "x": 450, "z": 235, "y": 0, "w": 780, "h": 90, "d": 410, "round": 4,
                "points": [ [0, 0, 0.3], [0.5, 0.3, 0.1], [0.52, 0.62, 0.15], [1, 0.64, 0.3], [1, 1], [0, 1] ],
                "material": "Matte plastic", "color": "#FF1C1C1E", "surface": "tolex", "surfaceDepth": 60 },
              { "name": "Sloped panel", "kind": "extrude", "plane": "side", "x": 450, "z": 235, "y": 0, "w": 740, "h": 90, "d": 410,
                "points": [ [0.03, -0.035], [0.49, 0.265], [0.49, 0.32], [0.03, 0.02] ], "material": "Brushed aluminium", "color": "#FFB4B8BE" },
              { "name": "Cheek", "kind": "extrude", "plane": "side", "x": 45, "z": 235, "y": 0, "w": 26, "h": 110, "d": 430, "round": 5, "mirror": "x",
                "points": [ [0, 0.35, 0.5], [0.55, 0, 0.3], [0.64, 0, 0.4], [0.68, 0.6, 0.2], [1, 0.62, 0.3], [1, 1], [0, 1] ], "material": "Satin plastic", "color": "#FF6B3A1E", "surface": "wood" },
              { "name": "Keys well", "kind": "box", "op": "cut", "x": 450, "z": 360, "y": 22, "w": 700, "h": 40, "d": 100, "blend": 3 },
              { "name": "Badge", "kind": "text", "text": "MELLOW", "bold": true, "x": 450, "z": 250, "y": 54, "w": 150, "h": 2, "d": 20, "material": "Gold", "color": "#FFE0B860" } ] })",

        // Drum machine (wedge)
        R"({ "tilt": 0, "shadow": 65, "floor": "#00000000", "editor": { "units": "mm", "mmWidth": 600 },
             "parts": [
              { "name": "Wedge", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 860, "h": 80, "d": 440, "round": 5,
                "points": [ [0, 0, 0.2], [0.12, 0, 0.3], [1, 0.55, 0.3], [1, 1], [0, 1] ], "material": "Painted metal", "color": "#FFA9A69F" },
              { "name": "Front strip", "kind": "box", "x": 450, "z": 452, "y": 0, "w": 860, "h": 30, "d": 8, "round": 3, "material": "Satin plastic", "color": "#FF1E1E22" },
              { "name": "Side", "kind": "extrude", "plane": "side", "x": 12, "z": 230, "y": 0, "w": 22, "h": 86, "d": 446, "round": 4, "mirror": "x",
                "points": [ [0, 0, 0.2], [0.12, 0, 0.3], [1, 0.55, 0.3], [1, 1], [0, 1] ], "material": "Satin plastic", "color": "#FF1E1E22" } ] })",

        // Groovebox
        R"({ "tilt": 0, "shadow": 65, "floor": "#00000000", "editor": { "units": "mm", "mmWidth": 360 },
             "parts": [
              { "name": "Slab", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 860, "h": 40, "d": 430, "round": 14, "material": "Matte plastic", "color": "#FF2B2D31" },
              { "name": "Screen well", "kind": "box", "op": "cut", "x": 230, "z": 90, "y": 34, "w": 260, "h": 10, "d": 90, "round": 4 },
              { "name": "Screen", "kind": "box", "x": 230, "z": 90, "y": 33, "w": 252, "h": 3, "d": 82, "round": 2, "glass": 50, "color": "#FF081018", "glow": "#FF0C2A3C", "glowStrength": 35 },
              { "name": "Pad well", "kind": "box", "op": "cut", "x": 640, "z": 280, "y": 37, "w": 380, "h": 6, "d": 250, "round": 4 },
              { "name": "Speaker", "kind": "box", "x": 120, "z": 330, "y": 38, "w": 160, "h": 4, "d": 160, "round": 8, "material": "Steel", "color": "#FF6E7278", "surface": "grille" } ] })",

        // Mixer with a meter bridge
        R"({ "tilt": 0, "shadow": 65, "floor": "#00000000", "editor": { "units": "mm", "mmWidth": 700 },
             "parts": [
              { "name": "Console", "kind": "extrude", "plane": "side", "x": 450, "z": 220, "y": 0, "w": 860, "h": 100, "d": 400, "round": 5,
                "points": [ [0, 0, 0.3], [0.22, 0, 0.2], [0.3, 0.55, 0.2], [1, 0.6, 0.3], [1, 1], [0, 1] ], "material": "Painted metal", "color": "#FF2E3036" },
              { "name": "Armrest", "kind": "box", "x": 450, "z": 438, "y": 0, "w": 860, "h": 48, "d": 40, "round": 18, "material": "Matte plastic", "color": "#FF141416", "surface": "leather" },
              { "name": "Side", "kind": "box", "x": 8, "z": 230, "y": 0, "w": 16, "h": 104, "d": 460, "round": 6, "material": "Satin plastic", "color": "#FF5A3218", "surface": "wood", "mirror": "x" } ] })",

        // Stompbox
        R"({ "tilt": 0, "shadow": 70, "floor": "#00000000", "editor": { "units": "mm", "mmWidth": 150 },
             "parts": [
              { "name": "Enclosure", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 700, "h": 90, "d": 440, "round": 10,
                "points": [ [0, 0.05, 0.4], [0.55, 0, 0.3], [1, 0.45, 0.4], [1, 1], [0, 1] ], "material": "Painted metal", "color": "#FF2A62C8" },
              { "name": "Footswitch", "kind": "cylinder", "x": 450, "z": 360, "y": 62, "w": 46, "h": 12, "d": 46, "round": 3, "material": "Chrome", "color": "#FFDADDE2" },
              { "name": "LED", "kind": "sphere", "x": 450, "z": 60, "y": 86, "w": 12, "h": 10, "d": 12, "glass": 40, "color": "#FFFF3020", "glow": "#FFFF3020", "glowStrength": 80 } ] })",

        // Eurorack case
        R"({ "tilt": 0, "shadow": 60, "floor": "#00000000", "editor": { "units": "mm", "mmWidth": 430 },
             "parts": [
              { "name": "Floor", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 830, "h": 46, "d": 404, "material": "Matte plastic", "color": "#FF151517" },
              { "name": "Rails", "kind": "box", "x": 450, "z": 14, "y": 44, "w": 830, "h": 12, "d": 28, "round": 2, "material": "Brushed aluminium", "color": "#FFB8BCC2",
                "repeat": { "kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 432 } },
              { "name": "Rail slots", "kind": "box", "op": "cut", "x": 450, "z": 14, "y": 54, "w": 822, "h": 4, "d": 5,
                "repeat": { "kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 432 } },
              { "name": "Side", "kind": "box", "x": 17, "z": 230, "y": 0, "w": 34, "h": 70, "d": 460, "round": 5, "material": "Satin plastic", "color": "#FF7A4A26", "surface": "wood", "mirror": "x" } ] })",

        // Guitar pedal (treadle stomp)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 73}, "parts": [
              {"name": "Base", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 400, "h": 78, "d": 440, "round": 8, "points": [[0, 0, 0.3], [0.4, 0, 0.2], [0.43, 0.42, 0.2], [1, 0.5, 0.4], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FFE9B526"},
              {"name": "Treadle", "kind": "extrude", "plane": "side", "x": 450, "z": 330, "y": 36, "w": 372, "h": 40, "d": 236, "round": 6, "points": [[0, 0, 0.3], [1, 0.62, 0.5], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FFE9B526"},
              {"name": "Rubber", "kind": "extrude", "plane": "side", "x": 450, "z": 336, "y": 36, "w": 330, "h": 40, "d": 200, "points": [[0, -0.1, 0.2], [1, 0.52, 0.3], [1, 0.6], [0, -0.02]], "material": "Rubber", "color": "#FF161618", "surface": "rubber", "surfaceDepth": 60},
              {"name": "Hinge gap", "kind": "box", "op": "cut", "x": 450, "z": 210, "y": 40, "w": 380, "h": 40, "d": 5},
              {"name": "Name plate", "kind": "box", "x": 450, "z": 118, "y": 77, "w": 300, "h": 1.5, "d": 44, "round": 3, "material": "Matte plastic", "color": "#FF18181A"},
              {"name": "Name", "kind": "text", "text": "OVERDRIVE", "bold": true, "x": 450, "z": 118, "y": 78.5, "w": 210, "h": 1.5, "d": 24, "material": "Satin plastic", "color": "#FFE9B526"},
              {"name": "LED", "kind": "sphere", "x": 450, "z": 168, "y": 74, "w": 10, "h": 8, "d": 10, "glass": 40, "color": "#FFFF3020", "glow": "#FFFF3020", "glowStrength": 80},
              {"name": "Jack", "kind": "cylinder", "axis": "x", "x": 248, "z": 150, "y": 34, "w": 16, "h": 26, "d": 26, "round": 2, "material": "Chrome", "color": "#FFD8DBE0", "mirror": "x"},
              {"name": "Screws", "kind": "cylinder", "x": 290, "z": 24, "y": 76, "w": 9, "h": 2, "d": 9, "round": 1, "material": "Steel", "color": "#FFC8CCD2", "mirror": "x"}]})",

        // Guitar pedal (wah rocker)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 102}, "parts": [
              {"name": "Base", "kind": "box", "x": 450, "z": 232, "y": 0, "w": 330, "h": 46, "d": 430, "round": 16, "chamfer": 0, "material": "Painted metal", "color": "#FF262729"},
              {"name": "Treadle", "kind": "box", "x": 450, "z": 228, "y": 56, "w": 300, "h": 16, "d": 410, "round": 12, "rotate": [-8, 0, 0], "material": "Painted metal", "color": "#FF2B2C30"},
              {"name": "Grip", "kind": "box", "x": 450, "z": 238, "y": 70, "w": 262, "h": 4, "d": 330, "round": 8, "rotate": [-8, 0, 0], "material": "Rubber", "color": "#FF141416", "surface": "rubber", "surfaceDepth": 70},
              {"name": "Pivot", "kind": "cylinder", "axis": "x", "x": 290, "z": 238, "y": 40, "w": 26, "h": 40, "d": 40, "round": 3, "material": "Brushed aluminium", "color": "#FFB8BCC2", "mirror": "x"},
              {"name": "Gear", "kind": "cylinder", "axis": "x", "x": 297, "z": 238, "y": 36, "w": 10, "h": 48, "d": 48, "round": 2, "material": "Steel", "color": "#FF8A8E95", "surface": "knurl", "mirror": "x"},
              {"name": "Badge", "kind": "text", "text": "WAH", "bold": true, "x": 450, "z": 104, "y": 86, "w": 110, "h": 2, "d": 36, "rotate": [-8, 0, 0], "material": "Chrome", "color": "#FFDDE0E4"},
              {"name": "Jack", "kind": "cylinder", "axis": "x", "x": 280, "z": 380, "y": 20, "w": 16, "h": 24, "d": 24, "round": 2, "material": "Chrome", "color": "#FFD8DBE0", "mirror": "x"}]})",

        // Monosynth (flip-up panel)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 720}, "parts": [
              {"name": "Case", "kind": "box", "x": 450, "z": 346, "y": 0, "w": 820, "h": 56, "d": 228, "round": 5, "material": "Satin plastic", "color": "#FF5E3519", "surface": "wood"},
              {"name": "Keys well", "kind": "box", "op": "cut", "x": 450, "z": 372, "y": 26, "w": 760, "h": 40, "d": 150, "blend": 3},
              {"name": "Panel back", "kind": "box", "x": 450, "z": 148, "y": 0, "w": 820, "h": 96, "d": 170, "round": 4, "material": "Matte plastic", "color": "#FF151517"},
              {"name": "Panel", "kind": "extrude", "plane": "side", "x": 450, "z": 151, "y": 0, "w": 820, "h": 200, "d": 176, "points": [[0, 0], [1, 0.6], [1, 0.66], [0, 0.06]], "material": "Painted metal", "color": "#FF141416"},
              {"name": "Cheek", "kind": "extrude", "plane": "side", "x": 24, "z": 230, "y": 0, "w": 32, "h": 204, "d": 440, "round": 6, "mirror": "x", "points": [[0, 0, 0.4], [0.12, 0, 0.3], [0.52, 0.6, 0.2], [1, 0.68, 0.4], [1, 1], [0, 1]], "material": "Satin plastic", "color": "#FF6B3A1E", "surface": "wood"},
              {"name": "Trim", "kind": "box", "x": 450, "z": 244, "y": 66, "w": 820, "h": 6, "d": 10, "round": 2, "material": "Brushed aluminium", "color": "#FFB9BDC4"}]})",

        // Pad sampler (4x4 pads)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 330}, "parts": [
              {"name": "Slab", "kind": "box", "x": 450, "z": 232, "y": 0, "w": 820, "h": 44, "d": 420, "round": 20, "material": "Matte plastic", "color": "#FF34373C"},
              {"name": "Screen well", "kind": "box", "op": "cut", "x": 230, "z": 104, "y": 38, "w": 300, "h": 10, "d": 104, "round": 5},
              {"name": "Screen", "kind": "box", "x": 230, "z": 104, "y": 37, "w": 292, "h": 3, "d": 96, "round": 3, "glass": 50, "color": "#FF081018", "glow": "#FF123247", "glowStrength": 35},
              {"name": "Pad well", "kind": "box", "op": "cut", "x": 643, "z": 273, "w": 352, "d": 338, "y": 38, "h": 10, "round": 10},
              {"name": "Pads", "kind": "box", "x": 520, "z": 150, "y": 33, "w": 72, "h": 12, "d": 72, "round": 9, "material": "Rubber", "color": "#FF6A6E74", "repeat": {"kind": "grid", "count": 4, "countZ": 4, "dx": 82, "dz": 82}},
              {"name": "Feet line", "kind": "box", "x": 450, "z": 444, "y": 0, "w": 760, "h": 8, "d": 6, "round": 2, "material": "Rubber", "color": "#FF111113"}]})",

        // Modular suitcase (3 rows)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 520}, "parts": [
              {"name": "Case", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 870, "h": 84, "d": 446, "round": 14, "shell": 14, "open": "top", "material": "Matte plastic", "color": "#FF19191B", "surface": "tolex", "surfaceDepth": 60},
              {"name": "Floor", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 846, "h": 46, "d": 420, "material": "Matte plastic", "color": "#FF0E0E10"},
              {"name": "Rails", "kind": "box", "x": 450, "z": 38, "y": 44, "w": 842, "h": 10, "d": 14, "round": 2, "material": "Brushed aluminium", "color": "#FFB8BCC2", "repeat": {"kind": "grid", "count": 1, "countZ": 3, "dx": 0, "dz": 138}},
              {"name": "Rails low", "kind": "box", "x": 450, "z": 148, "y": 44, "w": 842, "h": 10, "d": 14, "round": 2, "material": "Brushed aluminium", "color": "#FFB8BCC2", "repeat": {"kind": "grid", "count": 1, "countZ": 3, "dx": 0, "dz": 138}},
              {"name": "Corners", "kind": "box", "x": 22, "z": 22, "y": 0, "w": 46, "h": 90, "d": 46, "round": 10, "material": "Chrome", "color": "#FFD6D9DE", "mirror": "x", "repeat": {"kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 416}},
              {"name": "Latches", "kind": "box", "x": 250, "z": 452, "y": 50, "w": 46, "h": 24, "d": 10, "round": 3, "material": "Chrome", "color": "#FFD6D9DE", "mirror": "x"}]})",

        // Tape echo
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 560}, "parts": [
              {"name": "Case", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 820, "h": 86, "d": 410, "round": 12, "material": "Painted metal", "color": "#FF8C9A7E"},
              {"name": "Top plate", "kind": "box", "x": 450, "z": 230, "y": 84, "w": 790, "h": 3, "d": 380, "round": 8, "material": "Brushed aluminium", "color": "#FFC2C6CC"},
              {"name": "Tape bay", "kind": "box", "op": "cut", "x": 600, "z": 190, "y": 56, "w": 380, "h": 40, "d": 240, "round": 10, "blend": 2},
              {"name": "Bay floor", "kind": "box", "x": 600, "z": 190, "y": 50, "w": 376, "h": 10, "d": 236, "material": "Matte plastic", "color": "#FF101012"},
              {"name": "Tape", "kind": "sweep", "plane": "top", "x": 600, "z": 190, "y": 60, "w": 330, "h": 16, "d": 200, "thickness": 9, "section": "square", "path": "M0.2 0.08 L0.8 0.08 Q0.97 0.08 0.97 0.5 Q0.97 0.92 0.8 0.92 L0.2 0.92 Q0.03 0.92 0.03 0.5 Q0.03 0.08 0.2 0.08 Z", "material": "Gloss plastic", "color": "#FF6A4022"},
              {"name": "Heads", "kind": "box", "x": 520, "z": 284, "y": 60, "w": 26, "h": 18, "d": 20, "round": 3, "material": "Chrome", "color": "#FFD8DBE0", "repeat": {"kind": "row", "count": 4, "dx": 46}},
              {"name": "Capstan", "kind": "cylinder", "x": 760, "z": 280, "y": 60, "w": 22, "h": 24, "d": 22, "material": "Steel", "color": "#FFB0B4BA"},
              {"name": "Pinch roller", "kind": "cylinder", "x": 760, "z": 250, "y": 60, "w": 30, "h": 20, "d": 30, "round": 3, "material": "Rubber", "color": "#FF1A1A1C"},
              {"name": "Glass", "kind": "box", "x": 600, "z": 190, "y": 84, "w": 376, "h": 2, "d": 236, "round": 8, "glass": 92, "color": "#FFDDE8EE"},
              {"name": "Name", "kind": "text", "text": "ECHO", "bold": true, "x": 200, "z": 110, "y": 87, "w": 170, "h": 2, "d": 50, "material": "Painted metal", "color": "#FF2B3A2A"}]})",

        // Amp head (tolex)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 600}, "parts": [
              {"name": "Case", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 860, "h": 60, "d": 420, "round": 16, "material": "Matte plastic", "color": "#FF151516", "surface": "tolex", "surfaceDepth": 70},
              {"name": "Grille", "kind": "box", "x": 450, "z": 150, "y": 58, "w": 790, "h": 3, "d": 220, "round": 6, "material": "Satin plastic", "color": "#FF5C4D38", "surface": "grille", "surfaceScale": 0.6},
              {"name": "Piping", "kind": "sweep", "plane": "top", "x": 450, "z": 150, "y": 58, "w": 804, "h": 6, "d": 234, "thickness": 5, "path": "M0.02 0.02 L0.98 0.02 L0.98 0.98 L0.02 0.98 Z", "material": "Gloss plastic", "color": "#FFEDE8DC"},
              {"name": "Control plate", "kind": "box", "x": 450, "z": 340, "y": 58, "w": 790, "h": 3, "d": 120, "round": 4, "material": "Brushed aluminium", "color": "#FFD0B068"},
              {"name": "Logo", "kind": "text", "text": "BRITANNIA", "bold": true, "x": 450, "z": 150, "y": 61, "w": 320, "h": 3, "d": 56, "material": "Gold", "color": "#FFE6CB7A"},
              {"name": "Corners", "kind": "box", "x": 24, "z": 24, "y": 0, "w": 52, "h": 66, "d": 52, "round": 12, "material": "Chrome", "color": "#FFD6D9DE", "mirror": "x", "repeat": {"kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 412}}]})",

        // Combo amp (top panel)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 700}, "parts": [
              {"name": "Cabinet", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 860, "h": 60, "d": 420, "round": 18, "material": "Matte plastic", "color": "#FFDCCDA8", "surface": "tolex", "surfaceDepth": 50},
              {"name": "Panel well", "kind": "box", "op": "cut", "x": 450, "z": 118, "y": 46, "w": 780, "h": 20, "d": 150, "round": 6, "blend": 2},
              {"name": "Panel", "kind": "box", "x": 450, "z": 118, "y": 44, "w": 776, "h": 4, "d": 146, "round": 4, "material": "Brushed aluminium", "color": "#FFC9CDD3"},
              {"name": "Handle", "kind": "box", "x": 450, "z": 320, "y": 58, "w": 270, "h": 16, "d": 44, "round": 8, "material": "Satin plastic", "color": "#FF2A1A10", "surface": "leather"},
              {"name": "Handle caps", "kind": "box", "x": 310, "z": 320, "y": 58, "w": 34, "h": 12, "d": 50, "round": 4, "material": "Chrome", "color": "#FFD6D9DE", "mirror": "x"},
              {"name": "Piping", "kind": "sweep", "plane": "top", "x": 450, "z": 230, "y": 58, "w": 836, "h": 4, "d": 396, "thickness": 4, "path": "M0.02 0.02 L0.98 0.02 L0.98 0.98 L0.02 0.98 Z", "material": "Gloss plastic", "color": "#FF8A1E1E"}]})",

        // 500-series lunchbox
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 420}, "parts": [
              {"name": "Case", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 820, "h": 70, "d": 400, "round": 6, "shell": 10, "open": "top", "material": "Painted metal", "color": "#FF2C2E33"},
              {"name": "Floor", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 800, "h": 36, "d": 380, "material": "Matte plastic", "color": "#FF0F1012"},
              {"name": "Plates", "kind": "box", "x": 93, "z": 230, "y": 34, "w": 96, "h": 3, "d": 320, "round": 2, "material": "Painted metal", "color": "#FF3A3D44", "repeat": {"kind": "row", "count": 8, "dx": 102}},
              {"name": "Rails", "kind": "box", "x": 450, "z": 50, "y": 36, "w": 800, "h": 10, "d": 26, "round": 2, "material": "Brushed aluminium", "color": "#FFB8BCC2", "repeat": {"kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 360}},
              {"name": "Rail holes", "kind": "cylinder", "op": "cut", "x": 93, "z": 50, "y": 40, "w": 8, "h": 10, "d": 8, "repeat": {"kind": "grid", "count": 8, "countZ": 2, "dx": 102, "dz": 360}},
              {"name": "Wood end", "kind": "box", "x": 22, "z": 230, "y": 0, "w": 36, "h": 80, "d": 430, "round": 6, "material": "Satin plastic", "color": "#FF7A4A26", "surface": "wood", "mirror": "x"}]})",

        // Tube console (wood)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 640}, "parts": [
              {"name": "Cabinet", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 860, "h": 76, "d": 420, "round": 10, "points": [[0, 0, 0.2], [0.75, 0, 0.9], [1, 0.5, 0.9], [1, 1], [0, 1]], "material": "Satin plastic", "color": "#FF5A3218", "surface": "wood"},
              {"name": "Panel", "kind": "box", "x": 450, "z": 190, "y": 74, "w": 770, "h": 3, "d": 290, "round": 10, "material": "Bakelite", "color": "#FFE9DFC6"},
              {"name": "Dial well", "kind": "box", "op": "cut", "x": 270, "z": 110, "y": 70, "w": 320, "h": 10, "d": 80, "round": 8},
              {"name": "Dial", "kind": "box", "x": 270, "z": 110, "y": 70, "w": 312, "h": 4, "d": 72, "round": 6, "glass": 55, "color": "#FFF2D9A0", "glow": "#FFFFB347", "glowStrength": 40},
              {"name": "Brass strip", "kind": "box", "x": 450, "z": 346, "y": 74, "w": 770, "h": 3, "d": 10, "round": 1.5, "material": "Brass", "color": "#FFC9A44E"}]})",

        // Sloped desk console
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 900}, "parts": [
              {"name": "Desk", "kind": "extrude", "plane": "side", "x": 450, "z": 220, "y": 0, "w": 840, "h": 120, "d": 400, "round": 6, "points": [[0, 0, 0.2], [0.1, 0, 0.2], [1, 0.6, 0.2], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FF33353B"},
              {"name": "Panel", "kind": "extrude", "plane": "side", "x": 450, "z": 220, "y": 0, "w": 800, "h": 120, "d": 400, "points": [[0.1, -0.03], [1, 0.57], [1, 0.6], [0.1, 0]], "material": "Brushed aluminium", "color": "#FF9DA1A8"},
              {"name": "Armrest", "kind": "box", "x": 450, "z": 440, "y": 0, "w": 840, "h": 52, "d": 40, "round": 18, "material": "Matte plastic", "color": "#FF141416", "surface": "leather"},
              {"name": "Wood end", "kind": "extrude", "plane": "side", "x": 14, "z": 232, "y": 0, "w": 28, "h": 130, "d": 440, "round": 6, "mirror": "x", "points": [[0, 0, 0.2], [0.1, 0, 0.2], [1, 0.58, 0.2], [1, 1], [0, 1]], "material": "Satin plastic", "color": "#FF6B3A1E", "surface": "wood"}]})",

        // DJ mixer
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 320}, "parts": [
              {"name": "Case", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 380, "h": 64, "d": 440, "round": 12, "material": "Painted metal", "color": "#FF17181A"},
              {"name": "Top", "kind": "box", "x": 450, "z": 230, "y": 62, "w": 356, "h": 3, "d": 416, "round": 8, "material": "Brushed aluminium", "color": "#FF5A5E66"},
              {"name": "Fader slots", "kind": "box", "op": "cut", "x": 390, "z": 320, "y": 56, "w": 6, "h": 20, "d": 120, "round": 2.9, "mirror": "x"},
              {"name": "Crossfader slot", "kind": "box", "op": "cut", "x": 450, "z": 420, "y": 56, "w": 130, "h": 20, "d": 6, "round": 2.9},
              {"name": "Screws", "kind": "cylinder", "x": 282, "z": 26, "y": 64, "w": 9, "h": 2, "d": 9, "round": 1, "material": "Steel", "color": "#FFC8CCD2", "mirror": "x", "repeat": {"kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 408}}]})",

        // DJ controller (two decks)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 640}, "parts": [
              {"name": "Slab", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 870, "h": 40, "d": 410, "round": 22, "material": "Matte plastic", "color": "#FF19191C"},
              {"name": "Mixer plate", "kind": "box", "x": 450, "z": 230, "y": 38, "w": 230, "h": 3, "d": 380, "round": 8, "material": "Brushed aluminium", "color": "#FF4A4E56"},
              {"name": "Jog well", "kind": "cylinder", "op": "cut", "x": 200, "z": 180, "y": 34, "w": 246, "h": 10, "d": 246, "mirror": "x"},
              {"name": "Jog", "kind": "cylinder", "x": 200, "z": 180, "y": 30, "w": 232, "h": 14, "d": 232, "round": 4, "material": "Brushed aluminium", "color": "#FF8E939B", "surface": "brushed", "mirror": "x"},
              {"name": "Jog top", "kind": "cylinder", "x": 200, "z": 180, "y": 42, "w": 150, "h": 3, "d": 150, "round": 1.5, "material": "Rubber", "color": "#FF1A1A1D", "mirror": "x"},
              {"name": "Pads", "kind": "box", "x": 110, "z": 372, "y": 36, "w": 48, "h": 8, "d": 40, "round": 6, "material": "Rubber", "color": "#FF5A5E66", "mirror": "x", "repeat": {"kind": "row", "count": 4, "dx": 58}}]})",

        // Pocket synth (bare board)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 110}, "parts": [
              {"name": "Standoffs", "kind": "cylinder", "x": 110, "z": 90, "y": 0, "w": 18, "h": 18, "d": 18, "material": "Brass", "color": "#FFC9A44E", "mirror": "x", "repeat": {"kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 280}},
              {"name": "Board", "kind": "box", "x": 450, "z": 230, "y": 16, "w": 760, "h": 6, "d": 360, "round": 10, "material": "Matte plastic", "color": "#FF1D6A3B"},
              {"name": "Screen", "kind": "box", "x": 230, "z": 120, "y": 22, "w": 200, "h": 6, "d": 110, "round": 3, "glass": 40, "color": "#FF9FB4A0", "glow": "#FF5B7A5E", "glowStrength": 15},
              {"name": "Chip", "kind": "box", "x": 430, "z": 110, "y": 22, "w": 90, "h": 6, "d": 60, "round": 1, "material": "Matte plastic", "color": "#FF111113"},
              {"name": "Chip 2", "kind": "box", "x": 560, "z": 110, "y": 22, "w": 60, "h": 5, "d": 60, "round": 1, "material": "Matte plastic", "color": "#FF111113"},
              {"name": "Buttons", "kind": "cylinder", "x": 480, "z": 196, "y": 22, "w": 34, "h": 8, "d": 34, "round": 3, "material": "Satin plastic", "color": "#FFE8E8E4", "repeat": {"kind": "grid", "count": 4, "countZ": 4, "dx": 64, "dz": 54}},
              {"name": "Battery", "kind": "box", "x": 160, "z": 330, "y": 22, "w": 150, "h": 16, "d": 60, "round": 6, "material": "Matte plastic", "color": "#FF2A2A2C"},
              {"name": "Silk", "kind": "text", "text": "PO-1", "bold": true, "x": 230, "z": 200, "y": 22, "w": 90, "h": 1, "d": 24, "material": "Satin plastic", "color": "#FFE8E8E4"}]})",

        // Portable keyboard (speakers)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 900}, "parts": [
              {"name": "Case", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 880, "h": 72, "d": 420, "round": 12, "points": [[0, 0, 0.2], [0.75, 0, 0.9], [1, 0.5, 0.9], [1, 1], [0, 1]], "material": "Gloss plastic", "color": "#FFD9DAD6"},
              {"name": "Keys well", "kind": "box", "op": "cut", "x": 450, "z": 360, "y": 30, "w": 800, "h": 60, "d": 120, "blend": 3},
              {"name": "Speaker", "kind": "cylinder", "x": 130, "z": 140, "y": 68, "w": 150, "h": 4, "d": 150, "round": 1.5, "material": "Steel", "color": "#FF4A4D52", "surface": "perforated", "mirror": "x"},
              {"name": "Speaker ring", "kind": "sweep", "plane": "top", "x": 130, "z": 140, "y": 68, "w": 164, "h": 6, "d": 164, "thickness": 5, "section": "round", "path": "M0.5 0.03 A0.47 0.47 0 1 1 0.4999 0.03 Z", "material": "Chrome", "color": "#FFD6D9DE", "mirror": "x"},
              {"name": "Screen", "kind": "box", "x": 450, "z": 110, "y": 66, "w": 220, "h": 6, "d": 70, "round": 4, "glass": 50, "color": "#FF0C1A10", "glow": "#FF2A6040", "glowStrength": 30}]})",

        // Drawbar organ
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 1000}, "parts": [
              {"name": "Case", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 820, "h": 70, "d": 420, "round": 4, "material": "Painted metal", "color": "#FF141416"},
              {"name": "Keys well", "kind": "box", "op": "cut", "x": 450, "z": 368, "y": 30, "w": 800, "h": 60, "d": 130, "blend": 3},
              {"name": "Drawbar slots", "kind": "box", "op": "cut", "x": 96, "z": 120, "y": 58, "w": 12, "h": 20, "d": 110, "round": 5.9, "mirror": "x", "repeat": {"kind": "row", "count": 9, "dx": 26}},
              {"name": "Trim", "kind": "box", "x": 450, "z": 210, "y": 68, "w": 820, "h": 3, "d": 8, "round": 1.5, "material": "Chrome", "color": "#FFD6D9DE"},
              {"name": "Wood end", "kind": "box", "x": 22, "z": 230, "y": 0, "w": 44, "h": 84, "d": 440, "round": 8, "material": "Satin plastic", "color": "#FF5A3218", "surface": "wood", "mirror": "x"}]})",

        // Semi-modular (patch bay)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 420}, "parts": [
              {"name": "Case", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 840, "h": 50, "d": 400, "round": 8, "material": "Painted metal", "color": "#FF161618"},
              {"name": "Panel", "kind": "box", "x": 330, "z": 230, "y": 48, "w": 580, "h": 3, "d": 380, "round": 5, "material": "Brushed aluminium", "color": "#FFC2C6CC"},
              {"name": "Patch panel", "kind": "box", "x": 740, "z": 230, "y": 48, "w": 216, "h": 3, "d": 380, "round": 5, "material": "Anodized", "color": "#FF2A2C31"},
              {"name": "Jack nuts", "kind": "cylinder", "x": 671, "z": 80, "y": 50, "w": 22, "h": 4, "d": 22, "round": 1, "material": "Chrome", "color": "#FFD8DBE0", "repeat": {"kind": "grid", "count": 4, "countZ": 8, "dx": 46, "dz": 43}},
              {"name": "Jack holes", "kind": "cylinder", "op": "cut", "x": 671, "z": 80, "y": 44, "w": 10, "h": 14, "d": 10, "repeat": {"kind": "grid", "count": 4, "countZ": 8, "dx": 46, "dz": 43}},
              {"name": "Side rail", "kind": "box", "x": 24, "z": 230, "y": 0, "w": 24, "h": 58, "d": 420, "round": 6, "material": "Anodized", "color": "#FF8C2F2F", "mirror": "x"}]})",

        // Step sequencer (16 steps)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 480}, "parts": [
              {"name": "Case", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 880, "h": 44, "d": 280, "round": 12, "material": "Anodized", "color": "#FF1F2B4A"},
              {"name": "Steps", "kind": "box", "x": 60, "z": 290, "y": 42, "w": 38, "h": 8, "d": 38, "round": 6, "material": "Satin plastic", "color": "#FFD8D9DC", "repeat": {"kind": "row", "count": 16, "dx": 52}},
              {"name": "Lights", "kind": "sphere", "x": 60, "z": 250, "y": 42, "w": 8, "h": 6, "d": 8, "glass": 40, "color": "#FFFF4030", "glow": "#FFFF4030", "glowStrength": 60, "repeat": {"kind": "row", "count": 16, "dx": 52}},
              {"name": "Screen well", "kind": "box", "op": "cut", "x": 170, "z": 150, "y": 38, "w": 220, "h": 10, "d": 56, "round": 4},
              {"name": "Screen", "kind": "box", "x": 170, "z": 150, "y": 37, "w": 212, "h": 3, "d": 48, "round": 3, "glass": 50, "color": "#FF101010", "glow": "#FFFF6020", "glowStrength": 30}]})",

        // Vintage compressor (meter)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 483}, "parts": [
              {"name": "Front", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 880, "h": 10, "d": 320, "chamfer": 3, "material": "Painted metal", "color": "#FF707A80"},
              {"name": "Ear slots", "kind": "box", "op": "cut", "x": 28, "z": 150, "y": -1, "w": 16, "h": 14, "d": 10, "round": 4.9, "mirror": "x", "repeat": {"kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 160}},
              {"name": "Handles", "kind": "box", "x": 66, "z": 230, "y": 8, "w": 16, "h": 30, "d": 230, "round": 7, "material": "Chrome", "color": "#FFD6D9DE", "mirror": "x"},
              {"name": "Meter well", "kind": "box", "op": "cut", "x": 450, "z": 200, "y": 0, "w": 210, "h": 14, "d": 130, "round": 6},
              {"name": "Meter", "kind": "box", "x": 450, "z": 200, "y": 0, "w": 206, "h": 6, "d": 126, "round": 5, "glass": 45, "color": "#FFF0E2B8", "glow": "#FFFFC060", "glowStrength": 30},
              {"name": "Bezel", "kind": "sweep", "plane": "top", "x": 450, "z": 200, "y": 8, "w": 226, "h": 6, "d": 146, "thickness": 6, "section": "square", "round": 2, "path": "M0.04 0.04 L0.96 0.04 L0.96 0.96 L0.04 0.96 Z", "material": "Chrome", "color": "#FFD6D9DE"}]})",

        // Sampler workstation (floppy)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 520}, "parts": [
              {"name": "Case", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 860, "h": 80, "d": 420, "round": 8, "points": [[0, 0, 0.2], [0.12, 0, 0.3], [1, 0.4, 0.3], [1, 1], [0, 1]], "material": "Satin plastic", "color": "#FFCBC6B6"},
              {"name": "Screen well", "kind": "box", "op": "cut", "x": 260, "z": 110, "y": 66, "w": 340, "h": 20, "d": 130, "round": 4},
              {"name": "Screen", "kind": "box", "x": 260, "z": 110, "y": 64, "w": 332, "h": 4, "d": 122, "round": 3, "glass": 50, "color": "#FF0D1A10", "glow": "#FF2E6E3A", "glowStrength": 35},
              {"name": "Drive", "kind": "box", "x": 690, "z": 96, "y": 76, "w": 220, "h": 4, "d": 56, "round": 3, "material": "Matte plastic", "color": "#FF2A2A2C"},
              {"name": "Drive slot", "kind": "box", "op": "cut", "x": 670, "z": 90, "y": 74, "w": 160, "h": 10, "d": 6, "round": 2.9},
              {"name": "Drive light", "kind": "box", "x": 770, "z": 110, "y": 79, "w": 12, "h": 2, "d": 6, "glass": 40, "color": "#FF30FF60", "glow": "#FF30FF60", "glowStrength": 60},
              {"name": "Pads", "kind": "box", "x": 560, "z": 280, "y": 50, "w": 56, "h": 14, "d": 50, "round": 6, "material": "Rubber", "color": "#FF3A3C40", "repeat": {"kind": "grid", "count": 4, "countZ": 2, "dx": 70, "dz": 66}}]})",

        // Compact mixer (8 channels)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 360}, "parts": [
              {"name": "Console", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 860, "h": 64, "d": 420, "round": 6, "points": [[0, 0, 0.2], [0.1, 0, 0.3], [1, 0.45, 0.3], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FF2A2C30"},
              {"name": "Strip grooves", "kind": "box", "op": "cut", "x": 40, "z": 230, "y": 0, "w": 3, "h": 90, "d": 380, "repeat": {"kind": "row", "count": 9, "dx": 82}},
              {"name": "Fader slots", "kind": "box", "op": "cut", "x": 81, "z": 360, "y": 0, "w": 5, "h": 60, "d": 110, "round": 2.4, "repeat": {"kind": "row", "count": 8, "dx": 82}},
              {"name": "Feet", "kind": "box", "x": 450, "z": 446, "y": 0, "w": 820, "h": 10, "d": 8, "round": 3, "material": "Rubber", "color": "#FF111113"}]})",

        // Aluminium extrusion
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 300}, "parts": [
              {"name": "Extrusion", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 820, "h": 60, "d": 360, "points": [[0, 0, 1], [1, 0, 1], [1, 1], [0, 1]], "material": "Brushed aluminium", "color": "#FFB9BDC4"},
              {"name": "Ribs", "kind": "box", "op": "cut", "x": 450, "z": 60, "y": 40, "w": 830, "h": 30, "d": 3, "repeat": {"kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 340}},
              {"name": "End cap", "kind": "extrude", "plane": "side", "x": 34, "z": 230, "y": 0, "w": 20, "h": 64, "d": 368, "round": 3, "mirror": "x", "points": [[0, 0, 1], [1, 0, 1], [1, 1], [0, 1]], "material": "Matte plastic", "color": "#FF18181A"},
              {"name": "Screws", "kind": "cylinder", "axis": "x", "x": 24, "z": 160, "y": 34, "w": 4, "h": 10, "d": 10, "material": "Steel", "color": "#FFC8CCD2", "mirror": "x", "repeat": {"kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 140}}]})",

        // Modern poly (wood cheeks)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 860}, "parts": [
              {"name": "Chassis", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 820, "h": 72, "d": 420, "round": 6, "points": [[0, 0.05, 0.4], [0.58, 0, 0.3], [0.6, 0.46, 0.3], [1, 0.5, 0.4], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FF121315"},
              {"name": "Cheek", "kind": "extrude", "plane": "side", "x": 24, "z": 230, "y": 0, "w": 32, "h": 80, "d": 430, "round": 6, "mirror": "x", "points": [[0, 0.05, 0.4], [0.58, 0, 0.3], [0.62, 0.42, 0.3], [1, 0.46, 0.4], [1, 1], [0, 1]], "material": "Satin plastic", "color": "#FF7A4A26", "surface": "wood"},
              {"name": "Screen well", "kind": "box", "op": "cut", "x": 450, "z": 90, "y": 66, "w": 240, "h": 12, "d": 50, "round": 3},
              {"name": "Screen", "kind": "box", "x": 450, "z": 90, "y": 65, "w": 232, "h": 3, "d": 42, "round": 2, "glass": 50, "color": "#FF05080C", "glow": "#FF7FB8FF", "glowStrength": 30},
              {"name": "Accent", "kind": "box", "x": 450, "z": 244, "y": 71.2, "w": 760, "h": 0.8, "d": 3, "material": "Gloss plastic", "color": "#FFE0402A"},
              {"name": "Keybed", "kind": "box", "op": "cut", "x": 450, "z": 354, "y": 30, "w": 740, "h": 20, "d": 120, "round": 3, "blend": 2}]})",

        // Retro sci-fi console
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 700}, "parts": [
              {"name": "Hull", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 860, "h": 80, "d": 420, "chamfer": 34, "material": "Painted metal", "color": "#FF2E6E73"},
              {"name": "Deck", "kind": "box", "x": 450, "z": 250, "y": 78, "w": 560, "h": 3, "d": 250, "chamfer": 2, "material": "Brushed aluminium", "color": "#FFB8C2C4"},
              {"name": "Porthole", "kind": "cylinder", "x": 120, "z": 230, "y": 74, "w": 110, "h": 8, "d": 110, "glass": 60, "color": "#FF9CF2E8", "glow": "#FF2AD8C8", "glowStrength": 50, "mirror": "x"},
              {"name": "Porthole ring", "kind": "sweep", "plane": "top", "x": 120, "z": 230, "y": 76, "w": 128, "h": 8, "d": 128, "thickness": 8, "section": "round", "path": "M0.5 0.03 A0.47 0.47 0 1 1 0.4999 0.03 Z", "material": "Brass", "color": "#FFC9A44E", "mirror": "x"},
              {"name": "Rivets", "kind": "sphere", "x": 60, "z": 48, "y": 76, "w": 9, "h": 6, "d": 9, "material": "Steel", "color": "#FFC8CCD2", "repeat": {"kind": "grid", "count": 14, "countZ": 2, "dx": 60, "dz": 364}}]})",

        // Keyboard controller (pads)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 700}, "parts": [
              {"name": "Slab", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 880, "h": 40, "d": 410, "round": 12, "material": "Matte plastic", "color": "#FF19191B"},
              {"name": "Keys well", "kind": "box", "op": "cut", "x": 450, "z": 345, "y": 16, "w": 850, "h": 40, "d": 150, "blend": 3},
              {"name": "Strips", "kind": "box", "op": "cut", "x": 50, "z": 150, "y": 34, "w": 26, "h": 10, "d": 130, "round": 6, "repeat": {"kind": "row", "count": 2, "dx": 44}},
              {"name": "Pads", "kind": "box", "x": 440, "z": 110, "y": 36, "w": 48, "h": 8, "d": 48, "round": 6, "material": "Rubber", "color": "#FF505358", "repeat": {"kind": "grid", "count": 8, "countZ": 2, "dx": 58, "dz": 60}},
              {"name": "Screen", "kind": "box", "x": 230, "z": 110, "y": 38, "w": 150, "h": 3, "d": 50, "round": 3, "glass": 50, "color": "#FF05080C", "glow": "#FF7FB8FF", "glowStrength": 25}]})",

        // Eurorack skiff (slanted)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 440}, "parts": [
              {"name": "Skiff", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 820, "h": 96, "d": 320, "round": 4, "points": [[0, 0, 0.2], [1, 0.62, 0.2], [1, 1], [0, 1]], "material": "Matte plastic", "color": "#FF161618"},
              {"name": "Rails", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 820, "h": 96, "d": 320, "points": [[0.02, -0.06], [0.07, -0.03], [0.07, 0.06], [0.02, 0.03]], "material": "Brushed aluminium", "color": "#FFB8BCC2"},
              {"name": "Rails front", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 820, "h": 96, "d": 320, "points": [[0.92, 0.51], [0.97, 0.54], [0.97, 0.63], [0.92, 0.6]], "material": "Brushed aluminium", "color": "#FFB8BCC2"},
              {"name": "Cheek", "kind": "extrude", "plane": "side", "x": 20, "z": 230, "y": 0, "w": 34, "h": 104, "d": 330, "round": 5, "mirror": "x", "points": [[0, 0, 0.3], [1, 0.62, 0.3], [1, 1], [0, 1]], "material": "Satin plastic", "color": "#FF7A4A26", "surface": "wood"}]})",

        // Boutique desktop (rounded)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 260}, "parts": [
              {"name": "Skirt", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 790, "h": 26, "d": 420, "round": 40, "material": "Rubber", "color": "#FF1A1A1C"},
              {"name": "Shell", "kind": "box", "x": 450, "z": 230, "y": 8, "w": 760, "h": 66, "d": 396, "round": 36, "material": "Gloss plastic", "color": "#FFE8642A"},
              {"name": "Top plate", "kind": "box", "x": 450, "z": 230, "y": 72, "w": 700, "h": 3, "d": 340, "round": 28, "material": "Satin plastic", "color": "#FFF4F1EA"}]})",

        // Transistor radio
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 220}, "parts": [
              {"name": "Body", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 780, "h": 60, "d": 380, "round": 40, "material": "Bakelite", "color": "#FFB22A24"},
              {"name": "Grille", "kind": "box", "x": 290, "z": 230, "y": 58, "w": 380, "h": 4, "d": 300, "round": 30, "material": "Chrome", "color": "#FFC8CCD2", "surface": "perforated"},
              {"name": "Dial", "kind": "cylinder", "x": 650, "z": 200, "y": 56, "w": 210, "h": 12, "d": 210, "round": 4, "material": "Gold", "color": "#FFE0B860"},
              {"name": "Dial face", "kind": "cylinder", "x": 650, "z": 200, "y": 66, "w": 170, "h": 3, "d": 170, "glass": 50, "color": "#FFF6EBCB"},
              {"name": "Badge", "kind": "text", "text": "SEVEN", "bold": true, "x": 650, "z": 350, "y": 58, "w": 140, "h": 2, "d": 34, "material": "Gold", "color": "#FFE0B860"}]})",

        // Looper station (big wheel)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 420}, "parts": [
              {"name": "Slab", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 860, "h": 48, "d": 410, "round": 24, "material": "Matte plastic", "color": "#FF1B1C1F"},
              {"name": "Wheel well", "kind": "cylinder", "op": "cut", "x": 450, "z": 200, "y": 42, "w": 290, "h": 10, "d": 290},
              {"name": "Wheel", "kind": "cylinder", "x": 450, "z": 200, "y": 34, "w": 270, "h": 14, "d": 270, "round": 5, "material": "Rubber", "color": "#FF2A2B2F"},
              {"name": "Wheel cap", "kind": "cylinder", "x": 450, "z": 200, "y": 46, "w": 90, "h": 4, "d": 90, "round": 2, "material": "Brushed aluminium", "color": "#FFB8BCC2"},
              {"name": "Ring lights", "kind": "sphere", "x": 450, "z": 200, "y": 44, "w": 10, "h": 7, "d": 10, "glass": 40, "color": "#FF40FF80", "glow": "#FF40FF80", "glowStrength": 60, "repeat": {"kind": "circle", "count": 24, "radius": 162}},
              {"name": "Buttons", "kind": "box", "x": 150, "z": 380, "y": 44, "w": 80, "h": 10, "d": 40, "round": 6, "material": "Rubber", "color": "#FF4A4D52", "mirror": "x", "repeat": {"kind": "row", "count": 2, "dx": 100}}]})",

        // Tube amp chassis
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 480}, "parts": [
              {"name": "Chassis", "kind": "box", "x": 450, "z": 260, "y": 0, "w": 820, "h": 50, "d": 360, "round": 4, "material": "Steel", "color": "#FFB4B8BE", "surface": "brushed"},
              {"name": "Transformer", "kind": "box", "x": 110, "z": 150, "y": 48, "w": 120, "h": 90, "d": 100, "round": 6, "material": "Painted metal", "color": "#FF1A1A1C", "mirror": "x"},
              {"name": "Bell", "kind": "box", "x": 110, "z": 150, "y": 136, "w": 128, "h": 12, "d": 108, "round": 8, "material": "Chrome", "color": "#FFD6D9DE", "mirror": "x"},
              {"name": "Sockets", "kind": "cylinder", "x": 300, "z": 150, "y": 48, "w": 64, "h": 8, "d": 64, "round": 3, "material": "Bakelite", "color": "#FFE9E4D6", "repeat": {"kind": "row", "count": 4, "dx": 100}},
              {"name": "Tubes", "kind": "cylinder", "x": 300, "z": 150, "y": 54, "w": 46, "h": 110, "d": 46, "round": 20, "glass": 70, "color": "#FFE8D8C0", "glow": "#FFFF8A30", "glowStrength": 55, "repeat": {"kind": "row", "count": 4, "dx": 100}},
              {"name": "Front strip", "kind": "box", "x": 450, "z": 388, "y": 48, "w": 780, "h": 3, "d": 80, "round": 4, "material": "Painted metal", "color": "#FF22242A"}]})",

        // Guitar pedal (mini)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 40}, "parts": [
              {"name": "Enclosure", "kind": "box", "x": 450, "z": 232, "y": 0, "w": 250, "h": 70, "d": 430, "round": 14, "material": "Painted metal", "color": "#FF2AB0A2"},
              {"name": "Footswitch", "kind": "cylinder", "x": 450, "z": 340, "y": 68, "w": 56, "h": 12, "d": 56, "round": 4, "material": "Chrome", "color": "#FFDADDE2"},
              {"name": "Footswitch nut", "kind": "cylinder", "x": 450, "z": 340, "y": 66, "w": 74, "h": 4, "d": 74, "round": 1.5, "material": "Steel", "color": "#FFB0B4BA"},
              {"name": "LED", "kind": "sphere", "x": 450, "z": 262, "y": 67, "w": 10, "h": 8, "d": 10, "glass": 40, "color": "#FF4FA8FF", "glow": "#FF4FA8FF", "glowStrength": 80},
              {"name": "Top jacks", "kind": "cylinder", "axis": "z", "x": 400, "z": 10, "y": 36, "w": 26, "h": 26, "d": 18, "round": 2, "material": "Chrome", "color": "#FFD8DBE0", "mirror": "x"},
              {"name": "Power jack", "kind": "cylinder", "axis": "z", "x": 450, "z": 10, "y": 36, "w": 18, "h": 18, "d": 14, "round": 2, "material": "Matte plastic", "color": "#FF18181A"}]})",

        // Guitar pedal (dual footswitch)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 145}, "parts": [
              {"name": "Enclosure", "kind": "extrude", "plane": "side", "x": 450, "z": 232, "y": 0, "w": 620, "h": 80, "d": 430, "round": 10, "points": [[0, 0, 0.3], [0.5, 0, 0.3], [1, 0.3, 0.4], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FF1E2A44"},
              {"name": "Graphic", "kind": "box", "x": 450, "z": 150, "y": 77, "w": 560, "h": 1.5, "d": 120, "round": 6, "material": "Satin plastic", "color": "#FFD8D2C0"},
              {"name": "Footswitches", "kind": "cylinder", "x": 330, "z": 360, "y": 60, "w": 50, "h": 12, "d": 50, "round": 4, "material": "Chrome", "color": "#FFDADDE2", "repeat": {"kind": "row", "count": 2, "dx": 240}},
              {"name": "LEDs", "kind": "sphere", "x": 330, "z": 282, "y": 68, "w": 10, "h": 8, "d": 10, "glass": 40, "color": "#FFFF3020", "glow": "#FFFF3020", "glowStrength": 80, "repeat": {"kind": "row", "count": 2, "dx": 240}},
              {"name": "Jacks", "kind": "cylinder", "axis": "x", "x": 140, "z": 150, "y": 40, "w": 18, "h": 26, "d": 26, "round": 2, "material": "Chrome", "color": "#FFD8DBE0", "mirror": "x"}]})",

        // Guitar pedal (hammertone)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 120}, "parts": [
              {"name": "Enclosure", "kind": "box", "x": 450, "z": 232, "y": 0, "w": 400, "h": 74, "d": 430, "round": 12, "material": "Painted metal", "color": "#FF4E7A5A", "surface": "leather", "surfaceScale": 0.45, "surfaceDepth": 45},
              {"name": "Brass plate", "kind": "box", "x": 450, "z": 110, "y": 72, "w": 300, "h": 2, "d": 60, "round": 4, "material": "Brass", "color": "#FFC9A44E"},
              {"name": "Name", "kind": "text", "text": "TONE BENDER", "bold": true, "x": 450, "z": 110, "y": 74, "w": 240, "h": 1.5, "d": 24, "material": "Painted metal", "color": "#FF2A2418"},
              {"name": "Footswitch", "kind": "cylinder", "x": 450, "z": 350, "y": 70, "w": 52, "h": 12, "d": 52, "round": 4, "material": "Chrome", "color": "#FFDADDE2"},
              {"name": "Jewel lamp", "kind": "sphere", "x": 450, "z": 260, "y": 68, "w": 22, "h": 16, "d": 22, "glass": 60, "color": "#FFFF5A30", "glow": "#FFFF5A30", "glowStrength": 70},
              {"name": "Jewel bezel", "kind": "cylinder", "x": 450, "z": 260, "y": 70, "w": 30, "h": 4, "d": 30, "round": 1.5, "material": "Chrome", "color": "#FFD6D9DE"}]})",

        // Guitar pedal (slanted, top jacks)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 95}, "parts": [
              {"name": "Enclosure", "kind": "extrude", "plane": "side", "x": 450, "z": 232, "y": 0, "w": 430, "h": 90, "d": 430, "round": 10, "points": [[0, 0, 0.3], [1, 0.45, 0.4], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FFE04A6A"},
              {"name": "Footswitch", "kind": "cylinder", "x": 450, "z": 350, "y": 56, "w": 54, "h": 12, "d": 54, "round": 4, "material": "Chrome", "color": "#FFDADDE2"},
              {"name": "Top jacks", "kind": "cylinder", "axis": "z", "x": 360, "z": 8, "y": 50, "w": 26, "h": 26, "d": 18, "round": 2, "material": "Chrome", "color": "#FFD8DBE0", "mirror": "x"},
              {"name": "Power jack", "kind": "cylinder", "axis": "z", "x": 450, "z": 8, "y": 50, "w": 18, "h": 18, "d": 14, "round": 2, "material": "Matte plastic", "color": "#FF18181A"},
              {"name": "LED", "kind": "sphere", "x": 450, "z": 270, "y": 64, "w": 10, "h": 8, "d": 10, "glass": 40, "color": "#FF30FF60", "glow": "#FF30FF60", "glowStrength": 80}]})",

        // Pedalboard (five pedals)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 610}, "parts": [
              {"name": "Board", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 880, "h": 40, "d": 420, "round": 6, "points": [[0, 0, 0.2], [1, 0.4, 0.2], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FF1A1A1C"},
              {"name": "Rails", "kind": "box", "x": 450, "z": 60, "y": 36, "w": 880, "h": 4, "d": 8, "material": "Brushed aluminium", "color": "#FF9A9EA4", "repeat": {"kind": "grid", "count": 1, "countZ": 5, "dx": 0, "dz": 80}},
              {"name": "Pedal 1", "kind": "box", "x": 110, "z": 250, "y": 36, "w": 130, "h": 46, "d": 230, "round": 8, "material": "Painted metal", "color": "#FFE9B526"},
              {"name": "Pedal 2", "kind": "box", "x": 280, "z": 250, "y": 36, "w": 130, "h": 46, "d": 230, "round": 8, "material": "Painted metal", "color": "#FF2F7BFF"},
              {"name": "Pedal 3", "kind": "box", "x": 450, "z": 250, "y": 36, "w": 130, "h": 46, "d": 230, "round": 8, "material": "Painted metal", "color": "#FF3FA34D"},
              {"name": "Pedal 4", "kind": "box", "x": 620, "z": 250, "y": 36, "w": 130, "h": 46, "d": 230, "round": 8, "material": "Painted metal", "color": "#FF8A4FD8"},
              {"name": "Pedal 5", "kind": "box", "x": 790, "z": 250, "y": 36, "w": 130, "h": 46, "d": 230, "round": 8, "material": "Painted metal", "color": "#FFD8322A"},
              {"name": "Footswitches", "kind": "cylinder", "x": 110, "z": 320, "y": 80, "w": 36, "h": 10, "d": 36, "round": 3, "material": "Chrome", "color": "#FFDADDE2", "repeat": {"kind": "row", "count": 5, "dx": 170}},
              {"name": "Cables", "kind": "sweep", "plane": "top", "x": 450, "z": 110, "y": 40, "w": 760, "h": 8, "d": 40, "thickness": 6, "section": "round", "path": "M0 0.5 C0.06 0 0.16 0 0.22 0.5 C0.28 1 0.38 1 0.44 0.5 C0.5 0 0.6 0 0.66 0.5 C0.72 1 0.82 1 0.88 0.5 L1 0.5", "material": "Rubber", "color": "#FF101012"}]})",

        // Speaker cab (4x12)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 760}, "parts": [
              {"name": "Cabinet", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 860, "h": 50, "d": 430, "round": 14, "material": "Matte plastic", "color": "#FF151516", "surface": "tolex", "surfaceDepth": 70},
              {"name": "Grille", "kind": "box", "x": 450, "z": 236, "y": 48, "w": 780, "h": 3, "d": 354, "round": 8, "material": "Satin plastic", "color": "#FF2A2A2C", "surface": "grille", "surfaceScale": 0.7},
              {"name": "Piping", "kind": "sweep", "plane": "top", "x": 450, "z": 236, "y": 48, "w": 792, "h": 5, "d": 366, "thickness": 4, "path": "M0.02 0.02 L0.98 0.02 L0.98 0.98 L0.02 0.98 Z", "material": "Gloss plastic", "color": "#FFEDE8DC"},
              {"name": "Logo", "kind": "text", "text": "CAB 412", "bold": true, "x": 450, "z": 100, "y": 51, "w": 200, "h": 2, "d": 40, "material": "Chrome", "color": "#FFDDE0E4"},
              {"name": "Corners", "kind": "box", "x": 24, "z": 24, "y": 0, "w": 50, "h": 56, "d": 50, "round": 12, "material": "Chrome", "color": "#FFD6D9DE", "mirror": "x", "repeat": {"kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 412}}]})",

        // Amp stack (head on a cab)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 760}, "parts": [
              {"name": "Head", "kind": "box", "x": 450, "z": 88, "y": 0, "w": 820, "h": 56, "d": 150, "round": 12, "material": "Matte plastic", "color": "#FF151516", "surface": "tolex", "surfaceDepth": 70},
              {"name": "Head panel", "kind": "box", "x": 450, "z": 96, "y": 54, "w": 760, "h": 3, "d": 96, "round": 5, "material": "Brushed aluminium", "color": "#FFD0B068"},
              {"name": "Cab", "kind": "box", "x": 450, "z": 320, "y": 0, "w": 860, "h": 50, "d": 280, "round": 12, "material": "Matte plastic", "color": "#FF151516", "surface": "tolex", "surfaceDepth": 70},
              {"name": "Grille", "kind": "box", "x": 450, "z": 322, "y": 48, "w": 780, "h": 3, "d": 226, "round": 8, "material": "Satin plastic", "color": "#FF5C4D38", "surface": "grille", "surfaceScale": 0.6},
              {"name": "Logo", "kind": "text", "text": "STACK", "bold": true, "x": 450, "z": 238, "y": 48, "w": 140, "h": 2, "d": 22, "material": "Gold", "color": "#FFE6CB7A"},
              {"name": "Gap", "kind": "box", "op": "cut", "x": 450, "z": 168, "y": -1, "w": 900, "h": 80, "d": 4}]})",

        // Practice amp
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 330}, "parts": [
              {"name": "Cabinet", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 660, "h": 60, "d": 430, "round": 22, "material": "Matte plastic", "color": "#FF19191B", "surface": "tolex", "surfaceDepth": 60},
              {"name": "Control strip", "kind": "box", "x": 450, "z": 82, "y": 58, "w": 600, "h": 3, "d": 90, "round": 6, "material": "Brushed aluminium", "color": "#FF2A2C30"},
              {"name": "Grille", "kind": "cylinder", "x": 450, "z": 280, "y": 58, "w": 280, "h": 3, "d": 280, "round": 1.5, "material": "Steel", "color": "#FF3A3D42", "surface": "perforated"},
              {"name": "Grille ring", "kind": "sweep", "plane": "top", "x": 450, "z": 280, "y": 58, "w": 296, "h": 6, "d": 296, "thickness": 6, "section": "round", "path": "M0.5 0.03 A0.47 0.47 0 1 1 0.4999 0.03 Z", "material": "Chrome", "color": "#FFD6D9DE"},
              {"name": "Badge", "kind": "text", "text": "PRACTICE", "bold": true, "x": 450, "z": 420, "y": 60, "w": 150, "h": 2, "d": 20, "material": "Chrome", "color": "#FFDDE0E4"}]})",

        // Rack preamp (tube window)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 483}, "parts": [
              {"name": "Front", "kind": "box", "x": 450, "z": 230, "y": 0, "w": 880, "h": 10, "d": 200, "chamfer": 3, "material": "Anodized", "color": "#FF1D1E21"},
              {"name": "Ear slots", "kind": "box", "op": "cut", "x": 24, "z": 180, "y": -1, "w": 16, "h": 14, "d": 10, "round": 4.9, "mirror": "x", "repeat": {"kind": "grid", "count": 1, "countZ": 2, "dx": 0, "dz": 100}},
              {"name": "Window well", "kind": "box", "op": "cut", "x": 230, "z": 230, "y": 0, "w": 200, "h": 14, "d": 80, "round": 6},
              {"name": "Tubes", "kind": "cylinder", "axis": "z", "x": 180, "z": 230, "y": -4, "w": 36, "h": 36, "d": 70, "round": 14, "glass": 60, "color": "#FFE8D8C0", "glow": "#FFFF8A30", "glowStrength": 60, "repeat": {"kind": "row", "count": 3, "dx": 50}},
              {"name": "Window", "kind": "box", "x": 230, "z": 230, "y": 4, "w": 196, "h": 4, "d": 76, "round": 5, "glass": 85, "color": "#FFDDE8EE"},
              {"name": "Handles", "kind": "box", "x": 66, "z": 230, "y": 8, "w": 14, "h": 26, "d": 150, "round": 6, "material": "Chrome", "color": "#FFD6D9DE", "mirror": "x"}]})",

        // MIDI foot controller (10 switches)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 640}, "parts": [
              {"name": "Floorboard", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 880, "h": 60, "d": 420, "round": 8, "points": [[0, 0, 0.3], [0.3, 0, 0.2], [1, 0.4, 0.3], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FF1E1F23"},
              {"name": "Display well", "kind": "box", "op": "cut", "x": 450, "z": 70, "y": 50, "w": 300, "h": 20, "d": 60, "round": 4},
              {"name": "Display", "kind": "box", "x": 450, "z": 70, "y": 48, "w": 292, "h": 4, "d": 52, "round": 3, "glass": 50, "color": "#FF050505", "glow": "#FF801810", "glowStrength": 25},
              {"name": "Upper switches", "kind": "cylinder", "x": 130, "z": 250, "y": 46, "w": 36, "h": 10, "d": 36, "round": 3, "material": "Chrome", "color": "#FFD8DBE0", "repeat": {"kind": "row", "count": 5, "dx": 160}},
              {"name": "Upper switch lights", "kind": "sphere", "x": 130, "z": 215.8, "y": 53, "w": 9, "h": 6, "d": 9, "glass": 40, "color": "#FF30FF60", "glow": "#FF30FF60", "glowStrength": 70, "repeat": {"kind": "row", "count": 5, "dx": 160}},
              {"name": "Lower switches", "kind": "cylinder", "x": 130, "z": 380, "y": 38, "w": 36, "h": 10, "d": 36, "round": 3, "material": "Chrome", "color": "#FFD8DBE0", "repeat": {"kind": "row", "count": 5, "dx": 160}},
              {"name": "Lower switch lights", "kind": "sphere", "x": 130, "z": 345.8, "y": 38, "w": 9, "h": 6, "d": 9, "glass": 40, "color": "#FFFF3020", "glow": "#FFFF3020", "glowStrength": 70, "repeat": {"kind": "row", "count": 5, "dx": 160}}]})",

        // Multi-FX floorboard (big screen)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 680}, "parts": [
              {"name": "Floorboard", "kind": "extrude", "plane": "side", "x": 450, "z": 230, "y": 0, "w": 880, "h": 70, "d": 430, "round": 10, "points": [[0, 0, 0.3], [0.38, 0, 0.2], [1, 0.42, 0.3], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FF17181B"},
              {"name": "Screen bezel", "kind": "box", "x": 450, "z": 88, "y": 68, "w": 470, "h": 3, "d": 128, "round": 8, "material": "Gloss plastic", "color": "#FF0A0A0C"},
              {"name": "Screen", "kind": "box", "x": 450, "z": 88, "y": 69, "w": 450, "h": 3, "d": 110, "round": 5, "glass": 40, "color": "#FF0A1220", "glow": "#FF14243C", "glowStrength": 30},
              {"name": "Chain line", "kind": "box", "x": 450.0, "z": 72, "y": 72, "w": 370, "h": 0.8, "d": 2, "material": "Gloss plastic", "color": "#FFD8DEE8", "glow": "#FFB8C8E0", "glowStrength": 30},
              {"name": "Chain block 1", "kind": "box", "x": 282, "z": 72, "y": 72, "w": 34, "h": 1.2, "d": 28, "round": 5.04, "material": "Gloss plastic", "color": "#FFE0622A", "glow": "#FFE0622A", "glowStrength": 45},
              {"name": "Chain block 2", "kind": "box", "x": 330, "z": 72, "y": 72, "w": 34, "h": 1.2, "d": 28, "round": 5.04, "material": "Gloss plastic", "color": "#FF2F7BFF", "glow": "#FF2F7BFF", "glowStrength": 45},
              {"name": "Chain block 3", "kind": "box", "x": 378, "z": 72, "y": 72, "w": 34, "h": 1.2, "d": 28, "round": 5.04, "material": "Gloss plastic", "color": "#FFE8B52A", "glow": "#FFE8B52A", "glowStrength": 45},
              {"name": "Chain block 4", "kind": "box", "x": 426, "z": 72, "y": 72, "w": 34, "h": 1.2, "d": 28, "round": 5.04, "material": "Gloss plastic", "color": "#FF22A99B", "glow": "#FF22A99B", "glowStrength": 45},
              {"name": "Chain block 5", "kind": "box", "x": 474, "z": 72, "y": 72, "w": 34, "h": 1.2, "d": 28, "round": 5.04, "material": "Gloss plastic", "color": "#FF8A4FD8", "glow": "#FF8A4FD8", "glowStrength": 45},
              {"name": "Chain block 6", "kind": "box", "x": 522, "z": 72, "y": 72, "w": 34, "h": 1.2, "d": 28, "round": 5.04, "material": "Gloss plastic", "color": "#FF3FA34D", "glow": "#FF3FA34D", "glowStrength": 45},
              {"name": "Chain block 7", "kind": "box", "x": 570, "z": 72, "y": 72, "w": 34, "h": 1.2, "d": 28, "round": 5.04, "material": "Gloss plastic", "color": "#FF4F5FD8", "glow": "#FF4F5FD8", "glowStrength": 45},
              {"name": "Chain block 8", "kind": "box", "x": 618, "z": 72, "y": 72, "w": 34, "h": 1.2, "d": 28, "round": 5.04, "material": "Gloss plastic", "color": "#FF7A5236", "glow": "#FF7A5236", "glowStrength": 45},
              {"name": "Chain B line", "kind": "box", "x": 450.0, "z": 112, "y": 72, "w": 226, "h": 0.8, "d": 2, "material": "Gloss plastic", "color": "#FFD8DEE8", "glow": "#FFB8C8E0", "glowStrength": 30},
              {"name": "Chain B block 1", "kind": "box", "x": 354, "z": 112, "y": 72, "w": 34, "h": 1.2, "d": 20, "round": 3.5999999999999996, "material": "Gloss plastic", "color": "#FF22A99B", "glow": "#FF22A99B", "glowStrength": 45},
              {"name": "Chain B block 2", "kind": "box", "x": 402, "z": 112, "y": 72, "w": 34, "h": 1.2, "d": 20, "round": 3.5999999999999996, "material": "Gloss plastic", "color": "#FF8A4FD8", "glow": "#FF8A4FD8", "glowStrength": 45},
              {"name": "Chain B block 3", "kind": "box", "x": 450, "z": 112, "y": 72, "w": 34, "h": 1.2, "d": 20, "round": 3.5999999999999996, "material": "Gloss plastic", "color": "#FF3FA34D", "glow": "#FF3FA34D", "glowStrength": 45},
              {"name": "Chain B block 4", "kind": "box", "x": 498, "z": 112, "y": 72, "w": 34, "h": 1.2, "d": 20, "round": 3.5999999999999996, "material": "Gloss plastic", "color": "#FF4F5FD8", "glow": "#FF4F5FD8", "glowStrength": 45},
              {"name": "Chain B block 5", "kind": "box", "x": 546, "z": 112, "y": 72, "w": 34, "h": 1.2, "d": 20, "round": 3.5999999999999996, "material": "Gloss plastic", "color": "#FF7A5236", "glow": "#FF7A5236", "glowStrength": 45},
              {"name": "Upper switches", "kind": "cylinder", "x": 110, "z": 290, "y": 52, "w": 36, "h": 10, "d": 36, "round": 3, "material": "Chrome", "color": "#FFD8DBE0", "repeat": {"kind": "row", "count": 6, "dx": 136}},
              {"name": "Upper switch lights", "kind": "sphere", "x": 110, "z": 255.8, "y": 60, "w": 9, "h": 6, "d": 9, "glass": 40, "color": "#FF4FA8FF", "glow": "#FF4FA8FF", "glowStrength": 70, "repeat": {"kind": "row", "count": 6, "dx": 136}},
              {"name": "Lower switches", "kind": "cylinder", "x": 110, "z": 400, "y": 44, "w": 36, "h": 10, "d": 36, "round": 3, "material": "Chrome", "color": "#FFD8DBE0", "repeat": {"kind": "row", "count": 6, "dx": 136}},
              {"name": "Lower switch lights", "kind": "sphere", "x": 110, "z": 365.8, "y": 44, "w": 9, "h": 6, "d": 9, "glass": 40, "color": "#FFFFFFFF", "glow": "#FFFFFFFF", "glowStrength": 70, "repeat": {"kind": "row", "count": 6, "dx": 136}},
              {"name": "Knobs strip", "kind": "box", "x": 450, "z": 168, "y": 69, "w": 760, "h": 3, "d": 26, "round": 4, "material": "Brushed aluminium", "color": "#FF4A4E56"}]})",

        // Multi-FX compact (four switches)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 340}, "parts": [
              {"name": "Case", "kind": "box", "x": 450, "z": 232, "y": 0, "w": 760, "h": 56, "d": 420, "round": 18, "material": "Painted metal", "color": "#FF202226"},
              {"name": "Screen bezel", "kind": "box", "x": 450, "z": 120, "y": 54, "w": 420, "h": 3, "d": 150, "round": 10, "material": "Gloss plastic", "color": "#FF0A0A0C"},
              {"name": "Screen", "kind": "box", "x": 450, "z": 120, "y": 55, "w": 400, "h": 3, "d": 130, "round": 6, "glass": 40, "color": "#FF0A0E18", "glow": "#FF1A2840", "glowStrength": 30},
              {"name": "Chain line", "kind": "box", "x": 450.0, "z": 120, "y": 58, "w": 358, "h": 0.8, "d": 2, "material": "Gloss plastic", "color": "#FFD8DEE8", "glow": "#FFB8C8E0", "glowStrength": 30},
              {"name": "Chain block 1", "kind": "box", "x": 298, "z": 120, "y": 58, "w": 54, "h": 1.2, "d": 46, "round": 8.28, "material": "Gloss plastic", "color": "#FFE0622A", "glow": "#FFE0622A", "glowStrength": 45},
              {"name": "Chain block 2", "kind": "box", "x": 374, "z": 120, "y": 58, "w": 54, "h": 1.2, "d": 46, "round": 8.28, "material": "Gloss plastic", "color": "#FF2F7BFF", "glow": "#FF2F7BFF", "glowStrength": 45},
              {"name": "Chain block 3", "kind": "box", "x": 450, "z": 120, "y": 58, "w": 54, "h": 1.2, "d": 46, "round": 8.28, "material": "Gloss plastic", "color": "#FFE8B52A", "glow": "#FFE8B52A", "glowStrength": 45},
              {"name": "Chain block 4", "kind": "box", "x": 526, "z": 120, "y": 58, "w": 54, "h": 1.2, "d": 46, "round": 8.28, "material": "Gloss plastic", "color": "#FF22A99B", "glow": "#FF22A99B", "glowStrength": 45},
              {"name": "Chain block 5", "kind": "box", "x": 602, "z": 120, "y": 58, "w": 54, "h": 1.2, "d": 46, "round": 8.28, "material": "Gloss plastic", "color": "#FF8A4FD8", "glow": "#FF8A4FD8", "glowStrength": 45},
              {"name": "Footswitches", "kind": "cylinder", "x": 185, "z": 350, "y": 54, "w": 42, "h": 10, "d": 42, "round": 3, "material": "Chrome", "color": "#FFD8DBE0", "repeat": {"kind": "row", "count": 4, "dx": 176}},
              {"name": "Footswitch lights", "kind": "sphere", "x": 185, "z": 310.1, "y": 54, "w": 9, "h": 6, "d": 9, "glass": 40, "color": "#FF2FB3FF", "glow": "#FF2FB3FF", "glowStrength": 70, "repeat": {"kind": "row", "count": 4, "dx": 176}}]})",

        // Multi-FX desktop modeler (touch screen)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 520}, "parts": [
              {"name": "Case", "kind": "extrude", "plane": "side", "x": 450, "z": 232, "y": 0, "w": 860, "h": 110, "d": 420, "round": 10, "points": [[0, 0, 0.3], [0.62, 0.0, 0.2], [0.64, 0.55, 0.2], [1, 0.6, 0.3], [1, 1], [0, 1]], "material": "Painted metal", "color": "#FF2B2E33"},
              {"name": "Touch screen", "kind": "extrude", "plane": "side", "x": 450, "z": 232, "y": 0, "w": 700, "h": 110, "d": 420, "points": [[0.04, 0.0], [0.58, 0.0], [0.58, 0.035], [0.04, 0.035]], "material": "Gloss plastic", "color": "#FF0A1220", "glass": 30, "glow": "#FF14243C", "glowStrength": 35},
              {"name": "Chain line", "kind": "box", "x": 450.0, "z": 60, "y": 110, "w": 458, "h": 0.8, "d": 2, "material": "Gloss plastic", "color": "#FFD8DEE8", "glow": "#FFB8C8E0", "glowStrength": 30},
              {"name": "Chain block 1", "kind": "box", "x": 250, "z": 60, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FFE0622A", "glow": "#FFE0622A", "glowStrength": 45},
              {"name": "Chain block 2", "kind": "box", "x": 330, "z": 60, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF2F7BFF", "glow": "#FF2F7BFF", "glowStrength": 45},
              {"name": "Chain block 3", "kind": "box", "x": 410, "z": 60, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FFE8B52A", "glow": "#FFE8B52A", "glowStrength": 45},
              {"name": "Chain block 4", "kind": "box", "x": 490, "z": 60, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF22A99B", "glow": "#FF22A99B", "glowStrength": 45},
              {"name": "Chain block 5", "kind": "box", "x": 570, "z": 60, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF8A4FD8", "glow": "#FF8A4FD8", "glowStrength": 45},
              {"name": "Chain block 6", "kind": "box", "x": 650, "z": 60, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF3FA34D", "glow": "#FF3FA34D", "glowStrength": 45},
              {"name": "Chain B line", "kind": "box", "x": 450.0, "z": 128, "y": 110, "w": 378, "h": 0.8, "d": 2, "material": "Gloss plastic", "color": "#FFD8DEE8", "glow": "#FFB8C8E0", "glowStrength": 30},
              {"name": "Chain B block 1", "kind": "box", "x": 290, "z": 128, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FFE8B52A", "glow": "#FFE8B52A", "glowStrength": 45},
              {"name": "Chain B block 2", "kind": "box", "x": 370, "z": 128, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF22A99B", "glow": "#FF22A99B", "glowStrength": 45},
              {"name": "Chain B block 3", "kind": "box", "x": 450, "z": 128, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF8A4FD8", "glow": "#FF8A4FD8", "glowStrength": 45},
              {"name": "Chain B block 4", "kind": "box", "x": 530, "z": 128, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF3FA34D", "glow": "#FF3FA34D", "glowStrength": 45},
              {"name": "Chain B block 5", "kind": "box", "x": 610, "z": 128, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF4F5FD8", "glow": "#FF4F5FD8", "glowStrength": 45},
              {"name": "Chain C line", "kind": "box", "x": 450.0, "z": 196, "y": 110, "w": 298, "h": 0.8, "d": 2, "material": "Gloss plastic", "color": "#FFD8DEE8", "glow": "#FFB8C8E0", "glowStrength": 30},
              {"name": "Chain C block 1", "kind": "box", "x": 330, "z": 196, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF3FA34D", "glow": "#FF3FA34D", "glowStrength": 45},
              {"name": "Chain C block 2", "kind": "box", "x": 410, "z": 196, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF4F5FD8", "glow": "#FF4F5FD8", "glowStrength": 45},
              {"name": "Chain C block 3", "kind": "box", "x": 490, "z": 196, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF7A5236", "glow": "#FF7A5236", "glowStrength": 45},
              {"name": "Chain C block 4", "kind": "box", "x": 570, "z": 196, "y": 110, "w": 58, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FFE0622A", "glow": "#FFE0622A", "glowStrength": 45},
              {"name": "Knobs strip", "kind": "box", "x": 450, "z": 370, "y": 64, "w": 760, "h": 3, "d": 70, "round": 6, "material": "Brushed aluminium", "color": "#FF8E939B"}]})",

        // Multi-FX with expression pedal
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 520}, "parts": [
              {"name": "Floorboard", "kind": "box", "x": 380, "z": 232, "y": 0, "w": 700, "h": 60, "d": 420, "round": 14, "material": "Painted metal", "color": "#FF1B1D21"},
              {"name": "Screen", "kind": "box", "x": 380, "z": 95, "y": 58, "w": 420, "h": 3, "d": 110, "round": 6, "glass": 40, "color": "#FF0C1A10", "glow": "#FF1E3A26", "glowStrength": 30},
              {"name": "Chain line", "kind": "box", "x": 380.0, "z": 95, "y": 61, "w": 364, "h": 0.8, "d": 2, "material": "Gloss plastic", "color": "#FFD8DEE8", "glow": "#FFB8C8E0", "glowStrength": 30},
              {"name": "Chain block 1", "kind": "box", "x": 220, "z": 95, "y": 61, "w": 44, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF2F7BFF", "glow": "#FF2F7BFF", "glowStrength": 45},
              {"name": "Chain block 2", "kind": "box", "x": 284, "z": 95, "y": 61, "w": 44, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FFE8B52A", "glow": "#FFE8B52A", "glowStrength": 45},
              {"name": "Chain block 3", "kind": "box", "x": 348, "z": 95, "y": 61, "w": 44, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF22A99B", "glow": "#FF22A99B", "glowStrength": 45},
              {"name": "Chain block 4", "kind": "box", "x": 412, "z": 95, "y": 61, "w": 44, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF8A4FD8", "glow": "#FF8A4FD8", "glowStrength": 45},
              {"name": "Chain block 5", "kind": "box", "x": 476, "z": 95, "y": 61, "w": 44, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF3FA34D", "glow": "#FF3FA34D", "glowStrength": 45},
              {"name": "Chain block 6", "kind": "box", "x": 540, "z": 95, "y": 61, "w": 44, "h": 1.2, "d": 34, "round": 6.12, "material": "Gloss plastic", "color": "#FF4F5FD8", "glow": "#FF4F5FD8", "glowStrength": 45},
              {"name": "Footswitches", "kind": "cylinder", "x": 130, "z": 300, "y": 58, "w": 40, "h": 10, "d": 40, "round": 3, "material": "Chrome", "color": "#FFD8DBE0", "repeat": {"kind": "row", "count": 3, "dx": 125}},
              {"name": "Footswitch lights", "kind": "sphere", "x": 130, "z": 262.0, "y": 58, "w": 9, "h": 6, "d": 9, "glass": 40, "color": "#FFFF8A2A", "glow": "#FFFF8A2A", "glowStrength": 70, "repeat": {"kind": "row", "count": 3, "dx": 125}},
              {"name": "Lower switches", "kind": "cylinder", "x": 130, "z": 390, "y": 58, "w": 40, "h": 10, "d": 40, "round": 3, "material": "Chrome", "color": "#FFD8DBE0", "repeat": {"kind": "row", "count": 3, "dx": 125}},
              {"name": "Lower switch lights", "kind": "sphere", "x": 130, "z": 352.0, "y": 58, "w": 9, "h": 6, "d": 9, "glass": 40, "color": "#FF30FF60", "glow": "#FF30FF60", "glowStrength": 70, "repeat": {"kind": "row", "count": 3, "dx": 125}},
              {"name": "Pedal base", "kind": "box", "x": 800, "z": 232, "y": 0, "w": 170, "h": 60, "d": 420, "round": 14, "material": "Painted metal", "color": "#FF1B1D21"},
              {"name": "Treadle", "kind": "box", "x": 800, "z": 232, "y": 60, "w": 140, "h": 14, "d": 380, "round": 10, "rotate": [-7, 0, 0], "material": "Painted metal", "color": "#FF26282C"},
              {"name": "Grip", "kind": "box", "x": 800, "z": 240, "y": 72, "w": 118, "h": 3, "d": 300, "round": 6, "rotate": [-7, 0, 0], "material": "Rubber", "color": "#FF141416", "surface": "rubber"}]})",

        // Multi-FX mini (pedal-size modeler)
        R"({"tilt": 0, "shadow": 65, "softness": 55, "floor": "#00000000", "editor": {"units": "mm", "mmWidth": 160}, "parts": [
              {"name": "Enclosure", "kind": "box", "x": 450, "z": 232, "y": 0, "w": 520, "h": 66, "d": 430, "round": 16, "material": "Anodized", "color": "#FF2C3036"},
              {"name": "Screen", "kind": "box", "x": 450, "z": 110, "y": 64, "w": 360, "h": 3, "d": 110, "round": 6, "glass": 40, "color": "#FF0A1220", "glow": "#FF14243C", "glowStrength": 30},
              {"name": "Chain line", "kind": "box", "x": 450.0, "z": 110, "y": 67, "w": 324, "h": 0.8, "d": 2, "material": "Gloss plastic", "color": "#FFD8DEE8", "glow": "#FFB8C8E0", "glowStrength": 30},
              {"name": "Chain block 1", "kind": "box", "x": 330, "z": 110, "y": 67, "w": 84, "h": 1.2, "d": 56, "round": 10.08, "material": "Gloss plastic", "color": "#FFE0622A", "glow": "#FFE0622A", "glowStrength": 45},
              {"name": "Chain block 2", "kind": "box", "x": 450, "z": 110, "y": 67, "w": 84, "h": 1.2, "d": 56, "round": 10.08, "material": "Gloss plastic", "color": "#FF22A99B", "glow": "#FF22A99B", "glowStrength": 45},
              {"name": "Chain block 3", "kind": "box", "x": 570, "z": 110, "y": 67, "w": 84, "h": 1.2, "d": 56, "round": 10.08, "material": "Gloss plastic", "color": "#FF4F5FD8", "glow": "#FF4F5FD8", "glowStrength": 45},
              {"name": "Footswitches", "kind": "cylinder", "x": 290, "z": 350, "y": 64, "w": 46, "h": 10, "d": 46, "round": 3, "material": "Chrome", "color": "#FFD8DBE0", "repeat": {"kind": "row", "count": 3, "dx": 160}},
              {"name": "Footswitch lights", "kind": "sphere", "x": 290, "z": 306.3, "y": 64, "w": 9, "h": 6, "d": 9, "glass": 40, "color": "#FF4FA8FF", "glow": "#FF4FA8FF", "glowStrength": 70, "repeat": {"kind": "row", "count": 3, "dx": 160}}]})",
    };

    constexpr int jsonTemplates = (int) (sizeof (templateJson) / sizeof (templateJson[0]));

    // ...and the racks, made for the window: the 19" front plate (482.6 mm, 44.45 mm a unit)
    // across it, its ears' slots.
    juce::var rackTemplate (int units, int W, int H)
    {
        const float plateW = (float) W * 0.98f;
        const float plateD = juce::jmin ((float) H * 0.96f, plateW * (float) units * 44.45f / 482.6f);
        const float u = juce::jmin ((float) W, (float) H);
        const float left = ((float) W - plateW) * 0.5f, top = ((float) H - plateD) * 0.5f;
        const float thick = juce::jmax (3.0f, u * 0.02f);

        auto body = makeObject();
        setProperty (body, "tilt", 0);
        setProperty (body, "shadow", 55);
        setProperty (body, "floor", "#00000000");

        auto editor = makeObject();
        setProperty (editor, "units", "mm");
        setProperty (editor, "mmWidth", 482.6);
        setProperty (body, "editor", editor);

        juce::Array<juce::var> list;

        auto part = [] (std::initializer_list<std::pair<const char*, juce::var>> values)
        {
            auto p = makeObject();

            for (const auto& [key, value] : values)
                setProperty (p, key, value);

            return p;
        };

        list.add (part ({ { "name", "Front plate" }, { "kind", "box" }, { "x", W * 0.5 }, { "z", H * 0.5 }, { "y", 0 }, { "w", plateW },
                          { "h", thick }, { "d", plateD }, { "chamfer", thick * 0.3 }, { "material", "Anodized" }, { "color", "#FF1D1E21" } }));

        // A slot in each ear, every unit.
        const float slotW = plateW * 0.022f, slotD = juce::jmin (plateD / (float) units * 0.22f, plateW * 0.015f);
        auto repeat = makeObject();
        setProperty (repeat, "kind", "grid");
        setProperty (repeat, "count", 1);
        setProperty (repeat, "countZ", units);
        setProperty (repeat, "dx", 0);
        setProperty (repeat, "dz", plateD / (float) units);

        list.add (part ({ { "name", "Ear slots" }, { "kind", "box" }, { "op", "cut" }, { "x", left + plateW * 0.022 }, { "z", top + plateD / (float) units * 0.5f },
                          { "y", -1 }, { "w", slotW }, { "h", thick + 4 }, { "d", slotD }, { "round", slotD * 0.49 }, { "mirror", "x" }, { "repeat", repeat } }));

        // Handles on the taller ones.
        if (units >= 2)
            list.add (part ({ { "name", "Handle" }, { "kind", "box" }, { "x", left + plateW * 0.055 }, { "z", H * 0.5 }, { "y", thick - 1 },
                              { "w", juce::jmax (8.0f, plateW * 0.016f) }, { "h", u * 0.05 }, { "d", plateD * 0.72 }, { "round", juce::jmax (3.0f, plateW * 0.007f) },
                              { "material", "Chrome" }, { "color", "#FFD6D9DE" }, { "mirror", "x" } }));

        setProperty (body, "parts", list);
        return body;
    }

    //==========================================================================
    juce::String kindCaption (const juce::var& part)
    {
        const auto kind = part.getProperty ("kind", "box").toString();
        return kind.substring (0, 1).toUpperCase() + kind.substring (1);
    }

    struct BodyWindow final : public juce::DocumentWindow
    {
        BodyWindow (const juce::String& title, juce::Component* content, std::function<void()> closed)
            : juce::DocumentWindow (title, Theme::panel, juce::DocumentWindow::closeButton), onClosed (std::move (closed))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            setResizable (true, false);
        }

        void closeButtonPressed() override
        {
            setVisible (false);

            if (onClosed != nullptr)
                onClosed();
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey)
            {
                closeButtonPressed();
                return true;
            }

            return juce::DocumentWindow::keyPressed (key);
        }

        std::function<void()> onClosed;
    };
}

//==============================================================================
/** The parts, the body's own settings first. */
class BodyEditor::PartsList final : public juce::ListBox,
                                    private juce::ListBoxModel
{
public:
    explicit PartsList (BodyEditor& e) : editor (e)
    {
        setModel (this);
        setRowHeight (26);
        setColour (juce::ListBox::backgroundColourId, Theme::inset);
        setColour (juce::ListBox::outlineColourId, Theme::outline);
        setOutlineThickness (1);
    }

    int getNumRows() override
    {
        const auto* list = editor.parts();
        return 1 + (list != nullptr ? list->size() : 0);
    }

    // Front first, like layers: the top row is the last part (in front where surfaces meet;
    // a cut takes away from the rows under it). Row 0: the body itself.
    int partForRow (int row) const
    {
        const auto* list = editor.parts();
        return row <= 0 || list == nullptr ? -1 : list->size() - row;
    }

    static int rowForPart (int index, int count)    { return index < 0 ? 0 : count - index; }

    // Each part's row: its eye (shown or hidden) and its x (taken away), at its right.
    static juce::Rectangle<int> eyeArea (int width, int height)      { return { width - 52, 0, 24, height }; }
    static juce::Rectangle<int> removeArea (int width, int height)   { return { width - 26, 0, 24, height }; }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool isChosen) override
    {
        if (isChosen)
        {
            g.setColour (Theme::accentDeep);
            g.fillRect (0, 0, width, height);
        }

        auto area = juce::Rectangle<int> (0, 0, width, height).reduced (8, 0);

        if (row == 0)
        {
            g.setColour (Theme::text);
            g.setFont (Theme::font (13.0f, true));
            g.drawText ("The body: view and light", area, juce::Justification::centredLeft, true);
            return;
        }

        const auto* list = editor.parts();
        const auto index = partForRow (row);

        if (list == nullptr || ! juce::isPositiveAndBelow (index, list->size()))
            return;

        const auto& part = (*list)[index];
        const bool cut = part.getProperty ("op", "add").toString().equalsIgnoreCase ("cut");
        const bool hidden = (bool) part.getProperty ("hidden", false);

        // Its eye and its x.
        {
            const auto eye = eyeArea (width, height).toFloat().withSizeKeepingCentre (16.0f, 10.0f);
            juce::Path lid;
            lid.startNewSubPath (eye.getX(), eye.getCentreY());
            lid.quadraticTo (eye.getCentreX(), eye.getY() - 3.0f, eye.getRight(), eye.getCentreY());
            lid.quadraticTo (eye.getCentreX(), eye.getBottom() + 3.0f, eye.getX(), eye.getCentreY());
            g.setColour (hidden ? Theme::muted.withAlpha (0.6f) : Theme::text.withAlpha (0.85f));
            g.strokePath (lid, juce::PathStrokeType (1.3f));

            if (hidden)
                g.drawLine (eye.getX(), eye.getBottom() + 2.0f, eye.getRight(), eye.getY() - 2.0f, 1.5f);
            else
                g.fillEllipse (eye.withSizeKeepingCentre (5.0f, 5.0f));

            const auto x = removeArea (width, height).toFloat().withSizeKeepingCentre (9.0f, 9.0f);
            g.setColour (Theme::muted);
            g.drawLine (x.getX(), x.getY(), x.getRight(), x.getBottom(), 1.5f);
            g.drawLine (x.getRight(), x.getY(), x.getX(), x.getBottom(), 1.5f);
            area.setRight (width - 56);
        }

        // Its colour, a cut as a hole.
        const auto swatch = area.removeFromLeft (14).withSizeKeepingCentre (12, 12).toFloat();

        if (cut)
        {
            g.setColour (Theme::muted);
            g.drawEllipse (swatch.reduced (0.5f), 1.2f);
        }
        else
        {
            g.setColour (colourOf (part.getProperty ("color", part.getProperty ("colour", {})), juce::Colour (0xff202024)).withAlpha (1.0f));
            g.fillRoundedRectangle (swatch, 2.5f);
            g.setColour (Theme::outline.brighter (0.3f));
            g.drawRoundedRectangle (swatch, 2.5f, 1.0f);
        }

        area.removeFromLeft (8);
        const auto name = part.getProperty ("name", {}).toString();
        auto tag = kindCaption (part) + (cut ? " cut" : "");

        if (part.getProperty ("repeat", {}).isObject())
            tag << juce::String::fromUTF8 (" \xc3\x97") << (int) numberOf (part.getProperty ("repeat", {}), "count", 2);

        if (part.getProperty ("mirror", {}).toString().containsIgnoreCase ("x"))
            tag << " mirrored";

        g.setFont (Theme::font (11.0f));
        g.setColour (Theme::muted);
        const auto tagWidth = juce::jmin (area.getWidth() / 2, juce::GlyphArrangement::getStringWidthInt (Theme::font (11.0f), tag) + 4);
        g.drawText (tag, area.removeFromRight (tagWidth), juce::Justification::centredRight, true);

        g.setColour ((cut || hidden) ? Theme::muted : Theme::text);
        g.setFont (Theme::font (13.0f));
        g.drawText (name.isNotEmpty() ? name : kindCaption (part), area, juce::Justification::centredLeft, true);
    }

    void selectedRowsChanged (int row) override
    {
        if (! editor.updating && row >= 0)
            editor.choose (partForRow (row));
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        const auto index = partForRow (row);

        if (index < 0)
            return;

        const auto width = getVisibleRowWidth(), height = getRowHeight();

        if (eyeArea (width, height).contains (e.getPosition()))
            editor.toggleHidden (index);
        else if (removeArea (width, height).contains (e.getPosition()))
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<BodyEditor> (&editor), index]
                                             {
                                                 if (safe != nullptr)
                                                 {
                                                     safe->choose (index);
                                                     safe->deletePart();
                                                 }
                                             });
    }

    void deleteKeyPressed (int row) override
    {
        if (row > 0)
            editor.deletePart();
    }

private:
    BodyEditor& editor;
};

//==============================================================================
/** The settings: a caption and its control (or two side by side), a line, in groups. */
class BodyEditor::Rows final : public juce::Component
{
public:
    struct Row
    {
        juce::String caption;
        juce::Component* first = nullptr;
        juce::Component* second = nullptr;
        bool header = false;
        bool shown = true;
        int height = 24;
    };

    std::vector<Row> rows;
    static constexpr int captionWidth = 112;

    void add (const juce::String& caption, juce::Component& first, juce::Component* second = nullptr, int height = 24)
    {
        rows.push_back ({ caption, &first, second, false, true, height });
        addAndMakeVisible (first);

        if (second != nullptr)
            addAndMakeVisible (*second);
    }

    void header (const juce::String& caption)
    {
        rows.push_back ({ caption, nullptr, nullptr, true, true, 26 });
    }

    void show (juce::Component& c, bool shouldShow)
    {
        for (auto& row : rows)
            if (row.first == &c)
                row.shown = shouldShow;
    }

    /** Every row that isn't a header: shown or not. */
    void showAll (bool shouldShow)
    {
        for (auto& row : rows)
            if (! row.header)
                row.shown = shouldShow;
    }

    int layOut (int width)
    {
        int y = 4;

        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto& row = rows[i];

            if (row.header)
            {
                // A group shows when one of its rows does.
                bool any = false;

                for (size_t j = i + 1; j < rows.size() && ! rows[j].header; ++j)
                    any = any || rows[j].shown;

                row.shown = any;

                if (any)
                {
                    row.height = 26;
                    headerTops[i] = y;
                    y += row.height;
                }

                continue;
            }

            for (auto* c : { row.first, row.second })
                if (c != nullptr)
                    c->setVisible (row.shown);

            if (! row.shown)
                continue;

            auto area = juce::Rectangle<int> (0, y, width, row.height).reduced (8, 1);

            if (row.caption.isNotEmpty())
                area.removeFromLeft (captionWidth);

            if (row.second != nullptr)
            {
                row.first->setBounds (area.removeFromLeft (area.getWidth() / 2 - 2));
                area.removeFromLeft (4);
                row.second->setBounds (area);
            }
            else
            {
                row.first->setBounds (area);
            }

            captionTops[i] = y;
            y += row.height + 3;
        }

        return y + 8;
    }

    void paint (juce::Graphics& g) override
    {
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& row = rows[i];

            if (! row.shown)
                continue;

            if (row.header)
            {
                const auto top = headerTops[i];
                g.setColour (Theme::accent);
                g.setFont (Theme::font (12.0f, true));
                g.drawText (row.caption.toUpperCase(), 8, top + 6, getWidth() - 16, 16, juce::Justification::centredLeft, true);
                g.setColour (Theme::outline);
                g.drawHorizontalLine (top + 24, 8.0f, (float) getWidth() - 8.0f);
            }
            else if (row.caption.isNotEmpty())
            {
                g.setColour (Theme::muted);
                g.setFont (Theme::font (12.5f));
                g.drawText (row.caption, 8, captionTops[i], captionWidth - 6, row.height, juce::Justification::centredLeft, true);
            }
        }
    }

private:
    std::map<size_t, int> headerTops, captionTops;
};

//==============================================================================
/**
    The parts seen from the top, the front or the side: the window's outline (from the top),
    the floor (from the front and the side), every part where it lies, the chosen one with its
    corners to size it. Click a part to choose it, drag it to move it; it snaps to the grid.
*/
class BodyEditor::PlanView final : public juce::Component
{
public:
    explicit PlanView (BodyEditor& e) : editor (e) {}

    int view = 0;   // 0 top, 1 front, 2 side

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::inset);
        fit();

        const int W = editor.windowWidth(), H = editor.windowHeight();
        auto toScreen = juce::AffineTransform::translation (-world.getX(), -world.getY()).scaled (scale).translated (origin.x, origin.y);

        // The grid.
        if (const float grid = editor.gridPixels(); grid > 0.0f && grid * scale >= 5.0f)
        {
            g.setColour (Theme::outline.withAlpha (0.45f));

            for (float u = std::floor (world.getX() / grid) * grid; u <= world.getRight(); u += grid)
                g.drawVerticalLine (juce::roundToInt (origin.x + (u - world.getX()) * scale), 0.0f, (float) getHeight());

            for (float v = std::floor (world.getY() / grid) * grid; v <= world.getBottom(); v += grid)
                g.drawHorizontalLine (juce::roundToInt (origin.y + (v - world.getY()) * scale), 0.0f, (float) getWidth());
        }

        // The window from the top, the floor from the front and the side.
        g.setColour (Theme::muted.withAlpha (0.8f));

        if (view == 0)
        {
            g.drawRect (juce::Rectangle<float> (0, 0, (float) W, (float) H).transformedBy (toScreen), 1.0f);
        }
        else
        {
            const auto y = origin.y + (0.0f - world.getY()) * scale;
            g.drawHorizontalLine (juce::roundToInt (y), 0.0f, (float) getWidth());
        }

        const auto* list = editor.parts();

        if (list != nullptr)
        {
            for (int i = 0; i < list->size(); ++i)
            {
                const auto& part = (*list)[i];
                const bool cut = part.getProperty ("op", "add").toString().equalsIgnoreCase ("cut");
                const bool chosenOne = i == editor.chosen;

                if ((bool) part.getProperty ("hidden", false) && ! chosenOne)
                    continue;   // hidden (its eye shut)
                auto outline = outlineOf (part, view, Body3D::mirrorAtOf (editor.body(), W)).createPathWithRoundedCorners (0.0f);
                outline.applyTransform (toScreen);

                const auto colour = colourOf (part.getProperty ("color", part.getProperty ("colour", {})), juce::Colour (0xff505058)).withAlpha (1.0f);

                if (cut)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.35f));
                    g.fillPath (outline);
                    juce::Path dashed;
                    const float dashes[] { 4.0f, 3.0f };
                    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
                    g.setColour (chosenOne ? Theme::accent : juce::Colour (0xffd08060));
                    g.fillPath (dashed);
                }
                else
                {
                    // Its colour, lifted towards grey so a black part shows on the dark view.
                    const auto shown = colour.interpolatedWith (juce::Colour (0xff8a8f99), 0.4f);
                    g.setColour (shown.withAlpha (chosenOne ? 0.7f : 0.45f));
                    g.fillPath (outline);
                    g.setColour (chosenOne ? Theme::accent : shown.brighter (0.3f).withAlpha (0.85f));
                    g.strokePath (outline, juce::PathStrokeType (chosenOne ? 1.8f : 1.0f));
                }
            }

            // The chosen one's corners.
            if (juce::isPositiveAndBelow (editor.chosen, list->size()))
            {
                const auto box = boxOf ((*list)[editor.chosen]).transformedBy (toScreen);
                g.setColour (Theme::accent);

                for (const auto& handle : { box.getTopLeft(), box.getTopRight(), box.getBottomLeft(), box.getBottomRight() })
                    g.fillRect (juce::Rectangle<float> (7.0f, 7.0f).withCentre (handle));
            }
        }

        g.setColour (Theme::muted);
        g.setFont (Theme::font (11.5f));
        const juce::String captions[] { "From the top: the window as the panel shows it",
                                        "From the front: the floor at the bottom",
                                        "From the side: the back on the left, the front on the right" };
        g.drawText (captions[view], getLocalBounds().reduced (8, 4), juce::Justification::bottomLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        fit();
        const auto* list = editor.parts();
        mode = Mode::none;

        if (list == nullptr)
            return;

        const auto at = toWorld (e.position);

        // A corner of the chosen one: size it.
        if (juce::isPositiveAndBelow (editor.chosen, list->size()))
        {
            const auto box = boxOf ((*list)[editor.chosen]);
            const juce::Point<float> corners[] { box.getTopLeft(), box.getTopRight(), box.getBottomLeft(), box.getBottomRight() };

            for (int c = 0; c < 4; ++c)
                if (corners[c].getDistanceFrom (at) * scale < 7.0f)
                {
                    mode = Mode::size;
                    corner = c;
                    startBox = box;
                    return;
                }
        }

        // A part (the last drawn first): choose it, and move it.
        const int W = editor.windowWidth();

        for (int i = list->size(); --i >= 0;)
            if (! (bool) (*list)[i].getProperty ("hidden", false)
                && outlineOf ((*list)[i], view, Body3D::mirrorAtOf (editor.body(), W)).contains (at))
            {
                if (i != editor.chosen)
                    editor.choose (i);

                mode = Mode::move;
                startBox = boxOf ((*list)[i]);
                startAt = at;
                return;
            }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto* list = editor.parts();

        if (mode == Mode::none || list == nullptr || ! juce::isPositiveAndBelow (editor.chosen, list->size()))
            return;

        const auto part = (*list)[editor.chosen];
        const auto at = toWorld (e.position);
        auto box = startBox;

        if (mode == Mode::move)
        {
            const auto shift = at - startAt;
            box.setPosition (snap (startBox.getX() + shift.x), snap (startBox.getY() + shift.y));
        }
        else
        {
            const juce::Point<float> corners[] { startBox.getTopLeft(), startBox.getTopRight(), startBox.getBottomLeft(), startBox.getBottomRight() };
            const auto fixed = corners[3 - corner];
            const juce::Point<float> to (snap (at.x), snap (at.y));
            box = juce::Rectangle<float> (fixed, to);
            box.setSize (juce::jmax (1.0f, box.getWidth()), juce::jmax (1.0f, box.getHeight()));
        }

        apply (part, box);
        moved = true;
        editor.changed (false);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (moved)
            editor.changed (true);

        moved = false;
        mode = Mode::none;
    }

private:
    BodyEditor& editor;
    juce::Rectangle<float> world;   // what's shown, in the view's units
    juce::Point<float> origin;
    float scale = 1.0f;

    enum class Mode { none, move, size };
    Mode mode = Mode::none;
    int corner = 0;
    juce::Rectangle<float> startBox;
    juce::Point<float> startAt;
    bool moved = false;

    float snap (float v) const
    {
        const float grid = editor.gridPixels();
        return grid > 0.0f ? std::round (v / grid) * grid : std::round (v * 2.0f) * 0.5f;
    }

    juce::Point<float> toWorld (juce::Point<float> p) const
    {
        return { world.getX() + (p.x - origin.x) / scale, world.getY() + (p.y - origin.y) / scale };
    }

    /** What it shows: the window (and the parts past it), the body's height. */
    void fit()
    {
        const float W = (float) editor.windowWidth(), H = (float) editor.windowHeight();
        float top = juce::jmin (W, H) * 0.25f;

        if (const auto* list = editor.parts())
            for (const auto& part : *list)
                top = juce::jmax (top, numberOf (part, "y", 0) + numberOf (part, "h", 20));

        world = view == 0 ? juce::Rectangle<float> (0, 0, W, H)
                          : juce::Rectangle<float> (0, -top * 1.15f, view == 1 ? W : H, top * 1.15f + top * 0.25f);
        world = world.expanded (juce::jmax (world.getWidth(), world.getHeight()) * 0.04f);

        const auto area = getLocalBounds().reduced (10).withTrimmedBottom (14).toFloat();
        scale = juce::jmin (area.getWidth() / world.getWidth(), area.getHeight() / world.getHeight());
        origin = { area.getCentreX() - world.getWidth() * scale * 0.5f, area.getCentreY() - world.getHeight() * scale * 0.5f };
    }

    /** Its box in this view, as it is before it's turned: from the top x, z (w, d); from the
        front x, -y (w, h); from the side z, -y (d, h). */
    juce::Rectangle<float> boxOf (const juce::var& part) const
    {
        const float x = numberOf (part, "x", 0), y = numberOf (part, "y", 0), z = numberOf (part, "z", 0);
        const float w = numberOf (part, "w", 100), h = numberOf (part, "h", 20), d = numberOf (part, "d", 100);

        if (view == 0) return { x - w * 0.5f, z - d * 0.5f, w, d };
        if (view == 1) return { x - w * 0.5f, -(y + h), w, h };
        return { z - d * 0.5f, -(y + h), d, h };
    }

    void apply (const juce::var& part, juce::Rectangle<float> box)
    {
        auto put = [&] (const char* key, float v) { setProperty (part, key, std::round (v * 100.0f) / 100.0f); };

        if (view == 0)
        {
            put ("x", box.getCentreX()); put ("w", box.getWidth());
            put ("z", box.getCentreY()); put ("d", box.getHeight());
        }
        else
        {
            put (view == 1 ? "x" : "z", box.getCentreX());
            put (view == 1 ? "w" : "d", box.getWidth());
            put ("y", -box.getBottom());
            put ("h", box.getHeight());
        }
    }

public:
    /** Its outline in a view: its shape where it's upright and a copy is plain (a profile, a
        round), else every copy's box as turned. */
    static juce::Path outlineOf (const juce::var& part, int view, float mirrorAt)
    {
        juce::Path path;
        const auto kind = part.getProperty ("kind", "box").toString().toLowerCase();
        const auto r = rotationOf (part);
        const bool upright = std::abs (r[0]) + std::abs (r[1]) + std::abs (r[2]) < 0.01f;
        const auto repeat = part.getProperty ("repeat", {});
        const bool circle = repeat.isObject() && repeat.getProperty ("kind", "row").toString().equalsIgnoreCase ("circle");

        for (const auto& corners : copiesOf (part, mirrorAt))
        {
            std::vector<juce::Point<float>> points;

            for (const auto& c : corners)
                points.push_back (flat (c, view));

            auto hull = hullOf (points);

            if (upright && ! circle && ! hull.isEmpty())
            {
                const auto box = hull.getBounds();
                const auto plane = part.getProperty ("plane", "side").toString().toLowerCase();
                const auto axis = part.getProperty ("axis", "y").toString().toLowerCase();
                const int planeView = plane == "top" ? 0 : (plane == "front" ? 1 : 2);
                const int roundView = axis == "x" ? 2 : (axis == "z" ? 1 : 0);

                if (kind == "sphere" || (kind == "cylinder" && view == roundView) || (kind == "revolve" && view == 0))
                {
                    hull.clear();
                    hull.addEllipse (box);
                }
                else if (kind == "extrude" && view == planeView)
                {
                    auto profile = juce::Drawable::parseSVGPath (part.getProperty ("profile", "M0 0 L1 0 L1 1 L0 1 Z").toString());

                    if (! profile.isEmpty())
                    {
                        profile.applyTransform (juce::AffineTransform::scale (box.getWidth(), box.getHeight()).translated (box.getX(), box.getY()));
                        hull = profile;
                    }
                }
                else if (kind == "revolve")
                {
                    // Its half outline, and the other half mirrored.
                    auto half = juce::Drawable::parseSVGPath (part.getProperty ("profile", "M0 0 L1 0 L1 1 L0 1 Z").toString());

                    if (! half.isEmpty())
                    {
                        const auto mid = box.getCentreX(), radius = box.getWidth() * 0.5f;
                        auto right = half, left = half;
                        right.applyTransform (juce::AffineTransform::scale (radius, box.getHeight()).translated (mid, box.getY()));
                        left.applyTransform (juce::AffineTransform::scale (-radius, box.getHeight()).translated (mid, box.getY()));
                        hull = right;
                        hull.addPath (left);
                    }
                }
            }

            path.addPath (hull);
        }

        return path;
    }
};

//==============================================================================
/**
    The chosen part's profile: an extrude's outline in its plane, a revolve's half outline
    (its middle on the left). Drag a point; double-click a line for a new one, a point to take
    it away; right-click a point to round it. Presets above it.
*/
class BodyEditor::ProfileEditor final : public juce::Component
{
public:
    explicit ProfileEditor (BodyEditor& e) : editor (e)
    {
        addAndMakeVisible (presetBox);
        presetBox.setTextWhenNothingSelected ("Start from a profile...");
        presetBox.setTooltip ("A profile to start from: its points are dragged from there");
        presetBox.onChange = [this]
        {
            const auto index = presetBox.getSelectedItemIndex();
            const auto& presets = revolving() ? revolvePresets() : extrudePresets();

            if (juce::isPositiveAndBelow (index, (int) presets.size()))
            {
                points = presets[(size_t) index].points;
                write (true);
            }

            presetBox.setSelectedItemIndex (-1, juce::dontSendNotification);
        };
    }

    /** The chosen part changed: its points. */
    void load()
    {
        const auto part = editor.part();
        points.clear();
        active = -1;
        presetBox.clear (juce::dontSendNotification);

        if (editable())
        {
            points = pointsOf (part);
            int id = 1;

            for (const auto& preset : revolving() ? revolvePresets() : extrudePresets())
                presetBox.addItem (preset.name, id++);
        }

        presetBox.setEnabled (editable());
        repaint();
    }

    void resized() override
    {
        presetBox.setBounds (getLocalBounds().reduced (8).removeFromTop (24).removeFromLeft (240));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::inset);

        if (! editable())
        {
            g.setColour (Theme::muted);
            g.setFont (Theme::font (13.0f));
            g.drawFittedText ("Choose an extrude or a revolve part to draw its profile here.\n\n"
                              "An extrude is a profile pulled through its box (seen from the side: a synth's chassis with the step where its keys sit). "
                              "A revolve is a profile turned round its upright middle (a dial, a boot, a foot).",
                              getLocalBounds().reduced (24), juce::Justification::centred, 8);
            return;
        }

        const auto area = plane();

        // Its grid: tenths, and the snap's finer steps.
        g.setColour (Theme::outline.withAlpha (0.5f));

        for (int i = 0; i <= 10; ++i)
        {
            g.drawVerticalLine (juce::roundToInt (area.getX() + area.getWidth() * (float) i / 10.0f), area.getY(), area.getBottom());
            g.drawHorizontalLine (juce::roundToInt (area.getY() + area.getHeight() * (float) i / 10.0f), area.getX(), area.getRight());
        }

        g.setColour (Theme::muted);
        g.drawRect (area, 1.0f);

        // The outline, filled.
        auto outline = juce::Drawable::parseSVGPath (pathFromPoints (points));
        outline.applyTransform (juce::AffineTransform::scale (area.getWidth(), area.getHeight()).translated (area.getX(), area.getY()));
        const auto colour = colourOf (editor.part().getProperty ("color", {}), juce::Colour (0xff6a6f78)).withAlpha (1.0f);
        g.setColour (colour.interpolatedWith (Theme::accent, 0.25f).withAlpha (0.55f));
        g.fillPath (outline);
        g.setColour (Theme::accent);
        g.strokePath (outline, juce::PathStrokeType (1.6f));

        // Its points.
        for (size_t i = 0; i < points.size(); ++i)
        {
            const auto p = toScreen (points[i]);
            const auto dot = juce::Rectangle<float> (9.0f, 9.0f).withCentre (p);
            g.setColour ((int) i == active ? juce::Colours::white : Theme::accent);

            if (points[i].round > 0.0f)
                g.fillEllipse (dot);
            else
                g.fillRect (dot);
        }

        g.setColour (Theme::muted);
        g.setFont (Theme::font (11.5f));
        const auto plane = editor.part().getProperty ("plane", "side").toString().toLowerCase();
        const auto caption = revolving() ? juce::String ("Half its outline: its middle on the left, its edge on the right, its top up")
                           : plane == "front" ? juce::String ("Seen from the front: left to right, its top up")
                           : plane == "top" ? juce::String ("Seen from above: the back at the top, the front at the bottom")
                                            : juce::String ("Seen from the side: the back on the left, the front on the right, its top up");
        g.drawText (caption + (stretched ? "  (stretched to edit)" : ""), getLocalBounds().reduced (8, 4), juce::Justification::bottomLeft, true);
        g.drawText ("Double-click: add or take away a point. Right-click: round a corner.", getLocalBounds().reduced (8, 4).withTrimmedBottom (16),
                    juce::Justification::bottomLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! editable())
            return;

        active = pointAt (e.position);
        dragged = false;

        if (active >= 0 && e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            const auto round = points[(size_t) active].round;
            menu.addItem (1, "Sharp corner", true, round <= 0.0f);
            menu.addItem (2, "Rounded a little", true, round > 0.0f && round < 0.35f);
            menu.addItem (3, "Rounded", true, round >= 0.35f && round < 0.8f);
            menu.addItem (4, "Rounded all the way", true, round >= 0.8f);
            menu.addSeparator();
            menu.addItem (5, "Take this point away", points.size() > 3);

            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                                [safe = juce::Component::SafePointer<ProfileEditor> (this), index = active] (int result)
                                {
                                    if (safe == nullptr || ! juce::isPositiveAndBelow (index, (int) safe->points.size()) || result == 0)
                                        return;

                                    if (result == 5)
                                        safe->points.erase (safe->points.begin() + index);
                                    else
                                        safe->points[(size_t) index].round = result == 1 ? 0.0f : (result == 2 ? 0.25f : (result == 3 ? 0.6f : 1.0f));

                                    safe->active = -1;
                                    safe->write (true);
                                });
        }

        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! editable() || active < 0 || e.mods.isPopupMenu())
            return;

        auto p = toPlane (e.position);
        points[(size_t) active].x = p.x;
        points[(size_t) active].y = p.y;
        dragged = true;
        write (false);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragged)
            write (true);

        dragged = false;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! editable())
            return;

        // On a point: taken away (three stay)...
        if (const auto index = pointAt (e.position); index >= 0)
        {
            if (points.size() > 3)
            {
                points.erase (points.begin() + index);
                active = -1;
                write (true);
            }

            return;
        }

        // ...on a line: a new point there.
        const auto at = e.position;
        float best = 1.0e9f;
        int after = -1;

        for (size_t i = 0; i < points.size(); ++i)
        {
            const juce::Line<float> line (toScreen (points[i]), toScreen (points[(i + 1) % points.size()]));
            juce::Point<float> nearest;
            const auto distance = line.getDistanceFromPoint (at, nearest);

            if (distance < best)
            {
                best = distance;
                after = (int) i;
            }
        }

        if (after >= 0 && best < 12.0f)
        {
            const auto p = toPlane (at);
            points.insert (points.begin() + after + 1, { p.x, p.y, 0.0f });
            active = after + 1;
            write (true);
        }
    }

private:
    BodyEditor& editor;
    juce::ComboBox presetBox;
    std::vector<ProfilePoint> points;
    int active = -1;
    bool dragged = false;
    bool stretched = false;

    bool revolving() const    { return editor.part().getProperty ("kind", {}).toString().equalsIgnoreCase ("revolve"); }

    bool editable() const
    {
        const auto kind = editor.part().getProperty ("kind", {}).toString().toLowerCase();
        return editor.part().isObject() && (kind == "extrude" || kind == "revolve" || kind == "loft"
                                            || (kind == "sweep" && editor.part().getProperty ("section", {}).toString().equalsIgnoreCase ("profile")));
    }

    /** The plane in its proportions (very long or flat ones stretched, to be workable). */
    juce::Rectangle<float> plane()
    {
        const auto part = editor.part();
        const float w = numberOf (part, "w", 100), h = numberOf (part, "h", 20), d = numberOf (part, "d", 100);
        const auto planeName = part.getProperty ("plane", "side").toString().toLowerCase();
        float across = planeName == "side" ? d : w, down = planeName == "top" ? d : h;

        if (revolving())
        {
            across = w * 0.5f;
            down = h;
        }

        const auto area = getLocalBounds().reduced (16).withTrimmedTop (30).withTrimmedBottom (34).toFloat();
        const float ratio = across / juce::jmax (0.5f, down);
        const float shown = juce::jlimit (0.4f, 3.0f, ratio);
        stretched = std::abs (shown - ratio) > 0.01f;

        auto r = area;

        if (area.getWidth() / area.getHeight() > shown)
            r = area.withSizeKeepingCentre (area.getHeight() * shown, area.getHeight());
        else
            r = area.withSizeKeepingCentre (area.getWidth(), area.getWidth() / shown);

        return r;
    }

    juce::Point<float> toScreen (const ProfilePoint& p)
    {
        const auto r = plane();
        return { r.getX() + p.x * r.getWidth(), r.getY() + p.y * r.getHeight() };
    }

    juce::Point<float> toPlane (juce::Point<float> screen)
    {
        const auto r = plane();
        auto x = juce::jlimit (0.0f, 1.0f, (screen.x - r.getX()) / r.getWidth());
        auto y = juce::jlimit (0.0f, 1.0f, (screen.y - r.getY()) / r.getHeight());

        if (editor.snapToggle.getToggleState())
        {
            x = std::round (x * 40.0f) / 40.0f;
            y = std::round (y * 40.0f) / 40.0f;
        }

        return { x, y };
    }

    int pointAt (juce::Point<float> screen)
    {
        for (int i = (int) points.size(); --i >= 0;)
            if (toScreen (points[(size_t) i]).getDistanceFrom (screen) < 8.0f)
                return i;

        return -1;
    }

    void write (bool final)
    {
        writePoints (editor.part(), points);
        repaint();
        editor.changed (final);
    }
};

//==============================================================================

//==============================================================================
BodyEditor::BodyEditor (PluginCanvas& c)
    : canvas (c)
{
    // Across the top.
    for (auto* b : { &galleryButton, &openButton, &saveButton, &keepButton, &clearButton })
        addAndMakeVisible (*b);

    galleryButton.setTooltip ("The Body gallery: templates (a desktop module, a keyboard synth, rack units, a Eurorack case, a stompbox...) "
                              "and the bodies kept there, as pictures; click one to use it");
    galleryButton.onClick = [this] { openGallery(); };
    openButton.setTooltip ("A body from a file (.fkbody), fitted to this window");
    openButton.onClick = [this] { openFile(); };
    saveButton.setTooltip ("This body as a file (.fkbody), to use in other plugins or to share");
    saveButton.onClick = [this] { saveFile (false); };
    keepButton.setTooltip ("This body in the Body gallery, under a name of its own");
    keepButton.onClick = [this] { saveFile (true); };

    clearButton.setTooltip ("Every part taken away: no body (the panel as it was)");
    clearButton.onClick = [this]
    {
        if (parts() == nullptr || parts()->isEmpty())
            return;

        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Clear the body",
                                            "Take every part of the body away? (Undo brings it back.)", "Clear", "Cancel", this,
                                            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<BodyEditor> (this)] (int result)
                                            {
                                                if (safe == nullptr || result != 1)
                                                    return;

                                                setProperty (safe->body(), "parts", juce::Array<juce::var>());
                                                safe->chosen = -1;
                                                safe->refresh();
                                                safe->changed (true);
                                            }));
    };

    unitsLabel.setText ("Units", juce::dontSendNotification);
    mmLabel.setText ("Real width", juce::dontSendNotification);
    gridLabel.setText ("Grid", juce::dontSendNotification);

    for (auto* l : { &unitsLabel, &mmLabel, &gridLabel })
    {
        l->setColour (juce::Label::textColourId, Theme::muted);
        l->setFont (Theme::font (12.5f));
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (*l);
    }

    unitsBox.addItem ("Pixels", 1);
    unitsBox.addItem ("Millimetres", 2);
    unitsBox.setTooltip ("Sizes and positions in the window's pixels, or in millimetres of the real instrument (its width below)");
    unitsBox.onChange = [this]
    {
        if (updating)
            return;

        setEditorSetting ("units", unitsBox.getSelectedId() == 2 ? "mm" : "px");
        showUnits();
    };
    addAndMakeVisible (unitsBox);

    mmWidthBox.setInputRestrictions (7, "0123456789.");
    mmWidthBox.setTooltip ("How wide the real instrument is, in millimetres (the window's width): a 19\" rack 482.6, a Eurorack 84 HP case 430...");
    mmWidthBox.onReturnKey = mmWidthBox.onFocusLost = [this]
    {
        if (updating)
            return;

        const auto mm = mmWidthBox.getText().getFloatValue();

        if (mm > 1.0f)
            setEditorSetting ("mmWidth", std::round (mm * 10.0f) / 10.0f);

        showUnits();
    };
    addAndMakeVisible (mmWidthBox);

    for (const auto* step : { "Off", "0.5", "1", "2", "5", "10", "20", "50" })
        gridBox.addItem (step, gridBox.getNumItems() + 1);

    gridBox.setTooltip ("The grid the parts snap to when they're moved and sized (in the units chosen)");
    gridBox.onChange = [this]
    {
        if (updating)
            return;

        setEditorSetting ("grid", gridBox.getSelectedItemIndex() <= 0 ? 0.0 : gridBox.getText().getDoubleValue());
        showUnits();
    };
    addAndMakeVisible (gridBox);

    snapToggle.setTooltip ("Positions and sizes snap to the grid (and a profile's points to fortieths)");
    snapToggle.onClick = [this]
    {
        if (! updating)
        {
            setEditorSetting ("snap", snapToggle.getToggleState());
            showUnits();
        }
    };
    addAndMakeVisible (snapToggle);

    followToggle.setTooltip ("A 3D knob, button or slider standing on a sloped face of the body (a sloped rear panel, a wedge) is seen as that "
                             "face turns it: its top foreshortened, its back or side showing");
    followToggle.onClick = [this]
    {
        if (updating || ! body().isObject())
            return;

        setProperty (body(), "follow", followToggle.getToggleState());
        changed (true);
    };
    addAndMakeVisible (followToggle);

    // The parts.
    list = std::make_unique<PartsList> (*this);
    addAndMakeVisible (*list);

    for (auto* b : { &addButton, &duplicateButton, &deleteButton, &upButton, &downButton })
        addAndMakeVisible (*b);

    addButton.setTooltip ("A new part: a box, a cylinder, a sphere, an extrude (a profile pulled through its box), "
                          "a revolve (a profile turned round its middle), letters, a sweep (a section along a path), a loft (one profile "
                          "becoming another), or a model from an OBJ or STL file");
    addButton.onClick = [this]
    {
        juce::PopupMenu menu;
        const juce::StringArray kinds { "box", "cylinder", "sphere", "extrude", "revolve", "text", "sweep", "loft" };
        const juce::StringArray captions { "Box", "Cylinder", "Sphere", "Extrude (a profile pulled through)", "Revolve (a profile turned round)", "Letters",
                                           "Sweep (a section along a path: a handle, a rail)", "Loft (one profile becoming another)" };

        for (int i = 0; i < kinds.size(); ++i)
            menu.addItem (i + 1, captions[i]);

        menu.addSeparator();
        menu.addItem (101, juce::String::fromUTF8 ("A model from a file (OBJ, STL)\xe2\x80\xa6"));
        menu.addItem (100, "A cut (a box taken away)");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton),
                            [safe = juce::Component::SafePointer<BodyEditor> (this), kinds] (int result)
                            {
                                if (safe == nullptr || result == 0)
                                    return;

                                if (result == 101)
                                    safe->importModel (false);
                                else
                                    safe->addPart (result == 100 ? juce::String ("cut") : kinds[result - 1]);
                            });
    };
    duplicateButton.setTooltip ("A copy of the chosen part, beside it");
    duplicateButton.onClick = [this] { duplicatePart(); };
    deleteButton.setTooltip ("The chosen part taken away (Delete)");
    deleteButton.onClick = [this] { deletePart(); };
    upButton.setTooltip ("Up the list: in front (where its surfaces meet another part's, its colour shows); "
                         "a cut takes away from the parts under it");
    upButton.onClick = [this] { movePart (1); };
    downButton.setTooltip ("Down the list: behind");
    downButton.onClick = [this] { movePart (-1); };

    // The settings.
    rows = std::make_unique<Rows>();
    setUpRows();
    rowsView.setViewedComponent (rows.get(), false);
    rowsView.setScrollBarsShown (true, false);
    addAndMakeVisible (rowsView);

    // The views: the body in 3D; the chosen part's profile.
    for (const auto* name : { "3D view", "Profile" })
        viewTabs.addTab (name, Theme::raised, -1);

    viewTabs.setCurrentTabIndex (0, false);
    viewTabs.addChangeListener (this);
    addAndMakeVisible (viewTabs);

    room = std::make_unique<Showroom> (canvas);
    room->chosenPart = [this] { return chosen; };
    room->onChoose = [this] (int index)
    {
        if (index != chosen)
            choose (index);
    };
    room->onPartEdited = [this] (bool final)
    {
        showPart();       // its rows show where it is now
        changed (final);
    };
    room->gridPixels = [this] { return gridPixels(); };
    room->onDelete = [this] { deletePart(); };

    profile = std::make_unique<ProfileEditor> (*this);
    addAndMakeVisible (*room);
    addChildComponent (*profile);

    refresh();
    setSize (1500, 860);
    startTimer (400);
}

BodyEditor::~BodyEditor()
{
    stopTimer();
    galleryWindow = nullptr;
    canvas.bodyHighlight.clear();
}

std::unique_ptr<juce::DocumentWindow> BodyEditor::makeWindow (PluginCanvas& canvas)
{
    auto* editor = new BodyEditor (canvas);

    // Never bigger than the screen (its parts' buttons at the bottom always in reach).
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (canvas.getScreenBounds()))
        editor->setSize (juce::jmin (editor->getWidth(), display->userArea.getWidth() - 40),
                         juce::jmin (editor->getHeight(), display->userArea.getHeight() - 70));

    auto window = std::make_unique<BodyWindow> ("Showroom", editor, [safe = juce::Component::SafePointer<PluginCanvas> (&canvas)]
    {
        if (safe != nullptr)
            safe->repaint();   // its outline goes
    });

    window->setResizeLimits (1100, 560, 6000, 4000);
    window->centreAroundComponent (&canvas, editor->getWidth(), editor->getHeight());
    return window;
}

//==============================================================================
int BodyEditor::windowWidth() const      { return canvas.layout.width; }
int BodyEditor::windowHeight() const     { return canvas.layout.height; }
float BodyEditor::lightAngle() const     { return canvas.layout.lightAngle; }
float BodyEditor::lightAltitude() const  { return canvas.layout.lightAltitude; }

juce::var& BodyEditor::body()
{
    return canvas.layout.body;
}

juce::Array<juce::var>* BodyEditor::parts()
{
    return canvas.layout.body.getProperty ("parts", {}).getArray();
}

juce::var BodyEditor::part() const
{
    if (const auto* all = canvas.layout.body.getProperty ("parts", {}).getArray(); all != nullptr && juce::isPositiveAndBelow (chosen, all->size()))
        return (*all)[chosen];

    return {};
}

bool BodyEditor::millimetres() const
{
    return canvas.layout.body.getProperty ("editor", {}).getProperty ("units", "px").toString() == "mm";
}

float BodyEditor::unitsPerPixel() const
{
    if (! millimetres())
        return 1.0f;

    const float mm = numberOf (canvas.layout.body.getProperty ("editor", {}), "mmWidth", 450.0f);
    return juce::jmax (0.001f, mm) / (float) juce::jmax (1, canvas.layout.width);
}

float BodyEditor::gridPixels() const
{
    const auto settings = canvas.layout.body.getProperty ("editor", {});

    if (! (bool) settings.getProperty ("snap", true))
        return 0.0f;

    const float grid = numberOf (settings, "grid", millimetres() ? 1.0f : 2.0f);
    return grid > 0.0f ? grid / unitsPerPixel() : 0.0f;
}

void BodyEditor::setEditorSetting (const juce::Identifier& key, const juce::var& value)
{
    if (! body().isObject())
        body() = makeObject();

    auto settings = body().getProperty ("editor", {});

    if (! settings.isObject())
    {
        settings = makeObject();
        setProperty (body(), "editor", settings);
    }

    setProperty (settings, key, value);
    lastSeen = juce::JSON::toString (body(), true);
    canvas.edited();
}

//==============================================================================
void BodyEditor::setUpSlider (juce::Slider& s, const juce::String& tip, std::function<void (double)> apply, bool)
{
    s.setSliderStyle (juce::Slider::LinearBar);
    s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 80, 22);
    s.setColour (juce::Slider::trackColourId, Theme::accentDeep);
    s.setColour (juce::Slider::backgroundColourId, Theme::inset);
    s.setColour (juce::Slider::textBoxTextColourId, Theme::text);
    s.setColour (juce::Slider::textBoxOutlineColourId, Theme::outline);
    s.setTooltip (tip);
    s.onValueChange = [this, &s, apply]
    {
        if (updating)
            return;

        apply (s.getValue());
        changed (! s.isMouseButtonDown());
    };
    s.onDragEnd = [this] { if (! updating) changed (true); };
}

void BodyEditor::setNumber (const juce::Identifier& key, double pixels)
{
    if (const auto p = part(); p.isObject())
        setProperty (p, key, std::round (pixels * 100.0) / 100.0);
}

void BodyEditor::setUpRows()
{
    auto& r = *rows;

    // The body's own.
    setUpSlider (tiltSlider, "The body seen from the front: 0 straight down on the panel, up to 60 degrees", [this] (double v) { setProperty (body(), "tilt", v); });
    tiltSlider.setRange (0, 60, 1);
    tiltSlider.setTextValueSuffix (juce::String::fromUTF8 ("\xc2\xb0"));
    setUpSlider (shadowSlider, "How dark its shadow is on the floor", [this] (double v) { setProperty (body(), "shadow", v); });
    shadowSlider.setRange (0, 100, 1);
    setUpSlider (softnessSlider, "How soft its shadow's edges are", [this] (double v) { setProperty (body(), "softness", v); });
    softnessSlider.setRange (0, 100, 1);
    setUpSlider (exposureSlider, "Brighter or darker", [this] (double v) { setProperty (body(), "exposure", v); });
    exposureSlider.setRange (-100, 100, 1);
    floorSwatch.setTooltip ("The floor under the body: see-through (the panel's background shows) or a colour");
    floorSwatch.onColourChange = [this]
    {
        if (! updating)
        {
            setProperty (body(), "floor", colourText (floorSwatch.getColourValue()));
            changed (true);
        }
    };

    r.header ("View and light");
    r.add ("Tilt", tiltSlider);
    r.add ("Shadow", shadowSlider);
    r.add ("Softness", softnessSlider);
    r.add ("Exposure", exposureSlider);
    r.add ("Floor", floorSwatch, nullptr, 22);

    // A part's.
    nameBox.setTooltip ("What it's called in the list");
    nameBox.onReturnKey = nameBox.onFocusLost = [this]
    {
        if (! updating && part().isObject() && nameBox.getText() != part().getProperty ("name", {}).toString())
        {
            setProperty (part(), "name", nameBox.getText().trim());
            list->repaint();
            changed (true);
        }
    };

    for (const auto* k : { "Box", "Cylinder", "Sphere", "Extrude", "Revolve", "Letters", "Sweep", "Loft", "Model (OBJ, STL)" })
        kindBox.addItem (k, kindBox.getNumItems() + 1);

    kindBox.setTooltip ("Its shape: a box, a cylinder, a sphere, a profile pulled through its box (extrude), a profile turned round its middle "
                        "(revolve), or letters");
    kindBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        const juce::StringArray kinds { "box", "cylinder", "sphere", "extrude", "revolve", "text", "sweep", "loft", "mesh" };
        setProperty (part(), "kind", kinds[kindBox.getSelectedItemIndex()]);
        showPart();
        list->repaint();
        changed (true);
    };

    opBox.addItem ("Adds to the body", 1);
    opBox.addItem ("Cuts it away", 2);
    opBox.setTooltip ("A part adds to the body, or cuts its shape out of the parts before it (a keybed, a screen's well, a slot)");
    opBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        if (opBox.getSelectedId() == 2) setProperty (part(), "op", "cut");
        else                            removeProperty (part(), "op");

        list->repaint();
        changed (true);
    };

    axisBox.addItem ("Upright", 1);
    axisBox.addItem ("Lying across", 2);
    axisBox.addItem ("Lying back to front", 3);
    axisBox.setTooltip ("Which way the cylinder lies");
    axisBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        setProperty (part(), "axis", juce::StringArray { "y", "x", "z" }[axisBox.getSelectedItemIndex()]);
        changed (true);
    };

    planeBox.addItem ("Seen from the side (pulled across)", 1);
    planeBox.addItem ("Seen from the front (pulled back)", 2);
    planeBox.addItem ("Seen from above (pulled up)", 3);
    planeBox.setTooltip ("Which way its profile is drawn, and pulled through its box");
    planeBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        setProperty (part(), "plane", juce::StringArray { "side", "front", "top" }[planeBox.getSelectedItemIndex()]);
        profile->load();
        changed (true);
    };

    r.header ("Part");
    r.add ("Name", nameBox);
    r.add ("Shape", kindBox);
    r.add ("It", opBox);
    r.add ("Lies", axisBox);
    r.add ("Profile", planeBox);

    // Where it lies, how big: in the units chosen.
    auto lengthSlider = [this] (juce::Slider& s, const char* key, const juce::String& tip)
    {
        setUpSlider (s, tip, [this, key] (double v) { setNumber (key, v / (double) unitsPerPixel()); }, true);
    };

    lengthSlider (xSlider, "x", "Its middle, from the window's left");
    lengthSlider (zSlider, "z", "Its middle, from the window's top (the back)");
    lengthSlider (ySlider, "y", "Its foot, above the floor");
    lengthSlider (wSlider, "w", "Its width (across)");
    lengthSlider (dSlider, "d", "Its depth (back to front)");
    lengthSlider (hSlider, "h", "Its height");

    r.header ("Where and how big");
    r.add ("Across, back", xSlider, &zSlider);
    r.add ("Foot above floor", ySlider);
    r.add ("Width, depth", wSlider, &dSlider);
    r.add ("Height", hSlider);

    lengthSlider (roundSlider, "round", "Its edges rounded");
    lengthSlider (chamferSlider, "chamfer", "Its edges cut at 45 degrees");
    lengthSlider (blendSlider, "blend", "Blended into the parts before it (or, a cut, out of them), like a casting");

    auto turnSlider = [this] (juce::Slider& s, int axis, const juce::String& tip)
    {
        setUpSlider (s, tip, [this, axis] (double v)
        {
            if (! part().isObject())
                return;

            auto r3 = rotationOf (part());
            r3[(size_t) axis] = (float) v;

            if (std::abs (r3[0]) + std::abs (r3[1]) + std::abs (r3[2]) < 0.01f)
                removeProperty (part(), "rotate");
            else
                setProperty (part(), "rotate", juce::Array<juce::var> { r3[0], r3[1], r3[2] });
        });
        s.setRange (-180, 180, 0.5);
        s.setTextValueSuffix (juce::String::fromUTF8 ("\xc2\xb0"));
    };

    turnSlider (rotXSlider, 0, "Turned round its across line: a sloped rear panel (negative: rising to the back)");
    turnSlider (rotYSlider, 1, "Turned round its upright");
    turnSlider (rotZSlider, 2, "Turned round its back-to-front line: leaning left or right");

    mirrorToggle.setTooltip ("A copy mirrored across the body's middle: both cheeks, both handles, screws in pairs");
    mirrorToggle.onClick = [this]
    {
        if (updating || ! part().isObject())
            return;

        if (mirrorToggle.getToggleState()) setProperty (part(), "mirror", "x");
        else                               removeProperty (part(), "mirror");

        list->repaint();
        changed (true);
    };

    r.header ("Shape");
    r.add ("Round, chamfer", roundSlider, &chamferSlider);
    r.add ("Blend", blendSlider);
    r.add ("Turn across", rotXSlider);
    r.add ("Turn upright", rotYSlider);
    r.add ("Lean", rotZSlider);
    r.add ({}, mirrorToggle);

    // Its copies.
    for (const auto* k : { "None", "In a row", "In a grid", "Round a circle" })
        repeatBox.addItem (k, repeatBox.getNumItems() + 1);

    repeatBox.setTooltip ("Copies of it: in a row (pads, vents, screws), a grid, or round a circle (bolts round a dial)");
    repeatBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        const auto index = repeatBox.getSelectedItemIndex();

        if (index <= 0)
        {
            removeProperty (part(), "repeat");
        }
        else
        {
            auto repeat = part().getProperty ("repeat", {});

            if (! repeat.isObject())
            {
                repeat = makeObject();
                setProperty (repeat, "count", index == 3 ? 6 : 4);
                setProperty (repeat, "dx", std::round (numberOf (part(), "w", 40) * 1.5f));
                setProperty (repeat, "dz", std::round (numberOf (part(), "d", 40) * 1.5f));
                setProperty (repeat, "countZ", 2);
                setProperty (repeat, "radius", std::round (numberOf (part(), "w", 40) * 2.0f));
                setProperty (part(), "repeat", repeat);
            }

            setProperty (repeat, "kind", juce::StringArray { "", "row", "grid", "circle" }[index]);
        }

        showPart();
        list->repaint();
        changed (true);
    };

    auto repeatSlider = [this] (juce::Slider& s, const char* key, bool length, const juce::String& tip)
    {
        setUpSlider (s, tip, [this, key, length] (double v)
        {
            if (auto repeat = part().getProperty ("repeat", {}); repeat.isObject())
                setProperty (repeat, key, length ? std::round (v / (double) unitsPerPixel() * 100.0) / 100.0 : std::round (v));
        }, length);
    };

    repeatSlider (countSlider, "count", false, "How many (across)");
    countSlider.setRange (1, 64, 1);
    repeatSlider (countZSlider, "countZ", false, "How many back to front");
    countZSlider.setRange (1, 64, 1);
    repeatSlider (dxSlider, "dx", true, "From one to the next, across");
    repeatSlider (dzSlider, "dz", true, "From one to the next, back to front");
    repeatSlider (radiusSlider, "radius", true, "How far round its middle");

    r.header ("Copies");
    r.add ("Copies", repeatBox);
    r.add ("How many", countSlider, &countZSlider);
    r.add ("Step across, back", dxSlider, &dzSlider);
    r.add ("Circle radius", radiusSlider);

    // Its finish.
    for (const auto& m : Model3D::materialNames())
        materialBox.addItem (m, materialBox.getNumItems() + 1);

    materialBox.setTooltip ("What it's made of");
    materialBox.onChange = [this]
    {
        if (! updating && part().isObject())
        {
            setProperty (part(), "material", materialBox.getText());
            changed (true);
        }
    };

    colourSwatch.onColourChange = [this]
    {
        if (! updating && part().isObject())
        {
            setProperty (part(), "color", colourText (colourSwatch.getColourValue()));
            list->repaint();
            changed (true);
        }
    };

    for (const auto* s : { "None", "Tolex", "Knurl", "Wood", "Perforated", "Rubber", "Brushed", "Leather", "Grille" })
        surfaceBox.addItem (s, surfaceBox.getNumItems() + 1);

    surfaceBox.setTooltip ("Its texture: tolex, wood grain, a speaker's perforations or grille, knurling, brushing, rubber, leather");
    surfaceBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        if (surfaceBox.getSelectedItemIndex() <= 0) removeProperty (part(), "surface");
        else                                        setProperty (part(), "surface", surfaceBox.getText().toLowerCase());

        showPart();
        changed (true);
    };

    setUpSlider (surfaceScaleSlider, "How big its texture is", [this] (double v) { setNumber ("surfaceScale", v); });
    surfaceScaleSlider.setRange (0.25, 4.0, 0.05);
    setUpSlider (surfaceDepthSlider, "How deep its texture is", [this] (double v) { setNumber ("surfaceDepth", v); });
    surfaceDepthSlider.setRange (0, 100, 1);
    setUpSlider (glassSlider, "See-through: a display's window, clear acrylic", [this] (double v) { setNumber ("glass", v); });
    glassSlider.setRange (0, 100, 1);

    glowSwatch.setTooltip ("Light it gives (a screen, an LED strip): see-through for none");
    glowSwatch.onColourChange = [this]
    {
        if (! updating && part().isObject())
        {
            if (glowSwatch.getColourValue().getAlpha() == 0) removeProperty (part(), "glow");
            else                                             setProperty (part(), "glow", colourText (glowSwatch.getColourValue()));

            changed (true);
        }
    };

    setUpSlider (glowStrengthSlider, "How bright its light is", [this] (double v) { setNumber ("glowStrength", v); });
    glowStrengthSlider.setRange (0, 100, 1);

    r.header ("Finish");
    r.add ("Material", materialBox);
    r.add ("Colour", colourSwatch, nullptr, 22);
    r.add ("Texture", surfaceBox);
    r.add ("Its size, depth", surfaceScaleSlider, &surfaceDepthSlider);
    r.add ("Glass", glassSlider);
    r.add ("Glow, strength", glowSwatch, &glowStrengthSlider, 22);

    // Letters.
    wordsBox.setTooltip ("The words");
    wordsBox.onReturnKey = wordsBox.onFocusLost = [this]
    {
        if (! updating && part().isObject() && wordsBox.getText() != part().getProperty ("text", {}).toString())
        {
            setProperty (part(), "text", wordsBox.getText());
            changed (true);
        }
    };

    fontBox.addItem ("Standard", 1);

    for (int i = 0; i < (int) FontLibrary::families().size(); ++i)
        fontBox.addItem (FontLibrary::families()[(size_t) i].name, i + 2);

    fontBox.setTooltip ("Their font: one of the panel's fonts");
    fontBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        if (fontBox.getSelectedId() <= 1) removeProperty (part(), "font");
        else                              setProperty (part(), "font", fontBox.getText());

        changed (true);
    };

    boldToggle.onClick = [this]
    {
        if (! updating && part().isObject())
        {
            setProperty (part(), "bold", boldToggle.getToggleState());
            changed (true);
        }
    };

    r.header ("Letters");
    r.add ("Words", wordsBox);
    r.add ("Font", fontBox, &boldToggle);

    // The profile as SVG path data (drawn in the Profile view).
    svgBox.setTooltip ("Its profile as SVG path data, in 0..1 of its plane (M, L, Q, C, A, Z...): typed or pasted; the Profile view draws it");
    svgBox.setFont (Theme::monoFont (12.0f));
    svgBox.onReturnKey = svgBox.onFocusLost = [this]
    {
        if (updating || ! part().isObject() || svgBox.getText().trim() == part().getProperty ("profile", {}).toString())
            return;

        if (juce::Drawable::parseSVGPath (svgBox.getText().trim()).isEmpty())
            return;

        setProperty (part(), "profile", svgBox.getText().trim());
        removeProperty (part(), "points");
        profile->load();
        changed (true);
    };

    r.header ("Profile");
    r.add ("SVG path", svgBox);

    // Hollow: its walls, a face left open.
    setUpSlider (shellSlider, "Hollow: its walls this thick (0: solid)", [this] (double v)
    {
        if (v <= 0.0) removeProperty (part(), "shell");
        else          setNumber ("shell", v / (double) unitsPerPixel());
    }, true);

    for (const auto* o : { "Closed all round", "Open at the top", "Open underneath", "Open at the front", "Open at the back", "Open on the left", "Open on the right" })
        openBox.addItem (o, openBox.getNumItems() + 1);

    openBox.setTooltip ("A hollow part's face left open: a tray, a hood, a sleeve");
    openBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        const juce::StringArray faces { "none", "top", "bottom", "front", "back", "left", "right" };

        if (openBox.getSelectedItemIndex() <= 0) removeProperty (part(), "open");
        else                                     setProperty (part(), "open", faces[openBox.getSelectedItemIndex()]);

        changed (true);
    };

    r.header ("Hollow");
    r.add ("Walls", shellSlider);
    r.add ("Open", openBox);

    // A sweep: its path, its section.
    struct PathPreset { const char* name; const char* path; };
    static const PathPreset pathPresets[] {
        { "An arch (a handle)", "M0.05 1 L0.05 0.35 Q0.05 0.05 0.35 0.05 L0.65 0.05 Q0.95 0.05 0.95 0.35 L0.95 1" },
        { "Straight", "M0 0.5 L1 0.5" },
        { "An arc", "M0 1 Q0.5 -0.6 1 1" },
        { "An S", "M0 0.5 C0.35 -0.2 0.65 1.2 1 0.5" },
        { "A ring", "M0.5 0.05 A0.45 0.45 0 1 1 0.4999 0.05 Z" },
        { "A rounded rectangle (a rail round it)", "M0.15 0.05 L0.85 0.05 Q0.95 0.05 0.95 0.15 L0.95 0.85 Q0.95 0.95 0.85 0.95 L0.15 0.95 Q0.05 0.95 0.05 0.85 L0.05 0.15 Q0.05 0.05 0.15 0.05 Z" },
        { "A wave", "M0 0.5 Q0.125 0.1 0.25 0.5 T0.5 0.5 T0.75 0.5 T1 0.5" },
    };

    pathPresetBox.addItem ("Its own (typed below)", 1);

    for (const auto& preset : pathPresets)
        pathPresetBox.addItem (preset.name, pathPresetBox.getNumItems() + 1);

    pathPresetBox.setTooltip ("A ready path: a handle's arch, a straight rail, an arc, an S, a ring, a rail round a rectangle, a wave");
    pathPresetBox.onChange = [this]
    {
        if (updating || ! part().isObject() || pathPresetBox.getSelectedItemIndex() <= 0)
            return;

        setProperty (part(), "path", pathPresets[pathPresetBox.getSelectedItemIndex() - 1].path);
        showPart();
        changed (true);
    };

    pathBox.setTooltip ("Its path as SVG path data, in 0..1 of its plane (M, L, Q, C, A, Z...): an open line, or a closed one (Z)");
    pathBox.setFont (Theme::monoFont (12.0f));
    pathBox.onReturnKey = pathBox.onFocusLost = [this]
    {
        if (updating || ! part().isObject() || pathBox.getText().trim() == part().getProperty ("path", {}).toString())
            return;

        if (juce::Drawable::parseSVGPath (pathBox.getText().trim()).isEmpty())
            return;

        setProperty (part(), "path", pathBox.getText().trim());
        changed (true);
    };

    setUpSlider (thicknessSlider, "How thick it is: its section's size", [this] (double v) { setNumber ("thickness", v / (double) unitsPerPixel()); }, true);

    for (const auto* k : { "Round (a tube)", "Square", "Its profile (the Profile view)" })
        sectionBox.addItem (k, sectionBox.getNumItems() + 1);

    sectionBox.setTooltip ("Its section along the path: round, square (its corners rounded by Round), or a profile drawn in the Profile view");
    sectionBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        const juce::StringArray sections { "round", "square", "profile" };
        setProperty (part(), "section", sections[sectionBox.getSelectedItemIndex()]);

        if (sectionBox.getSelectedItemIndex() == 2 && ! part().hasProperty ("profile"))
            writePoints (part(), extrudePresets()[0].points);

        showPart();
        changed (true);
    };

    r.header ("Path");
    r.add ("Path", pathPresetBox);
    r.add ("SVG path", pathBox);
    r.add ("Thickness", thicknessSlider);
    r.add ("Section", sectionBox);

    // A loft: its end.
    endBox.setTooltip ("The profile it becomes at its end (SVG path data in 0..1, as its profile): empty, the same profile (sized and turned below)");
    endBox.setFont (Theme::monoFont (12.0f));
    endBox.onReturnKey = endBox.onFocusLost = [this]
    {
        if (updating || ! part().isObject() || endBox.getText().trim() == part().getProperty ("profileEnd", {}).toString())
            return;

        if (endBox.getText().trim().isEmpty())
            removeProperty (part(), "profileEnd");
        else if (! juce::Drawable::parseSVGPath (endBox.getText().trim()).isEmpty())
            setProperty (part(), "profileEnd", endBox.getText().trim());
        else
            return;

        changed (true);
    };

    setUpSlider (taperSlider, "How big its end is: 100 % as its start, under 100 narrowing (a pyramid, a cone), over 100 widening",
                 [this] (double v) { setProperty (part(), "taper", v); });
    taperSlider.setRange (5, 400, 1);
    taperSlider.setTextValueSuffix (" %");
    setUpSlider (twistSlider, "How far its end turns: a twisted column", [this] (double v) { setProperty (part(), "twist", v); });
    twistSlider.setRange (-360, 360, 1);
    twistSlider.setTextValueSuffix (juce::String::fromUTF8 ("\xc2\xb0"));

    r.header ("Loft");
    r.add ("End profile", endBox);
    r.add ("End size", taperSlider);
    r.add ("Twist", twistSlider);

    // A model.
    modelLabel.setColour (juce::Label::textColourId, Theme::text);
    modelLabel.setFont (Theme::font (12.5f));
    modelButton.setTooltip ("Another OBJ or STL file for this part (its size and place kept)");
    modelButton.onClick = [this] { importModel (true); };

    fitBox.addItem ("Keeps its proportions", 1);
    fitBox.addItem ("Stretched to its box", 2);
    fitBox.setTooltip ("The model in its box: as it was made (fitted inside), or stretched to fill it");
    fitBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        setProperty (part(), "fit", fitBox.getSelectedId() == 2 ? "stretch" : "keep");
        changed (true);
    };

    upBox.addItem ("Stands on y (most OBJ models)", 1);
    upBox.addItem ("Stands on z (most STL and CAD models)", 2);
    upBox.setTooltip ("Which way is up in the model's file");
    upBox.onChange = [this]
    {
        if (updating || ! part().isObject())
            return;

        setProperty (part(), "up", upBox.getSelectedId() == 2 ? "z" : "y");
        changed (true);
    };

    r.header ("Model");
    r.add ("File", modelLabel, &modelButton);
    r.add ("Fit", fitBox);
    r.add ("Up", upBox);
}

void BodyEditor::importModel (bool replaceChosen)
{
    chooser = std::make_unique<juce::FileChooser> ("A 3D model for the body", juce::File(), MeshField::wildcard);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<BodyEditor> (this), replaceChosen] (const juce::FileChooser& fc)
    {
        const auto source = fc.getResult();

        if (safe == nullptr || ! source.existsAsFile())
            return;

        // Kept with the design: a copy in its gui folder's models.
        const auto models = safe->canvas.imagesFolder().getParentDirectory().getChildFile (Body3D::modelsFolder);
        models.createDirectory();
        auto copy = models.getChildFile (source.getFileName());

        if (copy != source && copy.existsAsFile() && copy.getSize() != source.getSize())
            copy = models.getNonexistentChildFile (source.getFileNameWithoutExtension(), source.getFileExtension(), false);

        if (copy != source && ! source.copyFileTo (copy))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "A model for the body", "The model couldn't be copied into the design.");
            return;
        }

        const bool upZ = source.getFileExtension().equalsIgnoreCase (".stl");
        juce::String error;
        const auto grid = MeshField::load (copy, upZ, &error);

        if (grid == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "A model for the body", error);
            return;
        }

        const auto name = juce::String (Body3D::modelsFolder) + "/" + copy.getFileName();

        if (replaceChosen && safe->part().isObject())
        {
            setProperty (safe->part(), "kind", "mesh");
            setProperty (safe->part(), "file", name);
            setProperty (safe->part(), "up", upZ ? "z" : "y");
            safe->showPart();
            safe->changed (true);
            return;
        }

        // A new part: its proportions, a quarter of the window big, standing on the body.
        safe->addPart ("mesh");

        if (auto p = safe->part(); p.isObject())
        {
            const float size = (float) juce::jmin (safe->canvas.layout.width, safe->canvas.layout.height) * 0.3f;
            setProperty (p, "name", copy.getFileNameWithoutExtension());
            setProperty (p, "file", name);
            setProperty (p, "up", upZ ? "z" : "y");
            setProperty (p, "w", std::round (size * grid->extent[0]));
            setProperty (p, "h", std::round (size * grid->extent[1]));
            setProperty (p, "d", std::round (size * grid->extent[2]));
            safe->refresh();
            safe->changed (true);
        }
    });
}

//==============================================================================
void BodyEditor::refresh()
{
    const juce::ScopedValueSetter<bool> busy (updating, true);
    lastSeen = juce::JSON::toString (body(), true);

    const auto* list2 = parts();

    if (chosen >= 0 && (list2 == nullptr || chosen >= list2->size()))
        chosen = -1;

    list->updateContent();
    list->selectRow (PartsList::rowForPart (chosen, parts() != nullptr ? parts()->size() : 0), false, true);
    list->repaint();

    const auto settings = body().getProperty ("editor", {});
    unitsBox.setSelectedId (millimetres() ? 2 : 1, juce::dontSendNotification);
    mmWidthBox.setText (juce::String (numberOf (settings, "mmWidth", 450.0f)), false);
    snapToggle.setToggleState ((bool) settings.getProperty ("snap", true), juce::dontSendNotification);
    followToggle.setToggleState ((bool) body().getProperty ("follow", true), juce::dontSendNotification);
    mmLabel.setVisible (millimetres());
    mmWidthBox.setVisible (millimetres());

    const auto grid = numberOf (settings, "grid", millimetres() ? 1.0f : 2.0f);
    gridBox.setSelectedItemIndex (0, juce::dontSendNotification);

    for (int i = 1; i < gridBox.getNumItems(); ++i)
        if (std::abs (gridBox.getItemText (i).getFloatValue() - grid) < 0.001f)
            gridBox.setSelectedItemIndex (i, juce::dontSendNotification);

    showPart();
}

void BodyEditor::showUnits()
{
    {
        const juce::ScopedValueSetter<bool> busy (updating, true);
        mmLabel.setVisible (millimetres());
        mmWidthBox.setVisible (millimetres());
    }

    showPart();
    room->repaint();
}

void BodyEditor::showPart()
{
    const juce::ScopedValueSetter<bool> busy (updating, true);
    const auto p = part();
    const bool isPart = p.isObject();
    auto& r = *rows;

    r.showAll (false);

    // The body's own.
    for (auto* c : std::initializer_list<juce::Component*> { &tiltSlider, &shadowSlider, &softnessSlider, &exposureSlider, &floorSwatch })
        r.show (*c, ! isPart);

    tiltSlider.setValue (numberOf (body(), "tilt", 0), juce::dontSendNotification);
    shadowSlider.setValue (numberOf (body(), "shadow", 60), juce::dontSendNotification);
    softnessSlider.setValue (numberOf (body(), "softness", 55), juce::dontSendNotification);
    exposureSlider.setValue (numberOf (body(), "exposure", 0), juce::dontSendNotification);
    floorSwatch.setColourValue (colourOf (body().getProperty ("floor", {}), juce::Colour (0x00000000)), juce::dontSendNotification);

    if (isPart)
    {
        const auto kind = p.getProperty ("kind", "box").toString().toLowerCase();
        const auto upp = (double) unitsPerPixel();
        const auto unit = millimetres() ? juce::String (" mm") : juce::String (" px");
        const double grid = snapToggle.getToggleState() ? (double) gridPixels() * upp : 0.0;
        const double step = grid > 0.0 ? grid : (millimetres() ? 0.1 : 0.5);
        const double W = canvas.layout.width * upp, H = canvas.layout.height * upp, big = juce::jmax (W, H);

        auto length = [&] (juce::Slider& s, double from, double to, double pixels)
        {
            from = std::floor (from / step) * step;
            s.setRange (from, juce::jmax (from + step, to), step);
            s.setTextValueSuffix (unit);
            s.setValue (pixels * upp, juce::dontSendNotification);
        };

        nameBox.setText (p.getProperty ("name", {}).toString(), false);
        const juce::StringArray kinds { "box", "cylinder", "sphere", "extrude", "revolve", "text", "sweep", "loft", "mesh" };
        kindBox.setSelectedItemIndex (juce::jmax (0, kinds.indexOf (kind)), juce::dontSendNotification);
        opBox.setSelectedId (p.getProperty ("op", "add").toString().equalsIgnoreCase ("cut") ? 2 : 1, juce::dontSendNotification);
        axisBox.setSelectedItemIndex (juce::jmax (0, juce::StringArray { "y", "x", "z" }.indexOf (p.getProperty ("axis", "y").toString().toLowerCase())), juce::dontSendNotification);
        planeBox.setSelectedItemIndex (juce::jmax (0, juce::StringArray { "side", "front", "top" }.indexOf (p.getProperty ("plane", "side").toString().toLowerCase())), juce::dontSendNotification);

        // Room to grow: a part may reach well past the window (its sizes are never stuck at it).
        length (xSlider, -W, W * 2.0, numberOf (p, "x", 0));
        length (zSlider, -H, H * 2.0, numberOf (p, "z", 0));
        length (ySlider, -big, big * 2.0, numberOf (p, "y", 0));
        length (wSlider, step, juce::jmax (W * 3.0, numberOf (p, "w", 100) * upp * 1.5), numberOf (p, "w", 100));
        length (dSlider, step, juce::jmax (H * 3.0, numberOf (p, "d", 100) * upp * 1.5), numberOf (p, "d", 100));
        length (hSlider, step, juce::jmax (big * 2.0, numberOf (p, "h", 20) * upp * 1.5), numberOf (p, "h", 20));
        length (roundSlider, 0, big * 0.1, numberOf (p, "round", 0));
        length (chamferSlider, 0, big * 0.1, numberOf (p, "chamfer", 0));
        length (blendSlider, 0, big * 0.1, numberOf (p, "blend", 0));

        const auto r3 = rotationOf (p);
        rotXSlider.setValue (r3[0], juce::dontSendNotification);
        rotYSlider.setValue (r3[1], juce::dontSendNotification);
        rotZSlider.setValue (r3[2], juce::dontSendNotification);
        mirrorToggle.setToggleState (p.getProperty ("mirror", {}).toString().containsIgnoreCase ("x"), juce::dontSendNotification);

        const auto repeat = p.getProperty ("repeat", {});
        const auto repeatKind = repeat.isObject() ? repeat.getProperty ("kind", "row").toString().toLowerCase() : juce::String();
        repeatBox.setSelectedItemIndex (repeatKind == "row" ? 1 : (repeatKind == "grid" ? 2 : (repeatKind == "circle" ? 3 : 0)), juce::dontSendNotification);
        countSlider.setValue (numberOf (repeat, "count", 2), juce::dontSendNotification);
        countZSlider.setValue (numberOf (repeat, "countZ", 1), juce::dontSendNotification);
        length (dxSlider, -big * 0.5, big * 0.5, numberOf (repeat, "dx", numberOf (p, "w", 40) * 1.5f));
        length (dzSlider, -big * 0.5, big * 0.5, numberOf (repeat, "dz", numberOf (p, "d", 40) * 1.5f));
        length (radiusSlider, 0, big * 0.5, numberOf (repeat, "radius", 0));

        const auto material = p.getProperty ("material", {});
        int materialIndex = 0;

        if (material.isString())
            materialIndex = juce::jmax (0, Model3D::materialNames().indexOf (material.toString(), true));
        else if (! material.isVoid())
            materialIndex = juce::jlimit (0, Model3D::materialNames().size() - 1, (int) material);

        materialBox.setSelectedItemIndex (materialIndex, juce::dontSendNotification);
        colourSwatch.setColourValue (colourOf (p.getProperty ("color", p.getProperty ("colour", {})), juce::Colour (0xff202024)), juce::dontSendNotification);

        const juce::StringArray surfaces { "none", "tolex", "knurl", "wood", "perforated", "rubber", "brushed", "leather", "grille" };
        const auto surface = juce::jmax (0, surfaces.indexOf (p.getProperty ("surface", "none").toString().toLowerCase()));
        surfaceBox.setSelectedItemIndex (surface, juce::dontSendNotification);
        surfaceScaleSlider.setValue (numberOf (p, "surfaceScale", 1), juce::dontSendNotification);
        surfaceDepthSlider.setValue (numberOf (p, "surfaceDepth", 50), juce::dontSendNotification);
        glassSlider.setValue (numberOf (p, "glass", 0), juce::dontSendNotification);
        glowSwatch.setColourValue (colourOf (p.getProperty ("glow", {}), juce::Colour (0x00000000)), juce::dontSendNotification);
        glowStrengthSlider.setValue (numberOf (p, "glowStrength", 60), juce::dontSendNotification);

        wordsBox.setText (p.getProperty ("text", {}).toString(), false);
        fontBox.setSelectedId (1, juce::dontSendNotification);

        for (int i = 0; i < fontBox.getNumItems(); ++i)
            if (fontBox.getItemText (i).equalsIgnoreCase (p.getProperty ("font", {}).toString()))
                fontBox.setSelectedItemIndex (i, juce::dontSendNotification);

        boldToggle.setToggleState ((bool) p.getProperty ("bold", false), juce::dontSendNotification);
        svgBox.setText (p.getProperty ("profile", {}).toString(), false);

        // Hollow; a sweep's; a loft's; a model's.
        length (shellSlider, 0, big * 0.05, numberOf (p, "shell", 0));
        openBox.setSelectedItemIndex (juce::jmax (0, juce::StringArray { "none", "top", "bottom", "front", "back", "left", "right" }
                                                         .indexOf (p.getProperty ("open", "none").toString().toLowerCase())), juce::dontSendNotification);
        pathBox.setText (p.getProperty ("path", {}).toString(), false);
        pathPresetBox.setSelectedItemIndex (0, juce::dontSendNotification);
        length (thicknessSlider, step, big * 0.1, numberOf (p, "thickness", 10));
        sectionBox.setSelectedItemIndex (juce::jmax (0, juce::StringArray { "round", "square", "profile" }
                                                            .indexOf (p.getProperty ("section", "round").toString().toLowerCase())), juce::dontSendNotification);
        endBox.setText (p.getProperty ("profileEnd", {}).toString(), false);
        taperSlider.setValue (numberOf (p, "taper", 100), juce::dontSendNotification);
        twistSlider.setValue (numberOf (p, "twist", 0), juce::dontSendNotification);
        modelLabel.setText (p.getProperty ("file", "(none)").toString().fromLastOccurrenceOf ("/", false, false), juce::dontSendNotification);
        fitBox.setSelectedId (p.getProperty ("fit", "keep").toString().equalsIgnoreCase ("stretch") ? 2 : 1, juce::dontSendNotification);
        upBox.setSelectedId (p.getProperty ("up", "y").toString().equalsIgnoreCase ("z") ? 2 : 1, juce::dontSendNotification);

        // What applies to this kind.
        const bool profiled = kind == "extrude" || kind == "revolve";

        for (auto* c : std::initializer_list<juce::Component*> { &nameBox, &kindBox, &opBox, &xSlider, &ySlider, &wSlider, &hSlider,
                                                                  &roundSlider, &blendSlider, &rotXSlider, &rotYSlider, &rotZSlider,
                                                                  &mirrorToggle, &repeatBox, &materialBox, &colourSwatch, &surfaceBox, &glassSlider, &glowSwatch })
            r.show (*c, true);

        // A cut has no finish: it's a hole.
        const bool cut = p.getProperty ("op", "add").toString().equalsIgnoreCase ("cut");

        for (auto* c : std::initializer_list<juce::Component*> { &materialBox, &colourSwatch, &surfaceBox, &glassSlider, &glowSwatch })
            r.show (*c, ! cut);

        r.show (axisBox, kind == "cylinder");
        r.show (planeBox, kind == "extrude" || kind == "sweep" || kind == "loft");
        r.show (shellSlider, kind != "text" && kind != "sweep");
        r.show (openBox, kind != "text" && kind != "sweep" && numberOf (p, "shell", 0) > 0.0f);
        r.show (pathPresetBox, kind == "sweep");
        r.show (pathBox, kind == "sweep");
        r.show (thicknessSlider, kind == "sweep");
        r.show (sectionBox, kind == "sweep");
        r.show (endBox, kind == "loft");
        r.show (taperSlider, kind == "loft");
        r.show (twistSlider, kind == "loft");
        r.show (modelLabel, kind == "mesh");
        r.show (fitBox, kind == "mesh");
        r.show (upBox, kind == "mesh");
        r.show (countSlider, repeatKind.isNotEmpty());
        r.show (dxSlider, repeatKind == "row" || repeatKind == "grid");
        r.show (radiusSlider, repeatKind == "circle");
        r.show (surfaceScaleSlider, surface > 0 && ! cut);
        r.show (wordsBox, kind == "text");
        r.show (fontBox, kind == "text");
        r.show (svgBox, profiled || kind == "loft" || (kind == "sweep" && p.getProperty ("section", {}).toString().equalsIgnoreCase ("profile")));

        // A grid's second count and step.
        countZSlider.setEnabled (repeatKind == "grid");
        dzSlider.setEnabled (repeatKind == "grid");
    }

    rows->setSize (juce::jmax (200, rowsView.getMaximumVisibleWidth()), 10);
    rows->setSize (rows->getWidth(), rows->layOut (rows->getWidth()));
    rows->repaint();

    profile->load();
    room->repaint();
    highlight();
}

void BodyEditor::choose (int index)
{
    chosen = index;

    {
        const juce::ScopedValueSetter<bool> busy (updating, true);
        list->selectRow (PartsList::rowForPart (chosen, parts() != nullptr ? parts()->size() : 0), false, true);
    }

    showPart();

    // A profiled part: its profile, at once.
    const auto kind = part().getProperty ("kind", {}).toString().toLowerCase();
    const bool profiled = kind == "extrude" || kind == "revolve" || kind == "loft"
                       || (kind == "sweep" && part().getProperty ("section", {}).toString().equalsIgnoreCase ("profile"));

    // Its profile on the second tab (chosen there); a part without one: the 3D view.
    viewTabs.setTabName (1, profiled ? juce::String ("Profile (its outline)") : juce::String ("Profile"));

    if (! profiled && viewTabs.getCurrentTabIndex() == 1)
    {
        viewTabs.setCurrentTabIndex (0, false);
        showView();
    }

    room->repaint();
}

void BodyEditor::changed (bool final)
{
    lastSeen = juce::JSON::toString (body(), true);
    room->repaint();
    list->repaint();
    highlight();

    if (final)
        canvas.edited();
    else
        canvas.repaint();
}

void BodyEditor::highlight()
{
    juce::Path outline;

    if (const auto p = part(); p.isObject())
        outline = PlanView::outlineOf (p, 0, Body3D::mirrorAtOf (body(), canvas.layout.width));

    canvas.bodyHighlight = outline;
    canvas.repaint();
}

void BodyEditor::timerCallback()
{
    // The body changed elsewhere (an undo, another project, Fanan AI): shown as it is now.
    const auto now = juce::JSON::toString (body(), true);

    if (now != lastSeen)
        refresh();
}

//==============================================================================
void BodyEditor::addPart (const juce::String& kind)
{
    if (! body().isObject())
        body() = makeObject();

    if (parts() == nullptr)
        setProperty (body(), "parts", juce::Array<juce::var>());

    const float W = (float) canvas.layout.width, H = (float) canvas.layout.height, u = juce::jmin (W, H);
    auto p = makeObject();
    const bool cut = kind == "cut";
    const auto shape = cut ? juce::String ("box") : kind;

    // The body's top where it goes: a new part stands on it.
    float top = 0.0f;

    if (const auto* list2 = parts())
        for (const auto& other : *list2)
            if (! other.getProperty ("op", "add").toString().equalsIgnoreCase ("cut"))
                top = juce::jmax (top, numberOf (other, "y", 0) + numberOf (other, "h", 20));

    const juce::StringArray names { "box", "cylinder", "sphere", "extrude", "revolve", "text", "sweep", "loft", "mesh" };
    const juce::StringArray captions { "Box", "Cylinder", "Sphere", "Extrude", "Revolve", "Letters", "Sweep", "Loft", "Model" };
    setProperty (p, "name", cut ? juce::String ("Cut") : captions[juce::jmax (0, names.indexOf (shape))] + " " + juce::String (parts()->size() + 1));
    setProperty (p, "kind", shape);
    setProperty (p, "x", std::round (W * 0.5f));
    setProperty (p, "z", std::round (H * 0.5f));

    if (cut)
    {
        setProperty (p, "op", "cut");
        setProperty (p, "y", std::round (top - u * 0.04f));
        setProperty (p, "w", std::round (W * 0.3f));
        setProperty (p, "h", std::round (u * 0.08f));
        setProperty (p, "d", std::round (H * 0.2f));
    }
    else if (parts()->isEmpty())
    {
        // The first: the case itself.
        setProperty (p, "y", 0);
        setProperty (p, "w", std::round (W * 0.92f));
        setProperty (p, "h", std::round (u * 0.12f));
        setProperty (p, "d", std::round (H * 0.88f));
        setProperty (p, "round", std::round (u * 0.015f));
        setProperty (p, "material", "Painted metal");
        setProperty (p, "color", "#FF1E1F23");
    }
    else
    {
        const float size = shape == "text" ? u * 0.25f : u * 0.12f;
        setProperty (p, "y", std::round (top));
        setProperty (p, "w", std::round (size));
        setProperty (p, "h", std::round (shape == "text" ? 2.0f : u * 0.05f));
        setProperty (p, "d", std::round (shape == "text" ? size * 0.25f : size));
        setProperty (p, "material", "Satin plastic");
        setProperty (p, "color", "#FF3A3C42");

        if (shape == "text")
            setProperty (p, "text", "LOGO");
    }

    if (shape == "extrude")
        writePoints (p, extrudePresets()[0].points);
    else if (shape == "revolve")
        writePoints (p, revolvePresets()[3].points);
    else if (shape == "sweep")
    {
        // A handle standing up on the body: an arch seen from the front.
        setProperty (p, "plane", "front");
        setProperty (p, "w", std::round (u * 0.3f));
        setProperty (p, "h", std::round (u * 0.12f));
        setProperty (p, "d", 14);
        setProperty (p, "thickness", std::round (juce::jmax (4.0f, u * 0.025f)));
        setProperty (p, "path", "M0.05 1 L0.05 0.35 Q0.05 0.05 0.35 0.05 L0.65 0.05 Q0.95 0.05 0.95 0.35 L0.95 1");
        setProperty (p, "material", "Chrome");
        setProperty (p, "color", "#FFD8DADF");
    }
    else if (shape == "loft")
    {
        // A square narrowing as it rises.
        setProperty (p, "plane", "top");
        setProperty (p, "h", std::round (u * 0.1f));
        setProperty (p, "profile", "M0 0 L1 0 L1 1 L0 1 Z");
        setProperty (p, "taper", 60);
    }

    parts()->add (p);
    choose (parts()->size() - 1);
    refresh();
    changed (true);
}

void BodyEditor::duplicatePart()
{
    if (! part().isObject())
        return;

    auto copy = part().clone();
    setProperty (copy, "x", numberOf (copy, "x", 0) + gridPixels() * 4.0f + 10.0f);
    setProperty (copy, "name", part().getProperty ("name", {}).toString() + " copy");
    parts()->insert (chosen + 1, copy);
    chosen = chosen + 1;
    refresh();
    changed (true);
}

void BodyEditor::toggleHidden (int index)
{
    if (parts() == nullptr || ! juce::isPositiveAndBelow (index, parts()->size()))
        return;

    const auto& p = parts()->getReference (index);

    if (auto* o = p.getDynamicObject())
    {
        if ((bool) p.getProperty ("hidden", false))
            o->removeProperty ("hidden");
        else
            o->setProperty ("hidden", true);
    }

    list->repaint();
    changed (true);
}

void BodyEditor::deletePart()
{
    if (! part().isObject())
        return;

    parts()->remove (chosen);
    chosen = juce::jmin (chosen, parts()->size() - 1);
    refresh();
    changed (true);
}

void BodyEditor::movePart (int by)
{
    if (! part().isObject())
        return;

    const auto to = chosen + by;

    if (! juce::isPositiveAndBelow (to, parts()->size()))
        return;

    parts()->swap (chosen, to);
    chosen = to;
    refresh();
    changed (true);
}

void BodyEditor::useBody (const juce::var& newBody, int fromWidth, int fromHeight)
{
    auto apply = [safe = juce::Component::SafePointer<BodyEditor> (this), newBody, fromWidth, fromHeight]
    {
        if (safe == nullptr)
            return;

        safe->body() = scaledBody (newBody, fromWidth, fromHeight, safe->canvas.layout.width, safe->canvas.layout.height);
        safe->chosen = -1;
        safe->refresh();
        safe->changed (true);
    };

    // Over a body: asked first.
    if (parts() != nullptr && ! parts()->isEmpty())
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Use this body",
                                            "It takes the place of the body there is now. (Undo brings that one back.)", "Use it", "Cancel", this,
                                            juce::ModalCallbackFunction::create ([apply] (int result)
                                            {
                                                if (result == 1)
                                                    apply();
                                            }));
        return;
    }

    apply();
}

void BodyEditor::openFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a body", galleryFolder(), "*.fkbody");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<BodyEditor> (this)] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;

                              const auto json = juce::JSON::parse (fc.getResult());

                              if (! json.getProperty ("body", {}).isObject())
                              {
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Open a body",
                                                                          fc.getResult().getFileName() + " isn't a body file.");
                                  return;
                              }

                              safe->useBody (json.getProperty ("body", {}), (int) json.getProperty ("width", safe->canvas.layout.width),
                                             (int) json.getProperty ("height", safe->canvas.layout.height));
                          });
}

void BodyEditor::saveFile (bool toGallery)
{
    if (parts() == nullptr || parts()->isEmpty())
        return;

    auto file = makeObject();
    setProperty (file, "format", 1);
    setProperty (file, "width", canvas.layout.width);
    setProperty (file, "height", canvas.layout.height);
    setProperty (file, "body", body().clone());
    const auto text = juce::JSON::toString (file);

    if (toGallery)
    {
        auto* ask = new juce::AlertWindow ("Keep in the Body gallery", "Its name in the gallery:", juce::MessageBoxIconType::NoIcon, this);
        ask->addTextEditor ("name", "My body");
        ask->addButton ("Keep", 1, juce::KeyPress (juce::KeyPress::returnKey));
        ask->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        ask->enterModalState (true, juce::ModalCallbackFunction::create ([ask, text] (int result)
        {
            const auto name = juce::File::createLegalFileName (ask->getTextEditorContents ("name").trim());

            if (result != 1 || name.isEmpty())
                return;

            const auto target = galleryFolder().getChildFile (name + ".fkbody");

            if (! target.replaceWithText (text))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Keep in the Body gallery",
                                                        "It couldn't be written to " + target.getFullPathName());
        }), true);
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Save the body", galleryFolder().getChildFile ("Body.fkbody"), "*.fkbody");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [text] (const juce::FileChooser& fc)
                          {
                              if (fc.getResult() == juce::File())
                                  return;

                              fc.getResult().withFileExtension ("fkbody").replaceWithText (text);
                          });
}

void BodyEditor::openGallery()
{
    // The bodies are in the Gallery window, on their own page.
    GalleryHub::show (canvas, GalleryHub::Page::bodies, this);
}

void BodyEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    showView();
}

void BodyEditor::showView()
{
    const auto index = viewTabs.getCurrentTabIndex();
    room->setVisible (index == 0);
    profile->setVisible (index == 1);

    if (index == 1)
        profile->load();
}

//==============================================================================
void BodyEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (44, 0.0f, (float) getWidth());
}

void BodyEditor::resized()
{
    auto area = getLocalBounds();

    // Across the top.
    {
        auto top = area.removeFromTop (44).reduced (8, 8);
        galleryButton.setBounds (top.removeFromLeft (92));
        top.removeFromLeft (4);
        openButton.setBounds (top.removeFromLeft (70));
        top.removeFromLeft (4);
        saveButton.setBounds (top.removeFromLeft (84));
        top.removeFromLeft (4);
        keepButton.setBounds (top.removeFromLeft (128));
        top.removeFromLeft (4);
        clearButton.setBounds (top.removeFromLeft (58));
        top.removeFromLeft (12);

        followToggle.setBounds (top.removeFromRight (232));
        top.removeFromRight (8);
        snapToggle.setBounds (top.removeFromRight (64));
        gridBox.setBounds (top.removeFromRight (66));
        gridLabel.setBounds (top.removeFromRight (40));
        mmWidthBox.setBounds (top.removeFromRight (64));
        mmLabel.setBounds (top.removeFromRight (74));
        unitsBox.setBounds (top.removeFromRight (110));
        unitsLabel.setBounds (top.removeFromRight (46));
    }

    area.reduce (8, 8);

    // The parts.
    {
        auto left = area.removeFromLeft (290);
        auto buttons = left.removeFromBottom (28);
        left.removeFromBottom (6);
        list->setBounds (left);
        addButton.setBounds (buttons.removeFromLeft (60));
        buttons.removeFromLeft (4);
        duplicateButton.setBounds (buttons.removeFromLeft (74));
        buttons.removeFromLeft (4);
        deleteButton.setBounds (buttons.removeFromLeft (56));
        buttons.removeFromLeft (4);
        downButton.setBounds (buttons.removeFromRight (46));
        buttons.removeFromRight (2);
        upButton.setBounds (buttons);
    }

    area.removeFromLeft (8);

    // The settings.
    rowsView.setBounds (area.removeFromLeft (390));
    rows->setSize (rowsView.getMaximumVisibleWidth(), rows->layOut (rowsView.getMaximumVisibleWidth()));

    area.removeFromLeft (8);

    // The views.
    viewTabs.setBounds (area.removeFromTop (28));
    room->setBounds (area);
    profile->setBounds (area);
}

//==============================================================================
juce::StringArray BodyEditor::templateNames()
{
    return { "Desktop module", "Keyboard synth", "Vintage synth (sloped panel)", "Drum machine (wedge)", "Groovebox",
             "Mixer with a meter bridge", "Stompbox", "Eurorack case",
             "Guitar pedal (treadle stomp)", "Guitar pedal (wah rocker)", "Monosynth (flip-up panel)", "Pad sampler (4x4 pads)",
             "Modular suitcase (3 rows)", "Tape echo", "Amp head (tolex)", "Combo amp (top panel)",
             "500-series lunchbox", "Tube console (wood)", "Sloped desk console", "DJ mixer",
             "DJ controller (two decks)", "Pocket synth (bare board)", "Portable keyboard (speakers)", "Drawbar organ",
             "Semi-modular (patch bay)", "Step sequencer (16 steps)", "Vintage compressor (meter)", "Sampler workstation (floppy)",
             "Compact mixer (8 channels)", "Aluminium extrusion", "Modern poly (wood cheeks)", "Retro sci-fi console",
             "Keyboard controller (pads)", "Eurorack skiff (slanted)", "Boutique desktop (rounded)", "Transistor radio",
             "Looper station (big wheel)", "Tube amp chassis",
             "Guitar pedal (mini)", "Guitar pedal (dual footswitch)", "Guitar pedal (hammertone)", "Guitar pedal (slanted, top jacks)",
             "Pedalboard (five pedals)", "Speaker cab (4x12)", "Amp stack (head on a cab)", "Practice amp",
             "Rack preamp (tube window)", "MIDI foot controller (10 switches)", "Multi-FX floorboard (big screen)", "Multi-FX compact (four switches)",
             "Multi-FX desktop modeler (touch screen)", "Multi-FX with expression pedal", "Multi-FX mini (pedal-size modeler)",
             "Rack 1U", "Rack 2U", "Rack 3U" };
}

juce::var BodyEditor::makeTemplate (int index, int width, int height)
{
    width = juce::jmax (8, width);
    height = juce::jmax (8, height);

    if (index >= jsonTemplates)
        return rackTemplate (juce::jlimit (1, 3, index - jsonTemplates + 1), width, height);

    auto body = juce::JSON::parse (juce::String (templateJson[juce::jlimit (0, jsonTemplates - 1, index)]));

    // Profiles from their points.
    if (const auto* list = body.getProperty ("parts", {}).getArray())
        for (const auto& part : *list)
            if (part.getProperty ("points", {}).isArray())
                writePoints (part, pointsOf (part));

    return scaledBody (body, 900, 460, width, height);
}

juce::var BodyEditor::scaledBody (const juce::var& source, int fromWidth, int fromHeight, int toWidth, int toHeight)
{
    auto body = source.clone();
    const double sx = (double) juce::jmax (1, toWidth) / (double) juce::jmax (1, fromWidth);
    const double sz = (double) juce::jmax (1, toHeight) / (double) juce::jmax (1, fromHeight);
    const double sy = juce::jmin (sx, sz);

    if (std::abs (sx - 1.0) < 1.0e-6 && std::abs (sz - 1.0) < 1.0e-6)
        return body;

    auto scale = [] (const juce::var& v, const char* key, double by)
    {
        if (v.hasProperty (key))
            setProperty (v, key, std::round ((double) v.getProperty (key, 0) * by * 100.0) / 100.0);
    };

    scale (body, "mirrorAt", sx);   // the middle its mirrored parts are mirrored across

    if (const auto* list = body.getProperty ("parts", {}).getArray())
    {
        for (const auto& part : *list)
        {
            scale (part, "x", sx);
            scale (part, "w", sx);
            scale (part, "z", sz);
            scale (part, "d", sz);

            for (const auto* key : { "y", "h", "round", "chamfer", "blend", "shell", "thickness" })
                scale (part, key, sy);

            if (const auto repeat = part.getProperty ("repeat", {}); repeat.isObject())
            {
                scale (repeat, "dx", sx);
                scale (repeat, "dz", sz);
                scale (repeat, "radius", sy);
            }
        }
    }

    return body;
}

juce::File BodyEditor::galleryFolder()
{
    auto folder = SettingsFolder::folder ("Body Gallery");
    folder.createDirectory();
    return folder;
}
