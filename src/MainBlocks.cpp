// D:\workspace\Fanan_web_hub\src\MainBlocks.cpp
//
// Song Builder Blocks (SongBlocks.h) and its page: a block made from the selected clips, renamed,
// recoloured, copied or taken out, the blocks put in the song's order, and the song built from
// them on the timeline. Each is one step in Edit > Undo.
//
// Live (the page's Live switch): a clip launcher on the blocks. A block launched on a track (or
// a whole block, on all its tracks) starts at the next bar (a beat, two bars: the page's choice)
// and goes round until another is launched there or the track is stopped. It plays by being
// laid on the timeline, a copy at a time just ahead of the song, so the jam is the arrangement
// when it's done: one Undo step ("Live Jam") for all of it. Switched on, the song goes to the
// bar after its last clip, where the jam is written. Stopping the song stops what plays.

#include "MainComponent.h"
#include "Edits.h"

namespace
{
    constexpr int maxSequence = 1000;          // blocks in a song, at most
    constexpr int maxPreviewNotes = 256;       // notes a pad draws, per clip

    juce::var blockToVar (const SongBlock& block)
    {
        juce::Array<juce::var> parts;

        for (const auto& part : block.parts)
        {
            const auto& clip = part.clip;
            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("trackId", part.trackId);
            item->setProperty ("start", clip.start);
            item->setProperty ("length", clip.length);
            item->setProperty ("audio", clip.isAudio());

            // What the pad draws: the notes the clip shows, at their places in the block.
            juce::Array<juce::var> notes;

            for (const auto& note : clip.notes)
            {
                if (! clip.shows (note))
                    continue;

                if (notes.size() >= maxPreviewNotes)
                    break;

                notes.add (juce::Array<juce::var> { note.pitch, clip.songPosition (note.start),
                                                    juce::jmin (note.length, clip.start + clip.length - clip.songPosition (note.start)) });
            }

            item->setProperty ("notes", notes);
            parts.add (juce::var (item.get()));
        }

        juce::DynamicObject::Ptr result (new juce::DynamicObject());
        result->setProperty ("id", block.id);
        result->setProperty ("name", block.name);
        result->setProperty ("colour", block.colour);
        result->setProperty ("length", block.length);
        result->setProperty ("bars", block.bars);
        result->setProperty ("parts", parts);
        return juce::var (result.get());
    }

    bool isColour (const juce::String& text)
    {
        return text.length() == 7 && text.startsWithChar ('#') && text.substring (1).containsOnly ("0123456789abcdefABCDEF");
    }
}

