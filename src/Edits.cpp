// D:\workspace\Fanan_web_hub\src\Edits.cpp

#include "Edits.h"

//==============================================================================
Edits::Clips Edits::snapshot (const AudioEngine& engine, const std::vector<int>& trackIds)
{
    Clips clips;

    for (const auto id : trackIds)
        if (engine.findTrack (id) != nullptr)
            clips[id] = engine.getClips (id);

    return clips;
}

std::vector<int> Edits::allTrackIds (const AudioEngine& engine)
{
    std::vector<int> ids;

    for (const auto& track : engine.getTracks())
        ids.push_back (track->id);

    return ids;
}

bool Edits::same (const Clips& a, const Clips& b)
{
    if (a.size() != b.size())
        return false;

    for (const auto& [trackId, clips] : a)
    {
        const auto other = b.find (trackId);

        if (other == b.end() || other->second.size() != clips.size())
            return false;

        for (size_t i = 0; i < clips.size(); ++i)
        {
            const auto& x = clips[i];
            const auto& y = other->second[i];

            // Exact on purpose: the same clip copied twice, not two measurements.
            if (x.id != y.id || ! juce::exactlyEqual (x.start, y.start) || ! juce::exactlyEqual (x.length, y.length)
                || ! juce::exactlyEqual (x.offset, y.offset) || x.muted != y.muted || x.seed != y.seed
                || x.notes.size() != y.notes.size())
                return false;

            // Their operators too.
            for (size_t n = 0; n < x.notes.size(); ++n)
                if (! x.notes[n].sameAs (y.notes[n]))
                    return false;

            // The pitch bend and aftertouch lines too.
            const auto sameLine = [] (const std::vector<MidiControlPoint>& p, const std::vector<MidiControlPoint>& q)
            {
                if (p.size() != q.size())
                    return false;

                for (size_t k = 0; k < p.size(); ++k)
                    if (p[k].value != q[k].value || ! juce::exactlyEqual (p[k].time, q[k].time))
                        return false;

                return true;
            };

            if (! sameLine (x.pitchBend, y.pitchBend) || ! sameLine (x.aftertouch, y.aftertouch))
                return false;

            // An audio clip's window, volume and fades, and what it was rendered from.
            if (x.audioFile != y.audioFile || ! juce::exactlyEqual (x.audioOffset, y.audioOffset) || ! juce::exactlyEqual (x.audioLength, y.audioLength)
                || ! juce::exactlyEqual (x.audioGainDb, y.audioGainDb) || ! juce::exactlyEqual (x.fadeIn, y.fadeIn) || ! juce::exactlyEqual (x.fadeOut, y.fadeOut)
                || x.sourceFile != y.sourceFile || ! juce::exactlyEqual (x.sourceOffset, y.sourceOffset) || ! juce::exactlyEqual (x.sourceLength, y.sourceLength)
                || ! juce::exactlyEqual (x.sourceTempo, y.sourceTempo) || ! juce::exactlyEqual (x.pitch, y.pitch)
                || x.reversed != y.reversed || x.followTempo != y.followTempo || x.effectUid() != y.effectUid()
                || x.warp != y.warp || x.silence != y.silence)
                return false;
        }
    }

    return true;
}

Edits::TrackName Edits::nameOf (const AudioEngine& engine, int trackId)
{
    if (auto* track = engine.findTrack (trackId))
        return { track->name, track->nameIsAutomatic };

    return {};
}

//==============================================================================
bool Edits::Action::perform()
{
    // The first time is UndoManager::perform, after the change happened: nothing to do.
    if (std::exchange (fresh, false))
        return true;

    redo();
    return true;
}

bool Edits::Action::undo()
{
    revert();
    return true;
}

//==============================================================================
Edits::ClipsChanged::ClipsChanged (AudioEngine& e, Clips beforeChange, Clips afterChange)
    : engine (e), before (std::move (beforeChange)), after (std::move (afterChange))
{
}

int Edits::ClipsChanged::getSizeInUnits()
{
    size_t notes = 0;

    for (const auto* side : { &before, &after })
        for (const auto& [trackId, clips] : *side)
            for (const auto& clip : clips)
                notes += clip.notes.size() + (clip.pitchBend.size() + clip.aftertouch.size()) / 4;

    return 1 + static_cast<int> (notes / 64);
}

