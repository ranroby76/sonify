// D:\workspace\Fanan_web_hub\src\Workspace.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "KnobStudio.h"
#include "Looks.h"
#include "PluginCanvas.h"
#include "Toolbox.h"

#include <functional>

//==============================================================================
/**
    The window below the top bar, in five tabs (picked in the top bar):

        Build with AI  the conversation with Sonify AI (the main window's chat panel)
        Edit UI        the plugin, playing live, on a grid you zoom and move around, in two
                       tabs of its own, in the middle of its bar: Edit, where its GUI is
                       reshaped (with the primitives toolbox on the left), and Play, where the
                       GUI is locked and plays like the finished plugin, with its presets.
                       Empty (just the grid) until a project is open
        Knob Studio    KnobMaker's layer builder, for the looks of its knobs, sliders and
                       switches. It starts empty: a control's look opens in it from Edit UI
                       (its menu's "Design it in the Knob Studio"), with the controls like it
                       too when "Include similars" is ticked
        Protection     the plugin or MIDI FX, free or with a price, posted to Sonify's store
                       (ProtectionPage: the studio owns it)
        Store          Sonify's store, open to every user: browse, buy, add to Sonify
                       (StorePage: the studio owns it)
*/
class Workspace final : public juce::Component
{
public:
    enum TabIndex
    {
        aiTab = 0,
        pluginTab,      // Edit UI
        studioTab,
        protectionTab,  // free or a price, and the store
        storeTab        // Sonify's store: browse, buy, add
    };

    static constexpr int numPages = 5;

    enum class BuildState { idle, building, playing, failed };

    using ParamControl = PluginCanvas::Param;

    Workspace();

    /** The Build with AI tab's page: the main window owns it. */
    void setAiPage (juce::Component& page);

    /** name empty: no project is open. */
    void setProject (const juce::String& name, const juce::String& kindName);

    /** The open project's gui folder (pictures, looks, the Edit UI view); none: no project. */
    void setGuiFolder (const juce::File& folder);

    /** The looks the plugin's controls wear, and the Knob Studio edits. */
    Looks& getLooks() noexcept                    { return looks; }

    /** In place of Edit UI and the Knob Studio: a note on why there's nothing to design (a MIDI FX
        is open). An empty title: none. */
    void setNotice (const juce::String& title, const juce::String& detail);

    /** The Edit UI banner: Sonify AI at work on a request from an edit menu, then its answer. */
    void showAiWorking (const juce::String& what);
    void showAiReply (const juce::String& reply);

    /** The Edit UI tab's own tabs: Edit (false) or Play (true). */
    void setPlayMode (bool playMode);

    void showTab (TabIndex index);
    TabIndex getCurrentTab() const noexcept       { return currentTab; }

    void setBuild (BuildState state, const juce::String& status);
    void setExporting (bool exporting);
    void setExportingPlugin (bool exporting);   // Export: plugin files are being built
    void setParameters (const juce::Array<ParamControl>& params);
    void setLayout (const GuiLayout& layout);
    const GuiLayout& getLayout() const noexcept   { return pluginView.canvas.getLayout(); }

    /** The GUI's programmed elements from the latest build (nullptr: none). */
    void setElements (std::shared_ptr<WasmElements> elements)   { pluginView.canvas.setElements (std::move (elements)); }

    /** The presets, for Play's row and the panel's preset managers. */
    void setPresets (const PresetMenu& presets);

    /** What meters, lamps and scopes can watch (for the design panel's lists). */
    void setSources (const juce::StringArray& signals, const juce::StringArray& displays);

    /** For Export: the GUI baked into images. */
    PluginCanvas::Bake bakeGui()                  { return pluginView.canvas.bake(); }

    /** For Export to plugin files: the window baked page by page (PluginCanvas::bakeForExport). */
    PluginCanvas::ExportBake bakeForExport (const juce::StringArray& presetNames)   { return pluginView.canvas.bakeForExport (presetNames); }

    /** The panel as it plays now, about longest pixels on its longer side: the store's picture
        (invalid without a panel). */
    juce::Image snapshotPanel (int longest);

    /** The Protection tab's page: the studio owns it. */
    void setProtectionPage (juce::Component& page);

    /** The Store tab's page: the studio owns it. */
    void setStorePage (juce::Component& page);

