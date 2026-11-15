// D:\workspace\Fanan_web_hub\src\SelectorSet.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GuiLayout.h"

#include <vector>

//==============================================================================
/**
    Selector sets: a selector's positions as keys of their own, set out as a whole (a row, a
    column, triangles zipped tip up and tip down, a grid, keys round a ring or an arc, a
    joined tab strip or label ring, a D-pad, a rocker), drawn flat (2D) or as 3D keys, each
    lit when it's on.

    A selector wears one when its "settings" have a "set" (the design, below); its "mode"
    says how its keys behave:
        "single"    one at a time (the standard): the parameter is the key's number
        "toggles"   each key on and off by itself: its own parameter each, the selector's
                    parameter with the key's number after it ("fx" -> fx1, fx2, ...) unless
                    its roles "key1", "key2"... name others
        "multi"     several at once: one parameter, a key a bit (key 1 = 1, key 2 = 2, key 3 = 4...)

    The design ("set"), every key optional:
        "render"        "3d" (the standard) or "2d"
        "formation"     row, column, zipper, grid, ring, arc, segmented, cross, rocker
        "vertical"      zipper, segmented, rocker: up and down
        "columns"       grid: how many across
        "shape"         round (the standard), rect, circle, pill, triangle, segment (a ring's
                        or an arc's curved keys), led (a lamp with its name beside it)
        "gap"           between keys, % of a key
        "rounding"      corners, %
        "arcFrom", "arcTo"  ring and arc: degrees, 0 at the top, clockwise (an arc 100..260
                        runs round the bottom)
        "inner"         ring and arc: the keys' inner edge, % of the outer
        "labels"        where their names are: "caps" in the keys (the standard), "beside" out
                        of them, or "none"
        "labelSide"     out of them: "auto" (under a row, a grid or a strip; beside a column;
                        outside a ring or an arc; past a D-pad's arms), "above", "below",
                        "left", "right", "inside" (a ring's or an arc's inner side)
        "labelAngle"    "horizontal" (the standard: always read straight) or "follow" (they
                        follow the keys: up a tall key, along a ring's curve, round a ring's
                        edge, beside a column's edge)
        "textSize" %, "font", "bold", "italic", "upper" (capitals), "spacing" (%: between
                        the letters), "labelColour", "labelOnColour", "labelGlow" (a lit
                        key's name glows), "textEffects" (Photoshop's layer style on them)
        "capColour", "capOnColour"   the keys off and on; "keyColours": [...] and "group":
                        colours in turn, a group of keys each (a step row's)
        "glowColour", "glow" (0..100)   the light of a key that's on; "edge": its rim lit
                        (dimly when off, "edgeOff" %)
        "led"           none, dot, bar: a lamp on each key; "ledColour"
        "plate", "plateColour"   a plate the keys sit in
        "style"         2D: soft (the standard), flat, glossy
        "material", "height" (% of a key), "travel" (% of its height)   3D
*/
namespace SelectorSet
{
    /** Whether a selector wears a set, its design and its mode. */
    bool has (const GuiWidget& selector);
    juce::var designOf (const GuiWidget& selector);
    juce::String modeOf (const GuiWidget& selector);

    /** The parameter a key has in "toggles" mode. */
    juce::String keyParam (const GuiWidget& selector, int key);

    struct Key
    {
        juce::Path outline;               // the key
        juce::Path region;                // what's redrawn when it lights (its glow, its name)
        juce::Rectangle<float> label;     // where its name goes
        float angle = 0;                  // round a ring: its turn (radians)
        bool flipped = false;             // a zipper's tip down (or right)
        bool curved = false;              // a ring's or an arc's curved key
        juce::Point<float> ringCentre;    // round a ring: its middle...
        float textRadius = 0, textArc = 0;   // ...the radius its name follows there, and how much of the ring it may take (radians)
        float edge = 0;                   // out of it, beside an upright edge (a column's): -pi/2 (its name runs up it)
    };

    /** Its keys in an area (from 0, 0). */
    std::vector<Key> layout (const juce::var& design, juce::Rectangle<float> area, int count);

    /** The key at a point of the area; -1: none. */
    int keyAt (const juce::var& design, juce::Rectangle<float> area, int count, juce::Point<float> at);

    /** The set drawn in area, the keys in lit on. resolution: pixels a unit (a zoomed-in view
        asks for more). Drawn once, then kept, for each size, light and set of names. */
    void paint (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, const juce::StringArray& names,
                const std::vector<bool>& lit, float lightAngle, float lightAltitude, float resolution = 1.0f);

    /** The gallery's templates (and how many keys their pictures show). */
    juce::StringArray templateNames();
    juce::var makeTemplate (int index);
    int templateKeys (int index);

    /** The Selector gallery's folder (.fksel: { "name", "keys", "set" }). */
    juce::File galleryFolder();
    constexpr const char* fileExtension = ".fksel";
}
