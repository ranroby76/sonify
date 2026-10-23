// D:\workspace\Fanan_web_hub\src\hub-score.js
//
// The Score: an instrument track's notes shown as sheet music, as Studio One's Score view shows
// them, to read and print, not to write (the notes are edited in the Editor). The whole track:
// every clip on it, on the song's bars, from the bar of its first note to the bar of its last.
//
//   Opened from a track header's right-click menu (Show as Score...), a MIDI clip's (Show Track
//   as Score...) or View > Score (the selected track).
//   Notation   Auto (drums for a drum track, else one or two staves as the notes need), Piano
//              (two staves, split at a note), Treble clef, Bass clef or Drums (a drum staff:
//              General MIDI drums in their usual places, cymbals as x, the feet stemmed down).
//   Quantize   the grid the notes are read on (1/4 to 1/32): starts and ends land on it.
//   Export     PDF (A4 pages) and MusicXML (for MuseScore, Sibelius, Finale or Dorico); Sonify
//              asks where to save.
//
// How notes become notation: each staff has up to two voices. Notes starting and ending
// together are one chord; a chord that overlaps the voice above goes to the voice below (a
// third layer shortens the note before it). Notes and rests are split into values that show
// the beat, tied across the beat and the bar line; the key from the Song Channel spells the
// accidentals; the time signatures and the first tempo come from it too. Muted notes and
// clips are left out, as they don't play. Drums: each hit lasts until the next in its voice,
// at most to the end of its beat. The chord track's chords (Master Song Channel > Chords) show
// as chord symbols over the top staff (not on drums).
//
// Drawn by OpenSheetMusicDisplay (BSD licence) from the MusicXML made here; PDF pages by jsPDF
// and svg2pdf.js (MIT). The three load the first time the Score opens.

