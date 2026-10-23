// D:\workspace\Fanan_web_hub\src\VideoSound.cpp

#include "VideoSound.h"
#include "LiveStream.h"     // LiveStream::problem(): whether FFmpeg's DLLs are there

#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <vector>

#if FANAN_FFMPEG
extern "C"
{
 #include <libavcodec/avcodec.h>
 #include <libavformat/avformat.h>
 #include <libavutil/channel_layout.h>
 #include <libswresample/swresample.h>
}
#endif

bool VideoSound::available()
{
#if FANAN_FFMPEG
    return LiveStream::problem().isEmpty();
#else
    return false;
#endif
}

#if FANAN_FFMPEG
namespace
{
    /** A video without sound reads as silence, one channel at this rate. */
    constexpr double silenceRate = 8000.0;

    /** The furthest the sound is moved to line it up with the picture (further: its times are wrong). */
    constexpr double longestShift = 600.0;

    constexpr double notKnown = std::numeric_limits<double>::quiet_NaN();

    void quietFFmpeg()
    {
        static std::once_flag once;
        std::call_once (once, [] { av_log_set_level (AV_LOG_ERROR); });
    }

    //==============================================================================
    /** FFmpeg's side: the file open, its sound's decoder, and the sound decoded and not handed
        out yet (floats, one or two channels, at the rate of its first frame). */
    class Decoder final
    {
    public:
        Decoder() = default;
        ~Decoder()      { close(); }

