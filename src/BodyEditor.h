// D:\workspace\Fanan_web_hub\src\BodyEditor.h

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "UiHelpers.h"   // FineSlider

#include "ColourSwatch.h"

class PluginCanvas;
class Showroom;

//==============================================================================
/**
    The Showroom window: the window's 3D body (GuiLayout::body, Body3D) built part by part and
    seen in 3D (Showroom: from anywhere round it, its parts clicked, moved, turned and sized
    with their handles there), in a window of its own that stays open beside the panel, which
    shows every change as it's made.

    Across the top: the Body gallery (templates for a desktop module, a keyboard synth, rack
    units, a Eurorack case, a stompbox, a groovebox, a drum machine..., and the bodies kept
    there), a body file opened or saved (.fkbody), the units (pixels, or millimetres of the real
    instrument: how wide the window is in mm), the grid its sizes and positions snap to, and
    whether the controls standing on a sloped face lean with it.

    On the left the parts, front first like layers (where a part's surface meets another's,
    the one higher up the list shows; a cut takes away from the parts under it), each with its
    eye (hidden for now) and its x (taken away), with the body's own view and light first; in the middle what's chosen, its settings; on the right
    the body in 3D (the Showroom) and, on its second tab, the chosen part's profile (an extrude's or a revolve's outline: its points
    dragged, added with a double-click on a line, taken away with a double-click on one,
    rounded with a right-click). Beside its boxes, cylinders, spheres, extrudes, revolves and
    letters: sweeps (a section along a path: handles, rails, trims), lofts (one profile
    becoming another, tapering, twisting) and models from OBJ and STL files; any part hollow
    (its walls a thickness, a face left open). The Showroom shows the finished instrument from
    round it.

    The editor's own settings (units, grid) are kept in the body's "editor"; its points in a
    part's "points" (its "profile" is written from them).
*/
class BodyEditor final : public juce::Component,
                         private juce::Timer,
                         private juce::ChangeListener
{
public:
    explicit BodyEditor (PluginCanvas& canvas);
    ~BodyEditor() override;

    /** The editor in a window of its own (closing it hides it). */
    static std::unique_ptr<juce::DocumentWindow> makeWindow (PluginCanvas& canvas);

    //==========================================================================
    /** The templates: built for a window width x height. */
    static juce::StringArray templateNames();
    static juce::var makeTemplate (int index, int width, int height);

    /** A body made for one window size, fitted to another: across by the width, back to front
        by the height, heights by the smaller of the two. */
    static juce::var scaledBody (const juce::var& body, int fromWidth, int fromHeight, int toWidth, int toHeight);

    /** The Body gallery's folder (.fkbody files: { "width", "height", "body" }). */
    static juce::File galleryFolder();

    void paint (juce::Graphics&) override;
    void resized() override;

    class PlanView;
    class ProfileEditor;

private:
    class PartsList;
    class Rows;

    PluginCanvas& canvas;

    // The window it builds the body for, and the panel's light.
    int windowWidth() const;
    int windowHeight() const;
    float lightAngle() const;
    float lightAltitude() const;

    //==========================================================================
    // What it edits.
    juce::var& body();
    juce::Array<juce::var>* parts();
    juce::var part() const;                 // the chosen one (void: the body itself)
    int chosen = -1;                        // -1: the body's own settings
    juce::String lastSeen;                  // the body as the editor last left it (JSON)
    bool updating = false;

    void choose (int index);
    void changed (bool final);              // the panel shows it; final: kept (saved, undoable)
    void refresh();                         // everything shown from the body
    void showPart();
    void highlight();                       // the chosen part outlined on the panel
    void timerCallback() override;          // the body changed elsewhere (an undo, Fanan AI)

    void addPart (const juce::String& kind);
    void duplicatePart();
    void deletePart();
    void toggleHidden (int index);           // its eye: left out of the body for now
    void movePart (int by);
    void useBody (const juce::var& newBody, int fromWidth, int fromHeight);
    void openFile();
    void saveFile (bool toGallery);
    void openGallery();

    // Units: pixels or millimetres, and the grid.
    bool millimetres() const;
    float unitsPerPixel() const;            // mm (or 1) a window pixel
    float gridPixels() const;               // the grid in window pixels (0: none)
    void setEditorSetting (const juce::Identifier& key, const juce::var& value);
    void showUnits();

    //==========================================================================
    // Across the top.
    juce::TextButton galleryButton { juce::String::fromUTF8 ("Gallery\xe2\x80\xa6") };
    juce::TextButton openButton { juce::String::fromUTF8 ("Open\xe2\x80\xa6") };
    juce::TextButton saveButton { juce::String::fromUTF8 ("Save as\xe2\x80\xa6") };
    juce::TextButton keepButton { juce::String::fromUTF8 ("Keep in gallery\xe2\x80\xa6") };
    juce::TextButton clearButton { "Clear" };
    juce::ComboBox unitsBox, gridBox;
    juce::TextEditor mmWidthBox;
    juce::ToggleButton snapToggle { "Snap" };
    juce::ToggleButton followToggle { "Controls lean with sloped faces" };
    juce::Label unitsLabel, mmLabel, gridLabel;

    // The parts.
    std::unique_ptr<PartsList> list;
    juce::TextButton addButton { juce::String::fromUTF8 ("Add \xe2\x96\xbe") }, duplicateButton { "Duplicate" }, deleteButton { "Delete" };
    juce::TextButton upButton { "Up" }, downButton { "Down" };

    // The settings.
    std::unique_ptr<Rows> rows;
    juce::Viewport rowsView;

    // The views.
    juce::TabbedButtonBar viewTabs { juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<Showroom> room;   // the body in 3D: seen from anywhere, its parts chosen and moved there
    std::unique_ptr<ProfileEditor> profile;
    void showView();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;   // a view's tab picked

    // The body's own rows.
    FineSlider tiltSlider, shadowSlider, softnessSlider, exposureSlider;
    ColourSwatchButton floorSwatch { "Floor colour" };

    // A part's rows.
    juce::TextEditor nameBox, wordsBox, svgBox;
    juce::ComboBox kindBox, opBox, axisBox, planeBox, repeatBox, materialBox, surfaceBox, fontBox;
    FineSlider xSlider, ySlider, zSlider, wSlider, hSlider, dSlider;
    FineSlider roundSlider, chamferSlider, blendSlider, rotXSlider, rotYSlider, rotZSlider;
    FineSlider countSlider, countZSlider, dxSlider, dzSlider, radiusSlider;
    FineSlider surfaceScaleSlider, surfaceDepthSlider, glassSlider, glowStrengthSlider;
    juce::ToggleButton mirrorToggle { "Mirrored left and right" }, boldToggle { "Bold" };

    // Hollow; a sweep's path and section; a loft's end; a model's file.
    FineSlider shellSlider, thicknessSlider, taperSlider, twistSlider;
    juce::ComboBox openBox, sectionBox, pathPresetBox, fitBox, upBox;
    juce::TextEditor pathBox, endBox;
    juce::Label modelLabel;
    juce::TextButton modelButton { juce::String::fromUTF8 ("Another model\xe2\x80\xa6") };
    void importModel (bool replaceChosen);
    ColourSwatchButton colourSwatch { "Colour" }, glowSwatch { "Glow" };

    void setUpRows();
    void setUpSlider (juce::Slider&, const juce::String& tip, std::function<void (double)> apply, bool length = false);
    void setNumber (const juce::Identifier& key, double pixels);

    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::DocumentWindow> galleryWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BodyEditor)
};
