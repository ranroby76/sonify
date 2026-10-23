// D:\workspace\Fanan_web_hub\src\AudioToMidi.cpp

#include "AudioToMidi.h"

#if __has_include (<onnxruntime_c_api.h>)
 #include <onnxruntime_c_api.h>
 #define SONIFY_ONNXRUNTIME 1
#else
 #define SONIFY_ONNXRUNTIME 0
#endif

#if __has_include ("SonifyModelData.h")
 #include "SonifyModelData.h"
 #define SONIFY_BASIC_PITCH 1
#else
 #define SONIFY_BASIC_PITCH 0
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
    /** The clip's window as one mono signal at its file's rate. */
    std::vector<float> readMono (const AudioToMidi::Job& job, double& rate, juce::String& error)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (job.file));

        if (reader == nullptr || reader->sampleRate <= 0.0)
        {
            error = "The clip's audio can't be read: " + job.file.getFileName();
            return {};
        }

        rate = reader->sampleRate;
        const auto from = juce::jlimit<juce::int64> (0, reader->lengthInSamples, static_cast<juce::int64> (std::llround (job.offset * rate)));
        const auto seconds = juce::jmin (job.length, AudioToMidi::longest);
        const auto count = static_cast<int> (juce::jlimit<juce::int64> (0, reader->lengthInSamples - from, static_cast<juce::int64> (std::llround (seconds * rate))));

        if (count <= 0)
        {
            error = "There's no audio in the clip.";
            return {};
        }

        const auto channels = static_cast<int> (juce::jlimit (1u, 2u, reader->numChannels));
        juce::AudioBuffer<float> buffer (channels, count);
        reader->read (&buffer, 0, count, from, true, channels > 1);

        std::vector<float> mono (static_cast<size_t> (count));

        for (int i = 0; i < count; ++i)
        {
            float sum = 0.0f;
            for (int c = 0; c < channels; ++c)
                sum += buffer.getSample (c, i);
            mono[static_cast<size_t> (i)] = sum / static_cast<float> (channels);
        }

        return mono;
    }

    /** A Blackman-windowed sinc resampler, its cutoff a little under the lower Nyquist, its kernel
        tabled finely (looked up between entries). Says how far it got now and then; stopped, it
        gives back nothing. */
    std::vector<float> resample (const std::vector<float>& x, double from, double to, const AudioToMidi::Report& report)
    {
        if (std::abs (from - to) < 1.0e-9)
            return x;

        const auto ratio = from / to;                                // input samples per output sample
        const auto cutoff = 0.95 * std::min (1.0, 1.0 / ratio);     // of the input's Nyquist
        constexpr int zeros = 12;
        constexpr int perUnit = 512;                                 // table entries a sample
        const auto reach = zeros / cutoff;
        const auto length = static_cast<int> (x.size());
        const auto outLength = static_cast<int> (std::floor (length / ratio));
        std::vector<float> out (static_cast<size_t> (std::max (0, outLength)));

        std::vector<float> table (static_cast<size_t> (std::ceil (reach * perUnit)) + 2, 0.0f);

        for (size_t i = 0; i < table.size(); ++i)
        {
            const auto t = static_cast<double> (i) / perUnit;
            if (t >= reach) break;
            const auto s = t < 1.0e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * cutoff * t) / (juce::MathConstants<double>::pi * cutoff * t);
            const auto w = 0.42 + 0.5 * std::cos (juce::MathConstants<double>::pi * t / reach) + 0.08 * std::cos (2.0 * juce::MathConstants<double>::pi * t / reach);
            table[i] = static_cast<float> (cutoff * s * w);
        }

        const auto kernel = [&table] (double t)
        {
            const auto at = std::abs (t) * perUnit;
            const auto i = static_cast<size_t> (at);
            if (i + 1 >= table.size()) return 0.0f;
            const auto frac = static_cast<float> (at - static_cast<double> (i));
            return table[i] + (table[i + 1] - table[i]) * frac;
        };

        for (int j = 0; j < outLength; ++j)
        {
            if (report != nullptr && (j & 16383) == 0 && ! report (0.05 + 0.05 * j / outLength))
                return {};

            const auto centre = j * ratio;
            const auto first = std::max (0, static_cast<int> (std::ceil (centre - reach)));
            const auto last = std::min (length - 1, static_cast<int> (std::floor (centre + reach)));
            float sum = 0.0f;

            for (int i = first; i <= last; ++i)
                sum += x[static_cast<size_t> (i)] * kernel (centre - i);

            out[static_cast<size_t> (j)] = sum;
        }

        return out;
    }

   #if SONIFY_ONNXRUNTIME
    /** ONNX Runtime, loaded once, when it's first needed (as Split into Stems loads it). */
    struct Runtime
    {
        Runtime()
        {
            const auto folder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

           #if JUCE_WINDOWS
            const auto file = folder.getChildFile ("onnxruntime.dll");
           #elif JUCE_MAC
            const auto file = folder.getChildFile ("libonnxruntime.dylib");
           #else
            const auto file = folder.getChildFile ("libonnxruntime.so");
           #endif

            auto path = juce::SystemStats::getEnvironmentVariable ("SONIFY_ONNXRUNTIME", {});

            if (path.isEmpty())
            {
                if (! file.existsAsFile())
                {
                    problem = "ONNX Runtime (" + file.getFileName() + ") isn't next to Sonify: building Sonify puts it there.";
                    return;
                }

                path = file.getFullPathName();
            }

            if (! library.open (path))
            {
                problem = "ONNX Runtime (" + juce::File (path).getFileName() + ") couldn't be loaded.";
                return;
            }

            using GetApiBase = const OrtApiBase* (ORT_API_CALL*)();
            const auto getBase = reinterpret_cast<GetApiBase> (library.getFunction ("OrtGetApiBase"));
            const auto* base = getBase != nullptr ? getBase() : nullptr;
            api = base != nullptr ? base->GetApi (ORT_API_VERSION) : nullptr;

            if (api == nullptr)
                problem = "This ONNX Runtime is older than Sonify needs.";
        }

        juce::DynamicLibrary library;
        const OrtApi* api = nullptr;
        juce::String problem;
    };

    Runtime& runtime()
    {
        static Runtime instance;
        return instance;
    }

    juce::String errorOf (const OrtApi* api, OrtStatus* status)
    {
        if (status == nullptr)
            return {};

        juce::String message (api->GetErrorMessage (status));
        api->ReleaseStatus (status);
        return message.isNotEmpty() ? message : juce::String ("ONNX Runtime failed.");
    }

    /** Basic Pitch's model in a session: a 2-second window in, its notes' and onsets' activations out. */
    class Model
    {
    public:
        explicit Model (const OrtApi* a) : api (a) {}

        ~Model()
        {
            if (session != nullptr)  api->ReleaseSession (session);
            if (options != nullptr)  api->ReleaseSessionOptions (options);
            if (memory != nullptr)   api->ReleaseMemoryInfo (memory);
            if (env != nullptr)      api->ReleaseEnv (env);
        }

        juce::String open (const void* model, size_t bytes)
        {
            if (auto e = errorOf (api, api->CreateEnv (ORT_LOGGING_LEVEL_WARNING, "Sonify", &env)); e.isNotEmpty())                return e;
            if (auto e = errorOf (api, api->CreateSessionOptions (&options)); e.isNotEmpty())                                        return e;
            if (auto e = errorOf (api, api->SetIntraOpNumThreads (options, 2)); e.isNotEmpty())                                      return e;
            errorOf (api, api->AddSessionConfigEntry (options, "session.intra_op.allow_spinning", "0"));

            if (auto e = errorOf (api, api->CreateSessionFromArray (env, model, bytes, options, &session)); e.isNotEmpty())
                return "The note model couldn't be opened: " + e;

            return errorOf (api, api->CreateCpuMemoryInfo (OrtArenaAllocator, OrtMemTypeDefault, &memory));
        }

        /** One window (windowSamples): its frames' note and onset activations (framesPerWindow x 88 each). */
        juce::String run (std::vector<float>& window, std::vector<float>& notes, std::vector<float>& onsets, OrtRunOptions* runOptions)
        {
            const std::array<int64_t, 3> shape { 1, AudioToMidi::windowSamples, 1 };
            OrtValue* in = nullptr;
            OrtValue* out[2] = { nullptr, nullptr };

            auto e = errorOf (api, api->CreateTensorWithDataAsOrtValue (memory, window.data(), window.size() * sizeof (float), shape.data(), shape.size(),
                                                                       ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &in));
            const char* inputNames[1] = { "serving_default_input_2:0" };
            const char* outputNames[2] = { "StatefulPartitionedCall:1", "StatefulPartitionedCall:2" };   // notes, onsets

            if (e.isEmpty())
                e = errorOf (api, api->Run (session, runOptions, inputNames, &in, 1, outputNames, 2, out));

            for (int i = 0; e.isEmpty() && i < 2; ++i)
            {
                OrtTensorTypeAndShapeInfo* info = nullptr;
                size_t count = 0;
                e = errorOf (api, api->GetTensorTypeAndShape (out[i], &info));

                if (e.isEmpty())
                {
                    errorOf (api, api->GetTensorShapeElementCount (info, &count));
                    api->ReleaseTensorTypeAndShapeInfo (info);
                }

                float* data = nullptr;

                if (e.isEmpty())
                    e = errorOf (api, api->GetTensorMutableData (out[i], reinterpret_cast<void**> (&data)));

                if (e.isEmpty())
                {
                    if (count != static_cast<size_t> (AudioToMidi::framesPerWindow * AudioToMidi::keys))
                        e = "That isn't the note model Sonify knows.";
                    else
                        (i == 0 ? notes : onsets).assign (data, data + count);
                }
            }

            if (in != nullptr) api->ReleaseValue (in);
            for (auto* v : out) if (v != nullptr) api->ReleaseValue (v);
            return e;
        }

    private:
        const OrtApi* api;
        OrtEnv* env = nullptr;
        OrtSessionOptions* options = nullptr;
        OrtSession* session = nullptr;
        OrtMemoryInfo* memory = nullptr;
    };
   #endif
}

