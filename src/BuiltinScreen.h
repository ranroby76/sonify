// D:\workspace\Fanan_web_hub\src\BuiltinScreen.h

#pragma once

#include "GuiLayout.h"

#include <juce_core/juce_core.h>

//==============================================================================
/**
    The windows of Sonify's built-in effects and the Test Synth: each one's panel
    (panel_<name>.json, built in), drawn as the Plugins Creator draws a made plugin's (PluginCanvas,
    the same looks, materials and meters), its controls on the plugin's parameters, its meters and
    displays on what the plugin measures. The panels were designed by the Plugins Creator's own
    taste (skill_panel_taste.md and the inspiration pool): one character each, a hero control or
    two, an 8-pixel rhythm. Each has a preset manager: its factory presets (presets_<name>.json,
    built in), the user's own and banks, as a made plugin's (PresetLibrary).
*/
namespace BuiltinScreen
{
    /** Gives the built-ins their windows (once, at startup). */
    void install();

    /** A built-in panel's layout by name; false when there's none by that name. */
    bool layoutFor (const juce::String& panelName, GuiLayout& layout);

    /** A built-in file's text (panel_<name>.json, presets_<name>.json); false when there's none. */
    bool resourceText (const juce::String& fileName, juce::String& text);
}
