// C:\workspace\SONIFY PLUGIN zax\src\ZaxFactoryKits.h
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "ZaxKit.h"
#include "TomPrototypes.h"

//==============================================================================
//  ZaxFactoryKits.h  —  the fifteen kits Zax ships with.
//
//  Every kit fills EVERY GM key 27..84 with a sound of that key's role
//  (ZaxGm.h): the formation never moves, only the sounds and their shaping
//  change from kit to kit.  Each kit is voiced for its genre - nothing here
//  copies a hardware kit.
//
//  On first run these kits are written as .zaxkit files into Zax's Factory
//  Kits folder (ZaxKitFiles.h); from then on the FILE is the kit, so a kit
//  reshaped and saved in the editor stays as saved.
//
//  A put() naming a sound outside the key's role is collected in `problems`
//  (the build test fails on any) and the key keeps its role's default.
//==============================================================================

namespace zax
{
    class KitBuilder
    {
    public:
        KitBuilder (const char* name, std::vector<std::string>* problemsOut)
            : problems (problemsOut)
        {
            fillRoleDefaults (kit);
            kit.name     = name;
            kit.volumeDb = -3.0f;            // headroom when many keys ring together
        }

        // A key's sound, with its tune (semitones), decay (0..1, 0.5 = as designed)
        // and gain (dB).  Every other macro starts neutral.
        KitBuilder& put (int key, const char* sound, float tune = 0.0f, float decay = 0.5f, float gainDb = 0.0f)
        {
            const int idx = findForKey (key, sound);
            if (idx < 0)
            {
                if (problems != nullptr)
                    problems->push_back (kit.name + " key " + std::to_string (key) + ": " + sound);
                return *this;
            }
            auto& c  = kit.cells[(size_t) key];
            const int choke = c.chokeGroup;
            c = Cell();
            c.sound            = edm::library()[(size_t) idx].sound;
            c.macros.tuneSemis = tune;
            c.macros.decay     = decay;
            c.gainDb           = gainDb;
            c.chokeGroup       = choke;
            return *this;
        }

        // Shape one key further: any macro, the filter, the gain
        KitBuilder& with (int key, const std::function<void (Cell&)>& fn)
        {
            if (isZaxKey (key)) fn (kit.cells[(size_t) key]);
            return *this;
        }

        // The same tweak on every key of a family
        KitBuilder& family (int fam, const std::function<void (Cell&)>& fn)
        {
            for (int k = kFirstKey; k <= kLastKey; ++k)
                if (roleFamily (k) == fam) fn (kit.cells[(size_t) k]);
            return *this;
        }

        // The same tweak on every key
        KitBuilder& all (const std::function<void (Cell&)>& fn)
        {
            for (int k = kFirstKey; k <= kLastKey; ++k) fn (kit.cells[(size_t) k]);
            return *this;
        }

        // 42 closed, 44 pedal, 46 open: one hi-hat
        KitBuilder& hats (const char* sound, float tune = 0.0f, float openDecay = 0.5f)
        {
            put (42, sound, tune);
            put (44, sound, tune);
            put (46, sound, tune, openDecay);
            return *this;
        }

        // The six toms on their GM keys, one prototype up the tom ladder
        KitBuilder& toms (const char* sound, float tune = 0.0f, float decay = 0.5f)
        {
            static const int keys[6] = { 41, 43, 45, 47, 48, 50 };
            for (int i = 0; i < 6; ++i)
                put (keys[i], sound, tom::kTomLadderSemis[i] + tune, decay);
            return *this;
        }

        KitBuilder& cymbals (const char* crash1, const char* crash2, const char* ride1, const char* ride2,
                             const char* china = "Trash Crash", const char* splash = "Splash")
        {
            put (49, crash1);
            put (57, crash2, -1.0f);
            put (51, ride1);
            put (59, ride2);
            put (52, china);
            put (55, splash);
            put (53, "Ride Bell");
            return *this;
        }

