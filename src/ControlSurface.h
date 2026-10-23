// D:\workspace\Fanan_web_hub\src\ControlSurface.h

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

#include "Transport.h"

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

//==============================================================================
/**
    Control surfaces playing Sonify, as in Studio One's External Devices: Mackie Control,
    Mackie Control Extender and HUI, and the many surfaces that speak them (Behringer
    X-Touch, iCON Platform and QCon, PreSonus FaderPort, TASCAM's in their Mackie mode...).

    Their channel strips are the Console's channels, tracks first and then FX channels,
    eight to a surface. The surfaces line up in the order of the device list, so an Extender
    added after a Mackie Control carries channels 9 to 16, and the bank buttons move them
    all. A fader sets its channel's volume and its knob the pan (or a send's level); the
    strip's buttons record-arm, solo, mute and select; the master fader is Main's. The
    transport buttons, the jog wheel, the cursor keys, Undo and Save work too. Back go the
    motor faders, the lights, the names and values on the display, the meters and the song
    position.

    Message thread. The surfaces' input ports never reach the tracks: the engine keeps what
    they send for this (TakeIncoming hands it over), and this answers through their output
    ports.
*/
class ControlSurfaces final : private juce::Timer
{
public:
    //==============================================================================
    enum class Protocol { mackie, extender, hui };

    /** "mackie", "extender" or "hui" (anything else is Mackie Control). */
    static Protocol protocolFromName (const juce::String& name);
    static juce::String protocolName (Protocol protocol);       // as the page shows it

    /** A surface, from its device in Settings > MIDI Setup. */
    struct Config
    {
        juce::String name;                   // the device's: problems name it
        Protocol protocol = Protocol::mackie;
        juce::String input, output;          // MIDI ports by name: Receive From, Send To
    };

    /** A channel as the surfaces see it: a track or an FX channel. */
    struct Channel
    {
        enum class Kind { instrument, audio, fx };

        struct Send
        {
            int id = 0;
            float levelDb = 0.0f;
            bool on = true;
        };

        int id = 0;
        Kind kind = Kind::instrument;
        juce::String name;
        float volumeDb = 0.0f, pan = 0.0f;
        bool mute = false, solo = false, recordArm = false, selected = false;
        bool automationRead = true;
        std::vector<Send> sends;             // to FX channels, in order (sidechains left out)
    };

    /** Sonify as the surfaces see and change it. Message thread. */
    class Host
    {
    public:
        virtual ~Host() = default;

        /** The Console's channels in its order: tracks, then FX channels. Sends only when asked. */
        virtual std::vector<Channel> getSurfaceChannels (bool withSends) = 0;
        virtual float getMainVolume() = 0;
        virtual Transport::State getSurfaceTransport() = 0;
        virtual const TempoMap& getSurfaceTempoMap() = 0;  // the song's bars and time
        virtual double getSurfaceSongEnd() = 0;        // quarter notes
        virtual double getSurfaceFrameRate() = 0;      // the timecode display's frames per second

        virtual void setChannelVolume (int channelId, float db) = 0;   // mainChannel: Main
        virtual void setChannelPan (int channelId, float pan) = 0;
        virtual void setChannelMute (int channelId, bool mute) = 0;
        virtual void setChannelSolo (int channelId, bool solo) = 0;
        virtual void setChannelRecordArm (int channelId, bool arm) = 0;
        virtual void selectChannel (int channelId) = 0;
        virtual void setChannelSendLevel (int channelId, int sendId, float db) = 0;
        virtual void setChannelSendOn (int channelId, int sendId, bool on) = 0;
        virtual void setChannelAutomationRead (int channelId, bool read) = 0;

        virtual void surfaceTransport (const juce::String& action) = 0;   // play, stop, record, start
        virtual void surfaceLocate (double ppq) = 0;
        virtual void surfaceLoop (bool on) = 0;
        virtual void surfaceMetronome (bool on) = 0;
        virtual void surfaceUndo (bool undo) = 0;                         // false: redo
        virtual void surfaceSave() = 0;
        virtual void surfaceCommand (const juce::String& pageCommand) = 0; // zoomIn, zoomOut
    };

    using Message = std::pair<juce::String, juce::MidiMessage>;   // the port it came in on, and it
    using TakeIncoming = std::function<void (std::vector<Message>& into)>;

    ControlSurfaces (Host& host, TakeIncoming takeIncoming);
    ~ControlSurfaces() override;   // the surfaces go dark

    //==============================================================================
    /** The surfaces now: the device list or the MIDI ports changed. Surfaces that are the same
        keep what they show; new ones are greeted and shown everything. */
    void configure (const std::vector<Config>& list);
    bool isEmpty() const noexcept                       { return surfaces.empty(); }

    /** Why a surface can't work: a port not chosen, not there, or not opened, or a loopback
        cable used both ways. */
    juce::StringArray getProblems() const;

