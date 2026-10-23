// D:\workspace\Fanan_web_hub\src\hub-songchannel.js
//
// The Master Song Channel above the tracks, as Studio One's global tracks: a lane along the
// whole song that scrolls and zooms with the arrangement (View > Master Song Channel, or the
// button over the track list). Its tabs, over its header, pick what the lane shows, one at a
// time, with the whole height for it (drag its bottom edge for more or less):
//
//   Signature    time signature changes, each from a bar. Double-click empty space to add one
//                at that bar, double-click one to change it, drag one to another bar.
//   Key          key and scale changes, each from a bar, the same way (MIDI tools follow them).
//   Chords       the chord track: chords at song positions, each until the next. Double-click
//                empty space to add one there (snapped), double-click one to change it (type
//                Am7, C/E, F#m7b5...), drag one along. Right-click: Detect Chords from the
//                selected MIDI clips (the lane fills itself over them; correct it after), and the
//                Chord Pads: eight pads with the chords of the key where the song is, to hear
//                them on the selected track or (Write) put them on the lane where the song is.
//   Tempo        the tempo along the song, on a scale from 20 to 250 BPM. Click to add a change
//                (its height is its tempo), drag changes (snapped; Alt for free; Shift moves
//                the tempo a tenth as far, in hundredths), double-click one to type its tempo or
//                make it glide (ramp) to the next.
//   Volume       an envelope for Main's fader: click to add points and drag them. With points,
//                Main's fader follows it while the song plays.
//   Delete, the Eraser tool or right-click > Remove take a change or point away; the song's
//   first time signature, key and tempo stay. Right-click for the rest.
// Each change is one Undo step. The header shows what holds where the song is. The tab and the
// height are kept with the song's view.

