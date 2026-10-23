// D:\workspace\Fanan_web_hub\src\BuiltinEffects.h

#pragma once

#include "Plugin.h"

#include <memory>
#include <vector>

//==============================================================================
/**
    The built-ins brought from Grex (GrexFx.h), each with a designed panel: Sonify Chorus (the
    Juno-style BBD chorus), Sonify Phaser, Sonify Auto-Wah, Sonify Reverb (Grex's hall with its
    early reflections), Sonify Delay (the ping-pong delay, in time with the song or free), Sonify
    Sweetener (Soften, Peak, Tame and Round) and Sonify Ducker (two bands that dip where a
    sidechain plays); and the Test Synth (TestSynth.cpp), the instrument a track plays before it
    has one of its own, as a plugin with its panel and more to it.

    Builtins (BuiltinPlugins.h) lists and makes them with its own.
*/
namespace BuiltinEffects
{
    const std::vector<Plugin::Descriptor>& describe();

    /** The Browser's Type for one of these (empty: not one of these). */
    juce::String categoryOf (const juce::String& pluginId);

    /** One of these, made; null when the id isn't one of these. */
    std::unique_ptr<Plugin> create (const juce::String& pluginId);
}

namespace TestSynths
{
    Plugin::Descriptor describe();
    std::unique_ptr<Plugin> create();
}
