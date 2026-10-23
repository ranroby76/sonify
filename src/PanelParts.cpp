// D:\workspace\Fanan_web_hub\src\PanelParts.cpp

#include "PanelParts.h"
#include "FontLibrary.h"
#include "stella_keys.h"

#include <cmath>

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;

    juce::String setting (const GuiWidget& w, const char* key)
    {
        const auto found = w.settings.find (key);
        return found != w.settings.end() && ! found->second.isVoid() ? found->second.toString().trim() : juce::String();
    }

    juce::String displayOf (const GuiWidget& w)
    {
        return setting (w, "display").toLowerCase().removeCharacters (" -_");
    }

    juce::Colour colourSetting (const GuiWidget& w, const char* key, juce::Colour fallback)
    {
        const auto text = setting (w, key);
        return text.isNotEmpty() ? GuiLayout::colourFromString (text, fallback) : fallback;
    }

    /** A setting that's on or off: true, "yes", 1... (missing: fallback). */
    bool flagSetting (const GuiWidget& w, const char* key, bool fallback)
    {
        const auto found = w.settings.find (key);

        if (found == w.settings.end() || found->second.isVoid())
            return fallback;

        if (found->second.isBool() || found->second.isInt() || found->second.isInt64() || found->second.isDouble())
            return (bool) found->second;

        const auto t = found->second.toString().trim().toLowerCase();
        return t == "true" || t == "yes" || t == "on" || t == "1";
    }

    /** Words that read on a colour: near-black on light colours, white on dark ones. */
    juce::Colour inkOn (juce::Colour c)
    {
        return c.withAlpha (1.0f).getPerceivedBrightness() > 0.62f ? juce::Colour (0xff141416) : juce::Colours::white;
    }

    juce::Point<float> polar (float angle, float radius)
    {
        return { std::cos (angle) * radius, std::sin (angle) * radius };
    }

    /** One of a few names, or the first (the standard) when it's none of them. */
    juce::String pick (const juce::String& asked, const juce::StringArray& ids, std::initializer_list<std::pair<const char*, const char*>> aliases)
    {
        if (ids.contains (asked))
            return asked;

        for (const auto& [alias, id] : aliases)
            if (asked == alias)
                return id;

        return ids[0];
    }

    //==========================================================================
    // Lamps

    void paintDot (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float lit)
    {
        const auto size = juce::jmin (area.getWidth(), area.getHeight()) * 0.6f;
        const auto lamp = juce::Rectangle<float> (size, size).withCentre (area.getCentre());

        if (lit > 0.0f)
        {
            g.setColour (colour.withAlpha (0.35f * lit));
            g.fillEllipse (lamp.expanded (size * 0.35f));
        }

        g.setColour (colour.darker (2.2f).withAlpha (0.9f).interpolatedWith (colour, lit));
        g.fillEllipse (lamp);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (lamp, 1.0f);
    }

    void paintDome (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, juce::Colour bezel, float lit)
    {
        const auto d = juce::jmin (area.getWidth(), area.getHeight()) * 0.8f;
        const auto c = area.getCentre();
        const auto ring = juce::Rectangle<float> (d, d).withCentre (c);
        const auto lens = ring.reduced (d * 0.15f);

        // Its light on the panel round it.
        if (lit > 0.01f)
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (colour.withAlpha (0.1f * lit));
                g.fillEllipse (lens.expanded (d * 0.16f * (float) k));
            }

        // The bezel, lit from above, a dark lip inside it.
        g.setGradientFill (juce::ColourGradient (bezel.brighter (0.6f), c.x, ring.getY(), bezel.darker (0.5f), c.x, ring.getBottom(), false));
        g.fillEllipse (ring);
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.fillEllipse (lens.expanded (d * 0.035f));

        // The lens: brightest a little off its middle, darker to its edge.
        const auto off = colour.withMultipliedSaturation (0.7f).darker (2.0f);
        const auto core = off.brighter (0.25f).interpolatedWith (colour.brighter (0.9f), lit);
        const auto body = off.interpolatedWith (colour, lit);
        const auto edge = off.darker (0.8f).interpolatedWith (colour.darker (0.55f), lit);
        juce::ColourGradient fill (core, c.x - lens.getWidth() * 0.08f, c.y - lens.getHeight() * 0.08f, edge, c.x + lens.getWidth() * 0.5f, c.y, true);
        fill.addColour (0.55, body);
        g.setGradientFill (fill);
        g.fillEllipse (lens);

        // Its reflections: a soft one at the upper left, a crescent below.
        const auto spot = juce::Rectangle<float> (lens.getWidth() * 0.42f, lens.getHeight() * 0.3f)
                              .withCentre (c + juce::Point<float> (-lens.getWidth() * 0.13f, -lens.getHeight() * 0.2f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.75f), spot.getCentreX(), spot.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), spot.getCentreX(), spot.getBottom(), false));
        g.fillEllipse (spot);

        juce::Path crescent;
        crescent.addCentredArc (c.x, c.y, lens.getWidth() * 0.36f, lens.getHeight() * 0.36f, 0.0f, pi * 0.62f, pi * 1.05f, true);
        g.setColour (juce::Colours::white.withAlpha (0.12f + 0.14f * lit));
        g.strokePath (crescent, juce::PathStrokeType (juce::jmax (1.0f, d * 0.04f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void paintJewel (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float lit)
    {
        const auto d = juce::jmin (area.getWidth(), area.getHeight()) * 0.84f;
        const auto c = area.getCentre();
        const auto ring = juce::Rectangle<float> (d, d).withCentre (c);
        const auto radius = d * 0.34f;

        if (lit > 0.01f)
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (colour.withAlpha (0.11f * lit));
                g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c).expanded (d * 0.15f * (float) k));
            }

        // The chrome bezel: the room mirrored in it, bright above, a dark band, bright below.
        juce::ColourGradient chrome (juce::Colour (0xfff3f5f8), c.x, ring.getY(), juce::Colour (0xffe1e5ea), c.x, ring.getBottom(), false);
        chrome.addColour (0.42, juce::Colour (0xff9aa0a8));
        chrome.addColour (0.56, juce::Colour (0xff5d6168));
        chrome.addColour (0.8, juce::Colour (0xffb9bec5));
        g.setGradientFill (chrome);
        g.fillEllipse (ring);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (ring.reduced (0.5f), 1.0f);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c).expanded (d * 0.04f));

        // The jewel's facets, each catching the light by the way it faces (the light: upper left).
        const auto off = colour.withMultipliedSaturation (0.85f).darker (1.7f);
        const auto base = off.interpolatedWith (colour.brighter (0.25f), lit);
        constexpr int facets = 8;
        const float table = radius * 0.46f;

        for (int i = 0; i < facets; ++i)
        {
            const float a0 = (float) i / (float) facets * 2.0f * pi + pi / (float) facets, a1 = a0 + 2.0f * pi / (float) facets;
            juce::Path facet;
            facet.startNewSubPath (c + polar (a0, radius));
            facet.lineTo (c + polar (a1, radius));
            facet.lineTo (c + polar (a1, table));
            facet.lineTo (c + polar (a0, table));
            facet.closeSubPath();

            const float facing = std::cos ((a0 + a1) * 0.5f + 0.75f * pi);
            g.setColour (base.withMultipliedBrightness (0.62f + 0.4f * facing));
            g.fillPath (facet);
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.strokePath (facet, juce::PathStrokeType (0.6f));
        }

        juce::Path top;

        for (int i = 0; i < facets; ++i)
        {
            const auto p = c + polar ((float) i / (float) facets * 2.0f * pi + pi / (float) facets, table);

            if (i == 0) top.startNewSubPath (p);
            else        top.lineTo (p);
        }

        top.closeSubPath();
        g.setColour (base.brighter (0.25f + 0.5f * lit));
        g.fillPath (top);

        // Glints: a sparkle on the facets facing the light, a hot core when it's lit.
        const auto glint = c + polar (-0.75f * pi, radius * 0.6f);
        const auto sparkle = juce::jmax (1.5f, radius * 0.2f);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (juce::Rectangle<float> (sparkle, sparkle).withCentre (glint));
        g.drawLine ({ glint.translated (-sparkle * 1.4f, 0.0f), glint.translated (sparkle * 1.4f, 0.0f) }, juce::jmax (0.6f, sparkle * 0.18f));
        g.drawLine ({ glint.translated (0.0f, -sparkle * 1.4f), glint.translated (0.0f, sparkle * 1.4f) }, juce::jmax (0.6f, sparkle * 0.18f));

        if (lit > 0.01f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.45f * lit));
            g.fillEllipse (juce::Rectangle<float> (table * 1.2f, table * 1.2f).withCentre (c));
        }
    }

    void paintSquareLed (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, juce::Colour bezel, float lit)
    {
        const auto box = area.reduced (juce::jmin (area.getWidth(), area.getHeight()) * 0.1f);
        const auto small = juce::jmin (box.getWidth(), box.getHeight());
        const auto corner = small * 0.16f;

        if (lit > 0.01f)
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (colour.withAlpha (0.1f * lit));
                g.fillRoundedRectangle (box.expanded (small * 0.13f * (float) k), corner + small * 0.13f * (float) k);
            }

        g.setGradientFill (juce::ColourGradient (bezel.brighter (0.4f), box.getX(), box.getY(), bezel.darker (0.4f), box.getX(), box.getBottom(), false));
        g.fillRoundedRectangle (box, corner);

        const auto lens = box.reduced (small * 0.14f);
        const auto off = colour.withMultipliedSaturation (0.75f).darker (2.0f);
        g.setGradientFill (juce::ColourGradient (off.brighter (0.2f).interpolatedWith (colour.brighter (0.5f), lit), lens.getX(), lens.getY(),
                                                 off.darker (0.3f).interpolatedWith (colour.darker (0.2f), lit), lens.getX(), lens.getBottom(), false));
        g.fillRoundedRectangle (lens, corner * 0.6f);

        if (lit > 0.01f)
        {
            juce::ColourGradient hot (juce::Colours::white.withAlpha (0.5f * lit), lens.getCentreX(), lens.getCentreY(),
                                      juce::Colours::white.withAlpha (0.0f), lens.getCentreX() + lens.getWidth() * 0.4f, lens.getCentreY(), true);
            g.setGradientFill (hot);
            g.fillRoundedRectangle (lens, corner * 0.6f);
        }

        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.fillRoundedRectangle (lens.withHeight (lens.getHeight() * 0.42f), corner * 0.6f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (lens, corner * 0.6f, 1.0f);
    }

    void paintTube (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float lit)
    {
        const auto height = area.getHeight() * 0.94f;
        const auto width = juce::jmin (area.getWidth() * 0.86f, height * 0.5f);
        auto tube = juce::Rectangle<float> (width, height).withCentre (area.getCentre());
        const auto base = tube.removeFromBottom (height * 0.17f);
        const auto glassArea = tube;

        // Its glow on the panel.
        if (lit > 0.01f)
        {
            const auto centre = glassArea.getCentre().translated (0.0f, glassArea.getHeight() * 0.08f);
            juce::ColourGradient halo (colour.withAlpha (0.26f * lit), centre.x, centre.y, colour.withAlpha (0.0f), centre.x + width * 1.2f, centre.y, true);
            g.setGradientFill (halo);
            g.fillEllipse (juce::Rectangle<float> (width * 2.6f, glassArea.getHeight() * 1.2f).withCentre (centre));
        }

        // The pins, the base.
        g.setColour (juce::Colour (0xff8f9298));

        for (int i = 0; i < 3; ++i)
        {
            const auto x = base.getX() + base.getWidth() * (0.3f + 0.2f * (float) i);
            g.fillRect (juce::Rectangle<float> (juce::jmax (1.0f, width * 0.04f), base.getHeight() * 0.3f).withCentre ({ x, base.getBottom() }));
        }

        const auto body = base.reduced (base.getWidth() * 0.05f, 0.0f).withTrimmedBottom (base.getHeight() * 0.12f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3e2c21), body.getX(), body.getY(), juce::Colour (0xff120b08), body.getX(), body.getBottom(), false));
        g.fillRoundedRectangle (body, body.getHeight() * 0.22f);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawHorizontalLine (juce::roundToInt (body.getY() + body.getHeight() * 0.35f), body.getX() + 2.0f, body.getRight() - 2.0f);

        // The glass: round on top, down into the base.
        juce::Path glass;
        glass.addRoundedRectangle (glassArea.getX(), glassArea.getY(), glassArea.getWidth(), glassArea.getHeight() + body.getHeight() * 0.3f,
                                   width * 0.5f, width * 0.5f, true, true, false, false);

        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (glass);

            g.setColour (juce::Colour (0x46000000));
            g.fillPath (glass);

            // The plates either side, the cathode's glow between them, its filament.
            const auto inner = glassArea.reduced (width * 0.18f, glassArea.getHeight() * 0.16f).withTrimmedTop (glassArea.getHeight() * 0.08f);
            const auto plateW = inner.getWidth() * 0.3f;

            for (const auto& plate : { inner.withWidth (plateW), inner.withTrimmedLeft (inner.getWidth() - plateW) })
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff55575d), plate.getX(), plate.getY(), juce::Colour (0xff222327), plate.getRight(), plate.getY(), false));
                g.fillRect (plate);
            }

            const juce::Point<float> core (glassArea.getCentreX(), inner.getCentreY());
            juce::ColourGradient warm (colour.brighter (0.2f).withAlpha (0.95f * lit), core.x, core.y, colour.withAlpha (0.0f), core.x + inner.getWidth() * 0.95f, core.y, true);
            warm.addColour (0.35, colour.withAlpha (0.55f * lit));
            g.setGradientFill (warm);
            g.fillRect (glassArea);

            g.setColour (colour.brighter (0.9f).withAlpha (0.2f + 0.8f * lit));
            g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmax (1.5f, width * 0.05f), inner.getHeight() * 0.8f).withCentre (core), 1.0f);

            // The getter's silvering at the top, the light's streak down the glass.
            const auto top = glassArea.withHeight (glassArea.getHeight() * 0.22f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xd0c9ccd2), top.getCentreX(), top.getY(), juce::Colour (0x00c9ccd2), top.getCentreX(), top.getBottom(), false));
            g.fillRect (top);

            const auto streak = juce::Rectangle<float> (width * 0.13f, glassArea.getHeight() * 0.7f)
                                    .withPosition (glassArea.getX() + width * 0.13f, glassArea.getY() + glassArea.getHeight() * 0.12f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.42f), streak.getX(), streak.getCentreY(),
                                                     juce::Colours::white.withAlpha (0.0f), streak.getRight(), streak.getCentreY(), false));
            g.fillRoundedRectangle (streak, streak.getWidth() * 0.5f);
        }

        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.strokePath (glass, juce::PathStrokeType (1.0f));
    }

    void paintEye (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, juce::Colour bezel, float lit)
    {
        const auto d = juce::jmin (area.getWidth(), area.getHeight()) * 0.86f;
        const auto c = area.getCentre();
        const auto outer = d * 0.5f * 0.86f, inner = d * 0.5f * 0.34f;

        g.setGradientFill (juce::ColourGradient (bezel.brighter (0.35f), c.x, c.y - d * 0.5f, bezel.darker (0.6f), c.x, c.y + d * 0.5f, false));
        g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (c));

        // The glowing ring.
        juce::Path ring;
        ring.addEllipse (juce::Rectangle<float> (outer * 2.0f, outer * 2.0f).withCentre (c));
        ring.addEllipse (juce::Rectangle<float> (inner * 2.0f, inner * 2.0f).withCentre (c));
        ring.setUsingNonZeroWinding (false);

        juce::ColourGradient phosphor (colour.brighter (0.6f), c.x, c.y, colour.darker (0.9f), c.x + outer, c.y, true);
        phosphor.addColour (0.6, colour);
        g.setGradientFill (phosphor);
        g.fillPath (ring);

        // The shadow, closing as the level rises.
        const auto half = juce::degreesToRadians (6.0f + (1.0f - juce::jlimit (0.0f, 1.0f, lit)) * 54.0f);
        juce::Path wedge;
        wedge.addPieSegment (juce::Rectangle<float> (outer * 2.0f, outer * 2.0f).withCentre (c), -half, half, inner / outer);
        g.setColour (colour.darker (3.2f).withAlpha (0.93f));
        g.fillPath (wedge);

        // The cap in the middle, the glass over it.
        g.setColour (juce::Colour (0xff0b0b0c));
        g.fillEllipse (juce::Rectangle<float> (inner * 2.0f, inner * 2.0f).withCentre (c));

        juce::Path shine;
        shine.addCentredArc (c.x, c.y, outer * 0.82f, outer * 0.82f, 0.0f, -pi * 0.48f, -pi * 0.1f, true);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.strokePath (shine, juce::PathStrokeType (juce::jmax (1.0f, d * 0.035f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    //==========================================================================
    // Selectors

    bool selectorVertical (const juce::String& display, juce::Rectangle<float> area, int count)
    {
        if (count < 2)
            return false;

        if (display == "lamps")
            return area.getHeight() >= area.getWidth() * 0.6f;

        if (display == "pill" || display == "keys" || display == "tiles")
            return area.getHeight() > area.getWidth() * 1.25f;

        return false;
    }

    juce::Rectangle<float> cellOf (juce::Rectangle<float> area, bool vertical, int i, int count)
    {
        if (vertical)
        {
            const auto h = area.getHeight() / (float) count;
            return { area.getX(), area.getY() + (float) i * h, area.getWidth(), h };
        }

        const auto w = area.getWidth() / (float) count;
        return { area.getX() + (float) i * w, area.getY(), w, area.getHeight() };
    }

    void drawName (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> box, juce::Colour colour, const juce::Font& font,
                   juce::Justification justification = juce::Justification::centred)
    {
        g.setColour (colour);
        g.setFont (font);
        g.drawFittedText (text, box.toNearestInt(), justification, 1, 0.75f);
    }

    //==========================================================================
    // 14-segment characters for the LCD: a top, b upper right, c lower right, d bottom, e lower
    // left, f upper left, g1 middle left, g2 middle right, h upper left diagonal, i upper middle,
    // j upper right diagonal, k lower left diagonal, l lower middle, m lower right diagonal.
    enum : int { sa = 1, sb = 2, sc = 4, sd = 8, se = 16, sf = 32, sg1 = 64, sg2 = 128, sh = 256, si = 512, sj = 1024, sk = 2048, sl = 4096, sm = 8192 };

    int segmentsOf (juce::juce_wchar c)
    {
        switch (juce::CharacterFunctions::toUpperCase (c))
        {
            case '0': return sa | sb | sc | sd | se | sf | sj | sk;
            case '1': return sb | sc | sj;
            case '2': return sa | sb | sg1 | sg2 | se | sd;
            case '3': return sa | sb | sc | sd | sg2 | sg1;
            case '4': return sf | sg1 | sg2 | sb | sc;
            case '5': return sa | sf | sg1 | sg2 | sc | sd;
            case '6': return sa | sf | se | sd | sc | sg1 | sg2;
            case '7': return sa | sb | sc;
            case '8': return sa | sb | sc | sd | se | sf | sg1 | sg2;
            case '9': return sa | sb | sc | sd | sf | sg1 | sg2;
            case 'A': return sa | sb | sc | se | sf | sg1 | sg2;
            case 'B': return sa | sb | sc | sd | sg2 | si | sl;
            case 'C': return sa | sd | se | sf;
            case 'D': return sa | sb | sc | sd | si | sl;
            case 'E': return sa | sd | se | sf | sg1 | sg2;
            case 'F': return sa | se | sf | sg1;
            case 'G': return sa | sc | sd | se | sf | sg2;
            case 'H': return sb | sc | se | sf | sg1 | sg2;
            case 'I': return sa | sd | si | sl;
            case 'J': return sb | sc | sd | se;
            case 'K': return se | sf | sg1 | sj | sm;
            case 'L': return sd | se | sf;
            case 'M': return sb | sc | se | sf | sh | sj;
            case 'N': return sb | sc | se | sf | sh | sm;
            case 'O': return sa | sb | sc | sd | se | sf;
            case 'P': return sa | sb | se | sf | sg1 | sg2;
            case 'Q': return sa | sb | sc | sd | se | sf | sm;
            case 'R': return sa | sb | se | sf | sg1 | sg2 | sm;
            case 'S': return sa | sf | sg1 | sg2 | sc | sd;
            case 'T': return sa | si | sl;
            case 'U': return sb | sc | sd | se | sf;
            case 'V': return se | sf | sk | sj;
            case 'W': return sb | sc | se | sf | sk | sm;
            case 'X': return sh | sj | sk | sm;
            case 'Y': return sh | sj | sl;
            case 'Z': return sa | sd | sj | sk;
            case '-': return sg1 | sg2;
            case '+': return sg1 | sg2 | si | sl;
            case '*': return sg1 | sg2 | sh | si | sj | sk | sl | sm;
            case '/': return sj | sk;
            case '\\': return sh | sm;
            case '%': return sj | sk | sf | sc;
            case '=': return sg1 | sg2 | sd;
            case '<': return sj | sm;
            case '>': return sh | sk;
            case '(': return sj | sm;
            case ')': return sh | sk;
            case '[': return sa | sd | se | sf;
            case ']': return sa | sb | sc | sd;
            case '_': return sd;
            case '\'': return si;
            case '"': return sf | si;
            case '?': return sa | sb | sg2 | sl;
            case '#': return sb | sc | sd | sg1 | sg2 | si | sl;
            default:  return 0;
        }
    }

    /** One character cell: its segments, the ones that aren't lit faintly (as on a real LCD). */
    void paintSegments (juce::Graphics& g, juce::Rectangle<float> cell, int lit, juce::Colour ink, float slant)
    {
        const float w = cell.getWidth(), h = cell.getHeight();
        const float t = juce::jmax (1.0f, w * 0.15f);
        const float gap = t * 0.75f;

        auto at = [&] (float x, float y)
        {
            // A little italic, as the glass is printed.
            return juce::Point<float> (cell.getX() + x * w + (1.0f - y) * h * slant, cell.getY() + y * h);
        };

        struct Seg { int bit; float x1, y1, x2, y2; };
        static const Seg segs[] {
            { sa, 0, 0, 1, 0 }, { sb, 1, 0, 1, 0.5f }, { sc, 1, 0.5f, 1, 1 }, { sd, 0, 1, 1, 1 }, { se, 0, 0.5f, 0, 1 }, { sf, 0, 0, 0, 0.5f },
            { sg1, 0, 0.5f, 0.5f, 0.5f }, { sg2, 0.5f, 0.5f, 1, 0.5f }, { sh, 0, 0, 0.5f, 0.5f }, { si, 0.5f, 0, 0.5f, 0.5f },
            { sj, 1, 0, 0.5f, 0.5f }, { sk, 0, 1, 0.5f, 0.5f }, { sl, 0.5f, 0.5f, 0.5f, 1 }, { sm, 1, 1, 0.5f, 0.5f } };

        const juce::PathStrokeType stroke (t, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded);

        for (const auto& s : segs)
        {
            auto a = at (s.x1, s.y1), b = at (s.x2, s.y2);
            const auto along = b - a;
            const float length = along.getDistanceFromOrigin();

            if (length <= gap * 2.0f)
                continue;

            const auto unit = along / length;
            a += unit * gap;
            b -= unit * gap;

            juce::Path line;
            line.startNewSubPath (a);
            line.lineTo (b);
            g.setColour ((lit & s.bit) != 0 ? ink : ink.withMultipliedAlpha (0.07f));
            g.strokePath (line, stroke);
        }
    }

    //==========================================================================
    // Keyboards

    juce::Rectangle<float> keyArea (const GuiWidget& w, int note)
    {
        const auto b = w.bounds.toFloat();
        const auto k = stella::keys::keyOf (note, w.lowNote, w.highNote, b.getX(), b.getY(), b.getWidth(), b.getHeight());
        return { k.x, k.y, k.w, k.h };
    }

    void paintClassicKeys (juce::Graphics& g, const GuiWidget& w)
    {
        const auto b = w.bounds.toFloat();

        // The gaps between the white keys show the dark bed they sit in.
        g.setColour (juce::Colour (0xff111113));
        g.fillRect (b);

        for (int note = w.lowNote; note <= w.highNote; ++note)
        {
            if (stella::keys::isBlack (note))
                continue;

            const auto key = keyArea (w, note).reduced (0.5f, 0.0f);
            juce::ColourGradient ivory (juce::Colour (0xffcfcac0), key.getX(), key.getY(), juce::Colour (0xfffbfaf6), key.getX(), key.getBottom(), false);
            ivory.addColour (0.18, juce::Colour (0xffefece5));
            g.setGradientFill (ivory);
            g.fillRoundedRectangle (key.withTrimmedBottom (1.0f), 2.0f);

            // The front lip, a shade darker.
            g.setColour (juce::Colour (0xffd8d3c9));
            g.fillRect (key.withTop (key.getBottom() - juce::jmax (2.0f, key.getHeight() * 0.05f)).withTrimmedBottom (1.0f));
        }

        for (int note = w.lowNote; note <= w.highNote; ++note)
        {
            if (! stella::keys::isBlack (note))
                continue;

            const auto key = keyArea (w, note);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c31), key.getX(), key.getY(), juce::Colour (0xff09090b), key.getX(), key.getBottom(), false));
            g.fillRoundedRectangle (key, 1.5f);

            // Its top face, lit from above.
            const auto face = key.reduced (key.getWidth() * 0.14f, 0.0f).withTrimmedBottom (key.getHeight() * 0.12f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1b1f), face.getX(), face.getY(), juce::Colour (0xff3d3d44), face.getX(), face.getBottom(), false));
            g.fillRoundedRectangle (face, 1.0f);
        }
    }

    void paintReverseKeys (juce::Graphics& g, const GuiWidget& w)
    {
        const auto b = w.bounds.toFloat();
        g.setColour (juce::Colour (0xff050506));
        g.fillRect (b);

        // Ebony naturals, a brown-black with a sheen near the front.
        for (int note = w.lowNote; note <= w.highNote; ++note)
        {
            if (stella::keys::isBlack (note))
                continue;

            const auto key = keyArea (w, note).reduced (0.5f, 0.0f);
            juce::ColourGradient ebony (juce::Colour (0xff141214), key.getX(), key.getY(), juce::Colour (0xff2e2a2b), key.getX(), key.getBottom(), false);
            ebony.addColour (0.85, juce::Colour (0xff262224));
            g.setGradientFill (ebony);
            g.fillRoundedRectangle (key.withTrimmedBottom (1.0f), 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (key.withTop (key.getBottom() - juce::jmax (2.0f, key.getHeight() * 0.05f)).withTrimmedBottom (1.0f));
            g.setColour (juce::Colours::black);
            g.drawVerticalLine (juce::roundToInt (key.getRight()), key.getY(), key.getBottom());
        }

        // Bone sharps, raised, their top face lit.
        for (int note = w.lowNote; note <= w.highNote; ++note)
        {
            if (! stella::keys::isBlack (note))
                continue;

            const auto key = keyArea (w, note);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRoundedRectangle (key.translated (0.0f, 1.0f), 1.5f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffbfb8a8), key.getX(), key.getY(), juce::Colour (0xff8f897c), key.getX(), key.getBottom(), false));
            g.fillRoundedRectangle (key, 1.5f);

            const auto face = key.reduced (key.getWidth() * 0.14f, 0.0f).withTrimmedBottom (key.getHeight() * 0.12f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe4ddcd), face.getX(), face.getY(), juce::Colour (0xfff6f1e6), face.getX(), face.getBottom(), false));
            g.fillRoundedRectangle (face, 1.0f);
        }

        const auto shadow = b.withHeight (juce::jmax (4.0f, b.getHeight() * 0.08f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), shadow.getX(), shadow.getY(),
                                                 juce::Colours::transparentBlack, shadow.getX(), shadow.getBottom(), false));
        g.fillRect (shadow);
    }

    void paintModernKeys (juce::Graphics& g, const GuiWidget& w)
    {
        const auto b = w.bounds.toFloat();
        g.setColour (juce::Colour (0xff09090b));
        g.fillRect (b);

        for (int note = w.lowNote; note <= w.highNote; ++note)
        {
            if (stella::keys::isBlack (note))
                continue;

            const auto key = keyArea (w, note).reduced (1.0f, 0.0f).withTrimmedBottom (1.0f);
            const auto r = juce::jmin (4.0f, key.getWidth() * 0.18f);
            juce::Path shape;
            shape.addRoundedRectangle (key.getX(), key.getY(), key.getWidth(), key.getHeight(), r, r, false, false, true, true);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2d33), key.getX(), key.getY(), juce::Colour (0xff3d4149), key.getX(), key.getBottom(), false));
            g.fillPath (shape);

            // A soft front edge.
            g.setColour (juce::Colour (0xff4b505a));
            g.fillRect (key.withTop (key.getBottom() - juce::jmax (1.5f, key.getHeight() * 0.035f)).reduced (r * 0.5f, 0.0f));
        }

        for (int note = w.lowNote; note <= w.highNote; ++note)
        {
            if (! stella::keys::isBlack (note))
                continue;

            const auto key = keyArea (w, note).reduced (0.5f, 0.0f);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRoundedRectangle (key.expanded (1.0f, 0.0f).translated (0.0f, 1.5f), 2.5f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff18191c), key.getX(), key.getY(), juce::Colour (0xff050506), key.getX(), key.getBottom(), false));
            g.fillRoundedRectangle (key, 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.09f));
            g.fillRoundedRectangle (key.reduced (key.getWidth() * 0.2f, 0.0f).withTrimmedBottom (key.getHeight() * 0.15f).withTrimmedTop (2.0f), 1.5f);
        }
    }
}

