// D:\workspace\Fanan_web_hub\src\Body3D.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace MeshField { struct Grid; }

//==============================================================================
/**
    The plugin window's 3D body: a whole instrument's case (a desktop synth's brick with the
    step forward where its keyboard sits, a rack unit, a stompbox, a groovebox...) built from
    parts, rendered behind everything else on the panel (Render3D::renderBody), seen by the
    panel camera and lit by the panel's global light. The controls stand on it as they stand on
    any panel.

    It lives in the layout (GuiLayout::body, its "body" in layout.json) as JSON:

        { "tilt": 0, "shadow": 60, "softness": 55, "exposure": 0, "floor": "#00000000",
          "mirrorAt": 360, "parts": [ { ...a part... }, ... ] }

    "mirrorAt": where its mirrored parts are mirrored across (pixels from the window's left; the
    standard: the window's middle). It moves with the body when the body's moved or resized.

    Every part is a shape in a box: "x", "z" its middle on the window (pixels, from its left
    and top), "y" its foot above the floor, "w", "h", "d" its width, height and depth.

        "kind"      box, cylinder, sphere, extrude (a 2D "profile" pulled through its box),
                    revolve (a profile turned round its upright middle), text (letters
                    standing up from, or cut into, what's under them), sweep (a section
                    drawn along a path), loft (one profile becoming another through its box),
                    mesh (a model from an OBJ or STL file)
        "plane"     an extrude's profile: "side" (seen from the side: x 0 the back .. 1 the
                    front, y 0 the top .. 1 the bottom; pulled across its width), "front"
                    (x 0 left .. 1 right, y 0 top .. 1 bottom; pulled back through its depth)
                    or "top" (seen from above: x 0 left .. 1 right, y 0 the back .. 1 the
                    front; pulled up through its height)
        "profile"   SVG path data in 0..1 of the plane (M, L, H, V, C, Q, A, Z...); a
                    revolve's x is its radius (0 the middle .. 1 its edge), y 0 top .. 1 bottom
        "svg"       instead of "profile": an SVG file in the design's gui/images, its outline
        "axis"      a cylinder's: "y" upright (the standard), "x" lying across, "z" lying
                    back to front
        "op"        "add" (the standard) or "cut" (taken away from the parts before it).
                    Where a part's surface meets an earlier one's, the later part's in front
                    (its colour and finish show)
        "blend"     pixels: blended into (or out of) the parts before it, like a casting
        "round"     pixels: its edges rounded; "chamfer": its edges cut at 45 degrees
        "rotate"    [x, y, z] degrees round its middle: a sloped rear panel, a tilted section
        "hidden"    true: left out for now (the Body editor's eye)
        "mirror"    "x": a copy mirrored across the body's middle ("mirrorAt"; the standard: the
                    window's middle), left and right
        "repeat"    { "kind": "row" | "grid" | "circle", "count", "countZ", "dx", "dz",
                    "radius" }: copies of it along x (and z), or round its middle
        "material"  one of Model3D::materialNames() (Gloss plastic ... Painted metal)
        "color"     "#AARRGGBB"
        "surface"   none, tolex, knurl, wood, perforated, rubber, brushed, leather, grille;
                    "surfaceScale" (0.25..4) and "surfaceDepth" (0..100)
        "glass"     0..100: see-through (a display's window, clear acrylic)
        "glow"      "#AARRGGBB" light it gives (a screen, an LED strip); "glowStrength" 0..100
        "text", "font", "bold"   a text part's words and font (the panel fonts)
        "name"      what it's called in the parts list
        "shell"     pixels: hollow, its walls this thick; "open": none, top, bottom, front,
                    back, left, right (the face left open: a tray, a hood, a sleeve)

    A sweep: its "path" (SVG path data in 0..1 of its "plane": top, front or side, as an
    extrude's), "thickness" (pixels: its section's size), "section": round, square, or
    "profile" (its "profile" in 0..1 of the section: x across the path, y 0 top .. 1 bottom).
    A loft: its sections lie in its "plane" (top: stacked from the bottom up; front: from the
    back to the front; side: from the left to the right): its "profile" at the start, its
    "profileEnd" at the end (none: the same one), "taper" (% the end's size), "twist"
    (degrees the end turns).
    A mesh: its "file" (in the design's gui folder: "models/case.obj"), "fit": keep (its
    proportions, in its box) or stretch (to its box), "up": y (the standard) or z (most STL
    and CAD models stand on z).
*/
class Body3D
{
public:
    Body3D();
    ~Body3D();
    Body3D (const Body3D&) = delete;
    Body3D& operator= (const Body3D&) = delete;

