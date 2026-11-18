// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakNodes.h
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <bitset>
#include <memory>
#include <vector>
#include "ZodiakParams.h"
#include "ZodiakFx.h"
#include "ZodiakGrexFx.h"
#include "../SonifySpace.h"
#include "StyleDucker.h"

//==============================================================================
//  ZodiakNodes.h  —  the kinds of node a channel's graph (ZodiakGraph.h) holds,
//  their settings and Zodiak's own effects' sound.
//
//      VSTi            a VSTi loaded from file: plays the channel's notes
//      VST FX          a VST effect loaded from file
//      Vowel           the vowel filter, its eight columns marched by the tempo
//      Filter          OFF and I3Q's filter's 17 types (LP 24 open at 20 kHz in the
//                      default line: the Cutoff macro works at once), and its
//                      envelope: opened by a key's press, closed by the release
//      Trance Gate     16 steps, chopped by the tempo
//      Sync Delay      SYNC on: a note value; off: milliseconds
//      Saturator       Tape / Tube / Fuzz / Clip (anti-aliased waveshaping)
//      Reverb          Sonify's reverb, nine voicings (Greyhole the ninth)
//      Dyn EQ          Grex's dynamic EQ: its two points keyed by its SC input
//      Phaser          Grex's phaser
//      Wah             Grex's auto-wah
//      Feedback        the only way a cable may close a loop: what comes in
//                      goes out one block later, at its level
//      Sidechain In    another channel's sound (its graph's output), as a source
//      Output          the end of the graph: into the mixer strip
//
//  Each built-in node's settings are atomics (the windows write them, the
//  audio thread reads them once a block).  "def" is what a node in the
//  default line starts at (each one at rest, so the line sounds of the VSTi
//  alone); "add" is what a node the user adds by hand starts at (at work).
//==============================================================================

namespace zodiak::graph
{
    enum NodeType : int
    {
        ntInstrument = 0, ntPlugin, ntVowel, ntFilter, ntGate, ntDelay, ntSaturator, ntReverb,
        ntDynEq, ntPhaser, ntWah, ntFeedback, ntKey, ntOutput,
        kNumNodeTypes
    };

    inline const char* typeName (int t) noexcept
    {
        static const char* n[kNumNodeTypes] = { "VSTi", "VST FX", "Vowel", "Filter", "Trance Gate", "Sync Delay", "Saturator",
                                                "Reverb", "Dyn EQ", "Phaser", "Wah", "Feedback", "Sidechain In", "Output" };
        return t >= 0 && t < kNumNodeTypes ? n[t] : "";
    }

    inline const char* typeId (int t) noexcept
    {
        static const char* n[kNumNodeTypes] = { "vsti", "vstfx", "vowel", "filter", "gate", "delay", "saturator",
                                                "reverb", "dyneq", "phaser", "wah", "feedback", "key", "output" };
        return t >= 0 && t < kNumNodeTypes ? n[t] : "";
    }

    inline int typeFromId (const juce::String& id) noexcept
    {
        for (int t = 0; t < kNumNodeTypes; ++t)
            if (id == typeId (t)) return t;
        return -1;
    }

    inline bool isPluginType (int t) noexcept     { return t == ntInstrument || t == ntPlugin; }

    /** The node's inputs: 0 (a source), 1 (main), or 2 (main and SC, the Dyn EQ's key). */
    inline int numInputsOf (int t) noexcept
    {
        if (t == ntInstrument || t == ntKey) return 0;
        return t == ntDynEq ? 2 : 1;
    }

    inline bool hasOutputPin (int t) noexcept     { return t != ntOutput; }

    //==========================================================================
    //  A built-in node's setting
    //==========================================================================
    struct NodeParam
    {
        const char* id;
        const char* name;
        float min, max, def, add;
        const char* unit;
        float skew;
        int   kind;                 // pkContinuous, pkStepped, pkToggle, pkMask
    };

    static constexpr int kMaxNodeParams = 16;
    static constexpr int kNumSatModes = 4;

    inline const char* satModeName (int m) noexcept
    {
        static const char* n[kNumSatModes] = { "Tape", "Tube", "Fuzz", "Clip" };
        return m >= 0 && m < kNumSatModes ? n[m] : "";
    }

