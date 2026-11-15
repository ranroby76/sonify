// D:\workspace\Fanan_web_hub\src\ModMatrix.cpp

#include "ModMatrix.h"

#include "FontLibrary.h"
#include "Preferences.h"

#include <cmath>

namespace
{
    constexpr float pi = 3.14159265358979f;

    float numberOf (const juce::var& v, const juce::Identifier& key, float fallback)
    {
        const auto value = v.getProperty (key, {});
        return value.isVoid() || (value.isString() && value.toString().trim().isEmpty()) ? fallback : (float) (double) value;
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

    juce::Font fontOf (const juce::var& d, float height, bool bold = false)
    {
        return FontLibrary::font (d.getProperty ("font", {}).toString(), height, bold);
    }

    /** The colour an amount shows in: the accent for a positive one, its opposite for a negative one. */
    juce::Colour amountColour (juce::Colour accent, float amount)
    {
        return amount >= 0.0f ? accent : accent.withRotatedHue (0.45f);
    }

    juce::Colour routeColour (const juce::var& d, int route)
    {
        static const juce::uint32 standard[] { 0xffe8473a, 0xfff2b33a, 0xff4fc35a, 0xff3a8ef2, 0xffb05ef2, 0xff3ad6d6, 0xfff27ab4, 0xffe8e8e8 };

        if (const auto* list = d.getProperty ("cables", {}).getArray(); list != nullptr && ! list->isEmpty())
        {
            auto text = (*list)[route % list->size()].toString().removeCharacters ("#");
            return juce::Colour ((juce::uint32) (text.length() == 6 ? "FF" + text : text).getHexValue64());
        }

        return juce::Colour (standard[route % 8]);
    }

    //==========================================================================
    // Where everything is, for the drawing and the clicks alike.
    struct Geometry
    {
        juce::Rectangle<float> area;

        // slots
        float headerH = 0, rowH = 0;
        int rows = 0;

        // grid
        float nameW = 0, topH = 0, cellW = 0, cellH = 0;

        // patch
        float jack = 0, outY = 0, inY = 0;

        juce::Rectangle<float> row (int i) const    { return { area.getX(), area.getY() + headerH + rowH * (float) i, area.getWidth(), rowH }; }

        // A slot's parts: its clear button, source, arrow, destination, amount.
        juce::Rectangle<float> part (int i, int which) const
        {
            auto r = row (i).reduced (3.0f, 2.0f);
            const auto clear = r.removeFromLeft (r.getHeight());
            r.removeFromLeft (4.0f);
            const float w = r.getWidth();
            const auto source = r.removeFromLeft (w * 0.31f);
            const auto arrow = r.removeFromLeft (w * 0.06f);
            const auto destination = r.removeFromLeft (w * 0.35f);
            r.removeFromLeft (6.0f);
            juce::Rectangle<float> parts[] { clear, source, arrow, destination, r };
            return parts[juce::jlimit (0, 4, which)];
        }

        juce::Rectangle<float> cell (int rowIndex, int column) const
        {
            return { area.getX() + nameW + cellW * (float) column, area.getY() + topH + cellH * (float) rowIndex, cellW, cellH };
        }

        juce::Point<float> output (int i, int count) const
        {
            return { area.getX() + area.getWidth() * ((float) i + 0.5f) / (float) juce::jmax (1, count), outY };
        }

        juce::Point<float> input (int i, int count) const
        {
            return { area.getX() + area.getWidth() * ((float) i + 0.5f) / (float) juce::jmax (1, count), inY };
        }
    };

    Geometry geometryOf (const juce::var& d, juce::Rectangle<float> area, const ModMatrix::View& view)
    {
        Geometry g;
        g.area = area.reduced (4.0f);
        const auto style = ModMatrix::styleOf (d);

        if (style == "grid")
        {
            const int rows = juce::jmax (1, view.sources.size()), columns = juce::jmax (1, (int) view.columns.size());
            g.nameW = juce::jlimit (40.0f, g.area.getWidth() * 0.4f, g.area.getWidth() * 0.26f);
            g.topH = juce::jlimit (30.0f, g.area.getHeight() * 0.45f, g.area.getHeight() * 0.28f);
            g.cellW = (g.area.getWidth() - g.nameW) / (float) columns;
            g.cellH = (g.area.getHeight() - g.topH) / (float) rows;
        }
        else if (style == "patch")
        {
            const int most = juce::jmax (view.sources.size(), (int) view.columns.size(), 1);
            g.jack = juce::jlimit (8.0f, 40.0f, juce::jmin (g.area.getWidth() / (float) most * 0.55f, g.area.getHeight() * 0.2f));
            g.outY = g.area.getY() + g.area.getHeight() * 0.3f;
            g.inY = g.area.getY() + g.area.getHeight() * 0.7f;
        }
        else
        {
            const int routes = ModMatrix::routesOf (d);
            g.headerH = (bool) d.getProperty ("header", true) ? juce::jlimit (12.0f, 22.0f, g.area.getHeight() * 0.09f) : 0.0f;
            g.rows = juce::jmax (1, juce::jmin (routes, (int) ((g.area.getHeight() - g.headerH) / 14.0f)));
            g.rowH = juce::jmin (34.0f, (g.area.getHeight() - g.headerH) / (float) g.rows);
        }

        return g;
    }

    /** A route of a source and a destination (its index; -1: none). */
    int routeOf (const ModMatrix::View& view, int source, int destination)
    {
        for (size_t i = 0; i < view.routes.size(); ++i)
            if (view.routes[i].source == source && view.routes[i].destination == destination)
                return (int) i;

        return -1;
    }

    //==========================================================================
    void paintJack (juce::Graphics& g, const juce::var& d, juce::Point<float> c, float size, juce::Colour accent, bool lit)
    {
        const auto kind = d.getProperty ("jack", "eurorack").toString();
        const auto r = juce::Rectangle<float> (size, size).withCentre (c);

        if (kind == "minimal")
        {
            g.setColour (lit ? accent : accent.withAlpha (0.45f));
            g.drawEllipse (r.reduced (size * 0.12f), juce::jmax (1.5f, size * 0.1f));
            g.fillEllipse (r.withSizeKeepingCentre (size * 0.3f, size * 0.3f));
            return;
        }

        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), juce::roundToInt (size * 0.15f), { 0, juce::roundToInt (size * 0.06f) + 1 })
            .drawForRectangle (g, r.toNearestInt());