(() => {
  "use strict";

  const { $ } = Hub;
  const TABS = [
    { id: "signature", name: "Signature", title: "Time signature changes along the song" },
    { id: "key", name: "Key", title: "Key and scale changes along the song" },
    { id: "chords", name: "Chords", title: "The chord track: chords along the song" },
    { id: "tempo", name: "Tempo", title: "The tempo along the song, 20 to 250 BPM" },
    { id: "main", name: "Volume", title: "Main's volume along the song" }
  ];
  const DEFAULT_HEIGHT = 112;     // the channel's height: its tabs and header on the left, the lane on the right
  const MIN_HEIGHT = 76;
  const MAX_HEIGHT = 360;
  const GRAB = 7;                 // px within which a point is grabbed
  const ACCENT = "#25984d";
  const LINE = "#4fc27a";
  const NUMERATORS = 32;
  const DENOMINATORS = [1, 2, 4, 8, 16, 32];

  let shown = false;
  let tab = "tempo";              // what the lane shows
  let height = DEFAULT_HEIGHT;
  let selected = null;            // { lane, index }
  let drag = null;                // what a press in the lanes is doing
  let previewMap = null;          // the map as a drag would leave it
  let previewMain = null;         // Main Volume's points as a drag would leave them
  let markers = [];               // where the signature and key chips were drawn, for hits
  let editor = null;              // the small editor that types a value

  const tempoMap = () => previewMap || Hub.tempoMap;
  const chordName = (chord) => Hub.TempoMap.chordName(chord);
  const mainPoints = () => previewMain || Hub.songChannel.mainAutomation || [];
  const totalHeight = () => height;
  const laneHeight = () => height;
  const laneAt = (y) => (y >= 0 && y < height ? tab : null);

  const xOf = (ppq) => Hub.arrange.xOf(ppq);
  const bpmText = (bpm) => (Math.abs(bpm - Math.round(bpm)) < 0.005 ? String(Math.round(bpm)) : bpm.toFixed(2));

  // ---------------------------------------------------------------- scales of the two envelopes
  /** The tempo lane's scale: every tempo a song can have, 20 to 250 BPM, always the same. */
  const tempoRange = () => ({ lo: Hub.TempoMap.MIN_TEMPO, hi: Hub.TempoMap.MAX_TEMPO });
  const PAD = 8;                  // px above and below the tempo and volume lines' room

  function tempoY(bpm) {
    const { lo, hi } = tempoRange(), room = laneHeight() - 2 * PAD;
    return PAD + (1 - (bpm - lo) / Math.max(1, hi - lo)) * room;
  }

  /** The tempo at a height: whole BPM, or hundredths (fine). */
  function tempoAtY(y, fine) {
    const { lo, hi } = tempoRange(), room = laneHeight() - 2 * PAD;
    const bpm = lo + (1 - Hub.clamp01((y - PAD) / room)) * (hi - lo);
    const step = fine ? 0.01 : 1;
    return Hub.clamp(Math.round(bpm / step) * step, lo, hi);
  }

  function mainY(db) {
    const room = laneHeight() - 2 * PAD;
    return PAD + (1 - Hub.dbToFader(db)) * room;
  }

  function mainAtY(y) {
    const room = laneHeight() - 2 * PAD;
    return Hub.clamp(Math.round(Hub.faderToDb(1 - Hub.clamp01((y - PAD) / room)) * 10) / 10, Hub.SILENT_DB, Hub.MAX_DB);
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
    if (!shown || !Hub.arrange || !Hub.arrange.xOf) return;
    const canvas = $("song-lanes");
    const width = canvas.parentElement.clientWidth, height = totalHeight();
    const context = sizeCanvas(canvas, width, height);
    if (!context) return;

    const map = tempoMap(), scroll = Hub.arrange.scrollLeft();
    const from = Hub.arrange.ppqOfContentX(scroll), to = Hub.arrange.ppqOfContentX(scroll + width) + 1e-6;
    const colours = Hub.theme.lanes;
    markers = [];

    context.fillStyle = colours.lane;
    context.fillRect(0, 0, width, height);

    const chips = tab === "signature" ? map.signatures.map((s) => ({ ppq: map.ppqOfBar(s.bar), text: `${s.numerator}/${s.denominator}` }))
                : tab === "key" ? map.keys.map((k) => ({ ppq: map.ppqOfBar(k.bar), text: Hub.TempoMap.keyName(k) }))
                : tab === "chords" ? map.chords.map((c) => ({ ppq: c.ppq, text: chordName(c) })) : null;
    if (chips) shadeStretches(context, chips, width);

    // The bar lines, as many as the arrangement's grid shows.
    const step = Hub.arrange.gridStep(10);
    context.strokeStyle = colours.bar;
    context.beginPath();
    for (const line of map.lines(from, to, step.bars > 0 ? step : { q: map.barAt(from).length, bars: 1 })) {
      const x = Math.round(xOf(line.ppq)) + 0.5;
      context.moveTo(x, 0);
      context.lineTo(x, height);
    }
    context.stroke();

    context.font = "12px Segoe UI, system-ui, sans-serif";
    context.textBaseline = "middle";
    if (chips) drawChips(context, tab, chips, width);
    if (tab === "tempo") drawTempo(context, map, width);
    if (tab === "main") drawMain(context, width);
  }

  /** Every other change's stretch (to the next) shaded, so where each holds reads at a glance. */
  function shadeStretches(context, items, width) {
    items.forEach((item, index) => {
      const x = Math.round(xOf(item.ppq));
      const next = index + 1 < items.length ? Math.round(xOf(items[index + 1].ppq)) : width + 2;
      if (index % 2 === 1 && next > 0 && x < width) {
        context.globalAlpha = 0.45;
        context.fillStyle = Hub.theme.lanes.selected;
        context.fillRect(Math.max(0, x), 0, Math.min(width, next) - Math.max(0, x), height);
        context.globalAlpha = 1;
      }
    });
  }

  /** Each change a flag at the top, its line down the lane. */
  function drawChips(context, lane, items, width) {
    const chip = 22;
    items.forEach((item, index) => {
      const x = Math.round(xOf(item.ppq));
      const w = context.measureText(item.text).width + 14;
      if (x > width || x + w < 0) return;
      const picked = selected && selected.lane === lane && selected.index === index;
      context.fillStyle = picked ? ACCENT : Hub.theme.muted;
      context.fillRect(x, 0, picked ? 2 : 1, height);
      context.fillStyle = picked ? ACCENT : Hub.theme.line2;
      context.fillRect(x, 6, w, chip);
      context.fillStyle = picked ? "#ffffff" : Hub.theme.muted;
      context.fillRect(x, 6, 2, chip);
      context.fillStyle = picked ? "#ffffff" : Hub.theme.text;
      context.fillText(item.text, x + 8, 6 + chip / 2 + 0.5);
      markers.push({ lane, index, x0: x - 3, x1: x + w });
    });
  }

  /** The tempo scale's lines: every 10, 20, 25 or 50 BPM, as many as fit. */
  function drawScale(context, width) {
    const { lo, hi } = tempoRange();
    const perBpm = (laneHeight() - 2 * PAD) / (hi - lo);
    const step = [10, 20, 25, 50, 100].find((s) => s * perBpm >= 18) || 100;
    context.font = "10px Segoe UI, system-ui, sans-serif";
    context.textBaseline = "middle";
    for (let bpm = Math.ceil(lo / step) * step; bpm <= hi; bpm += step) {
      const y = Math.round(tempoY(bpm)) + 0.5;
      context.strokeStyle = Hub.theme.lanes.beat;
      context.beginPath();
      context.moveTo(0, y);
      context.lineTo(width, y);
      context.stroke();
      context.fillStyle = Hub.theme.muted;
      context.fillText(String(bpm), 4, y);
    }
    context.font = "12px Segoe UI, system-ui, sans-serif";
  }

  function drawTempo(context, map, width) {
    drawScale(context, width);
    const top = 0, bottom = height - 1;
    const points = map.tempos.map((t) => ({ x: xOf(t.ppq), y: tempoY(t.bpm), t }));

    const trace = () => {
      context.beginPath();
      context.moveTo(points[0].x, points[0].y);
      points.forEach((p, i) => {
        const next = points[i + 1];
        if (!next) { context.lineTo(width + 2, p.y); return; }
        if (p.t.ramp) context.lineTo(next.x, next.y);
        else { context.lineTo(next.x, p.y); context.lineTo(next.x, next.y); }
      });
    };

    trace();
    context.lineTo(width + 2, bottom);
    context.lineTo(points[0].x, bottom);
    context.closePath();
    context.fillStyle = "rgba(37, 152, 77, 0.16)";
    context.fill();

    trace();
    context.strokeStyle = LINE;
    context.lineWidth = 1.5;
    context.stroke();
    context.lineWidth = 1;

    context.textBaseline = "alphabetic";
    points.forEach((p, i) => {
      if (p.x < -40 || p.x > width + 4) return;
      const picked = selected && selected.lane === "tempo" && selected.index === i;
      context.fillStyle = picked ? "#ffffff" : LINE;
      context.fillRect(Math.round(p.x) - 3, Math.round(p.y) - 3, 7, 7);
      context.fillStyle = Hub.theme.text;
      const above = p.y - top > 16;
      // Clear of the scale's numbers at the left edge.
      context.fillText(bpmText(p.t.bpm) + (p.t.ramp ? " ↗" : ""), Math.max(p.x + 6, 30), above ? p.y - 5 : p.y + 13);
    });
    context.textBaseline = "middle";
  }

  function drawMain(context, width) {
    const points = mainPoints();
    const top = 0;

    if (!points.length) {
      const y = Math.round(mainY(Hub.state.master ? Hub.state.master.volume : 0)) + 0.5;
      context.strokeStyle = Hub.theme.muted;
      context.setLineDash([3, 4]);
      context.beginPath();
      context.moveTo(0, y);
      context.lineTo(width, y);
      context.stroke();
      context.setLineDash([]);
      context.fillStyle = Hub.theme.muted;
      context.fillText("Click to add points: Main's fader follows them while the song plays", 8, top + 14);
      return;
    }

    const xy = points.map(([ppq, db]) => ({ x: xOf(ppq), y: mainY(db) }));
    context.strokeStyle = "#e0b04a";
    context.lineWidth = 1.5;
    context.beginPath();
    context.moveTo(-2, xy[0].y);
    for (const p of xy) context.lineTo(p.x, p.y);
    context.lineTo(width + 2, xy[xy.length - 1].y);
    context.stroke();
    context.lineWidth = 1;

    xy.forEach((p, i) => {
      if (p.x < -4 || p.x > width + 4) return;
      const picked = selected && selected.lane === "main" && selected.index === i;
      context.fillStyle = picked ? "#ffffff" : "#e0b04a";
      context.fillRect(Math.round(p.x) - 3, Math.round(p.y) - 3, 7, 7);
    });
  }

  // ---------------------------------------------------------------- the header: its tabs, and what holds where the song is
  const heads = {};
  const HINTS = {
    signature: "Double-click to add or change; drag to move",
    key: "Double-click to add or change; drag to move",
    chords: "Double-click to add or change; drag to move; right-click: Detect, Pads",
    tempo: "",
    main: "Click to add points; drag them"
  };

  function buildHeads() {
    const box = $("song-heads");
    box.textContent = "";

    const tabs = Hub.element("div", "song-tabs");
    tabs.setAttribute("role", "tablist");
    tabs.setAttribute("aria-label", "Master Song Channel");
    heads.tabs = {};
    for (const each of TABS) {
      const button = Hub.element("button", "song-tab", each.name);
      button.type = "button";
      button.dataset.tab = each.id;
      button.title = each.title;
      button.setAttribute("role", "tab");
      button.addEventListener("click", () => setTab(each.id));
      tabs.appendChild(button);
      heads.tabs[each.id] = button;
    }
    box.appendChild(tabs);

    const head = Hub.element("div", "song-head");
    head.appendChild(Hub.element("span", "song-head-caption", "Master Song Channel"));
    const line = Hub.element("div", "song-head-line");
    heads.name = Hub.element("span", "song-head-name", "");
    heads.value = Hub.element("span", "song-head-value", "");
    line.append(heads.name, heads.value);
    head.appendChild(line);
    heads.hint = Hub.element("span", "song-head-hint", "");
    head.appendChild(heads.hint);
    box.appendChild(head);

    for (const holder of [box, $("song-lanes-wrap")]) {
      const edge = Hub.element("div", "song-resize");
      edge.title = "Drag for a taller or shorter Master Song Channel; double-click for the usual height";
      edge.addEventListener("pointerdown", startResize);
      edge.addEventListener("dblclick", () => setHeight(DEFAULT_HEIGHT, true));
      holder.appendChild(edge);
    }
    showTab();
  }

  const setText = (element, text) => { if (element && element.textContent !== text) element.textContent = text; };

  function updateHeads() {
    if (!shown || !heads.value) return;
    const map = Hub.tempoMap, at = Hub.songPosition();
    const name = TABS.find((each) => each.id === tab).name;
    let value = "", hint = HINTS[tab];
    if (tab === "signature") { const signature = map.signatureAt(at); value = `${signature.numerator}/${signature.denominator}`; }
    if (tab === "key") value = Hub.TempoMap.keyName(map.keyAt(at));
    if (tab === "chords") value = chordName(map.chordAt(at)) || "\u2014";
    if (tab === "tempo") {
      value = `${map.bpmAt(at).toFixed(2)} BPM`;
      const { lo, hi } = tempoRange();
      hint = `Scale ${lo}–${hi} BPM · Shift: fine`;
    }
    if (tab === "main") {
      const volume = Hub.state.master ? Hub.state.master.volume : 0;
      value = Hub.dbText(volume) + ((Hub.songChannel.mainAutomation || []).length ? " · Read" : "");
    }
    setText(heads.name, name);
    setText(heads.value, value);
    setText(heads.hint, hint);
  }

  // ---------------------------------------------------------------- the tabs and the height
  function showTab() {
    if (!heads.tabs) return;
    for (const [id, button] of Object.entries(heads.tabs)) {
      const on = id === tab;
      button.classList.toggle("on", on);
      button.setAttribute("aria-selected", String(on));
    }
    $("song-lanes").setAttribute("aria-label", `Master Song Channel: ${TABS.find((each) => each.id === tab).name}`);
  }

  function setTab(id) {
    if (!TABS.some((each) => each.id === id)) return;
    if (id !== tab) { selected = null; closeEditor(); }
    tab = id;
    showTab();
    updateHeads();
    draw();
    Hub.emit("viewChanged");
  }

  function applyHeight() {
    $("song-lanes-wrap").style.height = height + "px";
    $("song-heads").style.height = height + "px";
  }

  function setHeight(value, done) {
    const next = Math.round(Hub.clamp(Number(value) || DEFAULT_HEIGHT, MIN_HEIGHT, MAX_HEIGHT));
    if (next !== height) {
      height = next;
      applyHeight();
      Hub.emit("layout");
      if (Hub.arrange) Hub.arrange.redraw();
      draw();
    }
    if (done) Hub.emit("viewChanged");
  }

  function startResize(event) {
    if (event.button !== 0) return;
    event.preventDefault();
    event.stopPropagation();
    const edge = event.currentTarget, from = event.clientY, start = height;
    edge.setPointerCapture(event.pointerId);
    let frame = 0, wanted = start;
    const move = (e) => {
      wanted = start + e.clientY - from;
      if (!frame) frame = requestAnimationFrame(() => { frame = 0; setHeight(wanted, false); });
    };
    const up = () => {
      edge.removeEventListener("pointermove", move);
      edge.removeEventListener("pointerup", up);
      edge.removeEventListener("pointercancel", up);
      if (frame) { cancelAnimationFrame(frame); frame = 0; }
      setHeight(wanted, true);
    };
    edge.addEventListener("pointermove", move);
    edge.addEventListener("pointerup", up);
    edge.addEventListener("pointercancel", up);
  }

  let padsKey = "";

  function tick() {
    updateHeads();
    // The Chord Pads follow the key where the song is.
    if (pads) {
      const key = Hub.TempoMap.keyName(Hub.tempoMap.keyAt(Hub.songPosition()));
      if (key !== padsKey) { padsKey = key; fillPads(); }
    }
    requestAnimationFrame(tick);
  }

  // ---------------------------------------------------------------- showing it
  function show(on) {
    shown = Boolean(on);
    $("song-heads").hidden = !shown;
    $("song-lanes-wrap").hidden = !shown;
    applyHeight();
    const button = $("song-channel-toggle");
    if (button) button.setAttribute("aria-pressed", String(shown));
    if (!shown) { selected = null; closeEditor(); }
    Hub.emit("layout");
    requestAnimationFrame(() => { if (Hub.arrange) Hub.arrange.redraw(); });
  }

  Hub.commands.toggleSongChannel = () => {
    show(!shown);
    Hub.emit("viewChanged");
  };
  // The tabs from the keyboard or a menu: the channel shows with it.
  const COMMAND_OF = { signature: "songChannelSignature", key: "songChannelKey", chords: "songChannelChords", tempo: "songChannelTempo", main: "songChannelVolume" };
  for (const each of TABS) {
    Hub.commands[COMMAND_OF[each.id]] = () => {
      if (!shown) show(true);
      setTab(each.id);
    };
  }

  // ---------------------------------------------------------------- sending edits
  /** A changed map to the engine (one Undo step); the page has it at once. */
  function commitMap(next, name) {
    previewMap = null;
    if (next.equals(Hub.tempoMap)) { draw(); return; }
    Hub.tempoMap = next;
    Hub.send("setSongChannel", { map: next.toData(), name });
    Hub.emit("tempoMap", next);
  }

  function commitMain(points) {
    previewMain = null;
    const tidy = points.slice().sort((a, b) => a[0] - b[0]);
    Hub.songChannel.mainAutomation = tidy;
    Hub.send("setMainAutomation", { points: tidy });
    draw();
  }

  const withTempos = (map, tempos) => { const next = map.clone(); next.setTempos(tempos); return next; };
  const withSignatures = (map, list) => { const next = map.clone(); next.setSignatures(list); return next; };
  const withKeys = (map, list) => { const next = map.clone(); next.setKeys(list); return next; };
  const withChords = (map, list) => { const next = map.clone(); next.setChords(list); return next; };
  const copyTempos = (map) => map.tempos.map((t) => ({ ...t }));

  function removeItem(lane, index) {
    const map = Hub.tempoMap;
    if (lane === "signature" && index > 0) commitMap(withSignatures(map, map.signatures.filter((_, i) => i !== index)), "Remove Time Signature");
    if (lane === "key" && index > 0) commitMap(withKeys(map, map.keys.filter((_, i) => i !== index)), "Remove Key Change");
    if (lane === "chords" && index >= 0) commitMap(withChords(map, map.chords.filter((_, i) => i !== index)), "Remove Chord");
    if (lane === "tempo" && index > 0) commitMap(withTempos(map, map.tempos.filter((_, i) => i !== index)), "Remove Tempo Change");
    if (lane === "main") commitMain(mainPoints().filter((_, i) => i !== index));
    selected = null;
    draw();
  }

  function setRamp(index, ramp) {
    const tempos = copyTempos(Hub.tempoMap);
    if (!tempos[index] || index >= tempos.length - 1) return;
    tempos[index].ramp = ramp;
    commitMap(withTempos(Hub.tempoMap, tempos), ramp ? "Ramp Tempo" : "Hold Tempo");
  }

  // ---------------------------------------------------------------- hits
  function point(event) {
    const rect = $("song-lanes").getBoundingClientRect();
    const x = event.clientX - rect.left, y = event.clientY - rect.top;
    return { x, y, lane: laneAt(y), ppq: Hub.arrange.ppqAt(event.clientX) };
  }

  const chipAt = (p) => {
    const hits = markers.filter((m) => m.lane === p.lane && p.x >= m.x0 && p.x <= m.x1);
    return hits.length ? hits[hits.length - 1].index : -1;   // the one drawn on top
  };

  function tempoPointAt(p) {
    const map = tempoMap();
    for (let i = map.tempos.length - 1; i >= 0; i--) {
      const t = map.tempos[i];
      if (Math.abs(xOf(t.ppq) - p.x) <= GRAB && Math.abs(tempoY(t.bpm) - p.y) <= GRAB) return i;
    }
    return -1;
  }

  function mainPointAt(p) {
    const points = mainPoints();
    for (let i = points.length - 1; i >= 0; i--)
      if (Math.abs(xOf(points[i][0]) - p.x) <= GRAB && Math.abs(mainY(points[i][1]) - p.y) <= GRAB) return i;
    return -1;
  }

  const placed = (event, ppq) => (event.altKey ? Math.max(0, ppq) : Hub.arrange.snap(ppq));

  // ---------------------------------------------------------------- pressing, dragging
  function onDown(event) {
    if (event.button !== 0) return;
    closeEditor();
    const p = point(event);
    const tool = Hub.arrange.view.tool;
    const map = Hub.tempoMap;
    event.preventDefault();
    $("song-lanes").setPointerCapture(event.pointerId);

    if (p.lane === "signature" || p.lane === "key") {
      const index = chipAt(p);
      selected = index >= 0 ? { lane: p.lane, index } : null;
      if (index >= 0 && tool === "eraser") { removeItem(p.lane, index); return; }
      if (index > 0) drag = { kind: "chip", lane: p.lane, index, pointer: event.pointerId, base: map, moved: false, x: p.x };
    } else if (p.lane === "chords") {
      const index = chipAt(p);
      selected = index >= 0 ? { lane: "chords", index } : null;
      if (index >= 0 && tool === "eraser") { removeItem("chords", index); return; }
      if (index >= 0) {
        hearChord(map.chords[index]);
        drag = { kind: "chord", index, pointer: event.pointerId, base: map, moved: false, x: p.x, grab: p.ppq, start: map.chords[index].ppq };
      }
    } else if (p.lane === "tempo") {
      let index = tempoPointAt(p);
      if (index >= 0 && tool === "eraser") { removeItem("tempo", index); return; }
      if (tool === "eraser") { selected = null; draw(); return; }
      let tempos = copyTempos(map), added = false;
      if (index < 0) {
        // A new change where the press is, at the height's tempo.
        const at = placed(event, p.ppq);
        const same = tempos.findIndex((t) => Math.abs(t.ppq - at) < 1e-6);
        if (same >= 0) index = same;
        else {
          const before = tempos.filter((t) => t.ppq < at).length - 1;
          tempos.splice(before + 1, 0, { ppq: at, bpm: tempoAtY(p.y, event.shiftKey), ramp: Boolean(tempos[before] && tempos[before].ramp) });
          index = before + 1;
          added = true;
        }
        previewMap = withTempos(map, tempos);
        tempos = copyTempos(previewMap);
      }
      selected = { lane: "tempo", index };
      // y: where the pointer would be with Shift's tenths taken in (the tempo follows it).
      drag = { kind: "tempo", index, pointer: event.pointerId, tempos, base: map, grab: p, start: tempos[index], moved: added, added, y: p.y, lastY: p.y };
    } else if (p.lane === "main") {
      let index = mainPointAt(p);
      if (index >= 0 && tool === "eraser") { removeItem("main", index); return; }
      if (tool === "eraser") { selected = null; draw(); return; }
      const points = mainPoints().map((q) => q.slice());
      let added = false;
      if (index < 0) {
        const point = [placed(event, p.ppq), mainAtY(p.y)];
        points.push(point);
        points.sort((a, b) => a[0] - b[0]);
        index = points.indexOf(point);   // this one, even where another point is at the same place
        previewMain = points;
        added = true;
      }
      selected = { lane: "main", index };
      drag = { kind: "main", index, pointer: event.pointerId, points, grab: p, start: points[index].slice(), moved: added, added };
    }

    draw();
  }

  function onMove(event) {
    const p = point(event);
    if (!drag) {
      const over = (p.lane === "signature" || p.lane === "key" || p.lane === "chords") ? chipAt(p) >= 0
                 : p.lane === "tempo" ? tempoPointAt(p) >= 0 : p.lane === "main" ? mainPointAt(p) >= 0 : false;
      const tool = Hub.arrange.view.tool;
      $("song-lanes").style.cursor = tool === "eraser" ? (over ? "not-allowed" : "default")
                                   : over ? ((p.lane === "signature" || p.lane === "key" || p.lane === "chords") ? "grab" : "move")
                                   : (p.lane === "tempo" || p.lane === "main") ? "crosshair" : "default";
      return;
    }
    if (event.pointerId !== drag.pointer) return;
    const d = drag;

    if (d.kind === "chip") {
      if (Math.abs(p.x - d.x) > 3) d.moved = true;
      if (!d.moved) return;
      // To the nearest bar line, between the changes either side.
      const list = (d.lane === "signature" ? d.base.signatures : d.base.keys).map((item) => ({ ...item }));
      const lowest = list[d.index - 1].bar + 1, highest = d.index + 1 < list.length ? list[d.index + 1].bar - 1 : Hub.TempoMap.MAX_BAR;
      list[d.index].bar = Hub.clamp(d.base.barAt(d.base.nearestBar(p.ppq) + 1e-9).index, lowest, highest);
      previewMap = d.lane === "signature" ? withSignatures(d.base, list) : withKeys(d.base, list);
    } else if (d.kind === "chord") {
      if (Math.abs(p.x - d.x) > 3) d.moved = true;
      if (!d.moved) return;
      // Snapped as the arrangement snaps (Alt: free), between the chords either side.
      const list = d.base.chords.map((c) => ({ ...c }));
      const lowest = d.index > 0 ? list[d.index - 1].ppq + 1 / 64 : 0;
      const highest = d.index + 1 < list.length ? list[d.index + 1].ppq - 1 / 64 : Infinity;
      list[d.index].ppq = Hub.clamp(placed(event, d.start + p.ppq - d.grab), lowest, highest);
      previewMap = withChords(d.base, list);
    } else if (d.kind === "tempo") {
      if (Math.abs(p.x - d.grab.x) > 2 || Math.abs(p.y - d.grab.y) > 2) d.moved = true;
      const tempos = d.tempos.map((t) => ({ ...t }));
      const t = tempos[d.index];
      d.y += (p.y - d.lastY) * (event.shiftKey ? 0.1 : 1);
      d.lastY = p.y;
      // Its tempo stays as it was until the pointer goes up or down a little (a drag along the
      // song only moves it there).
      if (Math.abs(p.y - d.grab.y) >= 3) d.vertical = true;
      if (d.vertical) t.bpm = tempoAtY(tempoY(d.start.bpm) + (d.y - d.grab.y), event.shiftKey);
      if (d.index > 0) {
        const lowest = tempos[d.index - 1].ppq + 1 / 64, highest = d.index + 1 < tempos.length ? tempos[d.index + 1].ppq - 1 / 64 : Infinity;
        t.ppq = Hub.clamp(placed(event, d.start.ppq + p.ppq - d.grab.ppq), lowest, highest);
      }
      previewMap = withTempos(d.base, tempos);
    } else if (d.kind === "main") {
      if (Math.abs(p.x - d.grab.x) > 2 || Math.abs(p.y - d.grab.y) > 2) d.moved = true;
      const points = d.points.map((q) => q.slice());
      const lowest = d.index > 0 ? points[d.index - 1][0] : 0;
      const highest = d.index + 1 < points.length ? points[d.index + 1][0] : Infinity;
      points[d.index] = [Hub.clamp(placed(event, d.start[0] + p.ppq - d.grab.ppq), lowest, highest), mainAtY(mainY(d.start[1]) + (p.y - d.grab.y))];
      previewMain = points;
    }

    draw();
  }

  function onUp(event) {
    const d = drag;
    if (!d || event.pointerId !== d.pointer) return;
    drag = null;

    if (d.kind === "chip") {
      if (d.moved && previewMap) commitMap(previewMap, d.lane === "signature" ? "Move Time Signature" : "Move Key Change");
      else previewMap = null;
    } else if (d.kind === "chord") {
      if (d.moved && previewMap) commitMap(previewMap, "Move Chord");
      else previewMap = null;
    } else if (d.kind === "tempo") {
      if (d.moved && previewMap) commitMap(previewMap, d.added ? "Add Tempo Change" : "Change Tempo");
      else previewMap = null;
    } else if (d.kind === "main") {
      if (d.moved && previewMain) commitMain(previewMain);
      else previewMain = null;
    }

    draw();
  }

  // ---------------------------------------------------------------- typing a value
  function closeEditor() {
    if (!editor) return;
    editor.close();
  }

  /** A small panel by the pointer: fields, then OK (Enter), Cancel (Escape) and maybe Remove. */
  function openEditor(clientX, clientY, title, build, onOk, onRemove) {
    closeEditor();
    const panel = Hub.element("div", "song-edit");
    panel.setAttribute("role", "dialog");
    panel.appendChild(Hub.element("div", "song-edit-title", title));
    const fields = Hub.element("div", "song-edit-fields");
    panel.appendChild(fields);
    const read = build(fields);
    const buttons = Hub.element("div", "song-edit-buttons");
    if (onRemove) {
      const remove = Hub.element("button", "plain-button small song-edit-remove", "Remove");
      remove.type = "button";
      remove.addEventListener("click", () => { close(); onRemove(); });
      buttons.appendChild(remove);
    }
    const ok = Hub.element("button", "plain-button small primary", "OK");
    ok.type = "button";
    const cancel = Hub.element("button", "plain-button small", "Cancel");
    cancel.type = "button";
    buttons.append(ok, cancel);
    panel.appendChild(buttons);

    const close = () => {
      panel.remove();
      document.removeEventListener("pointerdown", outside, true);
      if (editor && editor.panel === panel) editor = null;
    };
    const accept = () => { const value = read(); close(); if (value !== null) onOk(value); };
    const outside = (event) => { if (!panel.contains(event.target)) close(); };

    ok.addEventListener("click", accept);
    cancel.addEventListener("click", close);
    panel.addEventListener("keydown", (event) => {
      if (event.key === "Enter") { event.preventDefault(); accept(); }
      if (event.key === "Escape") { event.preventDefault(); close(); }
      event.stopPropagation();
    });

    document.body.appendChild(panel);
    const width = panel.offsetWidth || 240, height = panel.offsetHeight || 120;
    panel.style.left = Math.max(4, Math.min(window.innerWidth - width - 4, clientX)) + "px";
    panel.style.top = Math.max(4, Math.min(window.innerHeight - height - 4, clientY + 8)) + "px";
    setTimeout(() => document.addEventListener("pointerdown", outside, true), 0);
    editor = { panel, close };
    const first = panel.querySelector("input, select");
    if (first) { first.focus(); if (first.select) first.select(); }
  }

  function select(options, value) {
    const element = document.createElement("select");
    for (const [label, v] of options) element.add(new Option(label, String(v), false, String(v) === String(value)));
    return element;
  }

  function editSignature(clientX, clientY, bar) {
    const map = Hub.tempoMap;
    const index = map.signatures.findIndex((s) => s.bar === bar);
    const now = map.bar(bar);
    openEditor(clientX, clientY, `Time signature from bar ${bar + 1}`, (fields) => {
      const numerator = select(Array.from({ length: NUMERATORS }, (_, i) => [String(i + 1), i + 1]), now.numerator);
      const denominator = select(DENOMINATORS.map((d) => [String(d), d]), DENOMINATORS.includes(now.denominator) ? now.denominator : 4);
      numerator.setAttribute("aria-label", "Beats per bar");
      denominator.setAttribute("aria-label", "Beat value");
      fields.append(numerator, Hub.element("span", "song-edit-slash", "/"), denominator);
      return () => ({ numerator: Number(numerator.value), denominator: Number(denominator.value) });
    }, (value) => {
      const list = map.signatures.filter((s) => s.bar !== bar).concat([{ bar, ...value }]);
      commitMap(withSignatures(map, list), index >= 0 ? "Change Time Signature" : "Add Time Signature");
    }, index > 0 ? () => removeItem("signature", index) : null);
  }

  function editKey(clientX, clientY, bar) {
    const map = Hub.tempoMap;
    const index = map.keys.findIndex((k) => k.bar === bar);
    const now = map.keyAt(map.ppqOfBar(bar));
    openEditor(clientX, clientY, `Key from bar ${bar + 1}`, (fields) => {
      const root = select(Hub.TempoMap.NOTE_NAMES.map((name, i) => [name, i]), now.root);
      const scale = select((Hub.songChannel.scales || Hub.TempoMap.SCALES).map((s) => [s, s]), now.scale);
      root.setAttribute("aria-label", "Key");
      scale.setAttribute("aria-label", "Scale");
      fields.append(root, scale);
      return () => ({ root: Number(root.value), scale: scale.value });
    }, (value) => {
      const list = map.keys.filter((k) => k.bar !== bar).concat([{ bar, ...value }]);
      commitMap(withKeys(map, list), index >= 0 ? "Change Key" : "Add Key Change");
    }, index > 0 ? () => removeItem("key", index) : null);
  }

  function editTempo(clientX, clientY, index) {
    const map = Hub.tempoMap;
    const t = map.tempos[index];
    if (!t) return;
    const last = index === map.tempos.length - 1;
    openEditor(clientX, clientY, index === 0 ? "The song's tempo" : `Tempo from ${map.formatBars(t.ppq).replace(/^0+/, "")}`, (fields) => {
      const bpm = document.createElement("input");
      bpm.type = "number";
      bpm.min = String(Hub.TempoMap.MIN_TEMPO);
      bpm.max = String(Hub.TempoMap.MAX_TEMPO);
      bpm.step = "0.01";
      bpm.value = t.bpm.toFixed(2);
      bpm.setAttribute("aria-label", "Tempo in BPM");
      const label = Hub.element("label", "song-edit-check");
      const ramp = document.createElement("input");
      ramp.type = "checkbox";
      ramp.checked = t.ramp;
      ramp.disabled = last;
      label.append(ramp, document.createTextNode(" Glide to the next tempo"));
      fields.append(bpm, Hub.element("span", "song-edit-unit", "BPM"), label);
      return () => {
        const value = Number(bpm.value);
        return Number.isFinite(value) && value > 0 ? { bpm: Hub.clamp(value, Hub.TempoMap.MIN_TEMPO, Hub.TempoMap.MAX_TEMPO), ramp: ramp.checked } : null;
      };
    }, (value) => {
      const tempos = copyTempos(map);
      tempos[index].bpm = Math.round(value.bpm * 100) / 100;
      tempos[index].ramp = value.ramp;
      commitMap(withTempos(map, tempos), "Change Tempo");
    }, index > 0 ? () => removeItem("tempo", index) : null);
  }

  // ---------------------------------------------------------------- the chord track
  /** A chord as notes from about C3 up (its own bass under it, when it has one). */
  function chordVoicing(chord) {
    const tones = Hub.TempoMap.chordPitches({ ...chord, bass: -1 });
    const base = 48 + chord.root;
    const notes = tones.map((pc) => base + ((pc - chord.root + 12) % 12));
    if (chord.bass >= 0) notes.unshift(36 + chord.bass);
    return notes;
  }

  let hearing = [], hearTimer = 0;

  /** A chord heard on the selected track, a moment long. */
  function hearChord(chord) {
    if (!chord) return;
    clearTimeout(hearTimer);
    for (const note of hearing) Hub.send("noteOff", { note });
    hearing = chordVoicing(chord);
    for (const note of hearing) Hub.send("noteOn", { note, velocity: 0.7 });
    hearTimer = setTimeout(() => { for (const note of hearing) Hub.send("noteOff", { note }); hearing = []; }, 650);
  }

  /** A chord put at a position (one there already is replaced). */
  function putChord(ppq, chord, name) {
    const map = Hub.tempoMap;
    const list = map.chords.filter((c) => Math.abs(c.ppq - ppq) > 1e-6).concat([{ ppq, ...chord }]);
    commitMap(withChords(map, list), name || "Add Chord");
  }

  function editChord(clientX, clientY, index, ppq) {
    const map = Hub.tempoMap;
    const chord = index >= 0 ? map.chords[index] : null;
    const at = chord ? chord.ppq : ppq;
    openEditor(clientX, clientY, `${chord ? "Chord" : "A chord"} at ${map.formatBars(at).replace(/^0+/, "")}`, (fields) => {
      const input = document.createElement("input");
      input.type = "text";
      input.className = "song-edit-chord";
      input.value = chord ? chordName(chord) : chordName(map.chordAt(at)) || "C";
      input.placeholder = "Am7, C/E, F#m7b5";
      input.spellcheck = false;
      input.setAttribute("aria-label", "Chord");
      fields.append(input, Hub.element("span", "song-edit-unit", "e.g. Am7, C/E, Bbmaj7"));
      return () => {
        const parsed = Hub.TempoMap.parseChord(input.value);
        if (!parsed) Hub.toast(`"${input.value}" isn't a chord Sonify knows: try Am7, C/E, F#m7b5 or Bbmaj7.`);
        return parsed;
      };
    }, (value) => {
      if (chord) {
        const list = map.chords.map((c, i) => (i === index ? { ppq: c.ppq, ...value } : c));
        commitMap(withChords(map, list), "Change Chord");
      } else {
        putChord(at, value);
      }
      hearChord(value);
    }, chord ? () => removeItem("chords", index) : null);
  }

  /** Detect Chords: the selected MIDI clips' notes read a beat at a time; the lane takes their
      chords over the stretch they cover (one a beat at the most, a beat with nothing new keeps
      the one before). */
  const DETECT = ["", "m", "7", "maj7", "m7", "dim", "sus4", "sus2", "m7b5", "aug"];

  function chordOfBeat(weights, lowest) {
    const total = weights.reduce((a, b) => a + b, 0);
    if (total < 1e-6) return null;
    const most = Math.max(...weights);
    let best = null;
    for (let root = 0; root < 12; root++) {
      for (const quality of DETECT) {
        const tones = Hub.TempoMap.CHORD_NOTES[quality].map((i) => (root + i) % 12);
        let inside = 0, missing = 0;
        for (const pc of tones) { inside += weights[pc]; if (weights[pc] < most * 0.08) missing++; }
        let score = inside - 0.6 * (total - inside) - 0.35 * most * missing - 0.04 * most * tones.length;
        if (lowest === root) score += 0.15 * most;
        if (!best || score > best.score + 1e-9) best = { score, root, quality, tones };
      }
    }
    // A bass of its own when the lowest note is in the chord but isn't its root (triads only).
    const bass = lowest !== null && lowest !== best.root && best.tones.includes(lowest) && (best.quality === "" || best.quality === "m") ? lowest : -1;
    return { root: best.root, quality: best.quality, bass };
  }

  function detectChords() {
    const map = Hub.tempoMap;
    const notes = [];
    for (const id of Hub.state.selectedClips || []) {
      const found = Hub.findClip(id);
      if (!found || found.clip.audioFile || !Array.isArray(found.clip.notes)) continue;
      const clip = found.clip, offset = clip.offset || 0;
      for (const n of clip.notes) {
        const [pitch, start, length, velocity, muted] = n;
        if (muted || start < offset - 1e-9 || start >= offset + clip.length) continue;
        const from = clip.start + start - offset;
        notes.push({ pitch, from, to: Math.min(from + length, clip.start + clip.length), weight: (velocity || 100) / 127 });
      }
    }
    if (!notes.length) { Hub.toast("Select one or more MIDI clips with notes first: their chords go on the lane."); return; }

    const first = Math.min(...notes.map((n) => n.from)), last = Math.max(...notes.map((n) => n.to));
    const found = [];
    for (let at = map.barAt(first + 1e-9).start; at < last - 1e-9;) {
      const bar = map.barAt(at + 1e-9);
      const end = Math.min(bar.end, at + bar.beat);
      const weights = new Array(12).fill(0);
      let lowest = null, lowestPitch = 128;
      for (const n of notes) {
        const overlap = Math.min(n.to, end) - Math.max(n.from, at);
        if (overlap <= 1e-9) continue;
        weights[n.pitch % 12] += overlap * n.weight;
        if (n.pitch < lowestPitch) { lowestPitch = n.pitch; lowest = n.pitch % 12; }
      }
      const chord = chordOfBeat(weights, lowest);
      const before = found[found.length - 1];
      if (chord && !(before && before.root === chord.root && before.quality === chord.quality && before.bass === chord.bass))
        found.push({ ppq: at, ...chord });
      at = end;
    }
    if (!found.length) { Hub.toast("Sonify couldn't hear chords in those notes."); return; }

    const from = found[0].ppq;
    const kept = map.chords.filter((c) => c.ppq < from - 1e-6 || c.ppq >= last - 1e-6);
    commitMap(withChords(map, kept.concat(found)), "Detect Chords");
    if (!shown) show(true);
    setTab("chords");
    Hub.toast(`${found.length} chord${found.length === 1 ? "" : "s"} on the chord track: double-click one to correct it.`);
  }

  // The Chord Pads: the key's chords where the song is, to hear or (Write) put on the lane.
  const SCALE_STEPS = { major: [0, 2, 4, 5, 7, 9, 11], minor: [0, 2, 3, 5, 7, 8, 10], dorian: [0, 2, 3, 5, 7, 9, 10],
                        phrygian: [0, 1, 3, 5, 7, 8, 10], lydian: [0, 2, 4, 6, 7, 9, 11], mixolydian: [0, 2, 4, 5, 7, 9, 10],
                        locrian: [0, 1, 3, 5, 6, 8, 10], "harmonic minor": [0, 2, 3, 5, 7, 8, 11], "melodic minor": [0, 2, 3, 5, 7, 9, 11] };
  const MINORISH = new Set(["minor", "minor pentatonic", "blues"]);
  let pads = null;   // the panel: {panel, grid, title, write}

  /** Seven chords of the key's own notes (a triad on each), and the dominant seventh. */
  function keyChords(key) {
    const steps = SCALE_STEPS[key.scale] || (MINORISH.has(key.scale) ? SCALE_STEPS.minor : SCALE_STEPS.major);
    const inScale = new Set(steps);
    const chords = steps.map((step) => {
      const third = inScale.has((step + 4) % 12) ? 4 : 3;
      const fifth = inScale.has((step + 7) % 12) ? 7 : inScale.has((step + 6) % 12) ? 6 : inScale.has((step + 8) % 12) ? 8 : 7;
      const quality = fifth === 6 ? "dim" : fifth === 8 ? "aug" : third === 4 ? "" : "m";
      return { root: (key.root + step) % 12, quality, bass: -1 };
    });
    chords.push({ root: (key.root + 7) % 12, quality: "7", bass: -1 });
    return chords;
  }

  const ROMAN = ["I", "II", "III", "IV", "V", "VI", "VII"];

  function fillPads() {
    if (!pads) return;
    const key = Hub.tempoMap.keyAt(Hub.songPosition());
    pads.title.textContent = `Chord Pads \u00b7 ${Hub.TempoMap.keyName(key)}`;
    pads.grid.textContent = "";
    keyChords(key).forEach((chord, i) => {
      const button = Hub.element("button", "chord-pad");
      button.type = "button";
      const degree = i < 7 ? (chord.quality === "m" || chord.quality === "dim" ? ROMAN[i].toLowerCase() : ROMAN[i]) + (chord.quality === "dim" ? "\u00b0" : chord.quality === "aug" ? "+" : "") : "V7";
      button.append(Hub.element("span", "chord-pad-name", chordName(chord)), Hub.element("span", "chord-pad-degree", degree));
      button.title = pads.write.checked ? `Hear ${chordName(chord)} and put it on the chord track where the song is` : `Hear ${chordName(chord)} on the selected track`;
      button.addEventListener("click", () => {
        hearChord(chord);
        if (pads.write.checked) {
          const map = Hub.tempoMap, at = Hub.songPosition();
          putChord(map.snap(at, { beat: true }), chord, "Chord from the Pads");
          if (!shown) show(true);
          if (tab !== "chords") setTab("chords");
        }
      });
      pads.grid.appendChild(button);
    });
  }

  function openPads() {
    if (pads) { fillPads(); return; }
    const panel = Hub.element("div", "chord-pads");
    panel.setAttribute("role", "dialog");
    const head = Hub.element("div", "chord-pads-head");
    const title = Hub.element("span", "chord-pads-title", "Chord Pads");
    const close = Hub.element("button", "icon-button", "\u00d7");
    close.type = "button";
    close.title = "Close the Chord Pads";
    close.setAttribute("aria-label", "Close the Chord Pads");
    head.append(title, close);
    const grid = Hub.element("div", "chord-pads-grid");
    const label = Hub.element("label", "chord-pads-write");
    const write = document.createElement("input");
    write.type = "checkbox";
    label.append(write, document.createTextNode(" Write: put the chord on the chord track where the song is"));
    panel.append(head, grid, label);
    document.body.appendChild(panel);
    pads = { panel, grid, title, write };
    close.addEventListener("click", () => { panel.remove(); pads = null; });
    write.addEventListener("change", fillPads);
    const lanes = $("song-lanes-wrap").getBoundingClientRect();
    panel.style.left = Math.max(8, Math.min(window.innerWidth - 380, lanes.left + 40)) + "px";
    panel.style.top = Math.max(8, Math.min(window.innerHeight - 200, (shown ? lanes.bottom : 140) + 6)) + "px";
    fillPads();
  }

  function onDoubleClick(event) {
    const p = point(event);
    const map = Hub.tempoMap;
    if (p.lane === "signature" || p.lane === "key") {
      const index = chipAt(p);
      const list = p.lane === "signature" ? map.signatures : map.keys;
      const bar = index >= 0 ? list[index].bar : map.barAt(p.ppq).index;
      if (p.lane === "signature") editSignature(event.clientX, event.clientY, bar);
      else editKey(event.clientX, event.clientY, bar);
    } else if (p.lane === "chords") {
      const index = chipAt(p);
      editChord(event.clientX, event.clientY, index, placed(event, p.ppq));
    } else if (p.lane === "tempo") {
      const index = tempoPointAt(p);
      if (index >= 0) editTempo(event.clientX, event.clientY, index);
    } else if (p.lane === "main") {
      const index = mainPointAt(p);
      if (index >= 0) removeItem("main", index);
    }
  }

  function onContextMenu(event) {
    event.preventDefault();
    const p = point(event);
    const map = Hub.tempoMap;
    const at = { getBoundingClientRect: () => ({ left: event.clientX, right: event.clientX, top: event.clientY, bottom: event.clientY }) };
    const x = event.clientX, y = event.clientY;

    if (p.lane === "signature" || p.lane === "key") {
      const index = chipAt(p);
      const list = p.lane === "signature" ? map.signatures : map.keys;
      const bar = index >= 0 ? list[index].bar : map.barAt(p.ppq).index;
      const edit = p.lane === "signature" ? editSignature : editKey;
      const what = p.lane === "signature" ? "Time Signature" : "Key";
      const items = index >= 0 ? [{ label: `Edit ${what}…`, value: "edit" }, { label: "Remove", value: "remove", disabled: index === 0 }]
                               : [{ label: `Add ${what} at Bar ${bar + 1}…`, value: "edit" }];
      if (index >= 0) { selected = { lane: p.lane, index }; draw(); }
      Hub.popupMenu(at, items, (pick) => {
        if (pick === "edit") edit(x, y, bar);
        if (pick === "remove") removeItem(p.lane, index);
      });
    } else if (p.lane === "chords") {
      const index = chipAt(p);
      const where = Hub.arrange.snap(p.ppq);
      const midi = (Hub.state.selectedClips || []).some((id) => { const f = Hub.findClip(id); return f && !f.clip.audioFile; });
      const items = index >= 0 ? [{ label: "Edit Chord\u2026", value: "edit" }, { label: "Remove", value: "remove" }]
                               : [{ label: "Add Chord Here\u2026", value: "add" }];
      items.push({ separator: true },
                 { label: "Detect Chords from the Selected MIDI Clips", value: "detect", disabled: !midi },
                 { label: "Chord Pads\u2026", value: "pads" },
                 { label: "Clear the Chord Track", value: "clear", disabled: !map.chords.length });
      if (index >= 0) { selected = { lane: "chords", index }; draw(); }
      Hub.popupMenu(at, items, (pick) => {
        if (pick === "edit") editChord(x, y, index, 0);
        if (pick === "add") editChord(x, y, -1, where);
        if (pick === "remove") removeItem("chords", index);
        if (pick === "detect") detectChords();
        if (pick === "pads") openPads();
        if (pick === "clear") { selected = null; commitMap(withChords(map, []), "Clear the Chord Track"); }
      });
    } else if (p.lane === "tempo") {
      const index = tempoPointAt(p);
      const before = index >= 0 ? index : Math.max(0, map.tempoIndexAt(p.ppq));
      const t = map.tempos[before], last = before === map.tempos.length - 1;
      const items = [];
      if (index >= 0) items.push({ label: "Edit Tempo…", value: "edit" });
      else items.push({ label: "Add Tempo Change Here", value: "add" });
      items.push({ label: t.ramp ? "Hold Until the Next Tempo" : "Glide to the Next Tempo", value: "ramp", disabled: last });
      if (index >= 0) items.push({ label: "Remove", value: "remove", disabled: index === 0 });
      if (index >= 0) { selected = { lane: "tempo", index }; draw(); }
      Hub.popupMenu(at, items, (pick) => {
        if (pick === "edit") editTempo(x, y, index);
        if (pick === "remove") removeItem("tempo", index);
        if (pick === "ramp") setRamp(before, !t.ramp);
        if (pick === "add") {
          const tempos = copyTempos(map);
          const where = Hub.arrange.snap(p.ppq);
          if (!tempos.some((q) => Math.abs(q.ppq - where) < 1e-6)) {
            tempos.push({ ppq: where, bpm: Math.round(map.bpmAt(where) * 100) / 100, ramp: false });
            commitMap(withTempos(map, tempos), "Add Tempo Change");
          }
        }
      });
    } else if (p.lane === "main") {
      const index = mainPointAt(p);
      const items = [];
      if (index >= 0) items.push({ label: "Remove Point", value: "remove" });
      items.push({ label: "Clear Main Volume", value: "clear", disabled: !mainPoints().length });
      Hub.popupMenu(at, items, (pick) => {
        if (pick === "remove") removeItem("main", index);
        if (pick === "clear") { selected = null; commitMain([]); }
      });
    }
  }

  // ---------------------------------------------------------------- wiring
  Hub.songLanes = {
    draw,
    show,
    isShown: () => shown,
    /** What the song's view keeps: the tab and the height. */
    getView: () => ({ tab, height }),
    applyView: (saved) => {
      const v = saved || {};
      tab = TABS.some((each) => each.id === v.tab) ? v.tab : "tempo";
      height = Math.round(Hub.clamp(Number(v.height) || DEFAULT_HEIGHT, MIN_HEIGHT, MAX_HEIGHT));
      selected = null;
      showTab();
      applyHeight();
    },
    setTab,
    detectChords,
    openPads
  };

  Hub.commands.detectChords = () => detectChords();
  Hub.commands.chordPads = () => openPads();

  Hub.on("tempoMap", () => { if (!drag) { previewMap = null; previewMain = null; } draw(); fillPads(); });
  Hub.on("theme", draw);
  // Main's fader moved elsewhere: an empty Main Volume lane shows where it is.
  Hub.on("status", (s) => { if (shown && s && s.masterVolume !== undefined && !mainPoints().length) draw(); });
  Hub.on("tracks", () => { if (shown && !mainPoints().length) draw(); });

  Hub.on("ready", () => {
    buildHeads();
    const canvas = $("song-lanes");
    canvas.addEventListener("pointerdown", onDown);
    canvas.addEventListener("pointermove", onMove);
    canvas.addEventListener("pointerup", onUp);
    canvas.addEventListener("pointercancel", onUp);
    canvas.addEventListener("dblclick", onDoubleClick);
    canvas.addEventListener("contextmenu", onContextMenu);
    $("song-channel-toggle").addEventListener("click", () => Hub.run("toggleSongChannel"));

    // Ctrl + wheel zooms here too, as over the lanes.
    $("song-lanes-wrap").addEventListener("wheel", (event) => {
      if (!event.ctrlKey || !Hub.arrange) return;
      event.preventDefault();
      Hub.arrange.setZoom(Hub.arrange.zoom() * (event.deltaY < 0 ? 1.2 : 1 / 1.2), event.clientX - $("lanes").getBoundingClientRect().left);
    }, { passive: false });

    // A press anywhere else in the arrangement leaves the song channel's selection.
    for (const id of ["lanes", "ruler-wrap", "tracklist"]) {
      $(id).addEventListener("pointerdown", () => { if (selected) { selected = null; draw(); } }, true);
    }

    // Delete takes the selected change or point away, before anything else.
    const previous = Hub.commands.deleteSelection;
    Hub.commands.deleteSelection = () => {
      if (shown && selected && Hub.state.focus === "arrange") {
        removeItem(selected.lane, selected.index);
        return;
      }
      if (previous) previous();
    };

    show(false);
    requestAnimationFrame(tick);
  });
})();
