// D:\workspace\Fanan_web_hub\src\LayerStyleWindow.cpp

#include "LayerStyleWindow.h"
#include "StellaLookAndFeel.h"   // Theme
#include "UiHelpers.h"           // configurePanelControls

#include <cmath>

namespace
{
    /** Closes the dialog window it lies in (it deletes itself, and what it shows). */
    void closeWindowOf (juce::Component& content)
    {
        if (auto* window = content.findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState (0);
    }

    float wrapAngle (float degrees)
    {
        auto a = std::fmod (degrees, 360.0f);

        if (a > 180.0f)        a -= 360.0f;
        else if (a <= -180.0f) a += 360.0f;

        return a;
    }

    void styleCaption (juce::Label& l, bool header)
    {
        l.setColour (juce::Label::textColourId, header ? Theme::accent : Theme::text.withAlpha (0.82f));
        l.setFont (Theme::font (header ? 12.0f : 13.0f, header));
        l.setJustificationType (juce::Justification::centredLeft);
        l.setInterceptsMouseClicks (false, false);
    }

    BlendMode modeOf (const juce::ComboBox& box)
    {
        return (BlendMode) juce::jlimit (0, (int) BlendMode::numModes - 1, box.getSelectedId() - 1);
    }

    void showMode (juce::ComboBox& box, BlendMode mode)
    {
        box.setSelectedId ((int) mode + 1, juce::dontSendNotification);
    }

    // The stroke's positions as Photoshop lists them.
    const StrokePosition strokePositions[] { StrokePosition::Outside, StrokePosition::Inside, StrokePosition::Centre };

    bool isStraight (const Contour& c)
    {
        for (const auto& p : c.points)
            if (std::abs (p.x - p.y) > 0.001f)
                return false;

        return true;
    }
}

//==============================================================================
juce::Rectangle<float> LightPad::disc() const
{
    const auto r = getLocalBounds().toFloat().reduced (6.0f);
    const auto side = std::floor (juce::jmin (r.getWidth(), r.getHeight()));
    return r.withSizeKeepingCentre (side, side);
}

void LightPad::setLight (float newAngle, float newAltitude)
{
    angle = wrapAngle (newAngle);
    altitude = juce::jlimit (0.0f, 90.0f, newAltitude);
    shade();
    repaint();
}

void LightPad::resized()
{
    shade();
}

void LightPad::shade()
{
    const auto d = disc();
    const int size = juce::jmax (1, (int) d.getWidth());
    dome = juce::Image (juce::Image::ARGB, size, size, true);

    // The light's direction: x to the right, y up, z out of the panel.
    const float a = juce::degreesToRadians (angle), e = juce::degreesToRadians (altitude);
    const float lx = std::cos (a) * std::cos (e), ly = std::sin (a) * std::cos (e), lz = std::sin (e);
    const float radius = (float) size * 0.5f;

    juce::Image::BitmapData data (dome, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float nx = ((float) x + 0.5f - radius) / radius;
            const float ny = (radius - ((float) y + 0.5f)) / radius;
            const float rr = nx * nx + ny * ny;
            const float cover = juce::jlimit (0.0f, 1.0f, (1.0f - std::sqrt (rr)) * radius + 0.5f);

            if (cover <= 0.0f)
                continue;

            // A matte dome with a little shine: brightest where it faces the light.
            const float nz = std::sqrt (juce::jmax (0.0f, 1.0f - rr));
            const float facing = juce::jmax (0.0f, nx * lx + ny * ly + nz * lz);
            const float shine = std::pow (facing, 36.0f);
            const float level = juce::jlimit (0.0f, 1.0f, 0.07f + 0.62f * facing + 0.35f * shine);

            data.setPixelColour (x, y, juce::Colour::fromFloatRGBA (0.09f + 0.80f * level, 0.09f + 0.79f * level, 0.10f + 0.76f * level, cover));
        }
    }
}

