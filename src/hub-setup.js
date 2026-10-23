// D:\workspace\Fanan_web_hub\src\hub-setup.js
//
// Song Setup (Song menu), after Studio One's:
//   General          the format recordings are written in (16 bit, 24 bit, 32 bit float) and
//                    the sample rate the device runs at.
//   Audio I/O Setup  the song's inputs and outputs as Studio One's matrix: a row each, a column
//                    for each of the device's channels.
//                      Inputs   mono or stereo, from the device's active input channels; audio
//                               tracks record from them. A mono input can be on several
//                               channels (M on each): it hears them all together, the same on
//                               both sides (a stereo sound of two same monos, recorded so).
//                      Outputs  Main, and the Sub outputs tracks and FX channels can play into
//                               instead, on any of the device's output channels (they're all
//                               open): a headphone mix, an outboard effect, another app down a
//                               virtual cable. A Sub can play the Main mix too. Audition is where
//                               the Browser's preview and the metronome play.
//                    Make Default keeps the inputs and outputs for new songs; Reset to Default
//                    brings them back here.
// Nothing changes until OK.

(() => {
  "use strict";

  const { $, element } = Hub;
  let draft = { inputs: [], outputs: [], audition: 0, recordBits: 24 };
  let ioTab = "inputs";
  let newId = -1;      // new outputs go with ids below 0; the engine gives them ids the song never used

  const clone = (value) => JSON.parse(JSON.stringify(value));
  const rowsOf = (kind) => (kind === "outputs" ? draft.outputs : draft.inputs);
  const channelNames = (kind) => (kind === "outputs" ? Hub.state.outputChannels : Hub.state.inputChannels) || [];

  /** A mono input's channels: its first (left), then the others it takes too (also). */
  const monoChannels = (row) => [row.left, ...(row.also || [])];
  const MAX_MONO_CHANNELS = 8;

  /** An input's mono row: a click puts M on the channel or takes it off (never its last one); a
      Shift+click puts it on that channel alone. */
  function toggleMono(row, channel, alone) {
    const now = monoChannels(row);
    let next;
    if (alone) next = [channel];
    else if (now.includes(channel)) next = now.length > 1 ? now.filter((c) => c !== channel) : now;
    else next = now.length < MAX_MONO_CHANNELS ? [...now, channel] : now;
    next = [...new Set(next)].sort((a, b) => a - b);
    row.left = next[0];
    row.also = next.slice(1);
  }

  /** Puts a row on a channel. A stereo row takes it and the next (the one before, at the end);
      Shift moves only its left channel, Alt only its right. */
  function assign(row, channel, count, mode) {
    if (row.right < 0) {
      row.left = channel;
      row.also = [];
    } else if (mode === "left") {
      if (channel !== row.right) row.left = channel;
    } else if (mode === "right") {
      if (channel !== row.left) row.right = channel;
    } else if (channel + 1 < count) {
      row.left = channel;
      row.right = channel + 1;
    } else if (channel > 0) {
      row.left = channel - 1;
      row.right = channel;
    }
  }

  /** Whether the song uses a row: an audio track records from the input, or something plays into the output. */
  function used(kind, row, index) {
    if (kind === "inputs") return Hub.state.tracks.some((track) => track.kind === "audio" && (track.input || 0) === index);
    if (row.id === 0) return true;
    return draft.audition === row.id || [...Hub.state.tracks, ...(Hub.state.busChannels || []), ...(Hub.state.fxChannels || [])].some((track) => track.output === row.id);
  }

  /** The first channels no other row takes (stereo: two side by side), for a new row. */
  function freeChannels(rows, stereo, count) {
    const taken = new Set();
    for (const row of rows) {
      taken.add(row.left);
      if (row.right >= 0) taken.add(row.right);
      for (const channel of row.also || []) taken.add(channel);
    }
    for (let c = 0; c + (stereo ? 1 : 0) < count; c++)
      if (!taken.has(c) && (!stereo || !taken.has(c + 1))) return [c, stereo ? c + 1 : -1];
    return [0, stereo ? 1 : -1];
  }

  // ---------------------------------------------------------------- the matrix
  function renderMatrix() {
    const kind = ioTab;
    const names = channelNames(kind);
    const rows = rowsOf(kind);
    const outputs = kind === "outputs";
    const table = $("io-matrix");
    table.replaceChildren();

    for (const button of document.querySelectorAll("#setup-io [data-io-tab]"))
      button.setAttribute("aria-selected", String(button.dataset.ioTab === kind));

    $("io-about").textContent = outputs
      ? "Main and the Sub outputs: tracks and FX channels play into Main unless the Console sends them to a Sub. Every output channel of the device is open."
      : "Inputs for audio tracks, taken from the device's active input channels (Settings \u2192 Audio Setup turns channels on).";

    // The heads: what each row has, then the device's channels by number and name.
    const head = element("tr");
    head.append(element("th", "io-name-head", outputs ? "Output" : "Input"), element("th", "io-small-head", "Used"));
    if (outputs) {
      const mix = element("th", "io-small-head", "Main mix");
      mix.title = "A Sub output that plays the Main mix too (after the Main fader)";
      head.appendChild(mix);
    }
    head.appendChild(element("th", "io-format-head", "Format"));
    names.forEach((name, channel) => {
      const cell = element("th", "io-channel");
      cell.title = `${channel + 1}: ${name}`;
      cell.append(element("span", "io-channel-name", name), element("span", "io-channel-number", String(channel + 1)));
      head.appendChild(cell);
    });
    head.appendChild(element("th", "io-remove-head"));
    const thead = element("thead");
    thead.appendChild(head);
    table.appendChild(thead);

    const body = element("tbody");
    const lost = [];

    rows.forEach((row, index) => {
      const main = outputs && row.id === 0;
      const tr = element("tr", "io-row" + (main ? " main" : ""));

      const name = element("input", "io-name");
      name.type = "text";
      name.value = row.name;
      name.maxLength = 40;
      name.setAttribute("aria-label", outputs ? "Output name" : "Input name");
      name.addEventListener("input", () => {
        row.name = name.value;
        renderAudition();
      });
      const nameCell = element("td");
      nameCell.appendChild(name);

      const usedCell = element("td", "io-small");
      usedCell.appendChild(element("span", used(kind, row, index) ? "io-dot on" : "io-dot"));
      tr.append(nameCell, usedCell);

      if (outputs) {
        const mixCell = element("td", "io-small");
        if (!main) {
          const box = element("input");
          box.type = "checkbox";
          box.checked = Boolean(row.mainMix);
          box.setAttribute("aria-label", `${row.name}: play the Main mix too`);
          box.addEventListener("change", () => { row.mainMix = box.checked; });
          mixCell.appendChild(box);
        }
        tr.appendChild(mixCell);
      }

      const format = element("select", "io-format");
      format.setAttribute("aria-label", "Mono or stereo");
      format.add(new Option("Mono", "mono"));
      format.add(new Option("Stereo", "stereo"));
      format.value = row.right >= 0 ? "stereo" : "mono";
      format.addEventListener("change", () => {
        const count = Math.max(names.length, 2);
        row.also = [];
        if (format.value === "mono") row.right = -1;
        else if (row.left + 1 < count) row.right = row.left + 1;
        else { row.right = row.left; row.left = Math.max(0, row.left - 1); }
        renderMatrix();
      });
      const formatCell = element("td");
      formatCell.appendChild(format);
      tr.appendChild(formatCell);

      names.forEach((channelName, channel) => {
        const cell = element("td", "io-cell");
        const mono = row.right < 0;
        // (An input's mono row can be on several channels: M on each.)
        const severalMono = !outputs && mono;
        const isLeft = row.left === channel, isRight = row.right === channel;
        const isMono = severalMono && monoChannels(row).includes(channel);
        if (isLeft || isRight || isMono) {
          cell.classList.add("on");
          cell.textContent = mono ? "M" : isLeft && isRight ? "LR" : isLeft ? "L" : "R";
        }
        cell.title = severalMono
          ? `${row.name || "This row"} on ${channel + 1}: ${channelName}. Click: M on or off (a mono track hears them all as one sound on both sides, a stereo track each on a side); Shift+click: on this one alone`
          : `${row.name || "This row"} on ${channel + 1}: ${channelName}`;
        cell.addEventListener("click", (event) => {
          if (severalMono) toggleMono(row, channel, event.shiftKey);
          else assign(row, channel, names.length, event.shiftKey ? "left" : event.altKey ? "right" : "pair");
          renderMatrix();
        });
        tr.appendChild(cell);
      });

      const removeCell = element("td");
      if (!main) {
        const remove = element("button", "icon-button", "\u00d7");
        remove.type = "button";
        remove.title = outputs ? "Remove this output: what plays into it goes to Main" : "Remove this input";
        remove.setAttribute("aria-label", `Remove ${row.name}`);
        remove.addEventListener("click", () => {
          rows.splice(index, 1);
          if (outputs && draft.audition === row.id) draft.audition = 0;
          renderMatrix();
        });
        removeCell.appendChild(remove);
      }
      tr.appendChild(removeCell);
      body.appendChild(tr);

      if (names.length && (row.left >= names.length || row.right >= names.length || (row.also || []).some((c) => c >= names.length))) lost.push(row.name || "A row");
    });

    table.appendChild(body);

    const empty = $("io-empty");
    empty.hidden = names.length > 0 && rows.length > 0;
    empty.textContent = !names.length
      ? "No audio device open: open one in Settings \u2192 Audio Setup to see its channels."
      : outputs ? "" : "No inputs: audio tracks will hear nothing.";

    const problem = $("io-problem");
    problem.hidden = !lost.length;
    problem.textContent = lost.length
      ? `${lost.join(", ")} ${lost.length === 1 ? "is" : "are"} on channels this device hasn't got: click others.${outputs && rows[0] && lost.includes(rows[0].name || "A row") ? " Main plays on the first two meanwhile." : ""}`
      : "";

    $("io-audition-row").hidden = !outputs;
    renderAudition();
  }

  function renderAudition() {
    const select = $("io-audition");
    select.replaceChildren(...draft.outputs.map((output) => new Option(output.name || "(no name)", String(output.id))));
    select.value = String(draft.audition);
    if (select.selectedIndex < 0) {
      draft.audition = 0;
      select.value = "0";
    }
  }

  function addRow(stereo) {
    const outputs = ioTab === "outputs";
    const rows = rowsOf(ioTab);
    const count = Math.max(channelNames(ioTab).length, 2);
    const [left, right] = freeChannels(rows, stereo, count);

    if (outputs) {
      if (rows.length > 32) return;   // Main and 32 Subs
      const number = rows.length;
      rows.push({ id: newId--, name: `Sub ${number}`, left, right, mainMix: false });
    } else {
      const number = rows.length + 1;
      rows.push({ name: stereo ? `Input ${number} L+R` : `Input ${number}`, left, right });
    }
    renderMatrix();
  }

  // ---------------------------------------------------------------- the dialog
  function showTab(name) {
    for (const tab of document.querySelectorAll("#song-setup [data-setup-tab]"))
      tab.setAttribute("aria-selected", String(tab.dataset.setupTab === name));
    $("setup-general").hidden = name !== "general";
    $("setup-io").hidden = name !== "io";
    if (name === "io") renderMatrix();
  }

  /** Opens Song Setup on a tab ("general" or "io"), and for Audio I/O, on its inputs or outputs. */
  function open(tab = "general", io) {
    draft = {
      inputs: clone(Hub.state.inputs || []).map((input) => ({ name: input.name, left: input.left, right: input.right,
                                                             also: input.right < 0 && Array.isArray(input.also) ? input.also.slice() : [] })),
      outputs: clone(Hub.state.outputs && Hub.state.outputs.length ? Hub.state.outputs : [{ id: 0, name: "Main", left: 0, right: 1 }])
        .map((output) => ({ id: output.id, name: output.name, left: output.left, right: output.right, mainMix: Boolean(output.mainMix) })),
      audition: Hub.state.audition || 0,
      recordBits: Hub.state.recordBits || 24
    };
    if (io) ioTab = io;
    $("setup-bits").value = String(draft.recordBits);
    const status = Hub.state.status;
    $("song-setup-rate").textContent = status && status.deviceOpen ? `${(status.sampleRate / 1000).toFixed(1)} kHz (the audio device's)` : "No audio device";
    showTab(tab);
    $("song-setup").hidden = false;
  }

  function apply() {
    draft.recordBits = Number($("setup-bits").value) || 24;
    Hub.send("songSetup", draft);
    closeSetup();
  }

  /** Closes it, letting go of the keyboard (or shortcuts would think we're still typing). */
  function closeSetup() {
    const box = $("song-setup");
    if (box.contains(document.activeElement)) document.activeElement.blur();
    box.hidden = true;
  }

  Hub.commands.songSetup = () => open();
  Hub.songSetup = { open };

  // The device's channels can change while it's open (another device chosen).
  Hub.on("tracks", () => {
    if (!$("song-setup").hidden && !$("setup-io").hidden && !$("song-setup").contains(document.activeElement)) renderMatrix();
  });

  Hub.on("ready", () => {
    for (const tab of document.querySelectorAll("#song-setup [data-setup-tab]"))
      tab.addEventListener("click", () => showTab(tab.dataset.setupTab));
    for (const tab of document.querySelectorAll("#setup-io [data-io-tab]"))
      tab.addEventListener("click", () => { ioTab = tab.dataset.ioTab; renderMatrix(); });
    $("setup-add-mono").addEventListener("click", () => addRow(false));
    $("setup-add-stereo").addEventListener("click", () => addRow(true));
    $("io-audition").addEventListener("change", (event) => { draft.audition = Number(event.target.value) || 0; });
    $("io-make-default").addEventListener("click", () => {
      Hub.send("songSetupDefault", { inputs: draft.inputs, outputs: draft.outputs, audition: draft.audition });
      Hub.toast("New songs will start with these inputs and outputs.");
    });
    $("io-reset-default").addEventListener("click", () => {
      const defaults = Hub.state.ioDefaults;
      if (!defaults) return;
      // The defaults' Subs are new to this song: what plays into its own Subs goes to Main.
      const ids = new Map([[0, 0]]);
      draft.inputs = clone(defaults.inputs || []);
      draft.outputs = clone(defaults.outputs || []).map((output) => {
        const id = output.id === 0 ? 0 : newId--;
        ids.set(output.id, id);
        return { id, name: output.name, left: output.left, right: output.right, mainMix: Boolean(output.mainMix) };
      });
      draft.audition = ids.has(defaults.audition) ? ids.get(defaults.audition) : 0;
      renderMatrix();
    });
    $("setup-ok").addEventListener("click", apply);
    $("setup-cancel").addEventListener("click", () => { closeSetup(); });
    $("song-setup").addEventListener("keydown", (event) => {
      if (event.key === "Escape") { event.preventDefault(); closeSetup(); }
    });
  });
})();
