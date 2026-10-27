// C:\workspace\SONIFY PLUGIN zax\src\ZaxGm.h
#pragma once

#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include "SynthLibrary.h"

//==============================================================================
//  ZaxGm.h  —  the GM formation Zax is built on.
//
//  Zax plays keys 27..84: the GM Level 1 drum map (35..81) plus the GS extras
//  below (27..34) and above (82..84).  Every key has a fixed ROLE taken from its
//  GM name - 35 and 36 are kicks, 42 is the closed hi-hat, 56 the cowbell - and
//  the role is the key's root:
//
//      * the key's SOUND menu lists only the sounds of its role's family
//        (a kick key shows kicks only, 42 shows closed hats only)
//      * the key always plays through its role family's effect rack
//      * nobody can change it: no family selector, no silent "other" sound
//
//  Roles are library families (SynthLibrary.h).  The three hi-hat keys add the
//  articulation: 42 closed, 44 pedal, 46 open.
//
//  Default choke groups follow the GM exclusive classes: the hats, the two
//  whistles, the two guiros, the two cuicas and the two triangles.
//
//  Header-only, no JUCE: the engine, the kit files and the editor all read it.
//==============================================================================

namespace zax
{
    static constexpr int kFirstKey = 27;
    static constexpr int kLastKey  = 84;
    static constexpr int kNumKeys  = kLastKey - kFirstKey + 1;      // 58

    // The fifteen library families, in the engine's order.  A key's role is one
    // of them; each has its own effect rack.
    enum Family : int
    {
        famKick = 0, famTom, famSnare, famSideStick, famClap, famHat, famCrash, famRide,
        famHandDrum, famShaker, famWood, famScrape, famMetal, famWhistle, famFx,
        kNumFamilies
    };

    inline const char* familyName (int f) noexcept
    {
        static const char* n[kNumFamilies] = { "Kick", "Tom", "Snare", "Side Stick", "Clap", "Hat", "Crash", "Ride",
                                               "Hand Drum", "Shaker", "Wood", "Scrape", "Metal", "Whistle", "FX" };
        return (f >= 0 && f < kNumFamilies) ? n[f] : "";
    }

    inline int familyIndex (const char* name) noexcept
    {
        for (int i = 0; i < kNumFamilies; ++i)
            if (std::strcmp (name, familyName (i)) == 0) return i;
        return -1;
    }

    // ARGB, one colour per family: key tints, labels and the FX page title
    inline unsigned int familyArgb (int f) noexcept
    {
        static const unsigned int c[kNumFamilies] = {
            0xFFB87A36, 0xFFC0623A, 0xFF3A8FB8, 0xFF3AB8A8, 0xFFB83A7A, 0xFF6AB83A, 0xFF8A5AC8, 0xFF5A7AC8,
            0xFFA8743A, 0xFF8AA83A, 0xFF9A6A48, 0xFF7A8A6A, 0xFFB8A83A, 0xFF3AA8B8, 0xFFC83A5A };
        return (f >= 0 && f < kNumFamilies) ? c[f] : 0xFF505050u;
    }

    //==========================================================================
    //  One GM key: its name, its role (family + hat articulation, -1 = any) and
    //  its default choke group.
    //==========================================================================
    struct GmKey
    {
        int         key;
        const char* name;
        int         family;
        int         articulation;   // hats: 0 closed, 1 pedal, 2 open; -1 = not a hat key
        int         choke;          // GM exclusive class, 0 = none
    };

