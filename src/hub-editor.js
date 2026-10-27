// D:\workspace\Fanan_web_hub\src\hub-editor.js
//
// The Editor (F2): Studio One's piano roll for one MIDI clip. Double-click a clip in
// the arrangement to open it.
//
//   Mouse       left click a note to select it (Shift or Ctrl adds or removes), drag on empty
//               space to select several in a box, drag notes to move them, drag a note's right
//               edge to change its length. Double-click empty space to draw a note. Right-click
//               anywhere for the menu (below).
//   Tools       Studio One's, picked with the toolbar, their keys (Studio One's map below), or
//               the right-click menu; the mouse pointer is the tool's own symbol:
//     Arrow (1)   as above.
//     Range (2)   drag across a stretch of time: every note starting in it is selected, at any
//                 pitch (Shift adds); drag a selected note to move them all.
//     Split (3)   click a note to cut it in two there (on the grid with Snap on); a selected one
//                 cuts every selected note under that time.
//     Eraser (4)  click or drag over notes to delete them.
//     Paint (5)   the Pencil: click to draw a note, drag to set its length. In the lane under
//                 the notes it draws as its shape says (the right-click menu's Paint Tool):
//                 Freehand, or a Line, Parabola, Square, Triangle, Saw or Sine from where the
//                 drag began to where the pointer is (a wave four Quantize steps a cycle).
//     Mute (6)    click a note to mute it (it stays, silent, drawn hollow) or unmute it; drag
//                 over notes to do the same to each. A selected one does it to the selection.
//     Listen (8)  hold the mouse down to hear the clip's track alone from there; let go and the
//                 song goes back where it was.
//   Right-click Studio One's menu: the tools, Cut, Copy, Paste (where you clicked), Duplicate,
//               Delete, Select All, Deselect All, Undo and Redo. On a note, the note joins the
//               selection first.
//   Quantize    the grid's lines, a new note's length and the steps notes move and grow by
//               (with Snap on); a note's end dragged moves a quarter of that at a time.
//   Zoom        the sliders right of the lanes' names: length (time) and height (the rows), with
//               - and + beside each; Ctrl + mouse wheel zooms the length too.
//   Keys        as the keyboard map in use has them (hub-keys.js); in Studio One's: Delete
//               removes the selected notes, Ctrl+A selects all, Ctrl+C / X / V / D copy, cut,
//               paste and duplicate them, Q quantizes. Always: Up/Down transpose (Shift: an
//               octave), Left/Right move by the grid.
//   Look        the piano roll follows Settings > Appearance > Luminance (Hub.theme).
//   Lanes       under the notes, chosen above them: Velocity (drag over the notes' bars), Pitch
//               Bend and Aftertouch (drag to draw the line; the Eraser clears it back to rest;
//               the middle of Pitch Bend catches), Chance (drag over the bars: how likely each
//               note plays).
//   Operators   (the toolbar's button) Bitwig's, for the selected notes: Chance and Vel Spread
//               (rolled again each pass), Repeats (ratchets: a count or a note value, their Curve
//               and the last one's Vel End), Occurrence (first pass, not first, with or without
//               the note before, or the one before on its key) and Recurrence (a cycle of 2 to 8
//               passes, and the ones it plays on). Seed: what the clip's rolls come from; the die
//               rolls a new one. A note shows them: a die's dots (Chance), a line at each repeat,
//               a corner (Occurrence) and the passes (Recurrence); Vel Spread shows in the
//               Velocity lane. A pass: each time the loop comes round over the clip, and copy by
//               copy along copies of it back to back.
//   Keyboard    click the keys on the left to hear a note.
//   Drums       the toolbar's Piano | Drums switch: the same clip and tools, a row for each drum
//               sound instead of a key (General MIDI's names; the usual kit always there, every
//               sound the clip plays, and the ones added with +). Hits are diamonds: a click on an
//               empty step puts one there (the Quantize grid's steps), a click on a hit takes it
//               away, Shift or Ctrl + click selects, a drag moves. The Paint tool dragged along a
//               row draws a roll. Double-click a row's name to rename it, right-click it to add or
//               take away rows; the names and rows added are the track's own (its drum map).
//
//   Process     (the toolbar's Process \u25be) on the selected notes, or all of them, one Undo step
//               each: Chop (in 2, 3 or 4, or on the Quantize grid), Glue (touching notes of a key
//               made one), Legato, Reverse, Invert (high notes low), Double or Half Speed, Fit to
//               Key (the song's key), Velocity (Crescendo, Decrescendo, Set to the toolbar's
//               Velocity, Compress) and Remove Doubles. The MIDI Tools (Tools\u2026) stay as they are.
//   Scale       (Piano view) the song's key where the clip starts, lit across the rows (its root
//               the brightest) and marked on the keys. Fold: only the rows of the key (and any note
//               played off it), or only the rows the clip plays; then a drag up or down moves
//               notes row by row, as the Drums view does.
//
// The editor works on a copy of the clip's notes and lines at song positions and sends the
// whole clip to the engine when the mouse is released.

