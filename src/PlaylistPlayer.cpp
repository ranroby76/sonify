// D:\workspace\Fanan_web_hub\src\PlaylistPlayer.cpp

#include "PlaylistPlayer.h"
#include "BuiltinPlugins.h"

#if __has_include (<signalsmith-stretch/signalsmith-stretch.h>)
 #include <signalsmith-stretch/signalsmith-stretch.h>
 #define SONIFY_PLAYLIST_STRETCH 1
#else
 #define SONIFY_PLAYLIST_STRETCH 0
#endif

#include <algorithm>
#include <cmath>

//==============================================================================
namespace
{
    std::function<juce::File()>& userFolder()
    {
        static std::function<juce::File()> folder;
        return folder;
    }

    Playlists::ScreenFactory& screenFactory()
    {
        static Playlists::ScreenFactory factory;
        return factory;
    }

    juce::File settingsFile()
    {
        return Playlists::folder().getChildFile ("Playlist Player.json");
    }

    bool isUsablePath (const juce::String& path)
    {
        return path.isNotEmpty() && juce::File::isAbsolutePath (path);
    }

    /** A file as it is now: a changed file is read again. */
    juce::String identityOf (const juce::File& file)
    {
        return file.getFullPathName() + "|" + juce::String (file.getSize()) + "|" + juce::String (file.getLastModificationTime().toMilliseconds());
    }

    /** What a video song needs: its sound (silence as long as it, when it has none). */
    VideoMedia::Wanted videoSound()
    {
        VideoMedia::Wanted wanted;
        wanted.sound = true;
        wanted.silenceWhenNoSound = true;
        wanted.thumbnails = false;
        return wanted;
    }

    float number (const juce::var& value, float fallback, float low, float high)
    {
        return value.isVoid() ? fallback : juce::jlimit (low, high, static_cast<float> (static_cast<double> (value)));
    }

    int whole (const juce::var& value, int low, int high)
    {
        return value.isVoid() ? low : juce::jlimit (low, high, static_cast<int> (value));
    }

    // How long the ear needs to not hear a click: pausing, stopping, seeking, playing another.
    constexpr int pauseFadeMs = 10;
    constexpr int stopFadeMs = 15;
    constexpr int switchFadeMs = 20;

    // How long a mixdown waits for a song to be read, and for the player's upkeep on the message
    // thread (it's never this slow unless something's wrong).
    constexpr juce::uint32 offlineWaitMs = 10000;
    constexpr juce::uint32 messageWaitMs = 2000;
}

//==============================================================================
Plugin::Descriptor Playlists::describe()
{
    Plugin::Descriptor descriptor;
    descriptor.id = playerId;
    descriptor.name = "Playlist Player";
    descriptor.vendor = "Sonify";
    descriptor.version = "1.0";
    descriptor.features = { "instrument", "player", "stereo" };
    return descriptor;
}

void Playlists::setUserFolder (std::function<juce::File()> userData)
{
    userFolder() = std::move (userData);
}

juce::File Playlists::folder()
{
    const auto base = userFolder() != nullptr ? userFolder()()
                                              : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sonify");
    return base.getChildFile ("Playlists");
}

juce::File Playlists::mediaFolder()
{
    const auto settings = juce::JSON::parse (settingsFile().loadFileAsString());
    const auto path = settings.getProperty ("mediaFolder", {}).toString();

    if (isUsablePath (path) && juce::File (path).isDirectory())
        return juce::File (path);

    return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
}

bool Playlists::setMediaFolder (const juce::File& chosen)
{
    if (! chosen.isDirectory() || ! folder().createDirectory())
        return false;

    auto* settings = new juce::DynamicObject();
    settings->setProperty ("mediaFolder", chosen.getFullPathName());
    return settingsFile().replaceWithText (juce::JSON::toString (juce::var (settings)));
}

void Playlists::setScreenFactory (ScreenFactory factory)
{
    screenFactory() = std::move (factory);
}

//==============================================================================
/** The audio thread's working space: one per player, used by each of its songs in turn. */
struct PlaylistPlayer::Scratch
{
    static constexpr int chunk = 128;               // output samples worked out at a time
    static constexpr int inputFrames = 1 << 17;     // file samples at a time, at most

    juce::AudioBuffer<float> input { 2, inputFrames };
    juce::AudioBuffer<float> output { 2, chunk };
};

//==============================================================================
/**
    One song as it plays: its file read ahead into a ring on the player's thread, played out of
    it on the audio thread at the song's speed and pitch (through Signalsmith Stretch, or
    resampled to the device's rate when it plays as it is), with its volume and a fade.

    Where it starts is fixed: seeking makes a new one, which takes over once it's read ahead.
    A song that starts at another speed or pitch has its stretcher readied on the player's
    thread before it plays, so the audio thread never does that work all at once.
*/
class PlaylistPlayer::Stream final : public juce::TimeSliceClient
{
public:
    Stream (juce::uint32 serialNumber, const PlaylistSong& song, std::unique_ptr<juce::AudioFormatReader> source,
            juce::int64 startAt, Start how, juce::TimeSliceThread& thread, double deviceRate)
        : serial (serialNumber),
          songId (song.id),
          start (how),
         #if SONIFY_PLAYLIST_STRETCH
          prime (needsStretch (song)),
         #else
          prime (false),
         #endif
          fileRate (source->sampleRate),
          length (source->lengthInSamples),
          startSample (juce::jlimit<juce::int64> (0, juce::jmax<juce::int64> (0, source->lengthInSamples - 1), startAt)),
          reader (std::move (source)),
          readThread (thread),
          capacity (juce::jmax (1 << 17, juce::nextPowerOfTwo (static_cast<int> (fileRate * 3.0)))),
          fifo (capacity),
          ring (2, capacity),
          readBuffer (2, readChunk),
          readyFrames (static_cast<int> (juce::jmin (capacity / 2.0, fileRate * 0.4))),
          readPosition (startSample)
    {
        update (song);
        gain = juce::jlimit (Playlists::minVolume, Playlists::maxVolume, song.volume);   // at its level from its first sample

       #if SONIFY_PLAYLIST_STRETCH
        stretcher.presetDefault (2, static_cast<float> (fileRate), true);   // its work spread evenly over the blocks

        if (prime)
        {
            const auto ratio = fileRate / (deviceRate > 0.0 ? deviceRate : fileRate);
            primeTranspose = transposeFor (song.pitch, ratio);
            primeLength = stretcher.outputSeekLength (static_cast<float> (ratio * song.speed));
            primeBuffer.setSize (2, juce::jmax (1, primeLength));
        }
       #else
        juce::ignoreUnused (deviceRate);
       #endif
    }

    /** Whether a song plays through the stretcher (another speed or pitch). */
    static bool needsStretch (const PlaylistSong& song) noexcept
    {
        return std::abs (song.pitch) > 0.001f || std::abs (song.speed - 1.0f) > 0.0005f;
    }

    const juce::uint32 serial, songId;
    const Start start;
    const bool prime;                 // stretched from its start, readied on the player's thread
    const double fileRate;
    const juce::int64 length, startSample;

    // The song's settings, as the message thread changes them.
    std::atomic<float> volume { 1.0f }, pitch { 0.0f }, speed { 1.0f };
    std::atomic<int> waitMs { 0 }, crossfadeMs { 0 };

    /** Whether the audio thread may start stretching it itself when its speed or pitch moves
        (the player turns that off when it brings a readied one in instead). */
    std::atomic<bool> lateStretch { true };

    void update (const PlaylistSong& song) noexcept
    {
        volume.store (song.volume);
        pitch.store (song.pitch);
        speed.store (song.speed);
        waitMs.store (song.waitMs);
        crossfadeMs.store (song.crossfadeMs);
    }