    /** Called when a problem shows up between configures (a loopback found), so the page hears. */
    std::function<void()> onProblemsChanged;

    /** The channels' levels (linear peaks since the last call, by channel id; mainChannel for
        Main), from the status the page gets, so the surfaces' meters show the same. */
    void setMeters (const std::map<int, float>& peaks);

    /** True for a while after a surface moved the channel's fader or knob (mainChannel: Main's
        fader). The page's faders follow those moves through the status. */
    bool movedLately (int channelId) const;

    static constexpr int mainChannel = 0;
    static constexpr int stripsPerSurface = 8;
    static constexpr float silentDb = -72.0f;   // Track::silentDb: at or below it the fader is closed
    static constexpr float maxDb = 10.0f;       // Track::maxDb

    /** Studio One's fader scale, as the page's: position (0 bottom .. 1 top) and dB. A closed
        fader is all the way down. */
    static float faderToDb (float position) noexcept;
    static float dbToFader (float db) noexcept;

    //==============================================================================
    // What the timer runs, open for tests: a message from a port, and one update (now in ms).
    void receive (const juce::String& port, const juce::MidiMessage& message, double nowMs);
    void update (double nowMs);

    /** A test hears what goes out here instead of through the ports (and no port is opened). */
    std::function<void (const juce::String& port, const juce::MidiMessage& message)> sendForTest;

private:
    //==============================================================================
    struct Surface;
    enum class View { all, instruments, audio, fx };
    enum class StripButton { record, solo, mute, select, knob };

    void timerCallback() override;

    // In
    static bool isOwnMessage (const Surface& surface, const juce::MidiMessage& message);
    void receiveMackie (Surface& surface, const juce::MidiMessage& message);
    void receiveHui (Surface& surface, const juce::MidiMessage& message);
    void mackieButton (Surface& surface, int note, bool down);
    void huiSwitch (Surface& surface, int zone, int port, bool down);
    void stripButton (int strip, StripButton button);
    void faderTouched (Surface& surface, int strip, bool down);
    void moveFader (Surface& surface, int strip, int value);
    void turnKnob (Surface& surface, int strip, int ticks);
    void applyMoves();

    // Doing
    void bankBy (int strips);
    void bankTo (bool end);
    void showView (View newView);
    void knobsForPan();
    void knobsForSend (int index);
    void startWinding (int direction);
    void wind();
    double position() const;
    void locate (double ppq);
    void stepBars (int direction);
    void stop();
    void cursor (int across, int upDown);
    void selectNext (int direction);
    void toggleReadMode();
    void clearAll (bool solos);
    void jog (int ticks);

    // Out
    void readSong();
    Channel* channelAt (int strip);
    int totalStrips() const noexcept;
    int lastBank() const noexcept;
    void feedback (Surface& surface);
    void feedbackMackie (Surface& surface);
    void feedbackHui (Surface& surface);
    void showDisplayMackie (Surface& surface);
    void showTimeMackie (Surface& surface);
    void showHuiText (Surface& surface);
    void showTimeHui (Surface& surface);
    void sendMeters (Surface& surface);
    void lightMackie (Surface& surface, int note, bool on);
    void lightHui (Surface& surface, int zone, int port, bool on);
    void send (Surface& surface, const juce::MidiMessage& message);
    void goDark (Surface& surface);
    void greet (Surface& surface);
    juce::String valueText (const Channel& channel) const;
    int ringFor (const Channel* channel) const;

    //==============================================================================
    Host& host;
    TakeIncoming takeIncoming;
    std::vector<std::unique_ptr<Surface>> surfaces;

    // The song as of the last look, and the channels the view shows (indexes into channels).
    std::vector<Channel> channels;
    std::vector<int> shown;
    Transport::State transportNow;
    float mainVolume = 0.0f;
    bool songChanged = true;          // something was done since the last look

    // Shared by all the surfaces: one bank, one view, one job for the knobs.
    int bank = 0;                     // the shown channel on the first strip
    View view = View::all;
    bool sendKnobs = false;           // the knobs set a send's level, not the pan
    int sendIndex = 0;
    bool showVolume = false;          // NAME/VALUE: the display's lower line shows the volume
    bool showSmpte = false;           // SMPTE/BEATS: the time in hours, minutes, seconds, frames
    bool zoom = false, scrub = false;
    bool shift = false, option = false;
    int winding = 0;                  // -1 rewinding, +1 fast forwarding (button held)
    double windAgain = 0.0;
    int jogTicks = 0;
    double locatedTo = 0.0, locatedAt = -1.0e12;   // the last jump, which the transport may not have made yet
    double now = 0.0;

    std::map<std::pair<Surface*, int>, int> faderMoves;   // this update's: where each fader went
    std::map<std::pair<Surface*, int>, int> knobTurns;    // and how far each knob turned
    std::map<int, double> moved;                           // channel id -> when a surface moved it
    std::map<int, float> meters;                           // channel id -> peak
    bool metersNew = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlSurfaces)
};