//==============================================================================
void MainComponent::handleSongBlocks (const juce::var& event)
{
    const auto action = event.getProperty ("action", {}).toString();

    if (action == "launcherOn" || action == "launchQuantize" || action == "launch" || action == "launchRow"
        || action == "stopTrack" || action == "stopAll")
    {
        handleLauncher (action, event);
        return;
    }

    if (action == "build")
    {
        buildSongFromBlocks (static_cast<double> (event.getProperty ("at", 0.0)));
        return;
    }

    auto blocks = engine.getSongBlocks();
    const auto before = blocks;
    const auto id = static_cast<int> (event.getProperty ("id", 0));
    juce::String label;
    int made = 0;

    if (action == "make")
    {
        std::vector<int> clipIds;

        if (const auto* ids = event.getProperty ("clipIds", {}).getArray())
            for (const auto& value : *ids)
                clipIds.push_back (static_cast<int> (value));

        made = SongBlockEdits::make (engine, blocks, clipIds, event.getProperty ("name", {}).toString());

        if (made == 0)
        {
            pushTracks ("Select the clips to make a block from first.");
            return;
        }

        label = "Make Block";
    }
    else if (action == "edit")
    {
        auto* block = blocks.find (id);

        if (block == nullptr)
            return;

        if (event.hasProperty ("name"))
        {
            const auto name = event.getProperty ("name", {}).toString().trim().substring (0, 60);

            if (name.isNotEmpty() && name != block->name)
            {
                block->name = name;
                label = "Rename Block";
            }
        }

        if (const auto colour = event.getProperty ("colour", {}).toString(); isColour (colour) && colour != block->colour)
        {
            block->colour = colour.toLowerCase();
            label = label.isEmpty() ? "Colour Block" : label;
        }
    }
    else if (action == "duplicate")
    {
        made = SongBlockEdits::duplicate (blocks, id);

        if (made == 0)
            return;

        label = "Duplicate Block";
    }
    else if (action == "remove")
    {
        if (! SongBlockEdits::remove (blocks, id))
            return;

        label = "Delete Block";
    }
    else if (action == "sequence")
    {
        // The song's order of them, as the window has it now (each id a block that's there).
        std::vector<int> sequence;

        if (const auto* ids = event.getProperty ("ids", {}).getArray())
            for (const auto& value : *ids)
                if (const auto blockId = static_cast<int> (value); blocks.find (blockId) != nullptr && static_cast<int> (sequence.size()) < maxSequence)
                    sequence.push_back (blockId);

        // The same as it was (or nothing the window asked for is there): the window is put right.
        if (sequence == blocks.sequence)
        {
            pushSongBlocks (true);
            return;
        }

        blocks.sequence = std::move (sequence);
        label = event.getProperty ("label", {}).toString().substring (0, 60);
        label = label.isNotEmpty() ? label : juce::String ("Arrange Song Builder Blocks");
    }

    if (label.isEmpty())
        return;

    engine.setSongBlocks (blocks);
    addToHistory (label, new Edits::BlocksChanged (engine, before, blocks));
    pushSongBlocks();

    // A new pad: the window shows it (and offers its name for typing).
    if (made != 0 && webView != nullptr)
    {
        juce::DynamicObject::Ptr reply (new juce::DynamicObject());
        reply->setProperty ("id", made);
        reply->setProperty ("action", action);
        webView->emitEventIfBrowserIsVisible ("songBlockMade", juce::var (reply.get()));
    }
}

void MainComponent::buildSongFromBlocks (double at)
{
    if (engine.getSongBlocks().sequence.empty())
    {
        pushTracks ("Put blocks in the song first: click their pads.");
        return;
    }

    // Each block's clips on the tracks they came from, one block after another from there.
    const auto touched = SongBlockEdits::tracksOf (engine, engine.getSongBlocks());
    auto before = Edits::snapshot (engine, touched);
    const auto built = SongBlockEdits::build (engine, at);
    juce::String notice;

    if (built.placed > 0)
        addToHistory ("Build Song", new Edits::ClipsChanged (engine, std::move (before), Edits::snapshot (engine, touched)));
    else
        notice = "None of the blocks' tracks are in the song any more.";

    if (built.skipped > 0 && built.placed > 0)
        notice = juce::String (built.skipped) + (built.skipped == 1 ? " clip's track isn't" : " clips' tracks aren't") + " in the song any more: left out.";

    // The page says it in one message: where the song went, and what couldn't go.
    if (webView != nullptr)
    {
        juce::DynamicObject::Ptr reply (new juce::DynamicObject());
        reply->setProperty ("from", built.from);
        reply->setProperty ("to", built.to);
        reply->setProperty ("placed", built.placed);
        reply->setProperty ("skipped", built.skipped);
        reply->setProperty ("notice", notice);
        webView->emitEventIfBrowserIsVisible ("songBlocksBuilt", juce::var (reply.get()));
    }

    pushTracks();
}

void MainComponent::pushSongBlocks (bool force)
{
    if (webView == nullptr)
        return;

    const auto generation = engine.getSongBlocksGeneration();

    if (! force && generation == pushedBlocksGeneration)
        return;

    pushedBlocksGeneration = generation;
    const auto& songBlocks = engine.getSongBlocks();
    juce::Array<juce::var> blocks, sequence;

    for (const auto& block : songBlocks.blocks)
        blocks.add (blockToVar (block));

    for (const auto id : songBlocks.sequence)
        sequence.add (id);

    juce::DynamicObject::Ptr data (new juce::DynamicObject());
    data->setProperty ("blocks", blocks);
    data->setProperty ("sequence", sequence);
    webView->emitEventIfBrowserIsVisible ("songBlocks", juce::var (data.get()));
}

