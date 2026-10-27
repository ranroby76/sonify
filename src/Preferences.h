// D:\workspace\Fanan_web_hub\src\Preferences.h

#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <utility>
#include <vector>

//==============================================================================
/**
    Sonify's own settings, the ones File > Settings shows: how the arrangement and the Editor
    look, which DAW's keyboard shortcuts it uses, whether remote control apps can find it on
    the network, where the user's things go, whether the audio device is let go while Sonify
    is in the background, the external MIDI devices (keyboards, controllers, drum kits,
    instruments and control surfaces) with what Sonify sends them (MIDI clock, MIDI timecode),
    and the MIDI input ports turned off.

    The audio device itself keeps its own file (audio.xml), as before.

    Saved as settings.json in Sonify's settings folder (SettingsFolder below), with Sonify's other
    settings: a new build of Sonify keeps them. Message thread only.
*/
class Preferences final
{
public:
    //==============================================================================
    /** An external MIDI device, as in Studio One's External Devices (Settings > MIDI Setup): a
        keyboard, a controller (pads, knobs) or a drum kit that plays notes, an instrument (a synth
        or drum machine with MIDI in), or a control surface that plays Sonify itself (Mackie
        Control, its Extender, HUI). Tracks choose the others by name; split channels make one
        input per MIDI channel. A control surface's port plays no track. */
    struct Device
    {
        juce::String id;                    // made when it's added; never changes
        juce::String model;                 // "New Keyboard", "New Instrument" or a model from the list
        juce::String kind = "keyboard";     // keyboard, controller, drums, instrument or surface
        juce::String protocol = "mackie";   // a surface's: mackie, extender or hui
        juce::String manufacturer;
        juce::String name;                  // unique; what tracks choose it by
        juce::String input, output;         // MIDI ports by name: Receive From, Send To (empty: none)
        int channels = 0xffff;              // the MIDI channels it uses, one bit each (bit 0: channel 1)
        bool filterAftertouch = false;      // these never reach the tracks
        bool filterProgramChange = false;
        bool filterPitchBend = false;
        bool filterControllers = false;
        bool splitChannels = false;         // one track input per channel
        bool defaultInput = false;          // new instrument tracks take it
        bool mpe = false;                   // MPE: per-note expression on its own channels
        int pitchRange = 48;                // semitones, for MPE
        bool sendClock = false;             // MIDI clock to its output while Sonify plays
        bool clockStart = false;            // Start rather than Song Position + Continue
        bool sendTimecode = false;          // MIDI timecode (MTC) to its output
    };

    //==============================================================================
    int luminance = 0;                      // 0 dark .. 100 bright: the arrangement and the Editor
    int contrast = 50;                      // 0 low .. 100 high: text and symbols against the dark
    juce::String accent = "green";          // the colour that leads the palette (the Start page): see getAccents
    juce::String keymap = "studioOne";      // see keymaps
    bool discoverable = false;              // remote control apps on the network can find Sonify
    juce::String userData;                  // empty: the default, Documents\Sonify
    juce::String browserFolder;             // the Browser's Files tab: the folder it last showed
    juce::String filesFolder;               // the Files tab's Open: the folder new songs (and songs without one) show
    juce::String clipExportFolder;          // where Export Clip to Audio writes (empty: the song's Exports)
    juce::String nodeRackFolder;            // where a Node Rack's Browse opens (empty: the VST3 folder)
    bool autosave = true;
    int autosaveMinutes = 5;
    bool releaseInBackground = false;       // let the audio device go while another app is in front
    juce::String timecodeRate = "30";       // MIDI timecode frames: 24, 25, 29.97 (drop frame) or 30
    std::vector<Device> devices;
    juce::StringArray midiInputsOff;        // MIDI input ports turned off by hand (by name); the rest are listened to
    juce::var songIo;                       // Song Setup's Make Default: {inputs, outputs, audition} new songs start with
    std::map<juce::String, bool> trackOptions;   // the track list's Options menu, by name (getTrackOptions)

    static constexpr int minAutosaveMinutes = 1;
    static constexpr int maxAutosaveMinutes = 120;

    /** The keyboard shortcut maps the page knows: Studio One's (the default), Cubase's, Logic's,
        Pro Tools', Ableton Live's, Cakewalk's and FL Studio's. */
    static juce::StringArray getKeymaps();

