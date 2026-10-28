// D:\workspace\Fanan_web_hub\src\hub-blocks.js
//
// Song Builder Blocks, a studio of its own (the top bar's selector, Ctrl+4): selected clips made into a
// block (a snapshot: a copy of them as they are, not linked to them), each block a pad on the
// page. Clicking pads puts them in the song's order (the strip under the pads); Build Song lays
// the song out on the timeline, one block after another, each clip on the track it came from.
// Every change is a step in Edit > Undo, and the blocks are kept with the song (MainBlocks.cpp,
// SongBlocks.h).
//
// Make one: in the Studio, Song > Make Block from Selected Clips or a clip's right-click (the
// Studio stays in front, and says where the block went); or here, Make from Selected Clips, with
// the clips selected in the Studio (the button counts them). A block made here is named at once.
//
// Live (the page's switch): the blocks as a clip launcher, a row for each block and a column for
// each track they have clips on. A cell launches that block on that track at the next beat, bar
// or two bars (the head's choice), going round until another is launched there or ■ stops it;
// a block's name launches all of it. Waiting cells blink, playing ones are lit. The engine plays
// them by laying them on the timeline after the song as it goes (MainBlocks.cpp), so the jam is
// the arrangement when it's done. Off, the page is as it was.
// Its cells, blocks' names, stops and Stop All take MIDI Link's pads, as every control does (drag
// the strip's last control onto one; Trigger: each press launches). They're linked by place in the
// grid ("row 2 on the 3rd track"), the same in every song, as a pad grid on a controller is.

