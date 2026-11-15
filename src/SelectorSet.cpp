// D:\workspace\Fanan_web_hub\src\SelectorSet.cpp

#include "SelectorSet.h"

#include "Body3D.h"
#include "FontLibrary.h"
#include "IconLibrary.h"   // icons in the keys: {name}
#include "LayerDoc.h"
#include "LayerRender.h"
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

    juce::String colourText (juce::Colour c)    { return "#" + c.toDisplayString (true); }

    //==========================================================================
    // A key's shape in its cell.
    juce::Path shapeIn (juce::Rectangle<float> r, const juce::String& shape, float rounding, int direction)
    {
        juce::Path p;
        const float m = juce::jmin (r.getWidth(), r.getHeight());

        if (shape == "circle")
        {
            p.addEllipse (r.withSizeKeepingCentre (m, m));
        }
        else if (shape == "pill")
        {
            p.addRoundedRectangle (r, m * 0.5f);
        }
        else if (shape == "led")
        {
            const float d = juce::jmin (r.getHeight() * 0.55f, r.getWidth() * 0.4f);
            p.addEllipse (juce::Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.5f + 2.0f, r.getCentreY() }));
        }
        else if (shape == "triangle")
        {
            juce::Path t;

            if (direction == 0)      t.addTriangle (r.getBottomLeft(), r.getBottomRight(), { r.getCentreX(), r.getY() });
            else if (direction == 1) t.addTriangle (r.getTopLeft(), r.getTopRight(), { r.getCentreX(), r.getBottom() });
            else if (direction == 2) t.addTriangle (r.getTopRight(), r.getBottomRight(), { r.getX(), r.getCentreY() });
            else                     t.addTriangle (r.getTopLeft(), r.getBottomLeft(), { r.getRight(), r.getCentreY() });

            p = t.createPathWithRoundedCorners (m * 0.3f * rounding);
        }
        else
        {
            p.addRoundedRectangle (r, m * 0.5f * juce::jlimit (0.0f, 1.0f, rounding));
        }

        return p;
    }

    /** A curved key round a ring: between radii ri and ro, from angle a0 to a1 (0 at the top, clockwise). */
    juce::Path segment (juce::Point<float> c, float ri, float ro, float a0, float a1, float rounding)
    {
        juce::Path p;
        const int steps = juce::jmax (4, (int) std::ceil (std::abs (a1 - a0) / 0.06f));

        auto at = [c] (float r, float a) { return juce::Point<float> (c.x + r * std::sin (a), c.y - r * std::cos (a)); };

        p.startNewSubPath (at (ro, a0));

        for (int i = 1; i <= steps; ++i)
            p.lineTo (at (ro, a0 + (a1 - a0) * (float) i / (float) steps));

        for (int i = steps; i >= 0; --i)
            p.lineTo (at (ri, a0 + (a1 - a0) * (float) i / (float) steps));

        p.closeSubPath();
        return rounding > 0.0f ? p.createPathWithRoundedCorners ((ro - ri) * 0.4f * rounding) : p;
    }

    /** The region a key lights: itself, a little round it (not into its neighbours), its name. */
    juce::Path regionOf (const juce::Path& outline, juce::Rectangle<float> label, float margin, bool withLabel)
    {
        juce::Path region (outline);

        if (margin > 0.5f)
        {
            juce::Path stroke;
            juce::PathStrokeType (margin * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (stroke, outline);
            region.addPath (stroke);
        }

        if (withLabel && ! label.isEmpty())
            region.addRectangle (label);

        region.setUsingNonZeroWinding (true);
        return region;
    }

    //==========================================================================
    // A path as SVG path data in 0..1 of its box (Body3D's extrude profile).
    juce::String unitSvg (const juce::Path& path)
    {
        const auto b = path.getBounds();
        const float sx = 1.0f / juce::jmax (1.0e-3f, b.getWidth()), sy = 1.0f / juce::jmax (1.0e-3f, b.getHeight());
        juce::String svg;
        juce::Path::Iterator it (path);

        auto n = [] (float v) { return juce::String (std::round (v * 10000.0f) / 10000.0f); };
        auto x = [&] (float v) { return n ((v - b.getX()) * sx); };
        auto y = [&] (float v) { return n ((v - b.getY()) * sy); };

        while (it.next())
        {
            using T = juce::Path::Iterator::PathElementType;

            switch (it.elementType)
            {
                case T::startNewSubPath: svg << "M" << x (it.x1) << " " << y (it.y1) << " "; break;
                case T::lineTo:          svg << "L" << x (it.x1) << " " << y (it.y1) << " "; break;
                case T::quadraticTo:     svg << "Q" << x (it.x1) << " " << y (it.y1) << " " << x (it.x2) << " " << y (it.y2) << " "; break;
                case T::cubicTo:         svg << "C" << x (it.x1) << " " << y (it.y1) << " " << x (it.x2) << " " << y (it.y2) << " "
                                             << x (it.x3) << " " << y (it.y3) << " "; break;
                case T::closePath:       svg << "Z "; break;
                default: break;
            }
        }

        return svg.trim();
    }

    juce::var object()    { return juce::var (new juce::DynamicObject()); }

    void put (const juce::var& v, const juce::Identifier& key, const juce::var& value)
    {
        if (auto* o = v.getDynamicObject())
            o->setProperty (key, value);
    }

    //==========================================================================
    // The pictures of a set, off and on, kept.
    struct Pictures
    {
        juce::Image off, on;
        int width = 0, height = 0;            // the pictures' size
        std::vector<SelectorSet::Key> keys;   // in the pictures' pixels
        std::vector<juce::Rectangle<float>> words;   // where each key's name is drawn
    };

    // Kept until the app closes (let go before JUCE checks for leaks).
    struct Cache final : juce::DeletedAtShutdown
    {
        std::mutex lock;
        std::map<juce::String, std::shared_ptr<Pictures>> items;
        std::list<juce::String> order;
        std::map<juce::String, std::shared_ptr<Pictures>> latest;   // each look's last pictures, whatever their size
        std::map<juce::String, std::shared_ptr<Pictures>> latestSlot;   // each size's last pictures, whatever their look
        std::map<juce::String, std::shared_ptr<Pictures>> latestPlace;  // each set of keys' last pictures, whatever their look and size

        static Cache& get()
        {
            static Cache* const instance = new Cache();
            return *instance;
        }
    };

    juce::Colour keyColour (const juce::var& d, int index, bool on)
    {
        const auto base = colourOf (d, "capColour", juce::Colour (0xff26282d));

        if (on)
            return colourOf (d, "capOnColour", base);

        if (const auto* colours = d.getProperty ("keyColours", {}).getArray(); colours != nullptr && ! colours->isEmpty())
        {
            const int group = juce::jmax (1, (int) numberOf (d, "group", 1));
            const auto text = (*colours)[(index / group) % colours->size()].toString().removeCharacters ("#");
            return juce::Colour ((juce::uint32) (text.length() == 6 ? ("FF" + text) : text).getHexValue64());
        }

        return base;
    }

    juce::Font labelFont (const juce::var& d, float height)
    {
        auto font = FontLibrary::font (d.getProperty ("font", {}).toString(), height, (bool) d.getProperty ("bold", true));

        if (const float spacing = numberOf (d, "spacing", 0.0f); std::abs (spacing) > 0.01f)
            font = font.withExtraKerningFactor (juce::jlimit (-0.2f, 1.0f, spacing * 0.01f));

        return font;
    }

    /** A turn that still reads (never upside down). */
    float readable (float angle)
    {
        while (angle > pi * 0.5f + 1.0e-3f)   angle -= pi;
        while (angle <= -pi * 0.5f + 1.0e-3f) angle += pi;
        return angle;
    }

    /** Words in a box, turned about its middle (a quarter turn: up a tall box), as an outline. */
    juce::Path wordsIn (const juce::String& text, const juce::Font& font, juce::Rectangle<float> box, float turn, bool italic)
    {
        const bool across = std::abs (std::sin (turn)) > 0.7f;
        const float w = across ? box.getHeight() : box.getWidth(), h = across ? box.getWidth() : box.getHeight();
        juce::Path words;

        // Icons among them: on one line, fitted.
        if (IconLibrary::hasIcons (text))
        {
            float baseline = 0.0f;
            words = IconText::fitted (text, font, { -w * 0.5f, -h * 0.5f, w, h }, juce::Justification::centred, 0.5f, &baseline);

            if (italic)
                words.applyTransform (juce::AffineTransform::shear (-0.2f, 0.0f).translated (0.2f * baseline, 0.0f));
        }
        else
        {
            juce::GlyphArrangement glyphs;
            glyphs.addFittedText (font, text, -w * 0.5f, -h * 0.5f, w, h, juce::Justification::centred, 2, 0.5f);
            glyphs.createPath (words);

            if (italic && glyphs.getNumGlyphs() > 0)
                words.applyTransform (juce::AffineTransform::shear (-0.2f, 0.0f).translated (0.2f * glyphs.getGlyph (0).getBaselineY(), 0.0f));
        }

        words.applyTransform (juce::AffineTransform::rotation (turn).translated (box.getCentre()));
        return words;
    }

    /** Words along a ring's curve: centred on an angle (0 at the top, clockwise), read left to
        right and upright on its lower half too, as small as they must be to fit their arc. */
    juce::Path wordsOnArc (const juce::String& text, juce::Font font, juce::Point<float> centre, float radius, float middle, float arc, bool italic)
    {
        juce::Path words;

        if (radius < 1.0f)
            return words;

        // Icons among them: the line, upright at its place on the ring (as small as its arc needs).
        if (IconLibrary::hasIcons (text))
        {
            float width = 0.0f;
            auto line = IconText::line (text, font, width);
            const float room = arc * radius * 0.94f;
            const float scale = width > room && width > 0.0f ? room / width : 1.0f;
            const bool lowerHalf = std::cos (middle) < -0.05f;
            const float lift = (font.getAscent() - font.getDescent()) * 0.5f * scale;

            if (italic)
                line.applyTransform (juce::AffineTransform::shear (-0.2f, 0.0f));

            const juce::Point<float> at (centre.x + radius * std::sin (middle), centre.y - radius * std::cos (middle));
            line.applyTransform (juce::AffineTransform::scale (scale).translated (-width * scale * 0.5f, lift)
                                     .rotated (lowerHalf ? middle + pi : middle).translated (at));
            return line;
        }

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, 0.0f, 0.0f);
        float width = glyphs.getBoundingBox (0, -1, true).getWidth();

        if (const float room = arc * radius * 0.94f; width > room && width > 0.0f)
        {
            font = font.withHeight (juce::jmax (4.0f, font.getHeight() * room / width));
            glyphs.clear();
            glyphs.addLineOfText (font, text, 0.0f, 0.0f);
            width = glyphs.getBoundingBox (0, -1, true).getWidth();
        }

        const bool lower = std::cos (middle) < -0.05f;
        const float direction = lower ? -1.0f : 1.0f;
        const float lift = (font.getAscent() - font.getDescent()) * 0.5f;

        for (int i = 0; i < glyphs.getNumGlyphs(); ++i)
        {
            const auto& glyph = glyphs.getGlyph (i);

            if (glyph.isWhitespace())
                continue;

            juce::Path one;
            glyph.createPath (one);
            const float x = (glyph.getLeft() + glyph.getRight()) * 0.5f;

            if (italic)
                one.applyTransform (juce::AffineTransform::shear (-0.2f, 0.0f));

            const float angle = middle + direction * (x - width * 0.5f) / radius;
            const juce::Point<float> at (centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle));
            one.applyTransform (juce::AffineTransform::translation (-x, lift).rotated (lower ? angle + pi : angle).translated (at));
            words.addPath (one);
        }

        return words;
    }

    /** A key's name as an outline, where and how the set puts it. */
    juce::Path wordsFor (const juce::var& d, const SelectorSet::Key& key, const juce::String& rawText, float press)
    {
        const auto text = (bool) d.getProperty ("upper", false) ? rawText.toUpperCase() : rawText;
        const bool caps = textOf (d, "labels", "caps") == "caps";
        const bool follow = textOf (d, "labelAngle", "horizontal") == "follow";
        const bool italic = (bool) d.getProperty ("italic", false);
        const float size = juce::jlimit (5.0f, 200.0f, numberOf (d, "textSize", 34.0f)) * 0.01f;
        const auto shape = textOf (d, "shape", "round");
        const auto area = key.label.translated (0.0f, caps ? press : 0.0f);

        if (area.isEmpty() || text.isEmpty())
            return {};

        const bool lamp = shape == "led";
        const auto keyBox = key.outline.getBounds();
        const bool tall = area.getHeight() > area.getWidth() * 1.25f;
        const float thickness = tall ? area.getWidth() : area.getHeight();
        const bool upward = follow && (tall || (! caps && std::abs (key.edge) > 0.1f));
        const float base = caps && ! lamp && ! key.curved && key.textRadius <= 0.0f
                               ? juce::jmin (thickness, juce::jmin (keyBox.getHeight(), keyBox.getWidth()) * 1.4f) * (upward ? 1.6f : 1.0f)
                               : thickness * (caps ? (key.curved ? 1.6f : 1.0f) : (key.textRadius > 0.0f ? 2.2f : 1.3f));
        const auto font = labelFont (d, juce::jmax (6.0f, base * size));

        if (follow)
        {
            // Along a ring: its curved keys' names (and every name out of a ring) follow the curve.
            if (key.textRadius > 0.0f && (key.curved || ! caps))
                return wordsOnArc (text, font, key.ringCentre, key.textRadius, key.angle, key.textArc, italic);

            // A turned key's name turns with it; a tall key's (or a column's edge) runs up it.
            if (key.textRadius > 0.0f)
                return wordsIn (text, font, area, readable (key.angle), italic);

            if (tall || (! caps && std::abs (key.edge) > 0.1f))
                return wordsIn (text, font, area, -pi * 0.5f, italic);
        }

        return wordsIn (text, font, area, 0.0f, italic);
    }

    /** Their names, in the keys or out of them, wearing their text effects. */
    void paintLabels (juce::Graphics& g, const juce::var& d, const std::vector<SelectorSet::Key>& keys, const juce::StringArray& names,
                      bool on, float press, juce::Rectangle<int> size, float lightAngle, float lightAltitude)
    {
        if (textOf (d, "labels", "caps") == "none")
            return;

        const auto colour = on ? colourOf (d, "labelOnColour", juce::Colours::white) : colourOf (d, "labelColour", juce::Colour (0xffd8dadf));
        juce::Path all;

        for (size_t i = 0; i < keys.size(); ++i)
            all.addPath (wordsFor (d, keys[i], (int) i < names.size() ? names[(int) i] : juce::String ((int) i + 1), press));

        if (all.isEmpty())
            return;

        // A lit key's name glows.
        if (on && (bool) d.getProperty ("labelGlow", true))
        {
            g.setColour (colourOf (d, "glowColour", colour).withAlpha (0.3f));
            g.strokePath (all, juce::PathStrokeType (juce::jmax (1.0f, all.getBounds().getHeight() * 0.01f) + 1.5f));
        }

        // Photoshop's layer style on them (a stroke, a bevel, shadows, an overlay...).
        if (const auto effects = d.getProperty ("textEffects", {}); effects.isObject())
        {
            const auto style = LayerStyle::fromVar (effects);

            if (style.any())
            {
                juce::Image layer (juce::Image::ARGB, juce::jmax (1, size.getWidth()), juce::jmax (1, size.getHeight()), true);

                {
                    juce::Graphics lg (layer);
                    lg.setColour (colour);
                    lg.fillPath (all);
                }

                LayerRender::applyLayerStyle (layer, style, 1.0f, lightAngle, lightAltitude);
                g.drawImageAt (layer, 0, 0);
                return;
            }
        }

        g.setColour (colour);
        g.fillPath (all);
    }

    /** A plate the keys sit in: round the ring, or a rounded box round them all. */
    juce::Path plateOf (const juce::var& d, const std::vector<SelectorSet::Key>& keys, juce::Rectangle<float> area)
    {
        juce::Path plate;
        juce::Rectangle<float> all;

        for (const auto& k : keys)
            all = all.isEmpty() ? k.outline.getBounds() : all.getUnion (k.outline.getBounds());

        const auto formation = textOf (d, "formation", "row");
        const float pad = juce::jmin (area.getWidth(), area.getHeight()) * 0.04f + 2.0f;

        if (formation == "ring" || formation == "arc")
        {
            const auto c = area.getCentre();
            float r = 0;

            for (const auto& k : keys)
            {
                const auto b = k.outline.getBounds();

                for (const auto& corner : { b.getTopLeft(), b.getTopRight(), b.getBottomLeft(), b.getBottomRight() })
                    r = juce::jmax (r, corner.getDistanceFrom (c));
            }

            if (formation == "ring")
            {
                plate.addEllipse (juce::Rectangle<float> ((r + pad) * 2.0f, (r + pad) * 2.0f).withCentre (c));
                return plate;
            }
        }

        plate.addRoundedRectangle (all.expanded (pad), pad * 1.6f);
        return plate;
    }

    //==========================================================================
    // 2D: soft (a gradient and a bevel), flat, glossy.
    void paint2d (juce::Graphics& g, const juce::var& d, const std::vector<SelectorSet::Key>& keys, juce::Rectangle<float> area, bool on)
    {
        const auto style = textOf (d, "style", "soft");
        const auto glowColour = colourOf (d, "glowColour", juce::Colour (0xff5fe0e8));
        const float glow = juce::jlimit (0.0f, 100.0f, numberOf (d, "glow", 60.0f)) * 0.01f;
        const bool edge = (bool) d.getProperty ("edge", false);
        const float edgeOff = juce::jlimit (0.0f, 100.0f, numberOf (d, "edgeOff", 25.0f)) * 0.01f;
        const auto led = textOf (d, "led", "none");
        const auto ledColour = colourOf (d, "ledColour", juce::Colour (0xffff3b30));

        if ((bool) d.getProperty ("plate", false))
        {
            const auto plate = plateOf (d, keys, area);
            const auto pc = colourOf (d, "plateColour", juce::Colour (0xff141518));
            g.setGradientFill (juce::ColourGradient (pc.brighter (0.12f), 0, plate.getBounds().getY(), pc.darker (0.2f), 0, plate.getBounds().getBottom(), false));
            g.fillPath (plate);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.strokePath (plate, juce::PathStrokeType (1.0f));
        }

        for (size_t i = 0; i < keys.size(); ++i)
        {
            const auto& k = keys[i];
            const auto b = k.outline.getBounds();
            const float m = juce::jmin (b.getWidth(), b.getHeight());
            const auto colour = keyColour (d, (int) i, on);

            // Its light round it.
            if (on && glow > 0.0f)
            {
                for (int s = 6; s >= 1; --s)
                {
                    g.setColour (glowColour.withAlpha (glow * 0.09f));
                    g.strokePath (k.outline, juce::PathStrokeType ((float) s * juce::jmax (1.2f, m * 0.035f)));
                }
            }

            if (style != "flat" && ! on)
                juce::DropShadow (juce::Colours::black.withAlpha (0.45f), juce::roundToInt (juce::jmax (2.0f, m * 0.08f)), { 0, juce::roundToInt (juce::jmax (1.0f, m * 0.03f)) })
                    .drawForPath (g, k.outline);

            if (style == "flat")
            {
                g.setColour (colour);
                g.fillPath (k.outline);
            }
            else
            {
                g.setGradientFill (juce::ColourGradient (colour.brighter (on ? 0.25f : 0.18f), 0, b.getY(), colour.darker (on ? 0.1f : 0.3f), 0, b.getBottom(), false));
                g.fillPath (k.outline);

                // Its top edge catching the light, its foot in shadow.
                juce::Graphics::ScopedSaveState state (g);
                g.reduceClipRegion (k.outline);
                g.setColour (juce::Colours::white.withAlpha (on ? 0.18f : 0.12f));
                g.strokePath (k.outline, juce::PathStrokeType (juce::jmax (1.0f, m * 0.05f)), juce::AffineTransform::translation (0.0f, juce::jmax (0.8f, m * 0.025f)));

                if (style == "glossy")
                {
                    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), 0, b.getY(), juce::Colours::white.withAlpha (0.0f), 0, b.getCentreY(), false));
                    g.fillRect (b.withHeight (b.getHeight() * 0.5f));
                }
            }

            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.strokePath (k.outline, juce::PathStrokeType (1.0f));

            if (edge)
            {
                g.setColour (glowColour.withAlpha (on ? 1.0f : edgeOff));
                g.strokePath (k.outline, juce::PathStrokeType (juce::jmax (1.2f, m * (on ? 0.045f : 0.03f))));
            }

            // Its lamp.
            if (led != "none" && textOf (d, "shape", "round") != "led")
            {
                const auto lamp = led == "bar" ? juce::Rectangle<float> (b.getWidth() * 0.4f, juce::jmax (2.0f, m * 0.07f)).withCentre ({ b.getCentreX(), b.getY() + m * 0.16f })
                                               : juce::Rectangle<float> (m * 0.14f, m * 0.14f).withCentre ({ b.getCentreX(), b.getY() + m * 0.17f });

                if (on)
                {
                    g.setColour (ledColour.withAlpha (0.35f));
                    g.fillRoundedRectangle (lamp.expanded (m * 0.06f), m * 0.08f);
                }

                g.setColour (on ? ledColour.brighter (0.3f) : ledColour.darker (0.75f));
                g.fillRoundedRectangle (lamp, lamp.getHeight() * 0.5f);
            }
        }
    }

    //==========================================================================
    // 3D: the keys as a Body3D (each its outline pulled up, rounded, its rim and lamp), lit by
    // the panel's light; their names painted over it.
    juce::Image render3d (const juce::var& d, const std::vector<SelectorSet::Key>& keys, juce::Rectangle<float> area, bool on,
                          float lightAngle, float lightAltitude)
    {
        const int W = juce::jmax (8, juce::roundToInt (area.getWidth())), H = juce::jmax (8, juce::roundToInt (area.getHeight()));
        auto body = object();
        put (body, "tilt", 0);
        put (body, "shadow", 55);
        put (body, "softness", 70);
        put (body, "floor", "#00000000");

        juce::Array<juce::var> parts;
        float smallest = 1.0e9f;

        for (const auto& k : keys)
            smallest = juce::jmin (smallest, juce::jmin (k.outline.getBounds().getWidth(), k.outline.getBounds().getHeight()));

        const float keyH = juce::jmax (2.0f, smallest * juce::jlimit (3.0f, 80.0f, numberOf (d, "height", 18.0f)) * 0.01f);
        const float travel = on ? keyH * juce::jlimit (0.0f, 90.0f, numberOf (d, "travel", 30.0f)) * 0.01f : 0.0f;
        const bool plate = (bool) d.getProperty ("plate", false);
        const float plateH = plate ? juce::jmax (1.5f, smallest * 0.06f) : 0.0f;
        const float rounding = juce::jlimit (0.0f, 100.0f, numberOf (d, "rounding", 25.0f)) * 0.01f;
        const auto glowColour = colourOf (d, "glowColour", juce::Colour (0xff5fe0e8));
        const float glow = juce::jlimit (0.0f, 100.0f, numberOf (d, "glow", 60.0f));
        const bool edge = (bool) d.getProperty ("edge", false);
        const auto material = d.getProperty ("material", "Satin plastic").toString();
        const auto led = textOf (d, "led", "none");

        auto extrude = [&] (const juce::String& name, const juce::Path& outline, float y, float h, float round)
        {
            const auto b = outline.getBounds();
            auto p = object();
            put (p, "name", name);
            put (p, "kind", "extrude");
            put (p, "plane", "top");
            put (p, "profile", unitSvg (outline));
            put (p, "x", b.getCentreX());
            put (p, "z", b.getCentreY());
            put (p, "w", juce::jmax (0.6f, b.getWidth()));
            put (p, "d", juce::jmax (0.6f, b.getHeight()));
            put (p, "y", y);
            put (p, "h", juce::jmax (0.6f, h));
            put (p, "round", round);
            return p;
        };

        if (plate)
        {
            auto p = extrude ("Plate", plateOf (d, keys, area), 0.0f, plateH, juce::jmin (plateH * 0.4f, 2.0f));
            put (p, "material", "Satin plastic");
            put (p, "color", colourText (colourOf (d, "plateColour", juce::Colour (0xff141518))));
            parts.add (p);
        }

        for (size_t i = 0; i < keys.size(); ++i)
        {
            const auto& k = keys[i];
            const auto b = k.outline.getBounds();
            const float m = juce::jmin (b.getWidth(), b.getHeight());

            // Its rim: the light round its foot.
            if (edge)
            {
                juce::Path rim;
                juce::PathStrokeType (juce::jmax (1.5f, m * 0.07f)).createStrokedPath (rim, k.outline);
                rim.addPath (k.outline);
                rim.setUsingNonZeroWinding (true);
                auto p = extrude ("Rim", rim, plateH - 0.5f, keyH * 0.45f + 0.5f, 0.5f);
                put (p, "material", "Gloss plastic");
                put (p, "color", colourText (on ? glowColour : glowColour.darker (1.2f)));
                put (p, "glow", colourText (glowColour));
                put (p, "glowStrength", on ? glow : glow * juce::jlimit (0.0f, 1.0f, numberOf (d, "edgeOff", 25.0f) * 0.01f));
                parts.add (p);
            }

            // The key, pressed in when it's on.
            auto cap = extrude ("Key", k.outline, plateH - travel, keyH, juce::jmin (keyH * 0.45f, m * 0.5f * juce::jmax (0.15f, rounding)));
            put (cap, "material", material);
            put (cap, "color", colourText (keyColour (d, (int) i, on)));

            if (on && (bool) d.getProperty ("capGlow", false))
            {
                put (cap, "glow", colourText (glowColour));
                put (cap, "glowStrength", glow * 0.35f);
            }

            parts.add (cap);

            // Its lamp.
            if (led != "none")
            {
                const auto ledColour = colourOf (d, "ledColour", juce::Colour (0xffff3b30));
                auto lamp = object();
                const bool bar = led == "bar";
                put (lamp, "name", "Lamp");
                put (lamp, "kind", bar ? "box" : "sphere");
                put (lamp, "x", b.getCentreX());
                put (lamp, "z", b.getY() + m * 0.17f);
                put (lamp, "y", plateH - travel + keyH - 0.6f);
                put (lamp, "w", bar ? b.getWidth() * 0.4f : m * 0.14f);
                put (lamp, "d", bar ? juce::jmax (1.5f, m * 0.07f) : m * 0.14f);
                put (lamp, "h", juce::jmax (1.0f, m * 0.05f));
                put (lamp, "round", bar ? 0.6f : 0.0f);
                put (lamp, "glass", 30);
                put (lamp, "color", colourText (on ? ledColour : ledColour.darker (0.9f)));

                if (on)
                {
                    put (lamp, "glow", colourText (ledColour));
                    put (lamp, "glowStrength", 85);
                }

                parts.add (lamp);
            }
        }

        put (body, "parts", parts);

        juce::Image image (juce::Image::ARGB, W, H, true, juce::SoftwareImageType());

        if (const auto shape = Body3D::fromVar (body, W, H))
            image = Render3D::renderBody (*shape, 1.0f, lightAngle, lightAltitude, 0.0f);

        return image;
    }

    /** A set's pictures, made now. */
    std::shared_ptr<Pictures> makePictures (const juce::var& design, int width, int height, int count, const juce::StringArray& names,
                                            float lightAngle, float lightAltitude)
    {
        auto pictures = std::make_shared<Pictures>();
        pictures->width = width;
        pictures->height = height;
        const auto area = juce::Rectangle<float> ((float) width, (float) height);
        pictures->keys = SelectorSet::layout (design, area, count);

        for (size_t i = 0; i < pictures->keys.size(); ++i)
            pictures->words.push_back (wordsFor (design, pictures->keys[i], (int) i < names.size() ? names[(int) i] : juce::String ((int) i + 1), 0.0f)
                                           .getBounds().expanded (2.0f));
        const bool threeD = textOf (design, "render", "3d") == "3d";

        for (int state = 0; state < 2; ++state)
        {
            const bool on = state == 1;
            juce::Image image;

            if (threeD)
            {
                image = render3d (design, pictures->keys, area, on, lightAngle, lightAltitude);
            }
            else
            {
                image = juce::Image (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());   // made on any thread
                juce::Graphics g (image);
                paint2d (g, design, pictures->keys, area, on);
            }

            {
                juce::Graphics g (image);
                const float press = threeD && on ? juce::jmax (0.5f, juce::jmin (area.getWidth(), area.getHeight()) * 0.01f) : 0.0f;
                paintLabels (g, design, pictures->keys, names, on, press, { width, height }, lightAngle, lightAltitude);
            }

            (on ? pictures->on : pictures->off) = image;
        }

        return pictures;
    }

    void keep (const juce::String& key, const juce::String& look, const juce::String& slot, const juce::String& keys,
               std::shared_ptr<Pictures> pictures)
    {
        auto& cache = Cache::get();
        const std::lock_guard<std::mutex> hold (cache.lock);
        cache.items[key] = pictures;

        if (cache.latest.size() > 200)
            cache.latest.clear();   // a fresh start now and then: looks change as they're designed

        if (cache.latestSlot.size() > 200)
            cache.latestSlot.clear();

        if (cache.latestPlace.size() > 200)
            cache.latestPlace.clear();

        cache.latest[look] = pictures;
        cache.latestSlot[slot] = pictures;
        cache.latestPlace[keys] = pictures;
        cache.order.push_back (key);

        while (cache.order.size() > 160)
        {
            cache.items.erase (cache.order.front());
            cache.order.pop_front();
        }
    }

    /** A set's pictures: kept, or made now, or (painted under a Render3D::ScopedLater) made in
        the background while those of the same look at another size, or the last ones made for
        this place, stand in (none: nullptr). */
    std::shared_ptr<Pictures> picturesOf (const juce::var& design, int width, int height, int count, const juce::StringArray& names,
                                          float lightAngle, float lightAltitude)
    {
        const auto json = juce::JSON::toString (design, true);
        const auto rest = juce::String (count) + "|" + names.joinIntoString ("\x1f") + "|"
                        + juce::String (lightAngle, 1) + "/" + juce::String (lightAltitude, 1);
        const auto look = json + "|" + rest;
        const auto size = juce::String (width) + "x" + juce::String (height) + (Render3D::drafting() ? "|draft" : "");
        const auto key = look + "|" + size;
        const auto slot = rest + "|" + size;   // the place: its size and keys, whatever their look

        auto& cache = Cache::get();
        auto* later = Render3D::later();

        {
            const std::lock_guard<std::mutex> hold (cache.lock);

            if (const auto found = cache.items.find (key); found != cache.items.end())
                return found->second;

            // Being resized: the last ones of this look stand in, stretched.
            if (Render3D::stretching())
                if (const auto found = cache.latest.find (look); found != cache.latest.end())
                    return found->second;
        }

        if (later != nullptr)
        {
            Render3D::makeLater (key, slot, [json, width, height, count, names, lightAngle, lightAltitude, key, look, slot, rest]
                                 {
                                     keep (key, look, slot, rest, makePictures (juce::JSON::parse (json), width, height, count, names, lightAngle, lightAltitude));
                                 },
                                 *later);

            // Standing in (stretched) till they're made: the last ones of this look at another
            // size, the last ones made for this place, or of its keys at any size.
            const std::lock_guard<std::mutex> hold (cache.lock);

            if (const auto found = cache.latest.find (look); found != cache.latest.end())
                return found->second;

            if (const auto found = cache.latestSlot.find (slot); found != cache.latestSlot.end())
                return found->second;

            const auto found = cache.latestPlace.find (rest);
            return found != cache.latestPlace.end() ? found->second : nullptr;
        }

        auto pictures = makePictures (design, width, height, count, names, lightAngle, lightAltitude);
        keep (key, look, slot, rest, pictures);
        return pictures;
    }
}