        if (kind == "studio")
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e8ec), r.getX(), r.getY(), juce::Colour (0xff6c7078), r.getRight(), r.getBottom(), false));
            g.fillEllipse (r);
        }
        else
        {
            // A hex nut.
            juce::Path nut;
            nut.addPolygon (c, 6, size * 0.56f, pi / 6.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd6d9de), r.getX(), r.getY(), juce::Colour (0xff5d6168), r.getRight(), r.getBottom(), false));
            g.fillPath (nut);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.strokePath (nut, juce::PathStrokeType (1.0f));
        }

        // The hole, and a ring of light when it's in use.
        const auto hole = r.withSizeKeepingCentre (size * 0.48f, size * 0.48f);
        g.setColour (juce::Colour (0xff050506));
        g.fillEllipse (hole);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawEllipse (hole.translated (0.0f, 0.6f), 1.0f);

        if (lit)
        {
            g.setColour (accent.withAlpha (0.9f));
            g.drawEllipse (r.expanded (size * 0.1f), juce::jmax (1.2f, size * 0.07f));
        }
    }

    void drawCable (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, float thickness, float sag, juce::Colour colour)
    {
        const float length = from.getDistanceFrom (to);
        const float drop = length * sag * 0.45f + thickness * 2.0f;
        juce::Path cable;
        cable.startNewSubPath (from);
        cable.cubicTo (from.translated ((to.x - from.x) * 0.25f, drop), to.translated ((from.x - to.x) * 0.25f, drop), to);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (cable, juce::PathStrokeType (thickness * 1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (0.0f, thickness * 0.6f));
        g.setColour (colour.darker (0.25f));
        g.strokePath (cable, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour.brighter (0.4f).withAlpha (0.7f));
        g.strokePath (cable, juce::PathStrokeType (thickness * 0.28f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (-thickness * 0.15f, -thickness * 0.2f));

        // Its plugs.
        for (const auto& end : { from, to })
        {
            g.setColour (colour.darker (0.5f));
            g.fillEllipse (juce::Rectangle<float> (thickness * 1.9f, thickness * 1.9f).withCentre (end));
            g.setColour (juce::Colour (0xffc8ccd2));
            g.fillEllipse (juce::Rectangle<float> (thickness * 0.8f, thickness * 0.8f).withCentre (end));
        }
    }
}

//==============================================================================
static juce::String partStyle (const juce::var& d)
{
    return d.getProperty ("style", {}).toString().trim().toLowerCase();
}

bool ModMatrix::isPart (const juce::var& d)
{
    const auto style = partStyle (d);
    return style == "jack" || style == "chip" || style == "assign";
}

bool ModMatrix::isPart (const GuiWidget& w)
{
    return w.type == GuiWidget::Type::matrix && isPart (designOf (w));
}

juce::String ModMatrix::roleOf (const juce::var& d)
{
    return partStyle (d) == "jack" && d.getProperty ("role", "out").toString().trim().equalsIgnoreCase ("in")
               ? juce::String ("in") : juce::String ("out");
}

int ModMatrix::sourceOf (const juce::var& d)
{
    return juce::jmax (0, (int) numberOf (d, "source", 1.0f));
}

juce::String ModMatrix::targetOf (const juce::var& d)
{
    return d.getProperty ("target", {}).toString().trim();
}

juce::Point<float> ModMatrix::partPoint (const juce::var& d, juce::Rectangle<float> area)
{
    if (partStyle (d) != "jack")
        return area.getCentre();

    // The jack in the middle, its name over (an output) or under (an input) it.
    const float size = juce::jmin (area.getWidth(), area.getHeight() * 0.62f);
    return { area.getCentreX(), roleOf (d) == "out" ? area.getBottom() - size * 0.55f : area.getY() + size * 0.55f };
}

void ModMatrix::paintPart (juce::Graphics& g, const juce::var& d, juce::Rectangle<float> area, const juce::String& label, bool lit)
{
    const auto style = partStyle (d);
    const auto accent = colourOf (d, "colour", juce::Colour (0xff2fb36b));
    const auto text = colourOf (d, "textColour", juce::Colour (0xffd8dadf));
    const float size = juce::jlimit (20.0f, 200.0f, numberOf (d, "textSize", 100.0f)) * 0.01f;

    if (style == "jack")
    {
        const float jack = juce::jmin (area.getWidth(), area.getHeight() * 0.62f);
        const auto c = partPoint (d, area);
        paintJack (g, d, c, jack * 0.9f, accent, lit);
        const auto labelBox = roleOf (d) == "out" ? area.withBottom (c.y - jack * 0.5f) : area.withTop (c.y + jack * 0.5f);
        g.setColour (text);
        g.setFont (fontOf (d, juce::jlimit (8.0f, 14.0f, labelBox.getHeight() * 0.8f) * size, true));
        g.drawFittedText (label, labelBox.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        return;
    }

    // A chip, an assign button: a pill with its source's name.
    const auto pill = area.reduced (1.0f);
    const float radius = pill.getHeight() * 0.5f;
    const bool assign = style == "assign";
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (pill.translated (0.0f, 1.5f), radius);
    g.setGradientFill (juce::ColourGradient (colourOf (d, "panelColour", juce::Colour (0xff24262b)).brighter (lit ? 0.0f : 0.12f), 0, pill.getY(),
                                             colourOf (d, "panelColour", juce::Colour (0xff24262b)).darker (0.2f), 0, pill.getBottom(), false));
    g.fillRoundedRectangle (pill, radius);

    if (lit)
    {
        g.setColour (accent.withAlpha (assign ? 0.85f : 0.3f));
        g.fillRoundedRectangle (pill, radius);
    }

    g.setColour (accent.withAlpha (lit ? 1.0f : 0.6f));
    g.drawRoundedRectangle (pill.reduced (0.5f), radius, 1.2f);

    // A chip's grip: a dot to drag.
    auto inside = pill.reduced (radius * 0.6f, 0.0f);

    if (! assign)
    {
        const auto dot = inside.removeFromLeft (pill.getHeight() * 0.7f);
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (pill.getHeight() * 0.36f, pill.getHeight() * 0.36f).withCentre (dot.getCentre()));
        g.setColour (accent.withAlpha (0.35f));
        g.drawEllipse (juce::Rectangle<float> (pill.getHeight() * 0.6f, pill.getHeight() * 0.6f).withCentre (dot.getCentre()), 1.0f);
    }

    g.setColour (assign && lit ? juce::Colours::black.withAlpha (0.85f) : text);
    g.setFont (fontOf (d, juce::jlimit (8.0f, 15.0f, pill.getHeight() * 0.48f) * size, true));
    g.drawFittedText (assign ? "ASSIGN " + label.toUpperCase() : label, inside.toNearestInt(), juce::Justification::centred, 1, 0.6f);
}

juce::Point<float> ModMatrix::outputPoint (const juce::var& d, juce::Rectangle<float> area, const View& view, int source)
{
    return geometryOf (d, area, view).output (source, view.sources.size());
}

juce::Point<float> ModMatrix::inputPoint (const juce::var& d, juce::Rectangle<float> area, const View& view, int column)
{
    return geometryOf (d, area, view).input (column, (int) view.columns.size());
}

float ModMatrix::jackSize (const juce::var& d, juce::Rectangle<float> area, const View& view)
{
    return geometryOf (d, area, view).jack;
}

void ModMatrix::paintJackAt (juce::Graphics& g, const juce::var& d, juce::Point<float> centre, float size, bool lit)
{
    paintJack (g, d, centre, size, colourOf (d, "colour", juce::Colour (0xff2fb36b)), lit);
}

void ModMatrix::paintCable (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, float thickness, float sag, juce::Colour colour)
{
    drawCable (g, from, to, thickness, sag, colour);
}

juce::Colour ModMatrix::cableColour (const juce::var& d, int route)
{
    return routeColour (d, route);
}

void ModMatrix::paintRing (juce::Graphics& g, juce::Rectangle<float> box, bool knob, bool vertical, float value, float amount, juce::Colour colour, int ringIndex)
{
    const float from = juce::jlimit (0.0f, 1.0f, value), to = juce::jlimit (0.0f, 1.0f, value + amount);

    if (knob)
    {
        // An arc round the knob, in its turn (-135..135 degrees), a ring a modulation.
        const float r = juce::jmin (box.getWidth(), box.getHeight()) * 0.5f + 3.0f + (float) ringIndex * 4.0f;
        const float a0 = (-0.75f + 1.5f * from) * pi, a1 = (-0.75f + 1.5f * to) * pi;
        juce::Path track, arc;
        track.addCentredArc (box.getCentreX(), box.getCentreY(), r, r, 0.0f, -0.75f * pi, 0.75f * pi, true);
        arc.addCentredArc (box.getCentreX(), box.getCentreY(), r, r, 0.0f, a0, a1, true);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre ({ box.getCentreX() + r * std::sin (a1), box.getCentreY() - r * std::cos (a1) }));
        return;
    }

    // A bar beside a slider's track.
    const float offset = 4.0f + (float) ringIndex * 4.0f;

    if (vertical)
    {
        const float x = box.getRight() + offset, y0 = box.getBottom() - box.getHeight() * from, y1 = box.getBottom() - box.getHeight() * to;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (x, box.getY(), 2.5f, box.getHeight()));
        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (x, juce::jmin (y0, y1), 2.5f, std::abs (y1 - y0)));
    }
    else
    {
        const float y = box.getBottom() + offset, x0 = box.getX() + box.getWidth() * from, x1 = box.getX() + box.getWidth() * to;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (box.getX(), y, box.getWidth(), 2.5f));
        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> (juce::jmin (x0, x1), y, std::abs (x1 - x0), 2.5f));
    }
}

