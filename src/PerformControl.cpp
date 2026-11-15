// D:\workspace\Fanan_web_hub\src\PerformControl.cpp

#include "PerformControl.h"

#include "Body3D.h"
#include "Preferences.h"
#include "Render3D.h"

#include <cmath>
#include <list>
#include <map>
#include <mutex>

namespace
{
    constexpr float pi = 3.14159265358979f;

    float numberOf (const juce::var& v, const juce::Identifier& key, float fallback)
    {
        const auto value = v.getProperty (key, {});
        return value.isVoid() || (value.isString() && value.toString().trim().isEmpty()) ? fallback : (float) (double) value;
    }

    juce::String textOf (const juce::var& v, const juce::Identifier& key, const juce::String& fallback)
    {
        const auto value = v.getProperty (key, {}).toString().trim().toLowerCase();
        return value.isEmpty() ? fallback : value;
    }

    juce::Colour colourOf (const juce::var& v, const juce::Identifier& key, juce::Colour fallback)
    {
        auto text = v.getProperty (key, {}).toString().trim().removeCharacters ("#");

        if (text.isEmpty())
            return fallback;

        if (text.length() == 6)
            text = "FF" + text;

        return juce::Colour ((juce::uint32) text.getHexValue64());
    }

    juce::String hex (juce::Colour c)    { return "#" + c.toDisplayString (true); }

    juce::var object()    { return juce::var (new juce::DynamicObject()); }

    /** The pieces of a 3D picture, as Body3D parts. */
    struct Parts
    {
        juce::Array<juce::var> list;

        juce::var add (const juce::String& kind, float x, float z, float y, float w, float h, float d, juce::Colour colour,
                       const juce::String& material = "Satin plastic")
        {
            auto p = object();
            auto* o = p.getDynamicObject();
            o->setProperty ("kind", kind);
            o->setProperty ("x", x);
            o->setProperty ("z", z);
            o->setProperty ("y", y);
            o->setProperty ("w", juce::jmax (0.6f, w));
            o->setProperty ("h", juce::jmax (0.6f, h));
            o->setProperty ("d", juce::jmax (0.6f, d));
            o->setProperty ("color", hex (colour));
            o->setProperty ("material", material);
            list.add (p);
            return p;
        }
    };

    void set (const juce::var& p, const juce::Identifier& key, const juce::var& value)
    {
        if (auto* o = p.getDynamicObject())
            o->setProperty (key, value);
    }

    /** Where a part leaning about a pivot has its middle: up its height, turned by Rz Rx. */
    juce::Point<float> leanOffset (float halfHeight, float ax, float az, float& dy)
    {
        const float rx = ax * pi / 180.0f, rz = az * pi / 180.0f;
        const float ux = -std::sin (rz) * std::cos (rx), uy = std::cos (rz) * std::cos (rx), uz = std::sin (rx);
        dy = halfHeight * uy;
        return { halfHeight * ux, halfHeight * uz };
    }

    //==========================================================================
    /** A wheel in its slot at (cx, cz): its value 0..1 turning it (ribs and mark move round). */
    void addWheel (Parts& parts, const juce::var& d, float cx, float cz, float slotW, float slotD, float housingH, bool vertical, float value)
    {
        const auto partColour = colourOf (d, "partColour", juce::Colour (0xff1c1d20));
        const auto accent = colourOf (d, "accentColour", juce::Colour (0xffeef0f2));
        const float R = (vertical ? slotD : slotW) * 0.62f;
        const float thick = (vertical ? slotW : slotD) * 0.86f;
        const float cy = housingH - R * 0.8f;

        // The slot, and a light in it.
        auto slot = parts.add ("box", cx, cz, housingH * 0.25f, slotW, housingH, slotD, juce::Colours::black);
        set (slot, "op", "cut");
        set (slot, "round", juce::jmin (slotW, slotD) * 0.12f);

        if ((bool) d.getProperty ("lit", false))
        {
            auto light = parts.add ("box", cx, cz, housingH * 0.25f, slotW * 0.96f, 0.8f, slotD * 0.96f, accent, "Gloss plastic");
            set (light, "glow", hex (accent));
            set (light, "glowStrength", numberOf (d, "glow", 60.0f));
        }

        // The wheel, its axle across the slot.
        auto wheel = parts.add ("cylinder", cx, cz, cy - R, vertical ? thick : 2.0f * R, 2.0f * R, vertical ? 2.0f * R : thick, partColour,
                                d.getProperty ("material", "Rubber").toString());
        set (wheel, "axis", vertical ? "x" : "z");
        set (wheel, "round", thick * 0.12f);

        // Its ribs and its mark, turned with it: up to 40 degrees each way (clean: a plain wheel).
        const float turn = (value - 0.5f) * 80.0f;
        const int ribs = (bool) d.getProperty ("clean", false) ? -1 : 7;

        for (int k = -ribs; k <= ribs; ++k)
        {
            const float angle = turn + (float) k * 11.0f;

            if (std::abs (angle) > 46.0f)
                continue;

            const float a = angle * pi / 180.0f;
            const bool mark = k == 0;
            const float along = R * std::sin (a), up = R * std::cos (a);
            const float rw = thick * (mark ? 0.98f : 0.94f), rt = R * (mark ? 0.07f : 0.035f);
            auto rib = parts.add ("box", vertical ? cx : cx + along, vertical ? cz - along : cz, cy + up - rt * 0.5f,
                                  vertical ? rw : rt, rt, vertical ? rt : rw, mark ? accent : partColour.brighter (0.25f), mark ? "Gloss plastic" : "Rubber");
            set (rib, "rotate", vertical ? juce::Array<juce::var> { -angle, 0, 0 } : juce::Array<juce::var> { 0, 0, -angle });

            if (mark && (bool) d.getProperty ("lit", false))
            {
                set (rib, "glow", hex (accent));
                set (rib, "glowStrength", numberOf (d, "glow", 60.0f) * 0.6f);
            }
        }
    }