//==============================================================================
bool SelectorSet::has (const GuiWidget& w)
{
    if (w.type != GuiWidget::Type::selector)
        return false;

    const auto found = w.settings.find ("set");
    return found != w.settings.end() && found->second.isObject();
}

juce::var SelectorSet::designOf (const GuiWidget& w)
{
    const auto found = w.settings.find ("set");
    return found != w.settings.end() ? found->second : juce::var();
}

juce::String SelectorSet::modeOf (const GuiWidget& w)
{
    const auto found = w.settings.find ("mode");
    const auto mode = found != w.settings.end() ? found->second.toString().trim().toLowerCase() : juce::String();
    return mode == "toggles" || mode == "toggle" || mode == "each" ? juce::String ("toggles")
         : mode == "multi" || mode == "several" || mode == "multiple" ? juce::String ("multi")
                                                                      : juce::String ("single");
}

juce::String SelectorSet::keyParam (const GuiWidget& w, int key)
{
    if (const auto found = w.roles.find ("key" + juce::String (key + 1)); found != w.roles.end() && found->second.isNotEmpty())
        return found->second;

    return w.param + juce::String (key + 1);
}

//==============================================================================
std::vector<SelectorSet::Key> SelectorSet::layout (const juce::var& d, juce::Rectangle<float> area, int count)
{
    std::vector<Key> keys;
    const int k = juce::jlimit (1, 256, count);
    auto formation = textOf (d, "formation", "row");
    auto shape = textOf (d, "shape", formation == "ring" || formation == "arc" ? "segment" : "round");
    const bool vertical = (bool) d.getProperty ("vertical", false);
    const float gapShare = juce::jlimit (0.0f, 200.0f, numberOf (d, "gap", 8.0f)) * 0.01f;
    const float rounding = juce::jlimit (0.0f, 100.0f, numberOf (d, "rounding", shape == "rect" ? 12.0f : 30.0f)) * 0.01f;
    const auto labels = textOf (d, "labels", "caps");
    const bool beside = labels == "beside";
    const auto side = textOf (d, "labelSide", "auto");
    const auto r = area.reduced (juce::jmin (area.getWidth(), area.getHeight()) * 0.04f + 1.0f);

    if (formation == "cross" && k != 4 && k != 5) formation = "row";
    if (formation == "rocker" && k != 2)         formation = "segmented";

    auto add = [&] (juce::Path outline, juce::Rectangle<float> label, float angle = 0.0f, bool flipped = false)
    {
        Key key;
        key.outline = outline;
        key.label = label;
        key.angle = angle;
        key.flipped = flipped;
        keys.push_back (key);
    };

    // A key's name on its cap (beside a led's lamp).
    auto capLabel = [&] (juce::Rectangle<float> cell)
    {
        if (shape == "led")
        {
            const float lamp = juce::jmin (cell.getHeight() * 0.55f, cell.getWidth() * 0.4f) + 6.0f;
            return cell.withTrimmedLeft (lamp);
        }

        if (shape == "circle")
        {
            const float m = juce::jmin (cell.getWidth(), cell.getHeight());
            return cell.withSizeKeepingCentre (m * 0.8f, m * 0.6f);
        }

        return cell.reduced (cell.getWidth() * 0.06f, cell.getHeight() * 0.08f);
    };

    if (formation == "ring" || formation == "arc")
    {
        float from = numberOf (d, "arcFrom", formation == "ring" ? 0.0f : 100.0f) * pi / 180.0f;
        float to = numberOf (d, "arcTo", formation == "ring" ? 360.0f : 260.0f) * pi / 180.0f;

        if (to <= from)
            to += 2.0f * pi;

        const float inner = juce::jlimit (5.0f, 95.0f, numberOf (d, "inner", 60.0f)) * 0.01f;
        const bool within = beside && side == "inside";              // its names on the inner side
        const float band = beside && ! within ? 0.32f : 0.0f;        // ...or outside, beyond the keys
        const bool full = std::abs (to - from - 2.0f * pi) < 0.01f;
        const float span = (to - from) / (float) k;

        // Fitted: the box the keys (and their names) cover at radius 1.
        float x0 = 1.0e9f, x1 = -1.0e9f, y0 = 1.0e9f, y1 = -1.0e9f;

        for (int i = 0; i <= 96; ++i)
        {
            const float a = from + (to - from) * (float) i / 96.0f;

            for (const float rad : { inner, 1.0f + band })
            {
                x0 = juce::jmin (x0, rad * std::sin (a)); x1 = juce::jmax (x1, rad * std::sin (a));
                y0 = juce::jmin (y0, -rad * std::cos (a)); y1 = juce::jmax (y1, -rad * std::cos (a));
            }
        }

        auto unit = full ? juce::Rectangle<float> (-1.0f - band, -1.0f - band, 2.0f + 2.0f * band, 2.0f + 2.0f * band)
                         : juce::Rectangle<float> (x0, y0, x1 - x0, y1 - y0);

        const float R = juce::jmin (r.getWidth() / juce::jmax (0.01f, unit.getWidth()), r.getHeight() / juce::jmax (0.01f, unit.getHeight()));
        const juce::Point<float> c (r.getCentreX() - unit.getCentreX() * R, r.getCentreY() - unit.getCentreY() * R);
        const float ro = R, ri = R * inner, mid = (ro + ri) * 0.5f;

        // An arc round the bottom reads left to right too: its keys from its far end.
        const bool backwards = ! full && std::cos ((from + to) * 0.5f) < -0.3f;

        for (int n = 0; n < k; ++n)
        {
            const int i = backwards ? k - 1 - n : n;
            const float a0 = from + span * (float) i, a1 = a0 + span, am = (a0 + a1) * 0.5f;
            const juce::Point<float> centre (c.x + mid * std::sin (am), c.y - mid * std::cos (am));
            const juce::Point<float> outside (c.x + (ro + R * band * 0.5f) * std::sin (am), c.y - (ro + R * band * 0.5f) * std::cos (am));
            const float chord = 2.0f * mid * std::sin (juce::jmin (span, pi) * 0.5f);
            juce::Path outline;
            juce::Rectangle<float> label;

            if (shape == "segment")
            {
                const float gapAngle = (ro - ri) * gapShare / juce::jmax (1.0f, mid);
                outline = segment (c, ri, ro, a0 + gapAngle * 0.5f, a1 - gapAngle * 0.5f, rounding);
                label = juce::Rectangle<float> (chord * 0.86f, (ro - ri) * 0.62f).withCentre (centre);
            }
            else
            {
                const float size = juce::jmin (ro - ri, chord) * (1.0f - juce::jmin (0.6f, gapShare * 0.5f));
                const auto cell = juce::Rectangle<float> (size, size).withCentre (centre);
                outline = shapeIn (cell, shape, rounding, 0);

                if (shape != "circle")
                    outline.applyTransform (juce::AffineTransform::rotation (am, centre.x, centre.y));

                label = capLabel (cell);
            }

            float textRadius = mid, textArc = span * 0.9f;

            if (within)
            {
                const float r2 = ri - R * 0.12f;
                const juce::Point<float> in (c.x + r2 * std::sin (am), c.y - r2 * std::cos (am));
                label = juce::Rectangle<float> (2.0f * r2 * std::sin (juce::jmin (span, pi) * 0.5f), R * 0.16f).withCentre (in);
                textRadius = r2;
                textArc = span;
            }
            else if (beside)
            {
                label = juce::Rectangle<float> (juce::jmax (chord, R * 0.5f), R * band * 0.55f).withCentre (outside);
                textRadius = ro + R * band * 0.5f;
                textArc = span;
            }

            add (outline, label, am);
            keys.back().curved = shape == "segment";
            keys.back().ringCentre = c;
            keys.back().textRadius = textRadius;
            keys.back().textArc = textArc;
        }

        return keys;
    }

    if (formation == "cross")
    {
        const float S = juce::jmin (r.getWidth(), r.getHeight()) * (beside ? 0.64f : 1.0f);
        const auto sq = juce::Rectangle<float> (S, S).withCentre (r.getCentre());
        const float a = S / 3.0f, g = a * gapShare * 0.5f;
        const auto o = sq.getTopLeft();
        const bool centreKey = k == 5;
        const float tip = centreKey ? a : a * 1.5f;

        auto arm = [&] (std::initializer_list<juce::Point<float>> pts)
        {
            juce::Path p;
            bool first = true;

            for (auto pt : pts)
            {
                if (first) p.startNewSubPath (o + pt);
                else       p.lineTo (o + pt);
                first = false;
            }

            p.closeSubPath();
            return p.createPathWithRoundedCorners (a * 0.25f * rounding);
        };

        // Up, right, down, left (and the middle).
        add (arm ({ { a + g, g }, { 2 * a - g, g }, { 2 * a - g, a - g }, { 1.5f * a, tip - g }, { a + g, a - g } }), sq.withTrimmedBottom (2 * a).reduced (a * 1.05f, a * 0.2f));
        add (arm ({ { 3 * a - g, a + g }, { 3 * a - g, 2 * a - g }, { 2 * a + g, 2 * a - g }, { 3 * a - tip + g, 1.5f * a }, { 2 * a + g, a + g } }),
             sq.withTrimmedLeft (2 * a).reduced (a * 0.2f, a * 1.05f));
        add (arm ({ { a + g, 3 * a - g }, { 2 * a - g, 3 * a - g }, { 2 * a - g, 2 * a + g }, { 1.5f * a, 3 * a - tip + g }, { a + g, 2 * a + g } }),
             sq.withTrimmedTop (2 * a).reduced (a * 1.05f, a * 0.2f));
        add (arm ({ { g, a + g }, { g, 2 * a - g }, { a - g, 2 * a - g }, { tip - g, 1.5f * a }, { a - g, a + g } }), sq.withTrimmedRight (2 * a).reduced (a * 0.2f, a * 1.05f));

        if (centreKey)
        {
            juce::Path middle;
            middle.addEllipse (juce::Rectangle<float> (a * 0.86f, a * 0.86f).withCentre (sq.getCentre()));
            add (middle, juce::Rectangle<float> (a * 0.8f, a * 0.5f).withCentre (sq.getCentre()));
        }

        // Their names past the arms' ends.
        if (beside)
        {
            const float room = (juce::jmin (r.getWidth(), r.getHeight()) - S) * 0.5f;
            keys[0].label = { sq.getCentreX() - a * 1.5f, sq.getY() - room, a * 3.0f, room };
            keys[1].label = { sq.getRight(), sq.getCentreY() - a * 1.5f, room, a * 3.0f };
            keys[2].label = { sq.getCentreX() - a * 1.5f, sq.getBottom(), a * 3.0f, room };
            keys[3].label = { sq.getX() - room, sq.getCentreY() - a * 1.5f, room, a * 3.0f };

            keys[1].edge = keys[3].edge = -pi * 0.5f;

            if (centreKey)
                keys[4].label = {};
        }

        return keys;
    }

    if (formation == "zipper")
    {
        // Triangles, tip up and tip down in turn (tip left and right, up and down); their
        // names out of them in a band of their own.
        auto room = r;
        juce::Rectangle<float> band;

        juce::Rectangle<float> band2;   // ...a tip-up one's under its base, a tip-down one's over it

        if (beside)
        {
            if (vertical) { band = room.removeFromRight (room.getWidth() * 0.26f); band2 = room.removeFromLeft (room.getWidth() * 0.35f); }
            else          { band = room.removeFromBottom (room.getHeight() * 0.22f); band2 = room.removeFromTop (room.getHeight() * 0.28f); }
        }

        const float along = vertical ? room.getHeight() : room.getWidth(), across = vertical ? room.getWidth() : room.getHeight();
        const float base0 = 2.0f * along / (float) (k + 1);
        const float g = base0 * gapShare * 0.5f;
        const float base = 2.0f * (along - g * (float) (k - 1)) / (float) (k + 1);
        const float step = base * 0.5f + g;

        for (int i = 0; i < k; ++i)
        {
            const bool flip = (i % 2) == 1;
            const auto cell = vertical ? juce::Rectangle<float> (room.getX(), room.getY() + step * (float) i, across, base)
                                       : juce::Rectangle<float> (room.getX() + step * (float) i, room.getY(), base, across);
            const int direction = vertical ? (flip ? 3 : 2) : (flip ? 1 : 0);
            auto outline = shapeIn (cell, "triangle", rounding, direction);

            // Its name in the wide part.
            auto label = vertical ? (flip ? cell.withTrimmedRight (cell.getWidth() * 0.45f) : cell.withTrimmedLeft (cell.getWidth() * 0.45f))
                                  : (flip ? cell.withTrimmedBottom (cell.getHeight() * 0.48f) : cell.withTrimmedTop (cell.getHeight() * 0.48f));
            label = vertical ? label.reduced (0.0f, cell.getHeight() * 0.22f) : label.reduced (cell.getWidth() * 0.22f, 0.0f);

            if (beside)
            {
                const auto& names = flip ? band2 : band;
                label = vertical ? juce::Rectangle<float> (names.getX(), cell.getY(), names.getWidth(), base)
                                 : juce::Rectangle<float> (cell.getX(), names.getY(), base, names.getHeight());
            }

            add (outline, label, 0.0f, flip);
        }

        return keys;
    }

    if (formation == "grid")
    {
        const int columns = juce::jlimit (1, k, (int) numberOf (d, "columns", (float) juce::roundToInt (std::ceil (std::sqrt ((float) k)))));
        const int rows = (k + columns - 1) / columns;
        const float cw0 = r.getWidth() / (float) columns, g = cw0 * gapShare;
        const float cw = (r.getWidth() - g * (float) (columns - 1)) / (float) columns;
        const float ch = (r.getHeight() - g * (float) (rows - 1)) / (float) rows;

        for (int i = 0; i < k; ++i)
        {
            const auto cell = juce::Rectangle<float> (r.getX() + (cw + g) * (float) (i % columns), r.getY() + (ch + g) * (float) (i / columns), cw, ch);
            const bool above = side == "above";
            const auto keyCell = beside ? (above ? cell.withTrimmedTop (ch * 0.3f) : cell.withTrimmedBottom (ch * 0.3f)) : cell;
            const auto nameCell = above ? cell.withBottom (keyCell.getY()) : cell.withTop (keyCell.getBottom());
            add (shapeIn (keyCell, shape, rounding, 0), beside ? nameCell : capLabel (keyCell));
        }

        return keys;
    }

    // In a line: a row, a column, a joined strip, a rocker.
    const bool down = formation == "column" || ((formation == "segmented" || formation == "rocker") && vertical);
    const bool joined = formation == "segmented" || formation == "rocker";
    auto line = r;
    juce::Rectangle<float> names;

    const bool lampNames = shape == "led" && ! joined;   // a lamp's name is beside it already

    if (beside && ! lampNames)
    {
        if (down) names = side == "left" ? line.removeFromLeft (line.getWidth() * 0.55f) : line.removeFromRight (line.getWidth() * 0.55f);
        else      names = side == "above" ? line.removeFromTop (line.getHeight() * 0.3f) : line.removeFromBottom (line.getHeight() * 0.3f);
    }

    const float along = down ? line.getHeight() : line.getWidth();
    const float g = joined ? juce::jmax (1.0f, along / (float) k * gapShare * 0.15f) : along / (float) k * gapShare;
    const float cell = (along - g * (float) (k - 1)) / (float) k;

    for (int i = 0; i < k; ++i)
    {
        auto c = down ? juce::Rectangle<float> (line.getX(), line.getY() + (cell + g) * (float) i, line.getWidth(), cell)
                      : juce::Rectangle<float> (line.getX() + (cell + g) * (float) i, line.getY(), cell, line.getHeight());

        // A column of round keys: square keys, their names beside.
        if (down && ! joined && (shape == "circle" || shape == "round" || shape == "rect") && c.getWidth() > c.getHeight() * 1.6f && beside)
            c = side == "left" ? c.withLeft (c.getRight() - c.getHeight()) : c.withWidth (c.getHeight());

        juce::Path outline;

        if (joined)
        {
            // Only the strip's ends are rounded.
            const float radius = juce::jmin (c.getWidth(), c.getHeight()) * 0.5f * (formation == "rocker" ? 1.0f : rounding);
            const bool first = i == 0, last = i == k - 1;
            outline.addRoundedRectangle (c.getX(), c.getY(), c.getWidth(), c.getHeight(), radius, radius,
                                         first, down ? first : last, down ? last : first, last);
        }
        else
        {
            outline = shapeIn (c, shape, rounding, 0);
        }

        juce::Rectangle<float> label = capLabel (c);

        if (beside && ! lampNames)
            label = down ? juce::Rectangle<float> (names.getX(), c.getY(), names.getWidth(), c.getHeight())
                         : juce::Rectangle<float> (c.getX() - g * 0.5f, names.getY(), c.getWidth() + g, names.getHeight());

        add (outline, label);

        if (beside && down && ! lampNames)
            keys.back().edge = -pi * 0.5f;
    }

    return keys;
}

