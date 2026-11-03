// D:\workspace\Fanan_web_hub\src\ObjectPaint.cpp

#include "ObjectPaint.h"
#include "LayerRender.h"    // Photoshop's layer style
#include "stella_paint.h"   // the materials, finishes and patterns, as PanelPainter paints them

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;

    juce::String settingText (const GuiWidget& w, const char* key)
    {
        const auto found = w.settings.find (key);
        return found != w.settings.end() && ! found->second.isVoid() ? found->second.toString().trim() : juce::String();
    }

    /** The material it's made of; empty for a flat colour. */
    juce::String materialOf (const GuiWidget& w)
    {
        const auto m = settingText (w, "material").toLowerCase();
        return m == "none" || m == "flat" || m == "colour" || m == "color" ? juce::String() : m;
    }

    juce::Colour colourSetting (const GuiWidget& w, const char* key)
    {
        return GuiLayout::colourFromString (settingText (w, key), juce::Colours::transparentBlack);
    }

    /** How round its corners are, 0 sharp .. 1 as round as they go (a rectangle: a pill).
        "rounding"; a rectangle from before it: "radius" in pixels. */
    float roundingOf (const GuiWidget& w, float boxW, float boxH)
    {
        if (w.settings.count ("rounding") > 0)
            return juce::jlimit (0.0f, 1.0f, w.number ("rounding", 0.0f));

        const float half = juce::jmin (boxW, boxH) * 0.5f;
        return half > 0.0f ? juce::jlimit (0.0f, 1.0f, w.number ("radius", 0.0f) / half) : 0.0f;
    }

    /** Its shape before it's stretched to its box: in -1..1 (about). */
    juce::Path unitShape (const GuiWidget& w, const juce::String& kind, float boxW, float boxH)
    {
        juce::Path p;

        if (kind == "circle")
        {
            p.addEllipse (-1.0f, -1.0f, 2.0f, 2.0f);
        }
        else if (kind == "triangle")
        {
            p.addTriangle (0.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f);
        }
        else if (kind == "star")
        {
            const int points = juce::jlimit (3, 32, juce::roundToInt (w.number ("points", 5.0f)));
            const float inner = juce::jlimit (0.05f, 1.0f, w.number ("inner", 0.45f));

            for (int k = 0; k < points * 2; ++k)
            {
                const float a = -pi * 0.5f + (float) k * pi / (float) points;
                const float r = (k % 2) == 0 ? 1.0f : inner;
                const juce::Point<float> at (r * std::cos (a), r * std::sin (a));

                if (k == 0) p.startNewSubPath (at);
                else        p.lineTo (at);
            }

            p.closeSubPath();
        }
        else if (kind == "drawn")
        {
            p = juce::Drawable::parseSVGPath (w.path);

            if (! p.isEmpty() && p.getBounds().getWidth() > 0.0f && p.getBounds().getHeight() > 0.0f)
                p.applyTransform (p.getTransformToScaleToFit (-1.0f, -1.0f, 2.0f, 2.0f, false));
            else
                p.clear();
        }

        if (p.isEmpty())
        {
            // A rectangle, its corners rounded in pixels whatever its box: fully round, a pill.
            const float r = roundingOf (w, boxW, boxH) * juce::jmin (boxW, boxH) * 0.5f;
            const float rx = juce::jmin (1.0f, 2.0f * r / juce::jmax (1.0f, boxW));
            const float ry = juce::jmin (1.0f, 2.0f * r / juce::jmax (1.0f, boxH));

            if (rx > 0.001f && ry > 0.001f)
                p.addRoundedRectangle (-1.0f, -1.0f, 2.0f, 2.0f, rx, ry);
            else
                p.addRectangle (-1.0f, -1.0f, 2.0f, 2.0f);
        }

        return p;
    }

    /** Lopsided: its middle moved to (cx, cy) (-1..1 each way) with its edges kept where they
        are, then leant: leanX moves its top right and its bottom left, leanY lifts its right
        side and drops its left. Every point of the path moves (curves keep their shape). */
    juce::Path lopsided (const juce::Path& unit, float cx, float cy, float leanX, float leanY)
    {
        if (cx == 0.0f && cy == 0.0f && leanX == 0.0f && leanY == 0.0f)
            return unit;

        auto pull = [] (float u, float c) { return u < 0.0f ? -1.0f + (u + 1.0f) * (1.0f + c) : c + u * (1.0f - c); };
        auto map = [&] (float x, float y)
        {
            const float px = pull (x, cx), py = pull (y, cy);
            return juce::Point<float> (px - leanX * py * 0.5f, py - leanY * px * 0.5f);
        };

        juce::Path out;
        juce::Path::Iterator it (unit);

        while (it.next())
        {
            switch (it.elementType)
            {
                case juce::Path::Iterator::startNewSubPath: out.startNewSubPath (map (it.x1, it.y1)); break;
                case juce::Path::Iterator::lineTo:          out.lineTo (map (it.x1, it.y1)); break;
                case juce::Path::Iterator::quadraticTo:     out.quadraticTo (map (it.x1, it.y1), map (it.x2, it.y2)); break;
                case juce::Path::Iterator::cubicTo:         out.cubicTo (map (it.x1, it.y1), map (it.x2, it.y2), map (it.x3, it.y3)); break;
                case juce::Path::Iterator::closePath:       out.closeSubPath(); break;
                default: break;
            }
        }

        return out;
    }

    float clamp01 (float v) noexcept    { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    //==========================================================================
    // Per corner: the box's corners as fractions of its inset box.
    using CornerFractions = std::array<float, 8>;   // x, y of top-left, top-right, bottom-right, bottom-left

    constexpr CornerFractions boxCorners { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };

    bool readCorners (const GuiWidget& w, CornerFractions& out)
    {
        const auto found = w.settings.find ("corners");

        if (found == w.settings.end() || w.shapeKind() == "line")
            return false;

        const auto* list = found->second.getArray();

        if (list == nullptr || list->size() != 8)
            return false;

        for (int i = 0; i < 8; ++i)
        {
            const auto v = (double) list->getReference (i);

            if (! std::isfinite (v))
                return false;

            out[(size_t) i] = juce::jlimit (-4.0f, 5.0f, (float) v);
        }

        return true;
    }

    void writeCorners (GuiWidget& w, const CornerFractions& f)
    {
        juce::Array<juce::var> list;

        for (const auto v : f)
            list.add (juce::roundToInt (v * 10000.0f) / 10000.0);

        w.settings["corners"] = list;
    }

    /** The box its outline lies in: inset by half its outline's width (as outline() has it). */
    juce::Rectangle<float> insetBox (const GuiWidget& w, float width, float height)
    {
        const float inset = juce::jmax (0.0f, w.strokeWidth * 0.5f);
        return juce::Rectangle<float> (0.0f, 0.0f, width, height).reduced (juce::jmin (inset, width * 0.25f), juce::jmin (inset, height * 0.25f));
    }

    /** Every point of the path taken from the rectangle `from` into the four-sided figure `to`
        (top-left, top-right, bottom-right, bottom-left): a perspective map, as Photoshop's Distort,
        so straight lines stay straight; a figure folded over itself is mapped bilinearly instead. */
    juce::Path warpInto (const juce::Path& path, juce::Rectangle<float> from, const std::array<juce::Point<float>, 4>& to)
    {
        if (from.getWidth() <= 0.0f || from.getHeight() <= 0.0f)
            return path;

        const auto x0 = (double) to[0].x, y0 = (double) to[0].y, x1 = (double) to[1].x, y1 = (double) to[1].y;
        const auto x2 = (double) to[2].x, y2 = (double) to[2].y, x3 = (double) to[3].x, y3 = (double) to[3].y;

        // The unit square onto the figure (Heckbert): x = (a u + b v + c) / (g u + h v + 1), ...
        double a = x1 - x0, b = x3 - x0, c = x0, d = y1 - y0, e = y3 - y0, f = y0, g = 0.0, h = 0.0;
        const double sx = x0 - x1 + x2 - x3, sy = y0 - y1 + y2 - y3;
        bool perspective = true;

        if (std::abs (sx) > 1.0e-9 || std::abs (sy) > 1.0e-9)
        {
            const double dx1 = x1 - x2, dx2 = x3 - x2, dy1 = y1 - y2, dy2 = y3 - y2;
            const double det = dx1 * dy2 - dx2 * dy1;

            if (std::abs (det) < 1.0e-9)
            {
                perspective = false;
            }
            else
            {
                g = (sx * dy2 - dx2 * sy) / det;
                h = (dx1 * sy - sx * dy1) / det;
                a = x1 - x0 + g * x1;  b = x3 - x0 + h * x3;
                d = y1 - y0 + g * y1;  e = y3 - y0 + h * y3;
            }

            // Its divisor must stay clear of zero over the square: it's linear, so its corners say.
            for (const auto& [u, v] : { std::pair<double, double> { 0.0, 0.0 }, { 1.0, 0.0 }, { 1.0, 1.0 }, { 0.0, 1.0 } })
                perspective = perspective && g * u + h * v + 1.0 > 0.05;
        }

        auto map = [&] (float px, float py)
        {
            const double u = (px - from.getX()) / from.getWidth(), v = (py - from.getY()) / from.getHeight();

            if (perspective)
            {
                const double w = g * u + h * v + 1.0;
                return juce::Point<float> ((float) ((a * u + b * v + c) / w), (float) ((d * u + e * v + f) / w));
            }

            const double x = (1 - u) * (1 - v) * x0 + u * (1 - v) * x1 + u * v * x2 + (1 - u) * v * x3;
            const double y = (1 - u) * (1 - v) * y0 + u * (1 - v) * y1 + u * v * y2 + (1 - u) * v * y3;
            return juce::Point<float> ((float) x, (float) y);
        };

        juce::Path out;
        juce::Path::Iterator it (path);

        while (it.next())
        {
            switch (it.elementType)
            {
                case juce::Path::Iterator::startNewSubPath: out.startNewSubPath (map (it.x1, it.y1)); break;
                case juce::Path::Iterator::lineTo:          out.lineTo (map (it.x1, it.y1)); break;
                case juce::Path::Iterator::quadraticTo:     out.quadraticTo (map (it.x1, it.y1), map (it.x2, it.y2)); break;
                case juce::Path::Iterator::cubicTo:         out.cubicTo (map (it.x1, it.y1), map (it.x2, it.y2), map (it.x3, it.y3)); break;
                case juce::Path::Iterator::closePath:       out.closeSubPath(); break;
                default: break;
            }
        }

        return out;
    }

    float smoothstep (float e0, float e1, float v) noexcept
    {
        const float t = clamp01 ((v - e0) / (e1 - e0));
        return t * t * (3.0f - 2.0f * t);
    }

    struct Segment { float x1, y1, x2, y2; };

    float segmentDistance (const Segment& s, float px, float py) noexcept
    {
        const float vx = s.x2 - s.x1, vy = s.y2 - s.y1, ux = px - s.x1, uy = py - s.y1;
        const float len = vx * vx + vy * vy;
        const float t = len > 0.0f ? clamp01 ((ux * vx + uy * vy) / len) : 0.0f;
        const float dx = ux - vx * t, dy = uy - vy * t;
        return std::sqrt (dx * dx + dy * dy);
    }

    //==========================================================================
    // Lines: a curve from the start to the end, through the bend point at its middle (a
    // quadratic curve, so it bulges most near the bend point), stroked into one outline with
    // its caps or arrowheads, so it fills, lights and casts its shadow like any shape.

    /** How far a line's points may lie from its box, in box sizes: while it's dragged its box
        keeps its place, so its points can go far outside it (a narrow box, a long way). */
    constexpr float lineReach = 10000.0f;

    /** A bend point closer than this (in pixels) to the straight way between the ends is straight. */
    constexpr float straightWithin = 3.0f;

    /** Its points as fractions of its box: start, its bend points, end (a straight line across
        the box's middle when it has none, or they're broken). Four numbers: its ends only, with
        one bend point halfway. */
    std::vector<float> lineFractions (const GuiWidget& w)
    {
        std::vector<float> f { 0.06f, 0.5f, 0.5f, 0.5f, 0.94f, 0.5f };
        const auto found = w.settings.find ("line");

        if (found == w.settings.end())
            return f;

        const auto* list = found->second.getArray();

        if (list == nullptr || list->size() < 4 || (list->size() % 2) != 0 || list->size() > 4 + ObjectPaint::maxBends * 2)
            return f;

        std::vector<float> read;

        for (const auto& v : *list)
        {
            const bool number = v.isInt() || v.isInt64() || v.isDouble() || v.isBool() || v.toString().containsOnly ("-0123456789.e");
            const auto value = (float) (double) v;

            if (! number || ! std::isfinite (value))
                return f;

            read.push_back (juce::jlimit (-lineReach, lineReach, value));
        }

        if (read.size() == 4)
            read.insert (read.begin() + 2, { (read[0] + read[2]) * 0.5f, (read[1] + read[3]) * 0.5f });

        return read;
    }

    /** Each bend point's kind: a corner, or an arc ("bendKinds": "arc" / "corner" each; left out: arcs). */
    std::vector<bool> lineCorners (const GuiWidget& w, size_t count)
    {
        std::vector<bool> corners (count, false);

        if (const auto found = w.settings.find ("bendKinds"); found != w.settings.end())
            if (const auto* list = found->second.getArray())
                for (size_t k = 0; k < count && (int) k < list->size(); ++k)
                    corners[k] = list->getReference ((int) k).toString().trim().equalsIgnoreCase ("corner");

        return corners;
    }

    float lineHalfWidth (const GuiWidget& w)
    {
        return juce::jlimit (0.25f, 60.0f, w.number ("lineWidth", 4.0f) * 0.5f);
    }

    juce::String lineArrows (const GuiWidget& w)
    {
        const auto a = settingText (w, "arrow").toLowerCase().removeCharacters (" -_");

        if (a == "start" || a == "begin" || a == "first" || a == "back")                    return "start";
        if (a == "end" || a == "last" || a == "yes" || a == "true" || a == "1" || a == "forward") return "end";
        if (a == "both" || a == "two" || a == "double")                                       return "both";
        return "none";
    }

    juce::String lineCap (const GuiWidget& w)
    {
        const auto c = settingText (w, "cap").toLowerCase();
        return c == "flat" || c == "butt" ? juce::String ("flat") : c == "square" ? juce::String ("square") : juce::String ("round");
    }

    /** The arrowheads' length and width for a line this wide. */
    float arrowLength (float half)    { return juce::jmax (7.0f, half * 6.0f); }
    float arrowWidth (float half)     { return juce::jmax (7.0f, half * 5.0f); }

    /** The curve through its points (a quadratic passing the bend point halfway), sampled. */
    std::vector<juce::Point<float>> lineSamples (juce::Point<float> a, juce::Point<float> m, juce::Point<float> b)
    {
        const auto control = m * 2.0f - (a + b) * 0.5f;
        const float roughLength = a.getDistanceFrom (control) + control.getDistanceFrom (b);
        const int count = juce::jlimit (2, 240, (int) (roughLength / 2.5f) + 2);
        // Straight while the bend point lies within a few pixels of the way between the ends (as
        // snapStraight snaps it), however long the line.
        const bool straight = std::abs ((b.x - a.x) * (m.y - a.y) - (b.y - a.y) * (m.x - a.x)) < straightWithin * juce::jmax (1.0f, a.getDistanceFrom (b));
        std::vector<juce::Point<float>> points;

        if (straight)
        {
            points.push_back (a);
            points.push_back (b);
            return points;
        }

        for (int i = 0; i < count; ++i)
        {
            const float t = (float) i / (float) (count - 1);
            points.push_back (a * ((1.0f - t) * (1.0f - t)) + control * (2.0f * t * (1.0f - t)) + b * (t * t));
        }

        return points;
    }

    /** The line through all its points, sampled. One arc bend point: the quadratic through it
        (as lines always were). Otherwise piece by piece between neighbouring points, each a cubic
        whose ends leave and arrive along the line through an arc's neighbours (smooth through
        it), or straight at a corner and at the line's ends: two corners side by side make a
        straight piece. */
    std::vector<juce::Point<float>> curveOf (const ObjectPaint::LinePoints& p)
    {
        if (p.bends.size() == 1 && (p.corners.empty() || ! p.corners.front()))
            return lineSamples (p.start, p.bends.front(), p.end);

        std::vector<juce::Point<float>> q { p.start };
        std::vector<bool> sharp { true };

        for (size_t k = 0; k < p.bends.size(); ++k)
        {
            q.push_back (p.bends[k]);
            sharp.push_back (k < p.corners.size() && p.corners[k]);
        }

        q.push_back (p.end);
        sharp.push_back (true);

        // An arc's way through: along the line from the point before it to the point after it.
        auto through = [&] (size_t i)
        {
            const auto d = q[i + 1] - q[i - 1];
            const float len = d.getDistanceFromOrigin();
            return len > 1.0e-4f ? d / len : juce::Point<float>();
        };

        std::vector<juce::Point<float>> points;

        for (size_t i = 0; i + 1 < q.size(); ++i)
        {
            const auto a = q[i], b = q[i + 1];
            const float span = a.getDistanceFrom (b);
            const auto out = sharp[i] ? (b - a) / 3.0f : through (i) * (span / 3.0f);
            const auto in = sharp[i + 1] ? (b - a) / 3.0f : through (i + 1) * (span / 3.0f);

            points.push_back (a);

            if (sharp[i] && sharp[i + 1])
                continue;   // straight

            const auto c1 = a + out, c2 = b - in;
            const float rough = span + c1.getDistanceFrom (a) + c2.getDistanceFrom (b);
            const int count = juce::jlimit (2, 240, (int) (rough / 2.5f) + 2);

            for (int s = 1; s < count - 1; ++s)
            {
                const float t = (float) s / (float) (count - 1), u = 1.0f - t;
                points.push_back (a * (u * u * u) + c1 * (3.0f * u * u * t) + c2 * (3.0f * u * t * t) + b * (t * t * t));
            }
        }

        points.push_back (q.back());

        // Points on top of each other (a bend point dropped on its neighbour) would have no way.
        std::vector<juce::Point<float>> kept;

        for (const auto& pt : points)
            if (kept.empty() || kept.back().getDistanceFrom (pt) > 1.0e-3f)
                kept.push_back (pt);

        if (kept.size() < 2)
            kept.push_back (kept.front());

        return kept;
    }

    /** The samples cut back by `length` from one end (the start when fromStart). */
    void cutBack (std::vector<juce::Point<float>>& points, float length, bool fromStart)
    {
        if (points.size() < 2 || length <= 0.0f)
            return;

        if (fromStart)
            std::reverse (points.begin(), points.end());

        float left = length;

        while (points.size() >= 2)
        {
            const auto last = points.back(), before = points[points.size() - 2];
            const float step = last.getDistanceFrom (before);

            if (step > left)
            {
                points.back() = last + (before - last) * (left / juce::jmax (1.0e-4f, step));
                break;
            }

            left -= step;
            points.pop_back();

            if (points.size() < 2)
            {
                points.push_back (before);
                break;
            }
        }

        if (fromStart)
            std::reverse (points.begin(), points.end());
    }

    /** One end's direction, pointing out of the line. */
    juce::Point<float> endDirection (const std::vector<juce::Point<float>>& points, bool atStart)
    {
        const auto tip = atStart ? points.front() : points.back();

        for (size_t k = 1; k < points.size(); ++k)
        {
            const auto other = atStart ? points[k] : points[points.size() - 1 - k];
            const auto d = tip - other;
            const float len = d.getDistanceFromOrigin();

            if (len > 1.0e-3f)
                return d / len;
        }

        return { 1.0f, 0.0f };
    }

    /** The whole stroke as one closed outline: down one side, round the end (its cap or its
        arrowhead), back up the other side, round the start. One outline, so it has no seams. */
    juce::Path lineOutline (const GuiWidget& w, float width, float height)
    {
        const auto f = lineFractions (w);
        ObjectPaint::LinePoints points;
        points.start = { f[0] * width, f[1] * height };
        points.end = { f[f.size() - 2] * width, f[f.size() - 1] * height };

        for (size_t k = 2; k + 2 < f.size(); k += 2)
            points.bends.push_back ({ f[k] * width, f[k + 1] * height });

        points.corners = lineCorners (w, points.bends.size());

        const float half = lineHalfWidth (w);
        const auto arrows = lineArrows (w);
        const auto cap = lineCap (w);
        const bool arrowStart = arrows == "start" || arrows == "both", arrowEnd = arrows == "end" || arrows == "both";

        auto samples = curveOf (points);
        juce::Path path;

        float spread = 0.0f;

        for (const auto& s : samples)
            spread = juce::jmax (spread, s.getDistanceFrom (points.start));

        if (spread < 0.5f)
        {
            path.addEllipse (juce::Rectangle<float> (half * 2.0f, half * 2.0f).withCentre (points.start));
            return path;
        }

        // Room for the arrowheads: the line stops where they start (never more than most of it).
        float total = 0.0f;

        for (size_t k = 1; k < samples.size(); ++k)
            total += samples[k].getDistanceFrom (samples[k - 1]);

        const float headLength = juce::jmin (arrowLength (half), total * (arrows == "both" ? 0.42f : 0.8f));
        const auto startTip = samples.front(), endTip = samples.back();
        const auto startOut = endDirection (samples, true), endOut = endDirection (samples, false);

        if (arrowEnd)   cutBack (samples, headLength * 0.98f, false);
        if (arrowStart) cutBack (samples, headLength * 0.98f, true);

        // Each sample's normal (to the left going start to end), from its neighbours: at a corner
        // the two ways' normals meet halfway, lengthened so the stroke keeps its width round it
        // (a mitre, held in at the sharpest).
        std::vector<juce::Point<float>> normals (samples.size());

        auto unitNormal = [] (juce::Point<float> from, juce::Point<float> to, juce::Point<float>& n)
        {
            const auto t = to - from;
            const float len = t.getDistanceFromOrigin();

            if (len <= 1.0e-5f)
                return false;

            n = { t.y / len, -t.x / len };
            return true;
        };

        for (size_t k = 0; k < samples.size(); ++k)
        {
            juce::Point<float> n1, n2;
            const bool hasIn = k > 0 && unitNormal (samples[k - 1], samples[k], n1);
            const bool hasOut = k + 1 < samples.size() && unitNormal (samples[k], samples[k + 1], n2);

            if (hasIn && hasOut)
            {
                auto n = n1 + n2;
                const float len = n.getDistanceFromOrigin();

                if (len > 1.0e-4f)
                {
                    n /= len;
                    normals[k] = n / juce::jmax (0.35f, n.x * n1.x + n.y * n1.y);
                }
                else
                {
                    normals[k] = n1;   // turned right back on itself
                }
            }
            else if (hasIn || hasOut)
            {
                normals[k] = hasIn ? n1 : n2;
            }
            else
            {
                normals[k] = k > 0 ? normals[k - 1] : juce::Point<float> (0.0f, -1.0f);
            }
        }

        auto capRound = [&] (juce::Point<float> centre, juce::Point<float> from, float sweep)
        {
            // Half a circle round an end: from one side, through the way out, to the other.
            constexpr int steps = 12;
            const float start = std::atan2 (from.y, from.x);

            for (int s = 1; s < steps; ++s)
            {
                const float angle = start + sweep * (float) s / (float) steps;
                path.lineTo (centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * half);
            }
        };

        auto endCap = [&] (bool atStart)
        {
            const auto centre = atStart ? samples.front() : samples.back();
            const auto out = atStart ? startOut : endOut;
            auto n = atStart ? normals.front() : normals.back();
            n /= juce::jmax (1.0e-4f, n.getDistanceFromOrigin());   // an end's own: no mitre
            // Going round the end from the side we came along to the other side.
            const auto fromSide = atStart ? -n : n;
            const auto toSide = -fromSide;
            const bool arrow = atStart ? arrowStart : arrowEnd;

            if (arrow)
            {
                const float wide = juce::jmax (arrowWidth (half), half * 2.0f + 2.0f) * 0.5f;
                path.lineTo (centre + fromSide * wide);
                path.lineTo (atStart ? startTip : endTip);
                path.lineTo (centre + toSide * wide);
            }
            else if (cap == "round")
            {
                // Which way round: through the way out.
                const float cross = fromSide.x * out.y - fromSide.y * out.x;
                capRound (centre, fromSide, cross > 0.0f ? juce::MathConstants<float>::pi : -juce::MathConstants<float>::pi);
            }
            else if (cap == "square")
            {
                path.lineTo (centre + fromSide * half + out * half);
                path.lineTo (centre + toSide * half + out * half);
            }

            path.lineTo (centre + toSide * half);
        };

        path.startNewSubPath (samples.front() + normals.front() * half);

        for (size_t k = 1; k < samples.size(); ++k)
            path.lineTo (samples[k] + normals[k] * half);

        endCap (false);

        for (size_t k = samples.size() - 1; k-- > 0;)
            path.lineTo (samples[k] - normals[k] * half);

        endCap (true);
        path.closeSubPath();
        return path;
    }
}

