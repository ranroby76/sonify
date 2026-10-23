// D:\workspace\Fanan_web_hub\src\Edits.h

#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "AudioEngine.h"

#include <map>

//==============================================================================
/**
    Undo and redo for the song, as juce::UndoableActions for a juce::UndoManager.

    Each action is made after its change has happened and knows how to take it back
    and do it again: the first perform() (from UndoManager::perform) just accepts the
    change, later ones redo it, and undo() reverses it. Whatever an action takes out of
    the song (a removed track, a replaced plugin) stays alive inside it, so bringing it
    back is instant and exact, plugin state and all.

    Like Studio One, it covers editing the song: clips and notes, recording, tracks,
    plugins and MIDI FX, tempo and time signature. Mixing (faders, pan, mute, solo, bypass) and
    plugin settings are not part of it.
*/
namespace Edits
{
    /** Some tracks' clips, by track id. */
    using Clips = std::map<int, std::vector<MidiClip>>;

    Clips snapshot (const AudioEngine& engine, const std::vector<int>& trackIds);
    std::vector<int> allTrackIds (const AudioEngine& engine);
    bool same (const Clips& a, const Clips& b);

    struct TrackName
    {
        juce::String text;
        bool automatic = true;       // still following its instrument's name
    };

    TrackName nameOf (const AudioEngine& engine, int trackId);

    //==============================================================================
    class Action : public juce::UndoableAction
    {
    public:
        bool perform() final;
        bool undo() final;

    protected:
        virtual void redo() = 0;
        virtual void revert() = 0;

    private:
        bool fresh = true;           // the change already happened when the action was made
    };

    //==============================================================================
    /** Any clip or note edit, a recording too: the touched tracks' clips before and after. */
    class ClipsChanged final : public Action
    {
    public:
        ClipsChanged (AudioEngine& engine, Clips before, Clips after);
        int getSizeInUnits() override;

    private:
        void redo() override         { apply (after); }
        void revert() override       { apply (before); }
        void apply (const Clips& clips);

        AudioEngine& engine;
        Clips before, after;
    };

    class TrackAdded final : public Action
    {
    public:
        TrackAdded (AudioEngine& engine, int trackId, int index);

    private:
        void redo() override;
        void revert() override;

        AudioEngine& engine;
        int id, index;
        std::unique_ptr<Track> kept;
    };

    class TrackRemoved final : public Action
    {
    public:
        TrackRemoved (AudioEngine& engine, int trackId, int index, std::unique_ptr<Track> removed);

    private:
        void redo() override;
        void revert() override;

        AudioEngine& engine;
        int id, index;
        std::unique_ptr<Track> kept;
    };

    /** Tracks put into buses (or back to their outputs): each one's bus before and after. */
    class BusesChanged final : public Action
    {
    public:
        BusesChanged (AudioEngine& e, std::map<int, int> beforeChange, std::map<int, int> afterChange)
            : engine (e), before (std::move (beforeChange)), after (std::move (afterChange)) {}

        void redo() override     { apply (after); }
        void revert() override   { apply (before); }
        int getSizeInUnits() override   { return 1; }

    private:
        void apply (const std::map<int, int>& buses)
        {
            for (const auto& [track, bus] : buses)
                if (auto* t = engine.findTrack (track))
                    t->outputBus.store (bus);   // the bus itself may be away (undone): the track waits for it
        }

        AudioEngine& engine;
        const std::map<int, int> before, after;
    };

    /** Song Blocks made, renamed, recoloured or taken out, or the song's order of them changed:
        all of them before and after (they're small: clips without audio). */
    class BlocksChanged final : public Action
    {
    public:
        BlocksChanged (AudioEngine& e, SongBlocks beforeChange, SongBlocks afterChange)
            : engine (e), before (std::move (beforeChange)), after (std::move (afterChange)) {}

        void redo() override     { engine.setSongBlocks (after); }
        void revert() override   { engine.setSongBlocks (before); }

