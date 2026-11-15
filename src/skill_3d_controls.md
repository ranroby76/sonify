<!-- D:\workspace\Fanan_web_hub\src\skill_3d_controls.md -->
---
name: 3d-controls
for: plugins
title: 3D controls
description: 3D looks for knobs, buttons, toggle switches and sliders: controls built as real objects (a turned knob with its skirt, grip, cap, pointer and light ring; a cap on its bezel that goes down and lights; a lever on its nut; a fader on its track) in materials, rendered, lit by the panel's global light and seen by its one camera. When to choose them, the 3D Gallery, make_3d_look and every setting of every kind, with recipes. Read it whenever the user asks for 3D, realistic, hardware-like, photoreal or rendered controls, names a 3D look, or a panel already wears 3D looks; read the 3d-panel skill with it before laying out the panel.
---
# 3D controls

A 3D look is a look like the others (a control wears it by its `"style"`), but it isn't drawn in layers: it's a model, built as a real object and rendered (ray-marched, with soft shadows, reflections of a studio room, a clear coat on plastics, brushed and spun metal that keep their highlights where the light is as the knob turns). The user edits them in the Plugins Creator Studio's **3D Studio** tab (the Knob Studio's sister: the same studio on the 3D looks), with the same Gallery, Export as PNG and everything else.

## When to choose 3D
- The user asks for 3D, realistic, photoreal, rendered, hardware, "real knobs", "like the picture" (a photo of gear).
- Classic hardware characters: a groovebox with black knobs and LED rings, a console with fader caps, a vintage amp with bakelite or chrome knobs, an arcade-style drum pad, a step sequencer's lit buttons.
- Not for flat, modern, minimal or cartoon panels: there layered looks (make_look, the knob gallery) fit better.
- One panel, one way: either every knob, slider and switch wears a 3D look, or none does. Never a 3D knob beside a layered knob. (Lamps, meters, labels, screens, keyboards and shapes are drawn as they always are, and they sit fine beside 3D controls: the 3d-panel skill says how to make them agree.)

## The workflow
1. `find_gallery_looks` with `"3d": true` and the kind (`knob`, `slider`, `switch`: buttons and toggle switches) and a few words for the plugin's character. The 3D Gallery holds ready-made models, every one allowed in any plugin, sold ones too.
2. `use_gallery_look` with `"3d": true` and the id: it's the plugin's own look now.
3. When it needs to change (a colour to the panel's accent, another material, the panel's view): `make_3d_look` with `"from"` its name and only the `"settings"` to change. Give it a new `"name"` to keep both.
4. Nothing in the gallery suits: `make_3d_look` with a `"kind"` (`knob`, `button`, `toggle`, `slider`, `hslider` for a slider across), a ready-made `"preset"` to start from, and the settings.
5. Put the looks on the controls with their `"style"` (`edit_layout`): one 3D look per kind of control, worn by all of them (a hero knob may wear a bigger, richer one of its own).

The 3D Gallery's models: knobs (Black skirted, Black with orange LEDs, Blue anodized fluted, Blue light ring, Brass dome, Chrome bullet, Chrome cap on black, Copper fluted, Cream fluted, Cyan pan ring, Flat brushed aluminium, Gold cap on black, Gold fluted, Green LED ring, Knurled steel, Mini steel trim, Orange cap on black, Purple gloss dome, Red anodized, Silver skirt black, Soft rubber, Soft white, Spun aluminium, Steel dish, Studio grey, Tall ribbed black, Vintage bakelite, Walnut bakelite); buttons (Arcade (lit cap), Arcade blue, Arcade red, Black cap with a light, Black square tactile, Blue light pill, Brass push, Chrome dome, Copper cap, Gold bezel black, Green rubber pad, Grey membrane, Keycap with a label, Mute key, On rocker, Orange step, Pill with a light bar, Play, Record, Red step, Round chrome, Rubber pad, Solo key, Stop, White glow ring, White keycap); toggle switches (Black ball lever, Chrome lever, Paddle on a plate); sliders (Black block, Blue cap, Brushed block, Chrome block, Console fader, Dense scale, Gold round, Grey console, Long-throw thin, Mini fader, No-scale black, Orange cap, Red line fader, Round cap, Rubber block, White cap fader, Wide track steel); sliders across (Black block across, Black crossfader across, Brass across, Chrome round across, Console fader across, Mini across, Red cap across, Round cap across, White block across). The user may add more: always look with `find_gallery_looks`.

