// D:\workspace\Fanan_web_hub\src\PerformStudio.h

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "UiHelpers.h"   // FineSlider

#include "ColourSwatch.h"
#include "GuiLayout.h"

class PluginCanvas;
namespace Render3D { struct ScopedDraft; }

//==============================================================================
/**
    The Performance Studio: one X-Y pad as a performance control (PerformControl), in a
    window of its own beside the panel, which shows every change as it's made.

    Its gallery is in the Gallery window (GalleryHub, the Gallery button across the top): the templates (pitch and mod wheels, a lit mod wheel,
    a bender lever, a ball on a cross, a vector joystick, an arcade stick, a gimbal in its
    boot, a thumbstick, X-Y pads, a ribbon, an LED touch strip) and the ones kept there
    (.fkperf), as pictures; click one and the pad becomes it. In the middle it as it looks:
    drag it, let it go (a pitch bend springs back). On the right its settings and parameters.
    Across the top how it moves: freely, or pitch bend and modulation (which axis bends).
*/
class PerformStudio final : public juce::Component,
                            private juce::Timer
{
public:
    PerformStudio (PluginCanvas& canvas, int widgetIndex);
    ~PerformStudio() override;

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
    bool alive() const;
    GuiWidget* pad() const;
    juce::var design() const;
    juce::var& ensureDesign();
    juce::String stateKey() const;

    void set (const juce::Identifier& key, const juce::var& value, bool final = true);
    void setSetting (const juce::String& key, const juce::var& value);
    void changed (bool final);
    void refresh();
    void showRows();
    void useTemplate (const juce::var& newDesign, const juce::var& settings);
    void timerCallback() override;

    void saveToGallery();

    // Across the top.
    juce::Label moveLabel;
    juce::ComboBox modeBox, bendBox;
    juce::TextButton keepButton { juce::String::fromUTF8 ("Keep in gallery\xe2\x80\xa6") };
    juce::TextButton plainButton { "Plain X-Y pad" };

    juce::TextButton galleryButton { juce::String::fromUTF8 ("Gallery\xe2\x80\xa6") };   // the Gallery window, on this one's page
    std::unique_ptr<Preview> preview;
    std::unique_ptr<Rows> rows;
    juce::Viewport rowsView;

    // Its settings.
    juce::TextEditor xParamBox, yParamBox;
    juce::ComboBox kindBox, styleBox, renderBox, materialBox;
    juce::ToggleButton cleanToggle { "Clean (no ribs, no mark)" };
    juce::ToggleButton verticalToggle { "Up and down" }, litToggle { "A light in it" }, ledsToggle { "Lamps along it" },
                       middleToggle { "Lit from the middle" };
    FineSlider glowSlider, travelSlider, rangeSlider, tiltSlider, framesSlider;
    ColourSwatchButton housingSwatch { "Housing" }, partSwatch { "Wheel, ball, stick" }, accentSwatch { "Mark, puck, lamps" };

    void setUpRows();
    void setUpSlider (juce::Slider&, double from, double to, double step, double fallback, const char* key, const juce::String& tip, const juce::String& suffix = {});
    void setUpCombo (juce::ComboBox&, const juce::StringArray& captions, const juce::StringArray& values, const juce::String& fallback, const char* key, const juce::String& tip);
    void setUpToggle (juce::ToggleButton&, bool fallback, const char* key, const juce::String& tip);
    void setUpSwatch (ColourSwatchButton&, juce::Colour fallback, const char* key, const juce::String& tip);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformStudio)
};
