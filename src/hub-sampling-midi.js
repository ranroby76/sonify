// D:\workspace\Fanan_web_hub\src\hub-sampling-midi.js
//
// The Sampling Studio's MIDI, beside its nodes (hub-sampling-nodes.js):
//
//   The keyboard    on the nodes' left, upright, the low notes at the bottom: it plays through
//                   the MIDI In node to the VSTis wired from it (as Sonify's other keyboards do:
//                   noteOn, noteOff). Further right on a key plays louder; drag along it for a
//                   glissando; the wheel scrolls it. What comes through the MIDI In node lights up
//                   on it.
//   The tabs        on the nodes' right: Sampled Keys (the keys picked, with their zones; written by
//                   hub-sampling.js) and MIDI Monitor: the last 20 messages that came in, oldest
//                   first, each with its number, channel, what it is and where it came from. One the
//                   MIDI In node doesn't hear (its input isn't ticked) is greyed.
//
// MIDI is heard only in the studio in front: while the Sampling Studio is, the live MIDI is the
// MIDI In node's, and no other studio's (AudioEngine::setMidiFocus).
//
// Sonify sends: sampling {kind: midiMonitor, messages [{n, time, source, bytes, sysex, heard}]}.

(() => {
  "use strict";

  const { $, element } = Hub;

  const NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const noteName = (n) => NAMES[n % 12] + (Math.floor(n / 12) - 1);
  const isBlack = (n) => [1, 3, 6, 8, 10].includes(n % 12);
  const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));

  const WHITE_H = 14;                     // a white key's height
  const SHOWN = 20;                       // the monitor keeps the last 20
  const CC_NAMES = {
    0: "Bank Select", 1: "Mod Wheel", 2: "Breath", 4: "Foot", 5: "Portamento Time", 7: "Volume", 8: "Balance", 10: "Pan",
    11: "Expression", 32: "Bank Select LSB", 64: "Sustain", 65: "Portamento", 66: "Sostenuto", 67: "Soft Pedal", 68: "Legato",
    71: "Resonance", 72: "Release", 73: "Attack", 74: "Cutoff", 120: "All Sound Off", 121: "Reset All", 123: "All Notes Off"
  };

  // The keys, bottom to top: each white key's index, and each black key's white key below it.
  const whiteIndex = [];
  let whites = 0;
  for (let n = 0; n < 128; n++) {
    whiteIndex[n] = whites;
    if (!isBlack(n)) whites++;
  }
  const HEIGHT = whites * WHITE_H;

  const st = {
    pressed: new Map(),      // pointerId -> note
    lit: new Set(),          // notes the MIDI In node is holding now (from the monitor)
    messages: [],            // the monitor's, oldest first
    chosen: "keys",          // the tab picked
    keysShown: true,         // Sampled Keys is there (not for chords)
    scrolled: false,         // the keyboard opened at middle C once
    frame: 0
  };

  // ---------------------------------------------------------------- the keyboard
  /** A white key's top (the low notes at the bottom). */
  const whiteTop = (n) => HEIGHT - (whiteIndex[n] + 1) * WHITE_H;
  const blackHeight = () => Math.round(WHITE_H * 0.64);

  /** A black key's top: across the line between the white keys either side of it. */
  const blackTop = (n) => HEIGHT - (whiteIndex[n]) * WHITE_H - blackHeight() / 2;

  function size() {
    const box = $("smp-vkeys");
    return { box, w: box ? box.clientWidth : 0, h: box ? box.clientHeight : 0 };
  }

  function blackWidth(w) { return Math.round(w * 0.6); }

  /** The key at a point (the canvas's own pixels), and how hard: further right is louder. */
  function keyAt(x, y, w) {
    const bw = blackWidth(w), bh = blackHeight();
    if (x < bw) {
      for (let n = 1; n < 128; n++) {
        if (!isBlack(n)) continue;
        const top = blackTop(n);
        if (y >= top && y < top + bh) return { note: n, velocity: 0.3 + 0.7 * clamp(x / bw, 0, 1) };
      }
    }
    const index = clamp(Math.floor((HEIGHT - y) / WHITE_H), 0, whites - 1);
    let note = 0;
    for (let n = 0; n < 128; n++) if (!isBlack(n) && whiteIndex[n] === index) { note = n; break; }
    return { note, velocity: 0.3 + 0.7 * clamp(x / w, 0, 1) };
  }

  function drawKeys() {
    st.frame = 0;
    const { box, w, h } = size();
    const canvas = $("smp-vkeys-canvas");
    if (!box || !canvas || w <= 0 || h <= 0) return;
    const dpr = window.devicePixelRatio || 1;
    if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(HEIGHT * dpr)) {
      canvas.width = Math.round(w * dpr);
      canvas.height = Math.round(HEIGHT * dpr);
      canvas.style.width = `${w}px`;
      canvas.style.height = `${HEIGHT}px`;
    }
    // The first time it shows: middle C in the middle.
    if (!st.scrolled) {
      st.scrolled = true;
      box.scrollTop = Math.max(0, whiteTop(60) - h / 2);
    }

    const g = canvas.getContext("2d");
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.fillStyle = "#141418";
    g.fillRect(0, 0, w, HEIGHT);
    const down = new Set(st.pressed.values());

    // White keys, a line between each.
    for (let n = 0; n < 128; n++) {
      if (isBlack(n)) continue;
      const top = whiteTop(n);
      g.fillStyle = down.has(n) ? "#7cc4ff" : st.lit.has(n) ? "#00aaff" : "#e6e6e6";
      g.fillRect(0, top, w, WHITE_H);
      g.fillStyle = "#8a8a8a";
      g.fillRect(0, top, w, 1);
      if (n % 12 === 0) {
        g.fillStyle = down.has(n) || st.lit.has(n) ? "#0a2a40" : "#6a6a6a";
        g.font = "bold 9px sans-serif";
        g.textAlign = "right";
        g.textBaseline = "middle";
        g.fillText(noteName(n), w - 3, top + WHITE_H / 2 + 0.5);
      }
    }

    // Black keys, from the left.
    const bw = blackWidth(w), bh = blackHeight();
    for (let n = 1; n < 128; n++) {
      if (!isBlack(n)) continue;
      const top = blackTop(n);
      g.fillStyle = down.has(n) ? "#4aa6e8" : st.lit.has(n) ? "#0088cc" : "#222226";
      g.fillRect(0, top, bw, bh);
      g.fillStyle = "rgba(255, 255, 255, 0.12)";
      g.fillRect(bw - 3, top + 1, 2, bh - 2);
    }
  }

  function drawSoon() {
    if (!st.frame) st.frame = requestAnimationFrame(drawKeys);
  }

  function local(event) {
    const r = $("smp-vkeys-canvas").getBoundingClientRect();
    return { x: event.clientX - r.left, y: event.clientY - r.top };
  }

  function hold(pointerId, key) {
    st.pressed.set(pointerId, key.note);
    Hub.send("noteOn", { note: key.note, velocity: key.velocity });
    drawSoon();
  }

  function unhold(pointerId) {
    const note = st.pressed.get(pointerId);
    if (note === undefined) return;
    st.pressed.delete(pointerId);
    Hub.send("noteOff", { note });
    drawSoon();
  }

  function releaseAll() {
    for (const id of [...st.pressed.keys()]) unhold(id);
  }

  // ---------------------------------------------------------------- the monitor
  /** What a message is, in a few words. */
  function describe(bytes, sysex) {
    const [status = 0, a = 0, b = 0] = bytes;
    const type = status & 0xf0;
    if (status === 0xf0) return { text: `SysEx (${sysex || bytes.length} bytes)` };
    if (status >= 0xf0) {
      const names = { 0xf1: "MTC Quarter Frame", 0xf2: `Song Position ${a | (b << 7)}`, 0xf3: `Song Select ${a}`, 0xf6: "Tune Request",
                      0xfa: "Start", 0xfb: "Continue", 0xfc: "Stop", 0xff: "Reset" };
      return { text: names[status] || `System ${status.toString(16).toUpperCase()}` };
    }
    const key = (n) => `${noteName(n)} (${n})`;
    switch (type) {
      case 0x90: return b > 0 ? { text: `Note On ${key(a)} v${b}`, kind: "on" } : { text: `Note Off ${key(a)}`, kind: "off" };
      case 0x80: return { text: `Note Off ${key(a)} v${b}`, kind: "off" };
      case 0xa0: return { text: `Poly Pressure ${key(a)} ${b}` };
      case 0xb0: return { text: `CC ${a}${CC_NAMES[a] ? " " + CC_NAMES[a] : ""} = ${b}` };
      case 0xc0: return { text: `Program ${a}` };
      case 0xd0: return { text: `Channel Pressure ${a}` };
      case 0xe0: {
        const bend = (a | (b << 7)) - 8192;
        return { text: `Pitch Bend ${bend > 0 ? "+" : ""}${bend}` };
      }
      default: return { text: bytes.map((x) => x.toString(16).padStart(2, "0")).join(" ") };
    }
  }

  /** The keys the MIDI In node holds, as what it hears goes by. */
  function follow(bytes) {
    const [status = 0, a = 0, b = 0] = bytes;
    const type = status & 0xf0;
    if (type === 0x90 && b > 0) st.lit.add(a);
    else if (type === 0x80 || type === 0x90) st.lit.delete(a);
    else if (type === 0xb0 && (a === 120 || a === 123)) st.lit.clear();
  }

  function renderMonitor() {
    const list = $("smp-monitor-list");
    if (!list) return;
    list.textContent = "";
    if (!st.messages.length) {
      list.appendChild(element("div", "smp-monitor-empty", "Nothing yet: play a MIDI keyboard (or the keys on the nodes' left). What the MIDI In node hears shows here, the last 20 messages."));
      return;
    }
    for (const m of st.messages) {
      const bytes = Array.isArray(m.bytes) ? m.bytes : [];
      const what = describe(bytes, m.sysex);
      const status = bytes[0] || 0;
      const row = element("div", "smp-monitor-row" + (what.kind ? " " + what.kind : "") + (m.heard ? "" : " unheard"));
      row.appendChild(element("span", "num", `#${m.n}`));
      row.appendChild(element("span", "ch", status < 0xf0 ? String((status & 0x0f) + 1) : ""));
      row.appendChild(element("span", "msg", what.text));
      row.appendChild(element("span", "src", m.source || ""));
      const hex = bytes.map((x) => x.toString(16).toUpperCase().padStart(2, "0")).join(" ");
      row.title = `${what.text}, channel ${status < 0xf0 ? (status & 0x0f) + 1 : "-"}, from ${m.source || "?"} (${hex})`
                + (m.heard ? "" : ". Not heard: the MIDI In node's list doesn't tick this input.");
      list.appendChild(row);
    }
    list.scrollTop = list.scrollHeight;
  }

  function take(messages) {
    let heard = false;
    for (const m of messages) {
      st.messages.push(m);
      if (m.heard && Array.isArray(m.bytes)) {
        follow(m.bytes);
        heard = true;
      }
    }
    if (st.messages.length > SHOWN) st.messages.splice(0, st.messages.length - SHOWN);
    renderMonitor();
    if (heard) {
      drawSoon();
      if (Hub.samplingNodes && Hub.samplingNodes.blink) Hub.samplingNodes.blink();
    }
  }

  // ---------------------------------------------------------------- the tabs
  function tabNow() {
    return st.keysShown ? st.chosen : "monitor";
  }

  function renderTabs() {
    const tab = tabNow();
    for (const b of document.querySelectorAll(".smp-side-tab")) {
      b.setAttribute("aria-selected", String(b.dataset.side === tab));
      b.tabIndex = b.dataset.side === tab ? 0 : -1;
      if (b.dataset.side === "keys") b.hidden = !st.keysShown;
    }
    $("smp-zone-list").hidden = tab !== "keys";
    $("smp-monitor").hidden = tab !== "monitor";
    if (tab === "monitor") renderMonitor();
  }

  /** Sampled Keys is there or not (chords have none); the tabs show with it, or with the nodes. */
  function layout(keysShown, nodesShown) {
    st.keysShown = Boolean(keysShown);
    const side = $("smp-side");
    if (side) side.hidden = !keysShown && !nodesShown;
    if (!nodesShown) releaseAll();
    renderTabs();
    drawSoon();
  }

  function showMonitor() {
    st.chosen = "monitor";
    renderTabs();
  }

  Hub.samplingMidi = { layout, showMonitor };

  Hub.on("sampling", (data) => {
    if (data && data.kind === "midiMonitor" && Array.isArray(data.messages)) take(data.messages);
  });

  // Leaving the studio: whatever the keyboard holds lets go, and the lights go out (the MIDI In
  // hears nothing until it's back).
  Hub.on("studio", (id) => {
    if (id === "sampling") return;
    releaseAll();
    st.lit.clear();
    drawSoon();
  });

  Hub.on("ready", () => {
    const canvas = $("smp-vkeys-canvas");
    if (!canvas) return;

    canvas.addEventListener("pointerdown", (event) => {
      if (event.button !== 0) return;
      event.preventDefault();
      canvas.setPointerCapture(event.pointerId);
      const p = local(event);
      hold(event.pointerId, keyAt(p.x, p.y, size().w));
    });

    // Along the keys: each in turn.
    canvas.addEventListener("pointermove", (event) => {
      if (!st.pressed.has(event.pointerId)) return;
      const p = local(event);
      const key = keyAt(p.x, p.y, size().w);
      if (key.note === st.pressed.get(event.pointerId)) return;
      unhold(event.pointerId);
      hold(event.pointerId, key);
    });

    const end = (event) => unhold(event.pointerId);
    canvas.addEventListener("pointerup", end);
    canvas.addEventListener("pointercancel", end);
    canvas.addEventListener("lostpointercapture", end);
    canvas.addEventListener("contextmenu", (event) => event.preventDefault());
    window.addEventListener("blur", releaseAll);

    new ResizeObserver(drawSoon).observe($("smp-vkeys"));

    for (const b of document.querySelectorAll(".smp-side-tab")) {
      b.addEventListener("click", () => {
        st.chosen = b.dataset.side;
        renderTabs();
      });
    }

    $("smp-monitor-clear").addEventListener("click", () => {
      st.messages = [];
      renderMonitor();
    });

    renderTabs();
  });
})();
