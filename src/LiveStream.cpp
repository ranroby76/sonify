// D:\workspace\Fanan_web_hub\src\LiveStream.cpp

#include "LiveStream.h"

#include <juce_gui_basics/juce_gui_basics.h>   // the screens (Desktop's displays)

#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>

#if FANAN_FFMPEG
extern "C"
{
 #include <libavcodec/avcodec.h>
 #include <libavdevice/avdevice.h>
 #include <libavformat/avformat.h>
 #include <libavutil/audio_fifo.h>
 #include <libavutil/channel_layout.h>
 #include <libavutil/error.h>
 #include <libavutil/opt.h>
 #include <libswresample/swresample.h>
 #include <libswscale/swscale.h>
}
#endif

#if FANAN_FFMPEG && JUCE_WINDOWS
 // COM for the encoder's thread (Windows' own H.264 encoder is a COM object).
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <objbase.h>
#endif

namespace LiveStream
{
namespace
{
    // The program's own background, and its accent (Sonify's).
    const juce::Colour background { 0xff0d0e11 };
    const juce::Colour accent { 0xffff8c42 };

    // A destination this far behind skips to the next keyframe.
    constexpr double mostBehindSeconds = 6.0;

    // The stream's sound: AAC at 48 kHz, stereo.
    constexpr int soundRate = 48000;

#if FANAN_FFMPEG
    juce::String errorText (int code)
    {
        char text[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror (code, text, sizeof (text));
        return juce::String (text);
    }

    void initialise()
    {
        static std::once_flag once;

        std::call_once (once, []
        {
            av_log_set_level (AV_LOG_ERROR);
            avdevice_register_all();
            avformat_network_init();
        });
    }

    /** Blocking FFmpeg calls (opening a camera, connecting, sending) give up when asked to. */
    struct Interrupt
    {
        std::function<bool()> stop;

        static int callback (void* opaque)
        {
            const auto* self = static_cast<Interrupt*> (opaque);
            return self->stop != nullptr && self->stop() ? 1 : 0;
        }
    };

    /** A picture's pixels into a frame (or the other way), its size and kind as they are. */
    void scale (SwsContext*& context, const juce::uint8* const* sourceData, const int* sourceStrides, int sourceWidth, int sourceHeight,
                AVPixelFormat sourceFormat, juce::uint8* const* targetData, const int* targetStrides, int targetWidth, int targetHeight,
                AVPixelFormat targetFormat)
    {
        context = sws_getCachedContext (context, sourceWidth, sourceHeight, sourceFormat, targetWidth, targetHeight, targetFormat,
                                        SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (context != nullptr)
            sws_scale (context, sourceData, sourceStrides, 0, sourceHeight, targetData, targetStrides);
    }
#endif

    /** A picture to draw into: one of a few kept (at most most), that nothing else holds now (or
        a new one). */
    juce::Image freeImage (std::vector<juce::Image>& pool, int width, int height, size_t most = 4)
    {
        for (auto& image : pool)
            if (image.isValid() && image.getWidth() == width && image.getHeight() == height && image.getReferenceCount() == 1)
                return image;

        juce::Image made (juce::Image::ARGB, width, height, false);

        if (pool.size() < most)
            pool.push_back (made);
        else
            pool[static_cast<size_t> (juce::Random::getSystemRandom().nextInt (static_cast<int> (pool.size())))] = made;

        return made;
    }

    /** A picture filling the whole frame (what's outside cut off): across, the part at focus (0 its
        left .. 1 its right); down, its middle. */
    void drawCover (juce::Graphics& g, const juce::Image& picture, int width, int height, float focus)
    {
        const auto pw = static_cast<float> (picture.getWidth()), ph = static_cast<float> (picture.getHeight());

        if (pw < 1.0f || ph < 1.0f)
            return;

        const auto scale = juce::jmax (static_cast<float> (width) / pw, static_cast<float> (height) / ph);
        const auto x = (static_cast<float> (width) - pw * scale) * juce::jlimit (0.0f, 1.0f, focus);
        const auto y = (static_cast<float> (height) - ph * scale) * 0.5f;
        g.drawImageTransformed (picture, juce::AffineTransform::scale (scale).translated (x, y));
    }

    /** A source as a box over the picture: its shadow, the picture filling it (cut to its shape),
        its edge. */
    void drawBox (juce::Graphics& g, const juce::Image& picture, juce::Rectangle<float> box, float unit)
    {
        const auto corner = juce::jmin (box.getWidth(), box.getHeight()) * 0.05f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (box.expanded (unit * 0.004f).translated (0.0f, unit * 0.005f), corner + 2.0f);

        {
            const juce::Graphics::ScopedSaveState kept (g);
            juce::Path shape;
            shape.addRoundedRectangle (box, corner);
            g.reduceClipRegion (shape);
            g.fillAll (juce::Colours::black);
            g.drawImage (picture, box, juce::RectanglePlacement (juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination));
        }

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawRoundedRectangle (box, corner, juce::jmax (1.5f, unit * 0.003f));
    }
}

//==============================================================================
juce::String problem()
{
#if FANAN_FFMPEG
   #if JUCE_WINDOWS
    // FFmpeg's DLLs are linked late (delay-loaded): each is looked for here before the first
    // call into it, so one that's missing is said, not a crash.
    static const juce::String why = []
    {
        static std::vector<std::unique_ptr<juce::DynamicLibrary>> kept;
        const auto folder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        const juce::StringArray names { "avutil-" + juce::String (LIBAVUTIL_VERSION_MAJOR), "swresample-" + juce::String (LIBSWRESAMPLE_VERSION_MAJOR),
                                        "swscale-" + juce::String (LIBSWSCALE_VERSION_MAJOR), "avcodec-" + juce::String (LIBAVCODEC_VERSION_MAJOR),
                                        "avformat-" + juce::String (LIBAVFORMAT_VERSION_MAJOR), "avdevice-" + juce::String (LIBAVDEVICE_VERSION_MAJOR) };

        for (const auto& name : names)
        {
            auto library = std::make_unique<juce::DynamicLibrary>();

            if (! library->open (folder.getChildFile (name + ".dll").getFullPathName()))
                return "Live streaming can't start: " + name + ".dll is missing next to Sonify.";

            kept.push_back (std::move (library));
        }

        return juce::String();
    }();

    return why;
   #else
    return {};
   #endif
#else
    return "Live streaming isn't available on this computer yet.";
#endif
}

std::vector<Device> cameras()
{
    std::vector<Device> found;

#if FANAN_FFMPEG
    if (problem().isEmpty())
    {
        initialise();

       #if JUCE_WINDOWS
        const auto* grabber = av_find_input_format ("dshow");
       #elif JUCE_MAC
        const auto* grabber = av_find_input_format ("avfoundation");
       #else
        const auto* grabber = av_find_input_format ("video4linux2");
       #endif

        AVDeviceInfoList* list = nullptr;

        if (grabber != nullptr && avdevice_list_input_sources (grabber, nullptr, nullptr, &list) >= 0 && list != nullptr)
        {
            for (int i = 0; i < list->nb_devices; ++i)
            {
                const auto* device = list->devices[i];
                bool video = device->nb_media_types == 0;

                for (int t = 0; t < device->nb_media_types; ++t)
                    video = video || device->media_types[t] == AVMEDIA_TYPE_VIDEO;

                if (video && device->device_name != nullptr)
                    found.push_back ({ juce::String::fromUTF8 (device->device_name),
                                       juce::String::fromUTF8 (device->device_description != nullptr ? device->device_description : device->device_name) });
            }
        }

        avdevice_free_list_devices (&list);
    }
#endif

    found.push_back ({ "test", "Test pattern" });
    return found;
}

std::vector<Device> screens()
{
    std::vector<Device> found;

   #if JUCE_WINDOWS
    // GDI's desktop is in physical pixels, from the main screen's top left.
    const auto& displays = juce::Desktop::getInstance().getDisplays().displays;

    for (int i = 0; i < displays.size(); ++i)
    {
        const auto& display = displays.getReference (i);
        const auto area = display.physicalBounds;
        const auto id = "screen:" + juce::String (area.getX()) + "," + juce::String (area.getY())
                      + "," + juce::String (area.getWidth()) + "," + juce::String (area.getHeight());
        found.push_back ({ id, "Screen " + juce::String (i + 1) + (display.isMain ? " (main)" : "")
                                   + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + juce::String (area.getWidth())
                                   + juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + juce::String (area.getHeight()) });
    }
   #endif

    return found;
}

juce::String ingestUrl (const juce::String& service, const juce::String& key, const juce::String& server)
{
    const auto trimmed = key.trim();

    if (service == "YouTube")   return "rtmp://a.rtmp.youtube.com/live2/" + trimmed;
    if (service == "Facebook")  return "rtmps://live-api-s.facebook.com:443/rtmp/" + trimmed;
    if (service == "Twitch")    return "rtmp://live.twitch.tv/app/" + trimmed;

    // TikTok's server changes from one account (and one live) to the next: its LIVE Center gives
    // it with the key. A key pasted whole (the server in it) goes as it is.
    if (service == "TikTok")
    {
        if (trimmed.startsWithIgnoreCase ("rtmp"))
            return trimmed;

        return server.trim().trimCharactersAtEnd ("/") + "/" + trimmed.trimCharactersAtStart ("/");
    }

    return trimmed;
}

bool isVerticalService (const juce::String& service)
{
    return service == "TikTok";
}

//==============================================================================
/** A camera or a screen, read on a thread of its own: its newest picture, as BGRA, and the ones
    just before it (for a camera held a little: setDelay). */
class Capture final : private juce::Thread
{
public:
    explicit Capture (juce::String sourceId)
        : juce::Thread ("Sonify picture source"), id (std::move (sourceId))
    {
        startThread();
    }

    ~Capture() override
    {
        signalThreadShouldExit();
        stopThread (6000);
    }

    const juce::String& getId() const noexcept   { return id; }

    juce::Image latest() const
    {
        const std::lock_guard<std::mutex> hold (lock);
        return recent.empty() ? juce::Image() : recent.back().second;
    }

    /** The picture as it was delayMs ago (the oldest kept, before then; the newest with no delay). */
    juce::Image delayed() const
    {
        const auto wait = delayMs.load (std::memory_order_relaxed);
        const std::lock_guard<std::mutex> hold (lock);

        if (recent.empty())
            return {};

        if (wait <= 0)
            return recent.back().second;

        const auto due = juce::Time::getMillisecondCounterHiRes() - wait;

        for (auto it = recent.rbegin(); it != recent.rend(); ++it)
            if (it->first <= due)
                return it->second;

        return recent.front().second;
    }

    void setDelay (int ms) noexcept   { delayMs.store (juce::jlimit (0, maxDelayMs, ms), std::memory_order_relaxed); }

    juce::String getProblem() const
    {
        const std::lock_guard<std::mutex> hold (lock);
        return problemText;
    }

private:
    void fail (const juce::String& why)
    {
        const std::lock_guard<std::mutex> hold (lock);
        problemText = why;
        recent.clear();
    }

    void run() override
    {
#if FANAN_FFMPEG
        if (const auto why = problem(); why.isNotEmpty())
        {
            fail (why);
            return;
        }

        initialise();

        struct Attempt
        {
            const char* format;
            juce::String url;
            std::vector<std::pair<const char*, juce::String>> options;
            AVCodecID codec = AV_CODEC_ID_NONE;         // the kind of pictures asked of a camera (none: its own choice)
        };

        std::vector<Attempt> attempts;
        const bool isScreen = id.startsWith ("screen:");

        if (id == "test")
        {
            attempts.push_back ({ "lavfi", "testsrc2=size=1280x720:rate=30,realtime", {} });
        }
        else if (isScreen)
        {
            const auto numbers = juce::StringArray::fromTokens (id.fromFirstOccurrenceOf (":", false, false), ",", "");

            if (numbers.size() == 4)
                attempts.push_back ({ "gdigrab", "desktop", { { "framerate", "30" }, { "draw_mouse", "1" },
                                                             { "offset_x", numbers[0] }, { "offset_y", numbers[1] },
                                                             { "video_size", numbers[2] + "x" + numbers[3] } } });
        }
        else
        {
           #if JUCE_WINDOWS
            const auto* grabber = "dshow";
            const auto url = "video=" + id;
           #elif JUCE_MAC
            const auto* grabber = "avfoundation";
            const auto url = id + ":none";
           #else
            const auto* grabber = "video4linux2";
            const auto url = id;
           #endif

            // HD at 30 if it can: as MJPEG first (four cameras share a USB port's room that way),
            // then as it likes; else whatever it gives.
           #if JUCE_WINDOWS
            attempts.push_back ({ grabber, url, { { "video_size", "1280x720" }, { "framerate", "30" }, { "rtbufsize", "64M" } }, AV_CODEC_ID_MJPEG });
           #endif
            attempts.push_back ({ grabber, url, { { "video_size", "1280x720" }, { "framerate", "30" }, { "rtbufsize", "64M" } } });
            attempts.push_back ({ grabber, url, { { "rtbufsize", "64M" } } });
        }

        Interrupt interrupt { [this] { return threadShouldExit(); } };
        AVFormatContext* input = nullptr;
        int opened = -1;

        for (const auto& attempt : attempts)
        {
            input = avformat_alloc_context();
            input->interrupt_callback = { &Interrupt::callback, &interrupt };
            input->video_codec_id = attempt.codec;
            AVDictionary* options = nullptr;

            for (const auto& [name, value] : attempt.options)
                av_dict_set (&options, name, value.toRawUTF8(), 0);

            opened = avformat_open_input (&input, attempt.url.toRawUTF8(), av_find_input_format (attempt.format), &options);
            av_dict_free (&options);

            if (opened >= 0 || threadShouldExit())
                break;

            input = nullptr;   // freed by the failed open
        }

        if (opened < 0)
        {
            if (! threadShouldExit())
                fail (isScreen ? "Sonify can't see that screen (" + errorText (opened) + ")."
                               : "Sonify can't open that camera: is another app using it? (" + errorText (opened) + ")");
            return;
        }

        const AVCodec* codec = nullptr;
        AVCodecContext* decoder = nullptr;
        AVPacket* packet = av_packet_alloc();
        AVFrame* picture = av_frame_alloc();
        SwsContext* converter = nullptr;
        int stream = -1;

        if (avformat_find_stream_info (input, nullptr) >= 0)
            stream = av_find_best_stream (input, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

        if (stream >= 0 && codec != nullptr)
        {
            decoder = avcodec_alloc_context3 (codec);
            avcodec_parameters_to_context (decoder, input->streams[stream]->codecpar);
            decoder->thread_count = 2;

            if (avcodec_open2 (decoder, codec, nullptr) < 0)
                avcodec_free_context (&decoder);
        }

        if (decoder == nullptr)
            fail ("Sonify can't read the pictures this source sends.");

        while (decoder != nullptr && ! threadShouldExit())
        {
            const auto read = av_read_frame (input, packet);

            if (read == AVERROR (EAGAIN))
            {
                wait (2);
                continue;
            }

            if (read < 0)
            {
                if (! threadShouldExit())
                    fail (isScreen ? "The screen stopped sending pictures." : "The camera stopped sending pictures (unplugged?).");
                break;
            }

            if (packet->stream_index == stream && avcodec_send_packet (decoder, packet) >= 0)
                while (avcodec_receive_frame (decoder, picture) >= 0)
                    keep (*picture, converter);

            av_packet_unref (packet);
        }

        sws_freeContext (converter);
        av_frame_free (&picture);
        av_packet_free (&packet);
        avcodec_free_context (&decoder);
        avformat_close_input (&input);
#else
        fail (problem());
#endif
    }

#if FANAN_FFMPEG
    /** A picture as BGRA, at most HD's size (its shape kept). */
    void keep (const AVFrame& picture, SwsContext*& converter)
    {
        if (picture.width <= 0 || picture.height <= 0)
            return;

        const auto shrink = juce::jmin (1.0, juce::jmin (1920.0 / picture.width, 1080.0 / picture.height));
        const auto width = juce::jmax (2, juce::roundToInt (picture.width * shrink));
        const auto height = juce::jmax (2, juce::roundToInt (picture.height * shrink));

        // Enough pictures kept for the delay asked (at 60 a second), and a few to draw into.
        const auto heldFor = delayMs.load (std::memory_order_relaxed);
        const auto keep = static_cast<size_t> (heldFor * 60 / 1000 + 2);
        auto image = freeImage (pool, width, height, keep + 3);

        {
            juce::Image::BitmapData pixels (image, juce::Image::BitmapData::writeOnly);
            juce::uint8* const target[] = { pixels.data, nullptr, nullptr, nullptr };
            const int strides[] = { pixels.lineStride, 0, 0, 0 };
            scale (converter, picture.data, picture.linesize, picture.width, picture.height, static_cast<AVPixelFormat> (picture.format),
                   target, strides, width, height, AV_PIX_FMT_BGRA);
        }

        const auto now = juce::Time::getMillisecondCounterHiRes();
        const std::lock_guard<std::mutex> hold (lock);
        recent.emplace_back (now, image);

        // The ones older than the delay needs go (the newest past it stays: it's the one shown).
        while (recent.size() > 1 && (recent.size() > keep || recent[1].first <= now - heldFor - 50.0))
            recent.pop_front();

        problemText.clear();
    }
#endif

    juce::String id;
    mutable std::mutex lock;
    std::deque<std::pair<double, juce::Image>> recent;   // when each came, oldest first
    std::atomic<int> delayMs { 0 };
    juce::String problemText;
    std::vector<juce::Image> pool;
};

#if FANAN_FFMPEG
//==============================================================================
/** A destination: what's encoded goes out to it on a thread of its own (connecting, and again
    when it drops, from the next keyframe). */
class Output final : private juce::Thread
{
public:
    Output (Destination where, std::vector<const AVCodecContext*> encoders)
        : juce::Thread ("Sonify stream output"), destination (std::move (where))
    {
        isFile = ! destination.url.startsWithIgnoreCase ("rtmp");

        for (const auto* encoder : encoders)
        {
            auto* parameters = avcodec_parameters_alloc();
            avcodec_parameters_from_context (parameters, encoder);
            streams.push_back ({ parameters, encoder->time_base, encoder->codec_type == AVMEDIA_TYPE_VIDEO });
        }

        stats.name = destination.name;
        stats.state = "connecting";
        startThread();
    }

    ~Output() override
    {
        signalThreadShouldExit();
        wake.notify_all();
        stopThread (4000);

        for (auto* packet : queue)
            av_packet_free (&packet);

        for (auto& stream : streams)
            avcodec_parameters_free (&stream.parameters);
    }

    /** A packet to send (copied: the caller keeps its own). */
    void push (const AVPacket& packet)
    {
        auto* copy = av_packet_clone (&packet);

        if (copy == nullptr)
            return;

        const std::lock_guard<std::mutex> hold (lock);

        // Fallen too far behind (a slow line): what's waiting goes, and it goes on from the next keyframe.
        if (! queue.empty())
        {
            const auto& first = *queue.front();
            const auto behind = seconds (packet) - seconds (first);

            if (behind > mostBehindSeconds)
            {
                for (auto* waiting : queue)
                    av_packet_free (&waiting);

                queue.clear();
                needKey = true;
                ++stats.skips;
            }
        }

        queue.push_back (copy);
        wake.notify_one();
    }

    /** No more packets: what's queued goes, then the destination is closed. */
    void finish()
    {
        const std::lock_guard<std::mutex> hold (lock);
        finishing = true;
        finishAsked = juce::Time::getMillisecondCounterHiRes();
        wake.notify_all();
    }

    bool isDone() const noexcept      { return ! isThreadRunning(); }

    /** Gives up on what's left (a destination that won't take it). */
    void abandon()
    {
        signalThreadShouldExit();
        wake.notify_all();
    }

    OutputStats getStats() const
    {
        const std::lock_guard<std::mutex> hold (lock);
        auto copy = stats;
        const auto now = juce::Time::getMillisecondCounterHiRes();

        if (now - rateFrom > 0.0 && stats.state == "live")
            copy.kbps = static_cast<double> (bytesSince) * 8.0 / (now - rateFrom);

        return copy;
    }

private:
    struct Stream
    {
        AVCodecParameters* parameters = nullptr;
        AVRational timeBase;
        bool video = false;
    };

    double seconds (const AVPacket& packet) const
    {
        const auto& stream = streams[static_cast<size_t> (packet.stream_index)];
        return static_cast<double> (packet.dts == AV_NOPTS_VALUE ? packet.pts : packet.dts) * av_q2d (stream.timeBase);
    }

    void setState (const juce::String& state, const juce::String& error = {})
    {
        const std::lock_guard<std::mutex> hold (lock);
        stats.state = state;
        stats.error = error;
    }

    bool connect()
    {
        const auto url = destination.url.toRawUTF8();

        if (avformat_alloc_output_context2 (&context, nullptr, isFile ? "mp4" : "flv", url) < 0 || context == nullptr)
            return false;

        context->interrupt_callback = { &Interrupt::callback, &interrupt };

        for (const auto& stream : streams)
        {
            auto* made = avformat_new_stream (context, nullptr);
            avcodec_parameters_copy (made->codecpar, stream.parameters);
            made->codecpar->codec_tag = 0;
            made->time_base = stream.timeBase;
        }

        int result = 0;

        if ((context->oformat->flags & AVFMT_NOFILE) == 0)
        {
            AVDictionary* io = nullptr;
            av_dict_set (&io, "rw_timeout", "15000000", 0);   // 15 s without a byte through: dropped
            result = avio_open2 (&context->pb, url, AVIO_FLAG_WRITE, &context->interrupt_callback, &io);
            av_dict_free (&io);
        }

        if (result >= 0)
        {
            AVDictionary* options = nullptr;

            if (isFile)
                av_dict_set (&options, "movflags", "+frag_keyframe+empty_moov+default_base_moof", 0);   // playable even if Sonify stops
            else
                av_dict_set (&options, "flvflags", "no_duration_filesize", 0);

            result = avformat_write_header (context, &options);
            av_dict_free (&options);
        }

        if (result < 0)
        {
            lastError = errorText (result);
            close (false);
            return false;
        }

        return true;
    }

    void close (bool wellEnded)
    {
        if (context == nullptr)
            return;

        if (wellEnded)
        {
            av_interleaved_write_frame (context, nullptr);
            av_write_trailer (context);
        }

        if ((context->oformat->flags & AVFMT_NOFILE) == 0)
            avio_closep (&context->pb);

        avformat_free_context (context);
        context = nullptr;
    }

    AVPacket* next()
    {
        std::unique_lock<std::mutex> hold (lock);
        wake.wait_for (hold, std::chrono::milliseconds (100), [this] { return ! queue.empty() || finishing || threadShouldExit(); });

        if (queue.empty())
            return nullptr;

        auto* packet = queue.front();
        queue.pop_front();
        return packet;
    }

    bool drained()
    {
        const std::lock_guard<std::mutex> hold (lock);
        return finishing && queue.empty();
    }

    bool tooLongFinishing()
    {
        const std::lock_guard<std::mutex> hold (lock);
        return finishing && juce::Time::getMillisecondCounterHiRes() - finishAsked > 8000.0;
    }

    void run() override
    {
        bool wentLive = false, turnedAway = false;   // connected at least once; failed to, at least once
        juce::String whyNot;                         // why it last didn't (not its being given up on)

        while (! threadShouldExit() && ! drained())
        {
            setState ("connecting");

            if (! connect())
            {
                if (isFile)
                {
                    setState ("failed", "Sonify can't write " + destination.url + " (" + lastError + ").");
                    return;
                }

                if (threadShouldExit())
                    break;   // given up on while connecting: said below

                turnedAway = true;
                whyNot = lastError;
                setState ("retrying", "It doesn't answer (" + lastError + "): trying again.");

                // Again in a few seconds (what comes meanwhile waits, or goes when there's too much).
                for (int i = 0; i < 30 && ! threadShouldExit() && ! drained(); ++i)
                    wait (100);

                continue;
            }

            wentLive = true;

            {
                const std::lock_guard<std::mutex> hold (lock);
                stats.state = "live";
                stats.error.clear();
                needKey = true;
                bytesSince = 0;
                rateFrom = juce::Time::getMillisecondCounterHiRes();
            }

            // Each stream's: what's taken off its times, where it went on from (the keyframe's
            // time), and how far its time has got on this connection (where the next may start).
            std::vector<juce::int64> base (streams.size(), 0), from (streams.size(), 0), reached (streams.size(), 0);
            bool sent = true;

            while (! threadShouldExit())
            {
                if (tooLongFinishing())
                    break;

                auto* packet = next();

                if (packet == nullptr)
                {
                    if (drained())
                        break;

                    continue;
                }

                const auto& stream = streams[static_cast<size_t> (packet->stream_index)];
                bool keep = true;

                {
                    const std::lock_guard<std::mutex> hold (lock);

                    // From a keyframe: a new connection's time starts there, at 0; after a skip
                    // it goes on from where it had got to (the time skipped isn't in it), never
                    // back (the muxer won't take a time earlier than the last).
                    if (needKey)
                    {
                        if (stream.video && (packet->flags & AV_PKT_FLAG_KEY) != 0)
                        {
                            needKey = false;
                            const auto key = seconds (*packet);
                            double resume = 0.0;

                            for (size_t s = 0; s < streams.size(); ++s)
                                resume = juce::jmax (resume, static_cast<double> (reached[s]) * av_q2d (streams[s].timeBase));

                            for (size_t s = 0; s < streams.size(); ++s)
                            {
                                const auto unit = av_q2d (streams[s].timeBase);
                                from[s] = static_cast<juce::int64> (std::llround (key / unit));
                                base[s] = from[s] - static_cast<juce::int64> (std::ceil (resume / unit));
                            }
                        }
                        else
                        {
                            keep = false;
                        }
                    }
                }

                const auto index = static_cast<size_t> (packet->stream_index);
                const auto at = base[index];

                if (keep && packet->dts != AV_NOPTS_VALUE && packet->dts < from[index])
                    keep = false;   // before the keyframe it went on from

                if (! keep)
                {
                    av_packet_free (&packet);
                    continue;
                }

                if (packet->pts != AV_NOPTS_VALUE) packet->pts -= at;
                if (packet->dts != AV_NOPTS_VALUE) packet->dts -= at;

                if (const auto stamp = packet->dts != AV_NOPTS_VALUE ? packet->dts : packet->pts; stamp != AV_NOPTS_VALUE)
                    reached[index] = juce::jmax (reached[index], stamp + juce::jmax<juce::int64> (1, packet->duration));

                av_packet_rescale_ts (packet, stream.timeBase, context->streams[packet->stream_index]->time_base);
                const auto size = packet->size;
                const auto written = av_interleaved_write_frame (context, packet);
                av_packet_free (&packet);

                if (written < 0)
                {
                    lastError = errorText (written);
                    sent = false;
                    break;
                }

                const std::lock_guard<std::mutex> hold (lock);
                bytesSince += size;
                const auto now = juce::Time::getMillisecondCounterHiRes();

                // The rate over the last few seconds.
                if (now - rateFrom > 4000.0)
                {
                    stats.kbps = static_cast<double> (bytesSince) * 8.0 / (now - rateFrom);
                    bytesSince = 0;
                    rateFrom = now;
                }
            }

            if (sent)
            {
                close (true);
                setState ("ended");
                return;
            }

            close (false);

            if (isFile)
            {
                setState ("failed", "Writing " + destination.url + " failed (" + lastError + ").");
                return;
            }

            setState ("retrying", "The connection dropped (" + lastError + "): trying again.");

            for (int i = 0; i < 20 && ! threadShouldExit() && ! drained(); ++i)
                wait (100);
        }

        const std::lock_guard<std::mutex> hold (lock);

        if (stats.state == "ended" || stats.state == "failed")
            return;

        if (wentLive || ! turnedAway)
        {
            stats.state = "ended";
        }
        else
        {
            // It never connected, however long it tried: that's a failure, and why.
            stats.state = "failed";
            stats.error = whyNot.isNotEmpty() ? "It never answered (" + whyNot + ")." : juce::String ("It never answered.");
        }
    }

    Destination destination;
    bool isFile = false;
    std::vector<Stream> streams;
    AVFormatContext* context = nullptr;
    Interrupt interrupt { [this] { return threadShouldExit(); } };
    juce::String lastError;

    mutable std::mutex lock;
    std::condition_variable wake;
    std::deque<AVPacket*> queue;
    bool finishing = false, needKey = true;
    double finishAsked = 0.0;
    OutputStats stats;
    juce::int64 bytesSince = 0;
    double rateFrom = 0.0;
};

//==============================================================================
/** The encoders, fed by the show's sound (its clock) and the pictures: the landscape picture's
    encoder for the destinations that take it, the vertical picture's for the vertical ones (each
    only when a destination wants it), the sound's for all; each packet to the outputs it's for. */
class Streamer final : private juce::Thread
{
public:
    /** wide, tall: the landscape picture and the vertical one, as made now; their sizes. */
    Streamer (Settings s, LiveShow::SoundTap& t, std::function<juce::Image()> wide, std::function<juce::Image()> tall,
              juce::Point<int> wideSize, juce::Point<int> tallSize, int f)
        : juce::Thread ("Sonify live stream"), settings (std::move (s)), tap (t), fps (juce::jlimit (10, 60, f))
    {
        for (const auto& destination : settings.destinations)
            (destination.vertical ? tallWanted : wideWanted) = true;

        if (wideWanted)
            lanes.push_back (makeLane (false, wideSize, std::move (wide)));

        if (tallWanted)
            lanes.push_back (makeLane (true, tallSize, std::move (tall)));

        startThread (juce::Thread::Priority::high);
    }

    ~Streamer() override
    {
        signalThreadShouldExit();
        stopThread (20000);
    }

    /** Ends it: the encoders give what they hold, then the outputs close. */
    void end()   { ending = true; }

    bool isDone() const noexcept   { return ! isThreadRunning(); }

    /** Which pictures it encodes (fixed when it's made). Any thread. */
    bool wantsWide() const noexcept   { return wideWanted; }
    bool wantsTall() const noexcept   { return tallWanted; }

    Stats getStats() const
    {
        Stats copy;

        {
            const std::lock_guard<std::mutex> hold (lock);
            copy.encoder = encoderNames;
            copy.error = error;
        }

        copy.streaming = isThreadRunning();
        copy.seconds = static_cast<double> (soundOut.load()) / soundRate;
        copy.soundDropped = tap.getDropped();

        const std::lock_guard<std::mutex> hold (outputsLock);

        for (const auto& output : outputs)
            copy.outputs.push_back (output->getStats());

        return copy;
    }

private:
    /** A picture's way out: its size, where it comes from, its encoder and frame. */
    struct Lane
    {
        bool tall = false;
        int width = 0, height = 0;
        std::function<juce::Image()> picture;
        AVCodecContext* encoder = nullptr;
        AVFrame* frame = nullptr;
        SwsContext* converter = nullptr;
        juce::Image black;
        juce::String encoderName;
    };

    static Lane makeLane (bool tall, juce::Point<int> size, std::function<juce::Image()> picture)
    {
        Lane lane;
        lane.tall = tall;
        lane.width = juce::jmax (16, size.x) & ~1;
        lane.height = juce::jmax (16, size.y) & ~1;
        lane.picture = std::move (picture);
        return lane;
    }

    void setError (const juce::String& why)
    {
        const std::lock_guard<std::mutex> hold (lock);
        error = why;
    }

    bool openVideo (Lane& lane)
    {
        struct Candidate
        {
            const char* name;
            AVPixelFormat format;
            std::vector<std::pair<const char*, const char*>> options;
        };

        // The graphics card's own first (NVIDIA, AMD, Intel), then Windows' (Media Foundation),
        // then software: OpenH264 (and x264, where FFmpeg has it). The vertical picture's encoder
        // is a second one: where the card won't give another, the next that starts.
        const std::vector<Candidate> candidates {
            { "h264_nvenc",  AV_PIX_FMT_NV12,    { { "preset", "p4" }, { "tune", "ll" }, { "rc", "cbr" } } },
            { "h264_amf",    AV_PIX_FMT_NV12,    { { "usage", "lowlatency" }, { "rc", "cbr" } } },
            { "h264_qsv",    AV_PIX_FMT_NV12,    { { "preset", "veryfast" } } },
            { "h264_mf",     AV_PIX_FMT_NV12,    { { "rate_control", "cbr" }, { "scenario", "live_streaming" }, { "hw_encoding", "1" } } },
            { "h264_mf",     AV_PIX_FMT_NV12,    { { "rate_control", "cbr" }, { "scenario", "live_streaming" } } },
            { "libopenh264", AV_PIX_FMT_YUV420P, { { "rc_mode", "bitrate" }, { "allow_skip_frames", "0" } } },
            { "libx264",     AV_PIX_FMT_YUV420P, { { "preset", "veryfast" }, { "tune", "zerolatency" }, { "nal-hrd", "cbr" } } },
        };

        for (const auto& candidate : candidates)
        {
            const auto* codec = avcodec_find_encoder_by_name (candidate.name);

            if (codec == nullptr)
                continue;

            auto* context = avcodec_alloc_context3 (codec);
            context->width = lane.width;
            context->height = lane.height;
            context->time_base = { 1, fps };
            context->framerate = { fps, 1 };
            context->pix_fmt = candidate.format;
            context->bit_rate = static_cast<juce::int64> (settings.videoKbps) * 1000;
            context->rc_max_rate = context->bit_rate;
            context->rc_buffer_size = static_cast<int> (context->bit_rate * 2);
            context->gop_size = fps * 2;            // a keyframe every 2 s, as YouTube, Facebook and TikTok ask
            context->max_b_frames = 0;
            context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            AVDictionary* options = nullptr;

            for (const auto& [name, value] : candidate.options)
                av_dict_set (&options, name, value, 0);

            const auto opened = avcodec_open2 (context, codec, &options);
            av_dict_free (&options);

            if (opened >= 0)
            {
                lane.encoder = context;
                lane.encoderName = candidate.name;
                return true;
            }

            avcodec_free_context (&context);
        }

        return false;
    }

    bool openSound()
    {
        const auto* codec = avcodec_find_encoder (AV_CODEC_ID_AAC);

        if (codec == nullptr)
            return false;

        sound = avcodec_alloc_context3 (codec);
        sound->sample_fmt = AV_SAMPLE_FMT_FLTP;
        sound->sample_rate = soundRate;
        av_channel_layout_default (&sound->ch_layout, 2);
        sound->bit_rate = static_cast<juce::int64> (settings.audioKbps) * 1000;
        sound->time_base = { 1, soundRate };
        sound->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2 (sound, codec, nullptr) < 0)
            return false;

        AVChannelLayout stereo;
        av_channel_layout_default (&stereo, 2);

        if (swr_alloc_set_opts2 (&resampler, &sound->ch_layout, AV_SAMPLE_FMT_FLTP, soundRate,
                                 &stereo, AV_SAMPLE_FMT_FLTP, juce::roundToInt (tap.getSampleRate()), 0, nullptr) < 0
            || swr_init (resampler) < 0)
            return false;

        waiting = av_audio_fifo_alloc (AV_SAMPLE_FMT_FLTP, 2, soundRate);
        soundFrame = av_frame_alloc();
        soundFrame->nb_samples = sound->frame_size > 0 ? sound->frame_size : 1024;
        soundFrame->format = AV_SAMPLE_FMT_FLTP;
        soundFrame->sample_rate = soundRate;
        av_channel_layout_copy (&soundFrame->ch_layout, &sound->ch_layout);
        return av_frame_get_buffer (soundFrame, 0) >= 0 && waiting != nullptr;
    }

    /** A frame through its encoder, and what comes out to the outputs it's for (lane: those of
        that picture; -1: every one, the sound). Frame null: the rest it holds. */
    void encode (AVCodecContext* encoder, AVFrame* frame, int stream, int lane)
    {
        if (avcodec_send_frame (encoder, frame) < 0)
            return;

        while (avcodec_receive_packet (encoder, packet) >= 0)
        {
            packet->stream_index = stream;

            {
                const std::lock_guard<std::mutex> hold (outputsLock);

                for (size_t i = 0; i < outputs.size(); ++i)
                    if (lane < 0 || outputLanes[i] == lane)
                        outputs[i]->push (*packet);
            }

            av_packet_unref (packet);
        }
    }

    /** A lane's picture now, into its encoder's frame (black before the first). */
    void makePicture (Lane& lane)
    {
        if (av_frame_make_writable (lane.frame) < 0)
            return;

        auto image = lane.picture();

        if (! image.isValid())
        {
            if (! lane.black.isValid())
            {
                lane.black = juce::Image (juce::Image::ARGB, lane.width, lane.height, false);
                lane.black.clear (lane.black.getBounds(), background);
            }

            image = lane.black;
        }

        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);
        const juce::uint8* const source[] = { pixels.data, nullptr, nullptr, nullptr };
        const int strides[] = { pixels.lineStride, 0, 0, 0 };
        scale (lane.converter, source, strides, image.getWidth(), image.getHeight(), AV_PIX_FMT_BGRA,
               lane.frame->data, lane.frame->linesize, lane.width, lane.height, lane.encoder->pix_fmt);
    }

    /** n samples of the show's sound (null: silence) into the encoder; pictures as they fall due. */
    void feed (const float* left, const float* right, int n)
    {
        std::vector<float> silence;

        if (left == nullptr)
        {
            silence.assign (static_cast<size_t> (n), 0.0f);
            left = right = silence.data();
        }

        const auto room = swr_get_out_samples (resampler, n) + 32;

        if (static_cast<int> (convertedLeft.size()) < room)
        {
            convertedLeft.resize (static_cast<size_t> (room));
            convertedRight.resize (static_cast<size_t> (room));
        }

        const juce::uint8* in[] = { reinterpret_cast<const juce::uint8*> (left), reinterpret_cast<const juce::uint8*> (right) };
        juce::uint8* out[] = { reinterpret_cast<juce::uint8*> (convertedLeft.data()), reinterpret_cast<juce::uint8*> (convertedRight.data()) };
        const auto made = swr_convert (resampler, out, room, in, n);

        if (made > 0)
            av_audio_fifo_write (waiting, reinterpret_cast<void**> (out), made);

        const auto frameSize = soundFrame->nb_samples;
        const auto shift = static_cast<juce::int64> (settings.syncMs) * soundRate / 1000;

        while (av_audio_fifo_size (waiting) >= frameSize)
        {
            if (av_frame_make_writable (soundFrame) < 0)
                break;

            av_audio_fifo_read (waiting, reinterpret_cast<void**> (soundFrame->data), frameSize);
            soundFrame->pts = soundOut.load() + shift;
            soundOut += frameSize;
            encode (sound, soundFrame, 1, -1);

            // The pictures due by now, by the sound's clock: each lane's, at the same moment.
            while (pictureCount * soundRate <= soundOut.load() * fps)
            {
                for (size_t l = 0; l < lanes.size(); ++l)
                {
                    makePicture (lanes[l]);
                    lanes[l].frame->pts = pictureCount;
                    encode (lanes[l].encoder, lanes[l].frame, 0, static_cast<int> (l));
                }

                ++pictureCount;
            }
        }
    }

    void run() override
    {
       #if JUCE_WINDOWS
        const auto com = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
       #endif

        stream();
        release();

       #if JUCE_WINDOWS
        if (SUCCEEDED (com))
            CoUninitialize();
       #endif
    }

    void stream()
    {
        initialise();
        packet = av_packet_alloc();

        for (auto& lane : lanes)
        {
            if (! openVideo (lane))
            {
                setError (lane.tall && lanes.size() > 1 ? "A second H.264 encoder, for the vertical picture, wouldn't start on this computer."
                                                        : "No H.264 encoder would start on this computer.");
                return;
            }

            lane.frame = av_frame_alloc();
            lane.frame->format = lane.encoder->pix_fmt;
            lane.frame->width = lane.width;
            lane.frame->height = lane.height;

            if (av_frame_get_buffer (lane.frame, 0) < 0)
            {
                setError ("There isn't memory enough for the stream's pictures.");
                return;
            }
        }

        if (! openSound())
        {
            setError ("The sound's encoder (AAC) wouldn't start.");
            return;
        }

        {
            // "h264_nvenc", or both: "h264_nvenc x2", "h264_nvenc + h264_mf".
            juce::StringArray names;

            for (const auto& lane : lanes)
                names.add (lane.encoderName);

            const std::lock_guard<std::mutex> hold (lock);
            encoderNames = names.size() == 2 && names[0] == names[1] ? names[0] + " x2" : names.joinIntoString (" + ");
        }

        {
            const std::lock_guard<std::mutex> hold (outputsLock);

            for (const auto& destination : settings.destinations)
            {
                for (size_t l = 0; l < lanes.size(); ++l)
                {
                    if (lanes[l].tall != destination.vertical)
                        continue;

                    outputs.push_back (std::make_unique<Output> (destination, std::vector<const AVCodecContext*> { lanes[l].encoder, sound }));
                    outputLanes.push_back (static_cast<int> (l));
                    break;
                }
            }
        }

        // From now on: what's come in before is old news.
        std::vector<float> left (4096), right (4096);

        while (tap.pull (left.data(), right.data(), static_cast<int> (left.size())) > 0) {}

        auto lastSound = juce::Time::getMillisecondCounterHiRes();
        const auto tapRate = tap.getSampleRate();

        while (! threadShouldExit() && ! ending)
        {
            const auto got = tap.pull (left.data(), right.data(), static_cast<int> (left.size()));
            const auto now = juce::Time::getMillisecondCounterHiRes();

            if (got > 0)
            {
                lastSound = now;
                feed (left.data(), right.data(), got);
                continue;
            }

            // The show's sound has stopped coming (a device hiccup): silence meanwhile, so the
            // stream goes on.
            if (now - lastSound > 500.0)
            {
                const auto missing = static_cast<int> ((now - lastSound) * 0.001 * tapRate);
                lastSound = now;

                for (int done = 0; done < missing; done += 4096)
                    feed (nullptr, nullptr, juce::jmin (4096, missing - done));

                continue;
            }

            wait (3);
        }

        // What the encoders hold, then each output closes (a destination that won't take it is
        // given up on).
        for (size_t l = 0; l < lanes.size(); ++l)
            encode (lanes[l].encoder, nullptr, 0, static_cast<int> (l));

        encode (sound, nullptr, 1, -1);

        {
            const std::lock_guard<std::mutex> hold (outputsLock);

            for (const auto& output : outputs)
                output->finish();
        }

        const auto started = juce::Time::getMillisecondCounterHiRes();

        for (;;)
        {
            bool done = true;

            {
                const std::lock_guard<std::mutex> hold (outputsLock);

                for (const auto& output : outputs)
                    done = done && output->isDone();
            }

            if (done || juce::Time::getMillisecondCounterHiRes() - started > 10000.0)
                break;

            wait (20);
        }

        const std::lock_guard<std::mutex> hold (outputsLock);

        for (const auto& output : outputs)
            output->abandon();

        // Their last word stays for the page; the outputs themselves go with the streamer.
    }

    void release()
    {
        for (auto& lane : lanes)
        {
            sws_freeContext (lane.converter);
            lane.converter = nullptr;
            av_frame_free (&lane.frame);
            avcodec_free_context (&lane.encoder);
        }

        swr_free (&resampler);

        if (waiting != nullptr)
            av_audio_fifo_free (waiting);

        waiting = nullptr;
        av_frame_free (&soundFrame);
        av_packet_free (&packet);
        avcodec_free_context (&sound);
    }

    Settings settings;
    LiveShow::SoundTap& tap;
    const int fps;
    bool wideWanted = false, tallWanted = false;   // set before the thread starts, then fixed
    std::vector<Lane> lanes;                       // made before the thread starts, never resized
    std::atomic<bool> ending { false };

    AVCodecContext* sound = nullptr;
    AVFrame* soundFrame = nullptr;
    AVPacket* packet = nullptr;
    SwrContext* resampler = nullptr;
    AVAudioFifo* waiting = nullptr;
    std::vector<float> convertedLeft, convertedRight;

    std::atomic<juce::int64> soundOut { 0 };
    juce::int64 pictureCount = 0;

    mutable std::mutex lock;
    juce::String encoderNames, error;

    mutable std::mutex outputsLock;
    std::vector<std::unique_ptr<Output>> outputs;
    std::vector<int> outputLanes;                  // each output's lane
};
#endif

//==============================================================================
struct Studio::Impl final : private juce::Thread
{
    Impl()
        : juce::Thread ("Sonify live picture")
    {
        startThread();
    }

