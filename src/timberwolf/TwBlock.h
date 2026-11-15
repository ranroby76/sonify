// D:\workspace\Fanan_web_hub\src\timberwolf\TwBlock.h
#pragma once
//==============================================================================
//  TwBlock.h  -  one effect of Timber Wolf's chain, and what the chain knows of it.
//
//  Every effect in Timber Wolf (a drive, an amp, a reverb, the split and the
//  blend too) is a Block: it is told the sample rate once, then processes a
//  stereo buffer in place, block after block, on the audio thread.  Its knobs
//  arrive as plain values (dB, ms, percent...) in knob[] before each buffer;
//  what a knob is - its name, range, default, unit, taper, or the names of a
//  selector's positions - is its KnobSpec in the block's BlockInfo.
//
//  BlockInfo is the effect as the rest of the plugin sees it: its id (kept in
//  presets and songs, never changed once shipped), its name, its family (the
//  badge shape and colour in the chain graph), its icon, where its code came
//  from (the credits), its knobs, and how to make one.  The registry
//  (TwRegistry.h) holds one for every effect.
//
//  Threads: prepare/reset on the message thread while the block isn't playing;
//  process on the audio thread.  A block never allocates in process.
//==============================================================================
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace tw
{
    /** How many knobs a block has at most (the host sees this many per slot). */
    inline constexpr int maxKnobs = 12;

    /** How many effects a chain holds at most (split and blend not counted). */
    inline constexpr int maxBlocks = 16;

    //==========================================================================
    /** The families: each has its own badge shape and colour in the chain graph. */
    enum class Family
    {
        drive = 0,      // circle
        dynamics,       // square
        filter,         // rhombus
        modulation,     // hexagon
        pitch,          // triangle
        time,           // pentagon
        ambience,       // octagon
        ampcab,         // wide rectangle
        utility,        // small rounded square
        count
    };

    inline const char* familyName (Family f)
    {
        static const char* names[] { "Drive", "Dynamics", "Filter / EQ", "Modulation", "Pitch",
                                     "Time", "Ambience", "Amp / Cab", "Utility" };
        return names[juce::jlimit (0, (int) Family::count - 1, (int) f)];
    }

    //==========================================================================
    /** One knob of a block. */
    struct KnobSpec
    {
        juce::String name;              // on the panel, short ("Gain", "Time")
        float min = 0.0f, max = 1.0f, def = 0.5f;
        juce::String unit;              // shown after the value: " dB", " ms", " %", " Hz"
        float centre = 0.0f;            // the value at the knob's middle (a log-like taper); 0: linear
        juce::StringArray choices;      // a selector: its positions (min 0, max count - 1, whole steps)
        int decimals = -1;              // shown with this many (-1: as the range suggests)

        bool isChoice() const noexcept  { return ! choices.isEmpty(); }

        /** 0..1 (the knob's travel, the host's value) to the plain value. */
        float toPlain (float norm) const noexcept
        {
            norm = juce::jlimit (0.0f, 1.0f, norm);

            if (isChoice())
                return (float) juce::jlimit (0, choices.size() - 1, (int) std::lround (norm * (float) (choices.size() - 1)));

            if (centre > min && centre < max)
            {
                // The skew that puts centre at the knob's middle (JUCE's NormalisableRange way).
                const float skew = std::log (0.5f) / std::log ((centre - min) / (max - min));
                return min + (max - min) * std::exp (std::log (juce::jmax (1.0e-9f, norm)) / skew);
            }

            return min + (max - min) * norm;
        }

        float toNorm (float plain) const noexcept
        {
            if (isChoice())
                return choices.size() > 1 ? juce::jlimit (0.0f, 1.0f, plain / (float) (choices.size() - 1)) : 0.0f;

            const float p = juce::jlimit (0.0f, 1.0f, (plain - min) / juce::jmax (1.0e-9f, max - min));

            if (centre > min && centre < max)
            {
                const float skew = std::log (0.5f) / std::log ((centre - min) / (max - min));
                return std::pow (p, skew);
            }

            return p;
        }

        juce::String text (float plain) const
        {
            if (isChoice())
                return choices[juce::jlimit (0, choices.size() - 1, (int) std::lround (plain))];

            int places = decimals;

            if (places < 0)
            {
                const float span = std::abs (max - min);
                places = span >= 100.0f ? 0 : (span >= 10.0f ? 1 : 2);

                if (unit == " %")
                    places = 0;
            }

            return juce::String (plain, places) + unit;
        }
    };

    //==========================================================================
    /** What the chain tells every block, read on the audio thread. */
    struct Context
    {
        double sampleRate = 48000.0;
        int maxBlock = 512;
        double bpm = 120.0;             // the host's tempo, or the tapped one
        bool playing = false;
    };

    //==========================================================================
    class Block
    {
    public:
        virtual ~Block() = default;

        /** Allocate here: the rate and the largest buffer process() will see. */
        virtual void prepare (double sampleRate, int maxBlock) = 0;

        /** Clear its memory (delay lines, filters, envelopes). */
        virtual void reset() {}

        /** In place: l and r are both always there (a mono guitar comes in on both). */
        virtual void process (float* l, float* r, int n) = 0;

        /** Samples of delay it adds (oversampling, look-ahead): the other path of a split is
            delayed to match, and the plugin reports the chain's total. */
        virtual int latency() const   { return 0; }

        /** A level for the block's panel meter (gain reduction, envelope), 0..1; -1: none. */
        virtual float meter() const   { return -1.0f; }

        /** About 30 times a second on the message thread, with its knobs as they are now in
            uiKnob: for what can't be done on the audio thread (loading an impulse response). */
        virtual void messageTick() {}
        std::array<float, maxKnobs> uiKnob {};

        /** Plain values (as its KnobSpecs give them), set before each buffer. */
        std::array<float, maxKnobs> knob {};

        /** Shared with the whole chain (tempo). Set before prepare. */
        const Context* context = nullptr;

        double bpm() const noexcept   { return context != nullptr ? context->bpm : 120.0; }
    };

    //==========================================================================
    /** An effect as the plugin knows it. */
    struct BlockInfo
    {
        juce::String id;                // kept in presets: never changed once shipped
        juce::String name;              // on the graph's label and the panel's title
        Family family = Family::utility;
        juce::String icon;              // the Guitar FX icon's id ("gfx-overdrive")
        juce::String credit;            // where its code came from, and its license
        juce::String description;       // one line under the panel's title
        std::vector<KnobSpec> knobs;
        std::function<std::unique_ptr<Block>()> make;
        bool trails = false;            // switched off, its tail rings on (delays, reverbs)
        bool hidden = false;            // not offered in the add menu (split, blend)
    };

    //==========================================================================
    //  Small helpers the blocks share.
    //==========================================================================
    inline float dbToGain (float db) noexcept      { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g) noexcept       { return 20.0f * std::log10 (juce::jmax (1.0e-9f, g)); }

    /** A one-pole smoother for a knob: ms to settle (about 63%). */
    struct Smooth
    {
        float value = 0.0f, coeff = 0.0f;

        void prepare (double sr, float ms, float start)
        {
            coeff = (float) std::exp (-1.0 / juce::jmax (1.0, (double) ms * 0.001 * sr));
            value = start;
        }

        float next (float target) noexcept    { value = target + (value - target) * coeff; return value; }
        void snap (float v) noexcept          { value = v; }
    };

    /** A knob spec, short to write. */
    inline KnobSpec knob (const juce::String& name, float min, float max, float def,
                          const juce::String& unit = {}, float centre = 0.0f, int decimals = -1)
    {
        KnobSpec k;
        k.name = name; k.min = min; k.max = max; k.def = def; k.unit = unit; k.centre = centre; k.decimals = decimals;
        return k;
    }

    inline KnobSpec choice (const juce::String& name, const juce::StringArray& positions, int def = 0)
    {
        KnobSpec k;
        k.name = name; k.min = 0.0f; k.max = (float) juce::jmax (0, positions.size() - 1); k.def = (float) def;
        k.choices = positions;
        return k;
    }

    inline KnobSpec percent (const juce::String& name, float def)   { return knob (name, 0.0f, 100.0f, def, " %"); }
    inline KnobSpec toggle (const juce::String& name, bool def)      { return choice (name, { "Off", "On" }, def ? 1 : 0); }
}
