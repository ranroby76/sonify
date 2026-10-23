// D:\workspace\Fanan_web_hub\src\PodcastExport.cpp

#include "PodcastExport.h"
#include "Mp3Encoder.h"
#include "SonifyDsp.h"

#include <cmath>

namespace
{
    constexpr int blockSize = 32768;

    // How far the whole recording may be turned: a recording this far off is something else.
    constexpr double mostGainDb = 40.0;

    /** Reads the file a block at a time as stereo (a mono one on both sides). */
    class Blocks
    {
    public:
        Blocks (juce::AudioFormatReader& r)
            : reader (r), buffer (2, blockSize)
        {
        }

        juce::int64 total() const noexcept     { return reader.lengthInSamples; }

        /** The next block (count samples, 0 at the end). */
        int next()
        {
            const auto count = static_cast<int> (juce::jmin<juce::int64> (blockSize, reader.lengthInSamples - at));

            if (count <= 0)
                return 0;

            buffer.clear();
            reader.read (&buffer, 0, count, at, true, reader.numChannels > 1);

            if (reader.numChannels < 2)
                buffer.copyFrom (1, 0, buffer, 0, 0, count);

            at += count;
            return count;
        }

        void rewind() noexcept                 { at = 0; }

        float* left() noexcept                 { return buffer.getWritePointer (0); }
        float* right() noexcept                { return buffer.getWritePointer (1); }

    private:
        juce::AudioFormatReader& reader;
        juce::AudioBuffer<float> buffer;
        juce::int64 at = 0;
    };

    /** Where the turned, limited sound goes as each block is made (the limiter's delay taken
        out): to a meter, or to the file. */
    using Out = std::function<bool (const float* left, const float* right, int count)>;

    /** The whole recording turned by gainDb through the limiter, into out. */
    bool render (Blocks& blocks, double rate, double gainDb, float ceilingDb, const Out& out,
                 const std::function<bool (double)>& report, double from, double share)
    {
        SonifyDsp::Limiter limiter;
        limiter.prepare (rate);
        limiter.setSettings (ceilingDb, 80.0f);

        const auto gain = static_cast<float> (juce::Decibels::decibelsToGain (gainDb));
        const auto delay = limiter.getLatency();
        juce::int64 skipped = 0;
        juce::int64 made = 0;
        blocks.rewind();

        const auto pass = [&] (float* left, float* right, int count)
        {
            limiter.process (left, right, count);

            // The limiter's first samples are its delay: what comes after them is the recording.
            auto offset = 0;

            if (skipped < delay)
            {
                offset = static_cast<int> (juce::jmin<juce::int64> (count, delay - skipped));
                skipped += offset;
            }

            return offset >= count || out (left + offset, right + offset, count - offset);
        };

        while (const auto count = blocks.next())
        {
            juce::FloatVectorOperations::multiply (blocks.left(), gain, count);
            juce::FloatVectorOperations::multiply (blocks.right(), gain, count);

            if (! pass (blocks.left(), blocks.right(), count))
                return false;

            made += count;

            if (! report (from + share * static_cast<double> (made) / static_cast<double> (juce::jmax<juce::int64> (1, blocks.total()))))
                return false;
        }

        // What the limiter still holds.
        std::vector<float> silenceLeft (static_cast<size_t> (juce::jmax (1, delay)), 0.0f), silenceRight (silenceLeft);
        return delay <= 0 || pass (silenceLeft.data(), silenceRight.data(), delay);
    }
}

//==============================================================================
PodcastExport::PodcastExport (Settings s)
    : juce::Thread ("Sonify podcast export"), settings (std::move (s))
{
}

PodcastExport::~PodcastExport()
{
    stopThread (10000);
    cancelPendingUpdate();
}

void PodcastExport::run()
{
    result = make (settings, [this] (double p)
    {
        progress = p;
        return ! threadShouldExit();
    });

    progress = 1.0;
    done = true;
    triggerAsyncUpdate();
}

void PodcastExport::handleAsyncUpdate()
{
    if (onDone != nullptr)
        onDone();
}