    ~Impl() override
    {
       #if FANAN_FFMPEG
        // A stream under way ends first, while all it asks for the pictures is still here: let go
        // under the lock (the picture's thread looks at it), its thread waited for outside it
        // (that thread takes the lock for each picture).
        std::unique_ptr<Streamer> gone;

        {
            const std::lock_guard<std::mutex> hold (lock);
            gone = std::move (streamer);
        }

        gone = nullptr;
       #endif
        signalThreadShouldExit();
        stopThread (4000);

        for (auto& capture : captures)
            capture = nullptr;
    }

    bool composingLocked() const
    {
       #if FANAN_FFMPEG
        return previewing || streamer != nullptr;
       #else
        return previewing;
       #endif
    }

    bool composing() const
    {
        const std::lock_guard<std::mutex> hold (lock);
        return composingLocked();
    }

    /** Each source opened or let go as it's wanted now: open while the picture is made (hidden
        ones too: a cut is at once), another camera taking a row's place. A source going stops
        here, not under the lock. */
    void applyCaptures()
    {
        std::array<std::unique_ptr<Capture>, numSources> gone;
        std::array<juce::String, numSources> wanted;

        {
            const std::lock_guard<std::mutex> hold (lock);
            const auto open = composingLocked();

            for (size_t i = 0; i < captures.size(); ++i)
            {
                wanted[i] = open ? sources[i].id : juce::String();

                if (captures[i] != nullptr && captures[i]->getId() != wanted[i])
                    gone[i] = std::move (captures[i]);
            }
        }

        for (auto& capture : gone)
            capture = nullptr;

        for (size_t i = 0; i < captures.size(); ++i)
        {
            std::unique_ptr<Capture> made;

            {
                const std::lock_guard<std::mutex> hold (lock);

                if (captures[i] != nullptr || wanted[i].isEmpty())
                {
                    if (captures[i] != nullptr)
                        captures[i]->setDelay (sources[i].delayMs);

                    continue;
                }
            }

            made = std::make_unique<Capture> (wanted[i]);

            const std::lock_guard<std::mutex> hold (lock);
            made->setDelay (sources[i].delayMs);
            captures[i] = std::move (made);
        }
    }

