// D:\workspace\Fanan_web_hub\src\hub-sampling-blob.js
//
// The Sampling Studio's SFZ to Blob tab: SFZs and their samples packed into blobs, each one file
// a sampler made in the Plugins Creator Studio plays from the disk (its Options > Sample
// library...). A list, made one after another: Add SFZ..., Add Folder... (every SFZ in a folder
// and its folders), Add Last Sampled; each row its name, where its blob goes, and how it went
// (waiting, the progress, what it made and what was left out, or why it couldn't); F shows the
// blob in its folder, x takes it off the list. Where the blobs go (beside each SFZ, or one
// folder), how the samples are kept (as they are, 16-bit, 24-bit, float), Make Blobs / Stop.
// Protection: None (open blobs, every sampler plays them), From a Creator project... (a Plugins
// Creator Studio project's protection) or From a license.soni file...: the blobs are locked to
// that product, and its license.soni goes into their folder with them; the row says which product.
//
// Sonify does the work (MainBlobs.cpp, BlobMaker.cpp): Hub.send("sampling", {action: "blob", op});
// it answers with sampling {kind: "blob", items, beside, folder, samples, protection, running}.

(() => {
  "use strict";

  const { $ } = Hub;
  const st = { visible: false, data: null, run: null };
  const send = (op, data = {}) => Hub.send("sampling", Object.assign({ action: "blob", op }, data));

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
    name.textContent = item.name || fileName(item.sfz);
    const where = Hub.element("div", "sbl-where");
    where.textContent = (item.state === "done" ? "" : fileName(item.sfz) + "  \u2192  ") + item.blob;
    where.title = item.sfz;
    text.append(name, where);

    const status = Hub.element("div", "sbl-state");

    if (item.state === "working") {
      const bar = Hub.element("div", "sbl-bar");
      const fill = Hub.element("div", "sbl-fill");
      fill.style.width = Math.round((Number(item.progress) || 0) * 100) + "%";
      bar.appendChild(fill);
      const percent = Hub.element("span", "sbl-percent");
      percent.textContent = Math.round((Number(item.progress) || 0) * 100) + "%";
      status.append(bar, percent);
    } else if (item.state === "done") {
      const made = Hub.element("span", "sbl-made");
      made.textContent = `${item.regions} regions, ${item.samples} samples, ${sizeText(Number(item.bytes) || 0)}` + (item.locked ? ", protected" : "");
      status.appendChild(made);
      if (item.message) {
        const note = Hub.element("span", "sbl-note-text");
        note.textContent = item.message;
        note.title = item.message;
        status.appendChild(note);
      }
    } else if (item.state === "failed") {
      const why = Hub.element("span", "sbl-why");
      why.textContent = item.message || "It couldn't be made.";
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
    reveal.title = item.state === "done" ? "Show the blob in its folder" : "Show the folder it goes to";
    reveal.addEventListener("click", () => send("reveal", { path: item.state === "done" ? item.blob : folderOf(item.blob) }));
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

  // ---------------------------------------------------------------- protection
  // Its row, made here under "Blobs go": Protection [None / From a Creator project... / From a
  // license.soni file...] Choose... and which product it is.
  const protectionKinds = [
    ["none", "None"],
    ["project", "From a Creator project\u2026"],
    ["file", "From a license.soni file\u2026"]
  ];

  function makeProtectionRow() {
    const options = $("sbl-where") && $("sbl-where").closest(".sbl-options");
    if (!options || $("sbl-protection")) return;

    const box = Hub.element("div", "sbl-options");
    const label = Hub.element("span", "sbl-label");
    label.textContent = "Protection";

    const select = Hub.element("select", "smp-select");
    select.id = "sbl-protection";
    select.setAttribute("aria-label", "The protection the blobs are made with");
    select.title = "A protected blob opens only with its product's license.soni beside it, and plays its demo until the computer is registered";
    for (const [value, text] of protectionKinds) {
      const option = Hub.element("option");
      option.value = value;
      option.textContent = text;
      select.appendChild(option);
    }
    select.addEventListener("change", () => send("protection", { kind: select.value }));

    const choose = Hub.element("button", "smp-button small");
    choose.id = "sbl-protection-choose";
    choose.type = "button";
    choose.textContent = "Choose\u2026";
    choose.addEventListener("click", () => {
      const kind = st.data && st.data.protection ? st.data.protection.kind : "none";
      if (kind !== "none") send("protection", { kind });
    });

    const said = Hub.element("span", "sbl-folder-text");
    said.id = "sbl-protection-text";

    box.append(label, select, choose, said);
    options.after(box);
  }

  function renderProtection(data, running) {
    if (!$("sbl-protection")) return;
    const p = data.protection || { kind: "none" };
    const kind = protectionKinds.some(([value]) => value === p.kind) ? p.kind : "none";
    const select = $("sbl-protection");
    select.value = kind;
    select.disabled = running;

    const choose = $("sbl-protection-choose");
    choose.style.display = kind === "none" ? "none" : "";
    choose.disabled = running;
    choose.title = kind === "project" ? "Another Creator project" : "Another license.soni file";

    const said = $("sbl-protection-text");
    said.style.color = p.problem ? "rgb(240, 120, 110)" : "";   // the page's red (.sbl-why)
    if (kind === "none") said.textContent = "Open: every sampler plays them";
    else if (p.problem) said.textContent = p.problem;
    else said.textContent = `Locked to ${p.name || p.product}` + (p.vendor ? ` by ${p.vendor}` : "") + ` (${p.product})`;
    said.title = kind === "none" ? "" : [p.problem, p.source].filter(Boolean).join("\n");
  }

  function render() {
    if (!st.visible) return;
    const data = st.data || { items: [], beside: true, folder: "", samples: "keep", running: false };
    const items = Array.isArray(data.items) ? data.items : [];
    const running = Boolean(data.running);

    const list = $("sbl-list");
    list.replaceChildren();

    if (!items.length) {
      const empty = Hub.element("div", "sbl-empty");
      empty.textContent = "No SFZs yet: Add SFZ..., Add Folder... or Add Last Sampled.";
      list.appendChild(empty);
    }

    for (const item of items) list.appendChild(row(item, running));

    for (const b of $("sbl-where").querySelectorAll("button")) {
      b.classList.toggle("on", (b.dataset.where === "beside") === Boolean(data.beside));
      b.disabled = running;
    }

    $("sbl-folder").disabled = running;
    $("sbl-folder-text").textContent = data.beside ? "" : (data.folder || "No folder set yet");
    $("sbl-samples").value = data.samples || "keep";
    $("sbl-samples").disabled = running;
    renderProtection(data, running);

    const waiting = items.filter((i) => i.state === "waiting").length;
    const done = items.filter((i) => i.state === "done").length;
    const failed = items.filter((i) => i.state === "failed").length;
    const make = $("sbl-make");
    make.textContent = running ? "Stop" : "Make Blobs";
    make.classList.toggle("stop", running);
    make.disabled = !running && waiting === 0;

    const working = items.find((i) => i.state === "working");
    const said = [];
    if (running && working) said.push(`Making ${working.name || fileName(working.sfz)}` + (waiting ? `, ${waiting} waiting` : ""));
    else if (waiting) said.push(`${waiting} to make`);
    if (done) said.push(`${done} made`);
    if (failed) said.push(`${failed} couldn't be made`);
    $("sbl-status").textContent = said.join(", ");

    $("sbl-clear").disabled = !items.length;
  }

  // ---------------------------------------------------------------- Sonify's word
  Hub.on("sampling", (data) => {
    if (!data || data.kind !== "blob") return;
    const before = st.data;
    const items = Array.isArray(data.items) ? data.items : [];
    st.data = data;

    // A run: the ones it makes, and (once it's over) how it went, said once.
    if (data.running && !(before && before.running))
      st.run = new Set(items.filter((i) => i.state === "waiting" || i.state === "working").map((i) => i.id));

    if (before && before.running && !data.running && st.run) {
      const ran = items.filter((i) => st.run.has(i.id));
      const done = ran.filter((i) => i.state === "done").length;
      const failed = ran.filter((i) => i.state === "failed").length;
      st.run = null;
      if (done || failed) Hub.toast(failed ? `${done} made, ${failed} couldn't be made: their rows say why.` : `${done === 1 ? "The blob is" : done + " blobs are"} made.`);
    }

    render();
  });

  Hub.samplingBlob = {
    setVisible(on) {
      st.visible = Boolean(on);
      if (st.visible) { send("state"); render(); }
    }
  };

  // ---------------------------------------------------------------- wiring
  Hub.on("ready", () => {
    makeProtectionRow();
    $("sbl-add").addEventListener("click", () => send("add"));
    $("sbl-add-folder").addEventListener("click", () => send("addFolder"));
    $("sbl-add-last").addEventListener("click", () => send("addLast"));
    $("sbl-clear").addEventListener("click", () => send("clear"));
    $("sbl-where").addEventListener("click", (event) => {
      const b = event.target.closest("button");
      if (!b || b.disabled) return;
      if (b.dataset.where === "folder" && !(st.data && st.data.folder)) send("setFolder");
      else send("beside", { on: b.dataset.where === "beside" });
    });
    $("sbl-folder").addEventListener("click", () => send("setFolder"));
    $("sbl-samples").addEventListener("change", () => send("samples", { value: $("sbl-samples").value }));
    $("sbl-make").addEventListener("click", () => send(st.data && st.data.running ? "stop" : "start"));
  });
})();
