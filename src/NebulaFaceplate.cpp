// NebulaFaceplate.cpp: see NebulaFaceplate.h. Keep this file byte-identical in NebulaVerb and Sonify.
#include "NebulaFaceplate.h"

#include <cmath>

namespace nebula_ui
{
namespace
{
using juce::Colour;
using juce::Graphics;
using juce::Justification;
using Rect = juce::Rectangle<float>;

const float pi = juce::MathConstants<float>::pi;

namespace col
{
    const Colour label   { 0xffe4ddcf };
    const Colour dim     { 0xffa39d91 };
    const Colour faint   { 0xff8a857b };
    const Colour amber   { 0xfff0b453 };
    const Colour amberHi { 0xffffc56b };
    const Colour ink     { 0xff0a0a0b };
    const Colour seam    { 0xff050506 };
}

juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float a, float b, float v) { return a * std::pow (b / a, v); },
             [] (float a, float b, float x) { return std::log (x / a) / std::log (b / a); },
             [] (float a, float b, float x) { return juce::jlimit (a, b, x); } };
}

juce::String fmtMs (float ms)   { return ms < 10.0f ? juce::String (ms, 1) + " ms" : (ms < 1000.0f ? juce::String (juce::roundToInt (ms)) + " ms" : juce::String (ms / 1000.0f, 2) + " s"); }
juce::String fmtHz (float hz)   { return hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " Hz" : juce::String (hz / 1000.0f, hz < 10000.0f ? 1 : 0) + " kHz"; }
juce::String fmtPct (float v)   { return juce::String (juce::roundToInt (v)) + " %"; }
juce::String fmtSec (float s)   { return juce::String (s, s < 10.0f ? 1 : 0) + " s"; }

juce::Font labelFont (float h, float kerning = 0.16f)
{
    return juce::Font (juce::FontOptions (h, juce::Font::bold)).withExtraKerningFactor (kerning).withHorizontalScale (0.92f);
}

juce::Font monoFont (float h)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain));
}

juce::Point<float> polar (juce::Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }

// Machined-metal look: a conic gradient approximated with thin pie wedges. The highlights are fixed in
// screen space, so a knob keeps the same lighting while its pointer turns.
void fillConicMetal (Graphics& g, Rect r, float rotation = 0.35f)
{
    static const struct { float pos; juce::uint32 argb; } stops[] = {
        { 0.00f, 0xff7d8086 }, { 0.10f, 0xffeceef0 }, { 0.24f, 0xff6a6d73 }, { 0.38f, 0xffc9cbcf },
        { 0.52f, 0xff55585d }, { 0.68f, 0xffe2e3e5 }, { 0.84f, 0xff6f7277 }, { 1.00f, 0xff7d8086 } };
    const int n = 96;
    for (int k = 0; k < n; ++k)
    {
        const float t = ((float) k + 0.5f) / (float) n;
        int s = 0; while (s < 6 && t > stops[s + 1].pos) ++s;
        const float u = (t - stops[s].pos) / (stops[s + 1].pos - stops[s].pos);
        g.setColour (Colour (stops[s].argb).interpolatedWith (Colour (stops[s + 1].argb), u));
        juce::Path p;
        p.addPieSegment (r, rotation + 2.0f * pi * (float) k / (float) n, rotation + 2.0f * pi * (float) (k + 1) / (float) n + 0.02f, 0.0f);
        g.fillPath (p);
    }
}

void drawLamp (Graphics& g, juce::Point<float> c, float r, bool on, Colour hot = Colour (0xfff5a623))
{
    g.setColour (Colour (0xff26272b)); g.fillEllipse (Rect (2 * r + 4, 2 * r + 4).withCentre (c));
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (hot.withAlpha (0.55f), c.x, c.y, hot.withAlpha (0.0f), c.x + r * 3.2f, c.y, true));
        g.fillEllipse (Rect (r * 6.4f, r * 6.4f).withCentre (c));
        g.setGradientFill (juce::ColourGradient (Colour (0xfffff2c9), c.x - r * 0.25f, c.y - r * 0.3f, hot.darker (0.5f), c.x + r, c.y + r, true));
    }
    else
        g.setGradientFill (juce::ColourGradient (Colour (0xff5a3d16), c.x - r * 0.25f, c.y - r * 0.3f, Colour (0xff2a1c0b), c.x + r, c.y + r, true));
    g.fillEllipse (Rect (2 * r, 2 * r).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (Rect (2 * r, 2 * r).withCentre (c), 0.8f);
}

// Alternating 3-degree wedges over a disc: the fine knurl of the design's knob skirt and toggle ring.
void fillKnurl (Graphics& g, Rect disc, Colour light, Colour dark)
{
    g.setColour (dark); g.fillEllipse (disc);
    g.setColour (light);
    juce::Path ridges;
    const int wedges = 120;
    for (int k = 0; k < wedges; k += 2)
        ridges.addPieSegment (disc, 2.0f * pi * (float) k / (float) wedges, 2.0f * pi * (float) (k + 1) / (float) wedges, 0.0f);
    g.fillPath (ridges);
}

// The design's knob (NebulaVerb mockup, Main artboard): a dark knurled skirt casting a long soft
// shadow, an amber pointer on the skirt, a machined conic cap over the middle 62%. Only the pointer
// turns; the light and every highlight stay put. `room` is how far past the skirt the component
// reaches: the soft shadow stays inside it, so it never ends in a hard clipped edge.
void drawKnobBody (Graphics& g, juce::Point<float> c, float r, float angle, float room)
{
    // cast shadows: a long soft one and a tight dark one where it meets the panel
    juce::Path disc; disc.addEllipse (Rect (2 * r, 2 * r).withCentre (c));
    const int dy = juce::roundToInt (juce::jmin (r * 0.27f, room * 0.45f));
    const int blur = juce::jmax (2, juce::roundToInt (juce::jmin (r * 0.45f, room - (float) dy)));
    juce::DropShadow (juce::Colours::black.withAlpha (0.75f), blur, { 0, dy }).drawForPath (g, disc);
    juce::DropShadow (juce::Colours::black.withAlpha (0.9f), 4, { 0, 3 }).drawForPath (g, disc);

    // knurled skirt with its rim: lit along the top edge, shaded inside the bottom
    const auto skirt = Rect (2 * r, 2 * r).withCentre (c);
    fillKnurl (g, skirt, Colour (0xff2c2e33), Colour (0xff141518));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), c.x, c.y - r, juce::Colours::transparentWhite, c.x, c.y - r * 0.6f, false));
    g.drawEllipse (skirt.reduced (0.6f), 1.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, c.x, c.y + r * 0.5f, juce::Colours::black.withAlpha (0.8f), c.x, c.y + r, false));
    g.drawEllipse (skirt.reduced (1.2f), 2.4f);

    // pointer: 3 px, from 90% to 64% of the radius, with its glow
    const auto p0 = polar (c, r * 0.64f, angle), p1 = polar (c, r * 0.90f, angle);
    juce::Path ptr; ptr.startNewSubPath (p0); ptr.lineTo (p1);
    const Colour amberPtr (0xfff4bb57);
    g.setColour (amberPtr.withAlpha (0.16f)); g.strokePath (ptr, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (amberPtr.withAlpha (0.32f)); g.strokePath (ptr, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (amberPtr); g.strokePath (ptr, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // metal cap
    const float cr = r * 0.62f;
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (Rect (2 * cr + 3, 2 * cr + 3).withCentre ({ c.x, c.y + 1.5f }));
    fillConicMetal (g, Rect (2 * cr, 2 * cr).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.22f)); g.drawEllipse (Rect (2 * cr * 0.6f, 2 * cr * 0.6f).withCentre (c), 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.55f), c.x, c.y - cr, juce::Colours::black.withAlpha (0.45f), c.x, c.y + cr, false));
    g.drawEllipse (Rect (2 * cr - 1.2f, 2 * cr - 1.2f).withCentre (c), 1.4f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (Rect (2 * cr, 2 * cr).withCentre (c), 0.8f);
}

