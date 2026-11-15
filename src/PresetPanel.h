// D:\workspace\Fanan_web_hub\src\PresetPanel.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GuiLayout.h"

#include <vector>

//==============================================================================
/**
    Preset managers: a preset element (GuiWidget::Type::preset) wearing a design of its own,
    in one of four styles, its buttons flat (2D) or 3D keys (SelectorSet), lit when chosen:

        "bar"       the classic: < name >, the arrows stepping, a click on the name opening
                    the presets' menu
        "list"      a drop list: the bar, and under it (or over it, where there's no room)
                    a styled list: search, the categories, the presets a page at a time, the
                    Save, Save as, Load and More keys
        "table"     a table of preset buttons: on the panel itself ("open": "panel") or in a
                    popup the bar opens ("open": "popup"), a page at a time
        "side"      a side panel: a module joined to the window's left or right side ("side"),
                    the window wider by its "width"; always there, or opened and closed by a
                    handle on the window's edge ("toggle", "startOpen")

    A preset element wears one when its "settings" have "presets" (the design), every key
    optional:
        "style"         bar, list, table, side
        "open"          table: "panel" or "popup"
        "side"          side: "left" or "right"; "width" (px), "toggle", "startOpen"
        "columns", "rows"   table: the buttons across and down (a page)
        "rowHeight"     list and side: a row's height (px)
        "listRows"      list: how many rows the list shows at most
        "categories"    "tabs" (a tab strip: All and each category), "headings" (each
                        category's name over its presets: list and side), "none"
        "search"        list and side: a search field
        "actions"       Save, Save as, Load and More (a table: a "..." beside its pages)
        "title"         side: its title
        "plate"         a table on the panel: a plate under it
        "colour", "panelColour", "textColour", "lineColour", "font", "textSize" %
        "keys"          its buttons (and the bar): a SelectorSet design (render 2d / 3d,
                        shape, rounding, gap, colours, material, height...)
        "barDisplay"    "keys" (the standard: the name on the bar) or "led": the preset's
                        number and name in LED digits (SegmentDisplay) between the arrows,
                        "ledNumber" digits (2; 0 none), and SegmentDisplay's settings with
                        "led" before them: "ledColour" (the standard: its "colour"),
                        "ledSegments", "ledCells", "ledGhost", "ledGlow", "ledSlant",
                        "ledThickness", "ledWindow", "ledBackground"
*/
namespace PresetPanel
{
    bool has (const GuiWidget& preset);
    juce::var designOf (const GuiWidget& preset);
    juce::String styleOf (const juce::var& design);   // bar, list, table, side
    bool opensPopup (const juce::var& design);          // a list, a table in a popup
    bool isSide (const juce::var& design);
    bool onLeft (const juce::var& design);
    int sideWidth (const juce::var& design);
    bool toggles (const juce::var& design);
    bool startsOpen (const juce::var& design);

    /** What it shows: the presets (and their categories), the chosen one, the page, the
        category tab, the search (and whether it's being typed). */
    struct View
    {
        juce::StringArray names, groups;
        int current = -1;
        bool modified = false;
        int page = 0, tab = 0;
        juce::String search;
        bool searching = false;
    };

    struct Hit
    {
        enum class Kind { none, body, prev, next, open, preset, tab, search, pagePrev, pageNext, action, more };
        Kind kind = Kind::none;
        int index = -1;      // a preset's, a tab's, an action's (0 Save, 1 Save as, 2 Load, 3 More)
    };

    /** The bar: < name (v) >. */
    void paintBar (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, const View& view,
                   float lightAngle, float lightAltitude, float resolution = 1.0f);
    Hit hitBar (const juce::var& design, juce::Rectangle<float> area, juce::Point<float> at);

    /** The list, the table or the side panel: in its area (a popup's, the element's, the
        module's). */
    void paintBody (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, const View& view,
                    float lightAngle, float lightAltitude, float resolution = 1.0f);
    Hit hitBody (const juce::var& design, juce::Rectangle<float> area, const View& view, juce::Point<float> at);
    int pagesOf (const juce::var& design, juce::Rectangle<float> area, const View& view);
    juce::Rectangle<float> searchArea (const juce::var& design, juce::Rectangle<float> area, const View& view);
    juce::StringArray tabsOf (const View& view);        // "All" and each category

    /** A list's or a table's popup, under the bar (over it, where there's no room) within the
        window. */
    juce::Rectangle<float> popupArea (const juce::var& design, juce::Rectangle<float> bar, juce::Rectangle<float> window, const View& view);

    /** A side panel's module beside a window of this size; its handle on the window's edge. */
    juce::Rectangle<int> moduleArea (const juce::var& design, int windowWidth, int windowHeight);
    juce::Rectangle<float> handleArea (const juce::var& design, int windowWidth, int windowHeight);
    void paintHandle (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, bool open);

    /** Whether its bar shows LED digits; what they say (its number and name) and in how many
        cells (as many as its longest). */
    bool barIsLed (const juce::var& design);
    juce::String ledText (const juce::var& design, const View& view, int& cells);

    /** One of its keys alone (no name on it), as its list ("row"), its table ("cell") or its bar
        ("bar") draws it at that size, lit or not: what an exported plugin draws its keys with. */
    juce::Image keyPicture (const juce::var& design, const juce::String& where, int width, int height, bool lit,
                            float lightAngle, float lightAltitude);

    /** The gallery's templates. */
    juce::StringArray templateNames();
    juce::var makeTemplate (int index);

    /** Made-up presets for pictures and a panel with none yet. */
    View sampleView();

    juce::File galleryFolder();
    constexpr const char* fileExtension = ".fkpresets";
}
