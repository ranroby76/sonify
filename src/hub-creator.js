// D:\workspace\Fanan_web_hub\src\hub-creator.js
//
// The Plugins Creator Studio (the top bar's selector, Ctrl+5): Stella AI Studio inside Sonify,
// with Sonify AI building instruments, effects, widgets and MIDI FX (Sonify's MIDI devices) for
// Sonify's tracks (Add to Sonify puts a plugin in the Browser, under Made in Sonify; a MIDI FX plays
// on its track as it's built, and is kept in My Devices). The studio is Sonify's own window, laid
// over this page's area while the studio is in front (creatorShow / creatorHide); it steps aside
// while a dialog, a menu, the Start page or the plugin scan is open, as the Web Browser's views do.
// Its tabs: Build with AI, Edit UI and Knob Studio.
// Sonify sends: creator {tab} when its tab changes; runCommand {command} for Sonify's keys pressed in
// the studio's window (Ctrl+1..8, Ctrl+Tab).

(() => {
  "use strict";

  const { $ } = Hub;
  let shown = false;          // the studio is in front

  // The page's own windows over the studio: the studio's window steps aside for them.
  function overlayOpen() {
    const boxes = [...document.querySelectorAll(".modal"), $("start-page"), $("scan-page")];
    return boxes.some((box) => box && !box.hidden) || Boolean(document.querySelector(".popup-menu"));
  }

  /** Where the studio's window goes (in the page's pixels, which are the window's own), or none. */
  function place() {
    if (!shown || overlayOpen()) {
      // Under a dialog or a menu the studio is still in front: it keeps the live notes.
      Hub.send("creatorHide", { overlay: shown });
      return;
    }

    const r = $("creator-studio").getBoundingClientRect();

    if (r.width < 2 || r.height < 2) {
      Hub.send("creatorHide", { overlay: true });   // no room just now: still in front
      return;
    }

    Hub.send("creatorShow", { x: Math.round(r.left), y: Math.round(r.top), width: Math.round(r.width), height: Math.round(r.height) });
  }

  /** The studio in front (on) or not. */
  function show(on) {
    if (on === shown) return;
    shown = on;
    place();
  }

  /** One of its tabs (Build with AI 0, Edit UI 1, Knob Studio 2). */
  function showTab(index) {
    Hub.send("creatorTab", { index });
  }

  Hub.creator = { show, showTab, isShown: () => shown };

  Hub.on("creator", () => {
    if (shown) place();
  });

  // Sonify's keys pressed inside the studio's window.
  const COMMANDS = new Set(["showStudio", "analogBoard", "sonifyAi", "songBlocks", "pluginsCreator", "liveShow", "samplingStudio", "webBrowser", "guide",
                            "nextStudio", "previousStudio"]);

  Hub.on("runCommand", (data) => {
    if (data && COMMANDS.has(data.command)) Hub.run(data.command);
  });

  Hub.on("ready", () => {
    // The window follows the page's area as the window changes.
    if (window.ResizeObserver) new ResizeObserver(() => { if (shown) place(); }).observe($("creator-studio"));
    window.addEventListener("resize", () => { if (shown) place(); });

    // A dialog or a menu opened over the studio needs the page in front: the window steps aside meanwhile.
    const watch = new MutationObserver(() => { if (shown) place(); });
    for (const box of [...document.querySelectorAll(".modal"), $("start-page"), $("scan-page")])
      if (box) watch.observe(box, { attributes: true, attributeFilter: ["hidden"] });
    watch.observe(document.body, { childList: true });

    Hub.send("creatorState");
  });
})();