//==============================================================================
AudioToMidi::AudioToMidi (Job j)
    : juce::Thread ("Sonify audio to MIDI"), job (std::move (j))
{
}

AudioToMidi::~AudioToMidi()
{
    cancel();
    stopThread (15000);
}

void AudioToMidi::cancel()
{
    // The model hears two seconds at a time, quickly: it stops after the one under way.
    signalThreadShouldExit();
}

void AudioToMidi::run()
{
    try
    {
        result = transcribe (job, [this] (double p)
        {
            progress.store (p);
            return ! threadShouldExit();
        });
    }
    catch (const std::bad_alloc&)
    {
        result = {};
        result.error = "There isn't enough memory to hear the clip.";
    }

    if (threadShouldExit() && ! result.ok)
        result.error = "Cancelled";

    done.store (true);
}

//==============================================================================
juce::String AudioToMidi::notesProblem()
{
   #if ! SONIFY_BASIC_PITCH
    return "This build of Sonify has no note model: build it again with its CMakeLists.txt.";
   #elif ! SONIFY_ONNXRUNTIME
    return "This build of Sonify has no ONNX Runtime: build it again with its CMakeLists.txt.";
   #else
    return runtime().problem;
   #endif
}

double AudioToMidi::frameTime (int frame)
{
    // As basic-pitch's model_frames_to_time: each window's frames are 2 s of 86 a second, less
    // the bit the window is short of that, and a measured alignment.
    const auto windowOffset = (fftHop / modelRate) * (framesPerWindow - windowSamples / static_cast<double> (fftHop)) + 0.0018;
    return frame * fftHop / modelRate - windowOffset * std::floor (frame / static_cast<double> (framesPerWindow));
}