void LightPad::paint (juce::Graphics& g)
{
    const auto d = disc();
    const auto centre = d.getCentre();
    const auto radius = d.getWidth() * 0.5f;

    g.drawImageAt (dome, (int) d.getX(), (int) d.getY());

    // The crosshair, and rings at 30 and 60 degrees up.
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawLine (d.getX(), centre.y, d.getRight(), centre.y, 1.0f);
    g.drawLine (centre.x, d.getY(), centre.x, d.getBottom(), 1.0f);

    for (const auto up : { 30.0f, 60.0f })
    {
        const auto r = radius * std::cos (juce::degreesToRadians (up));
        g.drawEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.0f);
    }

    g.setColour (Theme::outline.brighter (0.5f));
    g.drawEllipse (d, 1.2f);

    // The light: where it stands over the dome.
    const auto a = juce::degreesToRadians (angle);
    const auto r = radius * std::cos (juce::degreesToRadians (altitude));
    const juce::Point<float> dot (centre.x + std::cos (a) * r, centre.y - std::sin (a) * r);

    g.setColour (juce::Colour (0xffe8c020).withAlpha (0.8f));
    g.drawLine ({ centre, dot }, 1.4f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (dot.x - 8.0f, dot.y - 8.0f, 16.0f, 16.0f);
    g.setColour (juce::Colour (0xffffe27a));
    g.fillEllipse (dot.x - 6.0f, dot.y - 6.0f, 12.0f, 12.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (dot.x - 6.0f, dot.y - 6.0f, 12.0f, 12.0f, 1.0f);
}

void LightPad::mouseDown (const juce::MouseEvent& e)    { setFromMouse (e); }
void LightPad::mouseDrag (const juce::MouseEvent& e)    { setFromMouse (e); }

void LightPad::mouseUp (const juce::MouseEvent&)
{
    if (onRelease != nullptr)
        onRelease();
}

void LightPad::setFromMouse (const juce::MouseEvent& e)
{
    const auto d = disc();
    const auto radius = juce::jmax (1.0f, d.getWidth() * 0.5f);
    const float dx = e.position.x - d.getCentreX(), dy = d.getCentreY() - e.position.y;
    const float distance = juce::jmin (radius, std::hypot (dx, dy));

    if (distance > 0.5f)
    {
        auto a = juce::radiansToDegrees (std::atan2 (dy, dx));

        if (e.mods.isShiftDown())
            a = 15.0f * std::round (a / 15.0f);

        angle = wrapAngle ((float) juce::roundToInt (a));
    }

    altitude = (float) juce::roundToInt (juce::radiansToDegrees (std::acos (distance / radius)));
    shade();
    repaint();

    if (onChange != nullptr)
        onChange();
}

//==============================================================================
GlobalLightWindow::GlobalLightWindow (float angle, float altitude, Apply applyLight)
    : apply (std::move (applyLight)), startAngle (wrapAngle (angle)), startAltitude (juce::jlimit (0.0f, 90.0f, altitude))
{
    pad.setLight (startAngle, startAltitude);
    pad.setTooltip ("Drag the light: around the circle for where it comes from, in toward the middle for higher (Shift: steps of 15 degrees)");
    pad.onChange = [this]
    {
        const juce::ScopedValueSetter<bool> quiet (updating, true);
        angleSlider.setValue (pad.getAngle(), juce::dontSendNotification);
        altitudeSlider.setValue (pad.getAltitude(), juce::dontSendNotification);
        changed (false);
    };
    pad.onRelease = [this] { changed (true); };
    addAndMakeVisible (pad);

    angleLabel.setText ("Angle:", juce::dontSendNotification);
    altitudeLabel.setText ("Altitude:", juce::dontSendNotification);

    for (auto* l : { &angleLabel, &altitudeLabel })
    {
        styleCaption (*l, false);
        addAndMakeVisible (l);
    }

    note.setText ("One light for the whole panel. The knobs' looks, the shapes (their materials, and layer styles with "
                  "Use Global Light) and painted backgrounds are all lit from here.", juce::dontSendNotification);
    note.setColour (juce::Label::textColourId, Theme::muted);
    note.setFont (Theme::font (12.5f));
    note.setJustificationType (juce::Justification::topLeft);
    note.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (note);

    for (auto* s : { &angleSlider, &altitudeSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
        s->setTextValueSuffix (juce::String::fromUTF8 ("\xc2\xb0"));
        s->onValueChange = [this]
        {
            if (updating)
                return;

            pad.setLight ((float) angleSlider.getValue(), (float) altitudeSlider.getValue());
            changed (false);
        };
        s->onDragEnd = [this] { changed (true); };
        addAndMakeVisible (s);
    }

    angleSlider.setRange (-180.0, 180.0, 1.0);
    altitudeSlider.setRange (0.0, 90.0, 1.0);
    angleSlider.setValue (startAngle, juce::dontSendNotification);
    altitudeSlider.setValue (startAltitude, juce::dontSendNotification);
    angleSlider.setTooltip ("Where the light comes from, in degrees: 0 the right, 90 above, 120 the upper left, 180 the left");
    altitudeSlider.setTooltip ("How high the light stands, in degrees: low for long shadows and strong edges, 90 straight down");

    previewToggle.setToggleState (true, juce::dontSendNotification);
    previewToggle.setTooltip ("Show the new light on the panel as it's set");
    previewToggle.onClick = [this] { changed (true); };
    addAndMakeVisible (previewToggle);

    okButton.onClick = [this] { finish (true); };
    cancelButton.onClick = [this] { finish (false); };
    okButton.addShortcut (juce::KeyPress (juce::KeyPress::returnKey));
    addAndMakeVisible (okButton);
    addAndMakeVisible (cancelButton);

    configurePanelControls (*this);
    setSize (540, 262);
}

GlobalLightWindow::~GlobalLightWindow()
{
    stopTimer();

    // Closed without OK: the light as it was.
    if (! finished && apply != nullptr)
        apply (startAngle, startAltitude, false);
}

void GlobalLightWindow::launch (float angle, float altitude, Apply apply, juce::Component* centreAround)
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new GlobalLightWindow (angle, altitude, std::move (apply)));
    options.dialogTitle = "Global Light";
    options.dialogBackgroundColour = Theme::panel;
    options.componentToCentreAround = centreAround;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void GlobalLightWindow::changed (bool now)
{
    // What it lights is drawn again once the dot rests (a panel of knobs takes a moment).
    if (now)
        timerCallback();
    else
        startTimer (160);
}

void GlobalLightWindow::timerCallback()
{
    stopTimer();

    if (finished || apply == nullptr)
        return;

    if (previewToggle.getToggleState())
        apply (pad.getAngle(), pad.getAltitude(), false);
    else
        apply (startAngle, startAltitude, false);
}

void GlobalLightWindow::finish (bool keep)
{
    if (finished)
        return;

    finished = true;
    stopTimer();

    if (apply != nullptr)
    {
        if (keep) apply (pad.getAngle(), pad.getAltitude(), true);
        else      apply (startAngle, startAltitude, false);
    }

    closeWindowOf (*this);
}

void GlobalLightWindow::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);
}

