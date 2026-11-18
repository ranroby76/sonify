// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakWidgets.h
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <vector>
#include "ZodiakSliders.h"
#include "ZodiakParams.h"

//==============================================================================
//  ZodiakWidgets.h  —  Zodiak's own controls:
//
//      ChannelColours each channel's colours (Rob's mockup): light fill, edge,
//                     dark (lit, label), dark text, light text
//      ZodiakKnob     a round knob (the macros, the effects): drag up/down,
//                     wheel, double-click back to its rest, right-click for its
//                     menu; in a channel's colours on the banner, with detents
//                     for a stepped one (Octave: 7)
//      Fader          the mixer's fader (dB), Sonify-style
//      LevelMeter     a mixer meter (one side)
//      Binding        a Zax GoldSlider tied to one setting (an atomic), shown in
//                     the setting's own units
//      VowelGrid      the vowel filter's 8 columns of A E O U I, and its marker
//      GateSteps      the trance gate's 16 steps, and the one playing
//
//  Message thread only.
//==============================================================================

namespace zodiak::ui
{
    namespace col
    {
        inline const juce::Colour bg       { 0xFF161616 };
        inline const juce::Colour panel    { 0xFF1E1E1E };
        inline const juce::Colour row      { 0xFF202020 };
        inline const juce::Colour rowAlt   { 0xFF1B1B1B };
        inline const juce::Colour line     { 0xFF2C2C2C };
        inline const juce::Colour text     { 0xFFDADADA };
        inline const juce::Colour dim      { 0xFF8A8A8A };
        inline const juce::Colour accent   { Pal::kAccent };
        inline const juce::Colour accentLt { Pal::kAccentLight };
        inline const juce::Colour learn    { 0xFF3CB8E0 };
        inline const juce::Colour on       { 0xFF3CB85A };
    }

    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h).withStyle (bold ? "Bold" : "Regular"));
    }

    //==========================================================================
    //  Each channel's colours
    //==========================================================================
    struct ChannelColours
    {
        juce::Colour light, edge, dark, darkText, lightText;
    };

    inline const ChannelColours& channelColours (int ch)
    {
        static const ChannelColours c[kNumChannels] = {
            { juce::Colour (0xFFCECBF6), juce::Colour (0xFF7F77DD), juce::Colour (0xFF534AB7), juce::Colour (0xFF3C3489), juce::Colour (0xFFEEEDFE) },  // violet
            { juce::Colour (0xFF9FE1CB), juce::Colour (0xFF1D9E75), juce::Colour (0xFF0F6E56), juce::Colour (0xFF085041), juce::Colour (0xFFE1F5EE) },  // teal
            { juce::Colour (0xFFF5C4B3), juce::Colour (0xFFD85A30), juce::Colour (0xFF993C1D), juce::Colour (0xFF712B13), juce::Colour (0xFFFAECE7) },  // rust
            { juce::Colour (0xFFF4C0D1), juce::Colour (0xFFD4537E), juce::Colour (0xFF993556), juce::Colour (0xFF72243E), juce::Colour (0xFFFBEAF0) },  // pink
            { juce::Colour (0xFFB5D4F4), juce::Colour (0xFF378ADD), juce::Colour (0xFF185FA5), juce::Colour (0xFF0C447C), juce::Colour (0xFFE6F1FB) },  // blue
            { juce::Colour (0xFFC0DD97), juce::Colour (0xFF639922), juce::Colour (0xFF3B6D11), juce::Colour (0xFF27500A), juce::Colour (0xFFEAF3DE) },  // green
            { juce::Colour (0xFFFAC775), juce::Colour (0xFFBA7517), juce::Colour (0xFF854F0B), juce::Colour (0xFF633806), juce::Colour (0xFFFAEEDA) },  // amber
            { juce::Colour (0xFFF7C1C1), juce::Colour (0xFFE24B4A), juce::Colour (0xFFA32D2D), juce::Colour (0xFF791F1F), juce::Colour (0xFFFCEBEB) },  // red
        };
        return c[juce::jlimit (0, kNumChannels - 1, ch)];
    }

    inline const ChannelColours& masterColours()
    {
        static const ChannelColours c { juce::Colour (0xFFD3D1C7), juce::Colour (0xFF888780), juce::Colour (0xFF5F5E5A),
                                        juce::Colour (0xFF2C2C2A), juce::Colour (0xFFF1EFE8) };
        return c;
    }

    // Zodiak's window: graphite, as I3Q's window and Sonify's mixer
    namespace win
    {
        inline const juce::Colour bg       { 0xFF1C1F23 };
        inline const juce::Colour row      { 0xFF24282D };
        inline const juce::Colour strip    { 0xFF26282C };
        inline const juce::Colour stripLine{ 0xFF33363C };
        inline const juce::Colour dept     { 0xFF2B2D33 };
        inline const juce::Colour item     { 0xFF363940 };
        inline const juce::Colour itemLine { 0xFF454B53 };
        inline const juce::Colour well     { 0xFF171A1D };
        inline const juce::Colour text     { 0xFFE8EAED };
        inline const juce::Colour label    { 0xFFC9CDD3 };
        inline const juce::Colour dim      { 0xFF8C939B };
        inline const juce::Colour plus     { 0xFF8AB4F8 };
        inline const juce::Colour arrow    { 0xFFE8A13A };
        inline const juce::Colour on       { 0xFF3FB950 };
        inline const juce::Colour off      { 0xFF4A4E55 };
        inline const juce::Colour led      { 0xFFFF5A4E };
    }

    //==========================================================================
    //  A value in its unit, short
    //==========================================================================
    inline juce::String valueText (float v, const juce::String& unit)
    {
        if (unit == "Hz")  return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + "k" : juce::String ((int) std::lround (v));
        if (unit == "ms")  return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + "s" : juce::String ((int) std::lround (v)) + "ms";
        if (unit == "%")   return juce::String ((int) std::lround (v)) + "%";
        if (unit == "st")  return (v > 0.0f ? "+" : "") + juce::String (v, std::abs (v - std::round (v)) < 0.05f ? 0 : 1);
        if (unit == "ct")  return (v > 0.0f ? "+" : "") + juce::String ((int) std::lround (v));
        if (unit == "dB")  return v <= -59.5f ? juce::String ("-inf") : juce::String (v, 1);
        return juce::String (v, 2);
    }

    //==========================================================================
    //  ZodiakKnob
    //==========================================================================
    class ZodiakKnob final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<float()>                     getNorm;     // 0..1
        std::function<void (float)>                setNorm;
        std::function<juce::String (float)>        textFor;     // a 0..1 as text
        std::function<void()>                      onMenu;      // right-click
        std::function<void()>                      onDragEnd;

        juce::String name, badge;       // badge: the MIDI controller it follows ("CC 74")
        float        rest    = 0.5f;    // where double-click sends it
        bool         bipolar = true;
        bool         learning = false;
        juce::Colour arcColour = col::accent;

        // In a channel's colours (the banner): its light fill and edge, its dark
        // pointer; and the steps a stepped destination has (0: smooth)
        const ChannelColours* colours = nullptr;
        int          steps = 0;
        float        nameHeight = 16.0f;
        float        nameFont   = 0.0f;     // its name's size (0: to fit nameHeight, at most 13)

        void refresh()
        {
            if (getNorm == nullptr) return;
            const float v = getNorm();
            if (std::abs (v - shown) > 1.0e-4f) { shown = v; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            if (colours != nullptr) { paintChannel (g); return; }

            auto b = getLocalBounds().toFloat();
            const float nameH = std::min (16.0f, b.getHeight() * 0.22f);
            auto nameArea = b.removeFromBottom (nameH);
            const float d = std::min (b.getWidth(), b.getHeight()) - 6.0f;
            auto circle = juce::Rectangle<float> (d, d).withCentre (b.getCentre());
            const float r = d * 0.5f;
            const auto c = circle.getCentre();
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
            const float av = a0 + (a1 - a0) * shown;

            // Track
            juce::Path track;
            track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, a0, a1, true);
            g.setColour (juce::Colour (0xFF2E2E2E));
            g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Value arc: from the middle (bipolar) or from the start
            juce::Path arc;
            const float from = bipolar ? (a0 + a1) * 0.5f : a0;
            arc.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, std::min (from, av), std::max (from, av), true);
            g.setColour (learning ? col::learn : arcColour);
            g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Body
            const float br = r - 6.0f;
            juce::ColourGradient body (juce::Colour (0xFF3A3A3A), c.x, c.y - br, juce::Colour (0xFF151515), c.x, c.y + br, false);
            g.setGradientFill (body);
            g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
            g.setColour (juce::Colour (0xFF0A0A0A));
            g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

            // Pointer
            const juce::Point<float> p1 (c.x + std::sin (av) * br * 0.35f, c.y - std::cos (av) * br * 0.35f);
            const juce::Point<float> p2 (c.x + std::sin (av) * br * 0.9f,  c.y - std::cos (av) * br * 0.9f);
            g.setColour (col::accentLt);
            g.drawLine ({ p1, p2 }, 2.0f);

            // Value (while hovered or dragged), else the badge
            g.setFont (font (std::max (9.0f, br * 0.42f), true));
            if ((isMouseOverOrDragging() || learning) && textFor != nullptr)
            {
                g.setColour (col::text);
                g.drawText (learning ? juce::String ("LEARN") : textFor (shown), circle.reduced (r * 0.3f), juce::Justification::centred, false);
            }

            g.setColour (learning ? col::learn : col::dim);
            g.setFont (font (std::min (11.5f, nameH - 2.0f), true));
            g.drawFittedText (name.toUpperCase(), nameArea.toNearestInt(), juce::Justification::centred, 1, 0.8f);

            if (badge.isNotEmpty())
            {
                g.setFont (font (8.5f, true));
                g.setColour (col::learn.withAlpha (0.9f));
                g.drawText (badge, getLocalBounds().removeFromTop (11).toFloat(), juce::Justification::centredRight, false);
            }
        }

        void mouseEnter (const juce::MouseEvent&) override   { repaint(); }
        void mouseExit (const juce::MouseEvent&) override    { repaint(); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (onMenu) onMenu();
                return;
            }
            dragStart = shown;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) return;
            const float speed = e.mods.isShiftDown() ? 0.0015f : 0.006f;
            set (dragStart - (float) e.getDistanceFromDragStartY() * speed);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu() && onDragEnd) onDragEnd();
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu()) set (rest);
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            set (shown + w.deltaY * 0.05f);
        }

        void set (float v)
        {
            v = juce::jlimit (0.0f, 1.0f, v);
            if (steps > 0) v = std::round (v * (float) steps) / (float) steps;
            shown = v;
            if (setNorm) setNorm (v);
            repaint();
        }

    private:
        // The banner's knob: light body, coloured edge, dark pointer, its name under it
        void paintChannel (juce::Graphics& g)
        {
            auto b = getLocalBounds().toFloat();
            auto nameArea = b.removeFromBottom (nameHeight);
            const float d = std::min (b.getWidth(), b.getHeight()) - 16.0f;
            const auto c = b.getCentre();
            const float r = d * 0.5f;
            const float a0 = -juce::MathConstants<float>::pi * 0.75f, a1 = juce::MathConstants<float>::pi * 0.75f;
            const float av = a0 + (a1 - a0) * shown;

            // Detents: a stepped destination's places around the knob
            if (steps > 0)
                for (int i = 0; i <= steps; ++i)
                {
                    const float a = a0 + (a1 - a0) * (float) i / (float) steps;
                    const float x = c.x + std::sin (a) * (r + 6.0f), y = c.y - std::cos (a) * (r + 6.0f);
                    g.setColour (std::abs ((float) i / (float) steps - shown) < 0.01f ? colours->edge : win::dim);
                    g.fillEllipse (x - 2.0f, y - 2.0f, 4.0f, 4.0f);
                }

            g.setColour (learning ? col::learn : colours->light);
            g.fillEllipse (c.x - r, c.y - r, d, d);
            g.setColour (colours->edge);
            g.drawEllipse (c.x - r, c.y - r, d, d, 2.2f);

            const juce::Point<float> p2 (c.x + std::sin (av) * (r - 3.0f), c.y - std::cos (av) * (r - 3.0f));
            g.setColour (colours->dark);
            g.drawLine ({ c, p2 }, 3.2f);

            if ((isMouseOverOrDragging() || learning) && textFor != nullptr)
            {
                g.setColour (win::text);
                g.setFont (font (11.0f, true));
                g.drawText (learning ? juce::String ("LEARN") : textFor (shown),
                            juce::Rectangle<float> (b.getX() - 10.0f, b.getY() - 2.0f, b.getWidth() + 20.0f, 13.0f),
                            juce::Justification::centred, false);
            }

            g.setColour (learning ? col::learn : win::label);
            g.setFont (font (nameFont > 0.0f ? nameFont : std::min (13.0f, nameHeight - 2.0f), false));
            g.drawFittedText (name, nameArea.toNearestInt(), juce::Justification::centred, 1, 0.8f);

            if (badge.isNotEmpty())
            {
                g.setFont (font (9.0f, true));
                g.setColour (col::learn.withAlpha (0.9f));
                g.drawText (badge, getLocalBounds().removeFromTop (11).toFloat(), juce::Justification::centredRight, false);
            }
        }

        float shown = 0.0f, dragStart = 0.0f;
    };

    //==========================================================================
    //  Fader: Sonify's mixer fader, in dB (-60 .. +6, unity at 3/4ths up)
    //==========================================================================
    class Fader final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<float()>       getDb;
        std::function<void (float)>  setDb;

        static float dbToNorm (float db) noexcept   { return toNorm (db, -60.0f, 6.0f, 2.0f); }
        static float normToDb (float n) noexcept    { return fromNorm (n, -60.0f, 6.0f, 2.0f); }

        void refresh()
        {
            const float n = getDb ? dbToNorm (getDb()) : 0.0f;
            if (std::abs (n - shown) > 1.0e-4f) { shown = n; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            const float cx = b.getCentreX();
            const auto travel = b.reduced (0.0f, kCapH * 0.5f);
            g.setColour (juce::Colour (0xFF111315));
            g.fillRoundedRectangle (cx - 3.0f, travel.getY(), 6.0f, travel.getHeight(), 3.0f);

            const float y = travel.getBottom() - shown * travel.getHeight();
            auto cap = juce::Rectangle<float> (b.getWidth() - 4.0f, kCapH).withCentre ({ cx, y });
            juce::ColourGradient grad (juce::Colour (0xFFEDEEF0), cap.getX(), cap.getY(), juce::Colour (0xFFB6B9BE), cap.getX(), cap.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (cap, 4.0f);
            g.setColour (juce::Colour (0xFF6E737A));
            g.drawRoundedRectangle (cap, 4.0f, 1.0f);
            g.setColour (juce::Colour (0xFF55595F));
            g.drawHorizontalLine ((int) y, cap.getX() + 5.0f, cap.getRight() - 5.0f);
        }

        void mouseDown (const juce::MouseEvent&) override   { dragStart = shown; }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float h = juce::jmax (1.0f, (float) getHeight() - kCapH);
            const float speed = e.mods.isShiftDown() ? 0.25f : 1.0f;
            set (dragStart - (float) e.getDistanceFromDragStartY() / h * speed);
        }

        void mouseDoubleClick (const juce::MouseEvent&) override   { set (dbToNorm (0.0f)); }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            set (shown + w.deltaY * 0.04f);
        }

    private:
        void set (float n)
        {
            shown = juce::jlimit (0.0f, 1.0f, n);
            if (setDb) setDb (normToDb (shown));
            repaint();
        }

        static constexpr float kCapH = 26.0f;
        float shown = 0.0f, dragStart = 0.0f;
    };

    //==========================================================================
    //  LevelMeter: one side of a strip's meter, falling slowly
    //==========================================================================
    class LevelMeter final : public juce::Component
    {
    public:
        void push (float peak)
        {
            const float v = std::max (peak, level * 0.86f);
            if (std::abs (v - level) > 1.0e-4f) { level = v; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xFF111315));
            g.fillRoundedRectangle (b, 2.0f);
            const float db = juce::Decibels::gainToDecibels (level, -72.0f);
            const float n = juce::jlimit (0.0f, 1.0f, Fader::dbToNorm (juce::jmax (-60.0f, db)) * (db > -60.0f ? 1.0f : 0.0f));
            auto lit = b.removeFromBottom (b.getHeight() * n);
            g.setColour (db > 0.0f ? juce::Colour (0xFFE24B4A) : db > -6.0f ? juce::Colour (0xFFE0C040) : win::on);
            g.fillRoundedRectangle (lit, 2.0f);
        }

    private:
        float level = 0.0f;
    };

    //==========================================================================
    //  Binding: a GoldSlider on one setting
    //==========================================================================
    struct Mapping
    {
        float min = 0.0f, max = 1.0f, def = 0.0f, skew = 1.0f;
        bool  stepped = false;
        std::function<juce::String (float)> text;     // a real value as text
    };

    class Binding
    {
    public:
        // GoldSlider prints a bare integer for an integer-step slider with no
        // unit, before it asks displayFn: a unit of one space makes it ask.
        static constexpr const char* kTextUnit = " ";

        Binding (juce::Component& parent, const juce::String& label, std::atomic<float>& target, Mapping m,
                 std::function<void()> edited)
            : value (target), map (std::move (m)), onEdited (std::move (edited))
        {
            if (map.stepped)
            {
                slider = std::make_unique<zdx::GoldSlider> (label, map.min, map.max, map.def, kTextUnit);
                slider->setStep (1.0f);
                slider->displayFn = [this] (float v) { return map.text ? map.text (v) : juce::String ((int) std::lround (v)); };
            }
            else
            {
                slider = std::make_unique<zdx::GoldSlider> (label, 0.0f, 1000.0f, 1000.0f * toNorm (map.def, map.min, map.max, map.skew), kTextUnit);
                slider->setStep (1.0f);
                slider->displayFn = [this] (float u)
                {
                    const float v = fromNorm (u / 1000.0f, map.min, map.max, map.skew);
                    return map.text ? map.text (v) : juce::String (v, 2);
                };
            }
            slider->onChange = [this] (float u)
            {
                const float v = map.stepped ? u : fromNorm (u / 1000.0f, map.min, map.max, map.skew);
                value.store (v);
                lastShown = v;
                if (onEdited) onEdited();
            };
            parent.addAndMakeVisible (*slider);
            refresh();
        }

        void refresh()
        {
            const float v = value.load();
            if (v == lastShown) return;
            lastShown = v;
            slider->setValue (map.stepped ? v : 1000.0f * toNorm (v, map.min, map.max, map.skew), false);
        }

        zdx::GoldSlider& gold() noexcept   { return *slider; }

    private:
        std::atomic<float>&              value;
        Mapping                          map;
        std::function<void()>            onEdited;
        std::unique_ptr<zdx::GoldSlider> slider;
        float                            lastShown = std::numeric_limits<float>::quiet_NaN();
    };

    //==========================================================================
    //  VowelGrid
    //==========================================================================
    class VowelGrid final : public juce::Component
    {
    public:
        std::function<int (int column)>        getVowel;
        std::function<void (int column, int v)> setVowel;
        std::function<float()>                 getPosition;     // 0..6
        std::function<float()>                 getMix;          // dims the marker when the filter is off

        void refresh()
        {
            const float p = getPosition ? getPosition() : 0.0f;
            if (std::abs (p - pos) > 0.002f) { pos = p; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (col::panel);
            const float cw = (float) getWidth() / kVowelColumns, chH = (float) getHeight() / kNumVowels;
            for (int c = 0; c < kVowelColumns; ++c)
            {
                const int sel = getVowel ? getVowel (c) : 0;
                for (int v = 0; v < kNumVowels; ++v)
                {
                    auto r = juce::Rectangle<float> (c * cw, v * chH, cw, chH).reduced (4.0f, 3.0f);
                    const bool on = v == sel;
                    g.setColour (on ? col::accent : juce::Colour (0xFF2A2A2A));
                    g.fillRoundedRectangle (r, 4.0f);
                    g.setColour (on ? juce::Colours::black : col::dim);
                    g.setFont (font (std::min (24.0f, chH * 0.5f), true));
                    g.drawText (vowelName (v), r, juce::Justification::centred, false);
                }
            }

            // The marker, at the morph's place between the columns' centres
            const bool live = getMix == nullptr || getMix() > 0.001f;
            const float x = (pos + 0.5f) * cw;
            const float wrapX = x >= (float) getWidth() ? x - (float) getWidth() : x;
            g.setColour ((live ? col::learn : col::dim).withAlpha (0.85f));
            g.fillRect (wrapX - 1.5f, 0.0f, 3.0f, (float) getHeight());
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int c = juce::jlimit (0, kVowelColumns - 1, e.x * kVowelColumns / juce::jmax (1, getWidth()));
            const int v = juce::jlimit (0, kNumVowels - 1, e.y * kNumVowels / juce::jmax (1, getHeight()));
            if (setVowel) setVowel (c, v);
            repaint();
        }

    private:
        float pos = 0.0f;
    };

    //==========================================================================
    //  GateSteps
    //==========================================================================
    class GateSteps final : public juce::Component
    {
    public:
        std::function<int()>       getSteps;
        std::function<void (int)>  setSteps;
        std::function<int()>       getCurrent;

        void refresh()
        {
            const int c = getCurrent ? getCurrent() : -1;
            const int s = getSteps ? getSteps() : 0;
            if (c != current || s != steps) { current = c; steps = s; repaint(); }
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (col::panel);
            const float w = (float) getWidth() / kGateSteps;
            for (int i = 0; i < kGateSteps; ++i)
            {
                auto r = juce::Rectangle<float> (i * w, 0.0f, w, (float) getHeight()).reduced (3.0f, 6.0f);
                const bool on = ((steps >> i) & 1) != 0;
                g.setColour (on ? col::accent : juce::Colour (0xFF2A2A2A));
                g.fillRoundedRectangle (r, 3.0f);
                if (i == current)
                {
                    g.setColour (col::learn);
                    g.drawRoundedRectangle (r, 3.0f, 2.0f);
                }
                if (i % 4 == 0)
                {
                    g.setColour (col::dim);
                    g.setFont (font (10.0f, true));
                    g.drawText (juce::String (i / 4 + 1), r.removeFromBottom (14.0f), juce::Justification::centred, false);
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = juce::jlimit (0, kGateSteps - 1, e.x * kGateSteps / juce::jmax (1, getWidth()));
            paintOn = ((steps >> i) & 1) == 0;
            toggle (i);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const int i = juce::jlimit (0, kGateSteps - 1, e.x * kGateSteps / juce::jmax (1, getWidth()));
            if ((((steps >> i) & 1) != 0) != paintOn) toggle (i);
        }

    private:
        void toggle (int i)
        {
            steps ^= (1 << i);
            if (setSteps) setSteps (steps);
            repaint();
        }

        int  steps = 0, current = -1;
        bool paintOn = true;
    };
}
