// D:\workspace\Fanan_web_hub\src\LayerMerge.cpp

#include "LayerMerge.h"
#include "LayerRender.h"
#include "clipper.h"   // Clipper2: the outlines tidied into one shape

#include <algorithm>
#include <cmath>

namespace
{
    namespace c2 = Clipper2Lib;

    constexpr double unit = 1.0e6;   // 0..1 of the layer to Clipper's whole numbers

    bool alike (float a, float b) noexcept    { return std::abs (a - b) < 1.0e-4f; }

    bool alike (const AnimVal& a, const AnimVal& b) noexcept
    {
        return a.mode == b.mode && alike (a.from, b.from) && alike (a.to, b.to);
    }

    /** Whether two layers move alike: zoomed, moved and turned the same way at every frame. */
    bool moveAlike (const EffectSpec& a, const EffectSpec& b) noexcept
    {
        return a.zoomSeparate == b.zoomSeparate && alike (a.zoomX, b.zoomX) && (! a.zoomSeparate || alike (a.zoomY, b.zoomY))
            && alike (a.offsetX, b.offsetX) && alike (a.offsetY, b.offsetY) && alike (a.angle, b.angle)
            && a.keepDir == b.keepDir && alike (a.centreX, b.centreX) && alike (a.centreY, b.centreY);
    }

    //==========================================================================
    /** The outlines where a field (w x h, row by row) crosses iso, as closed loops in its
        pixels (a pixel's value at its middle). Past the field is 0, so every loop closes. */
    std::vector<std::vector<juce::Point<float>>> outlines (const std::vector<float>& field, int w, int h, float iso)
    {
        const int gw = w + 2, gh = h + 2;

        auto value = [&field, w, h] (int gx, int gy) noexcept
        {
            const int x = gx - 1, y = gy - 1;
            return x < 0 || y < 0 || x >= w || y >= h ? 0.0f : field[(size_t) y * (size_t) w + (size_t) x];
        };

        // Where the field crosses iso along a grid edge: horizontal (gx, gy) to (gx + 1, gy) is
        // edge 2k, vertical (gx, gy) to (gx, gy + 1) is 2k + 1, k = gy * gw + gx.
        auto crossing = [&value, gw, iso] (int id)
        {
            const int k = id / 2, gx = k % gw, gy = k / gw;
            const bool horizontal = (id % 2) == 0;
            const float v0 = value (gx, gy), v1 = horizontal ? value (gx + 1, gy) : value (gx, gy + 1);
            const float f = std::abs (v1 - v0) > 1.0e-6f ? juce::jlimit (0.0f, 1.0f, (iso - v0) / (v1 - v0)) : 0.5f;
            const float x = (float) gx - 0.5f + (horizontal ? f : 0.0f);
            const float y = (float) gy - 0.5f + (horizontal ? 0.0f : f);
            return juce::Point<float> (x, y);
        };

        std::vector<std::pair<int, int>> segments;
        std::vector<int> first ((size_t) gw * (size_t) gh * 2, -1), second ((size_t) gw * (size_t) gh * 2, -1);

        auto add = [&] (int a, int b)
        {
            const int s = (int) segments.size();
            segments.push_back ({ a, b });

            for (const int id : { a, b })
                (first[(size_t) id] < 0 ? first[(size_t) id] : second[(size_t) id]) = s;
        };

        for (int gy = 0; gy + 1 < gh; ++gy)
        {
            for (int gx = 0; gx + 1 < gw; ++gx)
            {
                const float tl = value (gx, gy), tr = value (gx + 1, gy), br = value (gx + 1, gy + 1), bl = value (gx, gy + 1);
                const int index = (tl > iso ? 1 : 0) | (tr > iso ? 2 : 0) | (br > iso ? 4 : 0) | (bl > iso ? 8 : 0);

                if (index == 0 || index == 15)
                    continue;

                const int T = (gy * gw + gx) * 2, B = ((gy + 1) * gw + gx) * 2;
                const int L = (gy * gw + gx) * 2 + 1, R = (gy * gw + gx + 1) * 2 + 1;
                const bool middleIn = (tl + tr + br + bl) * 0.25f > iso;

                switch (index)
                {
                    case 1: case 14: add (T, L); break;
                    case 2: case 13: add (T, R); break;
                    case 3: case 12: add (L, R); break;
                    case 4: case 11: add (R, B); break;
                    case 6: case 9:  add (T, B); break;
                    case 7: case 8:  add (L, B); break;

                    // Two corners across from each other: joined through the middle, or not.
                    case 5:  if (middleIn) { add (T, R); add (L, B); } else { add (T, L); add (R, B); } break;
                    case 10: if (middleIn) { add (T, L); add (R, B); } else { add (T, R); add (L, B); } break;
                    default: break;
                }
            }
        }

        // The segments linked end to end into loops.
        std::vector<std::vector<juce::Point<float>>> loops;
        std::vector<bool> used (segments.size(), false);

        for (size_t start = 0; start < segments.size(); ++start)
        {
            if (used[start])
                continue;

            std::vector<juce::Point<float>> loop;
            used[start] = true;
            loop.push_back (crossing (segments[start].first));

            int at = segments[start].second;
            int current = (int) start;

            for (;;)
            {
                loop.push_back (crossing (at));

                const int a = first[(size_t) at], b = second[(size_t) at];
                const int next = a == current ? b : a;

                if (next < 0 || used[(size_t) next])
                    break;

                used[(size_t) next] = true;
                at = segments[(size_t) next].first == at ? segments[(size_t) next].second : segments[(size_t) next].first;
                current = next;
            }

            if (loop.size() >= 3)
                loops.push_back (std::move (loop));
        }

        return loops;
    }

