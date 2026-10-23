// D:\workspace\Fanan_web_hub\src\hub-board.js
//
// The Analog Board: a tab of its own (+ Open new tab › Analog Board, or View › Analog Board), a
// second mixer laid out as an analog desk. A strip for each instrument and audio track, left to
// right as the song has them, and the master section on the right.
//   A strip, top to bottom: the preamp (Trim, Drive, phase), the high-pass filter and EQ In, the
//   EQ (HF, HMF, LMF, LF: gain and frequency), the compressor (threshold, ratio, release, make-up,
//   Comp In, Fast, and its reduction lights), the sends to the Plate and the Tape Echo, pan, Mute
//   and Solo, the fader with its meter, and the track's name on a strip of tape.
//   The master: two VU meters, the Plate, the Tape Echo, the Bus Compressor, the Tape Machine and
//   the master fader.
// Mix on the board: the song is mixed here instead of on the Console. Each track's dry sound comes
// in (before its inserts); the Console's inserts, faders, sends, FX channels, buses and Main are
// left out. The first time, the strips start where the Console's faders are.
// Knobs: drag up or down (Shift for fine), the mouse wheel, the arrow keys; double-click puts one
// back where it starts. MIDI Link's chips link to any knob, fader or button here.

(() => {
  "use strict";

  const { $, element } = Hub;
  let board = { mixing: false, specs: { strip: [], master: [] }, strips: [], master: {} };
  let shown = false;
  let renderDue = true;
  let dragging = 0;                     // controls being dragged: no redraw under them
  const specs = { strip: new Map(), master: new Map() };
  const meters = new Map();             // strip id ("master" too) -> { columns, clip, clipUntil, lights }
  let vus = [];                         // the master's two needles
  const pending = new Map();            // "strip|id" -> value, sent at the next frame
  let flushing = false;

  // A knob cap's colour says what it does, as on a desk.
  const CAPS = {
    trim: "steel", drive: "steel", hpf: "stone",
    hf: "red", hfFreq: "red", hmf: "green", hmfFreq: "green", lmf: "blue", lmfFreq: "blue", lf: "brown", lfFreq: "brown",
    threshold: "amber", ratio: "amber", release: "amber", makeup: "amber",
    sendPlate: "ivory", sendEcho: "ivory", pan: "grey",
    plateDecay: "ivory", plateTone: "ivory", platePredelay: "ivory", plateReturn: "ivory",
    echoNote: "sand", echoTime: "sand", echoFeedback: "sand", echoTone: "sand", echoWow: "sand", echoReturn: "sand",
    busThreshold: "amber", busRatio: "amber", busAttack: "amber", busRelease: "amber", busMakeup: "amber", busMix: "amber",
    tapeDrive: "brown", tapeSpeed: "brown", tapeWow: "brown"
  };

  // What a strip's knob is called on it (its legend), where the setting's own name is long.
  const SHORT = { lfFreq: "Freq", lmfFreq: "Freq", hmfFreq: "Freq", hfFreq: "Freq", threshold: "Thresh", release: "Release",
                  makeup: "Gain", sendPlate: "Plate", sendEcho: "Echo", hpf: "HPF", busThreshold: "Thresh", busMakeup: "Gain",
                  platePredelay: "Pre-delay", tapeWow: "Wow", echoTime: "Time" };

  // The LED meters: VU, 0 at -18 dBFS (RMS), from -20 to +3; the lights' steps.
  const VU_ZERO_DBFS = -18;
  const STEPS = [-20, -15, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3];
  const GR_STEPS = [1, 3, 6, 10, 15];

  // ---------------------------------------------------------------- values
  const clamp01 = (v) => Math.max(0, Math.min(1, v));
  const isLog = (spec) => spec.unit === "Hz" || spec.id === "release" || spec.id === "echoTime";
  const isSendLevel = (spec) => spec.unit === "dB" && (spec.id.startsWith("send") || spec.id.endsWith("Return"));
  const isBipolar = (spec) => spec.min < 0 && spec.max > 0 && spec.kind !== "fader" && !isSendLevel(spec);

  /** A setting's value to its knob's position (0..1), along the same curves MIDI Link uses. */
  function toPosition(spec, value) {
    if (spec.kind === "toggle") return value >= 0.5 ? 1 : 0;
    if (spec.kind === "choice") return spec.max > 0 ? clamp01(value / spec.max) : 0;
    if (spec.kind === "fader") return Hub.dbToFader(value <= Hub.SILENT_DB ? -1000 : value);
    if (isLog(spec)) return clamp01(Math.log(value / spec.min) / Math.log(spec.max / spec.min));
    if (isSendLevel(spec)) return clamp01(1 - Math.sqrt(Math.max(0, (spec.max - value) / (spec.max - spec.min))));
    return clamp01((value - spec.min) / (spec.max - spec.min));
  }

  function fromPosition(spec, position) {
    const p = clamp01(position);
    if (spec.kind === "toggle") return p >= 0.5 ? 1 : 0;
    if (spec.kind === "choice") return Math.round(p * spec.max);
    if (spec.kind === "fader") return Math.max(spec.min, Math.round(Hub.faderToDb(p) * 10) / 10);
    if (isLog(spec)) return spec.min * Math.pow(spec.max / spec.min, p);
    if (isSendLevel(spec)) return spec.max - (spec.max - spec.min) * (1 - p) * (1 - p);
    if (spec.id === "pan") { const pan = p * 2 - 1; return Math.abs(pan) < 0.01 ? 0 : pan; }
    return spec.min + (spec.max - spec.min) * p;
  }

  const trimZeros = (text) => text.includes(".") ? text.replace(/0+$/, "").replace(/\.$/, "") : text;

  /** A value as the board shows it: "+3.0 dB", "2.4 kHz", "Off", "L30", "4:1". */
  function text(spec, value) {
    if (spec.kind === "toggle") return value >= 0.5 ? "On" : "Off";
    if (spec.kind === "choice") return (spec.choices || [])[Math.round(value)] || "";
    if (spec.id === "pan") {
      const amount = Math.round(Math.abs(value) * 100);
      return amount === 0 ? "C" : (value < 0 ? "L" : "R") + amount;
    }
    if (spec.unit === "dB") {
      if (spec.kind === "fader" && value <= Hub.SILENT_DB) return "\u2212\u221e dB";
      if (isSendLevel(spec) && value <= spec.min) return "Off";
      const shown = Math.abs(value) < 0.05 ? 0 : value;
      return (shown > 0 && ["lf", "lmf", "hmf", "hf", "trim"].includes(spec.id) ? "+" : "") + shown.toFixed(1) + " dB";
    }
    if (spec.unit === "Hz") {
      if (spec.id === "hpf" && value <= spec.min + 0.5) return "Off";
      return value >= 1000 ? trimZeros((value / 1000).toFixed(value >= 10000 ? 1 : 2)) + " kHz" : Math.round(value) + " Hz";
    }
    if (spec.unit === "ms") return (value < 10 ? trimZeros(value.toFixed(1)) : String(Math.round(value))) + " ms";
    if (spec.unit === "%") return Math.round(value) + "%";
    if (spec.unit === ":1") return trimZeros(value.toFixed(1)) + ":1";
    return trimZeros(value.toFixed(2));
  }

  /** A strip's values ("master" for the master's). */
  function valuesOf(stripId) {
    if (stripId === "master") return board.master;
    const strip = board.strips.find((s) => s.id === stripId);
    return strip ? strip.values : null;
  }

  function valueOf(stripId, spec) {
    const values = valuesOf(stripId);
    return values && values[spec.id] !== undefined ? values[spec.id] : spec.default;
  }

  /** Sets a value: here at once, to Sonify at the next frame (a drag sends each frame's last). */
  function setValue(stripId, spec, value) {
    let values = valuesOf(stripId);
    if (!values) {
      values = {};
      board.strips.push({ id: stripId, values });
    }
    values[spec.id] = value;
    pending.set(`${stripId}|${spec.id}`, { strip: stripId, id: spec.id, value });
    if (!flushing) {
      flushing = true;
      requestAnimationFrame(() => {
        flushing = false;
        for (const change of pending.values()) Hub.send("board", { action: "set", ...change });
        pending.clear();
      });
    }
    if (spec.id === "echoSync") updateEchoTime();
  }

  // ---------------------------------------------------------------- controls
  // Ticks round a knob: eleven over its 270 degrees, the middle one marked on a centred knob.
  const TICKS = (() => {
    let path = "";
    for (let i = 0; i <= 10; i++) {
      const a = (-135 + 27 * i) * Math.PI / 180;
      const x1 = 20 + 16.6 * Math.sin(a), y1 = 20 - 16.6 * Math.cos(a), x2 = 20 + 19 * Math.sin(a), y2 = 20 - 19 * Math.cos(a);
      path += `M${x1.toFixed(2)} ${y1.toFixed(2)}L${x2.toFixed(2)} ${y2.toFixed(2)}`;
    }
    return path;
  })();

  /** Where a strip is for MIDI Link: the track's number, or "master". */
  const linkStrip = (stripId) => {
    if (stripId === "master") return "master";
    const track = Hub.trackById(stripId);
    return track ? String(track.number) : "";
  };

  const stripName = (stripId) => {
    if (stripId === "master") return "Master";
    const track = Hub.trackById(stripId);
    return track ? `Track ${track.number} (${track.name})` : "Track";
  };

  function knob(stripId, spec, extraClass = "") {
    const holder = element("div", `bk ${extraClass}`.trim());
    holder.dataset.boardStrip = linkStrip(stripId);
    holder.dataset.boardParam = spec.id;
    holder.dataset.continuous = "1";
    const cap = CAPS[spec.id] || "grey";
    holder.style.setProperty("--cap", `var(--cap-${cap})`);
    if (cap === "ivory" || cap === "sand" || cap === "stone") holder.style.setProperty("--pointer", "#1d2125");
    holder.tabIndex = 0;
    holder.setAttribute("role", "slider");
    holder.setAttribute("aria-label", `${stripName(stripId)} ${spec.name}`);

    const centred = isBipolar(spec);
    const choice = spec.kind === "choice";
    holder.innerHTML = `<svg viewBox="0 0 40 40" aria-hidden="true"><path class="bk-ticks${choice ? " detents" : ""}" d="${TICKS}"/>`
      + (centred ? '<circle class="bk-centre" cx="20" cy="2.2" r="1.2"/>' : "")
      + '<circle class="bk-skirt" cx="20" cy="20" r="14.6"/><circle class="bk-cap" cx="20" cy="20" r="10.8"/>'
      + '<circle class="bk-shine" cx="20" cy="20" r="10.8"/><g class="bk-rot"><line class="bk-pointer" x1="20" y1="6.2" x2="20" y2="13.5"/></g></svg>';
    const label = element("span", "bk-label", SHORT[spec.id] || spec.name);
    holder.appendChild(label);

    const rot = holder.querySelector(".bk-rot");
    const show = () => {
      const value = valueOf(stripId, spec);
      const position = toPosition(spec, value);
      rot.setAttribute("transform", `rotate(${(-135 + 270 * position).toFixed(1)} 20 20)`);
      holder.setAttribute("aria-valuetext", text(spec, value));
      holder.setAttribute("aria-valuenow", String(Math.round(position * 100)));
      holder.title = `${spec.name}: ${text(spec, value)}`;
      if (holder.classList.contains("reading")) label.textContent = text(spec, value);
    };

    const set = (value) => { setValue(stripId, spec, value); show(); };
    const setPosition = (position) => set(fromPosition(spec, position));
    const reading = (on) => {
      holder.classList.toggle("reading", on);
      label.textContent = on ? text(spec, valueOf(stripId, spec)) : (SHORT[spec.id] || spec.name);
    };

    let drag = null;
    holder.addEventListener("pointerdown", (event) => {
      if (event.button !== 0 || document.body.classList.contains("linking")) return;
      event.preventDefault();
      holder.focus();
      holder.setPointerCapture(event.pointerId);
      drag = { y: event.clientY, position: toPosition(spec, valueOf(stripId, spec)), fine: event.shiftKey };
      dragging++;
      reading(true);
    });
    holder.addEventListener("pointermove", (event) => {
      if (!drag) return;
      // Shift pressed or let go mid-drag: carries on from here at the new speed.
      if (event.shiftKey !== drag.fine) drag = { y: event.clientY, position: toPosition(spec, valueOf(stripId, spec)), fine: event.shiftKey };
      const travel = choice ? Math.max(60, 24 * spec.max) : (drag.fine ? 700 : 170);
      setPosition(drag.position + (drag.y - event.clientY) / travel);
    });
    const end = () => {
      if (!drag) return;
      drag = null;
      dragging = Math.max(0, dragging - 1);
      reading(holder.matches(":hover"));
      if (renderDue && !dragging) render();
    };
    holder.addEventListener("pointerup", end);
    holder.addEventListener("pointercancel", end);
    holder.addEventListener("lostpointercapture", end);   // the tab left mid-drag, say
    holder.addEventListener("pointerenter", () => { if (!drag) reading(true); });
    holder.addEventListener("pointerleave", () => { if (!drag) reading(false); });
    holder.addEventListener("dblclick", () => set(spec.default));
    holder.addEventListener("wheel", (event) => {
      event.preventDefault();
      const step = choice ? 1 / Math.max(1, spec.max) : event.shiftKey ? 0.004 : 0.02;
      setPosition(toPosition(spec, valueOf(stripId, spec)) + (event.deltaY < 0 ? step : -step));
    }, { passive: false });
    holder.addEventListener("keydown", (event) => {
      const step = choice ? 1 / Math.max(1, spec.max) : event.shiftKey ? 0.004 : 0.02;
      const now = toPosition(spec, valueOf(stripId, spec));
      if (event.key === "ArrowUp" || event.key === "ArrowRight") setPosition(now + step);
      else if (event.key === "ArrowDown" || event.key === "ArrowLeft") setPosition(now - step);
      else if (event.key === "Home") setPosition(0);
      else if (event.key === "End") setPosition(1);
      else if (event.key === "Delete" || event.key === "Backspace") set(spec.default);
      else return;
      event.preventDefault();
      event.stopPropagation();
    });

    show();
    return holder;
  }

  /** A lit push button (a switch). */
  function button(stripId, spec, label, extraClass = "") {
    const holder = element("button", `bb ${extraClass}`.trim(), label);
    holder.type = "button";
    holder.dataset.boardStrip = linkStrip(stripId);
    holder.dataset.boardParam = spec.id;
    holder.setAttribute("aria-label", `${stripName(stripId)} ${spec.name}`);
    const show = () => {
      const on = valueOf(stripId, spec) >= 0.5;
      holder.setAttribute("aria-pressed", String(on));
      holder.title = `${spec.name}: ${on ? "on" : "off"}`;
    };
    holder.addEventListener("click", () => {
      if (document.body.classList.contains("linking")) return;
      setValue(stripId, spec, valueOf(stripId, spec) >= 0.5 ? 0 : 1);
      show();
    });
    show();
    return holder;
  }

  /** A long fader, on the Console's scale, with its value under it. */
  function fader(stripId, spec, meterOf) {
    const holder = element("div", "bf-area");
    const travel = element("div", "bf");
    travel.dataset.boardStrip = linkStrip(stripId);
    travel.dataset.boardParam = spec.id;
    travel.dataset.continuous = "1";
    travel.tabIndex = 0;
    travel.setAttribute("role", "slider");
    travel.setAttribute("aria-label", `${stripName(stripId)} fader`);
    travel.innerHTML = '<div class="bf-slot"></div>'
      + [10, 5, 0, -5, -10, -20, -30, -40, -60].map((db) =>
        `<span class="bf-mark${db === 0 ? " zero" : ""}" style="bottom:calc(var(--cap-h) / 2 + ${Hub.dbToFader(db).toFixed(4)} * (100% - var(--cap-h)))">${db > 0 ? "+" + db : db}</span>`).join("")
      + '<div class="bf-cap"></div>';
    const readout = element("span", "bf-readout");
    const row = element("div", "bf-row");
    row.appendChild(travel);
    if (meterOf) row.appendChild(meterOf);
    holder.append(row, readout);

    const show = () => {
      const value = valueOf(stripId, spec);
      travel.style.setProperty("--pos", toPosition(spec, value).toFixed(4));
      readout.textContent = text(spec, value);
      travel.setAttribute("aria-valuetext", text(spec, value));
    };
    const set = (value) => { setValue(stripId, spec, value); show(); };

    let drag = null;
    travel.addEventListener("pointerdown", (event) => {
      if (event.button !== 0 || document.body.classList.contains("linking")) return;
      event.preventDefault();
      travel.focus();
      travel.setPointerCapture(event.pointerId);
      drag = { y: event.clientY, position: toPosition(spec, valueOf(stripId, spec)), fine: event.shiftKey };
      dragging++;
    });
    travel.addEventListener("pointermove", (event) => {
      if (!drag) return;
      if (event.shiftKey !== drag.fine) drag = { y: event.clientY, position: toPosition(spec, valueOf(stripId, spec)), fine: event.shiftKey };
      const height = Math.max(40, travel.clientHeight - 30);
      set(fromPosition(spec, drag.position + (drag.y - event.clientY) / height * (drag.fine ? 0.2 : 1)));
    });
    const end = () => {
      if (!drag) return;
      drag = null;
      dragging = Math.max(0, dragging - 1);
      if (renderDue && !dragging) render();
    };
    travel.addEventListener("pointerup", end);
    travel.addEventListener("pointercancel", end);
    travel.addEventListener("lostpointercapture", end);
    travel.addEventListener("dblclick", () => set(0));
    // A step up from all the way down starts where its scale does (-72 dB), not below it.
    const step = (up) => {
      const now = toPosition(spec, valueOf(stripId, spec));
      set(fromPosition(spec, (up ? Math.max(now, Hub.dbToFader(spec.min)) : now) + (up ? 0.01 : -0.01)));
    };
    travel.addEventListener("wheel", (event) => {
      event.preventDefault();
      step(event.deltaY < 0);
    }, { passive: false });
    travel.addEventListener("keydown", (event) => {
      if (event.key === "ArrowUp") step(true);
      else if (event.key === "ArrowDown") step(false);
      else if (event.key === "Delete" || event.key === "Backspace") set(0);
      else return;
      event.preventDefault();
      event.stopPropagation();
    });

    show();
    return holder;
  }

  /** A stereo LED meter with a clip light; kept by the strip's id for the status's levels. */
  function ledMeter(stripId) {
    const holder = element("div", "bm");
    holder.setAttribute("aria-hidden", "true");
    const clip = element("span", "bm-clip");
    const columns = [element("span", "bm-col"), element("span", "bm-col")];
    const pair = element("div", "bm-pair");
    pair.append(...columns);
    holder.append(clip, pair);
    meters.set(stripId, { columns, clip, clipUntil: 0, lights: meters.get(stripId) ? meters.get(stripId).lights : null, lit: [-1, -1] });
    return holder;
  }

  function reductionLights(stripId) {
    const holder = element("div", "bg");
    holder.setAttribute("aria-hidden", "true");
    holder.title = "Gain reduction: 1, 3, 6, 10 and 15 dB";
    const lights = GR_STEPS.map(() => element("span", "bg-led"));
    holder.append(...lights);
    const meter = meters.get(stripId) || { lit: [-1, -1] };
    meter.lights = lights;
    meter.reductionLit = -1;
    meters.set(stripId, meter);
    return holder;
  }

  // ---------------------------------------------------------------- a strip
  const cell = (cls, ...children) => {
    const holder = element("div", `bs-cell ${cls}`);
    holder.append(...children.filter(Boolean));
    return holder;
  };

  function strip(track) {
    const id = track.id;
    const s = (key) => specs.strip.get(key);
    const holder = element("div", "bs");
    holder.dataset.id = String(id);
    holder.style.setProperty("--track", track.colour || "#888");

    const meter = ledMeter(id);
    holder.append(
      cell("r-preamp", knob(id, s("trim")), knob(id, s("drive"))),
      cell("r-filter", button(id, s("phase"), "\u00d8", "small"), knob(id, s("hpf"), "small"), button(id, s("eqOn"), "EQ", "small")),
      cell("r-hf", knob(id, s("hf")), knob(id, s("hfFreq"), "freq")),
      cell("r-hmf", knob(id, s("hmf")), knob(id, s("hmfFreq"), "freq")),
      cell("r-lmf", knob(id, s("lmf")), knob(id, s("lmfFreq"), "freq")),
      cell("r-lf", knob(id, s("lf")), knob(id, s("lfFreq"), "freq")),
      cell("r-dyn1", knob(id, s("threshold")), knob(id, s("ratio"))),
      cell("r-dyn2", knob(id, s("release")), knob(id, s("makeup"))),
      cell("r-dyn3", button(id, s("compOn"), "Comp", "small amber"), button(id, s("fastAttack"), "Fast", "small amber"), reductionLights(id)),
      cell("r-sends", knob(id, s("sendPlate")), knob(id, s("sendEcho"))),
      cell("r-pan", knob(id, s("pan"))),
      cell("r-buttons", button(id, s("mute"), "Mute", "mute"), button(id, s("solo"), "Solo", "solo")),
      cell("r-fader", fader(id, s("volume"), meter))
    );

    const tape = element("div", "bs-tape", track.name);
    tape.title = `Track ${track.number}: ${track.name}\nRight-click: reset this strip`;
    tape.addEventListener("contextmenu", (event) => {
      event.preventDefault();
      Hub.popupMenu(tape, [{ group: track.name }, { label: "Reset this strip", value: "reset" }],
                    () => Hub.send("board", { action: "reset", strip: id }));
    });
    const number = element("span", "bs-number", String(track.number));
    holder.append(cell("r-name", number, tape));
    return holder;
  }

  // ---------------------------------------------------------------- the master
  function vuMeter(side) {
    const holder = element("div", "vu");
    holder.setAttribute("aria-hidden", "true");
    // The scale: -20 .. +3 VU on a voltage law, red from 0.
    const angleOf = (vu) => {
      const v = Math.pow(10, vu / 20), p = (v - 0.1) / (Math.pow(10, 3 / 20) - 0.1);
      return -48 + 96 * Math.max(-0.04, Math.min(1.04, p));
    };
    const at = (angle, r) => [60 + r * Math.sin(angle * Math.PI / 180), 74 - r * Math.cos(angle * Math.PI / 180)];
    const marks = [-20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3];
    let scale = "";
    for (const vu of marks) {
      const a = angleOf(vu);
      const [x1, y1] = at(a, 52), [x2, y2] = at(a, vu === 0 || vu === -20 || vu === 3 ? 60 : 57);
      const [tx, ty] = at(a, 66);
      scale += `<line class="vu-tick${vu > 0 ? " hot" : ""}" x1="${x1.toFixed(1)}" y1="${y1.toFixed(1)}" x2="${x2.toFixed(1)}" y2="${y2.toFixed(1)}"/>`;
      if ([-20, -10, -7, -5, -3, 0, 3].includes(vu)) scale += `<text class="vu-num${vu > 0 ? " hot" : ""}" x="${tx.toFixed(1)}" y="${ty.toFixed(1)}">${vu > 0 ? "+" + vu : vu === 0 ? "0" : Math.abs(vu)}</text>`;
    }
    const [a0x, a0y] = at(angleOf(0), 55), [a3x, a3y] = at(angleOf(3), 55), [m0x, m0y] = at(angleOf(-20), 55);
    holder.innerHTML = `<svg viewBox="0 0 120 84"><path class="vu-arc" d="M${m0x.toFixed(1)} ${m0y.toFixed(1)}A55 55 0 0 1 ${a0x.toFixed(1)} ${a0y.toFixed(1)}"/>`
      + `<path class="vu-arc hot" d="M${a0x.toFixed(1)} ${a0y.toFixed(1)}A55 55 0 0 1 ${a3x.toFixed(1)} ${a3y.toFixed(1)}"/>${scale}`
      + `<text class="vu-word" x="60" y="54">VU</text><text class="vu-side" x="60" y="66">${side}</text>`
      + `<g class="vu-needle"><line x1="60" y1="80" x2="60" y2="16"/></g><circle class="vu-pivot" cx="60" cy="80" r="6"/></svg>`;
    const needle = holder.querySelector(".vu-needle");
    needle.style.transform = `rotate(${angleOf(-40).toFixed(1)}deg)`;
    return { holder, needle, angleOf, shown: -999 };
  }

  function master() {
    const m = (key) => specs.master.get(key);
    const holder = element("div", "bmaster");
    const id = "master";

    vus = [vuMeter("Left"), vuMeter("Right")];
    const meterBridge = element("div", "bmaster-vus");
    meterBridge.append(vus[0].holder, vus[1].holder);
    const glue = element("div", "bmaster-gr");
    glue.append(element("span", "bmaster-gr-label", "Bus comp"), reductionLights("master"));

    const section = (title, cls, ...rows) => {
      const box = element("section", `bmod ${cls}`);
      box.appendChild(element("h3", "bmod-title", title));
      for (const row of rows) {
        const line = element("div", "bmod-row");
        line.append(...row.filter(Boolean));
        box.appendChild(line);
      }
      return box;
    };

    const echoTime = element("div", "echo-time");
    echoTime.append(knob(id, m("echoNote"), "note"), knob(id, m("echoTime"), "ms"));

    const modules = element("div", "bmaster-modules");
    modules.append(
      section("Plate", "plate", [knob(id, m("plateDecay")), knob(id, m("plateTone")), knob(id, m("platePredelay")), knob(id, m("plateReturn"))]),
      section("Tape Echo", "echo", [button(id, m("echoSync"), "Sync", "small"), echoTime, knob(id, m("echoFeedback"))],
              [knob(id, m("echoTone")), knob(id, m("echoWow")), knob(id, m("echoReturn"))]),
      section("Bus Compressor", "glue", [button(id, m("busOn"), "In", "small amber"), knob(id, m("busThreshold")), knob(id, m("busRatio")), knob(id, m("busAttack"))],
              [knob(id, m("busRelease")), knob(id, m("busMakeup")), knob(id, m("busMix"))]),
      section("Tape Machine", "tape", [button(id, m("tapeOn"), "In", "small amber"), knob(id, m("tapeDrive")), knob(id, m("tapeSpeed")), knob(id, m("tapeWow")),
                                       button(id, m("tapeHiss"), "Hiss", "small")])
    );

    const left = element("div", "bmaster-left");
    left.append(meterBridge, glue, modules);
    const right = element("div", "bmaster-right");
    right.append(element("span", "bmaster-name", "Master"), fader(id, m("masterVolume"), ledMeter("master")));
    holder.append(left, right);
    return holder;
  }

  /** The Tape Echo shows the note it's synced to, or its milliseconds. */
  function updateEchoTime() {
    const synced = valueOf("master", specs.master.get("echoSync") || { id: "echoSync", default: 1 }) >= 0.5;
    const box = document.querySelector("#board-master .echo-time");
    if (box) box.classList.toggle("free", !synced);
  }

  // ---------------------------------------------------------------- the board
  function render() {
    if (!shown) { renderDue = true; return; }
    if (dragging) { renderDue = true; return; }
    renderDue = false;

    const root = $("analog-board");
    root.classList.toggle("on", Boolean(board.mixing));
    const toggle = $("board-mixing");
    toggle.setAttribute("aria-checked", String(Boolean(board.mixing)));
    $("board-note").textContent = board.mixing
      ? "The board mixes the song. Each track's dry sound comes in here; the Console's inserts, faders, sends and Main are left out."
      : "The Console mixes the song. Set the board up here, then switch it on to mix on it.";

    if (!specs.strip.size) return;

    // Keeps the strips where they're scrolled to.
    const strips = $("board-strips");
    const scrollLeft = strips.scrollLeft, scrollTop = strips.scrollTop;
    const focused = document.activeElement && document.activeElement.closest && document.activeElement.closest("#analog-board [data-board-param]");
    const focusKey = focused ? `${focused.dataset.boardStrip}|${focused.dataset.boardParam}` : "";

    meters.clear();
    const tracks = (Hub.state.tracks || []).filter((t) => t.kind === "instrument" || t.kind === "audio");
    strips.replaceChildren($("board-legend"), ...tracks.map(strip));   // the legend first: it stays at the left as they scroll
    if (!tracks.length) strips.appendChild(element("p", "board-empty", "Add an instrument or audio track: each one gets a strip here."));
    strips.scrollLeft = scrollLeft;
    strips.scrollTop = scrollTop;

    $("board-master").replaceChildren(master());
    updateEchoTime();

    if (focusKey) {
      const [s, p] = focusKey.split("|");
      const again = document.querySelector(`#analog-board [data-board-strip="${CSS.escape(s)}"][data-board-param="${CSS.escape(p)}"]`);
      if (again) again.focus();
    }
  }

  function renderLegend() {
    const legend = $("board-legend");
    if (legend.childElementCount) return;
    const rows = [["Preamp", "r-preamp"], ["Filter", "r-filter"], ["EQ", "r-eq"], ["Dynamics", "r-dyn"], ["Sends", "r-sends"], ["Pan", "r-pan"], ["", "r-buttons"], ["Fader", "r-fader"]];
    for (const [name, cls] of rows) legend.appendChild(element("span", `bl ${cls}`, name));
  }

  // ---------------------------------------------------------------- meters
  const litCount = (vu) => STEPS.filter((step) => vu >= step).length;

  function feedMeters(levels) {
    if (!shown || !levels) return;
    const now = performance.now();
    const off = !board.mixing;

    const apply = (meter, reading) => {
      if (!meter || !reading) return;
      const [vuL, vuR, peakL, peakR, reduction] = off ? [-100, -100, -100, -100, 0] : reading;
      if (meter.columns) {
        [vuL, vuR].forEach((dbfs, side) => {
          const lit = litCount(dbfs - VU_ZERO_DBFS);
          if (lit !== meter.lit[side]) {
            meter.lit[side] = lit;
            meter.columns[side].style.setProperty("--lit", String(lit));
          }
        });
        if (Math.max(peakL, peakR) > -0.3) meter.clipUntil = now + 1200;
        const clipped = now < meter.clipUntil;
        if (clipped !== meter.clipped) { meter.clipped = clipped; meter.clip.classList.toggle("lit", clipped); }
      }
      if (meter.lights) {
        const lit = GR_STEPS.filter((step) => reduction >= step).length;
        if (lit !== meter.reductionLit) {
          meter.reductionLit = lit;
          meter.lights.forEach((light, i) => light.classList.toggle("lit", i < lit));
        }
      }
    };

    for (const [id, meter] of meters) apply(meter, id === "master" ? levels.master : (levels.strips || {})[String(id)]);

    // The VU needles: the master's sound, on a VU's scale (0 VU at -18 dBFS).
    if (levels.master) {
      vus.forEach((vu, side) => {
        const dbfs = off ? -100 : levels.master[side];
        const angle = Math.round(vu.angleOf(dbfs - VU_ZERO_DBFS) * 10) / 10;
        if (angle !== vu.shown) {
          vu.shown = angle;
          vu.needle.style.transform = `rotate(${angle}deg)`;
        }
      });
    }
  }

  // ---------------------------------------------------------------- from Sonify
  Hub.on("board", (data) => {
    board = { mixing: Boolean(data.mixing), specs: data.specs || board.specs, strips: data.strips || [], master: data.master || {} };
    // A value still on its way to Sonify stays as the page has it.
    for (const change of pending.values()) {
      const values = valuesOf(change.strip);
      if (values) values[change.id] = change.value;
    }
    specs.strip = new Map((board.specs.strip || []).map((spec) => [spec.id, spec]));
    specs.master = new Map((board.specs.master || []).map((spec) => [spec.id, spec]));
    render();
  });

  Hub.on("tracks", () => { if (shown) render(); else renderDue = true; });
  Hub.on("status", (status) => feedMeters(status.board));

  // ---------------------------------------------------------------- the tab
  function show(on) {
    if (on === shown) return;
    shown = on;
    if (!on) dragging = 0;   // nothing is held on a page that isn't shown
    $("analog-board").hidden = !on;
    Hub.send("board", { action: "shown", shown: on });
    if (on) {
      renderLegend();
      render();
    }
  }

  Hub.board = {
    show,
    isShown: () => shown,
    isMixing: () => Boolean(board.mixing),
    setMixing: (on) => Hub.send("board", { action: "mixing", on: Boolean(on) }),
    /** For MIDI Link: whether a board key ("board.<n>.<setting>") is a knob or fader (not a button). */
    isContinuous(key) {
      const parts = String(key).split(".");
      const spec = parts[1] === "master" ? specs.master.get(parts[2]) : specs.strip.get(parts[2]);
      return Boolean(spec) && spec.kind !== "toggle";
    },
    /** "Board · Track 2 (Piano) · HMF", "Board · Master · Return". */
    targetText(key) {
      const parts = String(key).split(".");
      const spec = parts[1] === "master" ? specs.master.get(parts[2]) : specs.strip.get(parts[2]);
      const track = parts[1] === "master" ? null : (Hub.state.tracks || []).find((t) => t.number === Number(parts[1]));
      const where = parts[1] === "master" ? "Master" : `Track ${parts[1]}${track ? ` (${track.name})` : ""}`;
      const section = spec && spec.section && spec.section !== "Fader" && spec.section !== "Master" ? `${spec.section} ` : "";
      return `Board · ${where} · ${section}${spec ? spec.name : parts[2]}`;
    }
  };

  Hub.on("ready", () => {
    $("board-mixing").addEventListener("click", () => Hub.board.setMixing(!board.mixing));
    $("board-copy").addEventListener("click", () => Hub.send("board", { action: "copyConsole" }));
    $("board-reset").addEventListener("click", (event) => {
      Hub.popupMenu(event.currentTarget, [
        { group: "Put every strip and the master back where they start?" },
        { label: "Reset the board", value: true },
        { label: "Keep it", value: false }
      ], (yes) => { if (yes) Hub.send("board", { action: "resetAll" }); });
    });
  });
})();
