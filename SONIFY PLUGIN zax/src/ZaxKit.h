// C:\workspace\SONIFY PLUGIN zax\src\ZaxKit.h
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include "ZaxGm.h"
#include "SynthParams.h"       // every sound-shaping field of a key (shape saving)
#include "DrumSplitEq.h"       // the EQ's slider rule: silence .. unity .. +24 dB

//==============================================================================
//  ZaxKit.h  —  a Zax kit, and its file: the .zaxkit.
//
//  A kit is one CELL per GM key 27..84 (ZaxGm.h): the library sound the key
//  plays (a full copy of its prototype, so every shaping value can be edited),
//  the eight macros, a gain, a choke group and the key's two-handle filter.
//  Plus one EFFECT RACK per family and the kit's volume.
//
//  THE RACK IS A CHAIN, NOT SENDS.  Every family's effects run one after the
//  other, in the order the editor's pages show them:
//
//      SWEET -> EQ -> SAT -> COMP -> CHORUS -> DELAY -> REVERB -> OUT
//
//  OUT is the family's pan and level.  A stage that is off costs nothing; a
//  family with nothing on goes straight to the mix.
//
//  THE FILE (.zaxkit) is plain text, integers only (values x1000, shapes
//  x1,000,000), so it reads back identically under any number locale:
//
//      ZAXKIT 1
//      name|Trance
//      volume|0
//      k|36|Deep Punch|0|tune|decay|damp|snap|color|drive|human|velsens|gain|choke|hp|lp[|shape]
//      fx|Clap|reverb=1,rev.size=600,...
//
//  Sounds are stored by NAME (+ hat articulation), so a prototype refined in a
//  later build is heard in every kit.  A key whose line is missing, or names a
//  sound outside its GM role, keeps its role's first sound: a Zax key is never
//  silent and never leaves its role.
//
//  Header-only, no JUCE.
//==============================================================================

namespace zax
{
    //==========================================================================
    struct Cell
    {
        edm::Sound   sound;
        drum::Macros macros;
        float        gainDb       = 0.0f;   // -20 .. +20
        int          chokeGroup   = 0;      // 0 = none, 1..8
        float        filterHpNorm = 0.0f;   // 20 * 1000^norm: 0 = low cut open
        float        filterLpNorm = 1.0f;   //                  1 = high cut open
    };

    //==========================================================================
    //  One family's rack.  Native units; the editor maps every field to 0..200.
    //==========================================================================
    struct FamilyFx
    {
        bool  sweetOn = false;
        float sweetMix = 1.0f, softenDepth = 0.0f, softenMs = 8.0f, peakCeilDb = 12.0f, peakRatio = 4.0f,
              tameDepthDb = 0.0f, tameFreqHz = 4000.0f, roundDrive = 0.0f, roundMix = 1.0f;

        bool  eqOn = false;
        float eqGainDb[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };   // 31 Hz .. 16 kHz, one band each

        bool  satOn = false;
        float satDrive = 0.0f, satMix = 1.0f;

        bool  compOn = false;
        float compThreshDb = -12.0f, compRatio = 4.0f, compAttackMs = 5.0f, compReleaseMs = 80.0f, compMakeupDb = 0.0f;

        bool  chorusOn = false;
        float chorusRate = 0.3f, chorusDepth = 0.5f, chorusMix = 0.5f;            // 0..1 each

        bool  delayOn = false;
        float delayNote = 4.0f;          // 0 = free (TIME), 1..9 = a note value (see delayDivision)
        float delayMs = 375.0f, delayFeedback = 0.35f, delayDampHz = 5000.0f, delayLowCutHz = 150.0f, delayMix = 0.25f;

        bool  reverbOn = false;
        float revSize = 0.5f, revDecay = 0.5f, revDamp = 0.5f, revPreMs = 25.0f,
              revLowCutHz = 150.0f, revHighCutHz = 12000.0f, revMix = 0.2f;

        float pan = 0.0f;                // -1 left .. +1 right
        float levelDb = 0.0f;            // -24 .. +12

        bool runs() const noexcept
        {
            return sweetOn || eqOn || satOn || compOn || chorusOn || delayOn || reverbOn
                || pan != 0.0f || levelDb != 0.0f;
        }
    };