//==============================================================================
// Live
void MainComponent::handleLauncher (const juce::String& action, const juce::var& event)
{
    if (action == "launcherOn")
    {
        const auto on = static_cast<bool> (event.getProperty ("on", false));

        if (on && ! launcherOn)
        {
            launchTracks.clear();
            launchHistoryOpen = false;
            launchWasPlaying = engine.getTransport().getState().playing;

            // The jam goes after the song: the bar after its last clip.
            double end = 0.0;

            for (const auto& track : engine.getTracks())
                for (const auto& clip : track->clips)
                    end = juce::jmax (end, clip.start + clip.length);

            const auto& map = engine.getTempoMap();
            auto at = 0.0;

            if (end > 0.0)
            {
                const auto bar = map.barAt (end - 1.0e-6);
                at = bar.start + bar.length();
            }

            launcherOn = true;
            engine.getTransport().locate (at);
        }
        else if (! on && launcherOn)
        {
            // What plays stops at the next launch point; what was laid stays.
            std::vector<std::pair<int, int>> stops;

            for (const auto& [trackId, live] : launchTracks)
                if (live.blockId != 0)
                    stops.push_back ({ trackId, 0 });

            if (! stops.empty() && engine.getTransport().getState().playing)
                launchOnTracks (stops);

            launcherOn = false;
            launchTracks.clear();
        }

        pushLauncher();
        return;
    }

    if (! launcherOn)
        return;

    if (action == "launchQuantize")
    {
        launchQuantize = juce::jlimit (0, 2, static_cast<int> (event.getProperty ("value", 1)));
        pushLauncher();
        return;
    }

    const auto trackId = static_cast<int> (event.getProperty ("trackId", 0));
    const auto blockId = static_cast<int> (event.getProperty ("blockId", 0));
    std::vector<std::pair<int, int>> launches;

    if (action == "launch")
    {
        launches.push_back ({ trackId, blockId });
    }
    else if (action == "launchRow")
    {
        // A whole block: each of its tracks (the others play on).
        if (const auto* block = engine.getSongBlocks().find (blockId))
            for (const auto& part : block->parts)
                if (std::none_of (launches.begin(), launches.end(), [&part] (const auto& l) { return l.first == part.trackId; }))
                    launches.push_back ({ part.trackId, blockId });
    }
    else if (action == "stopTrack")
    {
        launches.push_back ({ trackId, 0 });
    }
    else if (action == "stopAll")
    {
        for (const auto& [id, live] : launchTracks)
            if (live.blockId != 0)
                launches.push_back ({ id, 0 });
    }

    if (! launches.empty())
        launchOnTracks (launches);
}

double MainComponent::launchPoint (bool& startPlaying) const
{
    auto& transport = const_cast<AudioEngine&> (engine).getTransport();
    const auto state = transport.getState();
    const auto& map = engine.getTempoMap();

    // Stopped: from the bar the song stands in, and the song starts.
    if (! state.playing)
    {
        startPlaying = true;
        return map.barAt (state.ppq + 1.0e-9).start;
    }

    startPlaying = false;

    // Playing: the next point of the grid, at least a moment ahead (the song keeps going while it's laid).
    const auto from = map.ppqAt (map.secondsAt (state.ppq) + 0.15);
    const auto bar = map.barAt (from);
    const auto barEnd = bar.start + bar.length();

    if (launchQuantize == 0)
    {
        const auto beats = std::ceil ((from - bar.start) / bar.beat() - 1.0e-9);
        return juce::jmin (barEnd, bar.start + beats * bar.beat());
    }

    auto at = from - bar.start < 1.0e-9 ? bar.start : barEnd;

    if (launchQuantize == 2 && map.barAt (at + 1.0e-9).index % 2 != 0)
    {
        const auto next = map.barAt (at + 1.0e-9);
        at = next.start + next.length();
    }

    return at;
}

