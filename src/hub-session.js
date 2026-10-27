// D:\workspace\Fanan_web_hub\src\hub-session.js
//
// Track > Add Session Players (also in the "+" over the track list): a drummer, a bass player and
// a keys player who play a chord progression in a style, each on a new track with a fitting
// instrument. One undo step.
//   Chords        bars between |, two chords in a bar split it, % repeats the bar before
//   From Track    the chords an instrument track plays, read off its notes (and where it plays them);
//                 or a recording's, listened to for its tempo, beat and chords: played to the song's
//                 beat, it keeps its place; with a tempo of its own, adding the players sets the song
//                 to it and lines the recordings up with the bars (Half / Double Tempo when it's heard
//                 at the wrong speed)
//   Style         Pop, Rock, Ballad, Funk, House or Hip-Hop
//   Intensity     from calm to busy
//   Start at Bar  the playhead's bar at first; Play n times
//   Players       each on or off, with its instrument: Automatic (the best fit installed), the
//                 Cayman, or any installed instrument
// The choices stay for the next time, until Sonify closes.

(() => {
  "use strict";

  const { $ } = Hub;
  const PLAYERS = ["drums", "bass", "keys"];

  let open = false, busy = false;
  let best = {};        // the engine's best fit for each player: {file, id, name}, or null (Cayman)
  let listening = 0;    // the recording being listened to
  let heard = null;     // what was heard in the recording chosen: {trackId, tempo, songTempo, follows, steadiness}
  const last = { chords: "", style: "pop", intensity: 50, times: 1,
                 players: { drums: true, bass: true, keys: true },
                 instruments: { drums: "auto", bass: "auto", keys: "auto" } };   // "auto", "none" or file + "\n" + id

  const checkbox = (player) => document.querySelector(`#session-dialog input[data-player="${player}"]`);
  const picker = (player) => document.querySelector(`#session-dialog select[data-instrument="${player}"]`);
  const keyOf = (entry) => `${entry.file}\n${entry.id}`;
  const clampInt = (value, min, max, fallback) => {
    const n = Math.round(Number(value));
    return Number.isFinite(n) ? Hub.clamp(n, min, max) : fallback;
  };

  function problem(text) {
    $("session-problem").textContent = text || "";
    $("session-problem").hidden = !text;
  }

  /** How many bars the chords take, read as the engine reads them. */
  function countBars(text) {
    const clean = text.replace(/[\r\n\t]/g, " ");
    if (!clean.includes("|")) return clean.split(" ").filter(Boolean).length;
    const parts = clean.split("|");
    // Empty at either end is the | around the bars; empty between two | is a bar the chord goes on through.
    return parts.filter((part, i) => part.trim() || (i > 0 && i < parts.length - 1)).length;
  }

  function showLength() {
    const bars = countBars($("session-chords").value) * clampInt($("session-times").value, 1, 64, 1);
    const start = clampInt($("session-start").value, 1, 9999, 1);
    $("session-length").textContent = bars ? `${bars} bar${bars === 1 ? "" : "s"}: ${start} – ${start + bars - 1}` : "";
  }

  function showIntensity() {
    const input = $("session-intensity");
    input.style.setProperty("--fill", `${input.value}%`);
  }

  function showPlayers() {
    for (const player of PLAYERS) picker(player).disabled = busy || !checkbox(player).checked;
  }

  function setBusy(on) {
    busy = on;
    $("session-busy").hidden = !on;
    $("session-go").disabled = on || Boolean(listening);
    for (const id of ["session-chords", "session-from", "session-style", "session-intensity", "session-start", "session-times"]) $(id).disabled = on;
    for (const player of PLAYERS) checkbox(player).disabled = on;
    showPlayers();
  }

  // ---------------------------------------------------------------- a recording heard
  const bpmText = (value) => `${Number(value).toFixed(Math.abs(value - Math.round(value)) < 0.005 ? 0 : 2)} BPM`;

  function showHeard(progress = 0) {
    const name = (Hub.trackById(listening || (heard && heard.trackId)) || {}).name || "the recording";
    $("session-listening").hidden = !listening;
    $("session-tempo-row").hidden = Boolean(listening) || !heard;
    $("session-heard").hidden = !listening && !heard;
    $("session-go").disabled = busy || Boolean(listening);

    if (listening) {
      $("session-listening-text").textContent = `Listening to ${name} for its tempo and chords…`;
      $("session-listen-fill").style.width = `${Math.round(Hub.clamp01(progress) * 100)}%`;
      return;
    }

    if (!heard) return;
    let text = heard.follows
      ? `${name} plays to the song's beat (${bpmText(heard.tempo)}).`
      : `${name} plays at ${bpmText(heard.tempo)}, the song at ${bpmText(heard.songTempo)}. Adding the players sets the song to ${bpmText(heard.tempo)} and lines the recordings up with its bars.`;
    if (heard.steadiness < 0.7) text += " Its tempo wanders, so the players may drift from it in places.";
    $("session-tempo").textContent = text;
    $("session-tempo-buttons").hidden = heard.follows;
  }

  /** Listens to a recording (at a tempo, when one is given). */
  function listenTo(trackId, tempo) {
    heard = null;
    listening = trackId;
    showHeard(0);
    Hub.send("sessionDetect", tempo ? { trackId, tempo } : { trackId });
  }

  function stopListening() {
    if (listening) Hub.send("sessionCancelDetect");
    listening = 0;
  }

  // ---------------------------------------------------------------- lists
  /** Where chords can come from: instrument tracks that play notes, audio tracks with recordings. */
  function fillTracks() {
    const from = $("session-from");
    const chosen = from.value;
    from.textContent = "";
    from.add(new Option("None", ""));
    for (const track of Hub.state.tracks) {
      const clips = track.clips || [];
      const notes = track.kind === "instrument" && clips.some((clip) => (clip.notes || []).length);
      const recording = track.kind === "audio" && clips.some((clip) => clip.audioFile);
      if (notes || recording) from.add(new Option(`${track.number}. ${track.name}${recording ? " (recording)" : ""}`, String(track.id)));
    }
    from.value = [...from.options].some((option) => option.value === chosen) ? chosen : "";
  }

  function isRecording(trackId) {
    const track = Hub.trackById(trackId);
    return Boolean(track && track.kind === "audio");
  }

  function fillInstruments() {
    const entries = Hub.state.library.filter((entry) => entry.kind === "instrument" && !entry.missing)
                                     .sort((a, b) => a.name.localeCompare(b.name, undefined, { sensitivity: "base" }) || (a.format || "").localeCompare(b.format || ""));
    const counts = {};
    for (const entry of entries) counts[entry.name] = (counts[entry.name] || 0) + 1;

    for (const player of PLAYERS) {
      const select = picker(player);
      const chosen = select.options.length ? select.value : last.instruments[player];
      select.textContent = "";
      select.add(new Option(automaticLabel(player), "auto"));
      select.add(new Option("Cayman", "none"));

      if (entries.length) {
        const group = document.createElement("optgroup");
        group.label = "Installed";
        for (const entry of entries)
          group.appendChild(new Option(counts[entry.name] > 1 ? `${entry.name} (${entry.format})` : entry.name, keyOf(entry)));
        select.appendChild(group);
      }

      select.value = [...select.options].some((option) => option.value === chosen) ? chosen : "auto";
    }
  }

  function automaticLabel(player) {
    return `Automatic: ${best[player] ? best[player].name : "Cayman"}`;
  }

  // ---------------------------------------------------------------- the window
  function playheadBar() {
    return Hub.tempoMap.barAt(Hub.songPosition() + 1e-9).index + 1;
  }

  function openDialog() {
    if (open) return;
    open = true;
    problem("");
    $("session-chords").value = last.chords;
    $("session-style").value = last.style;
    $("session-intensity").value = String(last.intensity);
    $("session-times").value = String(last.times);
    $("session-start").value = String(playheadBar());
    for (const player of PLAYERS) {
      checkbox(player).checked = last.players[player];
      picker(player).textContent = "";   // filled afresh, as chosen last time
    }
    $("session-from").value = "";
    heard = null;
    fillTracks();
    fillInstruments();
    showIntensity();
    showLength();
    showHeard();
    setBusy(busy);
    $("session-dialog").hidden = false;
    Hub.send("sessionSetup");   // each player's best fit
    $("session-chords").focus();
  }

  function closeDialog() {
    if (!open) return;
    open = false;
    stopListening();
    remember();
    const box = $("session-dialog");
    if (box.contains(document.activeElement)) document.activeElement.blur();
    box.hidden = true;
  }

  function remember() {
    last.chords = $("session-chords").value;
    last.style = $("session-style").value;
    last.intensity = clampInt($("session-intensity").value, 0, 100, 50);
    last.times = clampInt($("session-times").value, 1, 64, 1);
    for (const player of PLAYERS) {
      last.players[player] = checkbox(player).checked;
      last.instruments[player] = picker(player).value || "auto";
    }
  }

  function start() {
    if (busy || listening) return;
    const chords = $("session-chords").value.trim();
    const players = PLAYERS.filter((player) => checkbox(player).checked);
    if (!chords) {
      problem("Type the chords to play, or take them from a track.");
      $("session-chords").focus();
      return;
    }
    if (!players.length) {
      problem("Choose at least one player.");
      return;
    }

    const instruments = {};
    for (const player of players) {
      const value = picker(player).value;
      if (value === "none") instruments[player] = "none";
      else if (value && value !== "auto") {
        const [file, id] = value.split("\n");
        instruments[player] = { file, id };
      }
    }

    const startBar = clampInt($("session-start").value, 1, 9999, 1);
    const times = clampInt($("session-times").value, 1, 64, 1);
    $("session-start").value = String(startBar);
    $("session-times").value = String(times);
    // A recording with a tempo of its own: the song takes it, the recordings onto its bars.
    const alignTrack = heard && !heard.follows && String(heard.trackId) === $("session-from").value ? heard.trackId : 0;
    remember();
    problem("");
    setBusy(true);
    Hub.send("addSessionPlayers", { style: $("session-style").value, intensity: clampInt($("session-intensity").value, 0, 100, 50) / 100,
                                    players, instruments, chords, startBar, times, ...(alignTrack ? { alignTrack } : {}) });
  }

  // ---------------------------------------------------------------- the engine
  Hub.on("sessionSetup", (reply) => {
    best = (reply && reply.best) || {};
    if (!open) return;
    for (const player of PLAYERS) {
      const auto = picker(player).querySelector('option[value="auto"]');
      if (auto) auto.textContent = automaticLabel(player);
    }
  });

  Hub.on("sessionChords", (reply) => {
    if (!open || String(reply.trackId) !== $("session-from").value) return;

    if (reply.audio) {
      if (reply.listening) {
        listening = reply.trackId;
        showHeard(reply.progress);
        return;
      }
      listening = 0;
      if (reply.error || !reply.chords) {
        heard = null;
        showHeard();
        problem(reply.error || "No chords were heard in that recording.");
        return;
      }
      heard = { trackId: reply.trackId, tempo: reply.tempo, songTempo: reply.songTempo, follows: Boolean(reply.follows), steadiness: reply.steadiness };
    } else if (!reply.chords) {
      problem("That track plays no notes to take chords from.");
      return;
    }

    problem("");
    $("session-chords").value = reply.chords;
    $("session-start").value = String(reply.startBar || 1);
    $("session-times").value = "1";
    showLength();
    showHeard();
  });

  Hub.on("sessionPlayers", (reply) => {
    const asked = busy;
    setBusy(false);
    const notes = (reply.notes || []).join(" ");

    if (reply.ok) {
      closeDialog();
      if (notes) Hub.toast(notes);
      return;
    }

    if (open) problem(reply.error || "The players couldn't be added.");
    else if (asked) Hub.toast(reply.error || "The players couldn't be added.");
  });

  // Tracks and instruments come and go while the window is open.
  Hub.on("tracks", () => { if (open) fillTracks(); });
  Hub.on("library", () => { if (open) fillInstruments(); });

  Hub.commands.addSessionPlayers = openDialog;

  Hub.on("ready", () => {
    $("session-go").addEventListener("click", start);
    $("session-cancel").addEventListener("click", closeDialog);
    $("session-chords").addEventListener("input", () => { problem(""); showLength(); });
    $("session-start").addEventListener("input", showLength);
    $("session-times").addEventListener("input", showLength);
    $("session-intensity").addEventListener("input", showIntensity);
    for (const player of PLAYERS) checkbox(player).addEventListener("change", () => { problem(""); showPlayers(); });

    $("session-from").addEventListener("change", () => {
      const trackId = Number($("session-from").value);
      problem("");
      stopListening();
      heard = null;
      showHeard();
      if (trackId && isRecording(trackId)) listenTo(trackId);
      else if (trackId) Hub.send("sessionDetect", { trackId });
    });

    // Heard at the wrong speed: listened to again at half or twice it.
    const again = (factor) => {
      if (!heard || heard.follows) return;
      problem("");
      listenTo(heard.trackId, Hub.clamp(Math.round(heard.tempo * factor * 100) / 100, 20, 300));
    };
    $("session-half").addEventListener("click", () => again(0.5));
    $("session-double").addEventListener("click", () => again(2));

    $("session-dialog").addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        event.preventDefault();
        closeDialog();
      } else if (event.key === "Enter" && !event.shiftKey && !["BUTTON", "SELECT"].includes(event.target.tagName)) {
        event.preventDefault();   // Shift + Enter for a new line among the chords
        start();
      }
    });
  });
})();
