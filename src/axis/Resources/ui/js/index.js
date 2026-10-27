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

// Without the bridge every control still moves, but moves only its own picture: no sound changes,
// no rig loads, nothing is saved. That is right when this file is opened in a browser to work on
// the design, and badly wrong inside the plugin, where it looks like a panel of dead controls. So
// say which one this is, plainly, on the panel itself.
if (! Juce) {
  const mark = document.getElementById("no-bridge");
  if (mark) mark.hidden = false;
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
const cabChoose    = native("axisCabChoose");
const presetChoose = native("axisPresetChoose");
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
      {id:"GATE_THRESH",  label:"THRESH",  size:"lg", min:-80, max:0,   val:-46, unit:"dB"},
      {id:"GATE_DEPTH",   label:"DEPTH",   size:"lg", min:0,   max:100, val:88},
      {id:"GATE_RELEASE", label:"RELEASE", size:"lg", min:5,   max:800, val:160, unit:"ms"},
      {id:"GATE_HOLD",    label:"HOLD",    size:"lg", min:0,   max:500, val:40,  unit:"ms"},
      {id:"GATE_SC_HPF",  label:"SC HPF",  size:"lg", min:20,  max:800, val:120, unit:"Hz"}
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
      {id:"COMP_THRESH",  label:"THRESH",  size:"lg", min:-48, max:0,   val:-18, unit:"dB"},
      {id:"COMP_RATIO",   label:"RATIO",   size:"lg", min:1,   max:20,  val:4,   unit:":1"},
      {id:"COMP_MAKEUP",  label:"MAKEUP",  size:"lg", min:0,   max:24,  val:6,   unit:"dB"},
      {id:"COMP_ATTACK",  label:"ATTACK",  size:"lg", min:1,   max:120, val:14,  unit:"ms"},
      {id:"COMP_RELEASE", label:"RELEASE", size:"lg", min:20,  max:1200,val:280, unit:"ms"}
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
      {id:"DRV_TONE",  label:"TONE",  size:"lg", min:0, max:100, val:42},
      {id:"DRV_GAIN",  label:"GAIN",  size:"lg", min:0, max:100, val:64},
      {id:"DRV_LEVEL", label:"LEVEL", size:"lg", min:0, max:100, val:55},
      {id:"DRV_BODY",  label:"BODY",  size:"lg", min:0, max:100, val:38},
      {id:"DRV_BITE",  label:"BITE",  size:"lg", min:0, max:100, val:71}
    ],
    buttons:[
      {id:"DRV_DYNAMIC",label:"DYNAMIC TOUCH", on:true},
      {id:"DRV_STEREO", label:"TRUE STEREO",   on:false},
      {id:"DRV_MORPH",  label:"MORPH LINK",    on:false}
    ],
    fader:{ id:"DRV_MIX", label:"MIX", min:0, max:100, val:100, unit:"%" }
  },
  {
    id:"AMP", type:"AMP & CAB", skin:"en-teal", bypassId:"AMP_BYPASS", on:true,
    sub:"BRITISH CRUNCH",
    // Under the pedal's name, the amp that's on (the plugin says it too, in __amp).
    subs:["AMERICAN CLEAN","BRITISH CRUNCH","BRITISH CHIME","BRITISH LEAD","MODERN HIGH GAIN"],
    voice:{ id:"AMP_VOICE", options:["CLEAN","CRUNCH","CHIME","LEAD","MODERN"], index:1 },
    knobs:[
      {id:"AMP_GAIN",    label:"GAIN",    size:"lg", min:0, max:100, val:50},
      {id:"AMP_BASS",    label:"BASS",    size:"lg", min:0, max:100, val:50},
      {id:"AMP_MID",     label:"MID",     size:"lg", min:0, max:100, val:50},
      {id:"AMP_TREBLE",  label:"TREBLE",  size:"lg", min:0, max:100, val:50},
      {id:"AMP_PRESENCE",label:"PRESENCE",size:"lg", min:0, max:100, val:50}
    ],
    buttons:[
      {id:"AMP_BRIGHT", label:"BRIGHT", on:false},
      {id:"AMP_TIGHT",  label:"TIGHT",  on:false},
      {id:"AMP_CAB",    label:"CABINET",on:true}
    ],
    fader:{ id:"AMP_MASTER", label:"MASTER", min:0, max:100, val:50 }
  },
  {
    id:"SPACE", type:"REVERB", skin:"en-crm", bypassId:"SPC_BYPASS", on:true,
    sub:"PLATE / HALL / SPRING",
    voice:{ id:"SPC_TYPE", options:["PLATE","HALL","SPRING"], index:2 },
    knobs:[
      {id:"SPC_PREDLY", label:"PRE-DLY", size:"lg", min:0,  max:250, val:24, unit:"ms"},
      {id:"SPC_DECAY",  label:"DECAY",   size:"lg", min:0,  max:100, val:46},
      {id:"SPC_SIZE",   label:"SIZE",    size:"lg", min:0,  max:100, val:58},
      {id:"SPC_DAMP",   label:"DAMP",    size:"lg", min:0,  max:100, val:62},
      {id:"SPC_WIDTH",  label:"WIDTH",   size:"lg", min:0,  max:100, val:80}
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
// A module whose voice is a different thing each time (the AMP's amps) says which under its name.
const subOf = (m) => (m.subs && m.subs[m.voice.index]) || m.sub;
const showSub = (m) => { if (current === m) document.getElementById("module-sub").textContent = subOf(m); };

let current = MODULES.find(m => m.selected) || MODULES[0];
const liveButtons = {}, liveFaders = {}, liveVoices = {};

const buildPanel = (m) => {
  current = m;
  for (const key in liveKnobs)   delete liveKnobs[key];
  for (const key in liveButtons) delete liveButtons[key];
  for (const key in liveFaders)  delete liveFaders[key];
  for (const key in liveVoices)  delete liveVoices[key];

  document.getElementById("module-title").textContent = m.id;
  document.getElementById("module-sub").textContent   = subOf(m);

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
      showSub(m);
      beginGesture(m.voice.id); setParam(m.voice.id, i); endGesture(m.voice.id);
    });
    seg.appendChild(b);
  });
  liveVoices[m.voice.id] = () => {
    [...seg.children].forEach((x, i) => x.classList.toggle("on", i === m.voice.index));
    showSub(m);
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

// The other way round: the parameters' 0..100 back into the dot's place on the picture.
const placeMic = (x, y) => {
  if (dmic) return;
  const r = stage.getBoundingClientRect();
  if (r.width < 150 || r.height < 40) return;   // not laid out yet
  const left = Math.max(112, Math.min(r.width - 30, 112 + (x / 100) * (r.width - 142)));
  const top  = Math.max(20,  Math.min(r.height - 20, (y / 100) * r.height));
  mic.style.left = left + "px";
  mic.style.top  = top + "px";
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

/* ── the cabinet ───────────────────────────────────────────
   Its plate on the rail says which cabinet plays: one of the four modelled
   ones, or an impulse response (from the AXIS pack beside Sonify, or a file of
   your own). Clicking it opens a menu drawn here, as the rig menu is; the
   plugin sends the lists with the rest of the state (__cab). */
const cabLcd  = document.getElementById("cab-lcd");
const cabMenu = document.getElementById("cab-menu");
const cabTag  = document.getElementById("cab-tag");
let cabInfo = { models: ["1x12 OPEN","2x12 ALNICO","4x12 VINTAGE","4x12 MODERN"], playing: 2, irType: 4,
                ir: "", irFrom: "", irSource: "", problem: "", pack: [], packProblem: "", mic: "CAP · ON AXIS" };

const fileNameOf = (path) => String(path || "").split(/[\\/]/).pop();

const showCab = () => {
  const ir = cabInfo.playing === cabInfo.irType;
  const name = ir ? (cabInfo.ir || "IMPULSE RESPONSE") : (cabInfo.models[cabInfo.playing] || "CABINET");
  const b = document.getElementById("cab-name"), line = document.getElementById("cab-line");
  const kind = document.getElementById("cab-kind");

  if (b) b.textContent = String(name).toUpperCase();
  if (kind) kind.textContent = ir ? "IR" : "MODEL";

  if (line) {
    if (cabInfo.problem) line.textContent = "IR NOT LOADED · " + String(cabInfo.problem).toUpperCase();
    else if (ir) line.textContent = cabInfo.irFrom === "pack" ? "IR · AXIS PACK" : "IR · " + fileNameOf(cabInfo.irSource).toUpperCase();
    else line.textContent = "MODELLED · CLICK TO CHANGE";
    line.title = cabInfo.problem || cabInfo.irSource || "";
  }

  if (cabTag) cabTag.textContent = String(name).toUpperCase() + " · " + (cabInfo.mic || "");
};

const closeCabMenu = () => { cabMenu.hidden = true; cabMenu.replaceChildren(); };

const buildCabMenu = () => {
  cabMenu.replaceChildren();

  const section = (text) => {
    const el = document.createElement("div");
    el.className = "sec";
    el.textContent = text;
    cabMenu.appendChild(el);
  };

  const line = (text, { on = false, off = false, pick = null, title = "" } = {}) => {
    const b = document.createElement("button");
    b.type = "button";
    b.setAttribute("role", "menuitem");
    b.textContent = text;
    if (title) b.title = title;
    if (on) b.classList.add("on");
    if (off) b.disabled = true;
    else if (pick) b.addEventListener("click", () => { closeCabMenu(); pick(); });
    cabMenu.appendChild(b);
  };

  const sep = () => { const el = document.createElement("div"); el.className = "sep"; cabMenu.appendChild(el); };

  section("MODELLED");
  cabInfo.models.forEach((name, i) =>
    line(name, { on: cabInfo.playing === i, pick: () => cabChoose("model", i) }));

  // The AXIS pack beside Sonify, when there is one (or one there that can't be read).
  const packHere = cabInfo.pack.length > 0 || !! cabInfo.packProblem;
  if (packHere) section("AXIS PACK");
  if (cabInfo.packProblem && ! cabInfo.pack.length)
    line("(the pack can't be read)", { off: true, title: cabInfo.packProblem });
  cabInfo.pack.forEach((name, i) =>
    line(name, { on: cabInfo.playing === cabInfo.irType && cabInfo.irFrom === "pack" && cabInfo.irSource === name,
                 pick: () => cabChoose("pack", i) }));

  sep();
  if (cabInfo.ir && cabInfo.irFrom === "file")
    line(cabInfo.ir, { on: cabInfo.playing === cabInfo.irType, pick: () => cabChoose("ir", 0), title: cabInfo.irSource });
  line("Load an IR file\u2026", { pick: () => cabChoose("file", 0) });
  if (cabInfo.ir) line("No IR", { pick: () => cabChoose("clear", 0) });
  if (packHere) line("Show the pack's folder", { pick: () => cabChoose("folder", 0) });
};

const openCabMenu = () => {
  buildCabMenu();
  cabMenu.hidden = false;

  // Under the plate, and kept inside the window.
  const at = cabLcd.getBoundingClientRect();
  const w = cabMenu.offsetWidth, h = cabMenu.offsetHeight;
  cabMenu.style.left = Math.max(6, Math.min(window.innerWidth - w - 6, at.right - w)) + "px";
  cabMenu.style.top = (at.bottom + h > window.innerHeight - 6 ? Math.max(6, at.top - h - 4) : at.bottom + 4) + "px";
};

cabLcd.addEventListener("click", (event) => {
  event.stopPropagation();
  if (cabMenu.hidden) openCabMenu();
  else closeCabMenu();
});

window.addEventListener("pointerdown", (event) => {
  if (! cabMenu.hidden && ! cabMenu.contains(event.target) && ! cabLcd.contains(event.target))
    closeCabMenu();
}, true);

window.addEventListener("keydown", (event) => { if (event.key === "Escape") closeCabMenu(); });
window.addEventListener("blur", closeCabMenu);

showCab();

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

/* ── the rig menu ──────────────────────────────────────────
   Drawn here, not by the host: a native menu asked for by a click inside this page is dismissed
   by that very click, so it never stays up. The plugin sends the list with the rest of the state
   (__rigs); picking a line sends back what was picked. */
let rigList = { factory: [], mine: [], kind: "", index: -1 };

const lcdEl  = document.getElementById("preset-lcd");
const menuEl = document.getElementById("rig-menu");

const closeRigMenu = () => { menuEl.hidden = true; menuEl.replaceChildren(); };

const buildRigMenu = () => {
  menuEl.replaceChildren();

  const section = (text) => {
    const el = document.createElement("div");
    el.className = "sec";
    el.textContent = text;
    menuEl.appendChild(el);
  };

  const line = (text, { on = false, off = false, pick = null } = {}) => {
    const b = document.createElement("button");
    b.type = "button";
    b.setAttribute("role", "menuitem");
    b.textContent = text;
    if (on) b.classList.add("on");
    if (off) b.disabled = true;
    else if (pick) b.addEventListener("click", () => { closeRigMenu(); pick(); });
    menuEl.appendChild(b);
  };

  section("FACTORY");
  rigList.factory.forEach((name, i) =>
    line(name, { on: rigList.kind === "factory" && rigList.index === i,
                 pick: () => presetChoose("factory", i) }));

  section("MY RIGS");
  if (! rigList.mine.length) line("(none saved yet)", { off: true });
  rigList.mine.forEach((name, i) =>
    line(name, { on: rigList.kind === "mine" && rigList.index === i,
                 pick: () => presetChoose("mine", i) }));

  const sep = document.createElement("div");
  sep.className = "sep";
  menuEl.appendChild(sep);
  line("Show rigs folder", { pick: () => presetChoose("folder", 0) });
};

const openRigMenu = () => {
  buildRigMenu();
  menuEl.hidden = false;

  // Under the LCD, and kept inside the window.
  const at = lcdEl.getBoundingClientRect();
  const w = menuEl.offsetWidth, h = menuEl.offsetHeight;
  const left = Math.max(6, Math.min(window.innerWidth - w - 6, at.left));
  const top = at.bottom + h > window.innerHeight - 6 ? Math.max(6, at.top - h - 4) : at.bottom + 4;
  menuEl.style.left = left + "px";
  menuEl.style.top = top + "px";
};

lcdEl.addEventListener("click", (event) => {
  event.stopPropagation();
  if (menuEl.hidden) openRigMenu();
  else closeRigMenu();
});

window.addEventListener("pointerdown", (event) => {
  if (! menuEl.hidden && ! menuEl.contains(event.target) && ! lcdEl.contains(event.target))
    closeRigMenu();
}, true);

window.addEventListener("keydown", (event) => { if (event.key === "Escape") closeRigMenu(); });
window.addEventListener("blur", closeRigMenu);
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

  // The amp that's on, as the plugin names it, under the pedal's name.
  if (st.__amp && st.__amp.sub) {
    const m = MODULES.find(x => x.id === "AMP"), i = Math.round(st.__amp.voice);
    if (m && m.subs && i >= 0 && i < m.subs.length && m.subs[i] !== st.__amp.sub) { m.subs[i] = st.__amp.sub; showSub(m); }
  }

  // The mic where the parameters have it (a rig, a song, automation), unless it's being dragged.
  if (st.CAB_MIC_X !== undefined && st.CAB_MIC_Y !== undefined) placeMic(st.CAB_MIC_X, st.CAB_MIC_Y);

  if (st.__cab) {
    const next = JSON.stringify(st.__cab);
    if (next !== JSON.stringify(cabInfo)) {
      cabInfo = st.__cab;
      showCab();
      if (! cabMenu.hidden) openCabMenu();   // it is open: the tick follows what was just picked
    }
  }

  if (st.__preset !== undefined) setPresetName(st.__preset);

  if (st.__rigs) {
    const next = JSON.stringify(st.__rigs);
    if (next !== JSON.stringify(rigList)) {
      rigList = st.__rigs;
      if (! menuEl.hidden) openRigMenu();   // it is open: the tick follows what was just loaded
    }
  }

  /* QUICK writes over the rig that's loaded, so it waits until one is: a
     pedalboard that was never saved has nowhere to write, and SAVE is the
     button for it (pressing QUICK then asks for a name anyway). */
  if (st.__presetSaved !== undefined && presetQuickEl)
    presetQuickEl.title = st.__presetSaved ? "Save over the rig that's loaded"
                                           : "Save over the rig that's loaded (this one has no file yet: it asks for a name)";
};

onBackend("axisUpdate", applyState);

getState().then(s => { if (s) applyState(s); }).catch(() => {});
