// D:\workspace\Fanan_web_hub\src\VideoMedia.cpp

#include "VideoMedia.h"
#include "VideoMp4.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <chrono>
#include <map>
#include <cmath>
#include <cstring>
#include <iterator>
#include <thread>
#include <type_traits>

#if JUCE_WINDOWS
 #include <stdlib.h>
#endif

namespace
{
    //==============================================================================
    // libvlc 3's C interface: what Sonify uses of it, as its headers declare it. Sonify doesn't
    // link with VLC or need its SDK: it finds these in libvlc when video is first used, and
    // without VLC everything but video works.
    extern "C"
    {
        struct libvlc_instance_t;
        struct libvlc_media_t;
        struct libvlc_media_player_t;
        using libvlc_time_t = juce::int64;

        enum libvlc_state_t
        {
            libvlc_NothingSpecial = 0, libvlc_Opening, libvlc_Buffering, libvlc_Playing,
            libvlc_Paused, libvlc_Stopped, libvlc_Ended, libvlc_Error
        };

        enum libvlc_track_type_t { libvlc_track_unknown = -1, libvlc_track_audio = 0, libvlc_track_video = 1, libvlc_track_text = 2 };
        enum libvlc_media_parse_flag_t { libvlc_media_parse_local = 0x00 };

        enum libvlc_media_parsed_status_t
        {
            libvlc_media_parsed_status_skipped = 1, libvlc_media_parsed_status_failed,
            libvlc_media_parsed_status_timeout, libvlc_media_parsed_status_done
        };

        enum libvlc_video_orient_t { libvlc_video_orient_top_left };
        enum libvlc_video_projection_t { libvlc_video_projection_rectangular };

        struct libvlc_audio_track_t { unsigned i_channels, i_rate; };
        struct libvlc_video_viewpoint_t { float f_yaw, f_pitch, f_roll, f_field_of_view; };

        struct libvlc_video_track_t
        {
            unsigned i_height, i_width, i_sar_num, i_sar_den, i_frame_rate_num, i_frame_rate_den;
            libvlc_video_orient_t i_orientation;
            libvlc_video_projection_t i_projection;
            libvlc_video_viewpoint_t pose;
        };

        struct libvlc_subtitle_track_t { char* psz_encoding; };

        struct libvlc_media_track_t
        {
            juce::uint32 i_codec, i_original_fourcc;
            int i_id;
            libvlc_track_type_t i_type;
            int i_profile, i_level;

            union
            {
                libvlc_audio_track_t* audio;
                libvlc_video_track_t* video;
                libvlc_subtitle_track_t* subtitle;
            };

            unsigned int i_bitrate;
            char* psz_language;
            char* psz_description;
        };

        using libvlc_video_lock_cb = void* (*) (void* opaque, void** planes);
        using libvlc_video_unlock_cb = void (*) (void* opaque, void* picture, void* const* planes);
        using libvlc_video_display_cb = void (*) (void* opaque, void* picture);
        using libvlc_video_format_cb = unsigned (*) (void** opaque, char* chroma, unsigned* width, unsigned* height,
                                                     unsigned* pitches, unsigned* lines);
        using libvlc_video_cleanup_cb = void (*) (void* opaque);
    }

    static_assert (sizeof (libvlc_state_t) == sizeof (int) && sizeof (libvlc_track_type_t) == sizeof (int));

    //==============================================================================
   #define SONIFY_VLC_FUNCTIONS(X) \
        X (libvlc_new, libvlc_instance_t* (*) (int, const char* const*)) \
        X (libvlc_release, void (*) (libvlc_instance_t*)) \
        X (libvlc_errmsg, const char* (*) ()) \
        X (libvlc_media_new_path, libvlc_media_t* (*) (libvlc_instance_t*, const char*)) \
        X (libvlc_media_add_option, void (*) (libvlc_media_t*, const char*)) \
        X (libvlc_media_release, void (*) (libvlc_media_t*)) \
        X (libvlc_media_parse_with_options, int (*) (libvlc_media_t*, libvlc_media_parse_flag_t, int)) \
        X (libvlc_media_get_parsed_status, libvlc_media_parsed_status_t (*) (libvlc_media_t*)) \
        X (libvlc_media_get_duration, libvlc_time_t (*) (libvlc_media_t*)) \
        X (libvlc_media_tracks_get, unsigned (*) (libvlc_media_t*, libvlc_media_track_t***)) \
        X (libvlc_media_tracks_release, void (*) (libvlc_media_track_t**, unsigned)) \
        X (libvlc_media_player_new_from_media, libvlc_media_player_t* (*) (libvlc_media_t*)) \
        X (libvlc_media_player_release, void (*) (libvlc_media_player_t*)) \
        X (libvlc_media_player_play, int (*) (libvlc_media_player_t*)) \
        X (libvlc_media_player_stop, void (*) (libvlc_media_player_t*)) \
        X (libvlc_media_player_set_pause, void (*) (libvlc_media_player_t*, int)) \
        X (libvlc_media_player_set_time, void (*) (libvlc_media_player_t*, libvlc_time_t)) \
        X (libvlc_media_player_get_time, libvlc_time_t (*) (libvlc_media_player_t*)) \
        X (libvlc_media_player_get_length, libvlc_time_t (*) (libvlc_media_player_t*)) \
        X (libvlc_media_player_set_rate, int (*) (libvlc_media_player_t*, float)) \
        X (libvlc_media_player_get_state, libvlc_state_t (*) (libvlc_media_player_t*)) \
        X (libvlc_media_player_get_position, float (*) (libvlc_media_player_t*)) \
        X (libvlc_video_set_callbacks, void (*) (libvlc_media_player_t*, libvlc_video_lock_cb, libvlc_video_unlock_cb, \
                                                 libvlc_video_display_cb, void*)) \
        X (libvlc_video_set_format_callbacks, void (*) (libvlc_media_player_t*, libvlc_video_format_cb, libvlc_video_cleanup_cb))

    struct Vlc
    {
       #define SONIFY_VLC_DECLARE(name, ...) std::type_identity_t<__VA_ARGS__> name = nullptr;
        SONIFY_VLC_FUNCTIONS (SONIFY_VLC_DECLARE)
       #undef SONIFY_VLC_DECLARE

        juce::DynamicLibrary library;
        libvlc_instance_t* instance = nullptr;
        juce::String problem;
        std::mutex lock;

        Vlc()
        {
            auto path = juce::SystemStats::getEnvironmentVariable ("SONIFY_LIBVLC", {});   // the tests' VLC
            const auto folder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

            if (path.isEmpty())
            {
               #if JUCE_WINDOWS
                const auto file = folder.getChildFile ("libvlc.dll");
               #elif JUCE_MAC
                const auto file = folder.getSiblingFile ("Frameworks").getChildFile ("libvlc.dylib");
               #else
                const juce::File file;
               #endif

               #if JUCE_WINDOWS || JUCE_MAC
                if (! file.existsAsFile())
                {
                    problem = "Video needs VLC (" + file.getFileName() + " and its plugins folder) next to Sonify: building Sonify puts them there.";
                    return;
                }

                path = file.getFullPathName();
               #else
                path = "libvlc.so.5";
               #endif
            }

           #if JUCE_WINDOWS
            // VLC finds its plugins beside libvlc; saying so too does no harm.
            const auto plugins = juce::File (path).getSiblingFile ("plugins").getFullPathName();
            _wputenv_s (L"VLC_PLUGIN_PATH", plugins.toWideCharPointer());
           #endif

            if (! library.open (path))
            {
                problem = "VLC (" + juce::File (path).getFileName() + ") couldn't be loaded.";
                return;
            }

            bool found = true;

           #define SONIFY_VLC_FIND(name, ...) \
            name = reinterpret_cast<decltype (name)> (library.getFunction (#name)); \
            found = found && name != nullptr;
            SONIFY_VLC_FUNCTIONS (SONIFY_VLC_FIND)
           #undef SONIFY_VLC_FIND

            if (! found)
            {
                problem = "This VLC is too old for Sonify: it needs VLC 3.";
                library.close();
                return;
            }

            const char* const options[] = {
                "--quiet", "--ignore-config", "--no-video-title-show", "--no-osd", "--no-snapshot-preview",
                "--no-stats", "--no-sub-autodetect-file", "--no-metadata-network-access", "--no-lua", "--no-xlib"
            };

            instance = libvlc_new (static_cast<int> (std::size (options)), options);

            if (instance == nullptr)
            {
                const char* const fewer[] = { "--quiet", "--ignore-config" };
                instance = libvlc_new (static_cast<int> (std::size (fewer)), fewer);
            }

            if (instance == nullptr)
            {
                const auto* message = libvlc_errmsg();
                problem = "VLC couldn't start" + (message != nullptr ? ": " + juce::String::fromUTF8 (message) : juce::String (".")) ;
            }
        }
    };

    Vlc& vlc();

    /** At the app's end (every player gone by then), VLC's own threads are let go of before its
        library is. */
    struct VlcRelease
    {
        ~VlcRelease()    { VideoMedia::shutdown(); }
    };

    Vlc& vlc()
    {
        static Vlc api;
        static VlcRelease release;   // made after api, so it's undone first
        return api;
    }

    bool vlcReady()
    {
        auto& api = vlc();
        return api.problem.isEmpty() && api.instance != nullptr;
    }

    double nowSeconds() noexcept
    {
        return juce::Time::getMillisecondCounterHiRes() * 0.001;
    }

    void sleepMs (int ms)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (ms));
    }

