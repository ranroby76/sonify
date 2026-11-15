// D:\workspace\Fanan_web_hub\src\PresetPanel.cpp

#include "PresetPanel.h"

#include "FontLibrary.h"
#include "Preferences.h"
#include "SegmentDisplay.h"
#include "SelectorSet.h"

#include <cmath>

namespace
{
    /** Every preset manager has its commands (Save, Save as, Load; More: the banks, Delete,
        the folder): a design saying "actions": false still has them. */
    bool hasActions (const juce::var&)    { return true; }
}

namespace
{
    constexpr float pi = 3.14159265358979f;

    float numberOf (const juce::var& v, const juce::Identifier& key, float fallback)
    {
        const auto value = v.getProperty (key, {});
        return value.isVoid() || value.toString().trim().isEmpty() ? fallback : (float) (double) value;
    }

    juce::String textOf (const juce::var& v, const juce::Identifier& key, const juce::String& fallback)
    {
        const auto text = v.getProperty (key, {}).toString().trim();
        return text.isEmpty() ? fallback : text.toLowerCase();
    }

    bool boolOf (const juce::var& v, const juce::Identifier& key, bool fallback)
    {
        const auto value = v.getProperty (key, {});
        return value.isVoid() ? fallback : (bool) value;
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

    void put (const juce::var& v, const juce::Identifier& key, const juce::var& value)
    {
        if (auto* o = v.getDynamicObject())
            o->setProperty (key, value);
    }

    juce::Colour accentOf (const juce::var& d)    { return colourOf (d, "colour", juce::Colour (0xff2fb36b)); }
    juce::Colour panelOf (const juce::var& d)     { return colourOf (d, "panelColour", juce::Colour (0xff17191c)); }
    juce::Colour textColourOf (const juce::var& d) { return colourOf (d, "textColour", juce::Colour (0xffd8dadf)); }
    juce::Colour lineOf (const juce::var& d)      { return colourOf (d, "lineColour", juce::Colour (0xff2c2f35)); }

    juce::Font fontOf (const juce::var& d, float height, bool bold = false)
    {
        const float size = juce::jlimit (40.0f, 200.0f, numberOf (d, "textSize", 100.0f)) * 0.01f;
        return FontLibrary::font (d.getProperty ("font", {}).toString(), juce::jmax (6.0f, height * size), bold);
    }

    juce::String categoriesOf (const juce::var& d)
    {
        const auto style = PresetPanel::styleOf (d);
        return textOf (d, "categories", style == "side" ? "tabs" : style == "list" ? "headings" : "none");
    }

    /** Its buttons' design (a SelectorSet's), set out as asked. */
    juce::var keysOf (const juce::var& d, const juce::String& formation, int columns, const juce::String& labels)
    {
        auto keys = d.getProperty ("keys", {}).isObject() ? d.getProperty ("keys", {}).clone() : juce::var (new juce::DynamicObject());

        auto standard = [&keys] (const char* key, const juce::var& value)
        {
            if (! keys.hasProperty (key))
                put (keys, key, value);
        };

        standard ("render", "2d");
        standard ("style", "soft");
        standard ("shape", "rect");
        standard ("rounding", 22);
        standard ("gap", 12);
        standard ("capColour", "#FF24262B");
        standard ("capOnColour", "#FF27392F");
        standard ("glowColour", "#FF2FB36B");
        standard ("glow", 35);
        standard ("labelColour", "#FFC8CBD0");
        standard ("labelOnColour", "#FFFFFFFF");
        standard ("textSize", 60);
        standard ("material", "Satin plastic");
        standard ("height", 14);

        put (keys, "formation", formation);
        put (keys, "columns", columns);
        put (keys, "labels", labels);

        if (formation != "grid")
        {
            // A list's rows: no lamps, no names out of them, no rings.
            put (keys, "shape", keys.getProperty ("shape", "rect").toString() == "circle" ? juce::var ("rect") : keys.getProperty ("shape", "rect"));
        }

        return keys;
    }

    //==========================================================================
    struct Entry
    {
        int preset = -1;          // a preset, or...
        juce::String heading;     // ...a category's name over its presets
    };

    std::vector<Entry> entriesOf (const juce::var& d, const PresetPanel::View& v)
    {
        std::vector<Entry> entries;
        const auto tabs = PresetPanel::tabsOf (v);
        const auto categories = categoriesOf (d);
        const bool filter = categories == "tabs" && tabs.size() > 2 && v.tab > 0 && v.tab < tabs.size();
        const bool headings = categories == "headings" && PresetPanel::styleOf (d) != "table";
        juce::String last = "\x01";

        for (int i = 0; i < v.names.size(); ++i)
        {
            const auto group = v.groups[i].trim();

            if (filter && (group.isEmpty() ? juce::String ("Presets") : group) != tabs[v.tab])
                continue;

            if (v.search.isNotEmpty() && ! v.names[i].containsIgnoreCase (v.search))
                continue;

            if (headings)
            {
                const auto heading = group.isEmpty() ? juce::String ("Presets") : group;

                if (heading != last)
                {
                    entries.push_back ({ -1, heading });
                    last = heading;
                }
            }

            entries.push_back ({ i, {} });
        }

        return entries;
    }

    struct Layout
    {
        juce::Rectangle<float> title, search, tabs, items, nav, actions;
        std::vector<juce::Rectangle<float>> tabBoxes, actionBoxes;
        std::vector<std::pair<Entry, juce::Rectangle<float>>> rows;   // this page's
        juce::Rectangle<float> pagePrev, pageNext, pageText, more;
        int pages = 1, page = 0, columns = 1, rowsDown = 1;
        bool grid = false, empty = false;
        float rowHeight = 26.0f;
    };

    Layout layoutOf (const juce::var& d, juce::Rectangle<float> area, const PresetPanel::View& v)
    {
        Layout l;
        const auto style = PresetPanel::styleOf (d);
        const bool table = style == "table", side = style == "side";
        auto r = area.reduced (side ? 10.0f : 8.0f);

        if (side)
        {
            l.title = r.removeFromTop (26.0f);
            r.removeFromTop (6.0f);
        }

        if (! table && boolOf (d, "search", true))
        {
            l.search = r.removeFromTop (26.0f);
            r.removeFromTop (6.0f);
        }

        const auto tabs = PresetPanel::tabsOf (v);

        if (categoriesOf (d) == "tabs" && tabs.size() > 2)
        {
            l.tabs = r.removeFromTop (24.0f);
            r.removeFromTop (6.0f);
            const float gap = 4.0f, each = (l.tabs.getWidth() - gap * (float) (tabs.size() - 1)) / (float) tabs.size();

            for (int i = 0; i < tabs.size(); ++i)
                l.tabBoxes.push_back ({ l.tabs.getX() + (float) i * (each + gap), l.tabs.getY(), each, l.tabs.getHeight() });
        }

        if (! table && hasActions (d))
        {
            l.actions = r.removeFromBottom (28.0f);
            r.removeFromBottom (6.0f);
            const float gap = 4.0f, each = (l.actions.getWidth() - gap * 3.0f) / 4.0f;

            for (int i = 0; i < 4; ++i)
                l.actionBoxes.push_back ({ l.actions.getX() + (float) i * (each + gap), l.actions.getY(), each, l.actions.getHeight() });
        }

        const auto entries = entriesOf (d, v);
        l.empty = entries.empty();
        int capacity = 1;

        if (table)
        {
            l.grid = true;
            l.columns = juce::jlimit (1, 8, (int) numberOf (d, "columns", 3.0f));
            l.rowsDown = juce::jlimit (1, 12, (int) numberOf (d, "rows", 4.0f));
            l.nav = r.removeFromBottom (22.0f);
            r.removeFromBottom (4.0f);
            capacity = l.columns * l.rowsDown;
        }
        else
        {
            l.rowHeight = juce::jlimit (14.0f, 60.0f, numberOf (d, "rowHeight", 26.0f));
            capacity = juce::jmax (1, (int) std::floor (r.getHeight() / l.rowHeight));

            if ((int) entries.size() > capacity)
            {
                l.nav = r.removeFromBottom (22.0f);
                r.removeFromBottom (4.0f);
                capacity = juce::jmax (1, (int) std::floor (r.getHeight() / l.rowHeight));
            }
        }

        l.items = r;
        l.pages = juce::jmax (1, ((int) entries.size() + capacity - 1) / capacity);
        l.page = juce::jlimit (0, l.pages - 1, v.page);

        const auto first = (size_t) (l.page * capacity);

        for (size_t k = first; k < entries.size() && k < first + (size_t) capacity; ++k)
        {
            const int at = (int) (k - first);

            if (table)
            {
                const float cw = r.getWidth() / (float) l.columns, ch = r.getHeight() / (float) l.rowsDown;
                l.rows.push_back ({ entries[k], { r.getX() + cw * (float) (at % l.columns), r.getY() + ch * (float) (at / l.columns), cw, ch } });
            }
            else
            {
                l.rows.push_back ({ entries[k], { r.getX(), r.getY() + l.rowHeight * (float) at, r.getWidth(), l.rowHeight } });
            }
        }

        if (! l.nav.isEmpty())
        {
            auto n = l.nav;

            if (table && hasActions (d))
            {
                l.more = n.removeFromRight (30.0f);
                n.removeFromRight (4.0f);
            }

            l.pagePrev = n.removeFromLeft (28.0f);
            l.pageNext = n.removeFromRight (28.0f);
            l.pageText = n;
        }

        return l;
    }

    void paintChevron (juce::Graphics& g, juce::Rectangle<float> box, int direction, juce::Colour colour)   // -1 left, 1 right, 2 down
    {
        const float a = juce::jmin (box.getWidth(), box.getHeight()) * 0.22f;
        const auto c = box.getCentre();
        juce::Path p;

        if (direction == 2)
            p.addTriangle (c.x - a, c.y - a * 0.5f, c.x + a, c.y - a * 0.5f, c.x, c.y + a * 0.6f);
        else
            p.addTriangle (c.x + (float) -direction * a * 0.6f, c.y - a, c.x + (float) -direction * a * 0.6f, c.y + a, c.x + (float) direction * a * 0.7f, c.y);

        g.setColour (colour);
        g.fillPath (p);
    }

    void paintPlate (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour panel, juce::Colour line, float corner)
    {
        g.setGradientFill (juce::ColourGradient (panel.brighter (0.06f), 0.0f, area.getY(), panel.darker (0.12f), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area, corner);
        g.setColour (line);
        g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) area.getY() + 1, area.getX() + corner, area.getRight() - corner);
    }
}

