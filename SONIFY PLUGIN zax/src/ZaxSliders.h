// C:\workspace\SONIFY PLUGIN zax\src\ZaxSliders.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>

//==============================================================================
//  ZaxSliders.h  —  Grex's proven sound-editor controls, as they are in Grex:
//  the vertical GoldSlider (every Zax value 0..200, right double-click resets)
//  and the two-handle BandFilterSlider (a key's low cut and high cut).
//  Only the palette moved into this file and the fonts to FontOptions.
//==============================================================================

namespace Pal
{
    // Grex's bronze
    inline constexpr juce::uint32 kAccent       = 0xFFB87A36;
    inline constexpr juce::uint32 kAccentLight  = 0xFFE5BB8D;
    inline constexpr juce::uint32 kAccentGrad   = 0xFFA97032;
    inline constexpr juce::uint32 kAccentGradDk = 0xFF5D370F;
    inline constexpr juce::uint32 kAccentDark   = 0xFF54320E;
}

// All slider visuals are LOCKED to fixed pixel sizes — handle diameter is 4× the
// track width regardless of how wide or how packed the parent frame is, so a
// 5-slider row looks the same as a 3-slider row.
//
// Visual layers (drawn bottom-up):
//   1. Medium-gray rounded backdrop                  (one per slider)
//   2. Black-gradient track (the "railway")
//   3. Bronze fill from handle position downward
//   4. Black-gradient outer handle (40 px diameter)
//   5. Bronze inner circle (14 px diameter, smaller than the handle)
//   6. Value text on top, label text on bottom
//
//   ┌─────────┐
//   │  VALUE  │   <- 14 pt
//   │ ░░░░░░░ │   <- medium-gray backdrop
//   │ ░ ███ ░ │   <- 10 px black railway / bronze fill
//   │ ░ ███ ░ │
//   │ ░ ◉◯ ░ │   <- 40 px black handle w/ bronze dot
//   │ ░ ███ ░ │
//   │ ░ ███ ░ │
//   │ ░░░░░░░ │
//   │  LABEL  │   <- 16 pt bold
//   └─────────┘
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
    { setMouseCursor(juce::MouseCursor::NormalCursor); }

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
        if (e.mods.isRightButtonDown()) { setValue(defVal, true); return; }
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

        juce::ColourGradient bronzeGrad(juce::Colour(Pal::kAccentLight), trkX - 1.0f, 0.0f,
                                        juce::Colour(Pal::kAccentDark), trkX + kTrackWidth + 1.0f, 0.0f, false);
        bronzeGrad.addColour(0.5, juce::Colour(Pal::kAccent));
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
        juce::ColourGradient innerGrad(juce::Colour(Pal::kAccentLight),
                                       cx - kInnerRadius * 0.35f,
                                       thumbY - kInnerRadius * 0.35f,
                                       juce::Colour(Pal::kAccentGradDk),
                                       cx + kInnerRadius,
                                       thumbY + kInnerRadius, true);
        innerGrad.addColour(0.5, juce::Colour(Pal::kAccentGrad));
        g.setGradientFill(innerGrad);
        g.fillEllipse(cx - kInnerRadius, thumbY - kInnerRadius,
                      kInnerRadius * 2.0f, kInnerRadius * 2.0f);

        // Thin dark rim around the bronze dot so it pops against the black handle
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawEllipse(cx - kInnerRadius + 0.2f, thumbY - kInnerRadius + 0.2f,
                      kInnerRadius * 2.0f - 0.4f, kInnerRadius * 2.0f - 0.4f, 0.7f);

        // ── Value text (top) ─────────────────────────────────────────────────
        g.setFont(juce::Font (juce::FontOptions (kValueFont)));
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.drawFittedText(formatVal(), 0, 2, (int) W, (int)(kValueAreaH - 2),
                         juce::Justification::centred, 1);

        // ── Label (bottom) ───────────────────────────────────────────────────
        g.setFont(juce::Font (juce::FontOptions (kLabelFont).withStyle ("Bold")));
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.drawFittedText(label, 0, (int)(H - kLabelAreaH), (int) W, (int) kLabelAreaH,
                         juce::Justification::centred, 1);
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

        juce::ColourGradient bronzeGrad(juce::Colour(Pal::kAccentLight), 0.0f, trkY - 1.0f,
                                        juce::Colour(Pal::kAccentDark), 0.0f, trkY + kTrackWidth + 1.0f, false);
        bronzeGrad.addColour(0.5, juce::Colour(Pal::kAccent));
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
        juce::ColourGradient innerGrad(juce::Colour(Pal::kAccentLight),
                                       thumbX - kInnerRadius * 0.35f,
                                       cy     - kInnerRadius * 0.35f,
                                       juce::Colour(Pal::kAccentGradDk),
                                       thumbX + kInnerRadius,
                                       cy     + kInnerRadius, true);
        innerGrad.addColour(0.5, juce::Colour(Pal::kAccentGrad));
        g.setGradientFill(innerGrad);
        g.fillEllipse(thumbX - kInnerRadius, cy - kInnerRadius,
                      kInnerRadius * 2.0f, kInnerRadius * 2.0f);

        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawEllipse(thumbX - kInnerRadius + 0.2f, cy - kInnerRadius + 0.2f,
                      kInnerRadius * 2.0f - 0.4f, kInnerRadius * 2.0f - 0.4f, 0.7f);

        // Value text (top), label (bottom) — same bands as the vertical layout.
        g.setFont(juce::Font (juce::FontOptions (kValueFont)));
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.drawFittedText(formatVal(), 0, 2, (int) W, (int)(kValueAreaH - 2),
                         juce::Justification::centred, 1);

        g.setFont(juce::Font (juce::FontOptions (kLabelFont).withStyle ("Bold")));
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.drawFittedText(label, 0, (int)(H - kLabelAreaH), (int) W, (int) kLabelAreaH,
                         juce::Justification::centred, 1);
    }

    juce::String formatVal() const
    {
        // Integer-valued sliders (step == 1) display without decimals — used
        // for the 0..100 normalised sliders in DrumsPopup.
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
};