        /** Opens the video and decodes up to its first sound. False: FFmpeg can't read it, or it's
            a kind VLC lines up itself. A video without sound is opened (info.hasSound false) when
            it says how long it is. soundOnly: an audio file (or anything with sound), no picture
            looked for, every kind FFmpeg reads; one without sound isn't opened. */
        bool open (const juce::File& file, VideoMedia::Info& info, bool soundOnly = false)
        {
            close();
            quietFFmpeg();

            // "file:" first: a name with a colon in it is never taken for a protocol.
            const auto url = "file:" + file.getFullPathName();

            if (avformat_open_input (&format, url.toRawUTF8(), nullptr, nullptr) < 0)
                return false;

            // Transport and program streams, and Ogg: their times start anywhere, and VLC shows
            // their picture by its own reading of them (VLC prepares their sound to match).
            if (! soundOnly && (format->iformat->flags & AVFMT_TS_DISCONT) != 0)
                return false;

            if (avformat_find_stream_info (format, nullptr) < 0)
                return false;

            if (soundOnly)
                return openSound (info, -1, true);

            // The picture (a cover's still picture isn't one).
            const auto picture = av_find_best_stream (format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

            if (picture < 0 || (format->streams[picture]->disposition & AV_DISPOSITION_ATTACHED_PIC) != 0)
                return false;

            auto* seen = format->streams[picture];
            const auto* look = seen->codecpar;

            if (look->width <= 0 || look->height <= 0)
                return false;

            // As it's shown: its pixels' own shape applied.
            auto width = static_cast<double> (look->width);
            const auto pixel = av_guess_sample_aspect_ratio (format, seen, nullptr);

            if (pixel.num > 0 && pixel.den > 0)
                width = width * pixel.num / pixel.den;

            info.width = static_cast<int> (std::lround (width));
            info.height = look->height;

            const auto frameRate = av_guess_frame_rate (format, seen, nullptr);
            info.fps = frameRate.num > 0 && frameRate.den > 0 ? av_q2d (frameRate) : 0.0;

            if (info.fps <= 0.0 || info.fps > 300.0)
                info.fps = 25.0;

            if (seen->start_time != AV_NOPTS_VALUE)
                pictureStart = static_cast<double> (seen->start_time) * av_q2d (seen->time_base);

            if (format->duration != AV_NOPTS_VALUE && format->duration > 0)
                info.seconds = static_cast<double> (format->duration) / AV_TIME_BASE;
            else if (seen->duration != AV_NOPTS_VALUE && seen->duration > 0)
                info.seconds = static_cast<double> (seen->duration) * av_q2d (seen->time_base);

            return openSound (info, picture, false);
        }

        /** Its sound (the one that goes with the picture, when there's one) ready to decode, and
            decoded up to its first sound. */
        bool openSound (VideoMedia::Info& info, int picture, bool soundOnly)
        {
            if (soundOnly && format->duration != AV_NOPTS_VALUE && format->duration > 0)
                info.seconds = static_cast<double> (format->duration) / AV_TIME_BASE;

            const AVCodec* decoder = nullptr;
            const auto found = av_find_best_stream (format, AVMEDIA_TYPE_AUDIO, -1, picture, &decoder, 0);

            if (found == AVERROR_STREAM_NOT_FOUND)
            {
                if (soundOnly)
                    return false;

                // A sound FFmpeg couldn't make out (its rate or channels not found where it looked)
                // is still a sound: VLC may read it.
                for (unsigned i = 0; i < format->nb_streams; ++i)
                    if (format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
                        return false;

                info.hasSound = false;
                return info.seconds > 0.0;   // silence as long as it is
            }

            if (found < 0 || decoder == nullptr)
                return false;                // a sound FFmpeg can't decode: VLC may

            stream = found;
            auto* sound = format->streams[stream];
            codec = avcodec_alloc_context3 (decoder);

            if (codec == nullptr || avcodec_parameters_to_context (codec, sound->codecpar) < 0)
                return false;

            // With its stream's time base, the decoder leaves out the priming the file marks (and
            // says so in the frames' times).
            codec->pkt_timebase = sound->time_base;

            if (avcodec_open2 (codec, decoder, nullptr) < 0)
                return false;

            // Only the sound is read: the rest is passed over (MP4's picture isn't even read).
            for (unsigned i = 0; i < format->nb_streams; ++i)
                format->streams[i]->discard = static_cast<int> (i) == stream ? AVDISCARD_DEFAULT : AVDISCARD_ALL;

            packet = av_packet_alloc();
            frame = av_frame_alloc();
            held = av_frame_alloc();

            if (packet == nullptr || frame == nullptr || held == nullptr)
                return false;

            timeBase = av_q2d (sound->time_base);
            info.hasSound = true;

            // AVI's times are made up as it's read (from how much has come): no holes to fill there.
            // Nor in an audio file: no picture to keep up with, its sound one stretch after another.
            fillsHoles = ! soundOnly && ! juce::String (format->iformat->name).containsIgnoreCase ("avi");

            // Its first sound: its real rate and channels (some say otherwise until they're
            // decoded), and where it starts.
            if (! decodeMore())
            {
                // Nothing came out of it after all: silence as long as it is.
                info.hasSound = false;
                return info.seconds > 0.0;
            }

            info.soundRate = outRate;
            return true;
        }

        void close()
        {
            avcodec_free_context (&codec);
            swr_free (&swr);
            av_packet_free (&packet);
            av_frame_free (&frame);
            av_frame_free (&held);
            avformat_close_input (&format);
            av_channel_layout_uninit (&inLayout);
            stream = -1;
            inRate = 0;
            inFormat = -1;
            outRate = 0;
            outChannels = 0;
            draining = finished = started = false;
            fillsHoles = true;
            failures = 0;
            produced = 0;
            holeLeft = 0;
            firstTicks = AV_NOPTS_VALUE;
            pictureStart = notKnown;

            for (auto& channel : out)
                channel.clear();

            outRead = 0;
        }

        double getRate() const noexcept            { return static_cast<double> (outRate); }
        int getChannels() const noexcept           { return outChannels; }

        /** Where its first sound is (seconds), as the file's times say (not known: NaN). */
        double getFirstSeconds() const noexcept
        {
            return firstTicks != AV_NOPTS_VALUE ? static_cast<double> (firstTicks) * timeBase : notKnown;
        }

        /** The video's time 0 in the file's times: 0 (a picture a little later than the sound is
            how the file keeps them together), or for a file cut out of a longer stream (its times
            going on from there), where its picture starts. */
        double getOrigin() const noexcept
        {
            if (format != nullptr && format->start_time != AV_NOPTS_VALUE)
                if (const auto start = static_cast<double> (format->start_time) / AV_TIME_BASE; start > 0.5)
                    return std::isfinite (pictureStart) ? pictureStart : start;

            return 0.0;
        }

        /** Where its sound ends (seconds, in the file's times; not known: NaN). */
        double getSoundEnd() const noexcept
        {
            if (format == nullptr || stream < 0)
                return notKnown;

            const auto* sound = format->streams[stream];

            if (sound->duration != AV_NOPTS_VALUE && sound->duration > 0)
            {
                const auto start = sound->start_time != AV_NOPTS_VALUE ? sound->start_time
                                 : firstTicks != AV_NOPTS_VALUE ? firstTicks : 0;
                return (static_cast<double> (start) + static_cast<double> (sound->duration)) * timeBase;
            }

            if (format->duration != AV_NOPTS_VALUE && format->duration > 0)
            {
                const auto start = format->start_time != AV_NOPTS_VALUE ? format->start_time : 0;
                return (static_cast<double> (start) + static_cast<double> (format->duration)) / AV_TIME_BASE;
            }

            return notKnown;
        }

        /** The sound decoded and not handed out yet. */
        int available() const noexcept            { return static_cast<int> (out[0].size() - outRead); }

        const float* data (int channel) const noexcept
        {
            return out[static_cast<size_t> (juce::jlimit (0, juce::jmax (0, outChannels - 1), channel))].data() + outRead;
        }

        void consume (int count) noexcept
        {
            outRead += static_cast<size_t> (juce::jlimit (0, available(), count));

            // What's been handed out goes, now and then.
            if (outRead >= 65536 && outRead * 2 >= out[0].size())
            {
                for (int c = 0; c < outChannels; ++c)
                {
                    auto& channel = out[static_cast<size_t> (c)];
                    channel.erase (channel.begin(), channel.begin() + static_cast<std::ptrdiff_t> (outRead));
                }

                outRead = 0;
            }
        }

        /** Decodes more of the sound. False: there's no more. */
        bool decodeMore()
        {
            if (codec == nullptr)
                return false;

            // A hole in the sound: its silence a second at a time, then the frame after it.
            if (holeLeft > 0)
            {
                const auto count = juce::jmin<juce::int64> (holeLeft, outRate);
                addSilence (count);
                holeLeft -= count;
                return true;
            }

            if (held != nullptr && held->nb_samples > 0)
            {
                const auto added = convert (held, false);
                av_frame_unref (held);

                if (added > 0)
                    return true;
            }

            if (finished)
                return false;

            for (;;)
            {
                const auto received = avcodec_receive_frame (codec, frame);

                if (received == 0)
                {
                    failures = 0;
                    const auto added = convert (frame, true);
                    av_frame_unref (frame);

                    if (added > 0)
                        return true;

                    if (holeLeft > 0)
                        return decodeMore();   // the hole before it: its silence first

                    continue;
                }

                // A frame it couldn't decode is passed over (a decoder failing over and over
                // without wanting more of the file is at its end).
                if (received != AVERROR (EAGAIN) && received != AVERROR_EOF && ! draining && ++failures < 100)
                    continue;

                if (received != AVERROR (EAGAIN) || draining)
                {
                    // Its end: what the converter still holds.
                    finished = true;
                    return drain() > 0;
                }

                if (av_read_frame (format, packet) < 0)
                {
                    draining = true;
                    avcodec_send_packet (codec, nullptr);
                    continue;
                }

                // A damaged packet is passed over.
                failures = 0;

                if (packet->stream_index == stream)
                    avcodec_send_packet (codec, packet);

                av_packet_unref (packet);
            }
        }

    private:
        /** A frame into floats at the first frame's rate, one or two channels. Returns how many came
            (none for a frame after a hole: it's held, and the hole's silence comes first). */
        int convert (AVFrame* decoded, bool mayFollowHole)
        {
            const auto channels = decoded->ch_layout.nb_channels;

            if (decoded->nb_samples <= 0 || channels <= 0 || decoded->sample_rate <= 0)
                return 0;

            if (! started)
            {
                started = true;
                firstTicks = decoded->pts != AV_NOPTS_VALUE ? decoded->pts : decoded->best_effort_timestamp;
                outRate = decoded->sample_rate;
                outChannels = juce::jlimit (1, 2, channels);
            }
            else if (mayFollowHole && fillsHoles && decoded->pts != AV_NOPTS_VALUE && firstTicks != AV_NOPTS_VALUE)
            {
                // A stretch the file has no sound for (frames lost in a recording): silence there,
                // so what comes after stays with its picture. The frame waits for it.
                const auto due = static_cast<double> (decoded->pts - firstTicks) * timeBase * outRate;
                const auto missing = static_cast<juce::int64> (std::llround (due)) - produced;

                if (missing > outRate / 4 && missing < static_cast<juce::int64> (outRate) * 600)
                {
                    holeLeft = missing;
                    av_frame_unref (held);
                    av_frame_move_ref (held, decoded);
                    return 0;
                }
            }

            if (! fitConverter (decoded))
                return 0;

            const auto room = swr_get_out_samples (swr, decoded->nb_samples);

            if (room <= 0)
                return 0;

            const auto made = swr_convert (swr, planes (room), room, const_cast<const uint8_t**> (decoded->extended_data), decoded->nb_samples);
            return keep (made);
        }

        /** What the converter holds back, at the end. */
        int drain()
        {
            if (swr == nullptr)
                return 0;

            const auto room = swr_get_out_samples (swr, 0);

            if (room <= 0)
                return 0;

            return keep (swr_convert (swr, planes (room), room, nullptr, 0));
        }

        uint8_t** planes (int room)
        {
            for (int c = 0; c < 2; ++c)
            {
                auto& plane = scratch[static_cast<size_t> (c)];

                if (static_cast<int> (plane.size()) < room)
                    plane.resize (static_cast<size_t> (room));

                planePointers[static_cast<size_t> (c)] = reinterpret_cast<uint8_t*> (plane.data());
            }

            return planePointers.data();
        }

        int keep (int made)
        {
            if (made <= 0)
                return 0;

            for (int c = 0; c < outChannels; ++c)
            {
                const auto* from = scratch[static_cast<size_t> (c)].data();
                auto& to = out[static_cast<size_t> (c)];
                to.insert (to.end(), from, from + made);
            }

            produced += made;
            return made;
        }

        void addSilence (juce::int64 count)
        {
            for (int c = 0; c < outChannels; ++c)
            {
                auto& to = out[static_cast<size_t> (c)];
                to.insert (to.end(), static_cast<size_t> (count), 0.0f);
            }

            produced += count;
        }

        /** The converter, set for this frame's kind of sound (set again when it changes). */
        bool fitConverter (const AVFrame* decoded)
        {
            AVChannelLayout layout {};

            if (decoded->ch_layout.order != AV_CHANNEL_ORDER_UNSPEC)
                av_channel_layout_copy (&layout, &decoded->ch_layout);
            else
                av_channel_layout_default (&layout, decoded->ch_layout.nb_channels);

            if (swr != nullptr && decoded->sample_rate == inRate && decoded->format == inFormat
                && av_channel_layout_compare (&layout, &inLayout) == 0)
            {
                av_channel_layout_uninit (&layout);
                return true;
            }

            swr_free (&swr);
            AVChannelLayout wanted {};
            av_channel_layout_default (&wanted, outChannels);

            const bool made = swr_alloc_set_opts2 (&swr, &wanted, AV_SAMPLE_FMT_FLTP, outRate, &layout,
                                                   static_cast<AVSampleFormat> (decoded->format), decoded->sample_rate, 0, nullptr) >= 0
                              && swr_init (swr) >= 0;
            av_channel_layout_uninit (&wanted);

            if (! made)
            {
                swr_free (&swr);
                av_channel_layout_uninit (&layout);
                return false;
            }

            av_channel_layout_uninit (&inLayout);
            inLayout = layout;   // kept (its own now)
            inRate = decoded->sample_rate;
            inFormat = decoded->format;
            return true;
        }

        AVFormatContext* format = nullptr;
        AVCodecContext* codec = nullptr;
        SwrContext* swr = nullptr;
        AVPacket* packet = nullptr;
        AVFrame* frame = nullptr;
        AVFrame* held = nullptr;                // the frame after a hole, while its silence goes out
        int stream = -1;
        double timeBase = 0.0;
        bool draining = false, finished = false, started = false;
        bool fillsHoles = true;                 // silence where the file has no sound (its times say)
        int failures = 0;                       // frames it couldn't decode since it last read the file
        juce::int64 produced = 0;               // samples made since its first sound (holes filled too)
        juce::int64 holeLeft = 0;               // a hole's silence still to come
        juce::int64 firstTicks = AV_NOPTS_VALUE;
        double pictureStart = notKnown;         // seconds, in the file's times

        // The converter's input as it was set up, and what it makes.
        AVChannelLayout inLayout {};
        int inRate = 0, inFormat = -1;
        int outRate = 0, outChannels = 0;

        std::array<std::vector<float>, 2> scratch, out;
        std::array<uint8_t*, 2> planePointers {};
        size_t outRead = 0;
    };

    //==============================================================================
    /** The sound as JUCE reads it: from its start on (a read further on decodes up to there; one
        before where it is opens the file again). */
    class Reader final : public juce::AudioFormatReader
    {
    public:
        explicit Reader (const juce::File& video)
            : juce::AudioFormatReader (nullptr, "Video sound"), file (video)
        {
            usesFloatingPointData = true;
            bitsPerSample = 32;
        }

        /** Opens it: its rate, channels and length, and how it lines up with the picture. */
        bool start (VideoMedia::Info& info)
        {
            if (! decoder.open (file, info))
                return false;

            if (! info.hasSound)
            {
                silent = true;
                sampleRate = silenceRate;
                numChannels = 1;
                lengthInSamples = static_cast<juce::int64> (std::ceil (info.seconds * silenceRate));
                decoder.close();
                return lengthInSamples > 0;
            }

            sampleRate = decoder.getRate();
            numChannels = static_cast<unsigned int> (decoder.getChannels());

            // Where its first sound is against the picture's time 0: silence in front of a later
            // start, an earlier one's beginning left out.
            const auto origin = decoder.getOrigin();
            const auto first = decoder.getFirstSeconds();
            auto shift = std::isfinite (first) ? first - origin : 0.0;

            if (std::abs (shift) > longestShift)
                shift = 0.0;

            startShift = static_cast<juce::int64> (std::llround (shift * sampleRate));

            const auto end = decoder.getSoundEnd();

            if (! std::isfinite (end) || end - origin <= 0.0)
                return false;

            lengthInSamples = static_cast<juce::int64> (std::llround ((end - origin) * sampleRate));
            fromTheStart();
            return lengthInSamples > 0;
        }

        bool readSamples (int* const* destChannels, int numDestChannels, int startOffsetInDestBuffer,
                          juce::int64 startSampleInFile, int numSamples) override
        {
            clearSamplesBeyondAvailableLength (destChannels, numDestChannels, startOffsetInDestBuffer,
                                               startSampleInFile, numSamples, lengthInSamples);

            if (numSamples <= 0)
                return true;

            // Before where it is: from its start again.
            if (startSampleInFile < position && ! reopen())
            {
                clear (destChannels, numDestChannels, startOffsetInDestBuffer, numSamples);
                return true;
            }

            skip (startSampleInFile - position);

            for (int done = 0; done < numSamples;)
            {
                const auto count = take (destChannels, numDestChannels, startOffsetInDestBuffer + done, numSamples - done);

                if (count <= 0)
                {
                    // Its sound ended a little before the length said: silence.
                    clear (destChannels, numDestChannels, startOffsetInDestBuffer + done, numSamples - done);
                    position += numSamples - done;
                    break;
                }

                done += count;
            }

            return true;
        }

    private:
        void fromTheStart() noexcept
        {
            position = 0;
            padLeft = juce::jmax<juce::int64> (0, startShift);
            dropLeft = juce::jmax<juce::int64> (0, -startShift);
        }

        bool reopen()
        {
            if (silent)
            {
                position = 0;
                return true;
            }

            VideoMedia::Info again;

            if (! decoder.open (file, again) || ! again.hasSound)
                return false;

            fromTheStart();
            return true;
        }

        /** Decoded sound ready to hand out (the start left out first). False: no more. */
        bool fill()
        {
            for (;;)
            {
                if (dropLeft > 0 && decoder.available() > 0)
                {
                    const auto dropped = static_cast<int> (juce::jmin<juce::int64> (dropLeft, decoder.available()));
                    decoder.consume (dropped);
                    dropLeft -= dropped;
                    continue;
                }

                if (dropLeft == 0 && decoder.available() > 0)
                    return true;

                if (! decoder.decodeMore())
                    return false;
            }
        }

        static void clear (int* const* dest, int numDest, int at, int count)
        {
            for (int c = 0; c < numDest; ++c)
                if (dest[c] != nullptr)
                    juce::FloatVectorOperations::clear (reinterpret_cast<float*> (dest[c]) + at, count);
        }

        /** Up to wanted samples into dest: the silence in front first, then the sound. 0: no more. */
        int take (int* const* dest, int numDest, int at, int wanted)
        {
            if (silent || padLeft > 0)
            {
                const auto count = silent ? wanted : static_cast<int> (juce::jmin<juce::int64> (padLeft, wanted));
                clear (dest, numDest, at, count);
                padLeft -= silent ? 0 : count;
                position += count;
                return count;
            }

            if (! fill())
                return 0;

            const auto count = juce::jmin (wanted, decoder.available());

            for (int c = 0; c < numDest; ++c)
                if (dest[c] != nullptr)
                    juce::FloatVectorOperations::copy (reinterpret_cast<float*> (dest[c]) + at, decoder.data (c), count);

            decoder.consume (count);
            position += count;
            return count;
        }

        /** Passes over this many samples. */
        void skip (juce::int64 count)
        {
            while (count > 0)
            {
                if (silent)
                {
                    position += count;
                    return;
                }

                if (padLeft > 0)
                {
                    const auto passed = juce::jmin (padLeft, count);
                    padLeft -= passed;
                    position += passed;
                    count -= passed;
                    continue;
                }

                if (! fill())
                {
                    position += count;   // past its end: silence from here
                    return;
                }

                const auto passed = static_cast<int> (juce::jmin<juce::int64> (count, decoder.available()));
                decoder.consume (passed);
                position += passed;
                count -= passed;
            }
        }

        const juce::File file;
        Decoder decoder;
        bool silent = false;
        juce::int64 startShift = 0;             // samples: + silence in front, - left out of its start
        juce::int64 position = 0;               // where the next sample handed out is
        juce::int64 padLeft = 0, dropLeft = 0;
    };
}
#endif

bool VideoSound::soundToWav (const juce::File& source, const juce::File& wav, juce::String& error, const std::function<bool()>& keepGoing)
{
    if (! source.existsAsFile())
    {
        error = source.getFileName() + " isn't there.";
        return false;
    }

#if FANAN_FFMPEG
    if (! available())
    {
        error = "this Sonify can't read " + source.getFileExtension().toUpperCase().trimCharactersAtStart (".") + " files (FFmpeg is missing next to it).";
        return false;
    }

    Decoder decoder;
    VideoMedia::Info info;

    if (! decoder.open (source, info, true) || ! info.hasSound || decoder.getRate() <= 0.0 || decoder.getChannels() <= 0)
    {
        error = "it isn't a sound Sonify can read.";
        return false;
    }

    wav.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = wav.createOutputStream();

    if (stream == nullptr)
    {
        error = "couldn't write " + wav.getFullPathName() + ".";
        return false;
    }

    const auto channels = decoder.getChannels();
    auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                                      .withSampleRate (decoder.getRate())
                                                                      .withNumChannels (channels)
                                                                      .withBitsPerSample (32)
                                                                      .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));

