// D:\workspace\Fanan_web_hub\src\GuiLayout.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <vector>

//==============================================================================
/** One element of the plugin's GUI. Controls are bound to a parameter by its id; meters,
    lamps and scopes watch a source (a signal or a module's display); pictures show a file
    from the project's gui/images folder; a keyboard plays notes into the plugin. A custom
    element is one Sonify AI programmed (elements/<Name>.cpp): it draws itself. */
struct GuiWidget
{
    enum class Type { knob, slider, toggle, selector, label, group,
                      meter, scope, lamp, envelope, filter, xy, shape, preset, image, keyboard, custom,
                      registration,   // Fanan's copy protection: the "Unregistered" badge (see Protection)
                      tabs };         // the page tabs: a click shows another page of the window (see GuiLayout::pages)

    Type type = Type::knob;
    juce::Rectangle<int> bounds;   // in the plugin window's pixels; controls include their caption
    juce::String param;            // "<moduleId>.<paramId>" for controls
    juce::String label;            // a control's caption, a group's title, a label's or registration badge's text
    juce::String style;            // knobs, sliders, switches: the name of a look (see Looks)
    juce::String originalStyle;    // ...and the look it wore before the user first tried another (see originalKnown):
    bool originalKnown = false;    // the edit menu's "Back to the original look" puts it back
    juce::Colour colour;           // labels, frames, selectors and the like: their colour (transparent: the default)
    float fontSize = 0.0f;         // labels and group titles (0: the default)
    bool bold = false;
    juce::String font;             // its text's font (FontLibrary's; empty: the standard one)
    float rounding = -1.0f;        // the preset manager: its corners, 0 square .. 1 round ends (below 0: as standard);
                                   // shapes keep theirs in settings["rounding"]
    bool vertical = true;          // sliders
    juce::StringArray options;     // selectors: one per position

    juce::String source;           // meters, lamps, scopes: "voices.out", "plugin.out L", "lfo.position";
                                   // filters: a display with the live cutoff (it opens and closes as it plays)
    juce::String mode;             // meters: "peak" or "rms"; filters: "lowpass", "highpass", "bandpass" (+ "24");
                                   // pictures: how they fill their box (see GuiLayout::pictureModes)
    juce::String image;            // pictures: a file name in the project's gui/images folder
    std::map<juce::String, juce::String> roles;   // envelope: attack, decay, sustain, release;
                                                  // filter: cutoff, resonance; xy: x, y (parameter ids)
    float threshold = 0.5f;        // lamps: on from this value
    juce::String path;             // drawn shapes: SVG path data, stretched to the box
    juce::Colour stroke;           // shapes: outline colour
    float strokeWidth = 0.0f;
    int lowNote = 48, highNote = 84;   // keyboards: the lowest and highest key (MIDI notes; C3 to C6),
                                       // white keys both (see stella::keys::normalise)
    juce::String element;          // custom: the programmed element's name (STELLA_ELEMENT)
    std::map<juce::String, juce::String> sources;   // custom: more live values by role ("sources" in the layout)
    std::map<juce::String, juce::var> settings;   // custom: anything else it reads ("settings" in the layout);
                                                  // shapes: their kind, geometry, material and patterns
                                                  // (see shapeKeys: in the layout beside "type", not under "settings");
                                                  // meters: how they show the level (see meterKeys)

    // Every element is an object: turned about its middle, and as see-through as it's asked.
    float angle = 0.0f;            // degrees, clockwise (0: upright)
    float opacity = 1.0f;          // 0 invisible .. 1 solid

    juce::String page;             // the page it lies on (see GuiLayout::pages); empty: every page

    bool isControl() const noexcept
    {
        return type == Type::knob || type == Type::slider || type == Type::toggle || type == Type::selector;
    }

    bool isLive() const noexcept   { return type == Type::meter || type == Type::scope || type == Type::lamp; }

    //==========================================================================
    // The object: turned and faded like any other (the registration badge never is).

