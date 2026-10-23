// D:\workspace\Fanan_web_hub\src\MainAutomation.cpp
//
// Recording automation, as Studio One's Touch, Latch and Write: a track in one of them records
// the controls taken hold of while the song plays into its envelopes. A control is taken hold
// of from the page (its fader, pan, a plugin's controls), a control surface, MIDI Link, or a
// plugin's own window (the plugin says a parameter was taken hold of). While it's held its
// envelope leaves it alone (Track::heldTargets); its value is looked at 60 times a second, a
// point kept wherever it moves.
//
//   Touch   recorded while it moves; let go (still for a moment) it's kept, and the envelope
//           comes back to where it was, a sixteenth later
//   Latch   once taken hold of, recorded until the song stops (at the last value after it's let go)
//   Write   every envelope the track has recorded from where the song starts playing, at its
//           control's value, and any control taken hold of; when the song stops it's kept and
//           the track goes to Touch
//
// What's kept replaces the envelope over the stretch recorded (nothing else of it changes). The
// loop going round keeps what was recorded on the way, and recording starts again from the
// loop's start. Everything kept as the song stops is one undo step.

#include "MainComponent.h"

#include <cmath>

namespace
{
    constexpr double releaseMs = 650.0;          // Touch: still this long, and it's let go
    constexpr double returnQuarters = 0.25;      // where the envelope takes over again after a stretch

    double nowMs()   { return juce::Time::getMillisecondCounterHiRes(); }
}

void MainComponent::automationParamTouched (int trackId, int insertId, int parameterIndex)
{
    auto* plugin = engine.getPlugin (trackId, insertId);

    if (plugin == nullptr || ! juce::isPositiveAndBelow (parameterIndex, static_cast<int> (plugin->getParameters().size())))
        return;

    const auto& parameter = plugin->getParameters()[static_cast<size_t> (parameterIndex)];

    if (parameter.readOnly || parameter.hidden)
        return;

    AutomationTarget target;
    target.kind = AutomationTarget::Kind::parameter;
    target.slot = insertId;
    target.paramId = parameter.id;
    automationTouched (trackId, target);
}

void MainComponent::automationTouched (int trackId, const AutomationTarget& target)
{
    auto* track = engine.findTrack (trackId);

    if (track == nullptr || track->isMixChannel() || ! Automation::records (track->automationMode.load())
        || ! engine.getTransport().getState().playing || engine.isMixdownRunning())
        return;

    // Already being recorded: it's still held.
    for (auto& write : automationWrites)
        if (write.trackId == trackId && write.target == target)
        {
            write.lastChange = nowMs();
            return;
        }

    double value = 0.0;

    if (! currentControlValue (trackId, target, value) || ! track->setHeld (target, true))
        return;

    AutomationWrite write;
    write.trackId = trackId;
    write.target = target;
    write.from = engine.getTransport().getState().ppq;
    write.lastValue = value;
    write.lastChange = nowMs();
    write.generation = songGeneration;
    write.points.push_back ({ write.from, value });
    automationWrites.push_back (std::move (write));
}

bool MainComponent::currentControlValue (int trackId, const AutomationTarget& target, double& value)
{
    auto* track = engine.findTrack (trackId);

    if (track == nullptr)
        return false;

    switch (target.kind)
    {
        case AutomationTarget::Kind::volume:
            value = track->volumeDb.load();
            return true;

        case AutomationTarget::Kind::pan:
            value = track->pan.load();
            return true;

        case AutomationTarget::Kind::parameter:
        default:
        {
            auto* plugin = engine.getPlugin (trackId, target.slot);
            const auto index = plugin != nullptr ? plugin->findParameter (target.paramId) : -1;

            if (index < 0)
                return false;

            value = plugin->getParameterValue (index);
            return std::isfinite (value);
        }
    }
}

void MainComponent::keepAutomationWrite (AutomationWrite& write, double to, bool joinStep)
{
    // Another song opened meanwhile: this was another song's (its tracks are gone).
    if (write.generation != songGeneration)
        return;

    auto* track = engine.findTrack (write.trackId);
    const auto* lanes = engine.getAutomation (write.trackId);

    if (track != nullptr)
        track->setHeld (write.target, false);

    if (track == nullptr || lanes == nullptr)
        return;

    auto recorded = write.points;
    to = juce::jmax (to, recorded.empty() ? write.from : recorded.back().ppq);

    if (recorded.empty() || recorded.back().ppq < to - 1.0e-9)
        recorded.push_back ({ to, write.lastValue });

    // Only the points where it turns: a level held is two points, not many.
    std::vector<AutomationPoint> points;

    for (size_t i = 0; i < recorded.size(); ++i)
    {
        const bool middle = i > 0 && i + 1 < recorded.size();

        if (middle && juce::approximatelyEqual (recorded[i - 1].value, recorded[i].value)
                   && juce::approximatelyEqual (recorded[i].value, recorded[i + 1].value))
            continue;

        points.push_back (recorded[i]);
    }

    // Into the envelope over the stretch recorded: what it had before and after stays, and it
    // goes back to where it was a moment after.
    auto before = *lanes;
    AutomationLane lane;
    lane.target = write.target;

    for (const auto& existing : before)
        if (existing.target == write.target)
            lane.points = existing.points;

    const auto from = write.from;
    std::vector<AutomationPoint> kept;

    if (! lane.points.empty())
    {
        const auto atFrom = Automation::valueAt (lane.points, from);
        const auto back = to + returnQuarters;
        const auto atBack = Automation::valueAt (lane.points, back);

        for (const auto& point : lane.points)
            if (point.ppq < from - 1.0e-6 || point.ppq > back + 1.0e-6)
                kept.push_back (point);

        if (from > 1.0e-3)
            kept.push_back ({ from - 1.0e-3, atFrom });

        kept.insert (kept.end(), points.begin(), points.end());
        kept.push_back ({ back, atBack });
    }
    else
    {
        kept = points;
    }

    lane.points = Automation::tidy (std::move (kept));
    engine.setAutomationLane (write.trackId, std::move (lane));
    addToHistory ("Record Automation", new Edits::AutomationChanged (engine, write.trackId, std::move (before), *engine.getAutomation (write.trackId)),
                  joinStep);
    markChanged();
}

