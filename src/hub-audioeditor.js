// D:\workspace\Fanan_web_hub\src\hub-audioeditor.js
//
// The Audio Editor: an audio clip big, on the song's bars, with the Audio menu's changes. It
// shares the space at the bottom with the Editor and the Console; its button is next to the
// Editor's (F2 stays the Editor's). It shows the audio clip selected in the arrangement, or
// the one double-clicked.
//
//   Clip tempo   the clip's own tempo (Detect listens for it)
//   Sync Tempo   a stretched copy whose beats land on the song's; the original is kept
//   Follow       the same, stretched again whenever the song's tempo changes under it
//   Pitch        semitones up or down (and hundredths), its length kept
//   Volume       the clip's own volume
//   Audio ▾      Reverse, Normalize, Audio to MIDI (Notes or Drums, on a new track), Back to
//                the Original
// The Event menu has the same, and so does an audio clip's right-click menu in the arrangement;
// AI Tools' Tempo & Pitch and Audio to MIDI tabs run them too (Hub.audioOps), and say themselves
// how they went.
//   Stretcher    drag over the waveform to choose a part of the clip, then drag its right edge
//                to stretch it longer or squeeze it shorter (what's after it moves along), or its
//                left edge (what's before it moves, the clip starting earlier or later). The clip
//                is rendered from its original, stretches and all; its bends show as marks.
//   Markers      warp markers: double-click the waveform to put one (Snap to hit: on the nearest
//                hit close by), drag one along to move that moment of the audio in time (snapped
//                to the grid as the arrangement snaps): only what's between the markers either
//                side of it stretches or squeezes. Double-click or right-click one to take it
//                away. A marker that has moved something is part of the clip (its stretch map);
//                one only put down stays while Sonify runs.
//   Slice        Slice to Sampler (the Audio menu too): the clip's cuts shown on it, from its hits
//                (a sensitivity), its warp markers or the grid; drag a cut to move it, double-click
//                to add one (at the hit close by, cutting at hits), double-click or right-click one
//                to take it away. Slice to Sampler makes each slice a key of a Sonify Sampler on a
//                new track under the clip's, from C2 (note 36) up, with a MIDI clip playing them
//                where the clip did; the clip stays as it is. One Undo step.
// Silence Areas (the Audio menu, the clip's icon): wherever the clip stays quieter than a
// threshold it goes silent, a noise gate over the whole clip; its window previews it on the clip.
// Each change is one Undo step in the engine. Click the waveform to move the song position;
// Ctrl + wheel zooms.