int SelectorSet::keyAt (const juce::var& d, juce::Rectangle<float> area, int count, juce::Point<float> at)
{
    const auto keys = layout (d, juce::Rectangle<float> (area.getWidth(), area.getHeight()), count);
    const auto local = at - area.getPosition();

    for (int i = (int) keys.size(); --i >= 0;)
        if (keys[(size_t) i].outline.contains (local))
            return i;

    // Near one (its name, or between): the nearest whose box is close.
    int best = -1;
    float bestDistance = juce::jmax (4.0f, juce::jmin (area.getWidth(), area.getHeight()) * 0.08f);

    for (int i = 0; i < (int) keys.size(); ++i)
    {
        const auto box = keys[(size_t) i].outline.getBounds().getUnion (keys[(size_t) i].label);
        const auto distance = box.getCentre().getDistanceFrom (local) - juce::jmin (box.getWidth(), box.getHeight()) * 0.5f;

        if (box.contains (local))
            return i;

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }

    return best;
}

void SelectorSet::paint (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, const juce::StringArray& names,
                         const std::vector<bool>& lit, float lightAngle, float lightAltitude, float resolution)
{
    if (area.isEmpty() || ! design.isObject())
        return;

    const float res = juce::jlimit (1.0f, 3.0f, resolution);
    const int w = juce::jmax (8, juce::roundToInt (area.getWidth() * res)), h = juce::jmax (8, juce::roundToInt (area.getHeight() * res));
    const int count = juce::jmax (1, (int) lit.size());
    const auto pictures = picturesOf (design, w, h, count, names, lightAngle, lightAltitude);

    if (pictures == nullptr)
        return;   // being made: painted again once it is

    const auto pw = juce::jmax (1, pictures->width > 0 ? pictures->width : w), ph = juce::jmax (1, pictures->height > 0 ? pictures->height : h);
    const auto place = juce::AffineTransform::scale (area.getWidth() / (float) pw, area.getHeight() / (float) ph).translated (area.getX(), area.getY());

    juce::Graphics::ScopedSaveState state (g);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageTransformed (pictures->off, place);

    // The keys that are on: their picture lit, where each is.
    const float glowRoom = juce::jmin (area.getWidth(), area.getHeight()) * res * 0.06f + 2.0f;
    const bool withLabels = textOf (design, "labels", "caps") == "beside";

    for (size_t i = 0; i < pictures->keys.size() && i < lit.size(); ++i)
    {
        if (! lit[i])
            continue;

        juce::Graphics::ScopedSaveState one (g);
        auto region = regionOf (pictures->keys[i].outline, i < pictures->words.size() ? pictures->words[i] : pictures->keys[i].label,
                                glowRoom * 0.5f, withLabels);
        region.applyTransform (place);
        g.reduceClipRegion (region);
        g.drawImageTransformed (pictures->on, place);
    }
}