//==============================================================================
juce::var ModMatrix::designOf (const GuiWidget& w)
{
    const auto found = w.settings.find ("matrix");
    return found != w.settings.end() && found->second.isObject() ? found->second : makeTemplate (0);
}

juce::String ModMatrix::styleOf (const juce::var& d)
{
    const auto style = d.getProperty ("style", "slots").toString().trim().toLowerCase();
    return style == "grid" || style == "pins" ? juce::String ("grid") : style == "patch" || style == "cables" ? juce::String ("patch") : juce::String ("slots");
}

int ModMatrix::routesOf (const juce::var& d)
{
    return juce::jlimit (1, 64, (int) numberOf (d, "routes", 8.0f));
}

juce::StringArray ModMatrix::sourcesOf (const juce::var& d)
{
    juce::StringArray list;

    if (const auto* sources = d.getProperty ("sources", {}).getArray())
        for (const auto& s : *sources)
            if (s.toString().trim().isNotEmpty())
                list.add (s.toString().trim());

    if (list.isEmpty())
        list = { "LFO 1", "LFO 2", "Env 1", "Env 2", "Velocity", "Mod wheel", "Aftertouch", "Key" };

    return list;
}

juce::String ModMatrix::paramId (const GuiWidget& w, int route, const char* part)
{
    return (w.param.isNotEmpty() ? w.param : juce::String ("mod")) + "_" + part + juce::String (route + 1);
}

