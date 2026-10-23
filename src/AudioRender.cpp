// D:\workspace\Fanan_web_hub\src\AudioRender.cpp

#include "AudioRender.h"

#if __has_include (<signalsmith-stretch/signalsmith-stretch.h>)
 #include <signalsmith-stretch/signalsmith-stretch.h>
 #define SONIFY_STRETCH 1
#else
 #define SONIFY_STRETCH 0
#endif

#include <algorithm>
#include <cmath>

//==============================================================================
AudioRender::AudioRender (Job j)
    : juce::Thread ("Sonify audio render"), job (std::move (j))
{
}

AudioRender::~AudioRender()
{
    cancel();
    stopThread (10000);
}

void AudioRender::run()
{
    try
    {
        result = render (job, [this] (double p)
        {
            progress.store (p);
            return ! threadShouldExit();
        });
    }
    catch (const std::bad_alloc&)
    {
        result = {};
        result.error = "There isn't enough memory for this render.";
    }

    done.store (true);
}

//==============================================================================
double AudioRender::windowSecondsAt (const Job& job, double t)
{
    const auto& map = job.timeMap;

    if (map.size() < 2)
        return t;

    // Straight lines between the points; past either end, the end's line goes on.
    const auto after = std::upper_bound (map.begin(), map.end(), t, [] (double value, const std::pair<double, double>& point) { return value < point.first; });
    const auto index = juce::jlimit<std::ptrdiff_t> (1, static_cast<std::ptrdiff_t> (map.size()) - 1, std::distance (map.begin(), after));
    const auto& a = map[static_cast<size_t> (index - 1)];
    const auto& b = map[static_cast<size_t> (index)];
    const auto span = b.first - a.first;
    return span > 1.0e-12 ? a.second + (b.second - a.second) * (t - a.first) / span : b.second;
}

double AudioRender::outputSecondsAt (const Job& job, double w)
{
    const auto& map = job.timeMap;

    if (map.size() < 2)
        return w;

    const auto after = std::upper_bound (map.begin(), map.end(), w, [] (double value, const std::pair<double, double>& point) { return value < point.second; });
    const auto index = juce::jlimit<std::ptrdiff_t> (1, static_cast<std::ptrdiff_t> (map.size()) - 1, std::distance (map.begin(), after));
    const auto& a = map[static_cast<size_t> (index - 1)];
    const auto& b = map[static_cast<size_t> (index)];
    const auto span = b.second - a.second;
    return span > 1.0e-12 ? a.first + (b.first - a.first) * (w - a.second) / span : b.first;
}

juce::File AudioRender::fileFor (const Job& job)
{
    juce::String key;
    key << job.source.getFullPathName() << "|" << juce::String (job.offset, 6) << "|" << juce::String (job.length, 6)
        << "|" << (job.reverse ? 1 : 0) << "|" << juce::String (job.semitones, 3) << "|" << juce::String (job.outputLength, 6);

    for (const auto& [out, in] : job.timeMap)
        key << ";" << juce::String (out, 5) << "," << juce::String (in, 5);

    juce::StringArray what;

    if (! job.timeMap.empty())
        what.add ("stretched");

    if (std::abs (job.semitones) > 1.0e-6)
        what.add ((job.semitones > 0 ? "+" : "") + juce::String (job.semitones, std::abs (job.semitones - std::round (job.semitones)) < 1.0e-6 ? 0 : 2) + " st");

    if (job.reverse)
        what.add ("reversed");

    const auto hash = juce::String::toHexString (static_cast<juce::int64> (key.hashCode64())).paddedLeft ('0', 16).substring (0, 10);
    const auto base = juce::File::createLegalFileName ((job.name.isNotEmpty() ? job.name : job.source.getFileNameWithoutExtension())
                                                      + (what.isEmpty() ? juce::String() : " (" + what.joinIntoString (", ") + ")"));
    return job.folder.getChildFile (base + " " + hash + ".wav");
}

//==============================================================================
AudioRender::Window AudioRender::originalWindowOf (const MidiClip& clip, double renderLength)
{
    if (! clip.isRender())
        return { juce::File (clip.audioFile), clip.audioOffset, clip.audioLength };

    // Following the tempo: its window in the original's beats.
    if (clip.followsTempo())
    {
        const auto perBeat = 60.0 / clip.sourceTempo;
        const auto length = juce::jlimit (0.0, clip.sourceLength, clip.length * perBeat);
        auto from = juce::jlimit (0.0, clip.sourceLength - length, clip.offset * perBeat);

        if (clip.reversed)
            from = clip.sourceLength - from - length;

        return { juce::File (clip.sourceFile), clip.sourceOffset + from, length };
    }

    // A render's time runs evenly against the original's (near enough, even through a ramp).
    if (renderLength <= 0.0)
        return { juce::File (clip.sourceFile), clip.sourceOffset, clip.sourceLength };

    const auto scale = clip.sourceLength / renderLength;
    const auto length = juce::jmin (clip.sourceLength, clip.audioLength * scale);
    auto from = juce::jlimit (0.0, clip.sourceLength - length, clip.audioOffset * scale);

    if (clip.reversed)
        from = clip.sourceLength - from - length;

    return { juce::File (clip.sourceFile), clip.sourceOffset + from, length };
}