    juce::Image getPicture() const
    {
        const std::lock_guard<std::mutex> hold (lock);
        return picture;
    }

    juce::Image getLayoutPicture() const
    {
        const std::lock_guard<std::mutex> hold (lock);
        return layoutPicture;
    }

    juce::Image getTallPicture() const
    {
        const std::lock_guard<std::mutex> hold (lock);
        return tallPicture;
    }

    juce::Image getTallLayoutPicture() const
    {
        const std::lock_guard<std::mutex> hold (lock);
        return tallLayoutPicture;
    }

    void run() override
    {
        auto next = juce::Time::getMillisecondCounterHiRes();
        int made = 0;
        auto counted = next;

        while (! threadShouldExit())
        {
            if (! composing())
            {
                wait (100);
                next = juce::Time::getMillisecondCounterHiRes();
                continue;
            }

            compose();
            ++made;

            const auto now = juce::Time::getMillisecondCounterHiRes();

            if (now - counted >= 2000.0)
            {
                madePerSecond = made * 1000.0 / (now - counted);
                made = 0;
                counted = now;
            }

            int rate = 30;

            {
                const std::lock_guard<std::mutex> hold (lock);
                rate = juce::jlimit (10, 60, look.fps);
            }

            next += 1000.0 / rate;

            if (next < now)
                next = now;   // fallen behind: on from now

            wait (juce::jmax (1, static_cast<int> (next - now)));
        }
    }

