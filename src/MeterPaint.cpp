// D:\workspace\Fanan_web_hub\src\MeterPaint.cpp

#include "MeterPaint.h"
#include "FontLibrary.h"

#include <cmath>

namespace
{
    using Style = MeterPaint::Style;

    // The zones: a signal's middle from -12 dB, its top from -3 dB (on -60..+6 dB); a
    // display's from 75 % and 90 %.
    constexpr float midSignal = 48.0f / 66.0f, highSignal = 57.0f / 66.0f, zeroSignal = 60.0f / 66.0f;
    constexpr float midDisplay = 0.75f, highDisplay = 0.9f;

    float midStart (bool display)     { return display ? midDisplay : midSignal; }
    float highStart (bool display)    { return display ? highDisplay : highSignal; }

    /** The colour of the zone a point of the meter lies in (LEDs). */
    juce::Colour zoneColour (const Style& s, float t, bool display)
    {
        return t >= highStart (display) ? s.high : (t >= midStart (display) ? s.mid : s.low);
    }

    /** The colour at a point of the meter, the zones running into each other (bars, rings). */
    juce::Colour blendColour (const Style& s, float t, bool display)
    {
        const auto m = midStart (display), h = highStart (display);

        if (t < m - 0.06f) return s.low;
        if (t < m + 0.02f) return s.low.interpolatedWith (s.mid, (t - (m - 0.06f)) / 0.08f);
        if (t < h - 0.03f) return s.mid;
        if (t < h + 0.01f) return s.mid.interpolatedWith (s.high, (t - (h - 0.03f)) / 0.04f);
        return s.high;
    }

    /** The zones' colours along the meter, low end to top. */
    juce::ColourGradient lengthGradient (const Style& s, juce::Rectangle<float> r, bool vertical, bool display)
    {
        juce::ColourGradient gradient (s.low, vertical ? r.getCentreX() : r.getX(), vertical ? r.getBottom() : r.getCentreY(),
                                       s.high, vertical ? r.getCentreX() : r.getRight(), vertical ? r.getY() : r.getCentreY(), false);
        gradient.addColour (midStart (display) - 0.06, s.low);
        gradient.addColour (midStart (display) + 0.02, s.mid);
        gradient.addColour (highStart (display) - 0.03, s.mid);
        gradient.addColour (highStart (display) + 0.01, s.high);
        return gradient;
    }

    /** A stretch of the meter's length, t0 to t1 (0: its low end). */
    juce::Rectangle<float> along (juce::Rectangle<float> r, bool vertical, float t0, float t1)
    {
        if (vertical)
            return { r.getX(), r.getBottom() - r.getHeight() * t1, r.getWidth(), r.getHeight() * (t1 - t0) };

        return { r.getX() + r.getWidth() * t0, r.getY(), r.getWidth() * (t1 - t0), r.getHeight() };
    }

    juce::Colour inkOn (juce::Colour face)
    {
        return face.withAlpha (1.0f).getPerceivedBrightness() > 0.55f ? juce::Colour (0xff1c1a17) : juce::Colour (0xffe8e4d8);
    }

    /** How many LEDs: as asked, or as many as fit. */
    int ledCount (const Style& s, float length, float cross, bool round)
    {
        if (s.segments > 0)
            return juce::jlimit (3, 64, s.segments);

        const auto pitch = round ? juce::jlimit (6.0f, 16.0f, cross) : 6.0f;
        return juce::jlimit (4, 48, (int) (length / pitch));
    }

    /** One LED of N: where it lies along the meter, its middle's level. */
    juce::Rectangle<float> ledArea (juce::Rectangle<float> inner, bool vertical, int i, int count, float gap)
    {
        const auto length = vertical ? inner.getHeight() : inner.getWidth();
        const auto g = juce::jmin (gap, length / (float) count * 0.4f) / length;
        return along (inner, vertical, (float) i / (float) count + g * 0.5f, (float) (i + 1) / (float) count - g * 0.5f);
    }

