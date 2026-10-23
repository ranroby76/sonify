// D:\workspace\Fanan_web_hub\src\Looks.h

#pragma once

#include "GuiLayout.h"
#include "LayerDoc.h"

#include <functional>
#include <map>
#include <vector>

//==============================================================================
/**
    How the plugin's knobs, sliders and switches look: KnobMaker layer documents
    (.fklayers), built in the Knob Studio the KnobMan way, one primitive per layer.

    A look's frames run through the control's travel: a knob's turn, a slider from the
    bottom (or the left) to the top (or the right), a switch from off (the first frame) to
    on (the last). The canvas draws a frame for each value, and Export bakes them into the
    strips the finished plugin plays.

    Where looks come from, in the order a name is looked up:
        the plugin's own   gui/looks in the project: every look edited for this plugin
        mine               <user data>\Plugins Creator\Looks: the user's, for every project
        built-in           the looks folder, compiled into the studio
*/
class Looks
{
public:
    /** What a look is made for. */
    enum class Kind { knob, slider, sliderAcross, button };

    static juce::String kindName (Kind kind);              // "knob", "slider", "hslider", "switch"
    static Kind kindFromName (const juce::String& name);
    static juce::String kindDisplayName (Kind kind);       // "knob", "slider", "slider across", "switch"

    /** Knobs, sliders and switches take a look; nothing else does. */
    static bool takesLook (const GuiWidget& widget);
    static Kind kindOf (const GuiWidget& widget);

    enum class Origin { builtIn, mine, project };

    struct Look
    {
        juce::String name, description;
        Kind kind = Kind::knob;
        Origin origin = Origin::builtIn;
        int order = 1000;
        bool pool = false;    // one of Fanan AI's inspiration pool (the inspiration-looks skill describes it)
        LayerDoc doc;
        int revision = 0;     // new on every change, so its drawings are made again
    };

    Looks();

    /** The open project's gui folder (none: no project): its looks are in gui/looks. */
    void setGuiFolder (const juce::File& guiFolder);
    bool isProjectOpen() const noexcept    { return guiFolder != juce::File(); }

    /** The plugin's own looks and mine, read again (after an undo, or a change on disk). */
    void reload();

    /** Exactly that name: the plugin's own first, then mine, then the built-in ones. */
    const Look* find (const juce::String& name) const;

    /** That name among one kind of look only (a built-in look the plugin's own one hides). */
    const Look* find (const juce::String& name, Origin origin) const;

    /** What a control shows: its look, else a look of its kind with a close name ("black"
        finds "Black knob"), else its kind's default. */
    const Look& lookFor (const GuiWidget& widget) const;
    const Look& defaultFor (Kind kind) const;

    /** Every look of a kind: the plugin's own, then mine, then the built-in ones (a name
        once: the plugin's own hides a built-in one of the same name). */
    std::vector<const Look*> listFor (Kind kind) const;
    std::vector<const Look*> all() const;

    /** The looks of one origin and kind, hidden ones too. */
    std::vector<const Look*> listFrom (Origin origin, Kind kind) const;

    //==========================================================================
    /** The Knob Studio's edits: saved as the plugin's own look. A built-in look (or one of
        mine) with the same name is set aside for this plugin from then on, and its
        description stays unless a new one is given. */
    void setProjectLook (const juce::String& name, Kind kind, const LayerDoc& doc, const juce::String& description = {});
    bool removeProjectLook (const juce::String& name);

    /** A name none of the plugin's own looks has, nor any in alsoTaken (the looks its
        controls wear, say): the base itself if it's free, else "Black knob 2". */
    juce::String freeName (const juce::String& base, const juce::StringArray& alsoTaken = {}) const;

    /** Copies a look into my looks, for every project. */
    bool saveToMine (const juce::String& name);

    /** Looks the layout uses that are only in my looks are copied into the plugin, so it
        opens the same on any computer. True when one was. */
    bool adoptMine (const GuiLayout& layout);

    //==========================================================================
    /** The panel's global light (GuiLayout's): every look is drawn lit from there, its own
        light turned to it (bevels on the global light take its height too). */
    void setLight (float angle, float altitude);

    /** A look's document lit by the panel's global light. */
    LayerDoc litBy (const LayerDoc& doc) const;

    /** The frames a control's strip has: the look's own; two for a switch (off, on). */
    static int framesFor (const Look& look, Kind kind);

    /** A frame of a look for a value (0..1; a switch: 0 off, 1 on), fitted in proportion
        into width x height and centred, with overhang (below) more on every side for its
        shadows and glows: draw it over the box expanded by that much. resolution: pixels per
        plugin pixel (a zoomed-in view asks for more). Kept until the look changes. */
    juce::Image frame (const Look& look, Kind kind, int width, int height, float value, float resolution = 1.0f);

    /** How far a look's drop shadows, glows and outside outlines reach past a width x height
        box, in plugin pixels: they fall on the panel around the control, never cut off at
        its box (0: it has none). */
    static int overhang (const Look& look, int width, int height);

    /** For Export: every frame, stacked top to bottom, each the box plus its overhang on
        every side. */
    juce::Image strip (const Look& look, Kind kind, int width, int height);

    /** A shape in the Shapes inventory was saved or deleted: the looks whose Shape layers draw
        from the inventory are drawn again (a new revision each, and onChanged). */
    void shapesChanged();

    /** A look was added, changed or removed. */
    std::function<void()> onChanged;

    //==========================================================================
    static constexpr const char* folderName = "looks";        // in the project's gui folder
    static constexpr const char* fileExtension = ".fklayers";
    static juce::File myLooksFolder();

    /** A .fklayers file: KnobMaker's layer document, plus what Stella knows about it
        (its name as given, kind, order, description, whether it's of the inspiration pool)
        as attributes KnobMaker leaves alone. */
    static bool readFile (const juce::File& file, Look& look);
    static bool writeFile (const juce::File& file, const Look& look);
    static bool readXml (const juce::String& xml, const juce::String& name, Look& look);

private:
    juce::File projectFolder() const;
    void readFolder (const juce::File& folder, Origin origin, std::map<juce::String, Look>& into);
    const Look* findIn (const std::map<juce::String, Look>& looks, const juce::String& name) const;
    void changed();

    juce::File guiFolder;
    std::map<juce::String, Look> builtIn, mine, project;
    std::map<int, Look> fallbacks;                    // by kind: when the built-in ones are missing
    std::map<int, std::map<juce::String, juce::Image>> frames;   // drawn frames: by look (its revision), then size, value and light
    int framesDrawn = 0;
    float lightAngle = 120.0f, lightAltitude = 30.0f;            // the panel's global light

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Looks)
};
