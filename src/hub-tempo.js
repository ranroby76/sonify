// D:\workspace\Fanan_web_hub\src\hub-tempo.js
//
// The song channel's map on the page, as the engine keeps it (TempoMap.cpp): tempo changes
// (each holds until the next, or ramps to it in a straight line), time signatures from bars,
// keys from bars, and the chord track (chords at song positions). It turns song positions (quarter notes) into seconds and back, finds
// the bars, writes and reads positions (bar.beat.sixteenth.ticks), and lays the grid and
// snapping on the bars as the signatures count them. Bars are counted from 0 here and shown
// from 1.
//
//   Hub.tempoMap        the song's map (a TempoMap), from the engine's "songChannel" event
//   Hub.songChannel     { mainAutomation: [[ppq, dB]], scales }
//   Hub.TempoMap        the class, for maps being edited

(() => {
  "use strict";

  const TINY = 1e-9;
  const MIN_TEMPO = 20, MAX_TEMPO = 250, MAX_BAR = 100000;
  const DENOMINATORS = [1, 2, 4, 8, 16, 32];
  const SCALES = ["major", "minor", "dorian", "phrygian", "lydian", "mixolydian", "locrian",
                  "harmonic minor", "melodic minor", "major pentatonic", "minor pentatonic", "blues", "chromatic"];
  const NOTE_NAMES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];
  // The chord track's qualities, as the engine knows them (TempoMap::getChordQualities), with their notes above the root.
  const CHORD_NOTES = { "": [0, 4, 7], m: [0, 3, 7], 7: [0, 4, 7, 10], maj7: [0, 4, 7, 11], m7: [0, 3, 7, 10], 6: [0, 4, 7, 9],
                        m6: [0, 3, 7, 9], 9: [0, 2, 4, 7, 10], maj9: [0, 2, 4, 7, 11], m9: [0, 2, 3, 7, 10], add9: [0, 2, 4, 7],
                        sus2: [0, 2, 7], sus4: [0, 5, 7], "7sus4": [0, 5, 7, 10], dim: [0, 3, 6], dim7: [0, 3, 6, 9],
                        m7b5: [0, 3, 6, 10], aug: [0, 4, 8], 5: [0, 7] };
  const CHORD_QUALITIES = ["", "m", "7", "maj7", "m7", "6", "m6", "9", "maj9", "m9", "add9", "sus2", "sus4", "7sus4", "dim", "dim7", "m7b5", "aug", "5"];
  // Other ways of writing them.
  const QUALITY_ALIASES = { maj: "", M: "", min: "m", "-": "m", mi: "m", dom7: "7", M7: "maj7", "\u03947": "maj7", "\u0394": "maj7", ma7: "maj7",
                            min7: "m7", "-7": "m7", mi7: "m7", "\u00b0": "dim", o: "dim", "\u00b07": "dim7", o7: "dim7", "\u00f8": "m7b5", "\u00f87": "m7b5",
                            "m7(b5)": "m7b5", "-7b5": "m7b5", "+": "aug", sus: "sus4", "2": "add9", add2: "add9", min6: "m6", min9: "m9", M9: "maj9", maj9: "maj9" };
  const LETTER = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };

  const finite = (value, fallback) => (Number.isFinite(Number(value)) ? Number(value) : fallback);
  const clamp = (value, min, max) => Math.min(max, Math.max(min, value));

  /** The last index whose key is at or before value (0 when none is). */
  function lastAtOrBefore(list, value, key) {
    let lo = 0, hi = list.length - 1, found = 0;
    while (lo <= hi) {
      const mid = (lo + hi) >> 1;
      if (key(list[mid]) <= value) { found = mid; lo = mid + 1; } else hi = mid - 1;
    }
    return found;
  }

  class TempoMap {
    /** data: the engine's form, {tempos: [[ppq, bpm, ramp]], signatures: [[bar, n, d]], keys: [[bar, root, scale]]}. */
    constructor(data) {
      const d = data || {};
      this.setTempos((Array.isArray(d.tempos) ? d.tempos : []).map((t) => ({ ppq: finite(t[0], -1), bpm: finite(t[1], 120), ramp: Boolean(t[2]) }))
                                                               .filter((t) => t.ppq >= 0));
      this.setSignatures((Array.isArray(d.signatures) ? d.signatures : []).map((s) => ({ bar: Math.trunc(finite(s[0], -1)), numerator: Math.trunc(finite(s[1], 4)),
                                                                                         denominator: Math.trunc(finite(s[2], 4)) }))
                                                                         .filter((s) => s.bar >= 0));
      this.setKeys((Array.isArray(d.keys) ? d.keys : []).map((k) => ({ bar: Math.trunc(finite(k[0], -1)), root: Math.trunc(finite(k[1], 0)), scale: String(k[2] || "major") }))
                                                         .filter((k) => k.bar >= 0));
      this.setChords((Array.isArray(d.chords) ? d.chords : []).filter((c) => Array.isArray(c) && c.length >= 3)
                       .map((c) => ({ ppq: finite(c[0], -1), root: Math.trunc(finite(c[1], 0)), quality: String(c[2] || ""), bass: Math.trunc(finite(c[3], -1)) })));
    }

    // ------------------------------------------------------------ lists, tidied as the engine tidies them
    setTempos(list) {
      const sorted = list.filter((t) => Number.isFinite(t.ppq) && Number.isFinite(t.bpm))
                         .map((t) => ({ ppq: Math.max(0, t.ppq), bpm: clamp(t.bpm, MIN_TEMPO, MAX_TEMPO), ramp: Boolean(t.ramp) }))
                         .sort((a, b) => a.ppq - b.ppq);
      const tidy = [];
      for (const t of sorted) {
        if (tidy.length && Math.abs(tidy[tidy.length - 1].ppq - t.ppq) < TINY) tidy[tidy.length - 1] = t;
        else tidy.push(t);
      }
      if (!tidy.length) tidy.push({ ppq: 0, bpm: 120, ramp: false });
      tidy[0].ppq = 0;
      tidy[tidy.length - 1].ramp = false;
      this.tempos = tidy;
      this.rebuild();
    }

    setSignatures(list) {
      const sorted = list.filter((s) => DENOMINATORS.includes(s.denominator))
                         .map((s) => ({ bar: clamp(s.bar, 0, MAX_BAR), numerator: clamp(s.numerator, 1, 32), denominator: s.denominator }))
                         .sort((a, b) => a.bar - b.bar);
      const tidy = [];
      for (const s of sorted) {
        if (tidy.length && tidy[tidy.length - 1].bar === s.bar) tidy[tidy.length - 1] = s;
        else tidy.push(s);
      }
      if (!tidy.length) tidy.push({ bar: 0, numerator: 4, denominator: 4 });
      tidy[0].bar = 0;
      this.signatures = tidy;
      this.rebuild();
    }

    setKeys(list) {
      const sorted = list.map((k) => ({ bar: clamp(k.bar, 0, MAX_BAR), root: ((k.root % 12) + 12) % 12, scale: SCALES.includes(k.scale) ? k.scale : "major" }))
                         .sort((a, b) => a.bar - b.bar);
      const tidy = [];
      for (const k of sorted) {
        if (tidy.length && tidy[tidy.length - 1].bar === k.bar) tidy[tidy.length - 1] = k;
        else tidy.push(k);
      }
      if (!tidy.length) tidy.push({ bar: 0, root: 0, scale: "major" });
      tidy[0].bar = 0;
      this.keys = tidy;
    }

    /** The chord track: in order, one to a position, as the engine tidies it (none is fine). */
    setChords(list) {
      const sorted = list.filter((c) => Number.isFinite(c.ppq) && c.ppq >= 0)
                         .map((c) => {
                           const root = ((Math.trunc(c.root) % 12) + 12) % 12;
                           const bass = Number.isFinite(c.bass) && c.bass >= 0 ? Math.trunc(c.bass) % 12 : -1;
                           return { ppq: c.ppq, root, quality: CHORD_QUALITIES.includes(c.quality) ? c.quality : "", bass: bass === root ? -1 : bass };
                         })
                         .sort((a, b) => a.ppq - b.ppq);
      const tidy = [];
      for (const c of sorted) {
        if (tidy.length && Math.abs(tidy[tidy.length - 1].ppq - c.ppq) < TINY) tidy[tidy.length - 1] = c;
        else tidy.push(c);
      }
      this.chords = tidy;
    }

    /** The chord that holds at a position (null: none before it). */
    chordAt(ppq) {
      if (!this.chords.length || ppq + TINY < this.chords[0].ppq) return null;
      return this.chords[lastAtOrBefore(this.chords, ppq + TINY, (c) => c.ppq)];
    }

    rebuild() {
      if (!this.tempos || !this.signatures) return;
      this.tempoSeconds = [0];
      for (let i = 1; i < this.tempos.length; i++)
        this.tempoSeconds.push(this.tempoSeconds[i - 1] + this.secondsIn(i - 1, this.tempos[i].ppq));
      this.signaturePpq = [0];
      for (let i = 1; i < this.signatures.length; i++) {
        const before = this.signatures[i - 1];
        this.signaturePpq.push(this.signaturePpq[i - 1] + (this.signatures[i].bar - before.bar) * before.numerator * 4 / before.denominator);
      }
    }

    toData() {
      const data = { tempos: this.tempos.map((t) => [t.ppq, t.bpm, t.ramp]),
                     signatures: this.signatures.map((s) => [s.bar, s.numerator, s.denominator]),
                     keys: this.keys.map((k) => [k.bar, k.root, k.scale]) };
      if (this.chords && this.chords.length) data.chords = this.chords.map((c) => [c.ppq, c.root, c.quality, c.bass]);
      return data;
    }

    clone() { return new TempoMap(this.toData()); }

    equals(other) { return Boolean(other) && JSON.stringify(this.toData()) === JSON.stringify(other.toData()); }

    hasOneTempo() { return this.tempos.length === 1; }

    // ------------------------------------------------------------ time
    slopeOf(index) {
      const from = this.tempos[index], to = this.tempos[index + 1];
      if (!from.ramp || !to) return 0;
      const span = to.ppq - from.ppq;
      return span > TINY ? (to.bpm - from.bpm) / span : 0;
    }

    secondsIn(index, ppq) {
      const from = this.tempos[index], distance = ppq - from.ppq, slope = this.slopeOf(index);
      if (Math.abs(slope) > 1e-12) return 60 / slope * Math.log((from.bpm + slope * distance) / from.bpm);
      return distance * 60 / from.bpm;
    }

    secondsAt(ppq) {
      if (!Number.isFinite(ppq) || ppq <= 0) return 0;
      const index = lastAtOrBefore(this.tempos, ppq, (t) => t.ppq);
      return this.tempoSeconds[index] + this.secondsIn(index, ppq);
    }

    ppqAt(seconds) {
      if (!Number.isFinite(seconds) || seconds <= 0) return 0;
      const index = lastAtOrBefore(this.tempoSeconds, seconds, (s) => s);
      const from = this.tempos[index], into = seconds - this.tempoSeconds[index], slope = this.slopeOf(index);
      if (Math.abs(slope) > 1e-12) return from.ppq + from.bpm / slope * (Math.exp(slope * into / 60) - 1);
      return from.ppq + into * from.bpm / 60;
    }

    bpmAt(ppq) {
      ppq = Number.isFinite(ppq) ? Math.max(0, ppq) : 0;
      const index = lastAtOrBefore(this.tempos, ppq, (t) => t.ppq);
      const from = this.tempos[index], to = this.tempos[index + 1];
      if (from.ramp && to && to.ppq - from.ppq > TINY) return from.bpm + (to.bpm - from.bpm) * clamp((ppq - from.ppq) / (to.ppq - from.ppq), 0, 1);
      return from.bpm;
    }

    /** The index of the tempo change in effect at ppq. */
    tempoIndexAt(ppq) { return lastAtOrBefore(this.tempos, Math.max(0, ppq) + TINY, (t) => t.ppq); }

    // ------------------------------------------------------------ bars
    /** The bar a position is in: {index, start, numerator, denominator, beat, length, end}. */
    barAt(ppq) {
      ppq = Number.isFinite(ppq) ? Math.max(0, ppq) : 0;
      const i = lastAtOrBefore(this.signaturePpq, ppq + TINY, (p) => p);
      return this.makeBar(i, Math.max(0, Math.floor((ppq - this.signaturePpq[i]) / this.lengthOf(i) + TINY)));
    }

    lengthOf(i) { const s = this.signatures[i]; return s.numerator * 4 / s.denominator; }

    makeBar(i, barsIn) {
      const s = this.signatures[i], length = this.lengthOf(i);
      const index = Math.min(MAX_BAR, s.bar + barsIn);
      const start = this.signaturePpq[i] + (index - s.bar) * length;
      return { index, start, numerator: s.numerator, denominator: s.denominator, beat: 4 / s.denominator, length, end: start + length };
    }

    /** The bar with this index (from 0). */
    bar(index) {
      index = clamp(Math.trunc(index), 0, MAX_BAR);
      const i = lastAtOrBefore(this.signatures, index, (s) => s.bar);
      return this.makeBar(i, index - this.signatures[i].bar);
    }

    ppqOfBar(index) { return this.bar(index).start; }

    nearestBar(ppq) {
      const bar = this.barAt(ppq);
      return ppq - bar.start < bar.length / 2 ? bar.start : bar.end;
    }

    signatureAt(ppq) { const b = this.barAt(ppq); return { bar: b.index, numerator: b.numerator, denominator: b.denominator }; }

    keyAt(ppq) {
      const bar = this.barAt(ppq).index;
      return this.keys[lastAtOrBefore(this.keys, bar, (k) => k.bar)];
    }

    /** Each bar overlapping [from, to), in order. */
    *barsBetween(from, to) {
      let bar = this.barAt(from);
      for (let n = 0; n < 20000 && bar.start < to; n++) {
        yield bar;
        const next = this.barAt(bar.end);
        if (next.start < bar.end - 1e-6) break;   // past the last bar the map counts
        bar = next;
      }
    }

    // ------------------------------------------------------------ positions as text
    /** Studio One's bars display: bar.beat.sixteenth.ticks, as in 00001.01.01.00 */
    formatBars(ppq) {
      const bar = this.barAt(Math.max(0, ppq) + 1e-9);
      const inBar = Math.max(0, ppq + 1e-9 - bar.start);
      const beat = Math.min(bar.numerator - 1, Math.floor(inBar / bar.beat));
      const perBeat = Math.max(1, 16 / bar.denominator);
      const sixteenthLength = bar.beat / perBeat;
      const inBeat = inBar - beat * bar.beat;
      const sixteenth = Math.min(perBeat - 1, Math.floor(inBeat / sixteenthLength));
      const ticks = Math.min(99, Math.floor((inBeat - sixteenth * sixteenthLength) / sixteenthLength * 100));
      const pad = (value, width) => String(value).padStart(width, "0");
      return `${pad(bar.index + 1, 5)}.${pad(beat + 1, 2)}.${pad(sixteenth + 1, 2)}.${pad(ticks, 2)}`;
    }

    /** Reads "5", "5.3", "5.3.2", "5.3.2.50" or "00005.03.02.50": quarter notes, or null. */
    parseBars(text) {
      const parts = String(text).trim().split(/[.:\s]+/).filter(Boolean);
      if (parts.length === 0 || parts.length > 4 || parts.some((part) => !/^\d+$/.test(part))) return null;
      const [barNumber, beat = 1, sixteenth = 1, ticks = 0] = parts.map(Number);
      if (barNumber < 1 || barNumber > MAX_BAR + 1) return null;
      const bar = this.bar(barNumber - 1);
      const perBeat = Math.max(1, 16 / bar.denominator);
      if (beat < 1 || beat > bar.numerator || sixteenth < 1 || sixteenth > perBeat || ticks > 99) return null;
      const sixteenthLength = bar.beat / perBeat;
      return bar.start + (beat - 1) * bar.beat + (sixteenth - 1) * sixteenthLength + ticks / 100 * sixteenthLength;
    }

    /** "5", "5.2" or "5.2.3" for a ruler label, as fine as the step needs. */
    label(ppq, step) {
      const bar = this.barAt(ppq + 1e-9);
      const inBar = ppq - bar.start;
      if (step >= bar.length - 1e-9 || inBar < 1e-6) return String(bar.index + 1);
      const beatIndex = Math.floor(inBar / bar.beat + 1e-9);
      const inBeat = inBar - beatIndex * bar.beat;
      if (step >= bar.beat - 1e-9 || inBeat < 1e-6) return `${bar.index + 1}.${beatIndex + 1}`;
      return `${bar.index + 1}.${beatIndex + 1}.${Math.floor(inBeat / 0.25 + 1e-9) + 1}`;
    }

    // ------------------------------------------------------------ the grid
    /**
        Grid steps from a 64th note up to many bars, for the bar at ref: the finest divide its
        beat; then the beat, the bar, and 2, 4, 8 ... bars. A step of a bar or more counts bars
        (whatever their lengths); finer steps count from each bar's start.
    */
    steps(ref) {
      const bar = this.barAt(ref);
      const list = [0.0625, 0.125, 0.25, 0.5, 1, 2].filter((s) => s < bar.beat && Math.abs(bar.beat / s - Math.round(bar.beat / s)) < 1e-9)
                                             .map((q) => ({ q, bars: 0 }));
      list.push({ q: bar.beat, bars: 0, beat: true });
      if (bar.length > bar.beat) list.push({ q: bar.length, bars: 1 });
      for (let m = 2; m <= 1024; m *= 2) list.push({ q: bar.length * m, bars: m });
      return list;
    }

    /** The finest step (no finer than finest quarters) at least minPx wide at zoom px per quarter. */
    gridStep(ref, zoom, minPx, finest = 0) {
      const all = this.steps(ref);
      return all.find((s) => s.q >= finest - 1e-9 && s.q * zoom >= minPx) || all[all.length - 1];
    }

    /** A step of q quarters: bars when q is at least the bar at ref, else q within bars. */
    stepOf(q, ref) {
      const bar = this.barAt(ref);
      if (q >= bar.length - 1e-9) return { q, bars: Math.max(1, Math.round(q / bar.length)) };
      return { q, bars: 0, beat: Math.abs(q - bar.beat) < 1e-9 };
    }

    /**
        The grid's lines from..to for a step: {ppq, bar (on a bar line), beat (on a beat)}.
        Bars at each bar start (every step.bars-th); finer steps from each bar's start, the
        beat step following each bar's own beat.
    */
    lines(from, to, step) {
      const out = [];
      for (const bar of this.barsBetween(Math.max(0, from), to)) {
        if (step.bars > 0) {
          if (bar.index % step.bars === 0 && bar.start >= from - 1e-9) out.push({ ppq: bar.start, bar: true, beat: true, index: bar.index });
          continue;
        }
        const q = step.beat ? bar.beat : step.q;
        const count = Math.ceil(bar.length / q - 1e-9);
        for (let k = 0; k < count; k++) {
          const ppq = bar.start + k * q;
          if (ppq < from - 1e-9) continue;
          if (ppq >= to) break;
          const inBar = k * q;
          out.push({ ppq, bar: k === 0, beat: Math.abs(inBar / bar.beat - Math.round(inBar / bar.beat)) < 1e-6, index: bar.index });
        }
        if (out.length > 20000) break;
      }
      return out;
    }

    /** The nearest grid position to ppq for a step (null step: ppq itself). */
    snap(ppq, step) {
      ppq = Math.max(0, ppq);
      if (!step) return ppq;
      const bar = this.barAt(ppq);
      if (step.bars > 0) {
        const lower = this.bar(Math.floor(bar.index / step.bars) * step.bars);
        const upper = this.bar(lower.index + step.bars);
        return ppq - lower.start <= upper.start - ppq ? lower.start : upper.start;
      }
      const q = step.beat ? bar.beat : step.q;
      const at = Math.min(bar.end, bar.start + Math.round((ppq - bar.start) / q) * q);
      return bar.end - ppq < Math.abs(ppq - at) ? bar.end : at;   // a step that doesn't fill the bar: its end may be nearer
    }

    /** The grid position at or before ppq. */
    snapFloor(ppq, step) {
      ppq = Math.max(0, ppq);
      if (!step) return ppq;
      const bar = this.barAt(ppq);
      if (step.bars > 0) return this.bar(Math.floor(bar.index / step.bars) * step.bars).start;
      const q = step.beat ? bar.beat : step.q;
      return bar.start + Math.floor((ppq - bar.start) / q + 1e-9) * q;
    }

    // ------------------------------------------------------------ keys
    static keyName(key) { return `${NOTE_NAMES[key.root] || "C"} ${key.scale}`; }

    // ------------------------------------------------------------ chords
    /** "Am7", "C/E". */
    static chordName(chord) {
      if (!chord) return "";
      const bass = chord.bass >= 0 && chord.bass !== chord.root ? `/${NOTE_NAMES[chord.bass]}` : "";
      return `${NOTE_NAMES[chord.root] || "C"}${chord.quality || ""}${bass}`;
    }

    /** A chord as typed ("Am7", "F#m7b5", "C/E", "Bbmaj7", "Dsus") as {root, quality, bass}; null when it isn't one. */
    static parseChord(text) {
      const match = /^\s*([A-Ga-g])([#b\u266f\u266d]?)(.*?)(?:\/([A-Ga-g])([#b\u266f\u266d]?))?\s*$/.exec(String(text || ""));
      if (!match) return null;
      const note = (letter, accidental) => (LETTER[letter.toUpperCase()] + (accidental === "#" || accidental === "\u266f" ? 1 : accidental ? -1 : 0) + 12) % 12;
      const typed = match[3].trim();
      const quality = CHORD_QUALITIES.includes(typed) ? typed : Object.prototype.hasOwnProperty.call(QUALITY_ALIASES, typed) ? QUALITY_ALIASES[typed]
                    : CHORD_QUALITIES.find((q) => q.toLowerCase() === typed.toLowerCase() && q !== "");
      if (quality === undefined) return null;
      const root = note(match[1], match[2]);
      const bass = match[4] ? note(match[4], match[5]) : -1;
      return { root, quality, bass: bass === root ? -1 : bass };
    }

    /** A chord's pitch classes (the bass first when it has one of its own). */
    static chordPitches(chord) {
      const tones = (CHORD_NOTES[chord.quality] || CHORD_NOTES[""]).map((i) => (chord.root + i) % 12);
      return chord.bass >= 0 && !tones.includes(chord.bass) ? [chord.bass, ...tones] : tones;
    }
  }

  TempoMap.MIN_TEMPO = MIN_TEMPO;
  TempoMap.MAX_TEMPO = MAX_TEMPO;
  TempoMap.MAX_BAR = MAX_BAR;
  TempoMap.DENOMINATORS = DENOMINATORS;
  TempoMap.SCALES = SCALES;
  TempoMap.NOTE_NAMES = NOTE_NAMES;
  TempoMap.CHORD_QUALITIES = CHORD_QUALITIES;
  TempoMap.CHORD_NOTES = CHORD_NOTES;

  Hub.TempoMap = TempoMap;
  Hub.tempoMap = new TempoMap();
  Hub.songChannel = { mainAutomation: [], scales: SCALES };

  // The engine sends the song channel when it changes (a song opened, an edit, an undo, the AI).
  Hub.on("songChannel", (data) => {
    const map = new TempoMap(data && data.map);
    const main = Array.isArray(data && data.mainAutomation) ? data.mainAutomation.filter((p) => Array.isArray(p) && p.length === 2) : [];
    const changed = !map.equals(Hub.tempoMap) || JSON.stringify(main) !== JSON.stringify(Hub.songChannel.mainAutomation);
    Hub.tempoMap = map;
    Hub.songChannel = { mainAutomation: main, scales: Array.isArray(data && data.scales) ? data.scales : SCALES };
    if (changed) Hub.emit("tempoMap", map);
  });
})();
