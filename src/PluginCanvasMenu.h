// D:\workspace\Fanan_web_hub\src\PluginCanvasMenu.h
// The canvas's edit menu, Sonify AI banner and programmed elements' bridge. Only the PluginCanvas files include this.

#pragma once

#include "ColourSwatch.h"
#include "PluginCanvas.h"
#include "stella_element_host.h"

//==============================================================================
/**
    The edit menu, the one menu a right-click opens: beside the element (or the window, for
    its empty space, or the elements selected together). Under its title what it does with
    them: Copy, Paste, Duplicate and the layers (a list of its own); then its settings in tabs
    (Object, Control, Meter, Text, Look, Shape, Surface, Pattern; the window's: Window,
    Background, Pattern), one
    setting a line, each named above it, a line between groups of them. At its foot, an
    instruction for Sonify AI about this very element: "make it gold and a bit bigger".
*/
class PluginCanvas::ElementMenu final : public juce::Component
{
public:
    explicit ElementMenu (PluginCanvas& owner);
    ~ElementMenu() override;

    /** An element (its index), or -1 for the window itself. */
    void showFor (int widgetIndex);

    /** Several elements selected together: what works on them all. */
    void showForSeveral (const std::vector<int>& indexes);

    int getWidgetIndex() const noexcept    { return widget; }

    /** What it edits is being dragged (moved, resized, turned): its position, size and angle
        show where it is now. */
    void followDrag();

    /** Words typed in a box that are used on Enter or on leaving it (a parameter, a size, the
        pages) and aren't used yet: used now, for what the menu shows, before a click on the
        canvas picks something else. (JUCE tells a box it lost the keyboard later, when the
        menu may show something else.) */
    void commitTyped();

    /** Another layout (another project, Sonify AI, an undo): words typed and not used yet are let go. */
    void forgetTyped()    { typedBox = nullptr; }

    int getIdealHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int menuWidth = 330;

private:
    /** The rows of settings with their captions, in tabs; they scroll when the menu is short
        of room. A header row is a group's title over a line: it shows when its group has a row
        showing. */
    class Rows final : public juce::Component
    {
    public:
        struct Row
        {
            juce::String caption;
            juce::Component* first = nullptr;
            juce::Component* second = nullptr;   // beside the first
            float firstShare = 0.5f;
            int height = 26;
            bool shown = true;
            juce::String tab;                    // the tab it's in
            bool surface = false;                // a surface row: in Surface, or the window's Background
            bool header = false;                 // a group's title and line
            int top = 0;                         // where it was laid out (headers)
            bool laidOut = false;
        };

        std::vector<Row> rows;
        juce::String tab;                        // the tab shown

        bool isShowing (size_t index, const juce::String& inTab) const;   // in that tab, and (a header) its group has rows
        int layOut (int width);                  // the tab shown's; returns the height they take
        int measure (const juce::String& inTab) const;   // a tab's height, without moving anything
        void paint (juce::Graphics&) override;
    };

    /** The tabs across the top of the settings. */
    class TabStrip final : public juce::Component
    {
    public:
        juce::StringArray names;
        juce::String current;
        std::function<void (const juce::String&)> onPick;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        std::vector<juce::Rectangle<float>> cells() const;
    };

    /** A line of text in the font chosen, under the Font list. */
    class FontSample final : public juce::Component
    {
    public:
        juce::String family, text;
        void paint (juce::Graphics&) override;
    };

    /** The round send arrow. */
    class SendButton final : public juce::Button
    {
    public:
        SendButton() : juce::Button ("Send") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    void addRow (const juce::String& tab, const juce::String& caption, juce::Component& first, juce::Component* second = nullptr,
                 float firstShare = 1.0f, int height = 26);
    void addHeader (const juce::String& tab, const juce::String& heading);
    void setShown (juce::Component& first, bool shouldShow, const juce::String& caption = {});
    void showTabs();               // the tabs with something showing; the last one chosen if it's among them
    void pickTab (const juce::String& name);
    void apply();
    void applySeveral();
    void applyWindow();
    void send();

    /** What the actions work on: the element, or the ones selected together. */
    std::vector<int> targets() const;
    void showActions (bool forElements);
    void layer (int choice);   // the Layer list's item

