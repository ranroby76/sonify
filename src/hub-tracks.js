// D:\workspace\Fanan_web_hub\src\hub-tracks.js
//
// Track list: the track headers on the left of the Song page. Plugins dragged
// from the Browser drop onto a track (an instrument replaces its instrument, an
// effect joins the end of its inserts, a MIDI tool joins its MIDI FX) or into the
// empty space below the tracks (a new track). The arrangement lives in hub-arrange.js. Selecting several tracks, and what
// they do together (buttons, faders, the right-click menu, dragging to move), is hub-group.js.
//
// Right-click the empty space under the tracks (or a header, under its own items) for Studio
// One's track menu: Add Tracks... (T: its dialog, with the name, count, type, mono or stereo,
// input and instrument), Add Audio Track (mono), Add Audio Track (stereo), Add Instrument Track,
// Add Choir Track (an audio track whose vocal sings as a choir too: its choir icon opens the
// Choir window, hub-choir.js),
// Add Tracks for all Inputs (one audio track for each of the song's inputs, named after it),
// Collapse All Tracks (all at the small size; again, back as they were), and Store Track
// Preset... (the selected tracks with their instruments, inserts and MIDI FX, into a file) and
// Load Track Preset... (a preset's tracks added to the song).

(() => {
  "use strict";

  const { $, element } = Hub;
  const BUILTIN_KEY = "builtin:test-synth";
  const LOADING_KEY = "loading:";

  const rows = new Map();           // track id -> row element
  let dropTarget = null;

  const pluginKey = (file, id) => `${file}|${id}`;

  // Cayman: what a track plays before it has an instrument (the engine's own tone), and
  // the same as a built-in plugin with its window. Lists show it once; opening it on a track loads
  // the plugin (its first sound is the same tone) and shows its window.
  const TEST_SYNTH_ID = "sonify.builtin.testsynth";
  const testSynthEntry = () => Hub.state.library.find((entry) => entry.id === TEST_SYNTH_ID) || null;
  const isTestSynth = (plugin) => Boolean(plugin) && plugin.id === TEST_SYNTH_ID;
  const openWhenLoaded = new Set();   // tracks whose Cayman window opens once it's loaded

  Hub.openTestSynth = (trackId) => {
    const track = Hub.trackById(trackId);
    if (!track || track.kind === "audio" || track.frozen) return;
    if (isTestSynth(track.instrument)) { Hub.openScreen(trackId, 0); return; }
    const entry = testSynthEntry();
    if (!entry) return;
    openWhenLoaded.add(trackId);
    Hub.send("loadPlugin", { trackId, file: entry.file, pluginId: entry.id });
  };

  Hub.on("tracks", () => {
    for (const id of [...openWhenLoaded]) {
      const track = Hub.trackById(id);
      if (!track) { openWhenLoaded.delete(id); continue; }
      if (isTestSynth(track.instrument)) { openWhenLoaded.delete(id); Hub.openScreen(id, 0); }
    }
  });

  // ---------------------------------------------------------------- icons
  const ICON_MONITOR = '<svg viewBox="0 0 16 16"><path d="M2 6h3l4-3v10L5 10H2z"/><path d="M11 5.5a3.5 3.5 0 0 1 0 5" fill="none" stroke="currentColor" stroke-width="1.4"/></svg>';
  const ICON_WAVE = '<svg viewBox="0 0 16 16"><path d="M1 8h2M4 4v8M6 2v12M8 5v6M10 3v10M12 6v4M14 7.5v1" stroke="currentColor" stroke-width="1.4" fill="none"/></svg>';
  const ICON_RECORD = '<svg viewBox="0 0 16 16"><circle cx="8" cy="8" r="4.5"/></svg>';
  const ICON_KEYS = '<svg viewBox="0 0 16 16"><path d="M1.5 3.5h13v9h-13z" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M5 3.5v5M8 3.5v5M11 3.5v5" stroke="currentColor" stroke-width="1.6"/></svg>';
  // A choir track's icon (it opens the Choir window): three singers.
  const ICON_CHOIR = '<svg viewBox="0 0 16 16"><circle cx="8" cy="4.6" r="2.1"/><path d="M4.4 11.5a3.6 3.6 0 0 1 7.2 0V13H4.4z"/><circle cx="3.2" cy="6.4" r="1.6" opacity=".75"/><path d="M.6 12.5a2.6 2.6 0 0 1 4-2.2 4.6 4.6 0 0 0-.9 2.2z" opacity=".75"/><circle cx="12.8" cy="6.4" r="1.6" opacity=".75"/><path d="M15.4 12.5a2.6 2.6 0 0 0-4-2.2 4.6 4.6 0 0 1 .9 2.2z" opacity=".75"/></svg>';
  const ICON_FREEZE = '<svg viewBox="0 0 16 16"><path d="M8 1.4v13.2M2.3 4.7l11.4 6.6M2.3 11.3l11.4-6.6M6.7 2.6 8 3.9l1.3-1.3M6.7 13.4 8 12.1l1.3 1.3M2.6 6.4l1.8-.5-.5-1.8M2.6 9.6l1.8.5-.5 1.8M13.4 6.4l-1.8-.5.5-1.8M13.4 9.6l-1.8.5.5 1.8" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round" stroke-linejoin="round"/></svg>';
  // The automation button: an envelope line with points.
  const ICON_AUTO = '<svg viewBox="0 0 16 16"><path d="M1.5 12.5 5 6.5l3.5 3 5-6.5" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"/><circle cx="5" cy="6.5" r="1.6"/><circle cx="8.5" cy="9.5" r="1.6"/><circle cx="13.5" cy="3" r="1.6"/></svg>';
  // An audio track's channel, as Studio One draws it: one circle mono, two stereo.
  const ICON_MONO = '<svg viewBox="0 0 16 16"><circle cx="8" cy="8" r="4.6" fill="none" stroke="currentColor" stroke-width="1.5"/></svg>';
  const ICON_STEREO = '<svg viewBox="0 0 16 16"><circle cx="5.6" cy="8" r="4.1" fill="none" stroke="currentColor" stroke-width="1.4"/><circle cx="10.4" cy="8" r="4.1" fill="none" stroke="currentColor" stroke-width="1.4"/></svg>';

  // ---------------------------------------------------------------- track list
  // The library's instruments and their signature, worked out once per library (not per row).
  let instrumentCache = { library: null, entries: [], signature: "" };

  function instrumentEntries() {
    if (instrumentCache.library !== Hub.state.library) {
      const entries = Hub.state.library.filter((entry) => entry.kind !== "effect" && entry.id !== TEST_SYNTH_ID);
      instrumentCache = { library: Hub.state.library, entries, signature: entries.map((entry) => pluginKey(entry.file, entry.id)).join(",") };
    }
    return instrumentCache;
  }

  function instrumentOptions(select, track) {
    // An instrument on its way (dropped, or picked here) shows already, while it loads.
    const loadingName = (track.loading || "").replace(/\.(vst3|wasm|wclap\.tar\.gz|wclap\.tgz|wclap)$/i, "");
    const current = loadingName ? LOADING_KEY : track.instrument && !isTestSynth(track.instrument) ? pluginKey(track.instrument.file, track.instrument.id) : BUILTIN_KEY;
    const { entries, signature: listSignature } = instrumentEntries();
    const signature = current + "#" + loadingName + "#" + listSignature;

    if (select.dataset.signature !== signature) {
      select.dataset.signature = signature;
      select.textContent = "";
      select.add(new Option("Cayman", BUILTIN_KEY));

      for (const entry of entries)
        select.add(new Option(entry.name, pluginKey(entry.file, entry.id)));

      if (track.instrument && !loadingName && !entries.some((entry) => pluginKey(entry.file, entry.id) === current))
        select.add(new Option(track.instrument.name, current));

      if (loadingName) {
        const loading = new Option(`${loadingName} (loading…)`, LOADING_KEY);
        loading.disabled = true;
        select.add(loading);
      }
    }

    // Always the track's own instrument, also when one is dropped on the track while this list
    // still has the keyboard focus from an earlier pick.
    select.value = current;
  }

  function createRow(track) {
    const row = element("div", "track");
    row.dataset.id = String(track.id);
    row.innerHTML = `
      <div class="track-colour"></div>
      <div class="track-body">
        <div class="track-top">
          <span class="track-number"></span>
          <button class="tbtn" data-action="mute" aria-pressed="false" title="Mute">M</button>
          <button class="tbtn" data-action="solo" aria-pressed="false" title="Solo">S</button>
          <button class="tbtn arm" data-action="recordArm" aria-pressed="false" title="Record arm: record on this track">${ICON_RECORD}</button>
          <button class="tbtn monitor" data-action="monitor" aria-pressed="false" title="Monitor: play this track from your keyboard">${ICON_MONITOR}</button>
          <button class="tbtn freeze" type="button" aria-pressed="false">${ICON_FREEZE}</button>
          <button class="tbtn track-auto-mode head" type="button" aria-pressed="true" aria-haspopup="menu" title="Automation: its mode, the envelope shown, Automation View">${ICON_AUTO}<i>R</i></button>
          <span class="track-name" title="Double-click to rename"></span>
          <button class="track-kind" type="button">${ICON_KEYS}</button>
        </div>
        <div class="track-meter" title="What the track sends out: left above, right below"><i></i><i></i></div>
        <label class="track-fader"><span>Vol</span><input type="range" data-control="volume" min="0" max="1" step="0.001"><output title="Double-click to type the volume"></output></label>
        <label class="track-fader pan-row"><span>Pan</span><input type="range" data-control="pan" min="-1" max="1" step="0.01"><output title="Double-click to type the pan"></output></label>
        <select class="track-instrument" data-control="instrument" aria-label="Instrument"></select>
        <button class="track-midi" type="button" aria-haspopup="true" aria-expanded="false"
          title="MIDI in: the MIDI devices and tracks this instrument hears"></button>
        <div class="track-input-row">
          <button class="tbtn track-mode" type="button"></button>
          <button class="tbtn polarity" data-action="polarity" type="button" aria-pressed="false" title="Polarity: turn the sound upside down (\u00d8)" aria-label="Invert polarity">\u00d8</button>
          <select class="track-input" data-control="input" aria-label="Input" title="The input this track hears and records"></select>
        </div>
        <div class="track-auto-row">
          <select class="track-auto" data-control="autoTarget" aria-label="Automation" title="What this track's envelope controls"></select>
          <button class="tbtn track-auto-mode" aria-pressed="true" title="Automation: Read. Click to choose Off, Read, Touch, Latch or Write.">R</button>
        </div>
      </div>
      <div class="track-resize" title="Drag to resize this track; double-click for the standard height"></div>`;
    return row;
  }

  /** The Automation View target menu: Volume, Pan, and every parameter of the track's plugins. */
  function autoOptions(select, track) {
    const automated = new Set((track.automation || []).map((lane) => Hub.arrange.laneKey(lane)));
    const plugins = [];
    if (track.instrument) plugins.push([0, track.instrument]);
    for (const insert of track.inserts || []) plugins.push([insert.insertId, insert]);
    const mark = (key, name) => (automated.has(key) ? "● " : "") + name;
    // The parameters' list key changes only when a plugin's list does (not with its values).
    const signature = JSON.stringify([...automated, ...plugins.map(([slot, p]) => [slot, p.name, p.paramsList !== undefined ? p.paramsList : (p.params || []).map((x) => x.id)])]);

    if (select.dataset.signature !== signature) {
      select.textContent = "";
      select.add(new Option(mark("volume", "Volume"), "volume"));
      select.add(new Option(mark("pan", "Pan"), "pan"));
      for (const [slot, plugin] of plugins) {
        const group = document.createElement("optgroup");
        group.label = plugin.name;
        for (const p of plugin.params || []) {
          const key = `param:${slot}:${p.id}`;
          group.appendChild(new Option(mark(key, p.name), key));
        }
        if (group.children.length) select.appendChild(group);
      }
      select.dataset.signature = signature;
    }
    if (document.activeElement !== select) select.value = Hub.arrange.autoTarget(track.id);
    // Its mode: in the header (always) and in Automation view's row.
    const now = AUTO_MODES.find(([value]) => value === track.autoMode) || AUTO_MODES[1];
    const row = select.closest(".track");
    for (const mode of row ? row.querySelectorAll(".track-auto-mode") : []) {
      mode.setAttribute("aria-pressed", String(track.autoMode !== "off"));
      mode.dataset.mode = now[0];
      if (mode.classList.contains("head")) {
        // The header's: the envelope icon, its mode's letter in the corner.
        const letter = mode.querySelector("i");
        if (letter) letter.textContent = now[0] === "off" ? "" : now[2];
        mode.title = `Automation (${now[1]}): click for its mode, the envelope shown and Automation View`;
      } else {
        mode.textContent = now[2];
        mode.title = `Automation: ${now[1]}. ${now[3]} Click to choose.`;
      }
    }
  }

  // The automation modes, as Studio One's: [value, name, letter, what it does].
  const AUTO_MODES = [
    ["off", "Off", "Off", "The envelopes rest."],
    ["read", "Read", "R", "The envelopes move the controls as the song plays."],
    ["touch", "Touch", "T", "As Read, but a control you take hold of while the song plays is recorded until you let go."],
    ["latch", "Latch", "L", "As Touch, but once taken hold of it goes on being recorded until the song stops."],
    ["write", "Write", "W", "Every envelope is recorded from where the song plays (and any control you move); then it goes to Touch."]
  ];

  /** An audio track's input choice: the song's inputs (Song Setup). */
  function inputOptions(select, track) {
    const inputs = Hub.state.inputs || [];
    const signature = inputs.map((input) => input.name).join("|");
    if (select.dataset.signature !== signature) {
      select.textContent = "";
      inputs.forEach((input, index) => select.add(new Option(input.name, String(index))));
      select.dataset.signature = signature;
    }
    if (document.activeElement !== select) select.value = String(track.input || 0);
  }

  /** An audio track's Mono/Stereo button: what its channel is now. */
  function showMode(button, track) {
    const mono = Boolean(track.mono);
    button.innerHTML = mono ? ICON_MONO : ICON_STEREO;
    button.dataset.mono = String(mono);
    button.title = mono
      ? "Mono: the input is one sound, connected to both sides (two same monos), and records as one. Click for stereo."
      : "Stereo: each of the input's channels on a side of its own (one channel alone on both sides). Click for mono.";
    button.setAttribute("aria-label", button.title);
  }

  function updateRow(row, track) {
    row.style.setProperty("--track-colour", track.colour);
    row.classList.toggle("selected", Hub.isTrackSelected(track.id));
    row.classList.toggle("audio", track.kind === "audio");
    // The kind icon: on an instrument track, the keyboard opens the instrument's window; on a
    // choir track, the singers open the Choir window.
    const choir = Boolean(track.choir);
    row.classList.toggle("choir", choir);
    const kindIcon = row.querySelector(".track-kind");
    const icon = choir ? "choir" : track.kind === "audio" ? "audio" : "keys";
    if (kindIcon.dataset.icon !== icon) {
      kindIcon.dataset.icon = icon;
      kindIcon.innerHTML = choir ? ICON_CHOIR : track.kind === "audio" ? ICON_WAVE : ICON_KEYS;
    }
    // Without an instrument it plays Cayman: its window loads it.
    const hasWindow = track.kind !== "audio" && (Boolean(track.instrument) || Boolean(testSynthEntry()));
    kindIcon.disabled = choir ? false : !hasWindow || Boolean(track.frozen);   // a frozen track's plugins rest
    kindIcon.classList.toggle("no-window", track.kind !== "audio" && !hasWindow);
    const instrumentName = track.instrument ? track.instrument.name : "Cayman";
    kindIcon.title = choir ? "Choir: its voices and their parts" : track.kind === "audio" ? "Audio track"
                   : track.frozen ? `${instrumentName}: frozen (unfreeze it to open it)` : `Open ${instrumentName}`;
    kindIcon.setAttribute("aria-label", kindIcon.title);
    row.classList.toggle("busy", Boolean(track.loading));
    row.querySelector(".track-number").textContent = String(track.number);

    const name = row.querySelector(".track-name");
    if (!name.querySelector("input")) {
      name.textContent = track.loading ? `Loading ${track.loading}…` : track.name;
      name.title = track.name + " · double-click to rename";
    }

    for (const action of ["mute", "solo", "recordArm", "monitor", "polarity"])
      row.querySelector(`[data-action="${action}"]`).setAttribute("aria-pressed", String(Boolean(track[action])));

    // Frozen: its render plays, so nothing live (recording, monitoring, its instrument or input)
    // until it thaws; its fader, pan, mute and solo still work.
    const frozen = Boolean(track.frozen);
    row.classList.toggle("frozen", frozen);
    const freeze = row.querySelector(".freeze");
    freeze.setAttribute("aria-pressed", String(frozen));
    if (!row.classList.contains("freezing"))
      freeze.title = frozen ? "Frozen: its render plays in its place, and its plugins rest. Click to unfreeze it."
                            : "Freeze: render this track's sound and play that instead, to rest its plugins";
    freeze.setAttribute("aria-label", freeze.title);
    for (const control of row.querySelectorAll('[data-action="recordArm"], [data-action="monitor"], .track-instrument, .track-midi, .track-input, .track-mode'))
      control.disabled = frozen;

    showFaders(row, track);

    if (track.kind === "audio") {
      inputOptions(row.querySelector('[data-control="input"]'), track);
      showMode(row.querySelector(".track-mode"), track);
    } else {
      instrumentOptions(row.querySelector('[data-control="instrument"]'), track);
    }
    autoOptions(row.querySelector('[data-control="autoTarget"]'), track);
    showMidiSummary(row.querySelector(".track-midi"), track);

    applyHeight(row, track);
  }

  /** A row as tall as its lane: its own height (dragged) or the view's size, times the height
      zoom (hub-arrange.js). Short rows show less: a small header (Vol only), then the top row alone. */
  function applyHeight(row, track) {
    if (row.classList.contains("resizing")) return;
    const shown = Hub.arrange && Hub.arrange.shownHeight ? Hub.arrange.shownHeight(track) : 0;
    row.style.height = shown > 0 ? `${shown}px` : track.height > 0 ? `${track.height}px` : "";
    row.classList.toggle("fit-small", shown > 0 && shown < 80);
    row.classList.toggle("fit-tiny", shown > 0 && shown < 52);
  }

  // The height zoom or the Track size changed: every header follows its lane.
  Hub.on("trackHeights", () => {
    for (const track of Hub.state.tracks) {
      const row = rows.get(track.id);
      if (row) applyHeight(row, track);
    }
  });

  // ---------------------------------------------------------------- MIDI in
  /** A track's MIDI sources by name: devices as they are, one channel of a split device as
      "Keys · Ch 3" (it's "Keys|3"), tracks as "Track 2 · Domina". */
  function midiSourceName(source) {
    if (!source.startsWith("track:")) {
      const bar = source.lastIndexOf("|");
      return bar > 0 && /^\d+$/.test(source.slice(bar + 1)) ? `${source.slice(0, bar)} \u00b7 Ch ${source.slice(bar + 1)}` : source;
    }
    const from = Hub.trackById(Number(source.slice(6)));
    return from ? `${from.name}${from.instrument ? " \u00b7 " + from.instrument.name : ""}` : "A removed track";
  }

  function showMidiSummary(button, track) {
    const sources = track.midiInputs || [];
    button.textContent = sources.length === 0 ? "No MIDI in"
      : sources.length === 1 ? `MIDI: ${midiSourceName(sources[0])}` : `MIDI: ${sources.length} inputs`;
    button.title = sources.length ? `MIDI in: ${sources.map(midiSourceName).join(", ")}` : "MIDI in: none. Click to choose a MIDI device or a track";
  }

  /** The MIDI in list: every MIDI input on the computer (keyboards first, then controllers, then
      virtual cables such as loopMIDI), and the tracks whose plugins play notes (a MIDI-out
      instrument, an arpeggiator); ticked ones are marked x. It stays open for several. A port Sonify isn't listening to (turned off, or held by
      another app) is in the list too, marked so, and choosing it turns it on. */
  let refreshMidiMenu = null;          // the open list's render, so it follows ports turned on or off
  let closeMidiMenu = null;
  let midiMenuShown = "";              // what that list was made from

  const midiMenuData = () => JSON.stringify([(Hub.state.status && Hub.state.status.midiInputs) || [],
                                             ((Hub.settings && Hub.settings.state()) || {}).midiInputs || []]);

  function openMidiMenu(button, trackId) {
    if (closeMidiMenu) closeMidiMenu();
    document.querySelectorAll(".popup-menu").forEach((menu) => menu.remove());
    const menu = element("div", "popup-menu midi-menu");
    menu.setAttribute("role", "menu");
    button.setAttribute("aria-expanded", "true");

    const render = () => {
      const track = Hub.trackById(trackId);
      if (!track) return;
      midiMenuShown = midiMenuData();
      const chosen = new Set(track.midiInputs || []);
      const heard = (Hub.state.status && Hub.state.status.midiInputs) || [];   // what Sonify listens to now
      const settings = (Hub.settings && Hub.settings.state()) || {};
      const known = settings.devices || [];
      const players = Hub.state.tracks.filter((t) => t.id !== trackId && t.kind === "instrument" && t.instrument && t.instrument.midiOut);

      const baseOf = (source) => {
        const bar = source.lastIndexOf("|");
        return bar > 0 && /^\d+$/.test(source.slice(bar + 1)) ? source.slice(0, bar) : source;
      };

      // An External Device shows the port it receives from, so a port is never lost under a device's name.
      const portOf = (source) => {
        const base = baseOf(source);
        const device = known.find((d) => d.name === base);
        return device && device.input && device.input !== base ? device.input : "";
      };

      // The ports Sonify isn't listening to, by the names tracks pick them by: the devices that
      // receive from them, or their own. A control surface's port plays Sonify, not tracks.
      const surfaces = known.filter((d) => d.kind === "surface");
      const surfacePorts = new Set(surfaces.map((d) => d.input).filter(Boolean));
      const quiet = new Map();
      for (const port of settings.midiInputs || []) {
        if (port.enabled || surfacePorts.has(port.name)) continue;
        const users = known.filter((d) => d.input === port.name).map((d) => d.name);
        for (const name of users.length ? users : [port.name])
          if (!heard.includes(name) && !quiet.has(name)) quiet.set(name, port);
      }
      const isSurface = (source) => surfacePorts.has(baseOf(source)) || surfaces.some((d) => d.name === baseOf(source));

      // Keyboards first, then controllers, then virtual MIDI cables (Hub.settings.inputRank); in
      // each, those Sonify listens to before the rest.
      const listed = [...heard, ...quiet.keys()];
      for (const source of chosen) if (!source.startsWith("track:") && !listed.includes(source)) listed.push(source);
      const rank = (source) => (Hub.settings && Hub.settings.inputRank ? Hub.settings.inputRank(source) : 1);
      const sources = listed.map((source, i) => [source, rank(source), i]).sort((a, b) => a[1] - b[1] || a[2] - b[2]).map(([source]) => source);

      menu.replaceChildren();
      const add = (value, label, state, detail, port) => {
        const on = chosen.has(value);
        const item = element("button", "midi-choice" + (state ? ` ${state}` : ""));
        item.type = "button";
        item.setAttribute("role", "menuitemcheckbox");
        item.setAttribute("aria-checked", String(on));
        item.append(element("span", "midi-box", on ? "\u2715" : ""), element("span", "midi-label", label));
        if (detail) item.appendChild(element("span", "midi-detail", detail));
        if (port) item.title = port.busy ? "Another app has this port open: choosing it tries again"
                                         : "Sonify isn't listening to this port: choosing it turns it on";
        item.addEventListener("click", (event) => {
          event.stopPropagation();
          const list = (track.midiInputs || []).filter((s) => s !== value);
          if (!on) list.push(value);
          track.midiInputs = list;   // shown at once; the engine confirms
          if (!on && port) Hub.send("midiPortEnabled", { id: port.id, enabled: true });
          showMidiSummary(button, track);
          render();
          Hub.send("trackSettings", { id: trackId, midiInputs: list });
        });
        menu.appendChild(item);
      };

      menu.appendChild(element("div", "popup-group", "MIDI devices"));
      if (!sources.length) menu.appendChild(element("div", "midi-empty", "No MIDI inputs found (File \u2192 Settings \u2192 MIDI Setup)"));
      for (const source of sources) {
        const name = midiSourceName(source);
        const port = heard.includes(source) ? null : quiet.get(source) || quiet.get(baseOf(source));
        if (heard.includes(source)) add(source, name, "", portOf(source));
        else if (port) add(source, `${name} (${port.busy ? "another app has it" : "off"})`, "quiet", portOf(source), port);
        else if (isSurface(source)) add(source, `${name} (a control surface: it plays Sonify)`, "missing", portOf(source));
        else add(source, `${name} (not connected)`, "missing", portOf(source));
      }

      menu.appendChild(element("div", "popup-group", "Tracks"));
      if (!players.length) menu.appendChild(element("div", "midi-empty", "No track's plugin plays notes itself"));
      for (const player of players) add(`track:${player.id}`, midiSourceName(`track:${player.id}`), "");
    };

    const refresh = () => { if (menu.isConnected) render(); };
    refreshMidiMenu = refresh;
    render();
    document.body.appendChild(menu);
    const r = button.getBoundingClientRect();
    const height = menu.offsetHeight || 0;
    menu.style.left = Math.max(4, Math.min(window.innerWidth - (menu.offsetWidth || 220) - 4, r.left)) + "px";
    menu.style.top = (r.bottom + height + 4 > window.innerHeight ? Math.max(4, r.top - height - 2) : r.bottom + 2) + "px";

    const close = () => {
      menu.remove();
      button.setAttribute("aria-expanded", "false");
      document.removeEventListener("pointerdown", outside, true);
      window.removeEventListener("keydown", escape, true);
      if (refreshMidiMenu === refresh) refreshMidiMenu = null;
      if (closeMidiMenu === close) closeMidiMenu = null;
    };
    closeMidiMenu = close;
    const outside = (event) => { if (!menu.contains(event.target) && event.target !== button) close(); };
    const escape = (event) => { if (event.key === "Escape") { event.stopPropagation(); close(); } };
    document.addEventListener("pointerdown", outside, true);   // a press anywhere else (even on the piano roll)
    window.addEventListener("keydown", escape, true);
    return menu;
  }

  // ---------------------------------------------------------------- the track filter
  // The filter button by + in the tracks' header: a list of the tracks, each ticked (x) to show it
  // or not, several at once, with Show All, Only the Selected Tracks and Hide the Selected Tracks.
  // A track hidden keeps playing as it is; it's just out of the track list and the arrangement.
  // Tracks added later show.
  let shownTracks = null;              // the ids shown; null: every track
  let knownTracks = new Set();         // the tracks there were, so new ones show
  let closeFilterMenu = null;

  Hub.isTrackShown = (track) => !shownTracks || !track || shownTracks.has(track.id);

  /** New tracks join what's shown; gone ones leave; all shown again is no filter. */
  function followFilter() {
    const ids = Hub.state.tracks.map((track) => track.id);
    if (shownTracks) {
      for (const id of ids) if (!knownTracks.has(id)) shownTracks.add(id);
      for (const id of [...shownTracks]) if (!ids.includes(id)) shownTracks.delete(id);
      if (ids.every((id) => shownTracks.has(id))) shownTracks = null;
    }
    knownTracks = new Set(ids);
    showFilterButton();
  }

  function showFilterButton() {
    const button = $("track-filter");
    if (!button) return;
    const total = Hub.state.tracks.length;
    const shown = shownTracks ? shownTracks.size : total;
    button.classList.toggle("active", Boolean(shownTracks));
    button.setAttribute("aria-pressed", String(Boolean(shownTracks)));
    button.title = shownTracks ? `Track filter: ${shown} of ${total} tracks shown. Click to choose` : "Track filter: choose the tracks shown";
    $("track-filter-count").textContent = shownTracks ? `${shown}/${total}` : "";
    $("track-filter-count").hidden = !shownTracks;
  }

  function setShownTracks(ids) {
    const all = Hub.state.tracks.map((track) => track.id);
    const chosen = new Set(ids.filter((id) => all.includes(id)));
    if (!chosen.size) { Hub.toast("At least one track stays shown."); return false; }
    shownTracks = chosen.size === all.length ? null : chosen;
    knownTracks = new Set(all);
    renderTrackList();
    Hub.arrange.redraw();
    return true;
  }

  function openFilterMenu(button) {
    if (closeFilterMenu) { closeFilterMenu(); return; }
    document.querySelectorAll(".popup-menu").forEach((menu) => menu.remove());
    const menu = element("div", "popup-menu midi-menu track-filter-menu");
    menu.setAttribute("role", "menu");
    button.setAttribute("aria-expanded", "true");

    const action = (label, run, disabled, title) => {
      const item = element("button", "track-filter-action", label);
      item.type = "button";
      item.setAttribute("role", "menuitem");
      item.disabled = Boolean(disabled);
      if (title) item.title = title;
      item.addEventListener("click", (event) => { event.stopPropagation(); if (run() !== false) render(); });
      menu.appendChild(item);
    };

    const render = () => {
      menu.replaceChildren();
      const tracks = Hub.state.tracks;
      const selected = tracks.filter((track) => Hub.isTrackSelected(track.id)).map((track) => track.id);
      const isShown = (track) => Hub.isTrackShown(track);

      menu.appendChild(element("div", "popup-group", "Show"));
      action("All Tracks", () => setShownTracks(tracks.map((t) => t.id)), !shownTracks);
      action("Only the Selected Tracks", () => setShownTracks(selected), !selected.length,
             selected.length ? `${selected.length} selected` : "Select tracks first (Ctrl or Shift for several)");
      action("Hide the Selected Tracks", () => setShownTracks(tracks.filter((t) => isShown(t) && !selected.includes(t.id)).map((t) => t.id)),
             !selected.some((id) => isShown(Hub.trackById(id))));
      menu.appendChild(element("div", "popup-separator"));
      menu.appendChild(element("div", "popup-group", "Tracks"));
      if (!tracks.length) menu.appendChild(element("div", "midi-empty", "No tracks yet"));

      for (const track of tracks) {
        const on = isShown(track);
        const item = element("button", "midi-choice track-filter-choice");
        item.type = "button";
        item.setAttribute("role", "menuitemcheckbox");
        item.setAttribute("aria-checked", String(on));
        const swatch = element("span", "track-filter-colour");
        swatch.style.background = track.colour || track.color || "var(--line-2)";
        item.append(element("span", "midi-box", on ? "\u2715" : ""), element("span", "track-filter-number", String(track.number || "")),
                    swatch, element("span", "midi-label", track.name));
        item.title = on ? "Shown: click to hide it" : "Hidden: click to show it";
        item.addEventListener("click", (event) => {
          event.stopPropagation();
          const ids = tracks.filter((t) => (t.id === track.id ? !on : isShown(t))).map((t) => t.id);
          if (setShownTracks(ids)) render();
        });
        menu.appendChild(item);
      }
    };

    render();
    document.body.appendChild(menu);
    const r = button.getBoundingClientRect();
    const height = menu.offsetHeight || 0;
    menu.style.left = Math.max(4, Math.min(window.innerWidth - (menu.offsetWidth || 230) - 4, r.left)) + "px";
    menu.style.top = (r.bottom + height + 4 > window.innerHeight ? Math.max(4, r.top - height - 2) : r.bottom + 2) + "px";

    const close = () => {
      menu.remove();
      button.setAttribute("aria-expanded", "false");
      document.removeEventListener("pointerdown", outside, true);
      window.removeEventListener("keydown", escape, true);
      if (closeFilterMenu === close) closeFilterMenu = null;
    };
    const follow = () => { if (menu.isConnected) render(); };
    closeFilterMenu = close;
    filterMenuRender = follow;
    const outside = (event) => { if (!menu.contains(event.target) && !button.contains(event.target)) close(); };
    const escape = (event) => { if (event.key === "Escape") { event.stopPropagation(); close(); } };
    document.addEventListener("pointerdown", outside, true);
    window.addEventListener("keydown", escape, true);
  }
  let filterMenuRender = null;         // the open list's render: it follows tracks added, renamed, selected

  // ---------------------------------------------------------------- the default MIDI input
  /** The first in a track's MIDI in list (keyboards first, as the list ranks them; not a control
      surface): a new instrument track hears it, ticked, unless Settings > MIDI Setup names a
      default device. Told to the engine whenever the ports or the settings change. */
  let preferredShown = null, preferredSent = null;

  function sendPreferredSource() {
    const data = midiMenuData();
    if (data === preferredShown) return;
    preferredShown = data;

    const heard = (Hub.state.status && Hub.state.status.midiInputs) || [];
    const settings = (Hub.settings && Hub.settings.state()) || {};
    const known = settings.devices || [];
    const surfaces = known.filter((d) => d.kind === "surface");
    const surfacePorts = new Set(surfaces.map((d) => d.input).filter(Boolean));
    const baseOf = (source) => {
      const bar = source.lastIndexOf("|");
      return bar > 0 && /^\d+$/.test(source.slice(bar + 1)) ? source.slice(0, bar) : source;
    };
    const rank = (source) => (Hub.settings && Hub.settings.inputRank ? Hub.settings.inputRank(source) : 1);
    const first = heard.filter((source) => !source.startsWith("track:") && !surfacePorts.has(baseOf(source)) && !surfaces.some((d) => d.name === baseOf(source)))
                       .map((source, i) => [source, rank(source), i]).sort((a, b) => a[1] - b[1] || a[2] - b[2])[0];
    const source = first ? first[0] : "";
    if (source === preferredSent) return;
    preferredSent = source;
    Hub.send("midiDefaultSource", { source });
  }

  // ---------------------------------------------------------------- resizing a track by its bottom edge
  function setupResizing(list) {
    list.addEventListener("pointerdown", (event) => {
      const edge = event.target.closest(".track-resize");
      if (!edge || event.button !== 0) return;
      event.preventDefault();
      event.stopPropagation();
      const row = edge.closest(".track");
      const track = Hub.trackById(Number(row.dataset.id));
      if (!track) return;

      const startY = event.clientY, startHeight = row.getBoundingClientRect().height;
      // A selected track's edge sizes the whole selection to the same height (tracks only: FX
      // channels and buses have no headers here).
      const group = Hub.groupOf(track.id).map((id) => ({ track: Hub.trackById(id), row: rows.get(id) })).filter((each) => each.track && each.row);
      for (const each of group) each.row.classList.add("resizing");
      edge.setPointerCapture(event.pointerId);

      // Dragged as it shows (the height zoom on it); kept as its own height at 100%.
      const zoom = Hub.arrange && Hub.arrange.vzoom ? Hub.arrange.vzoom() : 1;
      const move = (e) => {
        const height = Math.round(Math.max(56, Math.min(600, (startHeight + e.clientY - startY) / zoom)));
        for (const each of group) {
          each.track.height = height;
          each.row.style.height = `${Hub.arrange ? Hub.arrange.shownHeight(each.track) : height}px`;
        }
        if (Hub.arrange) { Hub.arrange.updateSizes(); Hub.arrange.redraw(); }
      };
      const up = () => {
        edge.removeEventListener("pointermove", move);
        edge.removeEventListener("pointerup", up);
        edge.removeEventListener("pointercancel", up);
        for (const each of group) {
          each.row.classList.remove("resizing");
          applyHeight(each.row, each.track);
        }
        if (group.length > 1) Hub.send("tracksSettings", { ids: group.map((each) => each.track.id), height: track.height || 0 });
        else Hub.send("trackSettings", { id: track.id, height: track.height || 0 });
      };
      edge.addEventListener("pointermove", move);
      edge.addEventListener("pointerup", up);
      edge.addEventListener("pointercancel", up);
    });

    list.addEventListener("dblclick", (event) => {
      const edge = event.target.closest(".track-resize");
      if (!edge) return;
      const row = edge.closest(".track");
      const track = Hub.trackById(Number(row.dataset.id));
      if (!track) return;
      // The whole selection back to the Track size, when this track is in it.
      const ids = Hub.groupOf(track.id).filter((id) => rows.has(id));
      for (const id of ids) {
        const each = Hub.trackById(id);
        if (!each) continue;
        each.height = 0;
        applyHeight(rows.get(id), each);
      }
      if (Hub.arrange) { Hub.arrange.updateSizes(); Hub.arrange.redraw(); }
      if (ids.length > 1) Hub.send("tracksSettings", { ids, height: 0 });
      else Hub.send("trackSettings", { id: track.id, height: 0 });
    });
  }

  /** Volume and pan in a header row, unless the user is dragging that control right now. */
  function showFaders(row, track) {
    const volume = row.querySelector('[data-control="volume"]');
    const pan = row.querySelector('[data-control="pan"]');
    if (document.activeElement !== volume) volume.value = String(Hub.dbToFader(track.volume));
    if (document.activeElement !== pan) pan.value = String(track.pan);
    volume.nextElementSibling.textContent = Hub.dbText(track.volume);
    pan.nextElementSibling.textContent = Hub.panText(track.pan);
  }

  function renderTrackList() {
    const list = $("track-rows");
    const alive = new Set();
    followFilter();

    Hub.state.tracks.forEach((track, index) => {
      let row = rows.get(track.id);

      if (!row) {
        row = createRow(track);
        rows.set(track.id, row);
      }

      updateRow(row, track);
      row.classList.toggle("filtered-out", !Hub.isTrackShown(track));
      alive.add(track.id);

      if (list.children[index] !== row) list.insertBefore(row, list.children[index] || null);
    });

    for (const [id, row] of rows) {
      if (!alive.has(id)) {
        row.remove();
        rows.delete(id);
      }
    }

    $("track-empty").hidden = Hub.state.tracks.length > 0;
    Hub.arrange.updateSizes();
  }

  function trackIdFrom(target) {
    const row = target.closest(".track");
    return row ? Number(row.dataset.id) : 0;
  }

  /** Double-click a header's volume or pan value to type it: dB for volume (-72 to +10, lower
      is off), -100 (left) to 100 (right) for pan; digits, a minus and at most two decimals.
      Enter or leaving the field sets it, Escape keeps what was there. */
  function startValueEdit(output) {
    const slider = output.previousElementSibling;
    const control = slider && slider.dataset.control;
    const id = trackIdFrom(output);
    const track = Hub.trackById(id);
    if (!track || (control !== "volume" && control !== "pan") || output.hidden) return;

    const current = control === "volume" ? (track.volume <= Hub.SILENT_DB ? -72 : track.volume) : track.pan * 100;
    const input = document.createElement("input");
    input.type = "text";
    input.className = "track-value-edit";
    input.inputMode = "decimal";
    input.spellcheck = false;
    input.value = String(Math.round(current * 100) / 100);
    input.setAttribute("aria-label", control === "volume" ? "Volume in dB" : "Pan, -100 left to 100 right");
    input.title = control === "volume" ? "Volume in dB (-72 to +10)" : "Pan: -100 left to 100 right";
    output.hidden = true;
    output.after(input);
    input.focus();
    input.select();

    let last = input.value;
    input.addEventListener("input", () => {
      // Digits only, a minus in front, at most two digits after the point.
      if (/^-?\d*(\.\d{0,2})?$/.test(input.value)) last = input.value;
      else {
        const at = Math.max(0, input.selectionStart - (input.value.length - last.length));
        input.value = last;
        input.setSelectionRange(at, at);
      }
    });

    let finished = false;
    const finish = (save) => {
      if (finished || !input.isConnected) return;
      finished = true;
      const typed = Number(input.value);
      input.remove();
      output.hidden = false;
      if (!save || input.value.trim() === "" || input.value === "-" || !Number.isFinite(typed)) return;
      const value = control === "volume"
        ? (typed <= Hub.SILENT_DB ? Hub.faderToDb(0) : Hub.clamp(Math.round(typed * 100) / 100, Hub.SILENT_DB, Hub.MAX_DB))
        : Hub.clamp(Math.round(typed * 100) / 10000, -1, 1);
      const shown = Hub.trackById(id);
      if (!shown) return;
      slider.value = String(control === "volume" ? Hub.dbToFader(value) : value);
      output.textContent = control === "volume" ? Hub.dbText(value) : Hub.panText(value);
      Hub.setTrackValue(id, control, value, slider);
    };

    input.addEventListener("keydown", (event) => {
      if (event.key === "Enter") { event.preventDefault(); finish(true); }
      if (event.key === "Escape") { event.preventDefault(); finish(false); }
      event.stopPropagation();
    });
    input.addEventListener("blur", () => finish(true));
    for (const type of ["pointerdown", "click", "dblclick"]) input.addEventListener(type, (event) => event.stopPropagation());
  }

  function startRename(nameElement, track) {
    Hub.inlineRename(nameElement, track.name, (value) => Hub.send("trackSettings", { id: track.id, name: value }));
  }

  function setupTrackList() {
    const list = $("track-rows");
    setupResizing(list);

    list.addEventListener("click", (event) => {
      const midi = event.target.closest(".track-midi");
      if (!midi) return;
      event.stopPropagation();
      // Its button closes the list as it opened it (a list some other menu took away opens again).
      if (midi.getAttribute("aria-expanded") === "true" && document.querySelector(".midi-menu") && closeMidiMenu) closeMidiMenu();
      else openMidiMenu(midi, trackIdFrom(midi));
    });

    list.addEventListener("click", (event) => {
      const id = trackIdFrom(event.target);
      if (!id) return;

      const kindIcon = event.target.closest(".track-kind");

      if (kindIcon) {
        const track = Hub.trackById(id);
        if (track && track.choir && Hub.choir) Hub.choir.open(id);   // a choir track's voices
        else if (track && !track.instrument) Hub.openTestSynth(id);   // Cayman, loaded as a plugin
        else Hub.openScreen(id, 0);   // the instrument's window, as the Inspector opens it
        return;
      }

      // The snowflake: freeze the track (again while it freezes: stop), or thaw it.
      const freeze = event.target.closest(".freeze");

      if (freeze) {
        const track = Hub.trackById(id);
        const freezing = freeze.closest(".track").classList.contains("freezing");
        if (track) Hub.send("trackFreeze", { id, on: !track.frozen && !freezing });
        return;
      }

      // An audio track's channel: mono or stereo.
      const mode = event.target.closest(".track-mode");

      if (mode) {
        const track = Hub.trackById(id);
        if (!track) return;
        track.mono = !track.mono;   // shown at once; the engine confirms
        showMode(mode, track);
        Hub.send("trackSettings", { id, mono: track.mono });
        return;
      }

      // Mute, Solo, Record, Monitor: the whole selection when this track is in it.
      const button = event.target.closest("[data-action]");

      if (button) {
        Hub.group.toggle(id, button.dataset.action, button);
        return;
      }

      if (!event.target.closest("input, select")) Hub.group.click(event, id);
    });

    // Right-click a header: Duplicate, Remove, Color, Rename, for the selection, then the track
    // menu. Under the tracks, the track menu alone.
    $("tracklist").addEventListener("contextmenu", (event) => {
      if (event.target.closest("input, select")) return;
      const id = trackIdFrom(event.target);
      event.preventDefault();
      if (id) Hub.group.menu(id, event.clientX, event.clientY);
      else Hub.popupMenu(Hub.pointAnchor(event), trackMenuItems(), runTrackMenu);
    });

    // Dragging a header by its number, name or colour moves the selection.
    list.addEventListener("pointerdown", (event) => {
      const handle = event.target.closest(".track-number, .track-name, .track-colour");
      if (!handle || handle.querySelector("input")) return;
      Hub.group.startReorder(event, trackIdFrom(handle), "y",
                             () => Hub.state.tracks.map((track) => ({ id: track.id, element: rows.get(track.id) })).filter((item) => item.element));
    });

    list.addEventListener("input", (event) => {
      const control = event.target.dataset.control;
      if (control !== "volume" && control !== "pan") return;
      const raw = Number(event.target.value);
      const value = control === "volume" ? Hub.faderToDb(raw) : raw;
      event.target.nextElementSibling.textContent = control === "volume" ? Hub.dbText(value) : Hub.panText(value);
      Hub.setTrackValue(trackIdFrom(event.target), control, value, event.target);
    });

    // Double-click a fader to reset it, as in Studio One; its value, to type one.
    list.addEventListener("dblclick", (event) => {
      const value = event.target.closest(".track-fader output");
      if (value) {
        event.preventDefault();
        startValueEdit(value);
        return;
      }

      const control = event.target.dataset && event.target.dataset.control;

      if (control === "volume" || control === "pan") {
        event.target.value = control === "volume" ? String(Hub.dbToFader(0)) : "0";
        event.target.dispatchEvent(new Event("input", { bubbles: true }));
        return;
      }

      const name = event.target.closest(".track-name");
      const track = name && Hub.trackById(trackIdFrom(name));
      if (track) startRename(name, track);
    });

    list.addEventListener("click", (event) => {
      const mode = event.target.closest(".track-auto-mode");
      if (!mode) return;
      const row = mode.closest(".track");
      const id = Number(row.dataset.id);
      const track = Hub.trackById(id);
      const items = [
        { group: "Mode" },
        ...AUTO_MODES.map(([value, name, , tip]) => ({ label: name, value: "mode:" + value, checked: Boolean(track) && track.autoMode === value, title: tip }))
      ];
      // The header's button has the rest too: which envelope to show, and Automation View.
      if (mode.classList.contains("head") && track) {
        const shown = Hub.arrange.autoTarget(id);
        const automated = new Set((track.automation || []).map((lane) => Hub.arrange.laneKey(lane)));
        const envelope = (key, name) => ({ label: (automated.has(key) ? "\u25cf " : "") + name, value: "show:" + key, checked: shown === key });
        items.push({ separator: true }, { group: "Envelope" }, envelope("volume", "Volume"), envelope("pan", "Pan"));
        const plugins = [];
        if (track.instrument) plugins.push([0, track.instrument]);
        for (const insert of track.inserts || []) plugins.push([insert.insertId, insert]);
        for (const [slot, plugin] of plugins) {
          const params = (plugin.params || []).map((p) => envelope(`param:${slot}:${p.id}`, p.name));
          if (params.length) items.push({ label: plugin.name, submenu: params, checked: shown.startsWith(`param:${slot}:`) });
        }
        const viewOn = Boolean(Hub.arrange.view.automation);
        items.push({ separator: true }, { label: "Automation View", value: "view", checked: viewOn, shortcut: "A",
                     title: "Each track's envelope over its clips" });
      }
      Hub.popupMenu(mode, items, (value) => {
        if (typeof value !== "string") return;
        if (value.startsWith("mode:")) {
          const chosen = value.slice(5);
          if (track) track.autoMode = chosen;   // shown at once; the engine confirms
          Hub.send("setAutomationMode", { trackId: id, mode: chosen });
        } else if (value.startsWith("show:")) {
          // The envelope chosen, shown: Automation View comes on.
          Hub.arrange.setAutoTarget(id, value.slice(5));
          if (!Hub.arrange.view.automation) Hub.run("toggleAutomation");
          Hub.selectTrack(id);
        } else if (value === "view") {
          Hub.run("toggleAutomation");
        }
      });
    });

    list.addEventListener("change", (event) => {
      if (event.target.dataset.control === "autoTarget") {
        const row = event.target.closest(".track");
        Hub.arrange.setAutoTarget(Number(row.dataset.id), event.target.value);
        event.target.blur();
        return;
      }
      if (event.target.dataset.control === "input") {
        const row = event.target.closest(".track");
        Hub.send("trackSettings", { id: Number(row.dataset.id), input: Number(event.target.value) });
        event.target.blur();
        return;
      }
      if (event.target.dataset.control !== "instrument") return;
      const id = trackIdFrom(event.target);
      const value = event.target.value;
      event.target.blur();

      // Cayman: the built-in plugin (the tone it starts with is the one a track plays without one).
      if (value === BUILTIN_KEY) {
        const entry = testSynthEntry();
        if (entry) Hub.send("loadPlugin", { trackId: id, file: entry.file, pluginId: entry.id });
        else Hub.send("clearInstrument", { trackId: id });
        return;
      }

      const entry = Hub.state.library.find((item) => pluginKey(item.file, item.id) === value);
      if (entry) Hub.send("loadPlugin", { trackId: id, file: entry.file, pluginId: entry.id });
    });

    // Like Studio One's Add Tracks: an instrument track or an audio track; or session players.
    $("add-track").addEventListener("click", () => {
      Hub.popupMenu($("add-track"), [
        { label: "Instrument Track", value: "addTrack" },
        { label: "Audio Track", value: "addAudioTrack" },
        { label: "Session Players…", value: "addSessionPlayers" }
      ], (command) => Hub.run(command));
    });

    // The track list follows the arrangement's vertical scrolling.
    $("tracklist").addEventListener("wheel", (event) => {
      $("lanes").scrollTop += event.deltaY;
      event.preventDefault();
    }, { passive: false });
  }

  // ---------------------------------------------------------------- drag and drop from the Browser
  function setDropTarget(target) {
    if (dropTarget === target) return;
    if (dropTarget) dropTarget.classList.remove("drop-target");
    dropTarget = target;
    if (dropTarget) dropTarget.classList.add("drop-target");
  }

  function dropTrackFor(event) {
    const row = event.target.closest && event.target.closest(".track");
    if (row) return Hub.trackById(Number(row.dataset.id));
    if (event.currentTarget === $("lanes")) return Hub.arrange.trackAt(event.clientY);
    return null;
  }

  /**
      Puts a Browser entry on a channel: a track id, Hub.MAIN, or null for a new track.
      position places an effect among the channel's inserts (-1: the end).
  */
  function dropEntryOn(entry, channel, position = -1) {
    const newTrack = channel === null || channel === undefined;

    if (entry.builtin) {
      if (newTrack) Hub.send("addTrack");
      else if (channel !== Hub.MAIN) Hub.send("clearInstrument", { trackId: channel });
      return;
    }

    if (newTrack) Hub.send("addTrack", { file: entry.file, pluginId: entry.id });
    else Hub.send("loadPlugin", { trackId: channel, file: entry.file, pluginId: entry.id, position });
  }

  Hub.dropEntryOn = dropEntryOn;

  function setupDropTargets() {
    for (const area of [$("tracklist"), $("lanes")]) {
      area.addEventListener("dragover", (event) => {
        if (!Hub.dragEntry && !Hub.dragFile && !Hub.dragMidiTool) return;
        const track = dropTrackFor(event);

        // A MIDI tool from the Browser's MIDI tab goes onto an instrument track's MIDI FX.
        if (Hub.dragMidiTool && !(track && track.kind === "instrument")) {
          setDropTarget(null);
          return;
        }

        event.preventDefault();
        event.dataTransfer.dropEffect = "copy";

        // An effect over an audio clip: that clip's own effect (the clip is the target, not the track).
        const onClip = event.currentTarget === $("lanes") && Hub.arrange.effectDropAt ? Hub.arrange.effectDropAt(event.clientX, event.clientY) : null;
        const clipId = onClip ? onClip.clip.id : null;
        if (clipId !== effectTarget) {
          effectTarget = clipId;
          Hub.arrange.redraw();
        }
        setDropTarget(onClip ? null : track ? rows.get(track.id) : $("track-space"));
      });

      area.addEventListener("dragleave", (event) => {
        if (!area.contains(event.relatedTarget)) { setDropTarget(null); clearEffectTarget(); }
      });

      area.addEventListener("drop", (event) => {
        // An audio file from the Browser's Files tab: onto an audio track where it's dropped
        // (at the playhead when dropped on a track header), or onto a new audio track.
        if (Hub.dragFile) {
          event.preventDefault();
          const track = dropTrackFor(event);
          const onLanes = event.currentTarget === $("lanes");
          const start = onLanes ? Hub.arrange.snap(Hub.arrange.ppqAt(event.clientX)) : Hub.songPosition();
          // The engine decides: audio onto an audio track, a MIDI file onto an instrument track (else a new one).
          Hub.send("importAudio", { file: Hub.dragFile.path, trackId: track ? track.id : 0, start });
          Hub.dragFile = null;
          setDropTarget(null);
          return;
        }

        if (Hub.dragMidiTool) {
          event.preventDefault();
          const track = dropTrackFor(event);
          setDropTarget(null);
          if (track && Hub.midiFx) Hub.midiFx.addDevice(track.id, Hub.dragMidiTool);
          Hub.dragMidiTool = null;
          return;
        }

        if (!Hub.dragEntry) return;
        event.preventDefault();
        const track = dropTrackFor(event);
        setDropTarget(null);
        clearEffectTarget();

        // An effect dropped on an audio clip: the clip's own.
        const onClip = event.currentTarget === $("lanes") && Hub.arrange.effectDropAt ? Hub.arrange.effectDropAt(event.clientX, event.clientY) : null;
        if (onClip) Hub.arrange.setClipEffect(onClip.track, onClip.clip, Hub.dragEntry);
        else dropEntryOn(Hub.dragEntry, track ? track.id : null);
        Hub.dragEntry = null;
      });
    }

    window.addEventListener("dragend", () => { setDropTarget(null); clearEffectTarget(); });
  }

  // The audio clip an effect dragged from the Browser would land on: outlined (hub-arrange.js draws it).
  let effectTarget = null;
  Hub.effectDropClip = () => effectTarget;
  function clearEffectTarget() {
    if (effectTarget === null) return;
    effectTarget = null;
    Hub.arrange.redraw();
  }

  // ---------------------------------------------------------------- the track menu
  const MENU_ICONS = {
    add: '<svg viewBox="0 0 16 16"><path d="M7 2.5h2v4.5h4.5v2H9v4.5H7V9H2.5V7H7z"/></svg>',
    mono: '<svg viewBox="0 0 16 16"><path d="M1 8h1.6M3.6 5v6M5.6 2.5v11M7.6 6v4M9.6 4v8M11.6 6.5v3M13.6 7.4v1.2" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"/></svg>',
    stereo: '<svg viewBox="0 0 16 16"><path d="M1 5h1.6M3.6 3v4M5.6 1.5v7M7.6 3.5v3M9.6 2.5v5M1 11.5h1.6M3.6 9.5v4M5.6 8v7M7.6 10v3M9.6 9v5" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linecap="round"/></svg>',
    instrument: '<svg viewBox="0 0 16 16"><path d="M1.5 3.5h13v9h-13z" fill="none" stroke="currentColor" stroke-width="1.2"/><path d="M5 3.5v5M8 3.5v5M11 3.5v5" stroke="currentColor" stroke-width="1.6"/></svg>',
    choir: ICON_CHOIR,
    inputs: '<svg viewBox="0 0 16 16"><path d="M1.5 3h7v1.6h-7zM1.5 7.2h7v1.6h-7zM1.5 11.4h7V13h-7z"/><path d="M11.4 5.5h1.6v2.7h2.5v1.6H13v2.7h-1.6V9.8H8.9V8.2h2.5z"/></svg>',
    collapse: '<svg viewBox="0 0 16 16"><path d="M8 4l5.5 7h-11z"/></svg>',
    expand: '<svg viewBox="0 0 16 16"><path d="M8 12l5.5-7h-11z"/></svg>'
  };

  /** Every track at the small size, none with a height of its own (Collapse All Tracks done). */
  const allCollapsed = () => $("track-size").value === "small" && Hub.state.tracks.every((track) => !(track.height > 0));
  let expandTo = null;   // what Collapse All Tracks left: {size, heights: [[id, height]]}

  function setSize(size) {
    const select = $("track-size");
    if (select.value === size) return;
    select.value = size;
    select.dispatchEvent(new Event("change"));
  }

  function setHeight(track, height) {
    track.height = height;
    Hub.send("trackSettings", { id: track.id, height });
  }

  /** Collapse All Tracks: the small size, heights of their own put aside. Again: back as they were. */
  function collapseAll() {
    if (allCollapsed()) {
      const back = expandTo;
      expandTo = null;
      setSize(back ? back.size : "normal");
      if (back) for (const [id, height] of back.heights) { const track = Hub.trackById(id); if (track) setHeight(track, height); }
    } else {
      expandTo = { size: $("track-size").value, heights: Hub.state.tracks.filter((track) => track.height > 0).map((track) => [track.id, track.height]) };
      for (const track of Hub.state.tracks) if (track.height > 0) setHeight(track, 0);
      setSize("small");
    }
    renderTrackList();
    if (Hub.arrange) Hub.arrange.updateSizes();
  }

  /** The tracks a preset is stored from: the selected ones (tracks and channels alike). */
  const presetIds = () => Hub.state.selectedTracks.filter((id) => Hub.trackById(id));

  /** Studio One's track menu, as far as Sonify has its parts. */
  function trackMenuItems() {
    const key = (command) => (Hub.keys ? Hub.keys.label(command) : "");
    const ids = presetIds();
    const collapsed = allCollapsed();
    return [
      { label: "Add Tracks\u2026", value: "addTracks", icon: MENU_ICONS.add, shortcut: key("addTracks") },
      { label: "Add Audio Track (mono)", value: "addMono", icon: MENU_ICONS.mono },
      { label: "Add Audio Track (stereo)", value: "addStereo", icon: MENU_ICONS.stereo },
      { label: "Add Instrument Track", value: "addInstrument", icon: MENU_ICONS.instrument, shortcut: key("addTrack") },
      { label: "Add Choir Track", value: "addChoir", icon: MENU_ICONS.choir,
        title: "An audio track whose vocal sings as a choir too: put a vocal on it, then set its voices with the choir icon on its header" },
      { label: "Add Tracks for all Inputs", value: "addForInputs", icon: MENU_ICONS.inputs, disabled: !(Hub.state.inputs || []).length,
        title: "An audio track for each of the song's inputs (Song > Song Setup), named after it" },
      { label: collapsed ? "Expand All Tracks" : "Collapse All Tracks", value: "collapse", icon: collapsed ? MENU_ICONS.expand : MENU_ICONS.collapse,
        disabled: !Hub.state.tracks.length },
      { separator: true },
      { label: ids.length > 1 ? `Store Track Preset (${ids.length} Tracks)\u2026` : "Store Track Preset\u2026", value: "storePreset", disabled: !ids.length,
        title: "The selected tracks with their instruments, inserts and MIDI FX, kept in a file" },
      { label: "Load Track Preset\u2026", value: "loadPreset", title: "A track preset's tracks, added to the song" }
    ];
  }

  function runTrackMenu(value) {
    if (value === "addTracks") openAddTracks();
    else if (value === "addMono") Hub.send("trackMenu", { action: "addTracks", type: "audio", count: 1, mono: true });
    else if (value === "addStereo") Hub.send("trackMenu", { action: "addTracks", type: "audio", count: 1, mono: false });
    else if (value === "addInstrument") Hub.run("addTrack");
    else if (value === "addChoir") Hub.send("trackMenu", { action: "addTracks", type: "choir", count: 1, mono: true });
    else if (value === "addForInputs") Hub.send("trackMenu", { action: "addForInputs" });
    else if (value === "collapse") collapseAll();
    else if (value === "storePreset") Hub.send("trackMenu", { action: "storePreset", ids: presetIds() });
    else if (value === "loadPreset") Hub.send("trackMenu", { action: "loadPreset" });
  }

  Hub.trackMenu = { items: trackMenuItems, run: runTrackMenu };

  // ---------------------------------------------------------------- Add Tracks (Studio One's dialog)
  const addTracksChoice = { name: "", count: 1, type: "instrument", format: "stereo", input: -1, ascending: false, instrument: BUILTIN_KEY };

  function showAddTracksRows() {
    const type = $("add-tracks-type").value;
    document.querySelectorAll("#add-tracks [data-for]").forEach((row) => { row.hidden = !row.dataset.for.split(" ").includes(type); });
  }

  function openAddTracks() {
    const inputs = $("add-tracks-input");
    inputs.textContent = "";
    inputs.add(new Option("As a new track has it", "-1"));
    (Hub.state.inputs || []).forEach((input, index) => inputs.add(new Option(input.name, String(index))));

    const instruments = $("add-tracks-instrument");
    instruments.textContent = "";
    instruments.add(new Option("Cayman", BUILTIN_KEY));
    for (const entry of Hub.state.library.filter((item) => item.kind !== "effect" && !item.missing && item.id !== TEST_SYNTH_ID))
      instruments.add(new Option(entry.name, pluginKey(entry.file, entry.id)));

    const choice = addTracksChoice;
    $("add-tracks-name").value = choice.name;
    $("add-tracks-count").value = String(choice.count);
    $("add-tracks-type").value = choice.type;
    $("add-tracks-format").value = choice.format;
    inputs.value = [...inputs.options].some((option) => option.value === String(choice.input)) ? String(choice.input) : "-1";
    $("add-tracks-ascending").checked = choice.ascending;
    instruments.value = [...instruments.options].some((option) => option.value === choice.instrument) ? choice.instrument : BUILTIN_KEY;
    showAddTracksRows();
    $("add-tracks").hidden = false;
    $("add-tracks-name").focus();
  }

  function addTracks() {
    const choice = addTracksChoice;
    choice.name = $("add-tracks-name").value.trim();
    choice.count = Hub.clamp(Math.round(Number($("add-tracks-count").value)) || 1, 1, 64);
    choice.type = $("add-tracks-type").value;
    choice.format = $("add-tracks-format").value;
    choice.input = Number($("add-tracks-input").value);
    choice.ascending = $("add-tracks-ascending").checked;
    choice.instrument = $("add-tracks-instrument").value;

    const message = { action: "addTracks", type: choice.type, count: choice.count, name: choice.name };
    if (choice.type === "audio" || choice.type === "choir") Object.assign(message, { mono: choice.format === "mono", input: choice.input, ascending: choice.ascending });
    if (choice.type === "instrument" && choice.instrument !== BUILTIN_KEY) {
      const entry = Hub.state.library.find((item) => pluginKey(item.file, item.id) === choice.instrument);
      if (entry) Object.assign(message, { file: entry.file, pluginId: entry.id });
    }
    Hub.send("trackMenu", message);
    $("add-tracks").hidden = true;
  }

  // ---------------------------------------------------------------- commands, events
  Hub.commands.addTrack = () => Hub.send("addTrack");
  Hub.commands.addAudioTrack = () => Hub.send("addTrack", { kind: "audio" });
  Hub.commands.addTracks = openAddTracks;

  function renderAll() {
    renderTrackList();   // also redraws the lanes, so the selection shows there too
  }

  Hub.on("tracks", renderAll);
  Hub.on("selection", renderAll);

  // A fader moved somewhere else (the Console): show it here too.
  Hub.on("trackValue", ({ id, source }) => {
    const row = rows.get(id);
    const track = Hub.trackById(id);
    if (row && track && !row.contains(source)) showFaders(row, track);
  });
  Hub.on("library", renderTrackList);

  /** How far a meter line reaches for a level (0 to 1: -60 dB to 0 dB). */
  const meterWidth = (level) => Hub.clamp01((20 * Math.log10(Math.max(level, 1e-6)) + 60) / 60) * 100 + "%";

  Hub.on("status", (s) => {
    for (const meter of s.trackMeters || []) {
      const row = rows.get(meter.id);
      if (!row) continue;
      // Two lines, left and right: what the track sends out, mono (the same on both, two same
      // monos) or stereo. An audio track armed but not monitoring shows its input as the channel
      // hears it, to set the level before recording.
      const track = Hub.trackById(meter.id);
      const input = track && track.kind === "audio" && track.recordArm && !track.monitor;
      const [left, right] = row.querySelectorAll(".track-meter i");
      left.style.setProperty("--level", meterWidth(Math.max(meter.left || 0, input ? meter.inputLeft || 0 : 0)));
      right.style.setProperty("--level", meterWidth(Math.max(meter.right || 0, input ? meter.inputRight || 0 : 0)));

      // Automation moves the fader and pan while the song plays.
      if (track && meter.volume !== undefined) {
        track.volume = meter.volume;
        track.pan = meter.pan;
        showFaders(row, track);
      }
    }
  });

  // A track freezing: its snowflake fills as the render goes.
  Hub.on("status", (s) => {
    const freezing = s.freezing || 0;
    for (const [id, row] of rows) {
      const on = id === freezing;
      if (!on && !row.classList.contains("freezing")) continue;
      row.classList.toggle("freezing", on);
      const freeze = row.querySelector(".freeze");
      if (on) {
        const percent = Math.round(Hub.clamp01(s.freezeProgress || 0) * 100);
        freeze.style.setProperty("--progress", percent + "%");
        freeze.title = `Freezing: ${percent}%. Click to stop.`;
      } else {
        freeze.style.removeProperty("--progress");
        const track = Hub.trackById(id);
        if (track) updateRow(row, track);
      }
    }
  });

  // An open MIDI list follows ports turned on or off, coming or going (Settings' own handler
  // takes the new settings first, so its list is read once this event is through).
  const followMidiMenu = () => { if (refreshMidiMenu && midiMenuData() !== midiMenuShown) refreshMidiMenu(); };
  Hub.on("status", followMidiMenu);
  Hub.on("settings", () => setTimeout(followMidiMenu, 0));
  Hub.on("status", sendPreferredSource);
  Hub.on("settings", () => setTimeout(sendPreferredSource, 0));
  Hub.on("tracks", () => { if (filterMenuRender && closeFilterMenu) filterMenuRender(); });
  Hub.on("selection", () => { if (filterMenuRender && closeFilterMenu) filterMenuRender(); });

  Hub.on("ready", () => {
    setupTrackList();
    setupDropTargets();
    $("track-filter").addEventListener("click", (event) => { event.stopPropagation(); openFilterMenu($("track-filter")); });

    $("add-tracks-type").addEventListener("change", showAddTracksRows);
    $("add-tracks-ok").addEventListener("click", addTracks);
    $("add-tracks-cancel").addEventListener("click", () => { $("add-tracks").hidden = true; });
    $("add-tracks").addEventListener("keydown", (event) => {
      if (event.key === "Escape") { event.preventDefault(); event.stopPropagation(); $("add-tracks").hidden = true; }
      else if (event.key === "Enter" && !event.target.closest("select")) { event.preventDefault(); addTracks(); }
    });

    renderAll();
  });
})();