    /** The colours that may lead the palette (the Start page; the page holds their shades). */
    static juce::StringArray getAccents();
    static juce::StringArray getTimecodeRates();

    /** The track list's Options menu (its wrench, as Studio One's), each by name with its default:
          soloFollowsSelection             the selected tracks are the ones soloed
          instrumentInputFollowsSelection  the instrument track selected monitors (the others stop,
                                           unless armed)
          audioInputFollowsSelection       the same for audio tracks
          locateOnEmptyClick               a click on the lanes' empty space moves the song position there
          cursorFollowsEdit                a clip clicked or moved takes the song position to its start
          automationFollowsEvents          a clip moved (or copied) takes its track's envelopes under it along
          noOverlap                        a clip moved, resized or pasted over others trims them
          colorizeTracks                   track headers in their track's colour
          showTrackNumbers                 the number on each track's header
          showClipNames                    the name on each clip
          monitorFollowsRecord             arming a track monitors it; disarming stops it
          scrollPausesAutoscroll           scrolling by hand while playing lets Autoscroll go until the
                                           playhead is in view again (or the song starts again) */
    static const std::vector<std::pair<juce::String, bool>>& getTrackOptions();

    /** A Track Option: as chosen, or its default. */
    bool trackOption (const juce::String& name) const;

    //==============================================================================
    void load();
    bool save() const;

    /** Everything, as the page gets it. */
    juce::var toVar() const;

    const Device* findDevice (const juce::String& id) const;

    /** Adds the device, or replaces the one with its id. Its name is made unique among the
        others, and only one device is the default input. Returns its id. */
    juce::String putDevice (Device device);
    bool removeDevice (const juce::String& id);

    /** A name no other device has: "Keys", else "Keys (2)" and so on. Never empty; "|" is
        left out (it names a channel in a track's input). */
    juce::String uniqueDeviceName (const juce::String& wanted, const juce::String& exceptId) const;

    static juce::var deviceToVar (const Device& device);
    static Device deviceFromVar (const juce::var& value);
    static juce::File getFile();

private:
    void tidy();
    juce::var trackOptionsToVar() const;
};

//==============================================================================
/**
    Where Sonify keeps its settings and what it learns (the audio device, the Live Show's tracks,
    its screens and stream, MIDI Link, recent songs...): AppData\Roaming\Fanan\Sonify on Windows
    (Library/Fanan/Sonify on a Mac), the same for every build and copy of Sonify on the computer, so
    a new build (its build folder made again) loses none of them. Any thread.
*/
namespace SettingsFolder
{
    juce::File get();      // made the first time it's asked for

    /** A settings file there. One a Sonify kept beside Sonify.exe before (as oldName) is copied
        over the first time it's asked for (the old one is left where it was). */
    juce::File file (const juce::String& name, const juce::String& oldName = {});

    /** A folder there (made when asked for). One a Sonify kept under AppData\Fanan Web Hub
        before (oldName, in roaming or, on Windows, local AppData) is moved over the first time
        it's asked for, and the old Fanan Web Hub folder goes once nothing is left in it. */
    juce::File folder (const juce::String& name, const juce::String& oldName = {});

    /** Windows: the one WebView2 profile every web view in Sonify uses (its page, the Web
        Browser's tabs, the store's videos), beside Sonify.exe where the page's always was. One
        profile is one browser process for them all: a new view opens in a moment, where a
        profile of its own started a browser of its own first and froze Sonify for seconds. */
    juce::File webViewProfile();

    /** The profiles the Web Browser's tabs and the store's videos had of their own before: gone,
        on a thread of their own (whatever's left goes at the next start). Any thread. */
    void removeOldWebViewProfiles();
}

//==============================================================================
/** Where the user's things go (Settings > Locations > User Data): new songs, recordings of
    songs not saved yet, mixdowns and autosaves. Message thread. */
namespace UserData
{
    juce::File getDefaultFolder();                 // Documents\Sonify
    juce::File getFolder();                        // the chosen one, or the default
    void setFolder (const juce::String& folder);   // empty: the default

    juce::File songs();                            // <user data>\Songs
    juce::File recordings();                       // <user data>\Recordings
    juce::File mixdown();                          // <user data>\Mixdown
    juce::File autosave();                         // <user data>\Autosave
}
