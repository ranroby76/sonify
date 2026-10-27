// C:\workspace\SONIFY PLUGIN natura\src\FananLookAndFeel.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// =============================================================================
// FananLookAndFeel
//
// Custom L&F that paints all standard JUCE controls using the Theme colors.
// Flat, no gradients, subtle 1px borders, rounded corners on cards/inputs.
// Owned by MainComponent and applied to its descendant tree.
//
// Rotary sliders (knobs) use the PNG background "knob back.png" embedded
// via juce_add_binary_data. The active arc + indicator is rendered on top
// in the brand yellow #E1EC0F, confined to the inner 74% of the canvas
// (matches the active area set in KnobMan).
// =============================================================================
class FananLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FananLookAndFeel();
    ~FananLookAndFeel() override = default;

    // Brand colour for the knob arc + indicator
    static constexpr juce::uint32 knobActiveARGB = 0xFFE1EC0F;

    // ComboBox -----------------------------------------------------------------
    void drawComboBox (juce::Graphics&, int width, int height,
                       bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override;

    // Buttons ------------------------------------------------------------------
    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // Scrollbars ---------------------------------------------------------------
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&,
                        int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition,
                        int thumbSize, bool isMouseOver,
                        bool isMouseDown) override;

    // Rotary slider (knob) -----------------------------------------------------
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

private:
    /** Returns the PNG knob background, loaded lazily from BinaryData and
        cached. Returns an invalid Image if the asset wasn't embedded. */
    static juce::Image getKnobBackground();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FananLookAndFeel)
};
