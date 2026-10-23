// D:\workspace\Fanan_web_hub\src\hub-sampling-nodes.js
//
// The Sampling Studio's nodes, under Clear All Keys (VSTi Sampling and Chord Sampling with the
// Nodes source): Sampler Arena's node system, kept within its area. Sonify plays them
// (SamplingRack.cpp); this draws them and wires them.
//
//   Sampling  the studio's notes: its MIDI out (red) wires to the VSTis that play them.
//             E: the sampler's Settings (speed and the rest); F: the folder the samples go to.
//   MIDI In   the live MIDI, to hear the VSTis before sampling: the inputs ticked in its list
//             (All inputs, or some), and the keyboard on the nodes' left, always. Its MIDI out
//             (red) wires to VSTis too; its light blinks with what comes through. It's heard only
//             while the Sampling Studio is in front (and never sampled).
//   VSTi      an instrument plugin: MIDI in, its sound out as a pair (blue: L and R).
//             E: its window; M: mute; X: remove.
//   Effect    an effect plugin: sound in as a pair, sound out as a pair. E, M, P (pass through:
//             untouched), X.
//   Output    stereo, L and R: what's wired into each side is what's sampled, and heard.
//
// A VSTi whose notes come from the Sampling node and whose sound reaches the Output (straight,
// or through effects) is flagged (a white box with a red dot): every flagged VSTi is sampled,
// together as one sound. Sample waits until there's one.
//
// The area is restricted, so the connectors sit on the nodes' sides: inputs on the right,
// outputs on the left. Drag from an output to an input to wire them: dropped on a connector, that
// side (L to L, or crossed); dropped on the node, both sides at once. Drag a wire's end off an
// input to move it or let it go; click a wire and press Delete to remove it. Drag a node by its
// body; double-click a plugin node for its window; right-click for the rest. The keyboard, the
// MIDI monitor and the tabs beside the nodes are hub-sampling-midi.js's.
//
// The format buttons (VST2, VST3, CLAP; AU on a Mac, LV2 on Linux) open a browse window at the
// format's usual folder (or the one a plugin was last loaded from), listing its folders and
// plugins: double-click a plugin to load it.

