// D:\workspace\Fanan_web_hub\src\timberwolf\TwLook.h
#pragma once
//==============================================================================
//  TwLook.h  -  Timber Wolf's colours, its family badges and its icons.
//
//  Every family has its shape and colour (the badge on the chain graph and on
//  the panel's title): drive a circle, dynamics a square, filter a rhombus,
//  modulation a hexagon, pitch a triangle, time a pentagon, ambience an
//  octagon, amp/cab a wide rectangle, utility a small rounded square.  The
//  badge is drawn lit from above: a glossy face, a rim, a soft drop shadow, and
//  a glow round it while the effect is on; switched off it goes grey and dim.
//==============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "TwBlock.h"
#include "TwIcons.h"

#include <map>

namespace tw::look
{
    // The panel.
    inline const juce::Colour ink        { 0xffe8eaee };
    inline const juce::Colour inkDim     { 0xff9aa0aa };
    inline const juce::Colour inkFaint   { 0xff5d636d };
    inline const juce::Colour accent     { 0xffff8a2a };   // the wolf's amber
    inline const juce::Colour accentHot  { 0xffffb15e };
    inline const juce::Colour screenBg   { 0xff070b12 };
    inline const juce::Colour screenLine { 0xff8ea3c4 };
    inline const juce::Colour deck       { 0xff23262c };
    inline const juce::Colour ledOn      { 0xff3dff7a };
    inline const juce::Colour ledOff     { 0xff1a2a1f };

    inline juce::Colour familyColour (Family f)
    {
        switch (f)
        {
            case Family::drive:      return juce::Colour (0xffe8642c);
            case Family::dynamics:   return juce::Colour (0xff22b07d);
            case Family::filter:     return juce::Colour (0xffe9b82c);
            case Family::modulation: return juce::Colour (0xff8c55dc);
            case Family::pitch:      return juce::Colour (0xffe4488c);
            case Family::time:       return juce::Colour (0xff3480ff);
            case Family::ambience:   return juce::Colour (0xff23b3a4);
            case Family::ampcab:     return juce::Colour (0xff8a919b);
            case Family::utility:
            case Family::count:
            default:                 return juce::Colour (0xff5b616b);
        }
    }

    /** The badge's outline for a family, in a box. */
    inline juce::Path badgeShape (Family f, juce::Rectangle<float> r)
    {
        juce::Path p;
        const auto c = r.getCentre();
        const float s = juce::jmin (r.getWidth(), r.getHeight());

        const auto polygon = [&] (int sides, float radius, float rotation)
        {
            juce::Path q;
            for (int i = 0; i < sides; ++i)
            {
                const float a = rotation + juce::MathConstants<float>::twoPi * (float) i / (float) sides;
                const juce::Point<float> pt (c.x + radius * std::sin (a), c.y - radius * std::cos (a));
                if (i == 0) q.startNewSubPath (pt); else q.lineTo (pt);
            }
            q.closeSubPath();
            return q.createPathWithRoundedCorners (radius * 0.12f);
        };

        switch (f)
        {
            case Family::drive:      p.addEllipse (juce::Rectangle<float> (s, s).withCentre (c)); break;
            case Family::dynamics:   p.addRoundedRectangle (juce::Rectangle<float> (s * 0.9f, s * 0.9f).withCentre (c), s * 0.12f); break;
            case Family::filter:     p = polygon (4, s * 0.56f, 0.0f); break;
            case Family::modulation: p = polygon (6, s * 0.53f, juce::MathConstants<float>::pi / 6.0f); break;
            case Family::pitch:      p = polygon (3, s * 0.66f, 0.0f); p.applyTransform (juce::AffineTransform::translation (0.0f, s * 0.09f)); break;
            case Family::time:       p = polygon (5, s * 0.54f, 0.0f); break;
            case Family::ambience:   p = polygon (8, s * 0.53f, juce::MathConstants<float>::pi / 8.0f); break;
            case Family::ampcab:     p.addRoundedRectangle (juce::Rectangle<float> (s * 1.45f, s * 0.82f).withCentre (c), s * 0.12f); break;
            case Family::utility:
            case Family::count:
            default:                 p.addRoundedRectangle (juce::Rectangle<float> (s * 0.7f, s * 0.7f).withCentre (c), s * 0.16f); break;
        }

        return p;
    }

