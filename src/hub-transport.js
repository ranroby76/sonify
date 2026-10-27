// D:\workspace\Fanan_web_hub\src\hub-transport.js
//
// The transport bar along the bottom of the window, laid out like Studio One's:
// performance and device readouts, the time display, transport buttons, loop
// locators, timing, tempo and metronome, then the panel toggles. Positions are written
// and read on the song channel's bars (Hub.tempoMap, hub-tempo.js); the tempo and the
// time signature show (and set) the ones where the song is.

(() => {
  "use strict";

  const { $ } = Hub;
  const DENOMINATORS = [1, 2, 4, 8, 16, 32];

  let transport = { playing: false, ppq: 0, seconds: 0, bpm: 120, numerator: 4, denominator: 4,
                    loop: false, loopStart: 0, loopEnd: 16, metronome: false, metronomeVolume: 0.6 };
  let receivedAt = 0;
  let showSeconds = false;
  let lastNoteOns = null;
  let ledTimer = 0;
  const tapTimes = [];

  // ---------------------------------------------------------------- formatting
  /** Studio One's bars display: bar.beat.sixteenth.ticks, as in 00001.01.01.00, on the song's bars. */
  const formatBars = (ppq) => Hub.tempoMap.formatBars(ppq);

  /**
      Reads a position typed Studio One's way: "5", "5.3", "5.3.2", "5.3.2.50" or the
      display's own "00005.03.02.50" (bar.beat.sixteenth.ticks, counted from 1).
      Returns quarter notes, or null when the text isn't a position.
  */
  const parseBars = (text) => Hub.tempoMap.parseBars(text);

  function formatSeconds(seconds) {
    const millis = Math.floor(Math.max(0, seconds) * 1000);
    const hours = Math.floor(millis / 3600000);
    const minutes = Math.floor(millis / 60000) % 60;
    const secs = Math.floor(millis / 1000) % 60;
    const pad = (value, width) => String(value).padStart(width, "0");
    return `${pad(hours, 2)}:${pad(minutes, 2)}:${pad(secs, 2)}.${pad(millis % 1000, 3)}`;
  }

  // Leaves a control alone while the user is working with it.
  function showValue(element, value) {
    if (document.activeElement !== element && element.value !== value) element.value = value;
  }

  // ---------------------------------------------------------------- position between status updates
  /** Song position now, advanced smoothly between the engine's 20-per-second updates: in time,
      then onto the map (the tempo may ramp). */
  Hub.songPosition = () => {
    if (!transport.playing) return transport.ppq;

    const map = Hub.tempoMap;
    const from = Number.isFinite(transport.seconds) ? transport.seconds : map.secondsAt(transport.ppq);
    let seconds = from + (performance.now() - receivedAt) / 1000;
    const { loopStart, loopEnd } = transport;

    if (transport.loop && transport.ppq < loopEnd && loopEnd > loopStart) {
      const startSeconds = map.secondsAt(loopStart), endSeconds = map.secondsAt(loopEnd);
      if (seconds >= endSeconds && endSeconds > startSeconds) seconds = startSeconds + (seconds - endSeconds) % (endSeconds - startSeconds);
    }

    return map.ppqAt(seconds);
  };

  Hub.transportInfo = () => transport;
  Hub.formatBars = formatBars;
  Hub.parseBars = parseBars;

  // ---------------------------------------------------------------- rendering
  // Like Studio One, Play lights up while playing; Stop is its own button.
  function renderPlaying(playing) {
    $("play").setAttribute("aria-pressed", String(playing));
  }

  function renderTime() {
    // Precount: the beats still to come, where the time shows.
    if (transport.counting) {
      $("time-value").textContent = `Count-in ${Math.max(1, transport.countBeats || 1)}`;
      $("time-unit").textContent = "Precount";
      return;
    }
    const ppq = Hub.songPosition();
    $("time-value").textContent = showSeconds ? formatSeconds(Hub.tempoMap.secondsAt(ppq)) : formatBars(ppq);
    $("time-unit").textContent = showSeconds ? "Seconds" : "Bars";
  }

  function renderLocators() {
    const start = formatBars(transport.loopStart);
    const end = formatBars(transport.loopEnd);
    $("locator-l").textContent = start;
    $("locator-r").textContent = end;
    showValue($("loop-start"), start);
    showValue($("loop-end"), end);
  }

  function render(t) {
    const wasPlaying = transport.playing;
    transport = t;
    receivedAt = performance.now();

    if (t.playing !== wasPlaying) renderPlaying(t.playing);

    renderTime();
    renderLocators();
    showValue($("tempo"), t.bpm.toFixed(2));
    showValue($("sig-num"), String(t.numerator));
    showValue($("sig-den"), String(t.denominator));
    $("loop").setAttribute("aria-pressed", String(t.loop));
    $("metronome").setAttribute("aria-pressed", String(t.metronome));
    $("record").setAttribute("aria-pressed", String(Boolean(t.recording)));
    document.body.classList.toggle("recording", Boolean(t.recording) && (t.playing || t.counting));
    document.body.classList.toggle("counting-in", Boolean(t.counting));
    renderRecordOptions(t.recordOptions);

    const volume = $("metronome-volume");
    showValue(volume, String(Math.round(t.metronomeVolume * 100)));
    volume.style.setProperty("--fill", volume.value + "%");
  }

  function renderStatus(s) {
    const cpu = Math.round((s.cpu || 0) * 100);
    $("cpu-fill").style.width = Hub.clamp(cpu, 0, 100) + "%";
    $("cpu-text").textContent = cpu + "%";
    $("perf").title = `Audio CPU ${cpu}%` + (s.xruns >= 0 ? ` · ${s.xruns} dropouts` : "");

    if (s.deviceOpen) {
      const khz = s.sampleRate / 1000;
      $("device-name").textContent = s.deviceName;
      $("device-detail").textContent = `${Number.isInteger(khz) ? khz : khz.toFixed(1)} kHz · ${s.bufferSize} · ${s.outputLatencyMs.toFixed(1)} ms`;
      $("device").title = `${s.typeName} · ${s.deviceName}\nBuffer ${s.bufferSize} samples (${s.bufferMs.toFixed(1)} ms), output latency ${s.outputLatencyMs.toFixed(1)} ms`;
    } else if (s.released) {
      $("device-name").textContent = "Audio device let go";
      $("device-detail").textContent = "Sonify is in the background";
      $("device").title = "Settings > Audio Setup > Release audio device in background: Sonify takes it back when it's in front again";
    } else {
      $("device-name").textContent = "No audio device";
      $("device-detail").textContent = "Open Audio Setup";
      $("device").title = "Choose a driver and device";
    }

    const level = (peak) => Hub.clamp01((20 * Math.log10(Math.max(peak || 0, 1e-6)) + 60) / 60) * 100 + "%";
    $("main-l").style.width = level(s.peakLeft);
    $("main-r").style.width = level(s.peakRight);

    const inputs = s.midiInputs || [];
    $("midi-led").title = inputs.length ? "MIDI inputs: " + inputs.map((input) => input.replace(/\|(\d+)$/, " \u00b7 Ch $1")).join(", ")
                                        : "No MIDI inputs (File → Settings → MIDI Setup)";

    if (lastNoteOns !== null && s.hardwareNoteOns !== lastNoteOns) {
      $("midi-led").classList.add("on");
      clearTimeout(ledTimer);
      ledTimer = setTimeout(() => $("midi-led").classList.remove("on"), 150);
    }

    lastNoteOns = s.hardwareNoteOns;
    if (s.transport) render(s.transport);
  }

  function tick() {
    if (transport.playing) renderTime();
    requestAnimationFrame(tick);
  }

  // ---------------------------------------------------------------- actions
  Hub.commands.togglePlay = () => {
    transport.playing = !transport.playing;
    renderPlaying(transport.playing);
    Hub.send("transport", { action: "toggle" });
  };

  Hub.commands.play = () => {
    transport.playing = true;
    renderPlaying(true);
    Hub.send("transport", { action: "play" });
  };

  // Like Studio One: Stop while stopped returns to zero.
  Hub.commands.stop = () => {
    if (transport.playing) {
      transport.playing = false;
      transport.recording = false;
      $("record").setAttribute("aria-pressed", "false");
      renderPlaying(false);
      Hub.send("transport", { action: "stop" });
    } else {
      Hub.run("returnToZero");
    }
  };

  Hub.commands.returnToZero = () => {
    transport.ppq = 0;
    transport.seconds = 0;
    receivedAt = performance.now();
    renderTime();
    Hub.send("transport", { action: "start" });
    if (Hub.arrange && Hub.arrange.reveal) Hub.arrange.reveal(0);   // the clips' view goes back with it
  };

  /** Back or forward one bar, as in every major DAW: Back from inside a bar goes to its start,
      from its start to the bar before (playing, a press just past a bar line counts as on it, so
      pressing again keeps stepping back); Forward goes to the next bar. On the song's bars (its
      time signatures), and the clips' view follows. */
  function stepBar(direction) {
    const map = Hub.tempoMap;
    const now = Hub.songPosition();
    const bar = map.barAt(now);
    let target;

    if (direction < 0) {
      const slack = transport.playing ? bar.length * 0.12 : 1e-6;
      target = now - bar.start > slack ? bar.start : map.ppqOfBar(Math.max(0, bar.index - 1));
    } else {
      target = bar.end;
    }

    Hub.locate(target);
    if (Hub.arrange && Hub.arrange.reveal) Hub.arrange.reveal(target);
  }

  Hub.commands.barBack = () => stepBar(-1);
  Hub.commands.barForward = () => stepBar(1);

  // ---------------------------------------------------------------- record options
  // Precount, Preroll, Autopunch and Postroll, as Studio One's transport has them; the menu
  // beside them chooses their bars. Kept with the song.
  const ROLL_OPTIONS = [["precount", "Precount"], ["preroll", "Preroll"], ["postroll", "Postroll"]];
  const BAR_CHOICES = [1, 2, 3, 4, 8];
  let recordOptions = { precount: false, precountBars: 1, preroll: false, prerollBars: 1, postroll: false, postrollBars: 1, autopunch: false };

  function renderRecordOptions(options) {
    if (options && typeof options === "object") recordOptions = Object.assign({}, recordOptions, options);
    for (const button of document.querySelectorAll(".rec-opt")) {
      const name = button.dataset.option;
      const on = Boolean(recordOptions[name]);
      button.setAttribute("aria-pressed", String(on));
      const bars = recordOptions[name + "Bars"];
      const label = { precount: "Precount", preroll: "Preroll", postroll: "Postroll", autopunch: "Autopunch" }[name];
      const text = bars ? `${label} ${bars}` : label;
      if (button.textContent !== text) button.textContent = text;
    }
  }

  function setRecordOption(changes) {
    recordOptions = Object.assign({}, recordOptions, changes);
    renderRecordOptions();
    Hub.send("transport", { recordOptions: changes });
    if (Hub.arrange) Hub.arrange.redraw();   // Autopunch shows on the loop lane
  }

  function openRecordOptionsMenu(anchor) {
    // Each on or off (a click), its bars beside it (the pointer over it).
    const items = ROLL_OPTIONS.map(([name, label]) => ({
      label: `${label}: ${recordOptions[name + "Bars"]} ${recordOptions[name + "Bars"] === 1 ? "bar" : "bars"}`, value: "toggle:" + name,
      checked: Boolean(recordOptions[name]),
      submenu: BAR_CHOICES.map((bars) => ({ label: bars === 1 ? "1 bar" : `${bars} bars`, value: `${name}:${bars}`, checked: recordOptions[name + "Bars"] === bars }))
    }));
    items.push({ separator: true }, { label: "Autopunch", value: "toggle:autopunch", checked: Boolean(recordOptions.autopunch),
                                      title: "Records only between the loop's start and end" });
    Hub.popupMenu(anchor, items, (value) => {
      if (typeof value !== "string") return;
      if (value.startsWith("toggle:")) { const name = value.slice(7); setRecordOption({ [name]: !recordOptions[name] }); return; }
      const [name, bars] = value.split(":");
      // Choosing its bars switches it on.
      setRecordOption({ [name + "Bars"]: Number(bars), [name]: true });
    });
  }

  Hub.recordOptions = () => recordOptions;

  // Like Studio One: Record starts playback when stopped; pressed while recording it
  // punches out and playback goes on.
  Hub.commands.toggleRecord = () => {
    transport.recording = !transport.recording;
    $("record").setAttribute("aria-pressed", String(transport.recording));
    // A count-in first: the song waits (the engine says when it plays).
    if (transport.recording && !transport.playing && !recordOptions.precount) {
      transport.playing = true;
      renderPlaying(true);
    }
    Hub.send("transport", { action: "record" });
  };

  Hub.commands.toggleLoop = () => {
    transport.loop = !transport.loop;
    $("loop").setAttribute("aria-pressed", String(transport.loop));
    Hub.send("transport", { loop: transport.loop });
    Hub.emit("transportEdited");
  };

  Hub.commands.toggleMetronome = () => {
    transport.metronome = !transport.metronome;
    $("metronome").setAttribute("aria-pressed", String(transport.metronome));
    Hub.send("transport", { metronome: transport.metronome });
  };

  /** Moves the song position, as when clicking in the ruler. */
  Hub.locate = (ppq) => {
    transport.ppq = Math.max(0, ppq);
    transport.seconds = Hub.tempoMap.secondsAt(transport.ppq);
    receivedAt = performance.now();
    renderTime();
    Hub.send("transport", { action: "locate", ppq: transport.ppq });
  };

  function sendTempo(bpm) {
    const value = Math.round(Hub.clamp(bpm, Hub.TempoMap.MIN_TEMPO, Hub.TempoMap.MAX_TEMPO) * 100) / 100;
    $("tempo").value = value.toFixed(2);
    transport.bpm = value;
    Hub.send("transport", { bpm: value });
    Hub.emit("transportEdited");
  }

  function tapTempo() {
    const now = performance.now();
    if (tapTimes.length && now - tapTimes[tapTimes.length - 1] > 2000) tapTimes.length = 0;
    tapTimes.push(now);
    if (tapTimes.length > 5) tapTimes.shift();
    if (tapTimes.length < 2) return;
    sendTempo(60000 * (tapTimes.length - 1) / (tapTimes[tapTimes.length - 1] - tapTimes[0]));
  }

  /** Sets the loop range in quarter notes; optionally switches looping on or off too. */
  Hub.setLoop = (start, end, enabled) => {
    if (!(end > start) || start < 0) return;
    transport.loopStart = start;
    transport.loopEnd = end;
    const message = { loopStart: start, loopEnd: end };

    if (enabled !== undefined) {
      transport.loop = enabled;
      message.loop = enabled;
      $("loop").setAttribute("aria-pressed", String(enabled));
    }

    renderLocators();
    Hub.send("transport", message);
    Hub.emit("transportEdited");
  };

  // L is where the loop starts, R where it ends. Anything that isn't a position, or an R
  // that isn't after L, puts the fields back as they were.
  function sendLocators() {
    const start = parseBars($("loop-start").value);
    const end = parseBars($("loop-end").value);

    if (start === null || end === null || end <= start) {
      $("loop-start").value = formatBars(transport.loopStart);
      $("loop-end").value = formatBars(transport.loopEnd);
      return;
    }

    Hub.setLoop(start, end);
  }

  Hub.on("ready", () => {
    for (let beats = 1; beats <= 32; beats++) $("sig-num").add(new Option(String(beats), String(beats)));
    for (const unit of DENOMINATORS) $("sig-den").add(new Option(String(unit), String(unit)));
    $("sig-num").value = "4";
    $("sig-den").value = "4";
    renderPlaying(false);
    renderTime();
    renderLocators();

    $("play").addEventListener("click", () => Hub.run("play"));
    $("stop").addEventListener("click", () => Hub.run("stop"));
    $("to-start").addEventListener("click", () => Hub.run("returnToZero"));
    $("bar-back").addEventListener("click", () => Hub.run("barBack"));
    $("bar-forward").addEventListener("click", () => Hub.run("barForward"));
    $("loop").addEventListener("click", () => Hub.run("toggleLoop"));
    $("record").addEventListener("click", () => Hub.run("toggleRecord"));
    for (const button of document.querySelectorAll(".rec-opt")) {
      button.addEventListener("click", () => setRecordOption({ [button.dataset.option]: !recordOptions[button.dataset.option] }));
      button.addEventListener("contextmenu", (event) => { event.preventDefault(); openRecordOptionsMenu(button); });
    }
    $("rec-options-menu").addEventListener("click", (event) => openRecordOptionsMenu(event.currentTarget));
    $("metronome").addEventListener("click", () => Hub.run("toggleMetronome"));
    $("tap").addEventListener("click", tapTempo);
    $("device").addEventListener("click", () => Hub.run("audioSetup"));

    $("time").addEventListener("click", () => {
      showSeconds = !showSeconds;
      renderTime();
    });

    $("metronome-volume").addEventListener("input", (event) => {
      event.target.style.setProperty("--fill", event.target.value + "%");
      Hub.send("transport", { metronomeVolume: Number(event.target.value) / 100 });
    });

    $("tempo").addEventListener("change", (event) => {
      const bpm = Number(event.target.value);
      sendTempo(Number.isFinite(bpm) && bpm > 0 ? bpm : transport.bpm);
    });

    for (const id of ["sig-num", "sig-den"]) {
      $(id).addEventListener("change", (event) => {
        event.target.blur();
        // Only the changed select is read: the other keeps the song's own value (never 0).
        if (id === "sig-num") transport.numerator = Number(event.target.value);
        else transport.denominator = Number(event.target.value);
        renderLocators();
        Hub.send("transport", { numerator: transport.numerator, denominator: transport.denominator });
        Hub.emit("transportEdited");
      });
    }

    $("loop-start").addEventListener("change", sendLocators);
    $("loop-end").addEventListener("change", sendLocators);

    // L and R show song positions; clicking one turns it into a bar-number field.
    for (const input of [$("loop-start"), $("loop-end")]) {
      const locator = input.closest(".locator");

      locator.querySelector(".locator-pos").addEventListener("click", (event) => {
        event.preventDefault();
        locator.classList.add("editing");
        input.focus();
        input.select();
      });

      input.addEventListener("keydown", (event) => {
        if (event.key === "Escape") {
          input.value = formatBars(input === $("loop-start") ? transport.loopStart : transport.loopEnd);
          input.blur();
        } else if (event.key === "Enter") {
          input.blur();
        }
      });

      input.addEventListener("blur", () => locator.classList.remove("editing"));
    }

    requestAnimationFrame(tick);
  });

  Hub.on("status", renderStatus);

  // The song's bars or tempo changed: the positions read differently now.
  Hub.on("tempoMap", () => {
    renderTime();
    renderLocators();
  });
})();
