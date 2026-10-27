// ============================================================================
//  AXIS — web UI
//
//  The same page the design was approved as, wired to the plugin. Opening this
//  file directly in a browser still works: without the native bridge every
//  control falls back to local state, which is what makes design iteration
//  cheap.
// ============================================================================

/* ── native bridge ───────────────────────────────────────── */
let Juce = null;

// The JUCE frontend library is served at the resource-provider ROOT, not next
// to this file. A relative "./juce/index.js" would resolve to /js/juce/index.js
// and — because the provider matches on filename — fetch THIS file again,
// recursing until the module graph dies and nothing on the page runs.
try {
  Juce = await import("/juce/index.js");
} catch (e) {
  console.log("AXIS: no native bridge, running as a design preview —", e && e.message);
}

const native = (name) => {
  if (Juce && typeof Juce.getNativeFunction === "function") {
    try { return Juce.getNativeFunction(name); } catch (e) { /* fall through */ }
  }
  return async () => undefined;
};

const setParam     = native("axisSetParam");
const beginGesture = native("axisBeginGesture");
const endGesture   = native("axisEndGesture");
const getState     = native("axisGetState");
const setOrder     = native("axisSetChainOrder");
const loadCapture  = native("axisLoadCapture");
const presetMenu   = native("axisPresetMenu");
const presetStep   = native("axisPresetStep");
const presetLoad   = native("axisPresetLoad");
const presetSave   = native("axisPresetSave");
const presetQuick  = native("axisPresetQuickSave");

const onBackend = (event, fn) => {
  if (window.__JUCE__ && window.__JUCE__.backend)
    window.__JUCE__.backend.addEventListener(event, fn);
};