//==============================================================================
bool PresetPanel::has (const GuiWidget& w)
{
    if (w.type != GuiWidget::Type::preset)
        return false;

    const auto found = w.settings.find ("presets");
    return found != w.settings.end() && found->second.isObject();
}

juce::var PresetPanel::designOf (const GuiWidget& w)
{
    const auto found = w.settings.find ("presets");
    return found != w.settings.end() && found->second.isObject() ? found->second : juce::var();
}

juce::String PresetPanel::styleOf (const juce::var& d)
{
    const auto style = textOf (d, "style", "list");
    return style == "bar" || style == "table" || style == "side" ? style : juce::String ("list");
}

bool PresetPanel::opensPopup (const juce::var& d)
{
    const auto style = styleOf (d);
    return style == "list" || (style == "table" && textOf (d, "open", "panel") == "popup");
}

bool PresetPanel::isSide (const juce::var& d)       { return styleOf (d) == "side"; }
bool PresetPanel::onLeft (const juce::var& d)       { return textOf (d, "side", "left") != "right"; }
int PresetPanel::sideWidth (const juce::var& d)     { return juce::jlimit (120, 480, (int) numberOf (d, "width", 220.0f)); }
bool PresetPanel::toggles (const juce::var& d)      { return boolOf (d, "toggle", false); }
bool PresetPanel::startsOpen (const juce::var& d)   { return ! toggles (d) || boolOf (d, "startOpen", false); }