    //==============================================================================
    /** A path in a VLC output chain: quoted, its quotes and backslashes escaped (VLC takes
        forward slashes on Windows too). */
    juce::String chainPath (const juce::File& file)
    {
        auto path = file.getFullPathName();

       #if JUCE_WINDOWS
        path = path.replaceCharacter ('\\', '/');
       #endif

        juce::String result ("'");

        for (const auto c : path)
        {
            if (c == '\'' || c == '"' || c == '\\')
                result << '\\';

            result << juce::String::charToString (c);
        }

        return result + "'";
    }

    bool chainCanWrite (const juce::File& file)
    {
        // VLC's chains can't carry braces in a value.
        return ! file.getFullPathName().containsAnyOf ("{}");
    }

    /** Has VLC read a file with these options (an output chain among them) to its end. reached:
        how long VLC said the file is, or how far it read when it didn't say (seconds). */
    juce::Result runVlc (const juce::File& video, const juce::StringArray& options,
                         const std::function<void (double)>& progress, std::atomic<bool>* cancelled,
                         double* reached = nullptr)
    {
        if (! vlcReady())
            return juce::Result::fail (vlc().problem.isNotEmpty() ? vlc().problem : juce::String ("VLC has stopped."));

        auto& api = vlc();
        auto* media = api.libvlc_media_new_path (api.instance, video.getFullPathName().toRawUTF8());

        if (media == nullptr)
            return juce::Result::fail ("VLC couldn't open " + video.getFileName() + ".");

        for (const auto& option : options)
            api.libvlc_media_add_option (media, option.toRawUTF8());

        auto* player = api.libvlc_media_player_new_from_media (media);
        api.libvlc_media_release (media);

        if (player == nullptr)
            return juce::Result::fail ("VLC couldn't open " + video.getFileName() + ".");

        juce::Result result = juce::Result::ok();

        if (api.libvlc_media_player_play (player) != 0)
        {
            result = juce::Result::fail ("VLC couldn't read " + video.getFileName() + ".");
        }
        else
        {
            float lastPosition = -1.0f;
            auto lastMoved = nowSeconds();
            bool began = false;

            for (;;)
            {
                sleepMs (15);
                const auto state = api.libvlc_media_player_get_state (player);

                if (reached != nullptr)
                {
                    const auto said = juce::jmax (api.libvlc_media_player_get_length (player), api.libvlc_media_player_get_time (player));
                    *reached = juce::jmax (*reached, static_cast<double> (said) / 1000.0);
                }

                if (state == libvlc_Ended)
                    break;

                if (state == libvlc_Error)
                {
                    result = juce::Result::fail ("VLC couldn't read " + video.getFileName() + ".");
                    break;
                }

                if (state == libvlc_Stopped && began)
                    break;

                if (state == libvlc_Playing)
                    began = true;

                if (cancelled != nullptr && cancelled->load())
                {
                    result = juce::Result::fail ("Cancelled");
                    break;
                }

                const auto position = api.libvlc_media_player_get_position (player);

                if (position != lastPosition)
                {
                    lastPosition = position;
                    lastMoved = nowSeconds();

                    if (progress && position >= 0.0f)
                        progress (juce::jlimit (0.0, 1.0, static_cast<double> (position)));
                }
                else if (nowSeconds() - lastMoved > 60.0)
                {
                    result = juce::Result::fail ("VLC stopped reading " + video.getFileName() + ".");
                    break;
                }
            }
        }

        api.libvlc_media_player_stop (player);
        api.libvlc_media_player_release (player);
        return result;
    }

    //==============================================================================
    juce::File& cacheRoot()
    {
        static juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Sonify Video Cache");
        return folder;
    }

    std::mutex& cacheLock()
    {
        static std::mutex lock;
        return lock;
    }

    /** A video's own folder in the cache: by its path, size and date (a changed file is new). */
    juce::File folderFor (const juce::File& video)
    {
        const auto identity = video.getFullPathName() + "|" + juce::String (video.getSize()) + "|"
                            + juce::String (video.getLastModificationTime().toMilliseconds()) + "|1";
        return VideoMedia::cacheFolder().getChildFile (juce::String::toHexString (identity.hashCode64()));
    }

    /** One preparation at a time in a video's folder (the playlist and the song may both want it). */
    std::shared_ptr<std::mutex> lockFor (const juce::File& folder)
    {
        static std::mutex guard;
        static std::map<juce::String, std::weak_ptr<std::mutex>> locks;
        const std::lock_guard<std::mutex> hold (guard);
        auto& slot = locks[folder.getFullPathName()];
        auto lock = slot.lock();

        if (lock == nullptr)
        {
            lock = std::make_shared<std::mutex>();
            slot = lock;
        }

        return lock;
    }

    /** A file written beside where it goes, then put in its place: never half a file there. */
    bool putInPlace (const juce::File& written, const juce::File& target)
    {
        target.deleteFile();
        return written.moveFileTo (target);
    }

    constexpr int maxThumbnails = 300;
    constexpr int thumbnailColumns = 20;
    constexpr int thumbnailHeight = 90;

    std::optional<VideoMedia::Prepared> readInfo (const juce::File& folder, const juce::File& video)
    {
        const auto json = juce::JSON::parse (folder.getChildFile ("info.json"));

        // (2: the lengths of files that only say once they play are VLC's, no longer counted from
        // the small pictures, which stop at maxThumbnails: those of 1 are made again.)
        if (! json.isObject() || static_cast<int> (json.getProperty ("version", 0)) != 2)
            return std::nullopt;

        VideoMedia::Prepared prepared;
        auto& info = prepared.info;
        info.seconds = json.getProperty ("seconds", 0.0);
        info.width = json.getProperty ("width", 0);
        info.height = json.getProperty ("height", 0);
        info.fps = json.getProperty ("fps", 0.0);
        info.hasSound = json.getProperty ("hasSound", false);
        info.soundRate = json.getProperty ("soundRate", 0.0);

        const auto thumbs = json.getProperty ("thumbnails", {});

        if (thumbs.isObject())
        {
            auto& t = prepared.thumbnails;
            t.file = folder.getChildFile ("thumbnails.jpg");
            t.count = thumbs.getProperty ("count", 0);
            t.columns = thumbs.getProperty ("columns", 0);
            t.cellWidth = thumbs.getProperty ("cellWidth", 0);
            t.cellHeight = thumbs.getProperty ("cellHeight", 0);
            t.every = thumbs.getProperty ("every", 0.0);
        }

        juce::ignoreUnused (video);
        return prepared;
    }