void MainComponent::launchOnTracks (const std::vector<std::pair<int, int>>& launches)
{
    bool startPlaying = false;
    const auto at = launchPoint (startPlaying);
    std::vector<int> touched;

    for (const auto& launch : launches)
        if (engine.findTrack (launch.first) != nullptr)
            touched.push_back (launch.first);

    if (touched.empty())
        return;

    launchEdit (touched, [&]
    {
        for (const auto& [trackId, blockId] : launches)
        {
            if (engine.findTrack (trackId) == nullptr)
                continue;

            const auto* block = blockId > 0 ? engine.getSongBlocks().find (blockId) : nullptr;

            if (blockId > 0 && (block == nullptr || std::none_of (block->parts.begin(), block->parts.end(),
                                                                   [trackId] (const SongBlock::Part& p) { return p.trackId == trackId; })))
                continue;

            auto& live = launchTracks[trackId];
            launchCut (trackId, live, at);

            // What plays there until then: what plays now (nothing, after a stop), unless something is
            // still waiting to start there: then what plays until it, as before.
            if (live.startsAt <= engine.getTransport().getState().ppq + 1.0e-9)
                live.beforeBlockId = live.blockId;

            live.blockId = blockId;
            live.startsAt = at;
            live.nextCopy = at;

            if (block != nullptr && block->length > 0.0)
                launchPlace (trackId, live, *block, at);
        }
    });

    if (startPlaying)
    {
        engine.getTransport().locate (at);
        engine.getTransport().setPlaying (true);
        launchWasPlaying = true;
    }

    pushLauncher();
}

void MainComponent::launchPlace (int trackId, LaunchTrack& live, const SongBlock& block, double at)
{
    for (const auto& part : block.parts)
    {
        if (part.trackId != trackId)
            continue;

        auto clip = part.clip;
        clip.start = at + part.clip.start;
        const auto id = engine.addClip (trackId, std::move (clip));

        if (id == 0)
            continue;

        if (const auto* track = engine.findTrack (trackId))
            for (const auto& placed : track->clips)
                if (placed.id == id)
                    live.placed.push_back ({ id, placed.start, placed.start + placed.length });
    }

    live.nextCopy = at + block.length;
}

void MainComponent::launchCut (int trackId, LaunchTrack& live, double at)
{
    // What was laid from there on goes; what plays across it ends there.
    for (auto it = live.placed.begin(); it != live.placed.end();)
    {
        if (it->start >= at - 1.0e-6 || at - it->start < 1.0 / 64.0)
        {
            engine.deleteClip (trackId, it->clipId);
            it = live.placed.erase (it);
            continue;
        }

        if (it->end > at + 1.0e-6 && engine.setClipRange (trackId, it->clipId, it->start, at - it->start))
            it->end = at;

        ++it;
    }

    live.placed.erase (std::remove_if (live.placed.begin(), live.placed.end(), [at] (const LaunchPlaced& p) { return p.end <= at + 1.0e-6; }),
                       live.placed.end());
}

void MainComponent::launchEdit (const std::vector<int>& trackIds, const std::function<void()>& change)
{
    auto before = Edits::snapshot (engine, trackIds);
    change();
    auto after = Edits::snapshot (engine, trackIds);

    if (Edits::same (before, after))
        return;

    // The jam is one step: each edit joins the last one, until something else is done or undone.
    const auto join = launchHistoryOpen && launchHistoryAt == historySteps;
    addToHistory ("Live Jam", new Edits::ClipsChanged (engine, std::move (before), std::move (after)), join);
    launchHistoryAt = historySteps;
    launchHistoryOpen = true;
    pushTracks();
}

