// D:\workspace\Fanan_web_hub\src\Body3D.cpp

#include "Body3D.h"
#include "FontLibrary.h"
#include "MeshField.h"
#include "Model3D.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <map>
#include <mutex>

namespace
{
    constexpr float pi = 3.14159265358979f;
    constexpr float far = 1.0e6f;

    inline float len2 (float a, float b)            { return std::sqrt (a * a + b * b); }
    inline float len3 (float a, float b, float c)   { return std::sqrt (a * a + b * b + c * c); }
    inline float clamp01 (float v)                  { return v < 0 ? 0 : (v > 1 ? 1 : v); }
    inline float mixf (float a, float b, float t)   { return a + (b - a) * t; }

    float numberOf (const juce::var& o, const char* key, float fallback)
    {
        const auto v = o.getProperty (key, {});
        return v.isVoid() || v.toString().isEmpty() ? fallback : (float) (double) v;
    }

    juce::Colour colourOf (const juce::var& v, juce::Colour fallback)
    {
        auto hex = v.toString().trim().trimCharactersAtStart ("#");

        if (hex.length() == 6)
            hex = "ff" + hex;

        return hex.length() == 8 && hex.containsOnly ("0123456789abcdefABCDEF") ? juce::Colour ((juce::uint32) hex.getHexValue64()) : fallback;
    }

