// D:\workspace\Fanan_web_hub\src\kit\kit_PluginAssets.h

#pragma once

#include <juce_core/juce_core.h>

#include "PluginDesign.h"
#include "Preferences.h"

//==============================================================================
/**
    What the plugin carries, unpacked into its private folder (SettingsFolder) the first time
    any copy of it starts in a session, where the window's code reads it as Sonify reads a
    project's files:

        gui\images        its pictures (picture elements, painted backgrounds, look pictures)
        gui\looks         its own looks (knobs, sliders, switches it was given in the studio)
        data\Looks        looks it wears that were the maker's own ("My looks")
        data\Shapes       the shapes its looks draw from (the Shapes inventory)
        data\Textures     the textures its looks are textured with
        presets.json      its factory presets (PluginPresets.cpp)

    A file is written only when it isn't there as it should be, so a second copy of the plugin
    (another track) writes nothing.
*/
namespace PluginAssets
{
    inline juce::File guiFolder()      { return SettingsFolder::get().getChildFile ("gui"); }
    inline juce::File presetsFile()    { return SettingsFolder::get().getChildFile (PresetBank::fileName); }

    inline void unpack()
    {
        static const bool unpacked = []
        {
            auto writeIfChanged = [] (const juce::File& target, const void* data, size_t size)
            {
                if (target.getSize() == (juce::int64) size)
                {
                    juce::MemoryBlock existing;

                    if (target.loadFileAsData (existing) && existing.getSize() == size && std::memcmp (existing.getData(), data, size) == 0)
                        return;
                }

                target.getParentDirectory().createDirectory();
                target.replaceWithData (data, size);
            };

            for (const auto& folder : PluginDesign::assetFolders())
            {
                const auto into = SettingsFolder::get().getChildFile (folder.folder);

                for (int i = 0; i < folder.count; ++i)
                {
                    int size = 0;

                    if (const auto* data = folder.data (folder.names[i], size); data != nullptr && size >= 0)
                        writeIfChanged (into.getChildFile (juce::String::fromUTF8 (folder.fileName (folder.names[i]))), data, (size_t) size);
                }
            }

            // The factory presets, where PresetLibrary reads them (presets.json, as PresetBank writes it).
            juce::Array<juce::var> list;

            for (const auto& preset : PluginDesign::factoryPresets())
                list.add (PresetBank::toVar (preset));

            juce::DynamicObject::Ptr json (new juce::DynamicObject());
            json->setProperty ("format", 1);
            json->setProperty ("presets", list);
            const auto text = juce::JSON::toString (juce::var (json.get()));

            writeIfChanged (presetsFile(), text.toRawUTF8(), text.getNumBytesAsUTF8());
            return true;
        }();

        juce::ignoreUnused (unpacked);
    }
}