/* ── module definitions — the five shipping pedals ───────── */
const MODULES = [
  {
    id:"GATE", type:"UTILITY", skin:"en-grey", bypassId:"GATE_BYPASS", on:true,
    sub:"ADAPTIVE NOISE GATE",
    voice:{ id:"GATE_MODE", options:["FAST","SMOOTH","ADAPT"], index:2 },
    knobs:[
      {id:"GATE_THRESH",  label:"THRESH",  size:"sm", min:-80, max:0,   val:-46, unit:"dB"},
      {id:"GATE_DEPTH",   label:"DEPTH",   size:"lg", min:0,   max:100, val:88},
      {id:"GATE_RELEASE", label:"RELEASE", size:"lg", min:5,   max:800, val:160, unit:"ms"},
      {id:"GATE_HOLD",    label:"HOLD",    size:"sm", min:0,   max:500, val:40,  unit:"ms"},
      {id:"GATE_SC_HPF",  label:"SC HPF",  size:"sm", min:20,  max:800, val:120, unit:"Hz"}
    ],
    buttons:[
      {id:"GATE_HISS", label:"HISS FILTER", on:true},
      {id:"GATE_LEARN",label:"AUTO-LEARN",  on:false}
    ],
    fader:{ id:"GATE_RANGE", label:"RANGE", min:0, max:100, val:100, unit:"%" }
  },
  {
    id:"COMP", type:"DYNAMICS", skin:"en-blue", bypassId:"COMP_BYPASS", on:true,
    sub:"OPTICAL COMPRESSOR",
    voice:{ id:"COMP_CIRCUIT", options:["OPTO","FET","VCA"], index:0 },
    knobs:[
      {id:"COMP_THRESH",  label:"THRESH",  size:"sm", min:-48, max:0,   val:-18, unit:"dB"},
      {id:"COMP_RATIO",   label:"RATIO",   size:"lg", min:1,   max:20,  val:4,   unit:":1"},
      {id:"COMP_MAKEUP",  label:"MAKEUP",  size:"lg", min:0,   max:24,  val:6,   unit:"dB"},
      {id:"COMP_ATTACK",  label:"ATTACK",  size:"sm", min:1,   max:120, val:14,  unit:"ms"},
      {id:"COMP_RELEASE", label:"RELEASE", size:"sm", min:20,  max:1200,val:280, unit:"ms"}
    ],
    buttons:[
      {id:"COMP_AUTO",   label:"AUTO GAIN", on:true},
      {id:"COMP_SC_HPF", label:"SC HPF",    on:false},
      {id:"COMP_PARAL",  label:"PARALLEL",  on:false}
    ],
    fader:{ id:"COMP_MIX", label:"MIX", min:0, max:100, val:100, unit:"%" }
  },
  {
    id:"DRIVE", type:"GAIN", skin:"en-amber", bypassId:"DRV_BYPASS", on:true, selected:true,
    sub:"TRANSPARENT OVERDRIVE",
    voice:{ id:"DRV_VOICE", options:["WARM","OPEN","TIGHT"], index:0 },
    knobs:[
      {id:"DRV_TONE",  label:"TONE",  size:"sm", min:0, max:100, val:42},
      {id:"DRV_GAIN",  label:"GAIN",  size:"lg", min:0, max:100, val:64},
      {id:"DRV_LEVEL", label:"LEVEL", size:"lg", min:0, max:100, val:55},
      {id:"DRV_BODY",  label:"BODY",  size:"sm", min:0, max:100, val:38},
      {id:"DRV_BITE",  label:"BITE",  size:"sm", min:0, max:100, val:71}
    ],
    buttons:[
      {id:"DRV_DYNAMIC",label:"DYNAMIC TOUCH", on:true},
      {id:"DRV_STEREO", label:"TRUE STEREO",   on:false},
      {id:"DRV_MORPH",  label:"MORPH LINK",    on:false}
    ],
    fader:{ id:"DRV_MIX", label:"MIX", min:0, max:100, val:100, unit:"%" }
  },
  {
    id:"AMP", type:"CAPTURE", skin:"en-teal", bypassId:"AMP_BYPASS", on:true,
    sub:"NAM CAPTURE PLAYER",
    voice:{ id:"AMP_SLIM", options:["SLIM 40","SLIM 60","FULL"], index:1 },
    knobs:[
      {id:"AMP_TRIM",    label:"TRIM",    size:"sm", min:-12, max:12,  val:0,  unit:"dB"},
      {id:"AMP_BASS",    label:"BASS",    size:"lg", min:0,   max:100, val:52},
      {id:"AMP_MID",     label:"MID",     size:"lg", min:0,   max:100, val:48},
      {id:"AMP_TREBLE",  label:"TREBLE",  size:"sm", min:0,   max:100, val:58},
      {id:"AMP_PRESENCE",label:"PRESENCE",size:"sm", min:0,   max:100, val:44}
    ],
    buttons:[
      {id:"AMP_CAB",   label:"CAB SIM",    on:true},
      {id:"AMP_STACK", label:"STACK POST", on:false},
      {id:"AMP_DBL",   label:"DOUBLER",    on:false}
    ],
    fader:{ id:"AMP_OUTPUT", label:"OUTPUT", min:0, max:100, val:72, unit:"%" }
  },
  {
    id:"SPACE", type:"REVERB", skin:"en-crm", bypassId:"SPC_BYPASS", on:true,
    sub:"PLATE / HALL / SPRING",
    voice:{ id:"SPC_TYPE", options:["PLATE","HALL","SPRING"], index:2 },
    knobs:[
      {id:"SPC_PREDLY", label:"PRE-DLY", size:"sm", min:0,  max:250, val:24, unit:"ms"},
      {id:"SPC_DECAY",  label:"DECAY",   size:"lg", min:0,  max:100, val:46},
      {id:"SPC_SIZE",   label:"SIZE",    size:"lg", min:0,  max:100, val:58},
      {id:"SPC_DAMP",   label:"DAMP",    size:"sm", min:0,  max:100, val:62},
      {id:"SPC_WIDTH",  label:"WIDTH",   size:"sm", min:0,  max:100, val:80}
    ],
    buttons:[
      {id:"SPC_SHIMMER",label:"SHIMMER", on:false},
      {id:"SPC_FREEZE", label:"FREEZE",  on:false},
      {id:"SPC_DUCK",   label:"DUCK",    on:true}
    ],
    fader:{ id:"SPC_MIX", label:"MIX", min:0, max:100, val:28, unit:"%" }
  }
];

const byParam = {};
MODULES.forEach(m => {
  m.knobs.forEach(k => byParam[k.id] = { kind:"knob", module:m, def:k });
  m.buttons.forEach(b => byParam[b.id] = { kind:"button", module:m, def:b });
  byParam[m.fader.id] = { kind:"fader", module:m, def:m.fader };
  byParam[m.voice.id] = { kind:"voice", module:m, def:m.voice };
  byParam[m.bypassId] = { kind:"bypass", module:m };
});

/* ── formatting ──────────────────────────────────────────── */
const A0 = -225, A1 = 45;