void GlobalLightWindow::resized()
{
    auto area = getLocalBounds().reduced (14);
    pad.setBounds (area.removeFromLeft (area.getHeight()));
    area.removeFromLeft (16);

    auto buttons = area.removeFromRight (96);
    okButton.setBounds (buttons.removeFromTop (28));
    buttons.removeFromTop (8);
    cancelButton.setBounds (buttons.removeFromTop (28));
    buttons.removeFromTop (14);
    previewToggle.setBounds (buttons.removeFromTop (24));
    area.removeFromRight (14);

    area.removeFromTop (6);
    angleLabel.setBounds (area.removeFromTop (20));
    angleSlider.setBounds (area.removeFromTop (26));
    area.removeFromTop (10);
    altitudeLabel.setBounds (area.removeFromTop (20));
    altitudeSlider.setBounds (area.removeFromTop (26));
    area.removeFromTop (14);
    note.setBounds (area);
}

//==============================================================================
/** The effects down the left, Photoshop's way: a tick to switch one on or off, its name to
    show its settings (and switch it on). */
class LayerStyleWindow::EffectList final : public juce::Component
{
public:
    explicit EffectList (LayerStyleWindow& w) : owner (w) {}

    static constexpr int headerHeight = 32, rowHeight = 30;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Theme::inset);

        g.setColour (Theme::muted);
        g.setFont (Theme::font (12.5f, true));
        g.drawText ("Styles", 12, 0, getWidth() - 12, headerHeight, juce::Justification::centredLeft);

        for (int i = 0; i < numPages; ++i)
        {
            const auto row = rowArea (i);
            const bool shown = i == owner.page;

            if (shown)
            {
                g.setColour (Theme::accentDeep);
                g.fillRect (row);
                g.setColour (Theme::accent);
                g.fillRect (row.withWidth (3));
            }

            const auto box = boxArea (i);
            g.setColour (Theme::window);
            g.fillRoundedRectangle (box, 2.5f);
            g.setColour (Theme::outline.brighter (0.6f));
            g.drawRoundedRectangle (box, 2.5f, 1.0f);

            if (owner.isOn (i))
            {
                juce::Path tick;
                tick.startNewSubPath (box.getX() + 3.0f, box.getCentreY());
                tick.lineTo (box.getX() + 6.0f, box.getBottom() - 3.5f);
                tick.lineTo (box.getRight() - 2.5f, box.getY() + 3.0f);
                g.setColour (Theme::accent.brighter (0.3f));
                g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            g.setColour (shown ? Theme::text : Theme::text.withAlpha (0.8f));
            g.setFont (Theme::font (13.5f, shown));
            g.drawText (owner.titles[(size_t) i], row.withLeft ((int) box.getRight() + 10).withTrimmedRight (6),
                        juce::Justification::centredLeft, true);
        }

        g.setColour (Theme::outline);
        g.drawRect (getLocalBounds());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < numPages; ++i)
        {
            if (! rowArea (i).contains (e.getPosition()))
                continue;

            if (boxArea (i).expanded (5.0f).contains (e.position))
            {
                owner.setOn (i, ! owner.isOn (i));
            }
            else
            {
                if (! owner.isOn (i))
                    owner.setOn (i, true);

                owner.showPage (i);
            }

            return;
        }
    }

private:
    juce::Rectangle<int> rowArea (int i) const    { return { 0, headerHeight + i * rowHeight, getWidth(), rowHeight }; }

    juce::Rectangle<float> boxArea (int i) const
    {
        // Contour is the bevel's: indented under it.
        const auto row = rowArea (i);
        const float x = i == contourPage ? 30.0f : 12.0f;
        return { x, (float) row.getY() + (float) (rowHeight - 14) * 0.5f, 14.0f, 14.0f };
    }

    LayerStyleWindow& owner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectList)
};