    //==========================================================================
    // Hash noise, for the surfaces' textures.
    inline float hash3 (int x, int y, int z)
    {
        auto h = (unsigned) (x * 374761393 + y * 668265263 + z * 2147483647);
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffffff) / (float) 0xffffff;
    }

    float noise3 (float x, float y, float z)
    {
        const int ix = (int) std::floor (x), iy = (int) std::floor (y), iz = (int) std::floor (z);
        const float fx = x - (float) ix, fy = y - (float) iy, fz = z - (float) iz;
        const float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy), uz = fz * fz * (3 - 2 * fz);

        auto lerp = [] (float a, float b, float t) { return a + (b - a) * t; };
        const float a = lerp (hash3 (ix, iy, iz),         hash3 (ix + 1, iy, iz),         ux);
        const float b = lerp (hash3 (ix, iy + 1, iz),     hash3 (ix + 1, iy + 1, iz),     ux);
        const float c = lerp (hash3 (ix, iy, iz + 1),     hash3 (ix + 1, iy, iz + 1),     ux);
        const float d = lerp (hash3 (ix, iy + 1, iz + 1), hash3 (ix + 1, iy + 1, iz + 1), ux);
        return lerp (lerp (a, b, uy), lerp (c, d, uy), uz);
    }

    //==========================================================================
    // Felzenszwalb's distance transform (squared), one line at a time.
    void edt1d (const float* f, float* d, int n, std::vector<int>& v, std::vector<float>& z)
    {
        v.resize ((size_t) n);
        z.resize ((size_t) n + 1);
        int k = 0;
        v[0] = 0;
        z[0] = -std::numeric_limits<float>::infinity();
        z[1] = std::numeric_limits<float>::infinity();

        for (int q = 1; q < n; ++q)
        {
            float s = ((f[q] + (float) (q * q)) - (f[v[(size_t) k]] + (float) (v[(size_t) k] * v[(size_t) k]))) / (float) (2 * q - 2 * v[(size_t) k]);

            while (s <= z[(size_t) k])
            {
                --k;
                s = ((f[q] + (float) (q * q)) - (f[v[(size_t) k]] + (float) (v[(size_t) k] * v[(size_t) k]))) / (float) (2 * q - 2 * v[(size_t) k]);
            }

            ++k;
            v[(size_t) k] = q;
            z[(size_t) k] = s;
            z[(size_t) k + 1] = std::numeric_limits<float>::infinity();
        }

        k = 0;

        for (int q = 0; q < n; ++q)
        {
            while (z[(size_t) k + 1] < (float) q)
                ++k;

            const auto dq = (float) (q - v[(size_t) k]);
            d[q] = dq * dq + f[v[(size_t) k]];
        }
    }

    /** Squared distance (in cells) from every cell to the nearest cell where `on` is true. */
    std::vector<float> edt2d (const std::vector<bool>& on, int w, int h)
    {
        const float inf = 1.0e20f;
        std::vector<float> grid ((size_t) w * (size_t) h);

        for (size_t i = 0; i < grid.size(); ++i)
            grid[i] = on[i] ? 0.0f : inf;

        std::vector<float> f ((size_t) std::max (w, h)), d ((size_t) std::max (w, h));
        std::vector<int> v;
        std::vector<float> z;

        for (int x = 0; x < w; ++x)
        {
            for (int y = 0; y < h; ++y) f[(size_t) y] = grid[(size_t) y * (size_t) w + (size_t) x];
            edt1d (f.data(), d.data(), h, v, z);
            for (int y = 0; y < h; ++y) grid[(size_t) y * (size_t) w + (size_t) x] = d[(size_t) y];
        }

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x) f[(size_t) x] = grid[(size_t) y * (size_t) w + (size_t) x];
            edt1d (f.data(), d.data(), w, v, z);
            for (int x = 0; x < w; ++x) grid[(size_t) y * (size_t) w + (size_t) x] = d[(size_t) x];
        }

        return grid;
    }

    /** The fields made lately, by what they were made from: a body edited part by part makes
        only the changed part's again (a profile's or the words' field is the slow thing). */
    class FieldCache
    {
    public:
        static FieldCache& get()
        {
            static FieldCache cache;
            return cache;
        }

        template <typename Make>
        std::shared_ptr<Body3D::Part::Field> find (const juce::String& key, Make&& make)
        {
            {
                const std::lock_guard<std::mutex> lock (mutex);

                if (const auto found = byKey.find (key); found != byKey.end())
                {
                    order.splice (order.begin(), order, found->second.second);   // most lately used first
                    return found->second.first;
                }
            }

            auto field = make();   // made outside the lock: another thread may make another meanwhile
            const std::lock_guard<std::mutex> lock (mutex);

            if (byKey.find (key) == byKey.end())
            {
                order.push_front (key);
                byKey[key] = { field, order.begin() };

                while (order.size() > 160)
                {
                    byKey.erase (order.back());
                    order.pop_back();
                }
            }

            return field;
        }

    private:
        std::mutex mutex;
        std::list<juce::String> order;
        std::map<juce::String, std::pair<std::shared_ptr<Body3D::Part::Field>, std::list<juce::String>::iterator>> byKey;
    };

    juce::String fieldKey (const char* kind, const juce::Path& path, float a, float b, float c = 0.0f)
    {
        return juce::String (kind) + "|" + juce::String (a, 3) + "|" + juce::String (b, 3) + "|" + juce::String (c, 3) + "|" + path.toString();
    }

    std::shared_ptr<Body3D::Part::Field> makeField (const juce::Path& path, float planeW, float planeH);
    std::shared_ptr<Body3D::Part::Field> makePathField (const juce::Path& path, float planeW, float planeH, float reach);

    std::shared_ptr<Body3D::Part::Field> fieldOf (const juce::Path& path, float planeW, float planeH)
    {
        return FieldCache::get().find (fieldKey ("fill", path, planeW, planeH), [&] { return makeField (path, planeW, planeH); });
    }

    std::shared_ptr<Body3D::Part::Field> pathFieldOf (const juce::Path& path, float planeW, float planeH, float reach)
    {
        return FieldCache::get().find (fieldKey ("path", path, planeW, planeH, reach), [&] { return makePathField (path, planeW, planeH, reach); });
    }

    /** A filled outline's signed distance field over a plane planeW x planeH pixels (the path in
        those pixels), fine enough for its size. */
    std::shared_ptr<Body3D::Part::Field> makeField (const juce::Path& path, float planeW, float planeH)
    {
        auto field = std::make_shared<Body3D::Part::Field>();
        const float longest = std::max (planeW, planeH);
        field->cell = std::max (0.25f, longest / 1024.0f);
        const int margin = 6;
        field->w = juce::jlimit (4, 2200, (int) std::ceil (planeW / field->cell) + 2 * margin);
        field->h = juce::jlimit (4, 2200, (int) std::ceil (planeH / field->cell) + 2 * margin);
        field->x0 = -(float) margin * field->cell;
        field->y0 = -(float) margin * field->cell;

        juce::Image mask (juce::Image::SingleChannel, field->w, field->h, true, juce::SoftwareImageType());
        {
            juce::Graphics g (mask);
            g.setColour (juce::Colours::white);
            g.fillPath (path, juce::AffineTransform::translation (-field->x0, -field->y0).scaled (1.0f / field->cell));
        }

        const auto n = (size_t) field->w * (size_t) field->h;
        std::vector<bool> inside (n), outside (n);
        std::vector<float> coverage (n);
        {
            const juce::Image::BitmapData data (mask, juce::Image::BitmapData::readOnly);

            for (int y = 0; y < field->h; ++y)
                for (int x = 0; x < field->w; ++x)
                {
                    const auto c = (float) *data.getPixelPointer (x, y) / 255.0f;
                    const auto i = (size_t) y * (size_t) field->w + (size_t) x;
                    coverage[i] = c;
                    inside[i] = c >= 0.5f;
                    outside[i] = ! inside[i];
                }
        }

        field->d.resize (n);

        // Exactly, from the outline's segments, where that's not too many to measure every cell
        // against (a profile, a few words): smooth, so the surfaces it makes are smooth too.
        std::vector<juce::Line<float>> segments;
        {
            juce::PathFlatteningIterator it (path, {}, std::max (0.02f, field->cell * 0.2f));
            juce::Point<float> start;
            bool open = false;

            while (it.next())
            {
                segments.emplace_back (it.x1, it.y1, it.x2, it.y2);

                if (! open) { start = { it.x1, it.y1 }; open = true; }

                if (it.closesSubPath)
                {
                    if (juce::Point<float> (it.x2, it.y2) != start)
                        segments.emplace_back (it.x2, it.y2, start.x, start.y);

                    open = false;
                }
            }
        }

        if (! segments.empty() && (double) segments.size() * (double) n <= 9.0e7)
        {
            for (int y = 0; y < field->h; ++y)
            {
                const float v = field->y0 + ((float) y + 0.5f) * field->cell;

                for (int x = 0; x < field->w; ++x)
                {
                    const float u = field->x0 + ((float) x + 0.5f) * field->cell;
                    float best = 1.0e20f;
                    int winding = 0;

                    for (const auto& line : segments)
                    {
                        const float ax = line.getStartX(), ay = line.getStartY(), bx = line.getEndX(), by = line.getEndY();
                        const float ex = bx - ax, ey = by - ay, wx = u - ax, wy = v - ay;
                        const float len = ex * ex + ey * ey;
                        const float t = len > 1.0e-12f ? clamp01 ((wx * ex + wy * ey) / len) : 0.0f;
                        const float dx = wx - ex * t, dy = wy - ey * t;
                        best = std::min (best, dx * dx + dy * dy);

                        // Its winding round the point (non-zero: inside).
                        if (ay <= v)
                        {
                            if (by > v && ex * wy - ey * wx > 0.0f) ++winding;
                        }
                        else if (by <= v && ex * wy - ey * wx < 0.0f)
                        {
                            --winding;
                        }
                    }

                    const float dist = std::sqrt (best);
                    field->d[(size_t) y * (size_t) field->w + (size_t) x] = winding != 0 ? -dist : dist;
                }
            }

            return field;
        }

        // Otherwise from its picture (many words, a big logo): in cells, a half cell either way
        // at the edge, the edge's coverage moving it within its cell.
        const auto toInside = edt2d (inside, field->w, field->h), toOutside = edt2d (outside, field->w, field->h);

        for (size_t i = 0; i < n; ++i)
        {
            const float shift = (0.5f - coverage[i]) * 0.9f;
            const float cells = inside[i] ? -(std::sqrt (toOutside[i]) - 0.5f) : (std::sqrt (toInside[i]) - 0.5f);
            field->d[i] = (std::abs (cells) < 1.5f ? cells * 0.5f + shift : cells) * field->cell;
        }

        return field;
    }

    /** A path's field over a plane planeW x planeH pixels (and reach pixels round it): how far
        from the path (its centre line), signed by its side (a closed path: below 0 inside it).
        A sweep's section is measured from it. */
    std::shared_ptr<Body3D::Part::Field> makePathField (const juce::Path& path, float planeW, float planeH, float reach)
    {
        auto field = std::make_shared<Body3D::Part::Field>();
        const float spanW = planeW + 2.0f * reach, spanH = planeH + 2.0f * reach;
        field->cell = std::max (0.25f, std::max (spanW, spanH) / 512.0f);
        field->w = juce::jlimit (4, 1100, (int) std::ceil (spanW / field->cell) + 2);
        field->h = juce::jlimit (4, 1100, (int) std::ceil (spanH / field->cell) + 2);
        field->x0 = -reach - field->cell;
        field->y0 = -reach - field->cell;

        std::vector<juce::Line<float>> segments;
        bool closed = false;
        {
            juce::PathFlatteningIterator it (path, {}, std::max (0.05f, field->cell * 0.25f));
            juce::Point<float> start;
            bool open = false;

            while (it.next())
            {
                segments.emplace_back (it.x1, it.y1, it.x2, it.y2);

                if (! open) { start = { it.x1, it.y1 }; open = true; }

                if (it.closesSubPath)
                {
                    closed = true;

                    if (juce::Point<float> (it.x2, it.y2) != start)
                        segments.emplace_back (it.x2, it.y2, start.x, start.y);

                    open = false;
                }
            }
        }

        field->d.assign ((size_t) field->w * (size_t) field->h, far);

        if (segments.empty())
            return field;

        for (int y = 0; y < field->h; ++y)
        {
            const float v = field->y0 + ((float) y + 0.5f) * field->cell;

            for (int x = 0; x < field->w; ++x)
            {
                const float u = field->x0 + ((float) x + 0.5f) * field->cell;
                float best = 1.0e20f, side = 1.0f;
                int winding = 0;

                for (const auto& line : segments)
                {
                    const float ax = line.getStartX(), ay = line.getStartY(), bx = line.getEndX(), by = line.getEndY();
                    const float ex = bx - ax, ey = by - ay, wx = u - ax, wy = v - ay;
                    const float len = ex * ex + ey * ey;
                    const float t = len > 1.0e-12f ? clamp01 ((wx * ex + wy * ey) / len) : 0.0f;
                    const float dx = wx - ex * t, dy = wy - ey * t;
                    const float d2 = dx * dx + dy * dy;

                    if (d2 < best)
                    {
                        best = d2;
                        side = ex * wy - ey * wx >= 0.0f ? 1.0f : -1.0f;
                    }

                    if (ay <= v)
                    {
                        if (by > v && ex * wy - ey * wx > 0.0f) ++winding;
                    }
                    else if (by <= v && ex * wy - ey * wx < 0.0f)
                    {
                        --winding;
                    }
                }

                const float dist = std::sqrt (best);
                field->d[(size_t) y * (size_t) field->w + (size_t) x] = closed ? (winding != 0 ? -dist : dist) : dist * side;
            }
        }

        return field;
    }

    juce::Path svgOutline (const juce::File& file)
    {
        if (auto drawable = juce::Drawable::createFromImageFile (file))
            return drawable->getOutlineAsPath();

        return {};
    }
}

