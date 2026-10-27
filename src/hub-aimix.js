// D:\workspace\Fanan_web_hub\src\hub-aimix.js
//
// AI Mix and AI Master: Fanan AI's mixing on the Analog Board and mastering in the Mastering
// section, as a suggestion. A window over any studio (AI Mix… on the board, AI Master… in the
// Mastering window, Song > AI Mix & Master…), dragged by its top.
//   Set up     the mode (AI Mix: Mix + Master; AI Master: Master only; Sonify says which fits),
//              a target (none, a genre, or a reference track Sonify listens to) and a note
//   Working    Sonify listening to the song, Fanan AI's moves as it makes them, Sonify checking
//              the result; Stop puts everything back
//   Suggestion Fanan AI's words, the loudness and true peak before and after, A/B (the original
//              heard at the suggestion's loudness), Apply or Discard
// Native side: MainAiMix.cpp ("aiMix" events both ways).

(() => {
  "use strict";

  const { $, element } = Hub;
  let state = { phase: "idle", mode: "mix", genres: [], entries: [], suggestedMode: "mix" };
  let shown = false;
  const choice = { mode: "", target: "none", note: "" };   // the window's own choices (mode "": as Sonify suggests)

  const send = (payload) => Hub.send("aiMix", payload);
  const fmt = (value, unit) => (typeof value === "number" ? `${value.toFixed(1)} ${unit}` : value === "silent" ? "silent" : "–");

  // ---------------------------------------------------------------- views
  function setupView() {
    const box = element("div", "aimix-setup");
    const mode = choice.mode || state.suggestedMode || "mix";

    // The mode, as two lit buttons.
    const modes = element("div", "aimix-modes");
    modes.setAttribute("role", "radiogroup");
    modes.setAttribute("aria-label", "What Fanan AI works on");
    for (const [value, title, sub] of [["mix", "AI Mix", "Mix + Master, on the Analog Board"], ["master", "AI Master", "Master only: the mix stays as it is"]]) {
      const b = element("button", "aimix-mode");
      b.type = "button";
      b.setAttribute("role", "radio");
      b.setAttribute("aria-checked", String(mode === value));
      b.append(element("strong", "", title), element("span", "", sub));
      b.addEventListener("click", () => { choice.mode = value; render(); });
      modes.appendChild(b);
    }
    box.appendChild(modes);
    box.appendChild(element("p", "aimix-hint", state.suggestedMode === "master"
      ? "Sonify suggests AI Master: the song is one finished track."
      : "Sonify suggests AI Mix: the song has its own tracks to mix."));

    // The target.
    const targetRow = element("label", "aimix-row");
    targetRow.appendChild(element("span", "", "Target"));
    const select = element("select", "aimix-select");
    select.add(new Option("None: clean and balanced", "none"));
    const group = document.createElement("optgroup");
    group.label = "Genres";
    for (const genre of state.genres || []) group.appendChild(new Option(genre, `genre:${genre}`));
    select.appendChild(group);
    select.add(new Option("Match a reference track…", "reference"));
    select.value = choice.target;
    if (select.selectedIndex < 0) select.value = "none";
    select.addEventListener("change", () => {
      choice.target = select.value;
      if (choice.target === "reference" && !state.reference) send({ action: "chooseReference" });
      render();
    });
    select.addEventListener("keydown", (event) => event.stopPropagation());
    targetRow.appendChild(select);
    box.appendChild(targetRow);

    if (choice.target === "reference") {
      const ref = element("div", "aimix-reference");
      const r = state.reference;
      const text = !r ? "No reference chosen yet."
        : state.referenceBusy ? `${r.name}: Sonify is listening…`
        : r.error ? `${r.name}: ${r.error}`
        : `${r.name} · ${fmt(r.lufs, "LUFS")}`;
      ref.appendChild(element("span", "aimix-ref-name", text));
      const choose = element("button", "plain-button small", r ? "Another…" : "Choose…");
      choose.type = "button";
      choose.addEventListener("click", () => send({ action: "chooseReference" }));
      ref.appendChild(choose);
      box.appendChild(ref);
    }

    // The note.
    const note = element("textarea", "aimix-note");
    note.placeholder = "Anything to keep in mind? (optional) e.g. \"keep the vocal upfront\", \"warmer\"";
    note.rows = 2;
    note.maxLength = 1000;
    note.value = choice.note;
    note.addEventListener("input", () => { choice.note = note.value; });
    note.addEventListener("keydown", (event) => event.stopPropagation());
    box.appendChild(note);

    const go = element("button", "plain-button primary aimix-go", mode === "master" ? "Make AI Master Suggestion" : "Make AI Mix Suggestion");
    go.type = "button";
    const refReady = choice.target !== "reference" || (state.reference && !state.referenceBusy && !state.reference.error && typeof state.reference.lufs === "number");
    go.disabled = !refReady;
    go.addEventListener("click", () => {
      const target = choice.target.startsWith("genre:") ? { kind: "genre", genre: choice.target.slice(6) }
        : choice.target === "reference" ? { kind: "reference" } : { kind: "none" };
      send({ action: "start", mode, target, note: choice.note });
    });
    box.appendChild(go);
    box.appendChild(element("p", "aimix-small", "Uses Fanan AI credits. Sonify renders the song to listen, so it's quiet for a few seconds now and then. "
      + "AI suggestions are a starting point. Trust your ears."));
    return box;
  }

  /** Fanan AI's moves so far, and its words, from this suggestion's conversation. */
  function conversation() {
    const steps = [], words = [], notices = [];
    for (const entry of state.entries || []) {
      if (entry.role === "assistant") {
        for (const step of entry.steps || []) steps.push(step);
        if (entry.text) words.push(entry.text);
      } else if (entry.role === "notice" && entry.text) {
        notices.push(entry.text);
      }
    }
    return { steps, reply: words.length ? words[words.length - 1] : "", notices };
  }

  function workingView() {
    const box = element("div", "aimix-working");
    box.appendChild(element("h3", "aimix-label", state.label || "AI Suggestion"));
    const phaseText = state.phase === "measuring" ? "Sonify is listening to the song…"
      : state.phase === "checking" ? "Sonify is checking the result…"
      : "Fanan AI is working…";
    box.appendChild(element("p", "aimix-phase", phaseText));

    if (state.phase !== "working") {
      const bar = element("div", "export-bar");
      const fill = element("i");
      fill.style.width = `${Math.round(Hub.clamp01(state.progress || 0) * 100)}%`;
      bar.appendChild(fill);
      box.appendChild(bar);
    }

    const { steps } = conversation();
    if (steps.length) {
      const list = element("ul", "aimix-steps");
      for (const step of steps) list.appendChild(element("li", step.ok === false ? "failed" : "", step.text));
      box.appendChild(list);
    }

    const stop = element("button", "plain-button", "Stop");
    stop.type = "button";
    stop.title = "Stop: everything goes back as it was";
    stop.addEventListener("click", () => send({ action: "stop" }));
    box.appendChild(stop);
    return box;
  }

  function suggestionView() {
    const box = element("div", "aimix-suggestion");
    box.appendChild(element("h3", "aimix-label", state.label || "AI Suggestion"));
    const { reply, steps } = conversation();
    if (reply) box.appendChild(element("div", "aimix-reply", reply));
    if (steps.length) {
      const details = document.createElement("details");
      details.appendChild(element("summary", "", `What changed (${steps.length})`));
      const list = element("ul", "aimix-steps");
      for (const step of steps) list.appendChild(element("li", step.ok === false ? "failed" : "", step.text));
      details.appendChild(list);
      box.appendChild(details);
    }

    // Before and after.
    if (state.before || state.after) {
      const numbers = element("div", "aimix-numbers");
      const column = (title, n) => {
        const c = element("div", "aimix-number");
        c.append(element("span", "", title), element("strong", "", fmt(n && n.lufs, "LUFS")), element("small", "", `peak ${fmt(n && n.truePeak, "dB")}`));
        return c;
      };
      numbers.append(column("Before", state.before), element("span", "aimix-arrow", "\u2192"), column("Suggestion", state.after));
      box.appendChild(numbers);
    }

    // A/B.
    const ab = element("div", "aimix-ab");
    ab.setAttribute("role", "radiogroup");
    ab.setAttribute("aria-label", "Listen to");
    const trim = typeof state.trim === "number" && Math.abs(state.trim) >= 0.1
      ? ` (${state.trim > 0 ? "+" : ""}${state.trim.toFixed(1)} dB to match)` : "";
    for (const [value, label] of [["suggestion", "Suggestion"], ["before", `Before${trim}`]]) {
      const b = element("button", "aimix-ab-button", label);
      b.type = "button";
      b.setAttribute("role", "radio");
      b.setAttribute("aria-checked", String(state.listening === value));
      b.addEventListener("click", () => send({ action: "listen", which: value }));
      ab.appendChild(b);
    }
    box.appendChild(ab);
    box.appendChild(element("p", "aimix-small", "Before is heard at the suggestion's loudness, so louder doesn't fool the ear. "
      + "AI suggestions are a starting point. Trust your ears."));

    const buttons = element("div", "aimix-buttons");
    const apply = element("button", "plain-button primary", "Apply");
    apply.type = "button";
    apply.title = "Keep the suggestion (its knobs stay where Fanan AI put them; change any of them)";
    apply.addEventListener("click", () => send({ action: "apply" }));
    const discard = element("button", "plain-button", "Discard");
    discard.type = "button";
    discard.title = "Everything back as it was";
    discard.addEventListener("click", () => send({ action: "discard" }));
    buttons.append(apply, discard);
    box.appendChild(buttons);
    return box;
  }

  function render() {
    if (!shown) return;
    const body = $("aimix-body");
    const focusedNote = document.activeElement && document.activeElement.classList.contains("aimix-note");
    const view = state.phase === "suggestion" ? suggestionView()
      : state.phase === "idle" ? setupView() : workingView();
    const nodes = [view];

    // A problem, or what Fanan AI's side said when the request couldn't go.
    const { notices } = conversation();
    const problem = state.problem || (state.phase === "idle" && notices.length ? notices[notices.length - 1] : "");
    if (problem) nodes.unshift(element("p", "aimix-problem", problem));
    body.replaceChildren(...nodes);

    if (focusedNote) {
      const note = body.querySelector(".aimix-note");
      if (note) { note.focus(); note.selectionStart = note.selectionEnd = note.value.length; }
    }
  }

  // ---------------------------------------------------------------- the window
  function open(mode) {
    const panel = $("aimix-panel");
    if (mode) choice.mode = mode;
    if (!shown) {
      shown = true;
      panel.hidden = false;
      if (!panel.dataset.placed) {
        panel.dataset.placed = "1";
        panel.style.left = `${Math.max(16, Math.round((window.innerWidth - panel.offsetWidth) / 2 + 40))}px`;
        panel.style.top = `${Math.max(56, Math.round((window.innerHeight - panel.offsetHeight) / 4))}px`;
      }
      send({ action: "shown", shown: true });
    }
    render();
    panel.focus();
  }

  function close() {
    if (!shown) return;
    shown = false;
    $("aimix-panel").hidden = true;
    send({ action: "shown", shown: false });
  }

  Hub.on("aiMix", (data) => {
    state = data || state;
    render();
  });

  Hub.aiMix = { open, close };
  Hub.commands.aiMixMaster = () => open();

  Hub.on("ready", () => {
    const panel = $("aimix-panel");
    $("aimix-close").addEventListener("click", close);
    panel.addEventListener("keydown", (event) => {
      if (event.key === "Escape") { event.preventDefault(); event.stopPropagation(); close(); }
    });
    const board = $("board-aimix");
    if (board) board.addEventListener("click", () => open("mix"));
    const master = $("mastering-aimaster");
    if (master) master.addEventListener("click", () => open("master"));

    // Dragged by its top.
    const head = $("aimix-head");
    let move = null;
    head.addEventListener("pointerdown", (event) => {
      if (event.button !== 0 || event.target.closest("button, select")) return;
      move = { x: event.clientX - panel.offsetLeft, y: event.clientY - panel.offsetTop };
      head.setPointerCapture(event.pointerId);
    });
    head.addEventListener("pointermove", (event) => {
      if (!move) return;
      panel.style.left = `${Math.min(window.innerWidth - 120, Math.max(0, event.clientX - move.x))}px`;
      panel.style.top = `${Math.min(window.innerHeight - 40, Math.max(0, event.clientY - move.y))}px`;
    });
    const stop = () => { move = null; };
    head.addEventListener("pointerup", stop);
    head.addEventListener("pointercancel", stop);
  });
})();