    namespace detail
    {
        using P = NodeParam;
        inline constexpr P vowel[] = {
            { "mix",  "Vowel",      0.0f, 1.0f, 0.0f, 1.0f, "", 1.0f, pkContinuous },
            { "res",  "Resonance",  0.0f, 1.0f, 0.5f, 0.5f, "", 1.0f, pkContinuous },
            { "rate", "Rate",       (float) kDiv4Bars, (float) kDiv1_16, (float) kDiv1_4, (float) kDiv1_4, "", 1.0f, pkStepped },
            { "mode", "Mode",       0.0f, 2.0f, 0.0f, 0.0f, "", 1.0f, pkStepped },
            { "c1",   "Column 1",   0.0f, 4.0f, 0.0f, 0.0f, "", 1.0f, pkStepped },
            { "c2",   "Column 2",   0.0f, 4.0f, 1.0f, 1.0f, "", 1.0f, pkStepped },
            { "c3",   "Column 3",   0.0f, 4.0f, 2.0f, 2.0f, "", 1.0f, pkStepped },
            { "c4",   "Column 4",   0.0f, 4.0f, 3.0f, 3.0f, "", 1.0f, pkStepped },
            { "c5",   "Column 5",   0.0f, 4.0f, 4.0f, 4.0f, "", 1.0f, pkStepped },
            { "c6",   "Column 6",   0.0f, 4.0f, 3.0f, 3.0f, "", 1.0f, pkStepped },
            { "c7",   "Column 7",   0.0f, 4.0f, 2.0f, 2.0f, "", 1.0f, pkStepped },
            { "c8",   "Column 8",   0.0f, 4.0f, 1.0f, 1.0f, "", 1.0f, pkStepped } };
        inline constexpr P filter[] = {
            { "type", "Type",       0.0f, (float) (fx::kNumFilterTypes - 1), 2.0f, 2.0f, "", 1.0f, pkStepped },
            { "cut",  "Cutoff",     20.0f, 20000.0f, 20000.0f, 1500.0f, "Hz", 0.25f, pkContinuous },
            { "res",  "Resonance",  0.0f, 1.0f, 0.2f, 0.35f, "", 1.0f, pkContinuous },
            // The envelope (Serum's way): opens the filter on a key's press, closes it
            // on the last key's release.  ENV: how far it opens (octaves, up or down)
            { "env",  "Env",        -8.0f, 8.0f, 0.0f, 0.0f, "oct", 1.0f, pkContinuous },
            { "att",  "Attack",     0.1f, 5000.0f, 5.0f, 5.0f, "ms", 0.3f, pkContinuous },
            { "dec",  "Decay",      1.0f, 5000.0f, 300.0f, 300.0f, "ms", 0.3f, pkContinuous },
            { "sus",  "Sustain",    0.0f, 1.0f, 0.0f, 0.0f, "", 1.0f, pkContinuous },
            { "rel",  "Release",    1.0f, 5000.0f, 250.0f, 250.0f, "ms", 0.3f, pkContinuous },
            // DRIVE: into the screaming types (Scream, Cry, Rave), the band of the SSM
            // ones, a tanh in front of the rest
            { "drive", "Drive",     0.0f, 1.0f, 0.0f, 0.0f, "", 1.0f, pkContinuous } };
        inline constexpr P gate[] = {
            { "depth", "Depth",     0.0f, 1.0f, 0.0f, 1.0f, "", 1.0f, pkContinuous },
            { "rate",  "Rate",      (float) kDiv1Bar, (float) kDiv1_32, (float) kDiv1_16, (float) kDiv1_16, "", 1.0f, pkStepped },
            { "att",   "Attack",    0.5f, 50.0f, 2.0f, 2.0f, "ms", 0.5f, pkContinuous },
            { "rel",   "Release",   1.0f, 300.0f, 30.0f, 30.0f, "ms", 0.5f, pkContinuous },
            { "steps", "Steps",     0.0f, 65535.0f, (float) kDefaultGateSteps, (float) kDefaultGateSteps, "", 1.0f, pkMask } };
        inline constexpr P delay[] = {
            { "sync", "Sync",       0.0f, 1.0f, 1.0f, 1.0f, "", 1.0f, pkToggle },
            { "note", "Time",       0.0f, (float) (kNumDelayNotes - 1), (float) kDelayNote1_8Dotted, (float) kDelayNote1_8Dotted, "", 1.0f, pkStepped },
            { "ms",   "Time",       10.0f, 2000.0f, 375.0f, 375.0f, "ms", 0.4f, pkContinuous },
            { "fb",   "Feedback",   0.0f, 0.95f, 0.35f, 0.35f, "", 1.0f, pkContinuous },
            { "tone", "Tone",       500.0f, 18000.0f, 6000.0f, 6000.0f, "Hz", 0.3f, pkContinuous },
            { "mix",  "Mix",        0.0f, 1.0f, 0.0f, 0.35f, "", 1.0f, pkContinuous } };
        inline constexpr P saturator[] = {
            { "mode",  "Mode",      0.0f, (float) (kNumSatModes - 1), 0.0f, 0.0f, "", 1.0f, pkStepped },
            { "drive", "Drive",     0.0f, 1.0f, 0.0f, 0.4f, "", 1.0f, pkContinuous },
            { "tone",  "Tone",      -1.0f, 1.0f, 0.0f, 0.0f, "", 1.0f, pkContinuous },
            { "mix",   "Mix",       0.0f, 1.0f, 1.0f, 1.0f, "", 1.0f, pkContinuous },
            { "out",   "Output",    -24.0f, 12.0f, 0.0f, 0.0f, "dB", 1.0f, pkContinuous } };
        inline constexpr P reverb[] = {
            { "voicing", "Voicing", 0.0f, (float) (kNumReverbVoicings - 1), 1.0f, 1.0f, "", 1.0f, pkStepped },
            { "size",    "Size",    0.0f, 1.0f, 0.6f, 0.6f, "", 1.0f, pkContinuous },
            { "decay",   "Decay",   0.0f, 1.0f, 0.5f, 0.5f, "", 1.0f, pkContinuous },
            { "damp",    "Damp",    0.0f, 1.0f, 0.5f, 0.5f, "", 1.0f, pkContinuous },
            { "pre",     "Pre-delay", 0.0f, 250.0f, 20.0f, 20.0f, "ms", 0.5f, pkContinuous },
            { "mix",     "Mix",     0.0f, 1.0f, 0.0f, 0.3f, "", 1.0f, pkContinuous } };
        // The Dyn EQ's two points as the groove box ships them: the low end a kick
        // takes from a bass, the body a snare or a lead takes from a pad - each at
        // a working depth, so a key wired into SC is heard at once (nothing on SC:
        // it rests)
        inline constexpr P dyneq[] = {
            { "b1_on",  "Point 1 On",     0.0f, 1.0f, 1.0f, 1.0f, "", 1.0f, pkToggle },
            { "b1_freq","Point 1 Freq",   40.0f, 16000.0f, 100.0f, 100.0f, "Hz", 0.3f, pkContinuous },
            { "b1_q",   "Point 1 Q",      0.3f, 8.0f, 0.8f, 0.8f, "", 0.5f, pkContinuous },
            { "b1_depth","Point 1 Depth", 0.0f, 24.0f, 12.0f, 12.0f, "dB", 1.0f, pkContinuous },
            { "b1_thr", "Point 1 Thresh", -60.0f, 0.0f, -32.0f, -32.0f, "dB", 1.0f, pkContinuous },
            { "b1_ratio","Point 1 Ratio", 1.0f, 20.0f, 6.0f, 6.0f, ":1", 0.5f, pkContinuous },
            { "b1_att", "Point 1 Attack", 0.5f, 200.0f, 2.0f, 2.0f, "ms", 0.4f, pkContinuous },
            { "b1_rel", "Point 1 Release",20.0f, 1000.0f, 160.0f, 160.0f, "ms", 0.4f, pkContinuous },
            { "b2_on",  "Point 2 On",     0.0f, 1.0f, 1.0f, 1.0f, "", 1.0f, pkToggle },
            { "b2_freq","Point 2 Freq",   40.0f, 16000.0f, 1500.0f, 1500.0f, "Hz", 0.3f, pkContinuous },
            { "b2_q",   "Point 2 Q",      0.3f, 8.0f, 0.9f, 0.9f, "", 0.5f, pkContinuous },
            { "b2_depth","Point 2 Depth", 0.0f, 24.0f, 6.0f, 6.0f, "dB", 1.0f, pkContinuous },
            { "b2_thr", "Point 2 Thresh", -60.0f, 0.0f, -32.0f, -32.0f, "dB", 1.0f, pkContinuous },
            { "b2_ratio","Point 2 Ratio", 1.0f, 20.0f, 4.0f, 4.0f, ":1", 0.5f, pkContinuous },
            { "b2_att", "Point 2 Attack", 0.5f, 200.0f, 4.0f, 4.0f, "ms", 0.4f, pkContinuous },
            { "b2_rel", "Point 2 Release",20.0f, 1000.0f, 140.0f, 140.0f, "ms", 0.4f, pkContinuous } };
        inline constexpr P phaser[] = {
            { "rate",  "Rate",      0.05f, 4.0f, 1.6f, 1.6f, "Hz", 0.5f, pkContinuous },
            { "depth", "Depth",     0.0f, 1.0f, 0.8f, 0.8f, "", 1.0f, pkContinuous },
            { "fb",    "Feedback",  0.0f, 0.9f, 0.5f, 0.5f, "", 1.0f, pkContinuous },
            { "mix",   "Mix",       0.0f, 1.0f, 0.5f, 0.5f, "", 1.0f, pkContinuous } };
        inline constexpr P wah[] = {
            { "sens",  "Sensitivity", 0.0f, 1.0f, 0.5f, 0.5f, "", 1.0f, pkContinuous },
            { "rate",  "Rate",      0.05f, 8.0f, 1.0f, 1.0f, "Hz", 0.4f, pkContinuous },
            { "lfo",   "LFO Depth", 0.0f, 1.0f, 0.0f, 0.0f, "", 1.0f, pkContinuous },
            { "freq",  "Frequency", 100.0f, 1500.0f, 400.0f, 400.0f, "Hz", 0.5f, pkContinuous },
            { "q",     "Q",         0.0f, 1.0f, 0.6f, 0.6f, "", 1.0f, pkContinuous },
            { "mix",   "Mix",       0.0f, 1.0f, 1.0f, 1.0f, "", 1.0f, pkContinuous } };
        inline constexpr P feedback[] = {
            { "level",  "Level",    0.0f, 1.0f, 0.5f, 0.5f, "", 1.0f, pkContinuous },
            { "invert", "Invert",   0.0f, 1.0f, 0.0f, 0.0f, "", 1.0f, pkToggle } };
        inline constexpr P key[] = {
            { "source", "Channel",  0.0f, (float) (kNumChannels - 1), 0.0f, 0.0f, "", 1.0f, pkStepped },
            { "gain",   "Gain",     -24.0f, 24.0f, 0.0f, 0.0f, "dB", 1.0f, pkContinuous } };