bool ModMatrix::isMatrixParam (const GuiLayout& layout, const juce::String& id)
{
    for (const auto& w : layout.widgets)
    {
        if (w.type != GuiWidget::Type::matrix)
            continue;

        const auto base = (w.param.isNotEmpty() ? w.param : juce::String ("mod")) + "_";

        if (id.startsWith (base + "src") || id.startsWith (base + "dst") || id.startsWith (base + "amt"))
            return true;
    }

    return false;
}

//==============================================================================
void ModMatrix::paint (juce::Graphics& g, const juce::var& d, juce::Rectangle<float> area, const View& view)
{
    const auto style = styleOf (d);
    const auto geo = geometryOf (d, area, view);
    const auto accent = colourOf (d, "colour", juce::Colour (0xff2fb36b));
    const auto panel = colourOf (d, "panelColour", juce::Colour (0xff141518));
    const auto text = colourOf (d, "textColour", juce::Colour (0xffd8dadf));
    const auto line = colourOf (d, "lineColour", juce::Colour (0xff2c2e33));
    const float size = juce::jlimit (20.0f, 200.0f, numberOf (d, "textSize", 100.0f)) * 0.01f;

    // No background of its own: it lies on whatever's under it (the panel, a plate, a picture).

    if (style == "slots")
    {
        // A header, then a row a route: clear, source, arrow, destination, amount.
        g.setFont (fontOf (d, juce::jmin (geo.rowH * 0.45f, 13.0f) * size, true));
        g.setColour (text.withAlpha (0.55f));

        if (geo.headerH > 0.0f)
        {
            const auto h = juce::Rectangle<float> (geo.area.getX(), geo.area.getY(), geo.area.getWidth(), geo.headerH);
            g.drawText ("SOURCE", geo.part (0, 1).withY (h.getY()).withHeight (h.getHeight()), juce::Justification::centredLeft, true);
            g.drawText ("DESTINATION", geo.part (0, 3).withY (h.getY()).withHeight (h.getHeight()), juce::Justification::centredLeft, true);
            g.drawText ("AMOUNT", geo.part (0, 4).withY (h.getY()).withHeight (h.getHeight()), juce::Justification::centred, true);
        }

        for (int i = 0; i < geo.rows && i < (int) view.routes.size(); ++i)
        {
            const auto& route = view.routes[(size_t) i];
            const auto r = geo.row (i);

            if (i % 2 == 1)
            {
                g.setColour (juce::Colours::white.withAlpha (0.025f));
                g.fillRect (r);
            }

            g.setColour (line);
            g.drawHorizontalLine (juce::roundToInt (r.getY()), r.getX() + 4.0f, r.getRight() - 4.0f);

            // Its clear button: lit while it routes.
            const auto clear = geo.part (i, 0).reduced (geo.rowH * 0.18f);
            g.setColour (route.active() ? accent : line.brighter (0.3f));
            g.fillEllipse (clear);

            if (route.active())
            {
                g.setColour (panel);
                const auto x = clear.reduced (clear.getWidth() * 0.3f);
                g.drawLine ({ x.getTopLeft(), x.getBottomRight() }, 1.4f);
                g.drawLine ({ x.getTopRight(), x.getBottomLeft() }, 1.4f);
            }

            auto boxed = [&] (juce::Rectangle<float> box, const juce::String& name, bool set)
            {
                g.setColour (juce::Colours::black.withAlpha (0.3f));
                g.fillRoundedRectangle (box, 3.0f);
                g.setColour (set ? text : text.withAlpha (0.35f));
                g.setFont (fontOf (d, juce::jmin (geo.rowH * 0.5f, 14.0f) * size));
                g.drawText (set ? name : juce::String ("none"), box.reduced (5.0f, 0.0f).withTrimmedRight (10.0f), juce::Justification::centredLeft, true);
                juce::Path down;
                const auto c = juce::Point<float> (box.getRight() - 7.0f, box.getCentreY());
                down.addTriangle (c.translated (-3.0f, -1.5f), c.translated (3.0f, -1.5f), c.translated (0.0f, 2.0f));
                g.fillPath (down);
            };

            boxed (geo.part (i, 1), juce::isPositiveAndBelow (route.source - 1, view.sources.size()) ? view.sources[route.source - 1] : juce::String(),
                   route.source > 0);
            g.setColour (text.withAlpha (0.5f));
            g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92"), geo.part (i, 2), juce::Justification::centred, false);
            boxed (geo.part (i, 3), juce::isPositiveAndBelow (route.destination - 1, view.destinations.size()) ? view.destinations[route.destination - 1] : juce::String(),
                   route.destination > 0);

            // Its amount: from the middle, either way.
            const auto bar = geo.part (i, 4).reduced (0.0f, geo.rowH * 0.22f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (bar, 2.0f);
            const float mid = bar.getCentreX(), to = mid + bar.getWidth() * 0.5f * juce::jlimit (-1.0f, 1.0f, route.amount);
            g.setColour (amountColour (accent, route.amount).withAlpha (route.active() ? 0.9f : 0.35f));
            g.fillRect (juce::Rectangle<float> (juce::jmin (mid, to), bar.getY(), std::abs (to - mid), bar.getHeight()));
            g.setColour (text.withAlpha (0.4f));
            g.drawVerticalLine (juce::roundToInt (mid), bar.getY(), bar.getBottom());
            g.setColour (text);
            g.setFont (fontOf (d, juce::jmin (geo.rowH * 0.42f, 12.0f) * size, true));
            g.drawText ((route.amount > 0.0f ? "+" : "") + juce::String (juce::roundToInt (route.amount * 100.0f)) + "%", bar, juce::Justification::centred, false);
        }

        if (geo.rows < routesOf (d))
        {
            g.setColour (text.withAlpha (0.5f));
            g.setFont (fontOf (d, 10.0f));
            g.drawText ("+" + juce::String (routesOf (d) - geo.rows) + " more (taller to show them)", geo.area.withTop (geo.area.getBottom() - 12.0f),
                        juce::Justification::centredRight, false);
        }

        return;
    }

    if (style == "grid")
    {
        const auto cellKind = d.getProperty ("cell", "pin").toString();
        const int rows = view.sources.size(), columns = (int) view.columns.size();

        // The sources down the side, the destinations up the top.
        g.setColour (text);
        g.setFont (fontOf (d, juce::jmin (geo.cellH * 0.55f, 13.0f) * size));

        for (int r = 0; r < rows; ++r)
            g.drawText (view.sources[r], juce::Rectangle<float> (geo.area.getX(), geo.cell (r, 0).getY(), geo.nameW - 6.0f, geo.cellH),
                        juce::Justification::centredRight, true);

        for (int c = 0; c < columns; ++c)
        {
            const auto name = juce::isPositiveAndBelow (view.columns[(size_t) c] - 1, view.destinations.size()) ? view.destinations[view.columns[(size_t) c] - 1] : juce::String();
            const auto box = juce::Rectangle<float> (geo.cell (0, c).getX(), geo.area.getY(), geo.cellW, geo.topH - 4.0f);
            juce::Graphics::ScopedSaveState state (g);
            g.addTransform (juce::AffineTransform::rotation (-pi * 0.5f, box.getCentreX(), box.getCentreY()));
            g.drawText (name, juce::Rectangle<float> (box.getHeight(), box.getWidth()).withCentre (box.getCentre()), juce::Justification::centredLeft, true);
        }

        // The grid, and where a source meets a destination.
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < columns; ++c)
            {
                const auto cell = geo.cell (r, c);
                g.setColour (line);
                g.drawRect (cell, 0.5f);

                const int route = routeOf (view, r + 1, view.columns[(size_t) c]);
                const float m = juce::jmin (cell.getWidth(), cell.getHeight());
                const auto dot = juce::Rectangle<float> (m * 0.5f, m * 0.5f).withCentre (cell.getCentre());

                if (route < 0)
                {
                    g.setColour (line.brighter (0.25f));
                    g.fillEllipse (dot.reduced (m * 0.12f));
                    continue;
                }

                const float amount = view.routes[(size_t) route].amount;
                const auto colour = amountColour (accent, amount);

                if (cellKind == "bar")
                {
                    const auto bar = cell.reduced (m * 0.18f);
                    const float mid = bar.getCentreY(), to = mid - bar.getHeight() * 0.5f * amount;
                    g.setColour (colour);
                    g.fillRect (juce::Rectangle<float> (bar.getX(), juce::jmin (mid, to), bar.getWidth(), std::abs (to - mid)));
                    continue;
                }

                if (cellKind == "knob")
                {
                    const auto knob = cell.withSizeKeepingCentre (m * 0.72f, m * 0.72f);
                    g.setColour (juce::Colour (0xff2a2c31));
                    g.fillEllipse (knob);
                    juce::Path arc;
                    arc.addCentredArc (knob.getCentreX(), knob.getCentreY(), knob.getWidth() * 0.5f, knob.getWidth() * 0.5f, 0.0f, 0.0f, amount * pi * 0.75f, true);
                    g.setColour (colour);
                    g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.5f, m * 0.08f)));
                    const float a = amount * pi * 0.75f;
                    g.drawLine ({ knob.getCentre(), knob.getCentre() + juce::Point<float> (std::sin (a), -std::cos (a)) * knob.getWidth() * 0.4f }, 1.5f);
                    continue;
                }

                // A pin, its ring the amount.
                g.setColour (colour);
                g.fillEllipse (dot);
                g.setColour (juce::Colours::white.withAlpha (0.45f));
                g.fillEllipse (dot.reduced (dot.getWidth() * 0.32f).translated (-dot.getWidth() * 0.08f, -dot.getWidth() * 0.08f));
                juce::Path ring;
                const float rr = m * 0.36f;
                ring.addCentredArc (cell.getCentreX(), cell.getCentreY(), rr, rr, 0.0f, 0.0f, amount * pi * 2.0f * 0.98f, true);
                g.setColour (colour.withAlpha (0.8f));
                g.strokePath (ring, juce::PathStrokeType (juce::jmax (1.0f, m * 0.05f)));
            }
        }

        return;
    }

    // The patch bay: the outputs up top, the inputs below, the cables between them.
    const int outs = view.sources.size(), ins = (int) view.columns.size();
    g.setColour (text);
    g.setFont (fontOf (d, juce::jmin (12.0f, geo.jack * 0.45f) * size));

    for (int i = 0; i < outs; ++i)
    {
        const auto c = geo.output (i, outs);
        bool used = false;

        for (const auto& r : view.routes)
            used = used || (r.active() && r.source == i + 1);

        paintJack (g, d, c, geo.jack, accent, used || view.draggingFrom == i);
        g.setColour (text);
        g.drawFittedText (view.sources[i], juce::Rectangle<float> (geo.area.getWidth() / (float) juce::jmax (1, outs), geo.jack * 0.7f)
                                               .withCentre (c.translated (0.0f, -geo.jack * 0.98f)).toNearestInt(), juce::Justification::centred, 2, 0.6f);
    }

    for (int i = 0; i < ins; ++i)
    {
        const auto c = geo.input (i, ins);
        const int dst = view.columns[(size_t) i];
        float amount = 0;
        bool used = false;

        for (const auto& r : view.routes)
            if (r.active() && r.destination == dst)
            {
                used = true;
                amount = r.amount;
            }

        paintJack (g, d, c, geo.jack, accent, used);

        if (used)
        {
            juce::Path ring;
            ring.addCentredArc (c.x, c.y, geo.jack * 0.78f, geo.jack * 0.78f, 0.0f, 0.0f, amount * pi * 0.95f, true);
            g.setColour (amountColour (accent, amount));
            g.strokePath (ring, juce::PathStrokeType (juce::jmax (1.5f, geo.jack * 0.08f)));
        }

        const auto name = juce::isPositiveAndBelow (dst - 1, view.destinations.size()) ? view.destinations[dst - 1] : juce::String();
        g.setColour (text);
        g.drawFittedText (name, juce::Rectangle<float> (geo.area.getWidth() / (float) juce::jmax (1, ins), geo.jack * 0.7f)
                                    .withCentre (c.translated (0.0f, geo.jack * 0.95f)).toNearestInt(), juce::Justification::centred, 2, 0.6f);
    }

    // The cables, over everything.
    const float sag = juce::jlimit (0.0f, 100.0f, numberOf (d, "sag", 45.0f)) * 0.01f;

    for (size_t r = 0; r < view.routes.size(); ++r)
    {
        const auto& route = view.routes[r];

        if (! route.active() || route.source > outs)
            continue;

        for (int i = 0; i < ins; ++i)
            if (view.columns[(size_t) i] == route.destination)
                drawCable (g, geo.output (route.source - 1, outs), geo.input (i, ins), geo.jack * 0.3f, sag,
                            routeColour (d, (int) r).withMultipliedAlpha ((int) r == view.hoverRoute ? 1.0f : 0.92f));
    }

    if (juce::isPositiveAndBelow (view.draggingFrom, outs))
        drawCable (g, geo.output (view.draggingFrom, outs), view.draggingTo, geo.jack * 0.3f, sag, routeColour (d, 0).withAlpha (0.8f));
}

