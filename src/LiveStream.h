// D:\workspace\Fanan_web_hub\src\LiveStream.h

#pragma once

#include <juce_graphics/juce_graphics.h>

#include "LiveShow.h"

#include <array>
#include <memory>
#include <vector>

//==============================================================================
/**
    The Live Show on video: up to four cameras and the screen (the Controller's rows: each the
    main picture, a box of its own size and place over it, or hidden) made into one picture with
    the show's title, the track playing and a logo over it, streamed live with the show's sound to
    YouTube, Facebook, Twitch, TikTok or any RTMP server (several at once), and kept as an MP4 if
    wanted.

    Two pictures: the landscape one (YouTube, Facebook, Twitch) and the vertical one (9:16: TikTok,
    or any destination said to be vertical), each source in its own box in each, the main source
    filling the vertical picture from the part across the Controller chose (its focus). Each
    picture is made, and encoded, only while something wants it: a vertical destination streaming
    gets an encoder of its own, beside the landscape one.

    FFmpeg does the work (LGPL; its DLLs next to Sonify, which the build puts there): the cameras
    (DirectShow, MJPEG when they have it, so four share a USB port's room) and the screen (GDI)
    come in through it, the picture goes out as H.264 made by the graphics card when it can
    (NVIDIA, AMD, Intel), else by Windows' own encoder (Media Foundation), the sound as AAC, over
    RTMP or RTMPS.

    The sources are open while the picture is made (the Stream settings panel or the Controller
    shows, or the stream goes out): hidden ones too, so switching is a cut, at once. Each camera
    can hold its pictures a little (a quick camera waits for a slower one).

    The picture keeps the sound's time: a frame each 1/30 s of the show's sound that's gone out,
    so the two never drift apart, however long the night. Sync moves the sound against the
    picture (a camera shows each moment a little late: the sound waits for it).

    Each destination sends on a thread of its own: one that's slow, or drops, doesn't hold the
    others up. It tries again (from the next keyframe) for as long as the stream goes on.

    Message thread, but for what's said otherwise.
*/
namespace LiveStream
{
    /** "" when FFmpeg is there and loads; otherwise why the stream can't go. */
    juce::String problem();

    /** A camera or a screen: id is what Studio opens. */
    struct Device
    {
        juce::String id, name;
    };

    /** The cameras on this computer, then a test pattern (id "test"). Takes a moment. */
    std::vector<Device> cameras();

    /** The screens (Windows), each by where it is on the desktop. */
    std::vector<Device> screens();

    /** The ingest URL of a service ("YouTube", "Facebook", "Twitch") with its stream key; TikTok:
        its server URL (TikTok's LIVE Center gives it, with the key) and the key after it; any
        other: the key is the whole URL. */
    juce::String ingestUrl (const juce::String& service, const juce::String& key, const juce::String& server = {});

    /** Whether a service's picture is vertical (TikTok's is). */
    bool isVerticalService (const juce::String& service);

    //==============================================================================
    /** The picture's sources: the cameras, then the screen (the Controller's rows). */
    constexpr int maxCameras = 4;
    constexpr int numSources = maxCameras + 1;
    constexpr int screenSource = maxCameras;

    /** How a source shows: the whole picture (one at a time), a box of its own size and place
        over it (several may), or not at all. */
    enum class Show { hidden, main, custom };

    struct Source
    {
        juce::String id;                        // a camera's or a screen's (empty: none)
        Show show = Show::hidden;
        juce::Rectangle<float> box;             // its box in the picture (0..1 across and down)
        juce::Rectangle<float> tallBox;         // its box in the vertical picture (0..1 across and down)
        float tallFocus = 0.5f;                 // Main, in the vertical picture: the part across that shows (0 left .. 1 right)
        int delayMs = 0;                        // its pictures held this long (0 .. maxDelayMs)
    };

    constexpr int maxDelayMs = 300;

    /** How the picture looks. */
    struct Look
    {
        int width = 1280, height = 720, fps = 30;
        juce::String title;                     // over the top left
        bool showNowPlaying = true;             // the track playing, at the bottom left
        juce::Image logo;                       // at the top right
    };

    struct Destination
    {
        juce::String name;                      // "YouTube", "MP4 copy"...
        juce::String url;                       // rtmp:// or rtmps:// (its key in it), or a file to write
        bool vertical = false;                  // it gets the vertical picture (TikTok)
    };

    struct Settings
    {
        std::vector<Destination> destinations;
        int videoKbps = 4500, audioKbps = 160;
        int syncMs = 0;                         // over 0: the sound later than the picture
    };

    struct OutputStats
    {
        juce::String name;
        juce::String state;                     // "connecting", "live", "retrying", "failed", "ended"
        juce::String error;
        double kbps = 0.0;
        int skips = 0;                          // times it fell behind and went on from a keyframe
    };

    struct Stats
    {
        bool streaming = false;                 // started and not yet finished
        double seconds = 0.0;                   // of the show sent
        double fps = 0.0;                       // pictures made a second
        juce::String encoder;                   // the H.264 encoders at work ("h264_nvenc", "h264_nvenc x2"...)
        juce::String error;                     // why it stopped by itself
        juce::int64 soundDropped = 0;           // samples it couldn't keep up with
        std::vector<OutputStats> outputs;
    };

    //==============================================================================
    /** The picture's sources, how it's made, and the stream. */
    class Studio
    {
    public:
        Studio();
        ~Studio();

        /** The sources and how each shows (a source that won't open says why). A cut: the
            next picture made has them. */
        void setSources (const std::array<Source, numSources>& sources);
        juce::String getSourceProblem() const;              // the first one's that has one
        juce::String getSourceProblem (int source) const;

        /** A source's newest picture, as it comes (none before its first, or while it's closed).
            Any thread. */
        juce::Image getSourcePicture (int source) const;

        void setLook (const Look& look);
        void setNowPlaying (const juce::String& title);

        /** Streaming not enabled: the picture going out is the show's name and "Back in a
            moment" (the logo too), never the cameras or the screen. */
        void setHolding (bool holding);

        /** The picture is made, and the sources open, while it's previewed (the Stream settings
            panel or the Controller shows) or streamed. */
        void setPreviewing (bool previewing);

        /** The picture as it goes out (held: the show's name), as made last; none before the
            first. Any thread. */
        juce::Image getPicture() const;

        /** The picture as it's laid out, held or not: the Controller's. Any thread. */
        juce::Image getLayoutPicture() const;

        /** The vertical picture is made too while it's previewed (the Controller showing it, or the
            Stream settings panel with a vertical destination), as well as while a vertical
            destination streams. */
        void setVerticalPreview (bool previewing);

        /** The vertical picture as it goes out, and as it's laid out (none while it isn't made).
            Any thread. */
        juce::Image getTallPicture() const;
        juce::Image getTallLayoutPicture() const;

        /** The vertical picture's size for a look: the landscape one's turned on its side. */
        static juce::Point<int> tallSize (const Look& look);

        /** Goes live with tap's sound (it must outlive the stream). The encoders start, and each
            destination connects, on threads of their own: getStats() says how it goes. */
        juce::Result start (const Settings& settings, LiveShow::SoundTap& tap);

        /** Ends the stream: what's made goes out, then each destination is closed (a moment). */
        void stop();

        /** Once a stream has ended (stopped, or failed by itself): lets it go, and true once. The
            tap may go after that. */
        bool collect();

        bool isStreaming() const;
        Stats getStats() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;

        JUCE_DECLARE_NON_COPYABLE (Studio)
    };
}
