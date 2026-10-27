#pragma once
//==============================================================================
//  PresetManager.h  -  AXIS rigs in Sonify's own preset format.
//
//  A rig is one .sonifypreset file in Sonify's own presets folder (the files,
//  the folder and the format are SonifyPresets.h, the same in every Sonify
//  plugin):
//
//      <Sonify's user data>\Presets\AXIS\<name>.sonifypreset
//
//  (Documents\Sonify\Presets\AXIS unless Settings > Locations says otherwise)
//  - the very folder Sonify's own preset manager reads, so a rig saved on the
//  pedalboard shows there, and one saved there shows here.
//
//  A rig holds every pedal's settings (each parameter by its own id, the same
//  ids the web UI binds to), the order the pedals are in (CHAIN_1 .. CHAIN_5:
//  the module in each slot) and, beside the values, the capture the AMP had
//  loaded.  Sonify reads the values and leaves the capture alone.
//==============================================================================
#include "Parameters.h"
#include "SonifyPresets.h"

#include <array>
#include <map>
#include <optional>

namespace axis
{
    struct PresetManager
    {
        static constexpr const char* pluginName = "AXIS";
        static constexpr const char* extension = sonify::presets::extension;

        /** The value id for one slot of the chain. */
        static juce::String chainId (int slot)          { return "CHAIN_" + juce::String (slot + 1); }

        static juce::File folder()                      { return sonify::presets::folderFor (pluginName); }
        static juce::File fileFor (const juce::String& name)   { return sonify::presets::fileFor (pluginName, name); }
        static juce::String cleanName (const juce::String& n)  { return sonify::presets::cleanName (n); }
        static juce::Array<juce::File> userPresets()    { return sonify::presets::list (pluginName); }
        static void showFolder()                        { sonify::presets::showFolder (pluginName); }

        /** A whole rig: the pedals' settings, their order, and the AMP's capture. */
        struct Rig
        {
            juce::String name;
            std::map<juce::String, float> values;
            std::array<int, numModules> order { 0, 1, 2, 3, 4 };
            juce::String capture;                       // the .nam file's path ("": none)
        };

        static juce::Result save (const juce::File& file, const Rig& rig)
        {
            auto values = rig.values;

            for (int slot = 0; slot < numModules; ++slot)
                values[chainId (slot)] = (float) rig.order[(size_t) slot];

            juce::DynamicObject::Ptr extras (new juce::DynamicObject());
            extras->setProperty ("capture", rig.capture);

            return sonify::presets::save (file, pluginName, rig.name, values, juce::var (extras.get()));
        }

        /** Nothing when the file isn't an AXIS rig. */
        static std::optional<Rig> load (const juce::File& file)
        {
            const auto read = sonify::presets::load (file);

            if (! read.has_value())
                return std::nullopt;

            // A preset another plugin saved is not AXIS's to load.
            if (read->plugin.isNotEmpty() && ! read->plugin.equalsIgnoreCase (pluginName))
                return std::nullopt;

            Rig rig;
            rig.name = read->name;
            rig.capture = read->whole.getProperty ("capture", {}).toString();

            // Every slot of the chain, then the pedals' own values.
            bool seen[numModules] = {};

            for (int slot = 0; slot < numModules; ++slot)
            {
                const auto found = read->values.find (chainId (slot));
                const int module = found != read->values.end()
                                     ? juce::jlimit (0, numModules - 1, (int) found->second) : slot;

                rig.order[(size_t) slot] = module;
            }

            // A file with a slot twice in it (hand-edited, or from an older AXIS) is put straight:
            // every module exactly once, in the order the file asked for as far as it makes sense.
            std::array<int, numModules> straight {};
            int next = 0;

            for (int slot = 0; slot < numModules; ++slot)
            {
                const int module = rig.order[(size_t) slot];

                if (! seen[module])
                {
                    seen[module] = true;
                    straight[(size_t) next++] = module;
                }
            }

            for (int module = 0; module < numModules && next < numModules; ++module)
                if (! seen[module])
                    straight[(size_t) next++] = module;

            rig.order = straight;

            for (const auto& [id, value] : read->values)
                if (! id.startsWith ("CHAIN_"))
                    rig.values[id] = value;

            return rig;
        }
    };
}
