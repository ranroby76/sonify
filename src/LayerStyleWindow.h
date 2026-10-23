// D:\workspace\Fanan_web_hub\src\LayerStyleWindow.h
// The Edit UI's two Photoshop windows. Global Light: where the panel's one light stands, a dot
// dragged across a lit dome (the knobs' looks, the shapes and painted backgrounds are all lit
// from there). Layer Style: a shape's Bevel & Emboss with its Contour, Stroke, Inner Shadow,
// Color Overlay and Drop Shadow, in Photoshop's list with a tick for each.

#pragma once

#include "BevelDialog.h"    // ContourEditor, ContourPresetButton, AngleDial, BevelPreview, fillBlendModes
#include "ColourSwatch.h"
#include "LayerDoc.h"       // LayerStyle

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <functional>
#include <vector>

//==============================================================================
/**
    The light over a dome, seen from above: the dot is where the light stands. Around the
    circle is its angle (Photoshop's: 0 from the right, 90 from above, 120 the upper left), in
    toward the middle its altitude (at the rim 0 degrees, low and raking; in the middle 90,
    straight down). The dome is lit by it, so the dot sits on its highlight. Shift: angles in
    steps of 15 degrees.
*/
class LightPad final : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    LightPad() = default;

    void setLight (float angle, float altitude);
    float getAngle() const noexcept       { return angle; }
    float getAltitude() const noexcept    { return altitude; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onChange;    // as it's dragged
    std::function<void()> onRelease;   // let go

private:
    juce::Rectangle<float> disc() const;
    void setFromMouse (const juce::MouseEvent&);
    void shade();

    float angle = 120.0f, altitude = 30.0f;
    juce::Image dome;   // the dome as the light shades it

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LightPad)
};

//==============================================================================
/**
    Photoshop's Global Light, the panel's one light (the layout's "light"): the dot on the dome,
    its angle and altitude typed in. What it lights shows it as it moves; OK keeps it, Cancel
    (or closing the window) puts the old light back. Modal, as Photoshop's is.
*/
class GlobalLightWindow final : public juce::Component,
                                private juce::Timer
{
public:
    /** The light as it's set; final: OK, keep it. */
    using Apply = std::function<void (float angle, float altitude, bool final)>;

    GlobalLightWindow (float angle, float altitude, Apply apply);
    ~GlobalLightWindow() override;

    /** Opens it over centreAround. */
    static void launch (float angle, float altitude, Apply apply, juce::Component* centreAround);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changed (bool now);   // the light moved: shown now, or once it rests
    void finish (bool keep);

    Apply apply;
    const float startAngle, startAltitude;
    bool finished = false, updating = false;

    LightPad pad;
    juce::Label angleLabel, altitudeLabel, note;
    juce::Slider angleSlider, altitudeSlider;
    juce::ToggleButton previewToggle { "Preview" };
    juce::TextButton okButton { "OK" }, cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalLightWindow)
};

