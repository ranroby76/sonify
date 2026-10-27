// D:\workspace\Fanan_web_hub\src\AiTools.h

#pragma once

#include "AudioEngine.h"
#include "Library.h"

#include <juce_core/juce_core.h>

#include <functional>
#include <set>
#include <utility>

//==============================================================================
/**
    The tools the AI drives the Hub with: each a small action on the song, in the shape
    Claude's tool use expects (a name, a description, a JSON schema for its input).

    Positions are in bars (from 1) and beats (quarter notes), volumes in dB, pan -1..1,
    plugin parameters in each plugin's own range. Plugins are found in the user's library
    by name: the model never loads a file it made up. Removing a track or deleting a clip the
    user made waits for the user's own Yes in Sonify's question box (runAsync): the model's
    word isn't enough, so text in a song (a track's or a file's name) can't talk it into it.
    What Fanan AI made itself in this session it may take away again without asking.

    Beyond its own tools it reaches everything Sonify's screen does: the page's commands
    (run_app_command) and the events the page sends (send_app_event, sonify_app_events.md lists
    them). One that removes, deletes, clears or closes anything waits for the user's Yes too.

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

        /** Sonify's own question box: answer (on the message thread) true only for the user's Yes. */
        std::function<void (const juce::String& title, const juce::String& message, const juce::String& yesText,
                            std::function<void (bool yes)> answer)> confirm;

        /** The page's commands (the menus' and keys' actions), by name. */
        std::function<juce::StringArray()> pageCommands;

        /** Runs a page command (command), or sends an event as the page would (event, payload). */
        std::function<void (const juce::String& command, const juce::String& event, const juce::var& payload)> runInPage;
    };

    AiTools (AudioEngine& engine, Library& library, Hooks hooks);

    /** The tool list for the model: [{ name, description, input_schema }]. */
    static juce::var definitions();

    /** Runs one tool: its result, or { error } when it couldn't. */
    juce::var run (const juce::String& name, const juce::var& input);

    /** As run, with done called (on the message thread) once it's done: remove_track and
        delete_clip on something the user made ask the user first (Hooks::confirm). */
    void runAsync (const juce::String& name, const juce::var& input, std::function<void (const juce::var&)> done);

    /** Another song (opened, new, closed, or one brought back by undo): what Fanan AI made is
        the user's from now on. */
    void forgetWhatItMade()     { madeTracks.clear(); madeClips.clear(); }

    /** One line on what a call did, for the page's list of changes ("" for a look-only tool). */
    static juce::String describe (const juce::String& name, const juce::var& input, const juce::var& result);

private:
    juce::var getSong();
    juce::var getClip (const juce::var& input);
    juce::var findPlugins (const juce::var& input);
    juce::var addTrack (const juce::var& input);
    juce::var removeTrack (const juce::var& input, bool userSaidYes);
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
    juce::var deleteClip (const juce::var& input, bool userSaidYes);
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
    juce::var appFeatures();
    juce::var runInPage (const juce::String& name, const juce::var& input);
    static bool takesAway (const juce::String& what, const juce::var& payload);
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

    // What Fanan AI made itself (tracks, FX channels, clips: { track, clip }): it may take
    // those away again without asking.
    std::set<int> madeTracks;
    std::set<std::pair<int, int>> madeClips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AiTools)
};
