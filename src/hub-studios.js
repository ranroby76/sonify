// D:\workspace\Fanan_web_hub\src\hub-studios.js
//
// The studios: Sonify's main pages, one tab each in the selector in the middle of the top bar, all
// there from the moment Sonify starts:
//   Studio                   the DAW: the arrangement, the Editor, the Console and the Browser
//   Analog Board             the song mixed on an analog desk (hub-board.js)
//   AI Tools                 Sonify's AI musical tools, each a small studio in a tab: Stem
//                            Splitter, Audio to MIDI, MIDI Generators, Tempo & Pitch and the
//                            Mastering Assistant (hub-aitools.js). Fanan AI, the assistant, is
//                            the AI Assistant window over every studio (hub-ai.js)
//   Song Builder Blocks      the song built from blocks of clips (hub-blocks.js)
//   Plugins Creator Studio   everything new is made here by asking Fanan AI: instruments and
//                            effects for the tracks (Stella AI Studio, Sonify's own window over
//                            the page: hub-creator.js), and MIDI devices in its MIDI Devices tab
//                            (hub-devices.js)
//   Live Show Studio         the DJ's decks and mixer with the singers and players on it, in
//                            place of the song while it shows or plays (hub-live.js)
//   Sampling Studio          an instrument track, a synth or the user playing, sampled note by
//                            note into an SFZ for the Sonify Sampler; and its Builder, an
//                            instrument from audio files and clips put on keys (hub-sampling.js)
//   Web Browser              the internet in tabs of its own, with + Open new tab (hub-web.js).
//                            HIDDEN for now (hidden: true below): its tab and its View menu item
//                            are taken out of the top bar, out of Ctrl+Tab and out of Ctrl+8, and
//                            nothing can open it. Everything it does is still here: take the one
//                            word away and it comes back where it was.
//   Guide                    Sonify's user guide in tabs and chapters, and Fanan AI answering
//                            anything about Sonify from it (hub-guide.js)
// MIDI in is heard by the studio in front only, so nothing plays behind it.
// The transport stays at the bottom of every studio, so the song plays and records wherever you
// are; its keys (Start / Stop, Record, Loop, the metronome) and Undo / Redo work in every studio
// too (hub-keys.js). Opening a studio from the Start page closes it, as Esc does.
// Keys: Ctrl+1 .. Ctrl+9 a studio, Ctrl+Tab / Ctrl+Shift+Tab the next / previous one.
// The top bar also has the internet switch and the Fanan account (left), and the logos (right).

