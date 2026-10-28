// D:\workspace\Fanan_web_hub\src\StemSplitter.cpp

#include "StemSplitter.h"
#include "Preferences.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstring>

#if __has_include (<onnxruntime_c_api.h>)
 #include <onnxruntime_c_api.h>
 #define SONIFY_ONNXRUNTIME 1
#else
 #define SONIFY_ONNXRUNTIME 0
#endif

namespace
{
    constexpr double pi = 3.14159265358979323846;

    // The model's own sizes: 7.8 seconds at 44.1 kHz, a 4096-point spectrogram 1024 apart.
    constexpr int segment = 343980;
    constexpr int nfft = 4096, hop = 1024, bins = nfft / 2;
    constexpr int frames = (segment + hop - 1) / hop;               // 336
    constexpr int specPad = hop / 2 * 3;                            // 1536
    constexpr int stride = static_cast<int> (0.75 * segment);       // a quarter overlapping
    constexpr const char* modelSha256 = "da9e5101ee0804d04933974b59d8aae9c862e80e14f2f24e7c74cae76bdbe748";
    constexpr const char* sixModelSha256 = "4d22f22858ee831f9165ecc8ee63439666e4d0a99ff9088770508b94896437e9";

    //==============================================================================
    /** A complex FFT of nfft points. */
    class Fft
    {
    public:
        Fft() : twiddle (static_cast<size_t> (nfft / 2)), reversed (static_cast<size_t> (nfft))
        {
            for (int k = 0; k < nfft / 2; ++k)
                twiddle[static_cast<size_t> (k)] = std::polar (1.0, -2.0 * pi * k / nfft);

            int bits = 0;

            while ((1 << bits) < nfft)
                ++bits;

            for (int i = 0; i < nfft; ++i)
            {
                int r = 0;

                for (int b = 0; b < bits; ++b)
                    if (i & (1 << b))
                        r |= 1 << (bits - 1 - b);

                reversed[static_cast<size_t> (i)] = r;
            }
        }

        /** In place; inverse without the 1 / N. */
        void transform (std::vector<std::complex<float>>& x, bool inverse) const
        {
            for (int i = 0; i < nfft; ++i)
                if (const auto j = reversed[static_cast<size_t> (i)]; i < j)
                    std::swap (x[static_cast<size_t> (i)], x[static_cast<size_t> (j)]);

            for (int length = 2; length <= nfft; length <<= 1)
            {
                const auto half = length / 2, step = nfft / length;

                for (int start = 0; start < nfft; start += length)
                {
                    for (int k = 0; k < half; ++k)
                    {
                        auto w = twiddle[static_cast<size_t> (k * step)];

                        if (inverse)
                            w = std::conj (w);

                        auto& a = x[static_cast<size_t> (start + k)];
                        auto& b = x[static_cast<size_t> (start + k + half)];
                        const auto t = std::complex<float> (static_cast<float> (w.real() * b.real() - w.imag() * b.imag()),
                                                            static_cast<float> (w.real() * b.imag() + w.imag() * b.real()));
                        b = a - t;
                        a += t;
                    }
                }
            }
        }

    private:
        std::vector<std::complex<double>> twiddle;
        std::vector<int> reversed;
    };

    /** The periodic Hann window torch uses. */
    const std::vector<float>& window()
    {
        static const std::vector<float> w = []
        {
            std::vector<float> v (static_cast<size_t> (nfft));

            for (int n = 0; n < nfft; ++n)
                v[static_cast<size_t> (n)] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * pi * n / nfft));

