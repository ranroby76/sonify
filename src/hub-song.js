// D:\workspace\Fanan_web_hub\src\hub-song.js
//
// Songs: the Start page, the New Song dialog, and File > New / Open / Save / Save As.
// The engine owns the song and its file; this shows what it reports (the song's name,
// unsaved changes, recent songs) and asks it for the rest. The arrangement's view
// settings (zoom, snap and so on) are kept with the song.
//
//   Ctrl+N new song, Ctrl+O open, Ctrl+S save, Ctrl+Shift+S save as.
//   Ctrl+Z undo, Ctrl+Y or Ctrl+Shift+Z redo (the engine keeps the history; Edit > Undo
//   and Redo show the step's name, like "Undo Move Clips").

(() => {
  "use strict";

  const { $, element } = Hub;
  let song = { name: "Untitled", path: "", changed: false, busy: "", folder: "", user: "", recent: [] };
  let newSongFolder = "";
  let viewTimer = 0;

  /** "4 weeks ago", as the Start page lists recent files. */
  function ago(ms) {
    if (!ms) return "";
    const seconds = Math.max(0, (Date.now() - ms) / 1000);
    const units = [["year", 31536000], ["month", 2592000], ["week", 604800], ["day", 86400], ["hour", 3600], ["minute", 60]];
    for (const [unit, size] of units) {
      const count = Math.floor(seconds / size);
      if (count >= 1) return `${count} ${unit}${count > 1 ? "s" : ""} ago`;
    }
    return "just now";
  }

  // ---------------------------------------------------------------- Start page
  function renderRecent() {
    const list = $("recent-list");
    const filter = $("start-search").value.trim().toLowerCase();
    const items = (song.recent || []).filter((item) => !filter || item.name.toLowerCase().includes(filter));
    list.replaceChildren();

    for (const item of items) {
      const row = element("button", "recent-item" + (item.exists ? "" : " missing"));
      row.type = "button";
      row.title = item.exists ? item.path : `${item.path} (not found)`;
      row.innerHTML = '<svg viewBox="0 0 16 16"><path d="M3 1.5h7l3 3v10H3z" fill="none" stroke="currentColor" stroke-width="1.2"/></svg><span class="recent-name"></span><span class="recent-time"></span>';
      row.querySelector(".recent-name").textContent = item.name;
      row.querySelector(".recent-time").textContent = item.exists ? ago(item.time) : "not found";
      row.disabled = !item.exists;
      row.addEventListener("click", () => Hub.send("openSong", { path: item.path }));
      list.append(row);
    }

    $("recent-empty").hidden = items.length > 0;
  }

  function renderSetup(s) {
    const open = s && s.deviceOpen;
    $("setup-device").textContent = open ? s.deviceName : "No audio device";
    $("setup-rate").textContent = open ? `${(s.sampleRate / 1000).toFixed(1)} kHz` : "–";
    $("setup-buffer").textContent = open ? `${s.bufferSize} samples` : "–";
  }

  function showStart(show) {
    $("start-page").hidden = !show;
    if (show) {
      $("start-search").value = "";
      renderRecent();
      renderSetup(Hub.state.status);
    }
  }

  // ---------------------------------------------------------------- New Song dialog
  function today() {
    const d = new Date();
    return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
  }

  function showNewSong() {
    newSongFolder = song.folder;
    $("new-song-name").value = `${today()} ${song.user || ""}`.trim();
    $("new-song-folder").textContent = newSongFolder;
    $("new-song-bpm").value = "120";
    $("new-song-numerator").value = "4";
    $("new-song-denominator").value = "4";
    $("new-song").hidden = false;
    $("new-song-name").focus();
    $("new-song-name").select();
  }

  function createSong() {
    const name = $("new-song-name").value.trim();
    if (!name) { $("new-song-name").focus(); return; }
    const bpm = Hub.clamp(Number($("new-song-bpm").value) || 120, Hub.TempoMap.MIN_TEMPO, Hub.TempoMap.MAX_TEMPO);
    const numerator = Hub.clamp(Math.round(Number($("new-song-numerator").value)) || 4, 1, 32);
    Hub.send("newSong", { name, folder: newSongFolder, bpm, numerator, denominator: Number($("new-song-denominator").value) || 4 });
    $("new-song").hidden = true;
  }

  // ---------------------------------------------------------------- commands
  function sendView() {
    clearTimeout(viewTimer);
    if (Hub.arrange) Hub.send("songView", { view: Hub.arrange.getView() });
  }

  Hub.commands.newSong = showNewSong;
  Hub.commands.openSong = () => Hub.send("openSong", {});
  Hub.commands.saveSong = () => { sendView(); Hub.send("saveSong"); };
  Hub.commands.saveSongAs = () => { sendView(); Hub.send("saveSongAs"); };
  Hub.commands.showStartPage = () => showStart(true);
  Hub.commands.closeSong = () => Hub.send("closeSong");
  Hub.commands.importDawProject = () => Hub.send("dawProject", { action: "import" });
  Hub.commands.exportDawProject = () => Hub.send("dawProject", { action: "export" });

  /** The Start page closed by the user: the song behind it is the project now. */
  function closeStart() {
    showStart(false);
    Hub.send("startPageClosed");
  }

  let firstSong = true;   // the first news of the song since this page loaded
  Hub.commands.undo = () => Hub.send("undo");
  Hub.commands.redo = () => Hub.send("redo");

  function renderHistory() {
    for (const [id, verb, step] of [["menu-undo", "Undo", song.undo], ["menu-redo", "Redo", song.redo]]) {
      const item = $(id);
      item.querySelector(".menu-label").textContent = step ? `${verb} ${step}` : verb;
      item.disabled = !step;
    }
  }

  // ---------------------------------------------------------------- from the engine
  Hub.on("song", (data) => {
    song = data;
    Hub.state.song = data;
    renderHistory();
    $("busy").hidden = !data.busy;
    $("busy-text").textContent = data.busy || "";

    // A song was opened or made: its view settings, and the Start page and dialog go.
    if (data.opened) {
      if (Hub.arrange) Hub.arrange.applyView(data.view);
      showStart(false);
      $("new-song").hidden = true;
    }

    // No project open (Close Project): the Start page is the place. A page that loaded again
    // while a project was open goes straight back to it.
    if (data.welcome === true && data.opened) showStart(true);
    if (firstSong && data.welcome === false) showStart(false);
    firstSong = false;

    if (!$("start-page").hidden) renderRecent();
  });

  Hub.on("songFolder", (data) => {
    if (!data.path) return;
    newSongFolder = data.path;
    $("new-song-folder").textContent = newSongFolder;
  });

  Hub.on("status", (s) => { if (!$("start-page").hidden) renderSetup(s); });

  // The view settings reach the engine a moment after they change, ready for the next save.
  Hub.on("viewChanged", () => {
    clearTimeout(viewTimer);
    viewTimer = setTimeout(sendView, 300);
  });

  Hub.on("ready", () => {
    $("start-new").addEventListener("click", showNewSong);
    $("start-open").addEventListener("click", () => Hub.run("openSong"));
    $("start-close").addEventListener("click", closeStart);
    $("start-search").addEventListener("input", renderRecent);
    $("setup-configure").addEventListener("click", () => Hub.run("audioSetup"));

    $("new-song-ok").addEventListener("click", createSong);
    $("new-song-cancel").addEventListener("click", () => { $("new-song").hidden = true; });
    $("new-song-browse").addEventListener("click", () => Hub.send("chooseSongFolder", { folder: newSongFolder }));
    $("new-song").addEventListener("keydown", (event) => {
      if (event.key === "Enter") { event.preventDefault(); createSong(); }
      if (event.key === "Escape") { event.preventDefault(); $("new-song").hidden = true; }
    });

    window.addEventListener("keydown", (event) => {
      if (event.key === "Escape" && !$("start-page").hidden && $("new-song").hidden) closeStart();
    });

    showStart(true);   // like Studio One, the Start page greets you
  });
})();