    //==============================================================================
    // The player's thread: reading ahead.
    int useTimeSlice() override
    {
       #if SONIFY_PLAYLIST_STRETCH
        if (prime && ! primed.load())
        {
            primeStretcher();
            primed.store (true);   // after the stretcher and its counts: the audio thread's from here
            return 0;
        }
       #endif

        if (endReached.load())
            return -1;

        const auto room = fifo.getFreeSpace();

        if (room < readChunk / 4)
            return 10;

        const auto count = static_cast<int> (juce::jmin<juce::int64> (juce::jmin (room, readChunk), length - readPosition));

        if (count <= 0 || ! reader->read (&readBuffer, 0, count, readPosition, true, true))
        {
            endReached.store (true);
            return -1;
        }

        int start1, size1, start2, size2;
        fifo.prepareToWrite (count, start1, size1, start2, size2);

        for (int channel = 0; channel < 2; ++channel)
        {
            if (size1 > 0) ring.copyFrom (channel, start1, readBuffer, channel, 0, size1);
            if (size2 > 0) ring.copyFrom (channel, start2, readBuffer, channel, size1, size2);
        }

        fifo.finishedWrite (size1 + size2);
        readPosition += size1 + size2;

        if (readPosition >= length)
        {
            endReached.store (true);   // after the samples: whoever sees it sees them all
            return -1;
        }

        return 0;
    }

    //==============================================================================
    // The audio thread.
    bool isReady() const noexcept
    {
        return (! prime || primed.load()) && (endReached.load() || fifo.getNumReady() >= readyFrames);
    }

    bool hasEnded() const noexcept          { return ended; }
    bool isSilent() const noexcept          { return fade <= 0.0f && fadeTarget <= 0.0f; }
    float getFade() const noexcept          { return fade; }

    /** How many samples its fade still has to go. */
    int getFadeLeft() const noexcept
    {
        return fadeStep != 0.0f ? static_cast<int> (std::ceil ((fadeTarget - fade) / fadeStep)) : 0;
    }
    double getLengthSeconds() const noexcept { return static_cast<double> (length) / fileRate; }

    /** Waits for it to be read ahead (a mixdown only: playing live, it never waits). */
    bool waitUntilReady (bool offline) noexcept
    {
        if (offline)
            waitFor (readyFrames);

        return isReady();
    }

    void setFade (float level) noexcept
    {
        fade = fadeTarget = level;
        fadeStep = 0.0f;
    }

    void fadeTo (float target, int samples) noexcept
    {
        fadeTarget = target;

        if (samples <= 0)
        {
            fade = target;
            fadeStep = 0.0f;
        }
        else
        {
            fadeStep = (target - fade) / static_cast<float> (samples);
        }
    }

    /** Where it plays, in file samples (what's in the stretcher not heard yet left out). */
    double getPositionSamples (double deviceRate) const noexcept
    {
        auto at = static_cast<double> (startSample + consumed + zerosFed);

       #if SONIFY_PLAYLIST_STRETCH
        if (stretching)
            at -= stretcher.inputLatency() + rateFor (deviceRate) * stretcher.outputLatency();
       #endif

        return juce::jlimit (0.0, static_cast<double> (length), at);
    }

    double getPosition (double deviceRate) const noexcept
    {
        return getPositionSamples (deviceRate) / fileRate;
    }

    /** How long it plays from where it is, and as a whole, in output samples at its speed now. */
    juce::int64 getOutputLeft (double deviceRate) const noexcept
    {
        return static_cast<juce::int64> ((static_cast<double> (length) - getPositionSamples (deviceRate)) / rateFor (deviceRate));
    }

    juce::int64 getOutputLength (double deviceRate) const noexcept
    {
        return static_cast<juce::int64> (static_cast<double> (length - startSample) / rateFor (deviceRate));
    }

    /** Adds up to numSamples of it to the outputs; fewer when it ends, or when it isn't read that
        far yet (live playing never waits for the disk: the rest is silent). */
    int render (float* outLeft, float* outRight, int numSamples, Scratch& scratch, double deviceRate, bool offline) noexcept
    {
        int done = 0;

        while (done < numSamples && ! ended)
        {
            const auto count = juce::jmin (Scratch::chunk, numSamples - done);
            auto* left = scratch.output.getWritePointer (0);
            auto* right = scratch.output.getWritePointer (1);
            const auto made = produce (left, right, count, scratch, deviceRate, offline);

            // Its volume, smoothed across the chunk, and its fade, sample by sample.
            const auto target = juce::jlimit (Playlists::minVolume, Playlists::maxVolume, volume.load (std::memory_order_relaxed));
            const auto gainStep = made > 0 ? (target - gain) / static_cast<float> (made) : 0.0f;

            for (int i = 0; i < made; ++i)
            {
                gain += gainStep;

                if (fadeStep != 0.0f)
                {
                    fade += fadeStep;

                    if ((fadeStep > 0.0f && fade >= fadeTarget) || (fadeStep < 0.0f && fade <= fadeTarget))
                    {
                        fade = fadeTarget;
                        fadeStep = 0.0f;
                    }
                }

                const auto level = gain * fade;
                outLeft[done + i] += left[i] * level;
                outRight[done + i] += right[i] * level;
            }

            if (made > 0)
                gain = target;

            done += made;

            if (made < count)
                break;
        }

        return done;
    }

private:
    static constexpr int readChunk = 16384;

    double rateFor (double deviceRate) const noexcept
    {
        // File samples per output sample.
        return fileRate / deviceRate * juce::jlimit (Playlists::minSpeed, Playlists::maxSpeed, speed.load (std::memory_order_relaxed));
    }

    void waitFor (int frames) noexcept
    {
        const auto until = juce::Time::getMillisecondCounter() + offlineWaitMs;

        while (((prime && ! primed.load()) || (fifo.getNumReady() < frames && ! endReached.load()))
               && juce::Time::getMillisecondCounter() < until)
        {
            readThread.notify();
            juce::Thread::sleep (1);
        }
    }

    struct Have
    {
        int frames;
        bool all;      // nothing more to read after these
    };

    Have have (int wanted, bool offline) noexcept
    {
        if (offline)
            waitFor (wanted);

        const auto all = endReached.load();   // first: if it's the end, every sample is in
        return { fifo.getNumReady(), all };
    }

    void peek (Scratch& scratch, int frames) noexcept
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (frames, start1, size1, start2, size2);