void MainComponent::finishAutomationWrites (int trackId)
{
    if (automationWrites.empty())
        return;

    const auto ppq = engine.getTransport().getState().ppq;
    bool kept = false;

    for (auto it = automationWrites.begin(); it != automationWrites.end();)
    {
        if (trackId != 0 && it->trackId != trackId)
        {
            ++it;
            continue;
        }

        keepAutomationWrite (*it, juce::jmax (ppq, automationLastPpq), kept);
        kept = true;
        it = automationWrites.erase (it);
    }

    if (kept)
        pushTracks();
}

void MainComponent::updateAutomationRecording()
{
    const auto state = engine.getTransport().getState();
    const auto playing = state.playing && ! engine.isMixdownRunning();
    const auto ppq = state.ppq;

    // Stopped: everything recorded is kept (one undo step); Write becomes Touch.
    if (! playing)
    {
        if (automationWasPlaying)
        {
            automationWasPlaying = false;
            bool kept = false;

            for (auto& write : automationWrites)
            {
                keepAutomationWrite (write, automationLastPpq, kept);
                kept = true;
            }

            automationWrites.clear();
            bool modesChanged = false;

            for (const auto& track : engine.getTracks())
                if (track->automationMode.load() == static_cast<int> (AutomationMode::write))
                {
                    engine.setAutomationMode (track->id, AutomationMode::touch);
                    modesChanged = true;
                }

            if (kept || modesChanged)
                pushTracks();
        }

        return;
    }

    // Starting: Write records every envelope its tracks have, from here.
    if (! automationWasPlaying)
    {
        automationWasPlaying = true;
        automationLastPpq = ppq;

        for (const auto& track : engine.getTracks())
            if (track->automationMode.load() == static_cast<int> (AutomationMode::write))
                for (const auto& lane : track->automation)
                    automationTouched (track->id, lane.target);
    }

    // The loop going round (or a jump back): what was recorded on the way is kept, and recording
    // goes on from here.
    if (ppq < automationLastPpq - 1.0e-3 && ! automationWrites.empty())
    {
        bool kept = false;

        for (auto& write : automationWrites)
        {
            keepAutomationWrite (write, automationLastPpq, kept);
            kept = true;

            if (auto* track = engine.findTrack (write.trackId))
                track->setHeld (write.target, true);

            write.from = ppq;
            write.points.clear();
            write.points.push_back ({ ppq, write.lastValue });
        }

        pushTracks();
    }

    const auto now = nowMs();
    bool released = false;

    for (auto it = automationWrites.begin(); it != automationWrites.end();)
    {
        auto& write = *it;
        auto* track = engine.findTrack (write.trackId);
        const auto mode = track != nullptr ? track->automationMode.load() : static_cast<int> (AutomationMode::off);
        double value = 0.0;

        // Its track gone, out of the recording modes, or the control gone: kept as far as it got.
        if (track == nullptr || ! Automation::records (mode) || ! currentControlValue (write.trackId, write.target, value))
        {
            keepAutomationWrite (write, ppq, released);
            released = true;
            it = automationWrites.erase (it);
            continue;
        }

        if (! juce::approximatelyEqual (value, write.lastValue))
        {
            // Held still until now: the level it held, up to the moment it moved again.
            if (! write.points.empty() && write.points.back().ppq < automationLastPpq - 1.0e-6)
                write.points.push_back ({ automationLastPpq, write.lastValue });

            write.points.push_back ({ ppq, value });
            write.lastValue = value;
            write.lastChange = now;
        }

        // Touch: let go, its envelope takes over again.
        if (mode == static_cast<int> (AutomationMode::touch) && now - write.lastChange > releaseMs)
        {
            keepAutomationWrite (write, ppq, released);
            released = true;
            it = automationWrites.erase (it);
            continue;
        }

        ++it;
    }

    automationLastPpq = ppq;

    if (released)
        pushTracks();
}