ModMatrix::Hit ModMatrix::hitTest (const juce::var& d, juce::Rectangle<float> area, const View& view, juce::Point<float> at)
{
    Hit hit;
    const auto style = styleOf (d);
    const auto geo = geometryOf (d, area, view);

    if (style == "slots")
    {
        for (int i = 0; i < geo.rows; ++i)
        {
            if (! geo.row (i).contains (at))
                continue;

            hit.route = i;

            for (const auto& [which, kind] : { std::pair<int, Hit::Kind> { 0, Hit::Kind::clear }, { 1, Hit::Kind::source }, { 3, Hit::Kind::destination },
                                               { 4, Hit::Kind::amount } })
                if (geo.part (i, which).contains (at))
                    hit.kind = kind;

            return hit;
        }

        return hit;
    }

    if (style == "grid")
    {
        for (int r = 0; r < view.sources.size(); ++r)
            for (int c = 0; c < (int) view.columns.size(); ++c)
                if (geo.cell (r, c).contains (at))
                {
                    hit.kind = Hit::Kind::cell;
                    hit.row = r;
                    hit.column = c;
                    hit.route = routeOf (view, r + 1, view.columns[(size_t) c]);
                    return hit;
                }

        return hit;
    }

    const int outs = view.sources.size(), ins = (int) view.columns.size();

    for (int i = 0; i < outs; ++i)
        if (geo.output (i, outs).getDistanceFrom (at) < geo.jack * 0.7f)
        {
            hit.kind = Hit::Kind::output;
            hit.row = i;
            return hit;
        }

    for (int i = 0; i < ins; ++i)
        if (geo.input (i, ins).getDistanceFrom (at) < geo.jack * 0.7f)
        {
            hit.kind = Hit::Kind::input;
            hit.column = i;

            for (int r = (int) view.routes.size(); --r >= 0;)
                if (view.routes[(size_t) r].active() && view.routes[(size_t) r].destination == view.columns[(size_t) i])
                {
                    hit.route = r;
                    break;
                }

            return hit;
        }

    return hit;
}

