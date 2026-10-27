// C:\workspace\SONIFY PLUGIN natura\src\Archetypes.h
#pragma once
//==============================================================================
//  Archetypes.h  -  the root model of every archetype, its own controls, and
//  the factory presets edited from those roots.
//
//  ROOT MODEL: choosing an archetype on the panel loads its root - every
//  control back to the values that make that mechanism sound like itself.
//  FACTORY PRESETS: GM-oriented instruments, each one a root plus the few
//  moves that turn it into that instrument (Air Jet -> Recorder, ...).
//==============================================================================
#include "Params.h"
#include "Articulations.h"
#include <vector>

namespace natura
{
    //==========================================================================
    //  The archetype-specific controls, in each model's spec order.
    //==========================================================================
    struct SpecList { int count; P ids[kMaxSpec]; };

    inline const SpecList& specsFor (Archetype a)
    {
        static const SpecList lists[kNumArchetypes] = {
            { 7,  { P::ss_hammerpos, P::ss_strings, P::ss_detune, P::ss_symp, P::ss_keyoff, P::ss_pedalnoise, P::ss_lid } },
            { 7,  { P::ep_type, P::ep_chime, P::ep_pickup, P::ep_bark, P::ep_tremrate, P::ep_tremdepth, P::ep_keyoff } },
            { 6,  { P::sb_mallet, P::sb_material, P::sb_strike, P::sb_tube, P::sb_motorrate, P::sb_motordepth } },
            { 15, { P::tw_db1, P::tw_db2, P::tw_db3, P::tw_db4, P::tw_db5, P::tw_db6, P::tw_db7, P::tw_db8, P::tw_db9,
                    P::tw_click, P::tw_leak, P::tw_perc, P::tw_percdecay, P::tw_percvol, P::tw_vib } },
            { 9,  { P::po_principal, P::po_flute, P::po_reed, P::po_mixture, P::po_bourdon, P::po_chiff, P::po_wind, P::po_tremrate, P::po_tremdepth } },
            { 5,  { P::fr_bellows, P::fr_register, P::fr_musette, P::fr_tone, P::fr_chamber } },
            { 10, { P::pl_position, P::pl_plectrum, P::pl_fret, P::pl_mute, P::pl_pickup, P::pl_ring, P::pl_amp, P::pl_drive, P::pl_amptone, P::pl_keyoff } },
            { 5,  { P::bw_pressure, P::bw_speed, P::bw_position, P::bw_rosin, P::bw_attack } },
            { 7,  { P::lr_tension, P::lr_growl, P::lr_bell, P::lr_mute, P::lr_wah, P::lr_bore, P::lr_buzz } },
            { 5,  { P::sr_stiffness, P::sr_bite, P::sr_bore, P::sr_subtone, P::sr_click } },
            { 5,  { P::dr_stiffness, P::dr_bite, P::dr_nasal, P::dr_bore, P::dr_click } },
            { 5,  { P::aj_breath, P::aj_jet, P::aj_flutter, P::aj_overblow, P::aj_pipe } },
            { 0,  {} }
        };
        return lists[(int) a];
    }

    /** The synth archetype's own page, grouped as it is shown. */
    inline const std::vector<P>& synthOsc1()  { static const std::vector<P> v { P::s1_wave, P::s1_oct, P::s1_semi, P::s1_fine, P::s1_pw, P::s1_level }; return v; }
    inline const std::vector<P>& synthOsc2()  { static const std::vector<P> v { P::s2_wave, P::s2_oct, P::s2_semi, P::s2_fine, P::s2_pw, P::s2_level }; return v; }
    inline const std::vector<P>& synthMix()   { static const std::vector<P> v { P::sy_fm, P::sy_ring, P::sy_sync, P::sy_sub, P::sy_noise, P::sy_drive }; return v; }
    inline const std::vector<P>& synthLadder(){ static const std::vector<P> v { P::sy_cutoff, P::sy_reso, P::sy_key, P::sy_env, P::sy_vel }; return v; }
    inline const std::vector<P>& synthLadEnv(){ static const std::vector<P> v { P::sy_a, P::sy_d, P::sy_s, P::sy_r, P::sy_level }; return v; }

