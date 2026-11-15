<!-- D:\workspace\Fanan_web_hub\src\skill_3d_panel.md -->
---
name: 3d-panel
for: plugins
title: 3D panels: the whole picture
description: How a panel of 3D controls becomes one believable object instead of rendered knobs pasted on a picture: one camera (the same view on every 3D look, the panel camera), one light (every look, shape, painted background and text lit from the global light), one surface (controls standing on the panel, screens set into it, plates and wells at the right depth), consistent scale, room for shadows, and how keyboards, meters, lamps and labels join in. With a step-by-step method and a final check. Read it with the 3d-controls skill before designing or changing any panel that wears 3D looks.
---
# 3D panels: the whole picture

The controls are half the story. A 3D knob looks real only when everything around it agrees with it: seen by the same camera, lit by the same light, standing on the same surface, at the same scale. Get one of them wrong and the panel reads as stickers on a picture. These are the rules that make it one object, like a photograph of real hardware.

## 1. One camera
- Every 3D look on the panel has the same `tilt` ("Seen from") and the same `perspective`. Set them on every look you make or bring in (`make_3d_look` with those two settings), never one knob seen from above and the next from in front.
- `tilt` 0 (straight above) is how plugin panels are usually seen: crisp, flat, every control the same. 10 to 25 is a gentle desk view: the knobs show their sides, the faders their caps' fronts. 30 to 60 is dramatic (a product shot, a seated player's view): keep it for a panel that's meant to be a scene, and make the window taller for it (the controls get shorter on screen and their tops and shadows reach further).
- `perspective` 20 to 35 looks natural; 0 is a flat, technical view; above 50 only for big hero controls.
- The panel camera: the window's `"camera"` (0..100, the layout's top level, beside `"light"`; 50 when it's left out). It's one camera over the whole panel: a 3D control on the left shows a little of its right side, one on the right a little of its left side, one high on the panel a little of its lower side and one low on the panel a little of its upper side, as in a photo of real gear. 30 to 60 suits most panels; 0 sees each control straight on (use it for a strict, technical look); 70 and up is a wide lens, close to the panel: strong at the edges, so keep the important controls near the middle. Layered looks don't change with it.
- The 3D keyboard joins in: a `keyboard`'s `"tilt"` (0..75), `"yaw"` (-45..45) and `"perspective"` (0..100) put it in 3D; give it the controls' tilt (at least 15 so it reads as 3D) and `"yaw": 0`, and the perspective the looks have.

## 2. One light
- The panel has one light, the layout's `"light"` (`"angle"` 120 the upper left, `"altitude"` 30): the 3D looks are rendered in it, the painted background, the shapes' bevels and shadows and the text effects too. Never give anything a light of its own (a bevel's own `"angle"`, a painter's own `"light"`): it's the surest way to break the picture.
- Shadows fall away from the light: with the light at the upper left they fall to the lower right. Leave room on that side of every control (the next point) and don't put a control's label where its shadow falls on it.
- Change the light only when the user asks; when they do, everything follows at once, and the painted background is painted again.

## 3. One surface
- The panel is the ground: the controls stand on it. Paint it as a real surface (`paint_picture` with PanelPainter, the background-painter skill: powder coat, brushed aluminium, anodized, wood, leather, tolex...) rather than a flat colour, and keep its texture fine: a coarse texture next to small rendered knobs breaks the scale.
- Depth comes in a few steps only: the panel; plates raised a little on it (a rectangle shape with a soft outer bevel and a drop shadow, its corners rounded) for groups; wells set into it (an inner bevel or an inner shadow, `"direction": "down"`) for screens, meters and scopes; the controls on top. Never more than three levels; never a plate on a plate on a plate.
- Screens (3D displays, scopes, meters, readouts) are set into the panel: a dark recessed well with a thin bezel, their glass flush, not floating over it.
- Lamps, LEDs and meters are flat light on the surface: give them `dome` or `jewel` displays (they have relief of their own, lit by the same light) rather than flat dots beside rendered knobs.
- Labels are printed on the panel: small, light on dark (or dark on light), no drop shadow floating them off it; on metal, engraved lettering (`"textEffects"` with an inner shadow, or a bevel pressed `"down"`), lit by the same light.

## 4. Scale
- Real hardware has a few sizes, repeated: one hero knob (1.5 to 2 times the others), the main knobs, small trim knobs (about two thirds of the main ones). Buttons about half a main knob; faders' caps as wide as about two thirds of a main knob.
- Same kind, same size, same look: every knob of a row the same diameter, every button of a step row the same size.
- Material scale matters: knurling, flutes and ribs look right only at their size: a 40 px knob wants fewer, deeper flutes (`gripCount` 16 to 24), a 100 px one can take 40 to 60 fine ribs.

## 5. Room and rhythm
- A rendered control throws a shadow past its box: the looks list gives each look's reach ("its shadow falls N% of its size past the control"): keep that much clear space on its shadow side, on top of the panel's usual 8 px rhythm. When room is short, make the controls smaller rather than packing them tighter.
- Align centres, not boxes: a row of knobs shares its centre line, a column of faders its tracks' line. A light ring adds about 25% to a knob's footprint: space ringed knobs for it.
- Symmetry reads as engineered: with the panel camera on, a symmetrical layout leans symmetrically, which looks like a real photo; a lopsided one looks tilted.

## 6. Colour and material harmony
- Pick the controls' materials from the panel's: a brushed aluminium panel takes black or silver knobs and chrome or black faders; a black panel takes black knobs with coloured LED rings or silver caps; wood takes brass, bakelite or cream.
- One accent colour, used for light only: LED rings, lit buttons, pointer lines, lamps, a screen's trace. The knob bodies stay neutral.
- A lit button's `lightOff` low (0 to 8) on a dark panel, so off is clearly off; 25 to 45 for tinted see-through caps (arcade buttons).

## 7. Method
1. Read the panel-taste skill for the character, then decide the camera (`tilt`, `perspective`, the window's `"camera"`) and keep the light as it is.
2. Paint the background (PanelPainter) in the panel's material; add plates and wells for the groups and the screens.
3. Bring the 3D looks from the 3D Gallery (`find_gallery_looks` and `use_gallery_look` with `"3d": true`); set each one's `tilt` and `perspective` to the panel's (`make_3d_look` from it), and its colours to the accent.
4. Place the controls in their sizes and rows, with room for their shadows; put the keyboard in the same view.
5. Lamps, meters, labels and screens last, in the panel's terms (recessed, printed, lit).

## 8. Final check (before you finish)
- Every 3D look: the same `tilt` and `perspective`? None with a light of its own?
- Every shadow falling the same way, onto the panel, onto nothing it shouldn't cover?
- At most three depth levels; screens recessed; plates raised; nothing floating?
- Sizes: one hero, the rest in two or three sizes, each row the same?
- No layered knob beside a 3D one; one look per kind of control (a hero's own at most)?
- The accent only on light; materials that belong together?