//==============================================================================
const juce::StringArray& PanelParts::lampDisplays()
{
    static const juce::StringArray ids { "dot", "dome", "jewel", "square", "tube", "eye" };
    return ids;
}

const juce::StringArray& PanelParts::lampDisplayNames()
{
    static const juce::StringArray names { "Round light", "Domed LED", "Jewel pilot light", "Square LED", "Glowing tube", "Magic eye" };
    return names;
}

juce::String PanelParts::lampDisplayOf (const GuiWidget& w)
{
    return pick (displayOf (w), lampDisplays(), { { "led", "dot" }, { "round", "dot" }, { "standard", "dot" }, { "domed", "dome" }, { "lens", "dome" },
                                                  { "consoleled", "dome" }, { "pilot", "jewel" }, { "pilotlight", "jewel" }, { "faceted", "jewel" },
                                                  { "gem", "jewel" }, { "crystal", "jewel" }, { "rect", "square" }, { "rectangle", "square" },
                                                  { "bar", "square" }, { "clip", "square" }, { "flat", "square" }, { "valve", "tube" },
                                                  { "vacuumtube", "tube" }, { "tubeglow", "tube" }, { "filament", "tube" }, { "magiceye", "eye" },
                                                  { "tuningeye", "eye" }, { "fan", "eye" } });
}

