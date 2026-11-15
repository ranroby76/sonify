// D:\workspace\Fanan_web_hub\src\timberwolf\TwFactoryPresets.cpp
//==============================================================================
//  The rigs Timber Wolf comes with (the format: TwFactory.h).  Each one's
//  footswitches: the effects it's built round, then preset down / up, tap and
//  the tuner.
//==============================================================================
#include "TwFactory.h"

namespace tw::factory
{
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> list
        {
            { "Clean Machine", R"({
                "chain": [ { "fx": "ax_gate", "k": { "Threshold": -56 } },
                           { "fx": "ax_comp", "k": { "Threshold": -22, "Ratio": 3 } },
                           { "fx": "ax_amp", "k": { "Voice": "CLEAN", "Gain": 32, "Bass": 52, "Mid": 46, "Treble": 58 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "2x12 ALNICO" } },
                           { "fx": "tw_chorus", "off": true, "k": { "Mix": 40 } },
                           { "fx": "ax_space", "k": { "Type": "Plate", "Mix": 22 } } ],
                "switches": [ { "job": "toggle", "fx": [ 2 ] }, { "job": "toggle", "fx": [ 5 ] }, { "job": "toggle", "fx": [ 6 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Blues Crunch", R"({
                "chain": [ { "fx": "ax_gate", "k": { "Threshold": -54 } },
                           { "fx": "klon", "k": { "Gain": 45, "Treble": 55, "Level": 62 } },
                           { "fx": "ax_amp", "k": { "Voice": "CRUNCH", "Gain": 42, "Bass": 48, "Mid": 58, "Treble": 52 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "2x12 ALNICO", "Mic X": 30 } },
                           { "fx": "tw_tape", "off": true, "k": { "Time": 360, "Feedback": 30, "Mix": 25 } },
                           { "fx": "spring", "k": { "Dwell": 45, "Mix": 28 } } ],
                "switches": [ { "job": "toggle", "fx": [ 2 ] }, { "job": "toggle", "fx": [ 5 ] }, { "job": "toggle", "fx": [ 6 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Classic Rock", R"({
                "chain": [ { "fx": "ax_gate", "k": { "Threshold": -50 } },
                           { "fx": "ocd", "k": { "Drive": 40, "Tone": 55, "Volume": 60, "Peak": "High" } },
                           { "fx": "ch_ampsim", "k": { "Gain": 58, "Low": 10, "Mid": 20, "High": 10, "Presence": 60 } },
                           { "fx": "ch_cabsim", "k": { "Cabinet": "Petaluma 4x12" } },
                           { "fx": "tw_tape", "k": { "Time": 420, "Feedback": 28, "Mix": 20 } },
                           { "fx": "plate", "k": { "Mix": 18, "Decay": 55 } } ],
                "switches": [ { "job": "toggle", "fx": [ 2 ] }, { "job": "toggle", "fx": [ 5 ] }, { "job": "toggle", "fx": [ 6 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Metal Tight", R"({
                "chain": [ { "fx": "ax_gate", "k": { "Mode": "Fast", "Threshold": -48, "Depth": 100 } },
                           { "fx": "ch_clipper", "k": { "Type": "Soft", "Boost": 10, "Ceiling": -4 } },
                           { "fx": "ch_chug", "k": { "Crossover": 220, "Amount": 55 } },
                           { "fx": "ax_amp", "k": { "Voice": "MODERN", "Gain": 74, "Bass": 55, "Mid": 40, "Treble": 60, "Tight": "On" } },
                           { "fx": "ax_cab", "k": { "Cabinet": "4x12 MODERN" } },
                           { "fx": "ch_eq560", "k": { "250": -2, "500": -1.5, "4K": 1.5 } },
                           { "fx": "room", "k": { "Size": 35, "Mix": 12 } } ],
                "switches": [ { "job": "toggle", "fx": [ 2 ] }, { "job": "toggle", "fx": [ 3 ] }, { "job": "toggle", "fx": [ 7 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Clean + Lead", R"({
                "chain": [ { "fx": "ax_gate", "k": { "Threshold": -52 } },
                           { "split": { "panA": -45, "panB": 45, "levelB": -2 },
                             "a": [ { "fx": "ax_amp", "k": { "Voice": "CLEAN", "Gain": 30 } }, { "fx": "ax_cab", "k": { "Cabinet": "2x12 ALNICO" } } ],
                             "b": [ { "fx": "ocd", "k": { "Drive": 55, "Volume": 55 } },
                                    { "fx": "ax_amp", "k": { "Voice": "LEAD", "Gain": 62 } }, { "fx": "ax_cab", "k": { "Cabinet": "4x12 VINTAGE" } } ] },
                           { "fx": "tw_pingpong", "k": { "Sync": "1/4 dot", "Feedback": 35, "Mix": 25 } },
                           { "fx": "hall", "k": { "Mix": 20 } } ],
                "snapshots": [ { "name": "Clean", "off": [ 4, 5, 6 ] }, { "name": "Both", "on": [ 4, 5, 6 ] },
                               { "name": "Lead", "off": [ 2, 3 ], "on": [ 4, 5, 6 ] } ],
                "switches": [ { "job": "snapshot", "snapshot": 1 }, { "job": "snapshot", "snapshot": 2 }, { "job": "snapshot", "snapshot": 3 },
                              { "job": "toggle", "fx": [ 7 ] }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Ambient Swells", R"({
                "chain": [ { "fx": "tw_swell", "k": { "Threshold": -40, "Rise": 450 } },
                           { "fx": "tw_chorus", "k": { "Rate": 0.4, "Depth": 40, "Mix": 35 } },
                           { "fx": "ax_amp", "k": { "Voice": "CLEAN", "Gain": 28, "Treble": 55 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "1x12 OPEN" } },
                           { "fx": "tw_tape", "k": { "Sync": "1/4", "Feedback": 55, "Mix": 40, "Wow": 40 } },
                           { "fx": "tw_shimmer", "k": { "Decay": 9, "Shimmer": 45, "Mix": 45 } } ],
                "switches": [ { "job": "toggle", "fx": [ 1 ] }, { "job": "toggle", "fx": [ 5 ] }, { "job": "toggle", "fx": [ 6 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Funk Wah", R"({
                "chain": [ { "fx": "ch_trackcomp", "k": { "Threshold": -26, "Ratio": 5, "Attack": 3, "Release": 120 } },
                           { "fx": "autowah", "k": { "Sensitivity": 6, "Amount": 90 } },
                           { "fx": "ax_amp", "k": { "Voice": "CLEAN", "Gain": 35, "Mid": 55, "Bright": "On" } },
                           { "fx": "ax_cab", "k": { "Cabinet": "2x12 ALNICO" } },
                           { "fx": "phaser", "off": true, "k": { "Rate": 0.3, "Feedback": 40 } },
                           { "fx": "room", "k": { "Mix": 15 } } ],
                "switches": [ { "job": "toggle", "fx": [ 2 ] }, { "job": "toggle", "fx": [ 5 ] }, { "job": "toggle", "fx": [ 1 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Cry Baby Lead", R"({
                "chain": [ { "fx": "ax_gate", "k": { "Threshold": -52 } },
                           { "fx": "crybaby", "k": { "Pedal": 50 } },
                           { "fx": "klon", "k": { "Gain": 60, "Level": 60 } },
                           { "fx": "ax_amp", "k": { "Voice": "LEAD", "Gain": 55 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "4x12 VINTAGE" } },
                           { "fx": "echo", "k": { "Sync": "1/8 dot", "Feedback": 30, "Mix": 22 } } ],
                "pedals": [ { "fx": 2, "knob": "Pedal", "from": 0, "to": 100 } ],
                "switches": [ { "job": "toggle", "fx": [ 2 ] }, { "job": "toggle", "fx": [ 3 ] }, { "job": "toggle", "fx": [ 6 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Whammy Dive", R"({
                "chain": [ { "fx": "ax_gate", "k": { "Threshold": -52 } },
                           { "fx": "tw_whammy", "k": { "Interval": "+1 oct" } },
                           { "fx": "ocd", "k": { "Drive": 62, "Volume": 55 } },
                           { "fx": "ax_amp", "k": { "Voice": "LEAD", "Gain": 60 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "4x12 MODERN" } },
                           { "fx": "echo", "k": { "Time": 330, "Feedback": 30, "Mix": 20 } } ],
                "pedals": [ { "fx": 2, "knob": "Pedal", "from": 0, "to": 100 } ],
                "switches": [ { "job": "toggle", "fx": [ 2 ] }, { "job": "toggle", "fx": [ 3 ] }, { "job": "toggle", "fx": [ 6 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Surf Spring", R"({
                "chain": [ { "fx": "ax_amp", "k": { "Voice": "CHIME", "Gain": 38, "Treble": 62, "Bright": "On" } },
                           { "fx": "ax_cab", "k": { "Cabinet": "1x12 OPEN" } },
                           { "fx": "tw_tremolo", "k": { "Rate": 6.5, "Depth": 55 } },
                           { "fx": "spring", "k": { "Dwell": 70, "Mix": 60, "Tension": 60 } } ],
                "switches": [ { "job": "toggle", "fx": [ 3 ] }, { "job": "toggle", "fx": [ 4 ] }, { "job": "none" },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Psychedelic Vibe", R"({
                "chain": [ { "fx": "univibe", "k": { "Speed": 4.5, "Mode": "Chorus", "Intensity": 75 } },
                           { "fx": "tw_fuzz", "k": { "Fuzz": 75, "Tone": 50, "Octave": 35 } },
                           { "fx": "ax_amp", "k": { "Voice": "CRUNCH", "Gain": 40 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "4x12 VINTAGE" } },
                           { "fx": "tw_tape", "k": { "Time": 480, "Feedback": 40, "Mix": 28, "Wow": 45 } } ],
                "switches": [ { "job": "toggle", "fx": [ 1 ] }, { "job": "toggle", "fx": [ 2 ] }, { "job": "toggle", "fx": [ 5 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Shoegaze Wall", R"({
                "chain": [ { "fx": "tw_fuzz", "k": { "Fuzz": 85, "Tone": 45, "Bias": 40 } },
                           { "fx": "sk_tonestack", "k": { "Stack": "Big Muff", "Bass": 55, "Mid": 40, "Treble": 55 } },
                           { "fx": "flanger", "k": { "Rate": 0.12, "Depth": 70, "Feedback": 40, "Mix": 60 } },
                           { "fx": "ax_amp", "k": { "Voice": "CHIME", "Gain": 45 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "2x12 ALNICO" } },
                           { "fx": "tw_reverse", "k": { "Time": 700, "Feedback": 35, "Mix": 35 } },
                           { "fx": "jpverb", "k": { "Decay": 8, "Size": 2, "Mix": 45 } } ],
                "switches": [ { "job": "toggle", "fx": [ 1 ] }, { "job": "toggle", "fx": [ 3 ] }, { "job": "toggle", "fx": [ 6 ] },
                              { "job": "toggle", "fx": [ 7 ] }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Rotary Organ", R"({
                "chain": [ { "fx": "ax_comp", "k": { "Threshold": -20, "Ratio": 3 } },
                           { "fx": "tw_rotary", "k": { "Speed": "Slow", "Drive": 6 } },
                           { "fx": "ax_amp", "k": { "Voice": "CLEAN", "Gain": 40 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "1x12 OPEN" } },
                           { "fx": "room", "k": { "Mix": 18 } } ],
                "snapshots": [ { "name": "Slow", "k": { "2": { "Speed": "Slow" } } }, { "name": "Fast", "k": { "2": { "Speed": "Fast" } } } ],
                "switches": [ { "job": "snapshot", "snapshot": 1 }, { "job": "snapshot", "snapshot": 2 }, { "job": "toggle", "fx": [ 2 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Octave Fuzz", R"({
                "chain": [ { "fx": "ax_gate", "k": { "Threshold": -50 } },
                           { "fx": "octaver", "k": { "Dry": 0, "Down 1": -4 } },
                           { "fx": "tw_fuzz", "k": { "Fuzz": 80, "Octave": 50 } },
                           { "fx": "ax_amp", "k": { "Voice": "CRUNCH", "Gain": 45 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "4x12 VINTAGE" } } ],
                "switches": [ { "job": "toggle", "fx": [ 2 ] }, { "job": "toggle", "fx": [ 3 ] }, { "job": "none" },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Bi-Amp Crossover", R"({
                "chain": [ { "fx": "ax_gate", "k": { "Threshold": -52 } },
                           { "split": { "type": "crossover", "xover": 600 },
                             "a": [ { "fx": "ax_comp", "k": { "Threshold": -24, "Ratio": 4 } } ],
                             "b": [ { "fx": "klon", "k": { "Gain": 70, "Level": 55 } } ] },
                           { "fx": "ax_amp", "k": { "Voice": "CRUNCH", "Gain": 40 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "2x12 ALNICO" } },
                           { "fx": "plate", "k": { "Mix": 20 } } ],
                "switches": [ { "job": "toggle", "fx": [ 2 ] }, { "job": "toggle", "fx": [ 3 ] }, { "job": "toggle", "fx": [ 6 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Lo-Fi Radio", R"({
                "chain": [ { "fx": "ax_amp", "k": { "Voice": "CLEAN", "Gain": 35 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "1x12 OPEN" } },
                           { "fx": "ch_telephone", "k": { "Squeeze": -35, "Noise": -54 } },
                           { "fx": "ch_crusher", "k": { "Factor": 4, "Bits": 8 } },
                           { "fx": "room", "k": { "Size": 30, "Mix": 15 } } ],
                "switches": [ { "job": "toggle", "fx": [ 3 ] }, { "job": "toggle", "fx": [ 4 ] }, { "job": "toggle", "fx": [ 5 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Granular Clouds", R"({
                "chain": [ { "fx": "ax_amp", "k": { "Voice": "CLEAN", "Gain": 30 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "2x12 ALNICO" } },
                           { "fx": "granular", "k": { "Grain": 140, "Pitch": 12, "Position": 300, "Jitter": 120, "Mix": 40 } },
                           { "fx": "greyhole", "k": { "Delay": 0.35, "Feedback": 85, "Mix": 45 } } ],
                "switches": [ { "job": "toggle", "fx": [ 3 ] }, { "job": "toggle", "fx": [ 4 ] }, { "job": "none" },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },

            { "Stereo Dimension", R"({
                "chain": [ { "fx": "ch_trackcomp", "k": { "Threshold": -20, "Ratio": 3 } },
                           { "fx": "ax_amp", "k": { "Voice": "CHIME", "Gain": 34 } },
                           { "fx": "ax_cab", "k": { "Cabinet": "2x12 ALNICO" } },
                           { "fx": "tw_doubler", "k": { "Mix": 55 } },
                           { "fx": "dimension", "k": { "Size": 45, "Mix": 45 } },
                           { "fx": "plate", "k": { "Mix": 22 } } ],
                "switches": [ { "job": "toggle", "fx": [ 4 ] }, { "job": "toggle", "fx": [ 5 ] }, { "job": "toggle", "fx": [ 6 ] },
                              { "job": "none" }, { "job": "presetDown" }, { "job": "presetUp" }, { "job": "tap" }, { "job": "tuner" } ] })" },
        };

        return list;
    }
}
