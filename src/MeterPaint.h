// D:\workspace\Fanan_web_hub\src\MeterPaint.h

#pragma once

#include "GuiLayout.h"

//==============================================================================
/**
    How a level meter shows its level: the canvas draws meters with it, in Edit, in Play and
    in a made plugin's window.

    Its "display" (the layout's settings, see GuiWidget::meterKeys):
        bar        a smooth bar, its colours running low to middle to top along it
        segments   a ladder of LEDs, each lit in its zone's colour
        dots       round LEDs with a glow
        neon       a thin glowing tube that changes colour as it rises
        tube       a glass tube filling with coloured light
        vu         an analog VU meter: a face with its scale and a needle (and a peak lamp)
        vuside     an upright VU: a tall face, its needle swinging up and down from its left
        edge       an edgewise meter: a scale on a band behind a long window, a thin pointer
                   running along it (upright when its box is tall)
        porthole   a round gauge in a chrome bezel, its needle turning about its middle
        arc        a ring that fills round, the level in dB in its middle

    Its colours: low, middle and top (the zones: the middle from -12 dB, the top from -3 dB;
    a display's from 75 % and 90 %) and its background (a VU's face). "segments" sets how many
    LEDs (0: as many as fit), "peakHold" keeps a mark at the highest level for a moment,
    "scale" draws its dB marks, "glow" (0..1) how much the light blooms.
*/
namespace MeterPaint
{
    struct Style
    {
        juce::String display { "bar" };
        juce::Colour low { 0xff00c853 }, mid { 0xffffb300 }, high { 0xffd50000 };
        juce::Colour background { 0x99000000 };
        bool backgroundChosen = false;   // a needle meter's face is cream unless one is chosen
        int segments = 0;
        bool peakHold = false, scale = false;
        float glow = 0.5f;
        juce::String font;               // the scale's numbers (FontLibrary's)
    };

    /** The displays, by id and by the name the menu shows. */
    const juce::StringArray& displays();
    const juce::StringArray& displayNames();

    /** A meter's style from its settings. */
    Style styleOf (const GuiWidget& w);

    /** Whether it has a needle (vu, vuside, edge, porthole): it swings after the level, and its
        face is cream unless a background is chosen. */
    bool hasNeedle (const Style&);

    /** Its colours as they come, before any is chosen. */
    Style standardStyle();

    /** A level as the meter shows it, 0..1: a signal in dB (-60..+6), a display as it is. */
    float fractionOf (float level, bool isDisplay);

    /** Its still part: background, frame, scale (a VU's face). */
    void paintStill (juce::Graphics&, juce::Rectangle<float> area, const Style&, bool isDisplay);

    /** Its moving part: the level (fractionOf), the peak held (below 0: none). A VU's needle
        is the level as it is given (the canvas gives it slowed, as a needle moves). */
    void paintLevel (juce::Graphics&, juce::Rectangle<float> area, const Style&, float fraction, float peak, bool isDisplay);
}
