// D:\workspace\Fanan_web_hub\src\hub-screens.js
//
// Plugin screens: floating windows inside the Hub's page, like Studio One's plugin windows.
//   web      a WCLAP's own page (CLAP's webview extension), in a sandboxed frame. The plugin
//            serves its files; its messages pass both ways as ArrayBuffers (postMessage).
//   generic  plugins without a screen of their own: the Hub's controls for their parameters.
//   native   VST3 editors open in a window of their own; nothing to show here.
//   panel    the page's own windows (a MIDI FX device's settings, hub-miditools.js): Hub.screens.panel.
// Drag a window by its title, resize it from its corner; a click brings it to the front.
// A plugin's window has its ON/OFF and its format in the title bar, as its own windows have on
// their header (PluginHeader.cpp): ON/OFF is its bypass where it sits (pluginOn), shown as the
// tracks say. A window closes by itself when its plugin is removed.

(() => {
  "use strict";

  const { $, element } = Hub;
  const windows = new Map();   // "channel:slot" -> { root, frame, channel, slot, kind, params }
  const TITLE = 28;
  let topZ = 50, opened = 0;

  const keyOf = (channel, slot) => `${channel}:${slot}`;

  /** The plugin in a slot: the instrument (slot 0) or an insert, from the tracks the page has; or a
      plugin inside a container on the channel (a node's slot id), however deep. */
  function pluginAt(channel, slot) {
    const track = channel === Hub.MAIN ? null : Hub.trackById(channel);
    if (channel !== Hub.MAIN && !track) return null;
    if (slot === 0) return (track && track.instrument) || null;
    const inserts = channel === Hub.MAIN ? (Hub.state.master.inserts || []) : (track.inserts || []);
    const direct = inserts.find((i) => i.insertId === slot);
    if (direct) return direct;
    const inside = (plugin) => {
      for (const node of (plugin && plugin.nodes) || []) {
        if (node.insertId === slot) return node;
        const deeper = inside(node);
        if (deeper) return deeper;
      }
      return null;
    };
    for (const plugin of [track && track.instrument, ...inserts]) {
      const found = inside(plugin);
      if (found) return found;
    }
    return null;
  }

  // ---------------------------------------------------------------- bytes <-> base64
  function toBase64(bytes) {
    let text = "";
    for (let i = 0; i < bytes.length; i += 0x8000) text += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
    return btoa(text);
  }

  function fromBase64(base64) {
    const text = atob(base64);
    const bytes = new Uint8Array(text.length);
    for (let i = 0; i < text.length; i++) bytes[i] = text.charCodeAt(i);
    return bytes;
  }

  // ---------------------------------------------------------------- windows
  function bringToFront(win) {
    win.root.style.zIndex = String(++topZ);
  }

  /** What a format is called on a window ("Sonify" for Sonify's own plugins). */
  const formatLabel = (format) => String(format || "").toUpperCase();

  /** ON/OFF and the format, as the tracks say. */
  function showPower(win) {
    if (!win.power) return;
    const plugin = pluginAt(win.channel, win.slot);
    const known = Boolean(plugin) && typeof plugin.bypass === "boolean";
    const on = !(plugin && plugin.bypass);
    win.power.hidden = !known;
    win.power.classList.toggle("off", !on);
    win.power.textContent = on ? "ON" : "OFF";
    win.power.title = on ? "On: click to switch it off (bypassed)" : "Off (bypassed): click to switch it on";
    win.power.setAttribute("aria-pressed", String(on));
    win.root.classList.toggle("plugin-off", known && !on);
    win.format.textContent = plugin ? formatLabel(plugin.format) : "";
  }

  function close(key, tellEngine = true) {
    const win = windows.get(key);
    if (!win) return;
    windows.delete(key);
    win.root.remove();
    if (tellEngine && win.kind === "web") Hub.send("closePluginScreen", { channel: win.channel, slot: win.slot });
  }

  function makeWindow(key, channel, slot, kind, title, width, height) {
    const root = element("div", "screen-window");
    root.dataset.key = key;
    root.setAttribute("role", "dialog");
    root.setAttribute("aria-label", title);

    const bar = element("div", "screen-title");
    const name = element("span", "screen-name", title);
    const shut = element("button", "screen-close", "×");
    shut.type = "button";
    shut.title = "Close";
    shut.setAttribute("aria-label", `Close ${title}`);
    shut.addEventListener("click", () => close(key));
    // A plugin's window: its ON/OFF before the name, its format after.
    let power = null, format = null;
    if (kind !== "panel") {
      power = element("button", "screen-power", "ON");
      power.type = "button";
      power.hidden = true;
      power.addEventListener("click", () => {
        const plugin = pluginAt(channel, slot);
        if (plugin) Hub.send("pluginOn", { channel, slot, on: Boolean(plugin.bypass) });
      });
      format = element("span", "screen-format");
      bar.append(power, name, format, shut);
    } else {
      bar.append(name, shut);
    }

    const body = element("div", "screen-body");
    const grip = element("div", "screen-resize");
    root.append(bar, body, grip);

    // Over the arrangement, clear of the track headers, cascaded.
    const w = Math.min(Math.max(width, 260), window.innerWidth - 40);
    const h = Math.min(Math.max(height, 120), window.innerHeight - 80) + TITLE;
    const n = opened++ % 8;
    const headers = $("tracks-area");
    const clear = headers ? headers.getBoundingClientRect().right + 24 : 260;
    root.style.left = `${Math.max(10, Math.min(clear + 60 + n * 28, window.innerWidth - w - 10))}px`;
    root.style.top = `${Math.max(10, Math.min(100 + n * 28, window.innerHeight - h - 10))}px`;
    root.style.width = `${w}px`;
    root.style.height = `${h}px`;

    const win = { root, body, channel, slot, kind, frame: null, params: new Map(), power, format };
    root.addEventListener("pointerdown", () => bringToFront(win), true);

    // Move by the title bar.
    bar.addEventListener("pointerdown", (event) => {
      if (event.target === shut || event.target === power) return;
      event.preventDefault();
      const startX = event.clientX, startY = event.clientY;
      const left = root.offsetLeft, top = root.offsetTop;
      root.classList.add("dragging");
      const move = (e) => {
        root.style.left = `${Math.min(Math.max(left + e.clientX - startX, -root.offsetWidth + 80), window.innerWidth - 80)}px`;
        root.style.top = `${Math.min(Math.max(top + e.clientY - startY, 0), window.innerHeight - TITLE)}px`;
      };
      const up = () => {
        root.classList.remove("dragging");
        window.removeEventListener("pointermove", move);
        window.removeEventListener("pointerup", up);
      };
      window.addEventListener("pointermove", move);
      window.addEventListener("pointerup", up);
    });

    // Resize by the corner.
    grip.addEventListener("pointerdown", (event) => {
      event.preventDefault();
      const startX = event.clientX, startY = event.clientY;
      const startW = root.offsetWidth, startH = root.offsetHeight;
      root.classList.add("dragging");
      const move = (e) => {
        root.style.width = `${Math.max(220, startW + e.clientX - startX)}px`;
        root.style.height = `${Math.max(TITLE + 80, startH + e.clientY - startY)}px`;
      };
      const up = () => {
        root.classList.remove("dragging");
        window.removeEventListener("pointermove", move);
        window.removeEventListener("pointerup", up);
      };
      window.addEventListener("pointermove", move);
      window.addEventListener("pointerup", up);
    });

    $("screens").appendChild(root);
    windows.set(key, win);
    bringToFront(win);
    showPower(win);
    return win;
  }

  /** The Hub's own controls, for plugins without a screen. */
  function fillGeneric(win, plugin) {
    const list = element("div", "params screen-params");
    const params = (plugin && plugin.params) || [];
    if (!params.length) list.appendChild(element("p", "slot-hint", "This plugin has no parameters."));

    for (const p of params) {
      const row = element("div", "param");
      const name = element("span", "param-name", p.name);
      name.title = p.name;
      const input = element("input");
      input.type = "range";
      input.min = String(p.min);
      input.max = String(p.max);
      input.step = p.stepped ? "1" : String((p.max - p.min) / 1000 || 0.001);
      input.value = String(p.value);
      input.disabled = Boolean(p.readOnly);
      input.setAttribute("aria-label", p.name);
      input.addEventListener("input", () => {
        Hub.send("setParam", { trackId: win.channel, insertId: win.slot, index: p.index, value: Number(input.value) });
      });
      const value = element("span", "param-value", p.text);
      row.append(name, input, value);
      list.appendChild(row);
      win.params.set(p.index, { input, value });
    }

    win.body.replaceChildren(list);
  }

  // ---------------------------------------------------------------- opening
  /** Opens (or brings forward) the screen of the plugin in a slot. */
  Hub.openScreen = (channel, slot) => {
    const existing = windows.get(keyOf(channel, slot));
    if (existing) {
      bringToFront(existing);
      return;
    }
    Hub.send("openPluginScreen", { channel, slot });
  };

  Hub.on("pluginScreen", (reply) => {
    const key = keyOf(reply.channel, reply.slot);
    if (reply.kind === "native" || windows.has(key)) return;

    if (reply.kind === "web") {
      const win = makeWindow(key, reply.channel, reply.slot, "web", reply.title, reply.width || 640, reply.height || 420);
      const frame = element("iframe", "screen-frame");
      // Its own origin: a plugin's page can talk to its plugin, but can't reach into the Hub.
      frame.setAttribute("sandbox", "allow-scripts allow-forms allow-popups");
      frame.title = reply.title;
      frame.src = reply.url;
      win.frame = frame;
      win.body.replaceChildren(frame);
      return;
    }

    const win = makeWindow(key, reply.channel, reply.slot, "generic", reply.title, 420, 300);
    fillGeneric(win, pluginAt(reply.channel, reply.slot));
  });

  // ---------------------------------------------------------------- messages
  // From a plugin's page: window.parent.postMessage(ArrayBuffer or typed array).
  window.addEventListener("message", (event) => {
    for (const win of windows.values()) {
      if (!win.frame || event.source !== win.frame.contentWindow) continue;
      const data = event.data;
      let bytes = null;
      if (data instanceof ArrayBuffer) bytes = new Uint8Array(data);
      else if (ArrayBuffer.isView(data)) bytes = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
      else if (typeof data === "string") bytes = new TextEncoder().encode(data);
      if (bytes) Hub.send("screenMessage", { channel: win.channel, slot: win.slot, data: toBase64(bytes) });
      return;
    }
  });

  // To a plugin's page: each message as an ArrayBuffer.
  Hub.on("screenMessages", (batch) => {
    const win = windows.get(keyOf(batch.channel, batch.slot));
    if (!win || !win.frame || !win.frame.contentWindow) return;
    for (const message of batch.messages || []) win.frame.contentWindow.postMessage(fromBase64(message).buffer, "*");
  });

  // Generic controls follow the plugin's parameters.
  Hub.on("param", (reply) => {
    const win = windows.get(keyOf(reply.trackId, reply.insertId || 0));
    const row = win && win.params.get(reply.index);
    if (row) row.value.textContent = reply.text;
  });

  // A plugin that has gone takes its window with it (the page's own windows look after themselves);
  // so does a frozen track's (its plugins rest: what's changed there wouldn't be heard), and one
  // in a freeze under way (what's changed there wouldn't be in its render).
  Hub.on("tracks", () => {
    for (const [key, win] of [...windows]) {
      if (win.kind === "panel") continue;
      const plugin = pluginAt(win.channel, win.slot);
      if (!plugin) {
        close(key, false);
        continue;
      }
      const channel = Hub.trackById(win.channel);
      if (channel && (channel.frozen || channel.rendering)) {
        close(key);
        continue;
      }
      showPower(win);
      if (win.kind === "generic") {
        for (const p of plugin.params || []) {
          const row = win.params.get(p.index);
          if (row && document.activeElement !== row.input) {
            row.input.value = String(p.value);
            row.value.textContent = p.text;
          }
        }
      }
    }
  });

  /** A floating window of the page's own (key names it): made, or brought forward if it's open. */
  function panel(key, title, width, height) {
    const existing = windows.get(key);
    if (existing) {
      bringToFront(existing);
      return existing;
    }
    return makeWindow(key, null, null, "panel", title, width, height);
  }

  Hub.screens = { windows, close, pluginAt, panel };
})();
