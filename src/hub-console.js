// D:\workspace\Fanan_web_hub\src\hub-console.js
//
// The Console (F3), Studio One's mixer: a channel strip for every track and the
// Main channel on the right, with a strip for each Sub output left of it. Each channel
// picks its output at the top (Main or a Sub); Main and the Subs show the device channels
// they play on (Song Setup > Audio I/O Setup > Outputs). Faders use the same scale as the
// track headers.
// Meters show each channel after its fader, with a peak hold and a clip light.
// Each strip lists its inserts: click one to open it in the Inspector, click its
// light to bypass it, + to add an effect, or drop one from the Browser. An instrument
// channel lists its MIDI FX above them (hub-miditools.js): the devices its notes go
// through before the instrument hears them.
// While the Analog Board mixes the song, the Console is dimmed and says so.

(() => {
  "use strict";

  const { $, element } = Hub;
  const SCALE = [10, 6, 0, -6, -12, -24, -36, -48, -72];
  const CAP = 32;                     // fader cap height in px; the meters and scale sit inside its travel
  const FALL_DB_PER_SECOND = 26;      // how fast a meter drops
  const HOLD_MS = 1500;               // how long the peak line stays before it falls too
  const FLOOR_DB = -144;

  const strips = new Map();           // track id -> strip (FX channels too)
  const expanded = new Set();         // strips showing their inserts and sends
  let main = null;

  const ICON_MONITOR = '<svg viewBox="0 0 16 16"><path d="M2 6h3l4-3v10L5 10H2z"/><path d="M11 5.5a3.5 3.5 0 0 1 0 5" fill="none" stroke="currentColor" stroke-width="1.4"/></svg>';
  const ICON_RECORD = '<svg viewBox="0 0 16 16"><circle cx="8" cy="8" r="4.5"/></svg>';
  const ICON_POWER = '<svg viewBox="0 0 16 16"><path d="M7.25 1.5h1.5v6h-1.5z"/><path d="M4.2 3.6l1.06 1.06A4.25 4.25 0 1 0 10.74 4.66L11.8 3.6A5.75 5.75 0 1 1 4.2 3.6z"/></svg>';
  const ICON_EXPAND = '<svg viewBox="0 0 16 16"><path d="M6 3.5l4.5 4.5L6 12.5" fill="none" stroke="currentColor" stroke-width="1.8"/></svg>';
  const ICON_SCREEN = '<svg viewBox="0 0 16 16"><rect x="2" y="3" width="12" height="10" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.3"/><path d="M2 5.8h12" stroke="currentColor" stroke-width="1.3"/></svg>';
  const ICON_MASTER = '<svg viewBox="0 0 16 16"><path d="M8 1.8l1.7 3.9 4.2.4-3.2 2.8.9 4.1L8 10.9 4.4 13l.9-4.1L2.1 6.1l4.2-.4z" fill="currentColor"/></svg>';
  const ICON_REMOVE = '<svg viewBox="0 0 16 16"><path d="M4.5 4.5l7 7M11.5 4.5l-7 7" stroke="currentColor" stroke-width="1.6"/></svg>';
  const ICON_KEYS = '<svg viewBox="0 0 16 16"><path d="M1.5 3.5h13v9h-13z" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M5 3.5v5M8 3.5v5M11 3.5v5" stroke="currentColor" stroke-width="1.6"/></svg>';

  // ---------------------------------------------------------------- meters
  class Meter {
    constructor(root) {
      this.bars = [...root.querySelectorAll(".bar")].map((bar) => ({
        cover: bar.querySelector(".cover"),
        hold: bar.querySelector(".hold"),
        level: FLOOR_DB, peak: FLOOR_DB, peakAt: 0, shownCover: -1, shownHold: -1
      }));
      this.clip = root.querySelector(".clip");
      this.clipped = false;
      this.clip.addEventListener("click", (event) => {
        event.stopPropagation();
        this.resetClip();
      });
    }

    /** New peaks from the engine (linear, 1.0 = 0 dBFS). */
    feed(left, right) {
      const now = performance.now();
      [left, right].forEach((gain, index) => {
        const bar = this.bars[index];
        if (!bar) return;
        const db = Hub.gainToDb(gain || 0);
        if (db > bar.level) bar.level = db;
        if (db >= bar.peak) { bar.peak = db; bar.peakAt = now; }
        if (gain >= 1 && !this.clipped) {
          this.clipped = true;
          this.clip.classList.add("on");
        }
      });
    }

    tick(now, seconds) {
      for (const bar of this.bars) {
        bar.level = Math.max(FLOOR_DB, bar.level - FALL_DB_PER_SECOND * seconds);
        if (now - bar.peakAt > HOLD_MS) bar.peak = Math.max(bar.level, bar.peak - FALL_DB_PER_SECOND * seconds);

        // Only touch the page when something visibly moved.
        const cover = Math.round((1 - Hub.dbToFader(bar.level)) * 1000) / 10;
        if (cover !== bar.shownCover) {
          bar.shownCover = cover;
          bar.cover.style.height = cover + "%";
        }

        const hold = bar.peak > Hub.SILENT_DB ? Math.round(Hub.dbToFader(bar.peak) * 1000) / 10 : -1;
        if (hold !== bar.shownHold) {
          bar.shownHold = hold;
          bar.hold.hidden = hold < 0;
          if (hold >= 0) bar.hold.style.bottom = hold + "%";
        }
      }
    }

    resetClip() {
      this.clipped = false;
      this.clip.classList.remove("on");
      for (const bar of this.bars) bar.peak = bar.level;
    }

    highestPeak() {
      return Math.max(...this.bars.map((bar) => bar.peak));
    }
  }

  function meterMarkup() {
    return `<div class="vmeter">
        <span class="clip" title="Clip: the signal went over 0 dB. Click to reset."></span>
        <div class="bar"><div class="cover"></div><div class="hold" hidden></div></div>
        <div class="bar"><div class="cover"></div><div class="hold" hidden></div></div>
      </div>`;
  }

  function scaleMarkup() {
    return `<div class="scale">${SCALE.map((db) =>
      `<span style="bottom:calc(${CAP / 2}px + ${Hub.dbToFader(db).toFixed(4)} * (100% - ${CAP}px))">${db}</span>`).join("")}</div>`;
  }

  // ---------------------------------------------------------------- faders
  /** A vertical fader you drag (Shift for fine), scroll, arrow-key, or double-click to 0 dB. */
  function makeFader(fader, get, set) {
    let drag = null;
    const travel = () => Math.max(40, fader.clientHeight - CAP);
    const move = (delta) => set(Hub.faderToDb(Hub.clamp01(Hub.dbToFader(get()) + delta)));

    fader.addEventListener("pointerdown", (event) => {
      if (event.button !== 0) return;
      event.preventDefault();
      event.stopPropagation();
      fader.focus();
      if (fader.setPointerCapture) fader.setPointerCapture(event.pointerId);
      drag = { y: event.clientY, position: Hub.dbToFader(get()), pointer: event.pointerId, fine: event.shiftKey };
      fader.classList.add("active");
    });

    fader.addEventListener("pointermove", (event) => {
      if (!drag || event.pointerId !== drag.pointer) return;

      // Changing Shift mid-drag restarts from here, so the cap never jumps.
      if (event.shiftKey !== drag.fine) {
        drag = { y: event.clientY, position: Hub.dbToFader(get()), pointer: drag.pointer, fine: event.shiftKey };
        return;
      }

      const scale = drag.fine ? 0.1 : 1;
      set(Hub.faderToDb(Hub.clamp01(drag.position + (drag.y - event.clientY) / travel() * scale)));
    });

    const end = (event) => {
      if (!drag || event.pointerId !== drag.pointer) return;
      drag = null;
      fader.classList.remove("active");
    };

    fader.addEventListener("pointerup", end);
    fader.addEventListener("pointercancel", end);
    fader.addEventListener("lostpointercapture", end);
    fader.addEventListener("dblclick", (event) => {
      event.stopPropagation();
      set(0);
    });

    fader.addEventListener("wheel", (event) => {
      event.preventDefault();
      move((event.deltaY < 0 ? 1 : -1) * (event.shiftKey ? 0.002 : 0.01));
    }, { passive: false });

    fader.addEventListener("keydown", (event) => {
      const steps = { ArrowUp: 0.005, ArrowDown: -0.005, PageUp: 0.05, PageDown: -0.05 };
      if (!(event.key in steps)) return;
      event.preventDefault();
      event.stopPropagation();
      move(steps[event.key]);
    });
  }

  function showFader(fader, readout, db) {
    const position = Hub.dbToFader(db);
    fader.style.setProperty("--pos", position.toFixed(4));
    fader.setAttribute("aria-valuenow", db.toFixed(1));
    fader.setAttribute("aria-valuetext", Hub.dbText(db));
    readout.textContent = Hub.dbText(db);
  }

  // ---------------------------------------------------------------- inserts on a strip
  function insertsMarkup() {
    return `<div class="strip-inserts">
        <div class="strip-insert-list"></div>
        <button class="strip-add" title="Add an effect" aria-label="Add an effect">+ Insert</button>
      </div>`;
  }

  /** Wires a strip's inserts area to its channel (a track id or Hub.MAIN). */
  function setupInserts(area, channel) {
    area.querySelector(".strip-add").addEventListener("click", (event) => {
      event.stopPropagation();
      Hub.addEffectMenu(event.currentTarget, channel);
    });

    area.addEventListener("click", (event) => {
      const item = event.target.closest(".strip-insert-item");
      if (!item) return;
      event.stopPropagation();
      const insertId = Number(item.dataset.insertId);

      if (event.target.closest(".mini-power")) {
        const power = item.querySelector(".mini-power");
        const bypass = power.getAttribute("aria-pressed") === "true";
        power.setAttribute("aria-pressed", String(!bypass));
        item.classList.toggle("bypassed", bypass);
        Hub.send("bypassInsert", { trackId: channel, insertId, bypass });
        return;
      }

      Hub.openScreen(channel, insertId);   // its window, as in Fender Studio
    });
  }

  /** Strips are narrow: drop a leading "[Pack]" tag, so "[Basics] Chorus" shows as "Chorus". */
  const shortName = (name) => name.replace(/^\[[^\]]*\]\s*/, "") || name;

  /** kept: a DAWproject's devices that can't play here (placeholders), listed greyed after the inserts. */
  function showInserts(area, inserts, channel, kept = []) {
    const key = inserts.map((i) => `${i.insertId}:${i.bypass ? 1 : 0}:${i.name}`).join(",")
      + "|" + kept.map((k) => k.name).join(",");
    if (area.dataset.key === key) return;
    area.dataset.key = key;

    const list = area.querySelector(".strip-insert-list");
    list.textContent = "";

    for (const insert of inserts) {
      const item = element("div", "strip-insert-item");
      item.dataset.insertId = String(insert.insertId);
      item.classList.toggle("bypassed", Boolean(insert.bypass));
      const power = element("button", "mini-power");
      power.innerHTML = ICON_POWER;
      power.setAttribute("aria-pressed", String(!insert.bypass));
      power.setAttribute("aria-label", insert.bypass ? `Switch ${insert.name} on` : `Bypass ${insert.name}`);
      const name = element("button", "strip-insert-name", shortName(insert.name));
      name.title = `${insert.name}: click to edit it in the Inspector, double-click to open its screen`;
      name.addEventListener("dblclick", () => Hub.openScreen(channel, insert.insertId));
      item.append(power, name);
      list.appendChild(item);
    }

    for (const device of kept) {
      const item = element("div", "strip-kept-item");
      const name = element("span", "strip-insert-name", shortName(device.name));
      item.title = `${device.name}${device.format ? ` (${device.format})` : ""}: from a DAWproject, not playable here. `
        + "It's kept with the song, with its settings, and Export DAWproject writes it back.";
      item.append(element("span", "strip-kept-mark"), name);
      list.appendChild(item);
    }

    area.querySelector(".strip-add").hidden = inserts.length >= Hub.MAX_INSERTS;
  }

  /** Browser plugins dropped on a strip: effects join its inserts, instruments replace its instrument. */
  function setupStripDrop(root, channel) {
    const accepts = () => Boolean(Hub.dragEntry) && (channel !== Hub.MAIN || Hub.dragEntry.kind === "effect");

    root.addEventListener("dragover", (event) => {
      if (!accepts()) return;
      event.preventDefault();
      if (event.dataTransfer) event.dataTransfer.dropEffect = "copy";
      root.classList.add("drop-target");
    });

    root.addEventListener("dragleave", (event) => {
      if (!root.contains(event.relatedTarget)) root.classList.remove("drop-target");
    });

    root.addEventListener("drop", (event) => {
      root.classList.remove("drop-target");
      if (!accepts()) return;
      event.preventDefault();
      Hub.dropEntryOn(Hub.dragEntry, channel);
      Hub.dragEntry = null;
    });
  }

  // ---------------------------------------------------------------- the expanded panel: inserts and sends
  /** Wires a strip's panel: its inserts (like Fender Studio's: on/off, name, the plugin's
      screen), its sends (to FX channels, or to a plugin's sidechain), the expand toggle. */
  function setupPanel(strip, channel) {
    const root = strip.root;
    setupPanelInserts(root, channel);

    root.querySelector(".panel-add-send").addEventListener("click", (event) => {
      event.stopPropagation();
      sendMenu(event.currentTarget, channel);
    });
    setupPanelSends(root, channel);

    root.querySelector(".panel-remove").addEventListener("click", (event) => {
      event.stopPropagation();
      Hub.send("removeTrack", { id: channel });
    });
  }

  /** The expand toggle and the panel's inserts (Main has these, and no sends). */
  function setupPanelInserts(root, channel) {
    root.querySelector(".strip-expand").addEventListener("click", (event) => {
      event.stopPropagation();
      if (expanded.has(channel)) expanded.delete(channel);
      else expanded.add(channel);
      root.classList.toggle("expanded", expanded.has(channel));
      event.currentTarget.setAttribute("aria-pressed", String(expanded.has(channel)));
    });

    root.querySelector(".panel-add").addEventListener("click", (event) => {
      event.stopPropagation();
      Hub.addEffectMenu(event.currentTarget, channel);
    });

    root.querySelector(".panel-insert-list").addEventListener("click", (event) => {
      const item = event.target.closest(".panel-insert");
      if (!item) return;
      event.stopPropagation();
      const insertId = Number(item.dataset.insertId);

      if (event.target.closest(".panel-power")) {
        const power = item.querySelector(".panel-power");
        const bypass = power.getAttribute("aria-pressed") === "true";
        power.setAttribute("aria-pressed", String(!bypass));
        item.classList.toggle("bypassed", bypass);
        Hub.send("bypassInsert", { trackId: channel, insertId, bypass });
      } else if (event.target.closest(".panel-insert-remove")) {
        Hub.send("removeInsert", { trackId: channel, insertId });
      } else {
        Hub.openScreen(channel, insertId);   // the UI button and the name both open it
      }
    });

    setupInsertReorder(root.querySelector(".panel-insert-list"), channel);
  }

  function setupPanelSends(root, channel) {
    const sends = root.querySelector(".panel-send-list");
    sends.addEventListener("click", (event) => {
      const row = event.target.closest(".panel-send");
      if (!row) return;
      event.stopPropagation();
      const sendId = Number(row.dataset.sendId);

      if (event.target.closest(".panel-send-on")) {
        const button = row.querySelector(".panel-send-on");
        const on = button.getAttribute("aria-pressed") !== "true";
        button.setAttribute("aria-pressed", String(on));
        row.classList.toggle("off", !on);
        Hub.send("setSend", { trackId: channel, sendId, on });
      } else if (event.target.closest(".panel-send-remove")) {
        Hub.send("removeSend", { trackId: channel, sendId });
      }
    });

    // The level: the engine follows the drag; one step goes into the history when it's let go.
    sends.addEventListener("pointerdown", (event) => {
      const slider = event.target.closest(".panel-send-level");
      if (slider) slider.dataset.from = String(Hub.faderToDb(Number(slider.value)));
    });
    // A key moves it with input then change: the step starts from the level before the key.
    sends.addEventListener("keydown", (event) => {
      const slider = event.target.closest(".panel-send-level");
      if (slider && !slider.matches(":active")) slider.dataset.from = String(Hub.faderToDb(Number(slider.value)));
    });
    sends.addEventListener("input", (event) => {
      const slider = event.target.closest(".panel-send-level");
      if (!slider) return;
      const row = slider.closest(".panel-send");
      const db = Hub.faderToDb(Number(slider.value));
      row.querySelector(".panel-send-db").textContent = sendDbText(db);
      Hub.send("setSend", { trackId: channel, sendId: Number(row.dataset.sendId), level: db, commit: false });
    });
    sends.addEventListener("change", (event) => {
      const slider = event.target.closest(".panel-send-level");
      if (!slider) return;
      const row = slider.closest(".panel-send");
      const db = Hub.faderToDb(Number(slider.value));
      const message = { trackId: channel, sendId: Number(row.dataset.sendId), level: db, commit: true };
      if (slider.dataset.from !== undefined) message.from = Number(slider.dataset.from);
      delete slider.dataset.from;
      Hub.send("setSend", message);
    });
    sends.addEventListener("dblclick", (event) => {
      const slider = event.target.closest(".panel-send-level");
      if (!slider) return;
      const row = slider.closest(".panel-send");
      Hub.send("setSend", { trackId: channel, sendId: Number(row.dataset.sendId), level: 0, commit: true });
    });
  }

  /** Inserts reorder by dragging their banners (a line shows where one lands). */
  function setupInsertReorder(list, channel) {
    let dragging = null;   // { insertId, from }
    let line = null;

    const rows = () => [...list.querySelectorAll(".panel-insert")];
    const indexAt = (clientY) => {
      const all = rows();
      const at = all.findIndex((row) => { const r = row.getBoundingClientRect(); return clientY < r.top + r.height / 2; });
      return at < 0 ? all.length : at;
    };
    const clearLine = () => { if (line) line.remove(); line = null; };

    list.addEventListener("dragstart", (event) => {
      const row = event.target.closest(".panel-insert");
      if (!row) return;
      dragging = { insertId: Number(row.dataset.insertId), from: rows().indexOf(row) };
      row.classList.add("dragging");
      if (event.dataTransfer) {
        event.dataTransfer.effectAllowed = "move";
        event.dataTransfer.setData("text/plain", row.dataset.insertId);
      }
    });

    list.addEventListener("dragover", (event) => {
      if (!dragging) return;
      event.preventDefault();
      event.stopPropagation();
      if (event.dataTransfer) event.dataTransfer.dropEffect = "move";
      const index = indexAt(event.clientY);
      clearLine();
      line = element("div", "drop-line");
      const all = rows();
      if (index < all.length) list.insertBefore(line, all[index]);
      else if (all.length) all[all.length - 1].after(line);
    });

    list.addEventListener("drop", (event) => {
      if (!dragging) return;
      event.preventDefault();
      event.stopPropagation();
      const index = indexAt(event.clientY);
      const position = index > dragging.from ? index - 1 : index;
      if (position !== dragging.from) Hub.send("moveInsert", { trackId: channel, insertId: dragging.insertId, position });
      clearLine();
    });

    list.addEventListener("dragend", () => {
      list.querySelectorAll(".dragging").forEach((row) => row.classList.remove("dragging"));
      dragging = null;
      clearLine();
    });
  }

  /** Where a track can send: FX channels (or a new one), and every plugin in the song with a
      sidechain input on another channel, for ducking. */
  function sendMenu(anchor, channel) {
    const track = Hub.trackById(channel);
    if (!track) return;
    const sends = track.sends || [];
    const items = [];

    const fxChannels = (Hub.state.fxChannels || []).filter((fx) => !sends.some((s) => !s.sidechain && s.target === fx.id));
    if (fxChannels.length) items.push({ group: "FX Channels" });
    for (const fx of fxChannels) items.push({ label: fx.name, value: { target: fx.id } });
    items.push({ label: "New FX Channel", value: { newFx: true } });

    const targets = (Hub.state.sidechainTargets || []).filter((t) => t.channel !== channel
      && !sends.some((s) => s.sidechain && s.target === t.channel && s.slot === t.slot));
    items.push({ group: targets.length ? "Sidechain" : "Sidechain: no plugin with a sidechain input yet" });
    for (const t of targets) items.push({ label: `${t.name} \u00b7 ${t.channelName}`, value: { target: t.channel, slot: t.slot, sidechain: true } });

    Hub.popupMenu(anchor, items, (pick) => {
      if (pick.newFx) Hub.send("addFxChannel", { from: channel });
      else Hub.send("addSend", { trackId: channel, target: pick.target, slot: pick.slot || 0, sidechain: Boolean(pick.sidechain) });
    });
  }

  function showPanelInserts(list, inserts, channel) {
    const key = inserts.map((i) => `${i.insertId}:${i.bypass ? 1 : 0}:${i.name}`).join(",");
    if (list.dataset.key === key) return;
    list.dataset.key = key;
    list.textContent = "";

    for (const insert of inserts) {
      const item = element("div", "panel-insert" + (insert.bypass ? " bypassed" : ""));
      item.dataset.insertId = String(insert.insertId);
      const power = element("button", "panel-power");
      power.type = "button";
      power.innerHTML = ICON_POWER;
      power.setAttribute("aria-pressed", String(!insert.bypass));
      power.setAttribute("aria-label", insert.bypass ? `Switch ${insert.name} on` : `Switch ${insert.name} off`);
      power.title = insert.bypass ? "Off: click to switch it on" : "On: click to switch it off";
      const name = element("button", "panel-insert-name", shortName(insert.name));
      name.type = "button";
      name.title = `${insert.name}${insert.sidechain ? " (has a sidechain input)" : ""}: click to open it; drag to reorder`;
      const ui = element("button", "panel-ui");
      ui.type = "button";
      ui.innerHTML = ICON_SCREEN;
      ui.title = `Show ${insert.name}`;
      ui.setAttribute("aria-label", `Show ${insert.name}`);
      const remove = element("button", "panel-insert-remove");
      remove.type = "button";
      remove.innerHTML = ICON_REMOVE;
      remove.title = `Remove ${insert.name}`;
      remove.setAttribute("aria-label", `Remove ${insert.name}`);
      item.draggable = true;
      item.append(power, name, ui, remove);
      list.appendChild(item);
    }

    if (!inserts.length) list.appendChild(element("p", "panel-note", "Drop an effect here"));
  }

  /** The narrow send row shows the number only ("-13.5"); the full reading is its tooltip. */
  const sendDbText = (db) => Hub.dbText(db).replace(" dB", "");

  function showSends(list, track) {
    const sends = track.sends || [];
    const key = sends.map((s) => `${s.id}:${s.on ? 1 : 0}:${s.level.toFixed(2)}:${s.name}`).join(",");
    if (list.dataset.key === key || list.contains(document.activeElement)) return;
    list.dataset.key = key;
    list.textContent = "";

    for (const send of sends) {
      const row = element("div", "panel-send" + (send.sidechain ? " sidechain" : "") + (send.on ? "" : " off"));
      row.dataset.sendId = String(send.id);
      const top = element("div", "panel-send-top");
      const name = element("span", "panel-send-name", send.name);
      name.title = send.sidechain ? `Sidechain: into ${send.name}` : `To ${send.name}`;
      const on = element("button", "panel-send-on");
      on.type = "button";
      on.innerHTML = ICON_POWER;
      on.setAttribute("aria-pressed", String(send.on));
      on.setAttribute("aria-label", `${send.on ? "Switch off" : "Switch on"} the send to ${send.name}`);
      const remove = element("button", "panel-send-remove");
      remove.type = "button";
      remove.innerHTML = ICON_REMOVE;
      remove.setAttribute("aria-label", `Remove the send to ${send.name}`);
      top.append(on, name, remove);
      const bottom = element("div", "panel-send-bottom");
      const level = element("input", "panel-send-level");
      level.type = "range";
      level.min = "0";
      level.max = "1";
      level.step = "0.001";
      level.value = String(Hub.dbToFader(send.level));
      level.setAttribute("aria-label", `Send level to ${send.name}`);
      level.title = "Double-click for 0 dB";
      const db = element("output", "panel-send-db", sendDbText(send.level));
      db.title = Hub.dbText(send.level);
      bottom.append(level, db);
      row.append(top, bottom);
      list.appendChild(row);
    }

    if (!sends.length) list.appendChild(element("p", "panel-note", "No sends"));
  }

  // ---------------------------------------------------------------- channel strips
  function createStrip(track) {
    const root = element("div", "strip");
    root.dataset.id = String(track.id);
    root.innerHTML = `
      <div class="strip-io"><select class="strip-output" aria-label="Output" title="Where it plays: Main, or a Sub output (Song Setup)"></select><button class="tbtn polarity" data-action="polarity" type="button" aria-pressed="false" title="Polarity: turn the sound upside down (\u00d8)" aria-label="Invert polarity">\u00d8</button></div>
      ${Hub.midiFx ? Hub.midiFx.stripMarkup() : ""}
      ${insertsMarkup()}
      <div class="strip-pan">
        <input type="range" data-control="pan" min="-1" max="1" step="0.01" aria-label="Pan">
        <output></output>
      </div>
      <div class="strip-buttons">
        <button class="tbtn" data-action="mute" aria-pressed="false" title="Mute">M</button>
        <button class="tbtn" data-action="solo" aria-pressed="false" title="Solo">S</button>
        <button class="tbtn arm" data-action="recordArm" aria-pressed="false" title="Record arm">${ICON_RECORD}</button>
        <button class="tbtn monitor" data-action="monitor" aria-pressed="false" title="Monitor">${ICON_MONITOR}</button>
      </div>
      <output class="strip-db"></output>
      <div class="strip-body">
        <div class="fader" role="slider" tabindex="0" aria-label="Volume" aria-valuemin="-72" aria-valuemax="10"><div class="fader-cap"></div></div>
        ${meterMarkup()}
        ${scaleMarkup()}
      </div>
      <div class="strip-number"><span></span>${ICON_KEYS}<button class="strip-feeds" type="button" hidden></button><button class="strip-expand" type="button" aria-pressed="false"
        title="Show inserts and sends" aria-label="Show inserts and sends">${ICON_EXPAND}</button></div>
      <div class="strip-name" title="Double-click to rename"></div>
      <div class="strip-panel">
        ${Hub.midiFx ? Hub.midiFx.panelMarkup() : ""}
        <div class="panel-inserts">
          <div class="panel-head"><span>Inserts</span><button class="panel-add" type="button" title="Add an effect (or drop one from the Browser)" aria-label="Add an effect">+</button></div>
          <div class="panel-list panel-insert-list"></div>
        </div>
        <div class="panel-sends">
          <div class="panel-head"><span>Sends</span><button class="panel-add-send" type="button" title="Send to an FX channel, or to a plugin's sidechain" aria-label="Add a send">+</button></div>
          <div class="panel-list panel-send-list"></div>
        </div>
        <div class="panel-fx">
          <div class="panel-head"><span>FX Channel</span></div>
          <p class="panel-note">Tracks send to it; it plays into Main.</p>
          <button class="panel-remove" type="button">Remove</button>
        </div>
      </div>`;

    const strip = {
      root,
      fader: root.querySelector(".fader"),
      readout: root.querySelector(".strip-db"),
      pan: root.querySelector('[data-control="pan"]'),
      meter: new Meter(root.querySelector(".vmeter"))
    };

    const id = track.id;
    const current = () => Hub.trackById(id);

    makeFader(strip.fader, () => (current() ? current().volume : 0), (db) => {
      showFader(strip.fader, strip.readout, db);
      Hub.setTrackValue(id, "volume", db, strip.fader);
    });

    strip.pan.addEventListener("input", () => {
      const value = Number(strip.pan.value);
      strip.pan.nextElementSibling.textContent = Hub.panText(value);
      Hub.setTrackValue(id, "pan", value, strip.pan);
    });

    strip.pan.addEventListener("dblclick", (event) => {
      event.stopPropagation();
      strip.pan.value = "0";
      strip.pan.dispatchEvent(new Event("input", { bubbles: true }));
    });

    root.addEventListener("click", (event) => {
      // Mute, Solo, Record, Monitor: the whole selection when this channel is in it.
      const button = event.target.closest("[data-action]");

      if (button) {
        Hub.group.toggle(id, button.dataset.action, button);
        return;
      }

      // FX channels join the selection too; the keyboard keeps playing the track it played.
      if (!event.target.closest("input, select, .fader, .clip, .strip-inserts, .strip-midifx, .strip-panel, .strip-expand") && current())
        Hub.group.click(event, id);
    });

    // Right-click on the strip: Duplicate, Remove, Color, Rename, for the selection.
    root.addEventListener("contextmenu", (event) => {
      if (event.target.closest("input, select, .strip-inserts, .strip-midifx, .strip-panel")) return;
      event.preventDefault();
      Hub.group.menu(id, event.clientX, event.clientY);
    });

    // Dragging a strip by its name or number moves the selection.
    root.addEventListener("pointerdown", (event) => {
      if (!event.target.closest(".strip-name, .strip-number") || event.target.closest("input, button")) return;
      Hub.group.startReorder(event, id, "x", () => Hub.channelOrder().map((other) => ({ id: other, element: strips.get(other) && strips.get(other).root }))
                                                          .filter((item) => item.element));
    });

    // Main, a Sub output, or (a track's) a bus.
    root.querySelector(".strip-output").addEventListener("change", (event) => {
      const value = event.target.value;
      const track = current();
      event.target.blur();

      if (value.startsWith("bus:")) {
        const bus = Number(value.slice(4));
        if (track) track.bus = bus;
        event.target.classList.remove("sub");
        Hub.send("trackSettings", { id, bus });
        return;
      }

      const output = Number(value) || 0;
      if (track) { track.output = output; track.bus = 0; }
      event.target.classList.toggle("sub", output !== 0);
      Hub.send("trackSettings", { id, output, bus: 0 });
    });

    root.querySelector(".strip-feeds").addEventListener("click", (event) => {
      event.stopPropagation();
      Hub.group.selectFeeders(id);
    });

    setupInserts(root.querySelector(".strip-inserts"), id);
    setupStripDrop(root, id);
    setupPanel(strip, id);

    if (Hub.midiFx) {
      Hub.midiFx.setupStrip(root.querySelector(".strip-midifx"), id);
      Hub.midiFx.setupPanel(root.querySelector(".panel-midifx"), id);
    }

    root.querySelector(".strip-name").addEventListener("dblclick", (event) => {
      const track = current();
      if (track) Hub.inlineRename(event.currentTarget, track.name, (value) => Hub.send("trackSettings", { id, name: value }));
    });

    return strip;
  }

  function updateStrip(strip, track) {
    const root = strip.root;
    root.style.setProperty("--track-colour", track.colour);
    root.classList.toggle("fx", track.kind === "fx");
    root.classList.toggle("bus", track.kind === "bus");
    root.classList.toggle("instrument", track.kind === "instrument");   // it has MIDI FX
    root.classList.toggle("expanded", expanded.has(track.id));
    root.querySelector(".strip-expand").setAttribute("aria-pressed", String(expanded.has(track.id)));
    showPanelInserts(root.querySelector(".panel-insert-list"), track.inserts || [], track.id);
    showSends(root.querySelector(".panel-send-list"), track);
    root.classList.toggle("selected", Hub.isTrackSelected(track.id));
    showInserts(root.querySelector(".strip-inserts"), track.inserts || [], track.id, track.kept || []);

    if (Hub.midiFx) {
      Hub.midiFx.showStrip(root.querySelector(".strip-midifx"), track);
      Hub.midiFx.showPanel(root.querySelector(".panel-midifx"), track);
    }

    for (const action of ["mute", "solo", "recordArm", "monitor", "polarity"])
      root.querySelector(`[data-action="${action}"]`).setAttribute("aria-pressed", String(Boolean(track[action])));

    showOutput(root.querySelector(".strip-output"), track);
    showValues(strip, track);

    root.querySelector(".strip-number span").textContent = track.kind === "fx" ? "FX" : track.kind === "bus" ? "Bus" : String(track.number);

    // A bus shows how many channels play into it; a click selects them.
    const feeders = track.kind === "bus" ? Hub.state.tracks.filter((other) => other.bus === track.id) : [];
    const feeds = root.querySelector(".strip-feeds");
    feeds.hidden = track.kind !== "bus";
    feeds.textContent = `◂ ${feeders.length}`;
    feeds.title = feeders.length ? `Plays: ${feeders.map((other) => other.name).join(", ")}. Click to select them.` : "No channel plays into this bus yet";
    feeds.disabled = !feeders.length;
    const name = root.querySelector(".strip-name");
    if (!name.querySelector("input")) name.textContent = track.name;
  }

  // ---------------------------------------------------------------- outputs
  /** "Out 1+2": an output's device channels, by number (their names in the title). */
  function channelsText(output) {
    const names = Hub.state.outputChannels || [];
    const number = (channel) => String(channel + 1);
    const name = (channel) => names[channel] !== undefined ? names[channel] : `Output ${channel + 1}`;
    return {
      text: `Out ${number(output.left)}${output.right >= 0 ? "+" + number(output.right) : ""}`,
      title: `${output.name} plays on ${name(output.left)}${output.right >= 0 ? " + " + name(output.right) : ""}. Song Setup → Audio I/O Setup → Outputs changes it.`
    };
  }

  /** A channel's output choice: Main, then the Sub outputs; a track's, the buses too (buses
      don't play into buses, nor FX channels into either). */
  function showOutput(select, track) {
    const outputs = Hub.state.outputs || [];
    const buses = track.kind === "fx" || track.kind === "bus" ? [] : Hub.state.busChannels || [];
    const signature = outputs.map((output) => `${output.id}:${output.name}`).join("|") + "#" + buses.map((bus) => `${bus.id}:${bus.name}`).join("|");
    if (select.dataset.signature !== signature) {
      const options = outputs.map((output) => new Option(output.name, String(output.id)));
      if (buses.length) {
        const group = document.createElement("optgroup");
        group.label = "Buses";
        group.append(...buses.map((bus) => new Option(`→ ${bus.name}`, `bus:${bus.id}`)));
        options.push(group);
      }
      select.replaceChildren(...options);
      select.dataset.signature = signature;
    }
    const onBus = track.bus && buses.some((bus) => bus.id === track.bus);
    if (document.activeElement !== select) select.value = onBus ? `bus:${track.bus}` : String(track.output || 0);
    if (select.selectedIndex < 0) select.value = "0";
    select.classList.toggle("sub", !onBus && (track.output || 0) !== 0);
    select.classList.toggle("to-bus", Boolean(onBus));
  }

  /** Main and each Sub output show where they play; a click opens Song Setup there. */
  function routeButton(button) {
    button.addEventListener("click", (event) => {
      event.stopPropagation();
      Hub.songSetup.open("io", "outputs");
    });
  }

  function showRoute(button, output) {
    const { text, title } = channelsText(output);
    button.textContent = text;
    button.title = title;
  }

  // A strip for each Sub output, left of Main: its meter, fader and mute.
  const subs = new Map();             // output id -> strip

  function createSubStrip(output) {
    const root = element("div", "strip main-strip sub-strip");
    root.dataset.output = String(output.id);
    root.innerHTML = `
      <div class="strip-io"><button class="strip-route" type="button"></button></div>
      <div class="strip-inserts sub-spacer"></div>
      <div class="main-peaks" title="Highest peak since the last reset. Click to reset.">
        <output class="peak"></output><output class="peak"></output>
      </div>
      <div class="strip-buttons">
        <button class="tbtn" data-action="mute" aria-pressed="false" title="Mute this output">M</button>
      </div>
      <output class="strip-db"></output>
      <div class="strip-body">
        <div class="fader" role="slider" tabindex="0" aria-label="Output volume" aria-valuemin="-72" aria-valuemax="10"><div class="fader-cap"></div></div>
        ${meterMarkup()}
        ${scaleMarkup()}
      </div>
      <div class="strip-number"><span>Sub</span></div>
      <div class="strip-name"></div>`;

    const id = output.id;
    const current = () => (Hub.state.outputs || []).find((o) => o.id === id);
    const strip = {
      root,
      fader: root.querySelector(".fader"),
      readout: root.querySelector(".strip-db"),
      meter: new Meter(root.querySelector(".vmeter")),
      peaks: [...root.querySelectorAll(".main-peaks .peak")],
      highest: [FLOOR_DB, FLOOR_DB]
    };

    makeFader(strip.fader, () => (current() ? current().volume || 0 : 0), (db) => {
      showFader(strip.fader, strip.readout, db);
      const out = current();
      if (out) out.volume = db;
      Hub.send("outputSettings", { id, volume: db });
    });

    root.querySelector('[data-action="mute"]').addEventListener("click", (event) => {
      const on = event.currentTarget.getAttribute("aria-pressed") !== "true";
      event.currentTarget.setAttribute("aria-pressed", String(on));
      const out = current();
      if (out) out.mute = on;
      Hub.send("outputSettings", { id, mute: on });
    });

    root.querySelector(".main-peaks").addEventListener("click", (event) => {
      event.stopPropagation();
      strip.highest = [FLOOR_DB, FLOOR_DB];
      strip.meter.resetClip();
      showStripPeaks(strip);
    });

    routeButton(root.querySelector(".strip-route"));
    showStripPeaks(strip);
    return strip;
  }

  function showStripPeaks(strip) {
    strip.highest.forEach((db, index) => {
      strip.peaks[index].textContent = db <= Hub.SILENT_DB ? "-∞" : db.toFixed(1);
      strip.peaks[index].classList.toggle("over", db >= 0);
    });
  }

  function renderSubStrips() {
    const box = $("console-outputs");
    const outputs = (Hub.state.outputs || []).slice(1);
    const alive = new Set();

    outputs.forEach((output, index) => {
      let strip = subs.get(output.id);
      if (!strip) {
        strip = createSubStrip(output);
        subs.set(output.id, strip);
      }
      alive.add(output.id);
      strip.root.querySelector(".strip-name").textContent = output.name;
      strip.root.querySelector('[data-action="mute"]').setAttribute("aria-pressed", String(Boolean(output.mute)));
      if (!strip.fader.classList.contains("active")) showFader(strip.fader, strip.readout, output.volume || 0);
      showRoute(strip.root.querySelector(".strip-route"), output);
      if (box.children[index] !== strip.root) box.insertBefore(strip.root, box.children[index] || null);
    });

    for (const [id, strip] of subs) {
      if (!alive.has(id)) {
        strip.root.remove();
        subs.delete(id);
      }
    }

    box.hidden = outputs.length === 0;
  }

  /** Fader and pan, unless the user is working that control right now. */
  function showValues(strip, track) {
    if (!strip.fader.classList.contains("active")) showFader(strip.fader, strip.readout, track.volume);
    if (document.activeElement !== strip.pan) strip.pan.value = String(track.pan);
    strip.pan.nextElementSibling.textContent = Hub.panText(track.pan);
  }

  function renderStrips() {
    const list = $("console-strips");
    const alive = new Set();

    [...Hub.state.tracks, ...(Hub.state.busChannels || []), ...(Hub.state.fxChannels || [])].forEach((track, index) => {
      let strip = strips.get(track.id);

      if (!strip) {
        strip = createStrip(track);
        strips.set(track.id, strip);
      }

      updateStrip(strip, track);
      alive.add(track.id);
      if (list.children[index] !== strip.root) list.insertBefore(strip.root, list.children[index] || null);
    });

    for (const [id, strip] of strips) {
      if (!alive.has(id)) {
        strip.root.remove();
        strips.delete(id);
      }
    }

    $("console-empty").hidden = Hub.state.tracks.length + (Hub.state.busChannels || []).length + (Hub.state.fxChannels || []).length > 0;
  }

  // ---------------------------------------------------------------- Main
  function createMain() {
    const root = $("console-main");
    root.innerHTML = `
      <div class="strip main-strip">
        <div class="strip-io"><button class="strip-route" type="button"></button></div>
        ${insertsMarkup()}
        <div class="main-peaks" title="Highest peak since the last reset. Click to reset.">
          <output class="peak"></output><output class="peak"></output>
        </div>
        <div class="strip-buttons">
          <button class="tbtn" data-action="mute" aria-pressed="false" title="Mute Main (the metronome keeps playing)">M</button>
          <button class="tbtn master-button" data-action="master" title="Mastering Assistant" aria-label="Mastering Assistant">${ICON_MASTER}</button>
          <button class="tbtn mastering-button" data-action="mastering" data-mastering-button aria-pressed="false"
            title="Mastering section: the last thing on Main, after its fader (lit while it's in)" aria-label="Mastering section">MS</button>
        </div>
        <output class="strip-db"></output>
        <div class="strip-body">
          <div class="fader" role="slider" tabindex="0" aria-label="Main volume" aria-valuemin="-72" aria-valuemax="10"><div class="fader-cap"></div></div>
          ${meterMarkup()}
          ${scaleMarkup()}
        </div>
        <div class="strip-number"><span>Out</span><button class="strip-expand" type="button" aria-pressed="false"
          title="Show inserts" aria-label="Show inserts">${ICON_EXPAND}</button></div>
        <div class="strip-name">Main</div>
        <div class="strip-panel">
          <div class="panel-inserts">
            <div class="panel-head"><span>Inserts</span><button class="panel-add" type="button" title="Add an effect (or drop one from the Browser)" aria-label="Add an effect">+</button></div>
            <div class="panel-list panel-insert-list"></div>
          </div>
        </div>
      </div>`;

    const strip = {
      root: root.firstElementChild,
      fader: root.querySelector(".fader"),
      readout: root.querySelector(".strip-db"),
      meter: new Meter(root.querySelector(".vmeter")),
      peaks: [...root.querySelectorAll(".main-peaks .peak")],
      highest: [FLOOR_DB, FLOOR_DB]
    };

    makeFader(strip.fader, () => Hub.state.master.volume, (db) => {
      showFader(strip.fader, strip.readout, db);
      Hub.setMasterValue("volume", db, strip.fader);
    });

    strip.root.querySelector('[data-action="mute"]').addEventListener("click", (event) => {
      const on = event.currentTarget.getAttribute("aria-pressed") !== "true";
      event.currentTarget.setAttribute("aria-pressed", String(on));
      Hub.setMasterValue("mute", on, event.currentTarget);
    });

    strip.root.querySelector('[data-action="master"]').addEventListener("click", () => Hub.run("masteringAssistant"));
    strip.root.querySelector('[data-action="mastering"]').addEventListener("click", () => Hub.run("masteringSection"));
    strip.root.querySelector('[data-action="mastering"]').setAttribute("aria-pressed", String(Boolean(Hub.state.masteringOn)));

    strip.root.querySelector(".main-peaks").addEventListener("click", (event) => {
      event.stopPropagation();
      strip.highest = [FLOOR_DB, FLOOR_DB];
      strip.meter.resetClip();
      showPeaks();
    });

    // Clicking Main shows its inserts in the Inspector.
    strip.root.addEventListener("click", (event) => {
      if (!event.target.closest("button, .fader, .clip, .strip-inserts, .main-peaks, .strip-panel")) Hub.inspectMain();
    });

    setupInserts(strip.root.querySelector(".strip-inserts"), Hub.MAIN);
    setupPanelInserts(strip.root, Hub.MAIN);
    setupStripDrop(strip.root, Hub.MAIN);
    routeButton(strip.root.querySelector(".strip-route"));
    return strip;
  }

  function showPeaks() {
    main.highest.forEach((db, index) => {
      main.peaks[index].textContent = db <= Hub.SILENT_DB ? "-∞" : db.toFixed(1);
      main.peaks[index].classList.toggle("over", db >= 0);
    });
  }

  function renderMain() {
    const master = Hub.state.master;
    main.root.classList.toggle("selected", Hub.state.inspectMain);
    showInserts(main.root.querySelector(".strip-inserts"), master.inserts || [], Hub.MAIN, master.kept || []);
    main.root.classList.toggle("expanded", expanded.has(Hub.MAIN));
    main.root.querySelector(".strip-expand").setAttribute("aria-pressed", String(expanded.has(Hub.MAIN)));
    showPanelInserts(main.root.querySelector(".panel-insert-list"), master.inserts || [], Hub.MAIN);
    main.root.querySelector('[data-action="mute"]').setAttribute("aria-pressed", String(Boolean(master.mute)));
    if (!main.fader.classList.contains("active")) showFader(main.fader, main.readout, master.volume);
    showRoute(main.root.querySelector(".strip-route"), (Hub.state.outputs || [])[0] || { name: "Main", left: 0, right: 1 });
  }

  // ---------------------------------------------------------------- animation
  let lastFrame = 0;

  function animate(now) {
    const seconds = lastFrame ? Math.min(0.1, (now - lastFrame) / 1000) : 0;
    lastFrame = now;

    if (!document.body.classList.contains("hide-console")) {
      for (const strip of strips.values()) strip.meter.tick(now, seconds);
      for (const strip of subs.values()) strip.meter.tick(now, seconds);
      main.meter.tick(now, seconds);
    }

    requestAnimationFrame(animate);
  }

  // ---------------------------------------------------------------- the Analog Board mixing
  // While the board mixes the song, the Console stays as it was set (dimmed): its faders, inserts
  // and sends aren't heard until it mixes again.
  let boardNote = null;

  function renderBoardNote() {
    const mixing = Boolean(Hub.state.boardMixing);
    $("console").classList.toggle("board-mixing", mixing);
    if (!mixing) {
      if (boardNote) boardNote.hidden = true;
      return;
    }
    if (!boardNote) {
      boardNote = element("div", "console-board-note");
      boardNote.setAttribute("role", "status");
      const words = element("span", "", "The Analog Board mixes this song: the Console isn't heard.");
      const open = element("button", "plain-button small", "Open the board");
      open.type = "button";
      open.addEventListener("click", () => Hub.run("analogBoard"));
      const back = element("button", "plain-button small", "Mix on the Console");
      back.type = "button";
      back.addEventListener("click", () => Hub.send("board", { action: "mixing", on: false }));
      boardNote.append(words, open, back);
      $("console").appendChild(boardNote);
    }
    boardNote.hidden = false;
  }

  // ---------------------------------------------------------------- events
  // Hidden, the Console isn't drawn: it catches up as it's shown.
  let consoleDue = false;

  function renderConsole() {
    consoleDue = false;
    renderStrips();
    renderSubStrips();
    renderMain();
    renderBoardNote();
  }

  Hub.on("tracks", () => {
    if (document.body.classList.contains("hide-console")) { consoleDue = true; return; }
    renderConsole();
  });

  new MutationObserver(() => {
    if (consoleDue && !document.body.classList.contains("hide-console")) renderConsole();
  }).observe(document.body, { attributes: true, attributeFilter: ["class"] });

  Hub.on("selection", () => {
    for (const [id, strip] of strips) strip.root.classList.toggle("selected", Hub.isTrackSelected(id));
    main.root.classList.toggle("selected", Hub.state.inspectMain);
  });

  // A fader moved somewhere else (a track header): show it here too.
  Hub.on("trackValue", ({ id, source }) => {
    const strip = strips.get(id);
    const track = Hub.trackById(id);
    if (strip && track && !strip.root.contains(source)) showValues(strip, track);
  });

  Hub.on("status", (s) => {
    for (const meter of s.trackMeters || []) {
      const strip = strips.get(meter.id);
      if (strip) strip.meter.feed(meter.left, meter.right);

      // Automation, or a control surface, moves the faders here too.
      const track = strip && meter.volume !== undefined ? Hub.trackById(meter.id) : null;
      if (track) {
        track.volume = meter.volume;
        track.pan = meter.pan;
        showValues(strip, track);
      }
    }

    main.meter.feed(s.peakLeft, s.peakRight);

    // Main's fader moved on a control surface.
    if (typeof s.masterVolume === "number" && !main.fader.classList.contains("active")) showFader(main.fader, main.readout, s.masterVolume);

    const left = Hub.gainToDb(s.peakLeft || 0), right = Hub.gainToDb(s.peakRight || 0);
    if (left > main.highest[0] || right > main.highest[1]) {
      main.highest = [Math.max(main.highest[0], left), Math.max(main.highest[1], right)];
      showPeaks();
    }

    // The Sub outputs.
    for (const meter of s.outputMeters || []) {
      const strip = subs.get(meter.id);
      if (!strip) continue;
      strip.meter.feed(meter.left, meter.right);
      const subLeft = Hub.gainToDb(meter.left || 0), subRight = Hub.gainToDb(meter.right || 0);
      if (subLeft > strip.highest[0] || subRight > strip.highest[1]) {
        strip.highest = [Math.max(strip.highest[0], subLeft), Math.max(strip.highest[1], subRight)];
        showStripPeaks(strip);
      }
    }
  });

  Hub.commands.resetClips = () => {
    for (const strip of strips.values()) strip.meter.resetClip();
    for (const strip of subs.values()) strip.meter.resetClip();
    main.meter.resetClip();
  };

  // ---------------------------------------------------------------- the Console's own menu
  // A right-click where there's no channel: a bus or an FX channel, as in Studio One's Console.
  function setupConsoleMenu() {
    $("console-strips").addEventListener("contextmenu", (event) => {
      if (event.target.closest(".strip, input, select")) return;
      event.preventDefault();
      const tracks = Hub.state.selectedTracks.filter((id) => {
        const channel = Hub.trackById(id);
        return channel && channel.kind !== "fx" && channel.kind !== "bus";
      });
      Hub.popupMenu(Hub.pointAnchor(event), [
        { label: "Add Bus Channel", value: "bus" },
        { label: "Add FX Channel", value: "fx" },
        { separator: true },
        { label: tracks.length > 1 ? `Add Bus for ${tracks.length} Selected Channels` : "Add Bus for Selected Channels", value: "busSelected", disabled: !tracks.length }
      ], (pick) => {
        if (pick === "bus") Hub.send("addBusChannel", { ids: [] });
        else if (pick === "fx") Hub.send("addFxChannel", {});
        else if (pick === "busSelected") Hub.send("addBusChannel", { ids: tracks });
      });
    });
  }

  Hub.on("ready", () => {
    setupConsoleMenu();
    main = createMain();
    showPeaks();
    renderStrips();
    renderSubStrips();
    renderMain();
    requestAnimationFrame(animate);
  });
})();