//==============================================================================
std::vector<AudioToMidi::FrameNote> AudioToMidi::notesFromActivations (std::vector<float> frames, std::vector<float> onsets, int numFrames,
                                                                     float onsetThreshold, float frameThreshold, int minNoteFrames,
                                                                     bool inferOnsets, bool melodiaTrick, int energyTolerance)
{
    // output_to_notes_polyphonic, step by step.
    std::vector<FrameNote> notes;
    const auto at = [] (int t, int k) { return static_cast<size_t> (t * keys + k); };

    if (numFrames <= 0)
        return notes;

    if (inferOnsets)
    {
        // Onsets also where a key's frame activation jumps up (over one and two frames).
        constexpr int diffs = 2;
        std::vector<float> diff (frames.size(), 0.0f);
        float maxDiff = 0.0f, maxOnset = 0.0f;

        for (int t = 0; t < numFrames; ++t)
        {
            for (int k = 0; k < keys; ++k)
            {
                float smallest = std::numeric_limits<float>::max();

                for (int n = 1; n <= diffs; ++n)
                    smallest = std::min (smallest, frames[at (t, k)] - (t - n >= 0 ? frames[at (t - n, k)] : 0.0f));

                const auto d = t < diffs ? 0.0f : std::max (0.0f, smallest);
                diff[at (t, k)] = d;
                maxDiff = std::max (maxDiff, d);
                maxOnset = std::max (maxOnset, onsets[at (t, k)]);
            }
        }

        if (maxDiff > 0.0f)
            for (size_t i = 0; i < onsets.size(); ++i)
                onsets[i] = std::max (onsets[i], maxOnset * diff[i] / maxDiff);
    }

    // Peaks in time of each key's onsets, at least the threshold, latest first.
    std::vector<std::pair<int, int>> starts;

    for (int t = 1; t + 1 < numFrames; ++t)
        for (int k = 0; k < keys; ++k)
        {
            const auto v = onsets[at (t, k)];
            if (v >= onsetThreshold && v > onsets[at (t - 1, k)] && v > onsets[at (t + 1, k)])
                starts.push_back ({ t, k });
        }

    std::sort (starts.begin(), starts.end(), [] (const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second > b.second; });

    auto remaining = frames;

    const auto clearAround = [&remaining, &at] (int t, int k)
    {
        remaining[at (t, k)] = 0.0f;
        if (k < keys - 1) remaining[at (t, k + 1)] = 0.0f;
        if (k > 0)        remaining[at (t, k - 1)] = 0.0f;
    };

    const auto mean = [&frames, &at] (int from, int to, int k)
    {
        double sum = 0.0;
        for (int t = from; t < to; ++t) sum += frames[at (t, k)];
        return static_cast<float> (to > from ? sum / (to - from) : 0.0);
    };

    for (const auto& [startFrame, key] : starts)
    {
        if (startFrame >= numFrames - 1)
            continue;

        // On until the key's activation has been under the frame threshold energyTolerance frames.
        int i = startFrame + 1, k = 0;

        while (i < numFrames - 1 && k < energyTolerance)
        {
            k = remaining[at (i, key)] < frameThreshold ? k + 1 : 0;
            ++i;
        }

        i -= k;

        if (i - startFrame <= minNoteFrames)
            continue;

        for (int t = startFrame; t < i; ++t)
            clearAround (t, key);

        notes.push_back ({ startFrame, i, key, mean (startFrame, i, key) });
    }

    if (melodiaTrick)
    {
        // Notes without onsets: from the strongest activation left, out both ways. The strongest
        // left is the strongest of the cells not yet cleared (values only ever go to zero).
        std::vector<std::pair<float, size_t>> cells;

        for (size_t c = 0; c < remaining.size(); ++c)
            if (remaining[c] > frameThreshold)
                cells.push_back ({ remaining[c], c });

        std::sort (cells.begin(), cells.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });

        for (const auto& [value, cell] : cells)
        {
            if (! (remaining[cell] > frameThreshold))
                continue;

            const auto middle = static_cast<int> (cell / keys), key = static_cast<int> (cell % keys);
            remaining[cell] = 0.0f;

            int i = middle + 1, k = 0;

            while (i < numFrames - 1 && k < energyTolerance)
            {
                k = remaining[at (i, key)] < frameThreshold ? k + 1 : 0;
                clearAround (i, key);
                ++i;
            }

            const auto end = i - 1 - k;
            i = middle - 1;
            k = 0;

            while (i > 0 && k < energyTolerance)
            {
                k = remaining[at (i, key)] < frameThreshold ? k + 1 : 0;
                clearAround (i, key);
                --i;
            }

            const auto start = i + 1 + k;

            if (end - start <= minNoteFrames)
                continue;

            notes.push_back ({ start, end, key, mean (start, end, key) });
        }
    }

    return notes;
}

