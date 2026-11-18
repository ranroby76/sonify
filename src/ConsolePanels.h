// D:\workspace\Fanan_web_hub\src\ConsolePanels.h

#pragma once

#include "BuiltinPlugins.h"
#include "PresetLibrary.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

//==============================================================================
/**
    The console-module faceplates of Sonify's stock effects and the Test Synth: blue-grey enamel,
    coloured knobs (black = the panel's main control, red = gain/level, grey = frequency/time/ratio,
    blue = Q/shape/mix), cream push buttons, cream VU meters and LED meters, all drawn in code
    (juce::Graphics, no pictures). Their layouts are the approved mockups
    (design/console-v1/make_console_mockups.js, the "Stock effects — console v1" design canvas).

    BuiltinScreen's panel window shows one in place of the designed JSON panel when there is a
    console layout for the plugin's panel name; the window keeps the preset manager (the strip at
    the top of the plate drives it) and the plugin header above.
*/
class ConsolePanel : public juce::Component
{
public:
    ~ConsolePanel() override = default;

    /** A console faceplate for this plugin, or none when its panel has no console layout. */
    static std::unique_ptr<ConsolePanel> create (BuiltinPlugin& plugin);

    /** The panel's size as designed (it scales to whatever size it is given). */
    virtual juce::Rectangle<int> designBounds() const = 0;

    /** The preset strip: what it shows, and what it asks for. */
    virtual void setPresets (const PresetMenu& menu) = 0;
    std::function<void (int index)> onPresetChosen;
    std::function<void (PresetAction)> onPresetAction;
};
