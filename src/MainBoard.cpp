// D:\workspace\Fanan_web_hub\src\MainBoard.cpp

#include "MainComponent.h"

//==============================================================================
// The Analog Board (AnalogBoard.h), its own tab: a second mixer the song can be mixed on. The
// page sets its knobs, switches it on and off, and sees its meters while it's shown; MIDI Link
// moves its knobs ("board.<n>.<setting>", "board.master.<setting>") and Sonify AI sets it
// (AiTools: get_board, set_board). Mixing, as in Studio One: not undo steps.

namespace
{
    juce::var specsToVar (const AnalogBoard::ParamSpec* specs, size_t count)
    {
        juce::Array<juce::var> list;

        for (size_t i = 0; i < count; ++i)
        {
            const auto& spec = specs[i];
            juce::DynamicObject::Ptr item (new juce::DynamicObject());
            item->setProperty ("id", spec.id);
            item->setProperty ("name", spec.name);
            item->setProperty ("section", spec.section);
            item->setProperty ("kind", spec.kind == AnalogBoard::ParamSpec::Kind::toggle ? "toggle"
                                     : spec.kind == AnalogBoard::ParamSpec::Kind::choice ? "choice"
                                     : spec.kind == AnalogBoard::ParamSpec::Kind::fader  ? "fader" : "knob");
            item->setProperty ("min", static_cast<double> (spec.minimum));
            item->setProperty ("max", static_cast<double> (spec.maximum));
            item->setProperty ("default", static_cast<double> (spec.defaultValue));
            item->setProperty ("unit", spec.unit);

            if (spec.kind == AnalogBoard::ParamSpec::Kind::choice)
            {
                juce::Array<juce::var> names;

                for (const auto& name : spec.choiceNames())
                    names.add (name);

                item->setProperty ("choices", names);
            }

            list.add (juce::var (item.get()));
        }

        return list;
    }

    /** A meter as the page draws it: [vu left, vu right (dBFS, RMS), peak left, peak right (since
        the last look), reduction (dB)]. */
    juce::var meterToVar (AnalogBoard::Meter& meter)
    {
        const auto round = [] (float value) { return std::round (value * 10.0f) / 10.0f; };
        const auto peakDb = [] (float peak) { return peak > 1.0e-5f ? 20.0f * std::log10 (peak) : -100.0f; };

        return juce::Array<juce::var> { static_cast<double> (round (meter.vuLeft.load())), static_cast<double> (round (meter.vuRight.load())),
                                        static_cast<double> (round (peakDb (meter.peakLeft.exchange (0.0f)))),
                                        static_cast<double> (round (peakDb (meter.peakRight.exchange (0.0f)))),
                                        static_cast<double> (round (meter.reduction.load())) };
    }
}

//==============================================================================
void MainComponent::handleBoard (const juce::var& event)
{
    const auto action = event.getProperty ("action", {}).toString();

    if (action == "shown")
    {
        boardShown = static_cast<bool> (event.getProperty ("shown", false));

        if (boardShown)
            pushBoard();

        return;
    }

    if (action == "mixing")
    {
        const auto on = static_cast<bool> (event.getProperty ("on", false));

        if (on == engine.isBoardMixing())
            return;

        // A mixdown being made keeps the mix it started with.
        if (engine.isMixdownRunning())
        {
            pushTracks ("Not while a mixdown is being made: switch once it's done.");
            return;
        }

        // The first time the board takes a song, its strips start where the Console's faders are.
        if (on && engine.isBoardFresh())
            engine.copyConsoleToBoard();

        engine.setBoardMixing (on);
        markChanged();
        pushTracks (on ? "The Analog Board mixes the song now: the Console's inserts, sends and Main are left out."
                       : "The Console mixes the song again.");
        return;
    }

    if (action == "set")
    {
        const auto id = event.getProperty ("id", {}).toString();
        const auto value = static_cast<float> (static_cast<double> (event.getProperty ("value", 0.0)));
        const auto strip = event.getProperty ("strip", 0);

        if (! std::isfinite (value))
            return;

        if (strip.toString() == "master" || static_cast<int> (strip) == 0)
        {
            if (const auto index = AnalogBoard::masterIndexOf (id); index >= 0)
                engine.getBoard().settings.set (index, value);
        }
        else if (auto* track = engine.findTrack (static_cast<int> (strip)); track != nullptr && ! track->isMixChannel())
        {
            if (const auto index = AnalogBoard::stripIndexOf (id); index >= 0)
                track->board.settings.set (index, value);
        }

        // The page shows the knob where it put it: nothing goes back.
        markChanged();
        return;
    }

    if (action == "copyConsole")
    {
        engine.copyConsoleToBoard();
        markChanged();
        pushBoard();
        return;
    }

    if (action == "reset")
    {
        const auto strip = event.getProperty ("strip", 0);

        if (strip.toString() == "master" || static_cast<int> (strip) == 0)
            engine.getBoard().settings.reset();
        else if (auto* track = engine.findTrack (static_cast<int> (strip)))
            track->board.settings.reset();

        markChanged();
        pushBoard();
        return;
    }

    if (action == "resetAll")
    {
        engine.resetBoard();
        markChanged();
        pushBoard();
        return;
    }

    if (action == "state")
        pushBoard();
}