    /** The show's name, big in the middle (and "Back in a moment" under it, while held). Sized
        by the frame's short side, so the vertical picture's is as the landscape one's. */
    static void drawName (juce::Graphics& g, const juce::Image& frame, const juce::String& title, bool comingBack, float alpha)
    {
        const auto unit = static_cast<float> (juce::jmin (frame.getWidth(), frame.getHeight()));
        const auto u = juce::jmin (frame.getWidth(), frame.getHeight());
        const auto name = title.isNotEmpty() ? title : juce::String ("Sonify Live Show");
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.setFont (juce::Font (juce::FontOptions (unit * 0.08f, juce::Font::bold)));

        if (! comingBack)
        {
            g.drawFittedText (name, frame.getBounds().reduced (u / 20, 0), juce::Justification::centred, 2);
            return;
        }

        g.drawFittedText (name, frame.getBounds().withTrimmedBottom (u / 8).reduced (u / 20, 0), juce::Justification::centred, 2);
        g.setColour (accent);
        g.setFont (juce::Font (juce::FontOptions (unit * 0.04f)));
        g.drawText ("Back in a moment", frame.getBounds().withTrimmedTop (u / 4), juce::Justification::centred);
    }

    /** Where the overlays go: in from the sides; the vertical picture's top and bottom further in,
        clear of what TikTok's app puts over it (the name at the top, the comments at the bottom). */
    struct Margins
    {
        float unit, side, top, bottom;
    };