void drawTicks (Graphics& g, juce::Point<float> c, float rInner, float rOuter, float a0, float a1, int count)
{
    g.setColour (Colour (0xffcfc7b6).withAlpha (0.72f));
    for (int k = 0; k < count; ++k)
    {
        const float a = a0 + (a1 - a0) * (float) k / (float) (count - 1);
        g.drawLine (juce::Line<float> (polar (c, rInner, a), polar (c, rOuter, a)), 1.4f);
    }
}

void drawSectionTitle (Graphics& g, Rect r, const juce::String& text)
{
    const auto f = labelFont (11.0f, 0.3f);
    const float tw = juce::GlyphArrangement::getStringWidth (f, text) + 20.0f;
    g.setFont (f); g.setColour (col::dim);
    g.drawText (text, r, Justification::centred, false);
    const float y = r.getCentreY(), half = (r.getWidth() - tw) * 0.5f;
    if (half > 6.0f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, r.getX(), y, Colour (0xff3a3b40), r.getX() + half * 0.5f, y, false));
        g.fillRect (Rect (r.getX(), y, half, 1.0f));
        g.setGradientFill (juce::ColourGradient (Colour (0xff3a3b40), r.getRight() - half * 0.5f, y, juce::Colours::transparentBlack, r.getRight(), y, false));
        g.fillRect (Rect (r.getRight() - half, y, half, 1.0f));
    }
}

//==============================================================================
struct NebulaLookAndFeel final : juce::LookAndFeel_V4
{
    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto area = juce::Rectangle<int> (x, y, w, h).toFloat();
        const float outer = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
        const float margin = (float) s.getProperties().getWithDefault ("tickMargin", 12.0);
        const auto c = area.getCentre();
        drawTicks (g, c, outer - 5.5f, outer - 0.5f, a0, a1, 11);
        drawKnobBody (g, c, outer - margin, a0 + pos * (a1 - a0), margin);
        if (s.hasKeyboardFocus (false)) { g.setColour (col::amber.withAlpha (0.6f)); g.drawEllipse (area.reduced (1.0f), 1.0f); }
    }
};

//==============================================================================
// A knob on one parameter. The slider runs 0..1 (the knob's travel); rangeOf() maps it to the
// parameter's real value, so the knob turns the same way in every host.
struct KnobUnit final : juce::Component
{
    KnobUnit (Model& m, Param p, juce::String labelText, juce::String minText, juce::String maxText, int knobDiameter, int tickMargin)
        : model (m), param (p), range (rangeOf (p)),
          label (std::move (labelText)), lo (std::move (minText)), hi (std::move (maxText)), wrap (knobDiameter + 2 * tickMargin)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (1.25f * pi, 2.75f * pi, true);
        slider.setMouseDragSensitivity (220);
        slider.setRange (0.0, 1.0, 0.0);
        slider.setDoubleClickReturnValue (true, range.convertTo0to1 (defaultOf (p)));
        slider.getProperties().set ("tickMargin", (double) tickMargin);
        slider.setTitle (label);
        slider.textFromValueFunction = [this] (double v) { return textOf (param, toReal (v)); };
        slider.onDragStart = [this] { model.beginGesture (param); };
        slider.onDragEnd = [this] { model.endGesture (param); };
        slider.onValueChange = [this]
        {
            if (! syncing)
                model.set (param, toReal (slider.getValue()));
            repaint();
        };
        addAndMakeVisible (slider);
        sync();
    }

    float toReal (double v) const { return range.snapToLegalValue (range.convertFrom0to1 ((float) juce::jlimit (0.0, 1.0, v))); }

    // From the model: automation, presets and undo move the knob too.
    void sync()
    {
        if (slider.isMouseButtonDown())
            return;
        const double v = range.convertTo0to1 (juce::jlimit (range.start, range.end, model.get (param)));
        if (std::abs (v - slider.getValue()) > 1.0e-6)
        {
            const juce::ScopedValueSetter<bool> s (syncing, true);
            slider.setValue (v, juce::dontSendNotification);
            repaint();
        }
    }

    int unitHeight() const { return 14 + 6 + wrap + 6 + 21; }

    void resized() override { slider.setBounds ((getWidth() - wrap) / 2, 20, wrap, wrap); }

    void paint (Graphics& g) override
    {
        const auto w = (float) getWidth();
        g.setFont (labelFont (12.0f)); g.setColour (col::label);
        g.drawText (label.toUpperCase(), Rect (-20.0f, 0.0f, w + 40.0f, 14.0f), Justification::centred, false);

        const auto sb = slider.getBounds().toFloat();
        g.setFont (labelFont (9.0f, 0.1f)); g.setColour (col::dim);
        // keep both labels inside the unit: anything outside its bounds is clipped
        const float lx = juce::jmax (0.0f, sb.getX() - 4.0f), rx = juce::jmin (w, sb.getRight() + 4.0f);
        if (wrap >= 100)   // small knobs sit too close together for end labels; their readout shows the value
        {
            g.drawText (lo.toUpperCase(), Rect (lx, sb.getBottom() - 8.0f, 44.0f, 10.0f), Justification::centredLeft, false);
            g.drawText (hi.toUpperCase(), Rect (rx - 44.0f, sb.getBottom() - 8.0f, 44.0f, 10.0f), Justification::centredRight, false);
        }

        const auto box = Rect (66.0f, 21.0f).withCentre ({ w * 0.5f, sb.getBottom() + 6.0f + 10.5f });
        g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (box.translated (0.0f, 1.0f), 3.0f);
        g.setColour (col::ink); g.fillRoundedRectangle (box, 3.0f);
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
        g.setFont (monoFont (13.0f)); g.setColour (col::amber);
        g.drawText (slider.getTextFromValue (slider.getValue()), box, Justification::centred, false);
    }

    Model& model;
    Param param;
    juce::NormalisableRange<float> range;
    juce::String label, lo, hi;
    int wrap;
    bool syncing = false;
    juce::Slider slider;
};

