<!-- D:\workspace\Fanan_web_hub\src\skill_background_painter.md -->
---
name: background-painter
for: plugins
title: Background painting
description: How to paint the panel's own pictures with paint_picture and the built-in PanelPainter: a whole panel lit by one light (the knobs' light) in any of 38 materials (powder coat, aluminium, steel, silver, chrome, gold, rose gold, brass, copper, bronze, liquid titanium, gunmetal, anodized, carbon, marble, black marble, granite, slate, concrete, terrazzo, plastic, rubber, bakelite, ceramic, wood, burl, leather, tolex, felt, paper, glass, pearl...), with a finish (polished, satin, brushed, hammered, sandblasted, matte, liquid), Photoshop-style pattern layers over it (diamond plate, knurl, guilloche, hexagons, carbon weave, damascus... in blend modes or embossed, debossed, engraved), sunk sections, raised plates with screws, glowing screens, wells for knobs, grooves, vents, grilles, pools of light and engraved words; how to repaint one from its recipe; and how to write a painter of your own with stella_paint.h. Read it when a panel needs a background or any picture the user didn't provide.
---
# Background painting

The panel's background is a picture painted in code: `paint_picture` runs a painter once and saves `gui/images/<file>.png`, and the panel shows it like any picture, free while the plugin plays. The built-in **PanelPainter** paints a whole panel from its settings, nothing to write: one light falls across everything (the panel's global light, the one the knobs' looks and the shapes are lit by), metals mirror a studio room, stone and lacquer carry a clear coat, raised plates catch the light on one edge and cast a soft shadow on the other, sunk sections take their rim's shadow, screws and grooves agree. Painted pictures are yours to repaint; the user's own pictures are never overwritten.

## Steps
1. The plugin must have been built once (the painters are part of its build).
2. Plan the parts from the layout: a sunk section or a raised block under each group of controls, a well under each important knob, a screen under each display, a plate for the name.
3. `paint_picture` with `"element": "PanelPainter"`, `"file": "background"`, the window's `width` and `height`, the main `"color"`, and the `"settings"` (below).
4. Place it: `edit_layout` adding `{ "type": "image", "x": 0, "y": 0, "w": <width>, "h": <height>, "image": "background.png", "mode": "stretch", "at": 0 }`, or first in a new `set_layout`. Where its sections are, the layout needs no `group` frames.
5. When controls move or the window is resized, paint it again at the same name with the new positions: it shows at once.

```json
{ "element": "PanelPainter", "file": "background", "width": 980, "height": 520, "color": "#FF9A9FA6",
  "settings": { "material": "titanium", "key": 0.5, "vignette": 0.45, "rim": 5, "screws": 1,
    "parts": [ { "kind": "plate", "x": 24, "y": 18, "w": 300, "h": 46, "material": "gold", "screws": true },
               { "kind": "recess", "x": 24, "y": 84, "w": 452, "h": 196, "radius": 10, "material": "blackmarble" },
               { "kind": "screen", "x": 500, "y": 84, "w": 456, "h": 196, "glow": "#FF40FF90" },
               { "kind": "raise", "x": 24, "y": 300, "w": 932, "h": 190, "material": "steel",
                 "pattern": "diamondplate", "patternBlend": "emboss", "patternSize": 26 },
               { "kind": "well", "x": 52, "y": 120, "w": 76, "h": 76 },
               { "kind": "light", "x": 760, "y": 0, "w": 220, "h": 140, "shape": "circle", "color": "#FF6A8CFF", "strength": 0.4 } ],
    "lines": [ { "x1": 24, "y1": 292, "x2": 956, "y2": 292, "width": 2 } ],
    "vents": [ { "x": 800, "y": 330, "w": 150, "h": 36, "slots": 8, "across": true } ],
    "texts": [ { "x": 40, "y": 28, "w": 270, "h": 26, "text": "BLACK HOLE", "size": 20, "bold": true, "style": "engraved", "color": "#FF4A3A10", "align": "centre" } ] } }
```