    static Margins marginsFor (float fw, float fh, bool tall)
    {
        const auto unit = juce::jmin (fw, fh);
        const auto side = unit * 0.035f;
        return { unit, side, tall ? fh * 0.085f : side, tall ? fh * 0.2f : side };
    }

    /** The logo's width at its height (unit * 0.12), at most 30% of the picture's. */
    static float logoWidth (const Look& l, float fw, float unit)
    {
        if (! l.logo.isValid())
            return 0.0f;

        const auto lh = unit * 0.12f;
        return juce::jmin (fw * 0.3f, lh * static_cast<float> (l.logo.getWidth()) / static_cast<float> (juce::jmax (1, l.logo.getHeight())));
    }

    /** The logo, top right. */
    static void drawLogo (juce::Graphics& g, const Look& l, float fw, const Margins& m)
    {
        if (! l.logo.isValid())
            return;

        const auto lh = m.unit * 0.12f;
        const auto lw = logoWidth (l, fw, m.unit);
        g.setOpacity (0.95f);
        g.drawImage (l.logo, juce::Rectangle<float> (fw - lw - m.side, m.top, lw, lh), juce::RectanglePlacement::centred);
        g.setOpacity (1.0f);
    }

    /** One picture as laid out (the landscape one, or the vertical one): the main source (fitted
        in the landscape picture; filling the vertical one, from its focus across), the boxes over
        it (in row order: the screen's on top), then the title, what's playing and the logo. And
        what goes out: held, the show's name instead (the logo too). */
    void composeOne (bool tall, const Look& l, const std::array<Source, numSources>& s, const std::array<juce::Image, numSources>& pictures,
                     const juce::String& playing, bool away, juce::Image& layout, juce::Image& out)
    {
        const auto size = tall ? Studio::tallSize (l) : juce::Point<int> (juce::jmax (16, l.width), juce::jmax (16, l.height));
        const auto w = size.x, h = size.y;
        const auto fw = static_cast<float> (w), fh = static_cast<float> (h);
        const auto m = marginsFor (fw, fh, tall);
        auto frame = freeImage (tall ? tallPool : pool, w, h, 6);

        {
            juce::Graphics g (frame);
            g.fillAll (background);
            g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);

            // The main picture: the one source shown whole (the first, if more say so).
            const juce::Image* main = nullptr;
            float focus = 0.5f;
            bool mainChosen = false;

            for (size_t i = 0; i < s.size() && ! mainChosen; ++i)
            {
                if (s[i].show != Show::main || s[i].id.isEmpty())
                    continue;

                mainChosen = true;
                focus = s[i].tallFocus;

                if (pictures[i].isValid())
                    main = &pictures[i];
            }

            if (main != nullptr && tall)
                drawCover (g, *main, w, h, focus);
            else if (main != nullptr)
                g.drawImageWithin (*main, 0, 0, w, h, juce::RectanglePlacement::centred);
            else if (! mainChosen)
                drawName (g, frame, l.title, false, 0.85f);   // nothing on Main: the show's name
            else
                drawName (g, frame, l.title, false, 0.18f);   // its camera still opening: the name, quietly

            // The boxes, each its own size and place (in each picture its own).
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (s[i].show != Show::custom || ! pictures[i].isValid())
                    continue;

                const auto& b = tall ? s[i].tallBox : s[i].box;
                const juce::Rectangle<float> box (b.getX() * fw, b.getY() * fh, b.getWidth() * fw, b.getHeight() * fh);

                if (box.getWidth() >= 4.0f && box.getHeight() >= 4.0f)
                    drawBox (g, pictures[i], box, m.unit);
            }

