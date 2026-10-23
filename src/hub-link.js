// D:\workspace\Fanan_web_hub\src\hub-link.js
//
// MIDI Link, at the right of the toolbar, as in Grex. The strip is two things:
//   MIDI Link        its title: opens the MIDI Link window (below)
//   the last control what a MIDI device moved last (a knob, a slider, a wheel, a pad or a key:
//                    "CC 74", "Note 36 (C2)"), held there until something else moves
// Drag the last control onto one of Sonify's controls (or tap it, then click the control) and the
// device moves that control from then on. A small window opens by the control:
//   type             Knob, Toggle, Trigger or Momentary (below)
//   Clear            the control is linked to nothing (this link and any other)
//   Cancel           the links as they were before the drop
//   ×                done
// The MIDI Link window lists every link: what moves it, from which device, what it moves (click to
// find the control on the page; pointing at the row outlines it), its type, and a bin to remove
// it. It also holds the Plugin Knob slot, saving and loading maps, and Clear All.
// The types:
//   Knob        follows the knob, slider or wheel (a button: on past the middle)
//   Toggle      each press switches it on or off (a fader: all the way up, then down)
//   Trigger     each press fires it once (a button: on; a fader: all the way up)
//   Momentary   1 while pressed or dragged, 0 when let go
// Linkable: the tracks' and channels' faders, pans, sends, Mute, Solo, Record and Monitor, Main's
// fader and Mute, the transport's buttons, every knob, fader and button of the Analog Board, and
// the Live Show's (its decks' knobs, faders, Play, Cue, Sync, hot cues and loops, its channels,
// its mixer, its pads, Enable streaming, Mute mics, Mute playback and REC), and Song Blocks' Live
// (its cells, blocks' names, stops and Stop All: Trigger, each press launches). A plugin's own knobs
// go through the window's Plugin Knob slot: touch the knob in the plugin's window, then drop the
// last control on the slot.
// The links are the setup's, not the song's: the same in every song, track by number ("Track 2's
// fader"). Right-click the strip for the map's menu too.
// Focus (the MIDI Link window): Volume, Pan, Mute, Solo, Record and eight Knobs that move the
// selected track's own controls, whichever track is selected (its Knobs: its instrument's first
// eight knobs, or an effect track's first insert's); link them like any control.
// Learn (MIDI Learn, the strip's button): the other way round. With Learn on, every control that
// can be linked is outlined; click one (it blinks), then move a knob, slider or wheel or press a pad
// or key on the device, and it's linked (its small window opens, for its type). Click the next
// control, and so on; Learn again (or Esc) stops.

