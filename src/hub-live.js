// D:\workspace\Fanan_web_hub\src\hub-live.js
//
// The Live Show Studio: a DJ's decks and mixer with the singers and players on it too (LiveShow.h,
// MainLive.cpp). VirtualDJ is its model. While its tab shows (or it plays) it has Main, and the
// song stops; playing with its tab out of sight, the Studio is heard with it.
//   Its tabs, the way it's used: DJ show (the decks, the mixer, the library and the pads: tracks
//   dragged straight onto the decks), Live & podcasts (the live channels too, a playlist and its
//   one player, the show's screen) and Live streaming (the same, streamed). In the middle of the
//   tabs' line, the show's switches, pills alike: Enable streaming (red: the picture and the sound
//   go out to the stream; off, it shows the show's name), Stream settings (purple: its panel),
//   Controller (green: its window, Sonify's own: up to four cameras and the screen, each Main,
//   Custom or Hidden, presets and MIDI pads), Monitor (teal: the picture exactly as the viewers
//   see it, in a window over every other, never in the stream), then Mute mics and Mute playback
//   (amber; both muted as Sonify starts; Live & podcasts has these two). The rail on the left holds the rest the tab
//   needs: 2 or 4 decks, the screen and its HDMI outputs, REC and Stems, Podcast.
//   DJ show's 2 or 4 decks: each with its track's waveform (scrolling, its beats marked, and the
//   whole track under it: click it to jump there; drag the hot cues' and loops' markers along it),
//   Play, its tempo (the player's BPM: a track synced to its slider plays at it) and SYNC (the
//   slider follows the song's tempo), Jump to cue 1-4, four loops (the first button puts its
//   markers in, the second plays round it), the lock (markers stay put), Eject, and the track's own
//   tempo, Slider sync, its key and volume. A track keeps its key at any tempo; Key moves it by
//   semitones.
//   The playlist's tabs have one player, the mixer on its left: the same deck, but tracks never go
//   into it straight (no drop): they go on the playlist, from the library or dropped in from a
//   folder on the computer, and play from there (a second deck behind it crossfades the next one
//   in). The playlist: tracks and videos in order; each opens to show its options (its tempo, Sync
//   to player (the player's SYNC with it: the song's tempo), Key, Next (when the next comes:
//   seconds after its end, or below 0 a crossfade over its last seconds) and Volume). With Auto on
//   it plays itself; each one's round play button, and only that, puts it in the player at its
//   start, Play starts it (one playing stops, back at its start: the playlist goes on from the one
//   chosen); the one in the player is lit. A click on a banner picks it to edit (lit too, its own
//   way), a double-click opens its options. A track that ends, or is stopped, goes back to its start;
//   the player's back-to-start button (left of Play) takes it there too. Save and Open keep it in a
//   file of its own, with its tracks' hot cues and loops.
//   The show's screen (Screen, on the rail): the picture in picture, a window of Sonify's own over
//   every other window (Sonify minimised too; drag its bar, its corner sizes it): a video's picture,
//   or a track that's only sound as its name on black. It opens by itself when a video goes on the
//   playlist or comes into the player. HDMI puts it full screen on the computer's other screens (a
//   projector, a TV). The list emptied empties the player too; a track taken off the list is never
//   played (one playing plays on).
//   The markers: a hot cue's, and a loop's edges, drag on the waveform and on the whole track
//   under it; a loop's middle on the waveform moves it whole.
//   The library: Browse picks its folder (kept for next time).
//   The pads (DJ show): eight sounds (jingles, stings) on their own fader, never ducked. Drop a file
//   from the library on one; click plays it (again: stops it); right-click clears it.
//   The headphones (a Sub output of the song's: Song Setup's Audio I/O): the headphones button of a
//   deck or channel puts it there before its fader; Mix sets the cue against the show, Split puts
//   the cue left and the show right. Nothing cued: the show.
//   REC records the night (Stems: each deck and channel on its own file too); Podcast makes a
//   recording ready to publish (-16 LUFS, or -14 for streams and video).
//   MIDI Link: drop a chip on any knob, fader or button here and a controller moves it.
//   Stream settings: the show live, its title, what's playing and a logo, to YouTube, Facebook,
//   Twitch, TikTok or a server of your own, several at once, and a copy kept if wanted; the cameras
//   and the screen are the Controller's. TikTok (or a server marked 9:16) gets the vertical
//   picture, encoded beside the landscape one; TikTok takes its server URL and its stream key (its
//   LIVE Center gives both, the key new for each live). The panel shows the picture as it goes
//   out (the vertical one beside it while a vertical destination is ticked); its settings are this
//   computer's.
//   The mixer: a channel for each deck (Gain, the fader and its meter, and A, Thru or B on the
//   crossfader), the crossfader with its curve (DJ show), and the master (the limiter's light by its
//   name, lit while it pulls the sound down; FX, dark red, lit red while the effects channel's
//   window is open). The playlist's tabs: the player's one channel (its Gain a slider by its meter,
//   as the master's fader is), no crossfader. The song's tempo is the transport's, under the page.
//   The meters: green to 75% of the way to clipping, yellow to 90%, red from there to the top.
//   Live channels: the Studio's tracks for the singers and players (+ Add channel), each through its
//   own plugins, with its fader, pan and Mute.
//   The effects channel (its window floats: drag it by its head): its effects in tabs, in the order
//   the sound goes through them, each with its switch (the light on its tab): the dynamic EQ (an EQ
//   of four points on the music that makes room for the voices of the live channels it listens to;
//   its curve has as it is (the dashed line) in the middle, up to +25 dB at the top, silence at the
//   bottom, the green line the music as it's heard now (on the dashed line while nothing is
//   changed): each point's ring is its Gain, its triangle its Duck, as low as the voices take it; drag
//   either across for its frequency, up or down for its level; the wheel on one sets its width (Q);
//   the dot shows where it is now; the Level point turns the whole music down, talking over a bed;
//   each point's light switches it on or off), then on the master the compressor, the drive (soft
//   saturation), the stereo width (wider or narrower, the lows kept in the middle, with its
//   goniometer and correlation meter) and the limiter.
// Knobs and faders: drag (Shift for fine), the wheel, the arrow keys; double-click puts one back
// where it starts.