    /** Read from the layout's "body" for a window width x height (images: the design's
        gui/images, for "svg" parts). Null when it has no parts. */
    static std::unique_ptr<Body3D> fromVar (const juce::var& json, int width, int height, const juce::File& images = {});

    /** Whether a layout's body has anything to draw. */
    static bool hasParts (const juce::var& json);

    /** Where a body's mirrored parts are mirrored across: its "mirrorAt" (pixels from the
        window's left), or the middle of a window this wide. */
    static float mirrorAtOf (const juce::var& json, int width);

    /** Where a mesh part's file is: its "file" in the gui folder (the one holding images). */
    static juce::File meshFile (const juce::var& part, const juce::File& images);
    static constexpr const char* modelsFolder = "models";

    //==========================================================================
    enum class Kind { box, cylinder, sphere, extrude, revolve, text, sweep, loft, mesh };
    enum class Plane { side, front, top };
    enum class Surface { none, tolex, knurl, wood, perforated, rubber, brushed, leather, grille };

    struct Part
    {
        juce::String name;
        int source = -1;                    // which of the JSON's parts it is
        Kind kind = Kind::box;
        Plane plane = Plane::side;
        int axis = 1;                       // a cylinder's: 0 x, 1 y, 2 z
        bool cut = false;
        float blend = 0, round = 0, chamfer = 0;
        float cx = 0, cy = 0, cz = 0;       // its middle, from the window's middle (y: up)
        float hw = 1, hh = 1, hd = 1;       // half its width, height, depth
        float rot[9] { 1, 0, 0, 0, 1, 0, 0, 0, 1 };   // world -> its own (transposed rotation)
        bool rotated = false;
        bool mirrorX = false;
        int repeatKind = 0;                 // 0 none, 1 row, 2 grid, 3 circle
        int count = 1, countZ = 1;
        float dx = 0, dz = 0, radius = 0;

        int material = 0;
        juce::Colour colour { 0xff202024 };
        Surface surface = Surface::none;
        float surfaceScale = 1.0f, surfaceDepth = 0.5f;
        float glass = 0;                    // 0..1
        juce::Colour glow;                  // transparent: none
        float glowStrength = 0.6f;

        // A profile's (or text's) distance field, in pixels, over its plane.
        struct Field
        {
            int w = 0, h = 0;
            float x0 = 0, y0 = 0, cell = 1;  // the field's first cell, in the plane's pixels
            std::vector<float> d;           // signed: below 0 inside
            float at (float u, float v) const;
        };

        std::shared_ptr<Field> field;

        // Hollow: its walls' thickness, the face left open (0 none, 1 top, 2 bottom, 3 front,
        // 4 back, 5 left, 6 right).
        float shell = 0;
        int open = 0;

        // A sweep: its path's field (signed distance from the path, over its plane), its
        // section (0 round, 1 square, 2 a profile: sectionField) and its size.
        int section = 0;
        float thickness = 10;
        std::shared_ptr<Field> sectionField;

        // A loft: its end's profile (none: the start's), how big the end is, how far it turns.
        std::shared_ptr<Field> fieldEnd;
        float taper = 1, twist = 0, lipschitz = 1;

        // A mesh: its model's distance field, and its scale each way (pixels a model unit).
        std::shared_ptr<const MeshField::Grid> mesh;
        float meshScale[3] { 1, 1, 1 };

        float lo[3] { 0, 0, 0 }, hi[3] { 0, 0, 0 };   // its box on the window, every copy of it
    };

    const std::vector<Part>& getParts() const noexcept   { return parts; }

    /** The window's size, and the body's view and light settings. */
    int width = 0, height = 0;
    float tilt = 0, shadow = 60, softness = 55, exposure = 0;
    juce::Colour floor { 0x00000000 };

    float mirrorAxis = 0;   // where mirrored parts are mirrored across, from the window's middle

    /** The body's box (from the window's middle; y up from the floor). */
    float lo[3] { 0, 0, 0 }, hi[3] { 0, 0, 0 };

    //==========================================================================
    /** Its distance at a point (from the window's middle, y up, pixels), and which part is
        there (its index; -1: none). solidOnly: see-through parts left out (what's behind glass). */
    float distance (float x, float y, float z, int& part, bool solidOnly = false) const;

    /** How a part's surface varies at a point (its texture): its colour times colourScale,
        its roughness plus roughAdd. */
    void surfaceAt (int part, float x, float y, float z, float& colourScale, float& roughAdd) const;

private:
    std::vector<Part> parts;

    float partDistance (const Part&, float x, float y, float z) const;
    static float shapeDistance (const Part&, float qx, float qy, float qz);
    static float relief (const Part&, float x, float y, float z);
};
