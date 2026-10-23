// D:\workspace\Fanan_web_hub\src\hub-sampling.js
//
// The Sampling Studio (the top bar's selector, Ctrl+7): Sampler Arena's Sampling Module, ported
// with its layout, pop-ups, settings and modes, and Sonify's own besides.
//
// The face (Sampler Arena's node): the family name, Play/Stop, where it is ("Cs4  3/48"), the
// L/R meters, F (the folder) and the progress line; Sonify adds Resume, Make Instrument, Discard,
// Skip and Redo (played by the user), Try It and Loop Editor, and the note to play.
// Sampling: VSTi Sampling, Chord Sampling and Sample Builder; Chord Types... (chords); Don't Use
// Key Ranges, Ad Hoc Folder, Settings. Sonify's source row (the nodes, a hardware synth, or the
// user playing). The zone graph (14 px a key, a colour for each zone, its keys, root and
// layers, the yellow locators dragged to move the zones) over the keyboard (click picks a key,
// a drag picks more, a right-click plays it: the lower on the key, the louder); a picked key's E
// opens Edit Key at the key (Velocity Layers, Duration, Key Shift, Custom Name; the Builder's
// Sample Slots). Clear All Keys, the count; the nodes (hub-sampling-nodes.js: the VSTis and effects
// sampled, wired from the Sampling node to the Output) and the zone list.
// Presets: Save Preset, Load Preset (Sampler Arena's .smppreset), Set Folder.
// Loop Editor: Load SFZ Patch, Batch Find All, Ask Fanan AI, Save, Save As; the samples (a green
// dot: it loops), Play/Stop, Auto Find, < >, the score, XFade, Remove Loop; the stereo waveform
// (drag S and E), the status.
// SFZ to Blob (hub-sampling-blob.js): SFZs and their samples packed into blobs, the files the
// Plugins Creator Studio's samplers play from the disk.
// Settings: Sampler Arena's floating window, two columns, its sections by mode.
//
// Sonify does the work (MainSampling.cpp): Hub.send("sampling", {action ...}); it answers with
// sampling {kind: state | progress | exists | builderFiles | builderInfo | preset | notice | loop
// | loopPosition | tried | problem}.

