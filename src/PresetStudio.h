// D:\workspace\Fanan_web_hub\src\PresetStudio.h

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "UiHelpers.h"   // FineSlider

#include "ColourSwatch.h"
#include "GuiLayout.h"
#include "PresetPanel.h"

class PluginCanvas;
namespace Render3D { struct ScopedDraft; }

//==============================================================================
/**
    The Preset Studio: one preset manager's design (PresetPanel), in a window of its own beside
    the panel, which shows every change as it's made.

    Its gallery is in the Gallery window (GalleryHub, the Gallery button across the top): the templates (drop lists dark, light and with 3D keys,
    tables on the panel and in popups, side panels left and right, opening and closing, the
    classic bar) and the ones kept there (.fkpresets), as pictures; click one and the preset
    manager wears it. In the middle it as the window shows it (a list or a popup table open
    under its bar); on the right its settings: its style, where a table opens, a side panel's
    side, width and handle, its pages, categories, search and Save / Load keys, its buttons
    (flat or 3D: shape, colours, material) and its colours and font.
*/
class PresetStudio final : public juce::Component,
                           private juce::Timer
{
public:
    PresetStudio (PluginCanvas& canvas, int widgetIndex);
    ~PresetStudio() override;

    static std::unique_ptr<juce::DocumentWindow> makeWindow (PluginCanvas& canvas, int widgetIndex);

    /** A design's picture as the Gallery window shows it (GalleryHub). */
    void static paintGalleryPicture (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, float lightAngle, float lightAltitude);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Preview;
    class Rows;
    struct Links;

    PluginCanvas& canvas;
    int widget = -1;
    bool updating = false;
    std::unique_ptr<Render3D::ScopedDraft> dragDraft;   // while a setting's dragged: drawn quickly, sharp when let go
    juce::String lastSeen;
    std::unique_ptr<Links> links;

    const GuiLayout& panel() const;
    GuiWidget* element() const;
    juce::var design() const;
    juce::var& ensureDesign();
    juce::String stateKey() const;
    PresetPanel::View view() const;    // the plugin's presets (made-up ones while it has none)

    juce::var valueOf (const juce::String& key) const;     // "keys.<key>": its buttons' design
    void set (const juce::String& key, const juce::var& value, bool final = true);
    void changed (bool final);
    void fitBounds();                                       // its box as its style needs
    void refresh();
    void showRows();
    void useDesign (const juce::var& newDesign);
    void timerCallback() override;
    void saveToGallery();

    juce::TextButton keepButton { juce::String::fromUTF8 ("Keep in gallery\xe2\x80\xa6") };

    juce::TextButton galleryButton { juce::String::fromUTF8 ("Gallery\xe2\x80\xa6") };   // the Gallery window, on this one's page
    std::unique_ptr<Preview> preview;
    std::unique_ptr<Rows> rows;
    juce::Viewport rowsView;

    juce::ComboBox styleBox, openBox, sideBox, categoriesBox, renderBox, keyStyleBox, shapeBox, materialBox, fontBox;
    juce::ToggleButton toggleToggle { "Opens and closes (a handle on the window's edge)" }, startOpenToggle { "Open when the plugin opens" },
                       searchToggle { "A search field" }, actionsToggle { "Save, Save as, Load, More" }, plateToggle { "A plate under it" };
    juce::TextEditor titleBox;
    FineSlider widthSlider, columnsSlider, rowsSlider, rowHeightSlider, listRowsSlider,
                 roundingSlider, gapSlider, heightSlider, glowSlider, keyTextSlider, textSizeSlider;
    ColourSwatchButton capSwatch { "Key" }, capOnSwatch { "Chosen" }, glowSwatch { "Glow" }, labelSwatch { "Name" }, labelOnSwatch { "Chosen name" },
                       accentSwatch { "Accent" }, panelSwatch { "Panel" }, textSwatch { "Text" }, lineSwatch { "Lines" };

    // Its bar as LED digits (SegmentDisplay): its number and name glowing between the arrows.
    juce::ComboBox barDisplayBox, ledSegmentsBox, ledWindowBox;
    FineSlider ledNumberSlider, ledGhostSlider, ledGlowSlider, ledSlantSlider, ledThicknessSlider;
    ColourSwatchButton ledSwatch { "Digits" }, ledBackSwatch { "Window" };

    void setUpRows();
    void setUpSlider (juce::Slider&, double from, double to, double step, double fallback, const juce::String& key, const juce::String& tip, const juce::String& suffix = {});
    void setUpCombo (juce::ComboBox&, const juce::StringArray& captions, const juce::StringArray& values, const juce::String& fallback, const juce::String& key, const juce::String& tip);
    void setUpToggle (juce::ToggleButton&, bool fallback, const juce::String& key, const juce::String& tip);
    void setUpSwatch (ColourSwatchButton&, juce::Colour fallback, const juce::String& key, const juce::String& tip);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetStudio)
};