bool PanelParts::lampFollows (const GuiWidget& w)
{
    const auto display = lampDisplayOf (w);
    return flagSetting (w, "follow", display == "tube" || display == "eye");
}

void PanelParts::paintLamp (juce::Graphics& g, juce::Rectangle<float> area, const GuiWidget& w, float lit)
{
    if (area.isEmpty())
        return;

    lit = juce::jlimit (0.0f, 1.0f, lit);
    const auto display = lampDisplayOf (w);
    const auto standard = display == "eye" ? juce::Colour (0xff4cf08a) : display == "tube" ? juce::Colour (0xffff8a2a) : juce::Colour (0xffffb020);
    const auto colour = w.colour.isTransparent() ? standard : w.colour;
    const auto bezel = colourSetting (w, "bezel", juce::Colour (0xff1b1b1e));

    if (display == "dome")        paintDome (g, area, colour, bezel, lit);
    else if (display == "jewel")  paintJewel (g, area, colour, lit);
    else if (display == "square") paintSquareLed (g, area, colour, bezel, lit);
    else if (display == "tube")   paintTube (g, area, colour, lit);
    else if (display == "eye")    paintEye (g, area, colour, bezel, lit);
    else                          paintDot (g, area, colour, lit);
}

//==============================================================================
const juce::StringArray& PanelParts::selectorDisplays()
{
    static const juce::StringArray ids { "cells", "pill", "keys", "lamps", "vfd", "tiles" };
    return ids;
}