PodcastExport::Result PodcastExport::make (const Settings& settings, const std::function<bool (double)>& report)
{
    Result result;
    const auto fail = [&result] (const juce::String& why) { result.error = why; return result; };

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (settings.source));

    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return fail (settings.source.existsAsFile() ? "Sonify can't read " + settings.source.getFileName() + "."
                                                    : settings.source.getFullPathName() + " isn't there.");

    const auto rate = reader->sampleRate;
    Blocks blocks (*reader);

    // 1. Its loudness as it is.
    {
        SonifyDsp::LoudnessMeter meter;
        meter.prepare (rate);
        juce::int64 read = 0;

        while (const auto count = blocks.next())
        {
            meter.process (blocks.left(), blocks.right(), count);
            read += count;

            if (! report (0.3 * static_cast<double> (read) / static_cast<double> (blocks.total())))
                return fail ("Cancelled");
        }

        result.measuredLufs = meter.getIntegrated();
    }

    if (result.measuredLufs < -70.0)
        return fail (settings.source.getFileName() + " is silent: there's nothing to make loud.");

    auto gainDb = juce::jlimit (-mostGainDb, mostGainDb, settings.lufs - result.measuredLufs);

    // 2. Turned and limited: what the limiter takes off is given back once.
    {
        SonifyDsp::LoudnessMeter meter;
        meter.prepare (rate);

        const auto measured = render (blocks, rate, gainDb, settings.ceilingDb, [&meter] (const float* l, const float* r, int count)
        {
            meter.process (l, r, count);
            return true;
        }, report, 0.3, 0.3);

        if (! measured)
            return fail ("Cancelled");

        const auto missing = settings.lufs - meter.getIntegrated();

        if (missing > 0.1)
            gainDb = juce::jlimit (-mostGainDb, mostGainDb, gainDb + missing);
    }

    result.gainDb = gainDb;

    // 3. Written: MP3, or WAV without LAME (or when a .wav is asked for).
    const bool wantMp3 = ! settings.target.hasFileExtension ("wav") && Mp3Encoder::problem().isEmpty();
    result.wroteWav = ! wantMp3;
    result.file = wantMp3 ? settings.target.withFileExtension ("mp3") : settings.target.withFileExtension ("wav");

    // A WAV in place of the MP3 asked for doesn't replace one that's there.
    if (result.file != settings.target && result.file.exists())
        result.file = result.file.getNonexistentSibling (false);

    result.file.getParentDirectory().createDirectory();

    const auto temporary = result.file.getSiblingFile (result.file.getFileNameWithoutExtension() + ".part" + result.file.getFileExtension());
    temporary.deleteFile();

    SonifyDsp::LoudnessMeter finalMeter;
    finalMeter.prepare (rate);
    SonifyDsp::TruePeakMeter peaks;
    peaks.reset();

    Mp3Encoder mp3;
    std::unique_ptr<juce::AudioFormatWriter> wav;

    if (wantMp3)
    {
        if (const auto opened = mp3.open (temporary, rate, settings.kbps); opened.failed())
            return fail (opened.getErrorMessage());
    }
    else
    {
        std::unique_ptr<juce::OutputStream> stream = temporary.createOutputStream();

        if (stream != nullptr)
        {
            juce::WavAudioFormat format;
            wav = format.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                   .withNumChannels (2)
                                                                                   .withBitsPerSample (24));
        }

        if (wav == nullptr)
            return fail ("Sonify can't write " + result.file.getFullPathName() + ".");
    }

    const auto written = render (blocks, rate, gainDb, settings.ceilingDb, [&] (const float* l, const float* r, int count)
    {
        finalMeter.process (l, r, count);
        peaks.process (l, r, count);

        if (wantMp3)
            return mp3.write (l, r, count);

        const float* const channels[] = { l, r };
        return wav->writeFromFloatArrays (channels, 2, count);
    }, report, 0.6, 0.4);

    const auto closed = wantMp3 ? mp3.close() : juce::Result::ok();
    wav = nullptr;   // closes the WAV

    if (! written || closed.failed())
    {
        temporary.deleteFile();
        return fail (! written && closed.wasOk() ? juce::String ("Cancelled")
                                                 : "Sonify couldn't finish " + result.file.getFileName() + (closed.failed() ? ": " + closed.getErrorMessage() : "."));
    }

    result.file.deleteFile();

    if (! temporary.moveFileTo (result.file))
    {
        temporary.deleteFile();
        return fail ("Sonify couldn't put " + result.file.getFileName() + " in place (is it open somewhere?).");
    }

    result.finalLufs = finalMeter.getIntegrated();
    result.truePeakDb = peaks.getPeakDb();
    result.ok = true;
    report (1.0);
    return result;
}