(() => {
  "use strict";

  const { $, element } = Hub;
  const DIV = 16;                  // MusicXML divisions: ticks per quarter note
  const MAX_MEASURES = 600;
  const LIBRARIES = ["opensheetmusicdisplay.min.js", "jspdf.umd.min.js", "svg2pdf.umd.min.js"];
  const NATURAL = [0, 2, 4, 5, 7, 9, 11];
  const LETTERS = ["C", "D", "E", "F", "G", "A", "B"];
  const MAJOR_FIFTHS = [0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5];          // by the tonic's pitch class
  // How far each scale's parent major key is from its root, and the MusicXML mode it shows as.
  const MODES = { major: [0, "major"], minor: [3, "minor"], dorian: [10, "dorian"], phrygian: [8, "phrygian"], lydian: [7, "lydian"],
                  mixolydian: [5, "mixolydian"], locrian: [1, "locrian"], "harmonic minor": [3, "minor"], "melodic minor": [3, "minor"],
                  "major pentatonic": [0, "major"], "minor pentatonic": [3, "minor"], blues: [3, "minor"] };
  // Note values in ticks: [ticks, type, dots], longest first.
  const VALUES = [[96, "whole", 1], [64, "whole", 0], [48, "half", 1], [32, "half", 0], [24, "quarter", 1], [16, "quarter", 0],
                  [12, "eighth", 1], [8, "eighth", 0], [6, "16th", 1], [4, "16th", 0], [3, "32nd", 1], [2, "32nd", 0], [1, "64th", 0]];
  // General MIDI drums on a drum staff: [display step, octave, notehead].
  const DRUMS = {
    35: ["F", 4, "normal"], 36: ["F", 4, "normal"], 37: ["C", 5, "x"], 38: ["C", 5, "normal"], 39: ["C", 5, "x"], 40: ["C", 5, "normal"],
    41: ["G", 4, "normal"], 43: ["A", 4, "normal"], 45: ["B", 4, "normal"], 47: ["D", 5, "normal"], 48: ["D", 5, "normal"], 50: ["E", 5, "normal"],
    42: ["G", 5, "x"], 44: ["D", 4, "x"], 46: ["G", 5, "circle-x"], 49: ["A", 5, "x"], 57: ["A", 5, "x"], 55: ["A", 5, "x"], 52: ["B", 5, "x"],
    51: ["F", 5, "x"], 59: ["F", 5, "x"], 53: ["F", 5, "diamond"], 54: ["B", 5, "x"], 56: ["E", 5, "triangle"]
  };
  const FEET = new Set([35, 36, 44]);

  const options = { trackId: 0, staff: "auto", split: 60, grid: 0.25, zoom: 1 };
  let osmd = null, loading = null, shownXml = "", renderTimer = 0;

  // ---------------------------------------------------------------- the notes
  const isDrumTrack = (track) => /drum|kit|perc|beat|cajon|808|909/i.test(`${track.name} ${(track.instrument && track.instrument.name) || ""}`);

  /** Every note the track plays, on the song's timeline (quarter notes): muted ones left out. */
  function trackNotes(track) {
    const notes = [];
    for (const clip of track.clips || []) {
      if (clip.audioFile || clip.muted) continue;
      const offset = clip.offset || 0, clipEnd = clip.start + clip.length;
      for (const [pitch, start, length, , muted] of clip.notes || []) {
        if (muted || start < offset || start >= offset + clip.length) continue;
        const songStart = clip.start + start - offset;
        notes.push({ pitch, start: songStart, end: Math.min(songStart + Math.max(length, 1 / 64), clipEnd) });
      }
    }
    return notes.sort((a, b) => a.start - b.start || b.pitch - a.pitch);
  }

  /** Notes on the grid, in ticks; the same note twice at once is kept once. */
  function quantize(notes, grid) {
    const g = Math.max(1, Math.round(grid * DIV));
    const seen = new Set(), out = [];
    for (const n of notes) {
      const start = Math.round(n.start * DIV / g) * g;
      let end = Math.round(n.end * DIV / g) * g;
      if (end <= start) end = start + g;
      const key = `${n.pitch}|${start}`;
      if (seen.has(key)) continue;
      seen.add(key);
      out.push({ pitch: n.pitch, start, end });
    }
    return out;
  }

  // ---------------------------------------------------------------- keys and spelling
  function keyOf(ppq) {
    const key = Hub.tempoMap.keyAt(ppq) || { root: 0, scale: "major" };
    const [offset, mode] = MODES[key.scale] || [0, "major"];
    return { fifths: MAJOR_FIFTHS[(key.root + offset) % 12], mode };
  }

  const spellings = new Map();

  /** Each pitch class's name in a key: the scale's own notes, then sharps (flats in flat keys). */
  function spelling(fifths) {
    if (spellings.has(fifths)) return spellings.get(fifths);
    const map = new Array(12);
    const tonicLetter = ((fifths * 4) % 7 + 7) % 7, tonicPc = ((fifths * 7) % 12 + 12) % 12;
    for (let i = 0; i < 7; i++) {
      const letter = (tonicLetter + i) % 7, pc = (tonicPc + NATURAL[i]) % 12;
      map[pc] = { step: LETTERS[letter], alter: ((pc - NATURAL[letter] + 18) % 12) - 6 };
    }
    for (let pc = 0; pc < 12; pc++) {
      if (map[pc]) continue;
      const natural = NATURAL.indexOf(pc);
      // C major: C#, Eb, F#, G#, Bb, as they're usually written.
      const flat = fifths < 0 || (fifths === 0 && (pc === 3 || pc === 10));
      if (natural >= 0) map[pc] = { step: LETTERS[natural], alter: 0 };
      else if (flat) map[pc] = { step: LETTERS[NATURAL.indexOf((pc + 1) % 12)], alter: -1 };
      else map[pc] = { step: LETTERS[NATURAL.indexOf((pc + 11) % 12)], alter: 1 };
    }
    spellings.set(fifths, map);
    return map;
  }

  function pitchXml(midi, fifths) {
    const { step, alter } = spelling(fifths)[midi % 12];
    const octave = Math.floor((midi - alter) / 12) - 1;
    return `<pitch><step>${step}</step>${alter ? `<alter>${alter}</alter>` : ""}<octave>${octave}</octave></pitch>`;
  }

  // ---------------------------------------------------------------- voices
  /** Chords of a staff in up to two voices, each a list of {start, end, notes} that don't overlap. */
  function voicesOf(notes) {
    const groups = new Map();
    for (const n of notes) {
      const key = `${n.start}|${n.end}`;
      if (!groups.has(key)) groups.set(key, { start: n.start, end: n.end, notes: [] });
      groups.get(key).notes.push(n);
    }
    const chords = [...groups.values()].sort((a, b) => a.start - b.start || Math.max(...b.notes.map((n) => n.pitch)) - Math.max(...a.notes.map((n) => n.pitch)));
    const voices = [[], []];
    const lastEnd = (v) => (v.length ? v[v.length - 1].end : -Infinity);

    const middle = (c) => c.notes.reduce((sum, n) => sum + n.pitch, 0) / c.notes.length;

    for (const chord of chords) {
      let v = voices.findIndex((voice) => lastEnd(voice) <= chord.start);
      if (v < 0) {
        // A third layer: the voice nearest it in pitch (an inner line, so the top and the bottom
        // stay whole) gives way where it starts.
        const near = voices.map((voice) => Math.abs(middle(voice[voice.length - 1]) - middle(chord)));
        v = near[0] < near[1] ? 0 : 1;
        const before = voices[v][voices[v].length - 1];
        if (before.start >= chord.start) {
          // Starting together: one chord, as long as the shorter.
          before.notes.push(...chord.notes);
          before.end = Math.min(before.end, chord.end);
          continue;
        }
        before.end = chord.start;
      }
      voices[v].push(chord);
    }
    return voices.filter((voice) => voice.length);
  }

  /** Drum hits in two voices (hands up, feet down), each lasting until the next, at most to the
      end of its beat (drums are never tied). */
  function drumVoices(notes, measureAt) {
    const voices = [[], []];
    for (const n of notes) {
      const v = FEET.has(n.pitch) ? 1 : 0;
      const voice = voices[v];
      const last = voice[voice.length - 1];
      if (last && last.start === n.start) {
        if (!last.notes.some((m) => (DRUMS[m.pitch] || DRUMS[38])[0] === (DRUMS[n.pitch] || DRUMS[38])[0]
                                     && (DRUMS[m.pitch] || DRUMS[38])[1] === (DRUMS[n.pitch] || DRUMS[38])[1])) last.notes.push(n);
        continue;
      }
      voice.push({ start: n.start, end: n.start, notes: [n] });
    }
    for (const voice of voices) {
      voice.forEach((chord, i) => {
        const measure = measureAt(chord.start);
        const next = i + 1 < voice.length ? voice[i + 1].start : Infinity;
        const beatEnd = measure.start + (Math.floor((chord.start - measure.start) / measure.beat) + 1) * measure.beat;
        chord.end = Math.max(chord.start + 1, Math.min(next, measure.end, beatEnd));
      });
    }
    return voices;
  }

  // ---------------------------------------------------------------- note values
  /** A stretch of a measure (from pos, ticks long) as note values that show the beat. */
  function values(pos, length, measure) {
    const out = [];
    let left = length;
    while (left > 0) {
      const fit = VALUES.find(([ticks, , dots]) => {
        if (ticks > left || ticks > measure.length) return false;
        const base = dots ? ticks / 3 * 2 : ticks;
        if (pos % base !== 0) return false;
        // In 4/4 a note doesn't hide the middle of the bar unless it starts the bar.
        if (measure.length === 64 && measure.numerator === 4 && pos !== 0 && pos < 32 && pos + ticks > 32) return false;
        // A note that starts off the beat ends by the next beat (it's tied on from there), so the
        // beats stay where the eye looks for them.
        if (pos % measure.beat !== 0 && Math.floor(pos / measure.beat) !== Math.floor((pos + ticks - 1) / measure.beat)) return false;
        return true;
      }) || [1, "64th", 0];
      out.push({ ticks: fit[0], type: fit[1], dots: fit[2] });
      pos += fit[0];
      left -= fit[0];
    }
    return out;
  }

  // ---------------------------------------------------------------- MusicXML
  const escape = (text) => String(text).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");

  const BEAMS = { eighth: 1, "16th": 2, "32nd": 3, "64th": 4 };

  /** One voice through one measure: its chords (tied over the bar line and between values) and
      rests, as {pos, ticks, type, dots, rest, notes, stop, start}; null when it has no chords there. */
  function voiceEvents(chords, measure) {
    const inside = chords.filter((c) => c.end > measure.start && c.start < measure.end);
    if (!inside.length) return null;
    const events = [];
    const rests = (from, to) => {
      if (from === measure.start && to === measure.end) { events.push({ pos: from, ticks: measure.length, whole: true, rest: true }); return; }
      let pos = from;
      for (const v of values(from - measure.start, to - from, measure)) { events.push({ pos, ...v, rest: true }); pos += v.ticks; }
    };
    let at = measure.start;
    for (const chord of inside) {
      const from = Math.max(chord.start, measure.start), to = Math.min(chord.end, measure.end);
      if (from > at) rests(at, from);
      let pos = from;
      for (const v of values(from - measure.start, to - from, measure)) {
        events.push({ pos, ...v, rest: false, notes: chord.notes, stop: pos > chord.start, start: pos + v.ticks < chord.end });
        pos += v.ticks;
      }
      at = to;
    }
    if (at < measure.end) rests(at, measure.end);
    return events;
  }

  /** Beams, as they'd be engraved: notes shorter than a quarter, one after another within a beat,
      joined; the shorter ones get their second (third...) beam, or a hook when alone. */
  function addBeams(events, measure) {
    let group = [];
    const flush = () => {
      if (group.length > 1) {
        const levels = group.map((e) => BEAMS[e.type] || 0);
        const most = Math.max(...levels);
        group.forEach((e) => { e.beams = []; });
        for (let level = 1; level <= most; level++) {
          group.forEach((e, i) => {
            if (levels[i] < level) return;
            const before = i > 0 && levels[i - 1] >= level, after = i < group.length - 1 && levels[i + 1] >= level;
            const kind = before && after ? "continue" : after ? "begin" : before ? "end" : i < group.length - 1 ? "forward hook" : "backward hook";
            e.beams.push(`<beam number="${level}">${kind}</beam>`);
          });
        }
      }
      group = [];
    };
    let beat = -1;
    for (const e of events) {
      const here = Math.floor((e.pos - measure.start) / measure.beat);
      if (e.rest || !BEAMS[e.type] || here !== beat) flush();
      beat = here;
      if (!e.rest && BEAMS[e.type]) group.push(e);
    }
    flush();
  }

  // ---------------------------------------------------------------- chord symbols
  const SHARP_SPELLING = [["C", 0], ["C", 1], ["D", 0], ["D", 1], ["E", 0], ["F", 0], ["F", 1], ["G", 0], ["G", 1], ["A", 0], ["A", 1], ["B", 0]];
  const FLAT_SPELLING = [["C", 0], ["D", -1], ["D", 0], ["E", -1], ["E", 0], ["F", 0], ["G", -1], ["G", 0], ["A", -1], ["A", 0], ["B", -1], ["B", 0]];
  // A chord quality as MusicXML's kind, and the degree it adds where the kind alone doesn't say it.
  const KINDS = { "": ["major"], m: ["minor"], 7: ["dominant"], maj7: ["major-seventh"], m7: ["minor-seventh"], 6: ["major-sixth"],
                  m6: ["minor-sixth"], 9: ["dominant-ninth"], maj9: ["major-ninth"], m9: ["minor-ninth"], add9: ["major", 9, 0],
                  sus2: ["suspended-second"], sus4: ["suspended-fourth"], "7sus4": ["suspended-fourth", 7, -1], dim: ["diminished"],
                  dim7: ["diminished-seventh"], m7b5: ["half-diminished"], aug: ["augmented"], 5: ["power"] };

  /** A chord as a MusicXML harmony, spelled with sharps or flats as the key has them. */
  function harmonyXml(chord, fifths) {
    const spell = (pc) => {
      const [step, alter] = (fifths < 0 ? FLAT_SPELLING : SHARP_SPELLING)[((pc % 12) + 12) % 12];
      return { step, alter };
    };
    const root = spell(chord.root);
    const [kind, degree, alter] = KINDS[chord.quality] || KINDS[""];
    let xml = `<harmony><root><root-step>${root.step}</root-step>${root.alter ? `<root-alter>${root.alter}</root-alter>` : ""}</root>`
            + `<kind text="${escape(chord.quality)}">${kind}</kind>`;
    if (chord.bass >= 0 && chord.bass !== chord.root) {
      const bass = spell(chord.bass);
      xml += `<bass><bass-step>${bass.step}</bass-step>${bass.alter ? `<bass-alter>${bass.alter}</bass-alter>` : ""}</bass>`;
    }
    if (degree) xml += `<degree><degree-value>${degree}</degree-value><degree-alter>${alter}</degree-alter><degree-type>add</degree-type></degree>`;
    return xml + "</harmony>";
  }

  /** One voice through one measure as MusicXML notes (null: nothing of it there). harmonies(from,
      to): the chord symbols for a stretch of ticks, written before the note or rest there. */
  function voiceXml(chords, measure, staff, voice, stem, drums, fifths, harmonies) {
    const events = voiceEvents(chords, measure);
    if (!events) return null;
    addBeams(events, measure);
    let xml = "";
    for (const e of events) {
      if (harmonies) xml += harmonies(e.pos, e.pos + e.ticks);
      if (e.rest) {
        xml += e.whole ? `<note><rest measure="yes"/><duration>${e.ticks}</duration><voice>${voice}</voice><staff>${staff}</staff></note>`
                       : `<note><rest/><duration>${e.ticks}</duration><voice>${voice}</voice><type>${e.type}</type>${"<dot/>".repeat(e.dots)}<staff>${staff}</staff></note>`;
        continue;
      }
      const ties = (e.stop ? '<tie type="stop"/>' : "") + (e.start ? '<tie type="start"/>' : "");
      const tied = e.stop || e.start ? `<notations>${e.stop ? '<tied type="stop"/>' : ""}${e.start ? '<tied type="start"/>' : ""}</notations>` : "";
      e.notes.slice().sort((a, b) => a.pitch - b.pitch).forEach((n, i) => {
        let what, head = "";
        if (drums) {
          const [step, octave, notehead] = DRUMS[n.pitch] || DRUMS[38];
          what = `<unpitched><display-step>${step}</display-step><display-octave>${octave}</display-octave></unpitched>`;
          if (notehead !== "normal") head = `<notehead>${notehead}</notehead>`;
        } else {
          what = pitchXml(n.pitch, fifths);
        }
        xml += `<note>${i ? "<chord/>" : ""}${what}<duration>${e.ticks}</duration>${ties}<voice>${voice}</voice><type>${e.type}</type>`
             + `${"<dot/>".repeat(e.dots)}${stem ? `<stem>${stem}</stem>` : ""}${head}<staff>${staff}</staff>${i === 0 && e.beams ? e.beams.join("") : ""}${tied}</note>`;
      });
    }
    return xml;
  }

  /** A staff with nothing in a measure: its whole-bar rest. */
  const restsXml = (measure, staff, voice) => `<note><rest measure="yes"/><duration>${measure.length}</duration><voice>${voice}</voice><staff>${staff}</staff></note>`;

  /** The staves the notation uses: [{clef, notes}], and whether they're drums. */
  function stavesOf(notes, mode, split) {
    if (mode === "drums") return { drums: true, staves: [{ clef: "percussion", notes }] };
    if (mode === "auto") {
      const pitches = notes.map((n) => n.pitch);
      const low = Math.min(...pitches), high = Math.max(...pitches);
      const middle = pitches.reduce((sum, p) => sum + p, 0) / Math.max(1, pitches.length);
      if (low >= 55 && (high >= 64 || middle >= 60)) mode = "treble";
      else if (high < 64) mode = "bass";
      else mode = "grand";
    }
    if (mode === "treble") return { drums: false, staves: [{ clef: "treble", notes }] };
    if (mode === "bass") return { drums: false, staves: [{ clef: "bass", notes }] };
    return { drums: false, staves: [{ clef: "treble", notes: notes.filter((n) => n.pitch >= split) },
                                   { clef: "bass", notes: notes.filter((n) => n.pitch < split) }] };
  }

  const CLEFS = { treble: "<sign>G</sign><line>2</line>", bass: "<sign>F</sign><line>4</line>", percussion: "<sign>percussion</sign>" };

  /** The whole score as MusicXML 4.0 (partwise): {xml, measures, notes} (xml empty: no notes). */
  function musicXml(track) {
    const map = Hub.tempoMap;
    const notes = quantize(trackNotes(track), options.grid);
    if (!notes.length) return { xml: "", measures: 0, notes: 0, cut: false };

    const mode = options.staff === "auto" && isDrumTrack(track) ? "drums" : options.staff;
    const { drums, staves } = stavesOf(notes, mode, options.split);

    // The bars, from the first note's to the last's.
    const first = map.barAt(notes[0].start / DIV + 1e-9);
    const lastEnd = Math.max(...notes.map((n) => n.end)) / DIV;
    const measures = [];
    for (let bar = first; bar.start < lastEnd - 1e-9 && measures.length < MAX_MEASURES; bar = map.barAt(bar.end + 1e-9)) {
      const compound = bar.denominator === 8 && bar.numerator % 3 === 0 && bar.numerator > 3;
      measures.push({ index: bar.index, start: Math.round(bar.start * DIV), end: Math.round(bar.end * DIV), length: Math.round(bar.length * DIV),
                      numerator: bar.numerator, denominator: bar.denominator, ppq: bar.start, compound,
                      beat: Math.round((compound ? 3 * 4 / bar.denominator : 4 / bar.denominator) * DIV) });
    }
    const cut = measures.length >= MAX_MEASURES && measures[measures.length - 1].end < lastEnd * DIV;
    const measureAt = (tick) => measures.find((m) => tick >= m.start && tick < m.end) || measures[measures.length - 1];

    const staffVoices = staves.map((staff) => (drums ? drumVoices(staff.notes, measureAt) : voicesOf(staff.notes)));
    const song = (Hub.state.song && Hub.state.song.name) || "Untitled";
    const today = new Date().toISOString().slice(0, 10);

    let xml = '<?xml version="1.0" encoding="UTF-8" standalone="no"?>\n'
      + '<!DOCTYPE score-partwise PUBLIC "-//Recordare//DTD MusicXML 4.0 Partwise//EN" "http://www.musicxml.org/dtds/partwise.dtd">\n'
      + '<score-partwise version="4.0">\n'
      + `<work><work-title>${escape(song)}</work-title></work>\n`
      + `<movement-title>${escape(track.name)}</movement-title>\n`
      + `<identification><encoding><software>Sonify</software><encoding-date>${today}</encoding-date></encoding></identification>\n`
      + `<part-list><score-part id="P1"><part-name>${escape(track.name)}</part-name></score-part></part-list>\n<part id="P1">\n`;

    // The chord track's chords as ticks, the one holding where the music starts at its first bar.
    const symbols = drums ? [] : map.chords.filter((c) => c.ppq * DIV >= measures[0].start - 1e-6).map((c) => ({ tick: Math.round(c.ppq * DIV), chord: c }));
    const holding = drums ? null : map.chordAt(measures[0].ppq);
    if (holding && !(symbols.length && symbols[0].tick === measures[0].start)) symbols.unshift({ tick: measures[0].start, chord: holding });

    let lastKey = null, lastTime = "";
    measures.forEach((m, i) => {
      const key = drums ? { fifths: 0, mode: "major" } : keyOf(m.ppq);
      const time = `${m.numerator}/${m.denominator}`;
      let attributes = "";
      if (i === 0) attributes += `<divisions>${DIV}</divisions>`;
      if (!lastKey || key.fifths !== lastKey.fifths || key.mode !== lastKey.mode) attributes += `<key><fifths>${key.fifths}</fifths><mode>${key.mode}</mode></key>`;
      if (time !== lastTime) attributes += `<time><beats>${m.numerator}</beats><beat-type>${m.denominator}</beat-type></time>`;
      if (i === 0) {
        if (staves.length > 1) attributes += `<staves>${staves.length}</staves>`;
        staves.forEach((staff, s) => { attributes += `<clef${staves.length > 1 ? ` number="${s + 1}"` : ""}>${CLEFS[staff.clef]}</clef>`; });
      }
      lastKey = key;
      lastTime = time;

      xml += `<measure number="${m.index + 1}">`;
      if (attributes) xml += `<attributes>${attributes}</attributes>`;
      if (i === 0) {
        const bpm = Math.round(map.bpmAt(m.ppq) * 100) / 100;
        xml += `<direction placement="above"><direction-type><metronome><beat-unit>quarter</beat-unit><per-minute>${bpm}</per-minute></metronome></direction-type><sound tempo="${bpm}"/></direction>`;
      }

      let first = true;
      staffVoices.forEach((voices, s) => {
        const staff = s + 1;
        const present = voices.map((v) => v.some((c) => c.end > m.start && c.start < m.end));
        const two = present.filter(Boolean).length > 1;
        voices.forEach((voice, v) => {
          const number = s * 4 + v + 1;
          const stem = drums ? (v === 0 ? "up" : "down") : two ? (v === 0 ? "up" : "down") : "";
          // The chord symbols go with the top staff's first voice.
          const harmonies = s === 0 && v === 0 && symbols.length
            ? (from, to) => symbols.filter((x) => x.tick >= from && x.tick < to).map((x) => harmonyXml(x.chord, key.fifths)).join("") : null;
          let part = voiceXml(voice, m, staff, number, stem, drums, key.fifths, harmonies);
          if (part === null && v === 0) part = (harmonies ? harmonies(m.start, m.end) : "") + restsXml(m, staff, number);   // every staff shows the bar
          if (part === null) return;
          if (!first) xml += `<backup><duration>${m.length}</duration></backup>`;
          xml += part;
          first = false;
        });
        if (!voices.length) {
          if (!first) xml += `<backup><duration>${m.length}</duration></backup>`;
          if (s === 0 && symbols.length) xml += symbols.filter((x) => x.tick >= m.start && x.tick < m.end).map((x) => harmonyXml(x.chord, key.fifths)).join("");
          xml += restsXml(m, staff, s * 4 + 1);
          first = false;
        }
      });
      if (i === measures.length - 1 && !cut) xml += '<barline location="right"><bar-style>light-heavy</bar-style></barline>';
      xml += "</measure>\n";
    });
    xml += "</part>\n</score-partwise>\n";
    return { xml, measures: measures.length, notes: notes.length, cut };
  }

  // ---------------------------------------------------------------- the libraries
  function loadScript(src) {
    return new Promise((resolve, reject) => {
      const script = document.createElement("script");
      script.src = src;
      script.onload = () => resolve();
      script.onerror = () => reject(new Error(`${src} didn't load`));
      document.head.appendChild(script);
    });
  }

  function loadLibraries() {
    if (!loading) loading = LIBRARIES.reduce((chain, src) => chain.then(() => loadScript(src)), Promise.resolve());
    return loading;
  }

  // ---------------------------------------------------------------- the view
  const scoreTrack = () => Hub.trackById(options.trackId);
  const instrumentTracks = () => Hub.state.tracks.filter((t) => t.kind !== "audio" && t.kind !== "fx" && t.kind !== "bus");

  function setStatus(text, action) {
    const status = $("score-status");
    status.replaceChildren(document.createTextNode(text || ""));
    if (action) {
      const button = element("button", "plain-button small", action.label);
      button.type = "button";
      button.addEventListener("click", action.run);
      status.append(" ", button);
    }
    status.hidden = !text;
  }

  function fillTracks() {
    const select = $("score-track");
    select.textContent = "";
    for (const track of instrumentTracks()) select.add(new Option(track.name, String(track.id)));
    select.value = String(options.trackId);
  }

  function showControls() {
    const track = scoreTrack();
    const drums = options.staff === "drums" || (options.staff === "auto" && track && isDrumTrack(track));
    $("score-split-box").hidden = options.staff !== "grand";
    $("score-export-pdf").disabled = !shownXml;
    $("score-export-xml").disabled = !shownXml;
    $("score-zoom-text").textContent = `${Math.round(options.zoom * 100)}%`;
    $("score-title").textContent = track ? `Score: ${track.name}` : "Score";
    $("score-paper").classList.toggle("drums", drums);
  }

  let rendering = false, again = false;

  /** Draws the score afresh; asked again while drawing, it draws once more when done. */
  async function render() {
    if (rendering) { again = true; return; }
    rendering = true;
    try {
      await draw();
    } finally {
      rendering = false;
      if (again) { again = false; render(); }
    }
  }

  async function draw() {
    const track = scoreTrack();
    shownXml = "";
    showControls();
    if (!track) { setStatus("Pick an instrument track."); $("score-render").replaceChildren(); return; }

    const made = musicXml(track);
    if (!made.xml) {
      $("score-render").replaceChildren();
      setStatus(`"${track.name}" has no notes to show yet.`);
      showControls();
      return;
    }

    setStatus("Writing the score\u2026");
    try {
      await loadLibraries();
      if (!osmd) {
        osmd = new window.opensheetmusicdisplay.OpenSheetMusicDisplay($("score-render"), {
          autoResize: true, backend: "svg", drawTitle: true, drawSubtitle: true, drawPartNames: false, drawMeasureNumbers: true,
          pageFormat: "Endless", drawingParameters: "default"
        });
      }
      await osmd.load(made.xml);
      osmd.zoom = options.zoom;
      osmd.render();
      shownXml = made.xml;
      setStatus(made.cut ? `The first ${made.measures} bars are shown (the score is very long).` : "");
    } catch (error) {
      console.error("Score:", error);
      setStatus("The score couldn't be drawn.");
    }
    showControls();
  }

  function renderSoon() {
    clearTimeout(renderTimer);
    renderTimer = setTimeout(render, 150);
  }

  /** Opens the Score for a track (the selected one when none is named). */
  function open(trackId) {
    const tracks = instrumentTracks();
    const wanted = Hub.trackById(trackId || Hub.state.selected);
    const track = wanted && tracks.includes(wanted) ? wanted : tracks[0];
    if (!track) { Hub.toast("There's no instrument track to show as a score."); return; }
    options.trackId = track.id;
    fillTracks();
    $("score-staff").value = options.staff;
    $("score-split").value = String(options.split);
    $("score-grid").value = String(options.grid);
    $("score-view").hidden = false;
    render();
  }

  function close() {
    $("score-view").hidden = true;
  }

  // ---------------------------------------------------------------- export
  const fileName = () => {
    const track = scoreTrack();
    const song = (Hub.state.song && Hub.state.song.name) || "Untitled";
    return `${song} - ${track ? track.name : "Score"}`;
  };

  function exportXml() {
    if (!shownXml) return;
    Hub.send("score", { action: "export", format: "musicxml", name: fileName(), data: shownXml });
  }

  /** A4 pages, drawn by a second copy of the score off screen, made into a PDF. */
  async function exportPdf() {
    if (!shownXml) return;
    setStatus("Making the PDF\u2026");
    const holder = element("div", "score-print");
    document.body.appendChild(holder);
    try {
      await loadLibraries();
      const printer = new window.opensheetmusicdisplay.OpenSheetMusicDisplay(holder, {
        autoResize: false, backend: "svg", drawTitle: true, drawSubtitle: true, drawPartNames: false, drawMeasureNumbers: true,
        pageFormat: "A4_P", pageBackgroundColor: "#FFFFFF"
      });
      await printer.load(shownXml);
      printer.render();

      const pages = [...holder.querySelectorAll("svg")];
      const { jsPDF } = window.jspdf;
      const pdf = new jsPDF({ orientation: "portrait", unit: "pt", format: "a4" });
      const width = pdf.internal.pageSize.getWidth(), height = pdf.internal.pageSize.getHeight();
      for (let i = 0; i < pages.length; i++) {
        if (i) pdf.addPage();
        const svg = pages[i];
        const w = Number(svg.getAttribute("width")) || svg.clientWidth || 1, h = Number(svg.getAttribute("height")) || svg.clientHeight || 1;
        const scale = Math.min(width / w, height / h);
        await pdf.svg(svg, { x: 0, y: 0, width: w * scale, height: h * scale });
      }
      const data = pdf.output("datauristring").split(",")[1] || "";
      Hub.send("score", { action: "export", format: "pdf", name: fileName(), data });
      setStatus("");
    } catch (error) {
      console.error("Score PDF:", error);
      setStatus("The PDF couldn't be made.");
    } finally {
      holder.remove();
    }
  }

  // Sonify's answer: where it saved the file (or why it couldn't).
  if (window.__JUCE__ && window.__JUCE__.backend)
    window.__JUCE__.backend.addEventListener("scoreExported", (data) => Hub.emit("scoreExported", data));

  Hub.on("scoreExported", (data) => {
    if (!data) return;
    if (data.ok) setStatus(`Saved ${data.name}`, { label: "Show in folder", run: () => Hub.send("score", { action: "reveal", path: data.path }) });
    else if (data.message) setStatus(data.message);
  });

  // ---------------------------------------------------------------- wiring
  Hub.commands.showScore = () => open();
  // A MIDI clip's right-click: its track.
  Hub.commands.showClipScore = () => {
    const found = Hub.findClip(Hub.state.selectedClips[Hub.state.selectedClips.length - 1]);
    open(found ? found.track.id : 0);
  };
  /** The MusicXML for a track, with these options for once (tests, and anything that needs it). */
  function musicXmlWith(track, overrides) {
    const kept = { ...options };
    Object.assign(options, overrides || {});
    try { return musicXml(track).xml; } finally { Object.assign(options, kept); }
  }

  Hub.score = { open, close, musicXml: musicXmlWith, isOpen: () => !$("score-view").hidden };

  Hub.on("tracks", () => {
    if ($("score-view").hidden) return;
    if (!scoreTrack()) { close(); return; }
    fillTracks();
    renderSoon();
  });
  Hub.on("tempoMap", () => { if (!$("score-view").hidden) renderSoon(); });

  Hub.on("ready", () => {
    const split = $("score-split");
    const names = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"];
    for (let pitch = 43; pitch <= 72; pitch++) split.add(new Option(`${names[pitch % 12]}${Math.floor(pitch / 12) - 1}`, String(pitch)));

    $("score-track").addEventListener("change", (event) => { options.trackId = Number(event.target.value); render(); });
    $("score-staff").addEventListener("change", (event) => { options.staff = event.target.value; render(); });
    split.addEventListener("change", (event) => { options.split = Number(event.target.value); render(); });
    $("score-grid").addEventListener("change", (event) => { options.grid = Number(event.target.value); render(); });
    $("score-zoom-out").addEventListener("click", () => { options.zoom = Math.max(0.4, Math.round((options.zoom - 0.1) * 10) / 10); render(); });
    $("score-zoom-in").addEventListener("click", () => { options.zoom = Math.min(2.5, Math.round((options.zoom + 0.1) * 10) / 10); render(); });
    $("score-export-pdf").addEventListener("click", exportPdf);
    $("score-export-xml").addEventListener("click", exportXml);
    $("score-close").addEventListener("click", close);
    $("score-view").addEventListener("keydown", (event) => {
      if (event.key === "Escape") { event.preventDefault(); event.stopPropagation(); close(); }
    });
  });
})();
