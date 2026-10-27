// C:\workspace\SONIFY PLUGIN natura\src\ToneLayer.h
#pragma once
//==============================================================================
//  ToneLayer.h  -  the TONE LAYER: a wavetable voice under the model, the
//  D-50's second half (the sampled attack is the first).  It plays the steady
//  spectrum of a real instrument - or a designed character such as a duduk or
//  a klezmer clarinet - at the model's pitch, and breathes with the model's
//  own loudness, so the model keeps its life and the layer gives it the body,
//  the depth and the colour.
//
//  attacks/natura_tones.bin (compiled in as NaturaTones.h): harmonic spectra
//  measured on the steady part of notes from VSCO 2 Community Edition and the
//  Versilian Community Sample Library (both CC0), soft and loud, across each
//  instrument's range; and the designed characters, made the same way from
//  their recipes (a harmonic source and formants).
//
//    "NATN" u32 version, u32 sets, u32 tables
//    per set:   char id[24], u32 first, u32 count
//    per table: u32 set, f32 rootHz, u32 layer, u32 layers, u32 harmonics,
//               then harmonics x u8 (0.5 dB steps below the loudest one)
//               and harmonics x u8 (phase, 256 steps)
//
//  A note's cycle is built from the table nearest its pitch: the table's own
//  harmonic pattern (a clarinet's missing evens stay missing), its formants
//  kept where they are in frequency (and moved by FORMANT), soft and loud
//  blended by velocity and COLOR, band-limited to the note.
//==============================================================================
#include "ModelDsp.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#if __has_include ("NaturaTones.h")
 #include "NaturaTones.h"
 #define NATURA_HAS_TONES 1
#else
 #define NATURA_HAS_TONES 0
#endif

namespace natura
{
    /** The tables, in the order the TONE SOURCE choice lists them (after AUTO and OFF). */
    enum class ToneSet : int
    {
        Piano, Harp, Marimba, Vibraphone,
        Violin, Viola, Cello, Contrabass,
        Trumpet, TrumpetStraight, TrumpetHarmon, Trombone, Horn, Tuba,
        Clarinet, TenorSax, SopranoSax, Oboe, Bassoon,
        Flute, Piccolo, Recorder, Ocarina, Harmonica, Organ, Psaltery,
        Duduk, Klezmer, Zurna, Ney, Bagpipe, Warm, Hollow, Nasal, Glassy,
        kCount
    };
    constexpr int kNumToneSets = (int) ToneSet::kCount;

    class ToneLibrary
    {
    public:
        static constexpr int kMaxHarm = 64;

        struct Table
        {
            int   set = 0, layer = 0, layers = 1, count = 0;
            float root = 220.0f;
            std::array<float, kMaxHarm> amp {}, env {}, phase {};    // env: the upper envelope (the formants without the odd/even pattern)
        };

        struct Set { int first = 0, count = 0; float lo = 0.0f, hi = 0.0f; bool present = false; };

        static const ToneLibrary& get() { static const ToneLibrary lib; return lib; }

        bool ok() const { return ! tables.empty(); }
        const Set& set (int s) const { return sets[(size_t) s]; }
        const Table& table (int i) const { return tables[(size_t) i]; }

        float semisOutside (int s, float hz) const
        {
            const auto& st = sets[(size_t) s];
            if (! st.present || hz <= 0.0f) return 1.0e9f;
            if (hz < st.lo) return 12.0f * std::log2 (st.lo / hz);
            if (hz > st.hi) return 12.0f * std::log2 (hz / st.hi);
            return 0.0f;
        }

        /** The table of that layer (0 soft .. layers-1 loud; -1 the loudest) nearest hz. */
        int nearest (int s, float hz, int layer) const
        {
            const auto& st = sets[(size_t) s];
            int best = -1;
            float bestD = 1.0e9f;
            for (int i = st.first; i < st.first + st.count; ++i)
            {
                const auto& t = tables[(size_t) i];
                const int want = layer < 0 ? t.layers - 1 : std::min (layer, t.layers - 1);
                if (t.layer != want) continue;
                const float d = std::abs (std::log2 (hz / t.root));
                if (d < bestD) { bestD = d; best = i; }
            }
            return best;
        }

