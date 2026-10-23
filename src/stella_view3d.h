// D:\workspace\Fanan_web_hub\src\stella_view3d.h
//
// A small 3D view for programmed elements, part of the elements' runtime: the 3D displays
// (stella_builtin_3d.cpp) are drawn with it, and an element of a project's own can be too:
// #include "stella_view3d.h", then stella::ui::view3d.
//
// A camera the user turns by dragging, points seen in perspective, and terrains: rows of heights
// stacked in depth, drawn front to back so nearer rows hide the ones behind them (the waterfall
// look). The scene is a box: x -1..1 across, y -1..1 up, z -1 (front) .. 1 (back).
#pragma once
#include "stella_element_api.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace stella::ui::view3d
{
    inline float clamp (float v, float lo, float hi) noexcept    { return v < lo ? lo : (v > hi ? hi : v); }

    /** "#38E0D0", "38e0d0" or "0xff38e0d0" from a setting; the fallback when there's none. */
    inline Colour colourFrom (const std::string& text, Colour fallback)
    {
        std::string hex;

        for (const char c : text)
            if (std::isxdigit ((unsigned char) c))
                hex += c;

        if (hex.size() > 2 && hex[0] == '0' && (text.find ('x') != std::string::npos || text.find ('X') != std::string::npos))
            hex = hex.substr (1);

        if (hex.size() == 6)
            return 0xff000000u | (Colour) std::strtoul (hex.c_str(), nullptr, 16);

        if (hex.size() == 8)
            return (Colour) std::strtoul (hex.c_str(), nullptr, 16);

        return fallback;
    }

    //==========================================================================
    /** Where the scene is seen from: turned (yaw), tilted (pitch, looking down) and zoomed. fit
        sizes the scene to the element (call it at the start of each paint). */
    struct Camera
    {
        float yaw = -0.24f, pitch = 0.42f, zoom = 1.0f;
        float width = 100.0f, height = 100.0f;
        float scale = 50.0f, ox = 50.0f, oy = 50.0f;   // set by fit

        struct Point { float x, y, depth; };

        /** In perspective, before fitting: x across, y down the screen, and the depth. */
        Point raw (float x, float y, float z) const noexcept
        {
            const float cy = std::cos (yaw), sy = std::sin (yaw);
            const float x1 = x * cy - z * sy, z1 = x * sy + z * cy;
            const float cp = std::cos (pitch), sp = std::sin (pitch);
            const float y2 = y * cp + z1 * sp;    // further back stands higher on the screen
            const float z2 = z1 * cp - y * sp;    // the distance from the eye
            const float p = 4.5f / (4.5f + z2);   // perspective: far is smaller
            return { x1 * p, -y2 * p, z2 };
        }

        Point project (float x, float y, float z) const noexcept
        {
            const auto r = raw (x, y, z);
            return { ox + r.x * scale, oy + r.y * scale, r.depth };
        }

        /** The scene's box (x and z -1..1, y from low to high) fitted to the element, leaving
            room around it (bottom: for a caption). */
        void fit (float low, float high, float margin = 8.0f, float bottomMargin = 20.0f) noexcept
        {
            float minX = 1.0e9f, maxX = -1.0e9f, minY = 1.0e9f, maxY = -1.0e9f;

            for (const float x : { -1.0f, 1.0f })
                for (const float y : { low, high })
                    for (const float z : { -1.0f, 1.0f })
                    {
                        const auto r = raw (x, y, z);
                        minX = std::fmin (minX, r.x); maxX = std::fmax (maxX, r.x);
                        minY = std::fmin (minY, r.y); maxY = std::fmax (maxY, r.y);
                    }

            const float roomX = std::fmax (10.0f, width - 2.0f * margin), roomY = std::fmax (10.0f, height - margin - bottomMargin);
            scale = std::fmin (roomX / std::fmax (maxX - minX, 1.0e-3f), roomY / std::fmax (maxY - minY, 1.0e-3f)) * zoom;
            ox = width * 0.5f - 0.5f * (minX + maxX) * scale;
            oy = margin + roomY * 0.5f - 0.5f * (minY + maxY) * scale;
        }
    };

    /** Turning the view with the mouse: drag to turn and tilt (Shift: finely), the wheel zooms,
        a double-click goes back to how it was. */
    struct Orbit
    {
        float homeYaw = -0.24f, homePitch = 0.42f;
        float startYaw = 0.0f, startPitch = 0.0f, startX = 0.0f, startY = 0.0f;

        void down (Camera& cam, const Mouse& m) noexcept
        {
            if (m.doubleClick)
            {
                cam.yaw = homeYaw;
                cam.pitch = homePitch;
                cam.zoom = 1.0f;
            }

            startYaw = cam.yaw; startPitch = cam.pitch; startX = m.x; startY = m.y;
        }

        void drag (Camera& cam, const Mouse& m) noexcept
        {
            const float fine = m.shift ? 0.25f : 1.0f;
            cam.yaw = clamp (startYaw + (m.x - startX) * 0.008f * fine, -1.2f, 1.2f);
            cam.pitch = clamp (startPitch + (m.y - startY) * 0.006f * fine, 0.10f, 1.30f);
        }

        void wheel (Camera& cam, float notches) noexcept
        {
            cam.zoom = clamp (cam.zoom * std::pow (1.12f, notches), 0.6f, 1.8f);
        }
    };

    //==========================================================================
    /** A line through points (x, y pairs), thickness wide, as thin polygons a few points long:
        much cheaper than Canvas::drawPolyline's round pieces (the canvas works on each shape's
        bounding box, so short pieces cost little), for the hundreds of lines a 3D view draws. */
    inline void ribbon (Canvas& g, const float* xy, int n, float thickness, Colour c, std::vector<float>& scratch)
    {
        if (n < 2 || thickness <= 0.0f || (c >> 24) == 0)
            return;

        const float half = thickness * 0.5f;
        scratch.resize ((size_t) n * 2);

        // Each point's offset across the line (the average direction of its two pieces).
        for (int i = 0; i < n; ++i)
        {
            const int a = i > 0 ? i - 1 : i, b = i < n - 1 ? i + 1 : i;
            float dx = xy[b * 2] - xy[a * 2], dy = xy[b * 2 + 1] - xy[a * 2 + 1];
            const float length = std::sqrt (dx * dx + dy * dy);

            if (length < 1.0e-4f) { dx = 1.0f; dy = 0.0f; }
            else                  { dx /= length; dy /= length; }

            scratch[(size_t) i * 2] = -dy * half;
            scratch[(size_t) i * 2 + 1] = dx * half;
        }

        constexpr int chunk = 8;
        float poly[(chunk + 1) * 4];

        for (int from = 0; from < n - 1; from += chunk)
        {
            const int to = std::min (n - 1, from + chunk), count = to - from + 1;

            for (int k = 0; k < count; ++k)
            {
                const int i = from + k, back = 2 * count - 1 - k;
                poly[k * 2] = xy[i * 2] + scratch[(size_t) i * 2];
                poly[k * 2 + 1] = xy[i * 2 + 1] + scratch[(size_t) i * 2 + 1];
                poly[back * 2] = xy[i * 2] - scratch[(size_t) i * 2];
                poly[back * 2 + 1] = xy[i * 2 + 1] - scratch[(size_t) i * 2 + 1];
            }

            g.fillPolygon (poly, 2 * count, c);
        }
    }

    //==========================================================================
    /** How one row of a terrain is drawn. */
    struct RowLook
    {
        Colour line = 0xff38e0d0;
        float thickness = 1.0f;
        Colour glow = 0;            // a soft halo around the line (0: none)
        float glowWidth = 7.0f;
        Colour cross = 0;           // the lines across the rows, from this row to the next one back (0: none)
        float xray = 0.0f;          // the parts hidden behind nearer rows, shown faintly on top (0: not at all)
    };

    /** Rows of heights (rows x points, each about -1..1), row 0 at the front, stacked to the
        back. height: how tall a height of 1 stands; floor: the y the rows stand on. lookOf (row)
        says how each row is drawn; crossEvery: a line across the rows every so many points
        (0: none), for a wireframe surface.

        Drawn front to back with a horizon: a part of a row is drawn only where it rises above
        every row in front of it, so what's behind is hidden, and nothing is filled (cheap, even
        with dozens of rows). */
    template <typename LookOf>
    void terrain (Canvas& g, const Camera& cam, const float* heights, int rows, int points, float height, float floor,
                  LookOf lookOf, int crossEvery = 0)
    {
        if (rows < 1 || points < 2)
            return;

        const int columns = (int) std::ceil (cam.width) + 1;
        const int step = std::max (1, (int) (cam.width / 260.0f));   // every column, or every other one on a wide view
        std::vector<float> horizon ((size_t) columns, 1.0e9f);
        std::vector<float> px ((size_t) points), py ((size_t) points), colY ((size_t) columns), piece, scratch;
        std::vector<float> crossX, crossY, lastCrossX, lastCrossY;
        std::vector<char> crossSeen, lastCrossSeen;
        std::vector<std::pair<Colour, std::vector<float>>> xrays;   // hidden parts to show last
        const int crossCount = crossEvery > 0 ? (points - 1) / crossEvery + 1 : 0;

        for (int r = 0; r < rows; ++r)
        {
            const float z = rows == 1 ? 0.0f : -1.0f + 2.0f * (float) r / (float) (rows - 1);
            const float* row = heights + (size_t) r * (size_t) points;

            for (int i = 0; i < points; ++i)
            {
                const auto p = cam.project (-1.0f + 2.0f * (float) i / (float) (points - 1), floor + (row[i] + 1.0f) * height, z);
                px[(size_t) i] = p.x;
                py[(size_t) i] = p.y;
            }

            const bool reversed = px.back() < px.front();
            const float left = reversed ? px.back() : px.front(), right = reversed ? px.front() : px.back();
            const int c0 = std::max (0, (int) std::ceil (left / (float) step) * step), c1 = std::min (columns - 1, (int) std::floor (right));

            // The row's height on the screen at each column it crosses.
            const auto at = [reversed, points] (int k) { return reversed ? points - 1 - k : k; };

            for (int c = c0, i = 0; c <= c1; c += step)
            {
                while (i < points - 2 && px[(size_t) at (i + 1)] < (float) c)
                    ++i;

                const float xa = px[(size_t) at (i)], xb = px[(size_t) at (i + 1)];
                const float t = xb > xa ? clamp (((float) c - xa) / (xb - xa), 0.0f, 1.0f) : 0.0f;
                colY[(size_t) c] = py[(size_t) at (i)] + (py[(size_t) at (i + 1)] - py[(size_t) at (i)]) * t;
            }

            const RowLook look = lookOf (r);

            // The visible stretches: above the horizon.
            auto drawPiece = [&]
            {
                const int n = (int) piece.size() / 2;

                if (n >= 2)
                {
                    if (look.glow != 0)
                        ribbon (g, piece.data(), n, look.glowWidth, look.glow, scratch);

                    ribbon (g, piece.data(), n, look.thickness, look.line, scratch);
                }

                piece.clear();
            };

            // Its hidden parts, when they're to show through (drawn over everything at the end).
            if (look.xray > 0.0f)
            {
                std::vector<float> hidden;

                for (int c = c0; c <= c1; c += step)
                {
                    if (colY[(size_t) c] >= horizon[(size_t) c] - 0.3f)
                    {
                        hidden.push_back ((float) c);
                        hidden.push_back (colY[(size_t) c]);
                    }
                    else if (! hidden.empty())
                    {
                        hidden.push_back ((float) c);
                        hidden.push_back (colY[(size_t) c]);
                        xrays.push_back ({ withAlpha (look.line, look.xray), hidden });
                        hidden.clear();
                    }
                }

                if (hidden.size() >= 4)
                    xrays.push_back ({ withAlpha (look.line, look.xray), hidden });
            }

            for (int c = c0; c <= c1; c += step)
            {
                if (colY[(size_t) c] < horizon[(size_t) c] - 0.3f)
                {
                    piece.push_back ((float) c);
                    piece.push_back (colY[(size_t) c]);
                }
                else
                {
                    if (! piece.empty())   // ends where it goes behind
                    {
                        piece.push_back ((float) c);
                        piece.push_back (std::fmin (colY[(size_t) c], horizon[(size_t) c]));
                    }

                    drawPiece();
                }
            }

            drawPiece();

            // The lines across: from this row's points to the same points on the row in front.
            if (crossCount > 0)
            {
                crossX.assign ((size_t) crossCount, 0.0f);
                crossY.assign ((size_t) crossCount, 0.0f);
                crossSeen.assign ((size_t) crossCount, 0);

                for (int k = 0; k < crossCount; ++k)
                {
                    const int i = std::min (points - 1, k * crossEvery);
                    const int c = std::min (columns - 1, std::max (0, (int) std::lround (px[(size_t) i] / (float) step) * step));
                    crossX[(size_t) k] = px[(size_t) i];
                    crossY[(size_t) k] = py[(size_t) i];
                    crossSeen[(size_t) k] = px[(size_t) i] >= 0.0f && px[(size_t) i] < (float) columns && py[(size_t) i] < horizon[(size_t) c] - 0.3f ? 1 : 0;

                    if (r > 0 && look.cross != 0 && crossSeen[(size_t) k] && lastCrossSeen[(size_t) k])
                    {
                        const float xy[] { lastCrossX[(size_t) k], lastCrossY[(size_t) k], crossX[(size_t) k], crossY[(size_t) k] };
                        ribbon (g, xy, 2, 1.0f, look.cross, scratch);
                    }
                }

                lastCrossX = crossX; lastCrossY = crossY; lastCrossSeen = crossSeen;
            }

            for (int c = c0; c <= c1; c += step)
                horizon[(size_t) c] = std::fmin (horizon[(size_t) c], colY[(size_t) c]);
        }

        for (const auto& [c, xy] : xrays)
            if (xy.size() >= 4)
                ribbon (g, xy.data(), (int) xy.size() / 2, 1.2f, c, scratch);
    }

    /** The stage: a dark gradient, and a faint grid on the floor for depth. */
    inline void stage (Canvas& g, const Camera& cam, Colour top, Colour bottom, Colour grid, float floor)
    {
        g.fillGradient (0.0f, 0.0f, cam.width, cam.height, top, bottom);
        std::vector<float> scratch;

        for (int k = 0; k <= 8; ++k)
        {
            const float t = -1.0f + 0.25f * (float) k;
            const auto a = cam.project (-1.0f, floor, t), b = cam.project (1.0f, floor, t);
            const auto c = cam.project (t, floor, -1.0f), d = cam.project (t, floor, 1.0f);
            const float across[] { a.x, a.y, b.x, b.y }, deep[] { c.x, c.y, d.x, d.y };
            ribbon (g, across, 2, 1.0f, grid, scratch);
            ribbon (g, deep, 2, 1.0f, grid, scratch);
        }
    }

    /** Smooth movement for a value read every frame: quick to follow, no jumps. */
    inline float glide (float shown, float target, float speed = 0.35f) noexcept
    {
        return shown + (target - shown) * speed;
    }
}
