// D:\workspace\Fanan_web_hub\src\Mp3Encoder.cpp

#include "Mp3Encoder.h"

#include <cmath>

//==============================================================================
// LAME's C functions, looked up in the library (lame.h's names and signatures).
struct Mp3Encoder::Lame
{
#if JUCE_WINDOWS
   #define FANAN_LAME_CALL __cdecl
#else
   #define FANAN_LAME_CALL
#endif

    using Init           = void* (FANAN_LAME_CALL*) ();
    using SetInt         = int   (FANAN_LAME_CALL*) (void*, int);
    using InitParams     = int   (FANAN_LAME_CALL*) (void*);
    using EncodeFloat    = int   (FANAN_LAME_CALL*) (void*, const float*, const float*, int, unsigned char*, int);
    using Flush          = int   (FANAN_LAME_CALL*) (void*, unsigned char*, int);
    using LameTag        = size_t (FANAN_LAME_CALL*) (const void*, unsigned char*, size_t);
    using Close          = int   (FANAN_LAME_CALL*) (void*);

    Lame()
    {
        const auto folder = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

       #if JUCE_WINDOWS
        const auto file = folder.getChildFile ("mp3lame.dll");
       #elif JUCE_MAC
        const auto file = folder.getChildFile ("libmp3lame.dylib");
       #else
        const auto file = folder.getChildFile ("libmp3lame.so");
       #endif

        // A library named elsewhere (tests).
        auto path = juce::SystemStats::getEnvironmentVariable ("SONIFY_MP3LAME", {});

        if (path.isEmpty())
        {
            if (! file.existsAsFile())
            {
                problem = "MP3 export needs LAME (" + file.getFileName() + ") next to Sonify: building Sonify puts it there.";
                return;
            }

            path = file.getFullPathName();
        }

        if (! library.open (path))
        {
            problem = "LAME (" + juce::File (path).getFileName() + ") couldn't be loaded.";
            return;
        }

        const auto find = [this] (const char* name) { return library.getFunction (name); };

        init          = reinterpret_cast<Init>        (find ("lame_init"));
        setInRate     = reinterpret_cast<SetInt>      (find ("lame_set_in_samplerate"));
        setChannels   = reinterpret_cast<SetInt>      (find ("lame_set_num_channels"));
        setBitRate    = reinterpret_cast<SetInt>      (find ("lame_set_brate"));
        setMode       = reinterpret_cast<SetInt>      (find ("lame_set_mode"));
        setQuality    = reinterpret_cast<SetInt>      (find ("lame_set_quality"));
        setVbr        = reinterpret_cast<SetInt>      (find ("lame_set_VBR"));
        setVbrTag     = reinterpret_cast<SetInt>      (find ("lame_set_bWriteVbrTag"));
        initParams    = reinterpret_cast<InitParams>  (find ("lame_init_params"));
        encode        = reinterpret_cast<EncodeFloat> (find ("lame_encode_buffer_ieee_float"));
        flush         = reinterpret_cast<Flush>       (find ("lame_encode_flush"));
        lameTag       = reinterpret_cast<LameTag>     (find ("lame_get_lametag_frame"));
        close         = reinterpret_cast<Close>       (find ("lame_close"));

        if (init == nullptr || setInRate == nullptr || setChannels == nullptr || setBitRate == nullptr || setMode == nullptr
            || setQuality == nullptr || setVbr == nullptr || setVbrTag == nullptr || initParams == nullptr || encode == nullptr
            || flush == nullptr || lameTag == nullptr || close == nullptr)
            problem = "This LAME (" + juce::File (path).getFileName() + ") is missing what Sonify needs.";
    }

    juce::DynamicLibrary library;
    juce::String problem;

    Init init = nullptr;
    SetInt setInRate = nullptr, setChannels = nullptr, setBitRate = nullptr, setMode = nullptr, setQuality = nullptr,
           setVbr = nullptr, setVbrTag = nullptr;
    InitParams initParams = nullptr;
    EncodeFloat encode = nullptr;
    Flush flush = nullptr;
    LameTag lameTag = nullptr;
    Close close = nullptr;