(() => {
  "use strict";

  const { $ } = Hub;

  // Each studio: its id (the selector's data-studio), its page (none: the DAW, which is the page
  // itself) and the body class while it shows. hidden: not offered for now - its tab, its menu
  // item and its key are out, and activate() sends anyone asking for it back to the Studio. Its
  // page stays where it is and still hears show(false), so nothing of it is lost.
  const STUDIOS = [
    { id: "studio",  page: null,            mode: "" },
    { id: "board",   page: "analog-board",  mode: "board-mode" },
    { id: "ai",      page: "ai-studio",     mode: "ai-mode" },
    { id: "blocks",  page: "song-blocks",   mode: "blocks-mode" },
    { id: "plugins", page: "creator-studio", mode: "creator-mode" },
    { id: "live",    page: "live-studio",   mode: "live-mode" },
    { id: "sampling", page: "sampling-studio", mode: "sampling-mode" },
    { id: "web",     page: "web-browser",   mode: "web-mode", hidden: true },
    { id: "guide",   page: "guide-studio",  mode: "guide-mode" }
  ];
  const IDS = STUDIOS.map((studio) => studio.id);

  // The ones offered: Ctrl+Tab goes round these, and a hidden studio's key does nothing. The keys
  // stay where they are - each studio keeps the number of its place in STUDIOS, so hiding one
  // never moves another's (the Guide stays Ctrl+9).
  const SHOWN = STUDIOS.filter((studio) => ! studio.hidden).map((studio) => studio.id);
  const isShown = (id) => SHOWN.includes(id);
  let active = "studio";

  /** The Start page goes, as its × or Esc does: the song behind it is the project now. */
  function closeStartPage() {
    const start = $("start-page");
    if (!start || start.hidden) return;
    start.hidden = true;
    Hub.send("startPageClosed");
  }

  function renderSwitch() {
    for (const button of document.querySelectorAll(".studio-tab")) {
      const on = button.dataset.studio === active;
      button.setAttribute("aria-selected", String(on));
      button.tabIndex = on ? 0 : -1;
    }
  }

  /** Shows a studio; the others' pages go. From the Start page, the Start page goes too
      (not as the page first draws itself: the Start page greets you then). */
  function activate(id, leaveStart = true) {
    const asked = STUDIOS.find((s) => s.id === id);

    // A studio that isn't offered for now stays shut: whoever asks (a command, a menu item a
    // plugin's window relayed, the Start page) is left where they are rather than moved.
    if (asked && asked.hidden) return;

    const studio = asked || STUDIOS[0];
    const was = active;
    active = studio.id;
    if (leaveStart) closeStartPage();

    for (const s of STUDIOS) {
      if (s.page) $(s.page).hidden = s.id !== active;
      if (s.mode) document.body.classList.toggle(s.mode, s.id === active);
    }
    document.body.classList.toggle("page-mode", active !== "studio");

    // Each page hears whether it shows: the board tells Sonify, which sends it its meters only
    // while it shows; the Plugins Creator Studio lays Sonify's own studio window over its page (and
    // its MIDI Devices tab tells Sonify); the Web Browser lays its page's view over its content
    // area, or takes it away.
    if (Hub.board) Hub.board.show(active === "board");
    if (Hub.creator) Hub.creator.show(active === "plugins");
    if (Hub.aiTools) Hub.aiTools.show(active === "ai");
    if (Hub.blocks) Hub.blocks.show(active === "blocks");
    if (Hub.web) Hub.web.show(active === "web");
    if (Hub.live) Hub.live.show(active === "live");
    if (Hub.sampling) Hub.sampling.show(active === "sampling");
    if (Hub.guide) Hub.guide.show(active === "guide");

    // The arrangement's keys (Delete and the like) rest while another studio is in front.
    if (active !== "studio") Hub.state.focus = active;
    else if (IDS.includes(Hub.state.focus)) Hub.state.focus = "arrange";

    renderSwitch();
    Hub.emit("studio", active);

    // MIDI is heard in the studio in front only: Sonify gives it the live notes (the song's
    // studios share the tracks'; the Plugins Creator Studio and the Sampling Studio have a MIDI In
    // of their own; AI Tools, the Web Browser and the Guide hear none).
    Hub.send("studioShown", { id: active });

    // Back in the Studio: the arrangement and the Editors measure themselves again.
    if (active === "studio" && was !== "studio") Hub.emit("layout");
  }

  function cycle(step) {
    const at = SHOWN.indexOf(active);
    activate(SHOWN[(at + step + SHOWN.length) % SHOWN.length]);
  }

  // The menu item each studio has (index.html's data-command), so a hidden studio's item is
  // taken out with its tab.
  const COMMAND_OF = {
    studio: "showStudio", board: "analogBoard", ai: "sonifyAi", blocks: "songBlocks",
    plugins: "pluginsCreator", live: "liveShow", sampling: "samplingStudio",
    web: "webBrowser", guide: "guide"
  };

  // ---------------------------------------------------------------- commands
  Hub.commands.showStudio = () => activate("studio");
  Hub.commands.analogBoard = () => activate("board");
  Hub.commands.sonifyAi = () => activate("ai");
  Hub.commands.songBlocks = () => activate("blocks");
  Hub.commands.pluginsCreator = () => activate("plugins");
  Hub.commands.liveShow = () => activate("live");
  Hub.commands.samplingStudio = () => activate("sampling");
  Hub.commands.webBrowser = () => activate("web");
  Hub.commands.guide = () => activate("guide");
  Hub.commands.nextStudio = () => cycle(1);
  Hub.commands.previousStudio = () => cycle(-1);

  Hub.studios = { activate, active: () => active, ids: () => IDS.slice(), shown: () => SHOWN.slice() };

  // ---------------------------------------------------------------- block internet
  let internetBlocked = false;

  function renderNetToggle() {
    const button = $("net-toggle");
    button.setAttribute("aria-checked", String(internetBlocked));
    button.classList.toggle("blocked", internetBlocked);
    button.querySelector(".net-label").textContent = internetBlocked ? "Internet off" : "Internet on";
    button.title = internetBlocked ? "Internet blocked: no plugin, web page, Fanan AI or Fanan content can reach the network. Click to allow it."
                                   : "Internet on. Click to block it (plugins, web pages, Fanan AI and Fanan content).";
  }

  Hub.on("internetBlocked", (data) => {
    internetBlocked = Boolean(data.blocked);
    renderNetToggle();
    if (internetBlocked) Hub.toast("Internet blocked");
  });

  Hub.netBlocked = () => internetBlocked;

  // ---------------------------------------------------------------- the selector and its keys
  Hub.on("ready", () => {
    // The studios not offered for now: their tab goes from the top bar and their item from the
    // View menu, so nothing on the page leads anywhere empty.
    for (const studio of STUDIOS) {
      if (! studio.hidden) continue;
      for (const tab of document.querySelectorAll(`.studio-tab[data-studio="${studio.id}"]`)) tab.hidden = true;
      for (const item of document.querySelectorAll(`[data-command="${COMMAND_OF[studio.id] || ""}"]`)) item.hidden = true;
    }

    renderNetToggle();
    $("net-toggle").addEventListener("click", () => Hub.send("setInternetBlocked", { blocked: !internetBlocked }));

    const strip = $("studio-switch");
    strip.addEventListener("click", (event) => {
      const button = event.target.closest(".studio-tab");
      if (button) activate(button.dataset.studio);
    });

    // The arrows move along the selector while it has the focus, as in any row of tabs.
    strip.addEventListener("keydown", (event) => {
      if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
      event.preventDefault();
      event.stopPropagation();
      cycle(event.key === "ArrowLeft" ? -1 : 1);
      const button = strip.querySelector(`[data-studio="${active}"]`);
      if (button) button.focus();
    });

    window.addEventListener("keydown", (event) => {
      const ctrl = event.ctrlKey || event.metaKey;

      // The page never reloads, prints or goes back (F5, Ctrl+R, Ctrl+P, Alt+Left/Right, the
      // browser keys), in any studio or field: the keys still reach the keyboard map (F5 is the
      // Browser's in the Studio), but not the web view.
      if (event.code === "F5" || (ctrl && (event.code === "KeyR" || event.code === "KeyP"))
          || (event.altKey && (event.code === "ArrowLeft" || event.code === "ArrowRight") && !ctrl)
          || event.code === "BrowserBack" || event.code === "BrowserForward" || event.code === "BrowserRefresh")
        event.preventDefault();

      if (!ctrl || event.altKey) return;
      if (!$("scan-page").hidden || document.querySelector(".modal:not([hidden])")) return;

      // In the Studio, a key the keyboard map gives a command is the DAW's.
      if (active === "studio" && Hub.keys && Hub.keys.isBound(event)) return;

      let handled = true;
      if (event.code === "Tab") cycle(event.shiftKey ? -1 : 1);
      else if (!event.shiftKey && /^Digit[1-9]$/.test(event.code) && isShown(IDS[Number(event.code.slice(5)) - 1]))
        activate(IDS[Number(event.code.slice(5)) - 1]);
      else handled = false;

      if (handled) {
        event.preventDefault();
        event.stopImmediatePropagation();
      }
    }, true);

    activate("studio", false);
  });
})();