const fmt = (d) => {
  const v = d.val, u = d.unit || "";
  if (u === "dB")  return (v > 0 ? "+" : "") + v.toFixed(1) + "dB";
  if (u === ":1")  return v.toFixed(1) + ":1";
  if (u === "ms" || u === "Hz") return Math.round(v) + " " + u;
  if (u === "%")   return Math.round(v) + "%";
  return Math.round(v);
};

/* ── knob widget ─────────────────────────────────────────── */
const ticks = (el, lg) => {
  const S = lg ? 124 : 90, C = S / 2, R = lg ? 58 : 42;
  let out = "";
  for (let i = 0; i <= 10; i++) {
    const a = (A0 + (A1 - A0) * i / 10) * Math.PI / 180;
    const maj = i % 5 === 0, r0 = R - (maj ? 7 : 4.5);
    out += `<line x1="${C + r0 * Math.cos(a)}" y1="${C + r0 * Math.sin(a)}"
                  x2="${C + R  * Math.cos(a)}" y2="${C + R  * Math.sin(a)}"
                  stroke="${maj ? "rgba(255,248,232,.34)" : "rgba(255,248,232,.17)"}"
                  stroke-width="${maj ? 2 : 1.2}" stroke-linecap="round"/>`;
  }
  el.innerHTML = out;
};

const liveKnobs = {};   // paramId -> repaint fn

const makeKnob = (k) => {
  const lg = k.size === "lg", S = lg ? 124 : 90;
  const n = document.createElement("div");
  n.className = "kb " + k.size;
  n.dataset.param = k.id;
  n.innerHTML =
    `<div class="dial">
       <svg class="ticks" viewBox="0 0 ${S} ${S}" width="${S}" height="${S}"></svg>
       <div class="bezel"></div><div class="body"></div><div class="flutes"></div>
       <div class="spec"></div><div class="rim"></div><div class="capdome"></div><div class="ptr"></div>
     </div>
     <div class="lbl engrave">${k.label}</div><div class="val"></div>`;

  ticks(n.querySelector(".ticks"), lg);

  const repaint = () => {
    const t = (k.val - k.min) / (k.max - k.min), ang = A0 + t * (A1 - A0);
    n.querySelector(".flutes").style.transform = `rotate(${ang + 90}deg)`;
    n.querySelector(".ptr").style.transform    = `rotate(${ang + 90}deg)`;
    n.querySelector(".val").textContent        = fmt(k);
  };
  repaint();
  liveKnobs[k.id] = repaint;

  let y0 = 0, v0 = 0, dragging = false;
  const dial = n.querySelector(".dial");

  dial.addEventListener("pointerdown", e => {
    dragging = true; y0 = e.clientY; v0 = k.val;
    dial.setPointerCapture(e.pointerId);
    beginGesture(k.id);
  });

  dial.addEventListener("pointermove", e => {
    if (!dragging) return;
    const step = (k.max - k.min) / 180 * (e.shiftKey ? 0.25 : 1);
    k.val = Math.max(k.min, Math.min(k.max, v0 + (y0 - e.clientY) * step));
    repaint();
    setParam(k.id, k.val);
  });

  const release = () => { if (dragging) { dragging = false; endGesture(k.id); } };
  dial.addEventListener("pointerup", release);
  dial.addEventListener("pointercancel", release);

  // industry conventions
  dial.addEventListener("dblclick", () => {
    k.val = k.default !== undefined ? k.default : (k.min + k.max) / 2;
    repaint(); beginGesture(k.id); setParam(k.id, k.val); endGesture(k.id);
  });

  dial.addEventListener("click", e => {
    if (e.ctrlKey || e.metaKey) {
      k.val = k.default !== undefined ? k.default : (k.min + k.max) / 2;
      repaint(); beginGesture(k.id); setParam(k.id, k.val); endGesture(k.id);
    }
  });

  return n;
};

/* ── panel builder ───────────────────────────────────────── */
let current = MODULES.find(m => m.selected) || MODULES[0];
const liveButtons = {}, liveFaders = {}, liveVoices = {};

