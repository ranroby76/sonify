// D:\workspace\Fanan_web_hub\src\LiveShow.cpp

#include "LiveShow.h"
#include "RecordingAnalysis.h"

#if __has_include (<signalsmith-stretch/signalsmith-stretch.h>)
 #include <signalsmith-stretch/signalsmith-stretch.h>
 #define SONIFY_LIVE_STRETCH 1
#else
 #define SONIFY_LIVE_STRETCH 0
#endif

#include <cmath>

namespace LiveShow
{
namespace
{
    using Kind = ParamSpec::Kind;
    using SonifyDsp::Biquad;

    constexpr float halfPi = 1.5707963267948966f;
    constexpr float pi = 3.14159265358979323846f;
    constexpr double butterworthQ = 0.70710678118654752;

    // Where a track's waveform splits its low, middle and high (as a DJ mixer's EQ would).
    constexpr double lowCrossover = 250.0, highCrossover = 2500.0;

    // The fastest a deck plays (the widest tempo range), for the stretcher's buffers; and how much
    // faster a track's own rate may run than the device's (a track read before the device changed).
    constexpr double fastest = 1.6;
    constexpr double widestRates = 2.5;

    // What the listening hears of a track: its start, this long.
    constexpr double listenedSeconds = 240.0;

    // The tempos a track is counted in, as DJs count them: an octave from 78 (up to, not including, 156).
    constexpr double slowestBpm = 78.0, fastestBpm = 156.0;

    float dbToGain (float db, float closedAt) noexcept
    {
        return db <= closedAt ? 0.0f : juce::Decibels::decibelsToGain (db);
    }

    /** Four points' curve through the middle two (Catmull-Rom). */
    inline float hermite (float before, float a, float b, float after, float t) noexcept
    {
        const auto c1 = 0.5f * (b - before);
        const auto c2 = before - 2.5f * a + 2.0f * b - 0.5f * after;
        const auto c3 = 0.5f * (after - before) + 1.5f * (a - b);
        return ((c3 * t + c2) * t + c1) * t + a;
    }

    template <size_t count>
    int indexIn (const std::array<ParamSpec, count>& specs, const juce::String& id)
    {
        for (size_t i = 0; i < count; ++i)
            if (id == specs[i].id)
                return static_cast<int> (i);

        return -1;
    }

    /** A mono signal at RecordingAnalysis's rate, from blocks read at another (read fills the
        block it's given and returns how many it filled, 0 at the end). */
    std::vector<float> monoForListening (double fromRate, juce::int64 available,
                                         const std::function<int (float*, int)>& read, const std::function<bool()>& keepGoing)
    {
        const auto ratio = fromRate / RecordingAnalysis::rate;   // samples in per sample out
        const auto wanted = static_cast<juce::int64> (juce::jmin (static_cast<double> (available), listenedSeconds * fromRate));
        std::vector<float> out;
        out.reserve (static_cast<size_t> (static_cast<double> (wanted) / ratio) + 16);

        juce::LagrangeInterpolator interpolator;
        std::vector<float> pending;
        std::vector<float> block (16384);
        juce::int64 taken = 0;
        bool ended = false;

        while (! ended && keepGoing())
        {
            const auto count = static_cast<int> (juce::jmin<juce::int64> (static_cast<juce::int64> (block.size()), wanted - taken));
            const auto got = count > 0 ? read (block.data(), count) : 0;

            if (got <= 0)
            {
                ended = true;
                pending.insert (pending.end(), 8, 0.0f);   // the interpolator reads a little past the end
            }
            else
            {
                pending.insert (pending.end(), block.begin(), block.begin() + got);
                taken += got;
            }

            const auto canMake = static_cast<int> ((static_cast<double> (pending.size()) - 8.0) / ratio);

            if (canMake <= 0)
                continue;

            const auto at = out.size();
            out.resize (at + static_cast<size_t> (canMake));
            const auto used = interpolator.process (ratio, pending.data(), out.data() + at, canMake);
            pending.erase (pending.begin(), pending.begin() + juce::jmin (used, static_cast<int> (pending.size())));
        }

        return out;
    }