//==============================================================================
LayerStyleWindow::LayerStyleWindow (const LayerStyle& startStyle, Host hostToUse)
    : style (startStyle), original (startStyle), host (std::move (hostToUse))
{
    titles = { "Bevel & Emboss", "Contour", "Stroke", "Inner Shadow", "Color Overlay", "Drop Shadow" };

    list = std::make_unique<EffectList> (*this);
    addAndMakeVisible (*list);

    const auto px = juce::String (" px"), percent = juce::String (" %"), degrees = juce::String::fromUTF8 ("\xc2\xb0");

    // Bevel & Emboss.
    bevelStyle.addItemList (bevelStyleNames(), 1);
    bevelTechnique.addItemList (bevelTechniqueNames(), 1);
    setUpSlider (bevelDepth, 1.0, 1000.0, 1.0, percent);
    bevelDepth.setSkewFactorFromMidPoint (150.0);
    setUpSlider (bevelSize, 0.0, 250.0, 1.0, px);
    bevelSize.setSkewFactorFromMidPoint (30.0);
    setUpSlider (bevelSoften, 0.0, 16.0, 1.0, px);
    setUpSlider (bevelAltitude, 0.0, 90.0, 1.0, degrees);
    setUpLight (bevelLight, &bevelAltitude);
    bevelUp.setRadioGroupId (9101);
    bevelDown.setRadioGroupId (9101);
    fillBlendModes (highlightMode);
    fillBlendModes (shadowMode);
    setUpSlider (highlightOpacity, 0.0, 100.0, 1.0, percent);
    setUpSlider (shadowOpacity, 0.0, 100.0, 1.0, percent);

    bevelStyle.setTooltip ("Inner Bevel: the edge inside the shape. Outer Bevel: around it, on the panel. Emboss: both. "
                           "Pillow Emboss: pressed into the panel at its edge. Stroke Emboss: the stroke embossed");
    bevelTechnique.setTooltip ("Smooth: soft rounded edges. Chisel Hard: sharp cut edges. Chisel Soft: cut, but softened");
    glossContour.setTooltip ("Gloss Contour: how the light reads across the bevel (a ring, a double ring: metal, glass)");
    highlightColour.setTooltip ("The highlight's colour");
    shadowColour.setTooltip ("The shadow's colour");

    glossContour.onPick = [this] (const Contour& c)
    {
        style.bevel.glossContour = c;
        glossContour.setContour (c);
        pull();
    };

    addHeader (bevelPage, "Structure");
    addRow (bevelPage, "Style", { { &bevelStyle, 0 } });
    addRow (bevelPage, "Technique", { { &bevelTechnique, 0 } });
    addRow (bevelPage, "Depth", { { &bevelDepth, 0 } });
    addRow (bevelPage, "Direction", { { &bevelUp, 80 }, { &bevelDown, 80 } });
    addRow (bevelPage, "Size", { { &bevelSize, 0 } });
    addRow (bevelPage, "Soften", { { &bevelSoften, 0 } });
    addHeader (bevelPage, "Shading");
    addRow (bevelPage, "Angle", { { &bevelLight.dial, 52 }, { &bevelLight.angle, 0 } }, 52);
    addRow (bevelPage, {}, { { &bevelLight.global, 0 } }, 22);
    addRow (bevelPage, "Altitude", { { &bevelAltitude, 0 } });
    addRow (bevelPage, "Gloss Contour", { { &glossContour, 60 }, { &glossAntiAliased, 0 } }, 36);
    addRow (bevelPage, "Highlight Mode", { { &highlightMode, 0 }, { &highlightColour, 44 } });
    addRow (bevelPage, "Opacity", { { &highlightOpacity, 0 } });
    addRow (bevelPage, "Shadow Mode", { { &shadowMode, 0 }, { &shadowColour, 44 } });
    addRow (bevelPage, "Opacity", { { &shadowOpacity, 0 } });

    // Contour: the bevel's cross-section.
    setUpSlider (contourRange, 1.0, 100.0, 1.0, percent);
    contourPreset.setTooltip ("The bevel's cross-section: a ramp, a ring, a double ring, a rounded step...");
    contourPreset.onPick = [this] (const Contour& c)
    {
        contourEditor.setContour (c);
        contourPreset.setContour (c);
        pull();
    };
    contourEditor.onChange = [this]
    {
        contourPreset.setContour (contourEditor.getContour());
        pull();
    };

    addHeader (contourPage, "Elements");
    addRow (contourPage, "Contour", { { &contourPreset, 60 }, { &contourAntiAliased, 0 } }, 36);
    addRow (contourPage, {}, { { &contourEditor, 220 } }, 200);
    addRow (contourPage, "Range", { { &contourRange, 0 } });

    // Stroke.
    setUpSlider (strokeSize, 1.0, 250.0, 1.0, px);
    strokeSize.setSkewFactorFromMidPoint (20.0);
    strokePosition.addItemList ({ "Outside", "Inside", "Center" }, 1);
    fillBlendModes (strokeBlend);
    setUpSlider (strokeOpacity, 0.0, 100.0, 1.0, percent);
    strokePosition.setTooltip ("Outside: around the shape. Inside: within its edge. Center: across it");
    strokeColour.setTooltip ("The stroke's colour");

    addHeader (strokePage, "Structure");
    addRow (strokePage, "Size", { { &strokeSize, 0 } });
    addRow (strokePage, "Position", { { &strokePosition, 0 } });
    addRow (strokePage, "Blend Mode", { { &strokeBlend, 0 } });
    addRow (strokePage, "Opacity", { { &strokeOpacity, 0 } });
    addHeader (strokePage, "Fill");
    addRow (strokePage, "Color", { { &strokeColour, 64 } });

    // Inner Shadow.
    fillBlendModes (innerBlend);
    setUpSlider (innerOpacity, 0.0, 100.0, 1.0, percent);
    setUpLight (innerLight, nullptr);
    setUpSlider (innerDistance, 0.0, 300.0, 1.0, px);
    innerDistance.setSkewFactorFromMidPoint (30.0);
    setUpSlider (innerChoke, 0.0, 100.0, 1.0, percent);
    setUpSlider (innerSize, 0.0, 250.0, 1.0, px);
    innerSize.setSkewFactorFromMidPoint (30.0);
    innerColour.setTooltip ("The inner shadow's colour");

    addHeader (innerShadowPage, "Structure");
    addRow (innerShadowPage, "Blend Mode", { { &innerBlend, 0 }, { &innerColour, 44 } });
    addRow (innerShadowPage, "Opacity", { { &innerOpacity, 0 } });
    addRow (innerShadowPage, "Angle", { { &innerLight.dial, 52 }, { &innerLight.angle, 0 } }, 52);
    addRow (innerShadowPage, {}, { { &innerLight.global, 0 } }, 22);
    addRow (innerShadowPage, "Distance", { { &innerDistance, 0 } });
    addRow (innerShadowPage, "Choke", { { &innerChoke, 0 } });
    addRow (innerShadowPage, "Size", { { &innerSize, 0 } });

    // Color Overlay.
    fillBlendModes (overlayBlend);
    setUpSlider (overlayOpacity, 0.0, 100.0, 1.0, percent);
    overlayColour.setTooltip ("The overlay's colour");

    addHeader (overlayPage, "Color");
    addRow (overlayPage, "Blend Mode", { { &overlayBlend, 0 }, { &overlayColour, 44 } });
    addRow (overlayPage, "Opacity", { { &overlayOpacity, 0 } });

    // Drop Shadow.
    fillBlendModes (dropBlend);
    setUpSlider (dropOpacity, 0.0, 100.0, 1.0, percent);
    setUpLight (dropLight, nullptr);
    setUpSlider (dropDistance, 0.0, 300.0, 1.0, px);
    dropDistance.setSkewFactorFromMidPoint (30.0);
    setUpSlider (dropSpread, 0.0, 100.0, 1.0, percent);
    setUpSlider (dropSize, 0.0, 250.0, 1.0, px);
    dropSize.setSkewFactorFromMidPoint (30.0);
    dropColour.setTooltip ("The drop shadow's colour");
    dropKnockout.setTooltip ("The shape hides its own shadow (off: a see-through shape shows its shadow through it)");

    addHeader (dropShadowPage, "Structure");
    addRow (dropShadowPage, "Blend Mode", { { &dropBlend, 0 }, { &dropColour, 44 } });
    addRow (dropShadowPage, "Opacity", { { &dropOpacity, 0 } });
    addRow (dropShadowPage, "Angle", { { &dropLight.dial, 52 }, { &dropLight.angle, 0 } }, 52);
    addRow (dropShadowPage, {}, { { &dropLight.global, 0 } }, 22);
    addRow (dropShadowPage, "Distance", { { &dropDistance, 0 } });
    addRow (dropShadowPage, "Spread", { { &dropSpread, 0 } });
    addRow (dropShadowPage, "Size", { { &dropSize, 0 } });
    addHeader (dropShadowPage, "Quality");
    addRow (dropShadowPage, {}, { { &dropKnockout, 0 } }, 22);

    // Every list, tick and colour edits the style as it changes.
    for (auto* box : { &bevelStyle, &bevelTechnique, &highlightMode, &shadowMode, &strokePosition, &strokeBlend, &innerBlend, &overlayBlend, &dropBlend })
        box->onChange = [this] { pull(); };

    for (auto* toggle : { &bevelUp, &bevelDown, &dropKnockout })
        toggle->onClick = [this] { pull(); };

    // One anti-aliasing for both of the bevel's contours: the two ticks are the same.
    glossAntiAliased.onClick = [this]
    {
        contourAntiAliased.setToggleState (glossAntiAliased.getToggleState(), juce::dontSendNotification);
        pull();
    };
    contourAntiAliased.onClick = [this]
    {
        glossAntiAliased.setToggleState (contourAntiAliased.getToggleState(), juce::dontSendNotification);
        pull();
    };

    for (auto* swatch : { &highlightColour, &shadowColour, &strokeColour, &innerColour, &overlayColour, &dropColour })
        swatch->onColourChange = [this] { pull(); };

    // OK, Cancel, Preview and the shape wearing it.
    okButton.onClick = [this] { finish (true); };
    cancelButton.onClick = [this] { finish (false); };
    okButton.addShortcut (juce::KeyPress (juce::KeyPress::returnKey));
    previewToggle.setToggleState (true, juce::dontSendNotification);
    previewToggle.setTooltip ("Show the style on the shape as it's set");
    previewToggle.onClick = [this] { pull(); };

    for (auto* c : std::initializer_list<juce::Component*> { &okButton, &cancelButton, &previewToggle, &sample })
        addAndMakeVisible (c);

    refresh();
    configurePanelControls (*this);

    // The first effect that's on, or Bevel & Emboss.
    for (int i = numPages - 1; i >= 0; --i)
        if (isOn (i) && i != contourPage)
            page = i;

    setSize (786, 600);
    startTimer (120);
}

