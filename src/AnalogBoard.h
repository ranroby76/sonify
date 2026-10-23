// D:\workspace\Fanan_web_hub\src\AnalogBoard.h

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "SonifyDsp.h"

#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

//==============================================================================
/**
    The Analog Board (a tab of its own): a second mixer beside the Console, an analog desk's.
    When the song is mixed on it, every track's dry sound (its instrument, or its clips and
    input, before any insert) comes into a strip of the board instead of the Console: the
    Console's inserts, sends, FX channels, buses and Main are left out, and the board's own
    tools mix the song into Main's outputs:

      each strip   the preamp (Trim, Drive, phase), a high-pass filter, a four-band EQ, a
                   compressor, two sends (the Plate and the Tape Echo), pan, fader, mute, solo
      the master   the Plate (a plate reverb) and the Tape Echo, back into the mix, then the Bus
                   Compressor, the Tape Machine and the master fader

    Every setting is an atomic anyone may set at any time (the page, Sonify AI, MIDI Link); the
    audio thread reads them each block. A strip belongs to its track (Track::board), so it comes
    and goes with it; the master is the engine's.

    prepare() allocates; nothing else does.
*/
namespace AnalogBoard
{
    //==============================================================================
    /** A setting as the page, songs and Sonify AI know it. */
    struct ParamSpec
    {
        enum class Kind { knob, toggle, choice, fader };

        const char* id;              // "lmfFreq": songs and Sonify AI name it so
        const char* name;            // "LMF Freq": what the page shows
        const char* section;         // "Preamp", "EQ" ...
        Kind kind;
        float minimum, maximum, defaultValue;
        const char* unit;            // "dB", "Hz", "ms", "%", or ""
        const char* choices;         // a choice's names, "2:1|4:1|10:1"; otherwise ""

        juce::StringArray choiceNames() const;
        float clamp (float value) const noexcept;

        /** The value as the page shows it: "-3.5 dB", "2.4 kHz", "Off", "4:1". */
        juce::String text (float value) const;
    };

    //==============================================================================
    // A strip's settings, in this order.
    enum StripParam
    {
        trim, drive, phase,
        hpf,
        lf, lfFreq, lmf, lmfFreq, hmf, hmfFreq, hf, hfFreq, eqOn,
        compOn, threshold, ratio, release, fastAttack, makeup,
        sendPlate, sendEcho,
        pan, volume, mute, solo,
        numStripParams
    };

    // The master's.
    enum MasterParam
    {
        plateDecay, plateTone, platePredelay, plateReturn,
        echoSync, echoNote, echoTime, echoFeedback, echoTone, echoWow, echoReturn,
        busOn, busThreshold, busRatio, busAttack, busRelease, busMakeup, busMix,
        tapeOn, tapeDrive, tapeSpeed, tapeWow, tapeHiss,
        masterVolume,
        numMasterParams
    };

    const std::array<ParamSpec, numStripParams>& stripSpecs();
    const std::array<ParamSpec, numMasterParams>& masterSpecs();

    /** A setting's place by its id; -1 when there's none. */
    int stripIndexOf (const juce::String& id);
    int masterIndexOf (const juce::String& id);

    constexpr float offDb = -60.0f;     // a send or a return at or below this is off
    constexpr float silentDb = -72.0f;  // a fader at or below this is closed

    //==============================================================================
    /** Settings as atomics, with a count of changes the audio thread watches. */
    template <size_t count>
    class Settings
    {
    public:
        explicit Settings (const std::array<ParamSpec, count>& specList) : specs (specList)   { reset(); }

        float get (int index) const noexcept
        {
            return juce::isPositiveAndBelow (index, static_cast<int> (count)) ? values[static_cast<size_t> (index)].load (std::memory_order_relaxed) : 0.0f;
        }

        bool isOn (int index) const noexcept   { return get (index) >= 0.5f; }

        /** Returns the value used (clamped, choices and switches whole). */
        float set (int index, float value) noexcept
        {
            if (! juce::isPositiveAndBelow (index, static_cast<int> (count)))
                return 0.0f;

            const auto used = specs[static_cast<size_t> (index)].clamp (value);
            values[static_cast<size_t> (index)].store (used, std::memory_order_relaxed);
            changes.fetch_add (1, std::memory_order_release);
            return used;
        }

        /** Every setting back to where a new board starts. */
        void reset() noexcept
        {
            for (size_t i = 0; i < count; ++i)
                values[i].store (specs[i].defaultValue, std::memory_order_relaxed);

            changes.fetch_add (1, std::memory_order_release);
        }

        juce::uint32 getChanges() const noexcept   { return changes.load (std::memory_order_acquire); }

        /** { <id>: value }: every setting, or only those away from where they start (songs keep those). */
        juce::var toVar (bool all = false) const
        {
            auto* object = new juce::DynamicObject();

            for (size_t i = 0; i < count; ++i)
            {
                const auto value = values[i].load (std::memory_order_relaxed);

                if (all || std::abs (value - specs[i].defaultValue) > 1.0e-6f)
                    object->setProperty (specs[i].id, static_cast<double> (value));
            }

            return juce::var (object);
        }