    inline const GmKey (&gmKeys())[kNumKeys]
    {
        static const GmKey k[kNumKeys] =
        {
            { 27, "High Q",          famFx,        -1, 0 },
            { 28, "Slap",            famClap,      -1, 0 },
            { 29, "Scratch Push",    famFx,        -1, 0 },
            { 30, "Scratch Pull",    famFx,        -1, 0 },
            { 31, "Sticks",          famWood,      -1, 0 },
            { 32, "Square Click",    famFx,        -1, 0 },
            { 33, "Metronome Click", famFx,        -1, 0 },
            { 34, "Metronome Bell",  famMetal,     -1, 0 },
            { 35, "Bass Drum 2",     famKick,      -1, 0 },
            { 36, "Bass Drum 1",     famKick,      -1, 0 },
            { 37, "Side Stick",      famSideStick, -1, 0 },
            { 38, "Snare 1",         famSnare,     -1, 0 },
            { 39, "Hand Clap",       famClap,      -1, 0 },
            { 40, "Snare 2",         famSnare,     -1, 0 },
            { 41, "Low Floor Tom",   famTom,       -1, 0 },
            { 42, "Closed Hi-Hat",   famHat,        0, 1 },
            { 43, "High Floor Tom",  famTom,       -1, 0 },
            { 44, "Pedal Hi-Hat",    famHat,        1, 1 },
            { 45, "Low Tom",         famTom,       -1, 0 },
            { 46, "Open Hi-Hat",     famHat,        2, 1 },
            { 47, "Low-Mid Tom",     famTom,       -1, 0 },
            { 48, "High-Mid Tom",    famTom,       -1, 0 },
            { 49, "Crash 1",         famCrash,     -1, 0 },
            { 50, "High Tom",        famTom,       -1, 0 },
            { 51, "Ride 1",          famRide,      -1, 0 },
            { 52, "Chinese Cymbal",  famCrash,     -1, 0 },
            { 53, "Ride Bell",       famRide,      -1, 0 },
            { 54, "Tambourine",      famShaker,    -1, 0 },
            { 55, "Splash Cymbal",   famCrash,     -1, 0 },
            { 56, "Cowbell",         famMetal,     -1, 0 },
            { 57, "Crash 2",         famCrash,     -1, 0 },
            { 58, "Vibraslap",       famScrape,    -1, 0 },
            { 59, "Ride 2",          famRide,      -1, 0 },
            { 60, "High Bongo",      famHandDrum,  -1, 0 },
            { 61, "Low Bongo",       famHandDrum,  -1, 0 },
            { 62, "Mute High Conga", famHandDrum,  -1, 0 },
            { 63, "Open High Conga", famHandDrum,  -1, 0 },
            { 64, "Low Conga",       famHandDrum,  -1, 0 },
            { 65, "High Timbale",    famHandDrum,  -1, 0 },
            { 66, "Low Timbale",     famHandDrum,  -1, 0 },
            { 67, "High Agogo",      famMetal,     -1, 0 },
            { 68, "Low Agogo",       famMetal,     -1, 0 },
            { 69, "Cabasa",          famShaker,    -1, 0 },
            { 70, "Maracas",         famShaker,    -1, 0 },
            { 71, "Short Whistle",   famWhistle,   -1, 2 },
            { 72, "Long Whistle",    famWhistle,   -1, 2 },
            { 73, "Short Guiro",     famScrape,    -1, 3 },
            { 74, "Long Guiro",      famScrape,    -1, 3 },
            { 75, "Claves",          famWood,      -1, 0 },
            { 76, "High Wood Block", famWood,      -1, 0 },
            { 77, "Low Wood Block",  famWood,      -1, 0 },
            { 78, "Mute Cuica",      famHandDrum,  -1, 4 },
            { 79, "Open Cuica",      famHandDrum,  -1, 4 },
            { 80, "Mute Triangle",   famMetal,     -1, 5 },
            { 81, "Open Triangle",   famMetal,     -1, 5 },
            { 82, "Shaker",          famShaker,    -1, 0 },
            { 83, "Jingle Bell",     famMetal,     -1, 0 },
            { 84, "Bell Tree",       famMetal,     -1, 0 },
        };
        return k;
    }

    inline bool isZaxKey (int key) noexcept           { return key >= kFirstKey && key <= kLastKey; }

    // The GM entry of a key, or nullptr outside 27..84
    inline const GmKey* gm (int key) noexcept
    {
        return isZaxKey (key) ? &gmKeys()[key - kFirstKey] : nullptr;
    }

    inline int roleFamily (int key) noexcept           { const auto* g = gm (key); return g != nullptr ? g->family : -1; }

    // The role in capitals, for the editor's key header ("KICK", "CLOSED HAT")
    inline std::string roleName (int key)
    {
        const auto* g = gm (key);
        if (g == nullptr) return {};
        if (g->family == famHat)
            return g->articulation == 0 ? "CLOSED HAT" : (g->articulation == 1 ? "PEDAL HAT" : "OPEN HAT");
        std::string s = familyName (g->family);
        for (auto& ch : s) ch = (char) std::toupper ((unsigned char) ch);
        return s;
    }

    //==========================================================================
    //  Does library entry `idx` fit key `key`'s role?  The family must match;
    //  on a hat key, the articulation too.
    //==========================================================================
    inline bool fitsRole (int key, int libIndex)
    {
        const auto* g = gm (key);
        const auto& lib = edm::library();
        if (g == nullptr || libIndex < 0 || libIndex >= (int) lib.size()) return false;
        const auto& e = lib[(size_t) libIndex];
        if (familyIndex (e.family) != g->family) return false;
        if (g->articulation >= 0 && e.sound.articulation != g->articulation) return false;
        return true;
    }

    // Every library sound key `key` may play, in library order
    inline std::vector<int> soundsForKey (int key)
    {
        std::vector<int> out;
        const int n = (int) edm::library().size();
        for (int i = 0; i < n; ++i)
            if (fitsRole (key, i)) out.push_back (i);
        return out;
    }

    // Library index of a sound by name for a key's role (hat keys pick their
    // own articulation), or -1 when that name is not in the role.
    inline int findForKey (int key, const char* soundName)
    {
        const auto* g = gm (key);
        if (g == nullptr) return -1;
        const int art = g->articulation >= 0 ? g->articulation : 0;
        const int idx = edm::findSound (soundName, art);
        return fitsRole (key, idx) ? idx : -1;
    }

    // Yamaha naming (C3 = 60), as in Grex and Sonify
    inline std::string noteName (int key)
    {
        static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return std::string (n[((key % 12) + 12) % 12]) + std::to_string (key / 12 - 2);
    }

    inline bool isBlackKey (int key) noexcept
    {
        const int pc = ((key % 12) + 12) % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }
}