//==============================================================================
std::vector<AudioToMidi::Note> AudioToMidi::drumHits (const std::vector<float>& mono, double rate)
{
    std::vector<Note> hits;

    if (mono.empty() || rate <= 0.0)
        return hits;

    // Three bands: the low end (kick), the body (snare), the top (hi-hat). Each edge stays under
    // the recording's highest frequency (half its rate): past it a filter isn't stable.
    const auto edge = [rate] (double hz) { return std::min (hz, rate * 0.45); };
    juce::IIRFilter low, midLow, midHigh, high;
    low.setCoefficients (juce::IIRCoefficients::makeLowPass (rate, edge (150.0)));
    midLow.setCoefficients (juce::IIRCoefficients::makeHighPass (rate, edge (180.0)));
    midHigh.setCoefficients (juce::IIRCoefficients::makeLowPass (rate, edge (2500.0)));
    high.setCoefficients (juce::IIRCoefficients::makeHighPass (rate, edge (6000.0)));

    const auto hop = std::max (1, static_cast<int> (rate * 0.005));          // 5 ms frames
    const auto count = static_cast<int> (mono.size()) / hop;
    std::vector<std::array<float, 4>> energy (static_cast<size_t> (count));  // all, low, mid, high

    for (int f = 0; f < count; ++f)
    {
        std::array<float, 4> e {};

        for (int i = f * hop; i < (f + 1) * hop; ++i)
        {
            const auto x = mono[static_cast<size_t> (i)];
            const auto l = low.processSingleSampleRaw (x);
            const auto m = midHigh.processSingleSampleRaw (midLow.processSingleSampleRaw (x));
            const auto h = high.processSingleSampleRaw (x);
            e[0] += x * x; e[1] += l * l; e[2] += m * m; e[3] += h * h;
        }

        energy[static_cast<size_t> (f)] = e;
    }

    // Hits: where the level jumps (9 dB over the quietest of the last 20 ms), at its steepest,
    // 80 ms apart at least, and not lost in the quiet (45 dB under the loudest).
    std::vector<float> level (static_cast<size_t> (count));
    float loudest = -120.0f;

    for (int f = 0; f < count; ++f)
    {
        level[static_cast<size_t> (f)] = 10.0f * std::log10 (energy[static_cast<size_t> (f)][0] / static_cast<float> (hop) + 1.0e-12f);
        loudest = std::max (loudest, level[static_cast<size_t> (f)]);
    }

    const auto rise = [&level] (int f)
    {
        auto quietest = 0.0f;
        bool any = false;

        for (int g = f - 4; g < f; ++g)
        {
            const auto v = g < 0 ? -120.0f : level[static_cast<size_t> (g)];
            quietest = any ? std::min (quietest, v) : v;
            any = true;
        }

        return level[static_cast<size_t> (f)] - quietest;
    };

    int lastHit = -1000;

    for (int f = 0; f < count; ++f)
    {
        const auto r = rise (f);

        if (r < 9.0f || level[static_cast<size_t> (f)] < loudest - 45.0f || f - lastHit < 16)
            continue;

        if ((f > 0 && r < rise (f - 1)) || (f + 1 < count && r <= rise (f + 1)))
            continue;

        lastHit = f;

        // Where it starts, to the sample: the first that reaches a tenth of the hit's peak.
        const auto from = std::max (0, (f - 1) * hop), to = std::min (static_cast<int> (mono.size()), (f + 4) * hop);
        float peak = 0.0f;
        for (int i = from; i < to; ++i) peak = std::max (peak, std::abs (mono[static_cast<size_t> (i)]));
        int first = from;
        while (first < to && std::abs (mono[static_cast<size_t> (first)]) < 0.1f * peak) ++first;

        // What its first 40 ms hold, band by band: the low end a kick, only the top a hi-hat,
        // a body with it a snare.
        std::array<float, 4> sum {};
        for (int g = f; g < std::min (count, f + 8); ++g)
            for (size_t b = 0; b < 4; ++b)
                sum[b] += energy[static_cast<size_t> (g)][b];

        const auto total = sum[1] + sum[2] + sum[3] + 1.0e-12f;
        const auto pitch = sum[1] / total > 0.5f ? 36 : (sum[2] < 0.12f * sum[3] && sum[1] < 0.05f * total) ? 42 : 38;
        const auto db = 10.0f * std::log10 (sum[0] / (8.0f * static_cast<float> (hop)) + 1.0e-12f) - loudest;
        const auto velocity = juce::jlimit (1, 127, static_cast<int> (std::lround (120.0f + db * 2.0f)));
        hits.push_back ({ first / rate, 0.1, pitch, velocity });
    }

    return hits;
}

