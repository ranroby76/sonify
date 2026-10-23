// D:\workspace\Fanan_web_hub\src\MainLiveControl.cpp
//
// The Live Show's Controller (LiveController.h): the stream's cameras and screen, each Main,
// Custom or Hidden (in the landscape picture and the vertical one, TikTok's, each its own boxes),
// its presets and its MIDI pads, in a window of its own; and its Monitor, the
// picture as the viewers see it, in a window over every other that's never in the stream. What it
// asks of Sonify: the stream's pictures, the cameras and screens, the MIDI inputs, the show's
// switches (Enable streaming, Mute mics, Mute playback). Its pads come in through MIDI Link's
// intake (it takes its own first) and reach no track.

#include "MainComponent.h"

//==============================================================================
void MainComponent::setupLiveControl()
{
    // A first Controller takes the stream's camera and screen as they were set before it came
    // (each as the stream showed it then).
    auto fallback = LiveControl::Setup::standard();
    const auto before = juce::JSON::parse (streamSettingsFile());

    if (before.isObject() && before.hasProperty ("camera"))
    {
        auto& camera = fallback.rows[0];
        auto& screen = fallback.rows[static_cast<size_t> (LiveControl::screenRow)];
        const auto layout = before.getProperty ("layout", "camera").toString();
        camera.device = before.getProperty ("camera", {}).toString().trim();
        screen.device = before.getProperty ("screen", {}).toString().trim();
        camera.show = camera.device.isEmpty() ? LiveControl::Show::hidden
                    : layout == "screen"      ? LiveControl::Show::hidden
                    : layout == "screenWithCamera" && screen.device.isNotEmpty() ? LiveControl::Show::custom
                                                                                 : LiveControl::Show::main;
        screen.show = screen.device.isNotEmpty() && layout != "camera" ? LiveControl::Show::main : LiveControl::Show::hidden;
    }

    liveControl = std::make_unique<LiveControl::Controller> (static_cast<LiveControl::Host&> (*this),
                                                             SettingsFolder::file ("liveshow-controller.json"), fallback);
    liveControl->onPadsChanged = [this] { applyLinkedSources(); };
    applyLinkedSources();

    // Its pads first, while it's in use (Live streaming, or its window open): the rest go on to
    // MIDI Link as before.
    if (midiLink != nullptr)
        midiLink->claim = [this] (const juce::String& port, const juce::MidiMessage& message)
        {
            return liveControl != nullptr && (liveMode == "stream" || liveControl->isWindowShown())
                && liveControl->handleMidi (port, message);
        };
}

void MainComponent::openLiveController (bool open)
{
    if (liveControl == nullptr)
        return;

    if (open)
    {
        if (liveMode != "stream")
        {
            pushLive ("The Controller is Live streaming's.");
            return;
        }

        liveStudio();   // the picture's maker, the Controller's rows as its sources
        liveControl->showWindow (true);
    }
    else
    {
        liveControl->showWindow (false);
    }

    applyLivePreviewing();
    pushLive();
}

void MainComponent::applyLiveControl()
{
    if (liveVideo != nullptr && liveControl != nullptr)
        liveVideo->setSources (liveControl->getSources());
}

void MainComponent::applyLivePreviewing()
{
    if (liveVideo == nullptr)
        return;

    const auto controllerShows = liveControl != nullptr && (liveControl->isWindowShown() || liveControl->isMonitorShown());
    liveVideo->setPreviewing (streamPanelOpen || controllerShows);

    // The vertical picture too: the Controller (or its Monitor) showing it, or the panel with a
    // vertical destination ticked.
    liveVideo->setVerticalPreview ((controllerShows && liveControl->isViewTall()) || (streamPanelOpen && streamHasVertical()));
}

//==============================================================================
// What the Controller asks.

void MainComponent::controllerChanged()
{
    applyLiveControl();
}

juce::Image MainComponent::controllerPicture()
{
    return liveVideo != nullptr ? liveVideo->getLayoutPicture() : juce::Image();
}

juce::Image MainComponent::controllerOutputPicture()
{
    return liveVideo != nullptr ? liveVideo->getPicture() : juce::Image();
}

juce::Image MainComponent::controllerTallPicture()
{
    return liveVideo != nullptr ? liveVideo->getTallLayoutPicture() : juce::Image();
}

juce::Image MainComponent::controllerTallOutputPicture()
{
    return liveVideo != nullptr ? liveVideo->getTallPicture() : juce::Image();
}

void MainComponent::controllerViewChanged()
{
    applyLivePreviewing();
}

juce::Image MainComponent::controllerSourcePicture (int row)
{
    return liveVideo != nullptr ? liveVideo->getSourcePicture (row) : juce::Image();
}

juce::String MainComponent::controllerSourceProblem (int row)
{
    return liveVideo != nullptr ? liveVideo->getSourceProblem (row) : juce::String();
}

std::vector<LiveStream::Device> MainComponent::controllerCameras (bool lookAgain)
{
    // Looked for once (a moment: Windows asks each), and again when asked.
    if (lookAgain || streamCameras.empty())
        streamCameras = LiveStream::cameras();

    return streamCameras;
}

std::vector<LiveStream::Device> MainComponent::controllerScreens()
{
    streamScreens = LiveStream::screens();
    return streamScreens;
}

juce::StringArray MainComponent::controllerMidiPorts()
{
    // Every MIDI input but a control surface's (the surfaces take all theirs send).
    juce::StringArray surfacePorts, ports;

    for (const auto& device : prefs.devices)
        if (device.kind == "surface" && device.input.isNotEmpty())
            surfacePorts.add (device.input);

    for (const auto& input : juce::MidiInput::getAvailableDevices())
        if (! surfacePorts.contains (input.name))
            ports.addIfNotAlreadyThere (input.name);

    return ports;
}

void MainComponent::controllerMidiPortChosen (const juce::String& port)
{
    // Turned off by hand in MIDI Setup: Sonify listens to it again.
    if (! prefs.midiInputsOff.contains (port))
        return;

    for (const auto& input : juce::MidiInput::getAvailableDevices())
        if (input.name == port)
            setMidiPortEnabled (input.identifier, true);
}

bool MainComponent::controllerSwitch (const juce::String& which)
{
    return which == "stream"   ? liveOnAir
         : which == "mics"     ? liveMicsMuted
         : which == "playback" && livePlaybackMuted;
}

void MainComponent::controllerSetSwitch (const juce::String& which, bool on)
{
    if (which == "stream" || which == "mics" || which == "playback")
        setLiveSwitch (which, on);
}

juce::String MainComponent::controllerStatus (bool& live)
{
    live = liveVideo != nullptr && liveVideo->isStreaming();

    if (! live)
        return "Not live";

    const auto seconds = juce::jmax (0, static_cast<int> (liveVideo->getStats().seconds));
    return juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f LIVE "))
         + juce::String::formatted ("%02d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60)
         + (streamEnding ? juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ending")) : juce::String());
}

void MainComponent::controllerClosed()
{
    openLiveController (false);
}

void MainComponent::controllerMonitorChanged()
{
    // Shown: the picture's maker is made (the Controller's rows its sources), and makes it.
    if (liveControl != nullptr && liveControl->isMonitorShown())
        liveStudio();

    applyLivePreviewing();
    pushLive();
}