//==============================================================================
juce::Path ObjectPaint::outline (const GuiWidget& w, float width, float height)
{
    const auto kind = w.shapeKind();

    // A line is its stroke: its points are where they are, past the box's edges if they go there.
    if (kind == "line")
        return lineOutline (w, width, height);

    const auto box = insetBox (w, width, height);

    // Per corner, its lean is in its corners.
    CornerFractions corners {};
    const bool perCorner = readCorners (w, corners);

    auto path = lopsided (unitShape (w, kind, box.getWidth(), box.getHeight()),
                          kind == "rectangle" ? 0.0f : juce::jlimit (-0.95f, 0.95f, w.number ("centerX", 0.0f)),
                          kind == "rectangle" || kind == "triangle" ? 0.0f : juce::jlimit (-0.95f, 0.95f, w.number ("centerY", 0.0f)),
                          perCorner ? 0.0f : juce::jlimit (-1.0f, 1.0f, w.number ("leanX", 0.0f)),
                          perCorner ? 0.0f : juce::jlimit (-1.0f, 1.0f, w.number ("leanY", 0.0f)));

    // Stretched to fill its box (a star in a wide box is a wide star).
    if (const auto b = path.getBounds(); b.getWidth() > 0.0f && b.getHeight() > 0.0f && ! box.isEmpty())
        path.applyTransform (path.getTransformToScaleToFit (box, false));

    // A triangle's or a star's corners rounded, in pixels once it's stretched: at 1 each side
    // curves all the way (a star turns into a flower).
    if (kind == "triangle" || kind == "star")
    {
        if (const auto rounding = roundingOf (w, box.getWidth(), box.getHeight()); rounding > 0.0f)
        {
            float longest = 0.0f;
            juce::Point<float> last, start;
            juce::Path::Iterator it (path);

            while (it.next())
            {
                const juce::Point<float> p (it.x1, it.y1);

                if (it.elementType == juce::Path::Iterator::startNewSubPath)
                    start = p;
                else if (it.elementType == juce::Path::Iterator::lineTo)
                    longest = juce::jmax (longest, last.getDistanceFrom (p));
                else if (it.elementType == juce::Path::Iterator::closePath)
                    longest = juce::jmax (longest, last.getDistanceFrom (start));

                if (it.elementType != juce::Path::Iterator::closePath)
                    last = p;
            }

            path = path.createPathWithRoundedCorners (rounding * longest * 0.5f);
        }
    }

    // Per corner: the whole of it taken into the figure its corners make.
    if (perCorner)
    {
        std::array<juce::Point<float>, 4> to;

        for (size_t k = 0; k < 4; ++k)
            to[k] = { box.getX() + corners[k * 2] * box.getWidth(), box.getY() + corners[k * 2 + 1] * box.getHeight() };

        path = warpInto (path, box, to);
    }

    return path;
}

