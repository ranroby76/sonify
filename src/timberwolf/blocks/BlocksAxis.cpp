// D:\workspace\Fanan_web_hub\src\timberwolf\blocks\BlocksAxis.cpp
//==============================================================================
//  AXIS's own effects, as Timber Wolf blocks: its gate, compressor, drive, the
//  five amps, the cabinets (modelled, or an impulse response from the AXIS
//  pack beside Sonify) and its reverb.  The DSP is Timber Wolf's copy of AXIS's
//  (timberwolf\axdsp), so AXIS itself never changes because of Timber Wolf.
//==============================================================================
#include "BlockKit.h"
#include "../TwRegistry.h"

#include "../axdsp/Gate.h"
#include "../axdsp/Comp.h"
#include "../axdsp/Drive.h"
#include "../axdsp/AmpModel.h"
#include "../axdsp/Space.h"
#include "../../AxisPack.h"

namespace tw
{
    namespace
    {
        const juce::String axisCredit = "Fanan AXIS (Sonify's own)";

        juce::dsp::ProcessSpec specOf (double sr, int maxBlock, int channels = 2)
        {
            return { sr, (juce::uint32) juce::jmax (1, maxBlock), (juce::uint32) channels };
        }

        /** A stereo buffer over l and r, for the AXIS DSP (which takes juce::AudioBuffer). */
        struct View
        {
            float* ch[2];
            juce::AudioBuffer<float> buffer;
            View (float* l, float* r, int n) : ch { l, r }, buffer (ch, 2, n) {}
        };

        //======================================================================
        class GateBlock final : public Block
        {
        public:
            void prepare (double sr, int maxBlock) override   { gate.prepare (specOf (sr, maxBlock)); }
            void reset() override                             { gate.reset(); }

            void process (float* l, float* r, int n) override
            {
                axdsp::Gate::Params p;
                p.mode = (axdsp::Gate::Mode) (int) knob[0];
                p.threshDb = knob[1];
                p.depth = knob[2];
                p.releaseMs = knob[3];
                p.holdMs = knob[4];
                p.scHpfHz = knob[5];
                p.hissFilter = knob[6] > 0.5f;
                gate.setParams (p);

                View v (l, r, n);
                gate.process (v.buffer);
            }

            float meter() const override    { return 1.0f - gate.currentGain(); }

        private:
            axdsp::Gate gate;
        };

        //======================================================================
        class CompBlock final : public Block
        {
        public:
            void prepare (double sr, int maxBlock) override   { comp.prepare (specOf (sr, maxBlock)); }
            void reset() override                             { comp.reset(); }

            void process (float* l, float* r, int n) override
            {
                axdsp::Comp::Params p;
                p.circuit = (axdsp::Comp::Circuit) (int) knob[0];
                p.threshDb = knob[1];
                p.ratio = knob[2];
                p.attackMs = knob[3];
                p.releaseMs = knob[4];
                p.makeupDb = knob[5];
                p.autoGain = knob[6] > 0.5f;
                p.scHpf = knob[7] > 0.5f;
                p.mix = knob[8];
                comp.setParams (p);

                View v (l, r, n);
                comp.process (v.buffer);
            }

            float meter() const override    { return juce::jlimit (0.0f, 1.0f, -comp.gainReductionDb() / 24.0f); }

        private:
            axdsp::Comp comp;
        };

        //======================================================================
        class DriveBlock final : public Block
        {
        public:
            void prepare (double sr, int maxBlock) override   { drive.prepare (specOf (sr, maxBlock)); for (auto& d : dc) d.prepare (sr); }
            void reset() override                             { drive.reset(); for (auto& d : dc) d.reset(); }

            void process (float* l, float* r, int n) override
            {
                axdsp::Drive::Params p;
                p.voice = (axdsp::Drive::Voice) (int) knob[0];
                p.gain = knob[1];
                p.tone = knob[2];
                p.level = knob[3];
                p.body = knob[4];
                p.bite = knob[5];
                p.dynamic = knob[6] > 0.5f;
                p.mix = knob[7];
                drive.setParams (p);

                View v (l, r, n);
                drive.process (v.buffer);

                // Its clipping is lopsided (that's its warmth): what that leaves off the middle goes.
                for (int i = 0; i < n; ++i) { l[i] = dc[0].run (l[i]); r[i] = dc[1].run (r[i]); }
            }

        private:
            axdsp::Drive drive;
            kit::DcBlock dc[2];
        };

        //======================================================================
        class AmpBlock final : public Block
        {
        public:
            void prepare (double sr, int maxBlock) override   { amp.prepare (specOf (sr, maxBlock)); }
            void reset() override                             { amp.reset(); }

            void process (float* l, float* r, int n) override
            {
                axdsp::AmpModel::Params p;
                p.voice = (int) knob[0];
                p.gain = knob[1];
                p.bass = knob[2];
                p.mid = knob[3];
                p.treble = knob[4];
                p.presence = knob[5];
                p.master = knob[6];
                p.bright = knob[7] > 0.5f;
                p.tight = knob[8] > 0.5f;
                p.cab = false;   // the cabinet is a block of its own
                amp.setParams (p);

                View v (l, r, n);
                amp.process (v.buffer, true);
            }