            return v;
        }();

        return w;
    }

    /** Reflected padding, as torch pads ("reflect": the edge sample isn't repeated). */
    std::vector<float> reflect (const float* x, int length, int left, int right)
    {
        std::vector<float> out (static_cast<size_t> (length + left + right));

        const auto at = [x, length] (int i)
        {
            // Mirror about the ends until inside (the pads here are shorter than the signal).
            while (i < 0 || i >= length)
                i = i < 0 ? -i : 2 * (length - 1) - i;

            return x[i];
        };

        for (int i = 0; i < length + left + right; ++i)
            out[static_cast<size_t> (i)] = at (i - left);

        return out;
    }

    /** The model's spectrogram of a stereo stretch (segment samples a channel): the real and
        imaginary parts of each channel as four planes of bins x frames, as HTDemucs makes it. */
    void spectrogram (const Fft& fft, const float* left, const float* right, std::vector<float>& out)
    {
        out.assign (static_cast<size_t> (4 * bins * frames), 0.0f);

        // Padded so the frames line up with the waveform: by 3/4 of a hop and up to whole hops,
        // then by half the FFT each side (torch's centring), both by reflection.
        const auto padded = [] (const float* x)
        {
            const auto first = reflect (x, segment, specPad, specPad + frames * hop - segment);
            return reflect (first.data(), static_cast<int> (first.size()), nfft / 2, nfft / 2);
        };

        const auto l = padded (left), r = padded (right);
        const auto& w = window();
        const auto scale = 1.0f / std::sqrt (static_cast<float> (nfft));
        std::vector<std::complex<float>> z (static_cast<size_t> (nfft));

        for (int frame = 0; frame < frames; ++frame)
        {
            // Frames 2 .. 2 + frames of torch's: the two at each end only pad.
            const auto start = static_cast<size_t> ((frame + 2) * hop);

            // Both channels in one transform: left as the real part, right as the imaginary.
            for (int n = 0; n < nfft; ++n)
                z[static_cast<size_t> (n)] = { l[start + static_cast<size_t> (n)] * w[static_cast<size_t> (n)],
                                               r[start + static_cast<size_t> (n)] * w[static_cast<size_t> (n)] };

            fft.transform (z, false);

            for (int k = 0; k < bins; ++k)
            {
                const auto a = z[static_cast<size_t> (k)], m = std::conj (z[static_cast<size_t> ((nfft - k) % nfft)]);
                const auto xl = 0.5f * (a + m), xr = std::complex<float> (0.0f, -0.5f) * (a - m);
                const auto index = static_cast<size_t> (k * frames + frame);
                const auto plane = static_cast<size_t> (bins * frames);
                out[index] = xl.real() * scale;
                out[plane + index] = xl.imag() * scale;
                out[2 * plane + index] = xr.real() * scale;
                out[3 * plane + index] = xr.imag() * scale;
            }
        }
    }

    /** One source's spectrogram branch back to sound (stereo, segment samples a channel):
        the inverse of the above, as HTDemucs does it (torch's istft, then the padding cut away).
        spec: the source's four planes (real and imaginary for each channel). Adds into out. */
    void resynthesise (const Fft& fft, const float* spec, float* outLeft, float* outRight)
    {
        // The frames as torch has them: two of silence each side, the Nyquist bin silent too.
        constexpr int allFrames = frames + 4;
        constexpr int length = hop * frames + 2 * specPad;                 // what torch's istft gives
        constexpr int full = nfft + hop * (allFrames - 1);                 // before its centring is cut off
        std::vector<float> l (static_cast<size_t> (full), 0.0f), r (static_cast<size_t> (full), 0.0f), envelope (static_cast<size_t> (full), 0.0f);
        const auto& w = window();
        const auto scale = 1.0f / std::sqrt (static_cast<float> (nfft));  // sqrt (N) for the normalisation, 1 / N for the inverse
        const auto plane = static_cast<size_t> (bins * frames);
        std::vector<std::complex<float>> z (static_cast<size_t> (nfft));

        for (int frame = 0; frame < allFrames; ++frame)
        {
            const auto start = static_cast<size_t> (frame * hop);

            for (int n = 0; n < nfft; ++n)
                envelope[start + static_cast<size_t> (n)] += w[static_cast<size_t> (n)] * w[static_cast<size_t> (n)];

            const auto own = frame - 2;

            if (own < 0 || own >= frames)
                continue;   // silence

            // Both channels in one inverse transform: left real, right imaginary (each Hermitian).
            for (int k = 0; k <= nfft / 2; ++k)
            {
                std::complex<float> xl, xr;

                if (k < bins)
                {
                    // At 0 Hz only the real part counts, as in torch's inverse.
                    const auto index = static_cast<size_t> (k * frames + own);
                    xl = { spec[index], k == 0 ? 0.0f : spec[plane + index] };
                    xr = { spec[2 * plane + index], k == 0 ? 0.0f : spec[3 * plane + index] };
                }

                z[static_cast<size_t> (k)] = xl + std::complex<float> (0.0f, 1.0f) * xr;

                if (k > 0 && k < nfft / 2)
                    z[static_cast<size_t> (nfft - k)] = std::conj (xl) + std::complex<float> (0.0f, 1.0f) * std::conj (xr);
            }

            fft.transform (z, true);

            for (int n = 0; n < nfft; ++n)
            {
                l[start + static_cast<size_t> (n)] += z[static_cast<size_t> (n)].real() * scale * w[static_cast<size_t> (n)];
                r[start + static_cast<size_t> (n)] += z[static_cast<size_t> (n)].imag() * scale * w[static_cast<size_t> (n)];
            }
        }

        // torch's centring: its output starts half an FFT in; then HTDemucs's padding comes off.
        juce::ignoreUnused (length);

        for (int i = 0; i < segment; ++i)
        {
            const auto at = static_cast<size_t> (nfft / 2 + specPad + i);
            const auto e = envelope[at] > 1.0e-11f ? envelope[at] : 1.0f;
            outLeft[i] += l[at] / e;
            outRight[i] += r[at] / e;
        }
    }

    //==============================================================================
    /** A Kaiser-windowed sinc resampler, its kernel tabled finely; the cutoff a little under the
        lower of the two Nyquists. */
    std::vector<float> resample (const float* x, int length, double from, double to)
    {
        if (std::abs (from - to) < 1.0e-6)
            return std::vector<float> (x, x + length);

        const auto ratio = from / to;                           // input samples an output sample
        const auto cutoff = 0.97 * std::min (1.0, to / from);   // of the input's Nyquist
        constexpr int zeros = 24;                               // zero crossings each side
        const auto reach = zeros / cutoff;                      // input samples each side
        constexpr int fine = 512;                               // table points per input sample
        const auto tableSize = static_cast<int> (std::ceil (reach * fine)) + 2;

        // I0, for the Kaiser window (beta 8).
        const auto bessel = [] (double v)
        {
            double sum = 1.0, term = 1.0;

            for (int k = 1; k < 30; ++k)
            {
                term *= (v / (2.0 * k)) * (v / (2.0 * k));
                sum += term;
            }

            return sum;
        };

        constexpr double beta = 8.0;
        const auto norm = bessel (beta);
        std::vector<float> table (static_cast<size_t> (tableSize));

        for (int i = 0; i < tableSize; ++i)
        {
            const auto t = static_cast<double> (i) / fine;
            const auto u = t / reach;

            if (u >= 1.0)
                continue;

            const auto arg = pi * cutoff * t;
            const auto sinc = t == 0.0 ? 1.0 : std::sin (arg) / arg;
            table[static_cast<size_t> (i)] = static_cast<float> (cutoff * sinc * bessel (beta * std::sqrt (1.0 - u * u)) / norm);
        }

        const auto kernel = [&table, tableSize] (double t)
        {
            const auto p = std::abs (t) * fine;
            const auto i = static_cast<int> (p);

            if (i + 1 >= tableSize)
                return 0.0f;

            const auto f = static_cast<float> (p - i);
            return table[static_cast<size_t> (i)] + f * (table[static_cast<size_t> (i + 1)] - table[static_cast<size_t> (i)]);
        };

        const auto outLength = static_cast<int> (std::floor (length / ratio));
        std::vector<float> out (static_cast<size_t> (std::max (0, outLength)));

        for (int j = 0; j < outLength; ++j)
        {
            const auto centre = j * ratio;
            const auto first = std::max (0, static_cast<int> (std::ceil (centre - reach)));
            const auto last = std::min (length - 1, static_cast<int> (std::floor (centre + reach)));
            float sum = 0.0f;

            for (int i = first; i <= last; ++i)
                sum += x[i] * kernel (centre - i);

            out[static_cast<size_t> (j)] = sum;
        }

        return out;
    }

   #if SONIFY_ONNXRUNTIME
    //==============================================================================
    /** ONNX Runtime, loaded once, when it's first needed. */
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

            // A library named elsewhere (tests, a system-wide one).
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

    /** An ONNX Runtime status as an error ("" for none); the status is let go. */
    juce::String errorOf (const OrtApi* api, OrtStatus* status)
    {
        if (status == nullptr)
            return {};

        juce::String message (api->GetErrorMessage (status));
        api->ReleaseStatus (status);
        return message.isNotEmpty() ? message : juce::String ("ONNX Runtime failed.");
    }

    /** A model in a session. Two export layouts:
          planes   two inputs (the waveform, its spectrogram as four planes: left real, left
                   imaginary, right real, right imaginary), two outputs (the spectrogram branch
                   for each source, five dimensions, the same four planes; the waveform branch);
          parts    three inputs (spec_real and spec_imag, each channel's part as a plane of its
                   own, and audio, the waveform), three outputs (out_spec_real, out_spec_imag and
                   out_wave, each source's).
        run() takes and gives the planes layout either way. */
    class Session
    {
    public:
        explicit Session (const OrtApi* a) : api (a) {}

        ~Session()
        {
            if (session != nullptr)  api->ReleaseSession (session);
            if (options != nullptr)  api->ReleaseSessionOptions (options);
            if (memory != nullptr)   api->ReleaseMemoryInfo (memory);
            if (env != nullptr)      api->ReleaseEnv (env);
        }

        juce::String open (const juce::File& model, int threads, int numSources)
        {
            sources = numSources;

            if (auto e = errorOf (api, api->CreateEnv (ORT_LOGGING_LEVEL_WARNING, "Sonify", &env)); e.isNotEmpty())                return e;
            if (auto e = errorOf (api, api->CreateSessionOptions (&options)); e.isNotEmpty())                                        return e;
            if (auto e = errorOf (api, api->SetIntraOpNumThreads (options, threads)); e.isNotEmpty())                                return e;
            if (auto e = errorOf (api, api->SetSessionGraphOptimizationLevel (options, ORT_ENABLE_ALL)); e.isNotEmpty())             return e;

            // Its threads wait without spinning, so the audio keeps the processor it needs.
            errorOf (api, api->AddSessionConfigEntry (options, "session.intra_op.allow_spinning", "0"));

           #if JUCE_WINDOWS
            const auto path = model.getFullPathName();
            const auto status = api->CreateSession (env, path.toWideCharPointer(), options, &session);
           #else
            const auto status = api->CreateSession (env, model.getFullPathName().toRawUTF8(), options, &session);
           #endif

            if (auto e = errorOf (api, status); e.isNotEmpty())
                return "The model couldn't be opened: " + e;

            if (auto e = errorOf (api, api->CreateCpuMemoryInfo (OrtArenaAllocator, OrtMemTypeDefault, &memory)); e.isNotEmpty())
                return e;

            OrtAllocator* allocator = nullptr;

            if (auto e = errorOf (api, api->GetAllocatorWithDefaultOptions (&allocator)); e.isNotEmpty())
                return e;

            size_t inputCount = 0, outputCount = 0;
            errorOf (api, api->SessionGetInputCount (session, &inputCount));
            errorOf (api, api->SessionGetOutputCount (session, &outputCount));

            if (! ((inputCount == 2 && outputCount == 2) || (inputCount == 3 && outputCount == 3)))
                return "That isn't the model Sonify knows.";

            for (const auto isInput : { true, false })
            {
                auto& names = isInput ? inputs : outputs;

                for (size_t i = 0; i < (isInput ? inputCount : outputCount); ++i)
                {
                    char* name = nullptr;
                    const auto status2 = isInput ? api->SessionGetInputName (session, i, allocator, &name)
                                                 : api->SessionGetOutputName (session, i, allocator, &name);

                    if (auto e = errorOf (api, status2); e.isNotEmpty())
                        return e;

                    names.push_back (name);
                    errorOf (api, api->AllocatorFree (allocator, name));
                }
            }

            parts = inputCount == 3;

            // The parts layout is known by its names: the order they're fed and read in.
            if (parts)
            {
                const auto has = [] (const std::vector<std::string>& names, const char* name)
                {
                    return std::find (names.begin(), names.end(), std::string (name)) != names.end();
                };

                for (const auto* name : { "spec_real", "spec_imag", "audio" })
                    if (! has (inputs, name))
                        return "That isn't the model Sonify knows.";

                for (const auto* name : { "out_spec_real", "out_spec_imag", "out_wave" })
                    if (! has (outputs, name))
                        return "That isn't the model Sonify knows.";

                inputs = { "spec_real", "spec_imag", "audio" };
                outputs = { "out_spec_real", "out_spec_imag", "out_wave" };
            }

            return {};
        }

        /** One stretch: the waveform (2 x segment) and its spectrogram (4 planes x bins x frames)
            in; the sources' spectrogram branch (sources x 4 planes x bins x frames) and waveform
            branch (sources x 2 x segment) out. */
        juce::String run (std::vector<float>& wave, std::vector<float>& spec, std::vector<float>& outSpec, std::vector<float>& outWave,
                          OrtRunOptions* runOptions)
        {
            const auto plane = static_cast<size_t> (bins * frames);
            const std::array<int64_t, 3> waveShape { 1, 2, segment };
            const std::array<int64_t, 4> specShape { 1, 4, bins, frames };
            const std::array<int64_t, 4> partShape { 1, 2, bins, frames };
            std::vector<OrtValue*> in (inputs.size(), nullptr), out (outputs.size(), nullptr);

            const auto cleanUp = [this, &in, &out]
            {
                for (auto* v : in)  if (v != nullptr) api->ReleaseValue (v);
                for (auto* v : out) if (v != nullptr) api->ReleaseValue (v);
            };

            const auto tensor = [this] (std::vector<float>& data, const int64_t* shape, size_t dims, OrtValue** value)
            {
                return errorOf (api, api->CreateTensorWithDataAsOrtValue (memory, data.data(), data.size() * sizeof (float), shape, dims,
                                                                         ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, value));
            };

            juce::String e;

            if (parts)
            {
                // Each channel's real and imaginary planes, apart.
                realPart.resize (2 * plane);
                imagPart.resize (2 * plane);

                for (size_t c = 0; c < 2; ++c)
                {
                    std::copy_n (spec.begin() + static_cast<std::ptrdiff_t> ((2 * c) * plane), plane, realPart.begin() + static_cast<std::ptrdiff_t> (c * plane));
                    std::copy_n (spec.begin() + static_cast<std::ptrdiff_t> ((2 * c + 1) * plane), plane, imagPart.begin() + static_cast<std::ptrdiff_t> (c * plane));
                }

                e = tensor (realPart, partShape.data(), partShape.size(), &in[0]);
                if (e.isEmpty()) e = tensor (imagPart, partShape.data(), partShape.size(), &in[1]);
                if (e.isEmpty()) e = tensor (wave, waveShape.data(), waveShape.size(), &in[2]);
            }
            else
            {
                e = tensor (wave, waveShape.data(), waveShape.size(), &in[0]);
                if (e.isEmpty()) e = tensor (spec, specShape.data(), specShape.size(), &in[1]);
            }

            std::vector<const char*> inputNames, outputNames;

            for (const auto& name : inputs)  inputNames.push_back (name.c_str());
            for (const auto& name : outputs) outputNames.push_back (name.c_str());

            if (e.isEmpty())
                e = errorOf (api, api->Run (session, runOptions, inputNames.data(), in.data(), in.size(), outputNames.data(), out.size(), out.data()));

            // Each output's numbers, and how many dimensions it has.
            std::vector<const float*> data (out.size(), nullptr);
            std::vector<size_t> counts (out.size(), 0), dims (out.size(), 0);

            for (size_t i = 0; e.isEmpty() && i < out.size(); ++i)
            {
                OrtTensorTypeAndShapeInfo* info = nullptr;
                e = errorOf (api, api->GetTensorTypeAndShape (out[i], &info));

                if (e.isEmpty())
                {
                    ONNXTensorElementDataType type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
                    errorOf (api, api->GetTensorElementType (info, &type));
                    errorOf (api, api->GetDimensionsCount (info, &dims[i]));
                    errorOf (api, api->GetTensorShapeElementCount (info, &counts[i]));
                    api->ReleaseTensorTypeAndShapeInfo (info);

                    if (type != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                        e = "That isn't the model Sonify knows.";
                }

                float* values = nullptr;

                if (e.isEmpty())
                    e = errorOf (api, api->GetTensorMutableData (out[i], reinterpret_cast<void**> (&values)));

                data[i] = values;
            }

            const auto specCount = static_cast<size_t> (sources) * 4 * plane;
            const auto waveCount = static_cast<size_t> (sources) * 2 * static_cast<size_t> (segment);

            if (e.isEmpty() && parts)
            {
                // out_spec_real and out_spec_imag (sources x 2 channels x bins x frames) woven into the
                // four planes of each source.
                if (counts[0] != specCount / 2 || counts[1] != specCount / 2 || counts[2] != waveCount)
                {
                    e = "That isn't the model Sonify knows.";
                }
                else
                {
                    outSpec.resize (specCount);

                    for (size_t source = 0; source < static_cast<size_t> (sources); ++source)
                        for (size_t c = 0; c < 2; ++c)
                        {
                            const auto from = (source * 2 + c) * plane;
                            std::copy_n (data[0] + from, plane, outSpec.begin() + static_cast<std::ptrdiff_t> ((source * 4 + 2 * c) * plane));
                            std::copy_n (data[1] + from, plane, outSpec.begin() + static_cast<std::ptrdiff_t> ((source * 4 + 2 * c + 1) * plane));
                        }

                    outWave.assign (data[2], data[2] + waveCount);
                }
            }
            else if (e.isEmpty())
            {
                // Which output is which: the spectrogram branch has five dimensions.
                for (size_t i = 0; e.isEmpty() && i < out.size(); ++i)
                {
                    auto& target = dims[i] == 5 ? outSpec : outWave;
                    const auto expected = dims[i] == 5 ? specCount : waveCount;

                    if (counts[i] != expected)
                        e = "That isn't the model Sonify knows.";
                    else
                        target.assign (data[i], data[i] + counts[i]);
                }
            }

            cleanUp();
            return e;
        }

    private:
        const OrtApi* api;
        OrtEnv* env = nullptr;
        OrtSessionOptions* options = nullptr;
        OrtSession* session = nullptr;
        OrtMemoryInfo* memory = nullptr;
        std::vector<std::string> inputs, outputs;
        bool parts = false;
        int sources = 4;
        std::vector<float> realPart, imagPart;
    };
   #endif
}