    /** A parameter's plain value as the 0..1 a model's spec slot expects. */
    inline float toSpec (P p, float v)
    {
        const auto& in = info (p);
        return juce::jlimit (0.0f, 1.0f, (v - in.minV) / std::max (1.0e-6f, in.maxV - in.minV));
    }

    /** Does this archetype listen to this shared EXCITER / RESONATOR control?
        (Body, character and unison apply to every archetype.)  The panel
        dims the ones that do nothing for the instrument on screen. */
    inline bool usesShared (Archetype a, P p)
    {
        using A = Archetype;
        switch (p)
        {
            case P::exc_strength:  return a != A::Tonewheel && a != A::PipeOrgan && a != A::FreeReed;
            case P::exc_hardness:  return a == A::StruckString || a == A::TineEP || a == A::StruckBar || a == A::BowedString
                                       || a == A::LipReed || a == A::Synth || a == A::AirJet;
            case P::exc_noise:     return a != A::TineEP && a != A::Tonewheel && a != A::PipeOrgan;
            case P::exc_transient: return a == A::Tonewheel || a == A::PipeOrgan || a == A::FreeReed || a == A::LipReed
                                       || a == A::SingleReed || a == A::DoubleReed || a == A::AirJet;
            case P::res_decay:     return a == A::StruckString || a == A::TineEP || a == A::StruckBar || a == A::PluckedString
                                       || a == A::BowedString || a == A::LipReed || a == A::Synth;
            case P::res_damping:   return a == A::StruckString || a == A::TineEP || a == A::StruckBar || a == A::PluckedString
                                       || a == A::BowedString || a == A::LipReed || a == A::AirJet || a == A::Synth;
            case P::res_inharm:    return a == A::StruckString || a == A::PluckedString || a == A::Synth;
            case P::res_oddeven:   return a == A::Synth;
            default:               return true;
        }
    }

    using Overrides = std::vector<std::pair<P, float>>;

