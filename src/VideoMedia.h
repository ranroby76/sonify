// D:\workspace\Fanan_web_hub\src\VideoMedia.h

#pragma once

#include <juce_graphics/juce_graphics.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

//==============================================================================
/**
    Video, read and shown by VLC (as On Stage does): libvlc and its plugins folder next to
    Sonify, loaded the first time a video is used. Sonify works without them; only video
    doesn't.

    A video is prepared once (VideoMedia::Preparation): VLC reads it faster than it plays, its
    sound goes into a WAV lined up with its picture (VLC's own reading is a few milliseconds off
    for some kinds of file; the WAV is put right, so sample 0 is where the picture starts), and
    a strip of small pictures along it is made for the page. What's made is kept in the video
    cache, by the file's path, size and date, so the next time is instant.

    A video shown (VideoMedia::Player) plays without its sound (that plays in Sonify, as a clip
    or a playlist song) and follows where Sonify says it should be.
*/
namespace VideoMedia
{
    /** "" when VLC is there and loads; otherwise why video can't play. */
    juce::String problem();

    /** Lets VLC go (the app's end, once every player has gone). */
    void shutdown();

    /** The video files Sonify opens, for file choosers ("*.mp4;*.mov;..."). */
    juce::String wildcards();
    bool isVideoFile (const juce::File& file);

    /** Where prepared videos are kept (set by the app; a folder in the temp folder until then). */
    void setCacheFolder (const juce::File& folder);
    juce::File cacheFolder();

    /** Clears out of the cache, on a thread of its own, what no video has used for two months. */
    void tidyCache();

    //==============================================================================
    struct Info
    {
        double seconds = 0.0;
        int width = 0, height = 0;          // the picture as it's shown (its pixel shape applied)
        double fps = 0.0;
        bool hasSound = false;
        double soundRate = 0.0;             // the sound's sample rate in the file
    };

    struct Thumbnails
    {
        juce::File file;                    // a JPEG of them in a grid, left to right, then down
        int count = 0, columns = 0;
        int cellWidth = 0, cellHeight = 0;
        double every = 0.0;                 // seconds between them; the first is at 0

        bool isValid() const noexcept        { return count > 0 && columns > 0 && file.existsAsFile(); }
    };

    struct Prepared
    {
        Info info;
        juce::File sound;                   // its sound as a WAV (none: it has none, and no silence was asked for)
        Thumbnails thumbnails;
        juce::String error;                 // why it couldn't be prepared ("" when it was)

        bool ok() const noexcept             { return error.isEmpty(); }
    };

    /** What a preparation makes. */
    struct Wanted
    {
        bool sound = true;                  // the WAV
        bool silenceWhenNoSound = false;    // a silent WAV as long as the video, when it has no sound
        bool thumbnails = true;
    };

    /** What the cache already has for a video, without reading it (nothing: it must be prepared). */
    std::optional<Prepared> cached (const juce::File& video, Wanted wanted);

    /** A video prepared on a thread of its own. Ask it how it's going from the message thread. */
    class Preparation final
    {
    public:
        Preparation (const juce::File& video, Wanted wanted);
        ~Preparation();                     // stops it, if it's still going

        const juce::File& getVideo() const noexcept      { return video; }
        bool isDone() const noexcept;
        double getProgress() const noexcept;             // 0..1
        Prepared getResult() const;                      // when done
        void cancel();

    private:
        class Worker;
        juce::File video;
        std::unique_ptr<Worker> worker;

        JUCE_DECLARE_NON_COPYABLE (Preparation)
    };

    /** Prepares a video on the thread calling it (Preparation's work; the tests call it). */
    Prepared prepareNow (const juce::File& video, Wanted wanted, std::atomic<double>* progress = nullptr,
                         std::atomic<bool>* cancelled = nullptr);

    /** Reads the picture's details (no sound or pictures made). */
    std::optional<Info> probe (const juce::File& video, juce::String& error);