AudioRender::Job AudioRender::jobFor (const MidiClip& clip, const Window& window, bool reverse, double semitones, double tempo,
                                      const TempoMap& map, const juce::File& folder, const juce::String& name, double leadBeats)
{
    Job job;
    job.source = window.file;
    job.offset = window.offset;
    job.length = window.length;
    job.reverse = reverse;
    job.semitones = semitones;
    job.folder = folder;
    job.name = name;

    if (tempo > 0.0)
    {
        // Each of the window's beats lands on one of the song's, from where the window starts (the
        // clip's start, less the beats before it): two points where the tempo holds, a point a
        // sixteenth where it changes. Before the song's start, its first tempo goes on back.
        const auto beats = window.length * tempo / 60.0;
        const auto anchor = clip.start - leadBeats;
        const auto secondsOf = [&map] (double ppq) { return ppq >= 0.0 ? map.secondsAt (ppq) : ppq * 60.0 / map.bpmAt (0.0); };
        const auto startSeconds = secondsOf (anchor);
        const auto endSeconds = secondsOf (anchor + beats);
        const auto steady = std::abs ((endSeconds - startSeconds) - beats * 60.0 / map.bpmAt (juce::jmax (0.0, anchor))) < 1.0e-9;
        const auto step = steady ? beats : 0.25;

        for (double b = 0.0;; b += step)
        {
            const auto at = juce::jmin (b, beats);
            job.timeMap.push_back ({ secondsOf (anchor + at) - startSeconds, at * 60.0 / tempo });

            if (at >= beats)
                break;
        }

        job.outputLength = job.timeMap.back().first;
    }

    return job;
}

//==============================================================================
void AudioRender::stretch (std::vector<std::vector<float>> input, std::vector<std::vector<float>>& output,
                           int outputSamples, double sampleRate, double semitones,
                           const std::function<double (double)>& inputAt, const Report& report)
{
    const auto channels = static_cast<int> (input.size());
    output.assign (input.size(), std::vector<float> (static_cast<size_t> (std::max (0, outputSamples)), 0.0f));

    if (channels == 0 || outputSamples <= 0)
        return;

   #if SONIFY_STRETCH
    signalsmith::stretch::SignalsmithStretch<float> stretcher (12345);   // the same render every time
    stretcher.presetDefault (channels, static_cast<float> (sampleRate));
    stretcher.setTransposeSemitones (static_cast<float> (semitones));

    // The input, with silence after it for the stretcher to read past the end.
    const auto length = static_cast<juce::int64> (input.front().size());
    const auto lead = static_cast<juce::int64> (stretcher.inputLatency() + 8 * stretcher.outputLatency() + 16384);
    auto& padded = input;

    for (auto& channel : padded)
        channel.resize (static_cast<size_t> (length + lead), 0.0f);

    // How fast the input goes by at an output sample (input samples per output sample).
    const auto rateAt = [&inputAt] (double at)
    {
        constexpr double span = 64.0;
        return juce::jlimit (0.05, 16.0, (inputAt (at + span) - inputAt (at)) / span);
    };

    std::vector<float*> in (input.size()), out (input.size());

    // To the start: the next output sample is the input's first.
    const auto first = stretcher.outputSeekLength (static_cast<float> (rateAt (0.0)));

    for (size_t c = 0; c < padded.size(); ++c)
        in[c] = padded[c].data();

    stretcher.outputSeek (in.data(), first);
    juce::int64 consumed = first;

    // Then a short stretch at a time, the input kept as far ahead of the output as the stretcher
    // needs at the rate of the moment.
    constexpr int chunk = 256;

    for (int done = 0; done < outputSamples;)
    {
        const auto count = std::min (chunk, outputSamples - done);
        const auto rate = rateAt (done);
        const auto target = inputAt (done + count) + stretcher.inputLatency() + rate * stretcher.outputLatency();
        const auto wanted = std::max<juce::int64> (0, static_cast<juce::int64> (std::llround (target)) - consumed);
        const auto take = static_cast<int> (std::min<juce::int64> (wanted, length + lead - consumed));

        for (size_t c = 0; c < padded.size(); ++c)
        {
            in[c] = padded[c].data() + consumed;
            out[c] = output[c].data() + done;
        }

        stretcher.process (in.data(), take, out.data(), count);
        consumed += take;
        done += count;

        if (report != nullptr && (done / chunk) % 64 == 0 && ! report (static_cast<double> (done) / outputSamples))
            return;
    }
   #else
    juce::ignoreUnused (sampleRate, semitones, inputAt, report);
   #endif
}