    //==========================================================================
    //  ROOT MODELS
    //==========================================================================
    inline const Overrides& rootOverrides (Archetype a)
    {
        static const Overrides roots[kNumArchetypes] = {
            /* STRUCK STRING */ { { P::amp_r, 20 }, { P::body_size, 70 }, { P::body_material, 40 }, { P::body_reso, 35 }, { P::body_mix, 35 },
                                  { P::exc_noise, 15 }, { P::res_damping, 35 }, { P::res_inharm, 30 }, { P::art_ks_on, 0 },
                                  { P::rev_mix, 18 }, { P::rev_size, 55 } },
            /* TINE EP       */ { { P::body_mix, 0 }, { P::res_damping, 40 }, { P::amp_r, 15 }, { P::art_ks_on, 0 }, { P::rev_mix, 14 } },
            /* STRUCK BAR    */ { { P::body_mix, 20 }, { P::res_damping, 30 }, { P::amp_r, 30 }, { P::art_ks_on, 0 }, { P::rev_mix, 22 } },
            /* TONEWHEEL     */ { { P::dyn_src, 3 }, { P::body_mix, 0 }, { P::chr_drive, 15 }, { P::amp_r, 4 }, { P::rot_on, 1 },
                                  { P::rot_drive, 25 }, { P::art_ks_on, 0 }, { P::rev_mix, 12 } },
            /* PIPE ORGAN    */ { { P::dyn_src, 3 }, { P::body_mix, 0 }, { P::amp_r, 8 }, { P::art_ks_on, 0 }, { P::rev_size, 85 },
                                  { P::rev_mix, 35 }, { P::rev_predelay, 25 } },
            /* FREE REED     */ { { P::body_mix, 25 }, { P::body_size, 40 }, { P::amp_r, 8 }, { P::art_ks_on, 0 }, { P::rev_mix, 15 } },
            /* PLUCKED       */ { { P::body_size, 55 }, { P::body_mix, 45 }, { P::body_material, 50 }, { P::res_damping, 40 },
                                  { P::res_inharm, 10 }, { P::amp_r, 25 }, { P::rev_mix, 16 } },
            /* BOWED         */ { { P::voice_mode, 1 }, { P::port, 15 }, { P::vib_depth, 25 }, { P::vib_rate, 45 }, { P::vib_delay, 30 },
                                  { P::body_size, 30 }, { P::body_mix, 55 }, { P::body_reso, 50 }, { P::amp_r, 18 }, { P::rev_mix, 24 },
                                  { P::rev_size, 60 }, { P::exc_noise, 15 } },
            /* LIP REED      */ { { P::voice_mode, 1 }, { P::port, 12 }, { P::vib_depth, 12 }, { P::vib_delay, 45 }, { P::body_mix, 0 },
                                  { P::amp_r, 12 }, { P::rev_mix, 22 }, { P::exc_noise, 15 }, { P::exc_transient, 55 } },
            /* SINGLE REED   */ { { P::voice_mode, 1 }, { P::port, 10 }, { P::body_mix, 0 }, { P::amp_r, 12 }, { P::rev_mix, 20 } },
            /* DOUBLE REED   */ { { P::voice_mode, 1 }, { P::port, 10 }, { P::vib_depth, 18 }, { P::vib_delay, 40 }, { P::body_mix, 0 },
                                  { P::amp_r, 12 }, { P::rev_mix, 22 } },
            /* AIR JET       */ { { P::voice_mode, 1 }, { P::port, 10 }, { P::vib_depth, 22 }, { P::vib_delay, 30 }, { P::vib_rate, 48 },
                                  { P::body_mix, 0 }, { P::amp_r, 14 }, { P::rev_mix, 25 }, { P::exc_transient, 40 } },
            /* SYNTH         */ { { P::body_mix, 0 }, { P::amp_d, 40 }, { P::amp_s, 75 }, { P::amp_r, 25 }, { P::rev_mix, 12 },
                                  { P::art_ks_on, 0 }, { P::res_damping, 50 }, { P::res_inharm, 20 } }
        };
        return roots[(int) a];
    }

    /** Every parameter at its archetype's root value. */
    inline std::vector<float> rootValues (Archetype a)
    {
        std::vector<float> v ((size_t) kNumParams);
        for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = info (i).defV;
        v[(size_t) P::archetype] = (float) (int) a;
        for (const auto& o : rootOverrides (a)) v[(size_t) o.first] = o.second;
        return v;
    }

    //==========================================================================
    //  FACTORY PRESETS
    //==========================================================================
    struct Factory { const char* name; Archetype arch; Overrides ov; };

