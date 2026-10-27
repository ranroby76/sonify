// D:\workspace\Fanan_web_hub\src\KnobStudio.h

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "GuiLayout.h"
#include "LayerWorkspace.h"
#include "Looks.h"
#include "ShapeWorkspace.h"

#include <functional>

//==============================================================================
/**
    The Knob Studio tab: KnobMaker's layer builder, the KnobMan way, working on the looks of
    the plugin's knobs, sliders and switches.

        header   the look being edited and how many controls wear it; Use on this knob,
                 Give this knob its own look, Back to the plugin
        left     the plugin's looks (what its controls wear, then its own), with New look,
                 Duplicate, Save to my looks and Delete
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

    /** KnobMaker: opens a knob from a file, a Sonify knob (.fklayers) or a KnobMan one (.knob). */
    void openKnobFile (const juce::File& file);

    //==========================================================================
    /** The Gallery (every knob, slider and switch in a folder, as pictures; at first the KnobMan
        gallery's downloads) in a window of its own, beside near. A double-click (or Open) gives
        onPick the file; closeOnPick: the window goes then (one pick), else it stays (several).
        dragOut: its items can also be dragged out onto Edit UI's window (galleryDrag). */
    static void showGallery (const juce::String& title, std::function<void (const juce::File&)> onPick,
                             bool closeOnPick, bool dragOut, juce::Component* near = nullptr);

    /** A gallery item dragged out: its drag description, and the file back from one ("" when it isn't). */
    static juce::var galleryDrag (const juce::File& file);
    static juce::File galleryDragFile (const juce::var& description);

    /** A gallery file (a KnobMan .knob or a Sonify .fklayers) as a look: its drawing, what it's
        for, and its credits (a KnobMan gallery download's author and licence). commercialSafe:
        whether its licence allows it in a plugin that's sold. False, with error, when it can't be read. */
    static bool readGalleryItem (const juce::File& file, LayerDoc& doc, Looks::Kind& kind,
                                 juce::String& credits, bool& commercialSafe, juce::String& error);

    std::function<void (int widgetIndex, const juce::String& lookName)> onUseLook;   // a control takes a look
    std::function<juce::String (int widgetIndex)> onGiveOwnLook;                       // returns the new look's name
    std::function<void()> onBack;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;

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
    void addLook (const juce::String& baseName, Looks::Kind kind, const LayerDoc& doc, const juce::String& description = {});
    void openLookFile();
    void importKnobFolder();
    void showExportMenu();
    void exportLookFile();
    void exportFilmstrip();
    juce::String ownerName() const;   // "this plugin", or "your library" (the workbench)
    void status (const juce::String& text);

    // KnobMaker's menu (the workbench).
    void workbenchLoad (const LayerDoc& doc, Looks::Kind kind, const juce::String& title);
    void saveKnobFile();
    void chooseKnobFile (bool knobMan);
    void openGallery();

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
    juce::TextButton exportButton { juce::String::fromUTF8 ("Export\xe2\x80\xa6") };
    juce::TextButton useButton, uniqueButton, backButton { "Back to the plugin" };

    // KnobMaker's menu, in place of the list of looks.
    juce::TextButton saveKnobButton { juce::String::fromUTF8 ("Save as Sonify knob\xe2\x80\xa6") };
    juce::TextButton exportPngButton { juce::String::fromUTF8 ("Export to PNG\xe2\x80\xa6") };
    juce::TextButton loadKnobButton { juce::String::fromUTF8 ("Load Sonify knob\xe2\x80\xa6") };
    juce::TextButton loadKnobManButton { juce::String::fromUTF8 ("Load KnobMan knob\xe2\x80\xa6") };
    juce::TextButton galleryButton { juce::String::fromUTF8 ("Gallery\xe2\x80\xa6") };
    juce::String knobTitle { "New knob" };   // the knob's file name
    juce::File knobFolder;                   // where the last one was loaded or saved

    /** Looks and Shapes; the Shapes tab reads the inventory again when it's shown. */
    struct Tabs final : public juce::TabbedComponent
    {
        Tabs() : juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop) {}
        std::function<void (int)> onChange;
        void currentTabChanged (int index, const juce::String&) override   { if (onChange != nullptr) onChange (index); }
    };

    LayerWorkspace layers;
    ShapeWorkspace shapes;
    Tabs tabs;
    juce::Rectangle<int> headerText, statusArea;
    juce::String statusText;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobStudio)
};
