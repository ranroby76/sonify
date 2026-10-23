// D:\workspace\Fanan_web_hub\src\StemSplitter.h

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

//==============================================================================
/**
    Split into Stems: a recording separated into its vocals, drums, bass and the rest, and when
    they're wanted its guitar and piano too, on this computer. The separation is Demucs v4 (the
    hybrid transformer): "htdemucs" for the four stems, "htdemucs_6s" when guitar or piano is
    wanted (then the rest is what's left after them as well). Run by ONNX Runtime on the
    processor: the recording, levelled as Demucs levels it, in 7.8-second stretches that overlap
    by a quarter, each through the model's two branches (the waveform, and its spectrogram turned
    back into sound), blended where they overlap. The stems come out at 44.1 kHz, as the model
    hears.

    ONNX Runtime is onnxruntime.dll next to Sonify (the build puts it there), loaded when it's
    first needed; each model is downloaded once into Models next to Sonify: the four-stem one in
    a package (98 MB, 174 MB unpacked), the six-stem one as it is (110 MB). Both export layouts
    are understood: the four planes of one spectrogram tensor, or its real and imaginary parts
    as tensors of their own (spec_real, spec_imag, audio in; out_spec_real, out_spec_imag,
    out_wave out).

    Splits on a thread of its own, writing each stem as it goes.
*/
class StemSplitter final : private juce::Thread
{
public:
    enum class Stem { drums, bass, other, vocals, guitar, piano };   // the models' order (the four-stem one's: the first four)

    static constexpr int maxStems = 6;
    static juce::String nameOf (Stem stem);             // "Drums", "Bass", "Other", "Vocals", "Guitar", "Piano"
    static bool stemFrom (const juce::String& name, Stem& stem);

    /** Which model splits: the four-stem one, or the six-stem one when guitar or piano is wanted. */
    enum class Model { fourStems, sixStems };
    static Model modelFor (const std::vector<Stem>& stems);
    static int sourcesOf (Model model) noexcept        { return model == Model::sixStems ? 6 : 4; }

    struct Job
    {
        juce::File source;                  // the recording's file
        double offsetSeconds = 0.0;         // the part of it to split
        double lengthSeconds = 0.0;
        std::vector<Stem> stems;            // the ones wanted
        Model model = Model::fourStems;     // what splits it (modelFor the stems)
        juce::File folder;                  // where they go
        juce::String name;                  // "<name> Vocals.wav"...
    };

    struct Result
    {
        bool ok = false;
        juce::String error;                 // "Cancelled" when cancelled
        std::map<Stem, juce::File> files;   // 44.1 kHz stereo, 32-bit float
        double lengthSeconds = 0.0;
    };

    explicit StemSplitter (Job job);
    ~StemSplitter() override;

    void start()                                       { startThread (juce::Thread::Priority::low); }
    void cancel();
    bool isDone() const noexcept                       { return done.load(); }
    double getProgress() const noexcept                { return progress.load(); }

    /** Once isDone(). */
    const Result& getResult() const noexcept           { return result; }

    //==============================================================================
    // What it needs on this computer.

    /** "" when ONNX Runtime is there and loads; otherwise why not. */
    static juce::String runtimeProblem();

    /** Where a model goes, and whether it's there whole. */
    static juce::File modelFile (Model model = Model::fourStems);
    static bool hasModel (Model model = Model::fourStems);

    /** What the first split with a model downloads, in megabytes (to say so). */
    static int downloadMegabytes (Model model) noexcept { return model == Model::sixStems ? 110 : 98; }

    static constexpr juce::int64 modelBytes = 174263526;
    static constexpr const char* modelPackage = "https://registry.npmjs.org/demucs/-/demucs-1.0.0.tgz";
    static constexpr juce::int64 sixModelBytes = 110395774;
    static constexpr const char* sixModelUrl = "https://huggingface.co/Ryan5453/unblend/resolve/30b749fd691312b1e4b3f8fe79df0b51cde836cd/htdemucs_6s_fp32.onnx";

    /** Downloads a model (the four-stem one's package, unpacked; the six-stem one as it is):
        progress 0..1 as it goes, false stops it. Blocks: a thread of its own. */
    static juce::Result downloadModel (Model model, const std::function<bool (double progress)>& report);

    /** Takes the four-stem model out of a .tgz package (the file inside it called htdemucs.onnx) and checks it. */
    static juce::Result unpackModel (const juce::File& package, const juce::File& destination);

    /** A model downloaded on a thread of its own. */
    class Download final : private juce::Thread
    {
    public:
        explicit Download (Model m) : juce::Thread ("Sonify model download"), model (m) {}
        ~Download() override                          { signalThreadShouldExit(); stopThread (30000); }

        void start()                                  { startThread (juce::Thread::Priority::low); }
        void cancel()                                 { signalThreadShouldExit(); }
        bool isDone() const noexcept                  { return done.load(); }
        double getProgress() const noexcept           { return progress.load(); }

        /** Once isDone(). */
        const juce::Result& getResult() const noexcept { return result; }
        Model getModel() const noexcept                { return model; }

    private:
        void run() override
        {
            result = downloadModel (model, [this] (double p) { progress = p; return ! threadShouldExit(); });
            done = true;
        }

        Model model;
        juce::Result result { juce::Result::ok() };
        std::atomic<bool> done { false };
        std::atomic<double> progress { 0.0 };
    };

    static constexpr double rate = 44100.0;           // what the model hears and gives

private:
    void run() override;
    juce::Result split();

    Job job;
    Result result;
    std::atomic<bool> done { false };
    std::atomic<double> progress { 0.0 };
    std::mutex runLock;
    void* running = nullptr;                           // the run under way (OrtRunOptions), to stop it at once

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemSplitter)
};