    //==============================================================================
    /**
        A video playing to the screen without its sound, following where it's told it should be.
        A window's timer calls follow() and paints takeFrame()'s picture. While it moves it plays
        at the rate asked, kept in step (a little faster or slower for a moment when it drifts,
        a jump when it's far off); standing still, it shows the picture at that moment. VLC that
        goes wrong (its Error), or stops moving while it should, is started again where it should
        be, a few times at most (further apart each time), so the picture comes back by itself.

        Its pictures come at the video's own size (its pixel shape applied) and its own frame rate,
        each one said as it comes (setOnFrame); no bigger than the biggest screen it can be shown
        on (HD at least), nor than 4K.

        Message thread, except VLC's own threads that hand it pictures.
    */
    class Player final
    {
    public:
        Player();
        ~Player();

        /** Opens a video (prepared: its Info says how big its picture is). biggestScreen: the
            computer's biggest screen (its pixels each way; none: not known), the most its pictures
            need. False: VLC can't. */
        bool open (const juce::File& video, const Info& info, juce::Point<int> biggestScreen = {});
        void close();
        const juce::File& getFile() const noexcept       { return file; }
        bool isOpen() const noexcept                     { return file != juce::File(); }

        /** Where the picture should be (seconds into the video), whether it's moving, how fast. */
        void follow (double seconds, bool moving, double rate);

        /** No picture (black), until follow() is called again with one. */
        void blank();

        /** The newest picture, in place of the image's last one; false when there's nothing new.
            None (an empty image): black. A picture taken is never written again while anyone holds
            it, so it can be kept and drawn on another thread too. */
        bool takeFrame (juce::Image& image);

        /** Called from VLC's own threads each time a new picture is there to take (to take it
            the moment it comes, at the video's frame rate). Set before a video is opened. */
        void setOnFrame (std::function<void()> callback);

        /** The picture's size as it's shown. */
        juce::Point<int> getPictureSize() const noexcept { return { outWidth, outHeight }; }

        /** For the tests: how far the picture was from where it should be, the last time it moved;
            and how many times VLC has been started again since the last good stretch. */
        double getLastDrift() const noexcept             { return lastDrift; }
        int getRestarts() const noexcept                 { return restarts; }

        struct Frames;      // VLC's pictures (shared with VLC's threads)

    private:
        class Grabber;

        void startMoving (double seconds, double rate, double now);
        void stopMoving();
        void restartVlc (double now);
        void setVlcRate (double rate);
        double estimate (double now) const;

        juce::File file;
        void* media = nullptr;
        void* player = nullptr;
        std::shared_ptr<Frames> frames;
        std::function<void()> told;
        std::unique_ptr<Grabber> grabber;
        int outWidth = 0, outHeight = 0;
        double fps = 25.0, length = 0.0;

        // Following (message thread)
        bool moving = false, learning = false;
        double vlcRate = 1.0, wantedRate = 1.0;
        double lead = 0.08;                    // how far ahead to jump, for the time VLC takes to get there
        double movedAt = 0.0, jumpedAt = -1.0e9, jumpedTo = 0.0;
        juce::int64 lastVlcTime = -1;
        double anchorSeconds = 0.0, anchorAt = -1.0;
        double shownStill = -1.0;              // the moment a still picture was asked for
        double lastDrift = 0.0;

        // Coming back by itself: VLC started again (gone wrong, or stuck), a few times at most.
        int restarts = 0;
        double restartAt = 0.0;                // not before then (further apart each time)
        double restartedAt = -1.0e9;
        double vlcTimeMovedAt = 0.0;           // when VLC last said a new time, moving
        bool ranOut = false;                   // its picture ended while the deck played on
        bool ranOutStillAsked = false;         // and its last picture asked for (nothing was showing)
        bool firstStillAsked = false;          // moving, nothing came: its first picture asked for
        double ranOutAt = 0.0;                 // where the deck was then (a jump back before it: VLC again)

        JUCE_DECLARE_NON_COPYABLE (Player)
    };

    //==============================================================================
    /**
        Has VLC write a video's picture again as H.264 in an MP4 (no sound), for a video whose
        picture can't go into an MP4 as it is. On the thread calling it.
    */
    juce::Result transcodePicture (const juce::File& video, const juce::File& target,
                                   const std::function<void (double)>& progress, std::atomic<bool>* cancelled);
}
