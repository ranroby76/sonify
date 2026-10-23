// D:\workspace\Fanan_web_hub\src\hub-panelsize.js
//
// The bottom panels (the Editor, the Audio Editor and the Console) are made taller or shorter by
// dragging their top edge: up as far as the toolbar (the arrangement gives up its room), down to
// a small strip. A double-click on the edge puts it back to its usual height. Each keeps its own
// height on this computer.

(() => {
  "use strict";

  const { $, element } = Hub;
  const PANELS = ["editor", "audio-editor", "console"];
  const MIN = 120;              // pixels: the shortest a panel goes
  const KEY = "sonify.panelHeights";

  let heights = {};
  try { heights = JSON.parse(localStorage.getItem(KEY) || "{}") || {}; } catch (error) { heights = {}; }

  function keep() {
    try { localStorage.setItem(KEY, JSON.stringify(heights)); } catch (error) { /* not kept */ }
  }

  /** The most a panel can be: itself plus all the room the arrangement has now. */
  function most(panel) {
    const workspace = $("workspace");
    return panel.offsetHeight + (workspace ? workspace.offsetHeight : 0);
  }

  function apply(panel, height) {
    if (height > 0) {
      panel.style.height = Math.round(height) + "px";
      // The arrangement gives up its room first; only a window too small for it squeezes the
      // panel (rather than pushing the transport off).
      panel.style.flexShrink = "0.001";
    } else {
      panel.style.height = "";
      panel.style.flexShrink = "";
    }
  }

  function setup(id) {
    const panel = $(id);
    if (!panel) return;
    if (getComputedStyle(panel).position === "static") panel.style.position = "relative";

    const grip = element("div", "panel-grip");
    grip.title = "Drag to make it taller or shorter; double-click for its usual height";
    panel.prepend(grip);
    if (heights[id] > 0) apply(panel, heights[id]);

    grip.addEventListener("pointerdown", (event) => {
      if (event.button !== 0) return;
      event.preventDefault();
      grip.setPointerCapture(event.pointerId);
      const from = event.clientY, start = panel.offsetHeight, limit = most(panel);
      grip.classList.add("dragging");
      document.body.classList.add("panel-resizing");

      const move = (e) => {
        const height = Hub.clamp(start + (from - e.clientY), MIN, Math.max(MIN, limit));
        apply(panel, height);
      };
      const up = () => {
        grip.removeEventListener("pointermove", move);
        grip.removeEventListener("pointerup", up);
        grip.removeEventListener("pointercancel", up);
        grip.classList.remove("dragging");
        document.body.classList.remove("panel-resizing");
        heights[id] = panel.offsetHeight;
        keep();
      };
      grip.addEventListener("pointermove", move);
      grip.addEventListener("pointerup", up);
      grip.addEventListener("pointercancel", up);
    });

    grip.addEventListener("dblclick", () => {
      delete heights[id];
      keep();
      apply(panel, 0);
    });
  }

  Hub.on("ready", () => PANELS.forEach(setup));
})();