//==============================================================================
juce::String StemSplitter::nameOf (Stem stem)
{
    switch (stem)
    {
        case Stem::drums:  return "Drums";
        case Stem::bass:   return "Bass";
        case Stem::other:  return "Other";
        case Stem::guitar: return "Guitar";
        case Stem::piano:  return "Piano";
        case Stem::vocals: break;
    }

    return "Vocals";
}

bool StemSplitter::stemFrom (const juce::String& name, Stem& stem)
{
    const auto n = name.trim().toLowerCase();

    if (n == "drums")                  { stem = Stem::drums;  return true; }
    if (n == "bass")                   { stem = Stem::bass;   return true; }
    if (n == "other" || n == "rest")   { stem = Stem::other;  return true; }
    if (n == "vocals" || n == "voice") { stem = Stem::vocals; return true; }
    if (n == "guitar" || n == "guitars") { stem = Stem::guitar; return true; }
    if (n == "piano" || n == "keys")   { stem = Stem::piano;  return true; }
    return false;
}

StemSplitter::Model StemSplitter::modelFor (const std::vector<Stem>& stems)
{
    for (const auto stem : stems)
        if (stem == Stem::guitar || stem == Stem::piano)
            return Model::sixStems;

    return Model::fourStems;
}

StemSplitter::StemSplitter (Job j)
    : juce::Thread ("Sonify stem splitting"), job (std::move (j))
{
}

