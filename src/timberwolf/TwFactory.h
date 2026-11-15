// D:\workspace\Fanan_web_hub\src\timberwolf\TwFactory.h
#pragma once
//==============================================================================
//  TwFactory.h  -  the rigs Timber Wolf comes with.
//
//  Each is written as a short JSON text (TwFactory.cpp): the chain, effect by
//  effect, each with only the knobs it changes from the effect's own defaults
//  (by the knob's name, in its own units), splits with their two paths, and
//  what the footswitches do.  rigOf() turns one into a rig, as a user's preset
//  file holds it, so a factory rig loads exactly as a saved one does.
//
//      { "name": "Clean + Lead",
//        "chain": [ { "fx": "ax_gate", "k": { "Threshold": -52 } },
//                   { "split": { "levelB": -3, "panA": -40, "panB": 40 },
//                     "a": [ { "fx": "ax_amp", "k": { "Voice": "CLEAN" } } ],
//                     "b": [ { "fx": "ax_amp", "k": { "Voice": "LEAD" } } ] },
//                   { "fx": "tape_echo", "off": true } ],
//        "switches": [ { "job": "toggle", "fx": [ 3 ] }, { "job": "tap" } ] }
//
//  "fx" in a switch counts the chain's effects in the order they're written, from 1.
//==============================================================================
#include <juce_core/juce_core.h>

#include <vector>

namespace tw::factory
{
    struct Preset
    {
        juce::String name;
        juce::String json;
    };

    const std::vector<Preset>& presets();

    /** A factory rig as a rig (TimberWolfProcessor::rigFromVar reads it). */
    juce::var rigOf (const Preset& preset);

    /** What's wrong with one, if anything (an effect, knob or position that isn't there): for
        the tests. */
    juce::StringArray problemsOf (const Preset& preset);
}