    void writeInfo (const juce::File& folder, const VideoMedia::Prepared& prepared)
    {
        juce::DynamicObject::Ptr json (new juce::DynamicObject());
        const auto& info = prepared.info;
        json->setProperty ("version", 2);
        json->setProperty ("seconds", info.seconds);
        json->setProperty ("width", info.width);
        json->setProperty ("height", info.height);
        json->setProperty ("fps", info.fps);
        json->setProperty ("hasSound", info.hasSound);
        json->setProperty ("soundRate", info.soundRate);

        if (prepared.thumbnails.count > 0)
        {
            const auto& t = prepared.thumbnails;
            juce::DynamicObject::Ptr thumbs (new juce::DynamicObject());
            thumbs->setProperty ("count", t.count);
            thumbs->setProperty ("columns", t.columns);
            thumbs->setProperty ("cellWidth", t.cellWidth);
            thumbs->setProperty ("cellHeight", t.cellHeight);
            thumbs->setProperty ("every", t.every);
            json->setProperty ("thumbnails", juce::var (thumbs.get()));
        }

        const auto part = folder.getChildFile ("info.part.json");

        if (part.replaceWithText (juce::JSON::toString (juce::var (json.get()))))
            putInPlace (part, folder.getChildFile ("info.json"));
    }

    //==============================================================================
    std::unique_ptr<juce::AudioFormatWriter> makeWav (const juce::File& file, double rate, int channels, int bits)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();

        if (stream == nullptr)
            return nullptr;

        return juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                                 .withSampleRate (rate)
                                                                 .withNumChannels (channels)
                                                                 .withBitsPerSample (bits)
                                                                 .withSampleFormat (bits == 32 ? juce::AudioFormatWriterOptions::SampleFormat::floatingPoint
                                                                                               : juce::AudioFormatWriterOptions::SampleFormat::integral));
    }

    /** VLC's WAV of the sound, put where the picture says (samples added in front, or taken off). */
    juce::Result lineUpSound (const juce::File& raw, const juce::File& target, int correction, double& rate, double& seconds)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (raw));

        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
            return juce::Result::fail ("empty");

        rate = reader->sampleRate;
        auto writer = makeWav (target, rate, 2, 32);

        if (writer == nullptr)
            return juce::Result::fail ("Couldn't write " + target.getFullPathName());

        constexpr int block = 16384;
        juce::AudioBuffer<float> buffer (2, block);
        juce::int64 written = 0;

        if (correction > 0)
        {
            buffer.clear();

            for (auto left = static_cast<juce::int64> (correction); left > 0; left -= block)
            {
                const auto count = static_cast<int> (juce::jmin<juce::int64> (block, left));
                writer->writeFromAudioSampleBuffer (buffer, 0, count);
                written += count;
            }
        }

        const auto length = reader->lengthInSamples;

        for (juce::int64 at = juce::jmax (0, -correction); at < length; at += block)
        {
            const auto count = static_cast<int> (juce::jmin<juce::int64> (block, length - at));
            reader->read (&buffer, 0, count, at, true, true);

            if (reader->numChannels == 1)
                buffer.copyFrom (1, 0, buffer, 0, 0, count);

            if (! writer->writeFromAudioSampleBuffer (buffer, 0, count))
                return juce::Result::fail ("Couldn't write " + target.getFullPathName());

            written += count;
        }

        writer.reset();
        seconds = static_cast<double> (written) / rate;
        return juce::Result::ok();
    }

    juce::Result writeSilence (const juce::File& target, double seconds)
    {
        constexpr double rate = 8000.0;
        auto writer = makeWav (target, rate, 1, 16);

        if (writer == nullptr)
            return juce::Result::fail ("Couldn't write " + target.getFullPathName());

        juce::AudioBuffer<float> buffer (1, 8000);
        buffer.clear();
        auto left = static_cast<juce::int64> (std::ceil (juce::jmax (0.1, seconds) * rate));

        while (left > 0)
        {
            const auto count = static_cast<int> (juce::jmin<juce::int64> (8000, left));
            writer->writeFromAudioSampleBuffer (buffer, 0, count);
            left -= count;
        }

        return juce::Result::ok();
    }

    /** The JPEGs VLC wrote one after another, into one grid of cells. */
    juce::Result buildThumbnails (const juce::File& frames, VideoMedia::Thumbnails& thumbs)
    {
        juce::MemoryBlock block;

        if (! frames.loadFileAsData (block) || block.getSize() < 4)
            return juce::Result::fail ("No pictures came out of the video.");

        const auto* data = static_cast<const juce::uint8*> (block.getData());
        const auto size = block.getSize();
        std::vector<size_t> starts;

        for (size_t i = 0; i + 3 <= size; ++i)
            if (data[i] == 0xff && data[i + 1] == 0xd8 && data[i + 2] == 0xff)
                starts.push_back (i);

        starts.resize (juce::jmin (starts.size(), static_cast<size_t> (maxThumbnails)));

        if (starts.empty())
            return juce::Result::fail ("No pictures came out of the video.");

        const auto count = static_cast<int> (starts.size());
        const auto columns = juce::jmin (thumbnailColumns, count);
        const auto rows = (count + columns - 1) / columns;
        juce::Image sheet (juce::Image::RGB, columns * thumbs.cellWidth, rows * thumbs.cellHeight, true, juce::SoftwareImageType());

        {
            juce::Graphics g (sheet);
            g.fillAll (juce::Colours::black);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

            for (int i = 0; i < count; ++i)
            {
                const auto from = starts[static_cast<size_t> (i)];
                const auto to = i + 1 < count ? starts[static_cast<size_t> (i + 1)] : size;
                const auto image = juce::ImageFileFormat::loadFrom (data + from, to - from);

                if (! image.isValid())
                    continue;

                const juce::Rectangle<float> cell (static_cast<float> ((i % columns) * thumbs.cellWidth),
                                                   static_cast<float> ((i / columns) * thumbs.cellHeight),
                                                   static_cast<float> (thumbs.cellWidth), static_cast<float> (thumbs.cellHeight));
                g.drawImage (image, cell, juce::RectanglePlacement::stretchToFit);
            }
        }

        const auto part = thumbs.file.getSiblingFile ("thumbnails.part.jpg");
        part.deleteFile();

        {
            juce::FileOutputStream out (part);

            if (! out.openedOk())
                return juce::Result::fail ("Couldn't write " + thumbs.file.getFullPathName());

            juce::JPEGImageFormat jpeg;
            jpeg.setQuality (0.82f);

            if (! jpeg.writeImageToStream (sheet, out))
                return juce::Result::fail ("Couldn't write " + thumbs.file.getFullPathName());
        }

        if (! putInPlace (part, thumbs.file))
            return juce::Result::fail ("Couldn't write " + thumbs.file.getFullPathName());

        thumbs.count = count;
        thumbs.columns = columns;
        return juce::Result::ok();
    }
}

//==============================================================================
juce::String VideoMedia::problem()
{
    return vlcReady() ? juce::String() : vlc().problem.isNotEmpty() ? vlc().problem : juce::String ("VLC has stopped.");
}

void VideoMedia::shutdown()
{
    auto& api = vlc();
    const std::lock_guard<std::mutex> guard (api.lock);

    if (api.instance != nullptr)
    {
        api.libvlc_release (api.instance);
        api.instance = nullptr;
    }
}

juce::String VideoMedia::wildcards()
{
    return "*.mp4;*.m4v;*.mov;*.mkv;*.webm;*.avi;*.wmv;*.mpg;*.mpeg;*.flv;*.3gp;*.ts;*.mts;*.m2ts;*.vob;*.ogv";
}

bool VideoMedia::isVideoFile (const juce::File& file)
{
    return file.hasFileExtension (wildcards().removeCharacters ("*").replaceCharacter (',', ';'));
}

void VideoMedia::tidyCache()
{
    const auto folder = cacheFolder();

    juce::Thread::launch ([folder]
    {
        const auto before = juce::Time::getCurrentTime() - juce::RelativeTime::days (60);

        for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findDirectories))
        {
            const auto& child = entry.getFile();

            // Only the cache's own folders (named in hex digits), not used for two months (its
            // info.json is marked each time it's read; a folder without one is a preparation
            // that never finished).
            const auto info = child.getChildFile ("info.json");
            const auto used = info.existsAsFile() ? info.getLastModificationTime() : child.getLastModificationTime();

            if (child.getFileName().length() <= 16 && child.getFileName().containsOnly ("0123456789abcdef") && used < before)
                child.deleteRecursively();
        }
    });
}

