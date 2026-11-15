// D:\workspace\Fanan_web_hub\src\MeshField.cpp

#include "MeshField.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <list>
#include <map>
#include <mutex>
#include <string>

namespace
{
    struct V { float x, y, z; };
    struct Tri { V a, b, c; };

    V sub (V a, V b)        { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    V add (V a, V b)        { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    V mul (V a, float s)    { return { a.x * s, a.y * s, a.z * s }; }
    float dot (V a, V b)    { return a.x * b.x + a.y * b.y + a.z * b.z; }
    float get (V v, int k)  { return k == 0 ? v.x : (k == 1 ? v.y : v.z); }

    /** The point of a triangle nearest p (Ericson, Real-Time Collision Detection 5.1.5). */
    V closest (V p, V a, V b, V c)
    {
        const V ab = sub (b, a), ac = sub (c, a), ap = sub (p, a);
        const float d1 = dot (ab, ap), d2 = dot (ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) return a;

        const V bp = sub (p, b);
        const float d3 = dot (ab, bp), d4 = dot (ac, bp);
        if (d3 >= 0.0f && d4 <= d3) return b;

        const float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) return add (a, mul (ab, d1 / (d1 - d3)));

        const V cp = sub (p, c);
        const float d5 = dot (ab, cp), d6 = dot (ac, cp);
        if (d6 >= 0.0f && d5 <= d6) return c;

        const float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) return add (a, mul (ac, d2 / (d2 - d6)));

        const float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
            return add (b, mul (sub (c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));

        const float denom = 1.0f / (va + vb + vc);
        return add (a, add (mul (ab, vb * denom), mul (ac, vc * denom)));
    }

    //==========================================================================
    bool readObj (const juce::File& file, std::vector<Tri>& tris)
    {
        juce::FileInputStream in (file);

        if (! in.openedOk())
            return false;

        std::vector<V> points;

        while (! in.isExhausted())
        {
            const auto line = in.readNextLine().trim();

            if (line.startsWith ("v "))
            {
                juce::StringArray t;
                t.addTokens (line.substring (2), " \t", {});
                t.removeEmptyStrings();

                if (t.size() >= 3)
                    points.push_back ({ t[0].getFloatValue(), t[1].getFloatValue(), t[2].getFloatValue() });
            }
            else if (line.startsWith ("f "))
            {
                juce::StringArray t;
                t.addTokens (line.substring (2), " \t", {});
                t.removeEmptyStrings();
                std::vector<int> face;

                for (const auto& token : t)
                {
                    int index = token.upToFirstOccurrenceOf ("/", false, false).getIntValue();

                    if (index < 0)
                        index = (int) points.size() + index + 1;

                    if (index >= 1 && index <= (int) points.size())
                        face.push_back (index - 1);
                }

                // A polygon: a fan of triangles.
                for (size_t k = 2; k < face.size(); ++k)
                    tris.push_back ({ points[(size_t) face[0]], points[(size_t) face[k - 1]], points[(size_t) face[k]] });
            }

            if (tris.size() > 4000000)
                break;
        }

        return ! tris.empty();
    }

    bool readStl (const juce::File& file, std::vector<Tri>& tris)
    {
        juce::MemoryBlock data;

        if (! file.loadFileAsData (data) || data.getSize() < 84)
            return false;

        const auto* bytes = static_cast<const unsigned char*> (data.getData());
        const auto count = (juce::uint32) bytes[80] | ((juce::uint32) bytes[81] << 8) | ((juce::uint32) bytes[82] << 16) | ((juce::uint32) bytes[83] << 24);

        // Binary: its size says so (a binary file may begin with "solid" too).
        if ((juce::uint64) data.getSize() == 84ull + 50ull * (juce::uint64) count)
        {
            auto floatAt = [bytes] (size_t at)
            {
                float f;
                std::memcpy (&f, bytes + at, 4);
                return f;
            };

            tris.reserve (count);

            for (juce::uint32 i = 0; i < count && i < 4000000; ++i)
            {
                const size_t at = 84 + (size_t) i * 50 + 12;   // past its normal
                tris.push_back ({ { floatAt (at), floatAt (at + 4), floatAt (at + 8) },
                                  { floatAt (at + 12), floatAt (at + 16), floatAt (at + 20) },
                                  { floatAt (at + 24), floatAt (at + 28), floatAt (at + 32) } });
            }

            return ! tris.empty();
        }

        // Text: "vertex x y z", three a facet.
        const auto text = data.toString();
        juce::StringArray lines;
        lines.addLines (text);
        std::vector<V> corner;

        for (const auto& raw : lines)
        {
            const auto line = raw.trim();

            if (! line.startsWithIgnoreCase ("vertex"))
                continue;

            juce::StringArray t;
            t.addTokens (line.substring (6), " \t", {});
            t.removeEmptyStrings();

            if (t.size() >= 3)
                corner.push_back ({ t[0].getFloatValue(), t[1].getFloatValue(), t[2].getFloatValue() });

            if (corner.size() == 3)
            {
                tris.push_back ({ corner[0], corner[1], corner[2] });
                corner.clear();
            }
        }

        return ! tris.empty();
    }

    //==========================================================================
    std::shared_ptr<MeshField::Grid> measure (std::vector<Tri>& tris)
    {
        // Centred, its longest side 1.
        V lo { 1.0e30f, 1.0e30f, 1.0e30f }, hi { -1.0e30f, -1.0e30f, -1.0e30f };

        for (const auto& t : tris)
            for (const auto& p : { t.a, t.b, t.c })
            {
                lo = { std::min (lo.x, p.x), std::min (lo.y, p.y), std::min (lo.z, p.z) };
                hi = { std::max (hi.x, p.x), std::max (hi.y, p.y), std::max (hi.z, p.z) };
            }

        const V size = sub (hi, lo);
        const float longest = std::max (size.x, std::max (size.y, size.z));

        if (! (longest > 0.0f) || ! std::isfinite (longest))
            return nullptr;

        const V middle = mul (add (lo, hi), 0.5f);

        for (auto& t : tris)
        {
            t.a = mul (sub (t.a, middle), 1.0f / longest);
            t.b = mul (sub (t.b, middle), 1.0f / longest);
            t.c = mul (sub (t.c, middle), 1.0f / longest);
        }

        auto grid = std::make_shared<MeshField::Grid>();
        grid->triangles = (int) tris.size();
        grid->extent[0] = std::max (1.0e-3f, size.x / longest);
        grid->extent[1] = std::max (1.0e-3f, size.y / longest);
        grid->extent[2] = std::max (1.0e-3f, size.z / longest);

        // Fine enough for a body part: 96 cells along its longest side, 3 more round it.
        const int along = 96, margin = 3;
        grid->cell = 1.0f / (float) along;
        int* counts[3] { &grid->nx, &grid->ny, &grid->nz };

        for (int k = 0; k < 3; ++k)
        {
            *counts[k] = (int) std::ceil (grid->extent[k] / grid->cell) + 2 * margin + 1;
            grid->lo[k] = -grid->extent[k] * 0.5f - (float) margin * grid->cell;
        }

        const int nx = grid->nx, ny = grid->ny, nz = grid->nz;
        const float cell = grid->cell;
        const size_t n = (size_t) nx * (size_t) ny * (size_t) nz;
        auto index = [nx, ny] (int x, int y, int z) { return ((size_t) z * (size_t) ny + (size_t) y) * (size_t) nx + (size_t) x; };
        auto pointAt = [&grid, cell] (int x, int y, int z) { return V { grid->lo[0] + (float) x * cell, grid->lo[1] + (float) y * cell, grid->lo[2] + (float) z * cell }; };

        // How far from the surface: exactly near it (two cells round each triangle)...
        const float far = 1.0e9f;
        std::vector<float> dist (n, far);
        std::vector<char> exact (n, 0);

        for (const auto& t : tris)
        {
            int i0[3], i1[3];

            for (int k = 0; k < 3; ++k)
            {
                const float a = std::min (get (t.a, k), std::min (get (t.b, k), get (t.c, k)));
                const float b = std::max (get (t.a, k), std::max (get (t.b, k), get (t.c, k)));
                const int count = *counts[k];
                i0[k] = juce::jlimit (0, count - 1, (int) std::floor ((a - grid->lo[k]) / cell) - 2);
                i1[k] = juce::jlimit (0, count - 1, (int) std::ceil ((b - grid->lo[k]) / cell) + 2);
            }

            for (int z = i0[2]; z <= i1[2]; ++z)
                for (int y = i0[1]; y <= i1[1]; ++y)
                    for (int x = i0[0]; x <= i1[0]; ++x)
                    {
                        const V p = pointAt (x, y, z);
                        const V q = sub (p, closest (p, t.a, t.b, t.c));
                        const float d = std::sqrt (dot (q, q));
                        const auto i = index (x, y, z);

                        if (d < dist[i])
                        {
                            dist[i] = d;
                            exact[i] = 1;
                        }
                    }
        }

        // ...and further out, along the grid (forwards, then back): a little under the truth,
        // so a ray never steps past the surface.
        const float w1 = cell, w2 = cell * 1.41421356f, w3 = cell * 1.7320508f;

        for (int pass = 0; pass < 2; ++pass)
        {
            const int step = pass == 0 ? 1 : -1;
            const int zs = pass == 0 ? 0 : nz - 1, ys = pass == 0 ? 0 : ny - 1, xs = pass == 0 ? 0 : nx - 1;

            for (int z = zs; z >= 0 && z < nz; z += step)
                for (int y = ys; y >= 0 && y < ny; y += step)
                    for (int x = xs; x >= 0 && x < nx; x += step)
                    {
                        const auto i = index (x, y, z);

                        if (exact[i])
                            continue;

                        float best = dist[i];

                        for (int dz = -1; dz <= 0; ++dz)
                            for (int dy = -1; dy <= 1; ++dy)
                                for (int dx = -1; dx <= 1; ++dx)
                                {
                                    if (dz == 0 && (dy > 0 || (dy == 0 && dx >= 0)))
                                        continue;   // only the neighbours already passed

                                    const int ox = x + dx * step, oy = y + dy * step, oz = z + dz * step;

                                    if (ox < 0 || oy < 0 || oz < 0 || ox >= nx || oy >= ny || oz >= nz)
                                        continue;

                                    const int kind = std::abs (dx) + std::abs (dy) + std::abs (dz);
                                    best = std::min (best, dist[index (ox, oy, oz)] + (kind == 1 ? w1 : kind == 2 ? w2 : w3));
                                }

                        dist[i] = best;
                    }
        }

        for (size_t i = 0; i < n; ++i)
            if (! exact[i])
                dist[i] *= 0.92f;

        // Inside or out: rays along each axis counting the surfaces they cross, the majority
        // of the three deciding (a model that isn't quite closed still comes out whole).
        std::vector<unsigned char> votes (n, 0);
        const float nudge[3] { 1.7e-5f * 3.14159f, 1.3e-5f * 2.71828f, 1.1e-5f * 1.41421f };

        for (int axis = 0; axis < 3; ++axis)
        {
            const int b = (axis + 1) % 3, c = (axis + 2) % 3;
            const int na = *counts[axis], nb = *counts[b], nc = *counts[c];
            std::vector<int> crossings (n, 0);

            auto at = [&] (int ia, int ib, int ic)
            {
                int xyz[3];
                xyz[axis] = ia; xyz[b] = ib; xyz[c] = ic;
                return index (xyz[0], xyz[1], xyz[2]);
            };

            for (const auto& t : tris)
            {
                const double ab0 = get (t.a, b), ac0 = get (t.a, c), bb0 = get (t.b, b), bc0 = get (t.b, c), cb0 = get (t.c, b), cc0 = get (t.c, c);
                const double area = (bb0 - ab0) * (cc0 - ac0) - (cb0 - ab0) * (bc0 - ac0);

                if (std::abs (area) < 1.0e-18)
                    continue;   // edge on to the rays

                const int jb0 = std::max (0, (int) std::floor ((std::min (ab0, std::min (bb0, cb0)) - grid->lo[b]) / cell));
                const int jb1 = std::min (nb - 1, (int) std::ceil ((std::max (ab0, std::max (bb0, cb0)) - grid->lo[b]) / cell));
                const int jc0 = std::max (0, (int) std::floor ((std::min (ac0, std::min (bc0, cc0)) - grid->lo[c]) / cell));
                const int jc1 = std::min (nc - 1, (int) std::ceil ((std::max (ac0, std::max (bc0, cc0)) - grid->lo[c]) / cell));

                for (int jc = jc0; jc <= jc1; ++jc)
                    for (int jb = jb0; jb <= jb1; ++jb)
                    {
                        const double pb = grid->lo[b] + jb * cell + nudge[b], pc = grid->lo[c] + jc * cell + nudge[c];
                        const double w0 = ((bb0 - pb) * (cc0 - pc) - (cb0 - pb) * (bc0 - pc)) / area;
                        const double w1b = ((cb0 - pb) * (ac0 - pc) - (ab0 - pb) * (cc0 - pc)) / area;
                        const double w2b = 1.0 - w0 - w1b;

                        if (w0 < 0.0 || w1b < 0.0 || w2b < 0.0)
                            continue;

                        const double hit = w0 * get (t.a, axis) + w1b * get (t.b, axis) + w2b * get (t.c, axis);
                        const int first = (int) std::ceil ((hit - grid->lo[axis]) / cell);   // the first point past it

                        if (first < na)
                            ++crossings[at (std::max (0, first), jb, jc)];
                    }
            }

            for (int jc = 0; jc < nc; ++jc)
                for (int jb = 0; jb < nb; ++jb)
                {
                    int sum = 0;

                    for (int ia = 0; ia < na; ++ia)
                    {
                        const auto i = at (ia, jb, jc);
                        sum += crossings[i];

                        if (sum & 1)
                            ++votes[i];
                    }
                }
        }

        grid->d.resize (n);

        for (size_t i = 0; i < n; ++i)
            grid->d[i] = votes[i] >= 2 ? -dist[i] : dist[i];

        return grid;
    }

    //==========================================================================
    struct Cache
    {
        std::mutex lock;
        std::map<std::string, std::shared_ptr<const MeshField::Grid>> items;
        std::list<std::string> order;
    };

    Cache& cache()
    {
        static Cache c;
        return c;
    }
}

//==============================================================================
float MeshField::Grid::at (float x, float y, float z) const
{
    if (nx < 2 || ny < 2 || nz < 2)
        return 1.0e9f;

    const float gx = (x - lo[0]) / cell, gy = (y - lo[1]) / cell, gz = (z - lo[2]) / cell;
    const float cx = juce::jlimit (0.0f, (float) (nx - 1), gx), cy = juce::jlimit (0.0f, (float) (ny - 1), gy), cz = juce::jlimit (0.0f, (float) (nz - 1), gz);
    const float past = std::sqrt ((gx - cx) * (gx - cx) + (gy - cy) * (gy - cy) + (gz - cz) * (gz - cz)) * cell;

    const int ix = std::min (nx - 2, (int) cx), iy = std::min (ny - 2, (int) cy), iz = std::min (nz - 2, (int) cz);
    const float fx = cx - (float) ix, fy = cy - (float) iy, fz = cz - (float) iz;

    auto v = [this] (int a, int b, int c) { return d[((size_t) c * (size_t) ny + (size_t) b) * (size_t) nx + (size_t) a]; };
    auto lerp = [] (float a, float b, float t) { return a + (b - a) * t; };

    const float c00 = lerp (v (ix, iy, iz), v (ix + 1, iy, iz), fx), c10 = lerp (v (ix, iy + 1, iz), v (ix + 1, iy + 1, iz), fx);
    const float c01 = lerp (v (ix, iy, iz + 1), v (ix + 1, iy, iz + 1), fx), c11 = lerp (v (ix, iy + 1, iz + 1), v (ix + 1, iy + 1, iz + 1), fx);
    return lerp (lerp (c00, c10, fy), lerp (c01, c11, fy), fz) + past;
}

std::shared_ptr<const MeshField::Grid> MeshField::load (const juce::File& file, bool upZ, juce::String* error)
{
    if (! file.existsAsFile())
    {
        if (error != nullptr) *error = "There's no file " + file.getFullPathName();
        return nullptr;
    }

    const auto key = (file.getFullPathName() + "|" + juce::String (file.getLastModificationTime().toMilliseconds()) + "|"
                      + juce::String (file.getSize()) + (upZ ? "|z" : "|y")).toStdString();

    {
        auto& c = cache();
        const std::lock_guard<std::mutex> hold (c.lock);

        if (const auto found = c.items.find (key); found != c.items.end())
            return found->second;
    }

    std::vector<Tri> tris;
    const auto extension = file.getFileExtension().toLowerCase();
    const bool read = extension == ".stl" ? readStl (file, tris) : readObj (file, tris);

    if (! read)
    {
        if (error != nullptr) *error = "No triangles could be read from " + file.getFileName() + " (OBJ and STL files only)";
        return nullptr;
    }

    // Standing on z (CAD, most STL files): turned to stand on y, as the body does.
    if (upZ)
        for (auto& t : tris)
            for (auto* p : { &t.a, &t.b, &t.c })
                *p = { p->x, p->z, -p->y };

    std::shared_ptr<const MeshField::Grid> grid = measure (tris);

    if (grid == nullptr)
    {
        if (error != nullptr) *error = file.getFileName() + " has no size";
        return nullptr;
    }

    auto& c = cache();
    const std::lock_guard<std::mutex> hold (c.lock);
    c.items[key] = grid;
    c.order.push_back (key);

    while (c.order.size() > 8)
    {
        c.items.erase (c.order.front());
        c.order.pop_front();
    }

    return grid;
}
