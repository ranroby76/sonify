// D:\workspace\Fanan_web_hub\src\hub-aitools.js
//
// AI Tools (the top bar's selector, Ctrl+3): Sonify's AI musical tools, each a small studio in a
// tab of its own. They work on the song's clips, on this computer, and each change is one undo
// step in the engine. Every tab picks its clip from a list of the song's clips (the clip selected
// in the Studio comes first); an audio tab can import a file straight into the song too.
//
//   Stem Splitter        a recording into vocals, drums, bass, guitar, piano and the rest, each on
//                        a new audio track under it (the same split as Event > Split into Stems:
//                        hub-stems.js); in the same go, each stem into MIDI (one after another) and
//                        the recording's chords, key and tempo (heard alongside the split: the
//                        engine's Analyse, audioAnalysis), shown under it with Copy Chords. Just the
//                        Chords, Key and Tempo listens without splitting.
//   Audio to MIDI        a recording's notes (any instrument or voice) or its drums, as a MIDI clip
//                        on a new track under it (the Audio menu's: hub-audioeditor.js)
//   MIDI Generators      the MIDI tools on a clip: the built-in devices and Magenta's models, which
//                        install here too (the same tools as Event > MIDI Tools: hub-miditools.js)
//   Tempo & Pitch        Sync Tempo, Follow Song Tempo, Detect, Pitch, Reverse, Normalize and Back
//                        to the Original (the Audio menu's)
//   Mastering Assistant  the whole song mastered on Main for a loudness and a character, then
//                        compared with the original (the same as Song > Mastering Assistant:
//                        hub-master.js)
// A tool that's working shows its progress, with Stop; then what came out, with Show in the
// Studio. Work started elsewhere (the AI splitting stems, a mastering from the Song menu) shows
// here too. Fanan AI, the assistant, is the AI Assistant window (hub-ai.js).

