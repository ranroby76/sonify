// D:\workspace\Fanan_web_hub\src\MainTrackOptions.cpp
//
// The track list's Options menu (its wrench, as Studio One's; hub-trackoptions.js): the options
// the engine carries out. Each is on or off in Settings (Preferences::getTrackOptions).
//
//   Monitoring follows record          arming a track monitors it; disarming stops it
//   Instrument Input follows Selection the instrument track selected monitors (plays from the
//                                      MIDI devices); the others stop, unless they're armed
//   Audio Input follows Selection      the same for audio tracks (their input heard)
//   No overlap when editing events     a clip moved, copied, resized or pasted over others trims
//                                      them where it lands (one inside it goes; one around it is
//                                      cut in two around it)
//   Automation follows events          a clip moved takes its track's envelopes over its stretch
//                                      along (a copy copies them): to another track only for an
//                                      envelope that track has too

#include "MainComponent.h"

#include <cmath>

namespace
{
    constexpr double nearly = 1.0e-6;      // quarter notes: positions this close are the same
    constexpr double bridge = 1.0e-3;      // where an envelope's own line is kept beside a stretch
}

void MainComponent::armTrack (int trackId, bool arm)
{
    engine.setTrackRecordArm (trackId, arm);

    if (! prefs.trackOption ("monitorFollowsRecord"))
        return;

    if (auto* track = engine.findTrack (trackId); track != nullptr && ! track->isMixChannel())
        engine.setTrackMonitor (trackId, track->recordArm.load(), true);
}

bool MainComponent::inputFollowsSelection (int trackId)
{
    const auto* chosen = engine.findTrack (trackId);

    if (chosen == nullptr || chosen->isMixChannel())
        return false;

    const auto kind = chosen->kind;

    if (! prefs.trackOption (kind == Track::Kind::instrument ? "instrumentInputFollowsSelection" : "audioInputFollowsSelection"))
        return false;

    bool changed = false;

    for (const auto& track : engine.getTracks())
    {
        if (track->kind != kind || track->isFrozen())
            continue;

        const bool on = track->id == trackId;

        // An armed track goes on hearing what it records.
        if (! on && track->recordArm.load())
            continue;

        if (track->monitor.load() != on)
        {
            engine.setTrackMonitor (track->id, on, true);
            changed = true;
        }
    }

    return changed;
}

void MainComponent::clearOverlaps (const std::vector<std::pair<int, int>>& placed)
{
    // Each track's clips put down by the gesture: those are the ones that stay whole.
    std::map<int, std::set<int>> mine;

    for (const auto& [trackId, clipId] : placed)
        mine[trackId].insert (clipId);

    for (const auto& [trackId, ids] : mine)
    {
        for (const auto id : ids)
        {
            // As the track is now (an earlier clip may have trimmed some already).
            const auto clips = engine.getClips (trackId);
            const auto laid = std::find_if (clips.begin(), clips.end(), [id] (const MidiClip& clip) { return clip.id == id; });

            if (laid == clips.end())
                continue;

            const auto from = laid->start, to = laid->start + laid->length;

            for (const auto& other : clips)
            {
                if (ids.count (other.id) > 0)
                    continue;

                const auto otherFrom = other.start, otherTo = other.start + other.length;

                if (otherTo <= from + nearly || otherFrom >= to - nearly)
                    continue;   // beside it, not under it

                const auto keepsStart = otherFrom < from - nearly;
                const auto keepsEnd = otherTo > to + nearly;
                const auto shortest = AudioEngine::minimumClipLength;

                if (! keepsStart && ! keepsEnd)
                {
                    engine.deleteClip (trackId, other.id);   // all under it
                    continue;
                }

                // Around it: cut in two where it ends (the right part is a clip of its own).
                if (keepsStart && keepsEnd && to - otherFrom >= shortest && otherTo - to >= shortest)
                    engine.splitClip (trackId, other.id, to);

                // Its end comes back to where this one starts, or its start goes on to where it
                // ends; too little left, and it goes.
                const auto start = keepsStart ? otherFrom : to;
                const auto length = keepsStart ? from - otherFrom : otherTo - to;

                if (length < shortest)
                    engine.deleteClip (trackId, other.id);
                else
                    engine.setClipRange (trackId, other.id, start, length);
            }
        }
    }
}

