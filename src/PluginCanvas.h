// D:\workspace\Fanan_web_hub\src\PluginCanvas.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UiHelpers.h"   // FineSlider

#include "GuiLayout.h"
#include "Looks.h"
#include "ObjectPaint.h"
#include "PresetLibrary.h"
#include "Primitives.h"
#include "ModMatrix.h"
#include "PresetPanel.h"

class Body3D;

#include <bitset>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>

class WasmElements;

//==============================================================================
/**
    The Edit UI tab's canvas: the plugin's window, drawn from its layout, sitting on a big
    grid you zoom and move around, like Colosseum's rack. Ctrl + wheel zooms (10% to 200%)
    around the mouse, the wheel scrolls (Shift + wheel: sideways), dragging empty space
    moves the view, and the minimap and the scrollbars jump anywhere. Fit shows the whole
    window.

    Play mode: the controls work like the finished plugin's: drag a knob or slider
    (Shift for fine moves), click a switch or a selector's position, drag in an XY pad,
    double-click for the default, use the mouse wheel; a keyboard's keys play notes while
    they're held (sliding across them plays each in turn). Meters, lamps and scopes move
    with the sound; envelope and filter curves follow their parameters; keyboards light up
    for every note that sounds, a MIDI keyboard's too.

    Edit mode: drag primitives in from the toolbox (or pictures from the desktop). Click an
    element to select it; right-click it for its menu (the one menu): Copy, Paste,
    Duplicate and its layer at the top, then what edits it (its parameter typed in by id),
    Delete, and an instruction for Sonify AI at its foot. Right-click the window's empty
    space for the window's own menu (size, colours, a picture behind everything, Paste).
    Drag to move (snapped); drag the selected element's handles (corners and sides) to
    resize it, Shift on a corner keeps its shape. Delete removes it, the arrow keys nudge
    it (Shift: further). Dragging a group carries what's inside. The window's own corner
    handle resizes the window; the Crop tool (the Crop button beside Fit, or C) crops it as
    Photoshop does: a box over it whose sides and corners drag in or out (past the window:
    it grows), dragged inside to move it, then Enter (or Apply, or a double-click) to crop to
    it, Esc to leave it. Double-clicking a knob, slider or switch opens its menu, where
    "Edit in Knob Studio" opens its look there, and "Apply this knob on similars" puts its look
    on every control of its kind (ticked: for now; Apply: for good); double-clicking a picture
    picks its file.

    Elements lie in layers, in the layout's order: the first at the back. The menu's Layer
    list brings one to the front or forward, or sends it backward or to the back (also
    Ctrl+Shift+], Ctrl+], Ctrl+[, Ctrl+Shift+[). Forward and backward go one layer of what
    overlaps it: a picture over a row of buttons and a background goes under the buttons,
    still over the background; again, and it's behind the background. Copy, Paste and Duplicate (Ctrl+C, Ctrl+V,
    Ctrl+D): a copied frame takes what's inside it along, and a paste lands where the menu
    was opened (Ctrl+V: under the mouse). Copies go through the system clipboard, so they
    paste into another project too.

    Every element is an object: the round handle above the selected one turns it (Shift: in
    steps of 15 degrees), its menu has its position, size, angle and opacity, and a turned one
    is still resized by its handles and played in its own frame. Shapes (rectangles, circles,
    triangles, stars) have a handle of their own (the yellow diamond) that rounds a rectangle's
    corners, moves a triangle's top point or a star's or circle's middle (lopsided), and their
    menu gives them a material with pattern layers, relief, a shadow and an outline.

    Several at once: Shift+click adds an element to the selection (or takes it away), and
    Shift+drag on empty space draws a box that adds what it touches. Dragging any of them
    moves them all; Delete, the arrow keys, Copy, Duplicate, Bring to front and Send to back
    work on them all, and their menu gives them one colour; Ctrl+M merges the shapes among
    them into one (Photoshop's Merge Shapes); a click on one of them alone selects just that one. A background picture is a picture element covering the window
    at the back; until it's selected, dragging it moves the view, as empty space does.

    Knobs, sliders and switches are drawn from their looks (see Looks): a frame for each
    value, made by KnobMaker's layer renderer. Programmed elements (Sonify AI's own, from
    the project's elements folder) draw themselves, through the GUI's sandboxed module
    (WasmElements); in Play mode they get the mouse, and their popups lie over everything.
*/
class PluginCanvas final : public juce::Component,
                           public juce::DragAndDropTarget,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    struct Param
    {
        int index = -1;
        juce::String id, name, unit;
        float min = 0.0f, max = 1.0f, def = 0.0f, value = 0.0f, skew = 1.0f;
        int decimals = -1;     // how many its value shows (-1: by its range; a percentage none)
    };

    PluginCanvas();
    ~PluginCanvas() override;

    void setLayout (const GuiLayout& newLayout);
    const GuiLayout& getLayout() const noexcept    { return layout; }

    /** The element chosen in Edit (-1: none). */
    int selectedElement() const noexcept           { return selected; }

    /** Fanan AI lays the window out again from a ready-made layout (PanelLayouts), bound to the
        plugin's own parameters, and dresses it in a material look (none: the current look), with
        its controls, displays and fonts, painted panel as asked. A window with a design: asked first. */
    void startFromLayout (const juce::String& layoutName, const juce::String& lookName, bool withControls, bool withTheme, bool withPainting);

    // A built-in material look (MaterialArchetypes) on one shape, or on the whole window (-1);
    // the whole window's knobs, sliders and switches too when withControls, its displays and
    // fonts when withTheme, and its painted panel behind everything when withPainting.
    bool applyMaterialLook (const juce::String& name, int widgetIndex, bool withControls = false, bool withTheme = false,
                            bool withPainting = false);

    // Paints a look's painted panel (MaterialArchetypes::painterRecipe) behind everything, in the
    // window's painted background if it has one (otherwise a new one).
    void paintMaterialPanel (const juce::String& lookName);

    /** The open project's gui folder: its pictures are in images/, the view is kept in
        view.json. None: no project. */
    void setGuiFolder (const juce::File& folder);

    void setDesignMode (bool shouldDesign);
    void setParameters (const juce::Array<Param>& newParams);

    /** The plugin's own window in Sonify (a plugin made here, on a track): Play mode, the panel
        alone, fitted to the component, with no grid, caption, minimap or scrollbars, and a view
        that doesn't move or zoom by hand. */
    void setPanelOnly (bool shouldBePanelOnly);
    bool isPanelOnly() const noexcept    { return panelOnly; }

    /** What meters, lamps and scopes can watch: signals ("voices.out", "plugin.out L") and
        module displays ("lfo.position"). For the edit menu's lists. */
    void setSources (const juce::StringArray& signals, const juce::StringArray& displays);

    /** What the preset manager widgets show: the presets under their headings, the chosen one
        (and whether it changed), and what can be done (Save, Delete...). */
    void setPresets (const PresetMenu& presets);

    /** The GUI's programmed elements from the latest build (nullptr: none, or not built yet). */
    void setElements (std::shared_ptr<WasmElements> newElements);

    /** The looks knobs, sliders and switches are drawn from (owned by the Workspace). */
    void setLooks (Looks& looksToUse);

    /** A look changed, came or went: draw again. */
    void looksChanged();

    /** A control takes a look; or gets a copy of its look of its own (named after its caption,
        returned), so changing it changes only that control. */
    void useLook (int widgetIndex, const juce::String& lookName);
    juce::String giveOwnLook (int widgetIndex);

    /** "Apply this knob on similars" (a control's menu): every other control of its kind (knobs,
        sliders the same way up, switches) wears its look. trySimilars on: they wear it for now,
        each one's own look kept; off: they wear their own again. applySimilars: for good, nothing
        kept (one control's try at a time: trying another puts the last one's back first).
        similarsToChange: how many of them wear another look now. */
    void trySimilars (int widgetIndex, bool on);
    void applySimilars (int widgetIndex);
    bool isTryingSimilars (int widgetIndex) const noexcept   { return widgetIndex >= 0 && similarsTry.widget == widgetIndex; }
    int similarsToChange (int widgetIndex) const;

    /** Edit mode: the Gallery's knobs, sliders and switches (KnobStudio::showGallery) on the panel.
        useGalleryItem: a control wears one instead of its look, keeping its size, place, parameter,
        caption and the rest of its settings; only a control of the same kind (a knob for a knob).
        chooseGalleryItem opens the Gallery to pick one for a control. addGalleryItem: a new control
        wearing one, centred on a point of the plugin window (or in the middle of what's showing).
        The item becomes the plugin's own look, its credits kept with it. */
    void chooseGalleryItem (int widgetIndex);
    bool useGalleryItem (int widgetIndex, const juce::File& file);
    void addGalleryItem (const juce::File& file, std::optional<juce::Point<float>> centre = {});

    /** The Gallery window's (GalleryHub) designs: worn by the selected element when it's that
        kind ("set" a selector, "perform" an X-Y pad, "matrix" a matrix or one of its parts,
        "presets" a preset manager), else (or asNew) a new element wearing it, in the middle of
        the view. settings: the element's own it comes with (a performance control's mode);
        keys: a set's (a new selector's positions). */
    void useGalleryDesign (const juce::String& kind, const juce::var& design, const juce::var& settings, int keys, bool asNew,
                           int target = -1, std::optional<juce::Point<float>> at = {});

    /** A body from the Body gallery (made for a window fromWidth x fromHeight): the window's. */
    void useGalleryBody (const juce::var& body, int fromWidth, int fromHeight);

    /** The window was fromWidth x fromHeight (its top-left now at shift in what it was): the 3D
        body grows or shrinks with it (and moves with a crop). */
    void bodyFollowsWindow (int fromWidth, int fromHeight, juce::Point<int> shift = {});

    /** An icon: the selected Icon element's, else a new Icon element. */
    void useGalleryIcon (const juce::String& name, int target = -1, std::optional<juce::Point<float>> at = {}, const juce::var& badge = {});

    /** A kept shape (the Shapes Gallery's .fkshapes): on the panel as a picture. */
    void addGalleryShape (const juce::File& file, std::optional<juce::Point<float>> at = {});

    /** Whether an element can wear a gallery design of that kind (GalleryHub's drags). */
    static bool wearsGalleryDesign (const GuiWidget& w, const juce::String& kind, const juce::var& design);

    /** Photoshop's Global Light window: the panel's one light (the layout's "light"), which
        lights the looks, the shapes and painted backgrounds. */
    void editGlobalLight();

    /** The window's 3D body (GuiLayout::body) in the Body editor (BodyEditor), a window of its
        own that stays open beside the panel. */
    void editBody();

    /** The Showroom: the finished instrument, its 3D body with the controls printed on it,
        seen from anywhere round it. */
    void showShowroom();

    /** The panel's controls alone (see-through round them), at scale: or with the panel's
        background under them. What the Showroom prints on the body. */
    juce::Image controlsPicture (float scale, bool withBackground);

    /** An element's studio, in the edit window's Advanced edit tab (what the E under it
        opens): a selector's keys (SelectorSet: the Selector Studio), an X-Y pad as a
        performance control (PerformControl: the Performance Studio), a modulation matrix
        (ModMatrix: the Matrix Studio), a preset manager (PresetPanel: the Preset Studio). */
    void editAdvanced (int widgetIndex);

    /** The plugin's window as it shows: the panel, and the side panels joined to it (a
        preset manager's), open ones only in Play (plugin pixels: a left one's from below 0). */
    juce::Rectangle<int> windowBounds() const;

    /** A side panel opened or closed in Play: the window it sits in is wider or narrower. */
    std::function<void()> onWindowChanged;

    /** Photoshop's Layer Style window for a shape (its "effects"). */
    void editLayerStyle (int widgetIndex);

    /** Text effects: Photoshop's Layer Style window on the element's words (its words, caption,
        positions' names, readout), PanelParts::textEffectsOf. */
    void editTextEffects (int widgetIndex);

    /** Edit mode: a primitive's element, centred on a point of the plugin window (in its
        pixels), or in the middle of what's showing. A background primitive asks for the
        picture instead. */
    void addPrimitive (const Primitive& primitive, std::optional<juce::Point<float>> centre = {});

    //==========================================================================
    /** The view: how big the window shows, 10% to 200%, kept around a point of the canvas
        (by default its middle). */
    void setZoom (float newZoom, std::optional<juce::Point<float>> around = {});
    float getZoom() const noexcept    { return zoom; }

    /** The whole window in view (at most at 100%). */
    void fitToView();

    static constexpr float minZoom = 0.10f, maxZoom = 2.0f;

    //==========================================================================
    /** An edit menu's instruction for Sonify AI, about one element (-1: the whole window). */
    std::function<void (int widgetIndex, const juce::String& instruction)> onAskAi;

    /** The banner's Open chat button. */
    std::function<void()> onOpenChat;

    /** The banner across the top: Sonify AI at work on a request made here, then its answer. */
    void showAiWorking (const juce::String& what);
    void showAiReply (const juce::String& reply);

    //==========================================================================
    /** For Export: the GUI as images the exported plugin draws from. Everything that never
        moves is in the background (pictures too); each control's moving part is a strip of
        frames (controls with the same look and size share one: their look's own frames, a
        switch's off and on). Meters, scopes, curves and XY dots are drawn live by the plugin
        itself. */
    /** An element's size when it's added, at a point of the plugin window. */
    static juce::Rectangle<int> defaultBounds (GuiWidget::Type type, int x, int y);

    /** The registration badge: its words when none are given, and how it's drawn (the
        exported plugin shows the same picture). */
    static juce::String registrationText();
    static void drawRegistration (juce::Graphics&, const GuiWidget&);

    struct Bake
    {
        juce::Image background;

        struct Strip
        {
            juce::Image image;   // frames stacked vertically
            int frameWidth = 0, frameHeight = 0, frames = 0;
        };

        std::vector<Strip> strips;
        std::vector<int> stripOf;                        // per widget: its strip, or -1
        std::vector<juce::Rectangle<int>> movingArea;    // per widget: where its moving part goes

        juce::Image registration;                        // the registration badge, if the panel has one
        juce::Rectangle<int> registrationArea;           // ...and where it goes
    };

    Bake bake();

    /** For Export to plugin files (PluginExporter, PluginCanvas_Export.cpp): the window baked page
        by page, as the exported plugin shows it. Each page's background holds everything still on
        it; what moves is a strip of frames, drawn as the panel draws it (a control's look at each
        value, turned and faded as it is; a meter at each level; a lamp lit; a selector at each
        position; a preset manager at each preset, then with none chosen; the page tabs at each
        page; a readout at each value). Scopes, curves, XY pads, keyboards and programmed
        elements are drawn live by the plugin. presetNames: the presets the plugin carries. */
    struct ExportBake
    {
        int width = 0, height = 0;
        std::vector<juce::Image> pages;      // each page's background (one when the window has no pages)

        struct Strip
        {
            juce::Image image;                // frames stacked top to bottom
            int frameWidth = 0, frameHeight = 0, frames = 0;
        };

        std::vector<Strip> strips;

        /** One element as the plugin has it (stella::gui::Widget), in the layout's order. */
        struct Item
        {
            int widget = -1;                  // its place in the layout
            juce::String kind;                // stella::gui::Kind's name: "knob" .. "tabs", "readout"
            int page = -1;                    // the page it's on; -1: every page
            juce::Rectangle<int> area;        // where its frames go, or what's drawn live
            juce::Rectangle<int> box;         // where it's played, upright
            int strip = -1;
            int count = 0, mode = 0;          // as stella::gui::Widget has them
            float angle = 0.0f;
            bool ownParams = false;           // param and param2 are its parameters, not the element's
            juce::String param, param2;       // (a key of a set; a wheel of a pair)
            int flags = 0;                    // stella::gui::Widget's "vertical" (a performance control's springs, its pair)
            std::shared_ptr<GuiWidget> element;   // what stands in for it: a programmed element (a matrix, a preset manager)
        };

        std::vector<Item> items;

        /** Pictures the programmed elements standing in ask for by name (a preset manager's keys). */
        std::vector<std::pair<juce::String, juce::Image>> pictures;
    };

    ExportBake bakeForExport (const juce::StringArray& presetNames);

    /** The panel as it plays now, as the finished plugin shows it (no Edit marks), scaled:
        the store's picture. Invalid when there's no panel. */
    juce::Image snapshot (float scale);

    /** The same, page by page (one picture when the window has no pages): what Sonify writes
        into a plugin's source as its window's pictures (SourceExport). */
    std::vector<juce::Image> snapshotPages (float scale);

    std::function<void (int index, float value)> onParameterChanged;
    std::function<void (int note, float velocity)> onNote;         // a keyboard's key: down (velocity 0..1) or up (0)
    std::function<bool (int note)> isNoteDown;                     // whether a note is sounding, for the keyboards to show
    std::function<void()> onLayoutEdited;                          // save it

    /** No plugin behind it (KnobMaster): a control with no parameter of its own is given a
        new one ("knob3", "switch2"...), so no two share one and move together. A layout set
        with such controls has them named so too. */
    bool inventsParams = false;
    bool nameUnboundControls();   // true: some were named (the layout changed)
    juce::String freshParamId (const juce::String& base, const juce::StringArray& avoid) const;
    std::function<void (int widgetIndex, bool includeSimilars)> onEditLook;   // open a control's look in the Knob Studio (with the controls like it)

    /** Paints a picture in code, as paint_picture does (the recipe: "element", "file", "width",
        "height", "color", "settings", and "quiet": the canvas reads it afresh itself); done
        gets what went wrong, or nothing when it's painted. The Edit tab's material controls. */
    std::function<void (const juce::var& recipe, std::function<void (const juce::String& problem)> done)> onPaintPicture;
    std::function<void (int presetIndex)> onPresetChosen;          // a preset manager's arrows or list, in Play mode
    std::function<void (PresetAction action)> onPresetAction;      // its menu's Save, Save as, Load, banks...
    std::function<void (const juce::String& why)> onElementsFailed; // the programmed elements crashed or got stuck

    /** A protected plugin's window (panel only): a click on the registration badge in Play mode
        opens its Registration window; once it's registered, the badge isn't drawn. */
    std::function<void()> onRegistrationClicked;
    void setRegistered (bool isRegistered);

    /** Live data, read about 30 times a second. */
    std::function<float (const juce::String& source, bool rms)> readLevel;
    std::function<void (const juce::String& source, float* destination, int numSamples)> readScope;
    std::function<void (const juce::StringArray& levels, const juce::StringArray& scopes)> onSourcesChanged;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void moved() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // Primitives dragged in from the toolbox.
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    // Pictures dragged in from the desktop.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    class ElementMenu;
    class AiBanner;
    class ElementBridge;
    friend class BodyEditor;
    friend class SelectorStudio;
    friend class PerformStudio;
    friend class MatrixStudio;
    friend class PresetStudio;
    friend class Showroom;

    //==========================================================================
    // The elements
    static constexpr int snapStep = 4;                 // design grid, in plugin pixels
    static constexpr int edgeSnap = 8;                 // how near a resized side sticks to the window's edge
    static constexpr float handleSize = 10.0f;         // resize handles, in view pixels
    static int snapped (float v)                       { return juce::roundToInt (v / (float) snapStep) * snapStep; }
    static bool isInteractive (const GuiWidget& w);
    static juce::String typeDisplayName (GuiWidget::Type type);
    static juce::Colour standardColour (const GuiWidget& w);   // what it's drawn in with no colour of its own
    static const char* defaultStarPath();

    /** A project is open: the window shows (even with nothing on it yet). */
    bool hasPanel() const noexcept    { return guiFolder != juce::File(); }

    void timerCallback() override;
    int widgetAt (juce::Point<float> plugin, bool interactiveOnly) const;
    const Param* paramById (const juce::String& id) const;
    const Param* paramFor (const GuiWidget& widget) const    { return paramById (widget.param); }
    float proportionOf (const Param& p, float value) const;
    float valueAt (const Param& p, float proportion) const;
    float currentValue (const Param& p) const;
    float roleProportion (const GuiWidget& w, const juce::String& role, float fallback) const;
    void setValue (const Param& p, float value);

    /** Repaints only what shows these parameters (their controls, displays and programmed
        elements); everything in Edit, where more of the canvas shows them. */
    void repaintParameters (const std::vector<int>& indices);

    /** Whether a widget may be skipped while painting: one with a bounded picture lying wholly
        outside the area being painted (panel pixels). */
    bool liesOutside (const GuiWidget& w, juce::Rectangle<float> painted) const;
    void setXy (const GuiWidget& w, juce::Point<float> plugin);

    /** What to draw of a widget: all of it, only what never moves, or only what moves. */
    enum class Part { all, still, moving };

    /** override >= 0 draws the moving part as if at that value: a proportion for knobs and
        sliders, 0 or 1 for switches and lamps, a position for selectors. */
    void drawWidget (juce::Graphics&, int index, Part part = Part::all, float override = -1.0f);
    bool bareBars = false;   // a plain preset manager drawn without its name (the export's picture of it)
    void drawUpright (juce::Graphics&, int index, Part part, float override);   // drawWidget's work, before it's turned and faded
    void drawShape (juce::Graphics&, const GuiWidget&, int index);
    void drawLive (juce::Graphics&, const GuiWidget&, int index, Part part, float override);
    void drawCurve (juce::Graphics&, const GuiWidget&, Part part);
    void drawPanel (juce::Graphics&, const std::function<void (int)>& behind = nullptr);   // the window's colours and background picture, what's behind the 3D body (behind draws each), the body; in plugin pixels

    /** An element sent behind the 3D body (its "behindBody" setting; a body there): drawn
        before it. The elements behind it are a layer group of their own, under it; "Send
        backward" from the bottom of the ones over it passes under it, "Bring forward" from
        the top of the ones under it comes back over it. */
    bool isBehindBody (int index) const;
    void setBehindBody (int index, bool behind);
    void drawBody (juce::Graphics&);    // ...and its 3D body (GuiLayout::body), over them

    //==========================================================================
    // Programmed elements (PluginCanvas_Elements.cpp)
    struct ElementView
    {
        juce::String type;                 // the element it was made as
        bool created = false;
        int width = 0, height = 0;
        juce::Image image, popupImage;
        juce::Rectangle<int> popup;        // the open popup, in plugin pixels (empty: closed)
    };

    void syncElements();                   // the layout's custom elements made, resized or let go
    void refreshElements (bool all);       // drawn again where they changed (all: every one)
    void drawElement (juce::Graphics&, const GuiWidget&, int index);
    void drawElementPopups (juce::Graphics&);
    int popupAt (juce::Point<float> plugin) const;   // whose open popup is there (-1: none)
    bool anyPopupOpen() const;
    void closeElementPopups();

    /** The mouse to a programmed element (or its popup): event as in WasmElements::mouse. */
    bool elementMouse (int index, bool popup, int event, juce::Point<float> plugin, const juce::ModifierKeys& mods,
                       bool doubleClick = false, float notches = 0.0f);

    /** Hovering in Play mode: the element (or popup) under the mouse hears it move, the one
        it left hears it leave. */
    void hoverElements (juce::Point<float> plugin, const juce::ModifierKeys& mods);
    void checkElementsFailed();
    float frameLevel (const juce::String& source, bool rms);
    void drawPicture (juce::Graphics&, const juce::String& name, juce::Rectangle<float> area, const juce::String& mode);
    juce::Rectangle<int> movingAreaOf (const GuiWidget& widget) const;
    juce::Rectangle<int> knobSquare (const GuiWidget& widget) const;

    /** A knob's, slider's or switch's look: its frame for a value, in its moving area. */
    void drawLook (juce::Graphics&, const GuiWidget&, juce::Rectangle<int> area, float value);

    /** A keyboard: its keys at rest (what never moves), then the keys that are down, drawn
        the way the exported plugin draws them over its baked background. */
    void drawKeyboard (juce::Graphics&, const GuiWidget&, int index, Part part);
    void drawKeyboardAtRest (juce::Graphics&, const GuiWidget&);

    /** Play mode: the key under the mouse plays (the one before stops); -1 lets go. */
    void playKey (int widgetIndex, juce::Point<float> plugin);
    void releaseKey();

    /** A try of "Apply this knob on similars": whose look, and what each other control wore. */
    struct SimilarsTry
    {
        int widget = -1;
        juce::String look;
        std::vector<std::pair<int, juce::String>> before;   // a control's index, and its style before
    };

    SimilarsTry similarsTry;
    void putSimilarsBack();   // the try undone: each control wears what it wore

    /** The look most of the window's controls of this kind wear, for a new one (none: the default). */
    juce::String commonLook (const GuiWidget& widget) const;
    void edited();
    int editStamp = 0;   // counts the edits (the Showroom prints the panel again after one)
    void reportSources();

    /** Adds an element: its parameter, caption and the like filled in where they're missing.
        Returns its index. */
    int addElement (GuiWidget w, std::optional<juce::Point<float>> centre);
    void removeSelected();

    /** Ctrl+M: the shapes among these merged into one (Photoshop's Merge Shapes: ShapeMerge),
        in the top one's layer and look, selected. Two at least. */
    void mergeShapes (const std::vector<int>& indexes);

    /** An element's box centred on a point (plugin pixels), snapped, inside the window. */
    juce::Rectangle<int> placed (juce::Rectangle<int> bounds, juce::Point<float> centre) const;

    /** Asks for a picture and copies it into the project: for an element, or (-1) the background. */
    void choosePicture (int widgetIndex);
    void usePicture (int widgetIndex, const juce::String& name);
    const juce::Image& picture (const juce::String& name);
    juce::File imagesFolder() const;

    //==========================================================================
    // The view (Colosseum's rack): plugin pixel p shows at (p - pan) * zoom.
    juce::Rectangle<float> panelArea() const;   // where the plugin window shows
    juce::Point<float> toPlugin (juce::Point<float> view) const;
    juce::Rectangle<float> toView (juce::Rectangle<int> plugin) const;
    juce::Rectangle<float> worldBounds() const;  // how far the view can go, in plugin pixels
    juce::Rectangle<float> viewArea() const;     // the canvas minus the scrollbars
    void clampPan();
    void centreOn (juce::Point<float> plugin);
    void followPosition();                       // the window stays put when the canvas's left edge moves
    void viewChanged();
    void loadView();
    void saveViewSoon();

    juce::Rectangle<float> minimapArea() const;
    juce::Rectangle<float> hScrollArea() const;
    juce::Rectangle<float> vScrollArea() const;
    juce::Rectangle<float> hThumb() const;
    juce::Rectangle<float> vThumb() const;
    juce::Rectangle<float> panelHandle() const;  // the window's own resize corner

    /** The crop tool: a box in plugin pixels over the window. Its handles are numbered as an
        element's (0 top-left round to 7 left); 8 is inside it (moves it), 9 a new box drawn
        from outside it. Applied, the window takes its size, everything moves with its
        top-left corner, a background picture fills the new window (painted ones are painted
        again at its size), and what lies wholly outside it is deleted. */
    bool cropping = false;
    juce::Rectangle<int> cropBox, cropStart;
    int cropHandle = -1;
    juce::TextButton cropButton { "Crop" }, cropApplyButton { "Apply" }, cropCancelButton { "Cancel" };
    void startCrop();
    void applyCrop();
    void cancelCrop();
    juce::Rectangle<float> cropHandleArea (int handle) const;
    int cropHandleAt (juce::Point<float> view) const;     // -1: none (8: inside the box)
    void dragCrop (juce::Point<float> delta, bool keepShape);
    void drawCrop (juce::Graphics&);
    void fitPaintedBackdrops();                            // painted backgrounds painted again at the window's size

    /** The selected element's handles: 0 top-left, 1 top, 2 top-right, 3 right, 4 bottom-right,
        5 bottom, 6 bottom-left, 7 left resize it (knobs, which stay round, have the corners
        only; a small element, its corners only; a line, none: its points shape it); 8 turns it
        (the round one above it); 9 is a shape's own (see ObjectPaint::adjust: a line's bend
        point); 10 and 11 a line's ends. All turn with it. Empty: no such handle. */
    static constexpr int turnHandle = 8, shapeHandle = 9, lineStartHandle = 10, lineEndHandle = 11,
                         cornerHandle = 12,   // ...to 15: a shape's corners, per corner (top-left, top-right, bottom-right, bottom-left)
                         bendHandle = 16,     // ...to 31: a line's bend points (ObjectPaint::maxBends of them)
                         maxBendHandles = 16,
                         numHandles = bendHandle + maxBendHandles;
    static bool isCornerHandle (int handle) noexcept    { return handle >= cornerHandle && handle < cornerHandle + 4; }
    static bool isBendHandle (int handle) noexcept      { return handle >= bendHandle && handle < bendHandle + maxBendHandles; }

    /** Whether the element at an index is a line (a shape of kind "line"). */
    bool isLine (int index) const;
    juce::Rectangle<float> handleArea (int handle) const;
    juce::Point<float> handleCentre (int handle) const;   // in view pixels (none: off the canvas)
    int handleAt (juce::Point<float> view) const;   // -1: none

    // The "E" under the selected element's box: its studio (the Selector, Performance, Matrix,
    // Preset or Knob Studio), when it has one.
    bool hasStudio (const GuiWidget& w) const;
    juce::Rectangle<float> editTabArea() const;   // in view pixels; empty: none
    void openStudio (int widgetIndex);

    /** An element's turn as the view shows it (about its middle there), and its outline there. */
    juce::AffineTransform viewTurn (const GuiWidget& w) const;
    juce::Path viewOutline (const GuiWidget& w, float grow) const;
    juce::Rectangle<int> viewAreaOf (const GuiWidget& w) const;   // what it covers in the view, turned: what to repaint

    /** A point of the window in an element's own unturned frame (where it's played and picked). */
    static juce::Point<float> intoWidget (const GuiWidget& w, juce::Point<float> plugin)    { return w.unturned (plugin); }

    /** Element indexes moved (added, deleted, reordered): what's kept by index starts afresh. */
    void indexesChanged();

    //==========================================================================
    // Pages (GuiLayout::pages): one shows at a time, in Edit and in Play; a tabs element
    // switches them, and so does the strip of page tabs above the window in Edit.
    int shownPage = 0;
    juce::String shownPageName() const     { return layout.pages.isEmpty() ? juce::String() : layout.pages[juce::jlimit (0, layout.pages.size() - 1, shownPage)]; }
    bool isShown (const GuiWidget& w) const { return layout.isOnPage (w, shownPageName()); }
    void showPage (int index);

    /** A command button's command (GuiWidget::command): the preset manager's Save as, Load or
        Save in place. */
    void runCommand (const juce::String& command);

    /** A new page, shown. The first time (no pages yet) there are two: "Main" with the window's
        controls on it, and the new one; withTabs: and a tabs element to switch them if there's none. */
    void addPage (bool withTabs = true);
    void renamePage (int index, const juce::String& name);
    void removePage (int index);       // its elements go to the page before it (or every page when none is left)
    void movePage (int index, int delta);
    void askToRenamePage (int index);

    /** The window menu's list of pages, in order: renamed where a name changed, added, removed. */
    void setPages (const juce::StringArray& names);

    /** The strip above the window in Edit: a tab per page, then "+". Index: a page; -1 "+". */
    std::vector<std::pair<int, juce::Rectangle<float>>> pageStrip() const;   // -1 "+", -2 X rulers, -3 Y rulers, -4 distances
    void drawPageStrip (juce::Graphics&);
    void pageStripClicked (int item, const juce::MouseEvent&);

    // Rulers (Edit, ticked above the window): lines along the chosen element's top and bottom
    // (X rulers) and its left and right (Y rulers), across the whole window, for placing
    // things exactly; lit where another element's edge lies on one.
    bool xRulers = false, yRulers = false;
    bool distances = false;              // the chosen control's distance from the 3D body's nearest edges (red arrows)
    bool bodyCentredX = false, bodyCentredY = false;   // the body, moved, on the window's middle (pink lines)
    void loadRulers();
    void saveRulers() const;
    void drawRulers (juce::Graphics&);

    /** A tabs element's tab at a point (in its own frame); -1: none. */
    int tabAt (const GuiWidget& tabs, juce::Point<float> local) const;

    //==========================================================================
    // Layers: the layout's order, the first element at the back.
    enum class Order { front, forward, backward, back, behindBody, overBody };
    void reorder (int index, Order order);

    /** Where forward (up) or backward one layer takes an element: past the next layer of the
        elements that overlap it (side by side ones are one layer: a row of buttons), not just
        one place in the order. -1: nothing overlaps it that way. */
    int layerStep (int index, bool up) const;

    // Several selected at once: `selected` is the one the mouse last picked, `manySelected` all of
    // them (empty when one or none is selected).
    std::vector<int> selection() const;                            // every selected element, back to front
    bool isSelected (int index) const;
    void toggleSelection (int index);                              // Shift+click
    void selectMany (const std::set<int>& indexes, int primary);   // one or none: a plain selection
    void reorderSelection (bool toFront);                          // all of them, keeping their order

    // Copy and paste: elements as JSON on the system clipboard.
    // Copy style and Paste style (Ctrl+Alt+C, Ctrl+Alt+V): how one element looks onto others,
    // their place, size, words and what they work kept (see applyStyle).
    void copyStyle (int index);
    void pasteStyle (const std::vector<int>& indexes);
    bool hasCopiedStyle() const;
    juce::String copiedStyleKind() const;                          // "knob", "label"... (empty: none)

    /** Its look onto `to`: colours, text style, font, opacity, and what its kind has of its own
        (a knob's look onto a knob, a shape's material and layer style onto a shape...). Its
        place, size, turn, words, parameter, picture, page and form stay its own. */
    static void applyStyle (const GuiWidget& from, GuiWidget& to);

    void copyElements (const std::vector<int>& indexes);           // frames with what's inside them
    void copyElement (int index)                                   { copyElements ({ index }); }
    std::vector<GuiWidget> clipboardElements() const;              // empty: nothing of ours there
    void pasteElements (std::optional<juce::Point<float>> at);     // at: where their top-left goes (plugin
                                                                   // pixels); none: a step beside the copies
    int pasteCount = 0;                                            // pastes since the copy, each a step further
    std::optional<juce::Point<float>> pastePoint;                  // where the menu was opened, for its Paste

    /** A picture covering the whole window: the background. */
    bool isBackdrop (const GuiWidget& w) const;

    /** The background picture that shows at the back (the last of those with only frames and
        backgrounds under them), or -1: a new frame goes just above it, a new background takes
        its place. */
    int backBackdrop() const;
    void navigateMinimapTo (juce::Point<float> view);
    void drawGrid (juce::Graphics&);
    void drawMinimap (juce::Graphics&);
    void drawScrollbars (juce::Graphics&);
    void drawDropGhost (juce::Graphics&);
    void showDropGhost (const juce::String& primitiveId, juce::Point<float> view);
    void repaintDropGhost();

    //==========================================================================
    // Selection and the edit menu
    void select (int index);                     // -1: nothing
    void selectPanel();
    void openMenu();                             // for what's selected
    void closeMenu();
    void fitMenuWindow (bool place);             // the edit window the menu's size (and placed beside what it edits)
    void followMenu();                           // what's dragged: its open menu shows where it is now

    /** The look a control wore before the user first tried another (see GuiWidget::originalStyle). */
    static void rememberOriginalLook (GuiWidget& w)
    {
        if (! w.originalKnown)
        {
            w.originalKnown = true;
            w.originalStyle = w.style;
        }
    }
    void layOutOverlays();
    void updateZoomControls();

    //==========================================================================
    GuiLayout layout;
    juce::Array<Param> params;
    std::map<int, float> values;                                   // by parameter index
    Looks* looks = nullptr;
    juce::StringArray signalSources, displaySources;
    PresetMenu presetMenu;
    int presetVersion = 0;                                         // counts the preset menu's changes (programmed elements follow it)

    // Painted backgrounds (PanelPainter): their material, colours and patterns from the Edit tab.
    juce::var paintedRecipeOf (const GuiWidget& w) const;   // a painted picture's recipe; void if it isn't one
    void repaintPainted (const juce::var& recipe);          // paints it again (the newest asked for wins)
    void paintNewBackground();                              // a lit panel behind everything, to shape in its menu
    void pictureRepainted (const juce::String& fileName);
    bool paintingPicture = false;
    std::map<juce::String, juce::var> pendingRecipes;              // by file: the newest change asked for while one paints
    int paintGeneration = 0;                                       // counts project switches: a paint asked for in the project before is let go

    // The global light: shown at once (looks, shapes); kept, it repaints painted backgrounds too.
    void setGlobalLight (float angle, float altitude);
    void repaintBackgrounds();
    bool lightKnown = false;                                       // the project's light was seen (a new one in a new layout: repaint)

    int heldNote = -1, heldKeyboard = -1;                          // the key the mouse holds down, and on which keyboard
    std::bitset<128> notesShown;                                   // the notes the keyboards show as down

    std::shared_ptr<WasmElements> elements;
    std::unique_ptr<ElementBridge> elementBridge;
    std::map<int, ElementView> elementViews;                       // by widget index
    juce::String elementsProblem;                                  // why the elements stopped
    bool pressedPopup = false;                                     // the press went to a popup
    int commandHeld = -1;                                          // the command button held down (lit; it runs on release)
    int hoveredElement = -1;                                       // the element (or popup) the mouse is over
    bool hoveredPopup = false;

    // Shapes painted pixel by pixel (a material, patterns, relief, a shadow): kept by element until
    // they change; big ones are painted on a thread of their own while the last one stands in.
    struct ShapePainting
    {
        ObjectPaint::Painting painting;
        juce::String pending;                                      // the key being painted on the thread
    };

    static constexpr int shapePixelsNow = 360000;                  // bigger ones go to the thread
    std::map<int, ShapePainting> shapePaintings;                   // by widget index
    std::unique_ptr<juce::ThreadPool> shapePainter;

    // The window's 3D body (GuiLayout::body): rendered on a thread of its own (quickly first,
    // then sharp), kept until it changes; Export and pictures of the panel wait for it.
    juce::Image bodyImage;
    juce::String bodyKey, bodyPending;                             // what bodyImage shows, what's being rendered
    bool bodySharp = false;                                        // bodyImage is the sharp one, not the quick one
    std::unique_ptr<juce::ThreadPool> bodyPainter;
    std::shared_ptr<std::atomic<int>> bodyGeneration = std::make_shared<std::atomic<int>> (0);   // the newest body asked for: older jobs stop
    juce::String bodyKeyNow() const;                               // what the body looks like now ("" none)

    // ...its shape, for what stands on it: a control on a sloped face is seen as that face
    // turns it (Render3D::bodyFace), when the body's "follow" is on (the standard).
    std::shared_ptr<const Body3D> bodyShape;
    juce::String bodyShapeKey;                                     // what bodyShape was built from
    juce::String bodyShapeKeyNow() const;
    std::map<juce::int64, juce::Point<float>> faceSlopes;          // by a control's middle
    juce::Point<float> faceSlope (juce::Rectangle<int> area);

    // The Body editor, and the part it has chosen outlined on the panel (plugin pixels).
    std::unique_ptr<juce::DocumentWindow> bodyEditorWindow;
    juce::Path bodyHighlight;

    // How many keys a selector has.
    int selectorKeys (const GuiWidget& selector) const;

    // The wheel of a pair being moved; let go: a pitch bend goes back.
    int performPart = 0;
    void releasePerform (int widgetIndex);

    // A matrix's routes as its parameters have them; a press, a drag (an amount, a cable
    // being plugged), a release and the wheel on one.
    ModMatrix::View matrixView (const GuiWidget& matrix) const;
    void setMatrixRoute (const GuiWidget& matrix, int route, int source, int destination, float amount);   // -1 (amount: below -2): unchanged

    struct MatrixDrag
    {
        int widget = -1;
        ModMatrix::Hit hit;
        juce::Point<float> start, to;   // plugin pixels
        float startAmount = 0;
        bool moved = false, remove = false;
        int from = -1;                   // a cable from this output
        int source = 0;                  // a part's cable: from this source (1..), its fixed end there
        juce::Point<float> anchor;
        float amount = 0.5f;             // ...the amount it's plugged in with
    };

    MatrixDrag matrixDrag;
    void matrixPress (int widgetIndex, juce::Point<float> plugin, bool popupMenu);
    void matrixDragTo (juce::Point<float> plugin);
    void matrixRelease();
    void matrixWheel (int widgetIndex, juce::Point<float> plugin, float delta);

    // Each preset manager's page, category tab, search (being typed) and, a side panel that
    // opens and closes, whether it's open; the one whose list or table is open; a panel with
    // no presets of its own: the made-up ones, and which is chosen.
    struct PresetState { int page = 0, tab = 0; juce::String search; bool searching = false, sideOpen = true; };
    std::map<int, PresetState> presetStates;
    int presetPopup = -1;
    int demoPreset = 0;

    PresetState& presetState (int widgetIndex);
    PresetPanel::View presetView (int widgetIndex) const;
    bool sideOpen (int widgetIndex) const;
    void syncSideModules();                                       // side panels where they join the window
    juce::Rectangle<float> windowArea() const;                    // windowBounds() in the view
    juce::Rectangle<float> presetPopupArea (int widgetIndex) const;
    void drawPresetWidget (juce::Graphics& g, int widgetIndex);
    void drawPresetOverlay (juce::Graphics& g);                   // the side panels' handles, the open list or table
    bool presetPress (juce::Point<float> plugin, bool popupMenu);  // the open list or table, a handle: taken
    void presetWidgetPress (int widgetIndex, juce::Point<float> plugin, bool popupMenu);
    void presetBodyHit (int widgetIndex, const PresetPanel::Hit& hit, juce::Rectangle<float> area);
    bool presetWheel (juce::Point<float> plugin, float delta);
    bool presetKey (const juce::KeyPress& key);
    void stepPreset (int delta);
    void choosePreset (int index);
    void presetAction (int action);                               // 0 Save, 1 Save as, 2 Load, 3 More (its menu)
    void showPresetMenu (juce::Rectangle<float> pluginArea);      // the classic menu: the presets, then the actions
    void endPresetSearch();

    // A matrix's parts lying anywhere on the panel (ModMatrix::isPart: jacks, source chips,
    // assign buttons) share the parameters of the matrix they belong to (the same <param>):
    // its home is the whole matrix (or, with none, the first part). The cables between them
    // run across the whole window; a control modulated wears a ring for its amount.
    static juce::String matrixBase (const GuiWidget& matrix);
    std::vector<int> matrixHomes() const;                        // a home per <param>
    int matrixHomeOf (const GuiWidget& matrix) const;
    juce::StringArray matrixDestinationIds() const;             // the destinations' parameter ids, in order (1..)
    int destinationOf (const GuiWidget& control) const;         // its destination number; 0: none

    struct Modulation { int home = -1, route = -1, source = 0; float amount = 0; juce::Colour colour; };
    std::vector<Modulation> modulationsOf (int destination) const;
    std::map<int, std::vector<Modulation>> allModulations() const;   // by destination

    /** Where a cable from a matrix of this <param> plugs in at a point: its destination
        number (0: nowhere), and the control taking it (-1: a jack). */
    int modTargetAt (juce::Point<float> plugin, const juce::String& base, int* control = nullptr) const;
    bool routeSource (int home, int source, int destination, float amount);   // a free route (or the same one again)
    float newAmountOf (const GuiWidget& matrix) const;

    void drawMatrixPart (juce::Graphics& g, int widgetIndex);
    void drawMatrixOverlay (juce::Graphics& g);

    // Compact assign: an assign button on (or a chip clicked), the controls clicked are routed
    // from its source; a control's amount dragged (Alt + drag, or a drag right after assigning).
    struct Assigning { int widget = -1, source = 0; };
    Assigning assigning;
    struct AmountDrag { int home = -1, route = -1; float startAmount = 0, startY = 0; };
    AmountDrag amountDrag;
    void partPress (int widgetIndex, juce::Point<float> plugin, bool popupMenu);
    void partRelease();
    juce::Point<float> sourceEnd (const juce::String& base, int source, juce::Point<float> fallback) const;
    bool modulationPress (int controlIndex, juce::Point<float> plugin, const juce::ModifierKeys& mods);
    bool modulationWheel (int controlIndex, float delta);
    void endAssigning();

    std::map<int, float> meterLevels;                              // by widget: shown level, with fall-back
    std::map<int, float> meterNeedles;                             // ...a VU's needle, swinging after it (0..1 of the meter)
    std::map<int, std::pair<float, double>> meterPeaks;            // ...the peak held (0..1) and until when (ms)
    std::map<juce::String, float> frameLevels;                     // read once per frame per source
    juce::StringArray reportedLevels, reportedScopes;
    std::vector<float> scopeBuffer;

    juce::File guiFolder;
    std::map<juce::String, juce::Image> pictures;                  // by file name in images/
    std::map<juce::String, juce::Time> pictureTimes;               // when each file was read: a newer file is read again
    std::unique_ptr<juce::FileChooser> pictureChooser;

    bool design = true, baking = false;
    bool panelOnly = false;   // a made plugin's window in Sonify: the panel alone
    bool registered = false;  // ...registered on this computer: its registration badge hides
    int selected = -1, active = -1, hovered = -1;
    std::set<int> manySelected;                                          // all selected, when more than one
    juce::Rectangle<int> marquee;                                  // Shift+drag's box, in plugin pixels
    bool panelSelected = false, menuOpen = false;

    // The 3D body, selected as a thing of its own (a click on it where nothing stands): a box
    // round it with eight handles, dragged to move it, its handles to resize it (its parts, and
    // the middle its mirrored parts are mirrored across, moved and scaled into the new box).
    // While it's dragged, and until its new picture's made, the last picture stands in,
    // stretched from where it was to where it is (bodyImageMap: from, to).
    bool bodySelected = false, pressedBody = false;
    juce::Rectangle<float> bodyStartRect, bodyDragRect;
    std::optional<std::pair<juce::Rectangle<float>, juce::Rectangle<float>>> bodyImageMap;
    juce::Rectangle<float> bodyBounds() const;                     // on the window (none: no body)
    juce::Rectangle<float> bodyHandleArea (int handle) const;     // 0..7 clockwise from top-left, in the view
    int bodyHandleAt (juce::Point<float> view) const;              // -1: none
    void selectBody();
    void dragBody (juce::Point<float> delta);
    void dropBody();                                               // a drag let go: the parts moved into the new box

    /** A body's parts moved and scaled from one box on the window into another. */
    static juce::var bodyInto (const juce::var& body, int windowWidth, int windowHeight, juce::Rectangle<float> from, juce::Rectangle<float> to);

    // What a press is doing.
    // A line: its end dragged (lineEnd, resizeHandle says which), or the line pulled where it was
    // grabbed (lineBend, at lineGrab along it); a right-press on it moves it, its menu opening
    // on release when it wasn't dragged (menuOnRelease).
    // A shape per corner: one corner dragged (corner, resizeHandle says which); its box fits
    // round its corners again when the drag ends. A line's bend point dragged: bendPoint.
    enum class Drag { none, element, resizeElement, turnElement, adjustShape, resizePanel, pan, minimap, hScroll, vScroll, control, marquee, crop,
                      lineEnd, lineBend, corner, bendPoint, moveBody, resizeBody };
    int activeBend = 0;   // the selected line's bend point its menu shows (the last one picked)
    Drag drag = Drag::none;
    float lineGrab = 0.5f;
    bool menuOnRelease = false;
    bool changed = false, dragMoved = false;                       // dragMoved: the press became a drag
    bool pressedPanel = false;                                     // the press started on the window's empty space
    int pressedBackdrop = -1;                                      // ...or on the background picture, not selected yet
    int resizeHandle = -1;                                         // the handle being dragged (see handleArea)
    bool reopenMenu = false;                                       // the menu was open when the press started
    juce::Point<float> dragStart;
    juce::Point<float> panAtDragStart;
    float startProportion = 0.0f;
    juce::Rectangle<int> startBounds;
    GuiWidget startWidget;                                         // the element as it was when a turn or a shape's handle began
    juce::Point<int> startPanelSize;
    std::vector<std::pair<int, juce::Rectangle<int>>> carried;     // a group's contents, while it moves

    // A frame resized: what lies inside it as it was, scaled with it (its shape kept: the
    // FlowStone view area's way), and whether the 3D body lies inside it (moved or scaled with
    // it, its last picture stretched meanwhile: bodyStartRect, bodyDragRect). A frame's
    // "scaleContents" setting (on unless it's false) says whether it scales them.
    std::vector<std::pair<int, GuiWidget>> frameContents;
    bool bodyInFrame = false;
    static bool scalesContents (const GuiWidget& frame);
    bool frameHoldsBody (juce::Rectangle<int> frame) const;
    static GuiWidget scaledWidget (const GuiWidget& w, juce::Rectangle<float> from, juce::Rectangle<float> to);

    /** A frame that scales what's inside it, given a new size by number: at once, its shape
        kept (the size that changed wins), from its top-left corner. */
    void scaleFrameTo (int frame, juce::Rectangle<int> to);

    // The view.
    float zoom = 1.0f;
    juce::Point<float> pan;                                        // the plugin pixel at the view's top-left
    int lastX = 0;                                                 // where the canvas sat in its parent
    bool autoFit = true;               // the view follows the window until the user moves it
    bool viewSaveScheduled = false;

    // Something dragged over the canvas: where it would land, in plugin pixels.
    std::optional<juce::Rectangle<int>> dropGhost;

    /** A Gallery item as one of the plugin's own looks: its name ("" when it can't be read, said). */
    juce::String lookFromGallery (const juce::File& file, Looks::Kind& kind);

    std::unique_ptr<ElementMenu> menu;
    std::unique_ptr<juce::DocumentWindow> menuWindow;   // the menu's window (gone before the menu)
    std::unique_ptr<AiBanner> banner;
    juce::TextButton fitButton { "Fit" };
    FineSlider zoomSlider;
    juce::Label zoomLabel;

    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginCanvas)
};