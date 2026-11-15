// D:\workspace\Fanan_web_hub\src\kit\kit_Preferences.h

#pragma once

#include <juce_core/juce_core.h>

#include "PluginInfo.h"

//==============================================================================
/**
    The plugin's places on the computer, under the names the window's code asks for them by
    (SettingsFolder and UserData are Sonify's names: the window's files are Sonify's own).

        SettingsFolder   the plugin's private folder: <application data>\<vendor>\<plugin>
                         (Windows: %APPDATA%; Mac: ~/Library; Linux: ~/.config). What the plugin
                         carries is unpacked there as it starts (PluginAssets.h): its window's
                         pictures and looks, its factory presets, the shapes and textures its
                         looks draw from. Nothing in it is the user's: it can be deleted.
        UserData         Documents\Sonify, Sonify's own: the user's presets of the plugin are
                         Documents\Sonify\Presets\<plugin>, shared with Sonify (the plugin made
                         in its Plugins Creator Studio, and the one on its tracks).
*/
namespace SettingsFolder
{
    inline juce::File get()
    {
        static const juce::File folder = []
        {
            const auto vendor = juce::String::fromUTF8 (PluginInfo::vendor).trim();
            const auto made = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                  .getChildFile (juce::File::createLegalFileName (vendor.isNotEmpty() ? vendor : juce::String ("Made in Sonify")))
                                  .getChildFile (juce::File::createLegalFileName (juce::String::fromUTF8 (PluginInfo::name)));
            made.createDirectory();
            return made;
        }();

        return folder;
    }

    inline juce::File file (const juce::String& name, const juce::String& = {})
    {
        return get().getChildFile (name);
    }

    /** A folder there (made when asked for): the galleries the editors open, never used by
        the playing window. */
    inline juce::File folder (const juce::String& name, const juce::String& = {})
    {
        const auto target = get().getChildFile (name);
        target.createDirectory();
        return target;
    }
}

namespace UserData
{
    inline juce::File getDefaultFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sonify");
    }

    inline juce::File getFolder()    { return getDefaultFolder(); }
}