(() => {
  "use strict";

  const { $ } = Hub;
  const MIN_ROW = 5, MAX_ROW = 36, DEFAULT_ROW = 12;        // pixels per semitone (the height zoom)
  const MIN_ZOOM = 8, MAX_ZOOM = 1200, DEFAULT_ZOOM = 96;   // pixels per quarter note (the length zoom)
  const QUANTIZE = [["1/1", 4], ["1/2", 2], ["1/4", 1], ["1/8", 0.5], ["1/16", 0.25], ["1/32", 0.125], ["1/64", 0.0625]];
  const BLACK = new Set([1, 3, 6, 8, 10]);
  const NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const LINES = ["pitchBend", "aftertouch"];                // the lanes that are lines of points
  const BEND_MIN = -8192, BEND_MAX = 8191, PRESSURE_MAX = 127;
  const POINT_STEP = 1 / 64;                                // quarter notes between a drawn line's points
  // The tools, as Studio One's menu lists them: [tool, its name, its key's command].
  const TOOLS = [["arrow", "Arrow Tool", "toolArrow"], ["range", "Range Tool", "toolRange"], ["split", "Split Tool", "toolSplit"],
                 ["eraser", "Eraser Tool", "toolEraser"], ["pencil", "Paint Tool", "toolPaint"], ["mute", "Mute Tool", "toolMute"],
                 ["listen", "Listen Tool", "toolListen"]];
  const ICONS = {
    arrow: '<svg viewBox="0 0 16 16"><path d="M4 1.5v12l3.2-3 2.1 4.6 1.8-.8-2.1-4.5H13z"/></svg>',
    range: '<svg viewBox="0 0 16 16"><path d="M2 2.5h3v1.4H3.4v8.2H5v1.4H2zM14 2.5h-3v1.4h1.6v8.2H11v1.4h3z"/><path d="M6 5h4v6H6z" opacity=".55"/></svg>',
    split: '<svg viewBox="0 0 16 16"><path d="M2 14l7.5-7.5 1.4 1.4L4 14.6z"/><path d="M9.5 6.5 13 2.2c1 .6 1.5 1.6 1.2 2.6l-3.3 3.1z" opacity=".75"/></svg>',
    eraser: '<svg viewBox="0 0 16 16"><path d="M9.5 2 15 7.5 8.5 14H4L1 11z"/><path d="M5 14h10v1.2H5z"/></svg>',
    pencil: '<svg viewBox="0 0 16 16"><path d="M11.3 1.7l3 3-8.6 8.6-3.7.7.7-3.7z"/></svg>',
    mute: '<svg viewBox="0 0 16 16"><path d="M1.5 6h2.8l3.7-3v10L4.3 10H1.5z"/><path d="M10 5.5l4.5 5M14.5 5.5l-4.5 5" stroke="currentColor" stroke-width="1.5" fill="none"/></svg>',
    listen: '<svg viewBox="0 0 16 16"><path d="M1.5 6h2.8l3.7-3v10L4.3 10H1.5z"/><path d="M10.2 5.3a3.6 3.6 0 0 1 0 5.4M12 3.6a6 6 0 0 1 0 8.8" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>'
  };
  // The Paint tool's shapes in the lane under the notes (Studio One's Paint Tool menu).
  const SHAPES = [["freehand", "Freehand", '<svg viewBox="0 0 16 16"><path d="M11.3 1.7l3 3-8.6 8.6-3.7.7.7-3.7z"/></svg>'],
                  ["line", "Line", '<svg viewBox="0 0 16 16"><path d="M3 13 13 3" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>'],
                  ["parabola", "Parabola", '<svg viewBox="0 0 16 16"><path d="M3 13c5 0 8-3 10-10" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>'],
                  ["square", "Square", '<svg viewBox="0 0 16 16"><path d="M2 11h3V5h5v6h4" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>'],
                  ["triangle", "Triangle", '<svg viewBox="0 0 16 16"><path d="M1.5 11 5 5l3.5 6L12 5l2.5 4" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>'],
                  ["saw", "Saw", '<svg viewBox="0 0 16 16"><path d="M1.5 12 5 4v8l3.5-8v8L12 4v8" stroke="currentColor" stroke-width="1.3" fill="none"/></svg>'],
                  ["sine", "Sine", '<svg viewBox="0 0 16 16"><path d="M1.5 8c1.6-5 3.6-5 5.2 0s3.6 5 5.2 0 2.1-3 2.6-2.4" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>']];
  const LANE_TIPS = { velocity: "Velocity: drag over the notes' bars to set it",
                      pitchBend: "Pitch Bend: drag to draw it, the Eraser takes it back to rest",
                      aftertouch: "Aftertouch: drag to draw it, the Eraser takes it back to rest",
                      chance: "Chance: drag over the notes' bars to set how likely each plays" };
  const BARS = new Set(["velocity", "chance"]);             // the lanes that are a bar for each note
  // Operators: Occurrence's choices, Repeats' (a count, or a note value in quarter notes) and
  // Recurrence's cycles.
  const OCCURRENCES = [["", "Always"], ["first", "First pass"], ["notFirst", "Not first pass"], ["prev", "With previous note"],
                       ["noPrev", "Without previous note"], ["prevKey", "With previous on its key"], ["noPrevKey", "Without previous on its key"]];
  const REPEATS = [["off", "Off"], ["n:2", "\u00d72"], ["n:3", "\u00d73"], ["n:4", "\u00d74"], ["n:5", "\u00d75"], ["n:6", "\u00d76"],
                   ["n:8", "\u00d78"], ["n:12", "\u00d712"], ["n:16", "\u00d716"], ["n:24", "\u00d724"], ["n:32", "\u00d732"],
                   ["r:1", "1/4"], ["r:0.5", "1/8"], ["r:1/3", "1/8T"], ["r:0.25", "1/16"], ["r:1/6", "1/16T"],
                   ["r:0.125", "1/32"], ["r:0.0625", "1/64"]];
  const RATES = { "r:1": 1, "r:0.5": 0.5, "r:1/3": 1 / 3, "r:0.25": 0.25, "r:1/6": 1 / 6, "r:0.125": 0.125, "r:0.0625": 0.0625 };
  const OPS_DEFAULT = { c: 100, vs: 0, rn: 0, rr: 0, rc: 0, rv: 0, oc: "", rl: 0, rm: 255 };

  const ed = {
    trackId: 0, clipId: 0, clipStart: 0, clipLength: 0, clipOffset: 0,
    notes: [],                     // the notes the clip shows: {pitch, start (song position), length, velocity 1-127, muted, ops, selected}
    hidden: [],                    // notes a trim has hidden: kept as they are, [pitch, content start, length, velocity, muted?, ops?]
    lines: { pitchBend: [], aftertouch: [] },         // the points the clip shows: {time (song position), value}
    hiddenLines: { pitchBend: [], aftertouch: [] },   // points a trim has hidden: [content time, value]
    edited: { pitchBend: false, aftertouch: false },  // drawn here since the engine last heard (only those lines are sent)
    zoom: DEFAULT_ZOOM, row: DEFAULT_ROW, tool: "arrow", snap: true, quantize: 0.25, velocity: 100, audition: true,
    scale: false, fold: "off",     // the song's key lit across the rows; Fold: "off", "key" or "used"
    shape: "freehand",             // the Paint tool's shape in the lane
    range: null,                   // the Range tool's stretch of time: {from, to} (song positions)
    splitHover: null,              // where the Split tool would cut: {note, time}
    lane: "velocity", drag: null, pendingReload: false,
    downOnNotes: false,            // the last press was on the notes (a double-click there draws one)
    hover: null,                   // the lane's value under the mouse
    hoverTime: 0, hoverInside: false,  // where the mouse is over the notes (Paste goes there)
    seed: 0, seedEdited: false,    // the clip's seed (operators roll from it); changed here, not yet sent
    showOps: false,                // the Operators row shows
    view: "piano"                  // "piano" or "drums" (the toolbar's switch; each track keeps its own while Sonify runs)
  };

  // ---------------------------------------------------------------- the Drums view's rows
  const GM_DRUMS = { 35: "Acoustic Kick", 36: "Kick", 37: "Side Stick", 38: "Snare", 39: "Clap", 40: "Electric Snare",
    41: "Low Floor Tom", 42: "Closed Hat", 43: "High Floor Tom", 44: "Pedal Hat", 45: "Low Tom", 46: "Open Hat",
    47: "Low-Mid Tom", 48: "Hi-Mid Tom", 49: "Crash", 50: "High Tom", 51: "Ride", 52: "China", 53: "Ride Bell",
    54: "Tambourine", 55: "Splash", 56: "Cowbell", 57: "Crash 2", 58: "Vibraslap", 59: "Ride 2", 60: "Hi Bongo",
    61: "Low Bongo", 62: "Mute Hi Conga", 63: "Open Hi Conga", 64: "Low Conga", 65: "High Timbale", 66: "Low Timbale",
    67: "High Agogo", 68: "Low Agogo", 69: "Cabasa", 70: "Maracas", 71: "Short Whistle", 72: "Long Whistle",
    73: "Short Guiro", 74: "Long Guiro", 75: "Claves", 76: "Hi Wood Block", 77: "Low Wood Block", 78: "Mute Cuica",
    79: "Open Cuica", 80: "Mute Triangle", 81: "Open Triangle" };
  const CORE_KIT = [36, 38, 39, 42, 46, 41, 45, 48, 49, 51];   // always shown: kick, snare, clap, hats, three toms, crash, ride
  const views = new Map();         // trackId -> its view, while Sonify runs
  let drumRows = [];               // the Drums view's pitches, top to bottom (kept as they are while dragging)

  const grid = () => $("editor-grid");
  const drums = () => ed.view === "drums";
  const isOpen = () => ed.clipId !== 0 && !document.body.classList.contains("hide-editor");
  const track = () => Hub.trackById(ed.trackId);
  const clipOf = () => { const t = track(); return t ? (t.clips || []).find((c) => c.id === ed.clipId) || null : null; };
  const noteName = (n) => NAMES[n % 12] + (Math.floor(n / 12) - 1);
  const keyOf = (n) => `${n.pitch}|${n.start.toFixed(5)}|${n.length.toFixed(5)}`;
  const selected = () => ed.notes.filter((n) => n.selected);
  const clipEnd = () => ed.clipStart + ed.clipLength;

  // The song's bars, as the song channel's time signatures count them (hub-tempo.js).
  const map = () => Hub.tempoMap;

  // The song's key (Scale and Fold): its notes as bits from its root, as the MIDI tools have them.
  const SCALE_NOTES = { "major": 0x0ab5, "minor": 0x05ad, "dorian": 0x06ad, "phrygian": 0x05ab, "lydian": 0x0ad5,
                        "mixolydian": 0x06b5, "locrian": 0x056b, "harmonic minor": 0x09ad, "melodic minor": 0x0aad,
                        "major pentatonic": 0x0295, "minor pentatonic": 0x04a9, "blues": 0x04e9, "chromatic": 0x0fff };
  const keyAtTime = (t) => {
    const k = map().keyAt(Math.max(0, t));
    return { root: k ? k.root : 0, scale: k ? k.scale : "major", notes: SCALE_NOTES[k && k.scale] || SCALE_NOTES.major };
  };
  const clipKey = () => keyAtTime(ed.clipStart);
  const inKey = (pitch, key) => ((key.notes >> ((((pitch - key.root) % 12) + 12) % 12)) & 1) === 1;
  let pianoRows = [];              // the Piano view's pitches when folded, top (highest) to bottom
  const folded = () => !drums() && ed.fold !== "off";

  /** The folded rows: the key's notes (Fold to key) and every note the clip plays (always shown). */
  function updatePianoRows() {
    const pitches = new Set();
    for (const n of ed.notes) pitches.add(n.pitch);
    for (const note of ed.hidden) pitches.add(note[0]);
    if (ed.fold === "key") { const key = clipKey(); for (let p = 0; p < 128; p++) if (inKey(p, key)) pitches.add(p); }
    if (!pitches.size) pitches.add(60);
    pianoRows = [...pitches].sort((a, b) => b - a);
  }

  // Rows: a key each (127 at the top) in the Piano view (folded: the rows kept), a drum sound each in the Drums view.
  const rowList = () => (drums() ? drumRows : pianoRows);
  const rowCount = () => (drums() || folded() ? rowList().length : 128);
  const rowOf = (pitch) => (drums() || folded() ? rowList().indexOf(pitch) : 127 - pitch);
  const pitchOfRow = (row) => (drums() || folded() ? rowList()[Hub.clamp(row, 0, rowList().length - 1)] : Hub.clamp(127 - row, 0, 127));

  /** The track's drum map: {names: {pitch: name}, rows: [pitches added]} (null: none yet). */
  const drumMapOf = () => { const t = track(); return t && t.drumMap && typeof t.drumMap === "object" ? t.drumMap : null; };

  function drumName(pitch) {
    const own = drumMapOf();
    const name = own && own.names ? own.names[String(pitch)] : "";
    return name || GM_DRUMS[pitch] || noteName(pitch);
  }

  /** The Drums view's rows: the usual kit, every sound the clip plays (shown or trimmed away), the rows added. */
  function updateDrumRows() {
    const pitches = new Set(CORE_KIT);
    for (const n of ed.notes) pitches.add(n.pitch);
    for (const note of ed.hidden) pitches.add(note[0]);
    const own = drumMapOf();
    if (own && Array.isArray(own.rows)) for (const p of own.rows) if (Number.isInteger(p) && p >= 0 && p <= 127) pitches.add(p);
    drumRows = [...pitches].sort((a, b) => a - b);
  }

  /** The track's drum map changed here: kept on the track (and with the song). */
  function setDrumMap(change) {
    const t = track();
    if (!t) return;
    const own = drumMapOf() || {};
    const next = { names: Object.assign({}, own.names || {}), rows: Array.isArray(own.rows) ? own.rows.slice() : [] };
    change(next);
    t.drumMap = next;
    Hub.send("trackSettings", { id: t.id, drumMap: next });
    updateDrumRows();
    updateSize();
    render();
  }

  /** The bar a position is in, and the first bar line at or after a position. */
  const barAt = (t) => map().barAt(t);
  const barLineAtOrAfter = (t) => { const b = barAt(t - 1e-9); return t - b.start < 1e-9 ? b.start : b.end; };

  // ---------------------------------------------------------------- operators
  /** A note's operators, every one of them (the defaults where it has none). */
  const opsOf = (n) => Object.assign({}, OPS_DEFAULT, (n && n.ops) || {});
  const repeating = (o) => o.rn >= 2 || o.rr > 0;
  /** Repeats as the list has them: "off", "n:<count>" or "r:<note value>". */
  function repeatCode(o) {
    if (o.rn >= 2) return `n:${o.rn}`;
    if (!(o.rr > 0)) return "off";
    const known = Object.keys(RATES).find((code) => Math.abs(RATES[code] - o.rr) < 1e-6);
    return known || `r:${Math.round(o.rr * 1e6) / 1e6}`;
  }
  const chanceOf = (n) => opsOf(n).c;

  /** Only what isn't the default, in range (null when nothing is): as Sonify keeps it. */
  function tidyOps(o) {
    const t = {};
    const whole = (v, lo, hi, fallback) => (Number.isFinite(Number(v)) ? Hub.clamp(Math.round(Number(v)), lo, hi) : fallback);
    const c = whole(o.c, 0, 100, 100), vs = whole(o.vs, 0, 100, 0);
    if (c !== 100) t.c = c;
    if (vs) t.vs = vs;
    const rn = whole(o.rn, 0, 64, 0), rr = Number(o.rr) || 0;
    if (rn >= 2) t.rn = rn;
    else if (rr >= 1 / 128 && rr <= 16) t.rr = rr;
    if (t.rn || t.rr) {
      const rc = whole(o.rc, -100, 100, 0), rv = whole(o.rv, -100, 100, 0);
      if (rc) t.rc = rc;
      if (rv) t.rv = rv;
    }
    if (OCCURRENCES.some(([code]) => code && code === o.oc)) t.oc = o.oc;
    const rl = whole(o.rl, 0, 8, 0);
    if (rl >= 2) { t.rl = rl; t.rm = whole(o.rm, 0, 255, 255) & ((1 << rl) - 1); }
    return Object.keys(t).length ? t : null;
  }

  /** Where a note's repeats start (song positions), as Sonify plays them: evenly or every note
      value through its length (cut at the clip's end), crowded by the curve; the ones crowded
      closer than the shortest note merge into the one before. */
  function repeatStarts(n) {
    const o = opsOf(n);
    if (!repeating(o)) return [n.start];
    const span = Math.min(n.start + n.length, clipEnd()) - n.start;
    if (span <= 2 / 128) return [n.start];
    const count = Hub.clamp(o.rn >= 2 ? o.rn : Math.ceil(span / o.rr - 1e-3), 1, 64);
    const exponent = Math.pow(4, -o.rc / 100);
    const place = (k) => Math.pow(o.rn >= 2 ? k / count : Math.min(1, (k * o.rr) / span), exponent);
    const starts = [n.start];
    let last = 0;
    for (let k = 1; k < count; k++) {
      if ((place(k) - last) * span < 1 / 128 || (1 - place(k)) * span < 1 / 128) continue;
      last = place(k);
      starts.push(n.start + last * span);
    }
    return starts;
  }

  // ---------------------------------------------------------------- snapping
  // With Snap on, notes start, move and grow by the quantize value (counted from each bar's
  // start); a new note is that long.
  const step = () => (ed.snap ? ed.quantize : 0);
  const quantizeStep = (t) => map().stepOf(ed.quantize, t);
  const snapFloor = (t) => (step() ? map().snapFloor(t, quantizeStep(t)) : t);
  const snapRound = (t) => (step() ? map().snap(t, quantizeStep(t)) : t);
  const minLength = () => step() || 1 / 64;
  const newLength = () => ed.quantize;

  // ---------------------------------------------------------------- geometry
  /** A pointer position as a place in the grid's content: pixels, song time and pitch. */
  function point(event) {
    const box = grid();
    const r = box.getBoundingClientRect();
    const x = event.clientX - r.left + box.scrollLeft;
    const y = event.clientY - r.top + box.scrollTop;
    return { x, y, time: Math.max(0, x / ed.zoom), pitch: pitchOfRow(Math.floor(y / ed.row)) };
  }

  /** Over the notes themselves, not the grid's scrollbars. */
  function overNotes(event) {
    const box = grid(), r = box.getBoundingClientRect();
    return event.clientX >= r.left && event.clientX < r.left + box.clientWidth && event.clientY >= r.top && event.clientY < r.top + box.clientHeight;
  }

  function noteAt(p) {
    // A drum hit is its diamond, around its start.
    const half = drums() ? Math.max(5, ed.row / 2) / ed.zoom : 0;
    for (let i = ed.notes.length - 1; i >= 0; i--) {
      const n = ed.notes[i];
      if (n.pitch !== p.pitch) continue;
      if (drums() ? Math.abs(p.time - n.start) <= half : p.time >= n.start && p.time < n.start + n.length) return n;
    }
    return null;
  }

  /** On the right edge, where dragging changes the length (not a drum hit: it has none to pull). */
  function onEnd(n, p) {
    if (drums()) return false;
    const endX = (n.start + n.length) * ed.zoom;
    return endX - p.x <= Math.min(6, (n.length * ed.zoom) / 3);
  }

  // ---------------------------------------------------------------- engine
  /** Takes the clip's notes and lines from the engine, keeping the selection on the same notes. */
  function load() {
    const clip = clipOf();

    if (!clip) {
      ed.clipId = 0;
      ed.notes = [];
      render();
      return;
    }

    const keep = new Set(selected().map(keyOf));
    ed.clipStart = clip.start;
    ed.clipLength = clip.length;
    ed.clipOffset = clip.offset || 0;
    ed.notes = [];
    ed.hidden = [];
    const shows = (time) => time >= ed.clipOffset && time < ed.clipOffset + clip.length;

    // Only what the clip shows is edited here; what a trim hid stays as it is.
    for (const note of clip.notes || []) {
      const [pitch, start, length, velocity, muted, ops] = note;
      if (shows(start)) ed.notes.push({ pitch, start: clip.start + start - ed.clipOffset, length, velocity, muted: Boolean(muted),
                                        ops: ops && typeof ops === "object" ? tidyOps(opsOf({ ops })) : null, selected: false });
      else ed.hidden.push(note.slice());
    }

    // The clip's seed (unless one chosen here is still on its way).
    if (!ed.seedEdited) ed.seed = Number(clip.seed) || 0;

    for (const name of LINES) {
      ed.lines[name] = [];
      ed.hiddenLines[name] = [];
      for (const [time, value] of clip[name] || []) {
        if (shows(time)) ed.lines[name].push({ time: clip.start + time - ed.clipOffset, value });
        else ed.hiddenLines[name].push([time, value]);
      }
    }

    for (const n of ed.notes) n.selected = keep.has(keyOf(n));

    $("editor-title").textContent = `${track().name} · bar ${barAt(clip.start + 1e-9).index + 1}`;
    updateSize();
    render();
  }

  /** Sends the clip as edited; label names the undo step (a MIDI tool's name), else Edit Notes. */
  function commit(label) {
    ed.notes.sort((a, b) => a.start - b.start || a.pitch - b.pitch);
    const message = { trackId: ed.trackId, clipId: ed.clipId };
    if (typeof label === "string" && label) message.label = label;

    // Notes taken past the clip's edges grow it to whole bars, like Studio One's parts.
    let first = ed.clipStart, last = clipEnd();
    for (const n of ed.notes) { first = Math.min(first, n.start); last = Math.max(last, n.start + n.length); }

    if (first < ed.clipStart - 1e-9 || last > clipEnd() + 1e-9) {
      const start = barAt(Math.max(0, first) + 1e-9).start;
      const end = barLineAtOrAfter(last);
      ed.clipOffset += start - ed.clipStart;
      ed.clipStart = start;
      ed.clipLength = end - start;
      Object.assign(message, { start, length: ed.clipLength, offset: ed.clipOffset });
    }

    const content = (time) => time - ed.clipStart + ed.clipOffset;
    message.notes = ed.notes.map((n) => {
      const values = [n.pitch, content(n.start), n.length, Math.round(n.velocity)];
      if (n.muted || n.ops) values.push(n.muted ? 1 : 0);
      if (n.ops) values.push(n.ops);
      return values;
    }).concat(ed.hidden);

    if (ed.seedEdited) {
      message.seed = ed.seed;
      ed.seedEdited = false;
    }

    // A line goes along only when it was drawn here, so points the engine has that the Editor
    // hasn't heard of yet (a take recording into this clip) aren't overwritten by a note edit.
    for (const name of LINES) {
      if (!ed.edited[name]) continue;
      message[name] = ed.lines[name].map((p) => [content(p.time), Math.round(p.value)]).concat(ed.hiddenLines[name]).sort((a, b) => a[0] - b[0]);
      ed.edited[name] = false;
    }
    Hub.send("setClipNotes", message);
  }

  // ---------------------------------------------------------------- audition
  let sounding = null, auditionTimer = 0;

  function stopAudition() {
    clearTimeout(auditionTimer);
    if (sounding === null) return;
    Hub.send("noteOff", { note: sounding });
    sounding = null;
  }

  function audition(pitch, velocity) {
    if (!ed.audition) return;
    stopAudition();
    sounding = pitch;
    Hub.send("noteOn", { note: pitch, velocity: velocity / 127 });
    auditionTimer = setTimeout(stopAudition, 220);
  }

  // ---------------------------------------------------------------- drawing
  function sizeCanvas(canvas, width, height) {
    const scale = window.devicePixelRatio || 1;
    const w = Math.max(1, Math.round(width * scale)), h = Math.max(1, Math.round(height * scale));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
      canvas.style.width = width + "px";
      canvas.style.height = height + "px";
    }
    const context = canvas.getContext("2d");
    if (context) context.setTransform(scale, 0, 0, scale, 0, 0);
    return context;
  }

  /** From colour a toward colour b by t (0..1). */
  function blend(a, b, t) {
    const rgb = (c) => [1, 3, 5].map((i) => parseInt(c.slice(i, i + 2), 16));
    const x = rgb(a), y = rgb(b);
    return "#" + x.map((v, i) => Math.round(v + (y[i] - v) * t).toString(16).padStart(2, "0")).join("");
  }

  /** Grid steps from a 64th note up to many bars (for the bar where the view starts); the
      finest (no finer than finest) at least minPx wide. */
  const gridStep = (minPx, finest = 0) => map().gridStep(grid().scrollLeft / ed.zoom, ed.zoom, minPx, finest);

  /** "5", "5.2" or "5.2.3" for a position, as fine as the label step needs. */
  const positionLabel = (t, labelStep) => map().label(t, labelStep.q);

  /** The lines down the grid, as Studio One draws them: every quantize step (when they're far
      enough apart to see), every beat, every bar, each counted from its bar's start. One path each. */
  function timeLines(context, sx, width, height, colours, minPx = 5) {
    const from = sx / ed.zoom, to = (sx + width) / ed.zoom + 1e-6;
    const ref = barAt(from);

    const draw = (lines, colour) => {
      context.strokeStyle = colour;
      context.beginPath();
      for (const line of lines) {
        const x = Math.round(line.ppq * ed.zoom - sx) + 0.5;
        context.moveTo(x, 0);
        context.lineTo(x, height);
      }
      context.stroke();
    };

    let bars = 1;
    while (ref.length * bars * ed.zoom < minPx && bars < 1024) bars *= 2;
    if (ed.quantize * ed.zoom >= minPx) draw(map().lines(from, to, map().stepOf(ed.quantize, from)).filter((l) => !l.beat), colours.fine);
    if (ref.beat * ed.zoom >= minPx) draw(map().lines(from, to, { q: ref.beat, bars: 0, beat: true }).filter((l) => !l.bar), colours.beat);
    draw(map().lines(from, to, { q: ref.length * bars, bars }), colours.bar);
  }

  function drawGrid() {
    const box = grid();
    const width = box.clientWidth, height = box.clientHeight;
    const context = sizeCanvas($("editor-canvas"), width, height);
    if (!context) return;

    const sx = box.scrollLeft, sy = box.scrollTop;
    const colours = Hub.theme.roll;
    const firstRow = Math.max(0, Math.floor(sy / ed.row)), lastRow = Math.min(rowCount() - 1, Math.ceil((sy + height) / ed.row));

    // Pitch rows: black keys darker, a line between every two (drum rows: every other one darker).
    if (drums() || folded()) {
      context.fillStyle = colours.black;
      context.fillRect(0, 0, width, height);
    }
    const key = !drums() && ed.scale ? clipKey() : null;
    for (let row = firstRow; row <= lastRow; row++) {
      const pitch = pitchOfRow(row);
      context.fillStyle = (drums() ? row % 2 === 1 : BLACK.has(pitch % 12)) ? colours.black : colours.white;
      context.fillRect(0, row * ed.row - sy, width, ed.row);
      // Scale: the key's rows lit, its root the brightest.
      if (key && inKey(pitch, key)) {
        context.fillStyle = (pitch - key.root) % 12 === 0 ? Hub.accentRgba(0.24) : Hub.accentRgba(0.11);
        context.fillRect(0, row * ed.row - sy, width, ed.row);
      }
    }
    context.fillStyle = colours.row;
    for (let row = firstRow; row <= lastRow; row++) context.fillRect(0, row * ed.row - sy + ed.row - 1, width, 1);

    // Down the grid: quantize steps, beats, bars.
    context.lineWidth = 1;
    timeLines(context, sx, width, height, colours);

    // A stronger line under every C, across them.
    context.fillStyle = colours.octave;
    for (let row = firstRow; row <= lastRow; row++)
      if (!drums() && pitchOfRow(row) % 12 === 0) context.fillRect(0, row * ed.row - sy + ed.row - 1, width, 1);

    // Outside the clip is dimmed.
    const c0 = ed.clipStart * ed.zoom - sx, c1 = clipEnd() * ed.zoom - sx;
    context.fillStyle = colours.outside;
    if (c0 > 0) context.fillRect(0, 0, Math.min(c0, width), height);
    if (c1 < width) context.fillRect(Math.max(c1, 0), 0, width - Math.max(c1, 0), height);

    // The Range tool's stretch of time, right down the grid.
    if (ed.range) {
      const r0 = Math.round(ed.range.from * ed.zoom - sx), r1 = Math.round(ed.range.to * ed.zoom - sx);
      context.fillStyle = Hub.accentRgba(0.14);
      context.fillRect(r0, 0, r1 - r0, height);
      context.fillStyle = Hub.accentLine();
      context.fillRect(r0, 0, 1, height);
      context.fillRect(r1 - 1, 0, 1, height);
    }

    // Notes in the track's colour, stronger with velocity, on the grid's own pixels so a note
    // as long as the quantize fills one step exactly. Selected ones stand out in any look:
    // lighter on the dark grid, darker on the bright one, and outlined.
    const colour = (track() || {}).colour || Hub.theme.accent;
    const bright = Hub.theme.luminance >= 50;
    const chosen = bright ? blend(colour, "#000000", 0.45) : blend(colour, "#ffffff", 0.5);
    for (const n of ed.notes) {
      if (drums()) { drawHit(context, n, sx, sy, width, height, colour, chosen, colours); continue; }
      const x0 = Math.round(n.start * ed.zoom - sx), x1 = Math.round((n.start + n.length) * ed.zoom - sx);
      const row = rowOf(n.pitch);
      if (row < 0) continue;
      const w = Math.max(3, x1 - x0), y = row * ed.row - sy;
      if (x0 > width || x0 + w < 0 || y > height || y + ed.row < 0) continue;
      // A muted note: hollow, its outline only (in the selection's colour when selected).
      if (n.muted) {
        context.globalAlpha = 0.14;
        context.fillStyle = colour;
        context.fillRect(x0, y + 1, w, ed.row - 2);
        context.globalAlpha = 1;
        context.strokeStyle = n.selected ? colours.mark : colour;
        context.lineWidth = n.selected ? 2 : 1;
        context.setLineDash([3, 2]);
        context.strokeRect(x0 + 1, y + 2, Math.max(1, w - 2), Math.max(1, ed.row - 4));
        context.setLineDash([]);
        context.lineWidth = 1;
        continue;
      }
      context.globalAlpha = n.selected ? 1 : 0.45 + 0.55 * (n.velocity / 127);
      context.fillStyle = n.selected ? chosen : colour;
      context.fillRect(x0, y + 1, w, ed.row - 2);
      context.globalAlpha = 1;
      if (n.ops) drawOps(context, n, x0, w, y, sx);
      if (n.selected && ed.row >= 8) {
        context.strokeStyle = colours.mark;
        context.lineWidth = 2;
        context.strokeRect(x0 + 1, y + 2, w - 2, ed.row - 4);
        context.lineWidth = 1;
      } else {
        context.strokeStyle = n.selected ? colours.mark : "rgba(0, 0, 0, 0.65)";
        context.strokeRect(x0 + 0.5, y + 1.5, w - 1, ed.row - 3);
      }
    }

    // Where the Split tool would cut the note under the pointer.
    const cut = ed.splitHover;
    if (cut && ed.tool === "split" && !ed.drag) {
      const x = Math.round(cut.time * ed.zoom - sx) + 0.5, y = rowOf(cut.note.pitch) * ed.row - sy;
      context.strokeStyle = bright ? "#000000" : "#ffffff";
      context.lineWidth = 2;
      context.beginPath();
      context.moveTo(x, y - 2);
      context.lineTo(x, y + ed.row + 2);
      context.stroke();
      context.lineWidth = 1;
    }

    // The selection box while dragging one.
    const d = ed.drag;
    if (d && d.kind === "box") {
      const x = Math.min(d.from.x, d.to.x) - sx, y = Math.min(d.from.y, d.to.y) - sy;
      const w = Math.abs(d.to.x - d.from.x), h = Math.abs(d.to.y - d.from.y);
      context.fillStyle = Hub.accentRgba(0.18);
      context.fillRect(x, y, w, h);
      context.strokeStyle = Hub.accentLine();
      context.strokeRect(Math.round(x) + 0.5, Math.round(y) + 0.5, Math.round(w), Math.round(h));
    }
  }

  /** A drum hit: a diamond at its start, stronger with velocity; hollow when muted. */
  function drawHit(context, n, sx, sy, width, height, colour, chosen, colours) {
    const row = rowOf(n.pitch);
    if (row < 0) return;
    const x = Math.round(n.start * ed.zoom - sx) + 0.5, y = row * ed.row - sy;
    const r = Math.max(3, Math.min(9, ed.row / 2 - 1)), cy = y + ed.row / 2;
    if (x + r < 0 || x - r > width || y > height || y + ed.row < 0) return;
    context.beginPath();
    context.moveTo(x, cy - r);
    context.lineTo(x + r, cy);
    context.lineTo(x, cy + r);
    context.lineTo(x - r, cy);
    context.closePath();
    if (n.muted) {
      context.globalAlpha = 0.14;
      context.fillStyle = colour;
      context.fill();
      context.globalAlpha = 1;
      context.setLineDash([2, 2]);
      context.strokeStyle = n.selected ? colours.mark : colour;
      context.stroke();
      context.setLineDash([]);
      return;
    }
    context.globalAlpha = n.selected ? 1 : 0.45 + 0.55 * (n.velocity / 127);
    context.fillStyle = n.selected ? chosen : colour;
    context.fill();
    context.globalAlpha = 1;
    context.lineWidth = n.selected ? 2 : 1;
    context.strokeStyle = n.selected ? colours.mark : "rgba(0, 0, 0, 0.65)";
    context.stroke();
    context.lineWidth = 1;
  }

  /** Folded keys: each row named, a white or black key's colour, the key's notes marked (Scale). */
  function drawFoldedKeys(context, width, height, sy) {
    const lit = new Set((Hub.state.status && Hub.state.status.notes) || []);
    const key = clipKey();
    const firstRow = Math.max(0, Math.floor(sy / ed.row)), lastRow = Math.min(rowCount() - 1, Math.ceil((sy + height) / ed.row));
    context.fillStyle = "#202125";
    context.fillRect(0, 0, width, height);
    context.font = `${Math.max(9, Math.min(11, ed.row - 3))}px Segoe UI, system-ui, sans-serif`;
    context.textBaseline = "middle";
    for (let row = firstRow; row <= lastRow; row++) {
      const pitch = pianoRows[row], y = row * ed.row - sy, black = BLACK.has(pitch % 12);
      context.fillStyle = lit.has(pitch) ? Hub.theme.accent : black ? "#1c1c1f" : "#e6e6e8";
      context.fillRect(0, y, width, ed.row);
      context.fillStyle = "#9ea2a8";
      context.fillRect(0, y + ed.row - 1, width, 1);
      if (inKey(pitch, key)) {
        context.fillStyle = (pitch - key.root) % 12 === 0 ? Hub.theme.accentSet.light : Hub.accentRgba(0.6);
        context.fillRect(width - 5, y + 1, 4, ed.row - 2);
      }
      if (ed.row < 8) continue;
      context.fillStyle = black ? "#d8d9dc" : "#3a3c42";
      context.fillText(noteName(pitch), 6, y + ed.row / 2);
    }
    context.fillStyle = "#9ea2a8";
    context.fillRect(width - 1, 0, 1, height);
  }

  /** The Drums view's names down the left, in place of the keys (lit while they sound). */
  function drawDrumNames(context, width, height, sy) {
    const lit = new Set((Hub.state.status && Hub.state.status.notes) || []);
    const firstRow = Math.max(0, Math.floor(sy / ed.row)), lastRow = Math.min(rowCount() - 1, Math.ceil((sy + height) / ed.row));
    context.fillStyle = "#202125";
    context.fillRect(0, 0, width, height);
    context.font = `${Math.max(9, Math.min(12, ed.row - 3))}px Segoe UI, system-ui, sans-serif`;
    context.textBaseline = "middle";
    for (let row = firstRow; row <= lastRow; row++) {
      const pitch = drumRows[row], y = row * ed.row - sy;
      context.fillStyle = lit.has(pitch) ? Hub.theme.accent : row % 2 ? "#1b1c1f" : "#24252a";
      context.fillRect(0, y, width, ed.row);
      context.fillStyle = "#33353a";
      context.fillRect(0, y + ed.row - 1, width, 1);
      if (ed.row < 8) continue;
      context.fillStyle = "#d8d9dc";
      const name = drumName(pitch);
      let text = name;
      while (text.length > 1 && context.measureText(text).width > width - 34) text = text.slice(0, -1);
      context.fillText(text === name ? name : text + "\u2026", 6, y + ed.row / 2);
      context.fillStyle = "#7d8088";
      context.fillText(noteName(pitch), width - 28, y + ed.row / 2);
    }
    context.fillStyle = "#3a3c42";
    context.fillRect(width - 1, 0, 1, height);
  }

  /** A note's operators, drawn on it: a line at each repeat, a die's dots for Chance (one to
      five), a corner for Occurrence, a square a pass for Recurrence (filled where it plays). */
  function drawOps(context, n, x0, w, y, sx) {
    const o = opsOf(n);
    if (ed.row < 7) return;
    // Dark on a light note, light on a dark one (a selected note is lighter on the dark grid,
    // darker on the bright one).
    const darkInk = (Hub.theme.luminance >= 50) !== Boolean(n.selected);
    const ink = darkInk ? "rgba(0, 0, 0, 0.78)" : "rgba(255, 255, 255, 0.9)";
    context.save();
    context.beginPath();
    context.rect(x0, y + 1, w, ed.row - 2);
    context.clip();
    context.fillStyle = ink;
    context.strokeStyle = ink;

    if (repeating(o))
      for (const t of repeatStarts(n).slice(1)) context.fillRect(Math.round(t * ed.zoom - sx), y + 2, 1, ed.row - 4);

    if (o.c < 100 && w >= 8) {
      const dots = Math.max(1, Math.ceil(o.c / 20));
      for (let i = 0; i < dots && 3 + i * 4 < w - 2; i++) {
        context.beginPath();
        context.arc(x0 + 3.5 + i * 4, y + 4.5, 1.2, 0, Math.PI * 2);
        context.fill();
      }
    }

    if (o.oc && w >= 6) {
      context.beginPath();
      context.moveTo(x0 + w - 1, y + 1);
      context.lineTo(x0 + w - 1, y + 6);
      context.lineTo(x0 + w - 6, y + 1);
      context.closePath();
      context.fill();
    }

    if (o.rl >= 2) {
      const size = Math.max(2, Math.min(4, ed.row - 7));
      const across = o.rl * (size + 1);
      if (across + 4 <= w) {
        const left = x0 + w - across - 2, top = y + ed.row - size - 3;
        for (let k = 0; k < o.rl; k++) {
          const x = left + k * (size + 1);
          if ((o.rm >> k) & 1) context.fillRect(x, top, size, size);
          else context.strokeRect(x + 0.5, top + 0.5, size - 1, size - 1);
        }
      }
    }

    context.restore();
  }

  function drawKeys() {
    const canvas = $("editor-keys");
    const width = canvas.parentElement.clientWidth, height = grid().clientHeight;
    const context = sizeCanvas(canvas, width, height);
    if (!context) return;

    const sy = grid().scrollTop;
    if (drums()) { drawDrumNames(context, width, height, sy); return; }
    if (folded()) { drawFoldedKeys(context, width, height, sy); return; }
    const lit = new Set((Hub.state.status && Hub.state.status.notes) || []);
    const blackWidth = Math.round(width * 0.62);
    const firstRow = Math.max(0, Math.floor(sy / ed.row)), lastRow = Math.min(127, Math.ceil((sy + height) / ed.row));
    context.font = "9px Segoe UI, system-ui, sans-serif";
    context.textBaseline = "middle";

    // The white keys under everything.
    context.fillStyle = "#e6e6e8";
    context.fillRect(0, 0, width, height);
    for (let row = firstRow; row <= lastRow; row++) {
      const pitch = 127 - row;
      if (lit.has(pitch) && !BLACK.has(pitch % 12)) {
        context.fillStyle = Hub.theme.accentSet.pale;
        context.fillRect(0, row * ed.row - sy, width, ed.row);
      }
    }

    // The lines between white keys: right across where no black key sits between them (E and F,
    // B and C), and from a black key's tip to the edge through its middle everywhere else.
    context.fillStyle = "#9ea2a8";
    for (let row = firstRow; row <= lastRow; row++) {
      const pitch = 127 - row, y = row * ed.row - sy;
      if (pitch % 12 === 0 || pitch % 12 === 5) context.fillRect(0, y + ed.row - 1, width, 1);
      if (BLACK.has(pitch % 12)) context.fillRect(blackWidth, y + Math.floor(ed.row / 2), width - blackWidth, 1);
    }

    // The black keys over them.
    for (let row = firstRow; row <= lastRow; row++) {
      const pitch = 127 - row;
      if (!BLACK.has(pitch % 12)) continue;
      context.fillStyle = lit.has(pitch) ? Hub.theme.accent : "#1c1c1f";
      context.fillRect(0, row * ed.row - sy + 1, blackWidth, ed.row - 2);
    }

    // Every C named; the edge against the grid.
    context.fillStyle = "#55585e";
    for (let row = firstRow; row <= lastRow; row++) {
      const pitch = 127 - row;
      if (pitch % 12 === 0) context.fillText(noteName(pitch), width - 26, row * ed.row - sy + ed.row / 2);
    }

    // Scale: the key's notes marked at the keys' edge, its root stronger.
    if (ed.scale) {
      const key = clipKey();
      for (let row = firstRow; row <= lastRow; row++) {
        const pitch = 127 - row;
        if (!inKey(pitch, key)) continue;
        context.fillStyle = (pitch - key.root) % 12 === 0 ? Hub.theme.accentSet.light : Hub.accentRgba(0.6);
        context.fillRect(width - 5, row * ed.row - sy + 1, 4, ed.row - 2);
      }
    }
    context.fillStyle = "#9ea2a8";
    context.fillRect(width - 1, 0, 1, height);
  }

  function drawRuler() {
    const canvas = $("editor-ruler");
    const width = grid().clientWidth, height = canvas.parentElement.clientHeight;
    const context = sizeCanvas(canvas, width, height);
    if (!context) return;

    const sx = grid().scrollLeft;
    const beat = barAt(sx / ed.zoom).beat;
    context.fillStyle = "#1d1e21";
    context.fillRect(0, 0, width, height);

    // The clip's extent along the top.
    const c0 = ed.clipStart * ed.zoom - sx, c1 = clipEnd() * ed.zoom - sx;
    context.fillStyle = (track() || {}).colour || Hub.theme.accent;
    context.fillRect(c0, 0, c1 - c0, 3);

    // Ticks where the grid has lines (quantize steps, beats, bars), numbers as room allows.
    context.font = "11px Segoe UI, system-ui, sans-serif";
    context.textBaseline = "top";
    const from = sx / ed.zoom, to = (sx + width) / ed.zoom + 1e-6;
    const tick = ed.quantize * ed.zoom >= 8 ? map().stepOf(Math.min(ed.quantize, beat), from)
                                            : beat * ed.zoom >= 8 ? { q: beat, bars: 0, beat: true } : gridStep(8);
    const label = gridStep(56, 0.25);
    const labelledAt = new Set(map().lines(from, to, label).map((line) => line.ppq.toFixed(6)));

    for (const line of map().lines(from, to, tick)) {
      const t = line.ppq, x = Math.round(t * ed.zoom - sx) + 0.5;
      const labelled = labelledAt.has(t.toFixed(6));
      context.strokeStyle = line.bar ? Hub.theme.muted : Hub.theme.tick;
      context.beginPath();
      context.moveTo(x, labelled ? 6 : line.beat ? height - 8 : height - 5);
      context.lineTo(x, height);
      context.stroke();
      if (labelled) {
        context.fillStyle = line.bar ? Hub.theme.text : Hub.theme.muted;
        context.fillText(positionLabel(t, label), x + 3, 5);
      }
    }
    context.fillStyle = Hub.theme.line;
    context.fillRect(0, height - 1, width, 1);
  }

  // ---------------------------------------------------------------- the lane under the notes
  /** A lane's y for a value: Pitch Bend's rest in the middle, the rest from the bottom. */
  function laneY(value, height) {
    if (ed.lane === "pitchBend") {
      const mid = height / 2, half = height / 2 - 3;
      return mid - (value >= 0 ? value / BEND_MAX : value / -BEND_MIN) * half;
    }
    const top = 3, bottom = height - 1;
    return bottom - (value / (ed.lane === "velocity" ? 127 : ed.lane === "chance" ? 100 : PRESSURE_MAX)) * (bottom - top);
  }

  /** The value at a pointer position in the lane (and its song time). */
  function laneValue(event) {
    const canvas = $("editor-lane");
    const r = canvas.getBoundingClientRect();
    const height = r.height || 1;
    const y = Hub.clamp(event.clientY - r.top, 0, height);
    const time = Math.max(0, (event.clientX - r.left + grid().scrollLeft) / ed.zoom);

    if (ed.lane === "pitchBend") {
      const mid = height / 2, half = height / 2 - 3;
      const share = Hub.clamp((mid - y) / half, -1, 1);
      const value = share >= 0 ? Math.round(share * BEND_MAX) : Math.round(share * -BEND_MIN);
      return { time, value: Math.abs(value) < 0.05 * BEND_MAX ? 0 : value };   // the middle catches
    }

    const top = 3, bottom = height - 1;
    const share = Hub.clamp((bottom - y) / (bottom - top), 0, 1);
    if (ed.lane === "chance") return { time, value: Math.round(share * 100) };
    return ed.lane === "velocity" ? { time, value: Hub.clamp(Math.round(share * 127), 1, 127) } : { time, value: Math.round(share * PRESSURE_MAX) };
  }

  /** What a line stands at where the clip's window opens: the last point a trim hid before it. */
  function lineStart(name) {
    let value = 0;
    for (const [time, v] of ed.hiddenLines[name]) if (time < ed.clipOffset) value = v;
    return value;
  }

  function laneText(value) {
    if (value === null || value === undefined) return "";
    if (ed.lane === "pitchBend") return value > 0 ? `+${value}` : String(value);
    if (ed.lane === "chance") return `${value}%`;
    return String(value);
  }

  function drawLane() {
    const canvas = $("editor-lane");
    const width = grid().clientWidth, height = canvas.parentElement.clientHeight;
    const context = sizeCanvas(canvas, width, height);
    if (!context) return;

    const sx = grid().scrollLeft;
    const colours = Hub.theme.roll;
    context.fillStyle = colours.velocity;
    context.fillRect(0, 0, width, height);
    context.lineWidth = 1;
    timeLines(context, sx, width, height, { fine: colours.velocity, beat: colours.velocityLine, bar: colours.bar }, 6);

    const c0 = ed.clipStart * ed.zoom - sx, c1 = clipEnd() * ed.zoom - sx;
    const colour = (track() || {}).colour || Hub.theme.accent;

    if (BARS.has(ed.lane)) {
      const chance = ed.lane === "chance";
      context.fillStyle = colours.velocityLine;
      context.fillRect(0, Math.round(height / 2), width, 1);
      for (const n of ed.notes) {
        const x = Math.round(n.start * ed.zoom - sx);
        if (x < -4 || x > width) continue;
        const value = chance ? chanceOf(n) : n.velocity;
        const top = Math.round(laneY(value, height));
        // Vel Spread: the range a pass may move the velocity to, faint behind its bar.
        const spread = chance ? 0 : opsOf(n).vs * 1.27;
        if (spread > 0) {
          const high = Math.round(laneY(Math.min(127, n.velocity + spread), height));
          const low = Math.round(laneY(Math.max(1, n.velocity - spread), height));
          context.globalAlpha = 0.28;
          context.fillStyle = n.selected ? colours.mark : colour;
          context.fillRect(x - 2, high, 7, Math.max(1, low - high));
        }
        context.globalAlpha = n.muted ? 0.35 : 1;
        context.fillStyle = n.selected ? colours.mark : colour;
        context.fillRect(x, top, 3, height - top);
        context.fillRect(x - 1, top - 1, 5, 3);
        context.globalAlpha = 1;
      }
    } else {
      const bend = ed.lane === "pitchBend";
      const base = bend ? Math.round(height / 2) : height - 1;
      context.fillStyle = colours.velocityLine;
      context.fillRect(0, base - (bend ? 0 : 1), width, 1);

      // A step line: each point holds until the next; back to rest at the clip's end.
      let value = lineStart(ed.lane);
      const path = new Path2D();
      path.moveTo(c0, laneY(value, height));
      for (const p of ed.lines[ed.lane]) {
        const x = p.time * ed.zoom - sx;
        path.lineTo(x, laneY(value, height));
        value = p.value;
        path.lineTo(x, laneY(value, height));
      }
      path.lineTo(c1, laneY(value, height));

      const area = new Path2D(path);
      area.lineTo(c1, base);
      area.lineTo(c0, base);
      area.closePath();
      context.globalAlpha = 0.3;
      context.fillStyle = colour;
      context.fill(area);
      context.globalAlpha = 1;
      context.strokeStyle = colour;
      context.lineWidth = 1.5;
      context.stroke(path);
      context.lineWidth = 1;
    }

    // Outside the clip is dimmed, as above.
    context.fillStyle = colours.outside;
    if (c0 > 0) context.fillRect(0, 0, Math.min(c0, width), height);
    if (c1 < width) context.fillRect(Math.max(c1, 0), 0, width - Math.max(c1, 0), height);

    $("editor-lane-label").textContent = laneText(ed.hover);
  }

  function placePlayhead() {
    const playhead = $("editor-playhead");
    const box = grid();
    const x = Hub.songPosition() * ed.zoom - box.scrollLeft;
    playhead.hidden = !isOpen() || x < 0 || x > box.clientWidth;
    playhead.style.height = ($("editor-ruler-wrap").offsetHeight + box.clientHeight) + "px";
    playhead.style.transform = `translateX(${Math.round(x)}px)`;
  }

  function render() {
    const open = ed.clipId !== 0;
    $("editor").classList.toggle("empty", !open);
    document.querySelectorAll("#editor-toolbar [data-tool]").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.tool === ed.tool)));
    document.querySelectorAll("#editor-lane-tabs [data-lane]").forEach((b) => b.setAttribute("aria-selected", String(b.dataset.lane === ed.lane)));
    grid().dataset.tool = ed.tool;
    $("editor-lane-wrap").dataset.tool = !BARS.has(ed.lane) && ed.tool === "eraser" ? "eraser" : "pencil";
    renderOps();
    document.querySelectorAll("#editor-toolbar [data-view]").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.view === ed.view)));
    $("editor-body").classList.toggle("drums", drums());
    if (drums() && !ed.drag) {
      const before = drumRows.length;
      updateDrumRows();
      if (drumRows.length !== before) updateSize();
    }
    if (folded() && !ed.drag) {
      const before = pianoRows.join(",");
      updatePianoRows();
      if (pianoRows.join(",") !== before) updateSize();
    }
    $("editor-scale").setAttribute("aria-pressed", String(ed.scale));
    $("editor-scale").title = `Scale: the song's key (${Hub.TempoMap.keyName(map().keyAt(Math.max(0, ed.clipStart)))}, where the clip starts) lit across the rows`;
    $("editor-fold").value = ed.fold;
    for (const id of ["editor-scale", "editor-fold"]) $(id).hidden = drums();
    $("editor-lane").setAttribute("aria-label", LANE_TIPS[ed.lane]);
    $("editor-lane").title = LANE_TIPS[ed.lane];
    if (!open || document.body.classList.contains("hide-editor")) return;
    drawGrid();
    drawKeys();
    drawRuler();
    drawLane();
    placePlayhead();
    showZoom();
  }

  function updateSize() {
    const quarters = Math.max(map().ppqOfBar(200), map().ppqOfBar(barAt(clipEnd()).index + 32));
    $("editor-size").style.width = Math.ceil(quarters * ed.zoom) + "px";
    $("editor-size").style.height = rowCount() * ed.row + "px";
  }

  // ---------------------------------------------------------------- editing gestures
  function erase(p) {
    const n = noteAt(p);
    if (!n) return;
    ed.notes.splice(ed.notes.indexOf(n), 1);
    ed.drag.erased = true;
  }

  /** A new note where the pointer is, as long as the quantize value, selected alone. */
  function addNote(p) {
    for (const n of ed.notes) n.selected = false;
    const note = { pitch: p.pitch, start: snapFloor(p.time), length: newLength(), velocity: ed.velocity, selected: true };
    ed.notes.push(note);
    audition(note.pitch, note.velocity);
    return note;
  }

  /** The Split tool: the note cut in two at a time (and, when it's selected, every selected
      note under that time). Each part keeps the note's pitch, velocity and mute. */
  function splitAt(hit, time) {
    const pool = hit.selected ? selected() : [hit];
    const shortest = minLength();
    let cut = false;

    for (const n of pool) {
      if (time <= n.start + shortest / 2 || time >= n.start + n.length - shortest / 2) continue;
      const rest = { pitch: n.pitch, start: time, length: n.start + n.length - time, velocity: n.velocity, muted: n.muted, ops: n.ops, selected: n.selected };
      n.length = time - n.start;
      ed.notes.push(rest);
      cut = true;
    }

    if (cut) commit();
    render();
  }

  /** The Split tool's cut, on the grid with Snap on. */
  const splitTime = (p) => (ed.snap ? snapRound(p.time) : p.time);

  /** The Mute tool: a note muted or unmuted, as the drag began (with a selected one, the selection). */
  function muteNote(n, d) {
    const pool = n.selected && d.first ? selected() : [n];
    for (const each of pool) {
      if (d.touched.has(each)) continue;
      d.touched.add(each);
      if (each.muted !== d.state) { each.muted = d.state; d.changed = true; }
    }
    d.first = false;
  }

  // ---------------------------------------------------------------- the Listen tool
  let listening = null;   // {wasPlaying, from, trackId, soloed}

  /** Held down: the clip's track alone, from the pointer's time. */
  function startListening(p) {
    stopListening();
    const t = track();
    listening = { wasPlaying: Boolean(Hub.transportInfo().playing), from: Hub.songPosition(), trackId: ed.trackId, soloed: false };
    if (t && !t.solo) {
      t.solo = true;
      listening.soloed = true;
      Hub.send("trackSettings", { id: t.id, solo: true });
    }
    Hub.locate(p.time);
    if (!listening.wasPlaying) Hub.run("play");
  }

  /** Let go: the song stops and goes back where it was (if it wasn't playing), the solo goes. */
  function stopListening() {
    if (!listening) return;
    const was = listening;
    listening = null;
    if (!was.wasPlaying) {
      if (Hub.transportInfo().playing) Hub.run("stop");
      Hub.locate(was.from);
    }
    if (was.soloed) {
      const t = Hub.trackById(was.trackId);
      if (t) t.solo = false;
      Hub.send("trackSettings", { id: was.trackId, solo: false });
    }
  }

  function onPointerDown(event) {
    ed.downOnNotes = event.target === $("editor-canvas");
    if (event.button !== 0 || !ed.downOnNotes || !ed.clipId) return;
    event.preventDefault();
    const p = point(event);
    const hit = noteAt(p);
    const additive = event.shiftKey || event.ctrlKey || event.metaKey;
    if (grid().setPointerCapture) grid().setPointerCapture(event.pointerId);
    const client = { clientX: event.clientX, clientY: event.clientY };

    if (ed.tool === "eraser") {
      ed.drag = { kind: "erase", pointer: event.pointerId, erased: false, client };
      erase(p);
      render();
      return;
    }

    if (ed.tool === "listen") {
      ed.drag = { kind: "listen", pointer: event.pointerId, client };
      startListening(p);
      return;
    }

    if (ed.tool === "split") {
      if (hit) splitAt(hit, splitTime(p));
      return;
    }

    if (ed.tool === "mute") {
      ed.drag = { kind: "mute", pointer: event.pointerId, state: hit ? !hit.muted : true, touched: new Set(), changed: false, first: true, client };
      if (hit) muteNote(hit, ed.drag);
      render();
      return;
    }

    // The Range tool: a selected note moves them all; anywhere else, a stretch of time begins.
    if (ed.tool === "range" && !(hit && hit.selected)) {
      if (!additive) for (const n of ed.notes) n.selected = false;
      const from = snapRound(p.time);
      ed.range = { from, to: from };
      ed.drag = { kind: "range", pointer: event.pointerId, from, before: new Set(selected()), client };
      render();
      return;
    }

    if (hit) {
      if (additive) {
        hit.selected = !hit.selected;
        if (!hit.selected) { render(); return; }
      } else if (!hit.selected) {
        for (const n of ed.notes) n.selected = false;
        hit.selected = true;
      }

      audition(hit.pitch, hit.velocity);
      const origin = new Map(selected().map((n) => [n, { start: n.start, pitch: n.pitch, length: n.length }]));
      ed.drag = { kind: onEnd(hit, p) ? "resize" : "move", pointer: event.pointerId, grab: p, anchor: hit, origin, changed: false, pitch: hit.pitch, client,
                  // Drums: a click on a hit (no Shift or Ctrl, no drag) takes it away
                  takes: drums() && !additive && (ed.tool === "arrow" || ed.tool === "pencil") };
      render();
      return;
    }

    if (ed.tool === "pencil" && drums()) {
      // Drums: a hit on the step, and more along the row as the drag goes (a roll).
      const note = addNote(p);
      ed.lastAdded = { note, at: performance.now() };
      ed.drag = { kind: "roll", pointer: event.pointerId, pitch: note.pitch, client };
      render();
      return;
    }

    if (ed.tool === "pencil") {
      const note = addNote(p);
      ed.drag = { kind: "draw", pointer: event.pointerId, note, from: p, pulled: false, client };
      render();
      return;
    }

    // Arrow on empty space: a selection box.
    if (!additive) for (const n of ed.notes) n.selected = false;
    ed.drag = { kind: "box", pointer: event.pointerId, from: p, to: p, before: new Set(selected()), client };
    render();
  }

  /** A drag followed to a pointer position (the mouse moving, or the grid scrolling under it). */
  function dragTo(client) {
    const d = ed.drag;
    const p = point(client);
    d.client = client;

    if (d.kind === "move") {
      // The grabbed note's start lands on the grid; the others keep their distance from it.
      const from = d.origin.get(d.anchor);
      let dt = snapRound(from.start + p.time - d.grab.time) - from.start;
      let dp = Math.round((d.grab.y - p.y) / ed.row);
      let lowest = 127, highest = 0, earliest = Infinity, top = Infinity, bottom = -Infinity;
      for (const o of d.origin.values()) {
        lowest = Math.min(lowest, o.pitch); highest = Math.max(highest, o.pitch); earliest = Math.min(earliest, o.start);
        top = Math.min(top, rowOf(o.pitch)); bottom = Math.max(bottom, rowOf(o.pitch));
      }
      dt = Math.max(dt, -earliest);
      if (drums() || folded()) {
        // Rows, not semitones: each hit (folded: each note) moves as many rows up or down.
        const list = rowList();
        const down = Hub.clamp(-dp, -top, list.length - 1 - bottom);
        for (const [n, o] of d.origin) { n.start = o.start + dt; n.pitch = list[rowOf(o.pitch) + down]; }
        dp = down;
      } else {
        dp = Hub.clamp(dp, -lowest, 127 - highest);
        for (const [n, o] of d.origin) { n.start = o.start + dt; n.pitch = o.pitch + dp; }
      }
      d.changed = d.changed || Math.abs(dt) > 1e-9 || dp !== 0;
      if (d.anchor.pitch !== d.pitch) { d.pitch = d.anchor.pitch; audition(d.anchor.pitch, d.anchor.velocity); }
    } else if (d.kind === "resize") {
      // A note's end moves in quarter steps of the quantize value, down to a quarter step long.
      const from = d.origin.get(d.anchor);
      const fine = step() / 4;
      const pulled = from.start + from.length + p.time - d.grab.time;
      const end = fine ? Math.round(pulled / fine) * fine : pulled;
      const change = end - (from.start + from.length);
      for (const [n, o] of d.origin) n.length = Math.max(fine || 1 / 64, o.length + change);
      d.changed = d.changed || Math.abs(change) > 1e-9;
    } else if (d.kind === "draw") {
      // Pulled out by more than a nudge: as long as the pointer goes, in quantize steps.
      d.pulled = d.pulled || Math.abs(p.x - d.from.x) > 4;
      if (d.pulled) d.note.length = Math.max(minLength(), snapRound(p.time) - d.note.start);
    } else if (d.kind === "roll") {
      // Along the hit's row, a hit on each step passed that hasn't one.
      const start = snapFloor(p.time);
      const half = Math.max(step(), 1 / 64) / 2;
      if (start >= 0 && !ed.notes.some((n) => n.pitch === d.pitch && Math.abs(n.start - start) < half))
        ed.notes.push({ pitch: d.pitch, start, length: newLength(), velocity: ed.velocity, selected: false });
    } else if (d.kind === "erase") {
      erase(p);
    } else if (d.kind === "mute") {
      const n = noteAt(p);
      if (n) muteNote(n, d);
    } else if (d.kind === "range") {
      const to = snapRound(p.time);
      ed.range = { from: Math.min(d.from, to), to: Math.max(d.from, to) };
      for (const n of ed.notes)
        n.selected = d.before.has(n) || (n.start >= ed.range.from - 1e-9 && n.start < ed.range.to - 1e-9);
    } else if (d.kind === "box") {
      d.to = p;
      const x0 = Math.min(d.from.x, p.x), x1 = Math.max(d.from.x, p.x), y0 = Math.min(d.from.y, p.y), y1 = Math.max(d.from.y, p.y);
      for (const n of ed.notes) {
        const nx0 = n.start * ed.zoom, nx1 = drums() ? nx0 + 1 : (n.start + n.length) * ed.zoom, ny0 = rowOf(n.pitch) * ed.row, ny1 = ny0 + ed.row;
        n.selected = d.before.has(n) || (nx1 > x0 && nx0 < x1 && ny1 > y0 && ny0 < y1);
      }
    }

    render();
  }

  function onPointerMove(event) {
    const d = ed.drag;

    if (!d) {
      if (event.target !== $("editor-canvas")) return;
      const p = point(event), hit = noteAt(p);
      ed.hoverTime = p.time;
      // The tool's own pointer (the style sheet's); a note's end shows it can be pulled.
      const pulls = (ed.tool === "arrow" || ed.tool === "pencil" || (ed.tool === "range" && hit && hit.selected)) && hit && onEnd(hit, p);
      grid().style.cursor = pulls ? "ew-resize" : "";

      // The Split tool shows where it would cut.
      const cut = ed.tool === "split" && hit ? { note: hit, time: splitTime(p) } : null;
      const was = ed.splitHover;
      if (Boolean(cut) !== Boolean(was) || (cut && (cut.note !== was.note || Math.abs(cut.time - was.time) > 1e-9))) {
        ed.splitHover = cut;
        drawGrid();
      }
      return;
    }

    if (event.pointerId !== d.pointer) return;
    dragTo({ clientX: event.clientX, clientY: event.clientY });
  }

  function onPointerUp(event) {
    const d = ed.drag;
    if (!d || event.pointerId !== d.pointer) return;
    ed.drag = null;

    const clicked = d.kind === "box" && Math.abs(d.to.x - d.from.x) < 4 && Math.abs(d.to.y - d.from.y) < 4;
    const justAdded = (n) => ed.lastAdded && ed.lastAdded.note === n && performance.now() - ed.lastAdded.at < 450;

    if (d.kind === "draw" || d.kind === "roll") {
      commit();
    } else if (drums() && d.kind === "box" && clicked && ed.tool === "arrow") {
      // Drums: a click on an empty step puts a hit there.
      const note = addNote(d.from);
      ed.lastAdded = { note, at: performance.now() };
      commit();
    } else if (d.kind === "move" && d.takes && !d.changed && !justAdded(d.anchor)) {
      // Drums: a click on a hit takes it away.
      ed.notes.splice(ed.notes.indexOf(d.anchor), 1);
      commit();
    } else if ((d.kind === "move" || d.kind === "resize") && d.changed) {
      commit();
    } else if (d.kind === "erase" && d.erased) {
      commit();
    } else if (d.kind === "mute" && d.changed) {
      commit(d.state ? "Mute Notes" : "Unmute Notes");
    } else if (d.kind === "listen") {
      stopListening();
    } else if (d.kind === "range" && ed.range && ed.range.to - ed.range.from < 1e-9) {
      ed.range = null;   // a click, not a stretch: nothing chosen
    }

    // An update from the engine that arrived mid-drag is taken now.
    if (ed.pendingReload) {
      ed.pendingReload = false;
      load();
    }

    render();
  }

  /** Dragging past the grid's edge scrolls it, and the drag follows. */
  function autoscroll() {
    const d = ed.drag;
    if (!d || !d.client || !["box", "move", "resize", "draw", "roll", "erase", "mute", "range"].includes(d.kind)) return;
    const box = grid(), r = box.getBoundingClientRect();
    const right = r.left + box.clientWidth, bottom = r.top + box.clientHeight;
    const dx = d.client.clientX < r.left ? d.client.clientX - r.left : d.client.clientX > right ? d.client.clientX - right : 0;
    const dy = d.client.clientY < r.top ? d.client.clientY - r.top : d.client.clientY > bottom ? d.client.clientY - bottom : 0;
    if (!dx && !dy) return;
    const left = box.scrollLeft, top = box.scrollTop;
    box.scrollLeft = Math.max(0, left + Hub.clamp(dx, -40, 40));
    box.scrollTop = Math.max(0, top + Hub.clamp(dy, -40, 40));
    if (box.scrollLeft !== left || box.scrollTop !== top) dragTo(d.client);
  }

  // ---------------------------------------------------------------- the lane's gestures
  let laneDrag = null;

  /** The Paint tool draws its shape in the lane (anything but Freehand). */
  const shaping = () => ed.tool === "pencil" && ed.shape !== "freehand";

  /** The Paint tool's shape between where a lane drag began and the pointer: the value at a time.
      A Line or a Parabola goes from the one to the other; a wave (Square, Triangle, Saw, Sine)
      swings between their values, four Quantize steps a cycle, from where the drag began. */
  function shapeValue(start, end, time) {
    let w;
    if (ed.shape === "line" || ed.shape === "parabola") {
      const span = end.time - start.time;
      const share = Math.abs(span) < 1e-9 ? 1 : Hub.clamp((time - start.time) / span, 0, 1);
      w = ed.shape === "line" ? share : share * share;
    } else {
      // Four Quantize steps a cycle (a beat at 1/16), from the grid line where the drag began, so
      // notes on the grid take the wave's different points rather than all the same one.
      const period = Math.max(ed.quantize * 4, POINT_STEP * 4);
      const anchor = step() ? snapFloor(start.time + 1e-9) : start.time;
      const phase = Math.abs(time - anchor) / period;
      const x = phase - Math.floor(phase);
      w = ed.shape === "square" ? (x < 0.5 ? 1 : 0)
        : ed.shape === "triangle" ? (x < 0.5 ? 2 * x : 2 - 2 * x)
        : ed.shape === "saw" ? x
        : (1 - Math.cos(2 * Math.PI * x)) / 2;
    }
    return start.value + (end.value - start.value) * w;
  }

  /** The lane's bars (Velocity, or Chance) under a drag: the notes there take its value. */
  function paintBars(event, first) {
    const canvas = $("editor-lane");
    const r = canvas.getBoundingClientRect();
    const x = event.clientX - r.left + grid().scrollLeft;
    const at = laneValue(event);
    const { value } = at;
    const pool = selected().length ? selected() : ed.notes;
    const chance = ed.lane === "chance";
    const get = (n) => (chance ? chanceOf(n) : n.velocity);
    const put = (n, v) => {
      if (get(n) === v) return false;
      if (chance) n.ops = tidyOps(Object.assign(opsOf(n), { c: v }));
      else n.velocity = v;
      return true;
    };
    ed.hover = value;

    // A shape: the notes it spans take its values, the rest keep their own.
    if (shaping()) {
      const d = laneDrag;
      if (first || !d.start) { d.start = at; d.values = new Map(ed.notes.map((n) => [n, get(n)])); }
      const lo = Math.min(d.start.time, at.time), hi = Math.max(d.start.time, at.time);
      for (const n of pool) {
        const inside = n.start >= lo - 1e-9 && n.start <= hi + 1e-9;
        const v = inside ? Hub.clamp(Math.round(shapeValue(d.start, at, n.start)), chance ? 0 : 1, chance ? 100 : 127) : d.values.get(n);
        if (v !== undefined && put(n, v)) d.changed = true;
      }
      render();
      return;
    }

    for (const n of pool)
      if (Math.abs(n.start * ed.zoom - x) <= 4 && put(n, value)) laneDrag.changed = true;

    render();
  }

  /** The value a line has just before a time (rest before its first point). */
  function valueBefore(points, time, start) {
    let value = start;
    for (const p of points) {
      if (p.time >= time - 1e-9) break;
      value = p.value;
    }
    return value;
  }

  /** Drawing a line: what was under the stroke goes, and points along it take its values. The
      Eraser clears what it sweeps back to rest, and what follows keeps its own value. */
  function paintLine(event, first) {
    const d = laneDrag;
    const name = ed.lane;
    const p = laneValue(event);
    p.time = Hub.clamp(p.time, ed.clipStart, clipEnd() - 1e-6);
    ed.hover = p.value;

    if (first) {
      d.last = p;
      d.original = ed.lines[name].slice();
      d.low = d.high = p.time;
    }

    const from = d.last;
    d.low = Math.min(d.low, p.time);
    d.high = Math.max(d.high, p.time);

    // A shape: from where the drag began to the pointer, over the line as it was.
    if (shaping()) {
      if (first || !d.start) d.start = p;
      const lo = Math.min(d.start.time, p.time), hi = Math.max(d.start.time, p.time);
      const [low, high] = name === "pitchBend" ? [BEND_MIN, BEND_MAX] : [0, PRESSURE_MAX];
      const kept = d.original.filter((q) => q.time < lo - 1e-9 || q.time > hi + 1e-9);
      const a = Math.ceil(lo / POINT_STEP - 1e-9), b = Math.floor(hi / POINT_STEP + 1e-9);
      for (let i = a; i <= b; i++) {
        const t = i * POINT_STEP;
        kept.push({ time: t, value: Hub.clamp(Math.round(shapeValue(d.start, p, t)), low, high) });
      }
      ed.lines[name] = tidy(kept);
      d.last = p;
      render();
      return;
    }

    if (ed.tool === "eraser") {
      const start = lineStart(name);
      const kept = d.original.filter((q) => q.time < d.low - 1e-9 || q.time > d.high + 1e-9);
      if (valueBefore(d.original, d.low, start) !== 0) kept.push({ time: d.low, value: 0 });
      const after = valueBefore(d.original, d.high + 1e-6, start);
      const next = d.original.find((q) => q.time > d.high + 1e-9);
      const end = Math.min(clipEnd() - 1e-6, Math.ceil((d.high + 1e-9) / POINT_STEP) * POINT_STEP);
      if (after !== 0 && (!next || next.time > end + 1e-9) && end > d.high) kept.push({ time: end, value: after });
      ed.lines[name] = tidy(kept);
    } else {
      const t0 = Math.min(from.time, p.time), t1 = Math.max(from.time, p.time);
      const kept = ed.lines[name].filter((q) => q.time < t0 - 1e-9 || q.time > t1 + 1e-9);
      const a = Math.ceil(t0 / POINT_STEP - 1e-9), b = Math.floor(t1 / POINT_STEP + 1e-9);
      for (let i = a; i <= b; i++) {
        const t = i * POINT_STEP;
        const share = p.time === from.time ? 1 : Hub.clamp((t - from.time) / (p.time - from.time), 0, 1);
        kept.push({ time: t, value: Math.round(from.value + (p.value - from.value) * share) });
      }
      // Where the pointer is now gets its value exactly (the line ends where the hand stopped).
      kept.push({ time: Hub.clamp(Math.round(p.time / POINT_STEP) * POINT_STEP, ed.clipStart, clipEnd() - 1e-6), value: p.value });
      ed.lines[name] = tidy(kept);
    }

    d.last = p;
    render();
  }

  /** Points by time, one per moment (the last wins). */
  function tidy(points) {
    points.sort((x, y) => x.time - y.time);
    const kept = [];
    for (const q of points) {
      if (kept.length && Math.abs(kept[kept.length - 1].time - q.time) < 1e-9) kept[kept.length - 1] = q;
      else kept.push(q);
    }
    return kept;
  }

  /** A point that changes nothing (the same value as the one before it) is dropped. */
  function thin(points, start) {
    const kept = [];
    let value = start;
    for (const q of points) {
      if (q.value === value && kept.length) continue;
      kept.push(q);
      value = q.value;
    }
    return kept;
  }

  // ---------------------------------------------------------------- commands
  let clipboard = null;   // copied notes: {notes: [{pitch, offset (from the first), length, velocity}]}

  function deleteSelected() {
    const before = ed.notes.length;
    ed.notes = ed.notes.filter((n) => !n.selected);
    if (ed.notes.length !== before) { commit(); render(); }
  }

  function copySelected() {
    const pool = selected();
    if (!pool.length) return false;
    const first = Math.min(...pool.map((n) => n.start));
    clipboard = { notes: pool.map((n) => ({ pitch: n.pitch, offset: n.start - first, length: n.length, velocity: n.velocity, ops: n.ops })) };
    return true;
  }

  /** Copies of notes from a position on, selected instead of what was. */
  function place(list, at) {
    for (const n of ed.notes) n.selected = false;
    for (const c of list) ed.notes.push({ pitch: c.pitch, start: Math.max(0, at + c.offset), length: c.length, velocity: c.velocity, ops: c.ops || null, selected: true });
    commit();
    render();
  }

  /** Where Paste puts notes: under the mouse, at the playhead when it's in the clip, or at the clip's start. */
  function pastePosition() {
    if (ed.hoverTime !== undefined && ed.hoverInside) return snapFloor(ed.hoverTime);
    const playhead = Hub.songPosition();
    return playhead >= ed.clipStart && playhead < clipEnd() ? snapFloor(playhead) : ed.clipStart;
  }

  function paste(at) {
    if (clipboard && clipboard.notes.length) place(clipboard.notes, at);
  }

  /** Right after the selection, on the grid: Studio One's Duplicate. */
  function duplicateSelected() {
    const pool = selected();
    if (!pool.length) return;
    const first = Math.min(...pool.map((n) => n.start)), last = Math.max(...pool.map((n) => n.start + n.length));
    const q = step();
    const span = q ? Math.max(q, Math.ceil((last - first) / q - 1e-9) * q) : last - first;
    place(pool.map((n) => ({ pitch: n.pitch, offset: n.start - first, length: n.length, velocity: n.velocity, ops: n.ops })), first + span);
  }

  /** The selected notes (or all) onto the grid, with the Quantize panel's settings (hub-quantize.js):
      its grid, or the Editor's Quantize when it follows, its strength and swing, starts and ends. */
  function quantize() {
    const pool = selected().length ? selected() : ed.notes;
    if (!pool.length) return;
    const panel = Hub.quantize;
    const grid = panel ? panel.gridFor("editor") : ed.quantize;
    for (const n of pool) {
      if (!panel) { n.start = map().snap(n.start, quantizeStep(n.start)); continue; }
      const placed = panel.place(n.start, n.length, grid);
      n.start = placed.start;
      n.length = placed.length;
    }
    commit();
    render();
  }

  function transpose(amount) {
    const pool = selected();
    if (!pool.length) return;
    if (drums()) {
      // Drums: a row up or down (Shift too: the rows are sounds, not octaves).
      const rows = pool.map((n) => rowOf(n.pitch));
      const down = Hub.clamp(amount > 0 ? -1 : 1, -Math.min(...rows), drumRows.length - 1 - Math.max(...rows));
      if (!down) return;
      for (const n of pool) n.pitch = drumRows[rowOf(n.pitch) + down];
      if (pool.length === 1) audition(pool[0].pitch, pool[0].velocity);
      commit();
      render();
      return;
    }
    const lowest = Math.min(...pool.map((n) => n.pitch)), highest = Math.max(...pool.map((n) => n.pitch));
    const change = Hub.clamp(amount, -lowest, 127 - highest);
    if (!change) return;
    for (const n of pool) n.pitch += change;
    if (pool.length === 1) audition(pool[0].pitch, pool[0].velocity);
    commit();
    render();
  }

  function nudge(direction) {
    const pool = selected();
    if (!pool.length) return;
    const amount = ed.quantize * direction;
    if (Math.min(...pool.map((n) => n.start)) + amount < -1e-9) return;
    for (const n of pool) n.start += amount;
    commit();
    render();
  }

  function setTool(tool) {
    if (!TOOLS.some(([name]) => name === tool)) return;
    stopListening();
    if (tool !== "range") ed.range = null;
    ed.splitHover = null;
    ed.tool = tool;
    render();
  }

  /** The Paint tool with a shape for the lane (Studio One's Paint Tool menu). */
  function setShape(shape) {
    if (!SHAPES.some(([name]) => name === shape)) return;
    ed.shape = shape;
    setTool("pencil");
  }

  function deselectAll() {
    ed.range = null;
    for (const n of ed.notes) n.selected = false;
    render();
  }

  function setLane(lane) {
    if (!LANE_TIPS[lane]) return;
    ed.lane = lane;
    ed.hover = null;
    render();
  }

  /** Studio One's right-click menu: the tools (the Paint tool's shapes beside it), then Cut,
      Copy, Paste (where it was clicked), Duplicate and Delete, Select All and Deselect All, Undo
      and Redo. A note right-clicked joins the selection first. Each shows its key in the map in use. */
  function showMenu(event) {
    const p = point(event), hit = noteAt(p);

    if (hit && !hit.selected) {
      for (const n of ed.notes) n.selected = false;
      hit.selected = true;
      render();
    }

    const key = (command) => (Hub.keys ? Hub.keys.label(command) : "");
    const any = selected().length > 0;
    const song = Hub.state.song || {};
    const items = TOOLS.map(([tool, label, command]) => ({
      label, value: "tool:" + tool, icon: ICONS[tool], checked: ed.tool === tool, shortcut: key(command),
      submenu: tool === "pencil" ? SHAPES.map(([shape, name, icon]) => ({ label: name, value: "shape:" + shape, icon, checked: ed.shape === shape,
                                                                     title: "The Paint tool's shape in the lane under the notes" })) : undefined
    }));

    items.push({ separator: true },
      { label: "Cut", value: "cut", shortcut: key("cut"), disabled: !any },
      { label: "Copy", value: "copy", shortcut: key("copy"), disabled: !any },
      { label: "Paste", value: "paste", shortcut: key("paste"), disabled: !(clipboard && clipboard.notes.length) },
      { label: "Duplicate", value: "duplicate", shortcut: key("duplicate"), disabled: !any },
      { label: "Delete", value: "delete", shortcut: key("delete"), disabled: !any },
      { separator: true },
      { label: "Select All", value: "selectAll", shortcut: key("selectAll"), disabled: !ed.notes.length },
      { label: "Deselect All", value: "deselectAll", shortcut: key("deselectAll"), disabled: !any && !ed.range },
      { separator: true },
      { label: song.undo ? `Undo ${song.undo}` : "Undo", value: "undo", shortcut: key("undo"), disabled: !song.undo },
      { label: song.redo ? `Redo ${song.redo}` : "Redo", value: "redo", shortcut: key("redo"), disabled: !song.redo });

    Hub.popupMenu(Hub.pointAnchor(event), items, (pick) => {
      if (typeof pick !== "string") return;
      if (pick.startsWith("tool:")) setTool(pick.slice(5));
      else if (pick.startsWith("shape:")) setShape(pick.slice(6));
      else if (pick === "cut") { if (copySelected()) deleteSelected(); }
      else if (pick === "copy") copySelected();
      else if (pick === "paste") paste(snapFloor(p.time));
      else if (pick === "duplicate") duplicateSelected();
      else if (pick === "delete") deleteSelected();
      else if (pick === "selectAll") { for (const n of ed.notes) n.selected = true; render(); }
      else if (pick === "deselectAll") deselectAll();
      else if (pick === "undo" || pick === "redo") Hub.run(pick);
    });
  }

  // ---------------------------------------------------------------- the Operators row
  /** A number box showing a value (none: the selected notes differ, or there are none). */
  function showNumber(id, value, off) {
    const input = $(id);
    input.disabled = off;
    input.closest(".ops-field").classList.toggle("off", off);
    if (document.activeElement === input) return;
    input.value = value === undefined || off ? "" : String(value);
    input.placeholder = value === undefined && !off ? "\u2013" : "";
  }

  /** A list showing a choice (none: they differ, shown as a dash). */
  function showChoice(id, value, off) {
    const select = $(id);
    select.disabled = off;
    select.closest(".ops-field").classList.toggle("off", off);
    if (document.activeElement === select) return;
    let option = [...select.options].find((o) => o.value === value);
    if (value !== undefined && !option) {
      // A value the list doesn't have (a count from a song file): shown as it is.
      option = new Option(value.startsWith("n:") ? `\u00d7${value.slice(2)}` : value, value);
      select.add(option);
    }
    select.querySelector("option[data-mixed]").hidden = value !== undefined;
    select.value = value === undefined ? "mixed" : value;
  }

  /** The row as the selection stands. */
  function renderOps() {
    const bar = $("editor-ops");
    if (!bar) return;
    bar.hidden = !ed.showOps;
    $("editor-ops-toggle").setAttribute("aria-pressed", String(ed.showOps));
    if (!ed.showOps) return;

    const pool = selected(), none = pool.length === 0;
    const all = pool.map(opsOf);
    const same = (get) => (all.length && all.every((o) => get(o) === get(all[0])) ? get(all[0]) : undefined);
    showNumber("ops-chance", same((o) => o.c), none);
    showNumber("ops-spread", same((o) => o.vs), none);
    showChoice("ops-repeats", same(repeatCode), none);
    const repeats = all.some(repeating);
    showNumber("ops-curve", same((o) => (repeating(o) ? o.rc : 0)), none || !repeats);
    showNumber("ops-vel-end", same((o) => (repeating(o) ? o.rv : 0)), none || !repeats);
    showChoice("ops-occurrence", same((o) => o.oc), none);
    const cycle = same((o) => o.rl);
    showChoice("ops-recurrence", cycle === undefined ? undefined : String(cycle >= 2 ? cycle : 0), none);

    // A button a pass, pressed where every selected note plays on it.
    const passes = $("ops-passes");
    const length = !none && cycle >= 2 ? cycle : 0;
    if (passes.children.length !== length) {
      passes.textContent = "";
      for (let k = 0; k < length; k++) {
        const b = document.createElement("button");
        b.type = "button";
        b.textContent = String(k + 1);
        b.dataset.pass = String(k);
        b.title = `Pass ${k + 1} of ${length}: click to play or skip it`;
        passes.appendChild(b);
      }
    }
    [...passes.children].forEach((b, k) => {
      const on = all.filter((o) => (o.rm >> k) & 1).length;
      b.setAttribute("aria-pressed", String(on === all.length && on > 0));
      b.classList.toggle("mixed", on > 0 && on < all.length);
    });

    $("ops-hint").textContent = none ? "Select notes to set their operators" : pool.length === 1 ? "1 note" : `${pool.length} notes`;
    const seed = $("ops-seed");
    if (document.activeElement !== seed) seed.value = String(ed.seed);
  }

  /** A change to every selected note's operators (one undo step). */
  function applyOps(change, label = "Note Operators") {
    const pool = selected();
    if (!pool.length) return;
    for (const n of pool) {
      const o = opsOf(n);
      change(o);
      n.ops = tidyOps(o);
    }
    commit(label);
    render();
  }

  function setSeed(seed, label) {
    ed.seed = Hub.clamp(Math.round(Number(seed)) || 0, 0, 99999);
    ed.seedEdited = true;
    commit(label);
    render();
  }

  function setupOps() {
    const fill = (select, items) => {
      const mixed = new Option("\u2013", "mixed");
      mixed.dataset.mixed = "1";
      mixed.hidden = true;
      select.add(mixed);
      for (const [value, label] of items) select.add(new Option(label, value));
    };
    fill($("ops-repeats"), REPEATS);
    fill($("ops-occurrence"), OCCURRENCES);
    fill($("ops-recurrence"), [["0", "Off"]].concat([2, 3, 4, 5, 6, 7, 8].map((k) => [String(k), `${k} passes`])));

    const number = (id, low, high, put) => $(id).addEventListener("change", (event) => {
      const v = Number(event.target.value);
      if (event.target.value === "" || !Number.isFinite(v)) { renderOps(); return; }
      const value = Hub.clamp(Math.round(v), low, high);
      applyOps((o) => put(o, value));
      event.target.blur();
    });
    number("ops-chance", 0, 100, (o, v) => { o.c = v; });
    number("ops-spread", 0, 100, (o, v) => { o.vs = v; });
    number("ops-curve", -100, 100, (o, v) => { o.rc = v; });
    number("ops-vel-end", -100, 100, (o, v) => { o.rv = v; });

    $("ops-repeats").addEventListener("change", (event) => {
      const code = event.target.value;
      if (code === "mixed") return;
      applyOps((o) => {
        o.rn = code.startsWith("n:") ? Number(code.slice(2)) : 0;
        o.rr = code.startsWith("r:") ? (RATES[code] || Number(code.slice(2)) || 0) : 0;
      });
      event.target.blur();
    });
    $("ops-occurrence").addEventListener("change", (event) => {
      if (event.target.value === "mixed") return;
      applyOps((o) => { o.oc = event.target.value; });
      event.target.blur();
    });
    $("ops-recurrence").addEventListener("change", (event) => {
      if (event.target.value === "mixed") return;
      const length = Number(event.target.value);
      // A new cycle plays on every pass to begin with; a note that had one keeps its passes.
      applyOps((o) => {
        if (length >= 2 && (o.rl < 2 || (o.rm & ((1 << length) - 1)) === 0)) o.rm = 255;
        o.rl = length;
      });
      event.target.blur();
    });
    $("ops-passes").addEventListener("click", (event) => {
      const b = event.target.closest("button[data-pass]");
      if (!b) return;
      const bit = 1 << Number(b.dataset.pass);
      const allOn = selected().every((n) => opsOf(n).rm & bit);
      applyOps((o) => { o.rm = allOn ? o.rm & ~bit : o.rm | bit; });
    });

    $("ops-seed").addEventListener("change", (event) => {
      const v = Number(event.target.value);
      if (event.target.value === "" || !Number.isFinite(v)) { renderOps(); return; }
      setSeed(v, "Seed");
      event.target.blur();
    });
    $("ops-roll").addEventListener("click", () => {
      let seed = ed.seed;
      while (seed === ed.seed) seed = 1 + Math.floor(Math.random() * 99999);
      setSeed(seed, "New Seed");
    });
    $("editor-ops-toggle").addEventListener("click", () => {
      ed.showOps = !ed.showOps;
      render();
    });
    $("editor-process").addEventListener("click", (event) => processMenu(event.currentTarget));
    $("editor-scale").addEventListener("click", () => { ed.scale = !ed.scale; render(); });
    $("editor-fold").addEventListener("change", (event) => setFold(event.target.value));
  }

  /** Opens a clip in the Editor, zoomed to fit and scrolled to its notes. */
  Hub.openEditor = (trackId, clipId) => {
    ed.trackId = trackId;
    ed.clipId = clipId;
    ed.notes = [];
    ed.view = views.get(trackId) || "piano";
    document.body.classList.remove("hide-editor");
    document.body.classList.add("hide-console", "hide-audio-editor");
    Hub.emit("layout");
    Hub.selectTrack(trackId, "keep");
    Hub.state.focus = "editor";

    const clip = clipOf();
    if (!clip) { ed.clipId = 0; render(); return; }

    // Shown and given its full height before measuring, so the width we fit the clip
    // into is the one left once the vertical scrollbar is there.
    $("editor").classList.remove("empty");
    ed.clipStart = clip.start;
    ed.clipLength = clip.length;
    updateSize();
    const box = grid();
    if (box.clientWidth > 0) ed.zoom = Hub.clamp((box.clientWidth - 48) / Math.max(clip.length, 1), MIN_ZOOM, MAX_ZOOM);
    load();

    const pitches = ed.notes.map((n) => n.pitch);
    const centre = pitches.length ? (Math.min(...pitches) + Math.max(...pitches)) / 2 : 60;
    box.scrollLeft = Math.max(0, clip.start * ed.zoom - 24);
    if (drums()) { updateDrumRows(); updateSize(); }
    if (folded()) { updatePianoRows(); updateSize(); }
    box.scrollTop = drums() ? 0 : Math.max(0, centreRow(centre) * ed.row - box.clientHeight / 2);
    render();
  };

  /** The row nearest a pitch (folded: the row closest to it). */
  function centreRow(pitch) {
    if (!folded()) return 127 - pitch;
    let best = 0;
    pianoRows.forEach((p, row) => { if (Math.abs(p - pitch) < Math.abs(pianoRows[best] - pitch)) best = row; });
    return best;
  }

  /** Fold: "off" (every key), "key" (the song's key, and any note off it) or "used" (the clip's notes). */
  function setFold(fold) {
    if (!["off", "key", "used"].includes(fold)) return;
    const box = grid();
    const middle = pitchOfRow(Math.floor((box.scrollTop + box.clientHeight / 2) / ed.row));
    ed.fold = fold;
    ed.drag = null;
    if (folded()) updatePianoRows();
    updateSize();
    box.scrollTop = Math.max(0, centreRow(middle) * ed.row - box.clientHeight / 2);
    render();
  }

  // ---------------------------------------------------------------- Process
  /** The Process menu's changes, on the selected notes (or all of them): one Undo step each. */
  function process(what) {
    const pool = selected().length ? selected() : ed.notes.slice();
    if (!pool.length) { Hub.toast("No notes to process."); return; }
    const MIN = 1 / 64;
    const copy = (n, extra) => Object.assign({ pitch: n.pitch, start: n.start, length: n.length, velocity: n.velocity, muted: n.muted,
                                               ops: n.ops ? JSON.parse(JSON.stringify(n.ops)) : null, selected: n.selected }, extra);
    const remove = (gone) => { ed.notes = ed.notes.filter((n) => !gone.has(n)); };
    const first = Math.min(...pool.map((n) => n.start)), last = Math.max(...pool.map((n) => n.start + n.length));
    let label = "";

    if (what.startsWith("chop:")) {
      // Each note cut into equal parts, or at the Quantize grid's lines inside it.
      const how = what.slice(5), made = [];
      for (const n of pool) {
        const cuts = [];
        if (how === "grid") {
          for (const line of map().lines(n.start, n.start + n.length, quantizeStep(n.start)))
            if (line.ppq > n.start + MIN && line.ppq < n.start + n.length - MIN && cuts.length < 512) cuts.push(line.ppq);
        } else {
          const parts = Number(how);
          if (n.length / parts >= MIN) for (let i = 1; i < parts; i++) cuts.push(n.start + n.length * i / parts);
        }
        if (!cuts.length) continue;
        const edges = [n.start, ...cuts, n.start + n.length];
        n.length = edges[1] - edges[0];
        for (let i = 1; i + 1 < edges.length; i++) made.push(copy(n, { start: edges[i], length: edges[i + 1] - edges[i] }));
      }
      if (!made.length) { Hub.toast("The notes are too short to chop that way."); return; }
      ed.notes.push(...made);
      label = "Chop";
    } else if (what === "glue") {
      // Notes of a key that touch or overlap become one (the first one's velocity).
      const gone = new Set();
      const byPitch = new Map();
      for (const n of pool) { if (!byPitch.has(n.pitch)) byPitch.set(n.pitch, []); byPitch.get(n.pitch).push(n); }
      for (const list of byPitch.values()) {
        list.sort((a, b) => a.start - b.start);
        let held = list[0];
        for (const n of list.slice(1)) {
          if (n.start <= held.start + held.length + 1e-6) { held.length = Math.max(held.start + held.length, n.start + n.length) - held.start; gone.add(n); }
          else held = n;
        }
      }
      if (!gone.size) { Hub.toast("No notes of the same key touch: nothing to glue."); return; }
      remove(gone);
      label = "Glue";
    } else if (what === "legato") {
      // Each note up to the next note's start (notes starting together: a chord, kept together).
      const starts = [...new Set(pool.map((n) => Math.round(n.start * 1e6) / 1e6))].sort((a, b) => a - b);
      for (const n of pool) {
        const next = starts.find((s) => s > n.start + 1e-6);
        if (next !== undefined) n.length = Math.max(MIN, next - n.start);
      }
      label = "Legato";
    } else if (what === "reverse") {
      for (const n of pool) n.start = first + (last - (n.start + n.length));
      label = "Reverse";
    } else if (what === "invert") {
      const low = Math.min(...pool.map((n) => n.pitch)), high = Math.max(...pool.map((n) => n.pitch));
      for (const n of pool) n.pitch = Hub.clamp(low + high - n.pitch, 0, 127);
      label = "Invert";
    } else if (what === "double" || what === "half") {
      const f = what === "double" ? 0.5 : 2;
      for (const n of pool) { n.start = first + (n.start - first) * f; n.length = Math.max(MIN, n.length * f); }
      label = what === "double" ? "Double Speed" : "Half Speed";
    } else if (what === "fitKey") {
      let moved = 0;
      for (const n of pool) {
        const key = keyAtTime(n.start);
        if (inKey(n.pitch, key)) continue;
        for (let d = 1; d < 12; d++) {
          if (inKey(n.pitch - d, key)) { n.pitch = Math.max(0, n.pitch - d); break; }
          if (inKey(n.pitch + d, key)) { n.pitch = Math.min(127, n.pitch + d); break; }
        }
        moved++;
      }
      if (!moved) { Hub.toast("The notes are in the song's key already."); return; }
      label = "Fit to Key";
    } else if (what.startsWith("vel:")) {
      const how = what.slice(4);
      const loudest = Math.max(...pool.map((n) => n.velocity)), average = pool.reduce((sum, n) => sum + n.velocity, 0) / pool.length;
      const ordered = pool.slice().sort((a, b) => a.start - b.start || a.pitch - b.pitch);
      const lastStart = Math.max(...pool.map((n) => n.start)), span = lastStart - first;
      const along = (n) => (span > 1e-9 ? (n.start - first) / span : 0);
      for (const n of ordered) {
        if (how === "up") n.velocity = loudest * (0.3 + 0.7 * along(n));
        else if (how === "down") n.velocity = loudest * (1 - 0.7 * along(n));
        else if (how === "set") n.velocity = ed.velocity;
        else if (how === "compress") n.velocity = (n.velocity + average) / 2;
        n.velocity = Hub.clamp(Math.round(n.velocity), 1, 127);
      }
      label = { up: "Crescendo", down: "Decrescendo", set: "Set Velocity", compress: "Compress Velocity" }[how] || "Velocity";
    } else if (what === "doubles") {
      // Notes of a key starting together: the longest stays.
      const gone = new Set(), seen = new Map();
      for (const n of pool.slice().sort((a, b) => b.length - a.length)) {
        const k = `${n.pitch}|${Math.round(n.start * 1e4)}`;
        if (seen.has(k)) gone.add(n); else seen.set(k, n);
      }
      if (!gone.size) { Hub.toast("No doubled notes."); return; }
      remove(gone);
      label = "Remove Doubles";
    } else {
      return;
    }

    commit(label);
    render();
  }

  function processMenu(anchor) {
    const any = ed.notes.length > 0, on = selected().length ? "the selected notes" : "all the notes";
    Hub.popupMenu(anchor, [
      { group: `Process ${on}` },
      { label: "Chop", disabled: !any, submenu: [
          { label: "In 2", value: "chop:2" }, { label: "In 3", value: "chop:3" }, { label: "In 4", value: "chop:4" },
          { label: "On the Quantize Grid", value: "chop:grid", title: "Cut at each line of the Quantize grid inside each note" }] },
      { label: "Glue", value: "glue", disabled: !any, title: "Notes of the same key that touch or overlap become one" },
      { label: "Legato", value: "legato", disabled: !any, title: "Each note up to the next note's start" },
      { separator: true },
      { label: "Reverse", value: "reverse", disabled: !any, title: "Backwards in time" },
      { label: "Invert", value: "invert", disabled: !any || drums(), title: "Upside down: the highest note becomes the lowest" },
      { label: "Double Speed", value: "double", disabled: !any },
      { label: "Half Speed", value: "half", disabled: !any },
      { label: "Fit to Key", value: "fitKey", disabled: !any || drums(), title: "Each note off the song's key moved to its nearest note" },
      { separator: true },
      { label: "Velocity", disabled: !any, submenu: [
          { label: "Crescendo", value: "vel:up", title: "Softer to louder, up to the loudest" },
          { label: "Decrescendo", value: "vel:down", title: "Louder to softer, from the loudest" },
          { label: `Set to ${ed.velocity}`, value: "vel:set", title: "The toolbar's Velocity" },
          { label: "Compress", value: "vel:compress", title: "Halfway to their average: more even" }] },
      { label: "Remove Doubles", value: "doubles", disabled: !any, title: "Notes of the same key starting together: the longest stays" }
    ], (pick) => { if (typeof pick === "string" && pick) process(pick); });
  }

  /** The toolbar's switch: the Piano view or the Drums view (the clip stays as it is). */
  function setView(view) {
    if (view !== "piano" && view !== "drums") return;
    if (view === ed.view) return;
    ed.view = view;
    if (ed.trackId) views.set(ed.trackId, view);
    ed.drag = null;
    if (drums()) updateDrumRows();
    $("editor-body").classList.toggle("drums", drums());
    updateSize();
    const box = grid();
    if (drums()) box.scrollTop = 0;
    else {
      const pitches = ed.notes.map((n) => n.pitch);
      const centre = pitches.length ? (Math.min(...pitches) + Math.max(...pitches)) / 2 : 60;
      if (folded()) updatePianoRows();
      updateSize();
      box.scrollTop = Math.max(0, centreRow(centre) * ed.row - box.clientHeight / 2);
    }
    render();
  }

  /** The Drums view's row names: one renamed in place (empty: back to its usual name). */
  function renameRow(pitch) {
    const row = rowOf(pitch);
    if (row < 0) return;
    const wrapBox = $("editor-keys-wrap");
    wrapBox.querySelectorAll(".drum-rename").forEach((old) => old.remove());
    const input = document.createElement("input");
    input.className = "drum-rename";
    input.type = "text";
    input.maxLength = 40;
    input.value = drumName(pitch);
    input.setAttribute("aria-label", `Name of the row for ${noteName(pitch)}`);
    input.style.top = `${row * ed.row - grid().scrollTop}px`;
    input.style.height = `${Math.max(18, ed.row)}px`;
    wrapBox.appendChild(input);
    input.focus();
    input.select();
    let done = false;
    const finish = (keep) => {
      if (done) return;
      done = true;
      const name = input.value.trim();
      input.remove();
      if (!keep || name === drumName(pitch)) return;
      setDrumMap((m) => {
        if (!name || name === (GM_DRUMS[pitch] || noteName(pitch))) delete m.names[String(pitch)];
        else m.names[String(pitch)] = name;
      });
    };
    input.addEventListener("keydown", (event) => {
      event.stopPropagation();
      if (event.key === "Enter") finish(true);
      else if (event.key === "Escape") finish(false);
    });
    input.addEventListener("blur", () => finish(true));
  }

  /** The drum sounds not in the view yet, to add one. */
  function addRowItems() {
    const shown = new Set(drumRows);
    const items = [{ group: "Add a drum sound" }];
    for (let pitch = 35; pitch <= 81; pitch++)
      if (!shown.has(pitch)) items.push({ label: `${GM_DRUMS[pitch]}  (${noteName(pitch)})`, value: "add:" + pitch });
    return items;
  }

  function addRow(pitch) {
    setDrumMap((m) => { if (!m.rows.includes(pitch)) m.rows.push(pitch); });
    const box = grid(), row = rowOf(pitch);
    if (row >= 0) box.scrollTop = Math.max(0, row * ed.row - box.clientHeight / 2);
    audition(pitch, ed.velocity);
  }

  /** A right-click on a row's name: rename it, add a row, or take an added row away (when nothing plays on it). */
  function rowMenu(event, pitch) {
    const own = drumMapOf();
    const added = own && Array.isArray(own.rows) && own.rows.includes(pitch);
    const used = ed.notes.some((n) => n.pitch === pitch) || ed.hidden.some((note) => note[0] === pitch);
    const items = [{ label: `Rename "${drumName(pitch)}"\u2026`, value: "rename" },
                   { label: "Back to its usual name", value: "reset", disabled: !(own && own.names && own.names[String(pitch)]) },
                   { label: "Take this row away", value: "remove", disabled: !added || used || CORE_KIT.includes(pitch),
                     title: "Only a row added with +, with nothing on it" },
                   { separator: true }, ...addRowItems()];
    Hub.popupMenu(Hub.pointAnchor(event), items, (pick) => {
      if (pick === "rename") renameRow(pitch);
      else if (pick === "reset") setDrumMap((m) => { delete m.names[String(pitch)]; });
      else if (pick === "remove") setDrumMap((m) => { m.rows = m.rows.filter((p) => p !== pitch); });
      else if (typeof pick === "string" && pick.startsWith("add:")) addRow(Number(pick.slice(4)));
    });
  }

  /** A MIDI tool's result (hub-miditools.js): the notes it took give way to the ones it made
      (song positions), which come in selected. One undo step, named after the tool. */
  /** A MIDI tool's result: the notes it took give way to the ones it made, in the clip it worked
      on (where: { trackId, clipId }). The notes taken are found by pitch, start and length, as the
      Editor may have refreshed its notes meanwhile. False (and nothing changed) when the Editor
      shows another clip by now. */
  function replaceNotes(taken, made, label, where) {
    if (!ed.clipId || (where && (where.trackId !== ed.trackId || where.clipId !== ed.clipId))) return false;
    const key = (n) => `${n.pitch}|${Math.round(n.start * 1e6)}|${Math.round(n.length * 1e6)}`;
    const gone = new Map();
    for (const n of taken) gone.set(key(n), (gone.get(key(n)) || 0) + 1);
    ed.notes = ed.notes.filter((n) => {
      const k = key(n), left = gone.get(k) || 0;
      if (!left) return true;
      gone.set(k, left - 1);
      return false;
    });
    for (const n of ed.notes) n.selected = false;
    for (const n of made)
      ed.notes.push({ pitch: Hub.clamp(Math.round(n.pitch), 0, 127), start: Math.max(0, n.start), length: Math.max(1 / 64, n.length),
                      velocity: Hub.clamp(Math.round(n.velocity || 100), 1, 127), ops: n.ops ? tidyOps(opsOf(n)) : null, selected: true });
    commit(label);
    render();
    return true;
  }

  Hub.editor = { state: ed, open: Hub.openEditor, isOpen, setTool, setShape, setLane, replaceNotes, quantize,
                 showOperators: (on = true) => { ed.showOps = Boolean(on); render(); } };

  // Settings > Appearance: the piano roll follows it.
  Hub.on("theme", () => render());

  // ---------------------------------------------------------------- zoom: length and height
  // As the arrangement's: the sliders run log steps of the length zoom, and the rows' height.
  const zoomToSlider = (zoom) => Math.round(1000 * Math.log(zoom / MIN_ZOOM) / Math.log(MAX_ZOOM / MIN_ZOOM));
  const sliderToZoom = (value) => MIN_ZOOM * Math.pow(MAX_ZOOM / MIN_ZOOM, value / 1000);
  const rowToSlider = (row) => Math.round(1000 * (row - MIN_ROW) / (MAX_ROW - MIN_ROW));
  const sliderToRow = (value) => MIN_ROW + (MAX_ROW - MIN_ROW) * value / 1000;

  /** Length zoom, the time under anchorX (the middle) staying put. */
  function setZoom(zoom, anchorX) {
    const box = grid();
    const anchor = anchorX === undefined ? box.clientWidth / 2 : anchorX;
    const time = (box.scrollLeft + anchor) / ed.zoom;
    ed.zoom = Hub.clamp(zoom, MIN_ZOOM, MAX_ZOOM);
    updateSize();
    box.scrollLeft = Math.max(0, time * ed.zoom - anchor);
    render();
  }

  /** Height zoom: taller or shorter rows, the pitch in the middle staying put. */
  function setRows(row) {
    const box = grid();
    const middle = (box.scrollTop + box.clientHeight / 2) / ed.row;
    ed.row = Hub.clamp(Math.round(row), MIN_ROW, MAX_ROW);
    updateSize();
    box.scrollTop = Math.max(0, middle * ed.row - box.clientHeight / 2);
    render();
  }

  function showZoom() {
    const length = $("editor-zoom"), height = $("editor-rows");
    if (document.activeElement !== length) length.value = String(zoomToSlider(ed.zoom));
    if (document.activeElement !== height) height.value = String(rowToSlider(ed.row));
  }

  Hub.commands.deleteNotes = deleteSelected;
  Hub.commands.selectAllNotes = () => {
    if (!isOpen()) return;
    for (const n of ed.notes) n.selected = true;
    render();
  };
  Hub.commands.deselectAllNotes = () => { if (isOpen()) deselectAll(); };

  // Deselect All acts on what you last worked with: notes in the Editor, or clips.
  Hub.commands.deselectAll = () => {
    if (isOpen() && Hub.state.focus === "editor") { deselectAll(); return; }
    // As the lanes' own Deselect All: the Range tool's selection goes with the clips.
    if (Hub.commands.deselectAllClips) { Hub.run("deselectAllClips"); return; }
    Hub.state.selectedClips = [];
    if (Hub.arrange) Hub.arrange.redraw();
  };
  Hub.commands.copyNotes = () => { if (isOpen()) copySelected(); };
  Hub.commands.cutNotes = () => { if (isOpen() && copySelected()) deleteSelected(); };
  Hub.commands.pasteNotes = () => { if (isOpen()) paste(pastePosition()); };
  Hub.commands.duplicateNotes = () => { if (isOpen()) duplicateSelected(); };

  // Edit > Cut, Copy, Paste and Duplicate (and their keys) act on notes while the Editor has focus.
  for (const [forClips, forNotes] of [["cutClips", "cutNotes"], ["copyClips", "copyNotes"], ["pasteClips", "pasteNotes"], ["duplicateClips", "duplicateNotes"]]) {
    const clipCommand = Hub.commands[forClips];
    Hub.commands[forClips] = () => {
      if (isOpen() && Hub.state.focus === "editor") Hub.commands[forNotes]();
      else if (clipCommand) clipCommand();
    };
  }

  // F2 (and the Editor button) opens the clip selected in the arrangement, if there is one.
  const toggleEditorPanel = Hub.commands.toggleEditor;
  Hub.commands.toggleEditor = () => {
    const opening = document.body.classList.contains("hide-editor");
    const chosen = Hub.state.selectedClip;
    // An audio clip opens in the Audio Editor, as a double-click on it does.
    const found = opening && chosen ? Hub.findClip(chosen.clipId) : null;
    if (found && found.clip.audioFile && Hub.openAudioEditor) { Hub.openAudioEditor(found.track.id, chosen.clipId); return; }
    toggleEditorPanel();
    if (opening && chosen && (chosen.trackId !== ed.trackId || chosen.clipId !== ed.clipId)) Hub.openEditor(chosen.trackId, chosen.clipId);
    else render();
  };

  // Delete acts on what you last worked with: notes in the Editor, or clips.
  Hub.commands.deleteSelection = () => {
    if (isOpen() && Hub.state.focus === "editor") deleteSelected();
    else Hub.run("deleteClip");
  };

  // ---------------------------------------------------------------- wiring
  Hub.on("tracks", () => {
    if (!ed.clipId) return;
    if (ed.drag || laneDrag) { ed.pendingReload = true; return; }
    load();
  });

  // A take recording into the clip open here: its notes as they come.
  Hub.on("takeGrew", (data) => {
    if (!ed.clipId || ed.drag || laneDrag) return;
    if (((data && data.tracks) || []).some((t) => (t.clips || []).some((c) => c.id === ed.clipId))) load();
  });

  Hub.on("status", () => {
    if (isOpen()) drawKeys();   // keys light up with the notes that sound
  });

  Hub.on("layout", () => requestAnimationFrame(render));
  Hub.on("transportEdited", render);
  Hub.on("tempoMap", () => { if (ed.clipId) { updateSize(); render(); } });

  function animate() {
    if (isOpen()) {
      const box = grid();
      // Autoscroll follows the playhead here too, when it's on.
      if (Hub.arrange && Hub.arrange.view.follow && Hub.transportInfo().playing && !ed.drag && box.clientWidth > 0) {
        const x = Hub.songPosition() * ed.zoom;
        if (x > box.scrollLeft + box.clientWidth * 0.95 || x < box.scrollLeft) box.scrollLeft = Math.max(0, x - box.clientWidth * 0.05);
      }
      autoscroll();
      placePlayhead();
    }
    requestAnimationFrame(animate);
  }

  Hub.on("ready", () => {
    const quantizeSelect = $("editor-quantize");
    for (const [label, value] of QUANTIZE) quantizeSelect.add(new Option(label, String(value), false, value === ed.quantize));

    quantizeSelect.addEventListener("change", () => { ed.quantize = Number(quantizeSelect.value); quantizeSelect.blur(); render(); });
    $("editor-snap").addEventListener("click", (event) => {
      ed.snap = !ed.snap;
      event.currentTarget.setAttribute("aria-pressed", String(ed.snap));
      event.currentTarget.title = ed.snap ? "Snap is on: notes land on the Quantize grid" : "Snap is off: notes go where you put them";
    });
    $("editor-velocity-value").addEventListener("change", (event) => {
      ed.velocity = Hub.clamp(Math.round(Number(event.target.value)) || ed.velocity, 1, 127);
      event.target.value = String(ed.velocity);
    });
    $("editor-audition").addEventListener("click", (event) => {
      ed.audition = !ed.audition;
      event.currentTarget.setAttribute("aria-pressed", String(ed.audition));
    });
    $("editor-quantize-now").addEventListener("click", quantize);
    setupOps();
    $("editor-close").addEventListener("click", () => Hub.run("toggleEditor"));
    document.querySelectorAll("#editor-toolbar [data-tool]").forEach((button) => button.addEventListener("click", () => setTool(button.dataset.tool)));
    document.querySelectorAll("#editor-lane-tabs [data-lane]").forEach((button) => button.addEventListener("click", () => setLane(button.dataset.lane)));

    // Zoom: length (time) and height (pitch), as sliders with - and + beside them.
    const lengthSlider = $("editor-zoom"), heightSlider = $("editor-rows");
    lengthSlider.addEventListener("input", () => setZoom(sliderToZoom(Number(lengthSlider.value))));
    heightSlider.addEventListener("input", () => setRows(sliderToRow(Number(heightSlider.value))));
    for (const slider of [lengthSlider, heightSlider]) slider.addEventListener("change", () => slider.blur());
    $("editor-zoom-out").addEventListener("click", () => setZoom(ed.zoom / 1.5));
    $("editor-zoom-in").addEventListener("click", () => setZoom(ed.zoom * 1.5));
    $("editor-rows-out").addEventListener("click", () => setRows(ed.row - 2));
    $("editor-rows-in").addEventListener("click", () => setRows(ed.row + 2));

    const box = grid();
    box.addEventListener("pointerdown", onPointerDown);
    box.addEventListener("pointermove", onPointerMove);
    box.addEventListener("pointerup", onPointerUp);
    box.addEventListener("pointercancel", onPointerUp);
    box.addEventListener("pointerenter", () => { ed.hoverInside = true; });
    box.addEventListener("pointerleave", () => { ed.hoverInside = false; });
    box.addEventListener("scroll", render);

    // Double-click empty space: a new note, as long as the quantize value. (The click before
    // it held the pointer, so the event comes to the grid rather than its canvas.)
    box.addEventListener("dblclick", (event) => {
      if (!ed.downOnNotes || (ed.tool !== "arrow" && ed.tool !== "pencil") || !ed.clipId || !overNotes(event) || drums()) return;
      const p = point(event);
      if (noteAt(p)) return;
      addNote(p);
      commit();
      render();
    });

    // Right-click: Studio One's menu (the tools, the edits, the selection, Undo and Redo).
    box.addEventListener("contextmenu", (event) => {
      event.preventDefault();
      if (!ed.clipId || !overNotes(event)) return;
      Hub.state.focus = "editor";
      showMenu(event);
    });

    // Ctrl + wheel zooms around the mouse.
    for (const area of [box, $("editor-ruler-wrap"), $("editor-lane-wrap")]) {
      area.addEventListener("wheel", (event) => {
        if (!event.ctrlKey) return;
        event.preventDefault();
        setZoom(ed.zoom * (event.deltaY < 0 ? 1.2 : 1 / 1.2), event.clientX - box.getBoundingClientRect().left);
      }, { passive: false });
    }

    // The keyboard: press a key to hear it, and drag along the keys to play each one passed (a
    // glissando), as on a real keyboard.
    const keys = $("editor-keys");
    let keyHeld = null, keyPointer = null;
    const keyAt = (event) => {
      const r = keys.getBoundingClientRect();
      return pitchOfRow(Math.floor((event.clientY - r.top + box.scrollTop) / ed.row));
    };
    const playKey = (pitch) => {
      if (pitch === keyHeld) return;
      if (keyHeld !== null) Hub.send("noteOff", { note: keyHeld });
      keyHeld = pitch;
      Hub.send("noteOn", { note: pitch, velocity: ed.velocity / 127 });
    };
    keys.addEventListener("pointerdown", (event) => {
      if (event.button !== 0) return;
      event.preventDefault();
      keyPointer = event.pointerId;
      if (keys.setPointerCapture) keys.setPointerCapture(event.pointerId);
      playKey(keyAt(event));
    });
    keys.addEventListener("pointermove", (event) => {
      if (event.pointerId !== keyPointer || keyHeld === null) return;
      playKey(keyAt(event));
    });
    const release = () => {
      keyPointer = null;
      if (keyHeld !== null) { Hub.send("noteOff", { note: keyHeld }); keyHeld = null; }
    };
    keys.addEventListener("pointerup", release);
    keys.addEventListener("pointercancel", release);
    keys.addEventListener("lostpointercapture", release);
    keys.addEventListener("wheel", (event) => { box.scrollTop += event.deltaY; event.preventDefault(); }, { passive: false });

    // The Drums view's names: double-click to rename, right-click for the rows' menu.
    keys.addEventListener("dblclick", (event) => { if (drums() && ed.clipId) renameRow(keyAt(event)); });
    keys.addEventListener("contextmenu", (event) => {
      event.preventDefault();
      if (drums() && ed.clipId) rowMenu(event, keyAt(event));
    });

    // The Piano | Drums switch, and the Drums view's + (a drum sound added as a row).
    document.querySelectorAll("#editor-toolbar [data-view]").forEach((button) => button.addEventListener("click", () => setView(button.dataset.view)));
    $("editor-add-row").addEventListener("click", (event) => {
      if (!drums() || !ed.clipId) return;
      Hub.popupMenu(event.currentTarget, addRowItems(), (pick) => {
        if (typeof pick === "string" && pick.startsWith("add:")) addRow(Number(pick.slice(4)));
      });
    });

    // The lane under the notes: velocities, or the pitch bend or aftertouch line.
    const lane = $("editor-lane");
    lane.addEventListener("pointerdown", (event) => {
      if (event.button !== 0 || !ed.clipId) return;
      event.preventDefault();
      if (lane.setPointerCapture) lane.setPointerCapture(event.pointerId);
      laneDrag = { pointer: event.pointerId, changed: false };
      if (BARS.has(ed.lane)) paintBars(event, true);
      else paintLine(event, true);
    });
    lane.addEventListener("pointermove", (event) => {
      if (laneDrag && event.pointerId === laneDrag.pointer) {
        if (BARS.has(ed.lane)) paintBars(event, false);
        else paintLine(event, false);
        return;
      }
      ed.hover = laneValue(event).value;
      $("editor-lane-label").textContent = laneText(ed.hover);
    });
    lane.addEventListener("pointerleave", () => {
      if (laneDrag) return;
      ed.hover = null;
      $("editor-lane-label").textContent = "";
    });
    const endLane = (event) => {
      if (!laneDrag || event.pointerId !== laneDrag.pointer) return;
      let changed = laneDrag.changed;
      if (!BARS.has(ed.lane)) {
        // Sent only when the stroke changed the line (an eraser click on nothing doesn't).
        const line = ed.lines[ed.lane] = thin(ed.lines[ed.lane], lineStart(ed.lane));
        const before = laneDrag.original || [];
        changed = line.length !== before.length || line.some((q, i) => q.time !== before[i].time || q.value !== before[i].value);
        if (changed) ed.edited[ed.lane] = true;
      }
      laneDrag = null;
      if (changed) commit(ed.lane === "chance" ? "Note Chance" : undefined);
      if (ed.pendingReload) {
        ed.pendingReload = false;
        load();
      }
      render();
    };
    lane.addEventListener("pointerup", endLane);
    lane.addEventListener("pointercancel", endLane);

    // Which area Delete, Ctrl+A and the arrows act on: the one last clicked.
    document.addEventListener("pointerdown", (event) => {
      if (event.target.closest("#editor")) Hub.state.focus = "editor";
      else if (event.target.closest("#workspace")) Hub.state.focus = "arrange";
    }, true);

    // The arrows, whatever keyboard map is in use (its other keys are in hub-keys.js).
    window.addEventListener("keydown", (event) => {
      if (!isOpen() || Hub.state.focus !== "editor" || Hub.isTyping(event.target) || event.altKey) return;
      if (event.ctrlKey || event.metaKey || document.querySelector(".modal:not([hidden])")) return;

      if (event.code === "ArrowUp" || event.code === "ArrowDown") {
        event.preventDefault();
        transpose((event.code === "ArrowUp" ? 1 : -1) * (event.shiftKey ? 12 : 1));
        return;
      }
      if (event.code === "ArrowLeft" || event.code === "ArrowRight") {
        event.preventDefault();
        nudge(event.code === "ArrowRight" ? 1 : -1);
      }
    });

    if (window.ResizeObserver) new ResizeObserver(render).observe(box);
    render();
    requestAnimationFrame(animate);
  });
})();