        template <size_t N> constexpr int count (const P (&)[N]) noexcept   { return (int) N; }
    }

    /** A built-in node's settings (none for the VSTi, VST FX and Output). */
    inline const NodeParam* paramsOf (int type, int& count) noexcept
    {
        using namespace detail;
        switch (type)
        {
            case ntVowel:     count = detail::count (vowel);     return vowel;
            case ntFilter:    count = detail::count (filter);    return filter;
            case ntGate:      count = detail::count (gate);      return gate;
            case ntDelay:     count = detail::count (delay);     return delay;
            case ntSaturator: count = detail::count (saturator); return saturator;
            case ntReverb:    count = detail::count (reverb);    return reverb;
            case ntDynEq:     count = detail::count (dyneq);     return dyneq;
            case ntPhaser:    count = detail::count (phaser);    return phaser;
            case ntWah:       count = detail::count (wah);       return wah;
            case ntFeedback:  count = detail::count (feedback);  return feedback;
            case ntKey:       count = detail::count (key);       return key;
            default:          count = 0;                         return nullptr;
        }
    }

    inline int numBuiltInParams (int type) noexcept   { int n = 0; paramsOf (type, n); return n; }

    inline const NodeParam* builtInParam (int type, int p) noexcept
    {
        int n = 0;
        const auto* ps = paramsOf (type, n);
        return ps != nullptr && p >= 0 && p < n ? &ps[p] : nullptr;
    }