//==============================================================================
bool ObjectPaint::hasCorners (const GuiWidget& w)
{
    CornerFractions f {};
    return w.type == GuiWidget::Type::shape && readCorners (w, f);
}

std::array<juce::Point<float>, 4> ObjectPaint::cornerPoints (const GuiWidget& w)
{
    CornerFractions f = boxCorners;
    readCorners (w, f);

    const auto box = insetBox (w, (float) w.bounds.getWidth(), (float) w.bounds.getHeight());
    std::array<juce::Point<float>, 4> points;

    for (size_t k = 0; k < 4; ++k)
        points[k] = { box.getX() + f[k * 2] * box.getWidth(), box.getY() + f[k * 2 + 1] * box.getHeight() };

    return points;
}

void ObjectPaint::startCorners (GuiWidget& w)
{
    if (w.type != GuiWidget::Type::shape || w.shapeKind() == "line" || hasCorners (w))
        return;

    // Its lean, as outline() draws it: the unleant shape's bounds, leant, then stretched back
    // into its box. Its box's corners go where that takes them, so it looks just as it did.
    const auto kind = w.shapeKind();
    const float width = (float) juce::jmax (1, w.bounds.getWidth()), height = (float) juce::jmax (1, w.bounds.getHeight());
    const auto box = insetBox (w, width, height);
    const float leanX = juce::jlimit (-1.0f, 1.0f, w.number ("leanX", 0.0f)), leanY = juce::jlimit (-1.0f, 1.0f, w.number ("leanY", 0.0f));
    auto fractions = boxCorners;

    if ((leanX != 0.0f || leanY != 0.0f) && ! box.isEmpty())
    {
        const auto cx = kind == "rectangle" ? 0.0f : juce::jlimit (-0.95f, 0.95f, w.number ("centerX", 0.0f));
        const auto cy = kind == "rectangle" || kind == "triangle" ? 0.0f : juce::jlimit (-0.95f, 0.95f, w.number ("centerY", 0.0f));
        const auto unit = unitShape (w, kind, box.getWidth(), box.getHeight());
        const auto upright = lopsided (unit, cx, cy, 0.0f, 0.0f).getBounds();
        const auto leant = lopsided (unit, cx, cy, leanX, leanY).getBounds();

        if (upright.getWidth() > 0.0f && upright.getHeight() > 0.0f && leant.getWidth() > 0.0f && leant.getHeight() > 0.0f)
        {
            const juce::Point<float> from[] { upright.getTopLeft(), upright.getTopRight(), upright.getBottomRight(), upright.getBottomLeft() };

            for (size_t k = 0; k < 4; ++k)
            {
                const auto p = from[k];
                const juce::Point<float> sheared (p.x - leanX * p.y * 0.5f, p.y - leanY * p.x * 0.5f);
                fractions[k * 2]     = (sheared.x - leant.getX()) / leant.getWidth();
                fractions[k * 2 + 1] = (sheared.y - leant.getY()) / leant.getHeight();
            }
        }
    }

    writeCorners (w, fractions);
    w.settings.erase ("leanX");
    w.settings.erase ("leanY");
}