    /** A stick leaning about its foot (pivot), with a cap on top: a sphere or a cylinder. */
    void addStick (Parts& parts, float px, float pz, float py, float length, float thickness, float ax, float az, juce::Colour colour,
                   const juce::String& material, const juce::String& capKind, float capW, float capH, juce::Colour capColour, const juce::String& capMaterial)
    {
        float dy = 0;
        const auto off = leanOffset (length * 0.5f, ax, az, dy);
        auto stick = parts.add ("cylinder", px + off.x, pz + off.y, py + dy - length * 0.5f, thickness, length, thickness, colour, material);
        set (stick, "rotate", juce::Array<juce::var> { ax, 0, az });
        set (stick, "round", thickness * 0.3f);

        float dy2 = 0;
        const auto top = leanOffset (length, ax, az, dy2);
        auto cap = parts.add (capKind, px + top.x, pz + top.y, py + dy2 - capH * 0.5f, capW, capH, capW, capColour, capMaterial);
        set (cap, "rotate", juce::Array<juce::var> { ax, 0, az });

        if (capKind == "cylinder")
            set (cap, "round", capH * 0.35f);
    }

    //==========================================================================
    juce::var bodyFor (const juce::var& d, float W, float H, float x, float y)
    {
        const auto kind = textOf (d, "kind", "joystick");
        const auto style = textOf (d, "style", "ball");
        const bool vertical = (bool) d.getProperty ("vertical", kind != "ribbon");
        const float m = juce::jmin (W, H);
        const float cx = W * 0.5f, cz = H * 0.5f;
        const float housingH = juce::jmax (2.0f, m * 0.09f);
        const auto housing = colourOf (d, "housingColour", juce::Colour (0xff1a1b1e));
        const auto partColour = colourOf (d, "partColour", juce::Colour (0xff202124));
        const auto accent = colourOf (d, "accentColour", juce::Colour (0xff5fe0e8));
        const auto material = d.getProperty ("material", "Satin plastic").toString();
        const float glow = numberOf (d, "glow", 60.0f);
        const float u = x * 2.0f - 1.0f, v = y * 2.0f - 1.0f;   // -1..1, v up (to the back)

        Parts parts;
        const bool round = kind == "joystick" && (style == "ball" || style == "thumb" || style == "gimbal");
        auto base = parts.add (round ? "cylinder" : "box", cx, cz, 0.0f, (round ? m : W) * 0.94f, housingH, (round ? m : H) * 0.94f, housing);
        set (base, "round", housingH * 0.45f);

        if (kind == "wheel")
        {
            addWheel (parts, d, cx, cz, vertical ? W * 0.42f : W * 0.84f, vertical ? H * 0.84f : H * 0.42f, housingH, vertical, vertical ? y : x);
        }
        else if (kind == "wheels")
        {
            addWheel (parts, d, W * 0.28f, cz, W * 0.24f, H * 0.84f, housingH, true, x);
            addWheel (parts, d, W * 0.72f, cz, W * 0.24f, H * 0.84f, housingH, true, y);
        }
        else if (kind == "lever")
        {
            // The bender: its slot, the paddle leaning across (bend) and away (mod).
            auto slot = parts.add ("box", cx, cz, housingH * 0.3f, W * 0.62f, housingH, H * 0.34f, juce::Colours::black);
            set (slot, "op", "cut");
            set (slot, "round", H * 0.06f);
            // Across: bends either way; away and toward the player: modulates either way. Its
            // angle (push and pull): the whole sweep, edge to edge.
            const float lean = PerformControl::rangeOf (d) * 0.5f;
            const float az = -u * lean, ax = -v * lean * 0.75f;
            const float length = m * 0.55f;
            float dy = 0;
            const auto off = leanOffset (length * 0.5f, ax, az, dy);
            auto paddle = parts.add ("box", cx + off.x, cz + off.y, housingH * 0.3f + dy - length * 0.5f, W * 0.16f, length, H * 0.07f, partColour, material);
            set (paddle, "rotate", juce::Array<juce::var> { ax, 0, az });
            set (paddle, "round", H * 0.03f);
            float dy2 = 0;
            const auto top = leanOffset (length, ax, az, dy2);
            auto grip = parts.add ("box", cx + top.x, cz + top.y, housingH * 0.3f + dy2 - m * 0.06f, W * 0.32f, m * 0.12f, H * 0.16f, partColour.brighter (0.15f), material);
            set (grip, "rotate", juce::Array<juce::var> { ax, 0, az });
            set (grip, "round", m * 0.045f);
        }
        else if (kind == "pad")
        {
            const auto surface = juce::Rectangle<float> (W * 0.86f, H * 0.86f).withCentre ({ cx, cz });
            auto glass = parts.add ("box", cx, cz, housingH * 0.5f, surface.getWidth(), housingH * 0.6f, surface.getHeight(), partColour, "Gloss plastic");
            set (glass, "glass", 35);
            set (glass, "round", m * 0.02f);

            if ((bool) d.getProperty ("lit", true))
            {
                set (glass, "glow", hex (accent.withMultipliedBrightness (0.4f)));
                set (glass, "glowStrength", glow * 0.15f);
            }

            const float px = surface.getX() + surface.getWidth() * x, pz = surface.getBottom() - surface.getHeight() * y;
            const float top = housingH * 1.1f;

            for (int line = 0; line < 2; ++line)
            {
                auto hair = parts.add ("box", line == 0 ? px : cx, line == 0 ? cz : pz, top - 0.6f, line == 0 ? 1.2f : surface.getWidth(), 0.8f,
                                       line == 0 ? surface.getHeight() : 1.2f, accent, "Gloss plastic");
                set (hair, "glow", hex (accent));
                set (hair, "glowStrength", glow * 0.35f);
            }

            auto puck = parts.add ("cylinder", px, pz, top - 0.5f, m * 0.13f, juce::jmax (1.5f, m * 0.03f), m * 0.13f, accent, "Gloss plastic");
            set (puck, "round", m * 0.012f);
            set (puck, "glow", hex (accent));
            set (puck, "glowStrength", glow);
        }
        else if (kind == "ribbon")
        {
            const float along = vertical ? H * 0.88f : W * 0.88f, across = vertical ? W * 0.5f : H * 0.5f;
            auto strip = parts.add ("box", cx, cz, housingH * 0.6f, vertical ? across : along, housingH * 0.5f, vertical ? along : across, partColour,
                                    d.getProperty ("material", "Rubber").toString());
            set (strip, "round", across * 0.2f);
            const float value = vertical ? y : x;
            const float top = housingH * 1.1f;

            if ((bool) d.getProperty ("leds", false))
            {
                // Lamps along it: lit up to where it's touched (from the middle, bending).
                const int count = 15;
                const bool fromMiddle = (bool) d.getProperty ("fromMiddle", false);

                for (int i = 0; i < count; ++i)
                {
                    const float t = ((float) i + 0.5f) / (float) count;
                    const float pos = (t - 0.5f) * along;
                    const bool on = fromMiddle ? (value >= 0.5f ? t >= 0.5f - 0.5f / count && t <= value : t <= 0.5f + 0.5f / count && t >= value) : t <= value;
                    auto lamp = parts.add ("sphere", vertical ? cx : cx + pos, vertical ? cz - pos : cz, top - 0.8f, across * 0.22f, across * 0.12f, across * 0.22f,
                                           on ? accent : accent.darker (0.85f), "Gloss plastic");
                    set (lamp, "glass", 30);

                    if (on)
                    {
                        set (lamp, "glow", hex (accent));
                        set (lamp, "glowStrength", glow);
                    }
                }
            }
            else
            {
                const float pos = (value - 0.5f) * along;
                auto marker = parts.add ("box", vertical ? cx : cx + pos, vertical ? cz - pos : cz, top - 0.6f, vertical ? across * 1.05f : 2.0f, 1.2f,
                                         vertical ? 2.0f : across * 1.05f, accent, "Gloss plastic");
                set (marker, "glow", hex (accent));
                set (marker, "glowStrength", glow);
            }
        }
        else   // joystick
        {
            // A stick leans as far as its range (the whole sweep, edge to edge: 160 degrees as
            // standard, 80 each way); a ball runs along its cross, a thumbstick's cap slides.
            const float lean = PerformControl::rangeOf (d) * 0.5f;
            const float reach = m * 0.33f * juce::jmin (1.06f, lean / 80.0f);   // the ball: as far as its angle

            // Its position kept in a circle (a corner is as far as an edge, not further).
            float su = u, sv = v;

            if (const float r = std::hypot (su, sv); r > 1.0f)
            {
                su /= r;
                sv /= r;
            }

            // A stick's lean: one angle, toward where it's pushed, as far as the range (turned
            // into the turns about x and z that lean it that way).
            float ax = 0.0f, az = 0.0f;

            if (const float r = std::hypot (su, sv); r > 1.0e-4f)
            {
                const float theta = lean * r * pi / 180.0f;
                const float dx = su / r, dz = -sv / r;
                const float rx = std::asin (juce::jlimit (-1.0f, 1.0f, std::sin (theta) * dz));
                const float rz = std::asin (juce::jlimit (-1.0f, 1.0f, -std::sin (theta) * dx / juce::jmax (1.0e-3f, std::cos (rx))));
                ax = rx * 180.0f / pi;
                az = rz * 180.0f / pi;
            }

            if (style == "ball")
            {
                // A cross on the plate, a ball riding it.
                const float barH = housingH * 0.7f;
                auto across = parts.add ("box", cx, cz, housingH - 0.5f, m * 0.78f, barH, m * 0.11f, accent, "Brushed aluminium");
                set (across, "round", m * 0.03f);
                auto back = parts.add ("box", cx, cz, housingH - 0.5f, m * 0.11f, barH, m * 0.78f, accent, "Brushed aluminium");
                set (back, "round", m * 0.03f);
                const float r = m * 0.14f;
                parts.add ("sphere", cx + su * reach, cz - sv * reach, housingH + barH - r * 0.35f, r * 2.0f, r * 2.0f, r * 2.0f, partColour, material);
            }
            else if (style == "vector")
            {
                for (int arm = 0; arm < 2; ++arm)
                {
                    auto slot = parts.add ("box", cx, cz, housingH * 0.3f, arm == 0 ? m * 0.72f : m * 0.08f, housingH, arm == 0 ? m * 0.08f : m * 0.72f, juce::Colours::black);
                    set (slot, "op", "cut");
                    set (slot, "round", m * 0.035f);
                }

                addStick (parts, cx, cz, housingH * 0.3f, m * 0.34f, m * 0.06f, ax, az, colourOf (d, "stickColour", juce::Colour (0xffc8ccd2)),
                          "Chrome", "sphere", m * 0.13f, m * 0.13f, partColour, material);
            }
            else if (style == "arcade")
            {
                auto washer = parts.add ("cylinder", cx, cz, housingH - 0.5f, m * 0.32f, 1.5f, m * 0.32f, juce::Colour (0xff101012), "Gloss plastic");
                set (washer, "round", 0.6f);
                addStick (parts, cx, cz, housingH, m * 0.30f, m * 0.06f, ax, az, juce::Colour (0xffc8ccd2), "Chrome",
                          "sphere", m * 0.24f, m * 0.24f, partColour, material);
            }
            else if (style == "gimbal")
            {
                auto boot = parts.add ("revolve", cx, cz, housingH - 0.5f, m * 0.40f, m * 0.12f, m * 0.40f, juce::Colour (0xff141416), "Rubber");
                set (boot, "profile", "M0 0 L0.3 0 Q0.55 0.25 1 1 L0 1 Z");
                addStick (parts, cx, cz, housingH, m * 0.36f, m * 0.05f, ax, az, juce::Colour (0xff2a2b2f), "Satin plastic",
                          "cylinder", m * 0.11f, m * 0.12f, partColour, material);
            }
            else   // thumb
            {
                auto ring = parts.add ("cylinder", cx, cz, housingH - 0.5f, m * 0.62f, 1.5f, m * 0.62f, juce::Colour (0xff101012), "Gloss plastic");
                set (ring, "round", 0.6f);
                const float shift = m * 0.14f * juce::jmin (1.6f, lean / 22.0f);
                auto cap = parts.add ("revolve", cx + su * shift, cz - sv * shift, housingH + 1.0f, m * 0.46f, m * 0.12f, m * 0.46f, partColour, material);
                set (cap, "profile", "M0 0.38 Q0.55 0.25 0.82 0 L0.94 0 Q1 0.05 1 0.25 L1 1 L0 1 Z");
                set (cap, "rotate", juce::Array<juce::var> { -sv * lean, 0, -su * lean });
                set (cap, "surface", "rubber");
            }
        }

        auto body = object();
        set (body, "tilt", juce::jlimit (0.0f, 45.0f, numberOf (d, "tilt", 0.0f)));
        set (body, "shadow", 60);
        set (body, "softness", 65);
        set (body, "floor", "#00000000");
        set (body, "parts", parts.list);
        return body;
    }

