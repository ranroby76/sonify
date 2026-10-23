// D:\workspace\Fanan_web_hub\src\hub-choir.js
//
// The Choir window: a choir track's voices (Choir.h), opened from the choir icon on its header.
// A choir track is an audio track whose clips (a vocal) sing as a choir as well, live as the song
// plays (Sonify reads the clips a little ahead, so the voices line up with the vocal exactly).
//   Lead voice    whether the vocal is a man's or a woman's (women's voices are made from a man's
//                 with their resonances raised, and the other way round)
//   Lead          the vocal itself in the mix (Off: the choir alone)
//   Choir, Spread, Hall    the voices together: their level, how wide, and their room
//   Timing, Pitch, Vibrato humanize: each singer a little late or early, a little off and drifting,
//                 with a vibrato of their own
//   Chords from   an instrument track whose notes give the chords (for the Chord parts)
//   A row per voice (up to 16): Man or Woman (switching moves its octave too), Character (darker
//   or brighter), the part (Scale: steps of the song's key from the note sung; Chord: chord notes
//   from it; Semitones: a fixed interval), Octave, Level, Pan, M(ute) and S(olo), and remove.
// Presets put in a whole choir, laid out around the lead's voice; Save keeps the choir as one of
// yours (on this computer). Changes are heard at once; they're saved with the song (they aren't
// Undo steps, as a plugin's knobs aren't).