void ObjectPaint::stopCorners (GuiWidget& w)
{
    w.settings.erase ("corners");
}

void ObjectPaint::moveCorner (GuiWidget& w, const GuiWidget& atStart, int corner, juce::Point<float> local)
{
    if (! juce::isPositiveAndBelow (corner, 4) || ! hasCorners (atStart))
        return;

    CornerFractions f {};
    readCorners (atStart, f);

    const auto box = insetBox (atStart, (float) atStart.bounds.getWidth(), (float) atStart.bounds.getHeight());

    if (box.getWidth() <= 0.0f || box.getHeight() <= 0.0f)
        return;

    // Upright, past its box too (the box fits it again when the drag ends); turned, inside it.
    const float low = atStart.isTurned() ? 0.0f : -3.0f, high = atStart.isTurned() ? 1.0f : 4.0f;
    f[(size_t) corner * 2]     = juce::jlimit (low, high, (local.x - box.getX()) / box.getWidth());
    f[(size_t) corner * 2 + 1] = juce::jlimit (low, high, (local.y - box.getY()) / box.getHeight());

    w.bounds = atStart.bounds;
    writeCorners (w, f);
}

void ObjectPaint::fitCornersBox (GuiWidget& w)
{
    if (! hasCorners (w) || w.isTurned())
        return;

    // Where its corners lie on the panel.
    const auto points = cornerPoints (w);
    const auto origin = w.bounds.getPosition().toFloat();
    auto low = points[0], high = points[0];

    for (const auto& p : points)
    {
        low = { juce::jmin (low.x, p.x), juce::jmin (low.y, p.y) };
        high = { juce::jmax (high.x, p.x), juce::jmax (high.y, p.y) };
    }

    // The box round them, its outline's inset outside them; then they're fractions of it again.
    const float inset = juce::jmax (0.0f, w.strokeWidth * 0.5f);
    auto fitted = juce::Rectangle<float> (low, high).translated (origin.x, origin.y).expanded (inset).getSmallestIntegerContainer();
    fitted = fitted.withSizeKeepingCentre (juce::jmax (8, fitted.getWidth()), juce::jmax (8, fitted.getHeight()));

    if (fitted == w.bounds)
        return;

    w.bounds = fitted;
    const auto box = insetBox (w, (float) fitted.getWidth(), (float) fitted.getHeight());
    const auto shift = origin - fitted.getPosition().toFloat();
    CornerFractions f {};

    for (size_t k = 0; k < 4; ++k)
    {
        const auto p = points[k] + shift;
        f[k * 2]     = (p.x - box.getX()) / juce::jmax (1.0f, box.getWidth());
        f[k * 2 + 1] = (p.y - box.getY()) / juce::jmax (1.0f, box.getHeight());
    }

    writeCorners (w, f);
}

ObjectPaint::LinePoints ObjectPaint::linePoints (const GuiWidget& w)
{
    const auto f = lineFractions (w);
    const float bw = (float) w.bounds.getWidth(), bh = (float) w.bounds.getHeight();
    LinePoints p;
    p.start = { f[0] * bw, f[1] * bh };
    p.end = { f[f.size() - 2] * bw, f[f.size() - 1] * bh };

    for (size_t k = 2; k + 2 < f.size(); k += 2)
        p.bends.push_back ({ f[k] * bw, f[k + 1] * bh });

    p.corners = lineCorners (w, p.bends.size());
    return p;
}