//==============================================================================
// Chrome bat-handle toggle with a lamp. `inlineLayout` puts label, lamp and lever in one row (header bypass).
struct LeverSwitch final : juce::Button
{
    LeverSwitch (Model& m, Param p, const juce::String& text, bool inlineLayout)
        : juce::Button (text), model (m), param (p), caption (text), isInline (inlineLayout)
    {
        setClickingTogglesState (true);
        setTitle (text);
        onClick = [this]
        {
            model.beginGesture (param);
            model.set (param, getToggleState() ? 1.0f : 0.0f);
            model.endGesture (param);
        };
        sync();
    }

    void sync()
    {
        const bool on = model.get (param) > 0.5f;
        if (on != getToggleState())
            setToggleState (on, juce::dontSendNotification);
    }

    void paintButton (Graphics& g, bool over, bool) override
    {
        const bool on = getToggleState();
        const auto b = getLocalBounds().toFloat();
        juce::Point<float> lampC, nutC;
        if (isInline)
        {
            g.setFont (labelFont (11.0f)); g.setColour (over ? col::label : col::dim);
            g.drawText (caption.toUpperCase(), b.withWidth (b.getWidth() - 74.0f), Justification::centredRight, false);
            lampC = { b.getRight() - 56.0f, b.getCentreY() }; nutC = { b.getRight() - 22.0f, b.getCentreY() };
        }
        else
        {
            g.setFont (labelFont (12.0f)); g.setColour (col::label);
            g.drawText (caption.toUpperCase(), Rect (-20.0f, 0.0f, b.getWidth() + 40.0f, 14.0f), Justification::centred, false);
            lampC = { b.getCentreX(), 28.0f }; nutC = { b.getCentreX(), 76.0f };
        }
        drawLamp (g, lampC, 6.0f, on);

        if (! isInline)
        {
            g.setFont (labelFont (8.0f, 0.14f)); g.setColour (col::faint);
            g.drawText ("ON", Rect (0.0f, nutC.y - 38.0f, b.getWidth(), 8.0f), Justification::centred, false);
            g.drawText ("OFF", Rect (0.0f, nutC.y + 30.0f, b.getWidth(), 8.0f), Justification::centred, false);
        }
        drawBatToggle (g, nutC, on);
        if (hasKeyboardFocus (false)) { g.setColour (col::amber.withAlpha (0.6f)); g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 1.0f); }
    }

    // The design's toggle (NebulaVerb mockup, Main artboard): a knurled ring, a chrome hex nut, a
    // domed face with its bore, and a tapered chrome bat with a ball tip, up for on and down for off.
    // Lit from the upper left in screen space, so the bat keeps its highlight either way.
    static void drawBatToggle (Graphics& g, juce::Point<float> c, bool up)
    {
        // knurled ring, 30 px, with its cast shadow and a groove inside its edge
        const auto ringR = Rect (30.0f, 30.0f).withCentre (c);
        juce::Path ring; ring.addEllipse (ringR);
        juce::DropShadow (juce::Colours::black.withAlpha (0.85f), 8, { 0, 5 }).drawForPath (g, ring);
        juce::DropShadow (juce::Colours::black, 1, { 0, 1 }).drawForPath (g, ring);
        fillKnurl (g, ringR, Colour (0xff4b4d53), Colour (0xff141518));
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawEllipse (Rect (19.0f, 19.0f).withCentre (c), 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.28f), c.x, c.y - 15.0f, juce::Colours::black.withAlpha (0.8f), c.x, c.y + 15.0f, false));
        g.drawEllipse (ringR.reduced (0.6f), 1.2f);

        // hex nut, 22 px, flats top and bottom; its chrome runs from the upper left to the lower right
        const auto hexR = Rect (22.0f, 22.0f).withCentre (c);
        juce::Path hex;
        hex.startNewSubPath (hexR.getRelativePoint (0.25f, 0.04f));
        hex.lineTo (hexR.getRelativePoint (0.75f, 0.04f)); hex.lineTo (hexR.getRelativePoint (1.0f, 0.5f));
        hex.lineTo (hexR.getRelativePoint (0.75f, 0.96f)); hex.lineTo (hexR.getRelativePoint (0.25f, 0.96f));
        hex.lineTo (hexR.getRelativePoint (0.0f, 0.5f)); hex.closeSubPath();
        {
            const float a = juce::degreesToRadians (160.0f), half = 0.5f * 22.0f * (std::abs (std::sin (a)) + std::abs (std::cos (a)));
            const juce::Point<float> dir (std::sin (a), -std::cos (a));
            juce::ColourGradient chrome (Colour (0xfff4f5f6), c - dir * half, Colour (0xff3c3f44), c + dir * half, false);
            chrome.addColour (0.26, Colour (0xffbfc2c6)); chrome.addColour (0.50, Colour (0xff6b6e74)); chrome.addColour (0.68, Colour (0xffa3a6ab));
            g.setGradientFill (chrome); g.fillPath (hex);
        }

        // domed face, 14 px, and the bore the bat comes out of
        const auto faceR = Rect (14.0f, 14.0f).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (faceR.expanded (1.0f));
        {
            const juce::Point<float> hot (faceR.getX() + 0.38f * 14.0f, faceR.getY() + 0.32f * 14.0f);
            juce::ColourGradient dome (juce::Colours::white, hot, Colour (0xff4a4d52), hot + juce::Point<float> (8.7f, 9.5f), true);
            dome.addColour (0.30, Colour (0xffd4d6d9)); dome.addColour (0.68, Colour (0xff7f8288));
            g.setGradientFill (dome); g.fillEllipse (faceR);
        }
        const auto boreR = Rect (8.0f, 8.0f).withCentre (c);
        g.setColour (juce::Colours::white.withAlpha (0.35f)); g.fillEllipse (boreR.translated (0.0f, 1.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black, c.x, c.y - 0.8f, Colour (0xff2b2c30), c.x + 4.0f, c.y + 3.2f, true));
        g.fillEllipse (boreR);

        // the bat: a tapered shaft, narrow at the bore and wide at the tip, then a ball on its end
        const float s = up ? -1.0f : 1.0f;   // which way it points
        juce::Path shaft;
        shaft.startNewSubPath (c.x - 2.16f, c.y + s * 1.0f);  shaft.lineTo (c.x + 2.16f, c.y + s * 1.0f);
        shaft.lineTo (c.x + 5.28f, c.y + s * 23.0f);          shaft.lineTo (c.x - 5.28f, c.y + s * 23.0f);
        shaft.closeSubPath();
        const auto ball = Rect (12.0f, 10.0f).withCentre ({ c.x, c.y + s * 22.0f });
        juce::Path bat (shaft); bat.addEllipse (ball);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 3, { 1, 4 }).drawForPath (g, bat);

        juce::ColourGradient chrome (Colour (0xff4f5258), c.x - 6.0f, 0.0f, Colour (0xff33363a), c.x + 6.0f, 0.0f, false);
        chrome.addColour (0.22, Colour (0xffc4c7cb)); chrome.addColour (0.38, juce::Colours::white);
        chrome.addColour (0.58, Colour (0xffa2a5aa)); chrome.addColour (0.80, Colour (0xff5b5e64));
        g.setGradientFill (chrome); g.fillPath (shaft);

        const juce::Point<float> hot (ball.getX() + 0.4f * 12.0f, ball.getY() + 0.3f * 10.0f);
        juce::ColourGradient sphere (juce::Colours::white, hot, Colour (0xff45484d), hot + juce::Point<float> (7.2f, 7.0f), true);
        sphere.addColour (0.28, Colour (0xffdcdee1)); sphere.addColour (0.72, Colour (0xff80838a));
        g.setGradientFill (sphere); g.fillEllipse (ball);
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, ball.getCentreX(), ball.getCentreY(), juce::Colours::black.withAlpha (0.5f), ball.getCentreX(), ball.getBottom(), false));
        g.drawEllipse (ball.reduced (0.5f), 1.0f);
    }

    Model& model;
    Param param;
    juce::String caption;
    bool isInline;
};

