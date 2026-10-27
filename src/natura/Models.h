// C:\workspace\SONIFY PLUGIN natura\src\Models.h
#pragma once
//==============================================================================
//  Models.h  -  the thirteen archetypes and the bank a voice carries.
//
//  Archetypes are grouped by HOW THE SOUND IS MADE, not by GM family name, so
//  one root model can be edited into every instrument that shares its
//  mechanism (one Air Jet becomes a flute, a recorder or a pan flute).
//==============================================================================
#include "ModelsKeys.h"
#include "ModelsOrgan.h"
#include "ModelsStrings.h"
#include "ModelsWind.h"

namespace natura
{
    enum class Archetype
    {
        StruckString = 0, TineEP, StruckBar, Tonewheel, PipeOrgan, FreeReed,
        PluckedString, BowedString, LipReed, SingleReed, DoubleReed, AirJet, Synth,
        kCount
    };

    constexpr int kNumArchetypes = (int) Archetype::kCount;

    inline const char* archetypeName (int a)
    {
        static const char* names[kNumArchetypes] = {
            "STRUCK STRING", "TINE / REED EP", "STRUCK BAR", "TONEWHEEL ORGAN", "PIPE ORGAN", "FREE REED",
            "PLUCKED STRING", "BOWED STRING", "LIP REED (BRASS)", "SINGLE REED", "DOUBLE REED", "AIR JET", "SYNTH"
        };
        return names[std::min (kNumArchetypes - 1, std::max (0, a))];
    }

    //==========================================================================
    //  SYNTH: the Bambino two-oscillator engine (Grex SOUND ENGINE).
    //  The shared sections act as macros on it: strength = drive, hardness =
    //  brightness, noise = noise, decay = filter-envelope decay, damping =
    //  darker, inharmonicity = FM, odd/even = pulse width.
    //==========================================================================
    class SynthModel final : public ModelBase
    {
    public:
        void prepare (double sampleRate) override { sr = sampleRate; reset(); }
        void reset() override { v = {}; }
        bool isSustained() const override { return false; }
        Dynamics dynamics() const override { return Dynamics::Struck; }

        void start (const ModelIn& in, bool legato) override
        {
            seed = seed * 1664525u + 1013904223u + (std::uint32_t) in.note;
            oscengine::noteOn (v, in.velocity, seed, legato);
        }

        void release (const ModelIn&) override { oscengine::release (v); }

        void render (const ModelIn& in, float* out, int n) override
        {
            if (in.osc == nullptr) { std::fill (out, out + n, 0.0f); return; }
            oscengine::Params p = *in.osc;
            p.drive  = dsp::clampv (p.drive + (in.strength - 0.5f) * 0.8f, 0.0f, 1.0f);
            p.cutoff = dsp::clampv (p.cutoff + (in.hardness - 0.5f) * 0.4f - (in.damping - 0.5f) * 0.4f + in.bright * 0.25f, 0.0f, 1.0f);
            p.noise  = dsp::clampv (p.noise + std::max (0.0f, in.noise - 0.2f) * 0.5f, 0.0f, 1.0f);
            p.envD   = dsp::clampv (p.envD + (in.decay - 0.5f) * 0.5f, 0.0f, 1.0f);
            p.fm     = dsp::clampv (p.fm + std::max (0.0f, in.inharm - 0.2f) * 0.6f, 0.0f, 1.0f);
            for (int o = 0; o < 2; ++o)
                p.pw[o] = dsp::clampv (p.pw[o] + (in.oddEven - 0.5f) * 0.6f, 0.05f, 0.95f);
            oscengine::prepareBlock (v, p, sr);
            for (int i = 0; i < n; ++i)
                out[i] = oscengine::render (v, p, (double) in.freq, in.note, sr) * 0.6f;
        }

    private:
        oscengine::Voice v;
        std::uint32_t seed = 0x1234567u;
    };

    //==========================================================================
    //  The bank: one instance of every model, so switching archetype never
    //  allocates on the audio thread.  Only the active one runs.
    //==========================================================================
    class ModelBank
    {
    public:
        void prepare (double sampleRate)
        {
            for (auto* m : all()) m->prepare (sampleRate);
        }
        void reset() { for (auto* m : all()) m->reset(); }

        ModelBase& get (Archetype a)
        {
            switch (a)
            {
                case Archetype::StruckString:  return struck;
                case Archetype::TineEP:        return tine;
                case Archetype::StruckBar:     return bar;
                case Archetype::Tonewheel:     return wheel;
                case Archetype::PipeOrgan:     return pipe;
                case Archetype::FreeReed:      return freeReed;
                case Archetype::PluckedString: return plucked;
                case Archetype::BowedString:   return bowed;
                case Archetype::LipReed:       return lip;
                case Archetype::SingleReed:    return single;
                case Archetype::DoubleReed:    return dbl;
                case Archetype::AirJet:        return jet;
                case Archetype::Synth:
                default:                       return synth;
            }
        }

    private:
        std::array<ModelBase*, kNumArchetypes> all()
        {
            return { &struck, &tine, &bar, &wheel, &pipe, &freeReed, &plucked, &bowed, &lip, &single, &dbl, &jet, &synth };
        }

        StruckStringModel  struck;
        TineModel          tine;
        StruckBarModel     bar;
        TonewheelModel     wheel;
        PipeOrganModel     pipe;
        FreeReedModel      freeReed;
        PluckedStringModel plucked;
        BowedStringModel   bowed;
        LipReedModel       lip;
        SingleReedModel    single;
        DoubleReedModel    dbl;
        AirJetModel        jet;
        SynthModel         synth;
    };
}
