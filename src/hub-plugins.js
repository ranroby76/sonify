// D:\workspace\Fanan_web_hub\src\hub-plugins.js
//
// The Plug-In Manager (the menu bar's last button), as in Studio One: every plugin Sonify knows,
// in a table to sort, filter, show or hide.
//   Filter            by name, format, type and vendor (the counts are what each holds)
//   Plug-Ins          the table: the dot shows a plugin (filled) or hides it (hollow) from every list
//                     in Sonify (the Browser, the insert and instrument lists); click a column to
//                     sort by it; select rows (Ctrl / Shift for several) to act on them together;
//                     right-click for the plugin's menu
//   Statistics        how many there are, by format, type and vendor, and the most used
//   Format Priority   which format of a plugin found in several (the same name from the same
//                     vendor) Hide Duplicates keeps; it can do so after every scan
//   Blocklist         files left out because they couldn't be read (or crashed Sonify while being
//                     read), and those removed from the Browser: Remove takes the selected ones off
//                     the list (they're read again now), Reset Blocklist all of them
// The library's own events: library (hub-core.js keeps Hub.state.libraryAll), pluginScan.

(() => {
  "use strict";

  const { $, element } = Hub;
  const FORMATS = ["VST3", "WCLAP", "Sonify", "Built-in"];
  const KINDS = [["instrument", "Instrument"], ["effect", "Effect"], ["noteEffect", "Note FX"]];
  const COLUMNS = [
    ["shown", ""], ["name", "Name"], ["format", "Format"], ["vendor", "Vendor"], ["category", "Type"],
    ["version", "Version"], ["modified", "Last Modified"], ["lastUsed", "Last Used"], ["uses", "Uses"]
  ];

  let library = { entries: [], blocked: [], formatPriority: ["VST3", "WCLAP", "Sonify"], hideDuplicatesAfterScans: false, thirdParty: true };
  let open = false;
  let tab = "plugins";
  let sort = { key: "name", up: true };
  const filter = { name: "", formats: new Set(FORMATS), kinds: new Set(KINDS.map(([k]) => k)), vendors: null };   // vendors null: all
  const selected = new Set();          // rows, by keyOf
  let anchor = "";                     // the row a Shift-click selects from
  const blockedPicked = new Set();     // Blocklist files picked for Remove
  let scanText = "";

  const keyOf = (entry) => `${entry.file}\n${entry.id}`;
  const entries = () => library.entries || [];
  const vendorOf = (entry) => entry.vendor || "Unknown vendor";

  // ---------------------------------------------------------------- words
  /** "Today", "3 days ago", "Last month", "2 years ago"; "" for never. */
  function ago(ms) {
    if (!(ms > 0)) return "";
    const days = Math.floor((Date.now() - ms) / 86400000);
    if (days < 1) return "Today";
    if (days < 2) return "Yesterday";
    if (days < 30) return `${days} days ago`;
    const months = Math.floor(days / 30.4);
    if (months < 2) return "Last month";
    if (months < 12) return `${months} months ago`;
    const years = Math.floor(days / 365);
    return years < 2 ? "Last year" : `${years} years ago`;
  }

  const dateText = (ms) => ms > 0 ? new Date(ms).toLocaleString() : "Never";

  // ---------------------------------------------------------------- what's listed
  function passes(entry, skip) {
    if (skip !== "formats" && !filter.formats.has(entry.format)) return false;
    if (skip !== "kinds" && !filter.kinds.has(entry.kind)) return false;
    if (skip !== "vendors" && filter.vendors && !filter.vendors.has(vendorOf(entry))) return false;
    const q = filter.name.trim().toLowerCase();
    if (q && !`${entry.name} ${vendorOf(entry)} ${entry.category || ""}`.toLowerCase().includes(q)) return false;
    return true;
  }

  function sortValue(entry, key) {
    switch (key) {
      case "shown": return entry.hidden ? 1 : 0;
      case "modified": case "lastUsed": case "uses": return Number(entry[key]) || 0;
      case "vendor": return vendorOf(entry).toLowerCase();
      default: return String(entry[key] || "").toLowerCase();
    }
  }

  function listed() {
    const list = entries().filter((entry) => passes(entry));
    const dir = sort.up ? 1 : -1;
    list.sort((a, b) => {
      const x = sortValue(a, sort.key), y = sortValue(b, sort.key);
      const by = typeof x === "number" ? x - y : x.localeCompare(y, undefined, { numeric: true });
      return by * dir || a.name.localeCompare(b.name) || a.format.localeCompare(b.format);
    });
    return list;
  }

  // ---------------------------------------------------------------- acting
  const pluginsOf = (list) => list.map((entry) => ({ file: entry.file, id: entry.id }));

  function setShown(list, shown) {
    if (!list.length) return;
    for (const entry of list) entry.hidden = !shown;   // shown at once; the library confirms
    Hub.send("pluginManager", { action: shown ? "show" : "hide", plugins: pluginsOf(list) });
    render();
  }

  /** The rows a click on one acts on: the selection when the row is in it, else the row. */
  function actedOn(entry) {
    return selected.has(keyOf(entry)) ? entries().filter((item) => selected.has(keyOf(item))) : [entry];
  }

  function removeFromBrowser(list) {
    const files = [...new Set(list.filter((entry) => entry.format !== "Built-in").map((entry) => entry.file))];
    if (!files.length) { Hub.toast("Sonify's own plugins stay: hide them instead."); return; }
    // Nothing is lost: it waits on the Blocklist, where Remove brings it back.
    Hub.send("pluginManager", { action: "remove", files });
    Hub.toast(`${files.length === 1 ? "1 plugin file is" : `${files.length} plugin files are`} on the Blocklist now: Remove there brings it back.`);
    for (const entry of list) selected.delete(keyOf(entry));
  }

  function rowMenu(event, entry) {
    event.preventDefault();
    if (!selected.has(keyOf(entry))) { selected.clear(); selected.add(keyOf(entry)); anchor = keyOf(entry); renderTable(); }
    const list = actedOn(entry);
    const many = list.length > 1;
    const anyHidden = list.some((item) => item.hidden), anyShown = list.some((item) => !item.hidden);
    Hub.popupMenu(Hub.pointAnchor(event), [
      { label: many ? `Show ${list.length} Plug-Ins` : "Show", value: "show", disabled: !anyHidden },
      { label: many ? `Hide ${list.length} Plug-Ins` : "Hide", value: "hide", disabled: !anyShown },
      { separator: true },
      { label: entry.favorite ? "Remove from Favorites" : "Add to Favorites", value: "favorite", disabled: many },
      { label: "Show in Explorer", value: "reveal", disabled: many || entry.format === "Built-in" },
      { separator: true },
      { label: many ? `Remove ${list.length} from the Browser` : "Remove from the Browser", value: "remove" }
    ], (action) => {
      if (action === "show" || action === "hide") setShown(list, action === "show");
      else if (action === "favorite") Hub.send("setFavorite", { file: entry.file, id: entry.id, favorite: !entry.favorite });
      else if (action === "reveal") Hub.send("revealLibraryFile", { file: entry.file });
      else if (action === "remove") removeFromBrowser(list);
    });
  }

  function clickRow(event, entry) {
    const key = keyOf(entry);
    if (event.shiftKey && anchor) {
      const keys = listed().map(keyOf);
      const a = keys.indexOf(anchor), b = keys.indexOf(key);
      if (a >= 0 && b >= 0) {
        if (!event.ctrlKey) selected.clear();
        for (let i = Math.min(a, b); i <= Math.max(a, b); i++) selected.add(keys[i]);
      }
    } else if (event.ctrlKey || event.metaKey) {
      if (selected.has(key)) selected.delete(key); else selected.add(key);
      anchor = key;
    } else {
      selected.clear();
      selected.add(key);
      anchor = key;
    }
    renderTable();
  }

  // ---------------------------------------------------------------- drawing
  function checkRow(label, count, on, onChange, title) {
    const row = element("label", "pm-check" + (count === 0 ? " empty" : ""));
    const box = element("input");
    box.type = "checkbox";
    box.checked = on;
    box.addEventListener("change", () => onChange(box.checked));
    row.append(box, element("span", "pm-check-name", label), element("span", "pm-check-count", String(count)));
    if (title) row.title = title;
    return row;
  }

  function renderFilter() {
    const formats = $("pm-formats");
    formats.textContent = "";
    for (const format of FORMATS) {
      const count = entries().filter((entry) => entry.format === format && passes(entry, "formats")).length;
      formats.appendChild(checkRow(format, count, filter.formats.has(format), (on) => {
        if (on) filter.formats.add(format); else filter.formats.delete(format);
        render();
      }));
    }

    const kinds = $("pm-kinds");
    kinds.textContent = "";
    for (const [kind, label] of KINDS) {
      const count = entries().filter((entry) => entry.kind === kind && passes(entry, "kinds")).length;
      kinds.appendChild(checkRow(label, count, filter.kinds.has(kind), (on) => {
        if (on) filter.kinds.add(kind); else filter.kinds.delete(kind);
        render();
      }));
    }

    const vendors = $("pm-vendors");
    const scroll = vendors.scrollTop;
    vendors.textContent = "";
    const names = [...new Set(entries().map(vendorOf))].sort((a, b) => a.localeCompare(b, undefined, { sensitivity: "base" }));
    for (const vendor of names) {
      const count = entries().filter((entry) => vendorOf(entry) === vendor && passes(entry, "vendors")).length;
      vendors.appendChild(checkRow(vendor, count, !filter.vendors || filter.vendors.has(vendor), (on) => {
        if (!filter.vendors) filter.vendors = new Set(names);
        if (on) filter.vendors.add(vendor); else filter.vendors.delete(vendor);
        if (filter.vendors.size === names.length) filter.vendors = null;
        render();
      }));
    }
    vendors.scrollTop = scroll;
  }

  function renderPriority() {
    const box = $("pm-priority");
    box.textContent = "";
    const order = library.formatPriority || [];
    order.forEach((format, index) => {
      const row = element("div", "pm-priority-row");
      const up = element("button", "pm-arrow", "▲");
      up.type = "button";
      up.title = `${format} before ${order[index - 1] || ""}`;
      up.disabled = index === 0;
      up.addEventListener("click", () => movePriority(index, -1));
      const down = element("button", "pm-arrow", "▼");
      down.type = "button";
      down.title = `${format} after ${order[index + 1] || ""}`;
      down.disabled = index === order.length - 1;
      down.addEventListener("click", () => movePriority(index, 1));
      row.append(element("span", "pm-priority-number", String(index + 1)), element("span", "pm-priority-name", format), up, down);
      box.appendChild(row);
    });
    $("pm-auto-hide").checked = Boolean(library.hideDuplicatesAfterScans);
  }

  function movePriority(index, by) {
    const order = [...(library.formatPriority || [])];
    const to = index + by;
    if (to < 0 || to >= order.length) return;
    [order[index], order[to]] = [order[to], order[index]];
    library.formatPriority = order;
    renderPriority();
    Hub.send("pluginManager", { action: "priority", formats: order });
  }

  function renderBlocklist() {
    const box = $("pm-blocklist");
    box.textContent = "";
    const blocked = library.blocked || [];
    for (const file of [...blockedPicked]) if (!blocked.some((item) => item.file === file)) blockedPicked.delete(file);
    if (!blocked.length) box.appendChild(element("div", "pm-block-empty", "Nothing is left out."));
    for (const item of blocked) {
      const row = element("button", "pm-block" + (blockedPicked.has(item.file) ? " picked" : ""));
      row.type = "button";
      row.setAttribute("aria-pressed", String(blockedPicked.has(item.file)));
      row.append(element("span", "pm-block-name", item.name), element("span", "pm-block-why", item.removed ? "removed" : "failed"));
      row.title = `${item.file}\n${item.reason}`;
      row.addEventListener("click", () => {
        if (blockedPicked.has(item.file)) blockedPicked.delete(item.file); else blockedPicked.add(item.file);
        renderBlocklist();
      });
      box.appendChild(row);
    }
    $("pm-unblock").disabled = blockedPicked.size === 0;
    $("pm-reset-blocklist").disabled = blocked.length === 0;
    $("pm-block-count").textContent = blocked.length ? String(blocked.length) : "";
  }

  function renderTable() {
    const head = $("pm-head");
    head.textContent = "";
    for (const [key, label] of COLUMNS) {
      const th = element("th", `pm-col-${key}` + (sort.key === key ? (sort.up ? " sorted up" : " sorted down") : ""));
      th.dataset.sort = key;
      th.title = key === "shown" ? "Sort by shown or hidden" : `Sort by ${label}`;
      th.append(element("span", "", label));
      head.appendChild(th);
    }

    const list = listed();
    const body = $("pm-rows");
    const scroll = $("pm-table-box").scrollTop;
    body.textContent = "";
    for (const entry of list) {
      const key = keyOf(entry);
      const tr = element("tr", "pm-row" + (entry.hidden ? " hidden-plugin" : "") + (entry.missing ? " missing" : "") + (selected.has(key) ? " selected" : ""));
      tr.dataset.key = key;

      const dot = element("button", "pm-dot" + (entry.hidden ? "" : " on"));
      dot.type = "button";
      dot.dataset.dot = "1";
      dot.title = entry.hidden ? "Hidden: no list offers it. Click to show it" : "Shown. Click to hide it from every list";
      dot.setAttribute("aria-label", entry.hidden ? "Show" : "Hide");
      const dotCell = element("td", "pm-col-shown");
      dotCell.appendChild(dot);

      const name = element("td", "pm-col-name");
      name.append(element("span", `pm-badge ${entry.format === "Built-in" ? "builtin" : entry.format.toLowerCase()}`, entry.kind === "instrument" ? "♪" : entry.kind === "noteEffect" ? "N" : "FX"),
                  element("span", "pm-name", entry.name));
      if (entry.favorite) name.appendChild(element("span", "pm-star", "★"));
      if (entry.missing) name.appendChild(element("span", "pm-missing", "missing"));
      name.title = `${entry.name}\n${entry.file}`;

      const cells = [
        element("td", "pm-col-format", entry.format),
        element("td", "pm-col-vendor", vendorOf(entry)),
        element("td", "pm-col-category", entry.category || ""),
        element("td", "pm-col-version", entry.version || ""),
        element("td", "pm-col-modified", ago(entry.modified)),
        element("td", "pm-col-lastUsed", ago(entry.lastUsed) || "Never"),
        element("td", "pm-col-uses", entry.uses ? String(entry.uses) : "")
      ];
      cells[4].title = dateText(entry.modified);
      cells[5].title = dateText(entry.lastUsed);
      cells[1].title = vendorOf(entry);
      tr.append(dotCell, name, ...cells);
      body.appendChild(tr);
    }
    $("pm-table-box").scrollTop = scroll;
    $("pm-none").hidden = list.length > 0;

    const all = entries();
    const hidden = all.filter((entry) => entry.hidden).length;
    $("pm-summary").textContent = `${list.length} of ${all.length} listed · ${hidden} hidden` + (selected.size ? ` · ${selected.size} selected` : "");
    $("pm-show-all").disabled = !list.some((entry) => entry.hidden);
    $("pm-hide-all").disabled = !list.some((entry) => !entry.hidden);
    $("pm-remove").disabled = selected.size === 0;
    const own = library.thirdParty === false;
    $("pm-update").disabled = own;
    $("pm-rescan").disabled = own;
  }

  function statTable(title, rows) {
    const box = element("div", "pm-stat");
    box.appendChild(element("h3", "", title));
    const table = element("table", "pm-stat-table");
    for (const [label, value, detail] of rows) {
      const tr = element("tr");
      const name = element("td", "", label);
      if (detail) name.title = detail;
      tr.append(name, element("td", "pm-stat-value", String(value)));
      table.appendChild(tr);
    }
    if (!rows.length) table.appendChild(element("tr")).appendChild(element("td", "pm-stat-none", "None yet"));
    box.appendChild(table);
    return box;
  }

  function renderStatistics() {
    const box = $("pm-stats");
    box.textContent = "";
    const all = entries();
    const count = (test) => all.filter(test).length;
    const tally = (by) => {
      const map = new Map();
      for (const entry of all) map.set(by(entry), (map.get(by(entry)) || 0) + 1);
      return [...map].sort((a, b) => b[1] - a[1] || String(a[0]).localeCompare(String(b[0])));
    };

    box.append(
      statTable("Plug-ins", [
        ["Listed", all.length], ["Shown", count((e) => !e.hidden)], ["Hidden", count((e) => e.hidden)],
        ["Favorites", count((e) => e.favorite)], ["Missing files", count((e) => e.missing)],
        ["On the Blocklist", (library.blocked || []).length], ["Vendors", new Set(all.map(vendorOf)).size]
      ]),
      statTable("By format", tally((e) => e.format)),
      statTable("By type", tally((e) => (KINDS.find(([k]) => k === e.kind) || [0, e.kind])[1])),
      statTable("By category", tally((e) => e.category || "Other").slice(0, 12)),
      statTable("Top vendors", tally(vendorOf).slice(0, 12)),
      statTable("Most used", all.filter((e) => e.uses > 0).sort((a, b) => b.uses - a.uses).slice(0, 12)
        .map((e) => [`${e.name} (${e.format})`, e.uses, `Last used ${dateText(e.lastUsed)}`])),
      statTable("Newest files", all.filter((e) => e.modified > 0 && e.format !== "Built-in").sort((a, b) => b.modified - a.modified).slice(0, 12)
        .map((e) => [`${e.name} (${e.format})`, ago(e.modified), dateText(e.modified)]))
    );
  }

  function render() {
    if (!open) return;
    $("pm-tab-plugins").setAttribute("aria-selected", String(tab === "plugins"));
    $("pm-tab-stats").setAttribute("aria-selected", String(tab === "stats"));
    $("pm-plugins").hidden = tab !== "plugins";
    $("pm-stats").hidden = tab !== "stats";
    renderFilter();
    renderPriority();
    renderBlocklist();
    if (tab === "plugins") renderTable(); else renderStatistics();
    $("pm-scan").textContent = scanText;
  }

  // ---------------------------------------------------------------- opening
  function show() {
    open = true;
    $("plugin-manager").hidden = false;
    render();
    $("pm-name-filter").focus();
  }

  function hide() {
    open = false;
    $("plugin-manager").hidden = true;
  }

  Hub.commands.pluginManager = show;

  Hub.on("library", (data) => {
    library = data || library;
    const keys = new Set(entries().map(keyOf));
    for (const key of [...selected]) if (!keys.has(key)) selected.delete(key);
    render();
  });

  Hub.on("pluginScan", (data) => {
    scanText = data.state === "running" ? `Scanning ${data.done || 0} of ${data.total || 0}${data.current ? ": " + data.current : ""}…` : "";
    if (open) $("pm-scan").textContent = scanText;
  });

  Hub.on("ready", () => {
    const box = $("plugin-manager");
    $("pm-close").addEventListener("click", hide);
    box.addEventListener("pointerdown", (event) => { if (event.target === box) hide(); });
    box.addEventListener("keydown", (event) => {
      if (event.key === "Escape" && !document.querySelector(".popup-menu")) { event.stopPropagation(); hide(); }
      // Ctrl + A in the table: every listed row.
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "a" && !Hub.isTyping(event.target)) {
        event.preventDefault();
        for (const entry of listed()) selected.add(keyOf(entry));
        renderTable();
      }
    });

    $("pm-name-filter").addEventListener("input", (event) => { filter.name = event.target.value; render(); });
    $("pm-deselect").addEventListener("click", () => {
      filter.formats.clear();
      filter.kinds.clear();
      filter.vendors = new Set();
      render();
    });
    $("pm-reset").addEventListener("click", () => {
      filter.name = "";
      $("pm-name-filter").value = "";
      FORMATS.forEach((f) => filter.formats.add(f));
      KINDS.forEach(([k]) => filter.kinds.add(k));
      filter.vendors = null;
      render();
    });

    $("pm-tab-plugins").addEventListener("click", () => { tab = "plugins"; render(); });
    $("pm-tab-stats").addEventListener("click", () => { tab = "stats"; render(); });

    $("pm-head").addEventListener("click", (event) => {
      const th = event.target.closest("[data-sort]");
      if (!th) return;
      sort = sort.key === th.dataset.sort ? { key: sort.key, up: !sort.up } : { key: th.dataset.sort, up: !["modified", "lastUsed", "uses"].includes(th.dataset.sort) };
      renderTable();
    });

    const entryOf = (target) => {
      const tr = target.closest(".pm-row");
      return tr ? entries().find((entry) => keyOf(entry) === tr.dataset.key) : null;
    };
    $("pm-rows").addEventListener("click", (event) => {
      const entry = entryOf(event.target);
      if (!entry) return;
      if (event.target.closest("[data-dot]")) setShown(actedOn(entry), entry.hidden);
      else clickRow(event, entry);
    });
    $("pm-rows").addEventListener("contextmenu", (event) => {
      const entry = entryOf(event.target);
      if (entry) rowMenu(event, entry);
    });

    $("pm-show-all").addEventListener("click", () => setShown(listed().filter((e) => e.hidden), true));
    $("pm-hide-all").addEventListener("click", () => setShown(listed().filter((e) => !e.hidden), false));
    $("pm-duplicates").addEventListener("click", () => Hub.send("pluginManager", { action: "hideDuplicates" }));
    $("pm-remove").addEventListener("click", () => removeFromBrowser(entries().filter((e) => selected.has(keyOf(e)))));
    $("pm-update").addEventListener("click", () => Hub.send("rescanLibrary", { full: false }));
    $("pm-rescan").addEventListener("click", () => Hub.run("rescanLibrary"));
    $("pm-auto-hide").addEventListener("change", (event) => Hub.send("pluginManager", { action: "autoHide", on: event.target.checked }));
    $("pm-unblock").addEventListener("click", () => Hub.send("pluginManager", { action: "unblock", files: [...blockedPicked] }));
    $("pm-reset-blocklist").addEventListener("click", () => Hub.send("pluginManager", { action: "resetBlocklist" }));
    $("pm-folders").addEventListener("click", () => { hide(); Hub.run("pluginFolders"); });
    $("pm-add").addEventListener("click", () => Hub.run("addLibraryFiles"));
  });
})();