        for (int channel = 0; channel < 2; ++channel)
        {
            if (size1 > 0) scratch.input.copyFrom (channel, 0, ring, channel, start1, size1);
            if (size2 > 0) scratch.input.copyFrom (channel, size1, ring, channel, start2, size2);
        }
    }

    void consume (int frames) noexcept
    {
        fifo.finishedRead (frames);
        consumed += frames;
    }

    void endIfDone (bool all) noexcept
    {
        if (all && fifo.getNumReady() == 0)
            ended = true;
    }

    int produce (float* left, float* right, int count, Scratch& scratch, double deviceRate, bool offline) noexcept
    {
        auto ratio = fileRate / deviceRate;
        const auto speedNow = juce::jlimit (Playlists::minSpeed, Playlists::maxSpeed, speed.load (std::memory_order_relaxed));
        const auto pitchNow = juce::jlimit (Playlists::minPitch, Playlists::maxPitch, pitch.load (std::memory_order_relaxed));

       #if SONIFY_PLAYLIST_STRETCH
        // Its speed or pitch moved: through the stretcher from here on (for the rest of it).
        if (! stretching && lateStretch.load (std::memory_order_relaxed)
            && (std::abs (pitchNow) > 0.001f || std::abs (speedNow - 1.0f) > 0.0005f))
            startStretching (scratch, ratio, speedNow, pitchNow, offline);

        if (stretching)
            return stretchChunk (left, right, count, scratch, ratio, speedNow, pitchNow, offline);
       #else
        ratio *= speedNow;   // no stretcher in this build: faster is higher
        juce::ignoreUnused (pitchNow);
       #endif

        if (std::abs (ratio - 1.0) < 1.0e-9)
            return copyChunk (left, right, count, offline);

        return resampleChunk (left, right, count, scratch, ratio, offline);
    }

    int copyChunk (float* left, float* right, int count, bool offline) noexcept
    {
        const auto available = have (count, offline);

        if (available.frames < count && ! available.all)
            return 0;

        const auto frames = juce::jmin (count, available.frames);
        int start1, size1, start2, size2;
        fifo.prepareToRead (frames, start1, size1, start2, size2);

        for (int channel = 0; channel < 2; ++channel)
        {
            auto* out = channel == 0 ? left : right;
            const auto* in = ring.getReadPointer (channel);

            if (size1 > 0) juce::FloatVectorOperations::copy (out, in + start1, size1);
            if (size2 > 0) juce::FloatVectorOperations::copy (out + size1, in + start2, size2);
        }

        consume (size1 + size2);
        endIfDone (available.all);
        return size1 + size2;
    }

    int resampleChunk (float* left, float* right, int count, Scratch& scratch, double ratio, bool offline) noexcept
    {
        const auto needed = static_cast<int> (std::ceil (count * ratio)) + 2;

        if (needed > Scratch::inputFrames)
            return 0;

        const auto available = have (needed, offline);

        if (available.frames < needed && ! available.all)
            return 0;

        const auto frames = juce::jmin (needed, available.frames);
        peek (scratch, frames);
        int used = 0;

        for (int channel = 0; channel < 2; ++channel)
            used = interpolators[(size_t) channel].process (ratio, scratch.input.getReadPointer (channel),
                                                            channel == 0 ? left : right, count, frames, 0);

        consume (juce::jlimit (0, frames, used));
        endIfDone (available.all);
        return count;
    }

   #if SONIFY_PLAYLIST_STRETCH
    static float transposeFor (float semitones, double ratio) noexcept
    {
        // The pitch asked for, and the file's own rate brought to the device's.
        return static_cast<float> (std::pow (2.0, semitones / 12.0) * ratio);
    }

    /** The player's thread, before the song plays: the stretcher seeded with its first samples,
        so its first output is the song's first sample (what startStretching does on the audio
        thread for a song whose speed or pitch moves while it plays). */
    void primeStretcher()
    {
        const auto frames = static_cast<int> (juce::jlimit<juce::int64> (0, primeLength, length - readPosition));
        primeBuffer.clear();

        if (frames > 0 && ! reader->read (&primeBuffer, 0, frames, readPosition, true, true))
            primeBuffer.clear();

        float* inputs[2] = { primeBuffer.getWritePointer (0), primeBuffer.getWritePointer (1) };
        lastTranspose = primeTranspose;
        stretcher.setTransposeFactor (primeTranspose);
        stretcher.outputSeek (inputs, primeLength);

        readPosition += frames;
        consumed = frames;
        zerosFed = primeLength - frames;
        stretching = true;
        debt = 0.0;

        if (readPosition >= length)
            endReached.store (true);
    }

    void startStretching (Scratch& scratch, double ratio, float speedNow, float pitchNow, bool offline) noexcept
    {
        const auto rate = ratio * speedNow;
        const auto seekLength = stretcher.outputSeekLength (static_cast<float> (rate));

        if (seekLength <= 0 || seekLength > juce::jmin (Scratch::inputFrames, capacity / 2))
            return;

        const auto available = have (seekLength, offline);

        if (available.frames < seekLength && ! available.all)
            return;   // not read that far yet: as it is for now

        const auto frames = juce::jmin (seekLength, available.frames);
        peek (scratch, frames);

        for (int channel = 0; channel < 2; ++channel)
            juce::FloatVectorOperations::clear (scratch.input.getWritePointer (channel, frames), seekLength - frames);

        consume (frames);
        zerosFed += seekLength - frames;

        lastTranspose = transposeFor (pitchNow, ratio);
        stretcher.setTransposeFactor (lastTranspose);

        float* inputs[2] = { scratch.input.getWritePointer (0), scratch.input.getWritePointer (1) };
        stretcher.outputSeek (inputs, seekLength);   // its next output is the next sample
        stretching = true;
        debt = 0.0;
    }

    int stretchChunk (float* left, float* right, int count, Scratch& scratch, double ratio, float speedNow, float pitchNow, bool offline) noexcept
    {
        const auto rate = ratio * speedNow;
        const auto transpose = transposeFor (pitchNow, ratio);

        if (std::abs (transpose - lastTranspose) > 1.0e-6f)
        {
            stretcher.setTransposeFactor (transpose);
            lastTranspose = transpose;
        }

        const auto owed = debt + count * rate;
        const auto take = juce::jmin (Scratch::inputFrames, static_cast<int> (owed));
        const auto available = have (take, offline);

        if (available.frames < take && ! available.all)
            return 0;

        debt = owed - take;
        const auto frames = juce::jmin (take, available.frames);
        peek (scratch, frames);

        for (int channel = 0; channel < 2; ++channel)
            juce::FloatVectorOperations::clear (scratch.input.getWritePointer (channel, frames), take - frames);

        consume (frames);
        zerosFed += take - frames;

        float* inputs[2] = { scratch.input.getWritePointer (0), scratch.input.getWritePointer (1) };
        float* outputs[2] = { left, right };
        stretcher.process (inputs, take, outputs, count);

        // Past the end, the stretcher is fed silence until all it held has played.
        const auto flush = stretcher.inputLatency() + static_cast<int> (std::ceil (rate * stretcher.outputLatency()));

        if (available.all && fifo.getNumReady() == 0 && zerosFed >= flush)
            ended = true;

        return count;
    }
   #endif

    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::TimeSliceThread& readThread;
    const int capacity;
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> ring, readBuffer;
    const int readyFrames;
    juce::int64 readPosition;             // the player's thread's
    std::atomic<bool> endReached { false };

    // The audio thread's
    juce::int64 consumed = 0, zerosFed = 0;
    bool ended = false, stretching = false;
    float gain = 1.0f, fade = 1.0f, fadeTarget = 1.0f, fadeStep = 0.0f;
    double debt = 0.0;
    std::array<juce::LagrangeInterpolator, 2> interpolators;   // as Sonify resamples its audio files

   #if SONIFY_PLAYLIST_STRETCH
    signalsmith::stretch::SignalsmithStretch<float> stretcher { 12345 };
    float lastTranspose = 1.0f, primeTranspose = 1.0f;
    int primeLength = 0;
    juce::AudioBuffer<float> primeBuffer;
   #endif
    std::atomic<bool> primed { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Stream)
};

//==============================================================================
const std::vector<Plugin::Parameter> PlaylistPlayer::noParameters;

PlaylistPlayer::PlaylistPlayer()
    : descriptor (Playlists::describe())
{
    inFile.push_back (descriptor);
    formats.registerBasicFormats();
    readThread.startThread (juce::Thread::Priority::normal);
    startTimer (30);
}

PlaylistPlayer::~PlaylistPlayer()
{
    stopTimer();
    screen.reset();

    for (auto& job : preparations)
        job.second->cancel();

    preparations.clear();
    running.store (false);
    dropAllStreams();
    readThread.stopThread (4000);
}

const juce::File& PlaylistPlayer::getSourceFile() const noexcept
{
    return Builtins::file();
}

//==============================================================================
// The playlist
int PlaylistPlayer::indexOf (juce::uint32 songId) const noexcept
{
    if (songId == 0)
        return -1;

    for (size_t i = 0; i < songs.size(); ++i)
        if (songs[i].id == songId)
            return static_cast<int> (i);

    return -1;
}

PlaylistSong* PlaylistPlayer::songById (juce::uint32 songId) noexcept
{
    const auto index = indexOf (songId);
    return index >= 0 ? &songs[(size_t) index] : nullptr;
}

juce::uint32 PlaylistPlayer::nextPlayableAfter (juce::uint32 songId) const
{
    const auto index = indexOf (songId);

    if (index < 0)
        return 0;

    for (auto i = static_cast<size_t> (index) + 1; i < songs.size(); ++i)
        if (! songs[i].missing)
            return songs[i].id;

    return 0;
}

