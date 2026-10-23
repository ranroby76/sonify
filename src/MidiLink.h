// D:\workspace\Fanan_web_hub\src\MidiLink.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

//==============================================================================
/**
    MIDI Link, as in Grex: move a knob, slider or wheel on a MIDI device (or hit a pad or a key)
    and it shows in the toolbar's MIDI Link strip, held there until something else on a device
    moves. Drag it onto one of Sonify's controls (or tap it, then the control) and the device
    moves that control from then on, as its link's type says:
        Knob        it follows the knob, slider or wheel (a button: on past the middle)
        Toggle      each press switches it on or off (a fader: all the way up, then down)
        Trigger     each press fires it once (a button: on; a fader: all the way up)
        Momentary   1 while pressed, 0 when let go (push in the files)
    Every type fits every control; a new link starts as the control suits (Knob for a fader from a
    knob, Momentary for a fader from a key or pad, Toggle for a button). The links are listed in
    the MIDI Link window (the strip's title opens it), with their types and a bin for each; a
    plugin's own knobs link through its Plugin Knob slot: touch the knob in the plugin's window,
    then drop the strip's control there.

    One source moves one control: linking it again takes it from the control it moved. What a
    linked source sends reaches no track (the engine keeps it back).

    The map belongs to the setup, not the song, as Grex's: the same in every song, kept next to
    Sonify's settings, and saved to and loaded from files of its own. So controls are named by
    where they are: "track.2.volume" is the second track's fader in any song, "fx.1.pan" the first
    FX channel's pan. A plugin's parameter is named by the plugin and the parameter's id, and
    moves the selected track's copy of that plugin (or the first one in the song).

    Message thread.
*/
class MidiLink final : private juce::Timer
{
public:
    //==============================================================================
    enum class Kind { cc, note, pitchBend };
    enum class Mode { push, toggle, knob, trigger };   // push: Momentary

    /** Where a link comes from: a controller, a note or the pitch wheel, on a channel of a port. */
    struct Source
    {
        juce::String port;                  // the MIDI port it comes in on, by name
        Kind kind = Kind::cc;
        int channel = 1;                    // 1 .. 16
        int number = 0;                     // the controller or the note; 0 for pitch bend

        /** "cc|1|74|Port Name": what the page and the files name it by. */
        juce::String key() const;
        static std::optional<Source> fromKey (const juce::String& key);

        /** "CC 74", "Note 60 (C4)" (middle C, as the Editor names it) or "Pitch Bend". */
        juce::String label() const;

        bool operator== (const Source& other) const noexcept;
        bool operator!= (const Source& other) const noexcept     { return ! operator== (other); }
    };

    /** What a link moves. key is one of:
            track.<n>.volume|pan|mute|solo|arm|monitor|send.<k>   the n-th track (as numbered)
            fx.<n>.volume|pan|mute|solo|send.<k>                  the n-th FX channel
            bus.<n>.volume|pan|mute|solo|send.<k>                 the n-th bus
            main.volume|mute
            board.<n>.<setting>, board.master.<setting>           the Analog Board: the n-th track's strip, its master
            live.deck.<n>.<setting>|play|cue|sync|hotcue<k>|loop<beats>, live.channel.<n>.<setting>,
            live.master.<setting>, live.pad.<n>, live.onair, live.record,  the Live Show: a deck (1 .. 4),
            live.mutemics, live.muteplayback (live.mute: both)       its n-th channel, its mixer, a pad,
                                                                  Enable streaming, REC and its mutes
            transport.play|stop|record|loop|metronome|start       start: back to the song's start
            launch.cell.<r>.<c>, launch.row.<r>, launch.stop.<c>, Song Blocks' Live (the clip launcher), by
            launch.stopall                                        place in its grid: a cell (the r-th block
                                                                  on the c-th track), a block, a track's
                                                                  stop, Stop All
            focus.volume|pan|mute|solo|arm|monitor|send.<k>,      Focus: the selected track's (whichever it
            focus.knob.<k>                                        is), and its k-th knob (1 .. 8: its
                                                                  instrument's, or its first insert's)
            plugin                                                a plugin's parameter (below) */
    struct Target
    {
        juce::String key;
        juce::String pluginId, pluginName, paramName;   // "plugin": the plugin's id and its parameter
        juce::uint32 paramId = 0;

        /** key, or "plugin|<pluginId>|<paramId>": one id per control. */
        juce::String id() const;

        /** A fader or a knob (Knob mode); the rest are buttons. */
        bool isContinuous() const;

        /** Something Sonify has (a well-formed key). */
        bool isValid() const;

        juce::var toVar() const;
        static Target fromVar (const juce::var& value);
    };

    struct Link
    {
        Source source;
        Target target;
        Mode mode = Mode::knob;
    };

    /** Sonify, as MIDI Link moves it. Message thread. */
    class Host
    {
    public:
        virtual ~Host() = default;

        /** A fader or knob to a position: 0 (all the way down, or left) .. 1. */
        virtual void linkMove (const Target& target, float position) = 0;