//==============================================================================
/**
    Photoshop's Layer Style for a shape: the effects listed on the left, each with its tick
    (clicking a name shows its settings and ticks it), the selected one's settings in the
    middle, OK, Cancel, Preview and the shape wearing the style on the right.

    Bevel & Emboss: Style, Technique, Depth, Direction, Size, Soften; Angle and Altitude with
    Use Global Light, Gloss Contour, Anti-aliased, the Highlight and Shadow modes, colours and
    opacities. Contour: the bevel's cross-section and its Range. Stroke: Size, Position
    (outside, inside, center), Blend Mode, Opacity, Color. Inner Shadow and Drop Shadow: Blend
    Mode and colour, Opacity, Angle with Use Global Light, Distance, Choke or Spread, Size (and
    whether the layer knocks the drop shadow out). Color Overlay: Blend Mode, colour, Opacity.

    With Use Global Light on, an angle (and the bevel's altitude) is the panel's global light:
    changing it moves the light for everything, as in Photoshop. Edits show on the shape as
    they're made; OK keeps them, Cancel (or closing the window) puts the shape and the light
    back as they were.
*/
class LayerStyleWindow final : public juce::Component,
                               private juce::Timer
{
public:
    /** What the window works on: the canvas's shape. */
    struct Host
    {
        std::function<bool()> alive;                                    // false: the shape is gone, the window closes
        std::function<void (const LayerStyle&, bool final)> apply;      // the shape wears it; final: OK, keep it
        std::function<void()> cancel;                                   // the shape and the light as they were
        std::function<juce::Image (const LayerStyle&)> sample;          // the shape wearing it
        std::function<juce::Point<float>()> globalLight;                // x: its angle, y: its altitude
        std::function<void (float angle, float altitude)> setGlobalLight;
    };

    LayerStyleWindow (const LayerStyle& style, Host host);
    ~LayerStyleWindow() override;

    /** Opens it over centreAround; title: "Layer Style: <the shape>". */
    static void launch (const LayerStyle& style, Host host, const juce::String& title, juce::Component* centreAround);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum Page { bevelPage = 0, contourPage, strokePage, innerShadowPage, overlayPage, dropShadowPage, numPages };

    class EffectList;

    /** A row of a page: its caption on the left, its controls across (width 0: the rest). */
    struct Row
    {
        juce::Label* label = nullptr;
        std::vector<std::pair<juce::Component*, int>> parts;
        int height = 26;
    };

    /** An angle, as Photoshop shows it: the dial, the degrees, Use Global Light. */
    struct LightControls
    {
        AngleDial dial;
        juce::Slider angle;
        juce::ToggleButton global { "Use Global Light" };
        juce::Slider* altitude = nullptr;   // the bevel's
    };

    void timerCallback() override;
    void showPage (int);
    bool isOn (int page) const;
    void setOn (int page, bool on);

    void refresh();                      // the style -> the controls
    void pull();                         // the controls -> the style -> the shape
    void lightMoved (LightControls&);    // an angle or altitude changed
    void showGlobal (LightControls&);    // shows the global light, while it's used
    void finish (bool keep);

    juce::Label* caption (const juce::String& text, bool header = false);
    void addRow (Page, const juce::String& caption, std::vector<std::pair<juce::Component*, int>> parts, int height = 26);
    void addHeader (Page, const juce::String& text);
    void setUpSlider (juce::Slider&, double low, double high, double step, const juce::String& suffix);
    void setUpLight (LightControls&, juce::Slider* altitude);

    LayerStyle style;
    const LayerStyle original;
    Host host;
    int page = bevelPage;
    bool updating = false, finished = false, sampleDue = true;

    std::unique_ptr<EffectList> list;
    juce::OwnedArray<juce::Label> labels;
    std::array<std::vector<Row>, numPages> rows;
    std::array<juce::String, numPages> titles;
    juce::Rectangle<int> pageArea;

    // Bevel & Emboss
    juce::ComboBox bevelStyle, bevelTechnique;
    juce::Slider bevelDepth, bevelSize, bevelSoften, bevelAltitude;
    juce::ToggleButton bevelUp { "Up" }, bevelDown { "Down" };
    LightControls bevelLight;
    ContourPresetButton glossContour;
    juce::ToggleButton glossAntiAliased { "Anti-aliased" };
    juce::ComboBox highlightMode, shadowMode;
    ColourSwatchButton highlightColour { "Highlight colour" }, shadowColour { "Shadow colour" };
    juce::Slider highlightOpacity, shadowOpacity;

    // Contour
    ContourPresetButton contourPreset;
    ContourEditor contourEditor;
    juce::ToggleButton contourAntiAliased { "Anti-aliased" };
    juce::Slider contourRange;

    // Stroke
    juce::Slider strokeSize, strokeOpacity;
    juce::ComboBox strokePosition, strokeBlend;
    ColourSwatchButton strokeColour { "Stroke colour" };

    // Inner Shadow
    juce::ComboBox innerBlend;
    ColourSwatchButton innerColour { "Inner shadow colour" };
    juce::Slider innerOpacity, innerDistance, innerChoke, innerSize;
    LightControls innerLight;

    // Color Overlay
    juce::ComboBox overlayBlend;
    ColourSwatchButton overlayColour { "Overlay colour" };
    juce::Slider overlayOpacity;

    // Drop Shadow
    juce::ComboBox dropBlend;
    ColourSwatchButton dropColour { "Drop shadow colour" };
    juce::Slider dropOpacity, dropDistance, dropSpread, dropSize;
    LightControls dropLight;
    juce::ToggleButton dropKnockout { "Layer Knocks Out Drop Shadow" };

    // OK, Cancel, Preview, the sample.
    juce::TextButton okButton { "OK" }, cancelButton { "Cancel" };
    juce::ToggleButton previewToggle { "Preview" };
    BevelPreview sample;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LayerStyleWindow)
};
