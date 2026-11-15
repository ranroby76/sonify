// D:\workspace\Fanan_web_hub\src\GalleryHub.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PluginCanvas;

//==============================================================================
/**
    The Gallery window: every gallery in one place, a button each down its left side:
        Knobs, sliders, switches   the Knob Gallery (drag one onto the window or a control)
        3D models                  the 3D Gallery
        Selector sets              the Selector Studio's sets (SelectorSet)
        Performance controls       wheels, levers, joysticks, pads, ribbons (PerformControl)
        Mod matrices               slots, grids, patch bays and their parts (ModMatrix)
        Preset managers            bars, drop lists, tables, side panels (PresetPanel)
        3D bodies                  whole cases (Body3D, the Body Editor's)
        Icons                      the icon library (IconLibrary)
        Guitar FX icons            the guitar effects' icons on badges (a rectangle, a circle, a
                                   rhombus, a polygon...), each family its colour: dropped, an Icon element wearing it
        Shapes                     the Knob Studio's kept shapes, put on the panel as pictures
        Material looks             the built-in looks (MaterialArchetypes): a panel's material, or
                                   the whole window's, with its controls, displays and painted panel
        Layouts                    the ready-made layouts (PanelLayouts): Fanan AI lays the window
                                   out from one, with the plugin's own controls
    Everything's dragged onto the window: a design dropped on an element of its kind is worn
    by it, elsewhere it's a new element there; a body dropped anywhere is the window's body;
    an icon dropped on an Icon element is its icon, elsewhere a new Icon. A design's menu
    (right-click) also puts it on the selected element or adds it, shows its file, or takes
    one kept there out.

    One window, opened (or brought forward, on the page asked for) by show().
*/
class GalleryHub final : public juce::Component,
                         public juce::DragAndDropContainer
{
public:
    enum class Page { knobs, models3d, selectorSets, perform, matrix, presets, bodies, icons, fxIcons, shapes, materialLooks, layouts };

    GalleryHub (PluginCanvas& canvas, Page page);
    ~GalleryHub() override;

    /** The Gallery window, on a page (opened beside near, or brought forward). */
    static void show (PluginCanvas& canvas, Page page, juce::Component* near = nullptr);

    void showPage (Page page);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int buttonsWidth = 176;

private:
    class Designs;

    PluginCanvas& canvas;
    Page page = Page::knobs;
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::Component> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GalleryHub)
};