const juce::StringArray& PanelParts::selectorDisplayNames()
{
    static const juce::StringArray names { "Buttons in a row", "Sliding pill", "Console keys", "Names with lamps", "Glowing display", "Colour tiles" };
    return names;
}

juce::String PanelParts::selectorDisplayOf (const GuiWidget& w)
{
    return pick (displayOf (w), selectorDisplays(), { { "buttons", "cells" }, { "row", "cells" }, { "standard", "cells" }, { "segmented", "cells" },
                                                      { "pills", "pill" }, { "capsule", "pill" }, { "slide", "pill" }, { "sliding", "pill" },
                                                      { "key", "keys" }, { "keycaps", "keys" }, { "console", "keys" }, { "consolekeys", "keys" },
                                                      { "pushbuttons", "keys" }, { "lit", "keys" }, { "lamp", "lamps" }, { "leds", "lamps" },
                                                      { "radio", "lamps" }, { "list", "lamps" }, { "display", "vfd" }, { "screen", "vfd" },
                                                      { "window", "vfd" }, { "stepper", "vfd" }, { "lcd", "vfd" }, { "tile", "tiles" },
                                                      { "colours", "tiles" }, { "colors", "tiles" }, { "swatches", "tiles" }, { "ramp", "tiles" } });
}

void PanelParts::paintSelector (juce::Graphics& g, juce::Rectangle<float> area, const GuiWidget& w, const juce::StringArray& names,
                                int chosen, juce::Colour accent, juce::Colour ink)
{
    if (area.isEmpty())
        return;

    const auto display = selectorDisplayOf (w);
    const int count = juce::jmax (1, names.size());
    const bool vertical = selectorVertical (display, area, count);
    auto nameOf = [&names] (int i) { return juce::isPositiveAndBelow (i, names.size()) ? names[i] : juce::String (i + 1); };

    if (display == "pill")
    {
        const auto track = area.reduced (1.0f);
        const auto radius = (vertical ? track.getWidth() : track.getHeight()) * 0.5f;
        g.setColour (colourSetting (w, "background", juce::Colour (0xc0101114)));
        g.fillRoundedRectangle (track, radius);

        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (track, radius);
            g.reduceClipRegion (clip);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), track.getX(), track.getY(),
                                                     juce::Colours::black.withAlpha (0.0f), track.getX(), track.getY() + juce::jmin (track.getHeight(), 12.0f), false));
            g.fillRect (track);
        }

        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.drawRoundedRectangle (track.reduced (0.5f), radius, 1.0f);

        if (juce::isPositiveAndBelow (chosen, count))
        {
            const auto pill = cellOf (track, vertical, chosen, count).reduced (2.5f);
            const auto r = juce::jmin (pill.getWidth(), pill.getHeight()) * 0.5f;
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (pill.translated (0.0f, 1.5f), r);
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.3f), pill.getX(), pill.getY(), accent.darker (0.25f), pill.getX(), pill.getBottom(), false));
            g.fillRoundedRectangle (pill, r);
            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.fillRoundedRectangle (pill.reduced (r * 0.6f, 0.0f).withHeight (pill.getHeight() * 0.4f).translated (0.0f, 1.0f), r * 0.6f);
        }

        for (int i = 0; i < count; ++i)
        {
            const auto cell = cellOf (track, vertical, i, count);
            const auto size = juce::jlimit (9.0f, 14.0f, (vertical ? cell.getHeight() : track.getHeight()) * 0.42f);
            drawName (g, nameOf (i), cell.reduced (radius * 0.35f, 0.0f), i == chosen ? inkOn (accent) : ink.withAlpha (0.72f),
                      FontLibrary::font (w.font, size, i == chosen || w.bold));
        }

        return;
    }

    if (display == "keys")
    {
        const float pitch = (vertical ? area.getHeight() : area.getWidth()) / (float) count;
        const float gap = juce::jlimit (2.0f, 6.0f, pitch * 0.08f);

        for (int i = 0; i < count; ++i)
        {
            const auto key = cellOf (area, vertical, i, count).reduced (vertical ? 1.0f : gap * 0.5f, vertical ? gap * 0.5f : 1.0f).withTrimmedBottom (1.5f);
            const bool on = i == chosen;
            const auto corner = juce::jmin (key.getWidth(), key.getHeight()) * 0.12f;

            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (key.translated (0.0f, 1.5f), corner);

            if (on)
            {
                g.setColour (accent.withAlpha (0.22f));
                g.fillRoundedRectangle (key.expanded (3.0f), corner + 3.0f);
            }

            g.setGradientFill (on ? juce::ColourGradient (accent.brighter (0.45f), key.getX(), key.getY(), accent.darker (0.1f), key.getX(), key.getBottom(), false)
                                  : juce::ColourGradient (juce::Colour (0xff46484e), key.getX(), key.getY(), juce::Colour (0xff25262a), key.getX(), key.getBottom(), false));
            g.fillRoundedRectangle (key, corner);

            const auto face = key.reduced (juce::jmax (1.5f, juce::jmin (key.getWidth(), key.getHeight()) * 0.08f));
            g.setGradientFill (juce::ColourGradient (on ? accent.brighter (0.8f).withAlpha (0.55f) : juce::Colours::white.withAlpha (0.1f), face.getX(), face.getY(),
                                                     juce::Colours::transparentWhite, face.getX(), face.getBottom(), false));
            g.fillRoundedRectangle (face, corner * 0.8f);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.5f : 0.16f));
            g.drawHorizontalLine (juce::roundToInt (key.getY() + 1.0f), key.getX() + corner, key.getRight() - corner);

            const auto size = juce::jlimit (9.0f, 14.0f, juce::jmin (key.getHeight() * 0.42f, key.getWidth() * 0.32f));
            drawName (g, nameOf (i), key.reduced (3.0f, 1.0f), on ? inkOn (accent) : ink.withAlpha (0.82f), FontLibrary::font (w.font, size, true));
        }

        return;
    }

    if (display == "lamps")
    {
        for (int i = 0; i < count; ++i)
        {
            const auto cell = cellOf (area, vertical, i, count);
            const bool on = i == chosen;
            const auto lampD = juce::jlimit (5.0f, 12.0f, (vertical ? cell.getHeight() : cell.getHeight() * 0.5f) * 0.45f);
            const auto lampAt = vertical ? juce::Point<float> (cell.getX() + lampD * 0.9f, cell.getCentreY())
                                         : juce::Point<float> (cell.getCentreX(), cell.getY() + lampD * 0.9f);
            const auto lamp = juce::Rectangle<float> (lampD, lampD).withCentre (lampAt);

            if (on)
            {
                g.setColour (accent.withAlpha (0.3f));
                g.fillEllipse (lamp.expanded (lampD * 0.45f));
            }

            g.setColour (on ? accent : accent.withMultipliedSaturation (0.7f).darker (2.2f));
            g.fillEllipse (lamp);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.18f));
            g.fillEllipse (juce::Rectangle<float> (lampD * 0.32f, lampD * 0.32f).withCentre (lampAt.translated (-lampD * 0.15f, -lampD * 0.15f)));
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (lamp, 0.8f);

            const auto size = juce::jlimit (9.0f, 14.0f, (vertical ? cell.getHeight() : cell.getHeight() * 0.5f) * 0.5f);
            const auto text = vertical ? cell.withTrimmedLeft (lampD * 2.1f) : cell.withTrimmedTop (lampD * 1.9f);
            drawName (g, nameOf (i), text, ink.withAlpha (on ? 1.0f : 0.68f), FontLibrary::font (w.font, size, on || w.bold),
                      vertical ? juce::Justification::centredLeft : juce::Justification::centred);
        }

        return;
    }

    if (display == "vfd")
    {
        const auto window = area.reduced (1.0f);
        const auto glow = w.colour.isTransparent() ? juce::Colour (0xff6af5e6) : accent;
        g.setColour (colourSetting (w, "background", juce::Colour (0xff060c0c)));
        g.fillRoundedRectangle (window, 4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), window.getX(), window.getY(),
                                                 juce::Colours::black.withAlpha (0.0f), window.getX(), window.getY() + juce::jmin (8.0f, window.getHeight() * 0.4f), false));
        g.fillRoundedRectangle (window, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (window.reduced (0.5f), 4.0f, 1.0f);

        // The arrows that step it, and the chosen name, glowing.
        const auto arrow = juce::jmin (window.getHeight() * 0.22f, 7.0f);
        juce::Path arrows;
        arrows.addTriangle (window.getX() + 7.0f, window.getCentreY(), window.getX() + 7.0f + arrow, window.getCentreY() - arrow, window.getX() + 7.0f + arrow, window.getCentreY() + arrow);
        arrows.addTriangle (window.getRight() - 7.0f, window.getCentreY(), window.getRight() - 7.0f - arrow, window.getCentreY() - arrow, window.getRight() - 7.0f - arrow, window.getCentreY() + arrow);
        g.setColour (glow.withAlpha (0.55f));
        g.fillPath (arrows);

        const auto text = juce::isPositiveAndBelow (chosen, count) ? nameOf (chosen) : juce::String ("-");
        const auto box = window.reduced (arrow + 12.0f, 1.0f).toNearestInt();
        g.setFont (FontLibrary::font (w.font.isNotEmpty() ? w.font : juce::String ("Share Tech Mono"), juce::jlimit (10.0f, 30.0f, window.getHeight() * 0.56f), w.bold));
        g.setColour (glow.withAlpha (0.16f));

        for (const auto& [dx, dy] : { std::pair<int, int> { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } })
            g.drawFittedText (text, box.translated (dx, dy), juce::Justification::centred, 1, 0.7f);

        g.setColour (glow.brighter (0.25f));
        g.drawFittedText (text, box, juce::Justification::centred, 1, 0.7f);

        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (window.withHeight (window.getHeight() * 0.45f), 4.0f);
        return;
    }

    if (display == "tiles")
    {
        for (int i = 0; i < count; ++i)
        {
            const auto tile = cellOf (area, vertical, i, count).reduced (2.0f);
            const bool on = i == chosen;
            const float t = count > 1 ? (float) i / (float) (count - 1) : 0.5f;
            auto hue = accent.getHue() + (t - 0.5f) * 0.3f;
            hue -= std::floor (hue);
            const auto colour = juce::Colour::fromHSV (hue, juce::jlimit (0.25f, 1.0f, accent.getSaturation()),
                                                      juce::jlimit (0.3f, 1.0f, accent.getBrightness() * (0.75f + 0.35f * t)), 1.0f);
            const auto corner = juce::jmin (tile.getWidth(), tile.getHeight()) * 0.12f;

            if (on)
            {
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillRoundedRectangle (tile.translated (0.0f, 2.0f), corner);
            }

            const auto face = on ? colour : colour.darker (0.45f).withAlpha (0.9f);
            g.setGradientFill (juce::ColourGradient (face.brighter (0.2f), tile.getX(), tile.getY(), face.darker (0.15f), tile.getX(), tile.getBottom(), false));
            g.fillRoundedRectangle (on ? tile.translated (0.0f, -1.0f) : tile, corner);

            if (on)
            {
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.drawRoundedRectangle (tile.translated (0.0f, -1.0f).reduced (0.75f), corner, 1.5f);
            }

            const auto size = juce::jlimit (9.0f, 14.0f, juce::jmin (tile.getHeight() * 0.42f, tile.getWidth() * 0.32f));
            drawName (g, nameOf (i), (on ? tile.translated (0.0f, -1.0f) : tile).reduced (3.0f, 1.0f), inkOn (face).withAlpha (on ? 1.0f : 0.8f),
                      FontLibrary::font (w.font, size, on || w.bold));
        }

        return;
    }

    // A row of buttons (the standard).
    for (int i = 0; i < count; ++i)
    {
        const auto cell = cellOf (area, false, i, count).reduced (1.0f);
        g.setColour (i == chosen ? accent : juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (cell, 3.0f);
        drawName (g, nameOf (i), cell, i == chosen ? juce::Colours::white : ink.withAlpha (0.8f), FontLibrary::font (w.font, 12.0f, i == chosen), juce::Justification::centred);
    }
}