    TrackInfo listenTo (std::vector<float> mono, const std::function<bool()>& keepGoing)
    {
        TrackInfo info;

        if (mono.empty() || ! keepGoing())
            return info;

        // No song to follow: a grid no music keeps (its tempo isn't one), so the track's own is found.
        RecordingAnalysis::Grid none;
        none.bpm = 997.0;
        none.numerator = 4;
        none.denominator = 4;

        const auto heard = RecordingAnalysis::analyse (std::move (mono), 0.0, none, 0.0, [&keepGoing] (double) { return keepGoing(); });

        if (heard.ok && heard.bpm > 0.0)
        {
            // A DJ's range: a beat heard at half (or double) time is counted as dancers count it.
            auto bpm = heard.bpm;

            while (bpm < slowestBpm)
                bpm *= 2.0;

            while (bpm >= fastestBpm)
                bpm *= 0.5;

            info.bpm = std::round (bpm * 100.0) / 100.0;

            // Its first downbeat in the track (one heard before the start: the bar after it).
            auto first = heard.downbeatSeconds;

            while (first < 0.0)
                first += 4.0 * 60.0 / heard.bpm;

            info.firstBeat = first;
            info.keyTonic = heard.keyTonic;
            info.keyMinor = heard.keyMinor;
        }

        return info;
    }
}

//==============================================================================
const std::array<ParamSpec, numDeckParams>& deckSpecs()
{
    static const std::array<ParamSpec, numDeckParams> specs {{
        { "gain",    "Gain",        "Channel", Kind::knob,   -20.0f, 12.0f, 0.0f, "dB", "" },
        { "volume",  "Fader",       "Channel", Kind::fader, silentDb, 0.0f, 0.0f, "dB", "" },
        { "assign",  "Crossfader",  "Channel", Kind::choice,   0.0f,  2.0f, 1.0f, "",   "A|Thru|B" },
        { "pitch",   "Speed",       "Deck",    Kind::knob,    -1.0f,  1.0f, 0.0f, "",   "" },
        { "range",   "Tempo Range", "Deck",    Kind::choice,   0.0f,  3.0f, 1.0f, "",   "6%|8%|16%|50%" },
        { "key",     "Key",         "Deck",    Kind::knob,   -12.0f, 12.0f, 0.0f, "st", "" },
        { "phones",  "Headphones",  "Channel", Kind::toggle,   0.0f,  1.0f, 0.0f, "",   "" },
        { "sync",    "Sync",        "Deck",    Kind::toggle,   0.0f,  1.0f, 0.0f, "",   "" },
        { "trackVolume", "Track Volume", "Deck", Kind::knob,    0.0f,  1.0f, 0.5f, "",   "" },
        { "bpm",     "Tempo",       "Deck",    Kind::knob,    60.0f, 200.0f, 120.0f, "BPM", "" },
        { "trackSync", "Track Sync", "Deck",   Kind::toggle,   0.0f,  1.0f, 0.0f, "",   "" },
    }};

    return specs;
}

const std::array<ParamSpec, numChannelParams>& channelSpecs()
{
    static const std::array<ParamSpec, numChannelParams> specs {{
        { "volume", "Fader", "Channel", Kind::fader,  silentDb, 6.0f, 0.0f, "dB", "" },
        { "pan",    "Pan",   "Channel", Kind::knob,     -1.0f,  1.0f, 0.0f, "",   "" },
        { "mute",   "Mute",  "Channel", Kind::toggle,    0.0f,  1.0f, 0.0f, "",   "" },
        { "duck",   "Duck",  "Channel", Kind::toggle,    0.0f,  1.0f, 1.0f, "",   "" },
        { "phones", "Headphones", "Channel", Kind::toggle, 0.0f, 1.0f, 0.0f, "",   "" },
    }};

    return specs;
}

const std::array<ParamSpec, numMasterParams>& masterSpecs()
{
    static const std::array<ParamSpec, numMasterParams> specs {{
        { "crossfader", "Crossfader",       "Mixer",  Kind::knob,    -1.0f,     1.0f,     0.0f, "",    "" },
        { "curve",      "Crossfader Curve", "Mixer",  Kind::choice,   0.0f,     2.0f,     1.0f, "",    "Smooth|Full|Cut" },
        { "decks",      "Decks",            "Mixer",  Kind::choice,   0.0f,     1.0f,     0.0f, "",    "2|4" },
        { "volume",     "Master",           "Master", Kind::fader,  silentDb,   6.0f,     0.0f, "dB",  "" },
        { "pads",       "Pads",             "Pads",   Kind::fader,  silentDb,   6.0f,     0.0f, "dB",  "" },
        { "phonesMix",  "Headphones Mix",   "Headphones", Kind::knob, 0.0f,     1.0f,     0.0f, "",    "" },
        { "phonesLevel","Headphones Level", "Headphones", Kind::fader, silentDb, 6.0f,     0.0f, "dB",  "" },
        { "phonesSplit","Split Cue",        "Headphones", Kind::toggle, 0.0f,   1.0f,     0.0f, "",    "" },
        { "duck",       "Dynamic EQ",       "Dynamic EQ", Kind::toggle,   0.0f,     1.0f,     1.0f, "",    "" },
        // Each bell: its switch, where it sits and how wide, its level (Gain) and as low as the
        // voices take it (Duck), both "vol" (Track Volume's: 0 silence, 0.5 as it is, 1 for +25 dB;
        // 0.4207 is 3 dB down, 0.3749 5 dB), then how it hears them.
        { "b1On",       "Point 1 On",       "Dynamic EQ", Kind::toggle,   0.0f,     1.0f,     1.0f, "",    "" },
        { "b1Freq",     "Point 1 Freq",     "Dynamic EQ", Kind::knob,    60.0f, 12000.0f,   450.0f, "Hz",  "" },
        { "b1Q",        "Point 1 Q",        "Dynamic EQ", Kind::knob,     0.3f,     6.0f,     1.2f, "",    "" },
        { "b1Gain",     "Point 1 Gain",     "Dynamic EQ", Kind::knob,     0.0f,     1.0f,     0.5f, "vol", "" },
        { "b1Duck",     "Point 1 Duck",     "Dynamic EQ", Kind::knob,     0.0f,     1.0f,  0.4207f, "vol", "" },
        { "b1Thresh",   "Point 1 Threshold","Dynamic EQ", Kind::knob,   -70.0f,     0.0f,   -40.0f, "dB",  "" },
        { "b1Ratio",    "Point 1 Ratio",    "Dynamic EQ", Kind::knob,     1.0f,    10.0f,     2.5f, ":1",  "" },
        { "b1Attack",   "Point 1 Attack",   "Dynamic EQ", Kind::knob,     1.0f,   200.0f,    15.0f, "ms",  "" },
        { "b1Release",  "Point 1 Release",  "Dynamic EQ", Kind::knob,    20.0f,  2000.0f,   300.0f, "ms",  "" },
        { "b2On",       "Point 2 On",       "Dynamic EQ", Kind::toggle,   0.0f,     1.0f,     1.0f, "",    "" },
        { "b2Freq",     "Point 2 Freq",     "Dynamic EQ", Kind::knob,    60.0f, 12000.0f,  2600.0f, "Hz",  "" },
        { "b2Q",        "Point 2 Q",        "Dynamic EQ", Kind::knob,     0.3f,     6.0f,     1.4f, "",    "" },
        { "b2Gain",     "Point 2 Gain",     "Dynamic EQ", Kind::knob,     0.0f,     1.0f,     0.5f, "vol", "" },
        { "b2Duck",     "Point 2 Duck",     "Dynamic EQ", Kind::knob,     0.0f,     1.0f,  0.3749f, "vol", "" },
        { "b2Thresh",   "Point 2 Threshold","Dynamic EQ", Kind::knob,   -70.0f,     0.0f,   -42.0f, "dB",  "" },
        { "b2Ratio",    "Point 2 Ratio",    "Dynamic EQ", Kind::knob,     1.0f,    10.0f,     2.5f, ":1",  "" },
        { "b2Attack",   "Point 2 Attack",   "Dynamic EQ", Kind::knob,     1.0f,   200.0f,    10.0f, "ms",  "" },
        { "b2Release",  "Point 2 Release",  "Dynamic EQ", Kind::knob,    20.0f,  2000.0f,   250.0f, "ms",  "" },
        { "b3On",       "Point 3 On",       "Dynamic EQ", Kind::toggle,   0.0f,     1.0f,     1.0f, "",    "" },
        { "b3Freq",     "Point 3 Freq",     "Dynamic EQ", Kind::knob,    60.0f, 12000.0f,  1100.0f, "Hz",  "" },
        { "b3Q",        "Point 3 Q",        "Dynamic EQ", Kind::knob,     0.3f,     6.0f,     1.2f, "",    "" },
        { "b3Gain",     "Point 3 Gain",     "Dynamic EQ", Kind::knob,     0.0f,     1.0f,     0.5f, "vol", "" },
        { "b3Duck",     "Point 3 Duck",     "Dynamic EQ", Kind::knob,     0.0f,     1.0f,     0.5f, "vol", "" },
        { "b3Thresh",   "Point 3 Threshold","Dynamic EQ", Kind::knob,   -70.0f,     0.0f,   -40.0f, "dB",  "" },
        { "b3Ratio",    "Point 3 Ratio",    "Dynamic EQ", Kind::knob,     1.0f,    10.0f,     2.5f, ":1",  "" },
        { "b3Attack",   "Point 3 Attack",   "Dynamic EQ", Kind::knob,     1.0f,   200.0f,    12.0f, "ms",  "" },
        { "b3Release",  "Point 3 Release",  "Dynamic EQ", Kind::knob,    20.0f,  2000.0f,   280.0f, "ms",  "" },
        { "b4On",       "Point 4 On",       "Dynamic EQ", Kind::toggle,   0.0f,     1.0f,     1.0f, "",    "" },
        { "b4Freq",     "Point 4 Freq",     "Dynamic EQ", Kind::knob,    60.0f, 12000.0f,  6000.0f, "Hz",  "" },
        { "b4Q",        "Point 4 Q",        "Dynamic EQ", Kind::knob,     0.3f,     6.0f,     1.4f, "",    "" },
        { "b4Gain",     "Point 4 Gain",     "Dynamic EQ", Kind::knob,     0.0f,     1.0f,     0.5f, "vol", "" },
        { "b4Duck",     "Point 4 Duck",     "Dynamic EQ", Kind::knob,     0.0f,     1.0f,     0.5f, "vol", "" },
        { "b4Thresh",   "Point 4 Threshold","Dynamic EQ", Kind::knob,   -70.0f,     0.0f,   -44.0f, "dB",  "" },
        { "b4Ratio",    "Point 4 Ratio",    "Dynamic EQ", Kind::knob,     1.0f,    10.0f,     2.5f, ":1",  "" },
        { "b4Attack",   "Point 4 Attack",   "Dynamic EQ", Kind::knob,     1.0f,   200.0f,     8.0f, "ms",  "" },
        { "b4Release",  "Point 4 Release",  "Dynamic EQ", Kind::knob,    20.0f,  2000.0f,   220.0f, "ms",  "" },
        { "lvOn",       "Level On",         "Dynamic EQ", Kind::toggle,   0.0f,     1.0f,     1.0f, "",    "" },
        { "lvDepth",    "Level Depth",      "Dynamic EQ", Kind::knob,     0.0f,    30.0f,     0.0f, "dB",  "" },
        { "lvThresh",   "Level Threshold",  "Dynamic EQ", Kind::knob,   -70.0f,     0.0f,   -40.0f, "dB",  "" },
        { "lvRatio",    "Level Ratio",      "Dynamic EQ", Kind::knob,     1.0f,    10.0f,     2.0f, ":1",  "" },
        { "lvAttack",   "Level Attack",     "Dynamic EQ", Kind::knob,     1.0f,   200.0f,    20.0f, "ms",  "" },
        { "lvRelease",  "Level Release",    "Dynamic EQ", Kind::knob,    20.0f,  3000.0f,   600.0f, "ms",  "" },
        { "compOn",     "Compressor",       "Compressor", Kind::toggle,  0.0f,     1.0f,     0.0f, "",    "" },
        { "compThresh", "Comp Threshold",   "Compressor", Kind::knob,  -40.0f,     0.0f,   -12.0f, "dB",  "" },
        { "compRatio",  "Comp Ratio",       "Compressor", Kind::knob,    1.0f,    20.0f,     3.0f, ":1",  "" },
        { "compAttack", "Comp Attack",      "Compressor", Kind::knob,    0.1f,   100.0f,    10.0f, "ms",  "" },
        { "compRelease","Comp Release",     "Compressor", Kind::knob,   10.0f,  2000.0f,   150.0f, "ms",  "" },
        { "compKnee",   "Comp Knee",        "Compressor", Kind::knob,    0.0f,    18.0f,     6.0f, "dB",  "" },
        { "compMakeup", "Comp Makeup",      "Compressor", Kind::knob,    0.0f,    24.0f,     0.0f, "dB",  "" },
        { "driveOn",    "Drive",            "Drive",      Kind::toggle,  0.0f,     1.0f,     0.0f, "",    "" },
        { "drive",      "Drive Amount",     "Drive",      Kind::knob,    0.0f,    24.0f,     6.0f, "dB",  "" },
        { "driveTone",  "Drive Tone",       "Drive",      Kind::knob, -100.0f,   100.0f,     0.0f, "%",   "" },
        { "driveMix",   "Drive Mix",        "Drive",      Kind::knob,    0.0f,   100.0f,   100.0f, "%",   "" },
        { "driveOut",   "Drive Output",     "Drive",      Kind::knob,  -12.0f,    12.0f,     0.0f, "dB",  "" },
        // The stereo width: Width (0% mono, 100% as it is), Bass Mono (at its lowest, off) and Output.
        { "widthOn",    "Stereo Width",     "Width",      Kind::toggle,  0.0f,     1.0f,     0.0f, "",    "" },
        { "width",      "Width",            "Width",      Kind::knob,    0.0f,   200.0f,   130.0f, "%",   "" },
        { "widthBass",  "Width Bass Mono",  "Width",      Kind::knob,   20.0f,   500.0f,   120.0f, "Hz",  "" },
        { "widthOut",   "Width Output",     "Width",      Kind::knob,  -12.0f,    12.0f,     0.0f, "dB",  "" },
        { "limOn",      "Limiter",          "Limiter",    Kind::toggle,  0.0f,     1.0f,     1.0f, "",    "" },
        { "limGain",    "Limiter Gain",     "Limiter",    Kind::knob,  -24.0f,    24.0f,     0.0f, "dB",  "" },
        { "limCeiling", "Limiter Ceiling",  "Limiter",    Kind::knob,  -12.0f,     0.0f,    -1.0f, "dB",  "" },
        { "limRelease", "Limiter Release",  "Limiter",    Kind::knob,   10.0f,  1000.0f,   200.0f, "ms",  "" },
    }};

    return specs;
}

int deckIndexOf (const juce::String& id)      { return indexIn (deckSpecs(), id); }
int channelIndexOf (const juce::String& id)   { return indexIn (channelSpecs(), id); }
int masterIndexOf (const juce::String& id)    { return indexIn (masterSpecs(), id); }

float pitchRangeOf (int choice) noexcept
{
    static constexpr std::array<float, numPitchRanges> ranges { 0.06f, 0.08f, 0.16f, 0.5f };
    return ranges[static_cast<size_t> (juce::jlimit (0, numPitchRanges - 1, choice))];
}

float trackVolumeGain (float position) noexcept
{
    const auto p = juce::jlimit (0.0f, 1.0f, position);

    if (p >= 0.5f)
        return juce::Decibels::decibelsToGain (25.0f * (p - 0.5f) / 0.5f);

    // Down to silence along a curve the ear hears as even (-12 dB at a quarter).
    const auto share = p / 0.5f;
    return share * share;
}

bool canKeepKey() noexcept
{
    return SONIFY_LIVE_STRETCH != 0;
}

juce::String keyName (int tonic, bool minor)
{
    static const juce::StringArray names { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    if (tonic < 0)
        return {};

    return names[((tonic % 12) + 12) % 12] + (minor ? "m" : "");
}

//==============================================================================
std::shared_ptr<const DeckTrack> DeckTrack::load (const juce::File& file, double playbackRate,
                                                  juce::AudioFormatManager& formats, juce::String& error,
                                                  const juce::File& standsFor, const std::function<bool()>& keepGoing)
{
    // A video's sound (read out into a WAV) goes by the video's name.
    const auto& named = standsFor != juce::File() ? standsFor : file;
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        error = file.existsAsFile() ? "Sonify can't play " + named.getFileName() + "." : named.getFullPathName() + " isn't there.";
        return nullptr;
    }

    return load (std::move (reader), named, playbackRate, error, keepGoing);
}

std::shared_ptr<const DeckTrack> DeckTrack::load (std::unique_ptr<juce::AudioFormatReader> reader, const juce::File& named,
                                                  double playbackRate, juce::String& error, const std::function<bool()>& keepGoing)
{
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        error = "Sonify can't play " + named.getFileName() + ".";
        return nullptr;
    }

    // The waveform's filters ring down to nothing after each sound: without this, their tails
    // (numbers too small to matter) made a track with silences in it slow to load.
    const juce::ScopedNoDenormals noDenormals;

    const auto fileRate = reader->sampleRate;
    const auto rate = playbackRate > 0.0 ? playbackRate : fileRate;

    if (static_cast<double> (reader->lengthInSamples) / fileRate > longestTrackMinutes * 60.0)
    {
        error = named.getFileName() + " is longer than an hour: a deck takes tracks up to an hour long.";
        return nullptr;
    }