void VideoMedia::setCacheFolder (const juce::File& folder)
{
    const std::lock_guard<std::mutex> lock (cacheLock());
    cacheRoot() = folder;
}

juce::File VideoMedia::cacheFolder()
{
    const std::lock_guard<std::mutex> lock (cacheLock());
    return cacheRoot();
}

//==============================================================================
std::optional<VideoMedia::Info> VideoMedia::probe (const juce::File& video, juce::String& error)
{
    if (! vlcReady())
    {
        error = problem();
        return std::nullopt;
    }

    if (! video.existsAsFile())
    {
        error = video.getFileName() + " isn't there any more.";
        return std::nullopt;
    }

    auto& api = vlc();
    auto* media = api.libvlc_media_new_path (api.instance, video.getFullPathName().toRawUTF8());

    if (media == nullptr)
    {
        error = "VLC couldn't open " + video.getFileName() + ".";
        return std::nullopt;
    }

    api.libvlc_media_parse_with_options (media, libvlc_media_parse_local, 15000);
    const auto until = nowSeconds() + 16.0;
    auto status = api.libvlc_media_get_parsed_status (media);

    while (status == 0 && nowSeconds() < until)
    {
        sleepMs (10);
        status = api.libvlc_media_get_parsed_status (media);
    }

    Info info;
    bool picture = false;

    if (status == libvlc_media_parsed_status_done)
    {
        const auto duration = api.libvlc_media_get_duration (media);
        info.seconds = duration > 0 ? static_cast<double> (duration) / 1000.0 : 0.0;

        libvlc_media_track_t** tracks = nullptr;
        const auto count = api.libvlc_media_tracks_get (media, &tracks);

        for (unsigned i = 0; i < count; ++i)
        {
            const auto* track = tracks[i];

            if (track->i_type == libvlc_track_video && ! picture && track->video != nullptr)
            {
                const auto* v = track->video;
                picture = v->i_width > 0 && v->i_height > 0;
                auto width = static_cast<double> (v->i_width);

                if (v->i_sar_num > 0 && v->i_sar_den > 0)
                    width = width * v->i_sar_num / v->i_sar_den;

                info.width = static_cast<int> (std::lround (width));
                info.height = static_cast<int> (v->i_height);
                info.fps = v->i_frame_rate_den > 0 ? static_cast<double> (v->i_frame_rate_num) / v->i_frame_rate_den : 0.0;
            }
            else if (track->i_type == libvlc_track_audio && ! info.hasSound && track->audio != nullptr)
            {
                info.hasSound = true;
                info.soundRate = track->audio->i_rate;
            }
        }

        if (tracks != nullptr)
            api.libvlc_media_tracks_release (tracks, count);
    }

    // Some kinds (MPEG transport streams) only say what's in them once they play: a moment of
    // playing, to nowhere, tells.
    if (! picture)
    {
        struct Seen
        {
            std::atomic<bool> formatted { false };
            std::vector<juce::uint32> pixels;

            static unsigned format (void** opaque, char* chroma, unsigned* w, unsigned* h, unsigned* pitches, unsigned* lines)
            {
                auto* self = static_cast<Seen*> (*opaque);
                std::memcpy (chroma, "RV32", 4);
                *w = 16;
                *h = 16;
                pitches[0] = 64;
                lines[0] = 16;
                self->formatted = true;
                return 1;
            }

            static void* lockPicture (void* opaque, void** planes)
            {
                planes[0] = static_cast<Seen*> (opaque)->pixels.data();
                return nullptr;
            }
        };

        Seen seen;
        seen.pixels.assign (256, 0u);
        api.libvlc_media_add_option (media, ":no-audio");
        api.libvlc_media_add_option (media, ":no-spu");

        if (auto* player = api.libvlc_media_player_new_from_media (media))
        {
            api.libvlc_video_set_callbacks (player, Seen::lockPicture, nullptr, nullptr, &seen);
            api.libvlc_video_set_format_callbacks (player, Seen::format, nullptr);

            if (api.libvlc_media_player_play (player) == 0)
            {
                const auto end = nowSeconds() + 5.0;

                while (! seen.formatted.load() && nowSeconds() < end)
                {
                    const auto state = api.libvlc_media_player_get_state (player);

                    if (state == libvlc_Ended || state == libvlc_Error)
                        break;

                    sleepMs (10);
                }

                sleepMs (100);

                libvlc_media_track_t** tracks = nullptr;
                const auto count = api.libvlc_media_tracks_get (media, &tracks);

                for (unsigned i = 0; i < count; ++i)
                {
                    const auto* track = tracks[i];

                    if (track->i_type == libvlc_track_video && ! picture && track->video != nullptr
                        && track->video->i_width > 0 && track->video->i_height > 0)
                    {
                        const auto* v = track->video;
                        picture = true;
                        auto width = static_cast<double> (v->i_width);

                        if (v->i_sar_num > 0 && v->i_sar_den > 0)
                            width = width * v->i_sar_num / v->i_sar_den;

                        info.width = static_cast<int> (std::lround (width));
                        info.height = static_cast<int> (v->i_height);
                        info.fps = v->i_frame_rate_den > 0 ? static_cast<double> (v->i_frame_rate_num) / v->i_frame_rate_den : 0.0;
                    }
                    else if (track->i_type == libvlc_track_audio && ! info.hasSound && track->audio != nullptr)
                    {
                        info.hasSound = true;
                        info.soundRate = track->audio->i_rate;
                    }
                }

                if (tracks != nullptr)
                    api.libvlc_media_tracks_release (tracks, count);

                // Such files' lengths are guesses until they play: VLC's own, now that it plays (a
                // moment more when it hasn't said yet). The sound is tried, in case there is some.
                const auto lengthBy = nowSeconds() + 1.0;
                auto length = api.libvlc_media_player_get_length (player);

                while (picture && length <= 0 && nowSeconds() < lengthBy)
                {
                    sleepMs (20);
                    length = api.libvlc_media_player_get_length (player);
                }

                info.seconds = length > 0 ? static_cast<double> (length) / 1000.0 : 0.0;
                info.hasSound = true;
                status = libvlc_media_parsed_status_done;
            }

            api.libvlc_media_player_stop (player);
            api.libvlc_media_player_release (player);
        }
    }

    api.libvlc_media_release (media);

    if (status != libvlc_media_parsed_status_done)
    {
        error = "VLC couldn't read " + video.getFileName() + ".";
        return std::nullopt;
    }

    if (! picture)
    {
        error = video.getFileName() + " has no picture.";
        return std::nullopt;
    }

    if (info.fps <= 0.0 || info.fps > 300.0)
        info.fps = 25.0;

    return info;
}

std::optional<VideoMedia::Prepared> VideoMedia::cached (const juce::File& video, Wanted wanted)
{
    const auto folder = folderFor (video);
    auto prepared = readInfo (folder, video);

    if (! prepared)
        return std::nullopt;

    folder.getChildFile ("info.json").setLastModificationTime (juce::Time::getCurrentTime());   // used: kept by tidyCache()

    if (wanted.thumbnails && ! prepared->thumbnails.isValid())
        return std::nullopt;

    if (wanted.sound)
    {
        if (prepared->info.hasSound)
        {
            prepared->sound = folder.getChildFile ("sound.wav");

            if (! prepared->sound.existsAsFile())
                return std::nullopt;
        }
        else if (wanted.silenceWhenNoSound)
        {
            prepared->sound = folder.getChildFile ("silence.wav");

            if (! prepared->sound.existsAsFile())
                return std::nullopt;
        }
    }

    return prepared;
}

