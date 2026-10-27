// C:\workspace\SONIFY PLUGIN natura\src\FananLookAndFeel.cpp
#include "FananLookAndFeel.h"

#if HAS_BINARY_DATA
 #include <BinaryData.h>
#endif

FananLookAndFeel::FananLookAndFeel()
{
    using namespace Theme::Color;

    // ComboBox / dropdowns ---------------------------------------------------
    setColour (juce::ComboBox::backgroundColourId,        bgInput);
    setColour (juce::ComboBox::textColourId,              textPrimary);
    setColour (juce::ComboBox::outlineColourId,           border);
    setColour (juce::ComboBox::arrowColourId,             textSecondary);
    setColour (juce::ComboBox::buttonColourId,            bgInput);
    setColour (juce::ComboBox::focusedOutlineColourId,    accent);

    // Popup menu (combo-box dropdown list) -----------------------------------
    setColour (juce::PopupMenu::backgroundColourId,            bgPanel);
    setColour (juce::PopupMenu::textColourId,                  textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentDim);
    setColour (juce::PopupMenu::highlightedTextColourId,       textPrimary);

    // TextButton --------------------------------------------------------------
    setColour (juce::TextButton::buttonColourId,    bgPanel);
    setColour (juce::TextButton::buttonOnColourId,  accentDim);
    setColour (juce::TextButton::textColourOffId,   textPrimary);
    setColour (juce::TextButton::textColourOnId,    textPrimary);

    // Label -------------------------------------------------------------------
    setColour (juce::Label::textColourId,            textPrimary);
    setColour (juce::Label::backgroundColourId,      juce::Colours::transparentBlack);

    // Scrollbar ---------------------------------------------------------------
    setColour (juce::ScrollBar::thumbColourId,       borderStrong);
    setColour (juce::ScrollBar::trackColourId,       bgStrip);
    setColour (juce::ScrollBar::backgroundColourId,  bgWorkspace);

    // Slider text box (we draw the value ourselves inside the rotary, but
    // these stay set in case any other slider in the app needs them) -------
    setColour (juce::Slider::textBoxTextColourId,        textPrimary);
    setColour (juce::Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,   accentDim);
}

// =============================================================================
// ComboBox
// =============================================================================
void FananLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                     bool /*isButtonDown*/,
                                     int /*buttonX*/, int /*buttonY*/,
                                     int /*buttonW*/, int /*buttonH*/,
                                     juce::ComboBox& box)
{
    using namespace Theme::Color;

    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    // Pill style: fully rounded ends (radius = half-height). Triggered via a
    // per-component property so individual ComboBoxes can opt in:
    //   combo.getProperties().set ("pillStyle", true);
    const bool pillStyle = (bool) box.getProperties().getWithDefault ("pillStyle", false);
    const float r = pillStyle
                        ? (float) height * 0.5f
                        : juce::jmin (5.0f, (float) height * 0.25f);

    g.setColour (bgInput);
    g.fillRoundedRectangle (bounds, r);

    g.setColour (box.hasKeyboardFocus (false) ? accent : border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);

    const float arrowX = (float) width - (float) height * 0.7f;
    const float arrowY = (float) height * 0.5f;
    const float arrowSize = juce::jmin (6.0f, (float) height * 0.25f);

    juce::Path arrow;
    arrow.addTriangle (arrowX - arrowSize, arrowY - arrowSize * 0.4f,
                       arrowX + arrowSize, arrowY - arrowSize * 0.4f,
                       arrowX,             arrowY + arrowSize * 0.6f);
    g.setColour (textSecondary);
    g.fillPath (arrow);
}

void FananLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0,
                     box.getWidth() - (int) ((float) box.getHeight() * 0.9f),
                     box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, Theme::Color::textPrimary);
}

juce::Font FananLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return Theme::Font::regular (Theme::Font::sizeS);
}

juce::Font FananLookAndFeel::getPopupMenuFont()
{
    return Theme::Font::regular (Theme::Font::sizeM);
}

juce::Font FananLookAndFeel::getLabelFont (juce::Label& l)
{
    if (l.getFont().getHeight() > 1.0f)
        return l.getFont();
    return Theme::Font::regular (Theme::Font::sizeM);
}

void FananLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    using namespace Theme::Color;
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (bgPanel);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
}

void FananLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                          const juce::Rectangle<int>& area,
                                          bool isSeparator, bool /*isActive*/,
                                          bool isHighlighted, bool isTicked,
                                          bool /*hasSubMenu*/,
                                          const juce::String& text,
                                          const juce::String& /*shortcutKeyText*/,
                                          const juce::Drawable* /*icon*/,
                                          const juce::Colour* /*textColour*/)
{
    using namespace Theme::Color;

    if (isSeparator)
    {
        g.setColour (border);
        g.fillRect (area.reduced (8, 0).withSizeKeepingCentre (area.getWidth() - 16, 1));
        return;
    }

    if (isHighlighted)
    {
        g.setColour (accentDim);
        g.fillRoundedRectangle (area.reduced (3, 1).toFloat(), 3.0f);
    }

    g.setColour (textPrimary);
    g.setFont (getPopupMenuFont());

    auto textArea = area.reduced (10, 0);
    if (isTicked)
    {
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (4.0f, (float) area.getCentreY() - 3.0f, 6.0f, 6.0f));
        textArea = textArea.withTrimmedLeft (6);
    }

    g.setColour (textPrimary);
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);
}