//==============================================================================
// BandFilterSlider — VERTICAL two-thumb low-cut / high-cut band control.
//
// Frequency runs bottom (20 Hz) to top (20 kHz), log-mapped with 20 * 1000^norm
// (the same law the engine uses for its cutoff), so each octave gets equal
// travel.  The BOTTOM thumb is the LOW-CUT (high-pass edge, BLUE); the TOP thumb
// is the HIGH-CUT (low-pass edge, RED).  The bronze fill between them is the
// pass-band.  The thumbs cannot cross (low <= high) but MAY meet — a zero-width
// band is silence, by design.  Each thumb prints its own cut frequency inside a
// rounded-rectangular handle ("20hz", "2.5khz").  Reuses GoldSlider's recessed
// bronze track look.  onChange fires with both normalised edges (low, high).
//==============================================================================
class BandFilterSlider : public juce::Component
{
public:
    static constexpr float kTrackWidth  = 10.0f;
    static constexpr float kHandleW     = 54.0f;
    static constexpr float kHandleH     = 22.0f;
    static constexpr float kHandleRad   = 6.0f;
    static constexpr float kLabelAreaH  = 24.0f;
    static constexpr float kTopPad      = kHandleH * 0.5f + 4.0f;
    static constexpr float kBotPad      = kLabelAreaH + kHandleH * 0.5f + 4.0f;
    static constexpr float kValueFont   = 13.0f;
    static constexpr float kLabelFont   = 16.0f;

    // Fired with the two normalised cut positions (0..1 each, low <= high).
    std::function<void(float loNorm, float hiNorm)> onChange;

    explicit BandFilterSlider(const juce::String& lbl) : label(lbl) {}

    // ── Frequency mapping (shared law with the engine: 20 * 1000^norm) ─────────
    static float normToHz(float n)  { return 20.0f * std::pow(1000.0f, juce::jlimit(0.0f, 1.0f, n)); }
    static float hzToNorm(float hz) { return juce::jlimit(0.0f, 1.0f,
                                          std::log(juce::jmax(20.0f, hz) / 20.0f) / std::log(1000.0f)); }