(() => {
  "use strict";

  const { $, element } = Hub;
  const NAMES = ["C", "Cs", "D", "Ds", "E", "F", "Fs", "G", "Gs", "A", "As", "B"];
  const TIMES = "\u00d7";
  const noteName = (n) => NAMES[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1);
  const isBlack = (n) => [1, 3, 6, 8, 10].includes(((n % 12) + 12) % 12);
  const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
  const clone = (o) => JSON.parse(JSON.stringify(o));
  const send = (action, data = {}) => Hub.send("sampling", Object.assign({ action }, data));

  // Sampler Arena's 18 chord types: name, file suffix, notes.
  const CHORDS = [
    ["Major", "Maj", [0, 4, 7]], ["Minor", "Min", [0, 3, 7]], ["Diminished", "Dim", [0, 3, 6]], ["Augmented", "Aug", [0, 4, 8]],
    ["Sus2", "Sus2", [0, 2, 7]], ["Sus4", "Sus4", [0, 5, 7]], ["Dom 7th", "7", [0, 4, 7, 10]], ["Maj 7th", "Maj7", [0, 4, 7, 11]],
    ["Min 7th", "Min7", [0, 3, 7, 10]], ["Dim 7th", "Dim7", [0, 3, 6, 9]], ["Aug 7th", "Aug7", [0, 4, 8, 10]], ["Min/Maj 7", "mM7", [0, 3, 7, 11]],
    ["Dom 9th", "9", [0, 4, 7, 10, 14]], ["Maj 9th", "Maj9", [0, 4, 7, 11, 14]], ["Min 9th", "Min9", [0, 3, 7, 10, 14]], ["Add 9", "Add9", [0, 4, 7, 14]],
    ["6th", "6", [0, 4, 7, 9]], ["Min 6th", "Min6", [0, 3, 7, 9]]
  ];

  // Sampling::Settings, as Sonify keeps them (toVar).
  const DEFAULTS = {
    mode: "vsti", source: "nodes", trackId: 0, midiOutput: "", midiChannel: 1, family: "Sample",
    keys: [], boundaries: {}, noKeyRanges: false, fillGap: false,
    layers: [[1, 127]], samplingVelocity: 127, velocityOffset: 3, keyShift: 0,
    durationMode: "time", durationMs: 2000, durationBars: 1, timeSigNum: 4, timeSigDen: 4, bpmLayers: [],
    minChordKey: 48, maxChordKey: 72, chords: [], overrides: {}, customNames: {}, slots: {},
    silenceDb: -60, silenceMs: 500, fadeOut: true, fadeMs: 200, createSfz: false, autoLoop: false,
    bitDepth: 24, sampleRate: 0, channelMode: 0, speed: "realtime",
    roundRobins: 0, trim: false, normalize: false, triggerDb: -35
  };

  // The zone graph and keyboard (Sampler Arena's ZoneGraphAndKeyboard).
  const NOTE_W = 14, GRAPH_H = 160, KB_H = 130;
  const WHITE_W = NOTE_W * 12 / 7, BLACK_W = NOTE_W * 1.3, BLACK_H = KB_H * 0.6;
  const ZONE_COLOURS = [[80, 100, 60], [60, 90, 80], [90, 80, 60], [70, 70, 100], [100, 70, 70], [60, 100, 90], [90, 90, 60], [80, 60, 100]];

  const st = {
    shown: false,
    tab: "sampling",
    s: clone(DEFAULTS),
    loaded: false,
    native: null,          // Sonify's last word: folders, rate, MIDI outputs...
    session: null,         // the session as Sonify last gave it whole (settings, shots)
    live: null,            // its latest progress
    build: null,
    made: null,
    problem: "",
    level: [0, 0],
    hit: { white: new Map(), black: new Map(), e: new Map(), drags: [], zones: [] },
    drag: null,            // a locator, or keys being picked
    audition: -1, auditionAt: 0,
    pop: null,
    settings: null,        // the Settings window, once opened
    settingsAway: false,   // it was open when the studio was left: it comes back with it
    info: new Map(),       // a slot's file -> {seconds, channels, rate, ok}
    loop: null, loopPeaks: null, loopPos: -1, loopDrag: null,
    configTimer: 0, xfadeTimer: 0, dragSent: 0
  };

  // ---------------------------------------------------------------- the settings, as Sonify reads them
  function safeName(name) {
    let safe = "";
    for (const c of String(name || "").trim()) safe += (c < " " || c === "\u007f" || " :/\\*?\"<>|".includes(c)) ? "_" : c;
    while (safe.endsWith(".")) safe = safe.slice(0, -1) + "_";
    return safe || "Sample";
  }

  function plural(count, one, many) { return `${count} ${count === 1 ? one : many}`; }
  function keysOf(s) { return [...new Set(s.keys)].sort((a, b) => a - b); }
  function override(note) { return st.s.overrides[note] || null; }
  function layersFor(note) { const o = override(note); return o && o.vel && o.vel.length ? o.vel : st.s.layers; }
  function keyShiftFor(note) { const o = override(note); return o && typeof o.shift === "number" ? o.shift : st.s.keyShift; }
  function hasOverride(note) { const o = override(note); return Boolean(o && (o.vel || o.dur || typeof o.shift === "number")); }
  function slotsOf(note) { return st.s.slots[note] || []; }
  function hasAnything(note) { return hasOverride(note) || Boolean(st.s.customNames[note]) || slotsOf(note).length > 0; }
  function busyStates() { return ["running", "stopping", "stopped", "finishing"]; }
  function sessionState() { const live = st.live || st.session; return live ? live.state : ""; }
  function isBusy() { return busyStates().includes(sessionState()) || Boolean(st.build && !st.build.done); }

  function velocityOf(layers, layer) {
    const v = layers.length === 1 && layer[0] <= 1 && layer[1] >= 127 ? st.s.samplingVelocity : layer[1] - st.s.velocityOffset;
    return clamp(v, 1, 127);
  }

  function holdSecondsFor(note, bpm) {
    const o = override(note);
    const d = o && o.dur ? o.dur : { mode: st.s.durationMode, ms: st.s.durationMs, bars: st.s.durationBars };
    if (d.mode === "bars") return d.bars * (60 / Math.max(1, bpm)) * st.s.timeSigNum * (4 / Math.max(1, st.s.timeSigDen));
    return d.ms / 1000;
  }

  /** Sampler Arena's zones: from 3 below each key, the yellow locators applied. */
  function zonesFor(s) {
    const keys = keysOf(s);
    if (!keys.length) return [];
    if (s.noKeyRanges) return keys.map((k) => ({ root: k, low: k, high: k }));
    const zones = keys.map((k, i) => {
      let low = i === 0 ? (s.fillGap ? 0 : keys[0]) : k - 3;
      let high = i === keys.length - 1 ? (s.fillGap ? 127 : keys[keys.length - 1]) : keys[i + 1] - 3 - 1;
      low = Math.min(clamp(low, 0, 127), k);
      high = Math.max(clamp(high, 0, 127), k);
      return { root: k, low, high };
    });
    for (const [indexText, key] of Object.entries(s.boundaries || {})) {
      const index = Number(indexText);
      if (index > 0 && index < zones.length) { zones[index].low = key; zones[index - 1].high = key - 1; }
      else if (index === 0) zones[0].low = key;
      else if (index === -1) zones[zones.length - 1].high = key;
    }
    for (const z of zones) { z.low = clamp(z.low, 0, 127); z.high = clamp(z.high, z.low, 127); }
    return zones;
  }

  /** The zones as they'll be: the Builder plays each sample on its own key (Sampler Arena drew
      zones there that its builder never used). */
  function shownZones() {
    return zonesFor(st.s.mode === "builder" ? Object.assign({}, st.s, { noKeyRanges: true }) : st.s);
  }

  function chordRoots() {
    const lo = Math.min(st.s.minChordKey, st.s.maxChordKey), hi = Math.max(st.s.minChordKey, st.s.maxChordKey);
    const list = [];
    for (const type of [...st.s.chords].sort((a, b) => a - b)) {
      const chord = CHORDS[type];
      if (!chord) continue;
      for (let root = lo; root <= hi; root++) if (chord[2].every((i) => root + i >= lo && root + i <= hi)) list.push({ root, type });
    }
    return list;
  }

  /** How many samples Start will make. */
  function planCount() {
    const s = st.s;
    if (s.mode === "builder") return keysOf(s).reduce((sum, k) => sum + slotsOf(k).length, 0);
    const passes = Math.max(1, new Set(s.bpmLayers).size), takes = s.roundRobins >= 2 ? s.roundRobins : 1;
    if (s.mode === "chord") return chordRoots().reduce((sum, c) => sum + layersFor(c.root).length, 0) * passes * takes;
    return keysOf(s).reduce((sum, k) => sum + layersFor(k).length, 0) * passes * takes;
  }

  /** The name a key's sample gets without a custom name (what Custom Name starts from). */
  function defaultName(note) {
    if (st.s.mode === "chord") return safeName(st.s.family);
    const layers = layersFor(note);
    return `${safeName(st.s.family)}_${noteName(note)}_V${velocityOf(layers, layers[layers.length - 1])}.wav`;
  }

  // ---------------------------------------------------------------- keeping the choices
  function changed(what = "all") {
    st.problem = "";
    clearTimeout(st.configTimer);
    st.configTimer = setTimeout(() => send("config", { settings: st.s }), 400);
    if (!st.shown) return;
    if (what === "all" || what === "zones") { drawZones(); renderCount(); }
    if (what === "all") { renderModeRow(); renderSource(); renderFace(); renderChordHint(); renderSettings(); }
    if (what === "face") renderFace();
    if (what === "chord") { renderChordHint(); renderFace(); }
  }

  function takeSettings(settings) {
    const s = Object.assign(clone(DEFAULTS), settings || {});
    s.keys = Array.isArray(s.keys) ? s.keys.map(Number) : [];
    s.layers = Array.isArray(s.layers) && s.layers.length ? s.layers : [[1, 127]];
    for (const key of ["boundaries", "overrides", "customNames", "slots"]) if (!s[key] || typeof s[key] !== "object" || Array.isArray(s[key])) s[key] = {};
    st.s = s;
  }

  // ---------------------------------------------------------------- pop-ups (call-out boxes at their key)
  function closePop() {
    if (!st.pop) return;
    const pop = st.pop;
    st.pop = null;
    pop.el.remove();
    document.removeEventListener("pointerdown", pop.outside, true);
    window.removeEventListener("keydown", pop.escape, true);
    if (pop.onClose) pop.onClose();
  }

  function placePop(el, rect) {
    const w = el.offsetWidth, h = el.offsetHeight, cx = rect.left + rect.width / 2;
    let top = rect.bottom + 9, below = true;
    if (top + h > window.innerHeight - 4) { top = Math.max(4, rect.top - h - 9); below = false; }
    const left = clamp(cx - w / 2, 4, Math.max(4, window.innerWidth - w - 4));
    el.style.left = `${left}px`;
    el.style.top = `${top}px`;
    el.classList.toggle("below", below);
    el.classList.toggle("above", !below);
    el.style.setProperty("--arrow", `${clamp(cx - left, 12, w - 12)}px`);
  }

  /** A pop-up at that rectangle (the page's), built by build(el); refresh() builds it again. */
  function openPop(rect, build, className = "", onClose = null) {
    closePop();
    const el = element("div", "smp-pop " + className);
    el.setAttribute("role", "dialog");
    document.body.appendChild(el);
    const pop = { el, rect, build, onClose };
    pop.refresh = () => { el.textContent = ""; build(el); placePop(el, rect); };
    pop.outside = (event) => { if (!el.contains(event.target) && !(event.target.closest && event.target.closest(".popup-menu"))) closePop(); };
    pop.escape = (event) => { if (event.key === "Escape") { event.stopPropagation(); closePop(); } };
    st.pop = pop;
    pop.refresh();
    setTimeout(() => {
      if (st.pop !== pop) return;
      document.addEventListener("pointerdown", pop.outside, true);
      window.addEventListener("keydown", pop.escape, true);
    }, 0);
    return pop;
  }

  function button(text, className, title, onClick, colour) {
    const b = element("button", "smp-button " + (className || ""), text);
    b.type = "button";
    if (title) b.title = title;
    if (colour) b.style.background = colour;
    b.addEventListener("click", onClick);
    return b;
  }

  /** A slider with its value box on the right (Sampler Arena's). show/parse: the box's text. */
  function slider(min, max, step, value, onChange, options = {}) {
    const wrap = element("div", "smp-slider" + (options.wide ? " wide" : ""));
    const range = element("input");
    range.type = "range"; range.min = min; range.max = max; range.step = step; range.value = value;
    const box = element("input", "smp-value");
    box.type = "text";
    const show = options.show || ((v) => String(v) + (options.suffix || ""));
    const parse = options.parse || ((t) => Number(String(t).replace(/[^0-9.+-]/g, "")));
    box.value = show(Number(value));
    const set = (v, from) => {
      if (!Number.isFinite(v)) { box.value = show(Number(range.value)); return; }
      v = clamp(Math.round(v / step) * step, min, max);
      v = Number(v.toFixed(4));
      if (from !== "range") range.value = v;
      box.value = show(v);
      onChange(v);
    };
    range.addEventListener("input", () => set(Number(range.value), "range"));
    box.addEventListener("change", () => set(parse(box.value), "box"));
    box.addEventListener("keydown", (event) => { if (event.key === "Enter") box.blur(); event.stopPropagation(); });
    wrap.append(range, box);
    wrap.setValue = (v) => { range.value = v; box.value = show(Number(v)); };
    return wrap;
  }

  function parseKey(text) {
    const t = String(text).trim();
    if (/^-?\d+$/.test(t)) return Number(t);
    const m = /^([A-Ga-g])(#|s|b)?(-?\d+)$/.exec(t);
    if (!m) return NaN;
    const base = { c: 0, d: 2, e: 4, f: 5, g: 7, a: 9, b: 11 }[m[1].toLowerCase()];
    const shift = m[2] === "#" || m[2] === "s" ? 1 : m[2] === "b" ? -1 : 0;
    return (Number(m[3]) + 1) * 12 + base + shift;
  }

  // ---------------------------------------------------------------- Edit Key and its editors
  function keyRect(note) {
    const canvas = $("smp-zones");
    const r = canvas.getBoundingClientRect();
    const e = st.hit.e.get(note) || st.hit.white.get(note) || st.hit.black.get(note);
    if (!e) return { left: r.left + r.width / 2, top: r.top, width: 1, height: 1, bottom: r.top + 1 };
    return { left: r.left + e.x, top: r.top + e.y, width: e.w, height: e.h, bottom: r.top + e.y + e.h };
  }

  function editKey(note) {
    const mode = st.s.mode;
    const o = override(note);
    openPop(keyRect(note), (el) => {
      el.appendChild(element("div", "smp-pop-title", `Edit Key ${noteName(note)}`));
      const rows = element("div", "smp-pop-rows");
      const row = (label, colour, action) => {
        const b = element("button", "smp-pop-row", label);
        b.type = "button";
        b.style.background = colour;
        b.addEventListener("click", action);
        rows.appendChild(b);
      };
      // The Builder's samples carry their own velocities: no layers there (Sampler Arena showed them anyway).
      if (mode !== "builder") {
        const on = Boolean(o && o.vel);
        row(on ? "Velocity Layers (custom)" : "Velocity Layers", on ? "rgb(140,60,60)" : "rgb(70,40,40)", () => velocityEditor(note));
      }
      if (mode !== "builder") {
        const on = Boolean(o && o.dur);
        row(on ? "Duration (custom)" : "Duration", on ? "rgb(50,90,160)" : "rgb(35,50,90)", () => durationEditor(note));
      }
      if (mode !== "chord") {
        const on = Boolean(o && typeof o.shift === "number");
        row(on ? "Key Shift (custom)" : "Key Shift", on ? "rgb(150,130,40)" : "rgb(70,60,25)", () => keyShiftEditor(note));
      }
      if (mode !== "builder") {
        const on = Boolean(st.s.customNames[note]);
        row(on ? "Custom Name (set)" : "Custom Name", on ? "rgb(70,140,90)" : "rgb(30,70,45)", () => nameEditor(note));
      }
      if (mode === "builder") {
        const on = slotsOf(note).length > 0;
        row(on ? "Sample Slots (loaded)" : "Sample Slots", on ? "rgb(140,90,50)" : "rgb(70,50,30)", () => slotsEditor(note));
      }
      el.appendChild(rows);
    });
  }

  function setOverride(note, key, value) {
    const o = Object.assign({}, st.s.overrides[note] || {});
    if (value === undefined) delete o[key]; else o[key] = value;
    if (Object.keys(o).length) st.s.overrides[note] = o; else delete st.s.overrides[note];
  }

  /** Velocity Layers (Low - High): the global ones (note -1), or a key's own. */
  function velocityEditor(note, anchor) {
    const global = note < 0;
    let layers = clone(global ? st.s.layers : layersFor(note));
    const commit = () => {
      if (global) st.s.layers = clone(layers); else setOverride(note, "vel", clone(layers));
      changed();
    };
    openPop(anchor || keyRect(note), (el) => {
      el.style.width = "330px";
      el.appendChild(element("div", "smp-pop-title left", global ? "Velocity Layers (Low - High)" : `Velocity Layers (Low - High): ${noteName(note)}`));
      layers.forEach((layer, i) => {
        const line = element("div", "smp-pop-line");
        line.appendChild(element("span", "smp-pop-label", `L${i + 1}`));
        line.appendChild(slider(1, 127, 1, layer[0], (v) => { layers[i][0] = v; commit(); }));
        line.appendChild(slider(1, 127, 1, layer[1], (v) => { layers[i][1] = v; commit(); }));
        const x = button("X", "small", "Remove this layer", () => { if (layers.length > 1) { layers.splice(i, 1); commit(); st.pop.refresh(); } }, "darkred");
        line.appendChild(x);
        el.appendChild(line);
      });
      const foot = element("div", "smp-pop-line");
      foot.appendChild(button("+", "small", "Another layer, above the last", () => {
        const next = Math.min(127, layers.length ? layers[layers.length - 1][1] + 1 : 1);
        layers.push([next, 127]);
        commit();
        st.pop.refresh();
      }, "rgb(40,100,40)"));
      if (!global) {
        foot.appendChild(element("span", "spacer"));
        foot.appendChild(button("Reset to Global", "small", "This key uses the global layers again", () => {
          setOverride(note, "vel", undefined);
          layers = clone(st.s.layers);
          changed();
          st.pop.refresh();
        }, "rgb(80,60,60)"));
      }
      el.appendChild(foot);
    });
  }

  function durationEditor(note) {
    const o = override(note);
    let d = o && o.dur ? clone(o.dur) : { mode: st.s.durationMode, ms: st.s.durationMs, bars: st.s.durationBars };
    const commit = () => { setOverride(note, "dur", clone(d)); changed(); };
    openPop(keyRect(note), (el) => {
      el.style.width = "250px";
      const mode = button(d.mode === "bars" ? "Mode: BARS" : "Mode: TIME", "", "Hold the note for a time, or for bars at the tempo", () => {
        d.mode = d.mode === "bars" ? "time" : "bars";
        commit();
        st.pop.refresh();
      }, "rgb(60,80,60)");
      mode.style.width = "90px";
      el.appendChild(mode);
      const line = element("div", "smp-pop-line");
      line.style.marginTop = "10px";
      line.appendChild(d.mode === "bars"
        ? slider(0.25, 16, 0.25, d.bars, (v) => { d.bars = v; commit(); }, { suffix: " bars", wide: true })
        : slider(200, 30000, 100, d.ms, (v) => { d.ms = v; commit(); }, { suffix: " ms", wide: true }));
      el.appendChild(line);
      const foot = element("div", "smp-pop-line");
      foot.style.marginTop = "8px";
      foot.appendChild(button("Reset to Global", "small", "", () => {
        setOverride(note, "dur", undefined);
        d = { mode: st.s.durationMode, ms: st.s.durationMs, bars: st.s.durationBars };
        changed();
        st.pop.refresh();
      }, "rgb(80,60,60)"));
      el.appendChild(foot);
    });
  }

  function keyShiftEditor(note) {
    let shift = keyShiftFor(note);
    openPop(keyRect(note), (el) => {
      el.style.width = "250px";
      const info = element("div", "smp-pop-title left");
      info.style.color = "rgb(220,200,60)";
      const label = () => { const to = clamp(note + shift, 0, 127); info.textContent = `Key ${note} -> ${noteName(to)} (${to})`; };
      label();
      el.appendChild(info);
      el.appendChild(slider(-48, 48, 1, shift, (v) => { shift = v; setOverride(note, "shift", v); label(); changed(); }));
      const foot = element("div", "smp-pop-line");
      foot.style.marginTop = "8px";
      foot.appendChild(button("Reset to Global", "small", "", () => { setOverride(note, "shift", undefined); shift = st.s.keyShift; changed(); st.pop.refresh(); }, "rgb(80,60,60)"));
      el.appendChild(foot);
    });
  }

  function nameEditor(note) {
    const fallback = defaultName(note);
    openPop(keyRect(note), (el) => {
      el.style.width = "340px";
      const title = element("div", "smp-pop-title left", `Custom Name for ${noteName(note)}`);
      title.style.color = "rgb(120,220,140)";
      el.appendChild(title);
      el.appendChild(element("p", "smp-pop-hint", st.s.mode === "chord" ? "Chord mode: replaces family prefix only" : "Single-note mode: replaces the entire filename"));
      const input = element("input");
      input.type = "text";
      input.maxLength = 96;
      input.value = st.s.customNames[note] || fallback;
      input.addEventListener("keydown", (event) => { if (event.key === "Enter") apply.click(); event.stopPropagation(); });
      el.appendChild(input);
      const foot = element("div", "smp-pop-line");
      foot.style.marginTop = "10px";
      const apply = button("Apply", "", "", () => {
        const value = input.value.trim();
        if (!value || value === fallback) delete st.s.customNames[note]; else st.s.customNames[note] = value;
        changed();
        closePop();
      }, "rgb(40,100,55)");
      apply.style.width = "100px";
      foot.appendChild(apply);
      foot.appendChild(button("Revert to Default", "", "", () => { delete st.s.customNames[note]; input.value = fallback; changed(); }, "rgb(80,60,40)"));
      el.appendChild(foot);
      setTimeout(() => { input.focus(); input.select(); }, 0);
    });
  }

  // ---------------------------------------------------------------- the Sample Builder's slots
  function songClips() {
    const clips = [];
    for (const track of Hub.state.tracks || []) {
      if (track.kind !== "audio") continue;
      for (const clip of track.clips || []) if (clip.audioFile && !clip.missing) clips.push(Object.assign({}, clip, { trackName: track.name }));
    }
    return clips;
  }

  function fileStem(path) {
    const name = String(path).split(/[\\/]/).pop() || "";
    const dot = name.lastIndexOf(".");
    return dot > 0 ? name.slice(0, dot) : name;
  }

  function setSlot(note, row, slot) {
    const list = slotsOf(note).slice();
    const old = list[row];
    if (old) { slot.lovel = old.lovel; slot.hivel = old.hivel; }
    if (row >= list.length) list.push(slot); else list[row] = slot;
    st.s.slots[note] = list;
    changed();
    if (st.pop && st.pop.slotsNote === note) st.pop.refresh();
    send("builderInfo", { paths: [slot.path] });
  }

  function slotsEditor(note) {
    const pop = openPop(keyRect(note), (el) => {
      el.classList.add("smp-slots");
      el.appendChild(element("div", "smp-pop-title left", `Sample Slots - Key ${noteName(note)}`)).style.color = "rgb(220,180,60)";
      const list = element("div", "smp-slots-list");
      const slots = slotsOf(note);
      for (let row = 0; row <= slots.length; row++) {
        const slot = slots[row] || null;
        const line = element("div", "smp-slot");
        const load = button("Load Sample", "smp-slot-load", "Choose an audio file for this slot", () => send("builderLoad", { note, row }));
        const drop = element("div", "smp-slot-drop" + (slot ? " filled" : ""), slot ? slot.name + (slot.clip ? " (clip)" : "") : "or drag & drop here");
        if (slot) {
          const info = st.info.get(slot.path);
          drop.title = slot.path + (info && info.ok ? `\n${info.seconds.toFixed(2)} s, ${info.channels} ch, ${Math.round(info.rate)} Hz` : "");
        }
        const fromComputer = (event) => event.dataTransfer && Array.from(event.dataTransfer.types || []).includes("Files");
        const ask = () => {
          const now = Date.now();
          if (now - st.dragSent < 400) return;
          st.dragSent = now;
          const r = drop.getBoundingClientRect();
          send("builderDrag", { left: r.left, top: r.top, width: r.width, height: r.height, note, row });
        };
        drop.addEventListener("dragenter", (event) => { if (fromComputer(event)) { drop.classList.add("over"); ask(); } });
        drop.addEventListener("dragover", (event) => { if (fromComputer(event)) { event.preventDefault(); event.dataTransfer.dropEffect = "copy"; ask(); } });
        drop.addEventListener("dragleave", () => drop.classList.remove("over"));
        drop.addEventListener("drop", (event) => { if (fromComputer(event)) { event.preventDefault(); drop.classList.remove("over"); Hub.toast("Hold the file over the slot a moment before letting go: Sonify takes it there."); } });
        const clipButton = button("Clip...", "smp-slot-clip", "One of the song's audio clips (its window, gain and fades)", () => {
          const clips = songClips();
          const items = clips.length ? clips.map((c) => ({ label: `${c.fileName}  (${c.trackName}, ${(c.audioLength || 0).toFixed(2)} s)`, value: c }))
                                     : [{ label: "The song has no audio clips", value: null, disabled: true }];
          Hub.popupMenu(clipButton, items, (c) => {
            if (!c) return;
            setSlot(note, row, { path: c.audioFile, name: c.fileName || fileStem(c.audioFile), lovel: 0, hivel: 127,
                                 clip: { offset: c.audioOffset || 0, length: c.audioLength || 0, gainDb: c.gain || 0, fadeIn: c.fadeIn || 0, fadeOut: c.fadeOut || 0 } });
          });
        });
        const vel = element("div", "smp-slot-vel");
        vel.appendChild(element("div", "smp-slot-vel-title", "Velocity"));
        const pair = element("div", "smp-slot-vel-pair");
        const lo = slider(0, 127, 1, slot ? slot.lovel : 0, (v) => {
          if (!slot) return;
          slot.lovel = v;
          if (slot.hivel < v) { slot.hivel = v; hi.setValue(v); }
          changed();
        });
        const hi = slider(0, 127, 1, slot ? slot.hivel : 127, (v) => {
          if (!slot) return;
          slot.hivel = v;
          if (slot.lovel > v) { slot.lovel = v; lo.setValue(v); }
          changed();
        });
        pair.append(lo, hi);
        vel.appendChild(pair);
        const remove = button("X", "smp-slot-remove", "Take this sample out", () => {
          const now = slotsOf(note).slice();
          now.splice(row, 1);
          if (now.length) st.s.slots[note] = now; else delete st.s.slots[note];
          changed();
          st.pop.refresh();
        });
        remove.style.visibility = slot ? "visible" : "hidden";
        line.append(load, drop, clipButton, vel, remove);
        list.appendChild(line);
      }
      el.appendChild(list);
    }, "", () => drawZones());
    pop.slotsNote = note;
  }

  function chordTypesEditor() {
    openPop($("smp-chord-types").getBoundingClientRect(), (el) => {
      el.style.width = "220px";
      const top = element("div", "smp-pop-line");
      top.appendChild(button("Select All", "small", "", () => { st.s.chords = CHORDS.map((_, i) => i); changed(); st.pop.refresh(); }, "rgb(60,80,50)"));
      top.appendChild(button("Clear All", "small", "", () => { st.s.chords = []; changed(); st.pop.refresh(); }, "rgb(80,50,50)"));
      el.appendChild(top);
      CHORDS.forEach(([name], i) => {
        const label = element("label", "smp-pop-check");
        const box = element("input");
        box.type = "checkbox";
        box.checked = st.s.chords.includes(i);
        box.addEventListener("change", () => {
          const set = new Set(st.s.chords);
          if (box.checked) set.add(i); else set.delete(i);
          st.s.chords = [...set].sort((a, b) => a - b);
          changed();
        });
        label.append(box, document.createTextNode(name));
        el.appendChild(label);
      });
    });
  }

  function speedInfo(anchor) {
    const text = [
      "Sampling Speed Modes",
      "",
      "REAL-TIME (1" + TIMES + ")",
      "The sound is recorded as it plays, through your audio device. Works",
      "with every plugin (big streaming libraries too), with a synth",
      "outside the computer, and with you playing. Use this if you're unsure.",
      "",
      "TURBO (~5" + TIMES + ")",
      "The nodes are rendered away from the audio device, about 5 times",
      "as fast as the music. Friendly to most streaming-sample instruments:",
      "their disk streaming can still mostly keep up. They're silent on the",
      "outputs while it runs.",
      "",
      "UNLEASHED (as fast as it goes)",
      "No pacing. Best for synthesizers, drum hits and one-shots: anything",
      "that doesn't stream samples from disk.",
      "",
      "WARNING for Unleashed: sustained notes from disk-streaming libraries",
      "may glitch or go silent once their preloaded part runs out. Use Turbo",
      "instead, or raise the instrument's preload buffer.",
      "",
      "Turbo and Unleashed are for the nodes: a hardware synth and you",
      "playing are always sampled in real time."
    ].join("\n");
    openPop(anchor.getBoundingClientRect(), (el) => { el.appendChild(element("div", "smp-pop-text", text)); });
  }

  // ---------------------------------------------------------------- the zone graph and keyboard
  function rgb(c, a = 1) { return a === 1 ? `rgb(${c[0]},${c[1]},${c[2]})` : `rgba(${c[0]},${c[1]},${c[2]},${a})`; }
  function brighter(c, amount) { const k = 1 / (1 + amount); return c.map((v) => Math.round(255 - (255 - v) * k)); }

  /** Each planned key's progress in the session: how many of its samples are in, and whether it plays now. */
  function keyProgress() {
    const result = new Map();
    const session = st.session, live = st.live || session;
    if (!session || !session.shots || !live) return result;
    const marks = live.marks || "";
    session.shots.forEach((shot, i) => {
      const note = shot[0];
      const entry = result.get(note) || { total: 0, done: 0, now: false };
      entry.total++;
      if (marks[i] === "1" || marks[i] === "2") entry.done++;
      if (i === live.shot && ["running", "stopping"].includes(live.state)) entry.now = true;
      result.set(note, entry);
    });
    return result;
  }

  function drawZones() {
    const canvas = $("smp-zones");
    if (!canvas || !st.shown || st.tab !== "sampling" || st.s.mode === "chord") return;
    const width = 1800, height = GRAPH_H + KB_H, dpr = window.devicePixelRatio || 1;
    if (canvas.width !== Math.round(width * dpr)) { canvas.width = Math.round(width * dpr); canvas.height = Math.round(height * dpr); }
    const g = canvas.getContext("2d");
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, width, height);
    const s = st.s;
    const selected = new Set(s.keys);
    const hit = { white: new Map(), black: new Map(), e: new Map(), drags: [], zones: [] };
    const busy = isBusy();

    // The zone graph.
    g.fillStyle = "rgb(35,40,30)";
    g.fillRect(0, 0, width, GRAPH_H);
    g.fillStyle = "rgb(50,55,45)";
    for (let oct = 0; oct <= 10; oct++) g.fillRect(Math.round(oct * 12 * NOTE_W), 0, 1, GRAPH_H);
    const zones = shownZones();
    hit.zones = zones;
    g.textAlign = "center";
    g.textBaseline = "middle";
    zones.forEach((z, i) => {
      const x1 = z.low * NOTE_W, x2 = (z.high + 1) * NOTE_W, c = ZONE_COLOURS[i % 8];
      g.fillStyle = rgb(c, 0.45);
      g.fillRect(x1, 0, x2 - x1, GRAPH_H);
      g.strokeStyle = rgb(brighter(c, 0.3));
      g.lineWidth = 1;
      g.strokeRect(x1 + 0.5, 0.5, x2 - x1 - 1, GRAPH_H - 1);
      g.fillStyle = "rgb(200,200,200)";
      g.font = "9px sans-serif";
      g.fillText(`${z.low}-${z.high}`, (x1 + x2) / 2, 7, x2 - x1 - 4);
      g.fillStyle = "rgb(255,220,40)";
      g.font = "bold 11px sans-serif";
      g.fillText(`${noteName(z.root)} (${z.root})`, (x1 + x2) / 2, 21, x2 - x1);
      g.fillStyle = "rgba(255,255,255,0.5)";
      g.font = "9px sans-serif";
      const layers = s.mode === "builder" ? slotsOf(z.root).length : layersFor(z.root).length;
      g.fillText(s.mode === "builder" ? `x${layers} smp` : `x${layers} vel`, (x1 + x2) / 2, GRAPH_H - 8, x2 - x1);
      if (!s.noKeyRanges && s.mode !== "builder") {
        g.fillStyle = "rgb(255,220,40)";
        g.fillRect(x1 - 2, 0, 4, GRAPH_H);
        hit.drags.push({ zone: i, x: x1 });
        g.fillRect(x2 - 2, 0, 4, GRAPH_H);
        if (i === zones.length - 1) hit.drags.push({ zone: -1, x: x2 });
      }
    });
    if (!zones.length) {
      g.fillStyle = "rgb(80,80,80)";
      g.font = "11px sans-serif";
      g.fillText(s.mode === "builder" ? "Select keys on the keyboard below, then load samples with each key's E button" : "Select keys on the keyboard below to define sampling zones", width / 2, GRAPH_H / 2);
    }

    // The keyboard.
    const progress = s.mode !== "builder" && st.session ? keyProgress() : new Map();
    const whiteIndex = new Map();
    let wi = 0;
    const drawE = (note, cx, y, w, h) => {
      const bw = w * 1.4, bh = h * 1.6, rect = { x: cx - bw / 2, y, w: bw, h: bh };
      const on = hasAnything(note);
      g.fillStyle = on ? "rgb(220,180,60)" : "rgb(70,60,35)";
      g.beginPath();
      g.roundRect(rect.x, rect.y, rect.w, rect.h, 3);
      g.fill();
      g.fillStyle = on ? "#000" : "#fff";
      g.font = "bold 9px sans-serif";
      g.fillText("E", cx, y + bh / 2 + 0.5);
      hit.e.set(note, rect);
    };
    const progressMark = (note, x, y, w) => {
      const p = progress.get(note);
      if (!p) return;
      g.fillStyle = p.done >= p.total ? "rgb(20,110,40)" : p.done > 0 ? "rgb(220,180,60)" : "rgba(0,0,0,0)";
      g.fillRect(x + 2, y + 2, w - 4, 4);
    };
    for (let note = 0; note <= 127; note++) {
      if (isBlack(note)) continue;
      const x = wi * WHITE_W, y = GRAPH_H, picked = selected.has(note), p = progress.get(note);
      g.fillStyle = p && p.now ? "rgb(255,170,60)" : picked ? (hasOverride(note) ? "rgb(0,180,100)" : "rgb(80,200,80)") : "rgb(230,230,230)";
      g.fillRect(x, y, WHITE_W - 0.5, KB_H);
      g.strokeStyle = "rgb(80,80,80)";
      g.lineWidth = 0.5;
      g.strokeRect(x + 0.25, y + 0.25, WHITE_W - 1, KB_H - 0.5);
      if (note % 12 === 0) {
        g.fillStyle = "#000";
        g.font = `${Math.max(8, WHITE_W * 0.4)}px sans-serif`;
        g.fillText(`C${note / 12 - 1}`, x + WHITE_W / 2, y + KB_H - 7);
      }
      progressMark(note, x, y, WHITE_W);
      if (picked && !busy) {
        const bw = Math.min(WHITE_W * 0.7, 16), bh = 11, labelSpace = Math.min(14, KB_H * 0.14);
        drawE(note, x + WHITE_W / 2, y + KB_H - labelSpace - bh * 1.6 - 2, bw, bh);
      }
      hit.white.set(note, { x, y, w: WHITE_W - 0.5, h: KB_H });
      whiteIndex.set(note, wi);
      wi++;
    }
    for (let note = 0; note <= 127; note++) {
      if (!isBlack(note)) continue;
      const before = note - 1;
      if (!whiteIndex.has(before)) continue;
      const x = (whiteIndex.get(before) + 1) * WHITE_W - BLACK_W / 2, y = GRAPH_H, picked = selected.has(note), p = progress.get(note);
      g.fillStyle = p && p.now ? "rgb(230,140,40)" : picked ? (hasOverride(note) ? "rgb(0,140,80)" : "rgb(40,160,40)") : "rgb(25,25,25)";
      g.fillRect(x, y, BLACK_W, BLACK_H);
      g.strokeStyle = "rgb(10,10,10)";
      g.lineWidth = 0.5;
      g.strokeRect(x + 0.25, y + 0.25, BLACK_W - 0.5, BLACK_H - 0.5);
      progressMark(note, x, y, BLACK_W);
      if (picked && !busy) drawE(note, x + BLACK_W / 2, y + BLACK_H * 0.2, Math.min(BLACK_W * 0.85, 14), 10);
      hit.black.set(note, { x, y, w: BLACK_W, h: BLACK_H });
    }
    st.hit = hit;
  }

  function inRect(r, p) { return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h; }

  function keyAt(p) {
    for (const [note, r] of st.hit.black) if (inRect(r, p)) return note;
    for (const [note, r] of st.hit.white) if (inRect(r, p)) return note;
    return -1;
  }

  function locatorAt(x) {
    for (let i = 0; i < st.hit.drags.length; i++) if (Math.abs(x - st.hit.drags[i].x) < 6) return i;
    return -1;
  }

  function velocityFromY(y, note) {
    const r = st.hit.black.get(note) || st.hit.white.get(note);
    if (!r) return 64;
    return clamp(1 + Math.floor(clamp((y - r.y) / r.h, 0, 1) * 126), 1, 127);
  }

  function auditionSettings() {
    return { source: st.s.source, trackId: st.s.trackId, midiOutput: st.s.midiOutput, midiChannel: st.s.midiChannel };
  }

  function audition(note, velocity, on) {
    send("audition", { note, velocity, on, settings: auditionSettings() });
  }

  function toggleKey(note) {
    const keys = new Set(st.s.keys);
    if (keys.has(note)) {
      keys.delete(note);
      delete st.s.overrides[note];
      delete st.s.customNames[note];
    } else {
      keys.add(note);
    }
    st.s.keys = [...keys].sort((a, b) => a - b);
    st.s.boundaries = {};   // the zones change: the locators start again (Sampler Arena kept them on the wrong zones)
  }

  function wireZones() {
    const canvas = $("smp-zones");
    const local = (event) => { const r = canvas.getBoundingClientRect(); return { x: event.clientX - r.left, y: event.clientY - r.top }; };

    canvas.addEventListener("pointerdown", (event) => {
      const p = local(event);
      if (event.button === 2) {
        if (p.y < GRAPH_H) return;
        const note = keyAt(p);
        if (note < 0) return;
        event.preventDefault();
        canvas.setPointerCapture(event.pointerId);
        st.audition = note;
        st.auditionAt = performance.now();
        audition(note, velocityFromY(p.y, note), true);
        return;
      }
      if (event.button !== 0 || isBusy()) return;
      if (p.y < GRAPH_H) {
        const index = locatorAt(p.x);
        if (index >= 0) { st.drag = { kind: "locator", index }; canvas.setPointerCapture(event.pointerId); }
        return;
      }
      for (const [note, r] of st.hit.e) if (inRect(r, p)) { editKey(note); return; }
      const note = keyAt(p);
      if (note < 0) return;
      toggleKey(note);
      st.drag = { kind: "keys", last: note };
      canvas.setPointerCapture(event.pointerId);
      changed();
    });

    canvas.addEventListener("pointermove", (event) => {
      const p = local(event);
      if (st.audition >= 0 && (event.buttons & 2)) {
        const note = p.y >= GRAPH_H ? keyAt(p) : -1;
        // Sliding along the keys, as on a keyboard (at most a note every 50 ms).
        if (note >= 0 && note !== st.audition && performance.now() - st.auditionAt >= 50) {
          audition(st.audition, 0, false);
          st.audition = note;
          st.auditionAt = performance.now();
          audition(note, velocityFromY(p.y, note), true);
        }
        return;
      }
      const drag = st.drag;
      if (drag && drag.kind === "locator") {
        const d = st.hit.drags[drag.index], zones = st.hit.zones;
        if (!d || !zones.length) return;
        const s = st.s;
        if (d.zone > 0 && d.zone < zones.length) {
          s.boundaries[d.zone] = clamp(Math.floor(p.x / NOTE_W), zones[d.zone - 1].root + 1, zones[d.zone].root - 1);
        } else if (d.zone === 0) {
          s.boundaries[0] = clamp(Math.floor(p.x / NOTE_W), 0, Math.min(zones[0].root, zones[zones.length - 1].high));
        } else if (d.zone === -1) {
          s.boundaries[-1] = clamp(clamp(Math.floor(p.x / NOTE_W), 0, 128) - 1, Math.max(zones[0].low, zones[zones.length - 1].root), 127);
        }
        changed("zones");
        return;
      }
      if (drag && drag.kind === "keys" && p.y >= GRAPH_H) {
        const note = keyAt(p);
        if (note >= 0 && note !== drag.last && !st.s.keys.includes(note)) {
          st.s.keys = [...st.s.keys, note].sort((a, b) => a - b);
          st.s.boundaries = {};
          drag.last = note;
          changed();
        }
        return;
      }
      canvas.style.cursor = p.y < GRAPH_H && !isBusy() && locatorAt(p.x) >= 0 ? "ew-resize" : "default";
    });

    const end = () => {
      if (st.audition >= 0) { audition(st.audition, 0, false); st.audition = -1; }
      st.drag = null;
    };
    canvas.addEventListener("pointerup", end);
    canvas.addEventListener("pointercancel", end);
    canvas.addEventListener("contextmenu", (event) => event.preventDefault());
  }

  // ---------------------------------------------------------------- the face
  function setMeter(left, right) {
    st.level = [Math.max(left, st.level[0] * 0.8), Math.max(right, st.level[1] * 0.8)];
    const put = (id, level) => {
      const el = $(id);
      const v = clamp(level, 0, 1);
      el.style.height = `${Math.round(v * 100)}%`;
      el.classList.toggle("hot", v >= 0.7 && v < 0.9);
      el.classList.toggle("over", v >= 0.9);
    };
    put("smp-level-l", st.level[0]);
    put("smp-level-r", st.level[1]);
  }

  function shotLabel(shot) {
    if (!shot) return "";
    const chord = shot[6];
    return chord ? `${noteName(shot[0]).replace(/-?\d+$/, "")} ${chord}` : noteName(shot[0]);
  }

  function renderFace() {
    if (!st.shown) return;
    const family = $("smp-family");
    if (document.activeElement !== family) family.value = st.s.family;
    const status = $("smp-face-status"), actions = $("smp-face-actions"), play = $("smp-play"), prompt = $("smp-prompt");
    const live = st.live || st.session;
    const state = live ? live.state : "";
    const builder = st.s.mode === "builder";
    const building = Boolean(st.build && !st.build.done);
    status.className = "smp-face-status";
    actions.textContent = "";
    prompt.hidden = true;
    let progress = 0;

    const running = ["running", "stopping"].includes(state);
    // The nodes: Sample waits for a VSTi whose chain reaches the Output.
    const unwired = !builder && st.s.source === "nodes" && !running && state !== "stopped"
                    && !(Hub.samplingNodes && Hub.samplingNodes.isReady());
    play.classList.toggle("running", running);
    play.disabled = state === "finishing" || building || unwired;
    play.title = running ? "Stop: the note playing is let go and thrown away; what's recorded is kept"
               : state === "stopped" ? "Resume: on from the first sample not recorded"
               : unwired ? "Wire the Sampling node's MIDI to a VSTi, and its chain to the Output, first"
               : builder ? "Build the instrument from the slots" : "Start: sample every key picked";

    if (building) {
      status.textContent = `Building ${st.build.written || 0}/${planCount()}`;
      status.classList.add("running");
      progress = st.build.progress || 0;
    } else if (st.problem) {
      status.textContent = st.problem;
      status.classList.add("problem");
    } else if (live && running) {
      const shot = st.session && st.session.shots ? st.session.shots[live.shot] : null;
      status.textContent = state === "stopping" ? "Stopping..." : `${shotLabel(shot)}  ${Math.min(live.total, live.shot + 1)}/${live.total}` + (live.offline ? `  ${st.session.settings.speed === "turbo" ? "Turbo" : "Unleashed"}` : "");
      status.classList.add("running");
      progress = live.total ? live.recorded / live.total : 0;
      actions.appendChild(button("Make Instrument", "small", "Stop, and make the samples from what's recorded so far", () => send("finish"), "rgb(50,100,70)"));
      if (st.session.settings.source === "voice") {
        actions.appendChild(button("Skip", "small", "Leave this note out", () => send("skip")));
        actions.appendChild(button("Redo", "small", "Play the last note again", () => send("redo")));
        if (shot && ["waiting", "recording"].includes(live.step)) {
          const layers = st.session.shots.filter((s) => s[0] === shot[0] && s[4] === shot[4] && s[3] === shot[3]).length;
          prompt.hidden = false;
          prompt.textContent = "";
          prompt.appendChild(document.createTextNode(live.step === "recording" ? `Recording ${shotLabel(shot)}...` : `Play ${shotLabel(shot)} now`));
          prompt.appendChild(element("small", "", `velocity about ${shot[1]}` + (layers > 1 ? `, layer ${shot[2] + 1} of ${layers}` : "") + (shot[3] ? `, take ${shot[3]}` : "")));
        }
      }
    } else if (live && state === "stopped") {
      status.textContent = `Stopped  ${live.recorded}/${live.total}`;
      status.classList.add("running");
      progress = live.total ? live.recorded / live.total : 0;
      actions.appendChild(button("Make Instrument", "small", "Make the samples from what's recorded", () => send("finish"), "rgb(50,100,70)"));
      actions.appendChild(button("Discard", "small", "Forget this session (its takes go)", () => send("discard"), "rgb(100,40,40)"));
    } else if (live && state === "finishing") {
      status.textContent = `Writing ${live.written || 0}/${live.recorded}...`;
      status.classList.add("running");
      progress = live.progress || 0;
    } else if (live && state === "failed") {
      status.textContent = live.message || "Sampling failed.";
      status.classList.add("problem");
      actions.appendChild(button("Discard", "small", "", () => send("discard"), "rgb(100,40,40)"));
    } else if ((live && state === "done") || (st.build && st.build.done)) {
      const built = st.build && st.build.done && (!live || state !== "done");
      if (built && st.build.problem) {
        status.textContent = st.build.problem;
        status.classList.add("problem");
      } else {
        const count = built ? st.build.written : live.written;
        status.textContent = `Done: ${plural(count, "sample", "samples")}` + (!built && live.silent ? ` (${live.silent} silent, left out)` : "")
          + (!built && live.message ? `. ${live.message}` : "");
        status.classList.add("done");
        progress = 1;
        if (st.made && st.made.sfz) {
          actions.appendChild(button("Try It on a New Track", "small", "A new instrument track with a Sonify Sampler playing it (Edit > Undo takes it back)", () => send("tryIt", { sfz: st.made.sfz }), "rgb(50,100,70)"));
          actions.appendChild(button("Loop Editor", "small", "Open it in the Loop Editor", () => { showTab("loop"); send("loopOpen", { path: st.made.sfz }); }));
        }
      }
    } else if (builder) {
      const count = planCount();
      status.textContent = count ? `${plural(count, "sample", "samples")} on ${plural(keysOf(st.s).filter((k) => slotsOf(k).length).length, "key", "keys")}` : "Press E on a key to load samples";
    } else if (st.s.mode === "chord") {
      const count = planCount();
      status.textContent = count ? `${plural(chordRoots().length, "chord", "chords")}, ${plural(count, "sample", "samples")}` : "Pick Chord Types...";
    } else {
      const keys = keysOf(st.s).length;
      status.textContent = keys ? `${keys} keys selected` : "Click keys to pick them";
    }
    $("smp-face-progress").style.width = `${Math.round(clamp(progress, 0, 1) * 100)}%`;
    $("smp-folder").title = `F: open the folder the samples go to (${st.native ? st.native.folder : ""})`;
  }

  function start(replace = false) {
    st.problem = "";
    send("start", { settings: st.s, replace });
  }

  // ---------------------------------------------------------------- the mode row, the source
  function renderModeRow() {
    const busy = isBusy();
    for (const b of document.querySelectorAll(".smp-mode")) {
      b.classList.toggle("on", b.dataset.mode === st.s.mode);
      b.disabled = busy;
    }
    $("smp-chord-types").hidden = st.s.mode !== "chord";
    $("smp-no-ranges").classList.toggle("on", st.s.noKeyRanges);
    $("smp-no-ranges").hidden = st.s.mode === "builder";
    const adHoc = st.native && st.native.adHoc;
    $("smp-adhoc").textContent = adHoc ? `Ad Hoc: ${adHoc.split(/[\\/]/).pop()}` : "Ad Hoc Folder";
    $("smp-adhoc").title = adHoc ? `For now the samples go to ${adHoc}` : "For now, the samples go to another folder (Set Folder, in Presets, stays as it is)";
    const chord = st.s.mode === "chord";
    $("smp-zone-wrap").hidden = chord;
    $("smp-clear-row").hidden = chord;
    $("smp-chord-hint").hidden = !chord;
    $("smp-source-row").hidden = st.s.mode === "builder";
    const nodesOn = st.shown && st.tab === "sampling" && st.s.mode !== "builder" && st.s.source === "nodes";
    if (Hub.samplingNodes) Hub.samplingNodes.setVisible(nodesOn);
    // Beside them: Sampled Keys (not for chords) and the MIDI Monitor (hub-sampling-midi.js).
    if (Hub.samplingMidi) Hub.samplingMidi.layout(!chord, nodesOn);
  }

  /** The audio tracks a synth or the user comes in on. */
  function tracksFor() {
    return (Hub.state.tracks || []).filter((t) => t.kind === "audio");
  }

  function fillSelect(select, items, value) {
    select.textContent = "";
    for (const [label, v] of items) {
      const option = element("option", "", label);
      option.value = String(v);
      select.appendChild(option);
    }
    select.value = String(value);
  }

  function renderSource() {
    if (!st.shown) return;
    const s = st.s, busy = isBusy();
    for (const b of $("smp-source").querySelectorAll("button")) {
      b.classList.toggle("on", b.dataset.source === s.source);
      b.setAttribute("aria-checked", String(b.dataset.source === s.source));
      b.disabled = busy;
    }
    for (const el of $("smp-source-row").querySelectorAll("[data-sources]")) el.hidden = !el.dataset.sources.split(" ").includes(s.source);
    const tracks = tracksFor();
    if (s.source !== "nodes" && !tracks.some((t) => t.id === s.trackId)) {
      const picked = tracks.find((t) => t.id === Hub.state.selected);
      const id = (picked || tracks[0] || { id: 0 }).id;
      if (id !== s.trackId) { s.trackId = id; clearTimeout(st.configTimer); st.configTimer = setTimeout(() => send("config", { settings: st.s }), 400); }
    }
    const items = tracks.map((t) => [`${t.number}  ${t.name}`, t.id]);
    if (!items.length) items.push(["No audio track: add one in the Studio", 0]);
    fillSelect($("smp-track"), items, s.trackId);
    $("smp-track").disabled = busy;
    const outputs = (st.native && st.native.midiOutputs) || [];
    if (!outputs.some((o) => o.id === s.midiOutput)) s.midiOutput = (st.native && st.native.synthPort) || (outputs[0] ? outputs[0].id : "");
    const outItems = outputs.map((o) => [o.name, o.id]);
    if (!outItems.length) outItems.push(["No MIDI output on this computer", ""]);
    fillSelect($("smp-midi-out"), outItems, s.midiOutput);
    fillSelect($("smp-midi-channel"), Array.from({ length: 16 }, (_, i) => [String(i + 1), i + 1]), s.midiChannel);
    $("smp-midi-out").disabled = busy;
    $("smp-midi-channel").disabled = busy;
    const hints = {
      nodes: "The VSTis wired below (flagged) play each note, through their effects, into the Output. Right-click a key to hear it.",
      hardware: "The notes go out of the MIDI port; the audio track's input (where the synth comes in) is recorded. Monitor the track to hear it.",
      voice: "Play each note as it shows: Sonify records it from where it starts until it dies away. Monitor the track to hear yourself."
    };
    $("smp-source-hint").textContent = (hints[s.source] || "") + (s.source !== "nodes" && s.speed !== "realtime" ? " (Always in real time.)" : "");
  }

  function renderChordHint() {
    if (st.s.mode !== "chord") return;
    const box = $("smp-chord-hint");
    box.textContent = "";
    const types = [...st.s.chords].sort((a, b) => a - b).map((i) => CHORDS[i] ? CHORDS[i][0] : "").filter(Boolean);
    const roots = chordRoots();
    const lo = Math.min(st.s.minChordKey, st.s.maxChordKey), hi = Math.max(st.s.minChordKey, st.s.maxChordKey);
    const line = (html) => { const p = element("div"); p.innerHTML = html; box.appendChild(p); };
    line(`<b>Chord Sampling</b>: every chord type picked is played on each root of the key range (${noteName(lo)} to ${noteName(hi)}, in Settings) whose notes all fit in it.`);
    line(types.length ? `Chord types: ${types.join(", ")}.` : "No chord types picked yet: Chord Types...");
    const takes = st.s.roundRobins >= 2 ? ` ${TIMES} ${st.s.roundRobins} takes` : "";
    line(`${plural(roots.length, "chord", "chords")}, each at its velocity layers` + (st.s.bpmLayers.length ? ` ${TIMES} ${plural(st.s.bpmLayers.length, "BPM layer", "BPM layers")}` : "") + takes + ` = ${plural(planCount(), "sample", "samples")}. The WAVs go in ${safeName(st.s.family)}\\samples (no SFZ for chords).`);
  }

  function renderCount() {
    if (!st.shown) return;
    const count = planCount();
    $("smp-count").textContent = `${count} samples`;
    const zones = shownZones();
    const lines = zones.map((z) => {
      const n = st.s.mode === "builder" ? `x${slotsOf(z.root).length} smp` : `x${layersFor(z.root).length} vel`;
      return `${noteName(z.root)} (${z.root})  keys ${z.low}-${z.high}  ${n}`;
    });
    $("smp-zone-list").textContent = lines.length ? lines.join("\n") : "No keys selected.";
    renderFace();
  }

  // ---------------------------------------------------------------- the Settings window
  function section(col, first) {
    if (!first) col.appendChild(element("div", "smp-sep"));
  }

  function row(labelText, control) {
    const r = element("div", "smp-row");
    r.appendChild(element("span", "smp-row-label", labelText));
    r.appendChild(control);
    return r;
  }

  function select(items, value, onChange) {
    const sel = element("select");
    fillSelect(sel, items, value);
    sel.addEventListener("change", () => onChange(sel.value));
    return sel;
  }

  function toggle(text, value, onChange, extra = "") {
    const label = element("label", "smp-toggle " + extra);
    const box = element("input");
    box.type = "checkbox";
    box.checked = value;
    box.addEventListener("change", () => onChange(box.checked));
    label.append(box, document.createTextNode(text));
    return label;
  }

  function group(title, className = "") {
    const g = element("div", "smp-group " + className);
    g.appendChild(element("span", "smp-group-title", title));
    return g;
  }

  function openSettings() {
    if (!st.settings) {
      const win = element("div", "smp-settings-window");
      win.setAttribute("role", "dialog");
      win.setAttribute("aria-label", "Sampler Settings");
      const head = element("div", "smp-settings-head");
      head.appendChild(document.createTextNode("Sampler Settings"));
      head.appendChild(element("span", "smp-settings-mode"));
      const close = element("button", "smp-settings-close", "\u00d7");
      close.type = "button";
      close.title = "Close";
      close.addEventListener("click", () => { win.hidden = true; });
      head.appendChild(close);
      win.appendChild(head);
      win.appendChild(element("div", "smp-settings-body"));
      document.body.appendChild(win);
      const w = Math.min(800, window.innerWidth - 20), h = Math.min(640, window.innerHeight - 20);
      win.style.width = `${w}px`;
      win.style.height = `${h}px`;
      win.style.left = `${Math.round((window.innerWidth - w) / 2)}px`;
      win.style.top = `${Math.round((window.innerHeight - h) / 2)}px`;
      head.addEventListener("pointerdown", (event) => {
        if (event.target.closest("button")) return;
        const r = win.getBoundingClientRect(), dx = event.clientX - r.left, dy = event.clientY - r.top;
        head.setPointerCapture(event.pointerId);
        const move = (e) => {
          win.style.left = `${clamp(e.clientX - dx, 0, window.innerWidth - 80)}px`;
          win.style.top = `${clamp(e.clientY - dy, 0, window.innerHeight - 30)}px`;
        };
        const up = () => { head.removeEventListener("pointermove", move); head.removeEventListener("pointerup", up); };
        head.addEventListener("pointermove", move);
        head.addEventListener("pointerup", up);
      });
      st.settings = win;
    }
    st.settings.hidden = false;
    renderSettings();
  }

  function renderSettings() {
    const win = st.settings;
    if (!win || win.hidden) return;
    const s = st.s, mode = s.mode, isBuilder = mode === "builder", isChord = mode === "chord";
    win.querySelector(".smp-settings-mode").textContent = { vsti: "VSTi Sampling", chord: "Chord Sampling", builder: "Sample Builder" }[mode];
    const body = win.querySelector(".smp-settings-body");
    const scroll = body.scrollTop;
    body.textContent = "";
    const left = element("div", "smp-settings-col"), right = element("div", "smp-settings-col");
    body.append(left, right);
    const set = (key, value, what = "all") => { s[key] = value; changed(what); };

    // LEFT: Audio I/O.
    section(left, true);
    const io = group("Audio I/O");
    io.appendChild(row("Bit Depth:", select([["16 bit", 16], ["24 bit", 24], ["32 bit (float)", 32]], s.bitDepth, (v) => set("bitDepth", Number(v), "face"))));
    io.appendChild(row("Sample Rate:", select([["Interface", 0], ["44100 Hz", 44100], ["48000 Hz", 48000], ["88200 Hz", 88200], ["96000 Hz", 96000], ["192000 Hz", 192000]], s.sampleRate, (v) => set("sampleRate", Number(v), "face"))));
    io.appendChild(row("Channels:", select([["Stereo", 0], ["Mono (Left)", 1], ["Mono (L+R Mix)", 2]], s.channelMode, (v) => set("channelMode", Number(v), "face"))));
    left.appendChild(io);

    // Velocity layers, velocity, offset, key shift.
    section(left);
    const velButton = element("button", "smp-wide-button", `Velocity Layers: ${s.layers.length}`);
    velButton.type = "button";
    velButton.addEventListener("click", () => velocityEditor(-1, velButton.getBoundingClientRect()));
    left.appendChild(velButton);
    left.appendChild(element("div")).style.height = "8px";
    left.appendChild(row("Velocity:", slider(1, 127, 1, s.samplingVelocity, (v) => set("samplingVelocity", v, "zones"))));
    left.lastChild.title = "MIDI velocity used to capture a single full-range layer. Higher = brighter; 127 captures the fullest timbre.";
    left.appendChild(row("Vel Offset:", slider(0, 30, 1, s.velocityOffset, (v) => set("velocityOffset", v, "zones"))));
    left.lastChild.title = "Records each velocity layer at (layer top - this). E.g. offset 3 -> a 1-127 layer samples at velocity 124.";
    if (!isChord) left.appendChild(row("Key Shift:", slider(-48, 48, 1, s.keyShift, (v) => set("keyShift", v, "zones"))));

    // Chords Mode Key Range.
    if (isChord) {
      section(left);
      const range = group("Chords Mode Key Range", "chords");
      const keySlider = (value, onChange) => slider(0, 127, 1, value, onChange, { show: noteName, parse: parseKey, wide: true });
      let minSlider = null, maxSlider = null;
      minSlider = keySlider(s.minChordKey, (v) => { s.minChordKey = v; if (v > s.maxChordKey) { s.maxChordKey = v; maxSlider.setValue(v); } changed("chord"); });
      maxSlider = keySlider(s.maxChordKey, (v) => { s.maxChordKey = v; if (v < s.minChordKey) { s.minChordKey = v; minSlider.setValue(v); } changed("chord"); });
      const minRow = row("Min Key:", minSlider);
      const maxRow = row("Max Key:", maxSlider);
      range.append(minRow, maxRow);
      left.appendChild(range);
    }

    // Sampling Speed.
    section(left);
    const speed = group("Sampling Speed");
    const speedRow = element("div", "smp-speed");
    for (const [label, value] of [["Real-Time", "realtime"], ["Turbo  (~5" + TIMES + ")", "turbo"], ["Unleashed", "unleashed"]]) {
      const b = element("button", s.speed === value ? "on" : "", label);
      b.type = "button";
      b.dataset.speed = value;
      b.addEventListener("click", () => set("speed", value));
      speedRow.appendChild(b);
    }
    const info = element("button", "smp-info", "i");
    info.type = "button";
    info.title = "What the speeds do";
    info.addEventListener("click", () => speedInfo(info));
    speedRow.appendChild(info);
    speed.appendChild(speedRow);
    if (s.speed !== "realtime") speed.appendChild(element("p", "smp-settings-note", "For the nodes: a hardware synth and you playing are always sampled in real time."));
    left.appendChild(speed);

    // Sonify's own.
    if (!isBuilder) {
      section(left);
      const extra = group("Sonify", "sonify");
      extra.appendChild(row("Round Robins:", select([["Off (one take)", 0], ["2 takes", 2], ["3 takes", 3], ["4 takes", 4]], s.roundRobins, (v) => set("roundRobins", Number(v)))));
      extra.appendChild(toggle("Trim the silence before each attack", s.trim, (v) => set("trim", v, "face")));
      extra.appendChild(toggle("Normalize together (the loudest at -1 dB)", s.normalize, (v) => set("normalize", v, "face")));
      if (s.source === "voice") extra.appendChild(row("Note starts above:", slider(-60, -6, 1, s.triggerDb, (v) => set("triggerDb", v, "face"), { suffix: " dB" })));
      left.appendChild(extra);
    } else {
      section(left);
      const extra = group("Sonify", "sonify");
      extra.appendChild(toggle("Normalize together (the loudest at -1 dB)", s.normalize, (v) => set("normalize", v, "face")));
      left.appendChild(extra);
    }

    // RIGHT: duration, time signature, BPM layers (not the Builder).
    let firstRight = true;
    if (!isBuilder) {
      section(right, firstRight);
      firstRight = false;
      const modeButton = element("button", "smp-wide-button mode", s.durationMode === "bars" ? "Mode: BARS" : "Mode: TIME");
      modeButton.type = "button";
      modeButton.addEventListener("click", () => set("durationMode", s.durationMode === "bars" ? "time" : "bars"));
      right.appendChild(modeButton);
      right.appendChild(element("div")).style.height = "8px";
      right.appendChild(row("Duration:", s.durationMode === "bars"
        ? slider(0.25, 16, 0.25, s.durationBars, (v) => set("durationBars", v, "face"), { suffix: " bars", wide: true })
        : slider(200, 30000, 100, s.durationMs, (v) => set("durationMs", v, "face"), { suffix: " ms", wide: true })));
      section(right);
      const sig = element("div", "smp-timesig");
      sig.appendChild(select(Array.from({ length: 16 }, (_, i) => [String(i + 1), i + 1]), s.timeSigNum, (v) => set("timeSigNum", Number(v), "face")));
      sig.appendChild(element("span", "", "/"));
      sig.appendChild(select([1, 2, 4, 8, 16, 32].map((d) => [String(d), d]), s.timeSigDen, (v) => set("timeSigDen", Number(v), "face")));
      right.appendChild(row("Time Sig:", sig));
      const bpm = element("input");
      bpm.type = "text";
      bpm.placeholder = "e.g. 80, 120, 160 (blank = none)";
      bpm.value = s.bpmLayers.join(", ");
      const commit = () => {
        const list = [...new Set(bpm.value.split(/[\s,]+/).map(Number).filter((b) => b > 0).map((b) => clamp(Math.round(b), 20, 300)))].sort((a, b) => a - b);
        bpm.value = list.join(", ");
        if (list.join(",") !== s.bpmLayers.join(",")) set("bpmLayers", list);
      };
      bpm.addEventListener("keydown", (event) => { if (event.key === "Enter") commit(); event.stopPropagation(); });
      bpm.addEventListener("blur", commit);
      bpm.addEventListener("input", () => { bpm.value = bpm.value.replace(/[^0-9, ]/g, ""); });
      right.appendChild(row("BPM Layers:", bpm));
    }

    // Silence.
    section(right, firstRight);
    right.appendChild(row("Silence Thresh:", slider(-90, -20, 1, s.silenceDb, (v) => set("silenceDb", v, "face"), { suffix: " dB" })));
    right.appendChild(element("div")).style.height = "6px";
    right.appendChild(row("Silence Dur:", slider(100, 5000, 50, s.silenceMs, (v) => set("silenceMs", v, "face"), { suffix: " ms" })));

    // Fade out.
    section(right);
    right.appendChild(toggle("Fade Out on End", s.fadeOut, (v) => set("fadeOut", v, "face"), "fade"));
    right.appendChild(row("Duration:", slider(10, 2000, 10, s.fadeMs, (v) => set("fadeMs", v, "face"), { suffix: " ms" })));

    // Output.
    section(right);
    right.appendChild(toggle("Create SFZ", s.createSfz, (v) => set("createSfz", v)));
    if (isChord && s.createSfz) right.appendChild(element("p", "smp-settings-note", "Chord Sampling writes the WAVs only (no SFZ), as Sampler Arena does."));
    right.appendChild(toggle("Create Legato Loop", s.autoLoop, (v) => set("autoLoop", v)));
    right.appendChild(element("p", "smp-settings-note", "Each sample gets the loop finder's best sustain loop (its .loop file, the loop in its WAV, and in the SFZ). The Loop Editor checks them, and Ask Fanan AI can pick better ones."));

    body.scrollTop = scroll;
  }

  // ---------------------------------------------------------------- Presets
  function renderPresets() {
    if (!st.shown || st.tab !== "presets") return;
    const n = st.native || {};
    $("smp-folder-text").textContent = `The samples go to: ${n.folder || ""}\nSet Folder (kept): ${n.moduleFolder || ""}\nAd Hoc Folder (this run only): ${n.adHoc || "none"}`;
  }

  // ---------------------------------------------------------------- the Loop Editor
  function renderLoop() {
    if (!st.shown || st.tab !== "loop") return;
    const L = st.loop || { regions: [], selected: -1, status: "", score: "Score: --", candidate: -1, candidates: 0, crossfadeMs: 100, crossfadeOn: true, playing: false, busy: false };
    $("sle-name").textContent = L.name || "No patch loaded";
    const has = L.regions && L.regions.length > 0;
    $("sle-save").disabled = !has || L.busy;
    $("sle-save-as").disabled = !has || L.busy;
    $("sle-batch").disabled = !has || L.busy;
    $("sle-ai").disabled = !has || L.busy;
    $("sle-ai").title = (L.signedIn ? "" : "Sign in to your Fanan account first (Build with AI). ") + "Fanan AI listens to the loop finder's candidates (and the loop there now) and picks the smoothest, with its crossfade. Uses credits.";
    const list = $("sle-list");
    const keepScroll = list.scrollTop;
    list.textContent = "";
    (L.regions || []).forEach((r, i) => {
      const item = element("div", "sle-row" + (r.looped ? " looped" : "") + (i === L.selected ? " selected" : "") + (r.missing ? " missing" : "") + (r.changed ? " changed" : ""), r.name);
      item.setAttribute("role", "option");
      item.title = `${r.name}: key ${noteName(r.key)}, velocities ${r.lovel}-${r.hivel}` + (r.missing ? " (file not found)" : "") + (r.changed ? " (changed: Save writes it)" : "");
      item.addEventListener("click", () => send("loopSelect", { index: i }));
      list.appendChild(item);
    });
    list.scrollTop = keepScroll;
    const play = $("sle-play");
    play.textContent = L.playing ? "Stop" : "Play";
    play.classList.toggle("playing", L.playing);
    $("sle-candidate").textContent = `${L.candidate >= 0 ? L.candidate + 1 : 0}/${L.candidates || 0}`;
    $("sle-score").textContent = L.score || "Score: --";
    if (document.activeElement !== $("sle-xfade-ms") && !st.xfadeTimer) {
      $("sle-xfade").value = L.crossfadeMs;
      $("sle-xfade-ms").value = L.crossfadeMs;
      $("sle-xfade-on").checked = L.crossfadeOn;
    }
    for (const id of ["sle-find", "sle-prev", "sle-next", "sle-remove"]) $(id).disabled = L.busy || L.selected < 0;
    $("sle-status").textContent = L.status || "";
    $("sle-status").title = L.status || "";
    drawWave();
  }

  function drawWave() {
    const canvas = $("sle-wave");
    if (!canvas || !st.shown || st.tab !== "loop") return;
    const r = canvas.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
    const w = Math.max(10, Math.round(r.width)), h = Math.max(10, Math.round(r.height));
    if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) { canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr); }
    const g = canvas.getContext("2d");
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.fillStyle = "rgb(18,18,24)";
    g.fillRect(0, 0, w, h);
    const L = st.loop, sample = L && L.sample, peaks = st.loopPeaks;
    g.textAlign = "left";
    g.textBaseline = "middle";
    if (!sample || !peaks || !peaks.length || !sample.frames) {
      g.fillStyle = "rgb(80,80,80)";
      g.font = "12px sans-serif";
      g.textAlign = "center";
      g.fillText("No waveform loaded", w / 2, h / 2);
      g.strokeStyle = "rgb(60,60,70)";
      g.strokeRect(0.5, 0.5, w - 1, h - 1);
      return;
    }
    const frames = sample.frames, x = (s) => s / frames * w;
    const drag = st.loopDrag;
    const loopStart = drag ? drag.start : sample.loopStart, loopEnd = drag ? drag.end : sample.loopEnd, looped = sample.looped || Boolean(drag);
    const mono = peaks.length < 2;
    if (!mono) { g.fillStyle = "rgb(50,50,60)"; g.fillRect(0, Math.round(h / 2), w, 1); }
    if (sample.sustainEnd > sample.sustainStart) { g.fillStyle = "rgba(50,70,50,0.3)"; g.fillRect(x(sample.sustainStart), 0, x(sample.sustainEnd) - x(sample.sustainStart), h); }
    // The crossfade: the loop's last part fades into what comes just before its start.
    if (looped && loopEnd > loopStart && sample.crossfade > 0) {
      g.fillStyle = "rgba(100,60,0,0.25)";
      g.fillRect(x(loopEnd - sample.crossfade), 0, x(loopEnd) - x(loopEnd - sample.crossfade), h);
      g.fillRect(x(Math.max(0, loopStart - sample.crossfade)), 0, x(loopStart) - x(Math.max(0, loopStart - sample.crossfade)), h);
    }
    const channel = (data, top, height, colour) => {
      const mid = top + height / 2, half = height / 2, count = data.length / 2;
      g.fillStyle = colour;
      for (let px = 0; px < w; px++) {
        const a = Math.floor(px / w * count), b = Math.max(a + 1, Math.floor((px + 1) / w * count));
        let lo = 127, hi = -127;
        for (let i = a; i < b && i < count; i++) { lo = Math.min(lo, data[2 * i]); hi = Math.max(hi, data[2 * i + 1]); }
        if (hi < lo) continue;
        const y1 = mid - hi / 127 * half, y2 = mid - lo / 127 * half;
        g.fillRect(px, y1, 1, Math.max(1, y2 - y1));
      }
    };
    g.font = "bold 9px sans-serif";
    if (mono) {
      channel(peaks[0], 0, h, "rgb(60,180,60)");
      g.fillStyle = "rgba(60,180,60,0.5)";
      g.fillText("MONO", 3, 8);
      g.fillStyle = "rgb(40,40,50)";
      g.fillRect(0, Math.round(h / 2), w, 1);
    } else {
      channel(peaks[0], 0, h / 2, "rgb(60,180,60)");
      channel(peaks[1], h / 2, h / 2, "rgb(50,160,220)");
      g.fillStyle = "rgba(60,180,60,0.5)";
      g.fillText("L", 3, 8);
      g.fillStyle = "rgba(50,160,220,0.5)";
      g.fillText("R", 3, h / 2 + 8);
      g.fillStyle = "rgb(40,40,50)";
      g.fillRect(0, Math.round(h / 4), w, 1);
      g.fillRect(0, Math.round(h * 3 / 4), w, 1);
    }
    if (st.loopPos >= 0 && L.playing) { g.fillStyle = "rgba(255,255,255,0.8)"; g.fillRect(x(st.loopPos) - 1, 0, 2, h); }
    g.textAlign = "center";
    if (looped && loopStart > 0) {
      g.fillStyle = "rgb(40,220,40)";
      g.fillRect(x(loopStart) - 2, 0, 4, h);
      g.fillText("S", x(loopStart), 8);
    }
    if (looped && loopEnd > 0) {
      g.fillStyle = "rgb(220,100,40)";
      g.fillRect(x(loopEnd) - 2, 0, 4, h);
      g.fillText("E", x(loopEnd), 8);
    }
    g.strokeStyle = "rgb(60,60,70)";
    g.lineWidth = 1;
    g.strokeRect(0.5, 0.5, w - 1, h - 1);
  }

  function wireLoop() {
    $("sle-load").addEventListener("click", () => send("loopLoad"));
    $("sle-batch").addEventListener("click", () => send("loopBatch"));
    $("sle-ai").addEventListener("click", () => send("loopAi", { all: $("sle-ai-scope").value === "all" }));
    $("sle-save").addEventListener("click", () => {
      const name = st.loop && st.loop.name;
      if (!name) { send("loopSave", { as: true }); return; }
      Hub.popupMenu($("sle-save"), [{ group: `Overwrite ${name}?` }, { label: "Save", value: true }, { label: "Cancel", value: false }], (yes) => { if (yes) send("loopSave", { as: false }); });
    });
    $("sle-save-as").addEventListener("click", () => send("loopSave", { as: true }));
    $("sle-play").addEventListener("click", () => send(st.loop && st.loop.playing ? "loopStop" : "loopPlay"));
    $("sle-find").addEventListener("click", () => send("loopFind"));
    $("sle-prev").addEventListener("click", () => send("loopCandidate", { index: (st.loop ? st.loop.candidate : 0) - 1 }));
    $("sle-next").addEventListener("click", () => send("loopCandidate", { index: (st.loop ? st.loop.candidate : 0) + 1 }));
    $("sle-remove").addEventListener("click", () => send("loopRemove"));
    const xfade = (ms) => {
      ms = clamp(Math.round((Number(ms) || 100) / 10) * 10, 20, 500);
      $("sle-xfade").value = ms;
      $("sle-xfade-ms").value = ms;
      clearTimeout(st.xfadeTimer);
      st.xfadeTimer = setTimeout(() => { st.xfadeTimer = 0; send("loopXfade", { ms, on: $("sle-xfade-on").checked }); }, 150);
    };
    $("sle-xfade").addEventListener("input", () => xfade($("sle-xfade").value));
    $("sle-xfade-ms").addEventListener("change", () => xfade($("sle-xfade-ms").value));
    $("sle-xfade-ms").addEventListener("keydown", (event) => { if (event.key === "Enter") $("sle-xfade-ms").blur(); event.stopPropagation(); });
    $("sle-xfade-on").addEventListener("change", () => xfade($("sle-xfade").value));

    const list = $("sle-list");
    list.tabIndex = 0;
    list.addEventListener("keydown", (event) => {
      if (!st.loop || !st.loop.regions) return;
      const step = event.key === "ArrowDown" ? 1 : event.key === "ArrowUp" ? -1 : 0;
      if (!step) return;
      event.preventDefault();
      send("loopSelect", { index: clamp(st.loop.selected + step, 0, st.loop.regions.length - 1) });
    });

    // S and E, dragged (at least 1024 samples apart, as Sampler Arena's).
    const canvas = $("sle-wave");
    const toSample = (event) => {
      const r = canvas.getBoundingClientRect(), frames = st.loop.sample.frames;
      return clamp(Math.round((event.clientX - r.left) / r.width * frames), 0, frames - 1);
    };
    const markerAt = (event) => {
      const L = st.loop;
      if (!L || !L.sample || !L.sample.looped || L.busy) return "";
      const r = canvas.getBoundingClientRect(), frames = L.sample.frames, px = event.clientX - r.left;
      if (Math.abs(px - L.sample.loopStart / frames * r.width) < 8) return "start";
      if (Math.abs(px - L.sample.loopEnd / frames * r.width) < 8) return "end";
      return "";
    };
    canvas.addEventListener("pointerdown", (event) => {
      const which = markerAt(event);
      if (!which) return;
      canvas.setPointerCapture(event.pointerId);
      st.loopDrag = { which, start: st.loop.sample.loopStart, end: st.loop.sample.loopEnd };
    });
    canvas.addEventListener("pointermove", (event) => {
      const d = st.loopDrag;
      if (!d) { canvas.style.cursor = markerAt(event) ? "ew-resize" : "default"; return; }
      const v = toSample(event);
      if (d.which === "start") d.start = Math.min(v, d.end - 1024); else d.end = Math.max(v, d.start + 1024);
      d.start = Math.max(0, d.start);
      drawWave();
    });
    const up = () => {
      const d = st.loopDrag;
      if (!d) return;
      st.loopDrag = null;
      if (st.loop && st.loop.sample) { st.loop.sample.loopStart = d.start; st.loop.sample.loopEnd = d.end; }
      send("loopSet", { start: d.start, end: d.end });
      drawWave();
    };
    canvas.addEventListener("pointerup", up);
    canvas.addEventListener("pointercancel", up);
  }

  // ---------------------------------------------------------------- tabs, showing
  function showTab(tab) {
    st.tab = tab;
    for (const t of document.querySelectorAll(".smp-tab")) t.setAttribute("aria-selected", String(t.dataset.tab === tab));
    $("smp-tab-sampling").hidden = tab !== "sampling";
    $("smp-tab-presets").hidden = tab !== "presets";
    $("smp-tab-loop").hidden = tab !== "loop";
    $("smp-tab-blob").hidden = tab !== "blob";
    if (Hub.samplingBlob) Hub.samplingBlob.setVisible(st.shown && tab === "blob");
    closePop();
    renderAll();
  }

  function renderAll() {
    if (!st.shown) return;
    renderFace();
    renderModeRow();
    renderSource();
    renderChordHint();
    renderCount();
    drawZones();
    renderPresets();
    renderLoop();
    renderSettings();
  }

  function show(on) {
    if (st.shown === on) return;
    st.shown = on;
    send("shown", { shown: on });
    if (on) renderAll(); else closePop();
    // The Settings window floats over every page: hidden while the studio is away, back with it.
    if (!on && st.settings) { st.settingsAway = !st.settings.hidden; st.settings.hidden = true; }
    if (on && st.settingsAway) { st.settingsAway = false; openSettings(); }
    if (!on && Hub.samplingNodes) Hub.samplingNodes.setVisible(false);
    if (Hub.samplingBlob) Hub.samplingBlob.setVisible(on && st.tab === "blob");
  }

  Hub.sampling = { show, openSettings, isBusy };

  // ---------------------------------------------------------------- Sonify's word
  Hub.on("sampling", (data) => {
    if (!data) return;

    switch (data.kind) {
      case "state": {
        st.native = data;
        if (!st.loaded) { takeSettings(data.kept); st.loaded = true; }
        const before = sessionState();
        const builtBefore = st.build && st.build.done;
        st.session = data.session || null;
        st.live = data.session ? Object.assign({}, data.session) : null;
        st.build = data.build || null;
        st.made = data.made || null;
        const now = sessionState();
        if (before && before !== "done" && now === "done") Hub.toast(st.live && st.live.message ? st.live.message : st.made && st.made.sfz ? `${st.made.name} is ready: Try It puts it on a new track.` : `The samples are in ${st.made ? st.made.folder : "their folder"}.`);
        if (now === "failed" && before !== "failed") Hub.toast(st.live.message || "Sampling failed.");
        if (st.build && st.build.done && !builtBefore) Hub.toast(st.build.problem || (st.made && st.made.sfz ? `${st.build.name} is built: Try It puts it on a new track.` : `${st.build.written} samples written.`));
        renderAll();
        break;
      }

      case "progress": {
        if (data.session && st.session) {
          const before = st.live ? st.live.marks : "";
          const shotBefore = st.live ? st.live.shot : -1;
          st.live = data.session;
          if (["running", "stopping"].includes(st.live.state)) setMeter(st.live.left || 0, st.live.right || 0);
          if (st.shown) {
            if (st.live.marks !== before || st.live.shot !== shotBefore) drawZones();
            renderFace();
          }
        }
        if (data.build) {
          st.build = Object.assign({}, st.build || {}, data.build);
          renderFace();
        }
        break;
      }

      case "exists": {
        const more = data.count > 1 ? ` and ${data.count - 1} more` : "";
        Hub.popupMenu($("smp-play"), [{ group: `${data.name}${more} already in ${data.folder}` }, { label: "Write over them", value: true }, { label: "Cancel", value: false }],
          (yes) => { if (yes) start(true); });
        break;
      }

      case "builderFiles": {
        const paths = data.paths || [];
        if (!paths.length) break;
        const note = typeof data.note === "number" ? data.note : 60;
        const row = typeof data.row === "number" ? data.row : slotsOf(note).length;
        setSlot(note, row, { path: paths[0], name: fileStem(paths[0]), lovel: 0, hivel: 127 });
        break;
      }

      case "builderInfo": {
        for (const file of data.files || []) st.info.set(file.path, file);
        break;
      }

      case "preset": {
        takeSettings(data.settings);
        closePop();
        Hub.toast(`Preset loaded: ${data.name}`);
        renderAll();
        break;
      }

      case "notice": {
        Hub.toast(data.text);
        break;
      }

      case "loop": {
        const before = st.loop;
        st.loop = data;
        if (data.withPeaks) st.loopPeaks = data.sample && data.sample.peaks ? data.sample.peaks : null;
        else if (before && before.selected !== data.selected) st.loopPeaks = null;
        if (!data.playing) st.loopPos = -1;
        renderLoop();
        break;
      }

      case "loopPosition": {
        st.loopPos = data.position;
        if (st.loop) st.loop.playing = true;
        if (st.tab === "loop") requestAnimationFrame(drawWave);
        break;
      }

      case "tried": {
        if (Hub.studios) Hub.studios.activate("studio");
        Hub.toast("The new track plays it in a Sonify Sampler: its window opens from the track's instrument.");
        break;
      }

      case "problem": {
        Hub.toast(data.text);
        st.problem = data.text;
        renderFace();
        break;
      }
    }
  });

  Hub.on("tracks", () => {
    if (!st.shown) return;
    renderSource();
  });

  // The nodes changed (wired, flagged): Sample may begin now, or not.
  Hub.on("samplingNodes", () => { if (st.shown) renderFace(); });

  // ---------------------------------------------------------------- wiring
  Hub.on("ready", () => {
    for (const tab of document.querySelectorAll(".smp-tab")) tab.addEventListener("click", () => showTab(tab.dataset.tab));

    $("smp-family").addEventListener("input", () => { st.s.family = $("smp-family").value; changed("face"); });
    $("smp-family").addEventListener("change", () => { if (!$("smp-family").value.trim()) { st.s.family = "Sample"; changed(); } });
    $("smp-family").addEventListener("keydown", (event) => { if (event.key === "Enter" || event.key === "Escape") $("smp-family").blur(); event.stopPropagation(); });

    $("smp-play").addEventListener("click", () => {
      const state = sessionState();
      if (["running", "stopping"].includes(state)) send("stop");
      else if (state === "stopped") send("resume");
      else start(false);
    });
    $("smp-folder").addEventListener("click", () => send("reveal"));

    for (const b of document.querySelectorAll(".smp-mode")) b.addEventListener("click", () => {
      if (isBusy() || st.s.mode === b.dataset.mode) return;
      st.s.mode = b.dataset.mode;
      closePop();
      changed();
      drawZones();
    });
    $("smp-chord-types").addEventListener("click", chordTypesEditor);
    $("smp-no-ranges").addEventListener("click", () => { st.s.noKeyRanges = !st.s.noKeyRanges; changed(); });
    $("smp-adhoc").addEventListener("click", () => {
      const adHoc = st.native && st.native.adHoc;
      if (!adHoc) { send("adHoc"); return; }
      Hub.popupMenu($("smp-adhoc"), [{ group: `Ad Hoc: ${adHoc}` }, { label: "Choose another folder...", value: "choose" }, { label: `Stop using it (back to ${st.native.moduleFolder})`, value: "clear" }],
        (v) => send(v === "clear" ? "adHocClear" : "adHoc"));
    });
    $("smp-settings").addEventListener("click", openSettings);

    $("smp-source").addEventListener("click", (event) => {
      const b = event.target.closest("button");
      if (!b || isBusy()) return;
      st.s.source = b.dataset.source;
      changed();
    });
    $("smp-track").addEventListener("change", () => { st.s.trackId = Number($("smp-track").value) || 0; changed("face"); });
    $("smp-midi-out").addEventListener("change", () => { st.s.midiOutput = $("smp-midi-out").value; changed("face"); });
    $("smp-midi-channel").addEventListener("change", () => { st.s.midiChannel = Number($("smp-midi-channel").value) || 1; changed("face"); });

    $("smp-clear").addEventListener("click", () => {
      if (isBusy()) return;
      st.s.keys = [];
      st.s.overrides = {};
      st.s.boundaries = {};
      changed();
    });

    $("smp-preset-save").addEventListener("click", () => send("presetSave", { settings: st.s }));
    $("smp-preset-load").addEventListener("click", () => send("presetLoad"));
    $("smp-set-folder").addEventListener("click", () => send("setFolder"));

    wireZones();
    wireLoop();

    // The meters fall back when nothing comes.
    setInterval(() => { if (st.shown && (st.level[0] > 0.001 || st.level[1] > 0.001)) setMeter(0, 0); }, 80);
    new ResizeObserver(() => { if (st.shown && st.tab === "loop") drawWave(); }).observe($("sle-wave"));
  });
})();
