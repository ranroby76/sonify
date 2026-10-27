// C:\workspace\SONIFY PLUGIN natura\src\GrexWidgets.h
#pragma once
//==============================================================================
//  GrexWidgets.h  -  the Grex synthesis-department controls, brought over as
//  they are: GoldSlider (the locked-geometry vertical slider), the velocity
//  curve display and the frame / button styling.  Only the palette moved
//  (Theme::Gold) and GoldSlider gained host gesture brackets.
//==============================================================================
#include <JuceHeader.h>
#include "Theme.h"

namespace VelCurve
{
    static constexpr float kMaxGamma = 2.5f;

    inline float gammaFor (float curve) noexcept
    {
        return std::pow (kMaxGamma, -juce::jlimit (-1.0f, 1.0f, curve));
    }

    /** 0..1 in, 0..1 out.  The shape, without MIDI's integer edges - the
        display draws this and apply() rounds it. */
    inline float shape (float x, float curve) noexcept
    {
        if (std::abs (curve) < 1.0e-3f) return x;
        return std::pow (juce::jlimit (0.0f, 1.0f, x), gammaFor (curve));
    }

    /** A MIDI velocity through the curve.  Never returns 0 for a note that was
        struck: velocity 0 is a note-OFF and turning a quiet hit into one would
        strand it. */
    inline int apply (int velocity, float curve) noexcept
    {
        if (velocity <= 0 || std::abs (curve) < 1.0e-3f) return velocity;
        const float out = shape ((float) velocity / 127.0f, curve) * 127.0f;
        return juce::jlimit (1, 127, (int) std::lround (out));
    }
}


/** The curve, drawn and draggable.  Vertical drag bends it; double-click
    returns to linear.  Small enough to live in half a column. */
class VelCurveDisplay : public juce::Component
{
public:
    std::function<void(float)> onChange;

    //==========================================================================
    /** The ENERGY offset currently in force on this slot's bank, 0 for none.

        THE READOUT SHOWS THE SUM, THE SLOT STORES ITS OWN VALUE.  Those are two
        different things on purpose, and conflating them was the design that got
        rejected: writing the offset back into the slot makes a performance
        control destructive.  velCurve is clamped, so a slot near an edge loses
        ground on every sweep - down 40 from 30 clamps at 0, back up 40 lands at
        40, and ten points of voicing are gone with nothing to say so.  One verse
        is enough to start it.

        So the offset stays separate and the ENGINE adds it, which it already
        does: two velocity curves in sequence add their curve values exactly.
        This only teaches the display to agree with the engine, so what you read
        is what is sounding, and moving ENERGY back leaves the slot untouched.

        Dragging still edits the BASE.  The drag applies to the number on screen
        and the base is back-computed, so what you set is what you see - which is
        the whole reason the sum is displayed rather than the base. */
    void setEnergyOffset (float c)
    {
        if (std::abs (c - energyOffset) < 1.0e-4f) return;
        energyOffset = c;
        repaint();
    }

    /** What the engine will actually apply: the slot's own curve plus ENERGY. */
    float effectiveCurve() const noexcept
    { return juce::jlimit (-1.0f, 1.0f, curve + energyOffset); }

