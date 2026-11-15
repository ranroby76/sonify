// D:\workspace\Fanan_web_hub\src\timberwolf\faust\FaustGlue.h
#pragma once
//==============================================================================
//  FaustGlue.h  -  what the Faust compiler's C++ needs around it, and the block
//  that plays one.
//
//  The effects in this folder (faust\*.h) are C++ written by the Faust compiler
//  (2.70, "-lang cpp -scn FaustDsp -ftz 2") from the .dsp texts beside them in
//  the build notes, over the Faust libraries (github.com/grame-cncm/
//  faustlibraries).  The libraries' LGPL comes with GRAME's exception: the code
//  the compiler writes may be shipped under any license.  Each function used
//  states its own license in the generated metadata() (MIT, BSD, STK-4.3 and
//  the LGPL with that exception only: none is GPL), and its authors are in
//  Timber Wolf's credits.
//
//  FaustBlock plays one as a Timber Wolf effect: the effect's knobs are the
//  Faust sliders of the same names (in the same units), and a mono effect runs
//  once on a mono guitar and once per side on stereo.
//==============================================================================
#include "../blocks/BlockKit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <math.h>
#include <string>
#include <vector>

#ifndef FAUSTFLOAT
 #define FAUSTFLOAT float
#endif

namespace tw::faustglue
{
    struct Meta
    {
        virtual ~Meta() = default;
        virtual void declare (const char*, const char*) {}
    };

    struct UI
    {
        virtual ~UI() = default;
        virtual void openTabBox (const char*) {}
        virtual void openHorizontalBox (const char*) {}
        virtual void openVerticalBox (const char*) {}
        virtual void closeBox() {}
        virtual void addButton (const char* label, FAUSTFLOAT* zone)          { add (label, zone); }
        virtual void addCheckButton (const char* label, FAUSTFLOAT* zone)     { add (label, zone); }
        virtual void addVerticalSlider (const char* label, FAUSTFLOAT* zone, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT)   { add (label, zone); }
        virtual void addHorizontalSlider (const char* label, FAUSTFLOAT* zone, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) { add (label, zone); }
        virtual void addNumEntry (const char* label, FAUSTFLOAT* zone, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT)         { add (label, zone); }
        virtual void addHorizontalBargraph (const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) {}
        virtual void addVerticalBargraph (const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) {}
        virtual void declare (FAUSTFLOAT*, const char*, const char*) {}

        virtual void add (const char*, FAUSTFLOAT*) {}
    };

    class FaustDsp
    {
    public:
        virtual ~FaustDsp() = default;
        virtual int getNumInputs() = 0;
        virtual int getNumOutputs() = 0;
        virtual void buildUserInterface (UI*) = 0;
        virtual int getSampleRate() = 0;
        virtual void init (int) = 0;
        virtual void instanceInit (int) = 0;
        virtual void instanceConstants (int) = 0;
        virtual void instanceResetUserInterface() = 0;
        virtual void instanceClear() = 0;
        virtual FaustDsp* clone() = 0;
        virtual void metadata (Meta*) = 0;
        virtual void compute (int, FAUSTFLOAT**, FAUSTFLOAT**) = 0;
    };

    /** The sliders of a Faust effect, by their labels. */
    struct Zones final : UI
    {
        std::vector<std::pair<std::string, FAUSTFLOAT*>> list;
        void add (const char* label, FAUSTFLOAT* zone) override   { list.emplace_back (label, zone); }

        FAUSTFLOAT* find (const juce::String& name) const
        {
            for (const auto& [label, zone] : list)
                if (name.equalsIgnoreCase (juce::String (label)))
                    return zone;

            return nullptr;
        }
    };

    //==========================================================================
    template <typename Dsp>
    class FaustBlock : public Block
    {
    public:
        /** names: the effect's knob names, in order (the sliders they drive). */
        explicit FaustBlock (juce::StringArray knobNames) : names (std::move (knobNames)) {}

        void prepare (double sampleRate, int maxBlock) override
        {
            rate = sampleRate;

            for (int side = 0; side < 2; ++side)
            {
                dsp[side] = std::make_unique<Dsp>();
                dsp[side]->init ((int) std::round (sampleRate));

                Zones zones;
                dsp[side]->buildUserInterface (&zones);

                for (int k = 0; k < maxKnobs; ++k)
                    zone[side][(size_t) k] = k < names.size() ? zones.find (names[k]) : nullptr;
            }

            ins = dsp[0]->getNumInputs();
            outs = dsp[0]->getNumOutputs();

            for (auto& b : scratch)
                b.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);

            secondStale = true;
        }

        void reset() override
        {
            for (auto& d : dsp)
                if (d != nullptr)
                    d->instanceClear();
        }

        void process (float* l, float* r, int n) override
        {
            for (int side = 0; side < 2; ++side)
                for (int k = 0; k < maxKnobs; ++k)
                    if (auto* z = zone[side][(size_t) k])
                        *z = valueFor (k);

            float* ol = scratch[0].data();
            float* orr = scratch[1].data();

            if (ins == 2 && outs == 2)
            {
                float* in[] { l, r };
                float* out[] { ol, orr };
                dsp[0]->compute (n, in, out);
                std::copy (ol, ol + n, l);
                std::copy (orr, orr + n, r);
                return;
            }

            // Mono: one run when both sides are the same (a guitar), one per side when they differ.
            const bool mono = kit::isMono (l, r, n);

            {
                float* in[] { l };
                float* out[] { ol };
                dsp[0]->compute (n, in, out);
            }

            if (mono)
            {
                std::copy (ol, ol + n, l);
                std::copy (ol, ol + n, r);
                secondStale = true;
                return;
            }

            if (secondStale)
            {
                dsp[1]->instanceClear();
                secondStale = false;
            }

            {
                float* in[] { r };
                float* out[] { orr };
                dsp[1]->compute (n, in, out);
            }

            std::copy (ol, ol + n, l);
            std::copy (orr, orr + n, r);
        }

    protected:
        /** What a knob's slider gets (its own value: an effect can work some out, a synced time). */
        virtual float valueFor (int k) const    { return knob[(size_t) k]; }

        double rate = 48000.0;

    private:
        juce::StringArray names;
        std::unique_ptr<Dsp> dsp[2];
        std::array<std::array<FAUSTFLOAT*, maxKnobs>, 2> zone {};
        std::vector<float> scratch[2];
        int ins = 1, outs = 1;
        bool secondStale = true;
    };
}
