// D:\workspace\Fanan_web_hub\src\LiveController.h

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "LiveStream.h"
#include "MidiLink.h"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <vector>

//==============================================================================
/**
    The Live Show's Controller (Live streaming): which picture does what, in a window of its own
    with its own button on the taskbar, so it minimises there. Minimised, it goes on working, and
    so do its pads (Sonify's window minimised too).

    Its rows: up to four cameras, then the screen. Each row's camera (or screen) is picked in it,
    and each row is the Main picture (one at a time: a new one sends the last to Hidden), a box of
    its own size and place over it (Custom: several at once; dragged and sized on the preview at
    the top), or Hidden. Every change is a cut. A camera's Sync holds its pictures a little, so a
    quick camera waits for a slower one. The preview shows the picture laid out, streaming
    enabled or not.

    Two pictures: the landscape one (YouTube, Facebook, Twitch) and the vertical one (TikTok). Each
    row shows the same way in both (Main, Custom or Hidden), but has a box of its own in each; and
    Main fills the vertical picture, dragged across on the preview for the part of it that shows.
    The tabs over the preview pick which picture it shows (and the Monitor with it).

    Presets: the rows as they are (each one Main, Custom or Hidden, and its boxes), each a file of
    its own (<user data>\Live Controller, .sonifycontrol). The list on the right picks one at a
    click, at once. Save keeps the rows in the preset picked; Save as makes a new one.

    MIDI: the device it listens to (any of the computer's MIDI inputs but a control surface's),
    and Learn: with Learn on, click a button (a row's Main, Custom or Hidden, a preset in the list,
    Enable streaming, Mute mics, Mute playback), then press a pad or a button on the device; a
    right-click forgets a button's pad. The pads are the Controller's, not a preset's, so a pad
    never changes what it does when a preset loads. With Pad lights on, a controller with lights
    has the pads of what's live lit (sent to its MIDI output of the same name). What its pads send
    plays the Controller, never a track.

    The Monitor: the picture exactly as the viewers see it (streaming not enabled: the show's
    name), in a small window of its own over every other window. It's never in the stream: Windows
    leaves it out of every screen capture (the Screen row's too). Moved by its bar, sized by its
    corner; minimised to its bar, maximised, or closed (the Monitor button, the page's pill or a
    pad opens it again).

    Its setup (each row's camera, how it shows, its box and sync, the device, the pads, the
    preset picked, its window's and the Monitor's places) is the computer's, kept next to
    Sonify's settings.

    Message thread.
*/
namespace LiveControl
{
    constexpr int numRows = LiveStream::numSources;     // the cameras, then the screen
    constexpr int screenRow = LiveStream::screenSource;
    using Show = LiveStream::Show;

    struct Row
    {
        juce::String device;                    // the camera's id, or the screen's (empty: none)
        Show show = Show::hidden;
        juce::Rectangle<float> box;             // its Custom box in the picture (0..1 across and down)
        juce::Rectangle<float> tallBox;         // its Custom box in the vertical picture
        float tallFocus = 0.5f;                 // Main in the vertical picture: the part across that shows (0 .. 1)
        int delayMs = 0;                        // a camera's Sync: its pictures held this long
    };

    struct Setup
    {
        std::array<Row, numRows> rows;
        juce::String midiPort;                  // the MIDI input it listens to, by name (empty: none)
        bool lights = true;                     // the pads of what's live lit
        std::map<juce::String, juce::String> pads;   // a button's action -> its MIDI source (MidiLink::Source::key)
        juce::String preset;                    // the preset picked last (empty: none)
        juce::Rectangle<int> window;            // where its window was (empty: centred on Sonify's)
        juce::Rectangle<int> monitor;           // the Monitor's own place and size (empty: the main screen's top right)
        juce::String monitorMode { "normal" };  // "normal", "min" (its bar) or "max"
        bool viewTall = false;                  // the preview (and the Monitor) shows the vertical picture

        /** The test pattern on Main in the first row, each box in a place of its own. */
        static Setup standard();
        static juce::Rectangle<float> standardBox (int row);
        static juce::Rectangle<float> standardTallBox (int row);

        juce::var toVar() const;
        static Setup fromVar (const juce::var& value);
    };

    /** A row's name: "Camera 1" .. "Camera 4", "Screen". */
    juce::String rowName (int row);

    /** Show as it's named in files and actions: "main", "custom", "hidden". */
    juce::String showName (Show show);
    Show showFromName (const juce::String& name);

    //==============================================================================
    /** What the Controller asks of Sonify. */
    class Host
    {
    public:
        virtual ~Host() = default;

        /** The rows changed: the stream's picture takes them (a cut). */
        virtual void controllerChanged() = 0;

        /** The picture as it's laid out; as it goes out (what the viewers see); a row's newest
            picture; why a row's source won't open. */
        virtual juce::Image controllerPicture() = 0;
        virtual juce::Image controllerOutputPicture() = 0;

        /** The vertical picture as it's laid out, and as it goes out (while it's made). */
        virtual juce::Image controllerTallPicture() = 0;
        virtual juce::Image controllerTallOutputPicture() = 0;

        /** The preview's picture picked (the landscape one or the vertical one). */
        virtual void controllerViewChanged() = 0;
        virtual juce::Image controllerSourcePicture (int row) = 0;
        virtual juce::String controllerSourceProblem (int row) = 0;