    inline int builtInParamIndex (int type, const juce::String& id) noexcept
    {
        int n = 0;
        const auto* ps = paramsOf (type, n);
        for (int p = 0; p < n; ++p)
            if (id == ps[p].id) return p;
        return -1;
    }

    /** A built-in setting's value as a person reads it. */
    inline juce::String builtInText (int type, int p, float v)
    {
        const auto* info = builtInParam (type, p);
        if (info == nullptr) return {};
        const juce::String id (info->id);
        const int i = (int) std::lround (v);

        if (info->kind == pkToggle) return v >= 0.5f ? "On" : "Off";
        switch (type)
        {
            case ntVowel:
                if (id == "rate") return divisionName (i);
                if (id == "mode") return vowelModeName (i);
                if (id.startsWith ("c")) return vowelName (i);
                break;
            case ntFilter:    if (id == "type") return filterTypeName (i); break;
            case ntGate:      if (id == "rate") return divisionName (i); break;
            case ntDelay:     if (id == "note") return delayNote (v).name; break;
            case ntSaturator: if (id == "mode") return satModeName (i); break;
            case ntReverb:    if (id == "voicing") return reverbVoicingName (i); break;
            case ntKey:       if (id == "source") return "Ch " + juce::String (i + 1); break;
            default: break;
        }

        const juce::String unit (info->unit);
        if (unit == "Hz")  return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String ((int) std::lround (v)) + " Hz";
        if (unit == "ms")  return (v < 10.0f ? juce::String (v, 1) : juce::String ((int) std::lround (v))) + " ms";
        if (unit == "dB")  return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
        if (unit == ":1")  return juce::String (v, 1) + ":1";
        if (unit == "oct") return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " oct";
        if (info->max <= 1.0f && info->min >= -1.0f) return juce::String ((int) std::lround (v * 100.0f)) + "%";
        return juce::String (v, 2);
    }