VideoMedia::Prepared VideoMedia::prepareNow (const juce::File& video, Wanted wanted, std::atomic<double>* progress,
                                             std::atomic<bool>* cancelled)
{
    if (auto ready = cached (video, wanted))
    {
        if (progress != nullptr)
            progress->store (1.0);

        return *ready;
    }

    Prepared prepared;
    const auto fail = [&prepared] (const juce::String& why) { prepared.error = why; return prepared; };
    const auto report = [progress] (double value) { if (progress != nullptr) progress->store (juce::jlimit (0.0, 1.0, value)); };

    const auto folder = folderFor (video);

    if (! chainCanWrite (folder))
        return fail ("VLC can't write into " + folder.getFullPathName() + " (the folder's name has { or } in it).");

    // (Twice: another preparation may make the cache folder itself at the same moment.)
    if (folder.createDirectory().failed() && folder.createDirectory().failed() && ! folder.isDirectory())
        return fail ("Couldn't make " + folder.getFullPathName());

    // One at a time in this folder: another preparation of the same video may be at work.
    const auto folderLock = lockFor (folder);
    std::unique_lock<std::mutex> working (*folderLock, std::defer_lock);

    while (! working.try_lock())
    {
        if (cancelled != nullptr && cancelled->load())
            return fail ("Cancelled");

        sleepMs (20);
    }

    // It may have been made meanwhile.
    if (auto ready = cached (video, wanted))
    {
        report (1.0);
        return *ready;
    }

    // What an earlier preparation already made is kept. (Its silence only with its info: it's as
    // long as that says, and info from before may have said wrong.)
    if (auto earlier = readInfo (folder, video))
        prepared = *earlier;
    else
        folder.getChildFile ("silence.wav").deleteFile();

    juce::String error;
    const auto info = probe (video, error);

    if (! info)
        return fail (error);

    const auto seconds = prepared.info.seconds;
    prepared.info = *info;

    if (prepared.info.seconds <= 0.0)
        prepared.info.seconds = seconds;

    const auto soundFile = folder.getChildFile ("sound.wav");
    const auto silenceFile = folder.getChildFile ("silence.wav");
    const bool needSound = wanted.sound && prepared.info.hasSound && ! soundFile.existsAsFile();
    const bool needPictures = wanted.thumbnails && ! prepared.thumbnails.isValid();
    const auto soundShare = needSound ? (needPictures ? 0.25 : 1.0) : 0.0;
    double reached = 0.0;   // how long VLC said it is, reading it (or how far it read)

    // The sound: VLC writes it as it reads it, then it's lined up with the picture.
    if (needSound)
    {
        const auto raw = folder.getChildFile ("vlc-sound.wav");
        raw.deleteFile();

        const auto result = runVlc (video, { ":no-sout-video", ":no-sout-spu",
                                             ":sout=#transcode{acodec=fl32,channels=2}:std{access=file,mux=wav,dst=" + chainPath (raw) + "}" },
                                    [&] (double part) { report (soundShare * part); }, cancelled, &reached);

        if (result.failed())
        {
            raw.deleteFile();
            return fail (result.getErrorMessage());
        }

        double rate = prepared.info.soundRate, length = 0.0;
        const auto correction = Mp4::soundCorrection (video, rate > 0.0 ? rate : 48000.0);
        const auto part = folder.getChildFile ("sound.part.wav");
        auto lined = lineUpSound (raw, part, correction, rate, length);
        raw.deleteFile();

        if (lined.wasOk() && ! putInPlace (part, soundFile))
            lined = juce::Result::fail ("Couldn't write " + soundFile.getFullPathName());

        if (lined.failed())
        {
            part.deleteFile();
            soundFile.deleteFile();

            // VLC found no sound to read after all.
            if (lined.getErrorMessage() != "empty")
                return fail (lined.getErrorMessage());

            prepared.info.hasSound = false;

            if (prepared.info.seconds <= 0.0)
                prepared.info.seconds = reached;
        }
        else
        {
            prepared.info.soundRate = rate;

            if (prepared.info.seconds <= 0.0)
                prepared.info.seconds = length;
        }
    }

    if (wanted.sound && ! prepared.info.hasSound && wanted.silenceWhenNoSound && ! silenceFile.existsAsFile())
    {
        if (prepared.info.seconds <= 0.0)
            return fail ("VLC couldn't tell how long " + video.getFileName() + " is.");

        const auto part = folder.getChildFile ("silence.part.wav");

        if (const auto result = writeSilence (part, prepared.info.seconds); result.failed() || ! putInPlace (part, silenceFile))
        {
            part.deleteFile();
            return fail (result.failed() ? result.getErrorMessage() : "Couldn't write " + silenceFile.getFullPathName());
        }
    }

    if (cancelled != nullptr && cancelled->load())
        return fail ("Cancelled");

    // The small pictures: VLC decodes the video (lightly: they're small) and writes one every
    // so often as a JPEG.
    if (needPictures)
    {
        auto& thumbs = prepared.thumbnails;
        const auto length = juce::jmax (1.0, prepared.info.seconds);
        thumbs.every = juce::jmax (1.0, std::ceil (length / maxThumbnails * 10.0) / 10.0);
        thumbs.cellHeight = thumbnailHeight;
        const auto aspect = prepared.info.height > 0 ? static_cast<double> (prepared.info.width) / prepared.info.height : 16.0 / 9.0;
        thumbs.cellWidth = juce::jlimit (32, 240, static_cast<int> (std::lround (thumbnailHeight * aspect / 2.0)) * 2);
        thumbs.file = folder.getChildFile ("thumbnails.jpg");

        const auto frames = folder.getChildFile ("frames.jpgs");
        frames.deleteFile();

        const auto chain = juce::String ("#transcode{vcodec=jpeg,width=") + juce::String (thumbs.cellWidth)
                         + ",height=" + juce::String (thumbs.cellHeight)
                         + ",fps=" + juce::String (1.0 / thumbs.every, 6)
                         + "}:std{access=file,mux=dummy,dst=" + chainPath (frames) + "}";

        const auto result = runVlc (video, { ":no-sout-audio", ":no-sout-spu", ":avcodec-hw=none",
                                             ":avcodec-skip-frame=1", ":avcodec-skiploopfilter=4", ":sout=" + chain },
                                    [&] (double part) { report (soundShare + (1.0 - soundShare) * part); }, cancelled, &reached);

        if (result.failed())
        {
            frames.deleteFile();
            return fail (result.getErrorMessage());
        }

        const auto built = buildThumbnails (frames, thumbs);
        frames.deleteFile();

        if (built.failed())
            return fail (built.getErrorMessage());

        // Still not known: how many pictures came says it, unless there were as many as are kept
        // (they stop there); then what VLC said reading it.
        if (prepared.info.seconds <= 0.0)
            prepared.info.seconds = thumbs.count < maxThumbnails ? thumbs.count * thumbs.every : reached;

        if (prepared.info.seconds <= 0.0)
            return fail ("VLC couldn't tell how long " + video.getFileName() + " is.");
    }

    writeInfo (folder, prepared);
    report (1.0);

    if (auto ready = cached (video, wanted))
        return *ready;

    return fail ("Couldn't prepare " + video.getFileName() + ".");
}

//==============================================================================
class VideoMedia::Preparation::Worker final : public juce::Thread
{
public:
    Worker (const juce::File& v, Wanted w) : juce::Thread ("Sonify Video"), video (v), wanted (w) {}

    ~Worker() override
    {
        cancelled = true;
        stopThread (30000);
    }

    void run() override
    {
        auto outcome = prepareNow (video, wanted, &progress, &cancelled);

        {
            const juce::ScopedLock lock (resultLock);
            result = std::move (outcome);
        }

        done = true;
    }

    const juce::File video;
    const Wanted wanted;
    std::atomic<double> progress { 0.0 };
    std::atomic<bool> cancelled { false }, done { false };
    juce::CriticalSection resultLock;
    Prepared result;
};

VideoMedia::Preparation::Preparation (const juce::File& v, Wanted wanted)
    : video (v), worker (std::make_unique<Worker> (v, wanted))
{
    worker->startThread();
}

VideoMedia::Preparation::~Preparation() = default;

bool VideoMedia::Preparation::isDone() const noexcept      { return worker->done.load(); }
double VideoMedia::Preparation::getProgress() const noexcept { return worker->progress.load(); }
void VideoMedia::Preparation::cancel()                     { worker->cancelled = true; }

VideoMedia::Prepared VideoMedia::Preparation::getResult() const
{
    const juce::ScopedLock lock (worker->resultLock);
    return worker->result;
}