    //==========================================================================
    // 2D: a pad, a ribbon.
    void paint2d (juce::Graphics& g, const juce::var& d, juce::Rectangle<float> area, float x, float y)
    {
        const auto kind = textOf (d, "kind", "pad");
        const auto housing = colourOf (d, "housingColour", juce::Colour (0xff141518));
        const auto accent = colourOf (d, "accentColour", juce::Colour (0xff5fe0e8));
        const float glow = juce::jlimit (0.0f, 100.0f, numberOf (d, "glow", 60.0f)) * 0.01f;
        const float m = juce::jmin (area.getWidth(), area.getHeight());
        auto r = area.reduced (m * 0.04f);

        g.setColour (housing);
        g.fillRoundedRectangle (r, m * 0.06f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (r.reduced (0.5f), m * 0.06f, 1.0f);

        if (kind == "ribbon")
        {
            const bool vertical = (bool) d.getProperty ("vertical", false);
            const auto strip = vertical ? r.withSizeKeepingCentre (r.getWidth() * 0.5f, r.getHeight() * 0.9f) : r.withSizeKeepingCentre (r.getWidth() * 0.92f, r.getHeight() * 0.5f);
            g.setColour (colourOf (d, "partColour", juce::Colour (0xff26282c)));
            g.fillRoundedRectangle (strip, juce::jmin (strip.getWidth(), strip.getHeight()) * 0.2f);
            const float value = vertical ? y : x;
            const auto at = vertical ? juce::Point<float> (strip.getCentreX(), strip.getBottom() - strip.getHeight() * value)
                                     : juce::Point<float> (strip.getX() + strip.getWidth() * value, strip.getCentreY());
            const auto fill = vertical ? strip.withTop (at.y) : strip.withRight (at.x);
            g.setColour (accent.withAlpha (0.25f + 0.25f * glow));
            g.fillRoundedRectangle (fill, juce::jmin (strip.getWidth(), strip.getHeight()) * 0.2f);
            g.setColour (accent);
            g.fillRect (vertical ? juce::Rectangle<float> (strip.getWidth(), 2.0f).withCentre (at) : juce::Rectangle<float> (2.0f, strip.getHeight()).withCentre (at));
            return;
        }

        // The pad: its grid, its cross-hair, its puck.
        const auto surface = r.reduced (m * 0.05f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));

        for (int i = 1; i < 4; ++i)
        {
            g.drawVerticalLine (juce::roundToInt (surface.getX() + surface.getWidth() * (float) i / 4.0f), surface.getY(), surface.getBottom());
            g.drawHorizontalLine (juce::roundToInt (surface.getY() + surface.getHeight() * (float) i / 4.0f), surface.getX(), surface.getRight());
        }

        const juce::Point<float> puck (surface.getX() + surface.getWidth() * x, surface.getBottom() - surface.getHeight() * y);
        g.setColour (accent.withAlpha (0.35f));
        g.drawVerticalLine (juce::roundToInt (puck.x), surface.getY(), surface.getBottom());
        g.drawHorizontalLine (juce::roundToInt (puck.y), surface.getX(), surface.getRight());

        for (int s = 5; s >= 1; --s)
        {
            g.setColour (accent.withAlpha (glow * 0.08f));
            g.fillEllipse (juce::Rectangle<float> (m * 0.07f * (float) s, m * 0.07f * (float) s).withCentre (puck));
        }

        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (m * 0.1f, m * 0.1f).withCentre (puck));
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.fillEllipse (juce::Rectangle<float> (m * 0.035f, m * 0.035f).withCentre (puck.translated (-m * 0.012f, -m * 0.012f)));
    }

    //==========================================================================
    struct Cache final : juce::DeletedAtShutdown
    {
        std::mutex lock;
        std::map<juce::String, juce::Image> items;
        std::list<juce::String> order;
        std::map<juce::String, juce::Image> latest;   // each look and position's last picture, whatever its size
        std::map<juce::String, juce::Image> latestDesign;   // each look's last picture at a size, wherever it was
        std::map<juce::String, juce::Image> latestSize;     // each size's last picture, whatever its look

        static Cache& get()
        {
            static Cache* const instance = new Cache();
            return *instance;
        }
    };

    juce::Image makeFrame (const juce::var& d, int width, int height, float x, float y, float lightAngle, float lightAltitude)
    {
        juce::Image image (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());   // made on any thread
        const auto kind = textOf (d, "kind", "joystick");

        if (textOf (d, "render", "3d") == "2d" && (kind == "pad" || kind == "ribbon"))
        {
            juce::Graphics g (image);
            paint2d (g, d, { (float) width, (float) height }, x, y);
        }
        else if (const auto body = Body3D::fromVar (bodyFor (d, (float) width, (float) height, x, y), width, height))
        {
            image = Render3D::renderBody (*body, 1.0f, lightAngle, lightAltitude, 0.0f);
        }

        return image;
    }

    void keep (const juce::String& key, const juce::String& look, const juce::String& design, const juce::String& size, const juce::Image& image)
    {
        auto& cache = Cache::get();
        const std::lock_guard<std::mutex> hold (cache.lock);
        cache.items[key] = image;

        if (cache.latest.size() > 200)
            cache.latest.clear();   // a fresh start now and then: looks change as they're designed

        if (cache.latestDesign.size() > 200)
            cache.latestDesign.clear();

        if (cache.latestSize.size() > 200)
            cache.latestSize.clear();

        cache.latest[look] = image;
        cache.latestDesign[design + "|" + size] = image;
        cache.latestSize[size] = image;
        cache.order.push_back (key);

        while (cache.order.size() > 600)
        {
            cache.items.erase (cache.order.front());
            cache.order.pop_front();
        }
    }

    /** A frame: kept, or made now. Painted under a Render3D::ScopedLater, a look not made at
        this size yet (a design being changed, a new size) is made in the background while the
        same look at another size, or the last frame made at this size, stands in; a look
        already made here is being played, its frames made now. */
    juce::Image frameOf (const juce::var& d, int width, int height, float x, float y, float lightAngle, float lightAltitude)
    {
        const auto json = juce::JSON::toString (d, true);
        const auto light = juce::String (lightAngle, 1) + "/" + juce::String (lightAltitude, 1);
        const auto look = json + "|" + juce::String (x, 3) + "," + juce::String (y, 3) + "|" + light;
        const auto size = juce::String (width) + "x" + juce::String (height) + (Render3D::drafting() ? "|draft" : "");
        const auto key = look + "|" + size;
        const auto design = json + "|" + light;
        auto& cache = Cache::get();
        auto* later = Render3D::later();

        {
            const std::lock_guard<std::mutex> hold (cache.lock);

            if (const auto found = cache.items.find (key); found != cache.items.end())
                return found->second;

            // Being resized: the last one stands in, stretched.
            if (Render3D::stretching())
                if (const auto found = cache.latest.find (look); found != cache.latest.end())
                    return found->second;

            if (later != nullptr && cache.latestDesign.count (design + "|" + size) > 0)
                later = nullptr;   // played: made now
        }

        if (later != nullptr)
        {
            Render3D::makeLater (key, size, [json, width, height, x, y, lightAngle, lightAltitude, key, look, design, size]
                                 {
                                     keep (key, look, design, size, makeFrame (juce::JSON::parse (json), width, height, x, y, lightAngle, lightAltitude));
                                 },
                                 *later);

            const std::lock_guard<std::mutex> hold (cache.lock);

            if (const auto found = cache.latest.find (look); found != cache.latest.end())
                return found->second;

            const auto found = cache.latestSize.find (size);
            return found != cache.latestSize.end() ? found->second : juce::Image();
        }

        const auto image = makeFrame (d, width, height, x, y, lightAngle, lightAltitude);
        keep (key, look, design, size, image);
        return image;
    }
}