        // The GM percussion 54..84 and the GS extras 27..34.  analog: the
        // 808-style cowbell, congas, maracas and claves.
        KitBuilder& percussion (bool analog)
        {
            put (54, "Tambourine");
            put (56, analog ? "808 Cowbell" : "Cowbell");
            put (58, "Vibraslap");
            put (60, "Bongo High");     put (61, "Bongo Low");
            put (62, analog ? "808 Conga Hi"  : "Conga Mute");
            put (63, analog ? "808 Conga Mid" : "Conga Open");
            put (64, analog ? "808 Conga Lo"  : "Conga Low");
            put (65, "Timbale High");   put (66, "Timbale Low");
            put (67, "Agogo High");     put (68, "Agogo Low");
            put (69, "Cabasa");
            put (70, analog ? "808 Maracas" : "Maracas");
            put (71, "Whistle Short");  put (72, "Whistle Long");
            put (73, "Guiro Short");    put (74, "Guiro Long");
            put (75, analog ? "808 Claves" : "Claves");
            put (76, "Woodblock High"); put (77, "Woodblock Low");
            put (78, "Cuica Mute");     put (79, "Cuica Open");
            put (80, "Triangle Mute");  put (81, "Triangle Open");
            put (82, "Shaker");
            put (83, "Jingle Bell");
            put (84, "Bell Tree");

            put (27, "Hi Q");
            put (29, "Scratch Push");
            put (30, "Scratch Pull");
            put (31, "Woodblock High", 7.0f, 0.25f);            // sticks: a short, high click of wood
            put (32, "Blip High");                                // square click
            put (33, "Noise Click");                              // metronome click
            put (34, "Agogo High", 3.0f, 0.3f);                   // metronome bell
            return *this;
        }

        KitBuilder& fx (int fam, const std::function<void (FamilyFx&)>& fn)
        {
            if (fam >= 0 && fam < kNumFamilies) fn (kit.fx[(size_t) fam]);
            return *this;
        }

        KitBuilder& volume (float db)   { kit.volumeDb = db; return *this; }

        // The level trims measured across the library: a few sounds sit a
        // little under the rest of their role at the same velocity.
        Kit& build()
        {
            static const std::pair<const char*, float> trims[] = { { "Lo-fi Snare", 3.0f }, { "Lo-fi Clap", 3.0f } };
            for (int k = kFirstKey; k <= kLastKey; ++k)
                for (const auto& [name, db] : trims)
                    if (std::string (kit.cells[(size_t) k].sound.name()) == name)
                        kit.cells[(size_t) k].gainDb += db;
            return kit;
        }

    private:
        std::unique_ptr<Kit> owned = std::make_unique<Kit>();   // ~45 KB: the heap, never the stack
        Kit& kit = *owned;
        std::vector<std::string>* problems = nullptr;
    };

    //==========================================================================
    //  Little helpers for the kit voicing below
    //==========================================================================
    namespace voice
    {
        inline auto drive (float d)  { return [d] (Cell& c) { c.macros.drive = d; }; }
        inline auto color (float v)  { return [v] (Cell& c) { c.macros.color = v; }; }
        inline auto snap (float v)   { return [v] (Cell& c) { c.macros.snap  = v; }; }
        inline auto damp (float v)   { return [v] (Cell& c) { c.macros.damp  = v; }; }
        inline auto decay (float v)  { return [v] (Cell& c) { c.macros.decay = v; }; }
        inline auto gain (float db)  { return [db] (Cell& c) { c.gainDb += db; }; }
        inline auto lowCut (float n) { return [n] (Cell& c) { c.filterHpNorm = n; }; }
        inline auto highCut (float n){ return [n] (Cell& c) { c.filterLpNorm = n; }; }

        inline void room (FamilyFx& f, float size, float decay, float mix, float damp = 0.5f)
        {
            f.reverbOn = true; f.revSize = size; f.revDecay = decay; f.revMix = mix; f.revDamp = damp;
            f.revLowCutHz = 200.0f; f.revHighCutHz = 11000.0f;
        }
        inline void glue (FamilyFx& f, float threshDb, float ratio, float makeupDb, float attackMs = 10.0f, float releaseMs = 90.0f)
        {
            f.compOn = true; f.compThreshDb = threshDb; f.compRatio = ratio; f.compMakeupDb = makeupDb;
            f.compAttackMs = attackMs; f.compReleaseMs = releaseMs;
        }
        inline void crunch (FamilyFx& f, float driveAmt, float mix) { f.satOn = true; f.satDrive = driveAmt; f.satMix = mix; }
        inline void echo (FamilyFx& f, int note, float feedback, float mix)
        {
            f.delayOn = true; f.delayNote = (float) note; f.delayFeedback = feedback; f.delayMix = mix;
            f.delayDampHz = 4500.0f; f.delayLowCutHz = 300.0f;
        }
    }

