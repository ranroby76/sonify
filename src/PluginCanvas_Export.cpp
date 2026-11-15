// D:\workspace\Fanan_web_hub\src\PluginCanvas_Export.cpp
// The canvas baked for Export to plugin files (PluginExporter): each page's background, and what
// moves as strips of frames drawn the way the panel draws it, so the exported plugin's window
// looks as the panel does in Play.

#include "PluginCanvas.h"
#include "MeterPaint.h"
#include "PanelParts.h"
#include "ModMatrix.h"
#include "PerformControl.h"
#include "SelectorSet.h"

namespace
{
    constexpr int meterFrames = 48, lampFrames = 16, readoutFrames = 101, plainControlFrames = 64;

    /** A box turned about its middle: what it covers, in whole pixels. */
    juce::Rectangle<int> turnedArea (const GuiWidget& w, juce::Rectangle<int> box)
    {
        if (! w.isTurned() || box.isEmpty())
            return box;

        juce::Path outline;
        outline.addRectangle (box.toFloat());
        return outline.getBoundsTransformed (w.turn()).getSmallestIntegerContainer().expanded (1);
    }

    /** A level that a meter shows at a fraction of its height (MeterPaint::fractionOf, backwards). */
    float levelAt (float fraction, bool isDisplay)
    {
        return isDisplay ? fraction : std::pow (10.0f, (fraction * 66.0f - 60.0f) / 20.0f);
    }
}