// =============================================================================
// TextButton
// =============================================================================
void FananLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& /*backgroundColour*/,
                                             bool isHighlighted, bool isDown)
{
    using namespace Theme::Color;

    auto bounds = button.getLocalBounds().toFloat();
    const float r = juce::jmin (5.0f, bounds.getHeight() * 0.25f);

    auto fill = bgPanel;
    if (button.getToggleState())  fill = accentDim;
    else if (isDown)              fill = bgPanelHi.darker (0.2f);
    else if (isHighlighted)       fill = bgPanelHi;

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, r);

    g.setColour (button.getToggleState() ? accent : border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);
}

juce::Font FananLookAndFeel::getTextButtonFont (juce::TextButton&, int /*buttonHeight*/)
{
    return Theme::Font::regular (Theme::Font::sizeM);
}

// =============================================================================
// Scrollbar
// =============================================================================
void FananLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& bar,
                                      int x, int y, int width, int height,
                                      bool isVertical, int thumbStart,
                                      int thumbSize, bool isOver, bool isDown)
{
    using namespace Theme::Color;

    g.setColour (bgStrip);
    g.fillRect (x, y, width, height);

    auto thumb = isVertical
        ? juce::Rectangle<int> (x + 2, thumbStart, width - 4, thumbSize)
        : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, height - 4);

    auto thumbColour = borderStrong;
    if (isDown) thumbColour = accent;
    else if (isOver) thumbColour = borderStrong.brighter (0.2f);

    g.setColour (thumbColour);
    g.fillRoundedRectangle (thumb.toFloat(), 2.0f);

    juce::ignoreUnused (bar);
}

// =============================================================================
// Rotary slider (knob)
// -----------------------------------------------------------------------------
// Layers, drawn in order:
//   1. PNG background "knob back.png" — fills the full visual bounds.
//   2. Active arc (yellow #E1EC0F) — confined to inner workable radius.
//   3. Indicator line at current angle — same color, slightly inside arc.
//   4. Numeric value (white) — dead center of the rotary; uses the slider's
//      textFromValueFunction so the project-wide 0..100 display convention
//      (see Knob.h) just works.
//
// RADIUS POLICY:
//   - The original Avenger PNG canvas reserved 74% of half-size as the active
//     workable area.
//   - Project spec calls for shrinking that active radius by an additional
//     20%, so the painted arc/indicator are 80% of the prior size while the
//     PNG itself remains unchanged.
//   - Net inner factor = 0.74 * 0.80 = 0.592 of half-size.
// =============================================================================
juce::Image FananLookAndFeel::getKnobBackground()
{
    static const juce::Image cached = []
    {
       #if HAS_BINARY_DATA
        int dataSize = 0;
        if (const char* data = ::BinaryData::getNamedResource ("knob_back_png", dataSize))
            if (dataSize > 0)
                return juce::ImageCache::getFromMemory (data, dataSize);
       #endif
        return juce::Image{};
    }();
    return cached;
}

void FananLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPos,
                                         float rotaryStart, float rotaryEnd,
                                         juce::Slider& slider)
{
    using namespace Theme::Color;

    const float cx   = (float) x + (float) width  * 0.5f;
    const float cy   = (float) y + (float) height * 0.5f;
    const float size = (float) juce::jmin (width, height);
    const float halfSize = size * 0.5f;

    // -- 1. Background (PNG at full size) --------------------------------------
    if (auto bg = getKnobBackground(); bg.isValid())
    {
        const juce::Rectangle<float> bgArea (cx - halfSize, cy - halfSize, size, size);
        g.drawImage (bg, bgArea, juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        // Fallback so the knob is still visible without the PNG asset
        const float r = halfSize - 1.0f;
        g.setColour (bgPanel);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour (border);
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
    }

    // -- 2. Inner workable radius (74% * 80% = 59.2% of half-size) ------------
    const float innerRadius = halfSize * 0.592f;

    // -- 3. Value arc (yellow) ------------------------------------------------
    const float currentAngle = rotaryStart + sliderPos * (rotaryEnd - rotaryStart);
    const bool  bipolar      = (bool) slider.getProperties()
                                            .getWithDefault ("bipolar", false);
    const float arcRadius    = innerRadius - 4.0f;
    const juce::Colour active { knobActiveARGB };

    juce::Path valueArc;
    if (bipolar)
    {
        const float centerAngle = rotaryStart + 0.5f * (rotaryEnd - rotaryStart);
        valueArc.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f,
                                centerAngle, currentAngle, true);
    }
    else
    {
        valueArc.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f,
                                rotaryStart, currentAngle, true);
    }

    g.setColour (active);
    g.strokePath (valueArc, juce::PathStrokeType (2.5f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // -- 4. Indicator line at current angle -----------------------------------
    const float lineInner = innerRadius * 0.55f;
    const float lineOuter = innerRadius - 2.0f;

    const float sinA = std::sin (currentAngle);
    const float cosA = std::cos (currentAngle);
    const float ix = cx + sinA * lineInner;
    const float iy = cy - cosA * lineInner;
    const float ox = cx + sinA * lineOuter;
    const float oy = cy - cosA * lineOuter;

    g.setColour (active);
    g.drawLine (ix, iy, ox, oy, 2.5f);

    // -- 5. Value text (white) — dead center of the rotary --------------------
    // Pulls through the slider's textFromValueFunction, so the standard
    // 0..1 -> "0".."100" mapping from Knob.h is honored automatically.
    const auto valueText = slider.getTextFromValue (slider.getValue());
    if (valueText.isNotEmpty())
    {
        const float textHeight = juce::jmax (10.0f, innerRadius * 0.42f);
        const float textWidth  = innerRadius * 1.5f;
        const juce::Rectangle<float> textArea (cx - textWidth * 0.5f,
                                               cy - textHeight * 0.5f,
                                               textWidth, textHeight);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (textHeight, juce::Font::bold)));
        g.drawText (valueText, textArea, juce::Justification::centred, false);
    }
}
