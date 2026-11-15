<!-- D:\workspace\Fanan_web_hub\src\skill_preset_managers.md -->
---
name: preset-managers
for: plugins
title: Preset managers
description: The preset manager's designs beyond the classic bar: a styled drop list (search, categories, pages, Save, Save as, Load, More), a table of preset buttons on the panel or in a popup, and a side panel joined to the window's left or right (the window wider by it, always open or opened by a handle); flat or 3D keys lit when chosen. Every design key, sizes, with recipes. Read it whenever the user asks for a preset browser, preset list, preset buttons, a sound bank panel or a side panel.
---
# Preset managers

A `preset` element wears a design when its `"settings"` have a `"presets"`. The user designs it in the edit window's Advanced edit tab, the Preset Studio (the E under the element, or a right-click on it, opens the edit window; its gallery: a page of the Gallery window); `design_templates` with `"kind": "presets"` lists the gallery's designs and gives any one's JSON. They show the plugin's presets (`save_preset`) and the user's own, their groups (Factory, My presets, each bank the user loads) as the categories.

```json
{ "type": "preset", "x": 500, "y": 14, "w": 230, "h": 30,
  "settings": { "presets": { "style": "list", "categories": "headings", "search": true, "actions": true,
                             "listRows": 10, "rowHeight": 26, "colour": "#FF2FB36B", "panelColour": "#FF17191C",
                             "keys": { "render": "2d", "style": "soft", "shape": "rect", "rounding": 20 } } } }
```

## Styles ("style")
- `bar`: the classic `< name >`: the arrows step, a click on the name opens the presets' menu. About 220 x 28.
- `list`: a drop list: the bar, and under it (over it where there's no room) a styled list: search, the categories, the presets a page at a time, the Save, Save as, Load and More keys. The element is the bar (about 230 x 30).
- `table`: a table of preset buttons, a page at a time: on the panel itself (`"open": "panel"`, the element is the table: about 380 x 220) or in a popup the bar opens (`"open": "popup"`, the element is the bar).
- `side`: a side panel, a module joined to the window's left or right (`"side"`), the window wider by its `"width"` (px, 220 standard); always there, or opened and closed by a handle on the window's edge (`"toggle": true`, `"startOpen"`). The element's own place doesn't matter (the studio puts it beside the window); put it anywhere, its `"w"` its width.

## The design ("presets"), every key optional
- `"columns"`, `"rows"`: a table's buttons across and down (a page). `"rowHeight"` (list and side), `"listRows"` (list: how many rows show at most).
- `"categories"`: `tabs` (a tab strip: All and each category), `headings` (each category's name over its presets: list and side), `none`.
- `"search"` (list and side), `"actions"` (always on, whatever it says: Save, Save as, Load and More; a table: a "..." beside its pages; More: Quick save, Save as, Load preset, Save bank, Load bank, Delete preset, Show folder. An exported plugin has all of them too: the user's presets go to Documents/<vendor>/<plugin> Presets, a loaded bank becomes a group of its own, and a plain preset bar opens the presets and these commands from its middle), `"title"` (side), `"plate"` (a table on the panel: a plate under it).
- `"colour"` (the accent), `"panelColour"`, `"textColour"`, `"lineColour"`, `"font"`, `"textSize"` %.
- `"barDisplay"`: `keys` (the standard: the name on the bar) or `led`: the preset's number and name in glowing LED segment digits between the arrows, as 80s synths showed them (the bar, a list's and a popup table's). `"ledNumber"` its number's digits (2; 0 none); `"ledColour"` (one colour: red `#FFFF3B2A` standard, green, amber, blue...), `"ledSegments"` (`auto`, `7`, `14`, `16`), `"ledGhost"` 0..60 %, `"ledGlow"` 0..100, `"ledSlant"` 0..15 degrees, `"ledThickness"` 6..30 %, `"ledWindow"` (`none`, `flat`, `3d`), `"ledBackground"`. The characters fill the bar between its arrows, as many as its number and longest name need.
- `"keys"`: its buttons and the bar, a selector set's design (the selector-sets skill): `"render"` `2d` or `3d`, `"shape"`, `"rounding"`, `"gap"`, `"capColour"`, `"capOnColour"`, `"glowColour"`, `"glow"`, `"labelColour"`, `"labelOnColour"`, `"textSize"`, `"material"`, `"height"`...: lit when chosen.

## Choosing
- A small effect: `bar` or `list`. A vintage synth or drum machine: `bar` (or `list`) with `"barDisplay": "led"`, a red 3D window. A synth with many sounds: `list` with headings and search, or a `side` panel with tabs (a browser that's always there).
- A performance instrument (a drum machine, a groovebox): a `table` on the panel, 3D keys lit in the accent: the presets are played like pads.
- One preset manager per window. Its keys in the panel's style: 3D keys on a 3D panel, flat on a flat one.

## In an exported plugin
It shows the plugin's factory presets (the ones `save_preset` saved): the list, the table and the side panel step, page and choose there; search, the category tabs and Save, Save as and Load are the studio's and Sonify's. A side panel is always open there (no handle).

## Check
- The style for the plugin's size of library; the element sized for its style?
- Presets saved, with clear names (a table or a side panel with none looks empty)?
- Its accent and keys matching the panel?
