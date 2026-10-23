// D:\workspace\Fanan_web_hub\src\hub-master.js
//
// Song > Mastering Assistant (also the star on Main's strip in the Console). Sonify listens to the
// whole song through Main, then masters it with Sonify Mastering as Main's last insert.
//   Loudness    Dynamic (-16 LUFS), Streaming (-14, where the streaming services play songs)
//               or Loud (-10); the true peak stays under -1 dBTP
//   Character   Clean, Warm or Bright: which way its EQ leans
// While it works: a progress bar, and Cancel. Then the loudness and true peak before and after,
// what it changed, and listening to the master or the original; with Same loudness, the master
// is turned down to the original's loudness while comparing, so only the sound differs. Controls
// opens Sonify Mastering's settings; Remove takes the mastering out (Edit > Undo brings it back).

(() => {
  "use strict";

  const { $ } = Hub;
  const LOUDNESS = { dynamic: "Dynamic", streaming: "Streaming", loud: "Loud" };
  const CHARACTER = { clean: "Clean", warm: "Warm", bright: "Bright" };
  const BANDS = ["low", "low mid", "mid", "presence", "air"];

  let state = { state: "idle", insertId: 0 };   // the engine's last word
  let view = "setup";                            // setup, working or result
  let open = false, asked = false, comparing = false;

  const isWorking = () => state.state === "rendering" || state.state === "analysing";
  const hasMaster = () => Boolean(state.insertId && state.result);
  const chosen = (name) => (document.querySelector(`input[name="${name}"]:checked`) || {}).value;

  function show(which) {
    view = which;
    $("master-setup").hidden = which !== "setup";
    $("master-working").hidden = which !== "working";
    $("master-result").hidden = which !== "result";
    $("master-go").hidden = which === "working";
    $("master-go").textContent = which === "result" ? "Master Again…" : "Master";
    $("master-close").textContent = which === "working" ? "Cancel" : "Close";
    $("master-controls").hidden = which !== "result";
    $("master-remove").hidden = which !== "result";
    if (which === "result") renderResult();
  }

  function problem(text) {
    $("master-problem").textContent = text || "";
    $("master-problem").hidden = !text;
  }

  const number = (value, unit) => (typeof value === "number" && isFinite(value) && value > -150 ? `${value.toFixed(1)} ${unit}` : "–");
  const signed = (value) => `${value > 0 ? "+" : ""}${value.toFixed(1)} dB`;

  function renderResult() {
    const r = state.result || {};
    $("master-before-lufs").textContent = number(r.beforeLufs, "LUFS");
    $("master-before-peak").textContent = number(r.beforePeak, "dBTP");
    $("master-after-lufs").textContent = number(r.afterLufs, "LUFS");
    $("master-after-peak").textContent = number(r.afterPeak, "dBTP");

    const eq = (r.eq || []).map((db, i) => (Math.abs(db) >= 0.1 ? `${BANDS[i]} ${signed(db)}` : "")).filter(Boolean);
    const parts = [
      `${LOUDNESS[r.loudness] || "Streaming"}, ${(CHARACTER[r.character] || "Clean").toLowerCase()}`,
      eq.length ? `EQ: ${eq.join(", ")}` : "EQ: none needed",
      r.compression >= 0.1 ? `glue: ${r.compression.toFixed(1)} dB` : "glue: none needed"
    ];
    if (typeof r.limiting === "number") parts.push(`limiter: up to ${r.limiting.toFixed(1)} dB`);
    $("master-moves").textContent = `${parts.join(" · ")}.`;
    showListen();
  }

  // ---------------------------------------------------------------- comparing
  function showListen() {
    const original = comparing && Boolean(state.original);
    $("master-hear-master").setAttribute("aria-pressed", String(!original));
    $("master-hear-original").setAttribute("aria-pressed", String(original));
  }

  function listen(original) {
    comparing = true;
    state.original = original;
    Hub.send("masteringListen", { original, sameLoudness: $("master-same").checked });
    showListen();
  }

  /** The master plays again as it is: when the window closes, a new master or none. */
  function stopComparing() {
    if (!comparing) return;
    comparing = false;
    state.original = false;
    Hub.send("masteringListen", { original: false, sameLoudness: false });
  }

  // ---------------------------------------------------------------- the window
  function openDialog() {
    if (open) return;
    open = true;
    asked = true;
    comparing = false;
    problem("");
    show(isWorking() ? "working" : hasMaster() ? "result" : "setup");
    $("master-dialog").hidden = false;
    Hub.send("masteringState");   // how it stands now (a mastering kept with the song, say)
    $("master-go").focus();
  }

  function closeDialog() {
    if (isWorking()) {
      Hub.send("cancelMastering");
      return;
    }
    open = false;
    stopComparing();
    const box = $("master-dialog");
    if (box.contains(document.activeElement)) document.activeElement.blur();
    box.hidden = true;
  }

  function start() {
    if (view === "result") {
      // Again, starting from what was chosen last time.
      const r = state.result || {};
      for (const [name, value] of [["master-loudness", r.loudness], ["master-character", r.character]]) {
        const input = document.querySelector(`input[name="${name}"][value="${value}"]`);
        if (input) input.checked = true;
      }
      problem("");
      show("setup");
      return;
    }
    if (view !== "setup") return;
    stopComparing();
    problem("");
    state = { ...state, state: "rendering", progress: 0 };
    showProgress();
    Hub.send("masterSong", { loudness: chosen("master-loudness") || "streaming", character: chosen("master-character") || "clean" });
  }

  function showProgress() {
    show("working");
    // Rendering is the first half, listening and deciding the second.
    const p = Hub.clamp01(state.progress || 0);
    const fraction = state.state === "analysing" ? 0.5 + 0.5 * p : 0.5 * p;
    $("master-fill").style.width = `${Math.round(fraction * 100)}%`;
    $("master-step").textContent = state.state === "analysing" ? "Finding the master…" : "Listening to the song…";
  }

  // ---------------------------------------------------------------- the engine
  Hub.on("mastering", (m) => {
    state = { ...m };
    if (!open) return;

    // Every word from the engine says how it stands: the first after opening is the answer.
    const answer = asked;
    asked = false;

    if (isWorking()) {
      showProgress();
      return;
    }

    if (m.state === "done") {
      comparing = false;
      show("result");
      return;
    }

    if (m.state === "failed") {
      show("setup");
      problem(m.error || "The mastering didn't finish.");
      return;
    }

    // Cancelled, or how it stands (the answer to opening the window: a mastering kept with the song).
    if (m.state === "cancelled" || answer) show(hasMaster() ? "result" : "setup");
  });

  // Sonify Mastering taken out (from the Console, or undone): the window forgets it.
  Hub.on("tracks", () => {
    if (!state.insertId) return;
    const inserts = (Hub.state.master && Hub.state.master.inserts) || [];
    if (inserts.some((insert) => insert.insertId === state.insertId)) return;
    state = { ...state, insertId: 0, result: null };
    comparing = false;
    if (open && view === "result") show("setup");
  });

  Hub.commands.masteringAssistant = openDialog;

  Hub.on("ready", () => {
    $("master-go").addEventListener("click", start);
    $("master-close").addEventListener("click", closeDialog);
    $("master-hear-master").addEventListener("click", () => listen(false));
    $("master-hear-original").addEventListener("click", () => listen(true));
    $("master-same").addEventListener("change", () => { if (comparing) listen(Boolean(state.original)); });
    $("master-controls").addEventListener("click", () => {
      const insertId = state.insertId;
      closeDialog();
      if (insertId) Hub.openScreen(Hub.MAIN, insertId);
    });
    $("master-remove").addEventListener("click", () => {
      if (!state.insertId) return;
      stopComparing();
      Hub.send("removeInsert", { trackId: Hub.MAIN, insertId: state.insertId });
      state = { ...state, insertId: 0, result: null };
      show("setup");
    });
    $("master-dialog").addEventListener("keydown", (event) => {
      if (event.key === "Escape") { event.preventDefault(); closeDialog(); }
      if (event.key === "Enter" && view === "setup" && event.target.tagName !== "BUTTON") { event.preventDefault(); start(); }
    });
  });
})();
