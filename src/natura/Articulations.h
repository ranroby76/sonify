// C:\workspace\SONIFY PLUGIN natura\src\Articulations.h
#pragma once
//==============================================================================
//  Articulations.h  -  what each archetype can be told to play, and how.
//
//  An articulation is either a NOTE articulation (it shapes the notes played
//  while it is selected: staccato, fall, pizzicato...) or a STATE articulation
//  (it flips something that lasts: rotor fast, motor off, sostenuto...).
//
//  Selection, in order of priority:
//      AUTO       overlapping notes -> Legato / Hammer-on, fast repeats -> Staccato
//      VELOCITY   below / above the split -> two chosen articulations
//      KEYSWITCH  a reserved octave of keys, or CC ZONES on a chosen CC
//      PANEL      the ARTIC tab (and its host parameter)
//==============================================================================
#include "Models.h"

namespace natura
{
    enum class Art
    {
        Normal, Legato, Detache, Staccato, Spiccato, Marcato, Sfz, Tremolo, Pizzicato, ColLegno,
        Harmonics, Trill, Fall, Doit, Scoop, Shake, Flutter, Growl, MuteStraight, MuteCup,
        MuteHarmon, MuteWah, Subtone, Overblow, PalmMute, Slide, HammerOn, Bend, DeadNote,
        Pedal, HalfPedal, Sostenuto, UnaCorda, DeadStroke, Roll, SoftMallet, HardMallet,
        MotorOn, MotorOff, RotorSlow, RotorFast, PercOn, PercOff, VibOn, VibOff,
        BellowsShake, RegisterUp, RegisterDown, GlideLegato, Retrigger, Sustain
    };

    constexpr int kMaxArts = 14;

    inline const char* artName (Art a)
    {
        switch (a)
        {
            case Art::Normal:       return "NORMAL";
            case Art::Legato:       return "LEGATO";
            case Art::Detache:      return "DETACHE";
            case Art::Staccato:     return "STACCATO";
            case Art::Spiccato:     return "SPICCATO";
            case Art::Marcato:      return "MARCATO";
            case Art::Sfz:          return "SFORZANDO";
            case Art::Tremolo:      return "TREMOLO";
            case Art::Pizzicato:    return "PIZZICATO";
            case Art::ColLegno:     return "COL LEGNO";
            case Art::Harmonics:    return "HARMONICS";
            case Art::Trill:        return "TRILL";
            case Art::Fall:         return "FALL";
            case Art::Doit:         return "DOIT";
            case Art::Scoop:        return "SCOOP";
            case Art::Shake:        return "SHAKE";
            case Art::Flutter:      return "FLUTTER";
            case Art::Growl:        return "GROWL";
            case Art::MuteStraight: return "STRAIGHT MUTE";
            case Art::MuteCup:      return "CUP MUTE";
            case Art::MuteHarmon:   return "HARMON MUTE";
            case Art::MuteWah:      return "WAH MUTE";
            case Art::Subtone:      return "SUBTONE";
            case Art::Overblow:     return "OVERBLOW";
            case Art::PalmMute:     return "PALM MUTE";
            case Art::Slide:        return "SLIDE";
            case Art::HammerOn:     return "HAMMER-ON";
            case Art::Bend:         return "BEND";
            case Art::DeadNote:     return "DEAD NOTE";
            case Art::Pedal:        return "PEDAL";
            case Art::HalfPedal:    return "HALF PEDAL";
            case Art::Sostenuto:    return "SOSTENUTO";
            case Art::UnaCorda:     return "UNA CORDA";
            case Art::DeadStroke:   return "DEAD STROKE";
            case Art::Roll:         return "ROLL";
            case Art::SoftMallet:   return "SOFT MALLET";
            case Art::HardMallet:   return "HARD MALLET";
            case Art::MotorOn:      return "MOTOR ON";
            case Art::MotorOff:     return "MOTOR OFF";
            case Art::RotorSlow:    return "ROTOR SLOW";
            case Art::RotorFast:    return "ROTOR FAST";
            case Art::PercOn:       return "PERC ON";
            case Art::PercOff:      return "PERC OFF";
            case Art::VibOn:        return "VIB/CHO ON";
            case Art::VibOff:       return "VIB/CHO OFF";
            case Art::BellowsShake: return "BELLOWS SHAKE";
            case Art::RegisterUp:   return "REGISTER UP";
            case Art::RegisterDown: return "REGISTER DOWN";
            case Art::GlideLegato:  return "GLIDE LEGATO";
            case Art::Retrigger:    return "RETRIGGER";
            case Art::Sustain:      return "SUSTAIN";
            default:                return "?";
        }
    }