const buildPanel = (m) => {
  current = m;
  for (const key in liveKnobs)   delete liveKnobs[key];
  for (const key in liveButtons) delete liveButtons[key];
  for (const key in liveFaders)  delete liveFaders[key];
  for (const key in liveVoices)  delete liveVoices[key];

  document.getElementById("module-title").textContent = m.id;
  document.getElementById("module-sub").textContent   = m.sub;

  const seg = document.getElementById("voice-seg");
  seg.innerHTML = "";
  m.voice.options.forEach((o, i) => {
    const b = document.createElement("b");
    b.textContent = o;
    if (i === m.voice.index) b.classList.add("on");
    b.addEventListener("click", () => {
      m.voice.index = i;
      [...seg.children].forEach(x => x.classList.remove("on"));
      b.classList.add("on");
      beginGesture(m.voice.id); setParam(m.voice.id, i); endGesture(m.voice.id);
    });
    seg.appendChild(b);
  });
  liveVoices[m.voice.id] = () => {
    [...seg.children].forEach((x, i) => x.classList.toggle("on", i === m.voice.index));
  };

  const deck = document.getElementById("knob-row");
  deck.innerHTML = "";
  m.knobs.forEach(k => deck.appendChild(makeKnob(k)));

  const foot = document.getElementById("mfoot");
  foot.innerHTML = "";

  m.buttons.forEach(b => {
    const el = document.createElement("div");
    el.className = "btn" + (b.on ? " on" : "");
    el.id = b.id;
    el.innerHTML = `<span class="jewel"></span>${b.label}`;
    el.addEventListener("click", () => {
      b.on = !b.on;
      el.classList.toggle("on", b.on);
      beginGesture(b.id); setParam(b.id, b.on ? 1 : 0); endGesture(b.id);
    });
    foot.appendChild(el);
    liveButtons[b.id] = () => el.classList.toggle("on", b.on);
  });

  const f = m.fader;
  const wrap = document.createElement("div");
  wrap.className = "faderwrap";
  wrap.innerHTML =
    `<div class="lbl engrave">${f.label}</div>
     <div class="fader" id="${f.id}-slider"><div class="cap"></div></div>
     <div class="num" id="${f.id}-value"></div>`;
  foot.appendChild(wrap);

  const fd   = wrap.querySelector(".fader");
  const cap  = wrap.querySelector(".cap");
  const num  = wrap.querySelector(".num");

  const paintFader = () => {
    const t = (f.val - f.min) / (f.max - f.min);
    cap.style.left = (t * 100) + "%";
    num.textContent = fmt(f);
  };
  paintFader();
  liveFaders[f.id] = paintFader;

  let dragging = false;
  const setFromEvent = e => {
    const r = fd.getBoundingClientRect();
    const t = Math.max(0, Math.min(1, (e.clientX - r.left) / r.width));
    f.val = f.min + t * (f.max - f.min);
    paintFader();
    setParam(f.id, f.val);
  };

  fd.addEventListener("pointerdown", e => {
    dragging = true; fd.setPointerCapture(e.pointerId);
    beginGesture(f.id); setFromEvent(e);
  });
  fd.addEventListener("pointermove", e => { if (dragging) setFromEvent(e); });
  const relFader = () => { if (dragging) { dragging = false; endGesture(f.id); } };
  fd.addEventListener("pointerup", relFader);
  fd.addEventListener("pointercancel", relFader);
};

/* ── pedalboard ──────────────────────────────────────────── */
const chain = document.getElementById("chain");
const cable = () => { const c = document.createElement("div"); c.className = "cable"; return c; };
const stompEls = {};

const pushOrder = () => {
  const ids = [...chain.querySelectorAll(".stomp")]
    .map(el => el.dataset.index)
    .filter(v => v !== undefined);
  setOrder(ids.join(","));
};