Ready-made presets for `make_3d_look` (`"preset"`): knobs Black skirted, Spun aluminium, Chrome cap on black, Soft rubber, Vintage bakelite, Gold fluted, Knurled steel, Blue light ring; buttons Black cap with a light, Keycap with a label, Round chrome, Rubber pad, Arcade (lit cap), Pill with a light bar; toggles Chrome lever, Black ball lever, Paddle on a plate; sliders Console fader, Black block, Round cap.

## How each kind moves
- A knob turns through its frames (65) from `turnFrom` to `turnTo` (degrees from 12 o'clock: -150 to 150 is the classic 300-degree turn; -135 to 135 for a pan with `ringFrom` "The middle"). Its light ring is on the panel round it and lights up to its value.
- A button has two frames: off (up, its light at `lightOff`) and on (down by `travel`, its light lit). It's worn by switches (`toggle` elements).
- A toggle switch has two frames: the lever up and down. Also worn by switches.
- A slider's cap travels its track through its frames; an upright slider wears an upright look, a slider across (`"orientation": "horizontal"`) a look made with `hslider`.

## Settings (make_3d_look's "settings", by key)
Choices by their names, flags true or false, colours "#AARRGGBB", numbers kept in their range. Only the settings given change. "only when" means the setting applies only while that's so: a choice there is by its number, 0 its first ("only when pointer!=0": with any pointer but None; "only when top!=0": a dome or a dish), a flag by its key ("only when ring": with the light ring on).

### Knob
Shape:
- diameter (Diameter): 20 to 96 %; default 66. Of the canvas (the skirt goes past it)
- height (Height): 5 to 200 %; default 30. Of its diameter: a flat disc to a tall cylinder (100: as tall as it's wide; up to 200). With Taper, a tall cone
- taper (Taper): -30 to 90 %; default 6. Its top narrower than its foot (minus: wider): 0 a cylinder, 40 a cone with a broad top, 90 a cone almost to a point
- edge (Edge rounding): 0 to 45 %; default 14. Its top edge rounded, as a share of its height
- top (Top): one of Flat, Dome, Dish; default Flat. A flat top, a dome or a dish
- topDepth (Dome or dish depth): 0 to 40 %; default 8; only when top!=0
Skirt:
- skirt (Skirt): true or false; default true. A wider foot round its base
- skirtDiameter (Skirt diameter): 100 to 150 %; default 122; only when skirt. Of the knob's
- skirtHeight (Skirt height): 5 to 70 %; default 24; only when skirt. Of the knob's height
- skirtBevel (Skirt bevel): 0 to 100 %; default 55; only when skirt. How far its top slopes down to its rim
- skirtMaterial (Skirt material): one of Gloss plastic, Satin plastic, Matte plastic, Rubber, Bakelite, Brushed aluminium, Spun aluminium, Chrome, Steel, Gold, Brass, Copper, Anodized, Painted metal; default Gloss plastic; only when skirt
- skirtColour (Skirt colour): a colour; default #FF1A1A1D; only when skirt
- skirtLine (Line on the skirt): true or false; default false; only when skirt. The pointer goes on over the skirt too
Grip:
- grip (Grip): one of Smooth, Flutes, Knurled, Ribs; default Smooth. Its side: smooth, fluted, knurled or ribbed
- gripCount (How many): 6 to 120; default 28; only when grip!=0
- gripDepth (Depth): 0.0 to 12.0 %; default 3.0; only when grip!=0. Of its radius
Top cap:
- cap (Cap insert): true or false; default false. A disc of another material set into its top
- capDiameter (Cap diameter): 20 to 95 %; default 72; only when cap. Of the top
- capMaterial (Cap material): one of Gloss plastic, Satin plastic, Matte plastic, Rubber, Bakelite, Brushed aluminium, Spun aluminium, Chrome, Steel, Gold, Brass, Copper, Anodized, Painted metal; default Spun aluminium; only when cap
- capColour (Cap colour): a colour; default #FFC9CCD2; only when cap
- capGroove (Groove round it): true or false; default true; only when cap
Pointer:
- pointer (Pointer): one of None, Line, Dot, Groove, Ridge; default Line. Painted (a line, a dot), cut in (a groove) or standing up (a ridge)
- pointerColour (Pointer colour): a colour; default #FFF4F1EA; only when pointer!=0
- pointerLength (Length): 10 to 100 %; default 62; only when pointer!=0. Of the top's radius
- pointerWidth (Width): 2.0 to 30.0 %; default 8.0; only when pointer!=0. Of the top's radius
- pointerSide (Down its side too): true or false; default false; only when pointer!=0. The line (or groove) runs on down its side as well: for tall cylinders and cones, whose side shows more than their top
- pointerY (Y position): -50 to 50 %; default 0; only when pointer!=0. Moves the pointer nearer the knob's edge (up to +50) or its centre (down to -50), in % of the top's radius
Body:
- material (Material): one of Gloss plastic, Satin plastic, Matte plastic, Rubber, Bakelite, Brushed aluminium, Spun aluminium, Chrome, Steel, Gold, Brass, Copper, Anodized, Painted metal; default Gloss plastic
- colour (Colour): a colour; default #FF202024
Light ring:
- ring (Light ring): true or false; default false. LEDs round it on the panel, lit up to its value
- ringColour (Light colour): a colour; default #FF2F8CFF; only when ring
- ringSegments (LEDs (0 a solid band)): 0 to 41; default 0; only when ring
- ringWidth (Ring width): 2.0 to 25.0 %; default 7.0; only when ring. Of the knob's radius
- ringGap (Gap): 0.0 to 30.0 %; default 5.0; only when ring. Between it and the knob
- ringFrom (Lights from): one of Its start, The middle; default Its start; only when ring. From the start of its turn, or both ways from the middle (pan, balance)
Movement:
- turnFrom (Turns from): -180 to 0 deg; default -150. Its first frame, from 12 o'clock
- turnTo (Turns to): 0 to 180 deg; default 150. Its last frame
View:
- tilt (Seen from (0 straight above)): 0 to 60 deg; default 0. Where the camera is: 0 straight above (as a plugin's controls are seen), up to 60 from in front, the sides of the control showing
- perspective (Perspective): 0 to 100 %; default 30. How much nearer parts look bigger: 0 none, 100 close up
- shadow (Shadow): 0 to 100 %; default 50. How dark the shadow it casts on the panel is (lit by the panel's global light)
- softness (Shadow softness): 0 to 100 %; default 55
- exposure (Exposure): -100 to 100 %; default 0. Brighter or darker overall

### Button
Shape:
- shape (Shape): one of Round, Rounded square, Square, Pill, Rounded rectangle; default Rounded square
- width (Width): 20 to 96 %; default 66. Of the canvas's width
- depth (Depth (top to bottom)): 20 to 96 %; default 66. Of the canvas's height (round and square: as wide)
- corner (Corner rounding): 0 to 100 %; default 30; only when shape=4
- height (Height): 3 to 50 %; default 20. Of its width
- edge (Edge rounding): 0 to 60 %; default 30. Of its height
- top (Top): one of Flat, Convex, Concave; default Flat
- topDepth (Bulge or dip): 0 to 30 %; default 6; only when top!=0
- material (Material): one of Gloss plastic, Satin plastic, Matte plastic, Rubber, Bakelite, Brushed aluminium, Spun aluminium, Chrome, Steel, Gold, Brass, Copper, Anodized, Painted metal; default Gloss plastic
- colour (Colour): a colour; default #FF26262A
- travel (Goes down by): 0 to 70 %; default 35. Pressed (its second frame): of its height
Bezel:
- bezel (Bezel): true or false; default true. A frame round it
- bezelWidth (Bezel width): 2.0 to 30.0 %; default 8.0; only when bezel. Of its width
- bezelHeight (Bezel height): 2 to 60 %; default 40; only when bezel. Of its height
- bezelMaterial (Bezel material): one of Gloss plastic, Satin plastic, Matte plastic, Rubber, Bakelite, Brushed aluminium, Spun aluminium, Chrome, Steel, Gold, Brass, Copper, Anodized, Painted metal; default Steel; only when bezel
- bezelColour (Bezel colour): a colour; default #FF9EA2A8; only when bezel
Light:
- light (Light): one of None, Dot, Bar, Ring round it, The whole cap; default Dot. What lights up when it's on (its second frame)
- lightPlace (Where): one of Top, Middle, Left; default Top; only when light=1
- lightColour (Light colour): a colour; default #FFFFC21A; only when light!=0
- lightOff (Off, it shows): 0 to 60 %; default 18; only when light!=0. How much of its colour shows while it's off
Label:
- label (Label): words; default "". Words printed on its top
- labelColour (Label colour): a colour; default #FFEAE6DC
- labelSize (Label size): 10 to 90 %; default 36. Of its depth
View:
- tilt (Seen from (0 straight above)): 0 to 60 deg; default 0. Where the camera is: 0 straight above (as a plugin's controls are seen), up to 60 from in front, the sides of the control showing
- perspective (Perspective): 0 to 100 %; default 30. How much nearer parts look bigger: 0 none, 100 close up
- shadow (Shadow): 0 to 100 %; default 50. How dark the shadow it casts on the panel is (lit by the panel's global light)
- softness (Shadow softness): 0 to 100 %; default 55
- exposure (Exposure): -100 to 100 %; default 0. Brighter or darker overall

### Toggle switch
Base:
- base (Nut): one of Hex nut, Round nut, Plate; default Hex nut
- baseDiameter (Nut size): 20 to 90 %; default 52. Of the canvas's width
- baseMaterial (Nut material): one of Gloss plastic, Satin plastic, Matte plastic, Rubber, Bakelite, Brushed aluminium, Spun aluminium, Chrome, Steel, Gold, Brass, Copper, Anodized, Painted metal; default Chrome
- baseColour (Nut colour): a colour; default #FFD8DBE0
Lever:
- leverLength (Lever length): 15 to 60 %; default 38. Of the canvas's height
- leverWidth (Lever thickness): 3.0 to 25.0 %; default 11.0. Of the canvas's width
- tip (Tip): one of Plain, Ball, Paddle; default Plain
- angle (Tilt): 10 to 50 deg; default 30. How far it leans up or down
- leverMaterial (Lever material): one of Gloss plastic, Satin plastic, Matte plastic, Rubber, Bakelite, Brushed aluminium, Spun aluminium, Chrome, Steel, Gold, Brass, Copper, Anodized, Painted metal; default Chrome
- leverColour (Lever colour): a colour; default #FFE2E5EA
View:
- tilt (Seen from (0 straight above)): 0 to 60 deg; default 0. Where the camera is: 0 straight above (as a plugin's controls are seen), up to 60 from in front, the sides of the control showing
- perspective (Perspective): 0 to 100 %; default 30. How much nearer parts look bigger: 0 none, 100 close up
- shadow (Shadow): 0 to 100 %; default 50. How dark the shadow it casts on the panel is (lit by the panel's global light)
- softness (Shadow softness): 0 to 100 %; default 55
- exposure (Exposure): -100 to 100 %; default 0. Brighter or darker overall

### Slider
Track:
- trackLength (Track length): 40 to 98 %; default 84. Of the canvas's length
- trackWidth (Track width): 2.0 to 40.0 %; default 9.0. Of the canvas's width
- trackColour (Track colour): a colour; default #FF08080A
- ticks (Scale marks (0 none)): 0 to 31; default 11
- tickLength (Mark length): 4.0 to 45.0 %; default 16.0; only when ticks!=0. Of the canvas's width
- tickColour (Mark colour): a colour; default #FFBDBDC2; only when ticks!=0
Cap:
- cap (Cap): one of Classic fader, Block, Round; default Classic fader
- capWidth (Cap width): 20 to 100 %; default 74. Of the canvas's width
- capLength (Cap length): 4.0 to 50.0 %; default 14.0; only when cap!=2. Of the canvas's length
- capHeight (Cap height): 3 to 60 %; default 26. Of the canvas's width
- grooves (Grooves): 0 to 12; default 4; only when cap=0
- material (Cap material): one of Gloss plastic, Satin plastic, Matte plastic, Rubber, Bakelite, Brushed aluminium, Spun aluminium, Chrome, Steel, Gold, Brass, Copper, Anodized, Painted metal; default Gloss plastic
- colour (Cap colour): a colour; default #FF2A2A2E
- lineColour (Line across it): a colour; default #FFF4F1EA. Its centre line (see-through: none)
View:
- tilt (Seen from (0 straight above)): 0 to 60 deg; default 0. Where the camera is: 0 straight above (as a plugin's controls are seen), up to 60 from in front, the sides of the control showing
- perspective (Perspective): 0 to 100 %; default 30. How much nearer parts look bigger: 0 none, 100 close up
- shadow (Shadow): 0 to 100 %; default 50. How dark the shadow it casts on the panel is (lit by the panel's global light)
- softness (Shadow softness): 0 to 100 %; default 55
- exposure (Exposure): -100 to 100 %; default 0. Brighter or darker overall

## Materials
Gloss plastic (a clear coat over its colour: the classic synth knob), Satin plastic (softer sheen), Matte plastic, Rubber (soft, no shine: drum pads, soft-touch knobs), Bakelite (deep glossy brown-black or cream, vintage), Brushed aluminium (straight brush lines), Spun aluminium (circular brush rays round the middle: the classic silver knob top), Chrome (a mirror), Steel, Gold, Brass, Copper, Anodized (coloured metal with a fine sheen: red, blue, black, purple), Painted metal. The colour tints every material (a gold colour on Brass is warm brass; a blue colour on Anodized is blue aluminium); chrome and steel want light greys.

## Recipes
- Groovebox black with LED rings (the classic acid box): knob `"skirt": false, "material": "Matte plastic", "colour": "#FF141416", "grip": "Ribs", "gripCount": 44, "gripDepth": 1.5, "ring": true, "ringColour": "#FFFF8A1A", "ringSegments": 21`; step buttons `"shape": "Rounded square", "light": "The whole cap", "lightColour": "#FFFF8A1A", "lightOff": 5, "bezel": true, "bezelMaterial": "Matte plastic", "bezelColour": "#FF111113"` (dark when off, glowing when on).
- Vintage British console: knob `"material": "Bakelite", "colour": "#FF2B1A12", "grip": "Flutes", "gripCount": 18, "gripDepth": 5, "taper": 14, "skirtLine": true, "pointerColour": "#FFE8DCC0"`; cream panels want `"Cream fluted"` from the gallery.
- Silver mastering: knob `"material": "Spun aluminium", "colour": "#FFC8CBD0", "skirt": false, "pointer": "Groove", "pointerColour": "#FF202024"`; faders `"cap": "Block", "material": "Brushed aluminium"`.
- Luxury gold: `"Gold cap on black"` or `"material": "Gold", "colour": "#FFE0B860", "grip": "Flutes", "gripCount": 30` with a black body.
- Mixer channel: sliders `"Grey console"` or `"Red line fader"`; mute and solo keys `"Mute key"`, `"Solo key"`; a pan knob `"Cyan pan ring"`.
- Pointer nearer the rim (a big-faced knob): `"pointerLength": 40, "pointerY": 25`.
- Tall knobs (seen from an angle: `tilt` 20 and up, or the panel camera): a tall cylinder `"height": 120, "taper": 0`, a tall cone `"height": 150, "taper": 75`, a cone to a point `"height": 180, "taper": 90, "top": "Dome"`; give them `"pointerSide": true` so their line runs down the side too. Straight from above (`tilt` 0) a tall knob looks like any other: its height shows only at an angle. A tall one is fitted into its box as a whole, so it looks smaller: give it a bigger box.

## Sizes and cost
- 3D looks are rendered, not drawn: every frame is a picture made once and kept. A panel renders each 3D look at each size and place it's shown at, so keep to a few looks (one per kind of control, a hero's own at most) and sensible sizes: knobs 40 to 110 px, a hero up to 160; buttons 28 to 64; sliders 30 to 60 wide.
- Their shadows fall on the panel past their box (the looks list says how far): leave that room (the 3d-panel skill).
