// C:\workspace\SONIFY PLUGIN natura\src\AttackLibrary.h
#pragma once
//==============================================================================
//  AttackLibrary.h  -  the sampled attacks (the D-50 way): the first 180 ms of
//  real notes, played over the start of the model's note and handed over to
//  it.  The model plays the note; the recording only gives it its first
//  moments - the tongue, the bow's bite, the hammer, the mallet.
//
//  attacks/natura_attacks.bin (compiled in as NaturaAttacks.h): 28 sets cut
//  from VSCO 2 Community Edition and the Versilian Community Sample Library
//  (both CC0), at each note's onset, 48 kHz mono 16-bit, levelled so every
//  attack is at the same level where it hands over to the model.
//
//    "NATK" u32 version, u32 sampleRate, u32 sets, u32 samples
//    per set:    char id[24], char name[24], f32 headMs, f32 fadeMs, u32 first, u32 count
//    per sample: u32 set, f32 rootHz, u32 layer, u32 layers, u32 offset, u32 length, f32 headMs
//    (headMs: where this recording has reached its own sustained level - the
//    handover starts there; never before the set's head time)
//    then every sample's int16 data
//==============================================================================
#include "ModelDsp.h"
#include <vector>
#include <cstring>
#include <cstdint>
#include <cmath>

#if __has_include ("NaturaAttacks.h")
 #include "NaturaAttacks.h"
 #define NATURA_HAS_ATTACKS 1
#else
 #define NATURA_HAS_ATTACKS 0
#endif

namespace natura
{
    /** The sets, in the order the ATTACK SOURCE choice lists them (after AUTO and OFF). */
    enum class AttackSet : int
    {
        Piano, Harpsichord, Harp, Marimba, Xylophone, Vibraphone, Glockenspiel, TubularBells,
        Violin, Viola, Cello, Contrabass,
        Trumpet, TrumpetStraight, TrumpetHarmon, Trombone, Horn, Tuba,
        Clarinet, TenorSax, SopranoSax, Oboe, Bassoon,
        Flute, Piccolo, Recorder, Ocarina, Harmonica,
        kCount
    };
    constexpr int kNumAttackSets = (int) AttackSet::kCount;

    class AttackLibrary
    {
    public:
        struct Set
        {
            float headMs = 40.0f, fadeMs = 50.0f;
            int   first = 0, count = 0;
            float loHz = 0.0f, hiHz = 0.0f;
            bool  present = false;
        };

        struct Sample
        {
            int   set = 0;
            float rootHz = 440.0f;
            int   layer = 0, layers = 1;
            int   offset = 0, length = 0;
            float headMs = 0.0f;
        };

        /** Loaded once, the first time (NaturaEngine::prepare asks first, off the audio thread). */
        static const AttackLibrary& get()
        {
            static const AttackLibrary lib;
            return lib;
        }

        bool ok() const { return loaded; }
        int  rate() const { return sampleRate; }
        const Set& set (int i) const { return sets[(size_t) i]; }
        const Sample& sample (int i) const { return samples[(size_t) i]; }
        const std::int16_t* pcm (int i) const { return data.data() + samples[(size_t) i].offset; }

        /** How many semitones hz lies outside the set's range of notes (0 inside). */
        float semisOutside (int s, float hz) const
        {
            const auto& st = sets[(size_t) s];
            if (! st.present || hz <= 0.0f) return 1.0e9f;
            if (hz < st.loHz) return 12.0f * std::log2 (st.loHz / hz);
            if (hz > st.hiHz) return 12.0f * std::log2 (hz / st.hiHz);
            return 0.0f;
        }

        /** The attack for this pitch and velocity (0..1): the nearest root in the
            velocity's layer, a neighbouring layer when that one has no note close by. */
        int pick (int s, float hz, float vel) const
        {
            const auto& st = sets[(size_t) s];
            if (! st.present) return -1;
            int best = -1;
            float bestCost = 1.0e9f;
            for (int i = st.first; i < st.first + st.count; ++i)
            {
                const auto& sm = samples[(size_t) i];
                const int want = std::clamp ((int) (vel * (float) sm.layers), 0, sm.layers - 1);
                const float cost = std::abs (12.0f * std::log2 (hz / sm.rootHz)) + 6.0f * (float) std::abs (sm.layer - want);
                if (cost < bestCost) { bestCost = cost; best = i; }
            }
            return best;
        }

