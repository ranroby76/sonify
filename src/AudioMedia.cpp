// C:\workspace\Fanan_web_hub\src\AudioMedia.cpp

#include "AudioMedia.h"

#include <atomic>
#include <set>

AudioMedia::AudioMedia()
{
    formats.registerBasicFormats();
}

AudioMedia::~AudioMedia()
{
    *alive = false;
    loaders.removeAllJobs (true, 30000);
}

std::shared_ptr<AudioData> AudioMedia::decode (juce::AudioFormatManager& formats, const juce::File& file, double playbackRate, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->sampleRate <= 0.0)
    {
        error = file.existsAsFile() ? "Can't read " + file.getFileName() : file.getFullPathName() + " is missing";
        return nullptr;
    }

    const auto channels = juce::jlimit (1, 2, static_cast<int> (reader->numChannels));
    const auto length = static_cast<int> (juce::jmin<juce::int64> (reader->lengthInSamples, std::numeric_limits<int>::max() / 4));

    juce::AudioBuffer<float> raw (channels, juce::jmax (1, length));
    raw.clear();
    reader->read (&raw, 0, length, 0, true, channels > 1);

    auto data = std::make_shared<AudioData>();
    data->fileRate = reader->sampleRate;
    data->playbackRate = playbackRate > 0.0 ? playbackRate : reader->sampleRate;
    data->lengthSeconds = length / reader->sampleRate;

    // The waveform: the loudest and quietest sample of each bucket, both channels together.
    const auto buckets = (length + AudioData::peakBucket - 1) / AudioData::peakBucket;
    data->peaks.reserve (static_cast<size_t> (buckets) * 2);

    for (int bucket = 0; bucket < buckets; ++bucket)
    {
        const auto start = bucket * AudioData::peakBucket;
        const auto count = juce::jmin (AudioData::peakBucket, length - start);
        float high = 0.0f, low = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (raw.getReadPointer (channel, start), count);
            high = juce::jmax (high, range.getEnd());
            low = juce::jmin (low, range.getStart());
        }

        data->peaks.push_back (static_cast<juce::int8> (juce::jlimit (-127, 127, juce::roundToInt (high * 127.0f))));
        data->peaks.push_back (static_cast<juce::int8> (juce::jlimit (-127, 127, juce::roundToInt (low * 127.0f))));
    }

    // At the device's rate, so playing it is a straight copy.
    if (juce::approximatelyEqual (data->fileRate, data->playbackRate) || length == 0)
    {
        data->samples = std::move (raw);
    }
    else
    {
        const auto ratio = data->fileRate / data->playbackRate;   // file samples per played sample
        const auto outLength = juce::jmax (1, static_cast<int> (std::ceil (length / ratio)));
        data->samples.setSize (channels, outLength);
        data->samples.clear();

        for (int channel = 0; channel < channels; ++channel)
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.process (ratio, raw.getReadPointer (channel), data->samples.getWritePointer (channel), outLength, length, 0);
        }
    }

    return data;
}

void AudioMedia::keep (const juce::String& key, std::shared_ptr<const AudioData> data)
{
    cache[key] = std::move (data);
    touch (key);
    newlyLoaded.push_back (key);
    trim();
}

std::shared_ptr<const AudioData> AudioMedia::get (const juce::File& file, double playbackRate, juce::String& error)
{
    const auto key = file.getFullPathName();

    if (const auto found = cache.find (key); found != cache.end() && juce::approximatelyEqual (found->second->playbackRate, playbackRate))
    {
        touch (key);
        return found->second;
    }

    auto data = decode (formats, file, playbackRate, error);

    if (data == nullptr)
        return nullptr;

    keep (key, data);
    return data;
}

void AudioMedia::prefetch (const juce::StringArray& paths, double playbackRate)
{
    // Each file not in memory yet (at this rate) once, read on the loaders' threads together.
    struct Job
    {
        juce::String key;
        std::shared_ptr<AudioData> data;
    };

    std::vector<std::shared_ptr<Job>> jobs;
    std::set<juce::String> seen;

    for (const auto& path : paths)
    {
        if (path.isEmpty() || ! seen.insert (path).second)
            continue;

        if (const auto found = cache.find (path); found != cache.end() && juce::approximatelyEqual (found->second->playbackRate, playbackRate))
            continue;

        if (! juce::File (path).existsAsFile())
            continue;

        jobs.push_back (std::make_shared<Job> (Job { path, nullptr }));
    }

    if (jobs.size() < 2)
        return;   // one file: get() reads it as it comes

    juce::WaitableEvent finished;
    std::atomic<int> left { static_cast<int> (jobs.size()) };

    for (auto& job : jobs)
    {
        loaders.addJob ([this, job, playbackRate, &left, &finished]
        {
            juce::String error;
            job->data = decode (formats, juce::File (job->key), playbackRate, error);

            if (--left == 0)
                finished.signal();
        });
    }

    finished.wait (-1);

    for (const auto& job : jobs)
        if (job->data != nullptr)
            keep (job->key, job->data);
}