## The surface: material, finish, colours
Every surface (the panel's, and any part's own) is set by the same keys.
- `material`, each with its own colours and finish (a `color` replaces the main colour):
  - Metals: `aluminium` (brushed), `steel` (brushed), `silver` (polished), `chrome` (a mirror), `gold`, `rosegold`, `copper` (polished), `brass`, `bronze`, `gunmetal` (satin), `titanium` (liquid: molten swirls with a faint rainbow), `iron` (cast), `hammertone` (hammered paint), `anodized` (satin, coloured).
  - Stone: `marble` (white, grey veins), `blackmarble` (gold veins), `granite`, `slate`, `concrete`, `terrazzo`, `sandstone`.
  - Plastic, paint and ceramic: `powder` (matte powder coat: modern hardware), `plastic` (glossy), `matteplastic`, `rubber`, `bakelite`, `ceramic` (enamel), `carbon` (carbon fibre under lacquer), `smooth` (plain colour).
  - Wood, leather and cloth: `wood` (`walnut`, `oak`, `rosewood`, `ebony`, `whitewood` (bleached maple), `orangewood` (light orange, cherry/teak), `bordowood` (bordeaux stain), `greywood` (dark grey, weathered) give its colour and grain), `burl`, `guitarwood` (the yellow flamed maple of a guitar top, lacquered), `leather`, `tolex`, `felt`, `paper`.
  - Glass: `glass` (dark, glossy), `frosted`, `pearl` (mother of pearl).
  - Other names work too: `metal` and `brushed` (aluminium), `mirror` (chrome), `liquidmetal` and `mercury` (titanium), `stone` (granite), `enamel`, `lacquer`, `matte`.
- `color`: the main colour (paint_picture's `"color"` is the panel's). `color2`: the second one: marble's veins, granite's flecks, terrazzo's chips, wood's dark grain; on plain materials the colour it shades into away from the light (a two-tone gradient). Tie them to the panel's palette (the panel-taste skill).
- `finish`: `natural` (the material's own), `polished`, `satin`, `brushed`, `hammered`, `sandblasted`, `matte`, `liquid`. Any material takes any finish: brushed gold, hammered copper, polished concrete, liquid chrome.
- `scale`: the texture's size (2: twice as big). `angle`: the direction of brushing, wood grain, veins (degrees). `grain`: its strength, 0 smooth .. 1 .. 2 coarse. `shine`: 0 matte .. 1 mirror (default: the material's). `flow`: liquid swirls 0..1. `seed`: another number, another texture.

## Pattern layers
A pattern is a second layer over the material, as in Photoshop: drawn in a colour with an opacity and a blend mode, or pressed into the surface so the light catches it.
- One layer: `"pattern": "<name>"` with `patternColor`, `patternOpacity`, `patternBlend`, `patternSize`, `patternAngle`, `patternWidth`, `patternDepth` beside it.
- Several (up to 8, bottom first): `"patterns": [ { "pattern", "color", "opacity", "blend", "size", "angle", "width", "depth", "x", "y", "count", "invert", "seed" } ]`.
- `pattern`: `stripes`, `pinstripes`, `checkers`, `dots`, `grid`, `hexagons` (honeycomb), `diamondplate` (tread plate), `knurl`, `crosshatch`, `waves`, `chevron` (herringbone), `scales`, `rings` (concentric, vinyl), `guilloche` (engine-turned), `sunburst` (rays), `perforated` (holes, mesh), `weave` (twill), `noise`, `camo`, `damascus`, `bricks`, `triangles`.
- `blend`: `normal`, `multiply`, `screen`, `overlay`, `softlight`, `add`, `darken`, `lighten` (in its `color`), or `emboss` (raised), `deboss` (pressed in), `engrave` (cut in: fine lines): these shape the surface itself, and the light, reflections and shadows follow.
- `opacity` 0..1 (0.5). `size`: the pattern's cell in pixels (24). `angle` in degrees. `width`: its lines' or shapes' share of the cell (0..1). `depth`: how deep it's pressed, in pixels (1.5). `x`, `y`: the centre of rings, sunburst and guilloche, 0..1 of the area (0.5). `count`: the sunburst's rays (48). `invert`: true swaps it.
- Classics: steel with `diamondplate` embossed (size 24 to 30); aluminium with `knurl` embossed on a strip; brass or silver with `guilloche` engraved (opacity 0.4 to 0.7); black anodized with `hexagons` debossed; carbon with nothing (its weave is its own); leather with `dots` debossed; marble or gold with `pinstripes` engraved, faint.