    //==========================================================================
    inline std::vector<Kit> buildFactoryKits (std::vector<std::string>* problems = nullptr)
    {
        using namespace voice;
        std::vector<Kit> kits;
        kits.reserve (15);

        // ---- TRANCE: a long, rounded kick, a wide clap in its own hall, offbeat open hats
        {
            KitBuilder b ("Trance", problems);
            b.percussion (false);
            b.put (36, "Deep Punch", 0.0f, 0.55f).put (35, "Big Room", -1.0f);
            b.put (38, "House Snare", 1.0f).put (40, "909 Snare", 2.0f, 0.45f);
            b.put (39, "Big Room Clap").put (28, "Snap Clap", 1.0f).put (37, "909 Rim");
            b.hats ("Techno Hat", 0.0f, 0.62f);
            b.toms ("Syn Tom", 0.0f, 0.55f);
            b.cymbals ("Big Room Crash", "909 Crash", "909 Ride", "Ping Ride");
            b.put (32, "Zap").put (27, "Laser Rise");
            b.family (famHat, color (0.6f));
            b.fx (famClap,  [] (FamilyFx& f) { room (f, 0.65f, 0.6f, 0.24f); });
            b.fx (famSnare, [] (FamilyFx& f) { room (f, 0.55f, 0.5f, 0.16f); });
            b.fx (famCrash, [] (FamilyFx& f) { room (f, 0.7f, 0.6f, 0.12f); });
            b.fx (famTom,   [] (FamilyFx& f) { echo (f, 7, 0.3f, 0.18f); });
            b.fx (famKick,  [] (FamilyFx& f) { glue (f, -10.0f, 3.0f, 2.0f, 15.0f, 80.0f); });
            kits.push_back (std::move (b.build()));
        }

        // ---- HOUSE: the rounded 909 kick, a short clap with a little room, swinging hats
        {
            KitBuilder b ("House", problems);
            b.percussion (false);
            b.put (36, "909 Round").put (35, "Deep House");
            b.put (38, "House Snare").put (40, "909 Snare", 1.0f);
            b.put (39, "House Clap").put (28, "Tight Clap").put (37, "909 Rim");
            b.hats ("House Hat", 0.0f, 0.5f);
            b.toms ("Electro Tom");
            b.cymbals ("909 Crash", "Big Room Crash", "909 Ride", "Ping Ride");
            b.family (famShaker, decay (0.45f));
            b.fx (famClap,  [] (FamilyFx& f) { room (f, 0.45f, 0.45f, 0.18f); });
            b.fx (famHat,   [] (FamilyFx& f) { f.pan = 0.12f; });
            b.fx (famShaker,[] (FamilyFx& f) { f.pan = -0.2f; });
            kits.push_back (std::move (b.build()));
        }

        // ---- TECHNO: a driven 909 kick, dry snare, dark rumbling room on the clap
        {
            KitBuilder b ("Techno", problems);
            b.percussion (false);
            b.put (36, "909 Classic", -1.0f, 0.6f).put (35, "Hardstyle", -3.0f, 0.45f);
            b.put (38, "909 Snare", 0.0f, 0.4f).put (40, "Electro Snap");
            b.put (39, "909 Clap").put (28, "Snap Clap").put (37, "909 Rim", -1.0f);
            b.hats ("Techno Hat", 0.0f, 0.55f);
            b.toms ("909 Tom", -2.0f);
            b.cymbals ("909 Crash", "Dark Crash", "909 Ride", "Dark Ride");
            b.with (36, drive (0.35f));
            b.family (famRide, color (0.42f));
            b.fx (famKick,  [] (FamilyFx& f) { crunch (f, 0.25f, 0.6f); });
            b.fx (famClap,  [] (FamilyFx& f) { room (f, 0.7f, 0.65f, 0.22f, 0.75f); });
            b.fx (famSideStick, [] (FamilyFx& f) { echo (f, 7, 0.4f, 0.22f); });
            kits.push_back (std::move (b.build()));
        }

        // ---- BIG ROOM: a huge produced kick, big claps and snares in a big hall
        {
            KitBuilder b ("Big Room", problems);
            b.percussion (false);
            b.put (36, "Big Room").put (35, "Hardstyle", -2.0f);
            b.put (38, "House Snare", 0.0f, 0.55f).put (40, "Electro Snap", -1.0f);
            b.put (39, "Big Room Clap", 0.0f, 0.55f).put (28, "Reverb Clap").put (37, "909 Rim");
            b.hats ("909 Hat");
            b.toms ("Syn Tom", -2.0f, 0.6f);
            b.cymbals ("Big Room Crash", "909 Crash", "909 Ride", "Ping Ride");
            b.put (27, "Laser Rise").put (32, "Zap");
            b.fx (famClap,  [] (FamilyFx& f) { room (f, 0.85f, 0.65f, 0.3f); });
            b.fx (famSnare, [] (FamilyFx& f) { room (f, 0.8f, 0.6f, 0.24f); });
            b.fx (famTom,   [] (FamilyFx& f) { room (f, 0.7f, 0.5f, 0.18f); });
            b.fx (famKick,  [] (FamilyFx& f) { glue (f, -12.0f, 4.0f, 3.0f, 20.0f, 70.0f); });
            kits.push_back (std::move (b.build()));
        }

        // ---- HARDSTYLE: the distorted kick, hard snares, short metallic hats
        {
            KitBuilder b ("Hardstyle", problems);
            b.percussion (false);
            b.put (36, "Hardstyle", 0.0f, 0.55f).put (35, "Big Room", -1.0f);
            b.put (38, "Electro Snap").put (40, "House Snare", 1.0f);
            b.put (39, "Big Room Clap").put (28, "Snap Clap").put (37, "909 Rim");
            b.hats ("Techno Hat", 1.0f, 0.45f);
            b.toms ("Syn Tom", -1.0f);
            b.cymbals ("Big Room Crash", "Trash Crash", "909 Ride", "Trash Ride");
            b.with (36, drive (0.55f));
            b.fx (famKick,  [] (FamilyFx& f) { crunch (f, 0.45f, 0.7f); glue (f, -10.0f, 4.0f, 2.0f, 5.0f, 60.0f); });
            b.fx (famClap,  [] (FamilyFx& f) { room (f, 0.75f, 0.55f, 0.22f); });
            b.fx (famSnare, [] (FamilyFx& f) { crunch (f, 0.2f, 0.5f); });
            kits.push_back (std::move (b.build()));
        }

        // ---- DRUM & BASS: a tight short kick, cracking high snares, bright fast hats
        {
            KitBuilder b ("Drum & Bass", problems);
            b.percussion (false);
            b.put (36, "Tight Pop", 0.0f, 0.38f).put (35, "Beater Head", 1.0f, 0.4f);
            b.put (38, "Rimshot Crack", 3.0f, 0.42f).put (40, "Tight Funk", 3.0f);
            b.put (39, "Real Clap Small").put (28, "Snap Clap", 2.0f).put (37, "Acoustic Cross Stick", 2.0f);
            b.hats ("Acoustic Bright", 1.0f, 0.45f);
            b.toms ("Tight Pop Toms", 2.0f);
            b.cymbals ("Bright Crash", "Splash", "Rock Ride", "Ping Ride");
            b.with (36, snap (0.7f)).with (38, snap (0.65f));
            b.fx (famSnare, [] (FamilyFx& f) { glue (f, -14.0f, 4.0f, 3.0f, 2.0f, 60.0f); room (f, 0.35f, 0.35f, 0.12f); });
            b.fx (famKick,  [] (FamilyFx& f) { glue (f, -12.0f, 3.0f, 2.0f, 4.0f, 50.0f); });
            b.fx (famHat,   [] (FamilyFx& f) { f.pan = 0.15f; });
            kits.push_back (std::move (b.build()));
        }

        // ---- DUBSTEP: a long diving kick, a huge pitched-down snare, trashy metal
        {
            KitBuilder b ("Dubstep", problems);
            b.percussion (false);
            b.put (36, "Long Dive").put (35, "Trap Boom");
            b.put (38, "Electro Snap", -2.0f, 0.62f).put (40, "Trap Snare");
            b.put (39, "Big Room Clap", -1.0f).put (28, "Snap Clap").put (37, "909 Rim", -2.0f);
            b.hats ("Techno Hat", -1.0f, 0.5f);
            b.toms ("Syn Tom", -3.0f, 0.6f);
            b.cymbals ("Big Room Crash", "Trash Crash", "Trash Ride", "Ping Ride", "Trash Crash", "Splash");
            b.put (32, "Zap Low").put (27, "Noise Sweep");
            b.fx (famSnare, [] (FamilyFx& f) { room (f, 0.8f, 0.65f, 0.28f); crunch (f, 0.18f, 0.5f); });
            b.fx (famClap,  [] (FamilyFx& f) { room (f, 0.8f, 0.6f, 0.25f); });
            b.fx (famKick,  [] (FamilyFx& f) { glue (f, -10.0f, 4.0f, 2.0f, 10.0f, 80.0f); });
            kits.push_back (std::move (b.build()));
        }

        // ---- TRAP: the sub-drop kick, the 808 long boom, crisp trap snares and hats
        {
            KitBuilder b ("Trap", problems);
            b.percussion (true);
            b.put (36, "Sub Drop").put (35, "808 Long Boom", -2.0f, 0.65f);
            b.put (38, "Trap Snare").put (40, "808 Snare", 2.0f);
            b.put (39, "Trap Clap").put (28, "Snap Clap").put (37, "808 Rimshot");
            b.hats ("Trap Hat", 0.0f, 0.45f);
            b.toms ("808 Tom", 0.0f, 0.6f);
            b.cymbals ("808 Cymbal", "Dark Crash", "808 Ride", "Ping Ride");
            b.fx (famClap,  [] (FamilyFx& f) { room (f, 0.5f, 0.45f, 0.18f); });
            b.fx (famHat,   [] (FamilyFx& f) { f.pan = -0.1f; });
            kits.push_back (std::move (b.build()));
        }

        // ---- HIP HOP: a warm boom-bap kick, a fat dusty snare, tight vintage hats
        {
            KitBuilder b ("Hip Hop", problems);
            b.percussion (false);
            b.put (36, "Vintage R&B", 0.0f, 0.55f).put (35, "808 Sub");
            b.put (38, "Tight Funk", -1.0f).put (40, "Lo-fi Snare");
            b.put (39, "Lo-fi Clap").put (28, "Snap Clap", -1.0f).put (37, "Woody Click");
            b.hats ("Vintage Tight");
            b.toms ("Lo-fi Tom");
            b.cymbals ("Dark Crash", "Lo-fi Crash", "Dark Ride", "Jazz Ride");
            b.family (famSnare, color (0.42f));
            b.fx (famSnare, [] (FamilyFx& f) { crunch (f, 0.22f, 0.55f); glue (f, -16.0f, 3.0f, 3.0f, 8.0f, 120.0f); });
            b.fx (famKick,  [] (FamilyFx& f) { crunch (f, 0.15f, 0.5f); });
            kits.push_back (std::move (b.build()));
        }

        // ---- LO-FI: crunchy, dark and dusty everywhere
        {
            KitBuilder b ("Lo-fi", problems);
            b.percussion (false);
            b.put (36, "Lo-fi Crunch").put (35, "Vintage R&B", -2.0f);
            b.put (38, "Lo-fi Snare").put (40, "Tight Funk", -2.0f);
            b.put (39, "Lo-fi Clap").put (28, "Real Clap Small", -2.0f).put (37, "Soft Cross Stick");
            b.hats ("Lo-fi Hat");
            b.toms ("Lo-fi Tom");
            b.cymbals ("Lo-fi Crash", "Dark Crash", "Dark Ride", "Jazz Ride");
            b.all (color (0.35f));
            b.all (highCut (0.86f));                         // about 7.5 kHz: the dust on top is gone
            for (int f = 0; f < kNumFamilies; ++f)
                b.fx (f, [] (FamilyFx& fx) { crunch (fx, 0.2f, 0.45f); });
            b.fx (famSnare, [] (FamilyFx& f) { room (f, 0.35f, 0.4f, 0.14f, 0.8f); });
            kits.push_back (std::move (b.build()));
        }

        // ---- SYNTHWAVE: big 80s drums - gated-sounding snare in a hall, power toms
        {
            KitBuilder b ("Synthwave", problems);
            b.percussion (false);
            b.put (36, "Beater Head").put (35, "Acoustic Rock 22");
            b.put (38, "Rimshot Crack", 0.0f, 0.45f).put (40, "Acoustic Rock");
            b.put (39, "Reverb Clap").put (28, "Real Clap Small").put (37, "Bright Cross Stick");
            b.hats ("Vintage Tight");
            b.toms ("Power Toms", 0.0f, 0.55f);
            b.cymbals ("Bright Crash", "Dark Crash", "Rock Ride", "Ping Ride");
            b.fx (famSnare, [] (FamilyFx& f) { room (f, 0.75f, 0.35f, 0.32f); glue (f, -18.0f, 6.0f, 4.0f, 1.0f, 140.0f); });
            b.fx (famTom,   [] (FamilyFx& f) { room (f, 0.7f, 0.4f, 0.24f); });
            b.fx (famClap,  [] (FamilyFx& f) { room (f, 0.6f, 0.45f, 0.18f); });
            b.fx (famHat,   [] (FamilyFx& f) { f.chorusOn = true; f.chorusRate = 0.25f; f.chorusDepth = 0.4f; f.chorusMix = 0.3f; });
            kits.push_back (std::move (b.build()));
        }

        // ---- ELECTRO: punchy 909 kick, snapping snares, 808 metal and congas
        {
            KitBuilder b ("Electro", problems);
            b.percussion (true);
            b.put (36, "909 Punch").put (35, "808 Sub");
            b.put (38, "Electro Snap").put (40, "909 Snare");
            b.put (39, "909 Clap").put (28, "808 Clap").put (37, "909 Rim");
            b.hats ("808 Hat");
            b.toms ("Electro Tom");
            b.cymbals ("909 Crash", "808 Cymbal", "909 Ride", "808 Ride");
            b.put (32, "Blip Low").put (27, "Zap");
            b.fx (famClap,  [] (FamilyFx& f) { echo (f, 7, 0.3f, 0.16f); });
            b.fx (famHandDrum, [] (FamilyFx& f) { echo (f, 4, 0.25f, 0.15f); f.pan = 0.2f; });
            kits.push_back (std::move (b.build()));
        }

        // ---- BREAKBEAT: real-drum character, crunched and compressed like a sampled break
        {
            KitBuilder b ("Breakbeat", problems);
            b.percussion (false);
            b.put (36, "Acoustic Rock 22", 0.0f, 0.45f).put (35, "Beater Head");
            b.put (38, "Acoustic Rock").put (40, "Lo-fi Snare", 1.0f);
            b.put (39, "Real Clap Small").put (28, "Lo-fi Clap").put (37, "Acoustic Cross Stick");
            b.hats ("Acoustic Bright");
            b.toms ("Acoustic Rock Toms");
            b.cymbals ("Bright Crash", "Dark Crash", "Rock Ride", "Jazz Ride");
            for (int f : { (int) famKick, (int) famSnare, (int) famTom })
                b.fx (f, [] (FamilyFx& fx) { crunch (fx, 0.3f, 0.5f); glue (fx, -18.0f, 4.0f, 5.0f, 3.0f, 70.0f); });
            b.fx (famSnare, [] (FamilyFx& f) { room (f, 0.4f, 0.35f, 0.14f); });
            kits.push_back (std::move (b.build()));
        }

        // ---- CLASSIC 808: the long boom, the 808 snare, clap, metal and congas
        {
            KitBuilder b ("Classic 808", problems);
            b.percussion (true);
            b.put (36, "808 Long Boom").put (35, "808 Sub");
            b.put (38, "808 Snare").put (40, "808 Snare", 2.0f, 0.4f);
            b.put (39, "808 Clap").put (28, "808 Clap", 2.0f).put (37, "808 Rimshot");
            b.hats ("808 Hat");
            b.toms ("808 Tom");
            b.cymbals ("808 Cymbal", "808 Cymbal", "808 Ride", "808 Ride");
            b.put (32, "Blip High").put (27, "Hi Q");
            kits.push_back (std::move (b.build()));
        }

        // ---- CLASSIC 909: the 909 kick, snare, clap, hats, toms and cymbals
        {
            KitBuilder b ("Classic 909", problems);
            b.percussion (false);
            b.put (36, "909 Classic").put (35, "909 Punch");
            b.put (38, "909 Snare").put (40, "909 Snare", 1.0f, 0.6f);
            b.put (39, "909 Clap").put (28, "909 Clap", 2.0f).put (37, "909 Rim");
            b.hats ("909 Hat");
            b.toms ("909 Tom");
            b.cymbals ("909 Crash", "909 Crash", "909 Ride", "909 Ride");
            kits.push_back (std::move (b.build()));
        }

        return kits;
    }

    inline const std::vector<Kit>& factoryKits()
    {
        static const std::vector<Kit> kits = buildFactoryKits();
        return kits;
    }

    inline const Kit* findFactoryKit (const std::string& name)
    {
        for (const auto& k : factoryKits())
            if (k.name == name) return &k;
        return nullptr;
    }
}