    private:
        AttackLibrary()
        {
           #if NATURA_HAS_ATTACKS
            load (reinterpret_cast<const unsigned char*> (NaturaAttacks::natura_attacks_bin), (size_t) NaturaAttacks::natura_attacks_binSize);
           #endif
        }

        static std::uint32_t u32 (const unsigned char* p) { std::uint32_t v; std::memcpy (&v, p, 4); return v; }
        static float f32 (const unsigned char* p) { float v; std::memcpy (&v, p, 4); return v; }

        void load (const unsigned char* b, size_t size)
        {
            static const char* ids[kNumAttackSets] = {
                "piano", "harpsichord", "harp", "marimba", "xylophone", "vibraphone", "glockenspiel", "tubularbells",
                "violin", "viola", "cello", "contrabass",
                "trumpet", "trumpetstraight", "trumpetharmon", "trombone", "horn", "tuba",
                "clarinet", "sax", "saxhigh", "oboe", "bassoon",
                "flute", "piccolo", "recorder", "ocarina", "harmonica" };

            if (size < 20 || std::memcmp (b, "NATK", 4) != 0 || u32 (b + 4) != 1) return;
            sampleRate = (int) u32 (b + 8);
            const int nSets = (int) u32 (b + 12), nSamples = (int) u32 (b + 16);
            const size_t setBytes = 64, smpBytes = 28;
            const size_t head = 20 + (size_t) nSets * setBytes + (size_t) nSamples * smpBytes;
            if (head > size || sampleRate <= 0) return;

            sets.assign ((size_t) kNumAttackSets, Set {});
            std::vector<int> fileToSet ((size_t) nSets, -1);
            for (int i = 0; i < nSets; ++i)
            {
                const unsigned char* p = b + 20 + (size_t) i * setBytes;
                char id[25] = {};
                std::memcpy (id, p, 24);
                for (int k = 0; k < kNumAttackSets; ++k)
                    if (std::strcmp (id, ids[k]) == 0) fileToSet[(size_t) i] = k;
                const int k = fileToSet[(size_t) i];
                if (k < 0) continue;
                auto& st = sets[(size_t) k];
                st.headMs = f32 (p + 48);
                st.fadeMs = f32 (p + 52);
            }

            const size_t pcmCount = (size - head) / 2;
            for (int i = 0; i < nSamples; ++i)
            {
                const unsigned char* p = b + 20 + (size_t) nSets * setBytes + (size_t) i * smpBytes;
                const int fs = (int) u32 (p);
                if (fs < 0 || fs >= nSets || fileToSet[(size_t) fs] < 0) continue;
                Sample sm;
                sm.set    = fileToSet[(size_t) fs];
                sm.rootHz = f32 (p + 4);
                sm.layer  = (int) u32 (p + 8);
                sm.layers = std::max (1, (int) u32 (p + 12));
                sm.offset = (int) u32 (p + 16);
                sm.length = (int) u32 (p + 20);
                sm.headMs = f32 (p + 24);
                if ((size_t) sm.offset + (size_t) sm.length > pcmCount || sm.length < 64 || sm.rootHz <= 0.0f) continue;
                samples.push_back (sm);
            }

            // group by set (the file keeps them grouped already; this keeps it so whatever the order)
            std::stable_sort (samples.begin(), samples.end(), [] (const Sample& a, const Sample& c) { return a.set < c.set; });
            for (int i = 0; i < (int) samples.size(); ++i)
            {
                auto& st = sets[(size_t) samples[(size_t) i].set];
                if (! st.present) { st.present = true; st.first = i; st.loHz = st.hiHz = samples[(size_t) i].rootHz; }
                st.count = i - st.first + 1;
                st.loHz = std::min (st.loHz, samples[(size_t) i].rootHz);
                st.hiHz = std::max (st.hiHz, samples[(size_t) i].rootHz);
            }

            data.resize (pcmCount);
            std::memcpy (data.data(), b + head, pcmCount * 2);
            loaded = ! samples.empty();
        }

        bool loaded = false;
        int  sampleRate = 48000;
        std::vector<Set> sets = std::vector<Set> ((size_t) kNumAttackSets);
        std::vector<Sample> samples;
        std::vector<std::int16_t> data;
    };

