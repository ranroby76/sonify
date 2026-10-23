<!-- D:\workspace\Fanan_web_hub\src\skill_inspiration_looks.md -->
---
name: inspiration-looks
for: plugins
title: Inspiration pool: looks
description: Your inspiration pool of whole-panel looks (console strips, a black master-bus console, a silver mastering panel, synth panels, an amp head, glossy, coloured and wood-trim panels, a hero-picture layout, two colour schemes), each a recipe in the studio's own features: the background's PanelPainter settings, the knob, slider and switch looks, meters, lamps, selectors, keyboards, fonts and layout. Read it before designing or redesigning any panel, and whenever the user picked a look beside Send or names one.
---
# Inspiration pool: looks

These are the panel looks you draw on when you design a plugin's window: a general look for the whole panel, mixed with parts from the others (the inspiration-parts skill has the parts one by one). They're your own originals, made in the studio's features: they take a feel (materials, colours, layout, the kind of controls), never a product's artwork, logo, name or exact layout. Never copy a real product one to one, never write a brand or product name on the panel, and never call a look by a real product's name to the user.

## How to use the pool
1. **The user picked a look** (the context says so: they chose it beside Send, or named it): build in that look, whatever the request says about the sound. Keep its palette, materials and control kinds; fit its layout to the plugin's controls.
2. **Otherwise choose**: one general look from the request's words and the plugin's kind (a channel strip or console: a console look; a mastering tool: Silver mastering panel or Dark master-bus console; a synth: '70s synth panel, Flow panel or Pearl-gloss console; a guitar amp or drive: Vintage amp head; lo-fi and vintage: Worn grey console, Olive military module, Mahogany studio). A request for something "new" or "surprising" may take a less obvious one.
3. **Mix with restraint**: one general look, then at most two or three parts from elsewhere (a tube glow from one, jewel lamps from another), never two general looks fighting in one window.
4. **Say it in one line**, as you present the design: "Look: Classic console strip, with jewel lamps and a tube glow between the meters." Nothing more about the pool.
5. **Make it the plugin's own**: its own name and title, its own section names, its own accent colour where the look allows one. Recolour looks with `make_look` (the Ring of a Ring knob, the Skirt of a Two-tone knob, the Cap of a Colour cap knob) rather than settling for a colour that fights the panel.
6. The looks below that end in "knob", "fader", "toggle", "key", "button" or "switch" are built-in looks of the pool (the project's looks list names them). Classic ones (Black knob, Silver knob, Cream knob, Gold knob, Steel knob, Titanium knob, Walnut knob, Marble knob, Black and silver knob, Pointer knob, Scale knob, Value ring, Fader, Fader across, Toggle switch, Push button, Light button) mix in freely.

Settings below are the toolbox's: lamps', selectors', keyboards' and labels' `"display"`, meters' `"display"`, shapes and lines, and the built-in RangeSlider, MeterKnob and DualKnob (the builder guide has them all). The background is PanelPainter's (the background-painter skill), painted with `paint_picture` once the plugin has been built.

## The looks

### Classic console strip
A tall channel strip from a classic large-format console: everything stacked top to bottom in signal order.
- Panel: PanelPainter `hammertone`, color `#FF4D5862`, rim 4, screws 1, key 0.45, vignette 0.35; a `recess` (radius 6) per section, `groove` lines between sections, an aluminium `plate` with screws for the name at the top.
- Controls: Arrow knob for gain and the hero controls; Ring knob for the EQ bands, its Ring recoloured per band (low `#FF8A5A2B`, mid `#FF2F6FD6`, high `#FFC8202A`); Console key or Jewel push button for switches; Chunky cap fader at the foot.
- Live: a `vu` meter at the top (or two `vuside` side by side), `dome` lamps in red and green.
- Text: white engraved section titles in Oswald, captions in Barlow Condensed; the name in Michroma.
- Layout: about 300 x 800 for one channel; a narrow column with the meter on top, the fader at the bottom.

### Dark master-bus console
A black master section, wide and calm, with gold accents and two big meters.
- Panel: `anodized` color `#FF1A1B1E`, key 0.5, vignette 0.5, rim 3; a `gold` brushed `plate` for the name; thin gold `inlay` stripes (2 px) under the section titles.
- Controls: Soft shadow knob (its gold line), big (80 to 96 px) for the hero controls; Knurled insert knob for trims; Toggle switch or Jewel push button with an amber lamp.
- Live: two `porthole` meters (cream faces) side by side, `dome` lamps in amber.
- Text: Cinzel or Michroma titles in `#FFD6B25E`, captions in Inter.
- Layout: wide (about 900 x 420), meters centred, controls in two balanced rows.

### '70s synth panel
A synthesizer of the seventies: wood cheeks, a black or brushed centre, coloured caps by section.
- Panel: `aluminium` brushed (or `powder` `#FF202024`), with walnut `wood` `raise` blocks as cheeks at the left and right; a stripe of colour (`inlay`) along the top per section.
- Controls: Colour cap knob, its Cap recoloured per section (oscillators `#FF1F9E9A`, filter `#FFE0812B`, envelopes `#FFE8C547`); Paddle switch or Toggle switch; selectors `"display": "keys"`.
- Live: `lcd` labels for values, `dot` lamps.
- Keyboard: across the bottom, `"display": "reverse"` for a vintage feel or `classic`.
- Text: Righteous or Bebas Neue for the name, Barlow Condensed captions.

### Silver mastering panel
Clean, bright, precise grey metal: measurement-grade mastering gear.
- Panel: `aluminium` color `#FFC9CCD1` brushed, key 0.4, vignette 0.25, rim 3, screws 1; `recess` sections shallow (depth 2); a dark `screen` for the graph.
- Controls: Spun aluminium knob (red line) and Knurled insert knob; Slide switch or Console key; RangeSlider for a band's low and high.
- Live: `edge` meters (edgewise strips), a `filter` or `scope` display in the screen, `readout` labels under the main knobs showing their values.
- Text: dark grey `#FF2A2C30` printed captions in Michroma or Rajdhani; values in Roboto Mono.

### Electric-blue panel
A bold powder-coated blue face with white legends and chrome controls: cold, clean, a little icy.
- Panel: `powder` color `#FF1F4FA8`, key 0.5, vignette 0.3, rim 4; white `inlay` lines; `frosted` glass `plate` for the name.
- Controls: Mirror dome knob or Silver knob; Pill toggle (Track on recoloured `#FF8FE3FF`); selectors `"display": "pill"`.
- Live: `segments` meters in cyan and white (`colorLow` `#FF6FE8FF`), `dome` lamps in ice blue.
- Text: white Exo 2 or Montserrat.

### Cream-module console
Cream enamel modules set into a dark frame, each with metal handles top and bottom.
- Panel: a dark frame (`powder` `#FF232327`) with one `raise` per module in `ceramic` color `#FFEDE6D3`; each module gets two chrome handles: a bent line (`"type": "line"`, `"lineWidth": 7`, `"material": "chrome"`, a small bow) at its top and bottom (inspiration-parts: metal handles).
- Controls: Two-tone knob (red skirt) and Arrow knob; Jewel push button; Chunky cap fader.
- Live: `vu` meter with a cream face, `jewel` lamps in red.
- Text: black printed captions (`#FF1C1A17`) in Oswald.

### Signal-red console
A red powder-coated console: loud, confident, vintage broadcast.
- Panel: `powder` color `#FFB0212A`, key 0.5, vignette 0.4, rim 4, screws 1; black `recess` sections; a cream `plate` for the name.
- Controls: Oxblood knob or black Two-tone knob (Skirt `#FF141416`), Toggle switch; selectors `"display": "keys"`.
- Live: `vu` meters with cream faces, `square` lamps in white or amber.
- Text: cream `#FFF2E9D6` Bebas Neue titles, Barlow Condensed captions.

### Worn grey console
A grey workhorse that has seen decades of sessions: matte, scuffed, honest.
- Panel: `hammertone` color `#FF6B7076` with `grain` 1.5, a `noise` pattern (multiply, opacity 0.2), rim 4, screws 1, vignette 0.5.
- Controls: Fluted bakelite knob (black: recolour its Flutes, Skirt and Top `#FF1A1A1C`) or Lobed knob; Toggle switch; Paddle switch.
- Live: `edge` meters, `dome` lamps in amber.
- Text: Russo One or Bebas Neue stencil-like titles, white printed.

### Pearl-gloss console
Glossy white lacquer, pearl and glass: luxurious, soft and bright.
- Panel: `ceramic` color `#FFF3F1EC` (or `plastic` glossy white), key 0.6, vignette 0.2, rim 2; `pearl` `inlay` stripes; displays under clear glass: a `clearglass` rectangle (shape) over each screen.
- Controls: Pearl knob or Gloss dome knob; Pill toggle; selectors `"display": "pill"`.
- Live: `arc` meters or `neon` in soft colours; `dome` lamps.
- Text: `#FF3A3A40` Montserrat or Playfair Display.

### Coral twin console
Two mirrored channels on a coral face, a big knob with a meter in the middle of each.
- Panel: `powder` color `#FFE26D5A`, key 0.45, vignette 0.3, rim 3; a darker coral `recess` per channel.
- Controls: MeterKnob for each channel's main control (threshold, drive), its "source" the channel's level; Ring knobs (Ring recoloured `#FF1A1A1C`) for the rest; Console key.
- Live: `porthole` meters, `dome` lamps in white.
- Text: `#FF2A1410` Montserrat, the name in Righteous.
- Layout: left and right halves mirrored about the centre.

### Hero picture layout
When the user asks for a big picture in front: the picture is the hero, the controls frame it.
- Panel: the picture large at the top or centre (an `image` element, `"mode": "fill"`), a dark strip (`powder` `#FF141416` or a `tintedglass` rectangle over the picture's foot) carrying one row of controls below it.
- Controls: few, quiet: Soft shadow knob or Value ring; Pill toggle.
- Text: the name over the picture in a display font (Bebas Neue, Playfair Display), with a soft `dropShadow` effect on a shape behind it or a `tintedglass` band.
- Keep the picture's centre clear of controls; let it breathe.

### Vintage amp head
A guitar amplifier's head: black tolex, grille cloth, a brushed faceplate with numbered knobs.
- Panel: `tolex` color `#FF19191B`; the grille as a `raise` part in `felt` color `#FF5A4A3A` with a `weave` pattern; a brushed `aluminium` or `gold` faceplate strip (`plate`) across the bottom carrying the controls.
- Controls: Numbered skirt knob (70 px or more) or Chrome hub pointer knob; Paddle switch for standby and power.
- Live: a red `jewel` lamp as the pilot light.
- Text: script or bold logo in Pacifico or Righteous on the grille; captions in Oswald, printed dark on the faceplate.

### Olive military module
Olive drab, stencilled, riveted: built for a field radio, now a studio tool.
- Panel: `powder` color `#FF4E5A3A` (or `hammertone`), rim 5, screws 1, `vents` or a `grilles` panel; black `recess` sections; small `plate`s with screws.
- Controls: Fluted bakelite knob (black) or Lobed knob; Toggle switch; Slide switch.
- Live: `edge` meters, `eye` lamp (magic eye) as a tuning or level indicator.
- Text: Russo One stencil-style capitals, `#FFE8E2C8`.

### Wood-trim black panel
A black face between wooden cheeks: classic studio outboard.
- Panel: `anodized` color `#FF151517`; walnut `wood` `raise` cheeks left and right (about 40 px), `groove` lines between sections.
- Controls: Soft shadow knob or Black and silver knob; Knurled insert knob for trims; Toggle switch.
- Live: a central `vu` meter (cream face), `dome` lamps in red.
- Text: white printed captions in Inter, the name in Michroma.

### Magenta glow panel
Deep plum with neon pink: electronic, nocturnal, a little synthwave.
- Panel: `anodized` color `#FF2A1230`, key 0.7, vignette 0.6; a `light` pool of `#FFFF3FA4` behind the hero display.
- Controls: Ring knob (Ring `#FFE0389A`) or Value ring (Arc `#FFFF3FA4`); Pill toggle (Track on `#FFFF3FA4`).
- Live: `neon` meters, `vfd` labels in pink, `arc` meters.
- Text: Monoton for the name, Exo 2 captions in `#FFF6D6EC`.

### Amber lamp console
Warm dark brown with amber light everywhere: displays, lamps, meters glow like old valves.
- Panel: `bakelite` color `#FF2B1A12`, key 0.5, vignette 0.5, rim 3.
- Controls: Fluted bakelite knob, Chrome hub pointer knob; Console key (amber lamp).
- Live: `vfd` and `readout` labels in amber (`"color": "#FFFFB347"`), `tube` lamps glowing amber between meters, `segments` meters in amber.
- Text: Share Tech Mono for values, Oswald captions in amber.

### Mahogany studio
Polished mahogany with brass: a boutique studio box with a big curve display.
- Panel: `burl` or `wood` color `#FF5C2A1A` polished, a brass `plate` for the name, a dark `screen` (glow `#FFFFC46B`) for the EQ curve.
- Controls: Fluted bakelite knob or Walnut knob; Gold knob for the hero; Toggle switch.
- Live: a `filter` curve display in the screen (warm colour), `porthole` meter, `dome` lamps in amber.
- Text: Playfair Display or Cinzel in brass `#FFD9B56A`.

### Sunray module
A module glowing warm yellow to orange like a late sun, black controls standing out on it.
- Panel: `powder` color `#FFF2A93B` with `color2` `#FFE0602A` (a two-tone shade), a `sunburst` pattern (screen, opacity 0.15) behind the main section, black `recess` strips.
- Controls: Black knob or Two-tone knob (Skirt `#FF141416`); Toggle switch.
- Live: `porthole` meter, `dot` lamps in red.
- Text: black Righteous name, Barlow Condensed captions.

### Flow panel
Modern and flowing: a flat dark panel, soft gradients, everything readable at a glance.
- Panel: `smooth` color `#FF1C1F26` with `color2` `#FF2A2F3A`, key 0.3, vignette 0.2, no screws; `inlay` accent stripes.
- Controls: Value ring or Ring knob; RangeSlider for high-pass and low-pass; Pill toggle; selectors `"display": "pill"`.
- Live: `bar` or `arc` meters, `readout` labels.
- Text: labels with arrowheads (`"arrows"`) pointing at what they name; Inter or Montserrat.

### Black and oxblood
A colour scheme to lay over any layout: black panel, oxblood accents, cream type.
- Panel `#FF121214` (anodized or powder); accents `#FF5C1519`; text `#FFE9DFC8`.
- Controls: Oxblood knob; Two-tone knob with Skirt `#FF5C1519`; lamps `#FFC8202A`.

### Jewel in a black case
A colour use: a black case holding one precious material, like a jewel in its box.
- 90 % black (`anodized` `#FF101012`, deep vignette 0.6), 10 % one precious material: a `gold` plate, Gold knob or Soft shadow knob (gold line), gold `inlay` lines; nothing else coloured.
- Shadows do the work: Soft shadow knob's two shadows, `dropShadow` effects on raised shapes.
