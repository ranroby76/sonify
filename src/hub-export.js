// D:\workspace\Fanan_web_hub\src\hub-export.js
//
// Song > Export Mixdown (Ctrl+E) and Song > Export Stems, after Studio One's: the song rendered
// faster than real time.
//   Name / Location   the song's name, in its Mixdown folder ("…" picks another)
//   Channels          (stems) the tracks, buses and FX channels to export, each to its own file in a
//                     "<name> Stems" folder, after its inserts, fader and pan (solos set aside,
//                     mutes kept); Main's mix with them when asked
//   Format            WAV (16 bit, 24 bit or 32 bit float), FLAC (16 or 24 bit, lossless),
//                     MP3 or OGG Vorbis (128 to 320 kbps)
//   Sample Rate       the device's, or converted to another (MP3 is 48 kHz at most)
//   Range             the whole song, or between the loop's locators
//   Tail              seconds after the range for effects to ring out
// A progress window follows, with Cancel; then the file (or the stems' folder), and Show in Explorer.
// The choices stay for the next time, until Sonify closes.

(() => {
  "use strict";

  const { $ } = Hub;
  const EXTENSIONS = { wav: ".wav", flac: ".flac", mp3: ".mp3", ogg: ".ogg" };
  let song = { name: "", mixdownFolder: "" };
  let folder = "", folderChosen = false, running = false, lastFile = "";
  let mode = "mixdown";                                   // or "stems"
  const last = { format: "wav", bits: "24", kbps: "320", main: false, unchecked: new Set() };

  /** Where the song ends: its last clip or envelope point. */
  function songEnd() {
    let end = 0;
    for (const track of Hub.state.tracks) {
      for (const clip of track.clips || []) end = Math.max(end, clip.start + clip.length);
      for (const lane of track.automation || []) if (lane.points.length) end = Math.max(end, lane.points[lane.points.length - 1][0]);
    }
    return end;
  }

  /** The bar (from 1) a position is in, on the song's bars. */
  const barOf = (ppq) => Hub.tempoMap.barAt(ppq + 1e-9).index + 1;

  /** The channels a stem can come from: tracks, then FX channels. */
  function channels() {
    return [...Hub.state.tracks.map((track) => ({ ...track, group: "Tracks" })),
            ...(Hub.state.busChannels || []).map((bus) => ({ ...bus, group: "Buses" })),
            ...(Hub.state.fxChannels || []).map((fx) => ({ ...fx, group: "FX Channels" }))];
  }

  function fillStems() {
    const list = $("export-stem-list");
    list.textContent = "";
    let group = "";
    for (const channel of channels()) {
      if (channel.group !== group) {
        group = channel.group;
        const heading = document.createElement("div");
        heading.className = "export-stem-group";
        heading.textContent = group;
        list.appendChild(heading);
      }
      const row = document.createElement("label");
      row.className = "export-stem";
      const box = document.createElement("input");
      box.type = "checkbox";
      box.dataset.channel = String(channel.id);
      box.checked = !last.unchecked.has(channel.id);
      const name = document.createElement("span");
      name.textContent = channel.number ? `${channel.number}. ${channel.name}` : channel.name;
      row.append(box, name);
      if (channel.mute) {
        const note = document.createElement("span");
        note.className = "muted small";
        note.textContent = "muted: exports silent";
        row.appendChild(note);
      }
      list.appendChild(row);
    }
    if (!list.children.length) list.textContent = "The song has no channels.";
  }

  const stemBoxes = () => [...document.querySelectorAll("#export-stem-list input[data-channel]")];

  /** Bit depth for WAV and FLAC, bit rate for MP3 and OGG. */
  function showQuality() {
    const format = $("export-format").value;
    const bits = $("export-bits");
    const lossy = format === "mp3" || format === "ogg";
    bits.hidden = lossy;
    $("export-kbps").hidden = !lossy;
    // FLAC has no floating point: 24 bit instead.
    bits.querySelector('option[value="32"]').disabled = format === "flac";
    if (format === "flac" && bits.value === "32") bits.value = "24";
    // MP3 goes up to 48 kHz: LAME converts anything higher.
    for (const option of $("export-rate").options) {
      const value = Number(option.value);
      option.disabled = format === "mp3" && value > 48000;
    }
    if ($("export-rate").selectedOptions[0] && $("export-rate").selectedOptions[0].disabled) $("export-rate").value = "0";
    const status = Hub.state.status;
    const deviceRate = status && status.deviceOpen ? status.sampleRate : 0;
    $("export-quality-note").textContent = format === "mp3" && deviceRate > 48000 && $("export-rate").value === "0"
      ? "MP3 is written at 48 kHz" : format === "flac" ? "lossless" : "";
  }

  function open(which) {
    if (running) return;
    mode = which === "stems" ? "stems" : "mixdown";
    const t = Hub.transportInfo();
    const status = Hub.state.status;
    const deviceRate = status && status.deviceOpen ? status.sampleRate : 0;

    $("export-title").textContent = mode === "stems" ? "Export Stems" : "Export Mixdown";
    $("export-ok").textContent = "Export";
    $("export-name").value = song.name || "Mixdown";
    if (!folderChosen) folder = song.mixdownFolder || "";
    $("export-folder").textContent = folder;
    $("export-folder").title = folder;

    const rate = $("export-rate");
    rate.textContent = "";
    rate.add(new Option(deviceRate ? `${(deviceRate / 1000).toFixed(1)} kHz (the device's)` : "The device's rate", "0"));
    for (const value of [44100, 48000, 88200, 96000])
      if (value !== deviceRate) rate.add(new Option(`${(value / 1000).toFixed(1)} kHz`, String(value)));

    $("export-format").value = last.format;
    $("export-bits").value = last.bits;
    $("export-kbps").value = last.kbps;
    showQuality();

    $("export-stems").hidden = mode !== "stems";
    $("export-main").checked = last.main;
    if (mode === "stems") fillStems();

    const end = songEnd();
    const hasSong = end > 0, hasLoop = t.loopEnd > t.loopStart;
    const songChoice = document.querySelector('input[name="export-range"][value="song"]');
    const loopChoice = document.querySelector('input[name="export-range"][value="loop"]');
    songChoice.disabled = !hasSong;
    loopChoice.disabled = !hasLoop;
    $("export-song-range").textContent = hasSong ? `Song (bars 1 – ${barOf(end - 1e-6)})` : "Song (empty)";
    $("export-loop-range").textContent = hasLoop ? `Loop (bars ${barOf(t.loopStart)} – ${barOf(t.loopEnd - 1e-6)})` : "Loop (no locators)";
    if (songChoice.disabled && !loopChoice.disabled) loopChoice.checked = true;
    if (loopChoice.disabled) songChoice.checked = true;

    // The mixdown is Main's, as in Studio One: what plays into a Sub output isn't in it (stems have every channel).
    const onSubs = mode === "stems" ? [] : [...Hub.state.tracks, ...(Hub.state.busChannels || []), ...(Hub.state.fxChannels || [])]
      .filter((track) => !track.bus && (track.output || 0) !== 0).map((track) => track.name);
    $("export-subs").hidden = !onSubs.length;
    $("export-subs").textContent = onSubs.length
      ? `The mixdown is Main's: ${onSubs.join(", ")} ${onSubs.length === 1 ? "plays into a Sub output, so it isn't" : "play into Sub outputs, so they aren't"} in it.`
      : "";

    problem((!hasSong && !hasLoop) ? "The song is empty: there's nothing to export." : "");
    $("export-ok").disabled = !hasSong && !hasLoop;

    $("export-dialog").hidden = false;
    $("export-name").focus();
    $("export-name").select();
  }

  function problem(text) {
    $("export-problem").hidden = !text;
    $("export-problem").textContent = text || "";
  }

  /** Hides a window, and lets go of the keyboard if it was in it (or shortcuts would think we're typing). */
  function hide(id) {
    const box = $(id);
    if (box.contains(document.activeElement)) document.activeElement.blur();
    box.hidden = true;
  }

  function remember() {
    last.format = $("export-format").value;
    last.bits = $("export-bits").value;
    last.kbps = $("export-kbps").value;
    if (mode === "stems") {
      last.main = $("export-main").checked;
      for (const box of stemBoxes()) {
        const id = Number(box.dataset.channel);
        if (box.checked) last.unchecked.delete(id);
        else last.unchecked.add(id);
      }
    }
  }

  function close() {
    remember();
    hide("export-dialog");
  }

  function start() {
    const range = document.querySelector('input[name="export-range"]:checked');
    if (!range || range.disabled) return;
    const name = $("export-name").value.trim() || song.name || "Mixdown";
    const format = $("export-format").value;
    const request = {
      name,
      folder,
      format,
      bits: Number($("export-bits").value),
      kbps: Number($("export-kbps").value),
      rate: Number($("export-rate").value),
      range: range.value,
      tail: Math.min(30, Math.max(0, Number($("export-tail").value) || 0)),
      reveal: $("export-reveal").checked
    };

    if (mode === "stems") {
      request.stems = stemBoxes().filter((box) => box.checked).map((box) => Number(box.dataset.channel));
      request.main = $("export-main").checked;
      if (!request.stems.length && !request.main) {
        problem("Choose at least one channel to export.");
        return;
      }
    }

    close();
    running = true;
    lastFile = "";
    showProgress("Exporting…", mode === "stems" ? `${name} Stems` : `${name}${EXTENSIONS[format] || ".wav"}`, 0, "");
    $("export-show").hidden = true;
    $("export-stop").textContent = "Cancel";
    Hub.send("exportMixdown", request);
  }

  function showProgress(title, file, fraction, message) {
    $("export-progress-title").textContent = title;
    if (file) $("export-file").textContent = file;
    $("export-fill").style.width = `${Math.round(Hub.clamp01(fraction) * 100)}%`;
    $("export-message").textContent = message;
    $("export-progress").hidden = false;
  }

  Hub.on("song", (s) => {
    song = { name: s.name || "", mixdownFolder: s.mixdownFolder || "" };
    if (s.opened) folderChosen = false;   // another song: its own Mixdown folder
  });

  Hub.on("mixdownFolder", (data) => {
    folder = data.path;
    folderChosen = true;
    $("export-folder").textContent = folder;
    $("export-folder").title = folder;
  });

  Hub.on("mixdown", (m) => {
    if (m.file) lastFile = m.file;
    if (m.state === "running") {
      running = true;
      showProgress("Exporting…", m.name, m.progress || 0, "");
      return;
    }
    running = false;
    if (m.state === "cancelled") {
      hide("export-progress");
      return;
    }
    $("export-stop").textContent = "Close";
    if (m.state === "done") {
      showProgress("Exported", m.name, 1, `Saved to ${m.file}`);
      $("export-show").hidden = false;
    } else {
      showProgress("The export didn't finish", m.name, 0, m.error || "Something went wrong.");
    }
  });

  Hub.commands.exportMixdown = () => open("mixdown");
  Hub.commands.exportStems = () => open("stems");

  Hub.on("ready", () => {
    $("export-ok").addEventListener("click", start);
    $("export-cancel").addEventListener("click", close);
    $("export-browse").addEventListener("click", () => Hub.send("chooseMixdownFolder", { folder }));
    $("export-format").addEventListener("change", showQuality);
    $("export-rate").addEventListener("change", showQuality);
    $("export-all").addEventListener("click", () => { for (const box of stemBoxes()) box.checked = true; problem(""); });
    $("export-none").addEventListener("click", () => { for (const box of stemBoxes()) box.checked = false; });
    $("export-stem-list").addEventListener("change", () => problem(""));
    $("export-main").addEventListener("change", () => problem(""));
    $("export-dialog").addEventListener("keydown", (event) => {
      if (event.key === "Escape") { event.preventDefault(); close(); }
      if (event.key === "Enter" && event.target.tagName !== "BUTTON" && event.target.type !== "checkbox") { event.preventDefault(); start(); }
    });
    $("export-stop").addEventListener("click", () => {
      if (running) Hub.send("cancelMixdown");
      else hide("export-progress");
    });
    $("export-show").addEventListener("click", () => { if (lastFile) Hub.send("revealLibraryFile", { file: lastFile }); });
  });
})();