    /** A parameter typed in: its id, or a name (any case) for the id; otherwise what was typed,
        as an id, for a parameter the plugin doesn't have yet. */
    juce::String paramIdFor (const juce::String& typed) const;
    juce::String paramCaption (const juce::String& caption, const juce::String& id) const;
    int footerSize() const    { return several.empty() ? 118 : 0; }

    PluginCanvas& canvas;
    int widget = -1;
    std::vector<int> several;   // the elements selected together (empty: one, or the window)
    bool updating = false;
    juce::TextEditor* typedBox = nullptr;   // typed in, for what the menu shows, its words not used yet
    juce::String title, subtitle;
    juce::String chosenTab;     // the tab last picked, shown again where there's one

    juce::TextButton closeButton;

    // What it does, under the title.
    juce::TextButton copyButton { "Copy" }, pasteButton { "Paste" }, duplicateButton { "Duplicate" };
    juce::ComboBox layerBox;
    // Knobs, sliders and switches: two long buttons under the row, one over the other.
    juce::TextButton galleryButton { "Replace this item with gallery item" };
    juce::TextButton knobStudioButton { "Edit in Knob Studio" };
    juce::ToggleButton similarToggle { "Include similars" };   // beside it: every control like this one
    int actionsSize() const;    // the actions' height: the row, and the two buttons when they show
    juce::Rectangle<int> actionsArea;
    TabStrip tabStrip;

    // An element's rows.
    juce::ComboBox type, source, mode, style, pictureMode, lowKey, highKey;
    juce::TextEditor param, roles[4];   // parameter ids, typed in
    juce::TextEditor label, options, textSize;
    juce::ToggleButton boldToggle { "Bold" };
    juce::ComboBox fontBox;     // its text's font (FontLibrary's)
    FontSample fontSample;
    ColourSwatchButton colour { "Colour" };
    juce::TextButton pictureButton, removeButton { "Delete" }, windowButton { "Window size and colours" };
    juce::StringArray roleNames;
    juce::StringArray lookNames;   // the Look list's entries, by item id - 1
    static constexpr int standardLookId = 9000;   // the Look list's "Standard look": none chosen
    juce::TextButton originalLookButton, restoreLookButton;   // back to the look it had; a built-in look as it came
    juce::Slider presetRounding;   // the preset manager's corners

    // Pages: the element's, and the window's list.
    juce::ComboBox pageBox;
    juce::TextEditor pagesBox;

    // The window's rows.
    juce::TextEditor widthBox, heightBox;
    juce::TextButton cropToolButton;
    ColourSwatchButton topColour { "Top colour" }, bottomColour { "Bottom colour" };
    juce::TextButton backgroundButton, paintBackgroundButton;

    // A painted background's surface (PanelPainter): its material in its colours and finish,
    // its texture, a pattern layer over it (Photoshop-style, with a blend mode), and its key
    // light's pool (it's lit by the global light).
    juce::ComboBox surfaceMaterial, surfaceFinish, patternBox, blendBox;
    ColourSwatchButton surfaceColour { "Main colour" }, surfaceColour2 { "Second colour" }, patternColour { "Pattern colour" };
    juce::Slider textureSize, textureStrength, patternOpacity, patternSize, patternAngle, keyLight;
    juce::var surfaceRecipe;   // how the selected picture was painted, with the changes asked for since

    juce::Colour lastMain;     // the main colour as it was: the second follows it until it's chosen
    bool shapeSurface = false; // the surface rows are the selected shape's (not a painted picture's)

    static constexpr int flatMaterialId = 1000;   // the Material list's "Flat colour" (shapes only)

    void showSurface (const juce::var& recipe);
    void showSurfaceRows();               // the surface rows that apply to what's chosen
    void applySurface (bool materialChanged);
    void fillMaterials (bool withFlat);   // a shape can be a flat colour too

    // A level meter: how it shows the level (MeterPaint), its colours, its LEDs, glow, peak and
    // scale.
    juce::ComboBox meterDisplay;
    ColourSwatchButton meterLow { "Low colour" }, meterMid { "Middle colour" }, meterHigh { "Top colour" }, meterBack { "Background" };
    juce::Slider meterSegments, meterGlow;
    juce::ToggleButton meterPeakToggle { "Peak hold" }, meterScaleToggle { "dB scale" };
    void showMeter (const GuiWidget&);
    void applyMeter();