(() => {
  "use strict";

  const { $, element } = Hub;
  const DECK_COLOURS = ["#3fa9f5", "#ff8c42", "#b57bff", "#e8d44d"];
  const CHANNEL_COLOURS = { low: [255, 92, 64], mid: [92, 224, 138], high: [87, 184, 255] };
  const CUE_COLOURS = ["#ff5a5f", "#36d1a0", "#ffb340", "#b98bff"];
  const LOOP_COLOURS = ["#ff6ec7", "#5ce1e6", "#b6f36b", "#8d9cff"];
  const KEY_NAMES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];
  const MODES = ["dj", "live", "stream"];

  let live = { on: false, mode: "dj", onAir: false, micsMuted: true, playbackMuted: true, controllerOpen: false, monitorOpen: false, songTempo: 120, canKeepKey: true,
               specs: { deck: [], channel: [], master: [] }, master: {}, decks: [], channels: [], pads: [], playlist: [], playlistAuto: true, playlistFile: "",
               outputs: [], phonesOutput: 0, recording: false, stems: true, lastTake: "", podcastBusy: false, podcastFile: "",
               front: 0, screen: { displays: [], outputs: 0 } };
  let podcastProgress = -1;
  let streamOpen = false;
  let effectsOpen = false;                       // the effects channel's window
  let screenOpen = false;                        // the show's screen, the picture in picture (Sonify's window)
  let fileDragSent = 0;                          // when Sonify was last asked to take a drop from a folder
  let streamStats = null;
  let streamSaveTimer = 0;
  let previewData = "";
  let previewTallData = "";
  const specMaps = { deck: new Map(), channel: new Map(), master: new Map() };
  const waves = [null, null, null, null];        // { file, length, perSecond, data, overview }
  const motion = [0, 1, 2, 3].map(() => ({ position: 0, at: 0, playing: false, speed: 1, bpm: 0, loop: null }));
  const markersLocked = [false, false, false, false];
  const opened = new Set();                      // the playlist's items shown open
  let selectedItem = 0;                          // the playlist's item picked for editing (lit): a click on its banner
  let markerDrag = null;                         // { d, kind: "cue" | "loopStart" | "loopEnd" | "loopMove", index }
  const WAVE_SECONDS = 8;                        // what a deck's waveform shows across
  let padsPlaying = [];
  let listing = null;
  let query = "";
  let shown = false;
  let renderDue = true;
  let dragging = 0;
  const pending = new Map();
  let flushing = false;
  let frame = 0;

  // What the page keeps to update without drawing again.
  let deckViews = [];
  let padViews = [];
  let meterViews = new Map();  // key -> { levels, peaks, hold, holdAt }
  let duckerView = null;       // the dynamic EQ's curve, while its tab shows
  let effectsView = null;      // the effects window's meters (and the drive's curve)
  let refreshers = [];         // each control's own redraw, for values changed elsewhere on the page
  let noteTimer = 0;

  // ---------------------------------------------------------------- values
  const clamp01 = (v) => Math.max(0, Math.min(1, v));
  const isLog = (spec) => spec.unit === "Hz" || spec.unit === "ms" || /Q$/.test(spec.id) || spec.id === "compRatio";
  const centred = (spec) => spec.kind === "knob" && spec.min < 0 && spec.max > 0;

  function toPosition(spec, value) {
    if (!spec) return 0;
    if (spec.kind === "toggle") return value >= 0.5 ? 1 : 0;
    if (spec.kind === "choice") return spec.max > 0 ? clamp01(value / spec.max) : 0;
    if (spec.kind === "fader") return value <= spec.min + 0.01 ? 0 : clamp01(Math.pow(10, (value - spec.max) / 40));
    if (isLog(spec)) return clamp01(Math.log(value / spec.min) / Math.log(spec.max / spec.min));
    if (centred(spec)) return value < 0 ? 0.5 * (1 - value / spec.min) : 0.5 + 0.5 * value / spec.max;
    return clamp01((value - spec.min) / (spec.max - spec.min));
  }

  function fromPosition(spec, position) {
    const p = clamp01(position);
    if (spec.kind === "toggle") return p >= 0.5 ? 1 : 0;
    if (spec.kind === "choice") return Math.round(p * spec.max);
    if (spec.kind === "fader") return p <= 0.003 ? spec.min : Math.max(spec.min, Math.round((spec.max + 40 * Math.log10(p)) * 10) / 10);
    if (isLog(spec)) return spec.min * Math.pow(spec.max / spec.min, p);
    if (centred(spec)) {
      if (Math.abs(p - 0.5) < 0.012) return 0;   // the middle holds
      return p < 0.5 ? spec.min * (1 - p / 0.5) : spec.max * (p - 0.5) / 0.5;
    }
    if (spec.unit === "BPM") return Math.round((spec.min + (spec.max - spec.min) * p) * 100) / 100;
    return spec.min + (spec.max - spec.min) * p;
  }

  const trim = (t) => t.includes(".") ? t.replace(/0+$/, "").replace(/\.$/, "") : t;

  /** A track's volume (0..1) as dB: silence to as recorded in its lower half, up to +25 dB above. */
  function trackVolumeText(value) {
    if (value <= 0.001) return "\u2212\u221e dB";
    const db = value >= 0.5 ? 25 * (value - 0.5) / 0.5 : 40 * Math.log10(value / 0.5);
    const shown = Math.abs(db) < 0.05 ? 0 : db;
    return `${shown > 0 ? "+" : shown < 0 ? "\u2212" : ""}${Math.abs(shown).toFixed(1)} dB`;
  }

  function text(spec, value) {
    if (!spec) return "";
    if (spec.kind === "toggle") return value >= 0.5 ? "On" : "Off";
    if (spec.kind === "choice") return (spec.choices || [])[Math.round(value)] || "";
    if (spec.id === "trackVolume" || spec.unit === "vol") return trackVolumeText(value);
    if (spec.unit === "BPM") return `${formatBpm(value)} BPM`;
    if (spec.id === "pan") {
      const amount = Math.round(Math.abs(value) * 100);
      return amount === 0 ? "C" : (value < 0 ? "L" : "R") + amount;
    }
    if (spec.id === "widthBass" && value <= spec.min + 0.5) return "Off";
    if (spec.id === "driveTone") {
      if (Math.abs(value) < 0.5) return "Even";
      return `${value < 0 ? "Darker" : "Brighter"} ${Math.round(Math.abs(value))}%`;
    }
    if (spec.unit === "%") return `${Math.round(value)}%`;
    if (spec.id === "crossfader") return Math.abs(value) < 0.01 ? "Middle" : (value < 0 ? "A " : "B ") + Math.round(Math.abs(value) * 100) + "%";
    if (spec.id === "phonesMix") {
      if (value < 0.02) return "Cue";
      if (value > 0.98) return "Show";
      return `Cue ${Math.round((1 - value) * 100)} \u00b7 Show ${Math.round(value * 100)}`;
    }
    if (spec.unit === "dB") {
      if (spec.kind === "fader" && value <= spec.min + 0.01) return "\u2212\u221e dB";
      const shown = Math.abs(value) < 0.05 ? 0 : value;
      return (shown > 0 && spec.min < 0 ? "+" : "") + shown.toFixed(1) + " dB";
    }
    if (spec.unit === "Hz") return value >= 1000 ? trim((value / 1000).toFixed(2)) + " kHz" : Math.round(value) + " Hz";
    if (spec.unit === "ms") return (value < 10 ? trim(value.toFixed(1)) : String(Math.round(value))) + " ms";
    if (spec.unit === ":1") return trim(value.toFixed(1)) + ":1";
    return trim(value.toFixed(2));
  }

  /** A key's name ("Am", "Bb"), moved by shift semitones (round the octave either way); "" when
      it isn't known (tonic -1), however far it's moved. */
  const keyName = (tonic, minor, shift = 0) => (tonic === undefined || tonic === null || tonic < 0 ? ""
    : KEY_NAMES[(((tonic + shift) % 12) + 12) % 12] + (minor ? "m" : ""));
  const formatBpm = (bpm) => (bpm > 0 ? (Math.abs(bpm - Math.round(bpm)) < 0.005 ? String(Math.round(bpm)) : bpm.toFixed(2)) : "");
  const infoOf = (d) => (live.decks[d] || {}).info || null;
  const modeIs = (...modes) => modes.includes(live.mode);

  /** The playlist's tabs: one player (decks 1 and 2 by turns, the one behind only crossfading the
      next track in). */
  const single = () => modeIs("live", "stream");

  /** The deck the player shows, in the playlist's tabs: the one playing (the newer), or the one to play. */
  const frontDeck = () => (live.front === 1 ? 1 : 0);

  /** An input of the page's own being typed in: what Sonify sends waits for it. */
  const typing = () => {
    const active = document.activeElement;
    if (!active || !active.classList) return false;
    return active.classList.contains("ld-number") || (active.tagName === "INPUT" && Boolean(active.closest("#live-stream")));
  };

  /** A recording's length: "1:02:03" or "2:03". */
  const formatClock = (seconds) => {
    const s = Math.max(0, Math.floor(seconds));
    const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), r = s % 60;
    return h > 0 ? `${h}:${String(m).padStart(2, "0")}:${String(r).padStart(2, "0")}` : `${m}:${String(r).padStart(2, "0")}`;
  };

  const formatTime = (seconds) => {
    const s = Math.max(0, seconds);
    const minutes = Math.floor(s / 60);
    return `${minutes}:${(s - minutes * 60).toFixed(1).padStart(4, "0")}`;
  };

  // ---------------------------------------------------------------- settings
  /** Where a setting lives: "master", a deck's place, or a channel's track. */
  function valuesOf(target, index) {
    if (target === "master") return live.master;
    if (target === "deck") return (live.decks[index] || {}).values || null;
    const channel = live.channels.find((c) => c.id === index);
    return channel ? channel.values : null;
  }

  function valueOf(target, index, spec) {
    if (!spec) return 0;
    const values = valuesOf(target, index);
    return values && values[spec.id] !== undefined ? values[spec.id] : spec.default;
  }

  /** Sets a value here at once, and in Sonify at the next frame (a drag sends each frame's last). */
  function setValue(target, index, spec, value) {
    const values = valuesOf(target, index);
    if (values) values[spec.id] = value;
    pending.set(`${target}|${index}|${spec.id}`, { target, index, id: spec.id, value });
    if (flushing) return;
    flushing = true;
    requestAnimationFrame(() => {
      flushing = false;
      for (const change of pending.values()) {
        const message = { action: "set", target: change.target, id: change.id, value: change.value };
        if (change.target === "deck") message.deck = change.index;
        if (change.target === "channel") message.track = change.index;
        Hub.send("live", message);
      }
      pending.clear();
    });
  }

  const spec = (group, id) => specMaps[group].get(id);
  const deckCount = () => (live.master.decks >= 0.5 ? 4 : 2);
  const deckOn = (d, id) => valueOf("deck", d, spec("deck", id)) >= 0.5;

  // ---------------------------------------------------------------- MIDI Link
  /** A setting as MIDI Link names it: "live.deck.1.volume", "live.channel.2.pan", "live.master.crossfader".
      The playlist's tabs: the player is deck 1 (Sonify gives it the deck in front). */
  function linkKey(target, index, id) {
    if (target === "master") return `live.master.${id}`;
    if (target === "deck") return `live.deck.${single() ? 1 : index + 1}.${id}`;
    const n = live.channels.findIndex((c) => c.id === index) + 1;
    return n > 0 ? `live.channel.${n}.${id}` : "";
  }

  /** Marks a control for MIDI Link (hub-link.js finds it by this). */
  function linkable(el, key, continuous) {
    if (!key) return el;
    el.dataset.liveLink = key;
    el.dataset.continuous = continuous ? "1" : "0";
    return el;
  }

  /** A key as MIDI Link's window shows it: "Live Show \u00b7 Deck 1 \u00b7 Fader". */
  function targetText(key) {
    const parts = key.split(".");
    if (parts[0] !== "live") return key;
    if (parts[1] === "onair") return "Live Show \u00b7 Enable streaming";
    if (parts[1] === "mute") return "Live Show \u00b7 Mute mics and playback";
    if (parts[1] === "mutemics") return "Live Show \u00b7 Mute mics";
    if (parts[1] === "muteplayback") return "Live Show \u00b7 Mute playback";
    if (parts[1] === "record") return "Live Show \u00b7 REC";
    if (parts[1] === "pad") return `Live Show \u00b7 Pad ${parts[2]}`;
    if (parts[1] === "master") {
      const s = spec("master", parts[2]);
      return `Live Show \u00b7 ${s ? s.name : parts[2]}`;
    }
    let who = single() && Number(parts[2]) <= 2 ? "Player" : `Deck ${parts[2]}`;
    let s = spec("deck", parts[3]);
    if (parts[1] === "channel") {
      const channel = live.channels[Number(parts[2]) - 1];
      const track = channel ? trackOf(channel.id) : null;
      who = track ? track.name : `Channel ${parts[2]}`;
      s = spec("channel", parts[3]);
    }
    const action = parts[3] || "";
    const what = s ? s.name : action === "play" ? "Play" : action === "rewind" ? "Back to start" : action.startsWith("hotcue") ? `Jump to cue ${action.slice(6)}`
      : action.startsWith("loop") ? `Loop ${action.slice(4)}` : action;
    return `Live Show \u00b7 ${who} \u00b7 ${what}`;
  }

  /** A knob or fader (a controller's knob moves it); the rest are buttons. */
  function isContinuous(key) {
    const parts = key.split(".");
    const group = parts[1] === "master" ? "master" : parts[1] === "deck" ? "deck" : parts[1] === "channel" ? "channel" : "";
    const s = group ? spec(group, group === "master" ? parts[2] : parts[3]) : null;
    return Boolean(s && s.kind !== "toggle");
  }

  // ---------------------------------------------------------------- controls
  /** Pointer dragging for a knob or fader: up (or right) raises it; Shift is fine. */
  function draggable(el, getPosition, setPosition, travel, vertical = true, onStart = null) {
    let drag = null;
    el.addEventListener("pointerdown", (event) => {
      if (event.button !== 0) return;
      event.preventDefault();
      el.focus();
      el.setPointerCapture(event.pointerId);
      if (onStart) onStart();
      drag = { x: event.clientX, y: event.clientY, position: getPosition(), fine: event.shiftKey };
      dragging++;
      el.classList.add("held");
    });
    el.addEventListener("pointermove", (event) => {
      if (!drag) return;
      if (event.shiftKey !== drag.fine) drag = { x: event.clientX, y: event.clientY, position: getPosition(), fine: event.shiftKey };
      const delta = vertical ? drag.y - event.clientY : event.clientX - drag.x;
      setPosition(drag.position + delta / (typeof travel === "function" ? travel() : travel) * (drag.fine ? 0.2 : 1));
    });
    const end = () => {
      if (!drag) return;
      drag = null;
      dragging = Math.max(0, dragging - 1);
      el.classList.remove("held");
      if (renderDue && !dragging) render();
    };
    el.addEventListener("pointerup", end);
    el.addEventListener("pointercancel", end);
    el.addEventListener("lostpointercapture", end);
  }

  function keyed(el, getPosition, setPosition, reset, step = 0.02) {
    el.addEventListener("keydown", (event) => {
      const now = getPosition();
      const amount = event.shiftKey ? step / 5 : step;
      if (event.key === "ArrowUp" || event.key === "ArrowRight") setPosition(now + amount);
      else if (event.key === "ArrowDown" || event.key === "ArrowLeft") setPosition(now - amount);
      else if (event.key === "Delete" || event.key === "Backspace") reset();
      else return;
      event.preventDefault();
      event.stopPropagation();
    });
    el.addEventListener("wheel", (event) => {
      event.preventDefault();
      setPosition(getPosition() + (event.deltaY < 0 ? 1 : -1) * (event.shiftKey ? step / 5 : step));
    }, { passive: false });
  }

  /** A knob: its cap turns over 270 degrees; its label shows the value while it's touched. */
  function knob(target, index, specId, label, extraClass = "") {
    const s = spec(target === "deck" ? "deck" : target === "channel" ? "channel" : "master", specId);
    const holder = element("div", `lv-knob ${extraClass}`.trim());
    if (!s) return holder;
    holder.tabIndex = 0;
    holder.setAttribute("role", "slider");
    holder.setAttribute("aria-label", s.name);
    holder.innerHTML = '<svg viewBox="0 0 40 40" aria-hidden="true"><path class="lk-arc" d="M8.7 31.3A16 16 0 1 1 31.3 31.3"/>'
      + '<path class="lk-fill"/>' + (centred(s) || s.id === "trackVolume" || s.unit === "vol" ? '<circle class="lk-centre" cx="20" cy="3" r="1.3"/>' : "")
      + '<circle class="lk-cap" cx="20" cy="20" r="11.5"/><line class="lk-pointer" x1="20" y1="9.5" x2="20" y2="15"/></svg>';
    const caption = element("span", "lk-label", label || s.name);
    holder.appendChild(caption);
    linkable(holder, linkKey(target, index, specId), true);
    const pointer = holder.querySelector(".lk-pointer");
    const fill = holder.querySelector(".lk-fill");

    const arcPoint = (p) => {
      const a = (-135 + 270 * p) * Math.PI / 180;
      return [20 + 16 * Math.sin(a), 20 - 16 * Math.cos(a)];
    };

    const show = () => {
      const value = valueOf(target, index, s);
      const p = toPosition(s, value);
      pointer.setAttribute("transform", `rotate(${(-135 + 270 * p).toFixed(1)} 20 20)`);
      const from = centred(s) || s.id === "trackVolume" || s.unit === "vol" ? 0.5 : 0;
      const [x1, y1] = arcPoint(Math.min(from, p)), [x2, y2] = arcPoint(Math.max(from, p));
      const large = Math.abs(p - from) * 270 > 180 ? 1 : 0;
      fill.setAttribute("d", Math.abs(p - from) < 0.002 ? "" : `M${x1.toFixed(2)} ${y1.toFixed(2)}A16 16 0 ${large} 1 ${x2.toFixed(2)} ${y2.toFixed(2)}`);
      holder.title = `${s.name}: ${text(s, value)}`;
      holder.setAttribute("aria-valuetext", text(s, value));
      if (holder.classList.contains("reading")) caption.textContent = text(s, value);
    };

    const setPosition = (p) => { setValue(target, index, s, fromPosition(s, p)); show(); };
    const getPosition = () => toPosition(s, valueOf(target, index, s));
    draggable(holder, getPosition, setPosition, 150);
    keyed(holder, getPosition, setPosition, () => { setValue(target, index, s, s.default); show(); });
    holder.addEventListener("dblclick", () => { setValue(target, index, s, s.default); show(); });
    holder.addEventListener("pointerenter", () => { holder.classList.add("reading"); caption.textContent = text(s, valueOf(target, index, s)); });
    holder.addEventListener("pointerleave", () => { if (holder.classList.contains("held")) return; holder.classList.remove("reading"); caption.textContent = label || s.name; });
    holder.addEventListener("lostpointercapture", () => { if (!holder.matches(":hover")) { holder.classList.remove("reading"); caption.textContent = label || s.name; } });
    show();
    refreshers.push(show);
    return holder;
  }

  /** A fader (vertical, or across for the crossfader and the decks' tempo), with its value under
      or beside it. */
  /** A gain's scale with its 0 dB where a fader's is (the player's Gain beside the Master's
      fader: both at the same height there), its cut below that and its boost above. */
  function gainScale(s) {
    const master = spec("master", "volume");
    const zero = master ? toPosition(master, 0) : 0.75;
    return {
      to: (value) => value < 0 ? clamp01(zero * (1 - value / s.min)) : clamp01(zero + (1 - zero) * value / s.max),
      from: (position) => {
        const p = clamp01(position);
        if (Math.abs(p - zero) < 0.012) return 0;   // 0 dB holds
        return p < zero ? s.min * (1 - p / zero) : s.max * (p - zero) / (1 - zero);
      }
    };
  }

  function fader(target, index, specId, options = {}) {
    const s = spec(target === "deck" ? "deck" : target === "channel" ? "channel" : "master", specId);
    const across = Boolean(options.across);
    const holder = element("div", `lv-fader${across ? " across" : ""}`);
    if (!s) return holder;
    holder.tabIndex = 0;
    holder.setAttribute("role", "slider");
    holder.setAttribute("aria-label", options.label || s.name);
    holder.innerHTML = '<div class="lf-slot"></div><div class="lf-cap"></div>';
    linkable(holder, linkKey(target, index, specId), true);

    // Its spec's scale, or a gain's with its 0 dB where a fader's is.
    const scale = options.zeroAtFader ? gainScale(s) : null;
    const toPos = (value) => scale ? scale.to(value) : toPosition(s, value);
    const fromPos = (p) => scale ? scale.from(p) : fromPosition(s, p);
    if (s.kind === "fader" || options.zeroMark) {
      const zero = toPos(0);
      if (zero > 0 && zero < 1) holder.insertAdjacentHTML("afterbegin", `<span class="lf-zero" style="--at:${zero.toFixed(4)}"></span>`);
    }

    const show = () => {
      const value = valueOf(target, index, s);
      const shown = options.format ? options.format(value) : text(s, value);
      holder.style.setProperty("--pos", toPos(value).toFixed(4));
      holder.title = options.title ? options.title(value) : `${options.label || s.name}: ${shown}`;
      holder.setAttribute("aria-valuetext", shown);
      if (options.readout) options.readout.textContent = shown;
      if (options.onShow) options.onShow(value);
    };
    const reset = () => options.resetTo ? options.resetTo() : s.default;
    const setPosition = (p) => { setValue(target, index, s, fromPos(p)); show(); };
    const getPosition = () => toPos(valueOf(target, index, s));
    draggable(holder, getPosition, setPosition, () => Math.max(40, (across ? holder.clientWidth : holder.clientHeight) - 16), !across, options.onStart || null);
    keyed(holder, getPosition, (p) => { if (options.onStart) options.onStart(); setPosition(p); },
          () => { setValue(target, index, s, reset()); show(); }, options.step || 0.02);
    holder.addEventListener("dblclick", () => { if (options.onStart) options.onStart(); setValue(target, index, s, reset()); show(); });
    show();
    refreshers.push(show);
    return holder;
  }

  /** A lit switch. */
  function toggle(target, index, specId, label, extraClass = "", onChange = null) {
    const s = spec(target === "deck" ? "deck" : target === "channel" ? "channel" : "master", specId);
    const button = element("button", `lv-switch ${extraClass}`.trim(), label);
    button.type = "button";
    if (!s) return button;
    linkable(button, linkKey(target, index, specId), false);
    const show = () => {
      const on = valueOf(target, index, s) >= 0.5;
      button.setAttribute("aria-pressed", String(on));
      if (!button.dataset.keepTitle) button.title = `${s.name}: ${on ? "on" : "off"}`;
    };
    button.addEventListener("click", () => {
      setValue(target, index, s, valueOf(target, index, s) >= 0.5 ? 0 : 1);
      show();
      if (onChange) onChange();
    });
    show();
    refreshers.push(show);
    return button;
  }

  /** A choice as a row of small buttons ("A | Thru | B"). */
  function choices(target, index, specId, names, extraClass = "") {
    const s = spec(target === "deck" ? "deck" : target === "channel" ? "channel" : "master", specId);
    const row = element("div", `lv-choices ${extraClass}`.trim());
    if (!s) return row;
    row.setAttribute("role", "radiogroup");
    row.setAttribute("aria-label", s.name);
    linkable(row, linkKey(target, index, specId), true);
    const labels = names || s.choices || [];
    const buttons = labels.map((name, value) => {
      const button = element("button", "", name);
      button.type = "button";
      button.setAttribute("role", "radio");
      button.addEventListener("click", () => { setValue(target, index, s, value); show(); });
      row.appendChild(button);
      return button;
    });
    const show = () => {
      const now = Math.round(valueOf(target, index, s));
      buttons.forEach((button, value) => button.setAttribute("aria-checked", String(value === now)));
    };
    show();
    refreshers.push(show);
    return row;
  }

  /** A small number box: Enter (or leaving it changed) sends it; Escape puts it back. asTyped:
      sent too a moment after the last key (a number it can read). */
  function numberBox(className, label, onCommit, asTyped = false) {
    const input = element("input", `ld-number ${className}`.trim());
    input.type = "text";
    input.inputMode = "decimal";
    input.spellcheck = false;
    input.autocomplete = "off";
    input.setAttribute("aria-label", label);
    let before = "";
    let typedTimer = 0;
    const read = () => parseFloat(input.value.replace(",", ".").replace("\u2212", "-"));
    input.addEventListener("focus", () => { before = input.value; input.select(); });
    input.addEventListener("keydown", (event) => {
      if (event.key === "Enter") { event.preventDefault(); input.blur(); }
      else if (event.key === "Escape") { clearTimeout(typedTimer); input.value = before; input.blur(); }
      event.stopPropagation();   // the Studio's keys stay out of it
    });
    if (asTyped) {
      input.addEventListener("input", () => {
        clearTimeout(typedTimer);
        typedTimer = setTimeout(() => {
          const value = read();
          if (input.value.trim() === before.trim() || !Number.isFinite(value)) return;
          before = input.value;
          onCommit(value);
        }, 700);
      });
    }
    input.addEventListener("blur", () => {
      clearTimeout(typedTimer);
      const value = read();
      if (input.value.trim() !== before.trim() && Number.isFinite(value)) onCommit(value);
      else if (!Number.isFinite(value) || input.value.trim() !== before.trim()) input.value = before;
      setTimeout(() => { if (renderDue && !typing() && !dragging && shown) render(); }, 0);
    });
    return input;
  }

  /** A small button. */
  function smallButton(className, label, title, onClick) {
    const button = element("button", className, label);
    button.type = "button";
    if (title) button.title = title;
    if (onClick) button.addEventListener("click", onClick);
    return button;
  }

  /** A thin line between a panel's groups. */
  const separator = () => element("span", "ld-sep");

  /** A stereo meter, kept by its key for the status's levels. Its colours stay where they are on
      it (green to 75% of the way to clipping, yellow to 90%, red to the top: clipping), the level
      showing as much of them as it reaches. */
  function meter(key, extraClass = "") {
    const holder = element("div", `lv-meter ${extraClass}`.trim());
    holder.innerHTML = '<div class="lm-bar"><i></i><b></b></div><div class="lm-bar"><i></i><b></b></div>';
    const bars = [...holder.querySelectorAll(".lm-bar")];
    meterViews.set(key, { holder, levels: bars.map((bar) => bar.querySelector("i")), peaks: bars.map((bar) => bar.querySelector("b")), hold: [0, 0], holdAt: [0, 0] });
    return holder;
  }

  const meterPosition = (db) => clamp01((db + 48) / 48);   // -48 dBFS at the bottom .. 0 dBFS (clipping) at the top

  function feedMeter(key, values, now) {
    const view = meterViews.get(key);
    if (!view || !values) return;
    for (let side = 0; side < 2; side++) {
      const vu = meterPosition(values[side] + 3);           // a VU sits a little under the peaks
      const peak = meterPosition(values[2 + side]);
      const level = Math.max(vu, peak * 0.92);
      view.levels[side].style.clipPath = `inset(${((1 - level) * 100).toFixed(1)}% 0 0 0)`;
      if (peak >= view.hold[side] || now - view.holdAt[side] > 900) {
        view.hold[side] = peak;
        view.holdAt[side] = now;
      }
      view.peaks[side].style.bottom = `min(${(view.hold[side] * 100).toFixed(1)}%, calc(100% - 2px))`;   // at clipping: the top's own line
      view.peaks[side].classList.toggle("over", values[2 + side] > -1.2);
    }
  }

  // ---------------------------------------------------------------- the decks
  const sendDeck = (d, what, extra = {}) => Hub.send("live", Object.assign({ action: "deck", deck: d, do: what }, extra));
  const LOCK_ICON = '<svg viewBox="0 0 16 16" aria-hidden="true"><path class="lock-shackle" d="M5 7V5a3 3 0 0 1 6 0v2"/><rect x="3.5" y="7" width="9" height="7" rx="1.5"/></svg>';
  const LOOP_ICON = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M12.5 8a4.5 4.5 0 1 1-1.32-3.18"/><path class="fill" d="M13 2.2v3.6H9.4z"/></svg>';

  /** A deck's loop (seconds) when it's on: [start, end]; else null. */
  function loopOf(d, index) {
    const info = infoOf(d);
    const loop = info && info.loops ? info.loops[index] : null;
    return loop && loop[0] >= 0 && loop[1] > loop[0] ? loop : null;
  }

  const activeLoop = (d) => {
    const deck = live.decks[d] || {};
    return deck.activeLoop !== undefined ? deck.activeLoop : -1;
  };

  /** Moving a deck's tempo by hand: its track follows the slider, and the song's tempo leads it no more. */
  function takeTempo(d) {
    if (deckOn(d, "sync")) setValue("deck", d, spec("deck", "sync"), 0);
    if (!deckOn(d, "trackSync")) setValue("deck", d, spec("deck", "trackSync"), 1);
    showDeckInfo(d);
  }

  function deckView(d) {
    // The playlist's tabs: one player (whichever deck is in front), in the first deck's colour.
    const one = single();
    const colour = DECK_COLOURS[one ? 0 : d];
    const linkDeck = one ? 1 : d + 1;
    const root = element("div", `live-deck${one ? " player" : ""}`);
    root.style.setProperty("--deck", colour);
    root.dataset.deck = String(d);

    // The head: the track (a video: marked so), its key, its tempo now, and the time.
    const head = element("div", "ld-head");
    const badge = element("span", "ld-badge", String(d + 1));
    const title = element("span", "ld-title", "");
    const video = element("span", "ld-video", "VIDEO");
    video.title = "A video: its picture is on the show's screen";
    video.hidden = true;
    const key = element("span", "ld-key", "");
    const bpmNow = element("span", "ld-bpm-now", "");
    const time = element("span", "ld-time", "");
    head.append(badge, title, video, key, bpmNow, time);

    // The tempo: the player's BPM (its slider), and SYNC (the song's tempo leads it).
    const beatRow = element("div", "ld-beat");
    const tempo = element("div", "ld-tempo");
    const tempoReadout = element("span", "ld-tempo-text", "");
    const bpmSpec = spec("deck", "bpm");
    const tempoFader = fader("deck", d, "bpm", {
      across: true,
      label: "Tempo",
      readout: tempoReadout,
      step: bpmSpec ? 0.1 / (bpmSpec.max - bpmSpec.min) : 0.001,
      format: (value) => `${formatBpm(value) || "\u2013"} BPM`,
      title: (value) => deckOn(d, "trackSync")
        ? `Tempo: ${formatBpm(value)} BPM, the track plays at it. Double-click: the track's own`
        : `Tempo: ${formatBpm(value)} BPM. The track plays as recorded: move it and the track follows`,
      onStart: () => takeTempo(d),
      resetTo: () => {
        const info = infoOf(d);
        return info && info.bpm > 0 && bpmSpec ? Math.max(bpmSpec.min, Math.min(bpmSpec.max, info.bpm)) : bpmSpec ? bpmSpec.default : 120;
      }
    });
    tempo.append(tempoFader, tempoReadout);
    const sync = toggle("deck", d, "sync", "SYNC", "ld-sync", () => showDeckInfo(d));
    sync.dataset.keepTitle = "1";
    sync.title = "SYNC: the tempo slider follows the song's tempo (the DAW's), and the track with it. Off: it stays where it is";
    beatRow.append(tempo, sync);

    const wave = element("canvas", "ld-wave");
    const overview = element("canvas", "ld-overview");
    overview.title = "Click to jump there; drag a hot cue's or a loop's marker to move it";
    wave.title = "Drag a hot cue's marker, or a loop's edge (its length) or its middle (the whole loop)";

    // The panel under it: (the playlist's player) Back to its start | Play | Jump to cue | the loops |
    // (DJ show) the track's tempo, key and volume | Eject.
    const controls = element("div", "ld-controls");
    const play = smallButton("ld-play", "", "Play / Pause");
    play.innerHTML = '<svg viewBox="0 0 16 16" aria-hidden="true"><path class="ld-play-icon" d="M4 2.5v11l9-5.5z"/><path class="ld-pause-icon" d="M4 2.5h3v11H4zM9 2.5h3v11H9z"/></svg>';
    const rewind = one ? smallButton("ld-rewind", "", "Back to its start (playing, it plays on from there)", () => {
      motion[d].position = 0;
      motion[d].at = performance.now();
      sendDeck(d, "rewind");
    }) : null;
    if (rewind) {
      rewind.innerHTML = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M3 3h2.2v10H3zM13.5 3v10L6.2 8z"/></svg>';
      rewind.setAttribute("aria-label", "Back to its start");
    }

    const cues = element("div", "ld-group ld-cues");
    const cueCaption = element("span", "ld-caption");
    cueCaption.append(element("span", "ld-long", "Jump to cue"), element("span", "ld-short", "Cue"));
    cues.appendChild(cueCaption);
    const cueButtons = CUE_COLOURS.map((cueColour, index) => {
      const button = smallButton("ld-hotcue", String(index + 1), "", () => sendDeck(d, "hotcue", { index }));
      button.style.setProperty("--cue", cueColour);
      cues.appendChild(button);
      return button;
    });
    const lock = smallButton("ld-lock", "", "", () => {
      markersLocked[d] = !markersLocked[d];
      showDeckInfo(d);
    });
    lock.innerHTML = LOCK_ICON;
    cues.appendChild(lock);

    const loops = element("div", "ld-group ld-loops");
    loops.appendChild(element("span", "ld-caption ld-long", "Loops"));
    const loopViews = LOOP_COLOURS.map((loopColour, index) => {
      const pair = element("div", "ld-looppair");
      pair.style.setProperty("--loop", loopColour);
      const on = smallButton("ld-loop-on", `L${index + 1}`, "", () => sendDeck(d, "loopOn", { index, on: !loopOf(d, index) }));
      const go = smallButton("ld-loop-go", "", "", () => sendDeck(d, "loopPlay", { index, on: activeLoop(d) !== index }));
      go.innerHTML = LOOP_ICON;
      go.setAttribute("aria-label", `Loop ${index + 1}: play round it`);
      linkable(go, `live.deck.${linkDeck}.loop${index + 1}`, false);
      pair.append(on, go);
      loops.appendChild(pair);
      return { on, go };
    });

    // DJ show: the track's own tempo, Slider sync, its key and volume (else on the playlist's item).
    let bpm = null, keyValue = null;
    const track = element("div", "ld-group ld-track");
    if (modeIs("dj")) {
      bpm = numberBox("ld-bpm", `Deck ${d + 1}: its track's tempo`, (value) => sendDeck(d, "trackBpm", { bpm: value }));
      bpm.title = "The track's own tempo (BPM): Sonify hears it, or type it";
      const sliderSync = toggle("deck", d, "trackSync", "SLIDER SYNC", "ld-slidersync", () => showDeckInfo(d));
      sliderSync.dataset.keepTitle = "1";
      sliderSync.title = "Slider sync: the track plays at the deck's tempo (its slider). Off: as recorded";
      const keyBox = element("div", "ld-keybox");
      const keySpec = spec("deck", "key");
      keyValue = element("span", "ld-key-value", "");
      keyValue.title = "Key: up or down in semitones, to suit the singer (double-click: as recorded)";
      const moveKey = (by) => {
        if (!keySpec) return;
        const now = Math.round(valueOf("deck", d, keySpec));
        setValue("deck", d, keySpec, Math.max(keySpec.min, Math.min(keySpec.max, now + by)));
        showDeckInfo(d);
      };
      const keyDown = smallButton("ld-small ld-step", "\u2212", "Key: a semitone down", () => moveKey(-1));
      const keyUp = smallButton("ld-small ld-step", "+", "Key: a semitone up", () => moveKey(1));
      keyValue.addEventListener("dblclick", () => { if (keySpec) { setValue("deck", d, keySpec, 0); showDeckInfo(d); } });
      keyBox.append(keyDown, keyValue, keyUp);
      if (!live.canKeepKey) for (const button of [keyDown, keyUp]) button.disabled = true;
      track.append(element("span", "ld-caption", "BPM"), bpm, sliderSync, keyBox, knob("deck", d, "trackVolume", "Vol", "small ld-vol"));
    }

    const eject = smallButton("ld-eject", "", one ? "Eject the track (not while it plays)" : "Eject the track (a deck that's playing keeps it)",
                              () => Hub.send("live", { action: "eject", deck: d }));
    eject.innerHTML = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 3 2.5 9.5h11zM2.5 11h11v2h-11z"/></svg>';

    linkable(play, `live.deck.${linkDeck}.play`, false);
    if (rewind) linkable(rewind, `live.deck.${linkDeck}.rewind`, false);
    cueButtons.forEach((button, index) => linkable(button, `live.deck.${linkDeck}.hotcue${index + 1}`, false));
    if (rewind) controls.append(rewind);
    controls.append(play, separator(), cues, separator(), loops);
    if (modeIs("dj")) controls.append(separator(), track);
    controls.append(eject);
    root.append(head, beatRow, wave, overview, controls);

    play.addEventListener("click", () => {
      const playing = !motion[d].playing;
      sendDeck(d, playing ? "play" : "pause");
      if ((live.decks[d] || {}).title || !playing) {
        motion[d].position = positionOf(d, performance.now());
        motion[d].playing = playing;
        motion[d].at = performance.now();
        showDeckButtons(d);
      }
    });

    wireMarkers(d, wave, false);
    wireMarkers(d, overview, true);

    // DJ show: a track dropped on the deck, from the library. (The playlist's player takes none:
    // its tracks come from the playlist, in their order.)
    if (!one) {
      root.addEventListener("dragover", (event) => {
        const types = event.dataTransfer.types;
        if (!types.includes("text/sonify-audio") && !types.includes("text/sonify-item")) return;
        event.preventDefault();
        root.classList.add("drop");
      });
      root.addEventListener("dragleave", () => root.classList.remove("drop"));
      root.addEventListener("drop", (event) => {
        root.classList.remove("drop");
        const item = Number(event.dataTransfer.getData("text/sonify-item")) || 0;
        const path = item ? ((live.playlist.find((i) => i.id === item) || {}).path || "") : event.dataTransfer.getData("text/sonify-audio");
        if (!path) return;
        event.preventDefault();
        Hub.send("live", { action: "load", deck: d, path, item });
      });
    }

    const view = { root, title, video, time, wave, overview, play, rewind, tempoFader, tempoReadout, key, bpmNow, bpm, sync, cueButtons, lock, loopViews, keyValue };
    for (const canvas of [wave, overview]) observeSize(canvas);
    return view;
  }

  /** The hot cues' and loops' markers, dragged with the mouse: on the waveform (the track going by
      under the playhead: a loop's edges stretch it or shorten it, its middle moves it whole) and on
      the whole track under it (where a click anywhere else jumps there). The lock keeps them put. */
  function wireMarkers(d, canvas, whole) {
    // Where a time is across the canvas (CSS pixels), and the time at a place.
    const geometry = () => {
      const w = waves[d];
      const r = canvas.getBoundingClientRect();
      if (!w || !(w.length > 0) || !(r.width > 0)) return null;
      if (whole) return { r, w, toX: (t) => t / w.length * r.width, toT: (x) => clamp01(x / r.width) * w.length };
      const per = WAVE_SECONDS / r.width, position = positionOf(d, performance.now()), middle = r.width / 2;
      return { r, w, toX: (t) => middle + (t - position) / per, toT: (x) => Math.max(0, Math.min(w.length, position + (x - middle) * per)) };
    };

    const hitAt = (event) => {
      const g = geometry();
      const info = infoOf(d);
      if (!g || !info) return null;
      const x = event.clientX - g.r.left, y = event.clientY - g.r.top;
      let best = null, bestDistance = 8;
      const consider = (hit, distance) => { if (distance <= bestDistance) { best = hit; bestDistance = distance; } };
      // A hot cue: its line, or its flag at the top.
      if (!whole || y <= g.r.height * 0.6) {
        (info.cues || []).forEach((at, index) => {
          if (at < 0) return;
          const cx = g.toX(at);
          consider({ kind: "cue", index }, y <= 14 && x >= cx && x <= cx + 13 ? 0 : Math.abs(x - cx));
        });
      }
      // A loop's edges.
      if (!whole || y >= g.r.height * 0.35) {
        for (let index = 0; index < LOOP_COLOURS.length; index++) {
          const loop = loopOf(d, index);
          if (!loop) continue;
          consider({ kind: "loopStart", index }, Math.abs(x - g.toX(loop[0])));
          consider({ kind: "loopEnd", index }, Math.abs(x - g.toX(loop[1])));
        }
      }
      if (best || whole) return best;
      // The waveform: inside a loop, the whole loop.
      for (let index = LOOP_COLOURS.length - 1; index >= 0; index--) {
        const loop = loopOf(d, index);
        if (loop && x > g.toX(loop[0]) && x < g.toX(loop[1])) return { kind: "loopMove", index, grab: g.toT(x) - loop[0] };
      }
      return null;
    };

    const cursorFor = (hit) => {
      if (!hit) return whole ? "pointer" : "";
      if (markersLocked[d]) return "not-allowed";
      return hit.kind === "loopMove" ? "grab" : "ew-resize";
    };

    canvas.addEventListener("pointerdown", (event) => {
      const w = waves[d];
      if (!w || event.button !== 0) return;
      const hit = hitAt(event);
      if (hit && !markersLocked[d]) {
        event.preventDefault();
        canvas.setPointerCapture(event.pointerId);
        markerDrag = Object.assign({ d, canvas }, hit);
        canvas.style.cursor = hit.kind === "loopMove" ? "grabbing" : "ew-resize";
        dragging++;
        return;
      }
      if (!whole) return;
      const g = geometry();
      if (!g) return;
      const seconds = g.toT(event.clientX - g.r.left);
      motion[d].position = seconds;
      motion[d].at = performance.now();
      sendDeck(d, "seek", { seconds });
    });

    canvas.addEventListener("pointermove", (event) => {
      if (!markerDrag || markerDrag.canvas !== canvas) {
        canvas.style.cursor = cursorFor(hitAt(event));
        return;
      }
      const info = infoOf(d);
      const g = geometry();
      if (!info || !g) return;
      const seconds = g.toT(event.clientX - g.r.left);
      const length = g.w.length;
      if (markerDrag.kind === "cue") {
        info.cues[markerDrag.index] = Math.max(0, Math.min(length, seconds));
        return;
      }
      const loop = info.loops[markerDrag.index];
      if (!loop) return;
      if (markerDrag.kind === "loopStart") loop[0] = Math.max(0, Math.min(seconds, loop[1] - 0.05));
      else if (markerDrag.kind === "loopEnd") loop[1] = Math.min(length, Math.max(seconds, loop[0] + 0.05));
      else {
        const span = loop[1] - loop[0];
        const start = Math.max(0, Math.min(length - span, seconds - markerDrag.grab));
        loop[0] = start;
        loop[1] = start + span;
      }
    });

    const end = () => {
      if (!markerDrag || markerDrag.canvas !== canvas) return;
      const drag = markerDrag;
      markerDrag = null;
      dragging = Math.max(0, dragging - 1);
      canvas.style.cursor = "";
      const info = infoOf(d);
      if (info) {
        if (drag.kind === "cue") sendDeck(d, "cueAt", { index: drag.index, seconds: info.cues[drag.index] });
        else if (info.loops[drag.index]) sendDeck(d, "loopAt", { index: drag.index, start: info.loops[drag.index][0], end: info.loops[drag.index][1] });
      }
      if (renderDue && !dragging) render();
    };
    canvas.addEventListener("pointerup", end);
    canvas.addEventListener("pointercancel", end);
    canvas.addEventListener("lostpointercapture", end);
  }

  function showDeckButtons(d) {
    const view = deckViews[d];
    if (!view) return;
    view.play.classList.toggle("playing", motion[d].playing);
    view.play.setAttribute("aria-pressed", String(motion[d].playing));
  }

  /** How many semitones a deck's key is moved now: by Key only (its tempo never moves it). */
  function keyShift(d) {
    const keySpec = spec("deck", "key");
    if (!live.canKeepKey) return 12 * Math.log2(Math.max(0.01, motion[d].speed || 1));   // a record's way: the key goes with the tempo
    return keySpec ? Math.round(valueOf("deck", d, keySpec)) : 0;
  }

  function showDeckInfo(d) {
    const view = deckViews[d];
    if (!view) return;
    const deck = live.decks[d] || {};
    const info = deck.info || null;
    const hasTrack = Boolean(deck.title);
    const empty = single() ? "A track's play button on the playlist puts it here" : "Drop a track here";
    // A video being read out first (its sound), how far; else loading.
    const loading = deck.loading ? (deck.progress >= 0 ? `Preparing ${deck.loading}\u2026 ${Math.round(deck.progress * 100)}%` : `Loading ${deck.loading}\u2026`) : "";
    view.title.textContent = loading || deck.title || (deck.error ? deck.error : empty);
    view.title.classList.toggle("empty", !deck.title && !deck.loading);
    view.title.title = deck.file || "";
    view.video.hidden = !(single() && hasTrack && deck.video && !deck.loading);

    // Its key, as it's heard now.
    const shift = keyShift(d);
    const rounded = Math.round(shift);
    const own = info ? keyName(info.keyTonic, info.keyMinor) : "";
    view.key.textContent = own ? keyName(info.keyTonic, info.keyMinor, rounded) : "";
    view.key.classList.toggle("moved", Boolean(own) && rounded !== 0);
    view.key.title = own ? `Its key: ${own}${rounded !== 0 ? `, heard now as ${keyName(info.keyTonic, info.keyMinor, rounded)} (${rounded > 0 ? "+" : ""}${rounded} semitone${Math.abs(rounded) === 1 ? "" : "s"})` : ""}` : "";

    // The tempo: free (as recorded) unless the track follows the slider.
    const follows = deckOn(d, "trackSync");
    view.tempoFader.classList.toggle("free", !follows);
    view.root.classList.toggle("synced", deckOn(d, "sync"));

    // SYNC: in the playlist's tabs, a playlist's track has one Sync (its banner's Sync to player).
    view.sync.title = single() && deck.item > 0
      ? "SYNC: the track plays at the song's tempo (the DAW's); its playlist banner's Sync to player goes with it. Off: as recorded"
      : "SYNC: the tempo slider follows the song's tempo (the DAW's), and the track with it. Off: it stays where it is";

    // DJ show: the track's own tempo, and its key.
    if (view.bpm && document.activeElement !== view.bpm) {
      view.bpm.value = info ? formatBpm(info.bpm) : "";
      view.bpm.disabled = !hasTrack;
      view.bpm.placeholder = deck.listening ? "\u2026" : hasTrack ? "?" : "";
    }
    if (view.keyValue) {
      const keySpec = spec("deck", "key");
      const keyNow = keySpec ? Math.round(valueOf("deck", d, keySpec)) : 0;
      view.keyValue.textContent = keyNow === 0 ? "Key" : `${keyNow > 0 ? "+" : "\u2212"}${Math.abs(keyNow)}`;
      view.keyValue.classList.toggle("moved", keyNow !== 0);
    }

    if (view.rewind) view.rewind.disabled = !hasTrack;

    // Jump to cue, the lock, the loops.
    const cues = info ? info.cues || [] : [];
    view.cueButtons.forEach((button, index) => {
      const at = cues[index] !== undefined ? cues[index] : -1;
      button.disabled = !hasTrack || at < 0;
      button.title = at >= 0 ? `Jump to cue ${index + 1} (${formatTime(at)}). Drag its marker on the waveform, or on the whole track under it.` : `Cue ${index + 1}`;
    });
    view.lock.classList.toggle("locked", markersLocked[d]);
    view.lock.setAttribute("aria-pressed", String(markersLocked[d]));
    view.lock.title = markersLocked[d] ? "The markers are locked: click to move them again" : "Lock the hot cues' and loops' markers where they are";
    const playingLoop = activeLoop(d);
    view.loopViews.forEach(({ on, go }, index) => {
      const loop = loopOf(d, index);
      on.disabled = !hasTrack;
      go.disabled = !hasTrack;
      on.setAttribute("aria-pressed", String(Boolean(loop)));
      go.setAttribute("aria-pressed", String(playingLoop === index));
      on.title = loop ? `Loop ${index + 1}: ${formatTime(loop[0])} to ${formatTime(loop[1])} (drag its edges on the waveform, or its middle to move it). Click: off`
                      : `Loop ${index + 1}: on, its markers where the deck is (four beats)`;
      go.title = playingLoop === index ? `Loop ${index + 1} plays round: click to go on past it`
                                       : `Loop ${index + 1}: to its start at once, and round it until clicked again`;
    });
    showDeckMotion(d);
  }

  /** What changes as it plays: its tempo now, and the loop lit. */
  function showDeckMotion(d) {
    const view = deckViews[d];
    if (!view) return;
    const m = motion[d];
    const text = m.bpm > 0 ? `${formatBpm(m.bpm)} BPM` : "";
    if (view.bpmNow.textContent !== text) view.bpmNow.textContent = text;
    view.bpmNow.title = text ? `Its tempo now${deckOn(d, "trackSync") ? " (the slider's)" : " (as recorded)"}` : "";
    view.root.classList.toggle("looping", Boolean(m.loop));
  }

  const sizes = new WeakMap();
  const resizer = new ResizeObserver((entries) => {
    for (const entry of entries) {
      const canvas = entry.target;
      const ratio = window.devicePixelRatio || 1;
      const width = Math.max(1, Math.round(entry.contentRect.width * ratio));
      const height = Math.max(1, Math.round(entry.contentRect.height * ratio));
      if (canvas.width !== width || canvas.height !== height) {
        canvas.width = width;
        canvas.height = height;
      }
      sizes.set(canvas, { width, height });
    }
  });
  let observed = [];
  const observeSize = (canvas) => { resizer.observe(canvas); observed.push(canvas); };

  /** A point's colour from its low, middle and high: warm for bass, green for the middle, blue on top. */
  function pointColour(data, i, alpha) {
    const low = data[i + 1], mid = data[i + 2], high = data[i + 3];
    const sum = low + mid + high || 1;
    const mix = (c) => Math.round((low * CHANNEL_COLOURS.low[c] + mid * CHANNEL_COLOURS.mid[c] + high * CHANNEL_COLOURS.high[c]) / sum);
    return `rgba(${mix(0)},${mix(1)},${mix(2)},${alpha})`;
  }

  /** Where a deck is now: its last word from Sonify, carried on at its speed while it plays. */
  function positionOf(d, now) {
    const m = motion[d];
    const w = waves[d];
    let position = m.position + (m.playing ? (now - m.at) / 1000 * m.speed : 0);
    if (w) position = Math.min(position, w.length);
    return Math.max(0, position);
  }

  function drawWave(d, now) {
    const view = deckViews[d];
    const w = waves[d];
    if (!view) return;
    const canvas = view.wave;
    const g = canvas.getContext("2d");
    const { width, height } = canvas;
    g.clearRect(0, 0, width, height);
    const middle = height / 2;
    g.fillStyle = "rgba(255,255,255,0.05)";
    g.fillRect(0, middle, width, 1);
    if (!w || !w.data) return;

    const ratio = window.devicePixelRatio || 1;
    const position = positionOf(d, now);
    const perPixel = WAVE_SECONDS / width;
    const playhead = Math.round(width * 0.5);
    const data = w.data;
    const points = data.length / 4;
    const column = Math.max(1, Math.round(ratio));

    for (let x = 0; x < width; x += column) {
      const t = position + (x - playhead) * perPixel;
      if (t < 0 || t >= w.length) continue;
      const from = Math.floor(t * w.perSecond), to = Math.max(from + 1, Math.floor((t + perPixel * column) * w.perSecond));
      let peak = 0, at = from;
      for (let p = from; p < to && p < points; p++) {
        if (data[p * 4] >= peak) { peak = data[p * 4]; at = p; }
      }
      if (at >= points) continue;
      const half = peak / 255 * (middle - 2);
      g.fillStyle = pointColour(data, at * 4, x < playhead ? 0.55 : 0.95);
      g.fillRect(x, middle - half, column, Math.max(1, half * 2));
    }

    const toX = (t) => playhead + (t - position) / perPixel;
    const line = Math.max(1, Math.round(ratio));
    const info = infoOf(d);

    // The loops, shaded in their colours (the one playing stronger).
    const playingLoop = activeLoop(d);
    for (let index = 0; index < LOOP_COLOURS.length; index++) {
      const loop = loopOf(d, index);
      if (!loop) continue;
      const x1 = Math.max(0, toX(loop[0])), x2 = Math.min(width, toX(loop[1]));
      if (x2 <= x1) continue;
      g.fillStyle = LOOP_COLOURS[index];
      g.globalAlpha = playingLoop === index ? 0.22 : 0.1;
      g.fillRect(x1, 0, x2 - x1, height);
      g.globalAlpha = 1;
      for (const edge of loop) {
        const x = Math.round(toX(edge));
        if (x >= 0 && x <= width) g.fillRect(x - line, 0, 2 * line, height);
      }
    }

    // The beats: a tick at each, a line through each bar's first.
    if (info && info.bpm > 0) {
      const beat = 60 / info.bpm;
      const first = Math.ceil((position - playhead * perPixel - info.firstBeat) / beat);
      const last = Math.floor((position + (width - playhead) * perPixel - info.firstBeat) / beat);
      const tick = Math.round(height * 0.16);
      if (last - first < width / 4) {
        for (let k = first; k <= last; k++) {
          const t = info.firstBeat + k * beat;
          if (t < 0 || t > w.length) continue;
          const x = Math.round(toX(t));
          if (((k % 4) + 4) % 4 === 0) {
            g.fillStyle = "rgba(255,255,255,0.5)";
            g.fillRect(x, 0, line, height);
          } else {
            g.fillStyle = "rgba(255,255,255,0.3)";
            g.fillRect(x, 0, line, tick);
            g.fillRect(x, height - tick, line, tick);
          }
        }
      }
    }

    // The hot cues: their colour and number.
    if (info && info.cues) {
      g.font = `700 ${Math.round(9 * ratio)}px Segoe UI, sans-serif`;
      info.cues.forEach((at, index) => {
        if (at < 0) return;
        const x = Math.round(toX(at));
        if (x < -20 * ratio || x > width) return;
        const size = Math.round(11 * ratio);
        g.fillStyle = CUE_COLOURS[index];
        g.fillRect(x, 0, line, height);
        g.fillRect(x, 0, size, size);
        g.fillStyle = "#0d0e10";
        g.fillText(String(index + 1), x + Math.round(3 * ratio), Math.round(9 * ratio));
      });
    }

    g.fillStyle = "#ffffff";
    g.fillRect(playhead - Math.round(ratio / 2), 0, Math.max(1, Math.round(ratio)), height);
  }

  /** The whole track, drawn once for its size, then the playhead and the markers over it each frame. */
  function drawOverview(d, now) {
    const view = deckViews[d];
    const w = waves[d];
    if (!view) return;
    const canvas = view.overview;
    const g = canvas.getContext("2d");
    const { width, height } = canvas;
    g.clearRect(0, 0, width, height);
    if (!w || !w.data) return;

    const key = `${width}x${height}|${w.file}`;
    if (!w.overview || w.overviewKey !== key) {
      const art = document.createElement("canvas");
      art.width = width;
      art.height = height;
      const a = art.getContext("2d");
      const data = w.data;
      const points = data.length / 4;
      const per = points / width;
      for (let x = 0; x < width; x++) {
        const from = Math.floor(x * per), to = Math.max(from + 1, Math.floor((x + 1) * per));
        let peak = 0, at = from;
        for (let p = from; p < to && p < points; p++) if (data[p * 4] >= peak) { peak = data[p * 4]; at = p; }
        if (at >= points) continue;
        const h = peak / 255 * (height - 2);
        a.fillStyle = pointColour(data, at * 4, 0.9);
        a.fillRect(x, height - 1 - h, 1, h);
      }
      w.overview = art;
      w.overviewKey = key;
    }
    g.drawImage(w.overview, 0, 0);

    const ratio = window.devicePixelRatio || 1;
    const position = positionOf(d, now);
    const x = w.length > 0 ? position / w.length * width : 0;
    g.fillStyle = "rgba(0,0,0,0.45)";
    g.fillRect(0, 0, x, height);   // what's played, darker
    if (!(w.length > 0)) return;
    const toX = (seconds) => Math.round(seconds / w.length * width);

    // The loops: shaded between their markers, a handle at each end (below).
    const playingLoop = activeLoop(d);
    const handle = Math.round(6 * ratio);
    for (let index = 0; index < LOOP_COLOURS.length; index++) {
      const loop = loopOf(d, index);
      if (!loop) continue;
      const x1 = toX(loop[0]), x2 = toX(loop[1]);
      g.fillStyle = LOOP_COLOURS[index];
      g.globalAlpha = playingLoop === index ? 0.4 : 0.2;
      g.fillRect(x1, 0, Math.max(2, x2 - x1), height);
      g.globalAlpha = 1;
      g.fillRect(x1, 0, Math.max(1, Math.round(ratio)), height);
      g.fillRect(x2 - Math.max(1, Math.round(ratio)), 0, Math.max(1, Math.round(ratio)), height);
      g.beginPath();
      g.moveTo(x1, height); g.lineTo(x1 + handle, height); g.lineTo(x1, height - handle); g.closePath();
      g.moveTo(x2, height); g.lineTo(x2 - handle, height); g.lineTo(x2, height - handle); g.closePath();
      g.fill();
    }

    // The hot cues: a line and a flag with its number.
    const info = infoOf(d);
    if (info && info.cues) {
      const flag = Math.round(9 * ratio);
      g.font = `700 ${Math.round(8 * ratio)}px Segoe UI, sans-serif`;
      info.cues.forEach((at, index) => {
        if (at < 0) return;
        const cx = toX(at);
        g.fillStyle = CUE_COLOURS[index];
        g.fillRect(cx, 0, Math.max(1, Math.round(ratio)), height);
        g.fillRect(cx, 0, flag, flag);
        g.fillStyle = "#0d0e10";
        g.fillText(String(index + 1), cx + Math.round(2 * ratio), Math.round(7.5 * ratio));
      });
    }
    g.fillStyle = "#ffffff";
    g.fillRect(Math.round(x), 0, Math.max(1, Math.round(ratio)), height);
  }

  function animate() {
    frame = 0;
    if (!shown) return;
    frame = requestAnimationFrame(animate);
    const now = performance.now();
    for (let d = 0; d < deckViews.length; d++) {
      drawWave(d, now);
      drawOverview(d, now);
      const w = waves[d];
      const position = positionOf(d, now);
      if (deckViews[d]) deckViews[d].time.textContent = w ? `${formatTime(position)}  \u2212${formatTime(w.length - position)}` : "";
    }
    if (duckerView && effectsOpen && specMaps.master.size) drawDucker();
    if (effectsView && specMaps.master.size) {
      if (effectsView.driveCurve) drawDriveCurve();
      if (effectsView.compressorCurve) drawCompressorCurve();
      if (effectsView.history) drawLimiterHistory(now);
      if (effectsView.goniometer) drawGoniometer();
    }
  }

  // ---------------------------------------------------------------- the mixer
  /** A deck's channel. The playlist's tabs: the player's one (deck 1's: deck 2 plays through it),
      its Gain as a slider beside its meter, as the master's fader is (the master's is the one
      fader; their 0 dB at the same height), and no place on the crossfader (there's none). */
  function mixerStrip(d) {
    const one = single();
    const strip = element("div", `lm-strip${one ? " player" : ""}`);
    strip.style.setProperty("--deck", DECK_COLOURS[d]);
    const head = element("div", "lm-strip-head");
    head.append(element("span", "lm-name", one ? "Player" : String(d + 1)), phonesButton("deck", d));
    strip.appendChild(head);
    const row = element("div", "lm-fader-row");
    const readout = element("span", "lm-readout");
    if (one) {
      const level = meter(`deck${d}`, "wide");
      level.title = "The player's level, after its Gain (the master's fader sets how loud it all goes out)";
      row.append(fader("deck", d, "gain", { readout, label: "Player gain", zeroMark: true, zeroAtFader: true }), level);
      strip.append(row, readout);
      return strip;
    }
    const knobs = element("div", "lm-knobs");
    knobs.append(knob("deck", d, "gain", "Gain"));
    strip.appendChild(knobs);
    row.append(fader("deck", d, "volume", { readout, label: `Deck ${d + 1} fader` }), meter(`deck${d}`));
    strip.append(row, readout);
    strip.appendChild(choices("deck", d, "assign", ["A", "Thru", "B"], "lm-assign"));
    return strip;
  }

  const PHONES_ICON = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.2A5.8 5.8 0 0 0 2.2 8v4.3c0 .8.6 1.5 1.5 1.5H5V9.3H3.7V8a4.3 4.3 0 0 1 8.6 0v1.3H11v4.5h1.3c.9 0 1.5-.7 1.5-1.5V8A5.8 5.8 0 0 0 8 2.2z"/></svg>';

  /** A deck's or channel's headphones button: heard there before its fader. */
  function phonesButton(target, index) {
    const button = toggle(target, index, "phones", "", "lv-phones");
    button.insertAdjacentHTML("afterbegin", PHONES_ICON);
    button.title = "Headphones: heard there before its fader (cue the next one while the room hears the show)";
    button.setAttribute("aria-label", "Headphones");
    return button;
  }

  function renderMixer() {
    const mixer = $("live-mixer");
    mixer.textContent = "";
    const one = single();
    const strips = element("div", "lm-strips");
    if (one) strips.appendChild(mixerStrip(0));
    else for (let d = 0; d < deckCount(); d++) strips.appendChild(mixerStrip(d));

    // The master: the limiter's light by its name, its fader and meter.
    const master = element("div", "lm-master");
    const masterHead = element("div", "lm-master-head");
    const led = element("span", "lm-led");
    led.id = "live-limit-led";
    led.title = "The limiter: lit while it pulls the sound down";
    masterHead.append(element("span", "lm-name", "Master"), led);
    master.appendChild(masterHead);
    const row = element("div", "lm-fader-row");
    const readout = element("span", "lm-readout");
    row.append(fader("master", 0, "volume", { readout, label: "Master" }), meter("master", "wide"));
    master.append(row, readout);
    strips.appendChild(master);

    // FX, under the faders, across: the effects channel's window (dark red; lit red while it's open).
    const fxRow = element("div", "lm-fx-row");
    const fx = smallButton("lm-fx", "FX", "The effects channel: the dynamic EQ, the compressor, the drive, the stereo width and the limiter. Click: its window (lit while it's open)",
      () => openEffects(!effectsOpen));
    fx.id = "live-effects-button";
    fx.setAttribute("aria-haspopup", "dialog");
    fx.setAttribute("aria-expanded", String(effectsOpen));
    fxRow.appendChild(fx);

    // DJ show: the crossfader, under it all, and its curve. (The playlist's one player has none.)
    const cross = element("div", "lm-cross");
    const curve = element("div", "lm-curve");
    if (!one) {
      const crossReadout = element("span", "lm-cross-text");
      cross.append(element("span", "lm-cross-side", "A"), fader("master", 0, "crossfader", { across: true, label: "Crossfader", readout: crossReadout }),
                   element("span", "lm-cross-side", "B"));
      curve.append(crossReadout, choices("master", 0, "curve", null, "lm-curve-choice"));
    }

    // The headphones: their output, the cue against the show, their level, Split.
    const phones = element("div", "lm-phones");
    const head = element("div", "lm-phones-head");
    const icon = element("span", "lm-phones-icon");
    icon.innerHTML = PHONES_ICON;
    const output = element("select", "lm-phones-output");
    output.title = "Where the headphones play: a Sub output of the song's (Song Setup > Audio I/O: your controller's outputs 3-4, say)";
    output.appendChild(element("option", "", live.outputs.length ? "Off" : "No Sub output"));
    output.options[0].value = "0";
    for (const sub of live.outputs) {
      const option = element("option", "", sub.name);
      option.value = String(sub.id);
      output.appendChild(option);
    }
    output.value = String(live.phonesOutput || 0);
    output.disabled = !live.outputs.length;
    output.addEventListener("change", () => Hub.send("live", { action: "phones", output: Number(output.value) }));
    head.append(icon, output, knob("master", 0, "phonesMix", "Mix", "small"), knob("master", 0, "phonesLevel", "Level", "small"),
                toggle("master", 0, "phonesSplit", "SPLIT", "lm-split"));
    phones.append(head);
    phones.classList.toggle("off", !live.phonesOutput);
    mixer.append(strips);
    if (!one) mixer.append(cross, curve);
    mixer.append(fxRow, phones);
  }

  // ---------------------------------------------------------------- live channels
  function trackOf(id) {
    return (Hub.state.tracks || []).find((t) => t.id === id) || null;
  }

  let channelRefreshers = new Set();             // the live channels' strips' redraws

  function renderChannels() {
    const box = $("live-channels");
    box.textContent = "";
    // The last strips' redraws go with them.
    refreshers = refreshers.filter((refresh) => !channelRefreshers.has(refresh));
    const firstRefresher = refreshers.length;
    for (const channel of live.channels) {
      const track = trackOf(channel.id);
      const strip = element("div", "lc-strip");
      strip.style.setProperty("--track", track ? track.colour : "#888");
      const name = element("span", "lc-name", track ? track.name : `Track ${channel.id}`);
      name.title = track ? `${track.name}: its ${track.kind === "audio" ? "input" : "live notes"} through its plugins in the Studio` : "";
      const remove = element("button", "lc-remove", "\u00d7");
      remove.type = "button";
      remove.title = "Take it off the show (the track stays in the Studio)";
      remove.addEventListener("click", () => Hub.send("live", { action: "channel", track: channel.id, on: false }));
      const head = element("div", "lc-head");
      head.append(name, phonesButton("channel", channel.id), remove);
      strip.appendChild(head);
      if (!channel.hasInput) strip.appendChild(element("span", "lc-warning", "No input"));
      const row = element("div", "lm-fader-row");
      const readout = element("span", "lm-readout");
      row.append(fader("channel", channel.id, "volume", { readout, label: `${track ? track.name : "Channel"} fader` }), meter(`ch${channel.id}`));
      const buttons = element("div", "lc-buttons");
      buttons.append(toggle("channel", channel.id, "mute", "M", "mute"));
      strip.append(knob("channel", channel.id, "pan", "Pan"), row, readout, buttons);
      box.appendChild(strip);
    }
    channelRefreshers = new Set(refreshers.slice(firstRefresher));
    if (!live.channels.length) {
      box.appendChild(element("p", "live-empty",
        "No live channels yet. Add the Studio's tracks for the singers and players: an audio track on each mic's input, or an instrument track for a keyboard."));
    }
  }

  function addChannelMenu(anchor) {
    const taken = new Set(live.channels.map((c) => c.id));
    const items = (Hub.state.tracks || []).filter((t) => (t.kind === "audio" || t.kind === "instrument") && !taken.has(t.id))
      .map((t) => ({ label: `${t.number}. ${t.name}${t.kind === "audio" ? "" : " (instrument)"}`, value: t.id }));
    if (!items.length) {
      Hub.toast("Every track is on the show already, or there are none: add an audio track for each mic in the Studio.");
      return;
    }
    Hub.popupMenu(anchor, [{ group: "A track for a singer or player" }, ...items], (id) => {
      if (id !== undefined) Hub.send("live", { action: "channel", track: id, on: true });
    });
  }

  // ---------------------------------------------------------------- the effects channel
  /** The effects channel's effects, in the order the sound goes through them: the dynamic EQ on the
      music (the decks), then on the master the compressor, the drive, the stereo width and the
      limiter. Each has its switch (its setting "on"). */
  const EFFECTS = [
    { id: "dynamicEq", name: "Dynamic EQ", on: "duck", colour: "#46c27a",
      about: "On the music: it makes room for the voices of the live channels it listens to" },
    { id: "compressor", name: "Compressor", on: "compOn", colour: "#e0a54a",
      about: "On the master, after its fader" },
    { id: "drive", name: "Drive", on: "driveOn", colour: "#ff8c42",
      about: "Soft saturation: the peaks stay where they were, the rest comes up, rounder" },
    { id: "width", name: "Stereo Width", on: "widthOn", colour: "#5cc8ff",
      about: "The stereo image wider or narrower (mid and side): Bass Mono keeps the lows in the middle, firm on any PA" },
    { id: "limiter", name: "Limiter", on: "limOn", colour: "#ff5a63",
      about: "Gain, then a peak limiter that never looks ahead (nothing is delayed): while it's on, nothing goes past its Ceiling" }
  ];
  const EFFECTS_KEPT = "sonify.live.effects";    // the window's place and the tab it showed, in this browser
  let effectsTab = "limiter";
  let effectsCanvases = [];                      // the page's canvases (watched for their size)
  let effectsRefreshers = new Set();             // the page's controls' redraws

  function effectsKept() {
    try {
      const kept = JSON.parse(localStorage.getItem(EFFECTS_KEPT) || "null");
      if (kept && typeof kept === "object") return kept;
    } catch (e) { /* nothing kept here */ }
    return {};
  }

  function keepEffects(changes) {
    try {
      localStorage.setItem(EFFECTS_KEPT, JSON.stringify(Object.assign(effectsKept(), changes)));
    } catch (e) { /* not kept: it opens where it opens */ }
  }

  /** The dynamic EQ's points: four bells (each with its Gain and its Duck, levels as a track's
      Volume has them: 0 silence, 0.5 as it is, 1 for +25 dB) and the level point (its Depth, dB). */
  const DUCK_POINTS = [
    { name: "Point 1", prefix: "b1", colour: "#ff9d5c" },
    { name: "Point 2", prefix: "b2", colour: "#5cc8ff" },
    { name: "Point 3", prefix: "b3", colour: "#e8d36a" },
    { name: "Point 4", prefix: "b4", colour: "#ff7cc8" },
    { name: "Level", prefix: "lv", colour: "#c79bff" }
  ];
  const BELLS = 4, LEVEL_POINT = 4;
  const FREQ_LOW = 60, FREQ_HIGH = 12000;
  let duckLevels = DUCK_POINTS.map(() => [0, -100]);   // each point's gain now (dB, -100: silence) and what it hears (dBFS)

  /** A level (0..1, as a track's Volume) as dB; -Infinity for silence. */
  const volDb = (p) => (p <= 0.001 ? -Infinity : p >= 0.5 ? 25 * (p - 0.5) / 0.5 : 40 * Math.log10(p / 0.5));
  /** dB as such a level: up to +25 dB in the top half, down to silence in the bottom one. */
  const dbVol = (db) => (!(db > -100) ? 0 : db >= 0 ? clamp01(0.5 + 0.5 * db / 25) : 0.5 * Math.pow(10, db / 40));

  /** A meter in the window: a caption, a bar and its value. */
  function effectMeter(caption, title) {
    const row = element("div", "llm-meter");
    row.title = title;
    const bar = element("div", "llm-bar");
    bar.innerHTML = "<i></i><b></b>";
    const value = element("span", "llm-value", "");
    row.append(element("span", "ld-caption", caption), bar, value);
    return { row, bar, fill: bar.querySelector("i"), value, shown: -100 };
  }

  /** The window's controls drawn again: a tab's light and the tab's On show the same switch. */
  function refreshEffects() {
    for (const refresh of effectsRefreshers) refresh();
  }

  /** Its tabs: each effect's light (its switch) and its name (shows it). */
  function renderEffectsTabs() {
    const bar = $("live-effects-tabs");
    if (!bar) return;
    bar.textContent = "";
    EFFECTS.forEach((effect, index) => {
      if (index > 0) bar.appendChild(element("span", "lfx-arrow", "\u203a"));
      const tab = element("div", "lfx-tab");
      tab.style.setProperty("--effect", effect.colour);
      tab.classList.toggle("shown", effect.id === effectsTab);
      const light = toggle("master", 0, effect.on, "", "lfx-led", refreshEffects);
      light.dataset.keepTitle = "1";
      light.title = `${effect.name}: on or off`;
      light.setAttribute("aria-label", `${effect.name} on`);
      const name = element("button", "lfx-name", effect.name);
      name.type = "button";
      name.setAttribute("role", "tab");
      name.setAttribute("aria-selected", String(effect.id === effectsTab));
      name.addEventListener("click", () => showEffect(effect.id));
      tab.append(light, name);
      bar.appendChild(tab);
    });
  }

  /** The tab shown: its switch, what it does, and its controls and meters. */
  function renderEffects() {
    if (!effectsOpen) return;
    const body = $("live-effects-body");
    if (!body) return;
    // The last tab's canvases and redraws go (its tabs' lights with them).
    for (const canvas of effectsCanvases) {
      resizer.unobserve(canvas);
      observed = observed.filter((c) => c !== canvas);
    }
    effectsCanvases = [];
    refreshers = refreshers.filter((refresh) => !effectsRefreshers.has(refresh));
    const firstRefresher = refreshers.length;
    renderEffectsTabs();
    body.textContent = "";
    duckerView = null;
    effectsView = { meters: {}, driveCurve: null, compressorCurve: null, history: null, goniometer: null };

    const effect = EFFECTS.find((e) => e.id === effectsTab) || EFFECTS[0];
    watchWidth(effect.id === "width");
    body.style.setProperty("--effect", effect.colour);
    body.dataset.effect = effect.id;
    const top = element("div", "lfx-top");
    const on = toggle("master", 0, effect.on, "On", "lfx-on", refreshEffects);
    on.dataset.keepTitle = "1";
    on.title = `${effect.name} on or off`;
    top.append(on, element("span", "lfx-about", effect.about));
    body.appendChild(top);

    if (effect.id === "dynamicEq") renderDynamicEq(body);
    else if (effect.id === "compressor") renderCompressor(body);
    else if (effect.id === "drive") renderDrive(body);
    else if (effect.id === "width") renderWidth(body);
    else renderLimiter(body);

    effectsRefreshers = new Set(refreshers.slice(firstRefresher));
  }

  /** A dynamic EQ point switched on (its own light; one missing from the settings: on). */
  function duckPointOn(index) {
    const s = spec("master", `${DUCK_POINTS[index].prefix}On`);
    return !s || valueOf("master", 0, s) >= 0.5;
  }

  /** The dynamic EQ (Grex's ducker): the live channels it listens to, its curve (as it is on the
      dashed line in the middle, up to +25 dB at the top, silence at the bottom; the green line
      the music as it's heard now), its four bells and its level point (each with its own light:
      on or off). */
  function renderDynamicEq(body) {
    const listen = element("div", "lfx-listen");
    listen.appendChild(element("span", "ld-caption", "Listens to"));
    for (const channel of live.channels) {
      const track = trackOf(channel.id);
      const chip = toggle("channel", channel.id, "duck", track ? track.name : `Track ${channel.id}`, "lfx-key");
      chip.style.setProperty("--track", track ? track.colour : "#888");
      chip.dataset.keepTitle = "1";
      chip.title = `${track ? track.name : "This channel"}: the dynamic EQ listens to it (after its fader). Click: on or off`;
      listen.appendChild(chip);
    }
    if (!live.channels.length) listen.appendChild(element("span", "lfx-none", "No live channels yet: add them in Live & podcasts or Live streaming"));

    const curve = element("canvas", "ldk-curve");
    curve.title = "The green line is the music as it's heard now: on the dashed line in the middle, as it came. Each point's ring is its Gain "
      + "(up to +25 dB at the top, as it is on the dashed line, silence at the bottom), "
      + "its triangle its Duck (as low as the voices take it). Drag either: across for its frequency, up or down for its level "
      + "(the two on one another, nothing ducked: down takes the Duck, up the Gain). The wheel on one: its width. "
      + "Double-click the ring: as it is; the triangle: no ducking.";
    observeSize(curve);
    effectsCanvases.push(curve);
    const columns = element("div", "ldk-columns");
    const keys = [];
    DUCK_POINTS.forEach((point, index) => {
      const bell = index < BELLS;
      const column = element("div", "ldk-column");
      column.style.setProperty("--point", point.colour);
      const head = element("div", "ldk-head");
      const light = toggle("master", 0, `${point.prefix}On`, "", "ldk-led", refreshEffects);
      light.dataset.keepTitle = "1";
      light.title = `${point.name}: on or off (off, the music goes through it as it came; it still listens)`;
      light.setAttribute("aria-label", `${point.name} on`);
      head.append(light, element("span", "ldk-name", point.name));
      if (bell) {
        // Its Gain and its Duck, as the curve's ring and triangle have them.
        const levels = element("span", "ldk-levels");
        levels.title = "Its Gain (the ring), and its Duck (the triangle: as low as the voices take it)";
        const showLevels = () => {
          const p = bellPoint(index);
          levels.textContent = `${trackVolumeText(p.gain).replace(" dB", "")} \u2193 ${trackVolumeText(p.duck)}`;
        };
        showLevels();
        refreshers.push(showLevels);
        head.appendChild(levels);
      }
      const showOn = () => column.classList.toggle("off", !duckPointOn(index));
      showOn();
      refreshers.push(showOn);
      const key = element("div", "ldk-key");
      key.innerHTML = '<i></i><b></b>';
      key.title = "What it hears of the voices in its band, and its threshold";
      keys.push(key);
      const knobs = element("div", "ldk-knobs");
      if (bell) knobs.append(knob("master", 0, `${point.prefix}Q`, "Q", "small"));
      else knobs.append(knob("master", 0, "lvDepth", "Depth", "small"));
      knobs.append(knob("master", 0, `${point.prefix}Thresh`, "Thresh", "small"), knob("master", 0, `${point.prefix}Ratio`, "Ratio", "small"),
                   knob("master", 0, `${point.prefix}Attack`, "Attack", "small"), knob("master", 0, `${point.prefix}Release`, "Release", "small"));
      column.append(head, key, knobs);
      columns.appendChild(column);
    });
    body.append(listen, curve, columns);
    duckerView = { curve, keys, drag: null };
    wireDuckerCurve(curve);
  }

  /** An effect's display beside its knobs: its canvas fills it (and is watched for its size). */
  function effectDisplay(className, title) {
    const holder = element("div", "lfx-display");
    const canvas = element("canvas", className);
    canvas.title = title;
    holder.appendChild(canvas);
    observeSize(canvas);
    effectsCanvases.push(canvas);
    return { holder, canvas };
  }

  function renderCompressor(body) {
    const row = element("div", "lfx-row three");
    const knobs = element("div", "lfx-knobs");
    knobs.append(knob("master", 0, "compThresh", "Threshold"), knob("master", 0, "compRatio", "Ratio"),
                 knob("master", 0, "compAttack", "Attack"), knob("master", 0, "compRelease", "Release"),
                 knob("master", 0, "compKnee", "Knee"), knob("master", 0, "compMakeup", "Makeup"));
    const display = effectDisplay("lfx-curve", "Its curve: how loud it goes in (across) and comes out (up), in dB, against a straight line (the dashed line: its Threshold)");
    row.append(knobs, display.holder);
    const reduction = effectMeter("Reduction", "How much the compressor pulls down");
    body.append(row, reduction.row);
    effectsView.meters.compressor = reduction;
    effectsView.compressorCurve = display.canvas;
  }

  function renderDrive(body) {
    const row = element("div", "lfx-row");
    const knobs = element("div", "lfx-knobs");
    knobs.append(knob("master", 0, "drive", "Drive"), knob("master", 0, "driveTone", "Tone"),
                 knob("master", 0, "driveMix", "Mix"), knob("master", 0, "driveOut", "Output"));
    const display = effectDisplay("lfx-curve", "Its curve: the sound going in (across) and coming out (up), against a straight line");
    row.append(knobs, display.holder);
    const rounding = effectMeter("Rounding", "How far its curve rounds the loudest moments");
    body.append(row, rounding.row);
    effectsView.meters.drive = rounding;
    effectsView.driveCurve = display.canvas;
  }

  /** Sonify is told whether the width's goniometer shows: its points come with the status meanwhile. */
  let widthWatched = false;
  let stereoPoints = null;     // the latest moments as mid and side, in turn (the status's)
  function watchWidth(on) {
    if (on === widthWatched) return;
    widthWatched = on;
    if (!on) stereoPoints = null;
    Hub.send("live", { action: "effectsWatch", width: on });
  }

  function renderWidth(body) {
    const row = element("div", "lfx-row three");
    const knobs = element("div", "lfx-knobs");
    knobs.append(knob("master", 0, "width", "Width"), knob("master", 0, "widthBass", "Bass Mono"), knob("master", 0, "widthOut", "Output"));
    const display = effectDisplay("lfx-goniometer", "The stereo as it leaves the width: mono stands straight up, wide spreads across, and a line lying flat is out of phase (it vanishes in mono)");
    row.append(knobs, display.holder);
    const meter = effectMeter("Correlation", "How alike the two sides are: +1 mono, 0 unrelated (wide), below 0 out of phase (it thins out in mono)");
    meter.bar.classList.add("correlation");
    body.append(row, meter.row);
    effectsView.meters.correlation = meter;
    effectsView.goniometer = display.canvas;
  }

  /** The goniometer: each moment's side across and its mid up (left leans one way, right the
      other), scaled to the loudest, the newest brightest. */
  function drawGoniometer() {
    const canvas = effectsView.goniometer;
    const g = canvas.getContext("2d");
    const { width, height } = canvas;
    if (width < 4 || height < 4) return;
    const ratio = window.devicePixelRatio || 1;
    const on = valueOf("master", 0, spec("master", "widthOn")) >= 0.5;
    const cx = width / 2, cy = height / 2, radius = Math.min(width, height) / 2 - 10 * ratio;
    g.clearRect(0, 0, width, height);
    g.lineWidth = 1;
    g.strokeStyle = "rgba(255,255,255,0.08)";
    g.beginPath(); g.arc(cx, cy, radius, 0, Math.PI * 2); g.stroke();
    g.beginPath(); g.moveTo(cx, cy - radius); g.lineTo(cx, cy + radius); g.stroke();
    g.beginPath(); g.moveTo(cx - radius, cy); g.lineTo(cx + radius, cy); g.stroke();
    const diagonal = radius * Math.SQRT1_2;
    g.setLineDash([3 * ratio, 4 * ratio]);
    g.beginPath(); g.moveTo(cx - diagonal, cy - diagonal); g.lineTo(cx + diagonal, cy + diagonal); g.stroke();
    g.beginPath(); g.moveTo(cx + diagonal, cy - diagonal); g.lineTo(cx - diagonal, cy + diagonal); g.stroke();
    g.setLineDash([]);
    g.font = `${10 * ratio}px Segoe UI, sans-serif`;
    g.fillStyle = "rgba(255,255,255,0.35)";
    g.textAlign = "center";
    g.fillText("M", cx, cy - radius - 2 * ratio);
    g.fillText("L", cx - diagonal - 6 * ratio, cy - diagonal);
    g.fillText("R", cx + diagonal + 6 * ratio, cy - diagonal);
    g.textAlign = "start";
    const points = stereoPoints;
    if (!points || points.length < 4) return;
    let loudest = 0.05;
    for (let i = 0; i < points.length; i++) loudest = Math.max(loudest, Math.abs(points[i]));
    const scale = radius / (loudest * 1.15);
    const count = points.length / 2;
    const colour = on ? "92,200,255" : "200,200,200";
    for (let i = 0; i < count; i++) {
      const mid = points[2 * i], side = points[2 * i + 1];
      g.fillStyle = `rgba(${colour},${(0.15 + 0.75 * i / count).toFixed(3)})`;
      g.fillRect(cx - side * scale - ratio, cy - mid * scale - ratio, 2 * ratio, 2 * ratio);
    }
  }

  function renderLimiter(body) {
    const row = element("div", "lfx-row three");
    const knobs = element("div", "lfx-knobs");
    knobs.append(knob("master", 0, "limGain", "Gain"), knob("master", 0, "limCeiling", "Ceiling"), knob("master", 0, "limRelease", "Release"));
    const display = effectDisplay("lfx-history", "Its last ten seconds: how loud it went in (dim), what came out (bright) and how much it pulled down (red, from the top); the dashed line is its Ceiling");
    row.append(knobs, display.holder);
    const input = effectMeter("In", "How loud it goes into the limiter, after Gain: past the mark (0 dBFS) it pulls it down (off: how loud it goes out)");
    input.bar.classList.add("level");
    const limited = effectMeter("Reduction", "How much the limiter pulls down (it never looks ahead: nothing is delayed)");
    body.append(row, input.row, limited.row);
    effectsView.meters.input = input;
    effectsView.meters.limiter = limited;
    effectsView.history = display.canvas;
  }

  /** Another tab (kept for next time). */
  function showEffect(id) {
    if (!EFFECTS.some((e) => e.id === id)) return;
    effectsTab = id;
    keepEffects({ tab: id });
    renderEffects();
  }

  /** The drive's curve: what comes out for what goes in (Drive, Mix and Output as they are). */
  function drawDriveCurve() {
    const canvas = effectsView.driveCurve;
    const g = canvas.getContext("2d");
    const { width, height } = canvas;
    if (width < 4 || height < 4) return;
    const ratio = window.devicePixelRatio || 1;
    const value = (id) => valueOf("master", 0, spec("master", id));
    const reach = 0.3 * Math.pow(10, Math.max(0, Math.min(24, value("drive"))) / 20);
    const keep = 1 / Math.tanh(reach);
    const mix = Math.max(0, Math.min(1, value("driveMix") / 100));
    const output = Math.pow(10, value("driveOut") / 20);
    const on = value("driveOn") >= 0.5;
    const pad = 6 * ratio;
    const toX = (x) => pad + (x + 1) / 2 * (width - 2 * pad);
    const toY = (y) => height - pad - (y + 1) / 2 * (height - 2 * pad);
    g.clearRect(0, 0, width, height);
    g.strokeStyle = "rgba(255,255,255,0.07)";
    g.lineWidth = 1;
    for (const at of [-1, -0.5, 0, 0.5, 1]) {
      g.beginPath(); g.moveTo(toX(at), toY(-1)); g.lineTo(toX(at), toY(1)); g.stroke();
      g.beginPath(); g.moveTo(toX(-1), toY(at)); g.lineTo(toX(1), toY(at)); g.stroke();
    }
    g.strokeStyle = "rgba(255,255,255,0.25)";
    g.beginPath(); g.moveTo(toX(-1), toY(-1)); g.lineTo(toX(1), toY(1)); g.stroke();
    g.strokeStyle = on ? "#ff8c42" : "rgba(255,255,255,0.35)";
    g.lineWidth = 2 * ratio;
    g.beginPath();
    for (let i = 0; i <= 100; i++) {
      const x = -1 + i / 50;
      const y = Math.max(-1.05, Math.min(1.05, (x + (Math.tanh(reach * x) * keep - x) * mix) * output));
      if (i === 0) g.moveTo(toX(x), toY(y)); else g.lineTo(toX(x), toY(y));
    }
    g.stroke();
  }

  /** The compressor's curve: how loud it comes out (up) for how loud it goes in (across), in dB
      (Threshold, Ratio, Knee and Makeup as they are; its knee as Sonify's). */
  function drawCompressorCurve() {
    const canvas = effectsView.compressorCurve;
    const g = canvas.getContext("2d");
    const { width, height } = canvas;
    if (width < 4 || height < 4) return;
    const ratio = window.devicePixelRatio || 1;
    const value = (id) => valueOf("master", 0, spec("master", id));
    const threshold = value("compThresh"), knee = Math.max(0, value("compKnee")), makeup = value("compMakeup");
    const slope = value("compRatio") > 1 ? 1 - 1 / value("compRatio") : 0;
    const on = value("compOn") >= 0.5;
    const LOW = -48, HIGH = 6;
    const pad = 8 * ratio;
    const toX = (db) => pad + (db - LOW) / (HIGH - LOW) * (width - 2 * pad);
    const toY = (db) => height - pad - (db - LOW) / (HIGH - LOW) * (height - 2 * pad);
    const reduction = (db) => {
      const above = db - threshold;
      if (2 * above <= -knee) return 0;
      if (knee > 0 && 2 * Math.abs(above) <= knee) return slope * (above + knee / 2) * (above + knee / 2) / (2 * knee);
      return slope * above;
    };
    g.clearRect(0, 0, width, height);
    g.lineWidth = 1;
    g.font = `${10 * ratio}px Segoe UI, sans-serif`;
    g.fillStyle = "rgba(255,255,255,0.3)";
    for (let db = LOW; db <= 0; db += 12) {
      g.strokeStyle = db === 0 ? "rgba(255,255,255,0.16)" : "rgba(255,255,255,0.07)";
      g.beginPath(); g.moveTo(toX(db), toY(LOW)); g.lineTo(toX(db), toY(HIGH)); g.stroke();
      g.beginPath(); g.moveTo(toX(LOW), toY(db)); g.lineTo(toX(HIGH), toY(db)); g.stroke();
      if (db > LOW) g.fillText(db === 0 ? "0 dB" : `\u2212${-db}`, toX(db) + 3 * ratio, toY(LOW) - 3 * ratio);
    }
    g.strokeStyle = "rgba(255,255,255,0.25)";
    g.beginPath(); g.moveTo(toX(LOW), toY(LOW)); g.lineTo(toX(HIGH), toY(HIGH)); g.stroke();
    g.setLineDash([4 * ratio, 4 * ratio]);
    g.strokeStyle = on ? "rgba(224,165,74,0.65)" : "rgba(255,255,255,0.25)";
    g.beginPath(); g.moveTo(toX(threshold), toY(LOW)); g.lineTo(toX(threshold), toY(HIGH)); g.stroke();
    g.setLineDash([]);
    g.strokeStyle = on ? "#e0a54a" : "rgba(255,255,255,0.35)";
    g.lineWidth = 2 * ratio;
    g.beginPath();
    for (let i = 0; i <= 108; i++) {
      const x = LOW + i * (HIGH - LOW) / 108;
      const y = x - reduction(x) + makeup;
      if (i === 0) g.moveTo(toX(x), toY(y)); else g.lineTo(toX(x), toY(y));
    }
    g.stroke();
  }

  const HISTORY_MS = 10000;
  let limiterHistory = [];     // the limiter's last seconds, as the status brought them: { at, input, output, reduction }

  /** What the status brought: what went in, what it pulled down, and so what came out (at most its
      Ceiling while it was on: switched off later, the past stays as it was). */
  function recordLimiter(levels, now) {
    if (typeof levels.limiterIn !== "number") return;
    const input = levels.limiterIn, reduction = Math.max(0, Number(levels.limiter) || 0);
    const on = valueOf("master", 0, spec("master", "limOn")) >= 0.5;
    const output = on ? Math.min(valueOf("master", 0, spec("master", "limCeiling")), input - reduction) : input;
    limiterHistory.push({ at: now, input, output, reduction });
    // One older than the ten seconds stays: the line starts at the left edge.
    let old = 0;
    while (old < limiterHistory.length - 1 && limiterHistory[old + 1].at < now - HISTORY_MS) old++;
    if (old) limiterHistory.splice(0, old);
  }

  /** The limiter's last ten seconds: how loud it went in (dim), what came out (bright), how much it
      pulled down (red, from the top) and its Ceiling (dashed). */
  function drawLimiterHistory(now) {
    const canvas = effectsView.history;
    const g = canvas.getContext("2d");
    const { width, height } = canvas;
    if (width < 4 || height < 4) return;
    const ratio = window.devicePixelRatio || 1;
    const value = (id) => valueOf("master", 0, spec("master", id));
    const on = value("limOn") >= 0.5;
    const ceiling = value("limCeiling");
    const TOP = 6, BOTTOM = -36;
    const perDb = height / (TOP - BOTTOM);
    const toY = (db) => (TOP - Math.max(BOTTOM, Math.min(TOP, db))) * perDb;
    const toX = (at) => width - (now - at) / HISTORY_MS * width;
    g.clearRect(0, 0, width, height);
    g.lineWidth = 1;
    g.font = `${10 * ratio}px Segoe UI, sans-serif`;
    g.fillStyle = "rgba(255,255,255,0.3)";
    for (let db = 0; db > BOTTOM; db -= 6) {
      const y = Math.round(toY(db)) + 0.5;
      g.strokeStyle = db === 0 ? "rgba(255,255,255,0.16)" : "rgba(255,255,255,0.06)";
      g.beginPath(); g.moveTo(0, y); g.lineTo(width, y); g.stroke();
      g.fillText(db === 0 ? "0 dB" : `\u2212${-db}`, 4 * ratio, y - 3 * ratio);
    }
    const points = limiterHistory;
    if (points.length > 1) {
      const first = toX(points[0].at), last = toX(points[points.length - 1].at);
      const area = (level, colour) => {
        g.beginPath();
        g.moveTo(first, height);
        for (const p of points) g.lineTo(toX(p.at), toY(level(p)));
        g.lineTo(last, height);
        g.closePath();
        g.fillStyle = colour;
        g.fill();
      };
      area((p) => p.input, "rgba(70,194,122,0.25)");
      area((p) => p.output, "rgba(70,194,122,0.7)");
      g.beginPath();
      g.moveTo(first, 0);
      for (const p of points) g.lineTo(toX(p.at), Math.min(height, p.reduction * perDb));
      g.lineTo(last, 0);
      g.closePath();
      g.fillStyle = "rgba(255,90,99,0.55)";
      g.fill();
    }
    if (on) {
      const y = Math.round(toY(ceiling)) + 0.5;
      g.setLineDash([5 * ratio, 4 * ratio]);
      g.strokeStyle = "rgba(255,90,99,0.85)";
      g.beginPath(); g.moveTo(0, y); g.lineTo(width, y); g.stroke();
      g.setLineDash([]);
    }
  }

  const freqToX = (f, width) => Math.log(f / FREQ_LOW) / Math.log(FREQ_HIGH / FREQ_LOW) * width;
  const xToFreq = (x, width) => FREQ_LOW * Math.pow(FREQ_HIGH / FREQ_LOW, clamp01(x / width));

  /** A bell's settings: where it sits, how wide, its Gain and its Duck (as a track's Volume has them). */
  function bellPoint(index) {
    const prefix = DUCK_POINTS[index].prefix;
    const gain = valueOf("master", 0, spec("master", `${prefix}Gain`));
    return {
      freq: valueOf("master", 0, spec("master", `${prefix}Freq`)),
      q: valueOf("master", 0, spec("master", `${prefix}Q`)),
      gain,
      duck: Math.min(gain, valueOf("master", 0, spec("master", `${prefix}Duck`)))
    };
  }

  /** The curve's frame: where levels go up and down (0..1 as a track's Volume: silence at the
      bottom, as it is in the middle, +25 dB at the top). */
  function duckerFrame(canvas) {
    const ratio = window.devicePixelRatio || 1;
    const top = 10 * ratio, bottom = canvas.height - 16 * ratio;
    return { ratio, top, bottom, toY: (p) => bottom - (bottom - top) * clamp01(p), toP: (y) => clamp01((bottom - y) / (bottom - top)) };
  }

  /** A bell at this gain (as a gain: 0 silence), as heard at frequency f: x + (g - 1) * its band,
      its band a band-pass with its centre at 0 dB. dB. */
  function bellDb(g, f, centre, q) {
    const r = f / centre;
    const re = 1 - r * r, im = r / Math.max(0.05, q);
    const d = re * re + im * im;
    const bandRe = im * im / d, bandIm = im * re / d;   // (j r/q) / (1 - r^2 + j r/q)
    const hRe = 1 + (g - 1) * bandRe, hIm = (g - 1) * bandIm;
    return 10 * Math.log10(Math.max(1e-12, hRe * hRe + hIm * hIm));
  }

  function drawDucker() {
    const view = duckerView;
    if (!view) return;
    const canvas = view.curve;
    const g = canvas.getContext("2d");
    const { width, height } = canvas;
    if (width < 4 || height < 4) return;
    g.clearRect(0, 0, width, height);
    const { ratio, top, bottom, toY } = duckerFrame(canvas);
    const yOfDb = (db) => toY(dbVol(db));
    const on = (live.master.duck !== undefined ? live.master.duck : 1) >= 0.5;

    // The grid: octaves across; levels up and down, as it is (0 dB) in the middle.
    g.lineWidth = 1;
    g.font = `${10 * ratio}px Segoe UI, sans-serif`;
    for (const f of [100, 200, 500, 1000, 2000, 5000, 10000]) {
      const x = Math.round(freqToX(f, width)) + 0.5;
      g.strokeStyle = "rgba(255,255,255,0.06)";
      g.beginPath(); g.moveTo(x, top); g.lineTo(x, bottom); g.stroke();
      g.fillStyle = "rgba(255,255,255,0.35)";
      g.fillText(f >= 1000 ? `${f / 1000}k` : String(f), x + 2, height - 3 * ratio);
    }
    // (Each level's words just over its line; the top one's under it.)
    for (const [db, label] of [[24, "+24"], [12, "+12"], [-6, "\u22126"], [-12, "\u221212"], [-24, "\u221224"], [-Infinity, "\u2212\u221e"]]) {
      const y = Math.round(yOfDb(db)) + 0.5;
      g.strokeStyle = "rgba(255,255,255,0.06)";
      g.beginPath(); g.moveTo(0, y); g.lineTo(width, y); g.stroke();
      g.fillStyle = "rgba(255,255,255,0.3)";
      g.fillText(label, 3 * ratio, db === 24 ? y + 11 * ratio : y - 3 * ratio);
    }
    const unity = Math.round(yOfDb(0)) + 0.5;
    g.strokeStyle = "rgba(70,194,122,0.45)";
    g.setLineDash([4 * ratio, 4 * ratio]);
    g.beginPath(); g.moveTo(0, unity); g.lineTo(width, unity); g.stroke();
    g.setLineDash([]);
    g.fillStyle = "rgba(125,255,176,0.55)";
    g.fillText("0 dB", 3 * ratio, unity - 3 * ratio);

    // The response across: the bells in turn (each at its gain), then the level point.
    const bells = [];
    for (let index = 0; index < BELLS; index++) bells.push(Object.assign(bellPoint(index), { on: duckPointOn(index) }));
    const responseAt = (x, gains, levelDb) => {
      const f = xToFreq(x, width);
      let db = levelDb;
      bells.forEach((b, index) => { if (gains[index] !== 1) db += bellDb(gains[index], f, b.freq, b.q); });
      return db;
    };
    const trace = (gains, levelDb) => {
      g.beginPath();
      for (let x = 0; x <= width; x += 2 * ratio) {
        const y = yOfDb(responseAt(x, gains, levelDb));
        if (x === 0) g.moveTo(x, y); else g.lineTo(x, y);
      }
    };

    // As set (dim): each bell at its Gain, a point switched off as it is. As heard now (the green
    // line): each at its gain now, ducked by the voices.
    const asSet = bells.map((b) => (on && b.on ? volGain(b.gain) : 1));
    const now = bells.map((b, index) => {
      const level = duckLevels[index];
      return !level || !Number.isFinite(level[0]) ? 1 : level[0] <= -99 ? 0 : Math.pow(10, level[0] / 20);
    });
    g.lineWidth = 1.5 * ratio;
    g.strokeStyle = "rgba(255,255,255,0.22)";
    trace(asSet, 0);
    g.stroke();
    g.strokeStyle = on ? "#7dffb0" : "rgba(255,255,255,0.3)";
    g.lineWidth = 2 * ratio;
    trace(now, duckLevels[LEVEL_POINT] && Number.isFinite(duckLevels[LEVEL_POINT][0]) ? Math.min(0, duckLevels[LEVEL_POINT][0]) : 0);
    g.stroke();

    // Each bell: its ring (Gain) and its triangle (Duck), the line between them as far as the
    // voices may take it, and the dot where it is now. A point switched off: dim, dashed.
    bells.forEach((b, index) => {
      const x = freqToX(b.freq, width);
      const ringY = toY(b.gain), duckY = toY(b.duck);
      const dotY = yOfDb(on && duckLevels[index] && Number.isFinite(duckLevels[index][0]) ? duckLevels[index][0] : 0);
      const colour = DUCK_POINTS[index].colour;
      g.strokeStyle = colour;
      g.fillStyle = colour;
      g.globalAlpha = b.on ? 0.55 : 0.2;
      g.lineWidth = 2 * ratio;
      if (duckY - ringY > 1) { g.beginPath(); g.moveTo(x, ringY); g.lineTo(x, duckY); g.stroke(); }
      g.globalAlpha = b.on ? 1 : 0.45;
      if (!b.on) g.setLineDash([3 * ratio, 3 * ratio]);
      g.beginPath(); g.arc(x, ringY, 7 * ratio, 0, Math.PI * 2); g.stroke();
      const s = 6 * ratio;
      g.beginPath(); g.moveTo(x - s, duckY - s * 0.6); g.lineTo(x + s, duckY - s * 0.6); g.lineTo(x, duckY + s * 0.8); g.closePath();
      g.stroke();
      g.setLineDash([]);
      g.beginPath(); g.arc(x, dotY, 3.5 * ratio, 0, Math.PI * 2); g.fill();
      g.globalAlpha = 1;
    });

    // The level point: a bar on the right, down from the middle (dim: its Depth; bright: now).
    const levelDepth = valueOf("master", 0, spec("master", "lvDepth"));
    if (levelDepth > 0) {
      const levelOn = duckPointOn(LEVEL_POINT);
      const levelNow = on && duckLevels[LEVEL_POINT] && Number.isFinite(duckLevels[LEVEL_POINT][0]) ? Math.min(0, duckLevels[LEVEL_POINT][0]) : 0;
      g.fillStyle = DUCK_POINTS[LEVEL_POINT].colour;
      g.globalAlpha = levelOn ? 0.3 : 0.12;
      g.fillRect(width - 8 * ratio, yOfDb(0), 6 * ratio, yOfDb(-levelDepth) - yOfDb(0));
      g.globalAlpha = 1;
      g.fillRect(width - 8 * ratio, yOfDb(0), 6 * ratio, yOfDb(levelNow) - yOfDb(0));
    }

    // The keys: what each point hears, against its threshold.
    view.keys.forEach((key, index) => {
      const threshold = valueOf("master", 0, spec("master", `${DUCK_POINTS[index].prefix}Thresh`));
      const heard = duckLevels[index] ? duckLevels[index][1] : -100;
      const at = (db) => clamp01((db + 70) / 70);
      key.querySelector("i").style.transform = `scaleX(${at(heard).toFixed(3)})`;
      key.querySelector("b").style.left = `${(at(threshold) * 100).toFixed(1)}%`;
      key.classList.toggle("over", heard > threshold);
    });
  }

  /** A level (0..1, as a track's Volume) as a gain. */
  const volGain = (p) => (p <= 0.001 ? 0 : p >= 0.5 ? Math.pow(10, 25 * (p - 0.5) / 0.5 / 20) : Math.pow(p / 0.5, 2));

  function wireDuckerCurve(canvas) {
    // The handle under the pointer: a bell's ring (Gain) or triangle (Duck); both, where they're
    // one on the other (the first move decides: up its Gain, down its Duck).
    const pick = (event) => {
      const r = canvas.getBoundingClientRect();
      const frame = duckerFrame(canvas);
      const x = (event.clientX - r.left) * frame.ratio, y = (event.clientY - r.top) * frame.ratio;
      let best = null, bestDistance = 18 * frame.ratio;
      for (let index = 0; index < BELLS; index++) {
        const b = bellPoint(index);
        const hx = freqToX(b.freq, canvas.width);
        const ringY = frame.toY(b.gain), duckY = frame.toY(b.duck);
        const together = Math.abs(duckY - ringY) < 4 * frame.ratio;
        for (const [handle, hy] of [["gain", ringY], ["duck", duckY]]) {
          const distance = Math.hypot(hx - x, hy - y);
          if (distance < bestDistance) { best = { index, handle: together ? "both" : handle, hx, hy }; bestDistance = distance; }
        }
      }
      return { hit: best, x, y, frame };
    };

    const setGain = (index, p) => {
      const prefix = DUCK_POINTS[index].prefix;
      const gainSpec = spec("master", `${prefix}Gain`), duckSpec = spec("master", `${prefix}Duck`);
      const gain = Math.round(clamp01(Math.abs(p - 0.5) < 0.008 ? 0.5 : p) * 1000) / 1000;
      setValue("master", 0, gainSpec, gain);
      if (valueOf("master", 0, duckSpec) > gain) setValue("master", 0, duckSpec, gain);   // the Duck never above it
    };
    const setDuck = (index, p) => {
      const prefix = DUCK_POINTS[index].prefix;
      const gain = valueOf("master", 0, spec("master", `${prefix}Gain`));
      setValue("master", 0, spec("master", `${prefix}Duck`), Math.round(Math.min(gain, clamp01(p)) * 1000) / 1000);
    };
    const showValues = () => refreshEffects();

    canvas.addEventListener("pointerdown", (event) => {
      if (event.button !== 0) return;
      const at = pick(event);
      if (!at.hit) return;
      event.preventDefault();
      canvas.setPointerCapture(event.pointerId);
      // Held where it was grabbed: the handle moves as far as the pointer does (no jump to it).
      duckerView.drag = Object.assign({ fromY: at.y, dx: at.hit.hx - at.x, dy: at.hit.hy - at.y }, at.hit);
      dragging++;
    });
    canvas.addEventListener("pointermove", (event) => {
      if (!duckerView || duckerView.drag === null) {
        canvas.style.cursor = pick(event).hit ? "grab" : "";
        return;
      }
      const at = pick(event);
      const drag = duckerView.drag;
      if (drag.handle === "both") {
        if (Math.abs(at.y - drag.fromY) < 3 * at.frame.ratio) {
          // Across only, so far: its frequency.
        } else {
          drag.handle = at.y < drag.fromY ? "gain" : "duck";
        }
      }
      const freqSpec = spec("master", `${DUCK_POINTS[drag.index].prefix}Freq`);
      setValue("master", 0, freqSpec, Math.round(Math.max(freqSpec.min, Math.min(freqSpec.max, xToFreq(at.x + drag.dx, canvas.width)))));
      if (drag.handle === "gain") setGain(drag.index, at.frame.toP(at.y + drag.dy));
      else if (drag.handle === "duck") setDuck(drag.index, at.frame.toP(at.y + drag.dy));
      showValues();
    });
    const end = () => {
      if (!duckerView || duckerView.drag === null) return;
      duckerView.drag = null;
      dragging = Math.max(0, dragging - 1);
      if (renderDue && !dragging) render();
    };
    canvas.addEventListener("pointerup", end);
    canvas.addEventListener("pointercancel", end);
    canvas.addEventListener("lostpointercapture", end);
    canvas.addEventListener("wheel", (event) => {
      const at = pick(event);
      if (!at.hit) return;
      event.preventDefault();
      const qSpec = spec("master", `${DUCK_POINTS[at.hit.index].prefix}Q`);
      const q = valueOf("master", 0, qSpec);
      setValue("master", 0, qSpec, Math.max(qSpec.min, Math.min(qSpec.max, q * (event.deltaY < 0 ? 1.08 : 1 / 1.08))));
      showValues();
    }, { passive: false });
    canvas.addEventListener("dblclick", (event) => {
      const at = pick(event);
      if (!at.hit) return;
      // The ring: as it is (its Duck kept as far below as it was); the triangle: no ducking.
      const b = bellPoint(at.hit.index);
      if (at.hit.handle === "duck") setDuck(at.hit.index, b.gain);
      else {
        const below = b.duck >= b.gain ? 0 : volDb(b.gain) - volDb(b.duck);
        setGain(at.hit.index, 0.5);
        setDuck(at.hit.index, Number.isFinite(below) ? dbVol(-below) : 0);
      }
      showValues();
    });
  }

  /** Its meters, as the status brings them (falling back gently, so they can be read). */
  function showEffectsMeters(levels) {
    const view = effectsView;
    if (!view) return;
    const show = (meter, value, falls, position, label, over) => {
      if (!meter) return;
      meter.shown = Math.max(value, meter.shown - falls);
      meter.fill.style.transform = `scaleX(${clamp01(position(meter.shown)).toFixed(3)})`;
      meter.bar.classList.toggle("over", over(meter.shown));
      meter.value.textContent = label(meter.shown);
    };
    const reduction = (db) => (db >= 0.05 ? `\u2212${db.toFixed(1)} dB` : "0 dB");
    show(view.meters.input, levels.limiterIn !== undefined ? levels.limiterIn : -100, 1.5, (db) => (db + 36) / 48,
         (db) => (db <= -99 ? "\u2212\u221e" : `${db > 0 ? "+" : db < 0 ? "\u2212" : ""}${Math.abs(db).toFixed(1)} dB`), (db) => db > 0);
    show(view.meters.compressor, levels.compressor || 0, 0.6, (db) => db / 24, reduction, () => false);
    show(view.meters.drive, levels.drive || 0, 0.6, (db) => db / 18, reduction, () => false);
    show(view.meters.limiter, levels.limiter || 0, 0.6, (db) => db / 24, reduction, (db) => db > 0.5);
    if (Array.isArray(levels.stereo)) stereoPoints = levels.stereo;
    const correlation = view.meters.correlation;
    if (correlation && typeof levels.correlation === "number") {
      const c = Math.max(-1, Math.min(1, levels.correlation));
      correlation.shown = c;
      correlation.fill.style.transform = `scaleX(${((c + 1) / 2).toFixed(3)})`;
      correlation.bar.classList.toggle("over", c < 0);
      correlation.value.textContent = `${c > 0.005 ? "+" : c < -0.005 ? "\u2212" : ""}${Math.abs(c).toFixed(2)}`;
    }
  }

  /** Inside the studio, where it was kept (the first time: in the middle). */
  function placeEffects(left, top) {
    const box = $("live-effects-window");
    const area = box.parentElement.getBoundingClientRect();
    if (!(area.width > 0) || box.hidden) return;
    const kept = effectsKept();
    const w = box.offsetWidth, h = box.offsetHeight;
    const x = left !== undefined ? left : typeof kept.left === "number" ? kept.left : (area.width - w) / 2;
    const y = top !== undefined ? top : typeof kept.top === "number" ? kept.top : (area.height - h) / 2;
    box.style.left = `${Math.round(Math.max(0, Math.min(Math.max(0, area.width - w), x)))}px`;
    box.style.top = `${Math.round(Math.max(0, Math.min(Math.max(0, area.height - h), y)))}px`;
  }

  /** The effects channel's window, floating over the studio (the master's FX button opens it). */
  function openEffects(open) {
    effectsOpen = open;
    const box = $("live-effects-window");
    box.hidden = !open;
    const button = $("live-effects-button");
    if (button) button.setAttribute("aria-expanded", String(open));
    if (open) {
      renderEffects();
      placeEffects();
    } else {
      duckerView = null;
      effectsView = null;
      watchWidth(false);
    }
  }

  /** Moved by its head, anywhere in the studio (its place kept). */
  function wireEffects() {
    const box = $("live-effects-window");
    const head = box.querySelector(".live-effects-head");
    head.addEventListener("pointerdown", (event) => {
      if (event.button !== 0 || event.target.closest("button")) return;
      event.preventDefault();
      head.setPointerCapture(event.pointerId);
      const start = { x: event.clientX, y: event.clientY, left: box.offsetLeft, top: box.offsetTop };
      const move = (e) => placeEffects(start.left + e.clientX - start.x, start.top + e.clientY - start.y);
      const end = () => {
        head.removeEventListener("pointermove", move);
        head.removeEventListener("pointerup", end);
        head.removeEventListener("lostpointercapture", end);
        keepEffects({ left: box.offsetLeft, top: box.offsetTop });
      };
      head.addEventListener("pointermove", move);
      head.addEventListener("pointerup", end);
      head.addEventListener("lostpointercapture", end);
    });
    $("live-effects-close").addEventListener("click", () => openEffects(false));
    $("live-effects-reset").addEventListener("click", (event) => {
      const effect = EFFECTS.find((e) => e.id === effectsTab) || EFFECTS[0];
      Hub.popupMenu(event.currentTarget, [
        { group: `Put the ${effect.name}'s controls back where they start? (Its switch stays as it is.)` },
        { label: `Reset the ${effect.name}`, value: true },
        { label: "Keep it", value: false }
      ], (yes) => { if (yes) Hub.send("live", { action: "resetEffect", effect: effect.id }); });
    });
    // The studio resized: it stays inside it (back where it was put once there's room again).
    new ResizeObserver(() => { if (!box.hidden) placeEffects(); }).observe(box.parentElement);
    const kept = effectsKept();
    if (EFFECTS.some((e) => e.id === kept.tab)) effectsTab = kept.tab;
  }

  // ---------------------------------------------------------------- the stream
  const SERVICES = [["YouTube", "YouTube"], ["Facebook", "Facebook"], ["Twitch", "Twitch"], ["TikTok", "TikTok"], ["Custom", "Other (RTMP)"]];

  /** Whether a destination gets the vertical picture: TikTok's, or a server marked 9:16. */
  const isVertical = (d) => d.service === "TikTok" || (d.service === "Custom" && Boolean(d.vertical));
  const anyVertical = () => (streamSettings().destinations || []).some((d) => d.on !== false && isVertical(d));
  const streamOf = () => live.stream || { available: false, settings: {}, cameras: [], screens: [] };

  function streamSettings() {
    const stream = streamOf();
    if (!stream.settings || typeof stream.settings !== "object") stream.settings = {};
    return stream.settings;
  }

  /** The settings to Sonify (which keeps them): at once, or after typing pauses. */
  function saveStream(now) {
    clearTimeout(streamSaveTimer);
    const send = () => Hub.send("live", { action: "streamSettings", settings: streamSettings() });
    if (now) send(); else streamSaveTimer = setTimeout(send, 400);
  }

  function setStream(key, value, now = true) {
    streamSettings()[key] = value;
    saveStream(now);
  }

  function openStream(open) {
    streamOpen = open;
    $("live-stream").hidden = !open;
    const button = $("live-stream-button");
    button.setAttribute("aria-expanded", String(open));
    button.classList.toggle("on", open);
    Hub.send("live", { action: "streamPanel", open });
    if (open) renderStream();
  }

  function streamField(label, control, hint) {
    const row = element("div", "ls-field");
    row.append(element("span", "ls-label", label), control);
    if (hint) row.title = hint;
    return row;
  }

  function streamSelect(options, value, onChange) {
    const box = element("select", "ls-select");
    for (const [id, name] of options) {
      const option = element("option", "", name);
      option.value = id;
      box.appendChild(option);
    }
    box.value = value;
    if (box.value !== value && options.length) box.value = options[0][0];
    box.addEventListener("change", () => onChange(box.value));
    return box;
  }

  function streamChoices(options, value, onChange) {
    const row = element("div", "lv-choices ls-choices");
    for (const [id, name] of options) {
      const button = smallButton("", name, "", () => {
        row.querySelectorAll("button").forEach((b) => b.setAttribute("aria-checked", String(b === button)));
        onChange(id);
      });
      button.setAttribute("aria-checked", String(id === value));
      row.appendChild(button);
    }
    return row;
  }

  function streamToggle(label, on, title, onChange) {
    const button = element("button", "lv-switch ls-toggle", label);
    button.type = "button";
    button.title = title;
    button.setAttribute("aria-pressed", String(on));
    button.addEventListener("click", () => {
      const now = button.getAttribute("aria-pressed") !== "true";
      button.setAttribute("aria-pressed", String(now));
      onChange(now);
    });
    return button;
  }

  function renderStream() {
    const box = $("live-stream");
    if (!box || !streamOpen) return;
    box.textContent = "";
    const stream = streamOf();
    const s = streamSettings();
    const onAirNow = Boolean(stream.live);

    const head = element("div", "ls-head");
    head.append(element("span", "ls-title", "Stream settings"), smallButton("ls-close", "\u00d7", "Close the panel (a stream goes on)", () => openStream(false)));

    // The picture as it goes out; the vertical one beside it while a vertical destination is ticked.
    const previews = element("div", "ls-previews");
    const preview = element("div", "ls-preview");
    const image = element("img", "");
    image.id = "ls-preview-image";
    image.alt = "The stream's picture";
    if (previewData) image.src = previewData;
    preview.append(image, element("span", "ls-badge", "LIVE"));
    preview.classList.toggle("live", onAirNow);
    previews.appendChild(preview);
    if (anyVertical()) {
      const tall = element("div", "ls-preview ls-preview-tall");
      const tallImage = element("img", "");
      tallImage.id = "ls-preview-tall-image";
      tallImage.alt = "The stream's vertical picture (TikTok)";
      if (previewTallData) tallImage.src = previewTallData;
      tall.append(tallImage, element("span", "ls-badge", "9:16"));
      tall.title = "The vertical picture (TikTok): its boxes and the part of Main that shows are set in the Controller";
      tall.classList.toggle("live", onAirNow);
      previews.classList.add("with-tall");
      previews.appendChild(tall);
    }
    box.append(head, previews);

    if (!stream.available && stream.problem) box.appendChild(element("p", "ls-problem", stream.problem));
    const status = element("div", "ls-status");
    status.id = "ls-status";
    box.appendChild(status);

    const body = element("div", "ls-body");

    // The picture: its size (its cameras and the screen are the Controller's).
    const picture = element("section", "ls-section");
    picture.appendChild(element("h4", "", "Picture"));
    picture.appendChild(streamField("Size", streamChoices([["480p", "480p"], ["720p", "720p"], ["1080p", "1080p"]], s.size || "720p", (v) => setStream("size", v))));
    const cameras = element("div", "ls-inline");
    cameras.append(element("span", "ls-hint", "Up to four cameras and the screen: the Controller"),
                   smallButton("ld-small", live.controllerOpen ? "Show it" : "Open it", "The Controller: which camera is the main picture, which are boxes, which are hidden",
                               () => Hub.send("live", { action: "controller", open: true })));
    picture.appendChild(streamField("Cameras", cameras));

    // What's over it.
    const over = element("section", "ls-section");
    over.appendChild(element("h4", "", "On the picture"));
    const title = element("input", "ls-input");
    title.type = "text";
    title.value = s.title || "";
    title.placeholder = "The show's name";
    title.maxLength = 80;
    title.addEventListener("input", () => setStream("title", title.value, false));
    title.addEventListener("keydown", (event) => event.stopPropagation());
    over.appendChild(streamField("Title", title));
    over.appendChild(streamField("Show", streamToggle("Now playing", s.nowPlaying !== false, "The track playing, at the bottom left", (v) => setStream("nowPlaying", v))));
    const logo = element("div", "ls-inline");
    logo.append(element("span", "ls-file", stream.logoName || "No logo"),
                smallButton("ld-small", "Choose\u2026", "A logo for the top right (a PNG with a clear background looks best)",
                            () => Hub.send("live", { action: "streamLogo", choose: true })));
    if (stream.logoName) logo.appendChild(smallButton("ld-small", "\u00d7", "No logo", () => Hub.send("live", { action: "streamLogo", choose: false })));
    over.appendChild(streamField("Logo", logo));

    // Where it goes.
    const destinations = element("section", "ls-section");
    destinations.appendChild(element("h4", "", "Destinations"));
    if (!Array.isArray(s.destinations)) s.destinations = [];
    const list = s.destinations;
    list.forEach((destination, index) => {
      const row = element("div", "ls-dest");
      const on = element("input", "");
      on.type = "checkbox";
      on.checked = destination.on !== false;
      on.title = "Stream to it";
      on.addEventListener("change", () => { destination.on = on.checked; saveStream(true); if (isVertical(destination)) renderStream(); });
      const service = streamSelect(SERVICES, destination.service || "YouTube", (v) => { destination.service = v; saveStream(true); renderStream(); });
      const custom = destination.service === "Custom";
      const tiktok = destination.service === "TikTok";
      const key = element("input", "ls-input ls-key");
      key.type = "password";
      key.value = destination.key || "";
      key.autocomplete = "off";
      key.spellcheck = false;
      key.placeholder = custom ? "rtmp://server/app/key" : "Stream key";
      key.title = custom ? "The server's whole RTMP or RTMPS address, its key at the end"
                : tiktok ? "TikTok's stream key, from its LIVE Center (it's new for each live: paste the new one before going live)"
                         : `Your ${destination.service || "YouTube"} stream key (its live dashboard gives it)`;
      key.addEventListener("input", () => { destination.key = key.value; saveStream(false); });
      key.addEventListener("keydown", (event) => event.stopPropagation());
      const reveal = smallButton("ld-small", "Show", "Show or hide the key", () => {
        key.type = key.type === "password" ? "text" : "password";
        reveal.textContent = key.type === "password" ? "Show" : "Hide";
      });
      const remove = smallButton("ld-small", "\u00d7", "Remove this destination", () => { list.splice(index, 1); saveStream(true); renderStream(); });
      row.append(on, service, key, reveal);
      // A server of your own: the vertical picture if it's a vertical platform's.
      if (custom) {
        const tall = streamToggle("9:16", Boolean(destination.vertical), "The vertical picture (9:16), for a vertical platform's server",
                                  (v) => { destination.vertical = v; saveStream(true); renderStream(); });
        tall.classList.add("ls-tall-toggle");
        row.appendChild(tall);
      }
      row.appendChild(remove);
      destinations.appendChild(row);
      // TikTok: its server URL too (its LIVE Center gives it with the key).
      if (tiktok) {
        const sub = element("div", "ls-dest ls-dest-sub");
        const server = element("input", "ls-input ls-key");
        server.type = "text";
        server.value = destination.server || "";
        server.autocomplete = "off";
        server.spellcheck = false;
        server.placeholder = "Server URL (rtmp://...)";
        server.title = "TikTok's server URL, from its LIVE Center (desktop: livecenter.tiktok.com), with the stream key";
        server.addEventListener("input", () => { destination.server = server.value.trim(); saveStream(false); });
        server.addEventListener("keydown", (event) => event.stopPropagation());
        sub.append(element("span", "ls-sub-label", "Server"), server);
        destinations.appendChild(sub);
        destinations.appendChild(element("p", "ls-hint ls-dest-note",
          "Vertical picture (9:16). TikTok's LIVE Center gives the server URL and the key (1,000 followers and up); the key is new for each live."));
      }
    });
    if (!list.length) destinations.appendChild(element("p", "ls-hint", "No destination yet: add YouTube, Facebook, Twitch, TikTok or a server of your own."));
    const foot = element("div", "ls-inline");
    foot.append(smallButton("plain-button small", "+ Add destination", "Another place to stream to, at the same time",
                            () => { list.push({ service: "YouTube", key: "", on: true }); saveStream(true); renderStream(); }),
                streamToggle("Keep a copy (MP4)", Boolean(s.copy), "The stream as it goes out, kept in the recordings folder", (v) => setStream("copy", v)));
    destinations.appendChild(foot);

    // How good, and the sound against the picture.
    const quality = element("section", "ls-section");
    quality.appendChild(element("h4", "", "Quality and sync"));
    quality.appendChild(streamField("Picture", streamSelect([["2500", "2500 kbps (480p)"], ["4500", "4500 kbps (720p)"], ["6000", "6000 kbps (1080p)"],
                                                             ["8000", "8000 kbps (1080p, a fast line)"]], String(s.videoKbps || 4500), (v) => setStream("videoKbps", Number(v)))));
    quality.appendChild(streamField("Sound", streamSelect([["128", "128 kbps"], ["160", "160 kbps"], ["192", "192 kbps"]], String(s.audioKbps || 160),
                                                          (v) => setStream("audioKbps", Number(v)))));
    const sync = element("input", "ls-range");
    sync.type = "range";
    sync.min = "-500";
    sync.max = "500";
    sync.step = "10";
    sync.value = String(s.syncMs || 0);
    const syncText = element("span", "ls-value", "");
    const showSync = () => { const v = Number(sync.value); syncText.textContent = `${v > 0 ? "+" : ""}${v} ms`; };
    showSync();
    sync.addEventListener("input", () => { showSync(); setStream("syncMs", Number(sync.value), false); });
    sync.addEventListener("dblclick", () => { sync.value = "0"; showSync(); setStream("syncMs", 0); });
    const syncRow = element("div", "ls-inline");
    syncRow.append(sync, syncText);
    quality.appendChild(streamField("Sync", syncRow,
      "The sound against the picture: a camera shows each moment a little late, so move the sound later (+) until lips and hands match. Set before going live; double-click: 0."));
    body.append(picture, over, destinations, quality);
    box.appendChild(body);

    const footer = element("div", "ls-footer");
    const go = element("button", "ls-go", onAirNow ? (stream.ending ? "Ending\u2026" : "End stream") : "Go live");
    go.type = "button";
    go.disabled = !stream.available || Boolean(stream.ending);
    go.classList.toggle("live", onAirNow);
    go.title = onAirNow ? "End the stream (each destination is closed properly)" : "Go live: the show as it plays, to every destination ticked (off air, its name only)";
    go.addEventListener("click", () => { saveStream(true); Hub.send("live", { action: "stream", on: !onAirNow }); });
    footer.appendChild(go);
    box.appendChild(footer);
    showStreamStatus();
  }

  const STATE_TEXT = { connecting: "connecting\u2026", live: "live", retrying: "trying again", failed: "failed", ended: "ended" };

  /** How the stream goes: live and how long, the encoder, each destination. */
  function showStreamStatus() {
    showStreamButton();
    const status = $("ls-status");
    if (!status) return;
    status.textContent = "";
    const stats = streamStats;
    const stream = streamOf();
    const line = (cls, text, title) => {
      const el = element("div", `ls-line ${cls}`.trim(), text);
      if (title) el.title = title;
      status.appendChild(el);
      return el;
    };
    if (stats && stats.source) line("warn", stats.source);
    if (!stats || (!stats.live && !(stats.outputs || []).length)) {
      if (stream.available) line("muted", live.onAir ? "Streaming enabled: ready to go live." : "Streaming not enabled: going live sends the show's name until you press Enable streaming.");
      return;
    }
    if (stats.live && !live.onAir) line("warn", "Streaming not enabled: viewers see the show's name. Press Enable streaming.");
    if (stats.live) line("on", `\u25cf LIVE ${formatClock(stats.seconds || 0)}${stats.encoder ? ` \u00b7 ${stats.encoder}` : ""}${stats.fps ? ` \u00b7 ${Math.round(stats.fps)} fps` : ""}`);
    for (const output of stats.outputs || []) {
      const row = element("div", `ls-out ${output.state}`);
      row.append(element("span", "ls-dot"), element("span", "ls-out-name", output.name),
                 element("span", "ls-out-state", `${STATE_TEXT[output.state] || output.state}${output.state === "live" && output.kbps ? ` \u00b7 ${output.kbps} kbps` : ""}`));
      row.title = output.error || "";
      status.appendChild(row);
    }
    if (stats.error) line("warn", stats.error);
  }

  /** The stream going out: how long, by the switches. */
  function showStreamButton() {
    const clock = $("live-stream-clock");
    if (!clock) return;
    const liveNow = live.mode === "stream" && Boolean((streamStats && streamStats.live) || streamOf().live);
    clock.hidden = !liveNow;
    if (liveNow) clock.textContent = `LIVE ${formatClock(streamStats ? streamStats.seconds || 0 : 0)}`;
  }

  // ---------------------------------------------------------------- the show's screen
  /** The show's screen as the picture in picture: a window of Sonify's own over every other window
      (Sonify's and the other apps', Sonify minimised too), moved by its bar and sized by its corner,
      with its own minimise, maximise, back-to-its-size and close (Sonify keeps where it was, how
      big, and whether it was left open). It opens by itself when a video comes into the player.
      The Screen button opens and closes it. */
  function openScreen(open, kept = true) {
    screenOpen = open;
    showScreen();
    Hub.send("live", { action: "screen", open, kept });
  }

  /** Its button, and its outputs' button. */
  function showScreen() {
    const button = $("live-screen-button");
    if (button) button.setAttribute("aria-pressed", String(screenOpen));
    showOutputs();
  }

  /** HDMI: lit while the picture is full screen on another screen. */
  function showOutputs() {
    const count = (live.screen && live.screen.outputs) || 0;
    const names = ((live.screen && live.screen.displays) || []).filter((d) => d.on).map((d) => d.name);
    const button = $("live-outputs-button");
    if (button) {
      button.classList.toggle("on", count > 0);
      button.title = count > 0 ? `The show's screen is full screen on ${names.join(", ")}. Click: the screens`
                               : "HDMI: the show's screen full screen on another screen (a projector, a TV)";
    }
    $("live-outputs-label").textContent = count > 0 ? `HDMI \u00b7 ${count}` : "HDMI";
  }

  /** The computer's other screens: the picture full screen on each one ticked. */
  function outputsMenu(anchor) {
    const displays = (live.screen && live.screen.displays) || [];
    const items = [{ group: "The show's screen, full screen on" }];
    if (!displays.length) items.push({ label: "No other screen found: connect a projector or a TV", value: null, disabled: true });
    for (const display of displays) items.push({ label: `${display.on ? "\u2713 " : "\u2003"}${display.name}`, value: display });
    items.push({ label: "Look for screens again", value: "again" });
    Hub.popupMenu(anchor, items, (choice) => {
      if (choice === "again") Hub.send("live", { action: "screenDisplays" });
      else if (choice && choice.id) Hub.send("live", { action: "screenOutput", id: choice.id, on: !choice.on });
    });
  }

  function wireScreen() {
    $("live-screen-button").addEventListener("click", () => openScreen(!screenOpen));
    $("live-outputs-button").addEventListener("click", (event) => outputsMenu(event.currentTarget));
  }

  // ---------------------------------------------------------------- the pads
  function renderPads() {
    const box = $("live-pads");
    if (!box) return;
    box.textContent = "";
    padViews = [];
    const head = element("div", "lp-head");
    head.append(element("span", "ld-caption", "Pads"), fader("master", 0, "pads", { across: true, label: "Pads" }));
    const grid = element("div", "lp-grid");
    for (let p = 0; p < 8; p++) {
      const pad = live.pads[p] || {};
      const button = smallButton("lp-pad", "", "");
      button.append(element("b", "", String(p + 1)), element("span", "", pad.loading ? "\u2026" : (pad.title || "")));
      button.classList.toggle("empty", !pad.title);
      linkable(button, `live.pad.${p + 1}`, false);
      button.title = pad.title ? `Pad ${p + 1}: ${pad.title} (${formatTime(pad.length || 0)}). Click: play it (again: stop it). Right-click: clear it.`
        : pad.loading ? `Loading ${pad.loading}\u2026` : `Pad ${p + 1}: drop a sound here from the library (a jingle, a sting, an effect)`;
      button.addEventListener("click", () => Hub.send("live", { action: "pad", pad: p }));
      button.addEventListener("contextmenu", (event) => {
        event.preventDefault();
        if (!pad.title) return;
        Hub.popupMenu(button, [{ group: `Pad ${p + 1}: ${pad.title}` }, { label: "Clear this pad", value: true }, { label: "Keep it", value: false }],
          (yes) => { if (yes) Hub.send("live", { action: "padClear", pad: p }); });
      });
      button.addEventListener("dragover", (event) => {
        if (!event.dataTransfer.types.includes("text/sonify-audio")) return;
        event.preventDefault();
        button.classList.add("drop");
      });
      button.addEventListener("dragleave", () => button.classList.remove("drop"));
      button.addEventListener("drop", (event) => {
        button.classList.remove("drop");
        const path = event.dataTransfer.getData("text/sonify-audio");
        if (!path) return;
        event.preventDefault();
        event.stopPropagation();
        Hub.send("live", { action: "padLoad", pad: p, path });
      });
      grid.appendChild(button);
      padViews.push(button);
    }
    box.append(head, grid);
    showPads();
  }

  function showPads() {
    padViews.forEach((button, p) => button.classList.toggle("playing", Boolean(padsPlaying[p])));
  }

  // ---------------------------------------------------------------- the library
  /** A track onto the first deck that isn't playing (an empty one first). */
  function loadOnFreeDeck(path, item = 0) {
    const free = [...Array(deckCount()).keys()];
    const empty = free.find((d) => !motion[d].playing && !(live.decks[d] || {}).title);
    const notPlaying = free.find((d) => !motion[d].playing);
    const deck = empty !== undefined ? empty : notPlaying;
    if (deck === undefined) Hub.toast("Every deck is playing: pause one first.");
    else Hub.send("live", { action: "load", deck, path, item });
  }

  // Sonify's videos (VideoMedia::wildcards).
  const VIDEO_FILE = /\.(mp4|m4v|mov|mkv|webm|avi|wmv|mpg|mpeg|flv|3gp|ts|mts|m2ts|vob|ogv)$/i;

  /** Onto the playlist. A video among them opens the show's screen: its picture is seen there. */
  function addToPlaylist(paths, at = -1) {
    Hub.send("live", { action: "playlistAdd", paths, at });
    if (single() && !screenOpen && paths.some((path) => VIDEO_FILE.test(String(path)))) openScreen(true, false);
  }

  function renderLibrary() {
    const files = $("live-files");
    const path = $("live-path");
    files.textContent = "";
    if (!listing) {
      path.textContent = "";
      return;
    }

    path.textContent = "";
    if (listing.parent) {
      const up = element("button", "plain-button small", "\u2191 Up");
      up.type = "button";
      up.addEventListener("click", () => openFolder(listing.parent));
      path.appendChild(up);
    }
    path.appendChild(element("span", "live-path-text", listing.path));

    const wanted = query.trim().toLowerCase();
    const match = (name, file = null) => !wanted || name.toLowerCase().includes(wanted)
      || Boolean(file && ((file.key || "").toLowerCase() === wanted || (file.bpm > 0 && String(Math.round(file.bpm)) === wanted)));
    const withPlaylist = modeIs("live", "stream");

    for (const folder of listing.folders || []) {
      if (!match(folder.name)) continue;
      const row = element("div", "lf-row folder");
      row.append(element("span", "lf-icon", "\u{1F4C1}"), element("span", "lf-name", folder.name));
      row.addEventListener("click", () => openFolder(folder.path));
      files.appendChild(row);
    }

    for (const file of listing.files || []) {
      // Videos are the playlist's (their picture on the show's screen); DJ show's decks play sound.
      if (file.video && !withPlaylist) continue;
      if (!match(file.name, file)) continue;
      const row = element("div", `lf-row file${file.video ? " video" : ""}`);
      row.draggable = true;
      const known = file.bpm > 0 ? `\n${formatBpm(file.bpm)} BPM${file.key ? `, ${file.key}` : ""}` : file.known ? "\nNo steady beat heard in it" : "";
      row.title = `${file.name}${file.video ? " (a video: its picture on the show's screen)" : ""}${known}\n`
        + (withPlaylist ? "Double-click: onto the playlist. Or drag it there."
                        : "Double-click: onto the first deck that isn't playing. Or drag it onto a deck, or a pad.");
      row.append(element("span", "lf-icon", file.video ? "\u{1F3AC}" : "\u266a"), element("span", "lf-name", file.name.replace(/\.[^.]+$/, "")),
                 element("span", "lf-bpm", file.bpm > 0 ? formatBpm(file.bpm) : file.known ? "\u2013" : ""), element("span", "lf-key", file.key || ""));
      const load = element("span", "lf-load");
      if (withPlaylist) {
        // The playlist's tabs: onto the playlist only (never straight into the player).
        const add = element("button", "lf-add", "+");
        add.type = "button";
        add.title = "Onto the playlist";
        add.addEventListener("click", (event) => { event.stopPropagation(); addToPlaylist([file.path]); });
        load.appendChild(add);
      } else {
        for (let d = 0; d < deckCount(); d++) {
          const button = element("button", "", String(d + 1));
          button.type = "button";
          button.style.setProperty("--deck", DECK_COLOURS[d]);
          button.title = `Load onto deck ${d + 1}`;
          button.addEventListener("click", (event) => {
            event.stopPropagation();
            Hub.send("live", { action: "load", deck: d, path: file.path });
          });
          load.appendChild(button);
        }
      }
      row.appendChild(load);
      row.addEventListener("dblclick", () => { if (withPlaylist) addToPlaylist([file.path]); else loadOnFreeDeck(file.path); });
      row.addEventListener("dragstart", (event) => {
        event.dataTransfer.setData("text/sonify-audio", file.path);
        event.dataTransfer.effectAllowed = "copy";
      });
      files.appendChild(row);
    }

    if (!files.children.length)
      files.appendChild(element("p", "live-empty", wanted ? "Nothing here by that name." : withPlaylist ? "No audio files or videos in this folder." : "No audio files in this folder."));
  }

  const openFolder = (path) => Hub.send("live", { action: "list", path: path || "" });

  // ---------------------------------------------------------------- the playlist
  const sendItem = (id, field, value) => Hub.send("live", { action: "playlistSet", id, field, value });

  /** The item in the player now, and the one ready behind it to come next (0: none). */
  function playerItems() {
    const front = frontDeck(), behind = 1 - front;
    const inFront = live.decks[front] || {}, back = live.decks[behind] || {};
    const now = inFront.title && !inFront.loading ? inFront.item || 0 : 0;
    const backPlayed = (live.playlist || []).some((item) => item.id === back.item && item.played);
    const next = back.title && !back.loading && !motion[behind].playing && back.item > 0 && back.item !== now && !backPlayed ? back.item : 0;
    return { now, next };
  }

  /** A thin line between a banner's controls. */
  const bannerSep = () => element("span", "lpl-sep");

  const playerPlaying = () => Boolean(motion[frontDeck()].playing);

  /** The play button's triangle (drawn: a font's play sign can come out as a coloured picture). */
  function playGlyph() {
    const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.setAttribute("viewBox", "0 0 10 10");
    svg.setAttribute("aria-hidden", "true");
    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("d", "M3.3 1.9 L8.3 5 L3.3 8.1 Z");
    svg.appendChild(path);
    return svg;
  }

  /** What a banner's play button does, as things are. */
  function playTitle(item, now, next) {
    if (item.missing) return "The file isn't there any more";
    if (now) return playerPlaying() ? "Playing now" : "In the player: Play starts it";
    const into = playerPlaying() ? "Into the player: what's playing stops (back at its start), Play starts this one" : "Into the player (Play starts it)";
    return next ? `It comes next (ready behind the player). ${into}` : into;
  }

  /** The player's item playing or not, on its banner (as the player starts and stops, without
      drawing the list again). */
  function showPlaylistPlaying() {
    const box = $("live-playlist");
    if (!box) return;
    const playing = playerPlaying();
    for (const row of box.querySelectorAll(".lpl-item")) {
      const item = (live.playlist || []).find((i) => i.id === Number(row.dataset.id));
      if (!item) continue;
      const now = row.classList.contains("now");
      row.classList.toggle("playing", now && playing);
      const button = row.querySelector(".lpl-play");
      if (button) button.title = playTitle(item, now, row.classList.contains("next"));
    }
  }

  /** The playlist's item picked to edit: lit (the one before it no longer), the list as it is. */
  function selectPlaylistItem(id) {
    selectedItem = id;
    const box = $("live-playlist");
    if (!box) return;
    for (const row of box.querySelectorAll(".lpl-item")) row.classList.toggle("selected", Number(row.dataset.id) === id);
  }

  function playlistItem(item, index, player) {
    const open = opened.has(item.id);
    const now = player.now === item.id, next = player.next === item.id;
    const row = element("div", `lpl-item${open ? " open" : ""}${now ? " now" : ""}${now && playerPlaying() ? " playing" : ""}${next ? " next" : ""}`
                               + `${item.missing ? " missing" : ""}${item.played && !now ? " played" : ""}${item.id === selectedItem ? " selected" : ""}`);
    row.setAttribute("role", "listitem");
    row.dataset.id = String(item.id);
    row.addEventListener("click", (event) => {
      if (event.target.closest(".lpl-play, .lpl-remove")) return;
      selectPlaylistItem(item.id);
    });

    // Its banner: its place, its play button, its name (a video marked), its tempo and key, its
    // timing, Next when it comes next, its options' arrow, and its x (off the list) in the corner.
    const banner = element("div", "lpl-banner");

    // Its play button (as Playlisted's): into the player, where Play starts it; one playing stops,
    // back at its start. It never starts it itself: the player's Play does. Lit while it's in the player.
    const cue = element("button", "lpl-play");
    cue.type = "button";
    cue.appendChild(playGlyph());
    cue.title = playTitle(item, now, next);
    cue.disabled = Boolean(item.missing);
    cue.addEventListener("click", (event) => {
      event.stopPropagation();
      if (event.detail > 1) return;   // a double-click's second click: once is enough
      Hub.send("live", { action: "playlistCue", id: item.id });
    });
    cue.addEventListener("dblclick", (event) => event.stopPropagation());
    banner.draggable = true;
    const grip = element("span", "lpl-grip", "\u2807");
    grip.title = "Drag it up or down the list";
    const name = element("span", "lpl-name", item.title || "");
    name.title = item.missing ? `${item.path}\nThe file isn't there any more.` : item.path;
    const kind = element("span", "lpl-kind", item.video ? "\u{1F3AC}" : "");
    kind.title = item.video ? "A video: its picture on the show's screen" : "";
    const shift = Math.round(item.key || 0);
    const key = keyName(item.keyTonic, item.keyMinor, shift);
    const tempo = element("span", "lpl-bpm", item.bpm > 0 ? `${formatBpm(item.bpm)}${item.sync ? " \u2192" : ""}` : "");
    tempo.title = item.bpm > 0 ? (item.sync ? "Its tempo, played at the player's: the song's (Sync to player)" : "Its tempo, played as recorded") : "";
    const keyChip = element("span", `lpl-key${shift !== 0 ? " moved" : ""}`, key);
    const timing = Number(item.timing) || 0;
    const timingChip = element("span", `lpl-timing${timing < 0 ? " cross" : ""}`, timing ? timingText(timing) : "");
    timingChip.title = timing < 0 ? `Crossfades into the next one over its last ${-timing} s` : timing > 0 ? `The next one starts ${timing} s after it ends` : "";
    const nextChip = element("span", "lpl-next", next ? "Next" : "");
    nextChip.title = next ? "It plays next: it's ready behind the player" : "";
    const arrow = smallButton("lpl-open", open ? "\u25b4" : "\u25be", open ? "Hide its options" : "Its options: its tempo, Sync, Key, Next and Volume", () => {
      if (opened.has(item.id)) opened.delete(item.id); else opened.add(item.id);
      renderPlaylist();
    });
    const remove = smallButton("lpl-remove", "\u00d7", "Off the playlist", () => Hub.send("live", { action: "playlistRemove", id: item.id }));
    banner.append(grip, element("span", "lpl-number", String(index + 1)), cue, kind, name, tempo, keyChip, timingChip, nextChip, arrow, remove);
    banner.title = (now ? "In the player. " : "Its play button alone puts it in the player (what's playing stops, back at its start). ")
      + "Click: pick it to edit (lit); double-click: its options";
    // A click picks it to edit (lit), never into the player: its play button alone does that. A
    // double-click opens (or closes) its options.
    banner.addEventListener("dblclick", (event) => {
      if (event.target.closest("button, input")) return;
      selectPlaylistItem(item.id);
      if (opened.has(item.id)) opened.delete(item.id); else opened.add(item.id);
      renderPlaylist();
    });
    banner.addEventListener("dragstart", (event) => {
      event.dataTransfer.setData("text/sonify-item", String(item.id));
      event.dataTransfer.effectAllowed = "move";
    });
    row.appendChild(banner);

    if (open) {
      const options = element("div", "lpl-options");

      // Its tempo: the track's own (heard, or typed), and Sync to the player's tempo slider.
      const tempoBox = element("div", "lpl-field");
      const bpm = numberBox("lpl-bpm-input", `${item.title}: its tempo`, (value) => Hub.send("live", { action: "playlistBpm", id: item.id, bpm: value }));
      bpm.value = item.bpm > 0 ? formatBpm(item.bpm) : "";
      bpm.placeholder = "?";
      bpm.title = "The track's own tempo (BPM): Sonify hears it, or type it";
      tempoBox.append(element("span", "ld-caption", "BPM"), bpm);
      const syncBox = element("div", "lpl-field");
      const sync = smallButton("lv-switch lpl-sync", "Sync to player", "Sync to player: the track plays at the player's tempo, the song's (the player's SYNC lights with it). Off: as recorded",
        () => sendItem(item.id, "sync", item.sync ? 0 : 1));
      sync.setAttribute("aria-pressed", String(Boolean(item.sync)));
      syncBox.appendChild(sync);

      // Its key, in semitones.
      const keyBox = element("div", "lpl-field");
      const keyText = element("span", `ld-key-value${shift !== 0 ? " moved" : ""}`, shift === 0 ? "0" : `${shift > 0 ? "+" : "\u2212"}${Math.abs(shift)}`);
      keyText.title = "Key: up or down in semitones (double-click: as recorded)";
      keyText.addEventListener("dblclick", () => sendItem(item.id, "key", 0));
      const down = smallButton("ld-small ld-step", "\u2212", "A semitone down", () => sendItem(item.id, "key", Math.max(-12, shift - 1)));
      const up = smallButton("ld-small ld-step", "+", "A semitone up", () => sendItem(item.id, "key", Math.min(12, shift + 1)));
      if (!live.canKeepKey) down.disabled = up.disabled = true;
      keyBox.append(element("span", "ld-caption", "Key"), down, keyText, up);

      // When the next one comes: at its end (0), seconds after (a pause), or before (below 0: a crossfade).
      const timingBox = element("div", "lpl-field");
      const timingInput = numberBox("lpl-timing-input", `${item.title}: when the next one comes`, (value) => sendItem(item.id, "timing", Math.max(-60, Math.min(60, value))), true);
      timingInput.value = String(timing);
      timingInput.title = "When the next track comes, in seconds: 0 at this one's end, 3 a 3-second pause after it, -5 a crossfade over its last 5 seconds (it fades out as the next fades in)";
      timingBox.append(element("span", "ld-caption", "Next"), timingInput, element("span", "lpl-unit", "s"));

      // Its volume: silence to as recorded (the middle), then up to +25 dB.
      const volumeBox = element("div", "lpl-field lpl-volume");
      const volume = element("input", "lpl-range");
      volume.type = "range";
      volume.min = "0";
      volume.max = "1000";
      volume.value = String(Math.round((item.volume !== undefined ? item.volume : 0.5) * 1000));
      const volumeText = element("span", "lpl-value", trackVolumeText(Number(volume.value) / 1000));
      volume.title = "Its volume: silence on the left, as recorded in the middle, +25 dB on the right (double-click: as recorded)";
      let volumeTimer = 0;
      volume.addEventListener("input", () => {
        volumeText.textContent = trackVolumeText(Number(volume.value) / 1000);
        clearTimeout(volumeTimer);
        volumeTimer = setTimeout(() => sendItem(item.id, "volume", Number(volume.value) / 1000), 60);
      });
      // Held, the page waits to draw again (the slider would go from under the pointer).
      volume.addEventListener("pointerdown", () => {
        dragging++;
        const release = () => {
          window.removeEventListener("pointerup", release);
          window.removeEventListener("pointercancel", release);
          dragging = Math.max(0, dragging - 1);
          if (renderDue && !dragging) setTimeout(render, 0);
        };
        window.addEventListener("pointerup", release);
        window.addEventListener("pointercancel", release);
      });
      volume.addEventListener("change", () => sendItem(item.id, "volume", Number(volume.value) / 1000));
      volume.addEventListener("dblclick", () => { volume.value = "500"; volumeText.textContent = trackVolumeText(0.5); sendItem(item.id, "volume", 0.5); });
      volumeBox.append(element("span", "ld-caption", "Vol"), volume, volumeText);

      // The controls a line apart: tempo | Sync | Key | Next, the volume under them.
      const controls = element("div", "lpl-row");
      controls.append(tempoBox, bannerSep(), syncBox, bannerSep(), keyBox, bannerSep(), timingBox);
      const level = element("div", "lpl-row");
      level.appendChild(volumeBox);
      options.append(controls, level);
      row.appendChild(options);
    }
    return row;
  }

  /** A track's timing, short: "\u22125s" (a crossfade), "+3s" (a pause). */
  const timingText = (seconds) => `${seconds > 0 ? "+" : "\u2212"}${trim(Math.abs(seconds).toFixed(1))}s`;

  function renderPlaylist() {
    const box = $("live-playlist");
    if (!box) return;
    box.textContent = "";
    const items = live.playlist || [];
    $("live-playlist-count").textContent = (live.playlistFile ? `${live.playlistFile} \u00b7 ` : "") + (items.length ? `${items.length} track${items.length === 1 ? "" : "s"}` : "");
    const auto = $("live-playlist-auto");
    auto.setAttribute("aria-checked", String(live.playlistAuto));
    const player = playerItems();
    items.forEach((item, index) => box.appendChild(playlistItem(item, index, player)));
    if (!items.length)
      box.appendChild(element("p", "live-empty", "Drop tracks and videos here, from the library or straight from a folder on the computer (or double-click them in the library). With Auto on they play in turn, each after the one before; a track's play button puts it in the player (Play starts it). Each one opens to show its tempo, Sync, Key, when the next comes and its Volume."));
  }

  /** Where a drop lands in the list: the item it's over (its upper half: before it). */
  function dropPlace(event) {
    const rows = [...$("live-playlist").querySelectorAll(".lpl-item")];
    for (let i = 0; i < rows.length; i++) {
      const r = rows[i].getBoundingClientRect();
      if (event.clientY < r.top + r.height / 2) return i;
    }
    return rows.length;
  }

  function wirePlaylist() {
    const box = $("live-playlist");
    box.addEventListener("dragover", (event) => {
      const types = event.dataTransfer.types;
      if (!types.includes("text/sonify-audio") && !types.includes("text/sonify-item")) return;
      event.preventDefault();
      box.classList.add("drop");
    });
    box.addEventListener("dragleave", (event) => { if (!box.contains(event.relatedTarget)) box.classList.remove("drop"); });
    box.addEventListener("drop", (event) => {
      box.classList.remove("drop");
      const at = dropPlace(event);
      const id = Number(event.dataTransfer.getData("text/sonify-item")) || 0;
      event.preventDefault();
      if (id) {
        const from = (live.playlist || []).findIndex((item) => item.id === id);
        if (from < 0) return;
        const to = Math.max(0, Math.min((live.playlist || []).length - 1, at > from ? at - 1 : at));
        if (to !== from) Hub.send("live", { action: "playlistMove", id, to });
        return;
      }
      const path = event.dataTransfer.getData("text/sonify-audio");
      if (path) addToPlaylist([path], at);
    });

    // Files dragged in straight from a folder on the computer (Explorer): the page can't learn
    // where they are, Sonify can, so it lays a drop zone of its own over the playlist for them.
    const panel = box.closest(".live-panel") || box;
    const fromComputer = (event) => {
      const types = event.dataTransfer ? [...event.dataTransfer.types] : [];
      return types.includes("Files") && !types.includes("text/sonify-audio") && !types.includes("text/sonify-item");
    };
    const askSonify = () => {
      const now = performance.now();
      if (now - fileDragSent < 400 || !single()) return;
      fileDragSent = now;
      const r = panel.getBoundingClientRect();
      Hub.send("live", { action: "fileDrag", left: r.left, top: r.top, width: r.width, height: r.height });
    };
    panel.addEventListener("dragenter", (event) => { if (fromComputer(event)) askSonify(); });
    panel.addEventListener("dragover", (event) => {
      if (!fromComputer(event)) return;
      event.preventDefault();
      event.dataTransfer.dropEffect = "copy";
      askSonify();
    });
    panel.addEventListener("drop", (event) => {
      if (!fromComputer(event)) return;
      event.preventDefault();
      Hub.toast("Hold the files over the playlist a moment before letting go: Sonify takes them there.");
    });

    // Anywhere else in the studio a file from the computer isn't taken (nor opened in the page's place).
    const studio = $("live-studio");
    studio.addEventListener("dragover", (event) => {
      if (!fromComputer(event) || panel.contains(event.target)) return;
      event.preventDefault();
      event.dataTransfer.dropEffect = "none";
    });
    studio.addEventListener("drop", (event) => { if (fromComputer(event)) event.preventDefault(); });
    $("live-playlist-auto").addEventListener("click", () => {
      live.playlistAuto = !live.playlistAuto;
      $("live-playlist-auto").setAttribute("aria-checked", String(live.playlistAuto));
      Hub.send("live", { action: "playlistAuto", on: live.playlistAuto });
    });
    $("live-playlist-open").addEventListener("click", () => Hub.send("live", { action: "playlistOpen" }));
    $("live-playlist-save").addEventListener("click", () => Hub.send("live", { action: "playlistSave" }));
    $("live-playlist-clear").addEventListener("click", (event) => {
      if (!(live.playlist || []).length) return;
      Hub.popupMenu(event.currentTarget, [{ group: "Empty the playlist? The player empties too (a track playing stops)" },
                                           { label: "Clear the playlist", value: true }, { label: "Keep it", value: false }],
        (yes) => { if (yes) Hub.send("live", { action: "playlistClear" }); });
    });
  }

  // ---------------------------------------------------------------- the page
  function renderHead() {
    const studio = $("live-studio");
    for (const mode of MODES) studio.classList.toggle(`mode-${mode}`, live.mode === mode);
    for (const tab of studio.querySelectorAll(".live-tab")) tab.setAttribute("aria-selected", String(tab.dataset.mode === live.mode));
    for (const el of studio.querySelectorAll("[data-modes]")) el.hidden = !el.dataset.modes.split(" ").includes(live.mode);

    // The rail's first group shown has no line over it.
    let first = true;
    for (const group of studio.querySelectorAll(".live-rail > .rail-group")) {
      group.classList.toggle("first", first && !group.hidden);
      if (!group.hidden) first = false;
    }

    // The show's switches.
    const pill = (id, on) => {
      const button = $(id);
      button.setAttribute(button.getAttribute("role") === "switch" ? "aria-checked" : "aria-pressed", String(on));
      button.classList.toggle("on", on);
    };
    pill("live-onair", live.onAir);
    pill("live-mute-mics", live.micsMuted);
    pill("live-mute-playback", live.playbackMuted);
    pill("live-controller-button", live.controllerOpen);
    pill("live-monitor-button", live.monitorOpen);
    document.body.classList.toggle("live-on-air", live.mode === "stream" && live.onAir);
    for (const button of studio.querySelectorAll("[data-decks]")) {
      const four = button.dataset.decks === "1";
      button.setAttribute("aria-checked", String(four === (deckCount() === 4)));
    }
    const rec = $("live-rec");
    rec.setAttribute("aria-checked", String(live.recording));
    rec.classList.toggle("on", live.recording);
    if (!live.recording) $("live-rec-time").textContent = "";
    $("live-stems").setAttribute("aria-checked", String(live.stems));
    showPodcast();
    showStreamButton();

    // The stream's panel belongs to the tab that has it; the show's screen to the playlist's tabs
    // (it stays open for them).
    if (streamOpen && live.mode !== "stream") openStream(false);
    showScreen();
  }

  function showPodcast() {
    const button = $("live-podcast");
    const busy = live.podcastBusy || podcastProgress >= 0;
    button.textContent = busy ? `Podcast ${Math.round(Math.max(0, podcastProgress) * 100)}%` : "Podcast\u2026";
    button.classList.toggle("busy", busy);
  }

  function podcastMenu(anchor) {
    if (live.podcastBusy) {
      Hub.toast("A recording is being made ready already: a moment.");
      return;
    }
    const last = live.lastTake ? live.lastTake.split(/[\\/]/).slice(-2).join(" \u203a ") : "";
    const items = [];
    if (last) {
      items.push({ group: `The last recording: ${last}` });
      items.push({ label: "For a podcast (\u221216 LUFS)", value: { lufs: -16 } });
      items.push({ label: "For streams and video (\u221214 LUFS)", value: { lufs: -14 } });
    }
    items.push({ group: "Another recording" });
    items.push({ label: "Choose one, for a podcast (\u221216 LUFS)\u2026", value: { lufs: -16, choose: true } });
    items.push({ label: "Choose one, for streams and video (\u221214 LUFS)\u2026", value: { lufs: -14, choose: true } });
    if (live.podcastFile) {
      items.push({ group: "Ready" });
      items.push({ label: `Show ${live.podcastFile.split(/[\\/]/).pop()} in its folder`, value: { reveal: live.podcastFile } });
    }
    if (live.lastTake) items.push({ label: "Show the recording in its folder", value: { reveal: live.lastTake } });
    Hub.popupMenu(anchor, items, (choice) => {
      if (!choice) return;
      if (choice.reveal) Hub.send("live", { action: "reveal", path: choice.reveal });
      else Hub.send("live", Object.assign({ action: "podcast" }, choice));
    });
  }

  function render() {
    if (dragging || typing()) {
      renderDue = true;
      return;
    }
    renderDue = false;
    meterViews = new Map();
    refreshers = [];
    for (const canvas of observed) resizer.unobserve(canvas);
    observed = [];
    renderHead();

    // The decks: DJ show's 1 (and 3) on the left, 2 (and 4) on the right; the playlist's tabs:
    // the one player (the deck in front), the mixer on its left.
    const left = $("live-left"), right = $("live-right");
    left.textContent = "";
    right.textContent = "";
    const one = single();
    const count = deckCount();
    $("live-stage").classList.toggle("four", !one && count === 4);
    $("live-stage").classList.toggle("single", one);
    deckViews = [];
    if (one) {
      const d = frontDeck();
      const view = deckView(d);
      deckViews[d] = view;
      right.appendChild(view.root);
      showDeckInfo(d);
      showDeckButtons(d);
    } else {
      for (let d = 0; d < count; d++) {
        const view = deckView(d);
        deckViews[d] = view;
        (d % 2 === 0 ? left : right).appendChild(view.root);
        showDeckInfo(d);
        showDeckButtons(d);
      }
    }

    renderMixer();
    if (modeIs("live", "stream")) {
      renderChannels();
      renderPlaylist();
    }
    if (effectsOpen) renderEffects();
    if (modeIs("dj")) renderPads();
    renderLibrary();
    renderStream();
    showScreen();
  }

  function show(on) {
    if (on === shown) return;
    shown = on;
    if (!on) dragging = 0;
    Hub.send("live", { action: "shown", shown: on });
    if (streamOpen) Hub.send("live", { action: "streamPanel", open: on });
    if (on) {
      // The waveforms the page already has don't come again.
      Hub.send("live", { action: "state", have: waves.map((w) => (w ? w.file : "")) });
      if (!listing) openFolder("");
      if (renderDue) render();
      if (!frame) frame = requestAnimationFrame(animate);
    } else if (frame) {
      cancelAnimationFrame(frame);
      frame = 0;
    }
  }

  function setMode(mode) {
    if (!MODES.includes(mode) || mode === live.mode) return;
    if (live.mode === "stream" && (streamOf().live || (streamStats && streamStats.live))) {
      Hub.toast("The stream is live: end it first (Stream settings), then change how the show is used.");
      return;
    }
    live.mode = mode;
    if (mode !== "dj") live.master.decks = 0;   // the playlist plays two decks by turns: one player
    Hub.send("live", { action: "mode", mode });
    render();
  }

  // ---------------------------------------------------------------- from Sonify
  Hub.on("live", (data) => {
    if (data.kind === "state") {
      live = {
        on: Boolean(data.on),
        mode: MODES.includes(data.mode) ? data.mode : "dj",
        onAir: Boolean(data.onAir),
        micsMuted: data.micsMuted !== false,
        playbackMuted: data.playbackMuted !== false,
        controllerOpen: Boolean(data.controllerOpen),
        monitorOpen: Boolean(data.monitorOpen),
        songTempo: data.songTempo || live.songTempo,
        canKeepKey: data.canKeepKey !== false,
        specs: data.specs || live.specs,
        master: data.master || {},
        decks: data.decks || [],
        channels: data.channels || [],
        pads: data.pads || [],
        playlist: data.playlist || [],
        playlistAuto: data.playlistAuto !== false,
        playlistFile: data.playlistFile || "",
        outputs: data.outputs || [],
        phonesOutput: data.phonesOutput || 0,
        recording: Boolean(data.recording),
        stems: data.stems !== false,
        lastTake: data.lastTake || "",
        podcastBusy: Boolean(data.podcastBusy),
        podcastFile: data.podcastFile || "",
        stream: data.stream || live.stream || null,
        front: data.front === 1 ? 1 : 0,
        screen: data.screen || { displays: [], outputs: 0 }
      };
      if (!live.podcastBusy) podcastProgress = -1;
      // A value still on its way to Sonify stays as the page has it.
      for (const change of pending.values()) {
        const values = valuesOf(change.target, change.index);
        if (values) values[change.id] = change.value;
      }
      for (const group of ["deck", "channel", "master"])
        specMaps[group] = new Map((live.specs[group] || []).map((s) => [s.id, s]));
      // A deck that's lost its track forgets its waveform.
      live.decks.forEach((deck, d) => { if (!deck.file && waves[d]) waves[d] = null; });
      for (const id of [...opened]) if (!live.playlist.some((item) => item.id === id)) opened.delete(id);
      if (selectedItem && !live.playlist.some((item) => item.id === selectedItem)) selectedItem = 0;
      // The picture in picture as Sonify has it (closed by its own button, opened for a video).
      screenOpen = Boolean(live.screen.pip);
      if (data.note) {
        $("live-note").textContent = data.note;
        clearTimeout(noteTimer);
        noteTimer = setTimeout(() => { $("live-note").textContent = ""; }, 12000);
      }
      renderHead();
      if (shown) render(); else renderDue = true;
      // Typing in a box, the rest waits; the decks' words show now.
      if (renderDue) for (let d = 0; d < deckViews.length; d++) showDeckInfo(d);
    } else if (data.kind === "wave") {
      const d = data.deck;
      if (d < 0 || d > 3) return;
      if (!data.data) {
        waves[d] = null;
        return;
      }
      const binary = atob(data.data);
      const bytes = new Uint8Array(binary.length);
      for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
      waves[d] = { file: data.file, length: data.length, perSecond: data.perSecond, data: bytes, overview: null, overviewKey: "" };
      motion[d] = { position: 0, at: performance.now(), playing: false, speed: motion[d].speed, bpm: motion[d].bpm, loop: null };
      showDeckButtons(d);
    } else if (data.kind === "preview") {
      if (data.tall) {
        previewTallData = `data:image/jpeg;base64,${data.data}`;
        const tallImage = $("ls-preview-tall-image");
        if (tallImage) tallImage.src = previewTallData;
      } else {
        previewData = `data:image/jpeg;base64,${data.data}`;
        const image = $("ls-preview-image");
        if (image) image.src = previewData;
      }
    } else if (data.kind === "fileDrop") {
      // Dropped from a folder over the playlist (Sonify's drop zone): there in the list.
      const paths = Array.isArray(data.paths) ? data.paths : [];
      if (paths.length) addToPlaylist(paths, dropPlace({ clientY: Number(data.y) || 0 }));
    } else if (data.kind === "values") {
      // The settings as a controller (or Sonify, following) moved them: each control shows its own.
      const fourBefore = deckCount() === 4;
      if (data.master) live.master = data.master;
      (data.decks || []).forEach((values, d) => { if (live.decks[d]) live.decks[d].values = values; });
      for (const channel of data.channels || []) {
        const known = live.channels.find((c) => c.id === channel.id);
        if (known) known.values = channel.values;
      }
      for (const change of pending.values()) {
        const values = valuesOf(change.target, change.index);
        if (values) values[change.id] = change.value;
      }
      if (fourBefore !== (deckCount() === 4)) {
        if (shown) render(); else renderDue = true;
        return;
      }
      for (const refresh of refreshers) refresh();
      for (let d = 0; d < deckViews.length; d++) showDeckInfo(d);
    } else if (data.kind === "listing") {
      listing = data;
      if (shown) renderLibrary();
    }
  });

  Hub.on("status", (status) => {
    const levels = status.live;
    if (!levels) return;
    const now = performance.now();
    (levels.decks || []).forEach((deck, d) => {
      const m = motion[d];
      const was = m.playing, speedWas = m.speed;
      m.position = deck.position;
      m.at = now;
      m.playing = Boolean(deck.playing);
      m.speed = deck.speed;
      m.bpm = deck.bpm || 0;
      m.loop = Array.isArray(deck.loop) ? deck.loop : null;
      if (was !== m.playing) {
        showDeckButtons(d);
        if (d < 2 && single()) showPlaylistPlaying();
      }
      if (speedWas !== m.speed) showDeckInfo(d);   // its tempo now
      else showDeckMotion(d);
      if (!single()) feedMeter(`deck${d}`, deck.meter, now);
    });
    // The playlist's one player: its channel's meter, the louder of its two decks (both while the
    // next crossfades in).
    if (single() && Array.isArray(levels.decks) && levels.decks.length >= 2) {
      const silent = [-100, -100, -100, -100];
      const a = levels.decks[0].meter || silent, b = levels.decks[1].meter || silent;
      feedMeter("deck0", a.map((value, i) => Math.max(value, b[i] !== undefined ? b[i] : -100)), now);
    }
    if (live.recording && levels.recorded !== undefined) $("live-rec-time").textContent = formatClock(levels.recorded);
    if (levels.stream || streamStats) {
      streamStats = levels.stream || null;
      showStreamStatus();
    }
    const progress = levels.podcast !== undefined ? levels.podcast : -1;
    if (progress !== podcastProgress) {
      podcastProgress = progress;
      showPodcast();
    }
    if (Array.isArray(levels.pads)) {
      const changed = levels.pads.some((playing, p) => Boolean(playing) !== Boolean(padsPlaying[p]));
      padsPlaying = levels.pads;
      if (changed) showPads();
    }
    for (const [id, values] of Object.entries(levels.channels || {})) feedMeter(`ch${id}`, values, now);
    feedMeter("master", levels.master, now);
    const led = $("live-limit-led");
    if (led) led.classList.toggle("lit", levels.limiter > 0.5);
    recordLimiter(levels, now);
    showEffectsMeters(levels);
    if (levels.ducker) duckLevels = levels.ducker;
    live.on = Boolean(levels.on);
    if (levels.songTempo) live.songTempo = levels.songTempo;   // (the transport shows it, under the page)
  });

  Hub.on("tracks", () => {
    if (shown && !dragging && modeIs("live", "stream")) renderChannels(); else renderDue = true;
    // The dynamic EQ's chips carry the tracks' names and colours.
    if (shown && !dragging && !typing() && duckerView) renderEffects();
  });

  Hub.live = {
    show,
    isShown: () => shown,
    isOnAir: () => live.mode === "stream" && live.onAir,
    targetText,
    isContinuous
  };

  Hub.on("ready", () => {
    for (const tab of document.querySelectorAll("#live-studio .live-tab")) tab.addEventListener("click", () => setMode(tab.dataset.mode));
    $("live-onair").addEventListener("click", () => {
      live.onAir = !live.onAir;
      renderHead();
      Hub.send("live", { action: "on", on: live.onAir });
    });
    $("live-mute-mics").addEventListener("click", () => {
      live.micsMuted = !live.micsMuted;
      renderHead();
      Hub.send("live", { action: "muteMics", on: live.micsMuted });
    });
    $("live-mute-playback").addEventListener("click", () => {
      live.playbackMuted = !live.playbackMuted;
      renderHead();
      Hub.send("live", { action: "mutePlayback", on: live.playbackMuted });
    });
    $("live-controller-button").addEventListener("click", () => {
      Hub.send("live", { action: "controller", open: !live.controllerOpen });
    });
    $("live-monitor-button").addEventListener("click", () => {
      Hub.send("live", { action: "monitor", open: !live.monitorOpen });
    });
    linkable($("live-onair"), "live.onair", false);
    linkable($("live-mute-mics"), "live.mutemics", false);
    linkable($("live-mute-playback"), "live.muteplayback", false);
    linkable($("live-rec"), "live.record", false);

    // A button the mouse pressed lets the keys go: Enter later never presses it again by surprise
    // (Enable streaming, a mute or Play in the middle of a show). Tab still reaches each one from the keyboard.
    $("live-studio").addEventListener("click", (event) => {
      const button = event.target instanceof Element ? event.target.closest("button") : null;
      if (button && event.detail > 0 && document.activeElement === button) button.blur();
    });

    // A drag that never heard its end (the button let go outside the window, or the control
    // drawn again under it) would keep the page from drawing again: once no button is down,
    // nothing is held.
    const letGo = () => {
      if (!dragging && !markerDrag) return;
      dragging = 0;
      markerDrag = null;
      if (duckerView) duckerView.drag = null;
      if (renderDue && shown) render();
    };
    window.addEventListener("pointerup", (event) => { if (event.buttons === 0) setTimeout(letGo, 0); }, true);
    window.addEventListener("pointermove", (event) => { if (event.buttons === 0 && (dragging || markerDrag)) setTimeout(letGo, 0); }, true);
    window.addEventListener("blur", () => setTimeout(letGo, 0));
    $("live-rec").addEventListener("click", () => Hub.send("live", { action: "record", on: !live.recording, stems: live.stems }));
    $("live-stems").addEventListener("click", () => {
      if (live.recording) {
        Hub.toast("Stems are chosen before recording starts.");
        return;
      }
      live.stems = !live.stems;
      renderHead();
      Hub.send("live", { action: "record", on: false, stems: live.stems });
    });
    $("live-podcast").addEventListener("click", (event) => podcastMenu(event.currentTarget));
    $("live-stream-button").addEventListener("click", () => openStream(!streamOpen));
    for (const button of document.querySelectorAll("#live-studio [data-decks]")) {
      button.addEventListener("click", () => {
        const s = spec("master", "decks");
        if (!s) return;
        setValue("master", 0, s, Number(button.dataset.decks));
        render();
      });
    }
    $("live-add-channel").addEventListener("click", (event) => addChannelMenu(event.currentTarget));
    wireEffects();
    wireScreen();
    $("live-browse").addEventListener("click", () => Hub.send("live", { action: "browse" }));
    $("live-analyze").addEventListener("click", () => {
      if (listing) Hub.send("live", { action: "analyze", path: listing.path });
    });
    $("live-search").addEventListener("input", (event) => {
      query = event.target.value;
      renderLibrary();
    });
    wirePlaylist();
  });
})();
