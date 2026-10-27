#pragma once

#include <juce_dsp/juce_dsp.h>

namespace axis::dsp
{

/** AMP — the capture player.

    The tone stack, trim, presence and output stage are implemented here and
    working. The neural capture engine itself is NOT wired yet: `loadCapture`
    and `runCapture` are the two seams where NeuralAudio (MIT) drops in, and
    until it does the module runs as a clean tone-shaping stage so the rest of
    the chain can be built and heard.

    When the engine is added:
      - loadCapture()  parses the .nam file and builds the model
      - runCapture()   replaces the identity call in processBlock
      - setSlim()      selects the A2 slimmable width (40 / 60 / 100 %)
      - latencySamples() reports the model's receptive-field delay to the host
*/
class AmpCapture
{
public:
    enum class Slim { slim40 = 0, slim60, full };

    struct Params
    {
        Slim  slim       = Slim::slim60;
        float trimDb     = 0.0f;
        float bass       = 52.0f;
        float mid        = 48.0f;
        float treble     = 58.0f;
        float presence   = 44.0f;
        bool  cabSim     = true;
        bool  stackPost  = false;
        bool  doubler    = false;
        float output     = 72.0f;
        float micX       = 36.0f;
        float micY       = 50.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate  = spec.sampleRate;
        numChannels = (int) spec.numChannels;

        lowShelf .resize ((size_t) numChannels);
        midPeak  .resize ((size_t) numChannels);
        highShelf.resize ((size_t) numChannels);
        presPeak .resize ((size_t) numChannels);
        cabLow   .resize ((size_t) numChannels);
        cabHigh  .resize ((size_t) numChannels);

        doublerDelay.resize ((size_t) numChannels,
                             juce::dsp::DelayLine<float> (2048));

        for (auto& d : doublerDelay)
        {
            d.prepare (spec);
            d.setMaximumDelayInSamples (2048);
        }

        reset();
    }

    void reset()
    {
        for (auto& f : lowShelf)  f.reset();
        for (auto& f : midPeak)   f.reset();
        for (auto& f : highShelf) f.reset();
        for (auto& f : presPeak)  f.reset();
        for (auto& f : cabLow)    f.reset();
        for (auto& f : cabHigh)   f.reset();
        for (auto& d : doublerDelay) d.reset();
    }

    void setParams (const Params& p)
    {
        params = p;

        const float bassDb = juce::jmap (p.bass,     0.0f, 100.0f, -12.0f, 12.0f);
        const float midDb  = juce::jmap (p.mid,      0.0f, 100.0f, -10.0f, 10.0f);
        const float trebDb = juce::jmap (p.treble,   0.0f, 100.0f, -12.0f, 12.0f);
        const float presDb = juce::jmap (p.presence, 0.0f, 100.0f,  -6.0f, 10.0f);

        auto lowC  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 110.0, 0.72, juce::Decibels::decibelsToGain (bassDb));
        auto midC  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 650.0, 0.80, juce::Decibels::decibelsToGain (midDb));
        auto highC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 3200.0, 0.70, juce::Decibels::decibelsToGain (trebDb));
        auto presC = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 4200.0, 1.10, juce::Decibels::decibelsToGain (presDb));

        // mic position: X moves from cone centre (bright) to edge (dark),
        // Y moves from on-axis to off-axis, which mostly costs top end
        const float axisDb = juce::jmap (p.micX, 0.0f, 100.0f,  4.0f, -7.0f);
        const float distDb = juce::jmap (std::abs (p.micY - 50.0f), 0.0f, 50.0f, 0.0f, -5.0f);

        auto cabLoC = juce::dsp::IIR::Coefficients<float>::makeHighPass (
            sampleRate, 78.0, 0.70);
        auto cabHiC = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 3600.0, 0.60,
            juce::Decibels::decibelsToGain (0.0f));

        auto cabTopC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 3400.0, 0.65,
            juce::Decibels::decibelsToGain (axisDb + distDb));

        juce::ignoreUnused (cabHiC);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            lowShelf [(size_t) ch].coefficients = lowC;
            midPeak  [(size_t) ch].coefficients = midC;
            highShelf[(size_t) ch].coefficients = highC;
            presPeak [(size_t) ch].coefficients = presC;
            cabLow   [(size_t) ch].coefficients = cabLoC;
            cabHigh  [(size_t) ch].coefficients = cabTopC;
        }

        const float dblMs = 11.0f + (p.micY * 0.06f);
        doublerSamples = dblMs * 0.001f * static_cast<float> (sampleRate);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = juce::jmin (numChannels, buffer.getNumChannels());
        const int numSm = buffer.getNumSamples();

        const float trim = juce::Decibels::decibelsToGain (params.trimDb);
        const float out  = juce::jmap (params.output, 0.0f, 100.0f, 0.0f, 1.5f);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);

            for (int i = 0; i < numSm; ++i)
            {
                float x = d[i] * trim;

                if (! params.stackPost)
                    x = toneStack (ch, x);

                x = runCapture (ch, x);

                if (params.stackPost)
                    x = toneStack (ch, x);

                if (params.cabSim)
                {
                    x = cabLow [(size_t) ch].processSample (x);
                    x = cabHigh[(size_t) ch].processSample (x);
                }

                if (params.doubler && numCh > 1)
                {
                    auto& dl = doublerDelay[(size_t) ch];
                    dl.pushSample (0, x);
                    const float delayed = dl.popSample (
                        0, doublerSamples * (ch == 0 ? 1.0f : 1.37f));
                    x = 0.62f * x + 0.38f * delayed;
                }

                d[i] = x * out;
            }
        }
    }

    // ------------------------------------------------------------------
    //  Capture engine seams — see the class comment
    // ------------------------------------------------------------------

    /** Loads a .nam capture. Returns false until the engine is wired in. */
    bool loadCapture (const juce::File& namFile)
    {
        juce::ignoreUnused (namFile);
        capturePath = namFile.getFullPathName();
        captureLoaded = false;          // flip once NeuralAudio is linked
        return captureLoaded;
    }

    juce::String currentCaptureName() const
    {
        return capturePath.isEmpty() ? "NO CAPTURE LOADED"
                                     : juce::File (capturePath).getFileNameWithoutExtension();
    }

    /** The capture's own file, for a preset to remember ("" when none is loaded). */
    const juce::String& currentCapturePath() const noexcept { return capturePath; }

    bool  isCaptureLoaded() const noexcept { return captureLoaded; }
    int   latencySamples()  const noexcept { return captureLoaded ? modelLatency : 0; }

private:
    float toneStack (int ch, float x) noexcept
    {
        x = lowShelf [(size_t) ch].processSample (x);
        x = midPeak  [(size_t) ch].processSample (x);
        x = highShelf[(size_t) ch].processSample (x);
        x = presPeak [(size_t) ch].processSample (x);
        return x;
    }

    /** Identity until the neural engine lands. The signature is already the
        one the engine needs, so wiring it is a body swap, not a refactor. */
    float runCapture (int ch, float x) noexcept
    {
        juce::ignoreUnused (ch);
        return captureLoaded ? x : x;
    }

    double sampleRate  = 44100.0;
    int    numChannels = 2;
    Params params;

    using Filter = juce::dsp::IIR::Filter<float>;
    std::vector<Filter> lowShelf, midPeak, highShelf, presPeak, cabLow, cabHigh;
    std::vector<juce::dsp::DelayLine<float>> doublerDelay;

    float        doublerSamples = 480.0f;
    juce::String capturePath;
    bool         captureLoaded  = false;
    int          modelLatency   = 0;
};

} // namespace axis::dsp
