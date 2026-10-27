#pragma once

#include <juce_dsp/juce_dsp.h>

#include "CabSpeakers.h"

#include <array>
#include <atomic>

namespace axis::dsp
{

/** CAB — the cabinet and its microphone, after the amp.

    One of four modelled cabinets (CabSpeakers.h), or an impulse response: one
    from the AXIS pack beside Sonify, or any IR file loaded one by one. Either
    way the microphone (CAB_MIC_X / CAB_MIC_Y, dragged on the cabinet's
    picture) tilts the top end the way moving a real one does.

    A change of cabinet, or the cabinet switched off or on, crossfades over a
    few milliseconds: cut over at once, the two filters' states don't meet and
    it clicks.

    One channel, as the amp in front of it is: AmpModel has one cabinet for
    each side it plays.

    Threads: setType and setMic from anywhere; loadIr from the message thread
    (the convolution loads it on a thread of its own and swaps it in, with a
    crossfade of its own); process on the audio thread.
*/
class Cabinet
{
public:
    /** CAB_TYPE's last choice: the impulse response. */
    static constexpr int irType = cab::numSpeakers;

    /** What CAB_TYPE falls back to when it says IR and none is loaded. */
    static constexpr int fallbackType = 2;

    Cabinet() : convolution (juce::dsp::Convolution::NonUniform { 512 }) {}

    void prepare (double newSampleRate, int maximumBlock)
    {
        sampleRate = newSampleRate;
        maxBlock = juce::jmax (1, maximumBlock);
        convolution.prepare ({ sampleRate, (juce::uint32) maxBlock, 1 });

        // Each modelled cabinet's level at this rate, now rather than when it's picked.
        for (int i = 0; i < cab::numSpeakers; ++i)
            levels[(size_t) i] = cab::Speaker::levelFor (i, sampleRate);

        older.assign ((size_t) maxBlock, 0.0f);
        fadeLength = juce::jmax (1, (int) (0.010 * sampleRate));
        reset();
    }

    void reset()
    {
        for (auto& s : slots)
        {
            s.path = noPath;
            s.speaker.reset();
            s.mic.reset();
        }

        convolution.reset();
        fadeLeft = 0;
        micX0 = micY0 = -1.0f;
    }

    void setType (int index) noexcept           { typeWanted.store (juce::jlimit (0, irType, index)); }
    void setMic (float x, float y) noexcept     { micX.store (x); micY.store (y); }

    /** An impulse response for the IR choice, as it is (AxisPack::readIr has conditioned it).
        Message thread. */
    void loadIr (juce::AudioBuffer<float>&& ir, double rate, const juce::String& name)
    {
        // The amp is mono, so a stereo IR is folded to one channel.
        if (ir.getNumChannels() > 1)
        {
            ir.addFrom (0, 0, ir, 1, 0, ir.getNumSamples());
            ir.applyGain (0, 0, ir.getNumSamples(), 0.5f);
            ir.setSize (1, ir.getNumSamples(), true);
        }

        convolution.loadImpulseResponse (std::move (ir), rate, juce::dsp::Convolution::Stereo::no,
                                         juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);

        const juce::SpinLock::ScopedLockType hold (nameLock);
        irName = name;
        irLoaded.store (true);
    }

    /** Back to no IR (the IR choice plays the fallback cabinet until one is loaded). */
    void clearIr()
    {
        irLoaded.store (false);
        irReady.store (false);
        const juce::SpinLock::ScopedLockType hold (nameLock);
        irName.clear();
    }

    bool hasIr() const noexcept                 { return irLoaded.load(); }

    juce::String getIrName() const
    {
        const juce::SpinLock::ScopedLockType hold (nameLock);
        return irName;
    }

    /** The modelled cabinet in use, or irType when the IR plays. An IR just loaded plays once
        the convolution has really swapped it in (until then its fallback cabinet does), so the
        amp is never heard through nothing for those few milliseconds. */
    int playing() const noexcept
    {
        const int t = typeWanted.load();
        return t == irType ? (irReady.load() ? irType : fallbackType) : t;
    }