//==============================================================================
juce::Result VideoMedia::transcodePicture (const juce::File& video, const juce::File& target,
                                           const std::function<void (double)>& progress, std::atomic<bool>* cancelled)
{
    // VLC writes into the cache, where its chain can name the file; then it's moved. (Without
    // B-frames: with them, VLC's MP4 puts the picture a frame late.)
    const auto folder = cacheFolder();

    if (! chainCanWrite (folder) || ! folder.createDirectory())
        return juce::Result::fail ("VLC can't write into " + folder.getFullPathName() + ".");

    const auto temporary = folder.getNonexistentChildFile ("picture", ".mp4", false);
    const auto result = runVlc (video, { ":no-sout-audio", ":no-sout-spu", ":avcodec-hw=none",
                                         ":sout=#transcode{vcodec=h264,venc=x264{preset=medium,crf=19,bframes=0}}:std{access=file,mux=mp4,dst="
                                             + chainPath (temporary) + "}" },
                                progress, cancelled);

    if (result.failed() || ! temporary.existsAsFile() || temporary.getSize() == 0)
    {
        temporary.deleteFile();
        return result.failed() ? result : juce::Result::fail ("VLC couldn't write the video's picture as H.264.");
    }

    target.deleteFile();

    if (! temporary.moveFileTo (target) && ! (temporary.copyFileTo (target) && temporary.deleteFile()))
    {
        temporary.deleteFile();
        return juce::Result::fail ("Couldn't write " + target.getFullPathName());
    }

    return juce::Result::ok();
}

//==============================================================================
namespace
{
    // A picture bigger than this (5K, 8K) comes 4K-sized (4096 x 2304's worth of pixels).
    constexpr double largestPicturePixels = 4096.0 * 2304.0;

    // Screens smaller than HD still get HD pictures (an HD projector may be plugged in later).
    constexpr int leastScreenWidth = 1920, leastScreenHeight = 1080;

    // VLC gone wrong or stuck is started again this many times (then left: the file is at fault).
    constexpr int mostRestarts = 3;
}

/** The pictures VLC hands over. VLC writes each one straight into an image of a few kept here
    (one nobody holds), and it's handed on as it is: the one taken is never written again while
    anyone holds it (a window drawing it, the page's picture being made from it on a thread of
    its own), so nothing is copied on its way to the screen. */
struct VideoMedia::Player::Frames
{
    std::mutex lock;
    int width = 0, height = 0;
    std::vector<juce::Image> made;              // every picture made (written again once nobody holds one)
    juce::Image ready;                          // the newest picture, not yet taken (none: black)
    bool fresh = false;
    std::atomic<bool> shownAny { false };       // a picture has been handed over since the last black
    std::atomic<bool> fromMain { false };       // the main player's pictures are shown (it's moving)
    std::atomic<int> generation { 0 };          // stills asked for before this are too late
    std::function<void()> told;                 // a new picture's there (VLC's and the stills' threads)

    // VLC's thread only: the picture it writes now, and how far it's got with it.
    juce::Image writing;
    std::vector<juce::uint32> spill;            // written when every picture is held (that one's dropped)
    enum class Step { none, writing, done } step = Step::none;

    static constexpr size_t mostPictures = 8;

    /** A picture nobody else holds, made if there are fewer than the most (none: they're all
        held). Under the lock. */
    juce::Image freePicture()
    {
        for (const auto& image : made)
            if (image.getReferenceCount() == 1)
                return image;

        if (made.size() >= mostPictures)
            return {};

        made.emplace_back (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
        return made.back();
    }

    /** Each pixel made opaque: some of VLC's ways of making the picture leave the alpha byte
        empty, and a video with see-through parts brings its own. A picture that's opaque all over
        already (VLC filled it, or left it as it was) is only looked over. */
    static void makeOpaque (juce::Image& image)
    {
        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readWrite);
        int from = pixels.height;

        for (int y = 0; y < pixels.height; ++y)
        {
            const auto* line = reinterpret_cast<const juce::uint32*> (pixels.getLinePointer (y));
            auto all = 0xffffffffu;

            for (int x = 0; x < pixels.width; ++x)
                all &= line[x];

            if ((all >> 24) != 0xffu)
            {
                from = y;
                break;
            }
        }

        for (int y = from; y < pixels.height; ++y)
        {
            auto* line = reinterpret_cast<juce::uint32*> (pixels.getLinePointer (y));

            for (int x = 0; x < pixels.width; ++x)
                line[x] |= 0xff000000u;
        }
    }

    void tell() const
    {
        if (told)
            told();
    }

    static unsigned format (void** opaque, char* chroma, unsigned* width, unsigned* height, unsigned* pitches, unsigned* lines)
    {
        auto* self = static_cast<Frames*> (*opaque);
        std::memcpy (chroma, "RV32", 4);
        *width = static_cast<unsigned> (self->width);
        *height = static_cast<unsigned> (self->height);
        pitches[0] = *width * 4;
        lines[0] = *height;
        return 1;
    }

    static void* lockPicture (void* opaque, void** planes)
    {
        auto* self = static_cast<Frames*> (opaque);

        {
            const std::lock_guard<std::mutex> guard (self->lock);
            self->writing = self->freePicture();
        }

        self->step = Step::writing;

        if (self->writing.isValid())
        {
            const juce::Image::BitmapData pixels (self->writing, juce::Image::BitmapData::writeOnly);

            if (pixels.lineStride == self->width * 4)
            {
                planes[0] = pixels.data;
                return nullptr;
            }

            self->writing = {};
        }

        self->spill.resize (static_cast<size_t> (self->width) * static_cast<size_t> (self->height));
        planes[0] = self->spill.data();
        return nullptr;
    }

    // The picture's written (VLC 3 says so before it's due): it's made opaque now, not when it shows.
    static void unlockPicture (void* opaque, void*, void* const*)
    {
        auto* self = static_cast<Frames*> (opaque);

        if (self->step == Step::writing && self->writing.isValid())
            makeOpaque (self->writing);

        if (self->step == Step::writing)
            self->step = Step::done;
    }

    static void display (void* opaque, void*)
    {
        auto* self = static_cast<Frames*> (opaque);
        auto picture = std::move (self->writing);
        const auto written = self->step == Step::done;
        self->writing = {};
        self->step = Step::none;

        if (! picture.isValid() || ! self->fromMain.load())
            return;

        if (! written)
            makeOpaque (picture);

        {
            const std::lock_guard<std::mutex> guard (self->lock);

            if (! self->fromMain.load())
                return;

            self->ready = std::move (picture);
            self->fresh = true;
            self->shownAny = true;
        }

        self->tell();
    }

    /** A still picture (the stills' thread). One asked for while moving (nothing had come) never
        covers the moving pictures that came meanwhile. */
    void deliver (const std::vector<juce::uint32>& picture, int forGeneration)
    {
        juce::Image image;

        {
            const std::lock_guard<std::mutex> guard (lock);

            if (forGeneration != generation.load() || picture.size() != static_cast<size_t> (width) * static_cast<size_t> (height)
                || (fromMain.load() && shownAny.load()))
                return;

            image = freePicture();
        }

        if (! image.isValid())
            return;

        {
            const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::writeOnly);

            for (int y = 0; y < height; ++y)
            {
                auto* line = reinterpret_cast<juce::uint32*> (pixels.getLinePointer (y));
                const auto* from = picture.data() + static_cast<size_t> (y) * static_cast<size_t> (width);

                for (int x = 0; x < width; ++x)
                    line[x] = from[x] | 0xff000000u;
            }
        }

        {
            const std::lock_guard<std::mutex> guard (lock);

            if (forGeneration != generation.load() || (fromMain.load() && shownAny.load()))
                return;

            ready = std::move (image);
            fresh = true;
            shownAny = true;
        }

        tell();
    }

    /** Black, until the next picture. */
    void clear()
    {
        const std::lock_guard<std::mutex> guard (lock);
        ready = {};
        fresh = true;
        shownAny = false;
    }
};

