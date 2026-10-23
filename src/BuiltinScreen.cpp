// D:\workspace\Fanan_web_hub\src\BuiltinScreen.cpp

#include "BuiltinScreen.h"
#include "BuiltinPlugins.h"
#include "Looks.h"
#include "PluginCanvas.h"
#include "Preferences.h"
#include "PresetLibrary.h"
#include "SonifyPanels.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace
{
    /** How far along its knob a parameter's middle sits: frequencies and times turn as they're
        heard (their geometric middle at the knob's middle). */
    float skewFor (const BuiltinPlugin::Spec& spec)
    {
        const juce::String unit (spec.unit);

        if (! (unit == "Hz" || unit == "ms") || spec.minValue <= 0.0 || spec.maxValue / spec.minValue < 8.0)
            return 1.0f;

        const auto middle = std::sqrt (spec.minValue * spec.maxValue);
        const auto t = (middle - spec.minValue) / (spec.maxValue - spec.minValue);
        return t > 0.0 && t < 1.0 ? static_cast<float> (std::log (0.5) / std::log (t)) : 1.0f;
    }

    //==============================================================================
    class PanelWindow final : public juce::DocumentWindow,
                              private juce::Timer
    {
    public:
        explicit PanelWindow (BuiltinPlugin& owner)
            : DocumentWindow (owner.getDescriptor().name, juce::Colours::black, DocumentWindow::closeButton),
              plugin (owner)
        {
            setUsingNativeTitleBar (true);

            // Pictures would come from a gui folder; the built-ins' panels have none (their
            // materials are drawn), but the canvas wants one to draw at all.
            const auto gui = UserData::getFolder().getChildFile ("Panels");
            looks.setGuiFolder (gui);
            canvas.setLooks (looks);
            canvas.setPanelOnly (true);
            canvas.setGuiFolder (gui);

            GuiLayout layout;

            if (! BuiltinScreen::layoutFor (plugin.getPanelName(), layout))
            {
                juce::Array<GuiLayout::ParamInfo> infos;

                for (const auto& spec : plugin.getSpecs())
                    if (! spec.hidden)
                        infos.add ({ spec.name, {}, spec.name, spec.unit, static_cast<float> (spec.minValue),
                                     static_cast<float> (spec.maxValue), static_cast<float> (spec.defaultValue) });

                layout = GuiLayout::makeDefault (infos, plugin.getDescriptor().name);
            }

            canvas.onParameterChanged = [this] (int index, float value)
            {
                plugin.queueParameter (index, value);
                plugin.panelTouched (index);
                pushParameters (true);
            };

            // The preset manager: its factory presets (presets_<name>.json, built in), the user's
            // own (Save, Save as, Load), and banks (Save bank, Load bank).
            if (setUpPresets())
            {
                presetUi.getValues = [this] { return currentValues(); };
                presetUi.apply = [this] (const Preset& preset) { applyPreset (preset); };
                presetUi.changed = [this] { remember(); };
                canvas.onPresetChosen = [this] (int index) { presetUi.choose (index); };
                canvas.onPresetAction = [this] (PresetAction action) { presetUi.perform (action); };
            }

            canvas.onNote = [this] (int note, float velocity) { plugin.panelNote (note, velocity); };
            canvas.isNoteDown = [this] (int note) { return plugin.isPanelNoteDown (note); };
            canvas.readLevel = [this] (const juce::String& source, bool) { return plugin.readSource (source); };
            canvas.readScope = [this] (const juce::String& source, float* destination, int numSamples)
            {
                plugin.readCurve (source, destination, numSamples);
            };

            juce::StringArray signals { "in L", "in R", "out L", "out R" };
            signals.addArray (plugin.getCurveSources());
            canvas.setSources (signals, plugin.getDisplaySources());

            canvas.setLayout (layout);
            pushParameters (true);
            canvas.setPresets (presets.getMenu());

            setContentNonOwned (&canvas, false);
            canvas.setSize (layout.width, layout.height);
            setContentComponentSize (layout.width, layout.height);
            setResizable (true, false);
            setResizeLimits (juce::jmax (120, layout.width / 3), juce::jmax (90, layout.height / 3), layout.width * 3, layout.height * 3);
            getConstrainer()->setFixedAspectRatio (static_cast<double> (layout.width) / layout.height);
            centreWithSize (getWidth(), getHeight());

            startTimerHz (15);
        }

        ~PanelWindow() override
        {
            stopTimer();
            presetUi.getValues = nullptr;
            presetUi.apply = nullptr;
            presetUi.changed = nullptr;
            clearContentComponent();
        }

        void show (const juce::String& title)
        {
            setName (title.isNotEmpty() ? title : plugin.getDescriptor().name);
            pushParameters (true);

            // Presets saved elsewhere since (another track's window) show up.
            if (presets.hasPlugin())
            {
                presets.refresh();
                follow();
            }

            setVisible (true);
            toFront (true);
        }

        void closeButtonPressed() override   { setVisible (false); }

    private:
        void timerCallback() override
        {
            // Automation, the Hub's controls and undo move its knobs too (and a song or an undo,
            // its preset).
            if (isVisible())
            {
                pushParameters (false);
                follow();
            }
        }

        //==============================================================================
        /** Its factory presets into the library (a file of them in Sonify's settings, which the
            library reads), and the preset the plugin is on. False: it has none. */
        bool setUpPresets()
        {
            juce::String json;

            if (! BuiltinScreen::resourceText ("presets_" + juce::String (plugin.getPanelName()) + ".json", json))
                return false;

            const auto file = SettingsFolder::get().getChildFile ("Factory presets")
                                                   .getChildFile (juce::String (plugin.getPanelName()) + ".json");

            if (! file.existsAsFile() || file.loadFileAsString() != json)
            {
                file.getParentDirectory().createDirectory();
                file.replaceWithText (json);
            }

            presets.setPlugin (plugin.getDescriptor().name, file, false);

            // A new one: on Default while its values are Default's.
            if (! plugin.presetChoice.known && ! presets.isEmpty())
            {
                const auto& first = presets.getEntries().front();

                if (sameValues (first.preset))
                    plugin.presetChoice = { first.preset.name, first.group, false, true };
                else
                    plugin.presetChoice = { {}, {}, false, true };
            }

            shownChoice = {};
            follow();
            return true;
        }

        /** The preset the plugin is on (a song, an undo), and its values moved off it meanwhile
            (the knobs, the Hub, automation): a * after its name. */
        void follow()
        {
            if (! presets.hasPlugin())
                return;

            const auto& choice = plugin.presetChoice;

            if (choice.name != shownChoice.name || choice.group != shownChoice.group || choice.modified != shownChoice.modified)
            {
                if (choice.name.isEmpty())
                    presets.choose (-1);
                else if (! presets.restore (choice.name, choice.group, choice.modified))
                    plugin.presetChoice = { {}, {}, false, true };   // gone since

                shownChoice = plugin.presetChoice;
            }

            if (const auto* entry = presets.getCurrentEntry(); entry != nullptr && ! presets.isModified() && ! sameValues (entry->preset))
            {
                presets.setModified (true);
                remember();
            }

            if (const auto menu = presets.getMenu(); menu != shownMenu)
            {
                shownMenu = menu;
                canvas.setPresets (menu);
            }
        }

        /** What the library is on, onto the plugin (kept with the song). */
        void remember()
        {
            juce::String name, group;

            if (const auto* entry = presets.getCurrentEntry())
            {
                name = entry->preset.name;
                group = entry->group;
            }

            const BuiltinPlugin::PresetChoice now { name, group, presets.isModified(), true };
            auto& choice = plugin.presetChoice;

            if (now.name != choice.name || now.group != choice.group || now.modified != choice.modified || ! choice.known)
            {
                choice = now;
                plugin.markStateChanged();
            }

            shownChoice = choice;
            shownMenu = presets.getMenu();
            canvas.setPresets (shownMenu);
        }

        /** The plugin's values by parameter name (what Save writes). */
        std::map<juce::String, float> currentValues() const
        {
            std::map<juce::String, float> values;
            const auto& specs = plugin.getSpecs();

            for (size_t i = 0; i < specs.size(); ++i)
                if (specs[i].saved && ! specs[i].hidden)
                    values[specs[i].name] = static_cast<float> (plugin.getParameterValue (static_cast<int> (i)));

            return values;
        }

        /** A preset's values on the plugin; the ones it doesn't name go back to their defaults. */
        void applyPreset (const Preset& preset)
        {
            const auto& specs = plugin.getSpecs();

            for (size_t i = 0; i < specs.size(); ++i)
            {
                if (! specs[i].saved || specs[i].hidden)
                    continue;

                const auto found = preset.values.find (specs[i].name);
                plugin.queueParameter (static_cast<int> (i), found != preset.values.end() ? static_cast<double> (found->second) : specs[i].defaultValue);
            }

            plugin.markStateChanged();
            pushParameters (true);
        }

        /** Whether the plugin's values are a preset's (as the plugin keeps them). */
        bool sameValues (const Preset& preset) const
        {
            const auto& specs = plugin.getSpecs();

            for (size_t i = 0; i < specs.size(); ++i)
            {
                if (! specs[i].saved || specs[i].hidden)
                    continue;

                const auto found = preset.values.find (specs[i].name);
                const auto wanted = BuiltinPlugin::quantised (specs[i], found != preset.values.end() ? static_cast<double> (found->second) : specs[i].defaultValue);

                if (std::abs (wanted - plugin.getParameterValue (static_cast<int> (i))) > 1.0e-4)
                    return false;
            }

            return true;
        }

        void pushParameters (bool always)
        {
            const auto& specs = plugin.getSpecs();
            bool changed = always || shown.size() != specs.size();

            for (size_t i = 0; i < specs.size() && ! changed; ++i)
                changed = std::abs (shown[i] - plugin.getParameterValue (static_cast<int> (i))) > 1.0e-9;

            if (! changed)
                return;

            shown.resize (specs.size());
            juce::Array<PluginCanvas::Param> params;

            for (size_t i = 0; i < specs.size(); ++i)
            {
                const auto& spec = specs[i];
                shown[i] = plugin.getParameterValue (static_cast<int> (i));

                PluginCanvas::Param p;
                p.index = static_cast<int> (i);
                p.id = spec.name;     // what the panel's controls name
                p.name = spec.name;
                p.unit = spec.unit;
                p.min = static_cast<float> (spec.minValue);
                p.max = static_cast<float> (spec.maxValue);
                p.def = static_cast<float> (spec.defaultValue);
                p.value = static_cast<float> (shown[i]);
                p.skew = skewFor (spec);
                p.decimals = BuiltinPlugin::decimalsOf (spec);
                params.add (p);
            }

            canvas.setParameters (params);
        }

        BuiltinPlugin& plugin;
        Looks looks;             // before the canvas, which draws with it
        PluginCanvas canvas;
        std::vector<double> shown;

        PresetLibrary presets;
        PresetUi presetUi { *this, presets };
        BuiltinPlugin::PresetChoice shownChoice;
        PresetMenu shownMenu;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelWindow)
    };

    class WindowScreen final : public BuiltinPlugin::Screen
    {
    public:
        explicit WindowScreen (BuiltinPlugin& owner) : plugin (owner) {}

        void show (const juce::String& title) override
        {
            if (window == nullptr)
                window = std::make_unique<PanelWindow> (plugin);

            window->show (title);
        }

        void hide() override
        {
            if (window != nullptr)
                window->setVisible (false);
        }

    private:
        BuiltinPlugin& plugin;
        std::unique_ptr<PanelWindow> window;
    };
}

