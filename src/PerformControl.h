// D:\workspace\Fanan_web_hub\src\PerformControl.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GuiLayout.h"

//==============================================================================
/**
    Performance controls: what a player's hand works while playing. An X-Y pad wears one
    when its "settings" have a "perform" (the design, below):

        wheel       a wheel in its slot, ribbed, with its mark (up and down, or across)
        wheels      a pitch wheel and a mod wheel side by side in one housing
        lever       a Roland-style bender: across bends, pushed away or pulled toward
                    modulates; in "bendmod" both go back to the middle when it's let go
        joystick    "style": ball (a ball riding a cross), vector (a stick in a cross-shaped
                    slot), arcade (a ball-topped stick), gimbal (a stick in a rubber boot),
                    thumb (a thumbstick's dished cap)
        pad         an X-Y surface with a glowing puck and its cross-hair
        ribbon      a ribbon strip with its marker, or a row of lamps (an LED touch strip)

    Its parameters are the pad's: "x" and "y" (its "params"). A wheel, a ribbon: one of them
    ("y" up and down, "x" across); the pair of wheels: "x" the left wheel, "y" the right.

    How it moves ("settings"):
        "mode"      "free" (the standard: it stays where it's left) or "bendmod" (pitch bend
                    and modulation: the bend axis goes back to its middle when it's let go,
                    the other stays)
        "bendAxis"  "x" or "y": which one bends (the pair of wheels: x is the left wheel)

    The design ("perform"), every key optional:
        "kind", "style", "vertical" (wheel, ribbon: up and down; the standard for a wheel)
        "render"        "3d" (the standard) or "2d" (a pad, a ribbon)
        "housingColour", "partColour" (the wheel, the ball, the stick), "accentColour" (its
                        mark, the puck, its lamps, its light), "material" (the moving part's)
        "glow" (0..100), "lit" (a light in its slot or under its pad), "leds" (a ribbon:
                        lamps along it), "travel" (%: how far it moves), "range" (push and pull:
                        a joystick's or a lever's whole sweep, edge to edge, in degrees, up to 170;
                        a stick's standard 160, 80 each way), "clean" (a wheel: no ribs, no mark)
        "tilt"          degrees: seen from the front (0 straight down)
        "frames"        pictures each way (the standard: 17 for two axes, 33 for one)

    It's drawn as a grid of pictures, one for each position, made as they're needed and kept,
    so the exported plugin can play them as frames.
*/
namespace PerformControl
{
    bool has (const GuiWidget& pad);
    juce::var designOf (const GuiWidget& pad);
    juce::String modeOf (const GuiWidget& pad);
    juce::String bendAxisOf (const GuiWidget& pad);

    /** Its push and pull angle: the whole sweep, edge to edge (degrees, 10..170). */
    float rangeOf (const juce::var& design);

    /** Whether it leans (a joystick, the lever): its angle is what "range" sets. */
    bool leans (const juce::var& design);

    /** Whether both its axes go back to the middle when it's let go (in "bendmod"): the lever. */
    bool springsBoth (const juce::var& design);

    /** What it moves: "x", "y", "both", or "pair" (two wheels). */
    juce::String axesOf (const juce::var& design);

    /** The pair of wheels: which one is at a point (0 the left, 1 the right); else 0. */
    int partAt (const juce::var& design, juce::Rectangle<float> area, juce::Point<float> at);

    /** Pictures each way (a one-axis control: along it). */
    int framesOf (const juce::var& design);

    /** It at x, y (0..1; y 0 at the bottom), drawn in area. */
    void paint (juce::Graphics& g, const juce::var& design, juce::Rectangle<float> area, float x, float y,
                float lightAngle, float lightAltitude, float resolution = 1.0f);

    /** The gallery's templates: their designs, and the pad's settings ("mode", "bendAxis") they come with. */
    juce::StringArray templateNames();
    juce::var makeTemplate (int index);
    juce::var templateSettings (int index);

    juce::File galleryFolder();
    constexpr const char* fileExtension = ".fkperf";
}
