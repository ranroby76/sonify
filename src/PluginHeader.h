// D:\workspace\Fanan_web_hub\src\PluginHeader.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

class Plugin;

//==============================================================================
/**
    The strip above every plugin's own window: its ON/OFF, its name and its format.

    Every window a plugin opens in has one, whatever made it: a VST3, VST2, AU or LV2 editor
    (Vst3Plugin.cpp), a native CLAP's own window (WclapPlugin.cpp), Sonify's own plugins
    (InternalPlugin.cpp: Natura, Zax, AXIS), a Plugins Creator Studio plugin (CreatorPlugin.cpp),
    the built-ins' panels (BuiltinScreen.cpp), the samplers, the racks and the Playlist Player.
    (A WCLAP's page and the generic controls open in the Hub's page, hub-screens.js, with the same
    switch in their title bar.)

    PRESETS, beside it, is a menu of preset files for whatever plugin it is:
        Save Preset... / Open Preset...    a .sonifypreset with the plugin's whole state, the
                                           same file for every plugin in Sonify (in
                                           <user data>\Presets\<plugin>)
        VST3 Preset (.vstpreset)           the VST3 standard, as Cubase, Studio One / Fender
                                           Studio, Reaper and Bitwig save them: for a VST3 only
        Show Presets Folder

    ON/OFF is the plugin's bypass where it sits, the same one the mixer shows: an insert's bypass,
    a rack node's mute, or a track instrument switched off (silent, though it keeps playing, so
    nothing it holds is lost). Who holds the plugin says what it is and switches it (a Switcher:
    Sonify's engine, MainComponent installs it); a plugin nobody holds that way (in the Sampling
    Studio's rack, say) shows no switch. The header follows it a few times a second, so a click
    in the mixer shows here too.

    attach() puts it in the window's menu bar place (DocumentWindow::setMenuBarComponent): the
    window then sizes itself around its content and the header together, exactly as it did
    around the content alone, and its constrainer (the header's own, set on the window) keeps
    the limits and the shape the window asks for to the content, not to the content and header.
    Call it before the window's content is set and before its limits.

    Message thread only.
*/
class PluginHeader final : public juce::Component,
                           private juce::Timer
{
public:
    /** DocumentWindow's menu bar height (its default, which setMenuBarComponent keeps). */
    static constexpr int height = 24;

    /** Puts a header on the window, above its content (see above). */
    static void attach (juce::DocumentWindow& window, Plugin& plugin);

    /** Who holds plugins and switches them: state says 1 (on) or 0 (off) for one it holds, -1 for
        one it doesn't; set switches one it holds. */
    struct Switcher
    {
        std::function<int (const Plugin&)> state;
        std::function<void (const Plugin&, bool on)> set;
    };

    /** Returns the id removeSwitcher takes. */
    static int addSwitcher (Switcher switcher);
    static void removeSwitcher (int id);

    /** A plugin's ON/OFF from whoever holds it: 1 on, 0 off, -1 nobody can switch it. */
    static int stateOf (const Plugin& plugin);

    //==============================================================================
    /** A window's constrainer whose limits and shape are the content's: the header's height
        comes off before they're checked, and goes back on after. */
    class Constrainer final : public juce::ComponentBoundsConstrainer
    {
    public:
        void checkBounds (juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previousBounds,
                          const juce::Rectangle<int>& limits, bool isStretchingTop, bool isStretchingLeft,
                          bool isStretchingBottom, bool isStretchingRight) override;
    };

    explicit PluginHeader (Plugin& plugin);
    ~PluginHeader() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void enablementChanged() override;

    Constrainer& getConstrainer() noexcept      { return constrainer; }

private:
    class Switch;

    void timerCallback() override;
    void refresh();
    void toggle();

    void showPresetMenu();
    void savePreset (bool vst3);
    void openPreset (bool vst3);
    void tell (const juce::String& title, const juce::String& message);
    juce::File presetFolder() const;

    Plugin& plugin;
    std::unique_ptr<Switch> button;
    juce::TextButton presetsButton;
    std::unique_ptr<juce::FileChooser> chooser;
    Constrainer constrainer;
    int state = -1;
    juce::String name, format;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginHeader)
};