//==============================================================================
bool PerformControl::has (const GuiWidget& w)
{
    if (w.type != GuiWidget::Type::xy)
        return false;

    const auto found = w.settings.find ("perform");
    return found != w.settings.end() && found->second.isObject();
}

juce::var PerformControl::designOf (const GuiWidget& w)
{
    const auto found = w.settings.find ("perform");
    return found != w.settings.end() ? found->second : juce::var();
}

juce::String PerformControl::modeOf (const GuiWidget& w)
{
    const auto found = w.settings.find ("mode");
    const auto mode = found != w.settings.end() ? found->second.toString().trim().toLowerCase() : juce::String();
    return mode == "bendmod" || mode == "bend" || mode == "pitch" ? juce::String ("bendmod") : juce::String ("free");
}

juce::String PerformControl::bendAxisOf (const GuiWidget& w)
{
    const auto found = w.settings.find ("bendAxis");
    return found != w.settings.end() && found->second.toString().trim().equalsIgnoreCase ("y") ? juce::String ("y") : juce::String ("x");
}

float PerformControl::rangeOf (const juce::var& d)
{
    // Its own, or (an older design) what its travel made it: a lever 60, a thumbstick 44, a
    // ball 160, a stick 160 degrees at full travel.
    if (! d.getProperty ("range", {}).isVoid())
        return juce::jlimit (10.0f, 170.0f, numberOf (d, "range", 160.0f));

    const auto kind = textOf (d, "kind", "joystick"), style = textOf (d, "style", "ball");
    const float travel = juce::jlimit (10.0f, 100.0f, numberOf (d, "travel", 60.0f)) * 0.01f;

    if (kind == "lever")                        return juce::jlimit (10.0f, 170.0f, 60.0f * travel);
    if (kind == "joystick" && style == "thumb") return juce::jlimit (10.0f, 170.0f, 44.0f * travel);
    if (kind == "joystick" && style == "ball")  return juce::jlimit (10.0f, 170.0f, 160.0f * travel);
    return 160.0f;
}

