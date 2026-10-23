// D:\workspace\Fanan_web_hub\src\hub-import.js
//
// Media into the song (MainImport.cpp does the work):
//
//   Files dragged in straight from a folder on the computer (Explorer, Finder) and dropped in the
//   arrangement. The page can't learn where such files are, so as they come over the arrangement
//   Sonify lays a drop zone of its own over it; while they move there it says where they are, and
//   the page shows where they'd go: the track and the bar (snapped as the grid is), and what
//   happens to each of them.
//     On an audio track:  the first file on it, the others on new audio tracks under it.
//     Anywhere else:      each file on a new audio track (under the track there, or at the end).
//     Shift held:         all on one track, one after another.
//   Folders bring their audio files, by name; a video becomes the song's video.
//
//   Load Media... in an audio track's right-click menu (at the song position) and in the
//   arrangement's (where it was clicked): the files chosen go the same way.
//
// Files of a kind Sonify doesn't play as they are (M4A, AAC, Opus, WMA...) are read into WAVs
// first; the page says which while it's at it. A file from the computer dropped anywhere else in
// the page isn't taken (nor opened in the page's place).

(() => {
  "use strict";

  const { $ } = Hub;
  let askedAt = 0;               // when Sonify was last asked for its drop zone
  let hint = null;               // where the dragged files would go: { box, line, band, label }

  const fromComputer = (event) => Boolean(event.dataTransfer) && Array.from(event.dataTransfer.types || []).includes("Files");
  const inStudio = () => !Hub.studios || Hub.studios.active() === "studio";

  /** Where files dropped at this point of the page go. */
  function placeAt(clientX, clientY, shift) {
    const lanes = $("lanes").getBoundingClientRect();
    const start = Math.max(0, Hub.arrange.snap(Hub.arrange.ppqAt(clientX)));
    const track = clientY >= lanes.top && clientY < lanes.bottom ? Hub.arrange.trackAt(clientY) : null;
    if (track && track.kind === "audio") return { trackId: track.id, after: track.id, start, oneTrack: Boolean(shift), track };
    return { trackId: 0, after: track ? track.id : 0, start, oneTrack: Boolean(shift), track };
  }

  /** Load Media...: a file dialog of Sonify's; the files go onto this track from start (none: the song position). */
  function choose(trackId, start) {
    const track = Hub.trackById(trackId);
    Hub.send("importMedia", {
      action: "choose",
      trackId: track && track.kind === "audio" ? track.id : 0,
      after: track ? track.id : 0,
      start: Number.isFinite(start) ? start : Hub.songPosition()
    });
  }

  /** Load Media... where the arrangement was right-clicked. */
  function chooseAt(clientX, clientY) {
    const place = placeAt(clientX, clientY, false);
    Hub.send("importMedia", { action: "choose", trackId: place.trackId, after: place.after, start: place.start });
  }

  // ---------------------------------------------------------------- where they'd go
  function buildHint() {
    const box = Hub.element("div", "file-drop-hint");
    box.id = "file-drop-hint";
    box.hidden = true;
    const band = Hub.element("div", "fdh-band");
    const line = Hub.element("div", "fdh-line");
    const label = Hub.element("div", "fdh-label");
    box.append(band, line, label);
    $("arrange").appendChild(box);
    hint = { box, line, band, label };
  }

  function words(place, count, name) {
    const bar = Hub.tempoMap.barAt(place.start + 1e-9);
    const beat = Math.min(bar.numerator, Math.floor((place.start + 1e-9 - bar.start) / bar.beat) + 1);
    const where = ` at bar ${bar.index + 1}${beat > 1 ? "." + beat : ""}`;
    if (count <= 0) return "Nothing here Sonify can bring in";
    const files = count === 1 ? name : `${count} files`;
    if (place.trackId) {
      const on = place.track.name;
      if (count === 1) return `${files} \u2192 ${on}${where}`;
      if (place.oneTrack) return `${files} \u2192 ${on}, one after another${where}`;
      return `${files}${where}: the first on ${on}, the others on new tracks under it (Shift: all on ${on})`;
    }
    if (count === 1) return `${files} \u2192 a new audio track${where}`;
    if (place.oneTrack) return `${files} \u2192 one new audio track, one after another${where}`;
    return `${files} \u2192 ${count} new audio tracks${where} (Shift: one track, one after another)`;
  }

  function showHint(data) {
    if (!hint) return;
    if (!data || !data.active || !inStudio()) { hint.box.hidden = true; return; }

    const x = Number(data.x) || 0, y = Number(data.y) || 0;
    const place = placeAt(x, y, data.shift);
    const arrange = $("arrange").getBoundingClientRect();
    const lanes = $("lanes").getBoundingClientRect();
    const left = lanes.left - arrange.left, top = lanes.top - arrange.top;
    const width = $("lanes").clientWidth, height = $("lanes").clientHeight;
    const scrollTop = $("lanes").scrollTop;
    hint.box.hidden = false;

    const lineX = Hub.clamp(Hub.arrange.xOf(place.start), 0, width);
    Object.assign(hint.line.style, { left: `${left + lineX}px`, top: `${top}px`, height: `${height}px` });

    // The track it goes on, from there on; or the line new tracks go in at.
    const tracks = Hub.state.tracks;
    let bandTop, bandHeight;
    if (place.trackId) {
      const index = tracks.indexOf(place.track);
      bandTop = Hub.arrange.rowTop(index) - scrollTop;
      bandHeight = Hub.arrange.heightOf(place.track);
      hint.band.className = "fdh-band";
      Object.assign(hint.band.style, { left: `${left + lineX}px`, width: `${Math.max(0, width - lineX)}px` });
    } else {
      const index = place.after ? tracks.findIndex((t) => t.id === place.after) + 1 : tracks.length;
      bandTop = Hub.arrange.rowTop(index) - scrollTop - 1;
      bandHeight = 3;
      hint.band.className = "fdh-band insert";
      Object.assign(hint.band.style, { left: `${left}px`, width: `${width}px` });
    }
    const clippedTop = Hub.clamp(bandTop, 0, height), clippedBottom = Hub.clamp(bandTop + bandHeight, 0, height);
    hint.band.hidden = clippedBottom - clippedTop <= 0;
    Object.assign(hint.band.style, { top: `${top + clippedTop}px`, height: `${Math.max(0, clippedBottom - clippedTop)}px` });

    // What happens, by the pointer, kept inside the lanes.
    hint.label.textContent = words(place, Number(data.count) || 0, String(data.name || ""));
    const labelWidth = hint.label.offsetWidth || 200, labelHeight = hint.label.offsetHeight || 22;
    const labelX = Hub.clamp(x - arrange.left + 14, left + 4, left + width - labelWidth - 4);
    const labelY = Hub.clamp(y - arrange.top + 16, top + 4, top + height - labelHeight - 4);
    Object.assign(hint.label.style, { left: `${labelX}px`, top: `${labelY}px` });
  }

  // ---------------------------------------------------------------- what Sonify says
  function dropped(data) {
    if (hint) hint.box.hidden = true;
    const paths = Array.isArray(data && data.paths) ? data.paths : [];
    if (!paths.length || !inStudio()) return;
    const place = placeAt(Number(data.x) || 0, Number(data.y) || 0, data.shift);
    Hub.send("importMedia", { action: "files", paths, trackId: place.trackId, after: place.after, start: place.start, oneTrack: place.oneTrack });
  }

  function reading(data) {
    if (!data) return;
    if (data.state === "working" && data.name)
      Hub.toast(`Reading ${data.name} into the song (${(Number(data.done) || 0) + 1} of ${Number(data.total) || 1})\u2026`);
  }

  if (window.__JUCE__ && window.__JUCE__.backend) {
    window.__JUCE__.backend.addEventListener("arrangeFileDrag", (data) => Hub.emit("arrangeFileDrag", data));
    window.__JUCE__.backend.addEventListener("arrangeFileDrop", (data) => Hub.emit("arrangeFileDrop", data));
    window.__JUCE__.backend.addEventListener("mediaImport", (data) => Hub.emit("mediaImport", data));
  }

  Hub.on("arrangeFileDrag", showHint);
  Hub.on("arrangeFileDrop", dropped);
  Hub.on("mediaImport", reading);

  Hub.importMedia = { choose, chooseAt, placeAt };
  Hub.commands.loadMedia = () => choose(Hub.state.selected);

  // ---------------------------------------------------------------- the page's side of the drag
  Hub.on("ready", () => {
    buildHint();
    const lanes = $("lanes");

    const askSonify = () => {
      const now = performance.now();
      if (now - askedAt < 400 || !inStudio()) return;
      askedAt = now;
      const r = lanes.getBoundingClientRect();
      if (r.width < 40 || r.height < 20) return;
      Hub.send("importMedia", { action: "fileDrag", left: r.left, top: r.top, width: r.width, height: r.height });
    };

    lanes.addEventListener("dragenter", (event) => { if (fromComputer(event)) askSonify(); });
    lanes.addEventListener("dragover", (event) => {
      if (!fromComputer(event)) return;
      event.preventDefault();
      event.dataTransfer.dropEffect = "copy";
      askSonify();
    });
    lanes.addEventListener("drop", (event) => {
      if (!fromComputer(event)) return;
      event.preventDefault();
      Hub.toast("Hold the files over the arrangement a moment before letting go: Sonify takes them there.");
    });

    // Anywhere nothing takes them, a file from the computer isn't taken, nor opened in the page's place.
    document.addEventListener("dragover", (event) => {
      if (!fromComputer(event) || event.defaultPrevented) return;
      event.preventDefault();
      event.dataTransfer.dropEffect = "none";
    });
    document.addEventListener("drop", (event) => { if (fromComputer(event)) event.preventDefault(); });
  });
})();