    // The note values of the delay's NOTE control, in quarter notes (0 = free)
    inline double delayDivision (int note) noexcept
    {
        static const double q[10] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 1.5, 0.75, 2.0 / 3.0, 1.0 / 3.0 };
        return q[std::min (9, std::max (0, note))];
    }

    inline const char* delayNoteName (int note) noexcept
    {
        static const char* n[10] = { "FREE", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4.", "1/8.", "1/4T", "1/8T" };
        return n[std::min (9, std::max (0, note))];
    }

    //==========================================================================
    //  THE RACK'S PAGES AND CONTROLS - one table for the editor, the file and
    //  the defaults.  A page's `on` is its stage switch (OUT has none).
    //==========================================================================
    enum class FxMap : uint8_t { Lin, Log, EqBand, Step };

    struct FxSpec
    {
        int         page;
        const char* id;                 // file id (never rename)
        const char* label;              // slider caption
        float       lo, hi;
        FxMap       map;
        float FamilyFx::* field;        // nullptr = an EQ band (band)
        int         band;
    };

    static constexpr int kNumFxPages = 8;

    inline const char* fxPageName (int p) noexcept
    {
        static const char* n[kNumFxPages] = { "SWEET", "EQ", "SAT", "COMP", "CHORUS", "DELAY", "REVERB", "OUT" };
        return (p >= 0 && p < kNumFxPages) ? n[p] : "";
    }

    inline bool* fxStage (FamilyFx& fx, int page) noexcept
    {
        switch (page)
        {
            case 0: return &fx.sweetOn;  case 1: return &fx.eqOn;     case 2: return &fx.satOn;
            case 3: return &fx.compOn;   case 4: return &fx.chorusOn; case 5: return &fx.delayOn;
            case 6: return &fx.reverbOn; default: return nullptr;
        }
    }

    inline const char* fxStageId (int page) noexcept
    {
        static const char* n[7] = { "sweet", "eq", "sat", "comp", "chorus", "delay", "reverb" };
        return (page >= 0 && page < 7) ? n[page] : "";
    }

    inline const std::vector<FxSpec>& fxSpecs()
    {
        using F = FamilyFx;
        constexpr float eqLo = Betel::DrumSplitEq::kSilenceDb, eqHi = Betel::DrumSplitEq::kMaxDb;
        static const std::vector<FxSpec> v
        {
            { 0, "sw.mix",    "MIX",      0.0f, 1.0f,     FxMap::Lin, &F::sweetMix,    -1 },
            { 0, "sw.soft",   "SOFTEN",  -1.0f, 1.0f,     FxMap::Lin, &F::softenDepth, -1 },
            { 0, "sw.softms", "SOFT TM",  1.0f, 150.0f,   FxMap::Log, &F::softenMs,    -1 },
            { 0, "sw.peak",   "PEAK",     3.0f, 24.0f,    FxMap::Lin, &F::peakCeilDb,  -1 },
            { 0, "sw.pratio", "P RATIO",  1.0f, 20.0f,    FxMap::Log, &F::peakRatio,   -1 },
            { 0, "sw.tame",   "TAME",     0.0f, 12.0f,    FxMap::Lin, &F::tameDepthDb, -1 },
            { 0, "sw.tamehz", "TAME HZ",  1500.0f, 8000.0f, FxMap::Log, &F::tameFreqHz, -1 },
            { 0, "sw.round",  "ROUND",    0.0f, 1.0f,     FxMap::Lin, &F::roundDrive,  -1 },
            { 0, "sw.rmix",   "R MIX",    0.0f, 1.0f,     FxMap::Lin, &F::roundMix,    -1 },

            { 1, "e0", "31",  eqLo, eqHi, FxMap::EqBand, nullptr, 0 }, { 1, "e1", "62",  eqLo, eqHi, FxMap::EqBand, nullptr, 1 },
            { 1, "e2", "125", eqLo, eqHi, FxMap::EqBand, nullptr, 2 }, { 1, "e3", "250", eqLo, eqHi, FxMap::EqBand, nullptr, 3 },
            { 1, "e4", "500", eqLo, eqHi, FxMap::EqBand, nullptr, 4 }, { 1, "e5", "1K",  eqLo, eqHi, FxMap::EqBand, nullptr, 5 },
            { 1, "e6", "2K",  eqLo, eqHi, FxMap::EqBand, nullptr, 6 }, { 1, "e7", "4K",  eqLo, eqHi, FxMap::EqBand, nullptr, 7 },
            { 1, "e8", "8K",  eqLo, eqHi, FxMap::EqBand, nullptr, 8 }, { 1, "e9", "16K", eqLo, eqHi, FxMap::EqBand, nullptr, 9 },

            { 2, "sat.drive", "DRIVE",    0.0f, 1.0f,     FxMap::Lin, &F::satDrive,    -1 },
            { 2, "sat.mix",   "MIX",      0.0f, 1.0f,     FxMap::Lin, &F::satMix,      -1 },

            { 3, "comp.thr",  "THRESH", -60.0f, 0.0f,     FxMap::Lin, &F::compThreshDb,  -1 },
            { 3, "comp.ratio","RATIO",    1.0f, 20.0f,    FxMap::Log, &F::compRatio,     -1 },
            { 3, "comp.att",  "ATTACK",   0.1f, 200.0f,   FxMap::Log, &F::compAttackMs,  -1 },
            { 3, "comp.rel",  "RELEASE",  5.0f, 2000.0f,  FxMap::Log, &F::compReleaseMs, -1 },
            { 3, "comp.mk",   "MAKEUP",   0.0f, 24.0f,    FxMap::Lin, &F::compMakeupDb,  -1 },

            { 4, "ch.rate",   "RATE",     0.0f, 1.0f,     FxMap::Lin, &F::chorusRate,  -1 },
            { 4, "ch.depth",  "DEPTH",    0.0f, 1.0f,     FxMap::Lin, &F::chorusDepth, -1 },
            { 4, "ch.mix",    "MIX",      0.0f, 1.0f,     FxMap::Lin, &F::chorusMix,   -1 },

            { 5, "dl.note",   "NOTE",     0.0f, 9.0f,     FxMap::Step, &F::delayNote,    -1 },
            { 5, "dl.ms",     "TIME",     10.0f, 2400.0f, FxMap::Log,  &F::delayMs,      -1 },
            { 5, "dl.fb",     "FEEDBACK", 0.0f, 0.95f,    FxMap::Lin,  &F::delayFeedback, -1 },
            { 5, "dl.damp",   "DAMP",     500.0f, 20000.0f, FxMap::Log, &F::delayDampHz,  -1 },
            { 5, "dl.lowcut", "LOW CUT",  20.0f, 1000.0f, FxMap::Log,  &F::delayLowCutHz, -1 },
            { 5, "dl.mix",    "MIX",      0.0f, 1.0f,     FxMap::Lin,  &F::delayMix,     -1 },

            { 6, "rv.size",   "SIZE",     0.0f, 1.0f,     FxMap::Lin, &F::revSize,      -1 },
            { 6, "rv.decay",  "DECAY",    0.0f, 1.0f,     FxMap::Lin, &F::revDecay,     -1 },
            { 6, "rv.damp",   "DAMP",     0.0f, 1.0f,     FxMap::Lin, &F::revDamp,      -1 },
            { 6, "rv.pre",    "PRE-DLY",  25.0f, 200.0f,  FxMap::Lin, &F::revPreMs,     -1 },
            { 6, "rv.lowcut", "LOW CUT",  20.0f, 1000.0f, FxMap::Log, &F::revLowCutHz,  -1 },
            { 6, "rv.hicut",  "HIGH CUT", 1000.0f, 20000.0f, FxMap::Log, &F::revHighCutHz, -1 },
            { 6, "rv.mix",    "MIX",      0.0f, 1.0f,     FxMap::Lin, &F::revMix,       -1 },

            { 7, "out.pan",   "PAN",     -1.0f, 1.0f,     FxMap::Lin, &F::pan,          -1 },
            { 7, "out.level", "LEVEL",  -24.0f, 12.0f,    FxMap::Lin, &F::levelDb,      -1 },
        };
        return v;
    }

    inline float& fxRef (FamilyFx& fx, const FxSpec& sp) noexcept
    {
        return sp.field != nullptr ? fx.*(sp.field) : fx.eqGainDb[sp.band];
    }

    inline float fxClamp (const FxSpec& sp, float v) noexcept
    {
        v = std::min (sp.hi, std::max (sp.lo, v));
        return sp.map == FxMap::Step ? std::round (v) : v;
    }

    // value <-> 0..200 on screen
    inline float fxToUi (const FxSpec& sp, float v) noexcept
    {
        if (sp.map == FxMap::EqBand) return 200.0f * Betel::DrumSplitEq::positionFromDb (v);
        v = fxClamp (sp, v);
        if (sp.map == FxMap::Log)    return 200.0f * std::log (v / sp.lo) / std::log (sp.hi / sp.lo);
        return 200.0f * (v - sp.lo) / (sp.hi - sp.lo);
    }

    inline float fxFromUi (const FxSpec& sp, float u) noexcept
    {
        const float t = std::min (200.0f, std::max (0.0f, u)) / 200.0f;
        if (sp.map == FxMap::EqBand) return Betel::DrumSplitEq::dbFromPosition (t);
        if (sp.map == FxMap::Log)    return sp.lo * std::pow (sp.hi / sp.lo, t);
        return fxClamp (sp, sp.lo + (sp.hi - sp.lo) * t);
    }

    //==========================================================================
    struct Kit
    {
        std::string                         name;
        std::array<Cell, 128>               cells {};
        std::array<FamilyFx, kNumFamilies>  fx {};
        float                               volumeDb = 0.0f;   // -24 .. +12
    };

    //==========================================================================
    //  A kit where every key holds its role's first sound, neutral: what a key
    //  falls back to when a file says nothing about it.
    //==========================================================================
    inline void fillRoleDefaults (Kit& k)
    {
        for (int key = kFirstKey; key <= kLastKey; ++key)
        {
            auto& c = k.cells[(size_t) key];
            const auto sounds = soundsForKey (key);
            c = Cell();
            if (! sounds.empty())
                c.sound = edm::library()[(size_t) sounds.front()].sound;
            c.chokeGroup = gm (key)->choke;
        }
    }

    // A key's library index (its sound by name + articulation), or -1
    inline int libraryIndexOf (const Cell& c)
    {
        return c.sound.isEmpty() ? -1 : edm::findSound (c.sound.name(), c.sound.articulation);
    }

    //==========================================================================
    //  THE FILE
    //==========================================================================
    namespace file
    {
        inline std::string q3 (float v)  { return std::to_string ((long long) std::llround ((double) v * 1000.0)); }
        inline float       dq3 (const std::string& s) { return (float) ((double) std::atoll (s.c_str()) / 1000.0); }

        inline std::vector<std::string> split (const std::string& line, char sep)
        {
            std::vector<std::string> f;
            for (size_t a = 0;;)
            {
                const size_t b = line.find (sep, a);
                f.push_back (line.substr (a, b == std::string::npos ? std::string::npos : b - a));
                if (b == std::string::npos) break;
                a = b + 1;
            }
            return f;
        }

        inline std::string fxToString (const FamilyFx& fx)
        {
            std::string out;
            auto add = [&out] (const std::string& id, const std::string& v) { out += (out.empty() ? "" : ",") + id + "=" + v; };
            FamilyFx copy = fx;
            for (int p = 0; p < 7; ++p)
                add (fxStageId (p), *fxStage (copy, p) ? "1" : "0");
            for (const auto& sp : fxSpecs())
                add (sp.id, q3 (fxRef (copy, sp)));
            return out;
        }

        inline void fxParse (const std::string& text, FamilyFx& fx)
        {
            for (const auto& item : split (text, ','))
            {
                const size_t eq = item.find ('=');
                if (eq == std::string::npos) continue;
                const std::string id = item.substr (0, eq), v = item.substr (eq + 1);
                bool done = false;
                for (int p = 0; p < 7 && ! done; ++p)
                    if (id == fxStageId (p)) { *fxStage (fx, p) = std::atoll (v.c_str()) != 0; done = true; }
                for (const auto& sp : fxSpecs())
                    if (! done && id == sp.id) { fxRef (fx, sp) = sp.map == FxMap::EqBand
                                                                    ? std::min (sp.hi, std::max (sp.lo, dq3 (v)))
                                                                    : fxClamp (sp, dq3 (v));
                                                 done = true; }
            }
        }

        inline bool sameFx (const FamilyFx& a, const FamilyFx& b)   { return fxToString (a) == fxToString (b); }
    }

    inline std::string serializeKit (const Kit& kit)
    {
        using namespace file;
        std::string out = "ZAXKIT 1\n";
        out += "name|" + kit.name + "\n";
        out += "volume|" + q3 (kit.volumeDb) + "\n";
        for (int key = kFirstKey; key <= kLastKey; ++key)
        {
            const auto& c = kit.cells[(size_t) key];
            if (c.sound.isEmpty()) continue;
            const auto& m = c.macros;
            out += "k|" + std::to_string (key) + "|" + c.sound.name() + "|" + std::to_string (c.sound.articulation)
                 + "|" + q3 (m.tuneSemis) + "|" + q3 (m.decay) + "|" + q3 (m.damp) + "|" + q3 (m.snap)
                 + "|" + q3 (m.color) + "|" + q3 (m.drive) + "|" + q3 (m.humanize) + "|" + q3 (m.velSens)
                 + "|" + q3 (c.gainDb) + "|" + std::to_string (c.chokeGroup)
                 + "|" + q3 (c.filterHpNorm) + "|" + q3 (c.filterLpNorm);
            if (const edm::Sound* ref = edm::params::librarySound (c.sound))
            {
                const auto shape = edm::params::shapeDiff (*ref, c.sound);
                if (! shape.empty()) out += "|" + shape;
            }
            out += "\n";
        }
        const FamilyFx neutral;
        for (int f = 0; f < kNumFamilies; ++f)
            if (! sameFx (kit.fx[(size_t) f], neutral))
                out += std::string ("fx|") + familyName (f) + "|" + fxToString (kit.fx[(size_t) f]) + "\n";
        return out;
    }

    //  Parse a .zaxkit into `out`.  False only when the text is not a Zax kit.
    inline bool deserializeKit (const std::string& text, Kit& out, std::vector<std::string>* problems = nullptr)
    {
        using namespace file;
        if (text.rfind ("ZAXKIT 1", 0) != 0) return false;

        auto heap = std::make_unique<Kit>();          // ~45 KB: never on the stack
        Kit& k = *heap;
        fillRoleDefaults (k);
        k.name = out.name;

        auto clampF = [] (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); };

        for (auto line : split (text, '\n'))
        {
            if (! line.empty() && line.back() == '\r') line.pop_back();
            const auto f = split (line, '|');
            if (f.size() < 2) continue;

            if (f[0] == "name")   { k.name = f[1]; continue; }
            if (f[0] == "volume") { k.volumeDb = clampF (dq3 (f[1]), -24.0f, 12.0f); continue; }
            if (f[0] == "fx" && f.size() >= 3)
            {
                const int fi = familyIndex (f[1].c_str());
                if (fi >= 0) { k.fx[(size_t) fi] = FamilyFx(); fxParse (f[2], k.fx[(size_t) fi]); }
                continue;
            }
            if (f[0] != "k" || f.size() < 16) continue;

            const int key = std::atoi (f[1].c_str());
            if (! isZaxKey (key)) continue;

            const int idx = findForKey (key, f[2].c_str());
            if (idx < 0)
            {
                if (problems != nullptr) problems->push_back ("key " + f[1] + ": " + f[2] + " is not a " + roleName (key) + " sound");
                continue;                               // the key keeps its role's default
            }
            auto& c = k.cells[(size_t) key];
            c.sound             = edm::library()[(size_t) idx].sound;
            c.macros.tuneSemis  = clampF (dq3 (f[4]),  -25.0f, 25.0f);
            c.macros.decay      = clampF (dq3 (f[5]),  0.0f, 1.0f);
            c.macros.damp       = clampF (dq3 (f[6]),  0.0f, 1.0f);
            c.macros.snap       = clampF (dq3 (f[7]),  0.0f, 1.0f);
            c.macros.color      = clampF (dq3 (f[8]),  0.0f, 1.0f);
            c.macros.drive      = clampF (dq3 (f[9]),  0.0f, 1.0f);
            c.macros.humanize   = clampF (dq3 (f[10]), 0.0f, 1.0f);
            c.macros.velSens    = clampF (dq3 (f[11]), 0.0f, 1.0f);
            c.gainDb            = clampF (dq3 (f[12]), -20.0f, 20.0f);
            c.chokeGroup        = std::min (8, std::max (0, std::atoi (f[13].c_str())));
            c.filterHpNorm      = clampF (dq3 (f[14]), 0.0f, 1.0f);
            c.filterLpNorm      = clampF (dq3 (f[15]), 0.0f, 1.0f);
            if (c.filterHpNorm > c.filterLpNorm) std::swap (c.filterHpNorm, c.filterLpNorm);
            if (f.size() >= 17 && ! f[16].empty())
                edm::params::applyShape (c.sound, f[16]);
        }

        out = k;
        return true;
    }
}
