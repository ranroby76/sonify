// D:\workspace\Fanan_web_hub\src\PlaylistPlayer.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include "Plugin.h"
#include "VideoMedia.h"

#include <map>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

class PlaylistPlayer;

//==============================================================================
/**
    The Playlist Player: Sonify's system tool for playing songs live, as OnStage's Media page
    does. It goes on a track's instrument slot (or into a container) and plays a playlist of
    audio files and videos, each song with its own volume, pitch (in semitones, its speed kept),
    speed (its pitch kept), wait (how long after it ends the next one starts, in milliseconds)
    and crossfade (how long it overlaps the next one). Auto-Play goes on down the list.

    A video's sound is read once, as it's added (VideoMedia: a WAV kept in the video cache), and
    plays like any song's; its picture shows in the player's window (and its Video window) where
    the song is, at its speed. A video still being read can't play yet.

    It's played like an instrument: its window has PLAY / PAUSE and STOP, and the track's notes
    play it too, from a keyboard or from clips: note 15 is PLAY / PAUSE and note 16 is STOP, on
    any channel. Its sound goes through the track like any instrument's.

    Songs are read from disk as they play, a little ahead, on a thread of the player's own. The
    playlist is kept with the song, and can be saved as a .playlist file.

    Threads: the playlist on the message thread; process() on the audio thread. Which song plays
    and when (Auto-Play, waits, crossfades) is decided on the audio thread, so a mixdown renders
    it as it would play.
*/
namespace Playlists
{
    inline constexpr const char* playerId = "sonify.builtin.playlist";
    inline constexpr int playPauseNote = 15;
    inline constexpr int stopNote = 16;
    inline constexpr const char* fileExtension = ".playlist";

    inline constexpr float minVolume = 0.0f, maxVolume = 2.0f;        // times the file's own level
    inline constexpr float minPitch = -12.0f, maxPitch = 12.0f;       // semitones
    inline constexpr float minSpeed = 0.5f, maxSpeed = 1.5f;
    inline constexpr int maxWaitMs = 30000;
    inline constexpr int maxCrossfadeMs = 10000;

    Plugin::Descriptor describe();

    /** Where Sonify keeps the user's things (Documents\Sonify unless the app says otherwise):
        playlists are saved in its Playlists folder. */
    void setUserFolder (std::function<juce::File()> userData);
    juce::File folder();

    /** Where Add Tracks starts: Set Default Folder's choice, the same for every player (kept in
        the Playlists folder). The user's Music folder until one is set. */
    juce::File mediaFolder();
    bool setMediaFolder (const juce::File& mediaFolder);

    /** The player's window: made by PlaylistScreen.cpp in the app (none in the engine tests). */
    struct Screen
    {
        virtual ~Screen() = default;
        virtual void show (const juce::String& title) = 0;
        virtual void hide() = 0;
        virtual bool isShowing() const = 0;
    };

    using ScreenFactory = std::function<std::unique_ptr<Screen> (PlaylistPlayer&)>;
    void setScreenFactory (ScreenFactory factory);
}

//==============================================================================
/** One song in a playlist. */
struct PlaylistSong
{
    juce::String file, title;
    float volume = 1.0f;
    float pitch = 0.0f;
    float speed = 1.0f;
    int waitMs = 0;               // after it ends, before the next one starts (Auto-Play)
    int crossfadeMs = 0;          // the next one starts this long before it ends (Auto-Play)
    bool expanded = false;        // its controls showing in the playlist

    juce::uint32 id = 0;          // the player's own name for it, while the player lives
    bool missing = false;         // its file can't be read

    // A video: its sound, read once (what plays), while it's being read, and why it couldn't be.
    bool video = false;
    juce::String sound;
    bool preparing = false;
    juce::String problem;
    juce::String failedFor;       // the file (path, size and date) a reading failed for: not read again

    /** The file that's played: a video's sound, or the song's own file. */
    const juce::String& playedFile() const noexcept   { return video ? sound : file; }
};