//==============================================================================
float Body3D::Part::Field::at (float u, float v) const
{
    if (w < 2 || h < 2)
        return far;

    const float gx = (u - x0) / cell, gy = (v - y0) / cell;
    const float cxp = juce::jlimit (0.0f, (float) (w - 1), gx), cyp = juce::jlimit (0.0f, (float) (h - 1), gy);
    const float outsideBy = len2 (gx - cxp, gy - cyp) * cell;   // past the field's edge

    const int ix = std::min (w - 2, (int) cxp), iy = std::min (h - 2, (int) cyp);
    const float fx = cxp - (float) ix, fy = cyp - (float) iy;
    const auto row = (size_t) iy * (size_t) w;
    const float a = d[row + (size_t) ix], b = d[row + (size_t) ix + 1];
    const float c = d[row + (size_t) w + (size_t) ix], e = d[row + (size_t) w + (size_t) ix + 1];
    return mixf (mixf (a, b, fx), mixf (c, e, fx), fy) + outsideBy;
}

//==============================================================================
Body3D::Body3D() = default;
Body3D::~Body3D() = default;

bool Body3D::hasParts (const juce::var& json)
{
    const auto* list = json.getProperty ("parts", {}).getArray();
    return list != nullptr && ! list->isEmpty();
}

float Body3D::mirrorAtOf (const juce::var& json, int width)
{
    const auto at = json.getProperty ("mirrorAt", {});
    return at.isDouble() || at.isInt() || at.isInt64() ? (float) (double) at : (float) width * 0.5f;
}

