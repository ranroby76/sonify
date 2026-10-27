// D:\workspace\Fanan_web_hub\src\hub-keys.js
//
// Keyboard shortcuts in the keys of the user's DAW (File > Settings > Keyboard Shortcuts):
// Studio One's (the default), Cubase's, Logic Pro's, Pro Tools', Ableton Live's, Cakewalk's or
// FL Studio's. Each map gives Sonify's commands the keys that DAW ships with; a command that DAW
// has no key for has none here either. Logic is a Mac DAW: its Cmd is Ctrl here and Option is Alt.
// Logic picks tools with the Tool menu: T, then the tool's letter.
//
// One listener runs them all. Keys that edit clips act while the arrangement has focus, keys
// for notes while the Editor has; the rest act anywhere in the Studio. In the other studios
// (Analog Board, Device Studio, Sonify AI, Song Blocks...) the transport's keys, Undo and Redo,
// and New, Open, Save and Save As act too: the song plays and records wherever you are. None act
// while a dialog is open, or while typing in a field (except New, Open, Save and Save As). Menus
// and button tips show the keys of the map in use.

(() => {
  "use strict";

  const { $ } = Hub;

  // Sonify's commands that keys can run, in the groups the Settings list shows. Ones marked
  // only exist in some maps (FL Studio's Ctrl+Z, Live's draw mode) and are listed only there.
  const COMMANDS = [
    ["Transport", [["togglePlay", "Start / Stop"], ["returnToZero", "Return to Zero"], ["barBack", "Back One Bar"],
                   ["barForward", "Forward One Bar"], ["toggleRecord", "Record"],
                   ["toggleLoop", "Loop Active"], ["toggleMetronome", "Metronome"]]],
    ["File", [["newSong", "New Song"], ["openSong", "Open"], ["saveSong", "Save"], ["saveSongAs", "Save As"],
              ["importAudioFiles", "Import Media Files..."], ["exportMixdown", "Export Mixdown"], ["exportStems", "Export Stems"]]],
    ["Edit", [["undo", "Undo"], ["undoToggle", "Undo / Redo the Last Step", true], ["redo", "Redo"], ["cut", "Cut"], ["copy", "Copy"],
              ["paste", "Paste at Playhead"], ["duplicate", "Duplicate"], ["selectAll", "Select All"], ["deselectAll", "Deselect All"],
              ["delete", "Delete"],
              ["splitAtPlayhead", "Split at Playhead"], ["quantize", "Quantize"]]],
    ["View", [["toggleEditor", "Editor"], ["toggleConsole", "Console"], ["toggleBrowser", "Browser"],
              ["toggleSongChannel", "Master Song Channel"], ["songChannelSignature", "Master Song Channel: Signature"],
              ["songChannelKey", "Master Song Channel: Key"], ["songChannelChords", "Master Song Channel: Chords"],
              ["songChannelTempo", "Master Song Channel: Tempo"], ["chordPads", "Chord Pads"],
              ["songChannelVolume", "Master Song Channel: Volume"],
              ["toggleAutomation", "Automation"], ["zoomIn", "Zoom In"], ["zoomOut", "Zoom Out"],
              ["zoomFull", "Zoom to the Whole Song"], ["zoomReset", "Default Zoom"],
              ["zoomInVertical", "Zoom In Vertically"], ["zoomOutVertical", "Zoom Out Vertically"]]],
    ["Tools", [["toolArrow", "Arrow"], ["toolRange", "Range"], ["toolSplit", "Split"], ["toolEraser", "Eraser"],
               ["toolPaint", "Paint (Pencil)"], ["toolMute", "Mute"], ["toolListen", "Listen"],
               ["toolPaintToggle", "Draw Mode On / Off", true]]],
    ["Track", [["addTracks", "Add Tracks..."], ["addTrack", "Add Instrument Track"], ["addAudioTrack", "Add Audio Track"],
               ["addBusChannel", "Add Bus Channel"], ["addBusForSelected", "Add Bus for Selected Channels"],
               ["duplicateTracks", "Duplicate Selected Tracks"], ["removeTrack", "Remove Selected Tracks"], ["selectAllTracks", "Select All Tracks"]]]
  ];

  // The maps. Each key: modifiers first ("Ctrl+Alt+S"), a key sequence with ", " ("T, T").
  const MAPS = {
    studioOne: {
      name: "Studio One",
      keys: {
        togglePlay: ["Space"], returnToZero: ["Num .", "Home"], barBack: ["Num -"], barForward: ["Num +"],
        toggleRecord: ["Num *"], toggleLoop: ["Num /", "/"],
        toggleMetronome: ["C"],
        newSong: ["Ctrl+N"], openSong: ["Ctrl+O"], saveSong: ["Ctrl+S"], saveSongAs: ["Ctrl+Alt+S"], exportMixdown: ["Ctrl+E"],
        undo: ["Ctrl+Z"], redo: ["Ctrl+Y", "Ctrl+Shift+Z"], cut: ["Ctrl+X"], copy: ["Ctrl+C"], paste: ["Ctrl+V"],
        duplicate: ["D"], selectAll: ["Ctrl+A"], deselectAll: ["Ctrl+D"], delete: ["Delete", "Backspace"], splitAtPlayhead: ["Alt+X"],
        quantize: ["Q"],
        toggleEditor: ["F2"], toggleConsole: ["F3"], toggleBrowser: ["F5"], toggleAutomation: ["A"],
        zoomIn: ["E", "Ctrl+=", "Ctrl+Num +"], zoomOut: ["W", "Ctrl+-", "Ctrl+Num -"], zoomFull: ["Alt+Z"], zoomReset: ["Ctrl+0"],
        zoomInVertical: ["Shift+E"], zoomOutVertical: ["Shift+W"],
        toolArrow: ["1"], toolRange: ["2"], toolSplit: ["3"], toolEraser: ["4"], toolPaint: ["5"], toolMute: ["6"], toolListen: ["8"],
        addTracks: ["T"]
      }
    },
    cubase: {
      name: "Cubase",
      keys: {
        togglePlay: ["Space"], returnToZero: ["Num ."], barBack: ["Num -"], barForward: ["Num +"],
        toggleRecord: ["Num *"], toggleLoop: ["Num /"], toggleMetronome: ["C"],
        newSong: ["Ctrl+N"], openSong: ["Ctrl+O"], saveSong: ["Ctrl+S"], saveSongAs: ["Ctrl+Shift+S"],
        undo: ["Ctrl+Z"], redo: ["Ctrl+Shift+Z"], cut: ["Ctrl+X"], copy: ["Ctrl+C"], paste: ["Ctrl+V"],
        duplicate: ["Ctrl+D"], selectAll: ["Ctrl+A"], delete: ["Delete", "Backspace"], splitAtPlayhead: ["Alt+X"], quantize: ["Q"],
        toggleEditor: ["Ctrl+E"], toggleConsole: ["F3"], toggleBrowser: ["F5"],
        zoomIn: ["H"], zoomOut: ["G"], zoomFull: ["Shift+F"], zoomReset: ["Ctrl+0"],
        zoomInVertical: ["Shift+H"], zoomOutVertical: ["Shift+G"],
        toolArrow: ["1"], toolRange: ["2"], toolSplit: ["3"], toolEraser: ["5"], toolMute: ["7"], toolPaint: ["8"], toolListen: ["9"],
        deselectAll: ["Ctrl+Shift+A"]
      }
    },
    logic: {
      name: "Logic Pro",
      note: "Logic is a Mac DAW: its Cmd is Ctrl here, and Option is Alt. Tools come from the Tool menu: press T, then the tool's letter.",
      keys: {
        togglePlay: ["Space"], returnToZero: ["Enter"], barBack: [","], barForward: ["."],
        toggleRecord: ["R"], toggleLoop: ["C"], toggleMetronome: ["K"],
        newSong: ["Ctrl+N"], openSong: ["Ctrl+O"], saveSong: ["Ctrl+S"], saveSongAs: ["Ctrl+Shift+S"], exportMixdown: ["Ctrl+B"],
        undo: ["Ctrl+Z"], redo: ["Ctrl+Shift+Z"], cut: ["Ctrl+X"], copy: ["Ctrl+C"], paste: ["Ctrl+V"],
        duplicate: ["Ctrl+R"], selectAll: ["Ctrl+A"], delete: ["Backspace", "Delete"], splitAtPlayhead: ["Ctrl+T"], quantize: ["Q"],
        toggleEditor: ["P"], toggleConsole: ["X"], toggleBrowser: ["Y", "F"], toggleAutomation: ["A"],
        zoomIn: ["Ctrl+Right"], zoomOut: ["Ctrl+Left"], zoomFull: ["Z"], zoomReset: ["Ctrl+0"],
        zoomInVertical: ["Ctrl+Down"], zoomOutVertical: ["Ctrl+Up"],
        toolArrow: ["T, T"], toolRange: ["T, R"], toolSplit: ["T, I"], toolEraser: ["T, E"], toolPaint: ["T, P"], toolMute: ["T, M"],
        toolListen: ["T, S"], deselectAll: ["Ctrl+Shift+A"],
        addTrack: ["Ctrl+Alt+S"], addAudioTrack: ["Ctrl+Alt+A"]
      }
    },
    proTools: {
      name: "Pro Tools",
      keys: {
        togglePlay: ["Space", "Num 0"], returnToZero: ["Enter"], barBack: ["Num 1"], barForward: ["Num 2"],
        toggleRecord: ["Ctrl+Space", "F12", "Num 3"],
        toggleLoop: ["Ctrl+Shift+L", "Num 4"], toggleMetronome: ["Num 7"],
        newSong: ["Ctrl+N"], openSong: ["Ctrl+O"], saveSong: ["Ctrl+S"], exportMixdown: ["Ctrl+Alt+B"],
        undo: ["Ctrl+Z"], redo: ["Ctrl+Shift+Z"], cut: ["Ctrl+X"], copy: ["Ctrl+C"], paste: ["Ctrl+V"],
        duplicate: ["Ctrl+D"], selectAll: ["Ctrl+A"], delete: ["Backspace", "Delete"], splitAtPlayhead: ["Ctrl+E"], quantize: ["Alt+0"],
        toggleConsole: ["Ctrl+="], toggleBrowser: ["Alt+;"], toggleAutomation: ["Alt+-"],
        zoomIn: ["Ctrl+]"], zoomOut: ["Ctrl+["], zoomFull: ["Alt+A"],
        zoomInVertical: ["Ctrl+Alt+]"], zoomOutVertical: ["Ctrl+Alt+["],
        toolArrow: ["F8"], toolRange: ["F7"], toolListen: ["F9"], toolPaint: ["F10"],
        addTrack: ["Ctrl+Shift+N"]
      }
    },
    ableton: {
      name: "Ableton Live",
      keys: {
        togglePlay: ["Space"], returnToZero: ["Home"], toggleRecord: ["F9"], toggleLoop: ["Ctrl+L"], toggleMetronome: ["O"],
        newSong: ["Ctrl+N"], openSong: ["Ctrl+O"], saveSong: ["Ctrl+S"], saveSongAs: ["Ctrl+Shift+S"], exportMixdown: ["Ctrl+Shift+R"],
        undo: ["Ctrl+Z"], redo: ["Ctrl+Y"], cut: ["Ctrl+X"], copy: ["Ctrl+C"], paste: ["Ctrl+V"],
        duplicate: ["Ctrl+D"], selectAll: ["Ctrl+A"], delete: ["Delete", "Backspace"], splitAtPlayhead: ["Ctrl+E"], quantize: ["Ctrl+U"],
        toggleEditor: ["Ctrl+Alt+L", "Ctrl+Alt+3"], toggleConsole: ["Ctrl+Alt+M"], toggleBrowser: ["Ctrl+Alt+B", "Ctrl+Alt+5"],
        toggleAutomation: ["A"],
        zoomIn: ["=", "Shift+=", "Num +"], zoomOut: ["-", "Num -"], zoomFull: ["W"], zoomReset: ["Ctrl+0"],
        zoomInVertical: ["Alt+="], zoomOutVertical: ["Alt+-"],
        toolPaintToggle: ["B"],
        addTrack: ["Ctrl+Shift+T"], addAudioTrack: ["Ctrl+T"]
      }
    },
    cakewalk: {
      name: "Cakewalk",
      keys: {
        togglePlay: ["Space"], returnToZero: ["Ctrl+Home", "W"], toggleRecord: ["R"], toggleLoop: ["L"], toggleMetronome: ["Ctrl+F3", "F3"],
        newSong: ["Ctrl+N"], openSong: ["Ctrl+O"], saveSong: ["Ctrl+S"],
        undo: ["Ctrl+Z"], redo: ["Ctrl+Y", "Ctrl+Shift+Z"], cut: ["Ctrl+X"], copy: ["Ctrl+C", "Ctrl+Insert"], paste: ["Ctrl+V", "Shift+Insert"],
        duplicate: ["Ctrl+D"], selectAll: ["Ctrl+A"], delete: ["Delete"], splitAtPlayhead: ["S"], quantize: ["Q"],
        toggleEditor: ["Alt+3"], toggleConsole: ["Alt+2"], toggleBrowser: ["B"], toggleAutomation: ["Shift+A"],
        zoomIn: ["Ctrl+Right"], zoomOut: ["Ctrl+Left"], zoomFull: ["Shift+F", "Ctrl+Alt+F"], zoomReset: ["Ctrl+0"],
        zoomInVertical: ["Ctrl+Down"], zoomOutVertical: ["Ctrl+Up"],
        toolArrow: ["F5", "F6"], toolSplit: ["F8"], toolEraser: ["F10"], toolPaint: ["F9"], deselectAll: ["Ctrl+Shift+A"],
        addAudioTrack: ["Ctrl+T"]
      }
    },
    flStudio: {
      name: "FL Studio",
      note: "As in FL Studio, Ctrl+Z undoes the last step and, right after, redoes it; Ctrl+Alt+Z goes back step by step.",
      keys: {
        togglePlay: ["Space"], returnToZero: ["Home"], toggleRecord: ["R"], toggleMetronome: ["Ctrl+M"],
        openSong: ["Ctrl+O"], saveSong: ["Ctrl+S"], saveSongAs: ["Ctrl+Shift+S"], exportMixdown: ["Ctrl+R"],
        undo: ["Ctrl+Alt+Z"], undoToggle: ["Ctrl+Z"], cut: ["Ctrl+X"], copy: ["Ctrl+C"], paste: ["Ctrl+V"],
        duplicate: ["Ctrl+B"], selectAll: ["Ctrl+A"], delete: ["Delete"], quantize: ["Ctrl+Q"],
        toggleEditor: ["F7"], toggleConsole: ["F9"], toggleBrowser: ["Alt+F8"],
        zoomIn: ["PageUp"], zoomOut: ["PageDown"], zoomReset: ["Ctrl+0"],
        toolArrow: ["E"], toolSplit: ["C"], toolEraser: ["D"], toolPaint: ["P"], toolMute: ["T"], toolListen: ["Y"]
      }
    }
  };

  // ---------------------------------------------------------------- keys
  const NAMED = {
    Space: "Space", Enter: "Enter", Home: "Home", End: "End", PageUp: "PageUp", PageDown: "PageDown",
    Delete: "Delete", Backspace: "Backspace", Insert: "Insert", Tab: "Tab",
    Left: "ArrowLeft", Right: "ArrowRight", Up: "ArrowUp", Down: "ArrowDown",
    "=": "Equal", "-": "Minus", "[": "BracketLeft", "]": "BracketRight", ";": "Semicolon", "/": "Slash",
    ",": "Comma", ".": "Period",
    "Num *": "NumpadMultiply", "Num /": "NumpadDivide", "Num .": "NumpadDecimal", "Num +": "NumpadAdd",
    "Num -": "NumpadSubtract", "Num Enter": "NumpadEnter"
  };
  const SHOWN = { Delete: "Del", PageUp: "Page Up", PageDown: "Page Down" };

  /** "Ctrl+Alt+S" as {ctrl, alt, shift, code, label}; null for a key name it doesn't know. */
  function parseKey(text) {
    const key = { ctrl: false, alt: false, shift: false, code: "", label: "" };
    let rest = text;
    const parts = [];

    for (;;) {
      const modifier = ["Ctrl", "Alt", "Shift"].find((m) => rest.startsWith(m + "+") && rest.length > m.length + 1);
      if (!modifier) break;
      key[modifier.toLowerCase()] = true;
      parts.push(modifier);
      rest = rest.slice(modifier.length + 1);
    }

    if (/^[A-Z]$/.test(rest)) key.code = "Key" + rest;
    else if (/^[0-9]$/.test(rest)) key.code = "Digit" + rest;
    else if (/^Num [0-9]$/.test(rest)) key.code = "Numpad" + rest.slice(4);
    else if (/^F([1-9]|1[0-2])$/.test(rest)) key.code = rest;
    else if (NAMED[rest]) key.code = NAMED[rest];
    else return null;

    parts.push(SHOWN[rest] || rest);
    key.label = parts.join(" + ");
    return key;
  }

  /** A map's bindings: [{command, steps: [key, key?], label}]. */
  function bindingsOf(mapName) {
    const map = MAPS[mapName] || MAPS.studioOne;
    const list = [];

    for (const [command, specs] of Object.entries(map.keys)) {
      for (const spec of specs) {
        const steps = spec.split(", ").map(parseKey);
        if (steps.some((step) => !step)) { console.warn(`Hub keys: "${spec}" isn't a key`); continue; }
        list.push({ command, steps, label: steps.map((step) => step.label).join(", ") });
      }
    }

    return list;
  }

  let current = "studioOne";
  let bindings = bindingsOf(current);
  let pending = null;            // the first key of a sequence: {binding steps' first key, time}

  const sameKey = (key, event) => key.code === event.code && key.ctrl === (event.ctrlKey || event.metaKey)
                                  && key.alt === event.altKey && key.shift === event.shiftKey;

  // ---------------------------------------------------------------- running a command
  const editorFocused = () => Hub.editor && Hub.editor.isOpen() && Hub.state.focus === "editor";
  const ARRANGE_ONLY = new Set(["cut", "copy", "paste", "duplicate", "splitAtPlayhead"]);
  const NOTES_TOO = new Set(["cut", "copy", "paste", "duplicate"]);   // in the Editor, on its notes
  const FILE = new Set(["newSong", "openSong", "saveSong", "saveSongAs"]);
  // What acts in every studio, not only in the Studio: the transport, Undo and Redo.
  const EVERY_STUDIO = new Set(["togglePlay", "returnToZero", "barBack", "barForward", "toggleRecord", "toggleLoop", "toggleMetronome",
                                "undo", "redo", "undoToggle"]);

  /** Picks a tool in the area with focus (the arrangement's, or the Editor's: its Pencil is the
      Paint tool there). */
  const ARRANGE_TOOLS = new Set(["arrow", "range", "split", "eraser", "paint", "mute", "listen"]);

  function pickTool(name) {
    if (editorFocused()) {
      const tool = { arrow: "arrow", range: "range", split: "split", eraser: "eraser", paint: "pencil", mute: "mute", listen: "listen" }[name];
      if (tool && Hub.editor.setTool) Hub.editor.setTool(tool);
    } else if (Hub.arrange && Hub.arrange.setTool && ARRANGE_TOOLS.has(name)) {
      Hub.arrange.setTool(name);
    }
  }

  const RUN = {
    cut: () => Hub.run("cutClips"),          // notes instead while the Editor has focus (hub-editor.js)
    copy: () => Hub.run("copyClips"),
    paste: () => Hub.run("pasteClips"),
    duplicate: () => Hub.run("duplicateClips"),
    selectAll: () => Hub.run(editorFocused() ? "selectAllNotes" : "selectAllClips"),
    delete: () => Hub.run("deleteSelection"),
    quantize: () => Hub.run("quantizeNotes"),
    undoToggle: () => Hub.send("undoToggle"),
    toolArrow: () => pickTool("arrow"),
    toolRange: () => pickTool("range"),
    toolSplit: () => pickTool("split"),
    toolEraser: () => pickTool("eraser"),
    toolPaint: () => pickTool("paint"),
    toolMute: () => pickTool("mute"),
    toolListen: () => pickTool("listen"),
    toolPaintToggle: () => {
      const now = editorFocused() ? Hub.editor.state.tool : Hub.arrange.view.tool;
      pickTool(now === "paint" || now === "pencil" ? "arrow" : "paint");
    }
  };

  function run(command) {
    if (RUN[command]) RUN[command]();
    else Hub.run(command);
  }

  /** May this command run now? Not with a dialog open, not while typing (File keys excepted),
      in the other studios only File keys and the ones for every studio, clip keys only with the
      arrangement in focus. */
  function allowed(command, event) {
    if (document.querySelector(".modal:not([hidden])")) return false;
    if (!$("scan-page").hidden) return false;
    const file = FILE.has(command);
    if (Hub.isTyping(event.target)) return file;
    if (!file && !$("start-page").hidden) return false;
    if (!file && !EVERY_STUDIO.has(command) && Hub.studios && Hub.studios.active() !== "studio") return false;
    if (ARRANGE_ONLY.has(command) && Hub.state.focus !== "arrange" && !(NOTES_TOO.has(command) && editorFocused())) return false;
    return true;
  }

  // The page itself never zooms (Ctrl + = / - / 0 and Ctrl + wheel): only the arrangement does.
  const BROWSER_ZOOM = new Set(["Equal", "Minus", "Digit0", "NumpadAdd", "NumpadSubtract", "Numpad0"]);

  window.addEventListener("wheel", (event) => {
    if (event.ctrlKey) event.preventDefault();
  }, { passive: false });

  window.addEventListener("keydown", (event) => {
    if (event.ctrlKey && BROWSER_ZOOM.has(event.code)) event.preventDefault();
    if (["Control", "Alt", "Shift", "Meta"].includes(event.key)) return;

    // The second key of a sequence (Logic's T, then the tool).
    if (pending) {
      const first = pending;
      pending = null;
      hideHint();

      if (Date.now() - first.time < 2000) {
        const match = bindings.find((b) => b.steps.length === 2 && sameKey(b.steps[0], first.event) && sameKey(b.steps[1], event));
        if (match) {
          event.preventDefault();
          if (allowed(match.command, event)) run(match.command);
          return;
        }
      }
    }

    const single = bindings.find((b) => b.steps.length === 1 && sameKey(b.steps[0], event));

    if (single) {
      if (!allowed(single.command, event)) return;
      // Also keeps F5 from reloading the page, Space from clicking a focused button, and the like.
      event.preventDefault();
      if (!event.repeat || /^(zoom|tool)/.test(single.command)) run(single.command);
      return;
    }

    // The first key of a sequence: wait for the next.
    const starts = bindings.filter((b) => b.steps.length === 2 && sameKey(b.steps[0], event));
    if (starts.length && allowed(starts[0].command, event) && !event.repeat) {
      event.preventDefault();
      pending = { event: { code: event.code, ctrlKey: event.ctrlKey, metaKey: event.metaKey, altKey: event.altKey, shiftKey: event.shiftKey }, time: Date.now() };
      showHint(starts);
    }
  });

  window.addEventListener("keyup", (event) => {
    if (event.code === "Space" && !Hub.isTyping(event.target)) event.preventDefault();
  });

  // The choices after the first key of a sequence, shown for a moment.
  function showHint(starts) {
    const names = { toolArrow: "Arrow", toolRange: "Range", toolSplit: "Split", toolEraser: "Eraser", toolPaint: "Pencil", toolMute: "Mute",
                    toolListen: "Listen" };
    const text = starts.map((b) => `${b.steps[1].label} ${names[b.command] || commandName(b.command)}`).join(" · ");
    const hint = $("key-hint");
    if (!hint) return;
    hint.textContent = text;
    hint.hidden = false;
    clearTimeout(showHint.timer);
    showHint.timer = setTimeout(hideHint, 2000);
  }

  function hideHint() {
    const hint = $("key-hint");
    if (hint) hint.hidden = true;
  }

  // ---------------------------------------------------------------- what menus and buttons show
  // Menu commands named differently from the keys' commands.
  const MENU_ALIASES = { cutClips: "cut", copyClips: "copy", pasteClips: "paste", duplicateClips: "duplicate",
                         selectAllClips: "selectAll", selectAllNotes: "selectAll", deleteSelection: "delete",
                         quantizeNotes: "quantize" };
  const KNOWN = new Set(COMMANDS.flatMap(([, items]) => items.map(([id]) => id)));

  function commandName(command) {
    for (const [, items] of COMMANDS)
      for (const [id, name] of items) if (id === command) return name;
    return command;
  }

  /** The keys for a command in the map in use, as shown: "Ctrl + E" (the first) or every one. */
  function label(command, all = false) {
    const labels = bindings.filter((b) => b.command === command).map((b) => b.label);
    return all ? labels : labels[0] || "";
  }

  /** Menu shortcuts and the tips of buttons marked data-key="command", with data-tip
      "Text{key}" ({key} becomes " (Ctrl + E)", or nothing when the map has no key). */
  function decorate() {
    for (const item of document.querySelectorAll("#menubar [data-command]")) {
      const command = item.dataset.command;
      const keys = MENU_ALIASES[command] || command;
      let text;

      if (KNOWN.has(keys)) text = label(keys);
      else continue;   // not a command keys run (the studios' Ctrl + 2..7): as the menu has it

      let span = item.querySelector(".shortcut");
      if (!span && text) {
        span = Hub.element("span", "shortcut");
        item.appendChild(span);
      }
      if (span) span.textContent = text;
    }

    for (const element of document.querySelectorAll("[data-key]")) {
      const text = label(element.dataset.key);
      const tip = (element.dataset.tip || "").replace("{key}", text ? ` (${text})` : "");
      if (tip) element.title = tip;
    }
  }

  // ---------------------------------------------------------------- the API
  Hub.keys = {
    maps: () => Object.entries(MAPS).map(([id, map]) => ({ id, name: map.name, note: map.note || "" })),
    current: () => current,

    /** Uses another map at once (the settings remember it). */
    use(mapName) {
      current = MAPS[mapName] ? mapName : "studioOne";
      bindings = bindingsOf(current);
      pending = null;
      decorate();
      Hub.emit("keymap", current);
    },

    /** Every command with its keys in a map, by group, for the Settings list. */
    list(mapName = current) {
      const map = bindingsOf(mapName);
      return COMMANDS.map(([group, items]) => ({
        group,
        items: items.map(([id, name, special]) => ({ id, name, special: Boolean(special),
                                                     keys: map.filter((b) => b.command === id).map((b) => b.label) }))
                    .filter((item) => !item.special || item.keys.length)
      }));
    },

    label,
    decorate,
    parse: parseKey,

    /** True when the map in use gives this key (as it's pressed) to a command. */
    isBound: (event) => bindings.some((b) => sameKey(b.steps[0], event))
  };

  /** True for keys the map in use gives to a command without Ctrl or Alt (so they don't play notes). */
  Hub.isShortcut = (code) => bindings.some((b) => !b.steps[0].ctrl && !b.steps[0].alt && b.steps[0].code === code);

  Hub.on("ready", decorate);
})();