juce::StringArray PresetPanel::tabsOf (const View& v)
{
    juce::StringArray tabs { "All" };

    for (int i = 0; i < v.names.size(); ++i)
    {
        const auto group = v.groups[i].trim().isEmpty() ? juce::String ("Presets") : v.groups[i].trim();
        tabs.addIfNotAlreadyThere (group);
    }

    return tabs;
}

int PresetPanel::pagesOf (const juce::var& d, juce::Rectangle<float> area, const View& v)
{
    return layoutOf (d, area, v).pages;
}

juce::Rectangle<float> PresetPanel::searchArea (const juce::var& d, juce::Rectangle<float> area, const View& v)
{
    return layoutOf (d, area, v).search;
}

//==============================================================================
void PresetPanel::paintBar (juce::Graphics& g, const juce::var& d, juce::Rectangle<float> area, const View& v,
                            float lightAngle, float lightAltitude, float resolution)
{
    const auto keys = keysOf (d, "row", 1, "none");
    SelectorSet::paint (g, keys, area, { "" }, { false }, lightAngle, lightAltitude, resolution);

    const auto arrows = juce::jmin (24.0f, area.getHeight());
    auto inside = area.reduced (4.0f, 0.0f);
    const auto left = inside.removeFromLeft (arrows), right = inside.removeFromRight (arrows);
    const auto accent = accentOf (d);
    paintChevron (g, left, -1, accent);
    paintChevron (g, right, 1, accent);

    if (opensPopup (d))
        paintChevron (g, inside.removeFromRight (16.0f), 2, accent.withAlpha (0.8f));

    // LED digits: its number and name in a window between the arrows.
    if (barIsLed (d))
    {
        int cells = 0;
        const auto text = ledText (d, v, cells);
        SegmentDisplay::paint (g, inside.reduced (2.0f, juce::jmin (3.0f, area.getHeight() * 0.1f)), text,
                               SegmentDisplay::styleOf (d, "led", SegmentDisplay::Style().lit), cells);
        return;
    }

    const auto name = juce::isPositiveAndBelow (v.current, v.names.size()) ? v.names[v.current] + (v.modified ? " *" : "")
                                                                         : juce::String (v.names.isEmpty() ? "No presets" : "Presets");
    g.setColour (colourOf (keys, "labelColour", textColourOf (d)));
    g.setFont (fontOf (d, juce::jmin (15.0f, area.getHeight() * 0.48f), true));
    g.drawFittedText (name, inside.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

bool PresetPanel::barIsLed (const juce::var& d)
{
    return textOf (d, "barDisplay", "keys") == "led";
}

juce::String PresetPanel::ledText (const juce::var& d, const View& v, int& cells)
{
    const auto digits = juce::jlimit (0, 4, juce::roundToInt (numberOf (d, "ledNumber", 2.0f)));
    int longest = 0;

    for (const auto& name : v.names)
        longest = juce::jmax (longest, SegmentDisplay::cellsOf (name.trim().substring (0, 24)));

    const bool chosen = juce::isPositiveAndBelow (v.current, v.names.size());
    juce::String text;

    if (digits > 0)
        text << (chosen ? juce::String (v.current + 1).paddedLeft ('0', digits) : juce::String::repeatedString ("-", digits));

    if (longest > 0 || v.names.isEmpty())
    {
        const auto name = chosen ? v.names[v.current].trim().substring (0, 24) : juce::String (v.names.isEmpty() ? "NO PRESETS" : "");
        text << (digits > 0 ? " " : "") << name.toUpperCase() << juce::String::repeatedString (" ", juce::jmax (0, longest - SegmentDisplay::cellsOf (name)));
    }

    cells = juce::jmax (SegmentDisplay::cellsOf (text), digits + (longest > 0 ? 1 + longest : 0));
    return text;
}

PresetPanel::Hit PresetPanel::hitBar (const juce::var&, juce::Rectangle<float> area, juce::Point<float> at)
{
    if (! area.contains (at))
        return {};

    const auto arrows = juce::jmin (24.0f, area.getHeight()) + 4.0f;

    if (at.x < area.getX() + arrows)      return { Hit::Kind::prev, -1 };
    if (at.x > area.getRight() - arrows)  return { Hit::Kind::next, -1 };
    return { Hit::Kind::open, -1 };
}

//==============================================================================
void PresetPanel::paintBody (juce::Graphics& g, const juce::var& d, juce::Rectangle<float> area, const View& v,
                             float lightAngle, float lightAltitude, float resolution)
{
    const auto style = styleOf (d);
    const bool side = style == "side", popup = opensPopup (d);
    const auto accent = accentOf (d), panel = panelOf (d), line = lineOf (d), text = textColourOf (d);
    const auto l = layoutOf (d, area, v);

    // Its plate: a side panel's module (joined to the window), a popup's, a table's on the panel.
    if (side)
    {
        g.setGradientFill (juce::ColourGradient (panel.brighter (0.07f), area.getX(), 0.0f, panel.darker (0.1f), area.getRight(), 0.0f, false));
        g.fillRect (area);
        g.setColour (line);
        g.drawRect (area, 1.0f);

        // Where it joins the window: a seam, and the plates bridging it.
        const float seam = onLeft (d) ? area.getRight() : area.getX();
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawVerticalLine ((int) seam, area.getY(), area.getBottom());

        for (const float at : { 0.14f, 0.5f, 0.86f })
        {
            const auto bridge = juce::Rectangle<float> (14.0f, 30.0f).withCentre ({ seam, area.getY() + area.getHeight() * at });
            g.setGradientFill (juce::ColourGradient (line.brighter (0.5f), bridge.getX(), 0.0f, line.darker (0.2f), bridge.getRight(), 0.0f, false));
            g.fillRoundedRectangle (bridge, 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawRoundedRectangle (bridge, 3.0f, 1.0f);

            for (const float y : { 0.28f, 0.72f })
            {
                const auto screw = juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ bridge.getCentreX(), bridge.getY() + bridge.getHeight() * y });
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillEllipse (screw);
            }
        }
    }
    else if (popup)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (area.translated (0.0f, 4.0f).expanded (2.0f), 9.0f);
        paintPlate (g, area, panel, line, 8.0f);
    }
    else if (boolOf (d, "plate", true))
    {
        paintPlate (g, area, panel, line, 8.0f);
    }

    if (! l.title.isEmpty())
    {
        g.setColour (accent);
        g.setFont (fontOf (d, 13.0f, true).withExtraKerningFactor (0.12f));
        g.drawText (d.getProperty ("title", "PRESETS").toString().toUpperCase(), l.title, juce::Justification::centredLeft, true);
        g.setColour (line);
        g.drawHorizontalLine ((int) l.title.getBottom() + 2, l.title.getX(), l.title.getRight());
    }

    // The search field: a glass and what's typed (a caret while it's typed in).
    if (! l.search.isEmpty())
    {
        g.setColour (panel.darker (0.45f));
        g.fillRoundedRectangle (l.search, l.search.getHeight() * 0.5f);
        g.setColour (v.searching ? accent : line.brighter (0.2f));
        g.drawRoundedRectangle (l.search.reduced (0.5f), l.search.getHeight() * 0.5f, 1.0f);

        auto inside = l.search.reduced (10.0f, 0.0f);
        const auto glass = inside.removeFromLeft (14.0f);
        const auto c = glass.getCentre().translated (-1.0f, -1.0f);
        g.setColour (text.withAlpha (0.6f));
        g.drawEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (c), 1.4f);
        g.drawLine (c.x + 3.0f, c.y + 3.0f, c.x + 6.0f, c.y + 6.0f, 1.6f);
        inside.removeFromLeft (6.0f);

        g.setFont (fontOf (d, 12.5f));
        g.setColour (v.search.isEmpty() ? text.withAlpha (0.4f) : text);
        const auto shown = v.search.isEmpty() && ! v.searching ? juce::String::fromUTF8 ("Search\xe2\x80\xa6") : v.search;
        g.drawText (shown, inside, juce::Justification::centredLeft, true);

        if (v.searching && (juce::Time::getMillisecondCounter() / 500) % 2 == 0)
        {
            const float x = inside.getX() + (float) juce::GlyphArrangement::getStringWidthInt (fontOf (d, 12.5f), v.search) + 1.0f;
            g.setColour (accent);
            g.fillRect (juce::Rectangle<float> (x, inside.getCentreY() - 6.0f, 1.5f, 12.0f));
        }
    }

    // The categories' tabs.
    const auto tabs = tabsOf (v);

    for (size_t i = 0; i < l.tabBoxes.size(); ++i)
    {
        const bool on = (int) i == juce::jlimit (0, tabs.size() - 1, v.tab);
        const auto box = l.tabBoxes[i];
        g.setColour (on ? accent : panel.brighter (0.08f));
        g.fillRoundedRectangle (box, box.getHeight() * 0.5f);
        g.setColour (on ? accent.brighter (0.2f) : line);
        g.drawRoundedRectangle (box.reduced (0.5f), box.getHeight() * 0.5f, 1.0f);
        g.setColour (on ? (accent.getPerceivedBrightness() > 0.62f ? juce::Colour (0xff141416) : juce::Colours::white) : text.withAlpha (0.75f));
        g.setFont (fontOf (d, 11.5f, true));
        g.drawFittedText (tabs[(int) i], box.reduced (4.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }

    // The presets: in rows (the headings between), or a table.
    if (l.empty)
    {
        g.setColour (text.withAlpha (0.45f));
        g.setFont (fontOf (d, 13.0f));
        g.drawText (v.names.isEmpty() ? "No presets yet" : "Nothing found", l.items, juce::Justification::centred, true);
    }
    else if (l.grid)
    {
        juce::StringArray names;
        std::vector<bool> lit;

        for (const auto& [entry, box] : l.rows)
        {
            names.add (v.names[entry.preset] + (entry.preset == v.current && v.modified ? " *" : ""));
            lit.push_back (entry.preset == v.current);
        }

        const float cellHeight = l.items.getHeight() / (float) l.rowsDown;
        const int down = ((int) l.rows.size() + l.columns - 1) / l.columns;
        const auto keys = keysOf (d, "grid", l.columns, "caps");

        // A part-filled page keeps its keys' size: its rows' height, its columns' width.
        SelectorSet::paint (g, keys, l.items.withHeight (cellHeight * (float) down), names, lit, lightAngle, lightAltitude, resolution);
    }
    else
    {
        const auto keys = keysOf (d, "column", 1, "caps");
        size_t k = 0;

        while (k < l.rows.size())
        {
            const auto& [entry, box] = l.rows[k];

            if (entry.preset < 0)
            {
                g.setColour (accent.withAlpha (0.85f));
                g.setFont (fontOf (d, 11.0f, true).withExtraKerningFactor (0.08f));
                g.drawText (entry.heading.toUpperCase(), box.reduced (4.0f, 0.0f), juce::Justification::bottomLeft, true);
                g.setColour (line);
                g.drawHorizontalLine ((int) box.getBottom() - 2, box.getX() + 4.0f, box.getRight() - 4.0f);
                ++k;
                continue;
            }

            // A run of presets between headings: one column of keys.
            juce::StringArray names;
            std::vector<bool> lit;
            auto run = box;

            for (; k < l.rows.size() && l.rows[k].first.preset >= 0; ++k)
            {
                const int p = l.rows[k].first.preset;
                names.add (v.names[p] + (p == v.current && v.modified ? " *" : ""));
                lit.push_back (p == v.current);
                run = run.getUnion (l.rows[k].second);
            }

            SelectorSet::paint (g, keys, run, names, lit, lightAngle, lightAltitude, resolution);
        }
    }

    // Its pages.
    if (! l.nav.isEmpty())
    {
        paintChevron (g, l.pagePrev, -1, l.page > 0 ? accent : accent.withAlpha (0.25f));
        paintChevron (g, l.pageNext, 1, l.page < l.pages - 1 ? accent : accent.withAlpha (0.25f));
        g.setColour (text.withAlpha (0.7f));
        g.setFont (fontOf (d, 11.5f));
        g.drawText (juce::String (l.page + 1) + " / " + juce::String (l.pages), l.pageText, juce::Justification::centred, true);

        if (! l.more.isEmpty())
        {
            g.setColour (accent);

            for (int i = -1; i <= 1; ++i)
                g.fillEllipse (juce::Rectangle<float> (3.5f, 3.5f).withCentre (l.more.getCentre().translated ((float) i * 6.0f, 0.0f)));
        }
    }

    // Save, Save as, Load, More.
    if (! l.actionBoxes.empty())
        SelectorSet::paint (g, keysOf (d, "row", 4, "caps"), l.actions, { "Save", "Save as", "Load", juce::String::fromUTF8 ("More\xe2\x80\xa6") },
                            { false, false, false, false }, lightAngle, lightAltitude, resolution);
}

PresetPanel::Hit PresetPanel::hitBody (const juce::var& d, juce::Rectangle<float> area, const View& v, juce::Point<float> at)
{
    if (! area.contains (at))
        return {};

    const auto l = layoutOf (d, area, v);

    if (l.search.contains (at))
        return { Hit::Kind::search, -1 };

    for (size_t i = 0; i < l.tabBoxes.size(); ++i)
        if (l.tabBoxes[i].contains (at))
            return { Hit::Kind::tab, (int) i };

    for (const auto& [entry, box] : l.rows)
        if (box.contains (at) && entry.preset >= 0)
            return { Hit::Kind::preset, entry.preset };

    if (l.pagePrev.contains (at))  return { Hit::Kind::pagePrev, -1 };
    if (l.pageNext.contains (at))  return { Hit::Kind::pageNext, -1 };
    if (l.more.contains (at))      return { Hit::Kind::more, -1 };

    for (size_t i = 0; i < l.actionBoxes.size(); ++i)
        if (l.actionBoxes[i].contains (at))
            return { Hit::Kind::action, (int) i };

    return { Hit::Kind::body, -1 };
}

juce::Rectangle<float> PresetPanel::popupArea (const juce::var& d, juce::Rectangle<float> bar, juce::Rectangle<float> window, const View& v)
{
    const bool table = styleOf (d) == "table";
    const bool tabs = categoriesOf (d) == "tabs" && tabsOf (v).size() > 2;
    float w = 0.0f, h = 16.0f + (tabs ? 30.0f : 0.0f);

    if (table)
    {
        const int columns = juce::jlimit (1, 8, (int) numberOf (d, "columns", 3.0f)), rows = juce::jlimit (1, 12, (int) numberOf (d, "rows", 4.0f));
        w = juce::jmax (bar.getWidth(), (float) columns * 104.0f + 16.0f);
        h += (float) rows * 40.0f + 26.0f;
    }
    else
    {
        const auto rowHeight = juce::jlimit (14.0f, 60.0f, numberOf (d, "rowHeight", 26.0f));
        const int shown = juce::jlimit (1, 40, (int) numberOf (d, "listRows", 10.0f));
        const int count = (int) entriesOf (d, v).size();
        w = juce::jmax (bar.getWidth(), 190.0f);
        h += (boolOf (d, "search", true) ? 32.0f : 0.0f) + (hasActions (d) ? 34.0f : 0.0f)
           + rowHeight * (float) juce::jlimit (3, shown, juce::jmax (3, count)) + (count > shown ? 26.0f : 0.0f) + 2.0f;
    }

    w = juce::jmin (w, window.getWidth());
    h = juce::jmin (h, window.getHeight());
    float y = bar.getBottom() + 4.0f;

    if (y + h > window.getBottom())
        y = bar.getY() - 4.0f - h;

    y = juce::jlimit (window.getY(), juce::jmax (window.getY(), window.getBottom() - h), y);
    const float x = juce::jlimit (window.getX(), juce::jmax (window.getX(), window.getRight() - w), bar.getX());
    return { x, y, w, h };
}

//==============================================================================
juce::Image PresetPanel::keyPicture (const juce::var& d, const juce::String& where, int width, int height, bool lit, float lightAngle, float lightAltitude)
{
    const int w = juce::jmax (1, width), h = juce::jmax (1, height);
    juce::Image image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (image);

    if (where == "bar")
    {
        SelectorSet::paint (g, keysOf (d, "row", 1, "none"), { (float) w, (float) h }, { "" }, { lit }, lightAngle, lightAltitude);
        return image;
    }

    // As one of several: its gap round it as the list's or the table's keys have it.
    if (where == "cell")
        SelectorSet::paint (g, keysOf (d, "grid", 2, "none"), { (float) w * 2.0f, (float) h * 2.0f }, { "", "", "", "" }, { lit, false, false, false },
                            lightAngle, lightAltitude);
    else
        SelectorSet::paint (g, keysOf (d, "column", 1, "none"), { (float) w, (float) h * 2.0f }, { "", "" }, { lit, false }, lightAngle, lightAltitude);

    return image;
}

juce::Rectangle<int> PresetPanel::moduleArea (const juce::var& d, int windowWidth, int windowHeight)
{
    const int width = sideWidth (d);
    return { onLeft (d) ? -width : windowWidth, 0, width, windowHeight };
}

juce::Rectangle<float> PresetPanel::handleArea (const juce::var& d, int windowWidth, int windowHeight)
{
    const float h = juce::jmin (110.0f, (float) windowHeight * 0.5f), w = 16.0f;
    return { onLeft (d) ? 0.0f : (float) windowWidth - w, ((float) windowHeight - h) * 0.5f, w, h };
}

void PresetPanel::paintHandle (juce::Graphics& g, const juce::var& d, juce::Rectangle<float> area, bool open)
{
    const bool left = onLeft (d);
    const auto accent = accentOf (d), panel = panelOf (d);

    // A tab on the window's edge, rounded on its inner side.
    juce::Path tab;
    tab.addRoundedRectangle (area.getX(), area.getY(), area.getWidth(), area.getHeight(), 6.0f, 6.0f, ! left, left, ! left, left);
    g.setColour (panel.withAlpha (0.92f));
    g.fillPath (tab);
    g.setColour (accent.withAlpha (0.8f));
    g.strokePath (tab, juce::PathStrokeType (1.2f));

    // Its arrow: where the panel goes (out when it's closed, back when it's open).
    const int direction = (left != open) ? 1 : -1;
    paintChevron (g, area.withHeight (area.getWidth() * 1.4f).translated (0.0f, 4.0f), direction, accent);

    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::rotation (-pi * 0.5f, area.getCentreX(), area.getCentreY()));
    g.setColour (accent.withAlpha (0.9f));
    g.setFont (FontLibrary::font ({}, 9.5f, true).withExtraKerningFactor (0.18f));
    const auto turned = juce::Rectangle<float> (area.getHeight() - 26.0f, area.getWidth()).withCentre (area.getCentre()).translated (-8.0f, 0.0f);
    g.drawText ("PRESETS", turned, juce::Justification::centred, false);
}