std::unique_ptr<Body3D> Body3D::fromVar (const juce::var& json, int width, int height, const juce::File& images)
{
    if (! hasParts (json))
        return nullptr;

    auto body = std::make_unique<Body3D>();
    body->width = juce::jmax (8, width);
    body->height = juce::jmax (8, height);
    body->tilt = juce::jlimit (0.0f, 60.0f, numberOf (json, "tilt", 0.0f));
    body->shadow = juce::jlimit (0.0f, 100.0f, numberOf (json, "shadow", 60.0f));
    body->softness = juce::jlimit (0.0f, 100.0f, numberOf (json, "softness", 55.0f));
    body->exposure = juce::jlimit (-100.0f, 100.0f, numberOf (json, "exposure", 0.0f));
    body->floor = colourOf (json.getProperty ("floor", {}), juce::Colour (0x00000000));
    body->mirrorAxis = mirrorAtOf (json, body->width) - (float) body->width * 0.5f;

    const float W = (float) body->width, H = (float) body->height;
    const auto& materials = Model3D::materialNames();

    for (int i = 0; i < 3; ++i)
    {
        body->lo[i] = far;
        body->hi[i] = -far;
    }

    int source = -1;

    for (const auto& item : *json.getProperty ("parts", {}).getArray())
    {
        ++source;

        if ((bool) item.getProperty ("hidden", false))
            continue;   // hidden in the Body editor: not there for now

        Part p;
        p.source = source;
        p.name = item.getProperty ("name", {}).toString();

        const auto kind = item.getProperty ("kind", "box").toString().trim().toLowerCase();
        p.kind = kind == "cylinder" ? Kind::cylinder : kind == "sphere" ? Kind::sphere : kind == "extrude" ? Kind::extrude
               : kind == "revolve" ? Kind::revolve : kind == "text" ? Kind::text : kind == "sweep" ? Kind::sweep
               : kind == "loft" ? Kind::loft : kind == "mesh" ? Kind::mesh : Kind::box;

        const auto plane = item.getProperty ("plane", "side").toString().trim().toLowerCase();
        p.plane = plane == "front" ? Plane::front : plane == "top" ? Plane::top : Plane::side;

        const auto axis = item.getProperty ("axis", "y").toString().trim().toLowerCase();
        p.axis = axis == "x" ? 0 : axis == "z" ? 2 : 1;

        p.cut = item.getProperty ("op", "add").toString().trim().equalsIgnoreCase ("cut");
        p.blend = juce::jlimit (0.0f, 200.0f, numberOf (item, "blend", 0.0f));
        p.round = juce::jlimit (0.0f, 400.0f, numberOf (item, "round", 0.0f));
        p.chamfer = juce::jlimit (0.0f, 400.0f, numberOf (item, "chamfer", 0.0f));

        const float w = juce::jlimit (0.5f, 8000.0f, numberOf (item, "w", 100.0f));
        const float h = juce::jlimit (0.5f, 8000.0f, numberOf (item, "h", 20.0f));
        const float d = juce::jlimit (0.5f, 8000.0f, numberOf (item, "d", 100.0f));
        p.hw = w * 0.5f; p.hh = h * 0.5f; p.hd = d * 0.5f;
        p.cx = numberOf (item, "x", W * 0.5f) - W * 0.5f;
        p.cz = numberOf (item, "z", H * 0.5f) - H * 0.5f;
        p.cy = numberOf (item, "y", 0.0f) + p.hh;

        const float limit = std::min (p.hw, std::min (p.hh, p.hd)) * 0.98f;
        p.chamfer = std::min (p.chamfer, limit);

        // Its turn: world = R local (R = Rz Ry Rx); kept as R transposed, world -> its own.
        if (const auto* r = item.getProperty ("rotate", {}).getArray(); r != nullptr && r->size() >= 3)
        {
            const float ax = (float) (double) (*r)[0] * pi / 180.0f, ay = (float) (double) (*r)[1] * pi / 180.0f, az = (float) (double) (*r)[2] * pi / 180.0f;

            if (std::abs (ax) + std::abs (ay) + std::abs (az) > 1.0e-5f)
            {
                const float cxr = std::cos (ax), sxr = std::sin (ax), cyr = std::cos (ay), syr = std::sin (ay), czr = std::cos (az), szr = std::sin (az);
                const float Rx[9] { 1, 0, 0, 0, cxr, -sxr, 0, sxr, cxr };
                const float Ry[9] { cyr, 0, syr, 0, 1, 0, -syr, 0, cyr };
                const float Rz[9] { czr, -szr, 0, szr, czr, 0, 0, 0, 1 };
                float a[9], m[9];

                auto mul = [] (const float* l, const float* r2, float* o)
                {
                    for (int i = 0; i < 3; ++i)
                        for (int j = 0; j < 3; ++j)
                            o[i * 3 + j] = l[i * 3] * r2[j] + l[i * 3 + 1] * r2[3 + j] + l[i * 3 + 2] * r2[6 + j];
                };

                mul (Rz, Ry, a);
                mul (a, Rx, m);

                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 3; ++j)
                        p.rot[i * 3 + j] = m[j * 3 + i];

                p.rotated = true;
            }
        }

        p.mirrorX = item.getProperty ("mirror", {}).toString().containsIgnoreCase ("x");

        if (const auto repeat = item.getProperty ("repeat", {}); repeat.isObject())
        {
            const auto rk = repeat.getProperty ("kind", "row").toString().trim().toLowerCase();
            p.repeatKind = rk == "grid" ? 2 : rk == "circle" ? 3 : 1;
            p.count = juce::jlimit (1, 256, (int) numberOf (repeat, "count", 2.0f));
            p.countZ = juce::jlimit (1, 256, (int) numberOf (repeat, "countZ", 1.0f));
            p.dx = numberOf (repeat, "dx", w * 1.5f);
            p.dz = numberOf (repeat, "dz", d * 1.5f);
            p.radius = std::max (0.0f, numberOf (repeat, "radius", 0.0f));

            if (p.repeatKind == 1)
                p.countZ = 1;
        }

        // Its material, by name (or its number) and colour.
        {
            const auto m = item.getProperty ("material", {});

            if (m.isString() && ! m.toString().containsOnly ("0123456789"))
            {
                p.material = 0;

                for (int k = 0; k < materials.size(); ++k)
                    if (materials[k].equalsIgnoreCase (m.toString().trim()))
                        p.material = k;

                for (int k = 0; k < materials.size() && ! materials[p.material].equalsIgnoreCase (m.toString().trim()); ++k)
                    if (materials[k].containsIgnoreCase (m.toString().trim()))
                    {
                        p.material = k;
                        break;
                    }
            }
            else
            {
                p.material = juce::jlimit (0, materials.size() - 1, (int) (double) m);
            }
        }

        p.colour = colourOf (item.getProperty ("color", item.getProperty ("colour", {})), p.colour);

        const auto surface = item.getProperty ("surface", "none").toString().trim().toLowerCase();
        p.surface = surface == "tolex" ? Surface::tolex : surface == "knurl" ? Surface::knurl : surface == "wood" ? Surface::wood
                  : surface == "perforated" ? Surface::perforated : surface == "rubber" ? Surface::rubber
                  : surface == "brushed" ? Surface::brushed : surface == "leather" ? Surface::leather
                  : surface == "grille" ? Surface::grille : Surface::none;
        p.surfaceScale = juce::jlimit (0.25f, 4.0f, numberOf (item, "surfaceScale", 1.0f));
        p.surfaceDepth = juce::jlimit (0.0f, 100.0f, numberOf (item, "surfaceDepth", 50.0f)) * 0.01f;
        p.glass = juce::jlimit (0.0f, 100.0f, numberOf (item, "glass", 0.0f)) * 0.01f;
        p.glow = colourOf (item.getProperty ("glow", {}), juce::Colour (0x00000000));
        p.glowStrength = juce::jlimit (0.0f, 100.0f, numberOf (item, "glowStrength", 60.0f)) * 0.01f;

        // Hollow, a face left open.
        p.shell = juce::jlimit (0.0f, std::min (p.hw, std::min (p.hh, p.hd)) * 0.9f, numberOf (item, "shell", 0.0f));
        {
            const auto open = item.getProperty ("open", "none").toString().trim().toLowerCase();
            p.open = juce::jmax (0, juce::StringArray { "none", "top", "bottom", "front", "back", "left", "right" }.indexOf (open));
        }

        // A model from a file: its distance field (no file: no part).
        if (p.kind == Kind::mesh)
        {
            const bool upZ = item.getProperty ("up", "y").toString().trim().equalsIgnoreCase ("z");
            p.mesh = MeshField::load (meshFile (item, images), upZ);

            if (p.mesh == nullptr)
                continue;

            const auto* e = p.mesh->extent;
            const float sx = 2.0f * p.hw / e[0], sy = 2.0f * p.hh / e[1], sz = 2.0f * p.hd / e[2];

            if (item.getProperty ("fit", "keep").toString().trim().equalsIgnoreCase ("stretch"))
            {
                p.meshScale[0] = sx; p.meshScale[1] = sy; p.meshScale[2] = sz;
            }
            else
            {
                p.meshScale[0] = p.meshScale[1] = p.meshScale[2] = std::min (sx, std::min (sy, sz));
            }
        }

        // A sweep: its path's field over its plane, its section.
        if (p.kind == Kind::sweep)
        {
            const float planeW = p.plane == Plane::side ? 2 * p.hd : 2 * p.hw;
            const float planeH = p.plane != Plane::top ? 2 * p.hh : 2 * p.hd;
            p.thickness = juce::jlimit (0.5f, 2000.0f, numberOf (item, "thickness", 10.0f));

            auto path = juce::Drawable::parseSVGPath (item.getProperty ("path", "M0 0.5 L1 0.5").toString());
            path.applyTransform (juce::AffineTransform::scale (planeW, planeH));
            p.field = pathFieldOf (path, planeW, planeH, p.thickness * 0.75f + 2.0f);

            const auto section = item.getProperty ("section", "round").toString().trim().toLowerCase();
            p.section = section == "square" ? 1 : section == "profile" ? 2 : 0;

            if (p.section == 2)
            {
                auto outline = juce::Drawable::parseSVGPath (item.getProperty ("profile", "M0 0 L1 0 L1 1 L0 1 Z").toString());
                outline.applyTransform (juce::AffineTransform::scale (p.thickness, p.thickness));
                p.sectionField = fieldOf (outline, p.thickness, p.thickness);
            }
        }

        // A loft: its two sections, how the end is sized and turned.
        if (p.kind == Kind::loft)
        {
            const float planeW = p.plane == Plane::side ? 2 * p.hd : 2 * p.hw;
            const float planeH = p.plane != Plane::top ? 2 * p.hh : 2 * p.hd;
            const float along = p.plane == Plane::top ? 2 * p.hh : (p.plane == Plane::front ? 2 * p.hd : 2 * p.hw);

            auto start = juce::Drawable::parseSVGPath (item.getProperty ("profile", "M0 0 L1 0 L1 1 L0 1 Z").toString());
            start.applyTransform (juce::AffineTransform::scale (planeW, planeH));
            p.field = fieldOf (start, planeW, planeH);

            if (const auto end = item.getProperty ("profileEnd", {}).toString().trim(); end.isNotEmpty())
            {
                auto outline = juce::Drawable::parseSVGPath (end);

                if (! outline.isEmpty())
                {
                    outline.applyTransform (juce::AffineTransform::scale (planeW, planeH));
                    p.fieldEnd = fieldOf (outline, planeW, planeH);
                }
            }

            p.taper = juce::jlimit (0.05f, 4.0f, numberOf (item, "taper", 100.0f) * 0.01f);
            p.twist = juce::jlimit (-720.0f, 720.0f, numberOf (item, "twist", 0.0f)) * pi / 180.0f;

            // Its sides lean (and turn): its distance measured a little short, so rays don't
            // step through them.
            const float radius = 0.5f * len2 (planeW, planeH);
            const float lean = (std::abs (1.0f - p.taper) * radius + std::abs (p.twist) * radius * std::max (1.0f, p.taper)) / std::max (1.0f, along);
            p.lipschitz = 1.0f / std::sqrt (1.0f + lean * lean) * (p.fieldEnd != nullptr ? 0.8f : 1.0f);
        }

        // A profile's (or the words') outline, as a distance field over its plane.
        if (p.kind == Kind::extrude || p.kind == Kind::revolve || p.kind == Kind::text)
        {
            const float planeW = p.kind == Kind::revolve ? p.hw : (p.plane == Plane::side ? 2 * p.hd : 2 * p.hw);
            const float planeH = p.kind == Kind::revolve || p.plane != Plane::top ? 2 * p.hh : 2 * p.hd;
            const float textW = 2 * p.hw, textH = 2 * p.hd;
            juce::Path outline;

            if (p.kind == Kind::text)
            {
                const auto words = item.getProperty ("text", "TEXT").toString();
                auto font = FontLibrary::font (item.getProperty ("font", {}).toString(), textH * 0.86f, (bool) item.getProperty ("bold", false));
                juce::GlyphArrangement glyphs;
                glyphs.addFittedText (font, words, 0.0f, 0.0f, textW, textH, juce::Justification::centred, 1, 0.6f);
                glyphs.createPath (outline);
                p.field = fieldOf (outline, textW, textH);
            }
            else
            {
                if (const auto svg = item.getProperty ("svg", {}).toString().trim(); svg.isNotEmpty() && images.isDirectory())
                {
                    outline = svgOutline (images.getChildFile (svg));

                    if (! outline.isEmpty())
                    {
                        const auto b = outline.getBounds();
                        outline.applyTransform (juce::AffineTransform::translation (-b.getX(), -b.getY())
                                                    .scaled (1.0f / juce::jmax (1.0e-3f, b.getWidth()), 1.0f / juce::jmax (1.0e-3f, b.getHeight())));
                    }
                }

                if (outline.isEmpty())
                    outline = juce::Drawable::parseSVGPath (item.getProperty ("profile", "M0 0 L1 0 L1 1 L0 1 Z").toString());

                outline.applyTransform (juce::AffineTransform::scale (planeW, planeH));
                p.field = fieldOf (outline, planeW, planeH);
            }
        }

        // Its box on the window, every copy of it: its own (with its copies), turned, mirrored.
        {
            float llo[3] { -p.hw, -p.hh, -p.hd }, lhi[3] { p.hw, p.hh, p.hd };

            if (p.repeatKind == 1 || p.repeatKind == 2)
            {
                const float ex = p.dx * (float) (p.count - 1), ez = p.dz * (float) (p.countZ - 1);
                llo[0] += std::min (0.0f, ex); lhi[0] += std::max (0.0f, ex);
                llo[2] += std::min (0.0f, ez); lhi[2] += std::max (0.0f, ez);
            }
            else if (p.repeatKind == 3)
            {
                const float r = p.radius + std::max (p.hw, p.hd);
                llo[0] = llo[2] = -r;
                lhi[0] = lhi[2] = r;
            }

            for (int k = 0; k < 3; ++k)
            {
                p.lo[k] = far;
                p.hi[k] = -far;
            }

            for (int c = 0; c < 8; ++c)
            {
                const float lx = (c & 1) ? lhi[0] : llo[0], ly = (c & 2) ? lhi[1] : llo[1], lz = (c & 4) ? lhi[2] : llo[2];

                // world = R local = rot^T local
                const float wx = p.rot[0] * lx + p.rot[3] * ly + p.rot[6] * lz;
                const float wy = p.rot[1] * lx + p.rot[4] * ly + p.rot[7] * lz;
                const float wz = p.rot[2] * lx + p.rot[5] * ly + p.rot[8] * lz;
                const float wv[3] { p.cx + wx, p.cy + wy, p.cz + wz };

                for (int k = 0; k < 3; ++k)
                {
                    p.lo[k] = std::min (p.lo[k], wv[k]);
                    p.hi[k] = std::max (p.hi[k], wv[k]);
                }
            }

            if (p.mirrorX)
            {
                const float a = body->mirrorAxis, l = p.lo[0], h = p.hi[0];
                p.lo[0] = std::min (l, 2.0f * a - h);
                p.hi[0] = std::max (h, 2.0f * a - l);
            }

            const float grow = p.blend + p.surfaceDepth * 2.0f + 1.0f + (p.kind == Kind::sweep ? p.thickness * 0.5f : 0.0f);

            for (int k = 0; k < 3; ++k)
            {
                p.lo[k] -= grow;
                p.hi[k] += grow;

                if (! p.cut)
                {
                    body->lo[k] = std::min (body->lo[k], p.lo[k]);
                    body->hi[k] = std::max (body->hi[k], p.hi[k]);
                }
            }
        }

        body->parts.push_back (std::move (p));
    }

    if (body->lo[0] > body->hi[0])
        return nullptr;   // nothing but cuts

    body->lo[1] = std::min (body->lo[1], 0.0f);
    return body;
}