MODULES.forEach((m, index) => {
  chain.appendChild(cable());

  const p = document.createElement("div");
  p.className = "stomp " + m.skin + (m.on ? " on" : " off") + (m.selected ? " sel" : "");
  p.dataset.index = index;
  p.draggable = true;
  p.innerHTML =
    `<div class="sc a"><i></i></div><div class="sc b"><i></i></div>
     <div class="sc c"><i></i></div><div class="sc d"><i></i></div>
     <div class="lamp"></div>
     <div class="nm">${m.id}</div><div class="ty">${m.type}</div>
     <div class="fsw"></div>`;

  p.addEventListener("click", () => {
    [...chain.querySelectorAll(".stomp")].forEach(x => x.classList.remove("sel"));
    p.classList.add("sel");
    buildPanel(m);
  });

  p.querySelector(".fsw").addEventListener("click", e => {
    e.stopPropagation();
    m.on = !m.on;
    p.classList.toggle("on", m.on);
    p.classList.toggle("off", !m.on);
    beginGesture(m.bypassId); setParam(m.bypassId, m.on ? 0 : 1); endGesture(m.bypassId);
  });

  // drag to reorder
  p.addEventListener("dragstart", e => {
    e.dataTransfer.setData("text/plain", index);
    p.style.opacity = "0.45";
  });
  p.addEventListener("dragend", () => { p.style.opacity = ""; });
  p.addEventListener("dragover", e => e.preventDefault());
  p.addEventListener("drop", e => {
    e.preventDefault();
    const from = e.dataTransfer.getData("text/plain");
    const src  = chain.querySelector(`.stomp[data-index="${from}"]`);
    if (src && src !== p) {
      const all = [...chain.querySelectorAll(".stomp")];
      if (all.indexOf(src) < all.indexOf(p)) p.after(src);
      else                                   p.before(src);
      pushOrder();
    }
  });

  stompEls[m.id] = p;
  chain.appendChild(p);
});

chain.appendChild(cable());
const add = document.createElement("div");
add.className = "stomp add";
add.innerHTML = `<div class="nm">+</div><div class="ty">ADD</div>`;
chain.appendChild(add);
chain.appendChild(cable());
const outJack = document.createElement("div");
outJack.className = "jack";
outJack.innerHTML = `<div class="hole"></div><span>OUTPUT</span>`;
chain.appendChild(outJack);

buildPanel(current);

/* ── VU meters ───────────────────────────────────────────── */
const vuFace = (svg) => {
  const CX = 65, CY = 72, R = 54;
  let s = "";
  for (let i = 0; i <= 8; i++) {
    const a = (-150 + 120 * i / 8) * Math.PI / 180;
    const red = i >= 6, r0 = R - (i % 2 ? 4 : 7);
    s += `<line x1="${CX + r0 * Math.cos(a)}" y1="${CY + r0 * Math.sin(a)}"
                x2="${CX + R  * Math.cos(a)}" y2="${CY + R  * Math.sin(a)}"
                stroke="${red ? "#A62E1C" : "#4A3D20"}" stroke-width="${i % 2 ? 1 : 1.8}"/>`;
  }
  s += `<text x="${CX}" y="34" text-anchor="middle" font-size="7.5" fill="#6A5A33"
              font-family="Inter,sans-serif" letter-spacing="1.6">VU</text>`;
  s += `<g id="${svg.id}-n"><line x1="${CX}" y1="${CY}" x2="${CX}" y2="${CY - R + 3}"
           stroke="#1E1A12" stroke-width="1.6" stroke-linecap="round"/></g>`;
  s += `<circle cx="${CX}" cy="${CY}" r="5" fill="#2A2419"/>`;
  s += `<circle cx="${CX}" cy="${CY}" r="3" fill="#6E6450"/>`;
  svg.innerHTML = s;
};

const vin = document.getElementById("vu-in"), vout = document.getElementById("vu-out");
vuFace(vin); vuFace(vout);

let needleIn = 0, needleOut = 0;
const swing = (svg, t) => {
  const g = document.getElementById(svg.id + "-n");
  if (g) g.setAttribute("transform", `rotate(${-60 + 120 * Math.max(0, Math.min(1, t))} 65 72)`);
};
swing(vin, 0); swing(vout, 0);

/* ── mic position ────────────────────────────────────────── */
const stage = document.getElementById("cab-stage"), mic = document.getElementById("cab-mic");
let dmic = false;

const pushMic = () => {
  const r = stage.getBoundingClientRect();
  const x = parseFloat(mic.style.left || "132");
  const y = parseFloat(mic.style.top  || String(r.height / 2));
  setParam("CAB_MIC_X", Math.max(0, Math.min(100, ((x - 112) / Math.max(1, r.width - 142)) * 100)));
  setParam("CAB_MIC_Y", Math.max(0, Math.min(100, (y / Math.max(1, r.height)) * 100)));
};

mic.addEventListener("pointerdown", e => {
  dmic = true; mic.setPointerCapture(e.pointerId);
  beginGesture("CAB_MIC_X"); beginGesture("CAB_MIC_Y");
});
mic.addEventListener("pointermove", e => {
  if (!dmic) return;
  const r = stage.getBoundingClientRect();
  mic.style.left = Math.max(112, Math.min(r.width - 30, e.clientX - r.left - 9)) + "px";
  mic.style.top  = Math.max(20,  Math.min(r.height - 20, e.clientY - r.top)) + "px";
  pushMic();
});
mic.addEventListener("pointerup", () => {
  if (!dmic) return;
  dmic = false; endGesture("CAB_MIC_X"); endGesture("CAB_MIC_Y");
});

