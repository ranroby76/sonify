// D:\workspace\Fanan_web_hub\src\AudioMedia.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

//==============================================================================
/**
    An audio file in memory: its samples at the device's sample rate, ready to play
    without touching the disk, and a peak summary of it for the page's waveforms.
    Shared by every clip that plays the file, and never changed once made.
*/
struct AudioData
{
    juce::AudioBuffer<float> samples;        // at playbackRate, one or two channels
    double playbackRate = 0.0;
    double fileRate = 0.0;
    double lengthSeconds = 0.0;

    static constexpr int peakBucket = 512;   // file samples per peak pair
    std::vector<juce::int8> peaks;           // max then min per bucket, -127..127, channels mixed
};

//==============================================================================
/**
    What an audio track's clips play, flattened for the audio thread the way a
    Sequence flattens MIDI clips: built on the message thread whenever the clips
    change, handed over whole, only ever read by the audio thread.

    Starts are song positions in quarter notes; offsets and lengths are samples of the
    file. The audio thread converts with the tempo of the moment, so audio keeps its
    speed and pitch whatever the tempo does.
*/
struct AudioPlaylist
{
    /** A clip's own automation line (Gain, Pan, an effect's parameter), in the file's samples (at
        the playback rate): straight lines between points, the first value before the first, the
        last after the last. */
    struct Curve
    {
        std::vector<juce::int64> at;         // sorted
        std::vector<float> values;

        /** Where to start looking for a sample (the point at or before it). */
        size_t find (juce::int64 sample) const noexcept
        {
            const auto after = std::upper_bound (at.begin(), at.end(), sample);
            return after == at.begin() ? 0 : static_cast<size_t> (after - at.begin()) - 1;
        }

        /** The value at a sample; hint: from find, moved on as the samples go forwards. */
        float valueAt (juce::int64 sample, size_t& hint) const noexcept
        {
            const auto count = at.size();

            if (count == 0)
                return 0.0f;

            if (sample <= at.front())
                return values.front();

            if (sample >= at.back())
                return values.back();

            while (hint + 1 < count && at[hint + 1] <= sample)
                ++hint;

            while (hint > 0 && at[hint] > sample)
                --hint;

            const auto span = at[hint + 1] - at[hint];
            return span > 0 ? values[hint] + (values[hint + 1] - values[hint]) * static_cast<float> (sample - at[hint]) / static_cast<float> (span)
                            : values[hint + 1];
        }
    };

    /** The clip's effect's parameters it automates, and what the audio thread last sent each. */
    struct ParamCurves
    {
        std::vector<juce::uint32> paramIds;
        std::vector<Curve> curves;
        mutable std::vector<double> lastSent;    // audio thread
        mutable std::vector<int> indexHints;     // audio thread
    };

    struct Item
    {
        juce::int64 startSample = 0;         // where it starts in the song's time (the tempo map's), in samples
        juce::int64 offsetSamples = 0;       // where in the file the clip's window starts
        juce::int64 lengthSamples = 0;       // how much of the file the window shows
        float gain = 1.0f;
        juce::int64 fadeInSamples = 0, fadeOutSamples = 0;
        std::shared_ptr<const AudioData> data;
        int effectSlot = 0;                  // its clip's own effect (Track::ClipEffectSlot::slotId); 0: none

        // Silence Areas: the stretches of the file (samples, sorted) it's silent in, ramping in and out.
        std::shared_ptr<const std::vector<std::pair<juce::int64, juce::int64>>> silence;
        juce::int64 silenceFadeSamples = 0;

        // The clip's own automation: Gain (dB) and Pan (-1..1) lines, and its effect's parameters.
        std::shared_ptr<const Curve> gainCurve, panCurve;
        std::shared_ptr<const ParamCurves> paramCurves;
    };

    std::vector<Item> items;
};

//==============================================================================
/** Loads audio files into memory once and shares them. Message thread only (the decoding itself
    may run on its own threads: prefetch, load).

    Files nothing plays any more (no clip, no preview holds them) are kept while they fit in a
    budget, the least recently used let go first, so opening a song again or undoing is quick
    but RAM doesn't fill with every file ever heard. */
class AudioMedia final
{
public:
    AudioMedia();
    ~AudioMedia();

    /** The file in memory at playbackRate (loaded now if it isn't yet), or null with a reason. */
    std::shared_ptr<const AudioData> get (const juce::File& file, double playbackRate, juce::String& error);

    /** Loads files not in memory yet, several at once on their own threads, and waits for them:
        a song's files before its clips ask for them one by one. */
    void prefetch (const juce::StringArray& paths, double playbackRate);

    /** The file in memory, loaded on its own thread when it isn't yet: done is called on the
        message thread (straight away when it's in memory), with null and a reason if it failed. */
    void load (const juce::File& file, double playbackRate,
               std::function<void (std::shared_ptr<const AudioData>, const juce::String& error)> done);

    /** Bytes kept for files nothing plays (the budget for them). */
    static constexpr size_t unusedBudget = 768u * 1024u * 1024u;

    /** Lets go of files nothing plays, the least recently used first, down to the budget. */
    void trim();

    /** How long a file plays, from memory when it's loaded; 0 when it can't be read. */
    double lengthOf (const juce::File& file, double playbackRate);

    /** Forgets everything, as when the device's sample rate changes. */
    void clear()                                            { cache.clear(); lastUsed.clear(); }
    void forget (const juce::File& file)                    { cache.erase (file.getFullPathName()); lastUsed.erase (file.getFullPathName()); }

    /** Files loaded since the last call: the page needs their waveforms. */
    std::vector<juce::String> takeNewlyLoaded()             { return std::exchange (newlyLoaded, {}); }

    juce::AudioFormatManager& getFormats() noexcept         { return formats; }

    /** A file the Hub can read as audio (WAV, AIFF, FLAC, Ogg; MP3 where the system has it). */
    bool isAudioFile (const juce::File& file);

    /** Peaks at a finer grain than AudioData::peaks, from the samples in memory: bucket is
        in played samples (at data.playbackRate). For zoomed-in waveforms. */
    static std::vector<juce::int8> peaksOf (const AudioData& data, int bucket);

private:
    /** Reads and prepares a file (any thread: the format manager only creates readers). */
    static std::shared_ptr<AudioData> decode (juce::AudioFormatManager& formats, const juce::File& file, double playbackRate, juce::String& error);
    void keep (const juce::String& key, std::shared_ptr<const AudioData> data);
    void touch (const juce::String& key)                    { lastUsed[key] = ++useClock; }

    juce::AudioFormatManager formats;
    std::map<juce::String, std::shared_ptr<const AudioData>> cache;
    std::map<juce::String, juce::uint64> lastUsed;
    juce::uint64 useClock = 0;
    std::vector<juce::String> newlyLoaded;

    juce::ThreadPool loaders { juce::ThreadPoolOptions().withThreadName ("Sonify audio loader")
                                                        .withNumberOfThreads (juce::jlimit (2, 8, juce::SystemStats::getNumCpus() - 1)) };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioMedia)
};