//==============================================================================
void BuiltinScreen::install()
{
    BuiltinPlugin::setScreenFactory ([] (BuiltinPlugin& plugin) -> std::unique_ptr<BuiltinPlugin::Screen>
    {
        return std::make_unique<WindowScreen> (plugin);
    });
}

bool BuiltinScreen::resourceText (const juce::String& fileName, juce::String& text)
{
    for (int i = 0; i < SonifyPanels::namedResourceListSize; ++i)
    {
        if (! fileName.equalsIgnoreCase (SonifyPanels::getNamedResourceOriginalFilename (SonifyPanels::namedResourceList[i])))
            continue;

        int size = 0;
        const auto* data = SonifyPanels::getNamedResource (SonifyPanels::namedResourceList[i], size);

        if (data == nullptr || size <= 0)
            return false;

        text = juce::String::fromUTF8 (data, size);
        return true;
    }

    return false;
}

bool BuiltinScreen::layoutFor (const juce::String& panelName, GuiLayout& layout)
{
    juce::String text;

    if (panelName.isEmpty() || ! resourceText ("panel_" + panelName + ".json", text))
        return false;

    const auto json = juce::JSON::parse (text);

    if (! json.isObject())
        return false;

    layout = GuiLayout::fromVar (json);
    return layout.width > 0 && layout.height > 0;
}