LayerStyleWindow::~LayerStyleWindow()
{
    stopTimer();

    // Closed without OK: the shape and the light as they were.
    if (! finished && host.cancel != nullptr)
        host.cancel();
}

void LayerStyleWindow::launch (const LayerStyle& style, Host host, const juce::String& title, juce::Component* centreAround)
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new LayerStyleWindow (style, std::move (host)));
    options.dialogTitle = title;
    options.dialogBackgroundColour = Theme::panel;
    options.componentToCentreAround = centreAround;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

//==============================================================================
juce::Label* LayerStyleWindow::caption (const juce::String& text, bool header)
{
    auto* l = labels.add (new juce::Label ({}, header ? text.toUpperCase() : text));
    styleCaption (*l, header);
    addChildComponent (l);
    return l;
}

void LayerStyleWindow::addRow (Page p, const juce::String& text, std::vector<std::pair<juce::Component*, int>> parts, int height)
{
    Row row;
    row.label = text.isNotEmpty() ? caption (text) : nullptr;
    row.parts = std::move (parts);
    row.height = height;

    for (auto& [part, width] : row.parts)
        addChildComponent (part);

    rows[(size_t) p].push_back (std::move (row));
}

void LayerStyleWindow::addHeader (Page p, const juce::String& text)
{
    Row row;
    row.label = caption (text, true);
    row.height = 20;
    rows[(size_t) p].push_back (std::move (row));
}

