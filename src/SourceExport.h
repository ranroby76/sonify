// D:\workspace\Fanan_web_hub\src\SourceExport.h

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GuiLayout.h"
#include "Looks.h"
#include "Project.h"

#include <vector>

//==============================================================================
/**
    The plugin's source code, written into its project folder (<project>\source) on every Save
    in the Plugins Creator Studio: a JUCE 9 project anyone can read and build into the plugin
    as it is in Sonify (the same sound, the same window, pixel for pixel), with nothing but
    CMake and a C++ compiler.

        CMakeLists.txt     how to build it (at its top: the commands): VST3, CLAP and Standalone
                           (and an Audio Unit on a Mac), everything in the build folder
        plugin\            the plugin: PluginProcessor (the Stella runtime as JUCE's plugin),
                           PluginEditor (the window), and what's this plugin's own, written from
                           the project: PluginInfo.h (its name, maker, ids, kind),
                           PluginWindow.cpp (the window, element by element, every setting
                           spelled out), PluginPresets.cpp (its factory presets),
                           PluginAssets.cpp (what it carries)
        gui\               Sonify's own window code (the Edit UI's canvas, the looks, the 3D
                           renderer...), as Sonify draws the panel; gui\data: its built-in looks,
                           fonts, icons and toolbox primitives
        stella\            the Stella runtime that plays the modules (as Sonify and its exports
                           play them), the graph (graph.json as C++), the programmed elements'
                           runtime
        modules\           the plugin's DSP modules, as the project has them
        elements\          its programmed elements, as the project has them
        assets\            what it carries: its pictures, its looks, the maker's looks it wears
                           and the shapes and textures they draw from; license.soni (protected)
        preview\           pictures of its window, page by page, as it was saved

    Files are written only when they change, and files Sonify wrote before that the plugin no
    longer has are taken away (only in those folders: a build folder inside source is left
    alone).
*/
namespace SourceExport
{
    inline constexpr const char* folderName = "source";

    /** What the export takes from the studio, on the message thread. */
    struct Request
    {
        juce::File projectFolder;
        ProjectInfo info;
        GuiLayout layout;                       // as saved
        std::vector<juce::Image> pages;         // the window as it plays, page by page

        /** Looks it carries besides the built-in ones: (file name, contents). */
        std::vector<std::pair<juce::String, juce::MemoryBlock>> ownLooks;   // the plugin's own (gui\looks)
        std::vector<std::pair<juce::String, juce::MemoryBlock>> myLooks;    // the maker's own it wears
        juce::Array<juce::File> pictures;       // pictures its looks name by a full path (into its pictures)
        juce::Array<juce::File> shapes;         // the Shapes inventory's shapes its looks draw from
        juce::Array<juce::File> textures;       // the textures its looks are textured with
    };

    /** Message thread: everything the export needs, taken from the project, the studio's looks
        and its panel (pages: Workspace::snapshotPages). */
    Request prepare (const juce::File& projectFolder, const ProjectInfo& info, const GuiLayout& layout,
                     Looks& looks, std::vector<juce::Image> pages);

    /** Any thread: writes <project>\source. Takes a second or two the first time (the kit),
        then only what changed. */
    juce::Result write (const Request& request);

    /** The window as C++ (PluginWindow.cpp's text): every element, every setting. Rebuilding
        the layout from it gives the layout itself (GuiLayout::toVar the same). */
    juce::String windowSource (const GuiLayout& layout, const juce::String& pluginName);
}
