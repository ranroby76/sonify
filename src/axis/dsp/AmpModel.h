#pragma once

#include <juce_dsp/juce_dsp.h>

#include "AmpEngine.h"
#include "Cabinet.h"

#include <array>
#include <memory>
#include <vector>

namespace axis::dsp
{

/** AMP — the amp and its cabinet.

    Five amps on one engine (AmpEngine.h, AmpVoices.h): tubes modelled from
    Koren's 12AX7, the real tone stacks from their component values, a power
    section with sag, presence and resonance. Its knobs are the amp's own:
    GAIN, BASS, MID, TREBLE, PRESENCE and MASTER, with BRIGHT (the bright cap
    on the gain pot) and TIGHT (the low end trimmed before the first stage).
    Then the cabinet (Cabinet.h): a modelled one, or an impulse response.

    Mono or stereo, as what comes in is. A guitar is mono, and Sonify hands
    a mono source over on both sides alike: then one amp plays and comes out
    on both (the amp's 4x oversampling is most of what AXIS costs, so it runs
    once). When the two sides differ - a stereo track, a stereo pedal in
    front - a second amp and cabinet, the same in every setting, take the
    right side, and each side has its own. It starts when they first differ
    (fading in over 20 ms, so it never clicks) and stops again once they've
    been alike for half a second.
*/
class AmpModel
{
public:
    static constexpr int numVoices = amp::AmpEngine::numVoices;
    static constexpr int oversamplingLog2 = 2;     // 4x: aliasing down at the measurement floor even at full gain

    static juce::StringArray voiceNames()
    {
        juce::StringArray names;

        for (const auto& v : amp::voiceSpecs())
            names.add (v.name);

        return names;
    }

    static juce::String voiceDescription (int index)
    {
        return amp::voiceSpecs()[(size_t) juce::jlimit (0, numVoices - 1, index)].description;
    }

    static juce::StringArray cabinetNames()
    {
        juce::StringArray names;

        for (const auto& s : cab::speakerSpecs())
            names.add (s.name);

        names.add ("IR");
        return names;
    }

    struct Params
    {
        int voice = 1;
        float gain = 50.0f, bass = 50.0f, mid = 50.0f, treble = 50.0f, presence = 50.0f, master = 50.0f;   // 0..100
        bool bright = false, tight = false, cab = true;
        int cabType = Cabinet::fallbackType;
        float micX = 36.0f, micY = 50.0f;
    };

    AmpModel()
    {
        // The tubes' curves, built here on the message thread, before any audio asks for them.
        amp::AmpEngine::curves();
    }

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        maxBlock = (int) juce::jmax ((juce::uint32) 1, spec.maximumBlockSize);

        for (auto& lane : lanes)
        {
            lane.oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
                1, (size_t) oversamplingLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
            lane.oversampling->initProcessing ((size_t) maxBlock);
            lane.signal.setSize (1, maxBlock);
            lane.engine.prepare (spec.sampleRate * (double) (1 << oversamplingLog2));
            lane.cabinet.prepare (spec.sampleRate, maxBlock);
        }

        wide.assign ((size_t) maxBlock << oversamplingLog2, 0.0);
        stereoStep = 1.0f / (float) juce::jmax (1.0, 0.020 * spec.sampleRate);
        holdLength = juce::jmax (1, (int) (0.5 * spec.sampleRate));

        // Switched out, it still delays by what it reports (latencySamples): see process.
        bypassDelay.setSize (2, juce::jmax (1, latencySamples()));
        bypassDelay.clear();
        bypassAt = 0;
        dry.setSize (2, maxBlock);
        switchStep = 1.0f / (float) juce::jmax (1.0, 0.010 * spec.sampleRate);
        reset();
    }

    void reset()
    {
        resetAmp();
        bypassDelay.clear();
        dry.clear();
    }

    /** Each block, from the parameters. */
    void setParams (const Params& p) noexcept
    {
        amp::AmpControls c;
        c.gain = p.gain * 0.01;
        c.bass = p.bass * 0.01;
        c.mid = p.mid * 0.01;
        c.treble = p.treble * 0.01;
        c.presence = p.presence * 0.01;
        c.master = p.master * 0.01;
        c.bright = p.bright;
        c.tight = p.tight;

        // Both sides alike, so the second is ready whenever it's wanted.
        for (auto& lane : lanes)
        {
            lane.engine.setVoice (p.voice);
            lane.engine.setControls (c);
            lane.cabinet.setType (p.cabType);
            lane.cabinet.setMic (p.micX, p.micY);
        }

        cabOn = p.cab;
    }