    float shownAngle() const noexcept     { return type == Type::registration ? 0.0f : angle; }
    float shownOpacity() const noexcept   { return type == Type::registration ? 1.0f : juce::jlimit (0.0f, 1.0f, opacity); }
    bool isTurned() const noexcept        { return shownAngle() != 0.0f; }

    /** Its turn about its middle, in the plugin window's pixels. */
    juce::AffineTransform turn() const noexcept
    {
        const auto c = bounds.toFloat().getCentre();
        return isTurned() ? juce::AffineTransform::rotation (juce::degreesToRadians (shownAngle()), c.x, c.y) : juce::AffineTransform();
    }

    /** A point of the window, in the element's own unturned frame (where its box is upright). */
    juce::Point<float> unturned (juce::Point<float> plugin) const noexcept
    {
        return isTurned() ? plugin.transformedBy (turn().inverted()) : plugin;
    }

    /** What it covers, turned: its box's corners where they end up. */
    juce::Rectangle<float> turnedArea() const
    {
        if (! isTurned())
            return bounds.toFloat();

        juce::Path box;
        box.addRectangle (bounds.toFloat());
        return box.getBoundsTransformed (turn());
    }

    //==========================================================================
    // Shapes: a rectangle, circle, triangle or star (or a drawn SVG path), each in a flat colour
    // or a material with pattern layers, flat or raised, with its outline; stretched to its box,
    // and made lopsided by moving its middle (centerX, centerY) or leaning it (leanX, leanY). A
    // line runs from one point to another through its bend point (ObjectPaint::LinePoints).

    /** "rectangle", "circle", "triangle", "star", "line" or "drawn" (its path). */
    juce::String shapeKind() const
    {
        const auto found = settings.find ("shape");
        const auto kind = found != settings.end() ? shapeKindFromName (found->second.toString()) : juce::String();
        return kind.isNotEmpty() ? kind : (path.isNotEmpty() ? juce::String ("drawn") : juce::String ("rectangle"));
    }

    static juce::StringArray shapeKinds()    { return { "rectangle", "circle", "triangle", "star", "line" }; }

    /** A shape's kind from a name ("rect", "oval", "ellipse"...); empty if it isn't one. */
    static juce::String shapeKindFromName (const juce::String& name)
    {
        const auto n = name.trim().toLowerCase().removeCharacters (" -_");

        if (n == "rectangle" || n == "rect" || n == "box" || n == "square" || n == "roundedrectangle" || n == "roundrect") return "rectangle";
        if (n == "circle" || n == "ellipse" || n == "oval" || n == "disc" || n == "disk" || n == "round")                return "circle";
        if (n == "triangle" || n == "tri")                                                                                 return "triangle";
        if (n == "star")                                                                                                   return "star";
        if (n == "line" || n == "bentline" || n == "bendline" || n == "bendingline" || n == "arcline" || n == "bow"
            || n == "arrow" || n == "stroke" || n == "rule" || n == "divider" || n == "handle")                         return "line";
        if (n == "drawn" || n == "path" || n == "drawing" || n == "svg")                                                  return "drawn";
        return {};
    }

    /** The settings a shape keeps beside "type" in the layout. */
    static const juce::StringArray& shapeKeys()
    {
        static const juce::StringArray keys { "shape", "radius", "rounding", "points", "inner", "centerX", "centerY", "leanX", "leanY",
                                              "line", "lineWidth", "arrow", "cap",
                                              "material", "finish", "color2", "scale", "grain", "shine", "flow", "seed",
                                              "pattern", "patterns", "patternColor", "patternOpacity", "patternBlend", "patternSize",
                                              "patternAngle", "patternWidth", "patternDepth", "depth", "shadow", "light", "altitude",
                                              "effects" };
        return keys;
    }