    /** What a built-in node is doing, short, for its box on the canvas. */
    inline juce::String builtInSummary (int type, const std::atomic<float>* v)
    {
        auto t = [&] (int p) { return builtInText (type, p, v[p].load (std::memory_order_relaxed)); };
        switch (type)
        {
            case ntVowel:     return t (0) + "  " + t (2);
            case ntFilter:
                if ((int) std::lround (v[0].load()) == 0) return "OFF";
                return t (0) + "  " + t (1) + (std::abs (v[3].load()) > 0.01f ? "  env " + t (3) : juce::String());
            case ntGate:      return t (0) + "  " + t (1);
            case ntDelay:     return (v[0].load() >= 0.5f ? t (1) : t (2)) + "  " + t (5);
            case ntSaturator: return t (0) + "  " + t (1);
            case ntReverb:    return t (0) + "  " + t (5);
            case ntPhaser:    return t (0) + "  " + t (3);
            case ntWah:       return t (3) + "  " + t (5);
            case ntFeedback:  return t (0) + (v[1].load() >= 0.5f ? "  inverted" : "");
            case ntKey:       return t (0) + "  " + t (1);
            default:          return {};
        }
    }

    //==========================================================================
    //  A built-in node's sound.  `v` holds its settings, read once a block.
    //  Main in and out are L/R in place; sc is the SC input (the Dyn EQ's key).
    //==========================================================================
    class NodeDsp
    {
    public:
        virtual ~NodeDsp() = default;
        virtual void prepare (double sampleRate, int maxBlock) = 0;
        virtual void reset() noexcept = 0;
        virtual void process (float* L, float* R, int n, const float* scL, const float* scR,
                              const Transport& t, const float* v) noexcept = 0;
        virtual void noteOn() noexcept {}

        /** For the window: the vowel's marker (0..7), the gate's step (0..15), the
            octaves the filter's envelope has its cutoff moved by now. */
        std::atomic<float> shown { 0.0f };

        /** The channel's notes for this block (set by the graph before process). */
        const juce::MidiBuffer* notes = nullptr;
    };

    namespace dsp
    {
        //======================================================================
        class Vowel final : public NodeDsp
        {
        public:
            void prepare (double sr, int) override       { fx.prepare (sr); }
            void reset() noexcept override               { fx.reset(); }
            void noteOn() noexcept override              { fx.noteOn(); }
            void process (float* L, float* R, int n, const float*, const float*, const Transport& t, const float* v) noexcept override
            {
                std::array<int, kVowelColumns> cols {};
                for (int i = 0; i < kVowelColumns; ++i) cols[(size_t) i] = juce::jlimit (0, kNumVowels - 1, (int) std::lround (v[4 + i]));
                fx.process (L, R, n, t, v[0], v[1], v[2], (int) std::lround (v[3]), cols);
                shown.store (fx.position(), std::memory_order_relaxed);
            }
        private:
            fx::VowelFilter fx;
        };

        //======================================================================
        class Filter final : public NodeDsp
        {
        public:
            void prepare (double sr, int maxBlock) override
            {
                rate = (float) (sr > 0.0 ? sr : 48000.0);
                fx.prepare (sr);
                mod.assign ((size_t) juce::jmax (maxBlock, 4096), 0.0f);
                reset();
            }