    /** active: false, the AMP is switched out - its input goes straight through, delayed by
        the latency AXIS reports for it (the host compensates for that either way), so switching
        it never moves the sound in time. Either way it crossfades over a few milliseconds, and
        switched back in after being out, the amp starts from clean. */
    void process (juce::AudioBuffer<float>& buffer, bool active)
    {
        const int channels = buffer.getNumChannels();
        const int total = buffer.getNumSamples();

        if (lanes[0].oversampling == nullptr || channels == 0)
            return;

        const float target = active ? 1.0f : 0.0f;

        for (int start = 0; start < total; start += maxBlock)
        {
            const int n = juce::jmin (maxBlock, total - start);
            const int both = juce::jmin (2, channels);

            // The dry path, always: the input delayed by the latency, kept running so a switch
            // out has it ready.
            for (int ch = 0; ch < both; ++ch)
                dry.copyFrom (ch, 0, buffer, ch, start, n);

            delayThrough (n, both);

            // All the way out, and staying out: the dry path is all there is.
            if (wet <= 0.0f && target <= 0.0f)
            {
                for (int ch = 0; ch < both; ++ch)
                    buffer.copyFrom (ch, start, dry, ch, 0, n);

                wasOut = true;
                continue;
            }

            if (wasOut)
            {
                resetAmp();
                wasOut = false;
            }

            processAmp (buffer, start, n);

            // On the way in or out: dry and amp, crossfaded.
            if (wet != target)
            {
                for (int i = 0; i < n; ++i)
                {
                    wet = wet < target ? juce::jmin (target, wet + switchStep) : juce::jmax (target, wet - switchStep);

                    for (int ch = 0; ch < both; ++ch)
                    {
                        const float d = dry.getSample (ch, i);
                        buffer.setSample (ch, start + i, d + wet * (buffer.getSample (ch, start + i) - d));
                    }
                }
            }
        }
    }

    /** What the oversampling's filters delay it by, in samples at the plain rate. */
    int latencySamples() const noexcept
    {
        return lanes[0].oversampling != nullptr ? (int) std::lround (lanes[0].oversampling->getLatencyInSamples()) : 0;
    }

    /** The cabinet, for what it says (both sides' are the same). */
    const Cabinet& getCabinet() const noexcept  { return lanes[0].cabinet; }

    /** An impulse response for both sides' cabinets (see Cabinet::loadIr). Message thread. */
    void loadIr (juce::AudioBuffer<float>&& ir, double rate, const juce::String& name)
    {
        juce::AudioBuffer<float> copy (ir);
        lanes[1].cabinet.loadIr (std::move (copy), rate, name);
        lanes[0].cabinet.loadIr (std::move (ir), rate, name);
    }

    void clearIr()
    {
        for (auto& lane : lanes)
            lane.cabinet.clearIr();
    }