//==============================================================================
struct TypeSelector final : juce::Component
{
    TypeSelector (Model& m) : model (m)
    {
        for (const auto& t : model.types()) names.add (t.name.toUpperCase());
        setTitle ("Algorithm");
        setWantsKeyboardFocus (true);
        sync();
    }

    int count() const { return juce::jmax (2, names.size()); }

    void sync()
    {
        const int i = juce::jlimit (0, count() - 1, juce::roundToInt (model.get (Param::type)));
        if (i != index) { index = i; repaint(); }
    }

    float angleFor (int i) const { return juce::degreesToRadians (-135.0f + (float) i * 270.0f / (float) (count() - 1)); }
    juce::Point<float> centre() const { return { (float) getWidth() * 0.5f, 136.0f }; }
    // labels sit on an ellipse wider than the knob, so the two names at the top do not run together
    Rect labelBox (int i) const
    {
        const float a = angleFor (i);
        return Rect (86.0f, 20.0f).withCentre ({ centre().x + 121.0f * std::sin (a), centre().y - 97.0f * std::cos (a) });
    }

    void paint (Graphics& g) override
    {
        drawSectionTitle (g, Rect (0.0f, 0.0f, (float) getWidth(), 12.0f), "ALGORITHM");
        const auto c = centre();
        drawTicks (g, c, 48.5f, 53.5f, angleFor (0), angleFor (count() - 1), count());
        drawKnobBody (g, c, 42.0f, angleFor (index), 24.0f);
        g.setFont (labelFont (11.0f));
        for (int i = 0; i < names.size(); ++i)
        {
            const bool sel = i == index;
            if (sel) { g.setColour (col::amberHi.withAlpha (0.18f)); g.drawText (names[i], labelBox (i).translated (0.0f, 0.6f).expanded (1.0f), Justification::centred, false); }
            g.setColour (sel ? col::amberHi : (i == hover ? col::label : col::dim));
            g.drawText (names[i], labelBox (i), Justification::centred, false);
        }
    }

    void setIndex (int i)
    {
        i = juce::jlimit (0, count() - 1, i);
        model.beginGesture (Param::type);
        model.set (Param::type, (float) i);
        model.endGesture (Param::type);
        index = i;
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        int h = -1; for (int i = 0; i < names.size(); ++i) if (labelBox (i).contains (e.position)) h = i;
        if (h != hover) { hover = h; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { if (hover != -1) { hover = -1; repaint(); } }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragStart = -1;
        for (int i = 0; i < names.size(); ++i) if (labelBox (i).contains (e.position)) { setIndex (i); return; }
        if (e.position.getDistanceFrom (centre()) < 46.0f) dragStart = index;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStart >= 0 && std::abs (e.getDistanceFromDragStartY()) > 6) setIndex (dragStart - e.getDistanceFromDragStartY() / 18);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragStart >= 0 && ! e.mouseWasDraggedSinceMouseDown()) setIndex ((index + 1) % count());
        dragStart = -1;
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY != 0.0f) setIndex (index + (w.deltaY > 0 ? 1 : -1));
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::rightKey || k == juce::KeyPress::upKey)   { setIndex (index + 1); return true; }
        if (k == juce::KeyPress::leftKey  || k == juce::KeyPress::downKey) { setIndex (index - 1); return true; }
        return false;
    }

    Model& model;
    juce::StringArray names;
    int index = -1, hover = -1, dragStart = -1;
};

//==============================================================================
// Amber display: a drawing of the reverb's shape from the current settings (not a measurement of the audio).
struct DecayDisplay final : juce::Component
{
    explicit DecayDisplay (Model& m) : model (m), shapes (m.types()) { sync(); }

    struct Snapshot
    {
        float v[12] = {};
        bool operator== (const Snapshot& o) const { for (int i = 0; i < 12; ++i) if (v[i] != o.v[i]) return false; return true; }
    };

    Snapshot read() const
    {
        static const Param ids[12] = { Param::type, Param::decay, Param::predelay, Param::damping, Param::earlyLate, Param::modulation,
                                       Param::shimmer, Param::feedback, Param::gate, Param::hold, Param::release, Param::bypass };
        Snapshot s; for (int i = 0; i < 12; ++i) s.v[i] = model.get (ids[i]); return s;
    }