    std::function<void()> onShowLogRequested;
    std::function<void()> onExportRequested;   // Add to Sonify: built first, then added
    std::function<void()> onExportPluginRequested;   // Export: VST3, CLAP (and on a Mac, Audio Unit) files
    std::function<void()> onPanicRequested;   // the Panic button: every note off
    std::function<void()> onAutoLayoutRequested;
    std::function<void()> onLayoutEdited;
    std::function<void (int index, float value)> onParameterChanged;
    std::function<void (int note, float velocity)> onNote;   // a keyboard element's key: down (velocity 0..1) or up (0)
    std::function<bool (int note)> isNoteDown;               // whether a note is sounding, for keyboards to show
    std::function<void (bool playMode)> onModeChanged;   // the user picked Edit or Play
    std::function<void (int tabIndex)> onTabChanged;     // whichever way the tab changed

    /** An edit menu's instruction for Sonify AI, about one element of the GUI (-1: the window). */
    std::function<void (int widgetIndex, const juce::String& instruction)> onAskAiAboutGui;

    std::function<void (int presetIndex)> onPresetChosen;
    std::function<void (PresetAction action)> onPresetAction;          // Save, Save as, Load, the banks...
    std::function<void (const juce::String& why)> onElementsFailed;   // the GUI's programmed elements stopped
    /** Repaints a painted picture from its recipe (a background's material, colours, pattern); done gets "" or the problem. */
    std::function<void (const juce::var& recipe, std::function<void (const juce::String& problem)> done)> onPaintPicture;

    std::function<float (const juce::String& source, bool rms)> readLevel;
    std::function<void (const juce::String& source, float* destination, int numSamples)> readScope;
    std::function<void (const juce::StringArray& levels, const juce::StringArray& scopes)> onSourcesChanged;

    void resized() override;

private:
    //==============================================================================
    class PluginView final : public juce::Component,
                             public juce::DragAndDropContainer
    {
    public:
        PluginView();

        void setProject (const juce::String& name, const juce::String& kindName);
        void setPlayMode (bool playMode);
        void setBuild (BuildState state, const juce::String& status);
        void setExporting (bool exporting);
        void setExportingPlugin (bool exporting);

        void setPresets (const PresetMenu& presets);

        std::function<void()> onShowLog, onExport, onExportPlugin, onAutoLayout, onPanic;
        std::function<void (int)> onPresetChosen;
        std::function<void (PresetAction)> onPresetAction;
        std::function<void (bool playMode)> onMode;

        void paint (juce::Graphics&) override;
        void resized() override;

        static constexpr int barHeight = 48, rowHeight = 40, modeTabWidth = 132;

        PluginCanvas canvas;
        Toolbox toolbox;

    private:
        void chooseMode (bool play);
        void showMoreMenu();

        // Two tabs, in the middle of the bar: Edit (reshape the panel) and Play (the panel locked,
        // its controls live).
        juce::TextButton editTab { "Edit" }, playTab { "Play" };
        juce::TextButton logButton { "Log" }, exportButton { "Add to Sonify" };
        juce::TextButton pluginButton { "Export Plugin" };   // VST3, CLAP (and on a Mac, Audio Unit) files
        juce::TextButton panicButton { "Panic" };   // every note off: stuck notes stop

        // The Edit tab's row: start over with an automatic panel.
        juce::TextButton autoButton { "Auto layout" };

        // The Play tab's row: the preset manager.
        juce::ComboBox presetBox;
        juce::TextButton previousButton, nextButton, saveButton { "Save" }, saveAsButton, loadButton, moreButton;
        PresetMenu presets;

        juce::String projectName, kind, buildStatus;
        BuildState buildState = BuildState::idle;
        bool playMode = false;
        juce::Rectangle<int> statusArea, hintArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginView)
    };

    //==============================================================================
    /** A note in the middle of the page: a title and a few words under it. */
    class HintView final : public juce::Component
    {
    public:
        HintView() = default;

        void setText (const juce::String& newTitle, const juce::String& newDetail);
        bool hasText() const noexcept    { return title.isNotEmpty(); }
        void paint (juce::Graphics&) override;

    private:
        juce::String title, detail;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HintView)
    };

    //==============================================================================
    void showPages();

    Looks looks;   // before the canvas and the studio, which both use it
    PluginView pluginView;
    KnobStudio studio { looks };
    HintView notice;   // in place of Edit UI and the Knob Studio, while it has something to say
    bool openingStudioForControl = false;
    juce::Component* aiPage = nullptr;
    juce::Component* protectionPage = nullptr;
    juce::Component* storePage = nullptr;
    TabIndex currentTab = aiTab;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Workspace)
};