void PlaylistPlayer::readSong (PlaylistSong& song)
{
    if (song.title.isEmpty() && isUsablePath (song.file))
        song.title = juce::File (song.file).getFileNameWithoutExtension();

    // A video: its sound, from the video cache, or read now (on a thread of its own) and
    // playable once it's there.
    if (isUsablePath (song.file) && VideoMedia::isVideoFile (juce::File (song.file)))
    {
        song.video = true;
        song.problem.clear();
        const juce::File video (song.file);

        if (auto ready = VideoMedia::cached (video, videoSound()))
        {
            song.sound = ready->sound.getFullPathName();
            song.preparing = false;
            song.missing = ! ready->sound.existsAsFile();
            return;
        }

        song.sound.clear();
        song.missing = true;

        // Read once already, and VLC couldn't: not again until the file changes.
        if (song.failedFor.isNotEmpty() && song.failedFor == identityOf (video))
        {
            song.preparing = false;
            return;
        }

        song.failedFor.clear();
        song.preparing = video.existsAsFile() && VideoMedia::problem().isEmpty();

        if (! video.existsAsFile())
            song.problem = video.getFileName() + " isn't there.";
        else if (! song.preparing)
            song.problem = VideoMedia::problem();
        else if (preparations.find (song.file) == preparations.end())
            preparations[song.file] = std::make_unique<VideoMedia::Preparation> (video, videoSound());

        return;
    }

    // Whether it's there and a kind Sonify reads; whether it really reads shows as it plays.
    song.video = false;
    song.missing = ! isUsablePath (song.file)
                   || ! juce::File (song.file).existsAsFile()
                   || formats.findFormatForFileExtension (juce::File (song.file).getFileExtension()) == nullptr;
}

void PlaylistPlayer::updatePreparations()
{
    for (auto it = preparations.begin(); it != preparations.end();)
    {
        if (! it->second->isDone())
        {
            ++it;
            continue;
        }

        const auto result = it->second->getResult();

        for (auto& song : songs)
        {
            if (song.file != it->first || ! song.video)
                continue;

            song.preparing = false;
            song.sound = result.ok() ? result.sound.getFullPathName() : juce::String();
            song.missing = ! result.ok() || ! result.sound.existsAsFile();
            song.problem = result.ok() ? juce::String() : result.error;
            song.failedFor = result.ok() || result.error == "Cancelled" ? juce::String() : identityOf (juce::File (song.file));
        }

        it = preparations.erase (it);
        songsChanged();
    }
}

void PlaylistPlayer::dropUnusedPreparations()
{
    for (auto it = preparations.begin(); it != preparations.end();)
    {
        const auto used = std::any_of (songs.begin(), songs.end(), [&it] (const PlaylistSong& song) { return song.file == it->first; });

        if (used)
        {
            ++it;
            continue;
        }

        it->second->cancel();
        it = preparations.erase (it);
    }
}

double PlaylistPlayer::getPreparation (int index) const
{
    if (index < 0 || index >= static_cast<int> (songs.size()) || ! songs[(size_t) index].preparing)
        return -1.0;

    const auto found = preparations.find (songs[(size_t) index].file);
    return found != preparations.end() ? found->second->getProgress() : 0.0;
}

void PlaylistPlayer::recheckMissing()
{
    // A drive plugged in after the song was opened: its songs play again.
    bool found = false;

    for (auto& song : songs)
    {
        if (! song.missing || song.preparing || ! isUsablePath (song.file) || ! juce::File (song.file).existsAsFile())
            continue;

        if (song.video)
        {
            if (song.failedFor.isNotEmpty() && song.failedFor == identityOf (juce::File (song.file)))
                continue;       // VLC couldn't read it: not again until the file changes

            readSong (song);    // read now, if it wasn't
            found = found || ! song.missing || song.preparing;
            continue;
        }

        if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (song.file))); reader != nullptr)
        {
            song.missing = false;
            found = true;
        }
    }

    if (found && onSongsChanged != nullptr)
        onSongsChanged();
}

void PlaylistPlayer::songsChanged()
{
    stateChanged.store (true);

    if (onSongsChanged != nullptr)
        onSongsChanged();
}

void PlaylistPlayer::setSelectedId (juce::uint32 id)
{
    selectedId = id;
    selectedShared.store (id);
}

void PlaylistPlayer::settingsChanged (int index)
{
    stateChanged.store (true);
    const auto& song = songs[(size_t) index];

    for (auto& stream : streams)
        if (stream->songId == song.id)
            stream->update (song);
}

PlaylistPlayer::Stream* PlaylistPlayer::findStream (juce::uint32 serial) const noexcept
{
    for (const auto& stream : streams)
        if (stream->serial == serial)
            return stream.get();

    return nullptr;
}

void PlaylistPlayer::readyStretchFor (int index)
{
   #if SONIFY_PLAYLIST_STRETCH
    // A song's speed or pitch moved off its own: what's read of it for playing goes through the
    // stretcher, readied on the player's thread (the audio thread would do it all at once).
    const auto& song = songs[(size_t) index];

    if (! Stream::needsStretch (song))
        return;   // back to its own: it stays stretched until it's played again

    const auto shown = static_cast<State> (shownState.load());
    auto* playing = findStream (activeSerial.load());
    auto* posted = postedActive != activeSerial.load() ? findStream (postedActive) : nullptr;
    const auto readyInFlight = posted != nullptr && posted->songId == song.id && posted->prime;

    if (playing != nullptr && playing->songId == song.id && ! playing->prime && ! readyInFlight)
    {
        const auto length = shownLength.load();
        const auto at = length > 0.0 ? juce::jlimit (0.0, 1.0, shownPosition.load() / length) : 0.0;

        if (shown == State::stopped)
        {
            if (auto* stream = makeStream (index, at, Start::cue))
                postActive (stream);
        }
        else if (auto* stream = makeStream (index, at, Start::seek))
        {
            playing->lateStretch.store (false);   // it plays on as it is until the readied one takes over
            postActive (stream);
        }
    }

    // Auto-Play's next one is made again, readied.
    if (postedUpcomingSong == song.id)
        if (auto* next = findStream (postedUpcoming); next != nullptr && ! next->prime)
            postedUpcoming = 0;
   #else
    juce::ignoreUnused (index);
   #endif
}

int PlaylistPlayer::addSongs (const juce::Array<juce::File>& files, int insertBefore)
{
    std::vector<PlaylistSong> added;

    for (const auto& file : files)
    {
        if (! file.existsAsFile() || (formats.findFormatForFileExtension (file.getFileExtension()) == nullptr
                                      && ! VideoMedia::isVideoFile (file)))
            continue;

        PlaylistSong song;
        song.file = file.getFullPathName();
        song.title = file.getFileNameWithoutExtension();
        song.id = nextSongId++;
        readSong (song);
        added.push_back (song);
    }

    if (added.empty())
        return 0;

    const auto at = insertBefore >= 0 && insertBefore < static_cast<int> (songs.size()) ? insertBefore : static_cast<int> (songs.size());
    songs.insert (songs.begin() + at, added.begin(), added.end());

    if (selectedId == 0)
        setSelectedId (songs.front().id);

    songsChanged();
    return static_cast<int> (added.size());
}

void PlaylistPlayer::dropPosted (juce::uint32 songId)
{
    // One the audio thread hasn't taken yet is claimed back here and goes now (with what it was to
    // take over from). One it has taken goes there, wherever it got to since it was looked at:
    // still being read ahead, or already playing.
    if (auto* before = newActive.exchange (nullptr))
    {
        destroy (before);
        send (cancelPendingCommand);
    }
    else
    {
        send (dropSongCommand, songId);
    }
}

