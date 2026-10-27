// D:\workspace\Fanan_web_hub\src\hub-video.js
//
// The song's video, as Studio One shows it: a Video lane over the tracks, a strip of small
// pictures from where the picture starts to where it ends, and the Video window (Sonify's own,
// on the desktop) showing the picture where the song is.
//   Song > Import Video…   a video file (or drag one from the Browser's Files onto the arrangement):
//                          its sound goes on an audio track of its own, its picture where it's put
//   Drag the strip         moves the picture (snapped; Alt for free). Its sound's clip stays where
//                          it is: move them together by moving both.
//   Double-click           the Video window (double-click it, or F, for full screen on its screen)
//   Right-click            Video Window, Move to the Song's Start, Export Video…, Remove Video
//   Song > Export Video…   a new MP4: the picture as it is, the song's mix as its sound
// Moving and removing are each one Undo step; importing too (with its track and clip).

(() => {
  "use strict";

  const { $ } = Hub;
  const HEIGHT = 58;
  const STRIP_TOP = 4, STRIP_HEIGHT = HEIGHT - 9;

  let video = {};               // the last songVideo from Sonify
  let sheet = null;             // the small pictures, loaded: { url, image }
  let drag = null;              // { x, from, to, moved }
  let exporting = false;

  const map = () => Hub.tempoMap;
  const hasVideo = () => Boolean(video.file);
  const isShown = () => hasVideo() || Boolean(video.preparing);

  /** Where the picture would end if it started here (a song position). */
  function endFrom(start) {
    if (!(video.seconds > 0)) return start;
    return map().ppqAt(map().secondsAt(start) + video.seconds);
  }

  const startNow = () => (drag ? drag.to : video.start || 0);
  const xOf = (ppq) => Hub.arrange.xOf(ppq);

  // ---------------------------------------------------------------- showing it
  function layout() {
    const on = isShown();
    const head = $("video-head"), wrap = $("video-lane-wrap");
    if (head.hidden === !on && wrap.hidden === !on) return;
    head.hidden = !on;
    wrap.hidden = !on;
    head.style.height = HEIGHT + "px";
    wrap.style.height = HEIGHT + "px";
    Hub.emit("layout");
    requestAnimationFrame(() => { if (Hub.arrange) Hub.arrange.redraw(); });
  }

  function updateHead() {
    const name = $("video-head-name"), state = $("video-head-state"), button = $("video-window-button");
    if (video.preparing) {
      name.textContent = video.preparing.name || "Video";
      state.textContent = `${video.preparing.importing ? "Importing" : "Getting ready"}… ${Math.round(100 * (video.preparing.progress || 0))}%`;
    } else {
      name.textContent = video.name || "Video";
      state.textContent = video.missing ? "isn't there" : video.error ? "couldn't be read" : video.seconds > 0 ? timeText(video.seconds) : "";
      state.title = video.error || "";
    }
    $("video-head").classList.toggle("missing", Boolean(video.missing || video.error));
    name.title = video.file || "";
    button.setAttribute("aria-pressed", String(Boolean(video.window)));
    button.disabled = !hasVideo();
  }

  function timeText(seconds) {
    const minutes = Math.floor(seconds / 60);
    return `${minutes}:${(seconds - minutes * 60).toFixed(1).padStart(4, "0")}`;
  }

  // ---------------------------------------------------------------- drawing
  function sizeCanvas(canvas, width, height) {
    const scale = window.devicePixelRatio || 1;
    const w = Math.max(1, Math.round(width * scale)), h = Math.max(1, Math.round(height * scale));
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
      canvas.style.width = width + "px";
      canvas.style.height = height + "px";
    }
    const context = canvas.getContext("2d");
    if (context) context.setTransform(scale, 0, 0, scale, 0, 0);
    return context;
  }

  function draw() {
    if (!isShown() || !Hub.arrange || !Hub.arrange.xOf) return;
    const canvas = $("video-lane");
    const width = canvas.parentElement.clientWidth;
    const context = sizeCanvas(canvas, width, HEIGHT);
    if (!context) return;

    const colours = Hub.theme.lanes;
    context.fillStyle = colours.lane;
    context.fillRect(0, 0, width, HEIGHT);

    // The bar lines, as the arrangement's grid shows them.
    const scroll = Hub.arrange.scrollLeft();
    const from = Hub.arrange.ppqOfContentX(scroll), to = Hub.arrange.ppqOfContentX(scroll + width) + 1e-6;
    const step = Hub.arrange.gridStep(10);
    context.strokeStyle = colours.bar;
    context.beginPath();
    for (const line of map().lines(from, to, step.bars > 0 ? step : { q: map().barAt(from).length, bars: 1 })) {
      const x = Math.round(xOf(line.ppq)) + 0.5;
      context.moveTo(x, 0);
      context.lineTo(x, HEIGHT);
    }
    context.stroke();

    context.font = "11px Segoe UI, system-ui, sans-serif";
    context.textBaseline = "middle";

    if (!hasVideo()) {
      if (video.preparing) {
        context.fillStyle = Hub.theme.muted || "#9aa0a6";
        context.fillText(`Importing ${video.preparing.name}… ${Math.round(100 * (video.preparing.progress || 0))}%`, 10, HEIGHT / 2);
      }
      return;
    }

    const start = startNow(), end = Math.max(start, endFrom(start));
    const x0 = Math.round(xOf(start)), x1 = Math.max(x0 + 4, Math.round(xOf(end)));
    const left = Math.max(0, x0), right = Math.min(width, x1);

    if (right > left) {
      context.save();
      context.beginPath();
      context.rect(left, STRIP_TOP, right - left, STRIP_HEIGHT);
      context.clip();
      context.fillStyle = "#101113";
      context.fillRect(left, STRIP_TOP, right - left, STRIP_HEIGHT);
      drawPictures(context, start, x0, left, right);
      if (video.missing) {
        context.fillStyle = "rgba(160, 30, 30, 0.45)";
        context.fillRect(left, STRIP_TOP, right - left, STRIP_HEIGHT);
      }
      context.restore();

      // Its frame, and its name at its start (or the view's left edge).
      context.strokeStyle = drag ? Hub.accentLine() : video.missing ? "#d14b4b" : "rgba(255, 255, 255, 0.55)";
      context.lineWidth = 1;
      context.strokeRect(x0 + 0.5, STRIP_TOP + 0.5, x1 - x0 - 1, STRIP_HEIGHT - 1);

      const label = video.missing ? `${video.name} isn't there any more` : video.error ? `${video.name}: ${video.error}` : video.name;
      const labelX = Math.max(x0, 0) + 4;
      const labelWidth = Math.min(context.measureText(label).width + 8, Math.max(0, right - labelX - 2));
      if (labelWidth > 12) {
        context.fillStyle = "rgba(0, 0, 0, 0.6)";
        context.fillRect(labelX, STRIP_TOP + 2, labelWidth, 15);
        context.fillStyle = "#fff";
        context.save();
        context.beginPath();
        context.rect(labelX, STRIP_TOP + 2, labelWidth, 15);
        context.clip();
        context.fillText(label, labelX + 4, STRIP_TOP + 10);
        context.restore();
      }
    }

    if (video.preparing) {
      context.fillStyle = "rgba(0, 0, 0, 0.55)";
      context.fillRect(left, STRIP_TOP, Math.max(0, right - left), STRIP_HEIGHT);
      context.fillStyle = "#fff";
      context.fillText(`Getting the pictures ready… ${Math.round(100 * (video.preparing.progress || 0))}%`, Math.max(x0, 0) + 8, HEIGHT / 2 + 8);
    }
  }

  /** Small pictures side by side along the strip, each the one at its place in the video. */
  function drawPictures(context, start, x0, left, right) {
    const t = video.thumbnails;
    if (!t || !sheet || !sheet.image || !sheet.image.complete || sheet.url !== t.url || !t.count) return;

    const height = STRIP_HEIGHT - 2;
    const width = Math.max(8, Math.round(height * t.cellWidth / t.cellHeight));
    const startSeconds = map().secondsAt(start);
    const scroll = Hub.arrange.scrollLeft();
    const first = x0 + Math.floor((left - x0) / width) * width;

    for (let x = first; x < right; x += width) {
      const ppq = Hub.arrange.ppqOfContentX(x + width / 2 + scroll);
      const into = map().secondsAt(ppq) - startSeconds;
      const index = Hub.clamp(Math.floor(into / t.every + 0.5), 0, t.count - 1);
      const sx = (index % t.columns) * t.cellWidth, sy = Math.floor(index / t.columns) * t.cellHeight;
      context.drawImage(sheet.image, sx, sy, t.cellWidth, t.cellHeight, x, STRIP_TOP + 1, width - 1, height);
    }
  }

  function loadPictures() {
    const t = video.thumbnails;
    if (!t || !t.url) { sheet = null; return; }
    if (sheet && sheet.url === t.url) return;
    const image = new Image();
    sheet = { url: t.url, image };
    image.onload = () => draw();
    image.src = t.url;
  }

  // ---------------------------------------------------------------- the mouse
  function onStrip(event) {
    if (!hasVideo()) return false;
    const box = $("video-lane").getBoundingClientRect();
    const x = event.clientX - box.left, y = event.clientY - box.top;
    const x0 = xOf(video.start || 0), x1 = Math.max(x0 + 4, xOf(endFrom(video.start || 0)));
    return x >= x0 - 2 && x <= x1 + 2 && y >= STRIP_TOP - 2 && y <= STRIP_TOP + STRIP_HEIGHT + 2;
  }

  function onDown(event) {
    if (event.button !== 0 || !onStrip(event)) return;
    event.preventDefault();
    $("video-lane").setPointerCapture(event.pointerId);
    drag = { x: event.clientX, from: video.start || 0, to: video.start || 0, moved: false };
    draw();
  }

  function onMove(event) {
    if (!drag) {
      $("video-lane").style.cursor = onStrip(event) ? "grab" : "";
      return;
    }
    const dx = event.clientX - drag.x;
    if (!drag.moved && Math.abs(dx) < 3) return;
    drag.moved = true;
    const free = Hub.arrange.ppqOfContentX(Hub.arrange.contentX(drag.from) + dx);
    drag.to = Math.max(0, event.altKey ? free : Hub.arrange.snap(free));
    $("video-lane").style.cursor = "grabbing";
    draw();
  }

  function onUp() {
    if (!drag) return;
    const { from, to, moved } = drag;
    drag = null;
    $("video-lane").style.cursor = "";
    if (moved && Math.abs(to - from) > 1e-9) {
      video.start = to;          // shown at once; Sonify says the same back
      Hub.send("songVideo", { action: "move", start: to });
    }
    draw();
  }

  function onContextMenu(event) {
    event.preventDefault();
    if (!hasVideo()) return;
    const at = { getBoundingClientRect: () => ({ left: event.clientX, right: event.clientX, top: event.clientY, bottom: event.clientY }) };
    Hub.popupMenu(at, menuItems(), run);
  }

  function menuItems() {
    return [
      { label: video.window ? "Hide the Video Window" : "Video Window", value: "window" },
      { label: "Move to the Song's Start", value: "start", disabled: !(video.start > 0) },
      { label: "Export Video…", value: "export", disabled: Boolean(video.missing) },
      { label: "Remove Video", value: "remove" }
    ];
  }

  function run(pick) {
    if (pick === "window") Hub.run("toggleVideoWindow");
    if (pick === "start") Hub.send("songVideo", { action: "move", start: 0 });
    if (pick === "export") Hub.run("exportVideo");
    if (pick === "remove") Hub.run("removeVideo");
  }

  // ---------------------------------------------------------------- Export Video's progress
  function showExport(title, fraction, message, done) {
    $("video-export-title").textContent = title;
    $("video-export-fill").style.width = `${Math.round(Hub.clamp01(fraction) * 100)}%`;
    $("video-export-message").textContent = message;
    $("video-export-stop").textContent = done ? "Close" : "Cancel";
    $("video-export").hidden = false;
  }

  function hideExport() {
    const box = $("video-export");
    if (box.contains(document.activeElement)) document.activeElement.blur();
    box.hidden = true;
  }

  Hub.on("videoExport", (e) => {
    // Refused before it began (no video, not ready, recording…): said, and an export under way
    // (or the last one's box) is left as it is.
    if (e.state === "refused") { Hub.toast(e.error || "Something went wrong."); return; }
    if (e.name) $("video-export-file").textContent = e.name;
    if (e.state === "running") {
      exporting = true;
      showExport("Exporting the video…", e.progress || 0, e.progress < 0.5 ? "The song's mix…" : "The new video…", false);
      return;
    }
    exporting = false;
    if (e.state === "cancelled") { hideExport(); return; }
    if (e.state === "done") showExport("Video exported", 1, `Saved to ${e.file}`, true);
    else if (!e.file && e.error) { hideExport(); Hub.toast(e.error); }
    else showExport("The export didn't finish", 0, e.error || "Something went wrong.", true);
  });

  // ---------------------------------------------------------------- commands
  Hub.commands.importVideo = () => Hub.send("songVideo", { action: "import" });
  Hub.commands.toggleVideoWindow = () => {
    if (!hasVideo()) { Hub.toast("The song has no video: Song > Import Video adds one."); return; }
    Hub.send("songVideo", { action: "window", show: !video.window });
  };
  Hub.commands.exportVideo = () => {
    if (!hasVideo()) { Hub.toast("The song has no video: Song > Import Video adds one."); return; }
    Hub.send("songVideo", { action: "export" });
  };
  Hub.commands.removeVideo = () => { if (hasVideo()) Hub.send("songVideo", { action: "remove" }); };

  Hub.on("songVideo", (data) => {
    video = data || {};
    if (drag && !hasVideo()) drag = null;
    loadPictures();
    layout();
    updateHead();
    for (const id of ["toggleVideoWindow", "exportVideo", "removeVideo"])
      for (const item of document.querySelectorAll(`[data-command="${id}"]`)) item.disabled = !hasVideo();
    draw();
  });

  Hub.on("tempoMap", draw);
  Hub.on("theme", draw);

  Hub.videoLane = { draw, isShown, get: () => video };

  Hub.on("ready", () => {
    const canvas = $("video-lane");
    canvas.addEventListener("pointerdown", onDown);
    canvas.addEventListener("pointermove", onMove);
    canvas.addEventListener("pointerup", onUp);
    canvas.addEventListener("pointercancel", onUp);
    canvas.addEventListener("dblclick", (event) => { if (onStrip(event)) Hub.send("songVideo", { action: "window", show: true }); });
    canvas.addEventListener("contextmenu", onContextMenu);
    $("video-window-button").addEventListener("click", () => Hub.run("toggleVideoWindow"));
    $("video-menu-button").addEventListener("click", (event) => Hub.popupMenu(event.currentTarget, menuItems(), run));
    $("video-export-stop").addEventListener("click", () => {
      if (exporting) Hub.send("songVideo", { action: "cancelExport" });
      else hideExport();
    });

    // Ctrl + wheel zooms here too, as over the lanes.
    $("video-lane-wrap").addEventListener("wheel", (event) => {
      if (!event.ctrlKey || !Hub.arrange) return;
      event.preventDefault();
      Hub.arrange.setZoom(Hub.arrange.zoom() * (event.deltaY < 0 ? 1.2 : 1 / 1.2), event.clientX - $("lanes").getBoundingClientRect().left);
    }, { passive: false });

    for (const id of ["toggleVideoWindow", "exportVideo", "removeVideo"])
      for (const item of document.querySelectorAll(`[data-command="${id}"]`)) item.disabled = true;
  });
})();