void MainComponent::carryAutomation (const std::vector<CarriedClip>& carried)
{
    // Every track involved, its envelopes as they were and as they'll be.
    std::map<int, std::vector<AutomationLane>> before, after;

    for (const auto& clip : carried)
        for (const auto trackId : { clip.fromTrack, clip.toTrack })
            if (const auto* lanes = engine.getAutomation (trackId); lanes != nullptr && before.count (trackId) == 0)
                before[trackId] = *lanes;

    const auto hasEnvelope = [&before] (int trackId, const AutomationTarget& target)
    {
        const auto found = before.find (trackId);

        if (found == before.end())
            return false;

        return std::any_of (found->second.begin(), found->second.end(),
                            [&target] (const AutomationLane& lane) { return lane.target == target && ! lane.points.empty(); });
    };

    // The stretches carried (taken from the envelopes as they were), and the ones they leave.
    struct Piece
    {
        int trackId = 0;
        AutomationTarget target;
        double from = 0.0, to = 0.0;
        std::vector<AutomationPoint> points;
    };

    std::vector<Piece> pieces, left;

    for (const auto& clip : carried)
    {
        if (std::abs (clip.shift) < nearly && clip.fromTrack == clip.toTrack)
            continue;   // where it was

        const auto source = before.find (clip.fromTrack);

        if (source == before.end())
            continue;

        for (const auto& lane : source->second)
        {
            if (lane.points.empty() || ! hasEnvelope (clip.toTrack, lane.target))
                continue;

            Piece piece;
            piece.trackId = clip.toTrack;
            piece.target = lane.target;
            piece.from = clip.start + clip.shift;
            piece.to = clip.end + clip.shift;
            piece.points.push_back ({ piece.from, Automation::valueAt (lane.points, clip.start) });

            for (const auto& point : lane.points)
                if (point.ppq > clip.start + nearly && point.ppq < clip.end - nearly)
                    piece.points.push_back ({ point.ppq + clip.shift, point.value });

            piece.points.push_back ({ piece.to, Automation::valueAt (lane.points, clip.end) });
            pieces.push_back (std::move (piece));

            if (! clip.copy)
                left.push_back ({ clip.fromTrack, lane.target, clip.start, clip.end, {} });
        }
    }

    if (pieces.empty())
        return;

    after = before;

    const auto inside = [] (double ppq, double from, double to) { return ppq >= from - nearly && ppq <= to + nearly; };

    for (auto& [trackId, lanes] : after)
    {
        for (auto& lane : lanes)
        {
            // What's left of the envelope: the stretches the clips left gone (its line bridges them).
            std::vector<AutomationPoint> rest;

            for (const auto& point : lane.points)
            {
                const auto gone = std::any_of (left.begin(), left.end(), [&] (const Piece& stretch)
                                               { return stretch.trackId == trackId && stretch.target == lane.target
                                                     && inside (point.ppq, stretch.from, stretch.to); });

                if (! gone)
                    rest.push_back (point);
            }

            std::vector<const Piece*> arriving;

            for (const auto& piece : pieces)
                if (piece.trackId == trackId && piece.target == lane.target)
                    arriving.push_back (&piece);

            if (arriving.empty())
            {
                lane.points = std::move (rest);
                continue;
            }

            const auto underPiece = [&arriving, &inside] (double ppq)
            {
                return std::any_of (arriving.begin(), arriving.end(), [&] (const Piece* piece) { return inside (ppq, piece->from, piece->to); });
            };

            // Where they land, the envelope keeps its own line up to their edges, then they replace it.
            std::vector<AutomationPoint> points;

            if (! rest.empty())
            {
                for (const auto* piece : arriving)
                {
                    for (const auto at : { piece->from - bridge, piece->to + bridge })
                        if (at >= 0.0 && ! underPiece (at))
                            points.push_back ({ at, Automation::valueAt (rest, at) });
                }

                for (const auto& point : rest)
                    if (! underPiece (point.ppq))
                        points.push_back (point);
            }

            for (const auto* piece : arriving)
                points.insert (points.end(), piece->points.begin(), piece->points.end());

            lane.points = Automation::tidy (std::move (points));
        }
    }

    for (auto& [trackId, lanes] : after)
    {
        const auto touches = [trackId = trackId] (const Piece& piece) { return piece.trackId == trackId; };

        if (std::none_of (pieces.begin(), pieces.end(), touches) && std::none_of (left.begin(), left.end(), touches))
            continue;

        engine.setAutomationLanes (trackId, lanes);

        if (const auto* now = engine.getAutomation (trackId))
            addToHistory ("Move Automation", new Edits::AutomationChanged (engine, trackId, std::move (before[trackId]), *now), true);
    }
}
