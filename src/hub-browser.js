// D:\workspace\Fanan_web_hub\src\hub-browser.js
//
// The Browser on the right, as in Studio One: instruments and effects from the library
// database, sorted Flat, by Folder, by Vendor or by Type, with Favorites and Recent at
// the top, and search across names, vendors, types and plugin features.
//
//   Drag an item onto a track, a Console strip or the Inspector's inserts, or into the
//   empty space below the tracks to make a new track. Double-click: an instrument makes
//   a new track; an effect joins the inserts of the channel in the Inspector.
//   Right-click an item: favorite, show its file, remove it from the Browser.
//   Plugin Folders (the folder button) are searched for WCLAPs at startup and on Rescan;
//   a plugin whose file has gone shows greyed out until it's back.
//   Files: folders and audio files, from places like Music or the song's Media folder.
//   Preview a file, see its format, double-click it (or Import) to put it on the selected
//   audio track at the playhead, or drag it onto the arrangement.
//   MIDI: the MIDI tools, built in and Magenta's (hub-miditools.js).
//   A plugin made in the Plugins Creator Studio (or any in its format) shows a picture of its
//   panel while the pointer rests on it (snapshot.png in its folder, drawn by Sonify).

(() => {
  "use strict";

  const { $, element } = Hub;
  const BUILTIN = { builtin: true, file: "", id: "test-synth", name: "Cayman", vendor: "Built-in", kind: "instrument",
                    category: "Synth", folder: "Built-in", features: ["instrument", "synthesizer"] };
  const KIND_TEXT = { instrument: "Instrument", effect: "Effect", noteEffect: "Note FX" };
  const RECENT_COUNT = 10;
  // Third-party plugins off for this run (the question at startup): only Sonify's own are offered.
  const OWN_ONLY = "Only Sonify's internal plugins this time. To use your own plugins, restart Sonify and choose Yes.";
  const ownOnly = () => library.thirdParty === false;
  let hint = "";   // the usual hint, as the page has it

  let tab = "instruments";
  let sortBy = "vendor";
  let query = "";
  let chosenKey = "";
  let library = { folders: [], scan: { active: false } };
  let listing = { path: "", parent: "", folders: [], files: [], places: [] };
  let chosenFile = "", fileInfo = null, listed = false;
  // Groups start closed (and close again when a song opens); while searching, all show open.
  const opened = new Set();
  const isOpen = (key) => opened.has(key) || query.trim() !== "";

  const keyOf = (entry) => entry.builtin ? "builtin" : `${entry.file}|${entry.id}`;
  // Built into Sonify: Cayman (the page's own) and Sonify's own effects (the engine's).
  const isBuiltin = (entry) => Boolean(entry.builtin) || entry.format === "Built-in";
  const typeOf = (entry) => entry.category || KIND_TEXT[entry.kind] || "Plugin";
  const inTab = (entry) => tab === "effects" ? entry.kind === "effect" : entry.kind !== "effect";
  const byName = (a, b) => a.localeCompare(b, undefined, { numeric: true, sensitivity: "base" });   // "Synth 2" before "Synth 10"

  function matches(entry, text) {
    if (!text) return true;
    return [entry.name, entry.vendor, typeOf(entry), ...(entry.features || [])].join(" ").toLowerCase().includes(text);
  }

  function entriesForTab() {
    const text = query.trim().toLowerCase();
    // Cayman is in the library as a built-in plugin (its window, its sound): listed once.
    const hasTestSynth = Hub.state.library.some((entry) => entry.id === "sonify.builtin.testsynth");
    return [...(hasTestSynth ? [] : [BUILTIN]), ...Hub.state.library].filter(inTab)
                                          .filter((entry) => matches(entry, text))
                                          .sort((a, b) => byName(a.name, b.name));
  }

  function groupOf(entry) {
    if (sortBy === "folder") return entry.builtin ? "Built-in" : entry.folder || "Added files";
    if (sortBy === "type") return typeOf(entry);
    return entry.vendor;
  }

  // ---------------------------------------------------------------- a made plugin's picture
  // Beside the Browser (it's on the right: to the left of the item), once the picture is in.
  const PREVIEW_DELAY = 350;           // ms the pointer rests before it shows
  let preview = null, previewTimer = 0, previewFor = null;

  const snapshotUrl = (entry) => `plugin-snapshot/${encodeURIComponent(entry.snapshot)}?v=${Math.round(entry.snapshotTime || 0)}`;

  function hidePreview() {
    clearTimeout(previewTimer);
    previewTimer = 0;
    previewFor = null;
    if (preview) preview.hidden = true;
  }

  function showPreview(item, entry) {
    hidePreview();
    if (!entry.snapshot || entry.missing) return;
    previewFor = item;
    previewTimer = setTimeout(() => {
      if (previewFor !== item || !item.isConnected) return;
      if (!preview) {
        preview = element("div", "plugin-preview");
        preview.setAttribute("aria-hidden", "true");
        preview.append(element("img", "plugin-preview-picture"), element("div", "plugin-preview-name"), element("div", "plugin-preview-about"));
        document.body.appendChild(preview);
      }
      const picture = preview.querySelector("img");
      preview.querySelector(".plugin-preview-name").textContent = entry.name;
      preview.querySelector(".plugin-preview-about").textContent = [entry.vendor, typeOf(entry), entry.version ? `v${entry.version}` : ""].filter(Boolean).join(" · ");

      const place = () => {
        if (previewFor !== item || !item.isConnected) return;
        preview.hidden = false;
        const r = item.getBoundingClientRect();
        const width = preview.offsetWidth, height = preview.offsetHeight;
        const left = r.left - width - 10 >= 4 ? r.left - width - 10 : Math.min(window.innerWidth - width - 4, r.right + 10);
        preview.style.left = Math.max(4, left) + "px";
        preview.style.top = Math.max(4, Math.min(window.innerHeight - height - 4, r.top + r.height / 2 - height / 2)) + "px";
      };

      const url = snapshotUrl(entry);
      if (picture.getAttribute("src") === url && picture.complete && picture.naturalWidth) { place(); return; }
      picture.onload = place;
      picture.onerror = () => { if (previewFor === item) hidePreview(); };
      picture.src = url;
    }, PREVIEW_DELAY);
  }

  // ---------------------------------------------------------------- items
  function showMenu(entry, x, y) {
    const sameFile = Hub.state.library.filter((other) => other.file === entry.file).length;
    const at = { getBoundingClientRect: () => ({ left: x, right: x, top: y, bottom: y }) };

    const favorite = { label: entry.favorite ? "Remove from Favorites" : "Add to Favorites", value: "favorite" };

    // Sonify's own effects have no file of their own to show or remove.
    Hub.popupMenu(at, isBuiltin(entry) ? [favorite] : [
      favorite,
      { label: "Show in Explorer", value: "reveal", disabled: entry.missing },
      { label: sameFile > 1 ? `Remove ${entry.fileName} from Browser (${sameFile} plugins)` : "Remove from Browser", value: "remove" }
    ], (value) => {
      if (value === "favorite") Hub.send("setFavorite", { file: entry.file, id: entry.id, favorite: !entry.favorite });
      if (value === "reveal") Hub.send("revealLibraryFile", { file: entry.file });
      if (value === "remove") { chosenKey = ""; Hub.send("removeLibraryFile", { file: entry.file }); }
    });
  }

  function itemFor(entry, nested) {
    const item = element("div", "browser-item" + (nested ? " nested" : "") + (entry.missing ? " missing" : ""));
    item.draggable = !entry.missing;
    item.dataset.key = keyOf(entry);
    item.setAttribute("role", "option");
    item.setAttribute("aria-selected", String(keyOf(entry) === chosenKey));
    item.appendChild(element("span", "browser-icon " + (entry.kind === "effect" ? "fx" : "inst")));
    item.appendChild(element("span", "browser-label", entry.name));
    if (entry.format === "VST3") item.appendChild(element("span", "browser-format", "VST3"));
    if (entry.format === "Sonify") item.appendChild(element("span", "browser-format made", "Made"));   // in the Plugins Creator Studio
    if (!isBuiltin(entry) && entry.file) {
      // The star: click it to mark the plugin a favorite, or to unmark it.
      const star = element("button", "browser-star" + (entry.favorite ? " on" : ""), entry.favorite ? "★" : "☆");
      star.type = "button";
      star.draggable = false;
      star.title = entry.favorite ? "Remove from Favorites" : "Add to Favorites";
      star.setAttribute("aria-pressed", String(!!entry.favorite));
      star.addEventListener("mousedown", (event) => event.stopPropagation());
      star.addEventListener("click", (event) => {
        event.stopPropagation();
        entry.favorite = !entry.favorite;   // shows at once; the library sends it back as kept
        Hub.send("setFavorite", { file: entry.file, id: entry.id, favorite: entry.favorite });
        render();
      });
      item.appendChild(star);
    }
    item.title = isBuiltin(entry) ? "Built into Sonify"
               : entry.missing ? `File not found: ${entry.file}`
               : entry.snapshot ? ""   // its picture shows instead
               : `${entry.vendor} · ${typeOf(entry)} · ${entry.fileName || ""}`;

    if (entry.snapshot) {
      item.addEventListener("pointerenter", (event) => { if (event.pointerType !== "touch") showPreview(item, entry); });
      item.addEventListener("pointerleave", hidePreview);
      item.addEventListener("pointerdown", hidePreview);
    }

    item.addEventListener("click", () => {
      chosenKey = keyOf(entry);
      render();
    });

    item.addEventListener("dblclick", () => {
      if (entry.missing) return;

      if (entry.kind !== "effect") {
        Hub.dropEntryOn(entry, null);
        return;
      }

      const channel = Hub.state.inspectMain ? Hub.MAIN : (Hub.selectedTrack() ? Hub.state.selected : null);
      Hub.dropEntryOn(entry, channel);
    });

    item.addEventListener("contextmenu", (event) => {
      event.preventDefault();
      if (entry.builtin) return;
      chosenKey = keyOf(entry);
      render();
      showMenu(entry, event.clientX, event.clientY);
    });

    item.addEventListener("dragstart", (event) => {
      hidePreview();
      Hub.dragEntry = entry;
      event.dataTransfer.effectAllowed = "copy";
      // What a Node Rack's window takes when the plugin is dropped on it (outside the page).
      event.dataTransfer.setData("text/plain", "sonify-plugin|" + entry.id + "|" + (entry.file || ""));
    });

    item.addEventListener("dragend", () => { Hub.dragEntry = null; });
    return item;
  }

  function folderRow(key, label, count, extraClass = "") {
    const folder = element("div", "browser-folder" + extraClass);
    folder.setAttribute("aria-expanded", String(isOpen(key)));
    folder.appendChild(element("span", "browser-caret"));
    folder.appendChild(element("span", "browser-label", label));
    folder.appendChild(element("span", "browser-count", String(count)));
    folder.addEventListener("click", () => {
      if (opened.has(key)) opened.delete(key);
      else opened.add(key);
      render();
    });
    return folder;
  }

  function addGroup(list, key, label, entries, extraClass) {
    list.appendChild(folderRow(key, label, entries.length, extraClass));
    if (isOpen(key))
      for (const entry of entries) list.appendChild(itemFor(entry, true));
  }

  // ---------------------------------------------------------------- the list, the info, the scan
  function renderInfo(entries) {
    const entry = entries.find((item) => keyOf(item) === chosenKey);
    const info = $("browser-info");
    info.hidden = !entry;
    if (!entry) return;

    $("info-icon").className = "browser-icon large " + (entry.kind === "effect" ? "fx" : "inst");
    $("info-name").textContent = entry.name;
    $("info-vendor").textContent = `Vendor: ${entry.vendor}`;
    $("info-kind").textContent = `Category: ${entry.builtin ? "Built-in" : entry.format || "WCLAP"} · ${typeOf(entry)}`;
    $("info-file").textContent = isBuiltin(entry) ? ""
                               : entry.missing ? `${entry.fileName || ""} (file not found)`
                               : [entry.version ? `Version ${entry.version}` : "", entry.fileName || ""].filter(Boolean).join(" · ");
    $("info-favorite").hidden = Boolean(entry.builtin);
    $("info-favorite").setAttribute("aria-pressed", String(Boolean(entry.favorite)));
    $("info-favorite").onclick = () => Hub.send("setFavorite", { file: entry.file, id: entry.id, favorite: !entry.favorite });
    $("info-remove").hidden = isBuiltin(entry);
    $("info-remove").onclick = () => {
      chosenKey = "";
      Hub.send("removeLibraryFile", { file: entry.file });
    };
  }

  function renderScan() {
    const scan = library.scan || {};
    $("browser-scan").hidden = !scan.active;
    if (!scan.active) return;
    $("browser-scan-text").textContent = `Scanning ${Math.min(scan.done + 1, scan.total)} of ${scan.total}: ${scan.current || ""}`;
    $("browser-scan-bar").style.width = `${scan.total ? (100 * scan.done) / scan.total : 0}%`;
  }

  // ---------------------------------------------------------------- Files
  const openFolder = (path) => Hub.send("listFolder", { path });

  function importFile(path) {
    const track = Hub.selectedTrack();
    Hub.send("importAudio", { file: path, trackId: track ? track.id : 0, start: Hub.songPosition() });
  }

  function togglePreview(path) {
    const status = Hub.state.status;
    if (status && status.previewing === path) Hub.send("stopPreview");
    else Hub.send("previewAudio", { path });
  }

  function fileRow(item, folder) {
    const row = element("div", "browser-item file-row" + (folder ? " folder" : ""));
    row.dataset.path = item.path;
    row.setAttribute("role", "option");
    row.setAttribute("aria-selected", String(item.path === chosenFile));
    row.appendChild(element("span", "file-icon", folder ? "📁" : item.video ? "🎞" : item.midi ? "🎹" : "♪"));
    row.appendChild(element("span", "browser-label", item.name));

    if (folder) {
      row.addEventListener("click", () => { chosenFile = item.path; render(); });
      row.addEventListener("dblclick", () => openFolder(item.path));
      return row;
    }

    // A video or a MIDI file: no preview here; double-click or drag it into the song.
    if (!item.video && !item.midi) {
      const previewing = Hub.state.status && Hub.state.status.previewing === item.path;
      const play = element("button", "file-play", previewing ? "■" : "▶");
      play.type = "button";
      play.title = previewing ? "Stop the preview" : "Preview";
      play.setAttribute("aria-label", previewing ? `Stop ${item.name}` : `Preview ${item.name}`);
      play.addEventListener("click", (event) => { event.stopPropagation(); togglePreview(item.path); });
      row.appendChild(play);
    }

    row.draggable = true;
    row.title = item.path;

    row.addEventListener("click", () => {
      chosenFile = item.path;
      fileInfo = item.video ? { path: item.path, video: true } : item.midi ? { path: item.path, midi: true } : null;
      if (!item.video && !item.midi) Hub.send("audioFileInfo", { path: item.path });
      render();
    });
    row.addEventListener("dblclick", () => importFile(item.path));
    row.addEventListener("dragstart", (event) => {
      Hub.dragFile = { path: item.path, name: item.name };
      event.dataTransfer.effectAllowed = "copy";
      event.dataTransfer.setData("text/plain", item.name);
    });
    row.addEventListener("dragend", () => { Hub.dragFile = null; });
    return row;
  }

  function describe(info) {
    if (info && info.video) return "Video: double-click it, or drag it onto the arrangement, to make it the song's video";
    if (info && info.midi) return "MIDI file: drag it onto an instrument track (or below the tracks for a new one), or double-click it";
    if (!info || !info.rate) return "Can't read this file";
    const minutes = Math.floor(info.length / 60), seconds = (info.length - minutes * 60).toFixed(1).padStart(4, "0");
    const channels = info.channels === 1 ? "Mono" : info.channels === 2 ? "Stereo" : `${info.channels} channels`;
    return `${info.format} · ${(info.rate / 1000).toFixed(1)} kHz · ${info.float ? "32 bit float" : info.bits + " bit"} · ${channels} · ${minutes}:${seconds}`;
  }

  function renderFiles() {
    if (!listed) { listed = true; openFolder(""); }

    const places = $("files-places");
    places.textContent = "";
    places.add(new Option(listing.path || "…", listing.path || ""));
    for (const place of listing.places || []) places.add(new Option(place.name, place.path));
    places.value = listing.path || "";
    $("files-up").disabled = !listing.parent;
    $("files-path").textContent = listing.path;

    const text = query.trim().toLowerCase();
    const keep = (item) => !text || item.name.toLowerCase().includes(text);
    const list = $("files-list");
    list.textContent = "";
    for (const folder of (listing.folders || []).filter(keep)) list.appendChild(fileRow(folder, true));
    for (const file of (listing.files || []).filter(keep)) list.appendChild(fileRow(file, false));
    $("files-none").hidden = list.children.length > 0;

    const chosen = (listing.files || []).find((file) => file.path === chosenFile);
    $("files-info").hidden = !chosen;
    if (chosen) {
      $("files-info-name").textContent = chosen.name;
      $("files-info-format").textContent = fileInfo && fileInfo.path === chosen.path ? describe(fileInfo) : "…";
      const previewing = Hub.state.status && Hub.state.status.previewing === chosen.path;
      $("files-preview").textContent = previewing ? "Stop" : "Preview";
      $("files-preview").hidden = Boolean(chosen.video || chosen.midi);
    }
  }

  function render() {
    hidePreview();   // its item is drawn again
    $("browser").classList.toggle("files", tab === "files");
    $("browser").classList.toggle("fanan", tab === "fanan");
    $("browser").classList.toggle("midi", tab === "midi");
    Hub.emit("browserTab", tab);
    for (const button of document.querySelectorAll("[data-browser-tab]"))
      button.setAttribute("aria-selected", String(button.dataset.browserTab === tab));

    if (tab === "files") {
      renderFiles();
      return;
    }

    if (tab === "fanan") {
      Hub.emit("contentTab");   // hub-content.js draws it
      return;
    }

    if (tab === "midi") {
      Hub.emit("midiTab");      // hub-miditools.js draws it
      return;
    }

    const entries = entriesForTab();
    const list = $("browser-list");
    list.textContent = "";

    for (const button of document.querySelectorAll("[data-sort]"))
      button.setAttribute("aria-pressed", String(button.dataset.sort === sortBy));

    // Favorites and Recent come first, as in Studio One (not while searching, and only when not empty).
    if (sortBy === "favorite") {
      const favorites = entries.filter((entry) => entry.favorite).sort((a, b) => byName(a.name, b.name));
      for (const entry of favorites) list.appendChild(itemFor(entry, false));
      if (!favorites.length)
        list.appendChild(element("div", "browser-empty", query.trim() ? "No favorites match the search." : "No favorites yet: click a plugin's ☆ to add it."));
    } else if (!query.trim()) {
      const favorites = entries.filter((entry) => entry.favorite);
      const recent = entries.filter((entry) => entry.lastUsed > 0).sort((a, b) => b.lastUsed - a.lastUsed).slice(0, RECENT_COUNT);
      if (favorites.length) addGroup(list, "special:favorites", "Favorites", favorites, " special favorites");
      if (recent.length) addGroup(list, "special:recent", "Recent", recent, " special recent");
    }

    if (sortBy === "favorite") {
      // listed above
    } else if (sortBy === "flat") {
      for (const entry of entries) list.appendChild(itemFor(entry, false));
    } else {
      const groups = new Map();

      for (const entry of entries) {
        const group = groupOf(entry);
        if (!groups.has(group)) groups.set(group, []);
        groups.get(group).push(entry);
      }

      const order = [...groups.keys()].sort((a, b) => (a === "Built-in") - (b === "Built-in") || byName(a, b));
      for (const group of order) addGroup(list, `${sortBy}:${group}`, group, groups.get(group), "");
    }

    const onlyBuiltin = Hub.state.library.length === 0;
    if (!hint) hint = $("browser-hint").textContent;
    $("browser-hint").textContent = ownOnly() ? OWN_ONLY : hint;
    $("browser-hint").hidden = (!onlyBuiltin && !ownOnly()) || query !== "";
    for (const id of ["browser-add", "browser-folders", "browser-rescan"]) $(id).hidden = ownOnly();
    $("browser-none").hidden = entries.length > 0;
    renderInfo(entries);
    renderScan();
  }

  // While third-party plugins are off, adding and rescanning them wait for the next start.
  // (The Plugin Folders are in File > Settings > Locations > Plug-ins: hub-settings.js.)
  const withThirdParty = (run) => () => ownOnly() ? Hub.toast(OWN_ONLY) : run();
  Hub.commands.addLibraryFiles = withThirdParty(() => Hub.send("addLibraryFiles"));
  Hub.commands.rescanLibrary = withThirdParty(() => Hub.send("rescanLibrary", { full: true }));

  Hub.on("library", (data) => {
    library = data;
    for (const command of ["addLibraryFiles", "rescanLibrary"])
      for (const item of document.querySelectorAll(`[data-command="${command}"]`)) item.disabled = ownOnly();
    render();
  });

  Hub.on("folderListing", (data) => {
    listing = data;
    chosenFile = "";
    render();
  });

  Hub.on("audioFileInfo", (info) => {
    fileInfo = info;
    if (tab === "files") render();
  });

  // The preview buttons follow what's playing.
  let previewing = "";
  Hub.on("status", (s) => {
    if ((s.previewing || "") === previewing) return;
    previewing = s.previewing || "";
    if (tab === "files") render();
  });

  Hub.on("song", (data) => {
    if (!data.opened) return;
    opened.clear();
    render();
  });

  Hub.commands.importAudioFiles = () => Hub.send("importAudioFiles");

  /** Shows one of the Browser's tabs (instruments, effects, files, fanan, midi). */
  Hub.showBrowserTab = (name) => {
    tab = name;
    render();
  };

  Hub.on("ready", () => {
    $("browser-list").addEventListener("scroll", hidePreview, { passive: true });
    for (const button of document.querySelectorAll("[data-browser-tab]")) {
      button.addEventListener("click", () => {
        tab = button.dataset.browserTab;
        render();
      });
    }

    for (const button of document.querySelectorAll("[data-sort]")) {
      button.addEventListener("click", () => {
        sortBy = button.dataset.sort;
        render();
      });
    }

    $("browser-search").addEventListener("input", (event) => {
      query = event.target.value;
      render();
    });

    $("browser-add").addEventListener("click", () => Hub.run("addLibraryFiles"));
    $("files-places").addEventListener("change", (event) => { openFolder(event.target.value); event.target.blur(); });
    $("files-up").addEventListener("click", () => { if (listing.parent) openFolder(listing.parent); });
    $("files-open").addEventListener("click", () => Hub.send("chooseFilesFolder"));
    $("files-preview").addEventListener("click", () => { if (chosenFile) togglePreview(chosenFile); });
    $("files-import").addEventListener("click", () => { if (chosenFile) importFile(chosenFile); });
    $("browser-folders").addEventListener("click", () => Hub.run("pluginFolders"));
    $("browser-rescan").addEventListener("click", () => Hub.run("rescanLibrary"));

    render();
  });
})();