    auto made = std::make_shared<DeckTrack>();
    made->file = named;
    made->title = named.getFileNameWithoutExtension();
    made->channels = juce::jlimit (1, 2, static_cast<int> (reader->numChannels));
    made->playbackRate = rate;
    made->frames = static_cast<juce::int64> (std::ceil (static_cast<double> (reader->lengthInSamples) * rate / fileRate));
    made->lengthSeconds = static_cast<double> (made->frames) / rate;

    // The waveform: a point every bucket frames (about a hundred a second).
    const auto bucket = juce::jmax (1, juce::roundToInt (rate / 100.0));
    made->wavePerSecond = rate / bucket;

    try
    {
        made->samples.resize (static_cast<size_t> (made->frames * made->channels));
        made->wave.reserve (static_cast<size_t> ((made->frames / bucket + 2) * 4));
    }
    catch (const std::bad_alloc&)
    {
        error = "There isn't enough memory for " + named.getFileName() + ".";
        return nullptr;
    }

    // Read in blocks, at the device's rate (resampled when the file's differs).
    constexpr int blockSize = 8192;
    juce::AudioBuffer<float> block (made->channels, blockSize);
    const bool sameRate = std::abs (fileRate - rate) < 0.5;
    const auto fileLength = reader->lengthInSamples;

    std::unique_ptr<juce::AudioFormatReaderSource> source;
    std::unique_ptr<juce::ResamplingAudioSource> resampler;
    juce::int64 readAt = 0;

    if (! sameRate)
    {
        source = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
        resampler = std::make_unique<juce::ResamplingAudioSource> (source.get(), false, made->channels);
        resampler->setResamplingRatio (fileRate / rate);
        resampler->prepareToPlay (blockSize, rate);
    }

    const auto fill = [&] (int count)
    {
        block.clear();

        if (resampler != nullptr)
        {
            juce::AudioSourceChannelInfo info (&block, 0, count);
            resampler->getNextAudioBlock (info);
        }
        else
        {
            reader->read (&block, 0, count, readAt, true, made->channels > 1);
            readAt += count;
        }
    };

    // The waveform's bands: its low, middle and high.
    const auto lowPass = Biquad::lowPass (rate, lowCrossover, butterworthQ);
    const auto highPass = Biquad::highPass (rate, highCrossover, butterworthQ);
    std::array<SonifyDsp::BiquadState, 4> bands {};
    float peak = 0.0f, low = 0.0f, mid = 0.0f, high = 0.0f;
    int inBucket = 0;

    const auto toByte = [] (float value) { return static_cast<juce::uint8> (juce::jlimit (0, 255, juce::roundToInt (value * 255.0f))); };
    const auto addPoint = [&]
    {
        made->wave.push_back (toByte (peak));
        made->wave.push_back (toByte (low));
        made->wave.push_back (toByte (mid));
        made->wave.push_back (toByte (high));
        peak = low = mid = high = 0.0f;
        inBucket = 0;
    };

    juce::int64 written = 0;
    auto* out = made->samples.data();

    while (written < made->frames)
    {
        // Sonify closing: it stops.
        if (keepGoing != nullptr && ! keepGoing())
        {
            error = "Stopped";
            return nullptr;
        }

        const auto count = static_cast<int> (juce::jmin<juce::int64> (blockSize, made->frames - written));

        if (resampler == nullptr && readAt >= fileLength)
            block.clear();
        else
            fill (count);

        for (int i = 0; i < count; ++i)
        {
            float mono = 0.0f;

            for (int channel = 0; channel < made->channels; ++channel)
            {
                const auto value = juce::jlimit (-1.0f, 1.0f, block.getSample (channel, i));
                *out++ = static_cast<juce::int16> (juce::roundToInt (value * 32767.0f));
                mono += value;
            }

            mono /= static_cast<float> (made->channels);
            const auto lo = bands[1].process (lowPass, bands[0].process (lowPass, mono));
            const auto hi = bands[3].process (highPass, bands[2].process (highPass, mono));
            peak = juce::jmax (peak, std::abs (mono));
            low = juce::jmax (low, std::abs (lo));
            high = juce::jmax (high, std::abs (hi));
            mid = juce::jmax (mid, std::abs (mono - lo - hi));

            if (++inBucket == bucket)
                addPoint();
        }

        written += count;
    }

    if (inBucket > 0)
        addPoint();

    return made;
}

//==============================================================================
juce::var TrackInfo::toVar() const
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("bpm", bpm);
    object->setProperty ("firstBeat", firstBeat);
    object->setProperty ("key", keyTonic);
    object->setProperty ("minor", keyMinor);
    object->setProperty ("byUser", bpmByUser);
    object->setProperty ("heard", heard);

    juce::Array<juce::var> cues;

    for (const auto cue : hotCues)
        cues.add (cue);

    object->setProperty ("cues", cues);

    juce::Array<juce::var> loopList;

    for (const auto& loop : loops)
        loopList.add (juce::Array<juce::var> { loop.start, loop.end });

    object->setProperty ("loops", loopList);
    return juce::var (object);
}

TrackInfo TrackInfo::fromVar (const juce::var& data)
{
    TrackInfo info;
    info.bpm = juce::jlimit (0.0, 400.0, static_cast<double> (data.getProperty ("bpm", 0.0)));
    info.firstBeat = juce::jmax (0.0, static_cast<double> (data.getProperty ("firstBeat", 0.0)));
    info.keyTonic = juce::jlimit (-1, 11, static_cast<int> (data.getProperty ("key", -1)));
    info.keyMinor = static_cast<bool> (data.getProperty ("minor", false));
    info.bpmByUser = static_cast<bool> (data.getProperty ("byUser", false));
    info.heard = static_cast<bool> (data.getProperty ("heard", true));   // kept before markers were: all heard

    if (const auto* cues = data.getProperty ("cues", {}).getArray())
        for (int i = 0; i < juce::jmin (numHotCues, cues->size()); ++i)
            info.hotCues[static_cast<size_t> (i)] = static_cast<double> (cues->getReference (i));

    if (const auto* loopList = data.getProperty ("loops", {}).getArray())
    {
        for (int i = 0; i < juce::jmin (numLoops, loopList->size()); ++i)
        {
            if (const auto* pair = loopList->getReference (i).getArray(); pair != nullptr && pair->size() == 2)
            {
                auto& loop = info.loops[static_cast<size_t> (i)];
                loop.start = static_cast<double> (pair->getReference (0));
                loop.end = static_cast<double> (pair->getReference (1));

                if (! loop.isOn())
                    loop = {};
            }
        }
    }

    return info;
}

TrackInfo analyse (const DeckTrack& track, const std::function<bool()>& keepGoing)
{
    juce::int64 at = 0;

    auto mono = monoForListening (track.playbackRate, track.frames, [&track, &at] (float* block, int count)
    {
        const auto made = static_cast<int> (juce::jmin<juce::int64> (count, track.frames - at));

        for (int i = 0; i < made; ++i)
            block[i] = 0.5f * (track.sampleAt (0, at + i) + track.sampleAt (1, at + i));

        at += made;
        return made;
    }, keepGoing);

    return listenTo (std::move (mono), keepGoing);
}

TrackInfo analyse (const juce::File& file, juce::AudioFormatManager& formats, const std::function<bool()>& keepGoing,
                   juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        error = "Sonify can't read " + file.getFileName() + ".";
        return {};
    }

    return analyse (*reader, keepGoing);
}

TrackInfo analyse (juce::AudioFormatReader& reader, const std::function<bool()>& keepGoing)
{
    if (reader.sampleRate <= 0.0 || reader.lengthInSamples <= 0)
        return {};

    const auto channels = juce::jlimit (1, 2, static_cast<int> (reader.numChannels));
    juce::AudioBuffer<float> buffer (channels, 16384);
    juce::int64 at = 0;

    auto mono = monoForListening (reader.sampleRate, reader.lengthInSamples, [&] (float* block, int count)
    {
        const auto made = static_cast<int> (juce::jmin<juce::int64> (juce::jmin (count, buffer.getNumSamples()), reader.lengthInSamples - at));

        if (made <= 0)
            return 0;

        reader.read (&buffer, 0, made, at, true, channels > 1);

        for (int i = 0; i < made; ++i)
            block[i] = channels > 1 ? 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)) : buffer.getSample (0, i);

        at += made;
        return made;
    }, keepGoing);

    return listenTo (std::move (mono), keepGoing);
}

//==============================================================================
TrackLibrary::TrackLibrary (juce::File storeFile)
    : store (std::move (storeFile))
{
}

void TrackLibrary::load()
{
    if (loaded)
        return;

    loaded = true;
    const auto data = juce::JSON::parse (store);

    if (const auto* tracks = data.getProperty ("tracks", {}).getDynamicObject())
    {
        for (const auto& [path, item] : tracks->getProperties())
        {
            Entry entry;
            entry.size = static_cast<juce::int64> (item.getProperty ("size", 0));
            entry.modified = static_cast<juce::int64> (item.getProperty ("modified", 0));
            entry.info = TrackInfo::fromVar (item);
            entries[path.toString()] = entry;
        }
    }
}

std::optional<TrackInfo> TrackLibrary::find (const juce::File& track)
{
    load();

    const auto found = entries.find (track.getFullPathName());

    if (found == entries.end() || found->second.size != track.getSize()
        || found->second.modified != track.getLastModificationTime().toMilliseconds())
        return std::nullopt;

    return found->second.info;
}

void TrackLibrary::set (const juce::File& track, const TrackInfo& info)
{
    load();

    Entry entry;
    entry.size = track.getSize();
    entry.modified = track.getLastModificationTime().toMilliseconds();
    entry.info = info;
    entries[track.getFullPathName()] = entry;
    dirty = true;
}

void TrackLibrary::save()
{
    if (! loaded)
        return;

    juce::DynamicObject::Ptr tracks (new juce::DynamicObject());

    for (const auto& [path, entry] : entries)
    {
        auto item = entry.info.toVar();
        item.getDynamicObject()->setProperty ("size", entry.size);
        item.getDynamicObject()->setProperty ("modified", entry.modified);
        tracks->setProperty (path, item);
    }

    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("format", "Sonify Live Show tracks");
    root->setProperty ("version", 1);
    root->setProperty ("tracks", juce::var (tracks.get()));

    store.getParentDirectory().createDirectory();
    store.replaceWithText (juce::JSON::toString (juce::var (root.get()), true));
    dirty = false;
}

//==============================================================================
struct Deck::Stretcher
{
   #if SONIFY_LIVE_STRETCH
    signalsmith::stretch::SignalsmithStretch<float> stretch { 12345 };
   #endif

    std::array<std::vector<float>, 2> input, seekInput;
    float transpose = 1.0f;

