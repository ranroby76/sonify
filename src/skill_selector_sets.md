<!-- D:\workspace\Fanan_web_hub\src\skill_selector_sets.md -->
---
name: selector-sets
for: plugins
title: Selector sets
description: A selector's positions as keys of their own, flat (2D) or as real 3D keys that light: a row or a column of buttons, a grid of pads, triangles zipped tip up and tip down, keys round a ring or an arc, a joined tab strip, a D-pad, a rocker; one lit at a time, each key on and off by itself, or several at once. Every design key, the three modes and their parameters, with recipes. Read it whenever a panel needs a waveform or mode chooser, step buttons, an FX on/off row, mute/solo keys, a pad grid, a rotary chooser or lit buttons of any kind.
---
# Selector sets

A `selector` wears a set when its `"settings"` have a `"set"` (the design). The user designs it in the edit window's Advanced edit tab, the Selector Studio (the E under the element, or a right-click on it, opens the edit window; its gallery: a page of the Gallery window); `design_templates` with `"kind": "set"` lists the gallery's designs and gives any one's JSON. Start from the one nearest to what's asked and change only what differs.

```json
{ "type": "selector", "param": "osc.wave", "x": 40, "y": 60, "w": 280, "h": 70, "label": "Wave",
  "options": ["Saw", "Square", "Tri", "Sine"],
  "settings": { "mode": "single",
                "set": { "render": "3d", "formation": "row", "shape": "rect", "rounding": 14, "gap": 14,
                         "led": "dot", "ledColour": "#FFFF3B30", "labels": "caps" } } }
```
`h` includes the 20 px caption under it (no caption: leave out `"label"`).

## Modes (the selector's "settings" "mode")
- `single` (the standard): one key lit at a time; the parameter is the key's number (0..N-1). Its options are the keys' names.
- `toggles`: each key on and off by itself, each its own 0..1 parameter: the selector's `"param"` with the key's number after it (`"param": "fx.on"` → `fx.on1`, `fx.on2`...), unless its `"params"` name them by role: `{ "key1": "chorus.on", "key2": "delay.on" }`. Declare those parameters in the modules.
- `multi`: several at once, one parameter holding a bit a key (key 1 = 1, key 2 = 2, key 3 = 4...): a 0..2^N-1 parameter read with `(int) std::lround (param (i))` and `& (1 << k)`. For a step row of up to 16 steps, or layers on and off.

## The design ("set"), every key optional
- `render`: `3d` (the standard: keys with height, travel and a lit cap) or `2d` (flat; `"style"` `soft`, `flat`, `glossy`).
- `formation`: `row`, `column`, `zipper` (triangles tip up and tip down), `grid` (`"columns"` across), `ring`, `arc` (`"arcFrom"`, `"arcTo"` degrees, 0 at the top, clockwise: 100..260 runs round the bottom; `"inner"` the keys' inner edge, % of the outer), `segmented` (a joined tab strip), `cross` (a D-pad), `rocker`. `"vertical": true` for zipper, segmented and rocker up and down.
- `shape`: `round` (the standard), `rect`, `circle`, `pill`, `triangle`, `segment` (a ring's or an arc's curved keys), `led` (a lamp with its name beside it). `"gap"` between keys (% of a key), `"rounding"` (%).
- Names: `"labels"` `caps` (in the keys), `beside` (out of them) or `none`; `"labelSide"` `auto`, `above`, `below`, `left`, `right`, `inside`; `"labelAngle"` `horizontal` or `follow`; `"textSize"` %, `"font"`, `"bold"`, `"italic"`, `"upper"`, `"spacing"` %; `"labelColour"`, `"labelOnColour"`, `"labelGlow"`, `"textEffects"` (the layer style on them).
- Colours: `"capColour"`, `"capOnColour"`; `"keyColours": [...]` with `"group"` (colours in turn, a group of keys each: a step row in fours); `"glowColour"` and `"glow"` (0..100) for a lit key's light; `"edge"` its rim lit (`"edgeOff"` % when off).
- `"led"`: `none`, `dot`, `bar` (a lamp on each key), `"ledColour"`.
- `"plate"`, `"plateColour"`: a plate the keys sit in.
- 3D: `"material"` (as the 3D looks'), `"height"` (% of a key), `"travel"` (% of its height: how far it goes down when lit).

## Recipes
- Waveform chooser on a vintage synth: `row`, `rect`, 3d, Bakelite caps, `led` dot in red, names `beside` below.
- Step sequencer: `toggles` or `multi`, `grid` with 16 keys in `"columns": 8` or a `row`, `"keyColours"` with `"group": 4`, a bright `glowColour`.
- Mute / solo: `toggles`, `pill` keys, 2d `glossy`, amber and blue glows.
- Rotary mode chooser: `ring` (or `arc` 220..500), `segment` keys, names `follow`.
- A pad grid: `grid`, `"columns": 4`, 3d rubber, `glow` 60 in the accent.

## In an exported plugin
Each key is its own picture lit over the set, so every mode works as in the studio. Keep sets upright (no `"angle"`).

## Check
- The right mode for what the parameter means, and its parameters declared (toggles: one per key)?
- Options naming every key; the box big enough for the formation (a ring wants a square box)?
- One set design per kind of chooser on the panel, its colours from the panel's accent?
