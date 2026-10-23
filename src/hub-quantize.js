// D:\workspace\Fanan_web_hub\src\hub-quantize.js
//
// Quantize: notes onto a grid, as Studio One's Quantize panel. The toolbar's Quantize button (next
// to the tools) opens the panel; it stays open while notes and clips are chosen, as the MIDI Tools
// panel does.
//   Grid             Follow the grid (the Editor's Quantize in the Editor, the toolbar's for clips),
//                    or one of its own: 1/1 to 1/64, or the triplets 1/4T to 1/32T
//   Strength         how far notes move toward the grid (100%: right onto it)
//   Swing            every second grid position later, up to a triplet shuffle at 100%
//   Starts, Ends     what moves (Ends: each note's end onto the grid too, so lengths tidy up)
//   Quantize         on the notes in the Editor (the selected ones, or all of them), or on the
//                    selected MIDI clips in the arrangement: one undo step. Q does the same, with
//                    the panel's settings, wherever the notes are.
//   Record Quantize  the notes recorded go onto the grid as each take ends, with these settings
//                    (Follow: the toolbar's grid), in the take's undo step (the engine does it:
//                    AudioEngine::NoteQuantize, TempoMap::quantized). The toolbar's button shows
//                    a light while it's on.
// The grid is laid from each bar's start (a grid of a bar or more: the nearest bar line), as the
// engine lays it. The settings stay on this computer (the page's storage) and go to Sonify at
// every start.

