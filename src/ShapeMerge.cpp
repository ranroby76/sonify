// D:\workspace\Fanan_web_hub\src\ShapeMerge.cpp

#include "ShapeMerge.h"
#include "ObjectPaint.h"
#include "clipper.h"   // Clipper2: the union

namespace
{
    namespace c2 = Clipper2Lib;

    constexpr double scale = 1000.0;          // panel pixels to Clipper's whole numbers: a thousandth of a pixel
    constexpr float flatness = 0.05f;         // curves as straight segments, this close (pixels)
    constexpr double simplify = 0.03;         // points that add less than this (pixels) go

    /** Its form on the panel: where it lies, turned. A line is its stroke; a shape that isn't
        filled, the form its outline runs along. */
    juce::Path panelPathOf (const GuiWidget& w)
    {
        const auto b = w.bounds.toFloat();
        auto path = ObjectPaint::geometry (w, b.getWidth(), b.getHeight());
        path.applyTransform (juce::AffineTransform::translation (b.getX(), b.getY()).followedBy (w.turn()));
        return path;
    }

    c2::Paths64 polygonsOf (const juce::Path& path)
    {
        c2::Paths64 out;
        c2::Path64 current;
        juce::PathFlatteningIterator it (path, juce::AffineTransform(), flatness);

        auto point = [] (float x, float y)
        {
            return c2::Point64 ((int64_t) std::llround ((double) x * scale), (int64_t) std::llround ((double) y * scale));
        };

        while (it.next())
        {
            if (current.empty())
                current.push_back (point (it.x1, it.y1));

            current.push_back (point (it.x2, it.y2));

            if (it.isLastInSubpath())
            {
                if (current.size() >= 3)
                    out.push_back (std::move (current));

                current.clear();
            }
        }

        if (current.size() >= 3)
            out.push_back (std::move (current));

        return out;
    }

    juce::String number (double v)
    {
        auto text = juce::String (v, 2);

        if (text.containsChar ('.'))
            text = text.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

        return text == "-0" ? juce::String ("0") : text;
    }
}

//==============================================================================
std::vector<int> ShapeMerge::mergeable (const GuiLayout& layout, const std::vector<int>& indexes)
{
    std::vector<int> shapes;

    for (const auto i : indexes)
        if (juce::isPositiveAndBelow (i, (int) layout.widgets.size()) && layout.widgets[(size_t) i].type == GuiWidget::Type::shape)
            shapes.push_back (i);

    std::sort (shapes.begin(), shapes.end());
    shapes.erase (std::unique (shapes.begin(), shapes.end()), shapes.end());
    return shapes;
}

std::optional<GuiWidget> ShapeMerge::merge (const std::vector<const GuiWidget*>& shapes)
{
    if (shapes.empty())
        return std::nullopt;

    // Each one alone first, so they all wind the same way (a shape drawn the other way round
    // would cancel out where it overlaps another); then the lot.
    c2::Paths64 all;

    for (const auto* w : shapes)
    {
        if (w == nullptr || w->bounds.isEmpty())
            continue;

        const auto own = c2::Union (polygonsOf (panelPathOf (*w)), c2::FillRule::NonZero);
        all.insert (all.end(), own.begin(), own.end());
    }

    auto merged = c2::SimplifyPaths (c2::Union (all, c2::FillRule::NonZero), simplify * scale);

    juce::Path path;

    for (const auto& polygon : merged)
    {
        if (polygon.size() < 3)
            continue;

        path.startNewSubPath ((float) ((double) polygon.front().x / scale), (float) ((double) polygon.front().y / scale));

        for (size_t k = 1; k < polygon.size(); ++k)
            path.lineTo ((float) ((double) polygon[k].x / scale), (float) ((double) polygon[k].y / scale));

        path.closeSubPath();
    }

    const auto area = path.getBounds();

    if (path.isEmpty() || area.getWidth() < 1.0f || area.getHeight() < 1.0f)
        return std::nullopt;

    // The top one's look, its form the merged one's: upright, its box snug round it (its
    // outline's inset outside it, as every shape's box has).
    auto result = *shapes.back();

    for (const auto* key : { "radius", "rounding", "points", "inner", "centerX", "centerY", "leanX", "leanY", "corners",
                             "line", "bendKinds", "lineWidth", "arrow", "cap" })
        result.settings.erase (key);

    result.settings["shape"] = "drawn";
    result.angle = 0.0f;
    result.rounding = -1.0f;

    const float inset = juce::jmax (0.0f, ObjectPaint::outlineWidthOf (result) * 0.5f);
    result.bounds = area.expanded (inset).getSmallestIntegerContainer();

    path.applyTransform (juce::AffineTransform::translation ((float) -result.bounds.getX(), (float) -result.bounds.getY()));
    result.path = toSvgPath (path);
    return result;
}

juce::String ShapeMerge::toSvgPath (const juce::Path& path)
{
    juce::String out;
    juce::Path::Iterator it (path);

    auto pointText = [] (float x, float y) { return number (x) + " " + number (y); };

    while (it.next())
    {
        if (out.isNotEmpty())
            out << " ";

        switch (it.elementType)
        {
            case juce::Path::Iterator::startNewSubPath: out << "M" << pointText (it.x1, it.y1); break;
            case juce::Path::Iterator::lineTo:          out << "L" << pointText (it.x1, it.y1); break;
            case juce::Path::Iterator::quadraticTo:     out << "Q" << pointText (it.x1, it.y1) << " " << pointText (it.x2, it.y2); break;
            case juce::Path::Iterator::cubicTo:         out << "C" << pointText (it.x1, it.y1) << " " << pointText (it.x2, it.y2) << " " << pointText (it.x3, it.y3); break;
            case juce::Path::Iterator::closePath:       out << "Z"; break;
            default: break;
        }
    }

    return out;
}