void LayerStyleWindow::setUpSlider (juce::Slider& s, double low, double high, double step, const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    s.setRange (low, high, step);
    s.setTextValueSuffix (suffix);
    s.onValueChange = [this] { pull(); };
}

void LayerStyleWindow::setUpLight (LightControls& light, juce::Slider* altitude)
{
    light.altitude = altitude;
    setUpSlider (light.angle, -180.0, 180.0, 1.0, juce::String::fromUTF8 ("\xc2\xb0"));
    light.global.setTooltip ("Use the panel's global light: changing the angle here moves the light for everything");

    light.dial.onChange = [this, &light]
    {
        light.angle.setValue (juce::roundToInt (light.dial.getAngle()), juce::dontSendNotification);
        lightMoved (light);
    };

    light.angle.onValueChange = [this, &light]
    {
        light.dial.setAngle ((float) light.angle.getValue());
        lightMoved (light);
    };

    if (altitude != nullptr)
        altitude->onValueChange = [this, &light] { lightMoved (light); };

    light.global.onClick = [this, &light]
    {
        if (light.global.getToggleState())
            showGlobal (light);

        pull();
    };
}

//==============================================================================
bool LayerStyleWindow::isOn (int p) const
{
    switch (p)
    {
        case bevelPage:       return style.bevel.enabled;
        case contourPage:     return style.bevel.contourEnabled;
        case strokePage:      return style.stroke.enabled;
        case innerShadowPage: return style.innerShadow.enabled;
        case overlayPage:     return style.colourOverlay.enabled;
        case dropShadowPage:  return style.dropShadow.enabled;
        default:              return false;
    }
}

void LayerStyleWindow::setOn (int p, bool on)
{
    switch (p)
    {
        case bevelPage:       style.bevel.enabled = on; break;
        case strokePage:      style.stroke.enabled = on; break;
        case innerShadowPage: style.innerShadow.enabled = on; break;
        case overlayPage:     style.colourOverlay.enabled = on; break;
        case dropShadowPage:  style.dropShadow.enabled = on; break;

        case contourPage:
        {
            // The contour is the bevel's: ticking it ticks the bevel. A straight one changes
            // nothing, so it starts as a curve that shows.
            style.bevel.contourEnabled = on;

            if (on)
            {
                style.bevel.enabled = true;

                if (isStraight (style.bevel.bevelContour))
                {
                    style.bevel.bevelContour = Contour::preset (3);
                    contourEditor.setContour (style.bevel.bevelContour);
                    contourPreset.setContour (style.bevel.bevelContour);
                }
            }

            break;
        }

        default: break;
    }

    list->repaint();
    pull();
}

void LayerStyleWindow::showPage (int p)
{
    page = juce::jlimit (0, numPages - 1, p);
    resized();
    repaint();
    list->repaint();
}

//==============================================================================
void LayerStyleWindow::showGlobal (LightControls& light)
{
    if (host.globalLight == nullptr)
        return;

    const juce::ScopedValueSetter<bool> quiet (updating, true);
    const auto global = host.globalLight();
    light.angle.setValue (wrapAngle (global.x), juce::dontSendNotification);
    light.dial.setAngle (wrapAngle (global.x));

    if (light.altitude != nullptr)
        light.altitude->setValue (global.y, juce::dontSendNotification);
}

void LayerStyleWindow::lightMoved (LightControls& light)
{
    if (updating)
        return;

    // Use Global Light: the angle (and the bevel's altitude) is the panel's light, for everything.
    if (light.global.getToggleState() && host.setGlobalLight != nullptr && host.globalLight != nullptr)
    {
        const auto global = host.globalLight();
        const auto altitude = light.altitude != nullptr ? (float) light.altitude->getValue() : global.y;
        host.setGlobalLight ((float) light.angle.getValue(), altitude);

        for (auto* other : { &bevelLight, &innerLight, &dropLight })
            if (other != &light && other->global.getToggleState())
                showGlobal (*other);
    }

    pull();
}