        /** Settings from songs or Sonify AI by id: the ones named change, the rest stay as they are
            (or start where a new board starts, when asked). A value may be a number, true or false,
            a number as text, or a choice's name ("4:1"). Returns what was left out, each as
            "<id> (why)". */
        juce::StringArray fromVar (const juce::var& data, bool resetFirst)
        {
            if (resetFirst)
                reset();

            juce::StringArray leftOut;

            if (const auto* object = data.getDynamicObject())
            {
                for (const auto& [key, value] : object->getProperties())
                {
                    size_t index = 0;

                    while (index < count && key.toString() != specs[index].id)
                        ++index;

                    if (index == count)
                    {
                        leftOut.add (key.toString() + " (not a setting)");
                        continue;
                    }

                    const auto& spec = specs[index];
                    double number = std::numeric_limits<double>::quiet_NaN();

                    if (value.isInt() || value.isInt64() || value.isDouble() || value.isBool())
                    {
                        number = static_cast<double> (value);
                    }
                    else if (value.isString())
                    {
                        const auto text = value.toString().trim();
                        const auto names = spec.choiceNames();

                        if (const auto choice = names.indexOf (text, true); choice >= 0)
                            number = choice;
                        else if (text.equalsIgnoreCase ("on") || text.equalsIgnoreCase ("true"))
                            number = 1.0;
                        else if (text.equalsIgnoreCase ("off") || text.equalsIgnoreCase ("false"))
                            number = 0.0;
                        else if (text.isNotEmpty() && text.containsOnly ("0123456789.-+eE"))
                            number = text.getDoubleValue();
                    }

                    if (std::isfinite (number))
                    {
                        set (static_cast<int> (index), static_cast<float> (number));
                    }
                    else
                    {
                        leftOut.add (key.toString() + (spec.kind == ParamSpec::Kind::choice
                                                           ? " (a choice: its number, or one of " + spec.choiceNames().joinIntoString (", ") + ")"
                                                           : juce::String (" (a number)")));
                    }
                }
            }

            return leftOut;
        }

        const std::array<ParamSpec, count>& specs;

    private:
        std::array<std::atomic<float>, count> values {};
        std::atomic<juce::uint32> changes { 0 };
    };

    //==============================================================================
    /** What the board's meters show, read on the message thread: a VU's level (dBFS, RMS with a
        VU's 300 ms swing), the peak since the last read, and the compressor's reduction. */
    struct Meter
    {
        std::atomic<float> vuLeft { -100.0f }, vuRight { -100.0f };
        std::atomic<float> peakLeft { 0.0f }, peakRight { 0.0f };
        std::atomic<float> reduction { 0.0f };

        void measure (const float* left, const float* right, int numSamples, double sampleRate, float reductionDb) noexcept;
        void clear() noexcept;

    private:
        double powerLeft = 0.0, powerRight = 0.0;   // audio thread
    };

    class Master;

    //==============================================================================
    /** A channel's compressor: a peak follower, its gain worked out every 16 samples and glided
        between (cheap enough for a strip on every track), with a soft knee. */
    class ChannelCompressor
    {
    public:
        void prepare (double rate) noexcept    { sampleRate = rate; reset(); }

        void reset() noexcept
        {
            envelope = 0.0f;
            gain = targetGain = makeup;
            step = 0.0f;
            count = 0;
            reductionDb = 0.0f;
        }

        void set (float thresholdDb, float ratio, float attackMs, float releaseMs, float kneeDb, float makeupDb) noexcept
        {
            const auto coefficient = [this] (float ms) { return static_cast<float> (std::exp (-1.0 / (juce::jmax (0.01, static_cast<double> (ms)) * 0.001 * sampleRate))); };
            threshold = thresholdDb;
            slope = ratio > 1.0f ? 1.0f - 1.0f / ratio : 0.0f;
            attack = coefficient (attackMs);
            release = coefficient (releaseMs);
            knee = juce::jmax (0.0f, kneeDb);
            makeup = juce::Decibels::decibelsToGain (makeupDb);
        }

        void process (float* left, float* right, int numSamples) noexcept
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto peak = juce::jmax (std::abs (left[i]), std::abs (right[i]));
                envelope = peak + (peak > envelope ? attack : release) * (envelope - peak);

                if (count == 0)
                {
                    const auto over = reductionAt (envelope);
                    reductionDb = over;
                    targetGain = makeup * juce::Decibels::decibelsToGain (-over);
                    step = (targetGain - gain) / 16.0f;
                    count = 16;
                }

