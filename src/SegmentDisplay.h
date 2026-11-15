// D:\workspace\Fanan_web_hub\src\SegmentDisplay.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GuiLayout.h"

//==============================================================================
/**
    LED segment displays, as the 80s synths had them: each character a cell of glowing bars,
    7 of them in an "8" for numbers, 14 or 16 (with diagonals and a split top and bottom) for
    letters too, the bars that aren't lit faintly there behind them.

    A label wears one with "display": "led" (PanelParts), showing its words or, with a
    "param", its parameter's value; a preset manager's bar with "barDisplay": "led"
    (PresetPanel). The characters always fill the area: as many cells as the text needs
    ("cells" 0) or as many as it's given, stretched to its width and height.

    Its settings (a label's "settings"; a preset bar's with "led" before them: "ledSegments"...):
        "segments"      "auto" (7 for numbers, 16 once there's a letter), "7", "14", "16"
        "cells"         how many characters (0: as many as the text needs)
        "ghost"         0..60 %: how much the bars that aren't lit show
        "glow"          0..100: the light round the lit bars
        "slant"         0..15 degrees: leaning forward, as printed LED digits do
        "thickness"     6..30 %: the bars' thickness, of a cell's width
        "window"        "none", "flat" (the standard: a dark window) or "3d" (a window set in
                        a bezel, with its glass)
        "background"    the window's colour
    Its lit colour is the label's colour (one colour for the whole display).
*/
namespace SegmentDisplay
{
    struct Style
    {
        int segments = 0;               // 0 auto, 7, 14, 16
        int cells = 0;                  // 0: as many as the text needs
        juce::Colour lit { 0xffff3b2a };
        juce::Colour background { 0xff0c0a0a };
        float ghost = 0.10f;            // 0..0.6
        float glow = 0.4f;              // 0..1
        float slant = 6.0f;             // degrees
        float thickness = 0.16f;        // of a cell's width
        int window = 1;                 // 0 none, 1 flat, 2 3d
    };

    /** A label's style (its colour the lit one; transparent: red). */
    Style styleOf (const GuiWidget& label);

    /** A preset bar's: its design's "led..." keys. */
    Style styleOf (const juce::var& design, const juce::String& prefix, juce::Colour litFallback);

    /** The colours offered by name: Red, Green, Amber, Blue, White, Cyan. */
    const juce::StringArray& colourNames();
    juce::Colour colourNamed (int index);

    /** How many cells a text takes (a dot, colon or comma after a character takes none). */
    int cellsOf (const juce::String& text);

    /** Whether it's drawn with 16 segments (auto: once there's a letter). */
    int segmentsFor (const Style&, const juce::String& text);

    /** The display in an area: its window, then the text in its cells (cells: the style's
        unless given), filling it. */
    void paint (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, const Style& style, int cells = 0);

    /** Just the characters (no window) in an area. */
    void paintCharacters (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, const Style& style, int cells = 0);
}