        /** A button on or off (start and stop act on "on"). */
        virtual void linkSet (const Target& target, bool on) = 0;

        /** Whether a button is on now (Toggle switches it the other way). */
        virtual bool linkIsOn (const Target& target) = 0;
    };

    using Message = std::pair<juce::String, juce::MidiMessage>;   // the port it came in on, and it
    using TakeIncoming = std::function<void (std::vector<Message>& into)>;

    /** takeIncoming hands over what came in since the last time; it's asked 60 times a second.
        None: messages come only through receive(). */
    MidiLink (Host& host, TakeIncoming takeIncoming);
    ~MidiLink() override;

    //==============================================================================
    /** The devices in Settings > MIDI Setup: the names the strip shows for their ports, and the
        ports whose notes are pads (a controller's). */
    void setDevices (std::map<juce::String, juce::String> deviceNamesByPort, juce::StringArray padPorts);
    juce::String deviceOf (const juce::String& port) const;

    /** A message from a port: it becomes what the strip shows, and plays its link if it has one. */
    void receive (const juce::String& port, const juce::MidiMessage& message);

    //==============================================================================
    // What the strip shows: the source that sent last (sample and hold).
    struct Recent
    {
        Source source;
        float value = 0.0f;                 // 0 .. 1, as it last sent
    };

    static constexpr size_t maxRecent = 1;
    const std::vector<Recent>& getRecent() const noexcept      { return recent; }

    //==============================================================================
    // The links.
    const std::vector<Link>& getLinks() const noexcept         { return links; }
    const Link* findLink (const Source& source) const;

    /** Links the source to the target, taking it from any control it moved (false: not a control
        Sonify has). */
    bool link (const Source& source, const Target& target, Mode mode);

    bool setMode (const Source& source, Mode mode);
    void unlinkTarget (const juce::String& targetId);   // every link to that control
    void unlinkSource (const Source& source);           // that source's link
    void clear();
    void setLinks (std::vector<Link> newLinks);          // as they were (Cancel), or loaded

    /** The type a new link to this target starts as: Knob for a fader, Toggle for a button. */
    static Mode defaultModeFor (const Target& target);
    static Mode defaultModeFor (const Source& source, const Target& target);   // Momentary for a fader from a key

    //==============================================================================
    // Files: the map kept next to Sonify's settings (saved at every change once it's set), and
    // maps saved by name (.sonifylink).
    juce::var toVar() const;
    juce::Result fromVar (const juce::var& value);     // links from a file; fails if it isn't a map
    bool save (const juce::File& file) const;
    juce::Result load (const juce::File& file);

    void setAutoFile (const juce::File& file)                  { autoFile = file; }
    static juce::File getDefaultAutoFile();                    // midilink.json, with Sonify's other settings
    static juce::File getMapsFolder();                         // <user data>\MIDI Link
    static constexpr const char* fileExtension = ".sonifylink";

    //==============================================================================
    /** The links changed: the engine keeps their sources from the tracks. */
    std::function<void (const std::vector<Source>&)> onLinkedSourcesChanged;

    /** Asked first of every message that comes in: true when another part of Sonify takes it
        (the Live Show's Controller: its pads, or one it's learning), and then it makes no chip
        and plays no link. */
    std::function<bool (const juce::String& port, const juce::MidiMessage& message)> claim;

    /** True once after something the strip shows changed: a new chip, a chip moved to the
        front, or the links. A chip's value alone isn't a change. */
    bool takeChanged() noexcept                                { return std::exchange (stripChanged, false); }

    //==============================================================================
    // Learn (MIDI Learn): a control waits, and the next knob, slider, wheel, pad or key a device
    // moves is linked to it (as the control suits: defaultModeFor); that message plays nothing.
    void learn (const Target& target)                          { learning = target; stripChanged = true; }
    void stopLearning()                                        { learning.reset(); stripChanged = true; }
    const std::optional<Target>& getLearning() const noexcept  { return learning; }

    /** The source Learn linked last (once), if it linked one since the last time asked. */
    std::optional<Source> takeLearned() noexcept               { return std::exchange (learned, std::nullopt); }

    static juce::String modeName (Mode mode);                  // "push", "toggle", "knob", "trigger"
    static Mode modeFromName (const juce::String& name, Mode fallback);

private:
    void timerCallback() override;
    void linksChanged();

    Host& host;
    TakeIncoming takeIncoming;
    std::vector<Message> incoming;
    std::vector<Recent> recent;
    std::vector<Link> links;
    std::vector<bool> held;                     // per link: its button or pad is down now
    std::vector<bool> raised;                   // per link: a fader a Toggle put all the way up
    std::map<juce::String, juce::String> deviceNames;
    juce::StringArray padPorts;
    juce::File autoFile;
    bool stripChanged = false;
    std::optional<Target> learning;             // Learn: the control waiting for a device's control
    std::optional<Source> learned;              // the source Learn linked last, not yet told

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLink)
};