void ObjectPaint::setLinePoints (GuiWidget& w, LinePoints p, bool fitBox)
{
    if (p.bends.empty())
        p.bends.push_back ((p.start + p.end) * 0.5f);

    if ((int) p.bends.size() > maxBends)
        p.bends.resize ((size_t) maxBends);

    p.corners.resize (p.bends.size(), false);
    auto box = w.bounds.toFloat();

    if (fitBox && ! w.isTurned())
    {
        // Snug round the curve and its stroke (arrowheads included), with a little room to pick it
        // by; its bend points inside too, so their handles are on it.
        // (By hand: a rectangle's union skips empty ones, and a point's is empty.)
        auto samples = curveOf (p);
        samples.insert (samples.end(), p.bends.begin(), p.bends.end());
        auto low = samples.front(), high = samples.front();

        for (const auto& s : samples)
        {
            low = { juce::jmin (low.x, s.x), juce::jmin (low.y, s.y) };
            high = { juce::jmax (high.x, s.x), juce::jmax (high.y, s.y) };
        }

        const juce::Rectangle<float> covered (low, high);

        const float half = lineHalfWidth (w);
        const float room = (lineArrows (w) != "none" ? arrowWidth (half) * 0.5f : half) + 6.0f;
        const auto fitted = covered.translated (box.getX(), box.getY()).expanded (room).getSmallestIntegerContainer();
        const auto oldOrigin = box.getPosition();

        box = fitted.withSizeKeepingCentre (juce::jmax (12, fitted.getWidth()), juce::jmax (12, fitted.getHeight())).toFloat();
        w.bounds = box.toNearestInt();
        box = w.bounds.toFloat();

        const auto shift = oldOrigin - box.getPosition();
        p.start += shift;
        p.end += shift;

        for (auto& b : p.bends)
            b += shift;
    }

    const float bw = juce::jmax (1.0f, box.getWidth()), bh = juce::jmax (1.0f, box.getHeight());
    auto fraction = [] (float v) { return juce::roundToInt (juce::jlimit (-lineReach, lineReach, std::isfinite (v) ? v : 0.5f) * 10000.0f) / 10000.0; };

    juce::Array<juce::var> list { fraction (p.start.x / bw), fraction (p.start.y / bh) };

    for (const auto& b : p.bends)
    {
        list.add (fraction (b.x / bw));
        list.add (fraction (b.y / bh));
    }

    list.add (fraction (p.end.x / bw));
    list.add (fraction (p.end.y / bh));
    w.settings["line"] = list;

    // Their kinds: only when there's a corner (arcs are what's left out).
    if (std::find (p.corners.begin(), p.corners.end(), true) == p.corners.end())
    {
        w.settings.erase ("bendKinds");
    }
    else
    {
        juce::Array<juce::var> kinds;

        for (const auto corner : p.corners)
            kinds.add (corner ? "corner" : "arc");

        w.settings["bendKinds"] = kinds;
    }
}

void ObjectPaint::straighten (GuiWidget& w)
{
    auto p = linePoints (w);
    const auto n = (float) p.bends.size();

    for (size_t k = 0; k < p.bends.size(); ++k)
        p.bends[k] = p.start + (p.end - p.start) * ((float) (k + 1) / (n + 1.0f));

    setLinePoints (w, p, true);
}

int ObjectPaint::bendCount (const GuiWidget& w)
{
    return (int) linePoints (w).bends.size();
}

bool ObjectPaint::isCornerBend (const GuiWidget& w, int bend)
{
    const auto p = linePoints (w);
    return juce::isPositiveAndBelow (bend, (int) p.corners.size()) && p.corners[(size_t) bend];
}

void ObjectPaint::setBendCorner (GuiWidget& w, int bend, bool corner)
{
    auto p = linePoints (w);

    if (! juce::isPositiveAndBelow (bend, (int) p.bends.size()))
        return;

    p.corners[(size_t) bend] = corner;
    setLinePoints (w, p, false);
}

int ObjectPaint::addBend (GuiWidget& w, int after)
{
    auto p = linePoints (w);

    if ((int) p.bends.size() >= maxBends)
        return -1;

    // The points in order; the part to split: the one after `after`, or the longest.
    std::vector<juce::Point<float>> q { p.start };
    q.insert (q.end(), p.bends.begin(), p.bends.end());
    q.push_back (p.end);

    int part = after + 1;   // part i runs from q[i] to q[i + 1]

    if (! juce::isPositiveAndBelow (part, (int) q.size() - 1))
    {
        part = 0;

        for (int i = 1; i + 1 < (int) q.size(); ++i)
            if (q[(size_t) i].getDistanceFrom (q[(size_t) i + 1]) > q[(size_t) part].getDistanceFrom (q[(size_t) part + 1]))
                part = i;
    }

    // Halfway along that part of the curve as it's drawn now, so the line keeps its look.
    const auto samples = curveOf (p);
    const auto a = q[(size_t) part], b = q[(size_t) part + 1];
    auto nearestTo = [&samples] (juce::Point<float> target)
    {
        size_t best = 0;

        for (size_t s = 1; s < samples.size(); ++s)
            if (samples[s].getDistanceSquaredFrom (target) < samples[best].getDistanceSquaredFrom (target))
                best = s;

        return best;
    };

    const auto from = nearestTo (a), to = nearestTo (b);
    auto at = (a + b) * 0.5f;

    if (to > from + 1)
    {
        // Halfway by length.
        float total = 0.0f;

        for (size_t s = from + 1; s <= to; ++s)
            total += samples[s].getDistanceFrom (samples[s - 1]);

        float walked = 0.0f;

        for (size_t s = from + 1; s <= to; ++s)
        {
            const float step = samples[s].getDistanceFrom (samples[s - 1]);

            if (walked + step >= total * 0.5f && step > 0.0f)
            {
                at = samples[s - 1] + (samples[s] - samples[s - 1]) * ((total * 0.5f - walked) / step);
                break;
            }

            walked += step;
        }
    }

    p.bends.insert (p.bends.begin() + part, at);
    p.corners.insert (p.corners.begin() + part, false);
    setLinePoints (w, p, false);
    return part;
}

void ObjectPaint::removeBend (GuiWidget& w, int bend)
{
    auto p = linePoints (w);

    if (p.bends.size() <= 1 || ! juce::isPositiveAndBelow (bend, (int) p.bends.size()))
        return;

    p.bends.erase (p.bends.begin() + bend);
    p.corners.erase (p.corners.begin() + bend);
    setLinePoints (w, p, true);
}

bool ObjectPaint::canPull (const GuiWidget& w)
{
    const auto p = linePoints (w);
    return p.bends.size() == 1 && ! p.corners.front();
}

namespace
{
    /** One arc bend point snapped onto the straight way between the ends when it's within a few pixels of it. */
    void snapStraight (ObjectPaint::LinePoints& p)
    {
        if (p.bends.size() != 1 || (! p.corners.empty() && p.corners.front()))
            return;

        const auto chord = p.end - p.start;
        const float length = chord.getDistanceFromOrigin();

        if (length <= 1.0f)
            return;

        auto& bend = p.bends.front();
        const auto off = bend - p.start;
        const float across = std::abs (chord.x * off.y - chord.y * off.x) / length;

        if (across < straightWithin)
            bend = p.start + chord * ((off.x * chord.x + off.y * chord.y) / (length * length));
    }

    juce::Point<float> quadraticAt (juce::Point<float> a, juce::Point<float> control, juce::Point<float> b, float t)
    {
        return a * ((1.0f - t) * (1.0f - t)) + control * (2.0f * t * (1.0f - t)) + b * (t * t);
    }
}

void ObjectPaint::moveBend (GuiWidget& w, const GuiWidget& atStart, int bend, juce::Point<float> local)
{
    auto p = linePoints (atStart);

    if (! juce::isPositiveAndBelow (bend, (int) p.bends.size()))
        return;

    // The box keeps its place until the drag ends (the canvas fits it then), so the line stays
    // put under the mouse.
    p.bends[(size_t) bend] = local;
    snapStraight (p);   // one arc, near the straight way between the ends: it snaps straight
    w.bounds = atStart.bounds;
    setLinePoints (w, p, false);
}

float ObjectPaint::lineParameterAt (const GuiWidget& w, juce::Point<float> local)
{
    const auto p = linePoints (w);
    const auto control = p.bends.front() * 2.0f - (p.start + p.end) * 0.5f;
    float best = 0.5f, nearest = std::numeric_limits<float>::max();

    for (int i = 0; i <= 200; ++i)
    {
        const float t = (float) i / 200.0f;
        const float d = quadraticAt (p.start, control, p.end, t).getDistanceSquaredFrom (local);

        if (d < nearest)
        {
            nearest = d;
            best = t;
        }
    }

    return best;
}

void ObjectPaint::pullLine (GuiWidget& w, const GuiWidget& atStart, float along, juce::Point<float> local)
{
    // One arc bend point: the curve passes `local` at `along`: solve for its control point, then
    // its bend point (the curve's middle). Too near an end, it would fly off: held a little way in.
    auto p = linePoints (atStart);

    if (p.bends.size() != 1)
        return;

    const float t = juce::jlimit (0.12f, 0.88f, along);
    const auto control = (local - p.start * ((1.0f - t) * (1.0f - t)) - p.end * (t * t)) / (2.0f * t * (1.0f - t));
    p.bends.front() = (control + (p.start + p.end) * 0.5f) * 0.5f;
    snapStraight (p);
    setLinePoints (w, p, false);
}

void ObjectPaint::moveLineEnd (GuiWidget& w, const GuiWidget& atStart, bool start, juce::Point<float> local)
{
    const auto before = linePoints (atStart);
    auto p = before;
    (start ? p.start : p.end) = local;

    // The bend points follow the ends as a whole: turned and stretched with the line (as complex
    // numbers, the old ends mapped onto the new ones).
    const auto oldChord = before.end - before.start, newChord = p.end - p.start;
    const float oldLength = oldChord.x * oldChord.x + oldChord.y * oldChord.y;

    for (size_t k = 0; k < p.bends.size(); ++k)
    {
        if (oldLength > 1.0e-3f)
        {
            const auto rel = before.bends[k] - before.start;
            const juce::Point<float> ratio ((newChord.x * oldChord.x + newChord.y * oldChord.y) / oldLength,
                                            (newChord.y * oldChord.x - newChord.x * oldChord.y) / oldLength);
            p.bends[k] = p.start + juce::Point<float> (rel.x * ratio.x - rel.y * ratio.y, rel.x * ratio.y + rel.y * ratio.x);
        }
        else
        {
            p.bends[k] = p.start + (p.end - p.start) * ((float) (k + 1) / (float) (p.bends.size() + 1));
        }
    }

    setLinePoints (w, p, false);
}

