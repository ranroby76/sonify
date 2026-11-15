// D:\workspace\Fanan_web_hub\src\KnobStudio.h

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "GuiLayout.h"
#include "LayerWorkspace.h"
#include "Looks.h"
#include "ShapeWorkspace.h"
#include "Workspace3D.h"

#include <functional>

//==============================================================================
/**
    The Knob Studio tab: KnobMaker's layer builder, the KnobMan way, working on the looks of
    the plugin's knobs, sliders and switches.

        header   the look being edited and how many controls wear it; Use on this knob,
                 Give this knob its own look, Back to the plugin
        left     KnobMaker's buttons on the look being designed: Save as Sonify knob, Export to
                 PNG, Load Sonify knob, Load KnobMan knob (a loaded knob replaces the look) and
                 Select from Gallery (Sonify's gallery, in a window that stays open: a knob
                 double-clicked there, or dragged here, replaces the look; a look dragged from
                 the list into it is kept there). Then the plugin's looks (what its controls
                 wear, then its own), with New look, Duplicate, Save to my looks and Delete
        right    Looks: the layer builder (the preview, its frames, the layers, each layer's
                 shape and effects, Bevel & Emboss, textures). Shapes: the shape inventory
                 Shape layers draw from

    It starts empty: nothing is loaded until a control's look is opened from Edit UI (its
    menu's "Design it in the Knob Studio"), or a look is picked from the list.

    Every change is saved into the plugin's own looks at once and shows on the plugin. A
    built-in look (or one of mine) becomes the plugin's own the moment it's changed. Opened
    from a control, the studio shows that control's look, and a look made here goes onto it:
    on that control alone (its first change gives it a look of its own when other controls
    wear the same one), or, with "Include similars", on every control like it (the same kind,
    wearing the same look) at once.
*/
class KnobStudio final : public juce::Component,
                         public juce::DragAndDropTarget,
                         private juce::ListBoxModel
{
public:
    explicit KnobStudio (Looks& looksToEdit);
    ~KnobStudio() override;

    /** Which looks the plugin's controls wear, and how many wear each. */
    void setLayout (const GuiLayout& layout);

    /** Opens a control's look (an index into the layout's widgets), for it alone or with every
        control like it (includeSimilars); -1: no control. */
    void editControl (int widgetIndex, bool includeSimilars = false);

    /** Empty again: no look loaded, no control. */
    void clear();

    /** The looks changed: the list again, and the open look if it changed elsewhere. */
    void looksChanged();

    /** KnobMaker: the studio on its own, on one knob at a time rather than a plugin's looks.
        Instead of the list of looks, its menu: Save as Sonify knob (a .fklayers file), Export
        to PNG (its frames as a filmstrip), Load Sonify knob, Load KnobMan knob (a .knob file)
        and Gallery (every knob in a folder, as pictures, to open one). The knob being worked
        on is kept in KnobMaker's library, so it's there again next time. No plugin, no Back. */
    void setWorkbench (bool isWorkbench);

    /** KnobMaster 2: the 3D Studio (its own tab): the same studio on the plugin's 3D looks
        (knobs, buttons, sliders and toggle switches built as objects, Workspace3D) instead of
        its looks of layers. A look of the other sort opened (from a control, the Gallery, a
        file) takes the studio there: onModeChanged says so. */
    void setMode3d (bool shouldBe3d);
    bool isMode3d() const noexcept    { return mode3d; }
    std::function<void (bool is3d)> onModeChanged;

    /** A knob from a file, a Sonify knob (.fklayers) or a KnobMan one (.knob): KnobMaker opens
        it; on a plugin it replaces the look being designed (with none open: a new look). */
    void openKnobFile (const juce::File& file);

    //==========================================================================
    /** The Gallery (every knob, slider and switch in a folder, as pictures; at first Sonify's
        gallery) in a window of its own, beside near. A double-click (or Open, or pickText) gives
        onPick the file; closeOnPick: the window goes then (one pick), else it stays (several).
        dragOut: its items can also be dragged out, onto Edit UI's window or the Knob Studio
        (galleryDrag). modal: nothing else in Sonify takes clicks while it's open; else it stays
        beside, Sonify in use too (the Knob Studio's, for dragging both ways). models3d: the 3D
        Gallery instead, the 3D Studio's: 3D models only (.fk3d), at first modelGalleryFolder().
        Returns the window. */
    static juce::ResizableWindow* showGallery (const juce::String& title, std::function<void (const juce::File&)> onPick,
                                               bool closeOnPick, bool dragOut, juce::Component* near = nullptr,
                                               const juce::String& pickText = {}, bool modal = true, bool models3d = false);

    /** The Gallery's knobs, sliders and switches (sort 0), 3D models (1) or kept shapes (2) as a
        component of its own, for the Gallery window (GalleryHub): a double-click (or Add) gives
        onPick the file; knobs and 3D models can also be dragged out onto Edit UI's window. */
    static std::unique_ptr<juce::Component> makeGallery (std::function<void (const juce::File&)> onPick, int sort);

    /** Sonify's gallery: AppData\Fanan\Sonify\Knob Gallery (Library/Fanan/Sonify on a Mac),
        made the first time, the knobs installed with Sonify copied into it (each once). */
    static juce::File galleryFolder();

    /** The 3D Gallery: the project's assets\knob_gallery_3d (else AppData ... Knob Gallery 3D),
        its 3D models (.fk3d: a 3D look's document, as a file of its own to share). */
    static juce::File modelGalleryFolder();

    /** A gallery item dragged out: its drag description, and the file back from one ("" when it isn't). */
    static juce::var galleryDrag (const juce::File& file);
    static juce::File galleryDragFile (const juce::var& description);

    /** A gallery file (a KnobMan .knob or a Sonify .fklayers) as a look: its drawing, what it's
        for, and its credits (what a Sonify knob file says about itself). commercialSafe: false
        only when those credits name a licence that doesn't allow selling. False, with error,
        when it can't be read. */
    static bool readGalleryItem (const juce::File& file, LayerDoc& doc, Looks::Kind& kind,
                                 juce::String& credits, bool& commercialSafe, juce::String& error);

    std::function<void (int widgetIndex, const juce::String& lookName)> onUseLook;   // a control takes a look
    std::function<juce::String (int widgetIndex)> onGiveOwnLook;                       // returns the new look's name
    std::function<void()> onBack;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;   // a look dragged into the Gallery

    void refreshList();
    void select (const juce::String& name);
    void updateButtons();
    int usersOf (const juce::String& name) const;
    const GuiWidget* editedControl() const;
    Looks::Kind kindForNew() const;
    juce::String controlName() const;   // "knob", "slider" or "switch"
    juce::String controlsName() const;  // "this knob", or "these 4 knobs" with the similar ones
    bool needsOwnLook() const;          // a control opened alone wears a look other controls wear too
    void useOnControls (const juce::String& lookName);

    void showNewMenu();
    void showNew3dMenu();
    bool loadIntoBuilder (const LayerDoc& doc);   // into the builder its sort takes (false: the studio changed sort)
    void addLook (const juce::String& baseName, Looks::Kind kind, const LayerDoc& doc, const juce::String& description = {});
    void openLookFile();
    void addLookFromFile (const juce::File& file);      // a .knob or .fklayers file as a new look (New look's Gallery pick)
    void replaceCurrentFrom (const juce::File& file);   // ...or in place of the look being designed
    void builderChanged();                              // the builder's drawing saved as the look (every change)
    void importKnobFolder();
    void exportFilmstrip();                          // Export to PNG: what to export, then where
    void exportPicture (int choice);                 // ...a filmstrip (1, 2: at 2x) or one picture of it (3 at rest, 4 full, 5 halfway, 6 at rest at 2x)
    juce::String ownerName() const;   // "this plugin", or "your library" (the workbench)
    void status (const juce::String& text);

    // KnobMaker's menu (the workbench).
    void workbenchLoad (const LayerDoc& doc, Looks::Kind kind, const juce::String& title);
    void saveKnobFile();
    void chooseKnobFile (bool knobMan);
    void openGallery();
    void closeGallery();     // the one beside the studio (not KnobMaker's modal one)
    void refreshGallery();   // the open Gallery reads its folder again

    // KnobMaster 2's Shapes Gallery (the Knob Studio's, not the 3D Studio's): layers kept as
    // shapes (.fkshapes, assets\shapes_gallery), to put on any look again.
    void openShapesGallery();
    void closeShapesGallery();
    void keepShapes (const juce::File& intoFolder);       // the layers chosen in the builder, named, into it
    void addShapesFromFile (const juce::File& file);      // a kept shape's layers onto the look being designed

    Looks& looks;
    GuiLayout layout;
    juce::StringArray names;     // the list: what the controls wear, then the plugin's own
    juce::String current;
    int loadedRevision = 0;      // the revision of the look showing in the builder
    int editingWidget = -1;
    bool similars = false;              // the controls like it are edited with it
    std::vector<int> similarWidgets;    // ...which are these (itself included)
    bool saving = false;
    bool workbench = false;             // KnobMaker: a library of looks, no plugin
    int shapeRevision = ShapeLibrary::revision();   // the Shapes inventory the looks were last drawn with

    juce::ListBox list { "Looks", this };
    juce::TextButton newButton { "+ New look" }, duplicateButton { "Duplicate" };
    juce::TextButton mineButton { "Save to my looks" }, deleteButton { "Delete" };
    juce::TextButton useButton, uniqueButton, backButton { "Back to the plugin" };

    // KnobMaker's buttons (KnobMaker: in place of the list of looks).
    juce::TextButton saveKnobButton { juce::String::fromUTF8 ("Save as Sonify knob\xe2\x80\xa6") };
    juce::TextButton exportPngButton { juce::String::fromUTF8 ("Export to PNG\xe2\x80\xa6") };
    juce::TextButton loadKnobButton { juce::String::fromUTF8 ("Load Sonify knob\xe2\x80\xa6") };
    juce::TextButton loadKnobManButton { juce::String::fromUTF8 ("Load KnobMan knob\xe2\x80\xa6") };
    juce::TextButton galleryButton { juce::String::fromUTF8 ("Select from Gallery\xe2\x80\xa6") };
    juce::TextButton shapesGalleryButton { juce::String::fromUTF8 ("Shapes Gallery\xe2\x80\xa6") };
    juce::Component::SafePointer<juce::ResizableWindow> shapesWindow;    // the Shapes Gallery, while it's open
    juce::Component::SafePointer<juce::ResizableWindow> galleryWindow;   // the Gallery it opened, while it's open
    bool galleryIs3d = false;                                            // ...the 3D Studio's (3D models only)
    bool dropHover = false;                                          // a gallery knob dragged over it
    juce::String knobTitle { "New knob" };   // the knob's file name
    juce::File knobFolder;                   // where the last one was loaded or saved
    juce::File modelFolder;                  // ...the last 3D model (.fk3d)

    /** Looks and Shapes; the Shapes tab reads the inventory again when it's shown. */
    struct Tabs final : public juce::TabbedComponent
    {
        Tabs() : juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop) {}
        std::function<void (int)> onChange;
        void currentTabChanged (int index, const juce::String&) override   { if (onChange != nullptr) onChange (index); }
    };

    LayerWorkspace layers;
    ShapeWorkspace shapes;
    Workspace3D workspace3d;   // the 3D Studio's builder
    bool mode3d = false;
    Tabs tabs;
    juce::Rectangle<int> headerText, statusArea;
    juce::String statusText;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobStudio)
};
