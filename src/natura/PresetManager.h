// C:\workspace\SONIFY PLUGIN natura\src\PresetManager.h
#pragma once
//==============================================================================
//  PresetManager.h  -  Natura presets in Sonify's own preset format.
//
//  The files, the folder and the format are Sonify's own SonifyPresets.h (one
//  copy, shared by every plugin of Sonify's own): one .sonifypreset file in
//
//      <Sonify's user data>\Presets\Natura\<name>.sonifypreset
//
//  (Documents\Sonify\Presets\Natura unless Settings > Locations says otherwise)
//  - the very folder Sonify's own preset manager reads, so a preset saved here
//  shows there, and one saved there shows here.  Presets Natura kept in AppData
//  before this are still listed; saving writes to the folder above.
//
//  This is Natura's side of it: its parameters by id, and loading from the
//  preset's archetype root upwards, so a preset from an older Natura (fewer
//  parameters) still loads as it was meant to sound.
//==============================================================================
#include "Archetypes.h"
#include "../SonifyPresets.h"   // Sonify's one copy of it, shared by every plugin of its own

namespace natura
{
    struct PresetManager
    {
        static constexpr const char* pluginName = "Natura";
        static constexpr const char* extension = sonify::presets::extension;

        static juce::File folder()                 { return sonify::presets::folderFor (pluginName); }
        /** Natura's editor names the file after the name, so this is safe for both. */
        static juce::String cleanName (juce::String n)
        {
            return sonify::presets::legalName (sonify::presets::cleanName (n));
        }

        static juce::Result save (const juce::File& file, const juce::String& name, const std::vector<float>& values)
        {
            std::map<juce::String, float> byId;

            for (int i = 0; i < kNumParams; ++i)
                byId[info (i).id] = values[(size_t) i];

            return sonify::presets::save (file, pluginName, name, byId);
        }

        struct Loaded { juce::String name; std::vector<float> values; };

        static std::optional<Loaded> load (const juce::File& file)
        {
            const auto read = sonify::presets::load (file);

            if (! read.has_value())
                return std::nullopt;

            // A preset another plugin saved is not Natura's to load.
            if (read->plugin.isNotEmpty() && ! read->plugin.equalsIgnoreCase (pluginName))
                return std::nullopt;

            int arch = 0;
            if (const auto found = read->values.find ("archetype"); found != read->values.end())
                arch = juce::jlimit (0, kNumArchetypes - 1, (int) found->second);

            Loaded out;
            out.name = read->name;
            out.values = rootValues ((Archetype) arch);

            for (const auto& [id, value] : read->values)
            {
                const int idx = findParam (id);

                if (idx < 0)
                    continue;

                const auto& in = info (idx);
                out.values[(size_t) idx] = juce::jlimit (in.minV, in.maxV, value);
            }

            return out;
        }

        static juce::Array<juce::File> userPresets()   { return sonify::presets::list (pluginName); }
    };
}
