// C:\workspace\SONIFY PLUGIN natura\src\Params.h
#pragma once
//==============================================================================
//  Params.h  -  every Natura parameter, in one list.
//
//  The X-macro below generates the parameter enum, the host parameter layout,
//  the GUI readouts and the preset keys from the same rows, so a parameter
//  can never exist in one place and be missing in another.
//
//  Percentage controls are whole steps 0..100 (101 positions), the way every
//  Sonify panel shows them.  Times and rates keep a 0..100 travel and print
//  the real value (Grex's squared time law: fine at the short end).
//==============================================================================
#include <JuceHeader.h>

namespace natura
{
    enum class Kind { Float, Int, Choice, Bool };

    enum class Unit
    {
        Pct, Bip, Raw,                     // 0..100, -100..100, plain number
        EnvA, EnvD, EnvR,                  // squared times: 5 s, 20 s, 10 s
        Cutoff,                            // 20 Hz * 1000^x
        LfoRate, LfoDelay, VibRate,        // 0..20 Hz, 0..5 s, 0..12 Hz
        Cents50, Semis, Octave, Note,      // fine tune, semitones, octaves, MIDI note name
        Db, PortaTime, GlideTime, GlideDepth, DelayTime, ArtRate, Drawbar, Voices, Width
    };

    //   id                 name             kind            min   max   def  choices / unit
#define NATURA_PARAMS(X) \
    X (archetype,        "Archetype",      Kind::Choice,     0, 12,   0, "STRUCK STRING|TINE / REED EP|STRUCK BAR|TONEWHEEL ORGAN|PIPE ORGAN|FREE REED|PLUCKED STRING|BOWED STRING|LIP REED (BRASS)|SINGLE REED|DOUBLE REED|AIR JET|SYNTH", Unit::Raw) \
    /* EXCITER */ \
    X (exc_strength,     "Strength",       Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (exc_hardness,     "Hardness",       Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (exc_noise,        "Noise",          Kind::Float,      0, 100, 20, "", Unit::Pct) \
    X (exc_transient,    "Transient",      Kind::Float,      0, 100, 50, "", Unit::Pct) \
    /* RESONATOR */ \
    X (res_decay,        "Decay",          Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (res_damping,      "Damping",        Kind::Float,      0, 100, 40, "", Unit::Pct) \
    X (res_inharm,       "Stiffness",      Kind::Float,      0, 100, 25, "", Unit::Pct) \
    X (res_oddeven,      "Odd/Even",       Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (res_unison,       "Unison",         Kind::Int,        1,   3,  1, "", Unit::Raw) \
    X (res_detune,       "Detune",         Kind::Float,      0, 100, 15, "", Unit::Pct) \
    /* BODY */ \
    X (body_size,        "Body Size",      Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (body_material,    "Body Material",  Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (body_reso,        "Body Resonance", Kind::Float,      0, 100, 40, "", Unit::Pct) \
    X (body_mix,         "Body Mix",       Kind::Float,      0, 100, 30, "", Unit::Pct) \
    /* CHARACTER */ \
    X (chr_drive,        "Drive",          Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (chr_sat,          "Warmth",         Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (chr_tone,         "Tone",           Kind::Float,   -100, 100,  0, "", Unit::Bip) \
    /* STRUCK STRING */ \
    X (ss_hammerpos,     "Hammer Pos",     Kind::Float,      0, 100, 35, "", Unit::Pct) \
    X (ss_strings,       "Strings",        Kind::Float,      0, 100,100, "", Unit::Pct) \
    X (ss_detune,        "String Detune",  Kind::Float,      0, 100, 25, "", Unit::Pct) \
    X (ss_symp,          "Sympathetic",    Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (ss_keyoff,        "Key Off",        Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (ss_pedalnoise,    "Pedal Noise",    Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (ss_lid,           "Lid",            Kind::Float,      0, 100, 60, "", Unit::Pct) \
    /* TINE / REED EP */ \
    X (ep_type,          "EP Type",        Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (ep_chime,         "Tine Chime",     Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (ep_pickup,        "Pickup",         Kind::Float,      0, 100, 60, "", Unit::Pct) \
    X (ep_bark,          "Bark",           Kind::Float,      0, 100, 40, "", Unit::Pct) \
    X (ep_tremrate,      "Trem Rate",      Kind::Float,      0, 100, 40, "", Unit::LfoRate) \
    X (ep_tremdepth,     "Trem Depth",     Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (ep_keyoff,        "EP Key Off",     Kind::Float,      0, 100, 20, "", Unit::Pct) \
    /* STRUCK BAR */ \
    X (sb_mallet,        "Mallet",         Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (sb_material,      "Bar",            Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (sb_strike,        "Strike Pos",     Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (sb_tube,          "Tube",           Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (sb_motorrate,     "Motor Rate",     Kind::Float,      0, 100, 40, "", Unit::LfoRate) \
    X (sb_motordepth,    "Motor Depth",    Kind::Float,      0, 100,  0, "", Unit::Pct) \
    /* TONEWHEEL */ \
    X (tw_db1,           "16'",            Kind::Int,        0,   8,  8, "", Unit::Drawbar) \
    X (tw_db2,           "5 1/3'",         Kind::Int,        0,   8,  8, "", Unit::Drawbar) \
    X (tw_db3,           "8'",             Kind::Int,        0,   8,  8, "", Unit::Drawbar) \
    X (tw_db4,           "4'",             Kind::Int,        0,   8,  0, "", Unit::Drawbar) \
    X (tw_db5,           "2 2/3'",         Kind::Int,        0,   8,  0, "", Unit::Drawbar) \
    X (tw_db6,           "2'",             Kind::Int,        0,   8,  0, "", Unit::Drawbar) \
    X (tw_db7,           "1 3/5'",         Kind::Int,        0,   8,  0, "", Unit::Drawbar) \
    X (tw_db8,           "1 1/3'",         Kind::Int,        0,   8,  0, "", Unit::Drawbar) \
    X (tw_db9,           "1'",             Kind::Int,        0,   8,  0, "", Unit::Drawbar) \
    X (tw_click,         "Click",          Kind::Float,      0, 100, 40, "", Unit::Pct) \
    X (tw_leak,          "Leakage",        Kind::Float,      0, 100, 20, "", Unit::Pct) \
    X (tw_perc,          "Percussion",     Kind::Choice,     0,   2,  0, "OFF|2ND|3RD", Unit::Raw) \
    X (tw_percdecay,     "Perc Decay",     Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (tw_percvol,       "Perc Volume",    Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (tw_vib,           "Vibrato/Chorus", Kind::Choice,     0,   6,  0, "OFF|V1|V2|V3|C1|C2|C3", Unit::Raw) \
    /* PIPE ORGAN */ \
    X (po_principal,     "Principal 8'",   Kind::Float,      0, 100, 80, "", Unit::Pct) \
    X (po_flute,         "Flute 8'",       Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (po_reed,          "Reed 8'",        Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (po_mixture,       "Mixture",        Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (po_bourdon,       "Bourdon 16'",    Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (po_chiff,         "Chiff",          Kind::Float,      0, 100, 40, "", Unit::Pct) \
    X (po_wind,          "Wind",           Kind::Float,      0, 100, 20, "", Unit::Pct) \
    X (po_tremrate,      "Tremulant Rate", Kind::Float,      0, 100, 40, "", Unit::LfoRate) \
    X (po_tremdepth,     "Tremulant",      Kind::Float,      0, 100,  0, "", Unit::Pct) \
    /* FREE REED */ \
    X (fr_bellows,       "Bellows",        Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (fr_register,      "Register",       Kind::Choice,     0,   9,  5, "BASSOON|CLARINET|PICCOLO|BANDONEON|VIOLIN|MUSETTE|OBOE|ORGAN|ACCORDION|MASTER", Unit::Raw) \
    X (fr_musette,       "Musette",        Kind::Float,      0, 100, 35, "", Unit::Pct) \
    X (fr_tone,          "Reed Tone",      Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (fr_chamber,       "Chamber",        Kind::Float,      0, 100, 30, "", Unit::Pct) \
    /* PLUCKED STRING */ \
    X (pl_position,      "Pluck Pos",      Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (pl_plectrum,      "Plectrum",       Kind::Float,      0, 100, 35, "", Unit::Pct) \
    X (pl_fret,          "Fret Noise",     Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (pl_mute,          "Palm Mute",      Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (pl_pickup,        "Pickup Pos",     Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (pl_ring,          "Let Ring",       Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (pl_amp,           "Amp",            Kind::Choice,     0,   1,  0, "OFF|ON", Unit::Raw) \
    X (pl_drive,         "Amp Drive",      Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (pl_amptone,       "Amp Tone",       Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (pl_keyoff,        "Release Noise",  Kind::Float,      0, 100, 20, "", Unit::Pct) \
    /* BOWED STRING */ \
    X (bw_pressure,      "Bow Force",      Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (bw_speed,         "Bow Speed",      Kind::Float,      0, 100, 60, "", Unit::Pct) \
    X (bw_position,      "Bow Pos",        Kind::Float,      0, 100, 40, "", Unit::Pct) \
    X (bw_rosin,         "Rosin",          Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (bw_attack,        "Bow Attack",     Kind::Float,      0, 100, 40, "", Unit::Pct) \
    /* LIP REED */ \
    X (lr_tension,       "Lip Tension",    Kind::Float,      0, 100, 55, "", Unit::Pct) \
    X (lr_growl,         "Growl",          Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (lr_bell,          "Bell",           Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (lr_mute,          "Mute",           Kind::Choice,     0,   4,  0, "OPEN|STRAIGHT|CUP|HARMON|WAH", Unit::Raw) \
    X (lr_wah,           "Wah",            Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (lr_bore,          "Bore",           Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (lr_buzz,          "Buzz",           Kind::Float,      0, 100, 40, "", Unit::Pct) \
    /* SINGLE REED */ \
    X (sr_stiffness,     "Reed",           Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (sr_bite,          "Bite",           Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (sr_bore,          "Bore Shape",     Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (sr_subtone,       "Subtone",        Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (sr_click,         "Key Click",      Kind::Float,      0, 100, 30, "", Unit::Pct) \
    /* DOUBLE REED */ \
    X (dr_stiffness,     "DR Reed",        Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (dr_bite,          "DR Bite",        Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (dr_nasal,         "Nasal",          Kind::Float,      0, 100, 60, "", Unit::Pct) \
    X (dr_bore,          "DR Bore",        Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (dr_click,         "DR Key Click",   Kind::Float,      0, 100, 30, "", Unit::Pct) \
    /* AIR JET */ \
    X (aj_breath,        "Breath",         Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (aj_jet,           "Jet",            Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (aj_flutter,       "Flutter",        Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (aj_overblow,      "Overblow",       Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (aj_pipe,          "Pipe",           Kind::Float,      0, 100,  0, "", Unit::Pct) \
    /* SYNTH (Bambino two-oscillator engine) */ \
    X (s1_wave,          "Osc1 Wave",      Kind::Choice,     0,   3,  0, "SAW|PULSE|TRI|SINE", Unit::Raw) \
    X (s1_oct,           "Osc1 Octave",    Kind::Int,       -2,   2,  0, "", Unit::Octave) \
    X (s1_semi,          "Osc1 Semi",      Kind::Int,      -12,  12,  0, "", Unit::Semis) \
    X (s1_fine,          "Osc1 Fine",      Kind::Float,    -50,  50,  0, "", Unit::Cents50) \
    X (s1_pw,            "Osc1 PW",        Kind::Float,      5,  95, 50, "", Unit::Pct) \
    X (s1_level,         "Osc1 Level",     Kind::Float,      0, 100,100, "", Unit::Pct) \
    X (s2_wave,          "Osc2 Wave",      Kind::Choice,     0,   3,  1, "SAW|PULSE|TRI|SINE", Unit::Raw) \
    X (s2_oct,           "Osc2 Octave",    Kind::Int,       -2,   2, -1, "", Unit::Octave) \
    X (s2_semi,          "Osc2 Semi",      Kind::Int,      -12,  12,  0, "", Unit::Semis) \
    X (s2_fine,          "Osc2 Fine",      Kind::Float,    -50,  50, -4, "", Unit::Cents50) \
    X (s2_pw,            "Osc2 PW",        Kind::Float,      5,  95, 50, "", Unit::Pct) \
    X (s2_level,         "Osc2 Level",     Kind::Float,      0, 100, 55, "", Unit::Pct) \
    X (sy_fm,            "FM",             Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (sy_ring,          "Ring",           Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (sy_sync,          "Sync",           Kind::Choice,     0,   1,  0, "OFF|ON", Unit::Raw) \
    X (sy_sub,           "Sub",            Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (sy_noise,         "Osc Noise",      Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (sy_drive,         "Osc Drive",      Kind::Float,      0, 100, 25, "", Unit::Pct) \
    X (sy_cutoff,        "Ladder Cutoff",  Kind::Float,      0, 100, 45, "", Unit::Pct) \
    X (sy_reso,          "Ladder Reso",    Kind::Float,      0, 100, 18, "", Unit::Pct) \
    X (sy_key,           "Ladder Key",     Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (sy_env,           "Ladder Env",     Kind::Float,   -100, 100, 40, "", Unit::Bip) \
    X (sy_vel,           "Ladder Vel",     Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (sy_a,             "Ladder A",       Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (sy_d,             "Ladder D",       Kind::Float,      0, 100, 35, "", Unit::Pct) \
    X (sy_s,             "Ladder S",       Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (sy_r,             "Ladder R",       Kind::Float,      0, 100, 25, "", Unit::Pct) \
    X (sy_level,         "Synth Level",    Kind::Float,      0, 100, 80, "", Unit::Pct) \
    /* SYNTHESIS */ \
    X (amp_a,            "Amp Attack",     Kind::Float,      0, 100,  0, "", Unit::EnvA) \
    X (amp_d,            "Amp Decay",      Kind::Float,      0, 100,100, "", Unit::EnvD) \
    X (amp_s,            "Amp Sustain",    Kind::Float,      0, 100,100, "", Unit::Pct) \
    X (amp_r,            "Amp Release",    Kind::Float,      0, 100, 15, "", Unit::EnvR) \
    X (amp_shape,        "Amp Shape",      Kind::Float,      0, 100, 65, "", Unit::Raw) \
    X (amp_gain,         "Amp Gain",       Kind::Float,      0, 200,100, "", Unit::Raw) \
    X (amp_var,          "Variation",      Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (flt_type,         "Filter Type",    Kind::Choice,     0,   3,  0, "LP|HP|BP|NT", Unit::Raw) \
    X (flt_cut,          "Cutoff",         Kind::Float,      0, 100,100, "", Unit::Cutoff) \
    X (flt_res,          "Resonance",      Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (flt_key,          "Key Track",      Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (fenv_a,           "Filter Env A",   Kind::Float,      0, 100,  4, "", Unit::EnvA) \
    X (fenv_d,           "Filter Env D",   Kind::Float,      0, 100, 20, "", Unit::EnvD) \
    X (fenv_s,           "Filter Env S",   Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (fenv_r,           "Filter Env R",   Kind::Float,      0, 100, 27, "", Unit::EnvR) \
    X (fenv_amt,         "Filter Env Amt", Kind::Float,   -100, 100,  0, "", Unit::Bip) \
    X (vel_curve,        "Velocity Curve", Kind::Float,   -100, 100,  0, "", Unit::Bip) \
    X (voice_mode,       "Voice Mode",     Kind::Choice,     0,   2,  0, "POLY|MONO LEGATO|MONO RETRIG", Unit::Raw) \
    X (mono_hold,        "Hold Stolen",    Kind::Bool,       0,   1,  1, "", Unit::Raw) \
    X (mono_retrig,      "Retrig Stolen",  Kind::Bool,       0,   1,  0, "", Unit::Raw) \
    X (port,             "Portamento",     Kind::Float,      0, 100, 22, "", Unit::PortaTime) \
    X (octave,           "Octave",         Kind::Int,       -3,   3,  0, "", Unit::Octave) \
    X (range_on,         "Allowed Notes",  Kind::Bool,       0,   1,  0, "", Unit::Raw) \
    X (range_lo,         "Lowest Note",    Kind::Int,        0, 127, 21, "", Unit::Note) \
    X (range_hi,         "Highest Note",   Kind::Int,        0, 127,108, "", Unit::Note) \
    X (voices,           "Voices",         Kind::Int,        1,  32, 16, "", Unit::Voices) \
    /* MODULATION */ \
    X (alfo_rate,        "Amp LFO Rate",   Kind::Float,      0, 100,  0, "", Unit::LfoRate) \
    X (alfo_depth,       "Amp LFO Depth",  Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (alfo_delay,       "Amp LFO Delay",  Kind::Float,      0, 100,  0, "", Unit::LfoDelay) \
    X (flfo_rate,        "Filter LFO Rate",Kind::Float,      0, 100,  0, "", Unit::LfoRate) \
    X (flfo_depth,       "Filter LFO Depth",Kind::Float,     0, 100,  0, "", Unit::Pct) \
    X (flfo_delay,       "Filter LFO Delay",Kind::Float,     0, 100,  0, "", Unit::LfoDelay) \
    X (vib_rate,         "Vibrato Rate",   Kind::Float,      0, 100, 45, "", Unit::VibRate) \
    X (vib_depth,        "Vibrato Depth",  Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (vib_delay,        "Vibrato Delay",  Kind::Float,      0, 100, 30, "", Unit::LfoDelay) \
    X (ag_mode,          "Attack Glide",   Kind::Choice,     0,   4,  0, "OFF|EVERY|EVERY NTH|RANDOM|VELOCITY", Unit::Raw) \
    X (ag_depth,         "Glide Depth",    Kind::Float,      0, 100, 50, "", Unit::GlideDepth) \
    X (ag_time,          "Glide Time",     Kind::Float,      0, 100, 35, "", Unit::GlideTime) \
    X (ag_shape,         "Glide Shape",    Kind::Float,      0, 100, 65, "", Unit::Raw) \
    X (ag_every,         "Glide Every",    Kind::Int,        2,  16,  4, "", Unit::Raw) \
    X (ag_odds,          "Glide Odds",     Kind::Int,        2,  16,  5, "", Unit::Raw) \
    X (ag_vel,           "Glide Velocity", Kind::Int,        1, 127,100, "", Unit::Raw) \
    X (humanize,         "Humanize",       Kind::Float,      0, 100,  0, "", Unit::Pct) \
    X (pb_range,         "Bend Range",     Kind::Int,        0,  24,  2, "", Unit::Semis) \
    X (dyn_src,          "Dynamics",       Kind::Choice,     0,   4,  0, "VELOCITY|MOD WHEEL|BREATH CC2|EXPRESSION CC11|AFTERTOUCH", Unit::Raw) \
    X (mw_target,        "Mod Wheel",      Kind::Choice,     0,   4,  0, "VIBRATO|BRIGHTNESS|DYNAMICS|WAH / MUTE|NONE", Unit::Raw) \
    X (at_target,        "Aftertouch",     Kind::Choice,     0,   4,  1, "VIBRATO|BRIGHTNESS|DYNAMICS|WAH / MUTE|NONE", Unit::Raw) \
    X (leg_vel,          "Legato Velocity",Kind::Float,      0, 100,  0, "", Unit::Pct) \
    /* ARTICULATIONS */ \
    X (art_select,       "Articulation",   Kind::Int,        0,  13,  0, "", Unit::Raw) \
    X (art_ks_on,        "Keyswitches",    Kind::Bool,       0,   1,  1, "", Unit::Raw) \
    X (art_ks_base,      "Keyswitch Base", Kind::Int,        0, 114, 24, "", Unit::Note) \
    X (art_cc_on,        "CC Zones",       Kind::Bool,       0,   1,  0, "", Unit::Raw) \
    X (art_cc_num,       "Zone CC",        Kind::Int,        0, 127, 20, "", Unit::Raw) \
    X (art_vel_on,       "Velocity Zones", Kind::Bool,       0,   1,  0, "", Unit::Raw) \
    X (art_vel_split,    "Velocity Split", Kind::Int,        1, 127,100, "", Unit::Raw) \
    X (art_vel_lo,       "Soft Artic",     Kind::Int,        0,  13,  0, "", Unit::Raw) \
    X (art_vel_hi,       "Hard Artic",     Kind::Int,        0,  13,  1, "", Unit::Raw) \
    X (art_auto_on,      "Auto Artic",     Kind::Bool,       0,   1,  0, "", Unit::Raw) \
    X (art_auto_ms,      "Repeat Time",    Kind::Int,       40, 400,140, "", Unit::Raw) \
    X (art_trill,        "Trill Interval", Kind::Int,        1,   2,  2, "", Unit::Semis) \
    X (art_rate,         "Artic Rate",     Kind::Float,      0, 100, 45, "", Unit::ArtRate) \
    /* FX: rotary -> phaser -> chorus -> delay -> reverb -> EQ */ \
    X (rot_on,           "Rotary",         Kind::Bool,       0,   1,  0, "", Unit::Raw) \
    X (rot_fast,         "Rotor Fast",     Kind::Bool,       0,   1,  0, "", Unit::Raw) \
    X (rot_drive,        "Rotary Drive",   Kind::Float,      0, 100, 20, "", Unit::Pct) \
    X (rot_balance,      "Horn/Drum",      Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (rot_mix,          "Rotary Mix",     Kind::Float,      0, 100,100, "", Unit::Pct) \
    X (pha_on,           "Phaser",         Kind::Bool,       0,   1,  0, "", Unit::Raw) \
    X (pha_rate,         "Phaser Rate",    Kind::Float,      0, 100, 25, "", Unit::LfoRate) \
    X (pha_depth,        "Phaser Depth",   Kind::Float,      0, 100, 60, "", Unit::Pct) \
    X (pha_fb,           "Phaser Feedback",Kind::Float,      0, 100, 40, "", Unit::Pct) \
    X (pha_mix,          "Phaser Mix",     Kind::Float,      0, 100, 50, "", Unit::Pct) \
    X (cho_on,           "Chorus",         Kind::Bool,       0,   1,  0, "", Unit::Raw) \
    X (cho_rate,         "Chorus Rate",    Kind::Float,      0, 100, 25, "", Unit::LfoRate) \
    X (cho_depth,        "Chorus Depth",   Kind::Float,      0, 100, 45, "", Unit::Pct) \
    X (cho_mix,          "Chorus Mix",     Kind::Float,      0, 100, 40, "", Unit::Pct) \
    X (dly_on,           "Delay",          Kind::Bool,       0,   1,  0, "", Unit::Raw) \
    X (dly_time,         "Delay Time",     Kind::Float,      0, 100, 55, "", Unit::DelayTime) \
    X (dly_fb,           "Delay Feedback", Kind::Float,      0, 100, 30, "", Unit::Pct) \
    X (dly_tone,         "Delay Tone",     Kind::Float,      0, 100, 60, "", Unit::Pct) \
    X (dly_pingpong,     "Ping Pong",      Kind::Bool,       0,   1,  1, "", Unit::Raw) \
    X (dly_mix,          "Delay Mix",      Kind::Float,      0, 100, 20, "", Unit::Pct) \
    X (rev_on,           "Reverb",         Kind::Bool,       0,   1,  1, "", Unit::Raw) \
    X (rev_size,         "Reverb Size",    Kind::Float,      0, 100, 55, "", Unit::Pct) \
    X (rev_damp,         "Reverb Damping", Kind::Float,      0, 100, 45, "", Unit::Pct) \
    X (rev_predelay,     "Pre-Delay",      Kind::Float,      0, 100, 15, "", Unit::Pct) \
    X (rev_mix,          "Reverb Mix",     Kind::Float,      0, 100, 18, "", Unit::Pct) \
    X (eq_low,           "Low EQ",         Kind::Float,    -12,  12,  0, "", Unit::Db) \
    X (eq_high,          "High EQ",        Kind::Float,    -12,  12,  0, "", Unit::Db) \
    X (out_width,        "Width",          Kind::Float,      0, 200,100, "", Unit::Width) \
    X (out_volume,       "Volume",         Kind::Float,    -36,   6, -3, "", Unit::Db)

    enum class P : int
    {
       #define NATURA_ENUM(id, name, kind, mn, mx, def, choices, unit) id,
        NATURA_PARAMS (NATURA_ENUM)
       #undef NATURA_ENUM
        kCount
    };

    constexpr int kNumParams = (int) P::kCount;

    struct ParamInfo
    {
        const char* id;
        const char* name;
        Kind  kind;
        float minV, maxV, defV;
        const char* choices;
        Unit  unit;
    };

    inline const ParamInfo& info (P p)
    {
        static const ParamInfo table[kNumParams] = {
           #define NATURA_INFO(id, name, kind, mn, mx, def, choices, unit) { #id, name, kind, (float) (mn), (float) (mx), (float) (def), choices, unit },
            NATURA_PARAMS (NATURA_INFO)
           #undef NATURA_INFO
        };
        return table[(int) p];
    }

    inline const ParamInfo& info (int p) { return info ((P) p); }

    inline int findParam (const juce::String& id)
    {
        for (int i = 0; i < kNumParams; ++i)
            if (id == info (i).id) return i;
        return -1;
    }

    inline juce::StringArray choiceNames (P p)
    {
        return juce::StringArray::fromTokens (juce::String (info (p).choices), "|", "");
    }

    //==========================================================================
    //  Value laws shared by the engine and the readouts
    //==========================================================================
    namespace law
    {
        inline float sq (float ui) { const float f = juce::jlimit (0.0f, 1.0f, ui / 100.0f); return f * f; }
        inline float envA (float ui)      { return std::max (0.0005f, sq (ui) * 5.0f); }    // seconds
        inline float envD (float ui)      { return std::max (0.002f,  sq (ui) * 20.0f); }
        inline float envR (float ui)      { return std::max (0.003f,  sq (ui) * 10.0f); }
        inline float cutoffHz (float ui)  { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 100.0f, ui) / 100.0f); }
        inline float lfoRate (float ui)   { return sq (ui) * 20.0f; }
        inline float lfoDelay (float ui)  { return sq (ui) * 5.0f; }                        // seconds
        inline float vibRate (float ui)   { return 0.5f + (ui / 100.0f) * 11.5f; }
        inline float portaMs (float ui)   { return sq (ui) * 2000.0f; }
        inline float glideMs (float ui)   { return std::max (1.0f, sq (ui) * 500.0f); }
        inline float glideCents (float ui){ return ui * 6.0f; }                             // 0..600 cents
        inline float delayMs (float ui)   { return 10.0f + sq (ui) * 1990.0f; }
        inline float artRate (float ui)   { return 2.0f + (ui / 100.0f) * 18.0f; }
        inline float shapeK (float ui)    { return (juce::jlimit (0.0f, 100.0f, ui) - 50.0f) * 0.4f; }
    }

    inline juce::String noteName (int n)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        n = juce::jlimit (0, 127, n);
        return juce::String (names[n % 12]) + juce::String (n / 12 - 2);   // Yamaha naming, C3 = 60
    }

    inline juce::String timeText (float sec)
    {
        return sec < 1.0f ? juce::String ((int) std::lround (sec * 1000.0f)) + "ms"
                          : juce::String (sec, sec < 10.0f ? 2 : 1) + "s";
    }

    /** What a control prints for a value. */
    inline juce::String displayText (P p, float v)
    {
        const auto& in = info (p);
        if (in.kind == Kind::Choice)
        {
            const auto names = choiceNames (p);
            const int i = juce::jlimit (0, names.size() - 1, (int) std::lround (v));
            return names[i];
        }
        if (in.kind == Kind::Bool) return v >= 0.5f ? "ON" : "OFF";

        switch (in.unit)
        {
            case Unit::Pct:        return juce::String ((int) std::lround (v));
            case Unit::Bip:        return (v > 0.5f ? "+" : "") + juce::String ((int) std::lround (v));
            case Unit::EnvA:       return timeText (law::envA (v));
            case Unit::EnvD:       return v >= 99.5f ? "HOLD" : timeText (law::envD (v));
            case Unit::EnvR:       return timeText (law::envR (v));
            case Unit::Cutoff:     { const float hz = law::cutoffHz (v); return v >= 99.5f ? "OPEN" : (hz < 1000.0f ? juce::String ((int) hz) + "Hz" : juce::String (hz / 1000.0f, 1) + "k"); }
            case Unit::LfoRate:    return juce::String (law::lfoRate (v), 2) + "Hz";
            case Unit::LfoDelay:   return timeText (law::lfoDelay (v));
            case Unit::VibRate:    return juce::String (law::vibRate (v), 1) + "Hz";
            case Unit::Cents50:    return juce::String (v, 1) + "c";
            case Unit::Semis:      return juce::String ((int) std::lround (v)) + "st";
            case Unit::Octave:     return (v > 0.5f ? "+" : "") + juce::String ((int) std::lround (v));
            case Unit::Note:       return noteName ((int) std::lround (v));
            case Unit::Db:         return juce::String (v, 1) + "dB";
            case Unit::PortaTime:  return timeText (law::portaMs (v) * 0.001f);
            case Unit::GlideTime:  return timeText (law::glideMs (v) * 0.001f);
            case Unit::GlideDepth: return juce::String (law::glideCents (v) / 100.0f, 2) + "st";
            case Unit::DelayTime:  return timeText (law::delayMs (v) * 0.001f);
            case Unit::ArtRate:    return juce::String (law::artRate (v), 1) + "Hz";
            case Unit::Drawbar:    return juce::String ((int) std::lround (v));
            case Unit::Voices:     return juce::String ((int) std::lround (v));
            case Unit::Width:      return juce::String ((int) std::lround (v)) + "%";
            case Unit::Raw:
            default:
                if (p == P::amp_shape || p == P::ag_shape)
                {
                    const float k = law::shapeK (v);
                    return k <= -8.0f ? "SWELL" : k < -2.0f ? "LOG" : k <= 2.0f ? "LIN" : k < 9.0f ? "EXP" : "TIGHT";
                }
                if (p == P::amp_gain) return juce::String ((int) std::lround (v));
                if (p == P::ag_odds) return "~1 in " + juce::String ((int) std::lround (v));
                if (p == P::ag_every) return "1 in " + juce::String ((int) std::lround (v));
                if (p == P::art_auto_ms) return juce::String ((int) std::lround (v)) + "ms";
                return in.kind == Kind::Float ? juce::String (v, 1) : juce::String ((int) std::lround (v));
        }
    }

    /** The host parameter layout. */
    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (int i = 0; i < kNumParams; ++i)
        {
            const auto& in = info (i);
            const juce::ParameterID pid { in.id, 1 };
            const P p = (P) i;
            auto toText = [p] (float v, int) { return displayText (p, v); };

            switch (in.kind)
            {
                case Kind::Choice:
                    layout.add (std::make_unique<juce::AudioParameterChoice> (pid, in.name, choiceNames (p), (int) in.defV));
                    break;
                case Kind::Bool:
                    layout.add (std::make_unique<juce::AudioParameterBool> (pid, in.name, in.defV >= 0.5f));
                    break;
                case Kind::Int:
                    layout.add (std::make_unique<juce::AudioParameterInt> (pid, in.name, (int) in.minV, (int) in.maxV, (int) in.defV,
                                    juce::AudioParameterIntAttributes().withStringFromValueFunction ([p] (int v, int) { return displayText (p, (float) v); })));
                    break;
                case Kind::Float:
                default:
                {
                    const bool whole = in.unit == Unit::Pct || in.unit == Unit::Bip;
                    juce::NormalisableRange<float> range (in.minV, in.maxV, whole ? 1.0f : 0.0f);
                    layout.add (std::make_unique<juce::AudioParameterFloat> (pid, in.name, range, in.defV,
                                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText)));
                    break;
                }
            }
        }
        return layout;
    }
}
