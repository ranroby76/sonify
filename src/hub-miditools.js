// D:\workspace\Fanan_web_hub\src\hub-miditools.js
//
// MIDI FX and MIDI tools.
//
//   MIDI FX     an instrument channel's devices in front of its instrument (clips and live notes
//               go through them), on its Console strip: + MIDI FX adds one (a built-in device, an
//               installed Magenta tool, or a Sonify device from My Devices, made in the Device
//               Studio: hub-devices.js), a click opens its window (its settings; a Sonify device's
//               outputs and their Map buttons too), the light switches it off; in the strip's
//               panel they also go (×) and reorder (drag).
//   MIDI tab    the Browser's: every MIDI tool. The built-in ones are always there; Magenta's
//               models install from the catalog (Fanan's server adds and updates them). Drag one
//               onto a track or a channel strip for its MIDI FX, or double-click it for the
//               selected track's.
//   Apply       Event > MIDI Tools… (and the Editor's Tools button): a tool on the selected notes
//               (or the clip's), or on the selected clips; each Apply is one undo step. AI Tools'
//               MIDI Generators tab (hub-aitools.js) lists the same tools and applies them to the
//               clip it picks (Hub.midiGenerators).
//   Magenta     the models run here, on this computer (magenta.js and TensorFlow.js, built into
//               Sonify): for Apply, and for the live Magenta device, whose windows the engine
//               sends as they end and which are answered here.
//
// Every tool keeps to the song channel's key where it plays notes of its own.