void PlaylistPlayer::removeSong (int index)
{
    if (index < 0 || index >= static_cast<int> (songs.size()))
        return;

    const auto id = songs[(size_t) index].id;

    // Playing it (or about to): it stops, as in OnStage.
    if (activeSong.load() == id || shownSong.load() == id)
        send (stopCommand);
    else if (auto* posted = postedActive != activeSerial.load() ? findStream (postedActive) : nullptr; posted != nullptr && posted->songId == id)
        dropPosted (id);

    songs.erase (songs.begin() + index);
    dropUnusedPreparations();

    if (selectedId == id)
        setSelectedId (songs.empty() ? 0 : songs[(size_t) juce::jmin (index, static_cast<int> (songs.size()) - 1)].id);

    songsChanged();
}

void PlaylistPlayer::moveSong (int from, int to)
{
    const auto count = static_cast<int> (songs.size());

    if (from < 0 || from >= count || to < 0 || to >= count || from == to)
        return;

    auto song = songs[(size_t) from];
    songs.erase (songs.begin() + from);
    songs.insert (songs.begin() + to, song);
    songsChanged();
}

void PlaylistPlayer::clearSongs()
{
    if (auto* before = newActive.exchange (nullptr))
        destroy (before);

    send (stopCommand);
    songs.clear();
    dropUnusedPreparations();
    setSelectedId (0);
    songsChanged();
}

void PlaylistPlayer::setVolume (int index, float value)
{
    if (index < 0 || index >= static_cast<int> (songs.size()))
        return;

    songs[(size_t) index].volume = juce::jlimit (Playlists::minVolume, Playlists::maxVolume, value);
    settingsChanged (index);
}

void PlaylistPlayer::setPitch (int index, float semitones)
{
    if (index < 0 || index >= static_cast<int> (songs.size()))
        return;

    songs[(size_t) index].pitch = juce::jlimit (Playlists::minPitch, Playlists::maxPitch, semitones);
    settingsChanged (index);
    readyStretchFor (index);
}

void PlaylistPlayer::setSpeed (int index, float value)
{
    if (index < 0 || index >= static_cast<int> (songs.size()))
        return;

    songs[(size_t) index].speed = juce::jlimit (Playlists::minSpeed, Playlists::maxSpeed, value);
    settingsChanged (index);
    readyStretchFor (index);
}

void PlaylistPlayer::setWait (int index, int milliseconds)
{
    if (index < 0 || index >= static_cast<int> (songs.size()))
        return;

    songs[(size_t) index].waitMs = juce::jlimit (0, Playlists::maxWaitMs, milliseconds);
    settingsChanged (index);
}

void PlaylistPlayer::setCrossfade (int index, int milliseconds)
{
    if (index < 0 || index >= static_cast<int> (songs.size()))
        return;

    songs[(size_t) index].crossfadeMs = juce::jlimit (0, Playlists::maxCrossfadeMs, milliseconds);
    settingsChanged (index);
}

void PlaylistPlayer::setExpanded (int index, bool expanded)
{
    if (index < 0 || index >= static_cast<int> (songs.size()) || songs[(size_t) index].expanded == expanded)
        return;

    songs[(size_t) index].expanded = expanded;
    songsChanged();
}

int PlaylistPlayer::getSelected() const
{
    return indexOf (selectedId);
}

void PlaylistPlayer::select (int index)
{
    if (index >= 0 && index < static_cast<int> (songs.size()))
        setSelectedId (songs[(size_t) index].id);
}

void PlaylistPlayer::setAutoPlay (bool on)
{
    if (autoPlay.exchange (on) != on)
        stateChanged.store (true);
}

juce::String PlaylistPlayer::getWildcards() const
{
    return formats.getWildcardForAllFormats() + ";" + VideoMedia::wildcards();
}

//==============================================================================
// Kept with the song, and as .playlist files
juce::var PlaylistPlayer::toVar (bool withSelection) const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "Sonify Playlist");
    root->setProperty ("version", 1);
    root->setProperty ("autoPlay", autoPlay.load());

    if (withSelection)
        root->setProperty ("selected", getSelected());

    juce::Array<juce::var> list;

    for (const auto& song : songs)
    {
        auto* item = new juce::DynamicObject();
        item->setProperty ("file", song.file);
        item->setProperty ("title", song.title);
        item->setProperty ("volume", song.volume);
        item->setProperty ("pitch", song.pitch);
        item->setProperty ("speed", song.speed);
        item->setProperty ("waitMs", song.waitMs);
        item->setProperty ("crossfadeMs", song.crossfadeMs);
        item->setProperty ("expanded", song.expanded);
        list.add (juce::var (item));
    }

    root->setProperty ("songs", list);
    return juce::var (root);
}

bool PlaylistPlayer::fromVar (const juce::var& saved)
{
    const auto* list = saved.getProperty ("songs", {}).getArray();

    if (list == nullptr)
        return false;

    std::vector<PlaylistSong> loaded;

    for (const auto& item : *list)
    {
        const auto file = item.getProperty ("file", {}).toString();

        if (file.isEmpty())
            continue;

        PlaylistSong song;
        song.file = file;
        song.title = item.getProperty ("title", {}).toString();
        song.volume = number (item.getProperty ("volume", {}), 1.0f, Playlists::minVolume, Playlists::maxVolume);
        song.pitch = number (item.getProperty ("pitch", {}), 0.0f, Playlists::minPitch, Playlists::maxPitch);
        song.speed = number (item.getProperty ("speed", {}), 1.0f, Playlists::minSpeed, Playlists::maxSpeed);
        song.waitMs = whole (item.getProperty ("waitMs", {}), 0, Playlists::maxWaitMs);
        song.crossfadeMs = whole (item.getProperty ("crossfadeMs", {}), 0, Playlists::maxCrossfadeMs);
        song.expanded = static_cast<bool> (item.getProperty ("expanded", false));
        loaded.push_back (song);
    }

    if (saved.hasProperty ("autoPlay"))
        autoPlay.store (static_cast<bool> (saved.getProperty ("autoPlay", true)));

    replaceSongs (std::move (loaded), static_cast<int> (saved.getProperty ("selected", 0)));
    return true;
}

void PlaylistPlayer::replaceSongs (std::vector<PlaylistSong> newSongs, int selectedIndex)
{
    // What played was of the old playlist.
    if (auto* before = newActive.exchange (nullptr))
        destroy (before);

    send (stopCommand);
    songs = std::move (newSongs);

    for (auto& song : songs)
    {
        song.id = nextSongId++;
        readSong (song);
    }

    dropUnusedPreparations();
    setSelectedId (songs.empty() ? 0 : songs[(size_t) juce::jlimit (0, static_cast<int> (songs.size()) - 1, selectedIndex)].id);
    songsChanged();
}

bool PlaylistPlayer::saveState (juce::MemoryBlock& destination)
{
    const auto text = juce::JSON::toString (toVar (true), true);
    destination.reset();
    destination.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
    return true;
}

bool PlaylistPlayer::loadState (const juce::MemoryBlock& source)
{
    const auto ok = fromVar (juce::JSON::parse (source.toString()));
    stateChanged.store (false);   // as the song has it
    return ok;
}

bool PlaylistPlayer::savePlaylist (const juce::File& file) const
{
    return file.getParentDirectory().createDirectory() && file.replaceWithText (juce::JSON::toString (toVar (false)));
}

bool PlaylistPlayer::loadPlaylist (const juce::File& file)
{
    const auto saved = juce::JSON::parse (file.loadFileAsString());

    if (! saved.hasProperty ("songs"))
        return false;

    return fromVar (saved);
}

//==============================================================================
// Playing, message thread
void PlaylistPlayer::send (int command, juce::uint32 song)
{
    int start1, size1, start2, size2;
    commandFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        commands[(size_t) start1] = command;
        commandSongs[(size_t) start1] = song;
        commandFifo.finishedWrite (1);
    }
}

void PlaylistPlayer::playSong (int index)
{
    if (index < 0 || index >= static_cast<int> (songs.size()))
        return;

    setSelectedId (songs[(size_t) index].id);

    // (A song marked missing is tried again: its drive may be back.)
    if (auto* stream = makeStream (index, 0.0, Start::play))
        postActive (stream);
}