//==============================================================================
float Body3D::relief (const Part& p, float x, float y, float z)
{
    if (p.surface == Surface::none || p.surfaceDepth <= 0.0f)
        return 0.0f;

    const float s = 1.0f / p.surfaceScale;
    const float depth = p.surfaceDepth;

    switch (p.surface)
    {
        case Surface::knurl:
        {
            // Diamonds: two sets of grooves crossing.
            const float k = 2.0f * pi / (6.0f * p.surfaceScale);
            const float a = std::abs (std::sin ((x + z) * k)), b = std::abs (std::sin ((x - z + y) * k));
            return -(1.0f - std::min (a, b)) * 0.9f * depth;
        }

        case Surface::tolex:
            return -(noise3 (x * 0.45f * s, y * 0.45f * s, z * 0.45f * s) * 0.7f + noise3 (x * 1.3f * s, y * 1.3f * s, z * 1.3f * s) * 0.3f) * 0.8f * depth;

        case Surface::leather:
            return -std::pow (noise3 (x * 0.22f * s, y * 0.22f * s, z * 0.22f * s), 2.0f) * 1.2f * depth;

        case Surface::rubber:
            return -noise3 (x * 0.9f * s, y * 0.9f * s, z * 0.9f * s) * 0.35f * depth;

        case Surface::perforated:
        {
            // Round holes in rows, offset every other row.
            const float pitch = 7.0f * p.surfaceScale;
            const float row = std::floor (z / pitch);
            const float ox = std::fmod (row, 2.0f) != 0.0f ? pitch * 0.5f : 0.0f;
            const float hx = std::fmod (std::abs (x + ox), pitch) - pitch * 0.5f, hz = std::fmod (std::abs (z), pitch) - pitch * 0.5f;
            const float r = pitch * 0.3f;
            return len2 (hx, hz) < r ? -2.5f * depth : 0.0f;
        }

        case Surface::grille:
        {
            // Slots across.
            const float pitch = 6.0f * p.surfaceScale;
            const float hz = std::fmod (std::abs (z), pitch) - pitch * 0.5f;
            return std::abs (hz) < pitch * 0.22f ? -2.5f * depth : 0.0f;
        }

        case Surface::wood:
        case Surface::brushed:
        case Surface::none:
        default:
            return 0.0f;
    }
}

