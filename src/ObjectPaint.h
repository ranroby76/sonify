// D:\workspace\Fanan_web_hub\src\ObjectPaint.h

#pragma once

#include "GuiLayout.h"
#include "LayerDoc.h"   // LayerStyle: Photoshop's layer style

#include <array>
#include <string>
#include <vector>

//==============================================================================
/**
    The panel's shapes, drawn: a rectangle, circle, triangle or star (or a drawn SVG path),
    stretched to its box and made lopsided by moving its middle (centerX, centerY: the
    triangle's top point, the star's and the circle's heart) or leaning it (leanX, leanY);
    or a line from one point to another, bowed through its bend point; filled with a flat
    colour (two colours: a two-tone shade) or a material (stella_paint.h's metals, stones,
    plastics, woods, clear and tinted glass... in their finishes, with pattern layers over
    them as in Photoshop), flat, raised or pressed in, with a shadow and an outline.

    Lit by the panel's global light, as the knobs' looks are, after it's turned: a turned shape
    keeps the light where the light is, while its material's grain and patterns turn with it.
    Over it, Photoshop's layer style: bevel and emboss with its contour, stroke, inner shadow,
    colour overlay and drop shadow (LayerRender::applyLayerStyle).

    A shape in a flat colour (and its outline) is drawn as a vector, at any zoom. One with a
    material, a pattern, relief or a shadow is painted pixel by pixel into a picture: the
    canvas keeps it until the shape changes, and paints big ones on a thread of their own.
*/
namespace ObjectPaint
{
    /** Its outline in its own box (0, 0 to width, height), upright, inset by half its
        outline's width so the outline stays inside the box. A line's is the outline of the
        whole stroke: its width, its caps or arrowheads, along its curve. */
    juce::Path outline (const GuiWidget& shape, float width, float height);

    //==========================================================================
    /** Per corner, Photoshop's Distort (every shape but a line): its box's four corners moved
        one at a time, the rest staying put, and the shape mapped into the four-sided figure
        they make (straight lines stay straight). Kept as fractions of its box, inside its
        outline's inset ("corners": [x, y of the top-left, top-right, bottom-right,
        bottom-left]), so resizing the box scales it. Its lean isn't used while it has them. */
    bool hasCorners (const GuiWidget& shape);

    /** Its four corners in its own box's pixels: top-left, top-right, bottom-right, bottom-left. */
    std::array<juce::Point<float>, 4> cornerPoints (const GuiWidget& shape);

    /** Per corner from now on, starting from its shape as it is (its lean taken into its corners). */
    void startCorners (GuiWidget& shape);

    /** Back to its box: its corners (and the lean they held) let go. */
    void stopCorners (GuiWidget& shape);

    /** One corner (0 top-left .. 3 bottom-left) moved to `local` (in its own box's pixels, its
        box as it was when the drag began: atStart); the other three stay. Upright, it may go
        past its box (fitCornersBox makes the box fit again); turned, it stays inside. */
    void moveCorner (GuiWidget& shape, const GuiWidget& atStart, int corner, juce::Point<float> local);

    /** Its box made snug round its corners, which stay where they are on the panel (upright only). */
    void fitCornersBox (GuiWidget& shape);

    //==========================================================================
    /** A line (shape kind "line"): from its start to its end, through its bend points, in
        order. Each bend point is an arc (the line curves smoothly through it) or a corner (it
        turns sharply there, straight on either side). With one arc bend point, the line is
        straight while it lies on the way between the ends; moved off it, the line bows through
        it, bulging most near it. Its points are kept in its box as fractions ("line": [x1, y1,
        bendX, bendY, ... x2, y2], one bend point or more), their kinds in "bendKinds" ("arc" or
        "corner" each; left out: arcs); "lineWidth" (pixels), "arrow" (none, start, end, both)
        and "cap" (round, flat, square) shape its stroke. Graph art: a zigzag of corners, a wave
        of arcs, or both. */
    static constexpr int maxBends = 16;

    struct LinePoints
    {
        juce::Point<float> start, end;               // in its own box's pixels (0, 0: the box's top-left)
        std::vector<juce::Point<float>> bends;       // start to end: one at least
        std::vector<bool> corners;                   // each bend point's kind: a corner (else an arc)
    };

    LinePoints linePoints (const GuiWidget& line);

    int  bendCount (const GuiWidget& line);
    bool isCornerBend (const GuiWidget& line, int bend);
    void setBendCorner (GuiWidget& line, int bend, bool corner);

    /** A new bend point on the line, halfway along the part after bend point `after` (-1: the
        longest part), an arc: the line looks as it did. Returns its index (-1: it has its most). */
    int addBend (GuiWidget& line, int after);

    /** One bend point gone (never the last one). */
    void removeBend (GuiWidget& line, int bend);