        /** The cameras (looked for again when asked: a moment) and the screens. */
        virtual std::vector<LiveStream::Device> controllerCameras (bool lookAgain) = 0;
        virtual std::vector<LiveStream::Device> controllerScreens() = 0;

        /** The MIDI inputs it can listen to, and the one chosen (Sonify listens to it). */
        virtual juce::StringArray controllerMidiPorts() = 0;
        virtual void controllerMidiPortChosen (const juce::String& port) = 0;

        /** The show's switches: "stream" (Enable streaming), "mics" (Mute mics), "playback"
            (Mute playback). */
        virtual bool controllerSwitch (const juce::String& which) = 0;
        virtual void controllerSetSwitch (const juce::String& which, bool on) = 0;

        /** How the stream goes, in a few words ("LIVE 00:12:34", "Not live"), and whether it's live. */
        virtual juce::String controllerStatus (bool& live) = 0;

        /** Its window closed (its close button); the Monitor shown or hidden. */
        virtual void controllerClosed() = 0;
        virtual void controllerMonitorChanged() = 0;
    };

    //==============================================================================
    class Controller final : private juce::Timer
    {
    public:
        /** setupFile: where its setup is kept; fallback: the setup when there's none yet. */
        Controller (Host& host, const juce::File& setupFile, const Setup& fallback);
        ~Controller() override;

        const Setup& getSetup() const noexcept           { return setup; }

        /** The rows as the stream's sources. */
        std::array<LiveStream::Source, numRows> getSources() const;

        //==============================================================================
        // The rows.
        void setShow (int row, Show show);              // Main sends the last Main to Hidden
        void setBox (int row, juce::Rectangle<float> box);
        void setTallBox (int row, juce::Rectangle<float> box);
        void setTallFocus (int row, float focus);

        /** Which picture the preview and the Monitor show: the vertical one, or the landscape one. */
        void setViewTall (bool tall);
        bool isViewTall() const noexcept                 { return setup.viewTall; }
        void setDevice (int row, const juce::String& device);
        void setDelay (int row, int ms);

        //==============================================================================
        // Presets.
        juce::StringArray getPresets() const;           // by name, sorted
        bool loadPreset (const juce::String& name);
        bool savePreset (const juce::String& name);     // the rows as they are (made, or replaced)
        bool deletePreset (const juce::String& name);
        bool renamePreset (const juce::String& from, const juce::String& to);   // its pad goes with it
        bool isPresetChanged() const noexcept            { return presetChanged; }   // the rows moved since it was picked
        static juce::File getPresetsFolder();           // <user data>\Live Controller
        static constexpr const char* presetExtension = ".sonifycontrol";

        //==============================================================================
        // MIDI.
        /** A message from a port: true when it's the Controller's (its device's pad, bound or
            being learned). */
        bool handleMidi (const juce::String& port, const juce::MidiMessage& message);

        void setMidiPort (const juce::String& port);
        void setLights (bool on);

        /** Learn: on, a click arms a button; the next pad pressed on the device is its. */
        void setLearnMode (bool on);
        bool isLearnMode() const noexcept                { return learnMode; }
        void learn (const juce::String& action);
        const juce::String& getLearning() const noexcept { return learning; }
        void forget (const juce::String& action);

        /** A button's pad as a chip names it ("C3", "CC 20"), or empty. */
        juce::String padLabel (const juce::String& action) const;

        /** Every pad bound: the engine keeps them from the tracks. */
        std::vector<MidiLink::Source> getPadSources() const;

        /** Does what a button does: "row.<n>.main|custom|hidden" (n 1 .. 5), "preset.<name>",
            "switch.stream|mics|playback", "show.monitor". */
        void perform (const juce::String& action);

        /** Whether a button's thing is on now (a pad's light, the button lit). */
        bool isLit (const juce::String& action);

        //==============================================================================
        // Its window.
        void showWindow (bool shouldShow);
        bool isWindowShown() const noexcept              { return window != nullptr; }

        // The Monitor.
        void showMonitor (bool shouldShow);
        bool isMonitorShown() const noexcept             { return monitor != nullptr; }

        /** Something it shows changed outside it (the show's switches, the stream). */
        void refresh();

        std::function<void()> onPadsChanged;            // the engine's kept sources

    private:
        class Window;
        class Panel;
        class Monitor;

        void changed (bool rows);                       // rows: the picture takes them
        void saveSoon();
        void save();
        void timerCallback() override;
        void openLights();
        void sendLight (const MidiLink::Source& source, bool lit);
        void sendLights (bool all);                     // all: every pad (else the ones that changed)
        void sendLightsOff();
        static std::optional<MidiLink::Source> sourceOf (const juce::String& port, const juce::MidiMessage& message, bool& down);
        juce::String actionOf (const MidiLink::Source& source) const;
        juce::File presetFile (const juce::String& name) const;
        void say (const juce::String& words);

        Host& host;
        juce::File file;
        Setup setup;
        bool presetChanged = false;
        bool learnMode = false;
        juce::String learning;
        bool saveDue = false;
        double changedAt = 0.0;
        std::unique_ptr<juce::MidiOutput> lights;
        juce::String lightsPort;
        std::map<juce::String, bool> litSent;
        double lightsTried = 0.0;
        std::unique_ptr<Window> window;
        std::unique_ptr<Monitor> monitor;

        JUCE_DECLARE_NON_COPYABLE (Controller)
    };
}