            int latency() const override    { return amp.latencySamples(); }

        private:
            axdsp::AmpModel amp;
        };

        //======================================================================
        //  The cabinets: AXIS's four modelled ones, or an impulse response from the AXIS pack.
        //======================================================================
        struct Pack
        {
            std::unique_ptr<AxisPack::Pack> pack;
            juce::StringArray cabNames;

            static juce::File file()
            {
                auto beside = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();

               #if JUCE_MAC
                beside = beside.getSiblingFile ("Resources");
               #endif

                if (const auto shipped = beside.getChildFile ("AXIS.blob"); shipped.existsAsFile())
                    return shipped;

                return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("Fanan").getChildFile ("Sonify").getChildFile ("AXIS").getChildFile ("AXIS.blob");
            }

            /** Opened once, on the message thread (the registry asks first). */
            static Pack& get()
            {
                static Pack p = []
                {
                    Pack made;
                    const auto f = file();

                    if (f.existsAsFile())
                    {
                        auto opened = std::make_unique<AxisPack::Pack>();
                        juce::String error;

                        if (opened->open (f, error))
                        {
                            for (const auto& entry : opened->getEntries())
                                if (entry.kind == AxisPack::Entry::Kind::cab)
                                    made.cabNames.add (entry.name);

                            made.pack = std::move (opened);
                        }
                    }

                    return made;
                }();

                return p;
            }
        };

        class CabBlock final : public Block
        {
        public:
            void prepare (double sr, int maxBlock) override
            {
                for (auto& c : cab)
                    c.prepare (sr, maxBlock);

                rate = sr;
                setFilters();
            }

            void reset() override
            {
                for (auto& c : cab) c.reset();
                for (auto& f : lowCut) f.reset();
                for (auto& f : highCut) f.reset();
            }

            void process (float* l, float* r, int n) override
            {
                const int type = (int) knob[0];
                const int cabType = type < axdsp::cab::numSpeakers ? type : axdsp::Cabinet::irType;

                for (auto& c : cab)
                {
                    c.setType (cabType);
                    c.setMic (knob[2], knob[3]);
                }

                if (knob[4] != lowHz || knob[5] != highHz)
                {
                    lowHz = knob[4];
                    highHz = knob[5];
                    setFilters();
                }

                // A mono guitar: one cabinet, on both sides.
                const bool mono = kit::isMono (l, r, n);
                cab[0].process (l, n, true);

                if (mono)
                    juce::FloatVectorOperations::copy (r, l, n);
                else
                    cab[1].process (r, n, true);

                const float g = dbToGain (knob[6]);

                for (int i = 0; i < n; ++i)
                {
                    l[i] = highCut[0].run (lowCut[0].run (l[i])) * g;
                    r[i] = mono ? l[i] : highCut[1].run (lowCut[1].run (r[i])) * g;
                }
            }

            /** The impulse response chosen: loaded here, on the message thread. */
            void messageTick() override
            {
                const bool wantsIr = (int) uiKnob[0] >= axdsp::cab::numSpeakers;
                const int index = (int) uiKnob[1];

                if (! wantsIr || index == loadedIndex)
                    return;

                loadedIndex = index;
                auto& p = Pack::get();

                if (p.pack == nullptr)
                    return;

                AxisPack::Ir ir;
                juce::String error;

                if (! p.pack->loadIr (index, ir, error) || ! ir.isValid())
                    return;

                for (auto& c : cab)
                {
                    juce::AudioBuffer<float> buffer (1, ir.frames());
                    buffer.copyFrom (0, 0, ir.left.data(), ir.frames());
                    c.loadIr (std::move (buffer), ir.sampleRate, ir.displayName());
                }
            }

        private:
            void setFilters()
            {
                for (auto& f : lowCut)  f.set (kit::Biquad::Type::highpass, rate, lowHz, 0.707);
                for (auto& f : highCut) f.set (kit::Biquad::Type::lowpass, rate, highHz, 0.707);
            }

            axdsp::Cabinet cab[2];
            kit::Biquad lowCut[2], highCut[2];
            float lowHz = 60.0f, highHz = 9000.0f;
            double rate = 48000.0;
            int loadedIndex = -1;
        };

        //======================================================================
        class SpaceBlock final : public Block
        {
        public:
            void prepare (double sr, int maxBlock) override   { space.prepare (specOf (sr, maxBlock)); }
            void reset() override                             { space.reset(); }

            void process (float* l, float* r, int n) override
            {
                axdsp::Space::Params p;
                p.type = (axdsp::Space::Type) (int) knob[0];
                p.preDlyMs = knob[1];
                p.decay = knob[2];
                p.size = knob[3];
                p.damp = knob[4];
                p.width = knob[5];
                p.duck = knob[6] > 0.5f;
                p.freeze = knob[7] > 0.5f;
                p.mix = knob[8];
                space.setParams (p);

                View v (l, r, n);
                space.process (v.buffer);
            }