    void sync() { const auto s = read(); if (! (s == last)) { last = s; repaint(); } }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (Colour (0xff2b2d31), 0.0f, 0.0f, Colour (0xff121316), 0.0f, b.getHeight(), false));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

        const auto glass = b.reduced (6.0f);
        juce::Path clip; clip.addRoundedRectangle (glass, 5.0f);
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (Colour (0xff3a2408), glass.getCentreX(), glass.getBottom() + 30.0f, Colour (0xff0b0906), glass.getCentreX(), glass.getY() - 40.0f, true));
        g.fillRect (glass);

        if (shapes.empty())
            return;

        const float type = last.v[0], decay = last.v[1], pd = last.v[2] * 0.001f, damp = last.v[3] * 0.01f, late = last.v[4] * 0.01f;
        const bool shim = last.v[6] > 0.5f, gate = last.v[8] > 0.5f, byp = last.v[11] > 0.5f;
        const float fb = last.v[7] * 0.01f, hold = last.v[9] * 0.001f, rel = last.v[10] * 0.001f;
        const auto& td = shapes[(size_t) juce::jlimit (0, (int) shapes.size() - 1, (int) type)];

        const float T = pd + juce::jmax (0.4f, decay * 0.9f) + 0.05f;
        const float x0 = glass.getX() + 8.0f, xw = glass.getWidth() - 16.0f, base = glass.getBottom() - 25.0f, top = glass.getY() + 26.0f, H = base - top;
        auto X = [&] (float t) { return x0 + xw * t / T; };

        g.setColour (Colour (0xffffbe64).withAlpha (0.08f));
        for (int i = 0; i <= 4; ++i) g.fillRect (Rect (x0, top + H * (float) i / 4.0f, xw, 1.0f));
        for (int i = 1; i <= 4; ++i) g.fillRect (Rect (x0 + xw * (float) i / 5.0f, top, 1.0f, H));

        auto env = [&] (float t, float dec)
        {
            if (t < pd) return 0.0f;
            float e = std::exp (-6.9f * (t - pd) / dec) * (1.0f - std::exp (-(t - pd) / 0.03f));
            if (gate) { const float gt = pd + hold + 0.05f; if (t > gt) e *= std::exp (-(t - gt) / juce::jmax (0.01f, rel / 4.0f)); }
            return e;
        };
        const float lateW = 0.35f + 0.65f * late, hfMult = std::pow (td.hiMult, 4.0f * damp * damp);
        juce::Path tail, hf, sh, area;
        area.startNewSubPath (x0, base);
        const int n = 160;
        for (int i = 0; i <= n; ++i)
        {
            const float t = T * (float) i / (float) n, x = X (t);
            const float y = base - env (t, decay) * lateW * H, yh = base - env (t, juce::jmax (0.05f, decay * hfMult)) * lateW * H;
            if (i == 0) { tail.startNewSubPath (x, y); hf.startNewSubPath (x, yh); } else { tail.lineTo (x, y); hf.lineTo (x, yh); }
            area.lineTo (x, y);
            if (shim)
            {
                const float s = t < pd ? 0.0f : (1.0f - std::exp (-(t - pd) / (decay * 0.25f + 0.05f))) * env (t, decay * (1.0f + fb)) * (0.3f + 0.6f * fb);
                if (i == 0) sh.startNewSubPath (x, base - s * H); else sh.lineTo (x, base - s * H);
            }
        }
        area.lineTo (x0 + xw, base); area.closeSubPath();

        g.setColour (Colour (0xffffc56b).withAlpha (0.35f));
        const float dashes[2] = { 2.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (X (pd), top - 4.0f, X (pd), base), dashes, 2, 1.0f);
        g.setColour (Colour (0xfff5a623).withAlpha (0.16f)); g.fillPath (area);

        // early reflections, from the type's own pattern
        g.setColour (Colour (0xffffd58a));
        const float early = 1.0f - late;
        for (int k = 0; k < td.erTaps; ++k)
        {
            const float u = ((float) k + 0.5f) / (float) td.erTaps;
            const float t = pd + 0.001f * (td.erStartMs + (td.erSpanMs - td.erStartMs) * std::pow (u, 1.35f));
            if (t > T) break;
            g.fillRect (Rect (X (t) - 0.8f, base - early * (0.95f - 0.6f * u) * H, 1.6f, early * (0.95f - 0.6f * u) * H));
        }
        juce::Path dashed; const float d2[2] = { 3.0f, 3.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, hf, d2, 2);
        g.setColour (Colour (0xffffd58a).withAlpha (0.55f)); g.fillPath (dashed);
        if (shim)
        {
            juce::Path dotted; const float d3[2] = { 1.5f, 3.0f };
            juce::PathStrokeType (1.4f).createDashedStroke (dotted, sh, d3, 2);
            g.setColour (Colour (0xff9fd4ff)); g.fillPath (dotted);
        }
        g.setColour (Colour (0xfff5a623).withAlpha (0.25f)); g.strokePath (tail, juce::PathStrokeType (5.0f));
        g.setColour (Colour (0xfff5a623)); g.strokePath (tail, juce::PathStrokeType (2.0f));

        g.setFont (monoFont (12.0f)); g.setColour (col::amberHi);
        const auto textRow = glass.reduced (10.0f, 6.0f);
        g.drawText (td.name.toUpperCase(), textRow.withHeight (14.0f), Justification::centredLeft, false);
        g.drawText ("RT60 " + textOf (Param::decay, decay), textRow.withHeight (14.0f), Justification::centredRight, false);
        g.setFont (monoFont (11.0f)); g.setColour (col::amberHi.withAlpha (0.75f));
        const auto bottomRow = textRow.withTop (textRow.getBottom() - 13.0f);
        g.drawText ("PRE " + textOf (Param::predelay, last.v[2]), bottomRow, Justification::centredLeft, false);
        juce::StringArray tags; if (shim) tags.add ("SHIMMER"); if (gate) tags.add ("GATE"); if (byp) tags.add ("BYPASS");
        g.drawText (tags.isEmpty() ? juce::String ("STEREO") : tags.joinIntoString (" / "), bottomRow, Justification::centredRight, false);

        // glass reflection
        juce::Path glare; glare.startNewSubPath (glass.getX(), glass.getY()); glare.lineTo (glass.getX() + glass.getWidth() * 0.55f, glass.getY());
        glare.lineTo (glass.getX(), glass.getY() + glass.getHeight() * 0.45f); glare.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.09f), glass.getX(), glass.getY(), juce::Colours::white.withAlpha (0.01f), glass.getX() + 80.0f, glass.getY() + 60.0f, false));
        g.fillPath (glare);
    }

    Model& model;
    std::vector<TypeShape> shapes;
    Snapshot last;
};

//==============================================================================
struct LedMeter final : juce::Component
{
    explicit LedMeter (Model& m) : model (m) { setOpaque (true); }