int PanelParts::selectorPick (const GuiWidget& w, juce::Rectangle<float> area, int count, int chosen, juce::Point<float> at)
{
    count = juce::jmax (1, count);
    const auto display = selectorDisplayOf (w);

    if (display == "vfd")
    {
        // The left arrow steps back, anywhere else steps on (round past the ends).
        const auto x = (at.x - area.getX()) / juce::jmax (1.0f, area.getWidth());
        const auto from = juce::isPositiveAndBelow (chosen, count) ? chosen : 0;
        return x < 0.35f ? (from + count - 1) % count : (from + 1) % count;
    }

    const bool vertical = selectorVertical (display, area, count);
    const auto along = vertical ? (at.y - area.getY()) / juce::jmax (1.0f, area.getHeight()) : (at.x - area.getX()) / juce::jmax (1.0f, area.getWidth());
    return juce::jlimit (0, count - 1, (int) std::floor (along * (float) count));
}

//==============================================================================
const juce::StringArray& PanelParts::keyboardDisplays()
{
    static const juce::StringArray ids { "classic", "reverse", "modern" };
    return ids;
}

const juce::StringArray& PanelParts::keyboardDisplayNames()
{
    static const juce::StringArray names { "Classic ivory", "Reverse colours", "Modern slate" };
    return names;
}