/* ── capture slot ────────────────────────────────────────── */
const slot = document.getElementById("NAM_SLOT");
slot.addEventListener("click", () => loadCapture());

const setCaptureName = (name, loaded) => {
  const b = slot.querySelector("b"), s = slot.querySelector("span");
  if (b) b.textContent = String(name || "NO CAPTURE LOADED").toUpperCase();
  if (s) s.textContent = loaded ? ".NAM · LOADED" : ".NAM · CLICK TO LOAD";
};

onBackend("axisCapture", (name) => setCaptureName(name, true));

/* ── rigs (presets) ──────────────────────────── */
/* The name on the LCD opens the list of saved rigs, its arrows step through
   them, and the three buttons beside it load one from a file, save the
   pedalboard as a rig of its own, and save over the rig that's loaded. Each
   one is the plugin's to do: the menus, the file chooser and the name box are
   native, so they look the same here as everywhere else in Sonify. */
const presetNameEl  = document.getElementById("preset-name");
const presetQuickEl = document.getElementById("preset-quick");

const setPresetName = (name) => {
  const shown = String(name || "DEFAULT").toUpperCase();
  if (presetNameEl && presetNameEl.textContent !== shown) presetNameEl.textContent = shown;
};

document.getElementById("preset-lcd") .addEventListener("click", () => presetMenu());
document.getElementById("preset-prev").addEventListener("click", () => presetStep(-1));
document.getElementById("preset-next").addEventListener("click", () => presetStep(1));
document.getElementById("preset-load").addEventListener("click", () => presetLoad());
document.getElementById("preset-save").addEventListener("click", () => presetSave());
presetQuickEl.addEventListener("click", () => presetQuick());

/* ── A/B rocker ──────────────────────────────────────────── */
document.querySelectorAll(".rocker span").forEach(s => s.addEventListener("click", () => {
  document.querySelectorAll(".rocker span").forEach(x => x.classList.remove("on"));
  s.classList.add("on");
}));

/* ── state sync from the plugin ──────────────────────────── */
const applyState = (json) => {
  let st;
  try { st = typeof json === "string" ? JSON.parse(json) : json; }
  catch (e) { return; }

  for (const id in byParam) {
    if (!(id in st)) continue;
    const entry = byParam[id], v = st[id];

    if (entry.kind === "knob" || entry.kind === "fader") {
      entry.def.val = v;
      if (liveKnobs[id])  liveKnobs[id]();
      if (liveFaders[id]) liveFaders[id]();
    } else if (entry.kind === "button") {
      entry.def.on = v > 0.5;
      if (liveButtons[id]) liveButtons[id]();
    } else if (entry.kind === "voice") {
      entry.def.index = Math.round(v);
      if (liveVoices[id]) liveVoices[id]();
    } else if (entry.kind === "bypass") {
      const m = entry.module, el = stompEls[m.id];
      m.on = !(v > 0.5);
      if (el) { el.classList.toggle("on", m.on); el.classList.toggle("off", !m.on); }
    }
  }

  if (st.__meters) {
    needleIn  += (Math.min(1, st.__meters.in  * 1.6) - needleIn)  * 0.4;
    needleOut += (Math.min(1, st.__meters.out * 1.6) - needleOut) * 0.4;
    swing(vin, needleIn); swing(vout, needleOut);
  }

  if (st.__capture !== undefined)
    setCaptureName(st.__capture, !!st.__captureLoaded);

  if (st.__preset !== undefined) setPresetName(st.__preset);

  /* QUICK writes over the rig that's loaded, so it waits until one is: a
     pedalboard that was never saved has nowhere to write, and SAVE is the
     button for it (pressing QUICK then asks for a name anyway). */
  if (st.__presetSaved !== undefined && presetQuickEl)
    presetQuickEl.title = st.__presetSaved ? "Save over the rig that's loaded"
                                           : "Save over the rig that's loaded (this one has no file yet: it asks for a name)";
};

onBackend("axisUpdate", applyState);

getState().then(s => { if (s) applyState(s); }).catch(() => {});