(() => {
  "use strict";

  const { $, element } = Hub;

  // ---------------------------------------------------------------- what there is
  let types = [];                 // the engine's kinds of device (midiFxTypes)
  let maxDevices = 8;
  const installed = new Map();    // Magenta tool id -> its definition (installed)
  let magentaBuilt = true;        // magenta.js is in this Sonify
  const installing = new Map();   // tool id -> fraction downloaded
  const status = new Map();       // "trackId:fxId" -> what a live Magenta device is doing

  const typeById = (id) => types.find((t) => t.id === id) || null;
  const builtinTypes = () => types.filter((t) => !t.magenta);

  /** A device's kind as its window shows it: a built-in kind, or (a Sonify device, made in the
      Plugins Creator Studio) its own build: its settings and outputs. */
  function typeOf(device) {
    if (device && device.type === "device") {
      const build = device.device || {};
      return { id: "device", sonify: true, name: build.name || device.name, description: build.description || "",
               params: build.params || [], outputs: build.outputs || [] };
    }
    return typeById(device && device.type);
  }

  // The Magenta tools Sonify knows; Fanan's server adds to them and updates them (kind miditool).
  const CHECKPOINTS = "https://storage.googleapis.com/magentadata/js/checkpoints";
  const CATALOG = [
    { id: "magenta.melody-rnn", title: "Continue Melody", model: "MusicRNN", checkpoint: `${CHECKPOINTS}/music_rnn/basic_rnn`, sizeMb: 13,
      actions: ["continue"], live: "continue", version: "1",
      description: "Carries a melody on: in a clip, more bars after it; live, an answer to what you play." },
    { id: "magenta.drums-rnn", title: "Continue Drums", model: "MusicRNN", checkpoint: `${CHECKPOINTS}/music_rnn/drum_kit_rnn`, sizeMb: 11.9,
      actions: ["continue"], live: "continue", drums: true, version: "1",
      description: "Carries a drum part on: in a clip, more bars after it; live, an answer to what you play." },
    { id: "magenta.improvise", title: "Improvise", model: "MusicRNN", checkpoint: `${CHECKPOINTS}/music_rnn/chord_pitches_improv`, sizeMb: 5.6,
      actions: ["improvise"], live: "improvise", chords: true, version: "1",
      description: "A melody over chords: a clip's, on a new track; live, over the chords you play." },
    { id: "magenta.melody-vae", title: "Melody Maker", model: "MusicVAE", checkpoint: `${CHECKPOINTS}/music_vae/mel_2bar_small`, sizeMb: 17.7,
      actions: ["generate", "vary", "morph"], live: "generate", generator: true, version: "1",
      description: "Two-bar melodies: new ones, variations of a clip's, or a morph from one clip to another." },
    { id: "magenta.drums-vae", title: "Drum Maker", model: "MusicVAE", checkpoint: `${CHECKPOINTS}/music_vae/drums_2bar_lokl_small`, sizeMb: 18.5,
      actions: ["generate", "vary", "morph"], live: "generate", generator: true, drums: true, version: "1",
      description: "Two-bar drum patterns: new ones, variations of a clip's, or a morph from one clip to another." },
    { id: "magenta.groove", title: "Groove", model: "MusicVAE", checkpoint: `${CHECKPOINTS}/music_vae/groovae_2bar_humanize`, sizeMb: 15.8,
      actions: ["groove"], live: "groove", drums: true, version: "1",
      description: "Plays a stiff drum part the way a drummer would: its timing and its dynamics." },
    { id: "magenta.drumify", title: "Drumify", model: "MusicVAE", checkpoint: `${CHECKPOINTS}/groovae/tap2drum_2bar`, sizeMb: 15.6,
      actions: ["drumify"], live: "drumify", drums: true, version: "1",
      description: "Turns a rhythm (any notes, or one tapped on a pad) into a whole drum beat." },
    { id: "magenta.harmonize", title: "Harmonize", model: "Coconet", checkpoint: `${CHECKPOINTS}/coconet/bach`, sizeMb: 2,
      actions: ["harmonize"], live: "harmonize", version: "1",
      description: "Bach-style harmony for a melody: three more voices under it." }
  ];

  const ACTIONS = { continue: "Continue", generate: "New", vary: "Vary", morph: "Morph", groove: "Groove",
                    drumify: "Drumify", improvise: "Improvise", harmonize: "Harmonize" };

  /** A Magenta tool by id: as installed, else as the catalog has it. */
  const toolById = (id) => installed.get(id) || CATALOG.find((t) => t.id === id) || null;

  // ---------------------------------------------------------------- keys
  const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const SCALE_NOTES = { "major": 0x0ab5, "minor": 0x05ad, "dorian": 0x06ad, "phrygian": 0x05ab, "lydian": 0x0ad5,
                        "mixolydian": 0x06b5, "locrian": 0x056b, "harmonic minor": 0x09ad, "melodic minor": 0x0aad,
                        "major pentatonic": 0x0295, "minor pentatonic": 0x04a9, "blues": 0x04e9, "chromatic": 0x0fff };

  const pitchName = (p) => NOTE_NAMES[((p % 12) + 12) % 12] + (Math.floor(p / 12) - 1);

  function keyAt(ppq) {
    const key = Hub.tempoMap.keyAt(Math.max(0, ppq));
    return { root: key ? key.root : 0, notes: SCALE_NOTES[key && key.scale] || SCALE_NOTES.major };
  }

  const inKey = (pitch, key) => ((key.notes >> ((((pitch - key.root) % 12) + 12) % 12)) & 1) === 1;

  /** The key's nearest note (the lower on a tie). */
  function snapToKey(pitch, key) {
    if (inKey(pitch, key)) return pitch;
    for (let d = 1; d < 12; d++) {
      if (inKey(pitch - d, key)) return pitch - d;
      if (inKey(pitch + d, key)) return pitch + d;
    }
    return pitch;
  }

  // ---------------------------------------------------------------- parameter controls
  function valueText(param, value) {
    const v = Number(value);
    switch (param.unit) {
      case "choice": case "rate": return (param.choices || [])[Math.round(v)] || "";
      case "toggle": return v > 0.5 ? "On" : "Off";
      case "percent": return `${Math.round(v)} %`;
      case "semitones": return `${v > 0 ? "+" : ""}${Math.round(v)} st`;
      case "steps": return `${v > 0 ? "+" : ""}${Math.round(v)}`;
      case "pitch": return pitchName(Math.round(v));
      case "track": { const t = Hub.trackById(Math.round(v)); return t ? t.name : "None"; }
      case "ms": return `${Math.round(v)} ms`;
      case "decimal": return v.toFixed(2);
      default:
        if (param.id === "velocity" && v === 0 && param.min === 0) return "As played";
        return String(Math.round(v));
    }
  }

  /** A row for one parameter: a list, a switch or a slider with its value. set() shows a value. */
  function paramControl(param, value, onChange) {
    const row = element("div", "param midifx-param");
    const name = element("span", "param-name", param.name);
    name.title = param.name;
    row.appendChild(name);

    if (param.unit === "choice" || param.unit === "rate") {
      const select = element("select", "midifx-choice");
      select.setAttribute("aria-label", param.name);
      (param.choices || []).forEach((label, index) => select.add(new Option(label, String(index))));
      select.value = String(Math.round(value));
      select.addEventListener("change", () => { onChange(Number(select.value)); select.blur(); });
      row.appendChild(select);
      return { row, set: (v) => { if (document.activeElement !== select) select.value = String(Math.round(v)); } };
    }

    // A track (Harmonize follows one): the song's instrument tracks, listed afresh each time it opens.
    if (param.unit === "track") {
      const select = element("select", "midifx-choice");
      select.setAttribute("aria-label", param.name);
      const fill = (v) => {
        const id = Math.round(Number(v)) || 0;
        select.textContent = "";
        select.add(new Option("None", "0"));
        for (const t of Hub.state.tracks || [])
          if (t.kind === "instrument") select.add(new Option(`${t.number || ""}  ${t.name}`.trim(), String(t.id)));
        if (id && !(Hub.state.tracks || []).some((t) => t.id === id)) select.add(new Option("(a track that's gone)", String(id)));
        select.value = String(id);
      };
      fill(value);
      select.addEventListener("focus", () => fill(select.value));
      select.addEventListener("change", () => { onChange(Number(select.value)); select.blur(); });
      row.appendChild(select);
      return { row, set: (v) => { if (document.activeElement !== select) fill(v); } };
    }

    if (param.unit === "toggle") {
      const box = element("input", "midifx-toggle");
      box.type = "checkbox";
      box.checked = Number(value) > 0.5;
      box.setAttribute("aria-label", param.name);
      box.addEventListener("change", () => onChange(box.checked ? 1 : 0));
      row.appendChild(box);
      return { row, set: (v) => { box.checked = Number(v) > 0.5; } };
    }

    const input = element("input");
    input.type = "range";
    input.min = String(param.min);
    input.max = String(param.max);
    input.step = param.unit === "decimal" ? "0.05" : "1";
    input.value = String(value);
    input.setAttribute("aria-label", param.name);
    const shown = element("span", "param-value", valueText(param, value));
    const fill = () => input.style.setProperty("--fill", `${Hub.clamp01((Number(input.value) - param.min) / ((param.max - param.min) || 1)) * 100}%`);
    input.addEventListener("input", () => {
      shown.textContent = valueText(param, input.value);
      fill();
      onChange(Number(input.value));
    });
    input.addEventListener("dblclick", () => {
      input.value = String(param.value);
      input.dispatchEvent(new Event("input"));
    });
    fill();
    row.append(input, shown);
    return {
      row,
      set: (v) => {
        if (document.activeElement === input) return;
        input.value = String(v);
        shown.textContent = valueText(param, v);
        fill();
      }
    };
  }

  // ---------------------------------------------------------------- devices on a track
  const devicesOf = (trackId) => { const t = Hub.trackById(trackId); return (t && t.midiFx) || []; };
  const isInstrument = (track) => Boolean(track) && track.kind === "instrument";

  /** Adds a device to a track: { type } for a built-in one, { tool } for a Magenta tool. */
  function addDevice(trackId, choice, position = -1, open = true) {
    const track = Hub.trackById(trackId);
    if (!isInstrument(track)) { Hub.toast("MIDI FX go on instrument tracks."); return; }
    if (devicesOf(trackId).length >= maxDevices) { Hub.toast(`That track has ${maxDevices} MIDI FX already.`); return; }

    if (choice.tool) {
      const tool = installed.get(choice.tool);
      if (!tool) { Hub.toast("Install that tool first (the Browser's MIDI tab)."); return; }
      Hub.send("midiFxAdd", { trackId, type: "magenta", position, tool: tool.id, toolName: tool.title || tool.id,
                              generator: Boolean(tool.generator), open,
                              params: { mode: tool.live === "drumify" || tool.live === "groove" ? 2 : (tool.generator ? 1 : 0) } });
      return;
    }

    if (choice.deviceFile) {
      Hub.send("midiFxAdd", { trackId, type: "device", file: choice.deviceFile, position, open });
      return;
    }

    Hub.send("midiFxAdd", { trackId, type: choice.type, position, open });
  }

  function addMenu(anchor, trackId) {
    const items = [{ group: "Built in" }];
    for (const type of builtinTypes()) items.push({ label: type.name, value: { type: type.id } });
    items.push({ group: "Magenta" });
    const tools = [...installed.values()];
    for (const tool of tools) items.push({ label: tool.title || tool.id, value: { tool: tool.id } });
    if (!tools.length) items.push({ label: "Install Magenta tools in the Browser's MIDI tab", value: { browse: true } });

    // Sonify devices, made in the Device Studio.
    items.push({ group: "My Devices" });
    const mine = Hub.devices ? Hub.devices.library() : [];
    for (const entry of mine) items.push({ label: entry.name, value: { deviceFile: entry.file } });
    items.push({ label: mine.length ? "New Device in the Plugins Creator Studio…" : "Make a device in the Plugins Creator Studio…", value: { studio: true } });

    if (devicesOf(trackId).length >= maxDevices) items.unshift({ label: `Full (${maxDevices} MIDI FX)`, value: null, disabled: true });
    Hub.popupMenu(anchor, items, (choice) => {
      if (!choice) return;
      if (choice.browse) { showMidiTab(); return; }
      if (choice.studio) { if (Hub.devices) Hub.devices.openStudio(true); return; }
      addDevice(trackId, choice);
    });
  }

  // ---------------------------------------------------------------- device windows
  const windows = new Map();      // "trackId:fxId" -> { win, controls, statusLine }

  function deviceTitle(device) {
    const track = Hub.trackById(device.trackId);
    return `${device.name}${track ? " · " + track.name : ""}`;
  }

  /** What a Sonify device's window shows that can change under it: rebuilt when it does. */
  const sonifyKey = (device) => JSON.stringify([device.device && device.device.hash, device.maps || {}, device.fault || "", device.missing || false,
                                                device.demo || false, device.locked || false]);

  /** A protected device's protection: its product, maker and demo times (none: not protected). */
  const protectionOf = (device) => (device && device.device && device.device.protection) || null;

  /** Seconds as people read them: "10", "2.5". */
  const secondsText = (value) => {
    const n = Number(value) || 0;
    return Number.isInteger(n) ? String(n) : n.toFixed(1);
  };

  // ---------------------------------------------------------------- Velocity Curve's graph
  /** Grex's velocity curve display: the curve over a grid with the straight line to compare,
      "LINEAR", "SOFT nn" or "HARD nn". Drag up for harder, down for softer; double-click: linear.
      values() gives { curve, low, high }; onCurve(c) sets the curve (-100..100). */
  function velocityGraph(values, onCurve) {
    const box = element("div", "velcurve");
    const canvas = element("canvas", "velcurve-canvas");
    canvas.width = 640; canvas.height = 320;
    canvas.title = "Drag up to play harder, down to play softer; double-click for a straight line";
    box.appendChild(canvas);
    const shape = (x, c) => (Math.abs(c) < 1e-3 ? x : Math.pow(x, Math.pow(2.5, -c)));

    const draw = () => {
      const g = canvas.getContext("2d");
      if (!g) return;
      const { curve, low, high } = values();
      const c = Hub.clamp(curve / 100, -1, 1);
      const w = canvas.width, h = canvas.height, pad = 16;
      const iw = w - pad * 2, ih = h - pad * 2;
      g.clearRect(0, 0, w, h);
      g.fillStyle = "#14161a";
      g.beginPath(); g.roundRect(0, 0, w, h, 12); g.fill();
      g.strokeStyle = "rgba(255,255,255,0.07)"; g.lineWidth = 2;
      for (let i = 1; i < 4; i++) {
        g.beginPath(); g.moveTo(pad + iw * i / 4, pad); g.lineTo(pad + iw * i / 4, pad + ih); g.stroke();
        g.beginPath(); g.moveTo(pad, pad + ih * i / 4); g.lineTo(pad + iw, pad + ih * i / 4); g.stroke();
      }
      g.strokeStyle = "rgba(255,255,255,0.18)"; g.setLineDash([8, 8]);
      g.beginPath(); g.moveTo(pad, pad + ih); g.lineTo(pad + iw, pad); g.stroke();
      g.setLineDash([]);
      g.strokeStyle = "#ffb347"; g.lineWidth = 4; g.lineJoin = "round";
      g.beginPath();
      for (let i = 0; i <= 128; i++) {
        const x = i / 128;
        const v = (low + (high - low) * shape(x, c)) / 127;
        const px = pad + iw * x, py = pad + ih * (1 - v);
        if (i === 0) g.moveTo(px, py); else g.lineTo(px, py);
      }
      g.stroke();
      g.fillStyle = "#ffb347"; g.font = "600 24px Segoe UI, system-ui, sans-serif";
      const n = Math.round(Math.abs(curve));
      g.fillText(n === 0 ? "LINEAR" : `${curve < 0 ? "SOFT" : "HARD"} ${n}`, pad + 10, pad + 32);
    };

    let drag = null;
    canvas.addEventListener("pointerdown", (event) => {
      canvas.setPointerCapture(event.pointerId);
      drag = { y: event.clientY, from: values().curve };
    });
    canvas.addEventListener("pointermove", (event) => {
      if (!drag) return;
      const height = Math.max(40, canvas.getBoundingClientRect().height);
      const next = Hub.clamp(Math.round(drag.from + (drag.y - event.clientY) / height * 100), -100, 100);
      if (next !== values().curve) { onCurve(next); draw(); }
    });
    canvas.addEventListener("pointerup", () => { drag = null; });
    canvas.addEventListener("dblclick", () => { onCurve(0); draw(); });
    draw();
    return { box, draw };
  }

  /** A device's name as the lists show it: "(demo)" while it plays its demo. */
  const shownName = (device) => device.demo ? `${device.name} (demo)` : device.name;

  function openDevice(trackId, fxId, rebuild = false) {
    const key = `${trackId}:${fxId}`;
    const device = devicesOf(trackId).find((d) => d.id === fxId);
    const type = device && typeOf(device);
    if (!device || !type || !Hub.screens) return;

    const existing = windows.get(key);
    const outputs = type.sonify ? type.outputs.length : 0;
    const height = 64 + 26 * type.params.length + (type.magenta ? 56 : 0) + (type.sonify ? 46 + (outputs ? 26 + 30 * outputs : 0) : 0)
                 + (device.demo ? 64 : 0) + (type.id === "velocityCurve" ? 176 : 0);
    const win = Hub.screens.panel(`midifx:${key}`, deviceTitle({ ...device, trackId }), 360, height);
    if (existing && existing.win === win && !rebuild) return;

    const body = element("div", "midifx-window");
    const about = element("p", "midifx-about muted small", type.magenta ? (toolById(device.tool) || {}).description || type.description : type.description);
    body.appendChild(about);
    const list = element("div", "params screen-params");
    const controls = [];

    type.params.forEach((param, index) => {
      const value = device.params && device.params[param.id] !== undefined ? device.params[param.id] : param.value;
      const control = paramControl(param, value, (v) => {
        const current = devicesOf(trackId).find((d) => d.id === fxId);
        if (current && current.params) current.params[param.id] = v;
        Hub.send("midiFxParam", { trackId, fxId, index, value: v });
      });
      controls.push({ param, control });
      list.appendChild(control.row);
    });

    // Velocity Curve: Grex's graph over its controls.
    let graph = null;
    if (type.id === "velocityCurve") {
      const current = () => devicesOf(trackId).find((d) => d.id === fxId) || device;
      const valueOf = (id) => {
        const d = current();
        const param = type.params.find((p) => p.id === id);
        return Number(d.params && d.params[id] !== undefined ? d.params[id] : param ? param.value : 0);
      };
      graph = velocityGraph(() => ({ curve: valueOf("curve"), low: valueOf("low"), high: valueOf("high") }), (curve) => {
        const d = current();
        if (d.params) d.params.curve = curve;
        Hub.send("midiFxParam", { trackId, fxId, index: type.params.findIndex((p) => p.id === "curve"), value: curve });
        const row = controls.find((c) => c.param.id === "curve");
        if (row) row.control.set(curve);
      });
      body.appendChild(graph.box);
      // The sliders move the graph too.
      list.addEventListener("input", () => requestAnimationFrame(graph.draw));
    }

    body.appendChild(list);
    let statusLine = null;

    if (type.magenta) {
      statusLine = element("p", "midifx-status small");
      body.appendChild(statusLine);
    }

    if (type.sonify) sonifyParts(body, device, type, trackId, fxId);

    win.body.replaceChildren(body);
    windows.set(key, { win, controls, statusLine, trackId, fxId, graph, sonify: type.sonify ? sonifyKey(device) : "" });
    showStatus(key);
  }

  /** A Sonify device's window: its outputs (each mapped to what it moves), how it's doing, and
      the Plugins Creator Studio. */
  function sonifyParts(body, device, type, trackId, fxId) {
    if (type.outputs.length) {
      const box = element("div", "device-outputs");
      box.appendChild(element("div", "device-outputs-title", "Outputs"));
      for (const output of type.outputs) {
        const target = device.maps ? device.maps[output.id] : null;
        const row = element("div", "device-output");
        row.append(element("span", "device-output-name", output.name),
                   element("span", "device-output-target" + (target ? "" : " muted"), target ? Hub.midiLink.targetText(target) : "Not mapped"));
        const map = element("button", "plain-button small", "Map");
        map.type = "button";
        map.title = `Choose what ${output.name} moves: then click a fader, a knob or a button`;
        map.addEventListener("click", () => mapOutput(trackId, fxId, output));
        row.appendChild(map);
        if (target) {
          const clear = element("button", "icon-button", "×");
          clear.type = "button";
          clear.title = `${output.name} moves nothing`;
          clear.setAttribute("aria-label", clear.title);
          clear.addEventListener("click", () => Hub.send("deviceMap", { trackId, fxId, output: output.id }));
          row.appendChild(clear);
        }
        box.appendChild(row);
      }
      body.appendChild(box);
    }

    const problem = device.missing ? "This device isn't on this computer: it lets the notes through. Its song keeps it for a Sonify that has it."
                  : device.fault || "";
    if (problem) body.appendChild(element("p", "midifx-status small problem", problem));

    // Protected: it plays its demo until it's registered on this computer (with the serial its
    // maker's site gives for this computer's ID), and someone else's isn't opened for editing.
    const protection = protectionOf(device);
    if (protection && device.demo)
      body.appendChild(element("p", "midifx-status small",
        `Demo: notes pass for ${secondsText(protection.passSeconds)} seconds, then are held back for ${secondsText(protection.holdSeconds)} seconds, until it's registered on this computer.`));

    const buttons = element("div", "device-window-buttons");
    if (protection && device.demo) {
      const register = element("button", "plain-button small primary", "Register…");
      register.type = "button";
      register.title = `Register it with the serial you got from ${protection.vendor || "its maker"}: the window shows your computer ID to give them`;
      register.addEventListener("click", () => Hub.send("deviceRegister", { trackId, fxId }));
      buttons.appendChild(register);
    }
    if (!device.missing && !device.locked) {
      const studio = element("button", "plain-button small", "Open in Plugins Creator Studio");
      studio.type = "button";
      studio.title = "Change it by asking Fanan AI";
      studio.addEventListener("click", () => { if (Hub.devices) Hub.devices.openSlot(trackId, fxId); });
      buttons.appendChild(studio);
    }
    const newer = Hub.devices ? Hub.devices.newerFor(device) : null;
    if (newer) {
      const update = element("button", "plain-button small primary", `Update to version ${newer.version}`);
      update.type = "button";
      update.title = "My Devices has a newer version of it: this place takes it, keeping its settings";
      update.addEventListener("click", () => Hub.send("deviceStudio", { action: "update", trackId, fxId }));
      buttons.appendChild(update);
    }
    if (buttons.childElementCount) body.appendChild(buttons);
  }

  /** An output's Map: the next control clicked is what it moves. */
  function mapOutput(trackId, fxId, output) {
    if (!Hub.midiLink || !Hub.midiLink.pick) return;
    Hub.midiLink.pick((target) => Hub.send("deviceMap", { trackId, fxId, output: output.id, target }), output.name);
  }

  function showStatus(key) {
    const view = windows.get(key);
    if (!view || !view.statusLine) return;
    const device = devicesOf(view.trackId).find((d) => d.id === view.fxId);
    const tool = device && installed.get(device.tool);
    let text = status.get(key) || "Waiting for the song (or a note) to start a window.";
    if (!magentaBuilt) text = "This Sonify was built without magenta.js, so Magenta tools can't run.";
    else if (device && !tool) text = "Its Magenta tool isn't installed: the Browser's MIDI tab installs it.";
    view.statusLine.textContent = text;
  }

  function refreshWindows() {
    for (const [key, view] of [...windows]) {
      const device = devicesOf(view.trackId).find((d) => d.id === view.fxId);
      if (!device || !Hub.screens.windows.has(`midifx:${key}`)) {
        windows.delete(key);
        if (!device) Hub.screens.close(`midifx:${key}`);
        continue;
      }
      // A Sonify device rebuilt, mapped or stopped: its window made again.
      if (view.sonify && view.sonify !== sonifyKey(device)) {
        openDevice(view.trackId, view.fxId, true);
        continue;
      }
      for (const { param, control } of view.controls)
        if (device.params && device.params[param.id] !== undefined) control.set(device.params[param.id]);
      if (view.graph) view.graph.draw();
      showStatus(key);
    }
  }

  // ---------------------------------------------------------------- the Console strips
  const POWER = '<svg viewBox="0 0 16 16"><path d="M7.25 1.5h1.5v6h-1.5z"/><path d="M4.2 3.6l1.06 1.06A4.25 4.25 0 1 0 10.74 4.66L11.8 3.6A5.75 5.75 0 1 1 4.2 3.6z"/></svg>';
  const REMOVE = '<svg viewBox="0 0 16 16"><path d="M4.5 4.5l7 7M11.5 4.5l-7 7" stroke="currentColor" stroke-width="1.6"/></svg>';
  const EDIT = '<svg viewBox="0 0 16 16"><rect x="2" y="3" width="12" height="10" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.3"/><path d="M2 5.8h12" stroke="currentColor" stroke-width="1.3"/></svg>';

  function setPower(trackId, fxId, button) {
    const on = button.getAttribute("aria-pressed") !== "true";
    button.setAttribute("aria-pressed", String(on));
    const item = button.closest("[data-fx-id]");
    if (item) item.classList.toggle("bypassed", !on);
    const device = devicesOf(trackId).find((d) => d.id === fxId);
    if (device) device.on = on;
    Hub.send("midiFxOn", { trackId, fxId, on });
  }

  const stripMarkup = () => `<div class="strip-midifx" hidden>
      <div class="strip-midifx-list"></div>
      <button class="strip-midifx-add" type="button" title="Add a MIDI FX: in front of the instrument, it hears the notes first" aria-label="Add a MIDI FX">+ MIDI FX</button>
    </div>`;

  function showStrip(area, track) {
    if (!area) return;
    area.hidden = !isInstrument(track);
    if (area.hidden) return;
    const devices = track.midiFx || [];
    const key = devices.map((d) => `${d.id}:${d.on ? 1 : 0}:${d.name}:${d.demo ? 1 : 0}`).join(",");
    if (area.dataset.key === key) return;
    area.dataset.key = key;

    const list = area.querySelector(".strip-midifx-list");
    list.textContent = "";
    for (const device of devices) {
      const item = element("div", "strip-midifx-item" + (device.on ? "" : " bypassed"));
      item.dataset.fxId = String(device.id);
      const power = element("button", "mini-power");
      power.type = "button";
      power.innerHTML = POWER;
      power.setAttribute("aria-pressed", String(Boolean(device.on)));
      power.setAttribute("aria-label", `${device.on ? "Switch off" : "Switch on"} ${device.name}`);
      const name = element("button", "strip-insert-name", shownName(device));
      name.type = "button";
      name.title = `${device.name}: click to open it`;
      item.append(power, name);
      list.appendChild(item);
    }
    area.querySelector(".strip-midifx-add").hidden = devices.length >= maxDevices;
  }

  function setupStrip(area, trackId) {
    if (!area) return;
    area.querySelector(".strip-midifx-add").addEventListener("click", (event) => {
      event.stopPropagation();
      addMenu(event.currentTarget, trackId);
    });
    area.addEventListener("click", (event) => {
      const item = event.target.closest(".strip-midifx-item");
      if (!item) return;
      event.stopPropagation();
      const fxId = Number(item.dataset.fxId);
      if (event.target.closest(".mini-power")) setPower(trackId, fxId, item.querySelector(".mini-power"));
      else openDevice(trackId, fxId);
    });

    // A MIDI tool dragged from the Browser onto the strip.
    const strip = area.closest(".strip") || area;
    strip.addEventListener("dragover", (event) => {
      if (!Hub.dragMidiTool || !isInstrument(Hub.trackById(trackId))) return;
      event.preventDefault();
      if (event.dataTransfer) event.dataTransfer.dropEffect = "copy";
      strip.classList.add("drop-target");
    });
    strip.addEventListener("drop", (event) => {
      if (!Hub.dragMidiTool) return;
      event.preventDefault();
      event.stopPropagation();
      strip.classList.remove("drop-target");
      addDevice(trackId, Hub.dragMidiTool);
      Hub.dragMidiTool = null;
    });
  }

  const panelMarkup = () => `<div class="panel-midifx">
      <div class="panel-head"><span>MIDI FX</span><button class="panel-add-midifx" type="button" title="Add a MIDI FX" aria-label="Add a MIDI FX">+</button></div>
      <div class="panel-list panel-midifx-list"></div>
    </div>`;

  function showPanel(section, track) {
    if (!section) return;
    section.hidden = !isInstrument(track);
    if (section.hidden) return;
    const list = section.querySelector(".panel-midifx-list");
    const devices = track.midiFx || [];
    const key = devices.map((d) => `${d.id}:${d.on ? 1 : 0}:${d.name}:${d.demo ? 1 : 0}`).join(",");
    if (list.dataset.key === key) return;
    list.dataset.key = key;
    list.textContent = "";

    for (const device of devices) {
      const row = element("div", "panel-insert panel-midifx-row" + (device.on ? "" : " bypassed"));
      row.dataset.fxId = String(device.id);
      row.draggable = true;
      const power = element("button", "panel-power");
      power.type = "button";
      power.innerHTML = POWER;
      power.setAttribute("aria-pressed", String(Boolean(device.on)));
      power.setAttribute("aria-label", `${device.on ? "Switch off" : "Switch on"} ${device.name}`);
      const name = element("button", "panel-insert-name", shownName(device));
      name.type = "button";
      name.title = `${device.name}: click to open it; drag to reorder`;
      const edit = element("button", "panel-ui");
      edit.type = "button";
      edit.innerHTML = EDIT;
      edit.title = `Open ${device.name}`;
      edit.setAttribute("aria-label", edit.title);
      const remove = element("button", "panel-insert-remove");
      remove.type = "button";
      remove.innerHTML = REMOVE;
      remove.title = `Remove ${device.name}`;
      remove.setAttribute("aria-label", remove.title);
      row.append(power, name, edit, remove);
      list.appendChild(row);
    }

    if (!devices.length) list.appendChild(element("p", "panel-note", "Notes go straight to the instrument"));
  }

  function setupPanel(section, trackId) {
    if (!section) return;
    section.querySelector(".panel-add-midifx").addEventListener("click", (event) => {
      event.stopPropagation();
      addMenu(event.currentTarget, trackId);
    });

    const list = section.querySelector(".panel-midifx-list");
    list.addEventListener("click", (event) => {
      const row = event.target.closest(".panel-midifx-row");
      if (!row) return;
      event.stopPropagation();
      const fxId = Number(row.dataset.fxId);
      if (event.target.closest(".panel-power")) setPower(trackId, fxId, row.querySelector(".panel-power"));
      else if (event.target.closest(".panel-insert-remove")) Hub.send("midiFxRemove", { trackId, fxId });
      else openDevice(trackId, fxId);
    });

    // Reordering by drag, a line showing where it lands.
    let dragging = null, line = null;
    const rows = () => [...list.querySelectorAll(".panel-midifx-row")];
    const indexAt = (y) => { const all = rows(); const i = all.findIndex((r) => { const b = r.getBoundingClientRect(); return y < b.top + b.height / 2; }); return i < 0 ? all.length : i; };
    const clearLine = () => { if (line) line.remove(); line = null; };

    list.addEventListener("dragstart", (event) => {
      const row = event.target.closest(".panel-midifx-row");
      if (!row) return;
      dragging = { fxId: Number(row.dataset.fxId), from: rows().indexOf(row) };
      if (event.dataTransfer) { event.dataTransfer.effectAllowed = "move"; event.dataTransfer.setData("text/plain", row.dataset.fxId); }
    });
    list.addEventListener("dragover", (event) => {
      if (!dragging) return;
      event.preventDefault();
      event.stopPropagation();
      clearLine();
      line = element("div", "drop-line");
      const all = rows(), index = indexAt(event.clientY);
      if (index < all.length) list.insertBefore(line, all[index]); else if (all.length) all[all.length - 1].after(line);
    });
    list.addEventListener("drop", (event) => {
      if (!dragging) return;
      event.preventDefault();
      event.stopPropagation();
      const index = indexAt(event.clientY);
      const position = index > dragging.from ? index - 1 : index;
      if (position !== dragging.from) Hub.send("midiFxMove", { trackId, fxId: dragging.fxId, position });
      clearLine();
    });
    list.addEventListener("dragend", () => { dragging = null; clearLine(); });
  }

  // ---------------------------------------------------------------- Magenta: the models
  const STEPS = 4;                // steps a quarter note: every model here counts sixteenths
  let magentaLoading = null;
  const models = new Map();       // tool id -> { stamp, ready: Promise of its model }
  const stampOf = (tool) => `${tool.version || ""}|${tool.installed || ""}`;

  function loadMagenta() {
    if (window.mm) return Promise.resolve(window.mm);
    if (!magentaBuilt) return Promise.reject(new Error("This Sonify was built without magenta.js, so Magenta tools can't run."));
    if (!magentaLoading) {
      magentaLoading = new Promise((resolve, reject) => {
        const script = document.createElement("script");
        script.src = "magenta/magentamusic.js";
        script.onload = () => {
          if (!window.mm) { reject(new Error("magenta.js didn't load.")); return; }
          try { window.mm.logging.setVerbosity(window.mm.logging.Level.WARN); } catch (e) { /* older builds */ }
          resolve(window.mm);
        };
        script.onerror = () => { magentaLoading = null; reject(new Error("magenta.js isn't in this Sonify.")); };
        document.head.appendChild(script);
      });
    }
    return magentaLoading;
  }

  /** A tool's model, loading (or loaded): { stamp, users, gone, ready }. */
  async function entryFor(tool) {
    if (!installed.has(tool.id)) throw new Error(`${tool.title || tool.id} isn't installed (the Browser's MIDI tab).`);
    const mm = await loadMagenta();
    const current = installed.get(tool.id);
    const kept = models.get(tool.id);
    if (kept && kept.stamp !== stampOf(current)) forgetModel(tool.id);
    if (!models.has(tool.id)) {
      const url = `midi-tools/${tool.id}/model`;
      const model = tool.model === "MusicRNN" ? new mm.MusicRNN(url) : tool.model === "Coconet" ? new mm.Coconet(url) : new mm.MusicVAE(url);
      const entry = { stamp: stampOf(current), users: 0, gone: false, ready: model.initialize().then(() => model) };
      entry.ready.catch(() => { if (models.get(tool.id) === entry) models.delete(tool.id); });
      models.set(tool.id, entry);
    }
    return models.get(tool.id);
  }

  /** A model no longer wanted (its tool removed or replaced): its memory is given back, once
      whatever is using it has finished. */
  function forgetModel(id) {
    const entry = models.get(id);
    if (!entry) return;
    models.delete(id);
    entry.gone = true;
    if (!entry.users) disposeModel(entry);
  }

  function disposeModel(entry) {
    entry.ready.then((model) => { try { model.dispose(); } catch (e) { /* already gone */ } }, () => {});
  }

  /** The pitch range a melody model plays (all of MIDI for others). */
  function rangeOf(model) {
    const c = model && model.dataConverter;
    return c && Number.isFinite(c.minPitch) && Number.isFinite(c.maxPitch) ? [c.minPitch, c.maxPitch] : [0, 127];
  }

  /** Octaves that move a phrase into a range: the shift, and each note folded in. */
  function fitInto(notes, low, high) {
    if (!notes.length || (low === 0 && high === 127)) return { shift: 0, notes };
    const pitches = notes.map((n) => n.pitch);
    const middle = (Math.min(...pitches) + Math.max(...pitches)) / 2;
    const shift = 12 * Math.round(((low + high) / 2 - middle) / 12);
    const fold = (p) => { while (p < low) p += 12; while (p > high) p -= 12; return p; };
    const folded = notes.map((n) => ({ ...n, pitch: fold(n.pitch + shift) })).filter((n) => n.pitch >= low && n.pitch <= high);
    return { shift, notes: folded };
  }

  /** Notes (start and length in quarter notes from 0) as a quantized sequence of so many steps. */
  function sequenceOf(notes, steps, drums = false, voice = 0) {
    const sequence = { notes: [], quantizationInfo: { stepsPerQuarter: STEPS }, totalQuantizedSteps: steps, tempos: [{ time: 0, qpm: 120 }] };
    for (const n of notes) {
      const start = Math.round(n.start * STEPS);
      if (start < 0 || start >= steps) continue;
      const end = Math.min(steps, Math.max(start + 1, Math.round((n.start + n.length) * STEPS)));
      sequence.notes.push({ pitch: n.pitch, quantizedStartStep: start, quantizedEndStep: end, velocity: n.velocity || 100, isDrum: drums, instrument: voice });
    }
    sequence.notes.sort((a, b) => a.quantizedStartStep - b.quantizedStartStep || b.pitch - a.pitch);
    return sequence;
  }

  /** A sequence's notes in quarter notes from its start (quantized, or timed in seconds at qpm). */
  function notesOf(sequence, qpm = 120, velocity = 96) {
    const quantized = Boolean(sequence.quantizationInfo && sequence.quantizationInfo.stepsPerQuarter);
    const perStep = quantized ? 1 / sequence.quantizationInfo.stepsPerQuarter : 0;
    return (sequence.notes || []).map((n) => {
      const start = quantized ? (n.quantizedStartStep || 0) * perStep : (n.startTime || 0) * qpm / 60;
      const end = quantized ? (n.quantizedEndStep || 0) * perStep : (n.endTime || 0) * qpm / 60;
      return { pitch: n.pitch, start, length: Math.max(1 / 16, end - start), velocity: n.velocity > 0 ? Math.min(127, n.velocity) : velocity, voice: n.instrument || 0 };
    });
  }

  /** Coconet gives each voice one step at a time: a held note is one note again. */
  function mergeHeld(notes) {
    const out = [];
    const sorted = [...notes].sort((a, b) => a.voice - b.voice || a.pitch - b.pitch || a.start - b.start);
    for (const n of sorted) {
      const last = out[out.length - 1];
      if (last && last.voice === n.voice && last.pitch === n.pitch && Math.abs(last.start + last.length - n.start) < 1e-6) last.length += n.length;
      else out.push({ ...n });
    }
    return out.sort((a, b) => a.start - b.start || a.pitch - b.pitch);
  }

  // ---------------------------------------------------------------- chords
  const CHORDS = [["", [0, 4, 7]], ["m", [0, 3, 7]], ["7", [0, 4, 7, 10]], ["maj7", [0, 4, 7, 11]], ["m7", [0, 3, 7, 10]],
                  ["dim", [0, 3, 6]], ["aug", [0, 4, 8]], ["sus4", [0, 5, 7]], ["sus2", [0, 2, 7]], ["m7b5", [0, 3, 6, 10]]];
  const CHORD_ROOTS = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];

  /** The chord notes sound in a stretch [from, to): its name ("Am7"), or "N.C." for none. */
  function chordIn(notes, from, to) {
    const weight = new Array(12).fill(0);
    let bass = -1, lowest = 999;
    for (const n of notes) {
      const overlap = Math.min(to, n.start + n.length) - Math.max(from, n.start);
      if (overlap <= 0) continue;
      weight[((n.pitch % 12) + 12) % 12] += overlap;
      if (n.pitch < lowest) { lowest = n.pitch; bass = n.pitch % 12; }
    }
    if (weight.every((w) => w === 0)) return "N.C.";
    let best = null;
    for (let root = 0; root < 12; root++) {
      for (const [suffix, intervals] of CHORDS) {
        const pcs = new Set(intervals.map((i) => (root + i) % 12));
        let score = 0;
        for (let pc = 0; pc < 12; pc++) score += pcs.has(pc) ? weight[pc] : -0.6 * weight[pc];
        score -= 0.05 * intervals.length;                 // simpler chords win ties
        if (root === bass) score += 0.1;
        if (!best || score > best.score) best = { score, name: CHORD_ROOTS[root] + suffix };
      }
    }
    return best.name;
  }

  /** One chord a beat over [from, from + length). */
  function chordsPerBeat(notes, from, length) {
    const beats = Math.max(1, Math.round(length));
    const list = [];
    for (let b = 0; b < beats; b++) list.push(chordIn(notes, from + b, from + b + 1));
    // A beat with nothing new keeps the chord before it.
    for (let b = 1; b < list.length; b++) if (list[b] === "N.C.") list[b] = list[b - 1];
    return list;
  }

  // ---------------------------------------------------------------- Magenta: what each tool does
  /**
      Runs a tool's action on notes (start, length in quarter notes from the stretch's start) over
      a stretch `length` quarter notes long. Returns notes in the same terms; for continue, from
      the end of the stretch on (`bars` more). options: temperature, similarity, bars, steps,
      qpm, other (morph's second clip's notes).
  */
  async function runMagenta(tool, action, notes, length, options = {}) {
    const entry = await entryFor(tool);
    entry.users += 1;
    try {
      return await runModel(await entry.ready, tool, action, notes, length, options);
    } finally {
      entry.users -= 1;
      if (entry.gone && !entry.users) disposeModel(entry);
    }
  }

  async function runModel(model, tool, action, notes, length, options) {
    const temperature = Number(options.temperature) || 1;
    const qpm = Number(options.qpm) || 120;
    const drums = Boolean(tool.drums);
    const velocity = notes.length ? Math.round(notes.reduce((s, n) => s + (n.velocity || 96), 0) / notes.length) : 96;

    if (action === "continue" || action === "improvise") {
      const [low, high] = rangeOf(model);
      const fitted = drums ? { shift: 0, notes } : fitInto(notes, low, high);
      const seedSteps = Math.max(STEPS, Math.round(length * STEPS));
      const steps = Math.max(STEPS, Math.round((action === "continue" ? options.more : length) * STEPS));
      let made;

      if (action === "improvise") {
        // Over the chords of the stretch, one a beat: a short seed (the first chord's root) and on.
        const chords = chordsPerBeat(notes, 0, length);
        const first = chords.find((c) => c !== "N.C.") || "C";
        const rootPc = CHORD_ROOTS.indexOf(first.match(/^[A-G][#b]?/)[0]);
        let seedPitch = rootPc + 12 * Math.floor(((low + high) / 2) / 12);
        while (seedPitch < low) seedPitch += 12;
        while (seedPitch > high) seedPitch -= 12;
        const seed = sequenceOf([{ pitch: seedPitch, start: 0, length: 1, velocity }], STEPS, false);
        const rest = Math.max(STEPS, Math.round(length * STEPS) - STEPS);
        const result = await model.continueSequence(seed, rest, temperature, chords.length ? chords : ["N.C."]);
        made = [{ pitch: seedPitch, start: 0, length: 1, velocity }, ...notesOf(result, qpm, velocity).map((n) => ({ ...n, start: n.start + 1 }))];
        return made.filter((n) => n.start < length).map((n) => ({ ...n, pitch: n.pitch, velocity }));
      }

      const seed = sequenceOf(fitted.notes, seedSteps, drums);
      const result = await model.continueSequence(seed, steps, temperature);
      made = notesOf(result, qpm, velocity).map((n) => ({ ...n, pitch: n.pitch - fitted.shift, velocity: drums ? n.velocity : velocity }));
      return made;
    }

    // The two-bar (or so) models work a chunk at a time.
    const chunkSteps = (model.dataConverter && model.dataConverter.numSteps) || 32;
    const chunk = chunkSteps / STEPS;
    const chunks = Math.max(1, Math.ceil(length / chunk - 1e-6));
    const inChunk = (c) => notes.filter((n) => n.start >= c * chunk && n.start < (c + 1) * chunk).map((n) => ({ ...n, start: n.start - c * chunk }));
    const out = [];
    // A groove can push a hit a hair before its chunk: the first chunk's start stops it there.
    const place = (list, c) => { for (const n of list) if (n.start < chunk && c * chunk + n.start < length) out.push({ ...n, start: Math.max(0, n.start + c * chunk) }); };

    if (action === "generate") {
      const samples = await model.sample(chunks, temperature, undefined, STEPS, qpm);
      samples.forEach((s, c) => place(notesOf(s, qpm, velocity), c));
      return out;
    }

    if (action === "morph") {
      const [low, high] = rangeOf(model);
      const a = drums ? { notes: inChunk(0) } : fitInto(inChunk(0), low, high);
      const b = drums ? { notes: options.other || [] } : fitInto(options.other || [], low, high);
      const count = Math.max(3, Math.min(8, Math.round(options.steps || 5)));
      const result = await model.interpolate([sequenceOf(a.notes, chunkSteps, drums), sequenceOf(b.notes, chunkSteps, drums)], count, temperature);
      const shift = drums ? 0 : Math.round(((a.shift || 0) + (b.shift || 0)) / 2);
      result.forEach((s, c) => { for (const n of notesOf(s, qpm, velocity)) out.push({ ...n, pitch: n.pitch - shift, start: n.start + c * chunk }); });
      return out;
    }

    for (let c = 0; c < chunks; c++) {
      const part = inChunk(c);
      if (!part.length && action !== "drumify") continue;

      if (action === "vary") {
        const [low, high] = rangeOf(model);
        const fitted = drums ? { shift: 0, notes: part } : fitInto(part, low, high);
        if (!fitted.notes.length) continue;
        const similarity = Hub.clamp01((options.similarity === undefined ? 75 : options.similarity) / 100);
        const [result] = await model.similar(sequenceOf(fitted.notes, chunkSteps, drums), 1, similarity, temperature);
        place(notesOf(result, qpm, velocity).map((n) => ({ ...n, pitch: n.pitch - fitted.shift })), c);
      } else if (action === "groove" || action === "drumify") {
        if (!part.length) continue;
        let input = sequenceOf(part, chunkSteps, true);
        if (action === "drumify") input = await model.dataConverter.toNoteSequence(model.dataConverter.toTensor(input), undefined, qpm);
        const z = await model.encode([input]);
        const [result] = await model.decode(z, temperature, undefined, undefined, qpm);
        z.dispose();
        place(notesOf(result, qpm, velocity), c);
      } else if (action === "harmonize") {
        // The melody on top (soprano), three voices under it.
        const fitted = fitInto(part, 60, 81);
        if (!fitted.notes.length) continue;
        const result = await model.infill(sequenceOf(fitted.notes, chunkSteps, false, 0),
                                          { temperature: Math.min(1.5, Math.max(0.1, temperature * 0.99)), numIterations: options.iterations || 64 });
        const voices = mergeHeld(notesOf(result, qpm, velocity)).filter((n) => n.voice > 0);
        place(voices.map((n) => ({ ...n, pitch: n.pitch - fitted.shift })), c);
      }
    }

    return out;
  }

  // ---------------------------------------------------------------- Magenta: live
  // The Magenta device hears a window and the engine sends it here; the answer goes back for the
  // next window (Answer, Follow) or to play round (Loop). One device works on one window at a time;
  // when its model is slower than the windows, the newest one waiting is the one answered next.
  const busy = new Set();          // "trackId:fxId" of the devices whose model is running
  const queued = new Map();        // "trackId:fxId" -> the newest window waiting

  async function answerWindow(request) {
    const key = `${request.trackId}:${request.fxId}`;
    const tool = installed.get(request.tool);
    if (!tool) { status.set(key, "Its Magenta tool isn't installed."); showStatus(key); return; }

    status.set(key, `Thinking about window ${request.window}…`);
    showStatus(key);
    const action = tool.live || (tool.actions || [])[0];
    const length = Number(request.length) || 4;
    const heard = (request.notes || []).map(([pitch, start, len, velocity]) => ({ pitch, start, length: len, velocity }));
    let notes;

    if (action === "continue") notes = await runMagenta(tool, "continue", heard, length, { more: length, temperature: request.temperature, qpm: request.bpm });
    else if (action === "generate") notes = await runMagenta(tool, heard.length ? "vary" : "generate", heard, length, { temperature: request.temperature, qpm: request.bpm, similarity: 70 });
    else notes = await runMagenta(tool, action, heard, length, { temperature: request.temperature, qpm: request.bpm });

    if (request.inKey && !tool.drums) {
      const key = request.key || { root: 0, notes: SCALE_NOTES.major };
      notes = notes.map((n) => ({ ...n, pitch: snapToKey(n.pitch, key) }));
    }

    const round = (x) => Math.round(x * 1e6) / 1e6;
    const answer = notes.filter((n) => n.start >= 0 && n.start < length)
                        .map((n) => [Hub.clamp(Math.round(n.pitch), 0, 127), round(n.start), round(Math.min(n.length, length - n.start)), Math.round(n.velocity || 96)]);
    Hub.send("magentaAnswer", { trackId: request.trackId, fxId: request.fxId, window: request.window + 1, length, notes: answer });
    status.set(key, answer.length ? `Answered window ${request.window} (${answer.length} notes).` : `Nothing to play for window ${request.window}.`);
    showStatus(key);
  }

  async function pump(key) {
    busy.add(key);
    while (queued.has(key)) {
      const request = queued.get(key);
      queued.delete(key);
      try {
        await answerWindow(request);
      } catch (error) {
        status.set(key, `Couldn't answer: ${error && error.message ? error.message : error}`);
        showStatus(key);
      }
    }
    busy.delete(key);
  }

  Hub.on("magentaRequest", (request) => {
    const key = `${request.trackId}:${request.fxId}`;
    queued.set(key, request);
    if (!busy.has(key)) pump(key);
  });

  // ---------------------------------------------------------------- Apply: what it works on
  /** The notes a tool works on: in the Editor (the selected notes, or the clip's), or the selected
      MIDI clips in the arrangement. */
  function currentTarget() {
    const editor = Hub.editor;
    if (editor && editor.isOpen() && (Hub.state.focus === "editor" || !midiClipsSelected().length)) {
      const ed = editor.state;
      const track = Hub.trackById(ed.trackId);
      const taken = ed.notes.some((n) => n.selected) ? ed.notes.filter((n) => n.selected) : ed.notes.slice();
      return { kind: "editor", track, trackId: ed.trackId, clipId: ed.clipId, taken, from: ed.clipStart, to: ed.clipStart + ed.clipLength,
               text: `${taken.length === ed.notes.length ? "All" : taken.length} ${taken.length === 1 ? "note" : "notes"} of the clip in the Editor${track ? " (" + track.name + ")" : ""}` };
    }
    const clips = midiClipsSelected();
    if (clips.length)
      return { kind: "clips", clips, text: `${clips.length} selected ${clips.length === 1 ? "clip" : "clips"}` };
    return null;
  }

  function midiClipsSelected() {
    const out = [];
    for (const id of Hub.state.selectedClips || []) {
      const found = Hub.findClip(id);
      if (found && found.track.kind === "instrument" && !found.clip.audioFile) out.push(found);
    }
    return out.sort((a, b) => a.clip.start - b.clip.start);
  }

  /** A clip's notes it shows, at song positions (a muted one's mute and their operators kept
      with them); and the ones a trim hides, as they are. */
  function clipNotes(clip) {
    const shown = [], hidden = [];
    const offset = clip.offset || 0;
    for (const note of clip.notes || []) {
      const [pitch, start, length, velocity, muted, ops] = note;
      if (start >= offset && start < offset + clip.length)
        shown.push({ pitch, start: clip.start + start - offset, length, velocity, muted: Boolean(muted), ops: ops && typeof ops === "object" ? ops : null });
      else hidden.push(note.slice());
    }
    return { shown, hidden };
  }

  /** A note as a clip keeps it: [pitch, content start, length, velocity], then its mute and its
      operators when it has them. */
  function noteArray(n, start) {
    const values = [n.pitch, start, n.length, Hub.clamp(Math.round(n.velocity || 100), 1, 127)];
    if (n.muted || n.ops) values.push(n.muted ? 1 : 0);
    if (n.ops) values.push(n.ops);
    return values;
  }

  /** The bar line at or after a song position. */
  function barLineAtOrAfter(t) {
    const bar = Hub.tempoMap.barAt(Math.max(0, t - 1e-9));
    return t - bar.start < 1e-9 ? bar.start : bar.end;
  }

  // ---------------------------------------------------------------- Apply: running a tool
  let requestId = 0;
  const waiting = new Map();      // requestId -> resolve

  /** The built-in devices on notes (song positions), through the engine. */
  function runBuiltin(type, params, notes, from, to) {
    return new Promise((resolve, reject) => {
      const id = ++requestId;
      waiting.set(id, { resolve, reject });
      Hub.send("midiToolRun", { requestId: id, devices: [{ type, params }], from, to,
                                notes: notes.map((n) => [n.pitch, n.start, n.length, Math.round(n.velocity || 100)]) });
      setTimeout(() => { if (waiting.has(id)) { waiting.delete(id); reject(new Error("Sonify didn't answer.")); } }, 20000);
    });
  }

  Hub.on("midiToolResult", (reply) => {
    const pending = waiting.get(reply.requestId);
    if (!pending) return;
    waiting.delete(reply.requestId);
    if (reply.error) pending.reject(new Error(reply.error));
    else pending.resolve((reply.notes || []).map(([pitch, start, length, velocity]) => ({ pitch, start, length, velocity })));
  });

  /** Each note a built-in tool made keeps its operators (Chance, Repeats...): one for one when the
      tool gives back as many notes as it was given (both by start, then pitch), otherwise those of
      the note given at the same pitch that started at the same moment. */
  function keepOps(made, given) {
    const oneForOne = made.length === given.length;
    return made.map((n, i) => {
      const source = oneForOne ? given[i] : given.find((g) => g.pitch === n.pitch && Math.abs(g.start - n.start) < 1e-6);
      return source && source.ops ? { ...n, ops: source.ops } : n;
    });
  }

  /**
      What a choice of tool makes of some notes (song positions) over [from, to): { notes, keep,
      newTrack, label }. keep: the notes given stay (the tool adds to them).
  */
  async function applyTo(choice, notes, from, to, extra = {}) {
    if (choice.type) {
      const type = typeById(choice.type);
      const given = notes.slice().sort((a, b) => a.start - b.start || a.pitch - b.pitch);   // as the engine gives them back
      const made = await runBuiltin(choice.type, choice.params, given, from, to);
      return { notes: keepOps(made, given), keep: false, label: type ? type.name : "MIDI Tool" };
    }

    const tool = installed.get(choice.tool);
    if (!tool) throw new Error("Install that tool first (the Browser's MIDI tab).");
    const action = choice.action || (tool.actions || [])[0];
    const label = `${tool.title || tool.id}${(tool.actions || []).length > 1 ? ": " + (ACTIONS[action] || action) : ""}`;
    const qpm = Hub.tempoMap.bpmAt(from);
    const relative = notes.map((n) => ({ ...n, start: n.start - from }));
    const options = { temperature: choice.temperature, similarity: choice.similarity, steps: choice.steps, qpm };

    if (action === "continue") {
      // More bars after the last of the notes (from the bar line after them); what leads up to
      // that line (four bars or so) is what the model carries on.
      const end = notes.length ? barLineAtOrAfter(Math.max(...notes.map((n) => n.start + n.length))) : to;
      const bars = Math.max(1, Number(choice.bars) || 2);
      let at = end, more = 0;
      for (let b = 0; b < bars; b++) { const bar = Hub.tempoMap.barAt(at + 1e-9); more += bar.end - at; at = bar.end; }
      const seedFrom = Math.max(from, end - 16);
      const seed = notes.filter((n) => n.start >= seedFrom).map((n) => ({ ...n, start: n.start - seedFrom }));
      const made = await runMagenta(tool, "continue", seed, end - seedFrom, { ...options, more });
      const placed = made.filter((n) => n.start < more).map((n) => ({ ...n, start: n.start + end }));
      return { notes: intoKeyAbsolute(placed, tool, choice.inKey), keep: true, label, extendTo: at };
    }

    if (action === "morph") {
      if (!extra.other) throw new Error("Morph goes from one clip to another: select two MIDI clips in the arrangement.");
      const made = await runMagenta(tool, "morph", relative, to - from, { ...options, other: extra.other });
      return { notes: intoKeyAbsolute(made.map((n) => ({ ...n, start: n.start + from })), tool, choice.inKey), keep: true, newTrack: true, label };
    }

    const made = await runMagenta(tool, action, relative, to - from, options);
    const absolute = intoKeyAbsolute(made.map((n) => ({ ...n, start: n.start + from })), tool, choice.inKey);
    const adds = action === "improvise" || action === "harmonize";
    return { notes: absolute, keep: adds, newTrack: adds && choice.newTrack !== false, label };
  }

  function intoKeyAbsolute(notes, tool, on) {
    if (on === false || tool.drums) return notes;
    return notes.map((n) => ({ ...n, pitch: Hub.clamp(snapToKey(Math.round(n.pitch), keyAt(n.start)), 0, 127) }));
  }

  // ---------------------------------------------------------------- Apply: the dialog
  const dialog = { open: false, choice: null, values: {}, busy: false };

  /** The choices: built-in devices and installed Magenta tools (the rest listed, to install). */
  function fillToolList() {
    const select = $("midi-tools-tool");
    const keep = select.value;
    select.textContent = "";
    const builtin = element("optgroup");
    builtin.label = "Built in";
    for (const type of builtinTypes()) builtin.appendChild(new Option(type.name, `fx:${type.id}`));
    select.appendChild(builtin);
    const magenta = element("optgroup");
    magenta.label = "Magenta";
    for (const tool of catalogEntries()) {
      const option = new Option(tool.title + (installed.has(tool.id) ? "" : " (not installed)"), `mg:${tool.id}`);
      option.disabled = !installed.has(tool.id);
      magenta.appendChild(option);
    }
    select.appendChild(magenta);
    if (keep && [...select.options].some((o) => o.value === keep && !o.disabled)) select.value = keep;
  }

  /** Settings for a Magenta tool's Apply, as parameters (so they look like a device's). */
  function magentaParams(tool, action) {
    const list = [];
    const actions = tool.actions || [];
    if (actions.length > 1) list.push({ id: "action", name: "Do", unit: "choice", choices: actions.map((a) => ACTIONS[a] || a), min: 0, max: actions.length - 1, value: 0 });
    if (action === "continue") list.push({ id: "bars", name: "Bars", unit: "choice", choices: ["1", "2", "4", "8"], min: 0, max: 3, value: 1 });
    list.push({ id: "temperature", name: "Surprise", unit: "decimal", min: 0.1, max: 2, value: 1 });
    if (action === "vary") list.push({ id: "similarity", name: "Likeness", unit: "percent", min: 0, max: 100, value: 75 });
    if (action === "morph") list.push({ id: "steps", name: "Steps", unit: "number", min: 3, max: 8, value: 5 });
    if (!tool.drums) list.push({ id: "inKey", name: "In key", unit: "toggle", min: 0, max: 1, value: 1 });
    if (action === "improvise" || action === "harmonize") list.push({ id: "newTrack", name: "On a new track", unit: "toggle", min: 0, max: 1, value: 1 });
    return list;
  }

  function showDialogParams() {
    const select = $("midi-tools-tool");
    const value = select.value;
    const box = $("midi-tools-params");
    box.textContent = "";
    if (!value) return;
    const kind = value.slice(0, 3), id = value.slice(3);
    const values = dialog.values[value] = dialog.values[value] || {};

    if (kind === "fx:") {
      const type = typeById(id);
      $("midi-tools-about").textContent = type ? type.description : "";
      for (const param of (type ? type.params : [])) {
        if (values[param.id] === undefined) values[param.id] = param.value;
        box.appendChild(paramControl(param, values[param.id], (v) => { values[param.id] = v; }).row);
      }
      return;
    }

    const tool = installed.get(id);
    $("midi-tools-about").textContent = tool ? tool.description || "" : "";
    if (!tool) return;
    const actions = tool.actions || [];
    const action = actions[Math.round(values.action || 0)] || actions[0];
    for (const param of magentaParams(tool, action)) {
      if (values[param.id] === undefined) values[param.id] = param.value;
      box.appendChild(paramControl(param, values[param.id], (v) => {
        values[param.id] = v;
        if (param.id === "action") showDialogParams();
      }).row);
    }
  }

  function currentChoice() {
    const value = $("midi-tools-tool").value;
    return choiceFor(value, dialog.values[value] || {});
  }

  /** What Apply runs: a tool ("fx:<type>" or "mg:<tool>") with the values set for it. */
  function choiceFor(value, values = {}) {
    if (!value) return null;
    if (value.startsWith("fx:")) return { type: value.slice(3), params: { ...values } };
    const tool = installed.get(value.slice(3));
    if (!tool) return null;
    const actions = tool.actions || [];
    return { tool: tool.id, action: actions[Math.round(values.action || 0)] || actions[0], temperature: values.temperature,
             similarity: values.similarity, steps: values.steps, bars: [1, 2, 4, 8][Math.round(values.bars === undefined ? 1 : values.bars)],
             inKey: values.inKey === undefined ? true : values.inKey > 0.5, newTrack: values.newTrack === undefined ? true : values.newTrack > 0.5 };
  }

  function showTarget() {
    const target = currentTarget();
    $("midi-tools-target").textContent = target ? `On: ${target.text}` : "Select notes in the Editor, or MIDI clips in the arrangement.";
    $("midi-tools-apply").disabled = !target || dialog.busy;
  }

  function openDialog(preset) {
    fillToolList();
    if (preset) $("midi-tools-tool").value = preset;
    if (!$("midi-tools-tool").value) $("midi-tools-tool").value = "fx:arpeggiator";
    showDialogParams();
    showTarget();
    $("midi-tools-dialog").hidden = false;
    dialog.open = true;
  }

  function closeDialog() {
    $("midi-tools-dialog").hidden = true;
    dialog.open = false;
  }

  function setBusy(text) {
    dialog.busy = Boolean(text);
    const box = $("midi-tools-busy");
    box.hidden = !text;
    box.textContent = text || "";
    showTarget();
  }

  async function applyDialog() {
    const target = currentTarget();
    const choice = currentChoice();
    if (!target || !choice || dialog.busy) return;
    setBusy(choice.tool ? "Thinking… (the first time, the model loads)" : "Applying…");

    try {
      if (target.kind === "editor") {
        // The muted notes stay as they are; the others go to the tool with their operators.
        const taken = target.taken.filter((n) => !n.muted).map((n) => ({ pitch: n.pitch, start: n.start, length: n.length, velocity: n.velocity, ops: n.ops || null }));
        const result = await applyTo(choice, taken, target.from, target.to);
        if (result.newTrack) newTrackWith(target.trackId, [{ from: target.from, to: Math.max(target.to, ...result.notes.map((n) => n.start + n.length)), notes: result.notes }], result, choice);
        else if (!Hub.editor.replaceNotes(result.keep ? [] : taken, result.notes, result.label, { trackId: target.trackId, clipId: target.clipId }))
          Hub.toast("The Editor shows another clip now, so the tool's notes weren't put in. Open that clip and apply again.");
      } else {
        await applyToClips(target.clips, choice);
      }
    } catch (error) {
      Hub.toast(error && error.message ? error.message : String(error));
    }

    setBusy("");
  }

  async function applyToClips(clips, choice) {
    const edits = [], spans = [];
    let label = "MIDI Tool", newTrack = false, firstTrack = clips[0].track.id, result = null;

    if (choice.tool && choice.action === "morph") {
      if (clips.length < 2) throw new Error("Morph goes from one clip to another: select two MIDI clips.");
      const [a, b] = [clips[0], clips[clips.length - 1]];
      const otherNotes = clipNotes(b.clip).shown.filter((n) => !n.muted).map((n) => ({ ...n, start: n.start - b.clip.start }));
      result = await applyTo(choice, clipNotes(a.clip).shown.filter((n) => !n.muted), a.clip.start, a.clip.start + a.clip.length, { other: otherNotes });
      const end = Math.max(...result.notes.map((n) => n.start + n.length), a.clip.start + 1);
      newTrackWith(a.track.id, [{ from: a.clip.start, to: barLineAtOrAfter(end), notes: result.notes }], result, choice);
      return { newTrack: true, label: result.label };
    }

    for (const { track, clip } of clips) {
      // The muted notes don't go to the tool: they stay as they are.
      const { shown, hidden } = clipNotes(clip);
      const muted = shown.filter((n) => n.muted);
      result = await applyTo(choice, shown.filter((n) => !n.muted), clip.start, clip.start + clip.length);
      label = result.label;

      if (result.newTrack) {
        newTrack = true;
        spans.push({ from: clip.start, to: clip.start + clip.length, notes: result.notes });
        continue;
      }

      const notes = (result.keep ? shown : muted).concat(result.notes);
      const end = Math.max(clip.start + clip.length, ...notes.map((n) => n.start + n.length), result.extendTo || 0);
      const length = end > clip.start + clip.length + 1e-9 ? barLineAtOrAfter(end) - clip.start : clip.length;
      const offset = clip.offset || 0;
      const content = notes.filter((n) => n.start >= clip.start - 1e-9).map((n) => noteArray(n, n.start - clip.start + offset));
      const edit = { op: "notes", trackId: track.id, clipId: clip.id, notes: content.concat(hidden) };
      if (length !== clip.length) Object.assign(edit, { start: clip.start, length, offset });
      edits.push(edit);
    }

    if (newTrack) {
      newTrackWith(firstTrack, spans, result, choice);
      return { newTrack: true, label };
    }
    if (edits.length) Hub.send("clipEdits", { edits, label });
    return { newTrack: false, label, changed: edits.length > 0 };
  }

  /** A tool's notes on a new track under a track (improvise, harmonize, morph). */
  function newTrackWith(afterTrackId, spans, result, choice) {
    const tool = choice.tool ? installed.get(choice.tool) : null;
    const source = Hub.trackById(afterTrackId);
    const clips = spans.filter((s) => s.notes.length).map((s) => ({
      start: s.from,
      length: Math.max(0.25, barLineAtOrAfter(Math.max(s.to, ...s.notes.map((n) => n.start + n.length))) - s.from),
      notes: s.notes.map((n) => [n.pitch, Math.max(0, n.start - s.from), n.length, Hub.clamp(Math.round(n.velocity || 96), 1, 127)])
    }));
    if (!clips.length) { Hub.toast("The tool had nothing to add."); return; }
    Hub.send("midiToolTrack", { afterTrackId, label: result.label, drums: Boolean(tool && tool.drums),
                                name: `${source ? source.name + " " : ""}${tool ? (ACTIONS[choice.action] || tool.title) : "MIDI"}`, clips });
  }

  // ---------------------------------------------------------------- the Browser's MIDI tab
  let tabShown = false;

  function versionNewer(a, b) {
    const pa = String(a || "0").split(".").map(Number), pb = String(b || "0").split(".").map(Number);
    for (let i = 0; i < Math.max(pa.length, pb.length); i++) {
      const x = pa[i] || 0, y = pb[i] || 0;
      if (x !== y) return x > y;
    }
    return false;
  }

  /** The Magenta tools to show: the catalog's, with the server's newer ones and its own extra ones. */
  function catalogEntries() {
    const server = (Hub.fananContent ? Hub.fananContent.items() : []).filter((item) => item.kind === "miditool" && item.tool_id);
    const list = CATALOG.map((tool) => ({ ...tool }));
    for (const item of server) {
      const known = list.find((t) => t.id === item.tool_id);
      if (known) {
        if (versionNewer(item.version, known.version)) Object.assign(known, { server: item, version: item.version, description: item.description || known.description });
      } else {
        list.push({ id: item.tool_id, title: item.title, description: item.description, version: item.version, sizeMb: (item.size || 0) / 1048576, server: item, actions: [] });
      }
    }
    for (const tool of installed.values()) if (!list.some((t) => t.id === tool.id)) list.push({ ...tool });
    return list;
  }

  function showMidiTab() {
    if (Hub.showBrowserTab) Hub.showBrowserTab("midi");
  }

  function renderTab() {
    Hub.emit("midiToolsChanged");   // AI Tools' MIDI Generators tab lists them too
    if (!tabShown) return;
    const list = $("midi-list");
    if (!list) return;
    const text = ($("browser-search").value || "").trim().toLowerCase();
    const matches = (...words) => !text || words.join(" ").toLowerCase().includes(text);
    list.replaceChildren();

    list.appendChild(element("div", "midi-group", "Built in"));
    for (const type of builtinTypes()) {
      if (!matches(type.name, type.description)) continue;
      list.appendChild(toolRow({ type: type.id }, type.name, type.description, "MIDI FX and Apply"));
    }

    list.appendChild(element("div", "midi-group", "Magenta"));
    if (!magentaBuilt) list.appendChild(element("p", "browser-note", "This Sonify was built without magenta.js, so Magenta tools can't run."));

    for (const tool of catalogEntries()) {
      if (!matches(tool.title, tool.description)) continue;
      const have = installed.get(tool.id);
      const meta = [(tool.actions || []).map((a) => ACTIONS[a] || a).join(", "), tool.sizeMb ? `${Math.max(0.1, tool.sizeMb).toFixed(tool.sizeMb < 10 ? 1 : 0)} MB` : ""].filter(Boolean).join(" · ");
      const row = toolRow(have ? { tool: tool.id } : null, tool.title || tool.id, tool.description || "", meta, have);

      if (installing.has(tool.id)) {
        const bar = element("div", "export-bar midi-bar");
        const fill = element("i");
        fill.style.width = `${Math.round(installing.get(tool.id) * 100)}%`;
        bar.appendChild(fill);
        row.querySelector(".midi-text").appendChild(bar);
      } else {
        const update = have && versionNewer(tool.version, have.version);
        if (!have || update) row.appendChild(button(update ? "Update" : "Install", update ? "A newer version is out" : "Download it to this computer", true, () => install(tool)));
        if (have) row.appendChild(button("Remove", "Remove it from this computer", false, () => Hub.send("midiToolRemove", { id: tool.id })));
      }
      list.appendChild(row);
    }
  }

  function button(label, title, primary, onClick) {
    const b = element("button", "plain-button small" + (primary ? " primary" : ""), label);
    b.type = "button";
    b.title = title;
    b.addEventListener("click", (event) => { event.stopPropagation(); onClick(); });
    return b;
  }

  function toolRow(choice, title, description, meta, haveTool = true) {
    const row = element("div", "midi-item" + (choice ? "" : " unavailable"));
    const text = element("div", "midi-text");
    const name = element("div", "midi-title", title);
    if (choice && choice.tool && haveTool) name.appendChild(element("span", "fanan-badge", "Installed"));
    text.append(name);
    if (meta) text.appendChild(element("div", "muted small", meta));
    if (description) text.appendChild(element("div", "midi-description small", description));
    row.appendChild(text);

    if (choice) {
      row.draggable = true;
      row.title = "Drag onto a track for its MIDI FX; double-click: the selected track's";
      row.addEventListener("dragstart", (event) => {
        Hub.dragMidiTool = choice;
        if (event.dataTransfer) { event.dataTransfer.effectAllowed = "copy"; event.dataTransfer.setData("text/plain", title); }
      });
      row.addEventListener("dragend", () => { Hub.dragMidiTool = null; });
      row.addEventListener("dblclick", () => {
        const track = Hub.selectedTrack();
        if (track) addDevice(track.id, choice); else Hub.toast("Select an instrument track first.");
      });
      const apply = button("Apply…", "Apply it to the selected notes or clips", false, () => openDialog(choice.type ? `fx:${choice.type}` : `mg:${choice.tool}`));
      row.appendChild(apply);
    }
    return row;
  }

  function install(tool) {
    installing.set(tool.id, 0);
    renderTab();
    if (tool.server) Hub.send("contentInstall", { id: tool.server.id });
    else Hub.send("midiToolInstall", { tool: { id: tool.id, title: tool.title, description: tool.description, model: tool.model, checkpoint: tool.checkpoint,
                                               actions: tool.actions, live: tool.live, drums: Boolean(tool.drums), generator: Boolean(tool.generator),
                                               chords: Boolean(tool.chords), version: tool.version || "1" } });
  }

  // ---------------------------------------------------------------- events
  Hub.on("midiFxTypes", (data) => {
    types = data.types || [];
    maxDevices = data.maxDevices || maxDevices;
    renderTab();
  });

  Hub.on("midiTools", (data) => {
    installed.clear();
    for (const tool of data.installed || []) installed.set(tool.id, tool);
    magentaBuilt = data.magenta !== false;
    for (const id of [...installing.keys()]) if (installed.has(id)) installing.delete(id);
    if (data.error) Hub.toast(data.error);
    // Models stay loaded unless their tool went or changed.
    for (const [id, entry] of [...models]) if (!installed.has(id) || stampOf(installed.get(id)) !== entry.stamp) forgetModel(id);
    renderTab();
    refreshWindows();
    if (dialog.open) { fillToolList(); showDialogParams(); }
  });

  Hub.on("midiToolProgress", (p) => {
    installing.set(p.id, Hub.clamp01(p.fraction || 0));
    renderTab();
  });

  Hub.on("midiToolInstalled", (r) => {
    installing.delete(r.id);
    const batch = all.ids.has(r.id);
    if (!r.ok) Hub.toast(r.error || "That tool didn't install.");
    else if (!batch) Hub.toast(`Installed \u201c${(toolById(r.id) || {}).title || r.id}\u201d: add it to a track's MIDI FX, or Apply it to notes`);
    if (batch) { all.ids.delete(r.id); if (r.ok) all.done += 1; else all.failed += 1; nextOfAll(); }
    renderTab();
  });

  // Fanan's server: its tools install through the content route.
  Hub.on("contentResult", (r) => {
    if (r.kind !== "miditool") return;
    for (const tool of catalogEntries()) {
      if (!tool.server || tool.server.id !== r.id) continue;
      installing.delete(tool.id);
      if (all.ids.has(tool.id)) { all.ids.delete(tool.id); if (r.ok === false) all.failed += 1; else all.done += 1; nextOfAll(); }
    }
    renderTab();
  });
  Hub.on("contentProgress", (p) => {
    for (const tool of catalogEntries()) if (tool.server && tool.server.id === p.id) installing.set(tool.id, Hub.clamp01(p.fraction || 0));
    if (tabShown) renderTab();
  });
  Hub.on("fananContent", () => setTimeout(renderTab, 0));   // after hub-content.js has the items

  Hub.on("midiFxAdded", (reply) => {
    if (reply.error) { Hub.toast(reply.error); return; }
    if (reply.open) setTimeout(() => openDevice(reply.trackId, reply.fxId), 0);
  });

  Hub.on("tracks", () => {
    refreshWindows();
    if (dialog.open) showTarget();
  });
  Hub.on("selection", () => { if (dialog.open) showTarget(); });

  // Fanan's list: asked for when the tab is drawn, at most every few minutes (Refresh asks now).
  const CONTENT_EVERY = 5 * 60 * 1000;
  let contentAsked = -Infinity;
  function askForContent(now) {
    if (!now && performance.now() - contentAsked < CONTENT_EVERY) return;
    contentAsked = performance.now();
    Hub.send("contentList");
  }

  Hub.on("midiTab", () => {
    tabShown = true;
    askForContent(false);
    renderTab();
  });
  Hub.on("browserTab", (name) => { tabShown = name === "midi"; });

  Hub.commands.midiTools = () => openDialog();

  Hub.midiFx = { stripMarkup, showStrip, setupStrip, panelMarkup, showPanel, setupPanel, addMenu, addDevice, openDevice,
                 openDialog, runMagenta, chordIn, snapToKey, catalog: CATALOG, installed, paramControl, valueText, typeOf, mapOutput };

  // ---------------------------------------------------------------- Download All
  // Every Magenta tool not here yet, one download after another; one note when they're all done.
  const all = { queue: [], ids: new Set(), done: 0, failed: 0 };

  function installAll() {
    if (!magentaBuilt) return;
    const missing = catalogEntries().filter((tool) => !installed.has(tool.id) && !installing.has(tool.id) && !all.ids.has(tool.id));
    for (const tool of missing) { all.queue.push(tool.id); all.ids.add(tool.id); }
    nextOfAll();
    renderTab();
  }

  function nextOfAll() {
    if ([...installing.keys()].some((id) => all.ids.has(id))) return;   // one at a time
    while (all.queue.length) {
      const id = all.queue.shift();
      const tool = catalogEntries().find((t) => t.id === id);
      if (tool && !installed.has(id)) { install(tool); return; }
      all.ids.delete(id);
    }
    if (!all.ids.size && (all.done || all.failed)) {
      Hub.toast(all.failed ? `Downloaded ${all.done} Magenta ${all.done === 1 ? "tool" : "tools"}; ${all.failed} didn't download (try again).`
                           : `Downloaded ${all.done === 1 ? "the Magenta tool" : "all " + all.done + " Magenta tools"}: they're ready to use.`);
      all.done = 0;
      all.failed = 0;
    }
  }

  // ---------------------------------------------------------------- AI Tools' MIDI Generators tab
  /** Every tool, for a list of its own: the built-in devices, and Magenta's (installed or not). */
  function toolList() {
    return {
      magentaBuilt,
      builtin: builtinTypes().map((type) => ({ key: `fx:${type.id}`, name: type.name, description: type.description || "" })),
      magenta: catalogEntries().map((tool) => {
        const have = installed.get(tool.id);
        return { key: `mg:${tool.id}`, id: tool.id, name: tool.title || tool.id, description: tool.description || "",
                 installed: Boolean(have), update: Boolean(have && versionNewer(tool.version, have.version)),
                 installing: installing.has(tool.id) ? installing.get(tool.id) : null, drums: Boolean(tool.drums),
                 queued: all.ids.has(tool.id) && !installing.has(tool.id),
                 actions: (tool.actions || []).map((a) => ACTIONS[a] || a), sizeMb: tool.sizeMb || 0 };
      })
    };
  }

  /** A tool's settings as parameters, for the values set so far (a Magenta tool's depend on its Do). */
  function paramsFor(value, values = {}) {
    if (!value) return [];
    if (value.startsWith("fx:")) {
      const type = typeById(value.slice(3));
      return type ? type.params : [];
    }
    const tool = installed.get(value.slice(3));
    if (!tool) return [];
    const actions = tool.actions || [];
    return magentaParams(tool, actions[Math.round(values.action || 0)] || actions[0]);
  }

  Hub.midiGenerators = {
    list: toolList,
    params: paramsFor,
    choice: choiceFor,
    apply: applyToClips,       // ([{ track, clip }], choice): one undo step; morph takes two clips
    install: (id) => { const tool = catalogEntries().find((t) => t.id === id); if (tool && !installing.has(id)) install(tool); },
    installAll,                // every Magenta tool not here yet, one after another
    downloadingAll: () => all.ids.size > 0,
    remove: (id) => Hub.send("midiToolRemove", { id }),
    refresh: (now = false) => { if (now) Hub.send("midiToolsList"); askForContent(now); }
  };

  Hub.on("ready", () => {
    $("midi-tools-tool").addEventListener("change", () => { showDialogParams(); $("midi-tools-tool").blur(); });
    $("midi-tools-apply").addEventListener("click", applyDialog);
    $("midi-tools-close").addEventListener("click", closeDialog);
    $("midi-tools-dialog").addEventListener("keydown", (event) => { if (event.key === "Escape") { event.stopPropagation(); closeDialog(); } });

    // The panel doesn't block the page: notes and clips can be chosen (and the song played) while
    // it's open, and what it works on follows.
    document.addEventListener("pointerup", () => { if (dialog.open) setTimeout(showTarget, 0); }, true);
    const editorTools = $("editor-midi-tools");
    if (editorTools) editorTools.addEventListener("click", () => { Hub.state.focus = "editor"; openDialog(); });
    $("midi-refresh").addEventListener("click", () => { Hub.send("midiToolsList"); askForContent(true); });
    $("browser-search").addEventListener("input", () => { if (tabShown) renderTab(); });
  });
})();