//==============================================================================
namespace
{
    struct Template { const char* name; const char* json; };

    const Template templates[] {
        { "Drop list (dark)",
          R"({ "style": "list", "categories": "headings", "search": true, "actions": true, "rowHeight": 26, "listRows": 10,
               "colour": "#FF2FB36B", "panelColour": "#FF17191C", "textColour": "#FFD8DADF", "lineColour": "#FF2C2F35",
               "keys": { "render": "2d", "style": "flat", "shape": "rect", "rounding": 20, "gap": 8, "capColour": "#FF1D1F23", "capOnColour": "#FF233A2D",
                         "glowColour": "#FF2FB36B", "glow": 25, "labelColour": "#FFC8CBD0", "labelOnColour": "#FFFFFFFF", "textSize": 60 } })" },
        { "Drop list (3D keys)",
          R"({ "style": "list", "categories": "tabs", "search": true, "actions": true, "rowHeight": 30, "listRows": 9,
               "colour": "#FF5FE0E8", "panelColour": "#FF141619", "textColour": "#FFD8DADF", "lineColour": "#FF2A2D33",
               "keys": { "render": "3d", "shape": "rect", "rounding": 24, "gap": 14, "capColour": "#FF2A2C31", "capOnColour": "#FF2F3A3D", "edge": true, "edgeOff": 0,
                         "glowColour": "#FF5FE0E8", "glow": 55, "labelColour": "#FFC8CBD0", "labelOnColour": "#FFFFFFFF", "textSize": 56,
                         "material": "Satin plastic", "height": 12 } })" },
        { "Drop list (light)",
          R"({ "style": "list", "categories": "headings", "search": true, "actions": true, "rowHeight": 26, "listRows": 10,
               "colour": "#FF2F6FDE", "panelColour": "#FFF1F2F4", "textColour": "#FF2A2C30", "lineColour": "#FFCDD1D7",
               "keys": { "render": "2d", "style": "flat", "shape": "rect", "rounding": 20, "gap": 8, "capColour": "#FFF7F8FA", "capOnColour": "#FFDCE7FB",
                         "glowColour": "#FF2F6FDE", "glow": 20, "labelColour": "#FF33363B", "labelOnColour": "#FF0F2F6B", "textSize": 60 } })" },
        { "Table on the panel (3D pads)",
          R"({ "style": "table", "open": "panel", "columns": 4, "rows": 3, "categories": "tabs", "actions": true, "plate": true,
               "colour": "#FF8C6CFF", "panelColour": "#FF141417", "textColour": "#FFD8DADF", "lineColour": "#FF2A2A30",
               "keys": { "render": "3d", "shape": "rect", "rounding": 22, "gap": 14, "capColour": "#FF2B2C30", "capOnColour": "#FF3A2F66",
                         "glowColour": "#FF8C6CFF", "glow": 65, "capGlow": true, "labelColour": "#FFC8CBD0", "labelOnColour": "#FFFFFFFF", "textSize": 42,
                         "material": "Rubber", "height": 16 } })" },
        { "Table on the panel (flat)",
          R"({ "style": "table", "open": "panel", "columns": 3, "rows": 4, "categories": "none", "actions": true, "plate": true,
               "colour": "#FF2FB36B", "panelColour": "#FF17191C", "textColour": "#FFD8DADF", "lineColour": "#FF2C2F35",
               "keys": { "render": "2d", "style": "soft", "shape": "rect", "rounding": 18, "gap": 10, "capColour": "#FF24262B", "capOnColour": "#FF27392F",
                         "glowColour": "#FF2FB36B", "glow": 35, "labelColour": "#FFC8CBD0", "labelOnColour": "#FFFFFFFF", "textSize": 42 } })" },
        { "Table in a popup",
          R"({ "style": "table", "open": "popup", "columns": 4, "rows": 4, "categories": "tabs", "actions": true,
               "colour": "#FFF2B33A", "panelColour": "#FF1A1B1E", "textColour": "#FFE6E6E6", "lineColour": "#FF33353A",
               "keys": { "render": "2d", "style": "glossy", "shape": "rect", "rounding": 16, "gap": 10, "capColour": "#FF26282C", "capOnColour": "#FF4A3A18",
                         "glowColour": "#FFF2B33A", "glow": 40, "labelColour": "#FFD0D3D8", "labelOnColour": "#FFFFFFFF", "textSize": 42 } })" },
        { "Table in a popup (3D)",
          R"({ "style": "table", "open": "popup", "columns": 3, "rows": 4, "categories": "none", "actions": true,
               "colour": "#FFFF6A3D", "panelColour": "#FF151618", "textColour": "#FFE6E6E6", "lineColour": "#FF2E3034",
               "keys": { "render": "3d", "shape": "rect", "rounding": 18, "gap": 12, "capColour": "#FFD9DADC", "capOnColour": "#FFF3F3F1",
                         "glowColour": "#FFFF6A3D", "glow": 55, "edge": true, "edgeOff": 0, "labelColour": "#FF2A2B2E", "labelOnColour": "#FF101010", "textSize": 42,
                         "material": "Satin plastic", "height": 16 } })" },
        { "Side panel (left)",
          R"({ "style": "side", "side": "left", "width": 220, "toggle": false, "categories": "tabs", "search": true, "actions": true, "rowHeight": 26,
               "title": "Presets", "colour": "#FF2FB36B", "panelColour": "#FF16181B", "textColour": "#FFD8DADF", "lineColour": "#FF2C2F35",
               "keys": { "render": "2d", "style": "flat", "shape": "rect", "rounding": 20, "gap": 8, "capColour": "#FF1D1F23", "capOnColour": "#FF233A2D",
                         "glowColour": "#FF2FB36B", "glow": 25, "labelColour": "#FFC8CBD0", "labelOnColour": "#FFFFFFFF", "textSize": 60 } })" },
        { "Side panel (right, opens and closes)",
          R"({ "style": "side", "side": "right", "width": 230, "toggle": true, "startOpen": false, "categories": "headings", "search": true, "actions": true,
               "rowHeight": 26, "title": "Presets", "colour": "#FF5FE0E8", "panelColour": "#FF121417", "textColour": "#FFD8DADF", "lineColour": "#FF262A30",
               "keys": { "render": "2d", "style": "soft", "shape": "rect", "rounding": 24, "gap": 10, "capColour": "#FF1E2125", "capOnColour": "#FF1F3438",
                         "glowColour": "#FF5FE0E8", "glow": 35, "labelColour": "#FFC8CBD0", "labelOnColour": "#FFFFFFFF", "textSize": 58 } })" },
        { "Side panel (3D keys)",
          R"({ "style": "side", "side": "left", "width": 240, "toggle": true, "startOpen": true, "categories": "tabs", "search": true, "actions": true,
               "rowHeight": 30, "title": "Sound library", "colour": "#FFFFB547", "panelColour": "#FF1B1C1F", "textColour": "#FFE6E6E6", "lineColour": "#FF303237",
               "keys": { "render": "3d", "shape": "rect", "rounding": 22, "gap": 14, "capColour": "#FF2C2E33", "capOnColour": "#FF3B3426", "edge": true, "edgeOff": 0,
                         "glowColour": "#FFFFB547", "glow": 50, "labelColour": "#FFD0D3D8", "labelOnColour": "#FFFFFFFF", "textSize": 56,
                         "material": "Satin plastic", "height": 12 } })" },
        { "Bar (classic)",
          R"({ "style": "bar", "colour": "#FF2FB36B", "textColour": "#FFE6E6E6",
               "keys": { "render": "2d", "style": "flat", "shape": "rect", "rounding": 22, "capColour": "#C0000000", "labelColour": "#FFE6E6E6" } })" },
        { "Bar (3D key)",
          R"({ "style": "bar", "colour": "#FF5FE0E8", "textColour": "#FFE6E6E6",
               "keys": { "render": "3d", "shape": "rect", "rounding": 30, "capColour": "#FF26282C", "labelColour": "#FFD8DADF", "material": "Satin plastic", "height": 14 } })" },
    };
}

