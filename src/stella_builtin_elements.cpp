// D:\workspace\Fanan_web_hub\src\stella_builtin_elements.cpp
//
// The toolbox's own elements that are made like programmed ones (stella_element_api.h),
// compiled with the elements' runtime into the studio's module and into every exported
// plugin that uses them. A project's own element of the same name takes a built-in's place.
//
//   DropDown      a drop-down list: "type": "dropdown" in a layout
//   RangeSlider   one track, two handles: a high-pass and a low-pass, a range's two ends
//   MeterKnob     a big knob on a dial plate with a meter's window above it
//   DualKnob      two concentric knobs: an outer ring and an inner cap
//   ModMatrix     the studio's modulation matrix, as an exported plugin draws it
//   PresetPanel   the studio's preset managers (drop list, table, side panel), as an exported plugin draws them

#include "stella_elements.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace stella::ui::builtin
{
    //==========================================================================
    /** A drop-down list for a whole-number parameter (0..N-1): the chosen option in a box,
        and a click opens the list under it. A long list opens in columns, so it never runs
        off the window: "settings": { "rows": 12 } is the most in one column. */
    class DropDown : public Element
    {
    public:
        void paint (Canvas& g) override
        {
            const auto accent = colour (0xffe5484d);
            const auto top = captionHeight();

            if (top > 0.0f)
                g.drawText (label(), 0.0f, 0.0f, (float) width(), top, 12.5f, 0xffd8d4ca, Align::left);

            const float x = 0.5f, y = top + 0.5f, w = (float) width() - 1.0f, h = (float) height() - top - 1.0f;
            const auto open = isPopupOpen();
            const float textSize = std::min (13.5f, std::max (9.0f, h * 0.45f));

            // Only its words: the chosen option, and a small arrow after it; lit while hovered.
            if (settingText ("look", "box") == "text")
            {
                const auto ink = open ? accent : (hovering ? 0xffffffff : 0xffeeeeee);
                g.drawText (optionText (current()), x, y, w - 16.0f, h, textSize, ink, Align::left);
                const float ax = x + w - 7.0f, ay = y + h * 0.5f, s = std::min (4.0f, h * 0.15f);

                if (open) g.fillTriangle (ax - s, ay + s * 0.5f, ax + s, ay + s * 0.5f, ax, ay - s * 0.7f, accent);
                else      g.fillTriangle (ax - s, ay - s * 0.5f, ax + s, ay - s * 0.5f, ax, ay + s * 0.7f, withAlpha (accent, hovering ? 1.0f : 0.8f));

                return;
            }

            // A rectangle, its corners from square ("rounding" 0) to round ends (100: a pill).
            const float rounding = setting ("rounding", -1.0f);
            const float corner = rounding < 0.0f ? 4.0f : std::min (w, h) * 0.5f * std::clamp (rounding, 0.0f, 100.0f) / 100.0f;
            const float inset = std::max (10.0f, corner * 0.6f);

            g.fillGradient (x, y, w, h, hovering || open ? 0xff34343c : 0xff26262c, hovering || open ? 0xff2a2a31 : 0xff1c1c21, corner);
            g.drawRoundedRect (x, y, w, h, corner, 1.0f, open ? accent : (hovering ? 0x50ffffff : 0x34ffffff));
            g.drawText (optionText (current()), x + inset, y, w - inset - 24.0f - corner * 0.3f, h, textSize, 0xffeeeeee, Align::left);

            // The arrow: down, or up while the list is open.
            const float ax = x + w - 15.0f - corner * 0.3f, ay = y + h * 0.5f, s = std::min (5.0f, h * 0.18f);

            if (open) g.fillTriangle (ax - s, ay + s * 0.5f, ax + s, ay + s * 0.5f, ax, ay - s * 0.7f, accent);
            else      g.fillTriangle (ax - s, ay - s * 0.5f, ax + s, ay - s * 0.5f, ax, ay + s * 0.7f, accent);
        }

        bool mouseDown (const Mouse&) override
        {
            hoverIndex = current();
            const auto shape = layout();
            openPopup (0.0f, (float) height() + 2.0f, (float) shape.columns * shape.columnWidth + 8.0f, (float) shape.rows * rowHeight + 8.0f);
            return false;
        }

        void mouseMove (const Mouse&) override    { hovering = true; }
        void mouseExit() override                 { hovering = false; }

        void mouseWheel (const Mouse&, float notches) override
        {
            if (notches != 0.0f)
                choose (std::clamp (current() + (notches > 0.0f ? -1 : 1), 0, count() - 1));
        }

        void paintPopup (Canvas& g) override
        {
            const auto accent = colour (0xffe5484d);
            const float w = (float) g.width(), h = (float) g.height();
            const auto shape = layout();

            g.fillRoundedRect (0.0f, 0.0f, w, h, 5.0f, 0xf41e1e23);
            g.drawRoundedRect (0.0f, 0.0f, w, h, 5.0f, 1.0f, 0x40ffffff);

            for (int i = 0; i < count(); ++i)
            {
                const auto column = i / shape.rows, row = i % shape.rows;
                const float cellX = 4.0f + (float) column * shape.columnWidth, cellY = 4.0f + (float) row * rowHeight;
                const bool chosen = i == current();

                if (column > 0 && row == 0)
                    g.fillRect (cellX - 0.5f, 6.0f, 1.0f, h - 12.0f, 0x18ffffff);

                if (i == hoverIndex)
                    g.fillRoundedRect (cellX + 1.0f, cellY, shape.columnWidth - 2.0f, rowHeight, 3.0f, withAlpha (accent, 0.3f));

                if (chosen)
                    g.fillCircle (cellX + 9.0f, cellY + rowHeight * 0.5f, 3.0f, accent);

                g.drawText (optionText (i), cellX + 20.0f, cellY, shape.columnWidth - 24.0f, rowHeight, 13.0f,
                            chosen ? 0xffffffff : 0xffcfcfcf, Align::left, chosen);
            }
        }

        void popupMouseMove (const Mouse& m) override    { hoverIndex = indexAt (m.x, m.y); }
        void popupMouseExit() override                    { hoverIndex = -1; }

        void popupMouseWheel (const Mouse&, float notches) override
        {
            if (notches != 0.0f)
                hoverIndex = std::clamp ((hoverIndex < 0 ? current() : hoverIndex) + (notches > 0.0f ? -1 : 1), 0, count() - 1);
        }

        void popupMouseDown (const Mouse& m) override
        {
            if (const auto index = indexAt (m.x, m.y); index >= 0)
                choose (index);

            closePopup();
        }

    private:
        static constexpr float rowHeight = 24.0f;

        struct Shape
        {
            int columns, rows;
            float columnWidth;
        };

        bool hovering = false;
        int hoverIndex = -1;

        float captionHeight() const   { return ! label().empty() && height() >= 40 ? 18.0f : 0.0f; }
        int count() const             { return numOptions() > 0 ? numOptions() : std::max (1, choices()); }
        int current() const           { return std::clamp ((int) std::lround (value() - minimum()), 0, count() - 1); }

        std::string optionText (int i) const
        {
            return i < numOptions() ? option (i) : std::to_string (i + 1);
        }

        void choose (int index)
        {
            setValue (minimum() + (float) index);
        }

        /** The list's columns: as few as fit "rows" options each, as wide as the box. */
        Shape layout() const
        {
            const auto most = std::max (1, std::min (64, (int) std::lround (setting ("rows", 12.0f))));
            const auto columns = (count() + most - 1) / most;
            const auto rows = (count() + columns - 1) / columns;
            return { columns, rows, std::max ((float) width(), 120.0f) };
        }

        int indexAt (float x, float y) const
        {
            const auto shape = layout();
            const auto column = (int) std::floor ((x - 4.0f) / shape.columnWidth);
            const auto row = (int) std::floor ((y - 4.0f) / rowHeight);

            if (column < 0 || column >= shape.columns || row < 0 || row >= shape.rows)
                return -1;

            const auto index = column * shape.rows + row;
            return index < count() ? index : -1;
        }
    };

    STELLA_BUILTIN_ELEMENT (DropDown, "A drop-down list: the chosen option in a box; a click opens the list (long lists in columns). "
                                      "For a 0..N-1 parameter; \"options\" names the positions.")

    //==========================================================================
    /** What the knobs and sliders below share: a colour from a setting, angles, a metal cap. */
    struct PartsKit
    {
        static constexpr float pi = 3.14159265f;

        /** "#AARRGGBB" or "#RRGGBB"; fallback when it's neither. */
        static Colour hexColour (const std::string& text, Colour fallback)
        {
            auto s = text;

            if (! s.empty() && s[0] == '#')
                s = s.substr (1);

            if (s.size() == 6)
                s = "ff" + s;

            if (s.size() != 8 || s.find_first_not_of ("0123456789abcdefABCDEF") != std::string::npos)
                return fallback;

            return (Colour) std::strtoul (s.c_str(), nullptr, 16);
        }

        static float clampUnit (float v)    { return std::clamp (v, 0.0f, 1.0f); }

        /** A knob's angle for a position 0..1: from 7 to 5 o'clock (0 at 12, clockwise). */
        static float knobAngle (float position)    { return (-0.75f + 1.5f * clampUnit (position)) * pi; }

        static void polar (float cx, float cy, float angle, float radius, float& x, float& y)
        {
            x = cx + std::sin (angle) * radius;
            y = cy - std::cos (angle) * radius;
        }

        /** A disc lit from the upper left: its colour shaded to its edge, a soft glow of light
            towards the light (fillRadialGradient fills a whole circle, so each lies inside it). */
        static void litDisc (Canvas& g, float cx, float cy, float r, Colour body, float shine)
        {
            g.fillRadialGradient (cx, cy, r, brighter (body, 0.12f), darker (body, 0.35f));
            g.fillRadialGradient (cx - r * 0.28f, cy - r * 0.32f, r * 0.55f, withAlpha (0xffffffff, shine), withAlpha (0xffffffff, 0.0f));
        }

        /** A round metal cap lit from the upper left, its pointer at angle. */
        static void metalCap (Canvas& g, float cx, float cy, float r, float angle, Colour pointer, Colour body = 0xffc7cbd1)
        {
            g.fillCircle (cx + r * 0.04f, cy + r * 0.1f, r * 1.03f, 0x80000000);   // its shadow
            g.fillCircle (cx, cy, r, darker (body, 0.6f));
            litDisc (g, cx, cy, r - 1.0f, darker (body, 0.1f), 0.35f);              // its skirt
            g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f, 0x90000000);
            g.fillCircle (cx, cy + r * 0.03f, r * 0.8f, darker (body, 0.45f));      // the top's edge
            litDisc (g, cx, cy, r * 0.76f, body, 0.55f);                           // its top

            float x0, y0, x1, y1;
            polar (cx, cy, angle, r * 0.22f, x0, y0);
            polar (cx, cy, angle, r * 0.72f, x1, y1);
            g.drawLine (x0, y0, x1, y1, std::max (1.5f, r * 0.13f), pointer);
        }
    };

    //==========================================================================
    /** One track, two handles: the low end and the high end of a range (a high-pass and a
        low-pass, a band, a key range). "params": { "low": "<id>", "high": "<id>" } (the
        element's own "param" stands in for a low it hasn't got). On one axis: a frequency's
        is spaced as the ear hears it (log), others evenly; the low handle never passes the
        high one. Across, or upright when its box is taller than wide. "settings": { "ticks":
        10 } marks the track; "values": 0 hides the values under the handles. */
    class RangeSlider : public Element
    {
    public:
        void paint (Canvas& g) override
        {
            const auto accent = colour (0xffe5484d);
            const auto geo = geometry();
            const bool upright = isUpright();

            if (geo.caption > 0.0f)
                g.drawText (label(), upright ? 0.0f : 2.0f, upright ? (float) height() - geo.caption : 0.0f, (float) width() - 4.0f, geo.caption,
                            12.5f, 0xffd8d4ca, upright ? Align::centre : Align::left);

            // The slot, and its ticks.
            const float t = geo.thickness;

            if (upright)
                g.fillRoundedRect (geo.cross - t * 0.5f, geo.start, t, geo.end - geo.start, t * 0.5f, 0xff0b0b0d);
            else
                g.fillRoundedRect (geo.start, geo.cross - t * 0.5f, geo.end - geo.start, t, t * 0.5f, 0xff0b0b0d);

            if (const auto ticks = (int) std::lround (setting ("ticks", 0.0f)); ticks > 1)
                for (int i = 0; i <= ticks; ++i)
                {
                    const float at = geo.start + (geo.end - geo.start) * (float) i / (float) ticks;
                    const float reach = geo.radius + 3.0f;

                    if (upright) g.fillRect (geo.cross + reach, at - 0.5f, 4.0f, 1.0f, 0x60ffffff);
                    else         g.fillRect (at - 0.5f, geo.cross + reach, 1.0f, 4.0f, 0x60ffffff);
                }

            // The band between the handles, lit.
            const float a = along (position (value (lowRole()))), b = along (position (value (highRole())));
            const float from = std::min (a, b), to = std::max (a, b);

            if (upright)
            {
                g.fillRoundedRect (geo.cross - t * 1.1f, from, t * 2.2f, to - from, t, withAlpha (accent, 0.22f));
                g.fillGradient (geo.cross - t * 0.5f, from, t, to - from, brighter (accent, 0.3f), darker (accent, 0.2f), t * 0.5f);
            }
            else
            {
                g.fillRoundedRect (from, geo.cross - t * 1.1f, to - from, t * 2.2f, t, withAlpha (accent, 0.22f));
                g.fillGradient (from, geo.cross - t * 0.5f, to - from, t, brighter (accent, 0.3f), darker (accent, 0.2f), t * 0.5f);
            }

            // The handles: half moons of chrome, their flat sides towards each other.
            handle (g, a, true, accent);
            handle (g, b, false, accent);

            // Their values, under them (beside them, upright).
            if (setting ("values", 1.0f) > 0.5f && geo.valueRoom > 10.0f)
            {
                for (int i = 0; i < 2; ++i)
                {
                    const float at = i == 0 ? a : b;
                    const auto text = valueText (i == 0 ? lowRole() : highRole());
                    const auto shown = dragging == i || hover == i ? 0xffffffff : 0xffb8b4ab;

                    if (upright)
                        g.drawText (text, geo.cross + geo.radius + 4.0f, at - 8.0f, (float) width() - geo.cross - geo.radius - 4.0f, 16.0f, 11.0f, shown, Align::left);
                    else
                        g.drawText (text, at - 40.0f, geo.cross + geo.radius + 3.0f, 80.0f, std::min (16.0f, geo.valueRoom), 11.0f, shown, Align::centre);
                }
            }
        }

        bool mouseDown (const Mouse& m) override
        {
            dragging = nearest (m);

            if (m.doubleClick)
            {
                setValue (defaultValue (roleOf (dragging)), roleOf (dragging));
                dragging = -1;
                return false;
            }

            beginEdit (roleOf (dragging));
            moveTo (m);
            return true;
        }

        void mouseDrag (const Mouse& m) override    { moveTo (m); }

        void mouseUp (const Mouse&) override
        {
            if (dragging >= 0)
                endEdit (roleOf (dragging));

            dragging = -1;
        }

        void mouseMove (const Mouse& m) override    { hover = nearest (m); }
        void mouseExit() override                   { hover = -1; }

        void mouseWheel (const Mouse& m, float notches) override
        {
            if (notches == 0.0f)
                return;

            const auto which = nearest (m);
            const auto role = roleOf (which);
            moveHandle (which, position (value (role)) + (notches > 0.0f ? 0.02f : -0.02f));
        }

    private:
        struct Geometry
        {
            float caption = 0.0f, start = 0.0f, end = 1.0f, cross = 0.0f, thickness = 6.0f, radius = 8.0f, valueRoom = 0.0f;
        };

        int dragging = -1, hover = -1;

        bool isUpright() const    { return height() > width(); }
        const char* lowRole() const     { return hasParam ("low") ? "low" : ""; }
        const char* highRole() const    { return hasParam ("high") ? "high" : ""; }
        const char* roleOf (int which) const    { return which == 1 ? highRole() : lowRole(); }

        Geometry geometry() const
        {
            Geometry geo;
            const bool upright = isUpright();
            geo.caption = ! label().empty() && (upright ? width() >= 30 && height() >= 90 : height() >= 40) ? 18.0f : 0.0f;

            if (upright)
            {
                const float w = (float) width();
                geo.radius = std::clamp (w * 0.22f, 6.0f, 14.0f);
                geo.thickness = std::clamp (w * 0.12f, 4.0f, 10.0f);
                geo.cross = std::min (w * 0.35f, geo.radius + 4.0f);
                geo.start = geo.radius + 2.0f;
                geo.end = (float) height() - geo.caption - geo.radius - 2.0f;
                geo.valueRoom = w - geo.cross - geo.radius - 4.0f;
            }
            else
            {
                const float h = (float) height() - geo.caption;
                geo.radius = std::clamp (h * 0.26f, 6.0f, 14.0f);
                geo.thickness = std::clamp (h * 0.15f, 4.0f, 10.0f);
                geo.cross = geo.caption + std::min (h * 0.42f, geo.radius + 6.0f);
                geo.start = geo.radius + 2.0f;
                geo.end = (float) width() - geo.radius - 2.0f;
                geo.valueRoom = (float) height() - geo.cross - geo.radius - 3.0f;
            }

            return geo;
        }

        /** The shared axis: from the lower of their minimums to the higher of their maximums. */
        float axisLow() const     { return std::min (minimum (lowRole()), minimum (highRole())); }
        float axisHigh() const    { return std::max (maximum (lowRole()), maximum (highRole())); }

        bool logAxis() const
        {
            const auto u = unit (lowRole()) + unit (highRole());
            return axisLow() > 0.0f && axisHigh() > axisLow() * 8.0f && (u.find ("Hz") != std::string::npos || setting ("log", 0.0f) > 0.5f);
        }

        float position (float v) const
        {
            const float lo = axisLow(), hi = axisHigh();

            if (hi <= lo)
                return 0.0f;

            if (logAxis())
                return PartsKit::clampUnit (std::log (std::max (v, lo) / lo) / std::log (hi / lo));

            return PartsKit::clampUnit ((v - lo) / (hi - lo));
        }

        float valueAtPosition (float p) const
        {
            const float lo = axisLow(), hi = axisHigh();
            p = PartsKit::clampUnit (p);
            return logAxis() ? lo * std::pow (hi / lo, p) : lo + (hi - lo) * p;
        }

        /** A position's place on the track, in pixels (upright: the low end at the bottom). */
        float along (float p) const
        {
            const auto geo = geometry();
            return isUpright() ? geo.end - (geo.end - geo.start) * p : geo.start + (geo.end - geo.start) * p;
        }

        float positionAt (const Mouse& m) const
        {
            const auto geo = geometry();
            const float span = std::max (1.0f, geo.end - geo.start);
            return isUpright() ? (geo.end - m.y) / span : (m.x - geo.start) / span;
        }

        int nearest (const Mouse& m) const
        {
            const float p = positionAt (m);
            const float a = position (value (lowRole())), b = position (value (highRole()));

            // Both at one place: the way the mouse is from it decides.
            if (std::abs (a - b) < 1.0e-4f)
                return p >= a ? 1 : 0;

            return std::abs (p - a) <= std::abs (p - b) ? 0 : 1;
        }

        void moveTo (const Mouse& m)
        {
            if (dragging >= 0)
                moveHandle (dragging, positionAt (m));
        }

        void moveHandle (int which, float p)
        {
            // Never past the other handle.
            const float other = position (value (roleOf (1 - which)));
            p = which == 0 ? std::min (p, other) : std::max (p, other);

            const auto role = roleOf (which);
            setValue (std::clamp (valueAtPosition (p), minimum (role), maximum (role)), role);
        }

        void handle (Canvas& g, float at, bool low, Colour accent) const
        {
            const auto geo = geometry();
            const bool upright = isUpright();
            const float r = geo.radius;
            const float cx = upright ? geo.cross : at, cy = upright ? at : geo.cross;
            constexpr float pi = PartsKit::pi;

            // Across: the low one faces left, the high one right; upright: the low one down.
            const float from = upright ? (low ? pi * 0.5f : -pi * 0.5f) : (low ? pi : 0.0f);
            const float to = from + pi;

            g.fillPie (cx + 1.0f, cy + 2.0f, r, from, to, 0x70000000);
            g.fillPie (cx, cy, r, from, to, 0xff3b3d42);
            g.fillPie (cx, cy, r - 1.5f, from, to, 0xffc3c7cd);
            g.fillPie (cx - r * 0.12f, cy - r * 0.14f, r * 0.62f, from, to, 0xffe9ecf0);

            // Its flat side, with the accent's mark.
            if (upright) g.fillRect (cx - r, cy - 0.75f, r * 2.0f, 1.5f, 0xff2a2c30);
            else         g.fillRect (cx - 0.75f, cy - r, 1.5f, r * 2.0f, 0xff2a2c30);

            const float mark = std::max (1.5f, r * 0.18f);

            if (upright) g.fillRoundedRect (cx - r * 0.5f, cy + (low ? 2.0f : -2.0f - mark), r, mark, mark * 0.5f, accent);
            else         g.fillRoundedRect (cx + (low ? -2.0f - mark : 2.0f), cy - r * 0.5f, mark, r, mark * 0.5f, accent);
        }
    };

    STELLA_BUILTIN_ELEMENT (RangeSlider, "One track with two handles for a range's low and high ends: a high-pass and a low-pass, a band, a key range. "
                                         "\"params\": { \"low\": id, \"high\": id }; frequencies spaced as heard; across, or upright in a tall box.")

    //==========================================================================
    /** A big knob on a dial plate, with a meter's window above it: the level of its "source"
        (a signal in dB, or "settings": { "meter": "linear" } for a display's 0..1) on a needle
        coming out from under the knob, and the knob's own value marked on the meter's scale
        (a threshold against the level it acts on). "settings": { "face": "#FFF1E4C3" (the
        window's), "plate": "#FF1D1E22" }; its colour marks the value. */
    class MeterKnob : public Element
    {
    public:
        void paint (Canvas& g) override
        {
            constexpr float pi = PartsKit::pi;
            const auto accent = colour (0xffe5484d);
            const auto face = PartsKit::hexColour (settingText ("face"), 0xfff1e4c3);
            const auto plate = PartsKit::hexColour (settingText ("plate"), 0xff1d1e22);
            const float caption = label().empty() ? 0.0f : 18.0f;
            const float size = std::max (8.0f, std::min ((float) width(), (float) height() - caption));
            const float cx = (float) width() * 0.5f, cy = size * 0.5f, r = size * 0.5f - 1.0f;
            const float half = 0.3f * pi;   // the window, each side of upright
            const float inner = r * 0.6f, outer = r * 0.9f;

            // The plate, a little domed.
            g.fillCircle (cx, cy, r, darker (plate, 0.5f));
            PartsKit::litDisc (g, cx, cy, r - 1.5f, plate, 0.1f);

            // The window: the face lit from behind, its frame, its scale.
            g.fillPie (cx, cy, outer, -half, half, darker (face, 0.25f));
            g.fillPie (cx, cy, outer - 1.5f, -half, half, face);
            g.fillRadialGradient (cx, cy, inner, brighter (plate, 0.08f), plate);
            g.drawArc (cx, cy, outer, -half, half, 1.5f, 0xff0c0c0e);
            g.drawArc (cx, cy, inner, -half, half, 1.5f, 0xff0c0c0e);

            for (const float side : { -half, half })
            {
                float x0, y0, x1, y1;
                PartsKit::polar (cx, cy, side, inner, x0, y0);
                PartsKit::polar (cx, cy, side, outer, x1, y1);
                g.drawLine (x0, y0, x1, y1, 1.5f, 0xff0c0c0e);
            }

            const auto ink = 0xff1c1a17;
            const bool linear = settingText ("meter", "db") == "linear";
            const float marks[] { 0.0f, 0.25f, 0.5f, 0.75f, 0.9091f, 1.0f };   // dB: -60, -43.5, -27, -10.5, 0, +6

            for (const auto at : marks)
            {
                const float angle = -half + 2.0f * half * at;
                float x0, y0, x1, y1;
                PartsKit::polar (cx, cy, angle, outer - 2.0f, x0, y0);
                PartsKit::polar (cx, cy, angle, outer - r * (at > 0.9f && ! linear ? 0.1f : 0.07f), x1, y1);
                g.drawLine (x0, y0, x1, y1, 1.2f, at > 0.9f && ! linear ? 0xffc8232c : ink);
            }

            if (! linear)
                g.drawArc (cx, cy, outer - r * 0.05f, -half + 2.0f * half * 0.9091f, half, std::max (1.5f, r * 0.035f), 0xffc8232c);

            // The knob's value on the meter's scale.
            {
                const float angle = -half + 2.0f * half * normalised();
                float tx, ty, lx, ly, rx, ry;
                PartsKit::polar (cx, cy, angle, outer + 1.0f, tx, ty);
                PartsKit::polar (cx, cy, angle - 0.06f, outer + r * 0.08f, lx, ly);
                PartsKit::polar (cx, cy, angle + 0.06f, outer + r * 0.08f, rx, ry);
                g.fillTriangle (tx, ty, lx, ly, rx, ry, accent);
            }

            // The needle, swinging after the level, out from under the knob.
            float target = 0.0f;

            if (hasSource())
            {
                const float lv = level();
                target = linear ? PartsKit::clampUnit (lv) : PartsKit::clampUnit ((20.0f * std::log10 (std::max (lv, 1.0e-5f)) + 60.0f) / 66.0f);
            }

            needle += (target - needle) * 0.25f;
            {
                const float angle = -half + 2.0f * half * PartsKit::clampUnit (needle);
                float x0, y0, x1, y1;
                PartsKit::polar (cx, cy, angle, r * 0.45f, x0, y0);
                PartsKit::polar (cx, cy, angle, outer - 3.0f, x1, y1);
                g.drawLine (x0 + 1.5f, y0 + 2.0f, x1 + 1.5f, y1 + 2.0f, std::max (1.0f, r * 0.02f), 0x40000000);
                g.drawLine (x0, y0, x1, y1, std::max (1.0f, r * 0.018f), ink);
            }

            g.drawArc (cx, cy, (inner + outer) * 0.5f, -half * 0.9f, -half * 0.2f, (outer - inner) * 0.35f, 0x16ffffff);   // the glass's sheen

            // The knob's own scale round the plate, then the knob.
            for (int i = 0; i <= 10; ++i)
            {
                const float angle = PartsKit::knobAngle ((float) i / 10.0f);

                if (std::abs (angle) < half + 0.05f)
                    continue;   // the window's there

                float x0, y0, x1, y1;
                PartsKit::polar (cx, cy, angle, r * 0.86f, x0, y0);
                PartsKit::polar (cx, cy, angle, r * (i % 5 == 0 ? 0.97f : 0.93f), x1, y1);
                g.drawLine (x0, y0, x1, y1, i % 5 == 0 ? 1.6f : 1.1f, 0xb0d8d4ca);
            }

            PartsKit::metalCap (g, cx, cy, r * 0.48f, PartsKit::knobAngle (normalised()), accent);

            if (caption > 0.0f)
                g.drawText (dragging ? valueText() : label(), 0.0f, (float) height() - caption, (float) width(), caption, 12.5f,
                            dragging ? 0xffffffff : 0xffd8d4ca, Align::centre, dragging);
        }

        bool mouseDown (const Mouse& m) override
        {
            if (m.doubleClick)
            {
                setValue (defaultValue());
                return false;
            }

            start = normalised();
            startX = m.x;
            startY = m.y;
            dragging = true;
            beginEdit();
            return true;
        }

        void mouseDrag (const Mouse& m) override
        {
            setNormalised (PartsKit::clampUnit (start + ((m.x - startX) - (m.y - startY)) / (m.shift ? 900.0f : 200.0f)));
        }

        void mouseUp (const Mouse&) override
        {
            dragging = false;
            endEdit();
        }

        void mouseWheel (const Mouse&, float notches) override
        {
            if (notches != 0.0f)
                setNormalised (PartsKit::clampUnit (normalised() + (notches > 0.0f ? 0.03f : -0.03f)));
        }

    private:
        float needle = 0.0f, start = 0.0f, startX = 0.0f, startY = 0.0f;
        bool dragging = false;
    };

    STELLA_BUILTIN_ELEMENT (MeterKnob, "A big knob on a dial plate with a meter's window above it: the \"source\" level on a needle, the knob's "
                                       "value marked on the meter's scale (a threshold against the level). About square, 120 to 200 px.")

    //==========================================================================
    /** Two concentric knobs: an outer ring and an inner cap, each its own parameter: "params":
        { "outer": id, "inner": id } (the element's own "param" stands in for an outer it hasn't
        got). Dragging near the middle turns the inner one, further out the outer one. Its colour
        is the inner cap's pointer; "settings": { "ring": "#FF2A2B30" } the outer ring's colour. */
    class DualKnob : public Element
    {
    public:
        void paint (Canvas& g) override
        {
            const auto accent = colour (0xffe5484d);
            const auto ring = PartsKit::hexColour (settingText ("ring"), 0xff2a2b30);
            const float caption = label().empty() ? 0.0f : 18.0f;
            const float size = std::max (8.0f, std::min ((float) width(), (float) height() - caption));
            const float cx = (float) width() * 0.5f, cy = size * 0.5f, r = size * 0.5f - 1.0f;

            // Its scale.
            for (int i = 0; i <= 10; ++i)
            {
                float x0, y0, x1, y1;
                const float angle = PartsKit::knobAngle ((float) i / 10.0f);
                PartsKit::polar (cx, cy, angle, r * 0.9f, x0, y0);
                PartsKit::polar (cx, cy, angle, r * (i % 5 == 0 ? 1.0f : 0.96f), x1, y1);
                g.drawLine (x0, y0, x1, y1, i % 5 == 0 ? 1.6f : 1.1f, 0xb0d8d4ca);
            }

            // The outer ring: ribbed, its notch showing its value.
            const float outer = r * 0.84f;
            g.fillCircle (cx + 1.0f, cy + 2.0f, outer, 0x80000000);
            g.fillCircle (cx, cy, outer, darker (ring, 0.45f));
            PartsKit::litDisc (g, cx, cy, outer - 1.0f, brighter (ring, 0.1f), 0.18f);

            for (int i = 0; i < 36; ++i)
            {
                float x0, y0, x1, y1;
                const float angle = (float) i / 36.0f * 2.0f * PartsKit::pi;
                PartsKit::polar (cx, cy, angle, outer * 0.9f, x0, y0);
                PartsKit::polar (cx, cy, angle, outer - 0.5f, x1, y1);
                g.drawLine (x0, y0, x1, y1, 1.0f, (i & 1) ? 0x30ffffff : 0x50000000);
            }

            {
                float x0, y0, x1, y1;
                const float angle = PartsKit::knobAngle (normalised (outerRole()));
                PartsKit::polar (cx, cy, angle, outer * 0.62f, x0, y0);
                PartsKit::polar (cx, cy, angle, outer * 0.97f, x1, y1);
                g.drawLine (x0, y0, x1, y1, std::max (2.0f, outer * 0.08f), 0xfff2efe6);
            }

            // The inner cap, with its own pointer.
            PartsKit::metalCap (g, cx, cy, outer * 0.5f, PartsKit::knobAngle (normalised (innerRole())), accent);

            if (caption > 0.0f)
                g.drawText (grabbed >= 0 ? valueText (grabbed == 1 ? innerRole() : outerRole()) : label(), 0.0f, (float) height() - caption,
                            (float) width(), caption, 12.5f, grabbed >= 0 ? 0xffffffff : 0xffd8d4ca, Align::centre, grabbed >= 0);
        }

        bool mouseDown (const Mouse& m) override
        {
            const float caption = label().empty() ? 0.0f : 18.0f;
            const float size = std::min ((float) width(), (float) height() - caption);
            const float dx = m.x - (float) width() * 0.5f, dy = m.y - size * 0.5f;
            grabbed = std::sqrt (dx * dx + dy * dy) < size * 0.5f * 0.84f * 0.55f ? 1 : 0;
            const auto role = grabbed == 1 ? innerRole() : outerRole();

            if (m.doubleClick)
            {
                setValue (defaultValue (role), role);
                grabbed = -1;
                return false;
            }

            start = normalised (role);
            startX = m.x;
            startY = m.y;
            beginEdit (role);
            return true;
        }

        void mouseDrag (const Mouse& m) override
        {
            if (grabbed >= 0)
                setNormalised (PartsKit::clampUnit (start + ((m.x - startX) - (m.y - startY)) / (m.shift ? 900.0f : 200.0f)),
                               grabbed == 1 ? innerRole() : outerRole());
        }

        void mouseUp (const Mouse&) override
        {
            if (grabbed >= 0)
                endEdit (grabbed == 1 ? innerRole() : outerRole());

            grabbed = -1;
        }

        void mouseWheel (const Mouse& m, float notches) override
        {
            const float caption = label().empty() ? 0.0f : 18.0f;
            const float size = std::min ((float) width(), (float) height() - caption);
            const float dx = m.x - (float) width() * 0.5f, dy = m.y - size * 0.5f;
            const auto role = std::sqrt (dx * dx + dy * dy) < size * 0.5f * 0.84f * 0.55f ? innerRole() : outerRole();

            if (notches != 0.0f)
                setNormalised (PartsKit::clampUnit (normalised (role) + (notches > 0.0f ? 0.03f : -0.03f)), role);
        }

    private:
        int grabbed = -1;
        float start = 0.0f, startX = 0.0f, startY = 0.0f;

        const char* outerRole() const    { return hasParam ("outer") ? "outer" : ""; }
        const char* innerRole() const    { return hasParam ("inner") ? "inner" : ""; }
    };

    STELLA_BUILTIN_ELEMENT (DualKnob, "Two concentric knobs, an outer ring and an inner cap, each its own parameter: \"params\": { \"outer\": id, "
                                      "\"inner\": id }. For pairs that belong together (frequency and gain, coarse and fine).")

    //==========================================================================
    // What exported plugins draw for the studio's own modulation matrix and preset managers
    // (ModMatrix, PresetPanel): the export turns each into one of these, its design in its
    // "settings" (and, for a preset manager, its keys baked as pictures).
    namespace exported
    {
        inline Colour colourFrom (const std::string& text, Colour fallback)
        {
            std::string hex;

            for (const auto c : text)
                if (std::isxdigit ((unsigned char) c))
                    hex += c;

            if (hex.size() == 6)
                hex = "ff" + hex;

            if (hex.size() != 8)
                return fallback;

            return (Colour) std::strtoul (hex.c_str(), nullptr, 16);
        }

        inline std::vector<std::string> split (const std::string& text, char separator)
        {
            std::vector<std::string> parts;
            std::string part;

            for (const auto c : text)
            {
                if (c == separator)
                {
                    parts.push_back (part);
                    part.clear();
                }
                else if (c != '\r')
                {
                    part += c;
                }
            }

            if (! part.empty())
                parts.push_back (part);

            return parts;
        }

        inline std::string percent (float amount)
        {
            const int p = (int) std::lround (amount * 100.0f);
            return (p > 0 ? "+" : "") + std::to_string (p) + "%";
        }

        inline Colour amountColour (Colour accent, float amount)
        {
            // Below zero: the accent's opposite.
            if (amount >= 0.0f)
                return accent;

            const auto r = (accent >> 16) & 255, g = (accent >> 8) & 255, b = accent & 255;
            return rgb ((int) b, (int) r, (int) g) | (accent & 0xff000000u);
        }

        /** A cable from one point to another, hanging (sag 0..1). */
        inline void cable (Canvas& g, float x0, float y0, float x1, float y1, float thickness, float sag, Colour colour)
        {
            const float length = std::sqrt ((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
            const float drop = length * sag * 0.45f + thickness * 2.0f;
            const float c0x = x0 + (x1 - x0) * 0.25f, c0y = y0 + drop, c1x = x1 + (x0 - x1) * 0.25f, c1y = y1 + drop;
            std::vector<float> xy;

            for (int i = 0; i <= 24; ++i)
            {
                const float t = (float) i / 24.0f, u = 1.0f - t;
                xy.push_back (u * u * u * x0 + 3 * u * u * t * c0x + 3 * u * t * t * c1x + t * t * t * x1);
                xy.push_back (u * u * u * y0 + 3 * u * u * t * c0y + 3 * u * t * t * c1y + t * t * t * y1);
            }

            std::vector<float> shadow (xy);

            for (size_t i = 1; i < shadow.size(); i += 2)
                shadow[i] += thickness * 0.6f;

            g.drawPolyline (shadow.data(), 25, thickness * 1.25f, withAlpha (0xff000000u, 0.35f));
            g.drawPolyline (xy.data(), 25, thickness, darker (colour, 0.25f));
            g.drawPolyline (xy.data(), 25, thickness * 0.28f, withAlpha (brighter (colour, 0.4f), 0.7f));

            for (const auto& [x, y] : { std::pair<float, float> { x0, y0 }, { x1, y1 } })
            {
                g.fillCircle (x, y, thickness * 0.95f, darker (colour, 0.5f));
                g.fillCircle (x, y, thickness * 0.4f, 0xffc8ccd2);
            }
        }

        //======================================================================
        // LED segment digits, as the studio draws them (SegmentDisplay): 7 segments for numbers,
        // 14 or 16 for letters, the dark bars faintly there, the lit ones glowing.
        namespace seg
        {
            enum : int { a1 = 1, a2 = 2, sb = 4, sc = 8, d1 = 16, d2 = 32, se = 64, sf = 128, g1 = 256, g2 = 512,
                         sh = 1024, si = 2048, sj = 4096, sk = 8192, sl = 16384, sm = 32768 };
            constexpr int sa = a1 | a2, sd = d1 | d2, sg = g1 | g2;

            inline int sixteenOf (char c)
            {
                switch (std::toupper ((unsigned char) c))
                {
                    case '0': return sa | sb | sc | sd | se | sf | sj | sk;
                    case '1': return sb | sc | sj;
                    case '2': return sa | sb | sg | se | sd;
                    case '3': return sa | sb | sc | sd | sg;
                    case '4': return sf | sg | sb | sc;
                    case '5': return sa | sf | sg | sc | sd;
                    case '6': return sa | sf | se | sd | sc | sg;
                    case '7': return sa | sb | sc;
                    case '8': return sa | sb | sc | sd | se | sf | sg;
                    case '9': return sa | sb | sc | sd | sf | sg;
                    case 'A': return sa | sb | sc | se | sf | sg;
                    case 'B': return sa | sb | sc | sd | g2 | si | sl;
                    case 'C': return sa | sd | se | sf;
                    case 'D': return sa | sb | sc | sd | si | sl;
                    case 'E': return sa | sd | se | sf | sg;
                    case 'F': return sa | se | sf | g1;
                    case 'G': return sa | sc | sd | se | sf | g2;
                    case 'H': return sb | sc | se | sf | sg;
                    case 'I': return sa | sd | si | sl;
                    case 'J': return sb | sc | sd | se;
                    case 'K': return se | sf | g1 | sj | sm;
                    case 'L': return sd | se | sf;
                    case 'M': return sb | sc | se | sf | sh | sj;
                    case 'N': return sb | sc | se | sf | sh | sm;
                    case 'O': return sa | sb | sc | sd | se | sf;
                    case 'P': return sa | sb | se | sf | sg;
                    case 'Q': return sa | sb | sc | sd | se | sf | sm;
                    case 'R': return sa | sb | se | sf | sg | sm;
                    case 'S': return sa | sf | sg | sc | sd;
                    case 'T': return sa | si | sl;
                    case 'U': return sb | sc | sd | se | sf;
                    case 'V': return se | sf | sk | sj;
                    case 'W': return sb | sc | se | sf | sk | sm;
                    case 'X': return sh | sj | sk | sm;
                    case 'Y': return sh | sj | sl;
                    case 'Z': return sa | sd | sj | sk;
                    case '-': return sg;
                    case '+': return sg | si | sl;
                    case '*': return sg | sh | si | sj | sk | sl | sm;
                    case '/': return sj | sk;
                    case '\\': return sh | sm;
                    case '%': return sj | sk | sf | sc | a1 | d2;
                    case '=': return sg | sd;
                    case '<': case '(': return sj | sm;
                    case '>': case ')': return sh | sk;
                    case '[': return a1 | d1 | si | sl;
                    case ']': return a2 | d2 | si | sl;
                    case '_': return sd;
                    case '\'': return si;
                    case '"': return sf | si;
                    case '?': return sa | sb | g2 | sl;
                    case '#': return sb | sc | sd | sg | si | sl;
                    case '&': return a1 | sh | sj | g1 | se | d1 | sm;
                    default:  return 0;
                }
            }

            inline int sevenOf (char c)
            {
                switch (c)
                {
                    case '0': case 'O': case 'D': return sa | sb | sc | sd | se | sf;
                    case '1': return sb | sc;
                    case '2': case 'Z': case 'z': return sa | sb | sg | se | sd;
                    case '3': return sa | sb | sc | sd | sg;
                    case '4': return sf | sg | sb | sc;
                    case '5': case 'S': case 's': return sa | sf | sg | sc | sd;
                    case '6': return sa | sf | se | sd | sc | sg;
                    case '7': return sa | sb | sc;
                    case '8': case 'B': return sa | sb | sc | sd | se | sf | sg;
                    case '9': return sa | sb | sc | sd | sf | sg;
                    case 'A': case 'a': return sa | sb | sc | se | sf | sg;
                    case 'b': return sf | se | sd | sc | sg;
                    case 'C': return sa | sf | se | sd;
                    case 'c': return sg | se | sd;
                    case 'd': return sb | sc | sd | se | sg;
                    case 'E': case 'e': return sa | sf | sg | se | sd;
                    case 'F': case 'f': return sa | sf | sg | se;
                    case 'G': case 'g': return sa | sf | se | sd | sc;
                    case 'H': return sf | se | sb | sc | sg;
                    case 'h': return sf | se | sg | sc;
                    case 'I': case 'l': return sf | se;
                    case 'i': return sc;
                    case 'J': case 'j': return sb | sc | sd | se;
                    case 'L': return sf | se | sd;
                    case 'N': case 'n': case 'M': case 'm': return se | sg | sc;
                    case 'o': return se | sg | sc | sd;
                    case 'P': case 'p': return sa | sb | sf | sg | se;
                    case 'Q': case 'q': return sa | sb | sf | sg | sc;
                    case 'R': case 'r': return se | sg;
                    case 'T': case 't': case 'K': case 'k': return sf | se | sd | sg;
                    case 'U': case 'V': case 'W': return sf | se | sd | sc | sb;
                    case 'u': case 'v': case 'w': return se | sd | sc;
                    case 'Y': case 'y': return sf | sg | sb | sc | sd;
                    case 'X': case 'x': return sf | se | sb | sc | sg;
                    case '-': return sg;
                    case '_': return sd;
                    case '=': return sg | sd;
                    default:  return 0;
                }
            }

            inline bool isDot (char c)    { return c == '.' || c == ':' || c == ','; }

            inline int cellsOf (const std::string& text)
            {
                int cells = 0;
                bool any = false;

                for (const auto c : text)
                {
                    if (isDot (c) && any)
                        continue;

                    ++cells;
                    any = true;
                }

                return cells;
            }

            inline bool sevenAlone (const std::string& text)
            {
                for (const auto c : text)
                    if (! (std::isdigit ((unsigned char) c) || isDot (c) || std::string (" -_='\"").find (c) != std::string::npos))
                        return false;

                return true;
            }

            struct Style
            {
                int segments = 0, cells = 0, window = 1;
                Colour lit = 0xffff3b2a, background = 0xff0c0a0a;
                float ghost = 0.1f, glow = 0.4f, slant = 6.0f, thickness = 0.16f;
            };

            /** One bar, its ends pointed: into on or off as a polygon of 6 points. */
            struct Bars
            {
                std::vector<std::vector<float>> on, off;
            };

            inline void bar (Bars& bars, bool lit, float ax, float ay, float bx, float by, float t, float trimA, float trimB,
                             float cx, float cy, float H, float lean)
            {
                const float dx = bx - ax, dy = by - ay, length = std::sqrt (dx * dx + dy * dy);

                if (length <= trimA + trimB + t * 0.3f)
                    return;

                const float ux = dx / length, uy = dy / length, nx = -uy, ny = ux, half = t * 0.5f;
                const float x0 = ax + ux * trimA, y0 = ay + uy * trimA, x1 = bx - ux * trimB, y1 = by - uy * trimB;
                const float tip = std::min (half, std::sqrt ((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)) * 0.5f);
                const float pts[12] { x0, y0, x0 + ux * tip + nx * half, y0 + uy * tip + ny * half, x1 - ux * tip + nx * half, y1 - uy * tip + ny * half,
                                      x1, y1, x1 - ux * tip - nx * half, y1 - uy * tip - ny * half, x0 + ux * tip - nx * half, y0 + uy * tip - ny * half };
                std::vector<float> placed (12);

                for (int i = 0; i < 6; ++i)
                {
                    placed[(size_t) i * 2] = cx + pts[i * 2] + (H - pts[i * 2 + 1]) * lean;
                    placed[(size_t) i * 2 + 1] = cy + pts[i * 2 + 1];
                }

                (lit ? bars.on : bars.off).push_back (std::move (placed));
            }

            inline void cell (Bars& bars, float cx, float cy, float cellW, float H, int segments, int lit, float t, float lean)
            {
                const float W = std::max (2.0f, cellW - H * lean);
                const float x0 = t * 0.5f, x1 = W - t * 0.5f, xm = W * 0.5f, y0 = t * 0.5f, y1 = H - t * 0.5f, ym = H * 0.5f;
                const float gap = t * 0.22f, cross = t * 1.15f, centre = t * 0.75f;
                struct B { int bits; float ax, ay, bx, by, ta, tb; };
                std::vector<B> list;

                if (segments == 16)
                    list.insert (list.end(), { { a1, x0, y0, xm, y0, gap, gap }, { a2, xm, y0, x1, y0, gap, gap },
                                               { d1, x0, y1, xm, y1, gap, gap }, { d2, xm, y1, x1, y1, gap, gap } });
                else
                    list.insert (list.end(), { { sa, x0, y0, x1, y0, gap, gap }, { sd, x0, y1, x1, y1, gap, gap } });

                list.insert (list.end(), { { sb, x1, y0, x1, ym, gap, gap }, { sc, x1, ym, x1, y1, gap, gap },
                                           { se, x0, ym, x0, y1, gap, gap }, { sf, x0, y0, x0, ym, gap, gap } });

                if (segments == 7)
                    list.push_back ({ sg, x0, ym, x1, ym, gap, gap });
                else
                    list.insert (list.end(), { { g1, x0, ym, xm, ym, gap, gap }, { g2, xm, ym, x1, ym, gap, gap },
                                               { sh, x0, y0, xm, ym, cross, centre }, { si, xm, y0, xm, ym, centre, gap },
                                               { sj, x1, y0, xm, ym, cross, centre }, { sk, x0, y1, xm, ym, cross, centre },
                                               { sl, xm, ym, xm, y1, gap, centre }, { sm, x1, y1, xm, ym, cross, centre } });

                for (const auto& b : list)
                    bar (bars, (lit & b.bits) == b.bits, b.ax, b.ay, b.bx, b.by, t, b.ta, b.tb, cx, cy, H, lean);
            }

            /** The characters filling an area (from the right, as LED counters show them). */
            inline void characters (Canvas& g, float x, float y, float w, float h, const std::string& text, const Style& s, int cellsIn)
            {
                if (w < 2.0f || h < 2.0f)
                    return;

                const int segments = s.segments != 0 ? s.segments : (sevenAlone (text) ? 7 : 16);
                const int needed = cellsOf (text);
                const int cells = std::max ({ 1, needed, cellsIn > 0 ? cellsIn : s.cells });
                const float spacing = 0.34f;
                const float cellW = w / ((float) cells + (float) (cells - 1) * spacing), step = cellW * (1.0f + spacing);
                const float t = std::max (1.0f, std::min (h * 0.2f, cellW * s.thickness));
                const float lean = std::tan (s.slant * 3.14159265f / 180.0f);
                Bars bars;
                std::vector<float> dots;   // x, y, size of each dot
                int at = cells - needed;
                bool any = false;

                for (int i = 0; i < at; ++i)
                    cell (bars, x + (float) i * step, y, cellW, h, segments, 0, t, lean);

                for (const auto c : text)
                {
                    if (isDot (c) && any)
                    {
                        const float dx = x + (float) at * step - step + cellW + cellW * spacing * 0.5f;
                        const float size = std::min (t * 1.1f, cellW * spacing * 0.9f);

                        if (c == ':')
                        {
                            for (const float f : { 0.3f, 0.7f })
                                dots.insert (dots.end(), { dx + (h - h * f) * lean, y + h * f, size });
                        }
                        else
                        {
                            dots.insert (dots.end(), { dx, y + h - size * 0.5f, size });
                        }

                        continue;
                    }

                    if (at >= 0 && at < cells)
                        cell (bars, x + (float) at * step, y, cellW, h, segments, segments == 7 ? sevenOf (c) : sixteenOf (c), t, lean);

                    ++at;
                    any = true;
                }

                if (s.ghost > 0.0f)
                    for (const auto& p : bars.off)
                        g.fillPolygon (p.data(), 6, withAlpha (s.lit, s.ghost));

                // The light round the lit bars, then the bars.
                if (s.glow > 0.0f)
                {
                    for (const auto& [width, alpha] : { std::pair<float, float> { 2.6f, 0.06f }, { 1.4f, 0.12f } })
                    {
                        for (auto p : bars.on)
                        {
                            p.push_back (p[0]);
                            p.push_back (p[1]);
                            g.drawPolyline (p.data(), 7, t * width, withAlpha (s.lit, alpha * s.glow * 1.6f));
                        }

                        for (size_t i = 0; i + 2 < dots.size(); i += 3)
                            g.fillCircle (dots[i], dots[i + 1], dots[i + 2] * 0.5f * (1.0f + width), withAlpha (s.lit, alpha * s.glow * 1.6f));
                    }
                }

                const auto colour = brighter (s.lit, 0.14f * s.glow);

                for (const auto& p : bars.on)
                    g.fillPolygon (p.data(), 6, colour);

                for (size_t i = 0; i + 2 < dots.size(); i += 3)
                    g.fillCircle (dots[i], dots[i + 1], dots[i + 2] * 0.5f, colour);
            }

            /** The display in an area: its window, then its characters. */
            inline void display (Canvas& g, float x, float y, float w, float h, const std::string& text, const Style& s, int cells)
            {
                if (s.window <= 0)
                {
                    characters (g, x, y, w, h, text, s, cells);
                    return;
                }

                const float corner = std::min (5.0f, h * 0.14f);

                if (s.window == 2)
                {
                    const float bezel = std::max (2.0f, std::min (10.0f, h * 0.09f));
                    g.fillRoundedRect (x, y + 1.5f, w, h, corner + 1.0f, 0x73000000u);
                    g.fillGradient (x, y, w, h, 0xff4a4c51, 0xff121315, corner + 1.0f);
                    g.drawRoundedRect (x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, corner + 0.5f, 1.0f, 0x2effffffu);
                    x += bezel; y += bezel; w -= bezel * 2.0f; h -= bezel * 2.0f;
                }

                const float inner = std::max (1.0f, corner - 1.0f);
                g.fillRoundedRect (x, y, w, h, inner, s.background);
                g.fillGradient (x, y, w, std::min (8.0f, h * 0.3f), 0x8c000000u, 0x00000000u, inner);
                g.drawRoundedRect (x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, inner, 1.0f, 0xb3000000u);

                const float padY = h * 0.17f, padX = std::max (padY, h * 0.2f);
                characters (g, x + padX, y + padY, w - padX * 2.0f, h - padY * 2.0f, text, s, cells);
                g.fillGradient (x + 1.0f, y + 1.0f, w - 2.0f, h * 0.45f, s.window == 2 ? 0x17ffffffu : 0x0dffffffu, 0x00ffffffu, inner);
            }
        }
    }

    //==========================================================================
    /** The modulation matrix as an exported plugin draws it: slots (a route a row: source,
        destination, amount), a grid (a pin, a knob or a bar where a source meets a
        destination), or a patch bay (jacks and cables). Its routes are its roles' parameters:
        "src1", "dst1", "amt1"... */
    class ModMatrix : public Element
    {
    public:
        void resized() override
        {
            style = settingText ("style", "slots");
            cellKind = settingText ("cell", "pin");
            routes = std::max (1, std::min (64, (int) setting ("routes", 8.0f)));
            sources = exported::split (settingText ("sources"), '\n');
            destinations = exported::split (settingText ("destinations"), '\n');
            columns.clear();

            for (const auto& c : exported::split (settingText ("columns"), ','))
                columns.push_back (std::atoi (c.c_str()));

            accent = exported::colourFrom (settingText ("colour"), 0xff2fb36b);
            panel = exported::colourFrom (settingText ("panelColour"), 0xff141518);
            text = exported::colourFrom (settingText ("textColour"), 0xffd8dadf);
            line = exported::colourFrom (settingText ("lineColour"), 0xff2c2e33);
            cables = exported::split (settingText ("cables"), ',');
            sag = std::max (0.0f, std::min (100.0f, setting ("sag", 45.0f))) * 0.01f;
            header = setting ("header", 1.0f) > 0.5f;
            newAmount = setting ("amount", style == "patch" ? 1.0f : 0.5f);
            jackOff = picture (settingText ("jackOff"));
            jackOn = picture (settingText ("jackOn"));
            layOut();
        }

        void paint (Canvas& g) override
        {
            // No background of its own: it lies on whatever's under it.
            if (style == "grid")       paintGrid (g);
            else if (style == "patch") paintPatch (g);
            else                       paintSlots (g);
        }

        bool mouseDown (const Mouse& m) override
        {
            dragFrom = -1;
            dragging = false;
            downX = m.x;
            downY = m.y;

            if (style == "slots")
            {
                for (int i = 0; i < rows; ++i)
                {
                    float x, y, w, h;

                    part (i, 0, x, y, w, h);
                    if (inside (m, x, y, w, h)) { clear (i); return false; }

                    part (i, 1, x, y, w, h);
                    if (inside (m, x, y, w, h)) { openList (i, true, x, y + h); return false; }

                    part (i, 3, x, y, w, h);
                    if (inside (m, x, y, w, h)) { openList (i, false, x, y + h); return false; }

                    part (i, 4, x, y, w, h);
                    if (inside (m, x, y, w, h))
                    {
                        held = i;
                        heldAmount = amountOf (i);
                        beginEdit (role ("amt", i).c_str());
                        return true;
                    }
                }

                return false;
            }

            if (style == "grid")
            {
                int r = -1, c = -1;

                if (! cellAt (m, r, c))
                    return false;

                held = routeOf (r + 1, columns[(size_t) c]);
                removing = held >= 0;

                if (held < 0 && (held = freeRoute()) >= 0)
                {
                    setValue ((float) (r + 1), role ("src", held).c_str());
                    setValue ((float) columns[(size_t) c], role ("dst", held).c_str());
                    setValue (newAmount, role ("amt", held).c_str());
                }

                heldAmount = held >= 0 ? amountOf (held) : 0.0f;
                return held >= 0;
            }

            // The patch bay: a cable from an output, or one pulled out of an input.
            const int outs = (int) sources.size(), ins = (int) columns.size();

            for (int i = 0; i < outs; ++i)
                if (near (m, outputX (i), outY, jack * 0.7f))
                {
                    dragFrom = i;
                    heldAmount = newAmount;
                    return true;
                }

            for (int i = 0; i < ins; ++i)
                if (near (m, inputX (i), inY, jack * 0.7f))
                {
                    for (int r = routes; --r >= 0;)
                        if (active (r) && dst (r) == columns[(size_t) i])
                        {
                            dragFrom = src (r) - 1;
                            heldAmount = amountOf (r);
                            setValue (0.0f, role ("src", r).c_str());
                            setValue (0.0f, role ("dst", r).c_str());
                            return true;
                        }
                }

            return false;
        }

        void mouseDrag (const Mouse& m) override
        {
            dragX = m.x;
            dragY = m.y;
            dragging = dragging || std::abs (m.x - downX) + std::abs (m.y - downY) > 3.0f;

            if (held >= 0 && dragging)
            {
                const float by = style == "slots" ? (m.x - downX) / std::max (40.0f, (float) width() * 0.15f) : (downY - m.y) / 80.0f;
                setValue (std::max (-1.0f, std::min (1.0f, heldAmount + by)), role ("amt", held).c_str());
                removing = false;
            }

            repaint();
        }

        void mouseUp (const Mouse& m) override
        {
            if (style == "slots" && held >= 0)
                endEdit (role ("amt", held).c_str());

            // A pin clicked (not dragged): taken out.
            if (style == "grid" && held >= 0 && removing && ! dragging)
                clear (held);

            // A cable let go over an input: plugged in.
            if (dragFrom >= 0)
            {
                for (int i = 0; i < (int) columns.size(); ++i)
                    if (near (m, inputX (i), inY, jack * 1.6f))
                        if (const int r = freeRoute(); r >= 0)
                        {
                            setValue ((float) (dragFrom + 1), role ("src", r).c_str());
                            setValue ((float) columns[(size_t) i], role ("dst", r).c_str());
                            setValue (heldAmount, role ("amt", r).c_str());
                        }
            }

            held = dragFrom = -1;
            dragging = removing = false;
            repaint();
        }

        void mouseWheel (const Mouse& m, float notches) override
        {
            int r = -1;

            if (style == "slots")
            {
                for (int i = 0; i < rows; ++i)
                    if (m.y >= headerH + rowH * (float) i && m.y < headerH + rowH * (float) (i + 1))
                        r = i;
            }
            else if (style == "grid")
            {
                int row = -1, c = -1;

                if (cellAt (m, row, c))
                    r = routeOf (row + 1, columns[(size_t) c]);
            }

            if (r >= 0 && active (r))
                setValue (std::max (-1.0f, std::min (1.0f, amountOf (r) + notches * 0.05f)), role ("amt", r).c_str());
        }

        //======================================================================
        // A slot's list: "None", then the sources or the destinations.
        void paintPopup (Canvas& g) override
        {
            const auto& names = listSources ? sources : destinations;
            g.fillRoundedRect (0.0f, 0.0f, (float) g.width(), (float) g.height(), 5.0f, darker (panel, 0.2f));
            g.drawRoundedRect (0.5f, 0.5f, (float) g.width() - 1.0f, (float) g.height() - 1.0f, 5.0f, 1.0f, accent);
            const int current = listSources ? src (listRoute) : dst (listRoute);

            for (int i = 0; i <= (int) names.size(); ++i)
            {
                float x, y;
                listCell (i, x, y);

                if (i == current || i == listHover)
                    g.fillRoundedRect (x + 2.0f, y + 1.0f, listColumnW - 4.0f, listRowH - 2.0f, 3.0f, i == current ? withAlpha (accent, 0.35f) : withAlpha (0xffffffffu, 0.08f));

                g.drawText (i == 0 ? std::string ("None") : names[(size_t) i - 1], x + 8.0f, y, listColumnW - 12.0f, listRowH, 12.5f,
                            i == 0 ? withAlpha (text, 0.6f) : text, Align::left);
            }
        }

        void popupMouseMove (const Mouse& m) override    { listHover = listAt (m); repaint(); }
        void popupMouseExit() override                   { listHover = -1; repaint(); }

        void popupMouseDown (const Mouse& m) override
        {
            const int chosen = listAt (m);

            if (chosen >= 0)
                setValue ((float) chosen, role (listSources ? "src" : "dst", listRoute).c_str());

            closePopup();
        }

    private:
        std::string style, cellKind;
        int routes = 8;
        std::vector<std::string> sources, destinations, cables;
        std::vector<int> columns;
        Colour accent = 0xff2fb36b, panel = 0xff141518, text = 0xffd8dadf, line = 0xff2c2e33;
        float sag = 0.45f, newAmount = 0.5f;
        bool header = true;
        Image jackOff, jackOn;

        // Where everything is (as the studio's ModMatrix lays it out).
        float ax = 4, ay = 4, aw = 0, ah = 0;
        float headerH = 0, rowH = 0, nameW = 0, topH = 0, cellW = 0, cellH = 0, jack = 0, outY = 0, inY = 0;
        int rows = 0;

        // The mouse.
        int held = -1, dragFrom = -1;
        float heldAmount = 0, downX = 0, downY = 0, dragX = 0, dragY = 0;
        bool dragging = false, removing = false;

        // A slot's list.
        int listRoute = 0, listHover = -1, listColumns = 1, listRowsEach = 1;
        bool listSources = true;
        float listColumnW = 160.0f, listRowH = 20.0f;

        std::string role (const char* part, int route) const    { return std::string (part) + std::to_string (route + 1); }
        int src (int r) const        { return hasParam (role ("src", r).c_str()) ? (int) std::lround (value (role ("src", r).c_str())) : 0; }
        int dst (int r) const        { return hasParam (role ("dst", r).c_str()) ? (int) std::lround (value (role ("dst", r).c_str())) : 0; }
        float amountOf (int r) const { return hasParam (role ("amt", r).c_str()) ? value (role ("amt", r).c_str()) : 0.0f; }
        bool active (int r) const    { return src (r) > 0 && dst (r) > 0; }

        int routeOf (int source, int destination) const
        {
            for (int r = 0; r < routes; ++r)
                if (src (r) == source && dst (r) == destination)
                    return r;

            return -1;
        }

        int freeRoute() const
        {
            for (int r = 0; r < routes; ++r)
                if (! active (r))
                    return r;

            return -1;
        }

        void clear (int r)
        {
            setValue (0.0f, role ("src", r).c_str());
            setValue (0.0f, role ("dst", r).c_str());
        }

        Colour routeColour (int r) const
        {
            static const Colour standard[] { 0xffe8473a, 0xfff2b33a, 0xff4fc35a, 0xff3a8ef2, 0xffb05ef2, 0xff3ad6d6, 0xfff27ab4, 0xffe8e8e8 };
            return cables.empty() ? standard[r % 8] : exported::colourFrom (cables[(size_t) r % cables.size()], standard[r % 8]);
        }

        static bool inside (const Mouse& m, float x, float y, float w, float h)    { return m.x >= x && m.y >= y && m.x < x + w && m.y < y + h; }
        static bool near (const Mouse& m, float x, float y, float r)               { return (m.x - x) * (m.x - x) + (m.y - y) * (m.y - y) < r * r; }

        void layOut()
        {
            aw = (float) width() - 8.0f;
            ah = (float) height() - 8.0f;

            if (style == "grid")
            {
                const int rowsHere = std::max (1, (int) sources.size()), columnsHere = std::max (1, (int) columns.size());
                nameW = std::max (40.0f, std::min (aw * 0.4f, aw * 0.26f));
                topH = std::max (30.0f, std::min (ah * 0.45f, ah * 0.28f));
                cellW = (aw - nameW) / (float) columnsHere;
                cellH = (ah - topH) / (float) rowsHere;
            }
            else if (style == "patch")
            {
                const int most = std::max (std::max ((int) sources.size(), (int) columns.size()), 1);
                jack = std::max (8.0f, std::min (40.0f, std::min (aw / (float) most * 0.55f, ah * 0.2f)));
                outY = ay + ah * 0.3f;
                inY = ay + ah * 0.7f;
            }
            else
            {
                headerH = header ? std::max (12.0f, std::min (22.0f, ah * 0.09f)) : 0.0f;
                rows = std::max (1, std::min (routes, (int) ((ah - headerH) / 14.0f)));
                rowH = std::min (34.0f, (ah - headerH) / (float) rows);
            }
        }

        // A slot's parts: clear, source, arrow, destination, amount.
        void part (int i, int which, float& x, float& y, float& w, float& h) const
        {
            float rx = ax + 3.0f, rw = aw - 6.0f;
            y = ay + headerH + rowH * (float) i + 2.0f;
            h = rowH - 4.0f;
            const float clearW = h;
            const float rest = rw - clearW - 4.0f;
            const float sw = rest * 0.31f, arrowW = rest * 0.06f, dw = rest * 0.35f;
            const float xs[] { rx, rx + clearW + 4.0f, rx + clearW + 4.0f + sw, rx + clearW + 4.0f + sw + arrowW, rx + clearW + 4.0f + sw + arrowW + dw + 6.0f };
            const float ws[] { clearW, sw, arrowW, dw, rx + rw - xs[4] };
            x = xs[std::max (0, std::min (4, which))];
            w = ws[std::max (0, std::min (4, which))];
        }

        float outputX (int i) const    { return ax + aw * ((float) i + 0.5f) / (float) std::max (1, (int) sources.size()); }
        float inputX (int i) const     { return ax + aw * ((float) i + 0.5f) / (float) std::max (1, (int) columns.size()); }

        bool cellAt (const Mouse& m, int& r, int& c) const
        {
            if (cellW <= 0.0f || cellH <= 0.0f)
                return false;

            c = (int) std::floor ((m.x - ax - nameW) / cellW);
            r = (int) std::floor ((m.y - ay - topH) / cellH);
            return r >= 0 && c >= 0 && r < (int) sources.size() && c < (int) columns.size();
        }

        std::string destinationName (int number) const
        {
            return number >= 1 && number <= (int) destinations.size() ? destinations[(size_t) number - 1] : std::string();
        }

        //======================================================================
        void paintSlots (Canvas& g)
        {
            const float size = std::min (rowH * 0.45f, 13.0f);

            if (headerH > 0.0f)
            {
                float x, y, w, h;
                part (0, 1, x, y, w, h);
                g.drawText ("SOURCE", x, ay, w, headerH, size, withAlpha (text, 0.55f), Align::left, true);
                part (0, 3, x, y, w, h);
                g.drawText ("DESTINATION", x, ay, w, headerH, size, withAlpha (text, 0.55f), Align::left, true);
                part (0, 4, x, y, w, h);
                g.drawText ("AMOUNT", x, ay, w, headerH, size, withAlpha (text, 0.55f), Align::centre, true);
            }

            for (int i = 0; i < rows; ++i)
            {
                const float ry = ay + headerH + rowH * (float) i;

                if (i % 2 == 1)
                    g.fillRect (ax, ry, aw, rowH, 0x06ffffffu);

                g.fillRect (ax + 4.0f, ry, aw - 8.0f, 1.0f, line);

                const bool on = active (i);
                float x, y, w, h;
                part (i, 0, x, y, w, h);
                const float inset = rowH * 0.18f;
                g.fillEllipse (x + inset, y + inset, w - 2 * inset, h - 2 * inset, on ? accent : brighter (line, 0.3f));

                if (on)
                {
                    const float cx = x + w * 0.5f, cy = y + h * 0.5f, k = (w - 2 * inset) * 0.2f;
                    g.drawLine (cx - k, cy - k, cx + k, cy + k, 1.4f, panel);
                    g.drawLine (cx + k, cy - k, cx - k, cy + k, 1.4f, panel);
                }

                auto boxed = [&] (int which, const std::string& name, bool set)
                {
                    float bx, by, bw, bh;
                    part (i, which, bx, by, bw, bh);
                    g.fillRoundedRect (bx, by, bw, bh, 3.0f, 0x4d000000u);
                    g.drawText (set ? name : std::string ("none"), bx + 5.0f, by, bw - 15.0f, bh, std::min (rowH * 0.5f, 14.0f), set ? text : withAlpha (text, 0.35f), Align::left);
                    const float cx = bx + bw - 7.0f, cy = by + bh * 0.5f;
                    g.fillTriangle (cx - 3.0f, cy - 1.5f, cx + 3.0f, cy - 1.5f, cx, cy + 2.0f, set ? text : withAlpha (text, 0.35f));
                };

                const int s = src (i), d = dst (i);
                boxed (1, s >= 1 && s <= (int) sources.size() ? sources[(size_t) s - 1] : std::string(), s > 0);

                part (i, 2, x, y, w, h);
                const float mx = x + w * 0.5f, my = y + h * 0.5f;
                g.drawLine (mx - 5.0f, my, mx + 4.0f, my, 1.2f, withAlpha (text, 0.5f));
                g.fillTriangle (mx + 2.0f, my - 3.0f, mx + 6.0f, my, mx + 2.0f, my + 3.0f, withAlpha (text, 0.5f));

                boxed (3, destinationName (d), d > 0);

                // Its amount: from the middle, either way.
                part (i, 4, x, y, w, h);
                const float bh = h - rowH * 0.44f, by = y + rowH * 0.22f;
                g.fillRoundedRect (x, by, w, bh, 2.0f, 0x59000000u);
                const float amount = std::max (-1.0f, std::min (1.0f, amountOf (i)));
                const float mid = x + w * 0.5f, to = mid + w * 0.5f * amount;
                g.fillRect (std::min (mid, to), by, std::abs (to - mid), bh, withAlpha (exported::amountColour (accent, amount), on ? 0.9f : 0.35f));
                g.fillRect (mid, by, 1.0f, bh, withAlpha (text, 0.4f));
                g.drawText (exported::percent (amount), x, by, w, bh, std::min (rowH * 0.42f, 12.0f), text, Align::centre, true);
            }

            if (rows < routes)
                g.drawText ("+" + std::to_string (routes - rows) + " more", ax, ay + ah - 12.0f, aw, 12.0f, 10.0f, withAlpha (text, 0.5f), Align::right);
        }

        void paintGrid (Canvas& g)
        {
            const int rowsHere = (int) sources.size(), columnsHere = (int) columns.size();
            const float size = std::min (cellH * 0.55f, 13.0f);

            for (int r = 0; r < rowsHere; ++r)
                g.drawText (sources[(size_t) r], ax, ay + topH + cellH * (float) r, nameW - 6.0f, cellH, size, text, Align::right);

            for (int c = 0; c < columnsHere; ++c)
                g.drawText (destinationName (columns[(size_t) c]), ax + nameW + cellW * (float) c, ay, cellW, topH - 4.0f, std::min (size, cellW * 0.32f),
                            text, Align::centre);

            for (int r = 0; r < rowsHere; ++r)
            {
                for (int c = 0; c < columnsHere; ++c)
                {
                    const float x = ax + nameW + cellW * (float) c, y = ay + topH + cellH * (float) r;
                    const float cx = x + cellW * 0.5f, cy = y + cellH * 0.5f, m = std::min (cellW, cellH);
                    g.drawRect (x, y, cellW, cellH, 0.5f, line);
                    const int route = routeOf (r + 1, columns[(size_t) c]);

                    if (route < 0)
                    {
                        g.fillCircle (cx, cy, m * 0.13f, brighter (line, 0.25f));
                        continue;
                    }

                    const float amount = amountOf (route);
                    const auto colour = exported::amountColour (accent, amount);

                    if (cellKind == "bar")
                    {
                        const float bx = x + m * 0.18f, bw = cellW - m * 0.36f, bh = cellH - m * 0.36f, mid = cy, to = mid - bh * 0.5f * amount;
                        g.fillRect (bx, std::min (mid, to), bw, std::abs (to - mid), colour);
                    }
                    else if (cellKind == "knob")
                    {
                        const float r2 = m * 0.36f;
                        g.fillCircle (cx, cy, r2, 0xff2a2c31);
                        g.drawArc (cx, cy, r2, 0.0f, amount * 2.356f, std::max (1.5f, m * 0.08f), colour);
                        const float a = amount * 2.356f;
                        g.drawLine (cx, cy, cx + std::sin (a) * r2 * 0.8f, cy - std::cos (a) * r2 * 0.8f, 1.5f, colour);
                    }
                    else
                    {
                        g.fillCircle (cx, cy, m * 0.25f, colour);
                        g.fillCircle (cx - m * 0.04f, cy - m * 0.04f, m * 0.09f, 0x73ffffffu);
                        g.drawArc (cx, cy, m * 0.36f, 0.0f, amount * 6.16f, std::max (1.0f, m * 0.05f), withAlpha (colour, 0.8f));
                    }
                }
            }
        }

        void drawJack (Canvas& g, float cx, float cy, bool lit)
        {
            const auto& picture = lit && jackOn.isValid() ? jackOn : jackOff;

            if (picture.isValid())
            {
                g.drawImage (picture, std::round (cx - (float) picture.width * 0.5f), std::round (cy - (float) picture.height * 0.5f));
                return;
            }

            g.fillCircle (cx, cy, jack * 0.5f, 0xffb8bcc4);
            g.fillCircle (cx, cy, jack * 0.24f, 0xff050506);

            if (lit)
                g.drawEllipse (cx - jack * 0.6f, cy - jack * 0.6f, jack * 1.2f, jack * 1.2f, std::max (1.2f, jack * 0.07f), accent);
        }

        void paintPatch (Canvas& g)
        {
            const int outs = (int) sources.size(), ins = (int) columns.size();
            const float size = std::min (12.0f, jack * 0.45f);

            for (int i = 0; i < outs; ++i)
            {
                bool used = dragFrom == i;

                for (int r = 0; r < routes; ++r)
                    used = used || (active (r) && src (r) == i + 1);

                drawJack (g, outputX (i), outY, used);
                const float cw = aw / (float) std::max (1, outs);
                g.drawText (sources[(size_t) i], outputX (i) - cw * 0.5f, outY - jack * 1.33f, cw, jack * 0.7f, size, text, Align::centre);
            }

            for (int i = 0; i < ins; ++i)
            {
                const int d = columns[(size_t) i];
                float amount = 0.0f;
                bool used = false;

                for (int r = 0; r < routes; ++r)
                    if (active (r) && dst (r) == d)
                    {
                        used = true;
                        amount = amountOf (r);
                    }

                drawJack (g, inputX (i), inY, used);

                if (used)
                    g.drawArc (inputX (i), inY, jack * 0.78f, 0.0f, amount * 2.98f, std::max (1.5f, jack * 0.08f), exported::amountColour (accent, amount));

                const float cw = aw / (float) std::max (1, ins);
                g.drawText (destinationName (d), inputX (i) - cw * 0.5f, inY + jack * 0.6f, cw, jack * 0.7f, size, text, Align::centre);
            }

            // The cables, over everything.
            for (int r = 0; r < routes; ++r)
            {
                if (! active (r) || src (r) > outs)
                    continue;

                for (int i = 0; i < ins; ++i)
                    if (columns[(size_t) i] == dst (r))
                        exported::cable (g, outputX (src (r) - 1), outY, inputX (i), inY, jack * 0.3f, sag, withAlpha (routeColour (r), 0.92f));
            }

            if (dragFrom >= 0 && dragging)
                exported::cable (g, outputX (dragFrom), outY, dragX, dragY, jack * 0.3f, sag, withAlpha (routeColour (0), 0.8f));
        }

        //======================================================================
        void openList (int route, bool ofSources, float x, float y)
        {
            listRoute = route;
            listSources = ofSources;
            listHover = -1;
            const int count = (int) (ofSources ? sources.size() : destinations.size()) + 1;
            listRowsEach = std::max (1, std::min (count, 16));
            listColumns = (count + listRowsEach - 1) / listRowsEach;
            openPopup (x, y + 2.0f, listColumnW * (float) listColumns + 8.0f, listRowH * (float) listRowsEach + 8.0f);
        }

        void listCell (int i, float& x, float& y) const
        {
            x = 4.0f + listColumnW * (float) (i / listRowsEach);
            y = 4.0f + listRowH * (float) (i % listRowsEach);
        }

        int listAt (const Mouse& m) const
        {
            const int count = (int) (listSources ? sources.size() : destinations.size()) + 1;
            const int c = (int) std::floor ((m.x - 4.0f) / listColumnW), r = (int) std::floor ((m.y - 4.0f) / listRowH);

            if (c < 0 || r < 0 || r >= listRowsEach)
                return -1;

            const int i = c * listRowsEach + r;
            return i < count ? i : -1;
        }
    };

    STELLA_BUILTIN_ELEMENT (ModMatrix, "The studio's modulation matrix as an exported plugin draws it: slots, a grid or a patch bay. Made by the export from "
                                       "a matrix element; its routes are its roles src1, dst1, amt1...")

    //==========================================================================
    /** A preset manager as an exported plugin draws it: a drop list, a table of preset buttons
        (on the panel or in a popup), a side panel, or the bar. Its keys are pictures the export
        baked (the studio's flat or 3D keys); the names, its pages and its arrows are drawn here.
        Every one has the commands: Save, Save as, Load and More (a list's, a side panel's), or
        "..." beside a table's pages; More lists Quick save, the banks, Delete and the folder. */
    class PresetPanel : public Element
    {
    public:
        void resized() override
        {
            style = settingText ("style", "list");
            open = settingText ("open", "panel");
            headings = settingText ("categories", style == "list" ? "headings" : "none") == "headings" && style != "table";
            columns = std::max (1, std::min (8, (int) setting ("columns", 3.0f)));
            rowsDown = std::max (1, std::min (12, (int) setting ("rows", 4.0f)));
            rowH = std::max (14.0f, std::min (60.0f, setting ("rowHeight", 26.0f)));
            listRows = std::max (1, std::min (40, (int) setting ("listRows", 10.0f)));
            title = settingText ("title", "Presets");
            plate = setting ("plate", 1.0f) > 0.5f;
            onLeft = settingText ("side", "left") != "right";
            accent = exported::colourFrom (settingText ("colour"), 0xff2fb36b);
            panel = exported::colourFrom (settingText ("panelColour"), 0xff17191c);
            text = exported::colourFrom (settingText ("textColour"), 0xffd8dadf);
            line = exported::colourFrom (settingText ("lineColour"), 0xff2c2f35);
            label = exported::colourFrom (settingText ("labelColour"), 0xffc8cbd0);
            labelOn = exported::colourFrom (settingText ("labelOnColour"), 0xffffffff);
            textSize = std::max (20.0f, std::min (100.0f, setting ("keyTextSize", 60.0f))) * 0.01f;

            // Its bar in LED digits: its number and name.
            barLed = settingText ("barDisplay") == "led";
            led.lit = exported::colourFrom (settingText ("ledColour"), 0xffff3b2a);
            led.background = exported::colourFrom (settingText ("ledBackground"), 0xff0c0a0a);
            const auto segments = settingText ("ledSegments");
            led.segments = segments == "7" ? 7 : segments == "14" ? 14 : segments == "16" ? 16 : 0;
            led.cells = std::max (0, std::min (64, (int) setting ("ledCells", 0.0f)));
            led.ghost = std::max (0.0f, std::min (60.0f, setting ("ledGhost", 10.0f))) * 0.01f;
            led.glow = std::max (0.0f, std::min (100.0f, setting ("ledGlow", 40.0f))) * 0.01f;
            led.slant = std::max (0.0f, std::min (15.0f, setting ("ledSlant", 6.0f)));
            led.thickness = std::max (6.0f, std::min (30.0f, setting ("ledThickness", 16.0f))) * 0.01f;
            const auto window = settingText ("ledWindow");
            led.window = window == "none" ? 0 : window == "3d" ? 2 : 1;
            ledDigits = std::max (0, std::min (4, (int) setting ("ledNumber", 2.0f)));

            // A plain preset manager: "<  name  >", its picture the export's (arrows and all).
            bareBar = setting ("bareBar", 0.0f) > 0.5f;
            barTextSize = setting ("barTextSize", 0.0f);

            const auto prefix = settingText ("pictures");
            bar = picture (prefix + "_bar.png");
            row = picture (prefix + "_row.png");
            rowOn = picture (prefix + "_rowOn.png");
            cell = picture (prefix + "_cell.png");
            cellOn = picture (prefix + "_cellOn.png");
        }

        void paint (Canvas& g) override
        {
            if (style == "side" || (style == "table" && open != "popup"))
                paintBody (g, 0.0f, 0.0f, (float) width(), (float) height(), false);
            else
                paintBar (g);
        }

        void paintPopup (Canvas& g) override
        {
            if (more)
                paintCommands (g, (float) g.width(), (float) g.height());
            else
                paintBody (g, 0.0f, 0.0f, (float) g.width(), (float) g.height(), true);
        }

        bool mouseDown (const Mouse& m) override
        {
            if (onPanel())
            {
                bodyClick (m, (float) width(), (float) height(), false);
                return false;
            }

            // The bar: the arrows step; the name opens the list or the table.
            const float arrows = bareBar ? (float) width() * 0.22f : std::min (24.0f, (float) height()) + 4.0f;

            if (m.x < arrows)                         { stepPreset (-1); return false; }
            if (m.x > (float) width() - arrows)       { stepPreset (1); return false; }

            more = false;
            float w, h;
            popupSize (w, h);
            openPopup (0.0f, (float) height() + 4.0f, w, h);
            return false;
        }

        void popupMouseDown (const Mouse& m) override
        {
            if (more)
            {
                commandClick (m);
                return;
            }

            float w, h;
            popupSize (w, h);

            if (bodyClick (m, w, h, true))
                closePopup();
        }

        void popupClosed() override
        {
            more = false;
        }

        void mouseWheel (const Mouse&, float notches) override
        {
            if (onPanel())
                turnPage (notches < 0.0f ? 1 : -1, (float) width(), (float) height());
            else
                stepPreset (notches < 0.0f ? 1 : -1);
        }

        void popupMouseWheel (const Mouse&, float notches) override
        {
            if (more)
                return;

            float w, h;
            popupSize (w, h);
            turnPage (notches < 0.0f ? 1 : -1, w, h);
        }

    private:
        std::string style, open, title;
        bool headings = false, plate = true, onLeft = true;
        int columns = 3, rowsDown = 4, listRows = 10, page = 0;
        float rowH = 26.0f, textSize = 0.6f;
        Colour accent = 0, panel = 0, text = 0, line = 0, label = 0, labelOn = 0;
        Image bar, row, rowOn, cell, cellOn;
        bool barLed = false, bareBar = false;
        float barTextSize = 0.0f;
        int ledDigits = 2;

        // More: the commands, in a popup of their own (from a popup: in its place, with Back).
        bool more = false, moreFromPopup = false;

        struct Command { const char* name; PresetCommand command; };

        static constexpr int numCommands = 7;
        static constexpr float commandH = 24.0f, actionsH = 28.0f, actionsGap = 6.0f, moreW = 30.0f;

        static const Command& commandAt (int i)
        {
            static const Command commands[numCommands] = {
                { "Quick save",          PresetCommand::save },
                { "Save as\xe2\x80\xa6",    PresetCommand::saveAs },
                { "Load preset\xe2\x80\xa6", PresetCommand::load },
                { "Save bank\xe2\x80\xa6",  PresetCommand::saveBank },
                { "Load bank\xe2\x80\xa6",  PresetCommand::loadBank },
                { "Delete preset",       PresetCommand::remove },
                { "Show folder",         PresetCommand::showFolder }
            };

            return commands[std::max (0, std::min (numCommands - 1, i))];
        }

        bool onPanel() const    { return style == "side" || (style == "table" && open != "popup"); }

        /** A list's (or a side panel's) Save, Save as, Load, More: the row at its bottom. */
        bool actionsBox (float W, float H, float& x, float& y, float& w) const
        {
            if (table())
                return false;

            const float pad = style == "side" ? 10.0f : 8.0f;
            x = pad;
            y = H - pad - actionsH;
            w = W - 2.0f * pad;
            return w > 40.0f;
        }

        /** A table's "...": beside its pages. */
        bool moreBox (float W, float navY, float& x, float& y) const
        {
            if (! table() || navY < 0.0f)
                return false;

            x = W - (style == "side" ? 10.0f : 8.0f) - moreW;
            y = navY;
            return true;
        }

        void openCommands (float x, float y, bool fromPopup)
        {
            more = true;
            moreFromPopup = fromPopup;
            const float h = 8.0f + commandH * (float) (numCommands + (fromPopup ? 1 : 0)) + 8.0f;

            if (fromPopup)
                openPopup (0.0f, (float) height() + 4.0f, std::max (190.0f, (float) width()), h);
            else
                openPopup (x, y, 190.0f, h);
        }

        void paintCommands (Canvas& g, float W, float H)
        {
            g.fillRoundedRect (1.0f, 3.0f, W - 2.0f, H - 3.0f, 8.0f, 0x59000000u);
            g.fillGradient (0.0f, 0.0f, W, H, brighter (panel, 0.06f), darker (panel, 0.12f), 8.0f);
            g.drawRoundedRect (0.5f, 0.5f, W - 1.0f, H - 1.0f, 8.0f, 1.0f, line);

            for (int i = 0; i < numCommands + (moreFromPopup ? 1 : 0); ++i)
            {
                const float y = 8.0f + commandH * (float) i;

                if (i == numCommands)
                {
                    g.fillRect (12.0f, y, W - 24.0f, 1.0f, line);
                    g.drawText ("Back", 14.0f, y, W - 28.0f, commandH, 12.5f, withAlpha (text, 0.8f), Align::left, true);
                    continue;
                }

                const auto& c = commandAt (i);
                const bool can = canDoPreset (c.command);
                g.drawText (c.name, 14.0f, y, W - 28.0f, commandH, 12.5f, can ? text : withAlpha (text, 0.3f), Align::left, true);
            }
        }

        void commandClick (const Mouse& m)
        {
            const int i = (int) std::floor ((m.y - 8.0f) / commandH);

            if (i == numCommands && moreFromPopup)
            {
                // Back: the list (or the table) again.
                more = false;
                float w, h;
                popupSize (w, h);
                openPopup (0.0f, (float) height() + 4.0f, w, h);
                return;
            }

            if (i < 0 || i >= numCommands)
                return;

            const auto command = commandAt (i).command;

            if (! canDoPreset (command))
                return;

            more = false;
            closePopup();
            doPreset (command);
        }

        void paintActions (Canvas& g, float x0, float y0, float W, float H)
        {
            float x, y, w;

            if (! actionsBox (W, H, x, y, w))
                return;

            static const char* names[4] = { "Save", "Save as", "Load", "More\xe2\x80\xa6" };
            const float gap = 4.0f, each = (w - gap * 3.0f) / 4.0f;

            for (int i = 0; i < 4; ++i)
            {
                const float bx = x0 + x + (float) i * (each + gap), by = y0 + y;
                const bool can = i == 3 || canDoPreset (i == 0 ? PresetCommand::save : i == 1 ? PresetCommand::saveAs : PresetCommand::load);
                g.fillGradient (bx, by, each, actionsH, brighter (panel, 0.16f), darker (panel, 0.04f), 5.0f);
                g.drawRoundedRect (bx + 0.5f, by + 0.5f, each - 1.0f, actionsH - 1.0f, 5.0f, 1.0f, line);
                g.drawText (names[i], bx + 2.0f, by, each - 4.0f, actionsH, std::min (12.0f, each * 0.2f + 2.0f),
                            can ? label : withAlpha (label, 0.3f), Align::centre, true);
            }
        }

        /** A click on the commands (the row, or "..."): true when it was one. */
        bool actionsClick (const Mouse& m, float W, float H, float navY, bool popup)
        {
            float x = 0.0f, y = 0.0f, w = 0.0f;

            if (moreBox (W, navY, x, y))
            {
                if (m.x >= x && m.y >= y && m.x < x + moreW && m.y < y + 22.0f)
                {
                    openCommands (x + moreW - 190.0f, y + 24.0f, popup);
                    return true;
                }

                return false;
            }

            if (! actionsBox (W, H, x, y, w) || m.y < y || m.y >= y + actionsH || m.x < x || m.x >= x + w)
                return false;

            const float gap = 4.0f, each = (w - gap * 3.0f) / 4.0f;
            const int i = std::max (0, std::min (3, (int) ((m.x - x) / (each + gap))));

            if (i == 3)
            {
                openCommands (x + w - 190.0f, y + actionsH + 2.0f, popup);
                return true;
            }

            const auto command = i == 0 ? PresetCommand::save : i == 1 ? PresetCommand::saveAs : PresetCommand::load;

            if (canDoPreset (command))
            {
                if (popup)
                    closePopup();

                doPreset (command);
            }

            return true;
        }
        exported::seg::Style led;

        /** Its LED text: the preset's number and name, padded to its longest name; how many cells. */
        std::string ledText (int& cells) const
        {
            int longest = 0;

            for (int i = 0; i < numPresets(); ++i)
                longest = std::max (longest, exported::seg::cellsOf (presetName (i).substr (0, 24)));

            const int current = currentPreset();
            std::string text;

            if (ledDigits > 0)
            {
                if (current >= 0)
                {
                    auto number = std::to_string (current + 1);
                    text = std::string ((size_t) std::max (0, ledDigits - (int) number.size()), '0') + number;
                }
                else
                {
                    text = std::string ((size_t) ledDigits, '-');
                }
            }

            if (longest > 0 || numPresets() == 0)
            {
                auto name = current >= 0 ? presetName (current).substr (0, 24) : std::string (numPresets() == 0 ? "NO PRESETS" : "");

                for (auto& c : name)
                    c = (char) std::toupper ((unsigned char) c);

                text += (ledDigits > 0 ? " " : "") + name + std::string ((size_t) std::max (0, longest - exported::seg::cellsOf (name)), ' ');
            }

            cells = std::max (exported::seg::cellsOf (text), ledDigits + (longest > 0 ? 1 + longest : 0));
            return text;
        }

        struct Entry { int preset; std::string heading; float x, y, w, h; };

        bool table() const    { return style == "table"; }

        void popupSize (float& w, float& h) const
        {
            if (table())
            {
                w = std::max ((float) width(), (float) columns * 104.0f + 16.0f);
                h = 16.0f + (float) rowsDown * 40.0f + 26.0f;
                return;
            }

            const int count = entryCount();
            w = std::max ((float) width(), 190.0f);
            h = 16.0f + actionsH + actionsGap + rowH * (float) std::max (3, std::min (listRows, std::max (3, count))) + (count > listRows ? 26.0f : 0.0f) + 2.0f;
        }

        int entryCount() const
        {
            int count = 0;
            std::string last = "\x01";

            for (int i = 0; i < numPresets(); ++i)
            {
                if (headings && presetGroup (i) != last)
                {
                    last = presetGroup (i);
                    ++count;
                }

                ++count;
            }

            return count;
        }

        /** Its rows (or cells) on this page, and how many pages. */
        std::vector<Entry> layOut (float W, float H, int& pages, float& navY, float& itemsY) const
        {
            const bool side = style == "side";
            const float pad = side ? 10.0f : 8.0f;
            float x = pad, y = pad, w = W - 2.0f * pad, h = H - 2.0f * pad;

            if (side)
            {
                y += 32.0f;
                h -= 32.0f;
            }

            if (! table())
                h -= actionsH + actionsGap;   // Save, Save as, Load, More

            itemsY = y;
            std::vector<Entry> all;
            std::string last = "\x01";

            for (int i = 0; i < numPresets(); ++i)
            {
                if (headings && presetGroup (i) != last)
                {
                    last = presetGroup (i);
                    all.push_back ({ -1, last.empty() ? std::string ("Presets") : last, 0, 0, 0, 0 });
                }

                all.push_back ({ i, {}, 0, 0, 0, 0 });
            }

            int capacity = 1;
            navY = -1.0f;

            if (table())
            {
                h -= 26.0f;
                navY = y + h + 4.0f;
                capacity = columns * rowsDown;
            }
            else
            {
                capacity = std::max (1, (int) std::floor (h / rowH));

                if ((int) all.size() > capacity)
                {
                    h -= 26.0f;
                    navY = y + h + 4.0f;
                    capacity = std::max (1, (int) std::floor (h / rowH));
                }
            }

            pages = std::max (1, ((int) all.size() + capacity - 1) / capacity);
            const int shown = std::max (0, std::min (pages - 1, page));
            std::vector<Entry> onPage;

            for (int k = shown * capacity; k < (int) all.size() && k < (shown + 1) * capacity; ++k)
            {
                auto e = all[(size_t) k];
                const int at = k - shown * capacity;

                if (table())
                {
                    const float cw = w / (float) columns, ch = h / (float) rowsDown;
                    e.x = x + cw * (float) (at % columns);
                    e.y = y + ch * (float) (at / columns);
                    e.w = cw;
                    e.h = ch;
                }
                else
                {
                    e.x = x;
                    e.y = y + rowH * (float) at;
                    e.w = w;
                    e.h = rowH;
                }

                onPage.push_back (e);
            }

            return onPage;
        }

        void paintBar (Canvas& g)
        {
            const float W = (float) width(), H = (float) height();

            if (bareBar)
            {
                // "<  name  >": its picture has the arrows.
                if (bar.isValid())
                    g.drawImage (bar, 0.0f, 0.0f);

                const float arrow = std::min (H * 0.32f, 8.0f), inset = 14.0f + arrow * 2.0f;
                const int current = currentPreset();
                const auto name = current >= 0 ? presetName (current) + (isPresetModified() ? " *" : "")
                                               : std::string (numPresets() > 0 ? "Presets" : "No presets");
                g.drawText (name, inset, 0.0f, std::max (1.0f, W - 2.0f * inset), H, barTextSize > 0.0f ? barTextSize : std::min (14.0f, H * 0.5f),
                            label, Align::centre, true);
                return;
            }

            if (bar.isValid())
                g.drawImage (bar, 0.0f, 0.0f);
            else
                g.fillRoundedRect (0.0f, 0.0f, W, H, 4.0f, 0x8c000000u);

            const float arrows = std::min (24.0f, H), a = arrows * 0.22f;
            const float lx = 4.0f + arrows * 0.5f, rx = W - 4.0f - arrows * 0.5f, cy = H * 0.5f;
            g.fillTriangle (lx + a * 0.6f, cy - a, lx + a * 0.6f, cy + a, lx - a * 0.7f, cy, accent);
            g.fillTriangle (rx - a * 0.6f, cy - a, rx - a * 0.6f, cy + a, rx + a * 0.7f, cy, accent);

            if (style != "bar")
            {
                const float dx = rx - arrows * 0.5f - 10.0f;
                g.fillTriangle (dx - 4.0f, cy - 2.0f, dx + 4.0f, cy - 2.0f, dx, cy + 3.0f, withAlpha (accent, 0.8f));
            }

            // LED digits between the arrows.
            if (barLed)
            {
                int cells = 0;
                const auto text = ledText (cells);
                const float left = 4.0f + arrows + 2.0f, right = W - 4.0f - arrows - 2.0f - (style != "bar" ? 16.0f : 0.0f);
                const float pad = std::min (3.0f, H * 0.1f);
                exported::seg::display (g, left, pad, right - left, H - pad * 2.0f, text, led, cells);
                return;
            }

            const int current = currentPreset();
            const auto name = current >= 0 ? presetName (current) + (isPresetModified() ? " *" : "") : std::string (numPresets() > 0 ? "Presets" : "No presets");
            g.drawText (name, arrows + 8.0f, 0.0f, W - 2.0f * arrows - 30.0f, H, std::min (15.0f, H * 0.48f), label, Align::centre, true);
        }

        void paintBody (Canvas& g, float x0, float y0, float W, float H, bool popup)
        {
            if (style == "side")
            {
                g.fillGradient (x0, y0, W, H, brighter (panel, 0.07f), darker (panel, 0.1f));
                g.drawRect (x0, y0, W, H, 1.0f, line);
                const float seam = onLeft ? x0 + W - 1.0f : x0;
                g.fillRect (seam, y0, 1.0f, H, 0x80000000u);

                for (const float at : { 0.14f, 0.5f, 0.86f })
                {
                    const float by = y0 + H * at - 15.0f, bx = seam - 7.0f;
                    g.fillGradient (bx, by, 14.0f, 30.0f, brighter (line, 0.5f), darker (line, 0.2f), 3.0f);
                    g.fillCircle (bx + 7.0f, by + 8.0f, 2.0f, 0x8c000000u);
                    g.fillCircle (bx + 7.0f, by + 22.0f, 2.0f, 0x8c000000u);
                }

                g.drawText (title, x0 + 10.0f, y0 + 10.0f, W - 20.0f, 26.0f, 13.0f, accent, Align::left, true);
                g.fillRect (x0 + 10.0f, y0 + 38.0f, W - 20.0f, 1.0f, line);
            }
            else if (popup || plate)
            {
                if (popup)
                    g.fillRoundedRect (x0 + 1.0f, y0 + 3.0f, W - 2.0f, H - 3.0f, 8.0f, 0x59000000u);

                g.fillGradient (x0, y0, W, H, brighter (panel, 0.06f), darker (panel, 0.12f), 8.0f);
                g.drawRoundedRect (x0 + 0.5f, y0 + 0.5f, W - 1.0f, H - 1.0f, 8.0f, 1.0f, line);
            }

            int pages = 1;
            float navY = -1.0f, itemsY = 0.0f;
            const auto entries = layOut (W, H, pages, navY, itemsY);
            const int current = currentPreset();

            if (numPresets() == 0)
                g.drawText ("No presets", x0, y0 + itemsY, W, 30.0f, 13.0f, withAlpha (text, 0.45f));

            for (const auto& e : entries)
            {
                if (e.preset < 0)
                {
                    g.drawText (e.heading, x0 + e.x + 4.0f, y0 + e.y, e.w - 8.0f, e.h - 3.0f, 11.0f, withAlpha (accent, 0.85f), Align::left, true);
                    g.fillRect (x0 + e.x + 4.0f, y0 + e.y + e.h - 2.0f, e.w - 8.0f, 1.0f, line);
                    continue;
                }

                const bool on = e.preset == current;
                const auto& picture = table() ? (on ? cellOn : cell) : (on ? rowOn : row);

                if (picture.isValid())
                    g.drawImage (picture, std::round (x0 + e.x), std::round (y0 + e.y));
                else
                    g.fillRoundedRect (x0 + e.x + 2.0f, y0 + e.y + 1.0f, e.w - 4.0f, e.h - 2.0f, 4.0f, on ? withAlpha (accent, 0.4f) : 0x33000000u);

                const auto name = presetName (e.preset) + (on && isPresetModified() ? " *" : "");
                g.drawText (name, x0 + e.x + 4.0f, y0 + e.y, e.w - 8.0f, e.h, std::max (8.0f, std::min (e.h * 0.8f, (table() ? 30.0f : rowH) * 0.8f) * textSize),
                            on ? labelOn : label, Align::centre, true);
            }

            if (navY >= 0.0f)
            {
                const float ny = y0 + navY, pad = style == "side" ? 10.0f : 8.0f, end = table() ? moreW + 4.0f : 0.0f;
                const auto left = page > 0 ? accent : withAlpha (accent, 0.25f), right = page < pages - 1 ? accent : withAlpha (accent, 0.25f);
                g.fillTriangle (x0 + pad + 18.0f, ny + 6.0f, x0 + pad + 18.0f, ny + 16.0f, x0 + pad + 10.0f, ny + 11.0f, left);
                g.fillTriangle (x0 + W - pad - end - 18.0f, ny + 6.0f, x0 + W - pad - end - 18.0f, ny + 16.0f, x0 + W - pad - end - 10.0f, ny + 11.0f, right);
                g.drawText (std::to_string (std::min (page, pages - 1) + 1) + " / " + std::to_string (pages), x0, ny, W - end, 22.0f, 11.5f, withAlpha (text, 0.7f));
            }

            // "...": the commands (a table's); a list's row of them.
            float mx = 0.0f, my = 0.0f;

            if (moreBox (W, navY, mx, my))
                for (int i = -1; i <= 1; ++i)
                    g.fillCircle (x0 + mx + moreW * 0.5f + (float) i * 6.0f, y0 + my + 11.0f, 1.75f, accent);

            paintActions (g, x0, y0, W, H);
        }

        void turnPage (int by, float W, float H)
        {
            int pages = 1;
            float navY = -1.0f, itemsY = 0.0f;
            layOut (W, H, pages, navY, itemsY);
            page = std::max (0, std::min (pages - 1, page + by));
            repaint();
        }

        /** A click in the list or the table: true when a preset was chosen. */
        bool bodyClick (const Mouse& m, float W, float H, bool popup)
        {
            int pages = 1;
            float navY = -1.0f, itemsY = 0.0f;
            const auto entries = layOut (W, H, pages, navY, itemsY);

            if (actionsClick (m, W, H, navY, popup))
                return false;

            for (const auto& e : entries)
                if (e.preset >= 0 && m.x >= e.x && m.y >= e.y && m.x < e.x + e.w && m.y < e.y + e.h)
                {
                    choosePreset (e.preset);
                    repaint();
                    return true;
                }

            if (navY >= 0.0f && m.y >= navY && m.y < navY + 22.0f)
                turnPage (m.x < (W - (table() ? moreW + 4.0f : 0.0f)) * 0.5f ? -1 : 1, W, H);

            return false;
        }
    };

    STELLA_BUILTIN_ELEMENT (PresetPanel, "A preset manager as an exported plugin draws it: a drop list, a table of preset buttons (on the panel or in a "
                                         "popup), a side panel or the bar. Made by the export from a preset manager's design.")
}

