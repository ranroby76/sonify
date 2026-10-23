<!-- D:\workspace\Fanan_web_hub\src\skill_3d_displays.md -->
---
name: 3d-displays
for: plugins
title: 3D displays
description: Exciting 3D screens for plugin panels, built into the studio: an oscillator's waves as a lit 3D terrain (the wavetable look), two oscillators blending as one surface, the filter opening and closing as a rolling 3D sweep, and the sound itself as a live 3D waterfall (spectrum or wave). The part that plays is lit and glows while the instrument sounds, and the user turns them by dragging. Read it whenever a synth has two or more oscillators, a morph or wavetable knob, or when the user asks for 3D, a waterfall, a wavetable view, a live filter, or a more exciting screen.
---
# 3D displays

Four elements built into the studio, drawn in 3D: rows of curves stacked in depth, the nearer rows hiding the ones behind, seen in perspective from above. Each is a panel's hero screen: dark, glowing, alive while the instrument plays. The user drags to turn and tilt it (Shift: finely), the wheel zooms, a double-click puts it back. Nothing to program: place one with its name as the `"type"`, bind it, build.

## Which one
- **WaveTerrain**: one oscillator's waves along a knob (its morph, its wavetable position, its fold depth...): a row for each step of the knob, the row that plays lit white and glowing, gliding as the knob or its modulation moves it, with light rippling out from it while notes sound. The classic wavetable view.
- **MorphSurface**: two oscillators and the blend between them as one surface: oscillator A's wave along the front edge, B's along the back, the mix in between (B tuned against A twists the shapes as they blend), colours running from A's to B's, the mix row lit. The one for any synth with an A/B mix.
- **FilterSweep**: the filter's response now at the front and the moments before rolling back, so the filter visibly opens and closes as you play (an envelope sweep rises and falls like a wave rolling away); the cutoff is a bright point.
- **SoundWaterfall**: the sound itself, live: its spectrum (or its waveform, held steady on the note's cycle) rolling back from the front.

Two or more oscillators: a `MorphSurface` when there's a mix between A and B; otherwise a `WaveTerrain` for each oscillator side by side, each with its own `"label"` ("Osc 1", "Osc 2") and `"color"`. Add a `FilterSweep` beside them when the filter has an envelope or LFO. At most two 3D displays on a panel: each takes about 4 ms of every frame while it shows.

Sizes: 360 to 520 wide, 180 to 260 tall; a 2:1 box suits them. They draw their own dark background, so they sit best in a screen area of the panel (a recessed screen from the background-painter skill, or a dark group). Colours that glow on black: teal `#38E0D0`, magenta `#FF4FD8`, amber `#FFB347`, periwinkle `#7C9CFF`, lime `#B5E48C`; use the panel's accent for the main one (the panel-taste skill).

## Placing them
With `edit_layout`, like any element; every key but the type and the box is optional:
```json
{ "type": "WaveTerrain", "x": 24, "y": 64, "w": 420, "h": 210, "label": "Osc 1", "color": "#38E0D0",
  "params": { "timbre": "synth.timbre", "morph": "synth.morph" },
  "sources": { "position": "synth.morph", "level": "synth.env" },
  "settings": { "engine": "Analog", "sweep": "morph", "rows": 40 } }
{ "type": "MorphSurface", "x": 24, "y": 64, "w": 440, "h": 220, "label": "A / B", "color": "#38E0D0",
  "params": { "mix": "synth.mix", "timbreA": "synth.timbre", "morphA": "synth.morph",
              "timbreB": "synth.timbre2", "morphB": "synth.morph2", "tune": "synth.tune2" },
  "sources": { "mix": "synth.mix", "level": "synth.env" },
  "settings": { "engineA": "Analog", "engineB": "Fm", "colorB": "#FF4FD8" } }
{ "type": "FilterSweep", "x": 460, "y": 64, "w": 400, "h": 210, "label": "Filter", "color": "#FFB347",
  "params": { "cutoff": "synth.cutoff", "resonance": "synth.reso" },
  "sources": { "cutoff": "synth.cutoff", "level": "synth.env" },
  "settings": { "mode": "lowpass24" } }
{ "type": "SoundWaterfall", "x": 24, "y": 290, "w": 420, "h": 210, "label": "Output", "color": "#7C9CFF",
  "source": "plugin.out L", "settings": { "mode": "spectrum" } }
```
- `"params"` binds knobs by role. WaveTerrain: `"timbre"`, `"morph"`; `"sweep"` (`"morph"` or `"timbre"`) is the one along the depth, the other shapes every row, and its value shows under it. MorphSurface: `"mix"`, `"timbreA"`, `"morphA"`, `"timbreB"`, `"morphB"`, `"tune"` (B's tuning in semitones); a missing knob counts as half way. FilterSweep: `"cutoff"` (Hz, kHz or 0..1), `"resonance"`.
- `"settings"`: `"rows"` (WaveTerrain 40, MorphSurface 32, FilterSweep 36, SoundWaterfall 40; fewer is lighter), `"points"` (96), FilterSweep `"mode"` (`lowpass`, `lowpass24`, `highpass`, `highpass24`, `bandpass`), FilterSweep and SoundWaterfall `"speed"` (rows a second rolling back), SoundWaterfall `"mode"` (`"spectrum"` or `"wave"`), MorphSurface `"colorB"`.

## Making them live
They read live values by role from `"sources"`: a module's display (its value as the module shows it). Without one they follow the knobs, and glow while any key is held.
- `"position"` (WaveTerrain) / `"mix"` (MorphSurface): where it plays now, 0..1, modulation included (the knob plus its LFO or envelope).
- `"level"`: how loud it sounds, 0..1: the loudest voice's envelope (the synth-engines template shows it as `"synth.env"`).
- `"cutoff"` (FilterSweep): the live cutoff in Hz (the synth-engines template shows it as `"synth.cutoff"`). The toolbox's own `filter` curve takes the same display as its `"source"`, and then opens and closes as it plays too.
- SoundWaterfall watches a signal as its own `"source"`: `"plugin.out L"`, or a module's output.

A module shows values by naming them after `create` in its ModuleType and calling `display()` in `process()`, once per block:
```cpp
// The names, given to the registrar after create: { ..., displayNames, STELLA_COUNT (displayNames) }.
// In the layout they're "<moduleId>.env" and "<moduleId>.morph".
const char* const displayNames[] { "env", "morph" };

// In process(), once per block, after the samples:
display (0, loudestLevel);   // the loudest voice's envelope, 0..1
display (1, morphNow);       // the morph with its modulation (knob plus LFO), 0..1
```

## The waves are the sound's
`"engine"` (WaveTerrain) and `"engineA"`/`"engineB"` (MorphSurface) name the synth-engines struct the oscillator uses: `Analog`, `Supersaw`, `Morph`, `Fold`, `Fm`, `PhaseDistortion`, `Additive`, `Formant` (anything else: a sine). The display draws that engine's wave, shaped by the knobs bound as `"timbre"` and `"morph"` as they shape the sound, so match the engine to the code.

An oscillator of your own (a wavetable, a new shape) gets its own element drawn the same way: `#include "stella_view3d.h"` (the 3D view the built-ins use) and fill the rows from the oscillator's shape. Put the shape function in a header in `modules/` that doesn't include stella_api.h, call it from the oscillator, and include it in the element as `"../modules/<Name>.h"`: the picture is then exactly what plays.
```cpp
// elements/WaveScan.cpp: my oscillator's waves in 3D along its scan knob, the playing row lit.
#include "stella_view3d.h"
#include "../modules/ScanShape.h"   // float scanShape (float phase, float scan): -1..1

using namespace stella::ui;

class WaveScan final : public Element
{
public:
    void resized() override { cam.width = (float) width(); cam.height = (float) height(); }

    void paint (Canvas& g) override
    {
        const Colour accent = colour (0xff38e0d0);
        constexpr int rows = 40, points = 96;

        if (heights.empty())   // the rows, once: row r is the wave at scan r / (rows - 1)
            for (int r = 0; r < rows; ++r)
                for (int i = 0; i < points; ++i)
                    heights.push_back (scanShape ((float) i / (points - 1), (float) r / (rows - 1)));

        const float target = hasSource ("position") ? level ("position") : (hasParam ("scan") ? normalised ("scan") : 0.5f);
        position = view3d::glide (position, view3d::clamp (target, 0.0f, 1.0f));
        const float sounding = hasSource ("level") ? view3d::clamp (level ("level"), 0.0f, 1.0f) : 0.0f;
        glow = sounding > glow ? view3d::glide (glow, sounding, 0.6f) : glow * 0.92f;

        cam.fit (-0.6f, 0.15f);   // the box from the floor to the top, fitted to the element
        view3d::stage (g, cam, 0xff0d1117, 0xff050608, withAlpha (accent, 0.08f), -0.6f);

        view3d::terrain (g, cam, heights.data(), rows, points, 0.36f, -0.6f, [&] (int r)
        {
            const float d = (float) r - position * (rows - 1);
            const float lit = std::exp (-d * d / 0.8f);

            view3d::RowLook look;
            look.line = withAlpha (mix (mix (0xff1b3a40, accent, 0.3f), 0xfff2fffd, lit), 0.55f + 0.45f * lit);
            look.thickness = 1.0f + 1.4f * lit;
            look.glow = lit > 0.2f ? withAlpha (accent, (0.12f + 0.30f * glow) * lit) : 0u;
            look.glowWidth = 6.0f + 8.0f * glow;
            look.xray = lit > 0.5f ? 0.4f : 0.0f;   // the playing row shows through what's in front
            return look;
        });

        g.drawText (label(), 8.0f, (float) height() - 18.0f, (float) width() * 0.5f, 16.0f, 12.0f, 0xd8e6edf3, Align::left, true);
    }

    bool mouseDown (const Mouse& m) override         { orbit.down (cam, m); return true; }
    void mouseDrag (const Mouse& m) override         { orbit.drag (cam, m); }
    void mouseWheel (const Mouse&, float n) override { orbit.wheel (cam, n); }

private:
    view3d::Camera cam;
    view3d::Orbit orbit;
    std::vector<float> heights;
    float position = 0.5f, glow = 0.0f;
};

STELLA_ELEMENT (WaveScan, "My oscillator's waves in 3D along its scan knob, the playing row lit; drag to turn it")
```
stella_view3d.h, in `stella::ui::view3d`: `Camera` (`fit (low, high)` each paint; `project (x, y, z)` gives the screen point: x -1..1 across, y up, z -1 front to 1 back), `Orbit` (the mouse), `terrain (g, cam, heights, rows, points, height, floor, lookOf, crossEvery)` (rows of -1..1 heights, row 0 at the front; `crossEvery` 8 adds lines across the rows, a wireframe), `RowLook` (`line`, `thickness`, `glow`, `glowWidth`, `cross`, `xray`), `stage`, `ribbon` (a cheap thick line), `glide`, `clamp`, `colourFrom` (a colour from a setting's text). Rows that change while playing roll back: shift `heights` back by one row and write the new one at the front.

## Steps
1. Read the project first: the parameter ids, the engine each oscillator uses, the module's displays.
2. If the module doesn't show the live values yet, add its displays (`"env"`, and the morph, mix or cutoff with its modulation) and `build`.
3. Place the displays with `edit_layout` as above, bound to the real ids and displays.

Tell the user in plain words: "Your oscillator now has a 3D screen: the wave that plays lights up and glows as you play. Drag it to turn it."