//==============================================================================
/** Still pictures: VLC opened where the picture's wanted, its first picture taken, closed again. */
class VideoMedia::Player::Grabber final : public juce::Thread
{
public:
    Grabber (std::shared_ptr<Frames> f, const juce::File& v, double seconds, double framesPerSecond)
        : juce::Thread ("Sonify Video Still"), frames (std::move (f)), video (v), length (seconds),
          rate (framesPerSecond > 0.0 ? framesPerSecond : 25.0)
    {
        startThread();
    }

    ~Grabber() override
    {
        signalThreadShouldExit();
        wake.signal();
        stopThread (10000);
    }

    void request (double seconds, int generation)
    {
        {
            const std::lock_guard<std::mutex> guard (lock);
            wanted = seconds;
            wantedGeneration = generation;
            pending = true;
        }

        wake.signal();
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            wake.wait (-1);

            for (;;)
            {
                double seconds = 0.0;
                int generation = 0;

                {
                    const std::lock_guard<std::mutex> guard (lock);

                    if (! pending || threadShouldExit())
                        break;

                    pending = false;
                    seconds = wanted;
                    generation = wantedGeneration;
                }

                grab (seconds, generation);
            }
        }
    }

private:
    struct Still
    {
        int width = 0, height = 0;
        std::vector<juce::uint32> pixels, shown;
        std::mutex lock;
        std::atomic<int> got { 0 };
        std::atomic<double> firstAt { 0.0 };

        static unsigned format (void** opaque, char* chroma, unsigned* w, unsigned* h, unsigned* pitches, unsigned* lines)
        {
            auto* self = static_cast<Still*> (*opaque);
            std::memcpy (chroma, "RV32", 4);
            *w = static_cast<unsigned> (self->width);
            *h = static_cast<unsigned> (self->height);
            pitches[0] = *w * 4;
            lines[0] = *h;
            return 1;
        }

        static void* lockPicture (void* opaque, void** planes)
        {
            planes[0] = static_cast<Still*> (opaque)->pixels.data();
            return nullptr;
        }

        static void display (void* opaque, void*)
        {
            auto* self = static_cast<Still*> (opaque);
            const std::lock_guard<std::mutex> guard (self->lock);
            self->shown = self->pixels;

            if (self->got.load() == 0)
                self->firstAt = nowSeconds();

            ++self->got;
        }
    };

    void grab (double seconds, int generation)
    {
        if (! vlcReady())
            return;

        auto& api = vlc();

        // VLC starts at the first picture at or after the time it's given: the start of the
        // picture showing at that moment, a hair before.
        const auto frame = 1.0 / rate;
        seconds = juce::jlimit (0.0, juce::jmax (0.0, length - frame), seconds);
        seconds = juce::jmax (0.0, std::floor (seconds * rate + 1.0e-6) / rate - 0.002);

        auto* stillMedia = api.libvlc_media_new_path (api.instance, video.getFullPathName().toRawUTF8());

        if (stillMedia == nullptr)
            return;

        api.libvlc_media_add_option (stillMedia, ":no-audio");
        api.libvlc_media_add_option (stillMedia, ":no-spu");
        api.libvlc_media_add_option (stillMedia, (":start-time=" + juce::String (seconds, 3)).toRawUTF8());

        auto* stillPlayer = api.libvlc_media_player_new_from_media (stillMedia);
        api.libvlc_media_release (stillMedia);

        if (stillPlayer == nullptr)
            return;

        Still still;
        {
            const std::lock_guard<std::mutex> guard (frames->lock);
            still.width = frames->width;
            still.height = frames->height;
        }

        still.pixels.assign (static_cast<size_t> (still.width) * static_cast<size_t> (still.height), 0u);
        api.libvlc_video_set_callbacks (stillPlayer, Still::lockPicture, nullptr, Still::display, &still);
        api.libvlc_video_set_format_callbacks (stillPlayer, Still::format, nullptr);

        if (api.libvlc_media_player_play (stillPlayer) == 0)
        {
            const auto until = nowSeconds() + 3.0;

            while (still.got.load() == 0 && nowSeconds() < until && ! threadShouldExit())
            {
                const auto state = api.libvlc_media_player_get_state (stillPlayer);

                if (state == libvlc_Ended || state == libvlc_Error)
                    break;

                sleepMs (2);
            }

            // Some files first show the picture before (VLC's run-up to where it was asked to
            // start), the right one following straight after: the last of those comes.
            while (still.got.load() > 0 && still.got.load() < 2 && nowSeconds() < still.firstAt.load() + 0.03)
                sleepMs (2);
        }

        api.libvlc_media_player_stop (stillPlayer);
        api.libvlc_media_player_release (stillPlayer);

        if (still.got.load() > 0)
        {
            const std::lock_guard<std::mutex> guard (still.lock);
            frames->deliver (still.shown, generation);
        }
    }

    std::shared_ptr<Frames> frames;
    const juce::File video;
    const double length, rate;
    juce::WaitableEvent wake;
    std::mutex lock;
    double wanted = 0.0;
    int wantedGeneration = 0;
    bool pending = false;
};

//==============================================================================
VideoMedia::Player::Player() = default;

VideoMedia::Player::~Player()
{
    close();
}

bool VideoMedia::Player::open (const juce::File& video, const Info& info, juce::Point<int> biggestScreen)
{
    close();

    if (! vlcReady() || ! video.existsAsFile())
        return false;

    // Its picture as it's shown, its own size (its pixel shape applied), at its own frame rate,
    // but no bigger than the computer's biggest screen shows (HD at least): a 4K video on HD
    // screens comes HD-sized, as those screens would show it anyway, without carrying pixels
    // that would only be thrown away. Past 4K (5K, 8K) it comes 4K-sized: more would take more
    // memory and time than a show can spare.
    auto width = info.width > 0 ? static_cast<double> (info.width) : 1280.0;
    auto height = info.height > 0 ? static_cast<double> (info.height) : 720.0;
    auto scale = juce::jmin (1.0, std::sqrt (largestPicturePixels / (width * height)));

    if (biggestScreen.x > 0 && biggestScreen.y > 0)
        scale = juce::jmin (scale, juce::jmax (biggestScreen.x, leastScreenWidth) / width,
                                   juce::jmax (biggestScreen.y, leastScreenHeight) / height);

    outWidth = juce::jmax (16, static_cast<int> (std::lround (width * scale / 2.0)) * 2);
    outHeight = juce::jmax (16, static_cast<int> (std::lround (height * scale / 2.0)) * 2);
    fps = info.fps > 0.0 ? info.fps : 25.0;
    length = info.seconds;

    frames = std::make_shared<Frames>();
    frames->width = outWidth;
    frames->height = outHeight;
    frames->fresh = true;       // black, until its first picture
    frames->told = told;

    auto& api = vlc();
    auto* m = api.libvlc_media_new_path (api.instance, video.getFullPathName().toRawUTF8());

    if (m == nullptr)
        return false;

    for (const auto* option : { ":no-audio", ":no-spu", ":start-paused" })
        api.libvlc_media_add_option (m, option);

    auto* p = api.libvlc_media_player_new_from_media (m);
    api.libvlc_media_release (m);

    if (p == nullptr)
        return false;

    api.libvlc_video_set_callbacks (p, Frames::lockPicture, Frames::unlockPicture, Frames::display, frames.get());
    api.libvlc_video_set_format_callbacks (p, Frames::format, nullptr);

    if (api.libvlc_media_player_play (p) != 0)
    {
        api.libvlc_media_player_release (p);
        return false;
    }

    player = p;
    file = video;
    grabber = std::make_unique<Grabber> (frames, video, length, fps);
    moving = false;
    vlcRate = wantedRate = 1.0;
    shownStill = -1.0;
    anchorAt = -1.0;
    restarts = 0;
    restartAt = 0.0;
    restartedAt = -1.0e9;
    ranOut = false;
    return true;
}

void VideoMedia::Player::close()
{
    grabber.reset();

    if (player != nullptr)
    {
        auto& api = vlc();
        auto* p = static_cast<libvlc_media_player_t*> (player);
        api.libvlc_media_player_stop (p);
        api.libvlc_media_player_release (p);
        player = nullptr;
    }

    frames.reset();
    file = juce::File();
    moving = false;
}