    void setCurve (float c, juce::NotificationType n = juce::sendNotification)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, c);
        if (std::abs (v - curve) < 1.0e-4f) return;
        curve = v;
        repaint();
        if (n == juce::sendNotification && onChange) onChange (curve);
    }
    float getCurve() const noexcept { return curve; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().reduced (2).toFloat();
        g.setColour (juce::Colour (0xFF141414));
        g.fillRoundedRectangle (b, 3.0f);

        // Grid + the linear diagonal, so the bend is readable against a
        // reference rather than as an abstract line.
        g.setColour (juce::Colour (0xFF2E2E2E));
        for (int i = 1; i < 4; ++i)
        {
            const float t = (float) i / 4.0f;
            g.drawHorizontalLine ((int) (b.getY() + t * b.getHeight()), b.getX(), b.getRight());
            g.drawVerticalLine   ((int) (b.getX() + t * b.getWidth()),  b.getY(), b.getBottom());
        }
        g.setColour (juce::Colour (0xFF3A3A3A));
        g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getY(), 1.0f);

        juce::Path path;
        const int steps = juce::jmax (16, (int) b.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const float x = (float) i / (float) steps;
            const float y = VelCurve::shape (x, effectiveCurve());
            const float px = b.getX() + x * b.getWidth();
            const float py = b.getBottom() - y * b.getHeight();
            if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
        }
        g.setColour (juce::Colour (Theme::Gold::kAccent));
        g.strokePath (path, juce::PathStrokeType (2.0f));

        g.setColour (juce::Colour (0xFFC2C2C2));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (label(), getLocalBounds().removeFromBottom (14),
                    juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override { dragFrom = curve; lastY = e.position.y; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float dy = lastY - e.position.y;          // up = harder
        setCurve (dragFrom + dy / juce::jmax (40.0f, (float) getHeight()));
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { setCurve (0.0f); }

private:
    /** Reads the EFFECTIVE curve, and says so when ENERGY is moving it - a
        number that silently disagrees with the slot's stored value would be the
        worst of both designs. */
    juce::String label() const
    {
        const float e = effectiveCurve();
        juce::String s;

        if (std::abs (e) < 0.02f) s = "LINEAR";
        else s = (e < 0.0f ? "SOFT " : "HARD ")
               + juce::String ((int) std::lround (std::abs (e) * 100.0f));

        if (std::abs (energyOffset) >= 0.005f)
            s += " (ENERGY " + juce::String (energyOffset > 0.0f ? "+" : "")
               + juce::String ((int) std::lround (energyOffset * 100.0f)) + ")";

        return s;
    }

    float curve = 0.0f, dragFrom = 0.0f, lastY = 0.0f;
    float energyOffset = 0.0f;
};


class GoldSlider : public juce::Component
{
public:
    // ── Locked geometry ───────────────────────────────────────────────────────
    static constexpr float kTrackWidth     = 10.0f;
    static constexpr float kOuterRadius    = 20.0f;   // 40 px black handle (4 × track width)
    static constexpr float kInnerRadius    = 7.0f;    // 14 px bronze inner dot
    static constexpr float kValueFont      = 14.0f;
    static constexpr float kLabelFont      = 16.0f;
    static constexpr float kValueAreaH     = 20.0f;
    static constexpr float kLabelAreaH     = 24.0f;
    static constexpr float kHandleClearance = 2.0f;   // gap between handle and value/label areas
    static constexpr float kVerticalReserved =
        kValueAreaH + kLabelAreaH + 2.0f * kOuterRadius + 2.0f * kHandleClearance;
    // Horizontal mode reserves only handle margin at each end; value text /
    // label live in dedicated top / bottom bands and don't consume track length.
    static constexpr float kHorizontalReserved =
        2.0f * kOuterRadius + 2.0f * kHandleClearance;

    std::function<void(float)> onChange;

    /** Natura: host gesture brackets, so automation records a drag as one move. */
    std::function<void()> onDragStart, onDragEnd;

    /** Optional readout override — takes the raw slider value and returns what
        to print.  See valueText. */
    std::function<juce::String(float)> displayFn;

    GoldSlider(const juce::String& lbl, float minV, float maxV, float defV,
               const juce::String& unitStr_ = "")
        : label(lbl), unit(unitStr_), minVal(minV), maxVal(maxV), value(defV), defVal(defV) {}

    /** Set a snap step (e.g. 1.0f for integer 0..100 sliders).  step <= 0
        means continuous (default).  Existing value is re-quantised. */
    void setStep(float s)
    {
        step = juce::jmax(0.0f, s);
        if (step > 0.0f) value = quantise(value);
        repaint();
    }
    float getStep() const { return step; }

    /** Switch to horizontal track layout (default is vertical).  Used by the
        portamento and octave sliders in SynthesisPanel where the host frame
        is short and wide and 7 discrete octave steps need horizontal room to
        be hit-targetable. */
    void setHorizontal (bool h) noexcept { horizontal = h; repaint(); }
    bool isHorizontal() const noexcept   { return horizontal; }

    void setValue(float v, bool notify = false)
    {
        const float clamped = juce::jlimit(minVal, maxVal, v);
        const float snapped = (step > 0.0f) ? quantise(clamped) : clamped;
        if (snapped == value && ! notify) return;
        value = snapped;
        repaint();
        if (notify && onChange) onChange(value);
    }
    float getValue() const { return value; }

    void paint(juce::Graphics& g) override
    {
        if (horizontal) paintHorizontal(g);
        else            paintVertical  (g);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragX = e.x;
        dragY = e.y;
        dragVal = value;
        setMouseCursor(horizontal ? juce::MouseCursor::LeftRightResizeCursor
                                  : juce::MouseCursor::UpDownResizeCursor);
        if (! e.mods.isRightButtonDown() && onDragStart) { dragging = true; onDragStart(); }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;      // right button = reset only
        if (horizontal)
        {
            const float trkW = (float) getWidth() - kHorizontalReserved;
            if (trkW > 0.0f)
                setValue(dragVal + ((float)(e.x - dragX) / trkW) * (maxVal - minVal), true);
        }
        else
        {
            const float trkH = (float) getHeight() - kVerticalReserved;
            if (trkH > 0.0f)
                setValue(dragVal + ((float)(dragY - e.y) / trkH) * (maxVal - minVal), true);
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        if (dragging) { dragging = false; if (onDragEnd) onDragEnd(); }
    }

    //==========================================================================
    // DOUBLE-CLICK GESTURES
    //
    //   RIGHT double-click -> reset to default.  (Moved here from the left
    //                         button so the left one is free.)
    //   LEFT  double-click -> onLeftDoubleClick, if the owner set one.  Only the
    //                         GAIN sliders do; everywhere else it is inert.
    //
    // A right DRAG no longer moves the value either — otherwise a slightly
    // sloppy right double-click would nudge the very number it is resetting.
    //==========================================================================
    std::function<void()> onLeftDoubleClick;

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            if (onDragStart) onDragStart();
            setValue(defVal, true);
            if (onDragEnd) onDragEnd();
            return;
        }
        if (onLeftDoubleClick) onLeftDoubleClick();
    }

private:
    void paintVertical(juce::Graphics& g)
    {
        const float W = (float) getWidth();
        const float H = (float) getHeight();
        if (W < 4.0f || H < 8.0f) return;

        const float cx = W * 0.5f;
        const float trkX = cx - kTrackWidth * 0.5f;
        const float trkT = kValueAreaH + kOuterRadius + kHandleClearance;
        const float trkB = H - kLabelAreaH - kOuterRadius - kHandleClearance;
        const float trkH = juce::jmax(8.0f, trkB - trkT);

        // ── Medium-gray rounded backdrop (one card per slider) ────────────────
        {
            const float inset  = 2.0f;
            const float corner = 6.0f;
            juce::Rectangle<float> bg(inset, inset, W - inset * 2.0f, H - inset * 2.0f);

            juce::ColourGradient bgGrad(juce::Colour(0xFF454545), bg.getX(), bg.getY(),
                                        juce::Colour(0xFF323232), bg.getX(), bg.getBottom(), false);
            g.setGradientFill(bgGrad);
            g.fillRoundedRectangle(bg, corner);

            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawRoundedRectangle(bg, corner, 1.0f);
        }

        // ── Recessed "valley" railway ─────────────────────────────────────────
        // Symmetric cross-section: the two outer edges are lighter (light from
        // above catches the lips of the groove), and the centre is darkest
        // (the floor of the U-shaped valley sits in shadow). This reads as a
        // channel cut INTO the panel, the opposite of a raised cylinder.
        juce::ColourGradient trackGrad(juce::Colour(0xFF2E2E2E), trkX - 1.0f, 0.0f,
                                       juce::Colour(0xFF2E2E2E), trkX + kTrackWidth + 1.0f, 0.0f, false);
        trackGrad.addColour(0.5, juce::Colour(0xFF020202));
        g.setGradientFill(trackGrad);
        g.fillRoundedRectangle(trkX, trkT, kTrackWidth, trkH, kTrackWidth * 0.5f);

        // Thin black outline reads as the groove's hard edge against the panel
        g.setColour(juce::Colour(0xFF000000).withAlpha(0.85f));
        g.drawRoundedRectangle(trkX + 0.5f, trkT + 0.5f,
                               kTrackWidth - 1.0f, trkH - 1.0f, kTrackWidth * 0.5f - 0.5f, 0.8f);

        // ── Bronze fill from handle position down ─────────────────────────────
        const float norm   = (maxVal > minVal) ? (value - minVal) / (maxVal - minVal) : 0.0f;
        const float thumbY = trkB - norm * trkH;

        juce::Path pillPath;
        pillPath.addRoundedRectangle(trkX, trkT, kTrackWidth, trkH, kTrackWidth * 0.5f);
        g.saveState();
        g.reduceClipRegion(pillPath);

        juce::ColourGradient bronzeGrad(juce::Colour(Theme::Gold::kAccentLight), trkX - 1.0f, 0.0f,
                                        juce::Colour(Theme::Gold::kAccentDark), trkX + kTrackWidth + 1.0f, 0.0f, false);
        bronzeGrad.addColour(0.5, juce::Colour(Theme::Gold::kAccent));
        g.setGradientFill(bronzeGrad);
        g.fillRect(juce::Rectangle<float>(trkX, thumbY, kTrackWidth, trkB - thumbY));
        g.restoreState();

        // ── Outer BLACK gradient handle (40 px diameter) ──────────────────────
        // Radial highlight in the upper-left, falling off to black at the rim.
        juce::ColourGradient outerGrad(juce::Colour(0xFF6A6A6A),
                                       cx - kOuterRadius * 0.4f,
                                       thumbY - kOuterRadius * 0.4f,
                                       juce::Colour(0xFF000000),
                                       cx + kOuterRadius,
                                       thumbY + kOuterRadius, true);
        outerGrad.addColour(0.45, juce::Colour(0xFF2E2E2E));
        outerGrad.addColour(0.85, juce::Colour(0xFF0A0A0A));
        g.setGradientFill(outerGrad);
        g.fillEllipse(cx - kOuterRadius, thumbY - kOuterRadius,
                      kOuterRadius * 2.0f, kOuterRadius * 2.0f);

        // Subtle bevel rim + dark inset
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawEllipse(cx - kOuterRadius + 0.6f, thumbY - kOuterRadius + 0.6f,
                      kOuterRadius * 2.0f - 1.2f, kOuterRadius * 2.0f - 1.2f, 0.9f);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.drawEllipse(cx - kOuterRadius + 0.2f, thumbY - kOuterRadius + 0.2f,
                      kOuterRadius * 2.0f - 0.4f, kOuterRadius * 2.0f - 0.4f, 0.8f);

        // ── Inner BRONZE circle (the "dot") ───────────────────────────────────
        juce::ColourGradient innerGrad(juce::Colour(Theme::Gold::kAccentLight),
                                       cx - kInnerRadius * 0.35f,
                                       thumbY - kInnerRadius * 0.35f,
                                       juce::Colour(Theme::Gold::kAccentGradDk),
                                       cx + kInnerRadius,
                                       thumbY + kInnerRadius, true);
        innerGrad.addColour(0.5, juce::Colour(Theme::Gold::kAccentGrad));
        g.setGradientFill(innerGrad);
        g.fillEllipse(cx - kInnerRadius, thumbY - kInnerRadius,
                      kInnerRadius * 2.0f, kInnerRadius * 2.0f);

        // Thin dark rim around the bronze dot so it pops against the black handle
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawEllipse(cx - kInnerRadius + 0.2f, thumbY - kInnerRadius + 0.2f,
                      kInnerRadius * 2.0f - 0.4f, kInnerRadius * 2.0f - 0.4f, 0.7f);

        // ── Value text (top) ─────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(kValueFont)));
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.drawFittedText(formatVal(), 0, 2, (int) W, (int)(kValueAreaH - 2),
                         juce::Justification::centred, 1, 0.6f);

        // ── Label (bottom) ───────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(kLabelFont, juce::Font::bold)));
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.drawFittedText(label, 0, (int)(H - kLabelAreaH), (int) W, (int) kLabelAreaH,
                         juce::Justification::centred, 1, 0.5f);
    }

    // ── Horizontal mode ──────────────────────────────────────────────────────
    // Mirrors paintVertical: same value-top / label-bottom bands, same handle
    // sizes, same bronze + black palette.  Only difference is that the track
    // runs LEFT to RIGHT through the middle of the component, the bronze fill
    // grows from the left edge toward the handle, and the U-channel gradient
    // runs perpendicular (top→middle→bottom) so the recessed cross-section
    // reads the same way.
    void paintHorizontal(juce::Graphics& g)
    {
        const float W = (float) getWidth();
        const float H = (float) getHeight();
        if (W < 8.0f || H < 4.0f) return;

        // The middle band sits between the value text (top) and label (bottom).
        const float cy   = kValueAreaH + kOuterRadius + kHandleClearance;
        const float trkY = cy - kTrackWidth * 0.5f;
        const float trkL = kOuterRadius + kHandleClearance;
        const float trkR = W - kOuterRadius - kHandleClearance;
        const float trkW = juce::jmax(8.0f, trkR - trkL);

        // Background card
        {
            const float inset  = 2.0f;
            const float corner = 6.0f;
            juce::Rectangle<float> bg(inset, inset, W - inset * 2.0f, H - inset * 2.0f);

            juce::ColourGradient bgGrad(juce::Colour(0xFF454545), bg.getX(), bg.getY(),
                                        juce::Colour(0xFF323232), bg.getX(), bg.getBottom(), false);
            g.setGradientFill(bgGrad);
            g.fillRoundedRectangle(bg, corner);

            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawRoundedRectangle(bg, corner, 1.0f);
        }

        // Recessed "valley" track — gradient runs vertically across the track
        // (light at top/bottom edges, dark at centre) for the same U-channel
        // read as the vertical case.
        juce::ColourGradient trackGrad(juce::Colour(0xFF2E2E2E), 0.0f, trkY - 1.0f,
                                       juce::Colour(0xFF2E2E2E), 0.0f, trkY + kTrackWidth + 1.0f, false);
        trackGrad.addColour(0.5, juce::Colour(0xFF020202));
        g.setGradientFill(trackGrad);
        g.fillRoundedRectangle(trkL, trkY, trkW, kTrackWidth, kTrackWidth * 0.5f);

        g.setColour(juce::Colour(0xFF000000).withAlpha(0.85f));
        g.drawRoundedRectangle(trkL + 0.5f, trkY + 0.5f,
                               trkW - 1.0f, kTrackWidth - 1.0f, kTrackWidth * 0.5f - 0.5f, 0.8f);

        // Bronze fill from LEFT edge to handle x
        const float norm   = (maxVal > minVal) ? (value - minVal) / (maxVal - minVal) : 0.0f;
        const float thumbX = trkL + norm * trkW;

        juce::Path pillPath;
        pillPath.addRoundedRectangle(trkL, trkY, trkW, kTrackWidth, kTrackWidth * 0.5f);
        g.saveState();
        g.reduceClipRegion(pillPath);

        juce::ColourGradient bronzeGrad(juce::Colour(Theme::Gold::kAccentLight), 0.0f, trkY - 1.0f,
                                        juce::Colour(Theme::Gold::kAccentDark), 0.0f, trkY + kTrackWidth + 1.0f, false);
        bronzeGrad.addColour(0.5, juce::Colour(Theme::Gold::kAccent));
        g.setGradientFill(bronzeGrad);
        g.fillRect(juce::Rectangle<float>(trkL, trkY, thumbX - trkL, kTrackWidth));
        g.restoreState();

        // Outer BLACK handle centred at (thumbX, cy)
        juce::ColourGradient outerGrad(juce::Colour(0xFF6A6A6A),
                                       thumbX - kOuterRadius * 0.4f,
                                       cy     - kOuterRadius * 0.4f,
                                       juce::Colour(0xFF000000),
                                       thumbX + kOuterRadius,
                                       cy     + kOuterRadius, true);
        outerGrad.addColour(0.45, juce::Colour(0xFF2E2E2E));
        outerGrad.addColour(0.85, juce::Colour(0xFF0A0A0A));
        g.setGradientFill(outerGrad);
        g.fillEllipse(thumbX - kOuterRadius, cy - kOuterRadius,
                      kOuterRadius * 2.0f, kOuterRadius * 2.0f);

        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.drawEllipse(thumbX - kOuterRadius + 0.6f, cy - kOuterRadius + 0.6f,
                      kOuterRadius * 2.0f - 1.2f, kOuterRadius * 2.0f - 1.2f, 0.9f);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.drawEllipse(thumbX - kOuterRadius + 0.2f, cy - kOuterRadius + 0.2f,
                      kOuterRadius * 2.0f - 0.4f, kOuterRadius * 2.0f - 0.4f, 0.8f);

        // Inner BRONZE dot
        juce::ColourGradient innerGrad(juce::Colour(Theme::Gold::kAccentLight),
                                       thumbX - kInnerRadius * 0.35f,
                                       cy     - kInnerRadius * 0.35f,
                                       juce::Colour(Theme::Gold::kAccentGradDk),
                                       thumbX + kInnerRadius,
                                       cy     + kInnerRadius, true);
        innerGrad.addColour(0.5, juce::Colour(Theme::Gold::kAccentGrad));
        g.setGradientFill(innerGrad);
        g.fillEllipse(thumbX - kInnerRadius, cy - kInnerRadius,
                      kInnerRadius * 2.0f, kInnerRadius * 2.0f);

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawEllipse(thumbX - kInnerRadius + 0.2f, cy - kInnerRadius + 0.2f,
                      kInnerRadius * 2.0f - 0.4f, kInnerRadius * 2.0f - 0.4f, 0.7f);

        // Value text (top), label (bottom) — same bands as the vertical layout.
        g.setFont(juce::Font(juce::FontOptions(kValueFont)));
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.drawFittedText(formatVal(), 0, 2, (int) W, (int)(kValueAreaH - 2),
                         juce::Justification::centred, 1, 0.6f);

        g.setFont(juce::Font(juce::FontOptions(kLabelFont, juce::Font::bold)));
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.drawFittedText(label, 0, (int)(H - kLabelAreaH), (int) W, (int) kLabelAreaH,
                         juce::Justification::centred, 1, 0.5f);
    }

    juce::String formatVal() const
    {
        // Integer-valued sliders (step == 1) display without decimals — used
        // for the 0..100 normalised sliders in DrumsPopup.
        // Natura: a readout override always wins (note names, choice names,
        // times), the integer shortcut is only for plain stepped numbers.
        if (displayFn) return displayFn (value);

        if (step >= 1.0f && unit.isEmpty())
            return juce::String((int) std::round(value));

        // A knob whose VALUE is not what it MEANS supplies its own readout.
        //
        // A/D/R carry 0..100 because the drag is linear between min and max
        // while their real scale is exponential, so the number under the knob
        // would otherwise be a POSITION, not a time.  That is exactly how a
        // release of "70" changed from 2.1 s to 629 ms in a rebuild with
        // nothing on screen disagreeing.  Show the time and it cannot recur.
        if (displayFn) return displayFn (value);

        if (unit == "s")   return value < 1.0f ? juce::String((int)(value*1000)) + "ms"
                                               : juce::String(value, 1) + "s";
        if (unit == "ms")  return juce::String((int)value) + "ms";
        if (unit == "Hz")  return juce::String(value, 2) + "Hz";
        if (unit == "dB")  return juce::String(value, 1) + "dB";
        if (unit == "st")  return juce::String(value, 1) + "st";
        if (unit == "BPM") return juce::String((int)value) + " BPM";
        return juce::String(value, 2) + unit;
    }

    float quantise(float v) const
    {
        if (step <= 0.0f) return v;
        const float n = std::round((v - minVal) / step);
        return juce::jlimit(minVal, maxVal, minVal + n * step);
    }

    juce::String label, unit;
    float minVal, maxVal, value, defVal;
    float step = 0.0f;     // 0 = continuous
    bool  horizontal = false;
    int   dragX = 0;
    int   dragY = 0;
    float dragVal = 0.f;
    bool  dragging = false;
};