//==============================================================================
AudioRender::Result AudioRender::render (const Job& job, const Report& report)
{
    Result result;
    const auto fail = [&result] (const juce::String& error) { result.error = error; return result; };
    const auto stretched = ! job.timeMap.empty() || std::abs (job.semitones) > 1.0e-6;

   #if ! SONIFY_STRETCH
    if (stretched)
        return fail ("This build of Sonify has no time stretching: build it again with its CMakeLists.txt.");
   #endif

    if (job.length <= 0.0)
        return fail ("There's no audio to render.");

    if (job.length > longest || job.outputLength > 4.0 * longest)
        return fail ("The clip is too long to render: 30 minutes at most.");

    result.file = fileFor (job);

    // Made before (the same window, the same changes): found again.
    if (result.file.existsAsFile() && result.file.getSize() > 44)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        if (std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (result.file)); reader != nullptr && reader->sampleRate > 0.0)
        {
            result.ok = true;
            result.reused = true;
            result.lengthSeconds = static_cast<double> (reader->lengthInSamples) / reader->sampleRate;
            return result;
        }
    }

    // The window of the original, at its own rate.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (job.source));

    if (reader == nullptr || reader->sampleRate <= 0.0)
        return fail ("The clip's original can't be read: " + job.source.getFileName());

    const auto rate = reader->sampleRate;
    const auto channels = static_cast<int> (juce::jlimit (1u, 2u, reader->numChannels));
    const auto from = juce::jlimit<juce::int64> (0, reader->lengthInSamples, static_cast<juce::int64> (std::llround (job.offset * rate)));
    const auto count = static_cast<int> (juce::jlimit<juce::int64> (0, reader->lengthInSamples - from, static_cast<juce::int64> (std::llround (job.length * rate))));

    if (count <= 0)
        return fail ("There's no audio in the clip's window.");

    std::vector<std::vector<float>> input (static_cast<size_t> (channels));

    {
        juce::AudioBuffer<float> window (channels, count);
        reader->read (&window, 0, count, from, true, channels > 1);
        reader.reset();

        for (int c = 0; c < channels; ++c)
        {
            input[static_cast<size_t> (c)].assign (window.getReadPointer (c), window.getReadPointer (c) + count);

            if (job.reverse)
                std::reverse (input[static_cast<size_t> (c)].begin(), input[static_cast<size_t> (c)].end());
        }
    }

    if (report != nullptr && ! report (0.05))
        return fail ("Cancelled");

    // Time and pitch.
    std::vector<std::vector<float>> output;
    const auto outputSeconds = job.outputLength > 0.0 ? job.outputLength : job.length;
    const auto outputSamples = static_cast<int> (std::llround (outputSeconds * rate));

    if (stretched)
    {
        bool stopped = false;
        stretch (std::move (input), output, outputSamples, rate, job.semitones,
                 [&job, rate] (double sample) { return windowSecondsAt (job, sample / rate) * rate; },
                 [&report, &stopped] (double p) { stopped = report != nullptr && ! report (0.05 + 0.85 * p); return ! stopped; });

        if (stopped)
            return fail ("Cancelled");
    }
    else
    {
        output = std::move (input);
        for (auto& channel : output)
            channel.resize (static_cast<size_t> (outputSamples), 0.0f);
    }

    // Written whole to a part file, then named: a half-written render is never found as made.
    if (! job.folder.createDirectory())
        return fail ("The song's Media folder can't be written: " + job.folder.getFullPathName());

    const auto part = result.file.getSiblingFile (result.file.getFileName() + ".part");
    part.deleteFile();

    {
        std::unique_ptr<juce::OutputStream> stream (part.createOutputStream().release());

        if (stream == nullptr)
            return fail ("The render can't be written: " + part.getFullPathName());

        juce::WavAudioFormat wav;
        const auto options = juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                             .withNumChannels (channels)
                                                             .withBitsPerSample (32)
                                                             .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor (stream, options);

        if (writer == nullptr)
        {
            part.deleteFile();
            return fail ("The render can't be written: " + part.getFullPathName());
        }

        std::vector<const float*> channelData;

        for (const auto& channel : output)
            channelData.push_back (channel.data());

        if (! writer->writeFromFloatArrays (channelData.data(), channels, outputSamples))
        {
            writer.reset();
            part.deleteFile();
            return fail ("The render can't be written: " + part.getFullPathName());
        }
    }

    if (report != nullptr && ! report (0.98))
    {
        part.deleteFile();
        return fail ("Cancelled");
    }

    result.file.deleteFile();

    if (! part.moveFileTo (result.file))
    {
        part.deleteFile();
        return fail ("The render can't be written: " + result.file.getFullPathName());
    }

    result.ok = true;
    result.lengthSeconds = static_cast<double> (outputSamples) / rate;
    return result;
}