(() => {
  "use strict";

  const { $, element } = Hub;
  const TABS = ["stems", "tomidi", "midigen", "tempo", "master"];
  const TAB_KEY = "sonify.aiTools.tab";

  let shown = false;
  let tab = "stems";
  try { const kept = localStorage.getItem(TAB_KEY); if (TABS.includes(kept)) tab = kept; } catch (error) { /* no storage here */ }

  // ---------------------------------------------------------------- small parts
  function button(label, title, primary, onClick) {
    const b = element("button", "plain-button" + (primary ? " primary" : ""), label);
    b.type = "button";
    if (title) b.title = title;
    b.addEventListener("click", onClick);
    return b;
  }

  const barOf = (ppq) => (Hub.tempoMap ? Hub.tempoMap.barAt(Math.max(0, ppq) + 1e-9).index + 1 : Math.floor(ppq / 4) + 1);
  const quote = (text) => `\u201c${text}\u201d`;
  const signed = (value, digits = 2) => `${value > 0 ? "+" : value < 0 ? "\u2212" : ""}${+Math.abs(value).toFixed(digits)}`;
  const bpmText = (bpm) => `${+Number(bpm).toFixed(2)} BPM`;

  /** The Studio in front, showing what a tool made: new tracks selected, or a clip in its editor. */
  function showInStudio({ trackIds = [], trackId = 0, clipId = 0 } = {}) {
    Hub.studios.activate("studio");
    const found = clipId ? Hub.findClip(clipId) : null;
    if (found) {
      Hub.state.selectedClips = [clipId];
      if (found.clip.audioFile) Hub.openAudioEditor(found.track.id, clipId);
      else if (Hub.openEditor) Hub.openEditor(found.track.id, clipId);
    } else {
      const ids = trackIds.filter((id) => Hub.trackById(id));
      if (!ids.length && trackId && Hub.trackById(trackId)) ids.push(trackId);
      ids.forEach((id, i) => Hub.selectTrack(id, i ? "toggle" : "only"));
    }
    if (Hub.arrange) Hub.arrange.redraw();
  }

  // ---------------------------------------------------------------- status: progress, a problem, a result
  const stops = {};   // tool -> what its Stop does

  const statusBox = (tool) => $(`ait-${tool}-status`);

  function showProgress(tool, text, fraction, canStop = true) {
    const box = statusBox(tool);
    let progress = box.querySelector(".ait-progress");
    if (!progress) {
      box.replaceChildren();
      progress = element("div", "ait-progress");
      const row = element("div", "ait-progress-row");
      const stop = button("Stop", "Stop it", false, () => { if (stops[tool]) stops[tool](); });
      stop.classList.add("small");
      row.append(element("span", "ait-progress-text"), stop);
      const bar = element("div", "export-bar");
      bar.setAttribute("role", "progressbar");
      bar.appendChild(element("i"));
      progress.append(row, bar);
      box.appendChild(progress);
    }
    progress.querySelector(".ait-progress-text").textContent = text;
    const fill = Math.round(Hub.clamp01(Number(fraction) || 0) * 100);
    progress.querySelector(".export-bar i").style.width = `${fill}%`;
    progress.querySelector(".export-bar").setAttribute("aria-valuenow", String(fill));
    const stop = progress.querySelector("button");
    stop.hidden = !canStop || !stops[tool];
  }

  const showingProgress = (tool) => Boolean(statusBox(tool).querySelector(".ait-progress"));

  function showProblem(tool, text) {
    statusBox(tool).replaceChildren(element("p", "ait-problem", text));
  }

  /** What came out: a line, and buttons ({ label, onClick, primary }). */
  function showResult(tool, text, actions = []) {
    const box = element("div", "ait-result");
    const mark = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    mark.setAttribute("viewBox", "0 0 16 16");
    mark.setAttribute("aria-hidden", "true");
    mark.innerHTML = '<path d="M3 8.5l3.2 3L13 4.5" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>';
    box.append(mark, element("span", "ait-result-text", text));
    for (const action of actions) {
      const b = button(action.label, action.title || "", Boolean(action.primary), action.onClick);
      b.classList.add("small");
      box.appendChild(b);
    }
    statusBox(tool).replaceChildren(box);
  }

  const clearStatus = (tool) => statusBox(tool).replaceChildren();

  // ---------------------------------------------------------------- the clip pickers
  const pickers = {};   // name -> { kind, select, info, empty, chosen, adopted, importing, adopt, onChange }

  /** The song's clips a kind of tool works on, track by track, each track's in order. */
  function clipsOfKind(kind) {
    const out = [];
    for (const track of Hub.state.tracks || []) {
      const clips = (track.clips || []).filter((clip) => (kind === "audio" ? Boolean(clip.audioFile) && !clip.recording
                                                                            : track.kind === "instrument" && !clip.audioFile));
      for (const clip of clips.slice().sort((a, b) => a.start - b.start)) out.push({ track, clip });
    }
    return out;
  }

  function clipText(kind, clip) {
    if (kind === "audio") return `${clip.fileName || "Audio"}  \u00b7  bar ${barOf(clip.start)}${clip.missing ? "  (missing)" : ""}`;
    const count = (clip.notes || []).length;
    return `Bar ${barOf(clip.start)}  \u00b7  ${count} ${count === 1 ? "note" : "notes"}`;
  }

  function makePicker(name, kind, { canImport = false, adopt = true, onChange = null } = {}) {
    const box = $(`ait-${name}`);
    const select = element("select", "ait-select");
    select.setAttribute("aria-label", kind === "audio" ? "The recording" : "The MIDI clip");
    const row = element("div", "ait-picker-row");
    row.appendChild(select);
    const picker = { name, kind, select, chosen: 0, adopted: 0, importing: null, adopt, onChange, signature: "" };
    if (canImport) {
      const add = button("Import Audio\u2026", "An audio file from this computer, into the song: on the selected audio track at the song position, or a new one", false, () => {
        picker.importing = new Set(clipsOfKind("audio").map((x) => x.clip.id));
        Hub.run("importAudioFiles");
      });
      row.appendChild(add);
    }
    picker.empty = element("p", "ait-empty", kind === "audio"
      ? "No audio clips in the song yet: import an audio file, or record one in the Studio."
      : "No MIDI clips in the song yet: draw one with the Paint tool in the Studio, record one, or make one from a recording in Audio to MIDI.");
    picker.info = element("p", "ait-picker-info");
    box.append(row, picker.empty, picker.info);
    select.addEventListener("change", () => {
      picker.chosen = Number(select.value) || 0;
      describePicker(picker);
      if (picker.onChange) picker.onChange();
    });
    pickers[name] = picker;
    return picker;
  }

  /** The list again: an imported clip, or one selected in the Studio since, becomes the choice. */
  function refreshPicker(picker) {
    const list = clipsOfKind(picker.kind);
    const ids = new Set(list.map((x) => x.clip.id));

    if (picker.importing) {
      const fresh = list.filter((x) => !picker.importing.has(x.clip.id));
      if (fresh.length) {
        picker.chosen = fresh[fresh.length - 1].clip.id;
        picker.importing = null;
      }
    }
    const selected = Hub.state.selectedClip;
    if (picker.adopt && selected && selected.clipId !== picker.adopted && ids.has(selected.clipId)) {
      picker.chosen = selected.clipId;
      picker.adopted = selected.clipId;
    }
    if (!ids.has(picker.chosen)) picker.chosen = list.length ? list[0].clip.id : 0;

    const signature = list.map((x) => `${x.track.id}:${x.track.name}:${x.clip.id}:${clipText(picker.kind, x.clip)}`).join("|");
    if (signature !== picker.signature) {
      picker.signature = signature;
      picker.select.replaceChildren();
      let group = null, groupTrack = null;
      for (const { track, clip } of list) {
        if (track !== groupTrack) {
          groupTrack = track;
          group = element("optgroup");
          group.label = track.name || "Track";
          picker.select.appendChild(group);
        }
        group.appendChild(new Option(clipText(picker.kind, clip), String(clip.id)));
      }
    }
    picker.select.value = picker.chosen ? String(picker.chosen) : "";
    picker.select.disabled = !list.length;
    picker.select.hidden = !list.length;
    picker.empty.hidden = list.length > 0;
    describePicker(picker);
  }

  /** The clip chosen and its track, or null. */
  function chosenOf(picker) {
    const found = picker && picker.chosen ? Hub.findClip(picker.chosen) : null;
    if (!found) return null;
    const ok = picker.kind === "audio" ? Boolean(found.clip.audioFile) : found.track.kind === "instrument" && !found.clip.audioFile;
    return ok ? found : null;
  }

  function describePicker(picker) {
    const found = chosenOf(picker);
    if (!found) { picker.info.textContent = ""; return; }
    const { track, clip } = found;
    const to = Math.max(barOf(clip.start), barOf(clip.start + clip.length - 1e-6));
    const where = `${track.name}  \u00b7  bars ${barOf(clip.start)} to ${to}`;
    if (picker.kind !== "audio") { picker.info.textContent = where; return; }
    const parts = [where];
    if (typeof clip.audioLength === "number") parts.push(`${clip.audioLength.toFixed(1)} s`);
    if (clip.sourceFile) {
      const done = [];
      if (clip.sourceTempo > 0) done.push(`${clip.followTempo ? "follows the song's tempo" : "synced"} from ${bpmText(clip.sourceTempo)}`);
      if (clip.pitch) done.push(`${signed(clip.pitch)} semitones`);
      if (clip.reversed) done.push("reversed");
      if (done.length) parts.push(`made from ${clip.sourceName || "the original"}: ${done.join(", ")}`);
    }
    picker.info.textContent = parts.join("  \u00b7  ");
  }

  function refreshPickers() {
    for (const picker of Object.values(pickers)) refreshPicker(picker);
  }

  // ---------------------------------------------------------------- Stem Splitter
  // The split (vocals, drums, bass, the rest; guitar and piano too, from the six-stem model), and
  // in the same go: each stem's notes as MIDI, one stem after another once the stems are there,
  // and the chords, key and tempo heard in the recording alongside the split.
  const STEMS = ["vocals", "drums", "bass", "guitar", "piano", "other"];
  const STEM_NAMES = { vocals: "vocals", drums: "drums", bass: "bass", guitar: "guitar", piano: "piano", other: "the rest" };
  const STEM_TITLES = { vocals: "Vocals", drums: "Drums", bass: "Bass", guitar: "Guitar", piano: "Piano", other: "The rest" };
  const ENGINE_NAMES = { vocals: "Vocals", drums: "Drums", bass: "Bass", guitar: "Guitar", piano: "Piano", other: "Other" };
  let stems = { state: "idle", progress: 0, modelReady: true, sixReady: true, problem: "", stems: [] };
  let stemsWatching = false, stemsStopping = false;
  const stemsWorking = (s = stems) => s.state === "downloading" || s.state === "splitting";
  const stemBox = (stem) => document.querySelector(`#ait-stems input[data-stem="${stem}"]`);

  // The whole go: { trackId, clipId, name, stems, midi, split ("working", "done", "failed",
  // "stopped"), splitError, stemTracks [{stem, trackId}], queue (the stems still to turn into
  // MIDI), current (the one now), midiTracks, midiFailed, analysing, analysePending,
  // analysisError, step, total, stopped }.
  let go = null;
  const analyses = new Map();     // clip id -> what was heard in it (audioAnalysis)
  const analysisErrors = new Map();
  let analysisProgress = 0;

  function stemList(names) {
    const words = names.map((stem) => STEM_NAMES[stem] || stem);
    return words.length > 1 ? `${words.slice(0, -1).join(", ")} and ${words[words.length - 1]}` : words.join("");
  }

  /** What the first split with the stems ticked downloads, and where guitar and piano come from. */
  function modelNote() {
    const six = ["guitar", "piano"].some((stem) => stemBox(stem).checked);
    if (six && stems.sixReady === false) return "Guitar and piano come from a second model: the first split with them downloads it (110 MB), once.";
    if (six) return "Guitar and piano come from a second model; the rest is what's left after the stems ticked.";
    if (stems.modelReady === false) return "The first split downloads what Sonify splits with (98 MB), once.";
    return "";
  }

  /** A stem's track, by the name the split gave it ("Mix Vocals"), else as listed. */
  function stemOfTrack(trackId, listed) {
    const name = ((Hub.trackById(trackId) || {}).name || "").toLowerCase();
    return STEMS.find((stem) => name.endsWith(" " + ENGINE_NAMES[stem].toLowerCase())) || listed || "other";
  }

  function renderStems() {
    const busy = stemsWorking() || Boolean(go);
    const found = chosenOf(pickers.stems);
    $("ait-stems-go").disabled = busy || !found || Boolean(stems.problem);
    $("ait-stems-listen").disabled = busy || !found || audioBusy();
    const note = modelNote();
    $("ait-stems-model").textContent = note;

    if (go) {
      goProgress();
    } else if (stemsWorking()) {
      // A split started elsewhere (Fanan AI, Event > Split into Stems): how far it is.
      stemsWatching = true;
      const where = stems.clipId ? Hub.findClip(stems.clipId) : null;
      const name = where ? quote(where.clip.fileName || "the clip") : "the recording";
      const p = Hub.clamp01(stems.progress || 0), percent = Math.round(p * 100);
      const text = stemsStopping ? "Stopping\u2026"
        : stems.state === "downloading" ? `Downloading what Sonify splits with (${stems.downloadMb || 98} MB, only this once)\u2026 ${percent}%`
        : `Splitting ${name} into ${stemList(stems.stems && stems.stems.length ? stems.stems : STEMS)}\u2026 ${percent}%`;
      showProgress("stems", text, p, !stemsStopping);
    } else if (showingProgress("stems") && !(audioMine && audioMine.tool === "stems")) {
      clearStatus("stems");
    }
    if (!busy && stems.problem) showProblem("stems", stems.problem);
    renderReadout();
  }

  /** The go's progress: its step, and how far that step is. */
  function goProgress() {
    if (!go) return;
    const prefix = go.total > 1 ? `Step ${go.step} of ${go.total}: ` : "";
    let text = "", fraction = 0;
    if (go.stopped) {
      text = "Stopping\u2026";
    } else if (go.split === "working") {
      fraction = Hub.clamp01(stems.progress || 0);
      text = stems.state === "downloading"
        ? `${prefix}downloading what Sonify splits with (${stems.downloadMb || 98} MB, only this once)\u2026 ${Math.round(fraction * 100)}%`
        : `${prefix}splitting ${quote(go.name)} into ${stemList(go.stems)}\u2026 ${Math.round(fraction * 100)}%`;
    } else if (go.current) {
      fraction = Hub.clamp01((audioJob && audioJob.progress) || 0);
      text = `${prefix}${STEM_TITLES[go.current.stem] || "A stem"} into MIDI\u2026 ${Math.round(fraction * 100)}%`;
    } else if (go.analysing || go.analysePending) {
      fraction = analysisProgress;
      text = `Listening for the chords, key and tempo\u2026 ${Math.round(fraction * 100)}%`;
    } else {
      text = "Finishing\u2026";
    }
    showProgress("stems", text, fraction, !go.stopped);
  }

  function splitStems() {
    const found = chosenOf(pickers.stems);
    if (!found || stemsWorking() || go) return;
    if (found.clip.missing) { showProblem("stems", "That recording's file is missing."); return; }
    const chosen = STEMS.filter((stem) => stemBox(stem).checked);
    if (!chosen.length) { showProblem("stems", "Choose at least one stem."); return; }

    const midi = $("ait-stems-midi").checked, analyse = $("ait-stems-analyse").checked;
    const six = chosen.includes("guitar") || chosen.includes("piano");
    go = { trackId: found.track.id, clipId: found.clip.id, name: found.clip.fileName || "the recording", stems: chosen, midi,
           split: "working", splitError: "", stemTracks: [], queue: [], current: null, midiTracks: [], midiFailed: [],
           analysing: false, analysePending: analyse, analysisError: "", step: 1, total: 1 + (midi ? chosen.length : 0), stopped: false };
    analysisErrors.delete(found.clip.id);
    if (analyse) analyses.delete(found.clip.id);
    if (Hub.stems) Hub.stems.claim();   // shown here: no note from Event > Split into Stems
    stemsStopping = false;
    stemsWatching = true;
    const ready = six ? stems.sixReady !== false : stems.modelReady !== false;
    stems = { ...stems, state: ready ? "splitting" : "downloading", progress: 0, stems: chosen, downloadMb: six ? 110 : 98,
              trackId: found.track.id, clipId: found.clip.id };
    clearStatus("stems");
    Hub.send("splitStems", { trackId: found.track.id, clipId: found.clip.id, stems: chosen, muteOriginal: $("ait-stems-mute").checked });

    // The chords, key and tempo are heard alongside the split (when nothing else has the audio).
    if (analyse && !audioBusy()) startAnalysis(found, true);
    renderStems();
  }

  /** Listens to the recording for its chords, key and tempo (part of the go, or on its own). */
  function startAnalysis(found, inGo) {
    if (!Hub.audioOps || !Hub.audioOps.run("analyse", {}, found.track.id, found.clip.id)) return false;
    audioMine = { tool: "stems", op: "analyse", want: "analyse", go: inGo, extra: {}, trackId: found.track.id, clipId: found.clip.id };
    analysisProgress = 0;
    analysisErrors.delete(found.clip.id);
    if (go && inGo) { go.analysing = true; go.analysePending = false; }
    renderReadout();
    return true;
  }

  /** Just the chords, key and tempo: no stems. */
  function listenOnly() {
    const found = chosenOf(pickers.stems);
    if (!found || go || stemsWorking() || audioBusy()) return;
    if (found.clip.missing) { showProblem("stems", "That recording's file is missing."); return; }
    analyses.delete(found.clip.id);
    clearStatus("stems");
    if (startAnalysis(found, false)) showProgress("stems", "Listening for the chords, key and tempo\u2026", 0);
    renderStems();
  }

  /** The next step of the go: the analysis (when it waited), each stem into MIDI, then the end. */
  function goNext() {
    if (!go) return;
    if (go.split === "working" || go.current || go.analysing) { renderStems(); return; }

    if (go.analysePending && !go.stopped) {
      const found = Hub.findClip(go.clipId);
      if (found && startAnalysis(found, true)) { renderStems(); return; }
      go.analysePending = false;
    }

    const task = go.stopped ? null : go.queue.shift();
    if (!task) { finishGo(); return; }
    go.step += 1;
    const track = Hub.trackById(task.trackId);
    const clip = track && (track.clips || []).find((c) => c.audioFile);
    if (!clip || !Hub.audioOps || !Hub.audioOps.run("toMidi", { mode: task.stem === "drums" ? "drums" : "notes" }, track.id, clip.id)) {
      go.midiFailed.push(task.stem);
      goNext();
      return;
    }
    go.current = task;
    audioMine = { tool: "stems", op: "toMidi", want: "toMidi", go: true, extra: {}, trackId: track.id, clipId: clip.id };
    renderStems();
  }

  /** What the go made: the stems, their MIDI, and what didn't work out. */
  function finishGo() {
    const g = go;
    go = null;
    if (!g) return;
    const stemIds = g.stemTracks.map((t) => t.trackId);
    const stemNames = stemIds.map((id) => (Hub.trackById(id) || {}).name).filter(Boolean);
    const parts = [];
    if (g.split === "done") parts.push(`Split into ${stemNames.length || stemIds.length} ${stemIds.length === 1 ? "stem" : "stems"}${stemNames.length ? ": " + stemNames.join(", ") : ""}.`);
    if (g.midiTracks.length) parts.push(g.midiTracks.length === stemIds.length ? "Each turned into MIDI, on a new track under its stem."
                                        : `${g.midiTracks.length} of them turned into MIDI, each on a new track under its stem.`);
    if (g.midiFailed.length) parts.push(`No notes came from ${stemList(g.midiFailed)}.`);
    if (g.analysisError) parts.push(`The chords couldn't be heard: ${g.analysisError}`);
    const show = { label: "Show in the Studio", onClick: () => showInStudio({ trackIds: stemIds.concat(g.midiTracks) }) };

    if (g.split === "failed") {
      showProblem("stems", g.splitError || "The stems couldn't be split.");
    } else if (g.split === "stopped" && !stemIds.length) {
      clearStatus("stems");
    } else if (g.stopped) {
      showResult("stems", `Stopped. ${parts.join(" ")}`.trim(), stemIds.length ? [show] : []);
    } else {
      showResult("stems", parts.join(" ") || "Done.", stemIds.length ? [show] : []);
    }
    renderStems();
  }

  stops.stems = () => {
    if (go) {
      go.stopped = true;
      go.queue = [];
      go.analysePending = false;
      if (go.split === "working") { stemsStopping = true; Hub.send("cancelStems"); }
      if (audioMine && audioMine.go) Hub.send("audioCancel");
      goProgress();
      return;
    }
    if (audioMine && audioMine.tool === "stems") { Hub.send("audioCancel"); return; }
    stemsStopping = true;
    renderStems();
    Hub.send("cancelStems");
  };

  Hub.on("stems", (s) => {
    stems = { ...stems, ...s };

    // The go's split.
    if (go && go.split === "working") {
      if (stemsWorking(s) || s.state === "idle") { renderStems(); return; }
      stemsWatching = false;
      stemsStopping = false;
      if (s.state === "done") {
        go.split = "done";
        const listed = s.stems && s.stems.length ? s.stems : go.stems;
        go.stemTracks = (s.trackIds || []).map((trackId, i) => ({ trackId, stem: stemOfTrack(trackId, listed[i]) }));
        go.queue = go.midi && !go.stopped ? go.stemTracks.slice() : [];
        go.total = 1 + go.queue.length;
      } else {
        go.split = s.state === "failed" ? "failed" : "stopped";
        go.splitError = s.error || "";
        go.stopped = true;
        go.queue = [];
      }
      goNext();
      return;
    }

    if (stemsWorking(s)) { renderStems(); return; }

    // A split started elsewhere, ended.
    const watching = stemsWatching;
    stemsWatching = false;
    stemsStopping = false;
    if (watching && s.state === "done") {
      const ids = s.trackIds || [];
      const names = ids.map((id) => (Hub.trackById(id) || {}).name).filter(Boolean);
      showResult("stems", names.length ? `Split into stems: ${names.join(", ")}, each on a new track under the recording.` : "Split into stems, each on a new track under the recording.",
                 [{ label: "Show in the Studio", onClick: () => showInStudio({ trackIds: ids }) }]);
    } else if (watching && s.state === "failed") {
      showProblem("stems", s.error || "The stems couldn't be split.");
    } else if (watching) {
      clearStatus("stems");
    }
    renderStems();
  });

  // ---------------------------------------------------------------- the chords, key and tempo
  const MAJOR_KEYS = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];
  const MINOR_KEYS = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"];

  const keyText = (d) => (d.keyTonic >= 0 && d.keyTonic < 12 ? `${(d.keyMinor ? MINOR_KEYS : MAJOR_KEYS)[d.keyTonic]} ${d.keyMinor ? "minor" : "major"}` : "Not clear");
  const barsOf = (d) => (String(d.chords || "").trim() ? String(d.chords).split("|").map((bar) => bar.trim() || "%") : []);
  const firstBarOf = (d) => (d.followsSong ? Math.max(1, Number(d.firstBar) || 1) : 1);

  /** The chords as text: the key and tempo, then four bars a line, each line starting with its bar. */
  function chordsText(d) {
    const bars = barsOf(d), first = firstBarOf(d), lines = [`Key: ${keyText(d)}`, `Tempo: ${bpmText(d.bpm)}`, ""];
    for (let i = 0; i < bars.length; i += 4) lines.push(`${first + i}\t| ${bars.slice(i, i + 4).join(" | ")} |`);
    return lines.join("\n");
  }

  function copyChords(d) {
    const text = chordsText(d);
    const fallback = () => {
      const area = element("textarea");
      area.value = text;
      area.style.position = "fixed";
      area.style.opacity = "0";
      document.body.appendChild(area);
      area.select();
      let ok = false;
      try { ok = document.execCommand("copy"); } catch (error) { ok = false; }
      area.remove();
      Hub.toast(ok ? "Copied the chords." : "The chords couldn't be copied here.");
    };
    try {
      if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(text).then(() => Hub.toast("Copied the chords."), fallback);
      else fallback();
    } catch (error) {
      fallback();
    }
  }

  /** What was heard in the clip the tab is on (or the go's): its key, tempo and a chord for each bar. */
  function renderReadout() {
    const box = $("ait-stems-readout");
    if (!box) return;
    const chosen = chosenOf(pickers.stems);
    const clipId = (go && go.clipId) || (audioMine && audioMine.want === "analyse" && audioMine.clipId) || (chosen ? chosen.clip.id : 0);
    const data = clipId ? analyses.get(clipId) : null;
    const listening = Boolean((audioMine && audioMine.want === "analyse" && audioMine.clipId === clipId) || (go && go.clipId === clipId && go.analysePending));
    const error = clipId ? analysisErrors.get(clipId) : "";

    if (!data && !listening && !error) { box.hidden = true; box.replaceChildren(); return; }
    box.hidden = false;
    box.replaceChildren();

    const head = element("div", "ait-readout-head");
    head.appendChild(element("h4", "ait-card-title", "Chords, key and tempo"));
    if (data && barsOf(data).length) {
      const copy = button("Copy Chords", "The key, the tempo and the chords, bar by bar, as text", false, () => copyChords(data));
      copy.classList.add("small");
      head.appendChild(copy);
    }
    box.appendChild(head);

    if (!data) {
      box.appendChild(element("p", error ? "ait-problem" : "ait-hint", error || `Listening for them\u2026 ${Math.round(analysisProgress * 100)}%`));
      return;
    }

    const facts = element("div", "ait-facts");
    const fact = (label, value, note) => {
      const item = element("div", "ait-fact");
      item.append(element("span", "ait-fact-label", label), element("b", "", value));
      if (note) item.appendChild(element("small", "", note));
      facts.appendChild(item);
    };
    fact("Key", keyText(data));
    fact("Tempo", bpmText(data.bpm), data.followsSong ? "the song's" : data.steadiness !== undefined && data.steadiness < 0.5 ? "not very steady" : "steady");
    fact("Bars", String(data.bars || barsOf(data).length));
    box.appendChild(facts);

    const bars = barsOf(data), first = firstBarOf(data);
    if (bars.length) {
      const chart = element("div", "ait-chart");
      for (let i = 0; i < bars.length; i += 4) {
        const row = element("div", "ait-chart-row");
        row.appendChild(element("span", "ait-chart-bar", String(first + i)));
        for (const chord of bars.slice(i, i + 4)) row.appendChild(element("span", "ait-chord" + (chord === "%" || chord === "N.C." ? " repeat" : ""), chord));
        chart.appendChild(row);
      }
      box.appendChild(chart);
    }
    box.appendChild(element("p", "ait-hint", data.followsSong
      ? "On the song's bars: it plays to the song's beat."
      : `Its own bars, counted from its first downbeat, ${(Number(data.downbeat) || 0).toFixed(1)} s in: it doesn't play to the song's beat (Tempo & Pitch's Sync Tempo puts it there). % repeats the bar before.`));
  }

  Hub.on("audioAnalysis", (data) => {
    if (!data) return;
    analyses.set(data.clipId, data);
    analysisErrors.delete(data.clipId);
    if (data.bpm > 0) heardTempo.set(data.clipId, { bpm: data.bpm, steadiness: data.steadiness });
    renderReadout();
  });

  Hub.on("song", (data) => { if (data && data.opened) { analyses.clear(); analysisErrors.clear(); } });

  // ---------------------------------------------------------------- the Audio menu's changes (Audio to MIDI, Tempo & Pitch, the go)
  const OP_TEXT = { syncTempo: "Syncing the tempo", followTempo: "Stretching to the song's tempo", pitch: "Moving the pitch",
                    reverse: "Reversing", normalize: "Normalizing", original: "Going back to the original",
                    toMidi: "Listening for notes", detectTempo: "Listening for the tempo", analyse: "Listening for the chords, key and tempo" };
  let audioJob = null;    // the engine's audio job working now (anyone's)
  let audioMine = null;   // { tool, op, want, go, trackId, clipId, heard }: the one started here
  const heardTempo = new Map();   // clip id -> { bpm, steadiness }
  stops.tomidi = stops.tempo = () => Hub.send("audioCancel");

  const audioBusy = () => Boolean(audioJob || audioMine || (Hub.audioOps && Hub.audioOps.busy()));

  /** One of the Audio menu's changes on the chosen recording. want: what it's for (Sync, after Detect). */
  function runAudio(tool, op, extra = {}) {
    const found = chosenOf(pickers[tool]);
    if (!found || !Hub.audioOps) return;
    if (found.clip.missing) { showProblem(tool, "That recording's file is missing."); return; }
    if (audioBusy()) { showProblem(tool, "Sonify is still working on another change to a recording: wait for it to finish."); return; }

    // Sync and Follow (on) take the recording's tempo: given, or heard first.
    const tempo = extra.tempo || 0;
    const stretch = op === "syncTempo" || (op === "followTempo" && extra.on !== false);
    const ok = stretch ? Hub.audioOps.syncTempo(op === "followTempo", found.track.id, found.clip.id, tempo)
                       : Hub.audioOps.run(op, extra, found.track.id, found.clip.id);
    if (!ok) return;

    audioMine = { tool, op, want: op, extra, trackId: found.track.id, clipId: found.clip.id, heard: false,
                  wasReversed: Boolean(found.clip.reversed) };
    const first = stretch && !tempo ? "detectTempo" : op;
    showProgress(tool, `${OP_TEXT[first] || "Working"}\u2026`, 0);
    renderAudioTools();
  }

  function audioDone(mine, job) {
    const tool = mine.tool;
    const found = Hub.findClip(mine.clipId);
    const show = { label: "Show in the Studio", onClick: () => showInStudio({ clipId: mine.clipId, trackId: mine.trackId }) };
    const asked = mine.extra || {};

    if (mine.want === "analyse") {
      clearStatus(tool);   // what was heard shows under it
      renderReadout();
      return;
    }
    if (mine.want === "toMidi") {
      const newTrackId = job.newTrackId || 0;
      const track = newTrackId ? Hub.trackById(newTrackId) : null;
      showResult(tool, `The notes are on a new track${track ? " " + quote(track.name) : ""}, under the recording's.`, [
        { label: "Show in the Studio", onClick: () => {
          const fresh = Hub.trackById(newTrackId);
          const first = fresh && (fresh.clips || []).find((c) => !c.audioFile);
          showInStudio(first ? { clipId: first.id } : { trackId: newTrackId });
        } },
        { label: "Open in MIDI Generators", title: "Its notes, with the MIDI Generators", onClick: () => {
          const fresh = Hub.trackById(newTrackId);
          const first = fresh && (fresh.clips || []).find((c) => !c.audioFile);
          if (first) { pickers["gen-clip"].chosen = first.id; pickers["gen-clip"].adopted = Hub.state.selectedClip ? Hub.state.selectedClip.clipId : 0; }
          showTab("midigen");
        } }
      ]);
      return;
    }
    if (mine.want === "detectTempo") {
      const heard = heardTempo.get(mine.clipId);
      if (!heard) { showProblem(tool, "Sonify couldn't hear a tempo in it."); return; }
      const shaky = heard.steadiness !== undefined && heard.steadiness < 0.5 ? " (it isn't very steady)" : "";
      showResult(tool, `Its tempo: ${bpmText(heard.bpm)}${shaky}. Sync Tempo puts it on the song's beat.`);
      return;
    }

    // What was asked (the clip's own word may come just after).
    const from = asked.tempo || (heardTempo.get(mine.clipId) || {}).bpm || (found && found.clip.sourceTempo) || 0;
    const texts = {
      syncTempo: () => `Synced: its beats land on the song's${from > 0 ? ", from " + bpmText(from) : ""}. The original is kept.`,
      followTempo: () => (asked.on !== false ? `It follows the song's tempo now${from > 0 ? ", from " + bpmText(from) : ""}: it's stretched again whenever the tempo changes.`
                                             : "It stays as it is now when the song's tempo changes."),
      pitch: () => (asked.semitones ? `Its pitch: ${signed(asked.semitones)} semitones from the original.` : "Back at the original's pitch."),
      reverse: () => (!mine.wasReversed ? "Reversed: it plays backwards now." : "Forwards again."),
      normalize: () => "Normalized.",
      original: () => "Back to the original recording."
    };
    showResult(tool, (texts[mine.want] || (() => "Done."))(), [show]);
  }

  /** A job of the go's ended: the analysis, or a stem into MIDI. */
  function goJobEnded(mine, job) {
    const ok = job.state === "done" && !job.error;
    if (mine.want === "analyse") {
      if (go) go.analysing = false;
      if (!ok && job.state !== "cancelled") {
        const why = job.error || "Sonify couldn't hear chords in it.";
        analysisErrors.set(mine.clipId, why);
        if (go) go.analysisError = why;
      }
    } else if (go) {
      const task = go.current;
      go.current = null;
      if (ok && job.newTrackId) go.midiTracks.push(job.newTrackId);
      else if (job.state === "cancelled") go.stopped = true;
      else if (task) go.midiFailed.push(task.stem);
    }
    goNext();
  }

  Hub.on("audioJob", (job) => {
    if (!job) return;
    const mine = audioMine;
    if (job.state === "working") {
      audioJob = job;
      const p = Hub.clamp01(job.progress || 0);
      if (mine && mine.want === "analyse") { analysisProgress = p; renderReadout(); }
      if (mine && mine.go) goProgress();
      else if (mine) showProgress(mine.tool, `${OP_TEXT[job.op] || "Working"}\u2026 ${Math.round(p * 100)}%`, p);
      renderAudioTools();
      return;
    }

    audioJob = null;
    if (!mine) { renderAudioTools(); return; }
    if (mine.go) {
      audioMine = null;
      goJobEnded(mine, job);
      renderAudioTools();
      return;
    }
    const ok = job.state === "done" && !job.error;

    // Listening for the tempo was the first step of Sync or Follow: the stretch comes next (the
    // Audio Editor sends it once the tempo is heard).
    if (ok && job.op === "detectTempo" && (mine.want === "syncTempo" || mine.want === "followTempo")) {
      showProgress(mine.tool, "Heard its tempo: stretching it\u2026", 0);
      return;
    }
    // Detect alone: done once the tempo is heard (it may come just after).
    if (ok && mine.want === "detectTempo" && !mine.heard) {
      mine.waiting = true;
      return;
    }

    audioMine = null;
    if (job.state === "failed" || job.error) {
      if (mine.want === "analyse") analysisErrors.set(mine.clipId, job.error || "Sonify couldn't hear chords in it.");
      showProblem(mine.tool, job.error || "That didn't work.");
    } else if (job.state === "done") {
      audioDone(mine, job);
    } else {
      clearStatus(mine.tool);   // stopped
    }
    renderAudioTools();
  });

  Hub.on("audioTempo", (data) => {
    if (!data) return;
    if (data.bpm > 0) heardTempo.set(data.clipId, { bpm: data.bpm, steadiness: data.steadiness });
    const mine = audioMine;
    if (mine && mine.clipId === data.clipId) {
      mine.heard = true;
      // Known already (the tempo it was synced from): no job comes, this answer is the end of it.
      if (mine.want === "detectTempo" && (mine.waiting || data.known)) {
        audioMine = null;
        audioDone(mine, {});
      } else if ((mine.want === "syncTempo" || mine.want === "followTempo") && !(data.bpm > 0)) {
        audioMine = null;
        showProblem(mine.tool, "Sonify couldn't hear its tempo: type it in, then Sync again.");
      }
    }
    tempoShown.clipId = -1;   // the field shows what was heard
    renderAudioTools();
  });

  function renderAudioTools() {
    renderToMidi();
    renderTempo();
    if (shown && tab === "stems") renderStems();
  }

  // ---------------------------------------------------------------- Audio to MIDI
  function renderToMidi() {
    const busy = audioBusy();
    $("ait-tomidi-go").disabled = busy || !chosenOf(pickers.tomidi);
    $("ait-tomidi-note").textContent = busy && !(audioMine && audioMine.tool === "tomidi") ? "Sonify is working on another change to a recording\u2026" : "";
  }

  function toMidi() {
    const mode = (document.querySelector('input[name="ait-tomidi-mode"]:checked') || {}).value || "notes";
    runAudio("tomidi", "toMidi", { mode });
  }

  // ---------------------------------------------------------------- Tempo & Pitch
  const tempoShown = { clipId: -1, bpm: "", pitch: "" };   // what the fields were last given

  function renderTempo() {
    const found = chosenOf(pickers.tempo);
    const busy = audioBusy();
    const ids = ["ait-tempo-detect", "ait-tempo-sync", "ait-tempo-follow", "ait-tempo-pitch-set", "ait-tempo-down", "ait-tempo-up",
                 "ait-tempo-reverse", "ait-tempo-normalize", "ait-tempo-original", "ait-tempo-bpm", "ait-tempo-pitch"];
    for (const id of ids) $(id).disabled = !found || busy;
    if (!found) {
      $("ait-tempo-song").textContent = "";
      tempoShown.clipId = -1;
      return;
    }

    const clip = found.clip;
    const heard = heardTempo.get(clip.id);
    const bpm = clip.sourceTempo > 0 ? String(+clip.sourceTempo.toFixed(2)) : heard ? String(+heard.bpm.toFixed(2)) : "";
    const pitch = String(+(clip.pitch || 0).toFixed(2));
    const bpmField = $("ait-tempo-bpm"), pitchField = $("ait-tempo-pitch");
    if (tempoShown.clipId !== clip.id || tempoShown.bpm !== bpm) { if (document.activeElement !== bpmField) bpmField.value = bpm; }
    if (tempoShown.clipId !== clip.id || tempoShown.pitch !== pitch) { if (document.activeElement !== pitchField) pitchField.value = pitch; }
    Object.assign(tempoShown, { clipId: clip.id, bpm, pitch });

    const songBpm = Hub.tempoMap ? Hub.tempoMap.bpmAt(clip.start) : 0;
    $("ait-tempo-song").textContent = songBpm ? `The song's tempo where it starts: ${bpmText(songBpm)}.` : "";
    const follow = $("ait-tempo-follow");
    follow.setAttribute("aria-pressed", String(Boolean(clip.followTempo)));
    follow.textContent = clip.followTempo ? "Stop Following" : "Follow Song Tempo";
    $("ait-tempo-reverse").textContent = clip.reversed ? "Forwards Again" : "Reverse";
    $("ait-tempo-original").disabled = !found || busy || !clip.sourceFile;
  }

  function typedTempo() {
    const value = Number($("ait-tempo-bpm").value);
    return $("ait-tempo-bpm").value !== "" && Number.isFinite(value) && value >= 20 && value <= 400 ? value : 0;
  }

  function syncTempo(follow) {
    const found = chosenOf(pickers.tempo);
    if (!found) return;
    if (follow && found.clip.followTempo) { runAudio("tempo", "followTempo", { on: false }); return; }
    runAudio("tempo", follow ? "followTempo" : "syncTempo", { tempo: typedTempo() });
  }

  function setPitch() {
    const found = chosenOf(pickers.tempo);
    const value = Number($("ait-tempo-pitch").value);
    if (!found || !Number.isFinite(value)) return;
    const semitones = Hub.clamp(Math.round(value * 100) / 100, -24, 24);
    if (Math.abs(semitones - (found.clip.pitch || 0)) < 1e-6) { showResult("tempo", `Its pitch is ${signed(semitones)} semitones already.`); return; }
    runAudio("tempo", "pitch", { semitones });
  }

  function nudgePitch(step) {
    const field = $("ait-tempo-pitch");
    const value = Number(field.value) || 0;
    field.value = String(+Hub.clamp(Math.round(value + step), -24, 24).toFixed(2));
  }

  // ---------------------------------------------------------------- MIDI Generators
  const gen = { key: "", values: {}, busy: false };

  function genEntry(list, key) {
    return list.builtin.find((t) => t.key === key) || list.magenta.find((t) => t.key === key) || null;
  }

  function renderGenList() {
    const api = Hub.midiGenerators;
    const box = $("ait-gen-list");
    if (!api) return;
    const list = api.list();
    if (!gen.key || !genEntry(list, gen.key)) gen.key = (list.builtin.find((t) => t.key === "fx:arpeggiator") || list.builtin[0] || list.magenta[0] || {}).key || "";

    const scroll = box.scrollTop;
    box.replaceChildren();
    // A Magenta tool not here yet downloads with a click on it (and shows itself meanwhile).
    const row = (entry, meta, state) => {
      const item = element("button", "ait-gen-item" + (state ? " " + state : ""));
      item.type = "button";
      item.setAttribute("role", "option");
      item.setAttribute("aria-selected", String(entry.key === gen.key));
      item.dataset.key = entry.key;
      item.append(element("span", "ait-gen-item-name", entry.name), element("span", "ait-gen-item-meta", meta));
      item.title = state === "download" ? `${entry.description} Click to download it (${sizeText(entry.sizeMb)}, once, no credits).` : entry.description;
      item.addEventListener("click", () => {
        gen.key = entry.key;
        if (state === "download") api.install(entry.id);
        renderGen();
      });
      box.appendChild(item);
    };

    box.appendChild(element("div", "ait-gen-group", "Built in"));
    if (!list.builtin.length) box.appendChild(element("p", "ait-empty", "Sonify is still listing them\u2026"));
    for (const entry of list.builtin) row(entry, "", "");

    // Magenta's, with Download All while any are still to come.
    const missing = list.magenta.filter((entry) => !entry.installed);
    const group = element("div", "ait-gen-group ait-gen-group-row");
    group.appendChild(element("span", "", "Magenta"));
    if (list.magentaBuilt && missing.length) {
      const busy = Boolean(api.downloadingAll && api.downloadingAll());
      const waiting = missing.filter((entry) => entry.installing === null && !entry.queued);
      const megabytes = waiting.reduce((sum, entry) => sum + (entry.sizeMb || 0), 0);
      const all = button(busy ? "Downloading\u2026" : `Download All${megabytes ? " (" + sizeText(megabytes) + ")" : ""}`,
                         "Every Magenta tool not here yet, one after another: once, no credits", true, () => { api.installAll(); renderGen(); });
      all.classList.add("small", "ait-gen-all");
      all.disabled = busy || !waiting.length;
      group.appendChild(all);
    }
    box.appendChild(group);
    if (!list.magentaBuilt) box.appendChild(element("p", "ait-empty", "This Sonify was built without magenta.js, so Magenta's tools can't run."));
    for (const entry of list.magenta) {
      const meta = entry.installing !== null ? `Downloading\u2026 ${Math.round(entry.installing * 100)}%`
        : entry.queued ? "Waiting to download"
        : entry.installed ? (entry.update ? "Click to update" : "Installed")
        : list.magentaBuilt ? "Click to download" : "Can't run here";
      const state = entry.installed ? (entry.update && entry.installing === null ? "download" : "")
        : entry.installing !== null || entry.queued ? "downloading" : list.magentaBuilt ? "download" : "unavailable";
      row(entry, meta, state);
    }
    box.scrollTop = scroll;
  }

  const sizeText = (mb) => `${Math.max(0.1, mb).toFixed(mb < 10 ? 1 : 0)} MB`;

  function renderGenDetail() {
    const api = Hub.midiGenerators;
    if (!api) return;
    const list = api.list();
    const entry = genEntry(list, gen.key);
    $("ait-gen-name").textContent = entry ? entry.name : "";
    $("ait-gen-about").textContent = entry ? entry.description : "";

    // A Magenta tool: installed here, once (a download, no credits).
    const install = $("ait-gen-install");
    install.replaceChildren();
    const magenta = entry && gen.key.startsWith("mg:");
    if (magenta) {
      if (entry.installing !== null) {
        const bar = element("div", "export-bar");
        const fill = element("i");
        fill.style.width = `${Math.round(entry.installing * 100)}%`;
        bar.appendChild(fill);
        install.append(element("span", "ait-hint", `Downloading\u2026 ${Math.round(entry.installing * 100)}%`), bar);
      } else {
        const size = entry.sizeMb ? ` (${sizeText(entry.sizeMb)})` : "";
        if (entry.queued) {
          install.appendChild(element("span", "ait-hint", "Waiting to download: the others before it come first."));
        } else if (!entry.installed || entry.update) {
          const b = button(entry.update ? "Update" : `Download${size}`, entry.update ? "A newer version is out" : "Download it to this computer, once: no credits", true, () => api.install(entry.id));
          b.classList.add("small");
          install.appendChild(b);
          if (!entry.installed) install.appendChild(element("span", "ait-hint", "Download it once to use it: it runs on this computer, with no credits."));
        }
        if (entry.installed) {
          const b = button("Remove", "Remove it from this computer", false, () => api.remove(entry.id));
          b.classList.add("small");
          install.appendChild(b);
          if (entry.actions.length) install.appendChild(element("span", "ait-hint", entry.actions.join(", ")));
        }
      }
    }

    // Its settings.
    const params = $("ait-gen-params");
    params.replaceChildren();
    const values = gen.values[gen.key] = gen.values[gen.key] || {};
    if (entry && (!magenta || entry.installed) && Hub.midiFx && Hub.midiFx.paramControl) {
      for (const param of api.params(gen.key, values)) {
        if (values[param.id] === undefined) values[param.id] = param.value;
        params.appendChild(Hub.midiFx.paramControl(param, values[param.id], (v) => {
          values[param.id] = v;
          if (param.id === "action") renderGenDetail();
        }).row);
      }
    }

    const choice = api.choice(gen.key, values);
    const morph = Boolean(choice && choice.tool && choice.action === "morph");
    $("ait-gen-other-row").hidden = !morph;
    if (morph) {
      const other = pickers["gen-other"];
      if (!other.chosen || other.chosen === pickers["gen-clip"].chosen) {
        const next = clipsOfKind("midi").find((x) => x.clip.id !== pickers["gen-clip"].chosen);
        if (next) { other.chosen = next.clip.id; refreshPicker(other); }
      }
    }
    renderGenGo();
  }

  function renderGenGo() {
    const api = Hub.midiGenerators;
    const choice = api ? api.choice(gen.key, gen.values[gen.key] || {}) : null;
    const found = chosenOf(pickers["gen-clip"]);
    const morph = Boolean(choice && choice.tool && choice.action === "morph");
    const other = morph ? chosenOf(pickers["gen-other"]) : null;
    const ready = Boolean(choice && found && (!morph || (other && other.clip.id !== found.clip.id)));
    $("ait-gen-go").disabled = gen.busy || !ready;
    $("ait-gen-note").textContent = gen.busy ? "" : !choice && gen.key.startsWith("mg:") ? "Download it first (click it in the list)."
      : morph && found && (!other || other.clip.id === found.clip.id) ? "Morph goes from one clip to another: choose a second clip." : "";
  }

  function renderGen() {
    renderGenList();
    renderGenDetail();
  }

  async function applyGenerator() {
    const api = Hub.midiGenerators;
    const choice = api ? api.choice(gen.key, gen.values[gen.key] || {}) : null;
    const found = chosenOf(pickers["gen-clip"]);
    if (!choice || !found || gen.busy) return;
    const morph = choice.tool && choice.action === "morph";
    const clips = [found];
    if (morph) {
      const other = chosenOf(pickers["gen-other"]);
      if (!other || other.clip.id === found.clip.id) return;
      clips.push(other);
    }

    gen.busy = true;
    stops.gen = null;
    renderGenGo();
    showProgress("gen", choice.tool ? "Thinking\u2026 (the first time, the model loads)" : "Applying\u2026", choice.tool ? 0.35 : 0.6, false);
    try {
      const result = (await api.apply(clips, choice)) || {};
      const label = result.label || "The tool";
      if (result.newTrack) showResult("gen", `${label}: its notes are on a new track under ${quote(found.track.name)}.`,
                                      [{ label: "Show in the Studio", onClick: () => showInStudio({ trackId: found.track.id }) }]);
      else if (result.changed === false) showResult("gen", `${label} left the clip as it was.`);
      else showResult("gen", `${label}: applied to the clip on ${quote(found.track.name)}. Edit > Undo takes it back.`,
                      [{ label: "Show in the Studio", onClick: () => showInStudio({ clipId: found.clip.id }) }]);
    } catch (error) {
      showProblem("gen", error && error.message ? error.message : String(error));
    }
    gen.busy = false;
    renderGenGo();
  }

  Hub.on("midiToolsChanged", () => { if (shown && tab === "midigen") renderGen(); });

  // ---------------------------------------------------------------- Mastering Assistant
  const LOUDNESS = { dynamic: "Dynamic", streaming: "Streaming", loud: "Loud" };
  const CHARACTER = { clean: "Clean", warm: "Warm", bright: "Bright" };
  const BANDS = ["low", "low mid", "mid", "presence", "air"];
  let master = { state: "idle", insertId: 0 };
  let masterView = "setup", masterAsked = false, comparing = false, hearOriginal = false;
  const masterWorking = () => master.state === "rendering" || master.state === "analysing";
  const hasMaster = () => Boolean(master.insertId && master.result);
  const picked = (name) => (document.querySelector(`input[name="${name}"]:checked`) || {}).value;

  function masterShow(view) {
    masterView = view;
    $("ait-master-setup").hidden = view !== "setup";
    $("ait-master-result").hidden = view !== "result";
    $("ait-master-go").hidden = view === "working";
    $("ait-master-go").textContent = view === "result" ? "Master Again\u2026" : "Master the Song";
    if (view !== "working" && showingProgress("master")) clearStatus("master");
    if (view === "result") renderMasterResult();
  }

  const level = (value, unit) => (typeof value === "number" && isFinite(value) && value > -150 ? `${value.toFixed(1)} ${unit}` : "\u2013");
  const db = (value) => `${value > 0 ? "+" : ""}${value.toFixed(1)} dB`;

  function renderMasterResult() {
    const r = master.result || {};
    $("ait-master-before-lufs").textContent = level(r.beforeLufs, "LUFS");
    $("ait-master-before-peak").textContent = level(r.beforePeak, "dBTP");
    $("ait-master-after-lufs").textContent = level(r.afterLufs, "LUFS");
    $("ait-master-after-peak").textContent = level(r.afterPeak, "dBTP");
    const eq = (r.eq || []).map((value, i) => (Math.abs(value) >= 0.1 ? `${BANDS[i]} ${db(value)}` : "")).filter(Boolean);
    const parts = [`${LOUDNESS[r.loudness] || "Streaming"}, ${(CHARACTER[r.character] || "Clean").toLowerCase()}`,
                   eq.length ? `EQ: ${eq.join(", ")}` : "EQ: none needed",
                   r.compression >= 0.1 ? `glue: ${r.compression.toFixed(1)} dB` : "glue: none needed"];
    if (typeof r.limiting === "number") parts.push(`limiter: up to ${r.limiting.toFixed(1)} dB`);
    $("ait-master-moves").textContent = `${parts.join(" \u00b7 ")}.`;
    renderListen();
  }

  function renderListen() {
    const original = comparing && hearOriginal;
    $("ait-master-hear-master").setAttribute("aria-pressed", String(!original));
    $("ait-master-hear-original").setAttribute("aria-pressed", String(original));
  }

  function listen(original) {
    comparing = true;
    hearOriginal = original;
    Hub.send("masteringListen", { original, sameLoudness: $("ait-master-same").checked });
    renderListen();
  }

  /** The master plays as it is again (leaving the tab, a new master, or none). */
  function stopComparing() {
    if (!comparing) return;
    comparing = false;
    hearOriginal = false;
    Hub.send("masteringListen", { original: false, sameLoudness: false });
    renderListen();
  }

  function masterProgress() {
    masterShow("working");
    // Rendering is the first half, listening and deciding the second.
    const p = Hub.clamp01(master.progress || 0);
    const fraction = master.state === "analysing" ? 0.5 + 0.5 * p : 0.5 * p;
    showProgress("master", master.state === "analysing" ? "Finding the master\u2026" : "Listening to the song\u2026", fraction);
  }

  function masterGo() {
    if (masterView === "result") {
      // Again, from what was chosen last time.
      const r = master.result || {};
      for (const [name, value] of [["ait-loudness", r.loudness], ["ait-character", r.character]]) {
        const input = document.querySelector(`input[name="${name}"][value="${value}"]`);
        if (input) input.checked = true;
      }
      clearStatus("master");
      masterShow("setup");
      return;
    }
    if (masterView !== "setup" || masterWorking()) return;
    stopComparing();
    clearStatus("master");
    master = { ...master, state: "rendering", progress: 0 };
    masterProgress();
    Hub.send("masterSong", { loudness: picked("ait-loudness") || "streaming", character: picked("ait-character") || "clean" });
  }

  stops.master = () => Hub.send("cancelMastering");

  Hub.on("mastering", (m) => {
    master = { ...m };
    const answer = masterAsked;
    masterAsked = false;
    if (masterWorking()) { masterProgress(); return; }
    if (m.state === "done") {
      comparing = false;
      hearOriginal = false;
      masterShow("result");
      return;
    }
    if (m.state === "failed") {
      masterShow("setup");
      showProblem("master", m.error || "The mastering didn't finish.");
      return;
    }
    // Stopped, or how it stands (the answer to showing the tab: a mastering kept with the song).
    if (m.state === "cancelled" || answer || masterView === "working") masterShow(hasMaster() ? "result" : "setup");
  });

  // Sonify Mastering taken out (from the Console, or undone): forgotten here too.
  function masterGone() {
    if (!master.insertId) return;
    const inserts = (Hub.state.master && Hub.state.master.inserts) || [];
    if (inserts.some((insert) => insert.insertId === master.insertId)) return;
    master = { ...master, insertId: 0, result: null };
    comparing = false;
    if (masterView === "result") masterShow("setup");
  }

  // ---------------------------------------------------------------- the tabs
  function onTabShown(name) {
    if (name === "stems") { Hub.send("stemsState"); renderStems(); }
    if (name === "tomidi" || name === "tempo") renderAudioTools();
    if (name === "midigen") { if (Hub.midiGenerators) Hub.midiGenerators.refresh(false); renderGen(); }
    if (name === "master") { masterAsked = true; Hub.send("masteringState"); }
  }

  function showTab(name) {
    if (!TABS.includes(name)) return;
    if (tab === "master" && name !== "master") stopComparing();
    tab = name;
    try { localStorage.setItem(TAB_KEY, name); } catch (error) { /* no storage here */ }
    for (const b of document.querySelectorAll(".ait-tab")) {
      const on = b.dataset.ait === name;
      b.setAttribute("aria-selected", String(on));
      b.tabIndex = on ? 0 : -1;
    }
    for (const t of TABS) $(`ait-${t}`).hidden = t !== name;
    if (shown) {
      refreshPickers();
      onTabShown(name);
    }
  }

  /** The studio shows (on) or goes. */
  function show(on) {
    if (on === shown) return;
    shown = on;
    if (on) {
      showTab(tab);
    } else {
      stopComparing();
      for (const picker of Object.values(pickers)) picker.importing = null;
    }
  }

  Hub.aiTools = { show, showTab, tab: () => tab };

  Hub.on("tracks", () => {
    masterGone();
    if (!shown) return;
    refreshPickers();
    renderStems();
    renderAudioTools();
    if (tab === "midigen") renderGenGo();
  });
  Hub.on("tempoMap", () => { if (shown) { for (const picker of Object.values(pickers)) picker.signature = ""; refreshPickers(); renderTempo(); } });

  Hub.on("ready", () => {
    makePicker("stems-clip", "audio", { canImport: true, onChange: renderStems });
    makePicker("tomidi-clip", "audio", { canImport: true, onChange: renderToMidi });
    makePicker("tempo-clip", "audio", { canImport: true, onChange: renderTempo });
    makePicker("gen-clip", "midi", { onChange: renderGenDetail });
    makePicker("gen-other", "midi", { adopt: false, onChange: renderGenGo });
    // The pickers are known by their tool's name too.
    pickers.stems = pickers["stems-clip"];
    pickers.tomidi = pickers["tomidi-clip"];
    pickers.tempo = pickers["tempo-clip"];
    delete pickers["stems-clip"];
    delete pickers["tomidi-clip"];
    delete pickers["tempo-clip"];

    const strip = $("ait-tabs");
    strip.addEventListener("click", (event) => {
      const b = event.target.closest(".ait-tab");
      if (b) showTab(b.dataset.ait);
    });
    strip.addEventListener("keydown", (event) => {
      if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
      event.preventDefault();
      const next = TABS[(TABS.indexOf(tab) + (event.key === "ArrowLeft" ? -1 : 1) + TABS.length) % TABS.length];
      showTab(next);
      const b = strip.querySelector(`[data-ait="${next}"]`);
      if (b) b.focus();
    });

    $("ait-stems-go").addEventListener("click", splitStems);
    $("ait-stems-listen").addEventListener("click", listenOnly);
    for (const stem of STEMS) stemBox(stem).addEventListener("change", () => { if (!stemsWorking() && !go && !audioMine) clearStatus("stems"); renderStems(); });

    $("ait-tomidi-go").addEventListener("click", toMidi);

    $("ait-tempo-detect").addEventListener("click", () => runAudio("tempo", "detectTempo"));
    $("ait-tempo-sync").addEventListener("click", () => syncTempo(false));
    $("ait-tempo-follow").addEventListener("click", () => syncTempo(true));
    $("ait-tempo-pitch-set").addEventListener("click", setPitch);
    $("ait-tempo-down").addEventListener("click", () => nudgePitch(-1));
    $("ait-tempo-up").addEventListener("click", () => nudgePitch(1));
    $("ait-tempo-reverse").addEventListener("click", () => runAudio("tempo", "reverse"));
    $("ait-tempo-normalize").addEventListener("click", () => runAudio("tempo", "normalize"));
    $("ait-tempo-original").addEventListener("click", () => runAudio("tempo", "original"));
    $("ait-tempo-pitch").addEventListener("keydown", (event) => { if (event.key === "Enter") { event.preventDefault(); setPitch(); } });
    $("ait-tempo-bpm").addEventListener("keydown", (event) => { if (event.key === "Enter") { event.preventDefault(); syncTempo(false); } });

    $("ait-gen-go").addEventListener("click", applyGenerator);
    $("ait-gen-refresh").addEventListener("click", () => { if (Hub.midiGenerators) Hub.midiGenerators.refresh(true); });

    $("ait-master-go").addEventListener("click", masterGo);
    $("ait-master-hear-master").addEventListener("click", () => listen(false));
    $("ait-master-hear-original").addEventListener("click", () => listen(true));
    $("ait-master-same").addEventListener("change", () => { if (comparing) listen(hearOriginal); });
    $("ait-master-controls").addEventListener("click", () => {
      const insertId = master.insertId;
      stopComparing();
      if (!insertId) return;
      Hub.studios.activate("studio");
      if (Hub.openScreen) Hub.openScreen(Hub.MAIN, insertId);
    });
    $("ait-master-remove").addEventListener("click", () => {
      if (!master.insertId) return;
      stopComparing();
      Hub.send("removeInsert", { trackId: Hub.MAIN, insertId: master.insertId });
      master = { ...master, insertId: 0, result: null };
      masterShow("setup");
      showResult("master", "The mastering is out. Edit > Undo brings it back.");
    });

    showTab(tab);
  });
})();