    /** One bend point moved to `local` (its own box's pixels, its box as it was when the drag
        began: atStart); the rest stay where they are. */
    void moveBend (GuiWidget& line, const GuiWidget& atStart, int bend, juce::Point<float> local);

    /** Whether dragging the line itself bows it (one arc bend point); otherwise it moves it. */
    bool canPull (const GuiWidget& line);

    /** Gives it these points (in its own box's pixels). fitBox: its box is made to hold the
        line snugly (an upright line only), its points staying where they are on the panel. */
    void setLinePoints (GuiWidget& line, LinePoints points, bool fitBox);

    /** The bend points back on the way between its ends, evenly: straight again (their kinds kept). */
    void straighten (GuiWidget& line);

    /** How far along it (0 its start .. 1 its end) the point of it nearest to `local` lies (in
        its own box's pixels). */
    float lineParameterAt (const GuiWidget& line, juce::Point<float> local);

    /** Pulled by the mouse: the line as it was at the drag's start, bowed so that the point
        grabbed (at `along`, from lineParameterAt) lies at `local` (its own box's pixels). Near
        straight, it snaps straight. */
    void pullLine (GuiWidget& line, const GuiWidget& atStart, float along, juce::Point<float> local);

    /** One end moved to `local` (its own box's pixels): the bow keeps its shape along the line. */
    void moveLineEnd (GuiWidget& line, const GuiWidget& atStart, bool start, juce::Point<float> local);

    /** Whether a bend point is off the way between its ends. */
    bool isBent (const GuiWidget& line);

    /** Its stroke's width in pixels. */
    float lineWidthOf (const GuiWidget& line);

    /** Where its own handle lies, in its own box (0, 0 at the box's top-left), upright: the
        rectangle's corner rounding, the triangle's top point, a line's bend point, the middle
        of anything else. */
    juce::Point<float> adjustPoint (const GuiWidget& shape);

    /** Its own handle dragged by moved (in its own box's pixels, upright) from where it was
        when the drag began (atStart): the rounding, the top point, a line's bend point or the
        middle follows. */
    void adjust (GuiWidget& shape, const GuiWidget& atStart, juce::Point<float> moved);

    /** Whether it's painted pixel by pixel (a material, a pattern, relief, a shadow or a layer
        style). Clear and tinted glass are see-through: the panel shows through them. */
    bool isPainted (const GuiWidget& shape);

    /** Its layer style (Photoshop's: bevel and emboss, stroke, inner shadow, colour overlay,
        drop shadow), from its "effects" setting; a shape from before layer styles: its relief
        ("depth"), its shadow and its outline, as the style that looks the same. */
    LayerStyle styleOf (const GuiWidget& shape);

    /** Gives it the style, which takes the place of its older relief, shadow and outline (and
        of a light of its own: it's lit by the global light). */
    void setStyle (GuiWidget& shape, const LayerStyle&);

    /** A shape that isn't painted: filled and outlined as a vector, turned as it is (the
        opacity is the caller's). fallback: its colour when it has none of its own. */
    void drawFlat (juce::Graphics&, const GuiWidget& shape, juce::Colour fallback);

    //==========================================================================
    /** What painting a shape takes: values only, so it can go to another thread. */
    struct Job
    {
        juce::String key;                 // everything it depends on, but where it lies and its opacity
        juce::Path outline;               // in its own box
        int width = 0, height = 0;        // its box
        float angle = 0.0f;               // degrees, clockwise
        std::string surface;              // stella_paint's settings (JSON): material, finish, color2, scale, grain, patterns...
        juce::Colour colour, colour2;     // its colours (transparent: the material's own)
        bool material = false;            // false: a flat colour, lit only where it has relief
        float depth = 0.0f;               // its bevel, in pixels: + raised, - pressed in, 0 flat
        float shadow = 0.0f;              // 0..1, the shadow a raised (or flat) shape casts
        float light = 120.0f, altitude = 30.0f;  // the panel's global light (or the shape's own "light")
        juce::Colour stroke;
        float strokeWidth = 0.0f;
        LayerStyle style;                         // Photoshop's layer style over it
        float globalAngle = 120.0f, globalAltitude = 30.0f;
    };

    /** fallback: its colour when it has none of its own; the panel's global light. */
    Job jobFor (const GuiWidget& shape, juce::Colour fallback, float globalAngle = 120.0f, float globalAltitude = 30.0f);

    /** A painted shape: its picture, and where the picture's top-left lies from the middle of
        the shape's box, in plugin pixels. */
    struct Painting
    {
        juce::Image image;
        juce::Point<float> offset;
        juce::String key;
        float angle = 0.0f;
        int width = 0, height = 0;        // the box it was painted for
    };

    /** Paints it (any thread). */
    Painting paint (const Job&);

    /** About how many pixels a job paints: big ones go to a thread of their own. */
    int pixelsOf (const Job&);
}