    int seekLength (double speed) const noexcept
    {
       #if SONIFY_LIVE_STRETCH
        return stretch.outputSeekLength (static_cast<float> (speed));
       #else
        juce::ignoreUnused (speed);
        return 0;
       #endif
    }
};

Deck::Deck()
    : stretcher (std::make_unique<Stretcher>())
{
}

Deck::~Deck() = default;

void Deck::cue() noexcept
{
    if (wantPlaying.load())
    {
        wantPlaying.store (false);
        seek (cuePoint.load());
    }
    else
    {
        cuePoint.store (positionSeconds.load());
    }
}

void Deck::startOver() noexcept
{
    wantPlaying.store (false);
    alignRequest.store (-1);
    cuePoint.store (0.0);
    positionSeconds.store (0.0);
    seekRequest.store (-1.0);
    shiftRequest.store (0.0);
    loopStart.store (-1.0);
    loopEnd.store (-1.0);
    trackBpm.store (0.0);
    firstBeat.store (0.0);
}

double Deck::getSpeed() const noexcept
{
    // The range and the slider as one: read while setSpeed writes them (or written meanwhile), the
    // speed as it was last read whole. Nobody waits.
    const auto before = speedWrites.load (std::memory_order_acquire);
    const auto pitch = settings.get (deckPitch);
    const auto range = settings.get (deckRange);
    std::atomic_thread_fence (std::memory_order_acquire);

    if ((before & 1u) != 0 || speedWrites.load (std::memory_order_relaxed) != before)
        return lastSpeed.load (std::memory_order_relaxed);

    const auto speed = 1.0 + static_cast<double> (pitch) * pitchRangeOf (static_cast<int> (range));
    lastSpeed.store (speed, std::memory_order_relaxed);
    return speed;
}

void Deck::setSpeed (int range, float pitch) noexcept
{
    speedWrites.fetch_add (1, std::memory_order_relaxed);   // odd: being written
    std::atomic_thread_fence (std::memory_order_release);
    settings.set (deckRange, static_cast<float> (range));
    settings.set (deckPitch, pitch);
    speedWrites.fetch_add (1, std::memory_order_release);   // even: whole again
}

void Deck::prepare (double rate, int maxBlockSize)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    const auto size = juce::jmax (1, maxBlockSize);
    output.setSize (2, size, false, true, true);
    preFader.setSize (2, size, false, true, true);
    plainBlock.setSize (2, size, false, true, true);

    // The stretcher, and room for the most input it can take in a block (or to start). Its work is
    // spread evenly over the blocks (split computation): all at once, every 30 ms, it would take
    // longer than a small audio buffer lasts, and the sound would crackle.
    auto& s = *stretcher;

   #if SONIFY_LIVE_STRETCH
    s.stretch.presetDefault (2, static_cast<float> (sampleRate), true);
    s.stretch.setTransposeFactor (1.0f);
   #endif

    s.transpose = 1.0f;
    const auto most = static_cast<size_t> (std::ceil (size * fastest * widestRates)) + 16;
    const auto seekMost = static_cast<size_t> (juce::jmax (1, s.seekLength (fastest * widestRates))) + 16;

    for (auto& channel : s.input)
        channel.assign (most, 0.0f);

    for (auto& channel : s.seekInput)
        channel.assign (seekMost, 0.0f);

    reset();
}

void Deck::reset() noexcept
{
    level = 0.0f;
    pendingJump = -1.0;
    pendingShift = 0.0;
    primed = false;
    fadeSeen = fadeRequests.load();
    fadePosition = fadeTarget = 1.0f;
    fadeLevel.store (1.0f, std::memory_order_relaxed);
    stretching = false;
    handingOver = false;
    inputCarry = 0.0;
    meter.clear();
}

void Deck::setTrack (const DeckTrack* newTrack) noexcept
{
    track = newTrack;
    position = 0.0;
    pendingJump = -1.0;
    pendingShift = 0.0;
    level = 0.0f;
    primed = false;
    inputCarry = 0.0;
    fadeSeen = fadeRequests.load();
    fadePosition = fadeTarget = 1.0f;
    fadeLevel.store (1.0f, std::memory_order_relaxed);
}

bool Deck::loopFrames (double& start, double& end) const noexcept
{
    const auto from = loopStart.load (std::memory_order_relaxed), to = loopEnd.load (std::memory_order_relaxed);

    if (track == nullptr || from < 0.0 || to <= from)
        return false;

    start = from * track->playbackRate;
    end = juce::jmin (to * track->playbackRate, static_cast<double> (track->frames - 1));
    return end - start > 16.0;
}

bool Deck::takeJump() noexcept
{
    if (pendingJump >= 0.0)
        position = pendingJump;
    else if (pendingShift != 0.0)
        position += pendingShift;
    else
        return false;

    pendingJump = -1.0;
    pendingShift = 0.0;
    position = juce::jlimit (0.0, static_cast<double> (juce::jmax<juce::int64> (0, track->frames - 1)), position);
    return true;
}

void Deck::lineUpWith (const Deck& lead) noexcept
{
    const auto* other = lead.track;

    if (track == nullptr || other == nullptr || track->frames < 2 || ! lead.wantPlaying.load (std::memory_order_relaxed))
        return;

    const auto ownBpm = trackBpm.load(), leadBpm = lead.trackBpm.load();
    const auto ownSpeed = getSpeed(), leadSpeed = lead.getSpeed();

    if (ownBpm <= 0.0 || leadBpm <= 0.0 || ownSpeed <= 0.0 || leadSpeed <= 0.0)
        return;

    // Where both are at this block's start (this one where a jump waiting takes it), in the room's
    // time: how long until each one's next beat, and the beat to line them up on (the shorter, when
    // one plays at half the other's tempo).
    const auto own = (pendingJump >= 0.0 ? pendingJump : position + pendingShift) / track->playbackRate;
    const auto theirs = lead.position / other->playbackRate;
    const auto fraction = [] (double beats) { return beats - std::floor (beats); };
    const auto leadPeriod = 60.0 / (leadBpm * leadSpeed), ownPeriod = 60.0 / (ownBpm * ownSpeed);
    const auto untilLead = (1.0 - fraction ((theirs - lead.firstBeat.load()) * leadBpm / 60.0)) * leadPeriod;
    const auto untilOwn = (1.0 - fraction ((own - firstBeat.load()) * ownBpm / 60.0)) * ownPeriod;
    const auto period = juce::jmin (leadPeriod, ownPeriod);
    auto late = untilOwn - untilLead;
    late -= period * std::round (late / period);

    // On by as much as its beat comes late (back, when early), in the track's frames.
    if (std::abs (late) > 0.0002)
        pendingShift += late * ownSpeed * track->playbackRate;
}

double Deck::fillInput (double from, int count, float* left, float* right) const noexcept
{
    double loopA = 0.0, loopB = 0.0;
    const bool looping = loopFrames (loopA, loopB);

    if (looping && from >= loopB)
        from = loopA + std::fmod (from - loopA, loopB - loopA);

    for (int i = 0; i < count; ++i)
    {
        const auto frame = static_cast<juce::int64> (from);
        left[i] = track->sampleAt (0, frame);
        right[i] = track->sampleAt (1, frame);
        from += 1.0;

        if (looping && from >= loopB)
            from -= loopB - loopA;
    }

    return from;
}

void Deck::playPlain (float* left, float* right, int numSamples) noexcept
{
    const auto* playing = track;
    const auto frames = playing->frames;
    const auto step = getSpeed() * playing->playbackRate / sampleRate;
    const auto fade = 1.0f / static_cast<float> (juce::jmax (1.0, 0.004 * sampleRate));   // 4 ms in or out
    double loopA = 0.0, loopB = 0.0;
    const bool looping = loopFrames (loopA, loopB);
    auto want = wantPlaying.load();

    for (int i = 0; i < numSamples; ++i)
    {
        level = want && ! jumpWaiting() ? juce::jmin (1.0f, level + fade) : juce::jmax (0.0f, level - fade);

        if (level <= 0.0f)
        {
            if (takeJump())
                continue;

            if (! want)
                break;
        }

        const auto frame = static_cast<juce::int64> (position);

        if (frame >= frames - 1)
        {
            // The end: it stops there.
            want = false;
            wantPlaying.store (false);
            position = static_cast<double> (frames - 1);
            continue;
        }

        const auto t = static_cast<float> (position - static_cast<double> (frame));

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto value = hermite (playing->sampleAt (channel, frame - 1), playing->sampleAt (channel, frame),
                                        playing->sampleAt (channel, frame + 1), playing->sampleAt (channel, frame + 2), t);
            (channel == 0 ? left : right)[i] = value * level;
        }

        position += step;

        if (looping && position >= loopB)
            position = loopA + std::fmod (position - loopA, loopB - loopA);
    }
}