//==============================================================================
class PlaylistPlayer final : public Plugin,
                             private juce::Timer
{
public:
    PlaylistPlayer();
    ~PlaylistPlayer() override;

    //==============================================================================
    // The playlist, message thread.
    const std::vector<PlaylistSong>& getSongs() const noexcept      { return songs; }

    /** Adds audio files and videos (at the end, or before a song); returns how many it could take. */
    int addSongs (const juce::Array<juce::File>& files, int insertBefore = -1);
    void removeSong (int index);
    void moveSong (int from, int to);
    void clearSongs();

    void setVolume (int index, float volume);
    void setPitch (int index, float semitones);
    void setSpeed (int index, float speed);
    void setWait (int index, int milliseconds);
    void setCrossfade (int index, int milliseconds);
    void setExpanded (int index, bool expanded);

    /** The song PLAY starts when nothing plays (it follows what plays). -1 when there's none. */
    int getSelected() const;
    void select (int index);

    bool isAutoPlay() const noexcept                                { return autoPlay.load(); }
    void setAutoPlay (bool on);

    /** The files Add Tracks takes (the audio formats Sonify reads, and videos). */
    juce::String getWildcards() const;

    /** How far a video song's reading has got (0..1), or -1 when it isn't being read. */
    double getPreparation (int index) const;

    bool savePlaylist (const juce::File& file) const;
    bool loadPlaylist (const juce::File& file);

    /** The playlist's songs changed (added, removed, moved, opened or closed, read again):
        the window builds its list again. Not for a song's settings: they're shown as they're set. */
    std::function<void()> onSongsChanged;

    //==============================================================================
    // Playing, message thread (the audio thread acts on it within a block).
    void playSong (int index);      // now, from its start
    void playPause();               // PLAY / PAUSE (note 15)
    void stop();                    // STOP (note 16)
    void seek (double fraction);    // in the song playing (or the one PLAY starts)

    enum class State { stopped, playing, paused, waiting };

    struct Status
    {
        State state = State::stopped;
        int song = -1;              // playing, paused, or next after a wait; -1 when none
        double position = 0.0;      // seconds into it
        double length = 0.0;        // its length in seconds
        double waitLeft = 0.0;      // seconds until the next song starts
    };

    Status getStatus() const;

    /** Upkeep, message thread: the player's timer calls it (the tests call it themselves). */
    void upkeep();

    //==============================================================================
    // Plugin
    juce::String getFormatName() const override                          { return "Built-in"; }
    const Descriptor& getDescriptor() const noexcept override            { return descriptor; }
    const std::vector<Descriptor>& getPluginsInFile() const noexcept override { return inFile; }
    const juce::File& getSourceFile() const noexcept override;
    Role getRole() const noexcept override                               { return Role::instrument; }
    int getMainInputChannels() const noexcept override                   { return 0; }
    int getMainOutputChannels() const noexcept override                  { return 2; }

    bool canSaveState() const noexcept override                          { return true; }
    bool saveState (juce::MemoryBlock& destination) override;
    bool loadState (const juce::MemoryBlock& source) override;
    bool consumeStateChanged() noexcept override                         { return stateChanged.exchange (false); }

    const std::vector<Parameter>& getParameters() const noexcept override { return noParameters; }
    int findParameter (uint32_t) const noexcept override                 { return -1; }
    double getParameterValue (int) const noexcept override               { return 0.0; }
    double readParameterValue (int) override                             { return 0.0; }
    juce::String getParameterText (int, double) override                 { return {}; }
    double queueParameter (int, double) override                         { return 0.0; }

    bool activate (double sampleRate, int maxBlockSize) override;
    void deactivate() override;
    bool isActive() const noexcept override                              { return running.load(); }

    void process (const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                  int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t* transport) noexcept override;

    ScreenKind getScreenKind() const noexcept override                   { return ScreenKind::native; }
    bool showNativeScreen (const juce::String& title) override;
    void hideNativeScreen() override;

    class Stream;
    struct Scratch;

private:
    enum class Start { cue, play, seek };
    enum Command { playPauseCommand = 1, stopCommand, cancelPendingCommand, takeActiveCommand, dropSongCommand };

    void timerCallback() override                                        { upkeep(); }

    // Message thread
    int indexOf (juce::uint32 songId) const noexcept;
    PlaylistSong* songById (juce::uint32 songId) noexcept;
    juce::uint32 nextPlayableAfter (juce::uint32 songId) const;
    void readSong (PlaylistSong& song);
    void updatePreparations();
    void dropUnusedPreparations();
    void songsChanged();
    void settingsChanged (int index);
    void readyStretchFor (int index);
    void setSelectedId (juce::uint32 id);
    void recheckMissing();
    juce::var toVar (bool withSelection) const;
    bool fromVar (const juce::var& state);
    void replaceSongs (std::vector<PlaylistSong> newSongs, int selectedIndex);
    Stream* makeStream (int index, double startFraction, Start start);
    Stream* findStream (juce::uint32 serial) const noexcept;
    void postActive (Stream* stream);
    void postUpcoming (Stream* stream);
    void dropPosted (juce::uint32 songId);
    void send (int command, juce::uint32 song = 0);
    bool isAlive (juce::uint32 serial) const noexcept;
    void destroy (Stream* stream);
    void collectRetired();
    void dropAllStreams();

    // Audio thread
    void renderSpan (float* outLeft, float* outRight, int from, int to, bool offline) noexcept;
    void takeMail (bool offline) noexcept;
    void takeUpcomingMail() noexcept;
    void swapIn (Stream* stream) noexcept;
    void runCommand (int command, juce::uint32 song, bool offline) noexcept;
    void playPauseNow (bool offline) noexcept;
    void stopNow() noexcept;
    void dropEverything() noexcept;
    juce::uint32 nextAfter (juce::uint32 songId, bool offline) noexcept;
    bool upcomingReady (juce::uint32 songId, bool offline) noexcept;
    bool startUpcoming (bool offline) noexcept;
    void songEnded (bool offline) noexcept;
    void letGo (Stream* stream, int fadeSamples) noexcept;
    void toFading (Stream* stream, int fadeSamples) noexcept;
    void retire (Stream* stream) noexcept;
    void publish() noexcept;
    int samplesFromMs (int ms) const noexcept;

    static juce::uint64 planOf (juce::uint32 after, juce::uint32 next) noexcept
    {
        return (static_cast<juce::uint64> (after) << 32) | next;
    }

    Descriptor descriptor;
    std::vector<Descriptor> inFile;
    static const std::vector<Parameter> noParameters;

    // The playlist (message thread)
    std::vector<PlaylistSong> songs;
    juce::uint32 nextSongId = 1, selectedId = 0, lastShownId = 0;
    juce::AudioFormatManager formats;
    juce::TimeSliceThread readThread { "Sonify Playlist Player" };
    std::vector<std::unique_ptr<Stream>> streams;     // every stream made, until the audio thread lets it go
    std::map<juce::String, std::unique_ptr<VideoMedia::Preparation>> preparations;   // videos being read, by file
    juce::uint32 nextSerial = 1, postedActive = 0, postedUpcoming = 0, postedUpcomingSong = 0;
    int upkeepCount = 0;
    std::atomic<bool> stateChanged { false }, running { false }, autoPlay { true };

    // Message thread to audio thread
    std::atomic<Stream*> newActive { nullptr }, newUpcoming { nullptr };
    std::atomic<juce::uint64> nextPlan { 0 };              // planOf (the song playing, the one Auto-Play goes on to)
    std::atomic<juce::uint32> selectedShared { 0 };        // the song PLAY starts
    juce::AbstractFifo commandFifo { 64 };
    std::array<int, 64> commands {};
    std::array<juce::uint32, 64> commandSongs {};          // the song a command is about (dropSongCommand)

    // Audio thread to message thread
    juce::AbstractFifo retiredFifo { 256 };
    std::array<Stream*, 256> retired {};
    std::array<Stream*, 64> backlog {};                  // retired while the FIFO was full
    int backlogCount = 0;
    std::atomic<int> shownState { 0 };
    std::atomic<juce::uint32> shownSong { 0 }, activeSong { 0 }, activeSerial { 0 };
    std::atomic<double> shownPosition { 0.0 }, shownLength { 0.0 }, shownWait { 0.0 };

    // The audio thread's own
    State state = State::stopped;
    Stream* current = nullptr;                // playing, paused, or cued for PLAY
    Stream* pendingActive = nullptr;          // posted, being read ahead before it takes over
    std::array<Stream*, 4> fadingOut {};      // let go of, fading out
    Stream* upcoming = nullptr;               // Auto-Play's next song, read ahead
    juce::uint32 waitingFor = 0;              // the song a wait leads to
    bool pausing = false, playWhenCued = false, wasOffline = false;
    juce::int64 waitLeft = 0, cueWaitLeft = 0;
    double deviceRate = 0.0;
    std::unique_ptr<Scratch> scratch;

    std::unique_ptr<Playlists::Screen> screen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaylistPlayer)
};
