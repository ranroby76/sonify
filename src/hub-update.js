// D:\workspace\Fanan_web_hub\src\hub-update.js
//
// The update reminder: when Fanan's site has a newer Sonify (UpdateCheck.cpp asks it a few
// seconds after Sonify starts, then once a day), a small window floats over the lower right of
// any studio: the new version, what's new, and
//   Get it              Sonify's page on fananteam.site opens in the browser
//   Later               it goes, and comes back at the next start
//   Skip this version   quiet until a later one (not offered for an important update)
// Dragged by its top. An important update has an orange edge.
(() => {
  const STYLE = `
    #update-popup { position: fixed; right: 18px; bottom: 64px; width: 340px; max-width: calc(100vw - 36px); z-index: 9000;
      background: var(--panel-2); color: var(--text); border: 1px solid var(--line-2); border-left: 4px solid var(--accent);
      border-radius: 8px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); font: 13px var(--ui); }
    #update-popup.important { border-left-color: var(--mute); }
    #update-popup .up-head { display: flex; align-items: center; gap: 8px; padding: 10px 12px 6px; cursor: move; user-select: none; }
    #update-popup .up-title { flex: 1; font-weight: 600; color: var(--bright); font-size: 14px; }
    #update-popup .up-close { background: none; border: none; color: var(--muted); font-size: 16px; cursor: pointer; padding: 0 2px; }
    #update-popup .up-close:hover { color: var(--bright); }
    #update-popup .up-sub { padding: 0 12px; color: var(--muted); font-size: 12px; }
    #update-popup .up-tag { display: inline-block; margin: 6px 12px 0; padding: 1px 6px; border-radius: 3px; background: var(--mute);
      color: #1b1c1f; font-size: 11px; font-weight: 600; }
    #update-popup ul { margin: 8px 12px 0 28px; padding: 0; max-height: 150px; overflow-y: auto; }
    #update-popup li { margin: 2px 0; line-height: 1.35; }
    #update-popup .up-buttons { display: flex; gap: 8px; padding: 12px; justify-content: flex-end; flex-wrap: wrap; }
    #update-popup button.up-btn { border: 1px solid var(--line-2); background: var(--panel); color: var(--text); border-radius: 4px;
      padding: 5px 12px; cursor: pointer; font: 12px var(--ui); }
    #update-popup button.up-btn:hover { border-color: var(--muted); }
    #update-popup button.up-get { background: var(--accent); border-color: var(--accent); color: var(--on-accent); font-weight: 600; }
    #update-popup button.up-get:hover { background: var(--accent-light); }
    #update-popup .up-skip { margin-right: auto; }
  `;

  let popup = null;

  function close() {
    if (popup) popup.remove();
    popup = null;
  }

  function button(text, cls, onClick) {
    const b = document.createElement("button");
    b.className = "up-btn " + cls;
    b.textContent = text;
    b.addEventListener("click", onClick);
    return b;
  }

  // Moved by its top, kept inside the window.
  function draggable(box, handle) {
    handle.addEventListener("pointerdown", (e) => {
      if (e.button !== 0 || e.target.closest("button")) return;
      const r = box.getBoundingClientRect();
      const dx = e.clientX - r.left, dy = e.clientY - r.top;
      handle.setPointerCapture(e.pointerId);
      const move = (m) => {
        const x = Math.max(0, Math.min(window.innerWidth - r.width, m.clientX - dx));
        const y = Math.max(0, Math.min(window.innerHeight - r.height, m.clientY - dy));
        box.style.left = x + "px"; box.style.top = y + "px";
        box.style.right = "auto"; box.style.bottom = "auto";
      };
      const end = () => { handle.removeEventListener("pointermove", move); handle.removeEventListener("pointerup", end); };
      handle.addEventListener("pointermove", move);
      handle.addEventListener("pointerup", end);
    });
  }

  function show(data) {
    if (!data || !data.version) return;
    close();

    if (!document.getElementById("update-popup-style")) {
      const style = document.createElement("style");
      style.id = "update-popup-style";
      style.textContent = STYLE;
      document.head.appendChild(style);
    }

    popup = document.createElement("div");
    popup.id = "update-popup";
    popup.setAttribute("role", "dialog");
    if (data.important) popup.classList.add("important");

    const head = document.createElement("div");
    head.className = "up-head";
    const title = document.createElement("div");
    title.className = "up-title";
    title.textContent = "Sonify " + data.version + " is out";
    const x = document.createElement("button");
    x.className = "up-close";
    x.title = "Later";
    x.textContent = "\u2715";
    x.addEventListener("click", () => { Hub.send("updateLater"); close(); });
    head.append(title, x);
    popup.appendChild(head);

    const sub = document.createElement("div");
    sub.className = "up-sub";
    sub.textContent = "You have " + (data.current || "an older version") + (data.released ? " \u00b7 released " + data.released : "");
    popup.appendChild(sub);

    if (data.important) {
      const tag = document.createElement("div");
      tag.className = "up-tag";
      tag.textContent = "Important update";
      popup.appendChild(tag);
    }

    const notes = Array.isArray(data.notes) ? data.notes : [];
    if (notes.length) {
      const list = document.createElement("ul");
      for (const note of notes) {
        const li = document.createElement("li");
        li.textContent = note;
        list.appendChild(li);
      }
      popup.appendChild(list);
    }

    const buttons = document.createElement("div");
    buttons.className = "up-buttons";
    if (!data.important)
      buttons.appendChild(button("Skip this version", "up-skip", () => { Hub.send("updateSkip"); close(); }));
    buttons.appendChild(button("Later", "up-later", () => { Hub.send("updateLater"); close(); }));
    buttons.appendChild(button("Get it", "up-get", () => { Hub.send("updateOpen"); close(); }));
    popup.appendChild(buttons);

    document.body.appendChild(popup);
    draggable(popup, head);
  }

  // Sonify's word (an event the page's own list doesn't name, so it's listened to here).
  if (window.__JUCE__ && window.__JUCE__.backend)
    window.__JUCE__.backend.addEventListener("updateAvailable", (data) => Hub.emit("updateAvailable", data));

  Hub.on("updateAvailable", show);

  Hub.update = { show, close };
})();