    /** Lamps', selectors', keyboards' and labels' settings (under "settings", or beside "type"):
        "display" (how it looks: PanelParts), a lamp's "follow" and "bezel", a selector's or a
        label's window "background", a label's "arrows". */
    static const juce::StringArray& partKeys()
    {
        static const juce::StringArray keys { "display", "follow", "bezel", "background", "arrows" };
        return keys;
    }

    /** Whether its kind takes a "display" of PanelParts' (a meter's is MeterPaint's). */
    bool takesPartDisplay() const noexcept
    {
        return type == Type::lamp || type == Type::selector || type == Type::keyboard || type == Type::label;
    }

    /** A meter's settings (under "settings" in the layout, or beside "type"): "display" (bar,
        segments, dots, neon, tube, vu, vuside, edge, porthole, arc), "colorLow", "colorMid",
        "colorHigh", "background", "segments" (how many LEDs), "peakHold", "scale" (its dB
        marks), "glow" (0..1). */
    static const juce::StringArray& meterKeys()
    {
        static const juce::StringArray keys { "display", "colorLow", "colorMid", "colorHigh", "background", "segments", "peakHold", "scale", "glow" };
        return keys;
    }

    /** A setting as a number (fallback when it's missing). */
    float number (const juce::String& key, float fallback) const
    {
        const auto found = settings.find (key);
        return found != settings.end() && ! found->second.isVoid() && found->second.toString().isNotEmpty() ? (float) (double) found->second : fallback;
    }

    /** A setting as text: a number or word as it is, a list or object as JSON. */
    juce::String settingText (const juce::String& key) const
    {
        const auto found = settings.find (key);

        if (found == settings.end())
            return {};

        const auto& v = found->second;
        return v.isArray() || v.isObject() ? juce::JSON::toString (v, true) : v.toString();
    }

    /** The roles a type's parameters play, for the ones bound by role. */
    static juce::StringArray rolesOf (Type t)
    {
        if (t == Type::envelope) return { "attack", "decay", "sustain", "release" };
        if (t == Type::filter)   return { "cutoff", "resonance" };
        if (t == Type::xy)       return { "x", "y" };
        return {};
    }

    /** ...and this element's: its type's, or a built-in control's (a RangeSlider's low and high,
        a DualKnob's outer and inner). */
    juce::StringArray rolesFor() const
    {
        if (type == Type::custom)
        {
            if (element == "RangeSlider") return { "low", "high" };
            if (element == "DualKnob")    return { "outer", "inner" };
            return {};
        }

        return rolesOf (type);
    }
};

//==============================================================================
/**
    The plugin's GUI as data, kept in the project as gui/layout.json. Sonify AI drafts it,
    the user reshapes it in the Edit UI tab, and the studio draws it. Knobs, sliders and
    switches name a look: KnobMaker layer documents, made in the Knob Studio (see Looks),
    so they look the same there, on the canvas and in the exported plugin. Pictures are
    picture elements showing files from the project's gui/images folder; a background
    picture is one covering the window, first in the list.

    The widgets lie in layers in the list's order: the first at the back, the last in front.
*/
class GuiLayout
{
public:
    int width = 720, height = 420;

    /** The panel's global light, as Photoshop's: where it comes from (degrees: 120 the upper
        left) and how high (degrees above the panel). The knobs' looks, the shapes, their layer
        styles and the painted backgrounds are all lit from here. */
    float lightAngle = 120.0f, lightAltitude = 30.0f;

    /** The window's pages, by name, in order (none: one page). Each element lies on one of them
        or on every page (its page empty: the background, the name, the preset manager, the tabs),
        and a tabs element shows them as tabs: a click shows another page. */
    juce::StringArray pages;

    /** Whether an element shows on a page (by name; the first page when none's given). */
    bool isOnPage (const GuiWidget& w, const juce::String& pageName) const
    {
        return pages.isEmpty() || w.page.isEmpty() || w.page == pageName || (! pages.contains (w.page) && pageName == pages[0]);
    }