StemSplitter::~StemSplitter()
{
    cancel();
    stopThread (15000);
}

void StemSplitter::cancel()
{
    signalThreadShouldExit();

   #if SONIFY_ONNXRUNTIME
    // The stretch under way stops at once too.
    const std::scoped_lock lock (runLock);

    if (auto* options = static_cast<OrtRunOptions*> (running); options != nullptr && runtime().api != nullptr)
        errorOf (runtime().api, runtime().api->RunOptionsSetTerminate (options));
   #endif
}

void StemSplitter::run()
{
    const auto outcome = split();

    if (outcome.failed())
    {
        // What it wrote is no use half done.
        for (const auto& [stem, file] : result.files)
            file.deleteFile();

        result.files.clear();
        result.ok = false;
        result.error = threadShouldExit() ? juce::String ("Cancelled") : outcome.getErrorMessage();
    }
    else
    {
        result.ok = true;
    }

    progress = 1.0;
    done = true;
}

juce::Result StemSplitter::split()
{
   #if ! SONIFY_ONNXRUNTIME
    return juce::Result::fail (runtimeProblem());
   #else
    if (const auto problem = runtimeProblem(); problem.isNotEmpty())
        return juce::Result::fail (problem);

    if (! hasModel (job.model))
        return juce::Result::fail ("Stem splitting's model isn't here yet.");

    if (job.stems.empty())
        return juce::Result::fail ("Choose at least one stem.");

    const auto sources = sourcesOf (job.model);

    for (const auto stem : job.stems)
        if (static_cast<int> (stem) >= sources)
            return juce::Result::fail ("Guitar and piano need the six-stem model.");

    // The six-stem model takes guitar and piano out of the rest: one not wanted on its own goes
    // back into it, so the rest is still everything but the stems chosen.
    const auto wanted = [this] (Stem stem) { return std::find (job.stems.begin(), job.stems.end(), stem) != job.stems.end(); };
    std::vector<Stem> intoRest;

    if (sources == 6 && wanted (Stem::other))
        for (const auto stem : { Stem::guitar, Stem::piano })
            if (! wanted (stem))
                intoRest.push_back (stem);

    const auto* api = runtime().api;

    //==============================================================================
    // The recording, stereo at 44.1 kHz.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (job.source));

    if (reader == nullptr || reader->sampleRate <= 0.0)
        return juce::Result::fail ("The recording (" + job.source.getFileName() + ") couldn't be read.");

    const auto fileRate = reader->sampleRate;
    const auto first = juce::jlimit<juce::int64> (0, reader->lengthInSamples, static_cast<juce::int64> (std::llround (job.offsetSeconds * fileRate)));
    const auto count = juce::jlimit<juce::int64> (0, reader->lengthInSamples - first, static_cast<juce::int64> (std::llround (job.lengthSeconds * fileRate)));

    if (count < static_cast<juce::int64> (0.5 * fileRate))
        return juce::Result::fail ("The recording is too short to split.");

    if (count > static_cast<juce::int64> (20.0 * 60.0 * fileRate))
        return juce::Result::fail ("Recordings up to 20 minutes long are split: shorten this one first.");

    juce::AudioBuffer<float> read (2, static_cast<int> (count));
    read.clear();
    reader->read (&read, 0, static_cast<int> (count), first, true, reader->numChannels > 1);

    if (reader->numChannels == 1)
        read.copyFrom (1, 0, read, 0, 0, static_cast<int> (count));

    std::array<std::vector<float>, 2> mix;

    for (int ch = 0; ch < 2; ++ch)
    {
        mix[static_cast<size_t> (ch)] = resample (read.getReadPointer (ch), static_cast<int> (count), fileRate, rate);

        if (threadShouldExit())
            return juce::Result::fail ("Cancelled");
    }

    read.setSize (0, 0);
    const auto total = static_cast<int> (mix[0].size());
    result.lengthSeconds = total / rate;

    // Levelled as Demucs levels a track: less the mean of the two channels' average, over its
    // spread (sample standard deviation). The stems get the spread back; the mean (a DC offset)
    // stays out of them.
    double mean = 0.0, spread = 0.0;

    for (int i = 0; i < total; ++i)
        mean += 0.5 * (static_cast<double> (mix[0][static_cast<size_t> (i)]) + mix[1][static_cast<size_t> (i)]);

    mean /= juce::jmax (1, total);

    for (int i = 0; i < total; ++i)
    {
        const auto d = 0.5 * (static_cast<double> (mix[0][static_cast<size_t> (i)]) + mix[1][static_cast<size_t> (i)]) - mean;
        spread += d * d;
    }

    spread = total > 1 ? std::sqrt (spread / (total - 1)) : 0.0;
    const auto scale = 1.0e-5 + spread;

    for (auto& channel : mix)
        for (auto& x : channel)
            x = static_cast<float> ((x - mean) / scale);

    progress = 0.03;

    //==============================================================================
    // The model.
    Session session (api);
    // A core left for the audio (and the page) when there are enough to spare one.
    const auto cores = juce::jmax (1, juce::SystemStats::getNumPhysicalCpus());
    const auto threads = cores >= 4 ? cores - 1 : cores;

    if (const auto e = session.open (modelFile (job.model), threads, sources); e.isNotEmpty())
        return juce::Result::fail (e);

    OrtRunOptions* runOptions = nullptr;

    if (const auto e = errorOf (api, api->CreateRunOptions (&runOptions)); e.isNotEmpty())
        return juce::Result::fail (e);

    {
        const std::scoped_lock lock (runLock);
        running = runOptions;
    }

    const auto releaseRun = [&]
    {
        const std::scoped_lock lock (runLock);
        running = nullptr;
        api->ReleaseRunOptions (runOptions);
    };

    //==============================================================================
    // The stems' files, written as the stretches finish.
    struct Output
    {
        Stem stem;
        std::unique_ptr<juce::AudioFormatWriter> writer;
        std::array<std::vector<float>, 2> sum;   // from `written` on
    };

    std::vector<Output> outputs;
    job.folder.createDirectory();

    for (const auto stem : job.stems)
    {
        const auto file = job.folder.getNonexistentChildFile (juce::File::createLegalFileName (job.name + " " + nameOf (stem)), ".wav", false);
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();

        if (stream == nullptr)
        {
            releaseRun();
            return juce::Result::fail ("Couldn't write " + file.getFullPathName());
        }

        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate)
                                                                                                     .withNumChannels (2)
                                                                                                     .withBitsPerSample (32)
                                                                                                     .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));

        if (writer == nullptr)
        {
            releaseRun();
            return juce::Result::fail ("Couldn't write " + file.getFullPathName());
        }

        result.files[stem] = file;
        Output output;
        output.stem = stem;
        output.writer = std::move (writer);
        output.sum[0].assign (static_cast<size_t> (segment), 0.0f);
        output.sum[1].assign (static_cast<size_t> (segment), 0.0f);
        outputs.push_back (std::move (output));
    }

    //==============================================================================
    // The stretches, a quarter overlapping, each weighted by a triangle where they blend.
    std::vector<float> weight (static_cast<size_t> (segment));

    for (int i = 0; i < segment / 2; ++i)
        weight[static_cast<size_t> (i)] = static_cast<float> (i + 1);

    for (int i = segment / 2; i < segment; ++i)
        weight[static_cast<size_t> (i)] = static_cast<float> (segment - i);

    const auto peak = *std::max_element (weight.begin(), weight.end());

    for (auto& w : weight)
        w /= peak;

    std::vector<float> weights (static_cast<size_t> (segment), 0.0f);
    std::vector<float> wave (static_cast<size_t> (2 * segment)), spec, outSpec, outWave;
    std::array<std::vector<float>, 2> heard;
    const Fft fft;
    const auto stretches = (total + stride - 1) / stride;
    int written = 0;

    for (int k = 0; k < stretches; ++k)
    {
        if (threadShouldExit())
        {
            releaseRun();
            return juce::Result::fail ("Cancelled");
        }

        const auto offset = k * stride;
        const auto length = std::min (segment, total - offset);

        // A short last stretch sits in the middle of a whole one, with what's around it.
        const auto start = offset - (segment - length) / 2;

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < segment; ++i)
            {
                const auto at = start + i;
                wave[static_cast<size_t> (ch * segment + i)] = at >= 0 && at < total ? mix[static_cast<size_t> (ch)][static_cast<size_t> (at)] : 0.0f;
            }

        spectrogram (fft, wave.data(), wave.data() + segment, spec);

        if (const auto e = session.run (wave, spec, outSpec, outWave, runOptions); e.isNotEmpty())
        {
            releaseRun();
            return juce::Result::fail (threadShouldExit() ? juce::String ("Cancelled") : "The separation failed: " + e);
        }

        const auto trim = (segment - length) / 2;

        for (auto& output : outputs)
        {
            // The waveform branch plus the spectrogram branch turned back into sound; the rest
            // takes a guitar or piano that wasn't wanted on its own.
            for (auto& channel : heard)
                channel.assign (static_cast<size_t> (segment), 0.0f);

            std::vector<size_t> heardSources { static_cast<size_t> (output.stem) };

            if (output.stem == Stem::other)
                for (const auto extra : intoRest)
                    heardSources.push_back (static_cast<size_t> (extra));

            for (const auto s : heardSources)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    const auto* branch = outWave.data() + (s * 2 + static_cast<size_t> (ch)) * static_cast<size_t> (segment);
                    auto& into = heard[static_cast<size_t> (ch)];

                    for (int i = 0; i < segment; ++i)
                        into[static_cast<size_t> (i)] += branch[i];
                }

                resynthesise (fft, outSpec.data() + s * static_cast<size_t> (4 * bins * frames), heard[0].data(), heard[1].data());
            }

            const auto from = offset - written;

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < length; ++i)
                    output.sum[static_cast<size_t> (ch)][static_cast<size_t> (from + i)] += weight[static_cast<size_t> (i)] * heard[static_cast<size_t> (ch)][static_cast<size_t> (trim + i)];
        }

        for (int i = 0; i < length; ++i)
            weights[static_cast<size_t> (offset - written + i)] += weight[static_cast<size_t> (i)];

        // What no later stretch reaches is done: written, and the sums move on.
        const auto finished = k + 1 < stretches ? std::min (total, offset + stride) : total;
        const auto ready = finished - written;
        std::array<std::vector<float>, 2> block { std::vector<float> (static_cast<size_t> (ready)), std::vector<float> (static_cast<size_t> (ready)) };

        for (auto& output : outputs)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& sum = output.sum[static_cast<size_t> (ch)];

                for (int i = 0; i < ready; ++i)
                {
                    const auto heardHere = weights[static_cast<size_t> (i)] > 0.0f ? sum[static_cast<size_t> (i)] / weights[static_cast<size_t> (i)] : 0.0f;
                    block[static_cast<size_t> (ch)][static_cast<size_t> (i)] = static_cast<float> (heardHere * scale);
                }

                std::move (sum.begin() + ready, sum.end(), sum.begin());
                std::fill (sum.end() - ready, sum.end(), 0.0f);
            }

            const float* channels[2] = { block[0].data(), block[1].data() };

            if (! output.writer->writeFromFloatArrays (channels, 2, ready))
            {
                releaseRun();
                return juce::Result::fail ("Couldn't write " + result.files[output.stem].getFullPathName());
            }
        }

        std::move (weights.begin() + ready, weights.end(), weights.begin());
        std::fill (weights.end() - ready, weights.end(), 0.0f);
        written = finished;
        progress = 0.03 + 0.97 * (k + 1) / stretches;
    }

    releaseRun();

    for (auto& output : outputs)
        output.writer.reset();   // finished and closed

    return juce::Result::ok();
   #endif
}