void PlaylistPlayer::playPause()
{
    // Stopped, and what waits at its start isn't the selected song (or nothing does yet):
    // the selected song itself.
    if (static_cast<State> (shownState.load()) == State::stopped
        && (activeSerial.load() == 0 || activeSong.load() != selectedId))
    {
        playSong (getSelected());
        return;
    }

    send (playPauseCommand);
}

void PlaylistPlayer::stop()
{
    send (stopCommand);
}

void PlaylistPlayer::seek (double fraction)
{
    const auto index = indexOf (activeSong.load());

    if (index < 0)
        return;

    if (auto* stream = makeStream (index, juce::jlimit (0.0, 1.0, fraction), Start::seek))
        postActive (stream);
}

PlaylistPlayer::Status PlaylistPlayer::getStatus() const
{
    Status status;
    status.state = static_cast<State> (shownState.load());
    status.song = indexOf (shownSong.load());
    status.position = shownPosition.load();
    status.length = shownLength.load();
    status.waitLeft = shownWait.load();
    return status;
}

PlaylistPlayer::Stream* PlaylistPlayer::makeStream (int index, double startFraction, Start start)
{
    auto& song = songs[(size_t) index];
    std::unique_ptr<juce::AudioFormatReader> reader;

    if (song.preparing)
        return nullptr;     // a video still being read

    if (isUsablePath (song.playedFile()))
        reader.reset (formats.createReaderFor (juce::File (song.playedFile())));

    // (A rate no audio file has means a broken one.)
    if (reader == nullptr || reader->sampleRate < 1000.0 || reader->sampleRate > 768000.0 || reader->lengthInSamples <= 0)
    {
        if (! song.missing)
        {
            song.missing = true;
            songsChanged();
        }

        return nullptr;
    }

    if (song.missing)
    {
        song.missing = false;
        songsChanged();
    }

    const auto startSample = static_cast<juce::int64> (startFraction * static_cast<double> (reader->lengthInSamples));
    auto stream = std::make_unique<Stream> (nextSerial++, song, std::move (reader), startSample, start, readThread, deviceRate);
    auto* made = stream.get();
    streams.push_back (std::move (stream));
    readThread.addTimeSliceClient (made);
    return made;
}

void PlaylistPlayer::postActive (Stream* stream)
{
    postedActive = stream->serial;

    // One the audio thread hadn't taken yet is replaced.
    if (auto* before = newActive.exchange (stream))
        destroy (before);

    // Taken in its turn among the buttons pressed (a STOP before it doesn't stop it; one after does).
    send (takeActiveCommand);
}

void PlaylistPlayer::postUpcoming (Stream* stream)
{
    postedUpcoming = stream->serial;
    postedUpcomingSong = stream->songId;

    if (auto* before = newUpcoming.exchange (stream))
        destroy (before);
}

bool PlaylistPlayer::isAlive (juce::uint32 serial) const noexcept
{
    return findStream (serial) != nullptr;
}

void PlaylistPlayer::destroy (Stream* stream)
{
    readThread.removeTimeSliceClient (stream);

    for (auto it = streams.begin(); it != streams.end(); ++it)
    {
        if (it->get() == stream)
        {
            streams.erase (it);
            return;
        }
    }
}

void PlaylistPlayer::collectRetired()
{
    std::vector<Stream*> done;
    int start1, size1, start2, size2;
    retiredFifo.prepareToRead (retiredFifo.getNumReady(), start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i) done.push_back (retired[(size_t) (start1 + i)]);
    for (int i = 0; i < size2; ++i) done.push_back (retired[(size_t) (start2 + i)]);

    retiredFifo.finishedRead (size1 + size2);

    for (auto* stream : done)
        destroy (stream);
}

void PlaylistPlayer::dropAllStreams()
{
    // Only while the audio thread isn't playing the player: it lets go of everything at once.
    newActive.store (nullptr);
    newUpcoming.store (nullptr);
    current = pendingActive = upcoming = nullptr;
    fadingOut.fill (nullptr);
    backlogCount = 0;
    retiredFifo.reset();
    commandFifo.reset();

    for (auto& stream : streams)
        readThread.removeTimeSliceClient (stream.get());

    streams.clear();
    state = State::stopped;
    pausing = playWhenCued = false;
    waitLeft = 0;
    waitingFor = 0;
    postedActive = postedUpcoming = postedUpcomingSong = 0;
    nextPlan.store (0);
    publish();
}

void PlaylistPlayer::upkeep()
{
    collectRetired();
    updatePreparations();

    // Every two seconds or so: songs whose files were missing are looked for again.
    if (++upkeepCount % 66 == 0)
        recheckMissing();

    const auto shown = static_cast<State> (shownState.load());
    const auto playingId = shownSong.load();
    const auto activeId = activeSong.load();

    // The selection follows what plays: Auto-Play moving on, a song started from its banner.
    if (playingId != 0 && playingId != lastShownId && indexOf (playingId) >= 0)
        setSelectedId (playingId);

    lastShownId = playingId;

    if (! running.load())
        return;

    // Nothing playing: the selected song waits at its start for PLAY.
    const auto activeInFlight = postedActive != 0 && activeSerial.load() != postedActive && isAlive (postedActive);

    if (shown == State::stopped && selectedId != 0 && activeId != selectedId && ! activeInFlight)
        if (const auto index = indexOf (selectedId); index >= 0 && ! songs[(size_t) index].missing)
            if (auto* stream = makeStream (index, 0.0, Start::cue))
                postActive (stream);

    // Auto-Play's plan: the song after the one playing (while waiting for it, it stands).
    if (shown != State::waiting)
    {
        if (autoPlay.load() && (shown == State::playing || shown == State::paused) && activeId != 0)
            nextPlan.store (planOf (activeId, nextPlayableAfter (activeId)));
        else
            nextPlan.store (0);
    }

    // ... and that song read ahead.
    const auto next = static_cast<juce::uint32> (nextPlan.load() & 0xffffffffu);

    if (next != 0 && ! (postedUpcomingSong == next && isAlive (postedUpcoming)))
    {
        const auto index = indexOf (next);
        auto* stream = index >= 0 ? makeStream (index, 0.0, Start::cue) : nullptr;

        if (stream != nullptr)
        {
            postUpcoming (stream);
        }
        else if (shown == State::waiting && (index < 0 || (songs[(size_t) index].missing && ! songs[(size_t) index].preparing)))
        {
            // The song a wait leads to can't be opened (makeStream marked it): the wait leads to the
            // next one that can instead, or nowhere (it stops). The audio thread sees the plan now
            // start from the song it waits for, and moves on.
            nextPlan.store (planOf (next, nextPlayableAfter (next)));
        }
    }
}

//==============================================================================
bool PlaylistPlayer::activate (double sampleRate, int)
{
    if (sampleRate <= 0.0)
        return false;

    // What's playing carries on: songs play at whatever rate the device has.
    deviceRate = sampleRate;

    if (scratch == nullptr)
        scratch = std::make_unique<Scratch>();

    running.store (true);
    return true;
}

void PlaylistPlayer::deactivate()
{
    running.store (false);
}

bool PlaylistPlayer::showNativeScreen (const juce::String& title)
{
    if (screen == nullptr)
        if (auto& factory = screenFactory())
            screen = factory (*this);

    if (screen == nullptr)
        return false;

    screen->show (title);
    return true;
}

void PlaylistPlayer::hideNativeScreen()
{
    if (screen != nullptr)
        screen->hide();
}

//==============================================================================
// Playing, audio thread
int PlaylistPlayer::samplesFromMs (int ms) const noexcept
{
    return static_cast<int> (ms * deviceRate / 1000.0);
}