bool ObjectPaint::isBent (const GuiWidget& w)
{
    const auto p = linePoints (w);
    const auto chord = p.end - p.start;
    const float length = chord.getDistanceFromOrigin();

    for (const auto& b : p.bends)
    {
        const auto off = b - p.start;
        const float across = length > 1.0f ? std::abs (chord.x * off.y - chord.y * off.x) / length : off.getDistanceFromOrigin();

        if (across >= straightWithin)
            return true;
    }

    return false;
}

float ObjectPaint::lineWidthOf (const GuiWidget& w)
{
    return lineHalfWidth (w) * 2.0f;
}

juce::Point<float> ObjectPaint::adjustPoint (const GuiWidget& w)
{
    const auto kind = w.shapeKind();
    const float bw = (float) w.bounds.getWidth(), bh = (float) w.bounds.getHeight();

    if (kind == "line")
        return linePoints (w).bends.front();

    if (kind == "rectangle")
    {
        const float r = roundingOf (w, bw, bh) * juce::jmin (bw, bh) * 0.5f;
        return { r, r };
    }

    const float x = (juce::jlimit (-0.95f, 0.95f, w.number ("centerX", 0.0f)) + 1.0f) * 0.5f * bw;

    if (kind == "triangle")
        return { x, 0.0f };

    return { x, (juce::jlimit (-0.95f, 0.95f, w.number ("centerY", 0.0f)) + 1.0f) * 0.5f * bh };
}

void ObjectPaint::adjust (GuiWidget& w, const GuiWidget& atStart, juce::Point<float> moved)
{
    const auto kind = w.shapeKind();
    const float bw = juce::jmax (1.0f, (float) w.bounds.getWidth()), bh = juce::jmax (1.0f, (float) w.bounds.getHeight());
    auto rounded = [] (float v, float step) { return juce::roundToInt (v / step) * (double) step; };

    if (kind == "line")
    {
        // The bend point follows the mouse; the box keeps its place until the drag ends (the
        // canvas fits it then), so the line stays put under the mouse.
        auto p = linePoints (atStart);
        p.bends.front() += moved;
        snapStraight (p);   // near the straight way between the ends, it snaps straight
        setLinePoints (w, p, false);
        return;
    }

    if (kind == "rectangle")
    {
        // Along the diagonal: further in rounds the corners more, all the way to a pill.
        const float half = juce::jmax (1.0f, juce::jmin (bw, bh) * 0.5f);
        const float r = roundingOf (atStart, bw, bh) * half + (moved.x + moved.y) * 0.5f;
        w.settings.erase ("radius");
        w.settings["rounding"] = rounded (juce::jlimit (0.0f, 1.0f, r / half), 0.01f);
        return;
    }

    w.settings["centerX"] = rounded (juce::jlimit (-0.95f, 0.95f, atStart.number ("centerX", 0.0f) + moved.x * 2.0f / bw), 0.01f);

    if (kind != "triangle")
        w.settings["centerY"] = rounded (juce::jlimit (-0.95f, 0.95f, atStart.number ("centerY", 0.0f) + moved.y * 2.0f / bh), 0.01f);
}

LayerStyle ObjectPaint::styleOf (const GuiWidget& w)
{
    if (const auto found = w.settings.find ("effects"); found != w.settings.end())
        return LayerStyle::fromVar (found->second);

    // From before layer styles: its relief, the shadow it casts and its outline, as the layer
    // style that looks the same, lit by the global light.
    auto style = LayerStyle::fromVar ({});
    const auto depth = juce::jlimit (-40.0f, 40.0f, w.number ("depth", 0.0f));
    const auto shadow = juce::jlimit (0.0f, 1.0f, w.number ("shadow", 0.0f));

    if (depth != 0.0f)
    {
        style.bevel.enabled = true;
        style.bevel.up = depth > 0.0f;
        style.bevel.size = std::fabs (depth);
    }

    if (shadow > 0.0f && depth >= 0.0f)
    {
        auto& d = style.dropShadow;
        d.enabled = true;
        d.opacity = std::round (shadow * 65.0f);
        d.distance = std::round (2.0f + std::fabs (depth) * 1.1f + shadow * 4.0f);
        d.size = std::round (3.0f + std::fabs (depth) + shadow * 6.0f);
    }

    if (w.strokeWidth > 0.0f)
    {
        // Its outline lay inside its box: the same, drawn inside its edge.
        style.stroke.enabled = true;
        style.stroke.size = w.strokeWidth;
        style.stroke.position = StrokePosition::Inside;
        style.stroke.colour = w.stroke.isTransparent() ? juce::Colours::black : w.stroke;
    }

    return style;
}

void ObjectPaint::setStyle (GuiWidget& w, const LayerStyle& style)
{
    // Its older relief, shadow and outline are the style's now, and a light of its own gives
    // way to the global light. An effect switched off keeps its settings for next time; with
    // nothing set at all, there's nothing to keep.
    for (const auto* key : { "depth", "shadow", "light", "altitude" })
        w.settings.erase (key);

    w.strokeWidth = 0.0f;

    const auto effects = style.toVar();

    if (const auto* o = effects.getDynamicObject(); o != nullptr && ! o->getProperties().isEmpty())
        w.settings["effects"] = effects;
    else
        w.settings.erase ("effects");
}

bool ObjectPaint::isPainted (const GuiWidget& w)
{
    if (materialOf (w).isNotEmpty() || w.number ("depth", 0.0f) != 0.0f || w.number ("shadow", 0.0f) > 0.0f)
        return true;

    if (w.settings.count ("effects") > 0 && styleOf (w).any())
        return true;

    namespace p = stella::paint;

    if (const auto name = settingText (w, "pattern"); name.isNotEmpty() && p::patternNamed (name.toStdString()) != p::Pattern::none)
        return true;

    if (const auto found = w.settings.find ("patterns"); found != w.settings.end())
        if (const auto* list = found->second.getArray())
            for (const auto& layer : *list)
                if (p::patternNamed (layer.getProperty ("pattern", layer.getProperty ("name", {})).toString().toStdString()) != p::Pattern::none)
                    return true;

    return false;
}