//==============================================================================
juce::String StemSplitter::runtimeProblem()
{
   #if SONIFY_ONNXRUNTIME
    return runtime().problem;
   #else
    return "This build of Sonify has no ONNX Runtime: build it again with its CMakeLists.txt.";
   #endif
}

juce::File StemSplitter::modelFile (Model model)
{
    const auto name = model == Model::sixStems ? "htdemucs_6s.onnx" : "htdemucs.onnx";

    // One shipped next to Sonify (or downloaded there by a build-folder Sonify before) is used
    // as it is; a download goes into Sonify's own folder in AppData, which can always be written
    // (Program Files, where Sonify is installed, can't).
    const auto beside = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("Models").getChildFile (name);

    if (beside.existsAsFile())
        return beside;

    return SettingsFolder::get().getChildFile ("Models").getChildFile (name);
}

bool StemSplitter::hasModel (Model model)
{
    const auto file = modelFile (model);
    return file.existsAsFile() && file.getSize() == (model == Model::sixStems ? sixModelBytes : modelBytes);
}

juce::Result StemSplitter::unpackModel (const juce::File& package, const juce::File& destination)
{
    // A .tgz: gzip around a tar, whose entries are a 512-byte header and the file in 512-byte blocks.
    juce::FileInputStream raw (package);

    if (! raw.openedOk())
        return juce::Result::fail ("The model's package couldn't be read.");

    juce::GZIPDecompressorInputStream tar (&raw, false, juce::GZIPDecompressorInputStream::gzipFormat);
    const auto partial = destination.getSiblingFile (destination.getFileName() + ".part");
    destination.getParentDirectory().createDirectory();

    for (;;)
    {
        char header[512];

        if (tar.read (header, 512) != 512)
            break;

        if (header[0] == 0)
            break;   // the end

        const auto field = [&header] (int from, int length) { return juce::String (header + from, static_cast<size_t> (strnlen (header + from, static_cast<size_t> (length)))); };
        auto name = field (0, 100);

        if (const auto prefix = field (345, 155); prefix.isNotEmpty())
            name = prefix + "/" + name;

        const auto size = static_cast<juce::int64> (std::strtoll (field (124, 12).trim().toRawUTF8(), nullptr, 8));   // octal
        const auto blocks = (size + 511) / 512 * 512;

        if (name.fromLastOccurrenceOf ("/", false, false) == "htdemucs.onnx" && (header[156] == '0' || header[156] == 0))
        {
            partial.deleteFile();

            {
                juce::FileOutputStream out (partial);

                if (! out.openedOk())
                    return juce::Result::fail ("Couldn't write " + partial.getFullPathName());

                if (out.writeFromInputStream (tar, size) != size)
                    return juce::Result::fail ("The model's package is damaged.");
            }

            if (partial.getSize() != modelBytes || juce::SHA256 (partial).toHexString() != modelSha256)
            {
                partial.deleteFile();
                return juce::Result::fail ("The model downloaded isn't the one Sonify knows.");
            }

            destination.deleteFile();

            if (! partial.moveFileTo (destination))
                return juce::Result::fail ("Couldn't put the model in " + destination.getFullPathName());

            return juce::Result::ok();
        }

        // Another file: past it.
        if (blocks > 0)
            tar.skipNextBytes (blocks);

        if (tar.isExhausted())
            break;
    }

    return juce::Result::fail ("The model wasn't in its package.");
}