(() => {
  "use strict";

  const { $ } = Hub;
  const MIN_ZOOM = 4, MAX_ZOOM = 2000;
  const ae = { trackId: 0, clipId: 0, zoom: 0, busy: null, pendingSync: null, autoZoom: true, quiet: false,
               stretcher: false, stretch: null, stretchDrag: null,     // stretch: the part chosen, {from, to} in the clip's seconds
               markers: false, snapHit: true, markDrag: null, warpSent: null,     // warpSent: a marker move on its way {clipId, at}
               slicing: false, slice: null, sliceDrag: null };   // slice: {clipId, file, offset, length, by, cuts: [seconds shown], waiting}

  const panelOpen = () => !document.body.classList.contains("hide-audio-editor");
  const map = () => Hub.tempoMap;

  /** The clip shown and its track (null when it's gone). */
  function current() {
    const track = Hub.trackById(ae.trackId);
    const clip = track ? (track.clips || []).find((c) => c.id === ae.clipId && c.audioFile) : null;
    return clip ? { track, clip } : null;
  }

  /** The audio clip a command works on: the selected one (or the one shown). */
  function target() {
    const chosen = Hub.state.selectedClip;
    if (chosen) {
      const found = Hub.findClip(chosen.clipId);
      if (found && found.clip.audioFile) return { track: found.track, clip: found.clip };
    }
    return current();
  }

  // ---------------------------------------------------------------- geometry
  const wrap = () => $("audio-wave-wrap");
  const xOf = (ppq) => ppq * ae.zoom - wrap().scrollLeft;
  const ppqAtX = (x) => Math.max(0, (x + wrap().scrollLeft) / ae.zoom);

  /** The song position where the clip's file is at a time (seconds into the file). */
  function ppqOfFile(clip, seconds) {
    const m = map();
    return m.ppqAt(m.secondsAt(clip.start) + seconds - (clip.audioOffset || 0));
  }

  function fileSecondsAt(clip, ppq) {
    const m = map();
    return (clip.audioOffset || 0) + m.secondsAt(ppq) - m.secondsAt(clip.start);
  }

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

  /** The zoom that shows the whole clip, with a little room either side. */
  function fitZoom(clip) {
    const width = wrap().clientWidth || 800;
    return Hub.clamp(width * 0.86 / Math.max(0.25, clip.length), MIN_ZOOM, MAX_ZOOM);
  }

  function updateSize(clip) {
    const end = clip ? Math.max(clip.start + clip.length, ppqOfFile(clip, Hub.state.peaks[clip.audioFile] ? Hub.state.peaks[clip.audioFile].length || 0 : 0)) : 16;
    $("audio-size").style.width = Math.ceil((end + 8) * ae.zoom) + "px";
    $("audio-size").style.height = "1px";
  }

  // ---------------------------------------------------------------- drawing
  function drawRuler(clip) {
    const canvas = $("audio-ruler");
    const width = wrap().clientWidth, height = canvas.parentElement.clientHeight;
    const context = sizeCanvas(canvas, width, height);
    if (!context) return;
    context.fillStyle = "#1d1e21";
    context.fillRect(0, 0, width, height);

    const from = wrap().scrollLeft / ae.zoom, to = (wrap().scrollLeft + width) / ae.zoom + 1e-6;
    const tick = map().gridStep(from, ae.zoom, 8), label = map().gridStep(from, ae.zoom, 56, 0.25);
    const labelled = new Set(map().lines(from, to, label).map((line) => line.ppq.toFixed(6)));
    context.font = "11px Segoe UI, system-ui, sans-serif";
    context.textBaseline = "top";

    for (const line of map().lines(from, to, tick)) {
      const x = Math.round(xOf(line.ppq)) + 0.5;
      const isLabel = labelled.has(line.ppq.toFixed(6));
      context.strokeStyle = line.bar ? Hub.theme.muted : Hub.theme.tick;
      context.beginPath();
      context.moveTo(x, isLabel ? 4 : height - 6);
      context.lineTo(x, height);
      context.stroke();
      if (isLabel) {
        context.fillStyle = line.bar ? Hub.theme.text : Hub.theme.muted;
        context.fillText(map().label(line.ppq, label.q), x + 3, 4);
      }
    }

    // The clip's extent along the bottom.
    if (clip) {
      const track = Hub.trackById(ae.trackId);
      context.fillStyle = (track && track.colour) || Hub.theme.accent;
      context.fillRect(xOf(clip.start), height - 3, clip.length * ae.zoom, 3);
    }
    context.fillStyle = Hub.theme.line;
    context.fillRect(0, height - 1, width, 1);
  }

  function drawWave(clip) {
    const canvas = $("audio-wave");
    const width = wrap().clientWidth, height = wrap().clientHeight;
    const context = sizeCanvas(canvas, width, height);
    if (!context) return;
    const colours = Hub.theme.lanes;
    context.fillStyle = colours.empty;
    context.fillRect(0, 0, width, height);
    if (!clip) return;

    // The bars behind.
    const from = wrap().scrollLeft / ae.zoom, to = (wrap().scrollLeft + width) / ae.zoom + 1e-6;
    for (const line of map().lines(from, to, map().gridStep(from, ae.zoom, 12))) {
      const x = Math.round(xOf(line.ppq)) + 0.5;
      context.strokeStyle = line.bar ? colours.bar : line.beat ? colours.beat : colours.fine;
      context.beginPath();
      context.moveTo(x, 0);
      context.lineTo(x, height);
      context.stroke();
    }

    // The clip's window lit; the rest of the file (trimmed away) dim.
    const x0 = xOf(clip.start), x1 = xOf(clip.start + clip.length);
    const track = Hub.trackById(ae.trackId);
    context.globalAlpha = 0.28;
    context.fillStyle = (track && track.colour) || Hub.theme.accent;
    context.fillRect(x0, 0, x1 - x0, height);
    context.globalAlpha = 1;

    const coarse = Hub.state.peaks[clip.audioFile], fine = Hub.state.finePeaks[clip.audioFile];
    if (!coarse) {
      context.fillStyle = Hub.theme.muted;
      context.font = "12px Segoe UI, system-ui, sans-serif";
      context.fillText(clip.missing ? "The clip's audio is missing." : "Loading the waveform…", Math.max(8, x0 + 8), 20);
      drawStretch(context, clip, height);
      return;
    }

    const pixelsPerSecond = ae.zoom * map().bpmAt(clip.start) / 60;
    const wantFine = pixelsPerSecond > 1.5 * coarse.rate / coarse.bucket;
    if (wantFine && !fine && ae.askedFine !== clip.audioFile) { ae.askedFine = clip.audioFile; Hub.send("audioPeaksFine", { file: clip.audioFile }); }
    const peaks = wantFine && fine ? fine : coarse;
    const perBucket = peaks.bucket / peaks.rate, buckets = peaks.peaks.length / 2;
    const gain = Math.pow(10, (clip.gain || 0) / 20);
    const middle = height / 2, scale = Math.min(gain, 2) * (height / 2 - 6) / 127;
    const fileStart = Math.max(-8, xOf(ppqOfFile(clip, 0))), fileEnd = Math.min(width + 8, xOf(ppqOfFile(clip, coarse.length || 0)));

    context.fillStyle = "rgba(18, 19, 22, 0.75)";
    context.fillRect(Math.max(0, fileStart), Math.round(middle), Math.max(0, fileEnd - Math.max(0, fileStart)), 1);
    let next = fileSecondsAt(clip, ppqAtX(Math.floor(Math.max(0, fileStart))));

    for (let x = Math.floor(Math.max(0, fileStart)); x < fileEnd; x++) {
      const a = next;
      next = fileSecondsAt(clip, ppqAtX(x + 1));
      let first = Math.floor(a / perBucket), last = Math.max(first, Math.ceil(next / perBucket) - 1);
      if (last < 0 || first >= buckets) continue;
      first = Math.max(0, first);
      last = Math.min(buckets - 1, last);
      let high = 0, low = 0;
      for (let b = first; b <= last; b++) { high = Math.max(high, peaks.peaks[2 * b]); low = Math.min(low, peaks.peaks[2 * b + 1]); }
      const inside = x >= x0 && x < x1;
      context.fillStyle = inside ? "rgba(18, 19, 22, 0.85)" : "rgba(18, 19, 22, 0.35)";
      context.fillRect(x, middle - high * scale, 1, Math.max(1, (high - low) * scale));
    }

    // Fades, as the arrangement draws them.
    const fade = (fromPpq, toPpq, rising) => {
      const a = xOf(fromPpq), b = xOf(toPpq);
      if (b - a < 1) return;
      context.beginPath();
      context.moveTo(a, 0);
      for (let i = 0; i <= 24; i++) {
        const t = i / 24, level = Math.sin(Math.PI / 2 * (rising ? t : 1 - t));
        context.lineTo(a + (b - a) * t, height - level * height);
      }
      context.lineTo(b, 0);
      context.closePath();
      context.fillStyle = "rgba(0, 0, 0, 0.3)";
      context.fill();
    };
    if (clip.fadeIn > 0) fade(clip.start, ppqOfFile(clip, (clip.audioOffset || 0) + clip.fadeIn), true);
    if (clip.fadeOut > 0) fade(ppqOfFile(clip, (clip.audioOffset || 0) + clip.audioLength - clip.fadeOut), clip.start + clip.length, false);

    context.strokeStyle = "#ffffff";
    context.strokeRect(Math.round(x0) + 0.5, 0.5, Math.max(1, Math.round(x1 - x0) - 1), height - 1);

    drawStretch(context, clip, height);
  }

  // ---------------------------------------------------------------- the Stretcher
  // x of a moment of what the clip shows (seconds from its left edge), and back.
  const xOfShown = (clip, seconds) => xOf(ppqOfFile(clip, (clip.audioOffset || 0) + seconds));
  const shownAtX = (clip, x) => Hub.clamp(fileSecondsAt(clip, ppqAtX(x)) - (clip.audioOffset || 0), 0, clip.audioLength);
  const EDGE = 6;   // px either side of a chosen part's edge that grab it

  function drawStretch(context, clip, height) {
    if (ae.slicing) { drawSlices(context, clip, height); return; }
    if (ae.markers) { drawMarkers(context, clip, height); return; }

    // Where it was stretched before: marks along the top.
    if (clip.warp && clip.warp.length > 2) {
      context.fillStyle = Hub.accentLine();
      for (const [t] of clip.warp.slice(1, -1)) {
        const s = t - (clip.audioOffset || 0);
        if (s <= 0 || s >= clip.audioLength) continue;
        const x = Math.round(xOfShown(clip, s));
        context.beginPath();
        context.moveTo(x - 4, 0);
        context.lineTo(x + 4, 0);
        context.lineTo(x, 6);
        context.closePath();
        context.fill();
        context.fillRect(x, 6, 1, height - 6);
      }
    }

    if (!ae.stretcher || !ae.stretch) return;
    const { from, to } = ae.stretch;
    const a = xOfShown(clip, from), b = xOfShown(clip, to);
    context.fillStyle = Hub.accentRgba(0.22);
    context.fillRect(a, 0, b - a, height);
    context.fillStyle = Hub.accentLine();
    context.fillRect(Math.round(a) - 1, 0, 2, height);
    context.fillRect(Math.round(b) - 1, 0, 2, height);

    // Being stretched: where it would go, and by how much.
    const d = ae.stretchDrag;
    if (d && d.kind === "edge" && Math.abs(d.length - (to - from)) > 1e-4) {
      const m = map(), start = m.secondsAt(clip.start);
      const newA = d.edge === "end" ? a : xOf(m.ppqAt(start + to - d.length));
      const newB = d.edge === "end" ? xOf(m.ppqAt(start + from + d.length)) : b;
      context.strokeStyle = "#ffd166";
      context.setLineDash([5, 3]);
      context.strokeRect(Math.round(newA) + 0.5, 1.5, Math.round(newB - newA), height - 3);
      context.setLineDash([]);
      const ratio = d.length / (to - from);
      const text = `\u00d7${ratio.toFixed(2)} \u00b7 ${(to - from).toFixed(2)} s \u2192 ${d.length.toFixed(2)} s`;
      context.font = "12px Segoe UI, system-ui, sans-serif";
      context.textBaseline = "top";
      const w = context.measureText(text).width + 12;
      const tx = Hub.clamp((newA + newB) / 2 - w / 2, 4, wrap().clientWidth - w - 4);
      context.fillStyle = "rgba(21, 22, 26, 0.9)";
      context.fillRect(tx, 8, w, 20);
      context.fillStyle = "#ffd166";
      context.fillText(text, tx + 6, 11);
    }
  }

  function setStretcher(on) {
    if (on && ae.slicing) setSlicing(false);
    ae.stretcher = on;
    ae.stretch = null;
    ae.stretchDrag = null;
    $("audio-stretcher").setAttribute("aria-pressed", String(on));
    $("audio-editor").classList.toggle("stretching", on);
    if (on && ae.markers) setMarkers(false);
    render();
  }

  // ---------------------------------------------------------------- warp markers
  // A marker is a moment of what the clip shows (seconds from its left edge). The clip's own are
  // the inner points of its stretch map (clip.warp); the ones only put down are kept here, by clip.
  const MARK_GRAB = 6;                 // px either side of a marker that grab it
  const putMarks = new Map();          // clipId -> {file, offset, list: [seconds shown]}

  function setMarkers(on) {
    ae.markers = on;
    ae.markDrag = null;
    $("audio-markers").setAttribute("aria-pressed", String(on));
    $("audio-snaphit").hidden = !on;
    $("audio-editor").classList.toggle("marking", on);
    $("audio-editor").classList.remove("on-mark");
    if (on && ae.stretcher) setStretcher(false);
    if (on && ae.slicing) setSlicing(false);
    if (on) askFine();
    render();
  }

  /** Snap to hit wants the waveform's fine peaks of the clip shown. */
  function askFine() {
    const found = current();
    if (found && !Hub.state.finePeaks[found.clip.audioFile] && ae.askedFine !== found.clip.audioFile) {
      ae.askedFine = found.clip.audioFile;
      Hub.send("audioPeaksFine", { file: found.clip.audioFile });
    }
  }

  /** The clip's own markers: its stretch map's inner points. */
  function clipMarks(clip) {
    if (!clip.warp || clip.warp.length <= 2) return [];
    const offset = clip.audioOffset || 0;
    return clip.warp.slice(1, -1).map(([t]) => t - offset).filter((s) => s > 0.005 && s < clip.audioLength - 0.005);
  }

  /** The markers only put down, brought up to date with the clip: a trim moves them along, a
      marker move's new render keeps them (what it shows is the same), anything else that made
      the clip again lets them go. */
  function putMarksOf(clip) {
    const entry = putMarks.get(clip.id);
    if (!entry) return [];
    const offset = clip.audioOffset || 0;

    if (entry.file !== clip.audioFile) {
      const sent = ae.warpSent;
      if (!sent || sent.clipId !== clip.id) { putMarks.delete(clip.id); return []; }
      ae.warpSent = null;
      entry.file = clip.audioFile;
      entry.offset = offset;
      entry.list = entry.list.filter((s) => Math.abs(s - sent.at) > 0.003);   // the one moved is the clip's own now
    } else if (Math.abs(entry.offset - offset) > 1e-9) {
      const shift = entry.offset - offset;
      entry.list = entry.list.map((s) => s + shift);
      entry.offset = offset;
    }

    entry.list = entry.list.filter((s) => s > 0.005 && s < clip.audioLength - 0.005);
    return entry.list;
  }

  /** Every marker, in order: {s, own} (own: the clip's, else only put down). */
  function allMarks(clip) {
    const own = clipMarks(clip);
    const marks = own.map((s) => ({ s, own: true }));
    for (const s of putMarksOf(clip)) if (!own.some((o) => Math.abs(o - s) < 0.003)) marks.push({ s, own: false });
    return marks.sort((a, b) => a.s - b.s);
  }

  /** The marker under x, if any. */
  function markAt(clip, x) {
    let best = null, distance = MARK_GRAB + 1;
    for (const mark of allMarks(clip)) {
      const d = Math.abs(xOfShown(clip, mark.s) - x);
      if (d < distance) { best = mark; distance = d; }
    }
    return best;
  }

  /** The markers either side of a moment (the clip's ends where there are none). */
  function neighbours(clip, s) {
    let prev = 0, next = clip.audioLength;
    for (const mark of allMarks(clip)) {
      if (mark.s < s - 0.001) prev = Math.max(prev, mark.s);
      else if (mark.s > s + 0.001) next = Math.min(next, mark.s);
    }
    return { prev, next };
  }

  /** Snap to hit: the attack of the nearest hit within 60 ms of a moment, from the fine peaks
      (the moment itself when there's none, or the peaks aren't here yet). */
  function hitNear(clip, s) {
    const fine = Hub.state.finePeaks[clip.audioFile];
    if (!fine || !fine.peaks || !fine.rate || !fine.bucket) return s;
    const per = fine.bucket / fine.rate, count = Math.floor(fine.peaks.length / 2);
    const level = (b) => (b < 0 || b >= count ? 0 : Math.max(fine.peaks[2 * b], -fine.peaks[2 * b + 1]));
    const offset = clip.audioOffset || 0;
    const centre = Math.round((offset + s) / per), reach = Math.round(0.06 / per);
    const behind = Math.max(2, Math.round(0.01 / per)), ahead = Math.max(1, Math.round(0.005 / per));
    let best = -1, bestRise = 0, bestBefore = 0, bestScore = 0;

    for (let b = centre - reach; b <= centre + reach; b++) {
      let before = 0, after = 0;
      for (let k = 1; k <= behind; k++) before = Math.max(before, level(b - k));
      for (let k = 0; k < ahead; k++) after = Math.max(after, level(b + k));
      const rise = after - before;
      // A strong rise wins; nearer the click wins a near tie.
      const score = rise * (1 - 0.3 * Math.abs(b - centre) / Math.max(1, reach));
      if (rise > 0 && score > bestScore) { best = b; bestRise = rise; bestBefore = before; bestScore = score; }
    }

    if (best < 0 || bestRise < 12) return s;

    // The attack: the first bucket of the rise past halfway.
    let onset = best;
    for (let k = 0; k < ahead; k++) if (level(best + k) >= bestBefore + bestRise / 2) { onset = best + k; break; }
    return Hub.clamp(onset * per - offset, 0.006, clip.audioLength - 0.006);
  }

  function putMark(clip, s) {
    if (s <= 0.005 || s >= clip.audioLength - 0.005) return;
    if (allMarks(clip).some((mark) => Math.abs(mark.s - s) < 0.003)) return;
    putMarksOf(clip);
    const entry = putMarks.get(clip.id) || { file: clip.audioFile, offset: clip.audioOffset || 0, list: [] };
    entry.list.push(s);
    putMarks.set(clip.id, entry);
    render();
  }

  function takeMark(found, mark) {
    if (!mark.own) {
      const entry = putMarks.get(found.clip.id);
      if (entry) entry.list = entry.list.filter((s) => Math.abs(s - mark.s) > 0.003);
      render();
      return;
    }
    if (found.clip.followTempo) { Hub.toast("The clip follows the song's tempo: turn Follow off first."); return; }
    send("warpRemove", { at: mark.s }, found);
  }

  /** Where a marker dragged to x would go (seconds shown): on the grid as the arrangement snaps,
      between the markers either side of it, and no more than 8 times longer or shorter. */
  function markTarget(clip, d, x) {
    const m = map();
    const ppq = ppqAtX(x);
    const snapped = Hub.arrange && Hub.arrange.snap ? Hub.arrange.snap(ppq) : ppq;
    const s = m.secondsAt(snapped) - m.secondsAt(clip.start);
    const { prev, next } = d;
    const low = Math.max(prev + 0.01, prev + (d.at - prev) / 8, next - (next - d.at) * 8);
    const high = Math.min(next - 0.01, prev + (d.at - prev) * 8, next - (next - d.at) / 8);
    return low < high ? Hub.clamp(s, low, high) : d.at;
  }

  function drawMarkers(context, clip, height) {
    const d = ae.markDrag;
    context.font = "11px Segoe UI, system-ui, sans-serif";
    context.textBaseline = "top";

    for (const mark of allMarks(clip)) {
      const moving = d && Math.abs(d.at - mark.s) < 1e-9;
      const x = Math.round(xOfShown(clip, mark.s)) + 0.5;
      context.globalAlpha = moving ? 0.35 : 1;
      context.strokeStyle = context.fillStyle = "#ffd166";
      context.beginPath();
      context.moveTo(x - 5, 0);
      context.lineTo(x + 5, 0);
      context.lineTo(x, 8);
      context.closePath();
      if (mark.own) context.fill(); else { context.lineWidth = 1.2; context.stroke(); }
      context.fillRect(x - 0.5, 8, 1, height - 8);
      context.globalAlpha = 1;
    }

    // Being dragged: where it goes, and what either side becomes.
    if (d && Math.abs(d.newAt - d.at) > 1e-4) {
      const x = Math.round(xOfShown(clip, d.newAt)) + 0.5;
      context.strokeStyle = "#ffd166";
      context.setLineDash([5, 3]);
      context.beginPath();
      context.moveTo(x, 0);
      context.lineTo(x, height);
      context.stroke();
      context.setLineDash([]);
      const before = (d.newAt - d.prev) / (d.at - d.prev), after = (d.next - d.newAt) / (d.next - d.at);
      const text = `\u00d7${before.toFixed(2)} | \u00d7${after.toFixed(2)}`;
      const w = context.measureText(text).width + 12;
      const tx = Hub.clamp(x - w / 2, 4, wrap().clientWidth - w - 4);
      context.fillStyle = "rgba(21, 22, 26, 0.9)";
      context.fillRect(tx, 12, w, 18);
      context.fillStyle = "#ffd166";
      context.fillText(text, tx + 6, 15);
    }
  }

  /** A press on the waveform with Markers on: a marker grabbed (true), else nothing here. */
  function markDown(event) {
    const found = current();
    if (!found || ae.busy) return false;
    const clip = found.clip;
    const x = event.clientX - wrap().getBoundingClientRect().left;
    const mark = markAt(clip, x);
    if (!mark) return false;
    event.preventDefault();
    if (clip.followTempo) { Hub.toast("The clip follows the song's tempo: turn Follow off first, then move its markers."); return true; }
    const { prev, next } = neighbours(clip, mark.s);
    $("audio-wave").setPointerCapture(event.pointerId);
    ae.markDrag = { pointer: event.pointerId, at: mark.s, newAt: mark.s, prev, next, startX: x, moved: false };
    render();
    return true;
  }

  function markMove(event) {
    const found = current();
    if (!found) return;
    const clip = found.clip;
    const x = event.clientX - wrap().getBoundingClientRect().left;
    const d = ae.markDrag;
    if (!d) {
      $("audio-editor").classList.toggle("on-mark", Boolean(markAt(clip, x)));
      return;
    }
    if (event.pointerId !== d.pointer) return;
    if (!d.moved && Math.abs(x - d.startX) < 3) return;   // a click, not a drag yet
    d.moved = true;
    d.newAt = markTarget(clip, d, x);
    render();
  }

  function markUp(event) {
    const d = ae.markDrag;
    if (!d || event.pointerId !== d.pointer) return;
    ae.markDrag = null;
    const found = current();
    if (found && event.type === "pointerup" && d.moved && Math.abs(d.newAt - d.at) > 0.001) {
      if (send("warpMove", { from: d.prev, to: d.next, at: d.at, newAt: d.newAt }, found))
        ae.warpSent = { clipId: found.clip.id, at: d.at };
    }
    render();
  }

  function markDoubleClick(event) {
    const found = current();
    if (!found || ae.busy) return;
    const clip = found.clip;
    const x = event.clientX - wrap().getBoundingClientRect().left;
    const mark = markAt(clip, x);
    if (mark) { takeMark(found, mark); return; }
    const s = shownAtX(clip, x);
    putMark(clip, ae.snapHit ? hitNear(clip, s) : s);
  }

  // ---------------------------------------------------------------- Slice to Sampler
  // The cuts are moments of what the clip shows (seconds from its left edge); the first slice
  // starts at the clip's start. They come from the clip's hits, its warp markers or the grid, and
  // are corrected by hand before the slices are made.
  const SLICE_COLOUR = "#4cc9f0", FIRST_KEY = 36, MOST_SLICES = 128 - FIRST_KEY;
  const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const noteName = (n) => `${NOTE_NAMES[n % 12]}${Math.floor(n / 12) - 1}`;   // middle C (60) is C4

  function setSlicing(on) {
    ae.slicing = on;
    ae.sliceDrag = null;
    ae.slice = null;
    $("audio-slice").setAttribute("aria-pressed", String(on));
    $("audio-slicebar").hidden = !on;
    $("audio-editor").classList.toggle("slicing", on);
    $("audio-editor").classList.remove("on-cut");
    if (on && ae.stretcher) setStretcher(false);
    if (on && ae.markers) setMarkers(false);
    if (on) { sliceCompute(); askFine(); }
    render();
  }

  /** The cuts again, from what the bar says (hits, warp markers or the grid). */
  function sliceCompute() {
    const found = current();
    if (!found) { ae.slice = null; return; }
    const clip = found.clip, m = map();
    const by = $("audio-slice-by").value;
    $("audio-slice-sens").hidden = by !== "hits";
    $("audio-slice-grid").hidden = by !== "grid";
    const slice = { clipId: clip.id, file: clip.audioFile, offset: clip.audioOffset || 0, length: clip.audioLength, by, cuts: [], waiting: null };
    let cuts = [];

    if (by === "hits") {
      const key = askTransients(found.track.id, clip, Number($("audio-slice-sens").value));
      if (transientTimes.has(key)) cuts = transientTimes.get(key).map((t) => t - slice.offset);
      else slice.waiting = key;
    } else if (by === "markers") {
      cuts = allMarks(clip).map((mark) => mark.s);
    } else {
      const step = $("audio-slice-grid").value, end = clip.start + clip.length, start = m.secondsAt(clip.start);
      const lines = [];
      if (step === "bar") {
        for (let bar = m.barAt(clip.start + 1e-9), guard = 0; bar.start < end && guard < 4096; bar = m.barAt(bar.end + 1e-9), guard++) lines.push(bar.start);
      } else {
        const size = Number(step);
        for (let p = Math.ceil(clip.start / size - 1e-9) * size, guard = 0; p < end && guard < 4096; p += size, guard++) lines.push(p);
      }
      cuts = lines.map((p) => m.secondsAt(p) - start);
    }

    slice.cuts = tidyCuts(cuts, slice.length);
    ae.slice = slice;
  }

  /** In order, inside the clip, none closer than 10 ms. */
  function tidyCuts(cuts, length) {
    const sorted = cuts.filter((s) => Number.isFinite(s) && s > 0.01 && s < length - 0.01).sort((a, b) => a - b);
    const tidy = [];
    for (const s of sorted) if (!tidy.length || s - tidy[tidy.length - 1] >= 0.01) tidy.push(s);
    return tidy;
  }

  /** The cuts of the clip shown, made again when the clip changed under them (null: nothing shown). */
  function sliceOf(clip) {
    const s = ae.slice;
    if (!s || s.clipId !== clip.id || s.file !== clip.audioFile || Math.abs(s.offset - (clip.audioOffset || 0)) > 1e-9
        || Math.abs(s.length - clip.audioLength) > 1e-9) sliceCompute();
    return ae.slice;
  }

  function showSliceCount(clip) {
    const label = $("audio-slice-count"), go = $("audio-slice-go");
    const s = clip ? sliceOf(clip) : null;
    if (!s) { label.textContent = ""; go.disabled = true; return; }
    if (s.waiting) { label.textContent = "Listening for hits\u2026"; label.classList.remove("too-many"); go.disabled = true; return; }
    const count = s.cuts.length + 1;
    const tooMany = count > MOST_SLICES;
    label.classList.toggle("too-many", tooMany);
    label.textContent = tooMany ? `${count} slices: ${MOST_SLICES} at most (take some cuts away)`
                                : count === 1 ? "1 slice: no cuts yet (double-click the waveform to put one)"
                                : `${count} slices \u00b7 ${noteName(FIRST_KEY)}\u2013${noteName(FIRST_KEY + count - 1)}`;
    go.disabled = tooMany || count < 2 || Boolean(ae.busy);
  }

  /** The cut under x (its index), or -1. */
  function cutAt(clip, x) {
    const s = sliceOf(clip);
    if (!s) return -1;
    let best = -1, distance = MARK_GRAB + 1;
    s.cuts.forEach((cut, i) => {
      const d = Math.abs(xOfShown(clip, cut) - x);
      if (d < distance) { best = i; distance = d; }
    });
    return best;
  }

  function drawSlices(context, clip, height) {
    const s = sliceOf(clip);
    if (!s) return;
    const edges = [0, ...s.cuts, clip.audioLength];
    context.font = "11px Segoe UI, system-ui, sans-serif";
    context.textBaseline = "top";

    for (let i = 0; i + 1 < edges.length; i++) {
      const a = xOfShown(clip, edges[i]), b = xOfShown(clip, edges[i + 1]);
      if (b < 0 || a > wrap().clientWidth) continue;
      if (i % 2 === 1) {
        context.fillStyle = "rgba(76, 201, 240, 0.08)";
        context.fillRect(a, 0, b - a, height);
      }
      if (i > 0) {
        const x = Math.round(a) + 0.5;
        context.fillStyle = SLICE_COLOUR;
        context.beginPath();
        context.moveTo(x - 5, 0);
        context.lineTo(x + 5, 0);
        context.lineTo(x, 8);
        context.closePath();
        context.fill();
        context.fillRect(x - 0.5, 8, 1, height - 8);
      }
      // Its number and its key, while there's room for them.
      if (b - a > 26 && i < MOST_SLICES) {
        const text = b - a > 52 ? `${i + 1} \u00b7 ${noteName(FIRST_KEY + i)}` : String(i + 1);
        context.fillStyle = "rgba(21, 22, 26, 0.85)";
        const w = context.measureText(text).width + 8;
        context.fillRect(a + 4, 11, w, 16);
        context.fillStyle = SLICE_COLOUR;
        context.fillText(text, a + 8, 13);
      }
    }
  }

  function sliceDown(event) {
    const found = current();
    if (!found || ae.busy) return false;
    const clip = found.clip, s = sliceOf(clip);
    const x = event.clientX - wrap().getBoundingClientRect().left;
    const index = cutAt(clip, x);
    if (!s || index < 0) return false;
    event.preventDefault();
    $("audio-wave").setPointerCapture(event.pointerId);
    ae.sliceDrag = { pointer: event.pointerId, index, startX: x, moved: false };
    return true;
  }

  function sliceMove(event) {
    const found = current();
    if (!found) return;
    const clip = found.clip, x = event.clientX - wrap().getBoundingClientRect().left;
    const d = ae.sliceDrag, s = sliceOf(clip);
    if (!d) { $("audio-editor").classList.toggle("on-cut", cutAt(clip, x) >= 0); return; }
    if (event.pointerId !== d.pointer || !s) return;
    if (!d.moved && Math.abs(x - d.startX) < 3) return;   // a click, not a drag yet
    d.moved = true;
    const low = (d.index > 0 ? s.cuts[d.index - 1] : 0) + 0.01;
    const high = (d.index + 1 < s.cuts.length ? s.cuts[d.index + 1] : clip.audioLength) - 0.01;
    if (low < high) s.cuts[d.index] = Hub.clamp(shownAtX(clip, x), low, high);
    render();
  }

  function sliceUp(event) {
    const d = ae.sliceDrag;
    if (!d || event.pointerId !== d.pointer) return;
    ae.sliceDrag = null;
    render();
  }

  /** A double-click: the cut there taken away, or one put there (at the hit close by, cutting at hits). */
  function sliceDoubleClick(event) {
    const found = current();
    if (!found || ae.busy) return;
    const clip = found.clip, s = sliceOf(clip);
    if (!s) return;
    const x = event.clientX - wrap().getBoundingClientRect().left;
    const index = cutAt(clip, x);
    if (index >= 0) s.cuts.splice(index, 1);
    else {
      const at = shownAtX(clip, x);
      s.cuts = tidyCuts([...s.cuts, s.by === "hits" ? hitNear(clip, at) : at], clip.audioLength);
    }
    render();
  }

  function sliceGo() {
    const found = current();
    const s = found ? sliceOf(found.clip) : null;
    if (!s || s.waiting) return;
    send("slice", { cuts: s.cuts.slice() }, found);
  }

  /** Which edge of the chosen part is under x, if any. */
  function edgeAt(clip, x) {
    if (!ae.stretch) return null;
    if (Math.abs(x - xOfShown(clip, ae.stretch.to)) <= EDGE) return "end";
    if (Math.abs(x - xOfShown(clip, ae.stretch.from)) <= EDGE) return "start";
    return null;
  }

  function stretchDown(event) {
    const found = current();
    if (!found || ae.busy) return;
    const clip = found.clip;
    if (clip.followTempo) { Hub.toast("The clip follows the song's tempo: turn Follow off first, then stretch it."); return; }
    event.preventDefault();
    const x = event.clientX - wrap().getBoundingClientRect().left;
    const edge = edgeAt(clip, x);
    $("audio-wave").setPointerCapture(event.pointerId);
    if (edge) ae.stretchDrag = { kind: "edge", edge, pointer: event.pointerId, length: ae.stretch.to - ae.stretch.from };
    else {
      const at = shownAtX(clip, x);
      ae.stretch = { from: at, to: at };
      ae.stretchDrag = { kind: "choose", pointer: event.pointerId, anchor: at };
    }
    render();
  }

  function stretchMove(event) {
    const found = current();
    if (!found) return;
    const clip = found.clip;
    const x = event.clientX - wrap().getBoundingClientRect().left;
    const d = ae.stretchDrag;
    if (!d) {
      $("audio-editor").classList.toggle("on-edge", Boolean(edgeAt(clip, x)));
      return;
    }
    if (event.pointerId !== d.pointer) return;
    const at = shownAtX(clip, x);
    if (d.kind === "choose") ae.stretch = { from: Math.min(d.anchor, at), to: Math.max(d.anchor, at) };
    else {
      // The part's new length, from where its other edge stays (in the song's seconds: a shown second is a song second).
      const m = map(), start = m.secondsAt(clip.start), t = m.secondsAt(ppqAtX(x)) - start;
      const span = ae.stretch.to - ae.stretch.from;
      const wanted = d.edge === "end" ? t - ae.stretch.from : ae.stretch.to - t;
      d.length = Hub.clamp(wanted, Math.max(0.01, span / 8), span * 8);
    }
    render();
  }

  function stretchUp(event) {
    const d = ae.stretchDrag;
    if (!d || event.pointerId !== d.pointer) return;
    ae.stretchDrag = null;
    const found = current();
    if (!found) return;
    if (d.kind === "choose") {
      if (ae.stretch.to - ae.stretch.from < 0.02) ae.stretch = null;   // a click: nothing chosen
      render();
      return;
    }
    const { from, to } = ae.stretch;
    if (event.type === "pointerup" && Math.abs(d.length - (to - from)) > 0.001) {
      if (send("stretch", { from, to, length: d.length, keepEnd: d.edge === "start" }, found))
        ae.stretch = { from, to: from + d.length };   // the part as it will be
    }
    render();
  }

  function placePlayhead() {
    const head = $("audio-playhead");
    if (!panelOpen() || !current()) { head.hidden = true; return; }
    const x = xOf(Hub.songPosition());
    head.hidden = x < 0 || x > wrap().clientWidth;
    head.style.transform = `translateX(${Math.round(x)}px)`;
  }

  /** What the clip is: its file, length, and what was done to it. */
  function describe(clip) {
    const parts = [`${clip.fileName}`, `${clip.audioLength.toFixed(2)} s`];
    if (clip.sourceFile) {
      const done = [];
      if (clip.sourceTempo > 0) done.push(`${clip.followTempo ? "follows the song's tempo" : "synced to the song"} from ${+clip.sourceTempo.toFixed(2)} BPM`);
      if (clip.pitch) done.push(`${clip.pitch > 0 ? "+" : ""}${+clip.pitch.toFixed(2)} semitones`);
      if (clip.reversed) done.push("reversed");
      if (clip.warp && clip.warp.length > 2) done.push("stretched in places");
      parts.push(`made from ${clip.sourceName}: ${done.join(", ")}`);
    }
    if (clip.gain) parts.push(`volume ${clip.gain > 0 ? "+" : ""}${clip.gain.toFixed(1)} dB`);
    return parts.join(" · ");
  }

  const showValue = (input, value) => { if (document.activeElement !== input && input.value !== value) input.value = value; };

  function render() {
    const found = current();
    const editor = $("audio-editor");
    editor.classList.toggle("empty", !found);
    if (!panelOpen()) return;
    const clip = found ? found.clip : null;

    if (found) {
      if (ae.autoZoom || !ae.zoom) {
        ae.zoom = fitZoom(clip);
        updateSize(clip);
        wrap().scrollLeft = Math.max(0, clip.start * ae.zoom - wrap().clientWidth * 0.07);
        ae.autoZoom = false;
      }
      updateSize(clip);   // a render can make it longer or shorter
      const bar = map().barAt(clip.start + 1e-9).index + 1;
      $("audio-title").textContent = `${found.track.name} · bar ${bar}`;
      $("audio-info").textContent = describe(clip);
      $("audio-follow").setAttribute("aria-pressed", String(Boolean(clip.followTempo)));
      showValue($("audio-pitch"), String(+(clip.pitch || 0).toFixed(2)));
      showValue($("audio-gain"), (clip.gain || 0).toFixed(1));
      if (clip.sourceTempo > 0) showValue($("audio-tempo"), String(+clip.sourceTempo.toFixed(2)));
    } else {
      $("audio-title").textContent = "";
      $("audio-info").textContent = "";
    }

    for (const id of ["audio-detect", "audio-sync", "audio-follow", "audio-menu", "audio-pitch", "audio-gain", "audio-tempo", "audio-stretcher", "audio-markers", "audio-slice"])
      $(id).disabled = !found || Boolean(ae.busy);

    if (ae.markers && found && ae.snapHit) askFine();   // Snap to hit reads the clip's fine waveform
    if (ae.slicing) {
      if (!found) setSlicing(false);
      else showSliceCount(clip);
    }

    drawRuler(clip);
    drawWave(clip);
    placePlayhead();
  }

  // ---------------------------------------------------------------- showing a clip
  Hub.openAudioEditor = (trackId, clipId) => {
    if (ae.trackId !== trackId || ae.clipId !== clipId) {
      ae.trackId = trackId;
      ae.clipId = clipId;
      ae.autoZoom = true;
      ae.askedFine = null;
      $("audio-tempo").value = "";
    }
    document.body.classList.remove("hide-audio-editor");
    document.body.classList.add("hide-editor", "hide-console");
    Hub.emit("layout");
    Hub.selectTrack(trackId, "keep");
    requestAnimationFrame(render);
  };

  /** The selection moved to another audio clip: the editor shows it (as Studio One's follows). */
  function followSelection() {
    const chosen = Hub.state.selectedClip;
    if (!chosen || (chosen.trackId === ae.trackId && chosen.clipId === ae.clipId)) return;
    const found = Hub.findClip(chosen.clipId);
    if (!found || !found.clip.audioFile) return;
    ae.trackId = found.track.id;
    ae.clipId = chosen.clipId;
    ae.autoZoom = true;
    ae.askedFine = null;
    $("audio-tempo").value = "";
    render();
  }

  // ---------------------------------------------------------------- the Audio menu
  /** quiet: AI Tools sent it, and its page says how it went (no notes here). */
  function send(op, extra = {}, chosen = target(), quiet = false) {
    if (!chosen) { Hub.toast("Select an audio clip first."); return false; }
    if (ae.busy) { Hub.toast("Sonify is still working on the last one."); return false; }
    ae.quiet = quiet;
    Hub.send("audioOp", { op, trackId: chosen.track.id, clipId: chosen.clip.id, ...extra });
    return true;
  }

  const typedTempo = () => {
    const value = Number($("audio-tempo").value);
    return Number.isFinite(value) && value >= 20 && value <= 400 ? value : 0;
  };

  /** Sync (or Follow) with the clip's tempo: the one given, the one typed, the one it was synced
      from, or heard first. */
  function syncTempo(follow, chosen = target(), given = 0, quiet = false) {
    if (!chosen) { Hub.toast("Select an audio clip first."); return false; }
    const tempo = given || (chosen.clip.id === ae.clipId ? typedTempo() : 0) || chosen.clip.sourceTempo || 0;
    if (tempo > 0) return send(follow ? "followTempo" : "syncTempo", follow ? { on: true, tempo } : { tempo }, chosen, quiet);
    if (!send("detectTempo", {}, chosen, quiet)) return false;
    ae.pendingSync = { follow, trackId: chosen.track.id, clipId: chosen.clip.id, quiet };
    return true;
  }

  function askPitch(chosen = target()) {
    if (!chosen) { Hub.toast("Select an audio clip first."); return; }
    if (chosen.clip.id !== ae.clipId) Hub.openAudioEditor(chosen.track.id, chosen.clip.id);
    requestAnimationFrame(() => { $("audio-pitch").focus(); $("audio-pitch").select(); });
  }

  function askToMidi(anchor, chosen = target()) {
    if (!chosen) { Hub.toast("Select an audio clip first."); return; }
    Hub.popupMenu(anchor, [{ group: "Audio to MIDI, on a new track" },
                           { label: "Notes: any instrument or voice, chords too", value: "notes" },
                           { label: "Drums: kick, snare and hi-hat", value: "drums" }],
                  (mode) => send("toMidi", { mode }, chosen));
  }

  // ---------------------------------------------------------------- Silence Areas
  const SILENCE_DEFAULT = { threshold: -50, shortest: 0.15, fade: 0.01 };
  const silenceAreas = new Map();      // what the engine said, by key: {areas: [[from, to] seconds of the file], silenced, length}
  const silenceAsked = new Set();
  let silenceOpen = null;              // the window's {trackId, clipId, settings, key, result}
  let silenceTimer = 0;

  const silenceKey = (clip, s) => `${clip.audioFile}|${clip.audioOffset}|${clip.audioLength}|${s.threshold}|${s.shortest}|${s.fade}`;

  function askAreas(trackId, clip, settings, key) {
    if (silenceAsked.has(key)) return;
    silenceAsked.add(key);
    Hub.send("silencePreview", { trackId, clipId: clip.id, silence: settings, key });
  }

  /** The stretches a clip is silent in (seconds of its file): its window's preview while open, else
      its own Silence Areas; null until the engine has said. */
  function areasFor(clip) {
    if (silenceOpen && silenceOpen.clipId === clip.id) {
      const preview = silenceAreas.get(silenceOpen.key);
      return preview ? preview.areas : null;
    }
    if (!clip.silence) return null;
    const key = silenceKey(clip, clip.silence);
    if (!silenceAreas.has(key)) {
      const found = Hub.findClip(clip.id);
      if (found) askAreas(found.track.id, clip, clip.silence, key);
      return null;
    }
    return silenceAreas.get(key).areas;
  }

  function setSilence(track, clip, settings) {
    Hub.send("clipEdits", { edits: [{ op: "silence", trackId: track.id, clipId: clip.id, silence: settings }],
                            label: settings ? "Silence Areas" : "Remove Silence Areas" });
  }

  function readSilenceWindow() {
    return {
      threshold: Hub.clamp(Number($("silence-threshold").value) || SILENCE_DEFAULT.threshold, -90, -10),
      shortest: Hub.clamp((Number($("silence-shortest").value) || 150) / 1000, 0.01, 5),
      fade: Hub.clamp((Number($("silence-fade").value) || 10) / 1000, 0.001, 0.5)
    };
  }

  /** The window's settings changed: shown at once, the clip's preview asked for a moment later. */
  function silenceChanged() {
    if (!silenceOpen) return;
    const found = Hub.findClip(silenceOpen.clipId);
    if (!found) { closeSilence(); return; }
    const settings = readSilenceWindow();
    $("silence-threshold-value").textContent = `${settings.threshold.toFixed(1)} dB`;
    silenceOpen.settings = settings;
    silenceOpen.key = silenceKey(found.clip, settings);
    clearTimeout(silenceTimer);
    silenceTimer = setTimeout(() => {
      if (!silenceOpen) return;
      if (silenceAreas.has(silenceOpen.key)) showSilenceResult();
      else askAreas(silenceOpen.trackId, found.clip, silenceOpen.settings, silenceOpen.key);
    }, 60);
    showSilenceResult();
    if (Hub.arrange) Hub.arrange.redraw();
  }

  function showSilenceResult() {
    if (!silenceOpen) return;
    const result = silenceAreas.get(silenceOpen.key);
    $("silence-result").textContent = !result ? "Listening…"
      : !result.areas.length ? "Nothing in the clip is that quiet: raise the threshold."
      : `${result.areas.length} ${result.areas.length === 1 ? "area" : "areas"} \u00b7 ${result.silenced.toFixed(1)} s of ${result.length.toFixed(1)} s silent (${Math.round(100 * result.silenced / Math.max(0.001, result.length))}%)`;
  }

  /** mode "strip": Strip Silence (the same window: the clip is cut, the quiet stretches gone). */
  function openSilence(track, clip, mode = "silence") {
    const strip = mode === "strip";
    const settings = clip.silence || SILENCE_DEFAULT;
    silenceOpen = { trackId: track.id, clipId: clip.id, settings, key: "", strip };
    $("silence-threshold").value = String(settings.threshold);
    $("silence-shortest").value = String(Math.round(settings.shortest * 1000));
    $("silence-fade").value = String(Math.round(settings.fade * 1000));
    $("silence-title").textContent = strip ? "Strip Silence" : "Silence Areas";
    $("silence-about").textContent = strip
      ? "The clip is cut into the parts that sound: wherever it stays quieter than the threshold, that stretch goes. Undo puts the clip back whole."
      : "Wherever the clip stays quieter than the threshold, it goes silent: background noise, hum and the room between phrases go. The audio file itself isn't changed, and the clip's icon brings this back.";
    $("silence-remove").hidden = strip || !clip.silence;
    $("silence-apply").textContent = strip ? "Strip" : clip.silence ? "Apply" : "Silence";
    $("silence-dialog").hidden = false;
    silenceChanged();
    $("silence-threshold").focus();
  }

  function closeSilence() {
    silenceOpen = null;
    clearTimeout(silenceTimer);
    $("silence-dialog").hidden = true;
    if (Hub.arrange) Hub.arrange.redraw();
  }

  /** The clip's Silence icon: its settings again, or none. */
  function silenceMenu(anchor, track, clip) {
    const s = clip.silence;
    if (!s) return;
    Hub.popupMenu(anchor, [
      { group: "Silence Areas" },
      { label: `Below ${s.threshold.toFixed(1)} dB, ${Math.round(s.shortest * 1000)} ms or longer`, value: "", disabled: true },
      { label: "Silence Settings\u2026", value: "settings" },
      { label: "Remove Silence", value: "remove", title: "The clip plays all of its sound again (Undo brings the silence back)" }
    ], (pick) => {
      if (pick === "settings") openSilence(track, clip);
      if (pick === "remove") setSilence(track, clip, null);
    });
  }

  Hub.silence = { areasFor, menu: silenceMenu, open: openSilence };

  Hub.on("silenceAreas", (reply) => {
    silenceAreas.set(reply.key, { areas: reply.areas || [], silenced: reply.silenced || 0, length: reply.length || 0 });
    silenceAsked.delete(reply.key);
    if (silenceAreas.size > 200) silenceAreas.delete(silenceAreas.keys().next().value);
    showSilenceResult();
    if (Hub.arrange) Hub.arrange.redraw();
  });

  /** The effects a clip can take (the Browser's), by vendor. */
  const effectChoices = () => Hub.state.library.filter((entry) => entry.kind === "effect" && !entry.missing)
                                               .sort((a, b) => (a.vendor || "").localeCompare(b.vendor || "") || a.name.localeCompare(b.name));

  /** A clip's own effect, in its menu: Edit and Remove it, or add (replace) one from the Browser's
      effects. Dropping an effect from the Browser onto the clip does the same. */
  function effectItems(clip) {
    const choices = effectChoices();
    const vendors = new Map();
    choices.forEach((entry, index) => {
      const vendor = entry.vendor || "Other";
      if (!vendors.has(vendor)) vendors.set(vendor, []);
      vendors.get(vendor).push({ label: entry.name, value: `fx:${index}`, title: `${entry.name} · ${entry.format || ""}` });
    });
    const submenu = [...vendors].map(([vendor, items]) => ({ label: vendor, submenu: items }));
    const effect = clip.effect;
    return [
      ...(effect ? [{ label: `Edit Effect (${effect.name})`, value: "fxEdit", disabled: !effect.slot || Boolean(effect.error), title: effect.error || "" },
                    { label: "Remove Effect", value: "fxRemove" }] : []),
      { label: effect ? "Replace Effect" : "Add Effect", submenu: submenu.length ? submenu : [{ label: "No effects in the Browser", value: "", disabled: true }],
        title: "An effect for this clip alone: the other clips on the track play as they are" }
    ];
  }

  /** The Audio menu as a popup (the toolbar's Audio button, an audio clip's right-click). */
  function audioMenu(anchor, chosen = target()) {
    if (!chosen) return;
    const clip = chosen.clip, track = chosen.track;
    const merge = Hub.clipTools.mergeable(track, clip);
    const shown = transientShown.get(clip.id);
    Hub.popupMenu(anchor, [
      { label: "Open in the Audio Editor", value: "open" },
      { label: "Automation", value: "automation", title: "The clip's own Gain, Pan and effect lines, drawn on the clip (they move and copy with it)" },
      { separator: true },
      { label: "Normalize", value: "normalize", title: "The clip's own volume set so its loudest moment is at 0 dB" },
      { label: "Reset Gain", value: "resetGain", disabled: !(clip.gain || clip.fadeIn || clip.fadeOut),
        title: "The clip's own volume back to 0 dB, and its fades off" },
      { separator: true },
      { label: clip.silence ? "Silence Areas Settings…" : "Silence Areas…", value: "silence",
        title: "Silent wherever the clip stays quieter than a threshold: takes background noise out (the clip stays whole)" },
      ...(clip.silence ? [{ label: "Remove Silence Areas", value: "silenceOff" }] : []),
      { label: "Strip Silence…", value: "strip", title: "The clip cut into the parts that sound: the quiet stretches between them go" },
      { label: "Transients", submenu: [
          { label: "Show Transients", value: "transShow", checked: Boolean(shown), title: "Marks where each hit or note starts" },
          { separator: true },
          ...SENSITIVITIES.map(([value, label]) => ({ label: `Sensitivity: ${label}`, value: "transSens:" + value,
                                                      checked: (shown ? shown.sensitivity : 0.5) === value })),
          { separator: true },
          { label: "Split at Transients", value: "transSplit", title: "The clip cut at each transient (Undo puts it back)" }
        ] },
      { separator: true },
      { label: merge.label, value: "merge", disabled: !merge.ok, title: merge.title },
      Hub.clipTools.bounceItem(track, clip),
      ...Hub.clipTools.exportItems(),
      { separator: true },
      { label: "Tempo & Pitch", submenu: [
          { label: "Sync Tempo", value: "sync" },
          { label: clip.followTempo ? "Stop Following the Song's Tempo" : "Follow Song Tempo", value: "follow" },
          { label: "Pitch…", value: "pitch" },
          { label: clip.reversed ? "Forwards Again" : "Reverse", value: "reverse" },
          { label: "Back to the Original", value: "original", disabled: !clip.sourceFile }
        ] },
      { label: "Audio to MIDI…", value: "toMidi" },
      { label: "Slice to Sampler…", value: "slice",
        title: "The clip cut at its hits, its warp markers or the grid (shown in the Audio Editor, for you to correct), each slice a key of a Sonify Sampler on a new track" },
      { label: "Clip Effect", submenu: effectItems(clip) },
      { label: "Song Builder Blocks", submenu: Hub.songBlockMenuItems || [] }
    ], (pick) => {
      if (pick === "sync") syncTempo(false, chosen);
      if (pick === "follow") { if (clip.followTempo) send("followTempo", { on: false }, chosen); else syncTempo(true, chosen); }
      if (pick === "pitch") askPitch(chosen);
      if (pick === "reverse") send("reverse", {}, chosen);
      if (pick === "normalize") send("normalize", {}, chosen);
      if (pick === "resetGain") resetGain(track, clip);
      if (pick === "toMidi") askToMidi(anchor, chosen);
      if (pick === "slice") { Hub.openAudioEditor(track.id, clip.id); setSlicing(true); }
      if (pick === "original") send("original", {}, chosen);
      if (pick === "open") Hub.openAudioEditor(track.id, clip.id);
      if (pick === "automation" && Hub.clipAutomation) Hub.clipAutomation.open(track, clip);
      if (pick === "silence") openSilence(track, clip);
      if (pick === "silenceOff") setSilence(track, clip, null);
      if (pick === "strip") openSilence(track, clip, "strip");
      if (pick === "transShow") showTransients(track, clip, !shown);
      if (typeof pick === "string" && pick.startsWith("transSens:")) showTransients(track, clip, true, Number(pick.slice(10)));
      if (pick === "transSplit") splitAtTransients(track, clip);
      if (pick === "merge") Hub.clipTools.merge(track, clip);
      if (pick === "bounce") Hub.clipTools.bounce(track, clip);
      if (pick === "export" || pick === "exportIn") Hub.clipTools.export(track, clip, pick === "exportIn");
      if (pick === "fxEdit" && clip.effect) Hub.openScreen(track.id, clip.effect.slot);
      if (pick === "fxRemove") Hub.send("clipEdits", { edits: [{ op: "effect", trackId: track.id, clipId: clip.id, remove: true }], label: "Remove Clip Effect" });
      if (typeof pick === "string" && pick.startsWith("fx:") && Hub.arrange.setClipEffect) {
        const entry = effectChoices()[Number(pick.slice(3))];
        if (entry) Hub.arrange.setClipEffect(track, clip, entry);
      }
      if (pick === "makeSongBlock" || pick === "songBlocks") Hub.run(pick);
    });
  }

  /** Reset Gain: the clip's own volume at 0 dB and its fades off (every selected audio clip). */
  function resetGain(track, clip) {
    const chosen = Hub.state.selectedClips.includes(clip.id) ? Hub.state.selectedClips : [clip.id];
    const edits = [];
    for (const id of chosen) {
      const found = Hub.findClip(id);
      if (found && found.clip.audioFile) edits.push({ op: "audio", trackId: found.track.id, clipId: id, gain: 0, fadeIn: 0, fadeOut: 0 });
    }
    if (edits.length) Hub.send("clipEdits", { edits, label: "Reset Gain" });
  }

  // ---------------------------------------------------------------- transients
  // Where each hit or note starts (the engine listens: Detect Transients), marked on the clip.
  const SENSITIVITIES = [[0.2, "Low"], [0.5, "Medium"], [0.85, "High"]];
  const transientShown = new Map();    // clip id: {sensitivity}
  const transientTimes = new Map();    // key: [seconds of the file]
  const transientAsked = new Set();
  const transientSplits = new Map();   // key: {trackId, clipId}: split once the times are in

  const transientKey = (clip, sensitivity) => `${clip.audioFile}|${clip.audioOffset}|${clip.audioLength}|${sensitivity}`;

  function askTransients(trackId, clip, sensitivity) {
    const key = transientKey(clip, sensitivity);
    if (transientTimes.has(key) || transientAsked.has(key)) return key;
    transientAsked.add(key);
    Hub.send("transients", { trackId, clipId: clip.id, sensitivity, key });
    return key;
  }

  function showTransients(track, clip, on, sensitivity) {
    const was = transientShown.get(clip.id);
    if (!on) transientShown.delete(clip.id);
    else {
      const value = sensitivity !== undefined ? sensitivity : was ? was.sensitivity : 0.5;
      transientShown.set(clip.id, { sensitivity: value });
      askTransients(track.id, clip, value);
    }
    if (Hub.arrange) Hub.arrange.redraw();
    render();
  }

  /** The times (seconds of the file) to mark on a clip; null when they aren't shown (or not in yet). */
  function transientsFor(clip) {
    const shown = transientShown.get(clip.id);
    if (!shown) return null;
    const key = transientKey(clip, shown.sensitivity);
    if (!transientTimes.has(key)) {
      const found = Hub.findClip(clip.id);
      if (found) askTransients(found.track.id, clip, shown.sensitivity);
      return null;
    }
    return transientTimes.get(key);
  }

  function splitAtTransients(track, clip) {
    const shown = transientShown.get(clip.id);
    const key = askTransients(track.id, clip, shown ? shown.sensitivity : 0.5);
    if (transientTimes.has(key)) doSplit(track.id, clip.id, transientTimes.get(key));
    else transientSplits.set(key, { trackId: track.id, clipId: clip.id });
  }

  function doSplit(trackId, clipId, times) {
    const found = Hub.findClip(clipId);
    if (!found) return;
    const clip = found.clip, m = map();
    const start = m.secondsAt(clip.start);
    // From the last back, so the clip being split keeps its id (the part left of each cut).
    const edits = times.slice().sort((a, b) => b - a)
      .map((t) => m.ppqAt(start + t - (clip.audioOffset || 0)))
      .filter((at) => at > clip.start + 1e-6 && at < clip.start + clip.length - 1e-6)
      .map((at) => ({ op: "split", trackId, clipId, at }));
    if (!edits.length) { Hub.toast("No transients found in the clip: try a higher sensitivity."); return; }
    Hub.send("clipEdits", { edits, label: "Split at Transients" });
  }

  Hub.on("transients", (reply) => {
    transientTimes.set(reply.key, reply.times || []);
    transientAsked.delete(reply.key);
    if (transientTimes.size > 200) transientTimes.delete(transientTimes.keys().next().value);
    const split = transientSplits.get(reply.key);
    if (split) { transientSplits.delete(reply.key); doSplit(split.trackId, split.clipId, reply.times || []); }
    if (ae.slicing && ae.slice && ae.slice.waiting === reply.key) sliceCompute();   // the hits are in: the cuts
    if (Hub.arrange) Hub.arrange.redraw();
    render();
  });

  Hub.transients = { timesFor: transientsFor };

  // ---------------------------------------------------------------- merging and exporting clips (audio and MIDI)
  /** The selected clips on the clip's track (the clip itself among them). */
  function selectedOnTrack(track, clip) {
    const ids = Hub.state.selectedClips.includes(clip.id) ? Hub.state.selectedClips : [clip.id];
    return (track.clips || []).filter((c) => ids.includes(c.id));
  }

  /** What Merge takes, as Studio One's Bounce Selection: the selected clips, and the clips of their
      kind between the first one and the last one's end (the muted ones stay as they are). */
  function mergeSpan(track, clip) {
    const selected = selectedOnTrack(track, clip);
    if (selected.length < 2) return selected;
    const audio = Boolean(clip.audioFile);
    const from = Math.min(...selected.map((c) => c.start)), to = Math.max(...selected.map((c) => c.start + c.length));
    return (track.clips || []).filter((c) => selected.includes(c)
      || (Boolean(c.audioFile) === audio && !c.muted && c.start >= from && c.start < to));
  }

  Hub.clipTools = {
    mergeable(track, clip) {
      const selected = selectedOnTrack(track, clip).length;
      const count = mergeSpan(track, clip).length;
      return { ok: selected >= 2, label: selected >= 2 ? `Merge ${count} Clips` : "Merge Clips",
               title: selected >= 2 ? (clip.audioFile ? "The selected clips on this track (and the clips between them) as one clip: each with its own effect, gain and fades, not the track's inserts"
                                                      : "The selected clips on this track (and the clips between them) as one clip")
                                    : "Select two or more clips on one track (Ctrl or Shift) to merge them" };
    },

    merge(track, clip) {
      if (selectedOnTrack(track, clip).length < 2) return;
      const clips = mergeSpan(track, clip).sort((a, b) => a.start - b.start);
      if (clip.audioFile) {
        Hub.send("mergeClips", { trackId: track.id, clipIds: selectedOnTrack(track, clip).map((c) => c.id) });
        return;
      }
      // MIDI: their notes (and bends) as one clip, from the first one's start to the last one's end.
      const from = Math.min(...clips.map((c) => c.start)), to = Math.max(...clips.map((c) => c.start + c.length));
      const notes = [], pitchBend = [], aftertouch = [];
      for (const c of clips) {
        const offset = c.offset || 0, end = c.start + c.length;
        for (const note of c.notes || []) {
          const [, start, length] = note;
          if (start < offset || start >= offset + c.length) continue;
          const at = c.start + start - offset;
          const merged = note.slice();
          merged[1] = at - from;
          merged[2] = Math.max(1 / 64, Math.min(length, end - at));
          notes.push(merged);
        }
        for (const [list, into] of [[c.pitchBend || [], pitchBend], [c.aftertouch || [], aftertouch]])
          for (const [time, value] of list)
            if (time >= offset && time < offset + c.length) into.push([c.start + time - offset - from, value]);
      }
      notes.sort((a, b) => a[1] - b[1]);
      // Their own automation lines too, each clip's stretch of them where it played (with its value
      // at its start, so a line picks up where the clip did).
      const lines = new Map();
      for (const c of clips) {
        const offset = c.offset || 0, base = c.start - offset - from;
        for (const env of c.envelopes || []) {
          const points = env.points || [];
          if (!points.length) continue;
          const at = (t) => {
            if (t <= points[0][0]) return points[0][1];
            if (t >= points[points.length - 1][0]) return points[points.length - 1][1];
            const i = points.findIndex((pt) => pt[0] > t);
            const [t0, v0] = points[i - 1], [t1, v1] = points[i];
            return t1 > t0 ? v0 + (v1 - v0) * (t - t0) / (t1 - t0) : v1;
          };
          const into = lines.get(env.key) || [];
          into.push([c.start - from, at(offset)]);
          for (const [t, v] of points) if (t > offset && t < offset + c.length) into.push([base + t, v]);
          into.push([c.start + c.length - from - 1e-4, at(offset + c.length)]);
          lines.set(env.key, into);
        }
      }
      const envelopes = [...lines].map(([key, points]) => ({ key, points: points.sort((a, b) => a[0] - b[0]) }));
      Hub.send("clipEdits", { label: "Merge Clips", edits: [
        ...clips.map((c) => ({ op: "delete", trackId: track.id, clipId: c.id })),
        { op: "add", trackId: track.id, start: from, length: to - from, offset: 0, notes, pitchBend: pitchBend.sort((a, b) => a[0] - b[0]),
          aftertouch: aftertouch.sort((a, b) => a[0] - b[0]), seed: clips[0].seed || 0, envelopes }
      ] });
    },

    /** Bounce to New Track, as Studio One's: the selected clips through the track (its instrument,
        its inserts, what rings on) onto a new audio track under it, the clips muted. */
    bounceItem(track, clip) {
      const count = selectedOnTrack(track, clip).length;
      return { label: count > 1 ? `Bounce ${count} Clips to New Track` : "Bounce to New Track", value: "bounce",
               title: "The clip" + (count > 1 ? "s" : "") + " as the track plays " + (count > 1 ? "them" : "it")
                      + " (its instrument and inserts) onto a new audio track under it; the original" + (count > 1 ? "s are" : " is") + " muted" };
    },

    bounce(track, clip) {
      const clips = selectedOnTrack(track, clip);
      if (clips.length) Hub.send("bounceClips", { trackId: track.id, clipIds: clips.map((c) => c.id) });
    },

    exportItems() {
      return [{ label: "Export Clip to Audio", value: "export", title: "A WAV of the clip as its track plays it, with its tempo, into the folder chosen last" },
              { label: "Export Clip to Audio In…", value: "exportIn", title: "Choose the folder (the next exports go there too)" }];
    },

    export(track, clip, choose) {
      Hub.send("exportClip", { trackId: track.id, clipId: clip.id, choose: Boolean(choose) });
    }
  };

  Hub.audioMenu = audioMenu;

  /** The same changes for AI Tools (hub-aitools.js), on a clip it names: false when one can't
      start (the clip is gone, or Sonify is still working on the last one). */
  const clipOf = (trackId, clipId) => {
    const track = Hub.trackById(trackId);
    const clip = track ? (track.clips || []).find((c) => c.id === clipId && c.audioFile) : null;
    return clip ? { track, clip } : null;
  };
  Hub.audioOps = {
    run: (op, extra, trackId, clipId) => { const found = clipOf(trackId, clipId); return found ? send(op, extra || {}, found, true) : false; },
    syncTempo: (follow, trackId, clipId, tempo = 0) => { const found = clipOf(trackId, clipId); return found ? syncTempo(follow, found, tempo, true) : false; },
    busy: () => ae.busy
  };
  Hub.commands.audioSyncTempo = () => syncTempo(false);
  Hub.commands.audioFollowTempo = () => { const t = target(); if (t && t.clip.followTempo) send("followTempo", { on: false }, t); else syncTempo(true, t); };
  Hub.commands.audioPitch = () => askPitch();
  Hub.commands.audioReverse = () => send("reverse");
  Hub.commands.audioNormalize = () => send("normalize");
  Hub.commands.audioOriginal = () => { const t = target(); if (t && !t.clip.sourceFile) Hub.toast("The clip plays its original already."); else send("original", {}, t); };
  Hub.commands.audioToMidi = () => {
    const anchor = $("audio-menu").offsetParent ? $("audio-menu") : { getBoundingClientRect: () => ({ left: window.innerWidth / 2 - 120, right: 0, top: 120, bottom: 120 }) };
    askToMidi(anchor);
  };

  // ---------------------------------------------------------------- what the engine says
  const OP_NAMES = { stretch: "Stretching", warpMove: "Moving the warp marker", warpRemove: "Taking the warp marker away", syncTempo: "Syncing the tempo", followTempo: "Stretching to the song's tempo", pitch: "Moving the pitch", reverse: "Reversing",
                     original: "Going back to the original", toMidi: "Listening for notes", detectTempo: "Listening for the tempo",
                     analyse: "Listening for the chords, key and tempo" };

  Hub.on("audioJob", (job) => {
    if (!job) return;
    if (job.state === "working") {
      ae.busy = job;
      $("audio-busy").hidden = false;
      $("audio-busy-text").textContent = `${OP_NAMES[job.op] || "Working"}… ${Math.round((job.progress || 0) * 100)}%`;
      render();
      return;
    }
    ae.busy = null;
    $("audio-busy").hidden = true;
    if (job.state === "failed" && job.error && !ae.quiet) Hub.toast(job.error);
    if (job.state === "done" && job.error && !ae.quiet) Hub.toast(job.error);
    if (job.state === "done" && job.op === "slice" && job.newTrackId) {
      Hub.toast(`The slices play on a new track under the clip's: a Sonify Sampler, from ${noteName(FIRST_KEY)} up.`);
      Hub.selectTrack(job.newTrackId, "only");
      if (ae.slicing) setSlicing(false);
    }
    if (job.state === "done" && job.op === "toMidi" && job.newTrackId && !ae.quiet) {
      Hub.toast("The notes are on a new track under the clip's.");
      Hub.selectTrack(job.newTrackId, "only");
    }
    if (job.state !== "done" && ae.pendingSync && job.op === "detectTempo") ae.pendingSync = null;
    if (job.state !== "done" && job.op === "warpMove") ae.warpSent = null;
    render();
  });

  Hub.on("audioTempo", (data) => {
    if (!data) return;
    if (data.trackId === ae.trackId && data.clipId === ae.clipId) $("audio-tempo").value = String(+Number(data.bpm).toFixed(2));
    const pending = ae.pendingSync;
    ae.pendingSync = null;

    // Detect Tempo was the first step of Sync or Follow: on with it.
    if (pending && pending.trackId === data.trackId && pending.clipId === data.clipId && data.bpm > 0) {
      ae.busy = null;
      const found = Hub.findClip(data.clipId);
      if (found) send(pending.follow ? "followTempo" : "syncTempo", pending.follow ? { on: true, tempo: data.bpm } : { tempo: data.bpm }, { track: found.track, clip: found.clip }, pending.quiet);
    } else if (!data.known && !ae.quiet) {
      Hub.toast(`The clip's tempo: ${+Number(data.bpm).toFixed(2)} BPM${data.steadiness !== undefined && data.steadiness < 0.5 ? " (it isn't very steady)" : ""}.`);
    }
  });

  // ---------------------------------------------------------------- wiring
  Hub.on("ready", () => {
    $("audio-close").addEventListener("click", () => Hub.run("toggleAudioEditor"));

    for (const id of ["silence-threshold", "silence-shortest", "silence-fade"]) $(id).addEventListener("input", silenceChanged);
    $("silence-cancel").addEventListener("click", closeSilence);
    $("silence-apply").addEventListener("click", () => {
      const open = silenceOpen;
      const found = open && Hub.findClip(open.clipId);
      if (found && open.strip)
        Hub.send("clipEdits", { edits: [{ op: "stripSilence", trackId: found.track.id, clipId: found.clip.id, silence: readSilenceWindow() }], label: "Strip Silence" });
      else if (found) setSilence(found.track, found.clip, readSilenceWindow());
      closeSilence();
    });
    $("silence-remove").addEventListener("click", () => {
      const found = silenceOpen && Hub.findClip(silenceOpen.clipId);
      if (found) setSilence(found.track, found.clip, null);
      closeSilence();
    });
    $("silence-dialog").addEventListener("keydown", (event) => {
      if (event.key === "Escape") { event.stopPropagation(); closeSilence(); }
      if (event.key === "Enter" && event.target.tagName !== "BUTTON") { event.preventDefault(); $("silence-apply").click(); }
    });
    $("audio-detect").addEventListener("click", () => { const t = current(); if (t) send("detectTempo", {}, t); });
    $("audio-sync").addEventListener("click", () => syncTempo(false, current()));
    $("audio-follow").addEventListener("click", () => {
      const t = current();
      if (!t) return;
      if (t.clip.followTempo) send("followTempo", { on: false }, t);
      else syncTempo(true, t);
    });
    $("audio-menu").addEventListener("click", (event) => audioMenu(event.currentTarget, current()));
    $("audio-busy-cancel").addEventListener("click", () => Hub.send("audioCancel"));

    $("audio-pitch").addEventListener("change", (event) => {
      const t = current(), value = Number(event.target.value);
      if (!t || !Number.isFinite(value)) { render(); return; }
      const semitones = Hub.clamp(Math.round(value * 100) / 100, -24, 24);
      if (Math.abs(semitones - (t.clip.pitch || 0)) > 1e-6) send("pitch", { semitones }, t);
    });

    $("audio-gain").addEventListener("change", (event) => {
      const t = current(), value = Number(event.target.value);
      if (!t || !Number.isFinite(value)) { render(); return; }
      send("gain", { db: Hub.clamp(value, -60, 12) }, t);
    });

    for (const id of ["audio-pitch", "audio-gain", "audio-tempo"]) {
      $(id).addEventListener("keydown", (event) => {
        if (event.key === "Enter") event.target.blur();
        if (event.key === "Escape") { event.target.value = ""; render(); event.target.blur(); }
      });
    }

    // Click the waveform to move the song position (snapped as the arrangement snaps).
    $("audio-wave").addEventListener("pointerdown", (event) => {
      if (event.button !== 0 || !current()) return;
      if (ae.stretcher) { stretchDown(event); return; }
      if (ae.markers && markDown(event)) return;
      if (ae.slicing && sliceDown(event)) return;
      const rect = wrap().getBoundingClientRect();
      const ppq = ppqAtX(event.clientX - rect.left);
      Hub.locate(Hub.arrange ? Hub.arrange.snap(ppq) : ppq);
    });

    $("audio-wave").addEventListener("pointermove", (event) => { if (ae.stretcher) stretchMove(event); else if (ae.markers) markMove(event); else if (ae.slicing) sliceMove(event); });
    $("audio-wave").addEventListener("pointerup", (event) => { stretchUp(event); markUp(event); sliceUp(event); });
    $("audio-wave").addEventListener("pointercancel", (event) => { stretchUp(event); markUp(event); sliceUp(event); });
    $("audio-wave").addEventListener("dblclick", (event) => {
      if (ae.markers) { event.preventDefault(); markDoubleClick(event); }
      else if (ae.slicing) { event.preventDefault(); sliceDoubleClick(event); }
    });
    $("audio-slice").addEventListener("click", () => setSlicing(!ae.slicing));
    $("audio-slice-cancel").addEventListener("click", () => setSlicing(false));
    $("audio-slice-go").addEventListener("click", sliceGo);
    for (const id of ["audio-slice-by", "audio-slice-sens", "audio-slice-grid"])
      $(id).addEventListener("change", () => { sliceCompute(); render(); });
    $("audio-stretcher").addEventListener("click", () => setStretcher(!ae.stretcher));
    $("audio-markers").addEventListener("click", () => setMarkers(!ae.markers));
    $("audio-snaphit").addEventListener("click", () => {
      ae.snapHit = !ae.snapHit;
      $("audio-snaphit").setAttribute("aria-pressed", String(ae.snapHit));
      if (ae.snapHit) askFine();
    });
    window.addEventListener("keydown", (event) => {
      if (event.key === "Escape" && ae.stretcher && ae.stretch && !Hub.isTyping(event.target)) { ae.stretch = null; ae.stretchDrag = null; render(); }
      else if (event.key === "Escape" && ae.slicing && panelOpen() && !Hub.isTyping(event.target)) setSlicing(false);
    });

    wrap().addEventListener("scroll", render);
    wrap().addEventListener("wheel", (event) => {
      if (!event.ctrlKey || !current()) return;
      event.preventDefault();
      const rect = wrap().getBoundingClientRect();
      const anchor = event.clientX - rect.left, ppq = ppqAtX(anchor);
      ae.zoom = Hub.clamp(ae.zoom * (event.deltaY < 0 ? 1.2 : 1 / 1.2), MIN_ZOOM, MAX_ZOOM);
      updateSize(current().clip);
      wrap().scrollLeft = Math.max(0, ppq * ae.zoom - anchor);
      render();
    }, { passive: false });

    // A right-click on the clip in the Audio Editor: the same Audio menu, for the clip shown.
    wrap().addEventListener("contextmenu", (event) => {
      const shown = current();
      if (!shown) return;
      event.preventDefault();

      // Markers on: a right-click on a marker takes it away (Slice: on a cut, the cut).
      if (ae.markers) {
        const mark = markAt(shown.clip, event.clientX - wrap().getBoundingClientRect().left);
        if (mark) { takeMark(shown, mark); return; }
      }
      if (ae.slicing) {
        const index = cutAt(shown.clip, event.clientX - wrap().getBoundingClientRect().left);
        if (index >= 0) { ae.slice.cuts.splice(index, 1); render(); return; }
      }
      if (!Hub.state.selectedClips.includes(shown.clip.id)) {
        Hub.state.selectedClips = [shown.clip.id];
        if (Hub.arrange) Hub.arrange.redraw();
      }
      audioMenu(Hub.pointAnchor(event), shown);
    });

    // An audio clip's right-click in the arrangement: the Audio menu.
    $("lanes").addEventListener("contextmenu", (event) => {
      if (!Hub.arrange || Hub.arrange.view.automation) return;
      const track = Hub.arrange.trackAt(event.clientY);
      const clip = track ? Hub.arrange.clipAt(track, Hub.arrange.ppqAt(event.clientX)) : null;
      if (!clip || !clip.audioFile) return;
      event.preventDefault();
      if (!Hub.state.selectedClips.includes(clip.id)) Hub.state.selectedClips = [clip.id];
      Hub.arrange.redraw();
      const at = { getBoundingClientRect: () => ({ left: event.clientX, right: event.clientX, top: event.clientY, bottom: event.clientY }) };
      audioMenu(at, { track, clip });
    });

    if (window.ResizeObserver) new ResizeObserver(() => render()).observe(wrap());
    requestAnimationFrame(function tick() { if (panelOpen()) placePlayhead(); requestAnimationFrame(tick); });
  });

  Hub.on("layout", () => requestAnimationFrame(render));
  Hub.on("theme", render);
  Hub.on("tempoMap", render);
  Hub.on("audioPeaks", render);
  Hub.on("audioPeaksFine", render);
  Hub.on("tracks", () => {
    if (!current() && ae.clipId) {
      ae.clipId = 0;
      ae.trackId = 0;
    }
    render();
  });
  document.addEventListener("pointerup", () => { if (panelOpen()) followSelection(); });
})();