juce::String PanelParts::keyboardDisplayOf (const GuiWidget& w)
{
    return pick (displayOf (w), keyboardDisplays(), { { "ivory", "classic" }, { "standard", "classic" }, { "piano", "classic" },
                                                      { "reversed", "reverse" }, { "inverted", "reverse" }, { "harpsichord", "reverse" },
                                                      { "black", "reverse" }, { "slate", "modern" }, { "flat", "modern" }, { "soft", "modern" },
                                                      { "dark", "modern" }, { "glow", "modern" } });
}

void PanelParts::paintKeyboard (juce::Graphics& g, const GuiWidget& w)
{
    const auto display = keyboardDisplayOf (w);

    if (display == "reverse")      paintReverseKeys (g, w);
    else if (display == "modern")  paintModernKeys (g, w);
    else                           paintClassicKeys (g, w);

    if (display != "classic")
        return;

    // The felt strip's shadow along the top.
    const auto b = w.bounds.toFloat();
    const auto shadow = b.withHeight (juce::jmax (4.0f, b.getHeight() * 0.08f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), shadow.getX(), shadow.getY(),
                                             juce::Colours::transparentBlack, shadow.getX(), shadow.getBottom(), false));
    g.fillRect (shadow);
}

void PanelParts::paintKeysDown (juce::Graphics& g, const GuiWidget& w, const std::function<bool (int)>& isDown, juce::Colour accent)
{
    const auto display = keyboardDisplayOf (w);
    const bool modern = display == "modern";
    bool whiteDown = false;

    for (int note = w.lowNote; note <= w.highNote; ++note)
    {
        if (stella::keys::isBlack (note) || ! isDown (note))
            continue;

        const auto key = keyArea (w, note).reduced (modern ? 1.0f : 0.5f, 0.0f);

        if (modern)
        {
            const auto r = juce::jmin (4.0f, key.getWidth() * 0.18f);
            juce::Path shape;
            shape.addRoundedRectangle (key.getX(), key.getY(), key.getWidth(), key.getHeight() - 1.0f, r, r, false, false, true, true);
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.55f), key.getX(), key.getY(), accent.withAlpha (0.95f), key.getX(), key.getBottom(), false));
            g.fillPath (shape);
            g.setColour (accent.brighter (0.6f));
            g.fillRect (key.withTop (key.getBottom() - juce::jmax (2.0f, key.getHeight() * 0.05f)).withTrimmedBottom (1.0f).reduced (r * 0.5f, 0.0f));
        }
        else
        {
            g.setColour (accent.withAlpha (display == "reverse" ? 0.65f : 0.55f));
            g.fillRect (key);
        }

        whiteDown = true;
    }

    // The black keys lie on top: as they were, over a white key that went down.
    if (whiteDown)
    {
        juce::RectangleList<int> blacks;

        for (int note = w.lowNote; note <= w.highNote; ++note)
            if (stella::keys::isBlack (note))
                blacks.addWithoutMerging (keyArea (w, note).getSmallestIntegerContainer());

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (blacks);
        paintKeyboard (g, w);
    }

    for (int note = w.lowNote; note <= w.highNote; ++note)
    {
        if (! stella::keys::isBlack (note) || ! isDown (note))
            continue;

        g.setColour (accent.withAlpha (modern ? 0.9f : 0.8f));

        if (modern)
            g.fillRoundedRectangle (keyArea (w, note).reduced (0.5f, 0.0f), 2.0f);
        else
            g.fillRect (keyArea (w, note));
    }
}