    static constexpr int segments = 16;

    // The lights 3 dB apart, the highest at -3 dBFS; the top one is the clip light (see sync).
    static int litFor (float peak)
    {
        const float db = peak > 1.0e-5f ? 20.0f * std::log10 (peak) : -100.0f;   // (no juce_audio_basics here)
        return juce::jlimit (0, segments - 1, (int) std::floor ((db + 45.0f) / 3.0f) + 1);
    }

    // The clip light: only a sample past 0 dBFS lights it (the model counts every over, so none
    // falls between two looks), and it stays lit 1.5 s.
    void sync()
    {
        const auto now = juce::Time::getMillisecondCounter();
        const int l = litFor (model.outputPeak (0)), r = litFor (model.outputPeak (1));
        bool changed = l != litL || r != litR;
        litL = l; litR = r;

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto count = model.outputOvers (ch);
            const bool fresh = seenOvers && count != lastOvers[ch];
            lastOvers[ch] = count;
            if (fresh || model.outputPeak (ch) > 1.0f) clipUntil[ch] = now + 1500;
            const bool c = now < clipUntil[ch];
            if (c != clip[ch]) { clip[ch] = c; changed = true; }
        }

        seenOvers = true;   // (overs from before the window opened don't count)
        if (changed) repaint();
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff08080a));
        const float segH = ((float) getHeight() - 12.0f - (float) (segments - 1) * 2.0f) / (float) segments;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < segments; ++i)
            {
                const bool lit = i == segments - 1 ? clip[ch] : i < (ch == 0 ? litL : litR);
                const Colour c = i == segments - 1 ? Colour (0xffff5a3c) : (i >= segments - 4 ? Colour (0xffffd35a) : Colour (0xfff5a623));
                const Rect seg (5.0f + (float) ch * 14.0f, (float) getHeight() - 6.0f - segH - (float) i * (segH + 2.0f), 10.0f, segH);
                if (lit) { g.setColour (c.withAlpha (0.28f)); g.fillRect (seg.expanded (1.5f)); }
                g.setColour (lit ? c : Colour (0xff24180a)); g.fillRect (seg);
            }
    }

    Model& model;
    int litL = 0, litR = 0;
    bool clip[2] = { false, false }, seenOvers = false;
    unsigned lastOvers[2] = { 0, 0 };
    juce::uint32 clipUntil[2] = { 0, 0 };
};

//==============================================================================
// Static faceplate artwork. Buffered to an image so knob repaints never redraw it.
struct Backdrop final : juce::Component
{
    explicit Backdrop (const Model& m) : title (m.title()), badgeCode (m.badgeCode()), footer (m.footer())
    {
        setInterceptsMouseClicks (false, false);
        setBufferedToImage (true);
    }

    static void screw (Graphics& g, juce::Point<float> c)
    {
        fillConicMetal (g, Rect (14.0f, 14.0f).withCentre (c), 0.7f);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (Rect (14.0f, 14.0f).withCentre (c), 0.8f);
        g.setColour (Colour (0xff1b1c1f));
        g.drawLine (juce::Line<float> (polar (c, 5.0f, 0.95f), polar (c, 5.0f, 0.95f + pi)), 2.0f);
    }

    static void seamV (Graphics& g, float x, float y0, float y1)
    {
        g.setColour (col::seam); g.fillRect (Rect (x, y0, 1.0f, y1 - y0));
        g.setColour (juce::Colours::white.withAlpha (0.045f)); g.fillRect (Rect (x + 1.0f, y0, 1.0f, y1 - y0));
    }
    static void seamH (Graphics& g, float y, float x0, float x1)
    {
        g.setColour (col::seam); g.fillRect (Rect (x0, y, x1 - x0, 1.0f));
        g.setColour (juce::Colours::white.withAlpha (0.045f)); g.fillRect (Rect (x0, y + 1.0f, x1 - x0, 1.0f));
    }

    // The plate's name in the header: 27 px, widely spaced; a longer name tightens, then shrinks, to
    // stay clear of the badge.
    static juce::Font titleFont (const juce::String& text)
    {
        constexpr float room = 222.0f;
        for (float kerning = 0.34f; kerning >= 0.12f; kerning -= 0.02f)
        {
            auto f = juce::Font (juce::FontOptions (27.0f, juce::Font::bold)).withExtraKerningFactor (kerning);
            if (juce::GlyphArrangement::getStringWidth (f, text) <= room)
                return f;
        }
        for (float size = 26.0f; size >= 14.0f; size -= 1.0f)
        {
            auto f = juce::Font (juce::FontOptions (size, juce::Font::bold)).withExtraKerningFactor (0.12f);
            if (juce::GlyphArrangement::getStringWidth (f, text) <= room)
                return f;
        }
        return juce::Font (juce::FontOptions (14.0f, juce::Font::bold)).withExtraKerningFactor (0.12f);
    }