juce::StringArray PresetPanel::templateNames()
{
    juce::StringArray names;

    for (const auto& t : templates)
        names.add (t.name);

    return names;
}

juce::var PresetPanel::makeTemplate (int index)
{
    const auto& t = templates[juce::jlimit (0, (int) std::size (templates) - 1, index)];
    return juce::JSON::parse (t.json);
}

PresetPanel::View PresetPanel::sampleView()
{
    View v;
    const std::pair<const char*, const char*> presets[] {
        { "Warm Pad", "Pads" },     { "Silk Strings", "Pads" }, { "Night Choir", "Pads" },  { "Glass Pad", "Pads" },
        { "Glass Keys", "Keys" },   { "Dusty Rhodes", "Keys" }, { "Bright Piano", "Keys" }, { "Soft Bells", "Keys" },
        { "Sub Bass", "Bass" },     { "Round Bass", "Bass" },   { "Acid Line", "Bass" },
        { "Saw Lead", "Leads" },    { "Soft Pluck", "Leads" },  { "Analog Brass", "Leads" }, { "Sync Lead", "Leads" },
        { "Init", "Leads" }
    };

    for (const auto& [name, group] : presets)
    {
        v.names.add (name);
        v.groups.add (group);
    }

    v.current = 0;
    return v;
}

juce::File PresetPanel::galleryFolder()
{
    auto folder = SettingsFolder::folder ("Preset Gallery");
    folder.createDirectory();
    return folder;
}