//==============================================================================
namespace
{
    struct Template
    {
        const char* name;
        int keys;
        const char* json;
    };

    const Template templates[] {
        { "Lit keys round the bottom of a knob", 6,
          R"({ "render": "3d", "formation": "arc", "arcFrom": 100, "arcTo": 260, "inner": 62, "shape": "segment", "gap": 14, "rounding": 40,
               "capColour": "#FF1B1D22", "capOnColour": "#FF20262B", "edge": true, "edgeOff": 30, "glowColour": "#FF5FE0E8", "glow": 75,
               "labelColour": "#FFA8E6EA", "labelOnColour": "#FFFFFFFF", "textSize": 26, "material": "Satin plastic", "height": 16 })" },
        { "Label ring (joined, round the top)", 4,
          R"({ "render": "2d", "style": "flat", "formation": "arc", "arcFrom": 250, "arcTo": 470, "inner": 76, "shape": "segment", "gap": 3, "rounding": 0,
               "capColour": "#FF17191C", "capOnColour": "#FF1E2B2E", "edge": true, "edgeOff": 35, "glowColour": "#FF6FE3EA", "glow": 45,
               "labelColour": "#FF9FD9DE", "labelOnColour": "#FFFFFFFF", "textSize": 30 })" },
        { "Square keys with lamps", 4,
          R"({ "render": "3d", "formation": "row", "shape": "rect", "rounding": 14, "gap": 14, "led": "dot", "ledColour": "#FFFF3B30",
               "capColour": "#FF2C2E33", "capOnColour": "#FF2C2E33", "glow": 0, "labelColour": "#FFD9DBE0", "labelOnColour": "#FFFFFFFF", "textSize": 26,
               "material": "Satin plastic", "height": 22 })" },
        { "Round buttons in a column", 3,
          R"({ "render": "3d", "formation": "column", "shape": "circle", "gap": 22, "labels": "beside", "capColour": "#FF2A2C31", "capOnColour": "#FF3A6FD8",
               "glowColour": "#FF4F8BFF", "glow": 55, "capGlow": true, "labelColour": "#FFB8BCC4", "labelOnColour": "#FFFFFFFF", "textSize": 24,
               "material": "Gloss plastic", "height": 26 })" },
        { "Zipper triangles", 6,
          R"({ "render": "3d", "formation": "zipper", "shape": "triangle", "gap": 10, "rounding": 30, "capColour": "#FFD6D7DA", "capOnColour": "#FFF4F4F2",
               "glowColour": "#FFFFB547", "glow": 60, "edge": true, "edgeOff": 0, "labelColour": "#FF2A2B2E", "labelOnColour": "#FF101010", "textSize": 30,
               "material": "Satin plastic", "height": 20 })" },
        { "Tab strip", 4,
          R"({ "render": "2d", "style": "soft", "formation": "segmented", "rounding": 100, "gap": 6, "capColour": "#FF24262B", "capOnColour": "#FF2FB36B",
               "glowColour": "#FF2FB36B", "glow": 25, "labelColour": "#FFB0B3BA", "labelOnColour": "#FFFFFFFF", "textSize": 36 })" },
        { "Step row in colour groups", 16,
          R"({ "render": "3d", "formation": "row", "shape": "rect", "rounding": 12, "gap": 18, "led": "dot", "ledColour": "#FFFF4A2A", "labels": "none",
               "keyColours": [ "#FFD8432F", "#FFE8892E", "#FFE9CE4A", "#FFEDEDE6" ], "group": 4, "capOnColour": "#FFFFFFFF", "glow": 0,
               "material": "Satin plastic", "height": 24 })" },
        { "Rubber pads 4 x 4", 16,
          R"({ "render": "3d", "formation": "grid", "columns": 4, "shape": "rect", "rounding": 22, "gap": 12, "capColour": "#FF2B2C30", "capOnColour": "#FF7A5CFF",
               "glowColour": "#FF8C6CFF", "glow": 70, "capGlow": true, "labels": "none", "material": "Rubber", "height": 16, "plate": true, "plateColour": "#FF141417" })" },
        { "D-pad", 5,
          R"({ "render": "3d", "formation": "cross", "gap": 10, "rounding": 40, "capColour": "#FF26282C", "capOnColour": "#FF2F3238", "glowColour": "#FF5FE0E8",
               "glow": 50, "edge": true, "edgeOff": 0, "labelColour": "#FFB8BCC4", "labelOnColour": "#FFFFFFFF", "textSize": 30, "material": "Satin plastic", "height": 18 })" },
        { "Rocker (down / up)", 2,
          R"({ "render": "3d", "formation": "rocker", "gap": 8, "capColour": "#FF2A2C30", "capOnColour": "#FF33363C", "glow": 0,
               "labelColour": "#FFD0D3D8", "labelOnColour": "#FFFFFFFF", "textSize": 46, "material": "Satin plastic", "height": 24, "travel": 40 })" },
        { "Lamps with names", 4,
          R"({ "render": "2d", "style": "flat", "formation": "column", "shape": "led", "gap": 10, "capColour": "#FF3A1210", "capOnColour": "#FFFF3B30",
               "glowColour": "#FFFF3B30", "glow": 60, "labelColour": "#FFB8BCC4", "labelOnColour": "#FFFFFFFF", "textSize": 46 })" },
        { "Chrome buttons round a ring", 8,
          R"({ "render": "3d", "formation": "ring", "shape": "circle", "inner": 52, "gap": 30, "labels": "beside", "capColour": "#FFD9DCE1", "capOnColour": "#FFF2F4F7",
               "glowColour": "#FF4FD1FF", "glow": 60, "edge": true, "edgeOff": 0, "labelColour": "#FFB8BCC4", "labelOnColour": "#FFFFFFFF", "textSize": 30,
               "material": "Chrome", "height": 24 })" },
        { "Chrome buttons in a row", 4,
          R"({ "render": "3d", "formation": "row", "shape": "circle", "gap": 30, "labels": "beside", "capColour": "#FFD9DCE1", "capOnColour": "#FFE8EBEF",
               "glowColour": "#FFFF8A2A", "glow": 55, "edge": true, "edgeOff": 0, "labelColour": "#FFB8BCC4", "labelOnColour": "#FFFFFFFF", "textSize": 34,
               "material": "Chrome", "height": 26 })" },
        { "Rubber pills", 4,
          R"({ "render": "3d", "formation": "row", "shape": "pill", "gap": 16, "capColour": "#FF3A3C41", "capOnColour": "#FFE84A3A", "glowColour": "#FFFF5A40",
               "glow": 45, "capGlow": true, "labelColour": "#FFD0D3D8", "labelOnColour": "#FFFFFFFF", "textSize": 34, "material": "Rubber", "height": 20 })" },
        { "Flat table", 8,
          R"({ "render": "2d", "style": "flat", "formation": "grid", "columns": 4, "shape": "rect", "rounding": 10, "gap": 6, "capColour": "#FF202226",
               "capOnColour": "#FF2F7BFF", "glowColour": "#FF2F7BFF", "glow": 30, "labelColour": "#FFB0B3BA", "labelOnColour": "#FFFFFFFF", "textSize": 32 })" },
        { "Round keys on an arc", 5,
          R"({ "render": "3d", "formation": "arc", "arcFrom": 230, "arcTo": 490, "inner": 55, "shape": "circle", "gap": 24, "capColour": "#FF2A2C31",
               "capOnColour": "#FF2A2C31", "led": "dot", "ledColour": "#FF4CFF7A", "glow": 0, "labelColour": "#FFD0D3D8", "labelOnColour": "#FFFFFFFF",
               "textSize": 30, "material": "Satin plastic", "height": 24, "plate": true, "plateColour": "#FF141518" })" },
    };
}

juce::StringArray SelectorSet::templateNames()
{
    juce::StringArray names;

    for (const auto& t : templates)
        names.add (t.name);

    return names;
}

juce::var SelectorSet::makeTemplate (int index)
{
    const auto count = (int) (sizeof (templates) / sizeof (templates[0]));
    return juce::JSON::parse (juce::String (templates[juce::jlimit (0, count - 1, index)].json));
}

int SelectorSet::templateKeys (int index)
{
    const auto count = (int) (sizeof (templates) / sizeof (templates[0]));
    return templates[juce::jlimit (0, count - 1, index)].keys;
}

juce::File SelectorSet::galleryFolder()
{
    auto folder = SettingsFolder::folder ("Selector Gallery");
    folder.createDirectory();
    return folder;
}
