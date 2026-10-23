// D:\workspace\Fanan_web_hub\src\stella_element_api.h
//
// Programmed GUI elements: for a GUI element the toolbox doesn't have (a drop-down list,
// a step sequencer, a custom-drawn knob...), write a class that draws itself and answers
// the mouse, in elements/<Name>.cpp. The studio shows it live in Edit UI and Play, and
// exported plugins and apps draw it the same way, pixel for pixel.
//
// Use it in gui/layout.json as { "type": "custom", "element": "<Name>", "x", "y", "w", "h" }
// plus what it needs: "param" (its parameter), "params" (more, by role: { "x": "id" }),
// "label", "options" (a list of texts), "color", "source" (a signal or display to watch),
// "sources" (more, by role: { "position": "osc.position", "cutoff": "synth.cutoff" }) and
// "settings" (anything else: { "rows": 4, "style": "round" }).
//
// An element can do everything the toolbox's own can (parameters, live signals and displays,
// notes, presets, pictures, popups), so any of them can be split up, combined or remade in
// another shape: a preset list apart from its Save and Load, a preset table in a popup...
//
// Coordinates are pixels, floats, from the element's top-left corner (0, 0) to
// (width(), height()); pixel (x, y) covers x..x+1, y..y+1. Drawing is anti-aliased and
// clipped to the element (a popup to the popup). Colours are 0xAARRGGBB.
//
// Read values in paint() and the mouse functions, not in the constructor: the element
// knows its size and parameters only once it's placed (resized() is called first).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace stella::ui
{
    //==========================================================================
    using Colour = std::uint32_t;   // 0xAARRGGBB: 0xff2080ff is solid blue

    constexpr Colour rgb (int r, int g, int b) noexcept
    {
        return 0xff000000u | ((std::uint32_t) (r & 255) << 16) | ((std::uint32_t) (g & 255) << 8) | (std::uint32_t) (b & 255);
    }

    Colour withAlpha (Colour c, float alpha) noexcept;    // alpha 0..1, times the colour's own
    Colour mix (Colour a, Colour b, float t) noexcept;    // t 0: a, 1: b
    Colour brighter (Colour c, float amount) noexcept;    // amount 0..1: towards white
    Colour darker (Colour c, float amount) noexcept;      // amount 0..1: towards black

    enum class Align { left, centre, right };

    /** A picture of the plugin's (Element::picture), ready to draw. */
    struct Image
    {
        int width = 0, height = 0;
        std::vector<std::uint32_t> pixels;    // width x height, row by row from the top: premultiplied 0xAARRGGBB

        bool isValid() const noexcept    { return width > 0 && height > 0 && pixels.size() == (std::size_t) width * (std::size_t) height; }
    };

    /** What a preset manager does besides choosing a preset (Element::doPreset). */
    enum class PresetCommand
    {
        save,          // the values over the chosen preset (one of the user's)
        saveAs,        // a new preset of the user's: the studio asks its name
        saveFactory,   // making the plugin: a preset it comes with (asks its name)
        load,          // a preset file from anywhere (asks which)
        saveBank,      // every preset into one bank file (asks where)
        loadBank,      // a bank file's presets (asks which)
        remove,        // deletes the chosen preset (asks "are you sure")
        showFolder     // opens the folder the user's presets are in
    };

    //==========================================================================
    /** Where an element draws. Angles are in radians, 0 at 12 o'clock, going clockwise
        (a knob from 7 to 5 o'clock: -2.36 to 2.36). */
    class Canvas
    {
    public:
        int width() const noexcept    { return width_; }
        int height() const noexcept   { return height_; }

        void fillAll (Colour c);

        /** One pixel, for pictures made point by point (noise, grain, brushed metal, wood):
            blends c over the pixel at whole-number x, y (an opaque colour replaces it); outside
            the clip it does nothing. Cheap enough to fill a whole window pixel by pixel in a
            picture painted once (paint_picture). */
        void drawPixel (int x, int y, Colour c);
        void fillRect (float x, float y, float w, float h, Colour c);
        void fillRoundedRect (float x, float y, float w, float h, float radius, Colour c);
        void fillEllipse (float x, float y, float w, float h, Colour c);
        void fillCircle (float cx, float cy, float radius, Colour c);
        void fillTriangle (float x0, float y0, float x1, float y1, float x2, float y2, Colour c);
        void fillPolygon (const float* xy, int numPoints, Colour c);           // xy: x0, y0, x1, y1, ...
        void fillPie (float cx, float cy, float radius, float from, float to, Colour c);

        /** Gradients: top to bottom in a box (radius > 0 rounds its corners), or from the
            middle of a circle out. */
        void fillGradient (float x, float y, float w, float h, Colour top, Colour bottom, float radius = 0.0f);
        void fillRadialGradient (float cx, float cy, float radius, Colour inside, Colour outside);

        /** Outlines of boxes and ellipses lie inside the box. Lines and arcs are centred on
            their path, with round ends (arcs: square ends unless roundEnds). */
        void drawRect (float x, float y, float w, float h, float thickness, Colour c);
        void drawRoundedRect (float x, float y, float w, float h, float radius, float thickness, Colour c);
        void drawEllipse (float x, float y, float w, float h, float thickness, Colour c);
        void drawLine (float x0, float y0, float x1, float y1, float thickness, Colour c);
        void drawPolyline (const float* xy, int numPoints, float thickness, Colour c);
        void drawArc (float cx, float cy, float radius, float from, float to, float thickness, Colour c, bool roundEnds = false);

        /** One line of text, centred up and down in the box and placed across it by align;
            too long, it ends in "...". size is the font's height in pixels (13 is usual). */
        void drawText (const std::string& text, float x, float y, float w, float h, float size, Colour c,
                       Align align = Align::centre, bool bold = false);
        float textWidth (const std::string& text, float size, bool bold = false);

        /** A picture (Element::picture) with its top-left at x, y (whole pixels), at its own
            size; alpha 0..1 fades it. The second draws only part of it: sw x sh pixels from sx, sy
            (one frame of a filmstrip, a strip of frames one under the other: sy = frame * sh). */
        void drawImage (const Image& image, float x, float y, float alpha = 1.0f);
        void drawImage (const Image& image, float x, float y, int sx, int sy, int sw, int sh, float alpha = 1.0f);

        /** Drawing stays inside this box until resetClip() (a scrolling list, say). */
        void setClip (float x, float y, float w, float h);
        void resetClip();

    private:
        friend struct CanvasAccess;
        Canvas() = default;

        std::uint32_t* pixels_ = nullptr;
        int width_ = 0, height_ = 0;
        int clip_[4] {};
        void* painter_ = nullptr;
    };

    //==========================================================================
    struct Mouse
    {
        float x = 0.0f, y = 0.0f;    // in the element's pixels (on the popup: the popup's)
        bool shift = false;          // Shift or Ctrl held: fine moves
        bool doubleClick = false;
    };

    //==========================================================================
    /** A programmed element. Override paint(), and the mouse functions it needs. */
    class Element
    {
    public:
        virtual ~Element() = default;

        virtual void paint (Canvas& g) = 0;
        virtual void resized() {}                                   // width() / height() are new (also once at the start)

        /** The mouse on the element. Return true from mouseDown to get the drag and mouseUp. */
        virtual bool mouseDown (const Mouse&)                       { return false; }
        virtual void mouseDrag (const Mouse&)                       {}
        virtual void mouseUp (const Mouse&)                         {}
        virtual void mouseMove (const Mouse&)                       {}  // hovering, no button down
        virtual void mouseExit()                                    {}
        virtual void mouseWheel (const Mouse&, float /*notches*/)   {}  // + is up

        //======================================================================
        // A popup: a box drawn above the whole window (a drop-down's list, say), while it's
        // open. A click outside it closes it (and isn't passed on).
        void openPopup (float x, float y, float w, float h);       // in the element's pixels: x 0, y height() opens it just below
        void closePopup();
        bool isPopupOpen() const;

        virtual void paintPopup (Canvas&)                           {}  // the popup's own pixels: (0, 0) is its top-left
        virtual void popupMouseDown (const Mouse&)                  {}
        virtual void popupMouseDrag (const Mouse&)                  {}
        virtual void popupMouseUp (const Mouse&)                    {}
        virtual void popupMouseMove (const Mouse&)                  {}
        virtual void popupMouseExit()                               {}
        virtual void popupMouseWheel (const Mouse&, float /*notches*/) {}
        virtual void popupClosed()                                  {}  // closed by a click outside it

        //======================================================================
        int width() const;
        int height() const;

        /** Parameters. role "" is the element's own ("param" in the layout); others are
            named in its "params". Values are the parameter's own (e.g. Hz); normalised is
            0..1 along its range, as a knob shows it. */
        bool hasParam (const char* role = "") const;
        float value (const char* role = "") const;
        float normalised (const char* role = "") const;
        float minimum (const char* role = "") const;
        float maximum (const char* role = "") const;
        float defaultValue (const char* role = "") const;
        int choices (const char* role = "") const;                 // whole-number positions: maximum - minimum + 1
        std::string paramName (const char* role = "") const;
        std::string unit (const char* role = "") const;
        std::string valueText (const char* role = "") const;       // "440 Hz"

        /** Setting a value. Alone, each call is one undoable change for the host; for a drag,
            put the calls between beginEdit and endEdit (endEdit comes by itself on mouseUp). */
        void setValue (float newValue, const char* role = "");
        void setNormalised (float proportion, const char* role = "");
        void beginEdit (const char* role = "");
        void endEdit (const char* role = "");

        /** From the layout. */
        std::string label() const;
        int numOptions() const;
        std::string option (int index) const;
        Colour colour (Colour fallback) const;                      // "color", or fallback if none
        float setting (const char* key, float fallback) const;      // "settings": a number
        std::string settingText (const char* key, const std::string& fallback = {}) const;

        /** Live. Reading level() or seconds() in paint() redraws the element every frame. */
        float level (bool rms = false) const;                       // its "source": a peak 0..1+ (or RMS), or a display's value
        void readScope (float* destination, int numSamples) const;  // its "source": the newest samples, oldest first

        /** More live values, by role (the layout's "sources"; role "" is its own "source"): a
            display's value as its module shows it (Hz, 0..1...), a signal's peak (or RMS) and
            newest samples. hasSource says whether the role is bound, so the element can fall
            back on a parameter's value when it isn't. */
        bool hasSource (const char* role = "") const;
        float level (const char* role, bool rms = false) const;
        void readScope (const char* role, float* destination, int numSamples) const;

        //======================================================================
        /** The plugin's presets, the same ones the toolbox's preset manager has: those it comes
            with, the user's own and the banks they loaded, each under its group ("Factory",
            "My presets", a bank's name), in the manager's order. Every part of a preset manager
            can be an element of its own (a list, a table in a popup, Save and Load buttons, a
            menu of the commands): they all work on these presets, follow each other, and the
            element is drawn again when they change. */
        int numPresets() const;
        std::string presetName (int index) const;
        std::string presetGroup (int index) const;
        int currentPreset() const;                                  // -1: none chosen
        bool isPresetModified() const;                              // its values changed since it was chosen (a manager adds "*")
        void choosePreset (int index);                              // puts its values on the plugin
        void stepPreset (int delta);                                // the next (+1) or previous (-1), round the list

        /** A preset command, with the dialog it needs (a name, a file, "are you sure"): the
            studio shows the dialog, the element only says which. canDoPreset: whether it can be
            done now (save: the chosen preset is the user's, otherwise saveAs; remove: it can be
            deleted; saveFactory: only while the plugin is being made). */
        void doPreset (PresetCommand command);
        bool canDoPreset (PresetCommand command) const;

        //======================================================================
        /** A picture of the plugin's (gui/images: the ones the user added and the painted ones,
            "knob.png"), scaled smoothly to w x h (0 and 0: its own size; one of them 0: keeping
            its shape), up to 4 million pixels. Empty (isValid() false) when there's no such
            picture. Load it in resized() (the size is known there) and keep it: it's too slow
            for every paint. */
        Image picture (const std::string& name, int w = 0, int h = 0) const;
        bool pictureSize (const std::string& name, int& w, int& h) const;   // its own size; false if there's none

        double seconds() const;                                     // a clock, for animation
        void playNote (int note, float velocity);                   // MIDI note 0..127; velocity 0..1, 0 stops it
        bool isNoteDown (int note) const;                           // sounding, from anywhere

        /** Draw again (it's done by itself after the mouse, and when its values change). */
        void repaint();

    private:
        friend struct ElementAccess;
        void* instance_ = nullptr;
    };

    //==========================================================================
    struct Registrar
    {
        Registrar (const char* name, const char* description, Element* (*make)());
    };
}

/** Makes an element available, after its class: STELLA_ELEMENT (DropDown, "A drop-down list"). */
#define STELLA_ELEMENT(Type, Description) \
    static ::stella::ui::Registrar stellaElementRegistrar_##Type (#Type, Description, [] () -> ::stella::ui::Element* { return new Type(); });
