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

#include "stella_elements.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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

            g.fillGradient (x, y, w, h, hovering || open ? 0xff34343c : 0xff26262c, hovering || open ? 0xff2a2a31 : 0xff1c1c21, 4.0f);
            g.drawRoundedRect (x, y, w, h, 4.0f, 1.0f, open ? accent : (hovering ? 0x50ffffff : 0x34ffffff));
            g.drawText (optionText (current()), x + 10.0f, y, w - 34.0f, h, std::min (13.5f, std::max (9.0f, h * 0.45f)), 0xffeeeeee, Align::left);

            // The arrow: down, or up while the list is open.
            const float ax = x + w - 15.0f, ay = y + h * 0.5f, s = std::min (5.0f, h * 0.18f);

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
}
