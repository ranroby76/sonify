// D:\workspace\Fanan_web_hub\src\Skills.h

#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
/**
    Fanan AI's skills: know-how it reads when a job needs it, the way a musician opens a
    manual at the right page. Every request carries only a short list of them (a name and
    what each is for); read_skill brings one skill's whole text into the request that needs
    it, so the rest cost nothing.

    Each skill is a file src/skill_<name>.md, built into Sonify (CMake gathers them all: a new
    file shows after the next build), starting with its card:

        ---
        name: synth-engines
        for: plugins            plugins (the Plugins Creator Studio's builder) or devices (MIDI devices)
        title: Synth engines    a few words for the chat ("Reading up on synth engines")
        description: ...        one line: what's in it and when to read it
        ---
*/
namespace Skills
{
    enum class For { plugins, devices };

    /** The list for a guide: a heading, how to use them, and one line per skill. Empty if
        there are none for these readers. */
    juce::String index (For readers);

    /** read_skill's definition for the model ({ name, description, input_schema }). */
    juce::var toolDefinition (For readers);

    /** A skill's text (without its card). Fails with the names there are when there's no such
        skill for these readers. */
    juce::Result read (For readers, const juce::String& name, juce::String& text);

    /** A skill's title in lower case, for the chat ("synth engines"); its name if unknown. */
    juce::String titleOf (const juce::String& name);

    /** What a skill lists under its "### " headings, in order: the inspiration pool's looks
        ("inspiration-looks"), for the look picker beside Send. Empty if there's no such skill. */
    juce::StringArray headings (const juce::String& name);
}