    if (writer == nullptr)
    {
        error = "couldn't write " + wav.getFullPathName() + ".";
        return false;
    }

    juce::int64 written = 0;

    for (;;)
    {
        if (keepGoing != nullptr && ! keepGoing())
        {
            writer.reset();
            wav.deleteFile();
            error = "Cancelled";
            return false;
        }

        if (const auto count = decoder.available(); count > 0)
        {
            const float* planes[2] = { decoder.data (0), decoder.data (1) };

            if (! writer->writeFromFloatArrays (planes, channels, count))
            {
                writer.reset();
                wav.deleteFile();
                error = "couldn't write " + wav.getFullPathName() + ".";
                return false;
            }

            written += count;
            decoder.consume (count);
            continue;
        }

        if (! decoder.decodeMore())
            break;
    }

    writer.reset();

    if (written <= 0)
    {
        wav.deleteFile();
        error = "there's no sound in it.";
        return false;
    }

    return true;
#else
    juce::ignoreUnused (wav, keepGoing);
    error = "this Sonify can't read " + source.getFileExtension().toUpperCase().trimCharactersAtStart (".") + " files.";
    return false;
#endif
}

VideoSound::Opened VideoSound::open (const juce::File& video)
{
    Opened opened;

    if (! video.existsAsFile())
    {
        opened.error = video.getFullPathName() + " isn't there.";
        return opened;
    }

#if FANAN_FFMPEG
    if (! available())
        return opened;

    auto reader = std::make_unique<Reader> (video);

    if (reader->start (opened.info))
        opened.reader = std::move (reader);
    else
        opened.info = {};
#endif

    return opened;
}