            // The title, top left (beside the logo, never under it).
            if (l.title.isNotEmpty())
            {
                const juce::Font font (juce::FontOptions (m.unit * 0.042f, juce::Font::bold));
                const auto textWidth = juce::GlyphArrangement::getStringWidth (font, l.title);
                const auto logoRoom = l.logo.isValid() ? logoWidth (l, fw, m.unit) + m.side * 2.0f : 0.0f;
                const auto widest = juce::jmin (fw * 0.7f, fw - m.side - logoRoom);
                const juce::Rectangle<float> box (m.side, m.top, juce::jmax (m.unit * 0.2f, juce::jmin (widest, textWidth + m.unit * 0.05f)), m.unit * 0.07f);
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRoundedRectangle (box, m.unit * 0.012f);
                g.setColour (accent);
                g.fillRect (box.withWidth (m.unit * 0.008f));
                g.setColour (juce::Colours::white);
                g.setFont (font);
                g.drawFittedText (l.title, box.reduced (m.unit * 0.025f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1);
            }

            // What's playing, bottom left.
            if (l.showNowPlaying && playing.isNotEmpty())
            {
                const juce::Font caption (juce::FontOptions (m.unit * 0.022f, juce::Font::bold));
                const juce::Font font (juce::FontOptions (m.unit * 0.036f));
                const auto textWidth = juce::jmax (juce::GlyphArrangement::getStringWidth (font, playing),
                                                   juce::GlyphArrangement::getStringWidth (caption, "NOW PLAYING"));
                const auto boxWidth = juce::jmin (tall ? fw - m.side * 2.0f : fw * 0.6f, textWidth + m.unit * 0.06f);
                const juce::Rectangle<float> box (m.side, fh - m.bottom - m.unit * 0.1f, boxWidth, m.unit * 0.1f);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRoundedRectangle (box, m.unit * 0.012f);
                auto inner = box.reduced (m.unit * 0.03f, m.unit * 0.014f);
                g.setColour (accent);
                g.setFont (caption);
                g.drawText ("NOW PLAYING", inner.removeFromTop (inner.getHeight() * 0.38f), juce::Justification::centredLeft);
                g.setColour (juce::Colours::white);
                g.setFont (font);
                g.drawFittedText (playing, inner.toNearestInt(), juce::Justification::centredLeft, 1);
            }

            drawLogo (g, l, fw, m);
        }