void PlaylistPlayer::retire (Stream* stream) noexcept
{
    if (stream == nullptr)
        return;

    int start1, size1, start2, size2;
    retiredFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        retired[(size_t) start1] = stream;
        retiredFifo.finishedWrite (1);
    }
    else if (backlogCount < static_cast<int> (backlog.size()))
    {
        backlog[(size_t) backlogCount++] = stream;
    }
    // (Otherwise it's let go of with the player.)
}

void PlaylistPlayer::toFading (Stream* stream, int fadeSamples) noexcept
{
    stream->fadeTo (0.0f, fadeSamples);

    // Several can fade at once (a crossfade cut short, a stop in a crossfade); with no room,
    // the quietest goes.
    auto* slot = &fadingOut[0];

    for (auto& candidate : fadingOut)
    {
        if (candidate == nullptr)
        {
            slot = &candidate;
            break;
        }

        if (candidate->getFade() < (*slot)->getFade())
            slot = &candidate;
    }

    retire (*slot);
    *slot = stream;
}

void PlaylistPlayer::letGo (Stream* stream, int fadeSamples) noexcept
{
    // Heard (playing, or on its way down to a pause): it fades out. Otherwise (paused, or
    // waiting at its start) it simply goes. Called before the state changes.
    if (stream == nullptr)
        return;

    if (state == State::playing && ! stream->isSilent())
        toFading (stream, fadeSamples);
    else
        retire (stream);
}

void PlaylistPlayer::swapIn (Stream* stream) noexcept
{
    switch (stream->start)
    {
        case Start::play:
            letGo (std::exchange (current, stream), samplesFromMs (switchFadeMs));
            current->setFade (1.0f);
            state = State::playing;
            pausing = playWhenCued = false;
            waitLeft = 0;
            break;

        case Start::seek:
            // Only into the song that's there: it may have changed since.
            if (current == nullptr || current->songId != stream->songId)
            {
                retire (stream);
                break;
            }

            letGo (std::exchange (current, stream), samplesFromMs (pauseFadeMs));

            if (state == State::playing && ! pausing)
            {
                stream->setFade (0.0f);
                stream->fadeTo (1.0f, samplesFromMs (pauseFadeMs));
            }
            else
            {
                stream->setFade (state == State::stopped ? 1.0f : 0.0f);

                if (pausing)
                {
                    pausing = false;
                    state = State::paused;
                }
            }
            break;

        case Start::cue:
            // Something plays already: this is out of date.
            if (state != State::stopped)
            {
                retire (stream);
                break;
            }

            retire (std::exchange (current, stream));
            current->setFade (1.0f);

            if (playWhenCued && current->songId == selectedShared.load())
            {
                state = State::playing;
                playWhenCued = false;
            }
            break;
    }
}

void PlaylistPlayer::takeUpcomingMail() noexcept
{
    if (auto* stream = newUpcoming.exchange (nullptr))
    {
        retire (upcoming);
        upcoming = stream;
    }
}

void PlaylistPlayer::takeMail (bool offline) noexcept
{
    if (auto* stream = newActive.exchange (nullptr))
    {
        retire (pendingActive);
        pendingActive = stream;
    }

    if (pendingActive != nullptr && pendingActive->waitUntilReady (offline))
        swapIn (std::exchange (pendingActive, nullptr));

    takeUpcomingMail();

    // Auto-Play's next song, if the plan has changed since it was read (the order changed, the
    // song changed, Auto-Play went off).
    if (upcoming != nullptr)
    {
        const auto plan = nextPlan.load();
        auto stale = false;

        if (state == State::waiting)
            stale = upcoming->songId != waitingFor;
        else if (state == State::stopped)
            stale = true;
        else if (current != nullptr && static_cast<juce::uint32> (plan >> 32) == current->songId)
            stale = upcoming->songId != static_cast<juce::uint32> (plan & 0xffffffffu);

        if (stale)
            retire (std::exchange (upcoming, nullptr));
    }
}

void PlaylistPlayer::playPauseNow (bool offline) noexcept
{
    switch (state)
    {
        case State::playing:
            pausing = ! pausing;
            current->fadeTo (pausing ? 0.0f : 1.0f, samplesFromMs (pauseFadeMs));

            // A song fading out under it (a crossfade) stops with it.
            if (pausing)
                for (auto* stream : fadingOut)
                    if (stream != nullptr)
                        stream->fadeTo (0.0f, samplesFromMs (pauseFadeMs));
            break;

        case State::paused:
            state = State::playing;
            pausing = false;
            current->fadeTo (1.0f, samplesFromMs (pauseFadeMs));
            break;

        case State::waiting:
            waitLeft = 0;   // the next song, now
            break;

        case State::stopped:
            if (current != nullptr && current->songId == selectedShared.load())
            {
                state = State::playing;
                current->setFade (1.0f);
                break;
            }

            // The selected song is being read: it starts as soon as it's there.
            playWhenCued = true;
            cueWaitLeft = static_cast<juce::int64> (deviceRate * 2.0);

            // A mixdown waits for it (the player readies it on the message thread).
            if (offline)
            {
                const auto until = juce::Time::getMillisecondCounter() + messageWaitMs;

                while (playWhenCued && juce::Time::getMillisecondCounter() < until)
                {
                    publish();   // what upkeep goes by
                    juce::Thread::sleep (1);
                    takeMail (true);
                }

                playWhenCued = false;
            }
            break;
    }
}

void PlaylistPlayer::stopNow() noexcept
{
    // A song fading out (a crossfade) goes as quickly as the one stopped.
    for (auto* stream : fadingOut)
        if (stream != nullptr)
            stream->fadeTo (0.0f, juce::jmin (juce::jmax (1, stream->getFadeLeft()), samplesFromMs (stopFadeMs)));

    letGo (std::exchange (current, nullptr), samplesFromMs (stopFadeMs));

    // What was about to play doesn't (a song started, then stopped before it was read).
    retire (std::exchange (pendingActive, nullptr));

    state = State::stopped;
    pausing = playWhenCued = false;
    waitLeft = 0;
}

void PlaylistPlayer::dropEverything() noexcept
{
    retire (std::exchange (current, nullptr));
    retire (std::exchange (pendingActive, nullptr));
    retire (std::exchange (upcoming, nullptr));

    for (auto& stream : fadingOut)
        retire (std::exchange (stream, nullptr));

    state = State::stopped;
    pausing = playWhenCued = false;
    waitLeft = 0;
}

void PlaylistPlayer::runCommand (int command, juce::uint32 song, bool offline) noexcept
{
    if (command == playPauseCommand)
    {
        playPauseNow (offline);
    }
    else if (command == stopCommand)
    {
        stopNow();
    }
    else if (command == cancelPendingCommand)
    {
        retire (std::exchange (pendingActive, nullptr));
    }
    else if (command == takeActiveCommand)
    {
        if (auto* stream = newActive.exchange (nullptr))
            retire (std::exchange (pendingActive, stream));
    }
    else if (command == dropSongCommand)
    {
        // A song taken out of the playlist: not played, wherever it got to.
        if (pendingActive != nullptr && pendingActive->songId == song)
            retire (std::exchange (pendingActive, nullptr));

        if (current != nullptr && current->songId == song)
            stopNow();
    }
}

juce::uint32 PlaylistPlayer::nextAfter (juce::uint32 songId, bool offline) noexcept
{
    // The plan is the message thread's; a mixdown, faster than real time, waits for it to catch up.
    auto plan = nextPlan.load();

    if (offline && static_cast<juce::uint32> (plan >> 32) != songId)
    {
        publish();   // what upkeep goes by
        const auto until = juce::Time::getMillisecondCounter() + messageWaitMs;

        while (static_cast<juce::uint32> ((plan = nextPlan.load()) >> 32) != songId && juce::Time::getMillisecondCounter() < until)
            juce::Thread::sleep (1);
    }

    // Not caught up: as if there were none.
    return static_cast<juce::uint32> (plan >> 32) == songId ? static_cast<juce::uint32> (plan & 0xffffffffu) : 0u;
}