        int getSizeInUnits() override
        {
            size_t notes = 0;

            for (const auto* blocks : { &before, &after })
                for (const auto& block : blocks->blocks)
                    for (const auto& part : block.parts)
                        notes += part.clip.notes.size() + 1;

            return static_cast<int> (notes / 64) + 1;
        }

    private:
        AudioEngine& engine;
        const SongBlocks before, after;
    };

    /** The song's video added, moved or taken away (its sound's clip is a ClipsChanged of its own). */
    class VideoChanged final : public Action
    {
    public:
        VideoChanged (AudioEngine& e, AudioEngine::SongVideo beforeChange, AudioEngine::SongVideo afterChange)
            : engine (e), before (std::move (beforeChange)), after (std::move (afterChange)) {}

        void redo() override     { engine.setSongVideo (after); }
        void revert() override   { engine.setSongVideo (before); }

    private:
        AudioEngine& engine;
        const AudioEngine::SongVideo before, after;
    };

    /** Tracks moved (dragged in the track list or the Console): the order before and after. */
    class TracksReordered final : public Action
    {
    public:
        TracksReordered (AudioEngine& e, std::vector<int> beforeChange, std::vector<int> afterChange)
            : engine (e), before (std::move (beforeChange)), after (std::move (afterChange)) {}

        void redo() override     { engine.setTrackOrder (after); }
        void revert() override   { engine.setTrackOrder (before); }
        int getSizeInUnits() override   { return 1; }

    private:
        AudioEngine& engine;
        const std::vector<int> before, after;
    };

    /** A track's automation envelopes, before and after an edit. */
    /** An AI request: the whole song before it and after it, so everything it did is one
        undo step. restore puts a song (as Song writes it) back into the Hub. */
    class SongSnapshot final : public Action
    {
    public:
        SongSnapshot (std::function<void (const juce::String&)> restoreSong, juce::String beforeSong, juce::String afterSong)
            : restore (std::move (restoreSong)), before (std::move (beforeSong)), after (std::move (afterSong)) {}

        void redo() override     { restore (after); }
        void revert() override   { restore (before); }
        int getSizeInUnits() override   { return static_cast<int> ((before.length() + after.length()) / 1024) + 1; }

    private:
        std::function<void (const juce::String&)> restore;
        const juce::String before, after;
    };

    /** A track's sends, before and after (added, removed, level, on). */
    class SendsChanged final : public Action
    {
    public:
        SendsChanged (AudioEngine& e, int trackId, std::vector<Track::Send> beforeChange, std::vector<Track::Send> afterChange)
            : engine (e), id (trackId), before (std::move (beforeChange)), after (std::move (afterChange)) {}

        void redo() override     { engine.setSends (id, after); }
        void revert() override   { engine.setSends (id, before); }
        int getSizeInUnits() override   { return 1; }

    private:
        AudioEngine& engine;
        const int id;
        const std::vector<Track::Send> before, after;
    };

    class AutomationChanged final : public Action
    {
    public:
        AutomationChanged (AudioEngine& engine, int trackId, std::vector<AutomationLane> before, std::vector<AutomationLane> after);

        void redo() override     { engine.setAutomationLanes (id, after); }
        void revert() override   { engine.setAutomationLanes (id, before); }
        int getSizeInUnits() override;

    private:
        AudioEngine& engine;
        const int id;
        const std::vector<AutomationLane> before, after;
    };

    class TrackRenamed final : public Action
    {
    public:
        TrackRenamed (AudioEngine& engine, int trackId, TrackName before, TrackName after);

    private:
        void redo() override         { set (after); }
        void revert() override       { set (before); }
        void set (const TrackName& name);

        AudioEngine& engine;
        int id;
        TrackName before, after;
    };