        // Streaming not enabled: what goes out is the show's name, and that it's coming back.
        out = frame;

        if (away)
        {
            out = freeImage (tall ? tallHeldPool : heldPool, w, h, 3);
            juce::Graphics g (out);
            g.fillAll (background);
            drawName (g, out, l.title, true, 0.85f);
            drawLogo (g, l, fw, m);
        }

        layout = frame;
    }

    /** The pictures wanted now: the landscape one while it's previewed or a destination takes it,
        the vertical one while it's previewed or a vertical destination takes it. */
    void compose()
    {
        Look l;
        std::array<Source, numSources> s;
        std::array<juce::Image, numSources> pictures;
        juce::String playing;
        bool away = false, wide = false, tall = false;

        {
            const std::lock_guard<std::mutex> hold (lock);
            l = look;
            s = sources;
            away = holding;
            playing = nowPlaying;
            wide = previewing;
            tall = previewing && verticalPreview;

           #if FANAN_FFMPEG
            if (streamer != nullptr)
            {
                wide = wide || streamer->wantsWide();
                tall = tall || streamer->wantsTall();
            }
           #endif

            for (size_t i = 0; i < captures.size(); ++i)
                if (captures[i] != nullptr && s[i].show != Show::hidden)
                    pictures[i] = captures[i]->delayed();
        }

        juce::Image wideLayout, wideOut, tallLayout, tallOut;

        if (wide)
            composeOne (false, l, s, pictures, playing, away, wideLayout, wideOut);

        if (tall)
            composeOne (true, l, s, pictures, playing, away, tallLayout, tallOut);

        const std::lock_guard<std::mutex> hold (lock);
        layoutPicture = wideLayout;
        picture = wideOut;
        tallLayoutPicture = tallLayout;
        tallPicture = tallOut;
    }

