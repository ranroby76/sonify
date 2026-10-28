// D:\workspace\Fanan_web_hub\src\hub-arrange.js
//
// The arrangement: ruler, track lanes and playhead, plus the Studio One toolbar
// settings that shape them (Quantize, Timebase, Snap, Autoscroll).
//
//   Zoom        Ctrl + mouse wheel (around the mouse), the slider under the lanes,
//               or Ctrl + = / Ctrl + - (Ctrl + 0 goes back to the default).
//   Height zoom every track taller or shorter at once (their own heights and the Track size
//               alike): Ctrl + Shift + mouse wheel (around the mouse), the slider under the
//               lanes with its - and +, or Shift + E / Shift + W (View > Zoom In / Out Vertically).
//   Ruler, top  the loop lane: drag to draw a loop, drag its ends or its middle to
//               change it, click it to switch looping on or off.
//   Ruler, rest click or drag to move the song position.
//   Snap        everything above lands on the grid Snap chooses (Adaptive, Quantize or Bar);
//               the magnet before it switches snapping off and on, keeping the grid.
//   Autoscroll  keeps the playhead in view while playing; scrolled away by hand, it waits until
//               the playhead is in view again (Track Options: Scrolling pauses Autoscroll).
//   Clips       drawn in their track's colour with their notes; the one being recorded
//               shows red. Double-click one to open it in the Editor.
//   Tools       Studio One's, from the toolbar, their keys or the right-click menu (on empty
//               space: the tools, Cut, Copy, Paste where it was clicked, Delete, Select All,
//               Deselect All, Undo and Redo, each with its key):
//     Arrow (1)   click, Shift/Ctrl-click or drag a box to select clips; drag them to move (to
//                 other tracks too; hold Alt to copy); drag an edge to resize.
//     Range (2)   drag over clips (across tracks too) for a stretch of time; double-click in it
//                 to cut the clips there at its start and its end (the part inside is selected).
//     Split (3)   click where to cut.
//     Eraser (4)  click or drag over clips.
//     Paint (5)   drag in an empty lane to draw an empty clip; in Automation view, draw the
//                 envelope, in its shape (the menu's Paint Tool: Freehand, Line, Parabola,
//                 Square, Triangle, Saw, Sine).
//     Mute (6)    click a clip (MIDI or audio) to mute it, or unmute it; drag over clips for
//                 each. A selected one does the whole selection. A muted clip shows pale.
//     Listen (8)  hold the mouse down on a track to hear it alone from there; let go and the
//                 song goes back where it was.
//   Audio clips have Studio One's handles: fade in and fade out at the top corners,
//               the clip's volume at the top middle (drag up and down; Shift for fine).
//   Automation  A (or the toolbar's A) shows each track's envelope over its clips, for the
//               target its header picks. Arrow: click to add a point, drag points (snapped;
//               Alt for free); Paint: draw; Eraser, double-click or Delete: remove.
//   Keys        as the keyboard map in use has them (hub-keys.js); in Studio One's: Delete,
//               Ctrl+X / C / V (paste at the playhead), D duplicates, Alt+X splits at the
//               playhead, Ctrl+A selects all clips, Alt+Z zooms to the whole song.
//   Look        the lanes follow Settings > Appearance > Luminance (Hub.theme).
//   Bars        the grid, the ruler and snapping follow the song channel's time signatures, and
//               the Seconds timebase and audio clips its tempo (Hub.tempoMap, hub-tempo.js).
// Trimming hides notes rather than deleting them; dragging the edge back shows them.