void LayerStyleWindow::refresh()
{
    const juce::ScopedValueSetter<bool> quiet (updating, true);
    const auto send = juce::dontSendNotification;

    const auto& b = style.bevel;
    bevelStyle.setSelectedId ((int) b.style + 1, send);
    bevelTechnique.setSelectedId ((int) b.technique + 1, send);
    bevelDepth.setValue (b.depth, send);
    bevelUp.setToggleState (b.up, send);
    bevelDown.setToggleState (! b.up, send);
    bevelSize.setValue (b.size, send);
    bevelSoften.setValue (b.soften, send);
    bevelLight.global.setToggleState (b.useGlobalLight, send);
    bevelLight.angle.setValue (wrapAngle (b.angle), send);
    bevelLight.dial.setAngle (wrapAngle (b.angle));
    bevelAltitude.setValue (b.altitude, send);
    glossContour.setContour (b.glossContour);
    glossAntiAliased.setToggleState (b.antiAliased, send);
    contourAntiAliased.setToggleState (b.antiAliased, send);
    showMode (highlightMode, b.highlightMode);
    highlightColour.setColourValue (b.highlightColour, send);
    highlightOpacity.setValue (b.highlightOpacity, send);
    showMode (shadowMode, b.shadowMode);
    shadowColour.setColourValue (b.shadowColour, send);
    shadowOpacity.setValue (b.shadowOpacity, send);

    contourPreset.setContour (b.bevelContour);
    contourEditor.setContour (b.bevelContour);
    contourRange.setValue (b.contourRange, send);

    const auto& s = style.stroke;
    strokeSize.setValue (s.size, send);

    for (int i = 0; i < 3; ++i)
        if (strokePositions[i] == s.position)
            strokePosition.setSelectedId (i + 1, send);

    showMode (strokeBlend, s.blend);
    strokeOpacity.setValue (s.opacity, send);
    strokeColour.setColourValue (s.colour, send);

    const auto& i = style.innerShadow;
    showMode (innerBlend, i.blend);
    innerColour.setColourValue (i.colour, send);
    innerOpacity.setValue (i.opacity, send);
    innerLight.global.setToggleState (i.useGlobalLight, send);
    innerLight.angle.setValue (wrapAngle (i.angle), send);
    innerLight.dial.setAngle (wrapAngle (i.angle));
    innerDistance.setValue (i.distance, send);
    innerChoke.setValue (i.spread, send);
    innerSize.setValue (i.size, send);

    const auto& o = style.colourOverlay;
    showMode (overlayBlend, o.blend);
    overlayColour.setColourValue (o.colour, send);
    overlayOpacity.setValue (o.opacity, send);

    const auto& d = style.dropShadow;
    showMode (dropBlend, d.blend);
    dropColour.setColourValue (d.colour, send);
    dropOpacity.setValue (d.opacity, send);
    dropLight.global.setToggleState (d.useGlobalLight, send);
    dropLight.angle.setValue (wrapAngle (d.angle), send);
    dropLight.dial.setAngle (wrapAngle (d.angle));
    dropDistance.setValue (d.distance, send);
    dropSpread.setValue (d.spread, send);
    dropSize.setValue (d.size, send);
    dropKnockout.setToggleState (d.knockout, send);

    // Those on the global light show it.
    for (auto* light : { &bevelLight, &innerLight, &dropLight })
        if (light->global.getToggleState())
            showGlobal (*light);
}

void LayerStyleWindow::pull()
{
    if (updating || finished)
        return;

    auto& b = style.bevel;
    b.style = (BevelStyle) juce::jlimit (0, (int) BevelStyle::numStyles - 1, bevelStyle.getSelectedId() - 1);
    b.technique = (BevelTechnique) juce::jlimit (0, (int) BevelTechnique::numTechniques - 1, bevelTechnique.getSelectedId() - 1);
    b.depth = (float) bevelDepth.getValue();
    b.up = ! bevelDown.getToggleState();
    b.size = (float) bevelSize.getValue();
    b.soften = (float) bevelSoften.getValue();
    b.useGlobalLight = bevelLight.global.getToggleState();
    b.angle = (float) bevelLight.angle.getValue();
    b.altitude = (float) bevelAltitude.getValue();
    b.antiAliased = glossAntiAliased.getToggleState();
    b.highlightMode = modeOf (highlightMode);
    b.highlightColour = highlightColour.getColourValue();
    b.highlightOpacity = (float) highlightOpacity.getValue();
    b.shadowMode = modeOf (shadowMode);
    b.shadowColour = shadowColour.getColourValue();
    b.shadowOpacity = (float) shadowOpacity.getValue();
    b.bevelContour = contourEditor.getContour();
    b.contourRange = (float) contourRange.getValue();

    auto& s = style.stroke;
    s.size = (float) strokeSize.getValue();
    s.position = strokePositions[juce::jlimit (0, 2, strokePosition.getSelectedId() - 1)];
    s.blend = modeOf (strokeBlend);
    s.opacity = (float) strokeOpacity.getValue();
    s.colour = strokeColour.getColourValue();

    auto& i = style.innerShadow;
    i.blend = modeOf (innerBlend);
    i.colour = innerColour.getColourValue();
    i.opacity = (float) innerOpacity.getValue();
    i.useGlobalLight = innerLight.global.getToggleState();
    i.angle = (float) innerLight.angle.getValue();
    i.distance = (float) innerDistance.getValue();
    i.spread = (float) innerChoke.getValue();
    i.size = (float) innerSize.getValue();

    auto& o = style.colourOverlay;
    o.blend = modeOf (overlayBlend);
    o.colour = overlayColour.getColourValue();
    o.opacity = (float) overlayOpacity.getValue();

    auto& d = style.dropShadow;
    d.blend = modeOf (dropBlend);
    d.colour = dropColour.getColourValue();
    d.opacity = (float) dropOpacity.getValue();
    d.useGlobalLight = dropLight.global.getToggleState();
    d.angle = (float) dropLight.angle.getValue();
    d.distance = (float) dropDistance.getValue();
    d.spread = (float) dropSpread.getValue();
    d.size = (float) dropSize.getValue();
    d.knockout = dropKnockout.getToggleState();

    // The shape wears it now (or, Preview off, as it was).
    if (host.apply != nullptr)
        host.apply (previewToggle.getToggleState() ? style : original, false);

    sampleDue = true;
    list->repaint();
}