//==============================================================================
AudioToMidi::Result AudioToMidi::transcribe (const Job& job, const Report& report)
{
    Result result;
    const auto fail = [&result] (const juce::String& error) { result.error = error; return result; };

    if (job.mode == Mode::notes)
        if (const auto problem = notesProblem(); problem.isNotEmpty())
            return fail (problem);

    double rate = 0.0;
    juce::String error;
    auto mono = readMono (job, rate, error);

    if (mono.empty())
        return fail (error);

    if (report != nullptr && ! report (0.05))
        return fail ("Cancelled");

    if (job.mode == Mode::drums)
    {
        result.notes = drumHits (mono, rate);
        result.ok = true;
        return result;
    }

   #if SONIFY_BASIC_PITCH && SONIFY_ONNXRUNTIME
    // At the model's rate, with half the windows' overlap of silence before it.
    auto audio = resample (mono, rate, modelRate, report);
    mono.clear();

    if (audio.empty())
        return fail ("Cancelled");
    const auto originalLength = static_cast<int> (audio.size());
    constexpr int overlapSamples = overlapFrames * fftHop;
    constexpr int hopSamples = windowSamples - overlapSamples;
    audio.insert (audio.begin(), static_cast<size_t> (overlapSamples / 2), 0.0f);

    if (report != nullptr && ! report (0.1))
        return fail ("Cancelled");

    Model model (runtime().api);

    if (const auto e = model.open (SonifyModelData::nmp_onnx, static_cast<size_t> (SonifyModelData::nmp_onnxSize)); e.isNotEmpty())
        return fail (e);

    // Each window's frames, less half the overlap at each end, one after another.
    constexpr int keep = framesPerWindow - overlapFrames;
    const auto windows = static_cast<int> ((audio.size() + hopSamples - 1) / hopSamples);
    std::vector<float> frames, onsets, window (static_cast<size_t> (windowSamples)), noteOut, onsetOut;
    frames.reserve (static_cast<size_t> (windows * keep * keys));
    onsets.reserve (frames.capacity());

    for (int w = 0; w < windows; ++w)
    {
        const auto start = static_cast<size_t> (w) * hopSamples;
        std::fill (window.begin(), window.end(), 0.0f);

        for (size_t i = 0; i < window.size() && start + i < audio.size(); ++i)
            window[i] = audio[start + i];

        if (const auto e = model.run (window, noteOut, onsetOut, nullptr); e.isNotEmpty())
            return fail (e);

        const auto from = static_cast<size_t> ((overlapFrames / 2) * keys), to = static_cast<size_t> ((overlapFrames / 2 + keep) * keys);
        frames.insert (frames.end(), noteOut.begin() + static_cast<std::ptrdiff_t> (from), noteOut.begin() + static_cast<std::ptrdiff_t> (to));
        onsets.insert (onsets.end(), onsetOut.begin() + static_cast<std::ptrdiff_t> (from), onsetOut.begin() + static_cast<std::ptrdiff_t> (to));

        if (report != nullptr && ! report (0.1 + 0.8 * (w + 1) / windows))
            return fail ("Cancelled");
    }

    // Trimmed to the frames the audio itself covers.
    const auto numFrames = std::min (static_cast<int> (frames.size() / keys),
                                     static_cast<int> (static_cast<double> (originalLength) / hopSamples * keep));
    frames.resize (static_cast<size_t> (numFrames * keys));
    onsets.resize (frames.size());

    const auto minNoteFrames = static_cast<int> (std::lround (127.70 / 1000.0 * (modelRate / fftHop)));

    for (const auto& n : notesFromActivations (std::move (frames), std::move (onsets), numFrames, 0.5f, 0.3f, minNoteFrames))
    {
        const auto start = frameTime (n.start), end = frameTime (n.end);

        if (end <= start)
            continue;

        result.notes.push_back ({ std::max (0.0, start), end - start, lowestKey + n.key,
                                  juce::jlimit (1, 127, static_cast<int> (std::lround (n.amplitude * 127.0f))) });
    }

    std::sort (result.notes.begin(), result.notes.end(), [] (const Note& a, const Note& b) { return a.start != b.start ? a.start < b.start : a.pitch < b.pitch; });
    result.ok = true;
    return result;
   #else
    return fail (notesProblem());
   #endif
}