//==============================================================================
const juce::StringArray& PanelParts::labelDisplays()
{
    static const juce::StringArray ids { "plain", "readout", "vfd", "lcd" };
    return ids;
}

const juce::StringArray& PanelParts::labelDisplayNames()
{
    static const juce::StringArray names { "Plain words", "Value readout", "Glowing display (VFD)", "Backlit LCD" };
    return names;
}

juce::String PanelParts::labelDisplayOf (const GuiWidget& w)
{
    return pick (displayOf (w), labelDisplays(), { { "text", "plain" }, { "standard", "plain" }, { "value", "readout" }, { "frame", "readout" },
                                                   { "valueframe", "readout" }, { "numeric", "readout" }, { "number", "readout" },
                                                   { "counter", "readout" }, { "fluorescent", "vfd" }, { "glow", "vfd" }, { "neon", "vfd" },
                                                   { "screen", "vfd" }, { "segments", "lcd" }, { "segment", "lcd" }, { "14segment", "lcd" },
                                                   { "backlit", "lcd" }, { "display", "lcd" } });
}

bool PanelParts::labelMoves (const GuiWidget& w)
{
    return labelDisplayOf (w) == "readout" && w.param.isNotEmpty();
}

void PanelParts::paintLabel (juce::Graphics& g, const GuiWidget& w, const juce::String& text, juce::Colour ink, float size)
{
    const auto b = w.bounds.toFloat();
    const auto display = labelDisplayOf (w);
    const bool own = ! w.colour.isTransparent();

    if (display == "readout" || display == "vfd")
    {
        const bool vfd = display == "vfd";
        const auto frame = b.reduced (0.5f);
        const auto corner = juce::jmin (4.0f, frame.getHeight() * 0.25f);
        const auto colour = own ? w.colour : (vfd ? juce::Colour (0xff7df4ff) : juce::Colour (0xffffc46b));

        // A window sunk in the panel: its lower lip catches the light.
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (frame.translated (0.0f, 1.0f), corner);
        g.setColour (colourSetting (w, "background", vfd ? juce::Colour (0xff050b0d) : juce::Colour (0xff0e1012)));
        g.fillRoundedRectangle (frame, corner);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), frame.getX(), frame.getY(),
                                                 juce::Colours::black.withAlpha (0.0f), frame.getX(), frame.getY() + juce::jmin (7.0f, frame.getHeight() * 0.4f), false));
        g.fillRoundedRectangle (frame, corner);
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawRoundedRectangle (frame, corner, 1.0f);

        const auto font = FontLibrary::font (w.font.isNotEmpty() ? w.font : juce::String (vfd ? "Share Tech Mono" : "Roboto Mono"),
                                             size > 0.0f ? size : juce::jlimit (9.0f, 26.0f, frame.getHeight() * 0.56f), w.bold);
        const auto box = frame.reduced (4.0f, 1.0f).toNearestInt();
        g.setFont (font);

        if (vfd)
        {
            g.setColour (colour.withAlpha (0.16f));

            for (const auto& [dx, dy] : { std::pair<int, int> { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } })
                g.drawFittedText (text, box.translated (dx, dy), juce::Justification::centred, 1, 0.7f);
        }

        g.setColour (vfd ? colour.brighter (0.25f) : colour);
        g.drawFittedText (text, box, juce::Justification::centred, 1, 0.7f);

        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.fillRoundedRectangle (frame.withHeight (frame.getHeight() * 0.45f), corner);
        return;
    }

    if (display == "lcd")
    {
        const auto frame = b.reduced (0.5f);
        const auto corner = juce::jmin (3.0f, frame.getHeight() * 0.2f);
        const auto back = colourSetting (w, "background", juce::Colour (0xffa8bc6c));
        const auto colour = own ? w.colour : juce::Colour (0xff17210f);

        g.setColour (juce::Colour (0xff1a1c1e));
        g.fillRoundedRectangle (frame, corner + 1.0f);

        const auto glass = frame.reduced (juce::jmax (2.0f, frame.getHeight() * 0.1f));
        juce::ColourGradient light (back.brighter (0.12f), glass.getCentreX(), glass.getCentreY(), back.darker (0.25f), glass.getX(), glass.getY(), true);
        g.setGradientFill (light);
        g.fillRoundedRectangle (glass, corner);

        // The characters, centred, as many as fit.
        const auto upper = text.toUpperCase();
        const float cellH = glass.getHeight() * 0.6f;
        const float cellW = cellH * 0.55f, advance = cellW * 1.42f;
        const int fits = juce::jmax (1, (int) ((glass.getWidth() - cellW * 0.6f) / advance));

        // A dot, colon or comma sits in the gap after a character: it takes no cell of its own.
        int cells = 0, end = 0;

        for (; end < upper.length(); ++end)
        {
            const auto c = upper[end];

            if (c != '.' && c != ':' && c != ',')
            {
                if (cells == fits)
                    break;

                ++cells;
            }
        }

        const auto shown = upper.substring (0, end);
        const float total = (float) cells * advance - (advance - cellW);
        float x = glass.getCentreX() - total * 0.5f;
        const float y = glass.getCentreY() - cellH * 0.5f;

        for (int i = 0; i < shown.length(); ++i)
        {
            const auto c = shown[i];

            if (c == '.' || c == ':' || c == ',')
            {
                // A dot (or two) in the gap after the last character.
                const auto dot = juce::jmax (2.0f, cellW * 0.24f);
                const auto gapMiddle = x - (advance - cellW) * 0.5f;
                g.setColour (colour);
                g.fillEllipse (gapMiddle - dot * 0.5f, y + cellH - dot, dot, dot);

                if (c == ':')
                    g.fillEllipse (gapMiddle - dot * 0.5f + cellH * 0.05f, y + cellH * 0.3f, dot, dot);

                continue;
            }

            paintSegments (g, { x, y, cellW, cellH }, segmentsOf (c), colour, 0.09f);
            x += advance;
        }

        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (glass.withHeight (glass.getHeight() * 0.4f), corner);
        return;
    }

    // Plain words, with arrowheads beside them if it has them.
    const auto font = FontLibrary::font (w.font, size > 0.0f ? size : 14.0f, w.bold);
    const auto arrows = setting (w, "arrows").toLowerCase();
    auto area = b;

    if (arrows.isNotEmpty() && arrows != "none")
    {
        const auto arrow = juce::jlimit (3.0f, 9.0f, font.getHeight() * 0.32f);
        const auto textWidth = juce::jmin (b.getWidth(), juce::GlyphArrangement::getStringWidth (font, text));
        const bool left = arrows == "left" || arrows == "both", right = arrows == "right" || arrows == "both";
        const bool up = arrows == "up", down = arrows == "down";
        const auto cy = b.getCentreY();
        juce::Path heads;

        if (left || up || down)
            area.removeFromLeft (arrow * 2.0f + 5.0f);

        const auto startX = b.getX();

        if (left)
            heads.addTriangle (startX, cy, startX + arrow * 1.6f, cy - arrow, startX + arrow * 1.6f, cy + arrow);
        else if (up)
            heads.addTriangle (startX + arrow, cy - arrow, startX + arrow * 2.0f, cy + arrow * 0.7f, startX, cy + arrow * 0.7f);
        else if (down)
            heads.addTriangle (startX + arrow, cy + arrow, startX + arrow * 2.0f, cy - arrow * 0.7f, startX, cy - arrow * 0.7f);

        if (right)
        {
            const auto endX = juce::jmin (b.getRight() - arrow * 1.6f, area.getX() + textWidth + 5.0f);
            heads.addTriangle (endX + arrow * 1.6f, cy, endX, cy - arrow, endX, cy + arrow);
        }

        g.setColour (ink);
        g.fillPath (heads);
    }

    g.setColour (ink);
    g.setFont (font);
    g.drawText (text, area, juce::Justification::centredLeft, true);
}

//==============================================================================
const juce::StringArray& PanelParts::displaysFor (GuiWidget::Type type)
{
    static const juce::StringArray none;

    switch (type)
    {
        case GuiWidget::Type::lamp:      return lampDisplays();
        case GuiWidget::Type::selector:  return selectorDisplays();
        case GuiWidget::Type::keyboard:  return keyboardDisplays();
        case GuiWidget::Type::label:     return labelDisplays();
        default:                         return none;
    }
}

const juce::StringArray& PanelParts::displayNamesFor (GuiWidget::Type type)
{
    static const juce::StringArray none;

    switch (type)
    {
        case GuiWidget::Type::lamp:      return lampDisplayNames();
        case GuiWidget::Type::selector:  return selectorDisplayNames();
        case GuiWidget::Type::keyboard:  return keyboardDisplayNames();
        case GuiWidget::Type::label:     return labelDisplayNames();
        default:                         return none;
    }
}