float Body3D::partDistance (const Part& p, float x, float y, float z) const
{
    // Mirrored: the copy across the body's middle (mirrorAxis), on the side the part is on.
    if (p.mirrorX)
        x = mirrorAxis + (p.cx < mirrorAxis ? -std::abs (x - mirrorAxis) : std::abs (x - mirrorAxis));

    // Into its own: from its middle, turned back.
    float qx = x - p.cx, qy = y - p.cy, qz = z - p.cz;

    if (p.rotated)
    {
        const float rx = p.rot[0] * qx + p.rot[1] * qy + p.rot[2] * qz;
        const float ry = p.rot[3] * qx + p.rot[4] * qy + p.rot[5] * qz;
        const float rz = p.rot[6] * qx + p.rot[7] * qy + p.rot[8] * qz;
        qx = rx; qy = ry; qz = rz;
    }

    // Its copies: the nearest one.
    if (p.repeatKind == 1 || p.repeatKind == 2)
    {
        if (std::abs (p.dx) > 1.0e-4f && p.count > 1)
            qx -= p.dx * juce::jlimit (0.0f, (float) (p.count - 1), std::round (qx / p.dx));

        if (p.repeatKind == 2 && std::abs (p.dz) > 1.0e-4f && p.countZ > 1)
            qz -= p.dz * juce::jlimit (0.0f, (float) (p.countZ - 1), std::round (qz / p.dz));
    }
    else if (p.repeatKind == 3 && p.count > 0)
    {
        const float sector = 2.0f * pi / (float) p.count;
        float a = std::atan2 (qz, qx);
        a -= sector * std::round (a / sector);
        const float r = len2 (qx, qz);
        qx = r * std::cos (a) - p.radius;
        qz = r * std::sin (a);
    }

    float d = shapeDistance (p, qx, qy, qz);

    // Hollow: the same shape, its walls' thickness in from its surface, taken out of it; an
    // open face: the hollow carried on out through it.
    if (p.shell > 0.0f)
    {
        const float t = p.shell, deep = 2.0f * t;

        switch (p.open)
        {
            case 1:  qy = std::min (qy, p.hh - deep); break;
            case 2:  qy = std::max (qy, -p.hh + deep); break;
            case 3:  qz = std::min (qz, p.hd - deep); break;
            case 4:  qz = std::max (qz, -p.hd + deep); break;
            case 5:  qx = std::max (qx, -p.hw + deep); break;
            case 6:  qx = std::min (qx, p.hw - deep); break;
            default: break;
        }

        d = std::max (d, -(shapeDistance (p, qx, qy, qz) + t));
    }

    return d + relief (p, x, y, z);
}

