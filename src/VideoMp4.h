// D:\workspace\Fanan_web_hub\src\VideoMp4.h

#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

//==============================================================================
/**
    MP4 (and QuickTime .mov) files, read and written by Sonify itself, for Export Video: the
    picture of the song's video goes into the new file exactly as it was (no frame lost or made
    again), with the song's mix as its sound (MP3, which every player of MP4 plays). A video
    whose picture can't go into an MP4 as it is (a WebM, an AVI...) is first written as H.264 by
    VLC, then put together the same way.

    Reading also tells how a file's sound is meant to line up with its picture (its edit list),
    which preparing a video needs.
*/
namespace Mp4
{
    constexpr juce::uint32 fourcc (const char (&text)[5]) noexcept
    {
        return (static_cast<juce::uint32> (static_cast<unsigned char> (text[0])) << 24)
             | (static_cast<juce::uint32> (static_cast<unsigned char> (text[1])) << 16)
             | (static_cast<juce::uint32> (static_cast<unsigned char> (text[2])) << 8)
             |  static_cast<juce::uint32> (static_cast<unsigned char> (text[3]));
    }

    struct Edit
    {
        juce::int64 segmentDuration = 0;   // in the movie's timescale
        juce::int64 mediaTime = 0;         // in the track's timescale; -1: an empty edit (a gap)
        juce::int16 rateInteger = 1, rateFraction = 0;
    };

    struct Sample
    {
        juce::int64 offset = 0;            // in the file
        juce::uint32 size = 0;
        juce::uint32 duration = 0;         // in the track's timescale
        juce::int32 composition = 0;       // presentation time minus decoding time
        bool sync = true;
    };

    struct Track
    {
        juce::uint32 id = 0;
        juce::uint32 handler = 0;          // 'vide', 'soun', ...
        juce::uint32 timescale = 0;
        juce::uint64 mediaDuration = 0;    // in its timescale
        juce::uint16 language = 0x55c4;    // packed ISO-639-2 ("und")
        juce::uint32 codec = 0;            // the sample entry's kind ('avc1', 'mp4a', ...)
        int objectType = 0;                // 'mp4a': the stream's object type (0x40 AAC, 0x6b MP3, ...)
        std::vector<Edit> edits;
        juce::MemoryBlock trackHeader;     // 'tkhd', the whole box
        juce::MemoryBlock sampleEntry;     // the first sample description, the whole box
        std::vector<Sample> samples;       // read only when asked for
        bool hasSyncTable = false;
    };

    struct Movie
    {
        juce::uint32 timescale = 0;
        juce::uint64 duration = 0;
        bool fragmented = false;           // its samples are in fragments (none in the tables)
        std::vector<Track> tracks;

        const Track* first (juce::uint32 handler) const noexcept;
    };

    /** An MP4's movie (nothing: not an MP4 or QuickTime file, or it can't be read). */
    std::optional<Movie> read (const juce::File& file, bool withSamples);

    /** Whether a picture of this kind can go into an MP4 as it is. */
    bool canCopyPicture (juce::uint32 codec) noexcept;

    /** How many samples (at the sound's own rate) VLC's reading of this file's sound needs in
        front of it (negative: taken off its start) to start where the picture starts. 0 when it
        reads it right, or it can't be told. */
    int soundCorrection (const juce::File& video, double soundRate);

    //==============================================================================
    /** An MP3's frames, as they are in its file (its tag frame left out). */
    struct Mp3Frames
    {
        int sampleRate = 0, channels = 0, samplesPerFrame = 0, objectType = 0;
        std::vector<juce::int64> offsets;
        std::vector<juce::uint32> sizes;
    };

    std::optional<Mp3Frames> readMp3 (const juce::File& file);

    /** An MP4 of a movie's picture (its first video track, copied as it is) and an MP3's frames,
        starting together. */
    juce::Result write (const juce::File& target, const juce::File& pictureFile, const Movie& picture,
                        const juce::File& mp3File, const Mp3Frames& sound,
                        const std::function<void (double)>& progress, std::atomic<bool>* cancelled);

    //==============================================================================
    /**
        Export Video on a thread of its own: a video's picture and a mix (a WAV the length of the
        video, its first sample where the picture starts) into a new MP4.
    */
    class Export final
    {
    public:
        Export (const juce::File& video, const juce::File& mix, const juce::File& target);
        ~Export();

        bool isDone() const noexcept;
        double getProgress() const noexcept;
        juce::Result getResult() const;      // when done
        void cancel();

        /** The work, on the thread calling it (the tests call it). */
        static juce::Result run (const juce::File& video, const juce::File& mix, const juce::File& target,
                                 std::atomic<double>* progress, std::atomic<bool>* cancelled);

    private:
        class Worker;
        std::unique_ptr<Worker> worker;

        JUCE_DECLARE_NON_COPYABLE (Export)
    };
}
