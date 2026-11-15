<!-- D:\workspace\Fanan_web_hub\src\skill_3d_body.md -->
---
name: 3d-body
for: plugins
title: 3D bodies
description: The plugin window's 3D body: the whole instrument's case (a desktop module, a keyboard synth with its keybed step, a sloped vintage panel, a drum machine wedge, a groovebox, a mixer with a meter bridge, a stompbox, guitar pedals (a treadle stomp, a wah rocker, mini, dual footswitch, hammertone, slanted with top jacks, a pedalboard), amps and cabs, modern multi-effects whose screens show the chain as coloured blocks, tape echoes, pad samplers, DJ gear, organs, modular cases, a Eurorack case, rack units and more) built from parts and rendered behind the controls, lit by the global light and seen by the panel camera. Every part kind (box, cylinder, sphere, extrude, revolve, text, sweep, loft, mesh), cuts, blends, hollow shells, mirrors and repeats, materials and surfaces, with recipes. Read it whenever the user asks for a 3D body, case, chassis, enclosure, housing or hardware shape, or the layout already has a "body".
---
# 3D bodies

A body is the instrument's case itself: the controls stand on it as they stand on any panel. It lives in the layout's `"body"` (beside `"light"` and `"camera"`), is rendered behind every element, and is the first thing that makes a plugin look like a real object. The user shapes it in the studio's Showroom: the body in 3D, seen from anywhere (a view cube, orbit, pan, roll, zoom), its parts list and settings beside it, a part clicked in the view to choose it and moved, turned and sized there with its handles, a profile editor on its second tab; it saves the view as a PNG (see-through if asked); the bodies to start from are a page of the Gallery window.

## Start from a template
`design_templates` with `"kind": "body"` lists them (Desktop module, Keyboard synth, Vintage synth (sloped panel), Drum machine (wedge), Groovebox, Mixer with a meter bridge, Stompbox, Eurorack case, Guitar pedal (treadle stomp), Guitar pedal (wah rocker), Monosynth (flip-up panel), Pad sampler (4x4 pads), Modular suitcase (3 rows), Tape echo, Amp head (tolex), Combo amp (top panel), 500-series lunchbox, Tube console (wood), Sloped desk console, DJ mixer, DJ controller (two decks), Pocket synth (bare board), Portable keyboard (speakers), Drawbar organ, Semi-modular (patch bay), Step sequencer (16 steps), Vintage compressor (meter), Sampler workstation (floppy), Compact mixer (8 channels), Aluminium extrusion, Modern poly (wood cheeks), Retro sci-fi console, Keyboard controller (pads), Eurorack skiff (slanted), Boutique desktop (rounded), Transistor radio, Looper station (big wheel), Tube amp chassis, Guitar pedal (mini), Guitar pedal (dual footswitch), Guitar pedal (hammertone), Guitar pedal (slanted, top jacks), Pedalboard (five pedals), Speaker cab (4x12), Amp stack (head on a cab), Practice amp, Rack preamp (tube window), MIDI foot controller (10 switches), Multi-FX floorboard (big screen), Multi-FX compact (four switches), Multi-FX desktop modeler (touch screen), Multi-FX with expression pedal, Multi-FX mini (pedal-size modeler), Rack 1U, 2U, 3U); with a `"name"` it gives that body for this window's size. Put it in with `edit_layout`'s `"body"` (the whole body: read `gui/layout.json` first when the user already has one, and change only what was asked). `"body": null` removes it.

## The body
`{ "tilt": 0, "shadow": 60, "softness": 55, "exposure": 0, "floor": "#00000000", "parts": [...] }`
- `tilt` 0..60: seen from the front (0 straight down onto the panel). Keep it 0 for a plugin window; the controls are drawn straight on.
- `shadow` 0..100 and `softness` 0..100: the shadow it casts on its floor; `exposure` -100..100 its brightness; `floor` the floor's colour (transparent: the window's background).
- `"editor": { "units": "mm", "mmWidth": 620 }` is the Showroom's own (it shows sizes in millimetres): keep it as it is.