    private:
        ToneLibrary()
        {
           #if NATURA_HAS_TONES
            load (reinterpret_cast<const unsigned char*> (NaturaTones::natura_tones_bin), (size_t) NaturaTones::natura_tones_binSize);
           #endif
        }

        static std::uint32_t u32 (const unsigned char* p) { std::uint32_t v; std::memcpy (&v, p, 4); return v; }
        static float f32 (const unsigned char* p) { float v; std::memcpy (&v, p, 4); return v; }

        void load (const unsigned char* b, size_t size)
        {
            static const char* ids[kNumToneSets] = {
                "piano", "harp", "marimba", "vibraphone", "violin", "viola", "cello", "contrabass",
                "trumpet", "trumpetstraight", "trumpetharmon", "trombone", "horn", "tuba",
                "clarinet", "sax", "saxhigh", "oboe", "bassoon",
                "flute", "piccolo", "recorder", "ocarina", "harmonica", "organ", "psaltery",
                "duduk", "klezmer", "zurna", "ney", "bagpipe", "warm", "hollow", "nasal", "glassy" };

            if (size < 16 || std::memcmp (b, "NATN", 4) != 0 || u32 (b + 4) != 1) return;
            const int nSets = (int) u32 (b + 8), nTables = (int) u32 (b + 12);
            size_t pos = 16;
            std::vector<int> fileToSet ((size_t) nSets, -1);
            for (int i = 0; i < nSets; ++i, pos += 32)
            {
                if (pos + 32 > size) return;
                char id[25] = {};
                std::memcpy (id, b + pos, 24);
                for (int k = 0; k < kNumToneSets; ++k)
                    if (std::strcmp (id, ids[k]) == 0) fileToSet[(size_t) i] = k;
            }

            sets.assign ((size_t) kNumToneSets, Set {});
            for (int i = 0; i < nTables; ++i)
            {
                if (pos + 20 > size) return;
                const int fs = (int) u32 (b + pos);
                Table t;
                t.root   = f32 (b + pos + 4);
                t.layer  = (int) u32 (b + pos + 8);
                t.layers = std::max (1, (int) u32 (b + pos + 12));
                const int n = (int) u32 (b + pos + 16);
                pos += 20;
                if (pos + (size_t) n * 2 > size) return;
                t.count = std::min (n, kMaxHarm);
                for (int k = 0; k < t.count; ++k)
                {
                    t.amp[(size_t) k]   = std::pow (10.0f, -0.5f * (float) b[pos + (size_t) k] / 20.0f);
                    t.phase[(size_t) k] = (float) b[pos + (size_t) n + (size_t) k] / 256.0f;     // in cycles
                }
                pos += (size_t) n * 2;
                // the upper envelope: each harmonic's level or its neighbours', whichever is higher
                for (int k = 0; k < t.count; ++k)
                {
                    float e = t.amp[(size_t) k];
                    if (k > 0)           e = std::max (e, t.amp[(size_t) k - 1]);
                    if (k + 1 < t.count) e = std::max (e, t.amp[(size_t) k + 1]);
                    t.env[(size_t) k] = std::max (1.0e-5f, e);
                }
                if (fs < 0 || fs >= nSets || fileToSet[(size_t) fs] < 0 || t.count < 1) continue;
                t.set = fileToSet[(size_t) fs];
                tables.push_back (t);
            }
            std::stable_sort (tables.begin(), tables.end(), [] (const Table& a, const Table& c) { return a.set < c.set; });
            for (int i = 0; i < (int) tables.size(); ++i)
            {
                auto& st = sets[(size_t) tables[(size_t) i].set];
                if (! st.present) { st.present = true; st.first = i; st.lo = st.hi = tables[(size_t) i].root; }
                st.count = i - st.first + 1;
                st.lo = std::min (st.lo, tables[(size_t) i].root);
                st.hi = std::max (st.hi, tables[(size_t) i].root);
            }
        }