void Deck::playStretched (float* left, float* right, int numSamples) noexcept
{
   #if SONIFY_LIVE_STRETCH
    const auto* playing = track;
    const auto frames = playing->frames;
    auto& s = *stretcher;
    const auto semitones = std::round (settings.get (deckKey));

    // The track's frames for each of the device's (its speed; and its own rate, when the device's
    // has changed since it was read: its sound then comes down by as much, to its own pitch).
    const auto rates = playing->playbackRate > 0.0 ? playing->playbackRate / sampleRate : 1.0;
    const auto speed = getSpeed() * rates;

    // The key stays whatever the tempo; Key moves it by semitones.
    const auto transpose = static_cast<float> (std::pow (2.0, semitones / 12.0) * rates);

    if (std::abs (transpose - s.transpose) > 1.0e-6f)
    {
        s.stretch.setTransposeFactor (transpose);
        s.transpose = transpose;
    }

    auto want = wantPlaying.load();

    // Silent: a jump (or the way it plays changing) happens now; stopped, there's nothing to make.
    if (level <= 0.0f)
    {
        if (takeJump())
            primed = false;

        if (! want)
        {
            primed = false;
            return;
        }
    }

    if (position >= static_cast<double> (frames - 1))
    {
        wantPlaying.store (false);
        want = false;
    }

    // Starting (or after a jump): the stretcher primed so its next output is the track at `position`.
    if (! primed)
    {
        const auto length = juce::jlimit (1, static_cast<int> (s.seekInput[0].size()), s.seekLength (speed));
        inputPosition = fillInput (position, length, s.seekInput[0].data(), s.seekInput[1].data());
        float* seekChannels[] = { s.seekInput[0].data(), s.seekInput[1].data() };
        s.stretch.outputSeek (seekChannels, length);
        primed = true;
        inputCarry = 0.0;
    }

    // This block: as much of the track as the speed takes, out as numSamples.
    inputCarry += speed * numSamples;
    const auto count = juce::jlimit (0, static_cast<int> (s.input[0].size()), static_cast<int> (inputCarry));
    inputCarry -= count;
    inputPosition = fillInput (inputPosition, count, s.input[0].data(), s.input[1].data());

    float* in[] = { s.input[0].data(), s.input[1].data() };
    float* out[] = { left, right };
    s.stretch.process (in, count, out, numSamples);

    // Fading in or out.
    const auto fade = 1.0f / static_cast<float> (juce::jmax (1.0, 0.004 * sampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        level = want && ! jumpWaiting() ? juce::jmin (1.0f, level + fade) : juce::jmax (0.0f, level - fade);
        left[i] *= level;
        right[i] *= level;
    }

    // Where it's heard: on by as much as it read (the stretcher's delay behind the reading), round the loop.
    position += count;
    double loopA = 0.0, loopB = 0.0;

    if (loopFrames (loopA, loopB) && position >= loopB)
        position = loopA + std::fmod (position - loopA, loopB - loopA);

    position = juce::jlimit (0.0, static_cast<double> (frames - 1), position);
   #else
    juce::ignoreUnused (left, right, numSamples);
   #endif
}

void Deck::process (int numSamples, bool settle) noexcept
{
    const auto n = juce::jmin (numSamples, output.getNumSamples());

    if (n <= 0)
        return;

    auto* left = output.getWritePointer (0);
    auto* right = output.getWritePointer (1);
    juce::FloatVectorOperations::clear (left, n);
    juce::FloatVectorOperations::clear (right, n);

    if (track == nullptr || track->frames < 2)
    {
        seekRequest.store (-1.0);
        shiftRequest.store (0.0);
        wantPlaying.store (false);
        level = 0.0f;
    }
    else
    {
        // A jump: at once while it's silent, after a short fade out while it plays.
        if (const auto jump = seekRequest.exchange (-1.0); jump >= 0.0)
        {
            pendingJump = juce::jmin (jump * track->playbackRate, static_cast<double> (track->frames - 1));
            pendingShift = 0.0;
        }

        if (const auto by = shiftRequest.exchange (0.0); by != 0.0)
            pendingShift += by * track->playbackRate;

        if (level <= 0.0f && takeJump())
            primed = false;

        // Another tempo or key: through the stretcher, which keeps the key whatever the tempo (Key
        // moves it). It takes over at once, without a break: primed where the track is, the block
        // crossfaded from the plain one. Back to playing plain (as recorded) only while silent,
        // so the tempo can come back to the track's own without a break either.
        const auto moved = std::abs (getSpeed() - 1.0) > 1.0e-9 || std::abs (std::round (settings.get (deckKey))) > 0.5f;
        const auto wantStretch = canKeepKey() && moved;

        if (wantStretch && ! stretching)
        {
            stretching = true;
            primed = false;
            handingOver = level > 0.0f && ! jumpWaiting();
        }
        else if (! wantStretch && stretching && level <= 0.0f)
        {
            stretching = false;
            primed = false;
        }

        if (handingOver)
        {
            // The block both ways from the same place, then from one to the other.
            handingOver = false;
            const auto from = position;
            const auto levelWas = level;
            auto* plainLeft = plainBlock.getWritePointer (0);
            auto* plainRight = plainBlock.getWritePointer (1);
            juce::FloatVectorOperations::clear (plainLeft, n);
            juce::FloatVectorOperations::clear (plainRight, n);
            playPlain (plainLeft, plainRight, n);

            position = from;
            level = levelWas;
            playStretched (left, right, n);

            const auto across = 1.0f / static_cast<float> (n);

            for (int i = 0; i < n; ++i)
            {
                const auto t = static_cast<float> (i + 1) * across;
                left[i] = plainLeft[i] + (left[i] - plainLeft[i]) * t;
                right[i] = plainRight[i] + (right[i] - plainRight[i]) * t;
            }
        }
        else if (stretching)
        {
            playStretched (left, right, n);
        }
        else
        {
            playPlain (left, right, n);
        }

        positionSeconds.store (position / track->playbackRate);
    }

    // A fade asked for (the playlist's crossfades): where it goes, and how fast.
    if (const auto seen = fadeRequests.load (std::memory_order_acquire); seen != fadeSeen)
    {
        fadeSeen = seen;

        if (const auto from = fadeFromRequest.load(); from >= 0.0f)
            fadePosition = juce::jlimit (0.0f, 1.0f, from);

        fadeTarget = juce::jlimit (0.0f, 1.0f, fadeToRequest.load());
        const auto seconds = fadeSecondsRequest.load();
        fadeStep = seconds > 0.0 ? static_cast<float> (1.0 / (seconds * sampleRate)) : 0.0f;

        if (seconds <= 0.0)
            fadePosition = fadeTarget;
    }

    if (fadePosition != fadeTarget)
    {
        const auto move = fadeStep * static_cast<float> (n);
        fadePosition = fadePosition < fadeTarget ? juce::jmin (fadeTarget, fadePosition + move) : juce::jmax (fadeTarget, fadePosition - move);
    }

    fadeLevel.store (fadePosition, std::memory_order_relaxed);
    const auto faded = fadePosition >= 1.0f ? 1.0f : std::sin (fadePosition * halfPi);

    // Its channel: its own, or the one it plays through (the playlist's one player).
    const auto& strip = channelFrom != nullptr ? *channelFrom : settings;

    // Gain, the track's own volume (a playlist's, or the deck's Track Volume), and the fade.
    const auto gain = juce::Decibels::decibelsToGain (strip.get (deckGain)) * trackVolumeGain (settings.get (deckTrackVolume)) * faded;
    const auto fader = noFader ? 1.0f : dbToGain (strip.get (deckVolume), silentDb);

    if (settle)
    {
        lastGain = gain;
        lastFader = fader;
    }

    const auto inverse = 1.0f / static_cast<float> (n);

    for (int i = 0; i < n; ++i)
    {
        const auto g = lastGain + (gain - lastGain) * static_cast<float> (i + 1) * inverse;
        left[i] *= g;
        right[i] *= g;
    }

    meter.measure (left, right, n, sampleRate, 0.0f);

    // In the headphones: as it is now, before its fader.
    cueing = strip.isOn (deckPhones);

    if (cueing)
    {
        preFader.copyFrom (0, 0, left, n);
        preFader.copyFrom (1, 0, right, n);
    }

    // The fader, last.
    for (int i = 0; i < n; ++i)
    {
        const auto f = lastFader + (fader - lastFader) * static_cast<float> (i + 1) * inverse;
        left[i] *= f;
        right[i] *= f;
    }

    lastGain = gain;
    lastFader = fader;
}

//==============================================================================
void Ducker::prepare (double rate)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    seenChanges = 0xffffffffu;
    reset();
}

void Ducker::reset() noexcept
{
    for (auto& point : points)
    {
        for (auto& state : point.music)
            state.reset();

        point.voice.reset();
        point.envelope = 0.0f;
        point.gain = 1.0f;
        point.step = 0.0f;
        point.loudest = 0.0f;
        point.working = false;
        point.glide = 0;
    }

    for (size_t i = 0; i < numPoints; ++i)
    {
        gainDb[i].store (0.0f, std::memory_order_relaxed);
        voiceLevel[i].store (-100.0f, std::memory_order_relaxed);
    }

    countdown = 0;
}

void Ducker::refresh (const Settings<numMasterParams>& settings) noexcept
{
    const auto coefficient = [this] (float ms)
    {
        return 1.0f - std::exp (-1.0f / (juce::jmax (0.1f, ms) * 0.001f * static_cast<float> (sampleRate)));
    };

    // A bell's level moved (its Gain or its Duck, dragged on the page's curve): it glides there
    // over 10 ms, never a jump (from silence to 25 dB up and back is a long way).
    const auto levelGlide = juce::roundToInt (0.01 * sampleRate);

    const auto setPoint = [&] (Point& point, int on, int threshold, int ratio, int attack, int release, float level, float duck)
    {
        const auto lowest = juce::jmin (duck, level);   // the Duck never above the Gain

        if (! fresh && (std::abs (level - point.level) > 1.0e-6f || std::abs (lowest - point.floor) > 1.0e-6f))
            point.glide = juce::jmax (point.glide, levelGlide);

        point.on = settings.isOn (on);
        point.level = level;
        point.floor = lowest;
        point.threshold = settings.get (threshold);
        point.slope = 1.0f - 1.0f / juce::jmax (1.0f, settings.get (ratio));
        point.attack = coefficient (settings.get (attack));
        point.release = coefficient (settings.get (release));
    };

    for (int bell = 0; bell < numBells; ++bell)
    {
        auto& point = points[static_cast<size_t> (bell)];
        point.band = Biquad::bandPass (sampleRate, settings.get (bellParam (bell, bell1Freq)), settings.get (bellParam (bell, bell1Q)));
        setPoint (point, bellParam (bell, bell1On), bellParam (bell, bell1Threshold), bellParam (bell, bell1Ratio),
                  bellParam (bell, bell1Attack), bellParam (bell, bell1Release),
                  trackVolumeGain (settings.get (bellParam (bell, bell1Gain))), trackVolumeGain (settings.get (bellParam (bell, bell1Duck))));
    }

    // The level point: as it is, down to its Depth.
    setPoint (points[numBells], levelOn, levelThreshold, levelRatio, levelAttack, levelRelease,
              1.0f, juce::Decibels::decibelsToGain (-juce::jmax (0.0f, settings.get (levelDepth))));
    fresh = false;
}

void Ducker::process (float* left, float* right, const float* side, int numSamples,
                      const Settings<numMasterParams>& settings, bool on) noexcept
{
    if (const auto changes = settings.getChanges(); changes != seenChanges)
    {
        seenChanges = changes;
        refresh (settings);
    }

    for (auto& point : points)
        point.loudest = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Every 16 samples, each point's gain: its own level, down as far as the voices in its band
        // ask for (over its threshold, by its ratio), no lower than its Duck (a bell's; the level
        // point's Depth). Switched in or out (the effect, or the point on its own), it glides there
        // over 30 ms: a deep cut, or a bell's boost, let go or put on at once would click.
        if (countdown <= 0)
        {
            for (auto& point : points)
            {
                const auto levelDb = 20.0f * std::log10 (juce::jmax (1.0e-6f, point.envelope));
                const auto over = levelDb - point.threshold;
                const bool working = on && point.on;

                if (working != point.working)
                {
                    point.working = working;
                    point.glide = juce::jmax (point.glide, juce::roundToInt (0.03 * sampleRate));
                }

                auto target = 1.0f;

                if (working)
                {
                    const auto ducked = over > 0.0f ? point.level * juce::Decibels::decibelsToGain (-over * point.slope, -200.0f) : point.level;
                    target = juce::jmax (point.floor, ducked);
                }

                point.step = (target - point.gain) / static_cast<float> (juce::jmax (16, point.glide));
                point.glide = juce::jmax (0, point.glide - 16);
            }

            countdown = 16;
        }

        --countdown;

        // The ear: each bell hears the voices in its band, the level point all of them.
        const auto heard = side[i];

        for (size_t p = 0; p < numPoints; ++p)
        {
            auto& point = points[p];
            const auto x = std::abs (p < numBells ? point.voice.process (point.band, heard) : heard);
            point.envelope += (x > point.envelope ? point.attack : point.release) * (x - point.envelope);
            point.loudest = juce::jmax (point.loudest, point.envelope);
            point.gain = juce::jmax (0.0f, point.gain + point.step);
        }

        // The music: a bell at gain g is x + (g - 1) * its band (its centre at g: 0 takes it out,
        // silence; the band's filters always run, so they're never stale when it comes in), the
        // four in turn, then the level.
        for (int channel = 0; channel < 2; ++channel)
        {
            auto& sample = (channel == 0 ? left : right)[i];
            auto x = sample;

            for (size_t p = 0; p < numBells; ++p)
            {
                auto& point = points[p];
                const auto band = point.music[static_cast<size_t> (channel)].process (point.band, x);
                x += (point.gain - 1.0f) * band;
            }

            sample = x * points[numBells].gain;
        }
    }

    for (size_t p = 0; p < numPoints; ++p)
    {
        const auto& point = points[p];
        gainDb[p].store (point.gain > 1.0e-5f ? 20.0f * std::log10 (point.gain) : -100.0f, std::memory_order_relaxed);
        voiceLevel[p].store (20.0f * std::log10 (juce::jmax (1.0e-5f, point.loudest)), std::memory_order_relaxed);
    }
}