    // A lamp's, selector's, keyboard's or label's display (PanelParts): how it looks; a lamp
    // that follows the level; a label's arrowheads.
    juce::ComboBox partDisplay, arrowsBox;
    juce::ToggleButton followToggle { "Follows the level" };
    void showPart (const GuiWidget&);
    void applyPart();

    // Every element is an object: where it lies, how big, turned and see-through.
    juce::TextEditor xBox, yBox, wBox, hBox, angleBox;
    juce::Slider opacitySlider;
    void showObject (const GuiWidget&);
    void applyObject();

    // A shape: its kind (in the Type list), its geometry (its corners' rounding: a rectangle
    // goes all the way to a pill), and Photoshop's Layer Style for it (bevel, contour, stroke,
    // shadows, colour overlay) in a window of its own.
    juce::Slider cornerRadius, starPoints, starInner, topPoint, middleX, middleY, leanX, leanY;
    juce::TextButton layerStyleButton;
    void showShape (const GuiWidget&);
    void applyShape();

    // A line: its width, its arrowheads and ends, and straight again.
    juce::Slider lineWidthSlider;
    juce::ComboBox lineArrowBox, lineCapBox;
    juce::TextButton straightenButton { "Straighten it" };

    // The panel's one light, in Photoshop's Global Light window (a lit element's Look tab, a
    // shape's Shape tab, the window's Window tab).
    juce::TextButton globalLightButton, shapeLightButton, windowLightButton;

    Rows rows;
    juce::Viewport viewport;

    // Sonify AI.
    juce::TextEditor instruction;
    SendButton sendButton;
    juce::Rectangle<int> headerArea, footerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElementMenu)
};

//==============================================================================
/** Across the top of the canvas: Sonify AI at work on a request made from an edit menu,
    then its answer (questions for the user in yellow), with Open chat and a close button. */
class PluginCanvas::AiBanner final : public juce::Component,
                                     private juce::Timer
{
public:
    explicit AiBanner (PluginCanvas& owner);
    ~AiBanner() override;

    void showWorking (const juce::String& what);
    void showReply (const juce::String& reply);
    int getIdealHeight (int width) const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::AttributedString makeText() const;

    PluginCanvas& canvas;
    juce::String text;
    bool working = false;
    int dots = 0;
    juce::TextButton chatButton { "Open chat" }, closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiBanner)
};

//==============================================================================
/**
    What the programmed elements ask of the canvas (stella_element_host.h): their
    parameters' values and ranges, their options and settings from the layout, levels to
    show, notes to play. element is the widget's index in the layout.
*/
class PluginCanvas::ElementBridge final : public stella::ui::ElementHost
{
public:
    explicit ElementBridge (PluginCanvas& owner) : canvas (owner) {}

    bool paramInfo (int element, int slot, int info, float& result) override;
    int findSlot (int element, const std::string& role) override;
    void setValue (int element, int slot, float newValue) override;
    void gesture (int element, int slot, bool starts) override;
    bool text (int element, int which, int index, const std::string& key, std::string& result) override;
    int numOptions (int element) override;
    std::uint32_t colour (int element) override;
    float level (int element, bool rms) override;
    void readScope (int element, float* destination, int numSamples) override;
    float sourceLevel (int element, const std::string& role, bool rms) override;
    void readSourceScope (int element, const std::string& role, float* destination, int numSamples) override;
    int presetInfo (int element, int which, int index) override;
    void presetTask (int element, int task, int index) override;
    bool pictureSize (int element, const std::string& name, int& width, int& height) override;
    bool readPicture (int element, const std::string& name, int width, int height, std::uint32_t* destination) override;
    int fontData (const std::string& name, bool bold, std::uint8_t* destination, int capacity) override;
    void playNote (int element, int note, float velocity) override;
    bool isNoteDown (int note) override;
    double seconds() override;

    /** Whether a preset command (stella::ui::PresetCommand, as PresetAction) can be done now. */
    static bool canDoPreset (const PresetMenu& menu, bool hasActions, int command);

    /** A picture of the plugin's, by a file name in gui/images (none outside it). */
    static juce::Image pictureIn (PluginCanvas& canvas, const std::string& name);

private:
    const GuiWidget* widget (int element) const;
    const PluginCanvas::Param* paramAt (int element, int slot) const;
    static juce::String sourceOf (const GuiWidget& w, const std::string& role);   // "" its own "source"

    PluginCanvas& canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElementBridge)
};