    /** A track's instrument loaded, replaced or removed; other is the one not on the track. */
    class InstrumentChanged final : public Action
    {
    public:
        InstrumentChanged (AudioEngine& engine, int trackId, std::unique_ptr<Plugin> other,
                           TrackName before, TrackName after);

    private:
        void redo() override         { swap (after); }
        void revert() override       { swap (before); }
        void swap (const TrackName& name);

        AudioEngine& engine;
        int id;
        std::unique_ptr<Plugin> other;
        TrackName before, after;
    };

    class InsertAdded final : public Action
    {
    public:
        InsertAdded (AudioEngine& engine, int channel, int insertId, int position);

    private:
        void redo() override;
        void revert() override;

        AudioEngine& engine;
        int channel, insertId, position;
        std::unique_ptr<EffectChain::Insert> kept;
    };

    class InsertRemoved final : public Action
    {
    public:
        InsertRemoved (AudioEngine& engine, int channel, int insertId, int position, std::unique_ptr<EffectChain::Insert> removed);

    private:
        void redo() override;
        void revert() override;

        AudioEngine& engine;
        int channel, insertId, position;
        std::unique_ptr<EffectChain::Insert> kept;
    };

    /** An insert swapped for another in the same place; other is the one out of the chain. */
    class InsertReplaced final : public Action
    {
    public:
        InsertReplaced (AudioEngine& engine, int channel, int currentId, std::unique_ptr<EffectChain::Insert> other, int position);

    private:
        void redo() override         { swap(); }
        void revert() override       { swap(); }
        void swap();

        AudioEngine& engine;
        int channel, currentId, position;
        std::unique_ptr<EffectChain::Insert> other;
    };

    class InsertMoved final : public Action
    {
    public:
        InsertMoved (AudioEngine& engine, int channel, int insertId, int from, int to);

    private:
        void redo() override         { engine.moveInsert (channel, insertId, to); }
        void revert() override       { engine.moveInsert (channel, insertId, from); }

        AudioEngine& engine;
        int channel, insertId, from, to;
    };

    /** A track's MIDI FX added, removed or moved: the devices before and after, as plain data
        (their settings and ids; what a device was in the middle of playing isn't kept). */
    class MidiFxChanged final : public Action
    {
    public:
        MidiFxChanged (AudioEngine& e, int trackId, juce::var beforeChange, juce::var afterChange)
            : engine (e), id (trackId), before (std::move (beforeChange)), after (std::move (afterChange)) {}

        int getSizeInUnits() override   { return 1; }

    private:
        void redo() override         { engine.setMidiFx (id, after); }
        void revert() override       { engine.setMidiFx (id, before); }

        AudioEngine& engine;
        const int id;
        const juce::var before, after;
    };

    /** The song channel's tempo map (tempo changes and ramps, time signatures, keys), before
        and after: the tempo field, the Tempo, Signature and Key lanes, a recording's tempo taken on. */
    class TempoChanged final : public Action
    {
    public:
        TempoChanged (AudioEngine& e, TempoMap beforeChange, TempoMap afterChange)
            : engine (e), before (std::move (beforeChange)), after (std::move (afterChange)) {}

        int getSizeInUnits() override   { return 1; }

    private:
        void redo() override         { engine.setTempoMap (after); }
        void revert() override       { engine.setTempoMap (before); }

        AudioEngine& engine;
        const TempoMap before, after;
    };

    /** Main's volume envelope (the song channel's Main Volume lane), before and after. */
    class MainAutomationChanged final : public Action
    {
    public:
        MainAutomationChanged (AudioEngine& e, std::vector<AutomationPoint> beforeChange, std::vector<AutomationPoint> afterChange)
            : engine (e), before (std::move (beforeChange)), after (std::move (afterChange)) {}

        int getSizeInUnits() override   { return 1; }

    private:
        void redo() override         { engine.setMainAutomation (after); }
        void revert() override       { engine.setMainAutomation (before); }

        AudioEngine& engine;
        const std::vector<AutomationPoint> before, after;
    };
}