        std::vector<Set> sets = std::vector<Set> ((size_t) kNumToneSets);
        std::vector<Table> tables;
    };

    //==========================================================================
    //  One voice's layer: its cycle, built at the note, and its oscillators.
    //==========================================================================
    class ToneLayer
    {
    public:
        static constexpr int kN = 2048;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            static const auto sine = []
            {
                std::array<float, kN> s {};
                for (int i = 0; i < kN; ++i) s[(size_t) i] = (float) std::sin (dsp::kTwoPi * (double) i / (double) kN);
                return s;
            }();
            sinTab = &sine;
            envUp   = 1.0f - (float) std::exp (-1.0 / (0.003 * sr));
            envDown = 1.0f - (float) std::exp (-1.0 / (0.040 * sr));
            active = false;
        }

        bool isActive() const { return active; }
        void stop() { active = false; }

        /** Builds the note's cycle.  morph 0 soft .. 1 loud; formant: how far the formants
            move (1 = where the instrument has them); fresh: a new note (else a slur). */
        void start (int setIndex, float hz, float morph, float formant, bool fresh)
        {
            const auto& lib = ToneLibrary::get();
            if (! lib.ok() || setIndex < 0 || setIndex >= kNumToneSets || ! lib.set (setIndex).present) { active = false; return; }
            const int soft = lib.nearest (setIndex, hz, 0), loud = lib.nearest (setIndex, hz, -1);
            if (soft < 0 || loud < 0) { active = false; return; }

            // band-limited to the note, with room for a bend and vibrato
            const int kmax = std::clamp ((int) (std::min (0.45 * sr, 18000.0) / (hz * 1.2)), 1, ToneLibrary::kMaxHarm);
            std::array<float, ToneLibrary::kMaxHarm> a {}, ph {};
            const auto& ts = lib.table (soft);
            const auto& tl = lib.table (loud);
            float power = 0.0f;
            for (int k = 1; k <= kmax; ++k)
            {
                const float as = level (ts, k, hz, formant), al = level (tl, k, hz, formant);
                a[(size_t) k - 1] = as + (al - as) * morph;
                const auto& tp = morph < 0.5f ? ts : tl;
                ph[(size_t) k - 1] = k <= tp.count ? tp.phase[(size_t) k - 1] : (float) ((k * 37) % 64) / 64.0f;
                power += a[(size_t) k - 1] * a[(size_t) k - 1];
            }
            const float norm = power > 1.0e-12f ? 1.0f / std::sqrt (0.5f * power) : 0.0f;   // the cycle's RMS is 1

            // the waveform shifted so its fundamental starts at phase 0 (a sine): the lock below
            // lines that fundamental up with the model's
            const float p1 = ph[0];
            for (int k = 1; k <= kmax; ++k)
            {
                float q = ph[(size_t) k - 1] - (float) k * p1;
                ph[(size_t) k - 1] = q - std::floor (q);
            }

            std::fill (cycle.begin(), cycle.end(), 0.0f);
            const auto& s = *sinTab;
            for (int k = 1; k <= kmax; ++k)
            {
                const float g = a[(size_t) k - 1] * norm;
                if (g < 1.0e-5f) continue;
                const int off = (int) (ph[(size_t) k - 1] * (float) kN) & (kN - 1);
                for (int n = 0; n < kN; ++n)
                    cycle[(size_t) n] += g * s[(size_t) ((k * n + off) & (kN - 1))];
            }
            cycle[kN] = cycle[0];

            if (fresh || ! active) { ph1 = 0.0; phSub = 0.0; ms = 0.0f; zc = zs = 0.0f; }
            zA = (float) std::min (1.0, (double) hz / (4.0 * sr));                         // four periods
            lockK = (float) (1.0 / (std::max (0.030, 12.0 / (double) hz) * sr));          // 30 ms, or twelve periods
            active = true;
        }

