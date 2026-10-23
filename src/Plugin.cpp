// D:\workspace\Fanan_web_hub\src\Plugin.cpp

#include "Plugin.h"

#include "BuiltinPlugins.h"
#include "CreatorPlugin.h"
#include "Vst3Plugin.h"
#include "WclapPlugin.h"

bool Plugin::isVst3 (const juce::File& file)
{
    return file.getFileName().endsWithIgnoreCase (".vst3");
}

std::unique_ptr<Plugin> Plugin::load (const juce::File& file, const juce::String& pluginId, juce::String& error)
{
    // Sonify's own effects are known by their ids, wherever the file says they were.
    if (Builtins::isBuiltinId (pluginId))
        return Builtins::create (pluginId, error);

    // Made in the Plugins Creator Studio: a folder of its own.
    if (MadePlugins::isMadeFile (file.getFullPathName()))
        return CreatorPlugin::load (file, error);

    if (isVst3 (file))
        return Vst3Plugin::load (file, pluginId, error);

    return WclapPlugin::load (file, pluginId, error);
}

std::vector<Plugin::Descriptor> Plugin::describe (const juce::File& file, juce::String& error)
{
    if (Builtins::isBuiltinFile (file.getFullPathName()))
        return Builtins::describe();

    if (MadePlugins::isMadeFile (file.getFullPathName()))
    {
        auto made = MadePlugins::describe (file);

        if (made.empty())
            error = "It isn't a plugin made in Sonify, or its plugin.json is damaged.";

        return made;
    }

    if (isVst3 (file))
        return Vst3Plugin::describe (file, error);

    // A WCLAP lists its plugins as it loads.
    const auto plugin = WclapPlugin::load (file, {}, error);
    return plugin != nullptr ? plugin->getPluginsInFile() : std::vector<Descriptor> {};
}

//==============================================================================
namespace
{
    /** JUCE's name for a format Sonify hosts through JUCE. */
    juce::String juceFormatName (const juce::String& format)
    {
        if (format == "VST3")  return "VST3";
        if (format == "VST2")  return "VST";
        if (format == "AU")    return "AudioUnit";
        if (format == "LV2")   return "LV2";
        return {};
    }
}

std::unique_ptr<Plugin> Plugin::loadFormat (const juce::File& file, const juce::String& format, const juce::String& pluginId, juce::String& error)
{
    if (! isFormatAvailable (format))
    {
        error = format + " plugins can't be loaded in this Sonify.";
        return nullptr;
    }

    if (! file.exists())
    {
        error = file.getFullPathName() + " isn't there.";
        return nullptr;
    }

    if (format == "CLAP")
        return WclapPlugin::loadNative (file, pluginId, error);

    return Vst3Plugin::loadAs (file, juceFormatName (format), format, pluginId, error);
}

std::vector<Plugin::Descriptor> Plugin::describeFormat (const juce::File& file, const juce::String& format, juce::String& error)
{
    if (! isFormatAvailable (format))
    {
        error = format + " plugins can't be loaded in this Sonify.";
        return {};
    }

    if (format == "CLAP")
    {
        const auto plugin = WclapPlugin::loadNative (file, {}, error);
        return plugin != nullptr ? plugin->getPluginsInFile() : std::vector<Descriptor> {};
    }

    return Vst3Plugin::describeAs (file, juceFormatName (format), format, error);
}

bool Plugin::isFormatAvailable (const juce::String& format)
{
    if (format == "VST3" || format == "CLAP")
        return true;

   #if JUCE_PLUGINHOST_VST
    if (format == "VST2")
        return true;
   #endif

   #if JUCE_MAC && JUCE_PLUGINHOST_AU
    if (format == "AU")
        return true;
   #endif

   #if (JUCE_LINUX || JUCE_BSD) && JUCE_PLUGINHOST_LV2
    if (format == "LV2")
        return true;
   #endif

    return false;
}

juce::StringArray Plugin::formatsForThisComputer()
{
   #if JUCE_MAC
    return { "VST2", "VST3", "CLAP", "AU" };
   #elif JUCE_LINUX
    return { "VST2", "VST3", "CLAP", "LV2" };
   #else
    return { "VST2", "VST3", "CLAP" };
   #endif
}

juce::File Plugin::defaultFolderFor (const juce::String& format)
{
    std::vector<juce::File> places;
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

   #if JUCE_WINDOWS
    const auto programs = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory);   // C:\Program Files
    const auto common = programs.getChildFile ("Common Files");

    if (format == "VST2")
    {
        places.push_back (programs.getChildFile ("vstplugins"));
        places.push_back (programs.getChildFile ("Steinberg").getChildFile ("VSTPlugins"));
        places.push_back (common.getChildFile ("VST2"));
    }
    else if (format == "VST3")
    {
        places.push_back (common.getChildFile ("VST3"));
    }
    else if (format == "CLAP")
    {
        places.push_back (common.getChildFile ("CLAP"));
        places.push_back (juce::File::getSpecialLocation (juce::File::windowsLocalAppData).getChildFile ("Programs").getChildFile ("Common").getChildFile ("CLAP"));
    }

    places.push_back (programs);
   #elif JUCE_MAC
    const juce::String folder = format == "VST2" ? "VST" : format == "VST3" ? "VST3" : format == "CLAP" ? "CLAP" : "Components";
    places.push_back (juce::File ("/Library/Audio/Plug-Ins").getChildFile (folder));
    places.push_back (home.getChildFile ("Library/Audio/Plug-Ins").getChildFile (folder));
    places.push_back (juce::File ("/Library/Audio/Plug-Ins"));
   #else
    const juce::String hidden = format == "VST2" ? ".vst" : format == "VST3" ? ".vst3" : format == "CLAP" ? ".clap" : ".lv2";
    const juce::String shared = format == "VST2" ? "vst" : format == "VST3" ? "vst3" : format == "CLAP" ? "clap" : "lv2";
    places.push_back (home.getChildFile (hidden));
    places.push_back (juce::File ("/usr/lib").getChildFile (shared));
    places.push_back (juce::File ("/usr/local/lib").getChildFile (shared));
   #endif

    for (const auto& place : places)
        if (place.isDirectory())
            return place;

    return home;
}

bool Plugin::looksLikePlugin (const juce::File& file, const juce::String& format)
{
    const auto name = file.getFileName().toLowerCase();

    if (format == "VST3")
        return name.endsWith (".vst3");

    if (format == "CLAP")
        return name.endsWith (".clap");

    if (format == "AU")
        return name.endsWith (".component");

    if (format == "LV2")
        return name.endsWith (".lv2") && file.isDirectory();

    if (format == "VST2")
    {
       #if JUCE_WINDOWS
        return name.endsWith (".dll") && ! file.isDirectory();
       #elif JUCE_MAC
        return name.endsWith (".vst");
       #else
        return name.endsWith (".so") && ! file.isDirectory();
       #endif
    }

    return false;
}