    juce::String number (double v)
    {
        auto text = juce::String (v, 5);

        if (text.containsChar ('.'))
            text = text.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

        return text == "-0" ? juce::String ("0") : text;
    }
}

//==============================================================================
int LayerMerge::merge (LayerDoc& doc, std::vector<int> indexes, float t)
{
    std::sort (indexes.begin(), indexes.end());
    indexes.erase (std::unique (indexes.begin(), indexes.end()), indexes.end());
    indexes.erase (std::remove_if (indexes.begin(), indexes.end(), [&doc] (int i) { return i < 0 || i >= (int) doc.layers.size(); }),
                   indexes.end());

    if (indexes.size() < 2)
        return -1;

    // Moving alike: merged in their own frame, and the merged one moves as they did.
    bool alikeMoves = true;

    for (const auto i : indexes)
        alikeMoves = alikeMoves && moveAlike (doc.layers[(size_t) i].eff, doc.layers[(size_t) indexes.front()].eff);

    // Their shapes, sharp: about a thousand pixels across the canvas.
    const float scale = juce::jlimit (1.0f, 16.0f, 1024.0f / (float) juce::jmax (1, doc.canvasWidth, doc.canvasHeight));
    const int w = juce::jmax (8, (int) std::lround (doc.canvasWidth * scale));
    const int h = juce::jmax (8, (int) std::lround (doc.canvasHeight * scale));

    // All of them in one: each solid where it's over half as solid as it gets.
    std::vector<float> field ((size_t) w * (size_t) h, 0.0f);

    for (const auto i : indexes)
    {
        auto shape = LayerRender::silhouette (doc, i, juce::jlimit (0.0f, 1.0f, t), scale, ! alikeMoves);

        if (shape.getWidth() != w || shape.getHeight() != h)
            continue;

        juce::Image::BitmapData data (shape, juce::Image::BitmapData::readOnly);
        int most = 0;

        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                most = juce::jmax (most, (int) data.getPixelColour (x, y).getAlpha());

        if (most == 0)
            continue;

        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                auto& v = field[(size_t) y * (size_t) w + (size_t) x];
                v = juce::jmax (v, (float) data.getPixelColour (x, y).getAlpha() / (float) most);
            }
    }

    // Their outlines, as one shape (outlines inside outlines are holes), in 0..1 of the layer.
    c2::Paths64 loops;

    for (const auto& loop : outlines (field, w, h, 0.5f))
    {
        c2::Path64 path;
        path.reserve (loop.size());

        for (const auto& p : loop)
            path.emplace_back ((int64_t) std::llround ((double) p.x / (double) w * unit), (int64_t) std::llround ((double) p.y / (double) h * unit));

        loops.push_back (std::move (path));
    }

    auto merged = c2::SimplifyPaths (c2::Union (loops, c2::FillRule::EvenOdd), 0.12 / (double) juce::jmax (w, h) * unit);
    merged.erase (std::remove_if (merged.begin(), merged.end(), [] (const c2::Path64& p) { return p.size() < 3; }), merged.end());

    if (merged.empty())
        return -1;

    juce::String svg;
    const c2::Path64* biggest = nullptr;

    for (const auto& path : merged)
    {
        if (biggest == nullptr || std::abs (c2::Area (path)) > std::abs (c2::Area (*biggest)))
            biggest = &path;

        for (size_t k = 0; k < path.size(); ++k)
            svg << (svg.isEmpty() ? "" : " ") << (k == 0 ? "M" : "L") << number ((double) path[k].x / unit) << " " << number ((double) path[k].y / unit);

        svg << " Z";
    }

    // The top one's look, as a shape of its own.
    const auto top = indexes.back();
    auto layer = doc.layers[(size_t) top];

    layer.name = "Merged shape";
    layer.knobmanLayer = -1;
    layer.prim.type = PrimType::Shape;
    layer.prim.shapeName.clear();
    layer.prim.shapePath = svg;
    layer.prim.shapePoints.clear();

    for (const auto& p : *biggest)
        layer.prim.shapePoints.push_back ({ (float) ((double) p.x / unit), (float) ((double) p.y / unit) });

    layer.prim.fill = true;
    layer.prim.aspect = 0.0f;
    layer.prim.angleStep = 0.0f;
    layer.prim.diffuse = 0.0f;

    // What its masks kept is in its shape now.
    layer.eff.mask1.enabled = false;
    layer.eff.mask2.enabled = false;

    // Moving differently: taken where they lay, so the merged one stays there.
    if (! alikeMoves)
    {
        auto& e = layer.eff;
        e.zoomSeparate = false;
        e.zoomX = AnimVal { 100.0f };
        e.zoomY = AnimVal { 100.0f };
        e.offsetX = AnimVal { 0.0f };
        e.offsetY = AnimVal { 0.0f };
        e.angle = AnimVal { 0.0f };
        e.keepDir = false;
        e.centreX = 50.0f;
        e.centreY = 50.0f;
    }

    // In the top one's place; the rest go (from the top down, so the ones below stay put).
    doc.layers[(size_t) top] = layer;

    for (auto it = std::next (indexes.rbegin()); it != indexes.rend(); ++it)
        doc.layers.erase (doc.layers.begin() + *it);

    return top - (int) (indexes.size() - 1);
}
