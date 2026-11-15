// D:\workspace\Fanan_web_hub\src\MatrixStudio.h

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "UiHelpers.h"   // FineSlider

#include "ColourSwatch.h"
#include "GuiLayout.h"
#include "ModMatrix.h"

class PluginCanvas;
namespace Render3D { struct ScopedDraft; }

//==============================================================================
/**
    The Matrix Studio: one modulation matrix (ModMatrix), in a window of its own beside the
    panel, which shows every change as it's made.

    Its gallery is in the Gallery window (GalleryHub, the Gallery button across the top): the templates (slots dark, light and compact, a VCS3-style
    pin grid, a knob grid, a bar grid, Eurorack, studio and minimal patch bays) and the ones
    kept there (.fkmatrix), as pictures; click one and the matrix wears it, its routes kept.
    In the middle it as it is, with its routes; on the right its settings: its style, how many
    routes (each one a source, a destination and an amount: three of the plugin's parameters),
    its sources, the destinations a grid or a patch bay shows, its colours, font and jacks.
*/
class MatrixStudio final : public juce::Component,
                           private juce::Timer
{
public:
    MatrixStudio (PluginCanvas& canvas, int widgetIndex);
    ~MatrixStudio() override;

    static std::unique_ptr<juce::DocumentWindow> makeWindow (PluginCanvas& canvas, int widgetIndex);

    /** A design's picture as the Gallery window shows it (GalleryHub). */
    void static paintGalleryPicture (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area);

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
    GuiWidget* matrix() const;
    juce::var design() const;
    juce::var& ensureDesign();
    juce::String stateKey() const;
    ModMatrix::View view() const;      // the matrix's routes as the panel has them

    void set (const juce::Identifier& key, const juce::var& value, bool final = true);
    void changed (bool final);
    void refresh();
    void showRows();
    void useDesign (const juce::var& newDesign);
    void pickTargets();
    void pickTarget();
    void drawPart (juce::Graphics&);   // a part lying on its own, as the window shows it
    void timerCallback() override;
    void saveToGallery();

    juce::TextButton keepButton { juce::String::fromUTF8 ("Keep in gallery\xe2\x80\xa6") };

    juce::TextButton galleryButton { juce::String::fromUTF8 ("Gallery\xe2\x80\xa6") };   // the Gallery window, on this one's page
    std::unique_ptr<Preview> preview;
    std::unique_ptr<Rows> rows;
    juce::Viewport rowsView;

    juce::TextEditor paramBox, sourcesBox, targetsBox, cablesBox;
    juce::TextButton targetsButton { juce::String::fromUTF8 ("Pick\xe2\x80\xa6") };
    juce::ComboBox styleBox, cellBox, jackBox, fontBox, roleBox, sourceBox;
    juce::TextButton targetButton { juce::String::fromUTF8 ("Pick\xe2\x80\xa6") };
    juce::ToggleButton headerToggle { "A header over its routes" };
    juce::ToggleButton matchToggle { "Matching inputs: an input for every output" };   // a patch bay's "matchInputs"
    FineSlider routesSlider, amountSlider, sagSlider, textSizeSlider;
    ColourSwatchButton accentSwatch { "Accent" }, panelSwatch { "Panel" }, textSwatch { "Text" }, lineSwatch { "Lines" };

    void setUpRows();
    void setUpSlider (juce::Slider&, double from, double to, double step, double fallback, const char* key, const juce::String& tip, const juce::String& suffix = {});
    void setUpCombo (juce::ComboBox&, const juce::StringArray& captions, const juce::StringArray& values, const juce::String& fallback, const char* key, const juce::String& tip);
    void setUpSwatch (ColourSwatchButton&, juce::Colour fallback, const char* key, const juce::String& tip);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatrixStudio)
};