bool PlaylistPlayer::upcomingReady (juce::uint32 songId, bool offline) noexcept
{
    const auto until = juce::Time::getMillisecondCounter() + messageWaitMs;

    if (offline)
        publish();   // what upkeep goes by

    for (;;)
    {
        takeUpcomingMail();

        if (upcoming != nullptr && upcoming->songId == songId && upcoming->waitUntilReady (offline))
            return true;

        // Playing live, it's never waited for; a mixdown waits for the player to read it.
        if (! offline || songId == 0 || juce::Time::getMillisecondCounter() > until)
            return false;

        juce::Thread::sleep (1);
    }
}

bool PlaylistPlayer::startUpcoming (bool offline) noexcept
{
    if (! upcomingReady (waitingFor, offline))
        return false;

    current = std::exchange (upcoming, nullptr);
    current->setFade (1.0f);
    state = State::playing;
    pausing = false;
    return true;
}

void PlaylistPlayer::songEnded (bool offline) noexcept
{
    const auto next = nextAfter (current->songId, offline);   // (while it's still the one playing)
    auto* finished = std::exchange (current, nullptr);
    const auto wait = finished->waitMs.load();
    retire (finished);
    pausing = false;

    // Auto-Play: the next song after its wait.
    if (autoPlay.load() && next != 0)
    {
        state = State::waiting;
        waitingFor = next;
        waitLeft = samplesFromMs (wait);
    }
    else
    {
        state = State::stopped;
    }
}

void PlaylistPlayer::publish() noexcept
{
    shownState.store (static_cast<int> (pausing ? State::paused : state));
    activeSong.store (current != nullptr ? current->songId : 0);
    activeSerial.store (current != nullptr ? current->serial : 0);
    shownSong.store (state == State::stopped ? 0u : (current != nullptr ? current->songId : waitingFor));
    shownPosition.store (current != nullptr ? current->getPosition (deviceRate) : 0.0);
    shownLength.store (current != nullptr ? current->getLengthSeconds() : 0.0);
    shownWait.store (state == State::waiting ? static_cast<double> (waitLeft) / deviceRate : 0.0);
}

void PlaylistPlayer::renderSpan (float* outLeft, float* outRight, int from, int to, bool offline) noexcept
{
    if (to <= from)
        return;

    // What was let go of fades out.
    for (auto& stream : fadingOut)
    {
        if (stream == nullptr)
            continue;

        stream->render (outLeft + from, outRight + from, to - from, *scratch, deviceRate, offline);

        if (stream->isSilent() || stream->hasEnded())
            retire (std::exchange (stream, nullptr));
    }

    auto done = from;

    while (done < to)
    {
        const auto left = to - done;

        if (state == State::playing && current != nullptr)
        {
            auto span = left;
            const auto crossfadeMs = current->crossfadeMs.load();

            // Pausing: only as far as its fade goes, so it resumes where it was heard.
            if (pausing)
                span = juce::jmin (span, juce::jmax (1, current->getFadeLeft()));

            // Crossfading into the next song, over this one's last moments.
            if (! pausing && autoPlay.load() && crossfadeMs > 0)
            {
                const auto crossfade = juce::jmin<juce::int64> (samplesFromMs (crossfadeMs), current->getOutputLength (deviceRate) / 2);
                const auto remaining = current->getOutputLeft (deviceRate);

                if (crossfade > 0 && remaining <= crossfade)
                {
                    const auto next = nextAfter (current->songId, offline);

                    if (next != 0 && upcomingReady (next, offline))
                    {
                        auto* going = std::exchange (current, std::exchange (upcoming, nullptr));
                        toFading (going, static_cast<int> (juce::jmax<juce::int64> (1, remaining)));
                        current->setFade (0.0f);
                        current->fadeTo (1.0f, static_cast<int> (juce::jmax<juce::int64> (1, juce::jmin (crossfade, remaining))));

                        // The one going plays on under it for the rest of the span.
                        going->render (outLeft + done, outRight + done, left, *scratch, deviceRate, offline);
                        continue;
                    }
                }
                else if (crossfade > 0 && remaining - crossfade < span)
                {
                    span = static_cast<int> (juce::jmax<juce::int64> (1, remaining - crossfade));
                }
            }

            const auto made = current->render (outLeft + done, outRight + done, span, *scratch, deviceRate, offline);
            done += made;

            if (current->hasEnded())
            {
                songEnded (offline);
                continue;
            }

            if (pausing && current->isSilent())
            {
                pausing = false;
                state = State::paused;
                break;
            }

            if (made < span)
                break;   // not read that far yet: the rest is silent

            continue;
        }

        if (state == State::waiting)
        {
            if (! autoPlay.load())
            {
                state = State::stopped;
                break;
            }

            // The song waited for couldn't be opened: the player's plan says which comes instead
            // (it never plans a song after itself otherwise). None: it stops.
            if (const auto plan = nextPlan.load(); static_cast<juce::uint32> (plan >> 32) == waitingFor)
            {
                waitingFor = static_cast<juce::uint32> (plan & 0xffffffffu);

                if (waitingFor == 0)
                {
                    state = State::stopped;
                    break;
                }
            }

            const auto step = static_cast<int> (juce::jmin<juce::int64> (left, juce::jmax<juce::int64> (0, waitLeft)));
            waitLeft -= step;
            done += step;

            if (waitLeft <= 0 && ! startUpcoming (offline))
                break;

            continue;
        }

        break;
    }
}

void PlaylistPlayer::process (const float*, const float*, float* outLeft, float* outRight,
                              int numSamples, const juce::MidiBuffer& midi, const clap_event_transport_t*) noexcept
{
    if (outLeft == nullptr || outRight == nullptr || numSamples <= 0)
        return;

    juce::FloatVectorOperations::clear (outLeft, numSamples);
    juce::FloatVectorOperations::clear (outRight, numSamples);

    if (! running.load() || scratch == nullptr || deviceRate <= 0.0)
        return;

    const auto offline = Plugin::offlineRendering().load (std::memory_order_relaxed);

    // A mixdown starts from silence, and what it played doesn't carry on afterwards.
    if (offline != wasOffline)
    {
        wasOffline = offline;
        dropEverything();
    }

    // What was retired while the FIFO was full goes now.
    for (int i = 0; i < backlogCount; )
    {
        int start1, size1, start2, size2;
        retiredFifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 == 0)
            break;

        retired[(size_t) start1] = backlog[(size_t) i];
        retiredFifo.finishedWrite (1);
        backlog[(size_t) i] = backlog[(size_t) --backlogCount];
    }

    // The window's buttons and the songs posted, in the order they came.
    {
        int start1, size1, start2, size2;
        commandFifo.prepareToRead (commandFifo.getNumReady(), start1, size1, start2, size2);

        for (int i = 0; i < size1; ++i) runCommand (commands[(size_t) (start1 + i)], commandSongs[(size_t) (start1 + i)], offline);
        for (int i = 0; i < size2; ++i) runCommand (commands[(size_t) (start2 + i)], commandSongs[(size_t) (start2 + i)], offline);

        commandFifo.finishedRead (size1 + size2);
    }

    takeMail (offline);

    // The track's notes, where they are in the block: 15 PLAY / PAUSE, 16 STOP.
    auto at = 0;

    for (const auto metadata : midi)
    {
        const auto* data = metadata.data;

        if (metadata.numBytes < 3 || (data[0] & 0xf0) != 0x90 || data[2] == 0
            || (data[1] != Playlists::playPauseNote && data[1] != Playlists::stopNote))
            continue;

        const auto position = juce::jlimit (at, numSamples, metadata.samplePosition);
        renderSpan (outLeft, outRight, at, position, offline);
        at = position;

        if (data[1] == Playlists::playPauseNote)
            playPauseNow (offline);
        else
            stopNow();
    }

    renderSpan (outLeft, outRight, at, numSamples, offline);

    if (playWhenCued && (cueWaitLeft -= numSamples) <= 0)
        playWhenCued = false;

    publish();
}
