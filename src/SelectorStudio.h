// D:\workspace\Fanan_web_hub\src\SelectorStudio.h

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "UiHelpers.h"   // FineSlider

#include "ColourSwatch.h"
#include "GuiLayout.h"

class PluginCanvas;
namespace Render3D { struct ScopedDraft; }

//==============================================================================
/**
    The Selector Studio: one selector's set of keys (SelectorSet), in a window of its own
    beside the panel, which shows every change as it's made.

    Its gallery is in the Gallery window (GalleryHub, the Gallery button across the top): the templates (keys round the bottom of a knob, a label
    ring, square keys with lamps, zipped triangles, a tab strip, a step row, a pad grid, a
    D-pad, a rocker...) and the sets kept there (.fksel), as pictures; click one and the
    selector wears it, its names and behaviour kept. In the middle the set as it looks, its
    keys working the way they behave; on the right its settings. Across the top how its keys
    behave (one at a time, each on its own, several at once), the gallery's Keep, a set's
    file opened or saved, and back to a plain selector.
*/
class SelectorStudio final : public juce::Component,
                             private juce::Timer
{
public:
    SelectorStudio (PluginCanvas& canvas, int widgetIndex);
    ~SelectorStudio() override;

    static std::unique_ptr<juce::DocumentWindow> makeWindow (PluginCanvas& canvas, int widgetIndex);

    /** A design's picture as the Gallery window shows it (GalleryHub). */
    void static paintGalleryPicture (juce::Graphics& g, const juce::var& design, int keys, juce::Rectangle<float> area, float lightAngle, float lightAltitude);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Preview;
    class Rows;

    PluginCanvas& canvas;
    int widget = -1;

    // The panel it's on (its colours and light, the selector's size).
    const GuiLayout& panel() const;

    // What each setting edits.
    struct Links;
    std::unique_ptr<Links> links;
    bool updating = false;
    std::unique_ptr<Render3D::ScopedDraft> dragDraft;   // while a setting's dragged: drawn quickly, sharp when let go
    juce::String lastSeen;

    // What it edits.
    bool alive() const;
    juce::var design() const;          // the selector's set (void: none yet)
    juce::var& ensureDesign();         // ...made when there's none
    int keys() const;
    juce::StringArray names() const;
    juce::String mode() const;
    void setMode (const juce::String& mode);

    void set (const juce::Identifier& key, const juce::var& value, bool final = true);
    void changed (bool final);
    void refresh();
    void showRows();
    void useDesign (const juce::var& newDesign);
    void timerCallback() override;

    void saveToGallery();
    void openFile();
    void saveFile();

    // Across the top.
    juce::ComboBox behaviourBox;
    juce::Label behaviourLabel;
    juce::TextButton keepButton { juce::String::fromUTF8 ("Keep in gallery\xe2\x80\xa6") };
    juce::TextButton openButton { juce::String::fromUTF8 ("Open\xe2\x80\xa6") };
    juce::TextButton saveButton { juce::String::fromUTF8 ("Save as\xe2\x80\xa6") };
    juce::TextButton plainButton { "Plain selector" };

    juce::TextButton galleryButton { juce::String::fromUTF8 ("Gallery\xe2\x80\xa6") };   // the Gallery window, on this one's page
    std::unique_ptr<Preview> preview;
    std::unique_ptr<Rows> rows;
    juce::Viewport rowsView;

    // The set's settings.
    juce::TextEditor namesBox;
    juce::ComboBox renderBox, styleBox, formationBox, shapeBox, labelsBox, fontBox, ledBox, materialBox;

    // Its text: in or out of the keys (and which side), straight or following them, its font,
    // size, spacing and colours, and Photoshop's layer style on it.
    juce::ComboBox labelSideBox, labelAngleBox;
    juce::ToggleButton italicToggle { "Italic" }, upperToggle { "CAPITALS" }, labelGlowToggle { "A lit key's name glows" };
    FineSlider spacingSlider;
    juce::TextButton textEffectsButton { juce::String::fromUTF8 ("Text effects\xe2\x80\xa6") };
    void editTextEffects();
    juce::ToggleButton verticalToggle { "Up and down" }, boldToggle { "Bold" }, edgeToggle { "Its rim lights" },
                       capGlowToggle { "Its cap glows" }, plateToggle { "On a plate" };
    FineSlider columnsSlider, gapSlider, roundingSlider, arcFromSlider, arcToSlider, innerSlider, textSizeSlider,
                 glowSlider, edgeOffSlider, heightSlider, travelSlider;
    ColourSwatchButton capSwatch { "Key" }, capOnSwatch { "Key when on" }, labelSwatch { "Name" }, labelOnSwatch { "Name when on" },
                       glowSwatch { "Its light" }, ledSwatch { "Lamp" }, plateSwatch { "Plate" };

    void setUpRows();
    void setUpSlider (juce::Slider&, double from, double to, double step, const char* key, const juce::String& tip, const juce::String& suffix = {});
    void setUpCombo (juce::ComboBox&, const juce::StringArray& captions, const juce::StringArray& values, const char* key, const juce::String& tip);
    void setUpToggle (juce::ToggleButton&, const char* key, const juce::String& tip);
    void setUpSwatch (ColourSwatchButton&, const char* key, const juce::String& tip);

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectorStudio)
};