    /** Set both edges.  Order is enforced (low <= high); they may be equal. */
    void setValues(float loN, float hiN, bool notify = false)
    {
        loN = juce::jlimit(0.0f, 1.0f, loN);
        hiN = juce::jlimit(0.0f, 1.0f, hiN);
        if (loN > hiN) { const float t = loN; loN = hiN; hiN = t; }
        if (loN == loNorm && hiN == hiNorm && ! notify) return;
        loNorm = loN; hiNorm = hiN;
        repaint();
        if (notify && onChange) onChange(loNorm, hiNorm);
    }
    float getLo() const { return loNorm; }
    float getHi() const { return hiNorm; }

    void paint(juce::Graphics& g) override
    {
        const float W = (float) getWidth(), H = (float) getHeight();
        if (W < 8.0f || H < 24.0f) return;

        const float cx   = W * 0.5f;
        const float trkX = cx - kTrackWidth * 0.5f;
        const float trkT = kTopPad;
        const float trkB = H - kBotPad;
        const float trkH = juce::jmax(8.0f, trkB - trkT);

        // ── Backdrop card (matches GoldSlider) ────────────────────────────────
        {
            const float inset = 2.0f, corner = 6.0f;
            juce::Rectangle<float> bg(inset, inset, W - inset * 2.0f, H - inset * 2.0f);
            juce::ColourGradient bgGrad(juce::Colour(0xFF454545), bg.getX(), bg.getY(),
                                        juce::Colour(0xFF323232), bg.getX(), bg.getBottom(), false);
            g.setGradientFill(bgGrad);
            g.fillRoundedRectangle(bg, corner);
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawRoundedRectangle(bg, corner, 1.0f);
        }

        // ── Recessed valley track (matches GoldSlider) ────────────────────────
        juce::ColourGradient trackGrad(juce::Colour(0xFF2E2E2E), trkX - 1.0f, 0.0f,
                                       juce::Colour(0xFF2E2E2E), trkX + kTrackWidth + 1.0f, 0.0f, false);
        trackGrad.addColour(0.5, juce::Colour(0xFF020202));
        g.setGradientFill(trackGrad);
        g.fillRoundedRectangle(trkX, trkT, kTrackWidth, trkH, kTrackWidth * 0.5f);
        g.setColour(juce::Colour(0xFF000000).withAlpha(0.85f));
        g.drawRoundedRectangle(trkX + 0.5f, trkT + 0.5f,
                               kTrackWidth - 1.0f, trkH - 1.0f, kTrackWidth * 0.5f - 0.5f, 0.8f);

        // norm 0 -> bottom, norm 1 -> top.  yHi is above yLo (high >= low).
        const float yLo = trkB - loNorm * trkH;   // low-cut  (blue, bottom)
        const float yHi = trkB - hiNorm * trkH;   // high-cut (red, top)

        // ── Bronze pass-band between the thumbs ───────────────────────────────
        {
            juce::Path clip;
            clip.addRoundedRectangle(trkX, trkT, kTrackWidth, trkH, kTrackWidth * 0.5f);
            g.saveState();
            g.reduceClipRegion(clip);
            juce::ColourGradient bronze(juce::Colour(Pal::kAccentLight), trkX - 1.0f, 0.0f,
                                        juce::Colour(Pal::kAccentDark), trkX + kTrackWidth + 1.0f, 0.0f, false);
            bronze.addColour(0.5, juce::Colour(Pal::kAccent));
            g.setGradientFill(bronze);
            g.fillRect(juce::Rectangle<float>(trkX, yHi, kTrackWidth, juce::jmax(0.0f, yLo - yHi)));
            g.restoreState();
        }

        // ── Handles: bottom = blue (low cut), top = red (high cut) ────────────
        drawHandle(g, cx, yLo, juce::Colour(0xFF2F78E6), juce::Colour(0xFF0B3F86), freqText(loNorm));
        drawHandle(g, cx, yHi, juce::Colour(0xFFE24B4A), juce::Colour(0xFF791F1F), freqText(hiNorm));

        // ── Bottom label ──────────────────────────────────────────────────────
        g.setFont(juce::Font (juce::FontOptions (kLabelFont).withStyle ("Bold")));
        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.drawFittedText(label, 0, (int) (H - kLabelAreaH), (int) W, (int) kLabelAreaH,
                         juce::Justification::centred, 1);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        activeThumb   = pickThumb((float) e.y);
        dragStartY    = (float) e.y;
        dragStartNorm = (activeThumb == 1) ? loNorm : hiNorm;
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) return;      // right button = reset only
        if (activeThumb == 0) return;
        const float delta = (dragStartY - (float) e.y) / trackH();   // drag up = increase
        const float n     = juce::jlimit(0.0f, 1.0f, dragStartNorm + delta);
        if (activeThumb == 1) setValues(juce::jmin(n, hiNorm), hiNorm, true);   // low, capped at high
        else                  setValues(loNorm, juce::jmax(n, loNorm), true);   // high, floored at low
    }
    void mouseUp(const juce::MouseEvent&) override
    { activeThumb = 0; setMouseCursor(juce::MouseCursor::NormalCursor); }

    // RIGHT double-click anywhere resets to fully open (20 Hz .. 20 kHz =
    // transparent).  Same relocation as GoldSlider, so one rule covers every
    // control in the editor rather than two that differ by widget.
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown()) setValues(0.0f, 1.0f, true);
    }

