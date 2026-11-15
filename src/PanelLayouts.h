// D:\workspace\Fanan_web_hub\src\PanelLayouts.h
// Ready-made panel layouts: where a plugin's window puts its title, groups, controls and
// displays, by the kind of plugin (a synth, a drum machine, a compressor...). Positions, sizes,
// names and parameter roles only: the look comes from a material look and its theme
// (MaterialArchetypes). Fanan AI starts a new window from the nearest one (design_templates,
// kind "layout"), binding its own parameters in place of the roles.

#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace PanelLayouts
{
    struct Entry
    {
        const char* name;
        const char* kind;   // instrument, effect or midi
        const char* json;   // a layout.json (and its "name", "kind" and "description")
    };

    inline const std::vector<Entry>& all()
    {
        static const std::vector<Entry> layouts {
            { "Three Oscillator Synth", "instrument", R"LAYOUT({
 "format": 1,
 "name": "Three Oscillator Synth",
 "kind": "instrument",
 "description": "Three-voice oscillator synthesis with filter shaping, envelopes, dual LFO modulation and a playable keyboard.",
 "width": 1160,
 "height": 740,
 "pages": [
  "Synthesis",
  "Modulation"
 ],
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 860,
   "h": 36,
   "text": "THREE OSCILLATOR SYNTH",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 916,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "tabs",
   "x": 24,
   "y": 64,
   "w": 1112,
   "h": 32
  },
  {
   "type": "keyboard",
   "x": 24,
   "y": 628,
   "w": 1112,
   "h": 88,
   "low": "C2",
   "high": "C6"
  },
  {
   "type": "group",
   "x": 24,
   "y": 112,
   "w": 268,
   "h": 252,
   "text": "OSCILLATOR 1",
   "page": "Synthesis"
  },
  {
   "type": "selector",
   "x": 48,
   "y": 264,
   "param": "osc1.wave",
   "w": 220,
   "h": 64,
   "label": "Wave",
   "options": [
    "Sine",
    "Saw",
    "Pulse",
    "Tri"
   ],
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 76,
   "y": 152,
   "param": "osc1.tune",
   "size": 64,
   "label": "Tune",
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 180,
   "y": 152,
   "param": "osc1.level",
   "size": 64,
   "label": "Level",
   "page": "Synthesis"
  },
  {
   "type": "group",
   "x": 308,
   "y": 112,
   "w": 268,
   "h": 252,
   "text": "OSCILLATOR 2",
   "page": "Synthesis"
  },
  {
   "type": "selector",
   "x": 332,
   "y": 264,
   "param": "osc2.wave",
   "w": 220,
   "h": 64,
   "label": "Wave",
   "options": [
    "Sine",
    "Saw",
    "Pulse",
    "Tri"
   ],
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 360,
   "y": 152,
   "param": "osc2.tune",
   "size": 64,
   "label": "Tune",
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 464,
   "y": 152,
   "param": "osc2.level",
   "size": 64,
   "label": "Level",
   "page": "Synthesis"
  },
  {
   "type": "group",
   "x": 592,
   "y": 112,
   "w": 268,
   "h": 252,
   "text": "OSCILLATOR 3",
   "page": "Synthesis"
  },
  {
   "type": "selector",
   "x": 616,
   "y": 264,
   "param": "osc3.wave",
   "w": 220,
   "h": 64,
   "label": "Wave",
   "options": [
    "Sine",
    "Saw",
    "Pulse",
    "Tri"
   ],
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 644,
   "y": 152,
   "param": "osc3.tune",
   "size": 64,
   "label": "Tune",
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 748,
   "y": 152,
   "param": "osc3.level",
   "size": 64,
   "label": "Level",
   "page": "Synthesis"
  },
  {
   "type": "group",
   "x": 876,
   "y": 112,
   "w": 260,
   "h": 252,
   "text": "NOISE / MASTER",
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 904,
   "y": 152,
   "param": "noise.level",
   "size": 72,
   "label": "Noise",
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 1024,
   "y": 152,
   "param": "master.volume",
   "size": 72,
   "label": "Master",
   "page": "Synthesis"
  },
  {
   "type": "selector",
   "x": 900,
   "y": 272,
   "param": "voice.mode",
   "w": 212,
   "h": 64,
   "label": "Voices",
   "options": [
    "Mono",
    "Legato",
    "Poly"
   ],
   "page": "Synthesis"
  },
  {
   "type": "group",
   "x": 24,
   "y": 380,
   "w": 560,
   "h": 224,
   "text": "FILTER",
   "page": "Synthesis"
  },
  {
   "type": "dropdown",
   "x": 48,
   "y": 424,
   "param": "filter.type",
   "w": 140,
   "h": 52,
   "label": "Type",
   "options": [
    "Lowpass",
    "Bandpass",
    "Highpass",
    "Notch"
   ],
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 208,
   "y": 420,
   "param": "filter.cutoff",
   "size": 88,
   "label": "Cutoff",
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 320,
   "y": 432,
   "param": "filter.resonance",
   "size": 64,
   "label": "Resonance",
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 408,
   "y": 432,
   "param": "filter.env_amount",
   "size": 64,
   "label": "Env Amt",
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 496,
   "y": 432,
   "param": "filter.key_track",
   "size": 64,
   "label": "Key Track",
   "page": "Synthesis"
  },
  {
   "type": "group",
   "x": 600,
   "y": 380,
   "w": 260,
   "h": 224,
   "text": "PERFORMANCE",
   "page": "Synthesis"
  },
  {
   "type": "knob",
   "x": 628,
   "y": 424,
   "param": "voice.glide",
   "size": 80,
   "label": "Glide",
   "page": "Synthesis"
  },
  {
   "type": "selector",
   "x": 732,
   "y": 432,
   "param": "voice.glide_mode",
   "w": 104,
   "h": 76,
   "label": "Glide Mode",
   "options": [
    "Off",
    "On",
    "Auto"
   ],
   "page": "Synthesis"
  },
  {
   "type": "group",
   "x": 876,
   "y": 380,
   "w": 260,
   "h": 224,
   "text": "OSCILLOSCOPE",
   "page": "Synthesis"
  },
  {
   "type": "scope",
   "x": 896,
   "y": 420,
   "source": "plugin.out L",
   "w": 220,
   "h": 156,
   "label": "Output Wave",
   "page": "Synthesis"
  },
  {
   "type": "group",
   "x": 24,
   "y": 112,
   "w": 544,
   "h": 220,
   "text": "FILTER ENVELOPE",
   "page": "Modulation"
  },
  {
   "type": "envelope",
   "x": 48,
   "y": 156,
   "params": {
    "attack": "filterenv.attack",
    "decay": "filterenv.decay",
    "sustain": "filterenv.sustain",
    "release": "filterenv.release"
   },
   "w": 496,
   "h": 144,
   "label": "Filter ADSR",
   "page": "Modulation"
  },
  {
   "type": "group",
   "x": 584,
   "y": 112,
   "w": 552,
   "h": 220,
   "text": "AMP ENVELOPE",
   "page": "Modulation"
  },
  {
   "type": "envelope",
   "x": 608,
   "y": 156,
   "params": {
    "attack": "amp.attack",
    "decay": "amp.decay",
    "sustain": "amp.sustain",
    "release": "amp.release"
   },
   "w": 504,
   "h": 144,
   "label": "Amplifier ADSR",
   "page": "Modulation"
  },
  {
   "type": "group",
   "x": 24,
   "y": 348,
   "w": 544,
   "h": 256,
   "text": "LFO 1",
   "page": "Modulation"
  },
  {
   "type": "selector",
   "x": 48,
   "y": 396,
   "param": "lfo1.shape",
   "w": 180,
   "h": 64,
   "label": "Shape",
   "options": [
    "Sine",
    "Tri",
    "Saw",
    "Square"
   ],
   "page": "Modulation"
  },
  {
   "type": "knob",
   "x": 268,
   "y": 392,
   "param": "lfo1.rate",
   "size": 72,
   "label": "Rate",
   "page": "Modulation"
  },
  {
   "type": "knob",
   "x": 400,
   "y": 392,
   "param": "lfo1.depth",
   "size": 72,
   "label": "Depth",
   "page": "Modulation"
  },
  {
   "type": "group",
   "x": 584,
   "y": 348,
   "w": 552,
   "h": 256,
   "text": "LFO 2",
   "page": "Modulation"
  },
  {
   "type": "selector",
   "x": 608,
   "y": 396,
   "param": "lfo2.shape",
   "w": 180,
   "h": 64,
   "label": "Shape",
   "options": [
    "Sine",
    "Tri",
    "Saw",
    "Square"
   ],
   "page": "Modulation"
  },
  {
   "type": "knob",
   "x": 828,
   "y": 392,
   "param": "lfo2.rate",
   "size": 72,
   "label": "Rate",
   "page": "Modulation"
  },
  {
   "type": "knob",
   "x": 960,
   "y": 392,
   "param": "lfo2.depth",
   "size": 72,
   "label": "Depth",
   "page": "Modulation"
  }
 ]
})LAYOUT" },
            { "Mono Bass Synth", "instrument", R"LAYOUT({
 "format": 1,
 "name": "Mono Bass Synth",
 "kind": "instrument",
 "description": "Compact analog-style bass synthesis with dual oscillators, sub tone, driven filter and slide performance.",
 "width": 912,
 "height": 620,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 612,
   "h": 36,
   "text": "MONO BASS SYNTH",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 668,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "keyboard",
   "x": 24,
   "y": 508,
   "w": 864,
   "h": 88,
   "low": "C1",
   "high": "C5"
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 292,
   "h": 392,
   "text": "OSCILLATORS"
  },
  {
   "type": "selector",
   "x": 48,
   "y": 128,
   "param": "osc1.wave",
   "w": 96,
   "h": 64,
   "label": "Osc 1 Wave",
   "options": [
    "Saw",
    "Pulse",
    "Tri"
   ]
  },
  {
   "type": "knob",
   "x": 164,
   "y": 120,
   "param": "osc1.tune",
   "size": 56,
   "label": "Tune 1"
  },
  {
   "type": "knob",
   "x": 240,
   "y": 120,
   "param": "osc1.level",
   "size": 56,
   "label": "Level 1"
  },
  {
   "type": "selector",
   "x": 48,
   "y": 240,
   "param": "osc2.wave",
   "w": 96,
   "h": 64,
   "label": "Osc 2 Wave",
   "options": [
    "Saw",
    "Pulse",
    "Tri"
   ]
  },
  {
   "type": "knob",
   "x": 164,
   "y": 232,
   "param": "osc2.tune",
   "size": 56,
   "label": "Tune 2"
  },
  {
   "type": "knob",
   "x": 240,
   "y": 232,
   "param": "osc2.level",
   "size": 56,
   "label": "Level 2"
  },
  {
   "type": "knob",
   "x": 164,
   "y": 360,
   "param": "sub.level",
   "size": 56,
   "label": "Sub"
  },
  {
   "type": "group",
   "x": 332,
   "y": 76,
   "w": 336,
   "h": 392,
   "text": "FILTER / ENVELOPE"
  },
  {
   "type": "selector",
   "x": 356,
   "y": 116,
   "param": "filter.type",
   "w": 288,
   "h": 52,
   "label": "Filter",
   "options": [
    "LP",
    "BP",
    "HP"
   ]
  },
  {
   "type": "knob",
   "x": 356,
   "y": 200,
   "param": "filter.cutoff",
   "size": 96,
   "label": "Cutoff"
  },
  {
   "type": "knob",
   "x": 480,
   "y": 208,
   "param": "filter.resonance",
   "size": 64,
   "label": "Resonance"
  },
  {
   "type": "knob",
   "x": 572,
   "y": 208,
   "param": "filter.drive",
   "size": 64,
   "label": "Drive"
  },
  {
   "type": "envelope",
   "x": 356,
   "y": 340,
   "params": {
    "attack": "amp.attack",
    "decay": "amp.decay",
    "sustain": "amp.sustain",
    "release": "amp.release"
   },
   "w": 288,
   "h": 100,
   "label": "Amplitude ADSR"
  },
  {
   "type": "group",
   "x": 684,
   "y": 76,
   "w": 204,
   "h": 392,
   "text": "PERFORMANCE"
  },
  {
   "type": "knob",
   "x": 704,
   "y": 128,
   "param": "perf.accent",
   "size": 64,
   "label": "Accent"
  },
  {
   "type": "knob",
   "x": 796,
   "y": 128,
   "param": "perf.slide",
   "size": 64,
   "label": "Slide"
  },
  {
   "type": "knob",
   "x": 744,
   "y": 256,
   "param": "master.volume",
   "size": 72,
   "label": "Master"
  }
 ]
})LAYOUT" },
            { "Pad Synth", "instrument", R"LAYOUT({
 "format": 1,
 "name": "Pad Synth",
 "kind": "instrument",
 "description": "Layered pad synthesizer with oscillator morphing, spacious unison, slow envelopes and ambient effects.",
 "width": 1120,
 "height": 712,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 820,
   "h": 36,
   "text": "PAD SYNTH",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 876,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 328,
   "h": 240,
   "text": "OSCILLATOR 1"
  },
  {
   "type": "selector",
   "x": 48,
   "y": 120,
   "param": "osc1.wave",
   "w": 136,
   "h": 64,
   "label": "Wave",
   "options": [
    "Sine",
    "Saw",
    "Tri",
    "Pulse"
   ]
  },
  {
   "type": "knob",
   "x": 208,
   "y": 120,
   "param": "osc1.tune",
   "size": 56,
   "label": "Tune"
  },
  {
   "type": "knob",
   "x": 280,
   "y": 120,
   "param": "osc1.level",
   "size": 56,
   "label": "Level"
  },
  {
   "type": "group",
   "x": 368,
   "y": 76,
   "w": 328,
   "h": 240,
   "text": "OSCILLATOR 2"
  },
  {
   "type": "selector",
   "x": 392,
   "y": 120,
   "param": "osc2.wave",
   "w": 136,
   "h": 64,
   "label": "Wave",
   "options": [
    "Sine",
    "Saw",
    "Tri",
    "Pulse"
   ]
  },
  {
   "type": "knob",
   "x": 552,
   "y": 120,
   "param": "osc2.tune",
   "size": 56,
   "label": "Tune"
  },
  {
   "type": "knob",
   "x": 624,
   "y": 120,
   "param": "osc2.level",
   "size": 56,
   "label": "Level"
  },
  {
   "type": "group",
   "x": 712,
   "y": 76,
   "w": 384,
   "h": 240,
   "text": "MORPH / SPREAD"
  },
  {
   "type": "knob",
   "x": 752,
   "y": 128,
   "param": "osc.morph",
   "size": 112,
   "label": "Morph"
  },
  {
   "type": "knob",
   "x": 892,
   "y": 132,
   "param": "unison.voices",
   "size": 64,
   "label": "Unison"
  },
  {
   "type": "knob",
   "x": 988,
   "y": 132,
   "param": "unison.spread",
   "size": 64,
   "label": "Spread"
  },
  {
   "type": "group",
   "x": 24,
   "y": 336,
   "w": 344,
   "h": 232,
   "text": "FILTER"
  },
  {
   "type": "knob",
   "x": 48,
   "y": 384,
   "param": "filter.cutoff",
   "size": 84,
   "label": "Cutoff"
  },
  {
   "type": "knob",
   "x": 160,
   "y": 396,
   "param": "filter.resonance",
   "size": 64,
   "label": "Resonance"
  },
  {
   "type": "dropdown",
   "x": 248,
   "y": 392,
   "param": "filter.type",
   "w": 96,
   "h": 52,
   "label": "Mode",
   "options": [
    "Lowpass",
    "Bandpass",
    "Highpass"
   ]
  },
  {
   "type": "group",
   "x": 384,
   "y": 336,
   "w": 344,
   "h": 232,
   "text": "FILTER ENVELOPE"
  },
  {
   "type": "envelope",
   "x": 408,
   "y": 380,
   "params": {
    "attack": "filterenv.attack",
    "decay": "filterenv.decay",
    "sustain": "filterenv.sustain",
    "release": "filterenv.release"
   },
   "w": 296,
   "h": 160,
   "label": "Slow Filter ADSR"
  },
  {
   "type": "group",
   "x": 744,
   "y": 336,
   "w": 352,
   "h": 232,
   "text": "AMP ENVELOPE"
  },
  {
   "type": "envelope",
   "x": 768,
   "y": 380,
   "params": {
    "attack": "amp.attack",
    "decay": "amp.decay",
    "sustain": "amp.sustain",
    "release": "amp.release"
   },
   "w": 304,
   "h": 160,
   "label": "Slow Amp ADSR"
  },
  {
   "type": "group",
   "x": 24,
   "y": 584,
   "w": 1072,
   "h": 116,
   "text": "ATMOSPHERE"
  },
  {
   "type": "knob",
   "x": 64,
   "y": 616,
   "param": "fx.chorus",
   "size": 48,
   "label": "Chorus"
  },
  {
   "type": "knob",
   "x": 208,
   "y": 616,
   "param": "fx.reverb_send",
   "size": 48,
   "label": "Reverb Send"
  },
  {
   "type": "knob",
   "x": 352,
   "y": 616,
   "param": "master.volume",
   "size": 48,
   "label": "Master"
  }
 ]
})LAYOUT" },
            { "Drum Machine", "instrument", R"LAYOUT({
 "format": 1,
 "name": "Drum Machine",
 "kind": "instrument",
 "description": "Eight parallel drum voices with per-voice tuning, decay, level and panorama plus master processing.",
 "width": 1200,
 "height": 744,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 900,
   "h": 36,
   "text": "DRUM MACHINE",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 956,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 132,
   "h": 488,
   "text": "VOICE 1"
  },
  {
   "type": "knob",
   "x": 64,
   "y": 120,
   "param": "drum1.tune",
   "size": 52,
   "label": "Tune"
  },
  {
   "type": "knob",
   "x": 64,
   "y": 220,
   "param": "drum1.decay",
   "size": 52,
   "label": "Decay"
  },
  {
   "type": "knob",
   "x": 64,
   "y": 320,
   "param": "drum1.level",
   "size": 52,
   "label": "Level"
  },
  {
   "type": "knob",
   "x": 64,
   "y": 420,
   "param": "drum1.pan",
   "size": 52,
   "label": "Pan"
  },
  {
   "type": "group",
   "x": 168,
   "y": 76,
   "w": 132,
   "h": 488,
   "text": "VOICE 2"
  },
  {
   "type": "knob",
   "x": 208,
   "y": 120,
   "param": "drum2.tune",
   "size": 52,
   "label": "Tune"
  },
  {
   "type": "knob",
   "x": 208,
   "y": 220,
   "param": "drum2.decay",
   "size": 52,
   "label": "Decay"
  },
  {
   "type": "knob",
   "x": 208,
   "y": 320,
   "param": "drum2.level",
   "size": 52,
   "label": "Level"
  },
  {
   "type": "knob",
   "x": 208,
   "y": 420,
   "param": "drum2.pan",
   "size": 52,
   "label": "Pan"
  },
  {
   "type": "group",
   "x": 312,
   "y": 76,
   "w": 132,
   "h": 488,
   "text": "VOICE 3"
  },
  {
   "type": "knob",
   "x": 352,
   "y": 120,
   "param": "drum3.tune",
   "size": 52,
   "label": "Tune"
  },
  {
   "type": "knob",
   "x": 352,
   "y": 220,
   "param": "drum3.decay",
   "size": 52,
   "label": "Decay"
  },
  {
   "type": "knob",
   "x": 352,
   "y": 320,
   "param": "drum3.level",
   "size": 52,
   "label": "Level"
  },
  {
   "type": "knob",
   "x": 352,
   "y": 420,
   "param": "drum3.pan",
   "size": 52,
   "label": "Pan"
  },
  {
   "type": "group",
   "x": 456,
   "y": 76,
   "w": 132,
   "h": 488,
   "text": "VOICE 4"
  },
  {
   "type": "knob",
   "x": 496,
   "y": 120,
   "param": "drum4.tune",
   "size": 52,
   "label": "Tune"
  },
  {
   "type": "knob",
   "x": 496,
   "y": 220,
   "param": "drum4.decay",
   "size": 52,
   "label": "Decay"
  },
  {
   "type": "knob",
   "x": 496,
   "y": 320,
   "param": "drum4.level",
   "size": 52,
   "label": "Level"
  },
  {
   "type": "knob",
   "x": 496,
   "y": 420,
   "param": "drum4.pan",
   "size": 52,
   "label": "Pan"
  },
  {
   "type": "group",
   "x": 600,
   "y": 76,
   "w": 132,
   "h": 488,
   "text": "VOICE 5"
  },
  {
   "type": "knob",
   "x": 640,
   "y": 120,
   "param": "drum5.tune",
   "size": 52,
   "label": "Tune"
  },
  {
   "type": "knob",
   "x": 640,
   "y": 220,
   "param": "drum5.decay",
   "size": 52,
   "label": "Decay"
  },
  {
   "type": "knob",
   "x": 640,
   "y": 320,
   "param": "drum5.level",
   "size": 52,
   "label": "Level"
  },
  {
   "type": "knob",
   "x": 640,
   "y": 420,
   "param": "drum5.pan",
   "size": 52,
   "label": "Pan"
  },
  {
   "type": "group",
   "x": 744,
   "y": 76,
   "w": 132,
   "h": 488,
   "text": "VOICE 6"
  },
  {
   "type": "knob",
   "x": 784,
   "y": 120,
   "param": "drum6.tune",
   "size": 52,
   "label": "Tune"
  },
  {
   "type": "knob",
   "x": 784,
   "y": 220,
   "param": "drum6.decay",
   "size": 52,
   "label": "Decay"
  },
  {
   "type": "knob",
   "x": 784,
   "y": 320,
   "param": "drum6.level",
   "size": 52,
   "label": "Level"
  },
  {
   "type": "knob",
   "x": 784,
   "y": 420,
   "param": "drum6.pan",
   "size": 52,
   "label": "Pan"
  },
  {
   "type": "group",
   "x": 888,
   "y": 76,
   "w": 132,
   "h": 488,
   "text": "VOICE 7"
  },
  {
   "type": "knob",
   "x": 928,
   "y": 120,
   "param": "drum7.tune",
   "size": 52,
   "label": "Tune"
  },
  {
   "type": "knob",
   "x": 928,
   "y": 220,
   "param": "drum7.decay",
   "size": 52,
   "label": "Decay"
  },
  {
   "type": "knob",
   "x": 928,
   "y": 320,
   "param": "drum7.level",
   "size": 52,
   "label": "Level"
  },
  {
   "type": "knob",
   "x": 928,
   "y": 420,
   "param": "drum7.pan",
   "size": 52,
   "label": "Pan"
  },
  {
   "type": "group",
   "x": 1032,
   "y": 76,
   "w": 132,
   "h": 488,
   "text": "VOICE 8"
  },
  {
   "type": "knob",
   "x": 1072,
   "y": 120,
   "param": "drum8.tune",
   "size": 52,
   "label": "Tune"
  },
  {
   "type": "knob",
   "x": 1072,
   "y": 220,
   "param": "drum8.decay",
   "size": 52,
   "label": "Decay"
  },
  {
   "type": "knob",
   "x": 1072,
   "y": 320,
   "param": "drum8.level",
   "size": 52,
   "label": "Level"
  },
  {
   "type": "knob",
   "x": 1072,
   "y": 420,
   "param": "drum8.pan",
   "size": 52,
   "label": "Pan"
  },
  {
   "type": "group",
   "x": 24,
   "y": 580,
   "w": 1152,
   "h": 140,
   "text": "MASTER BUS"
  },
  {
   "type": "knob",
   "x": 64,
   "y": 624,
   "param": "master.volume",
   "size": 64,
   "label": "Master"
  },
  {
   "type": "knob",
   "x": 172,
   "y": 624,
   "param": "master.drive",
   "size": 64,
   "label": "Drive"
  },
  {
   "type": "knob",
   "x": 280,
   "y": 624,
   "param": "master.swing",
   "size": 64,
   "label": "Swing"
  },
  {
   "type": "meter",
   "x": 1008,
   "y": 620,
   "source": "plugin.out L",
   "w": 140,
   "h": 80,
   "label": "Master Out",
   "settings": {
    "display": "segments"
   }
  }
 ]
})LAYOUT" },
            { "Sampler", "instrument", R"LAYOUT({
 "format": 1,
 "name": "Sampler",
 "kind": "instrument",
 "description": "Waveform-centred sample playback with loop shaping, root-note mapping, filtering and envelope control.",
 "width": 1080,
 "height": 700,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 780,
   "h": 36,
   "text": "SAMPLER",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 836,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 1032,
   "h": 220,
   "text": "SAMPLE WAVEFORM"
  },
  {
   "type": "scope",
   "x": 48,
   "y": 116,
   "source": "plugin.out L",
   "w": 984,
   "h": 152,
   "label": "Sample View"
  },
  {
   "type": "group",
   "x": 24,
   "y": 312,
   "w": 544,
   "h": 232,
   "text": "PLAYBACK / LOOP"
  },
  {
   "type": "knob",
   "x": 48,
   "y": 356,
   "param": "sample.start",
   "size": 64,
   "label": "Start"
  },
  {
   "type": "knob",
   "x": 148,
   "y": 356,
   "param": "sample.end",
   "size": 64,
   "label": "End"
  },
  {
   "type": "knob",
   "x": 248,
   "y": 356,
   "param": "sample.loop",
   "size": 64,
   "label": "Loop"
  },
  {
   "type": "dropdown",
   "x": 376,
   "y": 360,
   "param": "sample.root",
   "w": 160,
   "h": 56,
   "label": "Root Note",
   "options": [
    "C1",
    "C2",
    "C3",
    "C4",
    "C5",
    "C6"
   ]
  },
  {
   "type": "envelope",
   "x": 372,
   "y": 440,
   "params": {
    "attack": "amp.attack",
    "decay": "amp.decay",
    "sustain": "amp.sustain",
    "release": "amp.release"
   },
   "w": 168,
   "h": 80,
   "label": "Amp ADSR"
  },
  {
   "type": "group",
   "x": 584,
   "y": 312,
   "w": 472,
   "h": 232,
   "text": "FILTER"
  },
  {
   "type": "knob",
   "x": 608,
   "y": 356,
   "param": "filter.cutoff",
   "size": 96,
   "label": "Cutoff"
  },
  {
   "type": "knob",
   "x": 736,
   "y": 368,
   "param": "filter.resonance",
   "size": 64,
   "label": "Resonance"
  },
  {
   "type": "dropdown",
   "x": 848,
   "y": 360,
   "param": "filter.type",
   "w": 176,
   "h": 56,
   "label": "Type",
   "options": [
    "Lowpass",
    "Bandpass",
    "Highpass"
   ]
  },
  {
   "type": "keyboard",
   "x": 24,
   "y": 584,
   "w": 1032,
   "h": 88,
   "low": "C2",
   "high": "C6"
  }
 ]
})LAYOUT" },
            { "Arpeggiator", "midi", R"LAYOUT({
 "format": 1,
 "name": "Arpeggiator",
 "kind": "midi",
 "description": "Pattern-based arpeggiator with musical timing, latch control and a sixteen-step indicator row.",
 "width": 900,
 "height": 460,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 600,
   "h": 36,
   "text": "ARPEGGIATOR",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 656,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 440,
   "h": 180,
   "text": "PATTERN / TIMING"
  },
  {
   "type": "selector",
   "x": 48,
   "y": 120,
   "param": "arp.mode",
   "w": 176,
   "h": 68,
   "label": "Mode",
   "options": [
    "Up",
    "Down",
    "Alt",
    "Random"
   ]
  },
  {
   "type": "knob",
   "x": 248,
   "y": 120,
   "param": "arp.rate",
   "size": 60,
   "label": "Rate"
  },
  {
   "type": "knob",
   "x": 340,
   "y": 120,
   "param": "arp.gate",
   "size": 60,
   "label": "Gate"
  },
  {
   "type": "group",
   "x": 480,
   "y": 76,
   "w": 396,
   "h": 180,
   "text": "PLAY STYLE"
  },
  {
   "type": "knob",
   "x": 504,
   "y": 120,
   "param": "arp.octaves",
   "size": 60,
   "label": "Octaves"
  },
  {
   "type": "knob",
   "x": 592,
   "y": 120,
   "param": "arp.swing",
   "size": 60,
   "label": "Swing"
  },
  {
   "type": "switch",
   "x": 704,
   "y": 124,
   "param": "arp.latch",
   "w": 72,
   "h": 64,
   "label": "Latch"
  },
  {
   "type": "group",
   "x": 24,
   "y": 276,
   "w": 852,
   "h": 156,
   "text": "16-STEP ACTIVITY"
  },
  {
   "type": "lamp",
   "x": 48,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step1"
  },
  {
   "type": "lamp",
   "x": 96,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step2"
  },
  {
   "type": "lamp",
   "x": 144,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step3"
  },
  {
   "type": "lamp",
   "x": 192,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step4"
  },
  {
   "type": "lamp",
   "x": 240,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step5"
  },
  {
   "type": "lamp",
   "x": 288,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step6"
  },
  {
   "type": "lamp",
   "x": 336,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step7"
  },
  {
   "type": "lamp",
   "x": 384,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step8"
  },
  {
   "type": "lamp",
   "x": 432,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step9"
  },
  {
   "type": "lamp",
   "x": 480,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step10"
  },
  {
   "type": "lamp",
   "x": 528,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step11"
  },
  {
   "type": "lamp",
   "x": 576,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step12"
  },
  {
   "type": "lamp",
   "x": 624,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step13"
  },
  {
   "type": "lamp",
   "x": 672,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step14"
  },
  {
   "type": "lamp",
   "x": 720,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step15"
  },
  {
   "type": "lamp",
   "x": 768,
   "y": 328,
   "w": 20,
   "h": 20,
   "source": "arp.step16"
  }
 ]
})LAYOUT" },
            { "Compressor", "effect", R"LAYOUT({
 "format": 1,
 "name": "Compressor",
 "kind": "effect",
 "description": "Studio compressor with precision dynamics controls and separate input, gain-reduction and output meters.",
 "width": 1040,
 "height": 552,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 740,
   "h": 36,
   "text": "COMPRESSOR",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 796,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 608,
   "h": 304,
   "text": "DYNAMICS"
  },
  {
   "type": "knob",
   "x": 48,
   "y": 128,
   "param": "comp.threshold",
   "size": 112,
   "label": "Threshold"
  },
  {
   "type": "knob",
   "x": 200,
   "y": 144,
   "param": "comp.ratio",
   "size": 72,
   "label": "Ratio"
  },
  {
   "type": "knob",
   "x": 304,
   "y": 144,
   "param": "comp.attack",
   "size": 72,
   "label": "Attack"
  },
  {
   "type": "knob",
   "x": 408,
   "y": 144,
   "param": "comp.release",
   "size": 72,
   "label": "Release"
  },
  {
   "type": "knob",
   "x": 512,
   "y": 144,
   "param": "comp.knee",
   "size": 72,
   "label": "Knee"
  },
  {
   "type": "group",
   "x": 648,
   "y": 76,
   "w": 368,
   "h": 304,
   "text": "OUTPUT / BLEND"
  },
  {
   "type": "knob",
   "x": 680,
   "y": 136,
   "param": "comp.makeup",
   "size": 72,
   "label": "Makeup"
  },
  {
   "type": "knob",
   "x": 792,
   "y": 136,
   "param": "comp.mix",
   "size": 72,
   "label": "Mix"
  },
  {
   "type": "switch",
   "x": 912,
   "y": 144,
   "param": "comp.auto_gain",
   "w": 72,
   "h": 64,
   "label": "Auto"
  },
  {
   "type": "group",
   "x": 24,
   "y": 400,
   "w": 992,
   "h": 128,
   "text": "LEVEL MONITORING"
  },
  {
   "type": "meter",
   "x": 48,
   "y": 440,
   "source": "plugin.in L",
   "w": 280,
   "h": 68,
   "label": "Input",
   "settings": {
    "display": "segments"
   }
  },
  {
   "type": "meter",
   "x": 376,
   "y": 440,
   "source": "comp.gr",
   "w": 280,
   "h": 68,
   "label": "Gain Reduction",
   "settings": {
    "display": "segments"
   }
  },
  {
   "type": "meter",
   "x": 704,
   "y": 440,
   "source": "plugin.out L",
   "w": 280,
   "h": 68,
   "label": "Output",
   "settings": {
    "display": "segments"
   }
  }
 ]
})LAYOUT" },
            { "Four Band EQ", "effect", R"LAYOUT({
 "format": 1,
 "name": "Four Band EQ",
 "kind": "effect",
 "description": "Four-band equalizer with shelving ends, two parametric bells, a response curve and output gain.",
 "width": 1200,
 "height": 660,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 900,
   "h": 36,
   "text": "FOUR BAND EQ",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 956,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 1152,
   "h": 172,
   "text": "FREQUENCY RESPONSE"
  },
  {
   "type": "filter",
   "x": 48,
   "y": 116,
   "params": {
    "cutoff": "eq.display.cutoff",
    "resonance": "eq.display.resonance"
   },
   "mode": "bandpass",
   "w": 1104,
   "h": 112,
   "label": "EQ Curve"
  },
  {
   "type": "group",
   "x": 24,
   "y": 264,
   "w": 270,
   "h": 240,
   "text": "LOW SHELF"
  },
  {
   "type": "knob",
   "x": 44,
   "y": 308,
   "param": "eq.low.freq",
   "size": 64,
   "label": "Frequency"
  },
  {
   "type": "knob",
   "x": 128,
   "y": 308,
   "param": "eq.low.gain",
   "size": 64,
   "label": "Gain"
  },
  {
   "type": "knob",
   "x": 212,
   "y": 308,
   "param": "eq.low.q",
   "size": 64,
   "label": "Q"
  },
  {
   "type": "group",
   "x": 312,
   "y": 264,
   "w": 270,
   "h": 240,
   "text": "LOW-MID BELL"
  },
  {
   "type": "knob",
   "x": 332,
   "y": 308,
   "param": "eq.lowmid.freq",
   "size": 64,
   "label": "Frequency"
  },
  {
   "type": "knob",
   "x": 416,
   "y": 308,
   "param": "eq.lowmid.gain",
   "size": 64,
   "label": "Gain"
  },
  {
   "type": "knob",
   "x": 500,
   "y": 308,
   "param": "eq.lowmid.q",
   "size": 64,
   "label": "Q"
  },
  {
   "type": "group",
   "x": 600,
   "y": 264,
   "w": 270,
   "h": 240,
   "text": "HIGH-MID BELL"
  },
  {
   "type": "knob",
   "x": 620,
   "y": 308,
   "param": "eq.highmid.freq",
   "size": 64,
   "label": "Frequency"
  },
  {
   "type": "knob",
   "x": 704,
   "y": 308,
   "param": "eq.highmid.gain",
   "size": 64,
   "label": "Gain"
  },
  {
   "type": "knob",
   "x": 788,
   "y": 308,
   "param": "eq.highmid.q",
   "size": 64,
   "label": "Q"
  },
  {
   "type": "group",
   "x": 888,
   "y": 264,
   "w": 288,
   "h": 240,
   "text": "HIGH SHELF"
  },
  {
   "type": "knob",
   "x": 908,
   "y": 308,
   "param": "eq.high.freq",
   "size": 64,
   "label": "Frequency"
  },
  {
   "type": "knob",
   "x": 992,
   "y": 308,
   "param": "eq.high.gain",
   "size": 64,
   "label": "Gain"
  },
  {
   "type": "knob",
   "x": 1076,
   "y": 308,
   "param": "eq.high.q",
   "size": 64,
   "label": "Q"
  },
  {
   "type": "group",
   "x": 24,
   "y": 520,
   "w": 1152,
   "h": 112,
   "text": "OUTPUT TRIM"
  },
  {
   "type": "slider",
   "x": 48,
   "y": 556,
   "param": "eq.output",
   "w": 320,
   "h": 52,
   "label": "Output Level",
   "orientation": "horizontal"
  }
 ]
})LAYOUT" },
            { "Reverb", "effect", R"LAYOUT({
 "format": 1,
 "name": "Reverb",
 "kind": "effect",
 "description": "Spatial reverb with room size, tail shaping, stereo width, wet blend and output scope.",
 "width": 912,
 "height": 512,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 612,
   "h": 36,
   "text": "REVERB",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 668,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 608,
   "h": 252,
   "text": "REVERBERATION"
  },
  {
   "type": "knob",
   "x": 48,
   "y": 128,
   "param": "reverb.size",
   "size": 104,
   "label": "Room Size"
  },
  {
   "type": "knob",
   "x": 176,
   "y": 140,
   "param": "reverb.decay",
   "size": 72,
   "label": "Decay"
  },
  {
   "type": "knob",
   "x": 272,
   "y": 140,
   "param": "reverb.predelay",
   "size": 72,
   "label": "Pre-delay"
  },
  {
   "type": "knob",
   "x": 368,
   "y": 140,
   "param": "reverb.damping",
   "size": 72,
   "label": "Damping"
  },
  {
   "type": "knob",
   "x": 464,
   "y": 140,
   "param": "reverb.width",
   "size": 72,
   "label": "Width"
  },
  {
   "type": "group",
   "x": 648,
   "y": 76,
   "w": 240,
   "h": 252,
   "text": "WET / DRY"
  },
  {
   "type": "knob",
   "x": 704,
   "y": 128,
   "param": "reverb.mix",
   "size": 104,
   "label": "Mix"
  },
  {
   "type": "group",
   "x": 24,
   "y": 344,
   "w": 864,
   "h": 140,
   "text": "OUTPUT TAIL"
  },
  {
   "type": "scope",
   "x": 48,
   "y": 384,
   "source": "plugin.out L",
   "w": 816,
   "h": 84,
   "label": "Tail Waveform"
  }
 ]
})LAYOUT" },
            { "Delay", "effect", R"LAYOUT({
 "format": 1,
 "name": "Delay",
 "kind": "effect",
 "description": "Tempo-synchronized delay with division selection, feedback, ping-pong routing and filtering.",
 "width": 900,
 "height": 392,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 600,
   "h": 36,
   "text": "DELAY",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 656,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 408,
   "h": 280,
   "text": "TIMING"
  },
  {
   "type": "knob",
   "x": 56,
   "y": 128,
   "param": "delay.time",
   "size": 104,
   "label": "Time"
  },
  {
   "type": "switch",
   "x": 196,
   "y": 140,
   "param": "delay.sync",
   "w": 64,
   "h": 64,
   "label": "Sync"
  },
  {
   "type": "dropdown",
   "x": 280,
   "y": 132,
   "param": "delay.division",
   "w": 128,
   "h": 56,
   "label": "Division",
   "options": [
    "1/1",
    "1/2",
    "1/4",
    "1/8",
    "1/8T",
    "1/16",
    "1/16T",
    "1/32"
   ]
  },
  {
   "type": "group",
   "x": 448,
   "y": 76,
   "w": 428,
   "h": 280,
   "text": "FEEDBACK / MIX"
  },
  {
   "type": "knob",
   "x": 480,
   "y": 132,
   "param": "delay.feedback",
   "size": 80,
   "label": "Feedback"
  },
  {
   "type": "switch",
   "x": 592,
   "y": 140,
   "param": "delay.pingpong",
   "w": 72,
   "h": 64,
   "label": "Ping Pong"
  },
  {
   "type": "knob",
   "x": 692,
   "y": 132,
   "param": "delay.filter",
   "size": 64,
   "label": "Filter"
  },
  {
   "type": "knob",
   "x": 784,
   "y": 132,
   "param": "delay.mix",
   "size": 64,
   "label": "Mix"
  }
 ]
})LAYOUT" },
            { "Modulation", "effect", R"LAYOUT({
 "format": 1,
 "name": "Modulation",
 "kind": "effect",
 "description": "Unified modulation processor switching among chorus, flanger and phaser while sharing four core controls.",
 "width": 880,
 "height": 392,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 580,
   "h": 36,
   "text": "MODULATION",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 636,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 832,
   "h": 96,
   "text": "EFFECT TYPE"
  },
  {
   "type": "selector",
   "x": 48,
   "y": 112,
   "param": "mod.type",
   "w": 784,
   "h": 48,
   "label": "Mode",
   "options": [
    "Chorus",
    "Flanger",
    "Phaser"
   ]
  },
  {
   "type": "group",
   "x": 24,
   "y": 188,
   "w": 832,
   "h": 176,
   "text": "MOVEMENT"
  },
  {
   "type": "knob",
   "x": 64,
   "y": 232,
   "param": "mod.rate",
   "size": 72,
   "label": "Rate"
  },
  {
   "type": "knob",
   "x": 224,
   "y": 232,
   "param": "mod.depth",
   "size": 72,
   "label": "Depth"
  },
  {
   "type": "knob",
   "x": 384,
   "y": 232,
   "param": "mod.feedback",
   "size": 72,
   "label": "Feedback"
  },
  {
   "type": "knob",
   "x": 544,
   "y": 232,
   "param": "mod.mix",
   "size": 72,
   "label": "Mix"
  }
 ]
})LAYOUT" },
            { "Vowel Filter", "effect", R"LAYOUT({
 "format": 1,
 "name": "Vowel Filter",
 "kind": "effect",
 "description": "Morphing vowel-formant filter centred on a large vowel knob and tactile XY modulation pad.",
 "width": 880,
 "height": 492,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 580,
   "h": 36,
   "text": "VOWEL FILTER",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 636,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 264,
   "h": 388,
   "text": "FORMANT"
  },
  {
   "type": "knob",
   "x": 64,
   "y": 132,
   "param": "vowel.morph",
   "size": 136,
   "label": "Vowel Morph"
  },
  {
   "type": "knob",
   "x": 64,
   "y": 320,
   "param": "vowel.resonance",
   "size": 72,
   "label": "Resonance"
  },
  {
   "type": "knob",
   "x": 176,
   "y": 320,
   "param": "vowel.rate",
   "size": 72,
   "label": "Rate"
  },
  {
   "type": "group",
   "x": 304,
   "y": 76,
   "w": 360,
   "h": 388,
   "text": "FORMANT XY"
  },
  {
   "type": "xy",
   "x": 328,
   "y": 120,
   "params": {
    "x": "vowel.x",
    "y": "vowel.y"
   },
   "w": 312,
   "h": 280,
   "label": "XY Motion"
  },
  {
   "type": "group",
   "x": 680,
   "y": 76,
   "w": 176,
   "h": 388,
   "text": "BLEND"
  },
  {
   "type": "knob",
   "x": 724,
   "y": 144,
   "param": "vowel.mix",
   "size": 88,
   "label": "Mix"
  }
 ]
})LAYOUT" },
            { "Saturator", "effect", R"LAYOUT({
 "format": 1,
 "name": "Saturator",
 "kind": "effect",
 "description": "Harmonic saturation with a dominant drive control, tone and bias shaping, selectable character and level meters.",
 "width": 912,
 "height": 400,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 612,
   "h": 36,
   "text": "SATURATOR",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 668,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 148,
   "h": 292,
   "text": "INPUT"
  },
  {
   "type": "meter",
   "x": 48,
   "y": 128,
   "source": "plugin.in L",
   "w": 100,
   "h": 200,
   "label": "Input",
   "settings": {
    "display": "segments"
   }
  },
  {
   "type": "group",
   "x": 188,
   "y": 76,
   "w": 488,
   "h": 292,
   "text": "SATURATION"
  },
  {
   "type": "knob",
   "x": 224,
   "y": 132,
   "param": "sat.drive",
   "size": 112,
   "label": "Drive"
  },
  {
   "type": "knob",
   "x": 368,
   "y": 144,
   "param": "sat.tone",
   "size": 72,
   "label": "Tone"
  },
  {
   "type": "knob",
   "x": 464,
   "y": 144,
   "param": "sat.bias",
   "size": 72,
   "label": "Bias"
  },
  {
   "type": "selector",
   "x": 560,
   "y": 144,
   "param": "sat.character",
   "w": 92,
   "h": 112,
   "label": "Character",
   "options": [
    "Soft",
    "Warm",
    "Edge",
    "Hard"
   ]
  },
  {
   "type": "group",
   "x": 692,
   "y": 76,
   "w": 196,
   "h": 292,
   "text": "OUTPUT / BLEND"
  },
  {
   "type": "knob",
   "x": 716,
   "y": 128,
   "param": "sat.output",
   "size": 64,
   "label": "Output"
  },
  {
   "type": "knob",
   "x": 804,
   "y": 128,
   "param": "sat.mix",
   "size": 64,
   "label": "Mix"
  },
  {
   "type": "meter",
   "x": 716,
   "y": 252,
   "source": "plugin.out L",
   "w": 148,
   "h": 84,
   "label": "Output",
   "settings": {
    "display": "segments"
   }
  }
 ]
})LAYOUT" },
            { "Guitar Amp", "effect", R"LAYOUT({
 "format": 1,
 "name": "Guitar Amp",
 "kind": "effect",
 "description": "Amplifier front panel with gain stages, full tone stack, cabinet choice and input meter.",
 "width": 1040,
 "height": 472,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 740,
   "h": 36,
   "text": "GUITAR AMP",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 796,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 712,
   "h": 268,
   "text": "AMPLIFIER"
  },
  {
   "type": "knob",
   "x": 48,
   "y": 128,
   "param": "amp.gain",
   "size": 88,
   "label": "Gain"
  },
  {
   "type": "knob",
   "x": 168,
   "y": 136,
   "param": "amp.bass",
   "size": 64,
   "label": "Bass"
  },
  {
   "type": "knob",
   "x": 268,
   "y": 136,
   "param": "amp.middle",
   "size": 64,
   "label": "Middle"
  },
  {
   "type": "knob",
   "x": 368,
   "y": 136,
   "param": "amp.treble",
   "size": 64,
   "label": "Treble"
  },
  {
   "type": "knob",
   "x": 468,
   "y": 136,
   "param": "amp.presence",
   "size": 64,
   "label": "Presence"
  },
  {
   "type": "knob",
   "x": 568,
   "y": 136,
   "param": "amp.master",
   "size": 64,
   "label": "Master"
  },
  {
   "type": "group",
   "x": 752,
   "y": 76,
   "w": 264,
   "h": 268,
   "text": "CABINET"
  },
  {
   "type": "dropdown",
   "x": 776,
   "y": 128,
   "param": "cabinet.type",
   "w": 216,
   "h": 56,
   "label": "Cabinet",
   "options": [
    "Open 1x12",
    "Closed 2x12",
    "Closed 4x12",
    "Direct"
   ]
  },
  {
   "type": "switch",
   "x": 816,
   "y": 232,
   "param": "cabinet.enable",
   "w": 72,
   "h": 64,
   "label": "Cab On"
  },
  {
   "type": "group",
   "x": 24,
   "y": 360,
   "w": 992,
   "h": 88,
   "text": "INPUT LEVEL"
  },
  {
   "type": "meter",
   "x": 48,
   "y": 392,
   "source": "plugin.in L",
   "w": 944,
   "h": 40,
   "label": "Input",
   "settings": {
    "display": "bar"
   }
  }
 ]
})LAYOUT" },
            { "Guitar Pedal", "effect", R"LAYOUT({
 "format": 1,
 "name": "Guitar Pedal",
 "kind": "effect",
 "description": "Vertical stomp-box panel with three primary controls, an illuminated status LED and foot switch.",
 "width": 480,
 "height": 620,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 180,
   "h": 36,
   "text": "GUITAR PEDAL",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 236,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 432,
   "h": 516,
   "text": "PEDAL FACE"
  },
  {
   "type": "knob",
   "x": 60,
   "y": 160,
   "param": "pedal.drive",
   "size": 72,
   "label": "Drive"
  },
  {
   "type": "knob",
   "x": 204,
   "y": 160,
   "param": "pedal.tone",
   "size": 72,
   "label": "Tone"
  },
  {
   "type": "knob",
   "x": 348,
   "y": 160,
   "param": "pedal.level",
   "size": 72,
   "label": "Level"
  },
  {
   "type": "lamp",
   "x": 232,
   "y": 340,
   "w": 20,
   "h": 20,
   "source": "pedal.enabled"
  },
  {
   "type": "switch",
   "x": 204,
   "y": 420,
   "param": "pedal.bypass",
   "w": 80,
   "h": 72,
   "label": "Footswitch"
  }
 ]
})LAYOUT" },
            { "Channel Strip", "effect", R"LAYOUT({
 "format": 1,
 "name": "Channel Strip",
 "kind": "effect",
 "description": "Consolestyle channel strip combining input, gate, compression, three-band equalization and an output fader.",
 "width": 1152,
 "height": 656,
 "widgets": [
  {
   "type": "label",
   "x": 24,
   "y": 16,
   "w": 852,
   "h": 36,
   "text": "CHANNEL STRIP",
   "size": 24,
   "bold": true
  },
  {
   "type": "preset",
   "x": 908,
   "y": 20,
   "w": 220,
   "h": 28
  },
  {
   "type": "group",
   "x": 24,
   "y": 76,
   "w": 192,
   "h": 432,
   "text": "INPUT"
  },
  {
   "type": "knob",
   "x": 72,
   "y": 132,
   "param": "input.gain",
   "size": 80,
   "label": "Input Gain"
  },
  {
   "type": "switch",
   "x": 80,
   "y": 280,
   "param": "input.highpass",
   "w": 72,
   "h": 64,
   "label": "High Pass"
  },
  {
   "type": "group",
   "x": 232,
   "y": 76,
   "w": 208,
   "h": 432,
   "text": "GATE"
  },
  {
   "type": "knob",
   "x": 256,
   "y": 132,
   "param": "gate.threshold",
   "size": 64,
   "label": "Threshold"
  },
  {
   "type": "knob",
   "x": 352,
   "y": 132,
   "param": "gate.release",
   "size": 64,
   "label": "Release"
  },
  {
   "type": "switch",
   "x": 300,
   "y": 272,
   "param": "gate.enabled",
   "w": 72,
   "h": 64,
   "label": "Gate On"
  },
  {
   "type": "group",
   "x": 456,
   "y": 76,
   "w": 224,
   "h": 432,
   "text": "COMPRESSOR"
  },
  {
   "type": "knob",
   "x": 480,
   "y": 132,
   "param": "comp.threshold",
   "size": 72,
   "label": "Threshold"
  },
  {
   "type": "knob",
   "x": 580,
   "y": 132,
   "param": "comp.ratio",
   "size": 72,
   "label": "Ratio"
  },
  {
   "type": "knob",
   "x": 488,
   "y": 264,
   "param": "comp.attack",
   "size": 60,
   "label": "Attack"
  },
  {
   "type": "knob",
   "x": 588,
   "y": 264,
   "param": "comp.release",
   "size": 60,
   "label": "Release"
  },
  {
   "type": "group",
   "x": 696,
   "y": 76,
   "w": 296,
   "h": 432,
   "text": "THREE BAND EQ"
  },
  {
   "type": "knob",
   "x": 720,
   "y": 132,
   "param": "eq.low_gain",
   "size": 64,
   "label": "Low"
  },
  {
   "type": "knob",
   "x": 808,
   "y": 132,
   "param": "eq.mid_gain",
   "size": 64,
   "label": "Mid"
  },
  {
   "type": "knob",
   "x": 896,
   "y": 132,
   "param": "eq.high_gain",
   "size": 64,
   "label": "High"
  },
  {
   "type": "knob",
   "x": 808,
   "y": 264,
   "param": "eq.mid_freq",
   "size": 64,
   "label": "Mid Freq"
  },
  {
   "type": "group",
   "x": 1008,
   "y": 76,
   "w": 120,
   "h": 432,
   "text": "OUTPUT"
  },
  {
   "type": "slider",
   "x": 1048,
   "y": 132,
   "param": "output.level",
   "w": 40,
   "h": 260,
   "label": "Level"
  },
  {
   "type": "group",
   "x": 24,
   "y": 524,
   "w": 1104,
   "h": 104,
   "text": "IN / OUT METERS"
  },
  {
   "type": "meter",
   "x": 48,
   "y": 564,
   "source": "plugin.in L",
   "w": 504,
   "h": 40,
   "label": "Input",
   "settings": {
    "display": "bar"
   }
  },
  {
   "type": "meter",
   "x": 600,
   "y": 564,
   "source": "plugin.out L",
   "w": 504,
   "h": 40,
   "label": "Output",
   "settings": {
    "display": "bar"
   }
  }
 ]
})LAYOUT" },
        };

        return layouts;
    }

    inline juce::StringArray names()
    {
        juce::StringArray list;

        for (const auto& layout : all())
            list.add (layout.name);

        return list;
    }

    /** A layout as parsed JSON, by index (void: none). */
    inline juce::var layout (int index)
    {
        return juce::isPositiveAndBelow (index, (int) all().size()) ? juce::JSON::parse (juce::String (all()[(size_t) index].json)) : juce::var();
    }

    /** Each layout on a line: its name, kind, size and what it holds. */
    inline juce::String describe()
    {
        juce::StringArray lines;

        for (int i = 0; i < (int) all().size(); ++i)
        {
            const auto l = layout (i);
            lines.add ("- " + juce::String (all()[(size_t) i].name) + " (" + all()[(size_t) i].kind + ", "
                       + l.getProperty ("width", 0).toString() + " x " + l.getProperty ("height", 0).toString() + "): "
                       + l.getProperty ("description", {}).toString());
        }

        return lines.joinIntoString ("\n");
    }
}