            void reset() noexcept override
            {
                fx.reset();
                level = 0.0f;
                stage = idle;
                held.reset();
            }

            void process (float* L, float* R, int n, const float*, const float*, const Transport&, const float* v) noexcept override
            {
                n = juce::jmin (n, (int) mod.size());
                const float env = juce::jlimit (-8.0f, 8.0f, v[3]);
                const float attInc = 1.0f / juce::jmax (1.0f, v[4] * 0.001f * rate);
                const float decK = timeCoef (v[5]);
                const float sus = juce::jlimit (0.0f, 1.0f, v[6]);
                const float relK = timeCoef (v[7]);

                // The envelope, sample by sample, the notes where they fall
                auto it = notes != nullptr ? notes->begin() : juce::MidiBufferIterator {};
                const auto end = notes != nullptr ? notes->end() : juce::MidiBufferIterator {};
                for (int i = 0; i < n; ++i)
                {
                    while (notes != nullptr && it != end && (*it).samplePosition <= i)
                    {
                        const auto m = (*it).getMessage();
                        if (m.isNoteOn())                         { held.set ((size_t) m.getNoteNumber()); stage = attack; }
                        else if (m.isNoteOff())                   { held.reset ((size_t) m.getNoteNumber()); if (held.none() && stage != idle) stage = release; }
                        else if (m.isAllNotesOff() || m.isAllSoundOff()) { held.reset(); if (stage != idle) stage = release; }
                        ++it;
                    }
                    switch (stage)
                    {
                        case attack:  level += attInc; if (level >= 1.0f) { level = 1.0f; stage = decay; } break;
                        case decay:   level = sus + (level - sus) * decK; break;
                        case release: level *= relK; if (level < 1.0e-4f) { level = 0.0f; stage = idle; } break;
                        case idle:    break;
                    }
                    mod[(size_t) i] = env * level;
                }

                const bool moving = std::abs (env) > 1.0e-3f;
                fx.process (L, R, n, (int) std::lround (v[0]), v[1], v[2], moving ? mod.data() : nullptr, v[8]);
                shown.store (moving ? env * level : 0.0f, std::memory_order_relaxed);
            }

        private:
            enum Stage { idle, attack, decay, release };

            // To 0.1% of the way in `ms`
            float timeCoef (float ms) const noexcept   { return std::exp (-6.9f / juce::jmax (1.0f, ms * 0.001f * rate)); }

            fx::ChannelFilter fx;
            std::vector<float> mod;
            std::bitset<128> held;
            float rate = 48000.0f, level = 0.0f;
            Stage stage = idle;
        };

        //======================================================================
        class Gate final : public NodeDsp
        {
        public:
            void prepare (double sr, int) override       { fx.prepare (sr); }
            void reset() noexcept override               { fx.reset(); }
            void process (float* L, float* R, int n, const float*, const float*, const Transport& t, const float* v) noexcept override
            {
                fx.process (L, R, n, t, v[0], v[1], v[2], v[3], (int) v[4]);
                shown.store ((float) fx.currentStep(), std::memory_order_relaxed);
            }
        private:
            fx::TranceGate fx;
        };

        //======================================================================
        class Delay final : public NodeDsp
        {
        public:
            void prepare (double sr, int) override       { fx.prepare (sr); }
            void reset() noexcept override               { fx.reset(); }
            void process (float* L, float* R, int n, const float*, const float*, const Transport& t, const float* v) noexcept override
            {
                fx.process (L, R, n, t, v[0] > 0.5f, delayNote (v[1]).beats, v[2], v[3], v[4], v[5]);
            }
        private:
            fx::SyncDelay fx;
        };

        //======================================================================
        //  SATURATOR.  Each mode is a curve f(x) shaped through first-order
        //  antiderivative anti-aliasing (the difference of F, its integral,
        //  over the step: no oversampling, no latency).  DRIVE pushes the
        //  input up to +30 dB and the output comes back down by about as much,
        //  so the drive changes the colour more than the level; TONE tilts
        //  the driven sound dark or bright; MIX blends it with the dry.
        //  DRIVE 0 passes the sound untouched.
        //======================================================================
        class Saturator final : public NodeDsp
        {
        public:
            void prepare (double sr, int) override
            {
                rate = (float) (sr > 0.0 ? sr : 48000.0);
                reset();
            }

            void reset() noexcept override
            {
                for (int c = 0; c < 2; ++c) { xPrev[c] = 0.0f; fPrev[c] = 0.0f; lp[c] = 0.0f; }
                driveNow = -1.0f;
            }