void Edits::ClipsChanged::apply (const Clips& clips)
{
    for (const auto& [trackId, trackClips] : clips)
        engine.setClips (trackId, trackClips);
}

//==============================================================================
Edits::TrackAdded::TrackAdded (AudioEngine& e, int trackId, int trackIndex)
    : engine (e), id (trackId), index (trackIndex)
{
}

void Edits::TrackAdded::redo()      { engine.attachTrack (std::move (kept), index); }
void Edits::TrackAdded::revert()    { kept = engine.detachTrack (id, index); }

Edits::TrackRemoved::TrackRemoved (AudioEngine& e, int trackId, int trackIndex, std::unique_ptr<Track> removed)
    : engine (e), id (trackId), index (trackIndex), kept (std::move (removed))
{
}

void Edits::TrackRemoved::redo()    { kept = engine.detachTrack (id, index); }
void Edits::TrackRemoved::revert()  { engine.attachTrack (std::move (kept), index); }

//==============================================================================
Edits::TrackRenamed::TrackRenamed (AudioEngine& e, int trackId, TrackName beforeChange, TrackName afterChange)
    : engine (e), id (trackId), before (std::move (beforeChange)), after (std::move (afterChange))
{
}

void Edits::TrackRenamed::set (const TrackName& name)
{
    if (auto* track = engine.findTrack (id))
    {
        track->name = name.text;
        track->nameIsAutomatic = name.automatic;
    }
}

//==============================================================================
Edits::AutomationChanged::AutomationChanged (AudioEngine& e, int trackId, std::vector<AutomationLane> beforeChange,
                                             std::vector<AutomationLane> afterChange)
    : engine (e), id (trackId), before (std::move (beforeChange)), after (std::move (afterChange))
{
}

int Edits::AutomationChanged::getSizeInUnits()
{
    size_t points = 0;

    for (const auto* lanes : { &before, &after })
        for (const auto& lane : *lanes)
            points += lane.points.size();

    return static_cast<int> (1 + points / 64);
}

//==============================================================================
Edits::InstrumentChanged::InstrumentChanged (AudioEngine& e, int trackId, std::unique_ptr<Plugin> otherInstrument,
                                             TrackName beforeChange, TrackName afterChange)
    : engine (e), id (trackId), other (std::move (otherInstrument)), before (std::move (beforeChange)), after (std::move (afterChange))
{
}

void Edits::InstrumentChanged::swap (const TrackName& name)
{
    other = engine.swapInstrument (id, std::move (other));

    if (auto* track = engine.findTrack (id))
    {
        track->name = name.text;
        track->nameIsAutomatic = name.automatic;
    }
}

//==============================================================================
Edits::InsertAdded::InsertAdded (AudioEngine& e, int channelId, int id, int insertPosition)
    : engine (e), channel (channelId), insertId (id), position (insertPosition)
{
}

void Edits::InsertAdded::redo()     { engine.attachInsert (channel, std::move (kept), position); }
void Edits::InsertAdded::revert()   { kept = engine.detachInsert (channel, insertId, position); }

Edits::InsertRemoved::InsertRemoved (AudioEngine& e, int channelId, int id, int insertPosition, std::unique_ptr<EffectChain::Insert> removed)
    : engine (e), channel (channelId), insertId (id), position (insertPosition), kept (std::move (removed))
{
}

void Edits::InsertRemoved::redo()   { kept = engine.detachInsert (channel, insertId, position); }
void Edits::InsertRemoved::revert() { engine.attachInsert (channel, std::move (kept), position); }

Edits::InsertReplaced::InsertReplaced (AudioEngine& e, int channelId, int current, std::unique_ptr<EffectChain::Insert> otherInsert, int insertPosition)
    : engine (e), channel (channelId), currentId (current), position (insertPosition), other (std::move (otherInsert))
{
}

void Edits::InsertReplaced::swap()
{
    if (other == nullptr)
        return;

    const auto otherId = other->id;
    int where = position;
    auto out = engine.detachInsert (channel, currentId, where);

    if (out == nullptr)
        return;

    engine.attachInsert (channel, std::move (other), where);
    other = std::move (out);
    currentId = otherId;
}

Edits::InsertMoved::InsertMoved (AudioEngine& e, int channelId, int id, int fromPosition, int toPosition)
    : engine (e), channel (channelId), insertId (id), from (fromPosition), to (toPosition)
{
}
