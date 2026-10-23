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

    /** Its points as fractions of its box: start, bend, end (a straight line across the box's
        middle when it has none, or they're broken). */
    std::array<float, 6> lineFractions (const GuiWidget& w)
    {
        std::array<float, 6> f { 0.06f, 0.5f, 0.5f, 0.5f, 0.94f, 0.5f };
        const auto found = w.settings.find ("line");

        if (found == w.settings.end())
            return f;

        if (const auto* list = found->second.getArray(); list != nullptr && list->size() >= 4)
        {
            std::array<float, 6> read {};
            bool ok = true;

            for (int i = 0; i < 6 && ok; ++i)
            {
                // Four numbers: its ends only, straight; six: with the bend point between.
                const int from = list->size() >= 6 ? i : (i < 2 ? i : (i >= 4 ? i - 2 : -1));

                if (from < 0)
                    continue;

                const auto& v = list->getReference (from);
                ok = v.isInt() || v.isInt64() || v.isDouble() || v.isBool() || v.toString().containsOnly ("-0123456789.e");
                read[(size_t) i] = (float) (double) v;
                ok = ok && std::isfinite (read[(size_t) i]);
            }

            if (ok)
            {
                if (list->size() < 6)
                {
                    read[2] = (read[0] + read[4]) * 0.5f;
                    read[3] = (read[1] + read[5]) * 0.5f;
                }

                for (auto& v : read)
                    v = juce::jlimit (-lineReach, lineReach, v);

                f = read;
            }
        }

        return f;
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
        const juce::Point<float> a (f[0] * width, f[1] * height), m (f[2] * width, f[3] * height), b (f[4] * width, f[5] * height);
        const float half = lineHalfWidth (w);
        const auto arrows = lineArrows (w);
        const auto cap = lineCap (w);
        const bool arrowStart = arrows == "start" || arrows == "both", arrowEnd = arrows == "end" || arrows == "both";

        auto samples = lineSamples (a, m, b);
        juce::Path path;

        if (a.getDistanceFrom (b) < 0.5f && a.getDistanceFrom (m) < 0.5f)
        {
            path.addEllipse (juce::Rectangle<float> (half * 2.0f, half * 2.0f).withCentre (a));
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

        // Each sample's normal (to the left going start to end), from its neighbours.
        std::vector<juce::Point<float>> normals (samples.size());

        for (size_t k = 0; k < samples.size(); ++k)
        {
            const auto before = samples[k > 0 ? k - 1 : k], after = samples[k + 1 < samples.size() ? k + 1 : k];
            const auto t = after - before;
            const float len = t.getDistanceFromOrigin();

            if (len > 1.0e-5f)
                normals[k] = { t.y / len, -t.x / len };
            else
                normals[k] = k > 0 ? normals[k - 1] : juce::Point<float> (0.0f, -1.0f);
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
            const auto n = atStart ? normals.front() : normals.back();
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

    const float inset = juce::jmax (0.0f, w.strokeWidth * 0.5f);
    const auto box = juce::Rectangle<float> (0.0f, 0.0f, width, height).reduced (juce::jmin (inset, width * 0.25f), juce::jmin (inset, height * 0.25f));

    auto path = lopsided (unitShape (w, kind, box.getWidth(), box.getHeight()),
                          kind == "rectangle" ? 0.0f : juce::jlimit (-0.95f, 0.95f, w.number ("centerX", 0.0f)),
                          kind == "rectangle" || kind == "triangle" ? 0.0f : juce::jlimit (-0.95f, 0.95f, w.number ("centerY", 0.0f)),
                          juce::jlimit (-1.0f, 1.0f, w.number ("leanX", 0.0f)), juce::jlimit (-1.0f, 1.0f, w.number ("leanY", 0.0f)));

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

    return path;
}

ObjectPaint::LinePoints ObjectPaint::linePoints (const GuiWidget& w)
{
    const auto f = lineFractions (w);
    const float bw = (float) w.bounds.getWidth(), bh = (float) w.bounds.getHeight();
    return { { f[0] * bw, f[1] * bh }, { f[2] * bw, f[3] * bh }, { f[4] * bw, f[5] * bh } };
}

void ObjectPaint::setLinePoints (GuiWidget& w, LinePoints p, bool fitBox)
{
    auto box = w.bounds.toFloat();

    if (fitBox && ! w.isTurned())
    {
        // Snug round the curve and its stroke (arrowheads included), with a little room to pick it by.
        // (By hand: a rectangle's union skips empty ones, and a point's is empty.)
        const auto samples = lineSamples (p.start, p.bend, p.end);
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
        p.bend += shift;
        p.end += shift;
    }

    const float bw = juce::jmax (1.0f, box.getWidth()), bh = juce::jmax (1.0f, box.getHeight());
    auto fraction = [] (float v) { return juce::roundToInt (juce::jlimit (-lineReach, lineReach, std::isfinite (v) ? v : 0.5f) * 10000.0f) / 10000.0; };
    w.settings["line"] = juce::Array<juce::var> { fraction (p.start.x / bw), fraction (p.start.y / bh), fraction (p.bend.x / bw),
                                                  fraction (p.bend.y / bh), fraction (p.end.x / bw), fraction (p.end.y / bh) };
}

void ObjectPaint::straighten (GuiWidget& w)
{
    auto p = linePoints (w);
    p.bend = (p.start + p.end) * 0.5f;
    setLinePoints (w, p, true);
}

namespace
{
    /** The bend point snapped onto the straight way between the ends when it's within a few pixels of it. */
    void snapStraight (ObjectPaint::LinePoints& p)
    {
        const auto chord = p.end - p.start;
        const float length = chord.getDistanceFromOrigin();

        if (length <= 1.0f)
            return;

        const auto off = p.bend - p.start;
        const float across = std::abs (chord.x * off.y - chord.y * off.x) / length;

        if (across < straightWithin)
            p.bend = p.start + chord * ((off.x * chord.x + off.y * chord.y) / (length * length));
    }

    juce::Point<float> quadraticAt (juce::Point<float> a, juce::Point<float> control, juce::Point<float> b, float t)
    {
        return a * ((1.0f - t) * (1.0f - t)) + control * (2.0f * t * (1.0f - t)) + b * (t * t);
    }
}

float ObjectPaint::lineParameterAt (const GuiWidget& w, juce::Point<float> local)
{
    const auto p = linePoints (w);
    const auto control = p.bend * 2.0f - (p.start + p.end) * 0.5f;
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
    // The curve passes `local` at `along`: solve for its control point, then its bend point (the
    // curve's middle). Too near an end, it would fly off: held a little way in.
    auto p = linePoints (atStart);
    const float t = juce::jlimit (0.12f, 0.88f, along);
    const auto control = (local - p.start * ((1.0f - t) * (1.0f - t)) - p.end * (t * t)) / (2.0f * t * (1.0f - t));
    p.bend = (control + (p.start + p.end) * 0.5f) * 0.5f;
    snapStraight (p);
    setLinePoints (w, p, false);
}

void ObjectPaint::moveLineEnd (GuiWidget& w, const GuiWidget& atStart, bool start, juce::Point<float> local)
{
    const auto before = linePoints (atStart);
    auto p = before;
    (start ? p.start : p.end) = local;

    // The bow follows its ends as a whole: turned and stretched with the line (as complex numbers,
    // the old ends mapped onto the new ones).
    const auto oldChord = before.end - before.start, newChord = p.end - p.start;
    const float oldLength = oldChord.x * oldChord.x + oldChord.y * oldChord.y;

    if (oldLength > 1.0e-3f)
    {
        const auto rel = before.bend - before.start;
        const juce::Point<float> ratio ((newChord.x * oldChord.x + newChord.y * oldChord.y) / oldLength,
                                        (newChord.y * oldChord.x - newChord.x * oldChord.y) / oldLength);
        p.bend = p.start + juce::Point<float> (rel.x * ratio.x - rel.y * ratio.y, rel.x * ratio.y + rel.y * ratio.x);
    }
    else
    {
        p.bend = (p.start + p.end) * 0.5f;
    }

    setLinePoints (w, p, false);
}

bool ObjectPaint::isBent (const GuiWidget& w)
{
    const auto p = linePoints (w);
    return lineSamples (p.start, p.bend, p.end).size() > 2;
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
        return linePoints (w).bend;

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
        p.bend += moved;
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