(() => {
  "use strict";

  const { $, element } = Hub;
  const COLOURS = [["Green", "#2fb36b"], ["Blue", "#3f8fd8"], ["Amber", "#e5a93d"], ["Pink", "#d65db1"],
                   ["Red", "#e5484d"], ["Violet", "#8b6cf0"], ["Teal", "#25b5b0"], ["Olive", "#c2c95a"]];

  let data = { blocks: [], sequence: [] };
  let win = null;              // the page's parts: { pads, strip, info, clear, at, build, make }
  let shown = false;           // the page is in front
  let chosen = -1;             // the chip chosen in the song (its place), for Delete
  let renameId = 0;            // a pad just made on the page: its name is offered for typing
  let atTyped = false;         // the user typed where to build: kept until the song is built
  let padsDrawn = "";          // what the pads were drawn from (blocks and tracks), so they're only drawn again when it changes
  let live = { on: false, quantize: 1, tracks: [] };   // the engine's Live (songLive)
  let gridDrawn = "";          // what the Live grid was drawn from

  const send = (action, payload = {}) => Hub.send("songBlocks", Object.assign({ action }, payload));
  const blockById = (id) => data.blocks.find((block) => block.id === id) || null;
  const plural = (n, word) => `${n} ${word}${n === 1 ? "" : "s"}`;

  // ---------------------------------------------------------------- making one
  function makeFromSelection(name) {
    const ids = (Hub.state.selectedClips || []).slice();

    if (!ids.length) {
      Hub.toast(shown ? "Select the clips in the Studio (Ctrl+1) first, then make the block here."
                      : "Select the clips to make a block from first.");
      return;
    }

    send("make", name ? { clipIds: ids, name } : { clipIds: ids });
  }

  // ---------------------------------------------------------------- the song's order
  function setSequence(ids, label) {
    data.sequence = ids.slice();
    chosen = Math.min(chosen, data.sequence.length - 1);
    if (shown) renderSong();
    send("sequence", { ids, label });
  }

  const addToSong = (id) => setSequence([...data.sequence, id], "Add Block to the Song");

  function removeFromSong(place) {
    if (place < 0 || place >= data.sequence.length) return;
    const ids = data.sequence.slice();
    ids.splice(place, 1);
    chosen = Math.min(place, ids.length - 1);
    setSequence(ids, "Remove Block from the Song");
  }

  /** Where Build Song starts by default: the bar after the song's last clip (so nothing's covered). */
  function barAfterSong() {
    let end = 0;

    for (const track of Hub.state.tracks || [])
      for (const clip of track.clips || [])
        end = Math.max(end, clip.start + clip.length);

    if (end <= 0) return 1;
    const bar = Hub.tempoMap.barAt(end - 1e-6);
    return bar.index + 2;
  }

  function buildSong() {
    if (!data.sequence.length) {
      Hub.toast("Click pads to put their blocks in the song first.");
      return;
    }

    const bar = Math.max(1, Math.round(Number(win && win.at.value) || barAfterSong()));
    const at = Hub.tempoMap.ppqOfBar(bar - 1);
    if (Hub.arrange && Hub.arrange.expectNewClips) Hub.arrange.expectNewClips();   // they become the selection
    send("build", { at });
  }

  // ---------------------------------------------------------------- a pad's menu
  function padMenu(anchor, block) {
    Hub.popupMenu(anchor, [
      { label: "Add to the Song", value: "add" },
      { label: "Rename", value: "rename" },
      { label: "Colour…", value: "colour" },
      { label: "Duplicate", value: "duplicate" },
      { label: "Delete", value: "delete" }
    ], (pick) => {
      if (pick === "add") addToSong(block.id);
      if (pick === "rename") renamePad(block.id);
      if (pick === "duplicate") send("duplicate", { id: block.id });
      if (pick === "delete") send("remove", { id: block.id });
      if (pick === "colour") {
        Hub.popupMenu(anchor, [{ group: "Colour" }].concat(COLOURS.map(([name, value]) => ({ label: name, value, disabled: value === block.colour }))),
                      (colour) => send("edit", { id: block.id, colour }));
      }
    });
  }

  /** The pad's name, typed over (Enter or leaving it keeps it, Escape doesn't). */
  function renamePad(id) {
    if (!win) return;
    const pad = win.pads.querySelector(`[data-block="${id}"]`);
    const block = blockById(id);
    if (!pad || !block) return;

    const label = pad.querySelector(".block-name");
    const input = element("input");
    input.type = "text";
    input.value = block.name;
    input.maxLength = 60;
    input.setAttribute("aria-label", "Block name");
    label.textContent = "";
    label.appendChild(input);
    input.focus();
    input.select();

    let done = false;
    const finish = (save) => {
      if (done) return;
      done = true;
      const value = input.value.trim();
      label.textContent = save && value ? value : block.name;   // (the input goes with it)
      if (save && value && value !== block.name) send("edit", { id, name: value });
    };

    input.addEventListener("keydown", (event) => {
      if (event.key === "Enter") finish(true);
      if (event.key === "Escape") finish(false);
      event.stopPropagation();
    });
    input.addEventListener("blur", () => finish(true));
    input.addEventListener("click", (event) => event.stopPropagation());
    input.addEventListener("dblclick", (event) => event.stopPropagation());
  }

  // ---------------------------------------------------------------- drawing
  /** A pad's picture of its block: a lane per track, its clips, and their notes. */
  function drawPreview(canvas, block) {
    const scale = window.devicePixelRatio || 1;
    const width = canvas.clientWidth || 120, height = canvas.clientHeight || 40;
    canvas.width = Math.round(width * scale);
    canvas.height = Math.round(height * scale);
    const g = canvas.getContext("2d");
    if (!g) return;
    g.setTransform(scale, 0, 0, scale, 0, 0);
    g.clearRect(0, 0, width, height);

    const order = (Hub.state.tracks || []).map((t) => t.id);
    const lanes = [...new Set(block.parts.map((p) => p.trackId))].sort((a, b) => order.indexOf(a) - order.indexOf(b));
    if (!lanes.length || !(block.length > 0)) return;

    const laneHeight = height / lanes.length;
    const x = (ppq) => (ppq / block.length) * width;

    // Bar lines, faintly.
    g.fillStyle = "rgba(255,255,255,0.08)";
    for (let bar = 1; bar < block.bars; bar++) g.fillRect(Math.round((bar / block.bars) * width), 0, 1, height);

    lanes.forEach((trackId, lane) => {
      const track = Hub.trackById(trackId);
      const colour = track ? track.colour : "#777777";
      const top = lane * laneHeight;
      const parts = block.parts.filter((p) => p.trackId === trackId);
      const pitches = parts.flatMap((p) => (p.notes || []).map((n) => n[0]));
      const low = pitches.length ? Math.min(...pitches) : 0, high = pitches.length ? Math.max(...pitches) : 127;

      for (const part of parts) {
        const left = x(part.start), right = x(Math.min(block.length, part.start + part.length));
        g.globalAlpha = track ? 0.45 : 0.25;
        g.fillStyle = colour;
        g.fillRect(left, top + 1, Math.max(1, right - left), Math.max(1, laneHeight - 2));
        g.globalAlpha = 1;

        if (part.audio) {
          // An audio clip: a band across its middle.
          g.fillStyle = "rgba(255,255,255,0.55)";
          g.fillRect(left + 1, top + laneHeight / 2 - 1, Math.max(1, right - left - 2), 2);
          continue;
        }

        g.fillStyle = "rgba(255,255,255,0.85)";
        for (const [pitch, start, length] of part.notes || []) {
          const span = Math.max(1, high - low);
          const y = top + 2 + (1 - (pitch - low) / span) * Math.max(1, laneHeight - 5);
          g.fillRect(x(start), y, Math.max(1, x(start + length) - x(start) - 0.5), 1.5);
        }
      }
    });
  }

  function renderPads() {
    win.pads.textContent = "";

    if (!data.blocks.length) {
      win.pads.appendChild(element("p", "blocks-empty muted",
        "No blocks yet. In the Studio (Ctrl+1), select the clips of a whole verse or chorus, then come back and Make from Selected Clips, or right-click one of them there."));
      return;
    }

    for (const block of data.blocks) {
      const pad = element("div", "block-pad");
      pad.dataset.block = String(block.id);
      pad.tabIndex = 0;
      pad.setAttribute("role", "button");
      pad.title = "Click: add it to the song. Right-click: rename, colour, duplicate, delete.";
      pad.style.setProperty("--pad-colour", block.colour || "#2fb36b");

      const tracks = new Set(block.parts.map((p) => p.trackId)).size;
      const name = element("span", "block-name", block.name);
      name.title = "F2 or right-click: rename";
      const info = element("span", "block-info", `${plural(block.bars, "bar")} · ${plural(tracks, "track")}`);
      const preview = element("canvas", "block-preview");
      pad.append(name, info, preview);

      // A click adds it once (a double-click too: its second click is left alone).
      pad.addEventListener("click", (event) => {
        if (event.target.closest("input") || event.detail > 1) return;
        addToSong(block.id);
      });

      // Its keys are its own: not the arrangement's (Space would play, Delete would delete clips).
      pad.addEventListener("keydown", (event) => {
        if (event.target !== pad) return;
        const add = event.key === "Enter" || event.key === " ";
        if (add) addToSong(block.id);
        if (event.key === "F2") renamePad(block.id);
        if (event.key === "Delete") send("remove", { id: block.id });
        if (add || event.key === "F2" || event.key === "Delete") {
          event.preventDefault();
          event.stopPropagation();
        }
      });
      pad.addEventListener("contextmenu", (event) => {
        event.preventDefault();
        padMenu({ getBoundingClientRect: () => ({ left: event.clientX, right: event.clientX, top: event.clientY, bottom: event.clientY }) }, block);
      });

      win.pads.appendChild(pad);
      requestAnimationFrame(() => drawPreview(preview, block));
    }
  }

  function renderSong() {
    win.strip.textContent = "";
    const blocks = data.sequence.map(blockById);
    const bars = blocks.reduce((sum, block) => sum + (block ? block.bars : 0), 0);
    win.info.textContent = data.sequence.length ? `${plural(data.sequence.length, "block")} · ${plural(bars, "bar")}` : "";
    win.clear.disabled = !data.sequence.length;
    win.build.disabled = !data.sequence.length;

    if (!data.sequence.length) {
      win.strip.appendChild(element("p", "blocks-empty muted", "Click pads to put their blocks here, in the song's order."));
      return;
    }

    blocks.forEach((block, place) => {
      if (!block) return;
      const chip = element("div", "block-chip" + (place === chosen ? " chosen" : ""));
      chip.dataset.place = String(place);
      chip.style.setProperty("--pad-colour", block.colour || "#2fb36b");
      chip.style.flexGrow = String(Math.max(1, block.bars));
      chip.style.flexBasis = `${Math.max(1, block.bars) * 22}px`;
      chip.title = `${block.name}: ${plural(block.bars, "bar")}. Drag to move it; × or Delete takes it out.`;

      const label = element("span", "block-chip-name", block.name);
      const out = element("button", "block-chip-remove", "×");
      out.type = "button";
      out.setAttribute("aria-label", `Take ${block.name} out of the song`);
      out.addEventListener("click", (event) => { event.stopPropagation(); removeFromSong(place); });
      chip.append(label, out);

      chip.addEventListener("pointerdown", (event) => startChipDrag(event, chip, place));
      win.strip.appendChild(chip);
    });
  }

  /** The Make button counts the clips selected in the Studio. */
  function renderMake() {
    const count = (Hub.state.selectedClips || []).length;
    win.make.textContent = count ? `Make from ${plural(count, "Selected Clip")}` : "Make from Selected Clips";
  }

  function render(force = false) {
    if (!win || !shown) return;
    if (live.on) { if (force) gridDrawn = ""; renderGrid(); lightGrid(); return; }
    renderMake();

    // The pads again only when what they show changed (not while a name is being typed).
    const tracksKey = (Hub.state.tracks || []).map((t) => `${t.id}:${t.colour}`).join(",");
    const key = JSON.stringify(data.blocks) + "|" + tracksKey;
    const typing = win.pads.contains(document.activeElement) && document.activeElement.tagName === "INPUT";

    if ((force || key !== padsDrawn) && !typing) {
      padsDrawn = key;
      renderPads();
    }

    renderSong();
    if (!atTyped) win.at.value = String(barAfterSong());

    if (renameId && blockById(renameId)) {
      const id = renameId;
      renameId = 0;
      requestAnimationFrame(() => renamePad(id));
    }
  }

  // ---------------------------------------------------------------- moving a chip in the song
  function startChipDrag(event, chip, place) {
    if (event.button !== 0 || event.target.closest("button")) return;
    event.preventDefault();
    chosen = place;
    win.strip.querySelectorAll(".block-chip").forEach((c) => c.classList.toggle("chosen", c === chip));
    win.strip.focus();

    const startX = event.clientX;
    let moved = false, target = place;
    const marker = element("div", "block-drop");

    const move = (e) => {
      if (!moved && Math.abs(e.clientX - startX) < 5) return;
      moved = true;
      chip.classList.add("dragging");

      // Before the first chip whose middle is right of the pointer.
      const chips = [...win.strip.querySelectorAll(".block-chip")];
      target = chips.length;
      for (const other of chips) {
        const r = other.getBoundingClientRect();
        if (e.clientX < r.left + r.width / 2) { target = Number(other.dataset.place); break; }
      }

      const strip = win.strip.getBoundingClientRect();
      const ref = chips[Math.min(target, chips.length - 1)].getBoundingClientRect();
      marker.style.left = `${(target < chips.length ? ref.left : ref.right) - strip.left + win.strip.scrollLeft - 1}px`;
      if (!marker.isConnected) win.strip.appendChild(marker);
    };

    const up = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
      marker.remove();
      chip.classList.remove("dragging");
      if (!moved) return;

      const to = target > place ? target - 1 : target;
      if (to === place) return;
      const ids = data.sequence.slice();
      const [id] = ids.splice(place, 1);
      ids.splice(to, 0, id);
      chosen = to;
      setSequence(ids, "Move Block in the Song");
    };

    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  }

  // ---------------------------------------------------------------- Live
  /** The tracks the blocks have clips on, in the song's order of tracks. */
  function liveColumns() {
    const used = new Set(data.blocks.flatMap((b) => b.parts.map((p) => p.trackId)));
    return (Hub.state.tracks || []).filter((t) => used.has(t.id));
  }

  function renderGrid() {
    const grid = $("blocks-grid");
    const columns = liveColumns();
    const key = JSON.stringify([data.blocks.map((b) => [b.id, b.name, b.colour, [...new Set(b.parts.map((p) => p.trackId))]]),
                                columns.map((t) => [t.id, t.name, t.colour])]);
    if (key === gridDrawn) return;
    gridDrawn = key;
    grid.textContent = "";

    if (!data.blocks.length || !columns.length) {
      grid.style.gridTemplateColumns = "1fr";
      grid.appendChild(element("p", "blocks-empty muted", "No blocks to launch yet: make some from clips in the Studio first (switch Live off for Make)."));
      return;
    }

    grid.style.gridTemplateColumns = `minmax(120px, 180px) repeat(${columns.length}, minmax(90px, 1fr))`;
    grid.appendChild(element("div", "live-corner"));
    for (const track of columns) {
      const head = element("div", "live-col-head", track.name);
      head.style.setProperty("--track-colour", track.colour || "#777");
      head.title = track.name;
      grid.appendChild(head);
    }

    data.blocks.forEach((block, r) => {
      const row = element("button", "live-row-head");
      row.type = "button";
      row.style.setProperty("--pad-colour", block.colour || "#2fb36b");
      row.textContent = `\u25b6 ${block.name}`;
      row.title = `Launch all of ${block.name} (${plural(block.bars, "bar")}) on its tracks`;
      row.dataset.launchLink = `launch.row.${r + 1}`;
      row.addEventListener("click", () => send("launchRow", { blockId: block.id }));
      grid.appendChild(row);

      const tracks = new Set(block.parts.map((p) => p.trackId));
      columns.forEach((track, c) => {
        const has = tracks.has(track.id);
        const cell = element("button", "live-cell" + (has ? " has" : " empty"));
        cell.type = "button";
        cell.dataset.track = String(track.id);
        cell.dataset.block = String(block.id);
        cell.style.setProperty("--pad-colour", block.colour || "#2fb36b");
        cell.textContent = has ? "\u25b6" : "";
        cell.disabled = !has;
        cell.title = has ? `Launch ${block.name} on ${track.name}` : `${block.name} has nothing on ${track.name}`;
        if (has) {
          cell.dataset.launchLink = `launch.cell.${r + 1}.${c + 1}`;
          cell.addEventListener("click", () => send("launch", { trackId: track.id, blockId: block.id }));
        }
        grid.appendChild(cell);
      });
    });

    grid.appendChild(element("div", "live-corner"));
    columns.forEach((track, c) => {
      const stop = element("button", "live-stop", "\u25a0");
      stop.type = "button";
      stop.dataset.stop = String(track.id);
      stop.dataset.launchLink = `launch.stop.${c + 1}`;
      stop.title = `Stop ${track.name} at the next launch point`;
      stop.addEventListener("click", () => send("stopTrack", { trackId: track.id }));
      grid.appendChild(stop);
    });
  }

  /** A launcher key as MIDI Link's window shows it: "Live · Verse on Bass" (by place: what's there now). */
  function targetText(key) {
    const parts = key.split(".");
    const block = (r) => data.blocks[Number(r) - 1];
    const track = (c) => liveColumns()[Number(c) - 1];
    const blockName = (r) => (block(r) ? block(r).name : `Row ${r}`);
    const trackName = (c) => (track(c) ? track(c).name : `Track column ${c}`);
    if (parts[1] === "stopall") return "Live \u00b7 Stop All";
    if (parts[1] === "row") return `Live \u00b7 ${blockName(parts[2])} (all of it)`;
    if (parts[1] === "stop") return `Live \u00b7 Stop ${trackName(parts[2])}`;
    if (parts[1] === "cell") return `Live \u00b7 ${blockName(parts[2])} on ${trackName(parts[3])}`;
    return key;
  }

  /** Which cell plays and which waits, as the song goes (a waiting one starts at its point). */
  function lightGrid() {
    if (!shown || !live.on) return;
    const at = Hub.songPosition();
    const byTrack = new Map(live.tracks.map((t) => [t.trackId, t]));
    for (const cell of $("blocks-grid").querySelectorAll(".live-cell.has")) {
      const t = byTrack.get(Number(cell.dataset.track));
      const block = Number(cell.dataset.block);
      const waiting = Boolean(t) && t.startsAt > at + 1e-6;
      const playing = Boolean(t) && (waiting ? t.beforeBlockId : t.blockId) === block;
      cell.classList.toggle("playing", playing);
      cell.classList.toggle("queued", waiting && t.blockId === block);
    }
    for (const stop of $("blocks-grid").querySelectorAll(".live-stop")) {
      const t = byTrack.get(Number(stop.dataset.stop));
      stop.classList.toggle("queued", Boolean(t) && t.blockId === 0 && t.startsAt > at + 1e-6);
    }
  }

  function showLive() {
    $("blocks-live").setAttribute("aria-pressed", String(live.on));
    $("blocks-live-view").hidden = !live.on;
    document.querySelector("#song-blocks .blocks-body").hidden = live.on;
    $("blocks-make").hidden = live.on;
    $("blocks-live-quantize").value = String(live.quantize);
    if (live.on) { renderGrid(); lightGrid(); }
  }

  // ---------------------------------------------------------------- the page
  /** The page in front (on) or not: drawn afresh when it comes. */
  function show(on) {
    if (on === shown) return;
    shown = on;
    if (!on) {
      chosen = -1;
      return;
    }
    render(true);
    if (!renameId) win.pads.focus({ preventScroll: true });
  }

  function setUpPage() {
    win = { pads: $("blocks-pads"), strip: $("blocks-strip"), info: $("blocks-song-info"), clear: $("blocks-clear"),
            at: $("blocks-at"), build: $("blocks-build"), make: $("blocks-make") };
    win.pads.tabIndex = -1;

    win.make.addEventListener("click", () => makeFromSelection());
    $("blocks-live").addEventListener("click", () => send("launcherOn", { on: !live.on }));
    $("blocks-live-stop").addEventListener("click", () => send("stopAll"));
    $("blocks-live-stop").dataset.launchLink = "launch.stopall";
    $("blocks-live-quantize").addEventListener("change", (event) => send("launchQuantize", { value: Number(event.target.value) }));
    requestAnimationFrame(function tick() { lightGrid(); requestAnimationFrame(tick); });
    win.clear.addEventListener("click", () => setSequence([], "Clear the Song"));
    win.build.addEventListener("click", buildSong);
    win.at.addEventListener("input", () => { atTyped = true; });
    win.at.addEventListener("keydown", (event) => { event.stopPropagation(); if (event.key === "Enter") buildSong(); });

    // Delete and the arrows are the strip's own; everything else (Ctrl+Z, Space...) goes on as usual.
    win.strip.addEventListener("keydown", (event) => {
      if ((event.key === "Delete" || event.key === "Backspace") && chosen >= 0) {
        event.preventDefault();
        event.stopPropagation();
        removeFromSong(chosen);
      }
      if ((event.key === "ArrowLeft" || event.key === "ArrowRight") && data.sequence.length) {
        event.preventDefault();
        event.stopPropagation();
        chosen = Hub.clamp(chosen + (event.key === "ArrowLeft" ? -1 : 1), 0, data.sequence.length - 1);
        renderSong();
      }
    });
  }

  // ---------------------------------------------------------------- a clip's right-click
  const clipMenuItems = [
    { label: "Make Block from Selected Clips", value: "makeSongBlock" },
    { label: "Go to Song Builder Blocks", value: "songBlocks" }
  ];
  Hub.songBlockMenuItems = clipMenuItems;   // the Audio menu (an audio clip's right-click) has them too

  Hub.on("ready", () => {
    setUpPage();

    const lanes = $("lanes");
    if (!lanes) return;

    // A MIDI clip's right-click (an audio clip's is the Audio menu).
    lanes.addEventListener("contextmenu", (event) => {
      if (!Hub.arrange || Hub.arrange.view.automation) return;
      const track = Hub.arrange.trackAt(event.clientY);
      const clip = track ? Hub.arrange.clipAt(track, Hub.arrange.ppqAt(event.clientX)) : null;
      if (!clip || clip.audioFile) return;
      event.preventDefault();
      if (!Hub.state.selectedClips.includes(clip.id)) Hub.state.selectedClips = [clip.id];
      Hub.arrange.redraw();
      const at = { getBoundingClientRect: () => ({ left: event.clientX, right: event.clientX, top: event.clientY, bottom: event.clientY }) };
      // Merge Clips, Bounce to New Track and Export Clip to Audio, as an audio clip's menu has them (hub-audioeditor.js).
      const tools = Hub.clipTools;
      const merge = tools ? tools.mergeable(track, clip) : null;
      const items = [...Hub.arrange.clipEditItems(), { separator: true }, ...clipMenuItems, { separator: true },
                     { label: "Automation", value: "clipAutomation", title: "The clip's own controller lines (Mod Wheel, Expression, Sustain...), drawn on the clip (they move and copy with it)" },
                     { label: "Show Track as Score\u2026", value: "showClipScore" },
                     { label: "Detect Chords", value: "detectChords", title: "The selected MIDI clips' chords, onto the chord track (Master Song Channel > Chords)" }];
      if (tools) items.push({ separator: true }, { label: merge.label, value: "merge", disabled: !merge.ok, title: merge.title },
                            tools.bounceItem(track, clip), ...tools.exportItems());
      Hub.popupMenu(at, items, (command) => {
        if (Hub.arrange.runClipEdit(command)) return;
        if (command === "clipAutomation") { if (Hub.clipAutomation) Hub.clipAutomation.open(track, clip); }
        else if (command === "merge") tools.merge(track, clip);
        else if (command === "bounce") tools.bounce(track, clip);
        else if (command === "export" || command === "exportIn") tools.export(track, clip, command === "exportIn");
        else Hub.run(command);
      });
    });
  });

  // ---------------------------------------------------------------- what the engine says
  Hub.on("songBlocks", (message) => {
    data = {
      blocks: Array.isArray(message && message.blocks) ? message.blocks : [],
      sequence: Array.isArray(message && message.sequence) ? message.sequence : []
    };
    Hub.state.songBlocks = data;
    chosen = Math.min(chosen, data.sequence.length - 1);
    render();
  });

  // A pad just made. On the page: it shows, its name ready for typing. In the Studio (the menu or a
  // clip's right-click): the Studio stays in front, and says where the block went.
  Hub.on("songBlockMade", (message) => {
    const id = message && message.id ? message.id : 0;

    if (shown) {
      renameId = id;
      render();
      return;
    }

    const block = blockById(id);
    const what = block ? `Block "${block.name}"` : "Block";
    Hub.toast(`${what} ${message && message.action === "duplicate" ? "copied" : "made"}: it's in Song Builder Blocks (Ctrl+4).`);
  });

  Hub.on("songBlocksBuilt", (message) => {
    if (!message) return;
    const notice = message.notice || "";

    if (!message.placed) {
      if (notice) Hub.toast(notice);
      return;
    }

    const from = Hub.tempoMap.barAt(message.from).index + 1;
    const to = Hub.tempoMap.barAt(Math.max(message.from, message.to - 1e-6)).index + 1;
    Hub.toast(`Song built: bars ${from} to ${to}.` + (notice ? " " + notice : "") + (shown ? " See it in the Studio (Ctrl+1)." : ""));
    atTyped = false;
    render();
  });

  Hub.on("tracks", () => render());   // the pads draw their tracks' colours

  Hub.on("songLive", (message) => {
    const was = live.on;
    live = { on: Boolean(message && message.on), quantize: Number(message && message.quantize) || 0,
             tracks: Array.isArray(message && message.tracks) ? message.tracks : [] };
    if (live.on !== was) gridDrawn = "";
    if (live.on && !was) Hub.toast("Live: the song is at the bar after its end, where the jam is written. Launch cells as it plays.");
    showLive();
    if (!live.on && was) render(true);
  });
  Hub.on("theme", () => render(true));

  Hub.commands.makeSongBlock = () => makeFromSelection();   // Song Builder Blocks itself: the selector (hub-studios.js)
  Hub.blocks = { show, isShown: () => shown, targetText };
})();