void MainComponent::pushBoard()
{
    if (webView == nullptr)
        return;

    boardDue = false;

    juce::DynamicObject::Ptr state (new juce::DynamicObject());
    state->setProperty ("mixing", engine.isBoardMixing());

    // What the knobs are (once is enough, but they're small).
    juce::DynamicObject::Ptr specs (new juce::DynamicObject());
    specs->setProperty ("strip", specsToVar (AnalogBoard::stripSpecs().data(), AnalogBoard::stripSpecs().size()));
    specs->setProperty ("master", specsToVar (AnalogBoard::masterSpecs().data(), AnalogBoard::masterSpecs().size()));
    state->setProperty ("specs", juce::var (specs.get()));

    // Each instrument and audio track's strip, in the song's order: { id, values }.
    juce::Array<juce::var> strips;

    for (const auto& track : engine.getTracks())
    {
        if (track->isMixChannel())
            continue;

        juce::DynamicObject::Ptr strip (new juce::DynamicObject());
        strip->setProperty ("id", track->id);
        strip->setProperty ("values", track->board.settings.toVar (true));
        strips.add (juce::var (strip.get()));
    }

    state->setProperty ("strips", strips);
    state->setProperty ("master", engine.getBoard().settings.toVar (true));
    webView->emitEventIfBrowserIsVisible ("board", juce::var (state.get()));
}

juce::var MainComponent::boardMeters()
{
    juce::DynamicObject::Ptr meters (new juce::DynamicObject());
    juce::DynamicObject::Ptr strips (new juce::DynamicObject());

    for (const auto& track : engine.getTracks())
        if (! track->isMixChannel())
            strips->setProperty (juce::String (track->id), meterToVar (track->board.meter));

    meters->setProperty ("strips", juce::var (strips.get()));
    meters->setProperty ("master", meterToVar (engine.getBoard().meter));
    return juce::var (meters.get());
}

//==============================================================================
// MIDI Link: "board.<n>.<setting>" is the n-th track's strip (as the page numbers tracks),
// "board.master.<setting>" the master's.

bool MainComponent::linkBoardSetting (const juce::StringArray& parts, std::function<void (AnalogBoard::Settings<AnalogBoard::numStripParams>&, int)> strip,
                                      std::function<void (AnalogBoard::Settings<AnalogBoard::numMasterParams>&, int)> master)
{
    if (parts.size() != 3 || parts[0] != "board")
        return false;

    if (parts[1] == "master")
    {
        if (const auto index = AnalogBoard::masterIndexOf (parts[2]); index >= 0)
            master (engine.getBoard().settings, index);

        return true;
    }

    const auto wanted = parts[1].getIntValue();
    int count = 0;

    for (const auto& track : engine.getTracks())
    {
        if (! track->isMixChannel() && ++count == wanted)
        {
            if (const auto index = AnalogBoard::stripIndexOf (parts[2]); index >= 0)
                strip (track->board.settings, index);

            break;
        }
    }

    return true;
}

namespace
{
    /** A knob's position (0..1) to a setting's value, along the curve the page's knob has. */
    float boardValueAt (const AnalogBoard::ParamSpec& spec, float position)
    {
        const juce::String id (spec.id), unit (spec.unit);
        const auto low = spec.minimum, high = spec.maximum;

        if (spec.kind == AnalogBoard::ParamSpec::Kind::fader)
            return juce::jmax (low, ControlSurfaces::faderToDb (position));

        if (unit == "Hz" || id == "release" || id == "echoTime")
            return low * std::pow (high / low, position);

        if (unit == "dB" && (id.startsWith ("send") || id.endsWith ("Return")))
            return high - (high - low) * (1.0f - position) * (1.0f - position);

        if (id == "pan")
        {
            const auto pan = position * 2.0f - 1.0f;
            return std::abs (pan) < 0.01f ? 0.0f : pan;
        }

        return low + (high - low) * position;
    }
}

void MainComponent::linkMoveBoard (const juce::StringArray& parts, float position)
{
    linkBoardSetting (parts,
                      [&] (auto& settings, int index) { settings.set (index, boardValueAt (AnalogBoard::stripSpecs()[static_cast<size_t> (index)], position)); },
                      [&] (auto& settings, int index) { settings.set (index, boardValueAt (AnalogBoard::masterSpecs()[static_cast<size_t> (index)], position)); });
    markChanged();
    boardDue = true;
}

void MainComponent::linkSetBoard (const juce::StringArray& parts, bool on)
{
    linkBoardSetting (parts,
                      [on] (auto& settings, int index) { settings.set (index, on ? 1.0f : 0.0f); },
                      [on] (auto& settings, int index) { settings.set (index, on ? 1.0f : 0.0f); });
    markChanged();
    boardDue = true;
}

bool MainComponent::linkIsOnBoard (const juce::StringArray& parts)
{
    bool on = false;
    linkBoardSetting (parts,
                      [&on] (auto& settings, int index) { on = settings.isOn (index); },
                      [&on] (auto& settings, int index) { on = settings.isOn (index); });
    return on;
}
