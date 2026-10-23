<!-- D:\workspace\Fanan_web_hub\src\skill_panel_taste.md -->
---
name: panel-taste
for: plugins
title: Panel design
description: Design taste for plugin panels in gui/layout.json: choosing a character (hardware, modern flat, dark cinematic, retro synth, playful, boutique), hierarchy and hero controls, an 8-pixel rhythm, grouping without boxing everything, colour strategy with real palettes, type sizes, what to avoid, and exactly what to do when asked to critique, polish, make bolder, quieter, simpler, more delightful or more colourful. Read it before designing or redesigning any panel.
---
# Panel design

A panel is judged in a second: it should look like one instrument, made on purpose. These principles follow the "Impeccable" design approach (Paul Bakaus, Apache-2.0) and Anthropic's frontend-design guidance, rewritten for plugin panels and `gui/layout.json`.

## 1. Choose a character first
Decide before placing anything, from the plugin's sound and the user's words, and keep to it. Your inspiration pool (the inspiration-looks skill) holds whole recipes for many characters, and the look the user picked beside Send comes first; these are the broad families:
- **Classic hardware**: dark wood or brushed metal (a painted background: the background-painter skill), cream or black knobs, white engraved-style labels in capitals, red or amber accents. For analog synths, compressors, tape.
- **Modern flat**: one dark neutral with a hint of colour (not pure grey), one bright accent, thin outlines, generous space. For clean digital tools, EQs, utilities.
- **Dark cinematic**: near-black blue or violet, a soft glow accent (cyan, magenta), large central visual (a scope, an XY pad). For textures, ambient, reverbs, granular.
- **Retro synth**: off-white or beige panel, black and coloured knob caps by section, bold section stripes. For 80s polysynths, drum machines.
- **Playful**: warm saturated colours, big rounded controls, friendly words. For toys, lo-fi, kids, social plugins.
- **Boutique**: a single rich colour (oxblood, racing green, navy) with gold or cream details, few large knobs. For premium effects.

## 2. Hierarchy: one hero, then the rest
- The one or two controls people touch most (cutoff, drive, mix, the macro) are the biggest (knobs 72 to 96 px) and sit where the eye lands: centre or upper left.
- Secondary controls 48 to 56 px; utility and fine-tuning 36 to 44 px or small sliders.
- Not everything deserves equal size: a panel of identical knobs in a grid reads as a spreadsheet.

## 3. Rhythm and space
- Work on an 8-pixel grid: positions and sizes in multiples of 8 (4 for small adjustments).
- Margins 24 to 32 px from the window's edge; 16 to 24 px between controls in a group; 32 to 48 px between groups. Breathing room is what makes a panel look expensive.
- Align: a row of knobs shares its centre line; columns share their left edge or centre. Captions all the same distance below.
- Shadows need room: a look with a shadow (the looks list says how far it falls) gets that much clear space around it on top of the gaps above, and a painted well or section around it is larger by as much.
- Read in signal order, left to right (oscillator, filter, envelope, effects, output), or top to bottom.

## 4. Grouping without boxes
- Group by proximity and alignment first; a `group` frame only where a section must be visibly separate (two oscillators side by side). Never frames inside frames.
- A section title in small capitals (11 to 12 px, letter-spaced by its content: "FILTER") above a group often beats a box.
- A painted background with recessed areas (the background-painter skill) groups more elegantly than outlines.

## 5. Colour
- Neutrals tinted with the accent's hue (a dark navy-grey rather than pure grey; a warm cream rather than white). Background as a gentle top-to-bottom gradient (`"background": { "top", "bottom" }`), the bottom 8 to 15% darker.
- One accent colour for what's alive (value arcs, the active button, meters). A second colour only for a different meaning (warning, the second oscillator). Never a rainbow of knob colours.
- Text: about 90% white on dark (`#E6...`), secondary labels about 60%; on light panels near-black, never pure black on pure white.
- Contrast: labels must read at a glance; a 3:1 contrast or better against the panel.