            void process (float* L, float* R, int n, const float*, const float*, const Transport&, const float* v) noexcept override
            {
                const int mode = juce::jlimit (0, kNumSatModes - 1, (int) std::lround (v[0]));
                const float drive = juce::jlimit (0.0f, 1.0f, v[1]);
                const float tone  = juce::jlimit (-1.0f, 1.0f, v[2]);
                // DRIVE 0 is clean: the shaped sound comes in over the first tenth of it
                const float mix   = juce::jlimit (0.0f, 1.0f, v[3]) * juce::jlimit (0.0f, 1.0f, drive * 10.0f);
                const float outG  = std::pow (10.0f, v[4] * 0.05f);

                const float inT  = std::pow (10.0f, drive * 1.5f);          // up to +30 dB
                const float makeT = 1.0f / std::pow (inT, 0.85f);            // back down, most of the way
                if (driveNow < 0.0f) { driveNow = inT; makeNow = makeT; }
                const float dIn = (inT - driveNow) / (float) juce::jmax (1, n);
                const float dMk = (makeT - makeNow) / (float) juce::jmax (1, n);

                // Tone: a one-pole split; dark keeps the low, bright adds the high
                const float cut = 3000.0f;
                const float a = 1.0f - std::exp (-2.0f * fx::kPi * cut / rate);
                const float lowW = 1.0f, highW = 1.0f + tone;      // -1: the highs gone .. +1: doubled

                float* ch[2] = { L, R };
                for (int i = 0; i < n; ++i)
                {
                    driveNow += dIn;
                    makeNow  += dMk;
                    for (int c = 0; c < 2; ++c)
                    {
                        const float dry = ch[c][i];
                        const float x = dry * driveNow;
                        float y = shapeAa (mode, x, c) * makeNow;
                        lp[c] += a * (y - lp[c]);
                        y = lp[c] * lowW + (y - lp[c]) * juce::jmax (0.0f, highW);
                        ch[c][i] = (dry + mix * (y - dry)) * outG;
                    }
                }
                driveNow = inT;
                makeNow = makeT;
            }

        private:
            // The curves and their antiderivatives
            static float f (int mode, float x) noexcept
            {
                switch (mode)
                {
                    case 0:  return std::tanh (x);                                                     // tape
                    case 1:  return x >= 0.0f ? std::tanh (x) : std::tanh (0.6f * x) / 0.6f * 0.75f;   // tube (asymmetric)
                    case 2:  return std::tanh (2.0f * x) * 0.85f;                                     // fuzz
                    default: return juce::jlimit (-1.0f, 1.0f, x);                                     // clip
                }
            }

            static float logCosh (float x) noexcept
            {
                const float ax = std::abs (x);
                return ax > 12.0f ? ax - 0.69314718f : std::log (std::cosh (x));
            }

            static float F (int mode, float x) noexcept
            {
                switch (mode)
                {
                    case 0:  return logCosh (x);
                    case 1:  return x >= 0.0f ? logCosh (x) : logCosh (0.6f * x) / 0.36f * 0.75f;
                    case 2:  return logCosh (2.0f * x) * 0.5f * 0.85f;
                    default: return std::abs (x) <= 1.0f ? 0.5f * x * x : std::abs (x) - 0.5f;
                }
            }

            float shapeAa (int mode, float x, int c) noexcept
            {
                const float x0 = xPrev[c];
                const float Fx = F (mode, x);
                float y;
                if (std::abs (x - x0) < 1.0e-4f) y = f (mode, 0.5f * (x + x0));
                else                             y = (Fx - fPrev[c]) / (x - x0);
                xPrev[c] = x;
                fPrev[c] = Fx;
                return y;
            }

            float rate = 48000.0f;
            float xPrev[2] {}, fPrev[2] {}, lp[2] {};
            float driveNow = -1.0f, makeNow = 1.0f;
        };

        //======================================================================
        //  REVERB: Sonify's, as the master reverb was; here an insert, MIX
        //  between the dry and the wet.  At mix 0 it rests (it costs nothing),
        //  and starts from silence when it is turned up again.
        //======================================================================
        class Reverb final : public NodeDsp
        {
        public:
            void prepare (double sr, int) override       { fx.prepare (sr); last.fill (-1.0f); resting = true; mixNow = 0.0f; }
            void reset() noexcept override               { fx.reset(); mixNow = 0.0f; }