    /** One channel, in place. on: false, the cabinet is out (and fades out). At most the
        maximum block prepare was told. */
    void process (float* data, int numSamples, bool on) noexcept
    {
        numSamples = juce::jmin (numSamples, maxBlock);

        // The convolution starts with a one-sample pass-through until the first IR is swapped in
        // on its loading thread: an IR counts as in once it has.
        irReady.store (irLoaded.load() && convolution.getCurrentIRSize() > 1);

        // The microphone, when it moved: both slots, so a fade has it too.
        const float x = micX.load(), y = micY.load();

        if (x != micX0 || y != micY0)
        {
            for (auto& s : slots)
                s.mic.set (x, y, sampleRate);

            micX0 = x;
            micY0 = y;
        }

        // A different path: into the other slot, and the fade from this one to it.
        const int wanted = on ? playing() : offPath;

        if (wanted != slots[(size_t) now].path && fadeLeft == 0)
        {
            const int from = now;
            now = 1 - now;
            auto& next = slots[(size_t) now];
            next.path = wanted;
            next.mic.reset();

            if (wanted >= 0 && wanted < cab::numSpeakers)
                next.speaker.design (wanted, sampleRate, levels[(size_t) wanted]);
            else if (wanted == irType && slots[(size_t) from].path != irType)
                convolution.reset();    // nothing left over from the last time it played

            // The very first path needs no fade: there's nothing before it.
            fadeLeft = slots[(size_t) from].path == noPath ? 0 : fadeLength;
        }

        if (fadeLeft > 0)
        {
            // The old path into the side buffer, the new one in place, then the two together.
            std::copy (data, data + numSamples, older.begin());
            render (slots[(size_t) (1 - now)], older.data(), numSamples);
            render (slots[(size_t) now], data, numSamples);

            for (int i = 0; i < numSamples && fadeLeft > 0; ++i)
            {
                const float g = 1.0f - (float) fadeLeft / (float) fadeLength;
                data[i] = older[(size_t) i] + g * (data[i] - older[(size_t) i]);
                --fadeLeft;
            }

            if (fadeLeft == 0)
                slots[(size_t) (1 - now)].path = noPath;

            return;
        }

        render (slots[(size_t) now], data, numSamples);
    }

private:
    static constexpr int offPath = -1, noPath = -2;

    struct Slot
    {
        int path = noPath;      // a modelled cabinet, irType, offPath (the cabinet out), or noPath
        cab::Speaker speaker;
        cab::Mic mic;
    };

    void render (Slot& slot, float* data, int numSamples) noexcept
    {
        if (slot.path == offPath || slot.path == noPath)
            return;

        if (slot.path == irType)
        {
            float* channels[] { data };
            juce::dsp::AudioBlock<float> block (channels, 1, (size_t) numSamples);
            convolution.process (juce::dsp::ProcessContextReplacing<float> (block));

            for (int i = 0; i < numSamples; ++i)
                data[i] = (float) slot.mic.process (data[i]);

            return;
        }

        for (int i = 0; i < numSamples; ++i)
            data[i] = (float) slot.mic.process (slot.speaker.process (data[i]));
    }

    double sampleRate = 48000.0;
    int maxBlock = 512;

    juce::dsp::Convolution convolution;
    std::array<Slot, 2> slots;
    int now = 0;
    std::array<double, cab::numSpeakers> levels { 1.0, 1.0, 1.0, 1.0 };

    std::vector<float> older;
    int fadeLength = 480, fadeLeft = 0;

    std::atomic<int> typeWanted { fallbackType };
    std::atomic<float> micX { 36.0f }, micY { 50.0f };
    std::atomic<bool> irLoaded { false }, irReady { false };
    float micX0 = -1.0f, micY0 = -1.0f;

    mutable juce::SpinLock nameLock;
    juce::String irName;
};

} // namespace axis::dsp