    /** The dB marks (a display's: quarters), with their numbers where there's room. */
    void drawScale (juce::Graphics& g, juce::Rectangle<float> r, bool vertical, const Style& s, bool display)
    {
        const float signalMarks[] { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };
        const float displayMarks[] { 1.0f, 0.75f, 0.5f, 0.25f, 0.0f };
        const auto cross = vertical ? r.getWidth() : r.getHeight();
        const bool numbers = cross >= 26.0f;
        g.setFont (FontLibrary::font (s.font, juce::jlimit (8.0f, 11.0f, cross * 0.3f)));

        for (int i = 0; i < 5; ++i)
        {
            const auto t = display ? displayMarks[i] : (signalMarks[i] + 60.0f) / 66.0f;
            const auto text = display ? juce::String (juce::roundToInt (displayMarks[i] * 100.0f)) : juce::String (juce::roundToInt (signalMarks[i]));

            g.setColour (juce::Colours::white.withAlpha (0.45f));

            if (vertical)
            {
                const auto y = r.getBottom() - r.getHeight() * t;
                g.fillRect (r.getRight() - cross * 0.3f, y - 0.5f, cross * 0.3f, 1.0f);

                if (numbers)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.7f));
                    g.drawText (text, juce::Rectangle<float> (r.getX() + 2.0f, y - 6.0f, cross * 0.66f, 12.0f), juce::Justification::centredLeft, false);
                }
            }
            else
            {
                const auto x = r.getX() + r.getWidth() * t;
                g.fillRect (x - 0.5f, r.getBottom() - cross * 0.3f, 1.0f, cross * 0.3f);

                if (numbers)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.7f));
                    g.drawText (text, juce::Rectangle<float> (x - 14.0f, r.getY() + 1.0f, 28.0f, cross * 0.6f), juce::Justification::centred, false);
                }
            }
        }
    }

    //==========================================================================
    // The VU meter: a face with its scale, the needle swinging from below it.
    struct Vu
    {
        juce::Rectangle<float> face;
        juce::Point<float> pivot;
        float radius = 0.0f;
        static constexpr float swing = 48.0f;   // degrees each side of upright

        explicit Vu (juce::Rectangle<float> area) : face (area)
        {
            const auto w = area.getWidth(), h = area.getHeight();
            // Room above the scale for its numbers (they lie at 1.2 times its radius).
            pivot = { area.getCentreX(), area.getBottom() - h * 0.1f };
            radius = juce::jmax (4.0f, juce::jmin (w * 0.5f, (pivot.y - area.getY() - h * 0.06f) / 1.32f));
        }

        /** A VU reading (-20..+3) along the scale, 0..1: as a real one, by voltage. */
        static float positionOf (float vu)
        {
            const auto low = std::pow (10.0f, -20.0f / 20.0f), top = std::pow (10.0f, 3.0f / 20.0f);
            return (std::pow (10.0f, vu / 20.0f) - low) / (top - low);
        }

        juce::Point<float> at (float position, float distance) const
        {
            const auto angle = juce::degreesToRadians (-swing + 2.0f * swing * position);
            return { pivot.x + std::sin (angle) * distance, pivot.y - std::cos (angle) * distance };
        }

        juce::Colour faceColour (const Style& s) const    { return s.backgroundChosen ? s.background.withAlpha (1.0f) : juce::Colour (0xfff1e4c3); }
        juce::Rectangle<float> lamp() const
        {
            const auto size = juce::jlimit (5.0f, 12.0f, juce::jmin (face.getWidth(), face.getHeight()) * 0.07f);
            return { face.getRight() - size * 2.0f, face.getY() + size, size, size };
        }
    };

    void paintVuFace (juce::Graphics& g, juce::Rectangle<float> area, const Style& s)
    {
        const Vu vu (area);
        const auto face = vu.faceColour (s);
        const auto ink = inkOn (face);

        // The face, a little darker at its edges, in a dark bezel.
        g.setColour (juce::Colour (0xff141416));
        g.fillRoundedRectangle (area, 5.0f);
        const auto inner = area.reduced (juce::jmax (2.0f, area.getWidth() * 0.025f));
        g.setGradientFill (juce::ColourGradient (face, inner.getCentreX(), inner.getCentreY() + inner.getHeight() * 0.2f,
                                                 face.darker (0.35f), inner.getX(), inner.getY(), true));
        g.fillRoundedRectangle (inner, 3.5f);

        // The scale: its line, the top zone in red, its ticks and numbers.
        juce::Path arc;
        const auto radius = vu.radius;
        arc.addCentredArc (vu.pivot.x, vu.pivot.y, radius, radius, 0.0f, juce::degreesToRadians (-Vu::swing), juce::degreesToRadians (Vu::swing), true);
        g.setColour (ink.withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.0f, radius * 0.012f)));

        juce::Path red;
        const auto zero = Vu::positionOf (0.0f);
        red.addCentredArc (vu.pivot.x, vu.pivot.y, radius * 1.03f, radius * 1.03f, 0.0f,
                           juce::degreesToRadians (-Vu::swing + 2.0f * Vu::swing * zero), juce::degreesToRadians (Vu::swing), true);
        g.setColour (s.high);
        g.strokePath (red, juce::PathStrokeType (juce::jmax (2.0f, radius * 0.05f)));

        const float ticks[] { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };
        const auto fontSize = juce::jlimit (6.5f, 13.0f, radius * 0.12f);
        g.setFont (FontLibrary::font (s.font, fontSize));

        for (const auto v : ticks)
        {
            const auto p = Vu::positionOf (v);
            const bool major = v == -20.0f || v == -10.0f || v == -7.0f || v == -5.0f || v == -3.0f || v == 0.0f || v == 3.0f;
            g.setColour (v > 0.0f ? s.high : ink.withAlpha (0.9f));
            g.drawLine ({ vu.at (p, radius), vu.at (p, radius * (major ? 1.1f : 1.06f)) }, major ? 1.4f : 1.0f);

            if (major && radius > 30.0f)
            {
                const auto c = vu.at (p, radius * 1.2f);
                g.drawText (v > 0.0f ? "+" + juce::String ((int) v) : juce::String ((int) v),
                            juce::Rectangle<float> (fontSize * 2.6f, fontSize * 1.2f).withCentre (c), juce::Justification::centred, false);
            }
        }

        // VU, under the scale; the peak lamp (unlit).
        g.setColour (ink.withAlpha (0.85f));
        g.setFont (FontLibrary::font (s.font, juce::jlimit (8.0f, 22.0f, radius * 0.2f), true));
        g.drawText ("VU", juce::Rectangle<float> (radius, radius * 0.3f).withCentre ({ vu.pivot.x, vu.pivot.y - radius * 0.42f }),
                    juce::Justification::centred, false);

        const auto lamp = vu.lamp();
        g.setColour (s.high.darker (1.6f));
        g.fillEllipse (lamp);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (lamp, 0.8f);
    }

    void paintVuNeedle (juce::Graphics& g, juce::Rectangle<float> area, const Style& s, float fraction, float peak, bool display)
    {
        const Vu vu (area);
        const auto ink = inkOn (vu.faceColour (s));

        // The level as a VU reading (0 VU at -14 dBFS), a hair past the scale's ends at most.
        float position = fraction;

        if (! display)
        {
            const auto db = fraction * 66.0f - 60.0f;
            position = Vu::positionOf (juce::jlimit (-20.0f, 3.4f, db + 14.0f));
        }

        position = juce::jlimit (-0.02f, 1.03f, position);

        // Its shadow, the needle, the pivot's cap.
        const auto tip = vu.at (position, vu.radius * 1.06f);
        const auto base = vu.at (position, vu.radius * 0.12f);
        const auto width = juce::jmax (1.0f, vu.radius * 0.016f);

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (area.reduced (2.0f).toNearestInt());
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine ({ base.translated (2.0f, 2.0f), tip.translated (2.0f, 2.0f) }, width * 1.4f);
        g.setColour (ink);
        g.drawLine ({ base, tip }, width);
        g.drawLine ({ vu.pivot, base }, width * 2.2f);
        g.fillEllipse (juce::Rectangle<float> (vu.radius * 0.12f, vu.radius * 0.12f).withCentre (vu.pivot));

        // The peak lamp: lit at the top zone.
        if (juce::jmax (fraction, peak) >= highStart (display))
        {
            const auto lamp = vu.lamp();
            g.setColour (s.high.withAlpha (0.4f));
            g.fillEllipse (lamp.expanded (lamp.getWidth() * 0.6f));
            g.setColour (s.high.brighter (0.3f));
            g.fillEllipse (lamp);
        }

        // The glass over it.
        const auto inner = area.reduced (juce::jmax (2.0f, area.getWidth() * 0.025f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), inner.getX(), inner.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), inner.getX(), inner.getCentreY(), false));
        g.fillRoundedRectangle (inner.withHeight (inner.getHeight() * 0.5f), 3.5f);
    }

    //==========================================================================
    // The needle meters' common parts: a VU reading along their scale, their face's colour.

    /** Where the level lies on a VU scale, 0..1 (a display's level as it is). */
    float vuPosition (float fraction, bool display)
    {
        if (display)
            return juce::jlimit (-0.02f, 1.03f, fraction);

        const auto db = fraction * 66.0f - 60.0f;
        return juce::jlimit (-0.02f, 1.03f, Vu::positionOf (juce::jlimit (-20.0f, 3.4f, db + 14.0f)));
    }

    juce::Colour faceOf (const Style& s)    { return s.backgroundChosen ? s.background.withAlpha (1.0f) : juce::Colour (0xfff1e4c3); }

    const float vuTicks[] { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };

    bool isMajorTick (float v)    { return v == -20.0f || v == -10.0f || v == -7.0f || v == -5.0f || v == -3.0f || v == 0.0f || v == 3.0f; }

    juce::String tickText (float v)    { return v > 0.0f ? "+" + juce::String ((int) v) : juce::String ((int) v); }

    /** A face in a dark bezel, a little darker at its edges; the inner face's area. */
    juce::Rectangle<float> paintFace (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour face, float corner)
    {
        g.setColour (juce::Colour (0xff141416));
        g.fillRoundedRectangle (area, corner);
        const auto inner = area.reduced (juce::jmax (2.0f, juce::jmin (area.getWidth(), area.getHeight()) * 0.05f));
        g.setGradientFill (juce::ColourGradient (face, inner.getCentreX(), inner.getCentreY() + inner.getHeight() * 0.15f,
                                                 face.darker (0.35f), inner.getX(), inner.getY(), true));
        g.fillRoundedRectangle (inner, juce::jmax (1.0f, corner - 1.5f));
        return inner;
    }

    /** The glass over a face: a sheen over its upper part. */
    void paintGlass (juce::Graphics& g, juce::Rectangle<float> inner, float corner, bool sideways)
    {
        if (sideways)
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), inner.getX(), inner.getY(),
                                                     juce::Colours::white.withAlpha (0.0f), inner.getCentreX(), inner.getY(), false));
        else
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), inner.getX(), inner.getY(),
                                                     juce::Colours::white.withAlpha (0.0f), inner.getX(), inner.getCentreY(), false));

        g.fillRoundedRectangle (sideways ? inner.withWidth (inner.getWidth() * 0.5f) : inner.withHeight (inner.getHeight() * 0.5f), corner);
    }

    /** The peak lamp: dark, or lit at the top zone. */
    void paintPeakLamp (juce::Graphics& g, juce::Rectangle<float> lamp, juce::Colour high, bool lit)
    {
        if (lit)
        {
            g.setColour (high.withAlpha (0.4f));
            g.fillEllipse (lamp.expanded (lamp.getWidth() * 0.6f));
            g.setColour (high.brighter (0.3f));
            g.fillEllipse (lamp);
            return;
        }

        g.setColour (high.darker (1.6f));
        g.fillEllipse (lamp);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (lamp, 0.8f);
    }

    //==========================================================================
    // The upright VU: a tall face, its needle swinging up and down from a pivot at its left.
    struct SideVu
    {
        juce::Rectangle<float> inner;
        juce::Point<float> pivot;
        float radius = 0.0f;
        static constexpr float swing = 48.0f;

        explicit SideVu (juce::Rectangle<float> face) : inner (face)
        {
            pivot = { face.getX() + face.getWidth() * 0.08f, face.getCentreY() };
            radius = juce::jmax (4.0f, juce::jmin (face.getWidth() * 0.66f, face.getHeight() * 0.4f / std::sin (juce::degreesToRadians (swing))));
        }

        /** 0 at the bottom of its scale, 1 at the top. */
        juce::Point<float> at (float position, float distance) const
        {
            const auto angle = juce::degreesToRadians (-swing + 2.0f * swing * position);
            return { pivot.x + std::cos (angle) * distance, pivot.y - std::sin (angle) * distance };
        }

        juce::Rectangle<float> lamp() const
        {
            const auto size = juce::jlimit (5.0f, 11.0f, inner.getWidth() * 0.12f);
            return { inner.getX() + size * 0.8f, inner.getY() + size * 0.8f, size, size };
        }
    };

    juce::Rectangle<float> sideVuFace (juce::Rectangle<float> area)
    {
        return area.reduced (juce::jmax (2.0f, juce::jmin (area.getWidth(), area.getHeight()) * 0.05f));
    }

    void paintSideVuFace (juce::Graphics& g, juce::Rectangle<float> area, const Style& s)
    {
        const auto inner = paintFace (g, area, faceOf (s), 5.0f);
        const SideVu vu (inner);
        const auto ink = inkOn (faceOf (s));

        juce::Path arc;
        const auto r = vu.radius;
        // addCentredArc measures from 12 o'clock, clockwise: the scale runs from below the
        // pivot's right to above it.
        arc.addCentredArc (vu.pivot.x, vu.pivot.y, r, r, 0.0f, juce::degreesToRadians (90.0f + SideVu::swing), juce::degreesToRadians (90.0f - SideVu::swing), true);
        g.setColour (ink.withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.0f, r * 0.012f)));

        juce::Path red;
        const auto zero = Vu::positionOf (0.0f);
        red.addCentredArc (vu.pivot.x, vu.pivot.y, r * 1.03f, r * 1.03f, 0.0f,
                           juce::degreesToRadians (90.0f + SideVu::swing - 2.0f * SideVu::swing * zero), juce::degreesToRadians (90.0f - SideVu::swing), true);
        g.setColour (s.high);
        g.strokePath (red, juce::PathStrokeType (juce::jmax (2.0f, r * 0.05f)));

        const auto fontSize = juce::jlimit (6.5f, 12.0f, r * 0.13f);
        g.setFont (FontLibrary::font (s.font, fontSize));

        for (const auto v : vuTicks)
        {
            const auto p = Vu::positionOf (v);
            const bool major = isMajorTick (v);
            g.setColour (v > 0.0f ? s.high : ink.withAlpha (0.9f));
            g.drawLine ({ vu.at (p, r), vu.at (p, r * (major ? 1.1f : 1.06f)) }, major ? 1.4f : 1.0f);

            if (major && r > 28.0f)
                g.drawText (tickText (v), juce::Rectangle<float> (fontSize * 2.6f, fontSize * 1.2f).withCentre (vu.at (p, r * 1.24f)),
                            juce::Justification::centred, false);
        }

        g.setColour (ink.withAlpha (0.85f));
        g.setFont (FontLibrary::font (s.font, juce::jlimit (8.0f, 20.0f, r * 0.2f), true));
        g.drawText ("VU", juce::Rectangle<float> (r * 0.6f, r * 0.3f).withCentre ({ vu.pivot.x + r * 0.5f, inner.getBottom() - r * 0.22f }),
                    juce::Justification::centred, false);

        paintPeakLamp (g, vu.lamp(), s.high, false);
    }

    void paintSideVuNeedle (juce::Graphics& g, juce::Rectangle<float> area, const Style& s, float fraction, float peak, bool display)
    {
        const auto inner = sideVuFace (area);
        const SideVu vu (inner);
        const auto ink = inkOn (faceOf (s));
        const auto position = vuPosition (fraction, display);
        const auto tip = vu.at (position, vu.radius * 1.06f);
        const auto base = vu.at (position, vu.radius * 0.12f);
        const auto width = juce::jmax (1.0f, vu.radius * 0.018f);

        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (inner.toNearestInt());
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.drawLine ({ base.translated (2.0f, 2.0f), tip.translated (2.0f, 2.0f) }, width * 1.4f);
            g.setColour (ink);
            g.drawLine ({ base, tip }, width);
            g.drawLine ({ vu.pivot, base }, width * 2.2f);
            g.fillEllipse (juce::Rectangle<float> (vu.radius * 0.14f, vu.radius * 0.14f).withCentre (vu.pivot));
        }

        paintPeakLamp (g, vu.lamp(), s.high, juce::jmax (fraction, peak) >= highStart (display));
        paintGlass (g, inner, 3.5f, true);
    }

    //==========================================================================
    // The edgewise meter: a scale printed on a band behind a long window, a thin pointer
    // running along it (upright when its box is tall).
    juce::AffineTransform edgeTurn (juce::Rectangle<float> area)
    {
        return area.getHeight() > area.getWidth() ? juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, area.getCentreX(), area.getCentreY())
                                                  : juce::AffineTransform();
    }

    juce::Rectangle<float> edgeArea (juce::Rectangle<float> area)
    {
        // Upright: drawn across in its box turned, then turned back.
        return area.getHeight() > area.getWidth() ? juce::Rectangle<float> (area.getHeight(), area.getWidth()).withCentre (area.getCentre()) : area;
    }

    void paintEdgeStill (juce::Graphics& g, juce::Rectangle<float> area, const Style& s)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (edgeTurn (area));
        const auto box = edgeArea (area);

        g.setColour (juce::Colour (0xff111113));
        g.fillRoundedRectangle (box, 4.0f);
        const auto band = box.reduced (juce::jmax (2.0f, box.getHeight() * 0.1f));
        const auto face = faceOf (s);

        // The band curves away at its top and bottom edges: lit across its middle.
        juce::ColourGradient drum (face.darker (0.55f), band.getX(), band.getY(), face.darker (0.55f), band.getX(), band.getBottom(), false);
        drum.addColour (0.45, face.brighter (0.06f));
        g.setGradientFill (drum);
        g.fillRoundedRectangle (band, 2.0f);

        const auto ink = inkOn (face);
        const auto scale = band.reduced (band.getWidth() * 0.06f, 0.0f);
        const auto tickTop = band.getY() + band.getHeight() * 0.18f;
        const auto zero = Vu::positionOf (0.0f);

        g.setColour (s.high);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (scale.getX() + scale.getWidth() * zero, tickTop - 1.0f, scale.getRight(), tickTop + juce::jmax (2.0f, band.getHeight() * 0.06f)));
        g.setColour (ink.withAlpha (0.85f));
        g.drawHorizontalLine (juce::roundToInt (tickTop), scale.getX(), scale.getX() + scale.getWidth() * zero);

        const auto fontSize = juce::jlimit (6.5f, 11.0f, band.getHeight() * 0.3f);
        g.setFont (FontLibrary::font (s.font, fontSize));

        for (const auto v : vuTicks)
        {
            const auto x = scale.getX() + scale.getWidth() * Vu::positionOf (v);
            const bool major = isMajorTick (v);
            g.setColour (v > 0.0f ? s.high : ink.withAlpha (0.9f));
            g.drawLine (x, tickTop, x, tickTop + band.getHeight() * (major ? 0.22f : 0.13f), major ? 1.2f : 0.9f);

            if (major && band.getHeight() > 18.0f)
                g.drawText (tickText (v), juce::Rectangle<float> (fontSize * 2.6f, fontSize * 1.2f).withCentre ({ x, tickTop + band.getHeight() * 0.42f }),
                            juce::Justification::centred, false);
        }
    }

    void paintEdgeLevel (juce::Graphics& g, juce::Rectangle<float> area, const Style& s, float fraction, float peak, bool display)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (edgeTurn (area));
        const auto box = edgeArea (area);
        const auto band = box.reduced (juce::jmax (2.0f, box.getHeight() * 0.1f));
        const auto scale = band.reduced (band.getWidth() * 0.06f, 0.0f);
        const auto x = scale.getX() + scale.getWidth() * vuPosition (fraction, display);
        const auto ink = inkOn (faceOf (s));

        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (band.toNearestInt());
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (juce::jmax (2.0f, band.getHeight() * 0.07f), band.getHeight()).withCentre ({ x + 2.0f, band.getCentreY() }));
            g.setColour (s.high.interpolatedWith (ink, 0.5f));
            g.fillRect (juce::Rectangle<float> (juce::jmax (1.8f, band.getHeight() * 0.055f), band.getHeight()).withCentre ({ x, band.getCentreY() }));
        }

        const auto lampSize = juce::jlimit (4.0f, 8.0f, band.getHeight() * 0.22f);
        paintPeakLamp (g, juce::Rectangle<float> (lampSize, lampSize).withCentre ({ band.getRight() - lampSize, band.getBottom() - lampSize }), s.high,
                       juce::jmax (fraction, peak) >= highStart (display));

        // The window's glass.
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.2f), band.getX(), band.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), band.getX(), band.getY() + band.getHeight() * 0.45f, false));
        g.fillRoundedRectangle (band.withHeight (band.getHeight() * 0.45f), 2.0f);
    }

    //==========================================================================
    // The round gauge: a face in a chrome bezel, its needle turning about its middle.
    struct Gauge
    {
        juce::Point<float> centre;
        float radius = 0.0f, scale = 0.0f;
        static constexpr float swing = 120.0f;   // degrees each side of upright

        explicit Gauge (juce::Rectangle<float> area)
        {
            centre = area.getCentre();
            radius = juce::jmax (4.0f, juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f);
            scale = radius * 0.66f;
        }

        juce::Point<float> at (float position, float distance) const
        {
            const auto angle = juce::degreesToRadians (-swing + 2.0f * swing * position);
            return { centre.x + std::sin (angle) * distance, centre.y - std::cos (angle) * distance };
        }

        juce::Rectangle<float> face() const    { return juce::Rectangle<float> (radius * 1.7f, radius * 1.7f).withCentre (centre); }
    };

    void paintGaugeFace (juce::Graphics& g, juce::Rectangle<float> area, const Style& s)
    {
        const Gauge gauge (area);
        const auto outer = juce::Rectangle<float> (gauge.radius * 2.0f, gauge.radius * 2.0f).withCentre (gauge.centre);

        juce::ColourGradient chrome (juce::Colour (0xfff2f4f7), gauge.centre.x, outer.getY(), juce::Colour (0xffd6dbe0), gauge.centre.x, outer.getBottom(), false);
        chrome.addColour (0.45, juce::Colour (0xff8e949b));
        chrome.addColour (0.58, juce::Colour (0xff5e636a));
        g.setGradientFill (chrome);
        g.fillEllipse (outer);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (outer.reduced (0.5f), 1.0f);

        const auto face = faceOf (s);
        const auto ink = inkOn (face);
        const auto disc = gauge.face();
        g.setGradientFill (juce::ColourGradient (face, gauge.centre.x, gauge.centre.y + gauge.radius * 0.2f, face.darker (0.4f), disc.getX(), disc.getY(), true));
        g.fillEllipse (disc);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (disc, 1.0f);

        const auto r = gauge.scale;
        const auto zero = Vu::positionOf (0.0f);
        juce::Path arc, red;
        arc.addCentredArc (gauge.centre.x, gauge.centre.y, r, r, 0.0f, juce::degreesToRadians (-Gauge::swing), juce::degreesToRadians (-Gauge::swing + 2.0f * Gauge::swing * zero), true);
        red.addCentredArc (gauge.centre.x, gauge.centre.y, r, r, 0.0f, juce::degreesToRadians (-Gauge::swing + 2.0f * Gauge::swing * zero), juce::degreesToRadians (Gauge::swing), true);
        g.setColour (ink.withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.0f, r * 0.02f)));
        g.setColour (s.high);
        g.strokePath (red, juce::PathStrokeType (juce::jmax (2.0f, r * 0.07f)));

        const auto fontSize = juce::jlimit (6.0f, 12.0f, r * 0.18f);
        g.setFont (FontLibrary::font (s.font, fontSize));

        for (const auto v : vuTicks)
        {
            const auto p = Vu::positionOf (v);
            const bool major = isMajorTick (v);
            g.setColour (v > 0.0f ? s.high : ink.withAlpha (0.9f));
            g.drawLine ({ gauge.at (p, r * (major ? 0.84f : 0.9f)), gauge.at (p, r) }, major ? 1.3f : 0.9f);

            if (major && r > 22.0f)
                g.drawText (tickText (v), juce::Rectangle<float> (fontSize * 2.4f, fontSize * 1.2f).withCentre (gauge.at (p, r * 0.66f)),
                            juce::Justification::centred, false);
        }

        g.setColour (ink.withAlpha (0.8f));
        g.setFont (FontLibrary::font (s.font, juce::jlimit (7.0f, 16.0f, r * 0.22f), true));
        g.drawText ("VU", juce::Rectangle<float> (r, r * 0.3f).withCentre (gauge.centre.translated (0.0f, r * 0.5f)), juce::Justification::centred, false);
    }

    void paintGaugeNeedle (juce::Graphics& g, juce::Rectangle<float> area, const Style& s, float fraction, float peak, bool display)
    {
        const Gauge gauge (area);
        const auto ink = inkOn (faceOf (s));
        const auto position = vuPosition (fraction, display);
        const auto tip = gauge.at (position, gauge.scale * 1.04f);
        const auto tail = gauge.at (position, -gauge.scale * 0.18f);
        const auto width = juce::jmax (1.0f, gauge.scale * 0.03f);

        g.setColour (juce::Colours::black.withAlpha (0.2f));
        g.drawLine ({ tail.translated (1.5f, 2.0f), tip.translated (1.5f, 2.0f) }, width * 1.3f);
        g.setColour (s.high.interpolatedWith (ink, 0.5f));
        g.drawLine ({ tail, tip }, width);

        const auto cap = juce::Rectangle<float> (gauge.scale * 0.2f, gauge.scale * 0.2f).withCentre (gauge.centre);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4b50), cap.getX(), cap.getY(), juce::Colour (0xff0d0d0f), cap.getRight(), cap.getBottom(), false));
        g.fillEllipse (cap);

        const auto lampSize = juce::jlimit (4.0f, 9.0f, gauge.radius * 0.12f);
        paintPeakLamp (g, juce::Rectangle<float> (lampSize, lampSize).withCentre (gauge.centre.translated (0.0f, gauge.scale * 0.78f)), s.high,
                       juce::jmax (fraction, peak) >= highStart (display));

        // The glass: a crescent of light at its upper left.
        juce::Path shine;
        const auto disc = gauge.face();
        shine.addCentredArc (gauge.centre.x, gauge.centre.y, disc.getWidth() * 0.44f, disc.getHeight() * 0.44f, 0.0f,
                             juce::degreesToRadians (-80.0f), juce::degreesToRadians (-10.0f), true);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.strokePath (shine, juce::PathStrokeType (juce::jmax (1.5f, disc.getWidth() * 0.04f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    //==========================================================================
    // The ring: a 270-degree arc that fills round.
    struct Ring
    {
        juce::Point<float> centre;
        float radius = 0.0f, thickness = 0.0f;
        static constexpr float start = -135.0f, end = 135.0f;

        explicit Ring (juce::Rectangle<float> area)
        {
            const auto side = juce::jmin (area.getWidth(), area.getHeight());
            centre = area.getCentre();
            thickness = juce::jmax (2.0f, side * 0.11f);
            radius = juce::jmax (2.0f, side * 0.5f - thickness * 0.8f);
        }

        float angleOf (float t) const    { return juce::degreesToRadians (start + (end - start) * t); }
    };

    void paintRingStill (juce::Graphics& g, juce::Rectangle<float> area, const Style& s, bool display)
    {
        const Ring ring (area);
        g.setColour (s.background);
        g.fillEllipse (juce::Rectangle<float> (2.0f * (ring.radius + ring.thickness * 0.9f), 2.0f * (ring.radius + ring.thickness * 0.9f))
                           .withCentre (ring.centre));

        juce::Path track;
        track.addCentredArc (ring.centre.x, ring.centre.y, ring.radius, ring.radius, 0.0f, ring.angleOf (0.0f), ring.angleOf (1.0f), true);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.strokePath (track, juce::PathStrokeType (ring.thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (s.scale)
        {
            g.setColour (juce::Colours::white.withAlpha (0.4f));

            for (const auto t : { 0.0f, display ? 0.25f : 36.0f / 66.0f, display ? 0.5f : 48.0f / 66.0f, display ? 0.75f : 54.0f / 66.0f, display ? 1.0f : zeroSignal })
            {
                const auto a = ring.angleOf (t);
                const juce::Point<float> dir (std::sin (a), -std::cos (a));
                g.drawLine ({ ring.centre + dir * (ring.radius + ring.thickness * 0.6f), ring.centre + dir * (ring.radius + ring.thickness * 0.95f) }, 1.0f);
            }
        }
    }

    void paintRingLevel (juce::Graphics& g, juce::Rectangle<float> area, const Style& s, float fraction, float peak, bool display)
    {
        const Ring ring (area);
        constexpr int steps = 72;
        const auto filled = juce::roundToInt (fraction * (float) steps);

        // Short arcs, each in the colour of its place (a little overlap hides the joins).
        for (int i = 0; i < filled; ++i)
        {
            const auto t0 = (float) i / (float) steps, t1 = (float) (i + 1) / (float) steps;
            juce::Path piece;
            piece.addCentredArc (ring.centre.x, ring.centre.y, ring.radius, ring.radius, 0.0f, ring.angleOf (t0), ring.angleOf (juce::jmin (1.0f, t1 + 0.004f)), true);
            g.setColour (blendColour (s, (t0 + t1) * 0.5f, display));
            g.strokePath (piece, juce::PathStrokeType (ring.thickness, juce::PathStrokeType::curved,
                                                       i == 0 ? juce::PathStrokeType::rounded : juce::PathStrokeType::butt));
        }

        if (peak > 0.0f)
        {
            const auto a = ring.angleOf (juce::jlimit (0.0f, 1.0f, peak));
            const juce::Point<float> dir (std::sin (a), -std::cos (a));
            g.setColour (blendColour (s, peak, display).brighter (0.4f));
            g.drawLine ({ ring.centre + dir * (ring.radius - ring.thickness * 0.55f), ring.centre + dir * (ring.radius + ring.thickness * 0.55f) }, 2.0f);
        }

        // The level in its middle: dB (a display's: %).
        const auto ink = inkOn (s.background.withAlpha (1.0f));
        juce::String text;

        if (display)
            text = juce::String (juce::roundToInt (fraction * 100.0f)) + "%";
        else
            text = fraction <= 0.001f ? juce::String::fromUTF8 ("\xe2\x88\x92\xe2\x88\x9e") : juce::String (juce::roundToInt (fraction * 66.0f - 60.0f));

        const auto size = juce::jmax (7.0f, ring.radius * 0.5f);
        g.setColour (ink.withAlpha (0.92f));
        g.setFont (FontLibrary::font (s.font, size, true));
        g.drawText (text, juce::Rectangle<float> (ring.radius * 1.6f, size * 1.2f).withCentre (ring.centre), juce::Justification::centred, false);

        if (! display && ring.radius > 18.0f)
        {
            g.setColour (ink.withAlpha (0.55f));
            g.setFont (FontLibrary::font (s.font, juce::jmax (7.0f, size * 0.45f)));
            g.drawText ("dB", juce::Rectangle<float> (ring.radius, size * 0.6f).withCentre (ring.centre.translated (0.0f, size * 0.85f)),
                        juce::Justification::centred, false);
        }
    }
}

//==============================================================================
const juce::StringArray& MeterPaint::displays()
{
    static const juce::StringArray ids { "bar", "segments", "dots", "neon", "tube", "vu", "vuside", "edge", "porthole", "arc" };
    return ids;
}

const juce::StringArray& MeterPaint::displayNames()
{
    static const juce::StringArray names { "Bar", "LED segments", "LED dots", "Neon", "Glass tube", "VU needle", "VU needle, upright",
                                           "Edgewise strip", "Round gauge", "Ring" };
    return names;
}

bool MeterPaint::hasNeedle (const Style& s)
{
    return s.display == "vu" || s.display == "vuside" || s.display == "edge" || s.display == "porthole";
}

MeterPaint::Style MeterPaint::standardStyle()
{
    return {};
}

MeterPaint::Style MeterPaint::styleOf (const GuiWidget& w)
{
    auto s = standardStyle();
    s.font = w.font;

    auto text = [&w] (const char* key)
    {
        const auto found = w.settings.find (key);
        return found != w.settings.end() && ! found->second.isVoid() ? found->second.toString().trim() : juce::String();
    };

    auto flag = [&w] (const char* key)
    {
        const auto found = w.settings.find (key);

        if (found == w.settings.end() || found->second.isVoid())
            return false;

        if (found->second.isBool() || found->second.isInt() || found->second.isInt64() || found->second.isDouble())
            return (bool) found->second;

        const auto t = found->second.toString().trim().toLowerCase();
        return t == "true" || t == "yes" || t == "on" || t == "1";
    };

    // Its display (a few other names find theirs).
    const auto display = text ("display").toLowerCase().removeCharacters (" -_");

    if (displays().contains (display))                                                       s.display = display;
    else if (display == "led" || display == "leds" || display == "ladder" || display == "segment") s.display = "segments";
    else if (display == "dot" || display == "round" || display == "roundleds")                 s.display = "dots";
    else if (display == "glow" || display == "light")                                         s.display = "neon";
    else if (display == "glass" || display == "glasstube" || display == "liquid")             s.display = "tube";
    else if (display == "needle" || display == "analog" || display == "analogue" || display == "vuneedle" || display == "vumeter") s.display = "vu";
    else if (display == "verticalvu" || display == "vuvertical" || display == "uprightvu" || display == "sidevu" || display == "tallvu"
             || display == "vuupright")                                                       s.display = "vuside";
    else if (display == "edgewise" || display == "strip" || display == "edgemeter" || display == "stripmeter" || display == "drum"
             || display == "edgewisemeter")                                                   s.display = "edge";
    else if (display == "gauge" || display == "roundgauge" || display == "roundvu" || display == "circularvu" || display == "dialgauge"
             || display == "roundmeter" || display == "portholevu")                           s.display = "porthole";
    else if (display == "ring" || display == "circle" || display == "circular" || display == "dial") s.display = "arc";

    if (const auto c = text ("colorLow"); c.isNotEmpty())   s.low = GuiLayout::colourFromString (c, s.low);
    if (const auto c = text ("colorMid"); c.isNotEmpty())   s.mid = GuiLayout::colourFromString (c, s.mid);
    if (const auto c = text ("colorHigh"); c.isNotEmpty())  s.high = GuiLayout::colourFromString (c, s.high);

    if (const auto c = text ("background"); c.isNotEmpty())
    {
        s.background = GuiLayout::colourFromString (c, s.background);
        s.backgroundChosen = true;
    }

    s.segments = juce::jlimit (0, 64, juce::roundToInt (w.number ("segments", 0.0f)));
    s.peakHold = flag ("peakHold");
    s.scale = flag ("scale");
    s.glow = juce::jlimit (0.0f, 1.0f, w.number ("glow", 0.5f));
    return s;
}

float MeterPaint::fractionOf (float level, bool isDisplay)
{
    return isDisplay ? juce::jlimit (0.0f, 1.0f, level)
                     : juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (juce::jmax (level, 1.0e-5f)) + 60.0f) / 66.0f);
}

//==============================================================================
void MeterPaint::paintStill (juce::Graphics& g, juce::Rectangle<float> area, const Style& s, bool isDisplay)
{
    if (area.isEmpty())
        return;

    if (s.display == "vu")
    {
        paintVuFace (g, area, s);
        return;
    }

    if (s.display == "vuside")
    {
        paintSideVuFace (g, area, s);
        return;
    }

    if (s.display == "edge")
    {
        paintEdgeStill (g, area, s);
        return;
    }

    if (s.display == "porthole")
    {
        paintGaugeFace (g, area, s);
        return;
    }

    if (s.display == "arc")
    {
        paintRingStill (g, area, s, isDisplay);
        return;
    }

    const bool vertical = area.getHeight() >= area.getWidth();
    const auto cross = vertical ? area.getWidth() : area.getHeight();
    const auto length = vertical ? area.getHeight() : area.getWidth();
    const auto corner = s.display == "tube" ? cross * 0.5f : juce::jlimit (1.5f, 5.0f, cross * 0.2f);

    // Its well, with a little shade along its inside edge.
    g.setColour (s.background);
    g.fillRoundedRectangle (area, corner);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);

    const auto inner = area.reduced (juce::jmax (1.5f, cross * 0.12f));

    if (s.display == "segments" || s.display == "dots")
    {
        // The LEDs, dark until they light.
        const bool round = s.display == "dots";
        const auto count = ledCount (s, vertical ? inner.getHeight() : inner.getWidth(), vertical ? inner.getWidth() : inner.getHeight(), round);

        for (int i = 0; i < count; ++i)
        {
            const auto led = ledArea (inner, vertical, i, count, round ? 0.0f : 2.0f);
            const auto colour = zoneColour (s, ((float) i + 0.5f) / (float) count, isDisplay).withAlpha (0.14f);
            g.setColour (colour);

            if (round)
            {
                const auto d = juce::jmin (led.getWidth(), led.getHeight()) * 0.78f;
                g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (led.getCentre()));
            }
            else
            {
                g.fillRoundedRectangle (led, 1.0f);
            }
        }
    }
    else if (s.display == "neon")
    {
        // The tube, unlit: a faint line.
        const auto core = vertical ? inner.withSizeKeepingCentre (juce::jmax (2.0f, cross * 0.22f), inner.getHeight())
                                   : inner.withSizeKeepingCentre (inner.getWidth(), juce::jmax (2.0f, cross * 0.22f));
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (core, juce::jmin (core.getWidth(), core.getHeight()) * 0.5f);
    }
    else if (s.display == "tube")
    {
        // The glass's body: darker at its edges.
        juce::ColourGradient shade (juce::Colours::black.withAlpha (0.4f), vertical ? area.getX() : area.getCentreX(), vertical ? area.getCentreY() : area.getY(),
                                    juce::Colours::black.withAlpha (0.4f), vertical ? area.getRight() : area.getCentreX(), vertical ? area.getCentreY() : area.getBottom(), false);
        shade.addColour (0.5, juce::Colours::transparentBlack);
        g.setGradientFill (shade);
        g.fillRoundedRectangle (area, corner);
    }

    juce::ignoreUnused (length);
}

