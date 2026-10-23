// D:\workspace\Fanan_web_hub\src\AiTools.h

#pragma once

#include "AudioEngine.h"
#include "Library.h"

#include <juce_core/juce_core.h>

#include <functional>

//==============================================================================
/**
    The tools the AI drives the Hub with: each a small action on the song, in the shape
    Claude's tool use expects (a name, a description, a JSON schema for its input).

    Positions are in bars (from 1) and beats (quarter notes), volumes in dB, pan -1..1,
    plugin parameters in each plugin's own range. Plugins are found in the user's library
    by name: the model never loads a file it made up. Removing a track needs confirmed:
    true, which the model only sends after the user said yes.

    Everything runs on the message thread, as the page's own commands do.
*/
class AiTools final
{
public:
    /** What the Hub itself does around the engine. */
    struct Hooks
    {
        std::function<bool (int channel, int slot)> openWindow;
        std::function<juce::Result()> saveSong;
        std::function<juce::String (int trackId, int clipId, const juce::StringArray& stems, bool muteOriginal)> splitStems;   // "" when it starts
    };

    AiTools (AudioEngine& engine, Library& library, Hooks hooks);

    /** The tool list for the model: [{ name, description, input_schema }]. */
    static juce::var definitions();

    /** Runs one tool: its result, or { error } when it couldn't. */
    juce::var run (const juce::String& name, const juce::var& input);

    /** One line on what a call did, for the page's list of changes ("" for a look-only tool). */
    static juce::String describe (const juce::String& name, const juce::var& input, const juce::var& result);

private:
    juce::var getSong();
    juce::var getClip (const juce::var& input);
    juce::var findPlugins (const juce::var& input);
    juce::var addTrack (const juce::var& input);
    juce::var removeTrack (const juce::var& input);
    juce::var setTrack (const juce::var& input);
    juce::var setMain (const juce::var& input);
    juce::var loadPlugin (const juce::var& input, bool instrument);
    juce::var removeEffect (const juce::var& input);
    juce::var moveEffect (const juce::var& input);
    juce::var bypassEffect (const juce::var& input);
    juce::var getPluginParams (const juce::var& input);
    juce::var setPluginParams (const juce::var& input);
    juce::var addFxChannel (const juce::var& input);
    juce::var addSend (const juce::var& input, bool sidechain);
    juce::var setSend (const juce::var& input);
    juce::var removeSend (const juce::var& input);
    juce::var writeMidiClip (const juce::var& input);
    juce::var deleteClip (const juce::var& input);
    juce::var setTempo (const juce::var& input);
    juce::var setTimeSignature (const juce::var& input);
    juce::var setKey (const juce::var& input);
    juce::var transport (const juce::var& input);
    juce::var addSessionPlayers (const juce::var& input);
    juce::var splitStems (const juce::var& input);
    juce::var addMidiFx (const juce::var& input);
    juce::var setMidiFx (const juce::var& input);
    juce::var removeMidiFx (const juce::var& input);
    juce::var applyMidiTool (const juce::var& input);
    juce::var getBoard();
    juce::var setBoard (const juce::var& input);
    static juce::String nameOfMidiFx (const juce::String& type);

    // The channel a tool names: "main" (or 0) is Main, a number is a track or FX channel.
    int channelOf (const juce::var& value, juce::String& error) const;
    // The plugin slot on a channel: "instrument" (or 0), or an insert's id.
    Plugin* pluginAt (int channel, const juce::var& slot, int& slotId, juce::String& error);
    const Library::Entry* findEntry (const juce::var& input, const juce::String& wantKind, juce::String& error) const;
    double barToPpq (double bar) const;  // bars from 1 (fractions are part of that bar), through the time signatures
    double ppqToBar (double ppq) const;

    AudioEngine& engine;
    Library& library;
    Hooks hooks;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiTools)
};