        /** One sample: the layer and its sub under a model whose sample is modelMid.  lock: keep
            the layer's fundamental in step with the model's, so the two add up (the depth)
            instead of cancelling wherever their phases happen to fall. */
        float tick (float modelMid, float hz, float mixGain, float subGain, bool lock)
        {
            const float x2 = modelMid * modelMid;
            ms += (x2 - ms) * (x2 > ms ? envUp : envDown);
            const float env = std::sqrt (ms);

            if (lock)
            {
                const auto& s = *sinTab;
                const int i0 = (int) (ph1 * (double) kN) & (kN - 1);
                const float sn = s[(size_t) i0], cs = s[(size_t) ((i0 + kN / 4) & (kN - 1))];
                zs += (modelMid * sn - zs) * zA;
                zc += (modelMid * cs - zc) * zA;
                const float mag = std::sqrt (zs * zs + zc * zc);
                if (mag > 0.05f * env && mag > 1.0e-7f)
                {
                    const float err = std::atan2 (zc, zs) * (1.0f / 6.2831853f);          // cycles the model's fundamental is ahead
                    ph1 += (double) (err * lockK);
                    phSub += (double) (err * lockK * 0.5f);
                    if (ph1 < 0.0) ph1 += 1.0;
                    if (phSub < 0.0) phSub += 1.0;
                }
            }

            const double inc = (double) hz / sr;
            const double p = ph1 * (double) kN;
            const int i = std::min ((int) p, kN - 1);
            const float f = (float) (p - (double) i);
            const float y = cycle[(size_t) i] + (cycle[(size_t) i + 1] - cycle[(size_t) i]) * f;
            ph1 += inc;
            while (ph1 >= 1.0) ph1 -= 1.0;

            float sub = 0.0f;
            if (subGain > 0.0f)
            {
                phSub += inc * 0.5;
                while (phSub >= 1.0) phSub -= 1.0;
                const double q = phSub * (double) kN;
                const int j = (int) q;
                const auto& s = *sinTab;
                sub = s[(size_t) j & (kN - 1)] * 1.4142f;
            }
            return env * (y * mixGain + sub * subGain);
        }

    private:
        /** A table's level for harmonic k of a note at hz: its own harmonic k (the pattern),
            moved along its envelope to where that harmonic falls now (the formants stay put,
            or move by formant). */
        static float level (const ToneLibrary::Table& t, int k, float hz, float formant)
        {
            const float jNow = (float) k * hz / (t.root * formant);   // where harmonic k sits on the table's envelope
            const float eNow = envAt (t, jNow);
            if (k <= t.count)
                return t.amp[(size_t) k - 1] * eNow / t.env[(size_t) k - 1];
            return 0.5f * eNow;
        }

        static float envAt (const ToneLibrary::Table& t, float j)
        {
            if (j <= 1.0f) return t.env[0];
            if (j >= (float) t.count)
                return t.env[(size_t) t.count - 1] * std::pow (j / (float) t.count, -3.0f);   // past the table: falling fast
            const int i = (int) j;
            const float f = j - (float) i;
            const float a = std::log (t.env[(size_t) i - 1]), c = std::log (t.env[(size_t) i]);
            return std::exp (a + (c - a) * f);
        }

        double sr = 48000.0, ph1 = 0.0, phSub = 0.0;
        float ms = 0.0f, envUp = 0.01f, envDown = 0.001f;
        float zs = 0.0f, zc = 0.0f, zA = 0.01f, lockK = 0.0001f;
        bool active = false;
        const std::array<float, kN>* sinTab = nullptr;
        std::array<float, kN + 1> cycle {};
    };
}