    void paint (Graphics& g) override
    {
        const float W = (float) Faceplate::designW, H = (float) Faceplate::designH;
        g.fillAll (Colour (0xff070708));

        // rack ears
        for (int side = 0; side < 2; ++side)
        {
            const Rect ear (side == 0 ? 0.0f : W - 38.0f, 0.0f, 38.0f, H);
            juce::ColourGradient eg (Colour (0xff17181b), ear.getX(), 0.0f, Colour (0xff1a1b1e), ear.getRight(), 0.0f, false);
            eg.addColour (side == 0 ? 0.6 : 0.4, Colour (0xff26282c));
            g.setGradientFill (eg); g.fillRect (ear);
            for (float cy : { 57.0f, H - 57.0f })
            {
                const auto slot = Rect (12.0f, 26.0f).withCentre ({ ear.getCentreX(), cy });
                g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (slot.translated (0.0f, 1.0f), 6.0f);
                g.setColour (col::seam); g.fillRoundedRectangle (slot, 6.0f);
            }
        }
        g.setColour (juce::Colours::black); g.fillRect (Rect (37.0f, 0.0f, 1.0f, H)); g.fillRect (Rect (W - 38.0f, 0.0f, 1.0f, H));

        // faceplate
        const Rect plate (38.0f, 0.0f, W - 76.0f, H);
        juce::ColourGradient pg (Colour (0xff1c1d21), 0.0f, 0.0f, Colour (0xff0f1012), 0.0f, H, false);
        pg.addColour (0.38, Colour (0xff131417));
        g.setGradientFill (pg); g.fillRect (plate);
        g.setColour (juce::Colours::white.withAlpha (0.013f));
        for (float x = plate.getX(); x < plate.getRight(); x += 3.0f) g.fillRect (Rect (x, 0.0f, 1.0f, H));
        g.setColour (juce::Colours::white.withAlpha (0.09f)); g.fillRect (Rect (plate.getX(), 0.0f, plate.getWidth(), 1.0f));

        // header
        seamH (g, 74.0f, plate.getX(), plate.getRight());
        screw (g, { 71.0f, 37.0f }); screw (g, { W - 71.0f, 37.0f });
        g.setFont (titleFont (title)); g.setColour (Colour (0xffece6d8));
        g.drawText (title, Rect (106.0f, 14.0f, 260.0f, 30.0f), Justification::centredLeft, false);
        g.setFont (labelFont (11.0f, 0.42f)); g.setColour (col::dim);
        g.drawText (juce::String::fromUTF8 ("SPACE \xc2\xb7 HALL \xc2\xb7 REVERB"), Rect (107.0f, 47.0f, 280.0f, 14.0f), Justification::centredLeft, false);

        const Rect badge (338.0f, 22.0f, 262.0f, 30.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (badge.translated (0.0f, 1.0f), 4.0f);
        g.setGradientFill (juce::ColourGradient (Colour (0xff0c0c0e), 0.0f, badge.getY(), Colour (0xff151619), 0.0f, badge.getBottom(), false));
        g.fillRoundedRectangle (badge, 4.0f);
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)).withExtraKerningFactor (0.2f)); g.setColour (col::amber);
        g.drawText (badgeCode, badge.withTrimmedLeft (14.0f).withWidth (60.0f), Justification::centredLeft, false);
        g.setFont (labelFont (11.0f, 0.22f)); g.setColour (col::dim);
        g.drawText ("STEREO REVERB PROCESSOR", badge.withTrimmedLeft (72.0f), Justification::centredLeft, false);

        seamV (g, 1218.0f, 16.0f, 58.0f);
        g.setFont (labelFont (11.0f)); g.setColour (col::dim);
        g.drawText ("POWER", Rect (1232.0f, 30.0f, 60.0f, 14.0f), Justification::centredLeft, false);
        {
            const juce::Point<float> c { 1318.0f, 37.0f };
            const Colour blue (0xff4aa3ff);
            g.setColour (Colour (0xff2a2c30)); g.fillEllipse (Rect (28.0f, 28.0f).withCentre (c));
            g.setGradientFill (juce::ColourGradient (blue.withAlpha (0.5f), c.x, c.y, blue.withAlpha (0.0f), c.x + 34.0f, c.y, true));
            g.fillEllipse (Rect (68.0f, 68.0f).withCentre (c));
            g.setGradientFill (juce::ColourGradient (Colour (0xffe8f4ff), c.x - 3.0f, c.y - 4.0f, Colour (0xff0f4d9c), c.x + 9.0f, c.y + 9.0f, true));
            g.fillEllipse (Rect (22.0f, 22.0f).withCentre (c));
            g.setColour (juce::Colours::black); g.drawEllipse (Rect (28.0f, 28.0f).withCentre (c), 1.0f);
        }

        // body seams and section titles
        seamV (g, 384.0f, 94.0f, 530.0f);
        seamV (g, 1126.0f, 128.0f, 330.0f);
        seamV (g, 658.0f, 347.0f, 500.0f);
        seamV (g, 968.0f, 347.0f, 500.0f);
        drawSectionTitle (g, Rect (408.0f, 128.0f, 700.0f, 12.0f), "SPACE");
        drawSectionTitle (g, Rect (1144.0f, 128.0f, 232.0f, 12.0f), "OUTPUT");
        drawSectionTitle (g, Rect (408.0f, 347.0f, 232.0f, 12.0f), "TONE");
        drawSectionTitle (g, Rect (676.0f, 347.0f, 274.0f, 12.0f), "SHIMMER");
        drawSectionTitle (g, Rect (986.0f, 347.0f, 390.0f, 12.0f), "GATE");
        g.setFont (labelFont (9.0f)); g.setColour (col::dim);
        g.drawText (juce::String::fromUTF8 ("L \xc2\xb7 R"), Rect (1316.0f, 306.0f, 44.0f, 10.0f), Justification::centred, false);

        // footer
        seamH (g, 550.0f, plate.getX(), plate.getRight());
        g.setFont (labelFont (10.0f, 0.3f)); g.setColour (col::faint);
        g.drawText (juce::String::fromUTF8 ("DRAG KNOBS \xc2\xb7 CTRL-DRAG FINE \xc2\xb7 DOUBLE-CLICK RESET"), Rect (64.0f, 552.0f, 600.0f, 28.0f), Justification::centredLeft, false);
        g.drawText (footer, Rect (W - 464.0f, 552.0f, 400.0f, 28.0f), Justification::centredRight, false);
    }

    juce::String title, badgeCode, footer;
};
} // namespace

//==============================================================================
juce::NormalisableRange<float> rangeOf (Param p)
{
    switch (p)
    {
        case Param::type:       return { 0.0f, 7.0f, 1.0f };
        case Param::mix:        return { 0.0f, 100.0f, 0.1f };
        case Param::predelay:   return { 0.0f, 500.0f, 0.1f, 0.5f };
        case Param::decay:      return logRange (0.2f, 30.0f);
        case Param::modulation: return { 0.0f, 100.0f, 0.1f };
        case Param::earlyLate:  return { 0.0f, 100.0f, 0.1f };
        case Param::lowCut:     return logRange (20.0f, 1000.0f);
        case Param::highCut:    return logRange (1000.0f, 20000.0f);
        case Param::damping:    return { 0.0f, 100.0f, 0.1f };
        case Param::feedback:   return { 0.0f, 100.0f, 0.1f };
        case Param::shift:      return { -24.0f, 24.0f, 1.0f };
        case Param::split:      return logRange (100.0f, 4000.0f);
        case Param::threshold:  return { -60.0f, 0.0f, 0.1f };
        case Param::attack:     return logRange (0.1f, 100.0f);
        case Param::hold:       return { 0.0f, 1000.0f, 1.0f, 0.5f };
        case Param::release:    return logRange (10.0f, 2000.0f);
        case Param::shimmer:
        case Param::gate:
        case Param::bypass:     return { 0.0f, 1.0f, 1.0f };
    }
    return { 0.0f, 1.0f };
}

