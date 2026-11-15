// D:\workspace\Fanan_web_hub\src\timberwolf\TwPresetFiles.h
#pragma once
//==============================================================================
//  TwPresetFiles.h  -  where Timber Wolf's rigs are kept.
//
//  A rig is one .sonifypreset file in Sonify's own presets folder (the files,
//  the folder and the format are Sonify's SonifyPresets.h, the same in every
//  Sonify plugin):
//
//      <Sonify's user data>\Presets\Timber Wolf\<name>.sonifypreset
//
//  Its "values" are the slots' host parameters (0..1), as the format asks; the
//  rig itself - the chain, every effect's values by its uid, the snapshots, the
//  footswitch jobs and pedal targets - is beside them as "rig", which is what
//  Timber Wolf reads back.
//
//  A bank (.twbank) is many rigs in one file, to pass around: loading one
//  writes each rig in it into the folder above.
//==============================================================================
#include "../SonifyPresets.h"   // Sonify's one copy of it, shared by every plugin of its own

namespace tw::presetfiles
{
    inline constexpr const char* pluginName = "Timber Wolf";
    inline constexpr const char* bankFormat = "Timber Wolf bank";
    inline constexpr const char* bankExtension = ".twbank";

    inline juce::File folder()                                  { return sonify::presets::folderFor (pluginName); }
    inline juce::File fileFor (const juce::String& name)        { return sonify::presets::fileFor (pluginName, name); }
    inline juce::Array<juce::File> userPresets()                { return sonify::presets::list (pluginName); }
    inline void showFolder()                                    { sonify::presets::showFolder (pluginName); }
}
