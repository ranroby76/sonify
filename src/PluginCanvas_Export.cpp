// D:\workspace\Fanan_web_hub\src\PluginCanvas_Export.cpp
// The canvas baked for Export to plugin files (PluginExporter): each page's background, and what
// moves as strips of frames drawn the way the panel draws it, so the exported plugin's window
// looks as the panel does in Play.

#include "PluginCanvas.h"
#include "MeterPaint.h"
#include "PanelParts.h"

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

    ExportBake result;
    result.width = layout.width;
    result.height = layout.height;
    const juce::Rectangle<int> window (0, 0, layout.width, layout.height);
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

        for (size_t below = 0; isStill (w) && below < i && ! layered[i]; ++below)
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
        juce::Image page (juce::Image::ARGB, juce::jmax (1, layout.width), juce::jmax (1, layout.height), true);

        {
            juce::Graphics g (page);
            drawPanel (g);

            for (int i = 0; i < (int) layout.widgets.size(); ++i)
                if (! layered[(size_t) i])
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
                    const auto key = juce::String (look->revision) + "|" + Looks::kindName (kind) + "|"
                                   + juce::String (moving.getWidth()) + "x" + juce::String (moving.getHeight());

                    if (const auto found = lookStrips.find (key); found != lookStrips.end())
                    {
                        item.strip = found->second;
                    }
                    else
                    {
                        ExportBake::Strip strip;
                        strip.image = looks->strip (*look, kind, moving.getWidth(), moving.getHeight());
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
                const auto* p = paramFor (w);
                const auto count = juce::jmax (1, w.options.size() > 0 ? w.options.size() : (p != nullptr ? (int) (p->max - p->min) + 1 : 1));

                item.kind = "selector";
                item.count = count;
                item.box = moving;
                item.area = drawn;

                // How a click picks a position: across, down, or with arrows (PanelParts::selectorPick).
                const auto box = moving.toFloat();

                if (PanelParts::selectorDisplayOf (w) == "vfd")
                    item.mode = 2;
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
                // One frame per preset, then one with none chosen.
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

    presetMenu = savedPresets;
    values = savedValues;
    meterLevels = savedLevels;
    meterNeedles = savedNeedles;
    meterPeaks = savedPeaks;
    return result;
}