(() => {
  "use strict";

  const { $, element } = Hub;
  const send = (op, data = {}) => Hub.send("sampling", Object.assign({ action: "rack", op }, data));

  // Sampler Arena's node measures.
  const NODE_W = 156, NODE_H = 72, TITLE_H = 24, BTN = 20, BTN_GAP = 4, BTN_MARGIN = 5, BTN_LEFT = 12;
  const PIN_R = 6, PIN_HIT = 11, MARGIN = 6;
  // Inputs on the right, outputs on the left: the sound runs right to left.
  const INPUTS_RIGHT = true;
  const COLOURS = {
    background: "#1e1e1e", dots: "#2c2c2c", body: "#2d2d2d", bodyMuted: "#3a3a3a", border: "#555555",
    active: "#00aaff", audio: "#00aaff", midi: "#ff3b3b", text: "#ffffff", muted: "#9a9a9a",
    sampler: "#7a5c00", output: "#2f6b45", instrument: "#505050", effect: "#45505e", titleMuted: "#404040", missing: "#7a2e2e",
    midiin: "#6a2f63", lit: "#3ddc84", unlit: "#2a3a30"
  };
  const FORMAT_LABELS = { VST2: "VST2", VST3: "VST3", CLAP: "CLAP", AU: "AU", LV2: "LV2" };

  const st = {
    rack: null,            // {nodes, wires, ready}, as Sonify last gave it
    formats: [],
    visible: false,
    selected: 0,           // a node's id
    wire: null,            // a selected wire {from, to, fromSide, toSide}
    drag: null,            // what a press is doing
    hover: null,
    browse: null,          // the browse window: {format, folder, parent, entries, picked}
    size: { w: 600, h: 240 },
    placing: new Set(),    // nodes being given a first place
    midiPorts: [],         // what the MIDI In node can pick: [{name, device}]
    drop: null,            // the MIDI In node's list, while it's open
    heardAt: 0,            // when MIDI last came through the MIDI In node
    litTimer: 0
  };

  const nodes = () => (st.rack && st.rack.nodes) || [];
  const wires = () => (st.rack && st.rack.wires) || [];
  const byId = (id) => nodes().find((n) => n.id === id) || null;
  const isPlugin = (n) => n.kind === "instrument" || n.kind === "effect";
  const isMidiSource = (n) => n.kind === "sampler" || n.kind === "midiin";
  const sideOf = (v) => (v === 1 ? 1 : 0);
  const sameWire = (a, b) => Boolean(a && b) && a.from === b.from && a.to === b.to && sideOf(a.fromSide) === sideOf(b.fromSide) && sideOf(a.toSide) === sideOf(b.toSide);
  /** A wire as Sonify takes it: a MIDI wire's ends, or a sound wire's with its sides. */
  const wireData = (w) => (w.midi ? { from: w.from, to: w.to } : { from: w.from, to: w.to, fromSide: sideOf(w.fromSide), toSide: sideOf(w.toSide) });
  const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
  const busy = () => Boolean(Hub.sampling && Hub.sampling.isBusy && Hub.sampling.isBusy());

  // ---------------------------------------------------------------- where things are
  /** A node's place, kept inside the area (its own place may lie outside a smaller area). */
  function rectOf(n) {
    const w = st.size.w, h = st.size.h;
    let x = n.x, y = n.y;
    if (st.drag && st.drag.kind === "move" && st.drag.id === n.id) { x = st.drag.x; y = st.drag.y; }
    if (!(x >= 0) || !(y >= 0)) {
      const p = firstPlace(n);
      x = p.x; y = p.y;
    }
    return { x: clamp(x, MARGIN, Math.max(MARGIN, w - NODE_W - MARGIN)), y: clamp(y, MARGIN, Math.max(MARGIN, h - NODE_H - MARGIN)), w: NODE_W, h: NODE_H };
  }

  /** Where a node with no place of its own starts: the Sampling node on the right, the MIDI In
      by it, the Output on the left (the sound runs right to left), plugins between. */
  function firstPlace(n) {
    const w = st.size.w, h = st.size.h, middle = (h - NODE_H) / 2;
    if (n.kind === "sampler") return { x: w - NODE_W - 16, y: middle };
    if (n.kind === "output") return { x: 16, y: middle };
    if (n.kind === "midiin") {
      const sampler = nodes().find((x) => x.kind === "sampler");
      const s = sampler ? rectOf(sampler) : { x: w - NODE_W - 16, y: middle };
      if (s.y + 2 * NODE_H + 12 <= h - MARGIN) return { x: s.x, y: s.y + NODE_H + 12 };
      if (s.y - NODE_H - 12 >= MARGIN) return { x: s.x, y: s.y - NODE_H - 12 };
      return { x: Math.max(MARGIN, s.x - NODE_W - 24), y: s.y };
    }
    return freePlace(n.kind);
  }

  /** A free place for a new plugin node: VSTis right of the middle, effects left of it. */
  function freePlace(kind) {
    const w = st.size.w, h = st.size.h;
    const x = clamp(Math.round(w * (kind === "instrument" ? 0.62 : 0.36) - NODE_W / 2), MARGIN, Math.max(MARGIN, w - NODE_W - MARGIN));
    const taken = nodes().filter((n) => n.x >= 0 && n.y >= 0).map((n) => ({ x: n.x, y: n.y }));
    for (let column = 0; column < 4; column++) {
      for (let y = MARGIN + 8; y + NODE_H <= h - MARGIN; y += NODE_H + 12) {
        const cx = clamp(x + (column % 2 ? 1 : -1) * Math.ceil(column / 2) * (NODE_W + 16), MARGIN, Math.max(MARGIN, w - NODE_W - MARGIN));
        if (!taken.some((t) => Math.abs(t.x - cx) < NODE_W + 8 && Math.abs(t.y - y) < NODE_H + 8)) return { x: cx, y };
      }
    }
    return { x, y: MARGIN + 8 };
  }

  /** A node's connectors: inputs on the right, outputs on the left. MIDI is one connector; sound
      a pair, L above R (side 0 and 1). */
  function pinsOf(n) {
    const r = rectOf(n);
    const single = r.y + TITLE_H + (r.h - TITLE_H) / 2 - 4;
    const pair = [r.y + TITLE_H + 12, r.y + r.h - 14];
    const inX = INPUTS_RIGHT ? r.x + r.w : r.x, outX = INPUTS_RIGHT ? r.x : r.x + r.w;
    const inputs = [], outputs = [];
    if (n.kind === "instrument") inputs.push({ x: inX, y: single, side: 0, midi: true });
    if (n.kind === "effect" || n.kind === "output") pair.forEach((y, side) => inputs.push({ x: inX, y, side, midi: false }));
    if (isMidiSource(n)) outputs.push({ x: outX, y: single, side: 0, midi: true });
    if (isPlugin(n)) pair.forEach((y, side) => outputs.push({ x: outX, y, side, midi: false }));
    return { inputs, outputs };
  }

  /** The MIDI In node's inputs, in a few words. */
  function inputsSummary(n) {
    const list = Array.isArray(n.inputs) ? n.inputs : ["*"];
    if (list.includes("*")) return "All inputs";
    if (!list.length) return "Keyboard only";
    return list.length === 1 ? list[0] : `${list.length} inputs`;
  }

  /** A node's buttons, left to right along its bottom (the MIDI In's: its list, the width of it). */
  function buttonsOf(n) {
    const r = rectOf(n), y = r.y + r.h - BTN_MARGIN - BTN;
    const list = [];
    const add = (key, label) => list.push({ key, label, x: r.x + BTN_LEFT + list.length * (BTN + BTN_GAP), y, w: BTN, h: BTN });
    if (n.kind === "sampler") { add("settings", "E"); add("folder", "F"); }
    if (n.kind === "midiin") list.push({ key: "inputs", label: inputsSummary(n), x: r.x + BTN_LEFT, y, w: r.w - BTN_LEFT - 8, h: BTN });
    if (n.kind === "instrument") { add("window", "E"); add("mute", "M"); add("remove", "X"); }
    if (n.kind === "effect") { add("window", "E"); add("mute", "M"); add("pass", "P"); add("remove", "X"); }
    return list;
  }

  function bezierPoints(from, to) {
    const d = Math.max(36, Math.abs(from.x - to.x) * 0.5);
    const c1 = { x: from.x + (INPUTS_RIGHT ? -d : d), y: from.y }, c2 = { x: to.x + (INPUTS_RIGHT ? d : -d), y: to.y };
    return { from, c1, c2, to };
  }

  /** A wire's ends: its source's connector (MIDI, or its side of the pair) and its target's. */
  function wireEnds(w) {
    const a = byId(w.from), b = byId(w.to);
    if (!a || !b) return null;
    const midi = Boolean(w.midi);
    const pa = pinsOf(a).outputs.find((p) => p.midi === midi && (midi || p.side === sideOf(w.fromSide)));
    const pb = pinsOf(b).inputs.find((p) => p.midi === midi && (midi || p.side === sideOf(w.toSide)));
    return pa && pb ? { from: pa, to: pb } : null;
  }

  function nearWire(p) {
    for (const w of wires().slice().reverse()) {
      const ends = wireEnds(w);
      if (!ends) continue;
      const b = bezierPoints(ends.from, ends.to);
      for (let i = 0; i <= 32; i++) {
        const t = i / 32, u = 1 - t;
        const x = u * u * u * b.from.x + 3 * u * u * t * b.c1.x + 3 * u * t * t * b.c2.x + t * t * t * b.to.x;
        const y = u * u * u * b.from.y + 3 * u * u * t * b.c1.y + 3 * u * t * t * b.c2.y + t * t * t * b.to.y;
        if (Math.hypot(p.x - x, p.y - y) <= 5) return w;
      }
    }
    return null;
  }

  /** What's under a point: a button, a connector, a node, a wire, or nothing. */
  function hitAt(p) {
    const list = nodes().slice().reverse();   // the one drawn last is on top
    for (const n of list) {
      for (const b of buttonsOf(n)) if (p.x >= b.x && p.x < b.x + b.w && p.y >= b.y && p.y < b.y + b.h) return { kind: "button", node: n, button: b };
    }
    // The nearest connector in reach.
    let best = null;
    for (const n of list) {
      const pins = pinsOf(n);
      const consider = (kind, pin) => {
        const d = Math.hypot(p.x - pin.x, p.y - pin.y);
        if (d <= PIN_HIT && (!best || d < best.d)) best = { kind, node: n, pin, d };
      };
      for (const pin of pins.outputs) consider("output", pin);
      for (const pin of pins.inputs) consider("input", pin);
    }
    if (best) return best;
    for (const n of list) {
      const r = rectOf(n);
      if (p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h) return { kind: "node", node: n };
    }
    const w = nearWire(p);
    if (w) return { kind: "wire", wire: w };
    return { kind: "none" };
  }

  /** Whether a wire from this node may go into that one (Sonify says why not, if it's tried). */
  function canWire(from, to) {
    if (!from || !to || from.id === to.id) return false;
    if (isMidiSource(from)) return to.kind === "instrument";
    return isPlugin(from) && (to.kind === "effect" || to.kind === "output");
  }

  /** Whether a hovered connector is the same one as a pin of that node. */
  const samePin = (hover, n, pin, kind) => Boolean(hover && hover.node && hover.pin) && hover.node.id === n.id && hover.kind === kind
    && hover.pin.side === pin.side && hover.pin.midi === pin.midi;

  // ---------------------------------------------------------------- drawing
  function roundRect(g, x, y, w, h, r) {
    g.beginPath();
    g.roundRect(x, y, w, h, r);
  }

  function fitText(g, text, width) {
    if (g.measureText(text).width <= width) return text;
    let t = text;
    while (t.length > 1 && g.measureText(t + "...").width > width) t = t.slice(0, -1);
    return t + "...";
  }

  /** The area's size, once it's on the page (0 while hidden). */
  function measure() {
    const wrap = $("smp-nodes-wrap");
    if (!wrap || wrap.clientWidth <= 0 || wrap.clientHeight <= 0) return false;
    st.size = { w: Math.max(200, wrap.clientWidth), h: Math.max(120, wrap.clientHeight) };
    return true;
  }

  function draw() {
    const canvas = $("smp-nodes-canvas");
    if (!canvas || !st.visible || !measure()) return;
    const w = st.size.w, h = st.size.h, dpr = window.devicePixelRatio || 1;
    if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
      canvas.width = Math.round(w * dpr);
      canvas.height = Math.round(h * dpr);
      canvas.style.width = `${w}px`;
      canvas.style.height = `${h}px`;
    }
    const g = canvas.getContext("2d");
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.fillStyle = COLOURS.background;
    g.fillRect(0, 0, w, h);
    g.fillStyle = COLOURS.dots;
    for (let x = 12; x < w; x += 24) for (let y = 12; y < h; y += 24) g.fillRect(x, y, 2, 2);

    // Wires first, under the nodes.
    for (const wire of wires()) {
      const ends = wireEnds(wire);
      if (!ends) continue;
      const picked = sameWire(st.wire, wire);
      drawWire(g, ends.from, ends.to, wire.midi ? COLOURS.midi : COLOURS.audio, picked ? 4 : 2.2, picked);
    }
    if (st.drag && st.drag.kind === "wire") {
      const from = byId(st.drag.from);
      const start = from && pinsOf(from).outputs.find((p) => p.midi === st.drag.midi && (st.drag.midi || p.side === st.drag.fromSide));
      if (start) drawWire(g, start, st.drag.at, st.drag.midi ? COLOURS.midi : COLOURS.audio, 2, false, true);
    }

    for (const n of nodes()) drawNode(g, n);

    if (!nodes().some(isPlugin)) {
      g.fillStyle = COLOURS.muted;
      g.font = "12px sans-serif";
      g.textAlign = "center";
      g.textBaseline = "middle";
      g.fillText("Load a VSTi with the buttons above, wire the Sampling node's MIDI to it (the MIDI In's to hear it), and its sound to the Output", w / 2, h - 16);
    }
  }

  function drawWire(g, from, to, colour, width, glow, dashed) {
    const b = bezierPoints(from, to);
    g.save();
    if (glow) { g.shadowColor = colour; g.shadowBlur = 8; }
    if (dashed) g.setLineDash([6, 4]);
    g.strokeStyle = colour;
    g.lineWidth = width;
    g.lineCap = "round";
    g.beginPath();
    g.moveTo(b.from.x, b.from.y);
    g.bezierCurveTo(b.c1.x, b.c1.y, b.c2.x, b.c2.y, b.to.x, b.to.y);
    g.stroke();
    g.restore();
  }

  function drawNode(g, n) {
    const r = rectOf(n);
    const muted = Boolean(n.muted);
    const picked = st.selected === n.id;

    // Body and border.
    g.fillStyle = muted ? COLOURS.bodyMuted : COLOURS.body;
    roundRect(g, r.x, r.y, r.w, r.h, 5);
    g.fill();
    g.strokeStyle = picked ? COLOURS.active : COLOURS.border;
    g.lineWidth = picked ? 2 : 1;
    roundRect(g, r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1, 5);
    g.stroke();

    // Title.
    const titleColour = n.missing ? COLOURS.missing : muted ? COLOURS.titleMuted : COLOURS[n.kind] || COLOURS.instrument;
    g.fillStyle = titleColour;
    g.beginPath();
    g.roundRect(r.x + 1, r.y + 1, r.w - 2, TITLE_H - 1, [5, 5, 0, 0]);
    g.fill();
    g.fillStyle = COLOURS.text;
    g.font = "bold 12px sans-serif";
    g.textAlign = "left";
    g.textBaseline = "middle";
    const flagged = n.kind === "instrument" && n.flagged;
    g.fillText(fitText(g, n.name || "", r.w - 12 - (flagged ? 20 : 0)), r.x + 6, r.y + TITLE_H / 2 + 0.5);

    // MIDI In: its light, lit as MIDI comes through.
    if (n.kind === "midiin") {
      const lit = Date.now() - st.heardAt < 180;
      g.fillStyle = lit ? COLOURS.lit : COLOURS.unlit;
      g.beginPath();
      g.arc(r.x + r.w - 12, r.y + TITLE_H / 2, 4.5, 0, Math.PI * 2);
      g.fill();
      g.strokeStyle = "#111";
      g.lineWidth = 1;
      g.stroke();
    }

    // Sampled: Sampler Arena's flag, a white box with a red dot.
    if (flagged) {
      const lw = 14, lh = 10, lx = r.x + r.w - lw - 6, ly = r.y + TITLE_H / 2 - lh / 2;
      g.fillStyle = "#ffffff";
      roundRect(g, lx, ly, lw, lh, 2);
      g.fill();
      g.fillStyle = "#ff2020";
      g.beginPath();
      g.arc(lx + lw / 2, ly + lh / 2, 3, 0, Math.PI * 2);
      g.fill();
    }

    // A line about it, right of the buttons.
    const buttons = buttonsOf(n);
    const textX = buttons.length ? buttons[buttons.length - 1].x + BTN + 6 : r.x + 8;
    g.font = "10px sans-serif";
    g.fillStyle = n.missing ? "#ff8a8a" : COLOURS.muted;
    const about = n.missing ? "missing" : n.kind === "sampler" ? "MIDI out" : n.kind === "output" ? "stereo in" : n.kind === "midiin" ? "" : (FORMAT_LABELS[n.format] || n.format || "");
    if (about) g.fillText(fitText(g, about, r.x + r.w - textX - 6), textX, r.y + r.h - BTN_MARGIN - BTN / 2);
    if (n.kind === "midiin") {
      g.fillStyle = COLOURS.muted;
      g.fillText("live: its inputs and the keys", r.x + BTN_LEFT, r.y + TITLE_H + 12);
    }
    if (n.kind === "output") {
      g.fillStyle = COLOURS.muted;
      g.fillText(st.rack && st.rack.ready ? "sampled" : "wire a chain here", r.x + 8, r.y + TITLE_H + 12);
    }
    if (n.kind === "sampler") {
      g.fillStyle = COLOURS.muted;
      g.fillText("the studio's notes", r.x + BTN_LEFT, r.y + TITLE_H + 12);
    }

    // Buttons.
    for (const b of buttons) {
      if (b.key === "inputs") {
        // The MIDI In's list: what it hears, and the arrow that drops it.
        const open = Boolean(st.drop && !st.drop.el.hidden && st.drop.id === n.id);
        g.fillStyle = open ? "#4a4a5c" : "#3a3a48";
        roundRect(g, b.x, b.y, b.w, b.h, 3);
        g.fill();
        g.strokeStyle = "#5c5c70";
        g.lineWidth = 1;
        roundRect(g, b.x + 0.5, b.y + 0.5, b.w - 1, b.h - 1, 3);
        g.stroke();
        g.fillStyle = "#e6e6e6";
        g.font = "11px sans-serif";
        g.textAlign = "left";
        g.fillText(fitText(g, b.label, b.w - 22), b.x + 6, b.y + b.h / 2 + 0.5);
        g.fillText("\u25be", b.x + b.w - 13, b.y + b.h / 2 + 0.5);
        continue;
      }
      let fill = "#555", ink = "#000", label = b.label;
      if (b.key === "settings" || b.key === "window") { fill = "#008b8b"; if (b.key === "window" && !n.hasWindow) fill = "#3c5050"; }
      if (b.key === "folder") { fill = "#3c3c41"; ink = "#c8b464"; }
      if (b.key === "mute") fill = muted ? "#ff3030" : "#90ee90";
      if (b.key === "pass") fill = n.pass ? "#ffff00" : "#404040";
      if (b.key === "remove") { fill = "#8b0000"; ink = "#ffffff"; }
      g.fillStyle = fill;
      roundRect(g, b.x, b.y, b.w, b.h, 3);
      g.fill();
      g.fillStyle = ink;
      g.font = "bold 11px sans-serif";
      g.textAlign = "center";
      g.fillText(label, b.x + b.w / 2, b.y + b.h / 2 + 0.5);
      g.textAlign = "left";
    }

    // Connectors: red for MIDI, blue for sound (L and R on them).
    const pins = pinsOf(n);
    const drawPin = (p, kind) => {
      const hot = samePin(st.hover, n, p, kind);
      const radius = hot ? PIN_R + 2 : PIN_R;
      g.fillStyle = p.midi ? COLOURS.midi : COLOURS.audio;
      g.beginPath();
      g.arc(p.x, p.y, radius, 0, Math.PI * 2);
      g.fill();
      g.strokeStyle = "#111";
      g.lineWidth = 1;
      g.stroke();
      if (!p.midi) {
        g.fillStyle = "#04202f";
        g.font = `bold ${hot ? 9 : 8}px sans-serif`;
        g.textAlign = "center";
        g.fillText(p.side ? "R" : "L", p.x, p.y + 0.5);
        g.textAlign = "left";
      }
    };
    for (const p of pins.inputs) drawPin(p, "input");
    for (const p of pins.outputs) drawPin(p, "output");
  }

  // ---------------------------------------------------------------- the bar over the area
  function renderBar() {
    const box = $("smp-nodes-formats");
    if (!box) return;
    box.textContent = "";
    for (const f of st.formats) {
      const b = element("button", "smp-button small smp-format", FORMAT_LABELS[f.id] || f.id);
      b.type = "button";
      b.disabled = !f.available || busy();
      b.title = f.available ? `Load a ${f.id} plugin from its folder (${f.folder})` : f.id === "VST2" ? "VST2 plugins: this Sonify was built without Steinberg's VST2 SDK" : `${f.id} plugins can't be loaded here`;
      b.addEventListener("click", () => openBrowse(f.id));
      box.appendChild(b);
    }
    const status = $("smp-nodes-status");
    const flagged = nodes().filter((n) => n.kind === "instrument" && n.flagged).length;
    const vstis = nodes().filter((n) => n.kind === "instrument").length;
    status.classList.toggle("ready", flagged > 0);
    if (st.drop && !st.drop.el.hidden) renderInputs();
    status.textContent = flagged ? `${flagged} VSTi${flagged > 1 ? "s" : ""} sampled${flagged > 1 ? " together" : ""}`
                       : vstis ? "Wire the Sampling node's MIDI to a VSTi, and its sound to the Output"
                       : "Load a VSTi to sample";
  }

  // ---------------------------------------------------------------- the browse window
  function openBrowse(format) {
    if (!st.browse) buildBrowse();
    st.browse.format = format;
    st.browse.entries = [];
    st.browse.picked = -1;
    st.browse.el.hidden = false;
    $("smp-browse-title").textContent = `Load a ${format} plugin`;
    $("smp-browse-list").textContent = "";
    send("browse", { format, folder: "" });
  }

  function closeBrowse() {
    if (st.browse) st.browse.el.hidden = true;
  }

  function buildBrowse() {
    const el = element("div", "smp-browse");
    el.setAttribute("role", "dialog");
    el.setAttribute("aria-label", "Load a plugin");
    el.innerHTML = '<div class="smp-browse-box">'
      + '<div class="smp-browse-head"><span id="smp-browse-title" class="smp-browse-title">Load a plugin</span>'
      + '<button id="smp-browse-close" class="smp-browse-close" type="button" title="Close" aria-label="Close">\u00d7</button></div>'
      + '<div class="smp-browse-path"><button id="smp-browse-up" class="smp-button small" type="button" title="Up a folder">Up</button>'
      + '<input id="smp-browse-folder" class="smp-browse-folder" type="text" spellcheck="false" aria-label="Folder" title="Type a folder and press Enter">'
      + '<button id="smp-browse-usual" class="smp-button small" type="button" title="The usual folder for this format">Usual Folder</button></div>'
      + '<div id="smp-browse-list" class="smp-browse-list" role="listbox" aria-label="Folders and plugins" tabindex="0"></div>'
      + '<div class="smp-browse-foot"><span class="smp-browse-hint">Double-click a folder to open it, a plugin to load it.</span>'
      + '<button id="smp-browse-cancel" class="smp-button small" type="button">Cancel</button>'
      + '<button id="smp-browse-load" class="smp-button small smp-browse-load" type="button" disabled>Load</button></div>'
      + '</div>';
    document.body.appendChild(el);
    st.browse = { el, format: "VST3", folder: "", parent: "", entries: [], picked: -1 };

    const go = (folder) => send("browse", { format: st.browse.format, folder });
    $("smp-browse-close").addEventListener("click", closeBrowse);
    $("smp-browse-cancel").addEventListener("click", closeBrowse);
    $("smp-browse-up").addEventListener("click", () => { if (st.browse.parent) go(st.browse.parent); });
    $("smp-browse-usual").addEventListener("click", () => {
      const f = st.formats.find((x) => x.id === st.browse.format);
      go(f ? f.defaultFolder : "");
    });
    $("smp-browse-folder").addEventListener("keydown", (event) => {
      event.stopPropagation();
      if (event.key === "Enter") go($("smp-browse-folder").value.trim());
      if (event.key === "Escape") closeBrowse();
    });
    $("smp-browse-load").addEventListener("click", () => openEntry(st.browse.entries[st.browse.picked]));
    el.addEventListener("keydown", (event) => {
      if (event.target.tagName === "INPUT") return;
      if (event.key === "Escape") { event.stopPropagation(); closeBrowse(); }
      if (event.key === "Enter" && st.browse.picked >= 0) { event.preventDefault(); openEntry(st.browse.entries[st.browse.picked]); }
      if (event.key === "ArrowDown" || event.key === "ArrowUp") {
        event.preventDefault();
        const count = st.browse.entries.length;
        if (!count) return;
        pickEntry(clamp(st.browse.picked + (event.key === "ArrowDown" ? 1 : -1), 0, count - 1));
      }
      if (event.key === "Backspace" && st.browse.parent) { event.preventDefault(); go(st.browse.parent); }
    });
    el.addEventListener("pointerdown", (event) => { if (event.target === el) closeBrowse(); });
  }

  function pickEntry(index) {
    st.browse.picked = index;
    const items = $("smp-browse-list").children;
    for (let i = 0; i < items.length; i++) items[i].classList.toggle("on", i === index);
    if (items[index]) items[index].scrollIntoView({ block: "nearest" });
    const entry = st.browse.entries[index];
    $("smp-browse-load").disabled = !entry || entry.kind !== "plugin";
  }

  function openEntry(entry) {
    if (!entry) return;
    if (entry.kind === "folder") { send("browse", { format: st.browse.format, folder: entry.path }); return; }
    loadPlugin(st.browse.format, entry.path, "");
  }

  /** Loaded by Sonify; placed here once it's known to be a VSTi or an effect. */
  function loadPlugin(format, path, pluginId) {
    closeBrowse();
    Hub.toast("Loading the plugin\u2026");
    send("load", { format, path, pluginId, x: -1, y: -1 });
  }

  function renderBrowse(data) {
    if (!st.browse || st.browse.el.hidden || data.format !== st.browse.format) return;
    st.browse.folder = data.folder || "";
    st.browse.parent = data.parent || "";
    st.browse.entries = Array.isArray(data.entries) ? data.entries : [];
    st.browse.picked = -1;
    $("smp-browse-folder").value = st.browse.folder || "This computer";
    $("smp-browse-up").disabled = !st.browse.parent;
    const list = $("smp-browse-list");
    list.textContent = "";
    if (!st.browse.entries.length) list.appendChild(element("div", "smp-browse-empty", `No folders or ${data.format} plugins here.`));
    st.browse.entries.forEach((entry, i) => {
      const item = element("div", "smp-browse-item " + entry.kind);
      item.setAttribute("role", "option");
      item.appendChild(element("span", "smp-browse-icon", entry.kind === "folder" ? "\u25b8" : "\u25c6"));
      item.appendChild(element("span", "smp-browse-name", entry.name));
      if (entry.kind === "plugin") item.appendChild(element("span", "smp-browse-kind", data.format));
      item.addEventListener("click", () => pickEntry(i));
      item.addEventListener("dblclick", () => openEntry(entry));
      list.appendChild(item);
    });
    $("smp-browse-load").disabled = true;
    list.focus();
  }

  // ---------------------------------------------------------------- pressing, dragging
  function local(event) {
    const r = $("smp-nodes-canvas").getBoundingClientRect();
    return { x: event.clientX - r.left, y: event.clientY - r.top };
  }

  function pressButton(n, key) {
    if (key === "inputs") { toggleInputs(n); return; }
    if (key === "settings") { if (Hub.sampling && Hub.sampling.openSettings) Hub.sampling.openSettings(); return; }
    if (key === "folder") { Hub.send("sampling", { action: "reveal" }); return; }
    if (key === "window") { send("window", { id: n.id }); return; }
    if (key === "mute") { n.muted = !n.muted; draw(); send("mute", { id: n.id, on: n.muted }); return; }
    if (key === "pass") { n.pass = !n.pass; draw(); send("pass", { id: n.id, on: n.pass }); return; }
    if (key === "remove") removeNode(n);
  }

  function removeNode(n) {
    if (!n || !isPlugin(n) || busy()) return;
    if (st.selected === n.id) st.selected = 0;
    send("remove", { id: n.id });
  }

  function onDown(event) {
    const canvas = $("smp-nodes-canvas");
    canvas.focus();
    const p = local(event);
    const hit = hitAt(p);
    if (event.button === 2) { showMenu(event, hit); return; }
    if (event.button !== 0) return;
    event.preventDefault();

    if (hit.kind === "button") {
      if (busy() && ["remove"].includes(hit.button.key)) return;
      pressButton(hit.node, hit.button.key);
      return;
    }

    if (hit.kind === "output") {
      if (busy()) return;
      st.drag = { kind: "wire", from: hit.node.id, fromSide: hit.pin.side, midi: hit.pin.midi, at: p };
      canvas.setPointerCapture(event.pointerId);
      draw();
      return;
    }

    if (hit.kind === "input") {
      // Its last wire picked up by the end: moved to another input, or let go.
      const pin = hit.pin;
      const into = wires().filter((w) => w.to === hit.node.id && (pin.midi ? Boolean(w.midi) : !w.midi && sideOf(w.toSide) === pin.side));
      if (into.length && !busy()) {
        const w = into[into.length - 1];
        st.rack.wires = wires().filter((x) => x !== w);
        send("disconnect", wireData(w));
        st.drag = { kind: "wire", from: w.from, fromSide: sideOf(w.fromSide), midi: Boolean(w.midi), at: p };
        canvas.setPointerCapture(event.pointerId);
        draw();
      }
      return;
    }

    if (hit.kind === "node") {
      st.selected = hit.node.id;
      st.wire = null;
      const r = rectOf(hit.node);
      st.drag = { kind: "move", id: hit.node.id, dx: p.x - r.x, dy: p.y - r.y, x: r.x, y: r.y, moved: false };
      canvas.setPointerCapture(event.pointerId);
      draw();
      return;
    }

    if (hit.kind === "wire") {
      st.wire = hit.wire;
      st.selected = 0;
      draw();
      return;
    }

    st.selected = 0;
    st.wire = null;
    draw();
  }

  function onMove(event) {
    const p = local(event);
    const d = st.drag;
    if (d && d.kind === "move") {
      d.x = clamp(p.x - d.dx, MARGIN, Math.max(MARGIN, st.size.w - NODE_W - MARGIN));
      d.y = clamp(p.y - d.dy, MARGIN, Math.max(MARGIN, st.size.h - NODE_H - MARGIN));
      d.moved = true;
      draw();
      return;
    }
    if (d && d.kind === "wire") {
      d.at = p;
      const hit = hitAt(p);
      st.hover = hit.kind === "input" && hit.pin.midi === d.midi && canWire(byId(d.from), hit.node) ? hit : null;
      draw();
      return;
    }
    const hit = hitAt(p);
    const before = st.hover;
    st.hover = hit.kind === "input" || hit.kind === "output" ? hit : null;
    const key = (h) => (h ? `${h.kind}:${h.node.id}:${h.pin.side}:${h.pin.midi}` : "");
    if (key(before) !== key(st.hover)) draw();
    const canvas = $("smp-nodes-canvas");
    canvas.style.cursor = hit.kind === "button" ? "pointer" : hit.kind === "output" || hit.kind === "input" ? "crosshair"
                        : hit.kind === "node" ? "grab" : hit.kind === "wire" ? "pointer" : "default";
  }

  function onUp(event) {
    const d = st.drag;
    st.drag = null;
    if (!d) return;
    if (d.kind === "move") {
      const n = byId(d.id);
      if (n && d.moved) {
        n.x = Math.round(d.x);
        n.y = Math.round(d.y);
        send("move", { id: n.id, x: n.x, y: n.y });
      }
      draw();
      return;
    }
    if (d.kind === "wire") {
      const hit = hitAt(local(event));
      st.hover = null;
      const target = hit.node;
      if (target && target.id !== d.from && (hit.kind === "input" || hit.kind === "node" || hit.kind === "button")) {
        const source = byId(d.from);
        const fits = canWire(source, target);
        const add = (w) => { if (fits && !wires().some((x) => sameWire(x, w))) st.rack.wires = wires().concat([w]); };
        if (d.midi) {
          add({ from: d.from, to: target.id, midi: true });
          send("connect", { from: d.from, to: target.id });
        } else if (hit.kind === "input") {
          // On a connector: that side.
          const w = { from: d.from, to: target.id, fromSide: d.fromSide, toSide: hit.pin.side, midi: false };
          if (!hit.pin.midi) add(w);
          send("connect", wireData(w));
        } else {
          // On the node: both sides, each to its own.
          add({ from: d.from, to: target.id, fromSide: 0, toSide: 0, midi: false });
          add({ from: d.from, to: target.id, fromSide: 1, toSide: 1, midi: false });
          send("connect", { from: d.from, to: target.id });
        }
      }
      draw();
    }
  }

  function showMenu(event, hit) {
    event.preventDefault();
    const at = Hub.pointAnchor ? Hub.pointAnchor(event) : { getBoundingClientRect: () => ({ left: event.clientX, right: event.clientX, top: event.clientY, bottom: event.clientY }) };
    const items = [];
    const n = hit.node;
    if (n && isPlugin(n)) {
      st.selected = n.id;
      draw();
      items.push({ label: "Open Plugin Window", value: "window", disabled: !n.hasWindow });
      items.push({ label: "Mute", value: "mute", checked: Boolean(n.muted) });
      if (n.kind === "effect") items.push({ label: "Pass Through", value: "pass", checked: Boolean(n.pass) });
      items.push({ separator: true }, { label: "Remove", value: "remove", disabled: busy() });
    } else if (n && n.kind === "sampler") {
      items.push({ label: "Sampler Settings\u2026", value: "settings" }, { label: "Open the Samples Folder", value: "folder" });
    } else if (n && n.kind === "midiin") {
      items.push({ label: "MIDI Inputs\u2026", value: "inputs" }, { label: "Show the MIDI Monitor", value: "monitor" });
    } else if (hit.kind === "wire") {
      st.wire = hit.wire;
      draw();
      items.push({ label: "Remove Wire", value: "unwire", disabled: busy() });
    }
    if (!n || n.kind === "output") {
      if (items.length) items.push({ separator: true });
      for (const f of st.formats) items.push({ label: `Load ${f.id} Plugin\u2026`, value: "load:" + f.id, disabled: !f.available || busy() });
    }
    if (!items.length) return;
    Hub.popupMenu(at, items, (pick) => {
      if (typeof pick !== "string") return;
      if (pick.startsWith("load:")) { openBrowse(pick.slice(5)); return; }
      if (pick === "unwire" && st.wire) { send("disconnect", wireData(st.wire)); st.wire = null; return; }
      if (pick === "remove") { removeNode(n); return; }
      if (pick === "monitor") { if (Hub.samplingMidi) Hub.samplingMidi.showMonitor(); return; }
      if (n) pressButton(n, pick);
    });
  }

  function onKey(event) {
    if (event.key !== "Delete" && event.key !== "Backspace") return;
    event.preventDefault();
    event.stopPropagation();
    if (busy()) return;
    if (st.wire) { send("disconnect", wireData(st.wire)); st.wire = null; return; }
    const n = byId(st.selected);
    if (n) removeNode(n);
  }

  // ---------------------------------------------------------------- the MIDI In node's list
  /** Dropped from the node's button: All inputs, or the ports ticked one by one. */
  function toggleInputs(n) {
    if (st.drop && !st.drop.el.hidden && st.drop.id === n.id) { closeInputs(); return; }
    if (!st.drop) {
      const el = element("div", "smp-midiin-drop");
      el.setAttribute("role", "dialog");
      el.setAttribute("aria-label", "MIDI In: the inputs it hears");
      el.hidden = true;
      document.body.appendChild(el);
      el.addEventListener("keydown", (event) => { if (event.key === "Escape") { event.stopPropagation(); closeInputs(); } });
      st.drop = { el, id: 0 };
      document.addEventListener("pointerdown", (event) => {
        if (!st.drop || st.drop.el.hidden || st.drop.el.contains(event.target)) return;
        // A press on the node's own button toggles it there.
        if (event.target === $("smp-nodes-canvas")) {
          const hit = hitAt(local(event));
          if (hit.kind === "button" && hit.button.key === "inputs") return;
        }
        closeInputs();
      }, true);
    }
    st.drop.id = n.id;
    st.drop.el.hidden = false;
    renderInputs();
    draw();
  }

  function closeInputs() {
    if (!st.drop || st.drop.el.hidden) return;
    st.drop.el.hidden = true;
    draw();
  }

  function renderInputs() {
    const drop = st.drop;
    const n = drop && byId(drop.id);
    if (!drop || !n) { closeInputs(); return; }
    const chosen = Array.isArray(n.inputs) ? n.inputs : ["*"];
    const every = chosen.includes("*");
    const el = drop.el;
    el.textContent = "";

    const commit = (list) => {
      n.inputs = list;
      send("midiInputs", { inputs: list });
      renderInputs();
      draw();
    };

    const row = (label, checked, disabled, onChange, extra) => {
      const line = element("label", "smp-midiin-row" + (extra ? " " + extra : ""));
      const box = document.createElement("input");
      box.type = "checkbox";
      box.checked = checked;
      box.disabled = disabled;
      box.addEventListener("change", () => onChange(box.checked));
      line.appendChild(box);
      line.appendChild(element("span", "", label));
      el.appendChild(line);
    };

    el.appendChild(element("div", "smp-midiin-title", "MIDI In hears"));
    const ports = st.midiPorts.map((p) => p.name);
    // All off: every port here ticked to begin with, so nothing goes quiet until one is unticked.
    row("All inputs", every, false, (on) => commit(on ? ["*"] : ports.slice()), "every");
    for (const port of st.midiPorts) {
      const label = port.device ? `${port.name}  (${port.device})` : port.name;
      row(label, every || chosen.includes(port.name), every, (on) => {
        const list = chosen.filter((x) => x !== "*" && x !== port.name);
        commit(on ? list.concat([port.name]) : list);
      }, "port");
    }
    for (const name of chosen.filter((x) => x !== "*" && !ports.includes(x))) {
      row(`${name}  (not connected)`, true, every, (on) => { if (!on) commit(chosen.filter((x) => x !== name)); }, "port gone");
    }
    if (!st.midiPorts.length) el.appendChild(element("div", "smp-midiin-note", "No MIDI input is enabled: Audio Settings enables them."));
    el.appendChild(element("div", "smp-midiin-note", "The keyboard on the left always plays through it. Heard only while the Sampling Studio is in front."));

    // Under the node's button.
    const canvas = $("smp-nodes-canvas");
    const b = buttonsOf(n).find((x) => x.key === "inputs");
    if (!canvas || !b) return;
    const area = canvas.getBoundingClientRect();
    const width = Math.max(220, b.w + 60);
    const left = clamp(area.left + b.x, 8, Math.max(8, window.innerWidth - width - 8));
    el.style.width = `${width}px`;
    el.style.left = `${left}px`;
    el.style.top = `${area.top + b.y + b.h + 4}px`;
    const room = window.innerHeight - (area.top + b.y + b.h + 4) - 8;
    if (room < el.offsetHeight) el.style.top = `${Math.max(8, area.top + b.y - el.offsetHeight - 4)}px`;
  }

  /** MIDI came through the MIDI In node (the monitor says so): its light blinks. */
  function blink() {
    st.heardAt = Date.now();
    if (!st.visible) return;
    draw();
    clearTimeout(st.litTimer);
    st.litTimer = setTimeout(draw, 200);
  }

  // ---------------------------------------------------------------- Sonify's word
  function take(rack) {
    st.rack = rack || { nodes: [], wires: [], ready: false };
    if (st.selected && !byId(st.selected)) st.selected = 0;
    if (st.wire && !wires().some((w) => sameWire(w, st.wire))) st.wire = null;
    // Nodes with no place yet get one, kept (once the area has its size).
    const sized = st.visible && measure();
    for (const n of nodes()) {
      if (n.x >= 0 && n.y >= 0) { st.placing.delete(n.id); continue; }
      if (!sized || st.placing.has(n.id)) continue;
      const p = firstPlace(n);
      n.x = Math.round(p.x);
      n.y = Math.round(p.y);
      st.placing.add(n.id);
      send("move", { id: n.id, x: n.x, y: n.y });
    }
    renderBar();
    draw();
    Hub.emit("samplingNodes", st.rack);
  }

  Hub.on("sampling", (data) => {
    if (!data) return;
    if (data.kind === "rack") {
      st.formats = Array.isArray(data.formats) ? data.formats : st.formats;
      st.midiPorts = Array.isArray(data.midiPorts) ? data.midiPorts : st.midiPorts;
      take(data.rack);
    } else if (data.kind === "rackBrowse") {
      renderBrowse(data);
    } else if (data.kind === "rackChoose") {
      const plugins = Array.isArray(data.plugins) ? data.plugins : [];
      const anchor = $("smp-nodes-canvas");
      Hub.popupMenu(anchor, [{ group: `${data.path.split(/[\\/]/).pop()} holds ${plugins.length} plugins: which one?` }]
        .concat(plugins.map((p) => ({ label: p.vendor ? `${p.name}  (${p.vendor})` : p.name, value: p.id }))), (id) => {
        if (typeof id === "string" && id) send("load", { format: data.format, path: data.path, pluginId: id, x: data.x, y: data.y });
      });
    }
  });

  /** Shown or not (the Nodes source, VSTi or Chord Sampling): drawn to its area's size. The MIDI
      In node listens meanwhile. */
  function setVisible(on) {
    on = Boolean(on);
    const box = $("smp-nodes");
    if (box) box.hidden = !on;
    if (st.visible === on) { if (on) requestAnimationFrame(() => { renderBar(); draw(); }); return; }
    st.visible = on;
    send("visible", { on });
    if (!on) { closeInputs(); closeBrowse(); }
    if (on) {
      requestAnimationFrame(() => { renderBar(); draw(); take(st.rack); });
      if (!st.rack) send("state");
    }
  }

  Hub.samplingNodes = {
    setVisible,
    isVisible: () => st.visible,
    isReady: () => Boolean(st.rack && st.rack.ready),
    refresh: () => { renderBar(); draw(); },
    blink
  };

  Hub.on("ready", () => {
    const canvas = $("smp-nodes-canvas");
    if (!canvas) return;
    canvas.addEventListener("pointerdown", onDown);
    canvas.addEventListener("pointermove", onMove);
    canvas.addEventListener("pointerup", onUp);
    canvas.addEventListener("pointercancel", () => { st.drag = null; st.hover = null; draw(); });
    canvas.addEventListener("pointerleave", () => { if (!st.drag && st.hover) { st.hover = null; draw(); } });
    canvas.addEventListener("dblclick", (event) => {
      const hit = hitAt(local(event));
      if (hit.kind === "node" && isPlugin(hit.node)) send("window", { id: hit.node.id });
    });
    canvas.addEventListener("contextmenu", (event) => event.preventDefault());
    canvas.addEventListener("keydown", onKey);
    new ResizeObserver(() => { if (st.visible) draw(); }).observe($("smp-nodes-wrap"));
  });
})();
