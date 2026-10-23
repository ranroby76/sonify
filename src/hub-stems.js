// D:\workspace\Fanan_web_hub\src\hub-stems.js
//
// Event > Split into Stems: the selected audio clip separated into its vocals, drums, bass and
// the rest (Other), and its guitar and piano when they're ticked, worked out on this computer.
// Each stem goes on a new audio track under the clip's track, where the clip starts; Mute the
// original mutes the clip's track. One undo step. The first time, what Sonify splits with is
// downloaded, once (98 MB; guitar and piano split with a second model, 110 MB, and the rest then
// holds what's left after the ones ticked).
// While it works: a progress bar, Stop, and Hide (it goes on while you work; a note says when the
// stems are there). The AI can split stems too: the window then shows that split's progress.
// AI Tools' Stem Splitter tab (hub-aitools.js) starts the same split, and claims it (no note).
// The choices stay for the next time, until Sonify closes.

(() => {
  "use strict";

  const { $ } = Hub;
  const STEMS = ["vocals", "drums", "bass", "guitar", "piano", "other"];
  const NAMES = { vocals: "vocals", drums: "drums", bass: "bass", guitar: "guitar", piano: "piano", other: "the rest" };

  let state = { state: "idle", progress: 0, modelReady: true, sixReady: true, problem: "", stems: [] };   // the engine's last word
  let open = false, working = false, stopping = false;
  let target = null;      // the clip being split, or to split: {trackId, clipId}
  let announced = false;  // a split the AI started has been mentioned
  let claimed = false;    // AI Tools started it: its own page says how it ended (no note here)
  const last = { stems: { vocals: true, drums: true, bass: true, guitar: false, piano: false, other: true }, mute: true };

  const isWorking = (s = state) => s.state === "downloading" || s.state === "splitting";
  const checkbox = (stem) => document.querySelector(`#stems-dialog input[data-stem="${stem}"]`);

  function problem(text) {
    $("stems-problem").textContent = text || "";
    $("stems-problem").hidden = !text;
  }

  /** "Take 3 (Audio 2)": the clip's file and its track. */
  function clipLabel(where) {
    const found = where && Hub.findClip(where.clipId);
    if (!found) return "the clip";
    const name = found.clip.fileName || "the clip";
    return found.track.name ? `${name} (${found.track.name})` : name;
  }

  /** What the first split with the stems ticked downloads, when it's still to come ("" when not). */
  function modelNote() {
    const six = ["guitar", "piano"].some((stem) => checkbox(stem).checked);
    if (six && state.sixReady === false) return "Guitar and piano split with a second model: the first time, Sonify downloads it (110 MB), once.";
    if (!six && state.modelReady === false) return "The first time, Sonify downloads what it splits with: 98 MB, once.";
    return "";
  }

  function list(stems) {
    const words = stems.map((stem) => NAMES[stem] || stem);
    return words.length > 1 ? `${words.slice(0, -1).join(", ")} and ${words[words.length - 1]}` : words.join("");
  }

  // ---------------------------------------------------------------- the window
  function show() {
    working = isWorking();
    $("stems-choices").hidden = working;
    $("stems-progress").hidden = !working;
    $("stems-go").hidden = working;
    $("stems-hide").hidden = !working;
    $("stems-cancel").textContent = working ? "Stop" : "Cancel";
    $("stems-cancel").disabled = working && stopping;
    $("stems-model").textContent = modelNote();
    $("stems-model").hidden = working || !$("stems-model").textContent;
    $("stems-go").disabled = Boolean(state.problem);

    if (working) {
      const stems = state.stems && state.stems.length ? state.stems : STEMS;
      $("stems-lead").textContent = `Splitting ${clipLabel(target)} into ${list(stems)}, each on a new track under it.`;
      const p = Hub.clamp01(state.progress || 0);
      $("stems-fill").style.width = `${Math.round(p * 100)}%`;
      $("stems-step").textContent = stopping ? "Stopping…"
        : state.state === "downloading" ? `Downloading what Sonify splits with (${state.downloadMb || 98} MB, only this once)… ${Math.round(p * 100)}%`
        : `Splitting… ${Math.round(p * 100)}%`;
      return;
    }

    $("stems-lead").textContent = `${clipLabel(target)}: its vocals, drums, bass, guitar, piano and the rest, the ones ticked, each on a new track under it. `
                                + "It's worked out on this computer and takes a few minutes for a whole song.";
    if (state.problem) problem(state.problem);
  }

  function openDialog() {
    if (open) return;

    if (!isWorking()) {
      const picked = Hub.state.selectedClip;
      const found = picked && Hub.findClip(picked.clipId);
      if (!found || !found.clip.audioFile) {
        Hub.toast("Select an audio clip to split into stems.");
        return;
      }
      if (found.clip.recording) {
        Hub.toast("Not while it's recording.");
        return;
      }
      target = picked;
    }

    open = true;
    problem("");
    for (const stem of STEMS) checkbox(stem).checked = last.stems[stem];
    $("stems-mute").checked = last.mute;
    show();
    $("stems-dialog").hidden = false;
    Hub.send("stemsState");   // how it stands, and whether this computer can split stems
    (working ? $("stems-hide") : $("stems-go")).focus();
  }

  function closeDialog() {
    if (!open) return;
    open = false;
    if (!working) remember();
    const box = $("stems-dialog");
    if (box.contains(document.activeElement)) document.activeElement.blur();
    box.hidden = true;
  }

  function remember() {
    for (const stem of STEMS) last.stems[stem] = checkbox(stem).checked;
    last.mute = $("stems-mute").checked;
  }

  function start() {
    if (working || !target) return;
    const stems = STEMS.filter((stem) => checkbox(stem).checked);
    if (!stems.length) {
      problem("Choose at least one stem.");
      return;
    }
    if (state.problem) {
      problem(state.problem);
      return;
    }
    const found = Hub.findClip(target.clipId);
    if (!found || !found.clip.audioFile) {
      problem("That clip isn't there anymore: select an audio clip and try again.");
      return;
    }

    remember();
    problem("");
    stopping = false;
    announced = true;   // this window started it: no note about it
    claimed = false;    // ...but its end is told here
    const six = stems.includes("guitar") || stems.includes("piano");
    const ready = six ? state.sixReady !== false : state.modelReady !== false;
    state = { ...state, state: ready ? "splitting" : "downloading", progress: 0, stems, downloadMb: six ? 110 : 98 };
    show();
    $("stems-hide").focus();
    Hub.send("splitStems", { trackId: found.track.id, clipId: target.clipId, stems, muteOriginal: $("stems-mute").checked });
  }

  function stop() {
    if (!working) {
      closeDialog();
      return;
    }
    stopping = true;
    show();
    Hub.send("cancelStems");
  }

  // ---------------------------------------------------------------- the engine
  Hub.on("stems", (s) => {
    state = { ...state, ...s };

    if (isWorking(s)) {
      if (s.trackId && s.clipId) target = { trackId: s.trackId, clipId: s.clipId };
      // Started by the AI, with the window closed: said once where to see how far it is.
      if (!open && !announced) Hub.toast("Splitting stems: Event > Split into Stems shows how far it is.");
      announced = true;
      if (open) show();
      return;
    }

    const wasWorking = working;
    working = false;
    stopping = false;
    announced = false;
    const quiet = claimed;   // AI Tools' result card says how it ended
    if (s.state === "done" || s.state === "failed" || s.state === "cancelled") claimed = false;

    if (s.state === "done") {
      const names = (s.trackIds || []).map((id) => (Hub.trackById(id) || {}).name).filter(Boolean);
      if (open) closeDialog();
      if (!quiet) Hub.toast(names.length ? `Split into stems: ${names.join(", ")}.` : "Split into stems.");
      return;
    }

    if (s.state === "failed") {
      if (open) {
        show();
        problem(s.error || "The stems couldn't be split.");
      } else if (s.error && !quiet) {
        Hub.toast(s.error);
      }
      return;
    }

    // Cancelled, or how it stands (the answer to opening the window).
    if (open) {
      if (wasWorking || s.state === "cancelled") problem("");
      show();
    }
  });

  Hub.commands.splitStems = openDialog;

  // AI Tools' Stem Splitter starts splits of its own and shows how far they are: no note about them.
  Hub.stems = { claim: () => { announced = true; claimed = true; }, state: () => state };

  Hub.on("ready", () => {
    $("stems-go").addEventListener("click", start);
    $("stems-hide").addEventListener("click", closeDialog);
    $("stems-cancel").addEventListener("click", stop);
    for (const stem of STEMS) checkbox(stem).addEventListener("change", () => { problem(state.problem || ""); if (!working) show(); });
    $("stems-dialog").addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        event.preventDefault();
        closeDialog();   // while it works, Escape hides it: it goes on
      } else if (event.key === "Enter" && !working && event.target.tagName !== "BUTTON") {
        event.preventDefault();
        start();
      }
    });
  });
})();