## PanelPainter's other settings
- `light`, `altitude`: leave them out: the panel's global light (the layout's `"light"`, which lights the knobs' looks and the shapes too) lights it, and when the user moves that light, the background is painted again in it. Given, they're where its light comes from in degrees (120 the upper left, 90 above, 60 the upper right) and how high (10..80).
- `key`: the key light's pool and slope, 0 flat .. 1 strong (0.4 to 0.7); `keyX`, `keyY` (0..1) move the pool (by default it falls towards the light). `vignette`: darker edges, 0..1 (0.3 to 0.6).
- `rim`: the panel's own bevelled edge in pixels (3 to 6 for hardware; 0 none), `corner` its radius. `screws`: 1 for a screw in each corner.
- `parts`, painted in order (lights first): each has `"kind"`, its box `"x"`, `"y"`, `"w"`, `"h"` in window pixels, and as needed:
  - `recess`: sunk into the panel (under a group of controls). `radius` (8), `depth` (the slope's width, 4).
  - `raise`: a raised block; casts a soft shadow away from the light. `depth` 5. `screws`: true for screws in its corners.
  - `plate`: a raised plate, brushed aluminium unless it says otherwise, for the plugin's name or a section's title; `screws`: true.
  - `screen`: a window of dark glass sunk into the panel, with a reflection (under scopes, envelope and filter displays, 3D displays, meters). `color` its glass (`#FF0A1216`), `glow`: the colour of what it shows, lighting it from inside.
  - `well`: a round recess for a knob to sit in (a circle in its box).
  - `inlay`: flush, with a fine engraved outline: a stripe of the accent colour, a badge, a band of marble.
  - `light`: light falling on the panel: a pool of `color` (`"shape": "circle"` for a round pool, `strength` 0..1): an LED's spill, a neon wash behind the title. One or two at most.
  - Any part can be round: `"shape": "circle"`. `strength`: a raise's or plate's shadow, a recess's depth of shade.
  - A part with any surface key (`material`, `color`, `finish`, `pattern`, `patterns`, `shine`...) has a surface of its own; otherwise it's the panel's. A gold plate on titanium, a black marble recess, a diamond-plate strip: each part its own material.
- `lines`: grooves cut in the panel (`"kind": "groove"`) or ridges standing out (`"ridge"`): `x1`, `y1`, `x2`, `y2`, `width` (2). Dividers between rows of controls.
- `vents`: slots cut through: `x`, `y`, `w`, `h`, `slots` (how many), `across` (true: upright slots side by side).
- `grilles`: a perforated grille of round holes: `x`, `y`, `w`, `h`, `spacing` (7), `size` (3.5).
- `texts`: words on the panel, lit from the same place: `x`, `y`, `w`, `h`, `text`, `size`, `bold`, `align` (`left`, `centre`, `right`), `color`, and `style`: `engraved` (cut in), `embossed` (standing out) or `printed`. For the name and fixed section titles; captions of controls stay in the layout.

## Repainting, and the user's own changes
- Every painted picture's recipe is kept: the project description lists it under painted pictures (`painted with { ... }`). To change a background ("make it gold", "add a carbon strip", "brushed instead of polished"), take that recipe, change only what was asked, and `paint_picture` it again at the same name. Everything else stays as it was.
- The user changes it too: a painted background's menu in the Edit tab has its material, finish, colours, texture, a pattern with its blend, colour, opacity, size and angle, and the key light; the window's menu has "Paint a lit background", and every menu has "Global light", the one light for the whole panel. Their changes go into the recipe, so read it afresh before repainting, and keep what they chose unless they ask otherwise.

## Fitting it to the layout
- A section is 12 to 16 px larger than its controls on every side; a well 8 to 12 px larger than its knob (the knob's square, above its caption); a screen 4 to 8 px larger than its display.
- Knob looks cast their own shadows past their box (the layout's looks list says how far): keep that room clear around each knob, and wells large enough to hold it.
- One light for everything: the global light lights the background, the knobs' looks and the shapes alike, so never give the background a light of its own (a panel lit from one side while the knobs are lit from the other looks wrong without anyone knowing why).
- Busy materials (marble, burl, camo, damascus, strong patterns) fight the controls: keep them for plates, inlays, cheeks and strips, or calm them (`grain` 0.5, a pattern's `opacity` 0.2). Under controls, a quiet surface.
- Restraint: a few strong parts read better than many. One plate, sections for the groups, screens where something moves, wells only for the hero knobs.

## Characters
- **Hardware**: `aluminium` or dark `powder`, `rim` 4, `screws` 1, a `plate` with screws for the name, sunk `recess` sections, grooves between rows, `altitude` 30.
- **Luxury**: `blackmarble` or `gunmetal`, `gold` or `rosegold` plates, a `guilloche` engraved on the name plate, `key` 0.5.
- **Liquid metal**: `titanium` or `chrome` with `"finish": "liquid"`, dark `glass` screens with a glow, `vignette` 0.5.
- **Industrial**: `steel` with `diamondplate` embossed, `iron` or `concrete` sections, `knurl` strips, vents, `altitude` 25.
- **Modern flat**: `smooth` or `matteplastic`, `key` 0.3, `vignette` 0.2, `inlay` accent stripes, shallow `recess` sections (`depth` 2), no screws.
- **Dark cinematic**: black `anodized` or `glass`, `key` 0.8, a `light` pool of the accent colour behind the hero display, glowing `screen`s, `vignette` 0.6.
- **Retro synth**: `wood` cheeks (`walnut`) as `raise` blocks at the sides, an `aluminium` or `powder` centre, `plate` for the name; or `tolex` with a `bakelite` section.
- **Boutique**: `leather` or `paper`, `embossed` titles, a `plate` of `brass`.
- **Studio console**: `hammertone` or `anodized` blue-grey, `ceramic` or `paper` inlays for scales, `pinstripes` engraved.

## A painter of your own
For a picture PanelPainter can't make (a logo, an illustration, a special texture), write a programmed element that paints it, and include `stella_paint.h` (the toolkit PanelPainter is made of): `#include "stella_paint.h"`, then `stella::paint`. Read the settings in `resized()`; in `paint()` paint into a `Surface` and write it at the end:
```cpp
// elements/LogoBadge.cpp: a round gold badge with engraved rays on black marble, lit from the upper left.
#include "stella_paint.h"
using namespace stella::ui;
namespace p = stella::paint;

class LogoBadge final : public Element
{
public:
    void paint (Canvas& g) override
    {
        // The ground: black marble under the knobs' light.
        p::Panel panel;
        panel.setSize (width(), height());
        panel.surfacing = p::Surfacing::of (p::Material::blackMarble, 3);
        panel.light = p::Light::from (120.0f, 35.0f);
        panel.key = 0.4f;

        p::Surface s;
        s.resize (width(), height());
        p::paintBase (s, panel);

        // The badge: a raised plate of gold (tinted by paint_picture's "color"), rays engraved in it.
        const float r = std::fmin ((float) width(), (float) height()) * 0.42f;
        p::Part badge;
        badge.kind = p::PartKind::plate;
        badge.shape = p::Shape::circle ((float) width() * 0.5f, (float) height() * 0.5f, r);
        badge.ownSurface = true;
        badge.surfacing = p::Surfacing::of (p::Material::gold, 4);
        badge.surfacing.colour = p::rgbOf (colour (0xffd9b048));

        p::PatternLayer rays;
        rays.pattern = p::Pattern::sunburst;
        rays.blend = p::Blend::engrave;
        rays.opacity = 0.6f;
        rays.count = 36;
        badge.surfacing.patterns.push_back (rays);

        p::paintPart (s, badge, panel);
        s.write (g);
        g.drawText (label(), 0.0f, 0.0f, (float) width(), (float) height(), r * 0.5f, 0xff3a2a08, Align::centre, true);
    }
};

STELLA_ELEMENT (LogoBadge, "Painter for paint_picture: a round gold badge on black marble, with the label's letters")
```
stella_paint.h, in `stella::paint`:
- Colour: `Rgb` and `mix`, `scale`, `times`, `add`, `screen` (adds light), `luma`, `rainbow`; `rgbOf`, `colourOf`, `colourNamed` (`"#RRGGBB"`, `"#AARRGGBB"` or a name). Noise: `hash`, `noise`, `fbm`, `cells`.
- Surfaces: `Material` (`powder`, `aluminium`, `steel`, `silver`, `chrome`, `gold`, `roseGold`, `brass`, `copper`, `bronze`, `titanium`, `gunmetal`, `iron`, `hammertone`, `anodized`, `carbon`, `marble`, `blackMarble`, `granite`, `slate`, `concrete`, `terrazzo`, `sandstone`, `plastic`, `mattePlastic`, `rubber`, `bakelite`, `ceramic`, `wood`, `burl`, `leather`, `tolex`, `felt`, `paper`, `glass`, `frosted`, `pearl`, `smooth`), `materialNamed`, `Finish` (`natural`, `polished`, `satin`, `brushed`, `hammered`, `sandblasted`, `matte`, `liquid`); `Surfacing` (`material`, `colour`, `colour2`, `finish`, `scale`, `angle`, `grain`, `shine`, `flow`, `seed`, `patterns`) with `Surfacing::of (material, seed)`; `PatternLayer` (`pattern`, `colour` + `hasColour`, `opacity`, `blend`, `size`, `angle`, `width`, `depth`, `centreX`, `centreY`, `count`, `invert`) of a `Pattern` in a `Blend` (`normal`, `multiply`, `screen`, `overlay`, `softLight`, `add`, `darken`, `lighten`, `emboss`, `deboss`, `engrave`); `texelAt (surfacing, x, y, Frame)` for a texture's colour and height at a point; `surfacingFrom (Json, fallback)` reads the settings above.
- Light: `Light::from (angle, altitude)`; `Panel` (`surfacing`, `light`, `key`, `keyX`, `keyY`, `vignette`; call `setSize` first).
- Shapes: `Shape::box`, `Shape::circle`, `Shape::line` with `distance` (negative inside) and `outward`; `Surface` (`resize`, `at`, `within`, `write`); `shadeEdge`, `dropShadow`, `innerShadow`.
- Whole parts: `paintBase (surface, panel)`, `paintPart (surface, part, panel)` (a `Part`: `kind`, `shape`, `depth`, `strength`, `ownSurface` + `surfacing`, `colour` + `hasColour`, `screws`, `glow` + `glowAmount`), `paintLine`, `paintSlots`, `paintHoles`, `paintScrew`, `paintRim`; and `Json` to read a setting given as a list or object.
A `Surface` paints every pixel (an opaque picture); for a picture that's transparent around its shape (a badge over the background), draw it with the canvas's own shapes instead. A big picture takes a moment to paint: keep painters to one pass over the pixels.