    /** True when selecting it flips a lasting state rather than shaping notes. */
    inline bool isStateArt (Art a)
    {
        switch (a)
        {
            case Art::MotorOn: case Art::MotorOff: case Art::RotorSlow: case Art::RotorFast:
            case Art::PercOn: case Art::PercOff: case Art::VibOn: case Art::VibOff:
            case Art::RegisterUp: case Art::RegisterDown: case Art::Sostenuto:
                return true;
            default:
                return false;
        }
    }

    struct ArtList { int count; Art arts[kMaxArts]; };

    inline const ArtList& artsFor (Archetype a)
    {
        static const ArtList lists[kNumArchetypes] = {
            /* STRUCK STRING */ { 5,  { Art::Normal, Art::Pedal, Art::HalfPedal, Art::Sostenuto, Art::UnaCorda } },
            /* TINE EP       */ { 4,  { Art::Normal, Art::Pedal, Art::HalfPedal, Art::Sostenuto } },
            /* STRUCK BAR    */ { 7,  { Art::Normal, Art::DeadStroke, Art::Roll, Art::SoftMallet, Art::HardMallet, Art::MotorOn, Art::MotorOff } },
            /* TONEWHEEL     */ { 6,  { Art::RotorSlow, Art::RotorFast, Art::PercOn, Art::PercOff, Art::VibOn, Art::VibOff } },
            /* PIPE ORGAN    */ { 2,  { Art::Normal, Art::Staccato } },
            /* FREE REED     */ { 4,  { Art::Normal, Art::BellowsShake, Art::RegisterUp, Art::RegisterDown } },
            /* PLUCKED       */ { 7,  { Art::Sustain, Art::PalmMute, Art::Harmonics, Art::Slide, Art::HammerOn, Art::Bend, Art::DeadNote } },
            /* BOWED         */ { 11, { Art::Legato, Art::Detache, Art::Staccato, Art::Spiccato, Art::Marcato, Art::Sfz, Art::Tremolo,
                                        Art::Pizzicato, Art::ColLegno, Art::Harmonics, Art::Trill } },
            /* LIP REED      */ { 14, { Art::Legato, Art::Staccato, Art::Marcato, Art::Sfz, Art::Fall, Art::Doit, Art::Scoop,
                                        Art::Shake, Art::Flutter, Art::Growl, Art::MuteStraight, Art::MuteCup, Art::MuteHarmon, Art::MuteWah } },
            /* SINGLE REED   */ { 9,  { Art::Legato, Art::Staccato, Art::Flutter, Art::Growl, Art::Subtone, Art::Overblow, Art::Trill, Art::Fall, Art::Scoop } },
            /* DOUBLE REED   */ { 9,  { Art::Legato, Art::Staccato, Art::Flutter, Art::Growl, Art::Subtone, Art::Overblow, Art::Trill, Art::Fall, Art::Scoop } },
            /* AIR JET       */ { 9,  { Art::Legato, Art::Staccato, Art::Flutter, Art::Growl, Art::Subtone, Art::Overblow, Art::Trill, Art::Fall, Art::Scoop } },
            /* SYNTH         */ { 3,  { Art::Normal, Art::GlideLegato, Art::Retrigger } }
        };
        return lists[(int) a];
    }

    inline Art artAt (Archetype a, int index)
    {
        const auto& l = artsFor (a);
        return l.arts[std::min (l.count - 1, std::max (0, index))];
    }

    inline int findArt (Archetype a, Art want)
    {
        const auto& l = artsFor (a);
        for (int i = 0; i < l.count; ++i) if (l.arts[i] == want) return i;
        return -1;
    }

    //==========================================================================
    //  What a NOTE articulation does to the voice.
    //==========================================================================
    enum class PitchFx { None, Scoop, Fall, Doit, Bend, Shake, Trill, Slide };

    struct ArtFx
    {
        float   velMul       = 1.0f;
        float   transientAdd = 0.0f;
        float   brightAdd    = 0.0f;
        float   noiseAdd     = 0.0f;
        float   dampAdd      = 0.0f;
        float   decayMul     = 1.0f;
        float   attackMul    = 1.0f;     // drive-envelope attack
        float   autoRelMs    = 0.0f;     // > 0: the note lets go by itself (staccato)
        float   releaseMinS  = 0.0f;     // amp release at least this long (falls, doits)
        float   releaseMaxS  = 0.0f;     // > 0: amp release at most this long (staccato)
        float   accent       = 0.0f;     // drive overshoot at the attack (marcato)
        bool    sfz          = false;    // strong attack, then drop and hold low
        float   amRate       = 0.0f;     // drive AM (bowed tremolo, bellows shake)
        float   amDepth      = 0.0f;
        float   growl        = 0.0f;     // rough modulation of the drive
        float   flutter      = 0.0f;
        float   freqMul      = 1.0f;     // harmonics / overblow
        bool    harmonic     = false;    // plucked: excite at the node
        bool    softPluck    = false;    // hammer-on in poly
        bool    roll         = false;
        bool    forceLegato  = false;
        bool    forceRetrig  = false;
        float   pedal        = -1.0f;    // >= 0: this damper position for these notes
        float   soft         = 0.0f;
        float   overblowSpec = -1.0f;    // air jet: physical overblow
        float   subtoneSpec  = -1.0f;    // single reed subtone
        PitchFx pitch        = PitchFx::None;
        Excite  excite       = Excite::Normal;
        Mute    mute         = Mute::Open;
    };