//==============================================================================
namespace
{
    // How long the limiter holds once it has pulled down: longer than the bass's own half waves
    // (25 Hz and up), so it doesn't follow them up and down (which is heard as distortion).
    constexpr double limiterHoldSeconds = 0.02;

    // The drive's curve: with no Drive, full scale only just reaches into it (all but clean); the
    // most Drive takes it 24 dB further in.
    constexpr float driveReach = 0.3f;

    // The drive's Tone: a tilt about this frequency, this much down and up at its ends.
    constexpr double driveToneHz = 800.0;
    constexpr float driveToneDb = 4.0f;
}

void MasterEffects::prepare (double rate, int maxBlockSize)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    compressor.prepare (sampleRate);
    dryLeft.assign (static_cast<size_t> (juce::jmax (1, maxBlockSize)), 0.0f);
    dryRight.assign (static_cast<size_t> (juce::jmax (1, maxBlockSize)), 0.0f);
    toneCoefficient = static_cast<float> (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * driveToneHz / sampleRate));
    reset();
}

void MasterEffects::reset() noexcept
{
    compressor.reset();
    compressing = false;
    driving = false;
    widening = false;
    limiting = true;
    toneLow.fill (0.0f);
    hz1 = hz2 = 0.0f;
    bassCorner = 0.0f;
    sumLR = sumLL = sumRR = 0.0;
    correlation.store (1.0f, std::memory_order_relaxed);
    for (auto& point : pointMid)
        point.store (0.0f, std::memory_order_relaxed);

    for (auto& point : pointSide)
        point.store (0.0f, std::memory_order_relaxed);

    pointAt.store (0, std::memory_order_release);
    pointSkip = 0;
    gain = 1.0f;
    holding = 0;
    blockReduction = 0.0f;
    compressorMost.store (0.0f, std::memory_order_relaxed);
    driveMost.store (0.0f, std::memory_order_relaxed);
    limiterMost.store (0.0f, std::memory_order_relaxed);
    inputMost.store (-100.0f, std::memory_order_relaxed);
}

template <typename Work>
void MasterEffects::switchOver (float* left, float* right, int n, bool on, Work&& work) noexcept
{
    std::copy (left, left + n, dryLeft.begin());
    std::copy (right, right + n, dryRight.begin());
    work (left, right, n);
    const auto inverse = 1.0f / static_cast<float> (n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<float> (i + 1) * inverse;
        const auto share = on ? t : 1.0f - t;
        const auto place = static_cast<size_t> (i);
        left[i] = dryLeft[place] + (left[i] - dryLeft[place]) * share;
        right[i] = dryRight[place] + (right[i] - dryRight[place]) * share;
    }
}

void MasterEffects::process (float* left, float* right, int numSamples, const Settings<numMasterParams>& settings, bool settle) noexcept
{
    const auto n = juce::jmin (numSamples, static_cast<int> (dryLeft.size()));

    if (n <= 0)
        return;

    // Each effect, when it's on. Switched, the block glides from the sound as it came to the
    // effect's (or back): no jump either way.

    // The compressor.
    const bool compressWanted = settings.isOn (compressorOn);
    compressor.set (settings.get (compressorThreshold), juce::jmax (1.0f, settings.get (compressorRatio)), settings.get (compressorAttack),
                    settings.get (compressorRelease), settings.get (compressorKnee), settings.get (compressorMakeup));

    if (compressWanted != compressing)
    {
        if (compressWanted)
            compressor.prime (left, right, n);

        switchOver (left, right, n, compressWanted, [this] (float* l, float* r, int count) { compress (l, r, count); });
        compressing = compressWanted;
    }
    else if (compressing)
    {
        compress (left, right, n);
    }

    if (const auto compressed = compressing ? compressor.getReductionDb() : 0.0f; compressed > compressorMost.load (std::memory_order_relaxed))
        compressorMost.store (compressed, std::memory_order_relaxed);

    // The drive.
    const bool driveWanted = settings.isOn (driveOn);

    if (driveWanted != driving)
    {
        if (driveWanted)
            toneLow.fill (0.0f);

        // Its controls start where they are, not where they were when it was last on.
        switchOver (left, right, n, driveWanted, [&] (float* l, float* r, int count) { drive (l, r, count, settings, settle || driveWanted); });
        driving = driveWanted;
    }
    else if (driving)
    {
        drive (left, right, n, settings, settle);
    }

    // The stereo width.
    const bool widenWanted = settings.isOn (widthOn);

    if (widenWanted != widening)
    {
        if (widenWanted)
            hz1 = hz2 = 0.0f;

        switchOver (left, right, n, widenWanted, [&] (float* l, float* r, int count) { widen (l, r, count, settings, settle || widenWanted); });
        widening = widenWanted;
    }
    else if (widening)
    {
        widen (left, right, n, settings, settle);
    }

    watchStereo (left, right, n);

    // The limiter.
    const bool limitWanted = settings.isOn (limiterOn);

    if (limitWanted != limiting)
    {
        if (limitWanted)
        {
            gain = 1.0f;
            holding = 0;
        }

        switchOver (left, right, n, limitWanted, [&] (float* l, float* r, int count) { limit (l, r, count, settings, settle || limitWanted); });
        limiting = limitWanted;
    }
    else if (limiting)
    {
        limit (left, right, n, settings, settle);
    }
    else
    {
        // Off: nothing pulled down; the In meter shows how loud it goes out.
        float loudest = 0.0f;

        for (int i = 0; i < n; ++i)
            loudest = juce::jmax (loudest, std::abs (left[i]), std::abs (right[i]));

        blockReduction = 0.0f;

        if (const auto inDb = juce::Decibels::gainToDecibels (loudest, -100.0f); inDb > inputMost.load (std::memory_order_relaxed))
            inputMost.store (inDb, std::memory_order_relaxed);
    }
}

void MasterEffects::compress (float* left, float* right, int n) noexcept
{
    compressor.process (left, right, n);
}

void MasterEffects::drive (float* left, float* right, int n, const Settings<numMasterParams>& settings, bool settle) noexcept
{
    // How hard: how far into the curve full scale reaches, and what keeps full scale full (so the
    // peaks stay where they were and everything under them comes up, rounder).
    const auto reach = driveReach * juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 24.0f, settings.get (driveAmount)));
    const auto norm = 1.0f / std::tanh (reach);
    const auto tone = juce::jlimit (-1.0f, 1.0f, settings.get (driveTone) * 0.01f);
    const auto lowGain = juce::Decibels::decibelsToGain (-tone * driveToneDb);
    const auto highGain = juce::Decibels::decibelsToGain (tone * driveToneDb);
    const auto mix = juce::jlimit (0.0f, 1.0f, settings.get (driveMix) * 0.01f);
    const auto output = juce::Decibels::decibelsToGain (juce::jlimit (-12.0f, 12.0f, settings.get (driveOutput)));

    if (settle)
    {
        lastReach = reach;
        lastLowGain = lowGain;
        lastHighGain = highGain;
        lastMix = mix;
        lastOutput = output;
    }

    const auto inverse = 1.0f / static_cast<float> (n);
    float loudest = 0.0f;

    // While Drive moves, what keeps full scale full follows the curve's reach sample by sample
    // (gliding the two apart would let the peaks jump past full scale for a block).
    const bool reachMoves = reach != lastReach;

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<float> (i + 1) * inverse;
        const auto k = lastReach + (reach - lastReach) * t;
        const auto keep = reachMoves ? 1.0f / std::tanh (k) : norm;
        const auto gl = lastLowGain + (lowGain - lastLowGain) * t;
        const auto gh = lastHighGain + (highGain - lastHighGain) * t;
        const auto m = lastMix + (mix - lastMix) * t;
        const auto out = lastOutput + (output - lastOutput) * t;

        for (size_t channel = 0; channel < 2; ++channel)
        {
            auto& sample = (channel == 0 ? left : right)[i];
            const auto x = sample;
            loudest = juce::jmax (loudest, std::abs (x));

            // The curve, then its tone: the low band and the rest, tilted.
            const auto shaped = std::tanh (k * x) * keep;
            auto& low = toneLow[channel];
            low += toneCoefficient * (shaped - low);
            const auto wet = low * gl + (shaped - low) * gh;
            sample = (x + (wet - x) * m) * out;
        }
    }

    lastReach = reach;
    lastLowGain = lowGain;
    lastHighGain = highGain;
    lastMix = mix;
    lastOutput = output;

    // Its meter: how far the curve rounds the loudest moment, against a straight line.
    if (loudest > 1.0e-5f)
    {
        const auto straight = reach * loudest;
        const auto rounded = 20.0f * std::log10 (straight / juce::jmax (1.0e-9f, std::tanh (straight)));

        if (rounded > driveMost.load (std::memory_order_relaxed))
            driveMost.store (rounded, std::memory_order_relaxed);
    }
}