        private:
            axdsp::Space space;
        };
    }

    //==========================================================================
    void registry::addAxis (std::vector<BlockInfo>& list)
    {
        using namespace kit;

        list.push_back (info ("ax_gate", "AXIS Gate", Family::dynamics, "gfx-noise-gate", axisCredit,
            "AXIS's noise gate: closes under the threshold, learns the noise floor in Adapt.",
            { choice ("Mode", { "Fast", "Smooth", "Adapt" }, 2), knob ("Threshold", -80.0f, 0.0f, -46.0f, " dB", 0.0f, 1),
              percent ("Depth", 88.0f), knob ("Release", 10.0f, 1000.0f, 160.0f, " ms", 160.0f), knob ("Hold", 0.0f, 500.0f, 40.0f, " ms", 60.0f),
              knob ("SC filter", 20.0f, 1000.0f, 120.0f, " Hz", 150.0f), toggle ("Hiss filter", true) },
            maker<GateBlock>()));

        list.push_back (info ("ax_comp", "AXIS Comp", Family::dynamics, "gfx-compressor", axisCredit,
            "AXIS's compressor: opto, FET or VCA, with auto gain and a parallel mix.",
            { choice ("Circuit", { "Opto", "FET", "VCA" }), knob ("Threshold", -60.0f, 0.0f, -18.0f, " dB", 0.0f, 1),
              knob ("Ratio", 1.0f, 20.0f, 4.0f, ":1", 4.0f, 1), knob ("Attack", 0.1f, 200.0f, 14.0f, " ms", 15.0f),
              knob ("Release", 10.0f, 2000.0f, 280.0f, " ms", 250.0f), knob ("Makeup", 0.0f, 24.0f, 6.0f, " dB", 0.0f, 1),
              toggle ("Auto gain", true), toggle ("SC filter", false), percent ("Mix", 100.0f) },
            maker<CompBlock>()));

        list.push_back (info ("ax_drive", "AXIS Drive", Family::drive, "gfx-overdrive", axisCredit,
            "AXIS's drive: warm, open or tight, with body and bite.",
            { choice ("Voice", { "Warm", "Open", "Tight" }), percent ("Gain", 64.0f), percent ("Tone", 42.0f), percent ("Level", 55.0f),
              percent ("Body", 38.0f), percent ("Bite", 71.0f), toggle ("Dynamic", true), percent ("Mix", 100.0f) },
            maker<DriveBlock>()));

        list.push_back (info ("ax_amp", "AXIS Amp", Family::ampcab, "gfx-amp", axisCredit,
            "AXIS's five amps, modelled from their tubes and tone stacks.",
            { choice ("Voice", axdsp::AmpModel::voiceNames(), 1), percent ("Gain", 50.0f), percent ("Bass", 50.0f), percent ("Mid", 50.0f),
              percent ("Treble", 50.0f), percent ("Presence", 50.0f), percent ("Master", 50.0f), toggle ("Bright", false), toggle ("Tight", false) },
            maker<AmpBlock>()));

        {
            juce::StringArray types;
            for (const auto& s : axdsp::cab::speakerSpecs())
                types.add (s.name);
            types.add ("Pack IR");

            auto irs = Pack::get().cabNames;
            if (irs.isEmpty())
                irs.add ("(no AXIS pack)");

            list.push_back (info ("ax_cab", "AXIS Cab", Family::ampcab, "gfx-cab", axisCredit,
                "AXIS's cabinets: four modelled ones, or an impulse response from the AXIS pack.",
                { choice ("Cabinet", types, axdsp::Cabinet::fallbackType), choice ("IR", irs), percent ("Mic X", 36.0f), percent ("Mic Y", 50.0f),
                  knob ("Low cut", 20.0f, 300.0f, 60.0f, " Hz", 80.0f), knob ("High cut", 2000.0f, 20000.0f, 9000.0f, " Hz", 8000.0f),
                  knob ("Level", -24.0f, 12.0f, 0.0f, " dB", 0.0f, 1) },
                maker<CabBlock>()));
        }

        list.push_back (info ("ax_space", "AXIS Space", Family::ambience, "gfx-reverb", axisCredit,
            "AXIS's reverb: plate, hall or spring, ducking under the playing.",
            { choice ("Type", { "Plate", "Hall", "Spring" }, 2), knob ("Pre-delay", 0.0f, 200.0f, 24.0f, " ms", 40.0f), percent ("Decay", 46.0f),
              percent ("Size", 58.0f), percent ("Damp", 62.0f), percent ("Width", 80.0f), toggle ("Duck", true), toggle ("Freeze", false),
              percent ("Mix", 28.0f) },
            maker<SpaceBlock>(), true));
    }
}