            void process (float* L, float* R, int n, const float*, const float*, const Transport&, const float* v) noexcept override
            {
                const std::array<float, 5> s5 { v[0], v[1], v[2], v[3], v[4] };
                if (s5 != last)
                {
                    SonifySpace::ReverbSettings s;
                    s.voicing    = (SonifySpace::Voicing) juce::jlimit (0, kNumReverbVoicings - 1, (int) std::lround (v[0]));
                    s.size       = v[1];
                    s.decay      = v[2];
                    s.damping    = v[3];
                    s.preDelayMs = v[4];
                    s.early      = s.voicing == SonifySpace::Voicing::greyhole ? 0.0f : 0.25f;
                    fx.set (s, last[0] < 0.0f);
                    last = s5;
                }

                const float mixT = juce::jlimit (0.0f, 1.0f, v[5]);
                if (mixT <= 0.0f && mixNow <= 1.0e-5f)
                {
                    if (! resting) { fx.reset(); resting = true; }
                    mixNow = 0.0f;
                    return;
                }
                resting = false;

                const float dm = (mixT - mixNow) / (float) juce::jmax (1, n);
                for (int i = 0; i < n; ++i)
                {
                    float wl, wr;
                    fx.tick (L[i], R[i], wl, wr);
                    mixNow += dm;
                    L[i] += mixNow * (wl - L[i]);
                    R[i] += mixNow * (wr - R[i]);
                }
                mixNow = mixT;
            }

        private:
            SonifySpace::Reverb fx;
            std::array<float, 5> last { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
            float mixNow = 0.0f;
            bool resting = true;
        };

        //======================================================================
        //  DYN EQ: Grex's dynamic EQ, keyed by its SC input (nothing wired
        //  there: it keys from silence and rests).  Its settings live in the
        //  ducker itself (the curve's window drags them); the node's values
        //  are a view of them (Node::get / set).
        //======================================================================
        class DynEq final : public NodeDsp
        {
        public:
            DynEq()                                      { ducker.setEnabled (true); }
            void prepare (double sr, int) override       { ducker.prepare (sr); }
            void reset() noexcept override               { ducker.reset(); }

            void process (float* L, float* R, int n, const float* scL, const float* scR, const Transport&, const float*) noexcept override
            {
                float* main[2] = { L, R };
                float* side[2] = { const_cast<float*> (scL), const_cast<float*> (scR) };
                juce::AudioBuffer<float> style (main, 2, n);
                const juce::AudioBuffer<float> key (side, 2, n);
                ducker.process (style, key);
            }

            ZBetel::StyleDucker ducker;
        };

        //======================================================================
        class Phaser final : public NodeDsp
        {
        public:
            void prepare (double sr, int) override       { rate = sr; fx.prepare (sr); }
            void reset() noexcept override               { fx.reset(); }
            void process (float* L, float* R, int n, const float*, const float*, const Transport&, const float* v) noexcept override
            {
                const float rate01 = (juce::jlimit (0.05f, 4.0f, v[0]) - 0.05f) / 3.95f;
                fx.process (L, R, n, rate01, v[1], v[2], v[3], rate);
            }
        private:
            ZodiakGrexFx::PhaserFx fx;
            double rate = 48000.0;
        };

        //======================================================================
        class Wah final : public NodeDsp
        {
        public:
            void prepare (double sr, int) override       { rate = sr; fx.prepare (sr); }
            void reset() noexcept override               { fx.reset(); }
            void process (float* L, float* R, int n, const float*, const float*, const Transport&, const float* v) noexcept override
            {
                fx.process (L, R, n, v[0], v[1], v[2], v[3], v[4], v[5], rate);
            }
        private:
            ZodiakGrexFx::AutoWahFx fx;
            double rate = 48000.0;
        };

        inline std::unique_ptr<NodeDsp> make (int type)
        {
            switch (type)
            {
                case ntVowel:     return std::make_unique<Vowel>();
                case ntFilter:    return std::make_unique<Filter>();
                case ntGate:      return std::make_unique<Gate>();
                case ntDelay:     return std::make_unique<Delay>();
                case ntSaturator: return std::make_unique<Saturator>();
                case ntReverb:    return std::make_unique<Reverb>();
                case ntDynEq:     return std::make_unique<DynEq>();
                case ntPhaser:    return std::make_unique<Phaser>();
                case ntWah:       return std::make_unique<Wah>();
                default:          return nullptr;
            }
        }
    }
}