    inline ArtFx artFx (Art a, Archetype arch)
    {
        ArtFx f;
        switch (a)
        {
            case Art::Legato:       f.transientAdd = -0.35f; f.attackMul = 1.4f; break;
            case Art::Detache:      f.transientAdd = 0.25f; f.attackMul = 0.7f; break;
            case Art::Staccato:     f.autoRelMs = 110.0f; f.releaseMaxS = 0.09f; f.transientAdd = 0.3f; f.attackMul = 0.5f; break;
            case Art::Spiccato:     f.autoRelMs = 55.0f;  f.releaseMaxS = 0.06f; f.transientAdd = 0.45f; f.attackMul = 0.35f; f.velMul = 0.9f; break;
            case Art::Marcato:      f.accent = 0.45f; f.transientAdd = 0.4f; f.velMul = 1.2f; f.attackMul = 0.5f; break;
            case Art::Sfz:          f.sfz = true; f.transientAdd = 0.5f; f.attackMul = 0.4f; break;
            case Art::Tremolo:      f.amRate = 1.0f; f.amDepth = 0.75f; break;
            case Art::Pizzicato:    f.excite = Excite::Pluck; f.releaseMaxS = 0.35f; break;
            case Art::ColLegno:     f.excite = Excite::Strike; f.releaseMaxS = 0.25f; break;
            case Art::Harmonics:
                if (arch == Archetype::PluckedString) { f.harmonic = true; f.brightAdd = 0.2f; }
                else { f.freqMul = 2.0f; f.brightAdd = -0.25f; f.velMul = 0.8f; }
                break;
            case Art::Trill:        f.pitch = PitchFx::Trill; break;
            case Art::Fall:         f.pitch = PitchFx::Fall; f.releaseMinS = 0.45f; break;
            case Art::Doit:         f.pitch = PitchFx::Doit; f.releaseMinS = 0.4f; break;
            case Art::Scoop:        f.pitch = PitchFx::Scoop; break;
            case Art::Shake:        f.pitch = PitchFx::Shake; break;
            case Art::Flutter:      f.flutter = 0.6f; break;
            case Art::Growl:        f.growl = 0.55f; f.brightAdd = 0.1f; break;
            case Art::MuteStraight: f.mute = Mute::Straight; break;
            case Art::MuteCup:      f.mute = Mute::Cup; break;
            case Art::MuteHarmon:   f.mute = Mute::Harmon; break;
            case Art::MuteWah:      f.mute = Mute::Wah; break;
            case Art::Subtone:      f.brightAdd = -0.55f; f.noiseAdd = 0.3f; f.subtoneSpec = 0.85f; f.velMul = 0.85f; break;
            case Art::Overblow:
                if (arch == Archetype::AirJet) f.overblowSpec = 1.0f;
                else f.freqMul = 2.0f;
                f.brightAdd = 0.25f; f.noiseAdd = 0.1f;
                break;
            case Art::PalmMute:     f.dampAdd = 0.65f; f.brightAdd = -0.3f; break;
            case Art::Slide:        f.pitch = PitchFx::Slide; f.forceLegato = true; break;
            case Art::HammerOn:     f.forceLegato = true; f.softPluck = true; break;
            case Art::Bend:         f.pitch = PitchFx::Bend; break;
            case Art::DeadNote:     f.excite = Excite::Strike; f.dampAdd = 1.0f; f.releaseMaxS = 0.08f; break;
            case Art::Pedal:        f.pedal = 1.0f; break;
            case Art::HalfPedal:    f.pedal = 0.5f; break;
            case Art::UnaCorda:     f.soft = 1.0f; f.brightAdd = -0.2f; f.velMul = 0.92f; break;
            case Art::DeadStroke:   f.dampAdd = 0.92f; break;
            case Art::Roll:         f.roll = true; break;
            case Art::SoftMallet:   f.brightAdd = -0.5f; break;
            case Art::HardMallet:   f.brightAdd = 0.5f; break;
            case Art::BellowsShake: f.amRate = 0.4f; f.amDepth = 0.6f; break;
            case Art::GlideLegato:  f.forceLegato = true; break;
            case Art::Retrigger:    f.forceRetrig = true; break;
            default: break;
        }
        return f;
    }
}
