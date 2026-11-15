// D:\workspace\Fanan_web_hub\src\kit\kit_PluginDesign.h

#pragma once

#include "GuiLayout.h"
#include "PresetBank.h"

#include <vector>

//==============================================================================
/**
    What makes this plugin this plugin, written by Sonify's Plugins Creator Studio from its
    project each time it's saved (the rest of the source is the same for every plugin):

        PluginWindow.cpp    window()          the window: its size, light, camera and pages,
                                              then every element on it, back to front, with
                                              everything it has (where, how big, its look, its
                                              colours, its words, its parameter...)
        PluginPresets.cpp   factoryPresets()  the presets that come with it
        PluginAssets.cpp    assetFolders()    the files it carries (the assets folder): built in
                                              by CMake, unpacked into its private folder as it
                                              starts (PluginAssets.h)

    With PluginInfo.h (its name, maker, ids and kind), and the DSP: graph.json as
    stella/stella_graph.cpp, and its modules (modules folder).
*/
namespace PluginDesign
{
    /** The window, as designed in the Plugins Creator Studio's Edit UI. */
    GuiLayout window();

    /** The plugin's factory presets: each one's name and values, by parameter id. */
    std::vector<Preset> factoryPresets();

    /** One folder of what the plugin carries, as CMake built it in (juce_add_binary_data). */
    struct AssetFolder
    {
        const char* folder;                                    // where it's unpacked, in the private folder
        const char* const* names;                              // its namespace's namedResourceList
        int count;
        const char* (*data) (const char* resourceName, int& size);
        const char* (*fileName) (const char* resourceName);   // the file's own name
    };

    std::vector<AssetFolder> assetFolders();
}
