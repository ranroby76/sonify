// D:\workspace\Fanan_web_hub\src\hub-ai.js
//
// Fanan AI, the AI Assistant: ask in words, and it does it in the song (tracks, instruments,
// effect chains, sidechains, plugin settings, notes, tempo). Each request is one undo step (Ctrl+Z
// takes back everything it did). The conversation is in a window of its own over the studios: the
// transport's AI Assistant button (lower right) or View > AI Assistant opens and closes it; its bar
// drags it, its corner sizes it, and it opens again where it was left (kept on this computer). It
// stays out of the Plugins Creator Studio and the Web Browser, whose pages are Sonify's own windows
// (it comes back with the next studio), and closing it keeps the conversation.
//
// The flag, on the Send line: a request can be flagged with the studio it's about (Studio, Analog
// Board, Song Builder Blocks, Plugins Creator Studio, Live Show Studio, Sampling Studio, Web
// Browser), so Fanan AI
// takes it in that studio's terms. A choice, not a must: "No flag" leaves it to the AI. The flag
// stays until it's changed, and each flagged request shows its flag in the conversation. It works through Fanan's server with the Fanan
// account's credits (the same as Stella AI Studio's), so it needs the user signed in and
// the internet allowed.
//
// Answers show as Stella AI's do: short plain words, bold and italic as fonts, bullets as
// dots, and what the user should answer or do in yellow (==marked== by the AI, or any
// question when it marked none). The marks never show, and code never shows.
//
// Voice control (the row above the box, when Sonify offers it): the musician talks to Sonify
// from the recording room, into the mic on the chosen input: "Sonify", what they want, then
// "over". Sonify answers with beeps; what was said shows here, and requests for Fanan AI in
// the conversation, marked with a mic.
//
// Phone remote (the row under it, when Sonify offers it): Fanan AI from a phone, by text. In the
// studio, the phone scans the QR code (same Wi-Fi: Sonify serves the page itself); away, it opens
// the page /sonify of Fanan's site, signed in to the same account. Its requests show in the
// conversation like typed ones.