namespace InstrEditStyle
{
    //==========================================================================
    // PANEL LABEL COLOUR — #C2C2C2, and the ONE place it is decided.
    //
    // The left panel's captions and separator lines used to be pure #FFFFFF,
    // while the two macro triggers beside them rendered at #C2C2C2 (a plain
    // TextButton goes through the LookAndFeel, which does not draw at full
    // white).  Measured off a screenshot, that is a 61-unit gap on every
    // channel — enough that the panel read as two different families of control
    // sitting in one frame.
    //
    // Everything on that panel now uses this.  Pure white stays where it earns
    // its contrast: the amber-lit states, the LED speculars, and the tab row.
    //==========================================================================
    inline const juce::Colour kPanelLabel { 0xFFC2C2C2 };

    constexpr float kFrameCorner    = 20.0f;
    constexpr int   kFrameSidePad   = 14;
    constexpr int   kFrameTopPad    = 8;
    constexpr int   kFrameTitleH    = 22;
    constexpr int   kFrameTitleGap  = 6;
    constexpr int   kFrameBottomPad = 14;

    inline void paintComponentFrame(juce::Graphics& g, juce::Rectangle<int> bounds,
                                    const juce::String& title)
    {
        const auto fb = bounds.toFloat();

        g.setColour(juce::Colour(0xFF1A1A1A));
        g.fillRoundedRectangle(fb, kFrameCorner);

        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(fb.reduced(0.5f), kFrameCorner, 1.0f);

        if (title.isNotEmpty())
        {
            g.setColour(juce::Colour(Theme::Gold::kAccent));
            g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
            g.drawFittedText(title,
                             bounds.getX() + 12, bounds.getY() + kFrameTopPad,
                             bounds.getWidth() - 24, kFrameTitleH,
                             juce::Justification::centred, 1);
        }
    }