## A part
Every part is a shape in a box: `"x"`, `"z"` its middle on the window (pixels from the left and the top, as the controls' coordinates), `"y"` its foot above the floor, `"w"`, `"h"`, `"d"` its width, height (up from the floor) and depth (top to bottom of the window). The chassis is usually as big as the window less a few pixels round it, its `"h"` 30 to 60.
- `"kind"`: `box`, `cylinder` (`"axis"`: `y` upright, `x` lying across, `z` lying back to front), `sphere`, `extrude`, `revolve`, `text`, `sweep`, `loft`, `mesh`.
- `"op"`: `add` (the standard) or `cut` (taken away from the parts before it: wells, slots, a keybed, vents). Order matters: a cut takes from what came before it, a part after it fills the hole (a screen in its well); where a later part's surface meets an earlier one's (a plate laid on a face), the later part shows. The Body editor lists the parts front first (the last part at the top).
- `"hidden": true`: left out for now (the Body editor's eye); take it away to bring the part back.
- `"blend"` (pixels): melted into the parts before it, like a casting (or carved, for a cut). `"round"` (pixels) rounds its edges, `"chamfer"` cuts them at 45 degrees.
- `"rotate"`: `[x, y, z]` degrees about its middle (a sloped rear panel: `[-12, 0, 0]`).
- `"mirror": "x"`: a copy mirrored across the body's middle (handles, side cheeks, screws): the body's `"mirrorAt"` (pixels from the window's left; none: the window's middle). The user can select the body on the panel and move or resize it (or a frame round it), and `"mirrorAt"` moves with it; a body from the Gallery window starts at 75% of the window, in its middle.
- `"repeat"`: `{ "kind": "row", "count": 12, "dx": 7.2 }` copies along x; `"grid"` with `"countZ"` and `"dz"`; `"circle"` with `"count"` and `"radius"` (a speaker's holes).
- `"material"`: Gloss plastic, Satin plastic, Matte plastic, Rubber, Bakelite, Brushed aluminium, Spun aluminium, Chrome, Steel, Gold, Brass, Copper, Anodized, Painted metal. `"color"` `"#AARRGGBB"`.
- `"surface"`: none, tolex, knurl, wood, perforated, rubber, brushed, leather, grille; `"surfaceScale"` (0.25..4), `"surfaceDepth"` (0..100).
- `"glass"` 0..100: see-through (a display's window, clear acrylic). `"glow"` `"#AARRGGBB"` with `"glowStrength"` 0..100: light it gives (a screen, an LED strip).
- `"name"`: what the parts list calls it. Name every part ("Chassis", "Keybed", "Screen well").

## Profiles: extrude, revolve, loft
- `extrude`: a 2D `"profile"` (SVG path data in 0..1 of its plane: M, L, H, V, C, Q, A, Z) pulled through its box. `"plane"`: `side` (seen from the side: x 0 the back .. 1 the front, y 0 the top .. 1 the bottom; pulled across its width: a wedge, a keyboard's step, a sloped panel), `front` (x 0 left .. 1 right; pulled back through its depth: an arch, a nameplate's outline), `top` (seen from above; pulled up through its height: a rounded or odd-shaped case). `"svg"`: an SVG file in gui/images as the outline instead.
- `revolve`: the profile turned round its upright middle (x its radius 0..1, y 0 top .. 1 bottom): knobs' bases, feet, domes, a speaker cone.
- `loft`: one profile becoming another through its box. Its sections lie in its `"plane"` (top: stacked bottom to top; front: back to front; side: left to right); `"profile"` at the start, `"profileEnd"` at the end (left out: the same one), `"taper"` (% the end's size, 100 none) and `"twist"` (degrees the end turns): a horn, a tapered leg, a speaker port, a twisted ornament.

## Sweeps: handles, tubes, rails
`sweep`: a section drawn along a `"path"` (SVG path data in 0..1 of its `"plane"`, as an extrude's). `"thickness"` (pixels) is the section's size; `"section"`: `round` (a tube, a cable, a handle), `square` (a rail, a frame), or `profile` (its own `"profile"` in 0..1 of the section: x across the path, y 0 top .. 1 bottom). A carry handle: `"plane": "front"`, `"path": "M0 1 L0 0.35 Q0 0 0.3 0 L0.7 0 Q1 0 1 0.35 L1 1"`, `"thickness"` 10, chrome, standing on the case's top; a roll bar over a groovebox; a rack's rails; piping round an edge.

## Hollow shells
`"shell"` (pixels): the part is hollow, its walls this thick; `"open"`: the face left open (`none`, `top`, `bottom`, `front`, `back`, `left`, `right`). A tray (open top) for a well that shows its walls, a hood over a screen (open front), a sleeve, a speaker box, a meter bridge's housing. With `"open": "none"` it's a closed hollow part that a later cut opens.

## Text
`text`: `"text"`, `"font"` (the panel fonts), `"bold"`; standing up from what's under it, or cut into it with `"op": "cut"` (an engraved nameplate). Keep it big: small text in 3D blurs; small labels belong on the panel as `label` elements.

## Models from files
`mesh`: an OBJ or STL model the user brought into the Showroom (it copies it to `gui/models`): `"file": "models/case.obj"`, `"fit"`: `keep` (its proportions, in its box) or `stretch`, `"up"`: `y` (the standard) or `z` (most STL and CAD models). Use only files the layout already names; never invent a model's file.

## Method
1. Read the 3d-panel skill: the body is the panel's surface, so the controls (3D looks or layered) must agree with its view and light.
2. Start from the nearest template, then fit it: the chassis to the window, cut wells where the screens, meters and keyboard sit (a `cut` box a few pixels bigger than the element, `"round"` 2 to 4), raised plates under groups (`add` boxes 2 to 4 px tall), the screen's glass in its well.
3. Detail with restraint: screws (`cylinder`, steel, `"mirror": "x"`), vents (`cut` boxes with `"repeat"`), a handle (`sweep`), side cheeks (wood `box` or `extrude` with `"mirror": "x"`), a nameplate (`text`).
4. Keep the controls where they were: the body goes behind them. A keyboard element sits in a keybed cut; a meter in a well.
5. Tell the user they can walk round it and fine-tune it in the Showroom.

## Final check
- Parts named, in a sensible order (adds, then their cuts, then what fills the cuts)?
- The chassis inside the window with room for its shadow; nothing poking past the window's edges?
- Every well, plate and keybed under the element it's for, a few pixels bigger?
- Materials that belong together (the 3d-panel skill), one accent for light?