void ObjectPaint::drawFlat (juce::Graphics& g, const GuiWidget& w, juce::Colour fallback)
{
    const auto b = w.bounds.toFloat();

    if (b.isEmpty())
        return;

    auto path = outline (w, b.getWidth(), b.getHeight());
    path.applyTransform (juce::AffineTransform::translation (b.getX(), b.getY()));

    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (w.turn());

    const auto main = w.colour.isTransparent() ? fallback : w.colour;
    const auto second = colourSetting (w, "color2");

    if (! second.isTransparent() && second != main)
    {
        // Two colours: its own towards the upper left, the second towards the lower right (as
        // the materials shade them).
        const float hw = juce::jmax (1.0f, b.getWidth() * 0.5f), hh = juce::jmax (1.0f, b.getHeight() * 0.5f);
        const juce::Point<float> slope (0.5f / hw / 1.366f, 0.866f / hh / 1.366f);
        const auto reach = slope / (slope.x * slope.x + slope.y * slope.y);
        g.setGradientFill (juce::ColourGradient (main, b.getCentre() - reach, second, b.getCentre() + reach, false));
    }
    else
    {
        g.setColour (main);
    }

    g.fillPath (path);

    if (w.strokeWidth > 0.0f)
    {
        g.setColour (w.stroke.isTransparent() ? juce::Colours::black : w.stroke);
        g.strokePath (path, juce::PathStrokeType (w.strokeWidth, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    }
}

//==============================================================================
ObjectPaint::Job ObjectPaint::jobFor (const GuiWidget& w, juce::Colour fallback, float globalAngle, float globalAltitude)
{
    Job job;
    job.width = juce::jmax (1, w.bounds.getWidth());
    job.height = juce::jmax (1, w.bounds.getHeight());
    job.angle = w.shownAngle();
    job.outline = outline (w, (float) job.width, (float) job.height);

    const auto material = materialOf (w);
    job.material = material.isNotEmpty();

    // What stella_paint reads: the material and its finish, texture and pattern layers.
    auto* surface = new juce::DynamicObject();

    if (job.material)
        surface->setProperty ("material", material);

    for (const auto* key : { "finish", "scale", "grain", "shine", "flow", "seed", "pattern", "patterns", "patternColor", "patternOpacity",
                             "patternBlend", "patternSize", "patternAngle", "patternWidth", "patternDepth" })
        if (const auto found = w.settings.find (key); found != w.settings.end() && ! found->second.isVoid())
            surface->setProperty (key, found->second);

    job.surface = juce::JSON::toString (juce::var (surface), true).toStdString();
    job.colour = w.colour.isTransparent() && ! job.material ? fallback : w.colour;
    job.colour2 = colourSetting (w, "color2");
    job.depth = juce::jlimit (-40.0f, 40.0f, w.number ("depth", 0.0f));
    job.shadow = juce::jlimit (0.0f, 1.0f, w.number ("shadow", 0.0f));
    job.globalAngle = globalAngle;
    job.globalAltitude = juce::jlimit (0.0f, 90.0f, globalAltitude);
    job.light = w.number ("light", globalAngle);
    job.altitude = juce::jlimit (10.0f, 80.0f, w.number ("altitude", globalAltitude));
    job.strokeWidth = juce::jmax (0.0f, w.strokeWidth);
    job.stroke = job.strokeWidth > 0.0f && w.stroke.isTransparent() ? juce::Colours::black : w.stroke;

    if (w.settings.count ("effects") > 0)
        job.style = styleOf (w);

    job.key = juce::String (job.width) + "x" + juce::String (job.height) + "|" + juce::String (job.angle, 2) + "|" + job.outline.toString()
            + "|" + juce::String (job.surface) + "|" + job.colour.toString() + "|" + job.colour2.toString() + "|" + juce::String ((int) job.material)
            + "|" + juce::String (job.depth, 2) + "|" + juce::String (job.shadow, 2) + "|" + juce::String (job.light, 1) + "|"
            + juce::String (job.altitude, 1) + "|" + job.stroke.toString() + "|" + juce::String (job.strokeWidth, 2);

    if (job.style.any())
        job.key << "|" << juce::JSON::toString (job.style.toVar(), true) << "|" << juce::String (job.globalAngle, 1) << "/" << juce::String (job.globalAltitude, 1);

    return job;
}

int ObjectPaint::pixelsOf (const Job& job)
{
    const auto area = juce::Rectangle<float> ((float) job.width, (float) job.height)
                          .transformedBy (juce::AffineTransform::rotation (juce::degreesToRadians (job.angle), (float) job.width * 0.5f, (float) job.height * 0.5f));
    const float margin = 4.0f + job.strokeWidth + std::fabs (job.depth) * 2.0f + job.shadow * 20.0f + (job.style.any() ? job.style.reach() : 0.0f);
    return (int) ((area.getWidth() + margin * 2.0f) * (area.getHeight() + margin * 2.0f));
}

//==============================================================================
ObjectPaint::Painting ObjectPaint::paint (const Job& job)
{
    namespace p = stella::paint;

    Painting result;
    result.key = job.key;
    result.angle = job.angle;
    result.width = job.width;
    result.height = job.height;

    if (job.width < 2 || job.height < 2 || job.outline.isEmpty())
        return result;

    const float turnRadians = juce::degreesToRadians (job.angle);
    const float ca = std::cos (turnRadians), sa = std::sin (turnRadians);
    const float cx = (float) job.width * 0.5f, cy = (float) job.height * 0.5f;
    const auto turn = juce::AffineTransform::rotation (turnRadians, cx, cy);

    auto turned = job.outline;
    turned.applyTransform (turn);

    // How far it reaches past its outline: the outline's width, the shadow.
    const float relief = std::fabs (job.depth);
    const bool raised = job.depth >= 0.0f;
    const float strokeHalf = job.strokeWidth * 0.5f;
    const float shadowDistance = job.shadow > 0.0f ? 2.0f + relief * 1.1f + job.shadow * 4.0f : 0.0f;
    const float shadowSoftness = job.shadow > 0.0f ? 3.0f + relief + job.shadow * 6.0f : 0.0f;
    const float margin = 2.0f + strokeHalf + (job.shadow > 0.0f && raised ? shadowDistance + shadowSoftness : 0.0f)
                       + (job.style.any() ? job.style.reach() : 0.0f);
    const auto area = turned.getBounds().expanded (margin).getSmallestIntegerContainer();
    const int W = area.getWidth(), H = area.getHeight();

    if (W < 1 || H < 1 || (long long) W * H > 16LL * 1024 * 1024)
        return result;

    result.offset = { (float) area.getX() - cx, (float) area.getY() - cy };

    // The outline as straight segments, in the picture's pixels (open ends closed, as a fill closes them).
    std::vector<Segment> segments;
    {
        juce::PathFlatteningIterator it (turned, juce::AffineTransform::translation ((float) -area.getX(), (float) -area.getY()), 0.2f);
        float startX = 0.0f, startY = 0.0f;
        bool newSubPath = true;

        while (it.next())
        {
            if (newSubPath)
            {
                startX = it.x1;
                startY = it.y1;
                newSubPath = false;
            }

            segments.push_back ({ it.x1, it.y1, it.x2, it.y2 });

            if (it.isLastInSubpath())
            {
                if (! it.closesSubPath && (it.x2 != startX || it.y2 != startY))
                    segments.push_back ({ it.x2, it.y2, startX, startY });

                newSubPath = true;
            }
        }
    }

    if (segments.empty())
        return result;

    // The signed distance to the outline (negative inside), exact near it, held at +-band beyond.
    const float band = juce::jmax (relief, strokeHalf, shadowDistance + shadowSoftness) + 3.0f;
    std::vector<float> distance ((size_t) W * (size_t) H, band);

    for (const auto& s : segments)
    {
        const int x0 = juce::jmax (0, (int) std::floor (juce::jmin (s.x1, s.x2) - band));
        const int x1 = juce::jmin (W - 1, (int) std::ceil (juce::jmax (s.x1, s.x2) + band));
        const int y0 = juce::jmax (0, (int) std::floor (juce::jmin (s.y1, s.y2) - band));
        const int y1 = juce::jmin (H - 1, (int) std::ceil (juce::jmax (s.y1, s.y2) + band));

        for (int y = y0; y <= y1; ++y)
        {
            auto* row = distance.data() + (size_t) y * (size_t) W;

            for (int x = x0; x <= x1; ++x)
                row[x] = juce::jmin (row[x], segmentDistance (s, (float) x + 0.5f, (float) y + 0.5f));
        }
    }

    // Inside or out, row by row (non-zero winding, as a path fills).
    {
        std::vector<std::pair<float, int>> crossings;

        for (int y = 0; y < H; ++y)
        {
            const float yc = (float) y + 0.5f;
            crossings.clear();

            for (const auto& s : segments)
                if ((s.y1 <= yc && s.y2 > yc) || (s.y2 <= yc && s.y1 > yc))
                    crossings.push_back ({ s.x1 + (yc - s.y1) * (s.x2 - s.x1) / (s.y2 - s.y1), s.y2 > s.y1 ? 1 : -1 });

            std::sort (crossings.begin(), crossings.end());
            auto* row = distance.data() + (size_t) y * (size_t) W;
            int winding = 0;
            size_t next = 0;

            for (int x = 0; x < W; ++x)
            {
                const float xc = (float) x + 0.5f;

                while (next < crossings.size() && crossings[next].first < xc)
                    winding += crossings[next++].second;

                if (winding != 0)
                    row[x] = -row[x];
            }
        }
    }

    auto distanceAt = [&distance, W, H, band] (float x, float y) noexcept
    {
        // Bilinear, in pixel centres; beyond the picture: outside.
        const float fx = x - 0.5f, fy = y - 0.5f;
        const int ix = (int) std::floor (fx), iy = (int) std::floor (fy);
        const float tx = fx - (float) ix, ty = fy - (float) iy;

        auto at = [&] (int px, int py) { return px < 0 || py < 0 || px >= W || py >= H ? band : distance[(size_t) py * (size_t) W + (size_t) px]; };
        const float top = at (ix, iy) + (at (ix + 1, iy) - at (ix, iy)) * tx;
        const float bottom = at (ix, iy + 1) + (at (ix + 1, iy + 1) - at (ix, iy + 1)) * tx;
        return top + (bottom - top) * ty;
    };

    // Its surface, worked out over its own (upright) box.
    auto surfacing = p::surfacingFrom (p::Json::parse (job.surface), p::Surfacing::of (p::Material::smooth, 1));

    if (! job.colour.isTransparent())
        surfacing.colour = p::rgbOf (job.colour.getARGB());

    if (! job.colour2.isTransparent())
        surfacing.colour2 = p::rgbOf (job.colour2.getARGB());
    else if (! job.material)
        surfacing.colour2 = surfacing.colour;

    // A flat colour stays exactly its colour where it's flat: lit only on its relief.
    p::Optics optics = surfacing.optics();

    if (! job.material)
        optics = { 0.0f, 0.12f, 0.45f, 0.0f, 0.0f, 0.0f, 1.0f };

    p::Patch patch;
    patch.fill (surfacing, -1, -1, job.width, job.height, p::Frame { 0.0f, 0.0f, (float) job.width, (float) job.height });

    auto texelAt = [&patch] (float lx, float ly, p::Rgb* albedo) noexcept
    {
        const float fx = lx - 0.5f, fy = ly - 0.5f;
        const int ix = (int) std::floor (fx), iy = (int) std::floor (fy);
        const float tx = fx - (float) ix, ty = fy - (float) iy;
        const auto& a = patch.at (ix, iy);
        const auto& b = patch.at (ix + 1, iy);
        const auto& c = patch.at (ix, iy + 1);
        const auto& d = patch.at (ix + 1, iy + 1);

        if (albedo != nullptr)
            *albedo = p::mix (p::mix (a.albedo, b.albedo, tx), p::mix (c.albedo, d.albedo, tx), ty);

        return (a.height + (b.height - a.height) * tx) * (1.0f - ty) + (c.height + (d.height - c.height) * tx) * ty;
    };

    const auto light = p::Light::from (job.light, job.altitude);
    const auto view = p::View::over ((float) W, (float) H);

    // Clear or tinted glass: see-through, its rim and the light's reflections solid (the light
    // where the panel's is: the glass's sheen doesn't turn with it).
    const bool seeThrough = job.material && p::isSeeThrough (surfacing.material);
    p::Glass glass;

    if (seeThrough)
    {
        glass.tinted = surfacing.material == p::Material::tintedGlass;
        glass.cx = cx - (float) area.getX();
        glass.cy = cy - (float) area.getY();
        glass.halfW = (float) job.width * 0.5f;
        glass.halfH = (float) job.height * 0.5f;
        glass.rim = juce::jlimit (2.0f, 9.0f, (float) juce::jmin (job.width, job.height) * 0.07f);
        glass.light = light;
    }

    const p::Rgb strokeColour = p::rgbOf (job.stroke.getARGB());
    const float strokeAlpha = job.stroke.getFloatAlpha();
    const float shadowStrength = job.shadow * 0.65f;
    const float fillAlpha = job.material || job.colour.isTransparent() ? 1.0f : job.colour.getFloatAlpha();   // a see-through flat colour

    juce::Image image (juce::Image::ARGB, W, H, true, juce::SoftwareImageType());
    {
        juce::Image::BitmapData pixels (image, juce::Image::BitmapData::writeOnly);

        for (int iy = 0; iy < H; ++iy)
        {
            for (int ix = 0; ix < W; ++ix)
            {
                const float px = (float) ix + 0.5f, py = (float) iy + 0.5f;
                const float d = distance[(size_t) iy * (size_t) W + (size_t) ix];
                const float cover = clamp01 (0.5f - d);
                const float strokeCover = job.strokeWidth > 0.0f ? clamp01 (strokeHalf + 0.5f - std::fabs (d)) * strokeAlpha : 0.0f;

                // The shadow it casts, away from the light (a raised or flat shape).
                float shadowAlpha = 0.0f;

                if (shadowStrength > 0.0f && raised && cover < 1.0f)
                    shadowAlpha = shadowStrength * (1.0f - smoothstep (-shadowSoftness, shadowSoftness,
                                                                       distanceAt (px + light.tx * shadowDistance, py + light.ty * shadowDistance)));

                if (cover <= 0.0f && strokeCover <= 0.0f && shadowAlpha <= 0.0f)
                    continue;

                float r = 0.0f, g = 0.0f, b = 0.0f, a = shadowAlpha;   // premultiplied, the shadow at the bottom

                if (cover > 0.0f)
                {
                    // Where this pixel lies on the upright shape, and its surface there.
                    const float rx = (px + (float) area.getX()) - cx, ry = (py + (float) area.getY()) - cy;
                    const float lx = rx * ca + ry * sa + cx, ly = -rx * sa + ry * ca + cy;

                    p::Rgb albedo;
                    texelAt (lx, ly, &albedo);

                    if (seeThrough)
                    {
                        // Its edge's outward way, from the distance's slope.
                        float gx = distanceAt (px + 0.75f, py) - distanceAt (px - 0.75f, py);
                        float gy = distanceAt (px, py + 0.75f) - distanceAt (px, py - 0.75f);
                        const float len = std::sqrt (gx * gx + gy * gy);
                        gx = len > 1.0e-5f ? gx / len : 0.0f;
                        gy = len > 1.0e-5f ? gy / len : 0.0f;

                        glass.tint = albedo;
                        p::Rgb lit;
                        float solid = 0.0f;
                        glass.at (px, py, juce::jmax (0.0f, -d), gx, gy, lit, solid);

                        const float fill = cover * solid;
                        r = clamp01 (lit.r) * cover + r * (1.0f - fill);
                        g = clamp01 (lit.g) * cover + g * (1.0f - fill);
                        b = clamp01 (lit.b) * cover + b * (1.0f - fill);
                        a = fill + a * (1.0f - fill);
                    }
                    else
                    {
                        const float hl = texelAt (lx - 1.0f, ly, nullptr), hr = texelAt (lx + 1.0f, ly, nullptr);
                        const float hu = texelAt (lx, ly - 1.0f, nullptr), hd = texelAt (lx, ly + 1.0f, nullptr);

                        // Its relief's slope on the upright shape, turned to the picture's way up.
                        float nlx = -(hr - hl) * 0.5f * optics.bump, nly = -(hd - hu) * 0.5f * optics.bump;

                        if (optics.flow > 0.0f)
                        {
                            const float q = p::fbm (lx * 0.0035f, ly * 0.0035f, surfacing.seed + 21, 4);
                            nlx += optics.flow * (p::fbm (lx * 0.0035f + q * 2.2f, ly * 0.0035f - q * 1.7f, surfacing.seed + 23, 4) - 0.5f) * 1.1f;
                            nly += optics.flow * (p::fbm (lx * 0.0035f - q * 1.9f, ly * 0.0035f + q * 2.4f, surfacing.seed + 27, 4) - 0.5f) * 1.1f;
                        }

                        float nx = nlx * ca - nly * sa, ny = nlx * sa + nly * ca;

                        // The bevel round its edge, raised or pressed in.
                        if (relief > 0.0f && d > -relief - 1.0f)
                        {
                            float gx = distanceAt (px + 0.75f, py) - distanceAt (px - 0.75f, py);
                            float gy = distanceAt (px, py + 0.75f) - distanceAt (px, py - 0.75f);
                            const float len = std::sqrt (gx * gx + gy * gy);

                            if (len > 1.0e-5f)
                            {
                                const float slope = p::bevelSlope (d, relief, raised ? 1.0f : -1.0f);
                                nx += slope * gx / len;
                                ny += slope * gy / len;
                            }
                        }

                        const float nl = std::sqrt (nx * nx + ny * ny + 1.0f);
                        auto lit = p::lightAt (albedo, optics, nx / nl, ny / nl, 1.0f / nl, px, py, light, view);

                        if (! job.material)
                            lit = p::add (albedo, p::add (lit, p::scale (p::lightAt (albedo, optics, 0.0f, 0.0f, 1.0f, px, py, light, view), -1.0f)));

                        // Pressed in: the rim's shadow falls inside.
                        if (! raised && relief > 0.0f)
                        {
                            const float inner = smoothstep (-(relief + 2.0f), relief + 2.0f,
                                                            distanceAt (px + light.tx * (relief * 1.1f + 1.5f), py + light.ty * (relief * 1.1f + 1.5f)));
                            lit = p::scale (lit, 1.0f - inner * 0.5f);
                        }

                        const float fill = cover * fillAlpha;
                        r = clamp01 (lit.r) * fill + r * (1.0f - fill);
                        g = clamp01 (lit.g) * fill + g * (1.0f - fill);
                        b = clamp01 (lit.b) * fill + b * (1.0f - fill);
                        a = fill + a * (1.0f - fill);
                    }
                }

                if (strokeCover > 0.0f)
                {
                    r = clamp01 (strokeColour.r) * strokeCover + r * (1.0f - strokeCover);
                    g = clamp01 (strokeColour.g) * strokeCover + g * (1.0f - strokeCover);
                    b = clamp01 (strokeColour.b) * strokeCover + b * (1.0f - strokeCover);
                    a = strokeCover + a * (1.0f - strokeCover);
                }

                auto* pixel = reinterpret_cast<juce::PixelARGB*> (pixels.getPixelPointer (ix, iy));
                pixel->setARGB ((juce::uint8) juce::roundToInt (clamp01 (a) * 255.0f), (juce::uint8) juce::roundToInt (clamp01 (r) * 255.0f),
                                (juce::uint8) juce::roundToInt (clamp01 (g) * 255.0f), (juce::uint8) juce::roundToInt (clamp01 (b) * 255.0f));
            }
        }
    }

    // Photoshop's layer style over it (and its drop shadow under it), lit by the global light.
    if (job.style.any())
        LayerRender::applyLayerStyle (image, job.style, 1.0f, job.globalAngle, job.globalAltitude);

    result.image = image;
    return result;
}