void VideoMedia::Player::blank()
{
    if (frames == nullptr)
        return;

    ++frames->generation;
    frames->fromMain = false;

    if (moving)
        stopMoving();

    frames->clear();
    shownStill = -1.0e9;   // nothing until a still is asked for again
    ranOutStillAsked = false;
}

void VideoMedia::Player::setVlcRate (double rate)
{
    const auto now = nowSeconds();

    if (anchorAt >= 0.0)
    {
        anchorSeconds = estimate (now);
        anchorAt = now;
    }

    vlcRate = rate;
    vlc().libvlc_media_player_set_rate (static_cast<libvlc_media_player_t*> (player), static_cast<float> (rate));
}

double VideoMedia::Player::estimate (double now) const
{
    return anchorSeconds + (now - anchorAt) * vlcRate;
}

void VideoMedia::Player::startMoving (double seconds, double rate, double now)
{
    auto& api = vlc();
    auto* p = static_cast<libvlc_media_player_t*> (player);

    ++frames->generation;      // a still on its way is too late now
    frames->fromMain = true;
    moving = true;
    wantedRate = rate;

    if (std::abs (vlcRate - rate) > 1.0e-6)
    {
        vlcRate = rate;
        api.libvlc_media_player_set_rate (p, static_cast<float> (rate));
    }

    jumpedTo = juce::jmax (0.0, seconds + lead * rate);

    if (length > 0.0)
        jumpedTo = juce::jmin (jumpedTo, juce::jmax (0.0, length - 2.0 / fps));
    api.libvlc_media_player_set_time (p, static_cast<libvlc_time_t> (std::llround (jumpedTo * 1000.0)));
    api.libvlc_media_player_set_pause (p, 0);
    jumpedAt = now;
    vlcTimeMovedAt = now;
    firstStillAsked = false;
    anchorAt = -1.0;
    lastVlcTime = api.libvlc_media_player_get_time (p);
    learning = true;
}

void VideoMedia::Player::restartVlc (double now)
{
    // Opened again (paused at its start, as it was asked to open): follow() moves it on to where
    // it should be. Each time further apart: 2, 4, 8 seconds.
    auto& api = vlc();
    auto* p = static_cast<libvlc_media_player_t*> (player);
    ++restarts;
    restartedAt = now;
    restartAt = now + std::pow (2.0, static_cast<double> (restarts));
    ranOut = false;
    api.libvlc_media_player_stop (p);
    api.libvlc_media_player_play (p);
    moving = false;
}

void VideoMedia::Player::stopMoving()
{
    vlc().libvlc_media_player_set_pause (static_cast<libvlc_media_player_t*> (player), 1);
    moving = false;
    shownStill = -1.0;   // the picture where it stopped is fetched exactly
}

void VideoMedia::Player::follow (double seconds, bool shouldMove, double rate)
{
    if (player == nullptr || ! vlcReady())
        return;

    auto& api = vlc();
    auto* p = static_cast<libvlc_media_player_t*> (player);
    const auto now = nowSeconds();
    const auto state = api.libvlc_media_player_get_state (p);
    const auto frame = 1.0 / fps;
    rate = juce::jlimit (0.25, 4.0, rate);

    // Before the video: nothing. Past its end: its last picture, standing.
    if (seconds < 0.0)
    {
        if (shownStill > -1.0e8 || moving)
            blank();

        return;
    }

    if (length > 0.0 && seconds >= length - frame)
    {
        seconds = juce::jmax (0.0, length - frame);
        shouldMove = false;
    }

    // VLC ran to the end of its picture while the deck plays on (its sound longer than its picture,
    // a video that's one picture): the last picture stays (VLC isn't started again and again); one
    // that never showed a picture shows its first. A jump back starts VLC again.
    if (state == libvlc_Ended && shouldMove)
    {
        if (! ranOut)
        {
            ranOut = true;
            ranOutAt = seconds;
            ranOutStillAsked = false;
            moving = false;
        }

        if (seconds >= ranOutAt - 0.5)
        {
            // Nothing showing (none came, or it was blacked out meanwhile): the picture from just
            // before it ran out.
            if (! frames->shownAny.load() && ! ranOutStillAsked)
            {
                ranOutStillAsked = true;
                ++frames->generation;
                frames->fromMain = false;
                grabber->request (juce::jmax (0.0, ranOutAt - 0.5), frames->generation.load());
            }

            return;
        }
    }

    // VLC ran to the end (or was stopped): it's started again (paused at its start), ready to jump.
    if (state == libvlc_Ended || state == libvlc_Stopped)
    {
        ranOut = false;
        api.libvlc_media_player_stop (p);
        api.libvlc_media_player_play (p);
        moving = false;
        return;
    }

    // VLC went wrong (its decoder, the graphics card, the file for a moment): started again, a few
    // times at most, so the picture comes back by itself instead of staying black or frozen.
    if (state == libvlc_Error)
    {
        moving = false;

        if (restarts < mostRestarts && now >= restartAt)
            restartVlc (now);

        return;
    }

    if (shouldMove)
    {
        if (! moving)
        {
            if (state == libvlc_Playing || state == libvlc_Paused)
                startMoving (seconds, rate, now);

            return;
        }

        wantedRate = rate;
        const auto vlcTime = api.libvlc_media_player_get_time (p);

        // A new time from VLC (it says one every quarter second or so): where it is now.
        if (vlcTime != lastVlcTime)
        {
            lastVlcTime = vlcTime;
            vlcTimeMovedAt = now;

            // Going well again for a while: the next trouble may be met afresh.
            if (restarts > 0 && now - restartedAt > 20.0)
                restarts = 0;

            if (vlcTime >= 0 && now - jumpedAt > 0.2)
            {
                anchorSeconds = static_cast<double> (vlcTime) / 1000.0;
                anchorAt = now;

                // The first after a jump says how long VLC took to get there.
                if (learning)
                {
                    learning = false;
                    lead = juce::jlimit (0.0, 0.4, lead - 0.6 * (anchorSeconds - seconds));
                }
            }
        }

        // Moving, and no picture has come at all (a video that's one picture, or whose picture is
        // over before where it starts): its first picture, until moving ones come.
        if (! frames->shownAny.load() && ! firstStillAsked && now - jumpedAt > 0.75)
        {
            firstStillAsked = true;
            grabber->request (0.0, frames->generation.load());
        }

        // Its time standing still for seconds while it should play on (a decoder that stopped, the
        // graphics card reset under it): started again where it should be.
        if ((state == libvlc_Playing || state == libvlc_Paused) && now - jumpedAt > 3.0 && now - vlcTimeMovedAt > 3.0
            && restarts < mostRestarts && now >= restartAt)
        {
            restartVlc (now);
            return;
        }

        if (anchorAt < 0.0)
            return;

        const auto drift = estimate (now) - seconds;
        lastDrift = drift;

        if (std::abs (drift) > 0.25 && now - jumpedAt > 1.0)
        {
            startMoving (seconds, rate, now);
            return;
        }

        // A little faster or slower until it's back in step.
        const auto target = std::abs (drift) > 0.02 ? rate * (1.0 - juce::jlimit (-0.25, 0.25, drift * 1.2)) : rate;

        if (std::abs (target - vlcRate) > 0.002)
            setVlcRate (target);

        return;
    }

    if (moving)
        stopMoving();
    else if (state == libvlc_Playing)
        api.libvlc_media_player_set_pause (p, 1);

    // Standing still: the picture at that moment.
    if (std::abs (seconds - shownStill) > 0.5 * frame)
    {
        shownStill = seconds;
        ++frames->generation;
        frames->fromMain = false;
        grabber->request (seconds, frames->generation.load());
    }
}

bool VideoMedia::Player::takeFrame (juce::Image& image)
{
    if (frames == nullptr)
        return false;

    juce::Image taken;

    {
        const std::lock_guard<std::mutex> guard (frames->lock);

        if (! frames->fresh)
            return false;

        frames->fresh = false;
        taken = std::move (frames->ready);
        frames->ready = {};
    }

    image = std::move (taken);      // the last one goes back (outside the lock: VLC may want it)
    return true;
}

void VideoMedia::Player::setOnFrame (std::function<void()> callback)
{
    jassert (! isOpen());           // VLC's threads call it: it's set before a video opens
    told = std::move (callback);
}