void MainComponent::updateLauncher()
{
    if (! launcherOn)
        return;

    // Tracks that went (a track deleted, another song) are let go of.
    for (auto it = launchTracks.begin(); it != launchTracks.end();)
        it = engine.findTrack (it->first) == nullptr ? launchTracks.erase (it) : std::next (it);

    const auto state = engine.getTransport().getState();
    const auto& map = engine.getTempoMap();

    if (! state.playing)
    {
        if (launchWasPlaying)
        {
            // Stopped: what was laid past the end of this bar goes, and nothing plays on.
            const auto bar = map.barAt (state.ppq + 1.0e-9);
            const auto at = state.ppq - bar.start < 1.0e-6 ? bar.start : bar.start + bar.length();
            std::vector<int> touched;

            for (const auto& [id, live] : launchTracks)
                if (live.blockId != 0 || ! live.placed.empty())
                    touched.push_back (id);

            launchEdit (touched, [&]
            {
                for (auto& [id, live] : launchTracks)
                {
                    launchCut (id, live, at);
                    live = {};
                }
            });

            launchWasPlaying = false;
            pushLauncher();
        }

        return;
    }

    launchWasPlaying = true;

    // Copies a little ahead of the song, while it plays.
    const auto horizon = juce::jmax (state.ppq + 4.0, map.ppqAt (map.secondsAt (state.ppq) + 2.5));
    std::vector<int> due;

    for (const auto& [id, live] : launchTracks)
        if (live.blockId > 0 && live.nextCopy < horizon)
            due.push_back (id);

    if (due.empty())
        return;

    launchEdit (due, [&]
    {
        for (const auto id : due)
        {
            auto& live = launchTracks[id];
            const auto* block = engine.getSongBlocks().find (live.blockId);

            if (block == nullptr || ! (block->length > 0.0))
            {
                live.blockId = 0;
                continue;
            }

            // The song jumped on past it: the next copy where it would be, in step with the first.
            if (live.nextCopy < state.ppq)
                live.nextCopy = live.startsAt + std::ceil ((state.ppq - live.startsAt) / block->length) * block->length;

            for (int guard = 0; live.nextCopy < horizon && guard < 16; ++guard)
                launchPlace (id, live, *block, live.nextCopy);

            // What has played can't be cut any more: let go of it.
            live.placed.erase (std::remove_if (live.placed.begin(), live.placed.end(),
                                               [&state] (const LaunchPlaced& p) { return p.end <= state.ppq; }),
                               live.placed.end());
        }
    });
}

void MainComponent::linkSetLauncher (const juce::StringArray& parts, bool on)
{
    // A pad pressed (letting go does nothing), while Live is on.
    if (! on || ! launcherOn || parts.size() < 2)
        return;

    // By place in the grid, as the page draws it: rows the blocks in their order, columns the
    // tracks they have clips on, in the song's order of tracks.
    const auto& blocks = engine.getSongBlocks().blocks;

    const auto blockAt = [&blocks] (const juce::String& row)
    {
        const auto index = row.getIntValue() - 1;
        return juce::isPositiveAndBelow (index, static_cast<int> (blocks.size())) ? blocks[static_cast<size_t> (index)].id : 0;
    };

    const auto trackAt = [this, &blocks] (const juce::String& column)
    {
        auto left = column.getIntValue();

        for (const auto& track : engine.getTracks())
        {
            const auto used = std::any_of (blocks.begin(), blocks.end(), [&track] (const SongBlock& block)
            {
                return std::any_of (block.parts.begin(), block.parts.end(), [&track] (const SongBlock::Part& p) { return p.trackId == track->id; });
            });

            if (used && --left == 0)
                return track->id;
        }

        return 0;
    };

    juce::String action;
    int blockId = 0, trackId = 0;

    if (parts[1] == "cell" && parts.size() == 4)
    {
        action = "launch";
        blockId = blockAt (parts[2]);
        trackId = trackAt (parts[3]);
    }
    else if (parts[1] == "row" && parts.size() == 3)
    {
        action = "launchRow";
        blockId = blockAt (parts[2]);
    }
    else if (parts[1] == "stop" && parts.size() == 3)
    {
        action = "stopTrack";
        trackId = trackAt (parts[2]);
    }
    else if (parts[1] == "stopall")
    {
        action = "stopAll";
    }

    // A place with nothing there (fewer blocks or tracks in this song) does nothing.
    if (action.isEmpty() || ((action == "launch" || action == "launchRow") && blockId == 0)
        || ((action == "launch" || action == "stopTrack") && trackId == 0))
        return;

    juce::DynamicObject::Ptr event (new juce::DynamicObject());
    event->setProperty ("blockId", blockId);
    event->setProperty ("trackId", trackId);
    handleLauncher (action, juce::var (event.get()));
}