   #undef FANAN_LAME_CALL
};

Mp3Encoder::Lame& Mp3Encoder::lame()
{
    static Lame instance;
    return instance;
}

juce::String Mp3Encoder::problem()
{
    return lame().problem;
}

//==============================================================================
Mp3Encoder::Mp3Encoder() = default;

Mp3Encoder::~Mp3Encoder()
{
    if (flags != nullptr)
        lame().close (flags);

    // Never finished: no half file left behind.
    if (stream != nullptr)
    {
        stream.reset();
        target.deleteFile();
    }
}

juce::Result Mp3Encoder::open (const juce::File& file, double sampleRate, int kbps)
{
    auto& api = lame();

    if (api.problem.isNotEmpty())
        return juce::Result::fail (api.problem);

    if (flags != nullptr)
        return juce::Result::fail ("This MP3 has already started.");

    flags = api.init();

    if (flags == nullptr)
        return juce::Result::fail ("LAME couldn't start.");

    // The bit rates MP3 has, nearest to the one asked for.
    static constexpr int rates[] = { 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
    int rate = 320;

    for (const auto r : rates)
        if (std::abs (r - kbps) < std::abs (rate - kbps))
            rate = r;

    api.setInRate (flags, juce::roundToInt (sampleRate));
    api.setChannels (flags, 2);
    api.setBitRate (flags, rate);
    api.setMode (flags, 1);         // joint stereo
    api.setQuality (flags, 2);      // LAME's "high quality" (0 best .. 9 fastest)
    api.setVbr (flags, 0);          // constant bit rate
    api.setVbrTag (flags, 1);       // the LAME tag: exact length for players

    if (api.initParams (flags) < 0)
        return juce::Result::fail ("LAME can't write " + juce::String (juce::roundToInt (sampleRate)) + " Hz at " + juce::String (rate) + " kbps.");

    file.deleteFile();
    stream = std::make_unique<juce::FileOutputStream> (file);

    if (! stream->openedOk())
    {
        stream.reset();
        return juce::Result::fail ("Couldn't write " + file.getFullPathName());
    }

    target = file;
    failed = false;
    return juce::Result::ok();
}

bool Mp3Encoder::write (const float* left, const float* right, int numSamples)
{
    if (flags == nullptr || stream == nullptr || failed || numSamples <= 0)
        return ! failed;

    // As LAME asks: room for 1.25 times the samples plus 7200 bytes.
    const auto needed = static_cast<size_t> (numSamples + numSamples / 4 + 7200);

    if (buffer.size() < needed)
        buffer.resize (needed);

    const auto written = lame().encode (flags, left, right, numSamples, buffer.data(), static_cast<int> (buffer.size()));

    if (written < 0 || (written > 0 && ! stream->write (buffer.data(), static_cast<size_t> (written))))
        failed = true;

    return ! failed;
}

juce::Result Mp3Encoder::close()
{
    if (flags == nullptr || stream == nullptr)
        return juce::Result::fail ("This MP3 never started.");

    auto& api = lame();

    if (buffer.size() < 7200)
        buffer.resize (7200);

    const auto last = api.flush (flags, buffer.data(), static_cast<int> (buffer.size()));

    if (last < 0 || (last > 0 && ! stream->write (buffer.data(), static_cast<size_t> (last))))
        failed = true;

    // The LAME tag goes over the first frame, which LAME kept for it.
    if (! failed)
    {
        std::vector<unsigned char> tag (2880);
        const auto size = api.lameTag (flags, tag.data(), tag.size());

        if (size > 0 && size <= tag.size())
        {
            stream->flush();
            failed = ! stream->setPosition (0) || ! stream->write (tag.data(), size);
        }
    }

    stream->flush();
    failed = failed || stream->getStatus().failed();

    api.close (flags);
    flags = nullptr;
    stream.reset();

    if (failed)
    {
        target.deleteFile();
        return juce::Result::fail ("Couldn't write " + target.getFullPathName());
    }

    return juce::Result::ok();
}