void LayerStyleWindow::timerCallback()
{
    if (finished)
        return;

    // The shape went (deleted, or the layout changed under it): nothing left to style.
    if (host.alive != nullptr && ! host.alive())
    {
        finished = true;
        stopTimer();
        closeWindowOf (*this);
        return;
    }

    if (sampleDue && host.sample != nullptr)
    {
        sampleDue = false;
        sample.setImage (host.sample (previewToggle.getToggleState() ? style : original));
    }
}

void LayerStyleWindow::finish (bool keep)
{
    if (finished)
        return;

    finished = true;
    stopTimer();

    if (keep)
    {
        if (host.apply != nullptr)
            host.apply (style, true);
    }
    else if (host.cancel != nullptr)
    {
        host.cancel();
    }

    closeWindowOf (*this);
}

//==============================================================================
void LayerStyleWindow::paint (juce::Graphics& g)
{
    g.fillAll (Theme::panel);

    // The shown effect's name over its settings.
    auto title = pageArea.withHeight (26);
    g.setColour (Theme::text);
    g.setFont (Theme::font (16.0f, true));
    g.drawText (titles[(size_t) page], title, juce::Justification::centredLeft, true);
    g.setColour (Theme::outline);
    g.drawHorizontalLine (title.getBottom() + 3, (float) pageArea.getX(), (float) pageArea.getRight());

    if (! isOn (page))
    {
        g.setColour (Theme::muted);
        g.setFont (Theme::font (12.5f));
        g.drawText ("(off: tick it on the left)", title, juce::Justification::centredRight, true);
    }

    // Colour swatches get a frame, so a dark colour still shows where it is.
    g.setColour (Theme::outline.brighter (0.45f));

    for (auto* swatch : { &highlightColour, &shadowColour, &strokeColour, &innerColour, &overlayColour, &dropColour })
        if (swatch->isVisible())
            g.drawRoundedRectangle (swatch->getBounds().toFloat().expanded (1.5f), 4.0f, 1.0f);
}

void LayerStyleWindow::resized()
{
    auto area = getLocalBounds().reduced (12);

    list->setBounds (area.removeFromLeft (176));
    area.removeFromLeft (16);

    auto side = area.removeFromRight (124);
    area.removeFromRight (16);

    okButton.setBounds (side.removeFromTop (28));
    side.removeFromTop (8);
    cancelButton.setBounds (side.removeFromTop (28));
    side.removeFromTop (14);
    previewToggle.setBounds (side.removeFromTop (24));
    side.removeFromTop (12);
    sample.setBounds (side.removeFromTop (124));

    pageArea = area;
    auto content = area.withTrimmedTop (36);

    // Only the shown effect's controls.
    for (int p = 0; p < numPages; ++p)
    {
        for (auto& row : rows[(size_t) p])
        {
            if (row.label != nullptr)
                row.label->setVisible (p == page);

            for (auto& [part, width] : row.parts)
                part->setVisible (p == page);
        }
    }

    constexpr int labelWidth = 118, gap = 8;
    int y = content.getY();

    for (auto& row : rows[(size_t) page])
    {
        if (row.parts.empty())
        {
            // A heading.
            row.label->setBounds (content.getX(), y + 2, content.getWidth(), row.height);
            y += row.height + 6;
            continue;
        }

        if (row.label != nullptr)
            row.label->setBounds (content.getX(), y, labelWidth, juce::jmin (row.height, 26));

        int fixed = 0, flexible = 0;

        for (const auto& [part, width] : row.parts)
        {
            if (width > 0) fixed += width;
            else           ++flexible;
        }

        const int left = content.getX() + labelWidth;
        const int room = content.getRight() - left - gap * ((int) row.parts.size() - 1);
        const int flexWidth = flexible > 0 ? juce::jmax (20, (room - fixed) / flexible) : 0;
        int x = left;

        for (auto& [part, width] : row.parts)
        {
            const int w = width > 0 ? width : flexWidth;
            part->setBounds (x, y, w, row.height);
            x += w + gap;
        }

        y += row.height + 5;
    }
}