void MasterEffects::widen (float* left, float* right, int n, const Settings<numMasterParams>& settings, bool settle) noexcept
{
    // Width and Output, gliding over the block to where they're turned.
    const auto width = juce::jlimit (0.0f, 2.0f, settings.get (widthAmount) * 0.01f);
    const auto output = juce::Decibels::decibelsToGain (juce::jlimit (-12.0f, 12.0f, settings.get (widthOutput)));
    const auto corner = settings.get (widthBassMono);
    const bool bassMono = corner > masterSpecs()[static_cast<size_t> (widthBassMono)].minimum + 0.5f;

    if (settle)
    {
        lastWidth = width;
        lastWidthOutput = output;
    }

    // Bass Mono: the side through a highpass (12 dB an octave, Butterworth) at the corner.
    if (! bassMono)
    {
        bassCorner = 0.0f;
        hz1 = hz2 = 0.0f;
    }
    else if (corner != bassCorner)
    {
        bassCorner = corner;
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (20.0, sampleRate * 0.45, static_cast<double> (corner)) / sampleRate;
        const auto cosine = std::cos (w0), alpha = std::sin (w0) / juce::MathConstants<double>::sqrt2;   // Q = 1/sqrt(2)
        const auto a0 = 1.0 + alpha;
        hb0 = static_cast<float> ((1.0 + cosine) * 0.5 / a0);
        hb1 = static_cast<float> (-(1.0 + cosine) / a0);
        hb2 = hb0;
        ha1 = static_cast<float> (-2.0 * cosine / a0);
        ha2 = static_cast<float> ((1.0 - alpha) / a0);
    }

    const auto inverse = 1.0f / static_cast<float> (n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<float> (i + 1) * inverse;
        const auto w = lastWidth + (width - lastWidth) * t;
        const auto out = lastWidthOutput + (output - lastWidthOutput) * t;

        // Mid and side (Dozius' widener): the side's share scaled by Width.
        const auto mid = 0.5f * (left[i] + right[i]);
        auto side = 0.5f * (left[i] - right[i]);

        if (bassMono)
        {
            const auto filtered = hb0 * side + hz1;
            hz1 = hb1 * side - ha1 * filtered + hz2;
            hz2 = hb2 * side - ha2 * filtered;
            side = filtered;
        }

        side *= w;
        left[i] = (mid + side) * out;
        right[i] = (mid - side) * out;
    }

    // Denormals can't build up in the highpass's state.
    if (std::abs (hz1) < 1.0e-15f) hz1 = 0.0f;
    if (std::abs (hz2) < 1.0e-15f) hz2 = 0.0f;

    lastWidth = width;
    lastWidthOutput = output;
}

void MasterEffects::watchStereo (const float* left, const float* right, int n) noexcept
{
    // How alike the sides are, over about 300 ms; and every 4th moment for the goniometer.
    double lr = 0.0, ll = 0.0, rr = 0.0;
    auto at = pointAt.load (std::memory_order_relaxed);

    for (int i = 0; i < n; ++i)
    {
        lr += static_cast<double> (left[i]) * right[i];
        ll += static_cast<double> (left[i]) * left[i];
        rr += static_cast<double> (right[i]) * right[i];

        if (++pointSkip >= 4)
        {
            pointSkip = 0;
            pointMid[static_cast<size_t> (at)].store (0.5f * (left[i] + right[i]), std::memory_order_relaxed);
            pointSide[static_cast<size_t> (at)].store (0.5f * (left[i] - right[i]), std::memory_order_relaxed);
            at = (at + 1) % stereoPoints;
        }
    }

    pointAt.store (at, std::memory_order_release);

    const auto keep = std::exp (-static_cast<double> (n) / (0.3 * sampleRate));
    sumLR = sumLR * keep + lr;
    sumLL = sumLL * keep + ll;
    sumRR = sumRR * keep + rr;

    if (sumLL < 1.0e-30) sumLL = 0.0;
    if (sumRR < 1.0e-30) sumRR = 0.0;
    if (std::abs (sumLR) < 1.0e-30) sumLR = 0.0;

    // Silence rests at +1; one side alone (the other silent) is unrelated: 0.
    const auto energy = std::sqrt (sumLL * sumRR);
    const auto value = energy > 1.0e-9 ? juce::jlimit (-1.0, 1.0, sumLR / energy)
                     : sumLL + sumRR > 1.0e-7 ? 0.0 : 1.0;
    correlation.store (static_cast<float> (value), std::memory_order_relaxed);
}

void MasterEffects::readStereo (float* mids, float* sides, int count) const noexcept
{
    count = juce::jlimit (0, stereoPoints, count);
    const auto at = pointAt.load (std::memory_order_acquire);

    // (The audio thread writes on meanwhile: a point may be a newer one, never a torn number.)
    for (int i = 0; i < count; ++i)
    {
        const auto index = static_cast<size_t> ((at - count + i + stereoPoints) % stereoPoints);
        mids[i] = pointMid[index].load (std::memory_order_relaxed);
        sides[i] = pointSide[index].load (std::memory_order_relaxed);
    }
}

void MasterEffects::limit (float* left, float* right, int n, const Settings<numMasterParams>& settings, bool settle) noexcept
{
    const auto inverse = 1.0f / static_cast<float> (n);
    const auto push = juce::Decibels::decibelsToGain (settings.get (limiterGain));
    const auto ceiling = juce::Decibels::decibelsToGain (juce::jmin (0.0f, settings.get (limiterCeiling)));
    const auto release = static_cast<float> (std::exp (-1.0 / (juce::jmax (1.0, static_cast<double> (settings.get (limiterRelease))) * 0.001 * sampleRate)));
    const auto hold = static_cast<int> (std::lround (limiterHoldSeconds * sampleRate));

    if (settle)
    {
        lastGain = push;
        lastCeiling = ceiling;
    }

    // Gain, going in (gliding to where it's turned), then the peak limiter: down at once to what
    // each sample needs, held, then let go in dB over its Release. The very last guard: nothing
    // past the Ceiling.
    float loudest = 0.0f;
    auto lowest = 1.0f;

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<float> (i + 1) * inverse;
        const auto g = lastGain + (push - lastGain) * t;
        left[i] *= g;
        right[i] *= g;

        const auto c = lastCeiling + (ceiling - lastCeiling) * t;
        const auto level = juce::jmax (std::abs (left[i]), std::abs (right[i]));
        loudest = juce::jmax (loudest, level);
        const auto needed = level > c ? c / level : 1.0f;

        if (needed <= gain)
        {
            gain = needed;
            holding = hold;
        }
        else if (holding > 0)
        {
            --holding;
        }
        else if (gain < 1.0f)
        {
            gain = juce::jmin (needed, gain > 0.99999f ? 1.0f : std::pow (gain, release));
        }

        lowest = juce::jmin (lowest, gain);
        left[i] = juce::jlimit (-c, c, left[i] * gain);
        right[i] = juce::jlimit (-c, c, right[i] * gain);
    }

    lastGain = push;
    lastCeiling = ceiling;

    // The meters: the most it pulled down, and the loudest going in, until they're looked at.
    blockReduction = lowest < 1.0f ? -juce::Decibels::gainToDecibels (lowest, -100.0f) : 0.0f;

    if (blockReduction > limiterMost.load (std::memory_order_relaxed))
        limiterMost.store (blockReduction, std::memory_order_relaxed);

    if (const auto inDb = juce::Decibels::gainToDecibels (loudest, -100.0f); inDb > inputMost.load (std::memory_order_relaxed))
        inputMost.store (inDb, std::memory_order_relaxed);
}

//==============================================================================
Mixer::Mixer()
{
    resetSettings();
}

void Mixer::resetSettings()
{
    settings.reset();

    for (int d = 0; d < maxDecks; ++d)
    {
        auto& deck = decks[static_cast<size_t> (d)];
        deck.settings.reset();
        deck.settings.set (deckAssign, static_cast<float> (defaultAssign (d)));
    }
}

void Mixer::prepare (double rate, int maxBlockSize)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    const auto size = juce::jmax (1, maxBlockSize);

    music.setSize (2, size, false, true, true);
    voices.setSize (2, size, false, true, true);
    padBus.setSize (2, size, false, true, true);
    cueBus.setSize (2, size, false, true, true);
    scratch.setSize (2, size, false, true, true);
    side.assign (static_cast<size_t> (size), 0.0f);

    for (auto& deck : decks)
        deck.prepare (sampleRate, size);

    ducker.prepare (sampleRate);
    effects.prepare (sampleRate, size);
    phonesLimiter.prepare (sampleRate);
    phonesLimiter.setSettings (-1.0f, 120.0f);

    // The device (re)starting: a pad pressed while no sound could play is let go (never played
    // later, by itself).
    for (auto& pad : pads)
        pad.command.store (0);

    reset();
}

void Mixer::reset() noexcept
{
    for (auto& deck : decks)
        deck.reset();

    // (A pad's press waiting stays: the show switching on for it is reset first, then plays it. A
    // press that can't be played is let go where that's known: Pad::cancel, setSound, prepare.)
    for (auto& pad : pads)
    {
        pad.playing.store (false);
        pad.level = 0.0f;
        pad.at = 0.0;
    }

    ducker.reset();
    effects.reset();
    phonesLimiter.reset();
    lastCross.fill (0.0f);
    lastMaster = 0.0f;
    lastPads = 0.0f;
    lastPhones = 0.0f;
    lastCueShare = 0.0f;
    lastShowShare = 1.0f;
    lastPlayback = 1.0f;
    lastStream = 0.0f;
    settle = true;
    anyCued = false;
    meter.clear();
    phonesMeter.clear();
}

void Mixer::begin (int numSamples) noexcept
{
    blockSamples = juce::jmax (0, juce::jmin (numSamples, music.getNumSamples()));
    music.clear (0, blockSamples);
    voices.clear (0, blockSamples);
    cueBus.clear (0, blockSamples);
    anyCued = false;
    std::fill (side.begin(), side.begin() + blockSamples, 0.0f);
}

void Mixer::addChannel (Channel& channel, const float* left, const float* right, int numSamples) noexcept
{
    const auto n = juce::jmin (numSamples, blockSamples);

    if (n <= 0 || left == nullptr || right == nullptr)
        return;

    channel.meter.measure (left, right, n, sampleRate, 0.0f);
    const auto mutedNow = micsMuted.load (std::memory_order_relaxed);

    // Its recording, as it comes in (the mics muted: silence, so its file keeps time with the others).
    if (channel.recorder != nullptr)
    {
        if (mutedNow)
        {
            scratch.clear (0, n);
            channel.recorder->write (scratch.getArrayOfReadPointers(), n);
        }
        else
        {
            const float* const channels[] = { left, right };
            channel.recorder->write (channels, n);
        }
    }

    const auto& s = channel.settings;

    // In the headphones: before its fader, as it comes in.
    if (s.isOn (channelPhones))
    {
        cueBus.addFrom (0, 0, left, n);
        cueBus.addFrom (1, 0, right, n);
        anyCued = true;
    }

    const auto gain = s.isOn (channelMute) || mutedNow ? 0.0f : dbToGain (s.get (channelVolume), silentDb);
    const auto pan = s.get (channelPan);
    const auto toLeft = gain * juce::jmin (1.0f, 1.0f - pan);
    const auto toRight = gain * juce::jmin (1.0f, 1.0f + pan);
    const auto toSide = s.isOn (channelDuck) ? 0.5f * gain : 0.0f;

    if (settle)
    {
        channel.lastLeft = toLeft;
        channel.lastRight = toRight;
        channel.lastSide = toSide;
    }

    auto* voicesLeft = voices.getWritePointer (0);
    auto* voicesRight = voices.getWritePointer (1);
    const auto inverse = 1.0f / static_cast<float> (n);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<float> (i + 1) * inverse;
        voicesLeft[i] += left[i] * (channel.lastLeft + (toLeft - channel.lastLeft) * t);
        voicesRight[i] += right[i] * (channel.lastRight + (toRight - channel.lastRight) * t);
        side[static_cast<size_t> (i)] += (left[i] + right[i]) * (channel.lastSide + (toSide - channel.lastSide) * t);
    }

    channel.lastLeft = toLeft;
    channel.lastRight = toRight;
    channel.lastSide = toSide;
}