    /** Whether the right side has an amp of its own just now (for tests). */
    bool isStereo() const noexcept              { return spread > 0.0f; }

private:
    void processAmp (juce::AudioBuffer<float>& buffer, int start, int n)
    {
        const int channels = buffer.getNumChannels();
        auto* l = lanes[0].signal.getWritePointer (0);
        auto* r = lanes[1].signal.getWritePointer (0);
        const float* inL = buffer.getReadPointer (0, start);
        const float* inR = channels > 1 ? buffer.getReadPointer (1, start) : inL;

        // Do the sides differ? Then the right side wants its own amp, until they've been alike
        // for a while.
        if (channels > 1)
        {
            bool differ = false;

            for (int i = 0; i < n && ! differ; ++i)
                differ = std::abs (inL[i] - inR[i]) > 1.0e-6f;

            if (differ)
                alikeFor = 0;
            else
                alikeFor = juce::jmin (holdLength, alikeFor + n);
        }
        else
        {
            alikeFor = holdLength;
        }

        const float spreadTarget = alikeFor < holdLength ? 1.0f : 0.0f;

        if (spread <= 0.0f && spreadTarget > 0.0f)
            resetLane (lanes[1]);       // starting from clean, faded in

        const bool both = spread > 0.0f || spreadTarget > 0.0f;

        // Each side's amp takes its own side (alike, the left is both). The left side's never
        // changes when the right one's amp comes or goes: only what the right side hears does.
        juce::FloatVectorOperations::copy (l, inL, n);

        if (both)
            juce::FloatVectorOperations::copy (r, inR, n);

        runLane (lanes[0], l, n);

        if (! both)
        {
            for (int ch = 0; ch < channels; ++ch)
                buffer.copyFrom (ch, start, l, n);

            return;
        }

        runLane (lanes[1], r, n);

        // The right side: the left side's amp, crossfaded into its own as that fades in (and back
        // as it fades out). Stereo is only ever two channels: any more get the left side.
        for (int i = 0; i < n; ++i)
        {
            spread = spread < spreadTarget ? juce::jmin (spreadTarget, spread + stereoStep) : juce::jmax (spreadTarget, spread - stereoStep);
            r[i] = l[i] + spread * (r[i] - l[i]);
        }

        buffer.copyFrom (0, start, l, n);
        buffer.copyFrom (1, start, r, n);

        for (int ch = 2; ch < channels; ++ch)
            buffer.copyFrom (ch, start, l, n);
    }

    struct Lane
    {
        amp::AmpEngine engine;
        Cabinet cabinet;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        juce::AudioBuffer<float> signal;
    };

    /** One side: the amp, oversampled, then the cabinet. In place. */
    void runLane (Lane& lane, float* m, int n) noexcept
    {
        // A bad sample from upstream stops here.
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (m[i]))
                m[i] = 0.0f;

        float* monoChannels[] { m };
        juce::dsp::AudioBlock<float> block (monoChannels, 1, (size_t) n);
        auto up = lane.oversampling->processSamplesUp (block);
        const auto upCount = (int) up.getNumSamples();
        auto* u = up.getChannelPointer (0);

        for (int i = 0; i < upCount; ++i)
            wide[(size_t) i] = (double) u[i];

        lane.engine.process (wide.data(), upCount);

        for (int i = 0; i < upCount; ++i)
            u[i] = (float) wide[(size_t) i];

        lane.oversampling->processSamplesDown (block);

        // The cabinet, at the plain rate (switched out, it fades out rather than clicking).
        lane.cabinet.process (m, n, cabOn);
    }

    static void resetLane (Lane& lane)
    {
        if (lane.oversampling != nullptr)
            lane.oversampling->reset();

        lane.engine.restart();
        lane.cabinet.reset();
        lane.signal.clear();
    }

    void resetAmp()
    {
        for (auto& lane : lanes)
            resetLane (lane);

        spread = 0.0f;
        alikeFor = holdLength;
    }

    /** The dry buffer's first n samples, through the delay line. */
    void delayThrough (int n, int channels) noexcept
    {
        const int length = bypassDelay.getNumSamples();

        if (latencySamples() <= 0 || length <= 0)
            return;

        int at = bypassAt;

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* d = dry.getWritePointer (ch);
            auto* line = bypassDelay.getWritePointer (ch);
            at = bypassAt;

            for (int i = 0; i < n; ++i)
            {
                const float out = line[at];
                line[at] = d[i];
                d[i] = out;
                at = at + 1 < length ? at + 1 : 0;
            }
        }

        bypassAt = at;
    }

    std::array<Lane, 2> lanes;      // the left side (or both, alike), and the right side's own
    bool cabOn = true;
    std::vector<double> wide;
    int maxBlock = 512;

    float spread = 0.0f, stereoStep = 0.001f;   // how much of the right side's own amp is heard
    int alikeFor = 0, holdLength = 24000;       // how long the sides have been alike, in samples

    juce::AudioBuffer<float> bypassDelay, dry;
    int bypassAt = 0;
    bool wasOut = false;
    float wet = 1.0f, switchStep = 0.002f;      // how much of the amp is heard: 0 switched out, 1 in
};

} // namespace axis::dsp
