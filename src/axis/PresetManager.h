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
//  the module in each slot) and, beside the values, the impulse response the
//  cabinet had loaded, when it had one (a pack's by its name, "pack:<name>",
//  or a file's by its path).  Sonify reads the values and leaves that alone.
//
//  factoryRigs() are the six AXIS comes with.  Each says only what it changes
//  from the pedals' own settings, so a rig made for an older AXIS still sounds
//  as it was meant to when a pedal gains a control.
//==============================================================================
#include "Parameters.h"
#include "../SonifyPresets.h"   // Sonify's one copy of it, shared by every plugin of its own

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

        /** A whole rig: the pedals' settings, their order, and the cabinet's impulse response. */
        struct Rig
        {
            juce::String name;
            std::map<juce::String, float> values;
            std::array<int, numModules> order { 0, 1, 2, 3, 4 };
            juce::String cabIr;                         // "pack:<name>", a file's path, or "" (none)
        };

        static juce::Result save (const juce::File& file, const Rig& rig)
        {
            auto values = rig.values;

            for (int slot = 0; slot < numModules; ++slot)
                values[chainId (slot)] = (float) rig.order[(size_t) slot];

            juce::DynamicObject::Ptr extras (new juce::DynamicObject());
            extras->setProperty ("cabIr", rig.cabIr);

            return sonify::presets::save (file, pluginName, rig.name, values, juce::var (extras.get()));
        }

        /** The rigs AXIS comes with: a starting point for the things a guitar board is usually
            set up for, one for each of the AMP's five amps and one for bass. Each lists only what
            it changes; everything else stays as the pedals' own settings have it. */
        static const std::vector<Rig>& factoryRigs()
        {
            using namespace axis::pid;

            // The AMP's amps and the cabinets, by their place in AMP_VOICE and CAB_TYPE.
            constexpr float clean = 0.0f, crunch = 1.0f, chime = 2.0f, lead = 3.0f, modern = 4.0f;
            constexpr float open1x12 = 0.0f, alnico2x12 = 1.0f, vintage4x12 = 2.0f, modern4x12 = 3.0f;

            static const std::vector<Rig> rigs
            {
                // Clean: the American clean on an open 1x12, the compressor doing the work, a small plate.
                { "Clean", { { gateThresh, -54.0f }, { gateDepth, 70.0f },
                             { compCircuit, 0.0f }, { compThresh, -22.0f }, { compRatio, 3.0f }, { compMakeup, 7.0f },
                             { compAttack, 18.0f }, { compRelease, 320.0f },
                             { drvBypass, 1.0f },
                             { ampVoice, clean }, { ampGain, 42.0f }, { ampBass, 55.0f }, { ampMid, 45.0f },
                             { ampTreble, 60.0f }, { ampPresence, 50.0f }, { ampMaster, 50.0f }, { ampBright, 1.0f },
                             { cabType, open1x12 },
                             { spcType, 0.0f }, { spcDecay, 32.0f }, { spcSize, 40.0f }, { spcMix, 18.0f } } },

                // Crunch: the British plexi at the edge, bright on, a vintage 4x12, a spring behind it.
                { "Crunch", { { gateThresh, -46.0f },
                              { compThresh, -14.0f }, { compRatio, 2.5f }, { compMakeup, 4.0f },
                              { drvBypass, 1.0f },
                              { ampVoice, crunch }, { ampGain, 62.0f }, { ampBass, 50.0f }, { ampMid, 60.0f },
                              { ampTreble, 58.0f }, { ampPresence, 55.0f }, { ampMaster, 55.0f }, { ampBright, 1.0f },
                              { cabType, vintage4x12 },
                              { spcType, 2.0f }, { spcDecay, 30.0f }, { spcSize, 34.0f }, { spcMix, 16.0f } } },

                // Lead: the British lead pushed by the drive, mids up, tight, a modern 4x12, a hall to sing into.
                { "Lead", { { gateThresh, -40.0f }, { gateDepth, 94.0f }, { gateRelease, 120.0f },
                            { compCircuit, 1.0f }, { compThresh, -20.0f }, { compRatio, 6.0f }, { compMakeup, 8.0f },
                            { drvVoice, 2.0f }, { drvGain, 30.0f }, { drvTone, 54.0f }, { drvLevel, 62.0f },
                            { drvBody, 40.0f }, { drvBite, 60.0f },
                            { ampVoice, lead }, { ampGain, 68.0f }, { ampBass, 45.0f }, { ampMid, 66.0f },
                            { ampTreble, 60.0f }, { ampPresence, 58.0f }, { ampMaster, 55.0f }, { ampTight, 1.0f },
                            { cabType, modern4x12 },
                            { spcType, 1.0f }, { spcPredly, 36.0f }, { spcDecay, 62.0f }, { spcSize, 70.0f },
                            { spcWidth, 90.0f }, { spcMix, 30.0f } } },

                // Modern: the tight modern high gain, the gate firm, a modern 4x12, almost no room.
                { "Modern", { { gateThresh, -38.0f }, { gateDepth, 96.0f }, { gateRelease, 90.0f },
                              { compBypass, 1.0f }, { drvBypass, 1.0f },
                              { ampVoice, modern }, { ampGain, 62.0f }, { ampBass, 55.0f }, { ampMid, 45.0f },
                              { ampTreble, 60.0f }, { ampPresence, 60.0f }, { ampMaster, 55.0f }, { ampTight, 1.0f },
                              { cabType, modern4x12 }, { cabMicX, 28.0f },
                              { spcType, 0.0f }, { spcDecay, 22.0f }, { spcSize, 30.0f }, { spcMix, 10.0f } } },

                // Ambient: the British chime barely broken up on an alnico 2x12, a long shimmering hall
                // that ducks under the playing.
                { "Ambient", { { gateThresh, -62.0f }, { gateDepth, 60.0f },
                               { compThresh, -24.0f }, { compRatio, 2.0f }, { compMakeup, 5.0f },
                               { drvBypass, 1.0f },
                               { ampVoice, chime }, { ampGain, 36.0f }, { ampBass, 48.0f }, { ampMid, 50.0f },
                               { ampTreble, 62.0f }, { ampPresence, 60.0f }, { ampMaster, 50.0f },
                               { cabType, alnico2x12 },
                               { spcType, 1.0f }, { spcPredly, 60.0f }, { spcDecay, 86.0f }, { spcSize, 88.0f },
                               { spcDamp, 44.0f }, { spcWidth, 100.0f }, { spcShimmer, 1.0f }, { spcDuck, 1.0f },
                               { spcMix, 52.0f } } },

                // Bass: a firm VCA, the American amp barely warm, and no guitar cabinet (it would take
                // the bottom string's fundamental away), almost no room.
                { "Bass", { { gateThresh, -58.0f }, { gateScHpf, 60.0f },
                            { compCircuit, 2.0f }, { compThresh, -20.0f }, { compRatio, 5.0f }, { compMakeup, 6.0f },
                            { compAttack, 8.0f }, { compRelease, 180.0f }, { compScHpf, 1.0f },
                            { drvVoice, 0.0f }, { drvGain, 30.0f }, { drvTone, 36.0f }, { drvLevel, 60.0f },
                            { drvBody, 64.0f }, { drvBite, 40.0f }, { drvMix, 70.0f },
                            { ampVoice, clean }, { ampGain, 34.0f }, { ampBass, 66.0f }, { ampMid, 55.0f },
                            { ampTreble, 45.0f }, { ampPresence, 40.0f }, { ampMaster, 45.0f }, { ampCab, 0.0f },
                            { spcDecay, 20.0f }, { spcSize, 26.0f }, { spcMix, 8.0f } } }
            };

            return rigs;
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
            rig.cabIr = read->whole.getProperty ("cabIr", {}).toString();

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