float Mixer::crossfaderGain (int assign) const noexcept
{
    if (assign == 1)
        return 1.0f;   // Thru: the crossfader leaves it alone

    // How far the crossfader is toward the other side: 0 at the deck's own end, 1 at the far end.
    const auto x = juce::jlimit (-1.0f, 1.0f, settings.get (crossfader));
    const auto away = assign == 0 ? (x + 1.0f) * 0.5f : (1.0f - x) * 0.5f;

    switch (static_cast<int> (settings.get (crossCurve)))
    {
        case 0:  return std::cos (away * halfPi);                                                // Smooth: -3 dB in the middle
        case 2:  return away < 0.94f ? 1.0f : juce::jmax (0.0f, (1.0f - away) / 0.06f);          // Cut: for scratching
        default: return away <= 0.5f ? 1.0f : std::sin ((1.0f - away) * pi);                     // Full: both whole in the middle
    }
}

void Mixer::playPads (float* left, float* right, int numSamples) noexcept
{
    const auto fade = 1.0f / static_cast<float> (juce::jmax (1.0, 0.003 * sampleRate));

    for (auto& pad : pads)
    {
        const auto command = pad.command.exchange (0);
        const auto* sound = pad.sound;

        if (command == Pad::playCommand && sound != nullptr && sound->frames > 1)
        {
            pad.at = 0.0;
            pad.level = 1.0f;
            pad.stopping = false;
            pad.playing.store (true);
        }
        else if (command == Pad::stopCommand && pad.playing.load())
        {
            pad.stopping = true;
        }

        if (! pad.playing.load() || sound == nullptr)
            continue;

        const auto step = sound->playbackRate / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            if (pad.stopping)
            {
                pad.level -= fade;

                if (pad.level <= 0.0f)
                {
                    pad.playing.store (false);
                    break;
                }
            }

            const auto frame = static_cast<juce::int64> (pad.at);

            if (frame >= sound->frames - 1)
            {
                pad.playing.store (false);
                break;
            }

            const auto t = static_cast<float> (pad.at - static_cast<double> (frame));
            left[i] += pad.level * (sound->sampleAt (0, frame) + (sound->sampleAt (0, frame + 1) - sound->sampleAt (0, frame)) * t);
            right[i] += pad.level * (sound->sampleAt (1, frame) + (sound->sampleAt (1, frame + 1) - sound->sampleAt (1, frame)) * t);
            pad.at += step;
        }
    }
}

void Mixer::finish (float* left, float* right, int numSamples, float* phonesLeft, float* phonesRight) noexcept
{
    const auto n = juce::jmin (numSamples, blockSamples);

    if (n <= 0)
        return;

    auto* musicLeft = music.getWritePointer (0);
    auto* musicRight = music.getWritePointer (1);
    const auto inverse = 1.0f / static_cast<float> (n);
    const auto count = getDeckCount();
    const bool one = onePlayer.load (std::memory_order_relaxed);

    // Beats lined up first, from where every deck is at this block's start.
    for (int d = 0; d < count; ++d)
        decks[static_cast<size_t> (d)].lineUp (decks);

    // The decks, through the crossfader. Decks not shown rest where they are.
    for (int d = 0; d < maxDecks; ++d)
    {
        auto& deck = decks[static_cast<size_t> (d)];
        auto& last = lastCross[static_cast<size_t> (d)];

        if (d >= count)
        {
            last = 0.0f;

            // Out of sight while it records: silence, so its file keeps time with the others.
            if (deck.recorder != nullptr)
            {
                scratch.clear (0, n);
                deck.recorder->write (scratch.getArrayOfReadPointers(), n);
            }

            continue;
        }

        // The playlist's one player: deck 2 through deck 1's channel, and no crossfader for either,
        // nor a fader of its own (the master's is the one).
        deck.playThrough (one && d == 1 ? &decks[0].settings : nullptr, one && d < 2);
        deck.process (n, settle);

        const auto target = one && d < 2 ? 1.0f : crossfaderGain (static_cast<int> (deck.settings.get (deckAssign)));
        const auto from = settle ? target : last;
        const auto* deckLeft = deck.getOutput (0);
        const auto* deckRight = deck.getOutput (1);
        auto* takeLeft = scratch.getWritePointer (0);
        auto* takeRight = scratch.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            const auto g = from + (target - from) * static_cast<float> (i + 1) * inverse;
            takeLeft[i] = deckLeft[i] * g;
            takeRight[i] = deckRight[i] * g;
            musicLeft[i] += takeLeft[i];
            musicRight[i] += takeRight[i];
        }

        last = target;

        if (deck.recorder != nullptr)
        {
            if (playbackMuted.load (std::memory_order_relaxed))
                scratch.clear (0, n);   // playback muted: what goes out is silence, and so is its stem

            deck.recorder->write (scratch.getArrayOfReadPointers(), n);
        }

        // Cued: in the headphones, before its fader and the crossfader.
        if (const auto* cueLeft = deck.getCue (0))
        {
            cueBus.addFrom (0, 0, cueLeft, n);
            cueBus.addFrom (1, 0, deck.getCue (1), n);
            anyCued = true;
        }
    }

    // The music makes room for the voices (the effects channel's dynamic EQ).
    ducker.process (musicLeft, musicRight, side.data(), n, settings, settings.isOn (duckOn));

    // The pads, on their own fader (never ducked).
    padBus.clear (0, n);
    auto* padsLeft = padBus.getWritePointer (0);
    auto* padsRight = padBus.getWritePointer (1);
    playPads (padsLeft, padsRight, n);

    // Together, through the master fader and the master's effects (playback muted: the decks
    // and the pads silent, the voices still heard).
    const auto master = dbToGain (settings.get (masterVolume), silentDb);
    const auto padGain = dbToGain (settings.get (padVolume), silentDb);
    const auto playback = playbackMuted.load (std::memory_order_relaxed) ? 0.0f : 1.0f;
    const auto from = settle ? master : lastMaster;
    const auto padFrom = settle ? padGain : lastPads;
    const auto playbackFrom = settle ? playback : lastPlayback;
    const auto* voicesLeft = voices.getReadPointer (0);
    const auto* voicesRight = voices.getReadPointer (1);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<float> (i + 1) * inverse;
        const auto g = from + (master - from) * t;
        const auto b = playbackFrom + (playback - playbackFrom) * t;
        const auto p = (padFrom + (padGain - padFrom) * t) * b;
        left[i] = (musicLeft[i] * b + voicesLeft[i] + padsLeft[i] * p) * g;
        right[i] = (musicRight[i] * b + voicesRight[i] + padsRight[i] * p) * g;
    }

    lastMaster = master;
    lastPads = padGain;
    lastPlayback = playback;

    // Its effects: the compressor, the drive and the limiter (nothing delayed).
    effects.process (left, right, n, settings, settle);
    meter.measure (left, right, n, sampleRate, effects.getBlockReduction());

    // The night, as it's heard.
    if (recorder != nullptr)
    {
        const float* const channels[] = { left, right };
        recorder->write (channels, n);
        recordedSamples.fetch_add (n, std::memory_order_relaxed);
    }

    // And the live stream, while streaming is enabled (else silence, faded to).
    const auto streamGain = streamOpen.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
    const auto streamFrom = settle ? streamGain : lastStream;

    if (tap != nullptr)
    {
        if (streamGain >= 1.0f && streamFrom >= 1.0f)
        {
            tap->push (left, right, n);
        }
        else
        {
            auto* outLeft = scratch.getWritePointer (0);
            auto* outRight = scratch.getWritePointer (1);

            for (int i = 0; i < n; ++i)
            {
                const auto s = streamFrom + (streamGain - streamFrom) * static_cast<float> (i + 1) * inverse;
                outLeft[i] = left[i] * s;
                outRight[i] = right[i] * s;
            }

            tap->push (outLeft, outRight, n);
        }
    }

    lastStream = streamGain;

    if (phonesLeft != nullptr && phonesRight != nullptr)
        playPhones (left, right, phonesLeft, phonesRight, n);

    settle = false;
}

void Mixer::playPhones (const float* left, const float* right, float* phonesLeft, float* phonesRight, int numSamples) noexcept
{
    const auto n = numSamples;
    const auto inverse = 1.0f / static_cast<float> (n);
    const bool split = anyCued && settings.isOn (phonesSplit);
    const auto mix = juce::jlimit (0.0f, 1.0f, settings.get (phonesMix));

    // What's cued against the show: by the Mix knob (the middle: both, a little under), all
    // the cue split to the left and the show to the right, or the show alone when nothing's cued.
    const auto cueShare = ! anyCued ? 0.0f : split ? 1.0f : std::cos (mix * halfPi);
    const auto showShare = ! anyCued || split ? 1.0f : std::sin (mix * halfPi);
    const auto level = dbToGain (settings.get (phonesLevel), silentDb);

    if (settle)
    {
        lastCueShare = cueShare;
        lastShowShare = showShare;
        lastPhones = level;
    }

    const auto* cueLeft = cueBus.getReadPointer (0);
    const auto* cueRight = cueBus.getReadPointer (1);

    for (int i = 0; i < n; ++i)
    {
        const auto t = static_cast<float> (i + 1) * inverse;
        const auto c = lastCueShare + (cueShare - lastCueShare) * t;
        const auto m = lastShowShare + (showShare - lastShowShare) * t;
        const auto g = lastPhones + (level - lastPhones) * t;

        if (split)
        {
            phonesLeft[i] = 0.5f * (cueLeft[i] + cueRight[i]) * c * g;
            phonesRight[i] = 0.5f * (left[i] + right[i]) * m * g;
        }
        else
        {
            phonesLeft[i] = (cueLeft[i] * c + left[i] * m) * g;
            phonesRight[i] = (cueRight[i] * c + right[i] * m) * g;
        }
    }

    lastCueShare = cueShare;
    lastShowShare = showShare;
    lastPhones = level;

    // Never too loud for the ears: their own limiter.
    phonesLimiter.process (phonesLeft, phonesRight, n);
    juce::int64 limited = 0;
    const auto total = phonesLimiter.takeReduction (limited);
    phonesMeter.measure (phonesLeft, phonesRight, n, sampleRate, limited > 0 ? static_cast<float> (total / static_cast<double> (limited)) : 0.0f);
}
}