    mutable std::mutex lock;
    std::array<Source, numSources> sources;
    std::array<std::unique_ptr<Capture>, numSources> captures;
    Look look;
    juce::String nowPlaying;
    juce::Image picture, layoutPicture;
    juce::Image tallPicture, tallLayoutPicture;
    std::vector<juce::Image> pool, heldPool, tallPool, tallHeldPool;
    bool previewing = false;
    bool verticalPreview = false;
    bool holding = false;
    std::atomic<double> madePerSecond { 0.0 };

   #if FANAN_FFMPEG
    std::unique_ptr<Streamer> streamer;
   #endif
};

//==============================================================================
Studio::Studio()
    : impl (std::make_unique<Impl>())
{
}

Studio::~Studio() = default;

void Studio::setSources (const std::array<Source, numSources>& sources)
{
    {
        const std::lock_guard<std::mutex> hold (impl->lock);
        impl->sources = sources;

        for (auto& source : impl->sources)
        {
            source.id = source.id.trim();
            source.delayMs = juce::jlimit (0, maxDelayMs, source.delayMs);
            source.tallFocus = std::isfinite (source.tallFocus) ? juce::jlimit (0.0f, 1.0f, source.tallFocus) : 0.5f;
        }
    }

    impl->applyCaptures();
}

juce::String Studio::getSourceProblem() const
{
    const std::lock_guard<std::mutex> hold (impl->lock);

    for (const auto& source : impl->captures)
        if (source != nullptr)
            if (const auto why = source->getProblem(); why.isNotEmpty())
                return why;

    return {};
}

juce::String Studio::getSourceProblem (int source) const
{
    const std::lock_guard<std::mutex> hold (impl->lock);

    if (! juce::isPositiveAndBelow (source, numSources) || impl->captures[static_cast<size_t> (source)] == nullptr)
        return {};

    return impl->captures[static_cast<size_t> (source)]->getProblem();
}

juce::Image Studio::getSourcePicture (int source) const
{
    const std::lock_guard<std::mutex> hold (impl->lock);

    if (! juce::isPositiveAndBelow (source, numSources) || impl->captures[static_cast<size_t> (source)] == nullptr)
        return {};

    return impl->captures[static_cast<size_t> (source)]->latest();
}

void Studio::setLook (const Look& look)
{
    const std::lock_guard<std::mutex> hold (impl->lock);
    impl->look = look;
}

void Studio::setNowPlaying (const juce::String& title)
{
    const std::lock_guard<std::mutex> hold (impl->lock);
    impl->nowPlaying = title;
}

void Studio::setHolding (bool holding)
{
    const std::lock_guard<std::mutex> hold (impl->lock);
    impl->holding = holding;
}

void Studio::setPreviewing (bool previewing)
{
    {
        const std::lock_guard<std::mutex> hold (impl->lock);

        if (impl->previewing == previewing)
            return;

        impl->previewing = previewing;
    }

    impl->applyCaptures();
}

juce::Image Studio::getPicture() const
{
    return impl->getPicture();
}

juce::Image Studio::getLayoutPicture() const
{
    return impl->getLayoutPicture();
}

void Studio::setVerticalPreview (bool previewing)
{
    const std::lock_guard<std::mutex> hold (impl->lock);
    impl->verticalPreview = previewing;
}

juce::Image Studio::getTallPicture() const
{
    return impl->getTallPicture();
}

juce::Image Studio::getTallLayoutPicture() const
{
    return impl->getTallLayoutPicture();
}

juce::Point<int> Studio::tallSize (const Look& look)
{
    return { juce::jmax (16, look.height) & ~1, juce::jmax (16, look.width) & ~1 };
}

juce::Result Studio::start (const Settings& settings, LiveShow::SoundTap& tap)
{
   #if FANAN_FFMPEG
    if (const auto why = problem(); why.isNotEmpty())
        return juce::Result::fail (why);

    if (settings.destinations.empty())
        return juce::Result::fail ("Add a destination first: YouTube, Facebook, Twitch, TikTok or a server of your own (or a copy on this computer).");

    Look look;

    {
        const std::lock_guard<std::mutex> hold (impl->lock);

        if (impl->streamer != nullptr)
            return juce::Result::ok();

        look = impl->look;
    }

    // The pictures straight from the Impl, not through impl: while the Studio goes, impl may be
    // null already (as the Impl ends the stream).
    auto* const studio = impl.get();
    auto streamer = std::make_unique<Streamer> (settings, tap, [studio] { return studio->getPicture(); }, [studio] { return studio->getTallPicture(); },
                                                juce::Point<int> (look.width, look.height), tallSize (look), look.fps);

    {
        const std::lock_guard<std::mutex> hold (impl->lock);
        impl->streamer = std::move (streamer);
    }

    impl->applyCaptures();   // the sources open, if the picture wasn't being made
    return juce::Result::ok();
   #else
    juce::ignoreUnused (settings, tap);
    return juce::Result::fail (problem());
   #endif
}

void Studio::stop()
{
   #if FANAN_FFMPEG
    const std::lock_guard<std::mutex> hold (impl->lock);

    if (impl->streamer != nullptr)
        impl->streamer->end();
   #endif
}

bool Studio::collect()
{
   #if FANAN_FFMPEG
    std::unique_ptr<Streamer> done;

    {
        const std::lock_guard<std::mutex> hold (impl->lock);

        if (impl->streamer == nullptr || ! impl->streamer->isDone())
            return false;

        done = std::move (impl->streamer);
    }

    done = nullptr;          // it goes here, its thread already ended
    impl->applyCaptures();   // the sources let go, if nothing shows the picture
    return true;
   #else
    return false;
   #endif
}

bool Studio::isStreaming() const
{
   #if FANAN_FFMPEG
    const std::lock_guard<std::mutex> hold (impl->lock);
    return impl->streamer != nullptr;
   #else
    return false;
   #endif
}

Stats Studio::getStats() const
{
    Stats stats;

   #if FANAN_FFMPEG
    {
        const std::lock_guard<std::mutex> hold (impl->lock);

        if (impl->streamer != nullptr)
            stats = impl->streamer->getStats();
    }
   #endif

    stats.fps = impl->madePerSecond.load();
    return stats;
}
}