(() => {
  "use strict";

  const { $, element } = Hub;
  let state = { recent: [], links: [], touched: null };
  let armed = "";              // the last control tapped: the next control clicked is linked to it
  let press = null;            // the last control pressed: {key, x, y, pointer, ghost}
  let popup = null;            // {key, target, box, opened}
  let renderDue = false;       // the strip changed while the last control was held: drawn once it's let go
  let swallowUntil = 0;        // the click right after a press that linked a control does nothing else
  let learnMode = false;       // MIDI Learn: a click on a control makes it wait for the device
  let waiting = null;          // Learn: the control waiting {target, element}

  const TRANSPORT = { play: "Play", stop: "Stop", record: "Record", loop: "Loop", metronome: "Metronome", start: "Return to Zero" };
  const TRANSPORT_IDS = { play: "play", stop: "stop", record: "record", loop: "loop", metronome: "metronome", "to-start": "start" };
  const CONTROLS = { volume: "Volume", pan: "Pan", mute: "Mute", solo: "Solo", arm: "Record", monitor: "Monitor" };
  const MODES = [
    ["knob", "Knob", "Follows the knob, slider or wheel (a button: on past the middle)"],
    ["toggle", "Toggle", "Each press switches it on or off (a fader: all the way up, then down)"],
    ["trigger", "Trigger", "Each press fires it once (a button: on; a fader: all the way up)"],
    ["push", "Momentary", "1 while pressed or dragged, 0 when let go"]
  ];
  const BIN = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M6 1.5h4l.5 1H14V4H2V2.5h3.5zM3 5h10l-.8 9.2a1 1 0 0 1-1 .8H4.8a1 1 0 0 1-1-.8zm3 1.8v6h1.2v-6zm2.8 0v6H10v-6z"/></svg>';

  // What can be linked, for the highlight while the last control is dragged or tapped.
  const CANDIDATES = [
    '#track-rows .track [data-control="volume"]', '#track-rows .track [data-control="pan"]', "#track-rows .track [data-action]",
    "#console-strips .strip .fader", '#console-strips .strip [data-control="pan"]', "#console-strips .strip [data-action]",
    "#console-strips .panel-send", "#console-main .fader", '#console-main [data-action="mute"]',
    "#play", "#stop", "#record", "#loop", "#metronome", "#to-start", "#link-touched", "#analog-board [data-board-param]",
    "#live-studio [data-live-link]", "#song-blocks [data-launch-link]", "#link-window [data-focus-link]"
  ].join(", ");

  // ---------------------------------------------------------------- names
  const sourceOf = (key) => state.recent.find((chip) => chip.key === key) || state.links.find((link) => link.source === key) || null;
  const linkOf = (key) => state.links.find((link) => link.source === key) || null;
  const targetId = (target) => target.key === "plugin" ? `plugin|${target.pluginId}|${target.paramId}` : target.key;
  const modeLabel = (mode) => (MODES.find(([value]) => value === mode) || MODES[0])[1];

  /** "CC 74 · Ch 1 · Keystation". */
  function sourceText(source) {
    if (!source) return "";
    return `${source.label} · Ch ${source.channel} · ${source.device}`;
  }

  /** A target as the page shows it: "Track 2 (Piano) · Volume", "Main · Volume", "Pro-Q 3 · Gain". */
  function targetText(target) {
    if (!target) return "";
    if (target.key === "plugin") return `${target.pluginName || "Plugin"} · ${target.paramName || "Parameter"}`;
    const parts = target.key.split(".");
    if (parts[0] === "board") return Hub.board ? Hub.board.targetText(target.key) : target.key;
    if (parts[0] === "live") return Hub.live ? Hub.live.targetText(target.key) : target.key;
    if (parts[0] === "launch") return Hub.blocks && Hub.blocks.targetText ? Hub.blocks.targetText(target.key) : target.key;
    if (parts[0] === "focus") {
      if (parts[1] === "knob") { const knob = focusKnobs()[Number(parts[2]) - 1]; return `Focus \u00b7 Knob ${parts[2]}${knob ? ` (${knob.name})` : ""}`; }
      if (parts[1] === "send") return `Focus \u00b7 Send ${parts[2]}`;
      return `Focus \u00b7 ${CONTROLS[parts[1]] || parts[1]}`;
    }
    if (parts[0] === "transport") return `Transport · ${TRANSPORT[parts[1]] || parts[1]}`;
    if (parts[0] === "main") return `Main · ${CONTROLS[parts[1]] || parts[1]}`;

    const number = Number(parts[1]);
    const channel = parts[0] === "fx" ? (Hub.state.fxChannels || [])[number - 1]
                  : parts[0] === "bus" ? (Hub.state.busChannels || [])[number - 1]
                  : Hub.state.tracks.find((track) => track.number === number);
    const who = `${parts[0] === "fx" ? "FX" : parts[0] === "bus" ? "Bus" : "Track"} ${number}${channel ? ` (${channel.name})` : ""}`;
    if (parts[2] === "send") {
      const send = channel && (channel.sends || [])[Number(parts[3]) - 1];
      return `${who} · Send ${parts[3]}${send ? ` to ${send.name}` : ""}`;
    }
    return `${who} · ${CONTROLS[parts[2]] || parts[2]}`;
  }

  // ---------------------------------------------------------------- Focus
  /** The selected track's knobs Focus moves: its instrument's (a plugin's own: none hidden or
      read-only), or its first insert's that has some; the first eight. */
  function focusKnobs() {
    const track = Hub.selectedTrack ? Hub.selectedTrack() : Hub.trackById(Hub.state.selected);
    if (!track) return [];
    const knobsOf = (plugin) => (plugin && plugin.params ? plugin.params.filter((p) => !p.readOnly) : []);
    let knobs = knobsOf(track.instrument);
    if (!knobs.length) for (const insert of track.inserts || []) { knobs = knobsOf(insert); if (knobs.length) break; }
    return knobs.slice(0, 8);
  }

  function renderFocus() {
    const track = Hub.trackById(Hub.state.selected);
    $("link-focus-track").textContent = track ? track.name : "No track selected";
    const knobs = focusKnobs();
    const box = $("link-focus-knobs");
    if (box.childElementCount !== 8) {
      box.textContent = "";
      for (let k = 1; k <= 8; k++) {
        const slot = element("button", "link-focus-slot");
        slot.type = "button";
        slot.dataset.focusLink = `focus.knob.${k}`;
        slot.dataset.continuous = "1";
        box.appendChild(slot);
      }
    }
    [...box.children].forEach((slot, i) => {
      const knob = knobs[i];
      slot.textContent = knob ? knob.name : `Knob ${i + 1}`;
      slot.title = `Knob ${i + 1}: ${knob ? `${knob.name}, ` : ""}the selected track's ${i + 1}${["st", "nd", "rd"][i] || "th"} knob (its instrument's, or its first effect's). Drop a MIDI control here, or Learn it.`;
    });
    for (const slot of $("link-focus").querySelectorAll("[data-focus-link]")) {
      slot.classList.toggle("linked", state.links.some((link) => link.target.key === slot.dataset.focusLink));
      if (!slot.title || !slot.dataset.focusLink.startsWith("focus.knob"))
        slot.title = `The selected track's ${slot.textContent}, whichever track is selected. Drop a MIDI control here, or Learn it.`;
    }
  }

  // ---------------------------------------------------------------- targets
  /** The control an element is part of, as MIDI Link names it: {target, continuous, element}, or null. */
  function targetAt(el) {
    if (!el || !el.closest) return null;

    // Focus: the selected track's own controls.
    const focus = el.closest("#link-window [data-focus-link]");
    if (focus) return { target: { key: focus.dataset.focusLink }, continuous: focus.dataset.continuous === "1", element: focus };

    const touched = el.closest("#link-touched");
    if (touched) {
      const t = state.touched;
      if (!t) return null;
      return { target: { key: "plugin", pluginId: t.pluginId, pluginName: t.pluginName, paramId: t.paramId, paramName: t.paramName },
               continuous: true, element: touched };
    }

    // The Analog Board's knobs, faders and buttons: "board.<n>.<setting>", "board.master.<setting>".
    const board = el.closest("#analog-board [data-board-param]");
    if (board) {
      if (!board.dataset.boardStrip) return null;
      const key = `board.${board.dataset.boardStrip}.${board.dataset.boardParam}`;
      return { target: { key }, continuous: Hub.board ? Hub.board.isContinuous(key) : board.dataset.continuous === "1", element: board };
    }

    // The Live Show's: "live.deck.<n>.<setting or action>", "live.master.<setting>", "live.pad.<n>"...
    const live = el.closest("#live-studio [data-live-link]");
    if (live) return { target: { key: live.dataset.liveLink }, continuous: live.dataset.continuous === "1", element: live };

    // Song Blocks' Live: "launch.cell.<row>.<column>", "launch.row.<row>", "launch.stop.<column>", "launch.stopall".
    const launch = el.closest("#song-blocks [data-launch-link]");
    if (launch) return { target: { key: launch.dataset.launchLink }, continuous: false, trigger: true, element: launch };

    const button = el.closest("#play, #stop, #record, #loop, #metronome, #to-start");
    if (button) return { target: { key: `transport.${TRANSPORT_IDS[button.id]}` }, continuous: false, element: button };

    const main = el.closest("#console-main .main-strip");
    if (main) {
      const fader = el.closest(".fader");
      if (fader) return { target: { key: "main.volume" }, continuous: true, element: fader };
      const mute = el.closest('[data-action="mute"]');
      if (mute) return { target: { key: "main.mute" }, continuous: false, element: mute };
      return null;
    }

    const holder = el.closest("#track-rows .track[data-id], #console-strips .strip[data-id]");
    const track = holder ? Hub.trackById(Number(holder.dataset.id)) : null;
    if (!track) return null;

    const fx = track.kind === "fx" || track.kind === "bus";
    const list = track.kind === "fx" ? Hub.state.fxChannels || [] : Hub.state.busChannels || [];
    const number = fx ? list.findIndex((channel) => channel.id === track.id) + 1 : track.number;
    if (!number) return null;
    const prefix = `${fx ? track.kind : "track"}.${number}`;

    const send = el.closest(".panel-send[data-send-id]");
    if (send) {
      const position = [...send.parentElement.querySelectorAll(".panel-send[data-send-id]")].indexOf(send) + 1;
      return { target: { key: `${prefix}.send.${position}` }, continuous: true, element: send };
    }

    const volume = el.closest('.fader, [data-control="volume"]');
    if (volume && holder.contains(volume)) return { target: { key: `${prefix}.volume` }, continuous: true, element: volume };

    const pan = el.closest('[data-control="pan"]');
    if (pan) return { target: { key: `${prefix}.pan` }, continuous: true, element: pan };

    const action = el.closest("[data-action]");
    if (action) {
      const control = { mute: "mute", solo: "solo", recordArm: "arm", monitor: "monitor" }[action.dataset.action];
      // FX channels and buses have no recording and no monitoring.
      if (!control || (fx && (control === "arm" || control === "monitor"))) return null;
      return { target: { key: `${prefix}.${control}` }, continuous: false, element: action };
    }

    return null;
  }

  /** The control on the page a link moves (shown now), or null: a plugin's knob is in its own window. */
  function elementOf(target) {
    if (target.key === "plugin") return null;
    const id = targetId(target);
    for (const el of document.querySelectorAll(CANDIDATES)) {
      if (el.closest("#link-window") || !el.getClientRects().length) continue;
      const found = targetAt(el);
      if (found && targetId(found.target) === id) return found.element;
    }
    return null;
  }

  function markTargets(on) {
    document.body.classList.toggle("linking", on);
    document.querySelectorAll(".link-target, .link-hover").forEach((el) => el.classList.remove("link-target", "link-hover"));
    if (!on) return;
    for (const el of document.querySelectorAll(CANDIDATES)) {
      const found = targetAt(el);
      if (found) found.element.classList.add("link-target");
    }
  }

  // ---------------------------------------------------------------- linking
  /** Links the source to the control, and opens the window by it. */
  function linkTo(key, found) {
    const source = sourceOf(key);
    if (!source || !found) return;

    // As the control suits: Knob for a fader (Momentary from a key or pad), Toggle for a button.
    // The clip launcher's: Trigger, each press launches.
    const mode = found.trigger ? "trigger" : found.continuous ? (source.kind === "note" ? "push" : "knob") : "toggle";
    Hub.send("midiLinkSet", { source: key, target: found.target, mode });
    openPopup(key, found.target, found.element, mode);
  }

  // ---------------------------------------------------------------- MIDI Learn
  function setLearn(on) {
    if (on === learnMode) return;
    learnMode = on;
    $("link-learn").setAttribute("aria-pressed", String(on));
    if (on) {
      disarm();
      cancelPick();
      closePopup();
      markTargets(true);
      Hub.toast("Learn: click a control, then move what should move it on your device.");
      return;
    }
    if (waiting) {
      waiting.element.classList.remove("link-waiting");
      waiting = null;
      Hub.send("midiLinkLearn", {});
    }
    markTargets(false);
  }

  /** Learn on: a press on a control makes it wait for the device (and does nothing else). */
  function learnPress(event) {
    if (!learnMode || event.button !== 0) return;
    if (event.target.closest("#midi-link, .link-popup")) return;
    if (event.target.closest("#link-window") && !event.target.closest("[data-focus-link], #link-touched")) return;
    const found = targetAt(event.target);
    if (!found) return;
    event.preventDefault();
    event.stopImmediatePropagation();
    swallowUntil = performance.now() + 600;
    closePopup();
    if (waiting) waiting.element.classList.remove("link-waiting");
    waiting = found;
    if (!document.body.classList.contains("linking")) markTargets(true);
    found.element.classList.add("link-waiting");
    Hub.send("midiLinkLearn", { target: found.target });
  }

  function disarm() {
    if (!armed) return;
    armed = "";
    render();
    markTargets(false);
  }

  // ---------------------------------------------------------------- the window by the control
  function modeButtons(mode) {
    const modes = element("div", "link-modes");
    for (const [value, label, tip] of MODES) {
      const button = element("button", "plain-button small", label);
      button.type = "button";
      button.dataset.mode = value;
      button.title = tip;
      button.setAttribute("aria-pressed", String(value === mode));
      modes.appendChild(button);
    }
    return modes;
  }

  function openPopup(key, target, near, mode) {
    closePopup();
    const box = element("div", "link-popup");
    box.setAttribute("role", "dialog");
    box.setAttribute("aria-label", "MIDI Link");

    const close = element("button", "link-x", "×");
    close.type = "button";
    close.title = "Done";
    close.setAttribute("aria-label", "Done");
    const title = element("span", "link-popup-title", "MIDI Link");
    const head = element("div", "link-popup-head");
    head.append(close, title);

    const from = element("div", "link-from", sourceText(sourceOf(key)));
    const to = element("div", "link-to", `→ ${targetText(target)}`);
    const modes = modeButtons(mode);

    const actions = element("div", "link-actions");
    const clear = element("button", "plain-button small", "Clear");
    clear.type = "button";
    clear.title = "Link this control to nothing";
    const cancel = element("button", "plain-button small", "Cancel");
    cancel.type = "button";
    cancel.title = "The links as they were before";
    actions.append(clear, cancel);

    box.append(head, from, to, modes, actions);
    document.body.appendChild(box);

    // By the control: under it, or over it when there's no room.
    const r = near.getBoundingClientRect();
    const width = box.offsetWidth, height = box.offsetHeight;
    box.style.left = Math.max(4, Math.min(window.innerWidth - width - 4, r.left + r.width / 2 - width / 2)) + "px";
    box.style.top = (r.bottom + height + 8 > window.innerHeight ? Math.max(4, r.top - height - 6) : r.bottom + 6) + "px";

    close.addEventListener("click", closePopup);
    clear.addEventListener("click", () => { Hub.send("midiLinkClear", { target: targetId(target) }); closePopup(); });
    cancel.addEventListener("click", () => { Hub.send("midiLinkCancel"); closePopup(); });
    modes.addEventListener("click", (event) => {
      const button = event.target.closest("[data-mode]");
      if (!button) return;
      modes.querySelectorAll("[data-mode]").forEach((b) => b.setAttribute("aria-pressed", String(b === button)));
      Hub.send("midiLinkMode", { source: key, mode: button.dataset.mode });
    });

    popup = { key, target, box, opened: performance.now() };
  }

  function closePopup() {
    if (!popup) return;
    if (popup.box.contains(document.activeElement)) document.activeElement.blur();
    popup.box.remove();
    popup = null;
  }

  // ---------------------------------------------------------------- the MIDI Link window
  // Open until closed (×, Esc or the strip's title again), so a plugin's knob can be touched in
  // its own window and the last control dropped on the Plugin Knob slot.
  const windowOpen = () => !$("link-window").hidden;

  function placeWindow() {
    const box = $("link-window");
    const bar = $("toolbar").getBoundingClientRect();
    const strip = $("midi-link").getBoundingClientRect();
    const width = Math.min(box.offsetWidth || 640, window.innerWidth - 8);
    box.style.top = bar.bottom + 4 + "px";
    box.style.left = Math.max(4, Math.min(window.innerWidth - width - 4, strip.right - width)) + "px";
  }

  function openWindow(open) {
    const box = $("link-window");
    box.hidden = !open;
    $("link-open").setAttribute("aria-expanded", String(open));
    $("link-open").classList.toggle("open", open);
    if (!open) {
      if (box.contains(document.activeElement)) document.activeElement.blur();
      document.querySelectorAll(".link-found").forEach((el) => el.classList.remove("link-found"));
      return;
    }
    renderWindow();
    placeWindow();
  }

  function renderWindow() {
    const touched = state.touched;
    $("link-touched-name").textContent = touched ? `${touched.pluginName} · ${touched.paramName}` : "Touch a knob in a plugin's window";
    $("link-touched").classList.toggle("empty", !touched);
    $("link-touched").title = touched
      ? `${touched.paramName} of ${touched.pluginName} (${touched.where}): drop the MIDI control here to link it.`
      : "Take hold of a knob in a plugin's window and it shows here; then drop the MIDI control here to link it.";

    renderFocus();
    $("link-count").textContent = state.links.length ? `${state.links.length} link${state.links.length === 1 ? "" : "s"}` : "";
    $("link-empty").hidden = state.links.length > 0;
    $("link-table").hidden = state.links.length === 0;
    $("link-clear-all").disabled = state.links.length === 0;

    const rows = $("link-rows");
    rows.textContent = "";
    for (const link of state.links) {
      const row = element("tr", "link-row" + (state.recent[0] && state.recent[0].key === link.source ? " recent" : ""));
      row.dataset.source = link.source;

      const midi = element("td", "link-cell-midi", link.label);
      const from = element("td", "link-cell-from", `Ch ${link.channel} · ${link.device}`);
      from.title = link.device;

      const to = element("td", "link-cell-to");
      const find = element("button", "link-find", targetText(link.target));
      find.type = "button";
      find.dataset.find = "1";
      find.title = link.target.key === "plugin" ? "A plugin's knob: it's in the plugin's window" : "Show this control";
      to.appendChild(find);

      const type = element("td", "link-cell-type");
      const select = element("select", "link-type");
      select.setAttribute("aria-label", "Type");
      for (const [value, label, tip] of MODES) {
        const option = element("option", "", label);
        option.value = value;
        option.title = tip;
        select.appendChild(option);
      }
      select.value = link.mode;
      select.title = (MODES.find(([value]) => value === link.mode) || MODES[0])[2];
      type.appendChild(select);

      const bin = element("td", "link-cell-bin");
      const remove = element("button", "link-bin");
      remove.type = "button";
      remove.dataset.remove = "1";
      remove.title = "Remove this link";
      remove.setAttribute("aria-label", "Remove this link");
      remove.innerHTML = BIN;
      bin.appendChild(remove);

      row.append(midi, from, to, type, bin);
      rows.appendChild(row);
    }
  }

  function linkOfRow(el) {
    const row = el && el.closest ? el.closest(".link-row") : null;
    return row ? linkOf(row.dataset.source) : null;
  }

  function showFound(link, flash) {
    document.querySelectorAll(".link-found").forEach((el) => el.classList.remove("link-found"));
    const el = link ? elementOf(link.target) : null;
    if (!el) {
      if (flash && link) Hub.toast(link.target.key === "plugin" ? `${targetText(link.target)} is in the plugin's own window.`
                                                                : `${targetText(link.target)} isn't on screen now.`);
      return;
    }
    el.classList.add("link-found");
    if (flash) el.scrollIntoView({ block: "nearest", inline: "nearest" });
  }

  // ---------------------------------------------------------------- the strip
  function render() {
    // Not under the last control being held: it would go, and the drag with it.
    if (press) { renderDue = true; return; }
    renderDue = false;
    const box = $("link-last");
    box.textContent = "";

    const last = state.recent[0];
    if (last) {
      const button = element("button", "link-chip" + (last.link ? " linked" : "") + (last.key === armed ? " armed" : ""));
      button.type = "button";
      button.dataset.key = last.key;
      button.append(element("span", "link-chip-name", last.label), element("span", "link-chip-from", `Ch ${last.channel} · ${last.device}`));
      button.title = `${sourceText(last)}${last.link ? `\nLinked to ${targetText(last.link.target)} (${modeLabel(last.link.mode)})` : ""}\nDrag onto a control, or tap, then click the control.`;
      button.setAttribute("aria-pressed", String(last.key === armed));
      box.appendChild(button);
    } else {
      box.appendChild(element("span", "link-hint", "Move a control on your device"));
    }

    if (windowOpen()) renderWindow();
  }

  function startDrag(event) {
    const chip = event.target.closest(".link-chip");
    if (!chip || event.button !== 0) return;
    event.preventDefault();
    setLearn(false);   // the strip's control dragged or tapped: that way instead of Learn
    press = { key: chip.dataset.key, x: event.clientX, y: event.clientY, pointer: event.pointerId, ghost: null, chip };

    // Followed anywhere on the page, until it's let go.
    window.addEventListener("pointermove", moveDrag, true);
    window.addEventListener("pointerup", endDrag, true);
    window.addEventListener("pointercancel", endDrag, true);
  }

  function moveDrag(event) {
    if (!press || event.pointerId !== press.pointer) return;
    if (!press.ghost) {
      if (Math.hypot(event.clientX - press.x, event.clientY - press.y) < 5) return;
      disarm();
      closePopup();
      press.ghost = element("div", "link-ghost", sourceOf(press.key) ? sourceOf(press.key).label : "");
      document.body.appendChild(press.ghost);
      markTargets(true);
    }
    press.ghost.style.left = event.clientX + 10 + "px";
    press.ghost.style.top = event.clientY + 10 + "px";
    const found = targetAt(document.elementFromPoint(event.clientX, event.clientY));
    document.querySelectorAll(".link-hover").forEach((el) => el.classList.remove("link-hover"));
    if (found) found.element.classList.add("link-hover");
  }

  function endDrag(event) {
    if (!press || event.pointerId !== press.pointer) return;
    const done = press;
    press = null;
    window.removeEventListener("pointermove", moveDrag, true);
    window.removeEventListener("pointerup", endDrag, true);
    window.removeEventListener("pointercancel", endDrag, true);
    if (renderDue) render();

    // A tap: armed (or no longer), for touch screens and for anyone not dragging.
    if (!done.ghost) {
      armed = armed === done.key ? "" : done.key;
      closePopup();
      render();
      markTargets(Boolean(armed));
      return;
    }

    done.ghost.remove();
    markTargets(false);
    if (event.type === "pointerup") linkTo(done.key, targetAt(document.elementFromPoint(event.clientX, event.clientY)));
  }

  /** While the last control is armed, a press on a control links it instead of working the control. */
  function armedPress(event) {
    if (!armed || event.button !== 0 || event.target.closest("#midi-link .link-chip")) return;
    const found = targetAt(event.target);
    if (!found) {
      if (!event.target.closest(".link-popup")) disarm();
      return;
    }
    event.preventDefault();
    event.stopImmediatePropagation();
    swallowUntil = performance.now() + 600;
    const key = armed;
    disarm();
    linkTo(key, found);
  }

  // ---------------------------------------------------------------- picking a control
  // A device's output being mapped (hub-miditools.js): the next control clicked is what it moves.
  let picking = null;          // {done, banner}

  function pick(done, what) {
    cancelPick();
    disarm();
    closePopup();
    const banner = element("div", "pick-banner");
    banner.setAttribute("role", "status");
    banner.append(element("span", "", `Click what ${what || "it"} should move: a fader, a pan knob, a button, or the MIDI Link window's Plugin Knob slot for a plugin's knob.`));
    const cancel = element("button", "plain-button small", "Cancel");
    cancel.type = "button";
    cancel.addEventListener("click", cancelPick);
    banner.appendChild(cancel);
    document.body.appendChild(banner);
    picking = { done, banner };
    markTargets(true);
  }

  function cancelPick() {
    if (!picking) return;
    picking.banner.remove();
    picking = null;
    markTargets(false);
  }

  function pickPress(event) {
    if (!picking || event.button !== 0 || event.target.closest(".pick-banner")) return;
    // The strip's title still opens the window, for the Plugin Knob slot.
    if (event.target.closest("#link-open, #link-window") && !event.target.closest("#link-touched")) return;
    const found = targetAt(event.target);
    event.preventDefault();
    event.stopImmediatePropagation();
    swallowUntil = performance.now() + 600;
    const done = picking.done;
    cancelPick();
    if (found) done(found.target);
    else Hub.toast("That isn't a control a device can move: Map again, then click a fader, a knob or a button.");
  }

  function mapMenu(action) {
    Hub.send("midiLinkMenu", { action });
  }

  function showMenu(event) {
    event.preventDefault();
    const at = { getBoundingClientRect: () => ({ left: event.clientX, right: event.clientX, top: event.clientY, bottom: event.clientY }) };
    Hub.popupMenu(at, [
      { label: "MIDI Link Window", value: "window" },
      { label: "Save MIDI Link Map…", value: "save" },
      { label: "Load MIDI Link Map…", value: "load" },
      { label: `Clear All Links${state.links.length ? ` (${state.links.length})` : ""}`, value: "clearAll", disabled: !state.links.length }
    ], (action) => action === "window" ? openWindow(true) : mapMenu(action));
  }

  // ---------------------------------------------------------------- native
  Hub.on("midiLink", (data) => {
    state = { recent: data.recent || [], links: data.links || [], touched: data.touched || null };
    if (armed && !sourceOf(armed)) disarm();
    render();
    if (data.message) Hub.toast(data.message);

    // Learn linked the control waiting: its window opens, for its type; Learn goes on.
    if (data.learned && waiting) {
      const link = linkOf(data.learned), element = waiting.element;
      element.classList.remove("link-waiting");
      waiting = null;
      if (link && document.body.contains(element)) openPopup(data.learned, link.target, element, link.mode);
      if (learnMode) markTargets(true);
    } else if (waiting && !data.learning) {
      // The engine isn't waiting any more (the control can't be linked).
      waiting.element.classList.remove("link-waiting");
      waiting = null;
    }

    // The window shows how the link stands now (a link the engine refused is gone; news sent
    // before the drop reached it is passed over).
    if (popup) {
      const link = linkOf(popup.key);
      const fresh = performance.now() - popup.opened < 400;
      if (!link || targetId(link.target) !== targetId(popup.target)) { if (!fresh || data.message) closePopup(); }
      else popup.box.querySelectorAll("[data-mode]").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.mode === link.mode)));
    }
  });

  // Focus follows the selected track (its name, its knobs).
  for (const name of ["selection", "tracks"]) Hub.on(name, () => { if (windowOpen()) renderFocus(); });

  Hub.midiLink = { targetAt, targetText, state: () => state, pick, cancelPick, openWindow };

  Hub.on("ready", () => {
    const strip = $("midi-link");
    strip.addEventListener("pointerdown", startDrag);
    strip.addEventListener("contextmenu", showMenu);
    $("link-open").addEventListener("click", () => openWindow(!windowOpen()));

    const win = $("link-window");
    $("link-window-close").addEventListener("click", () => openWindow(false));
    win.addEventListener("click", (event) => {
      const menu = event.target.closest("[data-menu]");
      if (menu) { mapMenu(menu.dataset.menu); return; }
      const link = linkOfRow(event.target);
      if (!link) return;
      if (event.target.closest("[data-remove]")) {
        document.querySelectorAll(".link-found").forEach((el) => el.classList.remove("link-found"));
        Hub.send("midiLinkClear", { source: link.source });
      } else if (event.target.closest("[data-find]")) {
        showFound(link, true);
      }
    });
    win.addEventListener("change", (event) => {
      const select = event.target.closest(".link-type");
      const link = linkOfRow(select);
      if (link) Hub.send("midiLinkMode", { source: link.source, mode: select.value });
    });
    win.addEventListener("pointerover", (event) => {
      const row = event.target.closest(".link-row");
      if (row && !row.contains(event.relatedTarget)) showFound(linkOf(row.dataset.source), false);
    });
    win.addEventListener("pointerleave", () => document.querySelectorAll(".link-found").forEach((el) => el.classList.remove("link-found")));
    window.addEventListener("resize", () => { if (windowOpen()) placeWindow(); });

    document.addEventListener("pointerdown", pickPress, true);
    document.addEventListener("pointerdown", armedPress, true);
    document.addEventListener("pointerdown", learnPress, true);
    $("link-learn").addEventListener("click", () => setLearn(!learnMode));
    document.addEventListener("pointerdown", (event) => { if (popup && !popup.box.contains(event.target)) closePopup(); }, true);
    document.addEventListener("click", (event) => {
      if (performance.now() > swallowUntil) return;
      swallowUntil = 0;
      event.preventDefault();
      event.stopPropagation();
    }, true);
    window.addEventListener("keydown", (event) => {
      if (event.key !== "Escape" || (!popup && !armed && !picking && !learnMode && !windowOpen())) return;
      event.preventDefault();
      event.stopPropagation();
      if (learnMode && !popup) { setLearn(false); return; }
      // One step back at a time: the small window, then a link being made, then the MIDI Link window.
      if (popup || armed || picking) { closePopup(); disarm(); cancelPick(); }
      else openWindow(false);
    }, true);
    render();
    Hub.send("midiLinkState");
  });
})();
