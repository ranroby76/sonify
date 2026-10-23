// C:\workspace\Fanan_web_hub\src\hub-inspector.js
//
// The Inspector (F4): the selected track's instrument and inserts, or the Main
// channel's inserts. Each insert can be switched off (bypass), opened to show its
// parameters, removed, or dragged to a new place in the chain. Effects dragged from
// the Browser drop into the list where you let go; + adds one from a menu.

(() => {
  "use strict";

  const SCREEN_ICON = '<svg viewBox="0 0 16 16"><rect x="1.5" y="2.5" width="13" height="11" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.3"/><path d="M1.5 5.5h13" stroke="currentColor" stroke-width="1.3"/></svg>';

  const { $, element } = Hub;
  const POWER = '<svg viewBox="0 0 16 16"><path d="M7.25 1.5h1.5v6h-1.5z"/><path d="M4.2 3.6l1.06 1.06A4.25 4.25 0 1 0 10.74 4.66L11.8 3.6A5.75 5.75 0 1 1 4.2 3.6z"/></svg>';

  const expanded = new Set();     // insert ids showing their parameters
  const views = new Map();        // "instrument" or an insert id -> { signature, params }
  let listKey = "";               // the insert list as last built; rebuilt only when it changes
  let dragInsert = null;          // { channel, insertId, from } while an insert is being dragged

  const shownChannel = () => Hub.state.inspectMain ? Hub.MAIN : (Hub.selectedTrack() ? Hub.state.selected : null);

  function setFill(input) {
    const min = Number(input.min), max = Number(input.max);
    input.style.setProperty("--fill", Hub.clamp01(max > min ? (Number(input.value) - min) / (max - min) : 0) * 100 + "%");
  }

  function roleText(plugin) {
    return { instrument: "Instrument", effect: "Effect", noteEffect: "Note effect" }[plugin.role] || plugin.role;
  }

  // ---------------------------------------------------------------- parameters
  /** Sliders for a plugin's parameters. Returns index -> {input, value} for later updates. */
  function paramsView(container, channel, insertId, plugin) {
    const list = element("div", "params");
    const rows = new Map();

    if (plugin.params.length === 0) list.appendChild(element("p", "slot-hint", "This plugin has no parameters."));

    for (const p of plugin.params) {
      const row = element("div", "param");
      const name = element("span", "param-name", p.name);
      name.title = p.name;
      const input = element("input");
      input.type = "range";
      input.min = String(p.min);
      input.max = String(p.max);
      input.step = p.stepped ? "1" : String((p.max - p.min) / 1000 || 0.001);
      input.value = String(p.value);
      input.disabled = p.readOnly;
      input.setAttribute("aria-label", p.name);
      input.addEventListener("input", () => {
        setFill(input);
        Hub.send("setParam", { trackId: channel, insertId, index: p.index, value: Number(input.value) });
      });
      const value = element("span", "param-value", p.text);
      value.title = p.text;
      row.append(name, input, value);
      list.appendChild(row);
      rows.set(p.index, { input, value });
      setFill(input);
    }

    container.appendChild(list);
    return rows;
  }

  /** The same plugin as shown: refresh values in place, so a slider being dragged isn't rebuilt. */
  function refreshParams(rows, plugin) {
    for (const p of plugin.params) {
      const row = rows.get(p.index);
      if (!row) continue;
      if (document.activeElement !== row.input) {
        row.input.value = String(p.value);
        setFill(row.input);
      }
      row.value.textContent = p.text;
    }
  }

  const signature = (channel, plugin) => `${channel}|${plugin.file}|${plugin.id}|${plugin.params.map((p) => p.index).join(".")}`;

  // ---------------------------------------------------------------- instrument
  /** Audio tracks: which input the track hears and records (made on first use). */
  function renderInput(track) {
    let section = $("insp-input");
    if (!section) {
      section = element("section", "insp-section");
      section.id = "insp-input";
      section.innerHTML = '<h3 class="insp-heading">Input</h3><select id="insp-input-select" aria-label="Input"></select>'
                        + '<p class="muted small">Song Setup (Song menu) names the inputs and their channels.</p>';
      $("insp-instrument").after(section);
      $("insp-input-select").addEventListener("change", (event) => {
        const selected = Hub.selectedTrack();
        if (selected) Hub.send("trackSettings", { id: selected.id, input: Number(event.target.value) });
      });
    }
    section.hidden = !track || track.kind !== "audio";
    if (section.hidden) return;
    const select = $("insp-input-select");
    select.textContent = "";
    (Hub.state.inputs || []).forEach((input, index) => select.add(new Option(input.name, String(index))));
    select.value = String(track.input || 0);
  }

  function renderInstrument(track) {
    const section = $("insp-instrument");
    renderInput(track);
    section.hidden = !track || track.kind === "audio";
    if (section.hidden) return;

    const body = section.querySelector(".slot-body");
    const plugin = track.instrument;
    const sig = plugin ? signature(track.id, plugin) : `${track.id}|none`;
    const view = views.get("instrument");

    if (view && view.signature === sig) {
      if (plugin) refreshParams(view.params, plugin);
      return;
    }

    body.textContent = "";

    if (!plugin) {
      const head = element("div", "slot-head");
      head.appendChild(element("div", "slot-name", "Test Synth"));
      const open = element("button", "plain-button small", "Open");
      open.type = "button";
      open.title = "Its window: two oscillators, a filter, envelopes, an LFO and more";
      open.addEventListener("click", () => { if (Hub.openTestSynth) Hub.openTestSynth(track.id); });
      head.appendChild(open);
      body.appendChild(head);
      body.appendChild(element("p", "slot-hint", "Built-in. Open it to shape its sound, or drag an instrument from the Browser onto the track to replace it."));
      views.set("instrument", { signature: sig, params: new Map() });
      return;
    }

    const head = element("div", "slot-head");
    const instrumentName = element("div", "slot-name", plugin.name);
    instrumentName.title = "Double-click to open its screen";
    instrumentName.addEventListener("dblclick", () => Hub.openScreen(track.id, 0));
    head.appendChild(instrumentName);
    head.appendChild(screenButton(track.id, 0, plugin));
    const remove = element("button", "slot-remove", "×");
    remove.title = "Remove, back to the Test Synth";
    remove.setAttribute("aria-label", remove.title);
    remove.addEventListener("click", () => Hub.send("clearInstrument", { trackId: track.id }));
    head.appendChild(remove);
    body.appendChild(head);

    const details = [plugin.vendor, roleText(plugin)];
    if (plugin.outputs === 1) details.push("mono");
    body.appendChild(element("div", "slot-meta", details.filter(Boolean).join(" · ")));

    if (plugin.plugins.length > 1) {
      const choice = element("select", "slot-choice");
      choice.setAttribute("aria-label", "Plugin in this file");
      for (const entry of plugin.plugins) choice.add(new Option(entry.name, entry.id, false, entry.id === plugin.id));
      choice.addEventListener("change", () => {
        choice.blur();
        Hub.send("loadPlugin", { trackId: track.id, file: plugin.file, pluginId: choice.value });
      });
      body.appendChild(choice);
    }

    views.set("instrument", { signature: sig, params: paramsView(body, track.id, 0, plugin) });
  }

  /** Opens a plugin's screen: its own (web or native), or the Hub's controls. */
  function screenButton(channel, slot, plugin) {
    const button = element("button", "slot-screen");
    button.innerHTML = SCREEN_ICON;
    const own = plugin && (plugin.screen === "web" || plugin.screen === "native");
    button.title = own ? "Open its screen" : "Open its controls in a window";
    button.setAttribute("aria-label", `${button.title}: ${plugin ? plugin.name : ""}`);
    button.addEventListener("click", () => Hub.openScreen(channel, slot));
    return button;
  }

  // ---------------------------------------------------------------- inserts
  function insertRow(channel, insert, index) {
    const row = element("div", "insert");
    row.dataset.insertId = String(insert.insertId);
    row.draggable = true;
    row.classList.toggle("bypassed", Boolean(insert.bypass));

    const head = element("div", "insert-head");

    const power = element("button", "insert-power");
    power.innerHTML = POWER;
    power.setAttribute("aria-pressed", String(!insert.bypass));
    power.title = insert.bypass ? "Off (bypassed). Click to switch it on." : "On. Click to bypass.";
    power.setAttribute("aria-label", power.title);
    power.addEventListener("click", () => {
      const bypass = power.getAttribute("aria-pressed") === "true";
      power.setAttribute("aria-pressed", String(!bypass));
      row.classList.toggle("bypassed", bypass);
      Hub.send("bypassInsert", { trackId: channel, insertId: insert.insertId, bypass });
    });

    const isOpen = expanded.has(insert.insertId);
    const expand = element("button", "insert-expand");
    expand.setAttribute("aria-expanded", String(isOpen));
    expand.setAttribute("aria-label", "Show parameters");
    const toggle = () => {
      if (expanded.has(insert.insertId)) expanded.delete(insert.insertId); else expanded.add(insert.insertId);
      listKey = "";
      render();
    };
    expand.addEventListener("click", toggle);

    const name = element("span", "insert-name", `${index + 1}  ${insert.name}`);
    name.title = `${insert.name} · ${insert.vendor}. Double-click to open its screen; drag to reorder.`;
    name.addEventListener("dblclick", () => Hub.openScreen(channel, insert.insertId));

    const remove = element("button", "slot-remove", "×");
    remove.title = "Remove this insert";
    remove.setAttribute("aria-label", remove.title);
    remove.addEventListener("click", () => {
      expanded.delete(insert.insertId);
      Hub.send("removeInsert", { trackId: channel, insertId: insert.insertId });
    });

    head.append(power, expand, name, screenButton(channel, insert.insertId, insert), remove);
    row.appendChild(head);

    const body = element("div", "insert-body");
    body.hidden = !isOpen;
    const details = [insert.vendor, roleText(insert)];
    if (insert.outputs === 1) details.push("mono");
    body.appendChild(element("div", "slot-meta", details.filter(Boolean).join(" · ")));

    if (insert.plugins.length > 1) {
      const choice = element("select", "slot-choice");
      choice.setAttribute("aria-label", "Plugin in this file");
      for (const entry of insert.plugins) choice.add(new Option(entry.name, entry.id, false, entry.id === insert.id));
      choice.addEventListener("change", () => {
        choice.blur();
        Hub.send("loadPlugin", { trackId: channel, file: insert.file, pluginId: choice.value, replaceInsertId: insert.insertId });
      });
      body.appendChild(choice);
    }

    const params = isOpen ? paramsView(body, channel, insert.insertId, insert) : new Map();
    row.appendChild(body);
    views.set(insert.insertId, { signature: signature(channel, insert), params });

    row.addEventListener("dragstart", (event) => {
      dragInsert = { channel, insertId: insert.insertId, from: index };
      event.dataTransfer.effectAllowed = "move";
      event.dataTransfer.setData("text/plain", insert.name);
      row.classList.add("dragging");
    });

    row.addEventListener("dragend", () => {
      dragInsert = null;
      row.classList.remove("dragging");
      showDropLine(-1);
    });

    return row;
  }

  function renderInserts(channel, inserts) {
    $("insp-insert-count").textContent = `${inserts.length}/${Hub.MAX_INSERTS}`;
    $("insp-insert-empty").hidden = inserts.length > 0;

    const key = channel + "|" + inserts.map((i) =>
      `${i.insertId}:${i.bypass ? 1 : 0}:${signature(channel, i)}:${expanded.has(i.insertId) ? 1 : 0}`).join(",");

    if (key === listKey) {
      for (const insert of inserts) {
        const view = views.get(insert.insertId);
        if (view) refreshParams(view.params, insert);
      }
      return;
    }

    listKey = key;

    for (const id of [...views.keys()])
      if (id !== "instrument") views.delete(id);

    const list = $("insp-insert-list");
    list.textContent = "";
    inserts.forEach((insert, index) => list.appendChild(insertRow(channel, insert, index)));
  }

  // ---------------------------------------------------------------- dropping into the list
  /** Which gap the pointer is over: 0 is above the first insert. */
  function dropIndex(clientY) {
    const rows = [...$("insp-insert-list").querySelectorAll(".insert")];

    for (let i = 0; i < rows.length; i++) {
      const r = rows[i].getBoundingClientRect();
      if (clientY < r.top + r.height / 2) return i;
    }

    return rows.length;
  }

  function showDropLine(index) {
    const list = $("insp-insert-list");
    list.querySelectorAll(".drop-line").forEach((line) => line.remove());
    if (index < 0) return;
    const rows = list.querySelectorAll(".insert");
    list.insertBefore(element("div", "drop-line"), rows[index] || null);
  }

  function setupDrops() {
    const zone = $("insp-inserts");

    const accepts = () => {
      const channel = shownChannel();
      if (channel === null) return false;
      if (dragInsert) return dragInsert.channel === channel;
      return Boolean(Hub.dragEntry) && Hub.dragEntry.kind === "effect";
    };

    zone.addEventListener("dragover", (event) => {
      if (!accepts()) return;
      event.preventDefault();
      if (event.dataTransfer) event.dataTransfer.dropEffect = dragInsert ? "move" : "copy";
      showDropLine(dropIndex(event.clientY));
    });

    zone.addEventListener("dragleave", (event) => {
      if (!zone.contains(event.relatedTarget)) showDropLine(-1);
    });

    zone.addEventListener("drop", (event) => {
      if (!accepts()) return;
      event.preventDefault();
      const channel = shownChannel();
      const index = dropIndex(event.clientY);
      showDropLine(-1);

      if (dragInsert) {
        // Dropping below itself counts the gap it leaves behind.
        const position = index > dragInsert.from ? index - 1 : index;
        if (position !== dragInsert.from) Hub.send("moveInsert", { trackId: channel, insertId: dragInsert.insertId, position });
        dragInsert = null;
        return;
      }

      Hub.dropEntryOn(Hub.dragEntry, channel, index);
      Hub.dragEntry = null;
    });

    // An instrument dropped on the instrument section replaces it.
    const instrument = $("insp-instrument");

    instrument.addEventListener("dragover", (event) => {
      if (!Hub.dragEntry || Hub.dragEntry.kind === "effect" || Hub.state.inspectMain) return;
      event.preventDefault();
      instrument.classList.add("drop-target");
    });

    instrument.addEventListener("dragleave", () => instrument.classList.remove("drop-target"));

    instrument.addEventListener("drop", (event) => {
      if (!Hub.dragEntry || Hub.dragEntry.kind === "effect" || Hub.state.inspectMain) return;
      event.preventDefault();
      instrument.classList.remove("drop-target");
      Hub.dropEntryOn(Hub.dragEntry, Hub.state.selected);
      Hub.dragEntry = null;
    });
  }

  // ---------------------------------------------------------------- the whole Inspector
  function render() {
    const main = Hub.state.inspectMain;
    const track = main ? null : Hub.selectedTrack();
    const channel = main ? Hub.MAIN : (track ? track.id : null);
    const inspector = $("inspector");

    inspector.classList.toggle("empty", channel === null);
    inspector.classList.toggle("main", main);
    if (channel === null) return;

    const name = $("insp-name");
    name.disabled = main;
    if (document.activeElement !== name) name.value = main ? "Main" : track.name;
    $("insp-swatch").style.background = main ? "#5b5e65" : track.colour;

    renderInstrument(track);

    const state = main ? Hub.state.master : track;
    renderInserts(channel, state.inserts || []);

    const status = $("insp-status");
    status.textContent = state.loading ? `Loading ${state.loading}…` : (state.message || "");
    status.classList.toggle("error", Boolean(state.message) && !state.loading);
  }

  Hub.on("tracks", render);
  Hub.on("selection", render);
  Hub.on("library", render);

  Hub.on("param", (reply) => {
    if (reply.trackId !== shownChannel()) return;
    const view = views.get(reply.insertId ? reply.insertId : "instrument");
    const row = view && view.params.get(reply.index);
    if (row) {
      row.value.textContent = reply.text;
      row.value.title = reply.text;
    }
  });

  /** Opens an insert's parameters in the Inspector (from the Console). */
  Hub.showInsert = (channel, insertId) => {
    expanded.add(insertId);
    listKey = "";
    if (channel === Hub.MAIN) Hub.inspectMain(); else Hub.selectTrack(channel);
    if (document.body.classList.contains("hide-inspector")) Hub.run("toggleInspector");
    render();
  };

  Hub.on("ready", () => {
    const name = $("insp-name");

    name.addEventListener("change", () => {
      const track = Hub.selectedTrack();
      const value = name.value.trim();
      if (!Hub.state.inspectMain && track && value && value !== track.name) Hub.send("trackSettings", { id: track.id, name: value });
    });

    name.addEventListener("keydown", (event) => {
      if (event.key === "Enter") name.blur();
    });

    $("insp-remove").addEventListener("click", () => Hub.run("removeTrack"));
    $("insp-add-insert").addEventListener("click", (event) => {
      const channel = shownChannel();
      if (channel !== null) Hub.addEffectMenu(event.currentTarget, channel);
    });

    setupDrops();
    render();
  });
})();