(() => {
  "use strict";

  const { $, element } = Hub;
  let state = { busy: false, stopping: false, entries: [], configured: true, blocked: false, status: "checking",
                signedIn: false, credits: -1, siteAvailable: false, deep: false, deepNow: false };
  let voice = { offered: false, enabled: false, why: "", stage: "off", heard: "", message: "", input: -1, inputs: [] };
  let remote = { offered: false, enabled: false, why: "", connected: false, problem: "", url: null, email: "",   // url: null until Sonify says
                 localUrl: "", localProblem: "", localSeen: false };
  let qrShown = "";   // the address the QR code was drawn for

  const EXAMPLES = [
    "Add a synth on a new track and write an 8-bar chord progression in A minor",
    "Put a compressor on the bass and sidechain it to the kick",
    "Make a reverb FX channel and send the keys to it",
  ];

  // ---------------------------------------------------------------- answers, as Stella AI shows them
  /** A line's inline marks, each only when it's closed on the same line: **bold**, *italic*,
      `code` (shown as plain words), and ==a question or a task for the user== (yellow). */
  function parseLine(line, bold) {
    const runs = [];
    let italic = false, code = false, ask = false;
    let current = { text: "", bold, italic, code, ask };

    const flush = () => {
      if (current.text) runs.push(current);
      current = { text: "", bold, italic, code, ask };
    };
    const closedLater = (mark, from) => line.indexOf(mark, from) >= 0;

    for (let i = 0; i < line.length;) {
      const rest = line.slice(i);
      if (!code && rest.startsWith("**") && (bold || closedLater("**", i + 2))) {
        flush(); bold = !bold; current.bold = bold; i += 2;
      } else if (!code && rest.startsWith("==") && (ask || closedLater("==", i + 2))) {
        flush(); ask = !ask; current.ask = ask; i += 2;
      } else if (line[i] === "`" && (code || closedLater("`", i + 1))) {
        flush(); code = !code; current.code = code; i += 1;
      } else if (!code && line[i] === "*" && (italic || (i + 1 < line.length && line[i + 1] !== " " && closedLater("*", i + 1)))) {
        flush(); italic = !italic; current.italic = italic; i += 1;
      } else {
        current.text += line[i]; i += 1;
      }
    }

    flush();
    return runs;
  }

  /** When an answer marks nothing for the user, its questions are found by their "?". */
  function markQuestions(line) {
    let result = "", sentence = "";
    for (let i = 0; i < line.length; i++) {
      sentence += line[i];
      const end = line[i] === "." || line[i] === "!" || line[i] === "?";
      if (end && (i + 1 === line.length || line[i + 1] === " ")) {
        const trimmed = sentence.trim();
        result += line[i] === "?" && trimmed ? sentence.slice(0, sentence.length - trimmed.length) + "==" + trimmed + "==" : sentence;
        sentence = "";
      }
    }
    return result + sentence;
  }

  function renderAnswer(text) {
    // A fenced block of code is left out, whatever the AI wrote.
    const lines = [];
    let inCode = false;
    for (const line of text.split(/\r?\n/)) {
      if (line.trimStart().startsWith("```")) inCode = !inCode;
      else if (!inCode) lines.push(line);
    }
    while (lines.length && !lines[lines.length - 1].trim()) lines.pop();

    const marked = text.includes("==");
    const box = element("div", "ai-text");

    for (const original of lines) {
      let line = original;
      const trimmed = line.trimStart();
      let kind = "";

      if (trimmed === "---" || trimmed === "***" || trimmed === "___") line = "";
      else if (trimmed.startsWith("#")) { line = trimmed.replace(/^#+\s*/, ""); kind = " heading"; }
      else if (/^([-*]|\u2022) /.test(trimmed)) { line = trimmed.slice(2); kind = " bullet"; }
      else if (trimmed.startsWith("> ")) line = trimmed.slice(2);

      if (!marked) line = markQuestions(line);

      const row = element("div", "ai-line" + kind);
      row.dir = "auto";   // Hebrew, Arabic...: right to left
      for (const run of parseLine(line, kind === " heading")) {
        const span = element("span", [run.bold && "ai-bold", run.italic && "ai-italic", run.ask && "ai-ask"].filter(Boolean).join(" "), run.text);
        row.appendChild(span);
      }
      box.appendChild(row);
    }

    return box;
  }

  // ---------------------------------------------------------------- the conversation
  function render() {
    const log = $("ai-log");
    const nearBottom = log.scrollHeight - log.scrollTop - log.clientHeight < 40;
    log.replaceChildren();

    if (!state.entries.length) {
      const hello = element("div", "ai-hello");
      hello.appendChild(element("p", "", "Tell Fanan AI what you want in your song, and it does it while you listen. Undo takes back everything it did."));
      for (const text of EXAMPLES) {
        const example = element("button", "ai-example", text);
        example.type = "button";
        example.addEventListener("click", () => { $("ai-input").value = text; $("ai-input").focus(); });
        hello.appendChild(example);
      }
      log.appendChild(hello);
    }

    for (const entry of state.entries) {
      if (entry.role === "user" || entry.role === "notice") {
        if (entry.role === "user" && entry.studio) log.appendChild(flagChip(entry.studio));
        const line = element("div", entry.role === "user" ? "ai-user" : "ai-error", entry.text);
        line.dir = "auto";
        if (entry.spoken) {
          line.classList.add("spoken");
          line.title = "Said into the mic";
          line.prepend(micIcon());
        }
        log.appendChild(line);
        continue;
      }

      const steps = entry.steps || [];
      if (!steps.length && !entry.text) continue;   // nothing to show yet

      const box = element("div", "ai-assistant");
      if (steps.length) {
        const list = element("ul", "ai-steps");
        for (const step of steps) {
          const item = element("li", "ai-step" + (step.ok ? "" : " failed"));
          item.append(element("span", "ai-step-mark", step.ok ? "\u2713" : "\u2715"), element("span", "", step.text));
          list.appendChild(item);
        }
        box.appendChild(list);
      }
      if (entry.text) box.appendChild(renderAnswer(entry.text));
      log.appendChild(box);
    }

    if (state.busy) log.appendChild(element("div", "ai-working", state.stopping ? "Stopping after this step\u2026" : "Fanan AI is working\u2026"));
    if (nearBottom || state.busy) log.scrollTop = log.scrollHeight;

    // Why it can't work right now, if it can't, and what to do about it.
    const [reason, action, actionText] = whyNot();
    $("ai-notice").hidden = !reason;
    $("ai-notice-text").textContent = reason;
    $("ai-notice-action").hidden = !action;
    $("ai-notice-action").textContent = actionText || "";
    $("ai-notice-action").onclick = action;
    $("ai-input").disabled = Boolean(reason);
    $("ai-send").disabled = Boolean(reason);
    $("ai-send").hidden = state.busy;
    $("ai-stop").hidden = !state.busy;
    $("ai-stop").disabled = state.stopping;
    $("ai-clear").disabled = state.busy || !state.entries.length;
    renderDeep();

    // The account's credits, with a way to get more.
    const credits = $("ai-credits");
    credits.replaceChildren();
    if (state.signedIn && state.credits >= 0) {
      credits.appendChild(document.createTextNode(`${state.credits} ${state.credits === 1 ? "credit" : "credits"}`));
      if (state.siteAvailable) {
        const buy = element("button", "ai-buy", "Buy credits");
        buy.type = "button";
        buy.title = "Buy Fanan AI credits on the Fanan site: they work in every Fanan app";
        buy.addEventListener("click", () => Hub.run("buyCredits"));
        credits.append(" · ", buy);
      }
    }
  }

  /** [why Fanan AI can't work right now, what a button does about it, the button's text]. */
  function whyNot() {
    const tryAgain = () => Hub.send("accountCheck");
    if (!state.configured) return ["Fanan AI isn't set up in this build yet."];
    if (state.blocked) return ["The internet is blocked (top left): Fanan AI works through it."];
    if (state.status === "checking") return ["Connecting to Fanan's server…"];
    if (state.status === "noInternet") return ["No internet connection: Fanan AI works through it.", tryAgain, "Try again"];
    if (state.status !== "online") return ["Fanan's server isn't responding right now.", tryAgain, "Try again"];
    if (!state.signedIn) return ["Sign in to use Fanan AI. It's your Fanan account: its Fanan AI credits work in every Fanan app.", () => Hub.run("account"), "Sign in"];
    if (state.credits === 0) return ["You're out of credits.", () => Hub.run("buyCredits"), "Buy credits"];
    return [""];
  }

  function send() {
    const text = $("ai-input").value.trim();
    if (!text || state.busy || $("ai-input").disabled) return;
    const studio = $("ai-flag").value;
    Hub.send("aiSend", studio ? { text, studio } : { text });
    $("ai-input").value = "";
  }

  // ---------------------------------------------------------------- Deep
  /** Deep: every request to Fanan AI's strongest model. Off, the quick one takes it, and a long
      request (one that has taken many steps) finishes deep by itself: the button says so while
      it does. */
  function renderDeep() {
    const button = $("ai-deep");
    const stepped = Boolean(state.deepNow) && !state.deep;
    button.classList.toggle("on", Boolean(state.deep));
    button.classList.toggle("stepped", stepped);
    button.setAttribute("aria-pressed", state.deep ? "true" : "false");
    button.title = state.deep
      ? "Deep is on: every request goes to Fanan AI's strongest model (it costs twice the credits). Press to turn it off."
      : stepped
        ? "This request got long, so Fanan AI went deep by itself to finish it. Press to keep every request deep."
        : "Deep: every request to Fanan AI's strongest model, for big jobs (twice the credits). Off, the quick model takes it and a long request finishes deep by itself.";
  }

  // ---------------------------------------------------------------- the flag
  const FLAG_ICON = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M3.5 14.5V2M3.5 2.5h8.2l-1.8 3 1.8 3H3.5" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linejoin="round" stroke-linecap="round"/></svg>';

  /** A studio's colour, as its tab in the top bar has it. */
  function flagColour(studio) {
    const option = [...$("ai-flag").options].find((o) => o.value === studio);
    return option && option.dataset.colour ? option.dataset.colour : "";
  }

  /** The flag over a request in the conversation. */
  function flagChip(studio) {
    const chip = element("div", "ai-flag-chip");
    chip.insertAdjacentHTML("afterbegin", FLAG_ICON);
    chip.appendChild(element("span", "", studio));
    const colour = flagColour(studio);
    if (colour) chip.style.setProperty("--flag", colour);
    chip.title = `Flagged: about the ${studio}`;
    return chip;
  }

  function renderFlag() {
    const studio = $("ai-flag").value;
    const box = $("ai-flag-box");
    box.classList.toggle("on", Boolean(studio));
    const colour = flagColour(studio);
    if (colour) box.style.setProperty("--flag", colour);
    else box.style.removeProperty("--flag");
    box.title = studio ? `Flagged: the request is about the ${studio}. Choose "No flag" to take it off.`
                       : "Flag the request with the studio it's about, so Fanan AI takes it in that studio's terms (a choice, not a must)";
  }

  // ---------------------------------------------------------------- voice control
  function micIcon() {
    const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.setAttribute("viewBox", "0 0 16 16");
    svg.setAttribute("class", "ai-mic");
    svg.setAttribute("aria-hidden", "true");
    svg.innerHTML = '<rect x="5.5" y="1.5" width="5" height="8.5" rx="2.5" fill="none" stroke="currentColor" stroke-width="1.4"/>'
                  + '<path d="M3.5 7.5a4.5 4.5 0 009 0M8 12v2.5" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"/>';
    return svg;
  }

  function renderVoice() {
    $("ai-voice").hidden = !voice.offered;

    const toggle = $("voice-toggle");
    toggle.setAttribute("aria-checked", voice.enabled ? "true" : "false");
    toggle.classList.toggle("on", voice.enabled);
    toggle.classList.toggle("live", voice.enabled && voice.stage !== "off");

    // The song's inputs: the one the musician's mic is on.
    const select = $("voice-input");
    const names = ["Choose the mic's input…", ...voice.inputs];
    if (select.options.length !== names.length || names.some((name, i) => select.options[i].textContent !== name)) {
      select.replaceChildren(...names.map((name, i) => {
        const option = element("option", "", name);
        option.value = String(i - 1);
        return option;
      }));
    }
    select.value = String(voice.input >= 0 && voice.input < voice.inputs.length ? voice.input : -1);

    // What it's doing, or why it can't.
    let text, kind = "";
    if (!voice.enabled) text = "Talk to Sonify from the recording room: say “Sonify”, what you want, then “over”. It answers with beeps. What this mic hears goes to Fanan's server to be understood.";
    else if (voice.why) { text = voice.why; kind = " warn"; }
    else if (voice.stage === "command") { text = voice.heard ? `“${voice.heard}”… then say “over”.` : "Go on, then say “over”."; kind = " command"; }
    else text = "Listening for “Sonify”…";

    const status = $("voice-status");
    status.textContent = text;
    status.className = "voice-status" + kind;

    // The last command, or what went wrong.
    const last = $("voice-last");
    const showing = voice.enabled && !voice.why && voice.stage === "listening";
    last.textContent = !showing ? "" : voice.message ? voice.message : voice.heard ? `Last: “${voice.heard}”` : "";
    last.className = "voice-last" + (showing && voice.message ? " problem" : "");
  }

  // ---------------------------------------------------------------- phone remote
  function drawQr(text) {
    if (text === qrShown) return;
    qrShown = text;

    const box = $("remote-qr");
    box.replaceChildren();
    if (!text || typeof qrcode !== "function") return;

    const code = qrcode(0, "M");
    code.addData(text);
    code.make();

    // As an SVG of squares (the library's own markup isn't used: it's built here, safely).
    const count = code.getModuleCount();
    const ns = "http://www.w3.org/2000/svg";
    const svg = document.createElementNS(ns, "svg");
    svg.setAttribute("viewBox", `0 0 ${count} ${count}`);
    svg.setAttribute("shape-rendering", "crispEdges");
    let d = "";
    for (let row = 0; row < count; row++)
      for (let col = 0; col < count; col++)
        if (code.isDark(row, col)) d += `M${col} ${row}h1v1h-1z`;
    const path = document.createElementNS(ns, "path");
    path.setAttribute("d", d);
    path.setAttribute("fill", "#000");
    svg.append(path);
    box.append(svg);
  }

  function renderRemote() {
    $("ai-remote").hidden = !remote.offered;

    const toggle = $("remote-toggle");
    toggle.setAttribute("aria-checked", remote.enabled ? "true" : "false");
    toggle.classList.toggle("on", remote.enabled);
    toggle.classList.toggle("live", remote.enabled && (remote.connected || remote.localSeen));

    // In the studio: the QR code, while Sonify serves the page on this network.
    const localShown = remote.enabled && (Boolean(remote.localUrl) || Boolean(remote.localProblem));
    $("remote-local").hidden = !localShown;
    $("remote-away-title").hidden = !remote.enabled;
    drawQr(remote.enabled ? remote.localUrl : "");
    $("remote-qr").hidden = !remote.localUrl;
    $("remote-new-key").hidden = !remote.localUrl;

    const local = $("remote-local-status");
    local.textContent = remote.localProblem ? remote.localProblem
                      : remote.localSeen ? "A phone is connected."
                      : "Scan this with the phone's camera (the phone on this computer's Wi-Fi).";
    local.className = "voice-status" + (remote.localProblem ? " warn" : remote.localSeen ? " command" : "");

    // Away: the page's address on Fanan's site.
    const url = element("span", "remote-url", remote.url);
    const status = $("remote-status");
    let kind = "";
    status.replaceChildren();

    if (remote.url === null) {
      // Not heard from Sonify yet.
    } else if (!remote.enabled) {
      status.append("Talk to Fanan AI from your phone by text: in the studio by scanning a QR code, away on ",
                    remote.url ? url : "Fanan's site", ". Turn this on to start.");
    } else if (!remote.url) {
      status.append("Away from the studio, Sonify Remote isn't set up in this build yet.");
    } else if (remote.why) {
      status.append(remote.why);
      kind = " warn";
    } else if (remote.connected) {
      status.append("On your phone, open ", url, remote.email ? ` signed in as ${remote.email}.` : ".");
      kind = " command";
    } else if (remote.problem) {
      status.append(remote.problem);
      kind = " warn";
    } else {
      status.append("Connecting\u2026");
    }

    status.className = "voice-status" + kind;
  }

  Hub.on("aiState", (data) => {
    state = { ...state, ...data };
    render();
  });

  // Fanan AI drives Sonify as its screen does: a command (a menu's or key's action), or an event
  // sent exactly as the page sends it.
  Hub.on("aiRun", (data) => {
    if (data.command) {
      const run = Hub.commands[data.command];
      if (typeof run === "function") run();
    } else if (data.event) {
      Hub.send(data.event, data.payload || {});
    }
  });

  Hub.on("remoteState", (data) => {
    remote = { ...remote, ...data };
    renderRemote();
  });

  Hub.on("voiceState", (data) => {
    voice = { ...voice, ...data };
    renderVoice();
  });

  // ---------------------------------------------------------------- the AI Assistant window
  const GEOMETRY_KEY = "sonify.aiAssistant.geometry";
  const MIN_W = 320, MIN_H = 300, EDGE = 8;
  let windowOpen = false;
  let geometry = null;   // { left, top, width, height }, in the page's pixels

  /** A studio whose page is Sonify's own window (it would cover the assistant). */
  const covered = () => document.body.classList.contains("creator-mode") || document.body.classList.contains("web-mode");

  /** Where it may be: under the top bar, over the transport, inside the window. */
  function bounds() {
    const bar = $("top-bar"), transport = $("transport");
    const top = (bar ? bar.getBoundingClientRect().bottom : 0) + 4;
    const bottom = window.innerHeight - (transport ? transport.offsetHeight : 0) - 4;
    return { left: EDGE, top, right: window.innerWidth - EDGE, bottom: Math.max(top + 120, bottom) };
  }

  /** The first time: at the lower right, over the transport's button. */
  function firstGeometry() {
    const b = bounds();
    const width = Math.min(440, b.right - b.left), height = Math.min(620, b.bottom - b.top);
    return { left: b.right - width, top: b.bottom - height, width, height };
  }

  function fit(g) {
    const b = bounds();
    const width = Hub.clamp(g.width, Math.min(MIN_W, b.right - b.left), b.right - b.left);
    const height = Hub.clamp(g.height, Math.min(MIN_H, b.bottom - b.top), b.bottom - b.top);
    return { left: Hub.clamp(g.left, b.left, b.right - width), top: Hub.clamp(g.top, b.top, b.bottom - height), width, height };
  }

  function place() {
    geometry = fit(geometry || firstGeometry());
    const box = $("ai-window");
    box.style.left = `${Math.round(geometry.left)}px`;
    box.style.top = `${Math.round(geometry.top)}px`;
    box.style.width = `${Math.round(geometry.width)}px`;
    box.style.height = `${Math.round(geometry.height)}px`;
  }

  function saveGeometry() {
    try { localStorage.setItem(GEOMETRY_KEY, JSON.stringify(geometry)); } catch (error) { /* no storage here */ }
  }

  function loadGeometry() {
    try {
      const g = JSON.parse(localStorage.getItem(GEOMETRY_KEY) || "null");
      if (g && [g.left, g.top, g.width, g.height].every((v) => typeof v === "number" && isFinite(v))) geometry = g;
    } catch (error) { /* no storage here */ }
  }

  function renderWindow() {
    const away = covered();
    $("ai-window").hidden = !windowOpen || away;
    const button = $("ai-window-button");
    button.setAttribute("aria-pressed", String(windowOpen));
    button.disabled = away;
    button.title = away ? "AI Assistant: open it from any other studio (this studio's page is a window of its own, over the page)"
                        : windowOpen ? "AI Assistant: click to close it (the conversation stays)"
                        : "AI Assistant: Fanan AI in a window over the studio. Ask in words and it does the work in your song";
  }

  function openWindow() {
    if (covered()) return;
    windowOpen = true;
    renderWindow();
    place();
    render();
    const log = $("ai-log");
    log.scrollTop = log.scrollHeight;   // the latest is at the bottom
    if (!$("ai-input").disabled) $("ai-input").focus();
  }

  function closeWindow() {
    windowOpen = false;
    const box = $("ai-window");
    if (box.contains(document.activeElement)) document.activeElement.blur();
    renderWindow();
  }

  const toggleWindow = () => (windowOpen ? closeWindow() : openWindow());

  /** Its bar moves it, its corner sizes it (kept for next time). */
  function dragFrom(event, sizing) {
    if (event.button !== 0 || (!sizing && event.target.closest("button"))) return;
    event.preventDefault();
    const handle = event.currentTarget;
    const start = { x: event.clientX, y: event.clientY, g: { ...geometry } };
    handle.setPointerCapture(event.pointerId);
    $("ai-window").classList.add("moving");

    const move = (e) => {
      const dx = e.clientX - start.x, dy = e.clientY - start.y, g = start.g;
      if (sizing) {
        const b = bounds();
        geometry = fit({ left: g.left, top: g.top, width: Math.min(g.width + dx, b.right - g.left), height: Math.min(g.height + dy, b.bottom - g.top) });
      } else {
        geometry = fit({ ...g, left: g.left + dx, top: g.top + dy });
      }
      place();
    };
    const end = () => {
      handle.removeEventListener("pointermove", move);
      handle.removeEventListener("pointerup", end);
      handle.removeEventListener("pointercancel", end);
      $("ai-window").classList.remove("moving");
      saveGeometry();
    };
    handle.addEventListener("pointermove", move);
    handle.addEventListener("pointerup", end);
    handle.addEventListener("pointercancel", end);
  }

  Hub.commands.aiAssistant = toggleWindow;
  Hub.on("studio", renderWindow);

  Hub.ai = { state: () => state, voice: () => voice, remote: () => remote, renderAnswer,
             open: openWindow, close: closeWindow, toggle: toggleWindow, isOpen: () => windowOpen };

  Hub.on("ready", () => {
    $("ai-input").dir = "auto";
    $("ai-flag").addEventListener("change", () => { renderFlag(); $("ai-input").focus(); });
    renderFlag();
    $("ai-send").addEventListener("click", send);
    $("ai-stop").addEventListener("click", () => Hub.send("aiStop"));
    $("ai-clear").addEventListener("click", () => Hub.send("aiClear"));
    $("ai-deep").addEventListener("click", () => Hub.send("aiSettings", { deep: !state.deep }));
    $("ai-input").addEventListener("keydown", (event) => {
      if (event.key === "Enter" && !event.shiftKey) {
        event.preventDefault();
        send();
      }
    });
    $("voice-toggle").addEventListener("click", () => {
      voice = { ...voice, enabled: !voice.enabled };
      renderVoice();
      Hub.send("voiceSettings", { enabled: voice.enabled });
    });
    $("voice-input").addEventListener("change", (event) => {
      voice = { ...voice, input: Number(event.target.value) };
      Hub.send("voiceSettings", { input: voice.input });
    });
    $("remote-toggle").addEventListener("click", () => {
      remote = { ...remote, enabled: !remote.enabled, connected: false };
      renderRemote();
      Hub.send("remoteSettings", { enabled: remote.enabled });
    });
    $("remote-new-key").addEventListener("click", () => {
      if (window.confirm("Make a new pairing code? Phones paired before will have to scan the QR code again."))
        Hub.send("remoteSettings", { newKey: true });
    });

    // The page's commands, for Fanan AI, once every part of the page has added its own.
    setTimeout(() => Hub.send("aiCommandList", { names: Object.keys(Hub.commands).sort() }), 0);

    loadGeometry();
    $("ai-window-button").addEventListener("click", toggleWindow);
    $("ai-window-close").addEventListener("click", closeWindow);
    $("ai-window-bar").addEventListener("pointerdown", (event) => dragFrom(event, false));
    $("ai-window-bar").addEventListener("dblclick", (event) => {
      if (event.target.closest("button")) return;
      geometry = firstGeometry();   // back to where it starts
      place();
      saveGeometry();
    });
    $("ai-window-grip").addEventListener("pointerdown", (event) => dragFrom(event, true));
    $("ai-window").addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        event.preventDefault();
        event.stopPropagation();
        closeWindow();
      }
    });
    window.addEventListener("resize", () => { if (windowOpen && geometry) place(); });

    render();
    renderVoice();
    renderRemote();
    renderWindow();
  });
})();