    inline juce::Rectangle<int> componentFrameContent(juce::Rectangle<int> bounds)
    {
        const int top = kFrameTopPad + kFrameTitleH + kFrameTitleGap;
        return { bounds.getX() + kFrameSidePad,
                 bounds.getY() + top,
                 bounds.getWidth()  - kFrameSidePad * 2,
                 bounds.getHeight() - top - kFrameBottomPad };
    }

    inline void styleSquareButton(juce::TextButton& b, bool active)
    {
        const auto fill = active ? juce::Colour(Theme::Gold::kAccent) : juce::Colour(0xFF2A2A2A);

        // BOTH ids, and that is the whole fix for "my toggles never go orange".
        // LookAndFeel_V4::drawButtonBackground looks up buttonOnColourId when the
        // button's TOGGLE STATE is set and buttonColourId when it is not — so a
        // plain button styled here went amber correctly, while any button with
        // setClickingTogglesState(true) fell through to the untouched default the
        // moment it was switched on.
        b.setColour(juce::TextButton::buttonColourId,   fill);
        b.setColour(juce::TextButton::buttonOnColourId, fill);
        // Black on the amber fill, white on the inert one - the label has to
        // read against whichever background it just got.
        const auto text = active ? juce::Colours::black : juce::Colours::white;
        b.setColour(juce::TextButton::textColourOffId, text);
        b.setColour(juce::TextButton::textColourOnId,  text);
    }

    inline void drawSectionHeader(juce::Graphics& g, const juce::String& label,
                                  int x, int y, int w, int h = 14)
    {
        g.setColour(juce::Colour(Theme::Gold::kAccent));
        g.setFont(juce::Font(juce::FontOptions((float) juce::jmin(h - 2, 11), juce::Font::bold)));
        g.drawText(label, x, y, w, h, juce::Justification::centred, false);
    }
    inline void drawDivider(juce::Graphics& g, int x, int yTop, int yBot)
    {
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawVerticalLine(x, (float) yTop, (float) yBot);
    }
}