PluginCanvas::ExportBake PluginCanvas::bakeForExport (const juce::StringArray& presetNames)
{
    // As it plays: nothing hovered or held, no Edit marks. What's changed here is put back after.
    const juce::ScopedValueSetter<int> noHover (hovered, -1), noActive (active, -1);
    const juce::ScopedValueSetter<bool> bakingNow (baking, true);
    const juce::ScopedValueSetter<int> pageNow (shownPage, shownPage);
    const auto savedPresets = presetMenu;
    const auto savedValues = values;
    const auto savedLevels = meterLevels;
    const auto savedNeedles = meterNeedles;
    const auto savedPeaks = meterPeaks;

    presetMenu = {};
    presetMenu.names = presetNames;

    // Side panels join the window: it's as wide as with them all open (no handles in a plugin).
    juce::Rectangle<int> window (0, 0, layout.width, layout.height);

    for (const auto& w : layout.widgets)
        if (PresetPanel::has (w))
            if (const auto d = PresetPanel::designOf (w); PresetPanel::isSide (d))
                window = window.getUnion (PresetPanel::moduleArea (d, layout.width, layout.height));

    const auto shift = -window.getPosition();

    ExportBake result;
    result.width = window.getWidth();
    result.height = window.getHeight();
    const auto numPages = juce::jmax (1, layout.pages.size());

    // The page an element lies on (-1: every page), as GuiLayout::isOnPage has it.
    auto pageOf = [this] (const GuiWidget& w)
    {
        if (layout.pages.isEmpty() || w.page.isEmpty())
            return -1;

        return juce::jmax (0, layout.pages.indexOf (w.page));
    };

    auto sharePage = [&] (const GuiWidget& a, const GuiWidget& b)
    {
        const auto pa = pageOf (a), pb = pageOf (b);
        return pa < 0 || pb < 0 || pa == pb;
    };

    // What moves: in the plugin a readout's value and the page tabs move too.
    auto movingPart = [this] (const GuiWidget& w)
    {
        if (w.type == GuiWidget::Type::tabs)
            return w.bounds;

        return movingAreaOf (w);
    };

    auto isStill = [] (const GuiWidget& w)
    {
        return (w.type == GuiWidget::Type::label && ! PanelParts::labelMoves (w)) || w.type == GuiWidget::Type::group
            || w.type == GuiWidget::Type::shape || (w.type == GuiWidget::Type::image && w.image.isNotEmpty());
    };

    // Still things lying over something that moves (a frame over a meter, glass over a knob, a
    // caption on that glass) stay in front of it: layers of their own, not in the background.
    std::vector<bool> layered (layout.widgets.size(), false);

    for (size_t i = 0; i < layout.widgets.size(); ++i)
    {
        const auto& w = layout.widgets[i];

        for (size_t below = 0; isStill (w) && ! isBehindBody ((int) i) && below < i && ! layered[i]; ++below)
        {
            const auto& under = layout.widgets[below];

            if (sharePage (w, under))
                layered[i] = movingPart (under).intersects (w.bounds) || (layered[below] && under.bounds.intersects (w.bounds));
        }
    }

    // Each page's background: the window, and everything still on that page.
    for (int p = 0; p < numPages; ++p)
    {
        shownPage = p;
        juce::Image page (juce::Image::ARGB, juce::jmax (1, result.width), juce::jmax (1, result.height), true);

        {
            juce::Graphics g (page);
            g.setOrigin (shift);
            drawPanel (g, [&] (int i) { drawWidget (g, i); });   // (behind the body: all of it, there)

            for (int i = 0; i < (int) layout.widgets.size(); ++i)
                if (! layered[(size_t) i] && ! isBehindBody (i))
                    drawWidget (g, i, Part::still);
        }

        result.pages.push_back (page);
    }

    // Frames: each drawn over its area, as drawWidget draws it there.
    auto addStrip = [&] (int index, juce::Rectangle<int> area, int frames, const std::function<void (int frame, juce::Graphics&)>& draw)
    {
        ExportBake::Strip strip;
        strip.frameWidth = area.getWidth();
        strip.frameHeight = area.getHeight();
        strip.frames = frames;
        strip.image = juce::Image (juce::Image::ARGB, area.getWidth(), area.getHeight() * frames, true);

        for (int f = 0; f < frames; ++f)
        {
            juce::Graphics g (strip.image);
            g.reduceClipRegion (0, f * area.getHeight(), area.getWidth(), area.getHeight());
            g.setOrigin ({ -area.getX(), f * area.getHeight() - area.getY() });
            draw (f, g);
        }

        juce::ignoreUnused (index);
        result.strips.push_back (strip);
        return (int) result.strips.size() - 1;
    };

    std::map<juce::String, int> lookStrips;   // upright controls with the same look and size share frames

    for (int i = 0; i < (int) layout.widgets.size(); ++i)
    {
        if (isBehindBody (i))
            continue;   // behind the 3D body: in the background, under it

        const auto& w = layout.widgets[(size_t) i];
        const auto page = pageOf (w);
        shownPage = juce::jmax (0, page);   // so it's drawn

        ExportBake::Item item;
        item.widget = i;
        item.page = page;
        item.angle = w.shownAngle();
        item.box = w.bounds;

        const auto moving = movingAreaOf (w);
        const auto drawn = turnedArea (w, moving).getIntersection (window);

        // A still layer in front of something that moves: one frame.
        if (layered[(size_t) i])
        {
            const auto area = turnedArea (w, w.bounds).getIntersection (window);

            if (area.isEmpty())
                continue;

            item.kind = "picture";
            item.area = area;
            item.angle = 0.0f;
            item.strip = addStrip (i, area, 1, [&] (int, juce::Graphics& g) { drawWidget (g, i, Part::still); });
            result.items.push_back (item);
            continue;
        }

        // A performance control: its pictures for where it is, across and up (a pair of wheels:
        // each wheel by itself, both moved up and down).
        if (w.type == GuiWidget::Type::xy && PerformControl::has (w))
        {
            const auto design = PerformControl::designOf (w);
            const auto axes = PerformControl::axesOf (design);
            const auto pad = w.bounds.toFloat().withTrimmedBottom ((float) GuiLayout::captionHeight);
            const auto played = pad.reduced (2.0f).getSmallestIntegerContainer();
            const auto drawnArea = pad.getSmallestIntegerContainer().expanded (14).getIntersection (window);   // room for its shadow
            const auto frames = juce::jlimit (2, 64, PerformControl::framesOf (design));
            const auto springs = PerformControl::modeOf (w) == "bendmod" ? PerformControl::bendAxisOf (w) : juce::String();

            if (drawnArea.isEmpty())
                continue;

            auto roleOf = [&w] (const char* name)
            {
                const auto found = w.roles.find (name);
                return found != w.roles.end() ? found->second : juce::String();
            };

            item.kind = "perform";
            item.angle = 0.0f;
            item.ownParams = true;

            if (axes == "pair")
            {
                const auto middle = juce::roundToInt (pad.getCentreX());

                for (int part = 0; part < 2; ++part)
                {
                    auto wheel = item;
                    wheel.area = part == 0 ? drawnArea.withRight (juce::jmax (drawnArea.getX() + 1, middle)) : drawnArea.withLeft (juce::jmin (drawnArea.getRight() - 1, middle));
                    wheel.box = part == 0 ? played.withRight (juce::jmax (played.getX() + 1, middle)) : played.withLeft (juce::jmin (played.getRight() - 1, middle));
                    wheel.param2 = roleOf (part == 0 ? "x" : "y");
                    wheel.count = 1;
                    wheel.mode = frames;
                    wheel.flags = springs == (part == 0 ? "x" : "y") ? 4 : 0;
                    wheel.strip = addStrip (i, wheel.area, frames, [&] (int f, juce::Graphics& g)
                    {
                        const auto at = (float) f / (float) (frames - 1);
                        PerformControl::paint (g, design, pad, part == 0 ? at : 0.5f, part == 0 ? 0.5f : at, layout.lightAngle, layout.lightAltitude);
                    });

                    result.items.push_back (wheel);
                }

                continue;
            }

            const int across = axes == "y" ? 1 : frames, up = axes == "x" ? 1 : frames;
            item.param = axes != "y" ? roleOf ("x") : juce::String();
            item.param2 = axes != "x" ? roleOf ("y") : juce::String();
            item.count = across;
            item.mode = up;
            item.flags = (springs == "x" ? 2 : 0) | (springs == "y" ? 4 : 0);

            if (springs.isNotEmpty() && PerformControl::springsBoth (design))
                item.flags = 2 | 4;   // a lever: both go back to the middle
            item.area = drawnArea;
            item.box = played;
            item.strip = addStrip (i, drawnArea, across * up, [&] (int f, juce::Graphics& g)
            {
                const auto x = across > 1 ? (float) (f % across) / (float) (across - 1) : 0.5f;
                const auto y = up > 1 ? (float) (f / across) / (float) (up - 1) : 0.5f;
                PerformControl::paint (g, design, pad, x, y, layout.lightAngle, layout.lightAltitude);
            });

            result.items.push_back (item);
            continue;
        }

        switch (w.type)
        {
            case GuiWidget::Type::knob:
            case GuiWidget::Type::slider:
            case GuiWidget::Type::toggle:
            {
                item.kind = w.type == GuiWidget::Type::knob ? "knob" : (w.type == GuiWidget::Type::slider ? "slider" : "toggle");

                // A slider's track is where it's played; knobs and switches anywhere on them.
                if (w.type == GuiWidget::Type::slider)
                    item.box = moving;

                const bool withLook = Looks::takesLook (w) && looks != nullptr;
                auto kind = Looks::Kind();
                const Looks::Look* look = nullptr;
                int room = 0, frames = w.type == GuiWidget::Type::toggle ? 2 : plainControlFrames;

                if (withLook)
                {
                    looks->setLight (layout.lightAngle, layout.lightAltitude);   // lit by the panel's global light
                    kind = Looks::kindOf (w);
                    look = &looks->lookFor (w);
                    room = Looks::overhang (*look, moving.getWidth(), moving.getHeight());
                    frames = juce::jmax (1, Looks::framesFor (*look, kind));
                }

                // Upright and solid: the look's own frames, shared by the controls like it.
                if (withLook && ! w.isTurned() && w.shownOpacity() >= 0.999f && ! moving.isEmpty())
                {
                    item.area = moving.expanded (room);
                    const auto lean = Looks::leanFor (*look, layout.cameraLean (moving));   // a 3D look: where the panel camera sees it from
                    const auto slope = Looks::slopeFor (*look, look->doc.model3d != nullptr ? faceSlope (moving) : juce::Point<float>());   // ...on the body's face
                    const auto key = juce::String (look->revision) + "|" + Looks::kindName (kind) + "|"
                                   + juce::String (moving.getWidth()) + "x" + juce::String (moving.getHeight()) + Looks::leanKey (lean)
                                   + Looks::slopeKey (slope);

                    if (const auto found = lookStrips.find (key); found != lookStrips.end())
                    {
                        item.strip = found->second;
                    }
                    else
                    {
                        ExportBake::Strip strip;
                        strip.image = looks->strip (*look, kind, moving.getWidth(), moving.getHeight(), lean, slope);
                        strip.frameWidth = moving.getWidth() + 2 * room;
                        strip.frameHeight = moving.getHeight() + 2 * room;
                        strip.frames = frames;
                        result.strips.push_back (strip);
                        item.strip = lookStrips[key] = (int) result.strips.size() - 1;
                    }

                    result.items.push_back (item);
                    break;
                }

                // Turned or faded: each frame drawn as it shows, turned and faded.
                item.area = turnedArea (w, moving.expanded (room)).getIntersection (window);

                if (item.area.isEmpty())
                    break;

                item.strip = addStrip (i, item.area, frames, [&] (int f, juce::Graphics& g)
                {
                    drawWidget (g, i, Part::moving, frames > 1 ? (float) f / (float) (frames - 1) : 0.0f);
                });

                result.items.push_back (item);
                break;
            }

            case GuiWidget::Type::selector:
            {
                // A set of keys: the set all off, then each key lit by itself over it.
                if (SelectorSet::has (w))
                {
                    const auto area = w.bounds.toFloat().withTrimmedBottom ((float) GuiLayout::captionHeight);
                    const auto design = SelectorSet::designOf (w);
                    const auto mode = SelectorSet::modeOf (w);
                    const auto keys = selectorKeys (w);
                    juce::StringArray names;

                    for (int k = 0; k < keys; ++k)
                        names.add (k < w.options.size() ? w.options[k] : juce::String (k + 1));

                    auto paintSet = [&] (juce::Graphics& g, int lit)
                    {
                        std::vector<bool> on ((size_t) keys, false);

                        if (juce::isPositiveAndBelow (lit, keys))
                            on[(size_t) lit] = true;

                        SelectorSet::paint (g, design, area, names, on, layout.lightAngle, layout.lightAltitude);
                    };

                    const auto whole = area.getSmallestIntegerContainer().expanded (8).getIntersection (window);

                    if (whole.isEmpty())
                        break;

                    item.kind = "picture";
                    item.area = item.box = whole;
                    item.angle = 0.0f;
                    item.strip = addStrip (i, whole, 1, [&] (int, juce::Graphics& g) { paintSet (g, -1); });
                    result.items.push_back (item);

                    const auto shapes = SelectorSet::layout (design, { area.getWidth(), area.getHeight() }, keys);
                    const auto toArea = juce::AffineTransform::translation (area.getX(), area.getY());

                    for (int k = 0; k < (int) shapes.size(); ++k)
                    {
                        // What lights: the key, its glow, its name (beside it, too).
                        auto region = shapes[(size_t) k].region;
                        region.addPath (shapes[(size_t) k].outline);

                        if (! shapes[(size_t) k].label.isEmpty())
                            region.addRectangle (shapes[(size_t) k].label);

                        region.applyTransform (toArea);
                        const auto keyArea = region.getBounds().getSmallestIntegerContainer().expanded (1).getIntersection (window);

                        if (keyArea.isEmpty())
                            continue;

                        auto key = item;
                        key.kind = "key";
                        key.area = keyArea;
                        key.box = shapes[(size_t) k].outline.getBoundsTransformed (toArea).getSmallestIntegerContainer().getIntersection (window);
                        key.count = k;
                        key.mode = mode == "toggles" ? 1 : (mode == "multi" ? 2 : 0);

                        if (mode == "toggles")
                        {
                            key.ownParams = true;
                            key.param = SelectorSet::keyParam (w, k);
                        }

                        key.strip = addStrip (i, keyArea, 2, [&] (int f, juce::Graphics& g)
                        {
                            if (f == 1)
                            {
                                g.reduceClipRegion (region);
                                paintSet (g, k);
                            }
                        });

                        result.items.push_back (key);
                    }

                    break;
                }

                const auto* p = paramFor (w);
                const auto count = juce::jmax (1, w.options.size() > 0 ? w.options.size() : (p != nullptr ? (int) (p->max - p->min) + 1 : 1));

                item.kind = "selector";
                item.count = count;
                item.box = moving;
                item.area = drawn;

                // How a click picks a position: across, down, or with arrows (PanelParts::selectorPick).
                const auto box = moving.toFloat();

                if (const auto display = PanelParts::selectorDisplayOf (w); display == "vfd" || display == "dropdown")
                    item.mode = 2;     // an exported drop-down list steps as a vfd does (its runtime has no menus)
                else if (count > 1)
                    item.mode = PanelParts::selectorPick (w, box, count, 0, { box.getX() + 1.0f, box.getBottom() - 1.0f }) == count - 1 ? 1 : 0;

                if (item.area.isEmpty())
                    break;

                item.strip = addStrip (i, item.area, count, [&] (int f, juce::Graphics& g) { drawWidget (g, i, Part::moving, (float) f); });
                result.items.push_back (item);
                break;
            }

            case GuiWidget::Type::preset:
            {
                // A design of its own: the built-in PresetPanel element draws it, with its keys
                // as pictures baked here.
                if (PresetPanel::has (w))
                {
                    const auto d = PresetPanel::designOf (w);
                    const auto style = PresetPanel::styleOf (d);
                    const bool side = PresetPanel::isSide (d), onPanel = side || (style == "table" && ! PresetPanel::opensPopup (d));
                    const auto box = (side ? PresetPanel::moduleArea (d, layout.width, layout.height) : w.bounds).getIntersection (window);

                    if (box.isEmpty())
                        break;

                    const auto keys = d.getProperty ("keys", {});
                    const auto prefix = "presets" + juce::String (i);
                    auto element = std::make_shared<GuiWidget>();
                    element->type = GuiWidget::Type::custom;
                    element->element = "PresetPanel";
                    element->label = w.label;
                    element->font = w.font;

                    auto& set = element->settings;
                    set["style"] = style;
                    set["open"] = PresetPanel::opensPopup (d) ? "popup" : "panel";
                    set["categories"] = d.getProperty ("categories", side ? "tabs" : (style == "list" ? "headings" : "none"));
                    set["title"] = d.getProperty ("title", "PRESETS");
                    set["side"] = PresetPanel::onLeft (d) ? "left" : "right";
                    set["plate"] = (bool) d.getProperty ("plate", true) ? 1 : 0;
                    set["pictures"] = prefix;

                    for (const auto* key : { "columns", "rows", "rowHeight", "listRows" })
                        if (d.hasProperty (key))
                            set[key] = d.getProperty (key, {});

                    for (const auto* key : { "colour", "panelColour", "textColour", "lineColour" })
                        if (d.hasProperty (key))
                            set[key] = d.getProperty (key, {});

                    for (const auto* key : { "labelColour", "labelOnColour" })
                        if (keys.hasProperty (key))
                            set[key] = keys.getProperty (key, {});

                    if (keys.hasProperty ("textSize"))
                        set["keyTextSize"] = keys.getProperty ("textSize", {});

                    // Its bar in LED digits (SegmentDisplay's settings, "led" before them).
                    for (const auto* key : { "barDisplay", "ledNumber", "ledColour", "ledBackground", "ledSegments", "ledCells", "ledGhost",
                                             "ledGlow", "ledSlant", "ledThickness", "ledWindow" })
                        if (d.hasProperty (key))
                            set[key] = d.getProperty (key, {});

                    // Its keys at the sizes the element sets them out at.
                    const auto columns = juce::jlimit (1, 8, (int) d.getProperty ("columns", 3));
                    const auto rows = juce::jlimit (1, 12, (int) d.getProperty ("rows", 4));
                    const auto rowHeight = juce::jlimit (14, 60, (int) d.getProperty ("rowHeight", 26));
                    const auto pad = side ? 10 : 8;

                    auto add = [&] (const juce::String& name, const juce::String& where, int width, int height, bool lit)
                    {
                        if (width > 0 && height > 0)
                            result.pictures.push_back ({ prefix + "_" + name + ".png",
                                                         PresetPanel::keyPicture (d, where, width, height, lit, layout.lightAngle, layout.lightAltitude) });
                    };

                    if (! onPanel)
                        add ("bar", "bar", box.getWidth(), box.getHeight(), false);

                    if (style == "table")
                    {
                        const auto across = PresetPanel::opensPopup (d) ? juce::jmax (box.getWidth(), columns * 104 + 16) : box.getWidth();
                        const auto down = PresetPanel::opensPopup (d) ? rows * 40 : box.getHeight() - 2 * pad - 26;
                        const auto cellWidth = juce::roundToInt ((float) (across - 2 * pad) / (float) columns);
                        const auto cellHeight = juce::roundToInt ((float) down / (float) rows);
                        add ("cell", "cell", cellWidth, cellHeight, false);
                        add ("cellOn", "cell", cellWidth, cellHeight, true);
                    }
                    else if (style != "bar")
                    {
                        const auto across = side ? box.getWidth() : juce::jmax (box.getWidth(), 190);
                        add ("row", "row", across - 2 * pad, rowHeight, false);
                        add ("rowOn", "row", across - 2 * pad, rowHeight, true);
                    }

                    item.kind = "custom";
                    item.area = item.box = box;
                    item.angle = 0.0f;
                    item.element = element;
                    result.items.push_back (item);
                    break;
                }

                // Upright: the PresetPanel element as "<  name  >" (its picture without the name),
                // so it has the user's presets too, and the commands (Save, Load, the banks...).
                if (std::abs (w.shownAngle()) < 0.01f)
                {
                    const auto box = w.bounds.getIntersection (window);

                    if (box.isEmpty())
                        break;

                    const auto prefix = "presets" + juce::String (i);
                    auto element = std::make_shared<GuiWidget>();
                    element->type = GuiWidget::Type::custom;
                    element->element = "PresetPanel";
                    element->label = w.label;
                    element->font = w.font;

                    auto& set = element->settings;
                    set["style"] = "bar";
                    set["bareBar"] = 1;
                    set["pictures"] = prefix;
                    set["labelColour"] = "#" + (w.textColour.isTransparent() ? PanelParts::standardCaptionColour() : w.textColour).toDisplayString (true);

                    if (w.fontSize > 0.0f)
                        set["barTextSize"] = w.fontSize;

                    juce::Image picture (juce::Image::ARGB, box.getWidth(), box.getHeight(), true);

                    {
                        juce::Graphics g (picture);
                        g.setOrigin (-box.getPosition());
                        const juce::ScopedValueSetter<bool> bare (bareBars, true);
                        drawWidget (g, i, Part::moving, -1.0f);
                    }

                    result.pictures.push_back ({ prefix + "_bar.png", picture });
                    item.kind = "custom";
                    item.area = item.box = box;
                    item.angle = 0.0f;
                    item.element = element;
                    result.items.push_back (item);
                    break;
                }

                // Turned: one frame per preset, then one with none chosen.
                item.kind = "preset";
                item.count = presetNames.size();
                item.area = turnedArea (w, w.bounds).getIntersection (window);

                if (item.area.isEmpty())
                    break;

                item.strip = addStrip (i, item.area, presetNames.size() + 1, [&] (int f, juce::Graphics& g)
                {
                    drawWidget (g, i, Part::moving, f < presetNames.size() ? (float) f : -1.0f);
                });

                result.items.push_back (item);
                break;
            }

            // The modulation matrix: the built-in ModMatrix element draws it (its routes its
            // roles src1, dst1, amt1...). Its parts lying on their own aren't exported.
            case GuiWidget::Type::matrix:
            {
                if (ModMatrix::isPart (w))
                    break;

                const auto box = w.bounds.getIntersection (window);

                if (box.isEmpty())
                    break;

                const auto d = ModMatrix::designOf (w);
                const auto view = matrixView (w);
                const auto routes = ModMatrix::routesOf (d);
                const auto style = ModMatrix::styleOf (d);
                auto element = std::make_shared<GuiWidget>();
                element->type = GuiWidget::Type::custom;
                element->element = "ModMatrix";
                element->label = w.label;
                element->font = w.font;

                juce::StringArray columns, cables;

                for (const auto c : view.columns)
                    columns.add (juce::String (c));

                for (int r = 0; r < routes; ++r)
                    cables.add (ModMatrix::cableColour (d, r).toDisplayString (true));

                auto& set = element->settings;
                set["style"] = style;
                set["cell"] = d.getProperty ("cell", "pin");
                set["routes"] = routes;
                set["sources"] = view.sources.joinIntoString ("\n");
                set["destinations"] = view.destinations.joinIntoString ("\n");
                set["columns"] = columns.joinIntoString (",");
                set["cables"] = cables.joinIntoString (",");
                set["sag"] = d.getProperty ("sag", 40);
                set["header"] = (bool) d.getProperty ("header", true) ? 1 : 0;
                set["amount"] = (double) d.getProperty ("amount", style == "patch" ? 1.0 : 0.5);

                for (const auto* key : { "colour", "panelColour", "textColour", "lineColour" })
                    if (d.hasProperty (key))
                        set[key] = d.getProperty (key, {});

                for (int r = 0; r < routes; ++r)
                    for (const auto* part : { "src", "dst", "amt" })
                        if (const auto id = ModMatrix::paramId (w, r, part); id.isNotEmpty())
                            element->roles[part + juce::String (r + 1)] = id;

                // A patch bay's jacks, as its design draws them.
                if (style == "patch")
                {
                    const auto size = ModMatrix::jackSize (d, box.toFloat().withZeroOrigin(), view) * 0.9f;
                    const auto side = juce::jmax (4, (int) std::ceil (size * 1.5f));

                    for (const bool lit : { false, true })
                    {
                        juce::Image jack (juce::Image::ARGB, side, side, true);
                        juce::Graphics g (jack);
                        ModMatrix::paintJackAt (g, d, { (float) side * 0.5f, (float) side * 0.5f }, size, lit);
                        const auto name = "matrix" + juce::String (i) + (lit ? "_jackOn.png" : "_jackOff.png");
                        result.pictures.push_back ({ name, jack });
                        set[lit ? "jackOn" : "jackOff"] = name;
                    }
                }

                item.kind = "custom";
                item.area = item.box = box;
                item.angle = 0.0f;
                item.element = element;
                result.items.push_back (item);
                break;
            }

            case GuiWidget::Type::tabs:
            {
                // A frame per page, its own tab lit.
                item.kind = "tabs";
                item.count = numPages;
                item.area = turnedArea (w, w.bounds).getIntersection (window);

                if (item.area.isEmpty())
                    break;

                item.strip = addStrip (i, item.area, numPages, [&] (int f, juce::Graphics& g)
                {
                    shownPage = f;
                    drawWidget (g, i, Part::moving);
                    shownPage = juce::jmax (0, page);
                });

                result.items.push_back (item);
                break;
            }

            case GuiWidget::Type::meter:
            {
                // Its look at each level (a needle where the level puts it), the peak left out.
                const bool isDisplay = displaySources.contains (w.source);
                item.kind = "meter";
                item.mode = (w.mode == "rms" ? 1 : 0) | (MeterPaint::hasNeedle (MeterPaint::styleOf (w)) ? 2 : 0);
                item.area = drawn;

                if (item.area.isEmpty())
                    break;

                item.strip = addStrip (i, item.area, meterFrames, [&] (int f, juce::Graphics& g)
                {
                    const auto fraction = (float) f / (float) (meterFrames - 1);
                    meterLevels[i] = levelAt (fraction, isDisplay);
                    meterNeedles[i] = fraction;
                    meterPeaks.erase (i);
                    drawWidget (g, i, Part::moving);
                });

                result.items.push_back (item);
                break;
            }

            case GuiWidget::Type::lamp:
            {
                // Off and on; or, following the level, lit by steps.
                const auto follows = PanelParts::lampFollows (w);
                const auto frames = follows ? lampFrames : 2;
                item.kind = "lamp";
                item.mode = follows ? 1 : 0;
                item.area = drawn;

                if (item.area.isEmpty())
                    break;

                item.strip = addStrip (i, item.area, frames, [&] (int f, juce::Graphics& g)
                {
                    drawWidget (g, i, Part::moving, (float) f / (float) (frames - 1));
                });

                result.items.push_back (item);
                break;
            }

            case GuiWidget::Type::label:
            {
                // A readout: its value at each step of its parameter (words alone are still).
                const auto* p = paramFor (w);

                if (! PanelParts::labelMoves (w) || p == nullptr)
                    break;

                item.kind = "readout";
                item.area = turnedArea (w, w.bounds).getIntersection (window);

                if (item.area.isEmpty())
                    break;

                item.strip = addStrip (i, item.area, readoutFrames, [&] (int f, juce::Graphics& g)
                {
                    values[p->index] = valueAt (*p, (float) f / (float) (readoutFrames - 1));
                    drawWidget (g, i, Part::moving);
                });

                result.items.push_back (item);
                break;
            }

            // Drawn live by the plugin, upright.
            case GuiWidget::Type::scope:
            case GuiWidget::Type::envelope:
            case GuiWidget::Type::filter:
            case GuiWidget::Type::xy:
            case GuiWidget::Type::keyboard:
            case GuiWidget::Type::custom:
            {
                item.kind = w.type == GuiWidget::Type::scope ? "scope"
                          : w.type == GuiWidget::Type::envelope ? "envelope"
                          : w.type == GuiWidget::Type::filter ? "filter"
                          : w.type == GuiWidget::Type::xy ? "xy"
                          : w.type == GuiWidget::Type::keyboard ? "keyboard" : "custom";
                item.area = moving;
                item.box = moving;
                item.angle = 0.0f;

                if (w.type == GuiWidget::Type::keyboard)
                {
                    item.count = w.highNote;
                    item.mode = w.lowNote;
                }
                else if (w.type == GuiWidget::Type::filter)
                {
                    item.mode = juce::jmax (0, juce::StringArray { "lowpass", "lowpass24", "highpass", "highpass24", "bandpass" }.indexOf (w.mode));
                }

                if (! item.area.isEmpty())
                    result.items.push_back (item);

                break;
            }

            // The registration badge: as the panel shows it until the plugin is registered, then
            // gone (as it goes in Sonify), the plugin picking the frame. Clicking it opens the
            // plugin's Registration. Export keeps it only in a protected plugin (PluginExporter).
            case GuiWidget::Type::registration:
            {
                item.kind = "registration";
                item.count = 2;
                item.area = w.bounds.getIntersection (window);
                item.box = item.area;
                item.angle = 0.0f;

                if (item.area.isEmpty())
                    break;

                item.strip = addStrip (i, item.area, 2, [&] (int f, juce::Graphics& g)
                {
                    if (f == 0)
                        drawRegistration (g, w);   // 0: not registered; 1: registered, nothing shows
                });

                result.items.push_back (item);
                break;
            }

            // In the background (or a layer of their own, above).
            case GuiWidget::Type::group:
            case GuiWidget::Type::shape:
            case GuiWidget::Type::image:
                break;
        }
    }

    // Where it all is in the window, side panels and all.
    for (auto& item : result.items)
    {
        item.area.translate (shift.x, shift.y);
        item.box.translate (shift.x, shift.y);
    }

    presetMenu = savedPresets;
    values = savedValues;
    meterLevels = savedLevels;
    meterNeedles = savedNeedles;
    meterPeaks = savedPeaks;
    return result;
}