float Body3D::shapeDistance (const Part& p, float qx, float qy, float qz)
{
    float d = far;
    const float r = std::min (p.round, std::min (p.hw, std::min (p.hh, p.hd)) * 0.98f);

    switch (p.kind)
    {
        case Kind::box:
        {
            const float ax = std::abs (qx) - (p.hw - r), ay = std::abs (qy) - (p.hh - r), az = std::abs (qz) - (p.hd - r);
            d = len3 (std::max (ax, 0.0f), std::max (ay, 0.0f), std::max (az, 0.0f)) + std::min (std::max (ax, std::max (ay, az)), 0.0f) - r;

            if (p.chamfer > 0.0f)
            {
                const float c = p.chamfer;
                const float e1 = std::abs (qx) + std::abs (qy) - (p.hw + p.hh - c);
                const float e2 = std::abs (qy) + std::abs (qz) - (p.hh + p.hd - c);
                const float e3 = std::abs (qx) + std::abs (qz) - (p.hw + p.hd - c);
                d = std::max (d, std::max (e1, std::max (e2, e3)) * 0.70710678f);
            }
            break;
        }

        case Kind::cylinder:
        {
            // Its axis upright, across or back to front; its section round (or oval).
            const float pa = p.axis == 0 ? qy : qx, pb = p.axis == 2 ? qy : qz, pl = p.axis == 0 ? qx : (p.axis == 2 ? qz : qy);
            const float ra = p.axis == 0 ? p.hh : p.hw, rb = p.axis == 2 ? p.hh : p.hd, hl = p.axis == 0 ? p.hw : (p.axis == 2 ? p.hd : p.hh);
            const float rr = std::min (ra, rb);
            const float rc = std::min (p.round, std::min (rr, hl) * 0.98f);
            const float dr = (len2 (pa / ra, pb / rb) - 1.0f) * rr;
            const float wx = dr + rc, wy = std::abs (pl) - hl + rc;
            d = std::min (std::max (wx, wy), 0.0f) + len2 (std::max (wx, 0.0f), std::max (wy, 0.0f)) - rc;

            if (p.chamfer > 0.0f)
                d = std::max (d, (dr + std::abs (pl) - hl + std::min (p.chamfer, std::min (rr, hl))) * 0.70710678f);
            break;
        }

        case Kind::sphere:
        {
            const float k0 = len3 (qx / p.hw, qy / p.hh, qz / p.hd);
            const float k1 = len3 (qx / (p.hw * p.hw), qy / (p.hh * p.hh), qz / (p.hd * p.hd));
            d = k1 > 1.0e-6f ? k0 * (k0 - 1.0f) / k1 : -std::min (p.hw, std::min (p.hh, p.hd));
            break;
        }

        case Kind::extrude:
        case Kind::text:
        {
            float u, v, a, half;

            if (p.kind == Kind::text || p.plane == Plane::top)  { u = qx + p.hw; v = qz + p.hd; a = qy; half = p.hh; }
            else if (p.plane == Plane::front)                    { u = qx + p.hw; v = p.hh - qy; a = qz; half = p.hd; }
            else                                                 { u = qz + p.hd; v = p.hh - qy; a = qx; half = p.hw; }

            const float d2 = p.field != nullptr ? p.field->at (u, v) : far;
            const float rc = std::min (r, half * 0.98f);
            const float wx = d2 + rc, wy = std::abs (a) - half + rc;
            d = std::min (std::max (wx, wy), 0.0f) + len2 (std::max (wx, 0.0f), std::max (wy, 0.0f)) - rc;

            if (p.chamfer > 0.0f)
                d = std::max (d, (d2 + std::abs (a) - half + std::min (p.chamfer, half)) * 0.70710678f);
            break;
        }

        case Kind::revolve:
        {
            const float radius = len2 (qx, qz * (p.hw / std::max (0.01f, p.hd)));
            d = p.field != nullptr ? p.field->at (radius, p.hh - qy) : far;
            break;
        }

        case Kind::sweep:
        {
            // How far across the path (signed by its side) and out of its plane: the section there.
            float u, v, a;

            if (p.plane == Plane::top)        { u = qx + p.hw; v = qz + p.hd; a = qy; }
            else if (p.plane == Plane::front) { u = qx + p.hw; v = p.hh - qy; a = qz; }
            else                              { u = qz + p.hd; v = p.hh - qy; a = qx; }

            const float across = p.field != nullptr ? p.field->at (u, v) : far;
            const float half = p.thickness * 0.5f;

            if (p.section == 0)
            {
                d = len2 (across, a) - half;
            }
            else if (p.section == 1)
            {
                const float rc = std::min (p.round, half * 0.98f);
                const float wx = std::abs (across) - half + rc, wy = std::abs (a) - half + rc;
                d = std::min (std::max (wx, wy), 0.0f) + len2 (std::max (wx, 0.0f), std::max (wy, 0.0f)) - rc;
            }
            else
            {
                d = p.sectionField != nullptr ? p.sectionField->at (across + half, half - a) : far;
            }
            break;
        }

        case Kind::loft:
        {
            float u, v, a, half, planeW, planeH;

            if (p.plane == Plane::top)        { u = qx + p.hw; v = qz + p.hd; a = qy; half = p.hh; planeW = 2 * p.hw; planeH = 2 * p.hd; }
            else if (p.plane == Plane::front) { u = qx + p.hw; v = p.hh - qy; a = qz; half = p.hd; planeW = 2 * p.hw; planeH = 2 * p.hh; }
            else                              { u = qz + p.hd; v = p.hh - qy; a = qx; half = p.hw; planeW = 2 * p.hd; planeH = 2 * p.hh; }

            // Along it (0 the start .. 1 the end): the section sized and turned as far.
            const float t = clamp01 ((a + half) / (2.0f * half));
            const float scale = mixf (1.0f, p.taper, t), turn = -p.twist * t;
            const float cu = planeW * 0.5f, cv = planeH * 0.5f;
            const float du = u - cu, dv = v - cv;
            const float ru = (du * std::cos (turn) - dv * std::sin (turn)) / scale, rv = (du * std::sin (turn) + dv * std::cos (turn)) / scale;
            const float ds = p.field != nullptr ? p.field->at (cu + ru, cv + rv) * scale : far;
            const float de = p.fieldEnd != nullptr ? p.fieldEnd->at (cu + ru, cv + rv) * scale : ds;
            const float d2 = mixf (ds, de, t) * p.lipschitz;

            const float rc = std::min (r, half * 0.98f);
            const float wx = d2 + rc, wy = std::abs (a) - half + rc;
            d = std::min (std::max (wx, wy), 0.0f) + len2 (std::max (wx, 0.0f), std::max (wy, 0.0f)) - rc;
            break;
        }

        case Kind::mesh:
        {
            if (p.mesh == nullptr)
                break;

            const float sx = p.meshScale[0], sy = p.meshScale[1], sz = p.meshScale[2];
            d = p.mesh->at (qx / sx, qy / sy, qz / sz) * std::min (sx, std::min (sy, sz));
            break;
        }
    }

    return d;
}

