// D:\workspace\Fanan_web_hub\src\zodiak\ZodiakParams.h
#pragma once

#include <array>
#include <cmath>
#include <cstring>
#include "../stella_api.h"

//==============================================================================
//  ZodiakParams.h  —  Zodiak's eight channels and what each one's strip holds.
//
//  The eight channels are alike: each one plays a VSTi (or several) loaded
//  from file, through its own GRAPH of effects (ZodiakGraph.h): Zodiak's own
//  nodes (vowel, filter, trance gate, sync delay, saturator, reverb with
//  Greyhole, the sidechain dynamic EQ, phaser, wah, feedback, sidechain in)
//  and VST FX plugins, wired in a serial line to begin with, rewired freely.
//  Then the mixer strip: volume, pan, mute, solo.  The master: its VST FX,
//  volume and balance.
//
//  Plain C++, no JUCE: the engine, the set files and the windows all read it.
//==============================================================================

namespace zodiak
{
    static constexpr int kNumChannels  = 8;
    static constexpr int kNumMacros    = 9;
    static constexpr int kVowelColumns = 8;
    static constexpr int kGateSteps    = 16;

    //==========================================================================
    //  Where the song is, at the start of a block (or of a part of one)
    //==========================================================================
    struct Transport
    {
        double sampleRate  = 48000.0;
        double bpm         = 120.0;
        double ppq         = 0.0;       // quarter notes since the song's start
        double barStart    = 0.0;
        bool   playing     = false;
        int    numerator   = 4, denominator = 4;

        double beatsPerSample() const noexcept   { return (bpm > 1.0 ? bpm : 120.0) / (60.0 * sampleRate); }

        Transport advancedBy (int samples) const noexcept
        {
            Transport t = *this;
            if (playing) t.ppq += beatsPerSample() * samples;
            return t;
        }
    };

    //==========================================================================
    //  The vowels, in the order the columns' selectors show them
    //==========================================================================
    static constexpr int kNumVowels = 5;
    inline const char* vowelName (int v) noexcept
    {
        static const char* n[kNumVowels] = { "A", "E", "O", "U", "I" };
        return (v >= 0 && v < kNumVowels) ? n[v] : "";
    }

    inline const char* vowelModeName (int m) noexcept
    {
        static const char* n[3] = { "LOOP", "PING-PONG", "ONCE" };
        return (m >= 0 && m < 3) ? n[m] : "";
    }

    // The channel filter: OFF, then the I3Q filter's 17 types
    inline const char* filterTypeName (int t) noexcept;

    // A tempo division by its index in stella::syncDivisions (4 bars .. 1/32)
    inline const char* divisionName (int i) noexcept
    {
        if (i < 0) i = 0;
        if (i >= stella::numSyncDivisions) i = stella::numSyncDivisions - 1;
        return stella::syncDivisions[i].name;
    }

    // Division index ranges: the vowel marker 1/16 .. 4 bars, the gate's steps
    // 1/32 .. 1 bar.  (The delay has its own list: kDelayNotes.)
    static constexpr int kDiv4Bars = 0, kDiv1Bar = 2, kDiv1_4 = 6, kDiv1_8 = 9, kDiv1_8Dotted = 10, kDiv1_16 = 12, kDiv1_32 = 15;

    //==========================================================================
    //  The delay's note values, SYNC on: longest to shortest, so the TIME
    //  slider only ever gets shorter on its way up.  (SYNC off, the time is
    //  cpDelayMs, in milliseconds, whatever the tempo.)
    //==========================================================================
    struct DelayNote { const char* name; float beats; };

    inline constexpr DelayNote kDelayNotes[] {
        { "1 bar", 4.0f }, { "1/2 dotted", 3.0f }, { "1/2", 2.0f }, { "1/4 dotted", 1.5f }, { "1/2 triplet", 4.0f / 3.0f },
        { "1/4", 1.0f }, { "1/8 dotted", 0.75f }, { "1/4 triplet", 2.0f / 3.0f }, { "1/8", 0.5f }, { "1/16 dotted", 0.375f },
        { "1/8 triplet", 1.0f / 3.0f }, { "1/16", 0.25f }, { "1/16 triplet", 1.0f / 6.0f }, { "1/32", 0.125f } };
    inline constexpr int kNumDelayNotes = (int) (sizeof (kDelayNotes) / sizeof (kDelayNotes[0]));
    static constexpr int kDelayNote1_8Dotted = 6, kDelayNote1_8 = 8;

    inline const DelayNote& delayNote (float index) noexcept
    {
        const int i = (int) (index + 0.5f);
        return kDelayNotes[i < 0 ? 0 : (i >= kNumDelayNotes ? kNumDelayNotes - 1 : i)];
    }

    //==========================================================================
    //  The channel parameters: the notes' PITCH and the mixer strip.  Everything
    //  else a channel does lives in its graph (ZodiakGraph.h), as node settings.
    //==========================================================================
    enum ChanParam : int
    {
        cpTranspose = 0, cpOctave,
        cpVolume, cpPan, cpMute, cpSolo,
        kNumChanParams
    };

    enum ParamKind : int { pkContinuous = 0, pkStepped, pkToggle, pkMask };

