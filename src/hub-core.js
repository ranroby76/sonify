// D:\workspace\Fanan_web_hub\src\hub-core.js
//
// Shared by every part of the page: the bridge to the native engine, shared state,
// the fader law, menus, the panel toggles (Browser, Console, Editor) and the look
// (Settings > Appearance). Keyboard shortcuts are in hub-keys.js.

(() => {
  "use strict";

  const backend = window.__JUCE__ && window.__JUCE__.backend;
  const handlers = {};

  // Inputs that don't take text: the DAW's shortcuts still work while one of these has focus.
  const NOT_TEXT_INPUTS = new Set(["button", "checkbox", "radio", "range", "color", "file", "submit", "reset", "image", "hidden"]);

  const Hub = window.Hub = {
    state: { tracks: [], selected: 0, selectedTracks: [], inspectMain: false, selectedClips: [], focus: "arrange", master: { volume: 0, mute: false, inserts: [] },
             inputs: [], inputChannels: [], recordBits: 24, peaks: {}, finePeaks: {},
             outputs: [{ id: 0, name: "Main", left: 0, right: 1, mainMix: false, volume: 0, mute: false }], outputChannels: [],
             audition: 0, ioDefaults: null,
             library: [], transport: null, status: null },

    /** Channel 0 is the Main channel in every message to and from the engine. */
    MAIN: 0,
    commands: {},

    $: (id) => document.getElementById(id),

    /** Sends an event to the native engine. */
    send(eventId, payload = {}) {
      if (backend) backend.emitEvent(eventId, payload);
    },

    /** Listens to engine events (status, tracks, library, param) and page events. */
    on(eventId, handler) {
      (handlers[eventId] = handlers[eventId] || []).push(handler);
    },

    /** Each listener runs on its own: an error in one part of the page can't stop the others. */
    emit(eventId, data) {
      for (const handler of handlers[eventId] || []) {
        try {
          handler(data);
        } catch (error) {
          console.error(`Hub: a "${eventId}" listener failed`, error);
        }
      }
    },

    run(command) {
      const action = Hub.commands[command];
      if (action) action();
    },

    clamp: (value, min, max) => Math.min(max, Math.max(min, value)),
    clamp01: (value) => Math.min(1, Math.max(0, value)),

    /** Typing into a field must not play notes or trigger shortcuts. */
    /** Is the keyboard writing text here (a sign-in form, a name, the AI box)? Then the DAW's
        shortcuts keep out of its way. Every field that takes text counts: email, password,
        codes, numbers, addresses. Buttons, boxes and sliders don't, so Space still plays. */
    isTyping(target) {
      if (!(target instanceof Element)) return false;
      if (target.isContentEditable || target instanceof HTMLTextAreaElement) return true;
      return target instanceof HTMLInputElement && !NOT_TEXT_INPUTS.has(target.type);
    },

    element(tag, className, text) {
      const node = document.createElement(tag);
      if (className) node.className = className;
      if (text !== undefined) node.textContent = text;
      return node;
    },

    /** A clip and its track, by the clip's id (ids are unique across tracks). */
    findClip(clipId) {
      for (const track of Hub.state.tracks) {
        const clip = (track.clips || []).find((c) => c.id === clipId);
        if (clip) return { track, clip };
      }
      return null;
    },

    selectedTrack() {
      return Hub.state.tracks.find((track) => track.id === Hub.state.selected) || null;
    },

    trackById(id) {
      return Hub.state.tracks.find((track) => track.id === id)
          || (Hub.state.busChannels || []).find((channel) => channel.id === id)
          || (Hub.state.fxChannels || []).find((channel) => channel.id === id) || null;
    },

    dbText(db) {
      return db <= Hub.SILENT_DB ? "-∞ dB" : `${db > 0 ? "+" : ""}${db.toFixed(1)} dB`;
    },

    /** Changes one fader setting of a track and tells every view (header, Console) about it.
        A fader or pan of a selected channel moves the rest of the selection with it, by as much
        (in dB for faders), each from where it was when the drag began. */
    setTrackValue(id, key, value, source) {
      const track = Hub.trackById(id);
      const before = track ? track[key] : value;
      if (track) track[key] = value;
      Hub.send("trackSettings", { id, [key]: value });
      Hub.emit("trackValue", { id, key, value, source });

      const group = key === "volume" || key === "pan" ? Hub.groupOf(id) : [id];
      if (group.length < 2) return;
      const base = groupBase && groupBase.id === id ? groupBase.values : null;
      const delta = value - (base && base.has(id) ? base.get(id)[key] : before);

      for (const other of group) {
        const channel = other !== id && Hub.trackById(other);
        if (!channel) continue;
        const from = base && base.has(other) ? base.get(other)[key] : channel[key];
        const next = key === "pan" ? Hub.clamp(from + delta, -1, 1) : Hub.clamp(from + delta, OFF_DB, Hub.MAX_DB);
        channel[key] = next;
        Hub.send("trackSettings", { id: other, [key]: next });
        Hub.emit("trackValue", { id: other, key, value: next, source: null });
      }
    },

    /** The Console's order: tracks, buses, then FX channels (ids). */
    channelOrder() {
      return [...Hub.state.tracks.map((track) => track.id), ...(Hub.state.busChannels || []).map((channel) => channel.id),
              ...(Hub.state.fxChannels || []).map((channel) => channel.id)];
    },

    /** Whether a track or channel is in the selection (Main in focus: none is). */
    isTrackSelected(id) {
      return !Hub.state.inspectMain && Hub.state.selectedTracks.includes(id);
    },

    /** What a click on a channel's button works on: the whole selection when the channel is in
        it, otherwise the channel alone. In the Console's order. */
    groupOf(id) {
      const selection = Hub.state.selectedTracks;
      return selection.length > 1 && selection.includes(id) ? selection.slice() : [id];
    },

    setMasterValue(key, value, source) {
      Hub.state.master[key] = value;
      Hub.send("masterSettings", { [key]: value });
      Hub.emit("masterValue", { key, value, source });
    },

    /**
        Selects a track or channel at once on the page; the engine confirms with a tracks update.
          how "only"     it alone (a click)
              "toggle"   in or out of the selection, the rest kept (Ctrl + click)
              "range"    every channel from the last one clicked to it added (Shift + click)
              "keep"     the selection as it is when it's in it, else it alone (a clip clicked)
        The keyboard plays the track clicked last (an FX channel or a bus plays nothing: the track stays).
    */
    selectTrack(id, how = "only") {
      const track = id ? Hub.trackById(id) : null;
      if (!track) return;
      const order = Hub.channelOrder();
      let chosen = Hub.state.selectedTracks.filter((other) => order.includes(other));

      if (how === "toggle") {
        if (!chosen.includes(id)) chosen.push(id);
        else if (chosen.length > 1) chosen = chosen.filter((other) => other !== id);
        selectionAnchor = id;
      } else if (how === "range") {
        const from = order.indexOf(order.includes(selectionAnchor) ? selectionAnchor : Hub.state.selected);
        const to = order.indexOf(id);
        if (from < 0) chosen = [id];
        else for (const other of order.slice(Math.min(from, to), Math.max(from, to) + 1)) if (!chosen.includes(other)) chosen.push(other);
      } else if (how !== "keep" || !chosen.includes(id)) {
        chosen = [id];
        selectionAnchor = id;
      }

      Hub.state.selectedTracks = order.filter((other) => chosen.includes(other));

      // Only a track plays notes: FX channels and buses join the selection, the keyboard stays.
      const plays = (channel) => Boolean(channel) && channel.kind !== "fx" && channel.kind !== "bus";
      let primary = Hub.state.selected;
      if (plays(track) && chosen.includes(id)) primary = id;
      else if (!chosen.includes(primary)) {
        const playable = chosen.map((other) => Hub.trackById(other)).find(plays);
        if (playable) primary = playable.id;
      }

      const trackChanged = primary !== Hub.state.selected;
      Hub.state.inspectMain = false;
      Hub.state.selected = primary;
      Hub.emit("selection");
      if (trackChanged) Hub.send("selectTrack", { id: primary });
    },

    /** Marks the Main channel as the one in focus (its strip shows it). The keyboard keeps playing the selected track. */
    inspectMain() {
      if (Hub.state.inspectMain) return;
      Hub.state.inspectMain = true;
      Hub.emit("selection");
    },

    /** The inserts of a channel: a track id, or Hub.MAIN. */
    insertsOf(channel) {
      if (channel === Hub.MAIN) return Hub.state.master.inserts || [];
      const track = Hub.trackById(channel);
      return track ? track.inserts || [] : [];
    },

    effects() {
      return Hub.state.library.filter((entry) => entry.kind === "effect" && !entry.missing)
                              .sort((a, b) => a.vendor.localeCompare(b.vendor) || a.name.localeCompare(b.name));
    },

    /** A small menu under anchor (an element, or anything with getBoundingClientRect). Items:
        { label, value } picked with a click (onPick(value)), and as each needs: disabled, checked
        (a tick before it), icon (an SVG's markup, beside the tick), shortcut (its key, on the
        right), submenu (items of its own, opened beside it as the pointer comes over it; a click
        on the item itself still picks its value, when it has one); { separator: true } a line;
        { group } a heading. */
    popupMenu(anchor, items, onPick) {
      document.querySelectorAll(".popup-menu").forEach((menu) => menu.remove());
      const open = [];   // the menu, then each submenu opened from the one before it

      const close = () => {
        for (const menu of open) menu.remove();
        open.length = 0;
        document.removeEventListener("pointerdown", outside, true);
        window.removeEventListener("keydown", escape, true);
        window.removeEventListener("blur", close);
      };
      const outside = (event) => { if (!open.some((menu) => menu.contains(event.target))) close(); };
      const escape = (event) => { if (event.key === "Escape") { event.stopPropagation(); close(); } };

      /** Places a menu below (or above) a rectangle, or beside it for a submenu, inside the window. */
      const place = (menu, r, beside) => {
        const width = menu.offsetWidth || 220, height = menu.offsetHeight || 0;
        let left = beside ? r.right - 2 : r.left;
        let top = beside ? r.top - 4 : r.bottom + 2;
        if (beside && left + width > window.innerWidth - 4) left = r.left - width + 2;
        if (!beside && top + height > window.innerHeight - 4) top = Math.max(4, r.top - height - 2);
        if (beside && top + height > window.innerHeight - 4) top = Math.max(4, window.innerHeight - height - 4);
        menu.style.left = Math.max(4, Math.min(window.innerWidth - width - 4, left)) + "px";
        menu.style.top = top + "px";
      };

      const build = (list, level) => {
        const menu = document.createElement("div");
        menu.className = "popup-menu" + (level ? " popup-sub" : "");
        menu.setAttribute("role", "menu");
        const marks = list.some((item) => item.checked !== undefined || item.icon);
        if (marks) menu.classList.add("with-marks");

        const closeFrom = (depth) => { while (open.length > depth) open.pop().remove(); };

        for (const item of list) {
          if (item.group) {
            menu.appendChild(Hub.element("div", "popup-group", item.group));
            continue;
          }
          if (item.separator) {
            menu.appendChild(Hub.element("div", "popup-separator"));
            continue;
          }

          const button = Hub.element("button", item.submenu ? "has-submenu" : "");
          button.type = "button";
          button.setAttribute("role", "menuitem");
          button.disabled = Boolean(item.disabled);
          if (item.title) button.title = item.title;

          if (marks) {
            button.appendChild(Hub.element("span", "popup-check", item.checked ? "\u2713" : ""));
            const icon = Hub.element("span", "popup-icon");
            if (item.icon) icon.innerHTML = item.icon;
            button.appendChild(icon);
          }
          button.appendChild(Hub.element("span", "popup-label", item.label));
          if (item.shortcut) button.appendChild(Hub.element("span", "popup-key", item.shortcut));
          if (item.submenu) button.appendChild(Hub.element("span", "popup-arrow", "\u203a"));

          button.addEventListener("pointerenter", () => {
            closeFrom(level + 1);
            menu.querySelectorAll("button.open").forEach((other) => other.classList.remove("open"));
            if (!item.submenu || button.disabled) return;
            button.classList.add("open");
            const sub = build(item.submenu, level + 1);
            place(sub, button.getBoundingClientRect(), true);
          });
          button.addEventListener("click", () => {
            if (item.value === undefined && item.submenu) return;   // its submenu has the choices
            close();
            onPick(item.value);
          });
          menu.appendChild(button);
        }

        document.body.appendChild(menu);
        open.push(menu);
        return menu;
      };

      const menu = build(items, 0);
      place(menu, anchor.getBoundingClientRect(), false);
      document.addEventListener("pointerdown", outside, true);
      window.addEventListener("keydown", escape, true);
      window.addEventListener("blur", close);
      return menu;
    },

    /** A point (a right-click's), as popupMenu takes an anchor. */
    pointAnchor(event) {
      return { getBoundingClientRect: () => ({ left: event.clientX, right: event.clientX, top: event.clientY, bottom: event.clientY }) };
    },

    /** The "+" menu: every effect in the Browser, grouped by vendor, added to a channel. */
    addEffectMenu(anchor, channel, position = -1) {
      const effects = Hub.effects();
      const items = [];
      let vendor = null;

      for (const entry of effects) {
        if (entry.vendor !== vendor) {
          vendor = entry.vendor;
          items.push({ group: vendor });
        }
        items.push({ label: entry.name, value: entry });
      }

      if (!effects.length) items.push({ label: "No effects yet: add WCLAP files to the Browser", value: null, disabled: true });
      if (Hub.insertsOf(channel).length >= Hub.MAX_INSERTS) items.unshift({ label: "This channel is full (16 inserts)", value: null, disabled: true });

      return Hub.popupMenu(anchor, items, (entry) => {
        if (entry) Hub.send("loadPlugin", { trackId: channel, file: entry.file, pluginId: entry.id, position });
      });
    },

    /** Turns a label into a text field; Enter or leaving it commits, Escape cancels. */
    inlineRename(label, current, commit) {
      const input = document.createElement("input");
      input.type = "text";
      input.value = current;
      input.setAttribute("aria-label", "Name");
      label.textContent = "";
      label.appendChild(input);
      input.focus();
      input.select();

      // Once: taking the field away blurs it, which would finish it again.
      let finished = false;
      const finish = (save) => {
        if (finished || !input.isConnected) return;
        finished = true;
        const value = input.value.trim();
        input.remove();
        label.textContent = save && value ? value : current;
        if (save && value && value !== current) commit(value);
      };

      input.addEventListener("keydown", (event) => {
        if (event.key === "Enter") finish(true);
        if (event.key === "Escape") finish(false);
        event.stopPropagation();
      });
      input.addEventListener("blur", () => finish(true));
    },

    panText(pan) {
      if (Math.abs(pan) < 0.005) return "<C>";
      const amount = Math.round(Math.abs(pan) * 100);
      return pan < 0 ? `${amount}L` : `${amount}R`;
    },

    toast(text) {
      const toast = Hub.$("toast");
      if (!text) return;
      toast.textContent = text;
      toast.hidden = false;
      clearTimeout(Hub._toastTimer);
      Hub._toastTimer = setTimeout(() => { toast.hidden = true; }, 7000);
    },

    start() {
      if (backend) {
        for (const id of ["status", "tracks", "library", "param", "song", "songFolder", "audioPeaks", "audioPeaksFine", "folderListing", "audioFileInfo", "pluginScreen", "screenMessages", "mixdown", "mixdownFolder", "webTabOpened", "webTabState", "webTabClosed", "webTabsRestore", "account", "fananContent", "contentProgress", "contentResult", "contentPreviewing", "pluginChoice", "pluginScan", "internetBlocked", "aiState", "voiceState", "remoteState", "settings", "audioSetup", "runCommand", "mastering", "sessionSetup", "sessionChords", "sessionPlayers", "stems", "midiLink", "selectTracks", "songChannel", "audioJob", "audioTempo", "audioAnalysis", "silenceAreas", "transients", "midiFxTypes", "midiFxAdded", "midiToolResult", "magentaRequest", "midiTools", "midiToolProgress", "midiToolInstalled", "songBlocks", "songBlockMade", "songBlocksBuilt", "songLive", "pluginParams", "takeClips", "songVideo", "videoExport", "deviceStudio", "deviceStudioMessage", "deviceAiState", "deviceLive", "board", "live", "creator", "sampling"])
          backend.addEventListener(id, (data) => Hub.emit(id, data));
      }

      // While the page loads, the window covers it with Fanan's and Sonify's logos. Once the page
      // has drawn itself as the user set it up (the first settings bring Settings > Appearance),
      // it says so and the cover goes.
      let shown = false;
      Hub.on("settings", () => {
        if (shown) return;
        shown = true;
        requestAnimationFrame(() => requestAnimationFrame(() => Hub.send("uiShown")));
      });

      Hub.emit("ready");
      Hub.send("uiReady");
    }
  };

  // The clip selection is a list of clip ids; selectedClip is the one picked last, as
  // {trackId, clipId}, for anything that works on a single clip (the Editor's F2).
  Object.defineProperty(Hub.state, "selectedClip", {
    get() {
      const id = this.selectedClips[this.selectedClips.length - 1];
      const found = id === undefined ? null : Hub.findClip(id);
      return found ? { trackId: found.track.id, clipId: id } : null;
    },
    set(value) {
      this.selectedClips = value ? [value.clipId] : [];
    }
  });

  // Finer waveforms arrive when a zoomed-in view asks for them.
  Hub.on("audioPeaksFine", (data) => {
    if (!data || !data.file) return;
    Hub.state.finePeaks[data.file] = data;
    if (Hub.arrange) Hub.arrange.redraw();
  });

  // Waveforms arrive once per audio file.
  Hub.on("audioPeaks", (data) => {
    if (!data || !data.file) return;
    Hub.state.peaks[data.file] = data;
    if (Hub.arrange) Hub.arrange.redraw();
  });

  // Plugins' parameter lists come once each (pluginParams), and the track state names them by
  // key: they're put back in place here, before any other part of the page sees the state.
  const paramLists = new Map();

  Hub.on("pluginParams", (data) => {
    for (const [key, list] of Object.entries((data && data.params) || {})) paramLists.set(key, list);
  });

  function attachParams(data) {
    const named = new Set();
    const attach = (plugin) => {
      if (!plugin || typeof plugin !== "object") return;
      if (plugin.paramsKey !== undefined) {
        plugin.params = paramLists.get(plugin.paramsKey) || [];
        named.add(plugin.paramsKey);
      }
      for (const node of plugin.nodes || []) attach(node);   // a container's plugins
    };
    const channel = (track) => {
      attach(track.instrument);
      for (const insert of track.inserts || []) attach(insert);
      for (const clip of track.clips || []) attach(clip.effect);
    };
    for (const track of [...(data.tracks || []), ...(data.fxChannels || []), ...(data.busChannels || [])]) channel(track);
    if (data.master) for (const insert of data.master.inserts || []) attach(insert);
    for (const key of [...paramLists.keys()]) if (!named.has(key)) paramLists.delete(key);   // lists no longer used
  }

  // A take recording: only its tracks' clips, a few times a second (the whole state comes as it ends).
  Hub.on("takeClips", (data) => {
    for (const item of (data && data.tracks) || []) {
      const track = Hub.trackById(item.id);
      if (!track) continue;
      track.clips = item.clips || [];
      for (const clip of track.clips) if (clip.effect && clip.effect.paramsKey !== undefined) clip.effect.params = paramLists.get(clip.effect.paramsKey) || [];
    }
    Hub.emit("takeGrew", data);
  });

  Hub.on("tracks", (data) => {
    attachParams(data);
    const selectedBefore = Hub.state.selected;
    Hub.state.tracks = data.tracks || [];
    Hub.state.fxChannels = data.fxChannels || [];            // in the Console only
    Hub.state.busChannels = data.busChannels || [];          // in the Console only
    Hub.state.sidechainTargets = data.sidechainTargets || []; // plugins with a sidechain input
    Hub.state.inputs = data.inputs || Hub.state.inputs;
    Hub.state.inputChannels = data.inputChannels || Hub.state.inputChannels;
    Hub.state.outputs = data.outputs && data.outputs.length ? data.outputs : Hub.state.outputs;   // Main first
    Hub.state.outputChannels = data.outputChannels || Hub.state.outputChannels;
    if (data.audition !== undefined) Hub.state.audition = data.audition;
    Hub.state.ioDefaults = data.ioDefaults || Hub.state.ioDefaults;
    Hub.state.recordBits = data.recordBits || Hub.state.recordBits;
    Hub.state.selected = data.selected || 0;
    if (data.master) Hub.state.master = data.master;
    Hub.state.boardMixing = Boolean(data.boardMixing);   // the Analog Board mixes the song, not the Console
    if (data.notice) Hub.toast(data.notice);

    // The selection keeps the channels still there; the engine selecting another track (a new
    // one, the AI's) starts it afresh with that one.
    const order = Hub.channelOrder();
    let chosen = Hub.state.selectedTracks.filter((id) => order.includes(id));
    if (Hub.state.selected && Hub.state.selected !== selectedBefore && !chosen.includes(Hub.state.selected)) chosen = [Hub.state.selected];
    if (!chosen.length && Hub.state.selected) chosen = [Hub.state.selected];
    Hub.state.selectedTracks = order.filter((id) => chosen.includes(id));
  });

  // Copies just made (Duplicate): they're the selection.
  Hub.on("selectTracks", (data) => {
    const order = Hub.channelOrder();
    const chosen = (data.ids || []).filter((id) => order.includes(id));
    if (!chosen.length) return;
    Hub.state.selectedTracks = order.filter((id) => chosen.includes(id));
    selectionAnchor = chosen[0];
    Hub.state.inspectMain = false;
    Hub.emit("selection");
  });

  // ---------------------------------------------------------------- the web view's own menu
  // A right-click never shows the web view's menu (Back, Refresh, Save as, Print): the page's own
  // menus open where there are some, and nothing elsewhere. A text field keeps its Cut, Copy and
  // Paste.
  const isTextField = (el) => Boolean(el && el.closest && el.closest("textarea, [contenteditable=''], [contenteditable='true'], input"))
    && !(el.closest("input") && NOT_TEXT_INPUTS.has((el.closest("input").type || "").toLowerCase()));
  window.addEventListener("contextmenu", (event) => {
    if (!isTextField(event.target)) event.preventDefault();
  });

  // Where a Shift + click's range starts, and the selection's values as a fader drag began.
  let selectionAnchor = 0;
  let groupBase = null;

  document.addEventListener("pointerdown", (event) => {
    groupBase = null;
    const control = event.target.closest && event.target.closest('.fader, [data-control="volume"], [data-control="pan"]');
    const holder = control && control.closest(".track[data-id], .strip[data-id]");
    if (!holder) return;
    const id = Number(holder.dataset.id);
    const group = Hub.groupOf(id);
    if (group.length < 2) return;
    groupBase = { id, values: new Map(group.map((other) => {
      const channel = Hub.trackById(other) || {};
      return [other, { volume: channel.volume || 0, pan: channel.pan || 0 }];
    })) };
  }, true);

  // ---------------------------------------------------------------- fader law
  // Studio One's fader scale: +10 dB at the top, 0 dB about three quarters up,
  // then ever wider steps down to -72 dB and off.
  Hub.MAX_INSERTS = 16;
  Hub.SILENT_DB = -72;
  Hub.MAX_DB = 10;
  const OFF_DB = -144;   // what the page sends for "off": below -72 the engine is silent
  const LAW = [[0, OFF_DB], [0.05, -72], [0.14, -48], [0.24, -36], [0.36, -24], [0.52, -12],
               [0.64, -6], [0.78, 0], [0.91, 6], [1, 10]];

  /** Fader position (0 bottom .. 1 top) to dB. */
  Hub.faderToDb = (position) => {
    const p = Hub.clamp01(position);
    for (let i = 1; i < LAW.length; i++) {
      const [p1, d1] = LAW[i];
      if (p <= p1) {
        const [p0, d0] = LAW[i - 1];
        return d0 + (d1 - d0) * (p - p0) / (p1 - p0);
      }
    }
    return Hub.MAX_DB;
  };

  /** dB to fader position; also used to place meter levels on the same scale. */
  Hub.dbToFader = (db) => {
    if (!(db > OFF_DB)) return 0;
    for (let i = 1; i < LAW.length; i++) {
      const [p1, d1] = LAW[i];
      if (db <= d1) {
        const [p0, d0] = LAW[i - 1];
        return p0 + (p1 - p0) * (db - d0) / (d1 - d0);
      }
    }
    return 1;
  };

  Hub.gainToDb = (gain) => gain > 0 ? 20 * Math.log10(gain) : OFF_DB;

  Hub.on("library", (data) => {
    // Plugins hidden in the Plug-In Manager are offered nowhere; it alone lists them all.
    Hub.state.libraryAll = data.entries || [];
    Hub.state.library = Hub.state.libraryAll.filter((entry) => !entry.hidden);
    if (data.notice) Hub.toast(data.notice);
  });

  Hub.on("status", (data) => {
    Hub.state.status = data;
    if (data.transport) Hub.state.transport = data.transport;
    if (typeof data.masterVolume === "number") Hub.state.master.volume = data.masterVolume;   // Main's fader moved on a control surface
  });

  // A control surface asks for a page command: zooming with its jog wheel.
  Hub.on("runCommand", (data) => {
    if (data && ["zoomIn", "zoomOut", "zoomFull"].includes(data.command)) Hub.run(data.command);
  });

  // ---------------------------------------------------------------- panels
  const PANELS = { toggleBrowser: "hide-browser", toggleEditor: "hide-editor",
                   toggleConsole: "hide-console", toggleAudioEditor: "hide-audio-editor" };

  // Like Studio One, the Editor, the Audio Editor and the Console share the space at the bottom.
  const SHARES_WITH = { toggleEditor: ["hide-console", "hide-audio-editor"], toggleConsole: ["hide-editor", "hide-audio-editor"],
                        toggleAudioEditor: ["hide-editor", "hide-console"] };

  for (const [command, bodyClass] of Object.entries(PANELS)) {
    Hub.commands[command] = () => {
      const hidden = document.body.classList.toggle(bodyClass);
      if (!hidden && SHARES_WITH[command]) document.body.classList.add(...SHARES_WITH[command]);
      Hub.emit("layout");
    };
  }

  function syncPanelButtons() {
    for (const button of document.querySelectorAll("[data-panel]")) {
      const hidden = document.body.classList.contains(PANELS[button.dataset.panel]);
      button.setAttribute("aria-pressed", String(!hidden));
    }
  }

  Hub.on("layout", syncPanelButtons);

  // ---------------------------------------------------------------- appearance
  // Settings > Appearance. Luminance takes the arrangement's lanes and the Editor's piano roll
  // from dark to bright, as Studio One's does; Contrast sets how bright text and symbols are
  // against the dark. The canvases draw with Hub.theme; the rest of the page uses the CSS
  // variables. 0 and 50 are the look Sonify always had.
  const rgb = (colour) => [1, 3, 5].map((i) => parseInt(colour.slice(i, i + 2), 16));

  /** From a (t = 0) to b (t = 1). */
  function mix(a, b, t) {
    const x = rgb(a), y = rgb(b);
    return "#" + x.map((v, i) => Math.round(v + (y[i] - v) * Hub.clamp01(t)).toString(16).padStart(2, "0")).join("");
  }

  /** a at 0, b at 0.5 (the default), c at 1. */
  const mix3 = (a, b, c, t) => (t <= 0.5 ? mix(a, b, t * 2) : mix(b, c, (t - 0.5) * 2));

  function makeTheme(luminance, contrast) {
    const l = Hub.clamp01(luminance / 100), c = Hub.clamp01(contrast / 100);
    return {
      luminance, contrast,
      text: mix3("#a9abaf", "#d6d7d9", "#ffffff", c),
      bright: mix3("#c3c5c8", "#eceded", "#ffffff", c),
      muted: mix3("#6e7278", "#8e939a", "#b6bac0", c),
      line: mix3("#323438", "#3b3d42", "#4f5258", c),
      line2: mix3("#3c3e43", "#47494f", "#5d6067", c),
      tick: mix3("#45474d", "#55585e", "#6c6f76", c),
      lanes: {
        empty: mix("#26272a", "#b4b7bc", l),
        lane: mix("#2c2d31", "#c4c7cb", l),
        selected: mix("#3b3e44", "#d6d9dc", l),
        armed: mix("#35272a", "#dab9bb", l),
        bar: mix("#46484e", "#85888f", l),
        beat: mix("#36383c", "#a0a3a9", l),
        fine: mix("#2f3134", "#b1b4b9", l),
        mark: l < 0.5 ? "#ffffff" : "#16171a"       // a split line, a clip being painted
      },
      roll: {
        white: mix("#2b2c30", "#d0d3d7", l),
        black: mix("#232427", "#bdc1c6", l),
        row: mix("#1f2023", "#aeb2b8", l),          // the line between every two rows
        octave: mix("#3f4147", "#989ca3", l),
        bar: mix("#4a4d53", "#868a91", l),
        beat: mix("#383a3f", "#a2a6ac", l),
        fine: mix("#303236", "#b3b6bb", l),
        outside: `rgba(0, 0, 0, ${(0.38 - 0.2 * l).toFixed(3)})`,
        velocity: mix("#222326", "#c7cacf", l),
        velocityLine: mix("#34363b", "#a9acb1", l),
        mark: l < 0.5 ? "#ffffff" : "#16171a"       // a selected note's outline
      }
    };
  }

  Hub.theme = makeTheme(0, 50);

  /** Applies Settings > Appearance at once (the settings remember it). */
  Hub.setAppearance = ({ luminance = Hub.theme.luminance, contrast = Hub.theme.contrast } = {}) => {
    const theme = Hub.theme = makeTheme(luminance, contrast);
    const root = document.documentElement.style;
    root.setProperty("--text", theme.text);
    root.setProperty("--bright", theme.bright);
    root.setProperty("--muted", theme.muted);
    root.setProperty("--line", theme.line);
    root.setProperty("--line-2", theme.line2);
    root.setProperty("--lane", theme.lanes.lane);
    root.setProperty("--lane-selected", theme.lanes.selected);
    root.setProperty("--lane-bar", theme.lanes.bar);
    root.setProperty("--roll-white", theme.roll.white);
    root.setProperty("--roll-black", theme.roll.black);
    Hub.emit("theme", theme);
  };

  // ---------------------------------------------------------------- menus
  function setupMenus() {
    const bar = Hub.$("menubar");
    let openMenu = null;

    const close = () => {
      if (!openMenu) return;
      openMenu.classList.remove("open");
      openMenu.querySelector(".menu-title").setAttribute("aria-expanded", "false");
      openMenu = null;
    };

    const open = (menu) => {
      close();
      openMenu = menu;
      menu.classList.add("open");
      menu.querySelector(".menu-title").setAttribute("aria-expanded", "true");
    };

    bar.addEventListener("click", (event) => {
      const title = event.target.closest(".menu-title");

      if (title) {
        const menu = title.parentElement;
        if (menu === openMenu) close(); else open(menu);
        return;
      }

      const item = event.target.closest("[data-command]");

      if (item && !item.disabled) {
        close();
        Hub.run(item.dataset.command);
      }
    });

    bar.addEventListener("mouseover", (event) => {
      if (!openMenu) return;
      const menu = event.target.closest(".menu");
      if (menu && menu !== openMenu) open(menu);
    });

    document.addEventListener("pointerdown", (event) => {
      if (openMenu && !bar.contains(event.target)) close();
    }, true);

    window.addEventListener("keydown", (event) => {
      if (event.key === "Escape") close();
    });

    window.addEventListener("blur", close);
  }

  // Panel toggle buttons in the transport bar
  document.addEventListener("click", (event) => {
    const button = event.target.closest("[data-panel]");
    if (button) Hub.run(button.dataset.panel);
  });

  Hub.on("ready", () => {
    setupMenus();
    syncPanelButtons();
  });
})();