                --count;
                gain += step;
                left[i] *= gain;
                right[i] *= gain;
            }
        }

        /** Switched in: it starts as if it had been listening all along (its follower at this
            block's peak, its gain where that puts it), not from rest. */
        void prime (const float* left, const float* right, int numSamples) noexcept
        {
            envelope = 0.0f;

            for (int i = 0; i < numSamples; ++i)
                envelope = juce::jmax (envelope, std::abs (left[i]), std::abs (right[i]));

            gain = targetGain = makeup * juce::Decibels::decibelsToGain (-reductionAt (envelope));
            step = 0.0f;
            count = 16;
        }

        float getReductionDb() const noexcept   { return reductionDb; }

    private:
        float reductionAt (float level) const noexcept
        {
            const auto above = (level > 1.0e-6f ? 20.0f * std::log10 (level) : -120.0f) - threshold;

            if (2.0f * above <= -knee)
                return 0.0f;

            if (knee > 0.0f && 2.0f * std::abs (above) <= knee)
                return slope * (above + knee * 0.5f) * (above + knee * 0.5f) / (2.0f * knee);

            return slope * above;
        }

        double sampleRate = 48000.0;
        float threshold = 0.0f, slope = 0.0f, attack = 0.0f, release = 0.0f, knee = 4.0f, makeup = 1.0f;
        float envelope = 0.0f, gain = 1.0f, targetGain = 1.0f, step = 0.0f, reductionDb = 0.0f;
        int count = 0;
    };

    //==============================================================================
    /** One track's channel strip. */
    class Strip
    {
    public:
        Strip();

        Settings<numStripParams> settings { stripSpecs() };
        Meter meter;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        /** The track's dry block through the strip: the result (after the fader) is kept as
            getOutput() gives it, mixed into the master's bus and sent to its Plate and Echo.
            audible: not muted, and not left out by another strip's solo. settle: the fader and
            sends start where they are, without a ramp (a mixdown's first block). */
        void process (const float* left, const float* right, int numSamples, Master& master, bool audible, bool settle) noexcept;

        const float* getOutput (int channel) const noexcept   { return output.getReadPointer (juce::jlimit (0, 1, channel)); }
        int getOutputSamples() const noexcept                 { return outputSamples; }

    private:
        /** The preamp and the filters as the settings make them. */
        struct Chain
        {
            float trimGain = 1.0f, driveGain = 1.0f, driveBias = 0.0f, driveBiasOut = 0.0f, driveNorm = 1.0f;
            bool driving = false, hpfOn = false, eqOn = true;
            std::array<SonifyDsp::Biquad, 5> filters {};           // high-pass, LF, LMF, HMF, HF
        };

        /** One side's memory along the chain. */
        struct ChainState
        {
            std::array<SonifyDsp::BiquadState, 5> filters {};
            double dcIn = 0.0, dcOut = 0.0;
        };

        Chain workOutChain() const noexcept;
        static float run (const Chain& chain, ChainState& state, float input) noexcept;

        double sampleRate = 48000.0;
        juce::uint32 seenChanges = 0xffffffffu;
        Chain chain;                                    // as it plays now; a change crossfades over a block
        std::array<ChainState, 2> states {};
        ChannelCompressor compressor;
        float lastLeftGain = 1.0f, lastRightGain = 1.0f, lastPlate = 0.0f, lastEcho = 0.0f;
        juce::AudioBuffer<float> output, scratch;
        int outputSamples = 0;
        bool wasCompressing = false, fresh = true;
    };

    //==============================================================================
    /** The master section: what the strips bring in, the Plate and the Tape Echo, the Bus
        Compressor, the Tape Machine and the master fader. */
    class Master
    {
    public:
        Master();
        ~Master();

        Settings<numMasterParams> settings { masterSpecs() };
        Meter meter;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        /** As reset(), on the audio thread: the Plate's and the Echo's long memories are cleared
            a little each block (they're silent meanwhile, a few milliseconds), never all at once. */
        void startFresh() noexcept;

        /** Starts a block: the bus and the sends empty. */
        void begin (int numSamples) noexcept;

        // Where the strips mix in, this block.
        float* bus (int channel) noexcept         { return busBuffer.getWritePointer (channel); }
        float* plateSend (int channel) noexcept   { return plateBuffer.getWritePointer (channel); }
        float* echoSend (int channel) noexcept    { return echoBuffer.getWritePointer (channel); }
        int getBlockSize() const noexcept         { return busBuffer.getNumSamples(); }

        /** Ends the block: the returns, the bus compressor, the tape and the master fader, into
            left and right (which it writes, not adds). bpm times a synced echo. */
        void finish (float* left, float* right, int numSamples, double bpm, bool settle) noexcept;

    private:
        class Plate;
        class Echo;
        class Tape;

        void refresh() noexcept;

        /** A stage that's out (or switching): it runs on a copy of the bus; switching, the bus
            crossfades to it (switched in) or from it (switched out) over the block. */
        template <typename Process>
        void crossfade (float* left, float* right, int numSamples, bool on, bool switching, Process&& process) noexcept;

        double sampleRate = 48000.0;
        juce::uint32 seenChanges = 0xffffffffu;
        juce::AudioBuffer<float> busBuffer, plateBuffer, echoBuffer, returnBuffer;
        std::unique_ptr<Plate> plate;
        std::unique_ptr<Echo> echo;
        std::unique_ptr<Tape> tape;
        SonifyDsp::Compressor busCompressor;
        bool busWasOn = false, tapeWasOn = false, clearing = false;
        float lastMasterGain = 1.0f, lastPlateReturn = 1.0f, lastEchoReturn = 1.0f;
    };
}