bool PerformControl::leans (const juce::var& d)
{
    const auto kind = textOf (d, "kind", "joystick");
    return kind == "joystick" || kind == "lever";
}

bool PerformControl::springsBoth (const juce::var& d)
{
    return textOf (d, "kind", "joystick") == "lever";
}

juce::String PerformControl::axesOf (const juce::var& d)
{
    const auto kind = textOf (d, "kind", "joystick");

    if (kind == "wheels")
        return "pair";

    if (kind == "wheel")
        return (bool) d.getProperty ("vertical", true) ? "y" : "x";

    if (kind == "ribbon")
        return (bool) d.getProperty ("vertical", false) ? "y" : "x";

    return "both";
}

int PerformControl::partAt (const juce::var& d, juce::Rectangle<float> area, juce::Point<float> at)
{
    return axesOf (d) == "pair" && at.x > area.getCentreX() ? 1 : 0;
}

int PerformControl::framesOf (const juce::var& d)
{
    const bool one = axesOf (d) == "x" || axesOf (d) == "y";
    return juce::jlimit (3, 65, (int) numberOf (d, "frames", one ? 33.0f : 17.0f));
}

void PerformControl::paint (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, float x, float y,
                            float lightAngle, float lightAltitude, float resolution)
{
    if (area.isEmpty() || ! design.isObject())
        return;

    // On its grid of pictures.
    const auto steps = (float) (framesOf (design) - 1);
    x = std::round (juce::jlimit (0.0f, 1.0f, x) * steps) / steps;
    y = std::round (juce::jlimit (0.0f, 1.0f, y) * steps) / steps;

    const auto axes = axesOf (design);

    if (axes == "x") y = 0.5f;
    if (axes == "y") x = 0.5f;

    const float res = juce::jlimit (1.0f, 3.0f, resolution);
    const int w = juce::jmax (8, juce::roundToInt (area.getWidth() * res)), h = juce::jmax (8, juce::roundToInt (area.getHeight() * res));
    const auto image = frameOf (design, w, h, x, y, lightAngle, lightAltitude);

    if (! image.isValid())
        return;   // being made: painted again once it is

    juce::Graphics::ScopedSaveState state (g);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (image, area, juce::RectanglePlacement::stretchToFit);
}