(() => {
  "use strict";

  const { $, element } = Hub;
  const STORE = "sonify.quantize";
  const GRIDS = [["Follow the grid", "follow"], ["1/1", 4], ["1/2", 2], ["1/4", 1], ["1/8", 0.5], ["1/16", 0.25], ["1/32", 0.125], ["1/64", 0.0625],
                 ["1/4 triplet", 2 / 3], ["1/8 triplet", 1 / 3], ["1/16 triplet", 1 / 6], ["1/32 triplet", 1 / 12]];
  const settings = { grid: "follow", strength: 100, swing: 0, starts: true, ends: false, record: false };
  let open = false;

  try {
    const kept = JSON.parse(localStorage.getItem(STORE) || "null");
    if (kept && typeof kept === "object") {
      if (kept.grid === "follow" || GRIDS.some(([, value]) => value === kept.grid)) settings.grid = kept.grid;
      for (const key of ["strength", "swing"]) if (Number.isFinite(kept[key])) settings[key] = Hub.clamp(Math.round(kept[key]), 0, 100);
      for (const key of ["starts", "ends", "record"]) if (typeof kept[key] === "boolean") settings[key] = kept[key];
    }
  } catch (error) { /* no storage here */ }

  function keep() {
    try { localStorage.setItem(STORE, JSON.stringify(settings)); } catch (error) { /* no storage here */ }
  }

  // ---------------------------------------------------------------- the grid and the math
  /** The grid to quantize with, in quarter notes: the panel's own, or (Follow) the Editor's or the toolbar's. */
  function gridFor(where) {
    if (settings.grid !== "follow") return settings.grid;
    if (where === "editor" && Hub.editor && Hub.editor.state && Hub.editor.state.quantize) return Hub.editor.state.quantize;
    return (Hub.arrange && Hub.arrange.view && Hub.arrange.view.quantize) || 0.25;
  }

  const gridName = (grid) => (GRIDS.find(([, value]) => value !== "follow" && Math.abs(value - grid) < 1e-9) || [`${+grid.toFixed(3)} beats`])[0];

  /** A song position moved toward the nearest grid position (laid from its bar's start; a bar or
      more: the nearest bar line), swing and strength applied: as the engine's TempoMap::quantized. */
  function time(t, grid, swing = settings.swing / 100, strength = settings.strength / 100) {
    t = Math.max(0, t);
    const bar = Hub.tempoMap.barAt(t);
    const barEnd = bar.start + bar.length;
    const step = grid > 1e-6 ? grid : 0.25;
    let target;
    if (step >= bar.length - 1e-9) {
      target = t - bar.start <= barEnd - t ? bar.start : barEnd;
    } else {
      const shift = Hub.clamp01(swing) * step / 3;
      const k = Math.floor((t - bar.start) / step);
      let distance = Math.abs(barEnd - t);
      target = barEnd;
      for (let j = Math.max(0, k - 1); j <= k + 2; j++) {
        const at = Math.min(barEnd, bar.start + j * step + (j % 2 ? shift : 0));
        if (Math.abs(at - t) < distance) { target = at; distance = Math.abs(at - t); }
      }
    }
    return t + (target - t) * Hub.clamp01(strength);
  }

  /** A note (song positions) quantized with the panel's settings: { start, length }. */
  function place(start, length, grid) {
    const newStart = settings.starts ? time(start, grid) : start;
    let end = settings.ends ? time(start + length, grid) : newStart + length;
    const shortest = settings.ends ? Math.max(1 / 128, grid * settings.strength / 100) : length;
    if (end <= newStart + 1e-6) end = newStart + Math.max(1 / 128, shortest);
    return { start: newStart, length: end - newStart };
  }

  // ---------------------------------------------------------------- what it works on
  function midiClipsSelected() {
    const out = [];
    for (const id of Hub.state.selectedClips || []) {
      const found = Hub.findClip(id);
      if (found && found.track.kind === "instrument" && !found.clip.audioFile) out.push(found);
    }
    return out;
  }

  /** The notes in the Editor (when it's open and has the focus, or no MIDI clip is selected), or
      the selected MIDI clips. */
  function target() {
    const editor = Hub.editor;
    const clips = midiClipsSelected();
    if (editor && editor.isOpen() && editor.state.clipId && (Hub.state.focus === "editor" || !clips.length)) {
      const notes = editor.state.notes || [];
      const chosen = notes.filter((n) => n.selected).length;
      const track = Hub.trackById(editor.state.trackId);
      return { kind: "editor", text: `${chosen ? chosen : "All"} ${(chosen || notes.length) === 1 ? "note" : "notes"} in the Editor${track ? " (" + track.name + ")" : ""}` };
    }
    if (clips.length) return { kind: "clips", clips, text: `${clips.length} selected MIDI ${clips.length === 1 ? "clip" : "clips"}` };
    return null;
  }

  /** The selected MIDI clips' notes onto the grid: the notes each clip shows (the ones a trim
      hides stay as they are), each kept inside its clip; one undo step. */
  function quantizeClips(clips) {
    const grid = gridFor("arrange");
    const edits = [];
    for (const { track, clip } of clips) {
      const offset = clip.offset || 0, clipEnd = clip.start + clip.length;
      let changed = false;
      const notes = (clip.notes || []).map((note) => {
        const out = note.slice();
        const [, start, length] = note;
        if (start < offset || start >= offset + clip.length) return out;   // hidden by a trim
        const song = clip.start + start - offset;
        const q = place(song, length, grid);
        if (q.start >= clipEnd - 1e-9) return out;                       // would leave the clip
        const from = Math.max(clip.start, q.start);
        out[1] = from - clip.start + offset;
        out[2] = Math.max(1 / 128, q.length - (from - q.start));
        if (Math.abs(out[1] - start) > 1e-9 || Math.abs(out[2] - length) > 1e-9) changed = true;
        return out;
      });
      if (changed) edits.push({ op: "notes", trackId: track.id, clipId: clip.id, notes: notes.sort((a, b) => a[1] - b[1]) });
    }
    if (edits.length) Hub.send("clipEdits", { edits, label: "Quantize" });
    return edits.length;
  }

  /** Quantize, with the panel's settings, wherever the notes are (the panel's button, Q). */
  function quantize() {
    const where = target();
    if (!where) { Hub.toast("Select notes in the Editor, or MIDI clips in the arrangement, to quantize."); return; }
    if (where.kind === "editor") { if (Hub.editor.quantize) Hub.editor.quantize(); return; }
    if (!quantizeClips(where.clips)) Hub.toast("They're on the grid already.");
  }

  // ---------------------------------------------------------------- Record Quantize
  function sendRecord() {
    Hub.send("recordQuantize", { on: settings.record, grid: gridFor("arrange"), strength: settings.strength / 100, swing: settings.swing / 100,
                                 starts: settings.starts, ends: settings.ends });
  }

  // ---------------------------------------------------------------- the panel
  function render() {
    const follow = settings.grid === "follow";
    const select = $("quantize-grid");
    if (select.options.length) select.options[0].textContent = `Follow the grid (${gridName(gridFor(Hub.editor && Hub.editor.isOpen() && Hub.state.focus === "editor" ? "editor" : "arrange"))})`;
    select.value = follow ? "follow" : String(settings.grid);
    $("quantize-strength").value = String(settings.strength);
    $("quantize-strength-value").textContent = `${settings.strength} %`;
    $("quantize-swing").value = String(settings.swing);
    $("quantize-swing-value").textContent = `${settings.swing} %`;
    $("quantize-starts").checked = settings.starts;
    $("quantize-ends").checked = settings.ends;
    const record = $("quantize-record");
    record.setAttribute("aria-checked", String(settings.record));
    record.classList.toggle("on", settings.record);
    showTarget();

    const button = $("quantize-button");
    button.setAttribute("aria-pressed", String(open));
    button.classList.toggle("recording", settings.record);
    button.title = settings.record ? "Quantize: notes onto the grid (Record Quantize is on: recorded notes go onto it too)"
                                   : "Quantize: notes onto the grid, with strength, swing, and Record Quantize";
  }

  function showTarget() {
    if (!open) return;
    const where = target();
    $("quantize-target").textContent = where ? `On: ${where.text}` : "Select notes in the Editor, or MIDI clips in the arrangement.";
    $("quantize-apply").disabled = !where || (!settings.starts && !settings.ends);
  }

  function changed() {
    keep();
    render();
    sendRecord();
  }

  function openPanel() {
    open = true;
    const panel = $("quantize-panel");
    panel.hidden = false;
    const r = $("quantize-button").getBoundingClientRect();
    const width = panel.offsetWidth || 380;
    panel.style.left = `${Math.round(Hub.clamp(r.left, 8, window.innerWidth - width - 8))}px`;
    panel.style.top = `${Math.round(r.bottom + 6)}px`;
    panel.style.right = "auto";
    render();
    $("quantize-apply").focus();
  }

  function closePanel() {
    open = false;
    const panel = $("quantize-panel");
    if (panel.contains(document.activeElement)) document.activeElement.blur();
    panel.hidden = true;
    render();
  }

  Hub.quantize = { time, place, gridFor, quantize, open: openPanel, close: closePanel, settings: () => ({ ...settings }) };
  Hub.commands.quantizeNotes = quantize;
  Hub.commands.quantizePanel = () => (open ? closePanel() : openPanel());

  Hub.on("ready", () => {
    const select = $("quantize-grid");
    for (const [label, value] of GRIDS) select.add(new Option(label, String(value)));
    select.addEventListener("change", () => {
      settings.grid = select.value === "follow" ? "follow" : Number(select.value);
      select.blur();
      changed();
    });
    for (const key of ["strength", "swing"]) {
      $(`quantize-${key}`).addEventListener("input", (event) => {
        settings[key] = Hub.clamp(Math.round(Number(event.target.value) || 0), 0, 100);
        $(`quantize-${key}-value`).textContent = `${settings[key]} %`;
      });
      $(`quantize-${key}`).addEventListener("change", changed);
      $(`quantize-${key}`).addEventListener("dblclick", () => { settings[key] = key === "strength" ? 100 : 0; changed(); });
    }
    $("quantize-starts").addEventListener("change", (event) => { settings.starts = event.target.checked; changed(); });
    $("quantize-ends").addEventListener("change", (event) => { settings.ends = event.target.checked; changed(); });
    $("quantize-record").addEventListener("click", () => { settings.record = !settings.record; changed(); });
    $("quantize-apply").addEventListener("click", quantize);
    $("quantize-close").addEventListener("click", closePanel);
    $("quantize-button").addEventListener("click", () => (open ? closePanel() : openPanel()));
    $("quantize-panel").addEventListener("keydown", (event) => {
      if (event.key === "Escape") { event.stopPropagation(); closePanel(); }
    });

    // What it works on follows what's chosen while it's open; Follow follows the toolbar's grid.
    document.addEventListener("pointerup", () => { if (open) setTimeout(showTarget, 0); }, true);
    $("quantize").addEventListener("change", () => { render(); sendRecord(); });
    render();
    sendRecord();
  });

  Hub.on("selection", showTarget);
  Hub.on("tracks", showTarget);
  Hub.on("viewChanged", () => { render(); sendRecord(); });
  Hub.on("song", () => setTimeout(sendRecord, 0));   // a song's own toolbar grid
})();