int ModMatrix::inputNear (const juce::var& d, juce::Rectangle<float> area, const View& view, juce::Point<float> at)
{
    const auto geo = geometryOf (d, area, view);
    const int ins = (int) view.columns.size();
    int best = -1;
    float bestDistance = geo.jack * 1.6f;

    for (int i = 0; i < ins; ++i)
        if (const float distance = geo.input (i, ins).getDistanceFrom (at); distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }

    return best;
}

//==============================================================================
namespace
{
    struct Template
    {
        const char* name;
        const char* json;
    };

    const Template templates[] {
        { "Slots (dark)", R"({ "style": "slots", "routes": 8, "colour": "#FF2FB36B", "panelColour": "#FF141518", "textColour": "#FFD8DADF", "lineColour": "#FF2A2C31" })" },
        { "Slots (light)", R"({ "style": "slots", "routes": 6, "colour": "#FF2F7BFF", "panelColour": "#FFE9EAEC", "textColour": "#FF1E2024", "lineColour": "#FFC9CCD1" })" },
        { "Slots (16, compact)", R"({ "style": "slots", "routes": 16, "header": false, "colour": "#FFFF8A2A", "panelColour": "#FF101113", "textColour": "#FFE0E2E6", "lineColour": "#FF24262A", "textSize": 85 })" },
        { "Pin grid (VCS3 style)", R"({ "style": "grid", "routes": 16, "cell": "pin", "colour": "#FFE84A3A", "panelColour": "#FFE4E0D6", "textColour": "#FF2A2622", "lineColour": "#FFBDB6A8" })" },
        { "Knob grid", R"({ "style": "grid", "routes": 16, "cell": "knob", "colour": "#FF5FE0E8", "panelColour": "#FF121316", "textColour": "#FFCDD0D6", "lineColour": "#FF26282D" })" },
        { "Bar grid", R"({ "style": "grid", "routes": 24, "cell": "bar", "colour": "#FFB05EF2", "panelColour": "#FF141518", "textColour": "#FFCDD0D6", "lineColour": "#FF26282D" })" },
        { "Eurorack patch bay", R"({ "style": "patch", "matchInputs": true, "routes": 8, "jack": "eurorack", "colour": "#FF4FC35A", "panelColour": "#FF1C1D20", "textColour": "#FFE6E6E6", "lineColour": "#FF2E3035", "sag": 45 })" },
        { "Studio patch bay", R"({ "style": "patch", "matchInputs": true, "routes": 8, "jack": "studio", "colour": "#FFF2B33A", "panelColour": "#FF2A2C30", "textColour": "#FFE6E6E6", "lineColour": "#FF3A3C41", "sag": 35,
             "cables": [ "#FF1C1C1E", "#FFD8322A", "#FF2F7BFF", "#FFF2B33A" ] })" },
        { "Jack (output)", R"({ "style": "jack", "role": "out", "source": 1, "jack": "eurorack", "colour": "#FF4FC35A", "textColour": "#FFE6E6E6" })" },
        { "Jack (input)", R"({ "style": "jack", "role": "in", "jack": "eurorack", "colour": "#FF4FC35A", "textColour": "#FFE6E6E6" })" },
        { "Source chip (drag onto a knob)", R"({ "style": "chip", "source": 1, "colour": "#FF5FE0E8", "panelColour": "#FF1C1E22", "textColour": "#FFE6E6E6" })" },
        { "Assign button", R"({ "style": "assign", "source": 1, "colour": "#FFFF8A2A", "panelColour": "#FF1C1E22", "textColour": "#FFE6E6E6" })" },
        { "Minimal patch bay", R"({ "style": "patch", "matchInputs": true, "routes": 6, "jack": "minimal", "colour": "#FF5FE0E8", "panelColour": "#FF0E1013", "textColour": "#FFB8C4CC", "lineColour": "#FF1E2328", "sag": 25,
             "cables": [ "#FF5FE0E8", "#FFFF7A3A", "#FFB05EF2" ] })" },
    };

    constexpr int templateCount = (int) (sizeof (templates) / sizeof (templates[0]));
}

juce::StringArray ModMatrix::templateNames()
{
    juce::StringArray names;

    for (const auto& t : templates)
        names.add (t.name);

    return names;
}

juce::var ModMatrix::makeTemplate (int index)
{
    return juce::JSON::parse (juce::String (templates[juce::jlimit (0, templateCount - 1, index)].json));
}

juce::File ModMatrix::galleryFolder()
{
    auto folder = SettingsFolder::folder ("Matrix Gallery");
    folder.createDirectory();
    return folder;
}