juce::File Body3D::meshFile (const juce::var& part, const juce::File& images)
{
    const auto name = part.getProperty ("file", {}).toString().trim();

    if (name.isEmpty())
        return {};

    if (juce::File::isAbsolutePath (name))
        return juce::File (name);

    return images.getParentDirectory().getChildFile (name);
}

float Body3D::distance (float x, float y, float z, int& part, bool solidOnly) const
{
    constexpr float frontTie = 0.5f;
    float d = far;
    part = -1;

    for (size_t i = 0; i < parts.size(); ++i)
    {
        const auto& p = parts[i];

        if (solidOnly && p.glass > 0.0f)
            continue;

        // How far its box is: no nearer than that.
        const float bx = std::max (0.0f, std::max (p.lo[0] - x, x - p.hi[0]));
        const float by = std::max (0.0f, std::max (p.lo[1] - y, y - p.hi[1]));
        const float bz = std::max (0.0f, std::max (p.lo[2] - z, z - p.hi[2]));
        const float bound = len3 (bx, by, bz);

        // (Inside its box it may be anywhere, inside it too: only outside does its box tell.)
        if (! p.cut)
        {
            if (bound > 0.0f && bound - p.blend > d + frontTie)
                continue;

            const float dp = partDistance (p, x, y, z);

            if (p.blend > 0.0f && d < far * 0.5f)
            {
                const float k = p.blend;
                const float h = clamp01 (0.5f + 0.5f * (d - dp) / k);

                if (h > 0.5f)
                    part = (int) i;

                d = mixf (d, dp, h) - k * h * (1.0f - h);
            }
            else if (dp < d + frontTie)
            {
                // Nearer, or a later part's surface on an earlier one's (within half a pixel):
                // the later part's in front, its colour and finish shown.
                d = std::min (d, dp);
                part = (int) i;
            }
        }
        else
        {
            if (d >= far * 0.5f || (bound > 0.0f && bound - p.blend >= -d))
                continue;   // nothing there to cut, or it's too far to cut it

            const float dc = -partDistance (p, x, y, z);

            if (p.blend > 0.0f)
            {
                const float k = p.blend;
                const float h = clamp01 (0.5f - 0.5f * (dc - d) / k);
                d = mixf (dc, d, h) + k * h * (1.0f - h);
            }
            else
            {
                d = std::max (d, dc);
            }
        }
    }

    return d;
}

void Body3D::surfaceAt (int part, float x, float y, float z, float& colourScale, float& roughAdd) const
{
    colourScale = 1.0f;
    roughAdd = 0.0f;

    if (! juce::isPositiveAndBelow (part, (int) parts.size()))
        return;

    const auto& p = parts[(size_t) part];
    const float s = 1.0f / p.surfaceScale;
    const float rel = relief (p, x, y, z);

    switch (p.surface)
    {
        case Surface::wood:
        {
            // Grain along its width: rings warped by noise.
            const float n = noise3 (x * 0.012f * s, y * 0.05f * s, z * 0.05f * s) * 6.0f;
            const float ring = 0.5f + 0.5f * std::sin ((z * 0.35f * s + n) * 2.0f);
            const float fine = noise3 (x * 0.6f * s, y * 0.6f * s, z * 3.0f * s);
            colourScale = 0.72f + 0.3f * ring + 0.1f * fine;
            roughAdd = 0.1f * (1.0f - ring);
            break;
        }

        case Surface::perforated:
        case Surface::grille:
            colourScale = rel < -0.5f * p.surfaceDepth ? 0.08f : 1.0f;
            break;

        case Surface::tolex:
        case Surface::leather:
            colourScale = 0.85f + 0.15f * clamp01 (1.0f + rel / std::max (0.01f, p.surfaceDepth));
            roughAdd = 0.12f;
            break;

        case Surface::rubber:
            roughAdd = 0.2f;
            break;

        case Surface::knurl:
            colourScale = 0.8f + 0.2f * clamp01 (1.0f + rel / std::max (0.01f, p.surfaceDepth));
            break;

        case Surface::brushed:
        case Surface::none:
        default:
            break;
    }
}
