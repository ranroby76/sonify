<!-- D:\workspace\Fanan_web_hub\src\skill_inspiration_parts.md -->
---
name: inspiration-parts
for: plugins
title: Inspiration pool: parts
description: The parts of your inspiration pool, each with how to build it in the studio: every kind of knob (arrows, coloured metal rings, two colours, numbered skirts, mirror domes, cogs, lobes, spun metal, 3D, soft shadows, concentric, with a built-in meter, with a value frame), console faders, high-pass and low-pass range sliders, pill toggles, console keys, pill and key selectors, jewel and domed LEDs, glowing tubes and magic eyes, VU, upright VU, edgewise and round meters, graphs under glass, analog type, labels with arrows, metal module handles, glass, bending lines, background materials and shadow nuance, keyboards. Read it with inspiration-looks when designing a panel, or when the user asks for one of these parts.
---
# Inspiration pool: parts

Parts to mix into a panel's general look (inspiration-looks). Each is built from the studio's own features, so it's drawn live, in Edit, Play and the finished plugin. Exact settings for every element are in the builder guide.

## Knobs: every method
- **Arrow knobs** (a cap shaped like an arrowhead pointing the value): Arrow knob. A pointer bar on a chrome hub: Chrome hub pointer knob. A chicken head: Pointer knob.
- **Coloured metal frames**: Ring knob: a black knob inside an anodized ring; recolour the ring per section with `make_look`: `{ "name": "Ring knob red", "from": "Ring knob", "layers": { "Ring": { "color": "#FFC8202A" } } }`.
- **Two-colour knobs**: Two-tone knob (a black cap on a coloured skirt, the line running onto it; recolour the Skirt), Oxblood knob.
- **Knobs that reflect the room**: Mirror dome knob (a chrome dome with the room's light above and floor below), Gloss dome knob (glossy white), Pearl knob.
- **Spun and machined metal**: Spun aluminium knob (concentric spun face, red line), Knurled insert knob (knurled edge, black insert).
- **Grip shapes**: Lobed knob (five lobes), Cog knob (toothed, chrome screw-head cap), Fluted bakelite knob, Colour cap knob (fluted black skirt, coloured cap).
- **Numbered skirts**: Numbered skirt knob: numbers 1 to 10 printed on a skirt that turns under a fixed red mark (vintage amps; 70 px or more).
- **A scale with arrows out to its numbers**: Scale knob for the numbers round it, and for each number a short `line` shape with `"arrow": "end"` from just outside the knob towards the number (`"lineWidth": 1.5`, the panel's text colour). Work their points out on the circle the numbers lie on: angle -135 to +135 degrees, 0 straight up.
- **3D, angled depth**: Tall 3D knob (raised cap, long cast shadow).
- **Shadows with nuance**: Soft shadow knob: a tight, dark contact shadow and a long, faint, soft one, as a real knob casts in a lit room. Leave room for the long one.
- **Two knobs in one** (frequency and gain, coarse and fine): `{ "type": "DualKnob", "params": { "outer": "eq.freq", "inner": "eq.gain" }, "label": "Mid", "x": 40, "y": 60, "w": 96, "h": 114 }`.
- **A big knob with a meter built in** (a threshold against the level it acts on): `{ "type": "MeterKnob", "param": "comp.threshold", "source": "plugin.in L", "label": "Threshold", "x": 300, "y": 60, "w": 170, "h": 188 }` (`"settings": { "meter": "linear" }` for a display's 0..1, `"face"` and `"plate"` colours).
- **Value frames** (a small window showing a knob's value): a `label` with `"display": "readout"` and the knob's `"param"`, just under or over the knob: `{ "type": "label", "display": "readout", "param": "eq.freq", "x": 46, "y": 128, "w": 64, "h": 20 }`. Amber text unless it has a `"color"`; its window `"background"`.

## Faders and sliders
- **Console faders**: Chunky cap fader (a ticked scale either side; across: Chunky cap across), Ribbed fader (aluminium cap with ribs), Glass cap fader (clear glass cap over a lit slot), Ball handle across (a chrome ball on a rod).
- **High-pass and low-pass on one track**: `{ "type": "RangeSlider", "params": { "low": "filter.hp", "high": "filter.lp" }, "label": "Band", "x": 40, "y": 300, "w": 260, "h": 56, "settings": { "ticks": 10 } }`: frequencies spaced as heard, the band between the handles lit in its `"color"`; upright in a tall box.

## Switches and buttons
- **Pill toggles**: Pill toggle (its knob slides across, its track lights; recolour "Track on").
- **Console-style buttons**: Console key (a key cap with a lamp strip), Light button, Push button; a row of console keys choosing between options is a selector with `"display": "keys"` (stacked in a tall box).
- **Jewel buttons**: Jewel push button (chrome, a jewel lamp in its middle). Vintage: Paddle switch, Slide switch, Toggle switch.

## Selectors
`{ "type": "selector", "display": "pill", "param": "comp.attack", "options": ["Fast", "Med", "Slow"], "x": 40, "y": 200, "w": 200, "h": 52 }`
- `pill`: a rounded track with a sliding pill (attack, release, ratio choices).
- `keys`: console key caps, the chosen one lit; `lamps`: the names with small lamps (a vertical list in a tall box); `vfd`: a glowing window showing the chosen name, its arrows step; `tiles`: coloured tiles along a colour ramp. Its `"color"` lights the chosen one.

## Lamps and LEDs
`{ "type": "lamp", "display": "dome", "source": "plugin.out L", "threshold": 0.3, "color": "#FFFF3B30", "x": 20, "y": 20, "w": 28, "h": 28 }`
- `dome`: a console's domed LED; `jewel`: a faceted pilot light in chrome; `square`: a flat LED as wide as its box; `dot`: the plain one.
- **A glowing tube** (between two meters, or a big tube as the hero): `"display": "tube"`: a vacuum tube whose filament glows with the level (it follows the level by itself): about 40 x 90 (or 70 x 160 for a big one), its `"source"` the signal the meters watch, `"color"` the glow (amber `#FFFF8A2A`).
- **A magic eye**: `"display": "eye"`: a green ring whose dark wedge closes as the level rises.
- **A row of red output LEDs**: lamps with `"display": "square"`, red, each watching the output with a rising `"threshold"` (0.05, 0.1, 0.2, 0.35, 0.5, 0.7, 0.9), side by side; or a `meter` with `"display": "segments"` and all three colours red.
- Any lamp can follow the level instead of switching: `"follow": true`.

## Meters
`{ "type": "meter", "source": "plugin.out L", "display": "vuside", "x": 20, "y": 40, "w": 64, "h": 150 }`
- `vu`: the classic needle (about 140 x 90); `vuside`: an upright VU, the needle swinging up and down (tall: about 64 x 150, two side by side for left and right); `edge`: an edgewise meter, a scale on a lit band behind a long window (about 200 x 40, or upright 40 x 170); `porthole`: a round gauge in a chrome bezel (square, 90 to 140); `segments`, `dots`, `neon`, `tube`, `bar`, `arc`. Their faces are cream unless a `"background"` is chosen; `"colorHigh"` is the red zone.

## Graphs and screens
- **An EQ or filter graph**: a `filter` (or `envelope`) display on a dark PanelPainter `screen` part (`"glow"` its colour), with a `clearglass` rectangle shape over it for the glass; a warm colour on vintage looks (`#FFFFC46B`), cyan or white on modern ones.
- Scopes: `scope`. 3D graphs: the 3d-displays skill.

## Type
- **Old analog lettering** (stamped and printed legends): Share Tech Mono or Roboto Mono for small engraved-looking legends, Michroma for nameplates, Oswald or Barlow Condensed for captions; PanelPainter `texts` with `"style": "engraved"`, `"embossed"` or `"printed"` for fixed titles.
- **Labels with arrows**: `{ "type": "label", "text": "LOW CUT", "arrows": "both", ... }` (`left`, `right`, `both`, `up`, `down`).
- **Glowing and backlit text**: a `label` with `"display": "vfd"` (glowing, cyan unless coloured) or `"lcd"` (a backlit window with segment characters); with a `"param"` and `"display": "readout"` it shows the value.

## Panel construction
- **Metal handles on modules** (top and bottom of a rack module): a bent chrome line: `{ "type": "line", "from": [24, 18], "to": [96, 18], "bend": [60, 6], "lineWidth": 7, "material": "chrome", "effects": { "bevel": { "size": 3 }, "dropShadow": { "opacity": 55, "distance": 4, "size": 5 } } }`; one each side of the module's name, or across its top and bottom.
- **Background material and colour, done well**: a quiet main surface (60 %), a darker or contrasting section colour (30 %: `recess` parts), one accent (10 %: plates, inlays, lamps); a gentle key light (0.4 to 0.6) and vignette; one light for everything (the global light).
- **Shadow nuance**: two shadows read as real: a short dark one at the contact and a long soft faint one; on shapes, a `dropShadow` with a small `distance` and `size` plus a second shape behind with a big soft one; on knobs, Soft shadow knob.
- **Glass**: `"material": "clearglass"` (or `"tintedglass"` with a `"color"`) on shapes is see-through: bands across a picture, covers over screens and meters, a glass name plate; Glass cap fader for a glass cap. Glass is for shapes and caps over the panel, not for a painted background.
- **Bending lines**: `line` shapes: dividers, arrows from a label to its control, a cable's curve, handles; `"bend"` bows it through that point (left out: straight), `"arrow"`, `"cap"`, `"lineWidth"`, a colour or any material.
- **Plates, screws, grooves, wood cheeks, tolex, grille cloth**: PanelPainter parts (the background-painter skill).
- **A big picture in front**: an `image` element as the hero (inspiration-looks: Hero picture layout).

## Keyboards
`{ "type": "keyboard", "display": "reverse", "low": "C2", "high": "C6", ... }`: `classic` ivory, `reverse` (black naturals, ivory sharps: vintage), `modern` (slate keys that glow when played); its `"color"` lights the keys that are down (red on a modern one).