void MeterPaint::paintLevel (juce::Graphics& g, juce::Rectangle<float> area, const Style& s, float fraction, float peak, bool isDisplay)
{
    if (area.isEmpty())
        return;

    fraction = juce::jlimit (0.0f, 1.0f, fraction);
    peak = s.peakHold ? juce::jmin (1.0f, peak) : -1.0f;

    if (s.display == "vu")
    {
        paintVuNeedle (g, area, s, fraction, peak, isDisplay);
        return;
    }

    if (s.display == "vuside")
    {
        paintSideVuNeedle (g, area, s, fraction, peak, isDisplay);
        return;
    }

    if (s.display == "edge")
    {
        paintEdgeLevel (g, area, s, fraction, peak, isDisplay);
        return;
    }

    if (s.display == "porthole")
    {
        paintGaugeNeedle (g, area, s, fraction, peak, isDisplay);
        return;
    }

    if (s.display == "arc")
    {
        paintRingLevel (g, area, s, fraction, peak, isDisplay);
        return;
    }

    const bool vertical = area.getHeight() >= area.getWidth();
    const auto cross = vertical ? area.getWidth() : area.getHeight();
    const auto inner = area.reduced (juce::jmax (1.5f, cross * 0.12f));
    const auto crossInner = vertical ? inner.getWidth() : inner.getHeight();

    // The held peak: a bright mark across the meter (an LED of its own on a ladder).
    auto peakMark = [&] (juce::Rectangle<float> r)
    {
        if (peak <= 0.0f)
            return;

        const auto mark = along (r, vertical, peak, peak).expanded (vertical ? 0.0f : 1.0f, vertical ? 1.0f : 0.0f);
        g.setColour (blendColour (s, peak, isDisplay).brighter (0.35f));
        g.fillRect (mark);
    };

    if (s.display == "segments" || s.display == "dots")
    {
        const bool round = s.display == "dots";
        const auto count = ledCount (s, vertical ? inner.getHeight() : inner.getWidth(), crossInner, round);
        const auto peakLed = peak > 0.0f ? juce::jlimit (0, count - 1, (int) (peak * (float) count)) : -1;

        for (int i = 0; i < count; ++i)
        {
            const auto middle = ((float) i + 0.5f) / (float) count;
            const bool lit = middle <= fraction || i == peakLed;

            if (! lit)
                continue;

            const auto led = ledArea (inner, vertical, i, count, round ? 0.0f : 2.0f);
            const auto colour = zoneColour (s, middle, isDisplay);

            if (round)
            {
                const auto d = juce::jmin (led.getWidth(), led.getHeight()) * 0.78f;
                const auto dot = juce::Rectangle<float> (d, d).withCentre (led.getCentre());

                if (s.glow > 0.0f)
                {
                    g.setColour (colour.withAlpha (0.28f * s.glow));
                    g.fillEllipse (dot.expanded (d * 0.45f * s.glow));
                }

                g.setColour (colour);
                g.fillEllipse (dot);
                g.setColour (juce::Colours::white.withAlpha (0.45f));
                g.fillEllipse (juce::Rectangle<float> (d * 0.3f, d * 0.3f).withCentre (dot.getCentre().translated (-d * 0.15f, -d * 0.15f)));
            }
            else
            {
                g.setColour (colour);
                g.fillRoundedRectangle (led, 1.0f);

                // A little shine on its upper half.
                g.setColour (juce::Colours::white.withAlpha (0.16f));
                g.fillRect (led.withHeight (led.getHeight() * 0.45f));
            }
        }
    }
    else if (s.display == "neon")
    {
        const auto core = vertical ? inner.withSizeKeepingCentre (juce::jmax (2.0f, cross * 0.22f), inner.getHeight())
                                   : inner.withSizeKeepingCentre (inner.getWidth(), juce::jmax (2.0f, cross * 0.22f));
        const auto lit = along (core, vertical, 0.0f, fraction);
        const auto top = blendColour (s, fraction, isDisplay);
        const auto rounding = juce::jmin (core.getWidth(), core.getHeight()) * 0.5f;

        if (fraction > 0.0f)
        {
            // The bloom, then the tube in its zones' colours, then its white-hot middle.
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (top.withAlpha (0.1f * s.glow + 0.03f));
                g.fillRoundedRectangle (lit.expanded (cross * 0.1f * (float) k * s.glow), rounding + cross * 0.1f * (float) k * s.glow);
            }

            g.setGradientFill (lengthGradient (s, core, vertical, isDisplay));
            g.fillRoundedRectangle (lit, rounding);

            const auto hot = vertical ? lit.withSizeKeepingCentre (lit.getWidth() * 0.35f, lit.getHeight())
                                      : lit.withSizeKeepingCentre (lit.getWidth(), lit.getHeight() * 0.35f);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.fillRoundedRectangle (hot, rounding * 0.35f);
        }

        peakMark (core.expanded (vertical ? cross * 0.15f : 0.0f, vertical ? 0.0f : cross * 0.15f));
    }
    else if (s.display == "tube")
    {
        const auto corner = crossInner * 0.5f;
        const auto liquid = along (inner, vertical, 0.0f, fraction);

        if (fraction > 0.0f)
        {
            g.setGradientFill (lengthGradient (s, inner, vertical, isDisplay));
            g.fillRoundedRectangle (liquid, corner);

            // Its top, catching the light.
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.fillRect (vertical ? liquid.withHeight (1.5f) : liquid.withX (liquid.getRight() - 1.5f).withWidth (1.5f));
        }

        peakMark (inner);

        // The glass: a highlight down one side, its edge.
        const auto glass = area.reduced (cross * 0.08f);
        const auto stripe = vertical ? glass.withWidth (glass.getWidth() * 0.32f).translated (glass.getWidth() * 0.12f, 0.0f)
                                     : glass.withHeight (glass.getHeight() * 0.32f).translated (0.0f, glass.getHeight() * 0.12f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.3f), stripe.getX(), stripe.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), vertical ? stripe.getRight() : stripe.getX(),
                                                 vertical ? stripe.getY() : stripe.getBottom(), false));
        g.fillRoundedRectangle (stripe, juce::jmin (stripe.getWidth(), stripe.getHeight()) * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawRoundedRectangle (area.reduced (0.5f), cross * 0.5f, 1.0f);
    }
    else
    {
        // The bar: its zones' colours stay where they are along it; a sheen across it and a
        // bright edge at its top.
        const auto bar = along (inner, vertical, 0.0f, fraction);

        if (fraction > 0.0f)
        {
            if (s.glow > 0.0f)
            {
                g.setColour (blendColour (s, fraction, isDisplay).withAlpha (0.18f * s.glow));
                g.fillRoundedRectangle (bar.expanded (cross * 0.08f), 2.0f);
            }

            g.setGradientFill (lengthGradient (s, inner, vertical, isDisplay));
            g.fillRect (bar);

            juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.22f), vertical ? bar.getX() : bar.getCentreX(), vertical ? bar.getCentreY() : bar.getY(),
                                        juce::Colours::white.withAlpha (0.0f), vertical ? bar.getCentreX() : bar.getCentreX(), vertical ? bar.getCentreY() : bar.getCentreY(), false);
            g.setGradientFill (sheen);
            g.fillRect (bar);

            g.setColour (blendColour (s, fraction, isDisplay).brighter (0.5f).withAlpha (0.9f));
            g.fillRect (vertical ? bar.withHeight (juce::jmin (1.5f, bar.getHeight())) : bar.withX (bar.getRight() - 1.5f).withWidth (juce::jmin (1.5f, bar.getWidth())));
        }

        peakMark (inner);

        // 0 dB.
        if (! isDisplay)
        {
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            const auto zero = along (inner, vertical, zeroSignal, zeroSignal);

            if (vertical) g.fillRect (zero.getX(), zero.getY() - 0.5f, zero.getWidth(), 1.0f);
            else          g.fillRect (zero.getX() - 0.5f, zero.getY(), 1.0f, zero.getHeight());
        }
    }

    if (s.scale)
        drawScale (g, inner, vertical, s, isDisplay);
}
