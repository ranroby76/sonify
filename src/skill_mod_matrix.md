<!-- D:\workspace\Fanan_web_hub\src\skill_mod_matrix.md -->
---
name: mod-matrix
for: plugins
title: Modulation matrix
description: The modulation matrix element: routes from the plugin's modulation sources (LFOs, envelopes, velocity, the mod wheel) to its parameters as slots, a grid of pins or knobs, or a patch bay with jacks and cables; its parts (jacks, source chips, assign buttons) lying anywhere on the panel with cables across the whole window and amount rings round the controls. Its parameters, the code that makes it sound, every design key. Read it whenever the user asks for a mod matrix, modulation routing, a patch bay, patch cables, drag-to-modulate or a modular look.
---
# Modulation matrix

A `matrix` element routes modulation sources to the plugin's parameters, as many routes as it's given. The user designs it in the edit window's Advanced edit tab, the Matrix Studio (the E under the element, or a right-click on it, opens the edit window; its gallery: a page of the Gallery window); `design_templates` with `"kind": "matrix"` lists the gallery's designs and gives any one's JSON.

```json
{ "type": "matrix", "param": "synth.mod", "x": 20, "y": 130, "w": 460, "h": 200,
  "settings": { "matrix": { "style": "slots", "routes": 8,
                            "sources": ["LFO 1", "LFO 2", "Env 2", "Velocity", "Mod wheel"],
                            "colour": "#FF2FB36B", "panelColour": "#FF141518" } } }
```

## Its parameters
Every route is three of the plugin's parameters, so it's automated and kept in presets (`<param>` is the element's `"param"`, `mod` when it has none; n from 1):
- `<param>_src<n>`: the source, 0 none, 1.. the `"sources"` in order (0..number of sources).
- `<param>_dst<n>`: the destination, 0 none, 1.. the plugin's own parameters in order: every parameter in graph order then declaration order, the matrices' own left out (0..that many).
- `<param>_amt<n>`: the amount, -1..1 (default 0).

With `"param": "synth.mod"` the module `synth` declares `mod_src1`, `mod_dst1`, `mod_amt1`, `mod_src2`... Declare them after the module's other parameters.

## Making it sound
The element only sets those parameters; the module applies them. Keep the sources and their destinations in one module (the voice module: its LFOs and envelopes are the sources, its own parameters the destinations), first in `graph.json`, so destination d is its own parameter d-1. Each block, for each route with a source and a destination: take the source's value (bipolar -1..1 for LFOs, 0..1 for envelopes, velocity and the mod wheel), times the amount, times the destination's range, added to the destination's value before it's used (per voice for envelopes and velocity), clamped to its range. Cutoffs and pitches sound right modulated in octaves or semitones: scale the amount to ±4 octaves or ±24 semitones there. Smooth the result as the parameter itself is smoothed.

## The design ("matrix"), every key optional
- `"style"`: `slots` (a list of routes: source, destination, amount), `grid` (sources down, destinations across: a pin or a knob where they meet; `"cell"`: `pin` (on or off, the amount its ring), `knob`, `bar`), `patch` (a patch bay: the sources' output jacks, the destinations' input jacks, cables between them; `"jack"`: `eurorack` (the standard), `studio`, `minimal`; `"cables"`: their colours in turn; `"sag"` 0..100; `"matchInputs"` true: an input jack for every output, the destinations padded with spare inputs or cut to the sources' count; on in the gallery's patch bays, a toggle in its studio).
- `"routes"` 1..64 (8 standard); `"sources"` their names in order; `"targets"` (grid and patch): the destinations shown, as parameter ids (the standard: the plugin's first 8).
- `"amount"`: a new route's amount (0.5 standard; a patch bay's 1).
- `"colour"`, `"panelColour"` (its chips' and buttons' face), `"textColour"`, `"lineColour"`, `"font"`, `"textSize"` %. A matrix has no background of its own: it lies on the panel, a plate or a picture under it, so give it a plate (a rectangle shape behind it) when it needs one.

## Its parts anywhere on the panel
One matrix's parts can each be an element of their own, sharing its parameters (the same `"param"`): the cables run across the whole window, and a source dragged onto any knob or slider routes to it, its amount a ring round the control. A `matrix` element whose design has:
- `"style": "jack"`: a jack, `"role": "out"` with its `"source"` (1..), or `"in"` with its `"target"` (a parameter id). Put an output jack beside each LFO and envelope, an input jack beside each destination knob: a semi-modular panel.
- `"style": "chip"` with `"source"`: a source to drag onto any control or input.
- `"style": "assign"` with `"source"`: an assign button: on, the next control touched is routed from it.
Keep one main matrix element (it can be small, slots) holding `"routes"` and `"sources"`.

## In an exported plugin
The main matrix (slots, grid, patch bay) works there with its routes and cables; parts lying on their own (jacks, chips, assign buttons, amount rings) are studio-only for now: say so when the user wants them in a plugin they'll export.

## Check
- Its parameters declared, with the right ranges, and the module applying every route?
- Sources named as the module's sources, in the same order?
- Big enough: slots about 60 px wide a column and 26 a row; a grid 40 px a cell; a patch bay's jacks 36 px apart?