//==============================================================================
namespace
{
    struct Template
    {
        const char* name;
        const char* design;
        const char* settings;
    };

    const Template templates[] {
        { "Pitch and mod wheels", R"({ "kind": "wheels", "housingColour": "#FF18191C", "partColour": "#FF1E1F22", "accentColour": "#FFF2F2EE", "material": "Rubber" })",
          R"({ "mode": "bendmod", "bendAxis": "x" })" },
        { "Pitch and mod wheels (clean)", R"({ "kind": "wheels", "clean": true, "housingColour": "#FF18191C", "partColour": "#FF1E1F22", "accentColour": "#FFF2F2EE", "material": "Rubber" })",
          R"({ "mode": "bendmod", "bendAxis": "x" })" },
        { "Wheel (clean)", R"({ "kind": "wheel", "vertical": true, "clean": true, "housingColour": "#FF141518", "partColour": "#FF2A2B2F", "accentColour": "#FFF2F2EE", "material": "Satin plastic" })",
          R"({ "mode": "free" })" },
        { "Mod wheel, lit", R"({ "kind": "wheel", "vertical": true, "lit": true, "glow": 70, "housingColour": "#FF141518", "partColour": "#FF26282C", "accentColour": "#FF4FA8FF", "material": "Rubber" })",
          R"({ "mode": "free" })" },
        { "Pitch wheel", R"({ "kind": "wheel", "vertical": true, "housingColour": "#FF1A1B1E", "partColour": "#FF2A2B2F", "accentColour": "#FFFFFFFF", "material": "Satin plastic" })",
          R"({ "mode": "bendmod", "bendAxis": "y" })" },
        { "Wheel across", R"({ "kind": "wheel", "vertical": false, "housingColour": "#FF1A1B1E", "partColour": "#FF2A2B2F", "accentColour": "#FFFF8A2A", "material": "Rubber" })",
          R"({ "mode": "free" })" },
        { "Bender lever", R"({ "kind": "lever", "housingColour": "#FF1A1B1E", "partColour": "#FF2C2D31", "material": "Satin plastic", "range": 42 })",
          R"({ "mode": "bendmod", "bendAxis": "x" })" },
        { "Ball on a cross", R"({ "kind": "joystick", "style": "ball", "housingColour": "#FF141518", "partColour": "#FFE8E8EA", "accentColour": "#FFB8BCC4", "material": "Gloss plastic", "travel": 100 })",
          R"({ "mode": "free" })" },
        { "Vector joystick", R"({ "kind": "joystick", "style": "vector", "housingColour": "#FF1B1C20", "partColour": "#FF1A1A1C", "material": "Gloss plastic", "range": 160 })",
          R"({ "mode": "free" })" },
        { "Arcade stick", R"({ "kind": "joystick", "style": "arcade", "housingColour": "#FF1A1B1E", "partColour": "#FFD8322A", "material": "Gloss plastic", "range": 160 })",
          R"({ "mode": "free" })" },
        { "Gimbal stick in a boot", R"({ "kind": "joystick", "style": "gimbal", "housingColour": "#FF1E1F22", "partColour": "#FF3A3C40", "material": "Rubber", "range": 160 })",
          R"({ "mode": "bendmod", "bendAxis": "x" })" },
        { "Thumbstick", R"({ "kind": "joystick", "style": "thumb", "housingColour": "#FF141518", "partColour": "#FF2A2B2F", "material": "Rubber", "travel": 90 })",
          R"({ "mode": "free" })" },
        { "Glowing X-Y pad", R"({ "kind": "pad", "housingColour": "#FF121316", "partColour": "#FF0C1418", "accentColour": "#FF5FE0E8", "glow": 70, "lit": true })",
          R"({ "mode": "free" })" },
        { "Flat X-Y pad", R"({ "kind": "pad", "render": "2d", "housingColour": "#FF16171B", "accentColour": "#FFFF7A3A", "glow": 60 })",
          R"({ "mode": "free" })" },
        { "Ribbon strip", R"({ "kind": "ribbon", "vertical": false, "housingColour": "#FF1A1B1E", "partColour": "#FF2C2E33", "accentColour": "#FF5FE0E8", "glow": 70 })",
          R"({ "mode": "free" })" },
        { "LED touch strip (bends from the middle)", R"({ "kind": "ribbon", "vertical": false, "leds": true, "fromMiddle": true, "housingColour": "#FF141518", "partColour": "#FF1E2024", "accentColour": "#FF4CFF7A", "glow": 80 })",
          R"({ "mode": "bendmod", "bendAxis": "x" })" },
    };

    constexpr int templateCount = (int) (sizeof (templates) / sizeof (templates[0]));
}

juce::StringArray PerformControl::templateNames()
{
    juce::StringArray names;

    for (const auto& t : templates)
        names.add (t.name);

    return names;
}

juce::var PerformControl::makeTemplate (int index)
{
    return juce::JSON::parse (juce::String (templates[juce::jlimit (0, templateCount - 1, index)].design));
}

juce::var PerformControl::templateSettings (int index)
{
    return juce::JSON::parse (juce::String (templates[juce::jlimit (0, templateCount - 1, index)].settings));
}

juce::File PerformControl::galleryFolder()
{
    auto folder = SettingsFolder::folder ("Performance Gallery");
    folder.createDirectory();
    return folder;
}
