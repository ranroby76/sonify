// D:\workspace\Fanan_web_hub\src\PluginExporter.h

#pragma once

#include <juce_core/juce_core.h>

#include "GuiLayout.h"
#include "PluginCanvas.h"
#include "PluginLibrary.h"
#include "PresetBank.h"
#include "Project.h"

#include <map>

//==============================================================================
/**
    Export (the Plugins Creator Studio's Export button): turns the open plugin into plugin files
    a DAW loads, built on the user's own computer, for the system Sonify runs on:

        Windows   VST3 and CLAP, with the compiler that ships with Sonify ("Plugin Compiler",
                  llvm-mingw, beside Sonify.exe)
        macOS     VST3, Audio Unit and CLAP, universal (Apple silicon and Intel), with Apple's
                  own compiler (the Command Line Tools: when they're missing, macOS's installer
                  is opened); signed for this Mac
        Linux     VST3 and CLAP, with the system's C++ compiler

    The plugins have no JUCE in them: the project's modules, the Stella runtime, the window baked
    from the panel (PluginCanvas::bakeForExport) and a small wrapper per format (stella_vst3.cpp,
    stella_clap.cpp, stella_au.cpp), compiled natively, so they need nothing installed beside
    them. The formats' headers ship with Sonify ("Plugin SDKs": CLAP and VST3's pluginterfaces,
    both MIT licensed).

    They land in Documents\Sonify\Exports\<plugin name> unless another folder is chosen, each
    plugin in a folder of its own, by format:
        VST3\<name>\<name>.vst3 (a bundle folder)
        CLAP\<name>\<name>.clap
        AU\<name>.component (a Mac's)
    "Copy into my plugin folders" copies each plugin's folder the same way (Common\VST3\<name>\
    <name>.vst3); an Audio Unit goes straight into Components, as macOS asks.

    A protected project (Licences::loadProtection: the Protection tab) exports protected: its
    product id and demo times built in (stella_product.h), its license.soni beside each plugin
    (inside an Audio Unit, in Contents/Resources), and its panel's registration badge kept. It
    plays its demo until the computer is registered (stella_plugin.h). Every export can register
    the protected sample libraries it plays, so each is built with the computer's side of
    stella_licence.h (STELLA_LICENCE_NATIVE).

    A sampler's blobs (its sample library) aren't copied: the plugin knows them by their files'
    names and finds them where PluginLibrary says (a buyer puts them in <Documents>\<vendor>\
    <plugin>); on this computer it finds the creator's own, listed for it as it's exported.
*/
class PluginExporter final
{
public:
    enum class Format { vst3, clap, au };

    /** The formats this computer exports, in the order they're offered. */
    static juce::Array<Format> formatsHere();
    static juce::String formatName (Format format);           // "VST3", "CLAP", "Audio Unit"
    static juce::String fileNameFor (Format format, const juce::String& pluginName);

    /** The plugin's window, baked and ready to compile in: pictures as PNG, elements as C++. */
    struct Gui
    {
        int width = 0, height = 0;
        std::vector<juce::MemoryBlock> pages;   // none: the plugin has no window (hosts show its controls)

        struct Strip
        {
            juce::MemoryBlock png;
            int frameWidth = 0, frameHeight = 0, frames = 0;
            bool registration = false;           // the registration badge's: in a protected plugin only
        };

        std::vector<Strip> strips;
        juce::String widgetRows;
        int numWidgets = 0;
        juce::String registrationRows;           // the registration badge, kept apart: a protected plugin's only
        int numRegistration = 0;

        juce::String presetArrays, presetRows;   // the presets, as C++
        int numPresets = 0;

        juce::String customArrays, customRows;   // programmed elements: what each one's widget tells it, as C++
        int numCustom = 0;
        juce::StringArray elementTypes;          // the programmed elements the window uses
        juce::String roleSourceRows;             // ...and what else they watch, by role
        int numRoleSources = 0;

        struct Named
        {
            juce::String name;
            bool bold = false;
            juce::MemoryBlock data;
        };

        std::vector<Named> pictures;             // gui/images, for programmed elements that ask for them
        std::vector<Named> fonts;                // the fonts they're asked to write in
    };

    /** On the message thread: encodes the canvas's bake and describes each element. */
    static Gui prepareGui (const GuiLayout& layout, const PluginCanvas::ExportBake& bake,
                           const std::map<juce::String, juce::String>& unitsById, const std::map<juce::String, juce::String>& namesById,
                           const juce::StringArray& displayIds, const std::vector<Preset>& presets, const juce::File& guiFolder);

    struct Request
    {
        juce::File projectFolder;
        ProjectInfo info;
        juce::File destination;              // the folder the plugin files go into
        juce::Array<Format> formats;
        bool install = false;                // copied into the user's own plugin folders too, where DAWs look
        Gui gui;
        juce::Array<PluginLibrary::Instrument> library;   // a sampler's blobs, in order
    };

    struct Result
    {
        bool ok = false;
        juce::String log;                    // the compiler's messages, or what went wrong
        juce::Array<juce::File> files;       // what was made, when ok (each in its format's folder: VST3\<name>\...)
        juce::File folder;                   // where they all are (the folder chosen)
        bool isProtected = false;            // built protected, each with its license.soni
        juce::StringArray installed;         // where they were copied, when asked
        double seconds = 0.0;
    };

    /** Builds the plugins, each format at once. Takes a while: call it on a worker thread. */
    static Result exportPlugins (const Request& request);

    static juce::File defaultExportFolder (const juce::String& pluginName);

    /** Where "copy into my plugin folders" puts them, said plainly for this system. */
    static juce::String installFoldersText();

    /** Empty when this computer can build plugins; otherwise what's missing, said plainly (on a
        Mac, Apple's installer for its compiler is opened). */
    static juce::String compilerProblem();

    /** The formats' headers that ship with Sonify, or empty Files if missing. */
    static juce::File findClapHeaders();
    static juce::File findVst3Headers();   // the folder holding pluginterfaces/
};