(() => {
  "use strict";

  const { $, element } = Hub;
  const MAX_VOICES = 16;
  const LEAD_OFF = -60;
  const STORE = "sonify.choirPresets";

  // Who sings, relative to the lead: "same" or "other" gender, or a fixed one.
  const v = (g, mode, interval, octave, pan, level = 0) => ({ g, mode, interval, octave, pan, level });
  const PRESETS = [
    { name: "Double Track", timing: 0.6, pitch: 0.3, vibrato: 0, hall: 0.1, spread: 1, lead: 0,
      voices: [v("same", "semitones", 0, 0, -0.75, -3), v("same", "semitones", 0, 0, 0.75, -3)] },
    { name: "Wide Double (4 voices)", timing: 0.8, pitch: 0.4, vibrato: 0.1, hall: 0.15, spread: 1, lead: 0,
      voices: [v("same", "semitones", 0, 0, -1, -4), v("same", "semitones", 0, 0, -0.4, -4), v("same", "semitones", 0, 0, 0.4, -4), v("same", "semitones", 0, 0, 1, -4)] },
    { name: "Thirds", timing: 0.4, pitch: 0.3, vibrato: 0.15, hall: 0.2, spread: 0.8, lead: 0,
      voices: [v("same", "scale", 2, 0, -0.5), v("same", "scale", -2, 0, 0.5)] },
    { name: "Octaves", timing: 0.4, pitch: 0.3, vibrato: 0.1, hall: 0.2, spread: 0.7, lead: 0,
      voices: [v("same", "semitones", 0, 1, -0.4, -2), v("same", "semitones", 0, -1, 0.4, -2)] },
    { name: "Women's Trio", timing: 0.5, pitch: 0.4, vibrato: 0.2, hall: 0.25, spread: 0.8, lead: 0,
      voices: [v("female", "scale", 2, 0, -0.6), v("female", "scale", 0, 0, 0), v("female", "scale", -3, 0, 0.6)] },
    { name: "Men's Choir", timing: 0.5, pitch: 0.4, vibrato: 0.2, hall: 0.3, spread: 0.8, lead: 0,
      voices: [v("male", "scale", 2, 0, -0.6), v("male", "scale", 0, 0, -0.2), v("male", "scale", -3, 0, 0.2), v("male", "scale", 0, -1, 0.6, -2)] },
    { name: "Mixed Choir (SATB)", timing: 0.5, pitch: 0.4, vibrato: 0.2, hall: 0.3, spread: 0.8, lead: 0,
      voices: [v("female", "scale", 2, 0, -0.6), v("female", "scale", 0, 0, 0.6), v("male", "scale", 2, 0, -0.25), v("male", "scale", -3, 0, 0.25)] },
    { name: "Gospel (6 voices)", timing: 0.6, pitch: 0.4, vibrato: 0.25, hall: 0.3, spread: 0.9, lead: 0,
      voices: [v("female", "scale", 2, 0, -0.8), v("female", "scale", 0, 0, -0.5), v("female", "scale", -3, 0, -0.2),
               v("male", "scale", 2, 0, 0.2), v("male", "scale", 0, 0, 0.5), v("male", "scale", -3, 0, 0.8)] },
    { name: "Chord Pad (follows the chords)", timing: 0.5, pitch: 0.3, vibrato: 0.15, hall: 0.35, spread: 0.9, lead: 0,
      voices: [v("same", "chord", 1, 0, -0.6), v("same", "chord", 2, 0, 0.6), v("same", "chord", -1, 0, 0)] },
    { name: "Big Choir (12 voices)", timing: 0.7, pitch: 0.5, vibrato: 0.25, hall: 0.4, spread: 1, lead: -6,
      voices: [v("female", "scale", 2, 0, -1, -2), v("female", "scale", 2, 0, -0.6, -2), v("female", "scale", 0, 0, -0.3, -2),
               v("female", "scale", 0, 0, 0.3, -2), v("female", "scale", -3, 0, 0.6, -2), v("female", "scale", -3, 0, 1, -2),
               v("male", "scale", 2, 0, -0.9, -2), v("male", "scale", 2, 0, -0.5, -2), v("male", "scale", 0, 0, -0.15, -2),
               v("male", "scale", 0, 0, 0.15, -2), v("male", "scale", -3, 0, 0.5, -2), v("male", "scale", -3, 0, 0.9, -2)] }
  ];

  let trackId = 0;
  let state = null;              // the settings as the window shows them
  let sentJson = "";
  let sendTimer = 0;
  let sentAt = 0;
  let dragging = null;

  const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));
  const copy = (x) => JSON.parse(JSON.stringify(x));

  /** An octave a voice sits at for its gender against the lead's: a woman's an octave up from a
      man's lead, a man's an octave down from a woman's. */
  const octaveFor = (female, leadFemale) => (female && !leadFemale ? 1 : !female && leadFemale ? -1 : 0);

  function normalise(raw) {
    const s = Object.assign({ leadFemale: false, lead: 0, choir: 0, spread: 0.8, timing: 0.5, pitch: 0.4, vibrato: 0.2, hall: 0.25, chordTrack: 0, voices: [] }, raw || {});
    s.voices = (Array.isArray(s.voices) ? s.voices : []).slice(0, MAX_VOICES).map((voice) =>
      Object.assign({ female: false, character: 0, mode: "scale", interval: 0, octave: 0, level: 0, pan: 0, mute: false, solo: false }, voice));
    return s;
  }

  // ---------------------------------------------------------------- telling Sonify
  function send(now) {
    if (!state || !trackId) return;
    clearTimeout(sendTimer);
    const go = () => {
      sendTimer = 0;
      const json = JSON.stringify(state);
      if (json === sentJson) return;
      sentJson = json;
      sentAt = Date.now();
      Hub.send("choirSettings", { id: trackId, settings: copy(state) });
      const track = Hub.trackById(trackId);
      if (track) track.choir = copy(state);   // until Sonify sends the tracks again
    };
    if (now) go(); else sendTimer = setTimeout(go, 40);
  }

  // ---------------------------------------------------------------- presets
  function userPresets() {
    try {
      const kept = JSON.parse(localStorage.getItem(STORE) || "[]");
      return Array.isArray(kept) ? kept.filter((p) => p && typeof p.name === "string" && p.settings) : [];
    } catch (error) { return []; }
  }

  function keepUserPresets(list) {
    try { localStorage.setItem(STORE, JSON.stringify(list)); } catch (error) { Hub.toast("Presets can't be kept on this computer."); }
  }

  function applyPreset(preset) {
    const leadFemale = state.leadFemale;
    const voices = preset.voices.map((p, i) => {
      const female = p.g === "female" || (p.g === "same" && leadFemale) || (p.g === "other" && !leadFemale);
      return { female, character: 0, mode: p.mode, interval: p.interval, octave: clamp(p.octave + octaveFor(female, leadFemale), -2, 2),
               level: p.level || 0, pan: p.pan, mute: false, solo: false };
    });
    Object.assign(state, { lead: preset.lead, timing: preset.timing, pitch: preset.pitch, vibrato: preset.vibrato, hall: preset.hall, spread: preset.spread, voices });
    render();
    send(true);
  }

  function fillPresets() {
    const select = $("choir-preset");
    select.textContent = "";
    select.add(new Option("Presets\u2026", ""));
    const builtIn = element("optgroup");
    builtIn.label = "Sonify";
    PRESETS.forEach((preset, i) => builtIn.appendChild(new Option(preset.name, `b${i}`)));
    select.appendChild(builtIn);
    const mine = userPresets();
    if (mine.length) {
      const group = element("optgroup");
      group.label = "Yours";
      mine.forEach((preset, i) => group.appendChild(new Option(preset.name, `u${i}`)));
      select.appendChild(group);
    }
    select.value = "";
    $("choir-delete").hidden = true;
  }

  // ---------------------------------------------------------------- the window
  const pct = (x) => `${Math.round(x * 100)} %`;
  const db = (x) => (x <= LEAD_OFF ? "Off" : `${x > 0 ? "+" : ""}${Math.round(x * 10) / 10} dB`);

  const GLOBALS = [
    // id, key, min, max, step, scale (slider value -> setting), text
    ["lead", "lead", LEAD_OFF, 6, 0.5, 1, db, 0],
    ["choir", "choir", -36, 12, 0.5, 1, db, 0],
    ["spread", "spread", 0, 100, 1, 0.01, pct, 0.8],
    ["hall", "hall", 0, 100, 1, 0.01, pct, 0.25],
    ["timing", "timing", 0, 100, 1, 0.01, pct, 0.5],
    ["pitch", "pitch", 0, 100, 1, 0.01, pct, 0.4],
    ["vibrato", "vibrato", 0, 100, 1, 0.01, pct, 0.2]
  ];

  function intervalOptions(select, mode, value) {
    const signature = mode;
    if (select.dataset.mode !== signature) {
      select.dataset.mode = signature;
      select.textContent = "";
      if (mode === "semitones") {
        for (let i = 24; i >= -24; i--) select.add(new Option(i === 0 ? "Unison" : `${i > 0 ? "+" : ""}${i} semitone${Math.abs(i) === 1 ? "" : "s"}`, String(i)));
      } else if (mode === "chord") {
        for (let i = 4; i >= -4; i--) select.add(new Option(i === 0 ? "Unison" : `${Math.abs(i)} chord note${Math.abs(i) === 1 ? "" : "s"} ${i > 0 ? "above" : "below"}`, String(i)));
      } else {
        const names = ["Unison", "2nd", "3rd", "4th", "5th", "6th", "7th", "Octave"];
        for (let i = 7; i >= -7; i--) select.add(new Option(i === 0 ? names[0] : `${names[Math.abs(i)]} ${i > 0 ? "above" : "below"}`, String(i)));
      }
    }
    select.value = String(value);
  }

  const limitFor = (mode) => (mode === "semitones" ? 24 : mode === "chord" ? 4 : 7);

  function voiceRow(voice, index) {
    const row = element("div", "choir-voice");
    row.dataset.index = String(index);
    row.innerHTML = `
      <span class="choir-num">${index + 1}</span>
      <span class="choir-gender" role="group" aria-label="Voice">
        <button type="button" data-g="male" title="A man's voice">Man</button><button type="button" data-g="female" title="A woman's voice (switching moves its octave too)">Woman</button>
      </span>
      <input class="choir-character" type="range" min="-100" max="100" step="1" title="Character: darker (left) or brighter (right). Double-click: as it is" aria-label="Character">
      <select class="choir-mode" aria-label="Part" title="Scale: steps of the song's key from the note sung. Chord: chord notes from it (Chords from). Semitones: a fixed interval">
        <option value="scale">Scale</option><option value="chord">Chord</option><option value="semitones">Semitones</option>
      </select>
      <select class="choir-interval" aria-label="Interval"></select>
      <select class="choir-octave" aria-label="Octave">
        <option value="2">2 oct. up</option><option value="1">1 oct. up</option><option value="0">No octave</option><option value="-1">1 oct. down</option><option value="-2">2 oct. down</option>
      </select>
      <span class="choir-level"><input type="range" min="-36" max="6" step="0.5" aria-label="Level" title="Level (double-click: 0 dB)"><output></output></span>
      <input class="choir-pan" type="range" min="-100" max="100" step="1" aria-label="Pan" title="Pan (double-click: centre)">
      <button type="button" class="tbtn choir-mute" title="Mute this voice">M</button>
      <button type="button" class="tbtn choir-solo" title="Solo: hear this voice (and other soloed ones) alone">S</button>
      <button type="button" class="choir-remove" title="Remove this voice" aria-label="Remove">\u00d7</button>`;
    return row;
  }

  function updateRow(row, voice) {
    for (const button of row.querySelectorAll(".choir-gender button"))
      button.setAttribute("aria-pressed", String((button.dataset.g === "female") === voice.female));
    const character = row.querySelector(".choir-character");
    if (document.activeElement !== character) character.value = String(Math.round(voice.character * 100));
    row.querySelector(".choir-mode").value = voice.mode;
    intervalOptions(row.querySelector(".choir-interval"), voice.mode, voice.interval);
    row.querySelector(".choir-octave").value = String(voice.octave);
    const level = row.querySelector(".choir-level input");
    if (document.activeElement !== level) level.value = String(voice.level);
    row.querySelector(".choir-level output").textContent = db(voice.level);
    const pan = row.querySelector(".choir-pan");
    if (document.activeElement !== pan) pan.value = String(Math.round(voice.pan * 100));
    pan.title = `Pan: ${voice.pan === 0 ? "centre" : `${Math.round(Math.abs(voice.pan) * 100)} % ${voice.pan < 0 ? "left" : "right"}`} (double-click: centre)`;
    row.querySelector(".choir-mute").setAttribute("aria-pressed", String(voice.mute));
    row.querySelector(".choir-solo").setAttribute("aria-pressed", String(voice.solo));
  }

  function renderVoices() {
    const list = $("choir-voices");
    const rows = [...list.querySelectorAll(".choir-voice:not(.choir-voice-head)")];
    while (rows.length > state.voices.length) rows.pop().remove();
    while (rows.length < state.voices.length) {
      const row = voiceRow(state.voices[rows.length], rows.length);
      list.appendChild(row);
      rows.push(row);
    }
    rows.forEach((row, i) => { row.dataset.index = String(i); row.querySelector(".choir-num").textContent = String(i + 1); updateRow(row, state.voices[i]); });
    $("choir-add").disabled = state.voices.length >= MAX_VOICES;
    $("choir-count").textContent = `${state.voices.length} of ${MAX_VOICES} voices`;
    $("choir-empty").hidden = state.voices.length > 0;
  }

  function chordOptions() {
    const select = $("choir-chords");
    const tracks = Hub.state.tracks.filter((t) => t.kind === "instrument");
    const signature = tracks.map((t) => `${t.id}:${t.name}`).join("|");
    if (select.dataset.signature !== signature) {
      select.dataset.signature = signature;
      select.textContent = "";
      select.add(new Option("None (thirds of the key)", "0"));
      for (const t of tracks) select.add(new Option(`${t.number}. ${t.name}`, String(t.id)));
    }
    select.value = [...select.options].some((o) => o.value === String(state.chordTrack)) ? String(state.chordTrack) : "0";
  }

  function render() {
    if (!state) return;
    const track = Hub.trackById(trackId);
    $("choir-track").textContent = track ? track.name : "";
    $("choir-lead-gender").value = state.leadFemale ? "female" : "male";
    for (const [id, key, , , , scale, text] of GLOBALS) {
      const input = $(`choir-${id}`);
      if (document.activeElement !== input) input.value = String(state[key] / scale);
      $(`choir-${id}-value`).textContent = text(state[key]);
    }
    chordOptions();
    renderVoices();
  }

  function open(id) {
    const track = Hub.trackById(id);
    if (!track || !track.choir) return;
    trackId = id;
    state = normalise(copy(track.choir));
    sentJson = JSON.stringify(state);
    const panel = $("choir-window");
    const wasHidden = panel.hidden;
    panel.hidden = false;
    fillPresets();
    render();
    // Centred the first time; afterwards where it was left, kept on screen (the window may have
    // been made smaller meanwhile).
    const width = panel.offsetWidth || 900;
    if (wasHidden && !panel.dataset.placed) {
      panel.style.left = `${Math.max(8, Math.round((window.innerWidth - width) / 2))}px`;
      panel.style.top = `${Math.max(8, Math.round(window.innerHeight * 0.12))}px`;
      panel.style.right = "auto";
      panel.dataset.placed = "1";
    } else {
      keepOnScreen();
    }
  }

  function keepOnScreen() {
    const panel = $("choir-window");
    if (panel.hidden) return;
    const r = panel.getBoundingClientRect();
    panel.style.left = `${Math.round(clamp(r.left, 8, Math.max(8, window.innerWidth - r.width - 8)))}px`;
    panel.style.top = `${Math.round(clamp(r.top, 8, Math.max(8, window.innerHeight - r.height - 8)))}px`;
    panel.style.right = "auto";
  }

  function close() {
    send(true);
    const panel = $("choir-window");
    if (panel.contains(document.activeElement)) document.activeElement.blur();
    panel.hidden = true;
    trackId = 0;
    state = null;
  }

  const isOpen = () => !$("choir-window").hidden && state !== null;

  Hub.choir = { open, close, isOpen, presets: PRESETS, trackId: () => trackId, settings: () => (state ? copy(state) : null) };

  // ---------------------------------------------------------------- wiring
  function voiceAt(target) {
    const row = target.closest(".choir-voice");
    if (!row || row.classList.contains("choir-voice-head")) return null;
    const index = Number(row.dataset.index);
    return state && state.voices[index] ? { row, index, voice: state.voices[index] } : null;
  }

  Hub.on("ready", () => {
    const panel = $("choir-window");

    for (const [id, key, min, max, step, scale, text, reset] of GLOBALS) {
      const input = $(`choir-${id}`);
      Object.assign(input, { min: String(min), max: String(max), step: String(step) });
      const take = (final) => {
        if (!state) return;
        state[key] = clamp(Number(input.value) * scale, min * scale, max * scale);
        $(`choir-${id}-value`).textContent = text(state[key]);
        send(final);
      };
      input.addEventListener("input", () => take(false));
      input.addEventListener("change", () => take(true));
      input.addEventListener("dblclick", () => { if (!state) return; state[key] = reset; render(); send(true); });
    }

    $("choir-lead-gender").addEventListener("change", (event) => {
      if (!state) return;
      const leadFemale = event.target.value === "female";
      // The voices keep where they sit against the lead: a woman's an octave up from a man's lead...
      for (const voice of state.voices)
        voice.octave = clamp(voice.octave + octaveFor(voice.female, leadFemale) - octaveFor(voice.female, state.leadFemale), -2, 2);
      state.leadFemale = leadFemale;
      render();
      send(true);
    });

    $("choir-chords").addEventListener("change", (event) => {
      if (!state) return;
      state.chordTrack = Number(event.target.value) || 0;
      send(true);
    });

    $("choir-preset").addEventListener("change", (event) => {
      const value = event.target.value;
      if (!state || !value) { $("choir-delete").hidden = true; return; }
      if (value[0] === "b") {
        applyPreset(PRESETS[Number(value.slice(1))]);
        event.target.value = "";
        return;
      }
      const preset = userPresets()[Number(value.slice(1))];
      if (preset) {
        const chordTrack = state.chordTrack;
        state = normalise(copy(preset.settings));
        state.chordTrack = chordTrack;   // the chords stay this song's
        render();
        send(true);
      }
      $("choir-delete").hidden = false;
    });

    $("choir-delete").addEventListener("click", () => {
      const value = $("choir-preset").value;
      if (value[0] !== "u") return;
      const list = userPresets();
      list.splice(Number(value.slice(1)), 1);
      keepUserPresets(list);
      fillPresets();
    });

    $("choir-save").addEventListener("click", () => {
      const row = $("choir-save-row");
      row.hidden = !row.hidden;
      if (!row.hidden) { $("choir-save-name").value = ""; $("choir-save-name").focus(); }
    });

    const saveNamed = () => {
      const name = $("choir-save-name").value.trim().slice(0, 60);
      if (!name || !state) return;
      const list = userPresets().filter((p) => p.name !== name);
      const settings = copy(state);
      delete settings.chordTrack;
      list.push({ name, settings });
      list.sort((a, b) => a.name.localeCompare(b.name));
      keepUserPresets(list);
      fillPresets();
      $("choir-save-row").hidden = true;
      Hub.toast(`Saved the choir as "${name}".`);
    };
    $("choir-save-ok").addEventListener("click", saveNamed);
    $("choir-save-name").addEventListener("keydown", (event) => {
      if (event.key === "Enter") { event.preventDefault(); saveNamed(); }
      if (event.key === "Escape") { event.stopPropagation(); $("choir-save-row").hidden = true; }
    });

    $("choir-add").addEventListener("click", () => {
      if (!state || state.voices.length >= MAX_VOICES) return;
      const n = state.voices.length;
      const pan = n === 0 ? 0 : clamp((n % 2 ? 1 : -1) * (0.3 + 0.15 * Math.floor(n / 2)), -1, 1);
      state.voices.push({ female: state.leadFemale, character: 0, mode: "scale", interval: 0, octave: 0, level: 0, pan, mute: false, solo: false });
      render();
      send(true);
    });

    const list = $("choir-voices");
    list.addEventListener("click", (event) => {
      const at = voiceAt(event.target);
      if (!at) return;
      const { voice, index } = at;
      const gender = event.target.closest(".choir-gender button");
      if (gender) {
        const female = gender.dataset.g === "female";
        if (female !== voice.female) {
          voice.octave = clamp(voice.octave + octaveFor(female, state.leadFemale) - octaveFor(voice.female, state.leadFemale), -2, 2);
          voice.female = female;
        }
      } else if (event.target.closest(".choir-mute")) {
        voice.mute = !voice.mute;
      } else if (event.target.closest(".choir-solo")) {
        voice.solo = !voice.solo;
      } else if (event.target.closest(".choir-remove")) {
        state.voices.splice(index, 1);
      } else {
        return;
      }
      render();
      send(true);
    });

    list.addEventListener("change", (event) => {
      const at = voiceAt(event.target);
      if (!at) return;
      const { voice, row } = at;
      const target = event.target;
      if (target.classList.contains("choir-mode")) {
        voice.mode = target.value;
        voice.interval = clamp(voice.interval, -limitFor(voice.mode), limitFor(voice.mode));
      } else if (target.classList.contains("choir-interval")) {
        voice.interval = Number(target.value) || 0;
      } else if (target.classList.contains("choir-octave")) {
        voice.octave = clamp(Number(target.value) || 0, -2, 2);
      } else if (target.type !== "range") {
        return;
      }
      updateRow(row, voice);
      send(true);
    });

    list.addEventListener("input", (event) => {
      const at = voiceAt(event.target);
      if (!at || event.target.type !== "range") return;
      const { voice, row } = at;
      const target = event.target;
      if (target.classList.contains("choir-character")) voice.character = clamp(Number(target.value) / 100, -1, 1);
      else if (target.classList.contains("choir-pan")) voice.pan = clamp(Number(target.value) / 100, -1, 1);
      else if (target.closest(".choir-level")) voice.level = clamp(Number(target.value), -36, 6);
      updateRow(row, voice);
      send(false);
    });

    list.addEventListener("dblclick", (event) => {
      const at = voiceAt(event.target);
      if (!at || event.target.type !== "range") return;
      const target = event.target;
      if (target.classList.contains("choir-character")) at.voice.character = 0;
      else if (target.classList.contains("choir-pan")) at.voice.pan = 0;
      else if (target.closest(".choir-level")) at.voice.level = 0;
      updateRow(at.row, at.voice);
      send(true);
    });

    $("choir-close").addEventListener("click", close);
    panel.addEventListener("keydown", (event) => {
      if (event.key === "Escape") { event.stopPropagation(); close(); }
    });

    // Dragged by its title bar, kept on screen.
    const head = $("choir-head");
    head.addEventListener("pointerdown", (event) => {
      if (event.button !== 0 || event.target.closest("button, select, input")) return;
      const r = panel.getBoundingClientRect();
      dragging = { x: event.clientX - r.left, y: event.clientY - r.top, pointer: event.pointerId };
      head.setPointerCapture(event.pointerId);
      event.preventDefault();
    });
    head.addEventListener("pointermove", (event) => {
      if (!dragging || event.pointerId !== dragging.pointer) return;
      const width = panel.offsetWidth;
      panel.style.left = `${Math.round(clamp(event.clientX - dragging.x, 80 - width, window.innerWidth - 80))}px`;
      panel.style.top = `${Math.round(clamp(event.clientY - dragging.y, 0, window.innerHeight - 40))}px`;
      panel.style.right = "auto";
    });
    window.addEventListener("resize", keepOnScreen);
    const stopDrag = () => { dragging = null; };
    head.addEventListener("pointerup", stopDrag);
    head.addEventListener("lostpointercapture", stopDrag);
  });

  // The track gone, or no longer a choir: the window closes. Otherwise it follows what Sonify
  // has (an undo, a song opened), unless a change of its own is on its way.
  Hub.on("tracks", () => {
    if (!isOpen()) return;
    const track = Hub.trackById(trackId);
    if (!track || !track.choir) { close(); return; }
    if (sendTimer || Date.now() - sentAt < 600) { render(); return; }
    const theirs = JSON.stringify(normalise(copy(track.choir)));
    if (theirs !== sentJson && theirs !== JSON.stringify(state)) {
      state = normalise(copy(track.choir));
      sentJson = theirs;
    }
    render();
  });
})();
