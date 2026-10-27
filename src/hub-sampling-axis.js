// D:\workspace\Fanan_web_hub\src\hub-sampling-axis.js
//
// The Sampling Studio's AXIS Pack tab: cabinet impulse responses packed into one blob, the file
// AXIS plays its IR cabinets from (it models its amps itself). The same shape as SFZ to Blob
// beside it (hub-sampling-blob.js), and the same look: a list, Add IRs..., Add Folder...; each
// row its name and how it went (waiting,
// the progress, what it turned out to be, or why it couldn't be read); F shows it in its folder, x
// takes it off the list. Where the pack is written, what's done to the impulse responses, and
// Make Pack / Stop. Protection: None, From a Creator project... or From a license.soni file...,
// which locks the pack to that product so nothing can be lifted back out of it.
//
// Everything listed goes into the same pack, so one that can't be read is left out and its row
// says why; the rest still pack. The pack's own row at the bottom says how the writing went.
//
// Sonify does the work (MainAxisPack.cpp, AxisPacker.cpp): Hub.send("sampling", {action:
// "axisPack", op}); it answers with sampling {kind: "axisPack", items, pack, made, irs,
// protection, running}.

(() => {
  "use strict";

  const { $ } = Hub;
  const st = { visible: false, data: null, ran: false };
  const send = (op, data = {}) => Hub.send("sampling", Object.assign({ action: "axisPack", op }, data));

  function sizeText(bytes) {
    if (bytes >= 1073741824) return (bytes / 1073741824).toFixed(1) + " GB";
    if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + " MB";
    return Math.max(1, Math.round(bytes / 1024)) + " KB";
  }

  function fileName(path) {
    return String(path || "").split(/[\\/]/).pop();
  }

  function folderOf(path) {
    const parts = String(path || "").split(/[\\/]/);
    parts.pop();
    return parts.join(path && path.includes("\\") ? "\\" : "/");
  }

  // ---------------------------------------------------------------- the list
  function row(item, running) {
    const box = Hub.element("div", "sbl-row " + item.state);
    box.setAttribute("role", "listitem");

    const text = Hub.element("div", "sbl-text");
    const name = Hub.element("div", "sbl-name");
    name.textContent = (item.kind === "cab" ? "\u25a4  " : "\u25c9  ") + (item.name || fileName(item.file));
    name.title = "A cabinet: an impulse response";
    const where = Hub.element("div", "sbl-where");
    where.textContent = fileName(item.file);
    where.title = item.file;
    text.append(name, where);

    const status = Hub.element("div", "sbl-state");

    if (item.state === "working") {
      const waiting = Hub.element("span", "sbl-waiting");
      waiting.textContent = "Reading";
      status.appendChild(waiting);
    } else if (item.state === "done") {
      const made = Hub.element("span", "sbl-made");
      made.textContent = item.message || "Read";
      made.title = item.message || "";
      status.appendChild(made);
    } else if (item.state === "failed") {
      const why = Hub.element("span", "sbl-why");
      why.textContent = item.message || "It couldn't be read.";
      why.title = item.message || "";
      status.appendChild(why);
    } else {
      const waiting = Hub.element("span", "sbl-waiting");
      waiting.textContent = running ? "Waiting" : "Ready";
      status.appendChild(waiting);
    }

    const buttons = Hub.element("div", "sbl-buttons");
    const reveal = Hub.element("button", "smp-button small sbl-reveal");
    reveal.type = "button";
    reveal.textContent = "F";
    reveal.title = "Show it in its folder";
    reveal.addEventListener("click", () => send("reveal", { path: item.file }));
    const remove = Hub.element("button", "smp-button small sbl-remove");
    remove.type = "button";
    remove.textContent = "\u00d7";
    remove.title = "Take it off the list";
    remove.disabled = item.state === "working";
    remove.addEventListener("click", () => send("remove", { id: item.id }));
    buttons.append(reveal, remove);

    box.append(text, status, buttons);
    return box;
  }

  // The pack's own row, under the list: how the writing went.
  function packRow(data) {
    const made = data.made || { state: "none" };
    if (made.state === "none") return null;

    const box = Hub.element("div", "sbl-row " + (made.state === "working" ? "working" : made.state));
    box.setAttribute("role", "listitem");

    const text = Hub.element("div", "sbl-text");
    const name = Hub.element("div", "sbl-name");
    name.textContent = fileName(data.pack) || "The pack";
    const where = Hub.element("div", "sbl-where");
    where.textContent = data.pack || "";
    where.title = data.pack || "";
    text.append(name, where);

    const status = Hub.element("div", "sbl-state");

    if (made.state === "working") {
      const bar = Hub.element("div", "sbl-bar");
      const fill = Hub.element("div", "sbl-fill");
      fill.style.width = Math.round((Number(made.progress) || 0) * 100) + "%";
      bar.appendChild(fill);
      const percent = Hub.element("span", "sbl-percent");
      percent.textContent = Math.round((Number(made.progress) || 0) * 100) + "%";
      status.append(bar, percent);
    } else if (made.state === "done") {
      const said = Hub.element("span", "sbl-made");
      const amps = Number(made.amps) || 0;
      const cabs = Number(made.cabs) || 0;
      said.textContent = `${amps} ${amps === 1 ? "amp" : "amps"}, ${cabs} ${cabs === 1 ? "cabinet" : "cabinets"}, ${sizeText(Number(made.bytes) || 0)}` + (made.locked ? ", protected" : "");
      status.appendChild(said);
      if (made.message) {
        const note = Hub.element("span", "sbl-note-text");
        note.textContent = made.message;
        note.title = made.message;
        status.appendChild(note);
      }
    } else {
      const why = Hub.element("span", "sbl-why");
      why.textContent = made.message || "The pack couldn't be made.";
      why.title = made.message || "";
      status.appendChild(why);
    }

    const buttons = Hub.element("div", "sbl-buttons");
    const reveal = Hub.element("button", "smp-button small sbl-reveal");
    reveal.type = "button";
    reveal.textContent = "F";
    reveal.title = made.state === "done" ? "Show the pack in its folder" : "Show the folder it goes to";
    reveal.addEventListener("click", () => send("reveal", { path: made.state === "done" ? data.pack : folderOf(data.pack) }));
    buttons.appendChild(reveal);

    box.append(text, status, buttons);
    return box;
  }

  // ---------------------------------------------------------------- protection
  // Its row, made here under "Pack": the same three choices SFZ to Blob has.
  const protectionKinds = [
    ["none", "None"],
    ["project", "From a Creator project\u2026"],
    ["file", "From a license.soni file\u2026"]
  ];

  function makeProtectionRow() {
    const options = $("sax-irs") && $("sax-irs").closest(".sbl-options");
    if (!options || $("sax-protection")) return;

    const box = Hub.element("div", "sbl-options");
    const label = Hub.element("span", "sbl-label");
    label.textContent = "Protection";

    const select = Hub.element("select", "smp-select");
    select.id = "sax-protection";
    select.setAttribute("aria-label", "The protection the pack is made with");
    select.title = "A protected pack opens only with its product's license.soni beside it, and nothing can be lifted back out of it";
    for (const [value, text] of protectionKinds) {
      const option = Hub.element("option");
      option.value = value;
      option.textContent = text;
      select.appendChild(option);
    }
    select.addEventListener("change", () => send("protection", { kind: select.value }));

    const choose = Hub.element("button", "smp-button small");
    choose.id = "sax-protection-choose";
    choose.type = "button";
    choose.textContent = "Choose\u2026";
    choose.addEventListener("click", () => {
      const kind = st.data && st.data.protection ? st.data.protection.kind : "none";
      if (kind !== "none") send("protection", { kind });
    });

    const said = Hub.element("span", "sbl-folder-text");
    said.id = "sax-protection-text";

    box.append(label, select, choose, said);
    options.after(box);
  }

  function renderProtection(data, running) {
    if (!$("sax-protection")) return;
    const p = data.protection || { kind: "none" };
    const kind = protectionKinds.some(([value]) => value === p.kind) ? p.kind : "none";
    const select = $("sax-protection");
    select.value = kind;
    select.disabled = running;

    const choose = $("sax-protection-choose");
    choose.style.display = kind === "none" ? "none" : "";
    choose.disabled = running;
    choose.title = kind === "project" ? "Another Creator project" : "Another license.soni file";

    const said = $("sax-protection-text");
    said.style.color = p.problem ? "rgb(240, 120, 110)" : "";   // the page's red (.sbl-why)
    if (kind === "none") said.textContent = "Open: every copy of AXIS plays it";
    else if (p.problem) said.textContent = p.problem;
    else said.textContent = `Locked to ${p.name || p.product}` + (p.vendor ? ` by ${p.vendor}` : "") + ` (${p.product})`;
    said.title = kind === "none" ? "" : [p.problem, p.source].filter(Boolean).join("\n");
  }

  function render() {
    if (!st.visible) return;
    const data = st.data || { items: [], pack: "", irs: "level", made: { state: "none" }, running: false };
    const items = Array.isArray(data.items) ? data.items : [];
    const running = Boolean(data.running);

    const list = $("sax-list");
    list.replaceChildren();

    if (!items.length) {
      const empty = Hub.element("div", "sbl-empty");
      empty.textContent = "Nothing listed yet: Add IRs... picks cabinet impulse responses, Add Folder... every one in a folder.";
      list.appendChild(empty);
    }

    for (const item of items) list.appendChild(row(item, running));

    const made = packRow(data);
    if (made) list.appendChild(made);

    $("sax-pack").disabled = running;
    $("sax-pack-text").textContent = data.pack || "";
    $("sax-pack-text").title = data.pack || "";
    $("sax-irs").value = data.irs || "level";
    $("sax-irs").disabled = running;
    renderProtection(data, running);

    const amps = items.filter((i) => i.kind !== "cab").length;
    const cabs = items.length - amps;
    const failed = items.filter((i) => i.state === "failed").length;
    const working = items.find((i) => i.state === "working");

    const make = $("sax-make");
    make.textContent = running ? "Stop" : "Make Pack";
    make.classList.toggle("stop", running);
    make.disabled = !running && items.length === 0;

    const said = [];
    if (running && working) said.push(`Reading ${working.name || fileName(working.file)}`);
    else if (running) said.push("Writing the pack");
    else if (items.length) said.push(`${amps} ${amps === 1 ? "amp" : "amps"}, ${cabs} ${cabs === 1 ? "cabinet" : "cabinets"} to pack`);
    if (!running && failed) said.push(`${failed} couldn't be read`);
    $("sax-status").textContent = said.join(", ");

    $("sax-clear").disabled = !items.length;
  }

  // ---------------------------------------------------------------- Sonify's word
  Hub.on("sampling", (data) => {
    if (!data || data.kind !== "axisPack") return;
    const before = st.data;
    st.data = data;

    if (data.running) st.ran = true;

    // A run over: how it went, said once.
    if (before && before.running && !data.running && st.ran) {
      st.ran = false;
      const made = data.made || { state: "none" };
      if (made.state === "done") {
        const amps = Number(made.amps) || 0;
        const cabs = Number(made.cabs) || 0;
        Hub.toast(`The pack is made: ${amps} ${amps === 1 ? "amp" : "amps"} and ${cabs} ${cabs === 1 ? "cabinet" : "cabinets"} in one file.`);
      } else if (made.state === "failed" && made.message !== "Stopped.") {
        Hub.toast(made.message || "The pack couldn't be made.");
      }
    }

    render();
  });

  Hub.samplingAxis = {
    setVisible(on) {
      st.visible = Boolean(on);
      if (st.visible) { send("state"); render(); }
    }
  };

  // ---------------------------------------------------------------- wiring
  Hub.on("ready", () => {
    makeProtectionRow();
    $("sax-add-cabs").addEventListener("click", () => send("addCabs"));
    $("sax-add-folder").addEventListener("click", () => send("addFolder"));
    $("sax-clear").addEventListener("click", () => send("clear"));
    $("sax-pack").addEventListener("click", () => send("setPack"));
    $("sax-irs").addEventListener("change", () => send("irs", { value: $("sax-irs").value }));
    $("sax-make").addEventListener("click", () => send(st.data && st.data.running ? "stop" : "start"));
  });
})();