void AudioMedia::load (const juce::File& file, double playbackRate,
                       std::function<void (std::shared_ptr<const AudioData>, const juce::String& error)> done)
{
    const auto key = file.getFullPathName();

    if (const auto found = cache.find (key); found != cache.end() && juce::approximatelyEqual (found->second->playbackRate, playbackRate))
    {
        touch (key);

        if (done != nullptr)
            done (found->second, {});

        return;
    }

    loaders.addJob ([this, key, playbackRate, done = std::move (done), weak = std::weak_ptr<bool> (alive)]
    {
        juce::String error;
        auto data = decode (formats, juce::File (key), playbackRate, error);

        juce::MessageManager::callAsync ([this, key, playbackRate, data, error, done, weak]
        {
            if (weak.expired())
                return;

            std::shared_ptr<const AudioData> kept = data;

            // Read meanwhile by another way (get()): that one is shared, not a second copy.
            if (const auto found = cache.find (key); found != cache.end() && juce::approximatelyEqual (found->second->playbackRate, playbackRate))
                kept = found->second;
            else if (data != nullptr)
                keep (key, data);

            if (done != nullptr)
                done (kept, error);
        });
    });
}

void AudioMedia::trim()
{
    // What the files nothing plays take, oldest use first.
    std::vector<std::pair<juce::uint64, juce::String>> unused;
    size_t unusedBytes = 0;

    for (const auto& [key, data] : cache)
    {
        if (data.use_count() > 1)
            continue;   // a clip, a preview or a render holds it

        unusedBytes += static_cast<size_t> (data->samples.getNumChannels()) * static_cast<size_t> (data->samples.getNumSamples()) * sizeof (float)
                     + data->peaks.size();
        const auto used = lastUsed.find (key);
        unused.emplace_back (used != lastUsed.end() ? used->second : 0, key);
    }

    if (unusedBytes <= unusedBudget)
        return;

    std::sort (unused.begin(), unused.end());

    for (const auto& [when, key] : unused)
    {
        if (unusedBytes <= unusedBudget)
            break;

        const auto found = cache.find (key);
        const auto& data = found->second;
        unusedBytes -= static_cast<size_t> (data->samples.getNumChannels()) * static_cast<size_t> (data->samples.getNumSamples()) * sizeof (float)
                     + data->peaks.size();
        cache.erase (found);
        lastUsed.erase (key);
    }
}

bool AudioMedia::isAudioFile (const juce::File& file)
{
    return file.existsAsFile() && formats.findFormatForFileExtension (file.getFileExtension()) != nullptr;
}

std::vector<juce::int8> AudioMedia::peaksOf (const AudioData& data, int bucket)
{
    std::vector<juce::int8> peaks;
    const auto length = data.samples.getNumSamples();
    const auto channels = data.samples.getNumChannels();
    bucket = juce::jmax (1, bucket);
    peaks.reserve (static_cast<size_t> ((length + bucket - 1) / bucket) * 2);

    for (int start = 0; start < length; start += bucket)
    {
        const auto count = juce::jmin (bucket, length - start);
        float high = 0.0f, low = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (data.samples.getReadPointer (channel, start), count);
            high = juce::jmax (high, range.getEnd());
            low = juce::jmin (low, range.getStart());
        }

        peaks.push_back (static_cast<juce::int8> (juce::jlimit (-127, 127, juce::roundToInt (high * 127.0f))));
        peaks.push_back (static_cast<juce::int8> (juce::jlimit (-127, 127, juce::roundToInt (low * 127.0f))));
    }

    return peaks;
}

double AudioMedia::lengthOf (const juce::File& file, double playbackRate)
{
    juce::String error;
    const auto data = get (file, playbackRate, error);
    return data != nullptr ? data->lengthSeconds : 0.0;
}