    juce::Colour backgroundTop { 0xff2b2b30 }, backgroundBottom { 0xff17171a };
    juce::String backgroundImage;              // read from older layouts only: fromVar turns it into a
    juce::String backgroundMode { "fill" };    // picture element at the back (see fromVar)
    std::vector<GuiWidget> widgets;

    static constexpr const char* fileName = "layout.json";   // in the project's gui folder
    static constexpr const char* imagesFolder = "images";    // the pictures, in the gui folder
    static constexpr int captionHeight = 20;                 // the caption under a control
    static constexpr int minWidth = 240, maxWidth = 2400, minHeight = 160, maxHeight = 1600;

    /** How a picture fills its box: "fill" (covers it, cropping the edges), "fit" (whole,
        keeping its shape), "stretch", "centre" (its own size) or "tile". With names for menus. */
    static juce::StringArray pictureModes()       { return { "fill", "fit", "stretch", "centre", "tile" }; }
    static juce::StringArray pictureModeNames()   { return { "Fill (crop the edges)", "Fit (whole picture)", "Stretch", "Centre (own size)", "Tile" }; }

    /** The picture types the studio reads. */
    static juce::String pictureFiles()            { return "*.png;*.jpg;*.jpeg;*.gif"; }
    static bool isPicture (const juce::File& file);

    /** Copies a picture into the images folder (named after it; a different picture with
        that name gets a number) and returns its file name there, or empty if it failed.
        The same picture again reuses its copy. */
    static juce::String importPicture (const juce::File& picture, const juce::File& imagesFolderFile);

    /** Pictures painted in code (paint_picture: by Fanan AI, or the Edit tab's material
        controls), kept in gui/painted.json with how each was painted: they're the studio's to
        paint again, while the user's own pictures are never overwritten. */
    static constexpr const char* paintedListName = "painted.json";
    static juce::StringArray paintedPictures (const juce::File& guiFolder);

    /** How a painted picture was painted: what paint_picture got ("element", "file", "width",
        "height", "color", "settings"); void when it wasn't painted, or before that was kept. */
    static juce::var paintedRecipe (const juce::File& guiFolder, const juce::String& pictureName);
    static void rememberPainted (const juce::File& guiFolder, const juce::String& pictureName, const juce::var& recipe);

    static GuiLayout fromVar (const juce::var& json);
    juce::var toVar() const;

    /** The background picture's file: the backmost picture element covering the whole
        window (empty: none). */
    juce::String backgroundPicture() const;

    /** One element as JSON (the same fields as in layout.json), and back. */
    static juce::var widgetToVar (const GuiWidget& widget);
    static GuiWidget widgetFromVar (const juce::var& json);

    static juce::Result load (const juce::File& file, GuiLayout& result);
    juce::Result save (const juce::File& file) const;

    /** What a first GUI is made from: one group per module, a control per parameter. */
    struct ParamInfo
    {
        juce::String id, module, name, unit;
        float min = 0.0f, max = 1.0f, def = 0.0f;
    };

    static GuiLayout makeDefault (const juce::Array<ParamInfo>& params, const juce::String& title);

    static juce::String typeName (GuiWidget::Type type);

    /** A type by name; a name that isn't a toolbox type is a custom element ("custom", or
        the programmed element's own name). An empty name is a knob. */
    static GuiWidget::Type typeFromName (const juce::String& name);

    /** The toolbox's own element a type name means ("dropdown": "DropDown", "filter3d":
        "FilterSweep"); empty if none. They're custom elements made by the studio
        (stella_builtin_elements.cpp, stella_builtin_3d.cpp). */
    static juce::String builtInElementFor (const juce::String& typeName);

    /** Keyboards: a MIDI note's name ("C4" is middle C, 60), and a note from JSON: a name
        ("F#2", "Bb3") or a number. */
    static juce::String noteName (int note);
    static int noteFromVar (const juce::var& value, int fallback);
    static juce::String colourToString (juce::Colour colour);
    static juce::Colour colourFromString (const juce::String& text, juce::Colour fallback);
};