    /** An icon by its id ("gfx-overdrive"), drawn white in a box; nothing when it isn't there. */
    inline void drawIcon (juce::Graphics& g, const juce::String& id, juce::Rectangle<float> r, juce::Colour colour, float strokeScale = 1.0f)
    {
        static const std::map<juce::String, juce::Array<std::pair<bool, juce::Path>>> parsed = []
        {
            std::map<juce::String, juce::Array<std::pair<bool, juce::Path>>> m;

            for (const auto& icon : icons::all)
            {
                juce::Array<std::pair<bool, juce::Path>> parts;

                for (const auto& part : juce::StringArray::fromTokens (icon.path, "|", {}))
                    if (part.length() > 1)
                        parts.add ({ part[0] == 'F', juce::Drawable::parseSVGPath (part.substring (1)) });

                m[icon.id] = parts;
            }

            return m;
        }();

        auto it = parsed.find (id);

        if (it == parsed.end())
            return;

        const float s = juce::jmin (r.getWidth(), r.getHeight()) / 24.0f;
        const auto t = juce::AffineTransform::scale (s).translated (r.getCentreX() - 12.0f * s, r.getCentreY() - 12.0f * s);
        g.setColour (colour);

        for (const auto& [fill, path] : it->second)
        {
            auto p = path;
            p.applyTransform (t);

            if (fill)
                g.fillPath (p);
            else
                g.strokePath (p, juce::PathStrokeType (2.0f * s * strokeScale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    /** A family badge: lit glossy face, rim, shadow, the icon on it; glowing while on. */
    inline void drawBadge (juce::Graphics& g, Family f, const juce::String& iconId, juce::Rectangle<float> r, bool on,
                           bool selected = false, float glow = 1.0f)
    {
        const auto shape = badgeShape (f, r);
        const auto base = on ? familyColour (f) : juce::Colour (0xff3a3e45);

        // The glow round it, while it sounds.
        if (on && glow > 0.0f)
        {
            juce::DropShadow (base.withAlpha (0.55f * glow), (int) (r.getHeight() * 0.45f), {}).drawForPath (g, shape);
        }

        // Its drop shadow on the screen.
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), (int) (r.getHeight() * 0.18f),
                          { 0, (int) std::ceil (r.getHeight() * 0.08f) }).drawForPath (g, shape);

        // The face: lighter at the top (lit from above), darker at the bottom.
        const auto bounds = shape.getBounds();
        juce::ColourGradient face (base.brighter (0.45f), bounds.getCentreX(), bounds.getY(),
                                   base.darker (0.55f), bounds.getCentreX(), bounds.getBottom(), false);
        face.addColour (0.45, base);
        g.setGradientFill (face);
        g.fillPath (shape);

        // The gloss: a soft highlight over its top half.
        {
            juce::Graphics::ScopedSaveState keep (g);
            g.reduceClipRegion (shape);
            juce::ColourGradient gloss (juce::Colours::white.withAlpha (on ? 0.38f : 0.16f), bounds.getCentreX(), bounds.getY(),
                                        juce::Colours::white.withAlpha (0.0f), bounds.getCentreX(), bounds.getCentreY(), false);
            g.setGradientFill (gloss);
            g.fillEllipse (bounds.withHeight (bounds.getHeight() * 0.62f).expanded (bounds.getWidth() * 0.15f, 0.0f).translated (0.0f, -bounds.getHeight() * 0.08f));
        }

        // The rim.
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (shape, juce::PathStrokeType (1.2f));
        g.setColour (juce::Colours::white.withAlpha (on ? 0.35f : 0.12f));
        g.strokePath (shape, juce::PathStrokeType (0.6f));

        if (selected)
        {
            g.setColour (accentHot);
            g.strokePath (shape, juce::PathStrokeType (2.4f));
        }

        // The icon: white, sized to the badge's inner part.
        const float inner = juce::jmin (r.getWidth(), r.getHeight()) * (f == Family::pitch ? 0.42f : 0.56f);
        auto iconBox = juce::Rectangle<float> (inner, inner).withCentre (r.getCentre());
        if (f == Family::pitch) iconBox = iconBox.translated (0.0f, r.getHeight() * 0.1f);

        drawIcon (g, iconId, iconBox.translated (0.0f, 1.0f), juce::Colours::black.withAlpha (0.35f));
        drawIcon (g, iconId, iconBox, juce::Colours::white.withAlpha (on ? 1.0f : 0.45f));
    }

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }
}