(() => {
  "use strict";

  const { $ } = Hub;
  // The view's track sizes (normal fits the instrument and MIDI selectors); a track dragged
  // to a height of its own keeps it.
  const SIZES = { tiny: 30, small: 56, normal: 136, large: 190 };

  /** A track's height in the arrangement: its own, or the view's size; 0 while the track filter
      hides it (hub-tracks.js). */
  const heightOf = (track) => (track && Hub.isTrackShown && !Hub.isTrackShown(track) ? 0 : shownHeight(track));

  /** A track's height as the height zoom shows it (its own, or the view's size, times the zoom). */
  const MIN_ROW = 24;                                         // the header's top row still fits
  const scaled = (height) => Math.max(MIN_ROW, Math.round(height * view.vzoom));
  const shownHeight = (track) => scaled(track && track.height > 0 ? track.height : SIZES[view.size]);

  /** Where a row starts, in content pixels (rows past the last continue at the view's size). */
  function rowTop(index) {
    const tracks = Hub.state.tracks;
    let top = 0;
    for (let i = 0; i < Math.min(index, tracks.length); i++) top += heightOf(tracks[i]);
    if (index > tracks.length) top += (index - tracks.length) * scaled(SIZES[view.size]);
    return top;
  }

  /** The row at a height in content pixels (past the last track, where rows would continue). */
  function rowAt(y) {
    const tracks = Hub.state.tracks;
    let top = 0;
    for (let i = 0; i < tracks.length; i++) {
      const next = top + heightOf(tracks[i]);
      if (y < next) return i;
      top = next;
    }
    return tracks.length + Math.max(0, Math.floor((y - top) / scaled(SIZES[view.size])));
  }

  const contentHeight = () => rowTop(Hub.state.tracks.length);

  /** One of the track list's Options (hub-trackoptions.js), or its default before that's there. */
  const option = (name, fallback) => (Hub.trackOption ? Hub.trackOption(name) : fallback);
  const MIN_ZOOM = 2, MAX_ZOOM = 960, DEFAULT_ZOOM = 56;    // pixels per quarter note
  const MIN_VZOOM = 0.3, MAX_VZOOM = 3;                       // the height zoom: times the tracks' heights
  const LOOP_LANE = 13;                                      // ruler rows above this are the loop lane
  const MAIN_RULER = 34;                                     // the ruler's height; the second ruler goes under it
  /** The second ruler's timebase, when it shows ("" when it doesn't). */
  const secondRuler = () => (view.ruler2 && view.ruler2 !== "off" ? view.ruler2 : "");
  const EDGE = 6;                                            // px within which a loop end can be grabbed
  const EMPTY_SPACE = 240;                                   // room below the tracks for dropping new ones
  const MIN_SONG_BARS = 200;
  const QUANTIZE = [["1/1", 4], ["1/2", 2], ["1/4", 1], ["1/8", 0.5], ["1/16", 0.25], ["1/32", 0.125], ["1/64", 0.0625]];
  const SECONDS_STEPS = [0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600];

  const view = { zoom: DEFAULT_ZOOM, vzoom: 1, snap: "adaptive", snapOn: true, quantize: 0.25, timebase: "bars", follow: true, size: "normal", tool: "arrow", automation: false,
                 shape: "freehand",
                 // The Snap menu's (Studio One's): what else snaps, besides the grid.
                 fps: 30, snapCursor: false, snapEvents: false, snapZero: false, snapGrid: true, snapEnd: false, relativeGrid: false,
                 // The Timebase menu's: the grid the lanes and the ruler show (Bars), and how the timeline runs.
                 grid: "adaptive", linear: "beat" };

  // The Timebase menu (Studio One's): what the ruler counts in; under Bars, the grid it shows.
  const TIMEBASES = [["seconds", "Seconds", "Minutes and seconds"], ["samples", "Samples", "Samples at the device's rate"],
                     ["bars", "Bars", "Bars and beats"], ["frames", "Frames", "Timecode: hours, minutes, seconds and frames"]];
  const GRIDS = [[["adaptive", "Adaptive"], ["quantize", "Quantize"]],
                 [[1, "1/4"], [0.5, "1/8"], [0.25, "1/16"], [0.125, "1/32"], [0.0625, "1/64"]],
                 [[2 / 3, "1/4 T"], [1 / 3, "1/8 T"], [1 / 6, "1/16 T"], [1 / 12, "1/32 T"]],
                 [[0.4, "1/8 5:4"], [0.2, "1/16 5:4"], [0.1, "1/32 5:4"]],
                 [[4 / 7, "1/8 7:8"], [2 / 7, "1/16 7:8"], [1 / 7, "1/32 7:8"]]];
  const sameGrid = (a, b) => (typeof a === "number" && typeof b === "number" ? Math.abs(a - b) < 1e-9 : a === b);

  // The Snap menu: the grid's kind, then what snaps.
  const SNAP_MODES = [["adaptive", "Adaptive", "The finest grid line on screen: finer as you zoom in"], ["bar", "Bar", "Whole bars"],
                      ["quantize", "Quantize", "The Quantize value's grid"], ["frames", "Frames", "Video frames, at the frame rate chosen"]];
  const SNAP_OPTIONS = [["snapCursor", "Snap To Cursor & Loop", "The song position and the loop's ends pull what you drag when it's near them"],
                        ["snapEvents", "Snap To Events", "Other clips' starts and ends pull what you drag when it's near them"],
                        ["snapZero", "Snap To Zero Crossings", "An audio clip's cut or edge moves to where its sound crosses zero, so it doesn't click"],
                        ["snapGrid", "Snap To Grid", "What you drag lands on the grid"],
                        ["snapEnd", "Snap Event End", "A clip moved lands with its end on the grid when that's nearer than its start"],
                        ["relativeGrid", "Relative Grid", "A clip moved keeps where it sits between grid lines: it moves by whole grid steps"]];
  const FRAME_RATES = [23.976, 24, 25, 29.97, 30, 50, 59.94, 60];
  const MAGNET_PX = 8;          // how near (pixels) the cursor, the loop or another clip pulls

  // The tools, as Studio One's toolbar and menu have them: [tool, its name, its key's command].
  const TOOLS = [["arrow", "Arrow Tool", "toolArrow"], ["range", "Range Tool", "toolRange"], ["split", "Split Tool", "toolSplit"],
                 ["eraser", "Eraser Tool", "toolEraser"], ["paint", "Paint Tool", "toolPaint"], ["mute", "Mute Tool", "toolMute"],
                 ["listen", "Listen Tool", "toolListen"]];
  const TOOL_ICONS = {
    arrow: '<svg viewBox="0 0 16 16"><path d="M4 1.5v12l3.2-3 2.1 4.6 1.8-.8-2.1-4.5H13z"/></svg>',
    range: '<svg viewBox="0 0 16 16"><path d="M2 2.5h3v1.4H3.4v8.2H5v1.4H2zM14 2.5h-3v1.4h1.6v8.2H11v1.4h3z"/><path d="M6 5h4v6H6z" opacity=".55"/></svg>',
    split: '<svg viewBox="0 0 16 16"><circle cx="4.5" cy="4.5" r="2.3" fill="none" stroke="currentColor" stroke-width="1.4"/><circle cx="4.5" cy="11.5" r="2.3" fill="none" stroke="currentColor" stroke-width="1.4"/><path d="M6.4 5.8 14 12.2l-.9 1-7.6-6.4zM6.4 10.2 14 3.8l-.9-1-7.6 6.4z"/></svg>',
    eraser: '<svg viewBox="0 0 16 16"><path d="M9.5 2 15 7.5 8.5 14H4L1 11z"/><path d="M5 14h10v1.2H5z"/></svg>',
    paint: '<svg viewBox="0 0 16 16"><path d="M11.3 1.7l3 3-8.6 8.6-3.7.7.7-3.7z"/></svg>',
    mute: '<svg viewBox="0 0 16 16"><path d="M1.5 6h2.8l3.7-3v10L4.3 10H1.5z"/><path d="M10 5.5l4.5 5M14.5 5.5l-4.5 5" stroke="currentColor" stroke-width="1.5" fill="none"/></svg>',
    listen: '<svg viewBox="0 0 16 16"><path d="M1.5 6h2.8l3.7-3v10L4.3 10H1.5z"/><path d="M10.2 5.3a3.6 3.6 0 0 1 0 5.4M12 3.6a6 6 0 0 1 0 8.8" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>'
  };
  // The Paint tool's shapes (for envelopes in Automation view), with their pictures.
  const SHAPES = [["freehand", "Freehand", '<svg viewBox="0 0 16 16"><path d="M11.3 1.7l3 3-8.6 8.6-3.7.7.7-3.7z"/></svg>'],
                  ["line", "Line", '<svg viewBox="0 0 16 16"><path d="M3 13 13 3" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>'],
                  ["parabola", "Parabola", '<svg viewBox="0 0 16 16"><path d="M3 13c5 0 8-3 10-10" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>'],
                  ["square", "Square", '<svg viewBox="0 0 16 16"><path d="M2 11h3V5h5v6h4" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>'],
                  ["triangle", "Triangle", '<svg viewBox="0 0 16 16"><path d="M1.5 11 5 5l3.5 6L12 5l2.5 4" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>'],
                  ["saw", "Saw", '<svg viewBox="0 0 16 16"><path d="M1.5 12 5 4v8l3.5-8v8L12 4v8" stroke="currentColor" stroke-width="1.3" fill="none"/></svg>'],
                  ["sine", "Sine", '<svg viewBox="0 0 16 16"><path d="M1.5 8c1.6-5 3.6-5 5.2 0s3.6 5 5.2 0 2.1-3 2.6-2.4" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>']];
  let rangeSel = null;          // the Range tool's: {top, bottom (track rows), from, to (song positions)}
  let rangeSelect = null;       // after a range is cut: the clips inside it to select when they arrive
  let listening = null;         // the Listen tool held down: {wasPlaying, from, trackId, soloed}
  let drag = null;              // what the ruler is doing while the mouse is down
  let clipDrag = null;          // what a press in the lanes is doing
  let splitHover = null;        // where the Split tool would cut: {index, ppq}
  let clipboard = null;         // copied clips: {items: [{indexOffset, startOffset, length, offset, notes}]}
  let clipIdsBefore = null;     // when an edit makes new clips, the ids that existed before it
  let recordStart = 0;          // where the current recording began (for the growing take)
  const takeWaves = new Map();  // an audio take's waveform as it's written: track id -> { key, ppq, peaks }
  let hoverClip = null;         // the audio clip under the pointer: its handles show
  let wasRecording = false;
  let loopPreview = null;       // the loop being drawn or changed, before the engine has it

  // ---------------------------------------------------------------- geometry
  const lanes = () => $("lanes");
  const map = () => Hub.tempoMap;

  function timing() {
    return { t: Hub.transportInfo() };
  }

  // The timeline's place for a song position: Beat-Linear, each quarter note as wide; Time-Linear,
  // each second as wide (a slower tempo, wider bars). Its unit is a quarter at the song's first tempo,
  // so with one tempo both are the same. Content pixels are places times the zoom.
  const timeLinear = () => view.linear === "time";
  const placeScale = () => map().bpmAt(0) / 60;
  const placeOf = (ppq) => (timeLinear() ? map().secondsAt(ppq) * placeScale() : ppq);
  const ppqOfPlace = (place) => (timeLinear() ? map().ppqAt(Math.max(0, place) / placeScale()) : place);

  /** Content pixels per quarter note at a song position (Time-Linear: fewer where the tempo is faster). */
  const pxPerPpq = (at) => (timeLinear() ? view.zoom * placeScale() * 60 / map().bpmAt(Math.max(0, at)) : view.zoom);

  const contentX = (ppq) => placeOf(ppq) * view.zoom;
  const ppqOfContentX = (x) => Math.max(0, ppqOfPlace(x / view.zoom));
  const xOf = (ppq) => contentX(ppq) - lanes().scrollLeft;
  const ppqAt = (clientX) => ppqOfContentX(clientX - lanes().getBoundingClientRect().left + lanes().scrollLeft);

  /** Where the view starts: the grid's steps are those of the bar there. */
  const viewStart = () => ppqOfContentX(lanes().scrollLeft);

  /** The finest grid step (no finer than finest quarters) that is still at least minPx wide at this zoom. */
  const gridStep = (minPx, finest = 0) => map().gridStep(viewStart(), pxPerPpq(viewStart()), minPx, finest);

  /** The grid Snap lands on near a position (null: snapping is off, or not to the grid). */
  function snapStep(at = viewStart()) {
    if (!view.snapOn || !view.snapGrid) return null;
    switch (view.snap) {
      case "bar":      return { q: map().barAt(at).length, bars: 1 };
      case "quantize": return map().stepOf(view.quantize, at);
      case "frames":   return { q: Math.max(1e-6, map().bpmAt(at) / 60 / view.fps), frames: true };
      default:         return laneGrid(12);   // Adaptive: the grid on screen (the finest line, or the Timebase menu's)
    }
  }

  /** The grid the lanes and the ruler show: the finest line on screen (Adaptive), or the Timebase
      menu's (Quantize: the Quantize value's), the finest line instead when that's too dense to see. */
  function laneGrid(minPx = 10) {
    if (view.grid === "adaptive") return gridStep(minPx);
    const at = viewStart();
    const q = view.grid === "quantize" ? view.quantize : Number(view.grid);
    return q > 0 && q * pxPerPpq(at) >= 4 ? map().stepOf(q, at) : gridStep(minPx);
  }

  /** The nearest grid position (frames: the nearest frame of the song's time). */
  function gridSnap(ppq) {
    const step = snapStep(ppq);
    if (!step) return ppq;
    if (step.frames) return map().ppqAt(Math.round(map().secondsAt(ppq) * view.fps) / view.fps);
    return map().snap(ppq, step);
  }

  /** Where a position snaps to: the song position, the loop's ends or another clip's edge when it's
      that near (exclude: the clips being dragged), else the grid. {at, magnetic}. */
  function snapPoint(ppq, exclude) {
    if (!view.snapOn) return { at: ppq, magnetic: false };
    const reach = MAGNET_PX / pxPerPpq(ppq);
    let best = null, distance = reach;
    const consider = (target) => {
      const d = Math.abs(target - ppq);
      if (d <= distance) { best = target; distance = d; }
    };
    if (view.snapCursor) {
      const t = Hub.transportInfo();
      consider(Hub.songPosition());
      if (t && t.loopEnd > t.loopStart) { consider(t.loopStart); consider(t.loopEnd); }
    }
    if (view.snapEvents) {
      for (const track of Hub.state.tracks) {
        for (const clip of track.clips || []) {
          if (exclude && exclude.has(clip.id)) continue;
          consider(clip.start);
          consider(clip.start + clip.length);
        }
      }
    }
    if (best !== null) return { at: best, magnetic: true };
    return { at: gridSnap(ppq), magnetic: false };
  }

  /** The nearest place Snap allows. */
  const snap = (ppq, exclude) => snapPoint(ppq, exclude).at;

  /** The snap grid's step in quarter notes (0: off). */
  const snapLength = () => { const step = snapStep(); return step ? step.q : 0; };

  /** How far moved clips go: the grabbed one's start (or its end, Snap Event End) lands where Snap
      allows; Relative Grid moves it by whole grid steps instead, keeping where it sits between lines. */
  function moveDelta(anchor, raw, exclude) {
    if (!view.snapOn) return raw;
    const fromStart = snapPoint(anchor.start + raw, exclude);
    let best = { delta: fromStart.at - anchor.start, magnetic: fromStart.magnetic };
    if (view.snapEnd) {
      const end = anchor.start + anchor.length;
      const fromEnd = snapPoint(end + raw, exclude);
      const delta = fromEnd.at - end;
      if (Math.abs(delta - raw) < Math.abs(best.delta - raw)) best = { delta, magnetic: fromEnd.magnetic };
    }
    if (view.relativeGrid && !best.magnetic) {
      const step = snapStep(anchor.start);
      if (step && step.q > 0) return Math.round(raw / step.q) * step.q;   // whole steps of the grid where the clip starts
    }
    return best.delta;
  }

  // ---------------------------------------------------------------- the Timebase menu
  function showTimebase() {
    const label = $("timebase-mode");
    if (!label) return;
    const base = TIMEBASES.find(([value]) => value === view.timebase);
    label.textContent = view.timebase === "frames" ? `Frames ${+view.fps.toFixed(3)}` : base ? base[1] : "Bars";
    const grid = GRIDS.flat().find(([value]) => sameGrid(value, view.grid));
    $("timebase-menu").title = `Timebase: ${base ? base[1] : "Bars"}${view.timebase === "bars" && grid ? ` (grid ${grid[1]})` : ""}, ${view.linear === "time" ? "Time-Linear" : "Beat-Linear"}`;
  }

  function openTimebaseMenu() {
    const gridItems = [];
    GRIDS.forEach((group, index) => {
      if (index) gridItems.push({ separator: true });
      for (const [value, label] of group)
        gridItems.push({ label, value: "grid:" + value, checked: sameGrid(value, view.grid),
                         title: value === "adaptive" ? "The finest grid line that fits: finer as you zoom in" : value === "quantize" ? "The Quantize value's grid" : undefined });
    });
    const items = TIMEBASES.map(([value, label, title]) => ({
      label, value: "base:" + value, checked: view.timebase === value, title,
      submenu: value === "bars" ? gridItems
             : value === "frames" ? FRAME_RATES.map((fps) => ({ label: `${fps} fps`, value: "fps:" + fps, checked: view.timebase === "frames" && view.fps === fps }))
             : undefined
    }));
    items.push({ separator: true },
      { label: "Time-Linear", value: "linear:time", checked: view.linear === "time", title: "Every second as wide: where the tempo is slower, bars are wider" },
      { label: "Beat-Linear", value: "linear:beat", checked: view.linear !== "time", title: "Every beat as wide, whatever the tempo" });
    $("timebase-menu").setAttribute("aria-expanded", "true");
    Hub.popupMenu($("timebase-menu"), items, (pick) => {
      $("timebase-menu").setAttribute("aria-expanded", "false");
      if (typeof pick !== "string") return;
      if (pick.startsWith("base:")) view.timebase = pick.slice(5);
      else if (pick.startsWith("fps:")) { view.fps = Number(pick.slice(4)); view.timebase = "frames"; }
      else if (pick.startsWith("grid:")) {
        const value = pick.slice(5);
        view.grid = value === "adaptive" || value === "quantize" ? value : Number(value);
        view.timebase = "bars";
      } else if (pick.startsWith("linear:")) setLinear(pick.slice(7));
      showTimebase();
      showSnapMode();
      updateSizes();
      Hub.emit("viewChanged");
    });
    setTimeout(() => { if (!document.querySelector(".popup-menu")) $("timebase-menu").setAttribute("aria-expanded", "false"); }, 0);
  }

  /** Time-Linear or Beat-Linear, the song position under the middle of the view staying where it is. */
  function setLinear(mode) {
    const box = lanes();
    const middle = box.clientWidth / 2;
    const ppq = ppqOfContentX(box.scrollLeft + middle);
    view.linear = mode === "time" ? "time" : "beat";
    updateSizes();
    box.scrollLeft = Math.max(0, contentX(ppq) - middle);
  }

  // ---------------------------------------------------------------- the Snap menu
  function snapMenuLabel() {
    const mode = SNAP_MODES.find(([value]) => value === view.snap);
    return mode ? mode[1] : "Adaptive";
  }

  function showSnapMode() {
    const label = $("snap-mode");
    if (!label) return;
    label.textContent = view.snap === "frames" ? `Frames ${+view.fps.toFixed(3)}` : snapMenuLabel();
    $("snap-menu").title = `Snap: ${SNAP_OPTIONS.filter(([key]) => view[key]).map(([, name]) => name.replace("Snap To ", "")).join(", ") || "nothing (all off)"}`;
  }

  function openSnapMenu() {
    const items = SNAP_MODES.map(([value, label, title]) => ({
      label, value: "mode:" + value, checked: view.snap === value, title,
      submenu: value === "frames" ? FRAME_RATES.map((fps) => ({ label: `${fps} fps`, value: "fps:" + fps, checked: view.snap === "frames" && view.fps === fps })) : undefined
    }));
    items.push({ separator: true });
    for (const [key, label, title] of SNAP_OPTIONS) items.push({ label, value: "option:" + key, checked: Boolean(view[key]), title });
    $("snap-menu").setAttribute("aria-expanded", "true");
    Hub.popupMenu($("snap-menu"), items, (pick) => {
      $("snap-menu").setAttribute("aria-expanded", "false");
      if (typeof pick !== "string") return;
      if (pick.startsWith("mode:")) view.snap = pick.slice(5);
      else if (pick.startsWith("fps:")) { view.fps = Number(pick.slice(4)); view.snap = "frames"; }
      else if (pick.startsWith("option:")) { const key = pick.slice(7); view[key] = !view[key]; }
      if (!view.snapOn && !pick.startsWith("option:")) setSnapOn(true);   // choosing a grid turns Snap on
      showSnapMode();
      drawArrangement();
      Hub.emit("viewChanged");
    });
    setTimeout(() => { if (!document.querySelector(".popup-menu")) $("snap-menu").setAttribute("aria-expanded", "false"); }, 0);
  }

  /** Seconds of song between two positions, and the position a time after one. */
  const secondsBetween = (from, to) => map().secondsAt(to) - map().secondsAt(from);
  const ppqAfter = (from, seconds) => map().ppqAt(map().secondsAt(from) + seconds);

  function trackAt(clientY) {
    const index = rowAt(clientY - lanes().getBoundingClientRect().top + lanes().scrollTop);
    return Hub.state.tracks[index] || null;
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

  function loopShown() {
    const { t } = timing();
    return loopPreview || { start: t.loopStart, end: t.loopEnd, on: t.loop };
  }

  /** "5", "5.2" or "5.2.3" for a position, as fine as the label step needs. */
  const barLabel = (ppq, step) => map().label(ppq, step.q);

  function timeLabel(seconds, step) {
    const minutes = Math.floor(seconds / 60 + 1e-9);
    const rest = seconds - minutes * 60;
    const digits = step >= 1 ? 0 : step >= 0.1 ? 1 : 2;
    const text = rest.toFixed(digits).padStart(digits ? digits + 3 : 2, "0");
    return `${minutes}:${text}`;
  }

  /** Samples at the device's rate ("1,323,000"). */
  function sampleLabel(seconds) {
    const rate = (Hub.state.status && Hub.state.status.sampleRate) || 48000;
    return Math.round(seconds * rate).toLocaleString("en-US");
  }

  /** Timecode at the frame rate chosen: "00:01:23:12". */
  function frameLabel(seconds) {
    const perSecond = Math.round(view.fps);
    const frames = Math.round(seconds * view.fps);
    const whole = Math.floor(frames / perSecond), f = frames - whole * perSecond;
    const two = (n) => String(n).padStart(2, "0");
    return `${two(Math.floor(whole / 3600))}:${two(Math.floor(whole / 60) % 60)}:${two(whole % 60)}:${two(f)}`;
  }

  function drawRuler() {
    const canvas = $("ruler");
    const width = canvas.parentElement.clientWidth;
    const height = canvas.parentElement.clientHeight;
    const context = sizeCanvas(canvas, width, height);
    if (!context) return;

    const scroll = lanes().scrollLeft;

    context.fillStyle = "#1d1e21";
    context.fillRect(0, 0, width, height);
    context.fillStyle = "#18191b";
    context.fillRect(0, 0, width, LOOP_LANE);

    // Loop lane: the locators' line, in the leading colour (brighter while the loop is on), with grab handles at both ends.
    // With Autopunch on, it's the punch range too: red, as Studio One shows it.
    const loop = loopShown();
    const punch = Boolean(Hub.recordOptions && Hub.recordOptions().autopunch);
    const x0 = xOf(loop.start), x1 = xOf(loop.end);
    context.fillStyle = punch ? (loop.on ? "#b3363d" : "#82292f") : loop.on ? Hub.theme.accent : Hub.theme.accentSet.dim;
    context.fillRect(x0, 2, Math.max(1, x1 - x0), LOOP_LANE - 4);
    context.fillStyle = punch ? "#f2a3a8" : loop.on ? Hub.theme.accentSet.pale : Hub.theme.accentSet.soft;
    context.fillRect(x0, 1, 2, LOOP_LANE - 2);
    context.fillRect(x1 - 2, 1, 2, LOOP_LANE - 2);

    drawScale(context, view.timebase, LOOP_LANE + 3, MAIN_RULER, width, scroll);

    // The second ruler under it, in a timebase of its own (the track list header's toggle).
    if (secondRuler()) {
      context.fillStyle = "#1a1b1e";
      context.fillRect(0, MAIN_RULER, width, height - MAIN_RULER);
      drawScale(context, view.ruler2, MAIN_RULER + 3, height, width, scroll);
      context.fillStyle = Hub.theme.line;
      context.fillRect(0, MAIN_RULER - 1, width, 1);
      // Its timebase's name, at the left.
      const name = (TIMEBASES.find(([value]) => value === view.ruler2) || [, ""])[1];
      context.font = "10px Segoe UI, system-ui, sans-serif";
      const w = context.measureText(name).width + 8;
      context.fillStyle = "rgba(26, 27, 30, 0.92)";
      context.fillRect(0, MAIN_RULER, w + 2, height - MAIN_RULER - 1);
      context.fillStyle = Hub.theme.muted;
      context.fillText(name, 4, MAIN_RULER + 4);
    }

    context.fillStyle = Hub.theme.line;
    context.fillRect(0, height - 1, width, 1);
    context.fillRect(0, LOOP_LANE, width, 1);
  }

  /** A ruler's marks and labels in a timebase, from labelTop down to bottom. */
  function drawScale(context, timebase, labelTop, bottom, width, scroll) {
    context.font = "11px Segoe UI, system-ui, sans-serif";
    context.textBaseline = "top";

    if (timebase !== "bars") {
      // Time (Seconds, Samples, Frames) along the song: wider where the tempo is slower (Beat-Linear).
      const from = ppqOfContentX(scroll), to = ppqOfContentX(scroll + width);
      const firstSecond = map().secondsAt(from), lastSecond = map().secondsAt(to);
      const pxPerSecond = width / Math.max(1e-6, lastSecond - firstSecond);
      const frame = 1 / view.fps;
      const steps = timebase === "frames" ? [1, 2, 5, 10].map((f) => f * frame).concat(SECONDS_STEPS.filter((s) => s >= 1)) : SECONDS_STEPS;
      const step = steps.find((s) => s * pxPerSecond >= (timebase === "seconds" ? 64 : 92)) || steps[steps.length - 1];
      const ticksPerStep = timebase === "frames" && step < 1 ? Math.max(1, Math.round(step / frame)) : 5;
      const tick = step / ticksPerStep;
      const first = Math.floor(firstSecond / tick);
      const last = Math.ceil(lastSecond / tick);
      const label = (seconds) => timebase === "samples" ? sampleLabel(seconds) : timebase === "frames" ? frameLabel(seconds) : timeLabel(seconds, step);

      for (let i = first; i <= last; i++) {
        const seconds = i * tick;
        const x = Math.round(xOf(map().ppqAt(seconds))) + 0.5;
        const major = i % ticksPerStep === 0;
        context.strokeStyle = major ? Hub.theme.muted : Hub.theme.tick;
        context.beginPath();
        context.moveTo(x, major ? labelTop - 2 : bottom - 6);
        context.lineTo(x, bottom);
        context.stroke();
        if (major) {
          context.fillStyle = Hub.theme.text;
          context.fillText(label(seconds), x + 3, labelTop);
        }
      }
    } else {
      const tickStep = laneGrid(8);
      const labelStep = gridStep(48, 0.25);
      const from = ppqOfContentX(scroll), to = ppqOfContentX(scroll + width) + 1e-6;
      const labelledAt = new Set(map().lines(from, to, labelStep).map((line) => line.ppq.toFixed(6)));

      for (const line of map().lines(from, to, tickStep)) {
        const ppq = line.ppq;
        const x = Math.round(xOf(ppq)) + 0.5;
        const onBar = line.bar;
        const labelled = labelledAt.has(ppq.toFixed(6));
        context.strokeStyle = onBar ? Hub.theme.muted : Hub.theme.tick;
        context.beginPath();
        context.moveTo(x, onBar || labelled ? labelTop - 2 : bottom - 6);
        context.lineTo(x, bottom);
        context.stroke();
        if (labelled) {
          context.fillStyle = onBar ? Hub.theme.text : Hub.theme.muted;
          context.fillText(barLabel(ppq, labelStep), x + 3, labelTop);
        }
      }
    }

  }

  function drawLanes() {
    const box = lanes();
    const canvas = $("lanes-canvas");
    const width = box.clientWidth;
    const height = box.clientHeight;
    const context = sizeCanvas(canvas, width, height);
    if (!context) return;

    const scrollX = box.scrollLeft, scrollY = box.scrollTop;

    // Each row's top, once per drawing: tracks can have heights of their own.
    const tops = [];
    let running = 0;
    for (const track of Hub.state.tracks) { tops.push(running); running += heightOf(track); }

    const colours = Hub.theme.lanes;
    context.fillStyle = colours.empty;
    context.fillRect(0, 0, width, height);

    Hub.state.tracks.forEach((track, index) => {
      const y = tops[index] - scrollY, trackHeight = heightOf(track);
      if (!trackHeight || y > height || y + trackHeight < 0) return;
      if (track.recordArm) {
        context.fillStyle = colours.armed;      // an armed track's lane is tinted red, as in Studio One
        context.fillRect(0, y, width, trackHeight - 1);
        return;
      }
      context.fillStyle = Hub.isTrackSelected(track.id) ? colours.selected : colours.lane;   // the selected lanes, lighter
      context.fillRect(0, y, width, trackHeight - 1);
    });

    const loop = loopShown();
    if (loop.on) {
      context.fillStyle = Hub.accentRgba(0.08);
      context.fillRect(xOf(loop.start), 0, xOf(loop.end) - xOf(loop.start), height);
    }

    const fine = laneGrid(10);

    for (const line of map().lines(ppqOfContentX(scrollX), ppqOfContentX(scrollX + width) + 1e-6, fine)) {
      const x = Math.round(xOf(line.ppq)) + 0.5;
      context.strokeStyle = line.bar ? colours.bar : line.beat ? colours.beat : colours.fine;
      context.beginPath();
      context.moveTo(x, 0);
      context.lineTo(x, height);
      context.stroke();
    }

    // Clips go over the grid, as they are or as the drag in progress would leave them.
    const d = clipDrag;
    const picked = new Set(Hub.state.selectedClips);
    const moving = d && d.kind === "move" && d.moved;

    Hub.state.tracks.forEach((track, index) => {
      const laneTop = tops[index] - scrollY, trackHeight = heightOf(track);
      if (!trackHeight || laneTop > height || laneTop + trackHeight < 0) return;

      for (const clip of track.clips || []) {
        if (d && d.kind === "erase" && d.ids.has(clip.id)) continue;
        if (moving && !d.copy && picked.has(clip.id)) continue;       // drawn where it's going instead
        const shape = d && d.kind === "resize" && picked.has(clip.id) ? resizedShape(clip, d) : clip;
        drawClip(context, track, clip, shape, laneTop, trackHeight, width, picked.has(clip.id), 1);
      }

      // Frozen: its lane under frost (its render plays; its clips stay as they are until it thaws).
      if (track.frozen) {
        context.fillStyle = "rgba(160, 215, 255, 0.16)";
        context.fillRect(0, laneTop, width, trackHeight - 1);
      }
    });

    if (moving) {
      for (const c of d.clips) {
        const track = Hub.state.tracks[c.index + d.dTracks];
        const source = Hub.findClip(c.id);
        if (!track || !source) continue;
        const shape = { start: c.start + d.dPpq, length: c.length, offset: source.clip.offset || 0, audioOffset: source.clip.audioOffset || 0 };
        drawClip(context, track, source.clip, shape, rowTop(c.index + d.dTracks) - scrollY, heightOf(track), width, true, d.copy ? 0.7 : 1);
      }
    }

    if (d && d.kind === "paint") {
      const start = Math.min(d.from, d.to), end = Math.max(d.from, d.to);
      context.strokeStyle = colours.mark;
      context.setLineDash([4, 3]);
      const paintHeight = heightOf(Hub.state.tracks[d.index]);
      context.strokeRect(xOf(start) + 0.5, rowTop(d.index) - scrollY + 2.5, Math.max(2, xOf(end) - xOf(start)) - 1, paintHeight - 6);
      context.setLineDash([]);
    }

    if (d && d.kind === "box") {
      const x = Math.min(d.from.x, d.to.x) - scrollX, y = Math.min(d.from.y, d.to.y) - scrollY;
      const w = Math.abs(d.to.x - d.from.x), h = Math.abs(d.to.y - d.from.y);
      context.fillStyle = Hub.accentRgba(0.15);
      context.fillRect(x, y, w, h);
      context.strokeStyle = Hub.theme.accent;
      context.strokeRect(x + 0.5, y + 0.5, w, h);
    }

    // The Range tool's stretch of time, over the tracks it spans.
    if (rangeSel) {
      const x0 = Math.round(xOf(rangeSel.from)), x1 = Math.round(xOf(rangeSel.to));
      const y0 = rowTop(rangeSel.top) - scrollY, y1 = rowTop(rangeSel.bottom + 1) - scrollY;
      context.fillStyle = Hub.accentRgba(0.2);
      context.fillRect(x0, y0, x1 - x0, y1 - y0);
      context.strokeStyle = Hub.accentLine();
      context.strokeRect(x0 + 0.5, y0 + 0.5, Math.max(1, x1 - x0 - 1), Math.max(1, y1 - y0 - 1));
    }

    if (view.automation) drawEnvelopes(context, width, height, scrollY);
    else drawClipEnvelope(context, width, height, scrollY);

    // Recording: armed audio tracks show the take growing in red, as in Studio One, with its
    // waveform drawn as it's written (takeWave).
    const t = Hub.transportInfo();
    if (t.recording && t.playing) {
      Hub.state.tracks.forEach((track, index) => {
        if (track.kind !== "audio" || !track.recordArm || !heightOf(track)) return;
        const wave = takeWaves.get(track.id);
        const start = wave ? wave.ppq : recordStart;
        const from = xOf(start), to = xOf(Hub.songPosition());
        if (to <= from) return;
        const top = tops[index] - scrollY + 2, height = heightOf(track) - 5;
        context.fillStyle = "rgba(229, 72, 77, 0.35)";
        context.fillRect(from, top, to - from, height);
        if (wave && wave.peaks.length) {
          context.save();
          drawWaveform(context, { audioFile: wave.key, gain: 0 }, { start, length: Hub.songPosition() - start, audioOffset: 0 },
                       from, to, top + 2, height - 4, width);
          context.restore();
        }
      });
    }

    if (splitHover && view.tool === "split" && !d) {
      const x = Math.round(xOf(splitHover.ppq)) + 0.5;
      context.strokeStyle = colours.mark;
      context.beginPath();
      context.moveTo(x, rowTop(splitHover.index) - scrollY + 2);
      context.lineTo(x, rowTop(splitHover.index + 1) - scrollY - 3);
      context.stroke();
    }
  }

  /**
      A clip as Studio One draws a part: tinted body, header with the name, the notes it
      shows. shape is where it's drawn ({start, length, offset}): the clip itself, or
      where a drag would put it.
  */
  /** A clip muted, as it is or as a Mute tool drag is leaving it. */
  const isMuted = (clip) => (clipDrag && clipDrag.kind === "mute" && clipDrag.ids.has(clip.id) ? clipDrag.state : Boolean(clip.muted));

  // Clips have rounded corners, so where one ends and the next begins shows even when they touch.
  const CLIP_RADIUS = 5;

  /** A clip's outline as the path: a rectangle with rounded corners (less round when it's tiny). */
  function clipPath(context, x, y, w, h) {
    const r = Math.max(0, Math.min(CLIP_RADIUS, w / 2, h / 2));
    context.beginPath();
    if (context.roundRect) { context.roundRect(x, y, w, h, r); return; }
    context.moveTo(x + r, y);
    context.arcTo(x + w, y, x + w, y + h, r);
    context.arcTo(x + w, y + h, x, y + h, r);
    context.arcTo(x, y + h, x, y, r);
    context.arcTo(x, y, x + w, y, r);
    context.closePath();
  }

  /** The clip's border (white while selected). */
  function strokeClip(context, x0, x1, top, height, selected) {
    context.strokeStyle = selected ? "#ffffff" : "rgba(0, 0, 0, 0.55)";
    context.lineWidth = selected ? 2 : 1;
    clipPath(context, x0 + 0.5, top + 0.5, x1 - x0 - 1, height - 1);
    context.stroke();
  }

  function drawClip(context, track, clip, shape, laneTop, laneHeight, width, selected, alpha) {
    const x0 = xOf(shape.start), x1 = xOf(shape.start + shape.length);
    if (x1 < 0 || x0 > width || x1 - x0 < 1) return;

    // A muted clip, as Studio One shows it: pale, its name and notes faded.
    if (isMuted(clip)) {
      drawClipBody(context, track, clip, shape, laneTop, laneHeight, width, selected, alpha * 0.55);
      const top = laneTop + 2, height = laneHeight - 5;
      context.fillStyle = "rgba(255, 255, 255, 0.24)";
      clipPath(context, x0, top, x1 - x0, height);
      context.fill();
      return;
    }
    drawClipBody(context, track, clip, shape, laneTop, laneHeight, width, selected, alpha);
  }

  function drawClipBody(context, track, clip, shape, laneTop, laneHeight, width, selected, alpha) {
    const x0 = xOf(shape.start), x1 = xOf(shape.start + shape.length);

    const top = laneTop + 2, height = laneHeight - 5;
    const header = laneHeight < 70 ? 11 : 14;
    const colour = clip.recording ? "#e5484d" : track.colour;
    const offset = shape.offset || 0;

    context.save();
    clipPath(context, x0, top, x1 - x0, height);
    context.clip();

    context.globalAlpha = 0.42 * alpha;
    context.fillStyle = colour;
    context.fillRect(x0, top, x1 - x0, height);
    context.globalAlpha = alpha;
    context.fillRect(x0, top, x1 - x0, header);

    context.fillStyle = "#15161a";
    context.font = `${header - 3}px Segoe UI, system-ui, sans-serif`;
    context.textBaseline = "top";
    if (option("showClipNames", true))
      context.fillText(clip.audioFile ? (clip.missing ? `${clip.fileName} (missing)` : clip.fileName) : track.name, Math.max(x0, 0) + 4, top + 2);

    if (clip.audioFile) {
      drawWaveform(context, clip, shape, x0, x1, top + header, height - header, width);
      drawSilence(context, clip, shape, x0, x1, top + header, height - header);
      drawTransients(context, clip, shape, x0, x1, top + header, height - header);
      drawFades(context, clip, shape, x0, x1, top + header, height - header, selected || hoverClip === clip.id);
      drawClipIcons(context, clip, shape, laneTop, laneHeight, alpha);
      context.restore();
      context.globalAlpha = alpha;
      strokeClip(context, x0, x1, top, height, selected);
      // An effect from the Browser about to land on it: the clip's own.
      if (Hub.effectDropClip && Hub.effectDropClip() === clip.id) {
        context.strokeStyle = Hub.accentLine();
        context.lineWidth = 3;
        clipPath(context, x0 + 1.5, top + 1.5, x1 - x0 - 3, height - 3);
        context.stroke();
      }
      context.lineWidth = 1;
      context.globalAlpha = 1;
      return;
    }

    // The notes it shows, scaled to their own pitch range (at least an octave).
    const notes = (clip.notes || []).filter(([, start]) => start >= offset && start < offset + shape.length);

    if (notes.length) {
      let lo = 127, hi = 0;
      for (const [pitch] of notes) { lo = Math.min(lo, pitch); hi = Math.max(hi, pitch); }
      const middle = (lo + hi) / 2;
      lo = Math.min(lo, Math.floor(middle - 6)) - 1;
      hi = Math.max(hi, Math.ceil(middle + 6)) + 1;

      const areaTop = top + header + 3, areaHeight = height - header - 6;
      const noteHeight = Hub.clamp(areaHeight / (hi - lo + 1), 1.5, 4);
      context.fillStyle = "rgba(18, 19, 22, 0.85)";

      // A muted note (the Editor's Mute tool) only as its outline.
      for (const [pitch, start, length, , muted] of notes) {
        const nx = xOf(shape.start + start - offset);
        if (nx > x1) break;
        const ny = areaTop + (hi - pitch) / (hi - lo) * (areaHeight - noteHeight);
        if (muted) {
          context.strokeStyle = "rgba(18, 19, 22, 0.6)";
          context.strokeRect(nx + 0.5, ny + 0.5, Math.max(1.5, xOf(shape.start + start - offset + length) - nx) - 1, Math.max(1, noteHeight - 1));
        } else {
          context.fillRect(nx, ny, Math.max(1.5, xOf(shape.start + start - offset + length) - nx), noteHeight);
        }
      }
    }

    drawClipIcons(context, clip, shape, laneTop, laneHeight, alpha);
    context.restore();

    context.globalAlpha = alpha;
    strokeClip(context, x0, x1, top, height, selected);
    context.lineWidth = 1;
    context.globalAlpha = 1;
  }

  // ---------------------------------------------------------------- a clip's own effect
  // An audio clip with an effect of its own shows FX at its bottom-left corner; a press there opens
  // its menu: Edit Effect (its window), Remove Effect.
  // Silence Areas show a second icon beside it (hub-audioeditor.js has their window).
  const ICON = { width: 22, height: 15, margin: 4, gap: 3 };

  /** A clip's icons at its bottom-left corner, in the canvas's pixels: [{kind: "fx" | "silence", x, y,
      width, height}] (none when there's no room). */
  function clipIcons(clip, shape, laneTop, laneHeight) {
    const automated = (clip.envelopes || []).some((e) => e.points && e.points.length);
    const kinds = [...(clip.audioFile && clip.effect ? ["fx"] : []), ...(clip.audioFile && clip.silence ? ["silence"] : []), ...(automated ? ["auto"] : [])];
    if (!kinds.length) return [];
    const x0 = xOf(shape.start), x1 = xOf(shape.start + shape.length);
    const top = laneTop + 2, height = laneHeight - 5;
    const header = laneHeight < 70 ? 11 : 14;
    if (height - header < ICON.height + ICON.margin * 2) return [];
    const y = top + height - ICON.margin - ICON.height;
    const icons = [];
    let x = Math.max(x0, 0) + ICON.margin;
    for (const kind of kinds) {
      if (x + ICON.width > x1 - 2) break;
      icons.push({ kind, x, y, width: ICON.width, height: ICON.height });
      x += ICON.width + ICON.gap;
    }
    return icons;
  }

  function drawClipIcons(context, clip, shape, laneTop, laneHeight, alpha) {
    for (const r of clipIcons(clip, shape, laneTop, laneHeight)) {
      const broken = r.kind === "fx" && Boolean(clip.effect.error);
      context.save();
      context.globalAlpha = alpha;
      context.fillStyle = broken ? "rgba(229, 72, 77, 0.92)" : "rgba(21, 22, 26, 0.78)";
      context.beginPath();
      if (context.roundRect) context.roundRect(r.x, r.y, r.width, r.height, 3); else context.rect(r.x, r.y, r.width, r.height);
      context.fill();
      context.strokeStyle = broken ? "#ffb3b5" : "rgba(255, 255, 255, 0.55)";
      context.lineWidth = 1;
      context.stroke();
      context.fillStyle = "#ffffff";
      if (r.kind === "fx") {
        context.font = "bold 9px Segoe UI, system-ui, sans-serif";
        context.textAlign = "center";
        context.textBaseline = "middle";
        context.fillText("FX", r.x + r.width / 2, r.y + r.height / 2 + 0.5);
      } else if (r.kind === "auto") {
        // Automation: a little line with its points.
        context.strokeStyle = "#ffd166";
        context.lineWidth = 1.3;
        context.beginPath();
        context.moveTo(r.x + 4, r.y + r.height - 4);
        context.lineTo(r.x + 9, r.y + 5);
        context.lineTo(r.x + 14, r.y + 9);
        context.lineTo(r.x + 18, r.y + 4);
        context.stroke();
        context.fillStyle = "#ffd166";
        for (const [dx, dy] of [[9, 5], [14, 9]]) context.fillRect(r.x + dx - 1.5, r.y + dy - 1.5, 3, 3);
      } else {
        // Silence: a little sound, then a flat line.
        const mid = r.y + r.height / 2;
        for (const [dx, h] of [[4, 3], [6, 7], [8, 4], [10, 6]]) context.fillRect(r.x + dx, mid - h / 2, 1.2, h);
        context.fillRect(r.x + 12, mid - 0.5, 7, 1.2);
      }
      context.restore();
    }
  }

  /** The clip and the icon at a point of the lanes (lanePoint's): {clip, kind}, or null. */
  function clipIconAt(p) {
    if (!p.track) return null;
    const box = lanes();
    const clip = clipAt(p.track, p.ppq);
    if (!clip) return null;
    const laneTop = rowTop(p.index) - box.scrollTop;
    const cx = p.x - box.scrollLeft, cy = p.y - box.scrollTop;
    const icon = clipIcons(clip, clip, laneTop, heightOf(p.track)).find((r) => cx >= r.x && cx <= r.x + r.width && cy >= r.y && cy <= r.y + r.height);
    return icon ? { clip, kind: icon.kind } : null;
  }

  /** Transients (the Audio menu's Show Transients): a thin mark where each hit or note starts. */
  function drawTransients(context, clip, shape, x0, x1, top, height) {
    const times = Hub.transients ? Hub.transients.timesFor(clip) : null;
    if (!times || !times.length) return;
    const offset = shape.audioOffset !== undefined ? shape.audioOffset : clip.audioOffset || 0;
    const startSeconds = map().secondsAt(shape.start);
    context.fillStyle = "rgba(255, 209, 102, 0.85)";
    for (const t of times) {
      const x = Math.round(xOf(map().ppqAt(startSeconds + t - offset)));
      if (x <= x0 || x >= x1) continue;
      context.fillRect(x, top, 1, height);
      context.beginPath();
      context.moveTo(x - 3, top);
      context.lineTo(x + 4, top);
      context.lineTo(x + 0.5, top + 4);
      context.closePath();
      context.fill();
    }
  }

  /** Silence Areas: the stretches gone silent, dimmed over the waveform (their window's preview
      while it's open). */
  function drawSilence(context, clip, shape, x0, x1, top, height) {
    const areas = Hub.silence ? Hub.silence.areasFor(clip) : null;
    if (!areas || !areas.length) return;
    const offset = shape.audioOffset !== undefined ? shape.audioOffset : clip.audioOffset || 0;
    const startSeconds = map().secondsAt(shape.start);
    const xAt = (seconds) => xOf(map().ppqAt(startSeconds + seconds - offset));
    context.fillStyle = "rgba(10, 11, 14, 0.42)";
    for (const [from, to] of areas) {
      const a = Math.max(x0, xAt(from)), b = Math.min(x1, xAt(to));
      if (b - a >= 0.5) context.fillRect(a, top, b - a, height);
    }
  }

  /** The menu of a clip's effect (its icon, its right-click menu): Edit Effect, Remove Effect. */
  function effectMenu(anchor, track, clip) {
    const effect = clip.effect;
    if (!effect) return;
    Hub.popupMenu(anchor, [
      { group: effect.name || "Clip Effect" },
      ...(effect.error ? [{ label: effect.error, value: "", disabled: true }] : []),
      { label: "Edit Effect", value: "edit", disabled: !effect.slot || Boolean(effect.error),
        title: "Its window: what it does to this clip alone" },
      { label: "Remove Effect", value: "remove", title: "The clip plays as it is again (Undo brings the effect back)" }
    ], (pick) => {
      if (pick === "edit") Hub.openScreen(track.id, effect.slot);
      if (pick === "remove") Hub.send("clipEdits", { edits: [{ op: "effect", trackId: track.id, clipId: clip.id, remove: true }], label: "Remove Clip Effect" });
    });
  }

  /** Gives an audio clip an effect of its own: a Browser entry (an effect). */
  function setClipEffect(track, clip, entry) {
    if (!track || !clip || !clip.audioFile || !entry || entry.kind !== "effect") return false;
    if (track.frozen) { Hub.toast(frozenNote(track)); return true; }
    Hub.send("clipEdits", { edits: [{ op: "effect", trackId: track.id, clipId: clip.id, file: entry.file, pluginId: entry.id }],
                            label: clip.effect ? "Replace Clip Effect" : "Add Clip Effect" });
    return true;
  }

  /** Where a Browser entry dragged over the lanes would land on a clip: {track, clip} for an effect
      over an audio clip, else null (it goes on the track). */
  function effectDropAt(clientX, clientY) {
    const entry = Hub.dragEntry;
    if (!entry || entry.kind !== "effect" || entry.builtin) return null;
    const track = trackAt(clientY);
    const clip = track && track.kind === "audio" ? clipAt(track, ppqAt(clientX)) : null;
    return clip && clip.audioFile ? { track, clip } : null;
  }

  /** Where an audio clip's fade handles and volume handle sit, and its values (a drag's when dragging). */
  function audioShape(clip, shape) {
    const d = clipDrag;
    const live = d && d.kind === "audio" && d.clipId === clip.id ? d : null;
    return {
      gain: live ? live.gain : clip.gain || 0,
      fadeIn: live ? live.fadeIn : clip.fadeIn || 0,
      fadeOut: live ? live.fadeOut : clip.fadeOut || 0,
      start: shape.start,
      end: shape.start + shape.length
    };
  }

  function drawFades(context, clip, shape, x0, x1, top, height, handles) {
    const a = audioShape(clip, shape);
    const fadeInX = xOf(ppqAfter(a.start, a.fadeIn)), fadeOutX = xOf(ppqAfter(a.end, -a.fadeOut));
    const bottom = top + height;

    // The fades as Studio One draws them: a curve, with what it takes away shaded.
    const curve = (fromX, toX, rising) => {
      context.beginPath();
      context.moveTo(fromX, rising ? top : top);
      for (let i = 0; i <= 16; i++) {
        const t = i / 16, level = Math.sin(Math.PI / 2 * (rising ? t : 1 - t));
        context.lineTo(fromX + (toX - fromX) * t, bottom - level * height);
      }
      context.lineTo(toX, top);
      context.closePath();
      context.fillStyle = "rgba(0, 0, 0, 0.35)";
      context.fill();
    };

    if (a.fadeIn > 0) curve(x0, fadeInX, true);
    if (a.fadeOut > 0) curve(fadeOutX, x1, false);
    if (!handles) return;

    context.fillStyle = "#ffffff";
    context.fillRect(Math.round(fadeInX) - 3, top - 3, 6, 6);
    context.fillRect(Math.round(fadeOutX) - 3, top - 3, 6, 6);
    const middle = (x0 + x1) / 2;
    context.fillRect(Math.round(middle) - 5, top - 2, 10, 4);

    if (a.gain !== 0 || (clipDrag && clipDrag.kind === "audio" && clipDrag.clipId === clip.id)) {
      context.font = "10px Segoe UI, system-ui, sans-serif";
      context.textBaseline = "top";
      context.fillText(`${a.gain > 0 ? "+" : ""}${a.gain.toFixed(1)} dB`, middle + 8, top + 2);
    }
  }

  /** Which handle of an audio clip is under the pointer, if any. */
  function audioHandleAt(hit, p) {
    if (!hit || !hit.clip.audioFile) return null;
    const clip = hit.clip, height = heightOf(hit.track);
    const index = Hub.state.tracks.indexOf(hit.track);
    const top = rowTop(index) + 2 + (height < 70 ? 11 : 14);   // the clip body's top, in content pixels
    const y = p.y - top;
    if (Math.abs(y) > 7) return null;
    const x = (q) => contentX(q);
    const fadeInX = x(ppqAfter(clip.start, clip.fadeIn || 0));
    const fadeOutX = x(ppqAfter(clip.start + clip.length, -(clip.fadeOut || 0)));
    const middle = x(clip.start + clip.length / 2);
    if (Math.abs(p.x - middle) <= 7) return "gain";
    if (Math.abs(p.x - fadeInX) <= 6) return "fadeIn";
    if (Math.abs(p.x - fadeOutX) <= 6) return "fadeOut";
    return null;
  }

  /** An audio clip's waveform, from the file's peaks: each column shows the loudest and
      quietest moment of the audio under it. Zoomed in, finer peaks are asked for once. */
  const askedFine = new Set();

  function drawWaveform(context, clip, shape, x0, x1, top, height, width) {
    const coarse = Hub.state.peaks[clip.audioFile];
    const fine = Hub.state.finePeaks[clip.audioFile];
    const pixelsPerSecond = pxPerPpq(shape.start) * map().bpmAt(shape.start) / 60;
    const wantFine = coarse && pixelsPerSecond > 1.5 * coarse.rate / coarse.bucket && !String(clip.audioFile).startsWith("take:");

    if (wantFine && !fine && !askedFine.has(clip.audioFile)) {
      askedFine.add(clip.audioFile);
      Hub.send("audioPeaksFine", { file: clip.audioFile });
    }

    const peaks = wantFine && fine ? fine : coarse;
    const gain = Math.pow(10, audioShape(clip, shape).gain / 20);
    const middle = top + height / 2, scale = Math.min(gain, 2) * (height / 2 - 2) / 127;
    context.fillStyle = "rgba(18, 19, 22, 0.8)";
    context.fillRect(Math.max(x0, 0), Math.round(middle), Math.min(x1, width) - Math.max(x0, 0), 1);
    if (!peaks || !peaks.peaks) return;

    // Audio keeps its speed: each column shows the audio the song's time puts under it.
    const offset = shape.audioOffset !== undefined ? shape.audioOffset : clip.audioOffset || 0;
    const perBucket = peaks.bucket / peaks.rate;                    // seconds per peak pair
    const buckets = peaks.peaks.length / 2;
    const scroll = lanes().scrollLeft;
    const startSeconds = map().secondsAt(shape.start);
    const audioAt = (x) => offset + map().secondsAt(ppqOfContentX(x + scroll)) - startSeconds;

    let to = audioAt(Math.floor(Math.max(x0, 0)));

    for (let x = Math.floor(Math.max(x0, 0)); x < Math.min(x1, width); x++) {
      const from = to;
      to = audioAt(x + 1);
      let first = Math.floor(from / perBucket), last = Math.max(first, Math.ceil(to / perBucket) - 1);
      if (last < 0 || first >= buckets) continue;
      first = Math.max(0, first);
      last = Math.min(buckets - 1, last);
      let high = 0, low = 0;
      for (let b = first; b <= last; b++) {
        high = Math.max(high, peaks.peaks[2 * b]);
        low = Math.min(low, peaks.peaks[2 * b + 1]);
      }
      context.fillRect(x, Math.max(top, middle - high * scale), 1, Math.min(height, Math.max(1, (high - low) * scale)));
    }
  }

  // ---------------------------------------------------------------- automation
  // Two kinds of envelope share the editing here: a track's (Automation view: one lane over the
  // track's clips, for the target its header picks) and a clip's own (the clip's Automation: its
  // Gain, Pan and effect parameters, or a MIDI clip's controllers, drawn on the clip itself, with a
  // bar of its lanes above it). Each is a lane: where it's drawn, how values sit in its height, its
  // points as song positions, and how a change is kept.
  const autoTargets = new Map();   // track id -> the target its lane shows
  let autoDrag = null;             // a point being dragged, or a line being drawn: {lane, points, ...}
  let autoSelected = null;         // {lane id, ppq}

  const autoKey = (trackId) => autoTargets.get(trackId) || "volume";
  const laneKey = (lane) => lane.kind === "param" ? `param:${lane.slot}:${lane.param}` : lane.kind;

  function targetOf(key) {
    if (key === "volume" || key === "pan") return { kind: key, slot: 0, param: 0 };
    const [, slot, param] = key.split(":");
    return { kind: "param", slot: Number(slot), param: Number(param) };
  }

  /** A plugin parameter an envelope drives (from the track's instrument or inserts). */
  function paramOf(track, key) {
    const target = targetOf(key);
    if (target.kind !== "param") return null;
    const plugin = target.slot === 0 ? track.instrument : (track.inserts || []).find((i) => i.insertId === target.slot);
    return plugin ? (plugin.params || []).find((p) => p.id === target.param) || null : null;
  }

  /** A track's envelope lane (Automation view), on the track at index. */
  function trackLane(track, index) {
    const key = autoKey(track.id);
    const h = heightOf(track);
    const toNorm = (value) => {
      if (key === "volume") return Hub.dbToFader(value);
      if (key === "pan") return (value + 1) / 2;
      const p = paramOf(track, key);
      return p && p.max > p.min ? (value - p.min) / (p.max - p.min) : 0;
    };
    const fromNorm = (n) => {
      n = Hub.clamp01(n);
      if (key === "volume") return Hub.faderToDb(n);
      if (key === "pan") return n * 2 - 1;
      const p = paramOf(track, key);
      if (!p) return 0;
      const value = p.min + n * (p.max - p.min);
      return p.stepped ? Math.round(value) : value;
    };
    return {
      id: `track:${track.id}:${key}`, track, box: { top: rowTop(index) + 6, height: h - 13 }, from: 0, to: Infinity,
      toNorm, fromNorm,
      resting: () => key === "volume" ? track.volume : key === "pan" ? track.pan : (paramOf(track, key) || { value: 0 }).value,
      stored: () => { const lane = (track.automation || []).find((l) => laneKey(l) === key); return lane ? lane.points : []; },
      shapes: () => { const lane = (track.automation || []).find((l) => laneKey(l) === key); return lane && Array.isArray(lane.shapes) ? lane.shapes : []; },
      /** shapes: its shape regions as they're to be (left out: as they are). */
      commit(points, shapes) {
        const target = targetOf(key);
        // Shown at once; the engine's copy follows with the next tracks update.
        const old = (track.automation || []).find((l) => laneKey(l) === key);
        const lanesNow = (track.automation || []).filter((l) => laneKey(l) !== key);
        const keep = shapes !== undefined ? shapes : (old && old.shapes) || [];
        if (points.length) lanesNow.push({ kind: target.kind, slot: target.slot, param: target.param, points, shapes: keep });
        track.automation = lanesNow;
        const message = { trackId: track.id, target, points };
        if (shapes !== undefined) message.shapes = shapes;
        Hub.send("setAutomation", message);
      }
    };
  }

  // ------------------------------------------------ a clip's own automation
  // A MIDI clip's controllers: the common ones get a button each, the rest are in More.
  const CC_NAMES = { 1: "Mod Wheel", 2: "Breath", 4: "Foot", 5: "Portamento Time", 7: "Volume", 8: "Balance", 10: "Pan", 11: "Expression",
                     64: "Sustain", 65: "Portamento", 66: "Sostenuto", 67: "Soft Pedal", 71: "Resonance", 72: "Release", 73: "Attack",
                     74: "Brightness", 91: "Reverb", 93: "Chorus" };
  const CC_BUTTONS = [1, 11, 64, 7, 10, 2, 74];
  const CC_REST = { 7: 100, 8: 64, 10: 64, 11: 127, 71: 64, 72: 64, 73: 64, 74: 64 };   // where a controller sits untouched (else 0)
  const ccName = (n) => CC_NAMES[n] || `CC ${n}`;

  let clipAuto = null;             // the clip whose automation is open: {trackId, clipId, key}

  /** The clip's effect's parameter a key names (fx:<id>). */
  function clipParamOf(clip, key) {
    if (!key.startsWith("fx:") || !clip.effect) return null;
    const id = Number(key.slice(3));
    return (clip.effect.params || []).find((p) => p.id === id) || null;
  }

  /** What a clip lane's key is called. */
  function clipKeyName(clip, key) {
    if (key === "gain") return "Gain";
    if (key === "pan") return "Pan";
    if (key.startsWith("cc:")) return ccName(Number(key.slice(3)));
    const p = clipParamOf(clip, key);
    return p ? p.name : "Effect";
  }

  /** The clip whose automation is open, where it is: {track, clip, index}, or null (it's gone, hidden). */
  function clipAutoTarget() {
    if (!clipAuto) return null;
    const found = Hub.findClip(clipAuto.clipId);
    if (!found || found.track.id !== clipAuto.trackId) return null;
    const index = Hub.state.tracks.indexOf(found.track);
    return index >= 0 && heightOf(found.track) ? { track: found.track, clip: found.clip, index } : null;
  }

  /** The open clip's lane, drawn over its body (under its header). */
  function clipLane() {
    const target = clipAutoTarget();
    if (!target) return null;
    const { track, clip, index } = target, key = clipAuto.key;
    const h = heightOf(track), header = h < 70 ? 11 : 14;
    const audio = Boolean(clip.audioFile);
    // Content times (an audio clip's seconds into its file; a MIDI clip's quarter notes) and song positions.
    const startSeconds = map().secondsAt(clip.start);
    const toPpq = audio ? (t) => map().ppqAt(Math.max(0, startSeconds + t - (clip.audioOffset || 0))) : (t) => clip.start + t - (clip.offset || 0);
    const toTime = audio ? (ppq) => (clip.audioOffset || 0) + map().secondsAt(ppq) - startSeconds : (ppq) => ppq - clip.start + (clip.offset || 0);
    const param = clipParamOf(clip, key);
    const cc = key.startsWith("cc:") ? Number(key.slice(3)) : -1;

    const toNorm = (v) => key === "gain" ? (v + 60) / 72 : key === "pan" ? (v + 1) / 2 : cc >= 0 ? v / 127
                        : param && param.max > param.min ? (v - param.min) / (param.max - param.min) : 0;
    const fromNorm = (n) => {
      n = Hub.clamp01(n);
      if (key === "gain") return Math.round((n * 72 - 60) * 10) / 10;
      if (key === "pan") return Math.round((n * 2 - 1) * 100) / 100;
      if (cc >= 0) return Math.round(n * 127);
      if (!param) return 0;
      const value = param.min + n * (param.max - param.min);
      return param.stepped ? Math.round(value) : value;
    };
    const text = (v) => {
      if (key === "gain") return v <= -59.9 ? "-inf dB" : `${v > 0 ? "+" : ""}${v.toFixed(1)} dB`;
      if (key === "pan") return Math.abs(v) < 0.005 ? "C" : `${Math.round(Math.abs(v) * 100)}${v < 0 ? "L" : "R"}`;
      if (cc >= 0) return String(Math.round(v));
      return param && param.stepped ? String(Math.round(v)) : Number(v).toFixed(2);
    };

    return {
      id: `clip:${clip.id}:${key}`, track, clip, key, index, text,
      box: { top: rowTop(index) + 2 + header + 3, height: Math.max(8, h - 5 - header - 6) },
      from: clip.start, to: clip.start + clip.length,
      toNorm, fromNorm,
      resting: () => key === "gain" || key === "pan" ? 0 : cc >= 0 ? (CC_REST[cc] || 0) : param ? param.value : 0,
      stored: () => {
        const line = (clip.envelopes || []).find((e) => e.key === key);
        return line ? line.points.map(([t, v]) => [toPpq(t), v]) : [];
      },
      commit(points) {
        const times = points.map(([ppq, v]) => [toTime(ppq), v]);
        // Shown at once; the engine's copy follows with the next tracks update.
        const others = (clip.envelopes || []).filter((e) => e.key !== key);
        clip.envelopes = times.length ? others.concat([{ key, points: times }]) : others;
        Hub.send("clipEdits", { edits: [{ op: "envelope", trackId: track.id, clipId: clip.id, key, points: times }],
                                label: times.length ? "Clip Automation" : "Clear Clip Automation" });
      }
    };
  }

  function lanePoints(lane) {
    if (autoDrag && autoDrag.lane.id === lane.id) return autoDrag.points;
    return lane.stored();
  }

  const laneY = (lane, value) => lane.box.top + (1 - Hub.clamp01(lane.toNorm(value))) * lane.box.height;

  /** A lane's line, its points and (for the point picked) its value, clipped to x0..x1 (canvas pixels). */
  function drawLaneLine(context, lane, scrollX, scrollY, x0, x1) {
    const points = lanePoints(lane);
    const y = (value) => laneY(lane, value) - scrollY;
    context.strokeStyle = "#ffd166";
    context.lineWidth = 1.5;

    if (!points.length) {
      // No envelope yet: where the control is now, dashed.
      const at = Math.round(y(lane.resting())) + 0.5;
      context.setLineDash([4, 4]);
      context.beginPath();
      context.moveTo(x0, at);
      context.lineTo(x1, at);
      context.stroke();
      context.setLineDash([]);
      context.lineWidth = 1;
      return;
    }

    context.beginPath();
    context.moveTo(x0, y(points[0][1]));
    for (const [ppq, value] of points) context.lineTo(contentX(ppq) - scrollX, y(value));
    context.lineTo(x1, y(points[points.length - 1][1]));
    context.stroke();
    context.lineWidth = 1;

    // A shape region's points are its line alone (it's edited as a whole).
    const spans = lane.shapes ? lane.shapes().map((r) => [r.from, r.to]) : [];
    for (const [ppq, value] of points) {
      const x = Math.round(contentX(ppq) - scrollX);
      if (x < x0 - 4 || x > x1 + 4) continue;
      if (spans.some(([a, b]) => ppq >= a - 1e-6 && ppq <= b + 1e-6)) continue;
      const selected = autoSelected && autoSelected.id === lane.id && Math.abs(autoSelected.ppq - ppq) < 1e-9;
      context.fillStyle = selected ? "#ffffff" : "#ffd166";
      context.fillRect(x - 3, Math.round(y(value)) - 3, 7, 7);

      // The picked point's value beside it (a clip's lanes).
      if (selected && lane.text) {
        const label = lane.text(value);
        context.font = "10px Segoe UI, system-ui, sans-serif";
        context.textBaseline = "middle";
        const w = context.measureText(label).width + 8;
        const lx = x + 7 + w > x1 ? x - 7 - w : x + 7, ly = Math.round(y(value));
        context.fillStyle = "rgba(15, 16, 19, 0.85)";
        context.fillRect(lx, ly - 7, w, 14);
        context.fillStyle = "#ffffff";
        context.fillText(label, lx + 4, ly);
      }
    }
  }

  function drawEnvelopes(context, width, height, scrollY) {
    const scrollX = lanes().scrollLeft;
    Hub.state.tracks.forEach((track, index) => {
      if (!heightOf(track)) return;   // hidden by the track filter
      const lane = trackLane(track, index);
      const top = lane.box.top - scrollY;
      if (top > height || top + lane.box.height < 0) return;

      // The clips step back, so the envelope reads clearly.
      context.fillStyle = "rgba(24, 25, 28, 0.55)";
      context.fillRect(0, top - 5, width, lane.box.height + 10);
      drawShapes(context, lane, scrollY, width);
      drawLaneLine(context, lane, scrollX, scrollY, 0, width);
    });
  }

  // ------------------------------------------------ shape regions (Automation view)
  // A shape region: a repeating shape over a stretch of a track's lane, kept to edit again. Its
  // points are made from it (they're what plays) and replace the lane's points over its stretch;
  // made again whenever it changes. {id, from, to (song positions), shape, cycle (quarters a
  // cycle), low, high (0..1 of the lane's height), phase (0..1 of a cycle), seed (Random's)}.
  // Right-click the lane: Add Shape Region; on one: its Shape, its Cycle, Edit Shape..., Keep as
  // Points (the region goes, its points stay), Remove Shape. Its strip at the lane's top drags it
  // along, its ends resize it; a double-click on the strip opens Edit Shape.
  const SHAPE_KINDS = [["sine", "Sine"], ["triangle", "Triangle"], ["sawUp", "Saw Up"], ["sawDown", "Saw Down"], ["square", "Square"], ["random", "Random"]];
  const CYCLES = [[0.125, "1/32"], [0.25, "1/16"], [0.5, "1/8"], [1, "1/4"], [2, "1/2"], [4, "1 bar"], [8, "2 bars"], [16, "4 bars"]];
  const SHAPE_HEAD = 13;                 // px: a region's strip at the top of the lane
  let shapeDrag = null;                  // {lane, id, part: "move" | "left" | "right", grab, from, to, low, high, pointer, moved}
  let shapePanel = null;                 // the open Edit Shape panel: {box, trackId, key, id}

  const shapeName = (kind) => (SHAPE_KINDS.find(([k]) => k === kind) || SHAPE_KINDS[0])[1];
  const cycleName = (q) => { const c = CYCLES.find(([v]) => Math.abs(v - q) < 1e-9); return c ? c[1] : `${+q.toFixed(3)} q`; };

  /** Random's level for a cycle, the same every time for a region's seed. */
  function shapeRandom(seed, k) {
    let h = (Math.imul(seed | 0, 2654435761) ^ Math.imul(k | 0, 2246822519)) >>> 0;
    h = Math.imul(h ^ (h >>> 15), 2246822519) >>> 0;
    h = Math.imul(h ^ (h >>> 13), 3266489917) >>> 0;
    return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
  }

  /** Its points, in the lane's own values. */
  function shapePointsOf(r, lane) {
    const points = [], span = r.to - r.from;
    if (!(span > 1e-6)) return points;
    const cycle = Math.max(1 / 32, Number(r.cycle) || 1), phase = Number(r.phase) || 0;
    const level = (n) => lane.fromNorm(Hub.clamp01(r.low + (r.high - r.low) * n));
    const cyclesAt = (t) => (t - r.from) / cycle + phase;

    if (r.shape === "square" || r.shape === "random") {
      // Steps: a level each half cycle (Square, high first) or each cycle (Random).
      const part = r.shape === "square" ? 0.5 : 1;
      const valueOf = (k) => r.shape === "square" ? (((k % 2) + 2) % 2 === 0 ? 1 : 0) : shapeRandom(r.seed, k);
      let k = Math.floor(cyclesAt(r.from) / part + 1e-9);
      points.push([r.from, level(valueOf(k))]);
      for (let guard = 0; guard < 4000; guard++) {
        const next = r.from + ((k + 1) * part - phase) * cycle;
        if (next >= r.to - 1e-4) break;
        points.push([next - 1e-4, level(valueOf(k))]);
        k++;
        points.push([next, level(valueOf(k))]);
      }
      points.push([r.to, level(valueOf(k))]);
      return points;
    }

    const wave = (x) => r.shape === "triangle" ? (x < 0.5 ? 2 * x : 2 - 2 * x) : r.shape === "sawUp" ? x
                      : r.shape === "sawDown" ? 1 - x : (1 - Math.cos(2 * Math.PI * x)) / 2;
    const steps = Math.min(4000, Math.max(2, Math.ceil(span / cycle * 32)));
    for (let i = 0; i <= steps; i++) {
      const t = r.from + span * i / steps, c = cyclesAt(t);
      points.push([t, level(wave(c - Math.floor(c)))]);
    }
    return points;
  }

  /** The lane's points made again from its shape regions: each region's own over its stretch, the
      lane's as they are elsewhere (and none over cleared stretches: where a region was). */
  function applyShapes(lane, shapes, cleared = []) {
    const spans = shapes.map((r) => [r.from, r.to]).concat(cleared);
    let points = lane.stored().filter(([ppq]) => !spans.some(([a, b]) => ppq >= a - 1e-6 && ppq <= b + 1e-6)).map((p) => p.slice());
    for (const r of shapes) points = points.concat(shapePointsOf(r, lane));
    points.sort((a, b) => a[0] - b[0]);
    lane.commit(points, shapes.map((r) => Object.assign({}, r)));
    drawArrangement();
  }

  const regionsOf = (lane) => (lane.shapes ? lane.shapes() : []);

  /** The region whose stretch a point is in (anywhere down the lane), or null. */
  function regionAt(lane, p) {
    return regionsOf(lane).find((r) => p.ppq >= r.from - 1e-9 && p.ppq <= r.to + 1e-9) || null;
  }

  /** A press on a region's strip: which part (its ends resize it, the rest moves it), or null. */
  function shapeHit(lane, p) {
    if (p.y < lane.box.top - 4 || p.y > lane.box.top + SHAPE_HEAD) return null;
    for (const r of regionsOf(lane)) {
      const x0 = contentX(r.from), x1 = contentX(r.to);
      if (p.x < x0 - 4 || p.x > x1 + 4) continue;
      const part = p.x - x0 <= 6 ? "left" : x1 - p.x <= 6 ? "right" : "move";
      return { region: r, part };
    }
    return null;
  }

  function drawShapes(context, lane, scrollY, width) {
    const top = lane.box.top - scrollY;
    context.font = "10px Segoe UI, system-ui, sans-serif";
    context.textBaseline = "middle";
    for (const r of regionsOf(lane)) {
      const d = shapeDrag && shapeDrag.lane.id === lane.id && shapeDrag.id === r.id ? shapeDrag : null;
      const from = d ? d.from : r.from, to = d ? d.to : r.to;
      const x0 = Math.round(xOf(from)), x1 = Math.round(xOf(to));
      if (x1 < 0 || x0 > width) continue;
      context.fillStyle = "rgba(76, 201, 240, 0.09)";
      context.fillRect(x0, top, x1 - x0, lane.box.height);
      // Its range: where low and high sit.
      context.strokeStyle = "rgba(76, 201, 240, 0.35)";
      context.setLineDash([3, 3]);
      for (const n of [r.low, r.high]) {
        const y = Math.round(top + (1 - n) * lane.box.height) + 0.5;
        context.beginPath();
        context.moveTo(x0, y);
        context.lineTo(x1, y);
        context.stroke();
      }
      context.setLineDash([]);
      context.fillStyle = d ? "rgba(76, 201, 240, 0.6)" : "rgba(76, 201, 240, 0.4)";
      context.fillRect(x0, top - 4, x1 - x0, SHAPE_HEAD);
      context.fillStyle = "#4cc9f0";
      context.fillRect(x0, top - 4, 2, lane.box.height + 4);
      context.fillRect(x1 - 2, top - 4, 2, lane.box.height + 4);
      if (x1 - x0 > 40) {
        context.save();
        context.beginPath();
        context.rect(x0 + 4, top - 4, x1 - x0 - 8, SHAPE_HEAD);
        context.clip();
        context.fillStyle = "#ffffff";
        context.fillText(`${shapeName(r.shape)} \u00b7 ${cycleName(r.cycle)}`, x0 + 6, top - 4 + SHAPE_HEAD / 2);
        context.restore();
      }
    }
  }

  /** Add Shape Region: two bars from the bar clicked in (as far as the next region). */
  function addShape(lane, p) {
    const bar = map().barAt(Math.max(0, p.ppq) + 1e-9);
    const from = bar.start;
    let to = map().ppqOfBar(bar.index + 2);
    const regions = regionsOf(lane);
    if (regions.some((r) => from >= r.from - 1e-9 && from < r.to - 1e-9)) return;
    for (const r of regions) if (r.from >= from - 1e-9) to = Math.min(to, r.from);
    if (to - from < 0.25) { Hub.toast("No room for a shape region here."); return; }
    const region = { id: Date.now() % 1000000000 + Math.floor(Math.random() * 1000), from, to, shape: "sine", cycle: 1,
                     low: 0.25, high: 0.75, phase: 0, seed: 1 + Math.floor(Math.random() * 99999) };
    applyShapes(lane, regions.concat([region]));
  }

  function changeShape(lane, id, change) {
    const regions = regionsOf(lane).map((r) => Object.assign({}, r));
    const region = regions.find((r) => r.id === id);
    if (!region) return;
    change(region);
    region.low = Hub.clamp01(Number(region.low) || 0);
    region.high = Hub.clamp01(Number(region.high) || 0);
    region.phase = Hub.clamp(Number(region.phase) || 0, 0, 1);
    applyShapes(lane, regions);
  }

  /** The region goes: its points too (remove), or they stay as plain points (keep). */
  function dropShape(lane, id, keepPoints) {
    const region = regionsOf(lane).find((r) => r.id === id);
    if (!region) return;
    const rest = regionsOf(lane).filter((r) => r.id !== id);
    if (keepPoints) lane.commit(lane.stored().map((pt) => pt.slice()), rest.map((r) => Object.assign({}, r)));
    else applyShapes(lane, rest, [[region.from, region.to]]);
    closeShapePanel();
    drawArrangement();
  }

  function shapeDown(event, lane, hit, p) {
    event.preventDefault();
    if (lanes().setPointerCapture) lanes().setPointerCapture(event.pointerId);
    const r = hit.region, regions = regionsOf(lane);
    // As far as the regions either side.
    const low = Math.max(0, ...regions.filter((o) => o.id !== r.id && o.to <= r.from + 1e-9).map((o) => o.to));
    const high = Math.min(Infinity, ...regions.filter((o) => o.id !== r.id && o.from >= r.to - 1e-9).map((o) => o.from));
    shapeDrag = { lane, id: r.id, part: hit.part, grab: p.ppq, from: r.from, to: r.to, start: { from: r.from, to: r.to }, low, high,
                  pointer: event.pointerId, moved: false };
    drawArrangement();
  }

  function shapeMove(event) {
    const d = shapeDrag, p = lanePoint(event);
    const minimum = 0.25;
    if (d.part === "move") {
      const length = d.start.to - d.start.from;
      const from = Hub.clamp(snap(d.start.from + p.ppq - d.grab), d.low, d.high - length);
      d.from = from;
      d.to = from + length;
    } else if (d.part === "left") {
      d.from = Hub.clamp(snap(p.ppq), d.low, d.start.to - minimum);
    } else {
      d.to = Hub.clamp(snap(p.ppq), d.start.from + minimum, d.high);
    }
    d.moved = d.moved || Math.abs(d.from - d.start.from) > 1e-9 || Math.abs(d.to - d.start.to) > 1e-9;
    drawArrangement();
  }

  function shapeUp() {
    const d = shapeDrag;
    shapeDrag = null;
    if (!d || !d.moved) { drawArrangement(); return; }
    const regions = regionsOf(d.lane).map((r) => (r.id === d.id ? Object.assign({}, r, { from: d.from, to: d.to }) : Object.assign({}, r)));
    applyShapes(d.lane, regions, [[d.start.from, d.start.to]]);
  }

  /** The track lane a panel or a menu works on, as it is now. */
  function laneFor(trackId, key) {
    const track = Hub.trackById(trackId);
    if (!track || autoKey(trackId) !== key) return null;
    return trackLane(track, Hub.state.tracks.indexOf(track));
  }

  function closeShapePanel() {
    if (!shapePanel) return;
    shapePanel.box.remove();
    shapePanel = null;
  }

  /** Edit Shape: its shape, cycle, low and high (% of the lane's height) and phase. */
  function openShapePanel(lane, id, anchor) {
    closeShapePanel();
    const region = regionsOf(lane).find((r) => r.id === id);
    if (!region) return;
    const trackId = lane.track.id, key = autoKey(trackId);
    const box = Hub.element("div", "shape-panel");
    box.setAttribute("role", "dialog");
    box.setAttribute("aria-label", "Edit Shape");
    const field = (label, control) => { const l = Hub.element("label", "shape-field"); l.append(Hub.element("span", "tool-label", label), control); return l; };
    const select = (options, value) => {
      const s = document.createElement("select");
      for (const [v, text] of options) { const o = document.createElement("option"); o.value = String(v); o.textContent = text; s.appendChild(o); }
      s.value = String(value);
      return s;
    };
    const number = (value) => {
      const input = document.createElement("input");
      input.type = "number"; input.min = "0"; input.max = "100"; input.step = "1"; input.value = String(Math.round(value * 100));
      return input;
    };
    const shape = select(SHAPE_KINDS, region.shape), cycle = select(CYCLES.some(([v]) => Math.abs(v - region.cycle) < 1e-9) ? CYCLES : CYCLES.concat([[region.cycle, cycleName(region.cycle)]]), region.cycle);
    const low = number(region.low), high = number(region.high), phase = number(region.phase || 0);
    const close = Hub.element("button", "icon-button", "\u00d7");
    close.type = "button";
    close.title = "Done";
    const head = Hub.element("div", "shape-head");
    head.append(Hub.element("span", "shape-title", "Shape region"), close);
    box.append(head, field("Shape", shape), field("Cycle", cycle), field("Low %", low), field("High %", high), field("Phase %", phase));
    document.body.appendChild(box);

    const r = anchor.getBoundingClientRect();
    box.style.left = Math.max(4, Math.min(window.innerWidth - box.offsetWidth - 4, r.left)) + "px";
    box.style.top = Math.max(4, Math.min(window.innerHeight - box.offsetHeight - 4, r.bottom + 4)) + "px";

    const apply = () => {
      const now = laneFor(trackId, key);
      if (!now) { closeShapePanel(); return; }
      changeShape(now, id, (region) => {
        region.shape = shape.value;
        region.cycle = Number(cycle.value) || region.cycle;
        region.low = Hub.clamp(Number(low.value), 0, 100) / 100;
        region.high = Hub.clamp(Number(high.value), 0, 100) / 100;
        region.phase = Hub.clamp(Number(phase.value), 0, 100) / 100;
      });
    };
    for (const control of [shape, cycle, low, high, phase]) {
      control.addEventListener("change", apply);
      control.addEventListener("keydown", (event) => { event.stopPropagation(); if (event.key === "Enter") control.blur(); if (event.key === "Escape") closeShapePanel(); });
    }
    close.addEventListener("click", closeShapePanel);
    shapePanel = { box, trackId, key, id };
  }

  document.addEventListener("pointerdown", (event) => { if (shapePanel && !shapePanel.box.contains(event.target)) closeShapePanel(); }, true);

  /** The right-click menu's items for the Automation view's lane under the pointer (empty: none). */
  function shapeMenuItems(p) {
    if (!view.automation || !p.track || !heightOf(p.track)) return [];
    const lane = trackLane(p.track, p.index);
    if (p.y < lane.box.top - 6 || p.y > lane.box.top + lane.box.height + 6) return [];
    const region = regionAt(lane, p);
    if (!region) return [{ label: "Add Shape Region", value: "shapeAdd", title: "A repeating shape (Sine, Triangle, Saw, Square, Random) over two bars here, kept to edit again" },
                         { separator: true }];
    return [
      { group: `${shapeName(region.shape)} \u00b7 ${cycleName(region.cycle)}` },
      { label: "Shape", submenu: SHAPE_KINDS.map(([v, name]) => ({ label: name, value: "shapeKind:" + v, checked: region.shape === v })) },
      { label: "Cycle", submenu: CYCLES.map(([v, name]) => ({ label: name, value: "shapeCycle:" + v, checked: Math.abs(region.cycle - v) < 1e-9 })) },
      { label: "Edit Shape\u2026", value: "shapeEdit", title: "Its shape, cycle, low and high, and phase" },
      { label: "Keep as Points", value: "shapeKeep", title: "The region goes; its points stay, as plain points" },
      { label: "Remove Shape", value: "shapeRemove", title: "The region and its points go" },
      { separator: true }
    ];
  }

  function shapePick(pick, p, event) {
    const lane = p.track ? trackLane(p.track, p.index) : null;
    if (!lane) return false;
    const region = regionAt(lane, p);
    if (pick === "shapeAdd") { addShape(lane, p); return true; }
    if (!region) return false;
    if (pick.startsWith("shapeKind:")) changeShape(lane, region.id, (r) => { r.shape = pick.slice(10); });
    else if (pick.startsWith("shapeCycle:")) changeShape(lane, region.id, (r) => { r.cycle = Number(pick.slice(11)); });
    else if (pick === "shapeEdit") openShapePanel(lane, region.id, Hub.pointAnchor(event));
    else if (pick === "shapeKeep") dropShape(lane, region.id, true);
    else if (pick === "shapeRemove") dropShape(lane, region.id, false);
    else return false;
    return true;
  }

  /** The open clip's lane, over its body: the clip steps back, the line in front. */
  function drawClipEnvelope(context, width, height, scrollY) {
    const lane = clipLane();
    if (!lane) return;
    const scrollX = lanes().scrollLeft;
    const x0 = Math.max(0, xOf(lane.from)), x1 = Math.min(width, xOf(lane.to));
    const top = lane.box.top - scrollY;
    if (x1 <= x0 || top > height || top + lane.box.height < 0) return;

    context.save();
    context.beginPath();
    context.rect(x0, top - 4, x1 - x0, lane.box.height + 8);
    context.clip();
    context.fillStyle = "rgba(20, 21, 25, 0.5)";
    context.fillRect(x0, top - 4, x1 - x0, lane.box.height + 8);
    drawLaneLine(context, lane, scrollX, scrollY, x0, x1);
    context.restore();
  }

  function pointIndexAt(lane, p) {
    return lanePoints(lane).findIndex(([ppq, value]) => Math.abs(contentX(ppq) - p.x) <= 6 && Math.abs(laneY(lane, value) - p.y) <= 6);
  }

  function commitLane(lane, points) {
    lane.commit(points.slice().sort((a, b) => a[0] - b[0]));
    drawArrangement();
  }

  /** Whether a point of the lanes is on the open clip's lane (its body, under its header). */
  function onClipLane(lane, p) {
    return lane && p.x >= contentX(lane.from) && p.x <= contentX(lane.to)
      && p.y >= lane.box.top - 4 && p.y <= lane.box.top + lane.box.height + 4;
  }

  function autoDown(event, p, lane) {
    if (!lane) return;
    const box = lane.box;
    const inRange = (ppq) => Hub.clamp(ppq, lane.from, lane.to);
    const valueAt = (y) => lane.fromNorm(1 - (y - box.top) / box.height);
    const points = lanePoints(lane).map((pt) => pt.slice());
    const hit = pointIndexAt(lane, p);
    if (lanes().setPointerCapture) lanes().setPointerCapture(event.pointerId);

    if (view.tool === "eraser") {
      if (hit >= 0) {
        points.splice(hit, 1);
        commitLane(lane, points);
      }
      return;
    }

    if (view.tool === "paint") {
      // Drawing: a point on each grid step the pointer crosses, replacing what was there; or,
      // with a shape, the shape from where the press was to the pointer.
      const from = inRange(snap(p.ppq));
      autoDrag = { lane, valueAt, inRange, draw: true, original: points, drawn: new Map(), from, points,
                   start: { ppq: from, norm: Hub.clamp01(1 - (p.y - box.top) / box.height) } };
      autoDraw(p);
      return;
    }

    let index = hit;
    if (index < 0) {
      const at = inRange(event.altKey ? p.ppq : snap(p.ppq));
      points.push([at, valueAt(p.y)]);
      points.sort((a, b) => a[0] - b[0]);
      index = points.findIndex((pt) => pt[0] === at);
    }
    autoDrag = { lane, valueAt, inRange, index, points, moved: hit < 0 };
    autoSelected = { id: lane.id, ppq: points[index][0] };
    drawArrangement();
  }

  /** A shape's height (0..1) at a time: a Line or Parabola from the press to the pointer; a wave
      (Square, Triangle, Saw, Sine) between their heights, four Quantize steps a cycle. */
  function shapeNorm(start, end, ppq) {
    let w;
    if (view.shape === "line" || view.shape === "parabola") {
      const span = end.ppq - start.ppq;
      const share = Math.abs(span) < 1e-9 ? 1 : Hub.clamp((ppq - start.ppq) / span, 0, 1);
      w = view.shape === "line" ? share : share * share;
    } else {
      const phase = Math.abs(ppq - start.ppq) / Math.max(view.quantize * 4, 0.25);
      const x = phase - Math.floor(phase);
      w = view.shape === "square" ? (x < 0.5 ? 1 : 0) : view.shape === "triangle" ? (x < 0.5 ? 2 * x : 2 - 2 * x)
        : view.shape === "saw" ? x : (1 - Math.cos(2 * Math.PI * x)) / 2;
    }
    return start.norm + (end.norm - start.norm) * w;
  }

  function autoDraw(p) {
    const d = autoDrag, lane = d.lane;
    if (view.shape !== "freehand") {
      const end = { ppq: d.inRange(snap(p.ppq)), norm: Hub.clamp01(1 - (p.y - lane.box.top) / lane.box.height) };
      const from = Math.min(d.start.ppq, end.ppq), to = Math.max(d.start.ppq, end.ppq);
      const step = Math.max(view.quantize / 4, 1 / 64);
      const drawn = [];
      for (let ppq = from; ppq <= to + 1e-9; ppq += step) drawn.push([ppq, lane.fromNorm(shapeNorm(d.start, end, ppq))]);
      // A square's edges stand straight: each change of level gets a point just before it too.
      if (view.shape === "square")
        for (let i = drawn.length - 1; i > 0; i--)
          if (Math.abs(lane.toNorm(drawn[i][1]) - lane.toNorm(drawn[i - 1][1])) > 0.5 / 127)
            drawn.splice(i, 0, [drawn[i][0] - 1e-4, drawn[i - 1][1]]);
      d.points = d.original.filter(([ppq]) => ppq < from - 1e-9 || ppq > to + 1e-9).concat(drawn);
      d.points.sort((a, b) => a[0] - b[0]);
      d.moved = true;
      drawArrangement();
      return;
    }
    const at = d.inRange(snap(p.ppq));
    d.drawn.set(at, d.valueAt(p.y));
    const from = Math.min(d.from, ...d.drawn.keys()), to = Math.max(d.from, ...d.drawn.keys());
    d.points = d.original.filter(([ppq]) => ppq < from - 1e-9 || ppq > to + 1e-9).concat([...d.drawn].map(([ppq, value]) => [ppq, value]));
    d.points.sort((a, b) => a[0] - b[0]);
    d.moved = true;
    drawArrangement();
  }

  function autoMove(event) {
    const d = autoDrag, p = lanePoint(event);
    if (d.draw) {
      autoDraw(p);
      return;
    }
    const before = d.index > 0 ? d.points[d.index - 1][0] : d.lane.from;
    const after = d.index < d.points.length - 1 ? d.points[d.index + 1][0] : d.lane.to;
    const ppq = Hub.clamp(event.altKey ? p.ppq : snap(p.ppq), before, after);   // points keep their order
    d.points[d.index] = [ppq, d.valueAt(p.y)];
    autoSelected = { id: d.lane.id, ppq };
    d.moved = true;
    drawArrangement();
  }

  function autoUp() {
    const d = autoDrag;
    autoDrag = null;
    if (d && d.moved) commitLane(d.lane, d.points);
  }

  /** The lane the picked point is on, as it is now. */
  function selectedLane() {
    if (!autoSelected) return null;
    const lane = clipLane();
    if (lane && lane.id === autoSelected.id) return lane;
    if (!autoSelected.id.startsWith("track:")) return null;
    const track = Hub.trackById(Number(autoSelected.id.split(":")[1]));
    return track ? trackLane(track, Hub.state.tracks.indexOf(track)) : null;
  }

  function deleteSelectedPoint() {
    const lane = selectedLane();
    if (!lane || lane.id !== autoSelected.id) return false;
    const ppq = autoSelected.ppq;
    autoSelected = null;
    commitLane(lane, lanePoints(lane).filter(([at]) => Math.abs(at - ppq) > 1e-9));
    return true;
  }

  function setAutomationView(on) {
    view.automation = Boolean(on);
    autoSelected = null;
    if (view.automation) closeClipAutomation();
    document.body.classList.toggle("automation-view", view.automation);
    $("auto-view").setAttribute("aria-pressed", String(view.automation));
    drawArrangement();
    Hub.emit("viewChanged");
  }

  // ------------------------------------------------ the bar of a clip's lanes
  let clipAutoBar = null;

  function makeClipAutoBar() {
    const bar = document.createElement("div");
    bar.id = "clip-auto-bar";
    bar.className = "clip-auto-bar";
    bar.hidden = true;
    bar.setAttribute("role", "toolbar");
    bar.setAttribute("aria-label", "Clip Automation");
    bar.addEventListener("pointerdown", (event) => event.stopPropagation());
    bar.addEventListener("contextmenu", (event) => event.preventDefault());
    document.body.appendChild(bar);
    return bar;
  }

  /** Its buttons: the lanes the clip can have (a dot where one has points), Clear and ×. */
  function fillClipAutoBar(clip) {
    const bar = clipAutoBar;
    bar.textContent = "";
    const has = new Set((clip.envelopes || []).filter((e) => e.points && e.points.length).map((e) => e.key));
    const key = clipAuto.key;

    const title = document.createElement("span");
    title.className = "cab-title";
    title.textContent = "Automation";
    bar.appendChild(title);

    const lane = (label, value, tip) => {
      const b = document.createElement("button");
      b.type = "button";
      b.className = "cab-lane" + (has.has(value) ? " has" : "");
      b.textContent = label;
      b.title = tip || label;
      b.setAttribute("aria-pressed", String(key === value));
      b.addEventListener("click", () => showClipLane(value));
      bar.appendChild(b);
      return b;
    };

    const more = (label, pressed, hasData, items, tip) => {
      const b = document.createElement("button");
      b.type = "button";
      b.className = "cab-lane cab-more" + (hasData ? " has" : "");
      b.textContent = label + " ▾";
      b.title = tip;
      b.setAttribute("aria-pressed", String(pressed));
      b.addEventListener("click", () => Hub.popupMenu(b, items(), (value) => { if (value) showClipLane(value); }));
      bar.appendChild(b);
    };

    if (clip.audioFile) {
      lane("Gain", "gain", "The clip's volume as it plays (on top of its own volume)");
      lane("Pan", "pan", "The clip's place left to right as it plays");
      if (clip.effect) {
        const params = clip.effect.params || [];
        const fx = key.startsWith("fx:");
        const name = fx ? `FX: ${clipKeyName(clip, key)}` : "Effect";
        more(name, fx, [...has].some((k) => k.startsWith("fx:")),
             () => params.length ? params.filter((p) => !p.readOnly).map((p) => ({ label: p.name, value: `fx:${p.id}`, checked: key === `fx:${p.id}`,
                                                                                   title: has.has(`fx:${p.id}`) ? "Has automation" : undefined }))
                                 : [{ label: "Its parameters show once it's loaded", value: "", disabled: true }],
             `Automate a parameter of the clip's effect (${clip.effect.name || "its plugin"})`);
      }
    } else {
      for (const n of CC_BUTTONS) lane(ccName(n), `cc:${n}`, `MIDI CC ${n} (${ccName(n)}) as the clip plays`);
      const cc = key.startsWith("cc:") ? Number(key.slice(3)) : -1;
      const other = cc >= 0 && !CC_BUTTONS.includes(cc);
      more(other ? ccName(cc) : "More", other, [...has].some((k) => k.startsWith("cc:") && !CC_BUTTONS.includes(Number(k.slice(3)))),
           () => Array.from({ length: 128 }, (_, n) => n).filter((n) => !CC_BUTTONS.includes(n))
                   .map((n) => ({ label: `${n}${CC_NAMES[n] ? " " + CC_NAMES[n] : ""}`, value: `cc:${n}`, checked: key === `cc:${n}`,
                                  title: has.has(`cc:${n}`) ? "Has automation" : undefined })),
           "Any other MIDI controller");
    }

    const clear = document.createElement("button");
    clear.type = "button";
    clear.className = "cab-clear";
    clear.textContent = "Clear";
    clear.title = `Take this lane's points away (${clipKeyName(clip, key)})`;
    clear.disabled = !has.has(key);
    clear.addEventListener("click", () => {
      const l = clipLane();
      if (l) { autoSelected = null; commitLane(l, []); }
    });
    bar.appendChild(clear);

    const close = document.createElement("button");
    close.type = "button";
    close.className = "cab-close";
    close.textContent = "×";
    close.title = "Leave the clip's automation (Esc)";
    close.setAttribute("aria-label", "Close Clip Automation");
    close.addEventListener("click", closeClipAutomation);
    bar.appendChild(close);
    bar.dataset.shown = `${clip.id}|${key}|${[...has].sort().join(",")}|${clip.effect ? (clip.effect.params || []).length : 0}`;
  }

  /** Over the clip (just above it; inside its top when there's no room above), following it. */
  function placeClipAutoBar() {
    if (!clipAutoBar) return;
    const target = clipAutoTarget();
    if (clipAuto && !target && !autoDrag) { closeClipAutomation(); return; }
    if (!target) { clipAutoBar.hidden = true; return; }
    const { clip, index } = target;
    const shown = `${clip.id}|${clipAuto.key}|${(clip.envelopes || []).filter((e) => e.points && e.points.length).map((e) => e.key).sort().join(",")}|${clip.effect ? (clip.effect.params || []).length : 0}`;
    if (clipAutoBar.dataset.shown !== shown) fillClipAutoBar(clip);

    const box = lanes().getBoundingClientRect();
    const left = box.left + xOf(clip.start), right = box.left + xOf(clip.start + clip.length);
    const rowY = box.top + rowTop(index) - lanes().scrollTop;
    if (right < box.left + 4 || left > box.right - 4 || rowY + heightOf(target.track) < box.top || rowY > box.bottom) {
      clipAutoBar.hidden = true;
      return;
    }
    clipAutoBar.hidden = false;
    const barHeight = clipAutoBar.offsetHeight || 26;
    const width = clipAutoBar.offsetWidth || 300;
    const x = Hub.clamp(left, box.left + 2, Math.max(box.left + 2, box.right - width - 4));
    const y = rowY - barHeight - 2 >= box.top ? rowY - barHeight - 2 : rowY + 2;
    clipAutoBar.style.left = Math.round(x) + "px";
    clipAutoBar.style.top = Math.round(y) + "px";
  }

  function showClipLane(key) {
    if (!clipAuto) return;
    clipAuto.key = key;
    autoSelected = null;
    drawArrangement();
  }

  /** The clip's Automation: its bar over it and its first lane (one with points, or Gain / Mod Wheel). */
  function openClipAutomation(track, clip, key) {
    if (!track || !clip) return;
    if (track.frozen) { Hub.toast(frozenNote(track)); return; }
    if (view.automation) setAutomationView(false);
    if (!clipAutoBar) clipAutoBar = makeClipAutoBar();
    const first = (clip.envelopes || []).find((e) => e.points && e.points.length);
    clipAuto = { trackId: track.id, clipId: clip.id, key: key || (first ? first.key : clip.audioFile ? "gain" : "cc:1") };
    clipAutoBar.dataset.shown = "";
    Hub.state.selectedClips = [clip.id];
    autoSelected = null;
    drawArrangement();
  }

  function closeClipAutomation() {
    if (!clipAuto) return;
    clipAuto = null;
    autoSelected = null;
    if (clipAutoBar) clipAutoBar.hidden = true;
    drawArrangement();
  }

  Hub.clipAutomation = {
    open: openClipAutomation,
    close: closeClipAutomation,
    isOpen: () => Boolean(clipAuto)
  };

  // Esc leaves it.
  document.addEventListener("keydown", (event) => {
    if (event.key !== "Escape" || !clipAuto || document.querySelector(".popup-menu:not([hidden])")) return;
    closeClipAutomation();
  });

  /** What a click on a frozen track's clips says. */
  const frozenNote = (track) => `"${track.name}" is frozen: click its snowflake to unfreeze it, then edit it.`;

  /** The topmost clip of a track at a song position. */
  function clipAt(track, ppq) {
    const clips = track.clips || [];
    for (let i = clips.length - 1; i >= 0; i--)
      if (ppq >= clips[i].start && ppq < clips[i].start + clips[i].length) return clips[i];
    return null;
  }

  function placePlayhead() {
    const playhead = $("playhead");
    const x = xOf(Hub.songPosition());
    playhead.hidden = x < 0 || x > lanes().clientWidth;
    playhead.style.transform = `translateX(${Math.round(x)}px)`;
  }

  function drawArrangement() {
    drawRuler();
    if (Hub.videoLane) Hub.videoLane.draw();
    if (Hub.songLanes) Hub.songLanes.draw();
    drawLanes();
    placePlayhead();
    placeClipAutoBar();
  }

  function updateSizes() {
    const box = lanes();
    const viewEnd = map().barAt(ppqOfContentX(box.scrollLeft + box.clientWidth));
    const quarters = Math.max(map().ppqOfBar(MIN_SONG_BARS), map().ppqOfBar(viewEnd.index + 16));
    $("lanes-size").style.width = Math.ceil(contentX(quarters)) + "px";
    $("lanes-size").style.height = contentHeight() + EMPTY_SPACE + "px";
    $("track-space").style.height = EMPTY_SPACE + "px";
    drawArrangement();
  }

  // ---------------------------------------------------------------- zoom
  const zoomToSlider = (zoom) => Math.round(1000 * Math.log(zoom / MIN_ZOOM) / Math.log(MAX_ZOOM / MIN_ZOOM));
  const sliderToZoom = (value) => MIN_ZOOM * Math.pow(MAX_ZOOM / MIN_ZOOM, value / 1000);

  /** Zooms keeping the song position under anchorX (px from the lanes' left edge) in place. */
  function setZoom(zoom, anchorX) {
    const box = lanes();
    const anchor = anchorX === undefined ? box.clientWidth / 2 : anchorX;
    const ppq = (box.scrollLeft + anchor) / view.zoom;
    view.zoom = Hub.clamp(zoom, MIN_ZOOM, MAX_ZOOM);
    updateSizes();
    box.scrollLeft = Math.max(0, ppq * view.zoom - anchor);
    $("zoom").value = String(zoomToSlider(view.zoom));
    drawArrangement();
    Hub.emit("viewChanged");
  }

  /** Anchor for keyboard and button zooms: the playhead when it's on screen, else the middle. */
  function zoomAnchor() {
    const x = xOf(Hub.songPosition());
    return x >= 0 && x <= lanes().clientWidth ? x : undefined;
  }

  // ---------------------------------------------------------------- height zoom
  const vzoomToSlider = (vzoom) => Math.round(1000 * Math.log(vzoom / MIN_VZOOM) / Math.log(MAX_VZOOM / MIN_VZOOM));
  const sliderToVzoom = (value) => MIN_VZOOM * Math.pow(MAX_VZOOM / MIN_VZOOM, value / 1000);

  /** Every track taller or shorter, keeping what's at anchorY (px from the lanes' top) in place. */
  function setVZoom(vzoom, anchorY) {
    const box = lanes();
    const anchor = anchorY === undefined ? 0 : anchorY;
    const before = contentHeight() || 1;
    const at = box.scrollTop + anchor;
    view.vzoom = Hub.clamp(Math.abs(vzoom - 1) < 0.03 ? 1 : vzoom, MIN_VZOOM, MAX_VZOOM);   // 100% catches
    Hub.emit("trackHeights");                                // the track list's headers follow
    updateSizes();
    box.scrollTop = Math.max(0, at * contentHeight() / before - anchor);
    $("tracklist").scrollTop = box.scrollTop;
    $("vzoom").value = String(vzoomToSlider(view.vzoom));
    $("vzoom").title = `Height zoom: ${Math.round(view.vzoom * 100)}% (Ctrl + Shift + mouse wheel; double-click for 100%)`;
    Hub.emit("viewChanged");
  }

  Hub.commands.zoomInVertical = () => setVZoom(view.vzoom * 1.25);
  Hub.commands.zoomOutVertical = () => setVZoom(view.vzoom / 1.25);
  Hub.commands.zoomResetVertical = () => setVZoom(1);

  Hub.commands.zoomIn = () => setZoom(view.zoom * 1.5, zoomAnchor());
  Hub.commands.zoomOut = () => setZoom(view.zoom / 1.5, zoomAnchor());
  Hub.commands.zoomReset = () => setZoom(DEFAULT_ZOOM, zoomAnchor());

  /** Zoom Full: the whole song in view, from its start to the end of its last clip. */
  Hub.commands.zoomFull = () => {
    let end = 0;
    for (const track of Hub.state.tracks)
      for (const clip of track.clips || []) end = Math.max(end, clip.start + clip.length);
    const box = lanes();
    const width = box.clientWidth || 800;
    setZoom((width * 0.95) / Math.max(placeOf(end), placeOf(map().ppqOfBar(4))), 0);
    box.scrollLeft = 0;
    drawArrangement();
  };

  Hub.commands.zoomToLoop = () => {
    const { t } = timing();
    const box = lanes();
    const width = box.clientWidth || 800;
    setZoom((width * 0.9) / Math.max(placeOf(t.loopEnd) - placeOf(t.loopStart), 0.25), 0);
    box.scrollLeft = Math.max(0, contentX(t.loopStart) - width * 0.05);
    drawArrangement();
  };

  // ---------------------------------------------------------------- clip commands
  const minimumClip = () => Math.max(snapLength(), 1 / 16);

  /** The selected clips with their tracks' places: what drags and commands work on. */
  function selectedInfo() {
    const info = [];
    Hub.state.tracks.forEach((track, index) => {
      for (const clip of track.clips || [])
        if (Hub.state.selectedClips.includes(clip.id))
          info.push({ id: clip.id, trackId: track.id, index, start: clip.start, length: clip.length, offset: clip.offset || 0, notes: clip.notes || [],
                      pitchBend: clip.pitchBend || [], aftertouch: clip.aftertouch || [],
                      audioFile: clip.audioFile || "", audioOffset: clip.audioOffset || 0, audioLength: clip.audioLength || 0,
                      gain: clip.gain || 0, fadeIn: clip.fadeIn || 0, fadeOut: clip.fadeOut || 0,
                      sourceFile: clip.sourceFile || "", sourceOffset: clip.sourceOffset || 0, sourceLength: clip.sourceLength || 0,
                      sourceTempo: clip.sourceTempo || 0, pitch: clip.pitch || 0,
                      reversed: Boolean(clip.reversed), followTempo: Boolean(clip.followTempo),
                      muted: Boolean(clip.muted), seed: clip.seed || 0, effect: clip.effect || null,
                      silence: clip.silence || null, warp: clip.warp || null, envelopes: clip.envelopes || [] });
    });
    return info;
  }

  function resizedShape(clip, d) {
    const offset = clip.offset || 0;
    const audioOffset = clip.audioOffset || 0;
    if (d.edge === "right") return { start: clip.start, length: Math.max(minimumClip(), clip.length + d.delta), offset, audioOffset };
    const start = Hub.clamp(clip.start + d.delta, 0, clip.start + clip.length - minimumClip());
    return { start, length: clip.start + clip.length - start, offset: offset + (start - clip.start),
             audioOffset: audioOffset + secondsBetween(clip.start, start) };
  }

  /** Sends one gesture's edits; clips it creates are selected when they arrive. */
  function sendEdits(edits, makesClips) {
    if (!edits.length) return;
    if (makesClips) clipIdsBefore = new Set(Hub.state.tracks.flatMap((t) => (t.clips || []).map((c) => c.id)));
    Hub.send("clipEdits", { edits });
  }

  Hub.commands.deleteClip = () => {
    const doomed = selectedInfo();
    if (!doomed.length) return;
    Hub.state.selectedClips = [];
    sendEdits(doomed.map((c) => ({ op: "delete", trackId: c.trackId, clipId: c.id })));
    drawArrangement();
  };

  Hub.commands.copyClips = () => {
    const chosen = selectedInfo();
    if (!chosen.length) return;
    const firstIndex = Math.min(...chosen.map((c) => c.index)), firstStart = Math.min(...chosen.map((c) => c.start));
    clipboard = { items: chosen.map((c) => ({ indexOffset: c.index - firstIndex, startOffset: c.start - firstStart,
                                              length: c.length, offset: c.offset, notes: c.notes, pitchBend: c.pitchBend, aftertouch: c.aftertouch,
                                              audioFile: c.audioFile, audioOffset: c.audioOffset, audioLength: c.audioLength, muted: c.muted,
                                              gain: c.gain, fadeIn: c.fadeIn, fadeOut: c.fadeOut, sourceFile: c.sourceFile,
                                              sourceOffset: c.sourceOffset, sourceLength: c.sourceLength, sourceTempo: c.sourceTempo,
                                              pitch: c.pitch, reversed: c.reversed, followTempo: c.followTempo,
                                              effectUid: c.effect ? c.effect.uid : 0, silence: c.silence || null, warp: c.warp || null,
                                              seed: c.seed || 0, envelopes: c.envelopes || [] })) };
  };

  Hub.commands.cutClips = () => {
    Hub.run("copyClips");
    Hub.run("deleteClip");
  };

  // Like Studio One: at the playhead, starting on the selected track.
  Hub.commands.pasteClips = () => {
    if (!clipboard || !Hub.state.tracks.length) return;
    pasteAt(Hub.songPosition(), Math.max(0, Hub.state.tracks.findIndex((t) => t.id === Hub.state.selected)));
  };

  // Right after the selection, on the same tracks.
  Hub.commands.duplicateClips = () => {
    const chosen = selectedInfo();
    if (!chosen.length) return;
    const span = Math.max(...chosen.map((c) => c.start + c.length)) - Math.min(...chosen.map((c) => c.start));
    sendEdits(chosen.map((c) => ({ op: "copy", trackId: c.trackId, clipId: c.id, toTrackId: c.trackId, start: c.start + span })), true);
  };

  // The selected clips under the playhead, or, with none selected, the selected track's.
  Hub.commands.splitAtPlayhead = () => {
    const at = Hub.songPosition();
    const under = (c) => at > c.start + 1e-9 && at < c.start + c.length - 1e-9;
    let targets = selectedInfo().filter(under);
    if (!Hub.state.selectedClips.length) {
      const track = Hub.trackById(Hub.state.selected);
      targets = track ? (track.clips || []).filter(under).map((c) => ({ id: c.id, trackId: track.id })) : [];
    }
    sendEdits(targets.map((c) => ({ op: "split", trackId: c.trackId, clipId: c.id, at, ...(zeroCrossingFor(c) ? { zeroCross: true } : {}) })));
  };

  /** Snap To Zero Crossings: an audio clip's cut or edge is moved by the engine to where its sound
      crosses zero (the Split tool's and Split at Playhead's cuts, a dragged edge). */
  function zeroCrossingFor(c) {
    if (!view.snapOn || !view.snapZero) return false;
    const found = Hub.findClip(c.id);
    return Boolean(found && found.clip.audioFile);
  }

  Hub.commands.selectAllClips = () => {
    Hub.state.selectedClips = Hub.state.tracks.flatMap((t) => (t.clips || []).map((c) => c.id));
    drawArrangement();
  };

  function setTool(tool) {
    if (!TOOLS.some(([name]) => name === tool)) return;
    stopListening();
    if (tool !== "range") rangeSel = null;
    view.tool = tool;
    splitHover = null;
    document.querySelectorAll("[data-arrange-tool]").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.arrangeTool === tool)));
    showToolCursor();
    drawArrangement();
  }

  /** The tool's own symbol as the pointer over the lanes (hub.css draws each); a clip's edges and
      handles say how they resize, for the Arrow. */
  function showToolCursor(override = "") {
    const area = lanes();
    if (area.dataset.tool !== view.tool) area.dataset.tool = view.tool;
    if (area.style.cursor !== override) area.style.cursor = override;
  }

  /** The Paint tool with a shape for envelopes (Studio One's Paint Tool menu). */
  function setShape(shape) {
    if (!SHAPES.some(([name]) => name === shape)) return;
    view.shape = shape;
    setTool("paint");
  }

  Hub.commands.deselectAllClips = () => {
    rangeSel = null;
    Hub.state.selectedClips = [];
    drawArrangement();
  };

  // ---------------------------------------------------------------- Range, Mute and Listen
  /** The Range tool's double-click: the clips in the range cut at its start and end. Each is cut
      at the end first (it keeps the left part), then at the start, so both cuts reach it. */
  function cutRange() {
    const r = rangeSel;
    if (!r) return;
    const edits = [];
    const trackIds = new Set();

    for (let index = r.top; index <= r.bottom; index++) {
      const track = Hub.state.tracks[index];
      if (!track) continue;
      if (track.frozen) { Hub.toast(frozenNote(track)); continue; }
      trackIds.add(track.id);
      for (const clip of track.clips || []) {
        const start = clip.start, end = clip.start + clip.length;
        if (end <= r.from + 1e-9 || start >= r.to - 1e-9) continue;
        const zero = zeroCrossingFor(clip) ? { zeroCross: true } : {};
        if (r.to > start + 1e-9 && r.to < end - 1e-9) edits.push({ op: "split", trackId: track.id, clipId: clip.id, at: r.to, ...zero });
        if (r.from > start + 1e-9 && r.from < end - 1e-9) edits.push({ op: "split", trackId: track.id, clipId: clip.id, at: r.from, ...zero });
      }
    }

    rangeSelect = { trackIds, from: r.from, to: r.to };
    rangeSel = null;
    if (edits.length) Hub.send("clipEdits", { edits, label: "Split at Range" });
    else selectRange();
    drawArrangement();
  }

  /** The clips inside the range that was cut become the selection. */
  function selectRange() {
    const r = rangeSelect;
    rangeSelect = null;
    if (!r) return;
    Hub.state.selectedClips = Hub.state.tracks.filter((track) => r.trackIds.has(track.id))
      .flatMap((track) => (track.clips || []).filter((c) => c.start >= r.from - 1e-6 && c.start + c.length <= r.to + 1e-6).map((c) => c.id));
  }

  /** The Mute tool: a clip muted or unmuted as the drag began (a selected one: the selection). */
  function muteClip(hit) {
    const d = clipDrag;
    const pool = d.first && Hub.state.selectedClips.includes(hit.clip.id) ? selectedInfo().map((c) => ({ id: c.id, trackId: c.trackId }))
                                                                          : [{ id: hit.clip.id, trackId: hit.track.id }];
    d.first = false;
    for (const c of pool) {
      const found = Hub.findClip(c.id);
      if (!found || found.track.frozen || d.ids.has(c.id)) continue;
      d.ids.set(c.id, c.trackId);
    }
  }

  /** Held down on a track: it alone, from the pointer's place. */
  function startListening(track, ppq) {
    stopListening();
    listening = { wasPlaying: Boolean(Hub.transportInfo().playing), from: Hub.songPosition(), trackId: track.id, soloed: false };
    if (!track.solo) {
      track.solo = true;
      listening.soloed = true;
      Hub.send("trackSettings", { id: track.id, solo: true });
    }
    Hub.locate(ppq);
    if (!listening.wasPlaying) Hub.run("play");
  }

  /** Let go: the song stops and goes back where it was (if it wasn't playing); the solo goes. */
  function stopListening() {
    if (!listening) return;
    const was = listening;
    listening = null;
    if (!was.wasPlaying) {
      if (Hub.transportInfo().playing) Hub.run("stop");
      Hub.locate(was.from);
    }
    if (was.soloed) {
      const track = Hub.trackById(was.trackId);
      if (track) track.solo = false;
      Hub.send("trackSettings", { id: was.trackId, solo: false });
    }
  }

  // ---------------------------------------------------------------- the right-click menu
  /** Paste at a place and track (the menu's), the copied clips keeping their distances. */
  function pasteAt(at, baseIndex) {
    if (!clipboard || !Hub.state.tracks.length) return;
    const tracks = Hub.state.tracks;
    sendEdits(clipboard.items.map((item) => ({
      op: "add", trackId: tracks[Math.min(tracks.length - 1, baseIndex + item.indexOffset)].id,
      start: at + item.startOffset, length: item.length, offset: item.offset, notes: item.notes,
      pitchBend: item.pitchBend || [], aftertouch: item.aftertouch || [], muted: Boolean(item.muted), seed: item.seed || 0,
      envelopes: item.envelopes || [],
      ...(item.audioFile ? { audioFile: item.audioFile, audioOffset: item.audioOffset, audioLength: item.audioLength,
                             gain: item.gain || 0, fadeIn: item.fadeIn || 0, fadeOut: item.fadeOut || 0,
                             sourceFile: item.sourceFile || "", sourceOffset: item.sourceOffset || 0, sourceLength: item.sourceLength || 0,
                             sourceTempo: item.sourceTempo || 0, pitch: item.pitch || 0,
                             reversed: Boolean(item.reversed), followTempo: Boolean(item.followTempo), effectUid: item.effectUid || 0,
                             silence: item.silence || null, warp: item.warp || null } : {})
    })), true);
  }

  /** Studio One's menu on the lanes' empty space: the tools (the Paint tool's shapes beside it),
      Cut, Copy, Paste (where it was clicked), Delete, Select All, Deselect All, Undo and Redo
      (named after their steps), each with its key in the map in use. */
  /** A clip's right-click starts with these (hub-blocks.js, hub-audioeditor.js): Cut, Copy, Paste
      (at the playhead, on the selected track, as Ctrl+V), Delete and Duplicate, for the selection. */
  function clipEditItems() {
    const key = (command) => (Hub.keys ? Hub.keys.label(command) : "");
    const any = Hub.state.selectedClips.length > 0;
    return [
      { label: "Cut", value: "clipEdit:cutClips", shortcut: key("cut"), disabled: !any },
      { label: "Copy", value: "clipEdit:copyClips", shortcut: key("copy"), disabled: !any },
      { label: "Paste", value: "clipEdit:pasteClips", shortcut: key("paste"), disabled: !(clipboard && clipboard.items.length) || !Hub.state.tracks.length,
        title: "At the playhead, on the selected track" },
      { label: "Delete", value: "clipEdit:deleteClip", shortcut: key("delete"), disabled: !any },
      { label: "Duplicate", value: "clipEdit:duplicateClips", shortcut: key("duplicate"), disabled: !any,
        title: "Right after the selection, on the same tracks" }
    ];
  }

  /** One of clipEditItems picked: true when it was one. */
  function runClipEdit(pick) {
    if (typeof pick !== "string" || !pick.startsWith("clipEdit:")) return false;
    Hub.run(pick.slice(9));
    return true;
  }

  function showMenu(event) {
    const p = lanePoint(event);
    const key = (command) => (Hub.keys ? Hub.keys.label(command) : "");
    const any = Hub.state.selectedClips.length > 0;
    const song = Hub.state.song || {};
    const shapeItems = shapeMenuItems(p);
    const items = shapeItems.concat(TOOLS.map(([tool, label, command]) => ({
      label, value: "tool:" + tool, icon: TOOL_ICONS[tool], checked: view.tool === tool, shortcut: key(command),
      submenu: tool === "paint" ? SHAPES.map(([shape, name, icon]) => ({ label: name, value: "shape:" + shape, icon, checked: view.shape === shape,
                                                                         title: "The Paint tool's shape for envelopes (Automation view)" })) : undefined
    })));

    items.push({ separator: true },
      { label: "Cut", value: "cut", shortcut: key("cut"), disabled: !any },
      { label: "Copy", value: "copy", shortcut: key("copy"), disabled: !any },
      { label: "Paste", value: "paste", shortcut: key("paste"), disabled: !(clipboard && clipboard.items.length) || !Hub.state.tracks.length },
      { label: "Delete", value: "delete", shortcut: key("delete"), disabled: !any },
      { separator: true },
      { label: "Load Media\u2026", value: "loadMedia", title: "Audio files from here: onto this audio track, or onto new tracks" },
      { separator: true },
      { label: "Select All", value: "selectAll", shortcut: key("selectAll"), disabled: !Hub.state.tracks.some((t) => (t.clips || []).length) },
      { label: "Deselect All", value: "deselectAll", shortcut: key("deselectAll"), disabled: !any && !rangeSel },
      { separator: true },
      { label: song.undo ? `Undo ${song.undo}` : "Undo", value: "undo", shortcut: key("undo"), disabled: !song.undo },
      { label: song.redo ? `Redo ${song.redo}` : "Redo", value: "redo", shortcut: key("redo"), disabled: !song.redo });

    Hub.popupMenu(Hub.pointAnchor(event), items, (pick) => {
      if (typeof pick !== "string") return;
      if (pick.startsWith("shape") && !pick.startsWith("shape:") && shapePick(pick, p, event)) return;
      if (pick.startsWith("tool:")) setTool(pick.slice(5));
      else if (pick.startsWith("shape:")) setShape(pick.slice(6));
      else if (pick === "cut") Hub.run("cutClips");
      else if (pick === "copy") Hub.run("copyClips");
      else if (pick === "paste") pasteAt(snap(p.ppq), Math.min(p.index, Hub.state.tracks.length - 1));
      else if (pick === "delete") Hub.run("deleteClip");
      else if (pick === "loadMedia" && Hub.importMedia) Hub.importMedia.chooseAt(event.clientX, event.clientY);
      else if (pick === "selectAll") Hub.run("selectAllClips");
      else if (pick === "deselectAll") Hub.run("deselectAllClips");
      else if (pick === "undo" || pick === "redo") Hub.run(pick);
    });
  }

  Hub.commands.toggleFollow = () => {
    view.follow = !view.follow;
    followPaused = false;
    $("follow").setAttribute("aria-pressed", String(view.follow));
    Hub.emit("viewChanged");
  };

  // ---------------------------------------------------------------- ruler: loop and song position
  function rulerPoint(event) {
    const rect = $("ruler").getBoundingClientRect();
    return { x: event.clientX - rect.left, y: event.clientY - rect.top, ppq: ppqAt(event.clientX) };
  }

  /** What a press at this point of the ruler would grab. */
  function rulerTarget(point) {
    if (point.y >= LOOP_LANE) return "locate";
    const { t } = timing();
    const x0 = xOf(t.loopStart), x1 = xOf(t.loopEnd);
    if (Math.abs(point.x - x1) <= EDGE) return "loop-end";
    if (Math.abs(point.x - x0) <= EDGE) return "loop-start";
    if (point.x > x0 && point.x < x1) return "loop-move";
    return "loop-draw";
  }

  const CURSORS = { "locate": "pointer", "loop-start": "ew-resize", "loop-end": "ew-resize", "loop-move": "grab", "loop-draw": "crosshair" };

  // ---------------------------------------------------------------- the second ruler
  /** Shows the second ruler in a timebase (or "off"). */
  function setSecondRuler(value) {
    view.ruler2 = TIMEBASES.some(([tb]) => tb === value) ? value : "off";
    const on = Boolean(secondRuler());
    document.body.classList.toggle("second-ruler", on);
    const button = $("second-ruler-toggle");
    if (button) {
      button.setAttribute("aria-pressed", String(on));
      const name = on ? (TIMEBASES.find(([tb]) => tb === view.ruler2) || [, ""])[1] : "";
      button.title = on ? `Second ruler: ${name}. Click to choose its timebase or hide it` : "Second ruler: a second timebase under the ruler";
    }
    requestAnimationFrame(drawArrangement);
  }

  function openSecondRulerMenu(anchor) {
    const items = [{ group: "Second Ruler" }, { label: "Off", value: "off", checked: !secondRuler() }];
    for (const [value, label, title] of TIMEBASES) items.push({ label, value, checked: view.ruler2 === value, title });
    Hub.popupMenu(anchor, items, (value) => {
      if (typeof value !== "string") return;
      setSecondRuler(value);
      Hub.emit("viewChanged");
    });
  }

  Hub.commands.toggleSecondRuler = () => {
    // Off: on in the timebase the main ruler isn't in (Seconds under Bars, Bars under the rest).
    setSecondRuler(secondRuler() ? "off" : view.lastRuler2 || (view.timebase === "bars" ? "seconds" : "bars"));
    if (secondRuler()) view.lastRuler2 = view.ruler2;
    Hub.emit("viewChanged");
  };

  function setupRuler() {
    const canvas = $("ruler");
    const minimum = () => Math.max(snapLength(), 0.0625);

    // A right-click on the second ruler: its timebase.
    canvas.addEventListener("contextmenu", (event) => {
      if (!secondRuler() || rulerPoint(event).y < MAIN_RULER) return;
      event.preventDefault();
      openSecondRulerMenu(Hub.pointAnchor(event));
    });

    const toggle = $("second-ruler-toggle");
    if (toggle) toggle.addEventListener("click", (event) => openSecondRulerMenu(event.currentTarget));

    canvas.addEventListener("pointerdown", (event) => {
      if (event.button !== 0) return;
      event.preventDefault();
      const point = rulerPoint(event);
      const kind = rulerTarget(point);
      const { t } = timing();
      if (canvas.setPointerCapture) canvas.setPointerCapture(event.pointerId);

      if (kind === "locate") {
        drag = { kind, pointer: event.pointerId };
        Hub.locate(snap(point.ppq));
      } else {
        drag = { kind, pointer: event.pointerId, x: point.x, grab: point.ppq, start: t.loopStart, end: t.loopEnd, moved: false, anchor: snap(point.ppq) };
        loopPreview = kind === "loop-draw" ? { start: drag.anchor, end: drag.anchor, on: true } : { start: t.loopStart, end: t.loopEnd, on: t.loop };
        if (kind === "loop-move") canvas.style.cursor = "grabbing";
      }

      drawArrangement();
    });

    canvas.addEventListener("pointermove", (event) => {
      const point = rulerPoint(event);

      if (!drag) {
        canvas.style.cursor = CURSORS[rulerTarget(point)];
        return;
      }

      if (event.pointerId !== drag.pointer) return;
      if (Math.abs(point.x - drag.x) > 3) drag.moved = true;

      if (drag.kind === "locate") {
        Hub.locate(snap(point.ppq));
      } else if (drag.kind === "loop-start") {
        loopPreview.start = Math.min(snap(point.ppq), loopPreview.end - minimum());
      } else if (drag.kind === "loop-end") {
        loopPreview.end = Math.max(snap(point.ppq), loopPreview.start + minimum());
      } else if (drag.kind === "loop-move") {
        // Its start lands on the grid; it keeps its length.
        const length = drag.end - drag.start;
        loopPreview.start = snap(drag.start + point.ppq - drag.grab);
        loopPreview.end = loopPreview.start + length;
      } else {
        const here = snap(point.ppq);
        loopPreview.start = Math.min(drag.anchor, here);
        loopPreview.end = Math.max(drag.anchor, here);
      }

      drawArrangement();
    });

    const finish = (event) => {
      if (!drag || event.pointerId !== drag.pointer) return;
      const done = drag;
      const loop = loopPreview;
      drag = null;
      loopPreview = null;
      canvas.style.cursor = CURSORS[rulerTarget(rulerPoint(event))];

      if (done.kind === "loop-move" && !done.moved) {
        Hub.run("toggleLoop");                           // a click on the loop switches it on or off
      } else if (done.kind === "loop-draw") {
        if (loop.end - loop.start >= minimum() - 1e-9) Hub.setLoop(loop.start, loop.end, true);
      } else if (done.kind !== "locate" && done.moved) {
        Hub.setLoop(loop.start, loop.end);
      }

      drawArrangement();
    };

    canvas.addEventListener("pointerup", finish);
    canvas.addEventListener("pointercancel", finish);
  }

  // ---------------------------------------------------------------- toolbar and footer
  function setupControls() {
    const quantize = $("quantize");
    for (const [label, value] of QUANTIZE) quantize.add(new Option(label, String(value), false, value === view.quantize));

    quantize.addEventListener("change", () => {
      view.quantize = Number(quantize.value);
      quantize.blur();
      drawArrangement();
      Hub.emit("viewChanged");
    });

    $("snap-menu").addEventListener("click", openSnapMenu);
    showSnapMode();

    $("snap-on").addEventListener("click", () => {
      setSnapOn(!view.snapOn);
      Hub.emit("viewChanged");
    });

    $("timebase-menu").addEventListener("click", openTimebaseMenu);
    showTimebase();

    $("follow").addEventListener("click", () => Hub.run("toggleFollow"));
    document.querySelectorAll("[data-arrange-tool]").forEach((b) => b.addEventListener("click", () => setTool(b.dataset.arrangeTool)));
    showToolCursor();

    $("auto-view").addEventListener("click", () => Hub.run("toggleAutomation"));
    $("zoom-in").addEventListener("click", () => Hub.run("zoomIn"));
    $("zoom-out").addEventListener("click", () => Hub.run("zoomOut"));
    $("zoom-loop").addEventListener("click", () => Hub.run("zoomToLoop"));

    const slider = $("zoom");
    slider.value = String(zoomToSlider(view.zoom));
    slider.addEventListener("input", () => setZoom(sliderToZoom(Number(slider.value)), zoomAnchor()));

    // The height zoom: its slider, - and +; a double-click on the slider goes back to 100%.
    const vslider = $("vzoom");
    vslider.value = String(vzoomToSlider(view.vzoom));
    vslider.addEventListener("input", () => setVZoom(sliderToVzoom(Number(vslider.value))));
    vslider.addEventListener("dblclick", () => setVZoom(1));
    $("vzoom-in").addEventListener("click", () => Hub.run("zoomInVertical"));
    $("vzoom-out").addEventListener("click", () => Hub.run("zoomOutVertical"));

    const size = $("track-size");
    size.value = view.size;
    size.addEventListener("change", () => {
      setTrackSize(size.value);
      size.blur();
      Hub.emit("viewChanged");
    });
  }

  // ---------------------------------------------------------------- playhead, autoscroll
  // Scrolled away by hand while playing (Track Options: Scrolling pauses Autoscroll), the view
  // stays where it was put until the playhead is in it again, or the song starts again.
  let followPaused = false;
  let followLeft = -1;          // where Autoscroll last put the view
  let wasPlaying = false;

  function scrolledByHand() {
    const box = lanes();
    if (!view.follow || !Hub.transportInfo().playing || Math.abs(box.scrollLeft - followLeft) < 1) return;
    if (!option("scrollPausesAutoscroll", true)) return;
    const x = contentX(Hub.songPosition());
    if (x < box.scrollLeft || x > box.scrollLeft + box.clientWidth) followPaused = true;
  }

  /** Brings a song position into the clips' view when it's outside it (Return to Zero, a bar
      step): put a little in from the left edge, as Autoscroll pages it. */
  function reveal(ppq) {
    const box = lanes();
    if (!box || box.clientWidth <= 0) return;
    followPaused = false;
    const x = contentX(Math.max(0, ppq));
    if (x < box.scrollLeft || x > box.scrollLeft + box.clientWidth * 0.95) {
      box.scrollLeft = Math.max(0, x - box.clientWidth * 0.05);
      followLeft = box.scrollLeft;
    }
  }

  function animate() {
    const box = lanes();
    const playing = Hub.transportInfo().playing;
    if (playing && !wasPlaying) followPaused = false;
    wasPlaying = playing;

    // Autoscroll pages the view when the playhead runs off its right edge.
    if (view.follow && playing && !drag && box.clientWidth > 0) {
      const x = contentX(Hub.songPosition());
      const inView = x >= box.scrollLeft && x <= box.scrollLeft + box.clientWidth * 0.95;
      if (followPaused && (inView || !option("scrollPausesAutoscroll", true))) followPaused = false;
      if (!followPaused && !inView) {
        box.scrollLeft = Math.max(0, x - box.clientWidth * 0.05);
        followLeft = box.scrollLeft;
      }
    }

    // A take growing on an armed audio track is redrawn as it grows.
    const t = Hub.transportInfo();
    if (t.recording && t.playing && Hub.state.tracks.some((track) => track.kind === "audio" && track.recordArm)) drawLanes();

    placePlayhead();
    requestAnimationFrame(animate);
  }

  // ---------------------------------------------------------------- editing clips in the lanes
  /** A pointer event in lane terms: content pixels, song position, track row. */
  function lanePoint(event) {
    const box = lanes();
    const r = box.getBoundingClientRect();
    const x = event.clientX - r.left + box.scrollLeft, y = event.clientY - r.top + box.scrollTop;
    const index = rowAt(y);
    return { x, y, ppq: ppqOfContentX(x), index, track: Hub.state.tracks[index] || null };
  }

  /** The clip under the pointer, and whether it's on an edge (where dragging resizes). */
  function hitClip(p) {
    const clip = p.track ? clipAt(p.track, p.ppq) : null;
    if (!clip) return null;
    const x0 = contentX(clip.start), x1 = contentX(clip.start + clip.length);
    const zone = Math.min(6, (x1 - x0) / 4);
    return { track: p.track, clip, edge: p.x - x0 <= zone ? "left" : x1 - p.x <= zone ? "right" : null };
  }

  function eraseAt(p) {
    const hit = hitClip(p);
    if (hit) clipDrag.ids.add(hit.clip.id);
  }

  function splitPoint(hit, p) {
    const at = snap(p.ppq);
    return at > hit.clip.start + 1e-9 && at < hit.clip.start + hit.clip.length - 1e-9 ? at : null;
  }

  function onLaneDown(event) {
    if (event.button !== 0 || event.target !== $("lanes-canvas")) return;
    const p = lanePoint(event);
    if (p.track) Hub.selectTrack(p.track.id, "keep");
    if (view.automation && view.tool !== "listen") {
      // A shape region's strip (the Arrow tool): drags it along, or its ends resize it.
      if (view.tool === "arrow" && p.track && heightOf(p.track)) {
        const lane = trackLane(p.track, p.index), hit = shapeHit(lane, p);
        if (hit) { shapeDown(event, lane, hit, p); return; }
      }
      if (view.tool !== "range" && view.tool !== "mute" && p.track) autoDown(event, p, trackLane(p.track, p.index));   // those two are for clips
      return;
    }
    // A clip's open automation: its body draws the lane (its header still moves it).
    const openLane = clipLane();
    if (openLane && ["arrow", "paint", "eraser"].includes(view.tool) && onClipLane(openLane, p)) {
      event.preventDefault();
      autoDown(event, p, openLane);
      return;
    }
    // A frozen track's clips stay as they are until it thaws (it can still be heard).
    if (p.track && p.track.frozen && view.tool !== "listen" && view.tool !== "range") {
      Hub.toast(frozenNote(p.track));
      return;
    }
    // A clip's icon (its effect, its Silence Areas): its menu, whatever the tool.
    const icon = clipIconAt(p);
    if (icon) {
      event.preventDefault();
      Hub.state.selectedClips = [icon.clip.id];
      drawArrangement();
      if (icon.kind === "fx") effectMenu(Hub.pointAnchor(event), p.track, icon.clip);
      else if (icon.kind === "auto") openClipAutomation(p.track, icon.clip);
      else if (Hub.silence) Hub.silence.menu(Hub.pointAnchor(event), p.track, icon.clip);
      return;
    }
    const hit = hitClip(p);
    const additive = event.shiftKey || event.ctrlKey || event.metaKey;
    if (lanes().setPointerCapture) lanes().setPointerCapture(event.pointerId);

    if (view.tool === "listen") {
      if (p.track) {
        clipDrag = { kind: "listen", pointer: event.pointerId };
        startListening(p.track, p.ppq);
      }
      return;
    } else if (view.tool === "range") {
      // A press in the range keeps it (its double-click cuts there); a drag draws a new one.
      const inside = rangeSel && p.index >= rangeSel.top && p.index <= rangeSel.bottom && p.ppq >= rangeSel.from && p.ppq <= rangeSel.to;
      const last = Math.max(0, Hub.state.tracks.length - 1);
      const index = Hub.clamp(p.index, 0, last);
      clipDrag = { kind: "range", pointer: event.pointerId, grab: p, index, from: snap(p.ppq), inside, moved: false };
    } else if (view.tool === "mute") {
      clipDrag = { kind: "mute", pointer: event.pointerId, state: hit ? !hit.clip.muted : true, ids: new Map(), first: true };
      if (hit) muteClip(hit);
    } else if (view.tool === "eraser") {
      clipDrag = { kind: "erase", pointer: event.pointerId, ids: new Set() };
      eraseAt(p);
    } else if (view.tool === "split") {
      const at = hit && splitPoint(hit, p);
      if (at !== null && at !== undefined) sendEdits([{ op: "split", trackId: hit.track.id, clipId: hit.clip.id, at, ...(zeroCrossingFor(hit.clip) ? { zeroCross: true } : {}) }]);
    } else if (view.tool === "paint" && !hit && p.track) {
      const from = snap(p.ppq);
      clipDrag = { kind: "paint", pointer: event.pointerId, trackId: p.track.id, index: p.index, from, to: from };
    } else if (hit && view.tool === "arrow" && audioHandleAt(hit, p)) {
      // An audio clip's fade or volume handle.
      Hub.state.selectedClips = [hit.clip.id];
      clipDrag = { kind: "audio", handle: audioHandleAt(hit, p), pointer: event.pointerId, grab: p, clipId: hit.clip.id, trackId: hit.track.id,
                   clip: hit.clip, gain: hit.clip.gain || 0, fadeIn: hit.clip.fadeIn || 0, fadeOut: hit.clip.fadeOut || 0, changed: false };
    } else if (hit) {
      // Arrow (and Paint on a clip): select, then move or resize.
      const ids = new Set(Hub.state.selectedClips);
      if (additive) {
        if (ids.has(hit.clip.id)) ids.delete(hit.clip.id);
        else ids.add(hit.clip.id);
        Hub.state.selectedClips = [...ids];
        if (!ids.has(hit.clip.id)) { drawArrangement(); return; }
      } else if (!ids.has(hit.clip.id)) {
        Hub.state.selectedClips = [hit.clip.id];
      } else {
        // Already selected: it becomes the one picked last (for the Editor's F2).
        Hub.state.selectedClips = [...Hub.state.selectedClips.filter((id) => id !== hit.clip.id), hit.clip.id];
      }
      clipDrag = { kind: hit.edge ? "resize" : "move", edge: hit.edge, pointer: event.pointerId, grab: p, anchor: hit.clip.id,
                   clips: selectedInfo(), dPpq: 0, dTracks: 0, delta: 0, copy: false, moved: false };
      // Cursor follows Edit Position: the song position goes to the clip clicked.
      if (option("cursorFollowsEdit", false) && !additive) Hub.locate(hit.clip.start);
    } else if (view.tool === "arrow") {
      if (!additive) Hub.state.selectedClips = [];
      clipDrag = { kind: "box", pointer: event.pointerId, from: p, to: p, base: new Set(Hub.state.selectedClips) };
    }

    drawArrangement();
  }

  function onLaneMove(event) {
    if (shapeDrag) {
      shapeMove(event);
      return;
    }
    if (autoDrag) {
      autoMove(event);
      return;
    }
    const p = lanePoint(event);
    const d = clipDrag;

    if (!d) {
      // A shape region's strip (Automation view): its ends resize it, the rest drags it.
      if (view.automation && view.tool === "arrow" && p.track && heightOf(p.track) && event.target === $("lanes-canvas")) {
        const hit = shapeHit(trackLane(p.track, p.index), p);
        if (hit) { showToolCursor(hit.part === "move" ? "grab" : "ew-resize"); return; }
        showToolCursor("");
      }
      const hit = event.target === $("lanes-canvas") ? hitClip(p) : null;
      if (hit && clipIconAt(p)) { showToolCursor("pointer"); return; }
      const openLane = clipLane();
      if (openLane && onClipLane(openLane, p) && ["arrow", "paint", "eraser"].includes(view.tool)) {
        showToolCursor(view.tool === "arrow" ? (pointIndexAt(openLane, p) >= 0 ? "move" : "crosshair") : "");
        return;
      }
      const handle = view.tool === "arrow" ? audioHandleAt(hit, p) : null;
      const hovered = hit && hit.clip.audioFile ? hit.clip.id : null;
      if (hovered !== hoverClip) { hoverClip = hovered; drawArrangement(); }
      if (handle) { showToolCursor(handle === "gain" ? "ns-resize" : "ew-resize"); return; }
      showToolCursor(view.tool === "arrow" && hit && hit.edge ? "ew-resize" : "");
      const hover = view.tool === "split" && hit ? splitPoint(hit, p) : null;
      const next = hover === null || hover === undefined ? null : { index: p.index, ppq: hover };
      if (JSON.stringify(next) !== JSON.stringify(splitHover)) { splitHover = next; drawArrangement(); }
      return;
    }

    if (event.pointerId !== d.pointer) return;

    if (d.kind === "audio") {
      const c = d.clip, length = c.audioLength || 0;
      if (d.handle === "gain") d.gain = Hub.clamp(Math.round(((c.gain || 0) + (d.grab.y - p.y) * (event.shiftKey ? 0.05 : 0.25)) * 10) / 10, -60, 12);
      if (d.handle === "fadeIn") d.fadeIn = Hub.clamp(secondsBetween(c.start, p.ppq), 0, length - d.fadeOut);
      if (d.handle === "fadeOut") d.fadeOut = Hub.clamp(secondsBetween(p.ppq, c.start + c.length), 0, length - d.fadeIn);
      d.changed = true;
    } else if (d.kind === "move") {
      if (Math.abs(p.x - d.grab.x) > 3 || Math.abs(p.y - d.grab.y) > 3) d.moved = true;
      const anchor = d.clips.find((c) => c.id === d.anchor);
      // The grabbed clip's start (or end) lands where Snap allows; the others keep their distance from it.
      const moving = new Set(d.clips.map((c) => c.id));
      d.dPpq = Math.max(moveDelta(anchor, p.ppq - d.grab.ppq, moving), -Math.min(...d.clips.map((c) => c.start)));
      const lowest = Math.min(...d.clips.map((c) => c.index)), highest = Math.max(...d.clips.map((c) => c.index));
      d.dTracks = Hub.clamp(p.index - d.grab.index, -lowest, Hub.state.tracks.length - 1 - highest);
      d.copy = event.altKey;                 // Alt held: a copy, as in Studio One
    } else if (d.kind === "resize") {
      const anchor = d.clips.find((c) => c.id === d.anchor);
      const resizing = new Set(d.clips.map((c) => c.id));
      if (d.edge === "right") {
        const end = Math.max(snap(anchor.start + anchor.length + p.ppq - d.grab.ppq, resizing), anchor.start + minimumClip());
        d.delta = end - (anchor.start + anchor.length);
      } else {
        const start = Hub.clamp(snap(anchor.start + p.ppq - d.grab.ppq, resizing), 0, anchor.start + anchor.length - minimumClip());
        d.delta = start - anchor.start;
      }
    } else if (d.kind === "box") {
      d.to = p;
      const x0 = Math.min(d.from.x, p.x), x1 = Math.max(d.from.x, p.x), y0 = Math.min(d.from.y, p.y), y1 = Math.max(d.from.y, p.y);
      const ids = new Set(d.base);
      Hub.state.tracks.forEach((track, index) => {
        const rowY0 = rowTop(index), rowY1 = rowY0 + heightOf(track);   // tracks can have heights of their own
        for (const clip of track.clips || []) {
          const cx0 = contentX(clip.start), cx1 = contentX(clip.start + clip.length);
          if (cx1 > x0 && cx0 < x1 && rowY1 > y0 && rowY0 < y1) ids.add(clip.id);
        }
      });
      Hub.state.selectedClips = [...ids];
    } else if (d.kind === "paint") {
      d.to = snap(p.ppq);
    } else if (d.kind === "erase") {
      eraseAt(p);
    } else if (d.kind === "mute") {
      const hit = hitClip(p);
      if (hit) muteClip(hit);
    } else if (d.kind === "range") {
      if (Math.abs(p.x - d.grab.x) > 3 || Math.abs(p.y - d.grab.y) > 3) d.moved = true;
      if (d.moved) {
        const index = Hub.clamp(p.index, 0, Math.max(0, Hub.state.tracks.length - 1));
        const to = snap(p.ppq);
        rangeSel = { top: Math.min(d.index, index), bottom: Math.max(d.index, index), from: Math.min(d.from, to), to: Math.max(d.from, to) };
      }
    } else if (d.kind === "listen") {
      return;
    }

    drawArrangement();
  }

  function onLaneUp(event) {
    if (shapeDrag) {
      shapeUp();
      return;
    }
    if (autoDrag) {
      autoUp();
      return;
    }
    const d = clipDrag;
    if (!d || event.pointerId !== d.pointer) return;
    clipDrag = null;
    const tracks = Hub.state.tracks;

    // Clips moved or resized on (or onto) a frozen track: they stay as they are (a copy from one is fine).
    const frozenTrack = d.kind === "move" || d.kind === "resize"
      ? d.clips.flatMap((c) => [d.kind === "move" && d.copy ? null : tracks[c.index], tracks[c.index + (d.kind === "move" ? d.dTracks : 0)]])
               .find((t) => t && t.frozen)
      : null;

    if (frozenTrack && (d.kind === "move" ? d.moved : Math.abs(d.delta) > 1e-9)) {
      Hub.toast(frozenNote(frozenTrack));
    } else if (d.kind === "audio" && d.changed) {
      sendEdits([{ op: "audio", trackId: d.trackId, clipId: d.clipId, gain: d.gain, fadeIn: d.fadeIn, fadeOut: d.fadeOut }]);
    } else if (d.kind === "move" && d.moved && (d.copy || Math.abs(d.dPpq) > 1e-9 || d.dTracks !== 0)) {
      sendEdits(d.clips.map((c) => ({ op: d.copy ? "copy" : "move", trackId: c.trackId, clipId: c.id,
                                      toTrackId: tracks[c.index + d.dTracks].id, start: c.start + d.dPpq })), d.copy);
      const anchor = d.clips.find((c) => c.id === d.anchor);
      if (anchor && option("cursorFollowsEdit", false)) Hub.locate(anchor.start + d.dPpq);
    } else if (d.kind === "resize" && Math.abs(d.delta) > 1e-9) {
      sendEdits(d.clips.map((c) => {
        const shape = resizedShape(c, d);
        return { op: "range", trackId: c.trackId, clipId: c.id, start: shape.start, length: shape.length,
                 ...(zeroCrossingFor(c) ? { zeroEdge: d.edge === "right" ? "end" : "start" } : {}) };
      }));
    } else if (d.kind === "paint") {
      // A click without a drag draws one bar.
      const start = Math.min(d.from, d.to), end = Math.max(d.from, d.to);
      const length = end - start >= minimumClip() ? end - start : map().barAt(start).length;
      sendEdits([{ op: "add", trackId: d.trackId, start, length, notes: [] }], true);
    } else if (d.kind === "erase" && d.ids.size) {
      const edits = [];
      for (const id of d.ids) {
        const found = Hub.findClip(id);
        if (found) edits.push({ op: "delete", trackId: found.track.id, clipId: id });
      }
      Hub.state.selectedClips = Hub.state.selectedClips.filter((id) => !d.ids.has(id));
      sendEdits(edits);
    } else if (d.kind === "mute") {
      // Each clip that changes, as one undo step.
      const edits = [];
      for (const [id, trackId] of d.ids) {
        const found = Hub.findClip(id);
        if (found && Boolean(found.clip.muted) !== d.state) {
          found.clip.muted = d.state;   // shown at once; the engine confirms
          edits.push({ op: "mute", trackId, clipId: id, muted: d.state });
        }
      }
      if (edits.length) {
        const many = edits.length > 1 ? " Clips" : " Clip";
        Hub.send("clipEdits", { edits, label: (d.state ? "Mute" : "Unmute") + many });
      }
    } else if (d.kind === "listen") {
      stopListening();
    } else if (d.kind === "box") {
      // Locate when clicked in empty space: a click (no box drawn) moves the song position there.
      if (option("locateOnEmptyClick", false) && Math.abs(d.to.x - d.from.x) < 3 && Math.abs(d.to.y - d.from.y) < 3)
        Hub.locate(snap(Math.max(0, d.from.ppq)));
    } else if (d.kind === "range") {
      // A click outside the range (no drag) lets it go; a drag made the new one.
      if (!d.moved && !d.inside) rangeSel = null;
      if (rangeSel && rangeSel.to - rangeSel.from < 1e-9) rangeSel = null;
    }

    drawArrangement();
  }

  function setupLanes() {
    const box = lanes();

    box.addEventListener("scroll", () => {
      scrolledByHand();
      $("tracklist").scrollTop = box.scrollTop;
      if (box.scrollLeft + box.clientWidth > $("lanes-size").offsetWidth - 200) updateSizes();
      else drawArrangement();
    });

    box.addEventListener("pointerdown", onLaneDown);
    box.addEventListener("pointermove", onLaneMove);
    box.addEventListener("pointerup", onLaneUp);
    box.addEventListener("pointercancel", onLaneUp);
    box.addEventListener("pointerleave", () => { if (splitHover) { splitHover = null; drawArrangement(); } });

    // Double-click a clip to open it in the Editor.
    box.addEventListener("dblclick", (event) => {
      if (view.automation) {
        // Double-click a shape region's strip: Edit Shape. A point: it goes.
        const p = lanePoint(event);
        const lane = p.track ? trackLane(p.track, p.index) : null;
        const onShape = lane ? shapeHit(lane, p) : null;
        if (onShape) { openShapePanel(lane, onShape.region.id, Hub.pointAnchor(event)); return; }
        const hit = lane ? pointIndexAt(lane, p) : -1;
        if (hit >= 0) commitLane(lane, lanePoints(lane).filter((_, i) => i !== hit));
        return;
      }
      // On a clip's open automation: a double-click removes a point (and opens nothing).
      const openLane = clipLane();
      const point = lanePoint(event);
      if (openLane && onClipLane(openLane, point)) {
        const hit = pointIndexAt(openLane, point);
        if (hit >= 0) commitLane(openLane, lanePoints(openLane).filter((_, i) => i !== hit));
        return;
      }
      // The Range tool: a double-click in the range cuts its clips at its start and end.
      if (view.tool === "range" && rangeSel) {
        const p = lanePoint(event);
        if (p.index >= rangeSel.top && p.index <= rangeSel.bottom && p.ppq >= rangeSel.from && p.ppq <= rangeSel.to) {
          cutRange();
          return;
        }
      }
      // A MIDI clip opens in the Editor, an audio clip in the Audio Editor (not a frozen track's).
      const track = trackAt(event.clientY);
      const clip = track && !track.frozen ? clipAt(track, ppqAt(event.clientX)) : null;
      if (clip && !clip.audioFile && Hub.openEditor) Hub.openEditor(track.id, clip.id);
      if (clip && clip.audioFile && Hub.openAudioEditor) Hub.openAudioEditor(track.id, clip.id);
    });

    // Right-click: on empty space (or anywhere in Automation view), Studio One's menu; on a
    // clip, the clip's own (hub-blocks.js, hub-audioeditor.js).
    box.addEventListener("contextmenu", (event) => {
      if (event.target !== $("lanes-canvas")) return;
      const p = lanePoint(event);
      const clip = p.track ? clipAt(p.track, p.ppq) : null;
      if (clip && !view.automation) return;
      event.preventDefault();
      Hub.state.focus = "arrange";
      showMenu(event);
    });

    // Ctrl + wheel zooms around the mouse, over the lanes and over the ruler; with Shift too, the
    // tracks' heights (over the lanes and the track list), around the mouse.
    for (const area of [box, $("ruler-wrap"), $("tracklist")]) {
      area.addEventListener("wheel", (event) => {
        if (!event.ctrlKey) return;
        const delta = event.deltaY || event.deltaX;   // Shift turns the wheel sideways in some browsers
        if (event.shiftKey) {
          event.preventDefault();
          event.stopImmediatePropagation();   // not the track list's own scrolling too
          setVZoom(view.vzoom * (delta < 0 ? 1.15 : 1 / 1.15), event.clientY - box.getBoundingClientRect().top);
          return;
        }
        if (area === $("tracklist")) return;
        event.preventDefault();
        setZoom(view.zoom * (delta < 0 ? 1.2 : 1 / 1.2), event.clientX - box.getBoundingClientRect().left);
      }, { passive: false, capture: area === $("tracklist") });
    }

    if (window.ResizeObserver) new ResizeObserver(updateSizes).observe(box);
    else window.addEventListener("resize", updateSizes);
  }

  function setTrackSize(value) {
    view.size = SIZES[value] ? value : "normal";
    $("track-size").value = view.size;
    document.body.classList.remove("tracks-tiny", "tracks-small", "tracks-large");
    if (view.size !== "normal") document.body.classList.add(`tracks-${view.size}`);
    Hub.emit("trackHeights");
    updateSizes();
  }

  /** Snap on or off: the magnet shows it, the grid's choice dims while it's off. */
  function setSnapOn(on) {
    view.snapOn = on;
    const button = $("snap-on");
    button.setAttribute("aria-pressed", String(on));
    button.title = on ? "Snap is on: what you drag lands on the grid" : "Snap is off: what you drag goes where you put it";
    $("snap-menu").classList.toggle("off", !on);
  }

  /** The view settings a song keeps. */
  function getView() {
    return { zoom: view.zoom, vzoom: view.vzoom, snap: view.snap, snapOn: view.snapOn, quantize: view.quantize, timebase: view.timebase, follow: view.follow, size: view.size,
             fps: view.fps, snapCursor: view.snapCursor, snapEvents: view.snapEvents, snapZero: view.snapZero, snapGrid: view.snapGrid,
             snapEnd: view.snapEnd, relativeGrid: view.relativeGrid, grid: view.grid, linear: view.linear,
             automation: view.automation, ruler2: view.ruler2 || "off", songChannel: Boolean(Hub.songLanes && Hub.songLanes.isShown()),
             songChannelView: Hub.songLanes ? Hub.songLanes.getView() : null };
  }

  /** A song's view settings (none, or bad values: the defaults), with the view back at the start. */
  function applyView(saved) {
    const v = Object.assign({}, saved || {});
    // Songs from before the magnet kept Off as a grid: snapping off, on Adaptive.
    view.snap = ["adaptive", "quantize", "bar", "frames"].includes(v.snap) ? v.snap : "adaptive";
    setSnapOn(v.snap !== "off" && v.snapOn !== false);
    view.fps = FRAME_RATES.includes(v.fps) ? v.fps : 30;
    for (const [key] of SNAP_OPTIONS) view[key] = typeof v[key] === "boolean" ? v[key] : key === "snapGrid";
    showSnapMode();
    view.quantize = QUANTIZE.some(([, value]) => value === v.quantize) ? v.quantize : 0.25;
    view.timebase = TIMEBASES.some(([value]) => value === v.timebase) ? v.timebase : "bars";
    view.grid = GRIDS.flat().some(([value]) => sameGrid(value, v.grid)) ? v.grid : "adaptive";
    view.linear = v.linear === "time" ? "time" : "beat";
    view.follow = v.follow !== false;
    setSecondRuler(v.ruler2);
    if ($("auto-view")) setAutomationView(v.automation === true);
    if (Hub.songLanes) {
      Hub.songLanes.applyView(v.songChannelView);
      Hub.songLanes.show(v.songChannel === true);
    }
    $("quantize").value = String(view.quantize);
    showTimebase();
    $("follow").setAttribute("aria-pressed", String(view.follow));
    view.vzoom = Number.isFinite(v.vzoom) ? Hub.clamp(v.vzoom, MIN_VZOOM, MAX_VZOOM) : 1;
    $("vzoom").value = String(vzoomToSlider(view.vzoom));
    setTrackSize(v.size);
    Hub.state.selectedClips = [];
    lanes().scrollTop = 0;
    lanes().scrollLeft = 0;
    setZoom(Number.isFinite(v.zoom) ? v.zoom : DEFAULT_ZOOM, 0);
  }

  // ---------------------------------------------------------------- shared with the track list and songs
  Hub.commands.toggleAutomation = () => setAutomationView(!view.automation);

  // Delete removes the selected envelope point in Automation View, before anything else.
  Hub.on("ready", () => {
    const previous = Hub.commands.deleteSelection;
    Hub.commands.deleteSelection = () => {
      if ((view.automation || clipAuto) && Hub.state.focus === "arrange" && deleteSelectedPoint()) return;
      if (previous) previous();
    };
  });

  Hub.arrange = {
    view,
    getView,
    applyView,
    ppqAt,
    autoTarget: autoKey,
    setAutoTarget: (trackId, key) => { autoTargets.set(trackId, key); autoSelected = null; drawArrangement(); },
    openClipAutomation,
    laneKey,
    trackHeight: () => scaled(SIZES[view.size]),
    heightOf,
    shownHeight,
    vzoom: () => view.vzoom,
    rowTop,
    trackAt,
    clipAt,
    updateSizes,
    redraw: drawArrangement,
    snap,
    setTool,
    setShape,
    xOf,
    gridStep,
    // An edit made elsewhere (Build Song) is about to add clips: they become the selection.
    expectNewClips: () => { clipIdsBefore = new Set(Hub.state.tracks.flatMap((t) => (t.clips || []).map((c) => c.id))); },
    zoom: () => view.zoom,
    // The timeline's places (Beat-Linear or Time-Linear): content pixels for a song position, and back.
    contentX,
    ppqOfContentX,
    pxPerPpq,
    scrollLeft: () => lanes().scrollLeft,
    reveal,
    setZoom,
    // A clip's own effect (the Audio menu, a Browser effect dropped on a clip: hub-tracks.js).
    effectMenu,
    setClipEffect,
    // A clip's right-click: Cut, Copy, Paste, Delete, Duplicate.
    clipEditItems,
    runClipEdit,
    effectDropAt
  };

  // Settings > Appearance: the lanes and the ruler follow it.
  Hub.on("theme", () => drawArrangement());

  let lastTiming = "";

  Hub.on("status", (s) => {
    const t = s.transport;
    if (t && t.recording && !wasRecording) recordStart = t.ppq;
    wasRecording = Boolean(t && t.recording);

    // Redraw when the loop changes (the song channel's map comes as "tempoMap"; the tempo where
    // the song is changes all through a ramp).
    const signature = t ? `${t.loop}|${t.loopStart}|${t.loopEnd}` : "";
    if (signature !== lastTiming) {
      lastTiming = signature;
      updateSizes();
    }
  });

  Hub.on("transportEdited", drawArrangement);
  Hub.on("tempoMap", () => updateSizes());

  // Clips that are gone can't stay selected; clips an edit just made become the selection.
  Hub.on("tracks", () => {
    const ids = Hub.state.tracks.flatMap((t) => (t.clips || []).map((c) => c.id));

    if (clipIdsBefore) {
      const fresh = ids.filter((id) => !clipIdsBefore.has(id));
      clipIdsBefore = null;
      if (fresh.length) Hub.state.selectedClips = fresh;
    }
    if (rangeSelect) selectRange();

    const alive = new Set(ids);
    Hub.state.selectedClips = Hub.state.selectedClips.filter((id) => alive.has(id));
  });
  Hub.on("layout", () => requestAnimationFrame(updateSizes));
  Hub.on("takeGrew", () => updateSizes());   // a take recording: its clips grew (the song may be longer)

  // An audio take's waveform as it's written: new peaks a few times a second, per track (a new
  // pass starts it again). Drawn over the growing take; gone once the take is a clip (takeWaves).
  Hub.on("takeWave", (data) => {
    for (const item of (data && data.tracks) || []) {
      const key = `take:${item.id}`;
      let wave = takeWaves.get(item.id);
      if (!wave || item.reset) {
        wave = { key, ppq: item.ppq, peaks: [] };
        takeWaves.set(item.id, wave);
      }
      wave.ppq = item.ppq;
      for (const value of item.peaks || []) wave.peaks.push(value);
      Hub.state.peaks[key] = { file: key, rate: item.rate, bucket: item.bucket, peaks: wave.peaks };
    }
  });
  Hub.on("tracks", () => {
    if (Hub.transportInfo().recording) return;
    for (const wave of takeWaves.values()) delete Hub.state.peaks[wave.key];
    takeWaves.clear();
  });

  Hub.on("ready", () => {
    setupControls();
    setupRuler();
    setupLanes();
    updateSizes();
    requestAnimationFrame(animate);
  });
})();
