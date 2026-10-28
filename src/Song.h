// D:\workspace\Fanan_web_hub\src\Song.h

#pragma once

#include <juce_core/juce_core.h>

#include "AudioEngine.h"

//==============================================================================
/**
    Song files (.fsong): a song as JSON, enough to put it back exactly as it was.

    Inside: the transport settings, the song's audio inputs and outputs and record format, the
    Main channel, and every track (instrument or audio) with its settings, its instrument or
    input, its output, its inserts, and its clips: notes, or a window onto an audio file. Audio files
    inside the song's folder are kept as paths relative to it, so the folder can move. A plugin is
    kept as the file it came from and its id, plus its own saved state (the CLAP state
    extension) and its parameter values, which are used when it can't save a state. The Song
    Blocks (SongBlocks.h) come too, their clips naming their tracks by place, and the song's
    video (its file, relative when it's in the song's folder, and where its picture starts).
    The page's view settings (zoom, snap and so on) ride along under "view".

    Like Studio One, each new song gets its own folder: Songs\<name>\<name>.fsong.
*/
namespace Song
{
    inline constexpr const char* fileExtension = ".fsong";
    inline constexpr const char* formatName = "Fanan Web Hub Song";
    inline constexpr int formatVersion = 1;

    /** Documents\Fanan Web Hub\Songs: where new songs go unless told otherwise. */
    juce::File getDefaultFolder();

    /** The engine's song, ready for juce::JSON. view is whatever the page wants back on load.
        withIds: an undo's snapshot (never a file): the ids of its tracks, inserts (Main's too)
        and clips come too, and restore gives them back, so older undo steps still find them. */
    juce::var capture (AudioEngine& engine, const juce::String& name, const juce::var& view,
                       const juce::File& songFile = {}, bool withIds = false);

    /** Replaces the engine's song with this one. Something that isn't a song this version
        can read fails without touching the engine. Otherwise the song loads, and anything
        that couldn't come back (a plugin file that has gone, say) is listed in problems.
        Message thread. */
    juce::Result restore (AudioEngine& engine, const juce::var& song, juce::StringArray& problems,
                          const juce::File& songFile = {});

    /** A copy of a track, added at the end of the song: its settings, plugins (with their
        state), envelopes, sends and clips. Returns its id (0: no such track); anything that
        couldn't come with it (a plugin file that has gone) is listed in problems. */
    int duplicateTrack (AudioEngine& engine, int trackId, juce::StringArray& problems);

    /** A copy made as asked (the track menu's Duplicate Track): withClips false leaves out its clips
        and envelopes (an audio track's "Duplicate Track": its settings and effects only);
        withInstrument false leaves out its instrument (an instrument track's "Duplicate Track": its
        notes and settings, played by Cayman until another is chosen). */
    int duplicateTrack (AudioEngine& engine, int trackId, juce::StringArray& problems, bool withClips, bool withInstrument);

    /** A track as Copy keeps it (the track menu's Cut and Copy): everything, as duplicateTrack
        copies it, and which tracks it named then (its sends' FX channels), so Paste links the same
        ones while they're there. Void: no such track. */
    juce::var copyTrack (AudioEngine& engine, int trackId);

    /** Paste: a track Copy kept, added at the end of the song. Returns its id (0: it couldn't be). */
    int pasteTrack (AudioEngine& engine, const juce::var& copied, juce::StringArray& problems);

    /** Track presets (.trackpreset, Studio One's Store / Load Track Preset): tracks as a song
        keeps them, without what belongs to one song (clips, envelopes, sends, the bus or output
        and input they use, other tracks they hear, their height, freeze, mute, solo, arm and
        monitoring). Their instruments, inserts and MIDI FX come with their states, and their
        name, colour, fader, pan and mono or stereo. */
    inline constexpr const char* trackPresetExtension = ".trackpreset";
    inline constexpr const char* trackPresetFormat = "Sonify Track Preset";

    juce::var captureTrackPreset (AudioEngine& engine, const std::vector<int>& trackIds);

    /** The preset's tracks, added at the end of the song; returns their ids (none: not a track
        preset). What couldn't come with them (a plugin file that has gone) is listed in problems. */
    std::vector<int> restoreTrackPreset (AudioEngine& engine, const juce::var& preset, juce::StringArray& problems);

    /** The song's inputs and outputs (Song Setup's Audio I/O) as songs keep them, the page sees
        them, and Make Default stores them. */
    juce::var inputsToVar (const std::vector<AudioEngine::SongInput>& inputs);

    /** An audio clip's Stretcher time map ([render seconds, window seconds] points; none when they
        aren't one) and its Silence Areas ({threshold, shortest, fade}; none: off), as songs keep them. */
    std::vector<std::pair<double, double>> warpFromVar (const juce::var& points);
    juce::var warpToVar (const std::vector<std::pair<double, double>>& warp);
    MidiClip::Silence silenceFromVar (const juce::var& value);

    /** A clip's own automation lines, as songs, the page and undo keep them: [{key, points:
        [[time, value], ...]}] (content times); none when it has none. Read back tidied. */
    juce::var envelopesToVar (const std::vector<ClipEnvelope>& envelopes);
    std::vector<ClipEnvelope> envelopesFromVar (const juce::var& list, bool audio);
    std::vector<AudioEngine::SongInput> inputsFromVar (const juce::var& list);
    juce::var outputsToVar (const std::vector<AudioEngine::SongOutput>& outputs);
    std::vector<AudioEngine::SongOutput> outputsFromVar (const juce::var& list);
}