juce::Result StemSplitter::downloadModel (Model model, const std::function<bool (double)>& report)
{
    if (hasModel (model))
        return juce::Result::ok();

    const auto six = model == Model::sixStems;
    int status = 0;
    const auto stream = juce::URL (six ? sixModelUrl : modelPackage).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                                           .withConnectionTimeoutMs (30000)
                                                                                           .withStatusCode (&status)
                                                                                           .withNumRedirectsToFollow (5));

    if (stream == nullptr || status >= 400)
        return juce::Result::fail ("The model couldn't be downloaded" + (status >= 400 ? " (" + juce::String (status) + ")" : juce::String()) + ": check the internet.");

    // The six-stem model comes as it is; the four-stem one in a package.
    const auto package = six ? modelFile (model).getSiblingFile ("htdemucs_6s.onnx.part")
                             : modelFile (model).getSiblingFile ("htdemucs-package.tgz");
    package.getParentDirectory().createDirectory();
    package.deleteFile();
    const auto expected = stream->getTotalLength();

    juce::String failure;

    {
        juce::FileOutputStream out (package);

        if (! out.openedOk())
            return juce::Result::fail ("Couldn't write " + package.getFullPathName());

        juce::HeapBlock<char> buffer (1 << 16);
        juce::int64 got = 0;

        for (;;)
        {
            const auto n = stream->read (buffer, 1 << 16);

            if (n <= 0)
                break;

            if (! out.write (buffer, static_cast<size_t> (n)))
            {
                failure = "Couldn't write " + package.getFullPathName();
                break;
            }

            got += n;

            if (! report (expected > 0 ? 0.9 * static_cast<double> (got) / static_cast<double> (expected) : 0.0))
            {
                failure = "Cancelled";
                break;
            }
        }

        if (failure.isEmpty() && expected > 0 && got != expected)
            failure = "The download broke off: try again.";
    }

    // Closed now, so Windows lets it go.
    if (failure.isNotEmpty())
    {
        package.deleteFile();
        return juce::Result::fail (failure);
    }

    report (0.92);

    if (six)
    {
        // Checked whole, then put in its place.
        const auto destination = modelFile (model);

        if (package.getSize() != sixModelBytes || juce::SHA256 (package).toHexString() != sixModelSha256)
        {
            package.deleteFile();
            return juce::Result::fail ("The model downloaded isn't the one Sonify knows.");
        }

        destination.deleteFile();

        if (! package.moveFileTo (destination))
        {
            package.deleteFile();
            return juce::Result::fail ("Couldn't put the model in " + destination.getFullPathName());
        }

        report (1.0);
        return juce::Result::ok();
    }

    const auto unpacked = unpackModel (package, modelFile (model));
    package.deleteFile();
    report (1.0);
    return unpacked;
}