Palettes (background top, bottom, accent, text):
- Midnight: `#FF1F2430`, `#FF14171F`, `#FF4FD1C5` (teal), `#FFE6E9EF`.
- Studio black: `#FF2A2A2E`, `#FF19191C`, `#FFFF6A3D` (orange), `#FFEDE7E1`.
- Analog cream: `#FFE9E2D0`, `#FFD8CFB8`, `#FFC0392B` (red), `#FF2B2621`.
- Ocean glass: `#FF0F2A3D`, `#FF081A27`, `#FF5BC0EB` (sky), `#FFE3F2FB`.
- Oxblood: `#FF4A1C1F`, `#FF2E1013`, `#FFE0B15A` (gold), `#FFF3E6D3`.
- Neon night: `#FF1A1029`, `#FF0E0818`, `#FFFF4FD8` (magenta), `#FFEDE3FF`.
- Forest: `#FF22302A`, `#FF151E1A`, `#FFB5E48C` (lime), `#FFE8F1EA`.

## 6. Type
- Plugin name: 20 to 28 px, bold, top left (or centred on symmetric hardware); a small tagline or vendor 11 to 12 px beside or under it.
- Section titles 11 to 13 px capitals; captions 12 to 13 px; values on hover. Never under 11 px.
- Short words: "Cutoff", "Res", "Drive", "Mix"; one style of case throughout (all Title Case captions, or all capitals).

## 7. Feedback and life
- A meter or scope makes a panel feel alive: an output meter on effects, a scope or envelope display on synths, a gain-reduction meter on compressors.
- An `envelope` or `filter` curve beside its knobs shows what they do; a `filter` given the live cutoff as its `"source"` opens and closes as the synth plays.
- A synth with two or more oscillators, a morph or a wavetable knob deserves a 3D hero screen (the 3d-displays skill): the playing wave lit and glowing as it plays.
- A keyboard along the bottom for instruments (70 to 100 px high).
- A `preset` browser near the name (about 200 x 28).

## 8. What to avoid
- Identical knobs in a perfect grid for everything; frames around every group; nested frames.
- More than two accent colours; gradients on text; glassy "frosted" panels by default; drop shadows on everything.
- Text under 11 px, low-contrast labels, captions in different sizes.
- Controls touching the window's edge or each other; misaligned rows; a big empty area on one side and crowding on the other.
- Decorative elements that look clickable but aren't.
- Changing the user's arrangement when they asked about one element: edit only what was asked.

## 9. When the user asks for a kind of change
- **Critique** (or "what do you think of the panel?"): look (read the project: the GUI elements), then say in two or three short sentences what works and the one or two things that would improve it most; offer to do it. Don't change anything until they agree.
- **Polish**: align everything to the 8-pixel grid, even out spacing, make captions consistent, fix contrast, give controls of a kind one look. No new elements.
- **Bolder**: bigger hero controls, a stronger accent (more saturated, used on more of the live parts), a bigger title, higher contrast, fewer but larger groups.
- **Quieter / more minimal**: smaller secondary controls, muted accent, fewer frames (remove groups, keep titles), more space, maybe hide fine-tuning controls behind a smaller row.
- **Simpler / distill**: find the three to five controls that matter, make them big, shrink or tuck away the rest (a smaller "advanced" row along the bottom).
- **Delight**: one memorable detail: a live scope or animated element, a painted background with character, a playful title, a meter with personality. One, not five.
- **More colourful**: keep one accent but give each section a tint of its own (the knob looks' colours, section titles in the section's colour), within one palette family.
- **Clarify**: clearer captions, sections in signal order, units shown in values, related controls closer together.
- **Make it look like <hardware or style>**: pick the matching character above, its palette, its knob looks (from the project's list), a painted background in its material.

## 10. Working method
1. Read the project (the GUI elements list, the looks available, any pictures).
2. Sketch the structure in your head: character, palette, hero controls, groups, where the keyboard and meters go, the window size (instruments often 760 to 960 wide, effects 480 to 720).
3. First design: `set_layout` once, complete. Later changes: `edit_layout` with only the elements that change.
4. Bind every control to a real parameter; give controls of a kind one look from the project's list.
5. For a painted background or plate, the background-painter skill.