    inline const std::vector<Factory>& factoryPresets()
    {
        using A = Archetype;
        static const std::vector<Factory> list = {
            // keys
            { "Grand Piano",        A::StruckString, {} },
            { "Bright Piano",       A::StruckString, { { P::exc_hardness, 70 }, { P::ss_lid, 90 }, { P::body_mix, 25 } } },
            { "Electric Grand",     A::StruckString, { { P::ss_strings, 34 }, { P::res_decay, 35 }, { P::ss_lid, 85 }, { P::body_mix, 10 },
                                                       { P::chr_drive, 10 }, { P::res_damping, 25 }, { P::exc_hardness, 65 } } },
            { "Honky-Tonk",         A::StruckString, { { P::ss_detune, 100 }, { P::res_damping, 45 }, { P::ss_hammerpos, 25 } } },
            { "Dulcimer",           A::StruckString, { { P::exc_hardness, 85 }, { P::ss_detune, 40 }, { P::res_decay, 30 }, { P::res_damping, 20 },
                                                       { P::ss_hammerpos, 15 }, { P::body_size, 30 } } },
            { "Tine Piano",         A::TineEP,       {} },
            { "Suitcase Tremolo",   A::TineEP,       { { P::ep_tremdepth, 45 }, { P::ep_tremrate, 45 }, { P::ep_bark, 55 } } },
            { "Reed Piano",         A::TineEP,       { { P::ep_type, 100 }, { P::ep_chime, 40 }, { P::ep_bark, 50 }, { P::ep_pickup, 50 } } },
            { "Bell EP",            A::TineEP,       { { P::ep_chime, 85 }, { P::exc_hardness, 70 }, { P::cho_on, 1 } } },
            { "Marimba",            A::StruckBar,    {} },
            { "Xylophone",          A::StruckBar,    { { P::sb_material, 25 }, { P::sb_mallet, 85 }, { P::sb_tube, 20 } } },
            { "Vibraphone",         A::StruckBar,    { { P::sb_material, 50 }, { P::sb_motordepth, 45 }, { P::sb_tube, 60 }, { P::sb_mallet, 40 } } },
            { "Glockenspiel",       A::StruckBar,    { { P::sb_material, 75 }, { P::sb_mallet, 90 }, { P::sb_tube, 0 } } },
            { "Celesta",            A::StruckBar,    { { P::sb_material, 72 }, { P::sb_mallet, 55 }, { P::sb_tube, 0 }, { P::body_mix, 40 } } },
            { "Music Box",          A::StruckBar,    { { P::sb_material, 100 }, { P::sb_mallet, 80 }, { P::sb_tube, 0 } } },
            { "Tubular Bells",      A::StruckBar,    { { P::sb_material, 78 }, { P::res_decay, 80 }, { P::sb_mallet, 75 }, { P::sb_tube, 0 } } },
            // organs
            { "Drawbar 888",        A::Tonewheel,    {} },
            { "Jazz Perc",          A::Tonewheel,    { { P::tw_perc, 2 }, { P::tw_vib, 6 } } },
            { "Gospel Full",        A::Tonewheel,    { { P::tw_db4, 8 }, { P::tw_db5, 8 }, { P::tw_db6, 8 }, { P::tw_db7, 8 }, { P::tw_db8, 8 }, { P::tw_db9, 8 },
                                                       { P::rot_fast, 1 }, { P::chr_drive, 20 } } },
            { "Ballad 838",         A::Tonewheel,    { { P::tw_db2, 3 }, { P::tw_db9, 4 }, { P::tw_vib, 6 }, { P::chr_drive, 5 } } },
            { "Church Organ",       A::PipeOrgan,    {} },
            { "Flute Stop",         A::PipeOrgan,    { { P::po_principal, 10 }, { P::po_flute, 90 }, { P::po_mixture, 0 }, { P::po_bourdon, 20 }, { P::po_chiff, 70 } } },
            { "Full Plenum",        A::PipeOrgan,    { { P::po_principal, 100 }, { P::po_flute, 60 }, { P::po_reed, 50 }, { P::po_mixture, 80 }, { P::po_bourdon, 70 } } },
            { "Trumpet Stop",       A::PipeOrgan,    { { P::po_reed, 90 }, { P::po_principal, 30 }, { P::po_mixture, 0 } } },
            { "Accordion",          A::FreeReed,     {} },
            { "Tango Accordion",    A::FreeReed,     { { P::fr_register, 3 }, { P::fr_musette, 15 }, { P::fr_tone, 60 } } },
            { "Harmonica",          A::FreeReed,     { { P::fr_register, 1 }, { P::fr_tone, 85 }, { P::fr_chamber, 0 }, { P::body_mix, 0 } } },
            { "Reed Organ",         A::FreeReed,     { { P::fr_register, 7 }, { P::fr_tone, 25 }, { P::fr_chamber, 60 }, { P::fr_musette, 0 } } },
            // guitars and basses
            { "Nylon Guitar",       A::PluckedString,{} },
            { "Steel Guitar",       A::PluckedString,{ { P::pl_plectrum, 70 }, { P::res_damping, 25 }, { P::body_size, 60 } } },
            { "Jazz Guitar",        A::PluckedString,{ { P::pl_pickup, 20 }, { P::pl_amp, 1 }, { P::pl_drive, 5 }, { P::pl_amptone, 35 }, { P::body_mix, 0 },
                                                       { P::pl_plectrum, 40 }, { P::res_damping, 55 } } },
            { "Clean Electric",     A::PluckedString,{ { P::pl_pickup, 70 }, { P::pl_amp, 1 }, { P::pl_drive, 10 }, { P::pl_amptone, 60 }, { P::body_mix, 0 }, { P::pl_plectrum, 65 } } },
            { "Overdrive Guitar",   A::PluckedString,{ { P::pl_pickup, 80 }, { P::pl_amp, 1 }, { P::pl_drive, 55 }, { P::pl_amptone, 55 }, { P::body_mix, 0 }, { P::pl_plectrum, 75 } } },
            { "Distortion Guitar",  A::PluckedString,{ { P::pl_pickup, 85 }, { P::pl_amp, 1 }, { P::pl_drive, 90 }, { P::pl_amptone, 50 }, { P::body_mix, 0 }, { P::pl_plectrum, 80 } } },
            { "Acoustic Bass",      A::PluckedString,{ { P::pl_plectrum, 20 }, { P::res_damping, 55 }, { P::body_size, 85 }, { P::res_decay, 45 }, { P::octave, -1 } } },
            { "Finger Bass",        A::PluckedString,{ { P::pl_amp, 1 }, { P::pl_drive, 0 }, { P::pl_amptone, 30 }, { P::pl_pickup, 40 }, { P::pl_plectrum, 15 },
                                                       { P::body_mix, 0 }, { P::octave, -1 } } },
            { "Pick Bass",          A::PluckedString,{ { P::pl_amp, 1 }, { P::pl_drive, 8 }, { P::pl_amptone, 45 }, { P::pl_pickup, 55 }, { P::pl_plectrum, 85 },
                                                       { P::body_mix, 0 }, { P::octave, -1 } } },
            { "Harp",               A::PluckedString,{ { P::pl_plectrum, 10 }, { P::pl_ring, 100 }, { P::res_damping, 30 }, { P::res_decay, 70 }, { P::body_size, 60 } } },
            { "Harpsichord",        A::PluckedString,{ { P::pl_plectrum, 100 }, { P::pl_position, 12 }, { P::res_damping, 15 }, { P::pl_keyoff, 60 },
                                                       { P::res_decay, 35 }, { P::body_size, 45 } } },
            { "Clavinet",           A::PluckedString,{ { P::pl_pickup, 75 }, { P::pl_amp, 1 }, { P::pl_drive, 15 }, { P::pl_plectrum, 85 }, { P::res_decay, 20 },
                                                       { P::res_damping, 30 }, { P::body_mix, 0 } } },
            // strings
            { "Violin",             A::BowedString,  {} },
            { "Viola",              A::BowedString,  { { P::body_size, 45 }, { P::bw_position, 45 } } },
            { "Cello",              A::BowedString,  { { P::body_size, 70 }, { P::bw_position, 45 }, { P::vib_depth, 22 } } },
            { "Contrabass",         A::BowedString,  { { P::body_size, 95 }, { P::bw_position, 50 }, { P::vib_depth, 15 }, { P::octave, -1 } } },
            { "String Section",     A::BowedString,  { { P::voice_mode, 0 }, { P::res_unison, 3 }, { P::res_detune, 35 }, { P::vib_depth, 30 }, { P::rev_mix, 30 } } },
            { "Pizzicato Strings",  A::BowedString,  { { P::voice_mode, 0 }, { P::art_select, 7 }, { P::res_unison, 2 }, { P::res_detune, 25 } } },
            // brass
            { "Trumpet",            A::LipReed,      {} },
            { "Trombone",           A::LipReed,      { { P::lr_bore, 60 }, { P::lr_bell, 40 }, { P::port, 25 } } },
            { "French Horn",        A::LipReed,      { { P::lr_bore, 85 }, { P::lr_bell, 20 }, { P::lr_tension, 45 }, { P::rev_mix, 30 } } },
            { "Tuba",               A::LipReed,      { { P::lr_bore, 100 }, { P::lr_bell, 30 }, { P::octave, -1 } } },
            { "Muted Trumpet",      A::LipReed,      { { P::lr_mute, 3 } } },
            { "Brass Section",      A::LipReed,      { { P::voice_mode, 0 }, { P::res_unison, 3 }, { P::res_detune, 30 }, { P::vib_depth, 8 } } },
            // reeds
            { "Clarinet",           A::SingleReed,   {} },
            { "Alto Sax",           A::SingleReed,   { { P::sr_bore, 100 }, { P::vib_depth, 15 }, { P::vib_delay, 40 } } },
            { "Tenor Sax",          A::SingleReed,   { { P::sr_bore, 100 }, { P::sr_subtone, 20 }, { P::vib_depth, 15 }, { P::octave, -1 } } },
            { "Soprano Sax",        A::SingleReed,   { { P::sr_bore, 85 }, { P::vib_depth, 18 } } },
            { "Oboe",               A::DoubleReed,   {} },
            { "English Horn",       A::DoubleReed,   { { P::dr_bore, 35 } } },
            { "Bassoon",            A::DoubleReed,   { { P::dr_bore, 100 }, { P::dr_nasal, 50 }, { P::octave, -1 } } },
            // flutes
            { "Flute",              A::AirJet,       {} },
            { "Recorder",           A::AirJet,       { { P::aj_breath, 15 }, { P::aj_pipe, 20 }, { P::exc_noise, 10 }, { P::exc_transient, 70 }, { P::vib_depth, 5 } } },
            { "Pan Flute",          A::AirJet,       { { P::aj_breath, 70 }, { P::aj_pipe, 60 }, { P::exc_transient, 60 } } },
            { "Piccolo",            A::AirJet,       { { P::octave, 1 }, { P::aj_breath, 20 } } },
            { "Ocarina",            A::AirJet,       { { P::aj_pipe, 100 }, { P::aj_breath, 25 }, { P::vib_depth, 8 } } },
            { "Whistle",            A::AirJet,       { { P::aj_breath, 10 }, { P::exc_hardness, 75 }, { P::vib_depth, 10 } } },
            // synth
            { "Synth Bass 1",       A::Synth,        { { P::octave, -1 }, { P::voice_mode, 1 }, { P::port, 10 } } },
            { "Synth Bass 2",       A::Synth,        { { P::octave, -1 }, { P::voice_mode, 1 }, { P::s1_wave, 1 }, { P::s1_pw, 35 }, { P::s2_wave, 0 }, { P::s2_oct, 0 },
                                                       { P::s2_fine, 7 }, { P::s2_level, 70 }, { P::sy_sub, 35 }, { P::sy_drive, 35 }, { P::sy_cutoff, 30 },
                                                       { P::sy_reso, 45 }, { P::sy_env, 55 }, { P::sy_vel, 45 }, { P::sy_d, 22 }, { P::sy_s, 0 }, { P::sy_r, 15 } } },
            { "Saw Lead",           A::Synth,        { { P::voice_mode, 1 }, { P::s2_wave, 0 }, { P::s2_oct, 0 }, { P::s2_fine, 7 }, { P::s2_level, 80 },
                                                       { P::sy_cutoff, 60 }, { P::sy_reso, 25 }, { P::vib_depth, 15 }, { P::port, 18 } } },
            { "Warm Pad",           A::Synth,        { { P::amp_a, 50 }, { P::amp_r, 60 }, { P::sy_cutoff, 35 }, { P::sy_env, 20 }, { P::sy_s, 60 },
                                                       { P::res_unison, 3 }, { P::res_detune, 30 }, { P::cho_on, 1 }, { P::rev_mix, 30 } } }
        };
        return list;
    }

    inline std::vector<float> factoryValues (const Factory& f)
    {
        auto v = rootValues (f.arch);
        for (const auto& o : f.ov) v[(size_t) o.first] = o.second;
        return v;
    }
}