//==============================================================================
// Pad grids (Launchpad, APC mini): their pads are the launcher's grid (PadGrid.h).
void MainComponent::drainSurfaceMessages()
{
    std::vector<ControlSurfaces::Message> all;
    engine.takeSurfaceMessages (all);

    for (auto& [port, message] : all)
    {
        if (padGrids != nullptr && padGrids->ownsPort (port))
            padGrids->receive (port, message);
        else
            surfacePending.emplace_back (port, std::move (message));
    }

    // No control surface takes them (none set up): they're let go.
    if (surfaces == nullptr || surfaces->isEmpty() || surfacePending.size() > 4096)
        surfacePending.clear();
}

void MainComponent::updatePadGrids()
{
    if (padGrids == nullptr || padGrids->isEmpty())
        return;

    drainSurfaceMessages();
    padGrids->show (padView());
}

PadGrids::View MainComponent::padView()
{
    PadGrids::View view;
    view.on = launcherOn;

    if (! launcherOn)
        return view;

    // As the page draws the grid: the blocks in their order, the tracks they have clips on in
    // the song's order (the first eight of each).
    const auto& blocks = engine.getSongBlocks().blocks;
    std::vector<int> columns;

    for (const auto& track : engine.getTracks())
    {
        const auto used = std::any_of (blocks.begin(), blocks.end(), [&track] (const SongBlock& block)
        {
            return std::any_of (block.parts.begin(), block.parts.end(), [&track] (const SongBlock::Part& p) { return p.trackId == track->id; });
        });

        if (used && columns.size() < 8)
            columns.push_back (track->id);
    }

    const auto ppq = engine.getTransport().getState().ppq;

    for (size_t c = 0; c < columns.size(); ++c)
    {
        view.columns[c] = true;
        const auto found = launchTracks.find (columns[c]);

        if (found == launchTracks.end())
            continue;

        const auto& live = found->second;
        const auto waiting = live.startsAt > ppq + 1.0e-6;
        const auto now = waiting ? live.beforeBlockId : live.blockId;
        view.tracks[c] = waiting && live.blockId == 0 && now != 0 ? PadGrids::Light::queued
                       : now != 0                                  ? PadGrids::Light::playing
                                                                   : PadGrids::Light::off;
    }

    for (size_t r = 0; r < blocks.size() && r < 8; ++r)
    {
        const auto& block = blocks[r];
        view.rows[r] = true;
        view.rowColours[r] = juce::Colour::fromString ("ff" + block.colour.trimCharactersAtStart ("#"));

        for (size_t c = 0; c < columns.size(); ++c)
        {
            const auto trackId = columns[c];

            if (std::none_of (block.parts.begin(), block.parts.end(), [trackId] (const SongBlock::Part& p) { return p.trackId == trackId; }))
                continue;

            auto light = PadGrids::Light::clip;

            if (const auto found = launchTracks.find (trackId); found != launchTracks.end())
            {
                const auto& live = found->second;
                const auto waiting = live.startsAt > ppq + 1.0e-6;

                if (waiting && live.blockId == block.id)
                    light = PadGrids::Light::queued;
                else if ((waiting ? live.beforeBlockId : live.blockId) == block.id)
                    light = PadGrids::Light::playing;
            }

            view.cells[r][c] = light;
        }
    }

    return view;
}

void MainComponent::pushLauncher()
{
    if (webView == nullptr)
        return;

    juce::Array<juce::var> tracks;

    for (const auto& [id, live] : launchTracks)
    {
        if (live.blockId == 0 && live.beforeBlockId == 0)
            continue;

        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("trackId", id);
        item->setProperty ("blockId", live.blockId);
        item->setProperty ("startsAt", live.startsAt);
        item->setProperty ("beforeBlockId", live.beforeBlockId);
        tracks.add (juce::var (item.get()));
    }

    juce::DynamicObject::Ptr data (new juce::DynamicObject());
    data->setProperty ("on", launcherOn);
    data->setProperty ("quantize", launchQuantize);
    data->setProperty ("tracks", tracks);
    webView->emitEventIfBrowserIsVisible ("songLive", juce::var (data.get()));
}