    struct ParamInfo
    {
        const char* id;          // the set file's name for it
        const char* name;        // shown
        const char* group;       // the page it is on
        float       min, max, def;
        const char* unit;
        float       skew;        // as stella::Param: 1 = linear, below 1 = more room at the bottom
        int         kind;
    };

    inline const ParamInfo& chanParamInfo (int p) noexcept
    {
        static const ParamInfo info[kNumChanParams] =
        {
            { "transpose",   "Transpose",  "PITCH",  -24.0f, 24.0f, 0.0f, "st", 1.0f, pkStepped },
            { "octave",      "Octave",     "PITCH",  -3.0f, 3.0f, 0.0f, "oct", 1.0f, pkStepped },
            { "volume",      "Volume",     "MIXER",  -60.0f, 6.0f, 0.0f, "dB", 2.0f, pkContinuous },
            { "pan",         "Pan",        "MIXER",  -1.0f, 1.0f, 0.0f, "", 1.0f, pkContinuous },
            { "mute",        "Mute",       "MIXER",  0.0f, 1.0f, 0.0f, "", 1.0f, pkToggle },
            { "solo",        "Solo",       "MIXER",  0.0f, 1.0f, 0.0f, "", 1.0f, pkToggle },
        };
        return info[(p >= 0 && p < kNumChanParams) ? p : 0];
    }

    // The gate's 16 steps as they ship: the classic trance chop
    static constexpr int kDefaultGateSteps = 0b1011101110111011;

    inline int chanParamIndex (const char* id) noexcept
    {
        for (int p = 0; p < kNumChanParams; ++p)
            if (std::strcmp (chanParamInfo (p).id, id) == 0) return p;
        return -1;
    }

    inline float chanParamDefault (int p) noexcept   { return chanParamInfo (p).def; }

    //==========================================================================
    //  0..1 and back, with the parameter's skew (stella's convention)
    //==========================================================================
    inline float toNorm (float v, float min, float max, float skew) noexcept
    {
        if (max <= min) return 0.0f;
        float t = (v - min) / (max - min);
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return skew == 1.0f ? t : std::pow (t, skew);
    }

    inline float fromNorm (float n, float min, float max, float skew) noexcept
    {
        n = n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
        const float t = skew == 1.0f ? n : std::pow (n, 1.0f / skew);
        return min + (max - min) * t;
    }

    //==========================================================================
    //  The filter's names (OFF, then I3Q's)
    //==========================================================================
    inline const char* filterTypeName (int t) noexcept
    {
        static const char* n[38] = { "OFF", "LP 12", "LP 24", "HP 12", "HP 24", "BP 12", "BP 24", "Notch", "Peak",
                                     "Low Shelf", "High Shelf", "Dirty LP", "Ladder", "Formant", "Comb +", "Comb -", "Phaser", "Acid",
                                     "ANA LP 12", "ANA LP 24", "ANA HP 24", "NEX LP 12", "NEX LP 24", "NEX HP 24",
                                     "POW LP 24", "POW HP", "COS LP 24", "COS HP 12", "PXL LP 24", "PXL BP",
                                     "SSM BP", "SSM BS", "IRS LP 12", "IRS LP 24",
                                     "Scream LP", "Scream HP", "Cry BP", "Rave 24" };
        return (t >= 0 && t < 38) ? n[t] : "";
    }
    //==========================================================================
    //  The master: its strip (volume, balance, mute; its VST FX are its rack),
    //  and the TRANSPORT's: its own tempo, and FOLLOW DAW (off by default:
    //  Zodiak plays from its own PLAY); LOW VELOCITY RESPONSE (Grex's: lifts the
    //  quiet notes, 0 = off).
    //==========================================================================
    enum MasterParam : int
    {
        mpVolume = 0, mpPan, mpMute,
        mpTempo, mpFollow,
        mpLowVel,
        kNumMasterParams
    };

    inline const ParamInfo& masterParamInfo (int p) noexcept
    {
        static const ParamInfo info[kNumMasterParams] =
        {
            { "volume",         "Master",       "MASTER", -60.0f, 6.0f, 0.0f, "dB", 2.0f, pkContinuous },
            { "pan",            "Balance",      "MASTER", -1.0f, 1.0f, 0.0f, "", 1.0f, pkContinuous },
            { "mute",           "Mute",         "MASTER", 0.0f, 1.0f, 0.0f, "", 1.0f, pkToggle },
            { "tempo",          "Tempo",        "TRANSPORT", 40.0f, 240.0f, 120.0f, "BPM", 1.0f, pkContinuous },   // its own (Follow DAW off)
            { "follow_daw",     "Follow DAW",   "TRANSPORT", 0.0f, 1.0f, 0.0f, "", 1.0f, pkToggle },           // on: plays with the DAW's play/stop
            { "low_velocity",   "Low Velocity Response", "VELOCITY", 0.0f, 100.0f, 0.0f, "%", 1.0f, pkStepped },   // 0: off (Grex's)
        };
        return info[(p >= 0 && p < kNumMasterParams) ? p : 0];
    }

    static constexpr int kNumReverbVoicings = 9;

    inline const char* reverbVoicingName (int v) noexcept
    {
        static const char* n[kNumReverbVoicings] = { "Plate", "Hall", "Room", "Chamber", "Cathedral", "Spring", "Vintage", "Shimmer", "Greyhole" };
        return (v >= 0 && v < kNumReverbVoicings) ? n[v] : "";
    }
}
