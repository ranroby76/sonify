<!-- D:\workspace\Fanan_web_hub\src\skill_performance_controls.md -->
---
name: performance-controls
for: plugins
title: Performance controls
description: What a player's hand works while playing, as real controls: a pitch wheel and a mod wheel in their housing, a single wheel, a Roland-style bender lever, joysticks (a ball on a cross, a vector stick, an arcade stick, a gimbal stick in its boot, a thumbstick), a glowing X-Y pad, a ribbon or an LED touch strip. Their parameters, springing back (pitch bend) or staying, every design key, with recipes. Read it whenever an instrument needs pitch bend, modulation, a joystick, vector synthesis, an X-Y pad or a ribbon, or the user asks for wheels or a performance section.
---
# Performance controls

An `xy` element wears a performance control when its `"settings"` have a `"perform"` (the design). The user designs it in the edit window's Advanced edit tab, the Performance Studio (the E under the element, or a right-click on it, opens the edit window; its gallery: a page of the Gallery window); `design_templates` with `"kind": "perform"` lists the gallery's designs and gives any one's JSON with the `"mode"` and `"bendAxis"` it comes with.

```json
{ "type": "xy", "x": 20, "y": 200, "w": 110, "h": 170, "label": "Bend / Mod",
  "params": { "x": "synth.bend", "y": "synth.mod" },
  "settings": { "mode": "bendmod", "bendAxis": "x",
                "perform": { "kind": "wheels", "housingColour": "#FF18191C", "partColour": "#FF1E1F22",
                             "accentColour": "#FFF2F2EE", "material": "Rubber" } } }
```
`h` includes the 20 px caption.

## Kinds and what they move
- `wheel`: one wheel in its slot (`"vertical"`: up and down, the standard; else across). One parameter: `"y"` up and down, `"x"` across.
- `wheels`: a pitch wheel and a mod wheel side by side: `"x"` the left wheel, `"y"` the right, both moved up and down.
- `lever`: a Roland-style bender: across bends (`"x"`), pushed away or pulled toward modulates (`"y"`); in `"bendmod"` both go back to the middle when it's let go.
- `joystick`, `"style"`: `ball` (a ball riding a cross), `vector` (a stick in a cross-shaped slot), `arcade` (a ball-topped stick), `gimbal` (a stick in a rubber boot), `thumb` (a thumbstick's dished cap). Both `"x"` and `"y"`.
- `pad`: an X-Y surface with a glowing puck and its cross-hair.
- `ribbon`: a strip with its marker, or a row of lamps (`"leds": true`); one parameter as a wheel.

## How it moves (the element's "settings")
- `"mode"`: `free` (the standard: it stays where it's left) or `bendmod` (the bend axis springs back to its middle when let go, the other stays).
- `"bendAxis"`: `x` or `y`: which one bends (the wheels: `x` is the left wheel).
- Its parameters are ordinary ones, declared in a module: a bend -1..1 (default 0, its middle), a mod 0..1. The module reads them as any parameter (the host's own pitch bend and mod wheel still arrive in `context.pitchBend` and `context.modWheel`: add the two).

## The design ("perform"), every key optional
- `"kind"`, `"style"`, `"vertical"`; `"render"`: `3d` (the standard) or `2d` (a pad, a ribbon).
- `"housingColour"`, `"partColour"` (the wheel, the ball, the stick), `"accentColour"` (its mark, the puck, its lamps, its light), `"material"` (the moving part's: as the 3D looks').
- `"glow"` 0..100, `"lit"` (a light in its slot or under its pad), `"leds"` (a ribbon), `"range"` (push and pull angle: a joystick's or the lever's whole sweep, edge to edge, in degrees, 10..170; a stick's standard 160, 80 each way; the lever's 42), `"clean"` (a wheel or the pair: plain, no ribs and no mark; the gallery has "Pitch and mod wheels (clean)" and "Wheel (clean)").
- `"tilt"`: degrees, seen from the front (0 straight down): give it the panel's 3D view (the 3d-panel skill).
- `"frames"`: pictures each way (the standard 17 for two axes, 33 for one). More is smoother and bigger: keep two-axis controls at 17 to 21.

## Sizes
Wheels about 110 x 170, one wheel 60 x 170, a lever 150 x 110, a joystick or pad square (150 to 200), a ribbon long and thin (300 x 50 across). Put them at the left of a keyboard, as real synths do.

## In an exported plugin
They play as pictures for each position; a bend springs back as in the studio. Keep them upright (no `"angle"`).

## Check
- Pitch bend `bendmod` with its axis right, the mod staying?
- Its parameters declared and used by the sound (bend in semitones, mod into vibrato or the filter)?
- Next to the keyboard, in the panel's view and colours?
