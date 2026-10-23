// D:\workspace\Fanan_web_hub\src\stella_element_host.h
//
// What programmed GUI elements (stella_element_api.h) ask of whoever shows them: the
// studio, through WebAssembly, or an exported plugin's own GUI. element is the shower's
// own number for the element (its place in the layout). A slot is one of the element's
// parameters: 0 its own ("param"), 1 and up its roles ("params"), as findSlot gives them.

#pragma once

#include <cstdint>
#include <string>

namespace stella::ui
{
    class ElementHost
    {
    public:
        virtual ~ElementHost() = default;

        enum Info { valueInfo = 0, minInfo = 1, maxInfo = 2, defaultInfo = 3, skewInfo = 4 };

        /** A parameter's value or range; false if the slot has no parameter. */
        virtual bool paramInfo (int element, int slot, int info, float& result) = 0;
        virtual int findSlot (int element, const std::string& role) = 0;                  // -1: none
        virtual void setValue (int element, int slot, float newValue) = 0;
        virtual void gesture (int element, int slot, bool starts) = 0;                    // a drag starts or ends

        enum Text { labelText = 0, optionText = 1, paramNameText = 2, unitText = 3, settingText = 4, sourceText = 5,
                    presetNameText = 6, presetGroupText = 7 };

        /** option: index is which one; paramName and unit: index is the slot; setting: key
            names it; source: key is the role ("" its own), the result what it watches; preset
            name and group: index is the preset. False if there's none. */
        virtual bool text (int element, int which, int index, const std::string& key, std::string& result) = 0;
        virtual int numOptions (int element) = 0;
        virtual std::uint32_t colour (int element) = 0;                                   // 0: none given

        virtual float level (int element, bool rms) = 0;
        virtual void readScope (int element, float* destination, int numSamples) = 0;

        /** The same by role (the layout's "sources"; "" is the element's own "source"). */
        virtual float sourceLevel (int element, const std::string& role, bool rms)
        {
            return role.empty() ? level (element, rms) : 0.0f;
        }

        virtual void readSourceScope (int element, const std::string& role, float* destination, int numSamples)
        {
            if (role.empty())
                readScope (element, destination, numSamples);
            else
                for (int i = 0; i < numSamples; ++i)
                    destination[i] = 0.0f;
        }

        /** The presets (the preset manager's): how many, the chosen one (-1: none), whether its
            values changed since, whether a PresetCommand (index) can be done now, and a number
            that changes whenever any of them does. */
        enum PresetInfo { presetCountInfo = 0, presetCurrentInfo = 1, presetModifiedInfo = 2, presetCanDoInfo = 3, presetVersionInfo = 4 };

        virtual int presetInfo (int /*element*/, int which, int /*index*/)    { return which == presetCurrentInfo ? -1 : 0; }

        /** Choose a preset (index), step (index: +1 or -1), or a PresetCommand (index). */
        enum PresetTask { choosePresetTask = 0, stepPresetTask = 1, presetCommandTask = 2 };

        virtual void presetTask (int /*element*/, int /*task*/, int /*index*/) {}

        /** A picture of the plugin's by file name: its own size (false: there's none), and its
            pixels scaled to width x height (premultiplied 0xAARRGGBB, row by row from the top). */
        virtual bool pictureSize (int /*element*/, const std::string& /*name*/, int& /*width*/, int& /*height*/)   { return false; }

        virtual bool readPicture (int /*element*/, const std::string& /*name*/, int /*width*/, int /*height*/, std::uint32_t* /*destination*/)
        {
            return false;
        }

        /** A font the elements' text may be drawn in, by name (an element's "font" setting): the
            size of its file in bytes (0: there's no such font), and the file itself copied to
            destination when capacity is that size. bold: its bold (0 when it has none: the
            regular one stands in). */
        virtual int fontData (const std::string& /*name*/, bool /*bold*/, std::uint8_t* /*destination*/, int /*capacity*/)   { return 0; }

        virtual void playNote (int element, int note, float velocity) = 0;
        virtual bool isNoteDown (int note) = 0;
        virtual double seconds() = 0;
    };
}