float defaultOf (Param p)
{
    switch (p)
    {
        case Param::type:       return 2.0f;
        case Param::mix:        return 30.0f;
        case Param::predelay:   return 20.0f;
        case Param::decay:      return 3.6f;
        case Param::modulation: return 35.0f;
        case Param::earlyLate:  return 55.0f;
        case Param::lowCut:     return 20.0f;
        case Param::highCut:    return 20000.0f;
        case Param::damping:    return 50.0f;
        case Param::feedback:   return 40.0f;
        case Param::shift:      return 12.0f;
        case Param::split:      return 400.0f;
        case Param::threshold:  return -30.0f;
        case Param::attack:     return 2.0f;
        case Param::hold:       return 250.0f;
        case Param::release:    return 150.0f;
        case Param::shimmer:
        case Param::gate:
        case Param::bypass:     return 0.0f;
    }
    return 0.0f;
}

juce::String textOf (Param p, float v)
{
    switch (p)
    {
        case Param::predelay:
        case Param::attack:
        case Param::hold:
        case Param::release:    return fmtMs (v);
        case Param::decay:      return fmtSec (v);
        case Param::lowCut:
        case Param::highCut:
        case Param::split:      return fmtHz (v);
        case Param::earlyLate:  { const int l = juce::roundToInt (v); return "E" + juce::String (100 - l) + "/L" + juce::String (l); }
        case Param::shift:      { const int st = juce::roundToInt (v); return (st > 0 ? "+" : "") + juce::String (st) + " st"; }
        case Param::threshold:  return juce::String (juce::roundToInt (v)) + " dB";
        case Param::shimmer:
        case Param::gate:
        case Param::bypass:     return v > 0.5f ? "On" : "Off";
        case Param::type:       return juce::String (juce::roundToInt (v));
        case Param::mix:
        case Param::modulation:
        case Param::damping:
        case Param::feedback:   return fmtPct (v);
    }
    return juce::String (v);
}

//==============================================================================
struct Faceplate::Impl final : juce::Component, private juce::Timer
{
    explicit Impl (Model& m)
        : model (m), backdrop (m), display (m), types (m), meter (m),
          bypass (m, Param::bypass, "Bypass", true),
          shimmerSw (m, Param::shimmer, "On", false),
          gateSw (m, Param::gate, "On", false)
    {
        setLookAndFeel (&lnf);
        addAndMakeVisible (backdrop);
        addAndMakeVisible (display); addAndMakeVisible (types); addAndMakeVisible (meter);
        addAndMakeVisible (bypass); addAndMakeVisible (shimmerSw); addAndMakeVisible (gateSw);
        shimmerSw.setTitle ("Shimmer"); gateSw.setTitle ("Gate");

        auto add = [this] (std::vector<KnobUnit*>& group, Param id, const char* name, const char* lo, const char* hi, int size, int margin)
        {
            auto* k = knobs.add (new KnobUnit (model, id, name, lo, hi, size, margin));
            addAndMakeVisible (k); group.push_back (k);
        };
        add (space, Param::predelay,   "Pre-Delay",  "0",     "500",  88, 12);
        add (space, Param::decay,      "Decay",      "0.2",   "30 s", 88, 12);
        add (space, Param::modulation, "Modulation", "0",     "100",  88, 12);
        add (space, Param::earlyLate,  "Early-Late", "Early", "Late", 88, 12);
        add (out,   Param::mix,        "Mix",        "Dry",   "Wet", 104, 12);
        add (tone,  Param::lowCut,     "Low-Cut",    "20",    "1k",   54, 10);
        add (tone,  Param::highCut,    "High-Cut",   "1k",    "20k",  54, 10);
        add (tone,  Param::damping,    "Damping",    "0",     "100",  54, 10);
        add (shim,  Param::feedback,   "Feedback",   "0",     "100",  54, 10);
        add (shim,  Param::shift,      "Shift",      "-24",   "+24",  54, 10);
        add (shim,  Param::split,      "Split",      "100",   "4k",   54, 10);
        add (gate,  Param::threshold,  "Threshold",  "-60",   "0",    54, 10);
        add (gate,  Param::attack,     "Attack",     "0.1",   "100",  54, 10);
        add (gate,  Param::hold,       "Hold",       "0",     "1 s",  54, 10);
        add (gate,  Param::release,    "Release",    "10",    "2 s",  54, 10);

        timerCallback();
        startTimerHz (30);
    }

    ~Impl() override { stopTimer(); setLookAndFeel (nullptr); }

    static void layoutRow (const std::vector<KnobUnit*>& units, int x0, int x1, int y, int unitW)
    {
        const float slot = (float) (x1 - x0) / (float) units.size();
        for (size_t i = 0; i < units.size(); ++i)
            units[i]->setBounds (x0 + juce::roundToInt (slot * ((float) i + 0.5f)) - unitW / 2, y, unitW, units[i]->unitHeight());
    }

    void resized() override
    {
        backdrop.setBounds (getLocalBounds());
        display.setBounds (64, 110, 296, 162);
        types.setBounds (64, 290, 296, 214);
        bypass.setBounds (1040, 4, 164, 66);   // the bat and its shadow swing about 30 px either side of y 37

        layoutRow (space, 408, 1108, 152, 128);
        out[0]->setBounds (1150, 152, 150, out[0]->unitHeight());
        meter.setBounds (1320, 160, 34, 140);

        layoutRow (tone, 408, 640, 371, 76);
        shimmerSw.setBounds (676, 371, 46, 125);
        layoutRow (shim, 722, 950, 371, 76);
        gateSw.setBounds (990, 371, 46, 125);
        layoutRow (gate, 1040, 1372, 371, 76);
    }

    // Follow the model (automation, presets, undo); dim the Shimmer and Gate knobs while their
    // section is switched off (they stay adjustable).
    void timerCallback() override
    {
        for (auto* k : knobs) k->sync();
        bypass.sync(); shimmerSw.sync(); gateSw.sync();
        types.sync(); display.sync(); meter.sync();

        const bool s = model.get (Param::shimmer) > 0.5f, g = model.get (Param::gate) > 0.5f;
        for (auto* k : shim) k->setAlpha (s ? 1.0f : 0.4f);
        for (auto* k : gate) k->setAlpha (g ? 1.0f : 0.4f);
    }

    Model& model;
    NebulaLookAndFeel lnf;
    Backdrop backdrop;
    DecayDisplay display;
    TypeSelector types;
    LedMeter meter;
    LeverSwitch bypass, shimmerSw, gateSw;
    juce::OwnedArray<KnobUnit> knobs;
    std::vector<KnobUnit*> space, out, tone, shim, gate;
};

Faceplate::Faceplate (Model& model) : impl (std::make_unique<Impl> (model))
{
    addAndMakeVisible (*impl);
    setSize (designW, designH);
}

Faceplate::~Faceplate() = default;

void Faceplate::resized() { impl->setBounds (getLocalBounds()); }
} // namespace nebula_ui