private:
    float trackTop() const { return kTopPad; }
    float trackBot() const { return (float) getHeight() - kBotPad; }
    float trackH()   const { return juce::jmax(8.0f, trackBot() - trackTop()); }
    float normToY(float n) const { return trackBot() - n * trackH(); }

    // 1 = low-cut thumb, 2 = high-cut thumb.  Nearest to the click; when the two
    // coincide, pick by which side of them the click lands (below -> low).
    int pickThumb(float y) const
    {
        const float yLo = normToY(loNorm), yHi = normToY(hiNorm);
        if (std::abs(loNorm - hiNorm) < 1.0e-4f) return (y >= yLo) ? 1 : 2;
        return (std::abs(y - yLo) <= std::abs(y - yHi)) ? 1 : 2;
    }

    void drawHandle(juce::Graphics& g, float cx, float cy,
                    juce::Colour fill, juce::Colour edge, const juce::String& txt)
    {
        const float w = juce::jmin(kHandleW, (float) getWidth() - 6.0f);
        juce::Rectangle<float> r(cx - w * 0.5f, cy - kHandleH * 0.5f, w, kHandleH);
        g.setColour(fill);
        g.fillRoundedRectangle(r, kHandleRad);
        g.setColour(juce::Colours::white.withAlpha(0.22f));           // top bevel
        g.drawRoundedRectangle(r.reduced(1.2f), kHandleRad - 1.0f, 0.8f);
        g.setColour(edge);                                            // dark rim
        g.drawRoundedRectangle(r, kHandleRad, 1.5f);
        g.setFont(juce::Font (juce::FontOptions (kValueFont).withStyle ("Bold")));
        g.setColour(juce::Colours::white);
        g.drawFittedText(txt, r.toNearestInt(), juce::Justification::centred, 1);
    }

    static juce::String freqText(float norm)
    {
        const float hz = normToHz(norm);
        if (hz < 1000.0f) return juce::String(juce::roundToInt(hz)) + "hz";
        const float k = hz / 1000.0f;
        return (k < 10.0f ? juce::String(k, 1) : juce::String(juce::roundToInt(k))) + "khz";
    }

    juce::String label;
    float loNorm = 0.0f;    // low cut  (20 Hz  = fully open low)
    float hiNorm = 1.0f;    // high cut (20 kHz = fully open high)
    int   activeThumb = 0;  // 0 none, 1 low, 2 high
    float dragStartY = 0.0f, dragStartNorm = 0.0f;
};