    //==========================================================================
    //  What each set's attack was played at last time, per register and
    //  velocity, against the model as it is now: the level the next note's
    //  recorded attack starts at, so it meets the model at the same loudness.
    //  The engine keeps one for all its voices (audio thread only).
    //==========================================================================
    struct AttackMemory
    {
        static constexpr int kZones = 11, kVel = 4;
        struct Cell { float gain = -1.0f, vel = 0.0f; };
        Cell cells[kNumAttackSets][kZones][kVel];

        AttackMemory() { reset(); }

        void reset()
        {
            for (auto& s : cells) for (auto& z : s) for (auto& c : z) c = Cell {};
        }

        /** Roughly how the model's loudness at the handover follows velocity, per set
            (measured on the factory instruments): the recordings are levelled, the model
            carries the dynamics.  Only used to carry a level heard at one velocity over
            to another, and never by more than 6 dB. */
        static float velExp (int set)
        {
            using S = AttackSet;
            switch ((S) set)
            {
                case S::Piano:                                     return 2.4f;
                case S::Harpsichord: case S::Harp: case S::Harmonica: return 0.9f;
                case S::Marimba: case S::Xylophone: case S::Vibraphone:
                case S::Glockenspiel: case S::TubularBells:        return 1.2f;
                case S::Violin: case S::Viola: case S::Cello:      return 1.4f;
                case S::Contrabass:                                return 0.6f;
                case S::TrumpetHarmon:                             return 3.0f;
                case S::Trumpet: case S::TrumpetStraight:
                case S::Trombone: case S::Horn: case S::Tuba:      return 1.8f;
                case S::Clarinet: case S::TenorSax: case S::SopranoSax:
                case S::Oboe: case S::Bassoon:                     return 0.5f;
                default:                                           return 1.2f;   // flutes, recorder, ocarina
            }
        }

        /** The first note's guess, before anything has been heard (at full velocity). */
        static float firstGuess (int set)
        {
            static const float g0[kNumAttackSets] = {
                3.1f, 1.2f, 1.0f, 2.3f, 2.2f, 2.3f, 2.2f, 2.2f,     // piano .. tubular bells
                1.5f, 1.3f, 1.1f, 1.1f,                             // violin .. contrabass
                3.0f, 1.6f, 2.2f, 2.2f, 1.6f, 4.0f,                 // trumpet .. tuba
                1.2f, 0.7f, 0.7f, 1.5f, 0.5f,                       // clarinet .. bassoon
                1.3f, 1.1f, 1.5f, 1.5f, 1.1f };                     // flute .. harmonica
            return g0[set];
        }

        static int zone (int note) { return std::clamp (note / 12, 0, kZones - 1); }
        static int velZone (float vel) { return std::clamp ((int) (vel * (float) kVel), 0, kVel - 1); }

        /** The gain to start this note's attack at: the nearest note heard (same register
            and velocity first), carried to this velocity. */
        float lookup (int set, int note, float vel) const
        {
            const int z = zone (note), v = velZone (vel);
            const Cell* best = nullptr;
            float bestCost = 1.0e9f;
            for (int zz = 0; zz < kZones; ++zz)
                for (int vv = 0; vv < kVel; ++vv)
                    if (cells[set][zz][vv].gain > 0.0f)
                    {
                        const float cost = (float) std::abs (zz - z) + 1.5f * (float) std::abs (vv - v);
                        if (cost < bestCost) { bestCost = cost; best = &cells[set][zz][vv]; }
                    }
            if (best == nullptr) return firstGuess (set) * velLaw (set, vel);
            const float carry = dsp::clampv (velLaw (set, vel) / velLaw (set, best->vel), 0.5f, 2.0f);
            return dsp::clampv (best->gain * carry, 1.0e-3f, 20.0f);
        }

        void store (int set, int note, float vel, float gain)
        {
            auto& c = cells[set][zone (note)][velZone (vel)];
            if (c.gain > 0.0f)
            {
                // a new velocity in the cell: bring the old level over to it first
                const float carried = c.gain * dsp::clampv (velLaw (set, vel) / velLaw (set, c.vel), 0.5f, 2.0f);
                c.gain = 0.5f * carried + 0.5f * gain;
            }
            else c.gain = gain;
            c.vel = vel;
        }

    private:
        // the law only between soft and loud: at the very bottom the model is no power of velocity
        static float velLaw (int set, float vel) { return std::pow (dsp::clampv (vel, 0.25f, 1.0f), velExp (set)); }
    };
}
